//===-- SubstraitEnums.td - Substrait enums definitions ----*- tablegen -*-===//
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SUBSTRAIT_DIALECT_SUBSTRAIT_IR_SUBSTRAITENUMS
#define SUBSTRAIT_DIALECT_SUBSTRAIT_IR_SUBSTRAITENUMS

include "mlir/IR/EnumAttr.td"

def AggregationInvocationUnspecified: I32EnumAttrCase<"unspecified", 0>;
def AggregationInvocationAll: I32EnumAttrCase<"all", 1>;
def AggregationInvocationDistinct: I32EnumAttrCase<"distinct", 2>;

/// Represents the `AggregationInvocation` protobuf enum.
def AggregationInvocation : I32EnumAttr<
    "AggregationInvocation", "Aggregate invocation type", [
      AggregationInvocationUnspecified,
      AggregationInvocationAll,
      AggregationInvocationDistinct
    ]> {
  let cppNamespace = "::mlir::substrait";
}

#endif // SUBSTRAIT_DIALECT_SUBSTRAIT_IR_SUBSTRAITENUMS
