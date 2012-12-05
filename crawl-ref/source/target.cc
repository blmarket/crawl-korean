#include "AppHdr.h"

#include "target.h"

#include "beam.h"
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "fight.h"
#include "godabil.h"
#include "itemprop.h"
#include "libutil.h"
#include "losglobal.h"
#include "spl-damage.h"
#include "terrain.h"

#define notify_fail(x) (why_not = (x), false)

static string _wallmsg(coord_def c)
{
    ASSERT(map_bounds(c)); // there'd be an information leak
    const char *wall = feat_type_name(grd(c));
    return make_stringf(gettext("There is %s there."), article_a(wall).c_str());
}

bool targetter::set_aim(coord_def a)
{
    // This matches a condition in direction_chooser::move_is_ok().
    if (agent && !cell_see_cell(agent->pos(), a, LOS_NO_TRANS))
        return false;

    aim = a;
    return true;
}

bool targetter::can_affect_outside_range()
{
    return false;
}

bool targetter::anyone_there(coord_def loc)
{
    if (!map_bounds(loc))
        return false;
    if (agent && agent->is_player())
        return env.map_knowledge(loc).monsterinfo();
    return actor_at(loc);
}

targetter_beam::targetter_beam(const actor *act, int range, zap_type zap,
                               int pow, bool stop,
                               int min_ex_rad, int max_ex_rad) :
                               min_expl_rad(min_ex_rad),
                               max_expl_rad(max_ex_rad)
{
    ASSERT(act);
    ASSERT(min_ex_rad >= 0);
    ASSERT(max_ex_rad >= 0);
    ASSERT(max_ex_rad >= min_ex_rad);
    agent = act;
    beam.set_agent(const_cast<actor *>(act));
    origin = aim = act->pos();
    beam.attitude = ATT_FRIENDLY;
    zappy(zap, pow, beam);
    beam.is_tracer = true;
    beam.is_targetting = true;
    beam.range = range;
    beam.source = origin;
    beam.target = aim;
    beam.dont_stop_player = true;
    beam.friend_info.dont_stop = true;
    beam.foe_info.dont_stop = true;
    beam.ex_size = min_ex_rad;
    beam.aimed_at_spot = true;

    penetrates_targets = !stop;
    range2 = dist_range(range);
}

bool targetter_beam::set_aim(coord_def a)
{
    if (!targetter::set_aim(a))
        return false;

    bolt tempbeam = beam;

    tempbeam.target = aim;
    tempbeam.path_taken.clear();
    tempbeam.fire();
    path_taken = tempbeam.path_taken;

    if (max_expl_rad > 0)
    {
        bolt tempbeam2 = beam;
        tempbeam2.target = origin;
        for (vector<coord_def>::const_iterator i = path_taken.begin();
             i != path_taken.end(); ++i)
        {
            if (cell_is_solid(*i)
                && tempbeam.affects_wall(grd(*i)) != B_TRUE)
                break;
            tempbeam2.target = *i;
            if (anyone_there(*i)
                && !fedhas_shoot_through(tempbeam, monster_at(*i)))
            {
                break;
            }
        }
        tempbeam2.use_target_as_pos = true;
        exp_map_min.init(INT_MAX);
        tempbeam2.determine_affected_cells(exp_map_min, coord_def(), 0,
                                           min_expl_rad, true, true);
        exp_map_max.init(INT_MAX);
        tempbeam2.determine_affected_cells(exp_map_max, coord_def(), 0,
                                           max_expl_rad, true, true);
    }
    return true;
}

bool targetter_beam::valid_aim(coord_def a)
{
    if (a != origin && !cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        if (agent->see_cell(a))
            return notify_fail("There's something in the way.");
        return notify_fail("You cannot see that place.");
    }
    if ((origin - a).abs() > range2)
        return notify_fail("Out of range.");
    return true;
}

bool targetter_beam::can_affect_outside_range()
{
    // XXX is this everything?
    return max_expl_rad > 0;
}

