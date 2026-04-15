// app.js — calcyx web フロントエンド
// ネイティブ SheetView の挙動を再現する。

import { highlight } from './highlight.js';
import { applyColors } from './colors.js';
import { PasteDialog } from './paste-dialog.js';
import CalcyxModule from './calcyx.js';
import { CALCYX_VERSION, CACHE_BUST } from './version.js';

// Emscripten が calcyx.wasm を fetch するときのパスに ?v=... を付与する。
// calcyx.js 本体は静的 import 経由でビルド時にクエリが付くが、wasm は
// calcyx.js 内部の locateFile 経由で読まれるので別途指定する必要がある。
const _wasmInitOpts = {
  locateFile: (p) => p.endsWith('.wasm') ? `${p}?v=${CACHE_BUST}` : p,
};

// ---- カラーテーマ適用 ----
applyColors();

// ---- WASM 初期化 ----
const Module = await CalcyxModule(_wasmInitOpts);
const wasm = {
  init:           Module.cwrap('wasm_init',           null,     []),
  reset:          Module.cwrap('wasm_reset',          null,     []),
  eval_line:      Module.cwrap('wasm_eval_line',      'number', ['string']),
  get_result:     Module.cwrap('wasm_get_result',     'string', []),
  get_error:      Module.cwrap('wasm_get_error',      'string', []),
  result_visible: Module.cwrap('wasm_result_visible', 'number', ['string']),
};
wasm.init();

// WASM クラッシュ後の状態管理
let wasmBroken   = false;
let wasmReiniting = false;

async function reinitWasm() {
  console.info('[calcyx] WASM 再初期化開始...');
  try {
    const m = await CalcyxModule(_wasmInitOpts);
    wasm.init           = m.cwrap('wasm_init',           null,     []);
    wasm.reset          = m.cwrap('wasm_reset',          null,     []);
    wasm.eval_line      = m.cwrap('wasm_eval_line',      'number', ['string']);
    wasm.get_result     = m.cwrap('wasm_get_result',     'string', []);
    wasm.get_error      = m.cwrap('wasm_get_error',      'string', []);
    wasm.result_visible = m.cwrap('wasm_result_visible', 'number', ['string']);
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

// 永続フロート編集エディタ。フォーカス行の expr セルに重ねて表示する。
// 行移動 / renderAll() でも DOM から外さないので、Android の IME が閉じ直されない。
const floatEditor  = document.createElement('div');
floatEditor.id = 'float-editor';

const floatOverlay = document.createElement('div');
floatOverlay.className = 'expr-hl-overlay';

const floatInput = document.createElement('input');
floatInput.type = 'text';
floatInput.className = 'expr-input';
floatInput.setAttribute('spellcheck', 'false');
floatInput.setAttribute('autocomplete', 'off');

floatEditor.appendChild(floatOverlay);
floatEditor.appendChild(floatInput);
sheetEl.appendChild(floatEditor);

let rows = [{ expr: '', result: '', error: false }];
let focusedRow = 0;
let fileName = null;

const undoStack = [];
let undoPos = -1;
// フォーカス行に入った時点の expr。commit 時に差分があれば行ごとに push する
// (native: SheetView::commit() が original_expr_ と比較するのに相当)。
let originalExpr = '';

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
  renderAll();   // 先にDOMを再構築してからevalAll (逆順だと古いinput値を読んでしまう)
  evalAll();
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
  // 未コミット変更がある場合は Undo を有効化 (Ctrl+Z で取り消せるため)
  const snap = undoPos >= 0 ? undoStack[undoPos] : null;
  const hasUncommitted = floatInput.value !== (snap?.rows[focusedRow]?.expr ?? '');
  document.getElementById('btn-undo').disabled = undoPos <= 0 && !hasUncommitted;
  document.getElementById('btn-redo').disabled = undoPos >= undoStack.length - 1;
}

// ---- エンジン評価 ----

function evalAll() {
  if (focusedRow >= 0 && focusedRow < rows.length) {
    rows[focusedRow].expr = floatInput.value;
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
    if (!row.expr.trim()) { row.result = ''; row.error = false; row.showResult = true; continue; }
    // 変数定義・関数定義は = と右辺を非表示
    row.showResult = !!wasm.result_visible(row.expr);
    let ok = false;
    try {
      ok = wasm.eval_line(row.expr);
    } catch (e) {
      console.error('[calcyx] wasm.eval_line() クラッシュ:', JSON.stringify(row.expr), e.message);
      wasmBroken = true;
      row.result = 'internal error'; row.error = true;
      for (let j = i + 1; j < rows.length; j++) { rows[j].result = ''; rows[j].error = false; rows[j].showResult = true; }
      updateResultCells();
      return;
    }
    if (ok) { row.result = wasm.get_result(); row.error = false; }
    else    { row.result = wasm.get_error();  row.error = true;  }
  }
  updateResultCells();
}

// ---- レイアウト計算 ----

// テキスト幅計測用の hidden DOM 要素 (ネイティブの fl_width() 相当)。
// 当初は canvas.measureText を使っていたが、Android では実際の DOM レンダリング幅と
// 微妙にズレて左辺カラムが必要より短くなる問題があったため、オーバーレイと
// 同じフォントスタイルの DOM 要素で offsetWidth を読む方式に変更。
const _measureEl = document.createElement('div');
_measureEl.setAttribute('aria-hidden', 'true');
_measureEl.style.cssText =
  'position:absolute; visibility:hidden; left:-9999px; top:0; ' +
  'white-space:pre; padding:0; margin:0; border:0; ' +
  'font-family:var(--font); font-size:var(--font-size);';
document.body.appendChild(_measureEl);

function measureText(text) {
  _measureEl.textContent = text;
  return _measureEl.offsetWidth;
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
    if (!row.result || !row.showResult) continue;
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
    row.wrapped = !!(row.result && row.showResult && row.expr &&
                     Math.ceil(measureText(row.expr)) > eqPos);
  }

  sheetEl.style.setProperty('--expr-col-w', eqPos + 'px');
  sheetEl.style.setProperty('--eq-col-w',   eqW   + 'px');
}

