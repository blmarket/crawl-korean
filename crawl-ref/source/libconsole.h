#ifndef LIBCONSOLE_H
#define LIBCONSOLE_H

#include "defines.h"

void console_startup();
void console_shutdown();

class crawl_view_buffer;

int get_number_of_lines();
int get_number_of_cols();
int num_to_lines(int num);

void clrscr(void);
void clear_to_end_of_line();
void gotoxy_sys(int x, int y);
void textcolor(int c);
void textbackground(int c);
void cprintf(const char *format, ...);

int wherex(void);
int wherey(void);
void putwch(ucs_t c);
int getchk(void);
int getch_ck(void);
bool kbhit(void);
void delay(unsigned int ms);
void puttext(int x, int y, const crawl_view_buffer &vbuf);
void update_screen();

void set_cursor_enabled(bool enabled);
bool is_cursor_enabled();
void enable_smart_cursor(bool cursor);
bool is_smart_cursor_enabled();
void set_mouse_enabled(bool enabled);

static inline void put_colour_ch(int colour, unsigned ch)
{
    textcolor(colour);
    putwch(ch);
}

struct coord_def;
coord_def cgetpos(GotoRegion region = GOTO_CRT);
void cgotoxy(int x, int y, GotoRegion region = GOTO_CRT);
GotoRegion get_cursor_region();
#endif
