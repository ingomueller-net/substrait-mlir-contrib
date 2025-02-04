// RUN: substrait-opt -verify-diagnostics -split-input-file %s

// Test error if providing too many names (1 name for 0 fields).
substrait.plan version 0 : 42 : 1 {
  relation {
    // expected-error@+2 {{'substrait.named_table' op has mismatching 'field_names' (["a"]) and result type ('tuple<>')}}
    // expected-note@+1 {{too many field names provided}}
    %0 = named_table @t1 as ["a"] : <>
    yield %0 : !substrait.relation<>
  }
}

// -----

// Test error if providing too few names (0 names for 1 field).
substrait.plan version 0 : 42 : 1 {
  relation {
    // expected-error@+2 {{'substrait.named_table' op has mismatching 'field_names' ([]) and result type ('tuple<si32>')}}
    // expected-error@+1 {{not enough field names provided}}
    %0 = named_table @t1 as [] : <si32>
    yield %0 : !substrait.relation<si32>
  }
}

// -----

// Test error if providing duplicate field names in the same nesting level.
substrait.plan version 0 : 42 : 1 {
  relation {
    // expected-error@+2 {{'substrait.named_table' op has mismatching 'field_names' (["a", "a"]) and result type ('tuple<si32, si32>')}}
    // expected-error@+1 {{duplicate field name: 'a'}}
    %0 = named_table @t1 as ["a", "a"] : <si32, si32>
    yield %0 : !substrait.relation<si32, si32>
  }
}
