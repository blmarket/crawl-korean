/**
 * @file
 * @brief Functions used when building new levels.
**/

#include "AppHdr.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>
#include <cmath>

#include "abyss.h"
#include "acquire.h"
#include "artefact.h"
#include "branch.h"
#include "chardump.h"
#include "coordit.h"
#include "defines.h"
#include "dgn-shoals.h"
#include "dgn-swamp.h"
#include "dgn-labyrinth.h"
#include "dgn-layouts.h"
#include "effects.h"
#include "env.h"
#include "enum.h"
#include "map_knowledge.h"
#include "flood_find.h"
#include "food.h"
#include "fprop.h"
#include "externs.h"
#include "dbg-maps.h"
#include "dbg-scan.h"
#include "directn.h"
#include "dungeon.h"
#include "files.h"
#include "ghost.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "l_defs.h"
#include "lev-pand.h"
#include "libutil.h"
#include "makeitem.h"
#include "mapdef.h"
#include "mapmark.h"
#include "maps.h"
#include "message.h"
#include "misc.h"
#include "mon-util.h"
#include "mon-place.h"
#include "mgen_data.h"
#include "mon-pathfind.h"
#include "notes.h"
#include "place.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "spl-book.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "sprint.h"
#include "state.h"
#include "tags.h"
#include "terrain.h"
#include "tiledef-dngn.h"
#include "tileview.h"
#include "traps.h"
#include "travel.h"
#include "tutorial.h"
#include "zotdef.h"
#include "hints.h"

#ifdef DEBUG_DIAGNOSTICS
#define DEBUG_TEMPLES
#endif

#ifdef WIZARD
#include "cio.h" // for cancelable_get_line()
#endif

#define YOU_DUNGEON_VAULTS_KEY    "you_dungeon_vaults_key"
#define YOU_PORTAL_VAULT_MAPS_KEY "you_portal_vault_maps_key"

// DUNGEON BUILDERS
static bool _build_level_vetoable(int level_number, level_area_type level_type,
                                  bool enable_random_maps,
                                  dungeon_feature_type dest_stairs_type);
static void _build_dungeon_level(int level_number, level_area_type level_type,
                                 dungeon_feature_type dest_stairs_type);
static bool _valid_dungeon_level(int level_number, level_area_type level_type);

static bool _builder_by_type(int level_number, level_area_type level_type);
static void _builder_normal(int level_number);
static void _builder_items(int level_number, int items_wanted);
static void _builder_monsters(int level_number, level_area_type level_type,
                              int mon_wanted);
static coord_def _place_specific_feature(dungeon_feature_type feat);
static void _place_specific_stair(dungeon_feature_type stair,
                                  const std::string &tag = "",
                                  int dl = 0);
static void _place_branch_entrances(int dlevel, level_area_type level_type);
static void _place_extra_vaults();
static void _place_chance_vaults();
static void _place_minivaults(void);
static int _place_uniques(int level_number, level_area_type level_type);
static void _place_traps(int level_number);
static void _prepare_water(int level_number);
static void _check_doors();
static void _hide_doors();

static void _add_plant_clumps(int frequency = 10, int clump_density = 12,
                              int clump_radius = 4);

static void _portal_vault_level(int level_number);

static void _pick_float_exits(vault_placement &place,
                              std::vector<coord_def> &targets);
static bool _connect_spotty(const coord_def& from);
static bool _connect_vault_exit(const coord_def& exit);

// ITEM & SHOP FUNCTIONS
static object_class_type _item_in_shop(shop_type shop_type);

// VAULT FUNCTIONS
static bool _build_secondary_vault(int level_number, const map_def *vault,
                                   bool clobber = false,
                                   bool make_no_exits = false,
                                   const coord_def &where = coord_def(-1, -1));

static bool _build_primary_vault(int level_number, const map_def *vault);

static void _build_postvault_level(vault_placement &place);
static bool _build_vault_impl(int level_number,
                              const map_def *vault,
                              bool build_only = false,
                              bool check_collisions = false,
                              bool make_no_exits = false,
                              const coord_def &where = coord_def(-1, -1));

static void _vault_grid(vault_placement &,
                        int vgrid,
                        const coord_def& where,
                        keyed_mapspec *mapsp);
static void _vault_grid_glyph(vault_placement &place, const coord_def& where,
                              int vgrid);
static void _vault_grid_mapspec(vault_placement &place, const coord_def& where,
                                keyed_mapspec& mapsp);

static const map_def *_dgn_random_map_for_place(bool minivault);
static void _dgn_load_colour_grid();
static void _dgn_map_colour_fixup();

static void _dgn_unregister_vault(const map_def &map);

// Returns true if the given square is okay for use by any character,
// but always false for squares in non-transparent vaults.
static bool _dgn_square_is_passable(const coord_def &c);

static coord_def _dgn_random_point_in_bounds(
    dungeon_feature_type searchfeat,
    uint32_t mapmask = MMT_VAULT,
    dungeon_feature_type adjacent = DNGN_UNSEEN,
    bool monster_free = false,
    int tries = 1500);

// ALTAR FUNCTIONS
static int                  _setup_temple_altars(CrawlHashTable &temple);
static dungeon_feature_type _pick_temple_altar(vault_placement &place);
static dungeon_feature_type _pick_an_altar();

static std::vector<god_type> _temple_altar_list;
static CrawlHashTable*       _current_temple_hash = NULL; // XXX: hack!

// MISC FUNCTIONS
static void _dgn_set_floor_colours();
static bool _fixup_interlevel_connectivity();
static void _slime_connectivity_fixup();

static void _dgn_postprocess_level();
static void _calc_density();

//////////////////////////////////////////////////////////////////////////
// Static data

// A mask of vaults and vault-specific flags.
std::vector<vault_placement> Temp_Vaults;
static FixedVector<bool, NUM_MONSTERS> temp_unique_creatures;
static FixedVector<unique_item_status_type, MAX_UNRANDARTS> temp_unique_items;

const map_mask *Vault_Placement_Mask = NULL;

bool Generating_Level = false;

static bool use_random_maps = true;
static bool dgn_check_connectivity = false;
static int  dgn_zones = 0;

static CrawlHashTable _you_vault_list;
static std::string    _portal_vault_map_name;

class dgn_veto_exception : public std::exception
{
public:
    dgn_veto_exception(const std::string& _msg) : msg(_msg) { }
    ~dgn_veto_exception() throw () { }
    const char *what() const throw ()
    {
        return msg.c_str();
    }
private:
    std::string msg;
};

struct coloured_feature
{
    dungeon_feature_type feature;
    int                  colour;

    coloured_feature() : feature(DNGN_UNSEEN), colour(BLACK) { }
    coloured_feature(dungeon_feature_type f, int c)
        : feature(f), colour(c)
    {
    }
};

struct dgn_colour_override_manager
{
    dgn_colour_override_manager()
    {
        _dgn_load_colour_grid();
    }

    ~dgn_colour_override_manager()
    {
        _dgn_map_colour_fixup();
    }
};

typedef FixedArray< coloured_feature, GXM, GYM > dungeon_colour_grid;
static std::auto_ptr<dungeon_colour_grid> dgn_colour_grid;

typedef std::map<std::string, std::string> callback_map;
static callback_map level_type_post_callbacks;

/**********************************************************************
 * builder() - kickoff for the dungeon generator.
 *********************************************************************/
bool builder(int level_number, level_area_type level_type,
             bool enable_random_maps, dungeon_feature_type dest_stairs_type)
{
    const std::set<std::string> uniq_tags  = you.uniq_map_tags;
    const std::set<std::string> uniq_names = you.uniq_map_names;

    // Save a copy of unique creatures for vetoes.
    temp_unique_creatures = you.unique_creatures;
    // And unrands
    temp_unique_items = you.unique_items;

    unwind_bool levelgen(Generating_Level, true);

    // N tries to build the level, after which we bail with a capital B.
    int tries = 50;
    while (tries-- > 0)
    {
        // If we're getting low on available retries, disable random vaults
        // and minivaults (special levels will still be placed).
        if (tries < 5)
            enable_random_maps = false;

        try
        {
            if (_build_level_vetoable(level_number, level_type,
                                      enable_random_maps, dest_stairs_type))
                return (true);
        }
        catch (map_load_exception &mload)
        {
            mprf(MSGCH_ERROR, "Failed to load map %s, reloading all maps",
                 mload.what());
            reread_maps();
        }

        you.uniq_map_tags  = uniq_tags;
        you.uniq_map_names = uniq_names;
    }

    if (!crawl_state.map_stat_gen)
    {
        // Failed to build level, bail out.
        if (crawl_state.need_save)
        {
            save_game(true,
                  make_stringf("Unable to generate level for '%s'!",
                               level_id::current().describe().c_str()).c_str());
        }
        else
        {
            die("Unable to generate level for '%s'!",
                level_id::current().describe().c_str());
        }
    }

    env.level_layout_types.clear();
    return (false);
}

static bool _build_level_vetoable(int level_number, level_area_type level_type,
                                  bool enable_random_maps,
                                  dungeon_feature_type dest_stairs_type)
{
#ifdef DEBUG_DIAGNOSTICS
    mapgen_report_map_build_start();
#endif

    dgn_reset_level(enable_random_maps);

    if (player_in_branch(BRANCH_ECUMENICAL_TEMPLE))
        _setup_temple_altars(you.props);

    try
    {
        _build_dungeon_level(level_number, level_type, dest_stairs_type);
    }
    catch (dgn_veto_exception& e)
    {
        dprf("VETO: %s: %s", level_id::current().describe().c_str(), e.what());
#ifdef DEBUG_DIAGNOSTICS
        mapgen_report_map_veto();
#endif
        return false;
    }

    _dgn_set_floor_colours();

    if (!crawl_state.game_standard_levelgen()
        || _valid_dungeon_level(level_number, level_type))
    {
#ifdef DEBUG_MONS_SCAN
        // If debug_mons_scan() finds a problem while Generating_Level is
        // still true then it will announce that a problem was caused
        // during level generation.
        debug_mons_scan();
#endif

        if (!env.level_build_method.empty()
            && env.level_build_method[0] == ' ')
        {
            env.level_build_method = env.level_build_method.substr(1);
        }

        std::string level_layout_type = comma_separated_line(
            env.level_layout_types.begin(),
            env.level_layout_types.end(), ", ");

        // Save information in the level's properties hash table
        // so we can inlcude it in crash reports.
        env.properties[BUILD_METHOD_KEY] = env.level_build_method;
        env.properties[LAYOUT_TYPE_KEY]  = level_layout_type;
        env.properties[LEVEL_ID_KEY]     = level_id::current().describe();

        // Save information in the player's properties has table so
        // we can include it in the character dump.
        if (!_you_vault_list.empty())
        {
            const std::string lev = level_id::current().describe();
            CrawlHashTable &all_vaults =
                you.props[YOU_DUNGEON_VAULTS_KEY].get_table();

            CrawlHashTable &this_level = all_vaults[lev].get_table();
            this_level = _you_vault_list;
        }
        else if (!_portal_vault_map_name.empty())
        {
            CrawlVector &vault_maps =
                you.props[YOU_PORTAL_VAULT_MAPS_KEY].get_vector();
            if (vault_maps.size() < vault_maps.get_max_size())
                vault_maps.push_back(_portal_vault_map_name);
        }

        if (you.level_type == LEVEL_PORTAL_VAULT)
        {
            CrawlVector &vault_names =
                you.props[YOU_PORTAL_VAULT_NAMES_KEY].get_vector();
            if (vault_names.size() < vault_names.get_max_size())
                vault_names.push_back(you.level_type_name);
        }

        _dgn_postprocess_level();

        env.level_layout_types.clear();
        env.level_uniq_maps.clear();
        env.level_uniq_map_tags.clear();
        _dgn_map_colour_fixup();

        // Discard any Lua chunks we loaded.
        strip_all_maps();

        return (true);
    }
    return (false);
}

// Things that are bugs where we want to assert rather than to sweep it under
// the rug with a veto.
static void _builder_assertions()
{
#ifdef ASSERTS
    for (rectangle_iterator ri(0); ri; ++ri)
        if (!in_bounds(*ri))
            if (!is_valid_border_feat(grd(*ri)))
            {
                die("invalid map border at (%d,%d): %s", ri->x, ri->y,
                    dungeon_feature_name(grd(*ri)));
            }
#endif
}

// Should be called after a level is constructed to perform any final
// fixups.
static void _dgn_postprocess_level()
{
    shoals_postprocess_level();
    _builder_assertions();
    _calc_density();
}

void level_welcome_messages()
{
    for (int i = 0, size = env.level_vaults.size(); i < size; ++i)
    {
        const std::vector<std::string> &msgs
            = env.level_vaults[i]->map.welcome_messages;
        for (int j = 0, msize = msgs.size(); j < msize; ++j)
            mpr(msgs[j].c_str());
    }
}

void dgn_clear_vault_placements(vault_placement_refv &vps)
{
    for (vault_placement_refv::const_iterator i = vps.begin();
         i != vps.end(); ++i)
    {
        delete *i;
    }
    vps.clear();
}

// Removes vaults that are not referenced in the map index mask from
// the level_vaults array.
void dgn_erase_unused_vault_placements()
{
    std::set<int> referenced_vault_indexes;
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        const int map_index = env.level_map_ids(*ri);
        if (map_index != INVALID_MAP_INDEX)
            referenced_vault_indexes.insert(map_index);
    }

    // Walk backwards and toss unused vaults.
    std::map<int, int> new_vault_index_map;
    const int nvaults = env.level_vaults.size();
    for (int i = nvaults - 1; i >= 0; --i)
    {
        if (referenced_vault_indexes.find(i) == referenced_vault_indexes.end())
        {
            vault_placement *vp = env.level_vaults[i];
            // Unreferenced vault, blow it away
            dprf("Removing references to unused map #%d) '%s' (%d,%d) (%d,%d)",
                 i, vp->map.name.c_str(), vp->pos.x, vp->pos.y,
                 vp->size.x, vp->size.y);

            if (!vp->seen)
            {
                dprf("Unregistering unseen vault: %s", vp->map.name.c_str());
                _dgn_unregister_vault(vp->map);
            }

            delete vp;
            env.level_vaults.erase(env.level_vaults.begin() + i);

            // Fix new indexes for all higher indexed vaults that are
            // still referenced.
            for (int j = i + 1; j < nvaults; ++j)
            {
                std::map<int, int>::iterator imap =
                    new_vault_index_map.find(j);
                if (imap != new_vault_index_map.end())
                    --imap->second;
            }
        }
        else
        {
            // Vault is still referenced, make a note of this index.
            new_vault_index_map[i] = i;
        }
    }

    // Finally, update the index map.
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        const int map_index = env.level_map_ids(*ri);
        if (map_index != INVALID_MAP_INDEX)
        {
            std::map<int, int>::iterator imap =
                new_vault_index_map.find(map_index);
            if (imap != new_vault_index_map.end())
                env.level_map_ids(*ri) = imap->second;
        }
    }

#ifdef DEBUG_DIAGNOSTICS
    dprf("Extant vaults on level: %d", (int) env.level_vaults.size());
    for (int i = 0, size = env.level_vaults.size(); i < size; ++i)
    {
        const vault_placement &vp(*env.level_vaults[i]);
        dprf("%d) %s (%d,%d) size (%d,%d)",
             i, vp.map.name.c_str(), vp.pos.x, vp.pos.y,
             vp.size.x, vp.size.y);
    }
#endif
}

void level_clear_vault_memory()
{
    dgn_clear_vault_placements(env.level_vaults);
    Temp_Vaults.clear();
    env.level_map_mask.init(0);
    env.level_map_ids.init(INVALID_MAP_INDEX);
}

void dgn_flush_map_memory()
{
    you.uniq_map_tags.clear();
    you.uniq_map_names.clear();
}

static void _dgn_load_colour_grid()
{
    dgn_colour_grid.reset(new dungeon_colour_grid);
    dungeon_colour_grid &dcgrid(*dgn_colour_grid);
    for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
            if (env.grid_colours[x][y] != BLACK)
            {
                dcgrid[x][y]
                    = coloured_feature(grd[x][y], env.grid_colours[x][y]);
            }
}

static void _dgn_map_colour_fixup()
{
    if (!dgn_colour_grid.get())
        return;

    // If the original coloured feature has been changed, reset the colour.
    const dungeon_colour_grid &dcgrid(*dgn_colour_grid);
    for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
            if (dcgrid[x][y].colour != BLACK
                && grd[x][y] != dcgrid[x][y].feature
                && (grd[x][y] != DNGN_UNDISCOVERED_TRAP
                    || dcgrid[x][y].feature != DNGN_FLOOR))
            {
                env.grid_colours[x][y] = BLACK;
            }

    dgn_colour_grid.reset(NULL);
}

bool set_level_flags(uint32_t flags, bool silent)
{
    bool could_control = allow_control_teleport(true);
    bool could_map     = is_map_persistent();

    uint32_t old_flags = env.level_flags;
    env.level_flags |= flags;

    bool can_control = allow_control_teleport(true);
    bool can_map     = is_map_persistent();

    if (could_control && !can_control && !silent)
    {
        mpr(gettext("You sense the appearance of a powerful magical force "
            "which warps space."), MSGCH_WARN);
    }

    if (could_map && !can_map && !silent)
    {
        mpr(gettext("A powerful force appears that prevents you from "
            "remembering where you've been."), MSGCH_WARN);
    }

    return (old_flags != env.level_flags);
}

bool unset_level_flags(uint32_t flags, bool silent)
{
    bool could_control = allow_control_teleport(true);
    bool could_map     = is_map_persistent();

    iflags_t old_flags = env.level_flags;
    env.level_flags &= ~flags;

    bool can_control = allow_control_teleport(true);
    bool can_map     = is_map_persistent();

    if (!could_control && can_control && !silent)
    {
        // Isn't really a "recovery", but I couldn't think of where
        // else to send it.
        mpr(gettext("You sense the disappearance of a powerful magical force "
            "which warped space."), MSGCH_RECOVERY);
    }

    if (!could_map && can_map && !silent)
    {
        // Isn't really a "recovery", but I couldn't think of where
        // else to send it.
        mpr(gettext("You sense the disappearance of the force that prevented you "
            "from remembering where you've been."), MSGCH_RECOVERY);
    }

    return (old_flags != env.level_flags);
}

void dgn_set_grid_colour_at(const coord_def &c, int colour)
{
    if (colour != BLACK)
    {
        env.grid_colours(c) = colour;
        if (!dgn_colour_grid.get())
            dgn_colour_grid.reset(new dungeon_colour_grid);

        (*dgn_colour_grid)(c) = coloured_feature(grd(c), colour);
    }
}

void dgn_register_vault(const map_def &map)
{
    if (!map.has_tag("allow_dup"))
        you.uniq_map_names.insert(map.name);

    if (map.has_tag("luniq"))
        env.level_uniq_maps.insert(map.name);

    std::vector<std::string> tags = split_string(" ", map.tags);
    for (int t = 0, ntags = tags.size(); t < ntags; ++t)
    {
        const std::string &tag = tags[t];
        if (tag.find("uniq_") == 0)
            you.uniq_map_tags.insert(tag);
        else if (tag.find("luniq_") == 0)
            env.level_uniq_map_tags.insert(tag);
    }
}

static void _dgn_unregister_vault(const map_def &map)
{
    you.uniq_map_names.erase(map.name);
    env.level_uniq_maps.erase(map.name);

    std::vector<std::string> tags = split_string(" ", map.tags);
    for (int t = 0, ntags = tags.size(); t < ntags; ++t)
    {
        const std::string &tag = tags[t];
        if (tag.find("uniq_") == 0)
            you.uniq_map_tags.erase(tag);
        else if (tag.find("luniq_") == 0)
            env.level_uniq_map_tags.erase(tag);
    }
}

bool dgn_square_travel_ok(const coord_def &c)
{
    const dungeon_feature_type feat = grd(c);
    return (feat_is_traversable(feat) || feat_is_trap(feat)
            || feat == DNGN_SECRET_DOOR);
}

static bool _dgn_square_is_passable(const coord_def &c)
{
    // [enne] Why does this function check MMT_OPAQUE?
    //
    // Don't peek inside MMT_OPAQUE vaults (all vaults are opaque by
    // default) because vaults may choose to create isolated regions,
    // or otherwise cause connectivity issues even if the map terrain
    // is travel-passable.
    return (!(env.level_map_mask(c) & MMT_OPAQUE) && dgn_square_travel_ok(c));
}

static inline void _dgn_point_record_stub(const coord_def &) { }

template <class point_record>
static bool _dgn_fill_zone(
    const coord_def &start, int zone,
    point_record &record_point,
    bool (*passable)(const coord_def &) = _dgn_square_is_passable,
    bool (*iswanted)(const coord_def &) = NULL)
{
    bool ret = false;
    std::list<coord_def> points[2];
    int cur = 0;

    // No bounds checks, assuming the level has at least one layer of
    // rock border.

    for (points[cur].push_back(start); !points[cur].empty();)
    {
        for (std::list<coord_def>::const_iterator i = points[cur].begin();
             i != points[cur].end(); ++i)
        {
            const coord_def &c(*i);

            travel_point_distance[c.x][c.y] = zone;

            if (iswanted && iswanted(c))
                ret = true;

            for (adjacent_iterator ai(c); ai; ++ai)
            {
                const coord_def& cp = *ai;
                if (!map_bounds(cp)
                    || travel_point_distance[cp.x][cp.y] || !passable(cp))
                {
                    continue;
                }

                travel_point_distance[cp.x][cp.y] = zone;
                record_point(cp);
                points[!cur].push_back(cp);
            }
        }

        points[cur].clear();
        cur = !cur;
    }
    return (ret);
}

static bool _is_perm_down_stair(const coord_def &c)
{
    switch (grd(c))
    {
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    case DNGN_EXIT_HELL:
    case DNGN_EXIT_PANDEMONIUM:
    case DNGN_TRANSIT_PANDEMONIUM:
    case DNGN_EXIT_ABYSS:
        return (true);
    default:
        return (false);
    }
}

static bool _is_upwards_exit_stair(const coord_def &c)
{
    // Is this a valid upwards or exit stair out of a branch? In general,
    // ensure that each region has a stone stair up.
    switch (grd(c))
    {
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_EXIT_HELL:
    case DNGN_RETURN_FROM_DWARVEN_HALL:
    case DNGN_RETURN_FROM_ORCISH_MINES:
    case DNGN_RETURN_FROM_HIVE:
    case DNGN_RETURN_FROM_LAIR:
    case DNGN_RETURN_FROM_SLIME_PITS:
    case DNGN_RETURN_FROM_VAULTS:
    case DNGN_RETURN_FROM_CRYPT:
    case DNGN_RETURN_FROM_HALL_OF_BLADES:
    case DNGN_RETURN_FROM_ZOT:
    case DNGN_RETURN_FROM_TEMPLE:
    case DNGN_RETURN_FROM_SNAKE_PIT:
    case DNGN_RETURN_FROM_ELVEN_HALLS:
    case DNGN_RETURN_FROM_TOMB:
    case DNGN_RETURN_FROM_SWAMP:
    case DNGN_RETURN_FROM_SHOALS:
    case DNGN_RETURN_FROM_SPIDER_NEST:
    case DNGN_RETURN_FROM_FOREST:
    case DNGN_EXIT_PANDEMONIUM:
    case DNGN_TRANSIT_PANDEMONIUM:
    case DNGN_EXIT_ABYSS:
        return (true);
    default:
        return (false);
    }
}

static bool _is_exit_stair(const coord_def &c)
{
    // Branch entries, portals, and abyss entries are not considered exit
    // stairs here, as they do not provide an exit (in a transitive sense) from
    // the current level.
    switch (grd(c))
    {
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    case DNGN_ESCAPE_HATCH_DOWN:
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_ESCAPE_HATCH_UP:
    case DNGN_EXIT_HELL:
    case DNGN_RETURN_FROM_DWARVEN_HALL:
    case DNGN_RETURN_FROM_ORCISH_MINES:
    case DNGN_RETURN_FROM_HIVE:
    case DNGN_RETURN_FROM_LAIR:
    case DNGN_RETURN_FROM_SLIME_PITS:
    case DNGN_RETURN_FROM_VAULTS:
    case DNGN_RETURN_FROM_CRYPT:
    case DNGN_RETURN_FROM_HALL_OF_BLADES:
    case DNGN_RETURN_FROM_ZOT:
    case DNGN_RETURN_FROM_TEMPLE:
    case DNGN_RETURN_FROM_SNAKE_PIT:
    case DNGN_RETURN_FROM_ELVEN_HALLS:
    case DNGN_RETURN_FROM_TOMB:
    case DNGN_RETURN_FROM_SWAMP:
    case DNGN_RETURN_FROM_SHOALS:
    case DNGN_RETURN_FROM_SPIDER_NEST:
    case DNGN_RETURN_FROM_FOREST:
    case DNGN_EXIT_PANDEMONIUM:
    case DNGN_TRANSIT_PANDEMONIUM:
    case DNGN_EXIT_ABYSS:
        return (true);
    default:
        return (false);
    }
}

