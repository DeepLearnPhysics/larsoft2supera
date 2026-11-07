# Input validity normalization

Status: implemented for the existing Optical, AuxDet and Generator converters.
The optional normalization report keeps source-sentinel, NaN and infinity counts
and up to 16 record/particle/step/field examples per source product and event.

The shared contract lives in SuperaAtomic's `docs/validity.md`. Inputters own native
sentinel recognition; writers own output encodings. Native values must be checked
before unit conversions or casts. The mappings below are tested with
independent native fixtures and reference comparisons.

## Verified native mappings

| Source field | Native absence | Shared representation |
| --- | --- | --- |
| OpFlash x center | `!hasXCenter()` / `NoCenter` | NaN for x; retain available y/z |
| OpFlash x width | `NoCenter` | NaN for x width independently of center |
| CRT position/errors and total PE | `CRTHit::kInvalidF` (float lowest) | NaN in the affected component/measurement |
| CRT double clock readings | `CRTHit::kInvalidD` (double lowest) | NaN; retain the named clock and units |
| CRT seconds counter | `uint64_t(-1)` | Empty optional uint64; preserve valid integer precision |
| CRT plane | Native default -1 | Missing optional plane code |
| Generator input product | Missing optional handle | Source availability false; not present-empty |
| Generator neutrino record | `!MCTruth::NeutrinoSet()` | Absent neutrino record |
| Generator trajectory | Zero stored points | Absent initial step, empty trajectory |
| Generator track/status/PDG | `INT_MIN` in `MCParticle` | Absent optional ID/status; PDG 0 |
| Neutrino current/mode/type/target/nucleon/quark | `INT_MIN` | Absent optional code |
| Neutrino W/x/y/Q² | `double::min()` (smallest positive normal) | NaN |
| All floating measurements | NaN or either infinity | Canonical NaN, with reason count |
| Generated source/record/particle indices | No native sentinel | Checked nonnegative indices in their declared scope |

Evidence: lardataobj `949008add3a77ef7d532536d8e7b7343cb5e9190`,
`RecoBase/OpFlash.h`; sbnobj `55c657ca59214b449e5d951e691f21c2b5278f36`,
`Common/CRT/CRTHit.hh`. The pinned headers are used by the native tests.
Generator mappings were checked against the installed nusimdata `v1_29_04`
`MCParticle.cxx` and `MCNeutrino.cxx` constructors. In particular, the neutrino
floating sentinel is **min**, not **lowest**.

A native default documents missingness for these fields; it does not establish
all producer semantics or prove all other values are physically meaningful.

## Unresolved mappings and implementation consequences

- OpFlash has members without default member initialization. Reading an
  uninitialized C++ object is not a supported invalidity probe. Construct complete
  native fixtures; investigate producer-written fields separately. Do not infer
  that zero in a properly constructed flash means missing.
- CRT channel IDs, board IDs, and channel contributions need their own contracts.
  Do not generalize plane -1 or float lowest to every member of a hit. Existing
  tests that preserve a negative channel are not evidence of a valid native ID.
- Generator signed IDs, status and neutrino codes now normalize the constructor's
  explicit sentinel only. Negative track IDs, unknown origin 0, and code values
  such as nucleon/quark 0 remain meaningful. Trajectory components have no reserved
  finite sentinel in the exported contract; only nonfinite values normalize.
- Theta requires nonzero finite incoming and outgoing momentum directions. Missing
  or unusable directions yield NaN; selection rejects retained neutrinos without
  a defined angle rather than letting ROOT manufacture an apparently valid angle.
- AuxDet missing positions, uncertainty components, signal, plane and integer
  clocks are now explicitly projected by the writer. A channel ID is preserved as
  a source hardware code; no collection-bound or sign policy is invented for it.
- Legacy generator IDs of 65535 and present-source counters remain output-only
  compatibility findings. They must not determine canonical identity or validity.
- SpacePoint/Hit associations need checked product identity and collection bounds.
  A missing hit plane, no association, and a malformed association are distinct
  cases even when a legacy tensor represents several of them with -1.

Before declaring a product migrated, cover every exported field in its mapping,
including fields with no reserved native sentinel. Record normalization counts and
any expected reference differences. Keep missingness separate from physics cuts,
origin/status selection and labeling policy. No further product port should rely
on the current raw-copy behavior as its validity standard.

## LArCV2 boundary mappings

- Optical missing x center/width use the destination's legacy `double::max()`
  encoding. Other floating fields retain NaN; finite values are unchanged.
- The SBN CRT projection writes missing float measurements as float lowest,
  missing double clocks as double lowest, absent integer seconds as uint64 max,
  and missing plane as -1. Unsupported clock contracts still fail explicitly.
- Generator missing track IDs use unsigned max. Missing interaction codes use -1,
  matching the destination field conventions. Missing PDG is 0; missing floating
  kinematics remain NaN. Existing signed-ID wrapping is confined to this legacy
  output projection. A valid native -1 track ID collides with unsigned-max output;
  the shared optional signed identity preserves the distinction that LArCV2 loses.
- Missing required record identity, infinity supplied directly to a writer, and
  unrepresentable destination conversions fail before appending an event.

## Audited reference deviation

The immutable ICARUS references contain positive infinity in x and z uncertainties
for 12 CRT hits (24 fields). These now normalize to NaN internally and float lowest
in LArCV2. `tests/expected_crt_normalization.json` lists every event/hit/field.
The integration suite retains the raw byte-comparison result, then transforms only
these reference uncertainty fields in memory and requires exact equality for the
entire serialized products. The observed transformation list must match the
allowlist exactly. Neither input nor reference files are rewritten. Comparator
controls verify that unrelated field changes still fail.

The adapter pins Atomic `f80ee2c124cda0f70c07038e273440b6450cd9bf`, including
the shared validity contract and the separately committed large-grid shift fix.

## Validation of this checkpoint

The ICARUS integration passed for all ten cached events against both immutable
references: two optical trees match exactly; the three-product auxiliary and
five-product generator runs match after exactly the 24 enumerated CRT changes.
Generator and optical serialized products have no deviations. Installed-package
consumer checks, native converter/writer tests, empty-input sequences, expected
configuration failures and comparator controls also pass.

The lean converter suite covers 67/69 Optical/AuxDet lines (97.1%) and both
functions. Native Generator coverage is 66/68 lines (97.1%) and all four functions.
Sanitizer checks cover the shared contracts and all three converters. The optional
legacy labeling build and its grid tests also pass. These are local checks; remote CI status
must be checked separately when
publishing the checkpoint.
