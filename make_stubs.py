import re
import glob

with open('generated/sonicracingtransformed_init.cpp', 'r', encoding='utf-8') as f:
    init_content = f.read()

mapped_funcs = set(re.findall(r'\{\s*0x[0-9a-fA-F]+,\s*([a-zA-Z0-9_]+)\s*\}', init_content))
defined_funcs = set()
for cpp in glob.glob('generated/sonicracingtransformed_recomp.*.cpp'):
    with open(cpp, 'r', encoding='utf-8') as f:
        defined_funcs.update(re.findall(r'DEFINE_REX_FUNC\(([a-zA-Z0-9_]+)\)', f.read()))

missing = sorted(mapped_funcs - defined_funcs)
print(f"Writing {len(missing)} stubs to src/missing_stubs.cpp...")

with open('src/missing_stubs.cpp', 'w', encoding='utf-8') as f:
    f.write('// Auto-generated stubs for missing import thunks\n')
    f.write('#include "generated/sonicracingtransformed_init.h"\n\n')
    for fn in missing:
        f.write(f'DEFINE_REX_FUNC({fn}) {{\n')
        f.write('    REX_FUNC_PROLOGUE();\n')
        f.write('}\n\n')

print("Done!")
