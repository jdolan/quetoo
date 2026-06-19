/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


#include "sys.h"
#include "filesystem.h"

#include <errno.h>
#include <signal.h>
#include <time.h>

#if !defined(_WIN32)
  #include <unistd.h>
#endif

#if defined(_WIN32)
  #include <windows.h>
  #include <shlobj.h>
  #include <DbgHelp.h>
#endif

#if defined(__APPLE__)
  #include <mach-o/dyld.h>
#endif

#if HAVE_DLFCN_H
  #include <dlfcn.h>
#endif

#if HAVE_EXECINFO
  #include <execinfo.h>
  #define MAX_BACKTRACE_SYMBOLS 50
#endif

#if HAVE_SYS_TIME_H
  #include <sys/time.h>
#endif

#if !defined(_WIN32)
  #include <fcntl.h>
#endif

#include <SDL3/SDL.h>

/**
 * @return The current executable path (argv[0]).
 */
const char *Sys_ExecutablePath(void) {
  static char path[MAX_OS_PATH];

#if defined(__APPLE__)
  uint32_t i = sizeof(path);

  if (_NSGetExecutablePath(path, &i) > -1) {
    return path;
  }

#elif defined(__linux__)

  if (readlink(va("/proc/%d/exe", getpid()), path, sizeof(path) - 1) > -1) {
    path[sizeof(path) - 1] = '\0';
    return path;
  }

#elif defined(_WIN32)

  if (GetModuleFileName(0, path, sizeof(path))) {
    return path;
  }

#endif

  Com_Warn("Failed to resolve executable path\n");
  return NULL;
}

/**
 * @return The current user's name.
 */
const char *Sys_Username(void) {
  const char *name = getenv("USER");
  if (!name || !*name) { name = getenv("USERNAME"); }
  if (!name || !*name) { name = "unknown"; }
  return name;
}

/**
 * @brief Returns the current user's Quetoo directory.
 *
 * @details Uses `SDL_GetPrefPath`, which yields a platform-conventional
 * per-user, per-app directory:
 *   - Windows: `%APPDATA%\WickedOldGames\Quetoo`
 *   - macOS:   `~/Library/Application Support/WickedOldGames/Quetoo`
 *   - Linux:   `$XDG_DATA_HOME/WickedOldGames/Quetoo` (or `~/.local/share/WickedOldGames/Quetoo`)
 */
const char *Sys_UserDir(void) {
  static char user_dir[MAX_OS_PATH];

  if (*user_dir == '\0') {
    char *pref = SDL_GetPrefPath("WickedOldGames", "Quetoo");
    if (pref == NULL) {
      Com_Error(ERROR_FATAL, "SDL_GetPrefPath failed: %s\n", SDL_GetError());
    }

    {
      const size_t pref_len = strlen(pref);
      if (pref_len > 0 && (pref[pref_len - 1] == '/' || pref[pref_len - 1] == '\\')) {
        pref[pref_len - 1] = '\0';
      }
    }

    SDL_strlcpy(user_dir, pref, sizeof(user_dir));
    SDL_free(pref);
  }

  return user_dir;
}

/**
 * @brief Loads a shared library by name, searching the game filesystem for the .so/.dll file.
 * @return A handle to the loaded library, or aborts with `ERROR_DROP` on failure.
 */
void *Sys_OpenLibrary(const char *name, bool global) {

#if defined(_WIN32)
  const char *so_name = va("%s.dll", name);
#else
  const char *so_name = va("%s.so", name);
#endif

  if (Fs_Exists(so_name)) {
    char path[MAX_OS_PATH];

    SDL_snprintf(path, sizeof(path), "%s/%s", Fs_RealDir(so_name), so_name);
    Com_Print("  Loading %s...\n", path);

    void *handle = dlopen(path, RTLD_LAZY | (global ? RTLD_GLOBAL : RTLD_LOCAL));
    if (handle) {
      return handle;
    }

    Com_Error(ERROR_DROP, "%s\n", dlerror());
  }

  Com_Error(ERROR_DROP, "Couldn't find %s\n", so_name);
}

/**
 * @brief Closes an open game module.
 */
void Sys_CloseLibrary(void *handle) {
  dlclose(handle);
}

/**
 * @brief Opens and loads the specified shared library. The function identified by
 * `entry_point` is resolved and invoked with the specified parameters, its
 * return value returned by this function.
 */
