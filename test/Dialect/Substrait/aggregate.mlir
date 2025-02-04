// RUN: substrait-opt -split-input-file %s \
// RUN: | FileCheck %s

// Check complete op with all regions and attributes.

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = aggregate %[[V0]] : <si32> -> <si1, si1, si32, si32>
// CHECK-NEXT:      groupings {
// CHECK-NEXT:        ^[[BB0:.*]](%[[ARG0:.*]]: tuple<si32>):
// CHECK-NEXT:        %[[V2:.*]] = literal 0 : si1
// CHECK-NEXT:        yield %[[V2]], %[[V2]] : si1, si1
// CHECK-NEXT:      }
// CHECK-NEXT:      grouping_sets {{\[}}[0], [0, 1], [1], []]
// CHECK-NEXT:      measures {
// CHECK-NEXT:      ^[[BB1:.*]](%[[ARG1:.*]]: tuple<si32>):
// CHECK-DAG:         %[[V3:.*]] = field_reference %[[ARG1]][0]
// CHECK-DAG:         %[[V4:.*]] = call @function(%[[V3]]) aggregate :
// CHECK-NEXT:        yield %[[V4]] : si32
// CHECK-NEXT:      }
// CHECK-NEXT:      yield %[[V1]] :

substrait.plan version 0 : 42 : 1 {
  extension_uri @extension at "http://some.url/with/extensions.yml"
  extension_function @function at @extension["somefunc"]
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = aggregate %0 : <si32> -> <si1, si1, si32, si32>
      groupings {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si1
        yield %2, %2 : si1, si1
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

// Check complete op with different order.

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = aggregate %[[V0]]
// CHECK-NEXT:      groupings {
// CHECK:           }
// CHECK-NEXT:      grouping_sets
// CHECK-NEXT:      measures {
// CHECK-NEXT:      ^[[BB1:.*]](%[[ARG1:.*]]: tuple<si32>):
// CHECK-DAG:         %[[V3:.*]] = field_reference %[[ARG1]][0]
// CHECK-DAG:         %[[V4:.*]] = call @function(%[[V3]]) aggregate :
// CHECK-NEXT:        yield %[[V4]] : si32
// CHECK-NEXT:      }
// CHECK-NEXT:      yield %[[V1]]

substrait.plan version 0 : 42 : 1 {
  extension_uri @extension at "http://some.url/with/extensions.yml"
  extension_function @function at @extension["somefunc"]
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = aggregate %0 : <si32> -> <si1, si1, si32, si32>
      measures {
      ^bb0(%arg : tuple<si32>):
        %2 = field_reference %arg[0] : tuple<si32>
        %3 = call @function(%2) aggregate : (si32) -> si32
        yield %3 : si32
      }
      grouping_sets [[0], [0, 1], [1], []]
      groupings {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si1
        yield %2, %2 : si1, si1
      }
    yield %1 : !substrait.relation<si1, si1, si32, si32>
  }
}

// -----

// Check op without measures.

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = aggregate %[[V0]]
// CHECK-NEXT:      groupings {
// CHECK:           }
// CHECK-NEXT:      grouping_sets
// CHECK-NEXT:      yield %[[V1]]

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = aggregate %0 : <si32> -> <si1, si1, si32>
      groupings {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si1
        yield %2, %2 : si1, si1
      }
      grouping_sets [[0], [0, 1], [1], []]
    yield %1 : !substrait.relation<si1, si1, si32>
  }
}

// -----

// Check op with explicit single grouping_set.

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = aggregate %[[V0]]
// CHECK-NEXT:      groupings {
// CHECK:           }
// CHECK-NEXT:      yield %[[V1]]

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = aggregate %0 : <si32> -> <si1, si1>
      groupings {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si1
        yield %2, %2 : si1, si1
      }
      grouping_sets [[0, 1]]
    yield %1 : !substrait.relation<si1, si1>
  }
}

// -----

// Check op with implicit single grouping_set.

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = aggregate %[[V0]]
// CHECK-NEXT:      groupings {
// CHECK:           }
// CHECK-NEXT:      yield %[[V1]]

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = aggregate %0 : <si32> -> <si1, si1>
      groupings {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si1
        yield %2, %2 : si1, si1
      }
    yield %1 : !substrait.relation<si1, si1>
  }
}

// -----

// Check op without `grouping` and no grouping sets.

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = aggregate %[[V0]]
// CHECK-NEXT:      grouping_sets []
// CHECK-NEXT:      measures {
// CHECK:           }
// CHECK-NEXT:      yield %[[V1]]

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
        %3 = call @function(%2) aggregate : (si32) -> si32
        yield %3 : si32
      }
    yield %1 : !substrait.relation<si32>
  }
}

// -----

// Check op without `grouping` and implicit (empty) grouping set.

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = aggregate %[[V0]]
// CHECK-NEXT:      measures {
// CHECK:           }
// CHECK-NEXT:      yield %[[V1]]

substrait.plan version 0 : 42 : 1 {
  extension_uri @extension at "http://some.url/with/extensions.yml"
  extension_function @function at @extension["somefunc"]
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = aggregate %0 : <si32> -> <si32>
      measures {
      ^bb0(%arg : tuple<si32>):
        %2 = field_reference %arg[0] : tuple<si32>
        %3 = call @function(%2) aggregate : (si32) -> si32
        yield %3 : si32
      }
    yield %1 : !substrait.relation<si32>
  }
}

