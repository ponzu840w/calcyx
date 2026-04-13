// app.js — calcyx web フロントエンド
// ネイティブ SheetView の挙動を再現する。

import { highlight } from './highlight.js';
import { applyColors } from './colors.js';
import { PasteDialog } from './paste-dialog.js';
import CalcyxModule from './calcyx.js';

// ---- カラーテーマ適用 ----
applyColors();

// ---- WASM 初期化 ----
const Module = await CalcyxModule();
const wasm = {
  init:       Module.cwrap('wasm_init',       null,     []),
  reset:      Module.cwrap('wasm_reset',      null,     []),
  eval_line:  Module.cwrap('wasm_eval_line',  'number', ['string']),
  get_result: Module.cwrap('wasm_get_result', 'string', []),
  get_error:  Module.cwrap('wasm_get_error',  'string', []),
};
wasm.init();

// WASM クラッシュ後の状態管理
let wasmBroken   = false;
let wasmReiniting = false;

async function reinitWasm() {
  console.info('[calcyx] WASM 再初期化開始...');
  try {
    const m = await CalcyxModule();
    wasm.init       = m.cwrap('wasm_init',       null,     []);
    wasm.reset      = m.cwrap('wasm_reset',      null,     []);
    wasm.eval_line  = m.cwrap('wasm_eval_line',  'number', ['string']);
    wasm.get_result = m.cwrap('wasm_get_result', 'string', []);
    wasm.get_error  = m.cwrap('wasm_get_error',  'string', []);
    wasm.init();
    wasmBroken = false;
    console.info('[calcyx] WASM 再初期化完了');
  } catch (e) {
    console.error('[calcyx] WASM 再初期化失敗:', e);
  }
}

// ---- 状態 ----
const sheetEl = document.getElementById('sheet');
const pasteDialog = new PasteDialog();

let rows = [{ expr: '', result: '', error: false }];
let focusedRow = 0;
let fileName = null;

const undoStack = [];
let undoPos = -1;

// ---- Undo/Redo ----

function captureSnapshot() {
  return {
    rows:    rows.map(r => ({ expr: r.expr })),
    focused: focusedRow,
  };
}

function pushUndo() {
  undoStack.splice(undoPos + 1);
  undoStack.push(captureSnapshot());
  undoPos = undoStack.length - 1;
  updateUndoButtons();
}

function applySnapshot(snap) {
  rows = snap.rows.map(r => ({ expr: r.expr, result: '', error: false }));
  focusedRow = Math.min(snap.focused, rows.length - 1);
  evalAll();
  renderAll();
  focusInput();
}

function undo() {
  if (undoPos <= 0) return;
  undoPos--;
  applySnapshot(undoStack[undoPos]);
  updateUndoButtons();
}

function redo() {
  if (undoPos >= undoStack.length - 1) return;
  undoPos++;
  applySnapshot(undoStack[undoPos]);
  updateUndoButtons();
}

function updateUndoButtons() {
  document.getElementById('btn-undo').disabled = undoPos <= 0;
  document.getElementById('btn-redo').disabled = undoPos >= undoStack.length - 1;
}

// ---- エンジン評価 ----

function evalAll() {
  const input = sheetEl.querySelector('.expr-input');
  if (input && focusedRow >= 0 && focusedRow < rows.length) {
    rows[focusedRow].expr = input.value;
  }

  if (wasmBroken) {
    if (!wasmReiniting) {
      wasmReiniting = true;
      reinitWasm().finally(() => { wasmReiniting = false; evalAll(); });
    }
    return;
  }

  try {
    wasm.reset();
  } catch (e) {
    console.error('[calcyx] wasm.reset() クラッシュ:', e.message);
    wasmBroken = true;
    for (const row of rows) { row.result = 'internal error'; row.error = true; }
    updateResultCells();
    return;
  }

  for (let i = 0; i < rows.length; i++) {
    const row = rows[i];
    if (!row.expr.trim()) { row.result = ''; row.error = false; continue; }
    let ok = false;
    try {
      ok = wasm.eval_line(row.expr);
    } catch (e) {
      console.error('[calcyx] wasm.eval_line() クラッシュ:', JSON.stringify(row.expr), e.message);
      wasmBroken = true;
      row.result = 'internal error'; row.error = true;
      for (let j = i + 1; j < rows.length; j++) { rows[j].result = ''; rows[j].error = false; }
      updateResultCells();
      return;
    }
    if (ok) { row.result = wasm.get_result(); row.error = false; }
    else    { row.result = wasm.get_error();  row.error = true;  }
  }
  updateResultCells();
}

