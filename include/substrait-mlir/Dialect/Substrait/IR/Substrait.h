//===-- Substrait.h - Substrait dialect -------------------------*- C++ -*-===//
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SUBSTRAIT_MLIR_DIALECT_SUBSTRAIT_IR_SUBSTRAIT_H
#define SUBSTRAIT_MLIR_DIALECT_SUBSTRAIT_IR_SUBSTRAIT_H

#include "mlir/Dialect/Func/IR/FuncOps.h"         // IWYU: keep
#include "mlir/IR/Dialect.h"                      // IWYU: keep
#include "mlir/IR/OpImplementation.h"             // IWYU: keep
#include "mlir/IR/SymbolTable.h"                  // IWYU: keep
#include "mlir/Interfaces/InferTypeOpInterface.h" // IWYU: keep

#include "substrait-mlir/Dialect/Substrait/IR/SubstraitOpsDialect.h.inc" // IWYU: export

#include "substrait-mlir/Dialect/Substrait/IR/SubstraitEnums.h.inc" // IWYU: export
#include "substrait-mlir/Dialect/Substrait/IR/SubstraitOpInterfaces.h.inc" // IWYU: export
#include "substrait-mlir/Dialect/Substrait/IR/SubstraitTypeInterfaces.h.inc" // IWYU: export

#define GET_TYPEDEF_CLASSES
#include "substrait-mlir/Dialect/Substrait/IR/SubstraitOpsTypes.h.inc" // IWYU: export

#define GET_OP_CLASSES
#include "substrait-mlir/Dialect/Substrait/IR/SubstraitOps.h.inc" // IWYU: export

#endif // SUBSTRAIT_MLIR_DIALECT_SUBSTRAIT_IR_SUBSTRAIT_H
