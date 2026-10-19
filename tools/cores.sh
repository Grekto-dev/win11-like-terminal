#!/bin/bash
# The same picture as the script run on the Windows Terminal VM (refs/19): ANSI
# colours, styles, the 256 ramp, a truecolor gradient, box drawing and wide text.
e=$'\e'
echo "${e}[0mANSI 16 (normal / bright):"
for base in 30 90; do l=''; for i in 0 1 2 3 4 5 6 7; do l+="${e}[$((base+i))m XX ${e}[0m"; done; echo "$l"; done
for base in 40 100; do l=''; for i in 0 1 2 3 4 5 6 7; do l+="${e}[$((base+i))m    ${e}[0m"; done; echo "$l"; done
echo "${e}[1mbold${e}[0m ${e}[2mfaint${e}[0m ${e}[3mitalic${e}[0m ${e}[4munderline${e}[0m ${e}[7mreverse${e}[0m ${e}[9mstrike${e}[0m ${e}[53moverline${e}[0m ${e}[4:3mcurly${e}[0m"
l=''; for i in $(seq 16 51); do l+="${e}[48;5;${i}m ${e}[0m"; done; echo "$l"
l=''; for i in $(seq 232 255); do l+="${e}[48;5;${i}m ${e}[0m"; done; echo "$l"
l=''; for i in $(seq 0 47); do v=$((i*255/47)); l+="${e}[48;2;${v};0;$((255-v))m ${e}[0m"; done; echo "$l"
echo "box: ┌─┬─┐ │ │ │ ├─┼─┤ └─┴─┘ ░▒▓█ ▀▄ ●○ →"
echo "wide: 日本語 ✓ ✗ 😀 é ã ç"
echo "cursor test line"
