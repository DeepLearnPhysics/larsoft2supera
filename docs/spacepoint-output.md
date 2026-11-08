# Space-point rasterization and LArCV2 output review

The configured ICARUS fixed-grid space-point slice now runs from native input,
through shared selection, to all twenty legacy LArCV2 tensors. The input parser
still preserves the full cloud; rasterization belongs to Atomic's independently
buildable `supera::processing` component. No LArCV type enters the shared model.

## What is implemented

`select_spacepoints` takes owned source collections, a checked `FixedGrid`, and
explicit selection settings. It returns sorted unique voxel IDs, representative
source/point indices, occupancy, hit counts, selection diagnostics, and the owned
original collections. Retaining originals makes every selected attribute auditable.
The original charge and position columns are never overwritten by cuts or shifts.
Combining sources assumes a common coordinate frame and comparable charge units;
no implicit calibration is performed. Raw-cloud file output remains a separate
future feature; this writer produces the selected sparse tensors.

Selection applies the configured position shift and inclusive charge range.
It rejects nonfinite coordinates/charges and out-of-grid points, counting those
rejections. In legacy float-charge mode, values outside float range are also
counted in the invalid/nonfinite category before conversion. The configured
ranking is hit count, then charge, then the later point on an exact tie, in source
and point order. Charge is not summed. `PreferMoreHits: false` selects by charge
alone and permits points-only extraction. Occupancy counts accepted candidates,
including candidates that lose a collision.

`FloatCharge: true` reproduces legacy float rounding before cuts and ranking.
The shared default is false, preserving double precision for input layers that
choose it. This is an explicit processing policy, not an output-type dependency.

The LArCV2 writer projects the selected representatives into the configured field
families. For the reference setup these are charge, chi-square, cryostat, TPC,
three hit-charge tensors, and three hit-key tensors per cryostat. Occupancy, hit
count, amplitude, time, RMS, and multiplicity families are also supported and tested.
Absent plane fields become legacy `-1` only at this boundary. Integer hit keys
remain exact until projection; a key that cannot be represented exactly by the
legacy float field fails rather than silently rounding.

## Native configuration

[spacepoint_icarus.fcl](../fcl/spacepoint_icarus.fcl) explicitly supplies source and
association tags, allowed hit products, charge interpretation, grid dimensions,
shift, cuts, precision, ranking, and output fields. Setting `WithHits: false`
actually skips hit/association loading; select charge-only ranking and fields
`["", "chi2"]` in that case. The writer fails if requested hit-derived fields
need associations that were not supplied.

This checkpoint implements the frozen fixed-grid configuration. It does not claim
support for every legacy FHiCL option, dynamic/event-derived boxes, arbitrary
charge estimators, truth matching, or individual per-plane output suppression.
Changing detectors requires an explicit input/profile review, not a name switch.

## Deliberate safeguards

The selector validates all supplied association graphs before selecting from a
source. Null/invalid references, malformed offsets, more hits than planes, invalid
wire addresses, repeated planes, and mixed cryostat/TPC associations fail the
event. The parser still preserves these relationships: this stricter rule belongs
to this representative-selection policy, not to raw point-cloud extraction.

The old code could replace charge/chi-square before rejecting excessive hits,
leaving other attributes from a previous point. Repeated planes and mixed TPCs
also made results depend on association iteration order. The new representation
selects a whole point atomically and refuses these ambiguous cases. A future
alternative policy can be added deliberately; we do not silently guess now.

A selected point with zero hits has explicit missing detector/plane fields. Legacy
code could instead leave detector fields absent or stale. Invalid measurements
use canonical NaN internally and legacy `-1` in the affected sparse output field.
These are documented edge-case behavior changes, not differences observed in the
cached sample. See findings F021–F023.

The writer retains reverse product-offset concatenation for legacy hit keys.
Repeated references to the same product identity share one offset, and inconsistent
source sizes fail. Shared records always retain product identity plus original
key, independent of this compatibility encoding.

## Equivalence evidence

The initial complete run in `results/spacepoint-output-01` produced 6,163,365
selected voxels across ten events and both cryostats. All twenty product trees
matched both immutable reference files exactly: 200 product-events and
739,663,140 serialized bytes per reference. The comparison covers every serialized
field, including voxel IDs/order, float values, event identities, and metadata.
It compares product serialization, not whole ROOT file bytes or compression layout.
There is no numerical tolerance or difference allowlist for this slice.

[The recorded baseline](spacepoint-output-baseline.json) retains stage timings,
per-event counts, and evidence hashes. Median measured times for both cryostats
combined were 1.83 s extraction, 0.26 s selection, and 1.88 s projection/writing;
the median combined measurement was 4.17 s/event. These separate medians need not
sum exactly. Peak process RSS was 2553.3 decimal MB. This is one optimized run in
the existing emulated ICARUS environment, not a production throughput claim or a
comparison against the entire legacy Supera driver. Event-local raw clouds and
projected tensors coexist; memory should remain an explicit regression metric.

## Reproduction and tests

Run `tools/integration.sh` after the pinned ICARUS setup, supplying `SUPERA_SOURCE`,
`FIXTURE_ROOT`, and a new `VALIDATION_DIR`. It builds and installs the coordinated
Atomic tree, checks consumers and previously migrated products, validates parsing,
and runs `tests/spacepoint_output_integration.py` for output equivalence.
The fixture verifier checks hashes of the cached input and both reference files.

Standalone shared tests cover collision ranking, precision ties, source ordering,
shifts, cuts, nonfinite/outside points, malformed graphs, empty input, and points-only
selection. Native writer tests cover all supported field families, missing planes,
key offsets, float representability, malformed records, and nonempty/empty event
clearing. Comparator fixtures independently change event identity, inventory,
entry count, bounds, grid counts, voxel ID, and value to prove failures are detected.

Shared processing passes address, undefined-behavior, and float-cast-overflow
sanitizers and clang-tidy. Native coverage is 96/97 selector lines (3/3 functions)
and 140/150 sparse-projection lines (5/5 functions). The integration workflow gates
90% line and 100% function coverage for each component separately. Coverage reports
are retained in `results/spacepoint-output-coverage`. External framework libraries
are not instrumented by the shared-core sanitizer run.

The final checked output is retained in `results/spacepoint-output-final`, with
both comparison reports and eight passing comparator mutation checks. The prior
optical/auxiliary/generator regression is in `results/spacepoint-prior-products-regression`;
its existing CRT normalization allowlist is unchanged.

This checkpoint pins the coordinated Atomic commit in the C++ workflows and
fast-check script. Native integration requires the pinned ICARUS environment and
immutable fixtures; it runs on an explicitly provisioned runner.
