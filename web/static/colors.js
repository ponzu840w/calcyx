// colors.js — calcyx UI カラーテーマ (Web 版)
// calctus Settings.Appearance_Color_* に準拠した論理名で管理する。
// 将来の設定ダイアログで colors オブジェクトを書き換え applyColors() を呼ぶことで
// 全 UI に反映できる。C 側の CalcyxColors 構造体と同じ論理名を使用する。

/** デフォルト色 (移植元: Calctus/Settings.cs Appearance_Color_*) */
export const defaultColors = {
  /* 背景 / フレーム */
  bg:        '#161616',
  selBg:     '#262a37',
  rowline:   '#202024',
  sep:       '#373743',

  /* テキスト / カーソル */
  text:      '#ffffff',
  cursor:    '#b4c8ff',

  /* シンタックスハイライト */
  symbol:    '#40c0ff',  /* Appearance_Color_Symbols           */
  ident:     '#c0ff80',  /* Appearance_Color_Identifiers       */
  special:   '#ffc040',  /* Appearance_Color_Special_Literals  */
  siPfx:     '#e0a0ff',  /* Appearance_Color_SI_Prefix         */
  paren:    ['#40c0ff', '#c080ff', '#ff80c0', '#ffc040'],
             /* Appearance_Color_Parenthesis_1..4 */
  error:     '#6e6e6e',  /* Appearance_Color_Error             */
};

/** 現在のカラーテーマ (設定ダイアログから上書き可能) */
export const colors = {
  ...defaultColors,
  paren: [...defaultColors.paren],
};

/**
 * colors の値を CSS 変数に反映する。
 * 初期化時と設定変更時に呼ぶ。
 * @param {typeof colors} c
 */
export function applyColors(c = colors) {
  const s = document.documentElement.style;
  s.setProperty('--bg',        c.bg);
  s.setProperty('--sel-bg',    c.selBg);
  s.setProperty('--row-line',  c.rowline);
  s.setProperty('--sep',       c.sep);
  s.setProperty('--text',      c.text);
  s.setProperty('--cursor',    c.cursor);
  s.setProperty('--symbol',    c.symbol);
  s.setProperty('--ident',     c.ident);
  s.setProperty('--special',   c.special);
  s.setProperty('--si-pfx',    c.siPfx);
  c.paren.forEach((p, i) => s.setProperty(`--paren-${i}`, p));
  s.setProperty('--error',     c.error);
}
