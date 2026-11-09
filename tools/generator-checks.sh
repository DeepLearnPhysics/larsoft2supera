#!/usr/bin/env bash
# Run in the configured ICARUS release after installing the coordinated Atomic tree.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${VALIDATION_DIR:?Choose a directory for native generator reports}"
stage="$PWD/build/v2-install"
coverage="$PWD/build/generator-coverage"
sanitizers="$PWD/build/generator-sanitizers"

# Use genuine nusimdata and ROOT; no replacement four-vector or MCTruth classes.
cmake -S . -B "$coverage" -DSupera_DIR="$stage/lib64/cmake/Supera" -DCMAKE_PREFIX_PATH="$stage" \
  -DLARSOFT2SUPERA_BUILD_WRITER_MODULE=OFF \
  -DCMAKE_CXX_FLAGS='--coverage -O0 -g' -DCMAKE_EXE_LINKER_FLAGS=--coverage
cmake --build "$coverage" --target generator_test -j2
find "$coverage" -name '*.gcda' -delete
ctest --test-dir "$coverage" -R '^generator_fields$' --output-on-failure

# Record source coverage separately from ROOT/nusimdata implementation coverage.
mkdir -p "$VALIDATION_DIR/generator-coverage"
(
  cd "$VALIDATION_DIR/generator-coverage"
  gcov --json-format --branch-probabilities --branch-counts \
    "$coverage/CMakeFiles/generator.dir/src/Generator.cxx.gcno"
)
python3 tests/check_generator_coverage.py "$VALIDATION_DIR/generator-coverage"

# Instrument our converter and its test; external framework libraries are unchanged.
cmake -S . -B "$sanitizers" -DSupera_DIR="$stage/lib64/cmake/Supera" -DCMAKE_PREFIX_PATH="$stage" \
  -DLARSOFT2SUPERA_BUILD_WRITER_MODULE=OFF \
  -DCMAKE_CXX_FLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build "$sanitizers" --target generator_test -j2
ctest --test-dir "$sanitizers" -R '^generator_fields$' --output-on-failure

# The same native value-type library now includes transported-particle extraction.
cmake --build "$sanitizers" --target transport_test -j2
ctest --test-dir "$sanitizers" -R '^transport_fields$' --output-on-failure