aff_type targetter_beam::is_affected(coord_def loc)
{
    bool on_path = false;
    coord_def c;
    aff_type current = AFF_YES;
    for (vector<coord_def>::const_iterator i = path_taken.begin();
         i != path_taken.end(); ++i)
    {
        if (cell_is_solid(*i)
            && beam.affects_wall(grd(*i)) != B_TRUE
            && max_expl_rad > 0)
            break;

        c = *i;
        if (c == loc)
        {
            if (max_expl_rad > 0)
                on_path = true;
            else if (cell_is_solid(*i))
            {
                maybe_bool res = beam.affects_wall(grd(*i));
                if (res == B_TRUE)
                    return current;
                else if (res == B_MAYBE)
                    return AFF_MAYBE;
                else
                    return AFF_NO;

            }
            else
                return current;
        }
        if (anyone_there(*i)
            && !fedhas_shoot_through(beam, monster_at(*i))
            && !penetrates_targets)
        {
            // We assume an exploding spell will always stop here.
            if (max_expl_rad > 0)
                break;
            current = AFF_MAYBE;
        }
    }
    if (max_expl_rad > 0 && (loc - c).rdist() <= 9)
    {
        maybe_bool aff_wall = beam.affects_wall(grd(loc));
        if (!feat_is_solid(grd(loc)) || aff_wall != B_FALSE)
        {
            coord_def centre(9,9);
            if (exp_map_min(loc - c + centre) < INT_MAX)
                return (!feat_is_solid(grd(loc)) || aff_wall == B_TRUE)
                       ? AFF_YES : AFF_MAYBE;
            if (exp_map_max(loc - c + centre) < INT_MAX)
                return AFF_MAYBE;
        }
    }
    return on_path ? AFF_TRACER : AFF_NO;
}

targetter_imb::targetter_imb(const actor *act, int pow, int range) :
               targetter_beam(act, range, ZAP_MYSTIC_BLAST, pow, true, 0, 0)
{
}

bool targetter_imb::set_aim(coord_def a)
{
    if (!targetter_beam::set_aim(a))
        return false;

    vector<coord_def> cur_path;

    splash.clear();
    splash2.clear();

    coord_def end = path_taken[path_taken.size() - 1];

    // IMB never splashes if you self-target.
    if (end == origin)
        return true;

    coord_def c;
    bool first = true;

    for (vector<coord_def>::iterator i = path_taken.begin();
         i != path_taken.end(); i++)
    {
        c = *i;
        cur_path.push_back(c);
        if (!(anyone_there(c)
              && !fedhas_shoot_through(beam, monster_at(c)))
            && c != end)
            continue;

        vector<coord_def> *which_splash = (first) ? &splash : &splash2;

        for (adjacent_iterator ai(c); ai; ++ai)
        {
            if (!imb_can_splash(origin, c, cur_path, *ai))
                continue;

            which_splash->push_back(*ai);
            if (!cell_is_solid(*ai)
                && !(anyone_there(*ai)
                     && !fedhas_shoot_through(beam, monster_at(*ai))))
            {
                which_splash->push_back(c + (*ai - c) * 2);
            }
        }

        first = false;
    }

    return true;
}

