/**
 * @file
 * @brief Tracks monsters that are in suspended animation between levels.
**/

#include "AppHdr.h"

#include <algorithm>

#include "mon-transit.h"

#include "artefact.h"
#include "coord.h"
#include "coordit.h"
#include "dungeon.h"
#include "env.h"
#include "items.h"
#include "mon-place.h"
#include "mon-util.h"
#include "random.h"
#include "religion.h"
#include "travel.h"

#define MAX_LOST 100

monsters_in_transit the_lost_ones;
items_in_transit    transiting_items;

void transit_lists_clear()
{
    the_lost_ones.clear();
    transiting_items.clear();
}

static void level_place_lost_monsters(m_transit_list &m);
static void level_place_followers(m_transit_list &m);

static void cull_lost_mons(m_transit_list &mlist, int how_many)
{
    // First pass, drop non-uniques.
    for (m_transit_list::iterator i = mlist.begin(); i != mlist.end();)
    {
        m_transit_list::iterator finger = i++;
        if (!mons_is_unique(finger->mons.type))
        {
            mlist.erase(finger);

            if (--how_many <= MAX_LOST)
                return;
        }
    }

    // If we're still over the limit (unlikely), just lose
    // the old ones.
    while (how_many-- > MAX_LOST && !mlist.empty())
        mlist.erase(mlist.begin());
}

static void cull_lost_items(i_transit_list &ilist, int how_many)
{
    // First pass, drop non-artefacts.
    for (i_transit_list::iterator i = ilist.begin(); i != ilist.end();)
    {
        i_transit_list::iterator finger = i++;
        if (!is_artefact(*finger))
        {
            ilist.erase(finger);

            if (--how_many <= MAX_LOST)
                return;
        }
    }

    // Second pass, drop randarts.
    for (i_transit_list::iterator i = ilist.begin(); i != ilist.end();)
    {
        i_transit_list::iterator finger = i++;
        if (is_random_artefact(*finger))
        {
            ilist.erase(finger);

            if (--how_many <= MAX_LOST)
                return;
        }
    }

    // Third pass, drop unrandarts.
    for (i_transit_list::iterator i = ilist.begin(); i != ilist.end();)
    {
        i_transit_list::iterator finger = i++;
        if (is_unrandom_artefact(*finger)
            && !is_special_unrandom_artefact(*finger))
        {
            ilist.erase(finger);

            if (--how_many <= MAX_LOST)
                return;
        }
    }

    // If we're still over the limit (unlikely), just lose
    // the old ones.
    while (how_many-- > MAX_LOST && !ilist.empty())
        ilist.erase(ilist.begin());
}

m_transit_list *get_transit_list(const level_id &lid)
{
    monsters_in_transit::iterator i = the_lost_ones.find(lid);
    return (i != the_lost_ones.end()? &i->second : NULL);
}

void add_monster_to_transit(const level_id &lid, const monster& m)
{
    m_transit_list &mlist = the_lost_ones[lid];
    mlist.push_back(m);

    dprf("Monster in transit: %s", m.name(DESC_PLAIN).c_str());

    const int how_many = mlist.size();
    if (how_many > MAX_LOST)
        cull_lost_mons(mlist, how_many);
}

static void _place_lost_ones(void (*placefn)(m_transit_list &ml))
{
    level_id c = level_id::current();

    monsters_in_transit::iterator i = the_lost_ones.find(c);
    if (i == the_lost_ones.end())
        return;
    placefn(i->second);
    if (i->second.empty())
        the_lost_ones.erase(i);
}

void place_transiting_monsters()
{
    _place_lost_ones(level_place_lost_monsters);
}

void place_followers()
{
    _place_lost_ones(level_place_followers);
}

static bool place_lost_monster(follower &f)
{
    dprf("Placing lost one: %s", f.mons.name(DESC_PLAIN).c_str());
    return (f.place(false));
}

static void level_place_lost_monsters(m_transit_list &m)
{
    for (m_transit_list::iterator i = m.begin();
         i != m.end();)
    {
        m_transit_list::iterator mon = i++;

        // Monsters transiting to the Abyss have a 50% chance of being
        // placed, otherwise a 100% chance.
        if (you.level_type == LEVEL_ABYSS && coinflip())
            continue;

        if (place_lost_monster(*mon))
            m.erase(mon);
    }
}

static void level_place_followers(m_transit_list &m)
{
    for (m_transit_list::iterator i = m.begin(); i != m.end();)
    {
        m_transit_list::iterator mon = i++;
        if ((mon->mons.flags & MF_TAKING_STAIRS) && mon->place(true))
            m.erase(mon);
    }
}

void add_item_to_transit(const level_id &lid, const item_def &i)
{
    i_transit_list &ilist = transiting_items[lid];
    ilist.push_back(i);

    dprf("Item in transit: %s", i.name(false, DESC_PLAIN).c_str());

    const int how_many = ilist.size();
    if (how_many > MAX_LOST)
        cull_lost_items(ilist, how_many);
}

void place_transiting_items()
{
    level_id c = level_id::current();

    items_in_transit::iterator i = transiting_items.find(c);
    if (i == transiting_items.end())
        return;

    i_transit_list &ilist = i->second;
    i_transit_list keep;
    i_transit_list::iterator item;

    for (item = ilist.begin(); item != ilist.end(); ++item)
    {
        coord_def pos = item->pos;

        if (!in_bounds(pos))
            pos = random_in_bounds();

        const coord_def where_to_go =
            dgn_find_nearby_stair(DNGN_ESCAPE_HATCH_DOWN,
                                  pos, true);

        // List of items we couldn't place.
        if (!copy_item_to_grid(*item, where_to_go, 1, false, true))
            keep.push_back(*item);
    }

    // Only unplaceable items are kept in list.
    ilist = keep;
}

