# Contributing

## Small, reviewable changes

Begin with the [migration stages](docs/migration.md). Describe the concrete
behavior and its input/output contract before implementing it. Keep extraction,
shared algorithms, and serialization separate. Avoid speculative abstraction
layers; extract a common interface when its users and tests make it useful.

Each implementation change must explain what was preserved or changed, why,
how it was tested, and which questions remain. Link any relevant finding.
Record reviewer acceptance before treating a migration stage as approved.

## Readable code and documentation

Follow the [shared readability conventions](docs/readability.md): short purpose
comments above logical steps, related statements together, and one blank line
between steps. This applies to implementation and tests alike.

- Use explicit names for physical quantities, units, coordinate frames, and IDs.
- Prefer small functions with one coherent responsibility and explicit inputs.
- Use value semantics and RAII; document ownership and event-scoped lifetimes.
- Keep domain conversion functions independent of framework services where possible.
- Validate configuration and distinguish missing, empty, and invalid inputs.
- Document public contracts, invariants, assumptions, and non-obvious choices.
  Comments should explain intent rather than restate individual statements.
- Follow `.editorconfig`: two-space indentation, LF endings, final newlines, and
  no trailing whitespace. Use blank lines between logical steps.
- The first C++ change must add a reviewed `.clang-format`, pinned formatting and
  static-analysis tools, and enforced checks. Do not rely on manual style review.

## Testing and CI

The first optical component has local C++ and art integration checks. See the
[review packet](docs/optical-port.md) for execution and hosted CI limitations.

The first executable component must add a supported, reproducible build and:

1. Unit tests for every public conversion function and meaningful decision branch,
   including normal, boundary, invalid, missing, and empty cases as applicable.
2. Multi-event tests that detect stale state and lifetime mistakes.
3. Integration tests for framework product lookup, services, and associations;
   thin wrappers can be tested here rather than with mocks that reproduce art.
4. Coverage reporting, with uncovered behavior reviewed and explained. Coverage
   is evidence about exercised code, not a substitute for correct assertions.
5. Formatting and static analysis, plus appropriate compiler warnings and
   sanitizers for the framework-independent code where supported.

Fast synthetic checks should run on pull requests. A pinned LArSoft integration
job must validate adapter changes before a stage is accepted. The large overlay
sample belongs in an explicitly provisioned integration/regression workflow;
document fixture availability and failures rather than silently skipping checks.
Do not advertise experiment support based only on a standalone unit-test pass.

Test expected values should come from contracts and worked examples as well as
the frozen reference. Avoid tests that merely repeat the implementation.
Every confirmed bug fix needs a focused regression test. Keep existing reference
files immutable; intentional behavior changes need separate expected results and
an approved explanation.

## Documentation checks

Run the same pinned linter as CI:

```sh
npm ci
npm run lint
```

Alternatively, from this repository on a host with Docker:

```sh
docker run --rm -v "$PWD:/work" -w /work \
  node@sha256:5ec23b7c622b00547753bc6ffd78085dd868a68aeb41c8e0a1418f033e13d96d \
  sh -c 'npm ci && npm run lint'
```

Commit `package-lock.json` when changing tooling. The CI action and container are
pinned; review dependency updates explicitly. Node is only used for documentation
linting. GitHub execution of the workflow must be checked after publication;
running its commands locally does not prove the hosted workflow has passed.

The initial lockfile overrides `re2js` to 2.8.7 and `katex` to 0.18.2 to address
[GHSA-gg6h-894w-4m9c][re2-advisory] and [GHSA-238p-pmpm-9mq7][katex-advisory]
in the linter's transitive dependencies. These explicit overrides are tooling
maintenance decisions; re-evaluate them when upgrading markdownlint and rerun
lint plus `npm audit` rather than applying a blind forced update.

## Data, provenance, and licensing

Keep large event files, generated outputs, local paths, and build directories out
of Git. Record portable artifact names, versions, source revisions, and hashes.
Small distributable fixtures need a documented origin and expected behavior.

The repository license is not selected yet. Resolve licensing and preserve
attribution before importing implementation code from another project.

[re2-advisory]: https://github.com/advisories/GHSA-gg6h-894w-4m9c
[katex-advisory]: https://github.com/advisories/GHSA-238p-pmpm-9mq7
