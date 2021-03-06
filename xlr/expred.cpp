// *****************************************************************************
// expred.cpp                                                         XL project
// *****************************************************************************
//
// File description:
//
//    Information required by the compiler for expression reduction
//
//
//
//
//
//
//
//
// *****************************************************************************
// This software is licensed under the GNU General Public License v3
// (C) 2010-2011,2014,2017,2019, Christophe de Dinechin <christophe@dinechin.org>
// (C) 2012, Jérôme Forissier <jerome@taodyne.com>
// *****************************************************************************
// This file is part of XL
//
// XL is free software: you can r redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XL is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with XL, in a file named COPYING.
// If not, see <https://www.gnu.org/licenses/>.
// *****************************************************************************

#include "expred.h"
#include "unit.h"
#include "args.h"
#include "types.h"
#include "save.h"
#include "errors.h"
#include "renderer.h"
#include "llvm-crap.h"

XL_BEGIN

using namespace llvm;



// ============================================================================
//
//    Compile an expression
//
// ============================================================================

CompileExpression::CompileExpression(CompiledUnit *unit)
// ----------------------------------------------------------------------------
//   Constructor for a compiler expression
// ----------------------------------------------------------------------------
    : unit(unit), llvm(unit->compiler->llvm)
{}


llvm_value CompileExpression::DoInteger(Integer *what)
// ----------------------------------------------------------------------------
//   Compile an integer constant
// ----------------------------------------------------------------------------
{
    Compiler *compiler = unit->compiler;
    return ConstantInt::get(compiler->integerTy, what->value);
}


llvm_value CompileExpression::DoReal(Real *what)
// ----------------------------------------------------------------------------
//   Compile a real constant
// ----------------------------------------------------------------------------
{
    Compiler *compiler = unit->compiler;
    return ConstantFP::get(compiler->realTy, what->value);
}


llvm_value CompileExpression::DoText(Text *what)
// ----------------------------------------------------------------------------
//   Compile a text constant
// ----------------------------------------------------------------------------
{
    Compiler *compiler = unit->compiler;
    if (what->IsCharacter())
        return ConstantInt::get(compiler->characterTy,
                                what->value.length() ? what->value[0] : 0);
    llvm_value textConstant = compiler->TextConstant(unit->data, what->value);
    return textConstant;
}


llvm_value CompileExpression::DoName(Name *what)
// ----------------------------------------------------------------------------
//   Compile a name
// ----------------------------------------------------------------------------
{
    Context_p  where;
    Rewrite_p  rewrite;
    Context   *context  = unit->context;
    Tree      *existing = context->Bound(what, Context::SCOPE_LOOKUP,
                                         &where, &rewrite);
    assert(existing || !"Type checking didn't realize a name is missing");
    if (where == context)
        if (llvm_value result = unit->Known(rewrite->from))
            return result;

    // Check true and false values
    if (existing == xl_true)
        return ConstantInt::get(unit->compiler->booleanTy, 1);
    if (existing == xl_false)
        return ConstantInt::get(unit->compiler->booleanTy, 0);

    // Check if it is a global
    if (llvm_value global = unit->Global(existing))
        return global;

    // If we are in a context building a closure, record dependency
    if (unit->closureTy)
        return unit->NeedClosure(rewrite->from);

    return DoCall(what);
}


llvm_value CompileExpression::DoInfix(Infix *infix)
// ----------------------------------------------------------------------------
//   Compile infix expressions
// ----------------------------------------------------------------------------
{
    // Sequences
    if (infix->name == "\n" || infix->name == ";")
    {
        llvm_value left = ForceEvaluation(infix->left);
        llvm_value right = ForceEvaluation(infix->right);
        if (right)
            return right;
        if (left)
            return left;
        return NULL;
    }

    // Type casts - REVISIT: may need to do some actual conversion
    if (infix->name == ":" || infix->name == "as")
    {
        return infix->left->Do(this);
    }

    // Declarations: it's too early to define a function just yet,
    // because we don't have the actual argument types.
    if (infix->name == "->")
        return NULL;

    // General case: expression
    return DoCall(infix);
}