// Counts the number of mutually unreachable areas in the map,
// excluding isolated zones within vaults (we assume the vault author
// knows what she's doing). This is an easy way to check whether a map
// has isolated parts of the level that were not formerly isolated.
//
// All squares within vaults are treated as non-reachable, to simplify
// life, because vaults may change the level layout and isolate
// different areas without changing the number of isolated areas.
// Here's a before and after example of such a vault that would cause
// problems if we considered floor in the vault as non-isolating (the
// vault is represented as V for walls and o for floor squares in the
// vault).
//
// Before:
//
//   xxxxx    xxxxx
//   x<..x    x.2.x
//   x.1.x    xxxxx  3 isolated zones
//   x>..x    x.3.x
//   xxxxx    xxxxx
//
// After:
//
//   xxxxx    xxxxx
//   x<1.x    x.2.x
//   VVVVVVVVVVoooV  3 isolated zones, but the isolated zones are different.
//   x>3.x    x...x
//   xxxxx    xxxxx
//
// If count_stairless is true, returns the number of regions that have no
// stairs in them.
//
// If fill is non-zero, it fills any disconnected regions with fill.
//
int process_disconnected_zones(int x1, int y1, int x2, int y2,
                               bool choose_stairless,
                               dungeon_feature_type fill)
{
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));
    int nzones = 0;
    int ngood = 0;
    for (int y = y1; y <= y2 ; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            if (!map_bounds(x, y)
                || travel_point_distance[x][y]
                || !_dgn_square_is_passable(coord_def(x, y)))
            {
                continue;
            }

            const bool found_exit_stair =
                _dgn_fill_zone(coord_def(x, y), ++nzones,
                               _dgn_point_record_stub,
                               dgn_square_travel_ok,
                               choose_stairless ? (at_branch_bottom() ?
                                                   _is_upwards_exit_stair :
                                                   _is_exit_stair) : NULL);

            // If we want only stairless zones, screen out zones that did
            // have stairs.
            if (choose_stairless && found_exit_stair)
                ++ngood;
            else if (fill)
            {
                for (int fy = y1; fy <= y2 ; ++fy)
                    for (int fx = x1; fx <= x2; ++fx)
                        if (travel_point_distance[fx][fy] == nzones
                            && !map_masked(coord_def(fx, fy), MMT_OPAQUE))
                        {
                            grd[fx][fy] = fill;
                        }
            }
        }
    }

    return (nzones - ngood);
}

int dgn_count_disconnected_zones(bool choose_stairless,
                                 dungeon_feature_type fill)
{
    return process_disconnected_zones(0, 0, GXM-1, GYM-1, choose_stairless,
                                      fill);
}

static void _fixup_hell_stairs()
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (grd(*ri) >= DNGN_STONE_STAIRS_UP_I
            && grd(*ri) <= DNGN_ESCAPE_HATCH_UP)
        {
            grd(*ri) = DNGN_ENTER_HELL;
        }
    }
}

static void _fixup_pandemonium_stairs()
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (grd(*ri) >= DNGN_STONE_STAIRS_UP_I
            && grd(*ri) <= DNGN_ESCAPE_HATCH_UP)
        {
            if (one_chance_in(30))
                grd(*ri) = DNGN_EXIT_PANDEMONIUM;
            else
                grd(*ri) = DNGN_FLOOR;
        }

        if (grd(*ri) >= DNGN_ENTER_LABYRINTH
            && grd(*ri) <= DNGN_ESCAPE_HATCH_DOWN)
        {
            grd(*ri) = DNGN_TRANSIT_PANDEMONIUM;
        }
    }
}

static void _mask_vault(const vault_placement &place, unsigned mask)
{
    for (vault_place_iterator vi(place); vi; ++vi)
        env.level_map_mask(*vi) |= mask;
}

static void _dgn_apply_map_index(const vault_placement &place, int map_index)
{
    for (vault_place_iterator vi(place); vi; ++vi)
        env.level_map_ids(*vi) = map_index;
}

void dgn_register_place(const vault_placement &place, bool register_vault)
{
    const int  map_index    = env.level_vaults.size();
    const bool overwritable = place.map.is_overwritable_layout();
    const bool transparent  = place.map.has_tag("transparent");

    if (register_vault)
    {
        dgn_register_vault(place.map);

        // Identify each square in the map with its map_index.
        if (!overwritable && !transparent)
            _dgn_apply_map_index(place, map_index);
    }

    if (!overwritable)
    {
        if (place.map.orient == MAP_ENCOMPASS)
        {
            for (rectangle_iterator ri(0); ri; ++ri)
                env.level_map_mask(*ri) |= MMT_VAULT | MMT_NO_DOOR;
        }
        else
            _mask_vault(place, MMT_VAULT | MMT_NO_DOOR);

        if (!transparent)
            _mask_vault(place, MMT_OPAQUE);
    }

    if (place.map.has_tag("no_monster_gen"))
        _mask_vault(place, MMT_NO_MONS);

    if (place.map.has_tag("no_item_gen"))
        _mask_vault(place, MMT_NO_ITEM);

    if (place.map.has_tag("no_pool_fixup"))
        _mask_vault(place, MMT_NO_POOL);

    if (place.map.has_tag("no_wall_fixup"))
        _mask_vault(place, MMT_NO_WALL);

    if (place.map.has_tag("no_trap_gen"))
        _mask_vault(place, MMT_NO_TRAP);

    // Now do per-square by-symbol masking.
    for (vault_place_iterator vi(place); vi; ++vi)
    {
        const keyed_mapspec *spec = place.map.mapspec_at(*vi - place.pos);

        if (spec != NULL)
        {
            env.level_map_mask(*vi) |= (short)spec->map_mask.flags_set;
            env.level_map_mask(*vi) &= ~((short)spec->map_mask.flags_unset);
        }
    }

    set_branch_flags(place.map.branch_flags.flags_set, true);
    unset_branch_flags(place.map.branch_flags.flags_unset, true);

    set_level_flags(place.map.level_flags.flags_set, true);
    unset_level_flags(place.map.level_flags.flags_unset, true);

    if (place.map.floor_colour != BLACK)
        env.floor_colour = place.map.floor_colour;

    if (place.map.rock_colour != BLACK)
        env.rock_colour = place.map.rock_colour;

    if (!place.map.rock_tile.empty())
    {
        tileidx_t rock;
        if (tile_dngn_index(place.map.rock_tile.c_str(), &rock))
        {
            env.tile_default.wall_idx =
                store_tilename_get_index(place.map.rock_tile);

            env.tile_default.wall = rock;
        }
    }

    if (!place.map.floor_tile.empty())
    {
        tileidx_t floor;
        if (tile_dngn_index(place.map.floor_tile.c_str(), &floor))
        {
            env.tile_default.floor_idx =
                store_tilename_get_index(place.map.floor_tile);

            env.tile_default.floor = floor;
        }
    }

    env.level_vaults.push_back(new vault_placement(place));
    if (register_vault)
    {
        remember_vault_placement(place.map.has_tag("extra")
                                 ? LEVEL_EXTRAS_KEY: LEVEL_VAULTS_KEY,
                                 place);
    }
}

bool dgn_ensure_vault_placed(bool vault_success,
                             bool disable_further_vaults)
{
    if (!vault_success)
        throw dgn_veto_exception("Vault placement failure.");
    else if (disable_further_vaults)
        use_random_maps = false;
    return (vault_success);
}

static bool _ensure_vault_placed_ex(bool vault_success, const map_def *vault)
{
    return dgn_ensure_vault_placed(vault_success,
                                    (!vault->has_tag("extra")
                                     && vault->orient == MAP_ENCOMPASS));
}

static coord_def _find_level_feature(int feat)
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (grd(*ri) == feat)
            return *ri;
    }

    return coord_def(0, 0);
}

static bool _has_connected_stone_stairs_from(const coord_def &c)
{
    flood_find<feature_grid, coord_predicate> ff(env.grid, in_bounds);
    ff.add_feat(DNGN_STONE_STAIRS_DOWN_I);
    ff.add_feat(DNGN_STONE_STAIRS_DOWN_II);
    ff.add_feat(DNGN_STONE_STAIRS_DOWN_III);
    ff.add_feat(DNGN_STONE_STAIRS_UP_I);
    ff.add_feat(DNGN_STONE_STAIRS_UP_II);
    ff.add_feat(DNGN_STONE_STAIRS_UP_III);

    coord_def where = ff.find_first_from(c, env.level_map_mask);
    return (where.x || !ff.did_leave_vault());
}

static bool _has_connected_downstairs_from(const coord_def &c)
{
    flood_find<feature_grid, coord_predicate> ff(env.grid, in_bounds);
    ff.add_feat(DNGN_STONE_STAIRS_DOWN_I);
    ff.add_feat(DNGN_STONE_STAIRS_DOWN_II);
    ff.add_feat(DNGN_STONE_STAIRS_DOWN_III);
    ff.add_feat(DNGN_ESCAPE_HATCH_DOWN);

    coord_def where = ff.find_first_from(c, env.level_map_mask);
    return (where.x || !ff.did_leave_vault());
}

static bool _is_level_stair_connected()
{
    coord_def up = _find_level_feature(DNGN_STONE_STAIRS_UP_I);
    if (up.x && up.y)
        return _has_connected_downstairs_from(up);

    return (false);
}

static bool _valid_dungeon_level(int level_number, level_area_type level_type)
{
    if (level_number == 0 && level_type == LEVEL_DUNGEON)
        return _is_level_stair_connected();

    return (true);
}

void dgn_reset_level(bool enable_random_maps)
{
    env.level_uniq_maps.clear();
    env.level_uniq_map_tags.clear();

    you.unique_creatures = temp_unique_creatures;
    you.unique_items = temp_unique_items;

    _portal_vault_map_name.clear();
    _you_vault_list.clear();
    env.level_build_method.clear();
    env.level_layout_types.clear();
    level_clear_vault_memory();
    dgn_colour_grid.reset(NULL);

    use_random_maps = enable_random_maps;
    dgn_check_connectivity = false;
    dgn_zones        = 0;

    _temple_altar_list.clear();
    _current_temple_hash = NULL;

    // Forget level properties.
    env.properties.clear();
    env.heightmap.reset(NULL);

    // Set up containers for storing some level generation info.
    env.properties[LEVEL_VAULTS_KEY].new_table();
    env.properties[LEVEL_EXTRAS_KEY].new_table();

    // Blank level with DNGN_ROCK_WALL.
    env.grid.init(DNGN_ROCK_WALL);
    env.pgrid.init(0);
    env.grid_colours.init(BLACK);
    env.map_knowledge.init(map_cell());

    // Delete all traps.
    for (int i = 0; i < MAX_TRAPS; i++)
        env.trap[i].type = TRAP_UNASSIGNED;

    // Initialise all items.
    for (int i = 0; i < MAX_ITEMS; i++)
        init_item(i);

    // Reset all monsters.
    env.mid_cache.clear();
    for (int i = 0; i < MAX_MONSTERS; i++)
        menv[i].reset();
    init_anon();

    // ... and Pan/regular spawn lists.
    env.mons_alloc.init(MONS_NO_MONSTER);
    setup_vault_mon_list();

    // Zap clouds
    env.cgrid.init(EMPTY_CLOUD);

    const cloud_struct empty;
    env.cloud.init(empty);
    env.cloud_no = 0;

    mgrd.init(NON_MONSTER);
    igrd.init(NON_ITEM);
    env.tgrid.init(NON_ENTITY);

    // Reset all shops.
    for (int shcount = 0; shcount < MAX_SHOPS; shcount++)
        env.shop[shcount].type = SHOP_UNASSIGNED;

    // Clear all markers.
    env.markers.clear();

    // Lose all listeners.
    dungeon_events.clear();

    // Set default level flags.
    if (player_in_level_area(LEVEL_DUNGEON))
        env.level_flags = branches[you.where_are_you].default_level_flags;
    else if (player_in_level_area(LEVEL_LABYRINTH)
             || player_in_level_area(LEVEL_ABYSS))
    {
        env.level_flags = LFLAG_NO_TELE_CONTROL | LFLAG_NO_MAP;
    }
    else
        env.level_flags = 0;

    // Set default random monster generation rate (smaller is more often,
    // except that 0 == no random monsters).
    if (you.level_type == LEVEL_DUNGEON)
    {
        if (you.where_are_you == BRANCH_ECUMENICAL_TEMPLE
            || crawl_state.game_is_tutorial())
        {
            // No random monsters in tutorial or ecu temple
            env.spawn_random_rate = 0;
        }
        else
            env.spawn_random_rate = 240;
    }
    else if (player_in_level_area(LEVEL_ABYSS)
             || player_in_level_area(LEVEL_PANDEMONIUM))
    {
        // Abyss spawn rate is set for those characters that start out in the
        // Abyss; otherwise the number is ignored in the Abyss.
        env.spawn_random_rate = 50;
    }
    else
        // No random monsters in Labyrinths and portal vaults.
        env.spawn_random_rate = 0;
    env.density = 0;

    env.floor_colour = BLACK;
    env.rock_colour  = BLACK;

    // Clear exclusions
    clear_excludes();

    // Clear custom tile settings from vaults
    tile_init_default_flavour();
    tile_clear_flavour();
    env.tile_names.clear();
}

static void _build_layout_skeleton(int level_number, level_area_type level_type)
{
    if (_builder_by_type(level_number, level_type))
        _builder_normal(level_number);

    if (player_in_branch(BRANCH_SLIME_PITS))
        _slime_connectivity_fixup();
}

static int _num_items_wanted(int level_number)
{
    if (player_in_branch(BRANCH_VESTIBULE_OF_HELL)
     || player_in_hell()
     || player_in_branch(BRANCH_SLIME_PITS)
     || player_in_branch(BRANCH_HALL_OF_BLADES)
     || player_in_branch(BRANCH_ECUMENICAL_TEMPLE))
    {
        // No random items in hell, the slime pits, the temple, the hall.
        return 0;
    }
    else if (level_number > 5 && one_chance_in(500 - 5 * level_number))
        return (10 + random2avg(90, 2)); // rich level!
    else
        return (3 + roll_dice(3, 11));
}

static int _num_mons_wanted(level_area_type level_type)
{
    if (level_type == LEVEL_ABYSS)
        return 0;

    if (level_type == LEVEL_PANDEMONIUM)
        return random2avg(28, 3);

    int mon_wanted = roll_dice(3, 10);

    if (player_in_hell())
        mon_wanted += roll_dice(3, 8);

    if (mon_wanted > 60)
        mon_wanted = 60;

    return mon_wanted;
}

static void _fixup_walls()
{
    // If level part of Dis -> all walls metal.
    // If part of vaults -> walls depend on level.
    // If part of crypt -> all walls stone.

    dungeon_feature_type wall_type = DNGN_ROCK_WALL;

    if (you.level_type != LEVEL_DUNGEON)
        return;

    switch (you.where_are_you)
    {
    case BRANCH_DIS:
        wall_type = DNGN_METAL_WALL;
        break;

    case BRANCH_VAULTS:
    {
        const int bdepth = player_branch_depth();

        if (bdepth > 6 && one_chance_in(10))
            wall_type = DNGN_GREEN_CRYSTAL_WALL;
        else if (bdepth > 4)
            wall_type = DNGN_METAL_WALL;
        else if (bdepth > 2)
            wall_type = DNGN_STONE_WALL;

        break;
    }

    case BRANCH_CRYPT:
        wall_type = DNGN_STONE_WALL;
        break;

    case BRANCH_SLIME_PITS:
        wall_type = DNGN_SLIMY_WALL;
        break;

    default:
        return;
    }

    dgn_replace_area(0, 0, GXM-1, GYM-1, DNGN_ROCK_WALL, wall_type,
                     MMT_NO_WALL);
}

// Remove any items that are on squares that items should not be on.
// link_items() must be called after this function.
static void _fixup_misplaced_items()
{
    for (int i = 0; i < MAX_ITEMS; i++)
    {
        item_def& item(mitm[i]);
        if (!item.defined() || (item.pos.x == 0)
            || item.held_by_monster())
        {
            continue;
        }

        if (in_bounds(item.pos))
        {
            dungeon_feature_type feat = grd(item.pos);
            if (feat >= DNGN_MINITEM)
                continue;

            mprf(MSGCH_ERROR, "Item buggily placed in feature at (%d, %d).",
                 item.pos.x, item.pos.y);
        }
        else
        {
            mprf(MSGCH_ERROR, "Item buggily placed out of bounds at (%d, %d).",
                 item.pos.x, item.pos.y);
        }

        // Can't just unlink item because it might not have been linked yet.
        item.base_type = OBJ_UNASSIGNED;
        item.quantity = 0;
        item.pos.reset();
    }
}

static void _fixup_branch_stairs()
{
    // Top level of branch levels - replaces up stairs with stairs back to
    // dungeon or wherever:
    if (your_branch().exit_stairs != NUM_FEATURES
        && player_branch_depth() == 1
        && you.level_type == LEVEL_DUNGEON)
    {
        const dungeon_feature_type exit = your_branch().exit_stairs;
        for (rectangle_iterator ri(1); ri; ++ri)
        {
            if (grd(*ri) >= DNGN_STONE_STAIRS_UP_I
                && grd(*ri) <= DNGN_ESCAPE_HATCH_UP)
            {
                if (grd(*ri) == DNGN_STONE_STAIRS_UP_I
                    && !feature_mimic_at(*ri))
                {
                    env.markers.add(new map_feature_marker(*ri, grd(*ri)));
                }

                grd(*ri) = exit;
            }
        }
    }

    // Bottom level of branch - wipes out down stairs and hatches
    dungeon_feature_type feat = DNGN_FLOOR;

    if (at_branch_bottom())
    {
        for (rectangle_iterator ri(1); ri; ++ri)
        {
            if (grd(*ri) >= DNGN_STONE_STAIRS_DOWN_I
                && grd(*ri) <= DNGN_ESCAPE_HATCH_DOWN)
            {
                grd(*ri) = feat;
            }
        }
    }
}

static bool _fixup_stone_stairs(bool preserve_vault_stairs)
{
    // This function ensures that there is exactly one each up and down
    // stone stairs I, II, and III.  More than three stairs will result in
    // turning additional stairs into escape hatches (with an attempt to keep
    // level connectivity).  Fewer than three stone stairs will result in
    // random placement of new stairs.

    const unsigned int max_stairs = 20;
    FixedVector<coord_def, max_stairs> up_stairs;
    FixedVector<coord_def, max_stairs> down_stairs;
    unsigned int num_up_stairs   = 0;
    unsigned int num_down_stairs = 0;

    for (rectangle_iterator ri(1); ri; ++ri)
    {
        const coord_def& c = *ri;
        if (feature_mimic_at(c))
            continue;

        if (grd(c) >= DNGN_STONE_STAIRS_DOWN_I
            && grd(c) <= DNGN_STONE_STAIRS_DOWN_III
            && num_down_stairs < max_stairs)
        {
            down_stairs[num_down_stairs++] = c;
        }
        else if (grd(c) >= DNGN_STONE_STAIRS_UP_I
                 && grd(c) <= DNGN_STONE_STAIRS_UP_III
                 && num_up_stairs < max_stairs)
        {
            up_stairs[num_up_stairs++] = c;
        }
    }

    bool success = true;

    for (unsigned int i = 0; i < 2; i++)
    {
        FixedVector<coord_def, max_stairs>& stair_list = (i == 0 ? up_stairs
                                                                 : down_stairs);

        unsigned int num_stairs, needed_stairs;
        dungeon_feature_type base;
        dungeon_feature_type replace;
        if (i == 0)
        {
            num_stairs = num_up_stairs;
            replace = DNGN_FLOOR;
            base = DNGN_STONE_STAIRS_UP_I;
            needed_stairs = 3;
        }
        else
        {
            num_stairs = num_down_stairs;
            replace = DNGN_FLOOR;
            base = DNGN_STONE_STAIRS_DOWN_I;

            if (at_branch_bottom())
                needed_stairs = 0;
            else
                needed_stairs = 3;
        }

        // In Zot, don't create extra escape hatches, in order to force
        // the player through vaults that use all three down stone stairs.
        if (player_in_branch(BRANCH_HALL_OF_ZOT))
            replace = DNGN_GRANITE_STATUE;

        if (num_stairs > needed_stairs)
        {
            // Find pairwise stairs that are connected and turn one of them
            // into an escape hatch of the appropriate type.
            for (unsigned int s1 = 0; s1 < num_stairs; s1++)
            {
                if (num_stairs <= needed_stairs)
                    break;

                for (unsigned int s2 = s1 + 1; s2 < num_stairs; s2++)
                {
                    if (num_stairs <= needed_stairs)
                        break;

                    if (preserve_vault_stairs
                        && map_masked(stair_list[s2], MMT_VAULT))
                    {
                        continue;
                    }

                    flood_find<feature_grid, coord_predicate> ff(env.grid,
                                                                 in_bounds);

                    ff.add_feat(grd(stair_list[s2]));

                    // Ensure we're not searching for the feature at s1.
                    dungeon_feature_type save = grd(stair_list[s1]);
                    grd(stair_list[s1]) = DNGN_FLOOR;

                    const coord_def where =
                        ff.find_first_from(stair_list[s1],
                                           env.level_map_mask);
                    if (where.x)
                    {
                        grd(stair_list[s2]) = replace;
                        num_stairs--;
                        stair_list[s2] = stair_list[num_stairs];
                        s2--;
                    }

                    grd(stair_list[s1]) = save;
                }
            }

            // If that doesn't work, remove random stairs.
            while (num_stairs > needed_stairs)
            {
                int remove = random2(num_stairs);
                if (preserve_vault_stairs)
                {
                    int start = remove;
                    do
                    {
                        if (!map_masked(stair_list[remove], MMT_VAULT))
                            break;
                        remove = (remove + 1) % num_stairs;
                    }
                    while (start != remove);

                    // If we looped through all possibilities, then it
                    // means that there are more than 3 stairs in vaults and
                    // we can't preserve vault stairs.
                    if (start == remove)
                        break;
                }
                grd(stair_list[remove]) = replace;

                stair_list[remove] = stair_list[--num_stairs];
            }
        }

        if (num_stairs > needed_stairs && preserve_vault_stairs)
        {
            success = false;
            continue;
        }

        // If there are no stairs, it's either a branch entrance or exit.
        // If we somehow have ended up in a catastrophic "no stairs" state,
        // the level will not be validated, so we do not need to catch it here.
        if (num_stairs == 0)
            continue;

        // Add extra stairs to get to exactly three.
        for (unsigned int s = num_stairs; s < needed_stairs; s++)
        {
            const uint32_t mask = preserve_vault_stairs ? MMT_VAULT : 0;
            coord_def gc = _dgn_random_point_in_bounds(DNGN_FLOOR, mask, DNGN_UNSEEN);

            if (!gc.origin())
            {
                dprf("Adding stair %d at (%d,%d)", s, gc.x, gc.y);
                // base gets fixed up to be the right stone stair below...
                grd(gc) = base;
                stair_list[num_stairs++] = gc;
            }
            else
                success = false;
        }

        // Ensure uniqueness of three stairs.
        for (int s = 0; s < 4; s++)
        {
            int s1 = s % num_stairs;
            int s2 = (s1 + 1) % num_stairs;
            ASSERT(grd(stair_list[s2]) >= base
                   && grd(stair_list[s2]) < base + 3);

            if (grd(stair_list[s1]) == grd(stair_list[s2]))
            {
                grd(stair_list[s2]) = (dungeon_feature_type)
                    (base + (grd(stair_list[s2])-base+1) % 3);
            }
        }
    }

    return success;
}

