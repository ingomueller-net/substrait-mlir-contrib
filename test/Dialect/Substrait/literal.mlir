// RUN: substrait-opt -split-input-file %s \
// RUN: | FileCheck %s

// CHECK:      = substrait.literal #substrait.decimal<"1234567.89", P = 9, S = 2>
// CHECK-NEXT: = substrait.literal #substrait.decimal<"0.123", P = 3, S = 3>
// CHECK-NEXT: = substrait.literal #substrait.decimal<"0.123", P = 5, S = 3>
// CHECK-NEXT: = substrait.literal #substrait.decimal<"0.123", P = 3, S = 3>
// CHECK-NEXT: = substrait.literal #substrait.decimal<"0.012", P = 3, S = 3>
// CHECK-NEXT: = substrait.literal #substrait.decimal<"12.0", P = 3, S = 0>
// CHECK-NEXT: = substrait.literal #substrait.decimal<"123.0", P = 3, S = 0>
// CHECK-NEXT: = substrait.literal #substrait.decimal<"123.0", P = 3, S = 0>
// CHECK-NEXT: = substrait.literal #substrait.decimal<"1111111111111111111111111111111111113.2", P = 38, S = 1>
// CHECK-NEXT: = substrait.literal #substrait.decimal<"0.12", P = 3, S = 3>
// CHECK-NEXT: = substrait.literal #substrait.decimal<"12.0345", P = 20, S = 10>
// CHECK-NEXT: = substrait.literal #substrait.decimal<"1234.0", P = 20, S = 10>
// CHECK-NEXT: = substrait.literal #substrait.decimal<"1234.0", P = 20, S = 10>

%0 = substrait.literal #substrait.decimal<"1234567.89", P = 9, S = 2>
%1 = substrait.literal #substrait.decimal<"0.123", P = 3, S = 3>
%2 = substrait.literal #substrait.decimal<"0.123", P = 5, S = 3>
%3 = substrait.literal #substrait.decimal<"000.123", P = 3, S = 3>
%4 = substrait.literal #substrait.decimal<"0.012", P = 3, S = 3>
%5 = substrait.literal #substrait.decimal<"012.0", P = 3, S = 0>
%6 = substrait.literal #substrait.decimal<"123.0", P = 3, S = 0>
%7 = substrait.literal #substrait.decimal<"123.000", P = 3, S = 0>
%8 = substrait.literal #substrait.decimal<"1111111111111111111111111111111111113.2", P = 38, S = 1>
%9 = substrait.literal #substrait.decimal<"0.12", P = 3, S = 3>
%a = substrait.literal #substrait.decimal<"12.0345", P = 20, S = 10>
%b = substrait.literal #substrait.decimal<"1234.0", P = 20, S = 10>
%c = substrait.literal #substrait.decimal<"00001234.0000", P = 20, S = 10>

// -----

// CHECK:      substrait.plan version 0 : 42 : 1 {
// CHECK-NEXT:   relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = project %[[V0]] : rel<si1> -> rel<si1, fixed_binary<10>> {
// CHECK-NEXT:    ^[[BB0:.*]](%[[ARG0:.*]]: tuple<si1>):
// CHECK-NEXT:      %[[V2:.*]] = literal #substrait.fixed_binary<"8181818181">
// CHECK-NEXT:      yield %[[V2]] : fixed_binary<10>
// CHECK-NEXT:    }
// CHECK-NEXT:    yield %[[V1]] : rel<si1, fixed_binary<10>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : rel<si1>
    %1 = project %0 : rel<si1> -> rel<si1, fixed_binary<10>> {
    ^bb0(%arg : tuple<si1>):
      %bytes = literal #substrait.fixed_binary<"8181818181">
      yield %bytes : fixed_binary<10>
    }
    yield %1 : rel<si1, fixed_binary<10>>
  }
}

// -----

// CHECK:      substrait.plan version 0 : 42 : 1 {
// CHECK-NEXT:   relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = project %[[V0]] : rel<si1> -> rel<si1, var_char<6>> {
// CHECK-NEXT:    ^[[BB0:.*]](%[[ARG0:.*]]: tuple<si1>):
// CHECK-NEXT:      %[[V2:.*]] = literal #substrait.var_char<"hello", 6>
// CHECK-NEXT:      yield %[[V2]] : var_char<6>
// CHECK-NEXT:    }
// CHECK-NEXT:    yield %[[V1]] : rel<si1, var_char<6>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : rel<si1>
    %1 = project %0 : rel<si1> -> rel<si1, var_char<6>> {
    ^bb0(%arg : tuple<si1>):
      %var_char = literal #substrait.var_char<"hello", 6>
      yield %var_char : var_char<6>
    }
    yield %1 : rel<si1, var_char<6>>
  }
}

// -----

