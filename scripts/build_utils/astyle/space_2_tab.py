#!/usr/bin/env python3

import os

def replace_spaces_with_tabs(line, spaces_per_tab=4):
    return line.replace(' ' * spaces_per_tab, '\t')

def process_file(filepath, spaces_per_tab=4):
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            lines = f.readlines()

        converted = [replace_spaces_with_tabs(line, spaces_per_tab) for line in lines]

        with open(filepath, 'w', encoding='utf-8') as f:
            f.writelines(converted)

        print(f"Converted: {filepath}")
    except Exception as e:
        print(f"Failed to process {filepath}: {e}")

def recursively_convert(dir_path='.', spaces_per_tab=4, exts=None):
    for root, dirs, files in os.walk(dir_path):
        for file in files:
            if exts is None or file.endswith(exts):
                filepath = os.path.join(root, file)
                process_file(filepath, spaces_per_tab)

if __name__ == '__main__':
    # 示例：仅处理特定扩展名的文件，设为 None 则处理所有文件
    file_extensions = ('.c', '.h', '.cpp', '.S')  # 可改为 None
    recursively_convert(exts=file_extensions)

