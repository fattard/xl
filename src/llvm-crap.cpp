// ****************************************************************************
//  llvm-crap.cpp                                   XL - An extensible language
// ****************************************************************************
//
//   File Description:
//
//     LLVM Compatibility Recovery Adaptive Protocol
//
//     LLVM keeps breaking the API from release to release.
//     Of course, none of the choices are documented anywhere, and
//     the documentation is left out of date for years.
//
//     So this file is an attempt at reverse engineering all the API
//     changes over time to be able to compile with various versions of LLVM
//     Be prepared for the worst. It's so ugly I had to dispose of it by
//     putting it in its own separate file.
//
//
// ****************************************************************************
//  (C) 2017 Christophe de Dinechin <christophe@dinechin.org>
//   This software is licensed under the GNU General Public License v3
//   See LICENSE file for details.
// ****************************************************************************

#include "llvm-crap.h"
#include "renderer.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/IRTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/IndirectionUtils.h"
#include "llvm/ExecutionEngine/Orc/LambdaResolver.h"
#include "llvm/ExecutionEngine/Orc/OrcRemoteTargetClient.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>



// ============================================================================
//
//   Recorders related to LLVM
//
// ============================================================================

RECORDER(llvm, 64, "LLVM general operations");
RECORDER(llvm_prototypes, 64, "LLVM function prototypes");
RECORDER(llvm_externals, 64, "LLVM external functions");
RECORDER(llvm_functions, 64, "LLVM functions");
RECORDER(llvm_constants, 64, "LLVM constant values");
RECORDER(llvm_builtins, 64, "LLVM builtin functions");
RECORDER(llvm_globals, 64, "LLVM global variables");
RECORDER(llvm_blocks, 64, "LLVM basic blocks");
RECORDER(llvm_labels, 64, "LLVM labels for trees");
RECORDER(llvm_calls, 64, "LLVM calls");
RECORDER(llvm_stats, 64, "LLVM statistics");
RECORDER(llvm_code, 64, "LLVM code generation");
RECORDER(llvm_gc, 64, "LLVM garbage collection");
RECORDER(llvm_ir, 64, "LLVM intermediate representation");



