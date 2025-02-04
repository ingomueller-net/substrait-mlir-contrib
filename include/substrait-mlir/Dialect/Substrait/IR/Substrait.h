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
#include "mlir/Interfaces/CastInterfaces.h"       // IWYU: keep
#include "mlir/Interfaces/InferTypeOpInterface.h" // IWYU: keep

namespace mlir::substrait::detail {
struct RelationTypeStorage;
} // namespace mlir::substrait::detail

//===----------------------------------------------------------------------===//
// Substrait dialect
//===----------------------------------------------------------------------===//

#include "substrait-mlir/Dialect/Substrait/IR/SubstraitOpsDialect.h.inc" // IWYU: export

//===----------------------------------------------------------------------===//
// Substrait enums
//===----------------------------------------------------------------------===//

#include "substrait-mlir/Dialect/Substrait/IR/SubstraitEnums.h.inc" // IWYU: export

//===----------------------------------------------------------------------===//
// Substrait types
//===----------------------------------------------------------------------===//

#include "substrait-mlir/Dialect/Substrait/IR/SubstraitTypeInterfaces.h.inc" // IWYU: export
#define GET_TYPEDEF_CLASSES
#include "substrait-mlir/Dialect/Substrait/IR/SubstraitOpsTypes.h.inc" // IWYU: export

//===----------------------------------------------------------------------===//
// Substrait attributes
//===----------------------------------------------------------------------===//

#include "substrait-mlir/Dialect/Substrait/IR/SubstraitAttrInterfaces.h.inc" // IWYU: export
#define GET_ATTRDEF_CLASSES
#include "substrait-mlir/Dialect/Substrait/IR/SubstraitOpsAttrs.h.inc" // IWYU: export

//===----------------------------------------------------------------------===//
// Substrait ops
//===----------------------------------------------------------------------===//

#include "substrait-mlir/Dialect/Substrait/IR/SubstraitOpInterfaces.h.inc" // IWYU: export
#define GET_OP_CLASSES
#include "substrait-mlir/Dialect/Substrait/IR/SubstraitOps.h.inc" // IWYU: export

namespace mlir::substrait {

/// Returns the `Type` of the attribute through the `TypedAttrInterface` or the
/// `TypeInferableAttrInterface`. Returns an empty `Type` if the given attribute
/// does not implement one of the two interfaces.
Type getAttrType(Attribute attr);

} // namespace mlir::substrait

#endif // SUBSTRAIT_MLIR_DIALECT_SUBSTRAIT_IR_SUBSTRAIT_H
