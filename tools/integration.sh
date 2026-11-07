#!/usr/bin/env bash
# Run after setting up the pinned ICARUS UPS release, in a fresh shell.
set -o pipefail
cd "$(dirname "$0")/.."
: "${SUPERA_SOURCE:?Set SUPERA_SOURCE to the v2 Atomic checkout}"
: "${FIXTURE_ROOT:?Set FIXTURE_ROOT to the lab containing manifest artifacts}"
: "${VALIDATION_DIR:?Choose a new directory for results}"
if [[ "${ICARUSCODE_DIR##*/}" != v10_27_00 ]]; then
  echo "Expected ICARUS v10_27_00 setup" >&2
  exit 1
fi
if ! declare -F setup >/dev/null; then
  source /products/setup
fi
# UPS setup scripts are not errexit/nounset-safe. Check their final status.
setup cmake v3_27_4 || exit 1
setup cetmodules v3_24_01 || exit 1
set -euo pipefail
mkdir "$VALIDATION_DIR"
python3 tests/verify_baseline.py "$FIXTURE_ROOT" "$VALIDATION_DIR/event-ids.json"

stage="$PWD/build/v2-install"
cmake -S "$SUPERA_SOURCE" -B build/atomic-icarus -DSUPERA_BUILD_LARCV2=ON \
  -DSUPERA_BUILD_LABELING=OFF -DCMAKE_INSTALL_PREFIX="$stage"
cmake --build build/atomic-icarus -j2
ctest --test-dir build/atomic-icarus --output-on-failure
cmake --install build/atomic-icarus
cmake -S . -B build/adapter-icarus -DSupera_DIR="$stage/lib64/cmake/Supera" \
  -DCMAKE_PREFIX_PATH="$stage" \
  -DCMAKE_INSTALL_PREFIX="$stage"
cmake --build build/adapter-icarus -j2
ctest --test-dir build/adapter-icarus --output-on-failure
cmake --install build/adapter-icarus
cmake -S tests/consumer -B build/installed-consumer \
  -Dlarsoft2supera_DIR="$stage/lib64/cmake/larsoft2supera" \
  -DSupera_DIR="$stage/lib64/cmake/Supera" -DCMAKE_PREFIX_PATH="$stage"
cmake --build build/installed-consumer -j2
build/installed-consumer/bin/consumer

# Preserve all framework plugin paths. The fixture plugin is test-only.
export CET_PLUGIN_PATH="$stage/lib64:$PWD/build/adapter-icarus/lib64:${CET_PLUGIN_PATH:-}"
export LD_LIBRARY_PATH="$stage/lib64:${LD_LIBRARY_PATH:-}"
python3 tests/integration.py \
  --input "$FIXTURE_ROOT/data/icarus-overlay-cluster3d-v10_27_00/overlay_cluster3d_cache.root" \
  --reference "$FIXTURE_ROOT/results/staged-full-01/supera_reference.root" \
  --reference "$FIXTURE_ROOT/results/standard-full-01/larcv_mc_20261107_001608_861108.root" \
  --expected-ids "$VALIDATION_DIR/event-ids.json" --output "$VALIDATION_DIR/events"

# Native generator tests require ROOT/nusimdata and cannot run in the lean image.
bash tools/generator-checks.sh
