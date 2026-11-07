# Optical input audit

Status: discovery record. The [scoped optical port](optical-port.md) implements
existing fields only; association and truth extensions remain deferred.
The legacy output is a compatibility reference; matching it cannot demonstrate
that the input boundary preserves all useful information.

## Evidence scope

The [reference manifest](reference-baseline.json) pins the ten-event ICARUS overlay
sample, reconstruction cache, software, and legacy outputs. This audit combines
source inspection with a read-only gallery traversal of that cache on 2026-11-07.
It does not run reconstruction again or alter a reference file.

The local lab retains the audit implementation in `tests/audit_optical.hxx` and
`scripts/audit_optical.py`, with per-event results in
`results/optical-association-audit-v2.csv`. These are research tools, not adapter
code or a substitute for the planned integration suite.

## Stored attributes and their meaning

For the pinned [OpFlash definition][opflash], the legacy [SuperaOptical][legacy]
copies every stored attribute into LArCV2 Flash. No missing scalar/vector attribute
was identified in this version. That observation does not cover related products.

| Input attributes | Legacy treatment | Review requirement |
| --- | --- | --- |
| `Time`, `TimeWidth` | Copied unchanged | Trigger-relative time and width in microseconds; check the producer's estimator. |
| `AbsTime`, `Frame` | Copied unchanged | Establish actual producer clock conventions; do not infer wall-clock time from the name. |
| `PEs` | Entire vector copied in order | Establish whether indices represent channels or optical detectors, and record that convention. |
| `InBeamFrame`, `OnBeamTime`, `FastToTotal` | Copied unchanged | A stored value may be a producer default rather than a measurement. |
| `XCenter`, `XWidth`, `YCenter`, `YWidth`, `ZCenter`, `ZWidth` | Copied unchanged | Preserve missing-coordinate sentinels; distinguish unavailable coordinates from zero. |
| `WireCenters`, `WireWidths` | Entire vectors copied | Preserve empty vectors and document view/index conventions. |
| Collection index | Added as LArCV flash ID | This is local to the collection; retain source product identity for associations. |

`TotalPE()` and `hasXCenter()` are derived queries, not additional stored fields.
An input object's default constructor does not initialize every scalar; synthetic
tests must construct complete inputs rather than read indeterminate values.

The sample's embedded FHiCL identifies `ICARUSFlashFinder` with `OpHitProducer:
"ophit"` for both cryostats. The inspected release [producer][producer] uses the
constructor without an x estimate and supplies constants `0, 0, 1` for the two
beam flags and fast-to-total ratio. This is evidence that copying all fields is
insufficient to establish their physical meaning. The sample was produced by an
older release; historical producer behavior and actual field values still need
confirmation before assigning these semantics to every stored flash.

## Associations are part of the input

Both flash-to-OpHit association branches are present. Across all ten events, the
audit read 254 flashes and dereferenced all 82,315 linked OpHits successfully.
Legacy SuperaOptical reads only the flash collections and discards these links.
The counts are association entries, not necessarily unique hits.

The file also contains flash-to-CRT matching associations from `crtpmt`. Their
branch presence is established; their contents have not yet been traversed here.
These are reconstructed matches, not simulation truth labels.

The input contract should account for these relationships explicitly. If an
initial output cannot represent them, document that limitation rather than
silently declaring the optical input fully supported. Preserve source product
identity and element keys; a bare flash index is insufficient across cryostats.

## Truth exists, but the matching contract is unresolved

The installed `larsim/MCCheater/PhotonBackTracker.h` exposes flash-to-hit,
flash-to-track-ID, and hit-to-track-contribution APIs. Its usual truth input is
`sim::OpDetBacktrackerRecord`; the sample's Events branch inventory contains no
such product and no direct persisted optical-hit/flash-to-particle association.
That does not mean the sample has no optical truth.

The `shifted::DetSim` SimPhotons collection contains 639,997 photons. In the pinned
[OnePhoton definition][photons], each photon includes arrival time, positions,
energy, and a mother-track ID. The runtime audit found:

- No photon mother IDs equal to the type's unset sentinel.
- 169,190 photon mother IDs absent from `simplemerge::G4` by exact signed lookup.
- All 169,190 found by looking up the opposite sign in that particle collection.
- None of those exact signed IDs found in `largeant:droppedMCParticles:G4`.
- Event `13171:1:385` has populated flash collections but zero simulated photons.

These are photon counts, not unique tracks or reconstructed-hit contributions.
Opposite-sign lookup is a diagnostic, not an approved ID-normalization rule.
Investigate how the simulation, photon shifting, and merging stages encode IDs
before implementing a resolver. Likewise, a non-sentinel ID alone does not prove
correct ancestry or that a photon contributed to a reconstructed hit.

Before implementing optical truth extraction, establish:

1. How simulation photon times map to reconstructed hit times, including overlay
   shifts, clock offsets, units, hit windows, and channel/detector mapping.
2. What the photon collection represents relative to detection efficiency,
   waveform simulation, reconstructed charge, and calibrated photoelectrons.
3. How signed track IDs, ancestry, and interaction identities are resolved without
   accidentally merging distinct identities or dropping unresolved contributions.
4. Whether to consume existing backtracker records when available and how to
   support this sample's full-photon representation without inventing equivalent
   quantities that it does not contain.
5. Which weights and denominators are meaningful. A fraction of simulated photons
   is not automatically the simulation fraction of measured optical charge.
6. How missing truth, empty truth, failed matching, and real overlay activity are
   represented separately. Do not assign real data a fabricated MC identity.

## Consequences for the first port

Keep the existing LArCV2 flash fields as a compatibility projection, with exact
comparison against both frozen reference collections. Separately test complete
attribute preservation, association identity, availability states, and reviewed
truth semantics using independent expectations.

The first field-copy milestone must not be reported as complete optical support.
Association extraction and truth support need explicit contracts and tests; any
deliberate deferral must remain visible in the capability documentation. New
information may require an additional LArCV2 product or an extension of its data
model. Decide that with the reviewer rather than overloading existing fields or
silently omitting the information to satisfy the old schema.

This audit does not itself approve new conversion or truth policies. Deferred
capabilities remain separate review decisions from the optical-field checkpoint.

[opflash]: https://github.com/LArSoft/lardataobj/blob/949008add3a77ef7d532536d8e7b7343cb5e9190/lardataobj/RecoBase/OpFlash.h
[legacy]: https://github.com/DeepLearnPhysics/Supera/blob/9e7b533d44236c64ddc17748e92dbd9d2fbe0fae/SuperaOptical.cxx
[producer]: https://github.com/SBNSoftware/icaruscode/blob/ee06f3022034c77fd1e9389e09277ea4709524e2/icaruscode/PMT/OpReco/FlashFinder/ICARUSFlashFinder_module.cc
[photons]: https://github.com/LArSoft/lardataobj/blob/949008add3a77ef7d532536d8e7b7343cb5e9190/lardataobj/Simulation/SimPhotons.h
