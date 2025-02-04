//===-- Export.cpp - Export Substrait dialect to protobuf -------*- C++ -*-===//
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "substrait-mlir/Target/SubstraitPB/Export.h"
#include "ProtobufUtils.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Support/LogicalResult.h"
#include "mlir/Transforms/CSE.h"
#include "substrait-mlir/Dialect/Substrait/IR/Substrait.h"
#include "substrait-mlir/Target/SubstraitPB/Options.h"
#include "llvm/ADT/TypeSwitch.h"
#include "llvm/Support/ErrorHandling.h"

// TODO(ingomueller): Find a way to make `substrait-cpp` declare these headers
// as system headers and remove the diagnostic fiddling here.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
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

/// Main structure to drive export from the dialect to protobuf. This class
/// holds the visitor functions for the various ops etc. from the dialect as
/// well as state and utilities around the state that is built up during export.
class SubstraitExporter {
public:
// Declaration for the export function of the given operation type.
//
// We need one such function for most op type that we want to export. The
// `MESSAGE_TYPE` argument corresponds to the protobuf message type returned
// by the function.
#define DECLARE_EXPORT_FUNC(OP_TYPE, MESSAGE_TYPE)                             \
  FailureOr<std::unique_ptr<MESSAGE_TYPE>> exportOperation(OP_TYPE op);

  DECLARE_EXPORT_FUNC(AggregateOp, Rel)
  DECLARE_EXPORT_FUNC(CallOp, Expression)
  DECLARE_EXPORT_FUNC(CastOp, Expression)
  DECLARE_EXPORT_FUNC(CrossOp, Rel)
  DECLARE_EXPORT_FUNC(EmitOp, Rel)
  DECLARE_EXPORT_FUNC(ExpressionOpInterface, Expression)
  DECLARE_EXPORT_FUNC(ExtensionTableOp, Rel)
  DECLARE_EXPORT_FUNC(FieldReferenceOp, Expression)
  DECLARE_EXPORT_FUNC(FetchOp, Rel)
  DECLARE_EXPORT_FUNC(FilterOp, Rel)
  DECLARE_EXPORT_FUNC(JoinOp, Rel)
  DECLARE_EXPORT_FUNC(LiteralOp, Expression)
  DECLARE_EXPORT_FUNC(ModuleOp, pb::Message)
  DECLARE_EXPORT_FUNC(NamedTableOp, Rel)
  DECLARE_EXPORT_FUNC(PlanOp, Plan)
  DECLARE_EXPORT_FUNC(PlanVersionOp, PlanVersion)
  DECLARE_EXPORT_FUNC(ProjectOp, Rel)
  DECLARE_EXPORT_FUNC(RelOpInterface, Rel)
  DECLARE_EXPORT_FUNC(SetOp, Rel)

  template <typename MessageType>
  void exportAdvancedExtension(ExtensibleOpInterface op, MessageType &message);

  // Common export logic for aggregate, scalar, and window functions.
  template <typename MessageType>
  FailureOr<std::unique_ptr<MessageType>> exportCallOpCommon(CallOp op);

  // Special handling for aggregate, scalar, and window functions, which have
  // the same argument types but different return types.
  FailureOr<std::unique_ptr<AggregateFunction>>
  exportCallOpAggregate(CallOp op);
  FailureOr<std::unique_ptr<Expression>> exportCallOpScalar(CallOp op);
  FailureOr<std::unique_ptr<Expression>> exportCallOpWindow(CallOp op);

  std::unique_ptr<pb::Any> exportAny(StringAttr attr);
  FailureOr<std::unique_ptr<NamedStruct>>
  exportNamedStruct(Location loc, ArrayAttr fieldNames, TupleType tupleType);
  FailureOr<std::unique_ptr<pb::Message>> exportOperation(Operation *op);
  FailureOr<std::unique_ptr<proto::Type>> exportType(Location loc,
                                                     mlir::Type mlirType);

private:
  /// Returns the nearest symbol table to op. The symbol table is cached in
  /// `this` such that repeated calls that request the same symbol do not
  /// rebuild that table.
  SymbolTable &getSymbolTableFor(Operation *op) {
    Operation *nearestSymbolTableOp = SymbolTable::getNearestSymbolTable(op);
    if (!symbolTable || symbolTable->getOp() != nearestSymbolTableOp) {
      symbolTable = std::make_unique<SymbolTable>(nearestSymbolTableOp);
    }
    return *symbolTable;
  }

  /// Looks up the anchor value corresponding to the given symbol name in the
  /// context of the given op. The op is used to determine which symbol table
  /// was used to assign anchors.
  template <typename SymNameType>
  int32_t lookupAnchor(Operation *contextOp, const SymNameType &symName) {
    SymbolTable &symbolTable = getSymbolTableFor(contextOp);
    Operation *calleeOp = symbolTable.lookup(symName);
    return anchorsByOp.at(calleeOp);
  }

  DenseMap<Operation *, int32_t> anchorsByOp{}; // Maps anchors to ops.
  std::unique_ptr<SymbolTable> symbolTable;     // Symbol table cache.
};

template <typename MessageType>
void SubstraitExporter::exportAdvancedExtension(ExtensibleOpInterface op,
                                                MessageType &message) {
  if (!op.getAdvancedExtension())
    return;

  // Build the base `AdvancedExtension` message.
  AdvancedExtensionAttr extensionAttr = op.getAdvancedExtension().value();
  auto extension = std::make_unique<extensions::AdvancedExtension>();

  StringAttr optimizationAttr = extensionAttr.getOptimization();
  StringAttr enhancementAttr = extensionAttr.getEnhancement();

  // Set `optimization` field if present.
  if (optimizationAttr) {
    std::unique_ptr<pb::Any> optimization = exportAny(optimizationAttr);
    extension->set_allocated_optimization(optimization.release());
  }

  // Set `enhancement` field if present.
  if (enhancementAttr) {
    std::unique_ptr<pb::Any> enhancement = exportAny(enhancementAttr);
    extension->set_allocated_enhancement(enhancement.release());
  }

  // Set the `advanced_extension` field in the provided message.
  using Trait = advanced_extension_trait<MessageType>;
  Trait::set_allocated_advanced_extension(message, extension.release());
}

std::unique_ptr<pb::Any> SubstraitExporter::exportAny(StringAttr attr) {
  auto any = std::make_unique<pb::Any>();
  auto anyType = mlir::cast<AnyType>(attr.getType());
  std::string typeUrl = anyType.getTypeUrl().getValue().str();
  std::string value = attr.getValue().str();
  any->set_type_url(typeUrl);
  any->set_value(value);
  return any;
}

/// Function that export `IntegerType`'s to the corresponding Substrait types.
std::unique_ptr<proto::Type> exportIntegerType(IntegerType intType,
                                               MLIRContext *context) {
  assert(intType.isSigned() && "only signed integer types supported");

  switch (intType.getWidth()) {
  case 1: { // Handle SI1.
    // TODO(ingomueller): support other nullability modes.
    auto i1Type = std::make_unique<proto::Type::Boolean>();
    i1Type->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);

    auto type = std::make_unique<proto::Type>();
    type->set_allocated_bool_(i1Type.release());
    return type;
  }

  case 8: { // Handle SI8.
    // TODO(ingomueller): support other nullability modes.
    auto i8Type = std::make_unique<proto::Type::I8>();
    i8Type->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);

    auto type = std::make_unique<proto::Type>();
    type->set_allocated_i8(i8Type.release());
    return type;
  }

  case 16: { // Handle SI16.
    // TODO(ingomueller): support other nullability modes.
    auto i16Type = std::make_unique<proto::Type::I16>();
    i16Type->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);

    auto type = std::make_unique<proto::Type>();
    type->set_allocated_i16(i16Type.release());
    return type;
  }

  case 32: { // Handle SI32.
    // TODO(ingomueller): support other nullability modes.
    auto i32Type = std::make_unique<proto::Type::I32>();
    i32Type->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);

    auto type = std::make_unique<proto::Type>();
    type->set_allocated_i32(i32Type.release());
    return type;
  }

  case 64: { // Handle SI64.
    // TODO(ingomueller): support other nullability modes.
    auto i64Type = std::make_unique<proto::Type::I64>();
    i64Type->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);

    auto type = std::make_unique<proto::Type>();
    type->set_allocated_i64(i64Type.release());
    return type;
  }

  default:
    llvm_unreachable("We should have handled all integer types.");
  }
}

