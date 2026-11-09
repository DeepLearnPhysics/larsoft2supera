# Findings ledger

This ledger separates observations from confirmed bugs. No legacy behavior has
been approved as fixed by the reviewer yet. Local safeguards are identified below.
Reports should remain useful to upstream
maintainers without requiring access to this conversation.

## Status and reporting rules

Use `observed`, `suspected`, `confirmed`, `decision recorded`, `fixed`, or
`verified`. A log warning alone is not a confirmed bug. A fix is not verified
until a regression test demonstrates the intended behavior.

Every entry should record the pinned source/configuration, smallest reproduction,
expected versus observed behavior, affected outputs and events, classification
(extraction, algorithm, policy, numerical, performance, or documentation), proposed
resolution, reviewer decision, test, and upstream issue/PR when one exists.
Do not open upstream issues or mark fixes approved automatically.

## F-001: slow legacy processing

- **Status:** observed; cause not profiled conclusively.
- **Baseline:** Supera `9e7b533`, LArCV2 `v2_2_6`, ICARUS `v10_27_00`, x86-64
  execution on an ARM Mac. See the [manifest](reference-baseline.json).
- **Evidence:** staged event `13171:1:541` spent approximately 2313 seconds in
  `LArSoftSuperaDriver`. Both complete jobs succeeded and their LArCV data matched.
- **Confounders:** emulation, host suspension, and a brief debugger-induced pause
  in the staged run. These were not controlled performance benchmarks.
- **Related change:** SBN2025A updates both Supera and LArCV2 for runtime reduction
  in [fb812f1][speedup]; ICARUS `v10_06_00_13` and the verified `v10_06_00_14`
  variants use Supera v1.0.0 and LArCV2 v2_4_1. That is not proof of the bottleneck
  in the pinned baseline.
- **Next evidence:** profile a bounded case when useful. Preserve the reference
  output; do not silently replace the baseline with a faster release.
- **Fix/test/upstream report:** none in this repository.

## F-002: SEDLite absolute-time output

- **Status:** suspected configuration/implementation mismatch; source evidence
  established, runtime contents and intended semantics still to be checked.
- **Configuration:** `UseSEDLite: true` and `StoreAbsTime: true`.
- **Source:** [SuperaSimEnergyDeposit.cxx][sed] allocates the `sed_at` product,
  then calls the SEDLite helper with only the energy output. That helper does not
  populate time values; its time extraction is commented out.
- **Question:** should this combination populate time, reject the configuration,
  or explicitly advertise that time is unavailable?
- **Next evidence:** inspect `sed_at` contents in the saved sample and construct
  one deposit with known time. Check the underlying data type's time contract.
- **Fix/test/reviewer decision/upstream report:** none yet. Do not silently add
  time values during a compatibility port.

## F-003: unmatched ancestry and rejected voxel diagnostics

- **Status:** observed diagnostics; correctness unresolved.
- **Evidence:** both ten-event runs report invalid particle/voxel indices.
  Event `13171:1:457` reports two particles without ancestors and summed deposit
  energy `8.73268` in the legacy log's units.
- **Question:** which losses follow documented cuts, which are expected for
  overlay, and which reflect incorrect extraction or grouping?
- **Next evidence:** trace source identities, rejected deposits, and energy
  accounting for one small example. Confirm units rather than assuming them.
- **Fix/test/reviewer decision/upstream report:** none yet.

## F-004: optical relationships omitted by legacy extraction

- **Status:** observed omission; desired output contract under review.
- **Evidence:** the [optical audit](optical-audit.md) found 254 flashes and 82,315
  dereferenceable flash-to-hit links across the ten cached events. SuperaOptical
  copies the flash fields but does not read those links or extract optical truth.
- **Impact:** reproducing legacy Flash objects alone cannot demonstrate complete
  optical input support. Persisted CRT matches also need a separate review.
- **Next evidence:** define relationship identities, retained hit attributes, and
  output representation. Review the available photon truth before choosing a
  matching policy; do not infer that every missing MC match is real data.
- **Fix/test/reviewer decision/upstream report:** none yet; runtime inventory is
  evidence of availability, not a test of a new adapter.

## F-005: signed photon mother IDs require an explicit resolution policy

