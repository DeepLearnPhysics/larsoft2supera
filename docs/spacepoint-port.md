# Fixed grid and reconstructed space points: next-stage audit

Status: source audit and read-only runtime probes; no space-point adapter or
rasterizer has been added. This follows the published generator checkpoint.

## Pinned evidence

- Supera `9e7b533d44236c64ddc17748e92dbd9d2fbe0fae`:
  `SuperaBBoxInteraction.cxx` and `SuperaSpacePoint.cxx`.
- ICARUS `ee06f3022034c77fd1e9389e09277ea4709524e2`:
  `TPC/Tracking/cluster3D/Cluster3DICARUS_module.cc`.
- Effective configuration: the frozen staged run's `supera_processes.fcl`.
- Runtime inputs: the cached cluster3D file and staged LArCV2 reference in the
  [manifest](reference-baseline.json), with installed LArCV2 `v2_2_6`.
- Atomic grid implementation: `ImageMeta3D`, unchanged by the optical, auxiliary,
  and generator checkpoints.

The local Supera checkout has a newer space-point implementation than the frozen
baseline. This audit uses `git show` at the pinned commit, not the checkout HEAD.
The newer code changes the accumulation container and skips dropped-output work;
those optimizations are not silently substituted for baseline semantics.

## What the fixed grid does

The configured minimum is `[-412.89, -181.86, -894.951]` cm, with side lengths
`[1843.2, 1843.2, 1843.2]` cm and requested voxel sizes `[0.3, 0.3, 0.3]` cm.
Legacy computes integer counts by truncating length/size, then derives the actual
voxel widths from `(maximum - minimum) / count`. This yields 6144 bins per axis
for the sample. The resulting cube is not an active-TPC acceptance mask.

Flattening is x-fastest: `x + nx * y + nx * ny * z`. Exact upper boundaries are
accepted and mapped to the last bin; this is not a half-open volume. Comparisons
must include boundary neighbors, not only generic interior points. Non-finite
coordinates, count/product overflow, and invalid dimensions need explicit checks
before floating-to-integer conversion in the new path.

`UseFixedBBox: true` bypasses event-dependent bounds and randomization. The old
configuration still queries geometry to construct active-TPC world bounds, but
those bounds do not clip this fixed grid. A first port of this configuration does
not need the unused dynamic-box machinery.

## Space-point extraction is producer-specific

`recob::SpacePoint` normally exposes a position, six covariance entries, chi-square,
and an object ID. The pinned ICARUS cluster3D producer deliberately repurposes
`ErrXYZ()[1]` as total charge and `ErrXYZ()[3]` as charge asymmetry, including its
unclustered-point path. This is not a detector-independent interpretation of the
SpacePoint class. An explicit input profile must name that convention.

The legacy path reads associated `recob::Hit` objects using the space-point
producer tag. It also requires a same-tag Hit collection to construct offsets.
Hit identity must remain `(source product, key)` in shared records; a key alone
is only meaningful within its owning product.

## Selection and collision rules

For each cryostat, in configured source order and point order:

1. Cast the charge parameter to float, then retain values within the inclusive
   configured range `[-1000, 50000]`.
2. Apply configured `ShiftX` (zero here), then map the position to the shared grid.
   Reject out-of-volume points.
3. Read associated hits. Occupancy is incremented before representative selection,
   although occupancy output is dropped in this configuration.
4. If a voxel already has a representative, prefer more associated hits; for equal
   hit counts, prefer larger float charge. Exact ties select the later point.
   Charge is not summed across colliding points.
5. Copy the representative charge, chi-square, and hit-derived properties. A
   missing plane receives `-1` for the corresponding hit fields.
6. Sort output voxels by ID. Each output stores float values.

The representative ranking uses association count, not the number of distinct
valid planes. That distinction matters for malformed or repeated associations.

## Actual outputs and omissions

Each cryostat writes ten sparse tensors: charge, chi-square, cryostat, TPC, three
hit-charge tensors, and three hit-key tensors. Occupancy, hit count, hit amplitude,
hit time, hit RMS, and hit multiplicity are explicitly dropped. Charge asymmetry
is calculated but its store call is commented out. `get_common_charge` is not
called by the active processor and should not be ported as active behavior.

`StoreWireInfo` appears in FHiCL but is not read by this implementation. Wire
storage is controlled by the absence of `hit_*` in `DropOutput`. The port should
expose one unambiguous configuration rule, with a documented baseline mapping.

The legacy multi-source hit offsets accumulate the sizes of later configured
collections. They are ordering-dependent and assume association targets match
those collections. This sample has one source per cryostat, so each offset is
zero. Do not generalize that sample result into a universal identity scheme.

## Issues requiring explicit treatment

