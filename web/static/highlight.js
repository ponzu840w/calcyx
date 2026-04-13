// highlight.js — calcyx 式のシンタックスハイライト
// ネイティブ SheetView の色使いに準拠。

// トークン種別
const T = {
  NUMBER:  'number',
  COLOR:   'color',   // #RRGGBB / #RGB カラーリテラル (別扱い)
  STRING:  'string',
  IDENT:   'ident',
  SYMBOL:  'symbol',
  SIPFX:   'sipfx',
  COMMENT: 'comment',
  TEXT:    'text',
};

// SI 接頭語文字 (後置)
const SI_SUFFIXES = new Set(['k','M','G','T','P','E','K','m','u','n','p','f','a']);

/**
 * 式文字列をトークン列に分解する。
 * @param {string} src
 * @returns {{ type: string, text: string }[]}
 */
export function tokenize(src) {
  const tokens = [];
  let i = 0;
  const len = src.length;

  while (i < len) {
    // コメント (;)
    if (src[i] === ';') {
      tokens.push({ type: T.COMMENT, text: src.slice(i) });
      break;
    }

    // 文字列リテラル "..."
    if (src[i] === '"') {
      let j = i + 1;
      while (j < len && src[j] !== '"') {
        if (src[j] === '\\') j++;
        j++;
      }
      if (j < len) j++; // closing "
      tokens.push({ type: T.STRING, text: src.slice(i, j) });
      i = j;
      continue;
    }

    // 文字リテラル '...'
    if (src[i] === "'") {
      let j = i + 1;
      while (j < len && src[j] !== "'") {
        if (src[j] === '\\') j++;
        j++;
      }
      if (j < len) j++;
      tokens.push({ type: T.STRING, text: src.slice(i, j) });
      i = j;
      continue;
    }

    // 16進数 0x...
    if (src[i] === '0' && i + 1 < len && (src[i+1] === 'x' || src[i+1] === 'X')) {
      let j = i + 2;
      while (j < len && /[0-9a-fA-F_]/.test(src[j])) j++;
      tokens.push({ type: T.NUMBER, text: src.slice(i, j) });
      i = j;
      continue;
    }

    // 2進数 0b...
    if (src[i] === '0' && i + 1 < len && (src[i+1] === 'b' || src[i+1] === 'B')) {
      let j = i + 2;
      while (j < len && /[01_]/.test(src[j])) j++;
      tokens.push({ type: T.NUMBER, text: src.slice(i, j) });
      i = j;
      continue;
    }

    // 8進数 0o...
    if (src[i] === '0' && i + 1 < len && (src[i+1] === 'o' || src[i+1] === 'O')) {
      let j = i + 2;
      while (j < len && /[0-7_]/.test(src[j])) j++;
      tokens.push({ type: T.NUMBER, text: src.slice(i, j) });
      i = j;
      continue;
    }

    // 数値 (10進, 指数表記含む)
    if (/[0-9]/.test(src[i]) || (src[i] === '.' && i + 1 < len && /[0-9]/.test(src[i+1]))) {
      let j = i;
      while (j < len && /[0-9_]/.test(src[j])) j++;
      if (j < len && src[j] === '.') {
        j++;
        while (j < len && /[0-9_]/.test(src[j])) j++;
      }
      if (j < len && (src[j] === 'e' || src[j] === 'E')) {
        j++;
        if (j < len && (src[j] === '+' || src[j] === '-')) j++;
        while (j < len && /[0-9]/.test(src[j])) j++;
      }
      // SI 接頭語 (k, M, G, ...)
      if (j < len && SI_SUFFIXES.has(src[j]) &&
          (j + 1 >= len || !/[a-zA-Z_]/.test(src[j+1]))) {
        tokens.push({ type: T.NUMBER, text: src.slice(i, j) });
        tokens.push({ type: T.SIPFX,  text: src[j] });
        i = j + 1;
      } else {
        tokens.push({ type: T.NUMBER, text: src.slice(i, j) });
        i = j;
      }
      continue;
    }

    // # で始まるリテラル
    if (src[i] === '#') {
      let j = i + 1;
      // まず 16 進数字だけ読む
      while (j < len && /[0-9a-fA-F]/.test(src[j])) j++;
      const hexLen = j - i - 1;
      if (hexLen === 6 || hexLen === 3) {
        // #RRGGBB / #RGB → カラーリテラル (inline color-box)
        tokens.push({ type: T.COLOR, text: src.slice(i, j) });
      } else {
        // 日時リテラル #YYYY/MM/DD HH:MM:SS# など — 残りを読み切る
        // ネイティブ: FMT_DATETIME → C_SPECIAL (オレンジ)
        while (j < len && /[0-9a-fA-F\/\-:T\s]/.test(src[j])) j++;
        tokens.push({ type: T.STRING, text: src.slice(i, j) });
      }
      i = j;
      continue;
    }

    // 識別子 / キーワード
    if (/[a-zA-Z_]/.test(src[i])) {
      let j = i + 1;
      while (j < len && /[a-zA-Z0-9_]/.test(src[j])) j++;
      tokens.push({ type: T.IDENT, text: src.slice(i, j) });
      i = j;
      continue;
    }

    // 演算子・区切り文字 (2文字演算子を先にチェック)
    const two = src.slice(i, i + 2);
    if (['**', '//', '<<', '>>', '<=', '>=', '==', '!=', '&&', '||', '??'].includes(two)) {
      tokens.push({ type: T.SYMBOL, text: two });
      i += 2;
      continue;
    }

    // 1文字演算子 ($ はエンジン lexer の OP_SYMBOLS に含まれる)
    if (/[+\-*/%^&|~!<>=(),\[\]{}.:?$]/.test(src[i])) {
      tokens.push({ type: T.SYMBOL, text: src[i] });
      i++;
      continue;
    }

    // それ以外 (空白など)
    tokens.push({ type: T.TEXT, text: src[i] });
    i++;
  }

  return tokens;
}