// CHECK:      substrait.plan version 0 : 42 : 1 {
// CHECK-NEXT:   relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = project %[[V0]] : rel<si1> -> rel<si1, fixed_char<5>> {
// CHECK-NEXT:    ^[[BB0:.*]](%[[ARG0:.*]]: tuple<si1>):
// CHECK-NEXT:      %[[V2:.*]] = literal #substrait.fixed_char<"hello">
// CHECK-NEXT:      yield %[[V2]] : fixed_char<5>
// CHECK-NEXT:    }
// CHECK-NEXT:    yield %[[V1]] : rel<si1, fixed_char<5>>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : rel<si1>
    %1 = project %0 : rel<si1> -> rel<si1, fixed_char<5>> {
    ^bb0(%arg : tuple<si1>):
      %fixed_char = literal #substrait.fixed_char<"hello">
      yield %fixed_char : fixed_char<5>
    }
    yield %1 : rel<si1, fixed_char<5>>
  }
}

// -----

// CHECK:      substrait.plan version 0 : 42 : 1 {
// CHECK-NEXT:   relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = project %[[V0]] : rel<si1> -> rel<si1, uuid> {
// CHECK-NEXT:    ^[[BB0:.*]](%[[ARG0:.*]]: tuple<si1>):
// CHECK-NEXT:      %[[V2:.*]] = literal #substrait.uuid<1000000000 : i128>
// CHECK-NEXT:      yield %[[V2]] : uuid
// CHECK-NEXT:    }
// CHECK-NEXT:    yield %[[V1]] : rel<si1, uuid>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : rel<si1>
    %1 = project %0 : rel<si1> -> rel<si1, uuid> {
    ^bb0(%arg : tuple<si1>):
      %uuid = literal #substrait.uuid<1000000000 : i128>
      yield %uuid : uuid
    }
    yield %1 : rel<si1, uuid>
  }
}

// -----
// CHECK:      substrait.plan version 0 : 42 : 1 {
// CHECK-NEXT:   relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = project %[[V0]] : rel<si1> -> rel<si1, interval_ym, interval_ds> {
// CHECK-NEXT:    ^[[BB0:.*]](%[[ARG0:.*]]: tuple<si1>):
// CHECK-NEXT:      %[[V2:.*]] = literal #substrait.interval_year_month<2024y 1m>{{$}}
// CHECK-NEXT:      %[[V3:.*]] = literal #substrait.interval_day_second<9d 8000s>{{$}}
// CHECK-NEXT:      yield %[[V2]], %[[V3]] : interval_ym, interval_ds
// CHECK-NEXT:    }
// CHECK-NEXT:    yield %[[V1]] : rel<si1, interval_ym, interval_ds>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : rel<si1>
    %1 = project %0 : rel<si1> -> rel<si1, interval_ym, interval_ds> {
    ^bb0(%arg : tuple<si1>):
      %interval_year_month = literal #substrait.interval_year_month<2024y 1m>
      %interval_day_second = literal #substrait.interval_day_second<9d 8000s>
      yield %interval_year_month, %interval_day_second : interval_ym, interval_ds
    }
    yield %1 : rel<si1, interval_ym, interval_ds>
  }
}

// -----

// CHECK:      substrait.plan version 0 : 42 : 1 {
// CHECK-NEXT:   relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = project %[[V0]] : rel<si1> -> rel<si1, time> {
// CHECK-NEXT:    ^[[BB0:.*]](%[[ARG0:.*]]: tuple<si1>):
// CHECK-NEXT:      %[[V2:.*]] = literal #substrait.time<200000000us>{{$}}
// CHECK-NEXT:      yield %[[V2]] : time
// CHECK-NEXT:    }
// CHECK-NEXT:    yield %[[V1]] : rel<si1, time>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : rel<si1>
    %1 = project %0 : rel<si1> -> rel<si1, time> {
    ^bb0(%arg : tuple<si1>):
      %time = literal #substrait.time<200000000us>
      yield %time : time
    }
    yield %1 : rel<si1, time>
  }
}

// -----

// CHECK:      substrait.plan version 0 : 42 : 1 {
// CHECK-NEXT:   relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = project %[[V0]] : rel<si1> -> rel<si1, date> {
// CHECK-NEXT:    ^[[BB0:.*]](%[[ARG0:.*]]: tuple<si1>):
// CHECK-NEXT:      %[[V2:.*]] = literal #substrait.date<200000000>{{$}}
// CHECK-NEXT:      yield %[[V2]] : date
// CHECK-NEXT:    }
// CHECK-NEXT:    yield %[[V1]] : rel<si1, date>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : rel<si1>
    %1 = project %0 : rel<si1> -> rel<si1, date> {
    ^bb0(%arg : tuple<si1>):
      %date = literal #substrait.date<200000000>
      yield %date : date
    }
    yield %1 : rel<si1, date>
  }
}

// -----

