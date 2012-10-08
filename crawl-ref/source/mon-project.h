/**
 * @file
 * @brief Slow projectiles, done as monsters.
**/

#ifndef MON_PROJECT_H
#define MON_PROJECT_H

#include "beam.h"
#include "spl-cast.h"

spret_type cast_iood(actor *caster, int pow, bolt *beam,
                     float vx = 0, float vy = 0, int foe = MHITNOT,
                     bool fail = false);
void cast_iood_burst(int pow, coord_def target);
bool iood_act(monster& mon, bool no_trail = false);
void iood_catchup(monster* mon, int turns);
void boulder_start(monster *mon, bolt *beam);

#endif
