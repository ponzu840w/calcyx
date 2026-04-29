// クラッシュ時のレポート書き出し。
// ハンドラ内ではヒープ・C++ 例外・stdio を避け、低レベル I/O のみ。

#include "crash_handler.h"
#include "app_prefs.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <csignal>
#include <cstdint>
#include <exception>

#if defined(_WIN32)
#  include <windows.h>
#  include <dbghelp.h>
#  include <io.h>
#  include <fcntl.h>
/* Windows では UTF-8 → UTF-16 変換した s_crash_path_w を使って _wopen.
 * シグナルハンドラ内では malloc を避け、 起動時に変換して保持しておく。 */
#  define OPEN(p)  _wopen(s_crash_path_w, _O_WRONLY | _O_CREAT | _O_TRUNC, 0644)
#  define WRITE(fd, buf, n) _write(fd, buf, (unsigned)(n))
#  define CLOSE(fd) _close(fd)
#else
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/utsname.h>
#  if defined(__APPLE__)
#    include <execinfo.h>
#    include <mach-o/dyld.h>
#  elif defined(__linux__)
#    include <execinfo.h>
#  endif
#  define OPEN(p)  open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644)
#  define WRITE(fd, buf, n) write(fd, buf, n)
#  define CLOSE(fd) close(fd)
#endif

#include "path_utf8.h"

#ifndef CALCYX_VERSION_FULL
#  define CALCYX_VERSION_FULL "unknown"
#endif

static char s_crash_path[1024];
static char s_exe_path[1024];
#if defined(_WIN32)
/* シグナルハンドラ内 malloc を避けるため起動時に UTF-16 で保持。 */
static wchar_t s_crash_path_w[1024];
#endif

#define SHEET_SNAPSHOT_SIZE 4096
static char s_sheet_snapshot[SHEET_SNAPSHOT_SIZE];

#define CONFIG_SNAPSHOT_SIZE 1024
static char s_config_snapshot[CONFIG_SNAPSHOT_SIZE];

// ---- async-signal-safe な書き出しユーティリティ ----
static void wr(int fd, const char *s) {
    if (!s) return;
    if (WRITE(fd, s, (int)strlen(s))) {}
}

static void wr_ptr(int fd, const void *p) {
    unsigned long long v = (unsigned long long)(uintptr_t)p;
    char buf[20];
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 15; i >= 0; i--) {
        buf[2 + (15 - i)] = "0123456789abcdef"[(v >> (i * 4)) & 0xf];
    }
    buf[18] = '\0';
    wr(fd, buf);
}

static void wr_int(int fd, int val) {
    char buf[16];
    int pos = 14;
    buf[15] = '\0';
    bool neg = val < 0;
    unsigned int uv = neg ? (unsigned int)(-(val + 1)) + 1u : (unsigned int)val;
    if (uv == 0) buf[pos--] = '0';
    else while (uv > 0) { buf[pos--] = '0' + (uv % 10); uv /= 10; }
    if (neg) buf[pos--] = '-';
    wr(fd, buf + pos + 1);
}

// ---- OS 情報の書き出し ----
static void wr_os_info(int fd) {
#if defined(_WIN32)
    wr(fd, "platform: Windows");
    typedef LONG (WINAPI *RtlGetVersionPtr)(OSVERSIONINFOEXW *);
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll) {
        RtlGetVersionPtr fn = (RtlGetVersionPtr)GetProcAddress(ntdll, "RtlGetVersion");
        if (fn) {
            OSVERSIONINFOEXW ovi;
            memset(&ovi, 0, sizeof(ovi));
            ovi.dwOSVersionInfoSize = sizeof(ovi);
            if (fn(&ovi) == 0) {
                wr(fd, " ");
                wr_int(fd, (int)ovi.dwMajorVersion);
                wr(fd, ".");
                wr_int(fd, (int)ovi.dwMinorVersion);
                wr(fd, ".");
                wr_int(fd, (int)ovi.dwBuildNumber);
            }
        }
    }
    wr(fd, "\n");
#else
    struct utsname u;
    if (uname(&u) == 0) {
        wr(fd, "platform: ");
        wr(fd, u.sysname); wr(fd, " "); wr(fd, u.release);
        wr(fd, " ("); wr(fd, u.machine); wr(fd, ")\n");
    } else {
#  if defined(__APPLE__)
        wr(fd, "platform: macOS\n");
#  else
        wr(fd, "platform: Linux\n");
#  endif
    }
#endif
}

