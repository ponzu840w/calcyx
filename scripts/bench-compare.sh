#!/usr/bin/env bash
# bench-compare.sh — 2 つの git リビジョンをフルビルドして計測結果を diff
#
# 使い方:
#   scripts/bench-compare.sh                       # 既定 (369dd6c HEAD, 両プラットフォーム)
#   scripts/bench-compare.sh <before> <after>
#   scripts/bench-compare.sh --platform linux
#   scripts/bench-compare.sh --reps 5

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BEFORE="369dd6c"
AFTER="HEAD"
PLATFORM="both"
REPS=3

POSITIONAL=()
while [ $# -gt 0 ]; do
    case "$1" in
        --platform) PLATFORM="$2"; shift 2 ;;
        --reps)     REPS="$2";     shift 2 ;;
        -h|--help)
            echo "Usage: $0 [<before-ref>] [<after-ref>] [--platform linux|win|both] [--reps N]"
            exit 0 ;;
        *) POSITIONAL+=("$1"); shift ;;
    esac
done
[ ${#POSITIONAL[@]} -ge 1 ] && BEFORE="${POSITIONAL[0]}"
[ ${#POSITIONAL[@]} -ge 2 ] && AFTER="${POSITIONAL[1]}"

# AFTER=HEAD の場合は SHA に解決しておく (worktree add で HEAD が別の意味になるのを防ぐ)
AFTER_SHA=$(cd "$REPO_ROOT" && git rev-parse "$AFTER")
BEFORE_SHA=$(cd "$REPO_ROOT" && git rev-parse "$BEFORE")

# 一時ディレクトリ + worktree の cleanup
WORKTREES=()
cleanup() {
    for wt in "${WORKTREES[@]}"; do
        [ -d "$wt" ] && git -C "$REPO_ROOT" worktree remove --force "$wt" 2>/dev/null || rm -rf "$wt" || true
    done
}
trap cleanup EXIT

build_and_measure() {
    local sha="$1" label="$2"
    local wt="/tmp/calcyx-bench-${sha:0:8}"
    WORKTREES+=("$wt")

    echo "=== [$label] building $sha in $wt ===" >&2
    [ -d "$wt" ] && git -C "$REPO_ROOT" worktree remove --force "$wt" 2>/dev/null || true
    git -C "$REPO_ROOT" worktree add --detach "$wt" "$sha" >&2

    if [ "$PLATFORM" = "linux" ] || [ "$PLATFORM" = "both" ]; then
        (cd "$wt" && cmake --preset unix >/dev/null 2>&1 && cmake --build --preset unix -j) >&2
        "$SCRIPT_DIR/bench.sh" --build-dir "$wt/build" --platform linux --label "$label" --reps "$REPS"
        echo "---"
    fi
    if [ "$PLATFORM" = "win" ] || [ "$PLATFORM" = "both" ]; then
        (cd "$wt" && cmake --preset win >/dev/null 2>&1 && cmake --build --preset win -j) >&2
        "$SCRIPT_DIR/bench.sh" --build-dir "$wt/build-win" --platform win --label "$label" --reps "$REPS"
        echo "---"
    fi
}

RESULT_FILE=$(mktemp)
{
    build_and_measure "$BEFORE_SHA" "before"
    build_and_measure "$AFTER_SHA"  "after"
} > "$RESULT_FILE"

# --- 結果 parse & 表示 ---
# bench.sh の出力をレコードに集約
# レコード区切りは "---" 行
python3 - "$RESULT_FILE" "$BEFORE_SHA" "$AFTER_SHA" <<'PY'
import sys, re

path, before_sha, after_sha = sys.argv[1:]
with open(path) as f: text = f.read()
records = []
for blk in [b.strip() for b in text.split("---") if b.strip()]:
    rec = {}
    for line in blk.splitlines():
        m = re.match(r'(\w+)=(.*)', line)
        if m: rec[m.group(1)] = m.group(2).strip()
        m2 = re.match(r'text=(\S+)\s+data=(\S+)\s+bss=(\S+)', line)
        if m2:
            rec['text'], rec['data'], rec['bss'] = m2.groups()
    records.append(rec)

def idx(label, platform):
    for r in records:
        if r.get('label') == label and r.get('platform') == platform:
            return r
    return {}

def fmt_int(s):
    try: return f"{int(s):,}"
    except: return s

def delta(a, b):
    try:
        a, b = int(a), int(b)
        d = b - a
        pct = (d / a * 100) if a else 0
        sign = "+" if d >= 0 else ""
        return f"{sign}{d:,} ({sign}{pct:.1f}%)"
    except:
        return "-"

def delta_float(a, b):
    try:
        a, b = float(a), float(b)
        d = b - a
        return f"{d:+.3f}s"
    except:
        return "-"

print(f"## Comparison: {before_sha[:8]} → {after_sha[:8]}")
print()
for platform in ("linux", "win"):
    before = idx("before", platform)
    after  = idx("after",  platform)
    if not before or not after:
        continue
    print(f"### {platform}")
    print()
    print(f"| metric | before ({before_sha[:8]}) | after ({after_sha[:8]}) | Δ |")
    print(f"|---|---:|---:|---:|")
    print(f"| binary bytes | {fmt_int(before.get('binary_bytes','-'))} | {fmt_int(after.get('binary_bytes','-'))} | {delta(before.get('binary_bytes'), after.get('binary_bytes'))} |")
    print(f"| text         | {fmt_int(before.get('text','-'))} | {fmt_int(after.get('text','-'))} | {delta(before.get('text'), after.get('text'))} |")
    print(f"| data         | {fmt_int(before.get('data','-'))} | {fmt_int(after.get('data','-'))} | {delta(before.get('data'), after.get('data'))} |")
    print(f"| bss          | {fmt_int(before.get('bss','-'))} | {fmt_int(after.get('bss','-'))} | {delta(before.get('bss'), after.get('bss'))} |")
    print(f"| startup (s)  | {before.get('startup_s','-')} | {after.get('startup_s','-')} | {delta_float(before.get('startup_s'), after.get('startup_s'))} |")
    print(f"| peak RSS (KB)| {fmt_int(before.get('peak_rss_kb','-'))} | {fmt_int(after.get('peak_rss_kb','-'))} | {delta(before.get('peak_rss_kb'), after.get('peak_rss_kb'))} |")
    print()
PY

rm -f "$RESULT_FILE"
