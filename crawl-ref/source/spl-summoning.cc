/**
 * @file
 * @brief Summoning spells and other effects creating monsters.
**/

#include "AppHdr.h"

#include "spl-summoning.h"

#include "areas.h"
#include "artefact.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "directn.h"
#include "dungeon.h"
#include "env.h"
#include "food.h"
#include "fprop.h"
#include "ghost.h"
#include "godconduct.h"
#include "goditem.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "mapmark.h"
#include "message.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-iter.h"
#include "mon-place.h"
#include "mon-speak.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "options.h"
#include "player-equip.h"
#include "player-stats.h"
#include "religion.h"
#include "shout.h"
#include "teleport.h"
#include "terrain.h"
#include "xom.h"

bool summon_animals(int pow)
{
    bool success = false;

    // Maybe we should just generate a Lair monster instead (and
    // guarantee that it is mobile)?
    const monster_type animals[] = {
        MONS_BUMBLEBEE, MONS_WAR_DOG, MONS_SHEEP, MONS_YAK,
        MONS_HOG, MONS_SOLDIER_ANT, MONS_WOLF,
        MONS_GRIZZLY_BEAR, MONS_POLAR_BEAR, MONS_BLACK_BEAR,
        MONS_AGATE_SNAIL, MONS_BORING_BEETLE, MONS_BASILISK,
        MONS_KOMODO_DRAGON, MONS_SPINY_FROG, MONS_HOUND,
        MONS_ELEPHANT, MONS_HIPPOGRIFF, MONS_GRIFFON
    };

    int count = 0;
    const int count_max = 8;

    int pow_left = pow + 1;

    const bool varied = coinflip();

    monster_type mon = MONS_PROGRAM_BUG;

    while (pow_left >= 0 && count < count_max)
    {
        // Pick a random monster and subtract its cost.
        if (varied || count == 0)
            mon = RANDOM_ELEMENT(animals);

        const int pow_spent = mons_power(mon) * 3;

        // Allow a certain degree of overuse, but not too much.
        // Guarantee at least two summons.
        if (pow_spent >= pow_left * 2 && count >= 2)
            break;

        pow_left -= pow_spent;
        count++;

        const bool friendly = (random2(pow) > 4);

        if (create_monster(
                mgen_data(mon,
                          friendly ? BEH_FRIENDLY : BEH_HOSTILE, &you,
                          4, 0, you.pos(), MHITYOU)) != -1)
        {
            success = true;
        }
    }

    return (success);
}

spret_type cast_summon_butterflies(int pow, god_type god, bool fail)
{
    fail_check();
    bool success = false;

    const int how_many = std::min(15, 3 + random2(3) + random2(pow) / 10);

    for (int i = 0; i < how_many; ++i)
    {
        if (create_monster(
                mgen_data(MONS_BUTTERFLY, BEH_FRIENDLY, &you,
                          3, SPELL_SUMMON_BUTTERFLIES,
                          you.pos(), MHITYOU,
                          0, god)) != -1)
        {
            success = true;
        }
    }

    if (!success)
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_summon_small_mammals(int pow, god_type god, bool fail)
{
    fail_check();
    bool success = false;

    monster_type mon = MONS_PROGRAM_BUG;

    int count = (pow == 25) ? 2 : 1;

    for (int i = 0; i < count; ++i)
    {
        if (x_chance_in_y(10, pow + 1))
            mon = coinflip() ? MONS_BAT : MONS_RAT;
        else
            mon = coinflip() ? MONS_QUOKKA : MONS_GREY_RAT;

        if (create_monster(
                mgen_data(mon, BEH_FRIENDLY, &you,
                          3, SPELL_SUMMON_SMALL_MAMMALS,
                          you.pos(), MHITYOU,
                          0, god)) != -1)
        {
            success = true;
        }

    }

    if (!success)
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

static bool _snakable_missile(const item_def& item)
{
    if (item.base_type != OBJ_MISSILES
        || item.sub_type != MI_ARROW && item.sub_type != MI_JAVELIN)
    {
        return false;
    }

    return item.special != SPMSL_SILVER && item.special != SPMSL_STEEL;
}

static bool _snakable_weapon(const item_def& item)
{
    if (item.base_type == OBJ_STAVES)
        return true;
    return (item.base_type == OBJ_WEAPONS
           && (item.sub_type == WPN_CLUB
            || item.sub_type == WPN_SPEAR
            || item.sub_type == WPN_TRIDENT
            || item.sub_type == WPN_DEMON_TRIDENT
            || item.sub_type == WPN_STAFF
            || item.sub_type == WPN_QUARTERSTAFF
            || item.sub_type == WPN_SCYTHE
            || item.sub_type == WPN_GIANT_CLUB
            || item.sub_type == WPN_GIANT_SPIKED_CLUB
            || item.sub_type == WPN_BOW
            || item.sub_type == WPN_LONGBOW
            || item.sub_type == WPN_ANKUS
            || item.sub_type == WPN_HALBERD
            || item.sub_type == WPN_GLAIVE
            || item.sub_type == WPN_BARDICHE
            || item.sub_type == WPN_BLOWGUN)
           && !is_artefact(item));
}

bool item_is_snakable(const item_def& item)
{
    return (_snakable_missile(item) || _snakable_weapon(item));
}

spret_type cast_sticks_to_snakes(int pow, god_type god, bool fail)
{
    if (!you.weapon())
    {
        mprf("Your %s feel slithery!", you.hand_name(true).c_str());
        return SPRET_ABORT;
    }

    const item_def& wpn = *you.weapon();
    const std::string abort_msg =
            make_stringf("%s feel%s slithery for a moment!",
                         wpn.name(true, DESC_CAP_YOUR).c_str(),
                         wpn.quantity > 1 ? "" : "s");

    // Don't enchant sticks marked with {!D}.
    if (!check_warning_inscriptions(wpn, OPER_DESTROY))
    {
        mpr(abort_msg);
        return SPRET_ABORT;
    }

    const int dur = std::min(3 + random2(pow) / 20, 5);
    int how_many_max = 1 + random2(1 + you.skill(SK_TRANSMUTATIONS)) / 4;
    const bool friendly = (!wpn.cursed());
    const beh_type beha = (friendly) ? BEH_FRIENDLY : BEH_HOSTILE;

    int count = 0;

    if (_snakable_missile(wpn))
    {
        fail_check();
        if (wpn.quantity < how_many_max)
            how_many_max = wpn.quantity;

        for (int i = 0; i < how_many_max; i++)
        {
            monster_type mon;

            if (get_ammo_brand(wpn) == SPMSL_POISONED
                || one_chance_in(5 - std::min(4, div_rand_round(pow * 2, 25))))
            {
                mon = x_chance_in_y(pow / 3, 100) ? MONS_WATER_MOCCASIN
                                                  : MONS_SNAKE;
            }
            else
                mon = MONS_SMALL_SNAKE;

            const int mon_index = create_monster(mgen_data(mon, beha, &you,
                                      0, SPELL_STICKS_TO_SNAKES, you.pos(),
                                      MHITYOU, 0, god), false);
            if (mon_index != -1)
            {
                count++;
                monster* snake = &menv[mon_index];
                snake->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, dur));
            }
        }
    }

    if (_snakable_weapon(wpn))
    {
        fail_check();
        // Upsizing Snakes to Water Moccasins as the base class for using
        // the really big sticks (so bonus applies really only to trolls
        // and ogres).  Still, it's unlikely any character is strong
        // enough to bother lugging a few of these around. - bwr
        monster_type mon = MONS_SNAKE;

        if (get_weapon_brand(wpn) == SPWPN_VENOM || item_mass(wpn) >= 300)
            mon = MONS_WATER_MOCCASIN;

        if (pow > 20 && one_chance_in(3))
            mon = MONS_WATER_MOCCASIN;

        if (pow > 40 && coinflip())
            mon = MONS_WATER_MOCCASIN;

        if (pow > 70 && one_chance_in(3))
            mon = MONS_BLACK_MAMBA;

        if (pow > 90 && one_chance_in(3))
            mon = MONS_ANACONDA;

        const int mon_index = create_monster(mgen_data(mon, beha, &you,
                                  0, SPELL_STICKS_TO_SNAKES, you.pos(),
                                  MHITYOU, 0, god), false);
        if (mon_index != -1)
        {
            count++;
            monster* snake = &menv[mon_index];
            snake->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, dur));
        }
    }

    if (!count)
    {
        mpr(abort_msg);
        return SPRET_SUCCESS;
    }

    dec_inv_item_quantity(you.equip[EQ_WEAPON], count);
    mpr((count > 1) ? "You create some snakes!" : "You create a snake!");
    return SPRET_SUCCESS;
}

