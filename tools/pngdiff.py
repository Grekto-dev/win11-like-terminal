#!/usr/bin/env python3
"""Compare two same-size PNGs without Pillow:  pngdiff.py A B   -> differing pixels and their bounding box."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pngpx import load
wa, ha, ba, ra = load(sys.argv[1]); wb, hb, bb, rb = load(sys.argv[2])
if (wa, ha) != (wb, hb): sys.exit('size differs %dx%d vs %dx%d' % (wa, ha, wb, hb))
n = 0; x0 = y0 = 10**9; x1 = y1 = -1
for y in range(ha):
    a, b = ra[y], rb[y]
    if a == b: continue
    for x in range(wa):
        if a[x * ba:x * ba + 3] != b[x * bb:x * bb + 3]:
            n += 1; x0 = min(x0, x); x1 = max(x1, x); y0 = min(y0, y); y1 = max(y1, y)
print('%d px differ' % n + (' bbox x %d..%d y %d..%d' % (x0, x1, y0, y1) if n else ''))
