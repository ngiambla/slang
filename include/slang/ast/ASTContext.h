//------------------------------------------------------------------------------
//! @file ASTContext.h
//! @brief AST creation context
//
// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#pragma once

#include <tuple>

#include "slang/ast/EvalContext.h"
#include "slang/ast/Lookup.h"
#include "slang/numeric/ConstantValue.h"
#include "slang/syntax/SyntaxFwd.h"
#include "slang/util/Hash.h"
#include "slang/util/Util.h"

namespace slang::ast {

class Expression;
class InstanceSymbolBase;
class ProceduralBlockSymbol;
class Scope;
class Statement;
class TempVarSymbol;
class Type;
class VariableSymbol;
enum class RandMode;

/// Specifies flags that control expression and statement creation.
enum class SLANG_EXPORT ASTFlags : uint64_t {
    /// No special behavior specified.
    None = 0,

    /// The expression is inside a concatenation; this enables slightly
    /// different creation rules.
    InsideConcatenation = 1ull << 0,

    /// The expression is inside the unevaluated side of a conditional branch.
    /// This is used to avoid issuing warnings for things that won't happen.
    UnevaluatedBranch = 1ull << 1,

    /// Allow the expression to also be a data type; used in a few places like
    /// the first argument to system methods like $bits
    AllowDataType = 1ull << 2,

    /// The expression being created is an enum value initializer.
    EnumInitializer = 1ull << 3,

    /// Attributes are disallowed on expressions in this context.
    NoAttributes = 1ull << 4,

    /// Assignment is allowed in this context. This flag is cleared
    /// for nested subexpressions, unless they are directly parenthesized.
    AssignmentAllowed = 1ull << 5,

    /// Assignments are disallowed in this context. As opposed to the AssignmentAllowed
    /// flag, this is not cleared and overrides that fact even if we are in a
    /// procedural context and would otherwise be allowed to modify variables.
    AssignmentDisallowed = 1ull << 6,

    /// Expression is not inside a procedural context.
    NonProcedural = 1ull << 7,

    /// Expression is for a static variable's initializer. References to automatic
    /// variables will be disallowed.
    StaticInitializer = 1ull << 8,

    /// Streaming operator is allowed in assignment target, assignment source, bit-stream casting
    /// argument, or stream expressions of another streaming concatenation. This flag is cleared for
    /// nested subexpressions, unless they are directly parenthesized.
    StreamingAllowed = 1ull << 9,

    /// This is the first expression appearing as an expression statement; potentially this
    /// indicates whether a subroutine invocation is as a task (if set) or as a function (unset).
    /// Cleared for nested subexpressions.
    TopLevelStatement = 1ull << 10,

    /// Expression is allowed to be the unbounded literal '$' such as inside a queue select.
    AllowUnboundedLiteral = 1ull << 11,

    /// Expression is allowed to do arithmetic with an unbounded literal.
    AllowUnboundedLiteralArithmetic = 1ull << 12,

    /// AST creation is happening within a function body
    Function = 1ull << 13,

    /// AST creation is happening within a final block.
    Final = 1ull << 14,

    /// AST creation is happening within the intra-assignment timing control of
    /// a non-blocking assignment expression.
    NonBlockingTimingControl = 1ull << 15,

    /// AST creation is happening within an event expression.
    EventExpression = 1ull << 16,

    /// AST creation is in a context where type reference expressions are allowed.
    AllowTypeReferences = 1ull << 17,

    /// AST creation is happening within an assertion expression (sequence or property).
    AssertionExpr = 1ull << 18,

    /// Allow binding a clocking block as part of a top-level event expression.
    AllowClockingBlock = 1ull << 19,

    /// AST creation is for checking an assertion argument, prior to it being expanded as
    /// part of an actual instance.
    AssertionInstanceArgCheck = 1ull << 20,

    /// AST creation is for a cycle delay or sequence repetition, where references to
    /// assertion formal ports have specific type requirements.
    AssertionDelayOrRepetition = 1ull << 21,

    /// AST creation is for the left hand side of an assignment operation.
    LValue = 1ull << 22,

    /// AST creation is for the negation of a property, which disallows recursive
    /// instantiations.
    PropertyNegation = 1ull << 23,

    /// AST creation is for a property that has come after a positive advancement
    /// of time within the parent property definition.
    PropertyTimeAdvance = 1ull << 24,

    /// AST creation is for an argument passed to a recursive property instance.
    RecursivePropertyArg = 1ull << 25,

    /// AST creation is inside a concurrent assertion's action block.
    ConcurrentAssertActionBlock = 1ull << 26,

    /// AST creation is for a covergroup expression that permits referencing a
    /// formal argument of an overridden sample method.
    AllowCoverageSampleFormal = 1ull << 27,

    /// Expressions are allowed to reference coverpoint objects directly.
    AllowCoverpoint = 1ull << 28,

    /// User-defined nettypes are allowed to be looked up in this context.
    AllowNetType = 1ull << 29,