// ---- コンパイラ情報 ----
static void wr_compiler_info(int fd) {
    wr(fd, "compiler: ");
#if defined(__clang__)
    wr(fd, "clang " __clang_version__);
#elif defined(__GNUC__)
    wr(fd, "gcc " __VERSION__);
#elif defined(_MSC_VER)
    wr(fd, "MSVC");
#else
    wr(fd, "unknown");
#endif
    wr(fd, "\n");
}

// ---- 共通: ヘッダ書き出し ----
static int open_crash_log(const char *reason) {
    if (s_crash_path[0] == '\0') return -1;
    int fd = OPEN(s_crash_path);
    if (fd < 0) return -1;

    wr(fd, "calcyx crash report\n");
    wr(fd, "version:  " CALCYX_VERSION_FULL "\n");

    time_t now = time(nullptr);
    struct tm *tm = localtime(&now);
    if (tm) {
        char timebuf[64];
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm);
        wr(fd, "time:     "); wr(fd, timebuf); wr(fd, "\n");
    }

    wr(fd, "signal:   "); wr(fd, reason); wr(fd, "\n");
    wr_os_info(fd);
    wr_compiler_info(fd);
    wr(fd, "exe:      "); wr(fd, s_exe_path); wr(fd, "\n");

    return fd;
}

// ---- スタックトレースの書き出し ----
#if defined(_WIN32)
static void wr_stack_trace_win(int fd, void **frames, int n) {
    HANDLE proc = GetCurrentProcess();
    SymInitialize(proc, NULL, TRUE);

    union {
        SYMBOL_INFO info;
        char buf[sizeof(SYMBOL_INFO) + 256];
    } sym;

    for (int i = 0; i < n; i++) {
        wr(fd, "#"); wr_int(fd, i); wr(fd, " ");
        wr_ptr(fd, frames[i]);

        memset(&sym, 0, sizeof(sym));
        sym.info.SizeOfStruct = sizeof(SYMBOL_INFO);
        sym.info.MaxNameLen = 255;
        DWORD64 disp = 0;
        if (SymFromAddr(proc, (DWORD64)(uintptr_t)frames[i], &disp, &sym.info)) {
            wr(fd, " ");
            wr(fd, sym.info.Name);
            wr(fd, "+"); wr_ptr(fd, (void *)(uintptr_t)disp);
        }

        HMODULE mod = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)frames[i], &mod)) {
            char mod_name[MAX_PATH];
            if (GetModuleFileNameA(mod, mod_name, sizeof(mod_name))) {
                const char *slash = strrchr(mod_name, '\\');
                wr(fd, " [");
                wr(fd, slash ? slash + 1 : mod_name);
                wr(fd, "+"); wr_ptr(fd, (void *)((uintptr_t)frames[i] - (uintptr_t)mod));
                wr(fd, "]");
            }
        }
        wr(fd, "\n");
    }

    SymCleanup(proc);
}
#endif

static void wr_stack_trace(int fd) {
#if !defined(_WIN32) && (defined(__APPLE__) || defined(__linux__))
    void *frames[64];
    int n = backtrace(frames, 64);
    char **syms = backtrace_symbols(frames, n);
    for (int i = 0; i < n; i++) {
        wr(fd, "#"); wr_int(fd, i); wr(fd, " ");
        if (syms) wr(fd, syms[i]);
        else wr_ptr(fd, frames[i]);
        wr(fd, "\n");
    }
    if (syms) free(syms);
#elif defined(_WIN32)
    void *frames[64];
    USHORT n = CaptureStackBackTrace(0, 64, frames, NULL);
    wr_stack_trace_win(fd, frames, n);
#endif
}

// ---- シート内容の書き出し ----
static void wr_sheet_snapshot(int fd) {
    if (s_sheet_snapshot[0] == '\0') return;
    wr(fd, "\n--- sheet content ---\n");
    wr(fd, s_sheet_snapshot);
    wr(fd, "\n");
}

static void wr_config_snapshot(int fd) {
    if (s_config_snapshot[0] == '\0') return;
    wr(fd, "\n--- config ---\n");
    wr(fd, s_config_snapshot);
    wr(fd, "\n");
}

static void close_crash_log(int fd) {
    wr_config_snapshot(fd);
    wr_sheet_snapshot(fd);
    wr(fd, "\n--- end ---\n");
    CLOSE(fd);
}

