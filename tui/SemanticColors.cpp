/* tui_color_source = semantic 用の色名 ↔ FTXUI Color enum マッピング。 */

#include "SemanticColors.h"

#include <cstring>

namespace calcyx::tui {

using ftxui::Color;

/* ANSI 0-7 = 通常 8 色 (`white` は ftxui::Color::GrayLight = ANSI 7)、
 * ANSI 8-15 = light 8 色 (`white-light` = ftxui::Color::White = ANSI 15)。
 * `default` は端末既定色 (前景色 / 背景色は描画コンテキスト次第)。 */
const SemanticColorChoice kSemanticColors[] = {
    { "default",       Color::Default      },
    { "black",         Color::Black        },
    { "red",           Color::Red          },
    { "green",         Color::Green        },
    { "yellow",        Color::Yellow       },
    { "blue",          Color::Blue         },
    { "magenta",       Color::Magenta      },
    { "cyan",          Color::Cyan         },
    { "white",         Color::GrayLight    },
    { "black-light",   Color::GrayDark     },
    { "red-light",     Color::RedLight     },
    { "green-light",   Color::GreenLight   },
    { "yellow-light",  Color::YellowLight  },
    { "blue-light",    Color::BlueLight    },
    { "magenta-light", Color::MagentaLight },
    { "cyan-light",    Color::CyanLight    },
    { "white-light",   Color::White        },
};
const int kSemanticColorCount =
    (int)(sizeof(kSemanticColors) / sizeof(kSemanticColors[0]));

ftxui::Color parse_semantic_color(const std::string &name,
                                  ftxui::Color       fallback) {
    for (int i = 0; i < kSemanticColorCount; i++) {
        if (name == kSemanticColors[i].name)
            return kSemanticColors[i].color;
    }
    return fallback;
}

} // namespace calcyx::tui
