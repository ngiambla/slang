//------------------------------------------------------------------------------
// ArrayMethods.cpp
// Built-in methods on arrays
//
// File is under the MIT license; see LICENSE for details
//------------------------------------------------------------------------------
#include "slang/binding/SystemSubroutine.h"
#include "slang/compilation/Compilation.h"
#include "slang/diagnostics/ConstEvalDiags.h"
#include "slang/diagnostics/SysFuncsDiags.h"
#include "slang/symbols/VariableSymbols.h"
#include "slang/util/Function.h"

namespace slang::Builtins {

class ArrayReductionMethod : public SystemSubroutine {
public:
    using Operator = function_ref<void(SVInt&, const SVInt&)>;

    ArrayReductionMethod(const std::string& name, Operator op) :
        SystemSubroutine(name, SubroutineKind::Function), op(op) {
        withClauseMode = WithClauseMode::Iterator;
    }

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression* iterExpr) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, true, args, range, 0, 0))
            return comp.getErrorType();

        if (iterExpr) {
            if (!iterExpr->type->isIntegral()) {
                context.addDiag(diag::ArrayMethodIntegral, iterExpr->sourceRange) << name;
                return comp.getErrorType();
            }

            return *iterExpr->type;
        }
        else {
            auto elemType = args[0]->type->getArrayElementType();
            ASSERT(elemType);

            if (!elemType->isIntegral()) {
                context.addDiag(diag::ArrayMethodIntegral, args[0]->sourceRange) << name;
                return comp.getErrorType();
            }

            return *elemType;
        }
    }

    ConstantValue eval(EvalContext& context, const Args& args,
                       const CallExpression::SystemCallInfo& callInfo) const final {
        ConstantValue arr = args[0]->eval(context);
        if (!arr)
            return nullptr;

        if (callInfo.iterExpr) {
            ASSERT(callInfo.iterVar);
            if (arr.empty()) {
                auto elemType = callInfo.iterExpr->type;
                return SVInt(elemType->getBitWidth(), 0, elemType->isSigned());
            }

            auto it = begin(arr);
            auto iterVal = context.createLocal(callInfo.iterVar, *it);
            ConstantValue cv = callInfo.iterExpr->eval(context);
            if (!cv)
                return nullptr;

            SVInt result = cv.integer();
            for (++it; it != end(arr); ++it) {
                *iterVal = *it;
                cv = callInfo.iterExpr->eval(context);
                if (!cv)
                    return nullptr;

                op(result, cv.integer());
            }

            return result;
        }
        else {
            if (arr.empty()) {
                auto elemType = args[0]->type->getArrayElementType();
                return SVInt(elemType->getBitWidth(), 0, elemType->isSigned());
            }

            auto it = begin(arr);
            SVInt result = it->integer();
            for (++it; it != end(arr); ++it)
                op(result, it->integer());

            return result;
        }
    }

    bool verifyConstant(EvalContext&, const Args&, SourceRange) const final { return true; }

private:
    Operator op;
};