/// Function that export `FloatType`'s to the corresponding Substrait types.
std::unique_ptr<proto::Type> exportFloatType(FloatType floatType,
                                             MLIRContext *context) {

  switch (floatType.getWidth()) {
  case 32: { // Handle FP32.
    // TODO(ingomueller): support other nullability modes.
    auto fp32Type = std::make_unique<proto::Type::FP32>();
    fp32Type->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);

    auto type = std::make_unique<proto::Type>();
    type->set_allocated_fp32(fp32Type.release());
    return type;
  }

  case 64: { // Handle FP64.
    // TODO(ingomueller): support other nullability modes.
    auto fp64Type = std::make_unique<proto::Type::FP64>();
    fp64Type->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);

    auto type = std::make_unique<proto::Type>();
    type->set_allocated_fp64(fp64Type.release());
    return type;
  }

  default:
    llvm_unreachable("We should have handled all float types.");
  }
}

FailureOr<std::unique_ptr<proto::Type>>
SubstraitExporter::exportType(Location loc, mlir::Type mlirType) {
  MLIRContext *context = mlirType.getContext();

  // Handle `IntegerType`'s.
  if (auto intType = mlir::dyn_cast<IntegerType>(mlirType)) {
    return exportIntegerType(intType, context);
  }

  // Handle `FloatType`'s.
  if (auto floatType = mlir::dyn_cast<FloatType>(mlirType)) {
    return exportFloatType(floatType, context);
  }

  // Handle String.
  if (mlir::isa<StringType>(mlirType)) {
    // TODO(ingomueller): support other nullability modes.
    auto stringType = std::make_unique<proto::Type::String>();
    stringType->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);

    auto type = std::make_unique<proto::Type>();
    type->set_allocated_string(stringType.release());
    return std::move(type);
  }

  // Handle binary type.
  if (mlir::isa<BinaryType>(mlirType)) {
    // TODO(ingomueller): support other nullability modes.
    auto binaryType = std::make_unique<proto::Type::Binary>();
    binaryType->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);

    auto type = std::make_unique<proto::Type>();
    type->set_allocated_binary(binaryType.release());
    return std::move(type);
  }

  // Handle timestamp.
  if (mlir::isa<TimestampType>(mlirType)) {
    // TODO(ingomueller): support other nullability modes.
    auto timestampType = std::make_unique<proto::Type::Timestamp>();
    timestampType->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);

    auto type = std::make_unique<proto::Type>();
    type->set_allocated_timestamp(timestampType.release());
    return std::move(type);
  }

  // Handle timestamp_tz.
  if (mlir::isa<TimestampTzType>(mlirType)) {
    // TODO(ingomueller): support other nullability modes.
    auto timestampTzType = std::make_unique<proto::Type::TimestampTZ>();
    timestampTzType->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);

    auto type = std::make_unique<proto::Type>();
    type->set_allocated_timestamp_tz(timestampTzType.release());
    return std::move(type);
  }

  // Handle date.
  if (mlir::isa<DateType>(mlirType)) {
    // TODO(ingomueller): support other nullability modes.
    auto dateType = std::make_unique<proto::Type::Date>();
    dateType->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);

    auto type = std::make_unique<proto::Type>();
    type->set_allocated_date(dateType.release());
    return std::move(type);
  }

  // Handle time.
  if (mlir::isa<TimeType>(mlirType)) {
    // TODO(ingomueller): support other nullability modes.
    auto timeType = std::make_unique<proto::Type::Time>();
    timeType->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);

    auto type = std::make_unique<proto::Type>();
    type->set_allocated_time(timeType.release());
    return std::move(type);
  }

  // Handle interval_year.
  if (mlir::isa<IntervalYearMonthType>(mlirType)) {
    // TODO(ingomueller): support other nullability modes.
    auto intervalYearType = std::make_unique<proto::Type::IntervalYear>();
    intervalYearType->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);

    auto type = std::make_unique<proto::Type>();
    type->set_allocated_interval_year(intervalYearType.release());
    return std::move(type);
  }
  // Handle interval_day.
  if (mlir::isa<IntervalDaySecondType>(mlirType)) {
    // TODO(ingomueller): support other nullability modes.
    auto intervalDayType = std::make_unique<proto::Type::IntervalDay>();
    intervalDayType->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);

    auto type = std::make_unique<proto::Type>();
    type->set_allocated_interval_day(intervalDayType.release());
    return std::move(type);
  }

  // Handle uuid.
  if (mlir::isa<UUIDType>(mlirType)) {
    // TODO(ingomueller): support other nullability modes.
    auto uuidType = std::make_unique<proto::Type::UUID>();
    uuidType->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);

    auto type = std::make_unique<proto::Type>();
    type->set_allocated_uuid(uuidType.release());
    return std::move(type);
  }

  // Handle fixed char.
  if (mlir::isa<FixedCharType>(mlirType)) {
    // TODO(ingomueller): support other nullability modes.
    auto fixedCharType = std::make_unique<proto::Type::FixedChar>();
    fixedCharType->set_length(mlir::cast<FixedCharType>(mlirType).getLength());
    fixedCharType->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);
    auto type = std::make_unique<proto::Type>();
    type->set_allocated_fixed_char(fixedCharType.release());
    return std::move(type);
  }

  // Handle varchar.
  if (mlir::isa<VarCharType>(mlirType)) {
    // TODO(ingomueller): support other nullability modes.
    auto varCharType = std::make_unique<proto::Type::VarChar>();
    varCharType->set_length(mlir::cast<VarCharType>(mlirType).getLength());
    varCharType->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);
    auto type = std::make_unique<proto::Type>();
    type->set_allocated_varchar(varCharType.release());
    return std::move(type);
  }

  // Handle fixed binary.
  if (mlir::isa<FixedBinaryType>(mlirType)) {
    // TODO(ingomueller): support other nullability modes.
    auto fixedBinaryType = std::make_unique<proto::Type::FixedBinary>();
    fixedBinaryType->set_length(
        mlir::cast<FixedBinaryType>(mlirType).getLength());
    fixedBinaryType->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);
    auto type = std::make_unique<proto::Type>();
    type->set_allocated_fixed_binary(fixedBinaryType.release());
    return std::move(type);
  }

  // Handle decimal.
  if (auto decimalType = llvm::dyn_cast<DecimalType>(mlirType)) {
    auto decimalTypeProto = std::make_unique<proto::Type::Decimal>();
    decimalTypeProto->set_precision(decimalType.getPrecision());
    decimalTypeProto->set_scale(decimalType.getScale());

    // TODO(ingomueller): support other nullability modes.
    decimalTypeProto->set_nullability(
        Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);

    auto type = std::make_unique<proto::Type>();
    type->set_allocated_decimal(decimalTypeProto.release());
    return std::move(type);
  }

  // Handle tuple types.
  if (auto tupleType = llvm::dyn_cast<TupleType>(mlirType)) {
    auto structType = std::make_unique<proto::Type::Struct>();
    for (mlir::Type fieldType : tupleType.getTypes()) {
      // Convert field type recursively.
      FailureOr<std::unique_ptr<proto::Type>> type = exportType(loc, fieldType);
      if (failed(type))
        return failure();
      *structType->add_types() = *type.value();
    }

    auto type = std::make_unique<proto::Type>();
    type->set_allocated_struct_(structType.release());
    return std::move(type);
  }

  // TODO(ingomueller): Support other types.
  return emitError(loc) << "could not export unsupported type " << mlirType;
}