- **Status:** observed convention; not classified as a bug.
- **Evidence:** 169,190 of 639,997 photons have mother IDs absent from the
  `simplemerge` particles by exact signed lookup, but present with opposite sign.
  None of those exact signed IDs are in the dropped-particle collection.
- **Impact:** naive exact-ID matching loses contributions; blindly discarding
  the sign could conceal simulation semantics. Neither policy is approved.
- **Next evidence:** trace simulation ID assignment, overlay shifts and merging;
  validate identity and ancestry resolution on independent synthetic examples.
- **Fix/test/reviewer decision/upstream report:** none yet.

## F-006: duplicate optical output labels overwrite a collection

- **Status:** locally fixed; reviewer acceptance pending.
- **Baseline:** pinned SuperaOptical loops over input/output pairs and obtains the
  same EventFlash for duplicate output labels. Its second `emplace` replaces the
  first collection; configuration checks only the two lists' lengths.
- **Impact:** a misconfigured job can silently lose one collection.
- **Resolution:** the art composition and Atomic output component reject empty or
  duplicate collection names before opening an output file.
- **Test:** `duplicate-output` in the art suite requires the specific configuration
  failure; the Atomic writer contract test independently checks duplicate names.
- **Reference impact:** none; the frozen cryostat output labels are distinct.
- **Upstream report/reviewer decision:** none yet.

[speedup]: https://github.com/SBNSoftware/sbncode/commit/fb812f1431b207790483ab1d599c0bdee71b8f3a
[sed]: https://github.com/DeepLearnPhysics/Supera/blob/9e7b533d44236c64ddc17748e92dbd9d2fbe0fae/SuperaSimEnergyDeposit.cxx

## F-007: CRT timestamp names do not define a common clock contract

- **Status:** observed producer-dependent semantics, not classified as a bug.
- **Evidence:** ICARUS `v10_27_00`, `CRT/CRTUtils/CRTHitRecoAlg.cc` (`FillCRTHit`)
  stores `time0 % 1000000000` in `ts0_ns`, while `CRTTrueHitRecoAlg.cc` stores its
  full `time0` in the same field. The corrected seconds assignments also differ.
- **Impact:** normalizing or adding fields based only on their names can change
  timing in an overlay collection containing both source paths.
- **Resolution:** preserve the five original readings and exact integer seconds;
  label their reference as source-defined. No timing correction is approved.
- **Tests:** independent field expectations and a 2^53+1 integer counter; exact
  frozen-reference comparison of the resulting LArCV CRT product.
- **Reviewer decision/upstream report:** none. Resolve actual producer epochs
  before adding a normalized time or using these fields for matching.

## F-008: auxiliary compatibility projection must reject information loss

- **Status:** new local safeguards; reviewer acceptance pending.
- **Scope:** the generalized model permits wider IDs, explicit signal units,
  missing estimates, completeness, and timing uncertainty. LArCV2 CRTHit cannot
  faithfully encode every such record. This is not a claim of corrupted baseline data.
- **Resolution:** check the supported source contract and reject incompatible
  quantities or narrowing before modifying output. Initialize every CRT field;
  the LArCV2 default constructor initializes only its instance ID.
- **Tests:** Atomic auxiliary writer contract checks unsupported units, precision,
  range, required values, clocks, duplicate groups, and unsupported information;
  independent readback verifies every serialized field and empty-event clearing.
- **Reference impact:** intended to be none for the supported ICARUS sample.
- **Reviewer decision/upstream report:** none.

## F-009: generator particle momentum and energy use inconsistent units

- **Status:** confirmed legacy mapping; compatibility behavior retained explicitly.
- **Baseline:** `SuperaMCTruth.cxx` at `9e7b533`, ICARUS `v10_27_00`.
- **Evidence:** Particle momentum is multiplied by 1000 while its initial energy
  is copied unchanged from GeV. Neutrino momentum and energy both remain in GeV.
- **Impact:** interpreting both products with one momentum/energy unit convention
  is unsafe. The shared records now have explicit GeV-based field names.
- **Test:** independent 0.25 GeV/c and 1.25 GeV inputs produce the legacy Particle
  values 250 and 1.25, and Neutrino values 0.25 and 1.25.
- **Decision:** isolate this conversion in `project_legacy_generator`;
  changing persisted units requires a separate reviewed output policy.
- **Upstream report:** none.

## F-010: legacy generator identity depends on product availability

