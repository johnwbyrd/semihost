# Security Policy

ZBC is a pre-release project maintained by one person. This document
explains how to report a suspected vulnerability and what is in scope.

## Reporting

Use **GitHub's Private Vulnerability Reporting** form on the
[Security tab](https://github.com/johnwbyrd/zbc/security) so the
report stays private until it is resolved. If that is not an option,
email **johnwbyrd at gmail dot com** with `[ZBC SECURITY]` in the subject.

Please include enough detail to reproduce: target platform, build
configuration, the input that triggers the issue, and (if known) the
source file involved. Proof-of-concept code is welcome.

## What to expect

Acknowledgment within a few days; best-effort coordinated disclosure,
no fixed window. Fixes land on `main`. There are no tagged releases
yet, so no separate patch stream to backport to.

## In scope

The reference libraries -- `src/shared/`, `src/c/`, `src/cpp/` and the
matching `include/` paths -- with particular attention to:

- The RIFF parser and opcode dispatch (untrusted input from the guest)
- The ANSI host backend's filesystem sandbox
- The Linux seccomp policy used by the host
- The C client library running in resource-constrained guests

## Out of scope

- `test/` -- intentionally exercises insecure patterns
  (e.g. `test_ansi_insecure.c`); CodeQL findings here are expected
- `fuzz/` -- bugs surfaced by the fuzzer are valuable, but file them
  as regular issues unless the bug is itself a vulnerability in the
  in-scope code above
- Documentation, build files, and the
  [wiki](https://www.zeroboardcomputer.com/) (separate infrastructure)
- Findings that require attacker-controlled build inputs, host root,
  or modifications to the source tree
