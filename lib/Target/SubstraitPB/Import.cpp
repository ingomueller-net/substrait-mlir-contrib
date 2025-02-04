//===-- Import.cpp - Import protobuf to Substrait dialect -------*- C++ -*-===//
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "substrait-mlir/Target/SubstraitPB/Import.h"

#include "ProtobufUtils.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/ImplicitLocOpBuilder.h"
#include "mlir/IR/OwningOpRef.h"
#include "substrait-mlir/Dialect/Substrait/IR/Substrait.h"
#include "substrait-mlir/Target/SubstraitPB/Options.h"

// TODO(ingomueller): Find a way to make `substrait-cpp` declare these headers
// as system headers and remove the diagnostic fiddling here.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>
#include <substrait/proto/algebra.pb.h>
#include <substrait/proto/extensions/extensions.pb.h>
#include <substrait/proto/plan.pb.h>
#include <substrait/proto/type.pb.h>
#pragma clang diagnostic pop

using namespace mlir;
using namespace mlir::substrait;
using namespace mlir::substrait::protobuf_utils;
using namespace ::substrait;
using namespace ::substrait::proto;

namespace {

namespace pb = ::google::protobuf;

using ImportedNamedStruct = std::tuple<ArrayAttr, TupleType>;

// Copied from
// https://github.com/llvm/llvm-project/blob/dea33c/mlir/lib/Transforms/CSE.cpp.
struct SimpleOperationInfo : public llvm::DenseMapInfo<Operation *> {
  static unsigned getHashValue(const Operation *opC) {
    return OperationEquivalence::computeHash(
        const_cast<Operation *>(opC),
        /*hashOperands=*/OperationEquivalence::directHashValue,
        /*hashResults=*/OperationEquivalence::ignoreHashValue,
        OperationEquivalence::IgnoreLocations);
  }
  static bool isEqual(const Operation *lhsC, const Operation *rhsC) {
    auto *lhs = const_cast<Operation *>(lhsC);
    auto *rhs = const_cast<Operation *>(rhsC);
    if (lhs == rhs)
      return true;
    if (lhs == getTombstoneKey() || lhs == getEmptyKey() ||
        rhs == getTombstoneKey() || rhs == getEmptyKey())
      return false;
    return OperationEquivalence::isEquivalentTo(
        const_cast<Operation *>(lhsC), const_cast<Operation *>(rhsC),
        OperationEquivalence::IgnoreLocations);
  }
};

// Forward declaration for the import function of the given message type.
//
// We need one such function for most message types that we want to import. The
// forward declarations are necessary such all import functions are available
// for the definitions indepedently of the order of these definitions. The
// message type passed to the function (specified by `MESSAGE_TYPE`) may be
// different than the one it is responsible for: often the target op type
// (specified by `OP_TYPE`) depends on a nested field value (such as `oneof`)
// but the import logic needs the whole context; the message that is passed in
// is the most deeply nested message that provides the whole context.
#define DECLARE_IMPORT_FUNC(MESSAGE_TYPE, ARG_TYPE, OP_TYPE)                   \
  static FailureOr<OP_TYPE> import##MESSAGE_TYPE(ImplicitLocOpBuilder builder, \
                                                 const ARG_TYPE &message);

DECLARE_IMPORT_FUNC(AggregateFunction, AggregateFunction, CallOp)
DECLARE_IMPORT_FUNC(AggregateRel, Rel, AggregateOp)
DECLARE_IMPORT_FUNC(Any, pb::Any, StringAttr)
DECLARE_IMPORT_FUNC(Cast, Expression::Cast, CastOp)
DECLARE_IMPORT_FUNC(CrossRel, Rel, CrossOp)
DECLARE_IMPORT_FUNC(FetchRel, Rel, FetchOp)
DECLARE_IMPORT_FUNC(FilterRel, Rel, FilterOp)
DECLARE_IMPORT_FUNC(SetRel, Rel, SetOp)
DECLARE_IMPORT_FUNC(Expression, Expression, ExpressionOpInterface)
DECLARE_IMPORT_FUNC(ExtensionTable, Rel, ExtensionTableOp)
DECLARE_IMPORT_FUNC(FieldReference, Expression::FieldReference,
                    FieldReferenceOp)
DECLARE_IMPORT_FUNC(JoinRel, Rel, JoinOp)
DECLARE_IMPORT_FUNC(Literal, Expression::Literal, LiteralOp)
DECLARE_IMPORT_FUNC(NamedStruct, NamedStruct, ImportedNamedStruct)
DECLARE_IMPORT_FUNC(NamedTable, Rel, NamedTableOp)
DECLARE_IMPORT_FUNC(PlanRel, PlanRel, PlanRelOp)
DECLARE_IMPORT_FUNC(ProjectRel, Rel, ProjectOp)
DECLARE_IMPORT_FUNC(ReadRel, Rel, RelOpInterface)
DECLARE_IMPORT_FUNC(Rel, Rel, RelOpInterface)
DECLARE_IMPORT_FUNC(ScalarFunction, Expression::ScalarFunction, CallOp)
DECLARE_IMPORT_FUNC(TopLevel, Plan, PlanOp)
DECLARE_IMPORT_FUNC(TopLevel, PlanVersion, PlanVersionOp)

/// If present, imports the `advanced_extension` or `advanced_extensions` field
/// from the given message and sets the obtained attribute on the given op.
template <typename MessageType>
void importAdvancedExtension(ImplicitLocOpBuilder builder,
                             ExtensibleOpInterface op,
                             const MessageType &message);

template <typename MessageType>
static FailureOr<CallOp> importFunctionCommon(ImplicitLocOpBuilder builder,
                                              const MessageType &message);

