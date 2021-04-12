#pragma once

#include "src/common/include/types.h"
#include "src/expression/include/logical/logical_expression.h"
#include "src/planner/include/logical_plan/operator/logical_operator.h"

using namespace graphflow::expression;
using namespace graphflow::common;
using namespace std;

namespace graphflow {
namespace planner {

class LogicalFilter : public LogicalOperator {

public:
    LogicalFilter(shared_ptr<LogicalExpression> rootExpr, shared_ptr<LogicalOperator> prevOperator)
        : LogicalOperator{prevOperator}, rootExpr{rootExpr} {}

    LogicalOperatorType getLogicalOperatorType() const override {
        return LogicalOperatorType::LOGICAL_FILTER;
    }

    const LogicalExpression& getRootLogicalExpression() const { return *rootExpr; }

    string getOperatorInformation() const override { return rootExpr->getRawExpression(); }

public:
    shared_ptr<LogicalExpression> rootExpr;
};

} // namespace planner
} // namespace graphflow
