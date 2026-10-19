#!/bin/bash
# Cursor checks in Xephyr :11: blinking, shapes, resize, font zoom (which drops the atlas pages
# while the cursor sits on a blank cell), tabs opened and closed. Counts pure-white pixels
# (the cursor colour; the text is #CCCCCC) in the first terminal row, so no image reading is needed.
#   cursor_test.sh BINARY OUTDIR
set -e
BIN=$(readlink -f "$1"); OUT=$2; export DISPLAY=${DISPLAY:-:11}
cd "$(dirname "$0")/.."; mkdir -p "$OUT" captures/t126/cwd
count() { # PNG -> "n (x0,x1,y0,y1)" of white pixels in the first rows (x < 200)
python3 - "$1" <<'PY'
import sys; sys.path.insert(0,'tools')
from pngpx import load
w,h,b,rows=load(sys.argv[1]); n=0; xs=[]; ys=[]
for y in range(41,75):
    r=rows[y]
    for x in range(0,min(w,200)):
        if r[x*b:x*b+3]==b'\xff\xff\xff': n+=1; xs.append(x); ys.append(y)
print(n, (min(xs),max(xs),min(ys),max(ys)) if n else '-')
PY
}
shot() { tools/xephyr.sh shot :11 "$OUT/$1.png" >/dev/null; }
QSG_RHI_DEBUG_LAYER=1 QT_LOGGING_RULES="qt.rhi.*=true;qt.scenegraph.*=true;*.warning=true" \
WIN11TERMINAL_SETTINGS=$PWD/captures/t126/settings.json "$BIN" -d "$PWD/captures/t126/cwd" -e env PS1='$ ' bash --norc >"$OUT/app.log" 2>&1 &
PID=$!
sleep 3
W=$(xdotool search --class win11terminal | head -1); xdotool windowfocus "$W"; xdotool mousemove 900 450
type_() { xdotool type --delay 12 "$1"; xdotool key Return; sleep 0.7; }
type_ "printf '\\e[1 q'; clear"
echo "== blink (block, 16 shots at irregular intervals): white pixels per shot"
for i in $(seq 16); do shot b$i; echo -n "$(count $OUT/b$i.png | cut -d' ' -f1) "; sleep 0.$((RANDOM % 9 + 1)); done; echo
for n in 2 4 6 3 5 1; do
  type_ "printf '\\e[$n q'; clear"; sleep 0.2
  if [ $((n % 2)) = 1 ]; then # blinking: keep the best of 6 shots
    best="0 -"; for i in 1 2 3 4 5 6; do shot sh$n; c=$(count $OUT/sh$n.png); [ "${c%% *}" -gt "${best%% *}" ] && best=$c; sleep 0.13; done
  else shot sh$n; best=$(count $OUT/sh$n.png); fi
  echo "== shape DECSCUSR $n: $best"
done
type_ "printf '\\e[2 q'; clear"
echo "== steady block before resize: $(shot r0; count $OUT/r0.png)"
for g in "900 500" "1400 800" "700 420" "1115 628"; do
  xdotool windowsize "$W" $g; sleep 1; type_ "clear"; shot r_${g// /x}
  echo "== resize $g: $(count $OUT/r_${g// /x}.png)"
done
type_ "clear"
for k in 4 5 4 5 5 4 4 5; do # Ctrl+wheel: font size steps, atlas rebuild with the cursor on a blank cell
  xdotool keydown ctrl; xdotool click $k; xdotool keyup ctrl; sleep 0.4
done
sleep 0.5; shot zoom; echo "== after zoom steps: $(count $OUT/zoom.png)"
for i in 1 2 3 4 5 6; do xdotool key ctrl+shift+t; sleep 0.6; type_ "printf '\\e[2 q'; clear"; done
shot tabs_open; echo "== 7 tabs: $(count $OUT/tabs_open.png)"
for i in 1 2 3 4 5 6; do xdotool key ctrl+shift+w; sleep 0.5; done
shot tabs_closed; echo "== back to 1 tab: $(count $OUT/tabs_closed.png)"
if kill -0 $PID 2>/dev/null; then echo "app alive"; else echo "APP DIED"; fi
kill $PID 2>/dev/null || true; wait $PID 2>/dev/null || true
echo "log lines: $(wc -l < $OUT/app.log); suspicious:"; grep -i -E "error|fail|invalid|null|crash|assert|segmentation|validation|dangling|deleted" "$OUT/app.log" | sort | uniq -c | sort -rn | head -10