template <typename MessageType>
void importAdvancedExtension(ImplicitLocOpBuilder builder,
                             ExtensibleOpInterface op,
                             const MessageType &message) {
  using Trait = advanced_extension_trait<MessageType>;
  if (!Trait::has_advanced_extension(message))
    return;

  // Get the `advanced_extension(s)` field.
  const extensions::AdvancedExtension &advancedExtension =
      Trait::advanced_extension(message);

  // Import `optimization` field if present.
  StringAttr optimizationAttr;
  if (advancedExtension.has_optimization()) {
    const pb::Any &optimization = advancedExtension.optimization();
    optimizationAttr = importAny(builder, optimization).value();
  }

  // Import `enhancement` field if present.
  StringAttr enhancementAttr;
  if (advancedExtension.has_enhancement()) {
    const pb::Any &enhancement = advancedExtension.enhancement();
    enhancementAttr = importAny(builder, enhancement).value();
  }

  // Build attribute and set it on the op.
  MLIRContext *context = builder.getContext();
  auto advancedExtensionAttr =
      AdvancedExtensionAttr::get(context, optimizationAttr, enhancementAttr);
  op.setAdvancedExtensionAttr(advancedExtensionAttr);
}

FailureOr<StringAttr> importAny(ImplicitLocOpBuilder builder,
                                const pb::Any &message) {
  MLIRContext *context = builder.getContext();
  auto typeUrlAttr = StringAttr::get(context, message.type_url());
  auto anyType = AnyType::get(context, typeUrlAttr);
  return StringAttr::get(message.value(), anyType);
}

// Helpers to build symbol names from anchors deterministically. This allows
// to reate symbol references from anchors without look-up structure. Also,
// the format is exploited by the export logic to recover the original anchor
// values of (unmodified) imported plans.

/// Builds a deterministic symbol name for an URI with the given anchor.
static std::string buildUriSymName(int32_t anchor) {
  return ("extension_uri." + Twine(anchor)).str();
}

/// Builds a deterministic symbol name for a function with the given anchor.
static std::string buildFuncSymName(int32_t anchor) {
  return ("extension_function." + Twine(anchor)).str();
}

/// Builds a deterministic symbol name for a type with the given anchor.
static std::string buildTypeSymName(int32_t anchor) {
  return ("extension_type." + Twine(anchor)).str();
}

/// Builds a deterministic symbol name for a type variation with the given
/// anchor.
static std::string buildTypeVarSymName(int32_t anchor) {
  return ("extension_type_variation." + Twine(anchor)).str();
}

static mlir::FailureOr<mlir::Type> importType(MLIRContext *context,
                                              const proto::Type &type) {

  proto::Type::KindCase kindCase = type.kind_case();
  switch (kindCase) {
  case proto::Type::kBool:
    return IntegerType::get(context, 1, IntegerType::Signed);
  case proto::Type::kI8:
    return IntegerType::get(context, 8, IntegerType::Signed);
  case proto::Type::kI16:
    return IntegerType::get(context, 16, IntegerType::Signed);
  case proto::Type::kI32:
    return IntegerType::get(context, 32, IntegerType::Signed);
  case proto::Type::kI64:
    return IntegerType::get(context, 64, IntegerType::Signed);
  case proto::Type::kFp32:
    return Float32Type::get(context);
  case proto::Type::kFp64:
    return Float64Type::get(context);
  case proto::Type::kString:
    return StringType::get(context);
  case proto::Type::kBinary:
    return BinaryType::get(context);
  case proto::Type::kTimestamp:
    return TimestampType::get(context);
  case proto::Type::kTimestampTz:
    return TimestampTzType::get(context);
  case proto::Type::kDate:
    return DateType::get(context);
  case proto::Type::kTime:
    return TimeType::get(context);
  case proto::Type::kIntervalYear:
    return IntervalYearMonthType::get(context);
  case proto::Type::kIntervalDay:
    return IntervalDaySecondType::get(context);
  case proto::Type::kUuid:
    return UUIDType::get(context);
  case proto::Type::kFixedChar:
    return FixedCharType::get(context, type.fixed_char().length());
  case proto::Type::kVarchar:
    return VarCharType::get(context, type.varchar().length());
  case proto::Type::kFixedBinary:
    return FixedBinaryType::get(context, type.fixed_binary().length());
  case proto::Type::kDecimal: {
    const proto::Type::Decimal &decimalType = type.decimal();
    return mlir::substrait::DecimalType::get(context, decimalType.precision(),
                                             decimalType.scale());
  }
  case proto::Type::kStruct: {
    const proto::Type::Struct &structType = type.struct_();
    llvm::SmallVector<mlir::Type> fieldTypes;
    fieldTypes.reserve(structType.types_size());
    for (const proto::Type &fieldType : structType.types()) {
      FailureOr<mlir::Type> mlirFieldType = importType(context, fieldType);
      if (failed(mlirFieldType))
        return failure();
      fieldTypes.push_back(mlirFieldType.value());
    }
    return TupleType::get(context, fieldTypes);
  }
    // TODO(ingomueller): Support more types.
  default: {
    auto loc = UnknownLoc::get(context);
    const pb::FieldDescriptor *desc =
        proto::Type::GetDescriptor()->FindFieldByNumber(kindCase);
    assert(desc && "could not get field descriptor");
    return emitError(loc) << "could not import unsupported type "
                          << desc->name();
  }
  }
}

mlir::FailureOr<CallOp>
importAggregateFunction(ImplicitLocOpBuilder builder,
                        const AggregateFunction &message) {
  using AggregationPhase = ::mlir::substrait::AggregationPhase;

  Location loc = builder.getLoc();

  FailureOr<CallOp> maybeCallOp = importFunctionCommon(builder, message);
  if (failed(maybeCallOp))
    return failure();
  CallOp callOp = maybeCallOp.value();

  // Import `phase` field.
  proto::AggregationPhase phase = message.phase();
  std::optional<AggregationPhase> phaseEnum = symbolizeAggregationPhase(phase);
  if (!phaseEnum.has_value())
    return emitError(loc) << "unsupported enum value for aggregate phase";
  callOp.setAggregationPhase(phaseEnum);

  // Import `invocation` field.
  AggregateFunction::AggregationInvocation invocation = message.invocation();
  std::optional<AggregationInvocation> invocationEnum =
      symbolizeAggregationInvocation(invocation);
  if (!invocationEnum.has_value())
    return emitError(loc)
           << "unsupported enum value for aggregate function invocation";
  callOp.setAggregationInvocation(invocationEnum);

  assert(callOp.isAggregate() && "expected to build aggregate function");
  return callOp;
}

