//===-- SubstraitOps.td - Substrait operations definitions -*- tablegen -*-===//
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SUBSTRAIT_DIALECT_SUBSTRAIT_IR_SUBSTRAITOPS
#define SUBSTRAIT_DIALECT_SUBSTRAIT_IR_SUBSTRAITOPS

include "substrait-mlir/Dialect/Substrait/IR/SubstraitAttrs.td"
include "substrait-mlir/Dialect/Substrait/IR/SubstraitDialect.td"
include "substrait-mlir/Dialect/Substrait/IR/SubstraitEnums.td"
include "substrait-mlir/Dialect/Substrait/IR/SubstraitInterfaces.td"
include "substrait-mlir/Dialect/Substrait/IR/SubstraitTypes.td"
include "mlir/Interfaces/CastInterfaces.td"
include "mlir/Interfaces/InferTypeOpInterface.td"
include "mlir/Interfaces/SideEffectInterfaces.td"
include "mlir/IR/BuiltinAttributes.td"
include "mlir/IR/CommonAttrConstraints.td"
include "mlir/IR/EnumAttr.td"
include "mlir/IR/OpBase.td"
include "mlir/IR/SymbolInterfaces.td"

class Substrait_Op<string mnemonic, list<Trait> traits = []> :
  Op<Substrait_Dialect, mnemonic, traits> {
}

//===----------------------------------------------------------------------===//
// Constraints
//===----------------------------------------------------------------------===//

class RegionOf<Constraint condition> : Region<
  Concat<"::llvm::all_of($_self.getOps(), [](::mlir::Operation &op) { "
         "return ",
         SubstLeaves<"$_self", "op", condition.predicate>,
         "; })">,
  "region where each child op is " # condition.summary>;

class IsOp<string opType> : Constraint<
  CPred<"::llvm::isa<" # opType # ">($_self)">,
  "op of type '" # opType # "'">;

class AnyOf<list<Constraint> opTypes, string summary = ""> : Constraint<
  Or<!foreach(opType, opTypes, opType.predicate)>,
     !if(!eq(summary, ""),
        !interleave(!foreach(t, opTypes, t.summary), " or "),
        summary)>;

def StringArrayAttr :
    TypedArrayAttrBase<Builtin_StringAttr,
                       "array attribute of string attributes"> {
  let storageType = [{ ::mlir::ArrayAttr }];
}

//===----------------------------------------------------------------------===//
// Extensions
//===----------------------------------------------------------------------===//
// The definitions in this section are related to the extension messages.
// See https://substrait.io/serialization/binary_serialization/ and
// https://github.com/substrait-io/substrait/blob/main/proto/substrait/extensions/extensions.proto.
//===----------------------------------------------------------------------===//

def Substrait_ExtensionUriOp : Substrait_Op<"extension_uri", [
    Symbol
  ]> {
  let summary = "Declares a simple extension URI";
  let description = [{
    This op represents the `SimpleExtensionURI` message type of Substrait. It is
    a `Symbol` op, so it can be looked up in the symbol table of the plan it is
    contained in.

    Example code:

    ```mlir
    substrait.plan version 0 : 42 : 1 {
      extension_uri @uri at "http://some.url/with/extensions.yml"
      extension_function @function at @uri["func1"]
      // ...
    }
    ```
  }];
  let arguments = (ins
    SymbolNameAttr:$sym_name, // corresponds to `anchor`
    StrAttr:$uri
  );
  let assemblyFormat = "$sym_name `at` $uri attr-dict";
}

class Substrait_ExtensionOp<string mnemonic, list<Trait> traits = []> :
  Substrait_Op<"extension_" # mnemonic, traits # [
    DeclareOpInterfaceMethods<SymbolUserOpInterface>,
    DeclareOpInterfaceMethods<Symbol>
  ]> {
  let description = [{
      This op represents the `SimpleExtensionDeclaration` message type of
      Substrait along with the `Extension}]
      # snakeCaseToCamelCase<mnemonic>.ret #
    [{` message type in the `mapping_type` case. It is both a `Symbol` op, so it
      can be looked up in the symbol table of the plan it is contained in.
      Conversely, its symbol reference `uri` must refer to an extension URI op
      in the nearest symbol table.
    }];
  let arguments = (ins
    SymbolNameAttr:$sym_name, // corresponds to `anchor`
    FlatSymbolRefAttr:$uri,
    StrAttr:$name
  );
  let assemblyFormat = "$sym_name `at` $uri `[` $name `]` attr-dict";
  let extraClassDefinition = [{
    /// Implement `SymbolOpInterface`.
    ::mlir::LogicalResult $cppClass::verifySymbolUses(
        mlir::SymbolTableCollection &symbolTables) {
      if (!symbolTables.lookupNearestSymbolFrom<ExtensionUriOp>(*this,
                                                                getUriAttr()))
        return emitOpError() << "refers to " << getUriAttr()
                            << ", which is not a valid 'uri' op";
      return success();
    }
  }];
}

