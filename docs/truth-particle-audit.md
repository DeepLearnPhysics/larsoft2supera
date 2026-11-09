# Transported-particle truth audit

This is a pre-implementation review, dated 2026-11-08. No transported-particle
reader, labeling correction, or compatibility exception is approved by this audit.

The central finding is that the generator inventory, persisted Geant4 inventory,
`mcreco` objects, and Supera labeling particles are different representations.
Neither `mcreco` nor `particle_pcluster` is a complete analysis truth inventory.
The new adapter should preserve the available source inventory and relationships
before shared labeling applies cuts or merges particles.

## Evidence and reproducibility

The ten cached events and frozen LArCV files are identified in
[the reference manifest](reference-baseline.json). Event IDs are run 13171,
subrun 1, events 373, 385, 409, 433, 445, 457, 493, 529, 541, and 565.

| Layer | Audited version and evidence |
| --- | --- |
| Original simulation | ICARUS `v10_06_00_06p03`; dependency chain resolves to larg4 `v10_00_09` |
| Original retention configuration | Actual `G4` process parameter set read from the cached event provenance |
| Regenerated `mcreco` | larsim `v10_23_00` in ICARUS `v10_27_00`, using the frozen stage-1 configuration |
| Legacy Supera | Frozen sbncode submodule `9e7b533d44236c64ddc17748e92dbd9d2fbe0fae` |
| Output | Existing `particle_pcluster` and `particle_corrected` trees, aligned by run/subrun/event |

Read-only audit sources live in the migration lab's
`research/truth-particles/`: `TruthParticleAudit_module.cc`, `CMakeLists.txt`,
`job.fcl`, `run.sh`, and `summarize.py`. Source snapshots retain the two different
upstream release versions. Results are in `results/truth-particle-audit-03/`:
CSV inventories, actual `G4-resolved.fcl`, `summary.json`, and `run.log`.
The native job passed all ten events. It regenerated only `mcreco`; it did not
rerun legacy Supera or upstream reconstruction, or modify reference files.

Inside the configured container, run `bash /work/research/truth-particles/run.sh`.
The script refuses to reuse its existing results directory; select a new directory
for another run. Summarize with
`python3 research/truth-particles/summarize.py results/truth-particle-audit-03`.
The audit's CSV keys are sufficient for this single-product, single-subrun fixture;
they are not a proposed general identity schema.

This fixture has **30 status-1 muons, origin `kCosmicRay`, in ten MCTruth records**,
not neutrino interactions. Every generator particle has exactly one retained G4
primary through explicit `GeneratedParticleInfo` associations. Generator and G4
track numbers were not equated to establish that relationship.

## Upstream retention is already lossy

The original `ParticleListAction` settings are `EnergyCut: 0.5`,
`keepEMShowerDaughters: false`, `KeepDroppedParticlesInVolumes: [volTPCActive]`,
`storeTrajectories: true`, `SparsifyTrajectories: false`, and
`keepOnlyPrimaryFullTrajectories: false`. Generator trajectory labels are
`cosmgen` and `generator`. Another candidate configuration in the ROOT parameter
database has a different cut; it is not the actual `G4` process configuration.

In the pinned source, the secondary kinetic-energy comparison uses Geant4's
internal energy units (MeV), making this a 0.5 MeV cut. Selected EM production
processes are suppressed from the retained collection. The optional dropped
collection saves selected suppressed particles in configured volumes, with only
start/end trajectory points. Secondaries below the energy cut can be absent
even from that collection. An omitted particle's charge identity may be redirected
to a negative ancestor ID; original IDs and suppression ancestry are separate data.

The sample contains 219 retained particles and 1,274 dropped particles; their
`simplemerge` union has 1,493 records. Every dropped particle has exactly two
trajectory points. A full trajectory cannot be reconstructed from those endpoints.
The persisted list is therefore not the complete Geant4 tracking history.

Relevant source: [ParticleListAction.cc at larg4 v10_00_09][particle-list].
In particular, inspect `PreUserTrackingAction`, `PostUserTrackingAction`, and
`FillMCParticleCollection`. Dropped storage is enabled by the volume configuration;
an isolated legacy module flag is not sufficient evidence that it is disabled.

## What `mcreco` preserves and discards

The effective `SavePathPDGList` is:

```text
13 -13 211 -211 111 311 310 130 321 -321 2212 2112
2224 2214 2114 1114 3122
1000010020 1000010030 1000020030 1000020040
```

This is an exact signed whitelist. Antiprotons, antineutrons, anti-Lambdas, and
other unlisted species do not receive the same MCTrack treatment. Electron,
positron, and photon ancestry instead feeds shower grouping. This is a derived
representation policy, not a defensible whitelist for an analysis inventory.

