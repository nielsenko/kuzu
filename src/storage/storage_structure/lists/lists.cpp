#include "storage/storage_structure/lists/lists.h"

#include "storage/storage_structure/lists/lists_update_iterator.h"

using namespace kuzu::common;

namespace kuzu {
namespace storage {

// Note: The given nodeOffset and largeListHandle may not be connected. For example if we
// are about to read a new nodeOffset, say v5, after having read a previous nodeOffset, say v7, with
// a largeList, then the input to this function can be nodeOffset: 5 and largeListHandle containing
// information about the last portion of v7's large list. Similarly, if nodeOffset is v3 and v3
// has a small list then largeListHandle does not contain anything specific to v3 (it would likely
// be containing information about the last portion of the last large list that was read).
void Lists::readValues(const shared_ptr<ValueVector>& valueVector, ListHandle& listHandle) {
    if (listHandle.getListSourceStore() == ListSourceStore::UPDATE_STORE) {
        listsUpdateStore->readValues(
            storageStructureIDAndFName.storageStructureID.listFileID, listHandle, valueVector);
    } else {
        // If the startElementOffset is 0, it means that this is the first time that we read from
        // the list. As a result, we need to reset the cursor and mapper.
        if (listHandle.getStartElemOffset() == 0) {
            listHandle.setMapper(metadata);
        }
        readFromList(valueVector, listHandle);
    }
}

void Lists::readFromSmallList(const shared_ptr<ValueVector>& valueVector, ListHandle& listHandle) {
    auto dummyReadOnlyTrx = Transaction::getDummyReadOnlyTrx();
    auto pageCursor = PageUtils::getPageElementCursorForPos(
        ListHeaders::getSmallListCSROffset(listHandle.getListHeader()), numElementsPerPage);
    readBySequentialCopy(dummyReadOnlyTrx.get(), valueVector, pageCursor, listHandle.mapper);
}

void Lists::readFromLargeList(const shared_ptr<ValueVector>& valueVector, ListHandle& listHandle) {
    // Assumes that the associated adjList has already updated the syncState.
    auto pageCursor =
        PageUtils::getPageElementCursorForPos(listHandle.getStartElemOffset(), numElementsPerPage);
    auto dummyReadOnlyTrx = Transaction::getDummyReadOnlyTrx();
    readBySequentialCopy(dummyReadOnlyTrx.get(), valueVector, pageCursor, listHandle.mapper);
}

void Lists::readFromList(const shared_ptr<ValueVector>& valueVector, ListHandle& listHandle) {
    if (ListHeaders::isALargeList(listHandle.getListHeader())) {
        readFromLargeList(valueVector, listHandle);
    } else {
        readFromSmallList(valueVector, listHandle);
    }
}

uint64_t Lists::getNumElementsInPersistentStore(
    TransactionType transactionType, node_offset_t nodeOffset) {
    if (transactionType == TransactionType::WRITE &&
        listsUpdateStore->isNewlyAddedNode(
            storageStructureIDAndFName.storageStructureID.listFileID, nodeOffset)) {
        return 0;
    }
    return getNumElementsFromListHeader(nodeOffset);
}

void Lists::initListReadingState(
    node_offset_t nodeOffset, ListHandle& listHandle, TransactionType transactionType) {
    listHandle.resetSyncState();
    auto isNewlyAddedNode = listsUpdateStore->isNewlyAddedNode(
        storageStructureIDAndFName.storageStructureID.listFileID, nodeOffset);
    uint64_t numElementsInPersistentStore, numElementsInUpdateStore = 0;
    list_header_t listHeader;
    if (transactionType == TransactionType::WRITE) {
        numElementsInUpdateStore = listsUpdateStore->getNumInsertedRelsForNodeOffset(
            storageStructureIDAndFName.storageStructureID.listFileID, nodeOffset);
        // ListHeader is UINT32_MAX in two cases: (i) ListSyncState is not initialized; or (ii)
        // the list of a new node is being scanned so there is no header for the new node.
        listHeader = isNewlyAddedNode ? UINT32_MAX : headers->getHeader(nodeOffset);
        numElementsInPersistentStore =
            isNewlyAddedNode ? 0 : getNumElementsFromListHeader(nodeOffset);
    } else {
        listHeader = headers->getHeader(nodeOffset);
        numElementsInPersistentStore = getNumElementsFromListHeader(nodeOffset);
    }
    // If there's no element is persistentStore, we can skip reading from persistentStore and start
    // reading from listsUpdateStore directly.
    auto sourceStore = numElementsInPersistentStore == 0 ? ListSourceStore::UPDATE_STORE :
                                                           ListSourceStore::PERSISTENT_STORE;
    listHandle.initSyncState(nodeOffset, listHeader, numElementsInUpdateStore,
        numElementsInPersistentStore, sourceStore);
}

unique_ptr<InMemList> Lists::createInMemListWithDataFromUpdateStoreOnly(
    node_offset_t nodeOffset, vector<uint64_t>& insertedRelsTupleIdxInFT) {
    auto inMemList = make_unique<InMemList>(
        getNumElementsInListsUpdateStore(nodeOffset), elementSize, mayContainNulls());
    listsUpdateStore->readInsertionsToList(storageStructureIDAndFName.storageStructureID.listFileID,
        insertedRelsTupleIdxInFT, *inMemList, 0 /* numElementsInPersistentStore */,
        getDiskOverflowFileIfExists(), dataType, getNodeIDCompressionIfExists());
    return inMemList;
}

unique_ptr<InMemList> Lists::writeToInMemList(node_offset_t nodeOffset,
    const vector<uint64_t>& insertedRelTupleIdxesInFT,
    const unordered_set<uint64_t>& deletedRelOffsetsForList) {
    auto inMemList =
        make_unique<InMemList>(getTotalNumElementsInList(TransactionType::WRITE, nodeOffset),
            elementSize, mayContainNulls());
    auto numElementsInPersistentStore = getNumElementsFromListHeader(nodeOffset);
    fillInMemListsFromPersistentStore(
        nodeOffset, numElementsInPersistentStore, *inMemList, deletedRelOffsetsForList);
    listsUpdateStore->readInsertionsToList(storageStructureIDAndFName.storageStructureID.listFileID,
        insertedRelTupleIdxesInFT, *inMemList,
        numElementsInPersistentStore - deletedRelOffsetsForList.size(),
        getDiskOverflowFileIfExists(), dataType, getNodeIDCompressionIfExists());
    return inMemList;
}

void Lists::fillInMemListsFromPersistentStore(node_offset_t nodeOffset,
    uint64_t numElementsInPersistentStore, InMemList& inMemList,
    const unordered_set<uint64_t>& deletedRelOffsetsInList) {
    auto listHeader = headers->getHeader(nodeOffset);
    auto pageMapper = ListHandle::getPageMapper(metadata, listHeader, nodeOffset);
    auto pageCursor = ListHandle::getPageCursor(listHeader, numElementsPerPage);
    uint64_t numElementsRead = 0;
    uint64_t nextPosToWriteToInMemList = 0;
    auto numElementsToRead = numElementsInPersistentStore;
    while (numElementsRead < numElementsToRead) {
        auto numElementsToReadInCurPage = min(numElementsToRead - numElementsRead,
            (uint64_t)(numElementsPerPage - pageCursor.elemPosInPage));
        auto physicalPageIdx = pageMapper(pageCursor.pageIdx);
        auto frame = bufferManager.pin(fileHandle, physicalPageIdx);
        fillInMemListsFromFrame(inMemList, frame, pageCursor.elemPosInPage,
            numElementsToReadInCurPage, deletedRelOffsetsInList, numElementsRead,
            nextPosToWriteToInMemList);
        bufferManager.unpin(fileHandle, physicalPageIdx);
        numElementsRead += numElementsToReadInCurPage;
        pageCursor.nextPage();
    }
}

void Lists::fillInMemListsFromFrame(InMemList& inMemList, const uint8_t* frame,
    uint64_t elemPosInPage, uint64_t numElementsToReadInCurPage,
    const unordered_set<uint64_t>& deletedRelOffsetsInList, uint64_t numElementsRead,
    uint64_t& nextPosToWriteToInMemList) {
    auto nullBufferInPage = (uint64_t*)getNullBufferInPage(frame);
    auto frameData = frame + getElemByteOffset(elemPosInPage);
    auto listData = inMemList.getListData() + getElemByteOffset(nextPosToWriteToInMemList);
    // If we don't have any deleted rels, we can simply do sequential copy from frame to inMemList.
    if (deletedRelOffsetsInList.empty()) {
        memcpy(listData, frameData, numElementsToReadInCurPage * elementSize);
        if (inMemList.hasNullBuffer()) {
            NullMask::copyNullMask(nullBufferInPage, elemPosInPage, inMemList.getNullMask(),
                nextPosToWriteToInMemList, numElementsToReadInCurPage);
        }
        nextPosToWriteToInMemList += numElementsToReadInCurPage;
    } else {
        // If we have some deleted rels, we should check whether each rel has been deleted
        // or not before copying to inMemList.
        for (auto i = 0u; i < numElementsToReadInCurPage; i++) {
            auto relOffsetInList = numElementsRead + i;
            if (!deletedRelOffsetsInList.contains(relOffsetInList)) {
                memcpy(listData, frameData, elementSize);
                if (inMemList.hasNullBuffer()) {
                    NullMask::copyNullMask(nullBufferInPage, elemPosInPage, inMemList.getNullMask(),
                        nextPosToWriteToInMemList, 1 /* numBitsToCopy */);
                }
                listData += elementSize;
                nextPosToWriteToInMemList++;
            }
            frameData += elementSize;
        }
    }
}

void StringPropertyLists::readFromLargeList(
    const shared_ptr<ValueVector>& valueVector, ListHandle& listHandle) {
    valueVector->resetOverflowBuffer();
    Lists::readFromLargeList(valueVector, listHandle);
    diskOverflowFile.readStringsToVector(TransactionType::READ_ONLY, *valueVector);
}

void StringPropertyLists::readFromSmallList(
    const shared_ptr<ValueVector>& valueVector, ListHandle& listHandle) {
    valueVector->resetOverflowBuffer();
    Lists::readFromSmallList(valueVector, listHandle);
    diskOverflowFile.readStringsToVector(TransactionType::READ_ONLY, *valueVector);
}

void ListPropertyLists::readFromLargeList(
    const shared_ptr<ValueVector>& valueVector, ListHandle& listHandle) {
    valueVector->resetOverflowBuffer();
    Lists::readFromLargeList(valueVector, listHandle);
    diskOverflowFile.readListsToVector(TransactionType::READ_ONLY, *valueVector);
}

void ListPropertyLists::readFromSmallList(
    const shared_ptr<ValueVector>& valueVector, ListHandle& listHandle) {
    valueVector->resetOverflowBuffer();
    Lists::readFromSmallList(valueVector, listHandle);
    diskOverflowFile.readListsToVector(TransactionType::READ_ONLY, *valueVector);
}

void AdjLists::readValues(const shared_ptr<ValueVector>& valueVector, ListHandle& listHandle) {
    valueVector->state->selVector->resetSelectorToUnselected();
    if (listHandle.getListSourceStore() == ListSourceStore::UPDATE_STORE) {
        readFromListsUpdateStore(listHandle, valueVector);
    } else {
        readFromListsPersistentStore(listHandle, valueVector);
    }
}

unique_ptr<vector<nodeID_t>> AdjLists::readAdjacencyListOfNode(
    // We read the adjacency list of a node in 2 steps: i) we read all the bytes from the pages
    // that hold the list into a buffer; and (ii) we interpret the bytes in the buffer based on the
    // nodeIDCompressionScheme into a vector of nodeID_t.
    node_offset_t nodeOffset) {
    auto header = headers->getHeader(nodeOffset);
    auto pageMapper = ListHandle::getPageMapper(metadata, header, nodeOffset);
    auto pageCursor = ListHandle::getPageCursor(header, numElementsPerPage);
    // Step 1
    auto numElementsInList = getNumElementsFromListHeader(nodeOffset);
    auto listLenInBytes = numElementsInList * elementSize;
    auto buffer = make_unique<uint8_t[]>(listLenInBytes);
    auto sizeLeftToCopy = listLenInBytes;
    auto bufferPtr = buffer.get();
    while (sizeLeftToCopy) {
        auto physicalPageIdx = pageMapper(pageCursor.pageIdx);
        auto sizeToCopyInPage =
            min(((uint64_t)(numElementsPerPage - pageCursor.elemPosInPage) * elementSize),
                sizeLeftToCopy);
        auto frame = bufferManager.pin(fileHandle, physicalPageIdx);
        memcpy(bufferPtr, frame + mapElementPosToByteOffset(pageCursor.elemPosInPage),
            sizeToCopyInPage);
        bufferManager.unpin(fileHandle, physicalPageIdx);
        bufferPtr += sizeToCopyInPage;
        sizeLeftToCopy -= sizeToCopyInPage;
        pageCursor.nextPage();
    }

    // Step 2
    unique_ptr<vector<nodeID_t>> retVal = make_unique<vector<nodeID_t>>();
    auto sizeLeftToDecompress = listLenInBytes;
    bufferPtr = buffer.get();
    while (sizeLeftToDecompress) {
        nodeID_t nodeID(0, 0);
        nodeIDCompressionScheme.readNodeID(bufferPtr, &nodeID);
        bufferPtr += nodeIDCompressionScheme.getNumBytesForNodeIDAfterCompression();
        retVal->emplace_back(nodeID);
        sizeLeftToDecompress -= nodeIDCompressionScheme.getNumBytesForNodeIDAfterCompression();
    }
    return retVal;
}

void AdjLists::readFromLargeList(
    const shared_ptr<ValueVector>& valueVector, ListHandle& listHandle) {
    uint64_t nextPartBeginElemOffset =
        listHandle.hasValidRangeToRead() ? listHandle.getEndElemOffset() : 0;
    auto pageCursor =
        PageUtils::getPageElementCursorForPos(nextPartBeginElemOffset, numElementsPerPage);
    // The number of edges to read is the minimum of: (i) how may edges are left to read
    // (info.listLen - nextPartBeginElemOffset); and (ii) how many elements are left in the current
    // page that's being read (nextPartBeginElemOffset above should be set to the beginning of the
    // next page. Note that because of case (ii), this computation guarantees that what we read fits
    // into a single page. That's why we can call copyFromAPage.
    auto numValuesToCopy =
        min((uint32_t)(listHandle.getNumValuesInList() - nextPartBeginElemOffset),
            numElementsPerPage - (uint32_t)(nextPartBeginElemOffset % numElementsPerPage));
    valueVector->state->initOriginalAndSelectedSize(numValuesToCopy);
    listHandle.setRangeToRead(nextPartBeginElemOffset, numValuesToCopy);
    // map logical pageIdx to physical pageIdx
    auto physicalPageId = listHandle.mapper(pageCursor.pageIdx);
    // See comments for AdjLists::readFromSmallList.
    auto dummyReadOnlyTrx = Transaction::getDummyReadOnlyTrx();
    readNodeIDsFromAPageBySequentialCopy(dummyReadOnlyTrx.get(), valueVector, 0, physicalPageId,
        pageCursor.elemPosInPage, numValuesToCopy, nodeIDCompressionScheme, true /*isAdjLists*/);
}

// Note: This function sets the original and selected size of the DataChunk into which it will
// read a list of nodes and edges.
void AdjLists::readFromSmallList(
    const shared_ptr<ValueVector>& valueVector, ListHandle& listHandle) {
    valueVector->state->initOriginalAndSelectedSize(listHandle.getNumValuesInList());
    // We store the updates for adjLists in listsUpdateStore, however we store the
    // updates for adjColumn in the WAL version of the page. The adjColumn needs to pass a
    // transaction to readNodeIDsBySequentialCopy, so readNodeIDsBySequentialCopy can know whether
    // to read the wal version or the original version of the page. AdjLists never reads the wal
    // version of the page(since its updates are stored in listsUpdateStore), so we
    // simply pass a dummy read-only transaction to readNodeIDsBySequentialCopy.
    auto dummyReadOnlyTrx = Transaction::getDummyReadOnlyTrx();
    auto pageCursor = PageUtils::getPageElementCursorForPos(
        ListHeaders::getSmallListCSROffset(listHandle.getListHeader()), numElementsPerPage);
    readNodeIDsBySequentialCopy(dummyReadOnlyTrx.get(), valueVector, pageCursor, listHandle.mapper,
        nodeIDCompressionScheme, true /*isAdjLists*/);
    // We set the startIdx + numValuesToRead == numValuesInList in listSyncState to indicate to the
    // callers (e.g., the adj_list_extend or var_len_extend) that we have read the small list
    // already. This allows the callers to know when to switch to reading from the update store if
    // there is any updates.
    listHandle.setRangeToRead(0, listHandle.getNumValuesInList());
}

void AdjLists::readFromListsUpdateStore(
    ListHandle& listHandle, const shared_ptr<ValueVector>& valueVector) {
    if (!listHandle.hasValidRangeToRead()) {
        // We have read all values from persistent store or the persistent store is empty, we should
        // reset listSyncState to indicate ranges in listsUpdateStore and start
        // reading from it.
        listHandle.setRangeToRead(
            0, min(DEFAULT_VECTOR_CAPACITY, (uint64_t)listHandle.getNumValuesInList()));
    } else {
        listHandle.setRangeToRead(listHandle.getEndElemOffset(),
            min(DEFAULT_VECTOR_CAPACITY,
                (uint64_t)listHandle.getNumValuesInList() - listHandle.getEndElemOffset()));
    }
    // Note that: we always store nbr node in the second column of factorizedTable.
    listsUpdateStore->readValues(
        storageStructureIDAndFName.storageStructureID.listFileID, listHandle, valueVector);
}

void AdjLists::readFromListsPersistentStore(
    ListHandle& listHandle, const shared_ptr<ValueVector>& valueVector) {
    // If the startElemOffset is invalid, it means that we never read from the list. As a
    // result, we need to reset the cursor and mapper.
    if (!listHandle.hasValidRangeToRead()) {
        listHandle.setMapper(metadata);
    }
    readFromList(valueVector, listHandle);
}

// Note: this function will always be called right after scanRelID, so we have the
// guarantee that the relIDVector is always unselected.
void RelIDList::setDeletedRelsIfNecessary(
    Transaction* transaction, ListHandle& listHandle, const shared_ptr<ValueVector>& relIDVector) {
    // We only need to unselect the positions for deleted rels when we are reading from the
    // persistent store in a write transaction and the current nodeOffset has deleted rels in
    // persistent store.
    if (!transaction->isReadOnly() &&
        listHandle.getListSourceStore() != ListSourceStore::UPDATE_STORE &&
        listsUpdateStore->hasAnyDeletedRelsInPersistentStore(
            storageStructureIDAndFName.storageStructureID.listFileID,
            listHandle.getBoundNodeOffset())) {
        relIDVector->state->selVector->resetSelectorToValuePosBuffer();
        auto& selVector = relIDVector->state->selVector;
        auto nextSelectedPos = 0u;
        for (auto pos = 0; pos < relIDVector->state->originalSize; ++pos) {
            if (!listsUpdateStore->isRelDeletedInPersistentStore(
                    storageStructureIDAndFName.storageStructureID.listFileID,
                    listHandle.getBoundNodeOffset(), relIDVector->getValue<int64_t>(pos))) {
                selVector->selectedPositions[nextSelectedPos++] = pos;
            }
        }
        selVector->selectedSize = nextSelectedPos;
    }
}

unordered_set<uint64_t> RelIDList::getDeletedRelOffsetsInListForNodeOffset(
    node_offset_t nodeOffset) {
    unordered_set<uint64_t> deletedRelOffsetsInList;
    auto listHeader = headers->getHeader(nodeOffset);
    auto pageMapper = ListHandle::getPageMapper(metadata, listHeader, nodeOffset);
    auto pageCursor = ListHandle::getPageCursor(listHeader, numElementsPerPage);
    auto numElementsInPersistentStore = getNumElementsFromListHeader(nodeOffset);
    uint64_t numElementsRead = 0;
    while (numElementsRead < numElementsInPersistentStore) {
        auto numElementsToReadInCurPage = min(numElementsInPersistentStore - numElementsRead,
            (uint64_t)(numElementsPerPage - pageCursor.elemPosInPage));
        auto physicalPageIdx = pageMapper(pageCursor.pageIdx);
        auto frame = bufferManager.pin(fileHandle, physicalPageIdx) +
                     getElemByteOffset(pageCursor.elemPosInPage);
        for (auto i = 0u; i < numElementsToReadInCurPage; i++) {
            auto relID = *(int64_t*)frame;
            if (listsUpdateStore->isRelDeletedInPersistentStore(
                    storageStructureIDAndFName.storageStructureID.listFileID, nodeOffset, relID)) {
                deletedRelOffsetsInList.emplace(numElementsRead);
            }
            numElementsRead++;
            frame += elementSize;
        }
        bufferManager.unpin(fileHandle, physicalPageIdx);
        pageCursor.nextPage();
    }
    return deletedRelOffsetsInList;
}

} // namespace storage
} // namespace kuzu
