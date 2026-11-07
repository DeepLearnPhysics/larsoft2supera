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