- **Status:** confirmed information loss; shared identity corrected locally.
- **Evidence:** `mct_index` counts present products, not original MCTruth indices.
  Missing products do not advance the counter; present-empty products do.
  Multiple interactions in one product therefore share an output index.
- **Resolution:** shared records retain configured source slots, original MCTruth
  indices, and original particle indices. Missing products have an explicit
  availability flag. The LArCV2 compatibility counter remains separate.
- **Test:** missing, present-empty and populated sources, two interactions in one
  source, and status/origin selections retaining original indices.
- **Output impact:** deliberately unchanged for compatibility. A replacement
  format must persist the full source/interaction identity.
- **Reviewer decision/upstream report:** pending; none.

## F-011: legacy generator shape policy excludes positrons from showers

- **Status:** confirmed mapping asymmetry; physical policy correction deferred.
- **Evidence:** legacy code tests `pdg == 11 || pdg == 22`, so `pdg == -11`
  receives track shape. The incoming-neutrino particle keeps unknown shape.
- **Resolution:** do not assign these labels to shared generator records; retain
  the exact rule only in the explicitly named legacy output projection.
- **Test:** independent electron/positron expectations and incoming-neutrino shape.
- **Reviewer decision/upstream report:** no changed labeling policy approved; none.

## F-012: missing generator trajectories are dereferenced without checks

- **Status:** locally guarded; reviewer acceptance pending.
- **Evidence:** legacy reads position/momentum point zero for selected particles;
  native neutrino theta also needs incoming and outgoing momentum points.
- **Resolution:** extraction represents unavailable initial kinematics/theta as
  absent; shared selection filters first and rejects retained malformed records
  explicitly. No fabricated kinematics and no out-of-bounds access.
- **Test:** missing unselected trajectories remain harmless; selected malformed
  particles/neutrinos fail. Origin-rejected malformed interactions are allowed.
- **Reference impact:** expected none for well-formed input.
- **Upstream report:** none.

## F-013: generator output object IDs retain the wrong invalid sentinel

- **Status:** confirmed in the pinned LArCV2 writer test.
- **Evidence:** Particle/Neutrino initialize `_id` to `kINVALID_INDEX` (65535),
  while insertion assigns sequential IDs only for `kINVALID_INSTANCEID`, a
  different value. The legacy processor never assigns object IDs itself.
- **Impact:** these generator objects retain 65535 rather than collection-local
  unique IDs. The event's run/subrun/event identity is unaffected.
- **Resolution:** shared source/interaction/particle indices provide identity;
  compatibility serialization retains the observed sentinel. Do not depend on
  LArCV object IDs as a unique key for these products.
- **Test:** two interactions and multiple particles read back from both trees;
  every object keeps the pinned sentinel while event IDs remain correct.
- **Reviewer decision/upstream report:** output correction deferred; none.

## F-014: voxel shifts narrow valid large IDs to signed int

- **Status:** reproduced in pinned LArCV2 and pre-fix Atomic; local Atomic fix under review.
- **Evidence:** the configured 6144³ grid has 231,928,233,984 cells. Both `shift`
  implementations start with `int id = origin_id`. For voxel `(0,0,100)`, ID
  3,774,873,600, a zero shift returns the invalid sentinel rather than the same ID.
- **Resolution:** Atomic now decodes the full-width origin and bounds-checks signed
  offsets before unsigned arithmetic. No change to coordinate indexing or LArCV2.
- **Tests:** high-ID identity/round trip, each axis boundary, extreme signed offsets,
  invalid origins, uninitialized grid, and exhaustive neighbors on a 2×3×4 grid.
- **Scope:** the migrated optical/auxiliary/generator outputs do not call `shift`.
  Broader labeling impact needs the relevant integration fixtures; no claim of
  complete labeling parity is made.
- **Reviewer decision/upstream report:** pending; none.

## F-015: space-point fields contain producer-specific charge encodings

- **Status:** confirmed convention, not classified as a producer bug.
- **Evidence:** pinned ICARUS cluster3D writes charge into `ErrXYZ()[1]` and charge
  asymmetry into `[3]`; pinned Supera reads those entries accordingly. It computes
  asymmetry but does not serialize its tensor. `StoreWireInfo` is not consulted;
  the `DropOutput` pattern controls that behavior.
