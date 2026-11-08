# Space-point extraction

`larsoft2supera::spacepoint` extracts native `recob::SpacePoint` and optionally
associated `recob::Hit` records into Atomic's standard-library-only
`supera/data/SpacePoint.h`. Neither the parser nor its records depend on LArCV.
This stage performs no cuts, voxel selection, truth matching, or output writing.
The shared selection stage now connects these collections to `EventRecord` and
the LArCV2 writer; see [the output review](spacepoint-output.md). Raw cloud parsing
remains independently usable.

## Explicit input interpretation

`convert_spacepoints(points, profile)` retains collection order, positions in cm,
chi-square, and optional native IDs. The required profile is either:

- `Covariance`: preserves all six covariance entries, in native xx, xy, yy, xz,
  yz, zz order, in square cm.
- `IcarusCluster3D`: interprets `ErrXYZ()[1]` as charge and `[3]` as charge
  asymmetry. The remaining slots are producer placeholders, not covariance.
  Charge units remain explicitly source-defined. This profile is for the audited
  ICARUS cluster3D producer, not every producer in ICARUS.

Floating-point invalidity and field-specific native sentinels are normalized at
this boundary using the shared validity contract. Optional columns distinguish
absent information from present-but-empty products.

## Optional hit associations

`read_spacepoints(event, point_tag, profile)` reads points only. Passing a
`SpacePointHitInput` additionally requests one association tag and an explicit
list of allowed hit product tags. The Event-like template is tested with
`art::Event`; gallery integration has not been validated.

The reader requests `Assns<SpacePoint, Hit>`; art supplies the partner of native
`Assns<Hit, SpacePoint>` products. A table must refer to exactly the requested
point product. Mixed point-product tables are unsupported and rejected. Missing
requested products, null endpoints, out-of-range keys, unexpected hit products,
and duplicate configured product identities fail before returning a record.
Points-only mode never loads hits or associations.

The shared collection owns each referenced hit once per source product/key per
read call. Flat links reference these compact hit collections, with offsets
marking each point's span. Repeated links and serialized within-point order are
preserved. Original hit keys, event-scoped product identity, configured source
tag, and full native product size survive compaction. There are no art pointers
or handles in the result. Sharing across separate point collection reads is not
yet implemented.

All native hit scalar fields are retained, including wire address, channel,
view/signal codes, timing, amplitude, summed ADC, integral, errors, fit quality,
multiplicity, local index, and degrees of freedom. Source enum codes retain their
native meaning; no detector semantics are inferred from them.

## Validation and performance

`tools/integration.sh` runs native converter tests, installed-consumer validation,
and `tests/spacepoint_integration.py` against the checksum-pinned cached sample.
The latter checks every point and the full association graph, with native
fixtures for shared hits, duplicates, multiple products, empty events, and eight
malformed/missing-input cases. `tools/spacepoint-checks.sh` adds converter coverage
and address/undefined-behavior sanitizer checks using genuine native classes.

For three independent timing repetitions, run `spacepoint_integration.py` with
`--repeats 3` after the normal native setup and an optimized build. Its CSV files
separate point access, conversion, hit access, association access, and link/hit
conversion. Optional timing performs no per-object clock reads. Validation is
outside timed regions; timing repetitions disable validation. Product access
includes framework loading/decompression and depends on cache state.

`owned_capacity_bytes` counts owned vector payload capacity, excluding strings,
transient indexing arrays, framework products, and allocator overhead. Use the
MemoryTracker report for whole-process peak memory. Historical whole-Supera
timings in [performance.md](performance.md) include much more work and cannot
serve as a direct parser speedup denominator.

Normalization reports aggregate the read call. Hit examples identify the first
point referencing that hit, and each distinct referenced hit contributes once.
They do not claim to be separate per-hit-product reports. Converter sanitizer
coverage does not instrument external art libraries or the templated event reader;
the latter is exercised by native framework fixtures and full-sample validation.

The local native check completed with 58/58 converter lines and 2/2 functions
covered (45/70 gcov branches, including compiler-generated exception paths).
Reports live in `results/spacepoint-parser-checks`; the final checksum-pinned
framework validation is in `results/spacepoint-parser-final`. The shared model
also passes its dependency-free CTest and clang-tidy checks.