// ---- レイアウト計算 ----

// キャンバスでテキスト幅を計測 (ネイティブの fl_width() 相当)
const _measureCanvas = document.createElement('canvas');
const _measureCtx    = _measureCanvas.getContext('2d');
_measureCtx.font = `${getComputedStyle(document.documentElement)
  .getPropertyValue('--font-size').trim() || '13px'} "Courier New", Courier, monospace`;

function measureText(text) {
  return _measureCtx.measureText(text).width;
}

// ネイティブ SheetView.h と合わせる
const PAD = 3;
const eqW = Math.ceil(measureText('==')) + 4;   // fl_width("==") + 4
let eqPos = 200;   // update_layout() で更新。結果がない間は変更しない。

// ネイティブ SheetView::update_layout() の完全移植
// eq_pos_ = "=" 列の x 座標 (= 式列幅)
function updateLayout() {
  const avail      = sheetEl.clientWidth || 600;
  const minEqPos   = Math.min(eqW, Math.floor(avail / 5));

  let maxExprW = 0, maxAnsW = 0, hasResult = false;
  for (const row of rows) {
    if (!row.result) continue;
    hasResult = true;
    if (row.expr)
      maxExprW = Math.max(maxExprW, Math.ceil(measureText(row.expr)) + PAD * 2);
    if (!row.error)
      maxAnsW = Math.max(maxAnsW, Math.ceil(measureText(row.result)) + PAD * 2);
  }

  if (hasResult) {
    let newEq;
    if (maxExprW + maxAnsW + eqW < avail)
      newEq = Math.max(minEqPos, maxExprW);
    else
      newEq = Math.max(minEqPos, avail - maxAnsW - eqW);
    newEq = Math.min(newEq, avail - eqW * 2);
    newEq = Math.max(newEq, minEqPos);
    eqPos = newEq;
  }

  // 式幅が eqPos を超える行は折り返し (native: row.wrapped)
  for (const row of rows) {
    row.wrapped = !!(row.result && row.expr &&
                     Math.ceil(measureText(row.expr)) > eqPos);
  }

  sheetEl.style.setProperty('--expr-col-w', eqPos + 'px');
  sheetEl.style.setProperty('--eq-col-w',   eqW   + 'px');
}

window.addEventListener('resize', () => { updateLayout(); renderAll(); });

// ---- レンダリング ----

function renderAll() {
  sheetEl.innerHTML = '';
  for (let i = 0; i < rows.length; i++) {
    sheetEl.appendChild(buildRowEl(i));
  }
  updateLayout();
  syncFmtSelect();
}

