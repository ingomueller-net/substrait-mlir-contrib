// RUN: structured-translate -substrait-to-protobuf --split-input-file %s \
// RUN: | FileCheck %s

// RUN: structured-translate -substrait-to-protobuf %s \
// RUN:   --split-input-file --output-split-marker="# -----" \
// RUN: | structured-translate -protobuf-to-substrait \
// RUN:   --split-input-file="# -----" --output-split-marker="// ""-----" \
// RUN: | structured-translate -substrait-to-protobuf \
// RUN:   --split-input-file --output-split-marker="# -----" \
// RUN: | FileCheck %s

// Check complete op with all regions and attributes.

// CHECK:      extension_uris {
// CHECK-NEXT:   uri: "http://some.url/with/extensions.yml"
// CHECK-NEXT: }
// CHECK-NEXT: extensions {
// CHECK-NEXT:   extension_function {
// CHECK-NEXT:     name: "somefunc"
// CHECK-NEXT:   }
// CHECK:      relations {
// CHECK-NEXT:   rel {
// CHECK-NEXT:     aggregate {
// CHECK:            input {
// CHECK:            groupings {
// CHECK-NEXT:         grouping_expressions {
// CHECK-NEXT:           literal {
// CHECK-NEXT:             boolean: false
// CHECK-NEXT:           }
// CHECK-NEXT:         }
// CHECK-NEXT:       }
// CHECK-NEXT:       groupings {
// CHECK-NEXT:         grouping_expressions {
// CHECK-NEXT:           literal {
// CHECK-NEXT:             boolean: false
// CHECK-NEXT:           }
// CHECK-NEXT:         }
// CHECK-NEXT:         grouping_expressions {
// CHECK-NEXT:           literal {
// CHECK-NEXT:             boolean: false
// CHECK-NEXT:           }
// CHECK-NEXT:         }
// CHECK-NEXT:       }
// CHECK-NEXT:       groupings {
// CHECK-NEXT:         grouping_expressions {
// CHECK-NEXT:           literal {
// CHECK-NEXT:             boolean: false
// CHECK-NEXT:           }
// CHECK-NEXT:         }
// CHECK-NEXT:       }
// CHECK-NEXT:       groupings {
// CHECK-NEXT:       }
// CHECK-NEXT:       measures {
// CHECK-NEXT:         measure {
// CHECK-NEXT:           output_type {
// CHECK-NEXT:             i32 {
// CHECK-NEXT:               nullability: NULLABILITY_REQUIRED
// CHECK-NEXT:             }
// CHECK-NEXT:           }
// CHECK-NEXT:           arguments {
// CHECK-NEXT:             value {
// CHECK-NEXT:               selection {
// CHECK-NOT:                  measure
// CHECK:                    }
// CHECK:                  }
// CHECK:                }
// CHECK:              }
// CHECK:            }
// CHECK:            measures {
// CHECK-NEXT:         measure {
// CHECK-NEXT:           output_type {
// CHECK-NEXT:             bool {
// CHECK-NEXT:               nullability: NULLABILITY_REQUIRED
// CHECK-NEXT:             }
// CHECK-NEXT:           }
// CHECK-NEXT:           arguments {
// CHECK-NEXT:             value {
// CHECK-NEXT:               literal {
// CHECK-NEXT:                 boolean: false
// CHECK-NEXT:               }
// CHECK-NEXT:             }
// CHECK-NEXT:           }
// CHECK-NEXT:         }
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:   }
// CHECK-NEXT: }
// CHECK:      version

substrait.plan version 0 : 42 : 1 {
  extension_uri @extension at "http://some.url/with/extensions.yml"
  extension_function @function at @extension["somefunc"]
  relation {
    %0 = named_table @t1 as ["a"] : tuple<si32>
    %1 = aggregate %0 : tuple<si32> -> tuple<si1, si1, si32, si1, si32>
      groupings {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si1
        yield %2, %2 : si1, si1
      }
      grouping_sets [[0], [0, 1], [1], []]
      measures {
      ^bb0(%arg : tuple<si32>):
        %2 = field_reference %arg[0] : tuple<si32>
        %3 = literal 0 : si1
        %4 = call @function(%2) aggregate : (si32) -> si32
        %5 = call @function(%3) aggregate unspecified : (si1) -> si1
        yield %4, %5 : si32, si1
      }
    yield %1 : tuple<si1, si1, si32, si1, si32>
  }
}

// -----

// Check op without measures.

// CHECK:      relations {
// CHECK-NEXT:   rel {
// CHECK-NEXT:     aggregate {
// CHECK:            input {
// CHECK:            groupings {
// CHECK-NOT:        measures
// CHECK:      version

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : tuple<si32>
    %1 = aggregate %0 : tuple<si32> -> tuple<si1, si1, si32>
      groupings {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si1
        yield %2, %2 : si1, si1
      }
      grouping_sets [[0], [0, 1], [1], []]
    yield %1 : tuple<si1, si1, si32>
  }
}

// -----

// Check op without `grouping` and empty grouping set and invocation modes.


// CHECK:      extension_uris {
// CHECK:      relations {
// CHECK-NEXT:   rel {
// CHECK-NEXT:     aggregate {
// CHECK:            input {
// CHECK:            groupings {
// CHECK-NEXT:       }
// CHECK-NEXT:       measures {
// CHECK-NEXT:        measure {
// CHECK-NOT:           measure
// CHECK:               invocation: AGGREGATION_INVOCATION_ALL
// CHECK:             measure {
// CHECK-NOT:           measure
// CHECK:               invocation: AGGREGATION_INVOCATION_DISTINCT
// CHECK:      version

substrait.plan version 0 : 42 : 1 {
  extension_uri @extension at "http://some.url/with/extensions.yml"
  extension_function @function at @extension["somefunc"]
  relation {
    %0 = named_table @t1 as ["a"] : tuple<si32>
    %1 = aggregate %0 : tuple<si32> -> tuple<si32, si1>
      grouping_sets [[]]
      measures {
      ^bb0(%arg : tuple<si32>):
        %2 = field_reference %arg[0] : tuple<si32>
        %3 = literal 0 : si1
        %4 = call @function(%2) aggregate all : (si32) -> si32
        %5 = call @function(%3) aggregate distinct : (si1) -> si1
        yield %4, %5 : si32, si1
      }
    yield %1 : tuple<si32, si1>
  }
}

// -----

