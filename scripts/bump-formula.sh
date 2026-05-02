#!/usr/bin/env bash
# scripts/bump-formula.sh — Formula と Cask を新タグに合わせて生成する。
#
# 使い方:
#   scripts/bump-formula.sh v1.0.1   # 明示
#   scripts/bump-formula.sh          # git describe --abbrev=0 で自動取得
#
# 動作:
#   1. タグ名から source tarball URL と GitHub Release dmg URL を組み立て
#   2. それぞれを curl + shasum で sha256 計算
#   3. main repo の HomebrewFormula/calcyx.rb (Formula テンプレート) と
#      HomebrewFormula/Casks/calcyx.rb (Cask テンプレート) を読み込み、
#      placeholder を実値に置換した結果を tap repo の Formula/calcyx.rb /
#      Casks/calcyx.rb に書き出す。 main repo は変更しない。
#   4. tap repo の diff を表示し、 git add + commit + push まで自動実行
#
# 前提:
#   - タグが GitHub に push 済みであること (= source tarball が 404 にならない)
#   - 該当リリースに `calcyx-mac-<version>.dmg` がアップロード済みである
#     こと (= Cask sha256 計算に必要)。
#       gh release upload <tag> build/calcyx-mac-<version>.dmg
#     未アップロードのときは Cask 更新だけ skip して Formula のみ更新する。
#   - $TAP_DIR (default ../homebrew-calcyx) が <user>/homebrew-calcyx を
#     clone した git repo であること
set -euo pipefail

REPO="ponzu840w/calcyx"
FORMULA_TEMPLATE="HomebrewFormula/calcyx.rb"
CASK_TEMPLATE="HomebrewFormula/Casks/calcyx.rb"
TAP_DIR="${TAP_DIR:-../homebrew-calcyx}"
TAP_FORMULA="$TAP_DIR/Formula/calcyx.rb"
TAP_CASK="$TAP_DIR/Casks/calcyx.rb"

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

# v1.0.1 → 1.0.1
VERSION="${TAG#v}"

TARBALL_URL="https://github.com/${REPO}/archive/refs/tags/${TAG}.tar.gz"
DMG_URL="https://github.com/${REPO}/releases/download/${TAG}/calcyx-mac-${VERSION}.dmg"

if [ ! -f "$FORMULA_TEMPLATE" ]; then
    echo "error: $FORMULA_TEMPLATE がありません (リポジトリ root から実行してください)" >&2
    exit 1
fi
if [ ! -f "$CASK_TEMPLATE" ]; then
    echo "error: $CASK_TEMPLATE がありません" >&2
    exit 1
fi
if [ ! -d "$TAP_DIR/.git" ]; then
    echo "error: tap repo が見つかりません: $TAP_DIR" >&2
    echo "       \$TAP_DIR で別 path を指定するか、 ../homebrew-calcyx に" >&2
    echo "       <user>/homebrew-calcyx を clone してください。" >&2
    exit 1
fi

# --- helper: URL → sha256 ---
fetch_sha256() {
    local url="$1"
    local tmp
    tmp=$(mktemp -t calcyx-asset.XXXXXX)
    if ! curl -sLf -o "$tmp" "$url"; then
        rm -f "$tmp"
        return 1
    fi
    shasum -a 256 "$tmp" | awk '{print $1}'
    rm -f "$tmp"
}

# --- Formula (CLI) ---
echo "==> fetching tarball: $TARBALL_URL"
if ! TARBALL_SHA=$(fetch_sha256 "$TARBALL_URL"); then
    echo "error: tarball の取得に失敗しました" >&2
    echo "       タグ '$TAG' を git push --tags で送ってありますか?" >&2
    exit 1
fi
if [ "${#TARBALL_SHA}" -ne 64 ]; then
    echo "error: tarball sha256 の計算に失敗しました ($TARBALL_SHA)" >&2
    exit 1
fi
echo "    tarball sha256: $TARBALL_SHA"

mkdir -p "$TAP_DIR/Formula"
sed -E \
    -e "s|(archive/refs/tags/)[^\"]+(\\.tar\\.gz)|\\1${TAG}\\2|" \
    -e "s|(sha256 \")[0-9a-f]{64}(\")|\\1${TARBALL_SHA}\\2|" \
    "$FORMULA_TEMPLATE" > "$TAP_FORMULA"

# --- Cask (GUI) ---
# dmg は GitHub Release への手動 upload が前提。 未アップロードなら 404 で
# 落ちるので、 そのときは Cask 更新だけ skip する (= Formula のみ反映)。
echo
echo "==> fetching dmg: $DMG_URL"
CASK_UPDATED=0
if DMG_SHA=$(fetch_sha256 "$DMG_URL") && [ "${#DMG_SHA}" -eq 64 ]; then
    echo "    dmg sha256:     $DMG_SHA"
    mkdir -p "$TAP_DIR/Casks"
    sed -E \
        -e "s|(version \")[^\"]+(\")|\\1${VERSION}\\2|" \
        -e "s|(sha256 \")[0-9a-f]{64}(\")|\\1${DMG_SHA}\\2|" \
        "$CASK_TEMPLATE" > "$TAP_CASK"
    CASK_UPDATED=1
else
    echo "    (skip cask: dmg が GitHub Release にアップロードされていません)"
    echo "    後で 'gh release upload $TAG build/calcyx-mac-${VERSION}.dmg' して"
    echo "    再度このスクリプトを叩くと Cask だけ追従更新できます。"
fi

# --- tap repo に commit + push ---
if (cd "$TAP_DIR" && git diff --quiet --no-color -- Formula Casks 2>/dev/null); then
    echo
    echo "==> tap repo: no change (already up to date for $TAG)"
    exit 0
fi

echo
echo "==> diff (tap repo):"
(cd "$TAP_DIR" && git --no-pager diff --no-color -- Formula Casks)

# commit メッセージ
if [ "$CASK_UPDATED" -eq 1 ]; then
    MSG="calcyx: bump formula and cask to $TAG"
else
    MSG="calcyx: bump formula to $TAG (cask pending dmg upload)"
fi

echo
echo "==> committing & pushing tap repo..."
(
    cd "$TAP_DIR"
    git add Formula/calcyx.rb
    [ "$CASK_UPDATED" -eq 1 ] && git add Casks/calcyx.rb
    git commit -m "$MSG"
    git push
)
echo
echo "==> done."
echo "    brew update && brew upgrade calcyx                   # CLI 既存ユーザ"
echo "    brew tap ${REPO%/*}/calcyx                           # 新規ユーザ"
echo "    brew install calcyx                                   # CLI/TUI"
echo "    brew install --cask calcyx                            # GUI (.app)"