namespace XL
{

// ============================================================================
//
//    Implementation of an ORC-based LLVM JIT
//
// ============================================================================

// The Kaleidoscope JIT code does not bother with namespaces...
using namespace llvm;
using namespace llvm::orc;
using namespace llvm::legacy;


typedef RTDyldObjectLinkingLayer                     LinkingLayer;
typedef IRCompileLayer<LinkingLayer, SimpleCompiler> CompileLayer;
typedef std::shared_ptr<SymbolResolver>              SymbolResolver_s;
typedef std::unique_ptr<TargetMachine>               TargetMachine_u;
typedef std::shared_ptr<Module>                      Module_s;
typedef std::shared_ptr<Function>                    Function_s;
typedef std::function<Module_s(Module_s)>            Optimizer;
typedef IRTransformLayer<CompileLayer, Optimizer>    OptimizeLayer;
typedef std::unique_ptr<JITCompileCallbackManager>   CompileCallbacks_u;
typedef std::unique_ptr<IndirectStubsManager>        IndirectStubs_u;


class JITPrivate
// ----------------------------------------------------------------------------
//   JIT private data (from Kaleidoscope)
// ----------------------------------------------------------------------------
{
    friend class        JIT;
    friend class        JITBlock;
    friend class        JITBlockPrivate;

    unsigned            optLevel;
    LLVMContext         context;
    SymbolStringPool    strings;
    ExecutionSession    session;
    SymbolResolver_s    resolver;
    TargetMachine_u     target;
    const DataLayout    layout;
    LinkingLayer        linker;
    CompileLayer        compiler;
    OptimizeLayer       optimizer;
    CompileCallbacks_u  callbacks;
    IndirectStubs_u     stubs;

    Module_s            module;
    VModuleKey          key;
    Function_p          function;

public:
    JITPrivate();
    ~JITPrivate();

private:
    Module_p            Module();
    Module_p            Module(text name, bool renew=false);
    VModuleKey          ModuleKey();
    void                DeleteModule(VModuleKey key);
    Module_s            OptimizeModule(Module_s module);
    text                Mangle(text name);
    JITSymbol           Symbol(text name);
    JITTargetAddress    Address(text name);
};


static inline uint64_t globalSymbolAddress(const text &name)
// ----------------------------------------------------------------------------
//    Return the address of the symbol in the current address space
// ----------------------------------------------------------------------------
{
    return RTDyldMemoryManager::getSymbolAddressInProcess(name);
}


static inline CompileCallbacks_u createCallbacks(TargetMachine &target)
// ----------------------------------------------------------------------------
//   Built a callbacks manager
// ----------------------------------------------------------------------------
{
    return createLocalCompileCallbackManager(target.getTargetTriple(), 0);
}

static inline IndirectStubs_u createStubs(TargetMachine &target)
// ----------------------------------------------------------------------------
//   Built a sstubs manager
// ----------------------------------------------------------------------------
{
    auto b = createLocalIndirectStubsManagerBuilder(target.getTargetTriple());
    return b();
}


JITPrivate::JITPrivate()
// ----------------------------------------------------------------------------
//   Constructor for JIT private helper
// ----------------------------------------------------------------------------
// Lifted straight from Kaleidoscope chapter 1
// in LLVM version c921f23f8e457c3b462190e92189fae3ffcc93b0
// (This code is apparently churning quite a bit, and not for simplicity)
// If constructors take lambdas, this is functional code, right?
// Yuck. Barf.
    : optLevel(0),
      context(),
      strings(),
      session(strings),
      resolver(createLegacyLookupResolver(
                   [this](const text &name) -> JITSymbol
                   {
                       if (auto sym = stubs->findStub(name, false))
                           return sym;
                       if (auto sym = optimizer.findSymbol(name, false))
                           return sym;
                       else if (auto err = sym.takeError())
                           return std::move(err);
                       if (auto addr = globalSymbolAddress(name))
                           return JITSymbol(addr, JITSymbolFlags::Exported);
                       return nullptr;
                   },
                   [](Error err)
                   {
                       cantFail(std::move(err), "lookupFlags failed");
                   })),
      target(EngineBuilder().selectTarget()),
      layout(target->createDataLayout()),
      linker(session,
             [this](VModuleKey key)
             {
                 return RTDyldObjectLinkingLayer::Resources
                 {
                     std::make_shared<SectionMemoryManager>(),
                     resolver
                 };
             }),
      compiler(linker, SimpleCompiler(*target)),
      optimizer(compiler,
                [this](Module_s module) {
                    return OptimizeModule(std::move(module));
                }),
      callbacks(createCallbacks(*target)),
      stubs(createStubs(*target))
{
    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
    record(llvm, "JITPrivate %p constructed", this);
}


JITPrivate::~JITPrivate()
// ----------------------------------------------------------------------------
//    Destructor for JIT private helper
// ----------------------------------------------------------------------------
{
    record(llvm, "JITPrivate %p destroyed", this);
}


Module_p JITPrivate::Module()
// ----------------------------------------------------------------------------
//   Return the current module if there is any
// ----------------------------------------------------------------------------
{
    return module.get();
}


Module_p JITPrivate::Module(text name, bool renew)
// ----------------------------------------------------------------------------
//   Add a module to the JIT
// ----------------------------------------------------------------------------
{
    if (renew || module.get() == nullptr)
    {
        module = std::make_shared<llvm::Module>(name, context);
        module->setDataLayout(layout);

        key = session.allocateVModule();
        cantFail(optimizer.addModule(key, module));
    }
    return module.get();
}


VModuleKey JITPrivate::ModuleKey()
// ----------------------------------------------------------------------------
//   Return the key for the current module
// ----------------------------------------------------------------------------
{
    return key;
}


void JITPrivate::DeleteModule(VModuleKey key)
// ----------------------------------------------------------------------------
//   Remove the last module from the JIT
// ----------------------------------------------------------------------------
{
    cantFail(optimizer.removeModule(key));
}


Module_s JITPrivate::OptimizeModule(Module_s module)
// ----------------------------------------------------------------------------
//   Run the optimization pass
// ----------------------------------------------------------------------------
{
    if (optLevel <= 1)
        return module;

    // Create a function pass manager.
    legacy::FunctionPassManager fpm(module.get());

    // Add some optimizations.
    fpm.add(createInstructionCombiningPass());
    fpm.add(createReassociatePass());
    fpm.add(createGVNPass());
    fpm.add(createCFGSimplificationPass());

    if (optLevel >= 3)
    {
        // Start of function pass.
        // Break up aggregate allocas, using SSAUpdater.
        fpm.add(createScalarizerPass());
        fpm.add(createEarlyCSEPass());              // Catch trivial redundancies
        fpm.add(createJumpThreadingPass());         // Thread jumps.
        fpm.add(createCorrelatedValuePropagationPass()); // Propagate conditionals
        fpm.add(createCFGSimplificationPass());     // Merge & remove BBs
        fpm.add(createInstructionCombiningPass());  // Combine silly seq's
        fpm.add(createCFGSimplificationPass());     // Merge & remove BBs
        fpm.add(createReassociatePass());           // Reassociate expressions
        fpm.add(createLoopRotatePass());            // Rotate Loop
        fpm.add(createLICMPass());                  // Hoist loop invariants
        fpm.add(createInstructionCombiningPass());
        fpm.add(createIndVarSimplifyPass());        // Canonicalize indvars
        fpm.add(createLoopIdiomPass());             // Recognize idioms like memset
        fpm.add(createLoopDeletionPass());          // Delete dead loops
        fpm.add(createLoopUnrollPass());            // Unroll small loops
        fpm.add(createInstructionCombiningPass());  // Clean up after the unroller
        fpm.add(createGVNPass());                   // Remove redundancies
        fpm.add(createMemCpyOptPass());             // Remove memcpy / form memset
        fpm.add(createSCCPPass());                  // Constant prop with SCCP

        // Run instcombine after redundancy elimination to exploit opportunities
        // opened up by them.
        fpm.add(createInstructionCombiningPass());
        fpm.add(createJumpThreadingPass());         // Thread jumps
        fpm.add(createCorrelatedValuePropagationPass());
        fpm.add(createDeadStoreEliminationPass());  // Delete dead stores
        fpm.add(createAggressiveDCEPass());         // Delete dead instructions
        fpm.add(createCFGSimplificationPass());     // Merge & remove BBs
    }

    // Run the optimizations over all functions in the module being JITed
    fpm.doInitialization();
    for (auto &f : *module)
      fpm.run(f);

    return module;
}


text JITPrivate::Mangle(text name)
// ----------------------------------------------------------------------------
//   Return the symbol associated with the name
// ----------------------------------------------------------------------------
{
    text mangled;
    raw_string_ostream stream(mangled);
    Mangler::getNameWithPrefix(stream, name, layout);
    return stream.str();
}


JITSymbol JITPrivate::Symbol(text name)
// ----------------------------------------------------------------------------
//   Return the symbol associated with the name
// ----------------------------------------------------------------------------
{
    return optimizer.findSymbol(Mangle(name), true);
}


JITTargetAddress JITPrivate::Address(text name)
// ----------------------------------------------------------------------------
//   Return the address for the given symbol
// ----------------------------------------------------------------------------
{
    return cantFail(Symbol(name).getAddress());
}



// ============================================================================
//
//   JIT interface
//
// ============================================================================

JIT::JIT(int argc, char **argv)
// ----------------------------------------------------------------------------
//   Create the JIT
// ----------------------------------------------------------------------------
    : p(*new JITPrivate())
{
    recorder_type_fn jit_output_format =
        recorder_render<llvm::raw_string_ostream, llvm::Value *>;
    recorder_configure_type('v', jit_output_format);

    std::vector<char *> llvmArgv;
    llvmArgv.push_back(argv[0]);
    for (int arg = 1; arg < argc; arg++)
        if (strncmp(argv[arg], "-llvm", 5) == 0)
            llvmArgv.push_back(argv[arg] + 5);
    llvm::cl::ParseCommandLineOptions(llvmArgv.size(), &llvmArgv[0]);
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
}


JIT::~JIT()
// ----------------------------------------------------------------------------
//   Delete the JIT
// ----------------------------------------------------------------------------
{
    if (p.Module())
        p.DeleteModule(p.key);
    delete &p;
}


Type_p JIT::Type(Value_p value)
// ----------------------------------------------------------------------------
//   The type for the given value
// ----------------------------------------------------------------------------
{
    return value->getType();
}


Type_p JIT::ReturnType(Function_p fn)
// ----------------------------------------------------------------------------
//   The return type for the function
// ----------------------------------------------------------------------------
{
    return fn->getReturnType();
}


bool JIT::InUse(Function_p function)
// ----------------------------------------------------------------------------
//   Check if the function is currently in use
// ----------------------------------------------------------------------------
{
    return function->use_empty();
}


void JIT::EraseFromParent(Function_p function)
// ----------------------------------------------------------------------------
//   Erase function from the parent
// ----------------------------------------------------------------------------
{
    return function->eraseFromParent();
}


bool JIT::VerifyFunction(Function_p function)
// ----------------------------------------------------------------------------
//   Verify the input function, return true in case of error
// ----------------------------------------------------------------------------
//   WARNING: This returns true if there is an error, following llvm
{
    return llvm::verifyFunction(*function, &llvm::errs());
}


void JIT::Print(kstring label, Value_p value)
// ----------------------------------------------------------------------------
//   Print the tree on the error output
// ----------------------------------------------------------------------------
{
    llvm::errs() << label << ":\n";
    value->print(llvm::errs());
}


void JIT::SetOptimizationLevel(unsigned optLevel)
// ----------------------------------------------------------------------------
//   Set the optimization level
// ----------------------------------------------------------------------------
{
    p.optLevel = optLevel;
}


void JIT::PrintStatistics()
// ----------------------------------------------------------------------------
//   Print the LLVM statistics at end of compilation
// ----------------------------------------------------------------------------
{
    llvm::PrintStatistics(llvm::errs());
}


IntegerType_p JIT::IntegerType(unsigned bits)
// ----------------------------------------------------------------------------
//   Create an integer type with the given number of bits
// ----------------------------------------------------------------------------
{
    return IntegerType::get(p.context, bits);
}


Type_p JIT::FloatType(unsigned bits)
// ----------------------------------------------------------------------------
//   Create a floating-type type with the given number of bits
// ----------------------------------------------------------------------------
{
    assert (bits == 16 || bits == 32 || bits == 64);
    if (bits == 16)
        return Type::getHalfTy(p.context);
    if (bits == 32)
        return Type::getFloatTy(p.context);
    return Type::getDoubleTy(p.context);
}


StructType_p JIT::OpaqueType()
// ----------------------------------------------------------------------------
//   Create an opaque type (i.e. a struct without a content)
// ----------------------------------------------------------------------------
{
    return StructType::create(p.context);
}


StructType_p JIT::StructType(StructType_p base, const Signature &elements)
// ----------------------------------------------------------------------------
//    Refine a forward-declared structure type
// ----------------------------------------------------------------------------
{
    base->setBody(elements);
    return base;
}


StructType_p JIT::StructType(const Signature &items)
// ----------------------------------------------------------------------------
//    Define a structure type in one pass
// ----------------------------------------------------------------------------
{
    StructType_p type = StructType::get(p.context, ArrayRef<Type_p>(items));
    return type;
}


FunctionType_p JIT::FunctionType(Type_p rty, const Signature &parms, bool va)
// ----------------------------------------------------------------------------
//    Create a function type
// ----------------------------------------------------------------------------
{
    return llvm::FunctionType::get(rty, parms, va);
}


PointerType_p JIT::PointerType(Type_p rty)
// ----------------------------------------------------------------------------
//    Create a pointer type (always in address space 0)
// ----------------------------------------------------------------------------
{
    return llvm::PointerType::get(rty, 0);
}


void JIT::SetName(Type_p type, text name)
// ----------------------------------------------------------------------------
//   Set the name for a type (for debugging purpose)
// ----------------------------------------------------------------------------
{
    // Not sure if it's possible to set the name for non-struct types anymore
    if (type->isStructTy())
    {
        StructType_p stype = (StructType_p) type;
        if (!stype->isLiteral())
            stype->setName(name);
    }
}


Function_p JIT::Function(FunctionType_p type, text name)
// ----------------------------------------------------------------------------
//    Create a function with the given name and type
// ----------------------------------------------------------------------------
{
    Module_p module = p.Module();
    bool     top = module == nullptr;
    if (top)
    {
        record(llvm_functions, "Creating module for top-level function");
        module = p.Module(name, false);
    }
    Function_p f = llvm::Function::Create(type,
                                          llvm::Function::ExternalLinkage,
                                          name, module);
    record(llvm_functions, "Created %s %v type %v in module %v",
           top ? "top-level function" : "inner function",
           f, type, p.Module());

    if (top)
        p.function = f;

    return f;
}


void JIT::Finalize(Function_p f)
// ----------------------------------------------------------------------------
//   Finalize function code generation
// ----------------------------------------------------------------------------
{
    record(llvm_functions, "Finalizing %v", f);
    verifyFunction(*f);
}


void *JIT::ExecutableCode(Function_p f)
// ----------------------------------------------------------------------------
//   Return an executable pointer to the function
// ----------------------------------------------------------------------------
{
    JITTargetAddress address = p.Address(f->getName());
    record(llvm_functions, "Address of %v is %p (top level %v)",
           f, (void *) address, p.function);
    p.function = nullptr;
    return (void *) address;
}


Function_p JIT::ExternFunction(FunctionType_p type, text name)
// ----------------------------------------------------------------------------
//    Create an extern function with the given name and type
// ----------------------------------------------------------------------------
{
    assert(p.module);
    Function_p f = llvm::Function::Create(type,
                                          llvm::Function::ExternalLinkage,
                                          name, p.Module());
    record(llvm_externals, "Extern function %v type %v", f, type);
    return f;
}


Function_p JIT::Prototype(Function_p function)
// ----------------------------------------------------------------------------
//   Return a function prototype acceptable for this module
// ----------------------------------------------------------------------------
//   If the function is in this module, return it, else return prototype for it
{
    Module_p module = p.Module();
    text     name   = function->getName();

    // First check if we don't already have it in the current module
    if (module)
    {
        if (Function_p f = module->getFunction(name))
        {
            record(llvm_prototypes,
                   "Prototype for %v found in current module %v",
                   f, module);
            return f;
        }
    }

    FunctionType_p type = function->getFunctionType();
    Function_p proto = llvm::Function::Create(type,
                                              llvm::Function::ExternalLinkage,
                                              name,
                                              module);
    record(llvm_prototypes, "Created prototype %v for %v type %v in module %v",
           proto, function, type, module);
    return proto;
}


Value_p JIT::Prototype(Value_p callee)
// ----------------------------------------------------------------------------
//   Build a prototype from a callee that may not be a function
// ----------------------------------------------------------------------------
{
    Type_p type = callee->getType();
    if (type->isFunctionTy())
        return Prototype(Function_p(callee));

    record(llvm_prototypes, "Prototype for value %v type %v", callee, type);
    assert(type->isPointerTy() && "JIT::Prototype requires a callable value");
    return callee;
}



// ============================================================================
//
//    JITBlock class
//
// ============================================================================

typedef IRBuilder<> JITBuilder;

class JITBlockPrivate
// ----------------------------------------------------------------------------
//   LLVM data for a basic block in the JIT
// ----------------------------------------------------------------------------
{
    friend class JITBlock;
    JIT &               jit;
    BasicBlock_p        block;
    JITBuilder          builder;

    JITBlockPrivate(JIT &jit, Function_p function, kstring name);
    JITBlockPrivate(const JITBlockPrivate &other, kstring name);
    ~JITBlockPrivate();

    JITBuilder *        operator->() { return &builder; }
};


JITBlockPrivate::JITBlockPrivate(JIT &jit, Function_p function, kstring name)
// ----------------------------------------------------------------------------
//   Create private data for a JIT block
// ----------------------------------------------------------------------------
    : jit(jit),
      block(BasicBlock::Create(jit.p.context, name, function)),
      builder(block)
{
    record(llvm_blocks, "Create JIT block '%s' %p for block %v",
           name, this, block);
}


JITBlockPrivate::JITBlockPrivate(const JITBlockPrivate &other, kstring name)
// ----------------------------------------------------------------------------
//   Create a new basic block in the same function as 'other'
// ----------------------------------------------------------------------------
    : jit(other.jit),
      block(BasicBlock::Create(jit.p.context, name, other.block->getParent())),
      builder(block)
{
    record(llvm_blocks, "Copy JIT block '%s' %p for block %v from %p",
           name, this, block, &other);
}


JITBlockPrivate::~JITBlockPrivate()
// ----------------------------------------------------------------------------
//   Delete the private information for the block
// ----------------------------------------------------------------------------
{}


JITBlock::JITBlock(JIT &jit, Function_p function, kstring name)
// ----------------------------------------------------------------------------
//   Create a new JIT block
// ----------------------------------------------------------------------------
    : p(jit.p),
      b(*new JITBlockPrivate(jit, function, name))
{}


JITBlock::JITBlock(const JITBlock &other, kstring name)
// ----------------------------------------------------------------------------
//   Create a JIT block for the same function as another block
// ----------------------------------------------------------------------------
    : p(other.p),
      b(*new JITBlockPrivate(other.b, name))
{}


JITBlock::~JITBlock()
// ----------------------------------------------------------------------------
//   Delete the private information
// ----------------------------------------------------------------------------
{
    delete &b;
}


Constant_p JITBlock::IntegerConstant(Type_p ty, uint64_t value)
// ----------------------------------------------------------------------------
//   Build an unsigned integer constant
// ----------------------------------------------------------------------------
{
    assert(ty->isIntegerTy());
    Constant_p result = ConstantInt::get(ty, value);
    record(llvm_constants, "Unsigned constant %v for %llu", result, value);
    return result;
}


Constant_p JITBlock::IntegerConstant(Type_p ty, int64_t value)
// ----------------------------------------------------------------------------
//   Build a signed integer constant
// ----------------------------------------------------------------------------
{
    assert(ty->isIntegerTy());
    Constant_p result = ConstantInt::get(ty, value);
    record(llvm_constants, "Signed constant %v for %lld", result, value);
    return result;
}


Constant_p JITBlock::IntegerConstant(Type_p ty, unsigned value)
// ----------------------------------------------------------------------------
//   Build an unsigned integer constant
// ----------------------------------------------------------------------------
{
    return IntegerConstant(ty, uint64_t(value));
}


Constant_p JITBlock::IntegerConstant(Type_p ty, int value)
// ----------------------------------------------------------------------------
//   Build a signed integer constant
// ----------------------------------------------------------------------------
{
    return IntegerConstant(ty, int64_t(value));
}


Constant_p JITBlock::FloatConstant(Type_p ty, double value)
// ----------------------------------------------------------------------------
//   Build a floating-point constant
// ----------------------------------------------------------------------------
{
    Constant_p result = ConstantFP::get(ty, value);
    record(llvm_constants, "FP constant %v for %g", result, value);
    return result;
}


Constant_p JITBlock::PointerConstant(Type_p type, void *pointer)
// ----------------------------------------------------------------------------
//    Create a constant pointer
// ----------------------------------------------------------------------------
{
    llvm::APInt addr(JIT::BitsPerByte * sizeof(void *), (uintptr_t) pointer);
    Constant_p result = llvm::Constant::getIntegerValue(type, addr);
    record(llvm_constants, "Pointer constant %v for %p", result, pointer);
    return result;
}


Value_p JITBlock::TextConstant(text value)
// ----------------------------------------------------------------------------
//   Return a constant array of characters for the input text
// ----------------------------------------------------------------------------
{
    Value_p result = b->CreateGlobalStringPtr(value);
    record(llvm_constants, "Text constant %v for %s", result, value.c_str());
    return result;
}


void JITBlock::SwitchTo(JITBlock &block)
// ----------------------------------------------------------------------------
//   Switch the insertion point to the point in the other block
// ----------------------------------------------------------------------------
{
    record(llvm_ir, "Switching insertion point of %p to %p (%v)",
           this, &block, block.b.block);
    b->SetInsertPoint(block.b.block);
}


Value_p JITBlock::Call(Value_p callee, Value_p arg1)
// ----------------------------------------------------------------------------
//   Create a call with one argument
// ----------------------------------------------------------------------------
{
    Value_p proto = b.jit.Prototype(callee);
    Value_p result = b->CreateCall(proto, {arg1});
    record(llvm_ir, "Call %v(%v) = %v", callee, arg1, result);
    return result;
}


Value_p JITBlock::Call(Value_p callee, Value_p arg1, Value_p arg2)
// ----------------------------------------------------------------------------
//   Create a call with two arguments
// ----------------------------------------------------------------------------
{
    Value_p proto = b.jit.Prototype(callee);
    Value_p result = b->CreateCall(proto, {arg1, arg2});
    record(llvm_ir, "Call %v(%v, %v) = %v", callee, arg1, arg2, result);
    return result;
}


Value_p JITBlock::Call(Value_p callee, Value_p arg1, Value_p arg2, Value_p arg3)
// ----------------------------------------------------------------------------
//   Create a call with three arguments
// ----------------------------------------------------------------------------
{
    Value_p proto = b.jit.Prototype(callee);
    Value_p result = b->CreateCall(proto, {arg1, arg2});
    record(llvm_ir, "Call %v(%v, %v, %v) = %v", callee, arg1,arg2,arg3, result);
    return result;
}


Value_p JITBlock::Call(Value_p callee, Values &args)
// ----------------------------------------------------------------------------
//   Create a call with an arbitrary list of arguments
// ----------------------------------------------------------------------------
{
    Value_p proto = b.jit.Prototype(callee);
    Value_p result = b->CreateCall(proto, ArrayRef<Value_p>(args));
    record(llvm_ir, "Call %v(#%u) = %v", callee, args.size(), result);
    return result;
}


BasicBlock_p JITBlock::Block()
// ----------------------------------------------------------------------------
//   Return the basic block for this JIT block
// ----------------------------------------------------------------------------
{
    return b.block;
}


Value_p JITBlock::Return(Value_p value)
// ----------------------------------------------------------------------------
//  Return the given value, or RetVoid if nullptr
// ----------------------------------------------------------------------------
{
    auto inst = value ? b->CreateRet(value) : b->CreateRetVoid();
    record(llvm_ir, "Return(%v) is %v", value, inst);
    return inst;
}


Value_p JITBlock::Branch(JITBlock &to)
// ----------------------------------------------------------------------------
//   Create an unconditinal branch
// ----------------------------------------------------------------------------
{
    auto inst = b->CreateBr(to.b.block);
    record(llvm_ir, "Branch(%v) is %v", to.b.block, inst);
    return inst;
}


Value_p JITBlock::Branch(BasicBlock_p to)
// ----------------------------------------------------------------------------
//   Create an unconditinal branch
// ----------------------------------------------------------------------------
{
    auto inst = b->CreateBr(to);
    record(llvm_ir, "Block Branch(%v) is %v", to, inst);
    return inst;
}


Value_p JITBlock::IfBranch(Value_p cond, JITBlock &t, JITBlock &f)
// ----------------------------------------------------------------------------
//  Create a conditional branch
// ----------------------------------------------------------------------------
{
    auto inst = b->CreateCondBr(cond, t.b.block, f.b.block);
    record(llvm_ir, "Conditional branch(%v, %v, %v) = %v",
           cond, t.b.block, f.b.block, inst);
    return inst;
}


Value_p JITBlock::IfBranch(Value_p cond, BasicBlock_p t, BasicBlock_p f)
// ----------------------------------------------------------------------------
//  Create a conditional branch
// ----------------------------------------------------------------------------
{
    auto inst = b->CreateCondBr(cond, t, f);
    record(llvm_ir, "Conditional lock branch(%v, %v, %v) = %v",
           cond, t, f, inst);
    return inst;
}


Value_p JITBlock::Select(Value_p cond, Value_p t, Value_p f)
// ----------------------------------------------------------------------------
//   Create a select
// ----------------------------------------------------------------------------
{
    auto inst = b->CreateSelect(cond, t, f);
    record(llvm_ir, "Conditional select(%v, %v, %v) = %v",
           cond, t, f, inst);
    return inst;

}


Value_p JITBlock::Alloca(Type_p type, kstring name)
// ----------------------------------------------------------------------------
//  Do a local allocation
// ----------------------------------------------------------------------------
{
    auto inst = b->CreateAlloca(type, 0, nullptr, name);
    record(llvm_ir, "Alloca %s(%v) is %v", name, type, inst);
    return inst;
}


Value_p JITBlock::AllocateReturnValue(Function_p f, kstring name)
// ----------------------------------------------------------------------------
//   Do an alloca for the return value
// ----------------------------------------------------------------------------
{
    return Alloca(f->getReturnType(), name);
}


Value_p JITBlock::StructGEP(Value_p ptr, unsigned idx, kstring name)
// ----------------------------------------------------------------------------
//   Accessing a struct element used to be complicated. Now it's incompatible.
// ----------------------------------------------------------------------------
{
    auto inst =  b->CreateStructGEP(nullptr, ptr, idx, name);
    record(llvm_ir, "StructGEP %s(%v, %u) is %v", name, ptr, idx, inst);
    return inst;
}


#define UNARY(Name)                                                     \
/* ------------------------------------------------------------ */      \
/*  Create a unary operator                                     */      \
/* ------------------------------------------------------------ */      \
Value_p JITBlock::Name(Value_p v, kstring name)                         \
{                                                                       \
    auto value = b->Create##Name(v, name);                              \
    record(llvm_ir, #Name " %s(%v) = %v", name, v, value);              \
    return value;                                                       \
}


#define BINARY(Name)                                                    \
/* ------------------------------------------------------------ */      \
/*  Create a binary operator                                    */      \
/* ------------------------------------------------------------ */      \
    Value_p JITBlock::Name(Value_p l, Value_p r, kstring name)          \
{                                                                       \
    auto value = b->Create##Name(l, r, name);                           \
    record(llvm_ir, #Name " %s(%v, %v) = %v", name, l, r, value);       \
    return value;                                                       \
}


#define CAST(Name)                                                      \
/* ------------------------------------------------------------ */      \
/*  Create a cast operation                                     */      \
/* ------------------------------------------------------------ */      \
Value_p JITBlock::Name(Value_p v, Type_p t, kstring name)               \
{                                                                       \
    auto value = b->Create##Name(v, t, name);                           \
    record(llvm_ir, #Name " %s(%v, type %v) = %v", name, v, t, value);  \
    return value;                                                       \
}

#include "llvm-crap.tbl"



// ============================================================================
//
//    Debug helpers
//
// ============================================================================

void debugv(XL::Value_p v)
// ----------------------------------------------------------------------------
//   Dump a value for the debugger
// ----------------------------------------------------------------------------
{
    v->print(errs());
}


void debugv(XL::Type_p t)
// ----------------------------------------------------------------------------
//   Dump a value for the debugger
// ----------------------------------------------------------------------------
{
    t->print(errs());
}

} // namespace XL