// -----

// Check op without `grouping` and explicit empty grouping set.

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = aggregate %[[V0]]
// CHECK-NEXT:      measures {
// CHECK:           }
// CHECK-NEXT:      yield %[[V1]]

substrait.plan version 0 : 42 : 1 {
  extension_uri @extension at "http://some.url/with/extensions.yml"
  extension_function @function at @extension["somefunc"]
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = aggregate %0 : <si32> -> <si32>
      grouping_sets [[]]
      measures {
      ^bb0(%arg : tuple<si32>):
        %2 = field_reference %arg[0] : tuple<si32>
        %3 = call @function(%2) aggregate : (si32) -> si32
        yield %3 : si32
      }
    yield %1 : !substrait.relation<si32>
  }
}

// -----

// Check combinations of aggregation details.

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = aggregate %[[V0]]
// CHECK-NEXT:      measures {
// CHECK-NEXT:      ^[[BB1:.*]](%[[ARG1:.*]]: tuple<si32>):
// CHECK-DAG:         %[[V2:.*]] = field_reference %[[ARG1]][0]
// CHECK-DAG:         %[[V3:.*]] = call @function(%[[V2]]) aggregate :
// CHECK-DAG:         %[[V4:.*]] = call @function(%[[V2]]) aggregate :
// CHECK-DAG:         %[[V5:.*]] = call @function(%[[V2]]) aggregate :
// CHECK-DAG:         %[[V6:.*]] = call @function(%[[V2]]) aggregate :
// CHECK-DAG:         %[[V7:.*]] = call @function(%[[V2]]) aggregate unspecified all :
// CHECK-DAG:         %[[V8:.*]] = call @function(%[[V2]]) aggregate initial_to_result unspecified :
// CHECK-DAG:         %[[V9:.*]] = call @function(%[[V2]]) aggregate unspecified all :
// CHECK-DAG:         %[[Va:.*]] = call @function(%[[V2]]) aggregate distinct :
// CHECK-DAG:         %[[Vb:.*]] = call @function(%[[V2]]) aggregate distinct :
// CHECK-DAG:         %[[Vc:.*]] = call @function(%[[V2]]) aggregate intermediate_to_result :
// CHECK-DAG:         %[[Vd:.*]] = call @function(%[[V2]]) aggregate intermediate_to_result :
// CHECK-DAG:         %[[Ve:.*]] = call @function(%[[V2]]) aggregate initial_to_intermediate :
// CHECK-DAG:         %[[Vf:.*]] = call @function(%[[V2]]) aggregate intermediate_to_intermediate :
// CHECK-NEXT:        yield %[[V3]], %[[V4]], %[[V5]], %[[V6]], %[[V7]], %[[V8]], %[[V9]], %[[Va]], %[[Vb]], %[[Vc]], %[[Vd]], %[[Ve]], %[[Vf]] :
// CHECK-NEXT:      }

substrait.plan version 0 : 42 : 1 {
  extension_uri @extension at "http://some.url/with/extensions.yml"
  extension_function @function at @extension["somefunc"]
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = aggregate %0 : <si32>
          -> <si32, si32, si32, si32, si32, si32, si32, si32, si32, si32, si32, si32, si32>
      measures {
      ^bb0(%arg : tuple<si32>):
        %2 = field_reference %arg[0] : tuple<si32>
        %3 = call @function(%2) aggregate : (si32) -> si32
        %4 = call @function(%2) aggregate all : (si32) -> si32
        %5 = call @function(%2) aggregate initial_to_result : (si32) -> si32
        %6 = call @function(%2) aggregate initial_to_result all : (si32) -> si32
        %7 = call @function(%2) aggregate unspecified all : (si32) -> si32
        %8 = call @function(%2) aggregate initial_to_result unspecified : (si32) -> si32
        %9 = call @function(%2) aggregate unspecified : (si32) -> si32
        %a = call @function(%2) aggregate distinct : (si32) -> si32
        %b = call @function(%2) aggregate initial_to_result distinct : (si32) -> si32
        %c = call @function(%2) aggregate intermediate_to_result : (si32) -> si32
        %d = call @function(%2) aggregate intermediate_to_result all : (si32) -> si32
        %e = call @function(%2) aggregate initial_to_intermediate : (si32) -> si32
        %f = call @function(%2) aggregate intermediate_to_intermediate : (si32) -> si32
        yield %3, %4, %5, %6, %7, %8, %9, %a, %b, %c, %d, %e, %f
              : si32, si32, si32, si32, si32, si32, si32,
                si32, si32, si32, si32, si32, si32
      }
    yield %1 : !substrait.relation<si32, si32, si32, si32, si32, si32, si32, si32, si32, si32, si32, si32, si32>
  }
}

// -----

// Check op with advanced extension.

// CHECK-LABEL: substrait.plan
// CHECK:           aggregate %{{.*}} advanced_extension optimization = "\08*"
// CHECK-SAME:          : !substrait.any<"type.googleapis.com/google.protobuf.Int32Value">
// CHECK-SAME:        : <si32> -> <si1>

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