function buildRowEl(i) {
  const row = rows[i];
  const div = document.createElement('div');
  div.className = 'row' +
    (i === focusedRow ? ' focused' : '') +
    (row.expr === ''  ? ' empty'   : '') +
    (row.wrapped      ? ' wrapped' : '');
  div.dataset.row = i;

  // ---- 式列 ----
  if (i === focusedRow) {
    // フォーカス行: ハイライトオーバーレイ + 透明入力欄
    const exprCell = document.createElement('div');
    exprCell.className = 'expr-cell';

    const hlDiv = document.createElement('div');
    hlDiv.className = 'expr-hl-overlay';
    hlDiv.innerHTML = highlight(row.expr) || '&ZeroWidthSpace;';

    const input = document.createElement('input');
    input.type = 'text';
    input.className = 'expr-input';
    input.value = row.expr;
    input.setAttribute('spellcheck', 'false');
    input.setAttribute('autocomplete', 'off');

    exprCell.appendChild(hlDiv);
    exprCell.appendChild(input);
    div.appendChild(exprCell);
  } else {
    // 非フォーカス行: ハイライト表示のみ
    const exprCell = document.createElement('div');
    exprCell.className = 'expr-hl';
    exprCell.innerHTML = highlight(row.expr) || '&ZeroWidthSpace;';
    div.appendChild(exprCell);
  }

  // ---- = 列 ----
  const eqCell = document.createElement('div');
  eqCell.className = 'eq-cell';
  eqCell.textContent = '=';
  div.appendChild(eqCell);

  // ---- 結果列 ----
  const resCell = document.createElement('div');
  resCell.className = 'result-cell' + (row.error ? ' error' : '');
  resCell.dataset.result = i;
  resCell.tabIndex = -1;  // フォーカス可能 (Tab で移動できる)
  applyResultToCell(resCell, row);
  div.appendChild(resCell);

  return div;
}

// ネイティブ draw_result_at に準拠: エラー / 通常 (カラーボックス含む) を振り分け
// カラーリテラルの color-box は highlight() が inline span で処理する
function applyResultToCell(cell, row) {
  if (row.error) {
    cell.textContent = row.result;
  } else {
    cell.innerHTML = highlight(row.result);
  }
}

function updateResultCells() {
  for (let i = 0; i < rows.length; i++) {
    const row  = rows[i];
    const cell = sheetEl.querySelector(`[data-result="${i}"]`);
    if (!cell) continue;
    applyResultToCell(cell, row);
    cell.className = 'result-cell' + (row.error ? ' error' : '');
    const rowEl = cell.closest('.row');
    if (rowEl) {
      rowEl.classList.toggle('empty',   row.expr === '');
      rowEl.classList.toggle('wrapped', !!row.wrapped);
    }
  }
  updateLayout();
}

// ---- フォーカス管理 ----

// 入力欄のシンタックスハイライトオーバーレイを input の scroll に同期する
function syncOverlay() {
  const input = sheetEl.querySelector('.expr-input');
  if (!input) return;
  const hlDiv = input.closest('.expr-cell')?.querySelector('.expr-hl-overlay');
  if (!hlDiv) return;
  hlDiv.innerHTML = highlight(input.value) || '&ZeroWidthSpace;';
  hlDiv.style.transform = `translateX(${-input.scrollLeft}px)`;
}

function focusInput() {
  requestAnimationFrame(() => {
    const input = sheetEl.querySelector('.expr-input');
    if (input) {
      input.focus();
      input.setSelectionRange(input.value.length, input.value.length);
      input.closest('.row')?.scrollIntoView({ block: 'nearest' });
      syncOverlay();
    }
  });
}

// ウィンドウがフォーカスを取り戻したら input に戻す
window.addEventListener('focus', focusInput);

// メニューバーのボタンクリックでフォーカスを奪われないようにする
document.getElementById('menubar').addEventListener('mousedown', e => {
  if (e.target.tagName !== 'SELECT' && e.target.tagName !== 'INPUT') {
    e.preventDefault();
  }
});

function commitCurrentInput() {
  const input = sheetEl.querySelector('.expr-input');
  if (input && focusedRow >= 0 && focusedRow < rows.length) {
    rows[focusedRow].expr = input.value;
  }
}

// ---- フォーマット選択 ----

const FMT_FUNCS = ['dec','hex','bin','oct','si','kibi','char'];

function stripFormatter(expr) {
  const trimmed = expr.trimStart();
  for (const fn of FMT_FUNCS) {
    if (!trimmed.startsWith(fn)) continue;
    let p = fn.length;
    while (p < trimmed.length && trimmed[p] === ' ') p++;
    if (p >= trimmed.length || trimmed[p] !== '(') continue;
    const last = trimmed.trimEnd();
    if (!last.endsWith(')')) continue;
    return last.slice(p + 1, last.length - 1).trim();  // p は '(' の位置なので +1
  }
  return expr;
}