FailureOr<std::unique_ptr<Rel>>
SubstraitExporter::exportOperation(AggregateOp op) {
  // Build `RelCommon` message.
  auto relCommon = std::make_unique<RelCommon>();
  auto direct = std::make_unique<RelCommon::Direct>();
  relCommon->set_allocated_direct(direct.release());

  // Build `input` message.
  auto inputOp =
      llvm::dyn_cast_if_present<RelOpInterface>(op.getInput().getDefiningOp());
  if (!inputOp)
    return op->emitOpError("input was not produced by Substrait relation op");

  FailureOr<std::unique_ptr<Rel>> inputRel = exportOperation(inputOp);
  if (failed(inputRel))
    return failure();

  // Build `AggregateRel` message.
  auto aggregateRel = std::make_unique<AggregateRel>();
  aggregateRel->set_allocated_common(relCommon.release());
  aggregateRel->set_allocated_input(inputRel->release());

  // Build `groupings` field.
  {
    // Make sure grouping expressions are distinct after CSE.
    if (!op.getGroupings().empty()) {
      // Set up rewriter to make temporary copy.
      IRRewriter rewriter(op.getContext());
      IRRewriter::InsertionGuard guard(rewriter);
      rewriter.setInsertionPointAfter(op);

      // Create a temporary copy that gets *erased* by the rewriter when it goes
      // out of scope.
      auto eraseOp = [&](Operation *op) { rewriter.eraseOp(op); };
      std::unique_ptr<Operation, decltype(eraseOp)> opCopy(rewriter.clone(*op),
                                                           eraseOp);
      AggregateOp aggrOpCopy = mlir::cast<AggregateOp>(opCopy.get());

      // Run CSE on the copy.
      {
        DominanceInfo domInfo;
        mlir::eliminateCommonSubExpressions(rewriter, domInfo, opCopy.get());
      }

      // Make sure that all yielded values are different. If they are not, then
      // some of them would result in equivalent grouping expressions in the
      // protobuf format, which would change the semantics of the op.
      auto yieldOp = llvm::cast<YieldOp>(
          aggrOpCopy.getGroupings().front().getTerminator());
      ValueRange yieldedValues = yieldOp->getOperands();
      DenseSet<Value> distinctYieldedValues;
      distinctYieldedValues.insert(yieldedValues.begin(), yieldedValues.end());
      if (yieldedValues.size() != distinctYieldedValues.size())
        return op.emitOpError()
               << "cannot be exported: values yielded from 'groupings' region "
                  "are not all distinct after CSE";
    }

    // Export values yielded from `groupings` region as `Expression` messages.
    SmallVector<std::unique_ptr<Expression>> columnExpressions;
    {
      // Get grouping expressions if any.
      ArrayRef<Value> emptyValueRange;
      ValueRange columnValues = emptyValueRange;
      if (!op.getGroupings().empty()) {
        auto yieldOp =
            llvm::cast<YieldOp>(op.getGroupings().front().getTerminator());
        columnValues = yieldOp->getOperands();
      }

      columnExpressions.reserve(columnValues.size());
      for (auto [columnIdx, columnVal] : llvm::enumerate(columnValues)) {
        // Build `Expression` message for operand.
        auto definingOp = llvm::dyn_cast_if_present<ExpressionOpInterface>(
            columnVal.getDefiningOp());
        if (!definingOp)
          return op->emitOpError()
                 << "yields grouping column " << columnIdx
                 << " that was not produced by Substrait expression op";

        FailureOr<std::unique_ptr<Expression>> columnExpr =
            exportOperation(definingOp);
        if (failed(columnExpr))
          return failure();

        columnExpressions.push_back(std::move(columnExpr.value()));
      }
    }

    // Populate repeated `groupings` field according to grouping sets.
    for (auto groupingSet : op.getGroupingSets().getAsRange<ArrayAttr>()) {
      AggregateRel::Grouping *grouping = aggregateRel->add_groupings();
      for (auto columnIdxAttr : groupingSet.getAsRange<IntegerAttr>()) {
        // Look up exported expression and add as `grouping_expression`.
        int64_t columnIdx = columnIdxAttr.getInt();
        Expression *columnExpr = columnExpressions[columnIdx].get();
        *grouping->add_grouping_expressions() = *columnExpr;
      }
    }
  }

  // Export measures if any.
  if (!op.getMeasures().empty()) {
    auto yieldOp =
        llvm::cast<YieldOp>(op.getMeasures().front().getTerminator());
    for (auto [measureIdx, measureVal] :
         llvm::enumerate(yieldOp->getOperands())) {
      // Build `Expression` message for operand.
      auto callOp = llvm::cast<CallOp>(measureVal.getDefiningOp());
      assert(callOp.isAggregate() && "expected aggregate function");

      FailureOr<std::unique_ptr<AggregateFunction>> aggregateFunction =
          exportCallOpAggregate(callOp);
      if (failed(aggregateFunction))
        return failure();

      // Add `AggregateFunction` to `measures`.
      AggregateRel::Measure *measure = aggregateRel->add_measures();
      measure->set_allocated_measure(aggregateFunction.value().release());
    }
  }

  // Attach the `AdvancedExtension` message if the attribute exists.
  exportAdvancedExtension(op, *aggregateRel);

  // Build `Rel` message.
  auto rel = std::make_unique<Rel>();
  rel->set_allocated_aggregate(aggregateRel.release());

  return rel;
}

FailureOr<std::unique_ptr<Expression>>
SubstraitExporter::exportOperation(CallOp op) {
  if (op.isScalar())
    return exportCallOpScalar(op);
  if (op.isWindow())
    return op.emitError() << "has a window function, which is currently not "
                             "supported for export";
  assert(op.isAggregate() && "unexpected function type");
  return op->emitOpError() << "with aggregate function not expected here";
}

FailureOr<std::unique_ptr<Expression>>
SubstraitExporter::exportOperation(CastOp op) {
  using FailureBehavior = Expression::Cast::FailureBehavior;

  Location loc = op.getLoc();

  // Export `input` as `Expression` message.
  Value inputVal = op.getInput();
  auto definingOp = llvm::dyn_cast_if_present<ExpressionOpInterface>(
      inputVal.getDefiningOp());
  if (!definingOp)
    return op->emitOpError()
           << "with 'input' that was not produced by Substrait expression op";

  FailureOr<std::unique_ptr<Expression>> input = exportOperation(definingOp);
  if (failed(input))
    return failure();

  // Build message for `output_type`.
  FailureOr<std::unique_ptr<proto::Type>> outputType =
      exportType(loc, op.getResult().getType());
  if (failed(outputType))
    return failure();

  // Build `Cast` message.
  auto cast = std::make_unique<Expression::Cast>();
  cast->set_allocated_input(input->release());
  cast->set_allocated_type(outputType->release());
  cast->set_failure_behavior(
      static_cast<FailureBehavior>(op.getFailureBehavior()));

  // Build `Expression` message.
  auto expression = std::make_unique<Expression>();
  expression->set_allocated_cast(cast.release());

  return expression;
}

