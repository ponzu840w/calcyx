// highlight.js — calcyx 式のシンタックスハイライト
// ネイティブ SheetView の色使いに準拠。

// トークン種別
const T = {
  NUMBER:  'number',
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

    // Webカラー / 日時 #...  (# で始まる特殊リテラル)
    if (src[i] === '#') {
      let j = i + 1;
      while (j < len && /[0-9a-fA-F\-:T\s]/.test(src[j])) j++;
      tokens.push({ type: T.NUMBER, text: src.slice(i, j) });
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

    // 1文字演算子
    if (/[+\-*/%^&|~!<>=(),\[\]{}.:?]/.test(src[i])) {
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

// CSS クラス名へのマッピング
const CLASS_MAP = {
  [T.NUMBER]:  'hl-number',
  [T.STRING]:  'hl-string',
  [T.IDENT]:   'hl-ident',
  [T.SYMBOL]:  'hl-symbol',
  [T.SIPFX]:   'hl-sipfx',
  [T.COMMENT]: 'hl-comment',
  [T.TEXT]:    'hl-text',
};

/**
 * トークン列から HTML 文字列を生成する。
 * @param {string} src
 * @returns {string} HTML
 */
export function highlight(src) {
  if (!src) return '';
  const tokens = tokenize(src);
  return tokens.map(tok => {
    const cls = CLASS_MAP[tok.type] || 'hl-text';
    const escaped = tok.text
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;');
    return `<span class="${cls}">${escaped}</span>`;
  }).join('');
}
