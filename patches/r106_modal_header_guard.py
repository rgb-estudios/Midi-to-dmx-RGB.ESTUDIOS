from pathlib import Path

path = Path('product/AeylaVisualDmx/AeylaRuntimeStatusControl.h')
text = path.read_text(encoding='utf-8')

old = '''    // The shell is always first. Workspace navigation is presentation-only and\n    // can never touch physical authority. ARM and APAGÓN are explicit actions.\n    for(std::size_t index = 0U; index < 4U; ++index)\n    {\n'''
new = '''    // ARCHIVO is modal for navigation/content, while the two physical safety\n    // controls remain immediately reachable. A first click on a tab therefore\n    // closes ARCHIVO and is consumed instead of changing workspace underneath.\n    if(mFileMenuOpen && Contains(Header(), x, y) &&\n       !Contains(HeaderArmButton(), x, y) &&\n       !Contains(HeaderBlackoutButton(), x, y))\n    {\n      mFileMenuOpen = false;\n      SetDirty(false);\n      return;\n    }\n\n    // The shell is always first. Workspace navigation is presentation-only and\n    // can never touch physical authority. ARM and APAGÓN are explicit actions.\n    for(std::size_t index = 0U; index < 4U; ++index)\n    {\n'''
if text.count(old) != 1:
    raise SystemExit(f'header insertion count={text.count(old)}')
text = text.replace(old, new, 1)

text = text.replace(
    'EN VIVO · operación limpia; CONFIGURAR abre Learn, modo y fade sólo para una memoria.',
    'EN VIVO · operación limpia; EDITAR abre DMX, MIDI, modo y fade sólo para una memoria.'
)
text = text.replace(
    'EN VIVO · selecciona una memoria para operar; CONFIGURAR abre Learn/fade/modo sólo en esa memoria.',
    'EN VIVO · selecciona una memoria para operar; EDITAR abre DMX/MIDI/modo/fade sólo en esa memoria.'
)

path.write_text(text, encoding='utf-8')
print('R10.6 modal header guard patched')