// CHECK:      substrait.plan version 0 : 42 : 1 {
// CHECK-NEXT:   relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = project %[[V0]] : rel<si1> -> rel<si1, timestamp, timestamp_tz> {
// CHECK-NEXT:    ^[[BB0:.*]](%[[ARG0:.*]]: tuple<si1>):
// CHECK-NEXT:      %[[V2:.*]] = literal #substrait.timestamp<10000000000us>{{$}}
// CHECK-NEXT:      %[[V3:.*]] = literal #substrait.timestamp_tz<10000000000us>{{$}}
// CHECK-NEXT:      yield %[[V2]], %[[V3]] : timestamp, timestamp_tz
// CHECK-NEXT:    }
// CHECK-NEXT:    yield %[[V1]] : rel<si1, timestamp, timestamp_tz>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : rel<si1>
    %1 = project %0 : rel<si1> -> rel<si1, timestamp, timestamp_tz> {
    ^bb0(%arg : tuple<si1>):
      %timestamp = literal #substrait.timestamp<10000000000us>
      %timestamp_tz = literal #substrait.timestamp_tz<10000000000us>
      yield %timestamp, %timestamp_tz : timestamp, timestamp_tz
    }
    yield %1 : rel<si1, timestamp, timestamp_tz>
  }
}

// -----

// CHECK:      substrait.plan version 0 : 42 : 1 {
// CHECK-NEXT:   relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = project %[[V0]] : rel<si1> -> rel<si1, binary> {
// CHECK-NEXT:    ^[[BB0:.*]](%[[ARG0:.*]]: tuple<si1>):
// CHECK-NEXT:      %[[V2:.*]] = literal "4,5,6,7" : !substrait.binary
// CHECK-NEXT:      yield %[[V2]] : binary
// CHECK-NEXT:    }
// CHECK-NEXT:    yield %[[V1]] : rel<si1, binary>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : rel<si1>
    %1 = project %0 : rel<si1> -> rel<si1, binary> {
    ^bb0(%arg : tuple<si1>):
      %bytes = literal "4,5,6,7" : !substrait.binary
      yield %bytes : binary
    }
    yield %1 : rel<si1, binary>
  }
}

// -----

// CHECK:      substrait.plan version 0 : 42 : 1 {
// CHECK-NEXT:   relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = project %[[V0]] : rel<si1> -> rel<si1, string> {
// CHECK-NEXT:    ^[[BB0:.*]](%[[ARG0:.*]]: tuple<si1>):
// CHECK-NEXT:      %[[V2:.*]] = literal "hi" : !substrait.string
// CHECK-NEXT:      yield %[[V2]] : string
// CHECK-NEXT:    }
// CHECK-NEXT:    yield %[[V1]] : rel<si1, string>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : rel<si1>
    %1 = project %0 : rel<si1> -> rel<si1, string> {
    ^bb0(%arg : tuple<si1>):
      %hi = literal "hi" : !substrait.string
      yield %hi : string
    }
    yield %1 : rel<si1, string>
  }
}

// -----

// CHECK:      substrait.plan version 0 : 42 : 1 {
// CHECK-NEXT:   relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = project %[[V0]] : rel<f32> -> rel<f32, f32, f64> {
// CHECK-NEXT:    ^[[BB0:.*]](%[[ARG0:.*]]: tuple<f32>):
// CHECK-NEXT:      %[[V2:.*]] = literal 3.535000e+01 : f32
// CHECK-NEXT:      %[[V3:.*]] = literal 4.242000e+01 : f64
// CHECK-NEXT:      yield %[[V2]], %[[V3]] : f32, f64
// CHECK-NEXT:    }
// CHECK-NEXT:    yield %[[V1]] : rel<f32, f32, f64>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : rel<f32>
    %1 = project %0 : rel<f32> -> rel<f32, f32, f64> {
    ^bb0(%arg : tuple<f32>):
      %35 = literal 35.35 : f32
      %42 = literal 42.42 : f64
      yield %35, %42 : f32, f64
    }
    yield %1 : rel<f32, f32, f64>
  }
}

// -----

// CHECK:      substrait.plan version 0 : 42 : 1 {
// CHECK-NEXT:   relation
// CHECK:         %[[V0:.*]] = named_table
// CHECK-NEXT:    %[[V1:.*]] = project %[[V0]] : rel<si1> -> rel<si1, si1, si8, si16, si32, si64> {
// CHECK-NEXT:    ^[[BB0:.*]](%[[ARG0:.*]]: tuple<si1>):
// CHECK-NEXT:      %[[V2:.*]] = literal 0 : si1
// CHECK-NEXT:      %[[V3:.*]] = literal 2 : si8
// CHECK-NEXT:      %[[V4:.*]] = literal -1 : si16
// CHECK-NEXT:      %[[V5:.*]] = literal 35 : si32
// CHECK-NEXT:      %[[V6:.*]] = literal 42 : si64
// CHECK-NEXT:      yield %[[V2]], %[[V3]], %[[V4]], %[[V5]], %[[V6]] : si1, si8, si16, si32, si64
// CHECK-NEXT:    }
// CHECK-NEXT:    yield %[[V1]] : rel<si1, si1, si8, si16, si32, si64>

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : rel<si1>
    %1 = project %0 : rel<si1> -> rel<si1, si1, si8, si16, si32, si64> {
    ^bb0(%arg : tuple<si1>):
      %false = literal 0 : si1
      %2 = literal 2 : si8
      %-1 = literal -1 : si16
      %35 = literal 35 : si32
      %42 = literal 42 : si64
      yield %false, %2, %-1, %35, %42 : si1, si8, si16, si32, si64
    }
    yield %1 : rel<si1, si1, si8, si16, si32, si64>
  }
}
