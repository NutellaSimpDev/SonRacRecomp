with open('sonicracingtransformed_config.toml', 'r', encoding='utf-8') as f:
    lines = f.readlines()

entry = '0x824679c0 = { name = "mainCRTStartup" }\n'
if not any('824679c0' in l for l in lines):
    for i, l in enumerate(lines):
        if l.strip() == '[functions]':
            lines.insert(i + 1, entry)
            break
    with open('sonicracingtransformed_config.toml', 'w', encoding='utf-8') as f:
        f.writelines(lines)
    print("Added 0x824679c0 to config!")
else:
    print("Already present!")
