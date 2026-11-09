#!/usr/bin/env bash
# Genuine nusimdata converter coverage, alongside native round-trip integration.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${VALIDATION_DIR:?Choose a directory for native transport reports}"
stage="$PWD/build/v2-install"
coverage="$PWD/build/transport-coverage"
cmake -S . -B "$coverage" -DSupera_DIR="$stage/lib64/cmake/Supera" -DCMAKE_PREFIX_PATH="$stage" \
  -DLARSOFT2SUPERA_BUILD_WRITER_MODULE=OFF \
  -DCMAKE_CXX_FLAGS='--coverage -O0 -g' -DCMAKE_EXE_LINKER_FLAGS=--coverage
cmake --build "$coverage" --target transport_test -j2
find "$coverage" -name '*.gcda' -delete
ctest --test-dir "$coverage" -R '^transport_fields$' --output-on-failure
mkdir -p "$VALIDATION_DIR/transport-coverage"
(
  cd "$VALIDATION_DIR/transport-coverage"
  gcov --json-format --branch-probabilities --branch-counts \
    "$coverage/CMakeFiles/generator.dir/src/Transport.cxx.gcno"
)
python3 tests/check_transport_coverage.py "$VALIDATION_DIR/transport-coverage"