//////////////////////////////////////////////////////////////////////////
// follower

follower::follower(const monster& m) : mons(m), items()
{
    load_mons_items();
}

void follower::load_mons_items()
{
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
        if (mons.inv[i] != NON_ITEM)
            items[i] = mitm[ mons.inv[i] ];
        else
            items[i].clear();
}

bool follower::place(bool near_player)
{
    for (int i = 0; i < MAX_MONSTERS - 5; ++i)
    {
        // Find first empty slot in menv and copy monster into it.
        monster& m = menv[i];
        if (m.alive())
            continue;
        m = mons;

        bool placed = false;

        // In certain instances (currently, falling through a shaft)
        // try to place monster as close as possible to its previous
        // <x,y> coordinates.
        if (!near_player && you.level_type == LEVEL_DUNGEON
            && in_bounds(m.pos()))
        {
            const coord_def where_to_go =
                dgn_find_nearby_stair(DNGN_ESCAPE_HATCH_DOWN,
                                      m.pos(), true);

            if (where_to_go == you.pos())
                near_player = true;
            else if (m.find_home_near_place(where_to_go))
                placed = true;
        }

        if (!placed)
            placed = m.find_place_to_live(near_player);

        if (placed)
        {
            dprf("Placed follower: %s", m.name(DESC_PLAIN).c_str());
            m.target.reset();

            m.flags &= ~MF_TAKING_STAIRS;
            m.flags |= MF_JUST_SUMMONED;

            restore_mons_items(m);
            return (true);
        }

        m.reset();
        break;
    }

    return (false);
}

void follower::restore_mons_items(monster& m)
{
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
    {
        if (items[i].base_type == OBJ_UNASSIGNED)
            m.inv[i] = NON_ITEM;
        else
        {
            const int islot = get_mitm_slot(0);
            m.inv[i] = islot;
            if (islot == NON_ITEM)
                continue;

            item_def &it = mitm[islot];
            it = items[i];
            it.pos.set(-2, -2);
            it.link = NON_ITEM + 1 + m.mindex();
        }
    }
}

static bool _is_religious_follower(const monster* mon)
{
    return ((you.religion == GOD_YREDELEMNUL || you.religion == GOD_BEOGH)
            && is_follower(mon));
}

static bool _tag_follower_at(const coord_def &pos, bool &real_follower)
{
    if (!in_bounds(pos) || pos == you.pos())
        return (false);

    monster* fmenv = monster_at(pos);
    if (fmenv == NULL)
        return (false);

    if (!fmenv->alive()
        || fmenv->speed_increment < 50
        || fmenv->incapacitated()
        || mons_is_stationary(fmenv))
    {
        return (false);
    }

    if (!monster_habitable_grid(fmenv, DNGN_FLOOR))
        return (false);

    // Only non-wandering friendly monsters or those actively
    // seeking the player will follow up/down stairs.
    if (!fmenv->friendly()
          && (!mons_is_seeking(fmenv) || fmenv->foe != MHITYOU)
        || fmenv->foe == MHITNOT)
    {
        return (false);
    }

    // Monsters that are not directly adjacent are subject to more
    // stringent checks.
    if ((pos - you.pos()).abs() > 2)
    {
        if (!fmenv->friendly())
            return (false);

        // Undead will follow Yredelemnul worshippers, and orcs will
        // follow Beogh worshippers.
        if (!_is_religious_follower(fmenv))
            return (false);
    }

    // Monsters that can't use stairs can still be marked as followers
    // (though they'll be ignored for transit), so any adjacent real
    // follower can follow through. (jpeg)
    if (!mons_can_use_stairs(fmenv))
    {
        if (_is_religious_follower(fmenv))
        {
            fmenv->flags |= MF_TAKING_STAIRS;
            return (true);
        }
        return (false);
    }

    real_follower = true;

    // Monster is chasing player through stairs.
    fmenv->flags |= MF_TAKING_STAIRS;

    // Clear patrolling/travel markers.
    fmenv->patrol_point.reset();
    fmenv->travel_path.clear();
    fmenv->travel_target = MTRAV_NONE;

    fmenv->clear_clinging();

    dprf("%s is marked for following.",
         fmenv->name(DESC_THE, true).c_str());

    return (true);
}

static int follower_tag_radius2()
{
    // If only friendlies are adjacent, we set a max radius of 6, otherwise
    // only adjacent friendlies may follow.
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
    {
        if (const monster* mon = monster_at(*ai))
            if (!mon->friendly())
                return (2);
    }

    return (6 * 6);
}

void tag_followers()
{
    const int radius2 = follower_tag_radius2();
    int n_followers = 18;

    std::vector<coord_def> places[2];
    int place_set = 0;

    places[place_set].push_back(you.pos());
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));
    while (!places[place_set].empty())
    {
        for (int i = 0, size = places[place_set].size(); i < size; ++i)
        {
            const coord_def &p = places[place_set][i];
            for (adjacent_iterator ai(p); ai; ++ai)
            {
                if ((*ai - you.pos()).abs() > radius2
                    || travel_point_distance[ai->x][ai->y])
                {
                    continue;
                }
                travel_point_distance[ai->x][ai->y] = 1;

                bool real_follower = false;
                if (_tag_follower_at(*ai, real_follower))
                {
                    // If we've run out of our follower allowance, bail.
                    if (real_follower && --n_followers <= 0)
                        return;
                    places[!place_set].push_back(*ai);
                }
            }
        }
        places[place_set].clear();
        place_set = !place_set;
    }
}

void untag_followers()
{
    for (int m = 0; m < MAX_MONSTERS; ++m)
        menv[m].flags &= (~MF_TAKING_STAIRS);
}