- Hit keys become floats in LArCV tensors; arbitrary integer identity is not
  representable beyond float's exact-integer range. Keep integer identities in
  shared data and check any compatibility conversion.
- The code updates charge/chi-square before rejecting a point with more than three
  hits. The warning says the point is dropped, but those outputs were already
  changed; other fields may retain an older representative.
- Invalid planes are skipped individually. Duplicate-plane associations can
  overwrite each other and defeat the missing-plane fill condition. Cryostat/TPC
  fields are overwritten per hit, effectively retaining the last one.
- Dynamic bounding-box randomization uses the x length when completing y and z
  extents. This is dormant in the fixed-grid sample; porting that branch requires
  an anisotropic-box regression, not a cubic sample alone.
- Both old grid implementations narrow a voxel ID to `int` inside `shift`.
  The configured grid has 231,928,233,984 cells, so large valid IDs need a focused
  runtime probe. Basic coordinate-to-ID agreement does not establish safe shifts.

These observations are not permission to alter reference outputs silently. For
malformed inputs, define the intended rejection policy before rasterization;
for supported inputs, compare complete outputs and trace selected point identity.

## Bounded implementation sequence

1. **Fixed-grid contract:** shared bounds, counts, units and explicit boundary
   behavior; checked 64-bit indexing; exact metadata comparison with the reference.
   Reuse or modernize Atomic's grid logic rather than creating an unrelated indexer.
2. **Native records and associations:** copy source points and hits into owning
   records, retain exact product/key identities, and interpret cluster3D charge
   only through the named input convention. No rasterization in the reader.
3. **Shared representative selection:** origin-independent point cuts, grid
   assignment, deterministic collision ranking, and traceable rejection reasons.
   Test ties, shared hits, absent planes, repeated planes and malformed inputs.
4. **LArCV2 projection:** serialize the twenty configured space-point products,
   with explicit integer-to-float compatibility checks and sorted voxel IDs.
   Compare all twenty trees against both frozen references.

Truth-to-reco matching and charge rescaling remain separate later stages: they
have additional particle/deposit dependencies and must not be hidden in extraction.

## Runtime evidence from the cached sample

The read-only audit covered all ten events and both cryostats:

| Measurement | Result |
| --- | ---: |
| Input space points | 8,839,871 |
| Dereferenced point-to-hit links | 20,490,298 |
| Unique hit identities, summed per event/collection | 383,132 |
| Additional uses of those hit identities | 20,107,166 |
| Points landing in an already-seen voxel | 2,676,506 |
| Representative replacements | 1,822,809 |
| Exact hit-count/float-charge ties | 27 |
| Selected voxels | 6,163,365 |
| Grid-ID disagreements between installed LArCV2 and Atomic | 0 |
| Selected-charge mismatches against the reference | 0 |

No non-finite positions/charges, charge-cut rejections, out-of-grid points, empty
hit associations, more-than-three-hit points, repeated/invalid planes, mixed-TPC
associations, or inexact float hit keys were observed. Each event/cryostat
collection referenced one hit product. Those zero counts do not justify omitting
synthetic tests for the unsupported or malformed cases.

The high hit reuse is important for the shared representation: store owning hit
collections once and retain point-to-hit references, rather than copying complete
hit objects into every point. Retain integer identities until the output boundary.

The audit reproduces representative **charge** and checks grid IDs. It does not
establish full twenty-product parity for a new adapter; chi-square, hit attributes,
all output metadata, and behavior under malformed inputs still need port tests.
Reproduction sources are in the lab's `research/spacepoints/`, with CSV/JSON
reports under `results/spacepoint-audit/`. The compiled probe uses the actual
Atomic grid implementation and installed LArCV2 library, not copied index formulas.

## First implementation: large-ID shifts

A separate probe confirmed that both pre-fix implementations return the invalid
sentinel for `shift(3774873600, 0, 0, 0)` on this grid, although the expected result
is `3774873600`. Minimum, exact maximum, and just-outside coordinate probes agree
with the documented legacy boundary convention.

Atomic commit `1b67404` provides a focused correction: decode the full-width origin,
check each signed axis displacement before unsigned arithmetic, and reject invalid
origins or an uninitialized grid. The test covers the large ID, all six boundaries,
INT_MIN/INT_MAX offsets, and every immediate neighbor in a small non-cubic grid.
It does not change `id()` or the frozen coordinate-to-voxel mapping. Count overflow,
non-finite coordinate validation, metadata ownership and reader integration remain
part of the following fixed-grid work, not claims of this small correction.

Validation of the local shift correction: all four shared-core tests pass under
address and undefined-behavior sanitizers, including the new grid regression and
the existing labeling smoke test. The source audit and documentation lint pass.
This is the first bounded grid fix; the space-point production port has not yet
been implemented or claimed equivalent.
