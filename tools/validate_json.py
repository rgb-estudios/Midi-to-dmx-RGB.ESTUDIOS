#!/usr/bin/env python3
import json
from pathlib import Path

for path in Path('.').rglob('*.json'):
    json.loads(path.read_text(encoding='utf-8'))
    print(f'OK {path}')
