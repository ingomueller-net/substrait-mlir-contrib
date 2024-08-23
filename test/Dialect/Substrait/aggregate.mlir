// RUN: structured-opt -split-input-file %s \
// RUN: | FileCheck %s

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = aggregate %[[V0]] : tuple<si32> -> tuple<si32, si32>
// CHECK:           grouping
// CHECK:           grouping
// CHECK:           measures

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : tuple<si32>
    %1 = aggregate %0 : tuple<si32> -> tuple<si32, si32>
      grouping {
      ^bb0(%arg : tuple<si32>):
        yield
      }
      measures {
      ^bb0(%arg : tuple<si32>):
        yield
      }
      grouping {
      ^bb0(%arg : tuple<si32>):
        yield
      }
    yield %1 : tuple<si32, si32>
  }
}
