# Transport inventory and ancestry

This slice reads persisted transport truth directly, with no MCReco producer or
MCTrack/MCShower input. It is a particle inventory, not a replacement for the
charge-based labeling and shower-grouping algorithms. Those algorithms will
consume this inventory in subsequent slices.

The [pre-port audit](truth-particle-audit.md) explains why generator truth,
transported particles, and selected labeling particles must remain distinct.
This transport/deposit checkpoint was authorized for publication on 2026-11-09.

## Boundaries and ownership

- `larsoft2supera/Transport.h` and `src/Transport.cxx` copy native MCParticle value
  types without art services, LArCV, PDG selection, energy cuts, or voxel selection.
- `ReadTransport.h` loads explicitly configured products, attaches typed MCTruth /
  GeneratedParticleInfo associations, and preserves suppression ancestry maps.
- Atomic's `data/Transport.h` owns source records and relationships. Its
  `process/ResolveTransport` resolves ancestry without a framework or output dependency.
- Atomic's LArCV2 output layer writes a particle projection and a companion ROOT
  archive containing the fields that LArCV2 Particle cannot represent.

Every event is independent, including repeated event numbers in different subruns.
Particle product/key identities, signed native IDs, source ordering, and collection
membership remain available. Each configured transport collection is one track-ID
namespace. Unrelated namespaces must use separate collections; no implicit offset
or absolute-value transformation is applied.

The source MCParticle fields preserved are track and mother IDs, daughters, signed
PDG, status, creation/end processes, mass, weight, polarization, generator vertex,
rescatter code, every stored trajectory position/momentum, and trajectory process
indices, codes, and decoded names. Trajectory units are cm, ns, GeV/c, and GeV.
The generator vertex retains its native generator convention separately.

Optional identities/codes use the shared optional representation; unavailable
measurements use NaN. Required particle identities must exist and be unique within
one namespace. Missing trajectories are allowed. Persisted dropped-particle storage
is explicitly distinguished from retained storage; neither is a guarantee of a
complete unsparsified Geant4 trajectory.

## Ancestry contract

`mother_id` and `daughter_ids` always preserve the source relationships. Derived
indices never overwrite them:

| Field | Meaning |
| --- | --- |
| `primary` | Source creation process is `primary` and mother ID is a configured root marker |
| `direct_parent` | Index of the actual referenced mother, when that record exists |
| `available_parent` | Direct parent, or the nearest uniquely supported available ancestor |
| `primary_ancestor` | Resolved primary record, including through a supported bridge |
| `ancestry` | Primary, Direct, Bridged, Unresolved, Conflict, or Cycle |

Root markers default to `[0]` and are configurable for upstream conventions and
explicit overlay offsets. A missing parent does not make a secondary a primary.
A process/root-marker disagreement remains unresolved rather than being guessed.
Source-primary classification is distinct from generator final-state status.

Suppression-map entries describe ancestry, not immediate parenthood. When the
mother is absent, the resolver considers persisted evidence about the particle
and missing mother. Multiple candidates in a proven chain can resolve to its
nearest available member. Unrelated candidates remain conflicts. Direct source
parents remain intact even when other evidence contradicts them; contradictory
primary affiliation is marked unresolved/conflicting and propagated to descendants.
Explicit MCTruth product/key associations are checked against resolved roots when
both are available. Generated particle indices are never inferred from track IDs.

Traversal uses sparse identity lookup and an iterative memoized walk with cycle
handling. Large track offsets do not allocate arrays indexed by the largest ID.
Source daughter lists are retained independently; output children are derived from
validated direct-parent references, not invented from a bridged relationship.

An inferred generator affiliation is obtained through `primary_ancestor` and its
explicit `truth` link. It is not written into a dropped particle's `truth` field
as though an explicit source association existed.

## Configuration and missing inputs

`fcl/transport_icarus.fcl` reads the retained and dropped `largeant::G4` products,
its ancestry map, and retained-particle truth associations. It also runs the
existing generator projection. No reconstruction or `mcreco` is scheduled.

`TransportCollections` accepts `OutputLabel`, `Sources`, `AncestryMaps`, and
`RootMotherIDs`. Each source has `InputTag`, `Storage` (`Retained` or `Dropped`),
and optional `TruthAssociations`. Configured particle products/maps are required;
an absent product fails rather than masquerading as an empty collection. Repeated
products, duplicate track IDs, invalid trajectory process indices, unavailable
association targets, and out-of-range generator indices fail explicitly.

A present source without an association for a particular particle remains explicit
missing information. Association extraction never guesses an MCTruth from a vertex.
This profile reads scalar ParticleAncestryMap products. Vector-valued merged maps
and detector-specific offset transformations require an explicit future profile.

## Output and representational limits

For `OutputFile: "transport.root"`, output comprises:

- `transport.root`: LArCV2 `particle_transport` plus the configured generator products.
- `transport.root.transport.root`: versioned migration archive with `transport`
  and `events` TTrees and a `schema` metadata object.

The companion is an audit/preservation mechanism, not the proposed long-term
replacement output format. Both files are required for the full record. A failed
job may leave partial files; successful completion and validation are required.

