#ifndef SPL_DAMAGE_H
#define SPL_DAMAGE_H

#include "enum.h"
#include "spl-cast.h"
#include "act-iter.h"

struct bolt;
class dist;

spret_type fireball(int pow, bolt &beam, bool fail = false);
spret_type cast_delayed_fireball(bool fail);
void setup_fire_storm(const actor *source, int pow, bolt &beam);
spret_type cast_fire_storm(int pow, bolt &beam, bool fail);
bool cast_hellfire_burst(int pow, bolt &beam);
spret_type cast_chain_lightning(int pow, const actor *caster, bool fail = false);

spret_type cast_toxic_radiance(int pow, bool non_player = false, bool fail = false);
spret_type cast_refrigeration(int pow, bool non_player = false,
                              bool freeze_potions = true, bool fail = false);
void sonic_damage(bool scream);
void shillelagh(actor *wielder, coord_def where, int pow);
spret_type vampiric_drain(int pow, monster* mons, bool fail);
spret_type cast_freeze(int pow, monster* mons, bool fail);
spret_type cast_airstrike(int pow, const dist &beam, bool fail);
spret_type cast_shatter(int pow, bool fail);
spret_type cast_ignite_poison(int pow, bool fail);
spret_type cast_discharge(int pow, bool fail);
int disperse_monsters(coord_def where, int pow);
spret_type cast_dispersal(int pow, bool fail = false);
bool setup_fragmentation_beam(bolt &beam, int pow, const actor *caster,
                              const coord_def target, bool allow_random,
                              bool get_max_distance, bool quiet,
                              const char **what,
                              bool &destroy_wall, bool &hole);
spret_type cast_fragmentation(int powc, const actor *caster,
                              const coord_def target, bool fail);
int wielding_rocks();
spret_type cast_sandblast(int powc, bolt &beam, bool fail);
spret_type cast_tornado(int powc, bool fail);
void tornado_damage(actor *caster, int dur);
void cancel_tornado(bool tloc = false);
void tornado_move(const coord_def &pos);
spret_type cast_thunderbolt(actor *caster, int pow, coord_def aim,
                            bool fail = false);

actor* forest_near_enemy(const actor *mon);
void forest_message(const coord_def pos, const std::string &msg,
                    msg_channel_type ch = MSGCH_PLAIN);
                    void forest_damage(const actor *mon);
#endif
