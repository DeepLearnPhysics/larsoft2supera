# Staged migration

## Review sequence

The initial foundation and beginning the implementation were authorized on
2026-11-07. Each stage remains a small review boundary; authorization to start
does not mean that future behavior changes have been approved. Complex stages
should span several focused, tested commits.

The optical checkpoint was authorized for commit and publication on 2026-11-07.
The auxiliary-detector checkpoint was authorized for commit and publication on
2026-11-07; see the [auxiliary review packet](auxiliary-port.md).

**Output decision:** retain LArCV2 throughout the parity migration. First reproduce
the agreed LArCV2 products. The revised architecture establishes output-independent
Atomic records and a separate optional writer target now; a replacement format
remains deferred. See the [optical port](optical-port.md).

| Stage | Scope | Evidence needed before proceeding |
| --- | --- | --- |
| 0 | Inventory, ownership boundaries, frozen reference manifest, contribution rules. | Review the processor list and distinguish compatibility evidence from correctness. |
| 1 | Event identity and optical flashes. Establish the smallest useful art-to-value conversion. | Field-by-field contract; units/time origin; collection-local identity; empty/missing inputs; two-event isolation; comparison with both reference flash products. |
| 2 | Auxiliary records (ICARUS CRT first), then MCTruth interaction/generator records. | Experiment-specific CRT mapping; timestamp types/precision; truth origin filtering; multiple generators/interactions; missing `cosmgen`; particle/neutrino units. |
| 3 | Detector context, fixed grid, reconstructed points and hit associations. | Coordinate transforms and boundaries; both cryostats; invalid/dangling associations; shared hits; duplicate voxels; charge selection and tie-breaking. |
| 4 | Truth particle/deposit extraction, separately for SEDLite and SimChannels. | Track-ID identity and sign/offset policy; ancestry; dropped particles; units; conservation accounting; unassociated deposits. Start with extraction, not the complete grouped `sed` output. |
| 5 | True-to-reco matching, then particle grouping and semantic labeling. | Traceable association evidence; independently computed small cases; intermediate group comparisons; merge order; per-event reset; overlay background policy. Shared-core changes are separate changes. |
| 6 | Complete orchestration, tensor/particle post-processing, filtering and charge rescaling. | Component checks plus exact end-to-end comparison where behavior is preserved; every intentional difference explained and approved; event selection and output alignment checked. |
| 7 | Other experiments and input adapters. | Detector-specific integration fixtures and shared contract tests before claiming portability. |

Pure tensor/filter tests can be developed earlier in the shared-core work. The
table orders the adapter milestones; it does not require copying shared
post-processing into `larsoft2supera` or waiting until stage 6 to test algorithms.

## First implementation proposal

Start with the two `SuperaOptical` input collections. They exercise real LArSoft
product access and variable-length vectors without requiring ancestry, matching,
or semantic labels. They also have frozen reference products for comparison.

The first change should contain:

1. A written flash-record contract, including units, time origins, validity,
   collection identity, and all fields preserved or deliberately excluded.
2. The smallest useful conversion interface, with explicit ownership. LArCV2
   integration is allowed during this compatibility phase; keep serialization
   details localized for later writer extraction.
3. A conversion function that can be tested without starting art, plus a thin
   framework wrapper responsible for product lookup and event context.
4. Synthetic tests for field mapping, vector lengths, numerical edge cases, and
   event isolation; a cached-art integration test against the frozen flash data.
5. C++ formatting/static analysis, a pinned build, test execution, and a coverage
   report in CI. No success badge based only on documentation lint.

The local v2 Atomic data component now defines flash records and an optical event
envelope. Its existing particle labeling API is unchanged during this first slice.
The [optical audit](optical-audit.md) expands this review beyond field-copy parity:
inventory associations, available truth, producer defaults, and information the
legacy writer does not represent. A compatibility test cannot approve an omission.

The generator checkpoint was authorized for commit and publication on 2026-11-07;
see the [MCTruth audit and contract](mctruth-port.md).

The next stage begins with the [fixed-grid and space-point audit](spacepoint-port.md).
Its first implementation slice is the fixed grid, before point/hit extraction
and collision selection. The audit accompanies the validity checkpoint.

## Review packet for every behavior

