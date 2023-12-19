#pragma once

#include "binder/expression/expression.h"
#include "common/enums/clause_type.h"

namespace kuzu {
namespace binder {

class BoundReadingClause {
public:
    explicit BoundReadingClause(common::ClauseType clauseType) : clauseType{clauseType} {}
    BoundReadingClause(const BoundReadingClause& other)
        : clauseType{other.clauseType}, predicate{other.predicate} {}
    virtual ~BoundReadingClause() = default;

    common::ClauseType getClauseType() const { return clauseType; }

    inline void setPredicate(std::shared_ptr<Expression> predicate_) {
        predicate = std::move(predicate_);
    }
    inline bool hasPredicate() const { return predicate != nullptr; }
    inline std::shared_ptr<Expression> getPredicate() const { return predicate; }
    inline expression_vector getConjunctivePredicates() const {
        return hasPredicate() ? predicate->splitOnAND() : expression_vector{};
    }

    inline virtual std::unique_ptr<BoundReadingClause> copy() = 0;

private:
    common::ClauseType clauseType;
    // Predicate in WHERE clause
    std::shared_ptr<Expression> predicate;
};
} // namespace binder
} // namespace kuzu