def Substrait_ExtensionFunctionOp : Substrait_ExtensionOp<"function"> {
  let summary = "Declares a simple extension function";
}

def Substrait_ExtensionTypeOp : Substrait_ExtensionOp<"type"> {
  let summary = "Declares a simple extension type";
}

def Substrait_ExtensionTypeVariationOp :
    Substrait_ExtensionOp<"type_variation"> {
  let summary = "Declares a simple extension type variation";
}

//===----------------------------------------------------------------------===//
// Top-level ops
//===----------------------------------------------------------------------===//
// The definitions in this section are related to the top-level messages.
// See https://substrait.io/serialization/binary_serialization/ and
// https://github.com/substrait-io/substrait/blob/main/proto/substrait/plan.proto.
//===----------------------------------------------------------------------===//

def PlanBodyOp : AnyOf<[
    IsOp<"::mlir::substrait::PlanRelOp">,
    IsOp<"::mlir::substrait::ExtensionUriOp">,
    IsOp<"::mlir::substrait::ExtensionFunctionOp">,
    IsOp<"::mlir::substrait::ExtensionTypeOp">,
    IsOp<"::mlir::substrait::ExtensionTypeVariationOp">,
  ]>;

def Substrait_PlanVersionOp : Substrait_Op<"plan_version"> {
  let summary = "Represents a stand-alone plan version";
  let description = [{
    This op represents the `PlanVersion` message type of Substrait. It carries
    the version information as an attribute, so it also subsumes the `Version`
    message type.
  }];
  let arguments = (ins
    Substrait_VersionAttr:$version
  );
  let assemblyFormat = "$version attr-dict";
}

def Substrait_PlanOp : Substrait_Op<"plan", [
    DeclareOpInterfaceMethods<OpAsmOpInterface, ["getDefaultDialect"]>,
    DeclareOpInterfaceMethods<Substrait_ExtensibleOpInterface>,
    NoTerminator, NoRegionArguments, SingleBlock, SymbolTable
  ]> {
  let summary = "Represents a Substrait plan";
  let description = [{
    This op represents the `Plan` message type of Substrait. It carries the
    version information inline as attributes, so it also subsumes the `Version`
    message type. The body of the op consists of the `relation`s and the
    function and type extensions defined in the plan.
  }];
  let arguments = (ins
    UI32Attr:$major_number,
    UI32Attr:$minor_number,
    UI32Attr:$patch_number,
    DefaultValuedAttr<StrAttr, "\"\"">:$git_hash,
    DefaultValuedAttr<StrAttr, "\"\"">:$producer,
    OptionalAttr<Substrait_AdvancedExtensionAttr>:$advanced_extension,
    OptionalAttr<StringArrayAttr>:$expected_type_urls
  );
  let regions = (region RegionOf<PlanBodyOp>:$body);
  let assemblyFormat = [{
    `version` $major_number `:` $minor_number `:` $patch_number
    (`git_hash` $git_hash^)? (`producer` $producer^)?
    (`advanced_extension` `` $advanced_extension^)?
    (`expected_type_urls` `` $expected_type_urls^)?
    attr-dict-with-keyword $body
  }];
  let builders = [
      OpBuilder<(ins "uint32_t":$major, "uint32_t":$minor, "uint32_t":$patch), [{
        build($_builder, $_state, major, minor, patch,
              /*git_hash=*/StringAttr(), /*producer*/StringAttr());
      }]>,
      OpBuilder<
        (ins "uint32_t":$major, "uint32_t":$minor, "uint32_t":$patch,
             "::llvm::StringRef":$git_hash, "::llvm::StringRef":$producer), [{
        build($_builder, $_state, major, minor, patch, git_hash, producer,
              /*advanced_extension=*/AdvancedExtensionAttr());
      }]>,
      OpBuilder<
        (ins "uint32_t":$major, "uint32_t":$minor, "uint32_t":$patch,
             "::llvm::StringRef":$git_hash, "::llvm::StringRef":$producer,
             "::mlir::substrait::AdvancedExtensionAttr":$advanced_extension), [{
        build($_builder, $_state, major, minor, patch, git_hash, producer,
              advanced_extension, /*expected_type_urls=*/ArrayAttr());
      }]>,
    ];
  let extraClassDefinition = [{
    /// Implement OpAsmOpInterface.
    ::llvm::StringRef $cppClass::getDefaultDialect() {
      return SubstraitDialect::getDialectNamespace();
    }
  }];
}

