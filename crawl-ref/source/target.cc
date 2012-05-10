#include "AppHdr.h"

#include "target.h"

#include "beam.h"
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "itemprop.h"
#include "libutil.h"
#include "player.h"
#include "terrain.h"

#define notify_fail(x) (why_not = (x), false)

static std::string _wallmsg(coord_def c)
{
    ASSERT(map_bounds(c)); // there'd be an information leak
    const char *wall = feat_type_name(grd(c));
    return make_stringf(gettext("There is %s there."), article_a(wall).c_str());
}

bool targetter::set_aim(coord_def a)
{
    if (!valid_aim(a))
        return false;

    aim = a;
    return true;
}


targetter_view::targetter_view()
{
    origin = aim = you.pos();
}

bool targetter_view::valid_aim(coord_def a)
{
    return true; // don't reveal map bounds
}

aff_type targetter_view::is_affected(coord_def loc)
{
    if (loc == aim)
        return AFF_YES;

    return AFF_NO;
}


targetter_smite::targetter_smite(const actor* act, int ran,
                                 int exp_min, int exp_max, bool wall_ok,
                                 bool (*affects_pos_func)(const coord_def &)):
    exp_range_min(exp_min), exp_range_max(exp_max), affects_walls(wall_ok),
    affects_pos(affects_pos_func)
{
    ASSERT(act);
    ASSERT(exp_min >= 0);
    ASSERT(exp_max >= 0);
    ASSERT(exp_min <= exp_max);
    agent = act;
    origin = aim = act->pos();
    range2 = dist_range(ran);
}

bool targetter_smite::valid_aim(coord_def a)
{
    if (a != origin && !cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        // Scrying/glass/tree/grate.
        if (agent && agent->see_cell(a))
            return notify_fail(gettext("There's something in the way."));
        return notify_fail(gettext("You cannot see that place."));
    }
    if ((origin - a).abs() > range2)
        return notify_fail(gettext("Out of range."));
    if (!affects_walls && feat_is_solid(grd(a)))
        return notify_fail(_wallmsg(a));
    return true;
}

bool targetter_smite::set_aim(coord_def a)
{
    if (!targetter::set_aim(a))
        return false;

    if (exp_range_max > 0)
    {
        coord_def centre(9,9);
        bolt beam;
        beam.target = a;
        beam.use_target_as_pos = true;
        exp_map_min.init(INT_MAX);
        beam.determine_affected_cells(exp_map_min, coord_def(), 0,
                                      exp_range_min, true, true);
        exp_map_max.init(INT_MAX);
        beam.determine_affected_cells(exp_map_max, coord_def(), 0,
                                      exp_range_max, true, true);
    }
    return true;
}

aff_type targetter_smite::is_affected(coord_def loc)
{
    if (!valid_aim(aim))
        return AFF_NO;

    if (affects_pos && !affects_pos(loc))
        return AFF_NO;

    if (loc == aim)
        return AFF_YES;

    if (exp_range_max <= 0)
        return AFF_NO;

    if ((loc - aim).rdist() > 9)
        return AFF_NO;
    coord_def centre(9,9);
    if (exp_map_min(loc - aim + centre) < INT_MAX)
        return AFF_YES;
    if (exp_map_max(loc - aim + centre) < INT_MAX)
        return AFF_MAYBE;

    return AFF_NO;
}


targetter_reach::targetter_reach(const actor* act, reach_type ran) :
    range(ran)
{
    ASSERT(act);
    agent = act;
    origin = aim = act->pos();
}

bool targetter_reach::valid_aim(coord_def a)
{
    if (!cell_see_cell(origin, a, LOS_DEFAULT))
        return notify_fail(gettext("You cannot see that place."));
    if (!agent->see_cell_no_trans(a))
        return notify_fail(gettext("You can't get through."));

    int dist = (origin - a).abs();

    if (dist > reach_range(range))
        return notify_fail(gettext("You can't reach that far!"));

    return true;
}

aff_type targetter_reach::is_affected(coord_def loc)
{
    if (!valid_aim(loc))
        return AFF_NO;

    if (loc == aim)
        return AFF_YES;

    if (((loc - origin) * 2 - (aim - origin)).abs() <= 1
        && feat_is_reachable_past(grd(loc)))
    {
        return AFF_TRACER;
    }

    return AFF_NO;
}


targetter_cloud::targetter_cloud(const actor* act, int range,
                                 int count_min, int count_max) :
    cnt_min(count_min), cnt_max(count_max)
{
    ASSERT(cnt_min > 0);
    ASSERT(cnt_max > 0);
    ASSERT(cnt_min <= cnt_max);
    if (agent = act)
        origin = aim = act->pos();
    range2 = dist_range(range);
}