FailureOr<std::unique_ptr<Rel>> SubstraitExporter::exportOperation(CrossOp op) {
  // Build `RelCommon` message.
  auto relCommon = std::make_unique<RelCommon>();
  auto direct = std::make_unique<RelCommon::Direct>();
  relCommon->set_allocated_direct(direct.release());

  // Build `left` input message.
  auto leftOp =
      llvm::dyn_cast_if_present<RelOpInterface>(op.getLeft().getDefiningOp());
  if (!leftOp)
    return op->emitOpError(
        "left input was not produced by Substrait relation op");

  FailureOr<std::unique_ptr<Rel>> leftRel = exportOperation(leftOp);
  if (failed(leftRel))
    return failure();

  // Build `right` input message.
  auto rightOp =
      llvm::dyn_cast_if_present<RelOpInterface>(op.getRight().getDefiningOp());
  if (!rightOp)
    return op->emitOpError(
        "right input was not produced by Substrait relation op");

  FailureOr<std::unique_ptr<Rel>> rightRel = exportOperation(rightOp);
  if (failed(rightRel))
    return failure();

  // Build `CrossRel` message.
  auto crossRel = std::make_unique<CrossRel>();
  crossRel->set_allocated_common(relCommon.release());
  crossRel->set_allocated_left(leftRel->release());
  crossRel->set_allocated_right(rightRel->release());

  // Attach the `AdvancedExtension` message if the attribute exists.
  exportAdvancedExtension(op, *crossRel);

  // Build `Rel` message.
  auto rel = std::make_unique<Rel>();
  rel->set_allocated_cross(crossRel.release());

  return rel;
}

FailureOr<std::unique_ptr<Rel>> SubstraitExporter::exportOperation(EmitOp op) {
  auto inputOp =
      dyn_cast_if_present<RelOpInterface>(op.getInput().getDefiningOp());
  if (!inputOp)
    return op->emitOpError(
        "has input that was not produced by Substrait relation op");

  // Export input op.
  FailureOr<std::unique_ptr<Rel>> inputRel = exportOperation(inputOp);
  if (failed(inputRel))
    return failure();

  // Build the `emit` message.
  auto emit = std::make_unique<RelCommon::Emit>();
  for (auto intAttr : op.getMapping().getAsRange<IntegerAttr>())
    emit->add_output_mapping(intAttr.getInt());

  // Attach the `emit` message to the `RelCommon` message.
  FailureOr<RelCommon *> relCommon =
      protobuf_utils::getMutableCommon(inputRel->get(), op.getLoc());
  if (failed(relCommon))
    return failure();

  if (relCommon.value()->has_emit()) {
    InFlightDiagnostic diag =
        op->emitOpError("has 'input' that already has 'emit' message "
                        "(try running canonicalization?)");
    diag.attachNote(inputOp.getLoc()) << "op exported to 'input' message";
    return diag;
  }

  relCommon.value()->set_allocated_emit(emit.release());

  return inputRel;
}

FailureOr<std::unique_ptr<Rel>> SubstraitExporter::exportOperation(JoinOp op) {
  // Build `RelCommon` message.
  auto relCommon = std::make_unique<RelCommon>();
  auto direct = std::make_unique<RelCommon::Direct>();
  relCommon->set_allocated_direct(direct.release());

  // Build `left` input message.
  auto leftOp =
      llvm::dyn_cast_if_present<RelOpInterface>(op.getLeft().getDefiningOp());
  if (!leftOp)
    return op->emitOpError(
        "left input was not produced by Substrait relation op");

  FailureOr<std::unique_ptr<Rel>> leftRel = exportOperation(leftOp);
  if (failed(leftRel))
    return failure();

  // Build `right` input message.
  auto rightOp =
      llvm::dyn_cast_if_present<RelOpInterface>(op.getRight().getDefiningOp());
  if (!rightOp)
    return op->emitOpError(
        "right input was not produced by Substrait relation op");

  FailureOr<std::unique_ptr<Rel>> rightRel = exportOperation(rightOp);
  if (failed(rightRel))
    return failure();

  // Build `JoinRel` message.
  auto joinRel = std::make_unique<JoinRel>();
  joinRel->set_allocated_common(relCommon.release());
  joinRel->set_allocated_left(leftRel->release());
  joinRel->set_allocated_right(rightRel->release());
  joinRel->set_type(static_cast<JoinRel::JoinType>(op.getJoinType()));

  // Attach the `AdvancedExtension` message if the attribute exists.
  exportAdvancedExtension(op, *joinRel);

  // Build `Rel` message.
  auto rel = std::make_unique<Rel>();
  rel->set_allocated_join(joinRel.release());

  return rel;
}

FailureOr<std::unique_ptr<Expression>>
SubstraitExporter::exportOperation(ExpressionOpInterface op) {
  return llvm::TypeSwitch<Operation *, FailureOr<std::unique_ptr<Expression>>>(
             op)
      .Case<CallOp, CastOp, FieldReferenceOp, LiteralOp>(
          [&](auto op) { return exportOperation(op); })
      .Default(
          [](auto op) { return op->emitOpError("not supported for export"); });
}

FailureOr<std::unique_ptr<Rel>>
SubstraitExporter::exportOperation(ExtensionTableOp op) {
  Location loc = op.getLoc();

  // Build `RelCommon` message.
  auto relCommon = std::make_unique<RelCommon>();
  auto direct = std::make_unique<RelCommon::Direct>();
  relCommon->set_allocated_direct(direct.release());

  // Build `ExtensionTable` message.
  StringAttr detailAttr = op.getDetailAttr();
  std::unique_ptr<pb::Any> detail = exportAny(detailAttr);
  auto extensionTable = std::make_unique<ReadRel::ExtensionTable>();
  extensionTable->set_allocated_detail(detail.release());

  // TODO(ingomueller): factor out commong logic of `ReadRel`.
  // Export field names and result type into `base_schema`.
  RelationType relationType = op.getResult().getType();
  TupleType tupleType = relationType.getStructType();
  FailureOr<std::unique_ptr<NamedStruct>> baseSchema =
      exportNamedStruct(loc, op.getFieldNames(), tupleType);
  if (failed(baseSchema))
    return failure();

  // Build `ReadRel` message.
  auto readRel = std::make_unique<ReadRel>();
  readRel->set_allocated_common(relCommon.release());
  readRel->set_allocated_extension_table(extensionTable.release());
  readRel->set_allocated_base_schema(baseSchema->release());

  // Attach the `AdvancedExtension` message if the attribute exists.
  exportAdvancedExtension(op, *readRel);

  // Build `Rel` message.
  auto rel = std::make_unique<Rel>();
  rel->set_allocated_read(readRel.release());

  return rel;
}

FailureOr<std::unique_ptr<Expression>>
SubstraitExporter::exportOperation(FieldReferenceOp op) {
  using FieldReference = Expression::FieldReference;
  using ReferenceSegment = Expression::ReferenceSegment;

  // Build linked list of `ReferenceSegment` messages.
  // TODO: support masked references.
  std::unique_ptr<Expression::ReferenceSegment> referenceRoot;
  for (int64_t pos : llvm::reverse(op.getPosition())) {
    // Remember child segment and create new `ReferenceSegment` message.
    auto childReference = std::move(referenceRoot);
    referenceRoot = std::make_unique<ReferenceSegment>();

    // Create `StructField` message.
    // TODO(ingomueller): support other segment types.
    auto structField = std::make_unique<ReferenceSegment::StructField>();
    structField->set_field(pos);
    structField->set_allocated_child(childReference.release());

    referenceRoot->set_allocated_struct_field(structField.release());
  }

  // Build `FieldReference` message.
  auto fieldReference = std::make_unique<FieldReference>();
  fieldReference->set_allocated_direct_reference(referenceRoot.release());

  // Handle different `root_type`s.
  Value inputVal = op.getContainer();
  if (Operation *definingOp = inputVal.getDefiningOp()) {
    // If there is a defining op, the `root_type` is an `Expression`.
    ExpressionOpInterface exprOp =
        llvm::dyn_cast<ExpressionOpInterface>(definingOp);
    if (!exprOp)
      return op->emitOpError("has 'container' operand that was not produced by "
                             "Substrait expression");

    FailureOr<std::unique_ptr<Expression>> expression = exportOperation(exprOp);
    if (failed(expression))
      return failure();

    fieldReference->set_allocated_expression(expression->release());
  } else {
    // Input must be a `BlockArgument`. Only support root references for now.
    auto blockArg = llvm::cast<BlockArgument>(inputVal);
    if (blockArg.getOwner() != op->getBlock())
      // TODO(ingomueller): support outer reference type.
      return op.emitOpError("has unsupported outer reference");

    auto rootReference = std::make_unique<FieldReference::RootReference>();
    fieldReference->set_allocated_root_reference(rootReference.release());
  }

  // Build `Expression` message.
  auto expression = std::make_unique<Expression>();
  expression->set_allocated_selection(fieldReference.release());

  return expression;
}