static bool _add_feat_if_missing(bool (*iswanted)(const coord_def &),
                                 dungeon_feature_type feat)
{
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));
    int nzones = 0;
    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
        {
            // [ds] Use dgn_square_is_passable instead of
            // dgn_square_travel_ok here, for we'll otherwise
            // fail on floorless isolated pocket in vaults (like the
            // altar surrounded by deep water), and trigger the assert
            // downstairs.
            const coord_def gc(x, y);
            if (!map_bounds(x, y)
                || travel_point_distance[x][y] // already covered previously
                || !_dgn_square_is_passable(gc))
            {
                continue;
            }

            if (_dgn_fill_zone(gc, ++nzones, _dgn_point_record_stub,
                               _dgn_square_is_passable, iswanted))
            {
                continue;
            }

            bool found_feature = false;
            for (rectangle_iterator ri(0); ri; ++ri)
            {
                if (grd(*ri) == feat
                    && travel_point_distance[ri->x][ri->y] == nzones)
                {
                    found_feature = true;
                    break;
                }
            }

            if (found_feature)
                continue;

            int i = 0;
            while (i++ < 2000)
            {
                coord_def rnd(random2(GXM), random2(GYM));
                if (grd(rnd) != DNGN_FLOOR)
                    continue;

                if (travel_point_distance[rnd.x][rnd.y] != nzones)
                    continue;

                grd(rnd) = feat;
                found_feature = true;
                break;
            }

            if (found_feature)
                continue;

            for (rectangle_iterator ri(0); ri; ++ri)
            {
                if (grd(*ri) != DNGN_FLOOR)
                    continue;

                if (travel_point_distance[ri->x][ri->y] != nzones)
                    continue;

                grd(*ri) = feat;
                found_feature = true;
                break;
            }

            if (found_feature)
                continue;

#ifdef DEBUG_DIAGNOSTICS
            dump_map("debug.map", true, true);
#endif
            // [ds] Too many normal cases trigger this ASSERT, including
            // rivers that surround a stair with deep water.
            // die("Couldn't find region.");
            return (false);
        }

    return (true);
}

static bool _add_connecting_escape_hatches()
{
    // For any regions without a down stone stair case, add an
    // escape hatch.  This will always allow (downward) progress.

    if (branches[you.where_are_you].branch_flags & BFLAG_ISLANDED)
        return (true);

    if (you.level_type != LEVEL_DUNGEON)
        return (true);

    if (at_branch_bottom())
        return (dgn_count_disconnected_zones(true) == 0);

    if (!_add_feat_if_missing(_is_perm_down_stair, DNGN_ESCAPE_HATCH_DOWN))
        return (false);

    // FIXME: shouldn't depend on branch.
    if (you.where_are_you != BRANCH_ORCISH_MINES)
        return (true);

    return (_add_feat_if_missing(_is_upwards_exit_stair, DNGN_ESCAPE_HATCH_UP));
}

static bool _branch_entrances_are_connected()
{
    // Returns true if all branch entrances on the level are connected to
    // stone stairs.
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (!feat_is_branch_stairs(grd(*ri)))
            continue;
        if (!_has_connected_stone_stairs_from(*ri))
            return (false);
    }

    return (true);
}

static void _dgn_verify_connectivity(unsigned nvaults)
{
    // After placing vaults, make sure parts of the level have not been
    // disconnected.
    if (dgn_zones && nvaults != env.level_vaults.size())
    {
        const int newzones = dgn_count_disconnected_zones(false);

#ifdef DEBUG_DIAGNOSTICS
        std::ostringstream vlist;
        for (unsigned i = nvaults; i < env.level_vaults.size(); ++i)
        {
            if (i > nvaults)
                vlist << ", ";
            vlist << env.level_vaults[i]->map.name;
        }
        mprf(MSGCH_DIAGNOSTICS, "Dungeon has %d zones after placing %s.",
             newzones, vlist.str().c_str());
#endif
        if (newzones > dgn_zones)
        {
            throw dgn_veto_exception(make_stringf(
                 "Had %d zones, now has %d%s%s.", dgn_zones, newzones,
#ifdef DEBUG_DIAGNOSTICS
                 "; broken by ", vlist.str().c_str()
#else
                 "", ""
#endif
            ));
        }
    }

    // Also check for isolated regions that have no stairs.
    if (you.level_type == LEVEL_DUNGEON
        && !(branches[you.where_are_you].branch_flags & BFLAG_ISLANDED)
        && dgn_count_disconnected_zones(true) > 0)
    {
        throw dgn_veto_exception("Isolated areas with no stairs.");
    }

    if (!_fixup_stone_stairs(true))
    {
        dprf("Warning: failed to preserve vault stairs.");
        if (!_fixup_stone_stairs(false))
        {
            throw dgn_veto_exception("Failed to fix stone stairs.");
        }
    }

    if (!_branch_entrances_are_connected())
        throw dgn_veto_exception("A disconnected branch entrance.");

    if (!_add_connecting_escape_hatches())
        throw dgn_veto_exception("Failed to add connecting escape hatches.");

    // XXX: Interlevel connectivity fixup relies on being the last
    //      point at which a level may be vetoed.
    if (!_fixup_interlevel_connectivity())
        throw dgn_veto_exception("Failed to ensure interlevel connectivity.");
}

static void _fixup_hatches_dest()
{
    for (rectangle_iterator ri(0); ri; ++ri)
        if (feat_is_escape_hatch(grd(*ri)))
            env.markers.add(new map_position_marker(*ri, random_in_bounds()));
}

// Structure of OVERFLOW_TEMPLES:
//
// * A vector, with one cell per dungeon level (unset if there's no
//   overflow temples on that level).
//
// * The cell of the previous vector is a vector, with one overflow
//   temple definition per cell.
//
// * The cell of the previous vector is a hash table, containing the
//   list of gods for the overflow temple and (optionally) the name of
//   the vault to use for the temple.  If no map name is supplied,
//   it will randomly pick from vaults tagged "temple_overflow_num",
//   where "num" is the number of gods in the temple.  Gods are listed
//   in the order their altars are placed.
static void _build_overflow_temples(int level_number)
{
    if (!you.props.exists(OVERFLOW_TEMPLES_KEY))
        // Levels built while in testing mode.
        return;

    CrawlVector &levels = you.props[OVERFLOW_TEMPLES_KEY].get_vector();

    // Are we deeper than the last overflow temple?
    if (level_number >= levels.size())
        return;

    CrawlStoreValue &val = levels[level_number];

    // Does this level have an overflow temple?
    if (val.get_flags() & SFLAG_UNSET)
        return;

    CrawlVector &temples = val.get_vector();

    if (temples.empty())
        return;

    for (unsigned int i = 0; i < temples.size(); i++)
    {
        CrawlHashTable &temple = temples[i].get_table();

        const int num_gods = _setup_temple_altars(temple);

        const map_def *vault = NULL;

        if (temple.exists(TEMPLE_MAP_KEY))
        {
            std::string name = temple[TEMPLE_MAP_KEY].get_string();

            vault = find_map_by_name(name);
            if (vault == NULL)
            {
                mprf(MSGCH_ERROR,
                     "Couldn't find overflow temple map '%s'!",
                     name.c_str());
            }
        }
        else
        {
            std::string vault_tag;

            // For a single-altar temple, first try to find a temple specialized
            // for that god.
            if (num_gods == 1 && coinflip())
            {
                CrawlVector &god_vec = temple[TEMPLE_GODS_KEY];
                god_type     god     = (god_type) god_vec[0].get_byte();

                std::string name = god_name(god);
                name = replace_all(name, " ", "_");
                lowercase(name);

                if (you.uniq_map_tags.find("uniq_altar_" + name)
                    != you.uniq_map_tags.end())
                {
                    // We've already placed a specialized temple for this
                    // god, so do nothing.
#ifdef DEBUG_TEMPLES
                    mprf(MSGCH_DIAGNOSTICS, "Already placed specialized "
                         "single-altar temple for %s", name.c_str());
#endif
                    continue;
                }

                vault_tag = make_stringf("temple_overflow_%s", name.c_str());

                vault = random_map_for_tag(vault_tag, true);
#ifdef DEBUG_TEMPLES
                if (vault == NULL)
                    mprf(MSGCH_DIAGNOSTICS, "Couldn't find overflow temple "
                         "for god %s", name.c_str());
#endif
            }

            if (vault == NULL)
            {
                vault_tag = make_stringf("temple_overflow_%d", num_gods);

                vault = random_map_for_tag(vault_tag, true);
                if (vault == NULL)
                {
                    mprf(MSGCH_ERROR,
                         "Couldn't find overflow temple tag '%s'!",
                         vault_tag.c_str());
                }
            }
        }

        if (vault == NULL)
            // Might as well build the rest of the level if we couldn't
            // find the overflow temple map, so don't veto the level.
            return;

        if (!dgn_ensure_vault_placed(
                _build_secondary_vault(level_number, vault),
                false))
        {
#ifdef DEBUG_TEMPLES
            mprf(MSGCH_DIAGNOSTICS, "Couldn't place overflow temple '%s', "
                 "vetoing level.", vault->name.c_str());
#endif
            return;
        }
#ifdef DEBUG_TEMPLES
        mprf(MSGCH_DIAGNOSTICS, "Placed overflow temple %s",
             vault->name.c_str());
#endif
    }
    _current_temple_hash = NULL; // XXX: hack!
}

template <typename Iterator>
static void _ruin_level(Iterator ri,
                        unsigned vault_mask = MMT_VAULT,
                        int ruination = 10,
                        int plant_density = 5)
{
    typedef std::pair<coord_def, dungeon_feature_type> coord_feat;
    typedef std::vector<coord_feat> coord_feats;
    coord_feats to_replace;

    for (; ri; ++ri)
    {
        // don't alter map boundary
        if (!in_bounds(*ri))
            continue;

        /* only try to replace wall and door tiles */
        if (!feat_is_wall(grd(*ri)) && !feat_is_door(grd(*ri)))
            continue;

        /* don't mess with permarock */
        if (grd(*ri) == DNGN_PERMAROCK_WALL)
            continue;

        /* or vaults */
        if (map_masked(*ri, vault_mask))
            continue;

        // Pick a random adjacent non-wall, non-door, non-statue
        // feature, and count the number of such features.
        dungeon_feature_type replacement = DNGN_FLOOR;
        int floor_count = 0;
        for (adjacent_iterator ai(*ri); ai; ++ai)
        {
            if (!feat_is_wall(grd(*ai)) && !feat_is_door(grd(*ai))
                && !feat_is_statue_or_idol(grd(*ai))
                // Shouldn't happen, but just in case.
                && grd(*ai) != DNGN_MALIGN_GATEWAY)
            {
                if (one_chance_in(++floor_count))
                    replacement = grd(*ai);
            }
        }

        /* chance of removing the tile is dependent on the number of adjacent
         * floor(ish) tiles */
        if (x_chance_in_y(floor_count, ruination))
        {
            to_replace.push_back(coord_feat(*ri, replacement));
        }
    }

    for (coord_feats::const_iterator it = to_replace.begin();
         it != to_replace.end();
         ++it)
    {
        const coord_def &p(it->first);
        dungeon_feature_type replacement = it->second;

        // Don't replace doors with impassable features.
        if (feat_is_door(grd(p)))
        {
            if (feat_is_water(replacement))
                replacement = DNGN_SHALLOW_WATER;
            else
                replacement = DNGN_FLOOR;
        }
        else if (feat_has_solid_floor(replacement)
                 && replacement != DNGN_SHALLOW_WATER)
        {
            // Exclude traps, shops, stairs, portals, altars, fountains.
            // The first four, especially, are a big deal.
            replacement = DNGN_FLOOR;
        }

        /* only remove some doors, to preserve tactical options */
        if (feat_is_wall(grd(p)) || coinflip() && feat_is_door(grd(p)))
            grd(p) = replacement;

        /* but remove doors if we've removed all adjacent walls */
        for (adjacent_iterator wai(p); wai; ++wai)
        {
            if (feat_is_door(grd(*wai)))
            {
                bool remove = true;
                for (adjacent_iterator dai(*wai); dai; ++dai)
                {
                    if (feat_is_wall(grd(*dai)))
                        remove = false;
                }
                // It's always safe to replace a door with floor.
                if (remove)
                    grd(*wai) = DNGN_FLOOR;
            }
        }

        /* replace some ruined walls with plants/fungi/bushes */
        if (plant_density && one_chance_in(plant_density)
            && feat_has_solid_floor(replacement))
        {
            mgen_data mg;
            mg.cls = one_chance_in(20) ? MONS_BUSH  :
                     coinflip()        ? MONS_PLANT :
                     MONS_FUNGUS;
            mg.pos = p;
            mons_place(mgen_data(mg));
        }
    }
}

//#define DEBUG_MIMIC
#ifdef DEBUG_MIMIC
// Missing stairs are replaced in fixup_branch_stairs, but replacing
// too many breaks interlevel connectivity, so we don't use a chance of 1.
  #define FEATURE_MIMIC_CHANCE 2
  #define ITEM_MIMIC_CHANCE    1
  #define FEATURE_MIMIC_DEPTH  1
  #define ITEM_MIMIC_DEPTH     1
#else
  #define FEATURE_MIMIC_CHANCE 100
  #define ITEM_MIMIC_CHANCE    500
  #define FEATURE_MIMIC_DEPTH   10
  #define ITEM_MIMIC_DEPTH       7
#endif
static void _place_feature_mimics(int level_number,
                                  dungeon_feature_type dest_stairs_type)
{
    if (player_in_branch(BRANCH_ECUMENICAL_TEMPLE)
        || player_in_branch(BRANCH_VESTIBULE_OF_HELL)
        || player_in_branch(BRANCH_SLIME_PITS))
    {
        return;
    }

    if (level_number < FEATURE_MIMIC_DEPTH)
        return;

    for (rectangle_iterator ri(1); ri; ++ri)
    {
        const coord_def pos = *ri;
        const dungeon_feature_type feat = grd(pos);

        // Vault tag prevents mimic.
        if (map_masked(pos, MMT_NO_MIMIC))
            continue;

        // Only features valid for mimicing.
        if (!is_valid_mimic_feat(feat))
            continue;

        // Don't mimic the stairs the player is going to be placed on.
        if (feat == dest_stairs_type)
            continue;

        // Don't mimic vetoed doors.
        if (door_vetoed(pos))
            continue;

        // Don't mimic staircases in vaults to avoid trapping the player or
        // breaking vault layouts.
        if (map_masked(pos, MMT_VAULT)
            && (feat_is_escape_hatch(feat) || feat_is_stone_stair(feat)))
        {
            continue;
        }

        // Dont mimic guaranteed portals.
        if (feat == DNGN_ENTER_ABYSS && you.absdepth0 == 24)
                continue;

        if (feat == DNGN_ENTER_PANDEMONIUM && you.absdepth0 == 23)
                continue;

        // If this is the real branch entry, don't mimic it.
        if (feat_is_branch_stairs(feat)
            && player_branch_depth() == branches[get_branch_at(pos)].startdepth)
        {
            continue;
        }

        // If it is a branch entry, it's been put there for mimicing.
        if (feat_is_branch_stairs(feat) || one_chance_in(FEATURE_MIMIC_CHANCE))
        {
            // For normal stairs, there is a chance to create another mimics
            // elsewhere instead of turning this one. That way, when the 3
            // stairs are grouped and there is another isolated one, any of
            // the 4 staircase can be the mimic.
            if (feat_is_stone_stair(feat) && one_chance_in(4))
            {
                const coord_def new_pos = _place_specific_feature(feat);
                dprf("Placed %s mimic at (%d,%d).",
                     feat_type_name(feat), new_pos.x, new_pos.y);
                env.level_map_mask(new_pos) |= MMT_MIMIC;
                continue;
            }

            dprf("Placed %s mimic at (%d,%d).",
                 feat_type_name(feat), ri->x, ri->y);
            env.level_map_mask(*ri) |= MMT_MIMIC;

            // If we're mimicing a unique portal vault, give a chance for
            // another one to spawn.
            std::string dst = env.markers.property_at(pos, MAT_ANY, "dstname");
            if (dst.empty())
                dst = env.markers.property_at(pos, MAT_ANY, "dst");
            if (!dst.empty())
            {
                const std::string tag = "uniq_" + dst;
                if (you.uniq_map_tags.find(tag) != you.uniq_map_tags.end())
                    you.uniq_map_tags.erase(tag);
            }
        }
    }
}

static void _place_item_mimics(int level_number)
{

    if (level_number < ITEM_MIMIC_DEPTH)
        return;

    for (int i = 0; i < MAX_ITEMS; i++)
    {
        item_def& item(mitm[i]);
        if (!item.defined() || !in_bounds(item.pos)
            || item.flags & ISFLAG_NO_MIMIC
            || !is_valid_mimic_item(item.base_type)
            || mimic_at(item.pos))
        {
            continue;
        }

        if (one_chance_in(ITEM_MIMIC_CHANCE))
            item.flags |= ISFLAG_MIMIC;
    }

}

static void _build_dungeon_level(int level_number, level_area_type level_type,
                                 dungeon_feature_type dest_stairs_type)
{
    // Generate a random monster table for Pan.
    if (level_type == LEVEL_PANDEMONIUM)
    {
        init_pandemonium();
        setup_vault_mon_list();
    }

    _build_layout_skeleton(level_number, level_type);

    if (you.level_type == LEVEL_LABYRINTH
        || you.level_type == LEVEL_PORTAL_VAULT)
    {
        return;
    }

    // Now place items, mons, gates, etc.
    // Stairs must exist by this point (except in Shoals where they are
    // yet to be placed). Some items and monsters already exist.

    _check_doors();

    if (!player_in_branch(BRANCH_DIS) && !player_in_branch(BRANCH_VAULTS))
        _hide_doors();

    if (player_in_branch(BRANCH_LAIR))
    {
        int depth = player_branch_depth() + 1;
        do
        {
            _ruin_level(rectangle_iterator(1), MMT_VAULT,
                        20 - depth, depth / 2 + 5);
            _add_plant_clumps(12 - depth, 18 - depth / 4, depth / 4 + 2);
            depth -= 3;
        } while (depth > 0);
    }

    const unsigned nvaults = env.level_vaults.size();

    // Any further vaults must make sure not to disrupt level layout.
    dgn_check_connectivity = !player_in_branch(BRANCH_SHOALS);

    if (player_in_branch(BRANCH_MAIN_DUNGEON)
        && !crawl_state.game_is_tutorial())
    {
        _build_overflow_temples(level_number);
    }

    // Try to place minivaults that really badly want to be placed. Still
    // no guarantees, seeing this is a minivault.
    if (crawl_state.game_standard_levelgen())
    {
        _place_chance_vaults();
        _place_minivaults();
        _place_branch_entrances(level_number, level_type);
        _place_extra_vaults();

        // XXX: Moved this here from builder_monsters so that
        //      connectivity can be ensured
        _place_uniques(level_number, level_type);

        _place_feature_mimics(level_number, dest_stairs_type);

        // Any vault-placement activity must happen before this check.
        _dgn_verify_connectivity(nvaults);

        _place_traps(level_number);

        // Place items.
        _builder_items(level_number, _num_items_wanted(level_number));

        // Place monsters.
        if (!crawl_state.game_is_zotdef())
            _builder_monsters(level_number, level_type,
                              _num_mons_wanted(level_type));

        _fixup_walls();
        _fixup_branch_stairs();
        _fixup_hatches_dest();
    }

    _fixup_misplaced_items();

    link_items();
    _place_item_mimics(level_number);

    if (!player_in_branch(BRANCH_COCYTUS)
        && !player_in_branch(BRANCH_SWAMP)
        && !player_in_branch(BRANCH_SHOALS))
    {
        _prepare_water(level_number);
    }

    // Translate stairs for pandemonium levels.
    if (level_type == LEVEL_PANDEMONIUM)
        _fixup_pandemonium_stairs();

    if (player_in_hell())
        _fixup_hell_stairs();
}

void dgn_set_colours_from_monsters()
{
    if (env.mons_alloc[9] >= 0 && env.mons_alloc[9] != MONS_NO_MONSTER
        && env.mons_alloc[9] < NUM_MONSTERS)
    {
        env.floor_colour = mons_class_colour(env.mons_alloc[9]);
    }
    // Don't use silence or halo colours.
    if (env.floor_colour == BLACK
        || env.floor_colour == CYAN
        || env.floor_colour == YELLOW)
    {
        env.floor_colour = LIGHTGREY;
    }

    if (env.mons_alloc[8] >= 0 && env.mons_alloc[8] != MONS_NO_MONSTER
        && env.mons_alloc[8] < NUM_MONSTERS)
    {
        env.rock_colour = mons_class_colour(env.mons_alloc[8]);
    }
    if (env.rock_colour == BLACK || env.rock_colour == LIGHTGREY)
        env.rock_colour = coinflip() ? BROWN : LIGHTRED;
}

static void _dgn_set_floor_colours()
{
    uint8_t old_floor_colour = env.floor_colour;
    uint8_t old_rock_colour  = env.rock_colour;

    if (you.level_type == LEVEL_PANDEMONIUM || you.level_type == LEVEL_ABYSS)
        dgn_set_colours_from_monsters();
    else if (you.level_type == LEVEL_DUNGEON)
    {
        // level_type == LEVEL_DUNGEON
        // Hall of Zot colours handled in dat/zot.des
        const int youbranch = you.where_are_you;
        env.floor_colour    = branches[youbranch].floor_colour;
        env.rock_colour     = branches[youbranch].rock_colour;
    }

    if (old_floor_colour != BLACK)
        env.floor_colour = old_floor_colour;
    if (old_rock_colour != BLACK)
        env.rock_colour = old_rock_colour;

    if (env.floor_colour == BLACK)
        env.floor_colour = LIGHTGREY;
    if (env.rock_colour == BLACK)
        env.rock_colour  = BROWN;
}

static void _check_doors()
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (!feat_is_closed_door(grd(*ri)))
            continue;

        int solid_count = 0;

        for (orth_adjacent_iterator rai(*ri); rai; ++rai)
            if (feat_is_solid(grd(*rai)))
                solid_count++;

        grd(*ri) = (solid_count < 2 ? DNGN_FLOOR : DNGN_CLOSED_DOOR);
    }
}

static void _hide_doors()
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        // Only one out of four doors are candidates for hiding. {gdl}
        if (grd(*ri) == DNGN_CLOSED_DOOR && one_chance_in(4)
            && !map_masked(*ri, MMT_NO_DOOR))
        {
            int wall_count = 0;

            for (orth_adjacent_iterator rai(*ri); rai; ++rai)
                if (grd(*rai) == DNGN_ROCK_WALL)
                    wall_count++;

            // If door is attached to more than one wall, hide it. {dlb}
            if (wall_count > 1)
                grd(*ri) = DNGN_SECRET_DOOR;
        }
    }
}

int count_feature_in_box(int x0, int y0, int x1, int y1,
                         dungeon_feature_type feat)
{
    int result = 0;
    for (int i = x0; i < x1; ++i)
        for (int j = y0; j < y1; ++j)
        {
            if (grd[i][j] == feat)
                ++result;
        }

    return result;
}

// Count how many neighbours of grd[x][y] are the feature feat.
int count_neighbours(int x, int y, dungeon_feature_type feat)
{
    return count_feature_in_box(x-1, y-1, x+2, y+2, feat);
}

// Gives water which is next to ground/shallow water a chance of being
// shallow. Checks each water space.
static void _prepare_water(int level_number)
{
    dungeon_feature_type which_grid;   // code compaction {dlb}

    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (map_masked(*ri, MMT_NO_POOL))
            continue;

        if (grd(*ri) == DNGN_DEEP_WATER)
        {
            for (adjacent_iterator ai(*ri); ai; ++ai)
            {
                which_grid = grd(*ai);

                // must come first {dlb}
                if (which_grid == DNGN_SHALLOW_WATER
                    && one_chance_in(8 + level_number))
                {
                    grd(*ri) = DNGN_SHALLOW_WATER;
                }
                else if (which_grid >= DNGN_FLOOR
                         && x_chance_in_y(80 - level_number * 4,
                                          100))
                {
                    grd(*ri) = DNGN_SHALLOW_WATER;
                }
            }
        }
    }
}