function detectFormatter(expr) {
  const trimmed = expr.trimStart();
  for (const fn of FMT_FUNCS) {
    if (!trimmed.startsWith(fn)) continue;
    let p = fn.length;
    while (p < trimmed.length && trimmed[p] === ' ') p++;
    if (p >= trimmed.length || trimmed[p] !== '(') continue;
    const last = trimmed.trimEnd();
    if (last.endsWith(')')) return fn;
  }
  return '';
}

function syncFmtSelect() {
  fmtSelect.value = detectFormatter(rows[focusedRow]?.expr ?? '');
}

function applyFmt(fmtName) {
  commitCurrentInput();
  const body = stripFormatter(rows[focusedRow].expr);
  rows[focusedRow].expr = fmtName ? `${fmtName}(${body})` : body;
  pushUndo();
  renderAll();
  evalAll();
  focusInput();
}

const fmtSelect = document.getElementById('fmt-select');
fmtSelect.addEventListener('change', e => {
  fmtSelect.blur();
  applyFmt(e.target.value);
});
fmtSelect.addEventListener('blur', focusInput);

// ---- イベントデリゲーション ----

sheetEl.addEventListener('input', e => {
  if (!e.target.classList.contains('expr-input')) return;
  rows[focusedRow].expr = e.target.value;
  syncOverlay();
  evalAll();
});

sheetEl.addEventListener('keydown', e => {
  if (e.target.classList.contains('expr-input')) {
    handleKeyDown(e);
    // カーソル移動後に rAF でオーバーレイを同期
    requestAnimationFrame(syncOverlay);
    return;
  }
  if (e.target.dataset.result !== undefined) {
    handleResultKeyDown(e);
  }
});

sheetEl.addEventListener('paste', e => {
  if (!e.target.classList.contains('expr-input')) return;
  handlePaste(e);
});

sheetEl.addEventListener('pointerdown', e => {
  const rowEl = e.target.closest('[data-row]');
  if (!rowEl) return;
  const i = parseInt(rowEl.dataset.row, 10);
  if (isNaN(i)) return;
  if (i !== focusedRow) {
    commitCurrentInput();
    focusedRow = i;
    renderAll();
    evalAll();
  }
  // input 自体のクリックはブラウザのカーソル配置に任せる
  if (e.target.tagName !== 'INPUT') {
    e.preventDefault();
    focusInput();
  }
});

// ---- キーボードハンドラ ----