def RelationBodyOp : AnyOf<[
    IsOp<"::mlir::substrait::RelOpInterface">,
    IsOp<"::mlir::substrait::YieldOp">
  ]>;

def Substrait_PlanRelOp : Substrait_Op<"relation", [
    DeclareOpInterfaceMethods<OpAsmOpInterface, ["getDefaultDialect"]>,
    HasParent<"::mlir::substrait::PlanOp">,
    SingleBlockImplicitTerminator<"::mlir::substrait::YieldOp">,
    NoRegionArguments
  ]> {
  let summary = "Represents a query tree in a Substrait plan";
  let description = [{
    Represents a `PlanRel` message, which is used in the `relations` field of
    the `Plan` message. The same op can represent either the `Rel`, in which
    case the `fieldNames` attribute is not set, or the `RootRel` case, in which
    case the `fieldNames` attribute corresponds to the `RelRoot.names` field.
    The body of this op contains various `RelOpInterface` ops (corresponding to
    the `Rel` message type) producing SSA values and the one being yielded
    reprents the root of the query tree that this op contains.
  }];
  let arguments = (ins OptionalAttr<StringArrayAttr>:$fieldNames);
  let regions = (region RegionOf<RelationBodyOp>:$body);
  let assemblyFormat = "(`as` $fieldNames^)? attr-dict-with-keyword $body";
  let hasRegionVerifier = 1;
  let builders = [
      OpBuilder<(ins ), [{
        build($_builder, $_state, ArrayAttr());
      }]>
    ];
  let extraClassDefinition = [{
    /// Implement OpAsmOpInterface.
    ::llvm::StringRef $cppClass::getDefaultDialect() {
      return SubstraitDialect::getDialectNamespace();
    }
  }];
}

def Substrait_YieldOp : Substrait_Op<"yield", [
    Terminator,
    ParentOneOf<[
      "::mlir::substrait::AggregateOp",
      "::mlir::substrait::FilterOp",
      "::mlir::substrait::PlanRelOp",
      "::mlir::substrait::ProjectOp"
    ]>
  ]> {
  let summary = "Yields the result of a `PlanRelOp`";
  let arguments = (ins Variadic<AnyType>:$value);
  let assemblyFormat = "attr-dict ($value^ `:` type($value))?";
  let builders = [OpBuilder<(ins), [{ /* do nothing */ }]>];
}

//===----------------------------------------------------------------------===//
// Expressions
//===----------------------------------------------------------------------===//
// The definitions in this section are related to the various `Expression`
// message types. See https://substrait.io/expressions/field_references/ and
// https://github.com/substrait-io/substrait/blob/main/proto/substrait/algebra.proto.
//===----------------------------------------------------------------------===//

// TODO(ingomueller): Make this a proper base for expressions.
class Substrait_ExpressionOp<string mnemonic, list<Trait> traits = []> :
  Substrait_Op<mnemonic, traits # [
    Substrait_ExpressionOpInterface, Pure
  ]>;

def Substrait_CastOp : Substrait_ExpressionOp<"cast", [
     DeclareOpInterfaceMethods<CastOpInterface>
  ]> {
  let summary = "Cast expression";
  let description = [{
    Represents a `Cast` message.

    Example:

    ```mlir
    %0 = literal 42 : si32
    %1 = cast %0 : si32 to si64
    ```
  }];
  let arguments = (ins
    Substrait_ExpressionType:$input,
    FailureBehavior:$failure_behavior
  );
  // TODO(ingomueller): verify that result type is nullable if
  // `failure_behavior` is set to `return_null` once we have implemented
  // nullability.
  let results = (outs Substrait_ExpressionType:$result);
  let assemblyFormat = [{
    $input `or` $failure_behavior attr-dict `:` type($input) `to` type($result)
  }];
  let extraClassDefinition = [{
    /// Implement `CastOpInterface`.
    bool $cppClass::areCastCompatible(::mlir::TypeRange inputs,
                                      ::mlir::TypeRange outputs) {
      return true;
    }
  }];
}

