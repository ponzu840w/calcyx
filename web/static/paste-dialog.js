// paste-dialog.js — PasteOptionForm 相当の貼り付けオプションダイアログ
// 移植元: Calctus/UI/PasteOptionForm.cs

/**
 * テキストを改行で分割する。\r\n / \n / \r すべて対応。
 * @param {string} text
 * @returns {string[]}
 */
function splitLines(text) {
  return text.split(/\r\n|\r|\n/);
}

/**
 * 指定デリミタで行を分割する。
 * スペースの場合は連続空白をまとめる。
 * @param {string} line
 * @param {string} delim
 * @returns {string[]}
 */
function splitBy(line, delim) {
  if (delim === ' ') {
    return line.split(/[ \t]+/).filter(s => s.length > 0);
  }
  return line.split(delim);
}

function trim(s) { return s.trim(); }

/**
 * デリミタ入力文字列をデリミタ文字に変換する。
 * @param {string} v
 * @returns {string}
 */
function parseDelim(v) {
  if (v === '\\t') return '\t';
  if (!v) return ' ';
  return v[0];
}

/**
 * デリミタを自動検出する。
 * @param {string} text
 * @returns {string} デリミタ入力欄に入れる文字列
 */
function detectDelim(text) {
  if (text.includes('\t')) return '\\t';
  if (text.includes('|'))  return '|';
  if (text.includes(','))  return ',';
  return ' ';
}

/**
 * 最大カラム数を計算する。
 * @param {string} text
 * @param {string} delim
 * @returns {number}
 */
function countCols(text, delim) {
  let max = 0;
  for (const line of splitLines(text)) {
    if (!line) continue;
    const n = splitBy(line, delim).length;
    if (n > max) max = n;
  }
  return max;
}

export class PasteDialog {
  constructor() {
    this._dialog   = document.getElementById('paste-dialog');
    this._srcEl    = document.getElementById('paste-src');
    this._prevEl   = document.getElementById('paste-preview');
    this._delimEl  = document.getElementById('paste-delim');
    this._colIdxEl = document.getElementById('paste-col-idx');
    this._colTotEl = document.getElementById('paste-col-total');
    this._btnSelCol   = document.getElementById('btn-select-col');
    this._btnRemComma = document.getElementById('btn-remove-commas');
    this._btnRemRhs   = document.getElementById('btn-remove-rhs');
    this._btnOk       = document.getElementById('paste-ok');
    this._btnCancel   = document.getElementById('paste-cancel');

    this._delimEl.addEventListener('input', () => this._updateColCount());
    this._btnSelCol.addEventListener('click', () => this._selectColumn());
    this._btnRemComma.addEventListener('click', () => this._removeCommas());
    this._btnRemRhs.addEventListener('click', () => this._removeRightHands());
    this._btnOk.addEventListener('click', () => this._onOk());
    this._btnCancel.addEventListener('click', () => this._onCancel());

    this._resolve = null;
  }

  /**
   * ダイアログを表示して結果行の配列を返す。
   * キャンセル時は null を返す。
   * @param {string} clipboardText
   * @returns {Promise<string[]|null>}
   */
  open(clipboardText) {
    // 末尾改行を除去して正規化
    const src = clipboardText.replace(/[\r\n]+$/, '').replace(/\r\n|\r/g, '\n');
    this._srcEl.value  = src;
    this._prevEl.value = src;

    this._delimEl.value = detectDelim(src);
    this._colIdxEl.value = '1';
    this._updateColCount();

    this._dialog.showModal();

    return new Promise(resolve => { this._resolve = resolve; });
  }

  _updateColCount() {
    const src   = this._srcEl.value;
    const delim = parseDelim(this._delimEl.value);
    const n     = countCols(src, delim);
    this._colTotEl.textContent = `/ ${n}`;
  }

  _selectColumn() {
    const colN  = parseInt(this._colIdxEl.value, 10);
    const src   = this._srcEl.value;
    const delim = parseDelim(this._delimEl.value);

    if (isNaN(colN) || colN < 0) return;

    if (colN === 0) {
      this._prevEl.value = src;
      return;
    }

    const colIdx = colN - 1;
    const lines  = splitLines(src);
    const result = [];
    for (const line of lines) {
      if (!line) continue;
      const cols = splitBy(line, delim);
      if (colIdx < cols.length) result.push(trim(cols[colIdx]));
    }
    this._prevEl.value = result.join('\n');
  }

  _removeCommas() {
    this._prevEl.value = this._prevEl.value.replace(/,/g, '');
  }

  _removeRightHands() {
    const lines = splitLines(this._prevEl.value);
    const result = [];
    for (let line of lines) {
      line = trim(line);
      if (!line) continue;
      const eq = line.lastIndexOf('=');
      if (eq >= 0) line = trim(line.slice(0, eq));
      result.push(line);
    }
    this._prevEl.value = result.join('\n');
  }

  _onOk() {
    const text  = this._prevEl.value.replace(/[\r\n]+$/, '');
    const lines = text ? splitLines(text).filter(l => l !== '') : [];
    this._dialog.close();
    if (this._resolve) this._resolve(lines);
    this._resolve = null;
  }

  _onCancel() {
    this._dialog.close();
    if (this._resolve) this._resolve(null);
    this._resolve = null;
  }
}
