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
