# Documentation index

## Product and design

0. [`AEYLA_OPERATOR_WORKFLOW_ES.md`](AEYLA_OPERATOR_WORKFLOW_ES.md) — flujo
   humano objetivo Look → Cue → Song → Show y frontera del build actual.
1. [`PRODUCT_SPEC.md`](PRODUCT_SPEC.md) — complete product definition and boundaries.
2. [`USER_WORKFLOWS.md`](USER_WORKFLOWS.md) — editor, corrections, runtime and emergency workflows.
3. [`VISUAL_DESIGN_SYSTEM.md`](VISUAL_DESIGN_SYSTEM.md) — mandatory visual and interaction direction.
4. [`MIDI_EXECUTOR_SPEC.md`](MIDI_EXECUTOR_SPEC.md) — notes, layers, priorities and executor behaviour.

## Technical architecture

5. [`ARCHITECTURE.md`](ARCHITECTURE.md) — systems, boundaries and data flow.
6. [`FIXTURE_PROFILE_SPEC.md`](FIXTURE_PROFILE_SPEC.md) — semantic channel model.
7. [`PROJECT_FILE_FORMAT.md`](PROJECT_FILE_FORMAT.md) — `.aeylashow` package.
8. [`OUTPUT_BACKENDS.md`](OUTPUT_BACKENDS.md) — Art-Net and USB-DMX.
9. [`REALTIME_AND_SAFETY.md`](REALTIME_AND_SAFETY.md) — threading and fail-safe rules.
10. [`VST3_INTEGRATION_PLAN.md`](VST3_INTEGRATION_PLAN.md) — thin Ableton adapter and first host spine.
11. [`VST3_PLATFORM_ARCHITECTURE.md`](VST3_PLATFORM_ARCHITECTURE.md) — concrete silent-instrument topology, callback contract, host events and platform targets.
12. [`VST3_STATE_LIFECYCLE_SPEC.md`](VST3_STATE_LIFECYCLE_SPEC.md) — persistence, reload, state migration and safe lifecycle.
13. [`PLUGIN_DEPENDENCY_DECISION.md`](PLUGIN_DEPENDENCY_DECISION.md) — iPlug2/VST3 SDK proof and pinning decision.
14. [`VST3_RESEARCH_NOTES_2026-08-05.md`](VST3_RESEARCH_NOTES_2026-08-05.md) — official Ableton, Steinberg, Apple and iPlug2 findings plus unresolved proof questions.

## Validation, build and release

14a. [`AEYLA_VALIDATION_MATRIX.md`](AEYLA_VALIDATION_MATRIX.md) — estado de
     evidencia actual por host/plataforma y gates de Show Candidate.
14b. [`AEYLA_UI_FUNCTIONAL_MATRIX.md`](AEYLA_UI_FUNCTIONAL_MATRIX.md) — cadena
     control → handler → runtime → persistencia y bloqueos P0.
15. [`ABLETON_HOST_TEST_MATRIX.md`](ABLETON_HOST_TEST_MATRIX.md) — real Windows/macOS Ableton acceptance matrix.
16. [`CROSS_PLATFORM_BUILD_RELEASE.md`](CROSS_PLATFORM_BUILD_RELEASE.md) — builds, installers, signing, notarization and artifacts.
17. [`BUG_PREVENTION_AND_QA.md`](BUG_PREVENTION_AND_QA.md) — defect prevention, realtime audit and quality gates.
18. [`TEST_PLAN.md`](TEST_PLAN.md) — automated, hardware and show tests.
19. [`HARDWARE_VALIDATION.md`](HARDWARE_VALIDATION.md) — evidence matrix.
20. [`RELEASE_PROCESS.md`](RELEASE_PROCESS.md) — builds, packaging and rollback.

## Delivery and governance

21. [`ROADMAP.md`](ROADMAP.md) — staged implementation.
22. [`AI_AGENT_PLAYBOOK.md`](AI_AGENT_PLAYBOOK.md) — audit/correction procedure.
23. [`DECISIONS.md`](DECISIONS.md) — architecture decision records.
24. [`RISK_REGISTER.md`](RISK_REGISTER.md) — risks, severity and mitigations.
25. [`GITHUB_SETUP.md`](GITHUB_SETUP.md) — repository configuration.
26. [`PARALLEL_AI_HANDOFF.md`](PARALLEL_AI_HANDOFF.md) — operational context for parallel agents.
