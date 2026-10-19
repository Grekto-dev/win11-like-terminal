#!/bin/bash
# Visual smoke run for win11terminal in the Xephyr :11 (tools/xephyr.sh start :11 1600x900):
# the same keystrokes and clicks against any binary, one PNG per step, so two builds can be diffed.
#   smoke_shots.sh BINARY OUTDIR
# Steady cursor (DECSCUSR 2) so the blink does not make the shots differ.
set -e
BIN=$(readlink -f "$1"); OUT=$2; export DISPLAY=${DISPLAY:-:11}
cd "$(dirname "$0")/.."; mkdir -p "$OUT" captures/t126/cwd
shot() { tools/xephyr.sh shot :11 "$OUT/$1.png" >/dev/null; }
WIN11TERMINAL_SETTINGS=$PWD/captures/t126/settings.json "$BIN" -d "$PWD/captures/t126/cwd" -e env PS1='$ ' bash --norc >"$OUT/app.log" 2>&1 &
PID=$!
sleep 3
W=$(xdotool search --class win11terminal | head -1); xdotool windowfocus "$W"; xdotool mousemove 900 450
type_() { xdotool type --delay 12 "$1"; xdotool key Return; sleep 1; }
type_ "printf '\\e[2 q'; clear; bash $PWD/tools/cores.sh"
shot s1_colors
xdotool key ctrl+shift+t; sleep 1.5; type_ "printf '\\e[2 q'; echo second tab; seq 1 40"
shot s2_newtab
xdotool key alt+shift+d; sleep 1.5; type_ "printf '\\e[2 q'; echo pane two"
shot s3_panes
xdotool key ctrl+comma; sleep 2
xdotool mousemove 100 546; sleep 0.3; xdotool click 1; sleep 1; shot s4_profile
xdotool mousemove 700 350; for i in $(seq 15); do xdotool click 5; done; sleep 1
xdotool mousemove 700 302; sleep 0.3; xdotool click 1; sleep 1.5; shot s5_appearance_top
xdotool mousemove 700 350; for i in 1 2 3 4 5 6; do xdotool click 5; done; sleep 1; shot s6_appearance_mid
for i in $(seq 30); do xdotool click 5; done; sleep 1; shot s7_appearance_end
xdotool mousemove 100 546; sleep 0.3; xdotool click 1; sleep 1
xdotool mousemove 700 350; for i in $(seq 15); do xdotool click 5; done; sleep 1
xdotool mousemove 700 438; sleep 0.3; xdotool click 1; sleep 1.5; shot s8_advanced_top
xdotool mousemove 700 350; for i in $(seq 30); do xdotool click 5; done; sleep 1; shot s9_advanced_end
kill "$PID" 2>/dev/null || true
wait "$PID" 2>/dev/null || true
echo "shots in $OUT"