static void _pan_level(int level_number)
{
    const char *pandemon_level_names[] =
        { "mnoleg", "lom_lobon", "cerebov", "gloorx_vloq", };
    int which_demon = -1;
    PlaceInfo &place_info = you.get_place_info();
    bool all_demons_generated = true;

    for (int i = 0; i < 4; i++)
    {
        if (!you.uniq_map_tags.count(std::string("uniq_") + pandemon_level_names[i]))
        {
            all_demons_generated = false;
            break;
        }
    }

    // Unique pan lords become more common as you travel through pandemonium.
    // On average it takes 27 levels to see all four, and you're likely to see
    // your first one after about 10 levels.
    if (x_chance_in_y(1 + place_info.levels_seen, 65 + place_info.levels_seen * 2)
        && !all_demons_generated)
    {
        do
        {
            which_demon = random2(4);
        }
        while (you.uniq_map_tags.count(std::string("uniq_")
                                     + pandemon_level_names[which_demon]));
    }

    if (which_demon >= 0)
    {
        const map_def *vault =
            random_map_for_tag(pandemon_level_names[which_demon], false);

        ASSERT(vault);

        dgn_ensure_vault_placed(_build_primary_vault(level_number, vault),
                                 true);
    }
    else
    {
        const map_def *vault = random_map_for_tag("pan", true);
        ASSERT(vault);

        if (vault->orient == MAP_ENCOMPASS)
        {
            dgn_ensure_vault_placed(_build_primary_vault(level_number, vault),
                    true);
        }
        else
        {
            const map_def *layout = random_map_for_tag("layout", true, true);

            dgn_ensure_vault_placed(_build_primary_vault(level_number, layout),
                    true);

            _build_secondary_vault(level_number, vault);
        }
    }
}

// Take care of labyrinth, abyss, pandemonium. Returns false if we should skip
// further generation, and true otherwise.
static bool _builder_by_type(int level_number, level_area_type level_type)
{
    if (level_type == LEVEL_PORTAL_VAULT)
        _portal_vault_level(level_number);
    else if (level_type == LEVEL_LABYRINTH)
        dgn_build_labyrinth_level(level_number);
    else if (level_type == LEVEL_ABYSS)
        generate_abyss();
    else if (level_type == LEVEL_PANDEMONIUM)
        _pan_level(level_number);
    else
        return true;

    return false;
}

static void _portal_vault_level(int level_number)
{
    env.level_build_method += " portal_vault_level";
    env.level_layout_types.insert("portal vault");

    // level_type_tag may contain spaces for human readability, but the
    // corresponding vault tag name cannot use spaces, so force spaces to
    // _ when searching for the tag.
    const std::string trimmed_name =
        replace_all(trimmed_string(you.level_type_tag), " ", "_");

    ASSERT(!trimmed_name.empty());

    const char* level_name = trimmed_name.c_str();

    const map_def *vault = random_map_for_place(level_id::current(), false);

#ifdef WIZARD
    if (!vault && you.wizard && map_count_for_tag(level_name, false) > 1)
    {
        char buf[80];

        do
        {
            mprf(MSGCH_PROMPT, "Which %s (ESC or ENTER for random): ",
                 level_name);
            if (cancelable_get_line(buf, sizeof buf))
                break;

            std::string name = buf;
            trim_string(name);

            if (name.empty())
                break;

            lowercase(name);
            name = replace_all(name, " ", "_");

            // Allow finding e.g. "sewer_kobolds" with just "kobolds".
            vault = find_map_by_name(you.level_type_tag + "_" + name);

            // Allow finding the map with the full name.
            if (!vault && find_map_by_name(name))
                if (find_map_by_name(name)->has_tag(you.level_type_tag))
                    vault = find_map_by_name(name);

            if (!vault)
                mprf(MSGCH_DIAGNOSTICS, "No such %s, try again.",
                     level_name);
        }
        while (!vault);
    }
#endif

    if (!vault)
        vault = random_map_for_tag(level_name, false);

    if (!vault)
        vault = find_map_by_name("broken_portal");

    if (vault)
    {
        // XXX: This is pretty hackish, I confess.
        if (vault->border_fill_type != DNGN_ROCK_WALL)
            dgn_replace_area(0, 0, GXM-1, GYM-1, DNGN_ROCK_WALL,
                             vault->border_fill_type);

        dgn_ensure_vault_placed(_build_primary_vault(level_number, vault),
                                 true);
    }
    else
        die("No maps or tags named '%s', and no backup either.", level_name);

    link_items();

    // TODO: Let portal vault map have arbitrary properties which can
    // be passed onto the callback.
    callback_map::const_iterator
        i = level_type_post_callbacks.find(you.level_type_tag);

    if (i != level_type_post_callbacks.end())
        dlua.callfn(i->second.c_str(), 0, 0);
}

static const map_def *_dgn_random_map_for_place(bool minivault)
{
    if (!minivault && player_in_branch(BRANCH_ECUMENICAL_TEMPLE))
    {
        // Temple vault determined at new game time.
        const std::string name = you.props[TEMPLE_MAP_KEY];

        // Tolerate this for a little while, for old games.
        if (!name.empty())
        {
            const map_def *vault = find_map_by_name(name);

            if (vault)
                return (vault);

            mprf(MSGCH_ERROR, "Unable to find Temple vault '%s'",
                 name.c_str());

            // Fall through and use a different Temple map instead.
        }
    }

    const level_id lid = level_id::current();

    const map_def *vault = random_map_for_place(lid, minivault);

    if (!vault
        && lid.branch == BRANCH_MAIN_DUNGEON
        && lid.depth == 1)
    {
        if (crawl_state.game_is_sprint()
            || crawl_state.game_is_zotdef()
            || crawl_state.game_is_tutorial())
        {
            vault = find_map_by_name(crawl_state.map);
            if (vault == NULL)
            {
                end(1, false, "Couldn't find selected map '%s'.",
                    crawl_state.map.c_str());
            }
        }
        else
            vault = random_map_for_tag("entry");
    }

    return (vault);
}

static int _setup_temple_altars(CrawlHashTable &temple)
{
    _current_temple_hash = &temple; // XXX: hack!

    CrawlVector god_list = temple[TEMPLE_GODS_KEY].get_vector();

    _temple_altar_list.clear();

    for (unsigned int i = 0; i < god_list.size(); i++)
        _temple_altar_list.push_back((god_type) god_list[i].get_byte());

    return ((int) god_list.size());
}

struct map_component
{
    int label;


    map_component()
    {
        min_equivalent = NULL;
    }
    map_component * min_equivalent;


    coord_def min_coord;
    coord_def max_coord;

    coord_def seed_position;

    void start_component(const coord_def & pos, int in_label)
    {
        seed_position = pos;
        min_coord = pos;
        max_coord = pos;

        label = in_label;
        min_equivalent = NULL;
    }

    void add_coord(const coord_def & pos)
    {
        if (pos.x < min_coord.x)
            min_coord.x = pos.x;
        if (pos.x > max_coord.x)
            max_coord.x = pos.x;
        if (pos.y < min_coord.y)
            min_coord.y = pos.y;
        if (pos.y > max_coord.y)
            max_coord.y = pos.y;
    }

    void merge_region(const map_component & existing)
    {
        add_coord(existing.min_coord);
        add_coord(existing.max_coord);
    }
};

static int _min_transitive_label(map_component & component)
{
    map_component * current = &component;

    int label;
    do
    {
        label = current->label;

        current = current->min_equivalent;
    } while (current);

    return label;
}

// 8-way connected component analysis on the current level map.
template<typename comp>
static void _ccomps_8(FixedArray<int, GXM, GYM > & connectivity_map,
                      std::vector<map_component> & components,
                      comp & connected)
{
    std::map<int, map_component> intermediate_components;

    connectivity_map.init(0);
    components.clear();

    unsigned adjacent_size = 4;
    coord_def offsets[4] = {coord_def(-1, 0), coord_def(-1, -1), coord_def(0, -1), coord_def(1, -1)};

    int next_label = 1;


    // Pass 1, for each point, check the upper/left adjacent squares for labels
    // if a labels are found, use the min connected label, else assign a new
    // label and start a new component
    for (rectangle_iterator pos(1); pos; ++pos)
    {
        if (connected(*pos))
        {
            int absolute_min_label = INT_MAX;
            std::set<int> neighbor_labels;
            for (unsigned i = 0; i < adjacent_size; i++)
            {
                coord_def test = *pos + offsets[i];
                if (in_bounds(test) && connectivity_map(test) != 0)
                {
                    int neighbor_label = connectivity_map(test);
                    if (neighbor_labels.insert(neighbor_label).second)
                    {
                        int trans = _min_transitive_label(intermediate_components[neighbor_label]);

                        if (trans < absolute_min_label)
                            absolute_min_label = trans;
                    }
                }
            }

            int label;
            if (neighbor_labels.empty())
            {
                intermediate_components[next_label].start_component(*pos, next_label);
                label = next_label;
                next_label++;
            }
            else
            {
                label = absolute_min_label;
                map_component * absolute_min = &intermediate_components[absolute_min_label];

                absolute_min->add_coord(*pos);
                for (std::set<int>::iterator i = neighbor_labels.begin();
                     i != neighbor_labels.end();i++)
                {
                    map_component * current = &intermediate_components[*i];

                    while (current && current != absolute_min)
                    {
                        absolute_min->merge_region(*current);
                        map_component * next = current->min_equivalent;
                        current->min_equivalent = absolute_min;
                        current = next;
                    }
                }
            }
            connectivity_map(*pos) = label;
        }
    }

    int reindexed_label = 1;
    // Reindex root labels, and move them to output
    for (std::map<int, map_component>::iterator i = intermediate_components.begin();
         i != intermediate_components.end(); ++i)
    {
        if (i->second.min_equivalent == NULL)
        {
            i->second.label = reindexed_label++;
            components.push_back(i->second);
        }
    }

    // Pass 2, mark new labels on the grid
    for (rectangle_iterator pos(1); pos; ++pos)
    {
        int label = connectivity_map(*pos);
        if (label  != 0)
        {
            connectivity_map(*pos) = _min_transitive_label(intermediate_components[label]);
        }
    }
}

// Is this square a wall, or does it belong to a vault? both are considered to
// block connectivity.
static bool _passable_square(const coord_def & pos)
{
    return (!feat_is_wall(env.grid(pos)) && !(env.level_map_mask(pos) & MMT_VAULT));
}

struct adjacency_test
{
    adjacency_test()
    {
        adjacency.init(0);
    }
    FixedArray<int, GXM, GYM> adjacency;
    bool operator()(const coord_def & pos)
    {
        return (_passable_square(pos) && adjacency(pos) == 0);
    }
};

struct dummy_estimate
{
    bool operator() (const coord_def & pos)
    {
        return (0);
    }
};

struct adjacent_costs
{
    FixedArray<int, GXM, GYM> * adjacency;
    int operator()(const coord_def & pos)
    {
        return (*adjacency)(pos);
    }

};

struct label_match
{
    FixedArray<int, GXM, GYM> * labels;
    int target_label;
    bool operator()(const coord_def & pos)
    {
        return ((*labels)(pos) == target_label);
    }
};

// Connectivity checks to make sure that the parts of a bubble are not
// obstructed by slime wall adjacent squares
static void _slime_connectivity_fixup()
{
    // Generate a connectivity map considering any non wall, non vault square
    // passable
    FixedArray<int, GXM, GYM> connectivity_map;
    std::vector<map_component> components;
    _ccomps_8(connectivity_map, components, _passable_square);

    // Next we will generate a connectivity map with the above restrictions,
    // and also considering wall adjacent squares unpassable. But first we
    // build a map of how many walls are adjacent to each square in the level.
    // Walls/vault squares are flagged with DISCONNECT_DIST in this map.
    // This will be used to build the connectivity map, then later the adjacent
    // counts will define the costs of a search used to connect components in
    // the basic connectivity map that are broken apart in the restricted map
    FixedArray<int, GXM, GYM> non_adjacent_connectivity;
    std::vector<map_component> non_adj_components;
    adjacency_test adjacent_check;

    for (rectangle_iterator ri(1); ri; ++ri)
    {
        int count = 0;
        if (!_passable_square(*ri))
        {
            count = DISCONNECT_DIST;
        }
        else
        {
            for (adjacent_iterator adj(*ri); adj; ++adj)
            {
                if (feat_is_wall(env.grid(*adj)))
                {
                    // Not allowed to damage vault squares, so mark them
                    // inaccessible
                    if (env.level_map_mask(*adj) & MMT_VAULT)
                    {
                        count = DISCONNECT_DIST;
                        break;
                    }
                    else
                        count++;
                }

            }
        }
        adjacent_check.adjacency(*ri) = count;
    }

    _ccomps_8(non_adjacent_connectivity, non_adj_components, adjacent_check);

    // Now that we have both connectivity maps, go over each component in the
    // unrestricted map and connect any separate components in the restricted
    // map that it was broken up into.
    for (unsigned i = 0; i < components.size(); i++)
    {
        // Collect the components in the restricted connectivity map that
        // occupy part of the current component
        std::map<int, map_component *> present;
        for (rectangle_iterator ri(components[i].min_coord, components[i].max_coord); ri; ++ri)
        {
            int new_label = non_adjacent_connectivity(*ri);
            if (components[i].label == connectivity_map(*ri) && new_label != 0)
            {
                // the bit with new_label - 1 is foolish.
                present[new_label] = &non_adj_components[new_label-1];
            }
        }

        // Set one restricted component as the base point, and search to all
        // other restricted components
        std::map<int, map_component * >::iterator target_components = present.begin();

        // No non-wall adjacent squares in this component? This probably
        // shouldn't happen, but just move on.
        if (target_components == present.end())
            continue;

        map_component * base_component = target_components->second;
        ++target_components;

        adjacent_costs connection_costs;
        connection_costs.adjacency = &adjacent_check.adjacency;

        label_match valid_label;
        valid_label.labels = &non_adjacent_connectivity;

        dummy_estimate dummy;

        // Now search from our base component to the other components, and
        // clear out the path found
        for ( ; target_components != present.end(); ++target_components)
        {
            valid_label.target_label = target_components->second->label;

            std::vector<std::set<position_node>::iterator >path;
            std::set<position_node> visited;
            search_astar(base_component->seed_position, valid_label,
                         connection_costs, dummy, visited, path);


            // Did the search, now remove any walls adjacent to squares in
            // the path.
            const position_node * current = &(*path[0]);

            while (current)
            {
                if (adjacent_check.adjacency(current->pos) > 0)
                {
                    for (adjacent_iterator adj_it(current->pos); adj_it; ++adj_it)
                    {
                        if (feat_is_wall(env.grid(*adj_it)))
                        {
                            // This shouldn't happen since vault adjacent
                            // squares should have adjacency of DISCONNECT_DIST
                            // but oh well
                            if (env.level_map_mask(*adj_it) & MMT_VAULT)
                            {
                                mprf("Whoops, nicked a vault in slime connectivity fixup");
                            }
                            env.grid(*adj_it) = DNGN_FLOOR;
                        }
                    }
                    adjacent_check.adjacency(current->pos) = 0;
                }
                current = current->last;
            }

        }

    }
}

// Place vaults with CHANCE: that want to be placed on this level.
static void _place_chance_vaults()
{
    const level_id &lid(level_id::current());
    mapref_vector maps = random_chance_maps_in_depth(lid);
    // [ds] If there are multiple CHANCE maps that share an luniq_ or
    // uniq_ tag, only the first such map will be placed. Shuffle the
    // order of chosen maps so we don't have a first-map bias.
    std::random_shuffle(maps.begin(), maps.end());
    for (int i = 0, size = maps.size(); i < size; ++i)
    {
        const map_def *map = maps[i];
        if (!map->map_already_used())
        {
            dprf("Placing CHANCE vault: %s (%s)",
                 map->name.c_str(), map->chance(lid).describe().c_str());
            _build_secondary_vault(you.absdepth0, map);
        }
    }
}

static void _place_minivaults(void)
{
    if (use_random_maps)
    {
        const map_def *vault = NULL;

        if ((vault = random_map_for_place(level_id::current(), true)))
            _build_secondary_vault(you.absdepth0, vault);

        int tries = 0;
        do
        {
            vault = random_map_in_depth(level_id::current(), true);
            if (vault)
                _build_secondary_vault(you.absdepth0, vault);
        } // if ALL maps eligible are "extra" but fail to place, we'd be screwed
        while (vault && vault->has_tag("extra") && tries++ < 10000);
    }
}

static void _builder_normal(int level_number)
{
    const map_def *vault = _dgn_random_map_for_place(false);

    if (vault)
    {
        env.level_build_method += " random_map_for_place";
        _ensure_vault_placed_ex(_build_primary_vault(level_number, vault),
                                 vault);
        return;
    }

    if (use_random_maps)
    {
        vault = random_map_in_depth(level_id::current());
    }

    // We'll accept any kind of primary vault in the main dungeon, but only
    // ORIENT: encompass primary vaults in other branches. Other kinds of vaults
    // can still be placed in other branches as secondary vaults.
    if (vault && (player_in_branch(BRANCH_MAIN_DUNGEON)
                  || vault->orient == MAP_ENCOMPASS))
    {
        env.level_build_method += " random_map_in_depth";
        _ensure_vault_placed_ex(_build_primary_vault(level_number, vault),
                                 vault);
        return;
    }

    vault = random_map_for_tag("layout", true, true);

    if (!vault)
        die("Couldn't pick a layout.");

    dgn_ensure_vault_placed(_build_primary_vault(level_number, vault), false);
}

// Used to nuke shafts placed in corridors on low levels - it's just
// too nasty otherwise.
// Well, actually this just checks if it's next to a non-passable
// square. (jpeg)
static bool _shaft_is_in_corridor(const coord_def& c)
{
    for (orth_adjacent_iterator ai(c); ai; ++ai)
    {
        if (!in_bounds(*ai) || grd(*ai) < DNGN_MOVEMENT_MIN)
            return (true);
    }
    return (false);
}

static void _place_traps(int level_number)
{
    const int num_traps = num_traps_for_place(level_number);

    ASSERT(num_traps >= 0);
    ASSERT(num_traps <= MAX_TRAPS);

    for (int i = 0; i < num_traps; i++)
    {
        trap_def& ts(env.trap[i]);
        if (ts.type != TRAP_UNASSIGNED)
            continue;

        int tries;
        for (tries = 0; tries < 200; ++tries)
        {
            ts.pos.x = random2(GXM);
            ts.pos.y = random2(GYM);
            if (in_bounds(ts.pos)
                && grd(ts.pos) == DNGN_FLOOR
                && !map_masked(ts.pos, MMT_NO_TRAP))
            {
                break;
            }
        }

        if (tries == 200)
            break;

        while (ts.type >= NUM_TRAPS)
            ts.type = random_trap_for_place(level_number);

        if (ts.type == TRAP_SHAFT && level_number <= 7)
        {
            // Disallow shaft construction in corridors!
            if (_shaft_is_in_corridor(ts.pos))
            {
                // Choose again!
                ts.type = random_trap_for_place(level_number);

                // If we get shaft a second time, turn it into an alarm trap, or
                // if we got nothing.
                if (ts.type == TRAP_SHAFT || ts.type >= NUM_TRAPS)
                    ts.type = TRAP_ALARM;
            }
        }

        grd(ts.pos) = DNGN_UNDISCOVERED_TRAP;
        env.tgrid(ts.pos) = i;
        if (ts.type == TRAP_SHAFT && shaft_known(level_number, true))
            ts.reveal();
        ts.prepare_ammo();
    }

    if (player_in_branch(BRANCH_SPIDER_NEST))
    {
        int max_webs = 400 - num_traps - 50;
        // Adjust for branch depth
        max_webs = max_webs / (6 - player_branch_depth()) / 2;
        // Vary from 1/2 to full max amount
        place_webs(max_webs + random2(max_webs));
    }
    else if (player_in_branch(BRANCH_CRYPT))
        place_webs(random2(20));
}

static void _dgn_place_feature_at_random_floor_square(dungeon_feature_type feat,
                                                      unsigned mask = MMT_VAULT)
{
    const coord_def place =
        _dgn_random_point_in_bounds(DNGN_FLOOR, mask, DNGN_FLOOR);
    if (place.origin())
        throw dgn_veto_exception("Cannot place feature at random floor square.");
    else
        grd(place) = feat;
}

// Create randomly-placed stone stairs.
void dgn_place_stone_stairs(bool maybe_place_hatches)
{
    const int stair_start = DNGN_STONE_STAIRS_DOWN_I;
    const int stair_count = DNGN_ESCAPE_HATCH_UP - stair_start + 1;

    FixedVector < bool, stair_count > existing;

    existing.init(false);

    for (rectangle_iterator ri(0); ri; ++ri)
        if (grd(*ri) >= stair_start && grd(*ri) < stair_start + stair_count)
            existing[grd(*ri) - stair_start] = true;

    int pair_count = 3;

    if (maybe_place_hatches && coinflip())
        pair_count++;

    for (int i = 0; i < pair_count; ++i)
    {
        if (!existing[i])
            _dgn_place_feature_at_random_floor_square(
                static_cast<dungeon_feature_type>(DNGN_STONE_STAIRS_DOWN_I + i));

        if (!existing[DNGN_STONE_STAIRS_UP_I - stair_start + i])
            _dgn_place_feature_at_random_floor_square(
                static_cast<dungeon_feature_type>(DNGN_STONE_STAIRS_UP_I + i));
    }
}

bool dgn_has_adjacent_feat(coord_def c, dungeon_feature_type feat)
{
    for (adjacent_iterator ai(c); ai; ++ai)
        if (grd(*ai) == feat)
            return true;
    return false;
}

coord_def dgn_random_point_in_margin(int margin)
{
    return coord_def(random_range(margin, GXM - margin - 1),
                     random_range(margin, GYM - margin - 1));
}

static inline bool _point_matches_feat(coord_def c,
                                       dungeon_feature_type searchfeat,
                                       uint32_t mapmask,
                                       dungeon_feature_type adjacent_feat,
                                       bool monster_free)
{
    return (grd(c) == searchfeat
            && (!monster_free || !monster_at(c))
            && !map_masked(c, mapmask)
            && (adjacent_feat == DNGN_UNSEEN ||
                dgn_has_adjacent_feat(c, adjacent_feat)));
}

// Returns a random point in map bounds matching the given search feature,
// and respecting the map mask (a map mask of MMT_VAULT ensures that
// positions inside vaults will not be returned).
//
// If adjacent_feat is not DNGN_UNSEEN, the chosen square will be
// adjacent to a square containing adjacent_feat.
//
// If monster_free is true, the chosen square will never be occupied by
// a monster.
//
// If tries is set to anything other than -1, this function will make tries
// attempts to find a suitable square, and may fail if the map is crowded.
// If tries is set to -1, this function will examine the entire map and
// guarantees to find a suitable point if available.
//
// If a suitable point is not available (or was not found in X tries),
// returns coord_def(0,0)
//
static coord_def _dgn_random_point_in_bounds(dungeon_feature_type searchfeat,
                                     uint32_t mapmask,
                                     dungeon_feature_type adjacent_feat,
                                     bool monster_free,
                                     int tries)
{
    if (tries == -1)
    {
        // Try a quick and dirty random search:
        int n = 10;
        if (searchfeat == DNGN_FLOOR)
            n = 500;
        coord_def chosen = _dgn_random_point_in_bounds(searchfeat,
                                                       mapmask,
                                                       adjacent_feat,
                                                       monster_free,
                                                       n);
        if (!chosen.origin())
            return chosen;

        // Exhaustive search; will never fail if a suitable place is
        // available, but is also far more expensive.
        int nfound = 0;
        for (rectangle_iterator ri(1); ri; ++ri)
        {
            const coord_def c(*ri);
            if (_point_matches_feat(c, searchfeat, mapmask, adjacent_feat,
                                    monster_free)
                && one_chance_in(++nfound))
            {
                chosen = c;
            }
        }
        return (chosen);
    }
    else
    {
        // Random search.
        while (--tries >= 0)
        {
            const coord_def c = random_in_bounds();
            if (_point_matches_feat(c, searchfeat, mapmask, adjacent_feat,
                                    monster_free))
                return c;
        }
        return (coord_def(0, 0));
    }
}

static coord_def _place_specific_feature(dungeon_feature_type feat)
{
    /* Only overwrite vaults when absolutely necessary. */
    coord_def c = _dgn_random_point_in_bounds(DNGN_FLOOR, MMT_VAULT, DNGN_UNSEEN, true);
    if (!in_bounds(c))
        c = _dgn_random_point_in_bounds(DNGN_FLOOR, 0, DNGN_UNSEEN, true);

    if (in_bounds(c))
        env.grid(c) = feat;
    else
        throw dgn_veto_exception("Cannot place specific feature.");

    return c;
}

