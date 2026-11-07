# Readability conventions

Code should read as a sequence of small, coherent steps. Apply this convention
in both SuperaAtomic and larsoft2supera, including tests and build scripts.

## Logical paragraphs

- Keep closely related statements together. Separate the next logical step with
  one blank line; do not insert a blank line after every statement.
- Introduce a nontrivial step with a short standalone comment immediately above
  its code. Leave a blank line after the step, before the next comment or block.
- Describe the purpose or constraint: what this step accomplishes, preserves,
  or rejects. Avoid narrating obvious assignments or restating a function name.
- Make the progression visible: validate inputs, prepare local values, perform
  the conversion, then publish or return the result. Preserve ordering whenever
  it affects behavior; readability edits are not permission to reorder effects.
- Separate function definitions. Within records, group fields by meaning while
  preserving declaration order and layout during a readability-only change.
- In tests, distinguish setup, action, and assertions. Separate independent
  scenarios and give each nontrivial scenario a brief purpose comment.

For example:

```cpp
// Reject incompatible units before constructing an output record.
if (signal.unit != SignalUnit::Photoelectron)
  throw std::invalid_argument("Expected photoelectrons");

// Preserve the measurement without recalibration.
OutputRecord output;
output.signal = signal.value;
output.time = time;

return output;
```

Short functions with one obvious step need no artificial sections. Avoid banner
comments, numbered steps that drift during edits, and comments that promise more
than the code guarantees. A compact expression is useful only if its meaning is
still easy to follow. Name intermediate values or extract a focused helper when
that improves understanding; do not introduce helpers just to shorten a function.

## Formatting and review

The repositories use clang-format 14 with the same baseline settings: two-space
indentation, a 100-column limit, separated definitions, and at most one retained
blank line. The formatter preserves manually chosen logical paragraphs; it cannot
identify semantic boundaries or write useful comments. Those remain review tasks.

For each changed function, ask: can a reader identify its successive steps without
mentally executing every statement? Does each comment explain intent accurately?
Are related operations together, with a clear break before the next concern?

Apply this convention to new and touched code. Keep broad legacy formatting
separate from functional changes so reviews remain focused. A readability-only
pass must preserve executable code; verify that separately from formatting.