static void relaunch_with_crash_dialog() {
    if (s_exe_path[0] == '\0' || s_crash_path[0] == '\0') return;
#if defined(_WIN32)
    char cmdline[2200];
    snprintf(cmdline, sizeof(cmdline), "\"%s\" --show-crash \"%s\"",
             s_exe_path, s_crash_path);
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
#else
    pid_t pid = fork();
    if (pid == 0) {
        execl(s_exe_path, s_exe_path, "--show-crash", s_crash_path, (char *)NULL);
        _exit(127);
    }
#endif
}

// --- シグナルハンドラ (全プラットフォーム共通) ---
static void signal_handler(int sig) {
    const char *name = "UNKNOWN";
    switch (sig) {
        case SIGSEGV: name = "SIGSEGV"; break;
        case SIGABRT: name = "SIGABRT"; break;
        case SIGFPE:  name = "SIGFPE";  break;
        case SIGILL:  name = "SIGILL";  break;
#ifdef SIGBUS
        case SIGBUS:  name = "SIGBUS";  break;
#endif
    }

    int fd = open_crash_log(name);
    if (fd >= 0) {
        wr(fd, "\n--- stack trace ---\n");
        wr_stack_trace(fd);
        close_crash_log(fd);
    }

    relaunch_with_crash_dialog();

    signal(sig, SIG_DFL);
    raise(sig);
}

// --- std::terminate ハンドラ (未捕捉 C++ 例外) ---
static void terminate_handler() {
    int fd = open_crash_log("std::terminate (uncaught C++ exception)");
    if (fd >= 0) {
        wr(fd, "\n--- stack trace ---\n");
        wr_stack_trace(fd);
        close_crash_log(fd);
    }
    relaunch_with_crash_dialog();
    abort();
}

// --- Windows SEH (バックアップ) ---
#if defined(_WIN32)
static void wr_registers(int fd, CONTEXT *ctx) {
    wr(fd, "\n--- registers ---\n");
#if defined(_M_X64) || defined(__x86_64__)
    wr(fd, "rax="); wr_ptr(fd, (void *)ctx->Rax);
    wr(fd, " rbx="); wr_ptr(fd, (void *)ctx->Rbx);
    wr(fd, " rcx="); wr_ptr(fd, (void *)ctx->Rcx);
    wr(fd, "\nrdx="); wr_ptr(fd, (void *)ctx->Rdx);
    wr(fd, " rsi="); wr_ptr(fd, (void *)ctx->Rsi);
    wr(fd, " rdi="); wr_ptr(fd, (void *)ctx->Rdi);
    wr(fd, "\nrsp="); wr_ptr(fd, (void *)ctx->Rsp);
    wr(fd, " rbp="); wr_ptr(fd, (void *)ctx->Rbp);
    wr(fd, " rip="); wr_ptr(fd, (void *)ctx->Rip);
    wr(fd, "\nr8 ="); wr_ptr(fd, (void *)ctx->R8);
    wr(fd, " r9 ="); wr_ptr(fd, (void *)ctx->R9);
    wr(fd, " r10="); wr_ptr(fd, (void *)ctx->R10);
    wr(fd, "\nr11="); wr_ptr(fd, (void *)ctx->R11);
    wr(fd, " r12="); wr_ptr(fd, (void *)ctx->R12);
    wr(fd, " r13="); wr_ptr(fd, (void *)ctx->R13);
    wr(fd, "\nr14="); wr_ptr(fd, (void *)ctx->R14);
    wr(fd, " r15="); wr_ptr(fd, (void *)ctx->R15);
    wr(fd, "\n");
#elif defined(_M_IX86) || defined(__i386__)
    wr(fd, "eax="); wr_ptr(fd, (void *)(uintptr_t)ctx->Eax);
    wr(fd, " ebx="); wr_ptr(fd, (void *)(uintptr_t)ctx->Ebx);
    wr(fd, " ecx="); wr_ptr(fd, (void *)(uintptr_t)ctx->Ecx);
    wr(fd, "\nedx="); wr_ptr(fd, (void *)(uintptr_t)ctx->Edx);
    wr(fd, " esi="); wr_ptr(fd, (void *)(uintptr_t)ctx->Esi);
    wr(fd, " edi="); wr_ptr(fd, (void *)(uintptr_t)ctx->Edi);
    wr(fd, "\nesp="); wr_ptr(fd, (void *)(uintptr_t)ctx->Esp);
    wr(fd, " ebp="); wr_ptr(fd, (void *)(uintptr_t)ctx->Ebp);
    wr(fd, " eip="); wr_ptr(fd, (void *)(uintptr_t)ctx->Eip);
    wr(fd, "\n");
#endif
}