// CSS クラス名へのマッピング (COLOR・括弧は別途処理)
const CLASS_MAP = {
  [T.NUMBER]:  'hl-text',    // 数値はデフォルト色 (native: 白。特殊なケースは else で対処)
  [T.STRING]:  'hl-string',  // 文字/文字列リテラル → オレンジ (native: C_SPECIAL)
  [T.IDENT]:   'hl-ident',
  [T.SYMBOL]:  'hl-symbol',
  [T.SIPFX]:   'hl-sipfx',
  [T.COMMENT]: 'hl-comment',
  [T.TEXT]:    'hl-text',
};

/** カラーリテラルの色から文字色 (白/黒) を決定する */
function colorFg(hex6) {
  const r = parseInt(hex6.slice(0, 2), 16);
  const g = parseInt(hex6.slice(2, 4), 16);
  const b = parseInt(hex6.slice(4, 6), 16);
  return (r * 299 + g * 587 + b * 114) / 1000 < 128 ? '#ffffff' : '#000000';
}

/**
 * トークン列から HTML 文字列を生成する。
 * - 括弧: ネイティブ C_PAREN[4] に準拠した深さ別カラー
 * - カラーリテラル: inline color-box (式列・結果列共通)
 * @param {string} src
 * @returns {string} HTML
 */
export function highlight(src) {
  if (!src) return '';
  const tokens = tokenize(src);
  let parenDepth = 0;
  return tokens.map(tok => {
    // カラーリテラル → inline color box span
    if (tok.type === T.COLOR) {
      let hex = tok.text.slice(1).toUpperCase();
      if (hex.length === 3) hex = hex[0]+hex[0]+hex[1]+hex[1]+hex[2]+hex[2];
      const fg = colorFg(hex);
      const escaped = tok.text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
      return `<span class="hl-color-box" style="background:#${hex};color:${fg}">${escaped}</span>`;
    }

    let cls;
    if (tok.type === T.SYMBOL && (tok.text === '(' || tok.text === ')')) {
      // ネイティブ C_PAREN[4]: 深さ別に 4 色サイクル
      let d = parenDepth;
      if (tok.text === ')' && d > 0) d--;
      cls = `hl-paren-${d % 4}`;
      if (tok.text === '(') parenDepth++;
      else if (parenDepth > 0) parenDepth--;
    } else {
      cls = CLASS_MAP[tok.type] || 'hl-text';
    }
    const escaped = tok.text
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;');
    return `<span class="${cls}">${escaped}</span>`;
  }).join('');
}
