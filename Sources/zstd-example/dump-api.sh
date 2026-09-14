#!/bin/bash

# This is a helper to show what the Swift API looks like — full signatures
# including parameter and return types, optionality, etc.

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

# Swift keywords that need to be backticked when used as identifiers.
SWIFT_KEYWORDS = {
    'continue', 'break', 'case', 'class', 'default', 'else', 'for', 'if',
    'in', 'let', 'var', 'while', 'return', 'switch', 'do', 'try', 'throw',
    'init', 'func', 'enum', 'struct', 'protocol', 'extension', 'where',
    'self', 'Self', 'super', 'true', 'false', 'nil', 'fallthrough',
    'guard', 'defer', 'repeat', 'is', 'as', 'throws', 'rethrows', 'async',
    'await', 'inout', 'operator', 'precedencegroup', 'subscript',
}

def strip_module(t: str) -> str:
    """Remove common module prefixes for readability."""
    if not t:
        return t
    return t.replace('Swift.', '').replace('Zstd.', '')

def backtick(name: str) -> str:
    return f"`{name}`" if name in SWIFT_KEYWORDS else name

def type_children(node):
    """Return only the type-shaped children (parameters / return type)."""
    return [c for c in node.get('children', [])
            if c.get('kind', '').startswith('Type')
            and c.get('kind') != 'TypeAlias']

def split_signature(printed: str):
    """Split 'foo(a:b:c:)' into ('foo', ['a','b','c'])."""
    if '(' not in printed:
        return printed, []
    base, _, rest = printed.partition('(')
    return base, [l for l in rest.rstrip(')').split(':') if l]

def emit_function(node, indent: str = '') -> str:
    name, labels = split_signature(node.get('printedName', node['name']))
    types = type_children(node)
    if types:
        ret = strip_module(types[0].get('printedName', '?'))
        params = [strip_module(t.get('printedName', '?')) for t in types[1:]]
    else:
        ret, params = '?', []

    is_init = node.get('declKind') == 'Constructor'
    if is_init:
        # Constructors include the Self type as the first "param type" in the
        # JSON dump; drop it since it isn't actually written at the call site.
        params = params  # params already excludes ret; for init, treat all as actual params
    pieces = []
    for i, lbl in enumerate(labels):
        t = params[i] if i < len(params) else '?'
        pieces.append(f"_: {t}" if lbl == '_' else f"{lbl}: {t}")
    params_str = ', '.join(pieces)

    if is_init:
        return f"{indent}init({params_str})"
    ret_part = '' if ret in ('()', 'Void') else f" -> {ret}"
    return f"{indent}func {backtick(name)}({params_str}){ret_part}"

def emit_var(node, indent: str = '', top_level: bool = False) -> str:
    name = backtick(node.get('printedName', node['name']))
    decl = node.get('declKind', 'Var')
    if decl == 'EnumElement':
        return f"{indent}case {name}"
    types = type_children(node)
    t = strip_module(types[0].get('printedName', '?')) if types else '?'
    keyword = 'let' if top_level else 'var'
    suffix = '' if top_level else ' { get }'
    return f"{indent}{keyword} {name}: {t}{suffix}"

def emit_typealias(node, indent: str = '') -> str:
    name = node.get('printedName', node['name'])
    types = type_children(node)
    aliased = strip_module(types[0].get('printedName', '?')) if types else '?'
    return f"{indent}typealias {name} = {aliased}"

def banner(title: str):
    print('\n' + '=' * 70)
    print(title)
    print('=' * 70)

by_kind = {}
for c in top:
    k = c.get('kind', '')
    if k == 'Import':
        continue
    if k == 'TypeAlias' and c.get('name') in SKIP_TYPEALIAS:
        continue
    by_kind.setdefault(k, []).append(c)

# ---- Types -----------------------------------------------------------------
banner(f"TYPES ({len(by_kind.get('TypeDecl', []))})")
for c in sorted(by_kind.get('TypeDecl', []), key=lambda x: x['name']):
    print(f"\n  {c['name']}")
    for m in c.get('children', []):
        mk = m.get('kind', '')
        decl = m.get('declKind', '')
        if mk == 'TypeAlias':
            # Skip the synthesised .RawValue alias inside imported enums.
            if m.get('name') == 'RawValue':
                continue
            print(emit_typealias(m, indent='      '))
        elif decl == 'Constructor' or mk == 'Constructor':
            print(emit_function(m, indent='      '))
        elif mk == 'Function':
            print(emit_function(m, indent='      '))
        elif mk == 'Var':
            print(emit_var(m, indent='      '))

# ---- Typealiases (top-level) ----------------------------------------------
banner(f"TYPEALIASES ({len(by_kind.get('TypeAlias', []))})")
for c in sorted(by_kind.get('TypeAlias', []), key=lambda x: x['name']):
    print('  ' + emit_typealias(c))

# ---- Constants ------------------------------------------------------------
banner(f"CONSTANTS ({len(by_kind.get('Var', []))})")
for c in sorted(by_kind.get('Var', []), key=lambda x: x['name']):
    print('  ' + emit_var(c, top_level=True))

# ---- Functions ------------------------------------------------------------
funcs = by_kind.get('Function', [])
banner(f"FUNCTIONS ({len(funcs)})")
for c in sorted(funcs, key=lambda x: x.get('printedName', x['name'])):
    print('  ' + emit_function(c))
PY