static bool _place_vault_by_tag(const std::string &tag, int dlevel)
{
    const map_def *vault = random_map_for_tag(tag, true);
    if (!vault)
        return (false);

    return _build_secondary_vault(dlevel, vault);
}

static void _place_specific_stair(dungeon_feature_type stair,
                                  const std::string &tag,
                                  int dlevel)
{
    if (tag.empty() || !_place_vault_by_tag(tag, dlevel))
        _place_specific_feature(stair);
}

static void _place_branch_entrances(int dlevel, level_area_type level_type)
{
    if (level_type != LEVEL_DUNGEON)
        return;

    // Place actual branch entrances.
    for (int i = 0; i < NUM_BRANCHES; ++i)
    {
        // Vestibule of Hell startdepth is set only to prevent mimics.
        // Entries are placed with vaults.
        if (i == BRANCH_VESTIBULE_OF_HELL)
            continue;

        Branch *b = &branches[i];

        const bool mimic = !branch_is_unfinished(b->id)
                           && !is_hell_subbranch(b->id)
                           && dlevel >= FEATURE_MIMIC_DEPTH
                           && player_branch_depth() >= b->mindepth
                           && player_branch_depth() <= b->maxdepth
                           && one_chance_in(FEATURE_MIMIC_CHANCE);

        if (b->entry_stairs != NUM_FEATURES
            && player_in_branch(b->parent_branch)
            && (player_branch_depth() == b->startdepth || mimic))
        {
            // Place a stair.
            dprf("Placing stair to %s", b->shortname);

            std::string entry_tag = std::string(b->abbrevname);
            entry_tag += "_entry";
            lowercase(entry_tag);

            _place_specific_stair(b->entry_stairs, entry_tag, dlevel);
        }
    }
}

static void _place_extra_vaults()
{
    while (true)
    {
        if (!player_in_branch(BRANCH_MAIN_DUNGEON) && use_random_maps)
        {
            const map_def *vault = random_map_in_depth(level_id::current());

            // Encompass vaults can't be used as secondaries.
            if (!vault || vault->orient == MAP_ENCOMPASS)
                break;

            if (vault && _build_secondary_vault(you.absdepth0, vault))
            {
                const map_def &map(*vault);
                if (map.has_tag("extra"))
                    continue;
                use_random_maps = false;
            }
        }
        break;
    }
}

// Place uniques on the level.
// There is a hidden dependency on the player's actual
// location (through your_branch()).
// Return the number of uniques placed.
static int _place_uniques(int level_number, level_area_type level_type)
{
    // Unique beasties:
    if (level_number <= 0 || level_type != LEVEL_DUNGEON
        || !your_branch().has_uniques)
    {
        return 0;
    }

#ifdef DEBUG_UNIQUE_PLACEMENT
    FILE *ostat = fopen("unique_placement.log", "a");
    fprintf(ostat, "--- Looking to place uniques on: Level %d of %s ---\n", level_number, your_branch().shortname);
#endif

    int num_placed = 0;

    // Magic numbers for dpeg's unique system.
    int A = 2;
    const int B = 5;
    while (one_chance_in(A))
    {
        // In dpeg's unique placement system, chances is always 1 in A of even
        // starting to place a unique; reduced if there are less uniques to be
        // placed or available. Then there is a chance of uniques_available /
        // B; this only triggers on levels that have less than B uniques to be
        // placed.
        const mapref_vector uniques_available =
            find_maps_for_tag("place_unique", true, true);

        if (random2(B) >= std::min(B, int(uniques_available.size())))
            break;

        const map_def *uniq_map = random_map_for_tag("place_unique", true);
        if (!uniq_map)
        {
#ifdef DEBUG_UNIQUE_PLACEMENT
            fprintf(ostat, "Dummy balance or no uniques left.\n");
#endif
            break;
        }

        const bool map_placed = dgn_place_map(uniq_map, false, false);
        if (map_placed)
        {
            num_placed++;
            // Make the placement chance drop steeply after
            // some have been placed, to reduce chance of
            // many uniques per level.
            if (num_placed >= 3)
                A++;
#ifdef DEBUG_UNIQUE_PLACEMENT
            fprintf(ostat, "Placed valid unique map: %s.\n",
                    uniq_map->name.c_str());
#endif
            dprf("Placed %s.", uniq_map->name.c_str());
        }
#ifdef DEBUG_UNIQUE_PLACEMENT
        else
        {
            fprintf(ostat, "Didn't place valid map: %s\n",
                    uniq_map->name.c_str());
        }
#endif
    }

#ifdef DEBUG_UNIQUE_PLACEMENT
    fprintf(ostat, "--- Finished this set, placed %d uniques.\n", num_placed);
    fclose(ostat);
#endif
    return num_placed;
}

static int _place_monster_vector(std::vector<monster_type> montypes,
                                 int level_number, int num_to_place)
{
    int result = 0;

    mgen_data mg;
    mg.power     = level_number;
    mg.behaviour = BEH_SLEEP;
    mg.flags    |= MG_PERMIT_BANDS;
    mg.map_mask |= MMT_NO_MONS;

    for (int i = 0; i < num_to_place; i++)
    {
        mg.cls = montypes[random2(montypes.size())];

        if (player_in_hell() &&
            mons_class_can_be_zombified(mg.cls))
        {
            static const monster_type lut[3][2] =
            {
                { MONS_SKELETON_SMALL, MONS_SKELETON_LARGE },
                { MONS_ZOMBIE_SMALL, MONS_ZOMBIE_LARGE },
                { MONS_SIMULACRUM_SMALL, MONS_SIMULACRUM_LARGE },
            };

            mg.base_type = mg.cls;
            int s = mons_skeleton(mg.cls) ? 2 : 0;
            mg.cls = lut[random_choose_weighted(s, 0, 8, 1, 1, 2, 0)]
                        [mons_zombie_size(mg.base_type) == Z_BIG];
        }

        else
            mg.base_type = MONS_NO_MONSTER;
        if (place_monster(mg))
            ++result;
    }

    return result;
}


static void _place_aquatic_monsters(int level_number, level_area_type level_type)
{
    int lava_spaces = 0, water_spaces = 0;
    std::vector<monster_type> swimming_things(4u, MONS_NO_MONSTER);

    // [ds] Shoals relies on normal monster generation to place its monsters.
    // Given the amount of water area in the Shoals, placing water creatures
    // explicitly explodes the Shoals' xp budget.
    //
    // Also disallow water creatures below D:6.
    //
    if (player_in_branch(BRANCH_SHOALS)
        || (player_in_branch(BRANCH_MAIN_DUNGEON)
            && you.absdepth0 < 5))
    {
        return;
    }

    // Count the number of lava and water tiles {dlb}:
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (grd(*ri) == DNGN_LAVA)
            lava_spaces++;

        if (feat_is_water(grd(*ri)))
            water_spaces++;
    }

    if (lava_spaces > 49 && level_number > 6)
    {
        for (int i = 0; i < 4; i++)
        {
            swimming_things[i] = static_cast<monster_type>(
                                     MONS_LAVA_WORM + random2(3));

            if (one_chance_in(30))
                swimming_things[i] = MONS_SALAMANDER;
        }

        _place_monster_vector(swimming_things, level_number,
                              std::min(random2avg(9, 2)
                                       + (random2(lava_spaces) / 10), 15));
    }

    if (water_spaces > 49)
    {
        // This can probably be done in a better way with something
        // like water_monster_rarity().
        for (int i = 0; i < 4; i++)
        {
            swimming_things[i] =
                static_cast<monster_type>(MONS_BIG_FISH + random2(4));

            if (player_in_branch(BRANCH_SWAMP) && !one_chance_in(3))
                swimming_things[i] = MONS_SWAMP_WORM;
            else if (player_in_hell())
            {
                // Eels are useless when zombified
                if (swimming_things[i] == MONS_ELECTRIC_EEL)
                {
                    swimming_things[i] = one_chance_in(4) ? MONS_KRAKEN :
                                             MONS_WATER_ELEMENTAL;
                }
            }
        }

        // Don't place sharks in the Swamp.
        if (!player_in_branch(BRANCH_SWAMP)
            && level_number >= 9 && one_chance_in(4))
        {
            swimming_things[3] = MONS_SHARK;
        }

        if (level_number >= 25 && one_chance_in(5))
            swimming_things[0] = MONS_WATER_ELEMENTAL;

        _place_monster_vector(swimming_things, level_number,
                              std::min(random2avg(9, 2)
                                + (random2(water_spaces) / 10), 15));
    }
}

bool door_vetoed(const coord_def pos)
{
    return env.markers.property_at(pos, MAT_ANY, "veto_open") == "veto";
}

static void _builder_monsters(int level_number, level_area_type level_type, int mon_wanted)
{
    if (player_in_branch(BRANCH_ECUMENICAL_TEMPLE))
        return;

    if (level_type == LEVEL_PANDEMONIUM)
    {
        dprf("Generating Pan monsters, set:%s", zotdef_debug_wave_desc().c_str());
        // TODO: allow regular generation to handle this
        while (mon_wanted-- > 0)
            pandemonium_mons();
        return;
    }

    const bool in_shoals = player_in_branch(BRANCH_SHOALS);
    if (in_shoals)
        dgn_shoals_generate_flora();

    // Try to place Shoals monsters on floor where possible instead of
    // letting all the merfolk be generated in the middle of the
    // water.
    const dungeon_feature_type preferred_grid_feature =
        in_shoals? DNGN_FLOOR : DNGN_UNSEEN;

    dprf("_builder_monsters: Generating %d monsters", mon_wanted);
    for (int i = 0; i < mon_wanted; i++)
    {
        mgen_data mg;
        mg.behaviour              = BEH_SLEEP;
        mg.power                  = level_number;
        mg.flags                 |= MG_PERMIT_BANDS;
        mg.map_mask              |= MMT_NO_MONS;
        mg.preferred_grid_feature = preferred_grid_feature;

        place_monster(mg);
    }

    if (!player_in_branch(BRANCH_CRYPT)) // No water creatures in the Crypt.
        _place_aquatic_monsters(level_number, level_type);
}

static void _builder_items(int level_number, int items_wanted)
{
    int i = 0;
    object_class_type specif_type = OBJ_RANDOM;
    int items_levels = level_number;

    if (player_in_branch(BRANCH_VAULTS))
    {
        items_levels *= 15;
        items_levels /= 10;
    }
    else if (player_in_branch(BRANCH_ORCISH_MINES))
        specif_type = OBJ_GOLD;  // Lots of gold in the orcish mines.

    for (i = 0; i < items_wanted; i++)
    {
        items(1, specif_type, OBJ_RANDOM, false, items_levels, 250,
              MMT_NO_ITEM);
    }
}

static bool _connect_vault_exit(const coord_def& exit)
{
    flood_find<feature_grid, coord_predicate> ff(env.grid, in_bounds, true,
                                                 false);
    ff.add_feat(DNGN_FLOOR);

    coord_def target = ff.find_first_from(exit, env.level_map_mask);

    if (in_bounds(target))
        return join_the_dots(exit, target, MMT_VAULT);

    return false;
}

static bool _grid_needs_exit(const coord_def& c)
{
    return (!cell_is_solid(c)
            || feat_is_closed_door(grd(c))
            || grd(c) == DNGN_SECRET_DOOR);
}

static bool _map_feat_is_on_edge(const vault_placement &place,
                                 const coord_def &c)
{
    if (!place.map.in_map(c - place.pos))
        return false;

    for (adjacent_iterator ai(c); ai; ++ai)
        if (!place.map.in_map(*ai - place.pos))
            return true;

    return (false);
}

static void _pick_float_exits(vault_placement &place,
                              std::vector<coord_def> &targets)
{
    std::vector<coord_def> possible_exits;

    for (rectangle_iterator ri(place.pos, place.pos + place.size - 1); ri; ++ri)
        if (_grid_needs_exit(*ri) && _map_feat_is_on_edge(place, *ri))
            possible_exits.push_back(*ri);

    if (possible_exits.empty())
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_ERROR, "Unable to find exit from %s",
             place.map.name.c_str());
#endif
        return;
    }

    const int npoints = possible_exits.size();
    int nexits = npoints < 6? npoints : npoints / 8 + 1;

    if (nexits > 10)
        nexits = 10;

    while (nexits-- > 0)
    {
        int which_exit = random2(possible_exits.size());
        targets.push_back(possible_exits[which_exit]);
        possible_exits.erase(possible_exits.begin() + which_exit);
    }
}

static void _fixup_after_vault()
{
    _dgn_set_floor_colours();

    link_items();
    env.markers.activate_all();

    // Force teleport to place the player somewhere sane.
    you_teleport_now(false, false);

    setup_environment_effects();
}

// Places a map on the current level (minivault or regular vault).
//
// You can specify the centre of the map using "where" for floating vaults
// and minivaults. "where" is ignored for other vaults. XXX: it might be
// nice to specify a square that is not the centre, but is identified by
// a marker in the vault to be placed.
//
// NOTE: encompass maps will destroy the existing level!
//
// clobber: If true, assumes the newly placed vault can clobber existing
//          items and monsters (items may be destroyed, monsters may be
//          teleported).
//
// Non-dungeon code should generally use dgn_safe_place_map instead of
// this function to recover from map_load_exceptions.
bool dgn_place_map(const map_def *mdef,
                   bool clobber,
                   bool make_no_exits,
                   const coord_def &where)
{
    if (!mdef)
        return (false);

    const dgn_colour_override_manager colour_man;

    bool did_map = false;
    if (mdef->orient == MAP_ENCOMPASS && !Generating_Level)
    {
        if (clobber)
        {
            // For encompass maps, clear the entire level.
            unwind_bool levgen(Generating_Level, true);
            dgn_reset_level();
            dungeon_events.clear();
            const bool res = dgn_place_map(mdef, clobber, make_no_exits, where);
            _fixup_after_vault();
            return (res);
        }
        else
        {
            mprf(MSGCH_DIAGNOSTICS,
                 "Cannot generate encompass map '%s' without clobber=true",
                 mdef->name.c_str());

            return (false);
        }
    }

    const int map_index = env.level_vaults.size();
    did_map = _build_secondary_vault(you.absdepth0, mdef, clobber,
                                     make_no_exits, where);

    // Activate any markers within the map.
    if (did_map && !Generating_Level)
    {
        const vault_placement &vp = *env.level_vaults[map_index];
        ASSERT(mdef->name == vp.map.name);
        for (vault_place_iterator vpi(vp); vpi; ++vpi)
        {
            const coord_def p = *vpi;
            env.markers.activate_markers_at(p);
            if (!you.see_cell(p))
                set_terrain_changed(p);
        }
        env.markers.clear_need_activate();

        setup_environment_effects();
        _dgn_postprocess_level();
    }

    return (did_map);
}

// Identical to dgn_place_map, but recovers gracefully from
// map_load_exceptions. Prefer this function if placing maps *not*
// during level generation time.
//
// Returns the map actually placed if the map was placed successfully.
// This is usually the same as the map passed in, unless map load
// failed and maps had to be reloaded.
const map_def *dgn_safe_place_map(const map_def *mdef,
                                  bool clobber,
                                  bool make_no_exits,
                                  const coord_def &where)
{
    const std::string mapname(mdef->name);
    int retries = 10;
    while (true)
    {
        try
        {
            const bool placed =
                dgn_place_map(mdef, clobber, make_no_exits, where);
            return (placed? mdef : NULL);
        }
        catch (map_load_exception &mload)
        {
            if (retries-- > 0)
            {
                mprf(MSGCH_ERROR,
                     "Failed to load map %s in dgn_safe_place_map, "
                     "reloading all maps",
                     mload.what());
                reread_maps();

                mdef = find_map_by_name(mapname);
            }
            else
                return (NULL);
        }
    }
}

vault_placement *dgn_vault_at(coord_def p)
{
    const int map_index = env.level_map_ids(p);
    return (map_index == INVALID_MAP_INDEX? NULL : env.level_vaults[map_index]);
}

void dgn_seen_vault_at(coord_def p)
{
    if (vault_placement *vp = dgn_vault_at(p))
    {
        if (!vp->seen)
        {
            dprf("Vault %s (%d,%d)-(%d,%d) seen",
                 vp->map.name.c_str(), vp->pos.x, vp->pos.y,
                 vp->size.x, vp->size.y);
            vp->seen = true;
        }
    }
}

static bool _vault_wants_damage(const vault_placement &vp)
{
    const map_def &map = vp.map;
    if (map.has_tag("ruin"))
        return (true);

    // Some vaults may want to be ruined only in certain places with
    // tags like "ruin_abyss" or "ruin_lair"
    std::string place_desc = level_id::current().describe(false, false);
    lowercase(place_desc);
    return (map.has_tag("ruin_" + place_desc));
}

static void _ruin_vault(const vault_placement &vp)
{
    _ruin_level(vault_place_iterator(vp), 0, 12, 0);
}

// Places a vault somewhere in an already built level if possible.
// Returns true if the vault was successfully placed.
static bool _build_secondary_vault(int level_number, const map_def *vault,
                                   bool clobber, bool no_exits,
                                   const coord_def &where)
{
    const bool spotty = player_in_branch(BRANCH_ORCISH_MINES)
                        || player_in_branch(BRANCH_SLIME_PITS);
    const int map_index = env.level_vaults.size();
    if (_build_vault_impl(level_number, vault, true, !clobber, no_exits, where))
    {
        if (!no_exits)
        {
            const vault_placement &vp = *env.level_vaults[map_index];
            ASSERT(vault->name == vp.map.name);
            vp.connect(spotty);
        }
        return (true);
    }
    return (false);
}

// Builds a primary vault - i.e. a vault that is built before anything
// else on the level. After placing the vault, rooms and corridors
// will be constructed on the level and the vault exits will be
// connected to corridors.
//
// If portions of the level are already generated at this point, use
// _build_secondary_vault or dgn_place_map instead.
//
// NOTE: minivaults can never be placed as primary vaults.
//
static bool _build_primary_vault(int level_number, const map_def *vault)
{
    return _build_vault_impl(level_number, vault);
}

// Builds a vault or minivault. Do not use this function directly: always
// prefer _build_secondary_vault or _build_primary_vault.
static bool _build_vault_impl(int level_number, const map_def *vault,
                              bool build_only, bool check_collisions,
                              bool make_no_exits, const coord_def &where)
{
    if (dgn_check_connectivity && !dgn_zones)
        dgn_zones = dgn_count_disconnected_zones(false);

    vault_placement place;

    place.level_number = level_number;

    if (map_bounds(where))
        place.pos = where;

    const map_section_type placed_vault_orientation =
        vault_main(place, vault, check_collisions);

    dprf("Map: %s; placed: %s; place: (%d,%d), size: (%d,%d)",
         vault->name.c_str(),
         placed_vault_orientation != MAP_NONE? "yes" : "no",
         place.pos.x, place.pos.y, place.size.x, place.size.y);

    if (placed_vault_orientation == MAP_NONE)
        return (false);

    // XXX: Moved this out of dgn_register_place so that vault-set monsters can
    // be accessed with the '9' and '8' glyphs. (due)
    if (!place.map.random_mons.empty())
    {
        dprf("Setting the custom random mons list.");
        set_vault_mon_list(place.map.random_mons);
    }

    place.apply_grid();

    if (_vault_wants_damage(place))
        _ruin_vault(place);

    if (place.exits.empty() && placed_vault_orientation != MAP_ENCOMPASS
        && (!place.map.is_minivault() || place.map.has_tag("mini_float")))
    {
        _pick_float_exits(place, place.exits);
    }

    if (make_no_exits)
        place.exits.clear();

    // Must do this only after target_connections is finalised, or the vault
    // exits will not be correctly set.
    dgn_register_place(place, true);

#ifdef DEBUG_DIAGNOSTICS
    if (crawl_state.map_stat_gen)
        mapgen_report_map_use(place.map);
#endif

    const bool is_layout = place.map.is_overwritable_layout();
    // If the map takes the whole screen or we were only requested to
    // build the vault, our work is done.
    if (!build_only && (placed_vault_orientation != MAP_ENCOMPASS || is_layout))
    {
        if (!is_layout)
            _build_postvault_level(place);

        dgn_place_stone_stairs(true);
    }

    // Fire any post-place hooks defined for this map; any failure
    // here is an automatic veto. Note that the post-place hook must
    // be run only after _build_postvault_level.
    if (!place.map.run_postplace_hook())
        throw dgn_veto_exception("Post-place hook failed for: "
                                 + place.map.name);

    return (true);
}

static void _build_postvault_level(vault_placement &place)
{
    // Does this level require Dis treatment (metal wallification)?
    // XXX: Change this so the level definition can explicitly state what
    // kind of wallification it wants.
    if (place.map.has_tag("dis"))
        dgn_build_chaotic_city_level(DNGN_METAL_WALL);
    else if (player_in_branch(BRANCH_SWAMP))
        dgn_build_swamp_level(place.level_number);
    else
    {
        dgn_build_rooms_level(random_range(25, 100));

        // Excavate and connect the vault to the rest of the level.
        place.connect();
    }
}

static const object_class_type _acquirement_item_classes[] =
{
    OBJ_JEWELLERY,
    OBJ_BOOKS,
    OBJ_MISCELLANY,
    OBJ_WEAPONS,
    OBJ_ARMOUR,
    OBJ_WANDS,
    OBJ_STAVES,
};

#define NC_KITTEHS           3
#define NC_LESSER_LIFE_FORMS ARRAYSZ(_acquirement_item_classes)

int dgn_item_corpse(const item_spec &ispec, const coord_def where)
{
    mons_spec mspec(ispec.corpse_monster_spec());
    int corpse_index = -1;
    for (int tries = 0; ; tries++)
    {
        if (tries > 200)
            return NON_ITEM;
        monster *mon = dgn_place_monster(mspec, you.absdepth0, coord_def(), true);
        if (!mon)
            continue;
        mon->position = where;
        if (mons_class_can_leave_corpse(mon->type))
            corpse_index = place_monster_corpse(mon, true, true);
        // Dismiss the monster we used to place the corpse.
        mon->flags |= MF_HARD_RESET;
        monster_die(mon, KILL_DISMISSED, NON_MONSTER, false, true);

        if (corpse_index != -1 && corpse_index != NON_ITEM)
            break;
    }

    item_def &corpse(mitm[corpse_index]);
    if (ispec.props.exists(CORPSE_NEVER_DECAYS))
        corpse.props[CORPSE_NEVER_DECAYS].get_bool() =
            ispec.props[CORPSE_NEVER_DECAYS].get_bool();

    if (ispec.base_type == OBJ_CORPSES && ispec.sub_type == CORPSE_SKELETON)
        turn_corpse_into_skeleton(corpse);
    else if (ispec.base_type == OBJ_FOOD && ispec.sub_type == FOOD_CHUNK)
        turn_corpse_into_chunks(corpse, false, false);

    if (ispec.props.exists(MONSTER_HIT_DICE))
        corpse.props[MONSTER_HIT_DICE].get_short() =
            ispec.props[MONSTER_HIT_DICE].get_short();

    if (ispec.qty && ispec.base_type == OBJ_FOOD)
        corpse.quantity = ispec.qty;

    return corpse_index;
}

