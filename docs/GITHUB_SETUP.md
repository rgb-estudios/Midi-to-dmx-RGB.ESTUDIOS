# GitHub repository setup

## Canonical repository

```text
https://github.com/rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS
```

This repository is public and is the only repository authorised for AEYLA Visual DMX development. Do not place project code, issues or releases inside the Tour Manager or RGB Estudios website repositories.

## Local clone

```bash
git clone https://github.com/rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS.git
cd Midi-to-dmx-RGB.ESTUDIOS
```

## Branch policy

- `main` must remain buildable.
- Use short-lived branches named `agent/<scope>` or `feature/<scope>`.
- Open a draft PR for incomplete work.
- Prefer squash merge after CI and review pass.
- Never force-push `main`.

## Recommended branch protection for `main`

- Require pull request.
- Require CI status `core-ci`.
- Require branch up to date.
- Prevent force push and deletion.
- Require conversation resolution.
- Allow repository owner emergency bypass only.

## Labels

- `area:core`
- `area:ui`
- `area:vst3`
- `area:media`
- `area:artnet`
- `area:usb`
- `area:profiles`
- `area:project-format`
- `priority:p0`
- `priority:p1`
- `priority:p2`
- `status:needs-hardware`
- `status:needs-design-review`
- `breaking-change`

## Milestones

- Foundation
- Core runtime
- Standalone alpha
- VST3 alpha
- Hardware alpha
- Rehearsal candidate
- Show release

## Secrets and public-repository policy

- Never commit certificates, tokens, passwords, private show media or customer data.
- Future signing secrets must be GitHub Actions secrets.
- Public examples must use synthetic or explicitly cleared media.
- Hardware serial numbers, private network details and personal information must be redacted from issues and logs.
