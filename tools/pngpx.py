#!/usr/bin/env python3
"""Pixel reader without Pillow (8-bit RGB/RGBA PNG, not interlaced):  pngpx.py FILE x,y [x,y ...]"""
import sys, zlib, struct
def load(path):
    d = open(path, 'rb').read()
    pos, idat, w, h, bpp = 8, b'', 0, 0, 3
    while pos < len(d):
        n, t = struct.unpack('>I4s', d[pos:pos + 8]); body = d[pos + 8:pos + 8 + n]; pos += 12 + n
        if t == b'IHDR':
            w, h, depth, ct = struct.unpack('>IIBB', body[:10]); bpp = {2: 3, 6: 4}[ct]
        elif t == b'IDAT': idat += body
    raw = zlib.decompress(idat); stride = w * bpp; rows = []; prev = bytearray(stride); p = 0
    for _ in range(h):
        f = raw[p]; line = bytearray(raw[p + 1:p + 1 + stride]); p += 1 + stride
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0; b = prev[i]; c = prev[i - bpp] if i >= bpp else 0
            if f == 1: line[i] = (line[i] + a) & 255
            elif f == 2: line[i] = (line[i] + b) & 255
            elif f == 3: line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if pa <= pb and pa <= pc else b if pb <= pc else c
                line[i] = (line[i] + pr) & 255
        rows.append(line); prev = line
    return w, h, bpp, rows
if __name__ == '__main__':
    w, h, bpp, rows = load(sys.argv[1])
    for a in sys.argv[2:]:
        x, y = map(int, a.split(',')); px = rows[y][x * bpp:x * bpp + 3]
        print(a, '#%02x%02x%02x' % tuple(px))