function handleKeyDown(e) {
  const key  = e.key;
  const ctrl = e.ctrlKey || e.metaKey;
  const shift = e.shiftKey;

  // Undo / Redo
  if (ctrl && !shift && key === 'z') { e.preventDefault(); commitCurrentInput(); pushUndo(); undo(); return; }
  if (ctrl && (key === 'y' || (shift && key === 'z'))) { e.preventDefault(); redo(); return; }

  // Tab: 左辺 → 右辺へ (native: focus_result)
  if (key === 'Tab' && !shift && !ctrl) {
    e.preventDefault();
    const row = rows[focusedRow];
    if (row.result && !row.error) {
      const resCell = sheetEl.querySelector(`[data-result="${focusedRow}"]`);
      if (resCell) {
        resCell.focus();
        const sel = window.getSelection();
        const range = document.createRange();
        range.selectNodeContents(resCell);
        sel.removeAllRanges();
        sel.addRange(range);
        return;
      }
    }
    // 結果がない場合は次行へ
    moveToRow(focusedRow + 1, true);
    return;
  }

  // Enter: 次行へ
  if (key === 'Enter' && !ctrl && !shift) {
    e.preventDefault();
    commitCurrentInput();
    pushUndo();
    if (focusedRow === rows.length - 1) rows.push({ expr: '', result: '', error: false });
    focusedRow++;
    renderAll();
    focusInput();
    return;
  }

  // Shift+Enter: 上に挿入
  if (key === 'Enter' && shift) {
    e.preventDefault();
    commitCurrentInput();
    pushUndo();
    rows.splice(focusedRow, 0, { expr: '', result: '', error: false });
    renderAll();
    focusInput();
    evalAll();
    return;
  }

  // Shift+Delete / Shift+Backspace: 行削除
  if (shift && !ctrl && (key === 'Delete' || key === 'Backspace')) {
    e.preventDefault();
    commitCurrentInput();
    pushUndo();
    deleteCurrentRow(key === 'Delete');
    return;
  }

  // Ctrl+Delete: 次の行を削除
  if (ctrl && !shift && key === 'Delete') {
    e.preventDefault();
    commitCurrentInput();
    if (focusedRow < rows.length - 1) {
      pushUndo();
      rows.splice(focusedRow + 1, 1);
      renderAll();
      focusInput();
      evalAll();
    }
    return;
  }

  // Backspace on empty: 上の行へ移動して削除
  if (!ctrl && !shift && key === 'Backspace' && e.target.value === '') {
    e.preventDefault();
    pushUndo();
    deleteCurrentRow(false);
    return;
  }

  // 上下矢印: 行移動
  if (!ctrl && !shift && (key === 'ArrowUp' || key === 'ArrowDown')) {
    e.preventDefault();
    commitCurrentInput();
    if (key === 'ArrowUp'   && focusedRow > 0)               focusedRow--;
    if (key === 'ArrowDown' && focusedRow < rows.length - 1) focusedRow++;
    renderAll();
    focusInput();
    syncFmtSelect();
    return;
  }

  // Ctrl+Shift+Up/Down: 行の並び替え
  if (ctrl && shift && (key === 'ArrowUp' || key === 'ArrowDown')) {
    e.preventDefault();
    commitCurrentInput();
    pushUndo();
    const target = focusedRow + (key === 'ArrowUp' ? -1 : 1);
    if (target >= 0 && target < rows.length) {
      [rows[focusedRow], rows[target]] = [rows[target], rows[focusedRow]];
      focusedRow = target;
    }
    renderAll();
    focusInput();
    evalAll();
    return;
  }

  // Escape: 最後のコミット済み状態に戻す
  if (key === 'Escape') {
    e.preventDefault();
    if (undoPos >= 0 && undoPos < undoStack.length) {
      rows[focusedRow].expr = undoStack[undoPos].rows[focusedRow]?.expr ?? '';
    }
    renderAll();
    focusInput();
    evalAll();
    return;
  }
}

// 右辺セルのキーハンドラ (Tab で次行へ、文字入力で左辺に戻す)
function handleResultKeyDown(e) {
  if (e.key === 'Tab') {
    e.preventDefault();
    if (e.shiftKey) {
      focusInput();
    } else {
      // native: tab_from_result → 次行の左辺へ
      moveToRow(focusedRow + 1, true);
    }
  } else if (!e.ctrlKey && !e.metaKey && e.key.length === 1) {
    focusInput();
  }
}

// 行移動ヘルパー (末尾なら行追加)
function moveToRow(i, addIfEnd) {
  commitCurrentInput();
  if (i >= rows.length) {
    if (addIfEnd) {
      pushUndo();
      rows.push({ expr: '', result: '', error: false });
    } else {
      i = rows.length - 1;
    }
  }
  if (i < 0) i = 0;
  focusedRow = i;
  renderAll();
  focusInput();
}

function deleteCurrentRow(moveDown) {
  if (rows.length === 1) {
    rows[0] = { expr: '', result: '', error: false };
    renderAll();
    focusInput();
    evalAll();
    return;
  }
  rows.splice(focusedRow, 1);
  focusedRow = moveDown
    ? Math.min(focusedRow, rows.length - 1)
    : Math.max(0, focusedRow - 1);
  renderAll();
  focusInput();
  evalAll();
}

