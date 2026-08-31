from pathlib import Path

p = Path('.github/workflows/reaper-windows-pretest-package.yml')
text = p.read_text(encoding='utf-8')
replacements = {
    '      - docs/R10_6_PRO_UI_PRETEST.md\n': '      - docs/R10_6_PRO_UI_PRETEST.md\n      - docs/R10_7_LIVE_MEMORIES_PRETEST.md\n',
    'name: Windows / RGB Live Control R10.6 / manual REAPER pretest': 'name: Windows / RGB Live Control R10.7 / manual REAPER pretest',
    '          Copy-Item docs/R10_6_PRO_UI_PRETEST.md (Join-Path $root "R10_6_PRO_UI_PRETEST.md")\n': '          Copy-Item docs/R10_6_PRO_UI_PRETEST.md (Join-Path $root "R10_6_PRO_UI_PRETEST.md")\n          Copy-Item docs/R10_7_LIVE_MEMORIES_PRETEST.md (Join-Path $root "R10_7_LIVE_MEMORIES_PRETEST.md")\n',
    'SHOW / AEYLA — R10.6 WINDOWS / REAPER PRETEST': 'SHOW / AEYLA — R10.7 WINDOWS / REAPER PRETEST',
    '4. Confirma visualmente R10.6 PRETEST y revisa BUILD_ID.txt.': '4. Confirma visualmente R10.7 PRETEST y revisa BUILD_ID.txt.',
    'INTERFAZ / INTERACCION R10.6': 'INTERFAZ / INTERACCION R10.7',
    '- FRONTAL, HUMO/HAZE, BASE BLANCA y TEST LUMINARIAS.\n          - Operacion muestra pad/fader; EDITAR contiene Learn, modo y fade.\n          - DMX Learn usa snapshots OFF -> ON y mascara sparse de slots cambiados.': '- Cuatro memorias grandes por pagina: 1-4 / 5-8.\n          - + MEMORIA amplia el show hasta un maximo de 8 sin comprimir executors.\n          - Cada memoria se puede renombrar; nombre, cantidad, DMX y MIDI persisten en live.bin v2.\n          - Shows R10.6 live.bin v1 abren con sus cuatro memorias historicas sin conversion destructiva.\n          - Operacion muestra pad/fader; EDITAR contiene NOMBRE / DMX / MIDI / MODO / FADE.\n          - DMX Learn usa snapshots OFF -> ON y mascara sparse de slots cambiados.',
    'Lee R10_5_BLACKOUT_UI_CONTRACT.md, R10_6_PRO_UI_PRETEST.md y R10_FIELD_PRETEST_ES.md.': 'Lee R10_5_BLACKOUT_UI_CONTRACT.md, R10_6_PRO_UI_PRETEST.md, R10_7_LIVE_MEMORIES_PRETEST.md y R10_FIELD_PRETEST_ES.md.',
    '"BUILD_KIND=RGB_LIVE_CONTROL_R10_6_REAPER_WINDOWS_PRETEST"': '"BUILD_KIND=RGB_LIVE_CONTROL_R10_7_REAPER_WINDOWS_PRETEST"',
    'name: RGB-Live-Control-R10.6-REAPER-Windows-PRETEST': 'name: RGB-Live-Control-R10.7-REAPER-Windows-PRETEST',
}
for old, new in replacements.items():
    count = text.count(old)
    if count != 1:
        raise SystemExit(f'expected one match for {old!r}, found {count}')
    text = text.replace(old, new)
p.write_text(text, encoding='utf-8')
print('R10.7 package identity finalized')
