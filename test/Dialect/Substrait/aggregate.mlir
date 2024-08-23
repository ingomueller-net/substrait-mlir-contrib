// RUN: structured-opt -split-input-file %s \
// RUN: | FileCheck %s

// CHECK-LABEL: substrait.plan
// CHECK:         relation
// CHECK:         named_table
// CHECK-NEXT:    filter
// CHECK-NEXT:    (%[[ARG0:.*]]: tuple<si32>)
// CHECK-NEXT:      %[[V0:.*]] = field_reference %[[ARG0]]
// CHECK-NEXT:      %[[V1:.*]] = call @function(%[[V0]]) : (si32) -> si1
// CHECK-NEXT:      yield
// CHECK-NEXT:    }

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : tuple<si32>
    %1 = aggregate %0 : tuple<si32> -> tuple<si32, si32>
      grouping { yield }
      grouping { yield }
      measures { yield }
      grouping { yield }
    yield %1 : tuple<si32, si32>
  }
}
