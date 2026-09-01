from pathlib import Path

p = Path('product/AeylaVisualDmx/AeylaRuntimeStatusControl.h')
text = p.read_text(encoding='utf-8')
old = ' · R10.7 PRETEST'
new = ' · R10.8 PRETEST'
count = text.count(old)
if count != 1:
    raise SystemExit(f'expected exactly one visible R10.7 PRETEST marker, found {count}')
p.write_text(text.replace(old, new, 1), encoding='utf-8')
print('R10.8 visible PRETEST identity applied')
