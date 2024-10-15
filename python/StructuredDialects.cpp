//===-- StructuredDialects.cpp - Extension module ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "mlir-c/BuiltinAttributes.h"
#include "mlir-c/BuiltinTypes.h"
#include "mlir-c/IR.h"
#include "mlir/Bindings/Python/PybindAdaptors.h"
#include "structured-c/Dialects.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Signals.h"

#include <vector>

namespace py = pybind11;
using namespace mlir::python::adaptors;

PYBIND11_MODULE(_structuredDialects, mainModule) {
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
      py::arg("context") = py::none(), py::arg("load") = true);

  //
  // Import
  //

  static const auto importSubstraitPlan = [](const std::string &input,
                                             MlirContext context,
                                             MlirSubstraitSerdeFormat format) {
    MlirStringRef mlirInput{/*data=*/input.data(), /*length=*/input.size()};
    MlirModule module = mlirSubstraitImportPlan(context, mlirInput, format);
    if (mlirModuleIsNull(module))
      throw std::invalid_argument("Could not import Substrait plan");
    return module;
  };

  substraitModule.def(
      "from_binpb",
      [&](const std::string &input, MlirContext context) {
        return importSubstraitPlan(input, context,
                                   MlirSubstraitBinarySerdeFormat);
      },
      py::arg("input") = py::none(), py::arg("context") = py::none(),
      "Import a Substrait plan in the binary protobuf format");

  substraitModule.def(
      "from_textpb",
      [&](const std::string &input, MlirContext context) {
        return importSubstraitPlan(input, context,
                                   MlirSubstraitTextSerdeFormat);
      },
      py::arg("input") = py::none(), py::arg("context") = py::none(),
      "Import a Substrait plan in the textual protobuf format");

  substraitModule.def(
      "from_json",
      [&](const std::string &input, MlirContext context) {
        return importSubstraitPlan(input, context,
                                   MlirSubstraitJsonSerdeFormat);
      },
      py::arg("input") = py::none(), py::arg("context") = py::none(),
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
        return exportSubstraitPlan(op, MlirSubstraitBinarySerdeFormat);
      },
      py::arg("op"), "Export a Substrait plan into the binary protobuf format");

  substraitModule.def(
      "to_textpb",
      [&](MlirOperation op) {
        return exportSubstraitPlan(op, MlirSubstraitTextSerdeFormat);
      },
      py::arg("op"),
      "Export a Substrait plan into the textual protobuf format");

  substraitModule.def(
      "to_json",
      [&](MlirOperation op, bool pretty) {
        auto format = pretty ? MlirSubstraitPrettyJsonSerdeFormat
                             : MlirSubstraitJsonSerdeFormat;
        return exportSubstraitPlan(op, format);
      },
      py::arg("op"), py::arg("pretty") = false,
      "Export a Substrait plan into the JSON format");
}