class ArraySortMethod : public SystemSubroutine {
public:
    ArraySortMethod(const std::string& name, bool reversed) :
        SystemSubroutine(name, SubroutineKind::Function), reversed(reversed) {
        withClauseMode = WithClauseMode::Iterator;
    }

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression* iterExpr) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, true, args, range, 0, 0))
            return comp.getErrorType();

        if (iterExpr) {
            if (!iterExpr->type->isIntegral()) {
                context.addDiag(diag::ArrayMethodIntegral, iterExpr->sourceRange) << name;
                return comp.getErrorType();
            }
        }
        else {
            auto elemType = args[0]->type->getArrayElementType();
            ASSERT(elemType);

            if (!elemType->isIntegral()) {
                context.addDiag(diag::ArrayMethodIntegral, args[0]->sourceRange) << name;
                return comp.getErrorType();
            }
        }

        return comp.getVoidType();
    }

    ConstantValue eval(EvalContext& context, const Args& args,
                       const CallExpression::SystemCallInfo& callInfo) const final {
        auto lval = args[0]->evalLValue(context);
        if (!lval)
            return nullptr;

        auto target = lval.resolve();
        if (!target)
            return nullptr;

        if (callInfo.iterExpr) {
            ASSERT(callInfo.iterVar);
            auto iterVal = context.createLocal(callInfo.iterVar);

            auto sortTarget = [&](auto& target) {
                auto pred = [&](ConstantValue& a, ConstantValue& b) {
                    *iterVal = a;
                    ConstantValue cva = callInfo.iterExpr->eval(context);

                    *iterVal = b;
                    ConstantValue cvb = callInfo.iterExpr->eval(context);

                    return cva < cvb;
                };

                if (reversed)
                    std::sort(target.rbegin(), target.rend(), pred);
                else
                    std::sort(target.begin(), target.end(), pred);
            };

            if (target->isQueue()) {
                sortTarget(*target->queue());
            }
            else {
                auto& vec = std::get<ConstantValue::Elements>(target->getVariant());
                sortTarget(vec);
            }
        }
        else {
            auto sortTarget = [&](auto& target) {
                if (reversed)
                    std::sort(target.rbegin(), target.rend());
                else
                    std::sort(target.begin(), target.end());
            };

            if (target->isQueue()) {
                sortTarget(*target->queue());
            }
            else {
                auto& vec = std::get<ConstantValue::Elements>(target->getVariant());
                sortTarget(vec);
            }
        }

        return nullptr;
    }

    bool verifyConstant(EvalContext&, const Args&, SourceRange) const final { return true; }

private:
    bool reversed;
};

class ArraySizeMethod : public SimpleSystemSubroutine {
public:
    ArraySizeMethod(Compilation& comp, const std::string& name) :
        SimpleSystemSubroutine(name, SubroutineKind::Function, 0, {}, comp.getIntType(), true) {}

    ConstantValue eval(EvalContext& context, const Args& args,
                       const CallExpression::SystemCallInfo&) const final {
        auto val = args[0]->eval(context);
        if (!val)
            return nullptr;

        return SVInt(32, val.size(), true);
    }
};

class DynArrayDeleteMethod : public SimpleSystemSubroutine {
public:
    explicit DynArrayDeleteMethod(Compilation& comp) :
        SimpleSystemSubroutine("delete", SubroutineKind::Function, 0, {}, comp.getVoidType(),
                               true) {}

    ConstantValue eval(EvalContext& context, const Args& args,
                       const CallExpression::SystemCallInfo&) const final {
        auto lval = args[0]->evalLValue(context);
        if (!lval)
            return nullptr;

        lval.store(args[0]->type->getDefaultValue());
        return nullptr;
    }
};

class AssocArrayDeleteMethod : public SystemSubroutine {
public:
    AssocArrayDeleteMethod() : SystemSubroutine("delete", SubroutineKind::Function) {}

    const Expression& bindArgument(size_t argIndex, const BindContext& context,
                                   const ExpressionSyntax& syntax, const Args& args) const final {
        // Argument type comes from the index type of the previous argument.
        if (argIndex == 1) {
            auto indexType = args[0]->type->getAssociativeIndexType();
            if (indexType)
                return Expression::bindArgument(*indexType, ArgumentDirection::In, syntax, context);
        }

        return SystemSubroutine::bindArgument(argIndex, context, syntax, args);
    }

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression*) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, true, args, range, 0, 1))
            return comp.getErrorType();

        if (args.size() > 1) {
            auto& type = *args[0]->type;
            auto indexType = type.getAssociativeIndexType();
            if (!indexType && !args[1]->type->isIntegral())
                return badArg(context, *args[1]);
        }

        return comp.getVoidType();
    }

    ConstantValue eval(EvalContext& context, const Args& args,
                       const CallExpression::SystemCallInfo&) const final {
        auto lval = args[0]->evalLValue(context);
        if (!lval)
            return nullptr;

        if (args.size() > 1) {
            auto index = args[1]->eval(context);
            if (!index)
                return nullptr;

            auto target = lval.resolve();
            if (target && target->isMap()) {
                // Try to erase the element -- no warning if it doesn't exist.
                target->map()->erase(index);
            }
        }
        else {
            // No argument means we should empty the array.
            lval.store(args[0]->type->getDefaultValue());
        }
        return nullptr;
    }

    bool verifyConstant(EvalContext&, const Args&, SourceRange) const final { return true; }
};

