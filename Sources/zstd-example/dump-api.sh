#!/bin/bash

# This is a helper to show what the Swift API looks like.

xcrun swift-api-digester \
    -dump-sdk \
    -module Zstd \
    -o /tmp/zstd-api.json \
    -I lib \
    -I Sources/Zstd/include

python3 <<'PY'
import json

with open('/tmp/zstd-api.json') as f:
    d = json.load(f)

root = d.get('ABIRoot', d)
top  = root.get('children', [])

# Hide the C-builtin typedefs that the importer drags in.
SKIP_TYPEALIAS = {'__NSConstantString', '__builtin_ms_va_list', '__builtin_va_list',
                  'ptrdiff_t', 'rsize_t', 'size_t', 'wchar_t'}

by_kind = {}
for c in top:
    k = c.get('kind', '')
    if k == 'Import':
        continue
    if k == 'TypeAlias' and c.get('name') in SKIP_TYPEALIAS:
        continue
    by_kind.setdefault(k, []).append(c)

def banner(title):
    print('\n' + '=' * 70)
    print(title)
    print('=' * 70)

banner(f"TYPES ({len(by_kind.get('TypeDecl', []))})")
for c in sorted(by_kind.get('TypeDecl', []), key=lambda x: x['name']):
    print(f"\n  {c['name']}")
    for m in c.get('children', []):
        mk = m.get('kind', '')
        # Skip the synthesised .RawValue typealias inside imported enums.
        if mk == 'TypeAlias' and m.get('name') == 'RawValue':
            continue
        if mk in ('Constructor', 'Var', 'Function'):
            print(f"      .{m.get('printedName', m.get('name', ''))}")

banner(f"TYPEALIASES ({len(by_kind.get('TypeAlias', []))})")
for c in sorted(by_kind.get('TypeAlias', []), key=lambda x: x['name']):
    print(f"  {c['name']}")

banner(f"CONSTANTS ({len(by_kind.get('Var', []))})")
for c in sorted(by_kind.get('Var', []), key=lambda x: x['name']):
    print(f"  let {c.get('printedName', c['name'])}")

banner(f"FUNCTIONS ({len(by_kind.get('Function', []))})")
for c in sorted(by_kind.get('Function', []), key=lambda x: x.get('printedName', x['name'])):
    print(f"  {c.get('printedName', c['name'])}")
PY
