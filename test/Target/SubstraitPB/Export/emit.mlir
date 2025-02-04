// RUN: substrait-translate -substrait-to-protobuf --split-input-file %s \
// RUN: | FileCheck %s

// RUN: substrait-translate -substrait-to-protobuf %s \
// RUN:   --split-input-file --output-split-marker="# -----" \
// RUN: | substrait-translate -protobuf-to-substrait \
// RUN:   --split-input-file="# -----" --output-split-marker="// ""-----" \
// RUN: | substrait-translate -substrait-to-protobuf \
// RUN:   --split-input-file --output-split-marker="# -----" \
// RUN: | FileCheck %s

// Checks that the `emit` field of a `crosss` is exported correctly.

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      cross {
// CHECK-NEXT:        common {
// CHECK-NEXT:          emit {
// CHECK-NEXT:            output_mapping: 1
// CHECK-NEXT:            output_mapping: 0
// CHECK-NEXT:          }

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = cross %0 x %0 : <si32> x <si32>
    %2 = emit [1, 0] from %1 : <si32, si32> -> <si32, si32>
    yield %2 : !substrait.relation<si32, si32>
  }
}

// -----

// Checks that the `emit` field of a `named_table` is exported correctly.

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      read {
// CHECK-NEXT:        common {
// CHECK-NEXT:          emit {
// CHECK-NEXT:            output_mapping: 1
// CHECK-NEXT:          }

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a", "b"] : <si32, si1>
    %1 = emit [1] from %0 : <si32, si1> -> <si1>
    yield %1 : !substrait.relation<si1>
  }
}

// -----

// Checks that the `emit` field of a `named_table` is exported correctly.

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      filter {
// CHECK-NEXT:        common {
// CHECK-NEXT:          emit {
// CHECK-NEXT:            output_mapping: 1
// CHECK-NEXT:          }
// CHECK-LABEL:         input {
// CHECK-NEXT:            read {
// CHECK-NEXT:              common {
// CHECK-NEXT:                direct

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a", "b"] : <si32, si1>
    %1 = filter %0 : <si32, si1> {
    ^bb0(%arg : tuple<si32, si1>):
      %2 = literal -1 : si1
      yield %2 : si1
    }
    %2 = emit [1] from %1 : <si32, si1> -> <si1>
    yield %2 : !substrait.relation<si1>
  }
}
