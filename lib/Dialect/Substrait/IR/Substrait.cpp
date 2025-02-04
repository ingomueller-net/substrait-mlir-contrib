//===-- Substrait.cpp - Substrait dialect -----------------------*- C++ -*-===//
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "substrait-mlir/Dialect/Substrait/IR/Substrait.h"

#include "mlir/IR/DialectImplementation.h" // IWYU pragma: keep
#include "mlir/IR/PatternMatch.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/TypeSwitch.h" // IWYU pragma: keep
#include "llvm/Support/Regex.h"

using namespace mlir;
using namespace mlir::substrait;

//===----------------------------------------------------------------------===//
// Substrait dialect
//===----------------------------------------------------------------------===//

#include "substrait-mlir/Dialect/Substrait/IR/SubstraitOpsDialect.cpp.inc"

namespace mlir::substrait::detail {

/// Storage class for Substrait's `RelationType`.
///
/// This is a copy of the upstream `TupleTypeStorage`, which we can't reuse
/// because it is in a private header.
struct RelationTypeStorage final
    : public TypeStorage,
      public llvm::TrailingObjects<RelationTypeStorage, Type> {
  using KeyTy = TypeRange;

  RelationTypeStorage(unsigned numTypes) : numElements(numTypes) {}

  /// Construction.
  static RelationTypeStorage *construct(TypeStorageAllocator &allocator,
                                        TypeRange key) {
    // Allocate a new storage instance.
    auto byteSize = RelationTypeStorage::totalSizeToAlloc<Type>(key.size());
    auto *rawMem = allocator.allocate(byteSize, alignof(RelationTypeStorage));
    auto *result = ::new (rawMem) RelationTypeStorage(key.size());

    // Copy in the element types into the trailing storage.
    std::uninitialized_copy(key.begin(), key.end(),
                            result->getTrailingObjects<Type>());
    return result;
  }

  bool operator==(const KeyTy &key) const { return key == getTypes(); }

  /// Return the number of held types.
  unsigned size() const { return numElements; }

  /// Return the held types.
  ArrayRef<Type> getTypes() const {
    return {getTrailingObjects<Type>(), size()};
  }

  KeyTy getAsKey() const { return getTypes(); }

  /// The number of tuple elements.
  unsigned numElements;
};

} // namespace mlir::substrait::detail

void SubstraitDialect::initialize() {
#define GET_OP_LIST
  addOperations<
#include "substrait-mlir/Dialect/Substrait/IR/SubstraitOps.cpp.inc"
      >();
  addTypes<
#define GET_TYPEDEF_LIST
#include "substrait-mlir/Dialect/Substrait/IR/SubstraitOpsTypes.cpp.inc"
      >();
  addAttributes<
#define GET_ATTRDEF_LIST
#include "substrait-mlir/Dialect/Substrait/IR/SubstraitOpsAttrs.cpp.inc"
      >();
}

//===----------------------------------------------------------------------===//
// Free functions
//===----------------------------------------------------------------------===//

namespace mlir::substrait {

Type getAttrType(Attribute attr) {
  if (auto typedAttr = mlir::dyn_cast<TypedAttr>(attr))
    return typedAttr.getType();
  if (auto typedAttr = mlir::dyn_cast<TypeInferableAttrInterface>(attr))
    return typedAttr.getType();
  return Type();
}

} // namespace mlir::substrait

//===----------------------------------------------------------------------===//
// Substrait attributes
//===----------------------------------------------------------------------===//

LogicalResult AdvancedExtensionAttr::verify(
    llvm::function_ref<mlir::InFlightDiagnostic()> emitError,
    mlir::StringAttr optimization, mlir::StringAttr enhancement) {
  if (optimization && !mlir::isa<AnyType>(optimization.getType()))
    return emitError() << "has 'optimization' attribute of wrong type";
  if (enhancement && !mlir::isa<AnyType>(enhancement.getType()))
    return emitError() << "has 'enhancement' attribute of wrong type";
  return success();
}

LogicalResult mlir::substrait::FixedCharAttr::verify(
    llvm::function_ref<mlir::InFlightDiagnostic()> emitError, StringAttr value,
    Type type) {
  FixedCharType fixedCharType = mlir::dyn_cast<FixedCharType>(type);
  int32_t value_length = value.size();
  if (fixedCharType != nullptr && value_length != fixedCharType.getLength())
    return emitError() << "value length must be " << fixedCharType.getLength()
                       << " characters.";
  return success();
}

LogicalResult mlir::substrait::FixedBinaryAttr::verify(
    llvm::function_ref<mlir::InFlightDiagnostic()> emitError, StringAttr value,
    FixedBinaryType type) {
  FixedBinaryType fixedBinaryType = mlir::dyn_cast<FixedBinaryType>(type);
  if (fixedBinaryType == nullptr)
    return emitError() << "expected a fixed binary type";
  int32_t value_length = value.size();
  if (value_length != fixedBinaryType.getLength())
    return emitError() << "value length must be " << fixedBinaryType.getLength()
                       << " characters.";
  return success();
}

LogicalResult mlir::substrait::IntervalYearMonthAttr::verify(
    llvm::function_ref<mlir::InFlightDiagnostic()> emitError, int32_t year,
    int32_t month) {
  if (year < -100000 || year > 100000)
    return emitError() << "year must be in a range of [-10,000..10,000] years";
  if (month < -120000 || month > 120000)
    return emitError()
           << "month must be in a range of [120,000..120,000] months";
  return success();
}

LogicalResult mlir::substrait::IntervalDaySecondAttr::verify(
    llvm::function_ref<mlir::InFlightDiagnostic()> emitError, int32_t days,
    int32_t seconds) {
  if (days < -3650000 || days > 3650000)
    return emitError()
           << "days must be in a range of [-3,650,000..3,650,000] days";
  // The value of `seconds` should be within the range [-315,360,000,000..
  // 315,360,000,000]. However, this exceeds the limits of int32_t (as required
  // by the specification), making it untestable within the given constraints.
  return success();
}

