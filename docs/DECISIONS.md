# Architecture decision records

## ADR-001 — Semantic fixture model

**Decision:** authored looks target semantic attributes; profiles map attributes to DMX slots/ranges.

**Reason:** fixture model and channel order may change at short notice.

**Consequence:** profile compiler and validation are critical; no direct channel programming in looks.

## ADR-002 — Standalone authoring, VST3 runtime

**Decision:** full editing occurs in a standalone Windows application; Ableton uses a reduced VST3 runtime.

**Reason:** the designer must not purchase Ableton solely to edit lighting.

**Consequence:** both applications share engine and project format; corrections are delivered as project packages.

## ADR-003 — No audio analysis

**Decision:** runtime processes MIDI and optional host transport only.

**Reason:** authored synchronization and predictable operation are preferred.

## ADR-004 — Projector video remains in Ableton

**Decision:** plugin does not output projector video. Ableton's video window handles projection.

**Reason:** avoid duplicate video-server scope and preserve current show plan.

## ADR-005 — Windows-first and iPlug2 planned

**Decision:** first production targets are Windows x64 standalone and VST3 using a permissive framework.

**Reason:** target Lenovo/performer environment and no mandatory framework licence expenditure.

## ADR-006 — Art-Net first, named USB protocols second

**Decision:** Art-Net is initial production backend; USB support is implemented by protocol family.

**Reason:** no universal generic USB-DMX protocol exists.
