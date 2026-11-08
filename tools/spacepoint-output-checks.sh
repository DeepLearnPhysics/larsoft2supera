#!/usr/bin/env bash
# Run in the pinned native environment with genuine LArCV2 and ROOT.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${SUPERA_SOURCE:?Set SUPERA_SOURCE to the coordinated Atomic checkout}"
: "${VALIDATION_DIR:?Choose a directory for native reports}"
coverage="$PWD/build/spacepoint-output-coverage"
cmake -S "$SUPERA_SOURCE" -B "$coverage" -DSUPERA_BUILD_PROCESSING=ON \
  -DSUPERA_BUILD_LABELING=OFF -DSUPERA_BUILD_LARCV2=ON \
  -DCMAKE_CXX_FLAGS='--coverage -O0 -g' -DCMAKE_EXE_LINKER_FLAGS=--coverage \
  -DCMAKE_SHARED_LINKER_FLAGS=--coverage
cmake --build "$coverage" --target spacepoint_selection_test spacepoint_writer_test -j2
find "$coverage" -name '*.gcda' -delete
ctest --test-dir "$coverage" -R '^spacepoint_' --output-on-failure
mkdir -p "$VALIDATION_DIR/spacepoint-output-coverage"
(
  cd "$VALIDATION_DIR/spacepoint-output-coverage"
  gcov --json-format "$coverage/CMakeFiles/supera_processing.dir/src/supera/process/SelectSpacePoints.cxx.gcno"
  gcov --json-format "$coverage/CMakeFiles/supera_larcv2.dir/src/supera/io/larcv2/SpacePoint.cxx.gcno"
)
python3 tests/check_spacepoint_output_coverage.py "$VALIDATION_DIR/spacepoint-output-coverage"