static bool _cloudable(coord_def loc)
{
    return in_bounds(loc)
           && !feat_is_solid(grd(loc))
           && env.cgrid(loc) == EMPTY_CLOUD;
}

bool targetter_cloud::valid_aim(coord_def a)
{
    if (agent && (origin - a).abs() > range2)
        return notify_fail(gettext("Out of range."));
    if (!map_bounds(a)
        || agent
           && origin != a
           && !cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        // Scrying/glass/tree/grate.
        if (agent && agent->see_cell(a))
            return notify_fail(gettext("There's something in the way."));
        return notify_fail(gettext("You cannot see that place."));
    }
    if (feat_is_solid(grd(a)))
        return notify_fail(_wallmsg(a));
    if (agent)
    {
        if (env.cgrid(a) != EMPTY_CLOUD)
            return notify_fail(gettext("There's already a cloud there."));
        ASSERT(_cloudable(a));
    }
    return true;
}

bool targetter_cloud::set_aim(coord_def a)
{
    if (!targetter::set_aim(a))
        return false;

    seen.clear();
    queue.clear();
    queue.push_back(std::vector<coord_def>());

    int placed = 0;
    queue[0].push_back(a);

    for (unsigned int d1 = 0; d1 < queue.size() && placed < cnt_max; d1++)
    {
        unsigned int to_place = queue[d1].size();
        placed += to_place;

        for (unsigned int i = 0; i < to_place; i++)
        {
            coord_def c = queue[d1][i];
            for (adjacent_iterator ai(c); ai; ++ai)
                if (_cloudable(*ai) && seen.find(*ai) == seen.end())
                {
                    unsigned int d2 = d1 + ((*ai - c).abs() == 1 ? 5 : 7);
                    if (d2 >= queue.size())
                        queue.resize(d2 + 1);
                    queue[d2].push_back(*ai);
                    seen[*ai] = AFF_TRACER;
                }

            seen[c] = placed <= cnt_min ? AFF_YES : AFF_MAYBE;
        }
    }

    return true;
}

aff_type targetter_cloud::is_affected(coord_def loc)
{
    if (!valid_aim(aim))
        return AFF_NO;

    std::map<coord_def, aff_type>::const_iterator it = seen.find(loc);
    if (it == seen.end() || it->second <= 0) // AFF_TRACER is used privately
        return AFF_NO;

    return it->second;
}

targetter_splash::targetter_splash(const actor* act)
{
    ASSERT(act);
    agent = act;
    origin = aim = act->pos();
}

bool targetter_splash::valid_aim(coord_def a)
{
    if (agent && grid_distance(origin, a) > 1)
        return notify_fail("Out of range.");
    return true;
}

bool targetter_splash::anyone_there(coord_def loc)
{
    if (!map_bounds(loc))
        return false;
    if (agent && agent->is_player())
        return env.map_knowledge(loc).monsterinfo();
    return actor_at(loc);
}

aff_type targetter_splash::is_affected(coord_def loc)
{
    if (!valid_aim(aim) || !valid_aim(loc))
        return AFF_NO;

    if (loc == aim)
        return AFF_YES;

    // self-spit currently doesn't splash
    if (aim == origin)
        return AFF_NO;

    // it splashes around only upon hitting someone
    if (!anyone_there(aim))
        return AFF_NO;

    if (grid_distance(loc, aim) > 1)
        return AFF_NO;

    // you're safe from being splashed by own spit
    if (loc == origin)
        return AFF_NO;

    return anyone_there(loc) ? AFF_YES : AFF_MAYBE;
}


targetter_los::targetter_los(const actor *act, los_type _los,
                             int range, int range_max)
{
    ASSERT(act);
    agent = act;
    origin = aim = act->pos();
    los = _los;
    range2 = range * range + 1;
    if (!range_max)
        range_max = range;
    ASSERT(range_max >= range);
    range_max2 = range_max * range_max + 1;
}

bool targetter_los::valid_aim(coord_def a)
{
    if ((a - origin).abs() > range_max2)
        return notify_fail("Out of range.");
    // If this message ever becomes used, please improve it.  I did not
    // bother adding complexity just for monsters and "hit allies" prompts
    // which don't need it.
    if (!is_affected(a))
        return notify_fail("The effect is blocked.");
    return true;
}

aff_type targetter_los::is_affected(coord_def loc)
{
    if (loc == aim)
        return AFF_YES;

    if ((loc - origin).abs() > range_max2)
        return AFF_NO;

    if (!cell_see_cell(loc, origin, los))
        return AFF_NO;

    return (loc - origin).abs() > range_max2 ? AFF_MAYBE : AFF_YES;
}
