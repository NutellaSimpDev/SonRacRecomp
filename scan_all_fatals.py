import os
import re

gen_dir = r'C:\Users\EduPedu\Desktop\Decompilar\SonicRacingTransformed_Project\generated'
cpp_files = [os.path.join(gen_dir, f) for f in os.listdir(gen_dir) if f.endswith('.cpp')]

call_pattern = re.compile(r'REX_FATAL\("Unresolved call from (0x[0-9a-fA-F]+) to (0x[0-9a-fA-F]+)"\);')
jump_pattern = re.compile(r'REX_FATAL\("(Jump target [^"]+)"\);')

calls = []
jumps = []

for filepath in cpp_files:
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    for m in call_pattern.finditer(content):
        calls.append((os.path.basename(filepath), m.group(1), m.group(2)))

    for m in jump_pattern.finditer(content):
        jumps.append((os.path.basename(filepath), m.group(1)))

print(f"Total unresolved calls: {len(calls)}")
for f, src, tgt in calls[:5]:
    print(f"  {f}: from {src} to {tgt}")

print(f"\nTotal jump target errors: {len(jumps)}")
for f, msg in jumps[:10]:
    print(f"  {f}: {msg}")
