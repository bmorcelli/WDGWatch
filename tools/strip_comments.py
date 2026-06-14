#!/usr/bin/env python3
"""
Kaynak kod yorum temizleyici.
src/ altındaki tüm .cpp ve .h dosyalarından // ve /* */ yorumları kaldırır.
String içindeki URL'lere dokunmaz.
"""

import os
import re
import sys

SRC_ROOT = os.path.join(os.path.dirname(__file__), "..", "src")

def strip_comments(code: str) -> str:
    result = []
    i = 0
    n = len(code)
    in_string = False
    string_char = None

    while i < n:
        c = code[i]

        # String açılış/kapanış takibi
        if not in_string:
            if c in ('"', "'"):
                in_string = True
                string_char = c
                result.append(c)
                i += 1
                continue
        else:
            # String içindeyiz — escape kontrolü
            if c == '\\':
                result.append(c)
                i += 1
                if i < n:
                    result.append(code[i])
                    i += 1
                continue
            if c == string_char:
                in_string = False
                string_char = None
            result.append(c)
            i += 1
            continue

        # Çok satırlı yorum: /* ... */
        if code[i:i+2] == '/*':
            j = code.find('*/', i + 2)
            if j == -1:
                break  # Kapanmamış yorum — dosya sonu
            # Yorum içindeki satır sonlarını koru (boş satır bırakma)
            newlines = code[i:j+2].count('\n')
            result.append('\n' * newlines)
            i = j + 2
            continue

        # Tek satırlı yorum: // ...
        if code[i:i+2] == '//':
            j = code.find('\n', i + 2)
            if j == -1:
                break  # Dosya sonu
            i = j  # '\n' karakterini bırak (bir sonraki döngüde işlenecek)
            continue

        result.append(c)
        i += 1

    return ''.join(result)


def collapse_blank_lines(text: str) -> str:
    """Arka arkaya 3+ boş satırı 2'ye indir."""
    return re.sub(r'\n{3,}', '\n\n', text)


def process_file(path: str) -> bool:
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        original = f.read()

    cleaned = strip_comments(original)
    cleaned = collapse_blank_lines(cleaned)

    if cleaned == original:
        return False

    with open(path, 'w', encoding='utf-8') as f:
        f.write(cleaned)
    return True


def main():
    extensions = ('.cpp', '.h', '.c')
    changed = []
    skipped = []

    for dirpath, dirnames, filenames in os.walk(SRC_ROOT):
        # tools/ ve .pio/ gibi dizinleri atla
        dirnames[:] = [d for d in dirnames if not d.startswith('.')]

        for fname in filenames:
            if not fname.endswith(extensions):
                continue
            fpath = os.path.join(dirpath, fname)
            rel = os.path.relpath(fpath, SRC_ROOT)
            try:
                if process_file(fpath):
                    changed.append(rel)
                    print(f"  [OK] {rel}")
                else:
                    skipped.append(rel)
            except Exception as e:
                print(f"  [ERR] {rel}: {e}", file=sys.stderr)

    print(f"\nToplam: {len(changed)} dosya temizlendi, {len(skipped)} dosya değişmedi.")

if __name__ == "__main__":
    main()
