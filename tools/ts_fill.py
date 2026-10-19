#!/usr/bin/env python3
"""Fills translations/win11terminal_pt_BR.ts from tools/pt_BR.tsv (source <TAB> translation).

  lupdate -no-obsolete qml src -ts translations/win11terminal_pt_BR.ts
  tools/ts_fill.py

The terms are the ones Windows Terminal itself uses in pt-BR (measured on the VM or
found in its resources); a source text with no line here stays unfinished and is listed.
"""
import re, sys, os
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
tsv = {}
for line in open(os.path.join(root, 'tools/pt_BR.tsv'), encoding='utf-8'):
    line = line.rstrip('\n')
    if line and '\t' in line:
        a, b = line.split('\t', 1)
        tsv[a] = b
path = os.path.join(root, 'translations/win11terminal_pt_BR.ts')
s = open(path, encoding='utf-8').read()
missing = []
def esc(t): return t.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
def fix(m):
    src = m.group(1)
    plain = src.replace('&amp;', '&').replace('&lt;', '<').replace('&gt;', '>').replace('&quot;', '"').replace('&apos;', "'")
    if plain in tsv:
        return '<source>%s</source>\n%s<translation>%s</translation>' % (src, m.group(2), esc(tsv[plain]))
    missing.append(plain)
    return m.group(0)
s = re.sub(r'<source>([^<]*)</source>\n(\s*(?:<[^>/][^>]*>\s*)*?)<translation type="unfinished"></translation>', fix, s)
# a translation that lupdate copied from another context (same text) is done too
s = re.sub(r'<translation type="unfinished">([^<]+)</translation>', r'<translation>\1</translation>', s)
open(path, 'w', encoding='utf-8').write(s)
if missing:
    print('sem tradução:', *missing, sep='\n  ')
