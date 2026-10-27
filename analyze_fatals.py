import os
import re

gen_dir = r'C:\Users\EduPedu\Desktop\Decompilar\SonicRacingTransformed_Project\generated'
cpp_files = [os.path.join(gen_dir, f) for f in os.listdir(gen_dir) if f.endswith('.cpp')]

fatal_pattern = re.compile(r'REX_FATAL\("Unresolved branch from (0x[0-9a-fA-F]+) to (0x[0-9a-fA-F]+)"\);')
func_pattern = re.compile(r'DEFINE_REX_FUNC\((sub_[0-9a-fA-F]+)\)\s*\{')

# Load all known function names
all_sub_funcs = set()
with open(os.path.join(gen_dir, 'sonicracingtransformed_init.h'), 'r') as f:
    for line in f:
        m = re.search(r'(sub_[0-9a-fA-F]+)', line)
        if m:
            all_sub_funcs.add(m.group(1).lower())

same_func_count = 0
other_func_global_count = 0
unmatched_count = 0
unmatched_targets = []

for filepath in cpp_files:
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
    
    matches = list(func_pattern.finditer(content))
    for i in range(len(matches)):
        start = matches[i].end()
        end = matches[i+1].start() if i + 1 < len(matches) else len(content)
        body = content[start:end]
        
        fatals = fatal_pattern.findall(body)
        for src_hex, target_hex in fatals:
            lbl = 'loc_' + target_hex[2:].upper() + ':'
            if lbl in body:
                same_func_count += 1
            else:
                fn = 'sub_' + target_hex[2:].lower()
                if fn in all_sub_funcs:
                    other_func_global_count += 1
                else:
                    unmatched_count += 1
                    if len(unmatched_targets) < 20:
                        unmatched_targets.append((src_hex, target_hex, os.path.basename(filepath)))

print(f'Total FATAL branches: {same_func_count + other_func_global_count + unmatched_count}')
print(f'  Target label in SAME function: {same_func_count}')
print(f'  Target function exists globally: {other_func_global_count}')
print(f'  Truly unmatched: {unmatched_count}')
print('Sample truly unmatched:')
for src, tgt, fn in unmatched_targets:
    print(f'  from {src} to {tgt} in {fn}')
