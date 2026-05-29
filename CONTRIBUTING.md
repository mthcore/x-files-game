# Contributing

Thanks for your interest in helping decode the HDB format! A few short rules
keep this project safe, legal, and pleasant to collaborate on.

## Rule #1 — Never attach the game binary

`XFILES.HDB`, the game executable, and any other file from a retail copy of
*The X-Files: The Game* are **copyrighted assets**. Do not paste
them, attach them to issues, push them to a fork, or upload them anywhere.
Tests that need real-game data take a path argument and skip cleanly when
the file is absent.

If you must share a byte range for a discussion, post:
- a hex offset range (e.g. `bytes 0x40..0x80 of page 91`)
- a SHA-256 of the affected bytes
- never the bytes themselves if more than a few

## Rule #2 — DCO sign-off

Every commit must be signed off (`-s`) under the Developer Certificate of
Origin (https://developercertificate.org). This means appending:

```
Signed-off-by: Your Name <you@example.org>
```

to each commit message. `git commit -s` does this automatically.

## Style — Python

- Format with `ruff format` (Black-compatible). Lint with `ruff check`.
- Type-check with `mypy --strict` where new code is added.
- Tests use `pytest`. Tests that need real-game files must `pytest.skip`
  cleanly when the file path doesn't exist.
- No external runtime deps beyond `click`. Anything else needs justification.

## Style — C++

- C++17, no C++20 features yet (some users still target older toolchains).
- Format with `clang-format` using the repository `.clang-format` (LLVM
  style + 4-space indent + braces on next line for functions).
- Tests use Catch2 single-header (vendored under `cpp/third_party/`).
- No external runtime deps. Standard library only.

## Branching and PRs

- Branch from `main`. Branch names: `topic/<short-descr>` or
  `fix/<short-descr>` or `docs/<short-descr>`.
- One topic per PR. Small PRs review faster than big ones.
- Reference any issue in the PR description (`Fixes #N`, `Refs #N`).
- CI must be green before merge.

## What kinds of PRs are welcome?

- **Bug fixes** for the byte-direct decoder (counter-examples on real HDB
  bytes are extremely valuable).
- **New extractors** for game data we don't expose yet (e.g. additional
  cross-references, alternate output formats).
- **Documentation** improvements — clarifications, diagrams, examples.
- **C++ ports** of Python decoders, or vice versa.
- **B-tree internal sematic decoding** (G6 in our internal notes): if you
  can locate `CNeoNode.cp` / `CNeoIndex.cp` from a public NeoLogic source
  release, that would close the last remaining "structural-only" zone.
- **Other HyperBole / NeoPersist games** support (Quantum Gate, Doom Mac
  data files, etc.) — the format applies beyond X-Files.

## What kinds of PRs are NOT welcome?

- Anything that ships the game binary or sub-fragments of it.
- Anything that introduces a heavyweight runtime dependency (e.g. ORM,
  framework) for a problem solvable with stdlib.
- Premature abstractions or speculative features without a concrete user.

## Issue templates

See `.github/ISSUE_TEMPLATE/` — `bug.md` for bugs, `format-finding.md` for
new format findings, `feature.md` for feature requests.
