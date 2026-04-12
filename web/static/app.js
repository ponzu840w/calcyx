// app.js — calcyx web フロントエンド
// ネイティブ SheetView の挙動を再現する。

import { highlight } from './highlight.js';
import { PasteDialog } from './paste-dialog.js';
import CalcyxModule from './calcyx.js';

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
// RuntimeError が発生したらモジュール全体を再初期化して回復する。
let wasmBroken   = false;  // クラッシュ済みフラグ
let wasmReiniting = false; // 再初期化中フラグ

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
    // wasmBroken のまま維持
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

// デバッグログ (常に出力)
function dbg(...args) { console.log('[calcyx]', ...args); }

function evalAll() {
  // 常に現在の入力値を rows に同期してから評価する
  const input = sheetEl.querySelector('.expr-input');
  if (input && focusedRow >= 0 && focusedRow < rows.length) {
    rows[focusedRow].expr = input.value;
  }

  // WASM がクラッシュ済みなら再初期化をスケジュールして待機
  if (wasmBroken) {
    if (!wasmReiniting) {
      wasmReiniting = true;
      reinitWasm().finally(() => {
        wasmReiniting = false;
        evalAll();  // 再初期化後に再評価
      });
    }
    return;  // 再初期化が完了するまで評価しない
  }

  dbg('evalAll', rows.map(r => r.expr));

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
    if (!row.expr.trim()) {
      row.result = '';
      row.error  = false;
      continue;
    }
    let ok = false;
    try {
      ok = wasm.eval_line(row.expr);
    } catch (e) {
      // WASM RuntimeError (memory access out of bounds など) からの回復。
      // クラッシュ後はヒープが破壊されているため wasm.reset() も呼ばない。
      // wasmBroken = true にして次の evalAll 呼び出し時に再初期化する。
      console.error('[calcyx] wasm.eval_line() クラッシュ:', JSON.stringify(row.expr), e.message);
      console.error('[calcyx] スタック:', e.stack);
      wasmBroken = true;
      row.result = 'internal error';
      row.error  = true;
      // クラッシュ後の行は g_ctx が不整合なので評価できない → 結果をクリア
      for (let j = i + 1; j < rows.length; j++) {
        rows[j].result = '';
        rows[j].error  = false;
      }
      updateResultCells();
      return;
    }
    if (ok) {
      row.result = wasm.get_result();
      row.error  = false;
    } else {
      row.result = wasm.get_error();
      row.error  = true;
    }
    dbg(' ', JSON.stringify(row.expr), '->', row.result, row.error ? '(err)' : '');
  }
  updateResultCells();
}

// ---- レンダリング ----

function renderAll() {
  sheetEl.innerHTML = '';
  for (let i = 0; i < rows.length; i++) {
    sheetEl.appendChild(buildRowEl(i));
  }
  updateLayout();
}

function buildRowEl(i) {
  const row = rows[i];
  const div = document.createElement('div');
  div.className = 'row' + (i === focusedRow ? ' focused' : '') +
                  (row.expr === '' ? ' empty' : '');
  div.dataset.row = i;

  if (i === focusedRow) {
    const exprCell = document.createElement('div');
    exprCell.className = 'expr-cell';
    const input = document.createElement('input');
    input.type = 'text';
    input.className = 'expr-input';
    input.value = row.expr;
    input.setAttribute('spellcheck', 'false');
    input.setAttribute('autocomplete', 'off');
    exprCell.appendChild(input);
    div.appendChild(exprCell);
  } else {
    const exprCell = document.createElement('div');
    exprCell.className = 'expr-hl';
    exprCell.innerHTML = highlight(row.expr) || '&ZeroWidthSpace;';
    exprCell.dataset.clickRow = i;
    div.appendChild(exprCell);
  }

  const eqCell = document.createElement('div');
  eqCell.className = 'eq-cell';
  eqCell.textContent = '=';
  div.appendChild(eqCell);

  const resCell = document.createElement('div');
  resCell.className = 'result-cell' + (row.error ? ' error' : '');
  resCell.dataset.result = i;
  resCell.textContent = row.result;
  div.appendChild(resCell);

  return div;
}

function updateResultCells() {
  for (let i = 0; i < rows.length; i++) {
    const row = rows[i];
    const cell = sheetEl.querySelector(`[data-result="${i}"]`);
    if (!cell) continue;
    cell.textContent = row.result;
    cell.className   = 'result-cell' + (row.error ? ' error' : '');
    const rowEl = cell.closest('.row');
    if (rowEl) {
      if (row.expr === '') rowEl.classList.add('empty');
      else                 rowEl.classList.remove('empty');
    }
  }
  updateLayout();
}

// キャンバスでテキスト幅を計測 (ネイティブの fl_width() 相当)
const _measureCanvas = document.createElement('canvas');
const _measureCtx = _measureCanvas.getContext('2d');
_measureCtx.font = `${getComputedStyle(document.documentElement)
  .getPropertyValue('--font-size').trim() || '13px'} "Courier New", Courier, monospace`;

function measureText(text) {
  return _measureCtx.measureText(text).width;
}

const PAD = 8;   // 左右パディング合計
const EQ_W = 28; // = 列幅