- **Impact:** treating these values as a generic covariance matrix is incorrect.
  Copying the interpretation to another SpacePoint producer is unsafe.
- **Next step:** name the input convention explicitly, retain source identity,
  and separate native extraction from shared selection and output policy.
- **Test/evidence:** the cached sample's selected charge is reproduced for
  6,163,365 voxels; other output attributes still require port validation.
- **Reviewer decision/upstream report:** pending; none.

## F-016: malformed space-point associations can leave partial voxel updates

- **Status:** source-confirmed update ordering; no affected points seen in the sample.
- **Evidence:** charge and chi-square are replaced before the more-than-three-hit
  rejection. Invalid planes are skipped individually; repeated planes can overwrite
  prior values and bypass missing-plane filling. Cryostat/TPC retain the last hit.
- **Impact:** outputs can describe different representatives or incomplete topology
  despite a log saying a point was dropped.
- **Next step:** validate candidate associations before committing a representative;
  test malformed cases explicitly and document deliberate differences from legacy.
- **Runtime scope:** zero oversized, duplicate-plane, invalid-plane or mixed-TPC
  associations in the audited 8,839,871 points. This is not a general guarantee.
- **Fix/reviewer decision/upstream report:** none yet; do not silently change policy.

## F-017: source invalidity leaked into shared records

- **Evidence:** OpFlash reserves double max for missing x information; SBN CRT
  reserves float/double lowest, uint64 max and plane -1. nusimdata uses INT_MIN
  for unset codes and double min for neutrino kinematics. The initial converters
  copied these values into apparently populated shared fields.
- **Resolution:** normalize at the input boundary using `supera/data/Validity.h`.
  Preserve valid negative source IDs using optional signed integers. Emit bounded
  source/event diagnostics; keep destination-specific encodings in the writer.
- **Validation:** native default and nonfinite fixtures, partial vectors, exact
  integer clocks, signed IDs, checked index ranges and writer readback tests.

## F-018: infinite CRT position uncertainties in the reference sample

- **Evidence:** both immutable references contain positive infinity in x and z
  errors for 12 hits, totaling 24 fields. The exact list is stored in
  `tests/expected_crt_normalization.json`; source-event reports identify infinity.
- **Resolution:** these values become canonical missing measurements and then the
  explicit LArCV2 CRT float missing value. No finite estimate is fabricated.
- **Validation:** retain raw mismatches; require exact serialized equality after
  only the enumerated reference transformations. Controls detect unrelated changes.
- **Scope:** this establishes unusable uncertainty values, not their producer-side
  root cause. No reconstruction or uncertainty-estimation algorithm is changed.

## F-019: legacy voxel indexing assumes valid floating inputs and count products

- **Evidence:** `ImageMeta3D::id` performs floating-to-unsigned conversion after
  ordinary bounds comparisons. NaN passes those comparisons. `update` multiplies
  axis counts without overflow checks and does not validate finite effective widths.
- **Resolution in the new path:** `FixedGrid` validates metadata before configuring
  the legacy indexer and rejects nonfinite positions before any coordinate cast.
  It checks canonical/native index capacity and translates the legacy sentinel at
  the boundary. Finite out-of-volume points remain normal geometric rejections.
- **Validation:** boundary and overflow fixtures under address, undefined-behavior
  and float-cast-overflow sanitizers; exact reference metadata and sampled coordinate
  agreement for both immutable files.
- **Scope:** direct legacy API callers are not migrated by this change. No claim
  is made that the old API is safe for malformed inputs or that a sample exhibited
  NaN positions. The previous runtime point audit observed no nonfinite positions.

## F020: SpacePoint has two native missing-ID conventions

The pinned lardataobj v10_05_03 default constructor initializes `fID` to `-1`,
whereas `recob::SpacePoint::InvalidID` is `util::kBogusI`. The input boundary
recognizes both as absent; other signed IDs are preserved. Neither convention
is propagated into the shared optional identity. Native converter tests cover
both defaults and a valid negative ID. This does not reserve finite coordinates
or covariance entries as missing.

## F021: FindManyP association order is not serialized order

The pinned canvas v3_16_04 `detail/IPRHelper.h` constructs an
`std::unordered_multimap` and iterates equal ranges to populate `FindManyP`.
The cached sample demonstrates different ordering from the underlying `Assns`.
Consequently, using its iteration order for a last-wins choice can couple results
to an implementation detail. No physics error is claimed from this sample alone.

