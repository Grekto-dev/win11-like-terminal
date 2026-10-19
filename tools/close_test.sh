#!/bin/bash
# Closing a pane, a tab or a window ends its program and leaves no zombie; "New window" stays in one process.
#   close_test.sh BINARY OUTDIR   (needs the Xephyr :11)
set -e
BIN=$(readlink -f "$1"); OUT=$2; export DISPLAY=${DISPLAY:-:11}
cd "$(dirname "$0")/.."; mkdir -p "$OUT" captures/t126/cwd
WIN11TERMINAL_SETTINGS=$PWD/captures/t126/settings.json "$BIN" -d "$PWD/captures/t126/cwd" -e env PS1='$ ' bash --norc >"$OUT/app.log" 2>&1 &
PID=$!; sleep 3
nwin() { xdotool search --class win11terminal 2>/dev/null | wc -l; }
alive() { ps -eo pid,ppid,stat,args | awk -v m="$1" '$0 ~ m && $0 !~ /awk/ {print}' ; }
zombies() { ps -eo ppid,stat | awk -v p=$PID '$1==p && $2 ~ /^Z/' | wc -l; }
exes() { ps -eo args | grep -c -F -- "$BIN" || true; }
focus_last() { local w; w=$(xdotool search --class win11terminal | tail -1); xdotool windowfocus "$w"; }
W1=$(xdotool search --class win11terminal | head -1); xdotool windowfocus $W1; xdotool mousemove 900 450
t() { xdotool type --delay 12 "$1"; xdotool key Return; sleep 1; }
echo "windows: $(nwin)  processes of the binary: $(exes)  children of the app: $(pgrep -P $PID | wc -l)"
echo "== tab: sleep in a second tab, then close the tab"
xdotool key ctrl+shift+t; sleep 1.5; t "exec sleep 7771"
echo "sleep 7771 before: $(alive 'sleep 7771' | wc -l)"
xdotool key ctrl+shift+w; sleep 2
echo "sleep 7771 after closing the tab: $(alive 'sleep 7771' | wc -l); zombies: $(zombies)"
echo "== pane: sleep in a split pane, then close the pane"
xdotool key alt+shift+d; sleep 1.5; t "exec sleep 7772"
echo "sleep 7772 before: $(alive 'sleep 7772' | wc -l)"
xdotool key ctrl+shift+w; sleep 2
echo "sleep 7772 after closing the pane: $(alive 'sleep 7772' | wc -l); zombies: $(zombies); app alive: $(kill -0 $PID 2>/dev/null && echo yes || echo NO)"
echo "== new window in the same process (Ctrl+Shift+N)"
xdotool key ctrl+shift+n; sleep 2.5
echo "windows: $(nwin) (was 1); processes of the binary: $(exes) (was 1)"
focus_last; sleep 0.3; t "exec sleep 7773"
echo "sleep 7773 in the new window: $(alive 'sleep 7773' | wc -l)"
xdotool key ctrl+shift+w; sleep 2.5
echo "after closing that window: windows $(nwin); sleep 7773: $(alive 'sleep 7773' | wc -l); zombies: $(zombies); app alive: $(kill -0 $PID 2>/dev/null && echo yes || echo NO)"
echo "== closing the last window ends the app"
focus_last; xdotool key ctrl+shift+w; sleep 2.5
echo "app alive: $(kill -0 $PID 2>/dev/null && echo YES || echo no); children left: $(pgrep -P $PID | wc -l)"
kill $PID 2>/dev/null || true; wait $PID 2>/dev/null || true
