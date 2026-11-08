# Performance baseline and measurement contract

Performance is an acceptance criterion for each product port, alongside correctness.
The following records what was actually measured and defines the breakdown required
for subsequent benchmarks. Per-product parser profiling is now available in the native space-point harness.

## Retained legacy evidence

Both original runs retained `job.log`, `TimingReport.db`, `MemoryReport.db`, resolved
configuration and product versions. The timing databases contain per-event source,
module and full-event records. [The extracted baseline](performance-baseline.json)
preserves each Supera driver measurement and hashes the timing evidence. The
extraction script is in the lab at `research/performance/summarize.py`; it opens
SQLite read-only and verifies event identities against the reference manifest.

| Ten-event run | Job CPU seconds | Job Real seconds | Supera mean seconds/event | Supera median seconds/event | Supera maximum seconds/event | Peak RSS, decimal MB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Standard stage1 | 5485.80 | 7254.09 | 513.21 | 289.07 | 2257.97 | 4607.59 |
| Cached cluster3D input | 5163.17 | 6984.16 | 521.41 | 290.94 | 2313.00 | 3393.11 |

Job CPU/Real values and TimeTracker module timings are distinct measurements; do
not treat module-time sums as full-process elapsed time. In particular, a tiny
RootInput source time does not prove input I/O is cheap: lazy materialization can
occur inside consumer modules. The large maximum also makes an average alone an
inadequate description of this sample.

These runs used the pinned ICARUS `v10_27_00`, legacy Supera commit
`9e7b533d44236c64ddc17748e92dbd9d2fbe0fae`, and linux/amd64 emulation on the ARM Mac.
They are historical local baselines, not production throughput measurements.
Neither was a repeated controlled performance experiment. The standard/staged
wall-time difference must not be attributed solely to cached reconstruction.

The retained legacy timings cover the complete driver. They do **not** isolate
Optical, CRT, generator or SpacePoint parsing from labeling and LArCV output.
The new three-product adapter currently implements only part of that driver, so
its much shorter job time is not an end-to-end migration speedup. A legacy parser
breakdown would require a later instrumented run; no expensive reference rerun is
needed merely to preserve the existing evidence.

## Required breakdown for each new port

Record one row per event, source product, phase and repetition, with wall duration,
input/output object counts, relevant byte counts and build/input provenance.
Keep timing overhead at collection or phase granularity, not inside every hit loop.
Use a monotonic clock. Preserve raw measurements before computing summaries.

| Phase | Scope |
| --- | --- |
| Input access | Resolve/materialize the native product, including first-touch I/O where measurable |
| Association access | Resolve association tables and materialize target hit products; include lazy dereferences |
| Conversion | Owning records, sentinel normalization and bounded diagnostics, without output serialization |
| Selection/labeling | Shared selection, voxel assignment, collision ranking and subsequent labeling separately |
| Output projection | Conversion of shared records into the selected output format |
| Write/finalize | Event serialization plus buffered writes and close-time flushing |
| End-to-end | Whole event/process time and peak resident memory, including otherwise unassigned overhead |

Report diagnostic logging separately from collecting the bounded diagnostics.
Do not silently benchmark with diagnostics disabled and compare that result to a
production configuration that logs them. Resolve each event/product's identities
once; do not introduce logging, allocations or clock calls per point just to time it.

For parsing, report total milliseconds, microseconds/object and objects/second.
For associations, also report links/second and unique referenced hits. For grid
processing report points, occupied voxels and collision counts. Record both total
process RSS and, where available, an allocation profile; these are not the same
quantity as memory owned by converted records.

## Repeatability and interpretation

Use the frozen cached input with identical event ordering and checksums. Record
repository SHAs and dirty state, release, compiler/optimization flags, architecture,
container image, threading, diagnostic settings and output configuration.
Benchmark optimized builds without sanitizers or coverage instrumentation. Avoid
concurrent compilation or other validation jobs during timing runs.