def Substrait_FieldReferenceOp : Substrait_ExpressionOp<"field_reference", [
    DeclareOpInterfaceMethods<InferTypeOpInterface>
  ]> {
  let summary = "Field reference expression";
  let description = [{
    Represents a `FieldReference` message together with all messages it contains
    and the `Expression` message it is contained in.

    Example:

    ```mlir
    %0 = ...
    %1 = field_reference %0[0] : tuple<si32>
    // %1 is of type `si32`
    ```
  }];
  // TODO(ingomueller): extend to other types, map access, and masked references.
  let arguments = (ins
    Substrait_ContainerType:$container,
    DenseI64ArrayAttr:$position
  );
  let results = (outs Substrait_ExpressionType:$result);
  let assemblyFormat = [{
    $container `` $position attr-dict `:` type($container)
  }];
}

def Substrait_LiteralOp : Substrait_ExpressionOp<"literal", [
    ConstantLike,
    DeclareOpInterfaceMethods<InferTypeOpInterface>
  ]> {
  let summary = "Literal expression";
  let description = [{
    Represents a `Literal` message together with all messages it contains and
    the `Expression` message it is contained in.

    Example:

    ```mlir
    %0 = literal -1 : si1
    ```
  }];
  // TODO(ingomueller): extend to other types.
  let arguments = (ins Substrait_ExpressionAttribute:$value);
  let results = (outs Substrait_ExpressionType:$result);
  let assemblyFormat = "$value attr-dict";
  let hasFolder = 1;
}

def Substrait_CallOp : Substrait_ExpressionOp<"call", [
    DeclareOpInterfaceMethods<SymbolUserOpInterface>,
    PredOpTrait<"'aggregation_phase' and 'aggregation_invocation' must either "
                "both be set or both not be set",
                CPred<[{ ::llvm::all_equal(
                              {$aggregation_phase.has_value(),
                               $aggregation_invocation.has_value()}) }]>
    >,
  ]> {
  let summary = "Function call expression";
  let description = [{
    Represents a `ScalarFunction` or `AggregateFunction` message (or, in the
    future, a `WindowFunction` message) together with all messages it contains
    and, where applicable, the `Expression` message it is contained in. Which of
    the message types this op corresponds to depends on the presence of the
    (otherwise optional) aggregate or window-related attributes.

    For aggregate functions, the aggregation phase is ommitted if it is set to
    `initial_to_result`; the invocation type is omitted from the custom assembly
    of it is set to `all`. If only `unspecified` is provided, the aggregation
    phase will be set to that value and the invocation type to `all`. The
    printer avoids that case by always printing the attribute value of the other
    if one of these two is `unspecified`.

    Currently, the specification of the function, which is in an external YAML
    file, is not taken into account, for example, to verify whether a matching
    overload exists or to verify/compute the result type.

    Example:

    ```mlir
    extension_uri @extension at "http://some.url/with/extensions.yml"
    extension_function @function at @extension["somefunc"]
    relation {
      // ...
      %1 = call @function(%0) : (tuple<si32>) -> si1
      // ...
    }
    ```
  }];
  // TODO(ingomueller): Add `FunctionOptions`.
  // TODO(ingomueller): Add support for `enum` and `type` argument types.
  let arguments = (ins
    FlatSymbolRefAttr:$callee,
    Variadic<Substrait_ExpressionType>:$args,
    OptionalAttr<AggregationPhase>:$aggregation_phase,
    OptionalAttr<AggregationInvocation>:$aggregation_invocation
  );
  let results = (outs Substrait_ExpressionType:$result);
  let assemblyFormat = [{
    $callee `(` $args `)`
    (`aggregate` ``
      custom<AggregationDetails>($aggregation_phase, $aggregation_invocation)^)?
    attr-dict `:` `(` type($args) `)` `->` type($result)
  }];
  let builders = [
      OpBuilder<(ins "::mlir::Type":$result,
                     "::mlir::FlatSymbolRefAttr":$callee,
                     "::mlir::ValueRange":$args), [{
        build($_builder, $_state, result, callee, args,
              AggregationPhaseAttr(),
              AggregationInvocationAttr());
      }]>,
      OpBuilder<(ins "::mlir::Type":$result, "::llvm::StringRef":$callee,
                     "::mlir::ValueRange":$args), [{
        build($_builder, $_state, result, callee, args,
              AggregationPhaseAttr(),
              AggregationInvocationAttr());
      }]>
    ];
  let extraClassDeclaration = [{
    // Helpers to distinguish function types.
    bool isAggregate() { return getAggregationInvocation().has_value(); }
    bool isScalar() { return !isAggregate() && !isWindow(); }
    bool isWindow() { return false; } // TODO: change once supported.
  }];
}

