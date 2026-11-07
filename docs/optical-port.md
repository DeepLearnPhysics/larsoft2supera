# Optical port review packet

Status: validated optical checkpoint, authorized for publication on 2026-11-07.

## Scope and contract

This port recovers existing optical-flash functionality. It does not add hit
associations, CRT matches, or optical truth. The [audit](optical-audit.md) retains
those future opportunities and unresolved upstream conventions.

The extraction function returns `supera::OpticalFlash` values from Atomic's shared
data component. It preserves all stored fields, vector order, numeric values,
NaNs, signed zeros, and sentinels. IDs restart at zero per collection. The returned
vectors own their contents and do not retain art handles or references.

The art module assembles `supera::EventRecord` and calls Atomic's `EventWriter`
interface. The current composition selects Atomic's optional LArCV2 writer. Only
that output implementation creates LArCV objects. No LArCV headers, types, or link
dependencies remain in the extraction library.

Collections have logical names, configured source identifiers, and separate typed
conventions. Optional FHiCL `Detector`, `Region`, and `CoordinateFrame` strings
populate that context. PE indexing and absolute-time conventions remain unknown
unless established explicitly; the adapter does not guess. LArCV2 cannot serialize
this added context, so the compatibility projection stores only the existing
fields. No claim of metadata round-trip preservation is made.

## Deliberate safeguards

- A required missing product or wrong process name fails through art product lookup.
  Existing empty collections remain valid. This preserves the effective legacy
  `getValidHandle` behavior, despite its unreachable invalid-handle check.
- Empty configuration, empty tags/labels, and duplicate output labels are rejected.
  Duplicate labels previously allowed one collection to overwrite another.
- All input collections are converted before the writer is called. A late missing
  product cannot leave a partially populated event queued for serialization.
- The output adapter rejects unrepresentable IDs and mismatched collection inventories.
- Event-owned records replace previous contents. Explicit writer close reports errors.

No physics correction was identified and approved in this flash-field migration.
The configuration safeguards do not change the frozen sample's output.

## Builds

The two repositories are a coordinated checkpoint. This adapter pins Atomic
`7bb73fb443eb0d6073649273cb5eb7577b1d87c5` in CI and the fast-check script.
Install that revision before configuring against the shared v2 data API.

The default build uses installed art/lardataobj and enables the composed LArCV2
module. Set `LARSOFT2SUPERA_BUILD_WRITER_MODULE=OFF` to build/install extraction
against LArSoft without requiring Atomic's output component. The fast value-type
build uses `LARSOFT2SUPERA_BUILD_ART=OFF` and compiles the pinned genuine OpFlash
implementation without the full framework.

```sh
docker build -t larsoft2supera-checks -f tools/Dockerfile .
docker run --rm -v "$PWD:/work" -v /path/to/SuperaAtomic:/supera:ro \
  -e SUPERA_SOURCE=/supera larsoft2supera-checks bash tools/checks.sh
```

This image has no ROOT, LArCV, or art. It builds and installs Atomic's data-only
package, then checks formatting, field conversion, clang-tidy, coverage, and
address/undefined-behavior sanitizers. The LArSoft value-type revision and tooling
versions are pinned. Distribution compiler packages and gcovr's transitive Python
dependencies are not fully locked, so the checks image is not claimed bit-reproducible.

For the full integration test, enter the pinned ICARUS container and setup release,
then set `SUPERA_SOURCE`, `FIXTURE_ROOT` (the lab root), and `VALIDATION_DIR` (a new
results directory):

```sh
bash tools/integration.sh
```

The script checks reference hashes, builds/installs both packages, runs unit tests,
builds an independent installed consumer, and runs the art integration suite.
The runtime is ICARUS `v10_27_00`, `e26:prof`, LArCV2 `v2_2_6`; CMake `v3_27_4`
and cetmodules `v3_24_01` provide build tooling. The image digest and artifacts are
recorded in the [baseline manifest](reference-baseline.json).

## Validation and limits

The standalone extractor tests check every field against independently specified
values, ownership, collection order, empty inputs, consecutive-event isolation,
sentinels, vector contents, and floating-point edge cases. Atomic's writer tests
independently read back all flash fields, IDs, and an empty following event.

The art integration checks both frozen references, a three-event
nonempty/empty/nonempty fixture, seven invalid-input/configuration cases, and
comparator controls including a one-double-ULP change. Exact comparisons include
freshly serialized object bytes and event identities, excluding ROOT container
metadata such as file UUIDs.

The final local run on 2026-11-07 passed against both reference files: two trees,
ten events, twenty product-event records and 586,046 serialized bytes per
reference comparison. The lab report is
`results/optical-v2-validation-04/events/summary.json`; comparator controls are in
its `comparator-checks/checks.json`. Fixture checksums were verified before the run.
The new data-only build, existing labeling-core smoke test, installed consumer,
writer contract tests, and standalone adapter checks also passed.

Raw extractor coverage is 95.8% lines, 100% functions, and 52.3% branches with GCC
in the local checks image. The uncovered line is allocation-failure cleanup at the
function closing brace; branch totals include exception paths. Allocation failures
are not injected. The writer and art wrapper have functional tests but no coverage
percentage claim; clang-tidy currently covers the standalone extractor and tests.

The C++ and documentation workflows run on pushes and pull requests. Both adapter
workflows use the pinned Atomic revision. The full ICARUS workflow is manually
dispatched; local success does not establish hosted execution or runner availability.
The integration workflow additionally requires a provisioned runner, UPS products,
and checksum-matching fixtures. Missing infrastructure must not count as a pass.