void *Sys_LoadLibrary(void *handle, const char *entry_point, void *params) {
  typedef void *EntryPointFunc(void *);
  EntryPointFunc *EntryPoint;

  assert(handle);
  assert(entry_point);

  EntryPoint = (EntryPointFunc *) dlsym(handle, entry_point);
  if (!EntryPoint) {
    Com_Error(ERROR_DROP, "Failed to resolve entry point: %s\n", entry_point);
  }

  return EntryPoint(params);
}

/**
 * @brief On Linux archive installs, writes a .desktop file to
 * ~/.local/share/applications/ so the game appears in the system launcher.
 * No-op for system-managed installs (deb/rpm under /usr/).
 */
#if defined(__linux__)
void Sys_InstallDesktopEntry(void) {

  const char *exe = Sys_ExecutablePath();
  if (!exe) {
    return;
  }

  // Skip system-managed installs; the package manager owns desktop integration.
  if (strncmp(exe, "/usr/", 5) == 0) {
    return;
  }

  // Derive install prefix by stripping /bin/<name>.
  char prefix[MAX_OS_PATH];
  SDL_strlcpy(prefix, exe, sizeof(prefix));
  char *bin = strstr(prefix, "/bin/");
  if (!bin) {
    return;
  }
  *bin = '\0';

  char icon_path[MAX_OS_PATH];
  SDL_snprintf(icon_path, sizeof(icon_path),
    "%s/share/icons/hicolor/256x256/apps/quetoo.png", prefix);

  // Destination: ~/.local/share/applications/quetoo.desktop
  char desktop_dest[MAX_OS_PATH];
  {
    const char *xdg = getenv("XDG_DATA_HOME");
    char data_home[MAX_OS_PATH];
    if (xdg && *xdg) {
      SDL_strlcpy(data_home, xdg, sizeof(data_home));
    } else {
      SDL_snprintf(data_home, sizeof(data_home), "%s/.local/share", getenv("HOME") ? getenv("HOME") : "");
    }
    SDL_snprintf(desktop_dest, sizeof(desktop_dest), "%s/applications/quetoo.desktop", data_home);
  }

  // Skip writing if Exec= already points at this binary (no change needed).
  char expected_exec[MAX_OS_PATH];
  SDL_snprintf(expected_exec, sizeof(expected_exec), "Exec=%s", exe);
  if (SDL_GetPathInfo(desktop_dest, NULL)) {
    FILE *f = fopen(desktop_dest, "r");
    if (f) {
      char contents[4096] = "";
      fread(contents, 1, sizeof(contents) - 1, f);
      fclose(f);
      const bool up_to_date = strstr(contents, expected_exec) != NULL;
      if (up_to_date) {
        return;
      }
    }
  }

  {
    char dir[MAX_OS_PATH];
    SDL_strlcpy(dir, desktop_dest, sizeof(dir));
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; }
    SDL_CreateDirectory(dir);
  }

  char *content = NULL;
  SDL_asprintf(&content,
    "[Desktop Entry]\n"
    "Name=Quetoo\n"
    "Comment=Free, open-source arena first-person shooter\n"
    "Exec=%s %%U\n"
    "Icon=%s\n"
    "Terminal=false\n"
    "Type=Application\n"
    "Categories=Game;ActionGame;\n"
    "MimeType=x-scheme-handler/quetoo;\n"
    "StartupNotify=true\n",
    exe, icon_path);

  FILE *df = fopen(desktop_dest, "w");
  if (df) {
    fputs(content, df);
    fclose(df);
  } else {
    Com_Warn("Failed to install desktop entry: %s\n", desktop_dest);
  }

  free(content);

  // Register quetoo:// URI scheme handler for this user session.
  (void) system("update-desktop-database ~/.local/share/applications/ 2>/dev/null");
}

/**
 * @brief On Linux archive installs, creates symlinks in ~/.local/bin pointing
 * to the game's executables, making them available on the user's $PATH.
 * No-op for system-managed installs (deb/rpm under /usr/).
 */
