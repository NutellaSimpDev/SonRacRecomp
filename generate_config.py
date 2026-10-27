import re

with open('all_binary_targets.txt') as f:
    targets = [int(line.strip(), 16) for line in f]

xidata_valid = set(t for t in targets if 0x823B0000 <= t <= 0x824ECE73)

with open('sonicracingtransformed_config.toml') as f:
    cfg = f.read()

existing_addrs = set(int(m, 16) for m in re.findall(r'0x[0-9a-fA-F]{8}', cfg))
existing_valid = set(a for a in existing_addrs if a >= 0x823B0000)

all_combined = sorted(existing_valid.union(xidata_valid))

print(f"Total combined valid function entry points: {len(all_combined)}")

header = """# sonicracingtransformed - ReXGlue Codegen Configuration
project_name = "sonicracingtransformed"
file_path = "assets/default.xex"
out_directory_path = "generated"

[functions]
"""

with open('sonicracingtransformed_config.toml', 'w') as f:
    f.write(header)
    for addr in all_combined:
        f.write(f"0x{addr:08x} = {{ name = \"sub_{addr:08x}\" }}\n")

print("Successfully written sonicracingtransformed_config.toml!")
