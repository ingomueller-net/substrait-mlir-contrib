# ===-- e2e_duckdb.py - End-to-end test through DuckDB --------------------=== #
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# ===----------------------------------------------------------------------=== #

# RUN: %PYTHON %s | FileCheck %s
# XFAIL: system-windows

import duckdb

from substrait_mlir.dialects import substrait as ss
from substrait_mlir import ir


def run(f):
  print("\nTEST:", f.__name__)
  with ir.Context(), ir.Location.unknown():
    ss.register_dialect()
    f()
  return f


# CHECK-LABEL: TEST: testNamedTable
@run
def testNamedTable():
  # Set up test table.
  con = duckdb.connect()
  con.install_extension("substrait")
  con.load_extension("substrait")

  con.execute(query="CREATE TABLE t (a INT NOT NULL, b INT NOT NULL)")
  con.execute(query="INSERT INTO t VALUES (1, 7)")
  con.execute(query="INSERT INTO t VALUES (2, 8)")
  con.execute(query="INSERT INTO t VALUES (3, 9)")

  # Set up test plan in MLIR.
  plan = ir.Module.parse('''
    substrait.plan version 0 : 42 : 1 {
      relation as ["a", "b"] {
        %0 = named_table @t as ["a", "b"] : <si32, si32>
        yield %0 : !substrait.relation<si32, si32>
      }
    }
  ''')

  # Export MLIR plan to protobuf.
  pb_plan = ss.to_binpb(plan.operation)

  # Execute in duckdb and print result.
  # Ignore type because DuckDB's `from_substrait` has wrong type annotation.
  query_result = con.from_substrait(proto=pb_plan)  # type: ignore

  print(query_result.to_arrow_table())
  # CHECK-NEXT:          pyarrow.Table
  # CHECK-NEXT:          a: int32
  # CHECK-NEXT:          b: int32
  # CHECK-NEXT:          ----
  # CHECK-NEXT{LITERAL}: a: [[1,2,3]]
  # CHECK-NEXT{LITERAL}: b: [[7,8,9]]
