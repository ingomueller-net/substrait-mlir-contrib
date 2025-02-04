//===-- SubstraitDialects.cpp - Python module for Substrait MLIR-*- C++ -*-===//
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "mlir-c/BuiltinAttributes.h"
#include "mlir-c/IR.h"
#include "mlir/Bindings/Python/Nanobind.h"         // IWYU pragma: keep
#include "mlir/Bindings/Python/NanobindAdaptors.h" // IWYU pragma: keep
#include "substrait-mlir-c/Dialects.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Signals.h"

namespace nb = nanobind;
using namespace mlir::python::nanobind_adaptors;

NB_MODULE(_substraitDialects, mainModule) {
#ifndef NDEBUG
  static std::string executable =
      llvm::sys::fs::getMainExecutable(nullptr, nullptr);
  llvm::sys::PrintStackTraceOnErrorSignal(executable);
#endif

  //===--------------------------------------------------------------------===//
  // Substrait dialect.
  //===--------------------------------------------------------------------===//
  auto substraitModule = mainModule.def_submodule("substrait");

  //
  // Dialect
  //

  substraitModule.def(
      "register_dialect",
      [](MlirContext context, bool doLoad) {
        MlirDialectHandle handle = mlirGetDialectHandle__substrait__();
        mlirDialectHandleRegisterDialect(handle, context);
        if (doLoad)
          mlirDialectHandleLoadDialect(handle, context);
      },
      nb::arg("context").none() = nb::none(), nb::arg("load") = true,
      nb::sig("def register_dialect("
              "context: Optional[substrait_mlir.ir.Context] = None,"
              "load: bool = True) -> None"),
      "Register and optionally load the dialect with the given context");

  //
  // Types
  //

  mlir_type_subclass(substraitModule, "RelationType",
                     mlirTypeIsASubstraitRelationType)
      .def_classmethod(
          "get",
          [](const nb::object &cls, std::vector<MlirType> fieldTypes,
             MlirContext context) {
            return cls(mlirSubstraitRelationTypeGet(context, fieldTypes.size(),
                                                    fieldTypes.data()));
          },
          nb::arg("cls"), nb::arg("element_type"),
          nb::arg("context").none() = nb::none(),
          nb::sig("def get(cls: object, "
                  "element_type: typing.Sequence[substrait_mlir.ir.Type], "
                  "context: typing.Optional[substrait_mlir.ir.Context] = None) "
                  "-> RelationType"));

  //
  // Import
  //

  static const auto importSubstraitPlan = [](MlirStringRef input,
                                             MlirContext context,
                                             MlirSubstraitSerdeFormat format) {
    MlirModule module = mlirSubstraitImportPlan(context, input, format);
    if (mlirModuleIsNull(module))
      throw std::invalid_argument("Could not import Substrait plan");
    return module;
  };

  substraitModule.def(
      "from_binpb",
      [&](const nb::bytes &input, MlirContext context) {
        MlirStringRef mlirInput{reinterpret_cast<const char *>(input.data()),
                                input.size()};
        return importSubstraitPlan(mlirInput, context,
                                   MlirSubstraitBinarySerdeFormat);
      },
      nb::arg("input") = nb::none(), nb::arg("context") = nb::none(),
      nb::sig("def from_binpb(input: bytes,"
              "context: typing.Optional[substrait_mlir.ir.Context] = None)"
              "-> substrait_mlir.ir.Module"),
      "Import a Substrait plan in the binary protobuf format");

  substraitModule.def(
      "from_textpb",
      [&](const std::string &input, MlirContext context) {
        return importSubstraitPlan({input.data(), input.size()}, context,
                                   MlirSubstraitTextSerdeFormat);
      },
      nb::arg("input") = nb::none(), nb::arg("context") = nb::none(),
      nb::sig("def from_textpb(input: str,"
              "context: typing.Optional[substrait_mlir.ir.Context] = None)"
              "-> substrait_mlir.ir.Module"),
      "Import a Substrait plan in the textual protobuf format");

  substraitModule.def(
      "from_json",
      [&](const std::string &input, MlirContext context) {
        return importSubstraitPlan({input.data(), input.size()}, context,
                                   MlirSubstraitJsonSerdeFormat);
      },
      nb::arg("input") = nb::none(), nb::arg("context") = nb::none(),
      nb::sig("def from_json(input: str,"
              "context: typing.Optional[substrait_mlir.ir.Context] = None)"
              "-> substrait_mlir.ir.Module"),
      "Import a Substrait plan in the JSON format");

  //
  // Export
  //

  static const auto exportSubstraitPlan = [](MlirOperation op,
                                             MlirSubstraitSerdeFormat format) {
    MlirAttribute attr = mlirSubstraitExportPlan(op, format);
    if (mlirAttributeIsNull(attr))
      throw std::invalid_argument("Could not export Substrait plan");
    MlirStringRef strRef = mlirStringAttrGetValue(attr);
    std::string_view str(strRef.data, strRef.length);
    return str;
  };

  substraitModule.def(
      "to_binpb",
      [&](MlirOperation op) {
        std::string_view sv =
            exportSubstraitPlan(op, MlirSubstraitBinarySerdeFormat);
        return nb::bytes(sv.data(), sv.size());
      },
      nb::arg("op"),
      nb::sig("def to_binpb("
              "op: typing.Optional[substrait_mlir.ir.Operation] = None)"
              "-> bytes"),
      "Export a Substrait plan into the binary protobuf format");

  substraitModule.def(
      "to_textpb",
      [&](MlirOperation op) {
        return exportSubstraitPlan(op, MlirSubstraitTextSerdeFormat);
      },
      nb::arg("op"),
      nb::sig(
          "def to_textpb("
          "op: typing.Optional[substrait_mlir.ir.Operation] = None) -> str"),
      "Export a Substrait plan into the textual protobuf format");

  substraitModule.def(
      "to_json",
      [&](MlirOperation op, bool pretty) {
        auto format = pretty ? MlirSubstraitPrettyJsonSerdeFormat
                             : MlirSubstraitJsonSerdeFormat;
        return exportSubstraitPlan(op, format);
      },
      nb::arg("op"), nb::arg("pretty") = false,
      nb::sig("def to_json("
              "op: typing.Optional[substrait_mlir.ir.Operation] = None,"
              "pretty: typing.Optional[bool] = False) -> str"),
      "Export a Substrait plan into the JSON format");
}
