..
   # *******************************************************************************
   # Copyright (c) 2026 Contributors to the Eclipse Foundation
   #
   # See the NOTICE file(s) distributed with this work for additional
   # information regarding copyright ownership.
   #
   # This program and the accompanying materials are made available under the
   # terms of the Apache License Version 2.0 which is available at
   # https://www.apache.org/licenses/LICENSE-2.0
   #
   # SPDX-License-Identifier: Apache-2.0
   # *******************************************************************************

.. _time-module-roadmap:

Time Module — Roadmap to S-CORE v1.0
=====================================

| **Reference**: `S-CORE v1.0 Roadmap <https://github.com/eclipse-score/reference_integration/tree/main/docs/s_core_v_1>`_
| **Module board**: `TIM - Time FT #57 <https://github.com/orgs/eclipse-score/projects/57>`_
| **PI tickets**: `#2993 <https://github.com/eclipse-score/score/issues/2993>`_ (v0.8) · `#3011 <https://github.com/eclipse-score/score/issues/3011>`_ (v0.9) · `#3013 <https://github.com/eclipse-score/score/issues/3013>`_ (v0.10) · `#3014 <https://github.com/eclipse-score/score/issues/3014>`_ (v1.0) — all in ``eclipse-score/score``
| **Updated**: 2026-06-24

Context
-------

The S-CORE v1.0 roadmap is structured around four release gates.
Each gate has **mandatory minimum deliverables**; teams are free to advance ahead
but the gate requirements are non-negotiable for the final release.

.. list-table::
   :header-rows: 1
   :widths: auto

   * - Milestone
     - Period
     - Project Focus
   * - **v0.8**
     - 13 May – 13 Jul 2026
     - Requirements Engineering (PA2) + Architecture Design (PA3)
   * - **v0.9**
     - 14 Jul – 7 Sep 2026
     - Implementation — Detailed Design (PA4)
   * - **v0.10**
     - 8 Sep – 2 Nov 2026
     - Verification — Coverage & Inspection Completeness (PA5)
   * - **v1.0**
     - 3 Nov – 15 Dec 2026
     - Hardening & Release

**v0.8 establishes the baseline** for the Time module. The v0.8 delivery comprises all work
done to date — existing process artefacts in draft state, the existing CI setup, unit tests,
and the ``reference_integration`` onboarding
(`#68 <https://github.com/eclipse-score/inc_time/issues/68>`_ under PI
`#69 <https://github.com/eclipse-score/inc_time/issues/69>`_) — plus any work completed before
the gate closes on 13 Jul 2026.

This roadmap maps out a step-by-step plan to reach all v1.0 targets by December 2026, with
v0.9 as the primary catch-up sprint for process artefacts.

**Feature scope for v1.0** — features marked ``deferred`` have no ``valid_from ≤ v1.0`` on their
``feat_req`` nodes and are automatically excluded from v1.0 coverage statistics. Their ``feat_req``
nodes and component artefacts (``comp_req``, ``comp_arc``, ``comp_dd_sta``) will progress through
the full maturity cycle (``L2+`` → ``L4``) in a subsequent PI.

.. list-table::
   :header-rows: 1
   :widths: auto

   * - Feature
     - v1.0
     - Notes
   * - ``SteadyTime``
     - ✓
     -
   * - ``SystemTime``
     - ✓
     -
   * - ``VehicleTime``
     - ✓
     -
   * - ``HighResSteadyTime``
     - ✓
     -
   * - ``AbsoluteTime``
     - ``deferred``
     - post-v1.0 PI

Maturity Level Key
------------------

.. list-table::
   :header-rows: 1
   :widths: 8 18 74

   * - Level
     - Symbol
     - Meaning
   * - L0
     - ``—``
     - Not started
   * - L1
     - ``draft``
     - File/node exists; content incomplete or not yet consistent
   * - L2
     - ``valid``
     - sphinx-needs ``:status: valid``; all TBDs resolved; fully traceable
   * - L2+
     - ``valid+``
     - ``valid`` + ``valid_from`` attribute set on each node (enables release coverage statistics)
   * - L3
     - ``filed``
     - ``valid`` + inspection checklist submitted; all findings tracked with milestone ≤ v1.0
   * - L3+
     - ``filed+``
     - Inspection done; all findings documented; accepted deviations have a tracked resolution plan; ``:status: valid``
   * - L4
     - ``inspected``
     - ``:status: valid(inspected)``; all findings resolved; approved by a Committer

For CI / quality metrics, target values are stated directly (e.g. ``≥ 85%`` C0, ``≥ 50%``).

Roadmap Overview
----------------

High-level plan — one row per artefact type across the whole module.