void Sys_InstallLocalBin(void) {

  static const char *names[] = { "quetoo", "quemap", "quetoo-dedicated", NULL };

  const char *exe = Sys_ExecutablePath();
  if (!exe || strncmp(exe, "/usr/", 5) == 0) {
    return;
  }

  char prefix[MAX_OS_PATH];
  SDL_strlcpy(prefix, exe, sizeof(prefix));
  char *bin = strstr(prefix, "/bin/");
  if (!bin) {
    return;
  }
  *bin = '\0';

  char local_bin[MAX_OS_PATH];
  {
    const char *home = getenv("HOME");
    SDL_snprintf(local_bin, sizeof(local_bin), "%s/.local/bin", home ? home : "");
  }
  SDL_CreateDirectory(local_bin);

  for (const char * const *name = names; *name; name++) {
    char src[MAX_OS_PATH];
    SDL_snprintf(src, sizeof(src), "%s/bin/%s", prefix, *name);

    if (!SDL_GetPathInfo(src, NULL)) {
      continue;
    }

    char dest[MAX_OS_PATH];
    SDL_snprintf(dest, sizeof(dest), "%s/%s", local_bin, *name);

    char current[MAX_OS_PATH] = { 0 };
    const ssize_t rlen = readlink(dest, current, sizeof(current) - 1);
    if (rlen > 0) {
      current[rlen] = '\0';
      if (strcmp(current, src) == 0) {
        continue;
      }
      SDL_RemovePath(dest);
    }

    if (symlink(src, dest) != 0) {
      Com_Warn("Failed to install %s to %s: %s\n", src, dest, strerror(errno));
    }
  }
}
#endif

/**
 * @brief On platforms supporting it, capture a backtrace.
 * @return Heap-allocated string; caller must `free()`.
 * @param start How many frames to skip
 * @param count How many frames total to include
 */
char *Sys_Backtrace(uint32_t start, uint32_t max_count) {
  char buf[8192] = "";

#if HAVE_EXECINFO
  void *symbols[MAX_BACKTRACE_SYMBOLS];
  const int32_t symbol_count = backtrace(symbols, MAX_BACKTRACE_SYMBOLS);

  char **strings = backtrace_symbols(symbols, symbol_count);

  for (uint32_t i = start, s = 0; s < max_count && i < (uint32_t) symbol_count; i++, s++) {
    SDL_strlcat(buf, strings[i], sizeof(buf));
    SDL_strlcat(buf, "\n", sizeof(buf));
  }

  free(strings);
#elif defined(_WIN32)
  static bool symbols_initialized = false;
  void *symbols[32];
  const int name_length = 256;
  
    HANDLE process = GetCurrentProcess();

  if (!symbols_initialized) {
    SymSetOptions(SYMOPT_LOAD_LINES);
    SymInitialize(process, NULL, TRUE);
    symbols_initialized = true;
  }

  const int16_t symbol_count = RtlCaptureStackBackTrace(1, lengthof(symbols), symbols, NULL);

  PSYMBOL_INFO symbol = calloc(sizeof(*symbol) + name_length, 1);
  symbol->MaxNameLen = name_length - 1;
  symbol->SizeOfStruct = sizeof(*symbol);
  
  IMAGEHLP_LINE line;
  line.SizeOfStruct = sizeof(line);
  DWORD dwDisplacement;
  
  for (uint32_t i = start, s = 0; s < max_count && i < (uint32_t) symbol_count; i++, s++) {
    BOOL result = SymFromAddr(process, (DWORD64) symbols[i], 0, symbol);

    if (!result) {
      SDL_strlcat(buf, "> ???\n", sizeof(buf));
      continue;
    }

    // we don't care about UCRT/Windows SDK stuff
    if (!strcmp(symbol->Name, "invoke_main") || !strncmp(symbol->Name, "__scrt_", 7)) {
      break;
    }

    // check for line number support
    if (SymGetLineFromAddr(process, (DWORD64) symbols[i], &dwDisplacement, &line)) {
      char *last_slash = strrchr(line.FileName, '\\');

      if (!last_slash)
        last_slash = strrchr(line.FileName, '/');

      if (!last_slash)
        last_slash = line.FileName;
      else
        last_slash++;

      char frame[512];
      SDL_snprintf(frame, sizeof(frame), "> %s (%s:%i)\n", symbol->Name, last_slash, line.LineNumber);
      SDL_strlcat(buf, frame, sizeof(buf));
    }
    else {
      char frame[512];
      SDL_snprintf(frame, sizeof(frame), "> %s (unknown:unknown)\n", symbol->Name);
      SDL_strlcat(buf, frame, sizeof(buf));
    }
  }

  free(symbol);
#else
  SDL_strlcat(buf, "Backtrace not supported.\n", sizeof(buf));
#endif

  // cut off the last \n
  {
    size_t blen = strlen(buf);
    if (blen > 0 && buf[blen - 1] == '\n') {
      buf[blen - 1] = '\0';
    }
  }

  return SDL_strdup(buf);
}

/**
 * @brief Pre-computed crash log path, populated lazily on first use.
 */
static char sys_crash_log_path[MAX_OS_PATH];

/**
 * @brief Open file descriptor for the crash log, used in signal handlers.
 */
static int sys_crash_log_fd = -1;

