// RUN: substrait-opt -split-input-file %s \
// RUN: | FileCheck %s

// CHECK:      substrait.plan version 0 : 42 : 1 {
// CHECK-NEXT:   relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = project %[[V0]] :
// CHECK-NEXT:    ^[[BB0:.*]](%[[ARG0:.*]]: tuple<si32>):
// CHECK-NEXT:      %[[V2:.*]] = literal 42 : si32
// CHECK-NEXT:      %[[V3:.*]] = cast %[[V2]] or return_null : si32 to si64
// CHECK-NEXT:      %[[V4:.*]] = cast %[[V2]] or throw_exception : si32 to si64
// CHECK-NEXT:      %[[V5:.*]] = cast %[[V2]] or unspecified : si32 to si64
// CHECK-NEXT:      yield %[[V3]], %[[V4]], %[[V5]] : si64, si64, si64
// CHECK-NEXT:    }
// CHECK-NEXT:    yield %[[V1]] :

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