LogicalResult mlir::substrait::VarCharAttr::verify(
    llvm::function_ref<mlir::InFlightDiagnostic()> emitError, StringAttr value,
    VarCharType type) {
  int32_t value_length = value.size();
  if (value_length > type.getLength())
    return emitError() << "value length must be at most " << type.getLength()
                       << " characters.";
  return success();
}

//===----------------------------------------------------------------------===//
// Substrait types
//===----------------------------------------------------------------------===//

LogicalResult mlir::substrait::DecimalType::verify(
    llvm::function_ref<mlir::InFlightDiagnostic()> emitError,
    uint32_t precision, uint32_t scale) {
  if (precision > 38)
    return emitError() << "precision must be in a range of [0..38] but got "
                       << precision;

  if (scale > precision)
    return emitError() << "scale must be in a range of [0..P] (P = "
                       << precision << ") but got " << scale;

  return success();
}

// Count the number of digits in an APInt in base 10.
static size_t countDigits(const APInt &value) {
  llvm::SmallVector<char> buffer;
  value.toString(buffer, 10, /*isSigned=*/false);
  return buffer.size();
}

LogicalResult mlir::substrait::DecimalAttr::verify(
    llvm::function_ref<mlir::InFlightDiagnostic()> emitError, DecimalType type,
    IntegerAttr value) {

  if (value.getType().getIntOrFloatBitWidth() != 128)
    return emitError() << "value must be a 128-bit integer";

  // Max `P` digits.
  size_t nDigits = countDigits(value.getValue());
  size_t P = type.getPrecision();
  if (nDigits > P)
    return emitError() << "value must have at most " << P
                       << " digits as per the type " << type << " but got "
                       << nDigits;

  return success();
}

std::string DecimalAttr::toDecimalString(DecimalType type, IntegerAttr value) {
  size_t scale = type.getScale();
  size_t precision = type.getPrecision();

  // Convert to string and pad up to `P` digits with leading zeros.
  SmallVector<char> buffer;
  value.getValue().toString(buffer, 10, /*isSigned=*/false);
  buffer.insert(buffer.begin(), precision - buffer.size(), '0');
  assert(buffer.size() == precision &&
         "expected padded string to be exactly `P` digits long");

  // Get parts before and after the decimal point.
  StringRef str(buffer.data(), buffer.size());
  StringRef integralPart = str.drop_back(scale);
  StringRef fractionalPart = str.take_back(scale);
  assert(str.size() == precision &&
         "expected padded string to be exactly `P` digits long");

  {
    // Trim leading zeros of integral part.
    size_t firstNonZero = integralPart.find_first_not_of('0');
    if (firstNonZero != StringRef::npos)
      integralPart = integralPart.drop_front(firstNonZero);
    else
      integralPart = "0";

    // Trim trailing zeros of fractional part.
    size_t lastNonZero = fractionalPart.find_last_not_of('0');
    if (lastNonZero != StringRef::npos)
      fractionalPart = fractionalPart.take_front(lastNonZero + 1);
    else
      fractionalPart = "0";
  }

  // Make sure neither part is emtpy.
  if (integralPart.empty())
    integralPart = "0";
  if (fractionalPart.empty())
    fractionalPart = "0";

  return (integralPart + Twine(".") + fractionalPart).str();
}

ParseResult DecimalAttr::parseDecimalString(
    ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError, StringRef str,
    DecimalType type, IntegerAttr &value) {
  MLIRContext *context = type.getContext();

  // Parse as two point-separated integers, ignoring irrelevant zeros.
  static const llvm::Regex regex("^0*([0-9]+)\\.([0-9]*[1-9]|0)0*$");
  SmallVector<StringRef> matches;
  regex.match(str, &matches);

  if (matches.size() != 3)
    return emitError() << "'" << str << "' is not a valid decimal number";

  StringRef integralPart = matches[1];
  StringRef fractionalPart = matches[2];

  // Normalize corner cases where a part only consists of a zero.
  if (integralPart == "0")
    integralPart = "";
  if (fractionalPart == "0")
    fractionalPart = "";

  // Verify scale.
  size_t detectedScale = fractionalPart.size();
  if (detectedScale > type.getScale()) {
    return emitError()
           << "decimal value has too many digits after the decimal point ("
           << detectedScale << "). Expected <=" << type.getScale()
           << " as per the type " << type;
  }

  // Verify precision.
  size_t precision = type.getPrecision();
  size_t numDigits = detectedScale + integralPart.size();
  if (numDigits > precision)
    return emitError() << "decimal value has too many digits (" << numDigits
                       << "). Expected <=" << precision << " as per the type "
                       << type;

  // Concatenate parts to normalized string. Add trailing zeros if necessary
  // (detectedScale != type.getScale()). This is required to be able to
  // represent values where the number of digits after the decimal point is less
  // than the scale.
  std::string trailingZeros(type.getScale() - detectedScale, '0');
  std::string normalizedStr =
      (Twine(integralPart) + fractionalPart + trailingZeros).str();

  // Parse into APInt and create IntegerAttr.
  APInt intValue(128, normalizedStr, 10);
  auto intType = IntegerType::get(context, 128);
  value = IntegerAttr::get(intType, intValue);
  return success();
}

//===----------------------------------------------------------------------===//
// Substrait enums
//===----------------------------------------------------------------------===//

#include "substrait-mlir/Dialect/Substrait/IR/SubstraitEnums.cpp.inc"

//===----------------------------------------------------------------------===//
// Substrait interfaces
//===----------------------------------------------------------------------===//

#include "substrait-mlir/Dialect/Substrait/IR/SubstraitAttrInterfaces.cpp.inc"
#include "substrait-mlir/Dialect/Substrait/IR/SubstraitOpInterfaces.cpp.inc"
#include "substrait-mlir/Dialect/Substrait/IR/SubstraitTypeInterfaces.cpp.inc"

