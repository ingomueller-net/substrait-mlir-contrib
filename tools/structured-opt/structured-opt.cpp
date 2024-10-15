//===-- structured-opt.cpp - Optimizer Driver for Structured ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Main entry function for mlir-opt for when built as standalone binary.
// Reuse Registration.cpp as much as possible.
//
//===----------------------------------------------------------------------===//

#include "mlir/IR/Dialect.h"
#include "mlir/InitAllDialects.h"
#include "mlir/InitAllExtensions.h"
#include "mlir/InitAllPasses.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"
#include "structured/Dialect/Substrait/IR/Substrait.h"
#include "structured/Dialect/Substrait/Transforms/Passes.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ToolOutputFile.h"

using namespace mlir;
using namespace mlir::substrait;

static void registerSubstraitDialects(DialectRegistry &registry) {
  registry.insert<mlir::substrait::SubstraitDialect>();
}

int main(int argc, char **argv) {
#ifndef NDEBUG
  static std::string executable =
      llvm::sys::fs::getMainExecutable(nullptr, nullptr);
  llvm::sys::PrintStackTraceOnErrorSignal(executable);
#endif

  registerAllPasses();
  registerSubstraitPasses();

  DialectRegistry registry;
  registerAllDialects(registry);
  registerAllExtensions(registry);
  registerSubstraitDialects(registry);

  return failed(
      MlirOptMain(argc, argv, "MLIR modular optimizer driver\n", registry));
}
