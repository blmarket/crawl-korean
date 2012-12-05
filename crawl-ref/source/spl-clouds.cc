/**
 * @file
 * @brief Cloud creating spells.
**/

#include "AppHdr.h"

#include "spl-clouds.h"
#include "externs.h"

#include <algorithm>

#include "beam.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "fprop.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-util.h"
#include "ouch.h"
#include "player.h"
#include "skills.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "transform.h"
#include "viewchar.h"
#include "shout.h"

static void _burn_tree(coord_def pos)
{
    bolt beam;
    beam.origin_spell = SPELL_CONJURE_FLAME;
    beam.range = 1;
    beam.flavour = BEAM_FIRE;
    beam.name = "fireball"; // yay doing this by name
    beam.source = beam.target = pos;
    beam.set_agent(&you);
    beam.fire();
}

spret_type conjure_flame(int pow, const coord_def& where, bool fail)
{
    // FIXME: This would be better handled by a flag to enforce max range.
    if (distance2(where, you.pos()) > dist_range(spell_range(SPELL_CONJURE_FLAME,
                                                      pow))
        || !in_bounds(where))
    {
        mpr(gettext("That's too far away."));
        return SPRET_ABORT;
    }

    if (cell_is_solid(where))
    {
        switch (grd(where))
        {
        case DNGN_METAL_WALL:
            mpr(gettext("You can't ignite solid metal!"));
            break;
        case DNGN_GREEN_CRYSTAL_WALL:
            mpr(gettext("You can't ignite solid crystal!"));
            break;
        case DNGN_TREE:
        case DNGN_MANGROVE:
            fail_check();
            _burn_tree(where);
            return SPRET_SUCCESS;
        default:
            mpr(gettext("You can't ignite solid rock!"));
            break;
        }
        return SPRET_ABORT;
    }

    const int cloud = env.cgrid(where);

    if (cloud != EMPTY_CLOUD && env.cloud[cloud].type != CLOUD_FIRE)
    {
        mpr(gettext("There's already a cloud there!"));
        return SPRET_ABORT;
    }

    // Note that self-targetting is handled by SPFLAG_NOT_SELF.
    monster* mons = monster_at(where);
    if (mons)
    {
        if (you.can_see(mons))
        {
            mpr(gettext("You can't place the cloud on a creature."));
            return SPRET_ABORT;
        }

        fail_check();

        // FIXME: maybe should do _paranoid_option_disable() here?
        mpr(gettext("You see a ghostly outline there, and the spell fizzles."));
        return SPRET_SUCCESS;      // Don't give free detection!
    }

    fail_check();

    if (cloud != EMPTY_CLOUD)
    {
        // Reinforce the cloud - but not too much.
        // It must be a fire cloud from a previous test.
        mpr(_("The fire roars with new energy!"));
        const int extra_dur = 2 + min(random2(pow) / 2, 20);
        env.cloud[cloud].decay += extra_dur * 5;
        env.cloud[cloud].set_whose(KC_YOU);
    }
    else
    {
        const int durat = min(5 + (random2(pow)/2) + (random2(pow)/2), 23);
        place_cloud(CLOUD_FIRE, where, durat, &you);
        mpr(gettext("The fire roars!"));
    }
    noisy(2, where);

    return SPRET_SUCCESS;
}

// Assumes beem.range has already been set. -cao
spret_type stinking_cloud(int pow, bolt &beem, bool fail)
{
    beem.name        = "stinking cloud";
    beem.colour      = GREEN;
    beem.damage      = dice_def(1, 0);
    beem.hit         = 20;
    beem.glyph       = dchar_glyph(DCHAR_FIRED_ZAP);
    beem.flavour     = BEAM_POTION_MEPHITIC;
    beem.ench_power  = pow;
    beem.beam_source = MHITYOU;
    beem.thrower     = KILL_YOU;
    beem.is_beam     = false;
    beem.is_explosion = true;
    beem.aux_source.clear();

    // Fire tracer.
    beem.source            = you.pos();
    beem.can_see_invis     = you.can_see_invisible();
    beem.smart_monster     = true;
    beem.attitude          = ATT_FRIENDLY;
    beem.friend_info.count = 0;
    beem.is_tracer         = true;
    beem.fire();

    if (beem.beam_cancelled)
    {
        // We don't want to fire through friendlies.
        canned_msg(MSG_OK);
        return SPRET_ABORT;
    }

    fail_check();

    // Really fire.
    beem.is_tracer = false;
    beem.fire();

    return SPRET_SUCCESS;
}

