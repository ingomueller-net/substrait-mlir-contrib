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
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        input {
// CHECK-NEXT:          read {
// CHECK:             expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:            boolean: true
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:             i32: 42

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

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      project {
// CHECK-NEXT:        common {
// CHECK:             input {
// CHECK:             advanced_extension {
// CHECK-NEXT:        optimization {
// CHECK-NEXT:          type_url: "type.googleapis.com/google.protobuf.Int32Value"
// CHECK-NEXT:          value: "\010*"
// CHECK-NEXT:        }
// CHECK-NEXT:      }

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