class AssocArrayExistsMethod : public SystemSubroutine {
public:
    AssocArrayExistsMethod() : SystemSubroutine("exists", SubroutineKind::Function) {}

    const Expression& bindArgument(size_t argIndex, const BindContext& context,
                                   const ExpressionSyntax& syntax, const Args& args) const final {
        // Argument type comes from the index type of the previous argument.
        if (argIndex == 1) {
            auto indexType = args[0]->type->getAssociativeIndexType();
            if (indexType)
                return Expression::bindArgument(*indexType, ArgumentDirection::In, syntax, context);
        }

        return SystemSubroutine::bindArgument(argIndex, context, syntax, args);
    }

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression*) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, true, args, range, 1, 1))
            return comp.getErrorType();

        auto& type = *args[0]->type;
        auto indexType = type.getAssociativeIndexType();
        if (!indexType && !args[1]->type->isIntegral())
            return badArg(context, *args[1]);

        return comp.getIntType();
    }

    ConstantValue eval(EvalContext& context, const Args& args,
                       const CallExpression::SystemCallInfo&) const final {
        auto array = args[0]->eval(context);
        auto index = args[1]->eval(context);
        if (!array || !index)
            return nullptr;

        bool exists = array.map()->count(index);
        return SVInt(32, exists ? 1 : 0, true);
    }

    bool verifyConstant(EvalContext&, const Args&, SourceRange) const final { return true; }
};

class AssocArrayTraversalMethod : public SystemSubroutine {
public:
    explicit AssocArrayTraversalMethod(const std::string& name) :
        SystemSubroutine(name, SubroutineKind::Function) {}

    const Expression& bindArgument(size_t argIndex, const BindContext& context,
                                   const ExpressionSyntax& syntax, const Args& args) const final {
        // Argument type comes from the index type of the previous argument.
        if (argIndex == 1) {
            auto indexType = args[0]->type->getAssociativeIndexType();
            if (indexType)
                return Expression::bindArgument(*indexType, ArgumentDirection::Ref, syntax,
                                                context);
        }

        return SystemSubroutine::bindArgument(argIndex, context, syntax, args);
    }

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression*) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, true, args, range, 1, 1))
            return comp.getErrorType();

        auto& type = *args[0]->type;
        auto indexType = type.getAssociativeIndexType();
        if (!indexType) {
            context.addDiag(diag::AssociativeWildcardNotAllowed, range) << name;
            return context.getCompilation().getErrorType();
        }

        return comp.getIntType();
    }

    ConstantValue eval(EvalContext&, const Args&,
                       const CallExpression::SystemCallInfo&) const final {
        return nullptr;
    }
    bool verifyConstant(EvalContext& context, const Args&, SourceRange range) const final {
        return notConst(context, range);
    }
};

class QueuePopMethod : public SystemSubroutine {
public:
    QueuePopMethod(const std::string& name, bool front) :
        SystemSubroutine(name, SubroutineKind::Function), front(front) {}

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression*) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, true, args, range, 0, 0))
            return comp.getErrorType();

        return *args[0]->type->getArrayElementType();
    }

    ConstantValue eval(EvalContext& context, const Args& args,
                       const CallExpression::SystemCallInfo&) const final {
        auto lval = args[0]->evalLValue(context);
        if (!lval)
            return nullptr;

        auto target = lval.resolve();
        ASSERT(target && target->isQueue());

        auto& q = *target->queue();
        if (q.empty()) {
            context.addDiag(diag::ConstEvalEmptyQueue, args[0]->sourceRange);
            return args[0]->type->getArrayElementType()->getDefaultValue();
        }

        ConstantValue result = front ? std::move(q.front()) : std::move(q.back());
        if (front)
            q.pop_front();
        else
            q.pop_back();

        return result;
    }

    bool verifyConstant(EvalContext&, const Args&, SourceRange) const final { return true; }

private:
    bool front;
};

class QueuePushMethod : public SystemSubroutine {
public:
    QueuePushMethod(const std::string& name, bool front) :
        SystemSubroutine(name, SubroutineKind::Function), front(front) {}

