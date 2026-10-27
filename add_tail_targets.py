"""Add the tail-call targets rexglue reports as "not in any function" to the clean codegen config.

usage: python add_tail_targets.py <codegen log>

Only those targets are added: they lie in gaps no detected function covers, so
declaring them never splits a real function (unlike the old
sonicracingtransformed_config.toml, which forced every branch target).
"""
import re
import sys

CONFIG = 'sonicracingtransformed_clean.toml'

log = open(sys.argv[1], encoding='utf-8', errors='replace').read()
new = set(int(t, 16) for t in re.findall(r'  0x([0-9A-F]{8}) from 0x[0-9A-F]{8}: b ', log))

text = open(CONFIG, encoding='utf-8').read()
existing = set(int(a, 16) for a in re.findall(r'^0x([0-9a-f]{8}) = ', text, re.M))
added = sorted(new - existing)
with open(CONFIG, 'a', encoding='utf-8') as f:
    for a in added:
        f.write(f'0x{a:08x} = {{ name = "sub_{a:08x}" }}\n')
print(f'{len(added)} added, {len(existing) + len(added)} total')