// ネイティブの update_layout() 相当:
//   式列幅 = 最長の式テキスト幅 (ウィンドウ幅に依存しない)
//   結果列 = 残り全体 (CSS 側で 1fr)
// 収まらない場合は結果幅を確保して、式列を縮める。
function updateLayout() {
  let maxExprW = 60;
  let maxResW  = 60;
  for (const row of rows) {
    if (row.expr) {
      const w = Math.ceil(measureText(row.expr)) + PAD;
      if (w > maxExprW) maxExprW = w;
    }
    if (!row.error && row.result) {
      const w = Math.ceil(measureText(row.result)) + PAD;
      if (w > maxResW) maxResW = w;
    }
  }

  const avail = sheetEl.clientWidth || 600;
  let exprColW;
  if (maxExprW + EQ_W + maxResW <= avail) {
    // 全て収まる → 式列はコンテンツ幅
    exprColW = maxExprW;
  } else {
    // 収まらない → 結果列を確保して残りを式列に
    exprColW = Math.max(60, avail - EQ_W - maxResW);
  }

  sheetEl.style.setProperty('--expr-col-w', exprColW + 'px');
}

// ウィンドウリサイズ時も再計算
window.addEventListener('resize', updateLayout);

// focusInput: requestAnimationFrame で <select> 等からのフォーカス奪還を確実にする
function focusInput() {
  requestAnimationFrame(() => {
    const input = sheetEl.querySelector('.expr-input');
    if (input) {
      input.focus();
      input.setSelectionRange(input.value.length, input.value.length);
    }
  });
}

function commitCurrentInput() {
  const input = sheetEl.querySelector('.expr-input');
  if (input && focusedRow >= 0 && focusedRow < rows.length) {
    rows[focusedRow].expr = input.value;
  }
}

// ---- イベントデリゲーション（#sheet 全体で受け取る）----
// renderAll のたびにリスナーを付け直す必要がなく、常に安定して動作する。

sheetEl.addEventListener('input', e => {
  if (!e.target.classList.contains('expr-input')) return;
  dbg('input event value=', e.target.value, 'focusedRow=', focusedRow);
  rows[focusedRow].expr = e.target.value;
  evalAll();
});

sheetEl.addEventListener('keydown', e => {
  if (!e.target.classList.contains('expr-input')) return;
  handleKeyDown(e);
});

sheetEl.addEventListener('paste', e => {
  if (!e.target.classList.contains('expr-input')) return;
  handlePaste(e);
});

sheetEl.addEventListener('pointerdown', e => {
  const clickRowEl = e.target.closest('[data-click-row]');
  if (!clickRowEl) return;
  const i = parseInt(clickRowEl.dataset.clickRow, 10);
  if (isNaN(i) || i === focusedRow) return;
  commitCurrentInput();
  focusedRow = i;
  renderAll();
  focusInput();
  evalAll();
});

// ---- キーボードハンドラ ----

function handleKeyDown(e) {
  const key   = e.key;
  const ctrl  = e.ctrlKey || e.metaKey;
  const shift = e.shiftKey;

  // Undo / Redo
  if (ctrl && !shift && key === 'z') { e.preventDefault(); commitCurrentInput(); pushUndo(); undo(); return; }
  if (ctrl && (key === 'y' || (shift && key === 'z'))) { e.preventDefault(); redo(); return; }

  // Enter: 次行へ（末尾なら追加）
  if (key === 'Enter' && !ctrl && !shift) {
    e.preventDefault();
    commitCurrentInput();
    pushUndo();
    if (focusedRow === rows.length - 1) {
      rows.push({ expr: '', result: '', error: false });
    }
    focusedRow++;
    renderAll();
    focusInput();
    return;
  }

  // Shift+Enter: 現在行の上に挿入
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

// ---- フォーマット選択 ----

const FMT_FUNCS = ['dec','hex','bin','oct','si','kibi','char'];

function stripFormatter(expr) {
  const trimmed = expr.trimStart();
  for (const fn of FMT_FUNCS) {
    if (!trimmed.startsWith(fn)) continue;
    let p = fn.length;
    while (p < trimmed.length && trimmed[p] === ' ') p++;
    if (p >= trimmed.length || trimmed[p] !== '(') continue;
    p++;
    const last = trimmed.trimEnd();
    if (!last.endsWith(')')) continue;
    return last.slice(p, last.length - 1).trim();
  }
  return expr;
}

function applyFmt(fmtName) {
  commitCurrentInput();
  const body = stripFormatter(rows[focusedRow].expr);
  rows[focusedRow].expr = fmtName ? `${fmtName}(${body})` : body;
  pushUndo();
  renderAll();
  evalAll();
  focusInput(); // rAF 内で実行されるので evalAll より後でも問題なし
}

const fmtSelect = document.getElementById('fmt-select');
fmtSelect.addEventListener('change', e => {
  const fmt = e.target.value;
  // フォーカスをセレクトから手放させてから処理
  fmtSelect.blur();
  applyFmt(fmt);
});

// ---- ファイル操作 ----

document.getElementById('btn-new').addEventListener('click', () => {
  commitCurrentInput();
  pushUndo();
  rows = [{ expr: '', result: '', error: false }];
  focusedRow = 0;
  fileName = null;
  wasmBroken = false;  // 新規作成時は再初期化を試みる
  renderAll();
  focusInput();
  evalAll();  // wasm.reset() は evalAll 内で実行
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
  a.href     = url;
  a.download = fileName ?? 'calcyx.txt';
  a.click();
  URL.revokeObjectURL(url);
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
