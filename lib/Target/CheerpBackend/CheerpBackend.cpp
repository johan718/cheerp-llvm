//===-- CheerpBackend.cpp - Backend wrapper for CheerpWriter---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// Copyright 2011-2014 Leaning Technologies
//
//===----------------------------------------------------------------------===//

#include "CheerpTargetMachine.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Type.h"
#include "llvm/Cheerp/Writer.h"
#include "llvm/Cheerp/AllocaMerging.h"
#include "llvm/Cheerp/ResolveAliases.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ToolOutputFile.h"

using namespace llvm;

static cl::opt<std::string> SourceMap("cheerp-sourcemap", cl::Optional,
  cl::desc("If specified, the file name of the source map"), cl::value_desc("filename"));

static cl::opt<bool> PrettyCode("cheerp-pretty-code", cl::desc("Generate human-readable JS") );

extern "C" void LLVMInitializeCheerpBackendTarget() {
  // Register the target.
  RegisterTargetMachine<CheerpTargetMachine> X(TheCheerpBackendTarget);
}

namespace {
  class CheerpWritePass : public ModulePass {
  private:
    formatted_raw_ostream &Out;
    static char ID;
    void getAnalysisUsage(AnalysisUsage& AU) const;
  public:
    explicit CheerpWritePass(formatted_raw_ostream &o) :
      ModulePass(ID), Out(o) { }
    bool runOnModule(Module &M);
  };
} // end anonymous namespace.

bool CheerpWritePass::runOnModule(Module& M)
{
  AliasAnalysis &AA = getAnalysis<AliasAnalysis>();
  if (!SourceMap.empty())
  {
    std::string ErrorString;
    tool_output_file sourceMap(SourceMap.c_str(), ErrorString, sys::fs::F_None);
    if (!ErrorString.empty())
    {
       // An error occurred opening the source map file, bail out
       llvm::report_fatal_error(ErrorString.c_str(), false);
       return false;
    }
    cheerp::CheerpWriter writer(M, Out, AA, SourceMap, &sourceMap.os(), PrettyCode);
    sourceMap.keep();
    writer.makeJS();
  }
  else
  {
    cheerp::CheerpWriter writer(M, Out, AA, SourceMap, NULL, PrettyCode);
    writer.makeJS();
  }

  return false;
}

void CheerpWritePass::getAnalysisUsage(AnalysisUsage& AU) const
{
  AU.addRequired<AliasAnalysis>();
}

char CheerpWritePass::ID = 0;

//===----------------------------------------------------------------------===//
//                       External Interface declaration
//===----------------------------------------------------------------------===//

bool CheerpTargetMachine::addPassesToEmitFile(PassManagerBase &PM,
                                           formatted_raw_ostream &o,
                                           CodeGenFileType FileType,
                                           bool DisableVerify,
                                           AnalysisID StartAfter,
                                           AnalysisID StopAfter) {
  if (FileType != TargetMachine::CGFT_AssemblyFile) return true;
  PM.add(createResolveAliasesPass());
  PM.add(createAllocaMergingPass());
  PM.add(new CheerpWritePass(o));
  return false;
}