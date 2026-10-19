#!/usr/bin/env python3
"""Pixel measuring for captures (the VM's and ours). Needs Pillow.

  px.py crop IN OUT x0 y0 x1 y1 [zoom]    cut a region, blown up without smoothing
  px.py at IN x y [x y ...]               RGB of single pixels
  px.py row IN y x0 x1 [minlen]           runs of equal colour along a row
  px.py col IN x y0 y1 [minlen]           runs of equal colour down a column
  px.py box IN x0 y0 x1 y1 r,g,b [tol]    bounding box of pixels near a colour

Runs shorter than `minlen` (default 4) are dropped, so text stays out of the way.
Runs print as "start-end (length) #rrggbb".
"""
import sys
from PIL import Image


def load(p):
    return Image.open(p).convert('RGB')


def runs(px, minlen):
    out, s = [], 0
    for i in range(1, len(px) + 1):
        if i == len(px) or px[i] != px[s]:
            if i - s >= minlen:
                out.append((s, i - 1, px[s]))
            s = i
    return out


def hexc(c):
    return '#%02x%02x%02x' % c


def main(a):
    cmd = a[0]
    if cmd == 'crop':
        im = load(a[1]).crop(tuple(int(v) for v in a[3:7]))
        z = int(a[7]) if len(a) > 7 else 1
        im.resize((im.width * z, im.height * z), Image.NEAREST).save(a[2])
    elif cmd == 'at':
        im = load(a[1])
        v = a[2:]
        for i in range(0, len(v), 2):
            x, y = int(v[i]), int(v[i + 1])
            print(x, y, hexc(im.getpixel((x, y))))
    elif cmd in ('row', 'col'):
        im = load(a[1])
        k, lo, hi = int(a[2]), int(a[3]), int(a[4])
        m = int(a[5]) if len(a) > 5 else 4
        px = [im.getpixel((i, k) if cmd == 'row' else (k, i)) for i in range(lo, hi + 1)]
        for s, e, c in runs(px, m):
            print('%d-%d (%d) %s' % (lo + s, lo + e, e - s + 1, hexc(c)))
    elif cmd == 'box':
        im = load(a[1])
        x0, y0, x1, y1 = (int(v) for v in a[2:6])
        t = tuple(int(v) for v in a[6].split(','))
        tol = int(a[7]) if len(a) > 7 else 6
        xs, ys = [], []
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                if max(abs(p - q) for p, q in zip(im.getpixel((x, y)), t)) <= tol:
                    xs.append(x); ys.append(y)
        print('none' if not xs else 'x %d-%d  y %d-%d  n=%d' % (min(xs), max(xs), min(ys), max(ys), len(xs)))


main(sys.argv[1:])
