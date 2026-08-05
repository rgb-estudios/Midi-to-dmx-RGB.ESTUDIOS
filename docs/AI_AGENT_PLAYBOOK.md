# AI agent playbook

## New agent onboarding

Read in order:

1. `AGENTS.md`
2. `docs/PRODUCT_SPEC.md`
3. `docs/ARCHITECTURE.md`
4. `docs/VISUAL_DESIGN_SYSTEM.md`
5. relevant subsystem spec
6. `docs/DECISIONS.md`
7. current issues and changelog

Then run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Open the UI prototype and inspect at both target resolutions.

## Audit report format

```text
Scope:
Build/test status:
Current implementation status:
P0 defects:
P1 defects:
Architecture violations:
Visual/interaction defects:
Safety defects:
Compatibility/migration risks:
Hardware claims lacking evidence:
Recommended patch order:
Files changed:
Validation performed:
Remaining uncertainty:
```

## Correction rules

- Prefer the smallest architecture-consistent patch.
- Do not hide unresolved failures.
- Add regression tests.
- Preserve project-format compatibility or provide migration.
- Update screenshots for UI changes.
- Separate simulated and hardware-tested claims.
- Never make safety defaults more permissive without an explicit decision record.

## Handoff rules

Every agent leaves:

- updated changelog;
- exact commands run;
- test results;
- screenshots/evidence paths;
- open risks;
- next concrete task;
- no undocumented local-only configuration.