//===----------------------------------------------------------------------===//
// Relations
//===----------------------------------------------------------------------===//
// The definitions in this section are related to the various `Rel` message
// types. See https://substrait.io/relations/basics/ and
// https://github.com/substrait-io/substrait/blob/main/proto/substrait/algebra.proto.
//===----------------------------------------------------------------------===//

/// Attaches all traits that ops representing a `Rel` message type should have.
class Substrait_RelOp<string mnemonic, list<Trait> traits = []> :
  Substrait_Op<mnemonic, traits # [
    Substrait_RelOpInterface, Pure,
    PredOpTrait<"result must be extactly one Relation",
      And<[
        CPred<"$_op.getNumResults() == 1">,
        SubstLeaves<"$_self", "$_op.getResult(0).getType()",
          Substrait_Relation.predicate>
      ]>>
  ]>;

def Substrait_AggregateOp : Substrait_RelOp<"aggregate", [
    SingleBlockImplicitTerminator<"::mlir::substrait::YieldOp">,
    DeclareOpInterfaceMethods<OpAsmOpInterface, ["getDefaultDialect"]>,
    DeclareOpInterfaceMethods<Substrait_ExtensibleOpInterface>,
    DeclareOpInterfaceMethods<InferTypeOpInterface>,
  ]> {
  let summary = "Aggregate operation";
  let description = [{
    Represents an `AggregateRel ` message together with the `RelCommon` and the
    messages it contains. The `measures` field is represented as a region where
    the yielded values correspond to the `AggregateFunction`s (and thus have
    to be produced by a `CallOp` representing an aggregate function). Filters
    are currently not supported. The `groupings` field is represented as a
    region yielding the unique (deduplicated) grouping expressions and an array
    of array of references to these expressions representing the grouping sets.
    An empty array of grouping sets corresponds to *no* `groupings` messages;
    an array with an empty grouping set corresponds to an *empty* `groupings`
    messages. These two protobuf representations are different even though their
    semantic is equivalent. The op can only be exported to the protobuf format
    if the expressions yielded by the `groupings` region are all distinct after
    CSE. The assembly format omits an empty region of groupings, an empty region
    of measures, and the grouping sets attribute with one grouping set that
    consists of all values yielded from `groupings` (or the empty grouping set
    if that region is empty).

    Example:

    ```mlir
    %0 = ...
    // XXX: update example snippets, here and below
    %1 = aggregate %0 : tuple<si32> -> tuple<si32, si32>
      groupings {
      ^bb0(%arg : tuple<si32>):
        %2 = field_reference %arg[0] : tuple<si32>
        yield %2 : si32
      }
      grouping_sets [[0]]
      measures {
      ^bb0(%arg : tuple<si32>):
        %2 = field_reference %arg[0] : tuple<si32>
        %3 = call @function(%2) aggregate : (si32) -> si32
        yield %3 : si32
      }
    ```
  }];
  let arguments = (ins
    Substrait_Relation:$input,
    I64ArrayArrayAttr:$grouping_sets,
    OptionalAttr<Substrait_AdvancedExtensionAttr>:$advanced_extension
  );
  let results = (outs Substrait_Relation:$result);
  let regions = (region
    AnyRegion:$groupings,
    AnyRegion:$measures
  );
  let assemblyFormat = [{
    $input (`advanced_extension` `` $advanced_extension^)?
    attr-dict `:` type($input) `->` type($result)
    custom<AggregateRegions>($groupings, $measures, $grouping_sets)
  }];
  let hasRegionVerifier = 1;
  let builders = [
      OpBuilder<(ins
          "::mlir::Value":$input, "::mlir::ArrayAttr":$grouping_sets,
          "::mlir::Region *":$groupings, "::mlir::Region *":$measures
        )>,
    ];
  let extraClassDefinition = [{
    /// Implement OpAsmOpInterface.
    ::llvm::StringRef $cppClass::getDefaultDialect() {
      return SubstraitDialect::getDialectNamespace();
    }
  }];
}

