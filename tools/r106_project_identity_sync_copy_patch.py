from pathlib import Path


def patch(path: str, replacements):
    p = Path(path)
    text = p.read_text(encoding='utf-8')
    for old, new, expected in replacements:
        actual = text.count(old)
        if actual != expected:
            raise SystemExit(f'{path}: expected {expected}, found {actual}: {old!r}')
        text = text.replace(old, new)
    p.write_text(text, encoding='utf-8')

patch('product/AeylaVisualDmx/AeylaRuntimeStatusControl.h', [
    (
'''    g.DrawText(IText(9.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "RGB ESTUDIOS · SHOW / AEYLA · R10.6 PRETEST",
               IRECT(header.L + 14.0F, header.T + 28.0F,
                     header.L + 330.0F, header.T + 45.0F));
''',
'''    std::string projectName = mPlug.ProjectName();
    if(projectName == "Untitled AEYLA Show")
      projectName = "AEYLA";  // Legacy project default: preserve stored intent.
    else if(projectName.empty() || projectName == "Untitled Show")
      projectName = "SIN TÍTULO";
    const std::string showContext =
        "RGB ESTUDIOS · SHOW / " + projectName + " · R10.6 PRETEST";
    g.DrawText(IText(9.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle),
               showContext.c_str(),
               IRECT(header.L + 14.0F, header.T + 28.0F,
                     header.L + 440.0F, header.T + 45.0F));
''', 1)
])

patch('src/project/project_document.h', [
    ('  std::string name{"Untitled AEYLA Show"};\n',
     '  std::string name{"Untitled Show"};\n', 1)
])

patch('src/project/project_document.cpp', [
    ('  document.name = "Untitled AEYLA Show";\n',
     '  document.name = "Untitled Show";\n', 1)
])

patch('product/AeylaVisualDmx/AeylaShowPlayerIntegration.cpp', [
    ('                      " · REPRODUCIR / MTC";\n',
     '                      " · ANCLA DAW / MIDI SHOW";\n', 1),
    ('      syncMessage = " · SIN ANCLA REPRODUCIR / MTC · AJUSTA ENTRADA MANUALMENTE";\n',
     '      syncMessage = " · SIN ANCLA DAW / MIDI SHOW · AJUSTA ENTRADA MANUALMENTE";\n', 1),
    ('          ? " · ESPERANDO REPRODUCIR / MTC"\n',
     '          ? " · ESPERANDO PLAY DEL DAW / MIDI SHOW"\n', 1),
    ('      result += " · ESPERANDO REPRODUCIR / MTC";\n',
     '      result += " · ESPERANDO PLAY DEL DAW / MIDI SHOW";\n', 1),
])

patch('product/AeylaVisualDmx/AeylaMidiShowIntegration.cpp', [
    ('    error_message = "MIDI PLAY bloqueado mientras AEYLA está grabando";\n',
     '    error_message = "MIDI PLAY bloqueado mientras GRABAR DMX está activo";\n', 1)
])

patch('src/product/project_file_controller.cpp', [
    ('                           "Could not create a valid AEYLA project",\n',
     '                           "Could not create a valid project",\n', 1),
    ('                           "Could not open AEYLA project package",\n',
     '                           "Could not open project package",\n', 1),
    ('                           "Could not save AEYLA project package",\n',
     '                           "Could not save project package",\n', 1),
])

print('R10.6 project identity/sync copy patch applied')
