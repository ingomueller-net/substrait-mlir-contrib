// RUN: substrait-opt -split-input-file %s \
// RUN: | FileCheck %s

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = filter %[[V0]] : <si32> {
// CHECK-NEXT:    ^[[BB0:.*]](%[[ARG0:.*]]: tuple<si32>):
// CHECK-NEXT:      %[[V2:.*]] = literal -1 : si1
// CHECK-NEXT:      yield %[[V2]] : si1
// CHECK-NEXT:    }
// CHECK-NEXT:    yield %[[V1]] :

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = filter %0 : <si32> {
    ^bb0(%arg : tuple<si32>):
      %2 = literal -1 : si1
      yield %2 : si1
    }
    yield %1 : !substrait.relation<si32>
  }
}

// -----

// CHECK-LABEL: substrait.plan
// CHECK:         filter %{{.*}} advanced_extension optimization = "\08*"
// CHECK-SAME:          : !substrait.any<"type.googleapis.com/google.protobuf.Int32Value">
// CHECK-SAME:        : <si32> {

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = filter %0
            advanced_extension optimization = "\08*"
              : !substrait.any<"type.googleapis.com/google.protobuf.Int32Value">
            : <si32> {
    ^bb0(%arg : tuple<si32>):
      %2 = literal -1 : si1
      yield %2 : si1
    }
    yield %1 : !substrait.relation<si32>
  }
}