    /// AST creation is for an output (or inout) port or function argument.
    OutputArg = 1ull << 30,

    /// AST creation is for a procedural assign statement.
    ProceduralAssign = 1ull << 31,

    /// AST creation is for a procedural force / release / deassign statement.
    ProceduralForceRelease = 1ull << 32,

    /// AST creation is in a context that allows interconnect nets.
    AllowInterconnect = 1ull << 33,

    /// AST creation is in a context where drivers should not be registered for
    /// lvalues, even if they otherwise would normally be. This is used, for example,
    /// in potentially unrollable for loops to let the loop unroller handle the drivers.
    NotADriver = 1ull << 34,

    /// AST creation is for a range expression inside a streaming concatenation operator.
    StreamingWithRange = 1ull << 35,

    /// AST creation is happening inside a specify block.
    SpecifyBlock = 1ull << 36,

    /// AST creation is for a DPI argument type.
    DPIArg = 1ull << 37,

    /// AST creation is for an assertion instance's default argument.
    AssertionDefaultArg = 1ull << 38,

    /// AST creation is for an lvalue that also counts as an rvalue. Only valid
    /// when combined with the LValue flag -- used for things like the pre & post
    /// increment and decrement operators.
    LAndRValue = 1ull << 39,

    /// AST binding should not count symbol references towards that symbol being "used".
    /// If this flag is not set, accessing a variable or net in an expression will count
    /// that symbol as being "used".
    NoReference = 1ull << 40
};
BITMASK(ASTFlags, NoReference)

// clang-format off
#define DK(x) \
    x(Unknown) \
    x(Range) \
    x(AbbreviatedRange) \
    x(Dynamic) \
    x(Associative) \
    x(Queue) \
    x(DPIOpenArray)
// clang-format on
ENUM(DimensionKind, DK)
#undef DK

struct SLANG_EXPORT EvaluatedDimension {
    DimensionKind kind = DimensionKind::Unknown;
    ConstantRange range;
    const Type* associativeType = nullptr;
    uint32_t queueMaxSize = 0;

    bool isRange() const {
        return kind == DimensionKind::Range || kind == DimensionKind::AbbreviatedRange;
    }
};

/// Contains required context for binding syntax nodes with symbols to form
/// an AST. Expressions, statements, timing controls, constraints, and assertion
/// items all use this for creation.
class SLANG_EXPORT ASTContext {
public:
    /// The scope where the AST creation is occurring.
    not_null<const Scope*> scope;

    /// The location to use when looking up names.
    SymbolIndex lookupIndex;

    /// Various flags that control how AST creation is performed.
    bitmask<ASTFlags> flags;

private:
    const Symbol* instanceOrProc = nullptr;

public:
    /// If any temporary variables have been materialized in this context,
    /// contains a pointer to the first one along with a linked list of any
    /// others that may be active. Otherwise nullptr.
    const TempVarSymbol* firstTempVar = nullptr;

    /// A collection of information needed to bind names inside inline constraint
    /// blocks for class and scope randomize function calls.
    struct RandomizeDetails {
        /// The scope of the class type itself, if randomizing a class.
        const Scope* classType = nullptr;

        /// If randomizing a class via a dotted handle access, this is
        /// the the class handle symbol.
        const Symbol* thisVar = nullptr;

        /// A list of names to which class-scoped lookups are restricted.
        /// If empty, the lookup is unrestricted and all names are first
        /// tried in class-scope.
        std::span<const std::string_view> nameRestrictions;

        /// A set of variables for a scope randomize call that should be
        /// treated as a rand variable.
        flat_hash_set<const Symbol*> scopeRandVars;
    };

    /// If this context is for creating an inline constraint block for a randomize
    /// function call, this points to information about the scope. Name lookups
    /// happen inside the class scope before going through the normal local lookup,
    /// for example.
    const RandomizeDetails* randomizeDetails = nullptr;

    /// Information required to instantiate a sequence or property instance.
    struct AssertionInstanceDetails {
        /// The assertion member being instantiated.
        const Symbol* symbol = nullptr;

        /// The previous AST context used to start the instantiation.
        /// This effectively forms a linked list when expanding a nested
        /// stack of sequence and property instances.
        const ASTContext* prevContext = nullptr;

        /// The location where the instance is being instantiated.
        SourceLocation instanceLoc;

        /// A map of formal argument symbols to their actual replacements.
        flat_hash_map<const Symbol*, std::tuple<const syntax::PropertyExprSyntax*, ASTContext>>
            argumentMap;

        /// A map of local variables declared in the assertion item.
        /// These don't exist in any scope because their types can depend
        /// on the expanded arguments.
        flat_hash_map<std::string_view, const Symbol*> localVars;

        /// If an argument to a sequence or property is being expanded, this
        /// member contains the source location where the argument was referenced.
        SourceLocation argExpansionLoc;

