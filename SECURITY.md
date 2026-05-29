# Security Policy

## Scope

This project is a **read-only decoder** for a static game data file. It
parses `XFILES.HDB` and produces JSON / SQLite outputs. There is no
network code, no auth, no privilege escalation surface.

That said, the parser **does** read untrusted bytes (whatever HDB-shaped
file you point it at). The relevant attack surfaces are:

1. **Memory safety in the C++ decoder** (`cpp/`). A malformed HDB could
   theoretically trigger an out-of-bounds read in the parser.
2. **Resource exhaustion** in either Python or C++ via crafted page
   counts or recursive sub-objects.
3. **Bugs in the SQLite writer** that could corrupt a target database if
   the same DB is reused across runs.

If you ship this decoder as part of a service that ingests user-uploaded
HDB-like files (e.g. a web preview tool), please consider:
- Running the parser in a sandbox.
- Setting hard size limits on inputs (the original game HDB is ~6 MB).
- Disabling `dump-complete` for very large inputs (it produces a JSON
  ≈4.5× the input size).

## Reporting

Open a GitHub issue with the label `[security]`. If the bug is sensitive
(memory corruption, RCE — unlikely but possible), email the maintainers
directly before opening a public issue.

Coordinated disclosure window: **90 days** from the date a maintainer
acknowledges the report.

## Out of scope

- Anything that requires a copy of the game binary as the attack vector
  (that's a copyright issue, not a security issue).
- Trademark or IP claims (please open a regular issue, not a security one).
