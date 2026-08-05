# Visual design system

## Product character

The editor must feel like a professional live-show instrument: dark, spatial, calm and precise. It must not resemble a generic admin dashboard, spreadsheet or hobby DMX utility.

## Visual hierarchy

1. **Canvas is dominant.** The visual source and logical fixture samples occupy the largest area.
2. **Executors are immediate.** Large, legible pads remain visible without scrolling.
3. **Inspector is contextual.** Only parameters for the selected object are shown.
4. **System state is persistent.** Output armed/disarmed, backend, project version and errors are always visible.

## Layout

```text
Top bar: project / version / validation / output / arm
Left rail: sources, palettes, scenes, profiles, patch
Centre: visual canvas and fixture sampling
Right inspector: selected item parameters
Bottom: executors, layer state and master controls
```

Minimum supported viewport: 1366×768. Primary target: 1920×1080.

## Colour

- Near-black background, not pure black.
- Neutral graphite panels with clear elevation.
- White text with restrained contrast levels.
- Colour is reserved for authored palettes, executor identity, active states and warnings.
- Avoid default “tech blue” as the dominant brand colour.
- Output armed uses a clear but non-neon red state; connected/valid uses restrained green.

## Typography

- Humanist/geometric sans serif.
- Tabular numerals for DMX, MIDI, time and addresses.
- No condensed decorative type for operational values.
- Minimum operational text 12 px; executor labels 13–16 px depending on size.

## Canvas

- Logical fixtures are sampled points with visible active/inactive state.
- Fixture labels appear on hover/selection, not continuously when cluttered.
- Show sampled colour as ring/fill and provide a semantic-value inspector.
- Rig 10 keeps all 14 logical positions visible; missing physical fixtures appear ghosted.

## Motion

- UI animation is subtle and must never obscure output state.
- Canvas preview may run at 30/60 fps.
- Executor activation feedback is immediate (<50 ms perceived).
- Dangerous actions do not use playful animation.

## Accessibility and operation

- Never rely on colour alone for armed/error/selection state.
- Full keyboard navigation for executor grid and profile table.
- Large hit areas for stage use.
- Confirmation for reset and destructive project changes.
- High-contrast mode planned after core UI stabilizes.

## Visual acceptance tests

- Screenshot review at both supported resolutions.
- Canvas remains dominant.
- No horizontal scroll in standard workflow.
- All persistent safety state visible.
- Long project and fixture names truncate safely with tooltip.
- Executor labels remain readable at 16-pad layout.
