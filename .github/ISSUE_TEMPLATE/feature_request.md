---
name: Feature request
about: Propose an addition (design review happens here, before code)
title: ""
labels: enhancement
assignees: ""
---

## Problem

What real problem does this solve? PicoTTL rejects speculative
abstractions: a feature needs a concrete consumer.

## Proposed design

How would it work? Include the intended public API surface if any.

## API impact

PicoTTL's public API evolves additively only (docs/api_evolution.md).

- [ ] This proposal is purely additive (new names / defaulted fields /
      new enumerators / new backends)
- [ ] It changes existing behaviour or signatures (explain why this is
      unavoidable - expect strong pushback)

## Which layer?

- [ ] Platform-independent (Graphics / Text / Terminal / formats)
- [ ] Backend (MDA / CGA / EGA / host)
- [ ] New backend or platform
- [ ] Examples / demonstrations / diagnostics
- [ ] Tooling / documentation

## Historical authenticity

If this touches how a standard is reproduced (timings, colors, fonts,
connectors): what did the original IBM hardware do, and what is the
source for that?

## Alternatives considered

What else could solve the problem, and why is this design better?

## Hardware validation plan

Which monitor(s) would this be validated on, and with what pattern or
example?
