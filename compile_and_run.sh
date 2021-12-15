#!/bin/sh
./parser < $1 > ir.ll
clang ir.ll test/print.ll -o ir -Wno-override-module
./ir