//===----------------------------------------------------------------------===//
// Custom Parser and Printer for Substrait
//===----------------------------------------------------------------------===//

bool parseCountAsAll(OpAsmParser &parser, IntegerAttr &count) {
  // `all` keyword (corresponds to `-1`).
  if (!parser.parseOptionalKeyword("all")) {
    count = parser.getBuilder().getI64IntegerAttr(-1);
    return false;
  }

  // Normal integer.
  int64_t result;
  if (!parser.parseInteger(result)) {
    count = parser.getBuilder().getI64IntegerAttr(result);
    return false;
  }

  // Error.
  return true;
}

void printCountAsAll(OpAsmPrinter &printer, Operation *op, IntegerAttr count) {
  if (count.getInt() == -1) {
    printer << "all";
    return;
  }
  // Normal integer.
  printer << count.getValue();
}

// Parses a VarCharType by extracting the length from the given parser. Assumes
// the length is surrounded by `<` and `>` symbols, which are removed. On
// success, assigns the parsed type to `type` and returns success.
ParseResult parseVarCharTypeByLength(AsmParser &parser, VarCharType &type) {
  // remove `<` and `>` symbols
  int64_t result;
  if (parser.parseInteger(result))
    return failure();

  type = VarCharType::get(parser.getContext(), result);

  return success();
}

// Prints the VarCharType by outputting its length to the given printer.
void printVarCharTypeByLength(AsmPrinter &printer, VarCharType type) {
  // Normal integer.
  printer << type.getLength();
}

ParseResult parseDecimalNumber(AsmParser &parser, DecimalType &type,
                               IntegerAttr &value) {
  llvm::SMLoc loc = parser.getCurrentLocation();

  // Parse decimal value as quoted string.
  std::string valueStr;
  if (parser.parseString(&valueStr))
    return failure();

  // Parse `P = <precision>`.
  uint32_t precision;
  if (parser.parseComma() || parser.parseKeyword("P") || parser.parseEqual() ||
      parser.parseInteger(precision))
    return failure();

  // Parse `S = <scale>`.
  uint32_t scale;
  if (parser.parseComma() || parser.parseKeyword("S") || parser.parseEqual() ||
      parser.parseInteger(scale))
    return failure();

  // Create `DecimalType`.
  auto emitError = [&]() { return parser.emitError(loc); };
  if (!(type = DecimalType::getChecked(emitError, parser.getContext(),
                                       precision, scale)))
    return failure();

  // Parse value as the given type.
  if (failed(DecimalAttr::parseDecimalString(emitError, valueStr, type, value)))
    return failure();

  return success();
}

void printDecimalNumber(AsmPrinter &printer, DecimalType type,
                        IntegerAttr value) {
  printer << "\"" << DecimalAttr::toDecimalString(type, value) << "\", ";
  printer << "P = " << type.getPrecision() << ", S = " << type.getScale();
}

ParseResult parseFixedBinaryLiteral(AsmParser &parser, StringAttr &value,
                                    FixedBinaryType &type) {
  std::string valueStr;
  // Parse fixed binary value as quoted string.
  if (parser.parseString(&valueStr))
    return failure();

  // Create `FixedBinaryType`.
  auto emitError = [&]() {
    return parser.emitError(parser.getCurrentLocation());
  };
  MLIRContext *context = parser.getContext();
  uint32_t length = valueStr.size();
  if (!(type = FixedBinaryType::getChecked(emitError, context, length)))
    return failure();

  value = parser.getBuilder().getStringAttr(valueStr);

  return success();
}

void printFixedBinaryLiteral(AsmPrinter &printer, StringAttr value,
                             FixedBinaryType type) {
  printer << value;
}

Type RelationType::parse(AsmParser &parser) {
  // Parse `<` literal.
  if (failed(parser.parseLess()))
    return Type();

  // If we parse the `>` literal, we have an empty list of types and are done.
  if (succeeded(parser.parseOptionalGreater()))
    return get(parser.getContext());

  // Parse list of field types.
  SmallVector<Type> fieldTypes;
  if (failed(parser.parseCommaSeparatedList([&]() {
        Type type;
        if (failed(parser.parseType(type)))
          return failure();
        fieldTypes.push_back(type);
        return success();
      })))
    return Type();

  // Parse `>` literal.
  if (failed(parser.parseGreater()))
    return Type();

  return get(parser.getContext(), fieldTypes);
}

void RelationType::print(AsmPrinter &printer) const {
  printer << "<";
  llvm::interleaveComma(getFieldTypes(), printer);
  printer << ">";
}

//===----------------------------------------------------------------------===//
// Substrait operations
//===----------------------------------------------------------------------===//

namespace mlir {
namespace substrait {

static ParseResult
parseAggregationDetails(OpAsmParser &parser,
                        AggregationPhaseAttr &aggregationPhase,
                        AggregationInvocationAttr &aggregationInvocation);
static void
printAggregationDetails(OpAsmPrinter &printer, CallOp op,
                        AggregationPhaseAttr aggregationPhase,
                        AggregationInvocationAttr aggregationInvocation);
static ParseResult parseAggregateRegions(OpAsmParser &parser,
                                         Region &groupingsRegion,
                                         Region &measuresRegion,
                                         ArrayAttr &groupingSetsAttr);
static void printAggregateRegions(OpAsmPrinter &printer, AggregateOp op,
                                  Region &groupingsRegion,
                                  Region &measuresRegion,
                                  ArrayAttr groupingSetsAttr);

} // namespace substrait
} // namespace mlir

#define GET_OP_CLASSES
#include "substrait-mlir/Dialect/Substrait/IR/SubstraitOps.cpp.inc"

