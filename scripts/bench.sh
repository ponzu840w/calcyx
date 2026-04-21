#!/usr/bin/env bash
# bench.sh — 単一の calcyx ビルド成果物を計測
#
# 使い方:
#   scripts/bench.sh --build-dir build     --platform linux --label after
#   scripts/bench.sh --build-dir build-win --platform win   --label after
#
# 出力 (key=value 形式):
#   label=after
#   platform=linux
#   binary_bytes=2811664
#   text=2170137  data=65400  bss=27104
#   startup_s=0.12
#   peak_rss_kb=45600

set -euo pipefail

BUILD_DIR=""
PLATFORM=""
LABEL=""
EXIT_MS=200
TIMEOUT_MS=5000
REPS=3

usage() {
    echo "Usage: $0 --build-dir <dir> --platform linux|win [--label <name>] [--exit-ms <N>] [--timeout-ms <N>] [--reps <N>]"
    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir)  BUILD_DIR="$2"; shift 2 ;;
        --platform)   PLATFORM="$2";  shift 2 ;;
        --label)      LABEL="$2";     shift 2 ;;
        --exit-ms)    EXIT_MS="$2";   shift 2 ;;
        --timeout-ms) TIMEOUT_MS="$2";shift 2 ;;
        --reps)       REPS="$2";      shift 2 ;;
        -h|--help)    usage ;;
        *)            echo "Unknown arg: $1" >&2; usage ;;
    esac
done

[ -z "$BUILD_DIR" ] && usage
[ -z "$PLATFORM" ]  && usage

case "$PLATFORM" in
    linux) EXE_NAME="calcyx-gui"     ;;
    win)   EXE_NAME="calcyx-gui.exe" ;;
    *) echo "Unknown platform: $PLATFORM" >&2; exit 1 ;;
esac

EXE_PATH="$BUILD_DIR/ui/$EXE_NAME"
[ -f "$EXE_PATH" ] || { echo "Binary not found: $EXE_PATH" >&2; exit 1; }

# --- 3 つのサンプルから中央値を取るユーティリティ ---
median() {
    # 数値リストを sort して中央値を返す。欠損 ("-") は除外。
    local nums=()
    for v in "$@"; do [ "$v" != "-" ] && nums+=("$v"); done
    [ ${#nums[@]} -eq 0 ] && { echo "-"; return; }
    printf '%s\n' "${nums[@]}" | sort -g | awk -v n=${#nums[@]} 'NR==int(n/2)+1 { print; exit }'
}

# --- サイズ計測 ---
BIN_BYTES=$(stat -c '%s' "$EXE_PATH")

TEXT="-"; DATA="-"; BSS="-"
if [ "$PLATFORM" = "linux" ]; then
    if command -v size >/dev/null 2>&1; then
        read -r TEXT DATA BSS _ < <(size "$EXE_PATH" | awk 'NR==2 { print $1, $2, $3 }')
    fi
else
    # Windows exe は MinGW の size を使う (PE フォーマット対応)
    for SIZETOOL in x86_64-w64-mingw32-size size; do
        if command -v "$SIZETOOL" >/dev/null 2>&1; then
            if read -r TEXT DATA BSS _ < <("$SIZETOOL" "$EXE_PATH" 2>/dev/null | awk 'NR==2 { print $1, $2, $3 }'); then
                break
            fi
        fi
    done
fi

# --- 起動時間 + ピーク RSS の計測 ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PS1_PATH="$SCRIPT_DIR/bench_win.ps1"

run_once() {
    # 1 回分の計測を行い、"<elapsed_s> <peak_rss_kb>" を echo する。
    # タイムアウトや未対応リビジョンの場合は elapsed_s を "-" にする。
    if [ "$PLATFORM" = "linux" ]; then
        # /proc/PID/status の VmHWM をポーリングしてピーク RSS を取得する。
        # これなら hook 未対応のリビジョン (SIGTERM で殺す場合) でも RSS を
        # 測定できる。hook があれば通常終了し elapsed_s も取れる。
        local pid peak=0 sampled=0 start_ms now_ms elapsed_ms status rss
        CALCYX_BENCH_EXIT_MS="$EXIT_MS" "$EXE_PATH" >/dev/null 2>&1 &
        pid=$!
        start_ms=$(($(date +%s%N)/1000000))
        while kill -0 "$pid" 2>/dev/null; do
            rss=$(awk '/^VmHWM:/ { print $2 }' /proc/"$pid"/status 2>/dev/null || true)
            if [ -n "$rss" ] && [ "$rss" -gt "$peak" ] 2>/dev/null; then
                peak=$rss; sampled=1
            fi
            now_ms=$(($(date +%s%N)/1000000))
            elapsed_ms=$((now_ms - start_ms))
            if [ "$elapsed_ms" -ge "$TIMEOUT_MS" ]; then
                kill -TERM "$pid" 2>/dev/null || true
                sleep 0.2
                kill -KILL "$pid" 2>/dev/null || true
                wait "$pid" 2>/dev/null || true
                [ "$sampled" -eq 0 ] && peak="-"
                echo "- $peak"
                return
            fi
            sleep 0.02
        done
        wait "$pid" 2>/dev/null || true
        status=$?
        now_ms=$(($(date +%s%N)/1000000))
        elapsed_ms=$((now_ms - start_ms))
        [ "$sampled" -eq 0 ] && peak="-"
        if [ "$status" -eq 0 ]; then
            printf '%.3f %s\n' "$(awk -v m="$elapsed_ms" 'BEGIN { print m/1000.0 }')" "$peak"
        else
            echo "- $peak"
        fi
    else
        # Windows ネイティブ実行
        if ! command -v powershell.exe >/dev/null 2>&1; then
            echo "- -"; return
        fi
        local win_path output
        win_path=$(wslpath -w "$EXE_PATH")
        output=$(powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(wslpath -w "$PS1_PATH")" \
                   -Exe "$win_path" -ExitMs "$EXIT_MS" -TimeoutMs "$TIMEOUT_MS" 2>/dev/null | tr -d '\r')
        local e=$(echo "$output" | awk -F= '/^elapsed_s=/ { print $2 }')
        local r=$(echo "$output" | awk -F= '/^peak_rss_kb=/ { print $2 }')
        echo "${e:--} ${r:--}"
    fi
}

ELAPSED_LIST=()
RSS_LIST=()
for _ in $(seq 1 "$REPS"); do
    read -r e r < <(run_once)
    ELAPSED_LIST+=("$e")
    RSS_LIST+=("$r")
done

STARTUP_S=$(median "${ELAPSED_LIST[@]}")
PEAK_RSS_KB=$(median "${RSS_LIST[@]}")

# --- 出力 ---
echo "label=${LABEL}"
echo "platform=${PLATFORM}"
echo "binary_bytes=${BIN_BYTES}"
echo "text=${TEXT}  data=${DATA}  bss=${BSS}"
echo "startup_s=${STARTUP_S}"
echo "peak_rss_kb=${PEAK_RSS_KB}"
