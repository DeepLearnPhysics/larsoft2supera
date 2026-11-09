# Deposit extraction and attribution

This slice reads simulation deposits directly, without MCReco. The input layer
normalizes native fields; Atomic owns attribution and optional rasterization;
only the output layer knows LArCV. It does **not** implement semantic particle
grouping, reconstructed-charge matching, or non-ghost labels.

## Three independent views

| Reader | Shared kind | Preserved information | Profile output |
| --- | --- | --- | --- |
| `SEDLite` | `EnergyPoint` (0) | Position, time, energy, raw track ID | `deposit_energy` voxels and archive |
| `SimChannel` | `ReadoutIDE` (1) | Channel inventory, TDC, IDE order, position, energy, electrons, assigned/original IDs | `readout` archive |
| `SED` | `EnergyStep` (2) | Midpoint plus both endpoints/times, energy, electrons, photons, scintillation ratio, PDG, assigned/original IDs | `full_steps` archive |

All positions are cm, simulation times ns, and energies MeV. TDC remains an
unconverted electronics clock count. Missing fields use the shared validity
contract. Raw signed IDs are retained alongside normalized optional identities.
Native `NoParticleId` and `kBogusI` are checked **before** interpreting a negative
assigned ID as reassignment. Original IDs are not blindly made positive.

Full-step segments remain available even when a midpoint projection is requested.
That optional projection assigns the whole energy to the midpoint voxel; it does
not distribute energy along the segment. The supplied profile leaves full steps
unrasterized and projects only the reduced points used by the legacy comparison.

These views must not be added together. IDE energies repeat contributions through
readout channels/planes; the reader neither divides them by three nor interprets
them as independent physical deposits. Full and reduced products can also reflect
different processing histories and need not have equal energy totals.

### What the ICARUS source actually contains

The fixture's original `sedlite` module was `G4InfoReducer` with 0.3 cm bins and
`useOrigTrackID: true`. It groups by bin and track ID, sums energy, and keeps the
earliest time. Thus its positions are **upstream bin centers**, not unmodified
Geant4 step positions. Trigger shifting and filtering follow. The persisted full
`filtersed::DetSim` steps are a separate processed view, with their own endpoints
and assigned IDs. Neither reader claims to undo upstream reduction, space-charge
processing, or filtering.