    const Expression& bindArgument(size_t argIndex, const BindContext& context,
                                   const ExpressionSyntax& syntax, const Args& args) const final {
        // Argument type comes from the element type of the queue.
        if (argIndex == 1) {
            auto elemType = args[0]->type->getArrayElementType();
            if (elemType)
                return Expression::bindArgument(*elemType, ArgumentDirection::In, syntax, context);
        }

        return SystemSubroutine::bindArgument(argIndex, context, syntax, args);
    }

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression*) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, true, args, range, 1, 1))
            return comp.getErrorType();

        return comp.getVoidType();
    }

    ConstantValue eval(EvalContext& context, const Args& args,
                       const CallExpression::SystemCallInfo&) const final {
        auto lval = args[0]->evalLValue(context);
        auto cv = args[1]->eval(context);
        if (!lval || !cv)
            return nullptr;

        auto target = lval.resolve();
        ASSERT(target && target->isQueue());

        auto& q = *target->queue();
        if (front)
            q.push_front(std::move(cv));
        else
            q.push_back(std::move(cv));

        return nullptr;
    }

    bool verifyConstant(EvalContext&, const Args&, SourceRange) const final { return true; }

private:
    bool front;
};

class QueueInsertMethod : public SystemSubroutine {
public:
    QueueInsertMethod() : SystemSubroutine("insert", SubroutineKind::Function) {}

    const Expression& bindArgument(size_t argIndex, const BindContext& context,
                                   const ExpressionSyntax& syntax, const Args& args) const final {
        // Argument type comes from the element type of the queue.
        if (argIndex == 2) {
            auto elemType = args[0]->type->getArrayElementType();
            if (elemType)
                return Expression::bindArgument(*elemType, ArgumentDirection::In, syntax, context);
        }

        return SystemSubroutine::bindArgument(argIndex, context, syntax, args);
    }

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression*) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, true, args, range, 2, 2))
            return comp.getErrorType();

        if (!args[1]->type->isIntegral())
            return badArg(context, *args[1]);

        return comp.getVoidType();
    }

    ConstantValue eval(EvalContext& context, const Args& args,
                       const CallExpression::SystemCallInfo&) const final {
        auto lval = args[0]->evalLValue(context);
        auto ci = args[1]->eval(context);
        auto cv = args[2]->eval(context);
        if (!lval || !ci || !cv)
            return nullptr;

        auto target = lval.resolve();
        ASSERT(target && target->isQueue());

        auto& q = *target->queue();
        optional<int32_t> index = ci.integer().as<int32_t>();
        if (!index || *index < 0 || size_t(*index) >= q.size() + 1) {
            context.addDiag(diag::ConstEvalDynamicArrayIndex, args[1]->sourceRange)
                << ci << *args[0]->type << q.size() + 1;
            return nullptr;
        }

        q.insert(q.begin() + *index, std::move(cv));
        return nullptr;
    }

    bool verifyConstant(EvalContext&, const Args&, SourceRange) const final { return true; }
};

class QueueDeleteMethod : public SystemSubroutine {
public:
    QueueDeleteMethod() : SystemSubroutine("delete", SubroutineKind::Function) {}

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression*) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, true, args, range, 0, 1))
            return comp.getErrorType();

        if (args.size() > 1) {
            if (!args[1]->type->isIntegral())
                return badArg(context, *args[1]);
        }

        return comp.getVoidType();
    }

    ConstantValue eval(EvalContext& context, const Args& args,
                       const CallExpression::SystemCallInfo&) const final {
        auto lval = args[0]->evalLValue(context);
        if (!lval)
            return nullptr;

        auto target = lval.resolve();
        ASSERT(target && target->isQueue());
        auto& q = *target->queue();

        // If no arguments, clear the queue.
        if (args.size() == 1) {
            q.clear();
            return nullptr;
        }

        auto ci = args[1]->eval(context);
        optional<int32_t> index = ci.integer().as<int32_t>();
        if (!index || *index < 0 || size_t(*index) >= q.size()) {
            context.addDiag(diag::ConstEvalDynamicArrayIndex, args[1]->sourceRange)
                << ci << *args[0]->type << q.size();
            return nullptr;
        }

        q.erase(q.begin() + *index);
        return nullptr;
    }

    bool verifyConstant(EvalContext&, const Args&, SourceRange) const final { return true; }
};

