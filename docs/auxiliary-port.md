# Auxiliary detector port: review packet

Status: checkpoint approved for commit and publication on 2026-11-07. This extends the optical
checkpoint with ICARUS CRT extraction. It does not introduce new detector readers,
truth matching, associations, calibration, or labeling.

## Review path

1. Atomic `src/supera/data/AuxDet.h`: the shared measurement and collection types.
2. `include/larsoft2supera/AuxDet.h` and `src/AuxDet.cxx`: the native conversion.
3. Atomic `src/supera/io/larcv2/AuxDet.cxx`: the compatibility projection.
4. `plugins/SuperaWriter_module.cc` and `fcl/auxdet_icarus.fcl`: art composition.
5. `tests/AuxDet_test.cxx`, Atomic `tests/v2/AuxDetWriter_test.cxx`, and the extended
   `tests/integration.py`: independent expectations and reference comparisons.

The art module is now named `SuperaWriter`, replacing the early `OpticalWriter`
prototype. Both supplied configurations use the new name. The optical-only
configuration remains a separate regression case. An installed consumer links
both extraction targets without linking the output target.

## Why auxiliary detectors

Technology, subsystem role, and measurement kind are separate concepts. A
scintillator can serve cosmic tagging, beam timing, muon tagging, or veto roles.
A hit from a cosmic tagger does not establish that its particle was cosmic.

`AuxDetSpacePoint` describes a localized reconstructed measurement. It does not
represent the physical slab or all possible auxiliary products. ProtoDUNE-SP CRT
triggers/channel ADCs and beam TOF/Cherenkov information need different future
record kinds. SBND reconstruction completeness and timestamp uncertainties also
cannot be silently discarded into the existing LArCV2 CRT schema.

The shared model has no art, SBN, ROOT, or LArCV dependency. `SbnCrtHit` identifies
an explicit source-field contract; it is not an output-format tag or a particle
classification. Future readers can supply other contracts without changing the
meaning of the existing one.

## Current input contract

The native type is `sbn::crt::CRTHit` from sbnobj `v10_27_00`, commit
`55c657ca59214b449e5d951e691f21c2b5278f36`. Fast tests compile the genuine pinned
header; the full test uses the installed ICARUS release. The cached sample's
explicit input tag is `overlayCRTHit::Overlay`.

| Source | Shared representation | Behavior |
| --- | --- | --- |
| Collection order | Collection-local `id` | Starts at zero for each conversion |
| `x/y/z_pos`, `x/y/z_err` | Optional position and uncertainties in cm | Float values promoted exactly to double |
| `peshit` | Signal with `Photoelectron` unit | No recalibration or validity cuts |
| `feb_id` | Source-scoped readout IDs | Preserve order and duplicates |
| `pesmap` | Readout groups and channel signals in PE | Preserve empty groups, channel order and duplicate channels |
| `ts0_s` | Unsigned 64-bit reading in seconds | No conversion through double |
| Other four timestamp fields | Named double readings in source units | No epoch inference, correction, or recombination |
| `plane`, `tagger` | Plane and region annotations | Normalize missing plane; preserve region |
| No completeness field | Absent `complete` | Do not assume a complete 3D reconstruction |

Hardware IDs are not geometry indices. No geometry lookup is performed; physical
AuxDet/AuxDetSensitive references are deferred until a reader can provide them.
The collection records detector and coordinate-frame context. The shipped FHiCL
uses `source` for the frame: no coordinate transformation is implied.

Source absence and nonfinite values follow [the validity contract](validity.md).
Missing records remain optional; missing floating components use NaN. Exact integer
clock absence is explicit. No preservation of arbitrary NaN payload bits is claimed.
Records own their values and remain valid after input destruction.

### Timestamp caution

ICARUS reconstructed and simulated CRT producers assign different meanings to
fields with the same names. For example, one uses a remainder for `ts0_ns`, while
the other stores its full input time. The converter therefore retains all five
readings under the `sbn_crt_source` contract with `SourceDefined` reference. A
consumer must not assume they share an epoch or sum them automatically. See F-007
in the [findings ledger](findings.md).

## Output contract and deliberate safeguards

Only Atomic's optional `io/larcv2` component constructs LArCV objects. It maps the
supported source contract to the existing `crthit` product. The public writer
header and all input/shared types remain free of LArCV headers.

The writer rejects unsupported source contracts, missing required estimates,
ADC/energy presented where PE is required, unrepresentable IDs, lossy double-to-
float projections, duplicate readout groups, and incompatible clock contracts.
It also rejects supplied completeness or timing uncertainty that this output
cannot represent. It sets every LArCV CRT field explicitly, including empty
vectors/maps. Collection-level provenance, technology, roles, and frame metadata
are intentionally not persisted by the legacy format; this limitation is explicit.

The art wrapper rejects empty/duplicate output labels and unknown reader names.
All input products are resolved before the writer receives the event; all
projection validation precedes output-manager mutation. Missing input fails;
an existing empty collection writes an empty product for that event.

## Validation and reproduction

The suite uses the unchanged [reference manifest](reference-baseline.json). It
compares the two optical trees alone, then those same trees plus `crthit`, against
both frozen references. Serialized product bytes, event IDs, and inventories
must match exactly; ROOT file UUIDs and container metadata are excluded.

Independent tests exercise all source fields, ownership, empty collections,
source sentinels, exact integers above 2^53, duplicate channel/board values, and
output rejection paths. The art fixture has a nonempty/empty/nonempty CRT
sequence alongside the optical fixture. Missing products and invalid auxiliary
configuration are tested as failures.

Local commands use the tooling documented in [optical-port.md](optical-port.md),
with `SUPERA_SOURCE` pointing at the pinned Atomic checkout. The integration
script now runs both ports. `SBNOBJ_SOURCE` optionally selects the pinned sbnobj
checkout for fast checks; otherwise the script fetches it.

**Coordinated dependency:** this adapter pins Atomic
`f80ee2c124cda0f70c07038e273440b6450cd9bf` in `tools/checks.sh` and both C++
workflows. Install that revision before building this checkpoint. Full ICARUS
integration uses externally provisioned fixtures; local results below do not
imply that the self-hosted integration workflow has run.

Local validation on ICARUS `v10_27_00`:

- Optical-only: 20 product-events, 586,046 serialized bytes, identical to each
  frozen reference across ten events.
- Combined optical/CRT: 30 product-events, 1,435,684 serialized bytes, identical
  to each frozen reference across ten events.
- Atomic writer/data tests, both extraction tests, installed-consumer build,
  empty-event sequences, and eleven expected configuration/input failures pass.
- Formatting, clang-tidy, and address/undefined-behavior sanitizers pass.
- Converter coverage: 96.0% lines (48/50), 100% functions (2/2), 54.5% branches
  (48/88). The auxiliary converter alone has 96.2% lines (25/26). Uncovered
  allocation-failure cleanup/closing braces are not simulated.

Local artifacts live under the lab's `results/auxdet-validation-01/events/`
and `results/auxdet-fast.log`; they are intentionally not committed.
These results establish this ICARUS field-copy port, not complete auxiliary
support, new-detector integration, or physical correctness of legacy timing.