Separate first-event/startup behavior from steady-state measurements. Retain the
first run and at least three subsequent measured runs; identify warm-cache runs
honestly rather than calling them cold. Report per-event distributions, median,
maximum and raw repetition ranges. Ten events are insufficient for robust tail
percentiles or a broad detector-throughput claim. Use native production-like
hardware for final throughput acceptance; keep emulated Mac results separate.

Compare identical functionality on identical inputs. Do not estimate isolated
parser cost by subtracting two large total job times. Keep correctness checks
outside timed sections, but verify their results for every benchmark configuration.
Shared CI runners can check the harness and scaling fixtures; they should not
impose a noisy absolute seconds threshold before a stable baseline exists.

## SpacePoint/Hit design constraints

The audited sample contains 8,839,871 points and 20,490,298 association links across
ten events, but only 383,132 unique referenced hit identities when summed per
collection/event. This makes allocation behavior and repeated work important.

- Own each hit collection once; points retain exact product/key references.
- Prefer contiguous point and association storage. Evaluate a flat link array with
  offsets before adopting a separate heap-allocated vector for every point.
- Resolve product identities once per product; avoid strings, repeated product
  lookups, and complete hit copies for every association.
- Reserve capacity from known collection/association sizes where available.
- Target extraction work proportional to points + unique hits + links. Measure
  voxel collision handling separately; do not hide sorting or labeling in parsing.

Before accepting the SpacePoint reader, benchmark Optical, AuxDet and Generator
conversion as well, using the same harness. Establish measured per-product costs
and memory growth before setting numerical budgets. Investigate regressions using
profiles and allocation evidence; never trade away association checks or missing-
value semantics solely to obtain a better headline time.

## Space-point extraction measurements

[Machine-readable results](spacepoint-performance.json) retain hashes of the raw
CSV/log evidence in `results/spacepoint-parser-03`. These are three independent
processes per mode, each reading the same ten cached events, with `-O3 -DNDEBUG`
and no concurrent build. Measurements below sum both cryostats per event; medians
are over 30 measurements. OS caches were not flushed. The existing emulated
ICARUS environment is unchanged.

| Read mode | Median conversion, seconds/event | Median including product access, seconds/event | Whole-process peak RSS, decimal MB |
| --- | ---: | ---: | ---: |
| Points only | 0.0122 | 0.1636 | 952.9–954.3 |
| Points and associated hits | 0.0339 | 1.2636 | 1882.4–1883.7 |

The associated conversion time includes point conversion, compact hit ownership,
and flat association construction. Median association product access alone is
1.0524 seconds/event: framework deserialization/materialization dominates this
read. The reader does not run reconstruction, voxel selection, labeling, or
LArCV output. Its memory remains substantial because native products and the
owned representation coexist; these measurements are not a production memory
budget. Points-only and associated modes run in separate processes.

The same harness also times the existing parsers. Excluding each process's first
event, median collection conversion times are 7.9 microseconds (Optical E),
9.5 microseconds (Optical W), 145.9 microseconds (AuxDet/CRT), and 2.8 microseconds
(generator). Those exclude product access and reflect this sample's product
sizes, not universal throughput. Raw CSV retains counts and access times.

Full validation independently checked 8,839,871 points, 20,490,298 links, and
383,132 distinct referenced hits summed over the twenty point-collection reads.
Eight negative fixtures and empty/shared/duplicate/multi-product fixtures passed.
The direct association reader preserves stored order; see finding F021 for its
intentional distinction from `FindManyP` iteration.

## Complete fixed-grid space-point slice

The [output review](spacepoint-output.md) and [recorded baseline](spacepoint-output-baseline.json)
now include shared selection and LArCV2 projection/writing. Median combined time
was 4.17 s/event in one optimized run, with 2553.3 MB peak process RSS. This adds
work and retains both raw collections while writing; it is not directly comparable
to the earlier extraction-only memory and throughput figures.