/**
 * @brief Ensures `sys_crash_log_path` is set and its directory exists.
 */
static void Sys_EnsureCrashLogPath(void) {

  if (*sys_crash_log_path) {
    return;
  }

  char *dir = NULL;
  SDL_asprintf(&dir, "%s/default", Sys_UserDir());
  SDL_CreateDirectory(dir);
  SDL_snprintf(sys_crash_log_path, sizeof(sys_crash_log_path), "%s/crash.log", dir);
  free(dir);

#if !defined(_WIN32)
  if (sys_crash_log_fd == -1) {
    sys_crash_log_fd = open(sys_crash_log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
  }
#endif
}

/**
 * @brief Appends text to the crash log (creating it if needed) and to stderr.
 */
static void Sys_WriteCrashLog(const char *text) {

  Sys_EnsureCrashLogPath();

  FILE *log = fopen(sys_crash_log_path, "a");
  if (log) {
    fputs(text, log);
    fclose(log);
  }

  fputs(text, stderr);
  fflush(stderr);
}

/**
 * @brief Percent-encodes a string for use as a URL query parameter value.
 * @return Heap-allocated encoded string; caller must `free()`.
 */
static char *Sys_UrlEncode(const char *str) {

  const size_t max = strlen(str) * 3 + 1;
  char *out = Mem_Malloc(max);
  char *p = out;

  for (const char *s = str; *s; s++) {
    if (isalnum((unsigned char) *s) || *s == '-' || *s == '_' || *s == '.' || *s == '~') {
      *p++ = *s;
    } else {
      p += SDL_snprintf(p, out + max - p, "%%%02X", (unsigned char) *s);
    }
  }
  *p = '\0';

  return out;
}

#define CRASH_REPORT_GITHUB_URL "https://github.com/jdolan/quetoo/issues/new"
#define CRASH_REPORT_URL_MAX    8192
#define CRASH_REPORT_DIALOG_MAX 2048

/**
 * @brief Raise a fatal error dialog/exception.
 */
void Sys_Raise(const char *msg) {

  // Format timestamp
  char timestamp[32] = "";
  {
    time_t t = time(NULL);
    struct tm *tm_local = localtime(&t);
    if (tm_local) {
      strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_local);
    }
  }

  char *bt = Sys_Backtrace(0, UINT32_MAX);
  char *crash = NULL;
  SDL_asprintf(&crash, "Quetoo %s %s — %s\n\nError: %s\n\nBacktrace:\n%s",
               VERSION, BUILD, timestamp, msg, bt ? bt : "");
  free(bt);

  Sys_EnsureCrashLogPath();
  Sys_WriteCrashLog(crash);

  if (Com_WasInit(QUETOO_CLIENT)) {

    // Truncate the dialog message to avoid oversized message boxes
    char *dialog_msg = NULL;
    SDL_asprintf(&dialog_msg, "%s\n\nFull report saved to:\n%s", crash, sys_crash_log_path);
    if (strlen(dialog_msg) > CRASH_REPORT_DIALOG_MAX) {
      dialog_msg[CRASH_REPORT_DIALOG_MAX] = '\0';
    }

    // Build a pre-filled GitHub new-issue URL
    char *issue_body = NULL;
    SDL_asprintf(&issue_body, "**Quetoo %s %s crash report**\n\n**Error:** %s\n\n**Backtrace:**\n``\n%s\n``\n",
                 VERSION, BUILD, msg, crash);

    char *encoded_body = Sys_UrlEncode(issue_body);
    free(issue_body);

    char *issue_url = NULL;
    SDL_asprintf(&issue_url, "%s?title=Crash%%20Report&body=%s",
                 CRASH_REPORT_GITHUB_URL, encoded_body);
    free(encoded_body);

    if (strlen(issue_url) > CRASH_REPORT_URL_MAX) {
      issue_url[CRASH_REPORT_URL_MAX] = '\0';
    }

    const SDL_MessageBoxButtonData buttons[] = {
      {
        .flags    = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,
        .buttonID = 0,
        .text     = "Close"
      },
      {
        .buttonID = 1,
        .text     = "Copy to Clipboard"
      },
      {
        .flags    = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,
        .buttonID = 2,
        .text     = "Report Issue on GitHub"
      },
    };

    const SDL_MessageBoxData data = {
      .flags      = SDL_MESSAGEBOX_ERROR,
      .title      = "Fatal Error",
      .message    = dialog_msg,
      .numbuttons = lengthof(buttons),
      .buttons    = buttons,
    };

    int32_t button;
    SDL_ShowMessageBox(&data, &button);

    switch (button) {
      case 1:
        SDL_SetClipboardText(crash);
        break;
      case 2:
        SDL_OpenURL(issue_url);
        break;
      default:
        break;
    }

    free(dialog_msg);
    free(issue_url);
  }

  free(crash);

#if defined(_MSC_VER)
  RaiseException(EXCEPTION_NONCONTINUABLE_EXCEPTION, EXCEPTION_NONCONTINUABLE, 0, NULL);
#endif
}