spret_type cast_big_c(int pow, cloud_type cty, const actor *caster, bolt &beam,
                      bool fail)
{
    if (distance2(beam.target, you.pos()) > dist_range(beam.range)
        || !in_bounds(beam.target))
    {
        mpr(gettext("That is beyond the maximum range."));
        return SPRET_ABORT;
    }

    if (cell_is_solid(beam.target))
    {
        mpr(gettext("You can't place clouds on a wall."));
        return SPRET_ABORT;
    }

    fail_check();

    big_cloud(cty, caster, beam.target, pow, 8 + random2(3), -1);
    noisy(2, beam.target);
    return SPRET_SUCCESS;
}

static int _make_a_normal_cloud(coord_def where, int pow, int spread_rate,
                                cloud_type ctype, const actor *agent, int colour,
                                string name, string tile, int excl_rad)
{
    place_cloud(ctype, where,
                (3 + random2(pow / 4) + random2(pow / 4) + random2(pow / 4)),
                agent, spread_rate, colour, name, tile, excl_rad);

    return 1;
}

void big_cloud(cloud_type cl_type, const actor *agent,
               const coord_def& where, int pow, int size, int spread_rate,
               int colour, string name, string tile)
{
    apply_area_cloud(_make_a_normal_cloud, where, pow, size,
                     cl_type, agent, spread_rate, colour, name, tile,
                     -1);
}

spret_type cast_ring_of_flames(int power, bool fail)
{
    // You shouldn't be able to cast this in the rain. {due}
    if (in_what_cloud(CLOUD_RAIN))
    {
        mpr(gettext("Your spell sizzles in the rain."));
        return SPRET_ABORT;
    }

    fail_check();
    you.increase_duration(DUR_FIRE_SHIELD,
                          5 + (power / 10) + (random2(power) / 5), 50,
                          gettext("The air around you leaps into flame!"));
    manage_fire_shield(1);
    return SPRET_SUCCESS;
}

void manage_fire_shield(int delay)
{
    ASSERT(you.duration[DUR_FIRE_SHIELD]);

    int old_dur = you.duration[DUR_FIRE_SHIELD];

    you.duration[DUR_FIRE_SHIELD]-= delay;
    if (you.duration[DUR_FIRE_SHIELD] < 0)
        you.duration[DUR_FIRE_SHIELD] = 0;

    // Remove fire clouds on top of you
    if (env.cgrid(you.pos()) != EMPTY_CLOUD
        && env.cloud[env.cgrid(you.pos())].type == CLOUD_FIRE)
    {
        delete_cloud_at(you.pos());
    }

    if (!you.duration[DUR_FIRE_SHIELD])
    {
        mpr(gettext("Your ring of flames gutters out."), MSGCH_DURATION);
        return;
    }

    int threshold = get_expiration_threshold(DUR_FIRE_SHIELD);


    if (old_dur > threshold && you.duration[DUR_FIRE_SHIELD] < threshold)
        mpr(gettext("Your ring of flames is guttering out."), MSGCH_WARN);

    // Place fire clouds all around you
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
        if (!feat_is_solid(grd(*ai)) && env.cgrid(*ai) == EMPTY_CLOUD)
            place_cloud(CLOUD_FIRE, *ai, 1 + random2(6), &you);
}

spret_type cast_corpse_rot(bool fail)
{
    fail_check();
    corpse_rot(&you);
    return SPRET_SUCCESS;
}

void corpse_rot(actor* caster)
{
    for (radius_iterator ri(caster->pos(), 6, C_ROUND, caster->is_player() ? you.get_los_no_trans()
                                                                                    : caster->get_los());
         ri; ++ri)
    {
        if (!is_sanctuary(*ri) && env.cgrid(*ri) == EMPTY_CLOUD)
            for (stack_iterator si(*ri); si; ++si)
                if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY)
                {
                    // Found a corpse.  Skeletonise it if possible.
                    if (!mons_skeleton(si->mon_type))
                    {
                        item_was_destroyed(*si);
                        destroy_item(si->index());
                    }
                    else
                        turn_corpse_into_skeleton(*si);

                    place_cloud(CLOUD_MIASMA, *ri, 4+random2avg(16, 3),caster);

                    // Don't look for more corpses here.
                    break;
                }
    }

    if (you.can_smell() && you.can_see(caster))
        mpr(gettext("You smell decay."));

    // Should make zombies decay into skeletons?
}

int holy_flames(monster* caster, actor* defender)
{
    const coord_def pos = defender->pos();
    int cloud_count = 0;

    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        if (!in_bounds(*ai)
            || env.cgrid(*ai) != EMPTY_CLOUD
            || feat_is_solid(grd(*ai))
            || is_sanctuary(*ai)
            || monster_at(*ai))
        {
            continue;
        }

        place_cloud(CLOUD_HOLY_FLAMES, *ai, caster->hit_dice * 5, caster);

        cloud_count++;
    }

    return cloud_count;
}
