# Fixture profile specification

## Principle

A profile describes **meaning**, not merely channel order. Looks never reference a physical channel.

## Channel fields

- `slot`: 1-based slot inside the fixture footprint.
- `attribute`: canonical semantic identifier.
- `mode`: `continuous`, `range`, `constant`, `trigger`, `ignore`.
- `dmxMin`, `dmxMax`: output range.
- `home`: safe/off value.
- `invert`: reverses normalized mapping.
- `curve`: `linear` initially; future gamma/custom curves.
- `segments`: named ranges for shutter, strobe, macro and reset channels.
- `triggerDurationMs`: for protected temporary actions.
- `dangerous`: requires confirmation/arming.

## Supported attributes

| Identifier | Meaning | Automatic visual derivation |
|---|---|---|
| dimmer | master intensity | luminance or explicit look intensity |
| shutter | shutter/open state | no |
| strobe | native strobe speed | explicit only |
| red/green/blue | RGB emitters | yes |
| white | white emitter | configurable extraction |
| amber | amber emitter | configurable warm extraction |
| uv | ultraviolet emitter | manual by default |
| lime | lime emitter | configurable extraction |
| macro | internal fixture program | explicit only |
| speed | macro/effect speed | explicit only |
| reset | reset function | protected trigger only |
| zoom | beam angle | scene/executor/MIDI |
| fan | fan speed | explicit only |
| haze | haze output | explicit only |

## Behaviour when attributes are absent

- No physical dimmer: multiply colour channels by logical dimmer.
- No white/amber/lime/UV emitter: omit unsupported components; optional calibrated RGB compensation may be added later.
- No native strobe: software emulation is optional and disabled by default.
- Unsupported zoom/macro/reset: ignore and display capability warning.

## Reordering rule

Given equivalent semantic definitions, changing slots must change only DMX placement. The resolved semantic frame must remain identical. This is covered by automated tests.

## Replacement workflow

1. Duplicate the closest profile.
2. Enter footprint.
3. Assign each slot by semantic attribute.
4. Define ranges/home values.
5. Validate duplicates, gaps and dangerous functions.
6. Run test sequence.
7. Replace profile for selected fixtures.
8. Save as a new project version.
