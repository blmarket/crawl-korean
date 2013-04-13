#ifndef LIBUNIX_H
#define LIBUNIX_H

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef USE_TILE_LOCAL

#include <stdio.h>

#include "libconsole.h"
void fakecursorxy(int x, int y);

#ifdef USE_TILE_WEB
bool is_tiles();
#else
static inline constexpr bool is_tiles() { return false; }
#endif

extern int unixcurses_get_vi_key(int keyin);

#endif
#endif