spret_type cast_summon_scorpions(int pow, god_type god, bool fail)
{
    fail_check();
    bool success = false;

    const int how_many = stepdown_value(1 + random2(pow)/10 + random2(pow)/10,
                                        2, 2, 6, 8);

    for (int i = 0; i < how_many; ++i)
    {
        const bool friendly = (random2(pow) > 3);

        if (create_monster(
                mgen_data(MONS_SCORPION,
                          friendly ? BEH_FRIENDLY : BEH_HOSTILE, &you,
                          3, SPELL_SUMMON_SCORPIONS,
                          you.pos(), MHITYOU,
                          0, god)) != -1)
        {
            success = true;
        }
    }

    if (!success)
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

// Creates a mixed swarm of typical swarming animals.
// Number and duration depend on spell power.
spret_type cast_summon_swarm(int pow, god_type god, bool fail)
{
    fail_check();
    bool success = false;
    const int dur = std::min(2 + (random2(pow) / 4), 6);
    const int how_many = stepdown_value(2 + random2(pow)/10 + random2(pow)/25,
                                        2, 2, 6, 8);

    for (int i = 0; i < how_many; ++i)
    {
        const monster_type swarmers[] = {
            MONS_KILLER_BEE,     MONS_KILLER_BEE,    MONS_KILLER_BEE,
            MONS_SCORPION,       MONS_WORM,          MONS_VAMPIRE_MOSQUITO,
            MONS_GOLIATH_BEETLE, MONS_WOLF_SPIDER,   MONS_BUTTERFLY,
            MONS_YELLOW_WASP,    MONS_WORKER_ANT,    MONS_WORKER_ANT,
            MONS_WORKER_ANT
        };

        monster_type mon = MONS_NO_MONSTER;

        // If you worship a good god, don't summon an evil/unclean
        // swarmer (in this case, the vampire mosquito).
        do
            mon = RANDOM_ELEMENT(swarmers);
        while (player_will_anger_monster(mon));

        if (create_monster(
                mgen_data(mon, BEH_FRIENDLY, &you,
                          dur, SPELL_SUMMON_SWARM,
                          you.pos(),
                          MHITYOU,
                          0, god)) != -1)
        {
            success = true;
        }
    }

    if (!success)
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_call_canine_familiar(int pow, god_type god, bool fail)
{
    fail_check();
    monster_type mon = MONS_PROGRAM_BUG;

    const int chance = random2(pow);
    if (chance < 10)
        mon = MONS_JACKAL;
    else if (chance < 15)
        mon = MONS_HOUND;
    else
    {
        switch (chance % 7)
        {
        case 0:
            if (one_chance_in(you.species == SP_HILL_ORC ? 3 : 6))
                mon = MONS_WARG;
            else
                mon = MONS_WOLF;
            break;

        case 1:
        case 2:
            mon = MONS_WAR_DOG;
            break;

        case 3:
        case 4:
            mon = MONS_HOUND;
            break;

        default:
            mon = MONS_JACKAL;
            break;
        }
    }

    const int dur = std::min(2 + (random2(pow) / 4), 6);

    if (create_monster(
            mgen_data(mon, BEH_FRIENDLY, &you,
                      dur, SPELL_CALL_CANINE_FAMILIAR,
                      you.pos(),
                      MHITYOU,
                      0, god)) == -1)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
    }

    return SPRET_SUCCESS;
}

static int _count_summons(monster_type type)
{
    int cnt = 0;

    for (monster_iterator mi; mi; ++mi)
        if (mi->type == type
            && mi->attitude == ATT_FRIENDLY // friendly() would count charmed
            && mi->is_summoned())
        {
            cnt++;
        }

    return (cnt);
}

static monster_type _feature_to_elemental(const coord_def& where,
                                          monster_type strict_elem)
{
    if (!in_bounds(where))
        return (MONS_NO_MONSTER);

    if (strict_elem != MONS_NO_MONSTER
        && strict_elem != MONS_EARTH_ELEMENTAL
        && strict_elem != MONS_FIRE_ELEMENTAL
        && strict_elem != MONS_WATER_ELEMENTAL
        && strict_elem != MONS_AIR_ELEMENTAL)
    {
        return (MONS_NO_MONSTER);
    }

    const bool any_elem = strict_elem == MONS_NO_MONSTER;

    bool elem_earth;
    if (any_elem || strict_elem == MONS_EARTH_ELEMENTAL)
    {
        elem_earth = grd(where) == DNGN_ROCK_WALL
                     || grd(where) == DNGN_CLEAR_ROCK_WALL;
    }
    else
        elem_earth = false;

    bool elem_fire_cloud;
    bool elem_fire_lava;
    bool elem_fire;
    if (any_elem || strict_elem == MONS_FIRE_ELEMENTAL)
    {
        elem_fire_cloud = env.cgrid(where) != EMPTY_CLOUD
                          && env.cloud[env.cgrid(where)].type == CLOUD_FIRE;
        elem_fire_lava = grd(where) == DNGN_LAVA;
        elem_fire = elem_fire_cloud || elem_fire_lava;
    }
    else
    {
        elem_fire_cloud = false;
        elem_fire_lava = false;
        elem_fire = false;
    }

    bool elem_water;
    if (any_elem || strict_elem == MONS_WATER_ELEMENTAL)
        elem_water = feat_is_watery(grd(where));
    else
        elem_water = false;

    bool elem_air;
    if (any_elem || strict_elem == MONS_AIR_ELEMENTAL)
    {
        elem_air = grd(where) >= DNGN_FLOOR
                   && env.cgrid(where) == EMPTY_CLOUD;
    }
    else
        elem_air = false;

    monster_type elemental = MONS_NO_MONSTER;

    if (elem_earth)
    {
        grd(where) = DNGN_FLOOR;
        set_terrain_changed(where);

        elemental = MONS_EARTH_ELEMENTAL;
    }
    else if (elem_fire)
    {
        if (elem_fire_cloud)
            delete_cloud(env.cgrid(where));

        elemental = MONS_FIRE_ELEMENTAL;
    }
    else if (elem_water)
        elemental = MONS_WATER_ELEMENTAL;
    else if (elem_air)
        elemental = MONS_AIR_ELEMENTAL;

    return (elemental);
}

// 'unfriendly' is percentage chance summoned elemental goes
//              postal on the caster (after taking into account
//              chance of that happening to unskilled casters
//              anyway).
spret_type cast_summon_elemental(int pow, god_type god,
                                 monster_type restricted_type,
                                 int unfriendly, int horde_penalty, bool fail)
{
    monster_type mon;

    coord_def targ;
    dist smove;

    const int dur = std::min(2 + (random2(pow) / 5), 6);

    while (true)
    {
        mpr("Summon from material in which direction?", MSGCH_PROMPT);

        direction_chooser_args args;
        args.restricts = DIR_DIR;
        direction(smove, args);

        if (!smove.isValid)
        {
            canned_msg(MSG_OK);
            return SPRET_ABORT;
        }

        targ = you.pos() + smove.delta;

        if (const monster* m = monster_at(targ))
        {
            if (you.can_see(m))
                mpr("There's something there already!");
            else
            {
                fail_check();
                mpr("Something seems to disrupt your summoning.");
                return SPRET_SUCCESS; // still losing a turn
            }
        }
        else if (smove.delta.origin())
            mpr("You can't summon an elemental from yourself!");
        else if (!in_bounds(targ))
        {
            // XXX: Should this cost a turn?
            mpr("That material won't yield to your beckoning.");
            return SPRET_ABORT;
        }

        break;
    }

    mon = _feature_to_elemental(targ, restricted_type);

    // Found something to summon?
    if (mon == MONS_NO_MONSTER)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_ABORT;
    }

    fail_check();
    if (horde_penalty)
        horde_penalty *= _count_summons(mon);

    // silly - ice for water? 15jan2000 {dlb}
    // little change here to help with the above... and differentiate
    // elements a bit... {bwr}
    // - Water elementals are now harder to be made reliably friendly.
    // - Air elementals are harder because they're more dynamic/dangerous.
    // - Earth elementals are more static and easy to tame (as before).
    // - Fire elementals fall in between the two (10 is still fairly easy).
    const bool friendly = ((mon != MONS_FIRE_ELEMENTAL
                            || x_chance_in_y(you.skill(SK_FIRE_MAGIC)
                                             - horde_penalty, 10))

                        && (mon != MONS_WATER_ELEMENTAL
                            || x_chance_in_y(you.skill(SK_ICE_MAGIC)
                                             - horde_penalty,
                                             (you.species == SP_MERFOLK) ? 5
                                                                         : 15))

                        && (mon != MONS_AIR_ELEMENTAL
                            || x_chance_in_y(you.skill(SK_AIR_MAGIC)
                                             - horde_penalty, 15))

                        && (mon != MONS_EARTH_ELEMENTAL
                            || x_chance_in_y(you.skill(SK_EARTH_MAGIC)
                                             - horde_penalty, 5))

                        && random2(100) >= unfriendly);

    if (create_monster(
            mgen_data(mon,
                      friendly ? BEH_FRIENDLY : BEH_HOSTILE, &you,
                      dur, SPELL_SUMMON_ELEMENTAL,
                      targ,
                      MHITYOU,
                      0, god)) == -1)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_SUCCESS;
    }

    mpr("An elemental appears!");

    if (!friendly)
        mpr("It doesn't seem to appreciate being summoned.");

    return SPRET_SUCCESS;
}

