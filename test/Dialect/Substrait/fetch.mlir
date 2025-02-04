// RUN: substrait-opt -split-input-file %s \
// RUN: | FileCheck %s

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:           %[[V0:.*]] = named_table
// CHECK-NEXT:      %[[V1:.*]] = fetch 5 offset 3 from %[[V0]] : <si32>
// CHECK-NEXT:      yield %[[V1]] : !substrait.relation<si32>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = fetch 5 offset 3 from %0 : <si32>
    yield %1 : !substrait.relation<si32>
  }
}

// -----

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:           %[[V0:.*]] = named_table
// CHECK-NEXT:      %[[V1:.*]] = fetch all offset 3 from %[[V0]] : <si32>
// CHECK-NEXT:      yield %[[V1]] : !substrait.relation<si32>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = fetch -1 offset 3 from %0 : <si32>
    yield %1 : !substrait.relation<si32>
  }
}

// -----

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:           %[[V0:.*]] = named_table
// CHECK-NEXT:      %[[V1:.*]] = fetch all offset 3 from %[[V0]] : <si32>
// CHECK-NEXT:      yield %[[V1]] : !substrait.relation<si32>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = fetch all offset 3 from %0 : <si32>
    yield %1 : !substrait.relation<si32>
  }
}

// -----

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:           %[[V0:.*]] = named_table
// CHECK-NEXT:      %[[V1:.*]] = fetch 3 from %[[V0]] : <si32>
// CHECK-NEXT:      yield %[[V1]] : !substrait.relation<si32>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = fetch 3 from %0 : <si32>
    yield %1 : !substrait.relation<si32>
  }
}

// -----

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:           %[[V0:.*]] = named_table
// CHECK-NEXT:      %[[V1:.*]] = fetch 3 from %[[V0]] : <si32>
// CHECK-NEXT:      yield %[[V1]] : !substrait.relation<si32>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = fetch 3 offset 0 from %0 : <si32>
    yield %1 : !substrait.relation<si32>
  }
}

// -----

// CHECK-LABEL: substrait.plan
// CHECK:           fetch 3 from %{{[^ ]*}}
// CHECK-SAME:        advanced_extension optimization = "\08*"
// CHECK-SAME:          : !substrait.any<"type.googleapis.com/google.protobuf.Int32Value">
// CHECK-SAME:        : <si32>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = fetch 3 from %0
            advanced_extension optimization = "\08*"
              : !substrait.any<"type.googleapis.com/google.protobuf.Int32Value">
            : <si32>
    yield %1 : !substrait.relation<si32>
  }
}
