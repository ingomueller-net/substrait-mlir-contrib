// RUN: substrait-opt -split-input-file %s \
// RUN: | FileCheck %s

// CHECK-LABEL: substrait.plan
// CHECK-NEXT:    relation
// CHECK-NEXT:      %[[V0:.*]] = named_table
// CHECK-NEXT:      %[[V1:.*]] = emit [1, 0] from %[[V0]] :
// CHECK-SAME:          <si1, si32> -> <si32, si1>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a", "b"] : <si1, si32>
    %1 = emit [1, 0] from %0 : <si1, si32> -> <si32, si1>
    yield %1 : !substrait.relation<si32, si1>
  }
}

// -----

// CHECK-LABEL: substrait.plan
// CHECK-NEXT:    relation
// CHECK-NEXT:      %[[V0:.*]] = named_table
// CHECK-NEXT:      %[[V1:.*]] = emit [0, 0] from %[[V0]] :
// CHECK-SAME:          <si32> -> <si32, si32>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = emit [0, 0] from %0 : <si32> -> <si32, si32>
    yield %1 : !substrait.relation<si32, si32>
  }
}

// -----

// CHECK-LABEL: substrait.plan
// CHECK-NEXT:    relation
// CHECK-NEXT:      %[[V0:.*]] = named_table
// CHECK-NEXT:      %[[V1:.*]] = emit [1] from %[[V0]] : <si32, si1> -> <si1>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a", "b"] : <si32, si1>
    %1 = emit [1] from %0 : <si32, si1> -> <si1>
    yield %1 : !substrait.relation<si1>
  }
}