llvm_value CompileExpression::DoPrefix(Prefix *what)
// ----------------------------------------------------------------------------
//   Compile prefix expressions
// ----------------------------------------------------------------------------
{
    if (Name *name = what->left->AsName())
    {
        if (name->value == "data" || name->value == "extern")
            return NULL;

        if (name->value == "opcode")
        {
            // This is a builtin, find if we write to code or data
            llvm_builder bld = unit->code;
            Tree *builtin = what->right;
            if (Prefix *prefix = builtin->AsPrefix())
            {
                if (Name *name = prefix->left->AsName())
                {
                    if (name->value == "data")
                    {
                        bld = unit->data;
                        builtin = prefix->right;
                    }
                }
            }

            // Take args list for current function as input
            llvm_values args;
            Function *function = unit->function;
            uint i, max = function->arg_size();
            Function::arg_iterator arg = function->arg_begin();
            for (i = 0; i < max; i++)
            {
                llvm_value inputArg = &*arg++;
                args.push_back(inputArg);
            }

            // Call the primitive (effectively creating a wrapper for it)
            Name *name = builtin->AsName();
            assert(name || !"Malformed primitive");
            Compiler *compiler = unit->compiler;
            text op = name->value;
            uint sz = args.size();
            llvm_value *a = &args[0];
            return compiler->Primitive(*unit, bld, op, sz, a);
        }
    }
    return DoCall(what);
}


llvm_value CompileExpression::DoPostfix(Postfix *what)
// ----------------------------------------------------------------------------
//   Compile postfix expressions
// ----------------------------------------------------------------------------
{
    return DoCall(what);
}


llvm_value CompileExpression::DoBlock(Block *block)
// ----------------------------------------------------------------------------
//   Compile blocks
// ----------------------------------------------------------------------------
{
    return block->child->Do(this);
}


llvm_value CompileExpression::DoCall(Tree *call)
// ----------------------------------------------------------------------------
//   Compile expressions into calls for the right expression
// ----------------------------------------------------------------------------
{
    llvm_value result = NULL;

    rcall_map &rcalls = unit->inference->rcalls;
    rcall_map::iterator found = rcalls.find(call);
    assert(found != rcalls.end() || !"Type analysis botched on expression");

    Function *function = unit->function;
    LLVMContext &llvm = unit->llvm;
    RewriteCalls *rc = (*found).second;
    RewriteCandidates &calls = rc->candidates;

    // Optimize the frequent case where we have a single call candidate
    uint i, max = calls.size();
    if (max == 1)
    {
        // We now evaluate in that rewrite's type system
        RewriteCandidate &cand = calls[0];
        Save<TypeInference_p> saveTypes(unit->inference, cand.types);

        if (cand.conditions.size() == 0)
        {
            result = DoRewrite(cand);
            return result;
        }
    }

    // More general case: we need to generate expression reduction
    llvm_block isDone = BasicBlock::Create(llvm, "done", function);
    llvm_builder code = unit->code;
    llvm_value storage = unit->NeedStorage(call);
    llvm_type storageType = unit->ExpressionMachineType(call);

    for (i = 0; i < max; i++)
    {
        // Now evaluate in that candidate's type system
        RewriteCandidate &cand = calls[i];
        Save<TypeInference_p> saveTypes(unit->inference, cand.types);
        llvm_value condition = NULL;

        // Perform the tests to check if this candidate is valid
        std::vector<RewriteCondition> &conds = cand.conditions;
        std::vector<RewriteCondition>::iterator t;
        for (t = conds.begin(); t != conds.end(); t++)
        {
            llvm_value compare = Compare((*t).value, (*t).test);
            if (condition)
                condition = code->CreateAnd(condition, compare);
            else
                condition = compare;
        }

        if (condition)
        {
            llvm_block isBad = BasicBlock::Create(llvm, "bad", function);
            llvm_block isGood = BasicBlock::Create(llvm, "good", function);
            code->CreateCondBr(condition, isGood, isBad);
            code->SetInsertPoint(isGood);
            value_map saveComputed = computed;
            result = DoRewrite(cand);
            computed = saveComputed;
            result = unit->Autobox(result, storageType);
            code->CreateStore(result, storage);
            code->CreateBr(isDone);
            code->SetInsertPoint(isBad);
        }
        else
        {
            // If this particular call was unconditional, we are done
            result = DoRewrite(cand);
            result = unit->Autobox(result, storageType);
            code->CreateStore(result, storage);
            code->CreateBr(isDone);
            code->SetInsertPoint(isDone);
            result = code->CreateLoad(storage);
            return result;
        }
    }

    // The final call to xl_form_error if nothing worked
    unit->CallFormError(call);
    code->CreateBr(isDone);
    code->SetInsertPoint(isDone);
    result = code->CreateLoad(storage);
    return result;
}


