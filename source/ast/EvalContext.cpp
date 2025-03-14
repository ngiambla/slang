//------------------------------------------------------------------------------
// EvalContext.cpp
// Expression evaluation context
//
// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#include "slang/ast/EvalContext.h"

#include "../text/FormatBuffer.h"

#include "slang/ast/ASTContext.h"
#include "slang/ast/Compilation.h"
#include "slang/ast/symbols/SubroutineSymbols.h"
#include "slang/ast/symbols/VariableSymbols.h"
#include "slang/ast/types/Type.h"
#include "slang/diagnostics/ConstEvalDiags.h"

namespace slang::ast {

void EvalContext::reset() {
    steps = 0;
    disableTarget = nullptr;
    queueTarget = nullptr;
    stack.clear();
    lvalStack.clear();
    diags.clear();
    disableRange = {};
}

ConstantValue* EvalContext::createLocal(const ValueSymbol* symbol, ConstantValue value) {
    ASSERT(!stack.empty());
    ConstantValue& result = stack.back().temporaries[symbol];
    if (!value) {
        result = symbol->getType().getDefaultValue();
    }
    else {
        ASSERT(!value.isInteger() ||
               value.integer().getBitWidth() == symbol->getType().getBitWidth());

        result = std::move(value);
    }

    return &result;
}

ConstantValue* EvalContext::findLocal(const ValueSymbol* symbol) {
    if (stack.empty())
        return nullptr;

    auto& frame = stack.back();
    auto it = frame.temporaries.find(symbol);
    if (it == frame.temporaries.end())
        return nullptr;
    return &it->second;
}

void EvalContext::deleteLocal(const ValueSymbol* symbol) {
    if (!stack.empty()) {
        auto& frame = stack.back();
        frame.temporaries.erase(symbol);
    }
}

bool EvalContext::pushFrame(const SubroutineSymbol& subroutine, SourceLocation callLocation,
                            LookupLocation lookupLocation) {
    const uint32_t maxDepth = compilation.getOptions().maxConstexprDepth;
    if (stack.size() >= maxDepth) {
        addDiag(diag::ConstEvalExceededMaxCallDepth, subroutine.location) << maxDepth;
        return false;
    }

    Frame frame;
    frame.subroutine = &subroutine;
    frame.callLocation = callLocation;
    frame.lookupLocation = lookupLocation;
    stack.emplace_back(std::move(frame));
    return true;
}

void EvalContext::pushEmptyFrame() {
    stack.emplace_back(Frame{});
}

void EvalContext::popFrame() {
    stack.pop_back();
}

void EvalContext::pushLValue(LValue& lval) {
    lvalStack.push_back(&lval);
}

void EvalContext::popLValue() {
    lvalStack.pop_back();
}

LValue* EvalContext::getTopLValue() const {
    if (lvalStack.empty())
        return nullptr;

    return lvalStack.back();
}

bool EvalContext::step(SourceLocation loc) {
    if (++steps < compilation.getOptions().maxConstexprSteps)
        return true;

    addDiag(diag::ConstEvalExceededMaxSteps, loc);
    return false;
}

std::string EvalContext::dumpStack() const {
    FormatBuffer buffer;
    int index = 0;
    for (const Frame& frame : stack) {
        buffer.format("{}: {}\n", index++, frame.subroutine ? frame.subroutine->name : "<global>");
        for (auto& [symbol, value] : frame.temporaries)
            buffer.format("    {} = {}\n", symbol->name, value.toString());
    }
    return buffer.str();
}

Diagnostic& EvalContext::addDiag(DiagCode code, SourceLocation location) {
    auto& diag = diags.add(code, location);
    reportStack(diag);
    return diag;
}

Diagnostic& EvalContext::addDiag(DiagCode code, SourceRange range) {
    auto& diag = diags.add(code, range);
    reportStack(diag);
    return diag;
}

void EvalContext::addDiags(const Diagnostics& additional) {
    bool first = true;
    for (auto& diag : additional) {
        if (first) {
            Diagnostic copy = diag;
            reportStack(copy);
            diags.emplace_back(std::move(copy));
            first = false;
        }
        else {
            diags.push_back(diag);
        }
    }
}

void EvalContext::reportDiags(const ASTContext& context) {
    if (diags.empty())
        return;

    if (context.assertionInstance)
        context.addAssertionBacktrace(diags[0]);

    context.scope->addDiags(diags);
}

static void reportFrame(Diagnostic& diag, const EvalContext::Frame& frame) {
    if (!frame.subroutine)
        return;

    FormatBuffer buffer;
    buffer.format("{}(", frame.subroutine->name);

    for (auto arg : frame.subroutine->getArguments()) {
        auto it = frame.temporaries.find(arg);
        ASSERT(it != frame.temporaries.end());

        buffer.append(it->second.toString());
        if (arg != frame.subroutine->getArguments().last(1)[0])
            buffer.append(", ");
    }

    buffer.append(")");

    diag.addNote(diag::NoteInCallTo, frame.callLocation) << buffer.str();
}

void EvalContext::reportStack(Diagnostic& diag) const {
    const size_t limit = compilation.getOptions().maxConstexprBacktrace;
    if (stack.size() <= limit || limit == 0) {
        FormatBuffer buffer;
        for (const Frame& frame : make_reverse_range(stack))
            reportFrame(diag, frame);
        return;
    }

    const ptrdiff_t start = ptrdiff_t(limit / 2);
    const ptrdiff_t end = start + ptrdiff_t(limit % 2);
    auto reversed = make_reverse_range(stack);
    for (auto it = reversed.begin(), itEnd = it + start; it != itEnd; it++)
        reportFrame(diag, *it);

    diag.addNote(diag::NoteSkippingFrames, (reversed.begin() + start)->callLocation)
        << stack.size() - limit;

    for (auto it = reversed.end() - end, itEnd = reversed.end(); it != itEnd; it++)
        reportFrame(diag, *it);
}

} // namespace slang::ast
