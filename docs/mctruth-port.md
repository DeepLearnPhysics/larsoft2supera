# Generator truth port: audit and contract

Status: checkpoint approved for commit and publication on 2026-11-07. Baseline: Supera
`9e7b533d44236c64ddc17748e92dbd9d2fbe0fae`, ICARUS `v10_27_00`, nusimdata
`v1_29_04`, LArCV2 `v2_2_6`. The inspected baseline `SuperaMCTruth.cxx` is identical
to the corresponding file in the local Supera checkout.

Current validity normalization is specified in [the validity contract](validity.md).
The historical behavior below remains the compatibility audit, not the internal
missing-value standard.

## What the legacy processor does

The effective configuration reads `generator` then `cosmgen`, accepts every
origin (`Origin: 0`), and writes **both** `particle_mpv` and `neutrino_mpv`.
The cached overlay has no `cosmgen`. A missing product is logged and skipped;
a present empty product still advances the legacy producer counter.

For each accepted MCTruth, the incoming neutrino (if set) is appended to the
particle list first, then all particles with status 1 in source order. The
neutrino also becomes a separate Neutrino record, including its full trajectory.
This is generator truth, not the later transported MCParticle/deposit labeling.

| Quantity | Legacy particle product | Legacy neutrino product |
| --- | --- | --- |
| Source identity | `mct_index` is the counter of present producers | Same |
| Track identity | Signed source ID converted to unsigned | Incoming and outgoing-lepton IDs converted to unsigned |
| Position/time | First point, cm/ns unchanged | Same, plus every incoming trajectory point |
| Momentum | First point in GeV/c multiplied by 1000 | GeV/c unchanged, including trajectory |
| Initial energy | GeV unchanged | GeV unchanged |
| Process, PDG | Copied | Incoming process and PDG copied |
| Parent/ancestor | Self track ID and PDG, not source ancestry | Not applicable |
| Shape | Incoming neutrino unknown; PDG 11 or 22 shower; everything else track | Not applicable |
| Interaction fields | Current and interaction type on incoming neutrino only | Current, mode, type, target, nucleon, quark, W, x, y, Q², theta |
| Remaining fields | LArCV constructor defaults | LArCV constructor defaults |

The inspected constructors initialize object IDs with the 16-bit invalid index,
while collection insertion checks the different invalid-instance sentinel. Thus
the objects retain ID 65535. Independent readback tests confirm this behavior. Multiple MCTruth records
from one producer share the legacy `mct_index`; a missing preceding producer
shifts it. This is insufficient as a stable interaction identity.

## Issues and compatibility decisions

- Mixed particle momentum/energy units are preserved **only at the compatibility
  boundary**. Shared fields explicitly use GeV, GeV/c, GeV², cm and ns.
- Source identities include the configured source slot, original MCTruth index,
  and original particle index. Missing sources remain explicit records. The
  legacy output counter is derived separately and is never the shared identity.
- Positron PDG -11 maps to track in the old implementation. Preserve this in the
  named legacy projection for exact comparisons; do not make it a shared label.
- Self-parent/self-ancestor values are compatibility placeholders, not recovered
  generator genealogy. No shared ancestry is fabricated.
- A selected particle or incoming neutrino with no trajectory is rejected with
  context instead of blindly reading point zero. Unselected records need not
  have a trajectory. A declared required product must exist; optional missing
  products remain distinct from present-empty products.
- Generator metadata, daughters/mothers, outgoing-lepton kinematics, and particle
  trajectory tails are not part of this port. Record omissions rather than
  implying complete generator support. The incoming neutrino trajectory is kept.

## Shared contract and boundaries

`GeneratorCollection` contains ordered sources and `GeneratorRecord` entries.
Each record corresponds to one producer-defined MCTruth grouping, potentially
with multiple particles; it need not be one physical interaction or common vertex.
The `mpv` name is only a configurable output label, not a grouping rule.
The source slot and original indices survive selection. Native extraction copies
fields into owning standard-library records without LArCV types or constants.
A separate Atomic selection function applies origin and final-state status rules;
the LArCV writer performs the explicitly named legacy projection. It does not
silently impose a new physical interpretation on raw fields.

The existing Atomic `Particle` models transported tracks with deposition and
labeling metadata. It is not reused as a generator record: doing so would conflate
generator identity, Geant4 ancestry, and final labeling. Any future connection
between those models requires explicit associations.

## Validation plan

Independent fixtures cover neutrino and non-neutrino MCTruth, multiple sources
and interactions, origin/status filtering, signed track IDs, positrons, trajectory
absence, source ownership, and empty-event reset. Writer tests independently
check fields, units, defaults and IDs. The cached sample must match both frozen
references exactly for both `mpv` trees while the optical and auxiliary regressions
continue passing. Intentional safeguards have separate failure tests.

## Build and CI scope

Native generator conversion depends on ROOT-backed nusimdata. The lean hosted
image continues to test optical/auxiliary extraction and now tests shared generator
selection without ROOT. The configured ICARUS integration builds the genuine
converter and writer, runs field/selection/readback fixtures, and performs frozen
reference comparisons. `tools/generator-checks.sh` additionally measures converter
coverage (90% line and 100% function gates) and instruments our converter/test
with address and undefined-behavior sanitizers; external ROOT/nusimdata libraries
are not instrumented. No mock MCTruth or substitute four-vector type is used.

`LARSOFT2SUPERA_BUILD_GENERATOR` defaults to the art build setting. The
composed writer requires it; extraction-only builds can disable it explicitly.
The installed generator target is `larsoft2supera::generator`.

The new FHiCL deliberately makes two policies explicit: each source has a
`Required` flag (default true), and each collection requests
`Projection: "LegacySuperaMCTruth"`. Omit `Origin` to accept all origins;
`Origin: 0` now selects the actual unknown-origin code. Do not copy the old
zero-means-all setting into the new configuration. The supplied overlay FHiCL
omits it, preserving the baseline selection.

The coordinated Atomic dependency is
`bd218e470d5996eb8ae13182627a1b3a4c5887f1`, pinned in the fast-check script and
both adapter C++ workflows. Full ICARUS integration requires externally
provisioned fixtures; the local results do not imply a hosted integration run.

See findings F-009 through F-013 for confirmed legacy quirks and deliberate
safeguards. Exact agreement validates the compatibility projection, not those
quirks as scientifically correct conventions.

## Local results

The ten cached ICARUS events match both frozen references exactly for all five
migrated product trees: 50 product-events and 1,453,104 serialized bytes per
reference. Optical-only and optical-plus-CRT regression comparisons also pass.
The generator additions contribute both `particle_mpv` and `neutrino_mpv`.

Independent shared-selection, native extraction, and LArCV readback tests pass.
The art fixture covers two interactions and nonempty/empty/nonempty events, with
an explicitly absent optional source. Required missing sources, duplicate source
tags, and unsupported projections fail as expected, alongside the existing eleven
optical/auxiliary failure cases.

Native converter coverage is 48/50 lines (96%), 3/3 functions (100%), and 51/88
branches (58%). The report includes allocation/exception branches; no allocation
failure is injected. Art integration is tested with genuine products rather than
mocked framework handles. Local artifacts are in the lab under
`results/mctruth-validation-02/` and `results/mctruth-native-02.log`.

The converter and shared-selection tests also pass with address and
undefined-behavior sanitizers enabled. Static analysis of the shared selection
and existing lean converters, formatting checks, and documentation lint pass.
The new native converter uses strict compiler warnings and native runtime checks;
it is not included in the ROOT-free clang-tidy invocation.