def Substrait_CrossOp : Substrait_RelOp<"cross", [
    DeclareOpInterfaceMethods<InferTypeOpInterface>,
    DeclareOpInterfaceMethods<Substrait_ExtensibleOpInterface>,
  ]> {
  let summary = "Cross product operation";
  let description = [{
    Represents a `CrossRel` message together with the `RelCommon` and left and
    right `Rel` messages it contains.

    Example:

    ```mlir
    %0 = ...
    %1 = ...
    %2 = cross %0 x %1 : tuple<si32> x tuple<si32>
    ```
  }];
  let arguments = (ins
    Substrait_Relation:$left,
    Substrait_Relation:$right,
    OptionalAttr<Substrait_AdvancedExtensionAttr>:$advanced_extension
  );
  let results = (outs Substrait_Relation:$result);
  let assemblyFormat = [{
    $left `x` $right (`advanced_extension` `` $advanced_extension^)?
    attr-dict `:` type($left) `x` type($right)
  }];
  let builders = [
      OpBuilder<(ins "::mlir::Value":$left, "::mlir::Value":$right), [{
        build($_builder, $_state, left, right, /*advanced_extension=*/{});
      }]>,
    ];
}

def Substrait_EmitOp : Substrait_RelOp<"emit", [
    DeclareOpInterfaceMethods<OpAsmOpInterface, ["getDefaultDialect"]>,
    DeclareOpInterfaceMethods<InferTypeOpInterface>
  ]> {
  let summary = "Projection (a.k.a. 'emit') as dedicated operation";
  let description = [{
    Represents the `Emit` message of the `emit_kind` field in the `RelCommon`
    message. While projection is inlined into all relations in the protobuf
    format, this op separates out this functionality in a dedicated op in order
    to simplify rewriting.

    Example:

    ```mlir
    %0 = ...
    %1 = emit [2, 1] from %0 : tuple<si32, s1, si32> -> tuple<si32, si1>
    ```
  }];
  let arguments = (ins
    Substrait_Relation:$input,
    I64ArrayAttr:$mapping
  );
  let results = (outs Substrait_Relation:$result);
  let assemblyFormat = [{
    $mapping `from` $input attr-dict `:` type($input) `->` type($result)
  }];
  let hasFolder = 1;
  let extraClassDefinition = [{
    /// Implement OpAsmOpInterface.
    ::llvm::StringRef $cppClass::getDefaultDialect() {
      return SubstraitDialect::getDialectNamespace();
    }
  }];
}

def Substrait_ExtensionTableOp : Substrait_RelOp<"extension_table", [
    DeclareOpInterfaceMethods<Substrait_ExtensibleOpInterface>,
  ]> {
  let summary = "Extension table operation (i.e., a `ReadRel` case)";
  let description = [{
    Represents a `ExtensionTable` message together with the `ReadRel` and `Rel`
    messages that contain it.

    Example:

    ```mlir
    %0 = extension_table
           "\08*" : !substrait.any<"type.googleapis.com/google.protobuf.Int32Value">
           as ["a"] : tuple<si32>
    ```
  }];
  let arguments = (ins
    StringArrayAttr:$field_names,
    Substrait_AnyAttr:$detail,
    OptionalAttr<Substrait_AdvancedExtensionAttr>:$advanced_extension
  );
  let results = (outs Substrait_Relation:$result);
  let assemblyFormat = [{
    $detail `as` $field_names (`advanced_extension` `` $advanced_extension^)?
    attr-dict `:` type($result)
  }];
  let hasVerifier = true;
  let builders = [
      OpBuilder<(ins "::mlir::Type":$result, "::mlir::ArrayAttr":$field_names,
                     "::mlir::StringAttr":$detail), [{
        build($_builder, $_state, result, field_names, detail,
              /*advanced_extension=*/{});
      }]>,
    ];
}

