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
// CHECK-NEXT:      set {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        inputs {
// CHECK-NEXT:          read {
// CHECK:             inputs {
// CHECK-NEXT:          read {
// CHECK-NOT:             op:

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = named_table @t2 as ["b"] : <si32>
    %2 = set unspecified %0, %1 : <si32>
    yield %2 : !substrait.relation<si32>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      set {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        inputs {
// CHECK-NEXT:          read {
// CHECK:             inputs {
// CHECK-NEXT:          read {
// CHECK:             op: SET_OP_MINUS_PRIMARY

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = named_table @t2 as ["b"] : <si32>
    %2 = set minus_primary %0, %1 : <si32>
    yield %2 : !substrait.relation<si32>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      set {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        inputs {
// CHECK-NEXT:          read {
// CHECK:             inputs {
// CHECK-NEXT:          read {
// CHECK:             op: SET_OP_MINUS_MULTISET

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = named_table @t2 as ["b"] : <si32>
    %2 = set minus_multiset %0, %1 : <si32>
    yield %2 : !substrait.relation<si32>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      set {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        inputs {
// CHECK-NEXT:          read {
// CHECK:             inputs {
// CHECK-NEXT:          read {
// CHECK:             op: SET_OP_INTERSECTION_PRIMARY

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = named_table @t2 as ["b"] : <si32>
    %2 = set intersection_primary %0, %1 : <si32>
    yield %2 : !substrait.relation<si32>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      set {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        inputs {
// CHECK-NEXT:          read {
// CHECK:             inputs {
// CHECK-NEXT:          read {
// CHECK:             op: SET_OP_INTERSECTION_MULTISET

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = named_table @t2 as ["b"] : <si32>
    %2 = set intersection_multiset %0, %1 : <si32>
    yield %2 : !substrait.relation<si32>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      set {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        inputs {
// CHECK-NEXT:          read {
// CHECK:             inputs {
// CHECK-NEXT:          read {
// CHECK:             op: SET_OP_UNION_DISTINCT

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = named_table @t2 as ["b"] : <si32>
    %2 = set union_distinct %0, %1 : <si32>
    yield %2 : !substrait.relation<si32>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      set {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        inputs {
// CHECK-NEXT:          read {
// CHECK:             inputs {
// CHECK-NEXT:          read {
// CHECK:             op: SET_OP_UNION_ALL

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = named_table @t2 as ["b"] : <si32>
    %2 = set union_all %0, %1 : <si32>
    yield %2 : !substrait.relation<si32>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      set {
// CHECK:             advanced_extension {
// CHECK-NEXT:          optimization {
// CHECK-NEXT:            type_url: "type.googleapis.com/google.protobuf.Int32Value"
// CHECK-NEXT:            value: "\010*"
// CHECK-NEXT:          }

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = named_table @t2 as ["b"] : <si32>
    %2 = set union_all %0, %1
            advanced_extension optimization = "\08*"
              : !substrait.any<"type.googleapis.com/google.protobuf.Int32Value">
            : <si32>
    yield %2 : !substrait.relation<si32>
  }
}
