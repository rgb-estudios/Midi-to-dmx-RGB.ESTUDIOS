# Contributing

## Branches

- `main`: stable documentation and tested core.
- `agent/<scope>`: AI-assisted changes.
- `feature/<scope>`: product features.
- `fix/<scope>`: defects.

## Commit format

```text
<area>: <imperative summary>
```

Examples:

- `core: preserve semantic mapping across fixture reorder`
- `ui: improve executor hierarchy`
- `docs: define USB backend acceptance tests`

## Pull request requirements

- Scope and user impact.
- Architectural layer affected.
- Tests and validation evidence.
- Screenshots for UI changes.
- Hardware model and driver version for hardware claims.
- Project-format compatibility statement.
- Updated changelog.

No PR may describe a hardware feature as complete without evidence in `docs/HARDWARE_VALIDATION.md`.