static mlir::FailureOr<AggregateOp>
importAggregateRel(ImplicitLocOpBuilder builder, const Rel &message) {
  using Grouping = AggregateRel::Grouping;
  using Measure = AggregateRel::Measure;

  Location loc = builder.getLoc();

  const AggregateRel &aggregateRel = message.aggregate();

  // Import input.
  const Rel &inputRel = aggregateRel.input();
  mlir::FailureOr<RelOpInterface> inputOp = importRel(builder, inputRel);
  if (failed(inputOp))
    return failure();
  TypedValue<RelationType> inputVal = inputOp.value().getResult();
  TupleType inputTupleType = inputVal.getType().getStructType();

  // Import measures if any.
  auto measuresRegion = std::make_unique<Region>();
  if (aggregateRel.measures_size() > 0) {
    Block *measuresBlock = &measuresRegion->emplaceBlock();
    measuresBlock->addArgument(inputTupleType, loc);

    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPointToStart(measuresBlock);
    SmallVector<Value> measuresValues;
    measuresValues.reserve(aggregateRel.measures_size());
    for (const Measure &measure : aggregateRel.measures()) {
      const AggregateFunction &aggrFunc = measure.measure();

      // Import measure as `CallOp`.
      FailureOr<CallOp> callOp = importAggregateFunction(builder, aggrFunc);
      if (failed(callOp))
        return failure();

      measuresValues.push_back(callOp.value().getResult());
    }

    builder.create<YieldOp>(measuresValues);
  }

  // Import groupings if any.
  auto groupingsRegion = std::make_unique<Region>();
  SmallVector<Attribute> groupingSetsAttrs;
  if (aggregateRel.groupings_size() > 0) {
    Block *groupingsBlock = &groupingsRegion->emplaceBlock();
    groupingsBlock->addArgument(inputTupleType, loc);

    // Grouping expressions, i.e., values yielded from `groupings`.
    SmallVector<Value> groupingExprValues;
    groupingSetsAttrs.reserve(aggregateRel.groupings_size());

    // Ops that produce unique grouping expressions. In the protobuf messages,
    // each grouping set repeats the grouping expressions whereas the
    // `AggregateOp` yields unique grouping expressions from the `groupings`
    // region and represents the grouping sets as references to those.
    llvm::SmallDenseMap<Operation *, int64_t, 16, SimpleOperationInfo>
        groupingExprOps;

    // Import one grouping set at a time.
    for (const Grouping &grouping : aggregateRel.groupings()) {
      // Collect references of grouping expressions for this grouping set.
      SmallVector<int64_t> expressionRefs;
      expressionRefs.reserve(grouping.grouping_expressions_size());
      for (const Expression &expression : grouping.grouping_expressions()) {
        // Import expression message into `groupings` region.
        OpBuilder::InsertionGuard guard(builder);
        builder.setInsertionPointToStart(groupingsBlock);
        FailureOr<ExpressionOpInterface> exprOp =
            importExpression(builder, expression);
        if (failed(exprOp))
          return failure();

        // Create or look-up reference.
        auto [it, hasInserted] = groupingExprOps.try_emplace(exprOp.value());

        // If it's a new expression, assign new reference.
        if (hasInserted) {
          it->second = groupingExprOps.size() - 1;
          groupingExprValues.emplace_back(exprOp.value()->getResult(0));
        } else {
          // Otherwise, undo import by removing ops recursively.
          llvm::SmallVector<Operation *> worklist;
          worklist.push_back(exprOp.value());
          while (!worklist.empty()) {
            Operation *nextOp = worklist.pop_back_val();
            for (Value v : nextOp->getOperands()) {
              if (Operation *defOp = v.getDefiningOp())
                worklist.push_back(defOp);
            }
            nextOp->erase();
          }
        }

        // Remember reference for grouping set attribute.
        expressionRefs.push_back(it->second);
      }

      // Create `ArrayAttr` for current grouping set.
      ArrayAttr groupingSet = builder.getI64ArrayAttr(expressionRefs);
      groupingSetsAttrs.push_back(groupingSet);
    }

    // Assemble `YieldOp` of groupings region if there are grouping expressions.
    if (!groupingExprOps.empty()) {
      OpBuilder::InsertionGuard guard(builder);
      builder.setInsertionPointToEnd(groupingsBlock);
      builder.create<YieldOp>(loc, groupingExprValues);
    } else {
      // If there aren't any, we should clear the `groupings` region.
      groupingsRegion->getBlocks().clear();
    }
  }

  // Create attribute for grouping sets.
  auto groupingSets = ArrayAttr::get(builder.getContext(), groupingSetsAttrs);

  // Build `AggregateOp` and move regions into it.
  auto aggregateOp = builder.create<AggregateOp>(
      inputVal, groupingSets, groupingsRegion.get(), measuresRegion.get());

  // Import advanced extension if it is present.
  importAdvancedExtension(builder, aggregateOp, aggregateRel);

  return aggregateOp;
}

static mlir::FailureOr<CastOp> importCast(ImplicitLocOpBuilder builder,
                                          const Expression::Cast &message) {
  MLIRContext *context = builder.getContext();

  // Import `input` field.
  const Expression &input = message.input();
  FailureOr<ExpressionOpInterface> inputOp = importExpression(builder, input);
  if (failed(inputOp))
    return failure();

  // Import `type` field.
  const proto::Type &type = message.type();
  FailureOr<mlir::Type> mlirType = importType(context, type);
  if (failed(mlirType))
    return failure();

  // Import `failure_behavior` field.
  auto failureBehavior =
      static_cast<FailureBehavior>(message.failure_behavior());

  // Create `cast` op.
  Value inputVal = inputOp.value()->getResult(0);
  return builder.create<CastOp>(mlirType.value(), inputVal, failureBehavior);
}

static mlir::FailureOr<CrossOp> importCrossRel(ImplicitLocOpBuilder builder,
                                               const Rel &message) {
  const CrossRel &crossRel = message.cross();

  // Import left and right inputs.
  const Rel &leftRel = crossRel.left();
  const Rel &rightRel = crossRel.right();

  mlir::FailureOr<RelOpInterface> leftOp = importRel(builder, leftRel);
  mlir::FailureOr<RelOpInterface> rightOp = importRel(builder, rightRel);

  if (failed(leftOp) || failed(rightOp))
    return failure();

  // Build `CrossOp`.
  Value leftVal = leftOp.value().getResult();
  Value rightVal = rightOp.value().getResult();

  auto crossOp = builder.create<CrossOp>(leftVal, rightVal);

  // Import advanced extension if it is present.
  importAdvancedExtension(builder, crossOp, crossRel);

  return crossOp;
}

