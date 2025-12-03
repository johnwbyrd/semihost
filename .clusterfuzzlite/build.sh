#!/bin/bash -eu

# ClusterFuzzLite build script
# This script is called by the CFL infrastructure to build fuzz targets

cd $SRC/semihost

# Build with fuzzing instrumentation
# CFL sets: CC, CXX, CFLAGS, CXXFLAGS, LIB_FUZZING_ENGINE
cmake -B build \
    -DCMAKE_C_COMPILER=$CC \
    -DCMAKE_C_FLAGS="$CFLAGS" \
    -DENABLE_FUZZING=OFF

cmake --build build

# Build fuzz targets manually with libFuzzer
# (We can't use ENABLE_FUZZING because CFL provides its own fuzzing engine)
$CC $CFLAGS -I include -c fuzz/fuzz_riff_parser.c -o fuzz_riff_parser.o
$CC $CFLAGS $LIB_FUZZING_ENGINE fuzz_riff_parser.o build/libzbc_semi_host.a -o $OUT/fuzz_riff_parser

# Copy seed corpus
cp -r fuzz/corpus/riff_parser $OUT/fuzz_riff_parser_seed_corpus
