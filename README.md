# larsoft2supera

LArSoft input adapter for [SuperaAtomic][atomic], enabling shared LArTPC
voxelization and truth labeling across experiments.

**Status: optical, ICARUS auxiliary-detector, and generator checkpoints validated.** The shared data
model and LArCV2 writer are coordinated changes in SuperaAtomic.
The first target is ICARUS overlay data. Support for other experiments will be
added through explicit input contracts and integration tests.

## Purpose

`larsoft2supera` will translate LArSoft event products, detector context, and
associations into inputs suitable for a shared Supera labeling core. Its purpose
is to make that boundary explicit, testable, and understandable.

The long-term separation is:

```text
LArSoft event products and services
    -> larsoft2supera: native extraction and translation
    -> SuperaAtomic: shared records and optional labeling
    -> SuperaAtomic: selected output adapter (LArCV2 for this port)
```

The adapter owns LArSoft-specific access and translation. Shared particle
grouping, semantic priorities, and detector-independent algorithms belong in the
core. **The migration will keep LArCV2 output throughout.** We will first reproduce
the agreed LArCV2 products through a separate optional output component in Atomic.
The data/output dependency boundary is established now; a new output format is
deferred. The exact interface is still under review: SuperaAtomic's current
input model does not yet describe all the
reconstructed, optical, and CRT information needed by the legacy workflow.

## How we will build it

We will migrate one reviewable behavior at a time. Each step will document the
legacy behavior, establish its input/output contract, implement the smallest
useful change, and compare it with independent expectations and reference data.

- Preserve source identities, units, coordinate frames, and time conventions.
- Distinguish missing products, empty products, invalid associations, and
  detector activity without simulation truth.
- Keep event state local and make ownership and lifetimes explicit.
- Separate compatibility changes from deliberate corrections to labeling policy.
- Record suspected problems, reproductions, decisions, and fixes in the
  [findings ledger](docs/findings.md).
- Require behavioral tests, formatting, linting, and documentation alongside code.

The old implementation is a compatibility reference, not a correctness oracle.
Agreement with it is necessary for a faithful port; independent tests are needed
to decide whether the behavior itself is right.

## Starting point

The existing ICARUS workflow has a validated reconstruction cache. On the pinned
10-event overlay sample, the standard full chain and cached-reconstruction plus
legacy Supera both succeeded. All 43 LArCV product trees matched exactly across
the 10 events: 430 product-event comparisons. This validates the cache boundary;
it does not validate a new adapter or establish parity with SuperaAtomic.

Read the [current processor inventory](docs/processors.md),
[migration stages](docs/migration.md), and
[reference manifest](docs/reference-baseline.json).
Large event files and the local experiment environment remain outside this repo.

The first implementation target is event identity and optical-flash extraction,
with a small, framework-independent conversion function and a thin art wrapper.
The [optical input audit](docs/optical-audit.md) also tracks information outside
the flash fields, including associations and possible truth inputs. Field-copy
parity alone will not establish complete optical support.
Each functionality addition is a cohesive commit with its tests and migration
record. Broader behavior changes remain separate review decisions.

The [generator checkpoint (`mpv`)](docs/mctruth-port.md) includes
its field audit and explicit legacy compatibility decisions.

## Development

Read [CONTRIBUTING.md](CONTRIBUTING.md) for the review and testing requirements.
See the [auxiliary port review packet](docs/auxiliary-port.md) for the current change
and the [optical port review packet](docs/optical-port.md) for build instructions,
contracts, safeguards, and validation limits. Local C++ and integration checks
exist. Hosted C++ checks use a pinned Atomic revision; full ICARUS integration
requires externally provisioned fixtures and a runner.

With Node.js 22 or newer and npm installed:

```sh
npm ci
npm run lint
```

Node is only documentation tooling, not an adapter runtime dependency. See
[the development guide](CONTRIBUTING.md#documentation-checks) for the pinned
container command used locally and in CI.

Once this boundary is proven, we will use its contracts and tests to review
`edep2supera`, `larnd2supera`, and `flow2supera`. Their current behavior and
compatibility have not been assessed here.

[atomic]: https://github.com/DeepLearnPhysics/SuperaAtomic
