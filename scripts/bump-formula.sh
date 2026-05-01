#!/usr/bin/env bash
# scripts/bump-formula.sh — Homebrew Formula の url と sha256 を更新する。
#
# 使い方:
#   scripts/bump-formula.sh v0.5.0-beta   # 明示
#   scripts/bump-formula.sh               # git describe --abbrev=0 で自動取得
#
# 動作:
#   1. タグ名から GitHub の source tarball URL を組み立てる
#   2. tarball を curl でフェッチして sha256 を計算
#   3. HomebrewFormula/calcyx.rb の url / sha256 行を sed で書き換える
#   4. git diff を表示する (commit / push は手動)
#
# 前提:
#   - 該当タグが GitHub に push 済みであること (= GitHub が tarball を生成
#     できる状態)。 ローカルでタグだけ切って未 push の場合は 404。
set -euo pipefail

REPO="ponzu840w/calcyx"
FORMULA="HomebrewFormula/calcyx.rb"

# --- タグ取得 ---
if [ $# -ge 1 ]; then
    TAG="$1"
else
    if ! TAG=$(git describe --tags --abbrev=0 2>/dev/null); then
        echo "error: タグを引数で指定するか git tag を切ってください" >&2
        echo "usage: $0 [<tag>]" >&2
        exit 1
    fi
fi

# 引数 or git tag が "v" で始まらないと GitHub release URL と整合しない
case "$TAG" in
    v*) ;;
    *)  echo "error: タグは 'v' で始まる必要があります (got: $TAG)" >&2; exit 1 ;;
esac

URL="https://github.com/${REPO}/archive/refs/tags/${TAG}.tar.gz"

if [ ! -f "$FORMULA" ]; then
    echo "error: $FORMULA がありません (リポジトリ root から実行してください)" >&2
    exit 1
fi

echo "==> fetching: $URL"

TMPFILE=$(mktemp -t calcyx-tarball.XXXXXX)
trap 'rm -f "$TMPFILE"' EXIT

if ! curl -sLf -o "$TMPFILE" "$URL"; then
    echo "error: tarball の取得に失敗しました" >&2
    echo "       タグ '$TAG' を git push --tags で送ってありますか?" >&2
    exit 1
fi

# sha256 計算 (shasum は macOS / Linux 両方にある coreutils 標準)
SHA=$(shasum -a 256 "$TMPFILE" | awk '{print $1}')
if [ "${#SHA}" -ne 64 ]; then
    echo "error: sha256 の計算に失敗しました ($SHA)" >&2
    exit 1
fi

echo "    tag    : $TAG"
echo "    url    : $URL"
echo "    sha256 : $SHA"

# --- formula 書き換え (url 1 行 + sha256 1 行) ---
# sed は macOS BSD と Linux GNU で挙動が違うので、 -i に "" を付けて両対応。
SED_INPLACE=(-i)
if sed --version >/dev/null 2>&1; then
    : # GNU sed: -i 単独で OK
else
    SED_INPLACE=(-i "")  # BSD sed (macOS): -i '' (空 backup suffix) が必要
fi

# url 行: archive/refs/tags/<old>.tar.gz → archive/refs/tags/$TAG.tar.gz
sed "${SED_INPLACE[@]}" -E \
    -e "s|(archive/refs/tags/)[^\"]+(\\.tar\\.gz)|\\1${TAG}\\2|" \
    -e "s|(sha256 \")[0-9a-f]{64}(\")|\\1${SHA}\\2|" \
    "$FORMULA"

echo
echo "==> diff:"
git --no-pager diff --no-color "$FORMULA" || true
echo
echo "==> 次の手順:"
echo "    git add $FORMULA"
echo "    git commit -m 'formula: bump to $TAG'"
echo "    git push"
