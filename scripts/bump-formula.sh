#!/usr/bin/env bash
# scripts/bump-formula.sh — Homebrew Formula を新タグに合わせて生成する。
#
# 使い方:
#   scripts/bump-formula.sh v0.5.0-beta   # 明示
#   scripts/bump-formula.sh               # git describe --abbrev=0 で自動取得
#
# 動作:
#   1. タグ名から GitHub の source tarball URL を組み立てる
#   2. tarball を curl でフェッチして sha256 を計算
#   3. main repo の HomebrewFormula/calcyx.rb (= テンプレート) を読み込み、
#      url / sha256 行を実値に置換した結果を $TAP_DIR/Formula/calcyx.rb に
#      書き出す。 main repo のテンプレート自体は変更しない (= リリース毎に
#      main repo へコミットが入らない)。
#   4. tap repo の git diff を表示する (commit / push は手動)
#
# 前提:
#   - 該当タグが GitHub に push 済みであること (= 404 にならない)
#   - $TAP_DIR (default ../homebrew-calcyx) が <user>/homebrew-calcyx を
#     clone した git repo であること
set -euo pipefail

REPO="ponzu840w/calcyx"
TEMPLATE="HomebrewFormula/calcyx.rb"
TAP_DIR="${TAP_DIR:-../homebrew-calcyx}"
TAP_FORMULA="$TAP_DIR/Formula/calcyx.rb"

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

if [ ! -f "$TEMPLATE" ]; then
    echo "error: $TEMPLATE がありません (リポジトリ root から実行してください)" >&2
    exit 1
fi
if [ ! -d "$TAP_DIR/.git" ]; then
    echo "error: tap repo が見つかりません: $TAP_DIR" >&2
    echo "       \$TAP_DIR で別 path を指定するか、 ../homebrew-calcyx に" >&2
    echo "       <user>/homebrew-calcyx を clone してください。" >&2
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

# --- template から tap repo の rb を生成 ---
# main repo の rb は読むだけ (= テンプレート、 placeholder url/sha256 のまま)、
# 結果は tap repo の Formula/calcyx.rb に書き出す。
mkdir -p "$TAP_DIR/Formula"
sed -E \
    -e "s|(archive/refs/tags/)[^\"]+(\\.tar\\.gz)|\\1${TAG}\\2|" \
    -e "s|(sha256 \")[0-9a-f]{64}(\")|\\1${SHA}\\2|" \
    "$TEMPLATE" > "$TAP_FORMULA"

# --- tap repo に commit + push (= brew tap が新タグを参照可能になる) ---
if (cd "$TAP_DIR" && git diff --quiet --no-color -- Formula/calcyx.rb); then
    echo
    echo "==> tap repo: no change (already up to date for $TAG)"
    exit 0
fi

echo
echo "==> diff (tap repo):"
(cd "$TAP_DIR" && git --no-pager diff --no-color -- Formula/calcyx.rb)

echo
echo "==> committing & pushing tap repo..."
(
    cd "$TAP_DIR"
    git add Formula/calcyx.rb
    git commit -m "calcyx: bump to $TAG"
    git push
)
echo
echo "==> done."
echo "    brew update && brew upgrade calcyx   # 既存ユーザ"
echo "    brew tap ${REPO%/*}/calcyx && brew install calcyx   # 新規ユーザ"