llvm_value CompileExpression::DoRewrite(RewriteCandidate &cand)
// ----------------------------------------------------------------------------
//   Generate code for a particular rewwrite candidate
// ----------------------------------------------------------------------------
{
    Rewrite *rw = cand.rewrite;
    llvm_value result = NULL;

    // Evaluate parameters
    llvm_values args;
    RewriteBindings &bnds = cand.bindings;
    RewriteBindings::iterator b;
    for (b = bnds.begin(); b != bnds.end(); b++)
    {
        Tree *tree = (*b).value;
        if (llvm_value closure = (*b).Closure(unit))
        {
            args.push_back(closure);
        }
        else if (llvm_value value = Value(tree))
        {
            args.push_back(value);
            llvm_type mtype = value->getType();
            if (unit->compiler->IsClosureType(mtype))
                (*b).closure = value;
        }
    }

    // Check if this is an LLVM builtin
    Tree *builtin = NULL;
    if (rw->to)
        if (Prefix *prefix = rw->to->AsPrefix())
            if (Name *name = prefix->left->AsName())
                if (name->value == "opcode")
                    builtin = prefix->right;

    if (builtin)
    {
        llvm_builder bld = unit->code;
        if (Prefix *prefix = builtin->AsPrefix())
        {
            if (Name *name = prefix->left->AsName())
            {
                if (name->value == "data")
                {
                    bld = unit->data;
                    builtin = prefix->right;
                }
            }
        }

        Name *name = builtin->AsName();
        assert(name || !"Malformed primitive");
        Compiler *compiler = unit->compiler;
        text op = name->value;
        uint sz = args.size();
        llvm_value *a = &args[0];
        result = compiler->Primitive(*unit, bld, op, sz, a);
    }
    else
    {
        llvm_value function = unit->Compile(cand, args);
        if (function)
            result = llvm.CreateCall(unit->code, function, args);
    }

    return result;
}


llvm_value CompileExpression::Value(Tree *expr)
// ----------------------------------------------------------------------------
//   Evaluate an expression once
// ----------------------------------------------------------------------------
{
    llvm_value value = computed[expr];
    if (!value)
    {
        value = expr->Do(this);
        computed[expr] = value;
    }
    return value;
}