namespace {

/// Computes the type of the nested field of the given `type` identified by
/// `position`. Each entry `n` in the given index array `position` corresponds
/// to the `n`-th entry in that level.
FailureOr<Type> computeTypeAtPosition(Location loc, Type type,
                                      ArrayRef<int64_t> position);

namespace impl {

/// Helper that extracts computes the type at a position given a container type.
template <typename ContainerType>
FailureOr<Type> computeTypeAtPositionHelper(Location loc,
                                            ContainerType containerType,
                                            ArrayRef<int64_t> position) {
  assert(!position.empty() && "expected to be called with non-empty position");

  // Recurse into fields of first index in position array.
  int64_t index = position[0];
  ArrayRef<Type> fieldTypes = containerType.getTypes();
  if (index >= static_cast<int64_t>(fieldTypes.size()) || index < 0)
    return emitError(loc) << index << " is not a valid index for "
                          << containerType;

  return ::computeTypeAtPosition(loc, fieldTypes[index], position.drop_front());
}
} // namespace impl

// Implementation of `computeTypeAtPosition`.
//
// The function is implemented corecursively with `computeTypeAtPositionHelper`,
// where each recursion level extracts the type of the outer-most level
// identified by the first index in the `position` array. In each level, this
// function handles the leaf case and type-switches into the helper, which is
// templated using the container type such that the extraction of the nested
// types can use the concrete container type.
FailureOr<Type> computeTypeAtPosition(Location loc, Type type,
                                      ArrayRef<int64_t> position) {
  if (position.empty())
    return type;

  return TypeSwitch<Type, FailureOr<Type>>(type)
      .Case<RelationType, TupleType>([&](auto type) {
        return impl::computeTypeAtPositionHelper(loc, type, position);
      })
      .Default([&](auto type) {
        return emitError(loc) << "can't extract element from type " << type;
      });
}

/// Verifies that the provided field names match the provided field types. While
/// the field types are potentially nested, the names are given in a single,
/// flat list and correspond to the field types in depth first order (where each
/// nested tuple-typed field has a name and its nested field have names on their
/// own). Furthermore, the names on each nesting level need to be unique. For
/// details, see
/// https://substrait.io/tutorial/sql_to_substrait/#types-and-schemas.
FailureOr<int> verifyNamedStructHelper(Location loc,
                                       llvm::ArrayRef<Attribute> fieldNames,
                                       TypeRange fieldTypes) {
  int numConsumedNames = 0;
  llvm::SmallSet<llvm::StringRef, 8> currentLevelNames;
  for (Type type : fieldTypes) {
    // Check name of current field.
    if (numConsumedNames >= static_cast<int>(fieldNames.size()))
      return emitError(loc, "not enough field names provided");
    auto currentName = llvm::cast<StringAttr>(fieldNames[numConsumedNames]);
    if (!currentLevelNames.insert(currentName).second)
      return emitError(loc, llvm::Twine("duplicate field name: '") +
                                currentName.getValue() + "'");
    numConsumedNames++;

    // Recurse for nested structs/tuples.
    if (auto tupleType = llvm::dyn_cast<TupleType>(type)) {
      llvm::ArrayRef<Type> nestedFieldTypes = tupleType.getTypes();
      llvm::ArrayRef<Attribute> remainingNames =
          fieldNames.drop_front(numConsumedNames);
      FailureOr<int> res =
          verifyNamedStructHelper(loc, remainingNames, nestedFieldTypes);
      if (failed(res))
        return failure();
      numConsumedNames += res.value();
    }
  }
  return numConsumedNames;
}

LogicalResult verifyNamedStruct(Operation *op,
                                llvm::ArrayRef<Attribute> fieldNames,
                                TupleType tupleType) {
  Location loc = op->getLoc();
  TypeRange fieldTypes = tupleType.getTypes();

  // Emits error message with context on failure.
  auto emitErrorMessage = [&]() {
    InFlightDiagnostic error = op->emitOpError()
                               << "has mismatching 'field_names' ([";
    llvm::interleaveComma(fieldNames, error);
    error << "]) and result type (" << tupleType << ")";
    return error;
  };

  // Call recursive verification function.
  FailureOr<int> numConsumedNames =
      verifyNamedStructHelper(loc, fieldNames, fieldTypes);

  // Relay any failure.
  if (failed(numConsumedNames))
    return emitErrorMessage();

  // If we haven't consumed all names, we got too many of them, so report.
  if (numConsumedNames.value() != static_cast<int>(fieldNames.size())) {
    InFlightDiagnostic error = emitErrorMessage();
    error.attachNote(loc) << "too many field names provided";
    return error;
  }

  return success();
}

} // namespace

