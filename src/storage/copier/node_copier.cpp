#include "storage/copier/node_copier.h"

#include "common/string_utils.h"
#include "storage/copier/copy_task.h"
#include "storage/storage_structure/in_mem_file.h"

using namespace kuzu::catalog;
using namespace kuzu::common;

namespace kuzu {
namespace storage {

template<typename T>
std::unique_ptr<NodeCopyMorsel<arrow::Array>> CSVNodeCopySharedState<T>::getMorsel() {
    lock_t lck{this->mtx};
    std::shared_ptr<arrow::RecordBatch> recordBatch;
    auto result = csvStreamingReader->ReadNext(&recordBatch);
    if (!result.ok()) {
        throw common::CopyException(
            "Error reading a batch of rows from CSV using Arrow CSVStreamingReader.");
    }
    if (recordBatch == NULL) {
        return make_unique<CSVNodeCopyMorsel>(move(recordBatch), INVALID_NODE_OFFSET,
            NodeCopyMorsel<arrow::Array>::INVALID_BLOCK_IDX);
    }
    auto numRows = recordBatch->num_rows();
    this->startOffset += numRows;
    this->blockIdx++;
    return make_unique<CSVNodeCopyMorsel>(
        move(recordBatch), this->startOffset - numRows, this->blockIdx - 1);
}

template<typename T>
std::unique_ptr<NodeCopyMorsel<arrow::ChunkedArray>> ParquetNodeCopySharedState<T>::getMorsel() {
    lock_t lck{this->mtx};
    std::shared_ptr<arrow::Table> currTable;
    if (this->blockIdx == numBlocks) {
        return make_unique<ParquetNodeCopyMorsel>(move(currTable), INVALID_NODE_OFFSET,
            NodeCopyMorsel<arrow::ChunkedArray>::INVALID_BLOCK_IDX);
    }
    auto result = parquetReader->RowGroup(this->blockIdx)->ReadTable(&currTable);
    if (!result.ok()) {
        throw common::CopyException(
            "Error reading a batch of rows from CSV using Arrow CSVStreamingReader.");
    }
    // TODO(Semih): I have not verified that, similar to CSV reading, that if ReadTable runs out of
    // blocks to read, then it sets the currTable to NULL.
    if (currTable == NULL) {
        return make_unique<ParquetNodeCopyMorsel>(move(currTable), INVALID_NODE_OFFSET,
            NodeCopyMorsel<arrow::ChunkedArray>::INVALID_BLOCK_IDX);
    }
    auto numRows = currTable->num_rows();
    this->startOffset += numRows;
    this->blockIdx++;
    return make_unique<ParquetNodeCopyMorsel>(
        move(currTable), this->startOffset - numRows, this->blockIdx - 1);
}

void NodeCopier::initializeColumnsAndLists() {
    logger->info("Initializing in memory columns.");
    for (auto& property : tableSchema->properties) {
        auto fName = StorageUtils::getNodePropertyColumnFName(
            outputDirectory, tableSchema->tableID, property.propertyID, DBFileType::WAL_VERSION);
        columns[property.propertyID] = InMemBMPageCollectionFactory::getInMemBMPageCollection(
            fName, property.dataType, numRows, getNumBlocks());
    }
    logger->info("Done initializing in memory columns.");
}

void NodeCopier::populateColumnsAndLists(processor::ExecutionContext* executionContext) {
    arrow::Status status;
    auto primaryKey = reinterpret_cast<NodeTableSchema*>(tableSchema)->getPrimaryKey();
    switch (primaryKey.dataType.typeID) {
    case INT64: {
        status = populateColumns<int64_t>(executionContext);
    } break;
    case STRING: {
        status = populateColumns<ku_string_t>(executionContext);
    } break;
    default: {
        throw CopyException(StringUtils::string_format("Unsupported data type {} for the ID index.",
            Types::dataTypeToString(primaryKey.dataType)));
    }
    }
    throwCopyExceptionIfNotOK(status);
}

void NodeCopier::saveToFile() {
    logger->debug("Writing node columns to disk.");
    assert(!columns.empty());
    for (auto& [_, column] : columns) {
        taskScheduler.scheduleTask(CopyTaskFactory::createCopyTask(
            [&](NodeInMemColumn* x) { x->saveToFile(); }, column.get()));
    }
    taskScheduler.waitAllTasksToCompleteOrError();
    logger->debug("Done writing node columns to disk.");
}

template<typename T>
arrow::Status NodeCopier::populateColumns(processor::ExecutionContext* executionContext) {
    logger->info("Populating properties");
    auto pkIndex =
        std::make_unique<HashIndexBuilder<T>>(StorageUtils::getNodeIndexFName(this->outputDirectory,
                                                  tableSchema->tableID, DBFileType::WAL_VERSION),
            reinterpret_cast<NodeTableSchema*>(tableSchema)->getPrimaryKey().dataType);
    pkIndex->bulkReserve(numRows);
    arrow::Status status;
    switch (copyDescription.fileType) {
    case CopyDescription::FileType::CSV:
        status = populateColumnsFromCSV<T>(executionContext, pkIndex);
        break;
    case CopyDescription::FileType::PARQUET:
        status = populateColumnsFromParquet<T>(executionContext, pkIndex);
        break;
    default: {
        throw CopyException(StringUtils::string_format("Unsupported file type {}.",
            CopyDescription::getFileTypeName(copyDescription.fileType)));
    }
    }
    logger->info("Flush the pk index to disk.");
    pkIndex->flush();
    logger->info("Done populating properties, constructing the pk index.");
    return status;
}

template<typename T>
arrow::Status NodeCopier::populateColumnsFromCSV(
    processor::ExecutionContext* executionContext, std::unique_ptr<HashIndexBuilder<T>>& pkIndex) {
    for (auto& filePath : copyDescription.filePaths) {
        std::shared_ptr<arrow::csv::StreamingReader> csvStreamingReader;
        auto status = initCSVReaderAndCheckStatus(csvStreamingReader, filePath);
        throwCopyExceptionIfNotOK(status);
        CSVNodeCopySharedState sharedState{
            filePath, pkIndex.get(), fileBlockInfos.at(filePath).startOffset, csvStreamingReader};
        taskScheduler.scheduleTaskAndWaitOrError(
            CopyTaskFactory::createParallelCopyTask(executionContext->numThreads,
                batchPopulateColumnsTask<T, arrow::Array>, &sharedState, this, executionContext),
            executionContext);
    }
    return arrow::Status::OK();
}

template<typename T>
arrow::Status NodeCopier::populateColumnsFromParquet(
    processor::ExecutionContext* executionContext, std::unique_ptr<HashIndexBuilder<T>>& pkIndex) {
    for (auto& filePath : copyDescription.filePaths) {
        std::unique_ptr<parquet::arrow::FileReader> parquetReader;
        auto status = initParquetReaderAndCheckStatus(parquetReader, filePath);
        throwCopyExceptionIfNotOK(status);
        ParquetNodeCopySharedState sharedState{filePath, pkIndex.get(),
            fileBlockInfos.at(filePath).startOffset, fileBlockInfos.at(filePath).numBlocks,
            std::move(parquetReader)};
        taskScheduler.scheduleTaskAndWaitOrError(
            CopyTaskFactory::createParallelCopyTask(executionContext->numThreads,
                batchPopulateColumnsTask<T, arrow::ChunkedArray>, &sharedState, this,
                executionContext),
            executionContext);
    }
    return arrow::Status::OK();
}

template<typename T>
void NodeCopier::populatePKIndex(InMemColumnChunk* chunk, InMemOverflowFile* overflowFile,
    common::NullMask* nullMask, HashIndexBuilder<T>* pkIndex, offset_t startOffset,
    uint64_t numValues) {
    for (auto i = 0u; i < numValues; i++) {
        auto offset = i + startOffset;
        if (nullMask->isNull(offset)) {
            throw ReaderException("Primary key cannot be null.");
        }
        appendPKIndex(chunk, overflowFile, offset, pkIndex);
    }
}

template<typename T1, typename T2>
void NodeCopier::batchPopulateColumnsTask(NodeCopySharedState<T1, T2>* sharedState,
    NodeCopier* copier, processor::ExecutionContext* executionContext) {
    while (true) {
        if (executionContext->clientContext->isInterrupted()) {
            throw common::InterruptException{};
        }
        auto result = sharedState->getMorsel();
        if (!result->success()) {
            break;
        }
        copier->logger->trace(
            "Start: path={0} blkIdx={1}", sharedState->filePath, result->blockIdx);
        auto numLinesInCurBlock =
            copier->fileBlockInfos.at(sharedState->filePath).numLinesPerBlock[result->blockIdx];
        // Create a column chunk for tuples within the [StartOffset, endOffset] range.
        auto endOffset = result->startOffset + numLinesInCurBlock - 1;
        std::unordered_map<uint64_t, std::unique_ptr<InMemColumnChunk>> chunks;
        for (auto& [propertyIdx, column] : copier->columns) {
            chunks[propertyIdx] = std::make_unique<InMemColumnChunk>(result->startOffset, endOffset,
                column->getNumBytesForElement(), column->getNumElementsInAPage());
        }
        std::vector<PageByteCursor> overflowCursors(copier->tableSchema->getNumProperties());
        for (auto& [propertyIdx, column] : copier->columns) {
            putPropsOfLinesIntoColumns(chunks.at(propertyIdx).get(), column.get(),
                result->getArrowColumns()[propertyIdx], result->startOffset, numLinesInCurBlock,
                copier->copyDescription, overflowCursors[propertyIdx]);
        }
        // Flush each page within the [StartOffset, endOffset] range.
        for (auto& [propertyIdx, column] : copier->columns) {
            column->flushChunk(chunks[propertyIdx].get(), result->startOffset, endOffset);
        }

        auto primaryKeyPropertyIdx =
            reinterpret_cast<NodeTableSchema*>(copier->tableSchema)->primaryKeyPropertyID;
        auto pkColumn = copier->columns.at(primaryKeyPropertyIdx).get();
        populatePKIndex(chunks[primaryKeyPropertyIdx].get(), pkColumn->getInMemOverflowFile(),
            pkColumn->getNullMask(), sharedState->pkIndex, result->startOffset, numLinesInCurBlock);
        copier->logger->info("End: path={0} blkIdx={1}", sharedState->filePath, result->blockIdx);
    }
}

template<typename T>
void NodeCopier::putPropsOfLinesIntoColumns(InMemColumnChunk* columnChunk, NodeInMemColumn* column,
    std::shared_ptr<T> arrowArray, common::offset_t startNodeOffset, uint64_t numLinesInCurBlock,
    CopyDescription& copyDescription, PageByteCursor& overflowCursor) {
    auto setElementFunc =
        getSetElementFunc(column->getDataType().typeID, copyDescription, overflowCursor);
    for (auto i = 0u; i < numLinesInCurBlock; i++) {
        auto nodeOffset = startNodeOffset + i;
        auto currentToken = arrowArray->GetScalar(i);
        if ((*currentToken)->is_valid) {
            auto stringToken = currentToken->get()->ToString();
            setElementFunc(column, columnChunk, nodeOffset, stringToken);
        }
    }
}

template<>
void NodeCopier::appendPKIndex(InMemColumnChunk* chunk, InMemOverflowFile* overflowFile,
    common::offset_t offset, HashIndexBuilder<int64_t>* pkIndex) {
    auto element = *(int64_t*)chunk->getValue(offset);
    if (!pkIndex->append(element, offset)) {
        throw common::CopyException(
            common::Exception::getExistedPKExceptionMsg(std::to_string(element)));
    }
}

template<>
void NodeCopier::appendPKIndex(InMemColumnChunk* chunk, InMemOverflowFile* overflowFile,
    common::offset_t offset, HashIndexBuilder<common::ku_string_t>* pkIndex) {
    auto element = *(ku_string_t*)chunk->getValue(offset);
    auto key = overflowFile->readString(&element);
    if (!pkIndex->append(key.c_str(), offset)) {
        throw common::CopyException(common::Exception::getExistedPKExceptionMsg(key));
    }
}

set_element_func_t NodeCopier::getSetElementFunc(common::DataTypeID typeID,
    common::CopyDescription& copyDescription, PageByteCursor& pageByteCursor) {
    switch (typeID) {
    case common::DataTypeID::INT64:
        return setNumericElement<int64_t>;
    case common::DataTypeID::INT32:
        return setNumericElement<int32_t>;
    case common::DataTypeID::INT16:
        return setNumericElement<int16_t>;
    case common::DataTypeID::DOUBLE:
        return setNumericElement<double_t>;
    case common::DataTypeID::FLOAT:
        return setNumericElement<float_t>;
    case common::DataTypeID::BOOL:
        return setBoolElement;
    case common::DataTypeID::DATE:
        return setTimeElement<common::Date>;
    case common::DataTypeID::TIMESTAMP:
        return setTimeElement<common::Timestamp>;
    case common::DataTypeID::INTERVAL:
        return setTimeElement<common::Interval>;
    case common::DataTypeID::STRING:
        return std::bind(setStringElement, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3, std::placeholders::_4, pageByteCursor);
    case common::DataTypeID::VAR_LIST:
        return std::bind(setVarListElement, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3, std::placeholders::_4, copyDescription, pageByteCursor);
    case common::DataTypeID::FIXED_LIST:
        return std::bind(setFixedListElement, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3, std::placeholders::_4, copyDescription);
    default:
        throw common::RuntimeException("Unsupported data type.");
    }
}

} // namespace storage
} // namespace kuzu