Additional source-established behavior:

- `MCRecoPart` reads retained-particle MCTruth origins and daughters, but its
  dropped-particle constructor path does not populate origin, daughters, or
  detector trajectory. In the sample, 46 of 69 MCShowers have unknown origin.
- Detector trajectory acceptance uses the bounding box enclosing all TPCs,
  rather than the union of active volumes. Inter-TPC gaps can fall inside it.
- A whitelisted track with zero detector-path points is kept; a nonempty-path
  track without an energy-deposit lookup entry is skipped. Thus MCTrack presence
  is not simply a PDG or geometry selection.
- Shower selection uses initial total energy with `MinShowerEnergy: 10` MeV,
  not deposited energy. `MinNumDaughters` is zero here. Shower grouping collapses
  EM ancestry and is not one MCShower per transported particle.
- Missing parents can survive as unresolved IDs accompanied by invalid PDG and
  position values. Mother/ancestor traversal has no general cycle guard.
- This job uses `filtersed` SEDLite for `mcreco`, whereas Supera's particle
  matching uses `merge` SimChannels and `UseOrigTrackID: true`. Their inclusion
  and energy accounting must not be assumed identical.

The regenerated objects comprise 37 MCTracks and 69 MCShowers. Relevant source:
[MCRecoPart][mcreco-part], [MCTrackRecoAlg][mcreco-track],
[MCShowerRecoAlg][mcreco-shower], and [MCReco_module][mcreco-module].

## Observed ancestry failures and recoverable evidence

Following exact stored mother IDs in the merged inventory leaves 28 particles
with an unresolved ancestor. No cycles were observed. All 28 have a unique direct
entry in the persisted `ParticleAncestryMap` pointing to a retained primary.
Legacy Supera does not consume that map.

This can recover primary affiliation in these cases, but cannot recreate missing
immediate parent records, their properties, or the original complete lineage.
The map is a suppression/ancestor relation, not an immediate-parent table. Across
the full map, some descendants appear under multiple ancestors; a general resolver
must preserve and validate that information rather than take the first match.

Three broken chains reach the output as self-parented particle records:

| Event | Source track | Missing source ancestor | Map's retained primary |
| --- | --- | --- | --- |
| 445 | 24832 | 22367 | 1 |
| 457 | 7067 | 2181 | 2 |
| 565 | 5087 | 1357 | 2 |

All three are photon records with zero output voxels. Their ancestor PDG is
`2147483647` and interaction ID is `9223372036854775807`. Their missing parent
and ancestor times start as `DBL_MAX` in `mcreco` and appear in LArCV as
`-9223372036854.7754`. LArCV Vertex quantization converts `value * 1e6` through a
signed integer, which cannot represent that sentinel. These are invalid values,
not physical timestamps. Both output collections show this behavior.

The issue extends beyond those three roots: each collection contains 40 records
with this invalid parent-time value and four with this invalid ancestor-time
value. The fourth ancestor case is event 457 track 7074, a descendant of track
7067. Parent metadata is also left unavailable for some records whose source
parent exists: for example, event 373 track 405 has parent track 3 in the source
and output, but this malformed parent time. The legacy daughter loops propagate
ancestor metadata without consistently populating direct-parent positions.

Another 12 output records have a missing output parent index but a valid source
parent ID: their parents were filtered from the output collection. That is distinct
from missing source ancestry. Among the remaining parent/child output edges, the
audit found no dangling child IDs or reciprocal-edge discrepancies. The effective
legacy configuration nevertheless has `CheckParticleValidity: false`.

## Final-state primaries versus labeling particles

All 30 generator final-state particles are present as retained G4 primaries.
Only 17 appear as distinct particles in each of `pcluster` and `corrected`.
Thus **13 primaries are absent from those labeling lists**, not absent from the
entire file: the generator output remains a separate inventory.

An illustrative case is event 565, G4 track 3: a primary antimuon with initial
energy 1,983.320691 MeV, 441 stored trajectory points, and a regenerated MCTrack.
There are 6,069 SEDLite deposits under its literal ID, totaling 465.997542 MeV,
but no `merge` SimChannel IDE entries under either its track or original-track
ID (also checking the opposite sign). Its absence from a charge-based labeling
list is not evidence that Supera lost those SEDLite deposits. It does demonstrate
why that list must not define the full analysis truth inventory.

