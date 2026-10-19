#!/bin/bash
# An isolated X server for testing win11terminal, on a display of our own
# (default :11) and with our own pid files: never touches other projects' tests.
#   xephyr.sh start [:N] [WxH]   Xephyr, no window manager
#   xephyr.sh stop [:N]
#   xephyr.sh shot [:N] [out.png]
set -e
cmd=${1:-start}
D=${2:-:11}
N=${D#:}
cd "$(dirname "$0")/.."
RUN=$PWD/captures/run
mkdir -p "$RUN"
case "$cmd" in
start)
    S=${3:-1920x1080}
    if DISPLAY=$D xdpyinfo >/dev/null 2>&1; then
        echo "$D já existe (de outro projeto?): escolha outro número" >&2
        exit 1
    fi
    # A /tmp of its own: the real one has a per-user quota that can run out.
    DISPLAY=:0 unshare -Urm sh -c "mount -t tmpfs tmpfs /tmp && exec Xephyr $D -screen $S -no-host-grab -title 'win11terminal teste $D'" \
        >"$RUN/xephyr$N.log" 2>&1 &
    echo $! >"$RUN/xephyr$N.pid"
    for _ in $(seq 50); do DISPLAY=$D xdpyinfo >/dev/null 2>&1 && break; sleep 0.1; done
    DISPLAY=$D xdpyinfo >/dev/null 2>&1 && echo "Xephyr $D ($S) ok"
    ;;
stop)
    [ -f "$RUN/xephyr$N.pid" ] && kill "$(cat "$RUN/xephyr$N.pid")" 2>/dev/null || true
    rm -f "$RUN/xephyr$N.pid"
    ;;
shot)
    out=${3:-captures/x.png}
    mkdir -p "$(dirname "$out")"
    ffmpeg -loglevel error -y -f x11grab -draw_mouse 0 -i "$D" -frames:v 1 "$out"
    echo "$out"
    ;;
esac