// resize 時は DOM 再構築せずクラスだけ更新する。
// renderAll() を呼ぶと focused 行の <input> が破棄され、
// Android でソフトキーボード表示に伴う viewport resize で
// キーボードが即座に引っ込んでしまう。
window.addEventListener('resize', () => {
  updateLayout();
  for (let i = 0; i < rows.length; i++) {
    const rowEl = sheetEl.querySelector(`[data-row="${i}"]`);
    if (!rowEl) continue;
    rowEl.classList.toggle('empty',     rows[i].expr === '');
    rowEl.classList.toggle('wrapped',   !!rows[i].wrapped);
    rowEl.classList.toggle('no-result', rows[i].showResult === false);
  }
  positionFloatingEditor();
});

// ---- レンダリング ----

function renderAll() {
  // floatEditor / rowActionBar は DOM に残したまま行要素だけ差し替え、floatInput のフォーカスを維持する
  const rowActionBar = document.getElementById('row-action-bar');
  const toRemove = [];
  for (const c of sheetEl.children) {
    if (c !== floatEditor && c !== rowActionBar) toRemove.push(c);
  }
  toRemove.forEach(el => el.remove());
  for (let i = 0; i < rows.length; i++) {
    sheetEl.appendChild(buildRowEl(i));
  }
  updateLayout();
  // updateLayout() は row.wrapped を更新するが、buildRowEl はその前に
  // 古い row.wrapped でクラスを付けてしまっているので、ここで同期する。
  // これをやらないと初回サンプルロード時に「narrow 列 + 折り返しなし」の
  // 奇妙な表示になり、次の何かの操作 (updateResultCells 経由) で展開される。
  for (let i = 0; i < rows.length; i++) {
    const rowEl = sheetEl.querySelector(`[data-row="${i}"]`);
    if (!rowEl) continue;
    rowEl.classList.toggle('empty',     rows[i].expr === '');
    rowEl.classList.toggle('wrapped',   !!rows[i].wrapped);
    rowEl.classList.toggle('no-result', rows[i].showResult === false);
  }
  // フォーカス行の式を floatInput に反映 (変換中は触らない)
  const expr = rows[focusedRow]?.expr ?? '';
  if (!imeComposing && floatInput.value !== expr) {
    floatInput.value = expr;
    originalExpr = expr;  // 新しい編集セッションの比較基準
  }
  positionFloatingEditor();
  syncOverlay();
  syncFmtSelect();
}

