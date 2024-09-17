// RUN: structured-translate -verify-diagnostics -split-input-file %s \
// RUN:   -substrait-to-protobuf

// The groupings aren't unique after CSE. This has a different meaning once
// exported to protobuf.

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : tuple<si32>
    // expected-error@+1 {{'substrait.aggregate' op cannot be exported: values yielded from 'groupings' region are not all distinct after CSE}}
    %1 = aggregate %0 : tuple<si32> -> tuple<si1, si1>
      groupings {
      ^bb0(%arg : tuple<si32>):
        %2 = literal 0 : si1
        %3 = literal 0 : si1
        yield %2, %3 : si1, si1
      }
    yield %1 : tuple<si1, si1>
  }
}
