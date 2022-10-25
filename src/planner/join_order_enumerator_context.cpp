#include "src/planner/include/join_order_enumerator_context.h"

namespace graphflow {
namespace planner {

void JoinOrderEnumeratorContext::init(QueryGraph* queryGraph, expression_vector& predicates) {
    whereExpressionsSplitOnAND = predicates;
    this->queryGraph = queryGraph;
    // clear and resize subPlansTable
    subPlansTable->clear();
    maxLevel = queryGraph->getNumQueryNodes() + queryGraph->getNumQueryRels() + 1;
    subPlansTable->resize(maxLevel);
    // Restart from level 1 for new query part so that we get hashJoin based plans
    // that uses subplans coming from previous query part.See example in planRelIndexJoin().
    currentLevel = 1;
}

SubqueryGraph JoinOrderEnumeratorContext::getFullyMatchedSubqueryGraph() const {
    auto subqueryGraph = SubqueryGraph(*queryGraph);
    for (auto i = 0u; i < queryGraph->getNumQueryNodes(); ++i) {
        subqueryGraph.addQueryNode(i);
    }
    for (auto i = 0u; i < queryGraph->getNumQueryRels(); ++i) {
        subqueryGraph.addQueryRel(i);
    }
    return subqueryGraph;
}

void JoinOrderEnumeratorContext::resetState() {
    subPlansTable = make_unique<SubPlansTable>();
}

} // namespace planner
} // namespace graphflow