function buildRowEl(i) {
  const row = rows[i];
  const div = document.createElement('div');
  div.className = 'row' +
    (i === focusedRow ? ' focused' : '') +
    (row.expr === ''  ? ' empty'   : '') +
    (row.wrapped      ? ' wrapped' : '') +
    (row.showResult === false ? ' no-result' : '');
  div.dataset.row = i;

  // 式列は常に静的ハイライト。フォーカス行は CSS で visibility:hidden され、
  // その上に floatEditor が重なる。
  const exprCell = document.createElement('div');
  exprCell.className = 'expr-hl';
  exprCell.innerHTML = highlight(row.expr) || '&ZeroWidthSpace;';
  div.appendChild(exprCell);

  // ---- = 列・結果列 (show_result が false の場合は非表示) ----
  if (row.showResult !== false) {
    const eqCell = document.createElement('div');
    eqCell.className = 'eq-cell';
    eqCell.textContent = '=';
    div.appendChild(eqCell);

    const resCell = document.createElement('div');
    resCell.className = 'result-cell' + (row.error ? ' error' : '');
    resCell.dataset.result = i;
    resCell.tabIndex = -1;  // フォーカス可能 (Tab で移動できる)
    applyResultToCell(resCell, row);
    div.appendChild(resCell);
  }

  return div;
}

// フォーカス行の .expr-hl の位置・サイズを読んで floatEditor を重ねる。
// getBoundingClientRect でビューポート座標を取り、#sheet 基準 + scrollTop で
// #sheet の絶対座標空間に変換する (offsetTop は grid 子で不安定な環境がある)。
function positionFloatingEditor() {
  if (focusedRow < 0 || focusedRow >= rows.length) {
    floatEditor.style.display = 'none';
    positionRowActionBar();
    return;
  }
  const rowEl = sheetEl.querySelector(`[data-row="${focusedRow}"]`);
  if (!rowEl) { floatEditor.style.display = 'none'; positionRowActionBar(); return; }
  const exprEl = rowEl.querySelector('.expr-hl');
  if (!exprEl) { floatEditor.style.display = 'none'; positionRowActionBar(); return; }

  const sheetRect = sheetEl.getBoundingClientRect();
  const exprRect  = exprEl.getBoundingClientRect();
  floatEditor.style.display = '';
  floatEditor.style.top    = `${exprRect.top  - sheetRect.top  + sheetEl.scrollTop }px`;
  floatEditor.style.left   = `${exprRect.left - sheetRect.left + sheetEl.scrollLeft}px`;
  floatEditor.style.width  = `${exprRect.width }px`;
  floatEditor.style.height = `${exprRect.height}px`;
  positionRowActionBar();
}