int dgn_place_item(const item_spec &spec,
                   const coord_def &where,
                   int level)
{
    // Dummy object?
    if (spec.base_type == OBJ_UNASSIGNED)
        return (NON_ITEM);

    if (level == INVALID_ABSDEPTH)
        level = you.absdepth0;

    object_class_type base_type = spec.base_type;
    bool acquire = false;

    if (spec.level >= 0)
        level = spec.level;
    else
    {
        bool adjust_type = true;
        switch (spec.level)
        {
        case ISPEC_DAMAGED:
        case ISPEC_BAD:
        case ISPEC_RANDART:
            level = spec.level;
            break;
        case ISPEC_GOOD:
            level = 5 + level * 2;
            break;
        case ISPEC_SUPERB:
            level = MAKE_GOOD_ITEM;
            break;
        case ISPEC_ACQUIREMENT:
            acquire = true;
            break;
        default:
            adjust_type = false;
            break;
        }

        if (spec.props.exists("mimic") && base_type == OBJ_RANDOM)
            base_type = get_random_item_mimic_type();
        else if (adjust_type && base_type == OBJ_RANDOM)
        {
            base_type = _acquirement_item_classes[random2(
                            you.species == SP_FELID ? NC_KITTEHS :
                            NC_LESSER_LIFE_FORMS)];
        }
    }

    int useless_tries = 0;
retry:

    const int item_made =
        (acquire ?
         acquirement_create_item(base_type, spec.acquirement_source,
                                 true, where)
         : spec.corpselike() ? dgn_item_corpse(spec, where)
         : items(spec.allow_uniques, base_type,
                  spec.sub_type, true, level, spec.race, 0,
                  spec.ego, -1, spec.level == ISPEC_MUNDANE));

    if (item_made != NON_ITEM && item_made != -1)
    {
        item_def &item(mitm[item_made]);
        item.pos = where;
        CrawlHashTable props = spec.props;

        if (props.exists("make_book_theme_randart"))
        {
            std::string owner = props["randbook_owner"].get_string();
            if (owner == "player")
                owner = you.your_name;

            std::vector<spell_type> spells;
            CrawlVector spell_list = props["randbook_spells"].get_vector();
            for (unsigned int i = 0; i < spell_list.size(); ++i)
                spells.push_back((spell_type) spell_list[i].get_int());

            make_book_theme_randart(item,
                spells,
                props["randbook_disc1"].get_short(),
                props["randbook_disc2"].get_short(),
                props["randbook_num_spells"].get_short(),
                props["randbook_slevels"].get_short(),
                owner,
                props["randbook_title"].get_string());
        }

        // Remove unsuitable inscriptions such as {god gift}.
        item.inscription.clear();
        // And wipe item origin to remove "this is a god gift!" from there,
        // unless we're dealing with a corpse.
        if (!spec.corpselike())
            origin_reset(item);
        if (is_stackable_item(item) && spec.qty > 0)
        {
            item.quantity = spec.qty;
            if (is_blood_potion(item))
                init_stack_blood_potions(item);
        }

        if (spec.item_special)
            item.special = spec.item_special;

        if (spec.plus >= 0
            && (item.base_type == OBJ_BOOKS
                && item.sub_type == BOOK_MANUAL)
            || (item.base_type == OBJ_MISCELLANY
                && item.sub_type == MISC_RUNE_OF_ZOT))
        {
            item.plus = spec.plus;
            item_colour(item);
        }

        if (item_is_rune(item) && you.runes[item.plus])
        {
            destroy_item(item, true);
            return NON_ITEM;
        }

        if (props.exists("cursed"))
            do_curse_item(item);
        else if (props.exists("uncursed"))
            do_uncurse_item(item, false);
        if (props.exists("useful") && (useless_tries++ < 10)
            && is_useless_item(item, false))
        {
            destroy_item(item, true);
            goto retry;
        }
        if (item.base_type == OBJ_WANDS && props.exists("charges"))
            item.plus = props["charges"].get_int();
        if (props.exists("ident"))
            item.flags |= props["ident"].get_int();
        if (props.exists("unobtainable"))
            item.flags |= ISFLAG_UNOBTAINABLE;
        if (props.exists("mimic"))
        {
            const int chance = props["mimic"];
            if (chance > 0 && one_chance_in(chance))
                item.flags |= ISFLAG_MIMIC;
        }
        if (props.exists("no_mimic"))
            item.flags |= ISFLAG_NO_MIMIC;

        return (item_made);
    }

    return (NON_ITEM);
}

void dgn_place_multiple_items(item_list &list,
                              const coord_def& where, int level)
{
    const int size = list.size();
    for (int i = 0; i < size; ++i)
        dgn_place_item(list.get_item(i), where, level);
}

static void _dgn_place_item_explicit(int index, const coord_def& where,
                                     vault_placement &place,
                                     int level)
{
    item_list &sitems = place.map.items;

    if ((index < 0 || index >= static_cast<int>(sitems.size())) &&
        !crawl_state.game_is_sprint())
    {
        return;
    }

    const item_spec spec = sitems.get_item(index);
    dgn_place_item(spec, where, level);
}

static void _dgn_give_mon_spec_items(mons_spec &mspec,
                                     monster *mon,
                                     const int type,
                                     const int monster_level)
{
    unwind_var<int> save_speedinc(mon->speed_increment);

    // Get rid of existing equipment.
    for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
        if (mon->inv[i] != NON_ITEM)
        {
            item_def &item(mitm[mon->inv[i]]);
            mon->unequip(item, i, 0, true);
            destroy_item(mon->inv[i], true);
            mon->inv[i] = NON_ITEM;
        }

    item_make_species_type racial = MAKE_ITEM_RANDOM_RACE;

    if (mons_genus(type) == MONS_ORC)
        racial = MAKE_ITEM_ORCISH;
    else if (mons_genus(type) == MONS_DWARF)
        racial = MAKE_ITEM_DWARVEN;
    else if (mons_genus(type) == MONS_ELF)
        racial = MAKE_ITEM_ELVEN;

    item_list &list = mspec.items;

    const int size = list.size();
    for (int i = 0; i < size; ++i)
    {
        item_spec spec = list.get_item(i);

        if (spec.base_type == OBJ_UNASSIGNED)
            continue;

        // Don't give monster a randart, and don't randomly give
        // monster an ego item.
        if (spec.base_type == OBJ_ARMOUR || spec.base_type == OBJ_WEAPONS
            || spec.base_type == OBJ_MISSILES)
        {
            spec.allow_uniques = 0;
            if (spec.ego == 0)
                spec.ego = SP_FORBID_EGO;
        }

        // Gives orcs and elves appropriate racial gear, unless
        // otherwise specified.
        if (spec.race == MAKE_ITEM_RANDOM_RACE)
        {
            // But don't automatically give elves elven boots or
            // elven cloaks, or the same for dwarves.
            if ((racial != MAKE_ITEM_ELVEN
                    && racial != MAKE_ITEM_DWARVEN)
                || spec.base_type != OBJ_ARMOUR
                || (spec.sub_type != ARM_CLOAK
                    && spec.sub_type != ARM_BOOTS))
            {
                spec.race = racial;
            }
        }

        int item_level = monster_level;

        if (spec.level >= 0)
            item_level = spec.level;
        else
        {
            switch (spec.level)
            {
            case ISPEC_GOOD:
                item_level = 5 + item_level * 2;
                break;
            case ISPEC_SUPERB:
                item_level = MAKE_GOOD_ITEM;
                break;
            }
        }

        int useless_tries = 0;
    retry:

        const int item_made = items(spec.allow_uniques, spec.base_type,
                                     spec.sub_type, true, item_level,
                                     spec.race, 0, spec.ego, -1,
                                     spec.level == ISPEC_MUNDANE);

        if (item_made != NON_ITEM && item_made != -1)
        {
            item_def &item(mitm[item_made]);

            if (spec.props.exists("cursed"))
                do_curse_item(item);
            else if (spec.props.exists("uncursed"))
                do_uncurse_item(item, false);
            if (spec.props.exists("useful") && (useless_tries++ < 10)
                && is_useless_item(item, false))
            {
                destroy_item(item_made, true);
                goto retry;
            }

            // Mark items on summoned monsters as such.
            if (mspec.abjuration_duration != 0)
                item.flags |= ISFLAG_SUMMONED;

            if (!mon->pickup_item(item, 0, true))
                destroy_item(item_made, true);
        }
    }

    // Pre-wield ranged weapons.
    if (mon->inv[MSLOT_WEAPON] == NON_ITEM
        && mon->inv[MSLOT_ALT_WEAPON] != NON_ITEM)
    {
        mon->swap_weapons(false);
    }
}


monster* dgn_place_monster(mons_spec &mspec,
                           int monster_level, const coord_def& where,
                           bool force_pos, bool generate_awake, bool patrolling)
{
    if (mspec.type == -1)
        return 0;

    const monster_type type = static_cast<monster_type>(mspec.type);
    const bool m_generate_awake = (generate_awake || mspec.generate_awake);
    const bool m_patrolling     = (patrolling || mspec.patrolling);
    const bool m_band           = mspec.band;

    const int mlev = mspec.mlevel;
    if (mlev)
    {
        if (mlev > 0)
            monster_level = mlev;
        else if (mlev == -8)
            monster_level = 4 + monster_level * 2;
        else if (mlev == -9)
            monster_level += 5;
    }

    if (type != RANDOM_MONSTER && type < NUM_MONSTERS)
    {
        // Don't place a unique monster a second time.
        // (Boris is handled specially.)
        if (mons_is_unique(type) && you.unique_creatures[type]
            && !crawl_state.game_is_arena())
        {
            return 0;
        }

        const monster_type montype = mons_class_is_zombified(type)
                                                         ? mspec.monbase
                                                         : type;

        const habitat_type habitat = mons_class_primary_habitat(montype);

        if (in_bounds(where)
            && !monster_habitable_grid(montype, grd(where)))
        {
            dungeon_terrain_changed(where, habitat2grid(habitat));
        }
    }

    mgen_data mg(type);

    if (mg.cls == RANDOM_MONSTER && mspec.place.is_valid())
    {
        const monster_type mon =
            pick_random_monster_for_place(mspec.place, mspec.monbase,
                                          mlev == -9,
                                          mlev == -8,
                                          false);
        mg.cls = mon == MONS_NO_MONSTER? RANDOM_MONSTER : mon;
    }

    mg.power     = monster_level;
    mg.behaviour = (m_generate_awake) ? BEH_WANDER : BEH_SLEEP;
    switch (mspec.attitude)
    {
    case ATT_FRIENDLY:
        mg.behaviour = BEH_FRIENDLY;
        break;
    case ATT_NEUTRAL:
        mg.behaviour = BEH_NEUTRAL;
        break;
    case ATT_GOOD_NEUTRAL:
        mg.behaviour = BEH_GOOD_NEUTRAL;
        break;
    case ATT_STRICT_NEUTRAL:
        mg.behaviour = BEH_STRICT_NEUTRAL;
        break;
    default:
        break;
    }
    mg.base_type = mspec.monbase;
    mg.number    = mspec.number;
    mg.colour    = mspec.colour;

    if (mspec.god != GOD_NO_GOD)
        mg.god   = mspec.god;

    mg.mname     = mspec.monname;
    mg.hd        = mspec.hd;
    mg.hp        = mspec.hp;
    mg.props     = mspec.props;
    mg.initial_shifter = mspec.initial_shifter;

    // Marking monsters as summoned
    mg.abjuration_duration = mspec.abjuration_duration;
    mg.summon_type         = mspec.summon_type;
    mg.non_actor_summoner  = mspec.non_actor_summoner;

    // XXX: hack (also, never hand out darkgrey)
    if (mg.colour == -1)
        mg.colour = random_monster_colour();

    coord_def place(where);
    if (!force_pos && monster_at(place)
        && (mg.cls < NUM_MONSTERS || mg.cls == RANDOM_MONSTER))
    {
        const monster_type habitat_target =
            mg.cls == RANDOM_MONSTER ? MONS_BAT : mg.cls;
        place = find_newmons_square_contiguous(habitat_target, where, 0);
    }

    mg.pos = place;

    if (mons_class_is_zombified(mg.base_type))
    {
        if (mons_class_is_zombified(mg.cls))
            mg.base_type = MONS_NO_MONSTER;
        else
            std::swap(mg.base_type, mg.cls);
    }

    if (m_patrolling)
        mg.flags |= MG_PATROLLING;

    if (m_band)
        mg.flags |= MG_PERMIT_BANDS;

    // Store any extra flags here.
    mg.extra_flags |= mspec.extra_monster_flags;

    // have to do this before the monster is created, so things are
    // initialized properly
    if (mspec.props.exists("serpent_of_hell_flavour"))
        mg.props["serpent_of_hell_flavour"] =
            mspec.props["serpent_of_hell_flavour"].get_int();

    monster *mons = place_monster(mg, true, force_pos && place.origin());
    if (!mons)
        return 0;

    if (!mspec.items.empty())
        _dgn_give_mon_spec_items(mspec, mons, type, monster_level);

    if (mspec.explicit_spells)
        mons->spells = mspec.spells[random2(mspec.spells.size())];

    if (mspec.props.exists("monster_tile"))
    {
        mons->props["monster_tile"] =
            mspec.props["monster_tile"].get_short();
    }
    if (mspec.props.exists("monster_tile_name"))
    {
        mons->props["monster_tile_name"].get_string() =
            mspec.props["monster_tile_name"].get_string();
    }

    if (mspec.props.exists("always_corpse"))
        mons->props["always_corpse"] = true;

    // These are applied earlier to prevent issues with renamed monsters
    // and "<monster> comes into view" (see delay.cc:_monster_warning).
    //mons->flags |= mspec.extra_monster_flags;

    // Monsters with gods set by the spec aren't god gifts
    // unless they have the "god_gift" tag.  place_monster(),
    // by default, marks any monsters with gods as god gifts,
    // so unmark them here.
    if (mspec.god != GOD_NO_GOD && !mspec.god_gift)
        mons->flags &= ~MF_GOD_GIFT;

    if (mons->is_priest() && mons->god == GOD_NO_GOD)
        mons->god = GOD_NAMELESS;

    if (mons->type == MONS_DANCING_WEAPON)
    {
        item_def *wpn = mons->mslot_item(MSLOT_WEAPON);
        ASSERT(wpn);
        mons->ghost->init_dancing_weapon(*wpn, 180);
        mons->ghost_demon_init();
    }

    for (unsigned int i = 0; i < mspec.ench.size(); i++)
        mons->add_ench(mspec.ench[i]);

    return mons;
}

static bool _dgn_place_monster(const vault_placement &place, mons_spec &mspec,
                                int monster_level, const coord_def& where)
{
    const bool generate_awake
        = mspec.generate_awake || place.map.has_tag("generate_awake");

    const bool patrolling
        = mspec.patrolling || place.map.has_tag("patrolling");

    return dgn_place_monster(mspec, monster_level, where, false,
                             generate_awake, patrolling);
}

static bool _dgn_place_one_monster(const vault_placement &place,
                                    mons_list &mons, int monster_level,
                                    const coord_def& where)
{
    for (int i = 0, size = mons.size(); i < size; ++i)
    {
        mons_spec spec = mons.get_monster(i);
        if (_dgn_place_monster(place, spec, monster_level, where))
            return (true);
    }
    return (false);
}

/* "Oddball grids" are handled in _vault_grid. 'B' is arguably oddball, too, as
 * it depends on the place where the vault is. Maybe handling it here is not
 * such a good idea. */
static dungeon_feature_type _glyph_to_feat(int glyph,
                                           vault_placement *place = NULL)
{
    return ((glyph == 'x') ? DNGN_ROCK_WALL :
            (glyph == 'X') ? DNGN_PERMAROCK_WALL :
            (glyph == 'c') ? DNGN_STONE_WALL :
            (glyph == 'v') ? DNGN_METAL_WALL :
            (glyph == 'b') ? DNGN_GREEN_CRYSTAL_WALL :
            (glyph == 'a') ? DNGN_WAX_WALL :
            (glyph == 'm') ? DNGN_CLEAR_ROCK_WALL :
            (glyph == 'n') ? DNGN_CLEAR_STONE_WALL :
            (glyph == 'o') ? DNGN_CLEAR_PERMAROCK_WALL :
            (glyph == 't') ? DNGN_TREE :
            (glyph == '+') ? DNGN_CLOSED_DOOR :
            (glyph == '=') ? DNGN_SECRET_DOOR :
            (glyph == 'w') ? DNGN_DEEP_WATER :
            (glyph == 'W') ? DNGN_SHALLOW_WATER :
            (glyph == 'l') ? DNGN_LAVA :
            (glyph == '>') ? DNGN_ESCAPE_HATCH_DOWN :
            (glyph == '<') ? DNGN_ESCAPE_HATCH_UP :
            (glyph == '}') ? DNGN_STONE_STAIRS_DOWN_I :
            (glyph == '{') ? DNGN_STONE_STAIRS_UP_I :
            (glyph == ')') ? DNGN_STONE_STAIRS_DOWN_II :
            (glyph == '(') ? DNGN_STONE_STAIRS_UP_II :
            (glyph == ']') ? DNGN_STONE_STAIRS_DOWN_III :
            (glyph == '[') ? DNGN_STONE_STAIRS_UP_III :
            (glyph == 'A') ? DNGN_STONE_ARCH :
            (glyph == 'B') ? (place ? _pick_temple_altar(*place)
                                    : DNGN_ALTAR_ZIN) :
            (glyph == 'C') ? _pick_an_altar() :   // f(x) elsewhere {dlb}
            (glyph == 'I') ? DNGN_ORCISH_IDOL :
            (glyph == 'G') ? DNGN_GRANITE_STATUE :
            (glyph == 'T') ? DNGN_FOUNTAIN_BLUE :
            (glyph == 'U') ? DNGN_FOUNTAIN_SPARKLING :
            (glyph == 'V') ? DNGN_PERMADRY_FOUNTAIN :
            (glyph == 'Y') ? DNGN_FOUNTAIN_BLOOD :
            (glyph == '\0')? DNGN_ROCK_WALL
                           : DNGN_FLOOR); // includes everything else
}

dungeon_feature_type map_feature_at(map_def *map, const coord_def &c,
                                    int rawfeat)
{
    if (rawfeat == -1)
        rawfeat = map->glyph_at(c);

    if (rawfeat == ' ')
        return (NUM_FEATURES);

    keyed_mapspec *mapsp = map? map->mapspec_at(c) : NULL;
    if (mapsp)
    {
        feature_spec f = mapsp->get_feat();
        if (f.trap.get())
        {
            // f.feat == 1 means trap is generated known.
            if (f.feat == 1)
                return trap_category(static_cast<trap_type>(f.trap.get()->tr_type));
            else
                return (DNGN_UNDISCOVERED_TRAP);
        }
        else if (f.feat >= 0)
            return static_cast<dungeon_feature_type>(f.feat);
        else if (f.glyph >= 0)
            return map_feature_at(NULL, c, f.glyph);
        else if (f.shop.get())
            return (DNGN_ENTER_SHOP);

        return (DNGN_FLOOR);
    }

    return _glyph_to_feat(rawfeat);
}

static void _vault_grid_mapspec(vault_placement &place, const coord_def &where,
                                keyed_mapspec& mapsp)
{
    const feature_spec f = mapsp.get_feat();
    if (f.trap.get())
    {
        trap_spec* spec = f.trap.get();
        if (spec && spec->tr_type == TRAP_INDEPTH)
            place_specific_trap(where, random_trap_for_place(place.level_number));
        else if (spec)
            place_specific_trap(where, spec);

        // f.feat == 1 means trap is generated known.
        if (f.feat == 1)
            grd(where) = trap_category(spec->tr_type);
    }
    else if (f.feat >= 0)
        grd(where) = static_cast<dungeon_feature_type>(f.feat);
    else if (f.glyph >= 0)
        _vault_grid_glyph(place, where, f.glyph);
    else if (f.shop.get())
        place_spec_shop(place.level_number, where, f.shop.get());
    else
        grd(where) = DNGN_FLOOR;

    if (f.mimic > 0 && one_chance_in(f.mimic))
    {
        ASSERT(!feat_cannot_be_mimic(grd(where)));
        env.level_map_mask(where) |= MMT_MIMIC;
    }
    else if (f.no_mimic)
        env.level_map_mask(where) |= MMT_NO_MIMIC;

    mons_list &mons = mapsp.get_monsters();
    _dgn_place_one_monster(place, mons, place.level_number, where);

    item_list &items = mapsp.get_items();
    dgn_place_multiple_items(items, where, place.level_number);
}

static void _vault_grid_glyph(vault_placement &place, const coord_def& where,
                              int vgrid)
{
    // First, set base tile for grids {dlb}:
    if (vgrid != -1)
        grd(where) = _glyph_to_feat(vgrid, &place);

    if (feat_is_altar(grd(where))
        && is_unavailable_god(feat_altar_god(grd(where))))
    {
        grd(where) = DNGN_FLOOR;
    }

    // then, handle oddball grids {dlb}:
    switch (vgrid)
    {
    case '@':
    case '=':
    case '+':
        if (_map_feat_is_on_edge(place, where))
            place.exits.push_back(where);

        break;
    case '^':
        place_specific_trap(where, TRAP_RANDOM);
        break;
    case '~':
        place_specific_trap(where, random_trap_for_place(place.level_number));
        break;
    }

    // Then, handle grids that place "stuff" {dlb}:
    if (vgrid == '$' || vgrid == '%' || vgrid == '*' || vgrid == '|')
    {
        int item_made = NON_ITEM;
        object_class_type which_class = OBJ_RANDOM;
        uint8_t which_type = OBJ_RANDOM;
        int which_depth = place.level_number;
        int spec = 250;

        if (vgrid == '$')
        {
            which_class = OBJ_GOLD;
        }
        else if (vgrid == '|')
        {
            which_class = random_choose_weighted(
                            2, OBJ_WEAPONS,
                            1, OBJ_ARMOUR,
                            1, OBJ_JEWELLERY,
                            1, OBJ_BOOKS,
                            1, OBJ_STAVES,
                            1, OBJ_MISCELLANY,
                            0);
            which_depth = MAKE_GOOD_ITEM;
        }
        else if (vgrid == '*')
            which_depth = 5 + (place.level_number * 2);

        item_made = items(1, which_class, which_type, true,
                           which_depth, spec);

        if (item_made != NON_ITEM)
        {
            mitm[item_made].pos = where;
        }
    }

    // defghijk - items
    if (map_def::valid_item_array_glyph(vgrid))
    {
        int slot = map_def::item_array_glyph_to_slot(vgrid);
        _dgn_place_item_explicit(slot, where, place, place.level_number);
    }

    // Finally, handle grids that place monsters {dlb}:
    if (map_def::valid_monster_glyph(vgrid))
    {
        int monster_level;
        mons_spec monster_type_thing(RANDOM_MONSTER);

        monster_level = place.level_number;
        if (vgrid == '8')
            monster_level = 4 + (place.level_number * 2);
        else if (vgrid == '9')
            monster_level = 5 + place.level_number;

        if (monster_level > 30) // very high level monsters more common here
            monster_level = 30;

        if (vgrid != '8' && vgrid != '9' && vgrid != '0')
        {
            int slot = map_def::monster_array_glyph_to_slot(vgrid);
            monster_type_thing = place.map.mons.get_monster(slot);
            monster_type mt = static_cast<monster_type>(monster_type_thing.type);
            // Is a map for a specific place trying to place a unique which
            // somehow already got created?
            if (place.map.place.is_valid()
                && !invalid_monster_type(mt)
                && mons_is_unique(mt)
                && you.unique_creatures[mt])
            {
                mprf(MSGCH_ERROR, "ERROR: %s already generated somewhere "
                     "else; please file a bug report.",
                     mons_type_name(mt, DESC_THE).c_str());
                // Force it to be generated anyway.
                you.unique_creatures[mt] = false;
            }
        }

        _dgn_place_monster(place, monster_type_thing, monster_level, where);
    }
}

static void _vault_grid(vault_placement &place,
                        int vgrid,
                        const coord_def& where,
                        keyed_mapspec *mapsp)
{
    if (mapsp)
        _vault_grid_mapspec(place, where, *mapsp);
    else
        _vault_grid_glyph(place, where, vgrid);
}

// Currently only used for Slime: branch end
// where it will turn the stone walls into clear rock walls
// once the royal jelly has been killed.
bool seen_replace_feat(dungeon_feature_type old_feat,
                       dungeon_feature_type new_feat)
{
    ASSERT(old_feat != new_feat);

    coord_def p1(0, 0);
    coord_def p2(GXM - 1, GYM - 1);

    bool seen = false;
    for (rectangle_iterator ri(p1, p2); ri; ++ri)
    {
        if (grd(*ri) == old_feat)
        {
            grd(*ri) = new_feat;
            set_terrain_changed(*ri);
            if (you.see_cell(*ri))
                seen = true;
        }
    }

    return (seen);
}

void dgn_replace_area(int sx, int sy, int ex, int ey,
                      dungeon_feature_type replace,
                      dungeon_feature_type feature,
                      unsigned mmask, bool needs_update)
{
    dgn_replace_area(coord_def(sx, sy), coord_def(ex, ey),
                      replace, feature, mmask, needs_update);
}

