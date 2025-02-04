// RUN: substrait-opt -verify-diagnostics -split-input-file %s

// Verify that wrong arg type to `groupings` is detected.

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    // expected-error@+1 {{'substrait.aggregate' op has region #0 with invalid argument types (expected: 'tuple<si32>', got: 'tuple<si1>')}}
    %1 = aggregate %0 : <si32> -> <si1>
      groupings {
      ^bb0(%arg : tuple<si1>):
        %2 = literal 0 : si1
        yield %2 : si1
      }
    yield %1 : !substrait.relation<si1>
  }
}

// -----

// Verify that wrong arg type to `measures` is detected.

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    // expected-error@+1 {{'substrait.aggregate' op has region #1 with invalid argument types (expected: 'tuple<si32>', got: 'tuple<si1>')}}
    %1 = aggregate %0 : <si32> -> <si1>
      measures {
      ^bb0(%arg : tuple<si1>):
        %2 = literal 0 : si1
        %3 = call @function(%2) aggregate : (si1) -> si1
        yield %3 : si1
      }
    yield %1 : !substrait.relation<si1>
  }
}

// -----

// Verify that out-of-bound column refs in grouping sets are detected.

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    // expected-error@+1 {{'substrait.aggregate' op has invalid grouping set #0: column reference 1 (column #0) is out of bounds}}
    %1 = aggregate %0 : <si32> -> <si1>
      groupings {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si1
        yield %2 : si1
      }
      grouping_sets [[1]]
    yield %1 : !substrait.relation<si1>
  }
}

// -----

// Verify that it's detected if first occurrences of column references are not
// densely increasing.

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    // expected-error@+1 {{'substrait.aggregate' op has invalid grouping sets: the first occerrences of the column references must be densely increasing}}
    %1 = aggregate %0 : <si32> -> <si1, si1>
      groupings {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si1
        yield %2, %2 : si1, si1
      }
      grouping_sets [[1, 0]]
    yield %1 : !substrait.relation<si1, si1>
  }
}

// -----

// Verify that yielded value unused by all grouping sets is detected.

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    // expected-error@+1 {{'substrait.aggregate' op has 'groupings' region whose operand #1 is not contained in any 'grouping_set'}}
    %1 = aggregate %0 : <si32> -> <si1, si1>
      groupings {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si1
        yield %2, %2 : si1, si1
      }
      grouping_sets [[0]]
    yield %1 : !substrait.relation<si1, si1>
  }
}

// -----

// Verify that missing `groupings` *and* `measures regions is detected.

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    // expected-error@+1 {{one of 'groupings' or 'measures' must be specified}}
    %1 = aggregate %0 : <si32> -> <>
      grouping_sets [[]]
    yield %1 : !substrait.relation<>
  }
}

// -----

// Verify that unaggregated measure is detected.

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    // expected-error-re@+1 {{'substrait.aggregate' op yields value from 'measures' region that was not produced by an aggregate function: {{.*}}substrait.call{{.*}}}}
    %1 = aggregate %0 : <si32> -> <si32>
      measures {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si32
        %3 = call @function(%2) : (si32) -> si32
        yield %3 : si32
      }
    yield %1 : !substrait.relation<si32>
  }
}

// -----

// Verify that invalid aggregation invocation mode is detected.

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    %1 = aggregate %0 : <si32> -> <si32>
      measures {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si32
        // expected-error@+1 {{custom op 'substrait.call' has invalid aggregate invocation type specification: foo}}
        %3 = call @function(%2) aggregate foo : (si32) -> si32
        yield %3 : si32
      }
    yield %1 : !substrait.relation<si32>
  }
}

// -----

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    // expected-error@+1 {{'substrait.aggregate' op has region #1 that yields no values (use empty region instead)}}
    %1 = aggregate %0 : <si32> -> <>
      measures {
      ^bb0(%arg : tuple<si32>):
        yield
      }
    yield %1 : !substrait.relation<>
  }
}

// -----

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si32>
    // expected-error@+1 {{'substrait.aggregate' op has region #0 that yields no values (use empty region instead)}}
    %1 = aggregate %0 : <si32> -> <>
      groupings {
      ^bb0(%arg : tuple<si32>):
        yield
      }
    yield %1 : !substrait.relation<>
  }
}
