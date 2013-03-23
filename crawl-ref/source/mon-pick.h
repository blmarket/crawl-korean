/**
 * @file
 * @brief Functions used to help determine which monsters should appear.
**/


#ifndef MONPICK_H
#define MONPICK_H

#include "externs.h"

#define DEPTH_NOWHERE 999

typedef bool (*mon_pick_vetoer)(monster_type);

int mons_rarity(monster_type mcls, branch_type branch);
int mons_depth(monster_type mcls, branch_type branch);

monster_type pick_monster(level_id place, mon_pick_vetoer veto = nullptr);
monster_type pick_monster_no_rarity(branch_type branch);
monster_type pick_monster_by_hash(branch_type branch, uint32_t hash);
monster_type pick_monster_all_branches(int absdepth0, mon_pick_vetoer veto = nullptr);
bool branch_has_monsters(branch_type branch);
int branch_ood_cap(branch_type branch);

void debug_monpick();
#endif
