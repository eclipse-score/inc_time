# Time Module — Roadmap to S-CORE v1.0

> **Reference**: [S-CORE v1.0 Roadmap](https://github.com/eclipse-score/reference_integration/tree/main/docs/s_core_v_1)
> **Module board**: [TIM - Time FT #57](https://github.com/orgs/eclipse-score/projects/57)
> **PI tickets**: [#2993](https://github.com/eclipse-score/score/issues/2993) (v0.8) · [#3011](https://github.com/eclipse-score/score/issues/3011) (v0.9) · [#3013](https://github.com/eclipse-score/score/issues/3013) (v0.10) · [#3014](https://github.com/eclipse-score/score/issues/3014) (v1.0) — all in `eclipse-score/score`
> **Updated**: 2026-06-23

---

## Context

The S-CORE v1.0 roadmap is structured around four release gates.
Each gate has **mandatory minimum deliverables**; teams are free to advance ahead
but the gate requirements are non-negotiable for the final release.

| Milestone | Period               | Project Focus                                              |
| --------- | -------------------- | ---------------------------------------------------------- |
| **v0.8**  | 13 May – 13 Jul 2026 | Requirements Engineering (PA2) + Architecture Design (PA3) |
| **v0.9**  | 14 Jul – 7 Sep 2026  | Implementation — Detailed Design (PA4)                     |
| **v0.10** | 8 Sep – 2 Nov 2026   | Verification — Coverage & Inspection Completeness (PA5)    |
| **v1.0**  | 3 Nov – 15 Dec 2026  | Hardening & Release                                        |

**v0.8 establishes the baseline** for the Time module. The v0.8 delivery comprises all work
done to date — existing process artefacts in draft state, the existing CI setup, unit tests,
and the `reference_integration` onboarding
([#68](https://github.com/eclipse-score/inc_time/issues/68) under PI
[#69](https://github.com/eclipse-score/inc_time/issues/69)) — plus any work completed before
the gate closes on 13 Jul 2026.

This roadmap maps out a step-by-step plan to reach all v1.0 targets by December 2026, with
v0.9 as the primary catch-up sprint for process artefacts.

**Feature scope for v1.0** — features marked `deferred` have no `valid_from ≤ v1.0` on their
`feat_req` nodes and are automatically excluded from v1.0 coverage statistics. Their `feat_req`
nodes and component artefacts (`comp_req`, `comp_arc`, `comp_dd_sta`) will progress through the
full maturity cycle (`L2+` → `L4`) in a subsequent PI.

| Feature             |    v1.0    | Notes        |
| ------------------- | :--------: | ------------ |
| `SteadyTime`        |     ✓      |              |
| `SystemTime`        |     ✓      |              |
| `VehicleTime`       |     ✓      |              |
| `HighResSteadyTime` |     ✓      |              |
| `AbsoluteTime`      | `deferred` | post-v1.0 PI |

---

## Maturity Level Key

| Level | Symbol      | Meaning                                                                                                        |
| ----- | ----------- | -------------------------------------------------------------------------------------------------------------- |
| L0    | `—`         | Not started                                                                                                    |
| L1    | `draft`     | File/node exists; content incomplete or not yet consistent                                                     |
| L2    | `valid`     | sphinx-needs `:status: valid`; all TBDs resolved; fully traceable                                              |
| L2+   | `valid+`    | `valid` + `valid_from` attribute set on each node (enables release coverage statistics)                        |
| L3    | `filed`     | `valid` + inspection checklist submitted; all findings tracked with milestone ≤ v1.0                           |
| L3+   | `filed+`    | Inspection done; all findings documented; accepted deviations have a tracked resolution plan; `:status: valid` |
| L4    | `inspected` | `:status: valid(inspected)`; all findings resolved; approved by a Committer                                    |

For CI / quality metrics, target values are stated directly (e.g. `≥ 85%` C0, `≥ 50%`).

---

## Roadmap Overview

High-level plan — one row per artefact type across the whole module.

| Artefact / Process Area                          |         v0.8          |          v0.9           |          v0.10          |          v1.0           |
| ------------------------------------------------ | :-------------------: | :---------------------: | :---------------------: | :---------------------: |
| **PI ticket**                                    |    [#2993][pi-08]     |     [#3011][pi-09]      |     [#3013][pi-010]     |     [#3014][pi-10]      |
| **Feature requirements** (`feat_req`) ⁴          |         `L2`          |         `L2+` ¹         |          `L3`           |          `L4`           |
| **SW component requirements** (`comp_req`)       |          `—`          |          `L2`           |          `L3`           |          `L4`           |
| **Assumption of Use requirements** (`aou_req`)   |          `—`          |           `—`           |          `L2`           |          `L3+`          |
| Req. inspection checklists                       |          `—`          |           `—`           |          `L3`           |          `L3+`          |
| **Feature architecture** (`feat_arc_sta/dyn`)    |          `—`          |          `L2`           |          `L3`           |          `L4`           |
| **Component architecture** (`comp_arc_sta/dyn`)  |          `—`          |          `L2`           |          `L3`           |          `L4`           |
| Arch. inspection checklists                      |          `—`          |           `—`           |          `L3`           |          `L3+`          |
| **SW component detailed design** (`comp_dd_sta`) |          `—`          |          `L1`           |          `L2`           |          `L3+`          |
| Impl. inspection checklists                      |          `—`          |           `—`           |          `L3`           |          `L3+`          |
| Static code analysis — SCA (Clang-Tidy)          |       `active`        |       `reducing`        |       `reducing`        |    `active, tracked`    |
| Dynamic analysis — sanitizers (ASan/UBSan/TSan)  |       `active`        |       `reducing`        |       `reducing`        |       `reducing`        |
| `reference_integration` CI integration           | `CI integrated` (#68) |     `CI integrated`     |     `CI integrated`     |     `CI integrated`     |
| Unit tests + `RecordProperty` traceability       | `✓`, no traceability  |  `✓`  no traceability   |   `✓` + traceability    |      `✓ complete`       |
| C0 / C1 code coverage ²                          |      `≥ 85%` C0       | `≥ 85%` C0 / `≥ 50%` C1 | `≥ 90%` C0 / `≥ 70%` C1 | `≥ 95%` C0 / `≥ 85%` C1 |
| `comp_req` test coverage                         |          `—`          |           `—`           |         `≥ 25%`         |         `≥ 50%`         |
| Component Integration Tests                      |          `—`          |
| Feature Integration Tests                        |          `—`          |           `—`           |    `≥ 50% in-scope`     |    `≥ 50% in-scope`     |
| Module Verification Report                       |          `—`          |           `—`           |          `L1`           |         `L2` ³          |
| Platform Verification Report                     |          `—`          |           `—`           |           `—`           |          `L1`           |
| Safety & security analysis                       |          `—`          |           `—`           |          `L1`           |         `L2` ³          |

[pi-08]: https://github.com/eclipse-score/score/issues/2993
[pi-09]: https://github.com/eclipse-score/score/issues/3011
[pi-010]: https://github.com/eclipse-score/score/issues/3013
[pi-10]: https://github.com/eclipse-score/score/issues/3014

> ¹ `L2+`: set the `valid_from` attribute on each `feat_req` node; it marks the earliest release
> from which that requirement is counted in project coverage statistics.
> ² Existing `inc_time` CI enforces ≥ 85% C0 (line) coverage. v0.9 adds a C1 (branch) gate.
> ³ v1.0 targets for implementation, MVR, and safety analysis accept documented deviations;
> all open items must have a tracked resolution plan before release.
> ⁴ Targets apply to **in-scope** `feat_req` nodes only (those with `valid_from ≤ v1.0`). Deferred
> features (e.g. `AbsoluteTime`) are excluded from v1.0 statistics and will progress through the
> full `L2+` → `L4` cycle in a subsequent PI.

---

## Summary: Gate Compliance vs Project Requirements

The table below compares what the **project roadmap requires** at each gate against what the
Time module can **realistically deliver** given the v0.8 baseline state.

| Gate      | Target                                                                                                     | Expected Delivery                                                                                                                                    | Gap / Risk                                                       |
| --------- | ---------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------- |
| **v0.8**  | `feat_req` L2; `reference_integration` CI integrated                                                       | Stable API + functional chain; `reference_integration` integrated (#68); `feat_req` L2; `comp_dd_sta` at L1; `comp_req` / `comp_arc` not yet started | **Low** — accepted per plan; v0.9 opens all PA2/PA3 artefacts    |
| **v0.9**  | All `comp_req` + `aou_req` + `comp_arc` + `comp_dd_sta` L2; `feat_req` `valid_from` set                    | All comp artefacts L2; `feat_req` `valid_from` set; SCA reducing; `RecordProperty` traceability added; feat IT L1                                    | **Medium** — tight sprint scope; 4 components × 4 artefact types |
| **v0.10** | Req / arch L3; DD L3; SCA reducing; coverage ≥ 90%/70%; `comp_req` coverage ≥ 25%; feat IT ≥ 50%           | `feat_req` L3; `comp_req` + AoU + arch + DD L3; checklists filed; coverage ≥ 25%; feat IT ≥ 50% in-scope; MVR L1                                     | **Low** — safety L1 only; PVR deferred to v1.0                   |
| **v1.0**  | Req / arch L4; inspection checklists L3+; DD L3; coverage ≥ 95%/85%; `comp_req` cov ≥ 50%; MVR + safety L2 | All req/arch/feat-arc L4; inspection L3+; DD L3 ³; coverage ≥ 95%/85%; `comp_req` cov ≥ 50%; SCA active, tracked; MVR L2 ³; safety L2 ³; PVR L1      | **None** — targets met                                           |