FailureOr<std::unique_ptr<Rel>> SubstraitExporter::exportOperation(FetchOp op) {
  // Build `RelCommon` message.
  auto relCommon = std::make_unique<RelCommon>();
  auto direct = std::make_unique<RelCommon::Direct>();
  relCommon->set_allocated_direct(direct.release());

  // Build `input` message.
  auto inputOp =
      llvm::dyn_cast_if_present<RelOpInterface>(op.getInput().getDefiningOp());
  if (!inputOp)
    return op->emitOpError("input was not produced by Substrait relation op");

  FailureOr<std::unique_ptr<Rel>> inputRel = exportOperation(inputOp);
  if (failed(inputRel))
    return failure();

  // Build `FetchRel` message.
  auto fetchRel = std::make_unique<FetchRel>();
  fetchRel->set_allocated_common(relCommon.release());
  fetchRel->set_allocated_input(inputRel->release());
  fetchRel->set_offset(op.getOffset());
  fetchRel->set_count(op.getCount());

  // Attach the `AdvancedExtension` message if the attribute exists.
  exportAdvancedExtension(op, *fetchRel);

  // Build `Rel` message.
  auto rel = std::make_unique<Rel>();
  rel->set_allocated_fetch(fetchRel.release());

  return rel;
}

FailureOr<std::unique_ptr<Rel>>
SubstraitExporter::exportOperation(FilterOp op) {
  // Build `RelCommon` message.
  auto relCommon = std::make_unique<RelCommon>();
  auto direct = std::make_unique<RelCommon::Direct>();
  relCommon->set_allocated_direct(direct.release());

  // Build input `Rel` message.
  auto inputOp =
      llvm::dyn_cast_if_present<RelOpInterface>(op.getInput().getDefiningOp());
  if (!inputOp)
    return op->emitOpError("input was not produced by Substrait relation op");

  FailureOr<std::unique_ptr<Rel>> inputRel = exportOperation(inputOp);
  if (failed(inputRel))
    return failure();

  // Build condition `Expression` message.
  auto yieldOp = llvm::cast<YieldOp>(op.getCondition().front().getTerminator());
  // TODO(ingomueller): There can be cases where there isn't a defining op but
  //                    the region argument is returned directly. Support that.
  assert(yieldOp.getValue().size() == 1 &&
         "fitler op must yield exactly one value");
  auto conditionOp = llvm::dyn_cast_if_present<ExpressionOpInterface>(
      yieldOp.getValue().front().getDefiningOp());
  if (!conditionOp)
    return op->emitOpError("condition not supported for export: yielded op was "
                           "not produced by Substrait expression op");
  FailureOr<std::unique_ptr<Expression>> condition =
      exportOperation(conditionOp);
  if (failed(condition))
    return failure();

  // Build `FilterRel` message.
  auto filterRel = std::make_unique<FilterRel>();
  filterRel->set_allocated_common(relCommon.release());
  filterRel->set_allocated_input(inputRel->release());
  filterRel->set_allocated_condition(condition->release());

  // Attach the `AdvancedExtension` message if the attribute exists.
  exportAdvancedExtension(op, *filterRel);

  // Build `Rel` message.
  auto rel = std::make_unique<Rel>();
  rel->set_allocated_filter(filterRel.release());

  return rel;
}

FailureOr<std::unique_ptr<Expression>>
SubstraitExporter::exportOperation(LiteralOp op) {
  // Build `Literal` message depending on type.
  Attribute value = op.getValue();
  mlir::Type literalType = getAttrType(value);
  auto literal = std::make_unique<Expression::Literal>();

  // `IntegerType`s.
  if (auto intType = dyn_cast<IntegerType>(literalType)) {
    if (!intType.isSigned())
      op->emitOpError("has integer value with unsupported signedness");
    switch (intType.getWidth()) {
    case 1:
      literal->set_boolean(mlir::cast<IntegerAttr>(value).getSInt());
      break;
    case 8:
      literal->set_i8(mlir::cast<IntegerAttr>(value).getSInt());
      break;
    case 16:
      literal->set_i16(mlir::cast<IntegerAttr>(value).getSInt());
      break;
    case 32:
      // TODO(ingomueller): Add tests when we can express plans that use i32.
      literal->set_i32(mlir::cast<IntegerAttr>(value).getSInt());
      break;
    case 64:
      literal->set_i64(mlir::cast<IntegerAttr>(value).getSInt());
      break;
    default:
      op->emitOpError("has integer value with unsupported width");
    }
  }
  // `FloatType`s.
  else if (auto floatType = dyn_cast<FloatType>(literalType)) {
    switch (floatType.getWidth()) {
    case 32:
      literal->set_fp32(mlir::cast<FloatAttr>(value).getValueAsDouble());
      break;
    case 64:
      // TODO(ingomueller): Add tests when we can express plans that use i32.
      literal->set_fp64(mlir::cast<FloatAttr>(value).getValueAsDouble());
      break;
    default:
      op->emitOpError("has float value with unsupported width");
    }
  }
  // `StringType`.
  else if (auto stringType = dyn_cast<StringType>(literalType)) {
    literal->set_string(mlir::cast<StringAttr>(value).getValue().str());
  }
  // `BinaryType`.
  else if (auto binaryType = dyn_cast<BinaryType>(literalType)) {
    literal->set_binary(mlir::cast<StringAttr>(value).getValue().str());
  }
  // `TimestampType`s.
  else if (auto timestampType = dyn_cast<TimestampType>(literalType)) {
    literal->set_timestamp(mlir::cast<TimestampAttr>(value).getValue());
  } else if (auto timestampTzType = dyn_cast<TimestampTzType>(literalType)) {
    literal->set_timestamp_tz(mlir::cast<TimestampTzAttr>(value).getValue());
  }
  // `DateType`.
  else if (auto dateType = dyn_cast<DateType>(literalType)) {
    literal->set_date(mlir::cast<DateAttr>(value).getValue());
  }
  // `TimeType`.
  else if (auto timeType = dyn_cast<TimeType>(literalType)) {
    literal->set_time(mlir::cast<TimeAttr>(value).getValue());
  }
  // `IntervalType`'s.
  else if (auto intervalType = dyn_cast<IntervalYearMonthType>(literalType)) {
    auto intervalYearToMonth = std::make_unique<
        ::substrait::proto::Expression_Literal_IntervalYearToMonth>();
    auto intervalYearMonth = mlir::cast<IntervalYearMonthAttr>(value);
    int32_t intervalYear = intervalYearMonth.getYears();
    int32_t intervalMonth = intervalYearMonth.getMonths();
    intervalYearToMonth->set_years(intervalYear);
    intervalYearToMonth->set_months(intervalMonth);
    literal->set_allocated_interval_year_to_month(
        intervalYearToMonth.release());
  } else if (auto timeType = dyn_cast<IntervalDaySecondType>(literalType)) {
    auto intervalDaytoSecond = std::make_unique<
        ::substrait::proto::Expression_Literal_IntervalDayToSecond>();
    auto intervalDaySecond = mlir::cast<IntervalDaySecondAttr>(value);
    int32_t intervalDay = intervalDaySecond.getDays();
    int32_t intervalSecond = intervalDaySecond.getSeconds();
    intervalDaytoSecond->set_days(intervalDay);
    intervalDaytoSecond->set_seconds(intervalSecond);
    literal->set_allocated_interval_day_to_second(
        intervalDaytoSecond.release());
  } // `UUIDType`.
  else if (auto uuidType = dyn_cast<UUIDType>(literalType)) {
    llvm::APInt uuid = mlir::cast<UUIDAttr>(value).getValue().getValue();
    std::string res(16, 0);
    llvm::StoreIntToMemory(uuid, reinterpret_cast<uint8_t *>(res.data()), 16);
    literal->set_uuid(res);
    // `FixedCharType`.
  } else if (auto fixedCharType = dyn_cast<FixedCharType>(literalType)) {
    literal->set_fixed_char(mlir::cast<FixedCharAttr>(value).getValue().str());
    // `VarCharType`.
  } else if (auto varCharType = dyn_cast<VarCharType>(literalType)) {
    auto varChar =
        std::make_unique<::substrait::proto::Expression_Literal_VarChar>();
    varChar->set_value(mlir::cast<VarCharAttr>(value).getValue().str());
    literal->set_allocated_var_char(varChar.release());
    // `FixedBinaryType`.
  } else if (auto fixedBinaryType = dyn_cast<FixedBinaryType>(literalType)) {
    literal->set_allocated_fixed_binary(
        new std::string(mlir::cast<FixedBinaryAttr>(value).getValue().str()));
  } // `DecimalType`.
  else if (auto decimalType = dyn_cast<DecimalType>(literalType)) {
    auto decimal =
        std::make_unique<::substrait::proto::Expression_Literal_Decimal>();
    auto decimalAttr = mlir::cast<DecimalAttr>(value);
    APInt value = decimalAttr.getValue().getValue();
    std::string res(16, 0);
    llvm::StoreIntToMemory(value, reinterpret_cast<uint8_t *>(res.data()), 16);
    decimal->set_scale(decimalType.getScale());
    decimal->set_precision(decimalType.getPrecision());
    decimal->set_value(res);
    literal->set_allocated_decimal(decimal.release());
  } else
    op->emitOpError("has unsupported value");

  // Build `Expression` message.
  auto expression = std::make_unique<Expression>();
  expression->set_allocated_literal(literal.release());

  return expression;
}