/**
 * @brief Signal received flag, checked by the main loop.
 */
volatile sig_atomic_t sys_signal_received = 0;

/**
 * @brief Catch kernel interrupts and dispatch the appropriate exit routine.
 *
 * For graceful signals (`SIGINT`, `SIGTERM`, etc.), we set a flag and return so
 * the main loop can shut down safely — calling `Com_Shutdown` from signal
 * context risks crashing inside the GL driver or other non-reentrant code.
 *
 * For fatal signals (`SIGSEGV`, `SIGILL`, etc.), we reset to `SIG_DFL` and
 * re-raise to produce a clean crash with a core dump.
 */
void Sys_Signal(int32_t s) {

  switch (s) {
    case SIGINT:
    case SIGTERM:
#if !defined(_WIN32)
    case SIGHUP:
    case SIGQUIT:
#endif
      sys_signal_received = s;
      return;
    default:
      signal(s, SIG_DFL);
      raise(s);
      break;
  }
}

#if !defined(_WIN32)

/**
 * @brief Signal handler for fatal crash signals (`SIGSEGV`, `SIGILL`, etc.).
 *
 * Writes a crash log entry using only async-signal-safe functions, then
 * resets to the default handler and re-raises to produce a core dump.
 */
static void Sys_CrashSignal(int sig, siginfo_t *info, void *ctx) {
  (void) info;
  (void) ctx;

  static const char header[] = "\n--- Fatal Signal ---\n";

  const char *sig_name;
  switch (sig) {
    case SIGSEGV: sig_name = "SIGSEGV\n"; break;
    case SIGILL:  sig_name = "SIGILL\n";  break;
    case SIGFPE:  sig_name = "SIGFPE\n";  break;
    case SIGABRT: sig_name = "SIGABRT\n"; break;
    case SIGBUS:  sig_name = "SIGBUS\n";  break;
    default:      sig_name = "Unknown signal\n"; break;
  }

#if HAVE_EXECINFO
  void *frames[MAX_BACKTRACE_SYMBOLS];
  const int count = backtrace(frames, MAX_BACKTRACE_SYMBOLS);
#endif

  const int fds[] = { STDERR_FILENO, sys_crash_log_fd };
  for (int i = 0; i < (int) lengthof(fds); i++) {
    if (fds[i] == -1) {
      continue;
    }
    (void) write(fds[i], header, sizeof(header) - 1);
    (void) write(fds[i], sig_name, strlen(sig_name));
#if HAVE_EXECINFO
    backtrace_symbols_fd(frames, count, fds[i]);
#endif
  }

  // Attempt to display crash dialog and write crash log.
  // Not async-signal-safe, but acceptable in practice for crashes in game code.
  const char *name;
  switch (sig) {
    case SIGSEGV: name = "SIGSEGV"; break;
    case SIGILL:  name = "SIGILL";  break;
    case SIGFPE:  name = "SIGFPE";  break;
    case SIGABRT: name = "SIGABRT"; break;
    case SIGBUS:  name = "SIGBUS";  break;
    default:      name = "Unknown signal"; break;
  }
  Sys_Raise(name);

  signal(sig, SIG_DFL);
  raise(sig);
}

/**
 * @brief Installs async-signal-safe crash handlers for fatal signals.
 *
 * Uses sigaltstack so the handler survives stack overflows, and `SA_SIGINFO`
 * for access to fault context. Call once during startup, after signal().
 */
void Sys_InitCrashSignals(void) {

  Sys_EnsureCrashLogPath();

  static char sig_stack[SIGSTKSZ * 4];
  const stack_t ss = {
    .ss_sp   = sig_stack,
    .ss_size = sizeof(sig_stack),
  };
  sigaltstack(&ss, NULL);

  struct sigaction sa = {
    .sa_sigaction = Sys_CrashSignal,
    .sa_flags     = SA_SIGINFO | SA_ONSTACK,
  };
  sigemptyset(&sa.sa_mask);

  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGILL,  &sa, NULL);
  sigaction(SIGFPE,  &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);
  sigaction(SIGBUS,  &sa, NULL);
}

#endif /* !_WIN32 */
