import os
import re

gen_dir = r'C:\Users\EduPedu\Desktop\Decompilar\SonicRacingTransformed_Project\generated'
cpp_files = sorted([os.path.join(gen_dir, f) for f in os.listdir(gen_dir) if f.endswith('.cpp')])

print(f"Processing {len(cpp_files)} files for all remaining fatal errors...")

func_split_pattern = re.compile(r'DEFINE_REX_FUNC\((sub_[0-9a-fA-F]+)\)\s*\{')
call_re = re.compile(r'REX_FATAL\("Unresolved call from 0x[0-9a-fA-F]+ to (0x[0-9a-fA-F]+)"\);')
jump_re = re.compile(r'REX_FATAL\("Jump target (0x[0-9a-fA-F]+) unresolved at [^"]+"\);')

def main():
    total_replaced_goto = 0
    total_replaced_indirect = 0
    total_files_modified = 0

    for filepath in cpp_files:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        if 'REX_FATAL(' not in content:
            continue

        matches = list(func_split_pattern.finditer(content))
        if not matches:
            continue

        new_content_parts = []
        last_pos = 0

        file_goto_count = 0
        file_ind_count = 0

        for i in range(len(matches)):
            m = matches[i]
            body_start = m.end()
            body_end = matches[i+1].start() if i + 1 < len(matches) else len(content)

            new_content_parts.append(content[last_pos:body_start])
            body = content[body_start:body_end]
            labels = set(re.findall(r'(loc_[0-9a-fA-F]+):', body))

            def replace_target(match):
                nonlocal file_goto_count, file_ind_count
                target_addr = match.group(1)
                target_label = 'loc_' + target_addr[2:].upper()

                if target_label in labels:
                    file_goto_count += 1
                    return f'goto {target_label};'
                else:
                    file_ind_count += 1
                    return f'{{ REX_CALL_INDIRECT_FUNC({target_addr}); return; }}'

            new_body = call_re.sub(replace_target, body)
            new_body = jump_re.sub(replace_target, new_body)

            new_content_parts.append(new_body)
            last_pos = body_end

        new_content_parts.append(content[last_pos:])
        modified_content = ''.join(new_content_parts)

        if file_goto_count > 0 or file_ind_count > 0:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(modified_content)
            total_files_modified += 1
            total_replaced_goto += file_goto_count
            total_replaced_indirect += file_ind_count

    print(f"Done! Modified {total_files_modified} files:")
    print(f"  Replaced with goto <loc>: {total_replaced_goto}")
    print(f"  Replaced with REX_CALL_INDIRECT_FUNC: {total_replaced_indirect}")

if __name__ == '__main__':
    main()