FailureOr<std::unique_ptr<pb::Message>>
SubstraitExporter::exportOperation(ModuleOp op) {
  if (!op->getAttrs().empty()) {
    op->emitOpError("has attributes");
    return failure();
  }

  Region &body = op.getBodyRegion();
  if (llvm::range_size(body.getOps()) != 1) {
    op->emitOpError("has more than one op in its body");
    return failure();
  }

  Operation *innerOp = &*body.op_begin();
  if (llvm::isa<PlanOp, PlanVersionOp>(innerOp))
    return exportOperation(innerOp);

  op->emitOpError("contains an op that is not a 'substrait.plan'");
  return failure();
}

FailureOr<std::unique_ptr<NamedStruct>>
SubstraitExporter::exportNamedStruct(Location loc, ArrayAttr fieldNames,
                                     TupleType tupleType) {

  // Build `Struct` message.
  auto struct_ = std::make_unique<proto::Type::Struct>();
  struct_->set_nullability(
      Type_Nullability::Type_Nullability_NULLABILITY_REQUIRED);
  for (mlir::Type fieldType : tupleType.getTypes()) {
    FailureOr<std::unique_ptr<proto::Type>> type = exportType(loc, fieldType);
    if (failed(type))
      return (failure());
    *struct_->add_types() = *std::move(type.value());
  }

  // Build `NamedStruct` message.
  auto namedStruct = std::make_unique<NamedStruct>();
  namedStruct->set_allocated_struct_(struct_.release());
  for (Attribute attr : fieldNames) {
    namedStruct->add_names(mlir::cast<StringAttr>(attr).getValue().str());
  }

  return namedStruct;
}

FailureOr<std::unique_ptr<Rel>>
SubstraitExporter::exportOperation(NamedTableOp op) {
  Location loc = op.getLoc();

  // Build `NamedTable` message.
  auto namedTable = std::make_unique<ReadRel::NamedTable>();
  namedTable->add_names(op.getTableName().getRootReference().str());
  for (SymbolRefAttr attr : op.getTableName().getNestedReferences()) {
    namedTable->add_names(attr.getLeafReference().str());
  }

  // Build `RelCommon` message.
  auto relCommon = std::make_unique<RelCommon>();
  auto direct = std::make_unique<RelCommon::Direct>();
  relCommon->set_allocated_direct(direct.release());

  // TODO(ingomueller): factor out commong logic of `ReadRel`.
  // Export field names and result type into `base_schema`.
  RelationType relationType = op.getResult().getType();
  TupleType tupleType = relationType.getStructType();
  FailureOr<std::unique_ptr<NamedStruct>> baseSchema =
      exportNamedStruct(loc, op.getFieldNames(), tupleType);
  if (failed(baseSchema))
    return failure();

  // Build `ReadRel` message.
  auto readRel = std::make_unique<ReadRel>();
  readRel->set_allocated_common(relCommon.release());
  readRel->set_allocated_base_schema(baseSchema->release());
  readRel->set_allocated_named_table(namedTable.release());

  // Attach the `AdvancedExtension` message if the attribute exists.
  exportAdvancedExtension(op, *readRel);

  // Build `Rel` message.
  auto rel = std::make_unique<Rel>();
  rel->set_allocated_read(readRel.release());

  return rel;
}

/// Helper for creating unique anchors from symbol names. While in MLIR, symbol
/// names and their references are strings, in Substrait they are integer
/// numbers. In order to preserve the anchor values through an import/export
/// process (without modifications), the symbol names generated during import
/// have the form `<prefix>.<anchor>` such that the `anchor` value can be
/// recovered. During assigning of anchors, the uniquer fills a map mapping the
/// symbol ops to the assigned anchor values such that uses of the symbol can
/// look them up.
class AnchorUniquer {
public:
  AnchorUniquer(StringRef prefix, DenseMap<Operation *, int32_t> &anchorsByOp)
      : prefix(prefix), anchorsByOp(anchorsByOp) {}

  /// Assign a unique anchor to the given op and register the result in the
  /// mapping.
  template <typename OpTy>
  int32_t assignAnchor(OpTy op) {
    StringRef symName = op.getSymName();
    int32_t anchor;
    {
      // Attempt to recover the anchor from the symbol name.
      if (!symName.starts_with(prefix) ||
          symName.drop_front(prefix.size()).getAsInteger(10, anchor)) {
        // If that fails, find one that isn't used yet.
        anchor = nextAnchor;
      }
      // Ensure uniqueness either way.
      while (anchors.contains(anchor))
        anchor = nextAnchor++;
    }
    anchors.insert(anchor);
    auto [_, hasInserted] = anchorsByOp.try_emplace(op, anchor);
    assert(hasInserted && "op had already been assigned an anchor");
    return anchor;
  }

private:
  StringRef prefix;
  DenseMap<Operation *, int32_t> &anchorsByOp; // Maps ops to anchor values.
  DenseSet<int32_t> anchors;                   // Already assigned anchors.
  int32_t nextAnchor{0};                       // Next anchor candidate.
};

/// Traits for common handling of `ExtensionFunctionOp`, `ExtensionTypeOp`, and
/// `ExtensionTypeVariationOp`. While their corresponding protobuf message types
/// are structurally the same, they are (1) different classes and (2) have
/// different field names. The Trait thus provides the message type class as
/// well as accessors for that class for each of the op types.
template <typename OpTy>
struct ExtensionOpTraits;

template <>
struct ExtensionOpTraits<ExtensionFunctionOp> {
  using ExtensionMessageType =
      extensions::SimpleExtensionDeclaration::ExtensionFunction;
  static void setAnchor(ExtensionMessageType &ext, int32_t anchor) {
    ext.set_function_anchor(anchor);
  }
  static ExtensionMessageType *
  getMutableExtension(extensions::SimpleExtensionDeclaration &decl) {
    return decl.mutable_extension_function();
  }
};