// L/R コピーボタンの位置を更新。フォーカス行の「直下」(= 1 行ぶん下) に浮かべる。
// 次の行がある場合はその右側に重なり、最下段の場合は #sheet の padding-bottom
// (= var(--row-h)) が確保しているバッファ領域に着地する。
function positionRowActionBar() {
  const bar = document.getElementById('row-action-bar');
  if (!bar) return;
  if (focusedRow < 0 || focusedRow >= rows.length) { bar.hidden = true; return; }
  const rowEl = sheetEl.querySelector(`[data-row="${focusedRow}"]`);
  if (!rowEl) { bar.hidden = true; return; }
  const sheetRect = sheetEl.getBoundingClientRect();
  const rowRect   = rowEl.getBoundingClientRect();
  bar.hidden = false;
  bar.style.top = `${rowRect.bottom - sheetRect.top + sheetEl.scrollTop}px`;
  // right: 0 は CSS 側で設定済み
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
  // showResult が変化した行がある場合は DOM 再構築 (floatInput は DOM に残るのでフォーカス維持)
  for (let i = 0; i < rows.length; i++) {
    const cell = sheetEl.querySelector(`[data-result="${i}"]`);
    if (!!cell !== (rows[i].showResult !== false)) {
      renderAll();
      return;
    }
  }

  // updateLayout を先に走らせて row.wrapped を新しい値に更新してから
  // クラスをトグルする。逆順にすると 1 サイクル遅れて wrapped 表示がずれる
  // (IME 変換中は input が抑止されているので confirm 時に 1 サイクル分見切れる)。
  updateLayout();

  for (let i = 0; i < rows.length; i++) {
    const row  = rows[i];
    const cell = sheetEl.querySelector(`[data-result="${i}"]`);
    if (!cell) continue;
    applyResultToCell(cell, row);
    cell.className = 'result-cell' + (row.error ? ' error' : '');
    const rowEl = cell.closest('.row');
    if (rowEl) {
      rowEl.classList.toggle('empty',     row.expr === '');
      rowEl.classList.toggle('wrapped',   !!row.wrapped);
      rowEl.classList.toggle('no-result', row.showResult === false);
    }
  }

  // wrapped / no-result / 列幅変化に追従して floatEditor を再配置
  positionFloatingEditor();
}

// ---- フォーカス管理 ----

// floatOverlay を floatInput のスクロール位置に合わせる。
// floatInput はテキスト透明なので overlay がハイライト済み表示を担う。
function syncOverlay() {
  floatOverlay.innerHTML = highlight(floatInput.value) || '&ZeroWidthSpace;';
  floatOverlay.style.transform = `translateX(${-floatInput.scrollLeft}px)`;
}

