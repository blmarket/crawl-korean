/**
 * @file
 * @brief User interactions that need to be restarted if the game is forcibly
 *        saved (via SIGHUP/window close).
**/

#ifndef UNCANCEL_H
#define UNCANCEL_H

void add_uncancel(uncancellable_type kind, int arg = 0);
void run_uncancels();

static inline void run_uncancel(uncancellable_type kind, int arg = 0)
{
    add_uncancel(kind, arg);
    run_uncancels();
}

#endif