static LONG WINAPI seh_handler(EXCEPTION_POINTERS *ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;

    if (code == 0xE06D7363) return EXCEPTION_CONTINUE_SEARCH;

    const char *name = "UNKNOWN_EXCEPTION";
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:      name = "ACCESS_VIOLATION"; break;
        case EXCEPTION_STACK_OVERFLOW:         name = "STACK_OVERFLOW"; break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:    name = "INT_DIVIDE_BY_ZERO"; break;
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:    name = "FLT_DIVIDE_BY_ZERO"; break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:   name = "ILLEGAL_INSTRUCTION"; break;
        case EXCEPTION_IN_PAGE_ERROR:         name = "IN_PAGE_ERROR"; break;
    }

    int fd = open_crash_log(name);
    if (fd >= 0) {
        wr(fd, "exception code: "); wr_ptr(fd, (void *)(uintptr_t)code); wr(fd, "\n");
        wr(fd, "address:        "); wr_ptr(fd, ep->ExceptionRecord->ExceptionAddress); wr(fd, "\n");

        wr_registers(fd, ep->ContextRecord);

        wr(fd, "\n--- stack trace ---\n");
        void *frames[64];
        USHORT n = CaptureStackBackTrace(0, 64, frames, NULL);
        wr_stack_trace_win(fd, frames, n);

        close_crash_log(fd);
    }

    relaunch_with_crash_dialog();
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

// --- public API ---
void crash_handler_install() {
    std::string dir = AppPrefs::config_dir();
#if defined(_WIN32)
    snprintf(s_crash_path, sizeof(s_crash_path), "%s\\crash.log", dir.c_str());
    /* s_crash_path は UTF-8. シグナルハンドラ内で _wopen に渡せるよう
     * UTF-16 版を起動時に作っておく。 */
    MultiByteToWideChar(CP_UTF8, 0, s_crash_path, -1, s_crash_path_w,
                        (int)(sizeof(s_crash_path_w) / sizeof(s_crash_path_w[0])));
    {
        wchar_t wexe[1024];
        if (GetModuleFileNameW(NULL, wexe, (DWORD)(sizeof(wexe) / sizeof(wexe[0]))) > 0) {
            WideCharToMultiByte(CP_UTF8, 0, wexe, -1, s_exe_path,
                                (int)sizeof(s_exe_path), NULL, NULL);
        } else {
            s_exe_path[0] = '\0';
        }
    }
#elif defined(__APPLE__)
    snprintf(s_crash_path, sizeof(s_crash_path), "%s/crash.log", dir.c_str());
    uint32_t sz = sizeof(s_exe_path);
    _NSGetExecutablePath(s_exe_path, &sz);
#else
    snprintf(s_crash_path, sizeof(s_crash_path), "%s/crash.log", dir.c_str());
    ssize_t len = readlink("/proc/self/exe", s_exe_path, sizeof(s_exe_path) - 1);
    if (len > 0) s_exe_path[len] = '\0';
#endif

    signal(SIGSEGV, signal_handler);
    signal(SIGABRT, signal_handler);
    signal(SIGFPE,  signal_handler);
    signal(SIGILL,  signal_handler);
#ifdef SIGBUS
    signal(SIGBUS,  signal_handler);
#endif

    std::set_terminate(terminate_handler);

#if defined(_WIN32)
    SetUnhandledExceptionFilter(seh_handler);
#endif
}

std::string crash_handler_check() {
    if (s_crash_path[0] == '\0') return "";
    FILE *fp = calcyx_fopen(s_crash_path, "r");
    if (!fp) return "";
    std::string content;
    char buf[1024];
    while (size_t n = fread(buf, 1, sizeof(buf), fp))
        content.append(buf, n);
    fclose(fp);
    calcyx_remove(s_crash_path);
    return content;
}

void crash_handler_save_sheet(const char *content) {
    if (!content) { s_sheet_snapshot[0] = '\0'; return; }
    size_t len = strlen(content);
    if (len >= SHEET_SNAPSHOT_SIZE) len = SHEET_SNAPSHOT_SIZE - 1;
    memcpy(s_sheet_snapshot, content, len);
    s_sheet_snapshot[len] = '\0';
}

void crash_handler_save_config(const char *content) {
    if (!content) { s_config_snapshot[0] = '\0'; return; }
    size_t len = strlen(content);
    if (len >= CONFIG_SNAPSHOT_SIZE) len = CONFIG_SNAPSHOT_SIZE - 1;
    memcpy(s_config_snapshot, content, len);
    s_config_snapshot[len] = '\0';
}