// ---- 貼り付け処理 ----

async function handlePaste(e) {
  const text = e.clipboardData?.getData('text') ?? '';
  if (!text.includes('\n') && !text.includes('\r')) return;

  e.preventDefault();
  const lines = await pasteDialog.open(text);
  if (!lines || lines.length === 0) return;

  commitCurrentInput();
  pushUndo();
  rows[focusedRow].expr = lines[0];
  for (let i = 1; i < lines.length; i++) {
    rows.splice(focusedRow + i, 0, { expr: lines[i], result: '', error: false });
  }
  focusedRow = focusedRow + lines.length - 1;
  renderAll();
  focusInput();
  evalAll();
}

// ---- ファイル操作 ----

document.getElementById('btn-new').addEventListener('click', () => {
  commitCurrentInput();
  pushUndo();
  rows = [{ expr: '', result: '', error: false }];
  focusedRow = 0;
  fileName = null;
  wasmBroken = false;
  renderAll();
  focusInput();
  evalAll();
});

document.getElementById('btn-open').addEventListener('click', () => {
  document.getElementById('file-input').click();
});

document.getElementById('file-input').addEventListener('change', async e => {
  const file = e.target.files[0];
  if (!file) return;
  const text = await file.text();
  e.target.value = '';
  commitCurrentInput();
  pushUndo();
  const lines = text.replace(/\r\n|\r/g, '\n').split('\n');
  rows = lines.map(l => ({ expr: l, result: '', error: false }));
  if (rows.length === 0) rows = [{ expr: '', result: '', error: false }];
  focusedRow = 0;
  fileName = file.name;
  renderAll();
  focusInput();
  evalAll();
});

document.getElementById('btn-save').addEventListener('click', () => {
  const text = rows.map(r => r.expr).join('\n');
  const blob = new Blob([text], { type: 'text/plain' });
  const url  = URL.createObjectURL(blob);
  const a    = document.createElement('a');
  a.href = url; a.download = fileName ?? 'calcyx.txt'; a.click();
  URL.revokeObjectURL(url);
});

// ---- Examples ----

const exampleSelect = document.getElementById('example-select');
exampleSelect.addEventListener('blur', focusInput);
exampleSelect.addEventListener('change', async e => {
  const filename = e.target.value;
  e.target.value = '';
  if (!filename) return;
  try {
    const res = await fetch(`samples/${filename}`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const text = await res.text();
    commitCurrentInput();
    pushUndo();
    const lines = text.replace(/\r\n|\r/g, '\n').split('\n');
    while (lines.length > 1 && lines[lines.length - 1].trim() === '') lines.pop();
    rows = lines.map(l => ({ expr: l, result: '', error: false }));
    if (rows.length === 0) rows = [{ expr: '', result: '', error: false }];
    focusedRow = 0;
    fileName = filename;
    renderAll();
    focusInput();
    evalAll();
  } catch (err) {
    console.error('[calcyx] サンプルロード失敗:', filename, err);
  }
});

// ---- Undo/Redo ボタン ----

document.getElementById('btn-undo').addEventListener('click', () => { commitCurrentInput(); undo(); });
document.getElementById('btn-redo').addEventListener('click', () => { commitCurrentInput(); redo(); });

// ---- グローバルキーボードショートカット ----

document.addEventListener('keydown', e => {
  const ctrl = e.ctrlKey || e.metaKey;
  if (!ctrl) return;
  if (e.key === 'z' && !e.shiftKey) { e.preventDefault(); commitCurrentInput(); pushUndo(); undo(); }
  if (e.key === 'y' || (e.key === 'z' && e.shiftKey)) { e.preventDefault(); redo(); }
  if (e.key === 'o') { e.preventDefault(); document.getElementById('btn-open').click(); }
  if (e.key === 's') { e.preventDefault(); document.getElementById('btn-save').click(); }
});

// ---- 初期化 ----

pushUndo();
renderAll();
focusInput();
