from pathlib import Path


def replace_exact(path: str, old: str, new: str, count: int = 1) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    actual = text.count(old)
    if actual != count:
        raise SystemExit(f"{path}: expected {count} occurrence(s), found {actual}: {old!r}")
    p.write_text(text.replace(old, new), encoding="utf-8")


main = "product/AeylaVisualDmx/AeylaMainControl.h"
show = "product/AeylaVisualDmx/AeylaShowPlayerIntegration.cpp"
runtime = "product/AeylaVisualDmx/AeylaVisualDmx.cpp"

# Visible product identity: AEYLA is the artist/show, not the software or network.
replace_exact(main, '               "AEYLA",\n', '               "RGB ESTUDIOS",\n')
replace_exact(main,
              '               "RGB ESTUDIOS · R09 PRETEST · MIDI REC DIRECT",\n',
              '               "SHOW CONTROL · R10.6 PRETEST",\n')
replace_exact(main,
              '                  " REC STOP finaliza · AEYLA no usa MTC.").c_str(),\n',
              '                  " REC STOP finaliza · el plugin no usa MTC.").c_str(),\n')
replace_exact(main,
              '    Field(g, mLocalNetworkField, "IPv4 AEYLA / MÁSCARA DE SUBRED",\n',
              '    Field(g, mLocalNetworkField, "IPv4 TX / MÁSCARA DE SUBRED",\n')
replace_exact(main,
              '      ui->ShowMessageBox(result.message.c_str(), "AEYLA · OPERACIÓN BLOQUEADA", kMB_OK);\n',
              '      ui->ShowMessageBox(result.message.c_str(), "RGB ESTUDIOS · OPERACIÓN BLOQUEADA", kMB_OK);\n')

# Same-address and asynchronous network paths must both tell the operator that
# the output stays disarmed AND blackout remains intentionally latched.
replace_exact(show,
              '          network->directed_broadcast + " · U1 · SALIDA DESARMADA";\n',
              '          network->directed_broadcast +\n'
              '          " · U1 · SALIDA DESARMADA · APAGÓN ACTIVO";\n')
replace_exact(show,
              '          "CAMBIO EN CURSO · AEYLA agregará " + network->address + "/" +\n',
              '          "CAMBIO EN CURSO · se agregará " + network->address + "/" +\n')
replace_exact(runtime,
              '                network->directed_broadcast + " · U1 · SALIDA DESARMADA"\n',
              '                network->directed_broadcast +\n'
              '                " · U1 · SALIDA DESARMADA · APAGÓN ACTIVO"\n')

print("R10.6 identity/network UI patch applied")