void dgn_replace_area(const coord_def& p1, const coord_def& p2,
                       dungeon_feature_type replace,
                       dungeon_feature_type feature, uint32_t mapmask,
                       bool needs_update)
{
    for (rectangle_iterator ri(p1, p2); ri; ++ri)
    {
        if (grd(*ri) == replace && !map_masked(*ri, mapmask))
        {
            grd(*ri) = feature;
            if (needs_update && env.map_knowledge(*ri).seen())
            {
                env.map_knowledge(*ri).set_feature(feature, 0,
                                                   get_trap_type(*ri));
#ifdef USE_TILE
                env.tile_bk_bg(*ri) = feature;
#endif
            }
        }
    }
}

bool map_masked(const coord_def &c, unsigned mask)
{
    return (mask && (env.level_map_mask(c) & mask));
}

struct coord_comparator
{
    coord_def target;
    coord_comparator(const coord_def &t) : target(t) { }

    static int dist(const coord_def &a, const coord_def &b)
    {
        const coord_def del = a - b;
        return std::abs(del.x) * GYM + std::abs(del.y);
    }

    bool operator () (const coord_def &a, const coord_def &b) const
    {
        return dist(a, target) < dist(b, target);
    }
};

typedef std::set<coord_def, coord_comparator> coord_set;

static void _jtd_init_surrounds(coord_set &coords, uint32_t mapmask,
                                const coord_def &c)
{
    for (orth_adjacent_iterator ai(c); ai; ++ai)
    {
        if (!in_bounds(*ai) || travel_point_distance[ai->x][ai->y]
            || map_masked(*ai, mapmask))
        {
            continue;
        }
        coords.insert(*ai);

        const coord_def dp = *ai - c;
        travel_point_distance[ai->x][ai->y] = (-dp.x + 2) * 4 + (-dp.y + 2);
    }
}

static bool _join_the_dots_pathfind(coord_set &coords,
                                    const coord_def &from, const coord_def &to,
                                    uint32_t mapmask)
{
    coord_def curr = from;
    while (true)
    {
        int &tpd = travel_point_distance[curr.x][curr.y];
        tpd = !tpd? -1000 : -tpd;

        if (curr == to)
            break;

        _jtd_init_surrounds(coords, mapmask, curr);

        if (coords.empty())
            break;

        curr = *coords.begin();
        coords.erase(coords.begin());
    }

    if (curr != to)
        return (false);

    while (curr != from)
    {
        if (!map_masked(curr, mapmask))
            grd(curr) = DNGN_FLOOR;

        const int dist = travel_point_distance[curr.x][curr.y];
        ASSERT(dist < 0 && dist != -1000);
        curr += coord_def(-dist / 4 - 2, (-dist % 4) - 2);
    }
    if (!map_masked(curr, mapmask))
        grd(curr) = DNGN_FLOOR;

    return (true);
}

bool join_the_dots(const coord_def &from, const coord_def &to,
                           uint32_t mapmask)
{
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));

    const coord_comparator comp(to);
    coord_set coords(comp);
    const bool found = _join_the_dots_pathfind(coords, from, to, mapmask);

    return (found);
}

static dungeon_feature_type _pick_temple_altar(vault_placement &place)
{
    if (_temple_altar_list.empty())
    {
        if (_current_temple_hash != NULL)
        {
            mpr("Ran out of altars for temple!", MSGCH_ERROR);
            return (DNGN_FLOOR);
        }
        // Randomized altar list for mini-temples.
        _temple_altar_list = temple_god_list();
        std::random_shuffle(_temple_altar_list.begin(), _temple_altar_list.end());
    }

    const god_type god = _temple_altar_list.back();

    _temple_altar_list.pop_back();

    return altar_for_god(god);
}

//jmf: Generate altar based on where you are, or possibly randomly.
static dungeon_feature_type _pick_an_altar()
{
    god_type god;
    int temp_rand;              // probability determination {dlb}

    if (player_in_branch(BRANCH_ECUMENICAL_TEMPLE)
        || you.level_type == LEVEL_LABYRINTH)
    {
        // No extra altars in Temple, none at all in Labyrinth.
        god = GOD_NO_GOD;
    }
    else if (you.level_type == LEVEL_DUNGEON && !one_chance_in(5))
    {
        switch (you.where_are_you)
        {
        case BRANCH_CRYPT:
            god = (coinflip() ? GOD_KIKUBAAQUDGHA
                              : GOD_YREDELEMNUL);
            break;

        case BRANCH_DWARVEN_HALL:
            temp_rand = random2(7);

            god = ((temp_rand == 0) ? GOD_KIKUBAAQUDGHA :
                   (temp_rand == 1) ? GOD_YREDELEMNUL :
                   (temp_rand == 2) ? GOD_MAKHLEB :
                   (temp_rand == 3) ? GOD_TROG :
                   (temp_rand == 4) ? GOD_CHEIBRIADOS:
                   (temp_rand == 5) ? GOD_ELYVILON
                                    : GOD_OKAWARU);
            break;

        case BRANCH_ORCISH_MINES:    // violent gods
            temp_rand = random2(10); // 50% chance of Beogh

            god = ((temp_rand == 0) ? GOD_VEHUMET :
                   (temp_rand == 1) ? GOD_MAKHLEB :
                   (temp_rand == 2) ? GOD_OKAWARU :
                   (temp_rand == 3) ? GOD_TROG :
                   (temp_rand == 4) ? GOD_XOM
                                    : GOD_BEOGH);
            break;

        case BRANCH_VAULTS: // "lawful" gods
            temp_rand = random2(7);

            god = ((temp_rand == 0) ? GOD_ELYVILON :
                   (temp_rand == 1) ? GOD_SIF_MUNA :
                   (temp_rand == 2) ? GOD_SHINING_ONE :
                   (temp_rand == 3
                       || temp_rand == 4) ? GOD_OKAWARU
                                          : GOD_ZIN);
            break;

        case BRANCH_HALL_OF_BLADES:
            god = GOD_OKAWARU;
            break;

        case BRANCH_ELVEN_HALLS:    // "magic" gods
            temp_rand = random2(4);

            god = ((temp_rand == 0) ? GOD_VEHUMET :
                   (temp_rand == 1) ? GOD_SIF_MUNA :
                   (temp_rand == 2) ? GOD_XOM
                                    : GOD_MAKHLEB);
            break;

        case BRANCH_SLIME_PITS:
            god = GOD_JIYVA;
            break;

        case BRANCH_TOMB:
            god = GOD_KIKUBAAQUDGHA;
            break;

        default:
            do
                god = random_god(true);
            while (god == GOD_NEMELEX_XOBEH
                   || god == GOD_LUGONU
                   || god == GOD_BEOGH
                   || god == GOD_JIYVA);
            break;
        }
    }
    else
    {
        // Note: this case includes Pandemonium or the Abyss.
        temp_rand = random2(9);

        god = ((temp_rand == 0) ? GOD_ZIN :
               (temp_rand == 1) ? GOD_SHINING_ONE :
               (temp_rand == 2) ? GOD_KIKUBAAQUDGHA :
               (temp_rand == 3) ? GOD_XOM :
               (temp_rand == 4) ? GOD_OKAWARU :
               (temp_rand == 5) ? GOD_MAKHLEB :
               (temp_rand == 6) ? GOD_SIF_MUNA :
               (temp_rand == 7) ? GOD_TROG
                                : GOD_ELYVILON);
    }

    if (is_unavailable_god(god))
        god = GOD_NO_GOD;

    return (altar_for_god(god));
}

static bool _need_varied_selection(shop_type shop)
{
    return (shop == SHOP_BOOK);
}

void place_spec_shop(int level_number,
                      const coord_def& where,
                      int force_s_type, bool representative)
{
    shop_spec spec(static_cast<shop_type>(force_s_type));
    place_spec_shop (level_number, where, &spec, representative);
}

void place_spec_shop(int level_number,
                      const coord_def& where,
                      shop_spec* spec, bool representative)
{
    int force_s_type = static_cast<int>(spec->sh_type);

    int orb = 0;
    int i = 0;
    int j = 0;                  // loop variable
    int item_level;

    bool note_status = notes_are_active();
    activate_notes(false);

    for (i = 0; i < MAX_SHOPS; i++)
        if (env.shop[i].type == SHOP_UNASSIGNED)
            break;

    if (i == MAX_SHOPS)
        return;

    for (j = 0; j < 3; j++)
        env.shop[i].keeper_name[j] = 1 + random2(200);

    if (!spec->name.empty())
        env.shop[i].shop_name = spec->name;

    if (!spec->type.empty())
        env.shop[i].shop_type_name = spec->type;

    if (!spec->suffix.empty())
        env.shop[i].shop_suffix_name = spec->suffix;

    env.shop[i].level = level_number * 2;

    env.shop[i].type = static_cast<shop_type>(
        (force_s_type != SHOP_RANDOM) ? force_s_type
                                      : random2(NUM_SHOPS));

    if (env.shop[i].type == SHOP_FOOD)
    {
        env.shop[i].greed = 10 + random2(5);
    }
    else if (env.shop[i].type != SHOP_WEAPON_ANTIQUE
             && env.shop[i].type != SHOP_ARMOUR_ANTIQUE
             && env.shop[i].type != SHOP_GENERAL_ANTIQUE)
    {
        env.shop[i].greed = 10 + random2(5) + random2(level_number / 2);
    }
    else
        env.shop[i].greed = 15 + random2avg(19, 2) + random2(level_number);

    // Allow bargains in bazaars, prices randomly between 60% and 95%.
    if (you.level_type == LEVEL_PORTAL_VAULT && you.level_type_tag == "bazaar")
    {
        // Need to calculate with factor as greed (uint8_t)
        // is capped at 255.
        int factor = random2(8) + 12;

        dprf("Shop type %d: original greed = %d, factor = %d, discount = %d%%.",
             env.shop[i].type, env.shop[i].greed, factor, (20-factor)*5);

        factor *= env.shop[i].greed;
        factor /= 20;
        env.shop[i].greed = factor;
    }

    if (spec->greed != -1)
    {
        dprf("Shop spec overrides greed: %d becomes %d.", env.shop[i].greed, spec->greed);
        env.shop[i].greed = spec->greed;
    }

    int plojy = 5 + random2avg(12, 3);
    if (representative)
        plojy = env.shop[i].type == SHOP_WAND? NUM_WANDS : 16;

    if (spec->use_all && !spec->items.empty())
    {
        dprf("Shop spec wants all items placed: %d becomes %u.", plojy,
             (unsigned int)spec->items.size());
        plojy = (int) spec->items.size();
    }

    if (spec->num_items != -1)
    {
        dprf("Shop spec overrides number of items: %d becomes %d.", plojy, spec->num_items);
        plojy = spec->num_items;
    }

    // For books shops, store how many copies of a given book are on display.
    // This increases the diversity of books in a shop.
    int stocked[NUM_BOOKS];
    if (_need_varied_selection(env.shop[i].type))
    {
        for (int k = 0; k < NUM_BOOKS; k++)
             stocked[k] = 0;
    }

    coord_def stock_loc = coord_def(0, 5+i);

    for (j = 0; j < plojy; j++)
    {
        if (env.shop[i].type != SHOP_WEAPON_ANTIQUE
            && env.shop[i].type != SHOP_ARMOUR_ANTIQUE
            && env.shop[i].type != SHOP_GENERAL_ANTIQUE)
        {
            item_level = level_number + random2((level_number + 1) * 2);
        }
        else
        {
            item_level = level_number + random2((level_number + 1) * 3);
        }

        // Make bazaar items more valuable (up to double value).
        if (you.level_type == LEVEL_PORTAL_VAULT
            && you.level_type_tag == "bazaar")
        {
            int help = random2(item_level) + 1;
            item_level += help;

            if (item_level > level_number * 5)
                item_level = level_number * 5;
        }

        // Don't generate gold in shops! This used to be possible with
        // general stores (see item_in_shop() below)   (GDL)
        while (true)
        {
            const int subtype = representative? j : OBJ_RANDOM;

            if (!spec->items.empty() && !spec->use_all)
            {
                orb = dgn_place_item(spec->items.random_item_weighted(),
                        stock_loc, item_level);
            }
            else if (!spec->items.empty() && spec->use_all && j < (int)spec->items.size())
            {
                orb = dgn_place_item(spec->items.get_item(j), stock_loc, item_level);
            }
            else
            {
                orb = items(1, _item_in_shop(env.shop[i].type), subtype, true,
                             one_chance_in(4)? MAKE_GOOD_ITEM : item_level,
                             MAKE_ITEM_RANDOM_RACE);
            }

            // Try for a better selection.
            if (orb != NON_ITEM && _need_varied_selection(env.shop[i].type))
            {
                if (!one_chance_in(stocked[mitm[orb].sub_type] + 1))
                {
                    mitm[orb].clear();
                    orb = NON_ITEM; // try again
                }
            }

            if (orb != NON_ITEM
                && mitm[orb].base_type != OBJ_GOLD
                && (env.shop[i].type != SHOP_GENERAL_ANTIQUE
                    || (mitm[orb].base_type != OBJ_MISSILES
                        && mitm[orb].base_type != OBJ_FOOD)
                    || !spec->items.empty()))
            {
                break;
            }

            // Reset object and try again.
            if (orb != NON_ITEM)
                mitm[orb].clear();
        }

        if (orb == NON_ITEM)
            break;

        item_def& item(mitm[orb]);

        // Increase stock of this subtype by 1, unless it is an artefact
        // (allow for several artefacts of the same underlying subtype)
        // - the latter is currently unused but would apply to e.g. jewellery.
        if (_need_varied_selection(env.shop[i].type) && !is_artefact(item))
            stocked[item.sub_type]++;

        if (representative && item.base_type == OBJ_WANDS)
            item.plus = 7;

        // Set object 'position' (gah!) & ID status.
        item.pos = stock_loc;

        if (env.shop[i].type != SHOP_WEAPON_ANTIQUE
            && env.shop[i].type != SHOP_ARMOUR_ANTIQUE
            && env.shop[i].type != SHOP_GENERAL_ANTIQUE)
        {
            set_ident_flags(item, ISFLAG_IDENT_MASK);
        }
    }

    env.shop[i].pos = where;
    env.tgrid(where) = i;

    grd(where) = DNGN_ENTER_SHOP;

    activate_notes(note_status);
}

static object_class_type _item_in_shop(shop_type shop_type)
{
    switch (shop_type)
    {
    case SHOP_WEAPON:
        if (one_chance_in(5))
            return (OBJ_MISSILES);
        // *** deliberate fall through here  {dlb} ***
    case SHOP_WEAPON_ANTIQUE:
        return (OBJ_WEAPONS);

    case SHOP_ARMOUR:
    case SHOP_ARMOUR_ANTIQUE:
        return (OBJ_ARMOUR);

    case SHOP_GENERAL:
    case SHOP_GENERAL_ANTIQUE:
        return (OBJ_RANDOM);

    case SHOP_JEWELLERY:
        return (OBJ_JEWELLERY);

    case SHOP_WAND:
        return (OBJ_WANDS);

    case SHOP_BOOK:
        return (OBJ_BOOKS);

    case SHOP_FOOD:
        return (OBJ_FOOD);

    case SHOP_DISTILLERY:
        return (OBJ_POTIONS);

    case SHOP_SCROLL:
        return (OBJ_SCROLLS);

    default:
        die("unknown shop type");
    }

    return (OBJ_RANDOM);
}

// Keep seeds away from the borders so we don't end up with a
// straight wall.
static bool _spotty_seed_ok(const coord_def& p)
{
    const int margin = 4;
    return (p.x >= margin && p.y >= margin
            && p.x < GXM - margin && p.y < GYM - margin);
}

// Connect vault exit "from" to dungeon floor by growing a spotty chamber.
// This tries to be like _spotty_level, but probably isn't quite.
// It might be better to aim for a more open connection -- currently
// it stops pretty much as soon as connectivity is attained.
static bool _connect_spotty(const coord_def& from)
{
    std::set<coord_def> flatten;
    std::set<coord_def> border;
    std::set<coord_def>::const_iterator it;
    bool success = false;

    for (adjacent_iterator ai(from); ai; ++ai)
        if (!map_masked(*ai, MMT_VAULT) && _spotty_seed_ok(*ai))
            border.insert(*ai);

    while (!success && !border.empty())
    {
        coord_def cur;
        int count = 0;
        for (it = border.begin(); it != border.end(); ++it)
            if (one_chance_in(++count))
                cur = *it;
        border.erase(border.find(cur));

        // Flatten orthogonal neighbours, and add new neighbours to border.
        flatten.insert(cur);
        for (orth_adjacent_iterator ai(cur); ai; ++ai)
        {
            if (map_masked(*ai, MMT_VAULT))
                continue;

            if (grd(*ai) == DNGN_FLOOR)
                success = true; // Through, but let's remove the others, too.

            if (grd(*ai) != DNGN_ROCK_WALL
                || flatten.find(*ai) != flatten.end())
            {
                continue;
            }

            flatten.insert(*ai);
            for (adjacent_iterator bi(*ai); bi; ++bi)
            {
                if (!map_masked(*bi, MMT_VAULT)
                    && _spotty_seed_ok(*bi)
                    && flatten.find(*bi) == flatten.end())
                {
                    border.insert(*bi);
                }
            }
        }
    }

    if (success)
        for (it = flatten.begin(); it != flatten.end(); ++it)
            grd(*it) = DNGN_FLOOR;

    return (success);
}

bool place_specific_trap(const coord_def& where, trap_type spec_type)
{
    trap_spec spec(spec_type);

    return place_specific_trap(where, &spec);
}

bool place_specific_trap(const coord_def& where, trap_spec* spec)
{
    trap_type spec_type = spec->tr_type;

    if (spec_type == TRAP_RANDOM || spec_type == TRAP_NONTELEPORT
        || spec_type == TRAP_SHAFT && !is_valid_shaft_level())
    {
        trap_type forbidden1 = NUM_TRAPS;
        trap_type forbidden2 = NUM_TRAPS;

        if (spec_type == TRAP_NONTELEPORT)
        {
            forbidden1 = TRAP_SHAFT;
            forbidden2 = TRAP_TELEPORT;
        }
        else if (!is_valid_shaft_level())
            forbidden1 = TRAP_SHAFT;

        do
            spec_type = static_cast<trap_type>(random2(NUM_TRAPS));
        while (spec_type == forbidden1 || spec_type == forbidden2 ||
               spec_type == TRAP_GOLUBRIA || spec_type == TRAP_PLATE);
    }

    for (int tcount = 0; tcount < MAX_TRAPS; tcount++)
        if (env.trap[tcount].type == TRAP_UNASSIGNED)
        {
            env.trap[tcount].type = spec_type;
            env.trap[tcount].pos  = where;
            grd(where)            = DNGN_UNDISCOVERED_TRAP;
            env.tgrid(where)      = tcount;
            env.trap[tcount].prepare_ammo();
            return (true);
        }

    return (false);
}

static void _add_plant_clumps(int frequency /* = 10 */,
                              int clump_density /* = 12 */,
                              int clump_radius /* = 4 */)
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        mgen_data mg;
        if (mgrd(*ri) != NON_MONSTER)
        {
            /* clump plants around things that already exist */
            monster_type type = menv[mgrd(*ri)].type;
            if ((type == MONS_PLANT  ||
                 type == MONS_FUNGUS ||
                 type == MONS_BUSH) && one_chance_in(frequency))
                 {
                mg.cls = type;
            }
            else
            {
                continue;
            }
        }
        else
        {
            continue;
        }

        std::vector<coord_def> to_place;
        to_place.push_back(*ri);
        for (int i = 1; i < clump_radius; ++i)
        {
            for (radius_iterator rad(*ri, i, C_SQUARE); rad; ++rad)
            {
                if (grd(*rad) != DNGN_FLOOR)
                    continue;

                /* make sure the iterator stays valid */
                std::vector<coord_def> more_to_place;
                for (std::vector<coord_def>::const_iterator it = to_place.begin();
                     it != to_place.end();
                     ++it)
                {
                    if (*rad == *it)
                        continue;
                    /* only place plants next to previously placed plants */
                    if (abs(rad->x - it->x) <= 1 && abs(rad->y - it->y) <= 1)
                    {
                        if (one_chance_in(clump_density))
                            more_to_place.push_back(*rad);
                    }
                }
                to_place.insert(to_place.end(), more_to_place.begin(), more_to_place.end());
            }
        }

        for (std::vector<coord_def>::const_iterator it = to_place.begin();
             it != to_place.end();
             ++it)
        {
            if (*it == *ri)
                continue;
            mg.pos = *it;
            mons_place(mgen_data(mg));
        }
    }
}

struct nearest_point
{
    coord_def target;
    coord_def nearest;
    int       distance;

    nearest_point(const coord_def &t) : target(t), nearest(), distance(-1)
    {
    }
    void operator () (const coord_def &c)
    {
        if (grd(c) == DNGN_FLOOR)
        {
            const int ndist = (c - target).abs();
            if (distance == -1 || ndist < distance)
            {
                distance = ndist;
                nearest  = c;
            }
        }
    }
};

// Fill travel_point_distance out from all stone stairs on the level.
static coord_def _dgn_find_closest_to_stone_stairs(coord_def base_pos)
{
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));
    nearest_point np(base_pos);
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (!travel_point_distance[ri->x][ri->y]
            && feat_is_stone_stair(grd(*ri)) && !feature_mimic_at(*ri))
        {
            _dgn_fill_zone(*ri, 1, np, dgn_square_travel_ok);
        }
    }

    return (np.nearest);
}


double dgn_degrees_to_radians(int degrees)
{
    return degrees * M_PI / 180;
}

coord_def dgn_random_point_from(const coord_def &c, int radius, int margin)
{
    int attempts = 70;
    while (attempts-- > 0)
    {
        const double angle = dgn_degrees_to_radians(random2(360));
        const coord_def res =
            c + coord_def(static_cast<int>(radius * cos(angle)),
                          static_cast<int>(radius * sin(angle)));
        if (map_bounds_with_margin(res, margin))
        {
            return res;
        }
    }
    return coord_def();
}

coord_def dgn_find_feature_marker(dungeon_feature_type feat)
{
    std::vector<map_marker*> markers = env.markers.get_all();
    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        map_marker *mark = markers[i];
        if (mark->get_type() == MAT_FEATURE
            && dynamic_cast<map_feature_marker*>(mark)->feat == feat)
        {
            return (mark->pos);
        }
    }
    coord_def unfound;
    return (unfound);
}

static coord_def _dgn_find_labyrinth_entry_point()
{
    return (dgn_find_feature_marker(DNGN_ENTER_LABYRINTH));
}

// Make hatches and shafts land the player a bit away from the wall.
// Specifically, the adjacent cell with least slime walls next to it.
// XXX: This can still give bad situations if the layout is not bubbly,
//      e.g. when a vault is placed with connecting corridors.
static void _fixup_slime_hatch_dest(coord_def* pos)
{
    int max_walls = 9;
    for (adjacent_iterator ai(*pos, false); ai; ++ai)
    {
        if (!feat_is_traversable(env.grid(*ai)))
            continue;
        int walls = 0;
        for (adjacent_iterator bi(*ai); bi && walls < max_walls; ++bi)
            if (env.grid(*bi) == DNGN_SLIMY_WALL)
                walls++;
        if (walls < max_walls)
        {
            *pos = *ai;
            max_walls = walls;
        }
    }
    ASSERT(max_walls < 9);
}

