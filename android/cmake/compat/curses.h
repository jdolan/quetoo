/*
 * curses.h — no-op curses stub for the Quetoo Android client.
 *
 * The dedicated-server console (src/server/sv_console.c) uses curses, but the
 * client only links the server for *listen* servers, and Android has no terminal.
 * curses isn't available in the NDK, so this header-only stub (placed on the
 * include path ahead of any system curses) lets sv_console.c compile with the
 * console inert. See android/DEPENDENCIES.md (client-only).
 */
#ifndef QUETOO_COMPAT_CURSES_H
#define QUETOO_COMPAT_CURSES_H

#include <stdarg.h>

typedef struct qg_window { int _unused; } WINDOW;
typedef unsigned long chtype;
typedef unsigned long attr_t;

static WINDOW qg_curses_stdscr;
#define stdscr (&qg_curses_stdscr)

#define ERR (-1)
#define OK  (0)
#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

/* attributes / colors (values are irrelevant for no-ops) */
#define A_NORMAL 0
#define A_BOLD   1
#define COLOR_BLACK 0
#define COLOR_RED 1
#define COLOR_GREEN 2
#define COLOR_YELLOW 3
#define COLOR_BLUE 4
#define COLOR_MAGENTA 5
#define COLOR_CYAN 6
#define COLOR_WHITE 7
#define COLOR_PAIR(n) ((chtype) 0)

/* keys */
#define KEY_DOWN 0402
#define KEY_UP 0403
#define KEY_LEFT 0404
#define KEY_RIGHT 0405
#define KEY_HOME 0406
#define KEY_BACKSPACE 0407
#define KEY_DC 0512
#define KEY_NPAGE 0522
#define KEY_PPAGE 0523
#define KEY_STAB 0524
#define KEY_END 0550
#define KEY_ENTER 0527

/* getmaxyx is a macro that assigns into y,x */
#define getmaxyx(win, y, x) do { (void) (win); (y) = 24; (x) = 80; } while (0)

/* stdscr dimensions (curses globals); fixed for the inert console */
#define LINES 24
#define COLS  80

static inline WINDOW *initscr(void) { return stdscr; }
static inline int endwin(void) { return OK; }
static inline int cbreak(void) { return OK; }
static inline int noecho(void) { return OK; }
static inline int curs_set(int v) { (void) v; return OK; }
static inline int start_color(void) { return OK; }
static inline int has_colors(void) { return FALSE; }
static inline int use_default_colors(void) { return OK; }
static inline int init_pair(short p, short f, short b) { (void) p; (void) f; (void) b; return OK; }
static inline int keypad(WINDOW *w, int b) { (void) w; (void) b; return OK; }
static inline int nodelay(WINDOW *w, int b) { (void) w; (void) b; return OK; }
static inline int scrollok(WINDOW *w, int b) { (void) w; (void) b; return OK; }
static inline int wtimeout(WINDOW *w, int d) { (void) w; (void) d; return OK; }
static inline int wgetch(WINDOW *w) { (void) w; return ERR; }
static inline int wmove(WINDOW *w, int y, int x) { (void) w; (void) y; (void) x; return OK; }
static inline int clear(void) { return OK; }
static inline int refresh(void) { return OK; }
static inline int wclear(WINDOW *w) { (void) w; return OK; }
static inline int werase(WINDOW *w) { (void) w; return OK; }
static inline int wclrtoeol(WINDOW *w) { (void) w; return OK; }
static inline int wrefresh(WINDOW *w) { (void) w; return OK; }
static inline int wnoutrefresh(WINDOW *w) { (void) w; return OK; }
static inline int doupdate(void) { return OK; }
static inline int waddstr(WINDOW *w, const char *s) { (void) w; (void) s; return OK; }
static inline int waddnstr(WINDOW *w, const char *s, int n) { (void) w; (void) s; (void) n; return OK; }
static inline int wattron(WINDOW *w, int a) { (void) w; (void) a; return OK; }
static inline int wattroff(WINDOW *w, int a) { (void) w; (void) a; return OK; }
static inline int wattrset(WINDOW *w, int a) { (void) w; (void) a; return OK; }
static inline int wbkgd(WINDOW *w, chtype c) { (void) w; (void) c; return OK; }
static inline WINDOW *newwin(int nl, int nc, int by, int bx) { (void) nl; (void) nc; (void) by; (void) bx; return stdscr; }
static inline int delwin(WINDOW *w) { (void) w; return OK; }
static inline int wresize(WINDOW *w, int l, int c) { (void) w; (void) l; (void) c; return OK; }
static inline int wprintw(WINDOW *w, const char *fmt, ...) { (void) w; (void) fmt; return OK; }
static inline int mvwprintw(WINDOW *w, int y, int x, const char *fmt, ...) { (void) w; (void) y; (void) x; (void) fmt; return OK; }

/* stdscr-relative operations (no-ops; they act on the inert stdscr) */
static inline int attron(int a) { (void) a; return OK; }
static inline int attroff(int a) { (void) a; return OK; }
static inline int attrset(int a) { (void) a; return OK; }
static inline int color_set(short pair, void *opts) { (void) pair; (void) opts; return OK; }
static inline int bkgdset(chtype c) { (void) c; return OK; }
static inline int border(chtype ls, chtype rs, chtype ts, chtype bs,
                         chtype tl, chtype tr, chtype bl, chtype br) {
  (void) ls; (void) rs; (void) ts; (void) bs; (void) tl; (void) tr; (void) bl; (void) br; return OK;
}
static inline int addch(chtype c) { (void) c; return OK; }
static inline int addstr(const char *s) { (void) s; return OK; }
static inline int addnstr(const char *s, int n) { (void) s; (void) n; return OK; }
static inline int mvaddch(int y, int x, chtype c) { (void) y; (void) x; (void) c; return OK; }
static inline int mvaddstr(int y, int x, const char *s) { (void) y; (void) x; (void) s; return OK; }
static inline int move(int y, int x) { (void) y; (void) x; return OK; }
static inline int getch(void) { return ERR; }
static inline int printw(const char *fmt, ...) { (void) fmt; return OK; }
static inline int mvprintw(int y, int x, const char *fmt, ...) { (void) y; (void) x; (void) fmt; return OK; }

#endif /* QUETOO_COMPAT_CURSES_H */