aff_type targetter_imb::is_affected(coord_def loc)
{
    aff_type from_path = targetter_beam::is_affected(loc);
    if (from_path != AFF_NO)
        return from_path;

    for (vector<coord_def>::const_iterator i = splash.begin();
         i != splash.end(); ++i)
    {
        if (*i == loc)
            return cell_is_solid(*i) ? AFF_NO : AFF_MAYBE;
    }
    for (vector<coord_def>::const_iterator i = splash2.begin();
         i != splash2.end(); ++i)
    {
        if (*i == loc)
            return cell_is_solid(*i) ? AFF_NO : AFF_TRACER;
    }
    return AFF_NO;
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

bool targetter_smite::can_affect_outside_range()
{
    // XXX is this everything?
    return exp_range_max > 0;
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

targetter_fragment::targetter_fragment(const actor* act, int power, int ran) :
    targetter_smite(act, ran, 1, 1, true, NULL),
    pow(power)
{
}

bool targetter_fragment::valid_aim(coord_def a)
{
    if (!targetter_smite::valid_aim(a))
        return false;

    bolt tempbeam;
    bool temp;
    if (!setup_fragmentation_beam(tempbeam, pow, agent, a, false,
                                  true, true, NULL, temp, temp))
    {
        return notify_fail("You cannot affect that.");
    }
    return true;
}

bool targetter_fragment::set_aim(coord_def a)
{
    if (!targetter::set_aim(a))
        return false;

    bolt tempbeam;
    bool temp;

    if (setup_fragmentation_beam(tempbeam, pow, agent, a, false,
                                 false, true, NULL, temp, temp))
    {
        exp_range_min = tempbeam.ex_size;
        setup_fragmentation_beam(tempbeam, pow, agent, a, false,
                                 true, true, NULL, temp, temp);
        exp_range_max = tempbeam.ex_size;
    }
    else
    {
        exp_range_min = exp_range_max = 0;
        return false;
    }

    coord_def centre(9,9);
    bolt beam;
    beam.target = a;
    beam.use_target_as_pos = true;
    exp_map_min.init(INT_MAX);
    beam.determine_affected_cells(exp_map_min, coord_def(), 0,
                                  exp_range_min, false, false);
    exp_map_max.init(INT_MAX);
    beam.determine_affected_cells(exp_map_max, coord_def(), 0,
                                  exp_range_max, false, false);

    return true;
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

    if (dist > range)
        return notify_fail(_("You can't reach that far!"));

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

targetter_cleave::targetter_cleave(const actor* act, coord_def target)
{
    ASSERT(act);
    agent = act;
    origin = act->pos();
    aim = target;
    list<actor*> act_targets;
    get_all_cleave_targets(act, target, act_targets);
    while (!act_targets.empty())
    {
        targets.insert(act_targets.front()->pos());
        act_targets.pop_front();
    }
}

aff_type targetter_cleave::is_affected(coord_def loc)
{
    return targets.count(loc) ? AFF_YES : AFF_NO;
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
    queue.push_back(vector<coord_def>());

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

bool targetter_cloud::can_affect_outside_range()
{
    return true;
}

aff_type targetter_cloud::is_affected(coord_def loc)
{
    if (!valid_aim(aim))
        return AFF_NO;

    map<coord_def, aff_type>::const_iterator it = seen.find(loc);
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


targetter_thunderbolt::targetter_thunderbolt(const actor *act, int r,
                                             coord_def _prev)
{
    ASSERT(act);
    agent = act;
    origin = act->pos();
    prev = _prev;
    aim = prev.origin() ? origin : prev;
    ASSERT(r > 1 && r <= you.current_vision);
    range2 = sqr(r) + 1;
}

bool targetter_thunderbolt::valid_aim(coord_def a)
{
    if (a != origin && !cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        // Scrying/glass/tree/grate.
        if (agent->see_cell(a))
            return notify_fail("There's something in the way.");
        return notify_fail("You cannot see that place.");
    }
    if ((origin - a).abs() > range2)
        return notify_fail("Out of range.");
    return true;
}

static void _make_ray(ray_def &ray, coord_def a, coord_def b)
{
    // Like beams, we need to allow picking the "better" ray if one is blocked
    // by a wall.
    if (!find_ray(a, b, ray, opc_solid_see))
        ray = ray_def(geom::ray(a.x + 0.5, a.y + 0.5, b.x + 0.5, b.y + 0.5));
}

static bool left_of(coord_def a, coord_def b)
{
    return a.x * b.y > a.y * b.x;
}

bool targetter_thunderbolt::set_aim(coord_def a)
{
    aim = a;
    zapped.clear();

    if (a == origin)
        return false;

    arc_length.init(0);

    ray_def ray;
    coord_def p; // ray.pos() does lots of processing, cache it

    // For consistency with beams, we need to
    _make_ray(ray, origin, aim);
    bool hit = true;
    while ((origin - (p = ray.pos())).abs() <= range2)
    {
        if (!map_bounds(p) || opc_solid_see(p) >= OPC_OPAQUE)
            hit = false;
        if (hit && p != origin && zapped[p] <= 0)
        {
            zapped[p] = AFF_YES;
            arc_length[origin.range(p)]++;
        }
        ray.advance();
    }

    if (prev.origin())
        return true;

    _make_ray(ray, origin, prev);
    hit = true;
    while ((origin - (p = ray.pos())).abs() <= range2)
    {
        if (!map_bounds(p) || opc_solid_see(p) >= OPC_OPAQUE)
            hit = false;
        if (hit && p != origin && zapped[p] <= 0)
        {
            zapped[p] = AFF_MAYBE; // fully affected, we just want to highlight cur
            arc_length[origin.range(p)]++;
        }
        ray.advance();
    }

    coord_def a1 = prev - origin;
    coord_def a2 = aim - origin;
    if (left_of(a2, a1))
        swapv(a1, a2);

    for (int x = -LOS_RADIUS; x <= LOS_RADIUS; ++x)
        for (int y = -LOS_RADIUS; y <= LOS_RADIUS; ++y)
        {
            if (sqr(x) + sqr(y) > range2)
                continue;
            coord_def r(x, y);
            if (left_of(a1, r) && left_of(r, a2))
            {
                (p = r) += origin;
                if (zapped.find(p) == zapped.end())
                    arc_length[r.range()]++;
                if (zapped[p] <= 0 && cell_see_cell(origin, p, LOS_NO_TRANS))
                    zapped[p] = AFF_MAYBE;
            }
        }

    zapped[origin] = AFF_NO;

    return true;
}

aff_type targetter_thunderbolt::is_affected(coord_def loc)
{
    if (loc == aim)
        return zapped[loc] ? AFF_YES : AFF_TRACER;

    if ((loc - origin).abs() > range2)
        return AFF_NO;

    return zapped[loc];
}