The other 12 omitted primaries have no SEDLite deposits under their literal ID;
that alone is not a claim about every reassigned or descendant contribution.
The legacy retention code explicitly protects MCShower roots, while other groups
face validity, voxel-count, and low-energy-scatter selection. MCTrack existence
and primary status do not guarantee a distinct output particle.

Further policy differences matter for future neutrino samples:

- Initial group construction excludes large nuclear PDG codes. Later parent
  insertion can restore some records, so it is incorrect to claim all nuclei
  are always absent. Nuclear inventory and labeling policy require separate tests.
- Ionization, delta-ray, shower, and low-energy grouping can merge source particles.
  Output parent/group IDs therefore need not represent direct transport ancestry.
- Legacy interaction IDs are assigned from ancestor vertices, not explicit
  MCTruth product/key identity. This sample has one MCTruth containing three
  generated muons per event, but generally three valid legacy interaction IDs.
  This is a semantic difference, not proof that vertex grouping is universally wrong.
- The output record is a selected summary: it does not preserve full trajectories,
  transport status, end process, and explicit generator-index provenance together.

Relevant source: [SuperaMCParticleCluster.cxx][cluster]. Review initial grouping,
the MCShower retention set, `SetParticleAncestory`, interaction assignment, and
`MakeParticle` separately; each changes the meaning of the resulting record.

## Additional source-level risks, not reproduced failures

The frozen [MCParticleList][particle-cache] caches by run and event without subrun;
its cached run/event members are not initialized in the class declaration or
constructor. It builds dense arrays indexed by track ID, which becomes expensive
with large overlay offsets. Convenience arrays take absolute PDG/track values,
while some indexing uses raw signed IDs without a lower-bound check. Traversal
lacks a cycle guard. These need independent regression cases; this fixture does
not establish their runtime impact.

`mcreco` also accepts duplicate IDs into its particle vector while lookup-map
insertion retains the first mapping. No duplicate or negative particle IDs were
observed in this fixture. Signed deposit IDs are a separate convention and must
not be treated as proof of malformed particle IDs.

## Proposed next implementation boundary

1. Preserve all available transport records, including dropped records, without
   PDG, energy, voxel-count, or detector-visibility selection. Keep product/key
   identity, source track ID, signed PDG, status, start/end process, units, and
   trajectory availability. Do not advertise this as every simulated particle.
2. Preserve raw mother/daughter references, explicit MCTruth associations and
   generated-particle indices, and the separate suppression ancestry map.
   Resolve direct parent, available ancestor, and generator affiliation separately,
   recording the evidence and unresolved/conflicting status. Never manufacture a
   primary by self-parenting an unresolved chain.
3. Derive labeling groups in Atomic with explicit source membership and reason
   codes for merges or exclusions. Keep source inventory accessible even when a
   source particle contributes no label voxels. `mcreco` is a comparison source
   and optional derived input, not the authority defining particle existence.
4. Apply shared validity rules before arithmetic or unit conversion. Translate
   unavailable fields to LArCV conventions only at the writer boundary, with an
   explicit policy for fields that its API cannot safely represent.
5. Compare the legacy projection separately from inventory completeness. Record
   justified differences; do not demand preservation of broken ancestry or
   sentinel overflow for bitwise parity.

Before declaring general correctness, add fixtures for neutrino final states,
neutrons, nuclei and antiparticles, several MCTruth products, overlay offsets,
zero-charge primaries, absent ancestors, map conflicts, duplicate/negative IDs,
cycles, and repeated run/event numbers across subruns. Also test a parent excluded
from labeling while its daughter remains. The present sample cannot validate
neutrino completeness or all detector/upstream configurations.

[particle-list]: https://github.com/LArSoft/larg4/blob/v10_00_09/larg4/pluginActions/ParticleListAction.cc
[mcreco-part]: https://github.com/LArSoft/larsim/blob/v10_23_00/larsim/MCSTReco/MCRecoPart.cxx
[mcreco-track]: https://github.com/LArSoft/larsim/blob/v10_23_00/larsim/MCSTReco/MCTrackRecoAlg.cxx
[mcreco-shower]: https://github.com/LArSoft/larsim/blob/v10_23_00/larsim/MCSTReco/MCShowerRecoAlg.cxx
[mcreco-module]: https://github.com/LArSoft/larsim/blob/v10_23_00/larsim/MCSTReco/MCReco_module.cc
[cluster]: https://github.com/DeepLearnPhysics/Supera/blob/9e7b533d44236c64ddc17748e92dbd9d2fbe0fae/SuperaMCParticleCluster.cxx
[particle-cache]: https://github.com/DeepLearnPhysics/Supera/blob/9e7b533d44236c64ddc17748e92dbd9d2fbe0fae/MCParticleList.cxx
