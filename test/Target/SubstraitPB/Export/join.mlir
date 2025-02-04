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
// CHECK-NEXT:      join {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        left {
// CHECK-NEXT:          read {
// CHECK:             right {
// CHECK-NEXT:          read {
// CHECK-NOT:             op:

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = named_table @t2 as ["b"] : <si32>
    %2 = join unspecified %0, %1 : <si32>, <si32> -> <si32,si32>
    yield %2 : !substrait.relation<si32, si32>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      join {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        left {
// CHECK-NEXT:          read {
// CHECK:             right {
// CHECK-NEXT:          read {
// CHECK:             type: JOIN_TYPE_INNER

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = named_table @t2 as ["b"] : <si32>
    %2 = join inner %0, %1 : <si32>, <si32> -> <si32,si32>
    yield %2 : !substrait.relation<si32, si32>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      join {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        left {
// CHECK-NEXT:          read {
// CHECK:             right {
// CHECK-NEXT:          read {
// CHECK:             type: JOIN_TYPE_OUTER

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = named_table @t2 as ["b"] : <si32>
    %2 = join outer %0, %1 : <si32>, <si32> -> <si32,si32>
    yield %2 : !substrait.relation<si32, si32>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      join {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        left {
// CHECK-NEXT:          read {
// CHECK:             right {
// CHECK-NEXT:          read {
// CHECK:             type: JOIN_TYPE_LEFT

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = named_table @t2 as ["b"] : <si32>
    %2 = join left %0, %1 : <si32>, <si32> -> <si32,si32>
    yield %2 : !substrait.relation<si32, si32>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      join {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        left {
// CHECK-NEXT:          read {
// CHECK:             right {
// CHECK-NEXT:          read {
// CHECK:             type: JOIN_TYPE_RIGHT

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = named_table @t2 as ["b"] : <si32>
    %2 = join right %0, %1 : <si32>, <si32> -> <si32,si32>
    yield %2 : !substrait.relation<si32, si32>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      join {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        left {
// CHECK-NEXT:          read {
// CHECK:             right {
// CHECK-NEXT:          read {
// CHECK:             type: JOIN_TYPE_SEMI

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si1>
    %1 = named_table @t2 as ["b"] : <si32>
    %2 = join semi %0, %1 : <si1>, <si32> -> <si1>
    yield %2 : !substrait.relation<si1>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      join {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        left {
// CHECK-NEXT:          read {
// CHECK:             right {
// CHECK-NEXT:          read {
// CHECK:             type: JOIN_TYPE_ANTI

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si1>
    %1 = named_table @t2 as ["b"] : <si32>
    %2 = join anti %0, %1 : <si1>, <si32> -> <si1>
    yield %2 : !substrait.relation<si1>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      join {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        left {
// CHECK-NEXT:          read {
// CHECK:             right {
// CHECK-NEXT:          read {
// CHECK:             type: JOIN_TYPE_SINGLE

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = named_table @t2 as ["b"] : <si1>
    %2 = join single %0, %1 : <si32>, <si1> -> <si1>
    yield %2 : !substrait.relation<si1>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      join {
// CHECK:             advanced_extension {
// CHECK-NEXT:          optimization {
// CHECK-NEXT:            type_url: "type.googleapis.com/google.protobuf.Int32Value"
// CHECK-NEXT:            value: "\010*"
// CHECK-NEXT:          }

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = named_table @t2 as ["b"] : <si32>
    %2 = join inner %0, %1
            advanced_extension optimization = "\08*"
              : !substrait.any<"type.googleapis.com/google.protobuf.Int32Value">
            : <si32>, <si32> -> <si32, si32>
    yield %2 : !substrait.relation<si32, si32>
  }
}
