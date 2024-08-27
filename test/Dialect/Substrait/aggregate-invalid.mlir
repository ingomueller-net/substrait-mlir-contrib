// RUN: structured-opt -verify-diagnostics -split-input-file %s

// Verify that wrong arg type to `groupings` is detected.

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : tuple<si32>
    // expected-error@+1 {{'substrait.aggregate' op has region #1 with invalid argument types (expected: 'tuple<si32>', got: 'tuple<si1>')}}
    %1 = aggregate %0 : tuple<si32> -> tuple<si1>
      groupings {
      ^bb0(%arg : tuple<si1>):
        %2 = literal 0 : si1
        yield %2 : si1
      }
    yield %1 : tuple<si1>
  }
}

// -----

// Verify that wrong arg type to `measures` is detected.

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : tuple<si32>
    // expected-error@+1 {{'substrait.aggregate' op has region #0 with invalid argument types (expected: 'tuple<si32>', got: 'tuple<si1>')}}
    %1 = aggregate %0 : tuple<si32> -> tuple<si1>
      measures {
      ^bb0(%arg : tuple<si1>):
        %2 = literal 0 : si1
        yield %2 : si1
      }
    yield %1 : tuple<si1>
  }
}

// -----

// Verify that out-of-bound column refs in grouping sets are detected.

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : tuple<si32>
    // expected-error@+1 {{'substrait.aggregate' op has invalid grouping set #0: column reference 1 (column #0) is out of bounds}}
    %1 = aggregate %0 : tuple<si32> -> tuple<si1>
      groupings {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si1
        yield %2 : si1
      }
      grouping_sets [[1]]
    yield %1 : tuple<si1>
  }
}

// -----

// Verify that first occurrences of column references are densely increasing

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : tuple<si32>
    // expected-error@+1 {{'substrait.aggregate' op has invalid grouping sets: the first occerrences of the column references must be densely increasing}}
    %1 = aggregate %0 : tuple<si32> -> tuple<si1, si1>
      groupings {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si1
        yield %2, %2 : si1, si1
      }
      grouping_sets [[1, 0]]
    yield %1 : tuple<si1, si1>
  }
}

// -----

// Verify that first occurrences of column references are densely increasing

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : tuple<si32>
    // expected-error@+1 {{'substrait.aggregate' op has 'groupings' region whose operand #1 is not contained in any 'grouping_set'}}
    %1 = aggregate %0 : tuple<si32> -> tuple<si1, si1>
      groupings {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si1
        yield %2, %2 : si1, si1
      }
      grouping_sets [[0]]
    yield %1 : tuple<si1, si1>
  }
}

// -----

// Verify that first occurrences of column references are densely increasing

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : tuple<si32>
    // expected-error@+2 {{one of 'groupings' or 'measures' must be specified}}
    // expected-error@+1 {{'substrait.aggregate' op failed to infer returned types}}
    %1 = aggregate %0 : tuple<si32> -> tuple<>
      grouping_sets [[]]
    yield %1 : tuple<>
  }
}
