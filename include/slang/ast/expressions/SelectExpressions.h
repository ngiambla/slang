//------------------------------------------------------------------------------
//! @file SelectExpressions.h
//! @brief Definitions for selection expressions
//
// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#pragma once

#include "slang/ast/Expression.h"
#include "slang/syntax/SyntaxFwd.h"

namespace slang::ast {

class FieldSymbol;

/// Represents a single element selection expression.
class SLANG_EXPORT ElementSelectExpression : public Expression {
public:
    ElementSelectExpression(const Type& type, Expression& value, const Expression& selector,
                            SourceRange sourceRange) :
        Expression(ExpressionKind::ElementSelect, type, sourceRange),
        value_(&value), selector_(&selector) {}

    const Expression& value() const { return *value_; }
    Expression& value() { return *value_; }

    const Expression& selector() const { return *selector_; }

    bool isConstantSelect(EvalContext& context) const;

    ConstantValue evalImpl(EvalContext& context) const;
    LValue evalLValueImpl(EvalContext& context) const;
    bool requireLValueImpl(const ASTContext& context, SourceLocation location,
                           bitmask<AssignFlags> flags, const Expression* longestStaticPrefix) const;

    void getLongestStaticPrefixesImpl(
        SmallVector<std::pair<const ValueSymbol*, const Expression*>>& results,
        EvalContext& evalContext, const Expression* longestStaticPrefix) const;

    std::optional<ConstantRange> evalIndex(EvalContext& context, const ConstantValue& val,
                                           ConstantValue& associativeIndex, bool& softFail) const;

    void serializeTo(ASTSerializer& serializer) const;

    static Expression& fromSyntax(Compilation& compilation, Expression& value,
                                  const syntax::ExpressionSyntax& syntax, SourceRange fullRange,
                                  const ASTContext& context);

    static Expression& fromConstant(Compilation& compilation, Expression& value, int32_t index,
                                    const ASTContext& context);

    static bool isKind(ExpressionKind kind) { return kind == ExpressionKind::ElementSelect; }

    template<typename TVisitor>
    void visitExprs(TVisitor&& visitor) const {
        value().visit(visitor);
        selector().visit(visitor);
    }

private:
    Expression* value_;
    const Expression* selector_;
    bool warnedAboutIndex = false;
};

/// Represents a range selection expression.
class SLANG_EXPORT RangeSelectExpression : public Expression {
public:
    RangeSelectExpression(RangeSelectionKind selectionKind, const Type& type, Expression& value,
                          const Expression& left, const Expression& right,
                          SourceRange sourceRange) :
        Expression(ExpressionKind::RangeSelect, type, sourceRange),
        value_(&value), left_(&left), right_(&right), selectionKind(selectionKind) {}

    const Expression& value() const { return *value_; }
    Expression& value() { return *value_; }

    const Expression& left() const { return *left_; }
    const Expression& right() const { return *right_; }

    RangeSelectionKind getSelectionKind() const { return selectionKind; }

    bool isConstantSelect(EvalContext& context) const;

    ConstantValue evalImpl(EvalContext& context) const;
    LValue evalLValueImpl(EvalContext& context) const;
    bool requireLValueImpl(const ASTContext& context, SourceLocation location,
                           bitmask<AssignFlags> flags, const Expression* longestStaticPrefix) const;

    void getLongestStaticPrefixesImpl(
        SmallVector<std::pair<const ValueSymbol*, const Expression*>>& results,
        EvalContext& evalContext, const Expression* longestStaticPrefix) const;

    std::optional<ConstantRange> evalRange(EvalContext& context, const ConstantValue& val) const;

    void serializeTo(ASTSerializer& serializer) const;

    static Expression& fromSyntax(Compilation& compilation, Expression& value,
                                  const syntax::RangeSelectSyntax& syntax, SourceRange fullRange,
                                  const ASTContext& context);

    static Expression& fromConstant(Compilation& compilation, Expression& value,
                                    ConstantRange range, const ASTContext& context);

    static bool isKind(ExpressionKind kind) { return kind == ExpressionKind::RangeSelect; }

    template<typename TVisitor>
    void visitExprs(TVisitor&& visitor) const {
        value().visit(visitor);
        left().visit(visitor);
        right().visit(visitor);
    }

private:
    Expression* value_;
    const Expression* left_;
    const Expression* right_;
    RangeSelectionKind selectionKind;
    bool warnedAboutRange = false;
};

/// Represents an access of a structure variable's members.
class SLANG_EXPORT MemberAccessExpression : public Expression {
public:
    const Symbol& member;

    MemberAccessExpression(const Type& type, Expression& value, const Symbol& member,
                           SourceRange sourceRange) :
        Expression(ExpressionKind::MemberAccess, type, sourceRange),
        member(member), value_(&value) {}

    const Expression& value() const { return *value_; }
    Expression& value() { return *value_; }

    ConstantValue evalImpl(EvalContext& context) const;
    LValue evalLValueImpl(EvalContext& context) const;
    bool requireLValueImpl(const ASTContext& context, SourceLocation location,
                           bitmask<AssignFlags> flags, const Expression* longestStaticPrefix) const;

    void getLongestStaticPrefixesImpl(
        SmallVector<std::pair<const ValueSymbol*, const Expression*>>& results,
        EvalContext& evalContext, const Expression* longestStaticPrefix) const;

    std::optional<ConstantRange> getSelectRange() const;

    void serializeTo(ASTSerializer& serializer) const;

    static Expression& fromSelector(
        Compilation& compilation, Expression& expr, const LookupResult::MemberSelector& selector,
        const syntax::InvocationExpressionSyntax* invocation,
        const syntax::ArrayOrRandomizeMethodExpressionSyntax* withClause,
        const ASTContext& context);

    static Expression& fromSyntax(Compilation& compilation,
                                  const syntax::MemberAccessExpressionSyntax& syntax,
                                  const syntax::InvocationExpressionSyntax* invocation,
                                  const syntax::ArrayOrRandomizeMethodExpressionSyntax* withClause,
                                  const ASTContext& context);

    static bool isKind(ExpressionKind kind) { return kind == ExpressionKind::MemberAccess; }

private:
    Expression* value_;
};

} // namespace slang::ast