static mlir::FailureOr<SetOp> importSetRel(ImplicitLocOpBuilder builder,
                                           const Rel &message) {
  const SetRel &setRel = message.set();

  // Import inputs
  const google::protobuf::RepeatedPtrField<Rel> &inputsRel = setRel.inputs();

  // Build `SetOp`.
  llvm::SmallVector<Value> inputsVal;

  for (const Rel &inputRel : inputsRel) {
    mlir::FailureOr<RelOpInterface> inputOp = importRel(builder, inputRel);
    if (failed(inputOp))
      return failure();
    inputsVal.push_back(inputOp.value().getResult());
  }

  std::optional<SetOpKind> kind = static_cast<::SetOpKind>(setRel.op());

  // Check for unsupported set operations.
  if (!kind)
    return mlir::emitError(builder.getLoc(), "unexpected 'operation' found");

  auto setOp = builder.create<SetOp>(inputsVal, *kind);

  // Import advanced extension if it is present.
  importAdvancedExtension(builder, setOp, setRel);

  return setOp;
}

static mlir::FailureOr<ExpressionOpInterface>
importExpression(ImplicitLocOpBuilder builder, const Expression &message) {
  Location loc = builder.getLoc();

  Expression::RexTypeCase rex_type = message.rex_type_case();
  switch (rex_type) {
  case Expression::kCast:
    return importCast(builder, message.cast());
  case Expression::kLiteral:
    return importLiteral(builder, message.literal());
  case Expression::kSelection:
    return importFieldReference(builder, message.selection());
  case Expression::kScalarFunction:
    return importScalarFunction(builder, message.scalar_function());
  case Expression::REX_TYPE_NOT_SET:
    return emitError(loc) << Twine("expression type not set");
  default: {
    const pb::FieldDescriptor *desc =
        Expression::GetDescriptor()->FindFieldByNumber(rex_type);
    return emitError(loc) << Twine("unsupported expression type: ") +
                                 desc->name();
  }
  }
}

static mlir::FailureOr<ExtensionTableOp>
importExtensionTable(ImplicitLocOpBuilder builder, const Rel &message) {
  const ReadRel &readRel = message.read();
  const ReadRel::ExtensionTable &extensionTable = readRel.extension_table();

  // TODO(ingomueller): factor out common logic of `ReadRel`.
  // Import base schema and extract result names and types.
  const NamedStruct &baseSchema = readRel.base_schema();
  FailureOr<ImportedNamedStruct> importedNamedStruct =
      importNamedStruct(builder, baseSchema);
  if (failed(importedNamedStruct))
    return failure();
  auto [fieldNamesAttr, tupleType] = importedNamedStruct.value();

  // Import `detail` attribute.
  const pb::Any &detail = extensionTable.detail();
  auto detailAttr = importAny(builder, detail).value();

  // Assemble final op.
  auto resultType =
      RelationType::get(builder.getContext(), tupleType.getTypes());
  auto extensionTableOp =
      builder.create<ExtensionTableOp>(resultType, fieldNamesAttr, detailAttr);

  // Import advanced extension if it is present.
  importAdvancedExtension(builder, extensionTableOp, readRel);

  return extensionTableOp;
}

static mlir::FailureOr<FieldReferenceOp>
importFieldReference(ImplicitLocOpBuilder builder,
                     const Expression::FieldReference &message) {
  using ReferenceSegment = Expression::ReferenceSegment;

  Location loc = builder.getLoc();

  // Emit error on unsupported cases.
  // TODO(ingomueller): support more cases.
  if (!message.has_direct_reference())
    return emitError(loc) << "only direct reference supported";

  // Traverse list to extract indices.
  llvm::SmallVector<int64_t> indices;
  const ReferenceSegment *currentSegment = &message.direct_reference();
  while (true) {
    if (!currentSegment->has_struct_field())
      return emitError(loc) << "only struct fields supported";

    const ReferenceSegment::StructField &structField =
        currentSegment->struct_field();
    indices.push_back(structField.field());

    // Continue in linked list or end traversal.
    if (!structField.has_child())
      break;
    currentSegment = &structField.child();
  }

  // Get input value.
  Value container;
  if (message.has_root_reference()) {
    // For the `root_reference` case, that's the current block argument.
    mlir::Block::BlockArgListType blockArgs =
        builder.getInsertionBlock()->getArguments();
    assert(blockArgs.size() == 1 && "expected a single block argument");
    container = blockArgs.front();
  } else if (message.has_expression()) {
    // For the `expression` case, recursively import the expression.
    FailureOr<ExpressionOpInterface> maybeContainer =
        importExpression(builder, message.expression());
    if (failed(maybeContainer))
      return failure();
    container = maybeContainer.value()->getResult(0);
  } else {
    // For the `outer_reference` case, we need to refer to an argument of some
    // outer-level block.
    // TODO(ingomueller): support outer references.
    assert(message.has_outer_reference() && "unexpected 'root_type` case");
    return emitError(loc) << "outer references not supported";
  }

  // Build and return the op.
  return builder.create<FieldReferenceOp>(container, indices);
}

static mlir::FailureOr<JoinOp> importJoinRel(ImplicitLocOpBuilder builder,
                                             const Rel &message) {
  const JoinRel &joinRel = message.join();

  // Import left and right inputs.
  const Rel &leftRel = joinRel.left();
  const Rel &rightRel = joinRel.right();

  mlir::FailureOr<RelOpInterface> leftOp = importRel(builder, leftRel);
  mlir::FailureOr<RelOpInterface> rightOp = importRel(builder, rightRel);

  if (failed(leftOp) || failed(rightOp))
    return failure();

  // Build `JoinOp`.
  Value leftVal = leftOp.value().getResult();
  Value rightVal = rightOp.value().getResult();

  std::optional<JoinType> join_type = static_cast<JoinType>(joinRel.type());

  // Check for unsupported set operations.
  if (!join_type)
    return mlir::emitError(builder.getLoc(), "unexpected 'operation' found");

  auto joinOp = builder.create<JoinOp>(leftVal, rightVal, *join_type);

  // Import advanced extension if it is present.
  importAdvancedExtension(builder, joinOp, joinRel);

  return joinOp;
}