coord_def dgn_find_nearby_stair(dungeon_feature_type stair_to_find,
                                coord_def base_pos, bool find_closest)
{
    dprf("Level entry point on %sstair: %d (%s)",
         find_closest ? "closest " : "",
         stair_to_find, dungeon_feature_name(stair_to_find));

    if (stair_to_find == DNGN_EXIT_PORTAL_VAULT)
    {
        const coord_def pos(dgn_find_feature_marker(stair_to_find));
        if (in_bounds(pos))
        {
            if (map_marker *marker = env.markers.find(pos, MAT_FEATURE))
                env.markers.remove(marker);
            return (pos);
        }

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_WARN, "Ouch, no portal vault exit point!");
#endif

        stair_to_find = DNGN_FLOOR;
    }

    // Shafts and hatches.
    if (stair_to_find == DNGN_ESCAPE_HATCH_UP
        || stair_to_find == DNGN_ESCAPE_HATCH_DOWN)
    {
        coord_def pos(_dgn_find_closest_to_stone_stairs(base_pos));
        if (player_in_branch(BRANCH_SLIME_PITS))
            _fixup_slime_hatch_dest(&pos);
        if (in_bounds(pos))
            return (pos);
    }

    if (stair_to_find == DNGN_STONE_ARCH)
    {
        const coord_def pos(dgn_find_feature_marker(stair_to_find));
        if (in_bounds(pos) && grd(pos) == stair_to_find)
            return (pos);
    }

    if (stair_to_find == DNGN_ENTER_LABYRINTH)
    {
        const coord_def pos(_dgn_find_labyrinth_entry_point());
        if (in_bounds(pos))
            return (pos);

        // Couldn't find a good place, warn, and use old behaviour.
        dprf("Oops, couldn't find labyrinth entry marker.");
        stair_to_find = DNGN_FLOOR;
    }

    if (stair_to_find == your_branch().exit_stairs)
    {
        const coord_def pos(dgn_find_feature_marker(DNGN_STONE_STAIRS_UP_I));
        if (in_bounds(pos) && grd(pos) == stair_to_find)
            return (pos);
    }

    // Scan around the player's position first.
    int basex = base_pos.x;
    int basey = base_pos.y;

    // Check for illegal starting point.
    if (!in_bounds(basex, basey))
    {
        basex = 0;
        basey = 0;
    }

    coord_def result;

    int found = 0;
    int best_dist = 1 + GXM*GXM + GYM*GYM;

    // XXX These passes should be rewritten to use an iterator of STL
    // algorithm of some kind.

    // First pass: look for an exact match.
    for (int xcode = 0; xcode < GXM; ++xcode)
    {
        if (stair_to_find == DNGN_FLOOR)
            break;

        const int xsign = ((xcode % 2) ? 1 : -1);
        const int xdiff = xsign * (xcode + 1)/2;
        const int xpos  = (basex + xdiff + GXM) % GXM;

        for (int ycode = 0; ycode < GYM; ++ycode)
        {
            const int ysign = ((ycode % 2) ? 1 : -1);
            const int ydiff = ysign * (ycode + 1)/2;
            const int ypos  = (basey + ydiff + GYM) % GYM;

            // Note that due to the wrapping above, we can't just use
            // xdiff*xdiff + ydiff*ydiff.
            const int dist = (xpos-basex)*(xpos-basex)
                             + (ypos-basey)*(ypos-basey);

            if (grd[xpos][ypos] == stair_to_find
                && !feature_mimic_at(coord_def(xpos, ypos)))
            {
                found++;
                if (find_closest)
                {
                    if (dist < best_dist)
                    {
                        best_dist = dist;
                        result.x = xpos;
                        result.y = ypos;
                    }
                }
                else if (one_chance_in(found))
                {
                    result.x = xpos;
                    result.y = ypos;
                }
            }
        }
    }

    if (found)
        return result;

    best_dist = 1 + GXM*GXM + GYM*GYM;

    // Second pass: find a staircase in the proper direction.
    for (int xcode = 0; xcode < GXM; ++xcode)
    {
        if (stair_to_find == DNGN_FLOOR)
            break;

        const int xsign = ((xcode % 2) ? 1 : -1);
        const int xdiff = xsign * (xcode + 1)/2;
        const int xpos  = (basex + xdiff + GXM) % GXM;

        for (int ycode = 0; ycode < GYM; ++ycode)
        {
            const int ysign = ((ycode % 2) ? 1 : -1);
            const int ydiff = ysign * (ycode + 1)/2;
            const int ypos  = (basey + ydiff + GYM) % GYM;

            bool good_stair;
            const int looking_at = grd[xpos][ypos];

            if (stair_to_find <= DNGN_ESCAPE_HATCH_DOWN)
            {
                good_stair = (looking_at >= DNGN_STONE_STAIRS_DOWN_I
                              && looking_at <= DNGN_ESCAPE_HATCH_DOWN);
            }
            else
            {
                good_stair =  (looking_at >= DNGN_STONE_STAIRS_UP_I
                               && looking_at <= DNGN_ESCAPE_HATCH_UP);
            }

            const int dist = (xpos-basex)*(xpos-basex)
                             + (ypos-basey)*(ypos-basey);

            if (good_stair && !feature_mimic_at(coord_def(xpos, ypos)))
            {
                found++;
                if (find_closest && dist < best_dist)
                {
                    best_dist = dist;
                    result.x = xpos;
                    result.y = ypos;
                }
                else if (one_chance_in(found))
                {
                    result.x = xpos;
                    result.y = ypos;
                }
            }
        }
    }

    if (found)
        return result;

    // Look for marker with the desired feature. This is used by zigs.
    const coord_def pos(dgn_find_feature_marker(stair_to_find));
    if (in_bounds(pos))
        return (pos);

    // Still hosed? If we're in a portal vault, convert to a search for
    // any stone arch.
    if (you.level_type == LEVEL_PORTAL_VAULT
        && stair_to_find != DNGN_STONE_ARCH)
    {
        return dgn_find_nearby_stair(DNGN_STONE_ARCH, base_pos, find_closest);
    }

    // Look for any clear terrain and abandon the idea of looking
    // nearby now. This is used when taking transit Pandemonium gates,
    // or landing in Labyrinths. Never land the PC inside a Pan or Lab
    // vault.
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (grd(*ri) >= DNGN_FLOOR)
        {
            found++;
            if (one_chance_in(found))
                result = *ri;
        }
    }
    if (found)
        return result;

    // FAIL
    die("Can't find any floor to put the player on.");
}

void dgn_set_lt_callback(std::string level_type_tag,
                         std::string callback_name)
{
    ASSERT(!level_type_tag.empty());
    ASSERT(!callback_name.empty());

    level_type_post_callbacks[level_type_tag] = callback_name;
}

////////////////////////////////////////////////////////////////////
// dgn_region

bool dgn_region::overlaps(const dgn_region &other) const
{
    // The old overlap check checked only two corners - top-left and
    // bottom-right. I'm hoping nothing actually *relied* on that stupid bug.

    return (between(pos.x, other.pos.x, other.pos.x + other.size.x - 1)
               || between(pos.x + size.x - 1, other.pos.x,
                          other.pos.x + other.size.x - 1))
            && (between(pos.y, other.pos.y, other.pos.y + other.size.y - 1)
                || between(pos.y + size.y - 1, other.pos.y,
                           other.pos.y + other.size.y - 1));
}

bool dgn_region::overlaps_any(const dgn_region_list &regions) const
{
    for (dgn_region_list::const_iterator i = regions.begin();
         i != regions.end(); ++i)
    {
        if (overlaps(*i))
            return (true);
    }
    return (false);
}

bool dgn_region::overlaps(const dgn_region_list &regions,
                          const map_mask &mask) const
{
    return overlaps_any(regions) && overlaps(mask);
}

bool dgn_region::overlaps(const map_mask &mask) const
{
    const coord_def endp = pos + size;
    for (int y = pos.y; y < endp.y; ++y)
        for (int x = pos.x; x < endp.x; ++x)
        {
            if (mask[x][y])
                return (true);
        }

    return (false);
}

coord_def dgn_region::random_edge_point() const
{
    return x_chance_in_y(size.x, size.x + size.y) ?
                  coord_def(pos.x + random2(size.x),
                             coinflip()? pos.y : pos.y + size.y - 1)
                : coord_def(coinflip()? pos.x : pos.x + size.x - 1,
                             pos.y + random2(size.y));
}

coord_def dgn_region::random_point() const
{
    return coord_def(pos.x + random2(size.x), pos.y + random2(size.y));
}

struct StairConnectivity
{
    StairConnectivity()
    {
        region[0] = region[1] = region[2] = 0;
        connected[0] = connected[1] = connected[2] = true;
    }

    void connect_region(int idx)
    {
        for (int i = 0; i < 3; i++)
            connected[i] |= (region[i] == idx);
    }

    void read(reader &th)
    {
        region[0] = unmarshallByte(th);
        region[1] = unmarshallByte(th);
        region[2] = unmarshallByte(th);
        connected[0] = unmarshallBoolean(th);
        connected[1] = unmarshallBoolean(th);
        connected[2] = unmarshallBoolean(th);
    }

    void write(writer &th)
    {
        marshallByte(th, region[0]);
        marshallByte(th, region[1]);
        marshallByte(th, region[2]);
        marshallBoolean(th, connected[0]);
        marshallBoolean(th, connected[1]);
        marshallBoolean(th, connected[2]);
    }

    int8_t region[3];
    bool connected[3];
};

FixedVector<std::vector<StairConnectivity>, NUM_BRANCHES> connectivity;

void init_level_connectivity()
{
    for (int i = 0; i < NUM_BRANCHES; i++)
    {
        int depth = branches[i].depth > 0 ? branches[i].depth : 0;
        connectivity[i].resize(depth);
    }
}

void read_level_connectivity(reader &th)
{
#if TAG_MAJOR_VERSION == 32
    int nb = 23;
    if (th.getMinorVersion() >= TAG_MINOR_NUM_LEVEL_CONN)
        nb = unmarshallInt(th);
#else
    int nb = unmarshallInt(th);
#endif
    for (int i = 0; i < nb; i++)
    {
        unsigned int depth = branches[i].depth > 0 ? branches[i].depth : 0;
        unsigned int num_entries = unmarshallInt(th);
        connectivity[i].resize(std::max(depth, num_entries));

        for (unsigned int e = 0; e < num_entries; e++)
            connectivity[i][e].read(th);
    }
}

void write_level_connectivity(writer &th)
{
    marshallInt(th, NUM_BRANCHES);
    for (int i = 0; i < NUM_BRANCHES; i++)
    {
        marshallInt(th, connectivity[i].size());
        for (unsigned int e = 0; e < connectivity[i].size(); e++)
            connectivity[i][e].write(th);
    }
}

static bool _fixup_interlevel_connectivity()
{
    // Rotate the stairs on this level to attempt to preserve connectivity
    // as much as possible.  At a minimum, it ensures a path from the bottom
    // of a branch to the top of a branch.  If this is not possible, it
    // returns false.
    //
    // Note: this check is undirectional and assumes that levels below this
    // one have not been created yet.  If this is not the case, it will not
    // guarantee or preserve connectivity.
    //
    // XXX: If successful, the previous level's connectedness information
    //      is updated, so we rely on the level not being vetoed after
    //      this check.

    if (you.level_type != LEVEL_DUNGEON || your_branch().depth == -1)
        return (true);
    if (branches[you.where_are_you].branch_flags & BFLAG_ISLANDED)
        return (true);

    StairConnectivity prev_con;
    if (player_branch_depth() > 1)
        prev_con = connectivity[your_branch().id][player_branch_depth() - 2];
    StairConnectivity this_con;

    FixedVector<coord_def, 3> up_gc;
    FixedVector<coord_def, 3> down_gc;
    FixedVector<int, 3> up_region;
    FixedVector<int, 3> down_region;
    FixedVector<bool, 3> has_down;
    std::vector<bool> region_connected;

    up_region[0] = up_region[1] = up_region[2] = -1;
    down_region[0] = down_region[1] = down_region[2] = -1;
    has_down[0] = has_down[1] = has_down[2] = false;

    // Find up stairs and down stairs on the current level.
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));
    int nzones = 0;
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (!map_bounds(ri->x, ri->y)
            || travel_point_distance[ri->x][ri->y]
            || !dgn_square_travel_ok(*ri))
        {
            continue;
        }

        _dgn_fill_zone(*ri, ++nzones, _dgn_point_record_stub,
                       dgn_square_travel_ok, NULL);
    }

    int max_region = 0;
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (feature_mimic_at(*ri))
            continue;

        dungeon_feature_type feat = grd(*ri);
        switch (feat)
        {
        case DNGN_STONE_STAIRS_DOWN_I:
        case DNGN_STONE_STAIRS_DOWN_II:
        case DNGN_STONE_STAIRS_DOWN_III:
        {
            int idx = feat - DNGN_STONE_STAIRS_DOWN_I;
            if (down_region[idx] == -1)
            {
                down_region[idx] = travel_point_distance[ri->x][ri->y];
                down_gc[idx] = *ri;
                max_region = std::max(down_region[idx], max_region);
            }
            else
            {
                // Too many stairs!
                return (false);
            }
            break;
        }
        case DNGN_STONE_STAIRS_UP_I:
        case DNGN_STONE_STAIRS_UP_II:
        case DNGN_STONE_STAIRS_UP_III:
        {
            int idx = feat - DNGN_STONE_STAIRS_UP_I;
            if (up_region[idx] == -1)
            {
                up_region[idx] = travel_point_distance[ri->x][ri->y];
                up_gc[idx] = *ri;
                max_region = std::max(up_region[idx], max_region);
            }
            else
            {
                // Too many stairs!
                return (false);
            }
            break;
        }
        default:
            break;
        }
    }

    // Ensure all up stairs were found.
    for (int i = 0; i < 3; i++)
        if (up_region[i] == -1)
            return (false);

    region_connected.resize(max_region + 1);
    for (unsigned int i = 0; i < region_connected.size(); i++)
        region_connected[i] = false;

    // Which up stairs have a down stair? (These are potentially connected.)
    if (!at_branch_bottom())
    {
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
            {
                if (down_region[j] == up_region[i])
                    has_down[i] = true;
            }
    }

    bool any_connected = has_down[0] || has_down[1] || has_down[2];
    if (!any_connected && !at_branch_bottom())
        return (false);

    // Keep track of what stairs we've assigned.
    int assign_prev[3] = {-1, -1, -1};
    int assign_cur[3] = {-1, -1, -1};

    // Assign one connected down stair from the previous level to an
    // upstair on the current level with a downstair in the same region.
    // This ensures at least a single valid path to the top.
    bool minimal_connectivity = false;
    for (int i = 0; i < 3 && !minimal_connectivity; i++)
    {
        if (!prev_con.connected[i])
            continue;
        for (int j = 0; j < 3; j++)
        {
            if (!has_down[j] && !at_branch_bottom())
                continue;

            minimal_connectivity = true;
            assign_prev[i] = j;
            assign_cur[j] = i;
            region_connected[up_region[j]] = true;
            break;
        }
    }
    if (!minimal_connectivity)
        return (false);

    // For each disconnected stair (in a unique region) on the previous level,
    // try to reconnect to a connected up stair on the current level.
    for (int i = 0; i < 3; i++)
    {
        if (assign_prev[i] != -1 || prev_con.connected[i])
            continue;

        bool unique_region = true;
        for (int j = 0; j < i; j++)
        {
            if (prev_con.region[j] == prev_con.region[i])
                unique_region = false;
        }
        if (!unique_region)
            continue;

        // Try first to assign to any connected regions.
        for (int j = 0; j < 3; j++)
        {
            if (assign_cur[j] != -1 || !region_connected[up_region[j]])
                continue;

            assign_prev[i] = j;
            assign_cur[j] = i;
            prev_con.connect_region(prev_con.region[i]);
            break;
        }
        if (assign_prev[i] != -1)
            continue;

        // If we fail, then assign to any up stair with a down, and we'll
        // try to reconnect this section on the next level.
        for (int j = 0; j < 3; j++)
        {
            if (assign_cur[j] != -1 || !has_down[j])
                continue;

            assign_prev[i] = j;
            assign_cur[j] = i;
            break;
        }
    }

    // Assign any connected down stairs from the previous level to any
    // disconnected stairs on the current level.
    for (int i = 0; i < 3; i++)
    {
        if (!prev_con.connected[i] || assign_prev[i] != -1)
            continue;

        for (int j = 0; j < 3; j++)
        {
            if (has_down[j] || assign_cur[j] != -1)
                continue;
            if (region_connected[up_region[j]])
                continue;

            assign_prev[i] = j;
            assign_cur[j] = i;
            region_connected[up_region[j]] = true;
            break;
        }
    }

    // If there are any remaining stairs, assign those.
    for (int i = 0; i < 3; i++)
    {
        if (assign_prev[i] != -1)
            continue;
        for (int j = 0; j < 3; j++)
        {
            if (assign_cur[j] != -1)
                continue;
            assign_prev[i] = j;
            assign_cur[j] = i;

            if (region_connected[up_region[j]])
                prev_con.connect_region(prev_con.region[i]);
            else if (prev_con.connected[i])
                region_connected[up_region[j]] = true;
            break;
        }
    }

    // At the branch bottom, all up stairs must be connected.
    if (at_branch_bottom())
    {
        for (int i = 0; i < 3; i++)
            if (!region_connected[up_region[i]])
                return (false);
    }

    // Sanity check that we're not duplicating stairs.
    bool stairs_unique = (assign_cur[0] != assign_cur[1]
                          && assign_cur[1] != assign_cur[2]);
    ASSERT(stairs_unique);
    if (!stairs_unique)
        return (false);

    // Reassign up stair numbers as needed.
    for (int i = 0; i < 3; i++)
    {
        grd(up_gc[i]) =
            (dungeon_feature_type)(DNGN_STONE_STAIRS_UP_I + assign_cur[i]);
    }

    // Fill in connectivity and regions.
    for (int i = 0; i < 3; i++)
    {
        this_con.region[i] = down_region[i];
        if (down_region[i] != -1)
            this_con.connected[i] = region_connected[down_region[i]];
        else
            this_con.connected[i] = false;

    }

    // Save the connectivity.
    if (player_branch_depth() > 1)
        connectivity[your_branch().id][player_branch_depth() - 2] = prev_con;
    connectivity[your_branch().id][player_branch_depth() - 1] = this_con;

    return (true);
}

void run_map_epilogues ()
{
    // Iterate over level vaults and run each map's epilogue.
    for (unsigned i = 0, size = env.level_vaults.size(); i < size; ++i)
    {
        map_def &map = env.level_vaults[i]->map;
        map.run_lua_epilogue();
    }
}

//////////////////////////////////////////////////////////////////////////
// vault_placement

void vault_placement::reset()
{
    if (_current_temple_hash != NULL)
        _setup_temple_altars(*_current_temple_hash);
    else
        _temple_altar_list.clear();
}

void vault_placement::apply_grid()
{
    if (!size.zero())
    {
        bool clear = !map.has_tag("can_overwrite");

        // NOTE: assumes *no* previous item (I think) or monster (definitely)
        // placement.
        for (rectangle_iterator ri(pos, pos + size - 1); ri; ++ri)
        {
            const coord_def &rp(*ri);
            const coord_def dp = rp - pos;

            const int feat = map.map.glyph(dp);

            if (feat == ' ')
                continue;

            const dungeon_feature_type oldgrid = grd(*ri);

            if (clear)
            {
                env.grid_colours(*ri) = 0;
                env.pgrid(*ri) = 0;
                // what about heightmap?
                tile_clear_flavour(*ri);
            }

            keyed_mapspec *mapsp = map.mapspec_at(dp);
            _vault_grid(*this, feat, *ri, mapsp);

            if (!Generating_Level)
            {
                // Have to link items each square at a time, or
                // dungeon_terrain_changed could blow up.
                link_items();
                // Init tile flavour -- dungeon_terrain_changed does
                // this too, but only if oldgrid != newgrid, so we
                // make sure here.
                tile_init_flavour(*ri);
                const dungeon_feature_type newgrid = grd(*ri);
                grd(*ri) = oldgrid;
                dungeon_terrain_changed(*ri, newgrid, true, true);
                remove_markers_and_listeners_at(*ri);
            }
        }

        map.map.apply_overlays(pos);
    }
}

void vault_placement::draw_at(const coord_def &c)
{
    pos = c;
    apply_grid();
}

void vault_placement::connect(bool spotty) const
{
    for (std::vector<coord_def>::const_iterator i = exits.begin();
         i != exits.end(); ++i)
    {
        if (spotty && _connect_spotty(*i))
            continue;

        if (!_connect_vault_exit(*i))
            dprf("Warning: failed to connect vault exit (%d;%d).", i->x, i->y);
    }
}

void remember_vault_placement(std::string key, const vault_placement &place)
{
    // First we store some info on the vault into the level's properties
    // hash table, so that if there's a crash the crash report can list
    // them all.
    CrawlHashTable &table = env.properties[key].get_table();

    std::string name = make_stringf("%s [%d]", place.map.name.c_str(),
                                    table.size() + 1);

    std::string place_str
        = make_stringf("(%d,%d) (%d,%d) orient: %d lev: %d",
                       place.pos.x, place.pos.y, place.size.x, place.size.y,
                       place.orient, place.level_number);

    table[name] = place_str;

    // Second we setup some info to be saved in the player's properties
    // hash table, so the information can be included in the character
    // dump when the player dies/quits/wins.
    if (you.level_type == LEVEL_DUNGEON
        && !place.map.is_overwritable_layout()
        && !place.map.has_tag_suffix("dummy")
        && !place.map.has_tag("no_dump"))
    {
        const std::string type = place.map.has_tag("extra")
            ? "extra" : "normal";

        _you_vault_list[type].get_vector().push_back(place.map.name);
    }
    else if (you.level_type == LEVEL_PORTAL_VAULT
             && place.map.orient == MAP_ENCOMPASS
             && !place.map.has_tag("no_dump"))
    {
        _portal_vault_map_name = place.map.name;
    }
}

std::string dump_vault_maps()
{
    std::string out = "";

    std::vector<level_id> levels = all_dungeon_ids();

    CrawlHashTable &vaults = you.props[YOU_DUNGEON_VAULTS_KEY].get_table();
    for (unsigned int i = 0; i < levels.size(); i++)
    {
        level_id    &lid = levels[i];
        std::string  lev = lid.describe();

        if (!vaults.exists(lev))
            continue;

        out += lid.describe() + ":\n";

        CrawlHashTable &lists = vaults[lev].get_table();

        const char *types[] = {"normal", "extra"};
        for (int j = 0; j < 2; j++)
        {
            if (!lists.exists(types[j]))
                continue;

            out += "  ";
            out += types[j];
            out += ": ";

            CrawlVector &vec = lists[types[j]].get_vector();

            for (unsigned int k = 0, size = vec.size(); k < size; k++)
            {
                out += vec[k].get_string();
                if (k < (size - 1))
                    out += ", ";
            }

            out += "\n";
        }
        out += "\n";
    }
    CrawlVector &portals = you.props[YOU_PORTAL_VAULT_MAPS_KEY].get_vector();

    if (!portals.empty())
    {
        out += "\n";

        out += "Portal vault maps: ";

        for (unsigned int i = 0, size = portals.size(); i < size; i++)
        {
            out += portals[i].get_string();

            if (i < (size - 1))
                out += ", ";
        }

        out += "\n\n";
    }
    return (out);
}

///////////////////////////////////////////////////////////////////////////
// vault_place_iterator

vault_place_iterator::vault_place_iterator(const vault_placement &vp)
    : vault_place(vp), pos(vp.pos), tl(vp.pos), br(vp.pos + vp.size - 1)
{
    --pos.x;
    ++*this;
}

vault_place_iterator::operator bool () const
{
    return pos.y <= br.y && pos.x <= br.x;
}

coord_def vault_place_iterator::operator * () const
{
    return pos;
}

const coord_def *vault_place_iterator::operator -> () const
{
    return &pos;
}

vault_place_iterator &vault_place_iterator::operator ++ ()
{
    while (pos.y <= br.y)
    {
        if (++pos.x > br.x)
        {
            pos.x = tl.x;
            ++pos.y;
        }
        if (pos.y <= br.y && vault_place.map.in_map(pos - tl))
            break;
    }
    return (*this);
}

vault_place_iterator vault_place_iterator::operator ++ (int)
{
    const vault_place_iterator copy = *this;
    ++*this;
    return (copy);
}

//////////////////////////////////////////////////////////////////////////
// unwind_vault_placement_mask

unwind_vault_placement_mask::unwind_vault_placement_mask(const map_mask *mask)
    : oldmask(Vault_Placement_Mask)
{
    Vault_Placement_Mask = mask;
}

unwind_vault_placement_mask::~unwind_vault_placement_mask()
{
    Vault_Placement_Mask = oldmask;
}


// mark all unexplorable squares, count the rest
static void _calc_density()
{
    int open = 0;
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        // If for some reason a level gets modified afterwards, dug-out
        // places in unmodified parts should not suddenly become explorable.
        if (!testbits(env.pgrid(*ri), FPROP_SEEN_OR_NOEXP))
            for (adjacent_iterator ai(*ri, false); ai; ++ai)
                if (grd(*ai) >= DNGN_MINITEM)
                {
                    open++;
                    goto out;
                }
        env.pgrid(*ri) |= FPROP_SEEN_OR_NOEXP;
    out:;
    }

    dprf("Level density: %d", open);
    env.density = open;
}
