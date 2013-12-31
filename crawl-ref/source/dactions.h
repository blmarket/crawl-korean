#ifndef DGN_ACTIONS_H
#define DGN_ACTIONS_H

#include "travel.h"

void add_daction(daction_type act);
void catchup_dactions();
void update_da_counters(LevelInfo *lev);
unsigned int query_da_counter(daction_type c);

bool mons_matches_daction(const monster* mon, daction_type act);
void apply_daction_to_mons(monster* mons, daction_type act, bool local,
                           bool in_transit);

#endif
