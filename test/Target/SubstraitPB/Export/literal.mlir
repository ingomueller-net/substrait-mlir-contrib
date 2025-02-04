// RUN: substrait-translate -substrait-to-protobuf --split-input-file %s \
// RUN: | FileCheck %s

// RUN: substrait-translate -substrait-to-protobuf %s \
// RUN:   --split-input-file --output-split-marker="# -----" \
// RUN: | substrait-translate -protobuf-to-substrait \
// RUN:   --split-input-file="# -----" --output-split-marker="// ""-----" \
// RUN: | substrait-translate -substrait-to-protobuf \
// RUN:   --split-input-file --output-split-marker="# -----" \
// RUN: | FileCheck %s

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      project {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        input {
// CHECK-NEXT:          read {
// CHECK:             expressions {
// CHECK-NEXT:         literal {
// CHECK-NEXT:           decimal {
// CHECK-NEXT:             value: "\005\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
// CHECK-NEXT:             precision: 9
// CHECK-NEXT:             scale: 2

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si1>
    %1 = project %0 : <si1> -> <si1, !substrait.decimal<9, 2>> {
    ^bb0(%arg : tuple<si1>):
      %hi = literal #substrait.decimal<"0.05", P = 9, S = 2>
      yield %hi : !substrait.decimal<9, 2>
    }
    yield %1 : !substrait.relation<si1, !substrait.decimal<9, 2>>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      project {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        input {
// CHECK-NEXT:          read {
// CHECK:             expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:            fixed_binary: "8181818181"

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si1>
    %1 = project %0 : <si1> -> <si1, !substrait.fixed_binary<10>> {
    ^bb0(%arg : tuple<si1>):
      %fixed_binary = literal #substrait.fixed_binary<"8181818181">
      yield %fixed_binary : !substrait.fixed_binary<10>
    }
    yield %1 : !substrait.relation<si1, !substrait.fixed_binary<10>>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      project {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        input {
// CHECK-NEXT:          read {
// CHECK:             expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:           var_char {
// CHECK-NEXT:            value: "hello"
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:       }

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si1>
    %1 = project %0 : <si1> -> <si1, !substrait.var_char<6>> {
    ^bb0(%arg0: tuple<si1>):
      %2 = literal #substrait.var_char<"hello", 6>
      yield %2 : !substrait.var_char<6>
    }
    yield %1 : !substrait.relation<si1, !substrait.var_char<6>>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      project {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        input {
// CHECK-NEXT:          read {
// CHECK:             expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:            fixed_char: "hello"
// CHECK-NEXT:          }
// CHECK-NEXT:        }

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si1>
    %1 = project %0 : <si1> -> <si1, !substrait.fixed_char<5>> {
    ^bb0(%arg0: tuple<si1>):
      %2 = literal #substrait.fixed_char<"hello">
      yield %2 : !substrait.fixed_char<5>
    }
    yield %1 : !substrait.relation<si1, !substrait.fixed_char<5>>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      project {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        input {
// CHECK-NEXT:          read {
// CHECK:             expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:            uuid: "\000\312\232;\000\000\000\000\000\000\000\000\000\000\000\000"

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si1>
    %1 = project %0 : <si1> -> <si1, !substrait.uuid> {
    ^bb0(%arg : tuple<si1>):
      %uuid = literal #substrait.uuid<1000000000 : i128>
      yield %uuid : !substrait.uuid
    }
    yield %1 : !substrait.relation<si1, !substrait.uuid>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      project {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        input {
// CHECK-NEXT:          read {
// CHECK:             expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:            interval_year_to_month {
// CHECK-NEXT:              years: 2024
// CHECK-NEXT:              months: 1
// CHECK-NEXT:            }
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:            interval_day_to_second {
// CHECK-NEXT:              days: 9
// CHECK-NEXT:              seconds: 8000

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si1>
    %1 = project %0 : <si1> -> <si1, !substrait.interval_year_month, !substrait.interval_day_second> {
    ^bb0(%arg : tuple<si1>):
      %interval_year_month = literal #substrait.interval_year_month<2024y 1m>
      %interval_day_second = literal #substrait.interval_day_second<9d 8000s>
      yield %interval_year_month, %interval_day_second : !substrait.interval_year_month, !substrait.interval_day_second
    }
    yield %1 : !substrait.relation<si1, !substrait.interval_year_month, !substrait.interval_day_second>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      project {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        input {
// CHECK-NEXT:          read {
// CHECK:             expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:            time: 200000000
// CHECK-NEXT:          }
// CHECK-NEXT:        }

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si1>
    %1 = project %0 : <si1> -> <si1, !substrait.time> {
    ^bb0(%arg : tuple<si1>):
      %time = literal #substrait.time<200000000us>
      yield %time : !substrait.time
    }
    yield %1 : !substrait.relation<si1, !substrait.time>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      project {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        input {
// CHECK-NEXT:          read {
// CHECK:             expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:            date: 200000000

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si1>
    %1 = project %0 : <si1> -> <si1, !substrait.date> {
    ^bb0(%arg : tuple<si1>):
      %date = literal #substrait.date<200000000>
      yield %date : !substrait.date
    }
    yield %1 : !substrait.relation<si1, !substrait.date>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      project {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        input {
// CHECK-NEXT:          read {
// CHECK:             expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:            timestamp: 10000000000
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:            timestamp_tz: 10000000000

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si1>
    %1 = project %0 : <si1> -> <si1, !substrait.timestamp, !substrait.timestamp_tz> {
    ^bb0(%arg : tuple<si1>):
      %timestamp = literal #substrait.timestamp<10000000000us>
      %timestamp_tz = literal #substrait.timestamp_tz<10000000000us>
      yield %timestamp, %timestamp_tz : !substrait.timestamp, !substrait.timestamp_tz
    }
    yield %1 : !substrait.relation<si1, !substrait.timestamp, !substrait.timestamp_tz>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      project {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        input {
// CHECK-NEXT:          read {
// CHECK:             expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:            binary: "4,5,6,7"

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si1>
    %1 = project %0 : <si1> -> <si1, !substrait.binary> {
    ^bb0(%arg : tuple<si1>):
      %bytes = literal "4,5,6,7" : !substrait.binary
      yield %bytes : !substrait.binary
    }
    yield %1 : !substrait.relation<si1, !substrait.binary>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      project {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        input {
// CHECK-NEXT:          read {
// CHECK:             expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:            string: "hi"

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si1>
    %1 = project %0 : <si1> -> <si1, !substrait.string> {
    ^bb0(%arg : tuple<si1>):
      %hi = literal "hi" : !substrait.string
      yield %hi : !substrait.string
    }
    yield %1 : !substrait.relation<si1, !substrait.string>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      project {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        input {
// CHECK-NEXT:          read {
// CHECK:             expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:            fp32: 35.35
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:            fp64: 42.42

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <f32>
    %1 = project %0 : <f32> -> <f32, f32, f64> {
    ^bb0(%arg : tuple<f32>):
      %35 = literal 35.35 : f32
      %42 = literal 42.42 : f64
      yield %35, %42 : f32, f64
    }
    yield %1 : !substrait.relation<f32, f32, f64>
  }
}

// -----

// CHECK-LABEL: relations {
// CHECK-NEXT:    rel {
// CHECK-NEXT:      project {
// CHECK-NEXT:        common {
// CHECK-NEXT:          direct {
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        input {
// CHECK-NEXT:          read {
// CHECK:             expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:            boolean: false
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:            i8: 2
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:             i16: -1
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:            i32: 35
// CHECK-NEXT:          }
// CHECK-NEXT:        }
// CHECK-NEXT:        expressions {
// CHECK-NEXT:          literal {
// CHECK-NEXT:             i64: 42

substrait.plan version 0 : 42 : 1 {
  relation {
    %0 = named_table @t1 as ["a"] : <si1>
    %1 = project %0 : <si1> -> <si1, si1, si8, si16, si32, si64> {
    ^bb0(%arg : tuple<si1>):
      %false = literal 0 : si1
      %2 = literal 2 : si8
      %-1 = literal -1 : si16
      %35 = literal 35 : si32
      %42 = literal 42 : si64
      yield %false, %2, %-1, %35, %42 : si1, si8, si16, si32, si64
    }
    yield %1 : !substrait.relation<si1, si1, si8, si16, si32, si64>
  }
}
