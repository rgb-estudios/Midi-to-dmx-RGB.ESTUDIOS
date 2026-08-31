from pathlib import Path

path = Path('product/AeylaVisualDmx/AeylaRuntimeStatusControl.h')
text = path.read_text(encoding='utf-8')

old_hit = '''  bool IsHit(float x, float y) const override\n  {\n    if(Contains(Header(), x, y) || Contains(Footer(), x, y)) return true;\n    if(mPlug.UiWorkspace() == 1) return Contains(mRECT, x, y);\n    if(mFileMenuOpen && Contains(FileMenuPanel(), x, y)) return true;\n    return false;\n  }\n'''
new_hit = '''  bool IsHit(float x, float y) const override\n  {\n    if(Contains(Header(), x, y) || Contains(Footer(), x, y)) return true;\n    // A modal file menu owns the whole editor surface while open. Otherwise a\n    // click outside the panel can fall through to MainControl and trigger a\n    // transport/timeline/system action while the menu remains visible.\n    if(mFileMenuOpen) return Contains(mRECT, x, y);\n    if(mPlug.UiWorkspace() == 1) return Contains(mRECT, x, y);\n    return false;\n  }\n'''
if text.count(old_hit) != 1:
    raise SystemExit(f'IsHit block count={text.count(old_hit)}')
text = text.replace(old_hit, new_hit, 1)

old_arm = '''    if(Contains(HeaderArmButton(), x, y))\n    {\n      ReportLive(mPlug.ToggleTakeOutputArmFromUI());\n      SetDirty(false);\n      return;\n    }\n'''
new_arm = '''    if(Contains(HeaderArmButton(), x, y))\n    {\n      const bool takeArmed = mPlug.TakeOutputArmed();\n      const bool modelArmed = mPlug.OutputArmed();\n\n      // The header is the single global authority control. If either legacy\n      // model authority or Take authority is active, one DESARMAR gesture must\n      // remove every voluntary authority represented by this button. Never\n      // show DESARMAR and then route the click into an incompatible ARM path.\n      if(takeArmed || modelArmed)\n      {\n        if(takeArmed)\n          ReportLive(mPlug.ToggleTakeOutputArmFromUI());\n        if(modelArmed)\n          mPlug.ForceDisarmFromUI();\n        if(modelArmed && !takeArmed)\n        {\n          mLiveMessageError = false;\n          mLiveMessage = \"SALIDA DESARMADA · autoridad física retirada\";\n        }\n      }\n      else\n        ReportLive(mPlug.ToggleTakeOutputArmFromUI());\n\n      SetDirty(false);\n      return;\n    }\n'''
if text.count(old_arm) != 1:
    raise SystemExit(f'Header ARM block count={text.count(old_arm)}')
text = text.replace(old_arm, new_arm, 1)

path.write_text(text, encoding='utf-8')
print('R10.6 global ARM and modal overlay interaction patched')