The archive stores all converted source fields, source identity, explicit truth
links, derived relationship indices, complete suppression-map pairs, and configured
root markers. Its event table records even present-empty inventories. Trajectories
are flattened in stride-eight order: x, y, z, t, px, py, pz, energy. Enumerations use
the order declared in `data/Transport.h` (storage: Retained=0, Dropped=1;
ancestry: Primary=0, Direct=1, Bridged=2, Unresolved=3, Conflict=4, Cycle=5).

The LArCV projection keeps one particle per source record, initial/final positions,
initial momentum/energy in MeV units, direct-parent metadata, and resolved primary
metadata. No charge deposits, detector entry/exit points, semantic groups, or
physical interaction IDs are fabricated. Initial/final transport positions are
not equivalent to detector first/last deposition positions. Missing direct-parent
indices stay invalid even when a primary affiliation has been recovered.

LArCV2's Vertex API quantizes coordinates through a signed integer. The writer
therefore reserves **-9e12** as a writer-only unavailable coordinate and uses it
for missing or unrepresentable coordinates (`abs(value) >= 9e12`). It never passes
NaN or those out-of-range source values into Vertex. This convention is recorded
in the companion schema; it is not the shared validity standard or an existing
universal LArCV convention. Raw finite source values remain exact in the archive,
which also records `unrepresentable_trajectory_coordinates` per particle.

Signed negative particle IDs are supported by the shared inventory, but this
LArCV projection rejects them rather than wrapping them into unsigned values.
Such a profile needs a separately reviewed output identity mapping.

## Comparison with the frozen sample

The final checked-in integration harness (`results/transport-integration-01`)
passed against both frozen references, with identical comparison reports:

| Quantity across ten events | Result |
| --- | ---: |
| Persisted transport records | 1,493 |
| Stored trajectory points preserved | 34,521 |
| Source primaries retained | 30 |
| Same primaries represented separately in legacy labeling | 17 |
| Missing direct parents | 12 |
| Particles affected by broken source chains | 28 |
| Unresolved primary affiliations after bridging | 0 |
| Legacy records present in the new inventory | 123 / 123 |
| PDG / initial-energy differences for those common tracks | 0 / 0 |
| Changed primary ancestor IDs among common tracks | 4 |
| Legacy overflowed parent timestamps reproduced | 0 |
| Records with coordinates outside safe LArCV Vertex range | 22 |

Counts compare inventories, not semantic grouping equivalence. The 1,370 additional
records are not 1,370 new physical discoveries: legacy grouping/selection omitted
or combined them. All 13 additional primary records were already available in
source truth, including primaries without charge entries. The four changed ancestor
IDs include the three orphan shower roots and a descendant identified in the audit.

The independent native checker reads serialized archive fields and compares them
against the original MCParticles and typed generator associations without calling
the new converter. The extended checker also compares suppression maps. Tests
exercise empty trajectories, nonfinite measurements, signed identities, large
track offsets, missing/ambiguous ancestry, nested map evidence, cycles, conflicting
MCTruth evidence, repeated resolution, and a 20,000-record chain. Writer tests
exercise missing coordinates and exact preservation of a time outside LArCV's range.

`tests/transport_integration.py` runs the profile, source checker, comparisons to
both reference files, and timing summary. It is included in `tools/integration.sh`.
`tools/transport-checks.sh` gates converter line/function coverage; the native
sanitizer suite includes the converter. Atomic's framework-free CI builds and tests
ancestry with sanitizers and static analysis. The adapter pins the coordinated Atomic revision
in its check script and C++
workflows; native checks exercise the same shared implementation.

`TransportTimingFile` separates input reading/conversion, ancestry resolution, and
writer time. Writer time includes every configured product plus the companion.
The final ten-event run measured medians of **8.23 ms/event** for reading and
conversion, **1.55 ms/event** for ancestry resolution, and **10.80 ms/event** for
writing the configured products and companion. These are warm local measurements
under x86-64 emulation, not a cross-platform benchmark or a comparison with the
full legacy labeling job.

Validation passed all 13 native Atomic tests and all five native adapter tests,
plus the independent ten-event source/archive checker and both legacy comparisons.
Ancestry and native extraction passed address/undefined-behavior sanitizers.
Pinned clang-tidy reported no unsuppressed findings for the new ancestry code.
Measured line/function coverage was 97.6% / 100% for ancestry and 97.9% / 100% for
extraction. Branch coverage is lower because compiler-generated exception cleanup
and some malformed-input paths are not all exercised. The extraction line not
covered is rejection of a corrupt trajectory process index, which cannot be
constructed through the normal public source API. No claim of exhaustive testing
or neutrino validation follows from these results.

## What is still unavailable or unvalidated

The reader cannot restore absent intermediate particle records, unsaved trajectory
points, or all simulation activity below upstream retention thresholds. Bridging
recovers affiliation, not the omitted particle's properties. Extraordinarily large
persisted coordinates are retained as source evidence; their physical validity
needs a separate upstream investigation and is not assumed here.

This sample contains cosmic-origin muons, not neutrino interactions. Neutrino
final-state completeness, nuclear and antiparticle behavior, multi-generator
products, and real production overlay-offset conventions still need fixtures.
Generator parsing retains its existing contract; this slice does not claim a new
lossless serialization of every MCTruth/generator field.

Charge assignment, deposited energy, shower/track labeling, and replacement of
MCReco-derived calorimetry are not implemented by this inventory slice. The old
reference still uses MCReco; the new inventory path does not. Future labeling
must retain source membership and explain each merge or exclusion separately.
