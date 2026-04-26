/* color_presets.c — see color_presets.h */

#include "color_presets.h"

#include <stddef.h>
#include <string.h>

#define RGB(R,G,B) { (unsigned char)(R), (unsigned char)(G), (unsigned char)(B) }

static const calcyx_color_palette_t OTAKU_BLACK = {
    /* bg/sel_bg/rowline/sep */
    RGB( 22,  22,  22), RGB( 38,  42,  55), RGB( 32,  32,  36), RGB( 55,  55,  65),
    /* text/cursor */
    RGB(255, 255, 255), RGB(180, 200, 255),
    /* symbol/ident/special/si_pfx */
    RGB( 64, 192, 255), RGB(192, 255, 128), RGB(255, 192,  64), RGB(224, 160, 255),
    /* paren[4] */
    { RGB( 64, 192, 255), RGB(192, 128, 255), RGB(255, 128, 192), RGB(255, 192,  64) },
    /* error */
    RGB(110, 110, 110),
    /* ui_win_bg/ui_bg/ui_input/ui_btn/ui_menu/ui_text/ui_label */
    RGB( 30,  30,  30), RGB( 38,  38,  43), RGB( 50,  52,  60), RGB( 55,  60,  75),
    RGB( 40,  40,  45), RGB(215, 215, 225), RGB(180, 180, 190),
    /* pop_bg/pop_sel/pop_text/pop_desc/pop_desc_bg/pop_border */
    RGB( 28,  28,  35), RGB( 40,  80, 140), RGB(220, 220, 220), RGB(150, 150, 160),
    RGB( 20,  20,  28), RGB( 80,  80, 100),
};

static const calcyx_color_palette_t GYAKUBARI_WHITE = {
    RGB(250, 250, 250), RGB(210, 220, 240), RGB(230, 230, 232), RGB(200, 200, 210),
    RGB(  0,   0,   0), RGB( 40,  60, 160),
    RGB(  0, 100, 200), RGB( 40, 130,   0), RGB(180, 100,   0), RGB(140,  60, 200),
    { RGB(  0, 100, 200), RGB(140,  60, 200), RGB(200,  50, 120), RGB(180, 100,   0) },
    RGB(160, 160, 160),
    RGB(240, 240, 245), RGB(235, 235, 240), RGB(255, 255, 255), RGB(220, 220, 228),
    RGB(228, 228, 235), RGB( 30,  30,  35), RGB( 80,  80,  90),
    RGB(255, 255, 255), RGB(180, 210, 255), RGB( 20,  20,  25), RGB( 90,  90, 100),
    RGB(242, 242, 248), RGB(180, 180, 200),
};

static const calcyx_color_palette_t SABOTEN_GREY = {
    RGB( 32,  32,  32), RGB(  0,   0,   0), RGB( 40,  40,  40), RGB( 96,  96,  96),
    RGB(255, 255, 255), RGB(  0, 128, 255),
    RGB( 64, 192, 255), RGB(192, 255, 128), RGB(255, 192,  64), RGB(224, 160, 255),
    { RGB( 64, 192, 255), RGB(192, 128, 255), RGB(255, 128, 192), RGB(255, 192,  64) },
    RGB(255, 128, 128),
    RGB( 40,  40,  40), RGB( 48,  48,  48), RGB( 60,  60,  60), RGB( 72,  72,  80),
    RGB( 50,  50,  55), RGB(240, 240, 240), RGB(180, 180, 180),
    RGB( 38,  38,  42), RGB( 50,  85, 140), RGB(230, 230, 230), RGB(150, 150, 155),
    RGB( 28,  28,  32), RGB( 90,  90,  96),
};

static const calcyx_color_palette_t SABOTEN_WHITE = {
    RGB(224, 224, 224), RGB(255, 255, 255), RGB(216, 216, 216), RGB(160, 160, 160),
    RGB(  0,   0,   0), RGB(  0,  80, 160),
    RGB(  0, 120, 192), RGB( 64, 160,   0), RGB(192, 120,   0), RGB(144,  80, 224),
    { RGB(  0, 120, 192), RGB(128,  64, 192), RGB(192,  64, 128), RGB(192, 120,   0) },
    RGB(192,  64,  64),
    RGB(235, 235, 235), RGB(230, 230, 230), RGB(245, 245, 245), RGB(210, 210, 215),
    RGB(220, 220, 225), RGB( 20,  20,  25), RGB( 90,  90, 100),
    RGB(245, 245, 248), RGB(190, 215, 250), RGB( 15,  15,  20), RGB(100, 100, 110),
    RGB(232, 232, 238), RGB(170, 170, 185),
};

static const struct {
    int         id;
    const char *name;
    const calcyx_color_palette_t *pal;
} PRESETS[] = {
    { CALCYX_COLOR_PRESET_OTAKU_BLACK,     "otaku-black",     &OTAKU_BLACK     },
    { CALCYX_COLOR_PRESET_GYAKUBARI_WHITE, "gyakubari-white", &GYAKUBARI_WHITE },
    { CALCYX_COLOR_PRESET_SABOTEN_GREY,    "saboten-grey",    &SABOTEN_GREY    },
    { CALCYX_COLOR_PRESET_SABOTEN_WHITE,   "saboten-white",   &SABOTEN_WHITE   },
    { CALCYX_COLOR_PRESET_USER_DEFINED,    "user-defined",    &OTAKU_BLACK     },
};

#define N_PRESETS ((int)(sizeof(PRESETS) / sizeof(PRESETS[0])))

int calcyx_color_preset_lookup(const char *id) {
    int i;
    if (!id) return -1;
    for (i = 0; i < N_PRESETS; i++) {
        if (strcmp(PRESETS[i].name, id) == 0) return PRESETS[i].id;
    }
    return -1;
}

const char *calcyx_color_preset_id(int preset) {
    int i;
    for (i = 0; i < N_PRESETS; i++) {
        if (PRESETS[i].id == preset) return PRESETS[i].name;
    }
    return NULL;
}

void calcyx_color_preset_get(int preset, calcyx_color_palette_t *out) {
    int i;
    if (!out) return;
    for (i = 0; i < N_PRESETS; i++) {
        if (PRESETS[i].id == preset) {
            *out = *PRESETS[i].pal;
            return;
        }
    }
    *out = OTAKU_BLACK;
}
