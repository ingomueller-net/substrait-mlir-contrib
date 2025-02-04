// RUN: substrait-translate -substrait-to-protobuf --split-input-file %s \
// RUN: | FileCheck %s

// RUN: substrait-translate -substrait-to-protobuf %s \
// RUN:   --split-input-file --output-split-marker="# -----" \
// RUN: | substrait-translate -protobuf-to-substrait \
// RUN:   --split-input-file="# -----" --output-split-marker="// ""-----" \
// RUN: | substrait-translate -substrait-to-protobuf \
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
// CHECK-NEXT:             boolean: true
// CHECK-NEXT:           }
// CHECK-NEXT:         }
// CHECK-NEXT:       }
// CHECK-NEXT:       groupings {
// CHECK-NEXT:         grouping_expressions {
// CHECK-NEXT:           literal {
// CHECK-NEXT:             boolean: true
// CHECK-NEXT:           }
// CHECK-NEXT:         }
// CHECK-NEXT:       }
// CHECK-NEXT:       groupings {
// CHECK-NEXT:       }
// CHECK-NEXT:       measures {
// CHECK-NEXT:         measure {
// CHECK-NEXT:           phase: AGGREGATION_PHASE_INITIAL_TO_RESULT
// CHECK-NEXT:           output_type {
// CHECK-NEXT:             i32 {
// CHECK-NEXT:               nullability: NULLABILITY_REQUIRED
// CHECK-NEXT:             }
// CHECK-NEXT:           }
// CHECK-NEXT:           invocation: AGGREGATION_INVOCATION_ALL
// CHECK-NEXT:           arguments {
// CHECK-NEXT:             value {
// CHECK-NEXT:               selection {
// CHECK-NOT:                  measure
// CHECK:                    }
// CHECK:                  }
// CHECK:                }
// CHECK:              }
// CHECK:            }
// CHECK:          }
// CHECK:        }
// CHECK:      }
// CHECK:      version

substrait.plan version 0 : 42 : 1 {
  extension_uri @extension at "http://some.url/with/extensions.yml"
  extension_function @function at @extension["somefunc"]
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = aggregate %0 : <si32> -> <si1, si1, si32, si32>
      groupings {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si1
        %3 = literal -1 : si1
        yield %2, %3 : si1, si1
      }
      grouping_sets [[0], [0, 1], [1], []]
      measures {
      ^bb0(%arg : tuple<si32>):
        %2 = field_reference %arg[0] : tuple<si32>
        %3 = call @function(%2) aggregate : (si32) -> si32
        yield %3 : si32
      }
    yield %1 : !substrait.relation<si1, si1, si32, si32>
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
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = aggregate %0 : <si32> -> <si1>
      groupings {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si1
        yield %2 : si1
      }
      grouping_sets [[0]]
    yield %1 : !substrait.relation<si1>
  }
}

// -----

// Check op without `grouping` and no grouping sets.

// CHECK:      extension_uris {
// CHECK:      relations {
// CHECK-NEXT:   rel {
// CHECK-NEXT:     aggregate {
// CHECK-NOT:        groupings
// CHECK:      version

substrait.plan version 0 : 42 : 1 {
  extension_uri @extension at "http://some.url/with/extensions.yml"
  extension_function @function at @extension["somefunc"]
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = aggregate %0 : <si32> -> <si32>
      grouping_sets []
      measures {
      ^bb0(%arg : tuple<si32>):
        %2 = field_reference %arg[0] : tuple<si32>
        %4 = call @function(%2) aggregate : (si32) -> si32
        yield %4 : si32
      }
    yield %1 : !substrait.relation<si32>
  }
}

// -----

// Check op without `grouping` and (implicit) empty grouping set.

// CHECK:      extension_uris {
// CHECK:      relations {
// CHECK-NEXT:   rel {
// CHECK-NEXT:     aggregate {
// CHECK:          groupings {
// CHECK:      version

substrait.plan version 0 : 42 : 1 {
  extension_uri @extension at "http://some.url/with/extensions.yml"
  extension_function @function at @extension["somefunc"]
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = aggregate %0 : <si32> -> <si32>
      measures {
      ^bb0(%arg : tuple<si32>):
        %2 = field_reference %arg[0] : tuple<si32>
        %4 = call @function(%2) aggregate : (si32) -> si32
        yield %4 : si32
      }
    yield %1 : !substrait.relation<si32>
  }
}

