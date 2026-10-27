import os
import re

gen_dir = r'C:\Users\EduPedu\Desktop\Decompilar\SonicRacingTransformed_Project\generated'
cpp_files = [os.path.join(gen_dir, f) for f in os.listdir(gen_dir) if f.endswith('.cpp')]

call_errors = []
call_pattern = re.compile(r'REX_FATAL\("Unresolved call from (0x[0-9a-fA-F]+) to (0x[0-9a-fA-F]+)"\);')

for filepath in cpp_files:
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    matches = call_pattern.findall(content)
    if matches:
        for src, tgt in matches:
            call_errors.append((os.path.basename(filepath), src, tgt))

print(f"Total unresolved call errors: {len(call_errors)}")
for f, src, tgt in call_errors[:20]:
    print(f"  {f}: from {src} to {tgt}")
