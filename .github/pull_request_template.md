# Pull request

## What

One-paragraph summary of the change and its motivation. Link the
design issue if there was one (required for non-trivial changes - see
CONTRIBUTING.md, "Architecture-first workflow").

## API impact

- [ ] No public API change
- [ ] Additive public API change (new names / defaulted fields / new
      enumerators / new backend) - documented in the headers
- [ ] Breaking change (this violates the freeze policy; explain why it
      is even being proposed)

## Checks

- [ ] Host test suite passes (`./build-host/picottl_tests`, 0 failed)
- [ ] Pico build compiles cleanly (`cmake --build build`)
- [ ] **All examples compile unmodified** (the executable form of the
      API freeze)
- [ ] Documentation updated in the same PR (headers and, if
      architectural, docs/architecture.md)
- [ ] Generated files regenerated via `tools/`, not hand-edited

## Hardware validation (if the signal path is touched)

- Monitor make/model:
- Wiring (reference connector unless justified):
- System clock:
- What was verified, and result: (photos welcome)
- [ ] Not applicable - platform-independent change only (a maintainer
      will hardware-test before merge)

## Notes for the reviewer

Anything unusual: trade-offs taken, alternatives rejected, follow-ups
deferred.
