//===-- Dialects.cpp - CAPI for dialects ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "structured-c/Dialects.h"

#include "mlir-c/BuiltinTypes.h"
#include "mlir-c/IR.h"
#include "mlir/CAPI/IR.h"
#include "mlir/CAPI/Registration.h"
#include "mlir/CAPI/Support.h"
#include "mlir/IR/Types.h"
#include "structured/Dialect/Substrait/IR/Substrait.h"
#include "structured/Target/SubstraitPB/Export.h"
#include "structured/Target/SubstraitPB/Import.h"
#include "structured/Target/SubstraitPB/Options.h"

using namespace mlir;
using namespace mlir::substrait;

//===----------------------------------------------------------------------===//
// Substrait dialect
//===----------------------------------------------------------------------===//

MLIR_DEFINE_CAPI_DIALECT_REGISTRATION(Substrait, substrait, SubstraitDialect)

/// Converts the provided enum value into the equivalent value from
/// `::mlir::substrait::SerdeFormat`.
SerdeFormat convertSerdeFormat(MlirSubstraitSerdeFormat format) {
  switch (format) {
  case MlirSubstraitBinarySerdeFormat:
    return SerdeFormat::kBinary;
  case MlirSubstraitTextSerdeFormat:
    return SerdeFormat::kText;
  case MlirSubstraitJsonSerdeFormat:
    return SerdeFormat::kJson;
  case MlirSubstraitPrettyJsonSerdeFormat:
    return SerdeFormat::kPrettyJson;
  }
}

MlirModule mlirSubstraitImportPlan(MlirContext context, MlirStringRef input,
                                   MlirSubstraitSerdeFormat format) {
  ImportExportOptions options;
  options.serdeFormat = convertSerdeFormat(format);
  OwningOpRef<ModuleOp> owning =
      translateProtobufToSubstrait(unwrap(input), unwrap(context), options);
  if (!owning)
    return MlirModule{nullptr};
  return MlirModule{owning.release().getOperation()};
}

MlirAttribute mlirSubstraitExportPlan(MlirOperation op,
                                      MlirSubstraitSerdeFormat format) {
  std::string str;
  llvm::raw_string_ostream stream(str);
  ImportExportOptions options;
  options.serdeFormat = convertSerdeFormat(format);
  if (failed(translateSubstraitToProtobuf(unwrap(op), stream, options)))
    return wrap(Attribute());
  MLIRContext *context = unwrap(op)->getContext();
  Attribute attr = StringAttr::get(context, str);
  return wrap(attr);
}
