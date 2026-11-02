#!/bin/bash
# After the large run (finished or stopped): the layer started from the example's single-customer columns
# (COLGEN_INIT=single), so that it starts from the same point as the examples. Not started automatically.
cd /c/Users/marbe/Desktop/programming/git/rcspp
run="powershell -ExecutionPolicy Bypass -File benchmark-results/colgen/run_matrix.ps1"
layer=layer-cpp-highs-exact,layer-cpp-highspy-exact,layer-cpp-gurobipy-exact,layer-python-highs-exact,layer-python-highspy-exact,layer-python-gurobipy-exact
export COLGEN_INIT=single
$run --output benchmark-results/colgen/single-small --instances small --configs $layer > benchmark-results/colgen/single-small.out 2>&1
$run --output benchmark-results/colgen/single-large --instances C101,R101,RC101,C201,RC201,R201 --configs $layer > benchmark-results/colgen/single-large.out 2>&1
