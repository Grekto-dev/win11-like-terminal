#!/usr/bin/env python3
# Test app: enables mouse reporting (1000+1006), logs wheel/clicks to a file.
import os, sys, tty, termios, select
mode = sys.argv[1]  # "mouse" or "alt"
log = open(sys.argv[2], "w", buffering=1)
fd = sys.stdin.fileno()
old = termios.tcgetattr(fd)
tty.setraw(fd)
os.write(1, b"\x1b[?1049h\x1b[2J\x1b[H")
if mode == "mouse":
    os.write(1, b"\x1b[?1000h\x1b[?1006h")
os.write(1, ("mode=%s\r\n" % mode).encode())
try:
    while True:
        r, _, _ = select.select([fd], [], [], 30)
        if not r: break
        d = os.read(fd, 100)
        log.write(repr(d) + "\n")
        if b"q" in d: break
finally:
    os.write(1, b"\x1b[?1000l\x1b[?1006l\x1b[?1049l")
    termios.tcsetattr(fd, termios.TCSADRAIN, old)