static mlir::FailureOr<LiteralOp>
importLiteral(ImplicitLocOpBuilder builder,
              const Expression::Literal &message) {
  MLIRContext *context = builder.getContext();
  Location loc = builder.getLoc();

  Expression::Literal::LiteralTypeCase literalType =
      message.literal_type_case();
  switch (literalType) {
  case Expression::Literal::LiteralTypeCase::kBoolean: {
    auto attr = IntegerAttr::get(
        IntegerType::get(context, 1, IntegerType::Signed), message.boolean());
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kI8: {
    auto attr = IntegerAttr::get(
        IntegerType::get(context, 8, IntegerType::Signed), message.i8());
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kI16: {
    auto attr = IntegerAttr::get(
        IntegerType::get(context, 16, IntegerType::Signed), message.i16());
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kI32: {
    auto attr = IntegerAttr::get(
        IntegerType::get(context, 32, IntegerType::Signed), message.i32());
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kI64: {
    auto attr = IntegerAttr::get(
        IntegerType::get(context, 64, IntegerType::Signed), message.i64());
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kFp32: {
    auto attr = FloatAttr::get(Float32Type::get(context), message.fp32());
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kFp64: {
    auto attr = FloatAttr::get(Float64Type::get(context), message.fp64());
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kString: {
    auto attr = StringAttr::get(message.string(), StringType::get(context));
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kBinary: {
    auto attr = StringAttr::get(message.binary(), BinaryType::get(context));
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kTimestamp: {
    auto attr = TimestampAttr::get(context, message.timestamp());
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kTimestampTz: {
    auto attr = TimestampTzAttr::get(context, message.timestamp_tz());
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kDate: {
    auto attr = DateAttr::get(context, message.date());
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kTime: {
    auto attr = TimeAttr::get(context, message.time());
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kIntervalYearToMonth: {
    auto attr = IntervalYearMonthAttr::get(
        context, message.interval_year_to_month().years(),
        message.interval_year_to_month().months());
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kIntervalDayToSecond: {
    auto attr = IntervalDaySecondAttr::get(
        context, message.interval_day_to_second().days(),
        message.interval_day_to_second().seconds());
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kUuid: {
    APInt var(128, 0);
    llvm::LoadIntFromMemory(
        var, reinterpret_cast<const uint8_t *>(message.uuid().data()), 16);
    IntegerAttr integer_attr =
        IntegerAttr::get(IntegerType::get(context, 128), var);
    auto attr = UUIDAttr::get(context, integer_attr);
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kFixedChar: {
    StringAttr stringAttr = StringAttr::get(context, message.fixed_char());
    FixedCharType fixedCharType =
        FixedCharType::get(context, message.fixed_char().size());
    auto attr = FixedCharAttr::get(context, stringAttr, fixedCharType);
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kVarChar: {
    StringAttr stringAttr =
        StringAttr::get(context, message.var_char().value());
    VarCharType varCharType =
        VarCharType::get(context, message.var_char().value().size());
    auto attr = VarCharAttr::get(context, stringAttr, varCharType);
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kFixedBinary: {
    StringAttr stringAttr = StringAttr::get(context, message.fixed_binary());
    FixedBinaryType fixedBinaryType =
        FixedBinaryType::get(context, message.fixed_binary().size());
    auto attr = FixedBinaryAttr::get(context, stringAttr, fixedBinaryType);
    return builder.create<LiteralOp>(attr);
  }
  case Expression::Literal::LiteralTypeCase::kDecimal: {
    APInt var(128, 0);
    llvm::LoadIntFromMemory(
        var,
        reinterpret_cast<const uint8_t *>(message.decimal().value().data()),
        16);
    DecimalType type = DecimalType::get(context, message.decimal().precision(),
                                        message.decimal().scale());
    IntegerAttr value = IntegerAttr::get(IntegerType::get(context, 128), var);
    auto attr = DecimalAttr::get(context, type, value);
    return builder.create<LiteralOp>(attr);
  }

  // TODO(ingomueller): Support more types.
  default: {
    const pb::FieldDescriptor *desc =
        Expression::Literal::GetDescriptor()->FindFieldByNumber(literalType);
    return emitError(loc) << Twine("unsupported Literal type: ") + desc->name();
  }
  }
}

static mlir::FailureOr<FetchOp> importFetchRel(ImplicitLocOpBuilder builder,
                                               const Rel &message) {
  const FetchRel &fetchRel = message.fetch();

  // Import input.
  const Rel &inputRel = fetchRel.input();
  mlir::FailureOr<RelOpInterface> inputOp = importRel(builder, inputRel);

  // Build `FetchOp`.
  Value inputVal = inputOp.value().getResult();
  auto fetchOp =
      builder.create<FetchOp>(inputVal, fetchRel.offset(), fetchRel.count());

  // Import advanced extension if it is present.
  importAdvancedExtension(builder, fetchOp, fetchRel);

  return fetchOp;
}

static mlir::FailureOr<FilterOp> importFilterRel(ImplicitLocOpBuilder builder,
                                                 const Rel &message) {
  const FilterRel &filterRel = message.filter();

  // Import input op.
  const Rel &inputRel = filterRel.input();
  mlir::FailureOr<RelOpInterface> inputOp = importRel(builder, inputRel);
  if (failed(inputOp))
    return failure();

  // Create filter op.
  auto filterOp = builder.create<FilterOp>(inputOp.value().getResult());
  filterOp.getCondition().push_back(new Block);
  Block &conditionBlock = filterOp.getCondition().front();
  RelationType inputType = filterOp.getResult().getType();
  conditionBlock.addArgument(inputType.getStructType(), filterOp->getLoc());

  // Create condition region.
  const Expression &expression = filterRel.condition();
  {
    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPointToEnd(&conditionBlock);

    FailureOr<ExpressionOpInterface> conditionOp =
        importExpression(builder, expression);
    if (failed(conditionOp))
      return failure();

    builder.create<YieldOp>(conditionOp.value()->getResult(0));
  }

  // Import advanced extension if it is present.
  importAdvancedExtension(builder, filterOp, filterRel);

  return filterOp;
}

static mlir::FailureOr<ImportedNamedStruct>
importNamedStruct(ImplicitLocOpBuilder builder, const NamedStruct &message) {
  MLIRContext *context = builder.getContext();

  // Assemble field names from schema.
  llvm::SmallVector<Attribute> fieldNames;
  fieldNames.reserve(message.names_size());
  for (const std::string &name : message.names()) {
    auto attr = StringAttr::get(context, name);
    fieldNames.push_back(attr);
  }
  auto fieldNamesAttr = ArrayAttr::get(context, fieldNames);

  // Assemble field types from schema.
  const proto::Type::Struct &struct_ = message.struct_();
  llvm::SmallVector<mlir::Type> resultTypes;
  resultTypes.reserve(struct_.types_size());
  for (const proto::Type &type : struct_.types()) {
    FailureOr<mlir::Type> mlirType = importType(context, type);
    if (failed(mlirType))
      return failure();
    resultTypes.push_back(mlirType.value());
  }
  auto resultType = TupleType::get(context, resultTypes);

  return ImportedNamedStruct{fieldNamesAttr, resultType};
}

static mlir::FailureOr<NamedTableOp>
importNamedTable(ImplicitLocOpBuilder builder, const Rel &message) {
  const ReadRel &readRel = message.read();
  const ReadRel::NamedTable &namedTable = readRel.named_table();
  MLIRContext *context = builder.getContext();

  // Assemble table name.
  llvm::SmallVector<FlatSymbolRefAttr> tableNameRefs;
  tableNameRefs.reserve(namedTable.names_size());
  for (const std::string &name : namedTable.names()) {
    auto attr = FlatSymbolRefAttr::get(context, name);
    tableNameRefs.push_back(attr);
  }
  llvm::ArrayRef<FlatSymbolRefAttr> tableNameNestedRefs =
      llvm::ArrayRef<FlatSymbolRefAttr>(tableNameRefs).drop_front();
  llvm::StringRef tableNameRootRef = tableNameRefs.front().getValue();
  auto tableName =
      SymbolRefAttr::get(context, tableNameRootRef, tableNameNestedRefs);

  // TODO(ingomueller): factor out common logic of `ReadRel`.
  // Import base schema and extract result names and types.
  const NamedStruct &baseSchema = readRel.base_schema();
  FailureOr<ImportedNamedStruct> importedNamedStruct =
      importNamedStruct(builder, baseSchema);
  if (failed(importedNamedStruct))
    return failure();
  auto [fieldNamesAttr, tupleType] = importedNamedStruct.value();

  // Assemble final op.
  auto resultType = RelationType::get(context, tupleType.getTypes());
  auto namedTableOp =
      builder.create<NamedTableOp>(resultType, tableName, fieldNamesAttr);

  // Import advanced extension if it is present.
  importAdvancedExtension(builder, namedTableOp, readRel);

  return namedTableOp;
}

static FailureOr<PlanOp> importTopLevel(ImplicitLocOpBuilder builder,
                                        const Plan &message) {
  using extensions::SimpleExtensionDeclaration;
  using extensions::SimpleExtensionURI;
  using ExtensionFunction = SimpleExtensionDeclaration::ExtensionFunction;
  using ExtensionType = SimpleExtensionDeclaration::ExtensionType;
  using ExtensionTypeVariation =
      SimpleExtensionDeclaration::ExtensionTypeVariation;

  MLIRContext *context = builder.getContext();
  Location loc = builder.getLoc();

  // Import version.
  const Version &version = message.version();

  // Build `PlanOp`.
  auto planOp = builder.create<PlanOp>(
      version.major_number(), version.minor_number(), version.patch_number(),
      version.git_hash(), version.producer());
  planOp.getBody().push_back(new Block());

  // Import `expected_type_urls` if present.
  SmallVector<Attribute> expected_type_urls;
  for (const std::string &expected_type_url : message.expected_type_urls()) {
    expected_type_urls.push_back(StringAttr::get(context, expected_type_url));
  }
  if (!expected_type_urls.empty()) {
    planOp.setExpectedTypeUrlsAttr(ArrayAttr::get(context, expected_type_urls));
  }

  // Import advanced extension if it is present.
  importAdvancedExtension(builder, planOp, message);

  OpBuilder::InsertionGuard insertGuard(builder);
  builder.setInsertionPointToEnd(&planOp.getBody().front());

  // Import `extension_uris` creating symbol names deterministically.
  for (const SimpleExtensionURI &extUri : message.extension_uris()) {
    int32_t anchor = extUri.extension_uri_anchor();
    StringRef uri = extUri.uri();
    std::string symName = buildUriSymName(anchor);
    builder.create<ExtensionUriOp>(symName, uri);
  }

  // Import `extension`s reconstructing symbol references to URI ops from the
  // corresponding anchors using the same method as above.
  for (const SimpleExtensionDeclaration &ext : message.extensions()) {
    SimpleExtensionDeclaration::MappingTypeCase mappingCase =
        ext.mapping_type_case();
    switch (mappingCase) {
    case SimpleExtensionDeclaration::kExtensionFunction: {
      const ExtensionFunction &func = ext.extension_function();
      int32_t anchor = func.function_anchor();
      int32_t uriRef = func.extension_uri_reference();
      const std::string &funcName = func.name();
      std::string symName = buildFuncSymName(anchor);
      std::string uriSymName = buildUriSymName(uriRef);
      builder.create<ExtensionFunctionOp>(symName, uriSymName, funcName);
      break;
    }
    case SimpleExtensionDeclaration::kExtensionType: {
      const ExtensionType &type = ext.extension_type();
      int32_t anchor = type.type_anchor();
      int32_t uriRef = type.extension_uri_reference();
      const std::string &typeName = type.name();
      std::string symName = buildTypeSymName(anchor);
      std::string uriSymName = buildUriSymName(uriRef);
      builder.create<ExtensionTypeOp>(symName, uriSymName, typeName);
      break;
    }
    case SimpleExtensionDeclaration::kExtensionTypeVariation: {
      const ExtensionTypeVariation &typeVar = ext.extension_type_variation();
      int32_t anchor = typeVar.type_variation_anchor();
      int32_t uriRef = typeVar.extension_uri_reference();
      const std::string &typeVarName = typeVar.name();
      std::string symName = buildTypeVarSymName(anchor);
      std::string uriSymName = buildUriSymName(uriRef);
      builder.create<ExtensionTypeVariationOp>(symName, uriSymName,
                                               typeVarName);
      break;
    }
    default:
      const pb::FieldDescriptor *desc =
          SimpleExtensionDeclaration::GetDescriptor()->FindFieldByNumber(
              mappingCase);
      return emitError(loc)
             << Twine("unsupported SimpleExtensionDeclaration type: ") +
                    desc->name();
    }
  }

  for (const PlanRel &relation : message.relations()) {
    if (failed(importPlanRel(builder, relation)))
      return failure();
  }

  return planOp;
}

static FailureOr<PlanRelOp> importPlanRel(ImplicitLocOpBuilder builder,
                                          const PlanRel &message) {
  Location loc = builder.getLoc();

  if (!message.has_rel() && !message.has_root()) {
    PlanRel::RelTypeCase relType = message.rel_type_case();
    const pb::FieldDescriptor *desc =
        PlanRel::GetDescriptor()->FindFieldByNumber(relType);
    return emitError(loc) << Twine("unsupported PlanRel type: ") + desc->name();
  }

  // Create new `PlanRelOp`.
  auto planRelOp = builder.create<PlanRelOp>();
  planRelOp.getBody().push_back(new Block());
  Block *block = &planRelOp.getBody().front();

  // Handle `Rel` and `RelRoot` separately.
  const Rel *rel;
  if (message.has_rel())
    rel = &message.rel();
  else {
    const RelRoot &root = message.root();
    rel = &root.input();

    // Extract names.
    SmallVector<std::string> names(root.names().begin(), root.names().end());
    SmallVector<llvm::StringRef> nameAttrs(names.begin(), names.end());
    ArrayAttr namesAttr = builder.getStrArrayAttr(nameAttrs);
    planRelOp.setFieldNamesAttr(namesAttr);
  }

  // Import body of `PlanRelOp`.
  OpBuilder::InsertionGuard insertGuard(builder);
  builder.setInsertionPointToEnd(block);
  mlir::FailureOr<RelOpInterface> rootRel = importRel(builder, *rel);
  if (failed(rootRel))
    return failure();

  builder.setInsertionPointToEnd(block);
  builder.create<YieldOp>(rootRel.value().getResult());

  return planRelOp;
}

static FailureOr<PlanVersionOp> importTopLevel(ImplicitLocOpBuilder builder,
                                               const PlanVersion &message) {
  const Version &version = message.version();
  auto versionAttr = VersionAttr::get(
      builder.getContext(), version.major_number(), version.minor_number(),
      version.patch_number(), version.git_hash(), version.producer());
  return builder.create<PlanVersionOp>(versionAttr);
}

static mlir::FailureOr<ProjectOp> importProjectRel(ImplicitLocOpBuilder builder,
                                                   const Rel &message) {
  const ProjectRel &projectRel = message.project();

  // Import input op.
  const Rel &inputRel = projectRel.input();
  mlir::FailureOr<RelOpInterface> inputOp = importRel(builder, inputRel);
  if (failed(inputOp))
    return failure();

  // Create `expressions` block.
  auto conditionBlock = std::make_unique<Block>();
  RelationType inputType = inputOp.value().getResult().getType();
  conditionBlock->addArgument(inputType.getStructType(), inputOp->getLoc());

  // Fill `expressions` block with expression trees.
  YieldOp yieldOp;
  {
    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPointToEnd(conditionBlock.get());

    SmallVector<Value> values;
    values.reserve(projectRel.expressions_size());
    for (const Expression &expression : projectRel.expressions()) {
      // Import expression tree recursively.
      FailureOr<ExpressionOpInterface> rootExprOp =
          importExpression(builder, expression);
      if (failed(rootExprOp))
        return failure();
      values.push_back(rootExprOp.value()->getResult(0));
    }

    // Create final `yield` op with root expression values.
    yieldOp = builder.create<YieldOp>(values);
  }

  // Compute output type.
  SmallVector<mlir::Type> resultFieldTypes;
  resultFieldTypes.reserve(inputType.size() + yieldOp->getNumOperands());
  append_range(resultFieldTypes, inputType);
  append_range(resultFieldTypes, yieldOp->getOperandTypes());
  auto resultType = RelationType::get(builder.getContext(), resultFieldTypes);

  // Create `project` op.
  auto projectOp =
      builder.create<ProjectOp>(resultType, inputOp.value().getResult());
  projectOp.getExpressions().push_back(conditionBlock.release());

  // Import advanced extension if it is present.
  importAdvancedExtension(builder, projectOp, projectRel);

  return projectOp;
}

static mlir::FailureOr<RelOpInterface>
importReadRel(ImplicitLocOpBuilder builder, const Rel &message) {
  Location loc = builder.getLoc();

  const ReadRel &readRel = message.read();
  ReadRel::ReadTypeCase readType = readRel.read_type_case();
  switch (readType) {
  case ReadRel::ReadTypeCase::kExtensionTable: {
    return importExtensionTable(builder, message);
  }
  case ReadRel::ReadTypeCase::kNamedTable: {
    return importNamedTable(builder, message);
  }
  default:
    const pb::FieldDescriptor *desc =
        ReadRel::GetDescriptor()->FindFieldByNumber(readType);
    return emitError(loc) << Twine("unsupported ReadRel type: ") + desc->name();
  }
}

static mlir::FailureOr<RelOpInterface> importRel(ImplicitLocOpBuilder builder,
                                                 const Rel &message) {
  Location loc = builder.getLoc();

  // Import rel depending on its type.
  Rel::RelTypeCase relType = message.rel_type_case();
  FailureOr<RelOpInterface> maybeOp;
  switch (relType) {
  case Rel::RelTypeCase::kAggregate:
    maybeOp = importAggregateRel(builder, message);
    break;
  case Rel::RelTypeCase::kCross:
    maybeOp = importCrossRel(builder, message);
    break;
  case Rel::RelTypeCase::kFetch:
    maybeOp = importFetchRel(builder, message);
    break;
  case Rel::RelTypeCase::kFilter:
    maybeOp = importFilterRel(builder, message);
    break;
  case Rel::RelTypeCase::kJoin:
    maybeOp = importJoinRel(builder, message);
    break;
  case Rel::RelTypeCase::kProject:
    maybeOp = importProjectRel(builder, message);
    break;
  case Rel::RelTypeCase::kRead:
    maybeOp = importReadRel(builder, message);
    break;
  case Rel::RelTypeCase::kSet:
    maybeOp = importSetRel(builder, message);
    break;
  default:
    const pb::FieldDescriptor *desc =
        Rel::GetDescriptor()->FindFieldByNumber(relType);
    return emitError(loc) << Twine("unsupported Rel type: ") + desc->name();
  }
  if (failed(maybeOp))
    return failure();
  RelOpInterface op = maybeOp.value();

  // Remainder: Import `emit` op if needed.

  // Extract `RelCommon` message.
  FailureOr<const RelCommon *> maybeRelCommon =
      protobuf_utils::getCommon(message, loc);
  if (failed(maybeRelCommon))
    return failure();
  const RelCommon *relCommon = maybeRelCommon.value();

  // For the `direct` case, no further op needs to be created.
  if (relCommon->has_direct())
    return op;
  assert(relCommon->has_emit() && "expected either 'direct' or 'emit'");

  // For the `emit` case, we need to insert an `EmitOp`.
  const proto::RelCommon::Emit &emit = relCommon->emit();
  SmallVector<int64_t> mapping;
  append_range(mapping, emit.output_mapping());
  ArrayAttr mappingAttr = builder.getI64ArrayAttr(mapping);
  auto emitOp = builder.create<EmitOp>(op.getResult(), mappingAttr);

  return {emitOp};
}

static mlir::FailureOr<CallOp>
importScalarFunction(ImplicitLocOpBuilder builder,
                     const Expression::ScalarFunction &message) {
  FailureOr<CallOp> callOp = importFunctionCommon(builder, message);
  assert((failed(callOp) || callOp->isScalar()) &&
         "expected to build scalar function");
  return callOp;
}

template <typename MessageType>
FailureOr<CallOp> importFunctionCommon(ImplicitLocOpBuilder builder,
                                       const MessageType &message) {
  MLIRContext *context = builder.getContext();
  Location loc = builder.getLoc();

  // Import `output_type`.
  const proto::Type &outputType = message.output_type();
  FailureOr<mlir::Type> mlirOutputType = importType(context, outputType);
  if (failed(mlirOutputType))
    return failure();

  // Import `arguments`.
  SmallVector<Value> operands;
  for (const FunctionArgument &arg : message.arguments()) {
    // Error out on unsupported cases.
    // TODO(ingomueller): Support other function argument types.
    if (!arg.has_value()) {
      const pb::FieldDescriptor *desc =
          FunctionArgument::GetDescriptor()->FindFieldByNumber(
              arg.arg_type_case());
      return emitError(loc) << Twine("unsupported arg type: ") + desc->name();
    }

    // Handle `value` case.
    const Expression &value = arg.value();
    FailureOr<ExpressionOpInterface> expression =
        importExpression(builder, value);
    if (failed(expression))
      return failure();
    operands.push_back((*expression)->getResult(0));
  }

  // Import `function_reference` field.
  int32_t anchor = message.function_reference();
  std::string calleeSymName = buildFuncSymName(anchor);

  // Create op.
  auto callOp =
      builder.create<CallOp>(mlirOutputType.value(), calleeSymName, operands);

  return {callOp};
}

template <typename MessageType>
OwningOpRef<ModuleOp> translateProtobufToSubstraitTopLevel(
    llvm::StringRef input, MLIRContext *context, ImportExportOptions options,
    MessageType &message) {
  Location loc = UnknownLoc::get(context);

  // Parse from serialized form into desired protobuf `MessageType`.
  switch (options.serdeFormat) {
  case SerdeFormat::kText:
    if (!pb::TextFormat::ParseFromString(input.str(), &message)) {
      emitError(loc) << "could not parse string as '" << message.GetTypeName()
                     << "' message.";
      return {};
    }
    break;
  case SerdeFormat::kBinary:
    if (!message.ParseFromString(input.str())) {
      emitError(loc) << "could not deserialize input as '"
                     << message.GetTypeName() << "' message.";
      return {};
    }
    break;
  case SerdeFormat::kJson:
  case SerdeFormat::kPrettyJson: {
    absl::Status status = pb::util::JsonStringToMessage(input.str(), &message);
    if (!status.ok()) {
      emitError(loc) << "could not deserialize JSON as '"
                     << message.GetTypeName() << "' message:\n"
                     << status.message();
      return {};
    }
  }
  }

  // Set up infra for importing.
  context->loadDialect<SubstraitDialect>();

  ImplicitLocOpBuilder builder(loc, context);
  auto module = builder.create<ModuleOp>(loc);
  auto moduleRef = OwningOpRef<ModuleOp>(module);
  builder.setInsertionPointToEnd(&module.getBodyRegion().back());

  // Import protobuf message into corresponding MLIR op.
  if (failed(importTopLevel(builder, message)))
    return {};

  return moduleRef;
}

} // namespace

OwningOpRef<ModuleOp> mlir::substrait::translateProtobufToSubstraitPlan(
    llvm::StringRef input, MLIRContext *context, ImportExportOptions options) {

  Plan plan;
  return translateProtobufToSubstraitTopLevel(input, context, options, plan);
}

OwningOpRef<ModuleOp> mlir::substrait::translateProtobufToSubstraitPlanVersion(
    llvm::StringRef input, MLIRContext *context, ImportExportOptions options) {
  PlanVersion planVersion;
  return translateProtobufToSubstraitTopLevel(input, context, options,
                                              planVersion);
}
