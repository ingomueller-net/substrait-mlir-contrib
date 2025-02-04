// RUN: substrait-translate -substrait-to-protobuf --split-input-file %s \
// RUN: | FileCheck %s

// RUN: substrait-translate -substrait-to-protobuf %s \
// RUN:   --split-input-file --output-split-marker="# -----" \
// RUN: | substrait-translate -protobuf-to-substrait \
// RUN:   --split-input-file="# -----" --output-split-marker="// ""-----" \
// RUN: | substrait-translate -substrait-to-protobuf \
// RUN:   --split-input-file --output-split-marker="# -----" \
// RUN: | FileCheck %s

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      project {
// CHECK:             expressions {
// CHECK-NEXT:          cast {
// CHECK-NEXT:            type {
// CHECK-NEXT:              i64 {
// CHECK-NEXT:                nullability: NULLABILITY_REQUIRED
// CHECK-NEXT:              }
// CHECK-NEXT:            }
// CHECK-NEXT:            input {
// CHECK-NEXT:              literal {
// CHECK-NEXT:                i32: 42
// CHECK-NEXT:              }
// CHECK-NEXT:            }
// CHECK-NEXT:            failure_behavior: FAILURE_BEHAVIOR_RETURN_NULL
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        expressions {
// CHECK-NEXT:          cast {
// CHECK-NEXT:            type {
// CHECK-NEXT:              i64 {
// CHECK-NEXT:                nullability: NULLABILITY_REQUIRED
// CHECK-NEXT:              }
// CHECK-NEXT:            }
// CHECK-NEXT:            input {
// CHECK-NEXT:              literal {
// CHECK-NEXT:                i32: 42
// CHECK-NEXT:              }
// CHECK-NEXT:            }
// CHECK-NEXT:            failure_behavior: FAILURE_BEHAVIOR_THROW_EXCEPTION
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        expressions {
// CHECK-NEXT:          cast {
// CHECK-NEXT:            type {
// CHECK-NEXT:              i64 {
// CHECK-NEXT:                nullability: NULLABILITY_REQUIRED
// CHECK-NEXT:              }
// CHECK-NEXT:            }
// CHECK-NEXT:            input {
// CHECK-NEXT:              literal {
// CHECK-NEXT:                i32: 42
// CHECK-NEXT:              }
// CHECK-NEXT:            }
// CHECK-NEXT:          }
// CHECK-NEXT:        }

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = project %0 : <si32> -> <si32, si64, si64, si64> {
    ^bb0(%arg : tuple<si32>):
      %2 = literal 42 : si32
      %3 = cast %2 or return_null : si32 to si64
      %4 = cast %2 or throw_exception : si32 to si64
      %5 = cast %2 or unspecified : si32 to si64
      yield %3, %4, %5 : si64, si64, si64
    }
    yield %1 : !substrait.relation<si32, si64, si64, si64>
  }
}
