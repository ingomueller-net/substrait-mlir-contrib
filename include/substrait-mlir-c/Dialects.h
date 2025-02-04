//===-- Dialects.h - CAPI for dialects ----------------------------*- C -*-===//
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SUBSTRAIT_MLIR_C_DIALECTS_H
#define SUBSTRAIT_MLIR_C_DIALECTS_H

#include "mlir-c/IR.h"
#include "mlir-c/Support.h"

#ifdef __cplusplus
extern "C" {
#endif

//===----------------------------------------------------------------------===//
// Substrait dialect
//===----------------------------------------------------------------------===//

MLIR_DECLARE_CAPI_DIALECT_REGISTRATION(Substrait, substrait);

/// Checks whether the given type is a `RelationType`.
MLIR_CAPI_EXPORTED
bool mlirTypeIsASubstraitRelationType(MlirType type);

/// Gets the `RelationType` with the given `fieldTypes`.
MLIR_CAPI_EXPORTED
MlirType mlirSubstraitRelationTypeGet(MlirContext context, intptr_t numFields,
                                      MlirType *fieldTypes);

/// Serialization/deserialization format for exporting/importing Substrait
/// plans. This corresponds to `::mlir::substrait::SerdeFormat`.
typedef enum MlirSubstraitSerdeFormat {
  MlirSubstraitTextSerdeFormat,
  MlirSubstraitBinarySerdeFormat,
  MlirSubstraitJsonSerdeFormat,
  MlirSubstraitPrettyJsonSerdeFormat
} MlirSubstraitSerdeFormat;

/// Imports a `Plan` message from `input`, which must be in the specified
/// serialization format. Returns a null module and emits diagnostics in case of
/// an error.
MLIR_CAPI_EXPORTED
MlirModule mlirSubstraitImportPlan(MlirContext context, MlirStringRef input,
                                   MlirSubstraitSerdeFormat format);

/// Exports the provided `substrait.plan` or `builtin.module` op to protobuf in
/// the specified serialization format stored in the value of a `StringAttr`.
/// Returns a null attribute and emits diagnostics in case of an error.
MLIR_CAPI_EXPORTED
MlirAttribute mlirSubstraitExportPlan(MlirOperation op,
                                      MlirSubstraitSerdeFormat format);

#ifdef __cplusplus
}
#endif

#endif // SUBSTRAIT_MLIR_C_DIALECTS_H
