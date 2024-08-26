// RUN: structured-opt -split-input-file %s \
// RUN: | FileCheck %s

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = aggregate %[[V0]] : tuple<si32> -> tuple<si1, si1, si32, si1, si32>
// CHECK-NEXT:      groupings {
// CHECK-NEXT:        ^[[BB0:.*]](%[[ARG0:.*]]: tuple<si32>):
// CHECK-NEXT:        %[[V2:.*]] = literal 0 : si1
// CHECK-NEXT:        yield %[[V2]], %[[V2]] : si1, si1
// CHECK-NEXT:      }
// CHECK-NEXT:      grouping_sets {{\[}}[0], [0, 1], [1], []]
// CHECK-NEXT:      measures {
// CHECK-NEXT:      ^[[BB1:.*]](%[[ARG1:.*]]: tuple<si32>):
// CHECK-DAG:         %[[V3:.*]] = field_reference %[[ARG1]][0] : tuple<si32>
// CHECK-DAG:         %[[V4:.*]] = literal 0 : si1
// CHECK-NEXT:        yield %[[V3]], %[[V4]] : si32, si1
// CHECK-NEXT:      }
// CHECK-NEXT:      yield %[[V1]]

substrait.plan version 0 : 42 : 1 {
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
        yield %2, %3 : si32, si1
      }
    yield %1 : tuple<si1, si1, si32, si1, si32>
  }
}