def Substrait_FetchOp : Substrait_RelOp<"fetch", [
    DeclareOpInterfaceMethods<Substrait_ExtensibleOpInterface>,
    SameOperandsAndResultType,
  ]> {
  let summary = "Fetch operation";
  let description = [{
    Represents a `FetchOp` message together with the `RelCommon` and input
    `Rel` message it contains.
  }];
  let arguments = (ins
    Substrait_Relation:$input,
    DefaultValuedAttr<ConfinedAttr<I64Attr, [IntNonNegative]>, "0">: $offset,
    ConfinedAttr<I64Attr, [IntMinValue<-1>]>: $count,
    OptionalAttr<Substrait_AdvancedExtensionAttr>:$advanced_extension
  );
  let results = (outs Substrait_Relation:$result);
  let assemblyFormat = [{
    custom<CountAsAll>($count) (`offset` $offset^)? `from` $input
    (`advanced_extension` `` $advanced_extension^)? attr-dict `:` type($input)
  }];
  let builders = [
      OpBuilder<(ins "::mlir::Value":$input, "int64_t":$offset,
                      "int64_t":$count), [{
        build($_builder, $_state, input, offset, count,
              /*advanced_extension=*/{});
      }]>,
    ];
}

def Substrait_FilterOp : Substrait_RelOp<"filter", [
    SingleBlockImplicitTerminator<"::mlir::substrait::YieldOp">,
    DeclareOpInterfaceMethods<OpAsmOpInterface, ["getDefaultDialect"]>,
    DeclareOpInterfaceMethods<Substrait_ExtensibleOpInterface>,
    SameOperandsAndResultType
  ]> {
  let summary = "Filter operation";
  let description = [{
    Represents a `FilterRel` message together with the `RelCommon`, input `Rel`,
    and `Expression` messages it contains.

    Example:

    ```mlir
    %0 = ...
    %1 = filter %0 : tuple<si32> {
    ^bb0(%arg : tuple<si32>):
      %true = literal -1 : si1
      yield %true : si1
    }
    ```
  }];
  let arguments = (ins
    Substrait_Relation:$input,
    OptionalAttr<Substrait_AdvancedExtensionAttr>:$advanced_extension
  );
  let regions = (region AnyRegion:$condition);
  let results = (outs Substrait_Relation:$result);
  // TODO(ingomueller): We could elide/shorten the block argument from the
  //                    assembly by writing custom printers/parsers similar to
  //                    `scf.for` etc.
  let assemblyFormat = [{
    $input (`advanced_extension` `` $advanced_extension^)?
    attr-dict `:` type($input) $condition
  }];
  let hasRegionVerifier = 1;
  let extraClassDefinition = [{
    /// Implement OpAsmOpInterface.
    ::llvm::StringRef $cppClass::getDefaultDialect() {
      return SubstraitDialect::getDialectNamespace();
    }
  }];
}

def Substrait_JoinOp : Substrait_RelOp<"join", [
    DeclareOpInterfaceMethods<InferTypeOpInterface>,
    DeclareOpInterfaceMethods<Substrait_ExtensibleOpInterface>,
  ]> {
  let summary = "join operation";
  let description = [{
    Represents a `JoinRel` message together with the `RelCommon`, left and
    right `Rel` messages and `JoinType` enumeration it contains. The current
    implementation assumes the join expression to be True.
  }];
  //TODO(daliashaaban): Add support for join expressions.
  let arguments = (ins
    Substrait_Relation:$left,
    Substrait_Relation:$right,
    JoinType:$join_type,
    OptionalAttr<Substrait_AdvancedExtensionAttr>:$advanced_extension
  );
  let results = (outs Substrait_Relation:$result);
  let assemblyFormat = [{
    $join_type $left `,` $right (`advanced_extension` `` $advanced_extension^)?
    attr-dict `:` type($left) `,` type($right) `->` type($result)
  }];
  let builders = [
      OpBuilder<(ins "::mlir::Value":$left, "::mlir::Value":$right,
                     "JoinType":$join_type), [{
        build($_builder, $_state, left, right, join_type,
              /*advanced_extension=*/{});
      }]>,
    ];
}

