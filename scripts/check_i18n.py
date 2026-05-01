#!/usr/bin/env python3
"""calcyx i18n 完全性チェッカ。

呼び出し: python3 scripts/check_i18n.py [SOURCE_DIR]

検証する 2 方向:

1. (missing)
   ソース内の `_("...")` 呼出のすべてに、 shared/i18n_table.c の英語キーが
   存在するか。 存在しなければビルドは失敗。 これは「日本語訳が抜けている」
   状態を防ぐためのストリクトチェック。

2. (orphan)
   shared/i18n_table.c に登録されているすべての英語キーが、 ソース内のどこか
   (= `_("...")` でなくても、 文字列リテラルとして) 出現するか。 出現しなけ
   れば「未参照の死蔵翻訳」 として警告。 動的ディスパッチ (kTabLabels[t] や
   entries[i].label など) を経由する翻訳もリテラルとしてはソースに残るので、
   この単純な検査でカバーされる。

C/C++ の隣接文字列リテラル連結 (`"foo" "bar"` → "foobar") は両方向で正規化。

将来 ja 以外の言語を足すときは i18n_table.c とは別ファイルになる予定なので、
本スクリプトは ja のみを対象にする。 他言語には任意の部分翻訳を許可する。
"""

import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TABLE_FILE = ROOT / "shared" / "i18n_table.c"

SRC_EXTS = {".cpp", ".cc", ".c", ".h", ".hpp", ".mm"}
SRC_DIRS = ["gui", "tui", "cli", "shared", "engine"]
EXCLUDE_FILES = {TABLE_FILE.resolve()}

# C/C++ 文字列リテラル ("..." with \-escapes)
STR_LIT = r'"((?:\\.|[^"\\])*)"'
# 隣接連結された連続リテラル (空白・改行を含む)
ADJ_LITS = r'(?:\s*' + STR_LIT + r')+'

# `_("..." "...")` 呼出。 macro 呼出と区別するため `\b_\(` で開始。
US_CALL = re.compile(r'\b_\(' + ADJ_LITS + r'\s*\)', re.DOTALL)

# i18n_table.c のエントリ `{ "key" "key2", "ja" "ja2" }`
TABLE_ENTRY = re.compile(
    r'\{' + ADJ_LITS + r'\s*,' + ADJ_LITS + r'\s*\}',
    re.DOTALL,
)

# 個別リテラルを再抽出するための regex
JUST_LIT = re.compile(STR_LIT, re.DOTALL)


def decode_c_escapes(s: str) -> str:
    r"""C ソース上のエスケープを実バイト列にデコード (\xHH / \n / \t / \\ 等)。

    ソース表記と i18n_table 表記の表記揺れ ("\xe2\x88\x92" vs "−") を
    吸収するため、 比較前に両側で同じ decode を掛ける。
    """
    return bytes(s, "utf-8").decode("unicode_escape").encode("latin-1").decode("utf-8", errors="replace")


def normalise_literal_group(group_text: str) -> str:
    """`"foo" "bar"` のような隣接連結文字列群を 1 つの string に。"""
    parts = JUST_LIT.findall(group_text)
    return decode_c_escapes("".join(parts))


def strip_c_comments(text: str) -> str:
    """C/C++ コメント (// ..., /* ... */) を取り除く。

    `_("...")` の例示が doxygen コメント内にあると誤って拾うので、
    解析前にコメントを除去する。 文字列リテラル中の // や /* は無視
    しないと "http://" のような URL を消してしまうので、 文字列も
    パターンに含めて素通りさせる。
    """
    pattern = re.compile(
        r'//[^\n]*'
        r'|/\*.*?\*/'
        r'|' + STR_LIT +
        r"|'(?:\\.|[^'\\])*'",
        re.DOTALL,
    )

    def repl(m):
        s = m.group(0)
        if s.startswith('//') or s.startswith('/*'):
            # コメントは空白に置き換え (行番号は維持)
            return re.sub(r'[^\n]', ' ', s)
        return s

    return pattern.sub(repl, text)


def find_source_files():
    for d in SRC_DIRS:
        for root, _, names in os.walk(ROOT / d):
            for n in names:
                p = Path(root) / n
                if p.suffix not in SRC_EXTS:
                    continue
                if p.resolve() in EXCLUDE_FILES:
                    continue
                yield p


