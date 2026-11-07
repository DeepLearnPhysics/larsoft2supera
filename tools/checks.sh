#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

fetch_source() {
  local directory=$1 repository=$2 revision=$3
  if [[ ! -d "$directory/.git" ]]; then
    git init -q "$directory"
    git -c safe.directory="$directory" -C "$directory" fetch -q --depth 1 "$repository" "$revision"
    git -c safe.directory="$directory" -C "$directory" checkout -q --detach FETCH_HEAD
  fi
  test "$(git -c safe.directory="$directory" -C "$directory" rev-parse HEAD)" = "$revision"
}
export LARDATAOBJ_SOURCE=${LARDATAOBJ_SOURCE:-$PWD/build/deps/lardataobj}
export SBNOBJ_SOURCE=${SBNOBJ_SOURCE:-$PWD/build/deps/sbnobj}
export SUPERA_SOURCE=${SUPERA_SOURCE:-$PWD/build/deps/SuperaAtomic}
fetch_source "$SUPERA_SOURCE" https://github.com/DeepLearnPhysics/SuperaAtomic.git \
  77953b3ab72e139c28772004ddc3c8a94bc602b9
fetch_source "$LARDATAOBJ_SOURCE" https://github.com/LArSoft/lardataobj.git \
  949008add3a77ef7d532536d8e7b7343cb5e9190
fetch_source "$SBNOBJ_SOURCE" https://github.com/SBNSoftware/sbnobj.git \
  55c657ca59214b449e5d951e691f21c2b5278f36
# Build and install the shared data package without ROOT, LArCV, or art available.
cmake -S "$SUPERA_SOURCE" -B build/supera-data -DSUPERA_BUILD_LARCV2=OFF \
  -DSUPERA_BUILD_LABELING=OFF -DCMAKE_INSTALL_PREFIX="$PWD/build/supera-install"
cmake --build build/supera-data -j2
ctest --test-dir build/supera-data --output-on-failure
cmake --install build/supera-data
clang-tidy-14 -p build/supera-data "$SUPERA_SOURCE/tests/v2/Data_test.cxx" "$SUPERA_SOURCE/tests/v2/Generator_test.cxx"

mapfile -t files < <(find include src plugins tests -type f \
  \( -name '*.h' -o -name '*.hxx' -o -name '*.cc' -o -name '*.cxx' \) | sort)
clang-format-14 --dry-run --Werror "${files[@]}"
cmake -S . -B build/v2-coverage -DLARSOFT2SUPERA_BUILD_ART=OFF \
  -DSBNOBJ_SOURCE="$SBNOBJ_SOURCE" -DLARDATAOBJ_SOURCE="$LARDATAOBJ_SOURCE" -DCMAKE_PREFIX_PATH="$PWD/build/supera-install" \
  -DCMAKE_CXX_FLAGS='--coverage -O0 -g' -DCMAKE_EXE_LINKER_FLAGS=--coverage
cmake --build build/v2-coverage -j2
ctest --test-dir build/v2-coverage --output-on-failure
clang-tidy-14 -p build/v2-coverage src/Optical.cxx tests/Optical_test.cxx src/AuxDet.cxx tests/AuxDet_test.cxx
# GCC attributes allocation-failure cleanup to the closing brace (uncovered).
# Keep the raw report: all functions are covered; do not test std::bad_alloc here.
gcovr build/v2-coverage --root . --filter 'src/(Optical|AuxDet).cxx' --print-summary \
  --fail-under-line 90 --fail-under-function 100 --json-summary build/v2-coverage/summary.json
cmake -S . -B build/v2-sanitizers -DLARSOFT2SUPERA_BUILD_ART=OFF \
  -DSBNOBJ_SOURCE="$SBNOBJ_SOURCE" -DLARDATAOBJ_SOURCE="$LARDATAOBJ_SOURCE" -DCMAKE_PREFIX_PATH="$PWD/build/supera-install" \
  -DCMAKE_CXX_FLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build/v2-sanitizers -j2
ctest --test-dir build/v2-sanitizers --output-on-failure

# Shared selection is header-only and should also run under the sanitizer build.
cmake -S "$SUPERA_SOURCE" -B build/supera-sanitizers -DSUPERA_BUILD_LARCV2=OFF \
  -DSUPERA_BUILD_LABELING=OFF \
  -DCMAKE_CXX_FLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build/supera-sanitizers -j2
ctest --test-dir build/supera-sanitizers --output-on-failure
