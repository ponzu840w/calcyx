#include "clipboard.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#if defined(_WIN32)
#  define CALCYX_POPEN  _popen
#  define CALCYX_PCLOSE _pclose
#else
#  define CALCYX_POPEN  popen
#  define CALCYX_PCLOSE pclose
#endif

namespace calcyx::tui::clipboard {

namespace {

bool g_mock_on = false;
std::string g_mock_buf;

bool command_exists(const char *cmd) {
#if defined(_WIN32)
    std::string check = std::string("where ") + cmd + " >nul 2>&1";
#else
    std::string check = std::string("command -v ") + cmd + " >/dev/null 2>&1";
#endif
    return std::system(check.c_str()) == 0;
}

bool write_via_command(const char *cmd, const std::string &text) {
    FILE *p = CALCYX_POPEN(cmd, "w");
    if (!p) return false;
    size_t n = std::fwrite(text.data(), 1, text.size(), p);
    int rc = CALCYX_PCLOSE(p);
    return n == text.size() && rc == 0;
}

bool read_via_command(const char *cmd, std::string &out) {
    FILE *p = CALCYX_POPEN(cmd, "r");
    if (!p) return false;
    std::string buf;
    char chunk[4096];
    size_t got;
    while ((got = std::fread(chunk, 1, sizeof(chunk), p)) > 0)
        buf.append(chunk, got);
    int rc = CALCYX_PCLOSE(p);
    if (rc != 0) return false;
    out = std::move(buf);
    return true;
}

std::string base64_encode(const std::string &data) {
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        unsigned a = (unsigned char)data[i];
        unsigned b = i + 1 < data.size() ? (unsigned char)data[i + 1] : 0;
        unsigned c = i + 2 < data.size() ? (unsigned char)data[i + 2] : 0;
        unsigned v = (a << 16) | (b << 8) | c;
        out.push_back(tbl[(v >> 18) & 0x3f]);
        out.push_back(tbl[(v >> 12) & 0x3f]);
        out.push_back(i + 1 < data.size() ? tbl[(v >> 6) & 0x3f] : '=');
        out.push_back(i + 2 < data.size() ? tbl[v & 0x3f]       : '=');
    }
    return out;
}

bool write_via_osc52(const std::string &text) {
    /* OSC 52: ESC ] 52 ; c ; <base64> ESC \
     * stdout に直接書く。FTXUI の描画フレームの隙間で出力するため一瞬
     * 画面が乱れる可能性があるが、次フレームで FTXUI が上書きするので
     * 実害はない。端末がサポートしているか確認する手段がないので常に
     * 真を返す (非対応端末では黙って失敗)。 */
    std::string seq = "\x1b]52;c;";
    seq += base64_encode(text);
    seq += "\x1b\\";
    std::cout << seq;
    std::cout.flush();
    return true;
}

} // namespace

bool write(const std::string &text) {
    if (g_mock_on) { g_mock_buf = text; return true; }

#if defined(__APPLE__)
    if (write_via_command("pbcopy", text)) return true;
#elif defined(_WIN32)
    if (write_via_command("clip", text)) return true;
#else
    /* Wayland 優先、次に X11、最後に WSL */
    if (std::getenv("WAYLAND_DISPLAY") && command_exists("wl-copy"))
        if (write_via_command("wl-copy", text)) return true;
    if (command_exists("xclip"))
        if (write_via_command("xclip -selection clipboard", text)) return true;
    if (command_exists("xsel"))
        if (write_via_command("xsel --clipboard --input", text)) return true;
    if (command_exists("clip.exe"))
        if (write_via_command("clip.exe", text)) return true;
#endif
    return write_via_osc52(text);
}

bool read(std::string &out) {
    if (g_mock_on) { out = g_mock_buf; return true; }

#if defined(__APPLE__)
    return read_via_command("pbpaste", out);
#elif defined(_WIN32)
    return read_via_command("powershell -NoProfile -Command Get-Clipboard", out);
#else
    if (std::getenv("WAYLAND_DISPLAY") && command_exists("wl-paste"))
        if (read_via_command("wl-paste -n", out)) return true;
    if (command_exists("xclip"))
        if (read_via_command("xclip -selection clipboard -o", out)) return true;
    if (command_exists("xsel"))
        if (read_via_command("xsel --clipboard --output", out)) return true;
    if (command_exists("powershell.exe"))
        return read_via_command("powershell.exe -NoProfile -Command Get-Clipboard", out);
    return false;
#endif
}

void set_mock_for_test(bool on) { g_mock_on = on; if (!on) g_mock_buf.clear(); }
void set_mock_buffer(const std::string &t) { g_mock_buf = t; }
const std::string &get_mock_buffer() { return g_mock_buf; }

} // namespace calcyx::tui::clipboard