function focusInput(selectAll = false) {
  requestAnimationFrame(() => {
    floatInput.focus();
    if (selectAll) {
      floatInput.select();
    } else {
      floatInput.setSelectionRange(floatInput.value.length, floatInput.value.length);
    }
    const rowEl = sheetEl.querySelector(`[data-row="${focusedRow}"]`);
    rowEl?.scrollIntoView({ block: 'nearest' });
    syncOverlay();
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

// 現在行の floatInput 値を rows に書き戻す。差分があれば undo エントリを作る。
// native: SheetView::commit() 相当 — original_expr_ と比較し、差分があれば
// ChangeExpr の UndoEntry を1件 commit する。
function commitCurrentInput() {
  if (focusedRow < 0 || focusedRow >= rows.length) return;
  const newExpr = floatInput.value;
  if (newExpr !== originalExpr) {
    rows[focusedRow].expr = newExpr;
    originalExpr = newExpr;  // 二重登録防止
    pushUndo();
  } else {
    rows[focusedRow].expr = newExpr;
  }
}

// 行構造を変えない軽量フォーカス移動 (renderAll を呼ばない)。
// 矢印キーやタップで行を跨ぐときに使う。floatInput を触らないので
// ソフトキーボードが維持される。
function moveFocus(newRow) {
  if (newRow < 0 || newRow >= rows.length || newRow === focusedRow) return;
  commitCurrentInput();
  const oldIdx = focusedRow;
  const oldEl  = sheetEl.querySelector(`[data-row="${oldIdx}"]`);
  if (oldEl) {
    oldEl.classList.remove('focused');
    oldEl.classList.toggle('empty', rows[oldIdx].expr === '');
    // 直前まで隠されていた静的 .expr-hl にコミット済みの式を書き戻す
    const exprHl = oldEl.querySelector('.expr-hl');
    if (exprHl) exprHl.innerHTML = highlight(rows[oldIdx].expr) || '&ZeroWidthSpace;';
  }
  focusedRow = newRow;
  let newEl = sheetEl.querySelector(`[data-row="${focusedRow}"]`);
  newEl?.classList.add('focused');
  floatInput.value = rows[focusedRow].expr;
  originalExpr = floatInput.value;  // 新しい編集セッションの比較基準
  floatInput.setSelectionRange(floatInput.value.length, floatInput.value.length);
  // 旧行のコミット値で結果が変わる可能性があるので再評価。
  // evalAll → updateResultCells → updateLayout → positionFloatingEditor
  // (showResult 変化で renderAll に飛ぶケースあり、その場合 newEl は無効化する)
  evalAll();
  newEl = sheetEl.querySelector(`[data-row="${focusedRow}"]`);
  syncOverlay();
  newEl?.scrollIntoView({ block: 'nearest' });
  syncFmtSelect();
  floatInput.focus();
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

// IME 変換中はオーバーレイを隠して <input> 自身のテキストを表示する。
// Android Chrome では変換中に input.value が placeholder (スペース等) を返すため
// highlight() 経由でオーバーレイを描画すると文字化け + scrollLeft 追従で破綻する。
let imeComposing = false;

sheetEl.addEventListener('compositionstart', e => {
  if (e.target !== floatInput) return;
  imeComposing = true;
  const rowEl = sheetEl.querySelector(`[data-row="${focusedRow}"]`);
  rowEl?.classList.add('composing');
  floatEditor.classList.add('composing');
  // .composing により .expr-hl が全幅に広がるので位置を再計算
  positionFloatingEditor();
});
sheetEl.addEventListener('compositionend', e => {
  if (e.target !== floatInput) return;
  imeComposing = false;
  const rowEl = sheetEl.querySelector(`[data-row="${focusedRow}"]`);
  // Android Chrome では compositionend 時点で input.value が未確定なことがあり、
  // かつ後続の input (isComposing=false) が不発なケースもある。
  // setTimeout(0) で次tick まで待ってから row.expr を更新・再評価する。
  setTimeout(() => {
    rows[focusedRow].expr = floatInput.value;
    syncOverlay();
    rowEl?.classList.remove('composing');
    floatEditor.classList.remove('composing');
    evalAll();
    positionFloatingEditor();
    updateUndoButtons();
  }, 0);
});

sheetEl.addEventListener('input', e => {
  if (!e.target.classList.contains('expr-input')) return;
  if (e.isComposing || imeComposing) return;   // IME 変換中は compositionend 側で処理
  rows[focusedRow].expr = e.target.value;
  syncOverlay();
  evalAll();
  updateUndoButtons();
});

sheetEl.addEventListener('keydown', e => {
  if (e.target.classList.contains('expr-input')) {
    handleKeyDown(e);
    // カーソル移動後に rAF でオーバーレイを同期 (IME 変換中は不要・有害)
    if (!imeComposing) requestAnimationFrame(syncOverlay);
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
    moveFocus(i);
  }
  // 結果セル / floatInput 自体のクリックはブラウザに任せる
  if (e.target !== floatInput && !e.target.classList.contains('result-cell')) {
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
  if (ctrl && !shift && key === 'z') {
    e.preventDefault(); e.stopPropagation();
    commitCurrentInput();  // 差分があれば commitCurrentInput 内で pushUndo される
    undo();
    return;
  }
  if (ctrl && (key === 'y' || (shift && key === 'z'))) { e.preventDefault(); e.stopPropagation(); redo(); return; }

  // Tab: 左辺 → 右辺へ (native: focus_result)
  if (key === 'Tab' && !shift && !ctrl) {
    e.preventDefault();
    const row = rows[focusedRow];
    if (row.result && !row.error && row.showResult !== false) {
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

  // Shift+Tab: 前行の右辺 (なければ左辺) へ (Tab の逆方向)
  if (key === 'Tab' && shift && !ctrl) {
    e.preventDefault();
    if (focusedRow <= 0) return;
    const prev = rows[focusedRow - 1];
    const prevHasResult = prev.result && !prev.error && prev.showResult !== false;
    moveFocus(focusedRow - 1);
    if (prevHasResult) {
      const resCell = sheetEl.querySelector(`[data-result="${focusedRow}"]`);
      if (resCell) {
        resCell.focus();
        const sel = window.getSelection();
        const range = document.createRange();
        range.selectNodeContents(resCell);
        sel.removeAllRanges();
        sel.addRange(range);
      }
    }
    return;
  }

  // Enter: 現在行の下に新規行を挿入 (native: insert_row + ans プリフィル)
  if (key === 'Enter' && !ctrl && !shift) {
    e.preventDefault();
    commitCurrentInput();
    const cur = rows[focusedRow];
    const hasResult = cur && cur.result && !cur.error && cur.showResult !== false;
    rows.splice(focusedRow + 1, 0, { expr: hasResult ? 'ans' : '', result: '', error: false });
    focusedRow++;
    pushUndo();
    renderAll();
    focusInput(hasResult);  // ans 挿入時は全選択 (native: insert_position(3,0))
    if (hasResult) evalAll();
    return;
  }

  // Shift+Enter: 上に挿入
  if (key === 'Enter' && shift) {
    e.preventDefault();
    commitCurrentInput();
    rows.splice(focusedRow, 0, { expr: '', result: '', error: false });
    pushUndo();
    renderAll();
    focusInput();
    evalAll();
    return;
  }

  // Shift+Delete / Shift+Backspace: 行削除
  if (shift && !ctrl && (key === 'Delete' || key === 'Backspace')) {
    e.preventDefault();
    commitCurrentInput();
    deleteCurrentRow(key === 'Delete');
    pushUndo();
    return;
  }

  // Ctrl+Delete: 次の行を削除
  if (ctrl && !shift && key === 'Delete') {
    e.preventDefault();
    commitCurrentInput();
    if (focusedRow < rows.length - 1) {
      rows.splice(focusedRow + 1, 1);
      pushUndo();
      renderAll();
      focusInput();
      evalAll();
    }
    return;
  }

  // Backspace on empty: 上の行へ移動して削除
  if (!ctrl && !shift && key === 'Backspace' && e.target.value === '') {
    e.preventDefault();
    deleteCurrentRow(false);
    pushUndo();
    return;
  }

  // 上下矢印: 行移動 (軽量版・renderAll しない)
  if (!ctrl && !shift && (key === 'ArrowUp' || key === 'ArrowDown')) {
    e.preventDefault();
    const target = focusedRow + (key === 'ArrowUp' ? -1 : 1);
    moveFocus(target);
    return;
  }

  // Ctrl+Shift+Up/Down: 行の並び替え
  if (ctrl && shift && (key === 'ArrowUp' || key === 'ArrowDown')) {
    e.preventDefault();
    commitCurrentInput();
    const target = focusedRow + (key === 'ArrowUp' ? -1 : 1);
    if (target >= 0 && target < rows.length) {
      [rows[focusedRow], rows[target]] = [rows[target], rows[focusedRow]];
      focusedRow = target;
      pushUndo();
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
      rows.push({ expr: '', result: '', error: false });
      pushUndo();
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

// 複数行テキストを PasteDialog で整形し、現在行を起点に挿入する。
// 入力 event 経由 (handlePaste) と Paste ボタン経由の両方から呼ばれる。
async function openPasteDialogAndApply(text) {
  const lines = await pasteDialog.open(text);
  if (!lines || lines.length === 0) return;
  commitCurrentInput();
  rows[focusedRow].expr = lines[0];
  for (let i = 1; i < lines.length; i++) {
    rows.splice(focusedRow + i, 0, { expr: lines[i], result: '', error: false });
  }
  focusedRow = focusedRow + lines.length - 1;
  pushUndo();
  renderAll();
  focusInput();
  evalAll();
}

async function handlePaste(e) {
  const text = e.clipboardData?.getData('text') ?? '';
  if (!text.includes('\n') && !text.includes('\r')) return;
  e.preventDefault();
  await openPasteDialogAndApply(text);
}

// ---- ファイル操作 ----

document.getElementById('btn-new').addEventListener('click', () => {
  commitCurrentInput();
  if (rows.length === 1 && rows[0].expr === '') return;
  rows = [{ expr: '', result: '', error: false }];
  focusedRow = 0;
  fileName = null;
  wasmBroken = false;
  pushUndo();
  renderAll();
  focusInput();
  evalAll();
});

// ---- About ----

{
  const aboutDlg = document.getElementById('about-dialog');
  document.getElementById('about-version').textContent = CALCYX_VERSION;
  document.getElementById('btn-about').addEventListener('click', () => {
    aboutDlg.showModal();
  });
  document.getElementById('about-close').addEventListener('click', () => {
    aboutDlg.close();
    focusInput();
  });
}

// ---- Open ドロップダウン ----

const openPanel = document.getElementById('open-panel');

function openDropdownShow() {
  openPanel.hidden = false;
}
function openDropdownHide() {
  openPanel.hidden = true;
  focusInput();
}

document.getElementById('btn-open').addEventListener('click', e => {
  e.stopPropagation();
  openPanel.hidden ? openDropdownShow() : openDropdownHide();
});

// パネル外クリックで閉じる
document.addEventListener('click', e => {
  if (!openPanel.hidden && !openPanel.contains(e.target))
    openDropdownHide();
});

// Upload...
document.getElementById('dd-upload').addEventListener('click', () => {
  openDropdownHide();
  document.getElementById('file-input').click();
});

document.getElementById('file-input').addEventListener('change', async e => {
  const file = e.target.files[0];
  if (!file) return;
  const text = await file.text();
  e.target.value = '';
  commitCurrentInput();
  const lines = text.replace(/\r\n|\r/g, '\n').split('\n');
  while (lines.length > 1 && lines[lines.length - 1] === '') lines.pop();
  rows = lines.map(l => ({ expr: l, result: '', error: false }));
  if (rows.length === 0) rows = [{ expr: '', result: '', error: false }];
  focusedRow = 0;
  fileName = file.name;
  // native: load_file() は undo_buf_.clear() する。ロードを undo 対象にしない。
  undoStack.length = 0;
  undoPos = -1;
  pushUndo();
  renderAll();
  focusInput();
  evalAll();
});

// manifest.json を取得して samples セクションを動的構築
(async () => {
  try {
    const res = await fetch(`samples/manifest.json?v=${CACHE_BUST}`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const files = await res.json();
    const container = document.getElementById('dd-samples');
    for (const filename of files) {
      const item = document.createElement('div');
      item.className = 'dropdown-item';
      item.textContent = filename.replace(/\.txt$/i, '');
      item.addEventListener('click', async () => {
        openDropdownHide();
        try {
          const r = await fetch(`samples/${filename}?v=${CACHE_BUST}`);
          if (!r.ok) throw new Error(`HTTP ${r.status}`);
          const text = await r.text();
          commitCurrentInput();
          const lines = text.replace(/\r\n|\r/g, '\n').split('\n');
          while (lines.length > 1 && lines[lines.length - 1].trim() === '') lines.pop();
          rows = lines.map(l => ({ expr: l, result: '', error: false }));
          if (rows.length === 0) rows = [{ expr: '', result: '', error: false }];
          focusedRow = 0;
          fileName = filename;
          // native: load_file() は undo_buf_.clear() する。ロードを undo 対象にしない。
          undoStack.length = 0;
          undoPos = -1;
          pushUndo();
          renderAll();
          focusInput();
          evalAll();
        } catch (err) {
          console.error('[calcyx] サンプルロード失敗:', filename, err);
        }
      });
      container.appendChild(item);
    }
  } catch (err) {
    console.warn('[calcyx] manifest.json 取得失敗 (ローカル環境では正常):', err.message);
  }
})();

// ---- Undo/Redo ボタン ----

document.getElementById('btn-undo').addEventListener('click', () => { commitCurrentInput(); undo(); });
document.getElementById('btn-redo').addEventListener('click', () => { commitCurrentInput(); redo(); });

// ---- Save ドロップダウン (Save to file / Copy all to clipboard) ----

const savePanel = document.getElementById('save-panel');

function saveDropdownShow() { savePanel.hidden = false; }
function saveDropdownHide() { savePanel.hidden = true; focusInput(); }

document.getElementById('btn-save').addEventListener('click', e => {
  e.stopPropagation();
  savePanel.hidden ? saveDropdownShow() : saveDropdownHide();
});
document.addEventListener('click', e => {
  if (!savePanel.hidden && !savePanel.contains(e.target))
    saveDropdownHide();
});

function serializeAllRows() {
  return rows.map(r => {
    if (r.result && !r.error && r.showResult !== false) {
      return `${r.expr} = ${r.result}`;
    }
    return r.expr;
  }).join('\n');
}

document.getElementById('dd-save-file').addEventListener('click', () => {
  saveDropdownHide();
  commitCurrentInput();
  const text = serializeAllRows();
  const blob = new Blob([text + '\n'], { type: 'text/plain;charset=utf-8' });
  const url  = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = fileName || 'calcyx.txt';
  document.body.appendChild(a);
  a.click();
  a.remove();
  URL.revokeObjectURL(url);
});

document.getElementById('dd-copy-all').addEventListener('click', async () => {
  saveDropdownHide();
  commitCurrentInput();
  try {
    await navigator.clipboard.writeText(serializeAllRows());
  } catch (err) {
    console.error('[calcyx] クリップボードへのコピー失敗:', err);
  }
  focusInput();
});

// ---- 行単位の左辺/右辺コピー (フォーカス行の右端に浮かぶボタン) ----

async function writeClipboard(text) {
  try { await navigator.clipboard.writeText(text); }
  catch (err) { console.error('[calcyx] クリップボードへのコピー失敗:', err); }
}

// mousedown で preventDefault しないと floatInput のフォーカスが外れ、
// スマホではソフトキーボードが一瞬消える。
for (const id of ['btn-copy-lhs', 'btn-copy-rhs']) {
  document.getElementById(id).addEventListener('mousedown', e => e.preventDefault());
}

document.getElementById('btn-copy-lhs').addEventListener('click', async () => {
  commitCurrentInput();
  await writeClipboard(rows[focusedRow]?.expr ?? '');
  focusInput();
});

document.getElementById('btn-copy-rhs').addEventListener('click', async () => {
  const row = rows[focusedRow];
  if (!row) return;
  const text = (row.result && !row.error && row.showResult !== false) ? row.result : '';
  await writeClipboard(text);
  focusInput();
});

// ---- Paste ボタン ----

document.getElementById('btn-paste').addEventListener('click', async () => {
  let text;
  try {
    text = await navigator.clipboard.readText();
  } catch (err) {
    console.error('[calcyx] クリップボードの読み取り失敗:', err);
    return;
  }
  if (!text) return;
  if (text.includes('\n') || text.includes('\r')) {
    await openPasteDialogAndApply(text);
    return;
  }
  // 単一行: floatInput のカーソル位置に挿入
  const start = floatInput.selectionStart ?? floatInput.value.length;
  const end   = floatInput.selectionEnd   ?? floatInput.value.length;
  floatInput.value = floatInput.value.slice(0, start) + text + floatInput.value.slice(end);
  floatInput.selectionStart = floatInput.selectionEnd = start + text.length;
  floatInput.dispatchEvent(new Event('input', { bubbles: true }));
  focusInput();
});

// ---- グローバルキーボードショートカット ----

document.addEventListener('keydown', e => {
  const ctrl = e.ctrlKey || e.metaKey;
  if (!ctrl) return;
  // expr-input にフォーカスがある場合は handleKeyDown が処理済み (stopPropagation 済み)
  const fromInput = e.target.classList.contains('expr-input');
  if (e.key === 'z' && !e.shiftKey) { e.preventDefault(); if (!fromInput) { commitCurrentInput(); undo(); } }
  if (e.key === 'y' || (e.key === 'z' && e.shiftKey)) { e.preventDefault(); if (!fromInput) redo(); }
  if (e.key === 'o') { e.preventDefault(); document.getElementById('btn-open').click(); }
  if (e.key === 's') { e.preventDefault(); document.getElementById('btn-save').click(); }
});

// ---- 初期化 ----

pushUndo();
renderAll();
focusInput();