- **Legacy evidence:** pinned source, effective configuration, and a small trace.
- **Contract:** inputs, outputs, units, frames, identities, ownership, lifetimes,
  ordering, missing/empty behavior, and error handling.
- **Independent expectations:** a worked example, invariants, conservation checks,
  and boundary/error cases. Do not generate all expected values from the old code.
- **Comparison:** changed functions, affected products, exact differences, and
  tests actually run. State which integration tests were unavailable.
- **Decision:** preserve behavior, deliberately correct it, defer it, or reject
  the proposal. Record the reviewer/date and link any finding and regression test.

Prefer separate changes for extraction, algorithm movement, policy correction,
and performance optimization. A mismatch is evidence to investigate, not an
invitation to relax a numerical tolerance until a test passes.

## Reference strategy

Keep the existing stage0 input, augmented cluster3D cache, and both LArCV outputs
immutable. The [manifest](reference-baseline.json) identifies them by checksum.
Use cached reconstruction for integration; do not repeatedly run upstream
reconstruction for a converter change.

For each component, prepare a small, documented fixture and compare all relevant
fields before moving to the next component. Use exact equality when the contract
preserves bits and order. Any numerical tolerance or changed ordering must have
a reason, an approved contract, and tests showing what it permits and rejects.

The existing ten-event equivalence test compared the standard chain with staged
legacy Supera. It did not compare SuperaAtomic, exercise every branch, or prove
that the old labels are physically correct.

## Decisions still open

- Public event/auxiliary-data contract and where its types live.
- Build integration and supported experiment/compiler matrix.
- Location and availability of lightweight integration fixtures and CI runners.
- Repository license and attribution before importing implementation code.
- Replacement output format, after LArCV2 parity and writer separation.

The other adapters will be reviewed after this boundary is demonstrated, using
the same contracts and failure cases rather than requiring identical internal code.

The existing input converters now follow [the shared validity standard](validity.md).
The native reference has 24 explicitly enumerated CRT uncertainty fixes; all other
serialized fields remain exact. Review this normalization checkpoint before the
next product port. The adapter pins the coordinated Atomic validity checkpoint.

Performance is now an explicit per-port acceptance criterion. The
[performance contract](performance.md) preserves the legacy event/module timing
baseline and defines separate input-access, conversion, association, labeling and
output measurements. Per-product parser timings remain to be measured before
accepting the next reader; total partial-adapter runtime is not a legacy speedup.

### Space-point extraction review

Native point and optional hit parsing now uses Atomic-owned records with no LArCV
dependency. [The parser contract](spacepoint-parser.md) describes input profiles,
association ownership, validation, and remaining scope. [Measured timings](performance.md)
separate loading from conversion. The [completed fixed-grid slice](spacepoint-output.md)
now reproduces all twenty space-point output trees exactly against both references.
The space-point checkpoint was authorized for commit and publication on 2026-11-08.
The fast and native C++ workflows pin the coordinated Atomic space-point commit.

### Transported-particle truth review

Before implementing the next reader, the [truth audit](truth-particle-audit.md)
traces original simulation retention, regenerated `mcreco`, and frozen output.
It separates the available transport inventory from labeling groups, identifies
recoverable ancestor affiliation and invalid output values, and records the
limits of this cosmic-muon fixture. The audit preceded the direct transport implementation
described below.

The [transport inventory implementation](transport-port.md) now provides direct
MCParticle extraction, shared ancestry resolution, and LArCV2 projection with a
source-preserving companion archive. It removes MCReco from this input path;
charge-based labeling and calorimetry remain later stages. The intentional
inventory/legacy-labeling differences are documented separately
from final semantic grouping.

### Deposit extraction and attribution

The [deposit contract](deposit-port.md) adds reduced energy points, full simulation
steps, and readout IDEs without MCReco. Shared attribution uses explicit identity
and cross-product ancestry evidence; LArCV projection remains in Atomic's writer.
The profile is suitable for a truth inventory/deposit audit, but does not yet
provide semantic grouping or true non-ghost labels. Findings F029–F030 record
upstream ancestry and reduced-position acceptance issues. The transport/deposit checkpoint was
authorized for publication on 2026-11-09,
with the adapter pinned to the coordinated Atomic revision. Cluster merging,
semantic labeling, and reconstructed truth matching remain subsequent stages.
