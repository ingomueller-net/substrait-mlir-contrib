// RUN: substrait-opt -split-input-file %s \
// RUN: | FileCheck %s

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:           %[[V0:.*]] = extension_table
// CHECK-SAME:         "\08*" : !substrait.any<"type.googleapis.com/google.protobuf.Int32Value">
// CHECK-SAME:        as ["a"] : <si32>
// CHECK-NEXT:      yield %[[V0]] : !substrait.relation<si32>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = extension_table
           "\08*" : !substrait.any<"type.googleapis.com/google.protobuf.Int32Value">
           as ["a"] : <si32>
    yield %0 : !substrait.relation<si32>
  }
}

// -----

// CHECK-LABEL: substrait.plan
// CHECK:           extension_table
// CHECK-SAME:        "some detail" : !substrait.any<"some url"> as ["a"]
// CHECK-SAME:        advanced_extension optimization = "\08*"
// CHECK-SAME:          : !substrait.any<"type.googleapis.com/google.protobuf.Int32Value">
// CHECK-SAME:        : <si32>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = extension_table
           "some detail" : !substrait.any<"some url"> as ["a"]
            advanced_extension optimization = "\08*"
              : !substrait.any<"type.googleapis.com/google.protobuf.Int32Value">
            : <si32>
    yield %0 : !substrait.relation<si32>
  }
}
