# Findings ledger

This ledger separates observations from confirmed bugs. No legacy behavior has
been fixed by this repository yet. Reports should remain useful to upstream
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

[speedup]: https://github.com/SBNSoftware/sbncode/commit/fb812f1431b207790483ab1d599c0bdee71b8f3a
[sed]: https://github.com/DeepLearnPhysics/Supera/blob/9e7b533d44236c64ddc17748e92dbd9d2fbe0fae/SuperaSimEnergyDeposit.cxx