template <>
struct ExtensionOpTraits<ExtensionTypeOp> {
  using ExtensionMessageType =
      extensions::SimpleExtensionDeclaration::ExtensionType;
  static void setAnchor(ExtensionMessageType &ext, int32_t anchor) {
    ext.set_type_anchor(anchor);
  }
  static ExtensionMessageType *
  getMutableExtension(extensions::SimpleExtensionDeclaration &decl) {
    return decl.mutable_extension_type();
  }
};

template <>
struct ExtensionOpTraits<ExtensionTypeVariationOp> {
  using ExtensionMessageType =
      extensions::SimpleExtensionDeclaration::ExtensionTypeVariation;
  static void setAnchor(ExtensionMessageType &ext, int32_t anchor) {
    ext.set_type_variation_anchor(anchor);
  }
  static ExtensionMessageType *
  getMutableExtension(extensions::SimpleExtensionDeclaration &decl) {
    return decl.mutable_extension_type_variation();
  }
};

FailureOr<std::unique_ptr<Plan>> SubstraitExporter::exportOperation(PlanOp op) {
  using extensions::SimpleExtensionDeclaration;
  using extensions::SimpleExtensionURI;

  // Build `Plan` message.
  auto plan = std::make_unique<Plan>();

  // Build `Version` message.
  auto version = std::make_unique<Version>();
  version->set_major_number(op.getMajorNumber());
  version->set_minor_number(op.getMinorNumber());
  version->set_patch_number(op.getPatchNumber());
  version->set_producer(op.getProducer().str());
  version->set_git_hash(op.getGitHash().str());
  plan->set_allocated_version(version.release());

  // Attach the `AdvancedExtension` message if the attribute exists.
  exportAdvancedExtension(op, *plan);

  // Add `expected_type_urls` to plan if present.
  if (op.getExpectedTypeUrls()) {
    ArrayAttr expected_type_urls = op.getExpectedTypeUrls().value();
    for (auto expected_type_url : expected_type_urls.getAsRange<StringAttr>())
      plan->add_expected_type_urls(expected_type_url.str());
  }

  // Add `extension_uris` to plan.
  {
    AnchorUniquer anchorUniquer("extension_uri.", anchorsByOp);
    for (auto uriOp : op.getOps<ExtensionUriOp>()) {
      int32_t anchor = anchorUniquer.assignAnchor(uriOp);

      // Create `SimpleExtensionURI` message.
      SimpleExtensionURI *uri = plan->add_extension_uris();
      uri->set_uri(uriOp.getUri().str());
      uri->set_extension_uri_anchor(anchor);
    }
  }

  // Add `extensions` to plan. This requires the URIs to exist.
  {
    // Each extension type has its own anchor uniquer.
    AnchorUniquer funcUniquer("extension_function.", anchorsByOp);
    AnchorUniquer typeUniquer("extension_type.", anchorsByOp);
    AnchorUniquer typeVarUniquer("extension_type_variation.", anchorsByOp);

    // Export an op of a given type using the corresponding uniquer.
    auto exportExtensionOperation = [&](AnchorUniquer *uniquer, auto extOp) {
      using OpTy = decltype(extOp);
      using OpTraits = ExtensionOpTraits<OpTy>;

      // Compute URI reference and anchor value.
      int32_t uriReference = lookupAnchor(op, extOp.getUri());
      int32_t anchor = uniquer->assignAnchor(extOp);

      // Create `SimpleExtensionDeclaration` and extension-specific messages.
      typename OpTraits::ExtensionMessageType ext;
      OpTraits::setAnchor(ext, anchor);
      ext.set_extension_uri_reference(uriReference);
      ext.set_name(extOp.getName().str());
      SimpleExtensionDeclaration *decl = plan->add_extensions();
      *OpTraits::getMutableExtension(*decl) = ext;
    };

    // Iterate over the different types of extension ops. This must be a single
    // loop in order to preserve the order, which allows for interleaving of
    // different types in both the protobuf and the MLIR form.
    for (Operation &extOp : op.getOps()) {
      TypeSwitch<Operation &>(extOp)
          .Case<ExtensionFunctionOp>([&](auto extOp) {
            exportExtensionOperation(&funcUniquer, extOp);
          })
          .Case<ExtensionTypeOp>([&](auto extOp) {
            exportExtensionOperation(&typeUniquer, extOp);
          })
          .Case<ExtensionTypeVariationOp>([&](auto extOp) {
            exportExtensionOperation(&typeVarUniquer, extOp);
          });
    }
  }

  // Add `relation`s to plan.
  for (auto relOp : op.getOps<PlanRelOp>()) {
    Operation *terminator = relOp.getBody().front().getTerminator();
    auto rootOp =
        llvm::cast<RelOpInterface>(terminator->getOperand(0).getDefiningOp());

    FailureOr<std::unique_ptr<Rel>> rel = exportOperation(rootOp);
    if (failed(rel))
      return failure();

    // Handle `Rel`/`RelRoot` cases depending on whether `names` is set.
    PlanRel *planRel = plan->add_relations();
    if (std::optional<Attribute> names = relOp.getFieldNames()) {
      auto root = std::make_unique<RelRoot>();
      root->set_allocated_input(rel->release());

      auto namesArray = cast<ArrayAttr>(names.value()).getAsRange<StringAttr>();
      for (StringAttr name : namesArray) {
        root->add_names(name.getValue().str());
      }

      planRel->set_allocated_root(root.release());
    } else {
      planRel->set_allocated_rel(rel->release());
    }
  }

  return std::move(plan);
}

FailureOr<std::unique_ptr<PlanVersion>>
SubstraitExporter::exportOperation(PlanVersionOp op) {
  VersionAttr versionAttr = op.getVersion();

  // Build `Version` message.
  auto version = std::make_unique<Version>();
  version->set_major_number(versionAttr.getMajorNumber());
  version->set_minor_number(versionAttr.getMinorNumber());
  version->set_patch_number(versionAttr.getPatchNumber());
  if (versionAttr.getProducer())
    version->set_producer(versionAttr.getProducer().str());
  if (versionAttr.getGitHash())
    version->set_git_hash(versionAttr.getGitHash().str());

  // Build `PlanVersion` message.
  auto planVersion = std::make_unique<PlanVersion>();
  planVersion->set_allocated_version(version.release());

  return planVersion;
}

FailureOr<std::unique_ptr<Rel>>
SubstraitExporter::exportOperation(ProjectOp op) {
  // Build `RelCommon` message.
  auto relCommon = std::make_unique<RelCommon>();
  auto direct = std::make_unique<RelCommon::Direct>();
  relCommon->set_allocated_direct(direct.release());

  // Build input `Rel` message.
  auto inputOp =
      llvm::dyn_cast_if_present<RelOpInterface>(op.getInput().getDefiningOp());
  if (!inputOp)
    return op->emitOpError("input was not produced by Substrait relation op");

  FailureOr<std::unique_ptr<Rel>> inputRel = exportOperation(inputOp);
  if (failed(inputRel))
    return failure();

  // Build `ProjectRel` message.
  auto projectRel = std::make_unique<ProjectRel>();
  projectRel->set_allocated_common(relCommon.release());
  projectRel->set_allocated_input(inputRel->release());

  // Build `Expression` messages.
  auto yieldOp =
      llvm::cast<YieldOp>(op.getExpressions().front().getTerminator());
  for (Value val : yieldOp.getValue()) {
    // Make sure the yielded value was produced by an expression op.
    auto exprRootOp =
        llvm::dyn_cast_if_present<ExpressionOpInterface>(val.getDefiningOp());
    if (!exprRootOp)
      return op->emitOpError(
          "expression not supported for export: yielded op was "
          "not produced by Substrait expression op");

    // Export the expression recursively.
    FailureOr<std::unique_ptr<Expression>> expression =
        exportOperation(exprRootOp);
    if (failed(expression))
      return failure();

    // Add the expression to the `ProjectRel` message.
    *projectRel->add_expressions() = *expression.value();
  }

  // Attach the `AdvancedExtension` message if the attribute exists.
  exportAdvancedExtension(op, *projectRel);

  // Build `Rel` message.
  auto rel = std::make_unique<Rel>();
  rel->set_allocated_project(projectRel.release());

  return rel;
}