The new reader preserves serialized order within each point, including repeated
links, while accepting interleaved table rows. An explicit native fixture checks
that ordering. Cached-sample verification compares exact product/key multisets
against `FindManyP`, so no relationship or multiplicity may disappear. Before
porting per-plane last-wins behavior, the selection stage must establish an
explicit policy and characterize any reference differences. Direct table reading
also avoids a vector allocation per point and the hash-based lookup cache.

## F022: Legacy collision replacement can mix different points' attributes

The baseline `SuperaSpacePoint::process` writes charge, chi-square, asymmetry, and
hit count before checking whether hit count exceeds the plane count. Its later
`continue` does not undo that replacement. Other fields may therefore come from
an older point or be absent. Repeated planes and mixed cryostat/TPC links similarly
lack an unambiguous physical interpretation, compounded by F021's iteration order.

The shared selector validates these cases and fails the event before publishing
a result. Every voxel references one complete representative. Zero-hit points
explicitly project missing detector and plane values, avoiding stale attributes.
Synthetic tests cover the safeguards. None of these malformed cases occurred in
the cached sample; all twenty tensors match both references exactly.

## F023: Integer hit identities can silently round in float sparse tensors

Legacy space-point output stores hit keys as float values. Beyond the exact float
integer range, distinct keys can collapse to the same value. Shared records retain
integer keys and product identity. The compatibility writer checks exact float
representability and rejects lossy identities; measurement overflow also fails.
The test key 16,777,217 is rejected. Native sample keys are exactly representable,
so this safeguard does not require a reference-difference allowance.

## F024: Labeling particle lists omit available generator primaries

- **Status:** observed representation difference, not automatically a bug.
- **Evidence:** the [transported-particle audit](truth-particle-audit.md) finds all
  30 generator muons explicitly associated with retained G4 primaries, but only
  17 distinct primary records in each frozen labeling collection. Generator
  records remain available separately. Event 565 track 3 has 465.997542 MeV in
  literal-ID SEDLite deposits but no corresponding SimChannel IDEs under either
  track-ID field. This is not evidence of lost detector charge in Supera.
- **Impact:** interpreting the labeling list as an exhaustive final-state truth
  inventory biases particle counts and can leave daughters without output parents.
- **Proposed resolution:** preserve source inventory and generator associations
  independently of labeling selection, with explicit source-to-group membership.
- **Fix/test/reviewer decision/upstream report:** none; broader fixtures required.

## F025: Available suppression ancestry is not used to resolve broken chains

- **Status:** observed in the ten-event reference.
- **Evidence:** 28 merged particles have broken stored mother chains. Each has
  a unique suppression-map link to a retained primary. Three photon records
  reach output with unresolved ancestor fields and self-parent output IDs.
- **Impact:** primary affiliation can be unavailable downstream despite persisted
  evidence; self-parenting can conceal the distinction from a genuine root.
- **Proposed resolution:** preserve raw parents and suppression links separately;
  resolve available ancestry with explicit evidence, conflict checks, and cycle
  detection. The map does not restore missing immediate parents or trajectories.
- **Fix/test/reviewer decision/upstream report:** none. The audit supplies event
  and track IDs for regression fixtures; no production behavior changed.

## F026: Missing ancestor coordinates overflow LArCV Vertex quantization

- **Status:** observed malformed output, with a source-level conversion mechanism.
- **Evidence:** events/tracks 445/24832, 457/7067, and 565/5087 have `DBL_MAX`
  parent/ancestor times in `mcreco` and `-9223372036854.7754` in both reference
  particle collections. Vertex quantization casts the scaled value to signed
  integer outside its representable range. These are missing coordinates.
  Across each output collection, 40 records have that parent-time value and four
  have that ancestor-time value. Some source parents exist but their positions
  were not populated, so missing metadata is not confined to broken source chains.
- **Impact:** arithmetic transforms an unavailable value into a finite number
  that downstream code may interpret as a physical timestamp.
- **Proposed resolution:** normalize validity before arithmetic; handle the
  compatibility writer's inability to represent a value explicitly.
- **Fix/test/reviewer decision/upstream report:** none for this particle path.

## F027: `mcreco` dropped-particle path does not preserve origin or full metadata

