#!/usr/bin/env bash
# Genuine lardataobj converter coverage, alongside native round-trip integration.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${VALIDATION_DIR:?Choose a directory for native deposit reports}"
stage="$PWD/build/v2-install"
coverage="$PWD/build/deposit-coverage"
cmake -S . -B "$coverage" -DSupera_DIR="$stage/lib64/cmake/Supera" -DCMAKE_PREFIX_PATH="$stage" \
  -DLARSOFT2SUPERA_BUILD_WRITER_MODULE=OFF \
  -DCMAKE_CXX_FLAGS='--coverage -O0 -g' -DCMAKE_EXE_LINKER_FLAGS=--coverage
cmake --build "$coverage" --target deposit_test -j2
find "$coverage" -name '*.gcda' -delete
ctest --test-dir "$coverage" -R '^deposit_fields$' --output-on-failure
mkdir -p "$VALIDATION_DIR/deposit-coverage"
(
  cd "$VALIDATION_DIR/deposit-coverage"
  gcov --json-format --branch-probabilities --branch-counts \
    "$coverage/CMakeFiles/deposit.dir/src/Deposit.cxx.gcno"
)
python3 tests/check_deposit_coverage.py "$VALIDATION_DIR/deposit-coverage"

# Instrument the actual converter with genuine LArSoft value types.
sanitizers="$PWD/build/deposit-sanitizers"
cmake -S . -B "$sanitizers" -DSupera_DIR="$stage/lib64/cmake/Supera" -DCMAKE_PREFIX_PATH="$stage" \
  -DLARSOFT2SUPERA_BUILD_WRITER_MODULE=OFF \
  -DCMAKE_CXX_FLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build "$sanitizers" --target deposit_test -j2
ctest --test-dir "$sanitizers" -R '^deposit_fields$' --output-on-failure