class IteratorIndexMethod : public SystemSubroutine {
public:
    IteratorIndexMethod() : SystemSubroutine("index", SubroutineKind::Function) {}

    const Type& checkArguments(const BindContext& context, const Args& args, SourceRange range,
                               const Expression*) const final {
        auto& comp = context.getCompilation();
        if (!checkArgCount(context, true, args, range, 0, 1))
            return comp.getErrorType();

        if (args.size() > 1 && !args[1]->type->isIntegral())
            return badArg(context, *args[1]);

        auto& iterator = args[0]->as<NamedValueExpression>().symbol.as<IteratorSymbol>();
        if (iterator.arrayType.isAssociativeArray()) {
            auto indexType = iterator.arrayType.getAssociativeIndexType();
            if (!indexType) {
                context.addDiag(diag::AssociativeWildcardNotAllowed, range) << name;
                return context.getCompilation().getErrorType();
            }
            return *indexType;
        }

        return comp.getIntType();
    }

    ConstantValue eval(EvalContext&, const Args&,
                       const CallExpression::SystemCallInfo&) const final {
        return nullptr;
    }
    bool verifyConstant(EvalContext& context, const Args&, SourceRange range) const final {
        return notConst(context, range);
    }
};

void registerArrayMethods(Compilation& c) {
#define REGISTER(kind, name, ...) \
    c.addSystemMethod(kind, std::make_unique<name##Method>(__VA_ARGS__))

    for (auto kind : { SymbolKind::FixedSizeUnpackedArrayType, SymbolKind::DynamicArrayType,
                       SymbolKind::AssociativeArrayType, SymbolKind::QueueType }) {
        REGISTER(kind, ArrayReduction, "or", [](auto& l, auto& r) { l |= r; });
        REGISTER(kind, ArrayReduction, "and", [](auto& l, auto& r) { l &= r; });
        REGISTER(kind, ArrayReduction, "xor", [](auto& l, auto& r) { l ^= r; });
        REGISTER(kind, ArrayReduction, "sum", [](auto& l, auto& r) { l += r; });
        REGISTER(kind, ArrayReduction, "product", [](auto& l, auto& r) { l *= r; });
    }

    for (auto kind : { SymbolKind::DynamicArrayType, SymbolKind::AssociativeArrayType,
                       SymbolKind::QueueType }) {
        REGISTER(kind, ArraySize, c, "size");
    }

    for (auto kind : { SymbolKind::FixedSizeUnpackedArrayType, SymbolKind::DynamicArrayType,
                       SymbolKind::QueueType }) {
        REGISTER(kind, ArraySort, "sort", false);
        REGISTER(kind, ArraySort, "rsort", true);
    }

    // Associative arrays also alias "size" to "num" for some reason.
    REGISTER(SymbolKind::AssociativeArrayType, ArraySize, c, "num");

    // "delete" methods
    REGISTER(SymbolKind::DynamicArrayType, DynArrayDelete, c);
    REGISTER(SymbolKind::AssociativeArrayType, AssocArrayDelete, );
    REGISTER(SymbolKind::QueueType, QueueDelete, );

    // Associative array methods.
    REGISTER(SymbolKind::AssociativeArrayType, AssocArrayExists, );
    REGISTER(SymbolKind::AssociativeArrayType, AssocArrayTraversal, "first");
    REGISTER(SymbolKind::AssociativeArrayType, AssocArrayTraversal, "last");
    REGISTER(SymbolKind::AssociativeArrayType, AssocArrayTraversal, "next");
    REGISTER(SymbolKind::AssociativeArrayType, AssocArrayTraversal, "prev");

    // Queue methods
    REGISTER(SymbolKind::QueueType, QueuePop, "pop_front", true);
    REGISTER(SymbolKind::QueueType, QueuePop, "pop_back", false);
    REGISTER(SymbolKind::QueueType, QueuePush, "push_front", true);
    REGISTER(SymbolKind::QueueType, QueuePush, "push_back", false);
    REGISTER(SymbolKind::QueueType, QueueInsert, );

    // Iterator methods
    REGISTER(SymbolKind::Iterator, IteratorIndex, );
}

} // namespace slang::Builtins