- **Status:** source-established omission with native evidence.
- **Evidence:** larsim `v10_23_00` populates origins and daughters for retained
  particles but not its dropped-particle constructor path, which also lacks
  detector-path points. The fixture produces 46 unknown-origin MCShowers.
  All 1,274 input dropped particles already have only two trajectory points.
- **Impact:** treating derived objects as the transport inventory hides available
  affiliation evidence and conflates unavailable trajectories with physical paths.
- **Proposed resolution:** extract persisted particles and relationships first;
  keep `mcreco` derivation and retention policies explicit. Do not invent missing
  intermediate trajectory points.
- **Fix/test/reviewer decision/upstream report:** none. See the audit for the exact
  signed PDG whitelist, shower threshold, and additional unexercised source risks.

## F028: Persisted transport coordinates can exceed LArCV2 Vertex's numeric range

- **Status:** observed in native source and serialized inventory.
- **Evidence:** 22 of the 1,493 persisted particles contain trajectory coordinates
  with magnitude at least 9e12. The source comparison confirms that these values
  predate the new reader; some exceed 1e35. Their physical interpretation is not
  established by this audit.
- **Impact:** passing such values through the legacy scaled-integer Vertex
  conversion cannot faithfully represent them. This is separate from missing
  parent metadata in F026.
- **Local handling:** retain the exact values and an explicit range-loss count in
  the companion archive; project a documented writer-only unavailable coordinate
  into LArCV. Shared records retain source precision and units.
- **Tests:** synthetic out-of-range-time round-trip and native source/archive
  comparison. See [the transport contract](transport-port.md).
- **Reviewer decision/upstream report:** pending; no upstream production change.

### Transport implementation disposition of F024–F027

The new inventory retains all 30 available primaries (F024), bridges 12 missing
immediate-parent links to resolve the 28 affected primary affiliations (F025),
and does not reproduce overflowed parent/ancestor coordinates (F026). It reads
source associations directly and keeps inferred affiliation separate, avoiding
MCReco's dropped-particle metadata path (F027). These are local implementation and
validation results, not approvals or changes to upstream production. Neither
missing intermediate records nor unsaved trajectory points have been recovered.

## F029: Suppression-map insertion can precede parentage registration

- **Status:** source-established ordering hazard in the original simulation
  release, consistent with native evidence; current upstream status not assessed.
- **Evidence:** larg4 `v10_00_09` `ParticleListAction.cc` calls
  `fdroppedTracksMap[GetParentage(trackID)]` before inserting `fParentIDMap` in
  its low-energy path. `GetParentage` requires that insertion. Later negative
  assigned IDs can therefore be valid while the suppression-map key is
  `NoParticleId`. All 125 initially unresolved reduced-deposit IDs in this fixture
  appear only under that invalid map key.
- **Local recovery:** explicit original/assigned pairs in SimChannels recover
  27 of those IDs; persisted full steps supply the remaining 98. Keep this
  evidence separate from exact identity and from suppression-map affiliation.
- **Impact:** relying solely on the suppression map discards available ancestry.
  Recovery does not recreate suppressed particle records or trajectories.
- **Tests:** namespace-scoped cross-product evidence tests and native input/archive
  comparisons. See [the deposit contract](deposit-port.md).
- **Reviewer decision/upstream report:** pending; no upstream issue or patch sent.

## F030: Legacy geometric cuts act on already reduced deposit bin centers

- **Status:** observed difference, with independently reconstructed cause.
- **Evidence:** original `G4InfoReducer` produces 0.3 cm bin centers. Legacy
  Supera applies an active-volume bounding box to those positions before writing
  its `sed` tensor. On the frozen fixture, 32 records totaling 1.7058666665 MeV
  fail that cut, accounting for all 15 new-only voxels in the inclusive projection.
- **Local policy:** retain all valid source points inside the configured output
  grid; preserve raw points and explicit out-of-grid accounting. Do not assume
  that an outside bin center establishes an outside physical deposition.
- **Numerics:** accumulate in double precision and round to LArCV float only at
  output. Remaining legacy differences fit the float accumulation error bound.
- **Tests:** independent source-sum reconstruction and geometry-filtered voxel
  inventory comparison against both legacy references.
- **Reviewer decision/upstream report:** pending; the difference is intentional
  and documented, not silently classified as exact parity.