def Substrait_NamedTableOp : Substrait_RelOp<"named_table", [
    DeclareOpInterfaceMethods<Substrait_ExtensibleOpInterface>,
  ]> {
  let summary = "Read operation of a named table";
  let description = [{
    Represents a `NamedTable` message together with the `ReadRel` and `Rel`
    messages that contain it.

    Example:

    ```mlir
    %0 = named_table @t1 as ["a"] : tuple<si32>
    ```
  }];
  // TODO(ingomueller): Maybe the result names should be part of a to-be-created
  //                    `NamedStruct` type?
  let arguments = (ins
    SymbolRefAttr:$table_name,
    StringArrayAttr:$field_names,
    OptionalAttr<Substrait_AdvancedExtensionAttr>:$advanced_extension
  );
  let results = (outs Substrait_Relation:$result);
  let assemblyFormat = [{
    $table_name `as` $field_names
    (`advanced_extension` `` $advanced_extension^)? attr-dict `:` type($result)
  }];
  let hasVerifier = true;
  let builders = [
      OpBuilder<(ins "::mlir::Type":$result_type,
                     "::mlir::SymbolRefAttr":$table_name,
                     "::mlir::ArrayAttr":$field_names), [{
        build($_builder, $_state, result_type, table_name, field_names,
              /*advanced_extension=*/{});
      }]>,
    ];
}

def Substrait_ProjectOp : Substrait_RelOp<"project", [
    SingleBlockImplicitTerminator<"::mlir::substrait::YieldOp">,
    DeclareOpInterfaceMethods<Substrait_ExtensibleOpInterface>,
    DeclareOpInterfaceMethods<OpAsmOpInterface, ["getDefaultDialect"]>
  ]> {
  let summary = "Project operation";
  let description = [{
    Represents a `ProjectRel` message together with the `RelCommon`, input
    `Rel`, and `Expression` messages it contains. While in protobuf the
    different `Expression` messages are distinct trees, the `project` op has
    a single `expression` region with one terminating `yield` and the values
    yielded by that terminator correspond to the expressions. Each individual
    `Expression` thus corresponds to the whole use-def tree of the corresponding
    yielded value.

    Example:

    ```mlir
    %0 = ...
    %1 = project %0 : tuple<si32> -> tuple<si32, si1, si32> {
    ^bb0(%arg : tuple<si32>):
      %true = literal -1 : si1
      %42 = literal 42 : si32
      yield %true, %42 : si1, si32
    }
    ```
  }];
  let arguments = (ins
    Substrait_Relation:$input,
    OptionalAttr<Substrait_AdvancedExtensionAttr>:$advanced_extension
  );
  let regions = (region AnyRegion:$expressions);
  let results = (outs Substrait_Relation:$result);
  // TODO(ingomueller): We could elide/shorten the block argument from the
  //                    assembly by writing custom printers/parsers similar to
  //                    `scf.for` etc.
  let assemblyFormat = [{
    $input (`advanced_extension` `` $advanced_extension^)?
    attr-dict `:` type($input) `->` type($result) $expressions
  }];
  let hasRegionVerifier = 1;
  let hasFolder = 1;
  let extraClassDefinition = [{
    /// Implement OpAsmOpInterface.
    ::llvm::StringRef $cppClass::getDefaultDialect() {
      return SubstraitDialect::getDialectNamespace();
    }
  }];
}

 //TODO(daliashaaban): Change type inference logic once nullability is supported.
def Substrait_SetOp : Substrait_RelOp<"set", [
    DeclareOpInterfaceMethods<Substrait_ExtensibleOpInterface>,
    SameOperandsAndResultType,
  ]> {
  let summary = "set operation";
  let description = [{
    Represents the `SetRel` message together with the `RelCommon`, input
    `Rel`, and `SetOp` enumeration it contains. This operation performs
    relational set operations such as intersection, union, and difference.
    There must be at least two inputs.
  }];

  let arguments = (ins
    Variadic<Substrait_Relation>:$inputs,
    SetOpKind:$kind,
    OptionalAttr<Substrait_AdvancedExtensionAttr>:$advanced_extension
  );

  let results = (outs Substrait_Relation:$result);
  let assemblyFormat = [{
    $kind $inputs (`advanced_extension` `` $advanced_extension^)?
    attr-dict `:` type($result)
  }];
  let builders = [
      OpBuilder<(ins "::mlir::ValueRange":$inputs, "SetOpKind":$kind), [{
        build($_builder, $_state, inputs, kind, /*advanced_extension=*/{});
      }]>,
    ];
}

#endif // SUBSTRAIT_DIALECT_SUBSTRAIT_IR_SUBSTRAITOPS