def extract_underscore_keys(text: str):
    """`_("...")` 呼出を全て返す (decoded key, byte-offset)。"""
    for m in US_CALL.finditer(text):
        yield normalise_literal_group(m.group(0)), m.start()


def parse_table_keys():
    """i18n_table.c の全 entry の英語キーを返す。"""
    text = TABLE_FILE.read_text(encoding="utf-8")
    keys = set()
    for m in TABLE_ENTRY.finditer(text):
        # group_text は { "k1" "k2", "v1" "v2" } 全体。 最初の `,` で分割
        # しないと値側のリテラルまで含めてしまう。 分割は { "..." } のうち
        # 最後の "..." までを「キー側」 とみなすために手動で書き直す。
        key_part, _, _val = _split_entry(m.group(0))
        keys.add(normalise_literal_group(key_part))
    return keys


def _split_entry(entry: str):
    """`{ "key" "key2", "val" "val2" }` をキー側と値側に分割。

    string literal の中に `,` が含まれることはあっても regex 上は
    "..." の中身として捉えられているため、 リテラル境界の外側 (= 隣接
    リテラルの間の空白) にある最初の `,` がキーと値の区切り。
    """
    # 最初に "..." リテラルを見つけ、 連続している間進める。 その直後に
    # 来る非空白文字が `,` ならそこが境界。
    pos = 0
    while True:
        m = JUST_LIT.search(entry, pos)
        if not m:
            return entry, "", entry  # 不正な形 (起こらない想定)
        # 次の非空白
        i = m.end()
        while i < len(entry) and entry[i] in " \t\n":
            i += 1
        if i < len(entry) and entry[i] == ",":
            return entry[: m.end()], entry[m.start():m.end()], entry[i + 1:]
        # 隣接リテラルが続く場合は探索継続
        if i < len(entry) and entry[i] == '"':
            pos = i
            continue
        return entry, "", entry  # 想定外


def collect_source_keys():
    """全ソースから _("...") キー集合を返す。 file:line 情報も保持。"""
    keys: dict[str, list[str]] = {}
    for f in find_source_files():
        try:
            raw = f.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            raw = f.read_text(encoding="utf-8", errors="replace")
        text = strip_c_comments(raw)
        for key, off in extract_underscore_keys(text):
            line = text.count("\n", 0, off) + 1
            keys.setdefault(key, []).append(f"{f.relative_to(ROOT)}:{line}")
    return keys


def collect_source_literals():
    """全ソースの全文字列リテラル (decoded) を集合で返す。 隣接連結も。"""
    lits = set()
    # 隣接連結だけを正しくまとめる ad-hoc regex.
    adjacent = re.compile(r'(?:' + STR_LIT + r'\s*)+', re.DOTALL)
    for f in find_source_files():
        try:
            raw = f.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            raw = f.read_text(encoding="utf-8", errors="replace")
        text = strip_c_comments(raw)
        for m in adjacent.finditer(text):
            lits.add(normalise_literal_group(m.group(0)))
    return lits


def main():
    src_keys = collect_source_keys()
    table_keys = parse_table_keys()

    # 1. missing: source にあるが table に無い → 翻訳漏れ
    missing = sorted(set(src_keys) - table_keys)

    # 2. orphan: table にあるが source のどこにも (リテラルとして) 出ない
    src_lits = collect_source_literals()
    orphan = sorted(table_keys - src_lits)

    fail = False

    if missing:
        print(f"\n[FAIL] {len(missing)} key(s) used by _() but missing from i18n_table.c:")
        for k in missing:
            sites = src_keys[k][:3]
            extra = "" if len(src_keys[k]) <= 3 else f" (+{len(src_keys[k]) - 3} more)"
            print(f"  {k!r}")
            for s in sites:
                print(f"      at {s}")
            if extra:
                print(f"      {extra}")
        print("\n--- copy-paste template (fill in the JA translation) ---")
        for k in missing:
            esc = k.replace("\\", "\\\\").replace('"', '\\"')
            print(f'    {{ "{esc}", "TODO_JA" }},')
        print("--- end template ---")
        fail = True

    if orphan:
        print(f"\n[FAIL] {len(orphan)} key(s) in i18n_table.c not referenced anywhere in source:")
        for k in orphan:
            print(f"  {k!r}")
        fail = True

    if not fail:
        print(f"OK: {len(set(src_keys))} _() keys all present, "
              f"no orphan entries (table size {len(table_keys)}).")
        sys.exit(0)
    sys.exit(1)


if __name__ == "__main__":
    main()