        /// If an argument is being expanded, this is the context in which the
        /// argument was originally being created (as opposed to where it is being
        /// expanded now).
        const AssertionInstanceDetails* argDetails = nullptr;

        /// Indicates whether this particular instance has already been seen
        /// previously in the stack of assertion instances being expanded.
        /// Only applicable to properties, since this is illegal for sequences.
        bool isRecursive = false;
    };

    /// If this context is for creating an instantiation of a sequence or
    /// property this points to information about that instantiation.
    const AssertionInstanceDetails* assertionInstance = nullptr;

    ASTContext(const Scope& scope, LookupLocation lookupLocation,
               bitmask<ASTFlags> flags = ASTFlags::None) :
        scope(&scope),
        lookupIndex(lookupLocation.getIndex()), flags(flags) {
        ASSERT(!lookupLocation.getScope() || lookupLocation.getScope() == &scope);
    }

    Compilation& getCompilation() const { return scope->getCompilation(); }
    LookupLocation getLocation() const { return LookupLocation(scope, uint32_t(lookupIndex)); }
    bool inUnevaluatedBranch() const { return (flags & ASTFlags::UnevaluatedBranch) != 0; }

    DriverKind getDriverKind() const;
    const InstanceSymbolBase* getInstance() const;
    const ProceduralBlockSymbol* getProceduralBlock() const;
    const SubroutineSymbol* getContainingSubroutine() const;
    bool inAlwaysCombLatch() const;

    void setInstance(const InstanceSymbolBase& inst);
    void setProceduralBlock(const ProceduralBlockSymbol& block);
    void clearInstanceAndProc() { instanceOrProc = nullptr; }

    void setAttributes(const Statement& stmt,
                       std::span<const syntax::AttributeInstanceSyntax* const> syntax) const;

    void setAttributes(const Expression& expr,
                       std::span<const syntax::AttributeInstanceSyntax* const> syntax) const;

    void addDriver(const ValueSymbol& symbol, const Expression& longestStaticPrefix,
                   bitmask<AssignFlags> assignFlags) const;

    const Symbol& getContainingSymbol() const;

    Diagnostic& addDiag(DiagCode code, SourceLocation location) const;
    Diagnostic& addDiag(DiagCode code, SourceRange sourceRange) const;

    bool requireIntegral(const Expression& expr) const;
    bool requireIntegral(const ConstantValue& cv, SourceRange range) const;
    bool requireNoUnknowns(const SVInt& value, SourceRange range) const;
    bool requirePositive(const SVInt& value, SourceRange range) const;
    bool requirePositive(std::optional<int32_t> value, SourceRange range) const;
    bool requireGtZero(std::optional<int32_t> value, SourceRange range) const;
    bool requireBooleanConvertible(const Expression& expr) const;
    bool requireValidBitWidth(bitwidth_t width, SourceRange range) const;
    std::optional<bitwidth_t> requireValidBitWidth(const SVInt& value, SourceRange range) const;

    ConstantValue eval(const Expression& expr, bitmask<EvalFlags> extraFlags = {}) const;
    ConstantValue tryEval(const Expression& expr) const;

    std::optional<int32_t> evalInteger(const syntax::ExpressionSyntax& syntax,
                                       bitmask<ASTFlags> extraFlags = {}) const;
    std::optional<int32_t> evalInteger(const Expression& expr,
                                       bitmask<EvalFlags> extraFlags = {}) const;
    EvaluatedDimension evalDimension(const syntax::VariableDimensionSyntax& syntax,
                                     bool requireRange, bool isPacked) const;

    EvaluatedDimension evalPackedDimension(const syntax::VariableDimensionSyntax& syntax) const;
    EvaluatedDimension evalPackedDimension(const syntax::ElementSelectSyntax& syntax) const;
    EvaluatedDimension evalUnpackedDimension(const syntax::VariableDimensionSyntax& syntax) const;

    /// Subroutine argument expressions are parsed as property expressions, since we don't know
    /// up front whether they will be used to instantiate a property or a sequence or are actually
    /// for a subroutine. This method unwraps for the case where we are calling a subroutine.
    const syntax::ExpressionSyntax* requireSimpleExpr(const syntax::PropertyExprSyntax& expr) const;
    const syntax::ExpressionSyntax* requireSimpleExpr(const syntax::PropertyExprSyntax& expr,
                                                      DiagCode code) const;

    /// Gets the rand mode for the given symbol, taking into account any randomize
    /// scope that may be active in this context.
    RandMode getRandMode(const Symbol& symbol) const;

    /// If this context is within an assertion instance, report a backtrace of how that
    /// instance was expanded to the given diagnostic; otherwise, do nothing.
    void addAssertionBacktrace(Diagnostic& diag) const;

    ASTContext resetFlags(bitmask<ASTFlags> addedFlags) const;

private:
    void evalRangeDimension(const syntax::SelectorSyntax& syntax, bool isPacked,
                            EvaluatedDimension& result) const;
};

} // namespace slang::ast