namespace mlir {
namespace substrait {

ParseResult
parseAggregationDetails(OpAsmParser &parser,
                        AggregationPhaseAttr &aggregationPhase,
                        AggregationInvocationAttr &aggregationInvocation) {
  // This is essentially copied from `FieldParser<AggregationInvocation>` but
  // sets the default `all` case if no invocation type is present.

  MLIRContext *context = parser.getContext();
  std::string keyword;
  if (failed(parser.parseOptionalKeywordOrString(&keyword))) {
    // No keyword parsed --> use default value for both attributes.
    aggregationPhase =
        AggregationPhaseAttr::get(context, AggregationPhase::initial_to_result);
    aggregationInvocation =
        AggregationInvocationAttr::get(context, AggregationInvocation::all);
    return success();
  }

  // Try to symbolize the first keyword as aggregation phase.
  if (std::optional<AggregationPhase> attr =
          symbolizeAggregationPhase(keyword)) {
    // Success: use the symbolized value and read the next keyword.
    aggregationPhase = AggregationPhaseAttr::get(context, attr.value());
    if (failed(parser.parseOptionalKeywordOrString(&keyword))) {
      // If there is no other keyword, then we use the default value for the
      // invocation type and are done.
      aggregationInvocation =
          AggregationInvocationAttr::get(context, AggregationInvocation::all);
      return success();
    }
  } else {
    // If the symbolization as aggregation phase failed, set the default value.
    aggregationPhase =
        AggregationPhaseAttr::get(context, AggregationPhase::initial_to_result);
  }

  // If we arrive here, we have a parsed but not symbolized keyword that must be
  // the invocation type; otherwise it is invalid.

  // Try to symbolize the keyword as aggregation invocation.
  if (std::optional<AggregationInvocation> attr =
          symbolizeAggregationInvocation(keyword)) {
    aggregationInvocation =
        AggregationInvocationAttr::get(parser.getContext(), attr.value());
    return success();
  }

  // Symbolization failed.
  auto loc = parser.getCurrentLocation();
  return parser.emitError(loc)
         << "has invalid aggregate invocation type specification: " << keyword;
}

void printAggregationDetails(OpAsmPrinter &printer, CallOp op,
                             AggregationPhaseAttr aggregationPhase,
                             AggregationInvocationAttr aggregationInvocation) {
  if (!op.isAggregate())
    return;
  assert(aggregationPhase && aggregationInvocation &&
         "expected aggregate function to have 'aggregation_phase' and "
         "'aggregation_invocation' attributes");

  // Print each of the two keywords if they do not have their corresponding
  // default value. Also print the keyword if the other one has its
  // `unspecified` value: this avoids having only one keyword `unspecified`,
  // which would be ambiguous. Always start printing a white space because the
  // assembly format suppresses the whitespace before the aggregation details.

  // Print aggregation phase.
  if (aggregationPhase.getValue() != AggregationPhase::initial_to_result ||
      aggregationInvocation.getValue() == AggregationInvocation::unspecified) {
    printer << " " << aggregationPhase.getValue();
  }

  // Print aggregation invocation.
  if (aggregationInvocation.getValue() != AggregationInvocation::all ||
      aggregationPhase.getValue() == AggregationPhase::unspecified) {
    printer << " " << aggregationInvocation.getValue();
  }
}

ParseResult parseAggregateRegions(OpAsmParser &parser, Region &groupingsRegion,
                                  Region &measuresRegion,
                                  ArrayAttr &groupingSetsAttr) {
  MLIRContext *context = parser.getContext();

  // Parse `measures` and `groupings` regions as well as `grouping_sets` attr.
  bool hasMeasures = false;
  bool hasGroupings = false;
  bool hasGroupingSets = false;
  {
    auto ensureOneOccurrance = [&](bool &hasParsed,
                                   StringRef name) -> LogicalResult {
      if (hasParsed) {
        SMLoc loc = parser.getCurrentLocation();
        return parser.emitError(loc, llvm::Twine("can only have one ") + name);
      }
      hasParsed = true;
      return success();
    };

    StringRef keyword;
    while (succeeded(parser.parseOptionalKeyword(
        &keyword, {"measures", "groupings", "grouping_sets"}))) {
      if (keyword == "measures") {
        if (failed(ensureOneOccurrance(hasMeasures, "'measures' region")) ||
            failed(parser.parseRegion(measuresRegion)))
          return failure();
      } else if (keyword == "groupings") {
        if (failed(ensureOneOccurrance(hasGroupings, "'groupings' region")) ||
            failed(parser.parseRegion(groupingsRegion)))
          return failure();
      } else if (keyword == "grouping_sets") {
        if (failed(ensureOneOccurrance(hasGroupingSets,
                                       "'grouping_sets' attribute")) ||
            failed(parser.parseAttribute(groupingSetsAttr)))
          return failure();
      }
    }
  }

  // Create default value of `grouping_sets` attr if not provided.
  if (!hasGroupingSets) {
    // If there is no `groupings` region, create only the empty grouping set.
    if (!hasGroupings)
      groupingSetsAttr = ArrayAttr::get(context, ArrayAttr::get(context, {}));
    // Otherwise, create the grouping set with all grouping columns.
    else if (!groupingsRegion.empty()) {
      auto yieldOp =
          llvm::dyn_cast<YieldOp>(groupingsRegion.front().getTerminator());
      if (yieldOp) {
        unsigned numColumns = yieldOp->getNumOperands();
        SmallVector<int64_t> allColumns;
        llvm::append_range(allColumns, llvm::seq(0u, numColumns));
        IRRewriter rewriter(context);
        ArrayAttr allColumnsAttr = rewriter.getI64ArrayAttr(allColumns);
        groupingSetsAttr = rewriter.getArrayAttr({allColumnsAttr});
      }
    }
  }

  return success();
}

void printAggregateRegions(OpAsmPrinter &printer, AggregateOp op,
                           Region &groupingsRegion, Region &measuresRegion,
                           ArrayAttr groupingSetsAttr) {
  printer.increaseIndent();

  // `groupings` region.
  if (!groupingsRegion.empty()) {
    printer.printNewline();
    printer.printKeywordOrString("groupings");
    printer << " ";
    printer.printRegion(groupingsRegion);
  }

  // `grouping_sets` attribute.
  if (groupingSetsAttr.size() != 1) {
    // Note: A single grouping set is always of the form `seq(0, size)`.
    printer.printNewline();
    printer.printKeywordOrString("grouping_sets");
    printer << " ";
    printer.printAttribute(groupingSetsAttr);
  }

  // `measures` regions.
  if (!measuresRegion.empty()) {
    printer.printNewline();
    printer.printKeywordOrString("measures");
    printer << " ";
    printer.printRegion(measuresRegion);
  }

  printer.decreaseIndent();
}

void AggregateOp::build(OpBuilder &builder, OperationState &result, Value input,
                        ArrayAttr groupingSets, Region *groupings,
                        Region *measures) {

  MLIRContext *context = builder.getContext();
  auto loc = UnknownLoc::get(context);
  AggregateOp::Properties properties;
  properties.grouping_sets = groupingSets;
  SmallVector<Region *> regions = {groupings, measures};

  // Infer `returnTypes` from provided arguments. If that fails, then
  // `returnType` will be empty. The rest of this function will continue to
  // work, but the op that is built in the end will not verify and the
  // diagnostics of `inferReturnType` will have been emitted.
  SmallVector<mlir::Type> returnTypes;
  (void)AggregateOp::inferReturnTypes(context, loc, input, {},
                                      OpaqueProperties(&properties), regions,
                                      returnTypes);

  // Call existing `build` function and move bodies into the new regions.
  AggregateOp::build(builder, result, returnTypes, input, groupingSets,
                     /*advanced_extension=*/{});
  result.regions[0]->takeBody(*groupings);
  result.regions[1]->takeBody(*measures);
}

LogicalResult AggregateOp::inferReturnTypes(
    MLIRContext *context, std::optional<Location> loc, ValueRange operands,
    DictionaryAttr attributes, OpaqueProperties properties, RegionRange regions,
    llvm::SmallVectorImpl<Type> &inferredReturnTypes) {
  auto *typedProperties = properties.as<Properties *>();
  assert(typedProperties && "could not get typed properties");
  Region *groupings = regions[0];
  Region *measures = regions[1];
  SmallVector<Type> fieldTypes;
  if (!loc)
    loc = UnknownLoc::get(context);

  // The left-most output columns are the `groupings` columns, then the
  // `measures` columns.
  for (Region *region : {groupings, measures}) {
    if (region->empty())
      continue;
    auto yieldOp = llvm::cast<YieldOp>(region->front().getTerminator());
    llvm::append_range(fieldTypes, yieldOp.getOperandTypes());
  }

  // If there is more than one `grouping_set`, then we also have an additional
  // `si32` column for the grouping set ID.
  if (typedProperties->grouping_sets.size() > 1) {
    auto si32 = IntegerType::get(context, /*width=*/32, IntegerType::Signed);
    fieldTypes.push_back(si32);
  }

  // Build relation type from field types.
  auto resultType = RelationType::get(context, fieldTypes);
  inferredReturnTypes.push_back(resultType);

  return success();
}

LogicalResult AggregateOp::verifyRegions() {
  // Verify properties that need to hold for both regions.
  RelationType inputType = getInput().getType();
  TupleType inputTupleType = inputType.getStructType();
  for (auto [idx, region] : llvm::enumerate(getRegions())) {
    if (region->empty()) // Regions are allowed to be empty.
      continue;

    // Verify that the regions have the input struct as argument.
    if (region->getArgumentTypes() != TypeRange{inputTupleType})
      return emitOpError() << "has region #" << idx
                           << " with invalid argument types (expected: "
                           << inputTupleType
                           << ", got: " << region->getArgumentTypes() << ")";

    // Verify that at least one value is yielded.
    auto yieldOp = llvm::cast<YieldOp>(region->front().getTerminator());
    if (yieldOp->getNumOperands() == 0)
      return emitOpError()
             << "has region #" << idx
             << " that yields no values (use empty region instead)";
  }

  // Verify that the grouping sets refer to values yielded from `groupings`,
  // that all yielded values are referred to, and that the references are in the
  // correct order.
  {
    int64_t numGroupingColumns = 0;
    if (!getGroupings().empty()) {
      auto yieldOp =
          llvm::cast<YieldOp>(getGroupings().front().getTerminator());
      numGroupingColumns = yieldOp->getNumOperands();
    }

    // Check bounds, collect grouping columns.
    llvm::SmallSet<int64_t, 16> allGroupingRefs;
    for (auto [groupingSetIdx, groupingSet] :
         llvm::enumerate(getGroupingSets())) {
      for (auto [refIdx, refAttr] :
           llvm::enumerate(cast<ArrayAttr>(groupingSet))) {
        auto ref = cast<IntegerAttr>(refAttr).getInt();
        if (ref < 0 || ref >= numGroupingColumns)
          return emitOpError() << "has invalid grouping set #" << groupingSetIdx
                               << ": column reference " << ref << " (column #"
                               << refIdx << ") is out of bounds";
        auto [_, hasInserted] = allGroupingRefs.insert(ref);
        if (hasInserted &&
            ref != static_cast<int64_t>(allGroupingRefs.size() - 1))
          return emitOpError()
                 << "has invalid grouping sets: the first occerrences of the "
                    "column references must be densely increasing";
      }
    }

    // Check that all grouping columns are used.
    if (static_cast<int64_t>(allGroupingRefs.size()) != numGroupingColumns) {
      for (int64_t i : llvm::seq<int64_t>(0, numGroupingColumns)) {
        if (!allGroupingRefs.contains(i))
          return emitOpError() << "has 'groupings' region whose operand #" << i
                               << " is not contained in any 'grouping_set'";
      }
    }
  }

  // Verify that `measures` region yields only values produced by
  // `AggregateFunction`s.
  if (!getMeasures().empty()) {
    for (Value value : getMeasures().front().getTerminator()->getOperands()) {
      auto callOp = llvm::dyn_cast_or_null<CallOp>(value.getDefiningOp());
      if (!callOp || !callOp.isAggregate())
        return emitOpError() << "yields value from 'measures' region that was "
                                "not produced by an aggregate function: "
                             << value;
    }
  }

  if (getGroupings().empty() && getMeasures().empty())
    return emitOpError()
           << "one of 'groupings' or 'measures' must be specified";

  return success();
}

/// Implement `SymbolOpInterface`.
::mlir::LogicalResult
CallOp::verifySymbolUses(SymbolTableCollection &symbolTables) {
  if (!symbolTables.lookupNearestSymbolFrom<ExtensionFunctionOp>(
          *this, getCalleeAttr()))
    return emitOpError() << "refers to " << getCalleeAttr()
                         << ", which is not a valid 'extension_function' op";
  return success();
}

LogicalResult
CrossOp::inferReturnTypes(MLIRContext *context, std::optional<Location> loc,
                          ValueRange operands, DictionaryAttr attributes,
                          OpaqueProperties properties, RegionRange regions,
                          llvm::SmallVectorImpl<Type> &inferredReturnTypes) {
  Value leftInput = operands[0];
  Value rightInput = operands[1];

  TypeRange leftFieldTypes =
      cast<RelationType>(leftInput.getType()).getFieldTypes();
  TypeRange rightFieldTypes =
      cast<RelationType>(rightInput.getType()).getFieldTypes();

  SmallVector<mlir::Type> fieldTypes;
  llvm::append_range(fieldTypes, leftFieldTypes);
  llvm::append_range(fieldTypes, rightFieldTypes);
  auto resultType = RelationType::get(context, fieldTypes);

  inferredReturnTypes = SmallVector<Type>{resultType};

  return success();
}

OpFoldResult EmitOp::fold(FoldAdaptor adaptor) {
  MLIRContext *context = getContext();
  Type i64 = IntegerType::get(context, 64);

  // If the input is also an `emit`, fold it into this op.
  if (auto previousEmit =
          dyn_cast_or_null<EmitOp>(getInput().getDefiningOp())) {
    // Compute new mapping.
    ArrayAttr previousMapping = previousEmit.getMapping();
    SmallVector<Attribute> newMapping;
    newMapping.reserve(getMapping().size());
    for (auto attr : getMapping().getAsRange<IntegerAttr>()) {
      int64_t index = attr.getInt();
      int64_t newIndex = cast<IntegerAttr>(previousMapping[index]).getInt();
      newMapping.push_back(IntegerAttr::get(i64, newIndex));
    }

    // Update this op.
    setMappingAttr(ArrayAttr::get(context, newMapping));
    setOperand(previousEmit.getInput());
    return getResult();
  }

  // Remainder: fold away if the mapping is the identity mapping.

  // Return if the mapping is not the identity mapping.
  int64_t numFields = cast<RelationType>(getInput().getType()).size();
  int64_t numIndices = getMapping().size();
  if (numFields != numIndices)
    return {};
  for (int64_t i = 0; i < numIndices; ++i) {
    auto attr = getMapping()[i];
    int64_t index = cast<IntegerAttr>(attr).getInt();
    if (index != i)
      return {};
  }

  // The `emit` op *has* an identity mapping, so it does not have any effect.
  // Return its input instead.
  return getInput();
}

LogicalResult
EmitOp::inferReturnTypes(MLIRContext *context, std::optional<Location> loc,
                         ValueRange operands, DictionaryAttr attributes,
                         OpaqueProperties properties, RegionRange regions,
                         llvm::SmallVectorImpl<Type> &inferredReturnTypes) {
  auto *typedProperties = properties.as<Properties *>();
  if (!loc)
    loc = UnknownLoc::get(context);

  ArrayAttr mapping = typedProperties->getMapping();
  Type inputType = operands[0].getType();
  TypeRange inputTypes = mlir::cast<RelationType>(inputType).getFieldTypes();

  // Map input types to output types.
  SmallVector<Type> outputTypes;
  outputTypes.reserve(mapping.size());
  for (auto indexAttr : mapping.getAsRange<IntegerAttr>()) {
    int64_t index = indexAttr.getInt();
    if (index < 0 || index >= static_cast<int64_t>(inputTypes.size()))
      return ::emitError(loc.value())
             << index << " is not a valid index into " << inputType;
    Type mappedType = inputTypes[index];
    outputTypes.push_back(mappedType);
  }

  // Create final relation type.
  auto outputType = RelationType::get(context, outputTypes);
  inferredReturnTypes.push_back(outputType);

  return success();
}

LogicalResult ExtensionTableOp::verify() {
  llvm::ArrayRef<Attribute> fieldNames = getFieldNames().getValue();
  RelationType relationType = getResult().getType();
  TupleType tupleType = relationType.getStructType();
  return verifyNamedStruct(getOperation(), fieldNames, tupleType);
}

LogicalResult FieldReferenceOp::inferReturnTypes(
    MLIRContext *context, std::optional<Location> loc, ValueRange operands,
    DictionaryAttr attributes, OpaqueProperties properties, RegionRange regions,
    llvm::SmallVectorImpl<Type> &inferredReturnTypes) {
  auto *typedProperties = properties.as<Properties *>();
  if (!loc)
    loc = UnknownLoc::get(context);

  // Extract field type at given position.
  DenseI64ArrayAttr position = typedProperties->getPosition();
  Type inputType = operands[0].getType();
  FailureOr<Type> fieldType =
      computeTypeAtPosition(loc.value(), inputType, position);
  if (failed(fieldType))
    return ::emitError(loc.value())
           << "mismatching position and type (position: " << position
           << ", type: " << inputType << ")";

  inferredReturnTypes.push_back(fieldType.value());

  return success();
}

LogicalResult FilterOp::verifyRegions() {
  MLIRContext *context = getContext();
  Type si1 = IntegerType::get(context, /*width=*/1, IntegerType::Signed);
  Region &condition = getCondition();

  // Verify that type of yielded value is Boolean.
  auto yieldOp = llvm::cast<YieldOp>(condition.front().getTerminator());
  if (yieldOp.getValue().size() != 1)
    return emitOpError()
           << "must have 'condition' region yielding one value (yields "
           << yieldOp.getValue().size() << ")";

  Type yieldedType = yieldOp.getValue().getTypes()[0];
  if (yieldedType != si1)
    return emitOpError()
           << "must have 'condition' region yielding 'si1' (yields "
           << yieldedType << ")";

  // Verify that block has argument of input tuple type.
  RelationType relationType = getResult().getType();
  TupleType tupleType = relationType.getStructType();
  if (condition.getNumArguments() != 1 ||
      condition.getArgument(0).getType() != tupleType) {
    InFlightDiagnostic diag = emitOpError()
                              << "must have 'condition' region taking "
                              << tupleType << " as argument (takes ";
    if (condition.getNumArguments() == 0)
      diag << "no arguments)";
    else
      diag << condition.getArgument(0).getType() << ")";
    return diag;
  }

  return success();
}

OpFoldResult LiteralOp::fold(FoldAdaptor adaptor) { return getValue(); }

LogicalResult
LiteralOp::inferReturnTypes(MLIRContext *context, std::optional<Location> loc,
                            ValueRange operands, DictionaryAttr attributes,
                            OpaqueProperties properties, RegionRange regions,
                            llvm::SmallVectorImpl<Type> &inferredReturnTypes) {
  auto *typedProperties = properties.as<Properties *>();
  Attribute valueAttr = typedProperties->getValue();

  Type resultType = getAttrType(valueAttr);
  if (!resultType)
    return emitOptionalError(loc, "unsuited attribute for literal value: ",
                             typedProperties->getValue());

  inferredReturnTypes.emplace_back(resultType);
  return success();
}

LogicalResult
JoinOp::inferReturnTypes(MLIRContext *context, std::optional<Location> loc,
                         ValueRange operands, DictionaryAttr attributes,
                         OpaqueProperties properties, RegionRange regions,
                         llvm::SmallVectorImpl<Type> &inferredReturnTypes) {
  Value leftInput = operands[0];
  Value rightInput = operands[1];

  TypeRange leftFieldTypes =
      cast<RelationType>(leftInput.getType()).getFieldTypes();
  TypeRange rightFieldTypes =
      cast<RelationType>(rightInput.getType()).getFieldTypes();

  // Get accessor to `join_type`.
  Adaptor adaptor(operands, attributes, properties, regions);
  JoinType join_type = adaptor.getJoinType();

  SmallVector<mlir::Type> fieldTypes;

  switch (join_type) {
  case JoinType::unspecified:
  case JoinType::inner:
  case JoinType::outer:
  case JoinType::right:
  case JoinType::left:
    llvm::append_range(fieldTypes, leftFieldTypes);
    llvm::append_range(fieldTypes, rightFieldTypes);
    break;
  case JoinType::semi:
  case JoinType::anti:
    llvm::append_range(fieldTypes, leftFieldTypes);
    break;
  case JoinType::single:
    llvm::append_range(fieldTypes, rightFieldTypes);
    break;
  }

  auto resultType = RelationType::get(context, fieldTypes);

  inferredReturnTypes = SmallVector<Type>{resultType};

  return success();
}

LogicalResult NamedTableOp::verify() {
  llvm::ArrayRef<Attribute> fieldNames = getFieldNames().getValue();
  RelationType relationType = getResult().getType();
  TupleType tupleType = relationType.getStructType();
  return verifyNamedStruct(getOperation(), fieldNames, tupleType);
}

LogicalResult PlanRelOp::verifyRegions() {
  // Verify that we `yield` exactly one value.
  auto yieldOp = llvm::cast<YieldOp>(getBody().front().getTerminator());
  if (yieldOp.getValue().size() != 1)
    return emitOpError()
           << "must have 'body' region yielding one value (yields "
           << yieldOp.getValue().size() << ")";

  // Verify that the field names match the field types. If we don't have any,
  // we're done.
  if (!getFieldNames().has_value())
    return success();

  // Otherwise, use helper to verify.
  llvm::ArrayRef<Attribute> fieldNames = getFieldNames()->getValue();
  auto relationType = cast<RelationType>(yieldOp.getValue().getTypes()[0]);
  TupleType tupleType = relationType.getStructType();
  return verifyNamedStruct(getOperation(), fieldNames, tupleType);
}

OpFoldResult ProjectOp::fold(FoldAdaptor adaptor) {
  Operation *terminator = adaptor.getExpressions().front().getTerminator();

  // If the region does not yield any values, the the `project` has no effect.
  if (terminator->getNumOperands() == 0) {
    return getInput();
  }

  return {};
}

LogicalResult ProjectOp::verifyRegions() {
  // Verify that the expression block has a matching argument type.
  RelationType inputType = getInput().getType();
  TupleType inputTupleType = inputType.getStructType();
  TypeRange blockArgTypes = getExpressions().front().getArgumentTypes();
  if (blockArgTypes != TypeRange{inputTupleType})
    return emitOpError()
           << "has 'expressions' region with mismatching argument type"
           << " (has: " << blockArgTypes << ", expected: " << inputTupleType
           << ")";

  // Verify that the input field types are a prefix of the output field types.
  size_t numInputFields = inputType.size();
  auto outputType = llvm::cast<RelationType>(getResult().getType());
  ArrayRef<Type> outputPrefixTypes =
      outputType.getFieldTypes().take_front(numInputFields);

  if (inputType.getFieldTypes() != outputPrefixTypes)
    return emitOpError()
           << "has output field type whose prefix is different from "
           << "input field types (" << inputType.getFieldTypes() << " vs "
           << outputPrefixTypes << ")";

  // Verify that yielded operands have the same types as the new output fields.
  TypeRange newFieldTypes =
      outputType.getFieldTypes().drop_front(numInputFields);
  auto yieldOp = llvm::cast<YieldOp>(getExpressions().front().getTerminator());

  if (yieldOp.getOperandTypes() != newFieldTypes)
    return emitOpError()
           << "has output field type whose new fields are different from "
           << "the yielded operand types (" << newFieldTypes << " vs "
           << yieldOp.getOperandTypes() << ")";

  return success();
}

} // namespace substrait
} // namespace mlir

//===----------------------------------------------------------------------===//
// Substrait types and attributes
//===----------------------------------------------------------------------===//

#define GET_TYPEDEF_CLASSES
#include "substrait-mlir/Dialect/Substrait/IR/SubstraitOpsTypes.cpp.inc"

#define GET_ATTRDEF_CLASSES
#include "substrait-mlir/Dialect/Substrait/IR/SubstraitOpsAttrs.cpp.inc"
