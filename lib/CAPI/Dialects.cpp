//===-- Dialects.cpp - CAPI for dialects ------------------------*- C++ -*-===//
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "substrait-mlir-c/Dialects.h"

#include "substrait-mlir/Dialect/Substrait/IR/Substrait.h"
#include "substrait-mlir/Target/SubstraitPB/Export.h"
#include "substrait-mlir/Target/SubstraitPB/Import.h"
#include "substrait-mlir/Target/SubstraitPB/Options.h"

#include "mlir-c/IR.h"
#include "mlir-c/Support.h"
#include "mlir/CAPI/IR.h"
#include "mlir/CAPI/Registration.h"
#include "mlir/CAPI/Support.h"
#include "mlir/CAPI/Wrap.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/Types.h"
#include "mlir/Support/LLVM.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdint>
#include <string>

using namespace mlir;
using namespace mlir::substrait;

//===----------------------------------------------------------------------===//
// Substrait dialect
//===----------------------------------------------------------------------===//

MLIR_DEFINE_CAPI_DIALECT_REGISTRATION(Substrait, substrait, SubstraitDialect)

bool mlirTypeIsASubstraitRelationType(MlirType type) {
  return mlir::isa<RelationType>(unwrap(type));
}

MlirType mlirSubstraitRelationTypeGet(MlirContext context, intptr_t numFields,
                                      MlirType *fieldTypes) {
  SmallVector<Type, 4> types;
  ArrayRef<Type> typesRef = unwrapList(numFields, fieldTypes, types);
  return wrap(RelationType::get(unwrap(context), typesRef));
}

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
      translateProtobufToSubstraitPlan(unwrap(input), unwrap(context), options);
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
