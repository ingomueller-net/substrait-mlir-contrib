//===-- SubstraitInterfaces.td - Substrait interfaces ------*- tablegen -*-===//
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SUBSTRAIT_DIALECT_SUBSTRAIT_IR_SUBSTRAITINTERFACES
#define SUBSTRAIT_DIALECT_SUBSTRAIT_IR_SUBSTRAITINTERFACES

include "mlir/IR/OpBase.td"

def TypeInferableAttrInterface : AttrInterface<"TypeInferableAttrInterface"> {
  let cppNamespace = "::mlir::substrait";
  let description = [{
    This interface is used for attributes that have a type that can be inferred
    from the instance of the attribute. It is similar to the built-in
    `TypedAttrInterface` in that that type is understood to represent the type
    of the data contained in the attribute. However, it is different in that
    `TypedAttrInterface` is typically used for cases where the type is a
    parameter of the attribute such that there can be attribute instances with
    the same value but different types. With this interface, the type must be
    inferable from the value such that two instances with the same value always
    have the same type. Crucially, this allows to elide the type in the assembly
    format of the attribute.
  }];
  let methods = [InterfaceMethod<
    "Get the attribute's type",
    "::mlir::Type", "getType"
  >];
}

def Substrait_ExpressionOpInterface : OpInterface<"ExpressionOpInterface"> {
  let description = [{
    Interface for any expression in a Substrait plan. This corresponds to an
    `Expression` message, which only consists of the `rex_type` field, which, in
    turn, holds a more specialized message with the information specific to the
    concrete expression.
  }];
  let cppNamespace = "::mlir::substrait";
}

def Substrait_ExtensibleOpInterface : OpInterface<"ExtensibleOpInterface"> {
  let description = [{
    Interface for ops with the `advanced_extension` attribute. Several relations
    and other message types of the Substrait specification have a field with the
    same name (or the variant `advanced_extensions`, which has the same meaning)
    and the interface enables handling all of them transparently.
  }];
  let cppNamespace = "::mlir::substrait";
  let methods = [
    InterfaceMethod<
      "Get the `advanced_extension` attribute",
      "std::optional<::mlir::substrait::AdvancedExtensionAttr>",
      "getAdvancedExtension">,
    InterfaceMethod<
      "Get the `advanced_extension` attribute",
      "void", "setAdvancedExtensionAttr",
      (ins "::mlir::substrait::AdvancedExtensionAttr":$attr)>,
    ];
}

def Substrait_RelOpInterface : OpInterface<"RelOpInterface"> {
  let description = [{
    Interface for any relational operation in a Substrait plan. This corresponds
    to a `Rel` message, which only consists of the `rel_type` field, which, in
    turn, holds a more specialized message with the information specific to the
    concrete relational operation.
  }];
  let methods = [InterfaceMethod<
    "Get the result relation",
    "::mlir::TypedValue<::mlir::substrait::RelationType>", "getResult",
    /*args=*/(ins ), /*methodBody=*/ "return $_op.getResult();"
  >];
  let cppNamespace = "::mlir::substrait";
}

#endif // SUBSTRAIT_DIALECT_SUBSTRAIT_IR_SUBSTRAITINTERFACES
