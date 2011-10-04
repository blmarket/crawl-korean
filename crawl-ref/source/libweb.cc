/**
 * @file
 * @brief Functions that any display port needs to implement -- webtiles version.
**/

#include "AppHdr.h"

#ifdef USE_TILE_WEB

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cio.h"
#include "defines.h"
#include "env.h"
#include "externs.h"
#include "message.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "tiledef-main.h"
#include "travel.h"
#include "viewgeom.h"

int m_getch()
{
    return getchk();
}

void set_mouse_enabled(bool enabled)
{
    crawl_state.mouse_enabled = enabled;
}

void gui_init_view_params(crawl_view_geometry &geom)
{
    // The tile version handles its own layout on a pixel-by-pixel basis.
    // Pretend that all of the regions start at character location (1,1).

    geom.termp.x = 1;
    geom.termp.y = 1;

    geom.termsz.x = 80;
    geom.termsz.y = 24;

    geom.viewp.x = 1;
    geom.viewp.y = 1;

    geom.hudp.x = 1;
    geom.hudp.y = 1;

    geom.msgp.x = 1;
    geom.msgp.y = 1;

    geom.mlistp.x  = 1;
    geom.mlistp.y  = 1;
    geom.mlistsz.x = 0;
    geom.mlistsz.y = 0;

    geom.viewsz.x = 17;
    geom.viewsz.y = 17;
}

void putwch(ucs_t chr)
{
    if (!chr)
        chr = ' ';
    ucs_t buf[2];
    buf[0] = chr;
    buf[1] = 0;
    tiles.put_ucs_string(buf);
}

void clear_to_end_of_line()
{
    tiles.clear_to_end_of_line();
}

void cprintf(const char *format,...)
{
    char buffer[2048];          // One full screen if no control seq...
    va_list argp;
    va_start(argp, format);
    vsnprintf(buffer, sizeof(buffer), format, argp);
    va_end(argp);
    tiles.put_string(buffer);
}

void textcolor(int color)
{
    tiles.textcolor(color);
}

void textbackground(int bg)
{
    tiles.textbackground(bg);
}

void set_cursor_enabled(bool enabled)
{
    //    if (enabled)
    //        TextRegion::_setcursortype(1);
    //    else
    //        TextRegion::_setcursortype(0);
}

bool is_cursor_enabled()
{
    //    if (TextRegion::cursor_flag)
    //        return (true);

    return (false);
}

bool is_smart_cursor_enabled()
{
    return false;
}

void enable_smart_cursor(bool dummy)
{
}

int wherex()
{
    return tiles.wherex();
}

int wherey()
{
    return tiles.wherey();
}

int get_number_of_lines()
{
    return tiles.get_number_of_lines();
}

int get_number_of_cols()
{
    return tiles.get_number_of_cols();
}

int getch_ck()
{
    return (tiles.getch_ck());
}

int getchk()
{
    return getch_ck();
}

void clrscr()
{
    tiles.clrscr();
}

void cgotoxy(int x, int y, GotoRegion region)
{
    tiles.cgotoxy(x, y, region);
}

coord_def cgetpos(GotoRegion region)
{
    ASSERT(region == get_cursor_region());
    return (coord_def(wherex(), wherey()));
}

GotoRegion get_cursor_region()
{
    return (tiles.get_cursor_region());
}

void delay(unsigned int ms)
{
    tiles.redraw();
    fprintf(stdout, "delay(%d);\n", ms);

    if (ms)
        usleep(ms * 1000);
}

void update_screen()
{
    tiles.set_need_redraw();
}

bool kbhit()
{
    // Check stdin in a non-blocking way
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds); //STDIN_FILENO is 0
    select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &fds) != 0;
}

void console_startup()
{
    tiles.resize();
}

void console_shutdown()
{
    tiles.shutdown();
}
#endif // #ifdef USE_TILE_WEB