.. list-table::
   :header-rows: 1
   :widths: 44 14 14 14 14

   * - Artefact / Process Area
     - v0.8
     - v0.9
     - v0.10
     - v1.0
   * - **PI ticket**
     - `#2993 <https://github.com/eclipse-score/score/issues/2993>`_
     - `#3011 <https://github.com/eclipse-score/score/issues/3011>`_
     - `#3013 <https://github.com/eclipse-score/score/issues/3013>`_
     - `#3014 <https://github.com/eclipse-score/score/issues/3014>`_
   * - ``feat_req`` :sup:`4`
     - ``L2``
     - ``L2+`` :sup:`1`
     - ``L3``
     - ``L4``
   * - ``comp_req``
     - ``—``
     - ``L2``
     - ``L3``
     - ``L4``
   * - ``aou_req``
     - ``—``
     - ``L2``
     - ``L3``
     - ``L4``
   * - Req inspection checklists
     - ``—``
     - ``—``
     - ``L3``
     - ``L3+``
   * - ``feat_arc``
     - ``—``
     - ``L2``
     - ``L3``
     - ``L4``
   * - ``comp_arc``
     - ``—``
     - ``L2``
     - ``L3``
     - ``L4``
   * - Arch inspection checklists
     - ``—``
     - ``—``
     - ``L3``
     - ``L3+``
   * - ``comp_dd_sta``
     - ``L1``
     - ``L2``
     - ``L3``
     - ``L3`` :sup:`3`
   * - Impl inspection checklists
     - ``—``
     - ``—``
     - ``L3``
     - ``L3+``
   * - SCA
     - ``active``
     - ``reducing``
     - ``reducing``
     - ``active, tracked``
   * - sanitizers
     - ``active``
     - ``reducing``
     - ``reducing``
     - ``reducing``
   * - ``reference_integration`` CI
     - ``CI integrated`` (`#68 <https://github.com/eclipse-score/inc_time/issues/68>`_)
     - ``CI integrated``
     - ``CI integrated``
     - ``CI integrated``
   * - Unit tests + ``RecordProperty``
     - ``✓``, no traceability
     - ``✓`` + traceability
     - ``✓ complete``
     - ``✓ complete``
   * - C0 / C1 coverage :sup:`2`
     - ``≥ 85%`` C0
     - ``≥ 85%`` C0 / ``≥ 50%`` C1
     - ``≥ 90%`` C0 / ``≥ 70%`` C1
     - ``≥ 95%`` C0 / ``≥ 85%`` C1
   * - ``comp_req`` test coverage
     - ``—``
     - ``—``
     - ``≥ 25%``
     - ``≥ 50%``
   * - Component IT
     - ``✓ existing``
     - ``extended``
     - ``complete``
     - ``complete``
   * - Feature IT
     - ``—``
     - ``L1``
     - ``≥ 50% in-scope``
     - ``≥ 75% in-scope``
   * - MVR
     - ``—``
     - ``—``
     - ``L1``
     - ``L2`` :sup:`3`
   * - PVR
     - ``—``
     - ``—``
     - ``—``
     - ``L1``
   * - Safety & security
     - ``—``
     - ``—``
     - ``L1``
     - ``L2`` :sup:`3`

**Notes**

:sup:`1` ``L2+``: set the ``valid_from`` attribute on each ``feat_req`` node; it marks the
earliest release from which that requirement is counted in project coverage statistics.

:sup:`2` Existing ``inc_time`` CI enforces ≥ 85% C0 (line) coverage. v0.9 adds a C1 (branch) gate.

:sup:`3` v1.0 targets for implementation, MVR, and safety analysis accept documented deviations;
all open items must have a tracked resolution plan before release.

:sup:`4` Targets apply to **in-scope** ``feat_req`` nodes only (those with ``valid_from ≤ v1.0``).
Deferred features (e.g. ``AbsoluteTime``) are excluded from v1.0 statistics and will progress
through the full ``L2+`` → ``L4`` cycle in a subsequent PI.

Summary: Gate Compliance vs Project Requirements
-------------------------------------------------

The table below compares what the **project roadmap requires** at each gate against what the
Time module can **realistically deliver** given the v0.8 baseline state.

.. list-table::
   :header-rows: 1
   :widths: 8 30 37 25

   * - Gate
     - Target
     - Expected Delivery
     - Gap / Risk
   * - **v0.8**
     - ``feat_req`` L2; ``reference_integration`` CI integrated
     - Stable API + functional chain; ``reference_integration`` integrated (`#68 <https://github.com/eclipse-score/inc_time/issues/68>`_); ``feat_req`` L2; ``comp_dd_sta`` at L1; ``comp_req`` / ``comp_arc`` not yet started
     - **Low** — accepted per plan; v0.9 opens all PA2/PA3 artefacts
   * - **v0.9**
     - All ``comp_req`` + ``aou_req`` + ``comp_arc`` + ``comp_dd_sta`` L2; ``feat_req`` ``valid_from`` set
     - All comp artefacts L2; ``feat_req`` ``valid_from`` set; SCA reducing; ``RecordProperty`` traceability added; feat IT L1
     - **Medium** — tight sprint scope; 4 components × 4 artefact types
   * - **v0.10**
     - Req / arch L3; DD L3; SCA reducing; coverage ≥ 90%/70%; ``comp_req`` coverage ≥ 25%; feat IT ≥ 50%
     - ``feat_req`` L3; ``comp_req`` + AoU + arch + DD L3; checklists filed; coverage ≥ 25%; feat IT ≥ 50% in-scope; MVR L1
     - **Low** — safety L1 only; PVR deferred to v1.0
   * - **v1.0**
     - Req / arch L4; inspection checklists L3+; DD L3; coverage ≥ 95%/85%; ``comp_req`` cov ≥ 50%; MVR + safety L2
     - All req/arch/feat-arc L4; inspection L3+; DD L3 :sup:`3`; coverage ≥ 95%/85%; ``comp_req`` cov ≥ 50%; SCA active, tracked; MVR L2 :sup:`3`; safety L2 :sup:`3`; PVR L1
     - **None** — targets met
