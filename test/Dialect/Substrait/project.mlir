// RUN: substrait-opt -split-input-file %s \
// RUN: | FileCheck %s

// CHECK:      substrait.plan version 0 : 42 : 1 {
// CHECK-NEXT:   relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = project %[[V0]] : <si32> -> <si32, si1, si32> {
// CHECK-NEXT:    ^[[BB0:.*]](%[[ARG0:.*]]: tuple<si32>):
// CHECK-NEXT:      %[[V2:.*]] = literal -1 : si1
// CHECK-NEXT:      %[[V3:.*]] = literal 42 : si32
// CHECK-NEXT:      yield %[[V2]], %[[V3]] : si1, si32
// CHECK-NEXT:    }
// CHECK-NEXT:    yield %[[V1]] :

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = project %0 : <si32> -> <si32, si1, si32> {
    ^bb0(%arg : tuple<si32>):
      %true = literal -1 : si1
      %42 = literal 42 : si32
      yield %true, %42 : si1, si32
    }
    yield %1 : !substrait.relation<si32, si1, si32>
  }
}

// -----

// CHECK:      substrait.plan
// CHECK-NEXT:   relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = project %[[V0]] : <si32> -> <si32> {
// CHECK-NEXT:    ^[[BB0:.*]](%[[ARG0:.*]]: tuple<si32>):
// CHECK-NEXT:    }

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = project %0 : <si32> -> <si32> {
    ^bb0(%arg0: tuple<si32>):
      yield
    }
    yield %1 : !substrait.relation<si32>
  }
}

// -----

// CHECK:      substrait.plan version
// CHECK-NEXT:   relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = project %[[V0]]
// CHECK-SAME:      advanced_extension optimization = "\08*" :
// CHECK-SAME:        !substrait.any<"type.googleapis.com/google.protobuf.Int32Value">
// CHECK-SAME:      <si32> -> <si32> {

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = project %0
            advanced_extension optimization = "\08*"
              : !substrait.any<"type.googleapis.com/google.protobuf.Int32Value">
            : <si32> -> <si32> {
    ^bb0(%arg0: tuple<si32>):
      yield
    }
    yield %1 : !substrait.relation<si32>
  }
}
