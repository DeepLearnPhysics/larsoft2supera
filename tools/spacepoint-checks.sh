#!/usr/bin/env bash
# Run in the configured ICARUS release after installing the coordinated Atomic tree.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${VALIDATION_DIR:?Choose a directory for native spacepoint reports}"
stage="$PWD/build/v2-install"
coverage="$PWD/build/spacepoint-coverage"
sanitizers="$PWD/build/spacepoint-sanitizers"

# Use genuine lardataobj and ROOT; no replacement Hit or SpacePoint classes.
cmake -S . -B "$coverage" -DSupera_DIR="$stage/lib64/cmake/Supera" -DCMAKE_PREFIX_PATH="$stage" \
  -DLARSOFT2SUPERA_BUILD_WRITER_MODULE=OFF \
  -DCMAKE_CXX_FLAGS='--coverage -O0 -g' -DCMAKE_EXE_LINKER_FLAGS=--coverage
cmake --build "$coverage" --target spacepoint_test -j2
find "$coverage" -name '*.gcda' -delete
ctest --test-dir "$coverage" -R '^spacepoint_fields$' --output-on-failure

# Record source coverage separately from ROOT/lardataobj implementation coverage.
mkdir -p "$VALIDATION_DIR/spacepoint-coverage"
(
  cd "$VALIDATION_DIR/spacepoint-coverage"
  gcov --json-format --branch-probabilities --branch-counts \
    "$coverage/CMakeFiles/spacepoint.dir/src/SpacePoint.cxx.gcno"
)
python3 tests/check_spacepoint_coverage.py "$VALIDATION_DIR/spacepoint-coverage"

# Instrument our converter and its test; external framework libraries are unchanged.
cmake -S . -B "$sanitizers" -DSupera_DIR="$stage/lib64/cmake/Supera" -DCMAKE_PREFIX_PATH="$stage" \
  -DLARSOFT2SUPERA_BUILD_WRITER_MODULE=OFF \
  -DCMAKE_CXX_FLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build "$sanitizers" --target spacepoint_test -j2
ctest --test-dir "$sanitizers" -R '^spacepoint_fields$' --output-on-failure