// -----

// Check combinations of aggregate details.


// CHECK:      relations {
// CHECK:            measures {
// CHECK-NEXT:         measure {
// CHECK-NEXT:           phase: AGGREGATION_PHASE_INITIAL_TO_INTERMEDIATE
// CHECK-NOT:            measure
// CHECK:                invocation: AGGREGATION_INVOCATION_ALL
// CHECK-NEXT:           arguments {
// CHECK:              measure {
// CHECK-NEXT:           phase: AGGREGATION_PHASE_INTERMEDIATE_TO_INTERMEDIATE
// CHECK-NOT:            measure
// CHECK:                invocation: AGGREGATION_INVOCATION_ALL
// CHECK-NEXT:           arguments {
// CHECK:              measure {
// CHECK-NEXT:           phase: AGGREGATION_PHASE_INTERMEDIATE_TO_RESULT
// CHECK-NOT:            measure
// CHECK:                invocation: AGGREGATION_INVOCATION_ALL
// CHECK-NEXT:           arguments {
// CHECK:              measure {
// CHECK-NOT:            phase
// CHECK-NOT:            measure
// CHECK:                invocation: AGGREGATION_INVOCATION_ALL
// CHECK-NEXT:           arguments {
// CHECK:              measure {
// CHECK-NEXT:           phase: AGGREGATION_PHASE_INITIAL_TO_RESULT
// CHECK-NOT:            measure
// CHECK:                invocation: AGGREGATION_INVOCATION_ALL
// CHECK-NEXT:           arguments {
// CHECK:              measure {
// CHECK-NEXT:           phase: AGGREGATION_PHASE_INITIAL_TO_RESULT
// CHECK-NOT:            invocation
// CHECK:              measure {
// CHECK-NEXT:           phase: AGGREGATION_PHASE_INITIAL_TO_RESULT
// CHECK-NOT:            measure
// CHECK:                invocation: AGGREGATION_INVOCATION_DISTINCT
// CHECK-NEXT:           arguments {
// CHECK-NOT:            measure

substrait.plan version 0 : 42 : 1 {
  extension_uri @extension at "http://some.url/with/extensions.yml"
  extension_function @function at @extension["somefunc"]
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = aggregate %0 : <si32>
          -> <si32, si32, si32, si32, si32, si32, si32>
      measures {
      ^bb0(%arg : tuple<si32>):
        %2 = field_reference %arg[0] : tuple<si32>
        %3 = call @function(%2) aggregate initial_to_intermediate all : (si32) -> si32
        %4 = call @function(%2) aggregate intermediate_to_intermediate all : (si32) -> si32
        %5 = call @function(%2) aggregate intermediate_to_result all : (si32) -> si32
        %6 = call @function(%2) aggregate unspecified all : (si32) -> si32
        %7 = call @function(%2) aggregate initial_to_result all : (si32) -> si32
        %8 = call @function(%2) aggregate initial_to_result unspecified : (si32) -> si32
        %9 = call @function(%2) aggregate initial_to_result distinct : (si32) -> si32
        yield %3, %4, %5, %6, %7, %8, %9
              : si32, si32, si32, si32, si32, si32, si32
      }
    yield %1 : !substrait.relation<si32, si32, si32, si32, si32, si32, si32>
  }
}

// -----

// Check op with advanced extension.

// CHECK:      relations {
// CHECK:        rel {
// CHECK:          aggregate {
// CHECK:            groupings {
// CHECK:            advanced_extension {
// CHECK-NEXT:         optimization {
// CHECK-NEXT:           type_url: "type.googleapis.com/google.protobuf.Int32Value"
// CHECK-NEXT:           value: "\010*"
// CHECK-NEXT:         }
// CHECK-NEXT:       }

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = aggregate %0
            advanced_extension optimization = "\08*"
              : !substrait.any<"type.googleapis.com/google.protobuf.Int32Value">
            : <si32> -> <si1>
      groupings {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si1
        yield %2 : si1
      }
    yield %1 : !substrait.relation<si1>
  }
}