llvm_value CompileExpression::Compare(Tree *valueTree, Tree *testTree)
// ----------------------------------------------------------------------------
//   Perform a comparison between the two values and check if this matches
// ----------------------------------------------------------------------------
{
    llvm_value value = Value(valueTree);
    llvm_value test = Value(testTree);
    llvm_type valueType = value->getType();
    llvm_type testType = test->getType();

    CompiledUnit &u = *unit;
    Compiler &c = *u.compiler;
    llvm_builder code = u.code;

    // Comparison of boolean values
    if (testType == c.booleanTy)
    {
        if (valueType == c.treePtrTy || valueType == c.nameTreePtrTy)
        {
            value = u.Autobox(value, c.booleanTy);
            valueType = value->getType();
        }
        if (valueType != c.booleanTy)
            return ConstantInt::get(c.booleanTy, 0);
        return code->CreateICmpEQ(test, value);
    }

    // Comparison of character values
    if (testType == c.characterTy)
    {
        if (valueType == c.textTreePtrTy)
        {
            value = u.Autobox(value, testType);
            valueType = value->getType();
        }
        if (valueType != c.characterTy)
            return ConstantInt::get(c.booleanTy, 0);
        return code->CreateICmpEQ(test, value);
    }

    // Comparison of text constants
    if (testType == c.textTy)
    {
        test = u.Autobox(test, c.charPtrTy);
        testType = test->getType();
    }
    if (testType == c.charPtrTy)
    {
        if (valueType == c.textTreePtrTy)
        {
            value = u.Autobox(value, testType);
            valueType = value->getType();
        }
        if (valueType != c.charPtrTy)
            return ConstantInt::get(c.booleanTy, 0);
        value = llvm.CreateCall(code, c.strcmp_fn, test, value);
        test = ConstantInt::get(value->getType(), 0);
        value = code->CreateICmpEQ(value, test);
        return value;
    }

    // Comparison of integer values
    if (testType->isIntegerTy())
    {
        if (valueType == c.integerTreePtrTy)
        {
            value = u.Autobox(value, c.integerTy);
            valueType = value->getType();
        }
        if (!valueType->isIntegerTy())
            return ConstantInt::get(c.booleanTy, 0);
        if (valueType != c.integerTy)
            value = code->CreateSExt(value, c.integerTy);
        if (testType != c.integerTy)
            test = code->CreateSExt(test, c.integerTy);
        return code->CreateICmpEQ(test, value);
    }

    // Comparison of floating-point values
    if (testType->isFloatingPointTy())
    {
        if (valueType == c.realTreePtrTy)
        {
            value = u.Autobox(value, c.realTy);
            valueType = value->getType();
        }
        if (!valueType->isFloatingPointTy())
            return ConstantInt::get(c.booleanTy, 0);
        if (valueType != testType)
        {
            if (valueType != c.realTy)
            {
                value = code->CreateFPExt(value, c.realTy);
                valueType = value->getType();
            }
            if (testType != c.realTy)
            {
                test = code->CreateFPExt(test, c.realTy);
                testType = test->getType();
            }
            if (valueType != testType)
                return ConstantInt::get(c.booleanTy, 0);
        }
        return code->CreateFCmpOEQ(test, value);
    }

    // Test our special types
    if (testType == c.treePtrTy         ||
        testType == c.integerTreePtrTy  ||
        testType == c.realTreePtrTy     ||
        testType == c.textTreePtrTy     ||
        testType == c.nameTreePtrTy     ||
        testType == c.blockTreePtrTy    ||
        testType == c.infixTreePtrTy    ||
        testType == c.prefixTreePtrTy   ||
        testType == c.postfixTreePtrTy)
    {
        if (testType != c.treePtrTy)
        {
            test = code->CreateBitCast(test, c.treePtrTy);
            testType = test->getType();
        }

        // Convert value to a Tree * if possible
        if (valueType->isIntegerTy() ||
            valueType->isFloatingPointTy() ||
            valueType == c.charPtrTy ||
            valueType == c.textTy ||
            valueType == c.integerTreePtrTy  ||
            valueType == c.realTreePtrTy     ||
            valueType == c.textTreePtrTy     ||
            valueType == c.nameTreePtrTy     ||
            valueType == c.blockTreePtrTy    ||
            valueType == c.infixTreePtrTy    ||
            valueType == c.prefixTreePtrTy   ||
            valueType == c.postfixTreePtrTy)
        {
            value = u.Autobox(value, c.treePtrTy);
            valueType = value->getType();
        }

        if (testType != valueType)
            return ConstantInt::get(c.booleanTy, 0);

        // Call runtime function to perform tree comparison
        return llvm.CreateCall(code, c.xl_same_shape, value, test);
    }

    // Other comparisons fail for now
    return ConstantInt::get(c.booleanTy, 0);
}


llvm_value CompileExpression::ForceEvaluation(Tree *expr)
// ----------------------------------------------------------------------------
//   For top-level expressions, make sure we evaluate closures
// ----------------------------------------------------------------------------
{
    llvm_value result = expr->Do(this);
    if (result)
    {
        llvm_type resTy = result->getType();
        if (unit->compiler->IsClosureType(resTy))
            result = unit->InvokeClosure(result);
    }
    return result;
}


llvm_value CompileExpression::TopLevelEvaluation(Tree *expr)
// ----------------------------------------------------------------------------
//   Evaluate normally, but force evaluation for names
// ----------------------------------------------------------------------------
{
    if (expr->Kind() == NAME)
        return ForceEvaluation(expr);
    return expr->Do(this);
}

XL_END
