#!/bin/sh
H=/home/molesir/development/llvm-project
INC="-ICh2/src -I$H/llvm/include -I$H/build/include -I$H/mlir/include -I$H/build/tools/mlir/include"

mkdir -p build/Ch2

for pair in \
  "ops.h.inc gen-op-decls" \
  "ops.cpp.inc gen-op-defs" \
  "dialect.h.inc gen-dialect-decls" \
  "dialect.cpp.inc gen-dialect-defs"
do
  set -- $pair
  $H/build/bin/mlir-tblgen "-$2" $INC Ch2/src/ops.td -o "./Ch2/src/output/$1"
done