The source-level check uses
[G4InfoReducer at the original sbncode release](https://github.com/SBNSoftware/sbncode/blob/v10_06_00_06/sbncode/LArG4/G4InfoReducer_module.cc)
and the input file's persisted parameter sets. This provenance is specific to the
fixture; other production configurations must be reviewed independently.

## Attribution contract

The collection explicitly names its transport namespace. Track IDs are sparse
keys, never vector offsets. Every contribution survives, including unresolved or
conflicting contributions. The archive keeps the exact-creator index, assigned
index, chosen attribution, and primary affiliation as separate fields.

Attribution states, in serialized numeric order, are:

1. `Exact` (0): original creator is in the transport inventory.
2. `Reassigned` (1): upstream assigned identity is available; creator is absent.
3. `Ancestor` (2): only a primary from the suppression map is available.
4. `Unresolved` (3): no supported affiliation is available.
5. `Conflict` (4): evidence disagrees or implicates conflicted/cyclic ancestry.
6. `EvidenceAncestor` (5): another requested full-step/readout product explicitly
   pairs the missing original ID with a negative assigned ancestor ID.

All requested sources are loaded before cross-product evidence is built, so
configuration order cannot change the result. Evidence is transport-scoped.
Repeated pairs collapse; conflicting primary affiliations remain conflicts. If
multiple candidate ancestors share one primary, use that primary rather than
inventing a nearest parent. Raw source products in the archive retain the evidence
needed to reconstruct these decisions. A recovered affiliation never restores an
absent particle's trajectory or makes an ancestor the exact creator.

Invalid energies/positions and out-of-grid contributions have explicit counts and
energy accounting. Their raw records remain in the companion archive. The voxel
projection sums double precision energy by `(particle index, voxel index)`;
LArCV float conversion occurs only at the writer boundary.

## Outputs and use in an audit

Run `fcl/deposit_icarus.fcl` on the cached stage-one fixture. It produces:

- `deposits.root`: generator and complete transport particle lists, plus
  `cluster3d_deposit_energy` and `sparse3d_deposit_energy`.
- `deposits.root.transport.root`: complete transport fields and relationships.
- `deposits.root.deposits.root`: `deposits` contribution rows and `events`
  collection inventory/accounting, including present-but-empty products.
- Timing CSV files separating source access/conversion, evidence construction,
  shared attribution/rasterization, and combined output writing.

Cluster index equals transport-particle index. There is one additional final
cluster for unassigned contributions, even when empty. This is an **inventory
alignment**, not the legacy shower/track grouping or a final SPINE training-label
contract. Ordinary LArCV parsers can inspect the tensors and particle records;
the companion archives are needed for lossless ancestry and source audits.
Do not mistake the transport particle count for a reconstructed instance count.

## Validation and intentional differences

`tests/deposit_integration.py` runs the profile and an independent native checker
on all ten frozen events, then compares both standard and staged legacy outputs.
The checker reads the actual LArSoft products without calling the converter or
shared attribution. It checks every contribution's native fields, event/order,
raw identity, exact/assigned particle indices, and channel inventory.

The legacy `sed` tensor comparison is deliberately stronger than a loose numeric
tolerance: an independent reconstruction must reproduce every new voxel value,
while applying the measured legacy active-volume bounding box must reproduce
exactly the legacy voxel inventory. Remaining value differences must fit an
IEEE float accumulation error bound.

The legacy path excludes 32 reduced points (1.7058666665 MeV), accounting for all
15 additional voxels in the new output. These are bin centers outside its active
bounding box, still inside the requested output grid. The new policy retains
these persisted contributions. This does not establish that the original
physical steps were outside the active material. All legacy voxels remain.
Repeated float accumulation explains the remaining differences; the measured
maximum deviation from the independently rounded source sum is about
3.1e-7 MeV. This is explained equivalence with explicit policy changes, not a
claim of bit-identical output.

Tests also cover missing identities, signed reassignment, conflicting evidence,
namespace isolation, invalid measurements, voxel exclusions, empty products,
archive precision, duplicate projection keys, and rejected physical projection
of channel IDEs. The genuine LArSoft converter is checked with coverage and
ASan/UBSan; shared attribution has separate sanitizer and static-analysis checks.

The fixture contains cosmic-muon overlays, not neutrino interactions. Neutrino
ancestry, other detector configurations, semantic grouping, and true non-ghost
labels remain separate acceptance gates. No upstream production change is made.

### Frozen-fixture result

The successful ten-event run is in `results/deposit-integration-01` under the lab.
Both reference comparisons and the native source check passed.

| View | Contributions | Exact creator | Supported ancestor | Unresolved/conflicting |
| --- | ---: | ---: | ---: | ---: |
| Reduced points | 175,443 | 163,262 | 12,181 via explicit cross-product evidence | 0 |
| Full steps | 220,719 | 207,171 | 13,548 via assigned ID | 0 |
| Channel IDEs | 352,351 | 274,645 | 77,706 via assigned ID | 0 |

All contributions have valid energy/position; no reduced points leave the output
grid. Reduced-point energy totals 12,477.4963132 MeV; full-step energy totals
12,485.0843052 MeV. Their different source histories remain explicit rather than
being reconciled by an arbitrary rescaling.

Median event timings in this run:

| View | Source access and conversion | Attribution and optional rasterization |
| --- | ---: | ---: |
| Reduced points | 5.34 ms | 3.98 ms |
| Full steps | 16.42 ms | 2.37 ms |
| Channel IDEs | 12.27 ms | 3.09 ms |

Cross-product evidence construction takes another 0.61 ms/event. Writing all
configured products and the two audit archives takes 236 ms/event. These timings
are single-run observations on the local container, not cold-I/O benchmarks or
an end-to-end speedup relative to legacy Supera. CSV files preserve each event.