spret_type cast_summon_ice_beast(int pow, god_type god, bool fail)
{
    fail_check();
    const int dur = std::min(2 + (random2(pow) / 4), 6);

    if (create_monster(
            mgen_data(MONS_ICE_BEAST, BEH_FRIENDLY, &you,
                      dur, SPELL_SUMMON_ICE_BEAST,
                      you.pos(), MHITYOU,
                      0, god)) != -1)
    {
        mpr("A chill wind blows around you.");
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_summon_ugly_thing(int pow, god_type god, bool fail)
{
    fail_check();
    monster_type mon = MONS_PROGRAM_BUG;

    if (random2(pow) >= 46 || one_chance_in(6))
        mon = MONS_VERY_UGLY_THING;
    else
        mon = MONS_UGLY_THING;

    const int dur = std::min(2 + (random2(pow) / 4), 6);

    if (create_monster(
            mgen_data(mon,
                      BEH_FRIENDLY, &you,
                      dur, SPELL_SUMMON_UGLY_THING,
                      you.pos(),
                      MHITYOU,
                      0, god)) != -1)
    {
        mpr((mon == MONS_VERY_UGLY_THING) ? "A very ugly thing appears."
                                          : "An ugly thing appears.");
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_summon_hydra(actor *caster, int pow, god_type god, bool fail)
{
    fail_check();
    // Power determines number of heads. Minimum 4 heads, maximum 12.
    // Rare to get more than 8.
    int heads;

    // Small chance to create a huge hydra (if spell power is high enough)
    if (one_chance_in(6))
        heads = std::min((random2(pow) / 6), 12);
    else
        heads = std::min((random2(pow) / 6), 8);

    if (heads < 4)
        heads = 4;

    // Duration is always very short - just 1.
    int midx;
    if ((midx = create_monster(
            mgen_data(MONS_HYDRA, BEH_COPY, caster,
                      1, SPELL_SUMMON_HYDRA, caster->pos(),
                      (caster->atype() == ACT_PLAYER) ?
                          MHITYOU : caster->as_monster()->foe,
                      0, (god == GOD_NO_GOD) ? caster->deity() : god,
                      MONS_HYDRA, heads))) != -1)
    {
        if (you.see_cell(menv[midx].pos()))
            mpr("A hydra appears.");
        player_angers_monster(&menv[midx]); // currently no-op
    }
    else if (caster == &you)
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_summon_dragon(actor *caster, int pow, god_type god, bool fail)
{
    // Dragons are always friendly. Dragon type depends on power and
    // random chance, with two low-tier dragons possible at high power.
    // Duration fixed at 6.

    fail_check();
    bool success = false;

    if (god == GOD_NO_GOD)
        god = caster->deity();

    monster_type mon = MONS_PROGRAM_BUG;

    const int chance = random2(pow);
    int how_many = 1;

    if (chance >= 80 || one_chance_in(6))
        mon = (coinflip()) ? MONS_GOLDEN_DRAGON : MONS_QUICKSILVER_DRAGON;
    else if (chance >= 40 || one_chance_in(6))
        switch (random2(3))
        {
        case 0:
            mon = MONS_IRON_DRAGON;
            break;
        case 1:
            mon = MONS_SHADOW_DRAGON;
            break;
        default:
            mon = MONS_STORM_DRAGON;
            break;
        }
    else
    {
        mon = (coinflip()) ? MONS_DRAGON : MONS_ICE_DRAGON;
        if (pow >= 100)
            how_many = 2;
    }
    // For good gods, switch away from shadow dragons (and, for TSO,
    // golden dragons, since they poison) to storm/iron dragons.
    if (player_will_anger_monster(mon)
        || (god == GOD_SHINING_ONE && mon == MONS_GOLDEN_DRAGON))
    {
        mon = (coinflip()) ? MONS_STORM_DRAGON : MONS_IRON_DRAGON;
    }

    for (int i = 0; i < how_many; ++i)
    {
        int midx;
        if ((midx = create_monster(
                mgen_data(mon, BEH_COPY, caster,
                          6, SPELL_SUMMON_DRAGON,
                          caster->pos(),
                          (caster->atype() == ACT_PLAYER) ? MHITYOU
                            : caster->as_monster()->foe,
                          0, god))) != -1)
        {
            if (you.see_cell(menv[midx].pos()))
                mpr("A dragon appears.");
            // Xom summoning evil dragons if you worship a good god?  Sure!
            player_angers_monster(&menv[midx]);
            success = true;
        }
    }

    if (!success && caster == &you)
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

// This assumes that the specified monster can go berserk.
static void _make_mons_berserk_summon(monster* mon)
{
    mon->go_berserk(false);
    mon_enchant berserk = mon->get_ench(ENCH_BERSERK);
    mon_enchant abj = mon->get_ench(ENCH_ABJ);

    // Let Trog's gifts berserk longer, and set the abjuration timeout
    // to the berserk timeout.
    berserk.duration = berserk.duration * 3 / 2;
    berserk.maxduration = berserk.duration;
    abj.duration = abj.maxduration = berserk.duration;
    mon->update_ench(berserk);
    mon->update_ench(abj);
}

// This is actually one of Trog's wrath effects.
bool summon_berserker(int pow, actor *caster, monster_type override_mons)
{
    monster_type mon = MONS_PROGRAM_BUG;

    const int dur = std::min(2 + (random2(pow) / 4), 6);

    if (override_mons != MONS_PROGRAM_BUG)
        mon = override_mons;
    else
    {
        if (pow <= 100)
        {
            // bears
            mon = (coinflip()) ? MONS_BLACK_BEAR : MONS_GRIZZLY_BEAR;
        }
        else if (pow <= 140)
        {
            // ogres
            mon = (one_chance_in(3) ? MONS_TWO_HEADED_OGRE : MONS_OGRE);
        }
        else if (pow <= 180)
        {
            // trolls
            switch (random2(8))
            {
            case 0:
                mon = MONS_DEEP_TROLL;
                break;
            case 1:
            case 2:
                mon = MONS_IRON_TROLL;
                break;
            case 3:
            case 4:
                mon = MONS_ROCK_TROLL;
                break;
            default:
                mon = MONS_TROLL;
                break;
            }
        }
        else
        {
            // giants
            mon = (coinflip()) ? MONS_HILL_GIANT : MONS_STONE_GIANT;
        }
    }

    mgen_data mg(mon, caster ? BEH_COPY : BEH_HOSTILE, caster, dur, 0,
                 caster ? caster->pos() : you.pos(),
                 (caster && caster->atype() == ACT_MONSTER)
                     ? ((monster*)caster)->foe : MHITYOU,
                 0, GOD_TROG);

    if (!caster)
        mg.non_actor_summoner = "the rage of " + god_name(GOD_TROG, false);

    const int mons = create_monster(mg);

    if (mons == -1)
        return (false);

    _make_mons_berserk_summon(&menv[mons]);
    return (true);
}

static bool _summon_holy_being_wrapper(int pow, god_type god, int spell,
                                       monster_type mon, int dur, bool friendly,
                                       bool quiet)
{
    UNUSED(pow);

    mgen_data mg(mon,
                 friendly ? BEH_FRIENDLY : BEH_HOSTILE,
                 friendly ? &you : 0,
                 dur, spell,
                 you.pos(),
                 MHITYOU,
                 MG_FORCE_BEH, god);

    if (!friendly)
    {
        mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);
        mg.non_actor_summoner = god_name(god, false);
    }

    const int mons = create_monster(mg);

    if (mons == -1)
        return (false);

    monster* summon = &menv[mons];
    summon->flags |= MF_ATT_CHANGE_ATTEMPT;

    if (!quiet)
        mpr("You are momentarily dazzled by a brilliant light.");

    player_angers_monster(&menv[mons]);
    return (true);
}

static bool _summon_holy_being_wrapper(int pow, god_type god, int spell,
                                       holy_being_class_type hbct, int dur,
                                       bool friendly, bool quiet)
{
    monster_type mon = summon_any_holy_being(hbct);

    return _summon_holy_being_wrapper(pow, god, spell, mon, dur, friendly,
                                      quiet);
}

// Not a spell. Rather, this is TSO's doing.
bool summon_holy_warrior(int pow, god_type god, int spell,
                         bool force_hostile, bool permanent,
                         bool quiet)
{
    return _summon_holy_being_wrapper(pow, god, spell, HOLY_BEING_WARRIOR,
                                      !permanent ?
                                          std::min(2 + (random2(pow) / 4), 6) :
                                          0,
                                      !force_hostile, quiet);
}

static bool _can_weapon_dance(item_def* wpn)
{
    if (!wpn
        || (wpn->base_type != OBJ_WEAPONS && wpn->base_type != OBJ_STAVES)
        || is_range_weapon(*wpn)
        || is_special_unrandom_artefact(*wpn))
    {
        return false;
    }
    return true;
}

// Mass Tukima's dance
spret_type cast_tukimas_ball(actor *caster, int pow, god_type god,
                             bool force_hostile, bool fail)
{
    fail_check();
    bool some_weapon_was_animated = false;
    const int dur = std::min(2 + (random2(pow) / 5), 6);

    radius_iterator ri(caster->pos(), 6, C_ROUND,
                       caster->get_los_no_trans());
    //iterate over all weapons in view
    for (; ri; ++ri)
        for (stack_iterator si(*ri) ; si ; ++ si)
            if (si->base_type == OBJ_WEAPONS || si->base_type == OBJ_STAVES)
            {
                if (!_can_weapon_dance(&*si))
                {
                    mprf("%s flop%s limply for a second.",
                    si->name(true, DESC_CAP_THE).c_str(),
                    si->quantity > 1 ? "" : "s");
                    continue;
                }
                //attempt to animate weapon
                item_def wpn = *si;

                // Cursed weapons become hostile.
                const bool friendly = !force_hostile && !wpn.cursed();
                mgen_data mg(MONS_DANCING_WEAPON,
                             friendly ? BEH_FRIENDLY : BEH_HOSTILE,
                             caster,
                             dur,
                             SPELL_TUKIMAS_DANCE,
                             *ri,
                             (caster == &you) ? MHITYOU : caster->as_monster()->foe,
                             // mgen_flag_type - might be MG_PLAYER_MADE or MG_FORCE_PLACE
                             0,
                             // if you animate the weapon, your god gets mad
                             // if it poisons/drains stuff
                             god,
                             MONS_NO_MONSTER, 0, BLACK,
                             pow);
                mg.props[TUKIMA_WEAPON] = wpn;
                int mons = create_monster(mg);
                bool success = (mons != -1);
                some_weapon_was_animated |= success;

                //remove weapon from ground if animated
                if (success)
                    destroy_item(si->index());
            }

    if (some_weapon_was_animated)
        mpr("Haunting music fills the air, and weapons rise to join the dance!");
    else
        mpr("Strange music fills the air, but nothing else happens.");

    noisy(12, you.pos(), MHITYOU);
    return SPRET_SUCCESS;

    //TODO: add awesome disco lighting and rename the spell "Tukima's rave"
}


// This function seems to have very little regard for encapsulation.
spret_type cast_tukimas_dance(int pow, god_type god, bool force_hostile,
                              bool fail)
{
    const int dur = std::min(2 + (random2(pow) / 5), 6);
    item_def* wpn = you.weapon();

    // See if the wielded item is appropriate.
    if (!wpn
        || (wpn->base_type != OBJ_WEAPONS && wpn->base_type != OBJ_STAVES)
        || is_range_weapon(*wpn)
        || is_special_unrandom_artefact(*wpn))
    {
        if (wpn)
        {
            mprf("%s vibrate%s crazily for a second.",
                 wpn->name(true, DESC_CAP_YOUR).c_str(),
                 wpn->quantity > 1 ? "" : "s");
        }
        else
            mprf("Your %s twitch.", you.hand_name(true).c_str());

        return SPRET_ABORT;
    }

    fail_check();
    int mons = -1;
    item_def cp = *wpn;
    // Clear temp branding so we don't brand permanently.
    if (you.duration[DUR_WEAPON_BRAND])
        set_item_ego_type(cp, OBJ_WEAPONS, SPWPN_NORMAL);

    // Mark weapon as "thrown", so we'll autopickup it later.
    cp.flags |= ISFLAG_THROWN;

    // Cursed weapons become hostile.
    const bool friendly = (!force_hostile && !wpn->cursed());

    mgen_data mg(MONS_DANCING_WEAPON,
                 friendly ? BEH_FRIENDLY : BEH_HOSTILE,
                 force_hostile ? 0 : &you,
                 dur, SPELL_TUKIMAS_DANCE,
                 you.pos(),
                 MHITYOU,
                 0, god,
                 MONS_NO_MONSTER, 0, BLACK,
                 pow);
    mg.props[TUKIMA_WEAPON] = cp;

    if (force_hostile)
        mg.non_actor_summoner = god_name(god, false);

    mons = create_monster(mg);

    // We are successful.  Unwield the weapon, removing any wield
    // effects.
    unwield_item();

    mprf("%s dances into the air!", wpn->name(true, DESC_CAP_YOUR).c_str());

    // Find out what our god thinks before killing the item.
    conduct_type why = good_god_hates_item_handling(*wpn);
    if (!why)
        why = god_hates_item_handling(*wpn);

    wpn->clear();

    if (why)
    {
        simple_god_message(" booms: How dare you animate that foul thing!");
        did_god_conduct(why, 10, true, &menv[mons]);
    }

    return SPRET_SUCCESS;
}

spret_type cast_conjure_ball_lightning(int pow, god_type god, bool fail)
{
    fail_check();
    bool success = false;

    // Restricted so that the situation doesn't get too gross.  Each of
    // these will explode for 3d20 damage. -- bwr
    const int how_many = std::min(8, 3 + random2(2 + pow / 50));

    for (int i = 0; i < how_many; ++i)
    {
        coord_def target;
        bool found = false;
        for (int j = 0; j < 10; ++j)
        {
            if (random_near_space(you.pos(), target, true, true, false)
                && distance(you.pos(), target) <= 5)
            {
                found = true;
                break;
            }
        }

        // If we fail, we'll try the ol' summon next to player trick.
        if (!found)
            target = you.pos();

        const int mons =
            mons_place(
                mgen_data(MONS_BALL_LIGHTNING, BEH_FRIENDLY, &you,
                          0, SPELL_CONJURE_BALL_LIGHTNING,
                          target, MHITNOT, 0, god));

        if (mons != -1)
        {
            success = true;
            menv[mons].add_ench(ENCH_SHORT_LIVED);
        }
    }

    if (success)
        mpr("You create some ball lightning!");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_call_imp(int pow, god_type god, bool fail)
{
    fail_check();
    monster_type mon = MONS_PROGRAM_BUG;

    if (random2(pow) >= 46 || one_chance_in(6))
        mon = one_chance_in(3) ? MONS_IRON_IMP : MONS_SHADOW_IMP;
    else
        mon = one_chance_in(3) ? MONS_WHITE_IMP : MONS_CRIMSON_IMP;

    const int dur = std::min(2 + (random2(pow) / 4), 6);

    const int mons =
        create_monster(
            mgen_data(mon, BEH_FRIENDLY, &you, dur, SPELL_CALL_IMP,
                      you.pos(), MHITYOU, MG_FORCE_BEH, god));

    if (mons != -1)
    {
        mpr((mon == MONS_WHITE_IMP)  ? "A beastly little devil appears in a puff of frigid air." :
            (mon == MONS_IRON_IMP)   ? "A metallic apparition takes form in the air." :
            (mon == MONS_SHADOW_IMP) ? "A shadowy apparition takes form in the air."
                                     : "A beastly little devil appears in a puff of flame.");

        player_angers_monster(&menv[mons]);
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

static bool _summon_demon_wrapper(int pow, god_type god, int spell,
                                  monster_type mon, int dur, bool friendly,
                                  bool charmed, bool quiet)
{
    bool success = false;

    const int mons =
        create_monster(
            mgen_data(mon,
                      friendly ? BEH_FRIENDLY :
                          charmed ? BEH_CHARMED : BEH_HOSTILE, &you,
                      dur, spell, you.pos(), MHITYOU, MG_FORCE_BEH, god));

    if (mons != -1)
    {
        success = true;

        mpr("A demon appears!");

        if (!player_angers_monster(&menv[mons]) && !friendly)
        {
            mpr(charmed ? "You don't feel so good about this..."
                        : "It doesn't seem very happy.");
        }
        else if (friendly
                    && (mon == MONS_CRIMSON_IMP || mon == MONS_WHITE_IMP
                        || mon == MONS_IRON_IMP || mon == MONS_SHADOW_IMP))
        {
            std::string msg = getSpeakString("_friendly_imp_greeting");
            execute_embedded_lua(msg);
            if (msg == "__NONE")
                msg.clear();
            mons_speaks_msg(&menv[mons], msg, MSGCH_TALK,
                            silenced(menv[mons].pos()));
        }
    }

    return (success);
}

static bool _summon_demon_wrapper(int pow, god_type god, int spell,
                                  demon_class_type dct, int dur, bool friendly,
                                  bool charmed, bool quiet)
{
    monster_type mon = summon_any_demon(dct);

    return _summon_demon_wrapper(pow, god, spell, mon, dur, friendly, charmed,
                                 quiet);
}

bool _summon_lesser_demon(int pow, god_type god, int spell,
                          bool quiet)
{
    return _summon_demon_wrapper(pow, god, spell, DEMON_LESSER,
                                 std::min(2 + (random2(pow) / 4), 6),
                                 random2(pow) > 3, false, quiet);
}

bool _summon_common_demon(int pow, god_type god, int spell,
                          bool quiet)
{
    return _summon_demon_wrapper(pow, god, spell, DEMON_COMMON,
                                 std::min(2 + (random2(pow) / 4), 6),
                                 random2(pow) > 3, false, quiet);
}

bool summon_greater_demon(int pow, god_type god, int spell,
                          bool quiet)
{
    monster_type mon = summon_any_demon(DEMON_GREATER);

    const bool charmed = (random2(pow) > 5);
    const bool friendly = (charmed && mons_demon_tier(mon) == 2);

    return _summon_demon_wrapper(pow, god, spell, mon,
                                 5, friendly, charmed, quiet);
}

bool summon_demon_type(monster_type mon, int pow, god_type god,
                       int spell)
{
    return _summon_demon_wrapper(pow, god, spell, mon,
                                 std::min(2 + (random2(pow) / 4), 6),
                                 random2(pow) > 3, false, false);
}

spret_type cast_summon_demon(int pow, god_type god, bool fail)
{
    fail_check();
    mpr("You open a gate to Pandemonium!");

    if (!_summon_common_demon(pow, god, SPELL_SUMMON_DEMON, false))
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_demonic_horde(int pow, god_type god, bool fail)
{
    fail_check();
    mpr("You open a gate to Pandemonium!");

    bool success = false;

    const int how_many = 7 + random2(5);

    for (int i = 0; i < how_many; ++i)
    {
        if (_summon_lesser_demon(pow, god, SPELL_DEMONIC_HORDE, true))
            success = true;
    }

    if (!success)
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_summon_greater_demon(int pow, god_type god, bool fail)
{
    fail_check();
    mpr("You open a gate to Pandemonium!");

    if (!summon_greater_demon(pow, god, SPELL_SUMMON_GREATER_DEMON))
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_shadow_creatures(god_type god, bool fail)
{
    fail_check();
    mpr("Wisps of shadow whirl around you...");

    const int mons =
        create_monster(
            mgen_data(RANDOM_MOBILE_MONSTER, BEH_FRIENDLY, &you,
                      1, // This duration is only used for band members.
                      SPELL_SHADOW_CREATURES, you.pos(), MHITYOU,
                      MG_FORCE_BEH, god), false);

    if (mons != -1)
    {
        // Choose a new duration based on HD.
        int x = std::max(menv[mons].hit_dice - 3, 1);
        int d = div_rand_round(17,x);
        if (d < 1)
            d = 1;
        if (d > 4)
            d = 4;
        mon_enchant me = mon_enchant(ENCH_ABJ, d);
        me.set_duration(&menv[mons], &me);
        menv[mons].update_ench(me);
        player_angers_monster(&menv[mons]);
    }
    else
        mpr("The shadows disperse without effect.");

    return SPRET_SUCCESS;
}

bool can_cast_malign_gateway()
{
    timeout_malign_gateways(0);

    return count_malign_gateways() < 1;
}

coord_def find_gateway_location (actor* caster, bool (*environment_checker)(dungeon_feature_type))
{
    coord_def point = coord_def(0, 0);

    bool xray = you.xray_vision;
    you.xray_vision = false;

    unsigned compass_idx[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    std::random_shuffle(compass_idx, compass_idx + 8);

    bool check_environment = (environment_checker != NULL);

    for (unsigned i = 0; i < 8; ++i)
    {
        coord_def delta = Compass[compass_idx[i]];
        coord_def test = coord_def(-1, -1);

        bool found_spot = false;
        int tries = 8;

        for (int t = 0; t < tries; t++)
        {
            test = caster->pos() + (delta * (2+t+random2(4)));
            if (!in_bounds(test) || check_environment && !feat_is_test(test, environment_checker)
                || actor_at(test) || count_neighbours_with_func(test, &feat_is_solid) != 0
                || !caster->see_cell(test))
            {
                continue;
            }

            found_spot = true;
            break;
        }

        if (!found_spot)
            continue;

        point = test;
        break;
    }

    you.xray_vision = xray;

    return (point);
}

spret_type cast_malign_gateway(actor * caster, int pow, god_type god, bool fail)
{
    fail_check();
    coord_def point = find_gateway_location(caster);
    bool success = (point != coord_def(0, 0));

    bool is_player = (caster->atype() == ACT_PLAYER);

    if (success)
    {
        const int malign_gateway_duration = BASELINE_DELAY * (random2(5) + 5);
        env.markers.add(new map_malign_gateway_marker(point,
                                malign_gateway_duration,
                                is_player,
                                is_player ? "" : caster->as_monster()->full_name(DESC_NOCAP_A, true),
                                is_player ? BEH_FRIENDLY : attitude_creation_behavior(caster->as_monster()->attitude),
                                god,
                                pow));
        env.markers.clear_need_activate();
        env.grid(point) = DNGN_MALIGN_GATEWAY;

        noisy(10, point);
        mpr("The dungeon shakes, a horrible noise fills the air, and a portal to some otherworldly place is opened!", MSGCH_WARN);

        if (one_chance_in(5) && caster->atype() == ACT_PLAYER)
        {
            // if someone deletes the db, no message is ok
            mpr(getMiscString("SHT_int_loss").c_str());
            // Messages the same as for SHT, as they are currently (10/10) generic.
            lose_stat(STAT_INT, 1 + random2(3), false, "opening a malign portal");
        }
    }
    // We don't care if monsters fail to cast it.
    else if (is_player)
        mpr("A gateway cannot be opened in this cramped space!");

    return SPRET_SUCCESS;
}


spret_type cast_summon_horrible_things(int pow, god_type god, bool fail)
{
    fail_check();
    if (one_chance_in(5))
    {
        // if someone deletes the db, no message is ok
        mpr(getMiscString("SHT_int_loss").c_str());
        lose_stat(STAT_INT, 1 + random2(3), false, "summoning horrible things");
    }

    int how_many_small =
        stepdown_value(2 + (random2(pow) / 10) + (random2(pow) / 10),
                       2, 2, 6, -1);
    int how_many_big = 0;

    // No more than 2 tentacled monstrosities.
    while (how_many_small > 2 && how_many_big < 2 && one_chance_in(3))
    {
        how_many_small -= 2;
        how_many_big++;
    }

    // No more than 8 summons.
    how_many_small = std::min(8, how_many_small);
    how_many_big   = std::min(8, how_many_big);

    int count = 0;

    while (how_many_big-- > 0)
    {
        const int mons =
            create_monster(
               mgen_data(MONS_TENTACLED_MONSTROSITY, BEH_FRIENDLY, &you,
                         6, SPELL_SUMMON_HORRIBLE_THINGS,
                         you.pos(), MHITYOU,
                         MG_FORCE_BEH, god));

        if (mons != -1)
        {
            count++;
            player_angers_monster(&menv[mons]);
        }
    }

    while (how_many_small-- > 0)
    {
        const int mons =
            create_monster(
               mgen_data(MONS_ABOMINATION_LARGE, BEH_FRIENDLY, &you,
                         6, SPELL_SUMMON_HORRIBLE_THINGS,
                         you.pos(), MHITYOU,
                         MG_FORCE_BEH, god));

        if (mons != -1)
        {
            count++;
            player_angers_monster(&menv[mons]);
        }
    }

    if (!count)
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

static bool _animatable_remains(const item_def& item)
{
    return (item.base_type == OBJ_CORPSES
        && mons_class_can_be_zombified(item.plus));
}

// Try to equip the skeleton/zombie with the objects it died with.  This
// excludes holy items, items which were dropped by the player onto the
// corpse, and corpses which were picked up and moved by the player, so
// the player can't equip their undead slaves with items of their
// choice.
//
// The item selection logic has one problem: if a first monster without
// any items dies and leaves a corpse, and then a second monster with
// items dies on the same spot but doesn't leave a corpse, then the
// undead can be equipped with the second monster's items if the second
// monster is either of the same type as the first, or if the second
// monster wasn't killed by the player or a player's pet.
static void _equip_undead(const coord_def &a, int corps, int mons, int monnum)
{
    monster* mon = &menv[mons];

    if (mons_class_itemuse(monnum) < MONUSE_STARTING_EQUIPMENT)
        return;

    // If the player picked up and dropped the corpse, then all its
    // original equipment fell off.
    if (mitm[corps].flags & ISFLAG_DROPPED)
        return;

    // A monster's corpse is last in the linked list after its items,
    // so (for example) the first item after the second-to-last corpse
    // is the first item belonging to the last corpse.
    int objl      = igrd(a);
    int first_obj = NON_ITEM;

    while (objl != NON_ITEM && objl != corps)
    {
        item_def item(mitm[objl]);

        if (item.base_type != OBJ_CORPSES && first_obj == NON_ITEM)
            first_obj = objl;

        objl = item.link;
    }

    // If the corpse was being drained when it was raised the item is
    // already destroyed.
    ASSERT(objl == corps || objl == NON_ITEM);

    if (first_obj == NON_ITEM)
        return;

    // Iterate backwards over the list, since the items earlier in the
    // linked list were dropped most recently and hence more likely to
    // be items the monster didn't die with.
    std::vector<int> item_list;
    objl = first_obj;
    while (objl != NON_ITEM && objl != corps)
    {
        item_list.push_back(objl);
        objl = mitm[objl].link;
    }

    for (int i = item_list.size() - 1; i >= 0; --i)
    {
        objl = item_list[i];
        item_def &item(mitm[objl]);

        // Stop equipping monster if the item probably didn't originally
        // belong to the monster.
        if ((origin_known(item) && (item.orig_monnum - 1) != monnum)
            || (item.flags & (ISFLAG_DROPPED | ISFLAG_THROWN))
            || item.base_type == OBJ_CORPSES)
        {
            return;
        }

        // Don't equip the undead with holy items.
        if (is_holy_item(item))
            continue;

        mon_inv_type mslot;

        switch (item.base_type)
        {
        case OBJ_WEAPONS:
        case OBJ_STAVES:
        {
            const bool weapon = mon->inv[MSLOT_WEAPON] != NON_ITEM;
            const bool alt_weapon = mon->inv[MSLOT_ALT_WEAPON] != NON_ITEM;

            if ((weapon && !alt_weapon) || (!weapon && alt_weapon))
                mslot = !weapon ? MSLOT_WEAPON : MSLOT_ALT_WEAPON;
            else
                mslot = MSLOT_WEAPON;

            // Stupid undead can't use ranged weapons.
            if (!is_range_weapon(item))
                break;

            continue;
        }

        case OBJ_ARMOUR:
            mslot = equip_slot_to_mslot(get_armour_slot(item));

            // A piece of armour which can't be worn indicates that this
            // and further items weren't the equipment the monster died
            // with.
            if (mslot == NUM_MONSTER_SLOTS)
                return;
            break;

        // Stupid undead can't use missiles.
        case OBJ_MISSILES:
            continue;

        // Stupid undead can't use gold.
        case OBJ_GOLD:
            continue;

        // Stupid undead can't use wands.
        case OBJ_WANDS:
            continue;

        // Stupid undead can't use scrolls.
        case OBJ_SCROLLS:
            continue;

        // Stupid undead can't use potions.
        case OBJ_POTIONS:
            continue;

        // Stupid undead can't use miscellaneous objects.
        case OBJ_MISCELLANY:
            continue;

        default:
            continue;
        }

        // Two different items going into the same slot indicate that
        // this and further items weren't equipment the monster died
        // with.
        if (mon->inv[mslot] != NON_ITEM)
            return;

        unwind_var<int> save_speedinc(mon->speed_increment);
        mon->pickup_item(mitm[objl], false, true);
    }
}

// Displays message when raising dead with Animate Skeleton or Animate Dead.
static void _display_undead_motions(int motions)
{
    std::vector<std::string> motions_list;

    // Check bitfield from _raise_remains for types of corpse(s) being animated.
    if (motions & DEAD_ARE_WALKING)
        motions_list.push_back("walking");
    if (motions & DEAD_ARE_HOPPING)
        motions_list.push_back("hopping");
    if (motions & DEAD_ARE_FLOATING)
        motions_list.push_back("floating");
    if (motions & DEAD_ARE_SWIMMING)
        motions_list.push_back("swimming");
    if (motions & DEAD_ARE_FLYING)
        motions_list.push_back("flying");
    if (motions & DEAD_ARE_SLITHERING)
        motions_list.push_back("slithering");

    // Prevents the message from getting too long and spammy.
    if (motions_list.size() > 3)
        mpr("The dead have arisen!");
    else
        mpr("The dead are " + comma_separated_line(motions_list.begin(),
            motions_list.end()) + "!");
}

static bool _raise_remains(const coord_def &pos, int corps, beh_type beha,
                           unsigned short hitting, actor *as, std::string nas,
                           god_type god, bool actual, bool force_beh,
                           int* mon_index, int* motions_r)
{
    if (mon_index != NULL)
        *mon_index = -1;

    const item_def& item = mitm[corps];

    if (!_animatable_remains(item))
        return (false);

    if (!actual)
        return (true);

    const monster_type zombie_type =
        static_cast<monster_type>(item.plus);

    const int hd     = (item.props.exists(MONSTER_HIT_DICE)) ?
                           item.props[MONSTER_HIT_DICE].get_short() : 0;
    const int number = (item.props.exists(MONSTER_NUMBER)) ?
                           item.props[MONSTER_NUMBER].get_short() : 0;

    // Save the corpse name before because it can get destroyed if it is
    // being drained and the raising interrupts it.
    uint64_t name_type = 0;
    std::string name;
    if (is_named_corpse(item))
        name = get_corpse_name(item, &name_type);

    // Headless hydras cannot be raised, sorry.
    if (zombie_type == MONS_HYDRA && number == 0)
    {
        if (you.see_cell(pos))
        {
            mprf("The headless hydra %s sways and immediately collapses!",
                 item.sub_type == CORPSE_BODY ? "corpse" : "skeleton");
        }
        return (false);
    }

    monster_type mon = MONS_PROGRAM_BUG;

    if (item.sub_type == CORPSE_BODY)
    {
        mon = (mons_zombie_size(item.plus) == Z_SMALL) ? MONS_ZOMBIE_SMALL
                                                       : MONS_ZOMBIE_LARGE;
    }
    else
    {
        mon = (mons_zombie_size(item.plus) == Z_SMALL) ? MONS_SKELETON_SMALL
                                                       : MONS_SKELETON_LARGE;
    }

    const int monnum = item.orig_monnum - 1;

    // Use the original monster type as the zombified type here, to get
    // the proper stats from it.
    mgen_data mg(mon, beha, as, 0, 0, pos, hitting, MG_FORCE_BEH|MG_FORCE_PLACE,
                 god, static_cast<monster_type>(monnum), number);

    // No experience for monsters animated by god wrath or the Sword of
    // Zonguldrok.
    if (nas != "")
        mg.extra_flags |= MF_NO_REWARD;

    mg.non_actor_summoner = nas;

    const int mons = create_monster(mg);

    if (mon_index != NULL)
        *mon_index = mons;

    if (mons == -1)
        return (false);

    // If the original monster has been drained or levelled up, its HD
    // might be different from its class HD, in which case its HP should
    // be rerolled to match.
    if (menv[mons].hit_dice != hd)
    {
        menv[mons].hit_dice = std::max(hd, 1);
        roll_zombie_hp(&menv[mons]);
    }

    if (item.props.exists("ev"))
        menv[mons].ev = item.props["ev"].get_int();
    if (item.props.exists("ac"))
        menv[mons].ac = item.props["ac"].get_int();

    if (!name.empty()
        && (name_type == 0 || (name_type & MF_NAME_MASK) == MF_NAME_REPLACE))
    {
        name_zombie(&menv[mons], monnum, name);
    }

    // Re-equip the zombie.
    _equip_undead(pos, corps, mons, monnum);

    // Destroy the monster's corpse, as it's no longer needed.
    destroy_item(corps);

    if (!force_beh)
        player_angers_monster(&menv[mons]);

    // Bitfield for motions - determines text displayed when animating dead.
    if (mons_class_primary_habitat(zombie_type)    == HT_WATER
        || mons_class_primary_habitat(zombie_type) == HT_LAVA)
    {
        *motions_r |= DEAD_ARE_SWIMMING;
    }
    else if (mons_class_flies(zombie_type) == FL_FLY)
        *motions_r |= DEAD_ARE_FLYING;
    else if (mons_class_flies(zombie_type) == FL_LEVITATE)
        *motions_r |= DEAD_ARE_FLOATING;
    else if (mons_genus(zombie_type)    == MONS_SNAKE
             || mons_genus(zombie_type) == MONS_NAGA
             || mons_genus(zombie_type) == MONS_GUARDIAN_SERPENT
             || mons_genus(zombie_type) == MONS_GIANT_SLUG
             || mons_genus(zombie_type) == MONS_WORM)
    {
        *motions_r |= DEAD_ARE_SLITHERING;
    }
    else if (mons_genus(zombie_type)    == MONS_GIANT_FROG
             || mons_genus(zombie_type) == MONS_BLINK_FROG)
        *motions_r |= DEAD_ARE_HOPPING;
    else
        *motions_r |= DEAD_ARE_WALKING;

    return (true);
}

// Note that quiet will *not* suppress the message about a corpse
// you are butchering being animated.
// This is called for Animate Skeleton and from animate_dead.
int animate_remains(const coord_def &a, corpse_type class_allowed,
                    beh_type beha, unsigned short hitting,
                    actor *as, std::string nas,
                    god_type god, bool actual,
                    bool quiet, bool force_beh,
                    int* mon_index, int* motions_r)
{
    if (is_sanctuary(a))
        return (0);

    int number_found = 0;
    bool success = false;
    int motions = 0;

    // Search all the items on the ground for a corpse.
    for (stack_iterator si(a); si; ++si)
    {
        if (si->base_type == OBJ_CORPSES
            && (class_allowed == CORPSE_BODY
                || si->sub_type == CORPSE_SKELETON))
        {
            number_found++;

            if (!_animatable_remains(*si))
                continue;

            const bool was_draining = is_being_drained(*si);
            const bool was_butchering = is_being_butchered(*si);

            success = _raise_remains(a, si.link(), beha, hitting, as, nas,
                                     god, actual, force_beh, mon_index,
                                     &motions);

            if (actual && success)
            {
                // Ignore quiet.
                if (was_butchering || was_draining)
                {
                    mprf("The corpse you are %s rises to %s!",
                         was_draining ? "drinking from"
                                      : "butchering",
                         beha == BEH_FRIENDLY ? "join your ranks"
                                              : "attack");
                }

                if (!quiet && you.see_cell(a))
                    _display_undead_motions(motions);

                if (was_butchering)
                    xom_is_stimulated(200);
            }

            break;
        }
    }

    if (motions_r)
        *motions_r |= motions;

    if (number_found == 0)
        return (-1);

    if (!success)
        return (0);

    return (1);
}

int animate_dead(actor *caster, int pow, beh_type beha, unsigned short hitting,
                 actor *as, std::string nas, god_type god, bool actual)
{
    UNUSED(pow);

    int number_raised = 0;
    int number_seen   = 0;
    int motions       = 0;

    radius_iterator ri(caster->pos(), LOS_RADIUS, C_ROUND,
                       caster->get_los_no_trans());

    for (; ri; ++ri)
    {
        // There may be many corpses on the same spot.
        while (animate_remains(*ri, CORPSE_BODY, beha, hitting, as, nas, god,
                               actual, true, 0, 0, &motions) > 0)
        {
            number_raised++;
            if (you.see_cell(*ri))
                number_seen++;

            // For the tracer, we don't care about exact count (and the
            // corpse is not gone).
            if (!actual)
                break;
        }
    }

    if (actual && number_seen > 0)
        _display_undead_motions(motions);

    return (number_raised);
}

// XXX: we could check if there's any corpse or skeleton and abort
// freely before doing any butchering and dead raising.
spret_type cast_animate_skeleton(god_type god, bool fail)
{
    fail_check();
    mpr("You attempt to give life to the dead...");

    // First, we try to animate a skeleton if there is one.
    if (animate_remains(you.pos(), CORPSE_SKELETON, BEH_FRIENDLY,
                        MHITYOU, &you, "", god) != -1)
    {
        return SPRET_SUCCESS;
    }

    // If not, look for a corpse and butcher it.
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY
            && mons_skeleton(si->plus)
            && mons_class_can_be_zombified(si->plus))
        {
            turn_corpse_into_skeleton_and_chunks(*si);
            mpr("Before your eyes, flesh is ripped from the corpse!");
            if (Options.chunks_autopickup)
                request_autopickup();
            // Only convert the top one.
            break;
        }
    }

    // Now we try again to animate a skeleton.
    if (animate_remains(you.pos(), CORPSE_SKELETON, BEH_FRIENDLY,
                        MHITYOU, &you, "", god) < 0)
    {
        mpr("There is no skeleton here to animate!");
    }

    return SPRET_SUCCESS;
}

spret_type cast_animate_dead(int pow, god_type god, bool fail)
{
    fail_check();
    mpr("You call on the dead to rise...");

    if (!animate_dead(&you, pow + 1, BEH_FRIENDLY, MHITYOU, &you, "", god))
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

// Simulacrum
//
// This spell extends creating undead to Ice mages, as such it's high
// level, requires wielding of the material component, and the undead
// aren't overly powerful (they're also vulnerable to fire).  I've put
// back the abjuration level in order to keep down the army sizes again.
//
// As for what it offers necromancers considering all the downsides
// above... it allows the turning of a single corpse into an army of
// monsters (one per food chunk)... which is also a good reason for
// why it's high level.
//
// Hides and other "animal part" items are intentionally left out, it's
// unrequired complexity, and fresh flesh makes more "sense" for a spell
// reforming the original monster out of ice anyway.
spret_type cast_simulacrum(int pow, god_type god, bool fail)
{
    int count = 0;

    const item_def* weapon = you.weapon();

    if (weapon
        && (weapon->base_type == OBJ_CORPSES
            || (weapon->base_type == OBJ_FOOD
                && weapon->sub_type == FOOD_CHUNK)))
    {
        const monster_type sim_type = static_cast<monster_type>(weapon->plus);

        if (!mons_class_can_be_zombified(sim_type))
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            return SPRET_ABORT;
        }

        fail_check();

        const monster_type mon = mons_zombie_size(sim_type) == Z_BIG ?
            MONS_SIMULACRUM_LARGE : MONS_SIMULACRUM_SMALL;

        // Can't create more than the available chunks.
        int how_many = std::min(8, 4 + random2(pow) / 20);
        how_many = std::min<int>(how_many, weapon->quantity);

        const int monnum = weapon->orig_monnum - 1;

        for (int i = 0; i < how_many; ++i)
        {
            // Use the original monster type as the zombified type here,
            // to get the proper stats from it.
            const int mons =
                create_monster(
                    mgen_data(mon, BEH_FRIENDLY, &you,
                              0, SPELL_SIMULACRUM,
                              you.pos(), MHITYOU,
                              MG_FORCE_BEH, god,
                              static_cast<monster_type>(monnum)));

            if (mons != -1)
            {
                count++;

                dec_inv_item_quantity(you.equip[EQ_WEAPON], 1);

                player_angers_monster(&menv[mons]);

                monster* sim = &menv[mons];
                sim->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 6));
            }
        }

        if (!count)
            canned_msg(MSG_NOTHING_HAPPENS);

        return SPRET_SUCCESS;
    }
    else
    {
        mpr("You need to wield a piece of raw flesh for this spell to be "
            "effective!");
        return SPRET_ABORT;
    }
}

// Make the proper stat adjustments to turn a demonic abomination into
// an undead abomination.
bool undead_abomination_convert(monster* mon, int hd)
{
    if (mon->type == MONS_CRAWLING_CORPSE
        || mon->type == MONS_MACABRE_MASS)
    {
        mon->hit_points = mon->max_hit_points = mon->hit_dice = hd;
        return (true);
    }
    else if (mon->type != MONS_ABOMINATION_LARGE
             && mon->type != MONS_ABOMINATION_SMALL)
    {
        return (false);
    }

    const int max_hd = mon->type == MONS_ABOMINATION_LARGE ? 30 : 15;
    const int max_ac = mon->type == MONS_ABOMINATION_LARGE ? 20 : 10;

    // Mark this abomination as undead.
    mon->flags |= MF_FAKE_UNDEAD;

    mon->colour = ((hd > 2 * max_hd / 3) ? LIGHTRED : RED);

    mon->hit_dice = std::min(max_hd, hd);

    mon->max_hit_points = std::max(1, hit_points(mon->hit_dice, 2, 5));
    mon->hit_points     = mon->max_hit_points;

    mon->ac = std::min(max_ac, 2 * hd / 3);

    return (true);
}

// Return a definite/indefinite article for (number) things.
static const char *_count_article(int number, bool definite)
{
    if (number == 0)
        return ("No");
    else if (definite)
        return ("The");
    else if (number == 1)
        return ("A");
    else
        return ("Some");
}

bool twisted_resurrection(actor *caster, int pow, beh_type beha,
                          unsigned short foe, god_type god, bool actual)
{
    int num_orcs = 0;
    int num_holy = 0;
    int num_crawlies = 0;
    int num_masses = 0;
    int num_lost = 0;
    int num_lost_piles = 0;

    radius_iterator ri(caster->pos(), LOS_RADIUS, C_ROUND,
                       caster->get_los_no_trans());

    for (; ri; ++ri)
    {
        int num_corpses = 0;
        int total_mass = 0;

        // Count up number/size of corpses at this location.
        for (stack_iterator si(*ri); si; ++si)
        {
            if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY)
            {
                if (!actual)
                    return (true);

                if (mons_genus(si->plus) == MONS_ORC)
                    num_orcs++;
                if (mons_class_holiness(si->plus) == MH_HOLY)
                    num_holy++;

                if (food_is_rotten(*si))
                    total_mass += mons_weight(si->plus) / 4;
                else
                    total_mass += mons_weight(si->plus);

                ++num_corpses;
                destroy_item(si->index());
            }
        }

        if (num_corpses == 0)
            continue;

        // 20 aum per HD at max power; 30 at 100 power; and 60 at 0 power.
        int hd = div_rand_round((pow + 100) * total_mass, (200*300));

        if (hd <= 0)
        {
            num_lost += num_corpses;
            num_lost_piles++;
            continue;
        }

        // Getting a huge abomination shouldn't be too easy.
        if (hd > 15)
            hd = 15 + (hd - 15) / 2;

        hd = std::min(hd, 30);

        monster_type montype;

        if (hd >= 11 && num_corpses > 2)
            montype = MONS_ABOMINATION_LARGE;
        else if (hd >= 6 && num_corpses > 1)
            montype = MONS_ABOMINATION_SMALL;
        else if (num_corpses > 1)
            montype = MONS_MACABRE_MASS;
        else
            montype = MONS_CRAWLING_CORPSE;

        mgen_data mg(montype, beha, caster, 0, 0, *ri, foe, MG_FORCE_BEH, god);
        const int mons = create_monster(mg);

        if (mons != -1)
        {
            // Set hit dice, AC, and HP.
            undead_abomination_convert(&menv[mons], hd);

            // Override Lugonu/Makhleb, since these are not really demonic.
            menv[mons].god = god;

            if (num_corpses > 1)
                ++num_masses;
            else
                ++num_crawlies;
        }
        else
        {
            num_lost += num_corpses;
            num_lost_piles++;
        }
    }

    if (num_lost + num_crawlies + num_masses == 0)
        return (false);

    // The tracer should have stopped at the first corpse, or found no
    // corpses and returned false.
    ASSERT(actual);

    if (num_lost)
        mprf("%s %s into %s!",
             _count_article(num_lost, num_crawlies + num_masses == 0),
             num_lost == 1 ? "corpse collapses" : "corpses collapse",
             num_lost_piles == 1 ? "a pulpy mess" : "pulpy messes");

    if (num_crawlies > 0)
        mprf("%s %s to drag %s along the ground!",
             _count_article(num_crawlies, num_lost + num_masses == 0),
             num_crawlies == 1 ? "corpse begins" : "corpses begin",
             num_crawlies == 1 ? "itself" : "themselves");

    if (num_masses > 0)
        mprf("%s corpses meld into %s of writhing flesh!",
             _count_article(2, num_crawlies + num_lost == 0),
             num_masses == 1 ? "an agglomeration" : "agglomerations");

    if (num_orcs > 0 && caster == &you)
        did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2 * num_orcs);
    if (num_holy > 0 && caster == &you)
        did_god_conduct(DID_DESECRATE_HOLY_REMAINS, 2 * num_holy);

    return (true);
}

spret_type cast_twisted_resurrection(int pow, god_type god, bool fail)
{
    if (twisted_resurrection(&you, pow, BEH_FRIENDLY, MHITYOU, god, !fail))
        return (fail ? SPRET_FAIL : SPRET_SUCCESS);
    else
    {
        mpr("There are no corpses here!");
        return SPRET_ABORT;
    }
}

spret_type cast_haunt(int pow, const coord_def& where, god_type god, bool fail)
{
    monster* m = monster_at(where);

    if (m == NULL)
    {
        fail_check();
        mpr("An evil force gathers, but it quickly dissipates.");
        return SPRET_SUCCESS; // still losing a turn
    }

    int mi = m->mindex();
    ASSERT(!invalid_monster_index(mi));

    if (stop_attack_prompt(m, false, you.pos()))
        return SPRET_ABORT;

    fail_check();

    bool friendly = true;
    int success = 0;
    int to_summon = stepdown_value(2 + (random2(pow) / 10) + (random2(pow) / 10),
                                   2, 2, 6, -1);

    while (to_summon--)
    {
        const int chance = random2(25);
        monster_type mon = ((chance > 22) ? MONS_PHANTOM :            //  8%
                            (chance > 20) ? MONS_HUNGRY_GHOST :       //  8%
                            (chance > 18) ? MONS_FLAYED_GHOST :       //  8%
                            (chance > 16) ? MONS_SHADOW_WRAITH:       //  8%
                            (chance >  6) ? MONS_WRAITH :             // 40%
                            (chance >  2) ? MONS_FREEZING_WRAITH      // 16%
                                          : MONS_PHANTASMAL_WARRIOR); // 12%

        const int mons =
            create_monster(
                mgen_data(mon,
                          BEH_FRIENDLY, &you,
                          3, SPELL_HAUNT,
                          where, mi, MG_FORCE_BEH, god));

        if (mons != -1)
        {
            success++;

            if (player_angers_monster(&menv[mons]))
                friendly = false;
        }
    }

    if (success > 1)
        mpr(friendly ? "Insubstantial figures form in the air."
                     : "You sense hostile presences.");
    else if (success)
        mpr(friendly ? "An insubstantial figure forms in the air."
                     : "You sense a hostile presence.");
    else
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_SUCCESS;
    }

    //jmf: Kiku sometimes deflects this
    if (you.religion != GOD_KIKUBAAQUDGHA
        || player_under_penance() || you.piety < piety_breakpoint(3)
        || !x_chance_in_y(you.piety, MAX_PIETY))
    {
        you.sicken(25 + random2(50));
    }

    return SPRET_SUCCESS;
}

static int _abjuration(int pow, monster *mon)
{
    // Scale power into something comparable to summon lifetime.
    const int abjdur = pow * 12;

    // XXX: make this a prompt
    if (mon->wont_attack())
        return (false);

    int duration;
    if (mon->is_summoned(&duration))
    {
        int sockage = std::max(fuzz_value(abjdur, 60, 30), 40);
        dprf("%s abj: dur: %d, abj: %d",
             mon->name(DESC_PLAIN).c_str(), duration, sockage);

        bool shielded = false;
        // TSO and Trog's abjuration protection.
        if (mons_is_god_gift(mon, GOD_SHINING_ONE))
        {
            sockage = sockage * (30 - mon->hit_dice) / 45;
            if (sockage < duration)
            {
                simple_god_message(" protects a fellow warrior from your evil magic!",
                                   GOD_SHINING_ONE);
                shielded = true;
            }
        }
        else if (mons_is_god_gift(mon, GOD_TROG))
        {
            sockage = sockage * 8 / 15;
            if (sockage < duration)
            {
                simple_god_message(" shields an ally from your puny magic!",
                                   GOD_TROG);
                shielded = true;
            }
        }

        mon_enchant abj = mon->get_ench(ENCH_ABJ);
        if (!mon->lose_ench_duration(abj, sockage) && !shielded)
            simple_monster_message(mon, gettext(" shudders."));
    }

    return (true);
}

spret_type cast_abjuration(int pow, monster *mon, bool fail)
{
    fail_check();
    if (mon)
    {
        mpr(gettext("Send 'em back where they came from!"));
        _abjuration(pow, mon);
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_mass_abjuration(int pow, bool fail)
{
    fail_check();
    mpr(gettext("Send 'em back where they came from!"));
    for (monster_iterator mi(you.get_los()); mi; ++mi)
    {
        _abjuration(pow, *mi);
    }

    return SPRET_SUCCESS;
}