template <typename MessageType>
FailureOr<std::unique_ptr<MessageType>>
SubstraitExporter::exportCallOpCommon(CallOp op) {
  Location loc = op.getLoc();

  // Build main message.
  auto function = std::make_unique<MessageType>();
  int32_t anchor = lookupAnchor(op, op.getCallee());
  function->set_function_reference(anchor);

  // Build messages for arguments.
  for (auto [i, operand] : llvm::enumerate(op->getOperands())) {
    // Build `Expression` message for operand.
    auto definingOp = llvm::dyn_cast_if_present<ExpressionOpInterface>(
        operand.getDefiningOp());
    if (!definingOp)
      return op->emitOpError()
             << "with operand " << i
             << " that was not produced by Substrait expression op";

    FailureOr<std::unique_ptr<Expression>> expression =
        exportOperation(definingOp);
    if (failed(expression))
      return failure();

    // Build `FunctionArgument` message and add to arguments.
    FunctionArgument arg;
    arg.set_allocated_value(expression->release());
    *function->add_arguments() = arg;
  }

  // Build message for `output_type`.
  FailureOr<std::unique_ptr<proto::Type>> outputType =
      exportType(loc, op.getResult().getType());
  if (failed(outputType))
    return failure();
  function->set_allocated_output_type(outputType->release());

  return function;
}

FailureOr<std::unique_ptr<AggregateFunction>>
SubstraitExporter::exportCallOpAggregate(CallOp op) {
  assert(op.isAggregate() && "expected aggregate function");

  using AggregationPhase = ::mlir::substrait::AggregationPhase;

  // Export common fields.
  FailureOr<std::unique_ptr<AggregateFunction>> maybeAggregateFunction =
      exportCallOpCommon<AggregateFunction>(op);
  if (failed(maybeAggregateFunction))
    return failure();
  std::unique_ptr<AggregateFunction> aggregateFunction =
      std::move(maybeAggregateFunction.value());

  // Add aggregation-specific fields.
  AggregationPhase phase = op.getAggregationPhase().value();
  aggregateFunction->set_phase(static_cast<proto::AggregationPhase>(phase));
  AggregationInvocation invocation = op.getAggregationInvocation().value();
  aggregateFunction->set_invocation(
      static_cast<AggregateFunction::AggregationInvocation>(invocation));

  return aggregateFunction;
}

FailureOr<std::unique_ptr<Expression>>
SubstraitExporter::exportCallOpScalar(CallOp op) {
  using ScalarFunction = Expression::ScalarFunction;
  assert(op.isScalar() && "expected scalar function");

  // Export common fields.
  FailureOr<std::unique_ptr<ScalarFunction>> scalarFunction =
      exportCallOpCommon<ScalarFunction>(op);
  if (failed(scalarFunction))
    return failure();

  // Build `Expression` message.
  auto expression = std::make_unique<Expression>();
  expression->set_allocated_scalar_function(scalarFunction.value().release());

  return expression;
}

FailureOr<std::unique_ptr<Expression>>
SubstraitExporter::exportCallOpWindow(CallOp op) {
  llvm_unreachable("not implemented");
}

FailureOr<std::unique_ptr<Rel>> SubstraitExporter::exportOperation(SetOp op) {
  // Build `RelCommon` message.
  auto relCommon = std::make_unique<RelCommon>();
  auto direct = std::make_unique<RelCommon::Direct>();
  relCommon->set_allocated_direct(direct.release());

  llvm::SmallVector<Operation *> inputRel;

  // Build `SetRel` message.
  auto setRel = std::make_unique<SetRel>();
  setRel->set_allocated_common(relCommon.release());
  setRel->set_op(static_cast<SetRel::SetOp>(op.getKind()));

  // Build `inputs` message.
  for (Value input : op.getInputs()) {
    auto inputOp =
        llvm::dyn_cast_if_present<RelOpInterface>(input.getDefiningOp());
    if (!inputOp)
      return op->emitOpError(
          "inputs were not produced by Substrait relation op");
    FailureOr<std::unique_ptr<Rel>> inputRel = exportOperation(inputOp);
    if (failed(inputRel))
      return failure();
    setRel->add_inputs()->CopyFrom(*inputRel->get());
  }

  // Attach the `AdvancedExtension` message if the attribute exists.
  exportAdvancedExtension(op, *setRel);

  // Build `Rel` message.
  auto rel = std::make_unique<Rel>();
  rel->set_allocated_set(setRel.release());

  return rel;
}

FailureOr<std::unique_ptr<Rel>>
SubstraitExporter::exportOperation(RelOpInterface op) {
  return llvm::TypeSwitch<Operation *, FailureOr<std::unique_ptr<Rel>>>(op)
      .Case<
          // clang-format off
          AggregateOp,
          CrossOp,
          EmitOp,
          ExtensionTableOp,
          FetchOp,
          FieldReferenceOp,
          FilterOp,
          JoinOp,
          NamedTableOp,
          ProjectOp,
          SetOp
          // clang-format on
          >([&](auto op) { return exportOperation(op); })
      .Default([](auto op) {
        op->emitOpError("not supported for export");
        return failure();
      });
}

FailureOr<std::unique_ptr<pb::Message>>
SubstraitExporter::exportOperation(Operation *op) {
  return llvm::TypeSwitch<Operation *, FailureOr<std::unique_ptr<pb::Message>>>(
             op)
      .Case<ModuleOp, PlanOp, PlanVersionOp>(
          [&](auto op) -> FailureOr<std::unique_ptr<pb::Message>> {
            auto typedMessage = exportOperation(op);
            if (failed(typedMessage))
              return failure();
            return std::unique_ptr<pb::Message>(typedMessage.value().release());
          })
      .Default([](auto op) {
        op->emitOpError("not supported for export");
        return failure();
      });
}

} // namespace

mlir::LogicalResult mlir::substrait::translateSubstraitToProtobuf(
    Operation *op, llvm::raw_ostream &output,
    mlir::substrait::ImportExportOptions options) {
  SubstraitExporter exporter;
  FailureOr<std::unique_ptr<::google::protobuf::Message>> result =
      exporter.exportOperation(op);
  if (failed(result))
    return failure();

  std::string out;
  switch (options.serdeFormat) {
  case substrait::SerdeFormat::kText:
    if (!::google::protobuf::TextFormat::PrintToString(*result.value(), &out)) {
      op->emitOpError("could not be serialized to text format");
      return failure();
    }
    break;
  case substrait::SerdeFormat::kBinary:
    if (!result->get()->SerializeToString(&out)) {
      op->emitOpError("could not be serialized to binary format");
      return failure();
    }
    break;
  case substrait::SerdeFormat::kJson:
  case substrait::SerdeFormat::kPrettyJson: {
    ::google::protobuf::util::JsonPrintOptions jsonOptions;
    if (options.serdeFormat == SerdeFormat::kPrettyJson)
      jsonOptions.add_whitespace = true;
    absl::Status status = ::google::protobuf::util::MessageToJsonString(
        *result.value(), &out, jsonOptions);
    if (!status.ok()) {
      InFlightDiagnostic diag =
          op->emitOpError("could not be serialized to JSON format");
      diag.attachNote() << status.message();
      return diag;
    }
  }
  }

  output << out;
  return success();
}
