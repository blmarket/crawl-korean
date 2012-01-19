/**
 * @file
 * @brief Misc abyss specific functions.
**/

#include "AppHdr.h"

#include "abyss.h"

#include <cmath>
#include <cstdlib>
#include <algorithm>

#include "abyss.h"
#include "areas.h"
#include "artefact.h"
#include "cellular.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "dungeon.h"
#include "env.h"
#include "itemprop.h"
#include "items.h"
#include "l_defs.h"
#include "los.h"
#include "makeitem.h"
#include "mapmark.h"
#include "maps.h"
#include "message.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-iter.h"
#include "mon-pathfind.h"
#include "mon-place.h"
#include "mon-transit.h"
#include "mon-util.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "showsymb.h"
#include "sprint.h"
#include "state.h"
#include "terrain.h"
#include "tiledef-dngn.h"
#include "tileview.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "viewgeom.h"
#include "xom.h"
#ifdef WIZARD
 #include "wiz-dgn.h"
#endif

const coord_def ABYSS_CENTRE(GXM / 2, GYM / 2);

static const int ABYSSAL_RUNE_MAX_ROLL = 200;

abyss_state abyssal_state;

static std::vector<dungeon_feature_type> abyssal_features;
static std::list<monster*> displaced_monsters;

static void abyss_area_shift(void);

// If not_seen is true, don't place the feature where it can be seen from
// the centre.  Returns the chosen location, or INVALID_COORD if it
// could not be placed.
static coord_def _place_feature_near(const coord_def &centre,
                                     int radius,
                                     dungeon_feature_type candidate,
                                     dungeon_feature_type replacement,
                                     int tries, bool not_seen = false)
{
    coord_def cp = INVALID_COORD;
    const int radius2 = radius * radius + 1;
    for (int i = 0; i < tries; ++i)
    {
        cp = centre + coord_def(random_range(-radius, radius),
                                random_range(-radius, radius));

        if (cp == centre || (cp - centre).abs() > radius2 || !in_bounds(cp))
            continue;

        if (not_seen && cell_see_cell_nocache(cp, centre))
            continue;

        if (grd(cp) == candidate)
        {
            dprf("Placing %s at (%d,%d)",
                 dungeon_feature_name(replacement),
                 cp.x, cp.y);
            grd(cp) = replacement;
            return (cp);
        }
    }
    return (INVALID_COORD);
}

//#define DEBUG_ABYSS

// Returns a feature suitable for use in the proto-Abyss level.
static dungeon_feature_type _abyss_proto_feature()
{
    return random_choose_weighted(3000, DNGN_FLOOR,
                                   600, DNGN_ROCK_WALL,
                                   300, DNGN_STONE_WALL,
                                   100, DNGN_METAL_WALL,
                                     1, DNGN_CLOSED_DOOR,
                                     0);
}

static void _write_abyssal_features()
{
    if (abyssal_features.empty())
        return;

    const int count = abyssal_features.size();
    const int scalar = 0xFF;
    int index = 0;
    for (radius_iterator ri(ABYSS_CENTRE, LOS_RADIUS, C_ROUND); ri; ++ri)
    {
        const int dist = distance(ABYSS_CENTRE, *ri);
        int chance = pow(0.98, dist) * scalar;
        if (!map_masked(*ri, MMT_VAULT))
        {
            if (dist < 4 || x_chance_in_y(chance, scalar))
            {
                if (abyssal_features[index] != DNGN_UNSEEN)
                {
                    grd(*ri) = abyssal_features[index];
                    env.level_map_mask(*ri) = MMT_VAULT;
                }
            }
            else
            {
                //Entombing the player is lame.
                grd(*ri) = DNGN_FLOOR;
            }
        }

        ++index;
        if (index > count)
            return;
    }
}

// Returns the roll to use to check if we want to create an abyssal rune.
static int _abyssal_rune_roll()
{
    if (you.runes[RUNE_ABYSSAL])
        return (-1);

    // The longer the player's hung around in the Abyss, the more
    // likely the rune. Never generate a new rune if the player
    // already found one, but make the Abyssal rune eligible for
    // generation again if the player fails to find or pick it up.

    // If the player leaves the Abyss turns_on_level resets to 0. So
    // hang in there if you want your Abyssal rune fix quick. :P

    // Worshippers of Lugonu with decent piety will attract the rune
    // to themselves.

    // In general, the base chance of an abyssal rune is 1/200 for
    // every item roll (the item roll is 1/200 for every floor square
    // in the abyss). Once the player has spent more than 500 turns on
    // level, the abyss rune chance increases linearly every 228 turns
    // up to a maximum of 34/200 (17%) per item roll after ~8000
    // turns.

    // For Lugonu worshippers, the base chance of an abyssal rune is 1
    // in 200 as for other players, but the rune chance increases
    // linearly after 50 turns on the level, up to the same 34/200
    // (17%) chance after ~2000 turns.

    const bool lugonu_favoured =
        (you.religion == GOD_LUGONU && !player_under_penance()
         && you.piety > 120);

    const int cutoff = lugonu_favoured ? 50 : 500;
    const int scale = lugonu_favoured ? 57 : 228;

    const int odds =
        std::min(1 + std::max((env.turns_on_level - cutoff) / scale, 0), 34);
#ifdef DEBUG_ABYSS
    dprf("Abyssal rune odds: %d in %d (%.2f%%)",
         odds, ABYSSAL_RUNE_MAX_ROLL, odds * 100.0 / ABYSSAL_RUNE_MAX_ROLL);
#endif
    return (odds);
}

static void _abyss_create_room(const map_mask &abyss_genlevel_mask)
{
    const int largest_room_dimension = 10;

    // Pick the corners
    const coord_def tl(
        random_range(MAPGEN_BORDER,
                     GXM - 1 - (MAPGEN_BORDER + largest_room_dimension)),
        random_range(MAPGEN_BORDER,
                     GYM - 1 - (MAPGEN_BORDER + largest_room_dimension)));
    const coord_def roomsize(random_range(1, largest_room_dimension),
                             random_range(1, largest_room_dimension));
    const coord_def br = tl + roomsize;

    // Check if the room is taken.
    for (rectangle_iterator ri(tl, br); ri; ++ri)
        if (!abyss_genlevel_mask(*ri))
            return;

    // Make the room.
    for (rectangle_iterator ri(tl, br); ri; ++ri)
        grd(*ri) = DNGN_FLOOR;
}

static void _abyss_create_rooms(const map_mask &abyss_genlevel_mask,
                                int rooms_to_do)
{
    for (int i = 0; i < rooms_to_do; ++i)
    {
        if (one_chance_in(100))
            break;
        _abyss_create_room(abyss_genlevel_mask);
    }
}

static void _abyss_erase_stairs_from(const vault_placement *vp)
{
    for (vault_place_iterator vi(*vp); vi; ++vi)
    {
        const coord_def p(*vi);
        const dungeon_feature_type feat(grd(p));
        if (feat_is_stair(feat)
            && feat != DNGN_EXIT_ABYSS
            && feat != DNGN_ENTER_PORTAL_VAULT)
        {
            grd(p) = DNGN_FLOOR;
        }
    }
}

static bool _abyss_place_map(const map_def *mdef)
{
    const bool did_place = dgn_safe_place_map(mdef, false, false, INVALID_COORD);
    if (did_place)
        _abyss_erase_stairs_from(env.level_vaults[env.level_vaults.size() - 1]);

    return (did_place);
}

static bool _abyss_place_vault_tagged(const map_mask &abyss_genlevel_mask,
                                      const std::string &tag)
{
    const map_def *map = random_map_for_tag(tag, false, true);
    if (map)
    {
        unwind_vault_placement_mask vaultmask(&abyss_genlevel_mask);
        return (_abyss_place_map(map));
    }
    return (false);
}

static bool _abyss_place_rune_vault(const map_mask &abyss_genlevel_mask)
{
    return _abyss_place_vault_tagged(abyss_genlevel_mask, "abyss_rune");
}

static bool _abyss_place_rune(const map_mask &abyss_genlevel_mask,
                              bool use_vaults)
{
    // Use a rune vault if there's one.
    if (use_vaults && _abyss_place_rune_vault(abyss_genlevel_mask))
        return (true);

    coord_def chosen_spot;
    int places_found = 0;

    // Pick a random spot to drop the rune. We specifically do not use
    // random_in_bounds and similar, because we may be dealing with a
    // non-rectangular region, and we want to place the rune fairly.
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        const coord_def p(*ri);
        if (abyss_genlevel_mask(p)
            && grd(p) == DNGN_FLOOR && igrd(p) == NON_ITEM
            && one_chance_in(++places_found))
        {
            chosen_spot = p;
        }
    }

    if (places_found)
    {
        dprf("Placing abyssal rune at (%d,%d)", chosen_spot.x, chosen_spot.y);
        int thing_created = items(1, OBJ_MISCELLANY,
                                  MISC_RUNE_OF_ZOT, true,
                                  DEPTH_ABYSS, 0);
        if (thing_created != NON_ITEM)
        {
            mitm[thing_created].plus = RUNE_ABYSSAL;
            item_colour(mitm[thing_created]);
        }
        move_item_to_grid(&thing_created, chosen_spot);
        return (thing_created != NON_ITEM);
    }

    return (false);
}

// Returns true if items can be generated on the given square.
static bool _abyss_square_accepts_items(const map_mask &abyss_genlevel_mask,
                                        coord_def p)
{
    return (abyss_genlevel_mask(p)
            && grd(p) == DNGN_FLOOR
            && igrd(p) == NON_ITEM
            && !map_masked(p, MMT_VAULT));
}

static int _abyss_create_items(const map_mask &abyss_genlevel_mask,
                               bool placed_abyssal_rune,
                               bool use_vaults)
{
    // During game start, number and level of items mustn't be higher than
    // that on level 1. Abyss in sprint games has no items.
    int num_items = 150, items_level = DEPTH_ABYSS;
    int items_placed = 0;

    if (crawl_state.game_is_sprint())
    {
        num_items   = 0;
        items_level = 0;
    }
    else if (you.char_direction == GDT_GAME_START)
    {
        num_items   = 3 + roll_dice(3, 11);
        items_level = 0;
    }

    const int abyssal_rune_roll = _abyssal_rune_roll();
    bool should_place_abyssal_rune = false;
    std::vector<coord_def> chosen_item_places;
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        if (_abyss_square_accepts_items(abyss_genlevel_mask, *ri))
        {
            if (items_placed < num_items && one_chance_in(200))
            {
                // [ds] Don't place abyssal rune in this loop to avoid
                // biasing rune placement toward the north-west of the
                // abyss level. Instead, make a note of whether we
                // decided to place the abyssal rune at all, and if we
                // did, place it randomly somewhere in the map at the
                // end of the item-gen pass. We may as a result create
                // (num_items + 1) items instead of num_items, which
                // is acceptable.
                if (!placed_abyssal_rune && !should_place_abyssal_rune
                    && abyssal_rune_roll != -1
                    && you.char_direction != GDT_GAME_START
                    && x_chance_in_y(abyssal_rune_roll, ABYSSAL_RUNE_MAX_ROLL))
                {
                    should_place_abyssal_rune = true;
                }

                chosen_item_places.push_back(*ri);
            }
        }
    }

    if (!placed_abyssal_rune && should_place_abyssal_rune)
    {
        if (_abyss_place_rune(abyss_genlevel_mask, use_vaults))
            ++items_placed;
    }

    for (int i = 0, size = chosen_item_places.size(); i < size; ++i)
    {
        const coord_def place(chosen_item_places[i]);
        if (_abyss_square_accepts_items(abyss_genlevel_mask, place))
        {
            int thing_created = items(1, OBJ_RANDOM, OBJ_RANDOM,
                                      true, items_level, 250);
            move_item_to_grid(&thing_created, place);
            if (thing_created != NON_ITEM)
                items_placed++;
        }
    }

    return (items_placed);
}

void push_features_to_abyss()
{
    abyssal_features.clear();

    for (radius_iterator ri(you.pos(), LOS_RADIUS, C_ROUND); ri; ++ri)
    {
        dungeon_feature_type feature = grd(*ri);

        if (!in_bounds(*ri))
            feature = DNGN_UNSEEN;

        if (feat_is_stair(feature))
            feature = (one_chance_in(3) ? DNGN_STONE_ARCH : DNGN_FLOOR);

        if (feat_is_altar(feature))
            feature = (one_chance_in(9) ? DNGN_ALTAR_XOM : DNGN_FLOOR);

        if (feat_is_trap(feature, true) || feature == DNGN_ENTER_SHOP)
            feature = DNGN_FLOOR;

        switch (feature)
        {
            // demote permarock
            case DNGN_PERMAROCK_WALL:
                feature = DNGN_ROCK_WALL;
                break;
            case DNGN_CLEAR_PERMAROCK_WALL:
                feature = DNGN_CLEAR_ROCK_WALL;
                break;
            case DNGN_SLIMY_WALL:
                feature = DNGN_GREEN_CRYSTAL_WALL;
            default:
                // handle more terrain types.
                break;
        }
        abyssal_features.push_back(feature);
    }
}

// Returns N so that the chance of placing an abyss exit on any given
// square is 1 in N.
static int _abyss_exit_chance()
{
    int exit_chance = 7500;
    if (crawl_state.game_is_sprint())
    {
        exit_chance = sprint_modify_abyss_exit_chance(exit_chance);
    }
    return exit_chance;
}

static bool _abyss_check_place_feat(coord_def p,
                                    const int feat_chance,
                                    int *feats_wanted,
                                    bool *use_map,
                                    dungeon_feature_type which_feat,
                                    const map_mask &abyss_genlevel_mask)
{
    if (!which_feat)
        return (false);

    const bool place_feat = feat_chance && one_chance_in(feat_chance);

    if (place_feat && feats_wanted)
        ++*feats_wanted;

    // There's no longer a need to check for features under items,
    // since we're working on fresh grids that are guaranteed
    // item-free.
    if (place_feat || (feats_wanted && *feats_wanted > 0))
    {
        dprf("Placing abyss feature: %s.", dungeon_feature_name(which_feat));

        // When placing Abyss exits, try to use a vault if we have one.
        if (which_feat == DNGN_EXIT_ABYSS
            && use_map && *use_map
            && _abyss_place_vault_tagged(abyss_genlevel_mask, "abyss_exit"))
        {
            *use_map = false;
        }
        else
            grd(p) = which_feat;

        if (feats_wanted)
            --*feats_wanted;
        return (true);
    }
    return (false);
}

static dungeon_feature_type _abyss_pick_altar()
{
    // Lugonu has a flat 50% chance of corrupting the altar.
    if (coinflip())
        return (DNGN_ALTAR_LUGONU);

    god_type god;

    do
        god = random_god(true);
    while (is_good_god(god)
           || is_unavailable_god(god));

    return (altar_for_god(god));
}

static bool _abyssal_rune_at(const coord_def p)
{
    for (stack_iterator si(p); si; ++si)
        if (item_is_rune(*si, RUNE_ABYSSAL))
            return true;
    return false;
}

class xom_abyss_feature_amusement_check
{
private:
    bool exit_was_near;
    bool rune_was_near;

private:
    bool abyss_exit_nearness() const
    {
        // env.map_knowledge().known() doesn't work on unmappable levels because
        // mapping flags are not set on such levels.
        for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
            if (grd(*ri) == DNGN_EXIT_ABYSS && env.map_knowledge(*ri).seen())
                return true;

        return false;
    }

    bool abyss_rune_nearness() const
    {
        // See above comment about env.map_knowledge().known().
        for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
            if (env.map_knowledge(*ri).seen() && _abyssal_rune_at(*ri))
                return true;
        return false;
    }

public:
    xom_abyss_feature_amusement_check()
    {
        exit_was_near = abyss_exit_nearness();
        rune_was_near = abyss_rune_nearness();
    }

    // If the player was almost to the exit when it disappeared, Xom
    // is extremely amused. He's also extremely amused if the player
    // winds up right next to an exit when there wasn't one there
    // before. The same applies to Abyssal runes.
    ~xom_abyss_feature_amusement_check()
    {
        // Update known terrain
        viewwindow();

        const bool exit_is_near = abyss_exit_nearness();
        const bool rune_is_near = abyss_rune_nearness();

        if (exit_was_near && !exit_is_near || rune_was_near && !rune_is_near)
            xom_is_stimulated(200, gettext("Xom snickers loudly."), true);

        if (!rune_was_near && rune_is_near || !exit_was_near && exit_is_near)
            xom_is_stimulated(200);
    }
};

static void _abyss_lose_monster(monster& mons)
{
    if (mons.needs_abyss_transit())
        mons.set_transit(level_id(LEVEL_ABYSS));

    mons.destroy_inventory();
    monster_cleanup(&mons);
}

// If a sanctuary exists and is in LOS, moves it to keep it in the
// same place relative to the player's location after a shift. If the
// sanctuary is not in player LOS, removes it.
static void _abyss_move_sanctuary(const coord_def abyss_shift_start_centre,
                                  const coord_def abyss_shift_end_centre)
{
    if (env.sanctuary_time > 0 && in_bounds(env.sanctuary_pos))
    {
        if (you.see_cell(env.sanctuary_pos))
            env.sanctuary_pos += (abyss_shift_end_centre -
                                  abyss_shift_start_centre);
        else
            remove_sanctuary(false);
    }
}

static void _push_displaced_monster(monster* mon)
{
    displaced_monsters.push_back(mon);
}

static void _place_displaced_monsters()
{
    std::list<monster*>::iterator mon_itr;

    for (mon_itr = displaced_monsters.begin();
         mon_itr != displaced_monsters.end(); ++mon_itr)
    {
        monster* mon = *mon_itr;
        if (!mon->find_home_near_place(mon->pos()))
        {
            maybe_bloodify_square(mon->pos());
            if (you.can_see(mon))
                simple_monster_message(mon, gettext(" is pulled into the abyss."),
                        MSGCH_BANISHMENT);
            _abyss_lose_monster(*mon);

        }
    }

    displaced_monsters.clear();
}

static void _push_items()
{
    for (int i = 0; i < MAX_ITEMS; i++)
    {
        item_def& item(mitm[i]);
        if (!item.defined() || !in_bounds(item.pos) || item.held_by_monster())
            continue;

        if (grd(item.pos) == DNGN_FLOOR)
            continue;

        for (distance_iterator di(item.pos); di; ++di)
            if (grd(*di) == DNGN_FLOOR)
            {
                move_item_to_grid(&i, *di, true);
                break;
            }
    }
}

// Deletes everything on the level at the given position.
// Things that are wiped:
// 1. Dungeon terrain (set to DNGN_UNSEEN)
// 2. Monsters (the player is unaffected)
// 3. Items
// 4. Clouds
// 5. Terrain properties
// 6. Terrain colours
// 7. Vault (map) mask
// 8. Vault id mask
// 9. Map markers

static void _abyss_wipe_square_at(coord_def p, bool saveMonsters=false)
{
    // Nuke terrain.
    destroy_shop_at(p);
    destroy_trap(p);

    // Nuke vault flag.
    if (map_masked(p, MMT_VAULT))
        env.level_map_mask(p) &= ~MMT_VAULT;

    grd(p) = DNGN_UNSEEN;

    // Nuke items.
#ifdef DEBUG_ABYSS
    if (igrd(p) != NON_ITEM)
        dprf("Nuke item stack at (%d, %d)", p.x, p.y);
#endif
    lose_item_stack(p);

    // Nuke monster.
    if (monster* mon = monster_at(p))
    {
        if (saveMonsters)
            _push_displaced_monster(mon);
        else
            _abyss_lose_monster(*mon);
    }

    // Delete cloud.
    delete_cloud_at(p);

    env.pgrid(p)        = 0;
    env.grid_colours(p) = 0;
#ifdef USE_TILE
    env.tile_bk_fg(p)   = 0;
    env.tile_bk_bg(p)   = 0;
#endif
    tile_clear_flavour(p);
    tile_init_flavour(p);

    env.level_map_mask(p) = 0;
    env.level_map_ids(p)  = INVALID_MAP_INDEX;

    remove_markers_and_listeners_at(p);

    env.map_knowledge(p).clear();
}

// Removes monsters, clouds, dungeon features, and items from the
// level, torching all squares for which the supplied mask is false.
static void _abyss_wipe_unmasked_area(const map_mask &abyss_preserve_mask)
{
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
        if (!abyss_preserve_mask(*ri))
            _abyss_wipe_square_at(*ri);
}

// Moves everything at src to dst.
static void _abyss_move_entities_at(coord_def src, coord_def dst)
{
    dgn_move_entities_at(src, dst, true, true, true);
}

// Move all vaults within the mask by the specified delta.
static void _abyss_move_masked_vaults_by_delta(const map_mask &mask,
                                               const coord_def delta)
{
    std::set<int> vault_indexes;
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        const int vi = env.level_map_ids(*ri);
        if (vi != INVALID_MAP_INDEX)
            vault_indexes.insert(vi);
    }

    for (std::set<int>::const_iterator i = vault_indexes.begin();
         i != vault_indexes.end(); ++i)
    {
        vault_placement &vp(*env.level_vaults[*i]);
#ifdef DEBUG_DIAGNOSTICS
        const coord_def oldp = vp.pos;
#endif
        vp.pos += delta;
        dprf("Moved vault (%s) from (%d,%d)-(%d,%d)",
             vp.map.name.c_str(), oldp.x, oldp.y, vp.pos.x, vp.pos.y);
    }
}

// Moves the player, monsters, terrain and items in the square (circle
// in movement distance) around the player with the given radius to
// the square centred on target_centre.
//
// Assumes:
// a) target can be truncated if not fully in bounds
// b) source and target areas may overlap
//
static void _abyss_move_entities(coord_def target_centre,
                                 map_mask *shift_area_mask)
{
    const coord_def source_centre = you.pos();
    const coord_def delta = (target_centre - source_centre).sgn();

    // When moving a region, walk backward to handle overlapping
    // ranges correctly.
    coord_def direction = -delta;

    if (!direction.x)
        direction.x = 1;
    if (!direction.y)
        direction.y = 1;

    coord_def start(MAPGEN_BORDER, MAPGEN_BORDER);
    coord_def end(GXM - 1 - MAPGEN_BORDER, GYM - 1 - MAPGEN_BORDER);

    if (direction.x == -1)
        std::swap(start.x, end.x);
    if (direction.y == -1)
        std::swap(start.y, end.y);

    end += direction;

    for (int y = start.y; y != end.y; y += direction.y)
    {
        for (int x = start.x; x != end.x; x += direction.x)
        {
            const coord_def src(x, y);
            if (!(*shift_area_mask)(src))
                continue;

            (*shift_area_mask)(src) = false;

            const coord_def dst = src - source_centre + target_centre;
            if (map_bounds_with_margin(dst, MAPGEN_BORDER))
            {
                (*shift_area_mask)(dst) = true;
                // Wipe the dstination clean before dropping things on it.
                _abyss_wipe_square_at(dst);
                _abyss_move_entities_at(src, dst);
            }
            else
            {
                // Wipe the source clean even if the dst is not in bounds.
                _abyss_wipe_square_at(src);
            }
        }
    }

    _abyss_move_masked_vaults_by_delta(*shift_area_mask,
                                       target_centre - source_centre);
}

static void _abyss_expand_mask_to_cover_vault(map_mask *mask,
                                              int map_index)
{
    dprf("Expanding mask to cover vault %d (nvaults: %u)",
         map_index, (unsigned int)env.level_vaults.size());
    const vault_placement &vp = *env.level_vaults[map_index];
    for (vault_place_iterator vpi(vp); vpi; ++vpi)
        (*mask)(*vpi) = true;
}

// Identifies the smallest movement circle around the given source that can
// be shifted without breaking up any vaults.
static void _abyss_identify_area_to_shift(coord_def source, int radius,
                                          map_mask *mask)
{
    mask->init(false);

    std::set<int> affected_vault_indexes;
    for (radius_iterator ri(source, radius, C_SQUARE); ri; ++ri)
    {
        if (!map_bounds_with_margin(*ri, MAPGEN_BORDER))
            continue;

        (*mask)(*ri) = true;

        const int map_index = env.level_map_ids(*ri);
        if (map_index != INVALID_MAP_INDEX)
            affected_vault_indexes.insert(map_index);
    }

    for (std::set<int>::const_iterator i = affected_vault_indexes.begin();
         i != affected_vault_indexes.end(); ++i)
    {
        _abyss_expand_mask_to_cover_vault(mask, *i);
    }
}

static void _abyss_invert_mask(map_mask *mask)
{
    for (rectangle_iterator ri(0); ri; ++ri)
        (*mask)(*ri) = !(*mask)(*ri);
}

// Moves everything in the given radius around the player (where radius=0 =>
// only the player) to another part of the level, centred on target_centre.
// Everything not in the given radius is wiped to DNGN_UNSEEN and the provided
// map_mask is set to true for all wiped squares.
//
// Things that are moved:
// 1. Dungeon terrain
// 2. Actors (player + monsters)
// 3. Items
// 4. Clouds
// 5. Terrain properties
// 6. Terrain colours
// 7. Vaults
// 8. Vault (map) mask
// 9. Vault id mask
// 10. Map markers
//
// After the shift, any vaults that are no longer referenced in the id
// mask will be discarded. If those vaults had any unique tags or
// names, the tag/name will NOT be unregistered.
//
// Assumes:
// a) radius >= LOS_RADIUS
// b) All points in the source and target areas are in bounds.
static void _abyss_shift_level_contents_around_player(
    int radius,
    coord_def target_centre,
    map_mask &abyss_destruction_mask)
{
    const coord_def source_centre = you.pos();

    abyssal_state.major_coord += (source_centre - ABYSS_CENTRE);

    ASSERT(radius >= LOS_RADIUS);
#ifdef WIZARD
    // This should only really happen due to wizmode blink/movement.
    if (!map_bounds_with_margin(source_centre, radius))
        mprf("source_centre(%d, %d) outside map radius %d", source_centre.x, source_centre.y, radius);
    if (!map_bounds_with_margin(target_centre, radius))
        mprf("target_centre(%d, %d) outside map radius %d", target_centre.x, target_centre.y, radius);
#else
    ASSERT(map_bounds_with_margin(source_centre, radius));
    ASSERT(map_bounds_with_margin(target_centre, radius));
#endif

    _abyss_identify_area_to_shift(source_centre, radius,
                                  &abyss_destruction_mask);

    // Shift sanctuary centre if it's close.
    _abyss_move_sanctuary(you.pos(), target_centre);

    // Zap everything except the area we're shifting, so that there's
    // nothing in the way of moving stuff.
    _abyss_wipe_unmasked_area(abyss_destruction_mask);

    // Move stuff to its new home. This will also move the player.
    _abyss_move_entities(target_centre, &abyss_destruction_mask);

    // [ds] Rezap everything except the shifted area. NOTE: the old
    // code did not do this, leaving a repeated swatch of Abyss behind
    // at the old location for every shift; discussions between Linley
    // and dpeg on ##crawl confirm that this (repeated swatch of
    // terrain left behind) was not intentional.
    _abyss_wipe_unmasked_area(abyss_destruction_mask);

    // So far we've used the mask to track the portions of the level we're
    // preserving. The inverse of the mask represents the area to be filled
    // with brand new abyss:
    _abyss_invert_mask(&abyss_destruction_mask);

    // Update env.level_vaults to discard any vaults that are no longer in
    // the picture.
    dgn_erase_unused_vault_placements();
}

static void _abyss_generate_monsters(int nmonsters)
{
    mgen_data mons;
    mons.level_type = LEVEL_ABYSS;
    mons.proximity  = PROX_AWAY_FROM_PLAYER;

    for (int mcount = 0; mcount < nmonsters; mcount++)
        mons_place(mons);
}

void maybe_shift_abyss_around_player()
{
    ASSERT(you.level_type == LEVEL_ABYSS);
    if (map_bounds_with_margin(you.pos(),
                               MAPGEN_BORDER + ABYSS_AREA_SHIFT_RADIUS + 1))
    {
        return;
    }

    dprf("Shifting abyss at (%d,%d)", you.pos().x, you.pos().y);

    abyss_area_shift();
    if (you.pet_target != MHITYOU)
        you.pet_target = MHITNOT;

#ifdef DEBUG_DIAGNOSTICS
    int j = 0;
    for (int i = 0; i < MAX_ITEMS; ++i)
         if (mitm[i].defined())
             ++j;

    dprf("Number of items present: %d", j);

    j = 0;
    for (monster_iterator mi; mi; ++mi)
        ++j;

    dprf("Number of monsters present: %d", j);
    dprf("Number of clouds present: %d", env.cloud_no);
#endif
}

void save_abyss_uniques()
{
    for (monster_iterator mi; mi; ++mi)
        if (mi->needs_abyss_transit()
            && !testbits(mi->flags, MF_TAKING_STAIRS))
        {
            mi->set_transit(level_id(LEVEL_ABYSS));
        }
}

static bool _abyss_teleport_within_level()
{
    // Try to find a good spot within the shift zone.
    for (int i = 0; i < 100; i++)
    {
        const coord_def newspot =
            dgn_random_point_in_margin(MAPGEN_BORDER
                                       + ABYSS_AREA_SHIFT_RADIUS
                                       + 1);

        if ((grd(newspot) == DNGN_FLOOR
             || grd(newspot) == DNGN_SHALLOW_WATER)
            && !monster_at(newspot)
            && env.cgrid(newspot) == EMPTY_CLOUD)
        {
#ifdef DEBUG_ABYSS
            dprf("Abyss same-area teleport to (%d,%d).",
                 newspot.x, newspot.y);
#endif
            you.moveto(newspot);
            return (true);
        }
    }
    return (false);
}

static dungeon_feature_type _abyss_grid(double x, double y, double depth,
                                        cloud_type &cloud, int &cloud_lifetime)
{
    const dungeon_feature_type terrain_elements[] =
    {
        DNGN_ROCK_WALL,
        DNGN_ROCK_WALL,
        DNGN_ROCK_WALL,
        DNGN_ROCK_WALL,
        DNGN_STONE_WALL,
        DNGN_STONE_WALL,
        DNGN_METAL_WALL,
        DNGN_GREEN_CRYSTAL_WALL,
        DNGN_LAVA,
        DNGN_DEEP_WATER,
        DNGN_SHALLOW_WATER,
        DNGN_SHALLOW_WATER,
    };
    const int n_terrain_elements = ARRAYSZ(terrain_elements);

    const int NUM_CLOUDS = 6;
    const cloud_type clouds[NUM_CLOUDS] =
    {
        CLOUD_BLACK_SMOKE,
        CLOUD_GREY_SMOKE,
        CLOUD_BLUE_SMOKE,
        CLOUD_PURPLE_SMOKE,
        CLOUD_TLOC_ENERGY,
        CLOUD_MIST
    };

    const double scale = 1.0 / 2.2;
    const double sub_scale_x = 17.0;
    const double sub_scale_y = 31.0;
    const double sub_scale_depth = 0.1;

    worley::noise_datum noise = worley::worley(x * scale, y * scale, depth);
    dungeon_feature_type feat = DNGN_FLOOR;

    worley::noise_datum sub_noise = worley::worley(x * sub_scale_x,
                                                   y * sub_scale_y,
                                                   depth * sub_scale_depth);

    int dist = noise.distance[0] * 100;
    bool isWall = (dist > 118 || dist < 30);

    if (noise.id[0] + noise.id[1] % 2  == 0)
        isWall = sub_noise.id[0] % 2;

    if (sub_noise.id[0] % 3 == 0)
        isWall = isWall ^ true;

    if (isWall)
    {
        int fuzz = (sub_noise.id[1] % 3 ? 0 : sub_noise.id[1] % 2 + 1);
        int id = (noise.id[0] + fuzz) % n_terrain_elements;
        feat = terrain_elements[id];
    }

    if (feat == DNGN_FLOOR && !(noise.id[1] % 3))
    {
        // Only used if the feature actually changed.
        cloud = clouds[sub_noise.id[1] % NUM_CLOUDS];
        cloud_lifetime = (noise.id[1] % 4)+2;
    }
    else
    {
        cloud = CLOUD_NONE;
        cloud_lifetime = 0;
    }

    return feat;
}

static void _abyss_apply_terrain(const map_mask &abyss_genlevel_mask,
                                 bool morph = false, double old_depth = 0.0)
{
    if (one_chance_in(3) && !morph)
        _abyss_create_rooms(abyss_genlevel_mask, random_range(1, 10));

    const int exit_chance = _abyss_exit_chance();

    // Except for the altar on the starting position, don't place any altars.
    const int altar_chance = you.char_direction != GDT_GAME_START? 10000 : 0;

    int exits_wanted  = 0;
    int altars_wanted = 0;
    bool use_abyss_exit_map = true;

    const coord_def major_coord = abyssal_state.major_coord;
    const double abyss_depth = abyssal_state.depth;

    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        const coord_def p(*ri);

        if (you.pos() == p || !abyss_genlevel_mask(p))
            continue;

        // Don't decay vaults.
        if (map_masked(p, MMT_VAULT))
            continue;

        // Don't morph a cell with the Rune or the exit.
        if (_abyssal_rune_at(p) || grd(p) == DNGN_EXIT_ABYSS)
            continue;

        // Don't morph altars and stone arches.
        if (grd(p) == DNGN_STONE_ARCH || feat_is_altar(grd(p)))
            continue;

        if (grd(p) != DNGN_UNSEEN && !morph)
            continue;

        double x = (p.x + major_coord.x);
        double y = (p.y + major_coord.y);

        cloud_type cloud = CLOUD_NONE;
        int cloud_lifetime = 0;

        // What should have been there previously?  It might not be because
        // of external changes such as digging.
        const dungeon_feature_type oldfeat = _abyss_grid(x, y, old_depth,
                                                         cloud, cloud_lifetime);
        const dungeon_feature_type feat = _abyss_grid(x, y, abyss_depth,
                                                      cloud, cloud_lifetime);

        // If the selected grid is already there, *or* if we're morphing and
        // the selected grid should have been there, do nothing.
        if (feat != grd(p) && (!morph || feat != oldfeat))
        {
            // _abyss_grid is responsible for ensuring clouds are selected
            // only on floor grids.
            if (cloud != CLOUD_NONE && in_los_bounds_g(p))
                check_place_cloud(cloud, p, cloud_lifetime, 0);

            grd(p) = feat;

            monster* mon = monster_at(p);
            if (mon && !monster_habitable_grid(mon, feat))
                _push_displaced_monster(mon);
        }

        if (morph)
            continue;

        // Place abyss exits, stone arches, and altars to liven up the scene
        // (only on area creation, not on morphing).
        (_abyss_check_place_feat(p, exit_chance,
                                 &exits_wanted,
                                 &use_abyss_exit_map,
                                 DNGN_EXIT_ABYSS,
                                 abyss_genlevel_mask)
         ||
         _abyss_check_place_feat(p, altar_chance,
                                 &altars_wanted,
                                 NULL,
                                 _abyss_pick_altar(),
                                 abyss_genlevel_mask)
         ||
         _abyss_check_place_feat(p, 10000, NULL, NULL,
                                 DNGN_STONE_ARCH,
                                 abyss_genlevel_mask));
    }
}

static int _abyss_place_vaults(const map_mask &abyss_genlevel_mask)
{
    unwind_vault_placement_mask vaultmask(&abyss_genlevel_mask);

    int vaults_placed = 0;

    const int maxvaults = 4;
    for (int i = 0; i < maxvaults; ++i)
    {
        const map_def *map = random_map_for_tag("abyss", false, true);
        if (!map)
            break;

        if (_abyss_place_map(map) && !one_chance_in(2 + (++vaults_placed)))
            break;
    }

    return (vaults_placed);
}

static void _generate_area(const map_mask &abyss_genlevel_mask)
{
    // Any rune on the floor prevents the abyssal rune from being generated.
    const bool placed_abyssal_rune =
        find_floor_item(OBJ_MISCELLANY, MISC_RUNE_OF_ZOT);

#ifdef DEBUG_ABYSS
    dprf("_generate_area(). turns_on_level: %d, rune_on_floor: %s",
         env.turns_on_level, placed_abyssal_rune? "yes" : "no");
#endif

    // Nuke map knowledge.
    env.map_knowledge.init(map_cell());
    _abyss_apply_terrain(abyss_genlevel_mask);

    bool use_vaults = (you.char_direction == GDT_GAME_START ? false : true);

    if (use_vaults)
        _abyss_place_vaults(abyss_genlevel_mask);
    _abyss_create_items(abyss_genlevel_mask, placed_abyssal_rune, use_vaults);
    generate_random_blood_spatter_on_level(&abyss_genlevel_mask);
    setup_environment_effects();

    // Abyss has a constant density.
    env.density = 0;
}

static void abyss_area_shift(void)
{
#ifdef DEBUG_ABYSS
    dprf("area_shift() - player at pos (%d, %d)",
         you.pos().x, you.pos().y);
#endif

    {
        xom_abyss_feature_amusement_check xomcheck;

        // Use a map mask to track the areas that the shift destroys and
        // that must be regenerated by _generate_area.
        map_mask abyss_genlevel_mask;
        _abyss_shift_level_contents_around_player(
            ABYSS_AREA_SHIFT_RADIUS, ABYSS_CENTRE, abyss_genlevel_mask);
        forget_map(true);
        _generate_area(abyss_genlevel_mask);

        // Update LOS at player's new abyssal vacation retreat.
        los_changed();
    }

    // Place some monsters to keep the abyss party going.
    _abyss_generate_monsters(15);

    // And allow monsters in transit another chance to return.
    place_transiting_monsters();
    place_transiting_items();
}

static void _initialize_abyss_state()
{
    abyssal_state.major_coord.x = random2(0x7FFFFFFF);
    abyssal_state.major_coord.y = random2(0x7FFFFFFF);
    abyssal_state.phase = 0.0;
    abyssal_state.depth = 0.0;
}

static uint8_t _roll_abyss_floor_colour()
{
    return random_choose_weighted(
         108, BLUE,
         632, GREEN,
         // no CYAN (silence)
         932, RED,
         488, MAGENTA,
         433, BROWN,
        3438, LIGHTGRAY,
         // no DARKGREY (out of LOS)
         766, LIGHTBLUE,
         587, LIGHTGREEN,
         794, LIGHTCYAN,
         566, LIGHTRED,
         313, LIGHTMAGENTA,
         // no YELLOW (halo)
         890, WHITE,
          50, ETC_FIRE,
    0);
}

static uint8_t _roll_abyss_rock_colour()
{
    return random_choose_weighted(
         130, BLUE,
         409, GREEN,
         // no CYAN (metal)
         770, RED,
         522, MAGENTA,
        1292, BROWN,
         // no LIGHTGRAY (stone)
         // no DARKGREY (out of LOS)
         570, LIGHTBLUE,
         705, LIGHTGREEN,
         952, LIGHTCYAN,
        1456, LIGHTRED,
         377, LIGHTMAGENTA,
         105, YELLOW,
         101, WHITE,
          60, ETC_FIRE,
    0);
}

static void _abyss_generate_new_area()
{
    _initialize_abyss_state();
    remove_sanctuary(false);

    env.floor_colour = _roll_abyss_floor_colour();
    env.rock_colour = _roll_abyss_rock_colour();
    tile_init_flavour();

    map_mask abyss_genlevel_mask(false);
    _abyss_wipe_unmasked_area(abyss_genlevel_mask);
    dgn_erase_unused_vault_placements();
    ASSERT(env.cloud_no == 0);

    you.moveto(ABYSS_CENTRE);
    abyss_genlevel_mask.init(true);
    _generate_area(abyss_genlevel_mask);
    grd(you.pos()) = DNGN_FLOOR;
    if (one_chance_in(5))
    {
        _place_feature_near(you.pos(), LOS_RADIUS,
                            DNGN_FLOOR, DNGN_ALTAR_LUGONU, 50);
    }

    los_changed();
    place_transiting_monsters();
    place_transiting_items();
}

// Ensure that there is a path between the abyss centre and an exit location,
// by morphing the abyss until there is.
void _abyss_make_path(const coord_def &to)
{
    const int ntries = 30;  // Rarely do we need more than one.
    for (int i = 1; i <= ntries; ++i)
    {
        ASSERT(grd(to) == DNGN_EXIT_ABYSS);

        monster_pathfind pf;
        if (pf.init_pathfind(ABYSS_CENTRE, to))
        {
            if (i > 1)
                dprf("_abyss_make_path needed %d attempts", i);
            return;
        }

        // Try to morph.
        const double old_depth = abyssal_state.depth;
        abyssal_state.depth += 0.1;
        map_mask abyss_genlevel_mask;
        _abyss_invert_mask(&abyss_genlevel_mask);

        // Assumes that abyss morphing won't remove the exit.
        _abyss_apply_terrain(abyss_genlevel_mask, true, old_depth);
    }
    die("Could not create path to exit at (%d, %d) after %d attempts.",
        to.x, to.y, ntries);
}

// Generate the initial (proto) Abyss level. The proto Abyss is where
// the player lands when they arrive in the Abyss from elsewhere.
// _generate_area generates all other Abyss areas.
void generate_abyss()
{
    env.level_build_method += " abyss";
    env.level_layout_types.insert("abyss");

    _initialize_abyss_state();

    dprf("generate_abyss(); turn_on_level: %d", env.turns_on_level);

    // Generate the initial abyss without vaults. Vaults are horrifying.
    _abyss_generate_new_area();
    _write_abyssal_features();
    map_mask abyss_genlevel_mask;
    _abyss_invert_mask(&abyss_genlevel_mask);
    _abyss_apply_terrain(abyss_genlevel_mask);

    // If we're starting out in the Abyss, make sure the starting grid is
    // an altar to Lugonu and there's an exit near-by.
    // Otherwise, we start out on floor and there's a chance there's an
    // altar near-by.
    if (you.char_direction == GDT_GAME_START)
    {
        grd(ABYSS_CENTRE) = DNGN_ALTAR_LUGONU;
        const coord_def eloc = _place_feature_near(ABYSS_CENTRE, LOS_RADIUS + 2,
                                                   DNGN_FLOOR, DNGN_EXIT_ABYSS,
                                                   50, true);
        // Now make sure there is a path from the abyss centre to the exit.
        // If for some reason an exit could not be placed, don't bother.
        if (eloc != INVALID_COORD)
            _abyss_make_path(eloc);
    }
    else
    {
        grd(ABYSS_CENTRE) = DNGN_FLOOR;
        if (one_chance_in(5))
        {
            _place_feature_near(ABYSS_CENTRE, LOS_RADIUS,
                                DNGN_FLOOR, DNGN_ALTAR_LUGONU, 50);
        }
    }

    generate_random_blood_spatter_on_level();
    setup_environment_effects();
}

void abyss_morph(double duration)
{
    if (you.level_type != LEVEL_ABYSS)
        return;

    // Between .02 and .07 per ten ticks, half that for Chei worshippers.
    double delta_t = you.time_taken * (you.abyss_speed + 40.0) / 20000.0;
    if (you.religion == GOD_CHEIBRIADOS)
        delta_t /= 2.0;

    const double theta = abyssal_state.phase;
    const double old_depth = abyssal_state.depth;

    // Up to 3 times the old rate of change, as low as 1/5, with an average of
    // 89%.  Period of 2*pi, so from 90 to 314 turns depending on abyss speed
    // (double for Chei worshippers).  Starts in the middle of a cool period.
    // Increasing the exponent reduces the lengths of the unstable periods.  It
    // should be an even integer.
    abyssal_state.depth += delta_t * (0.2 + 2.8 * pow(sin(theta/2), 10.0));

    // Phase mod pi.
    abyssal_state.phase += delta_t;
    if (abyssal_state.phase > M_PI)
        abyssal_state.phase -= M_PI;

    map_mask abyss_genlevel_mask;
    _abyss_invert_mask(&abyss_genlevel_mask);
    dgn_erase_unused_vault_placements();
    _abyss_apply_terrain(abyss_genlevel_mask, true, old_depth);
    _place_displaced_monsters();
    _push_items();
    los_changed();
}

void abyss_teleport(bool new_area)
{
    xom_abyss_feature_amusement_check xomcheck;

    if (!new_area && _abyss_teleport_within_level())
        return;

#ifdef DEBUG_ABYSS
    dprf("New area Abyss teleport.");
#endif

    // Teleport to a new area of the abyss.
    _abyss_generate_new_area();
}

//////////////////////////////////////////////////////////////////////////////
// Abyss effects in other levels, courtesy Lugonu.

struct corrupt_env
{
    int rock_colour, floor_colour;
    corrupt_env(): rock_colour(BLACK), floor_colour(BLACK) { }
};

static void _place_corruption_seed(const coord_def &pos, int duration)
{
    env.markers.add(new map_corruption_marker(pos, duration));
    // Corruption markers don't need activation, though we might
    // occasionally miss other unactivated markers by clearing.
    env.markers.clear_need_activate();
}

static void _initialise_level_corrupt_seeds(int power)
{
    const int low = power * 40 / 100, high = power * 140 / 100;
    const int nseeds = random_range(-1, std::min(2 + power / 110, 4), 2);

    const int aux_seed_radius = 4;

    dprf("Placing %d corruption seeds (power: %d)", nseeds, power);

    // The corruption centreed on the player is free.
    _place_corruption_seed(you.pos(), high + 300);

    for (int i = 0; i < nseeds; ++i)
    {
        coord_def where;
        int tries = 100;
        while (tries-- > 0)
        {
            where = dgn_random_point_from(you.pos(), aux_seed_radius, 2);
            if (grd(where) == DNGN_FLOOR && !env.markers.find(where, MAT_ANY))
                break;
            where.reset();
        }

        if (!where.origin())
            _place_corruption_seed(where, random_range(low, high, 2) + 300);
    }
}

// Create a corruption spawn at the given position. Returns false if further
// monsters should not be placed near this spot (overcrowding), true if
// more monsters can fit in.
static bool _spawn_corrupted_servant_near(const coord_def &pos)
{
    const beh_type beh =
        x_chance_in_y(100, 200 + you.skill(SK_INVOCATIONS, 25)) ? BEH_HOSTILE
        : BEH_NEUTRAL;

    // [ds] No longer summon hostiles -- don't create the monster if
    // it would be hostile.
    if (beh == BEH_HOSTILE)
        return (true);

    // Thirty tries for a place.
    for (int i = 0; i < 30; ++i)
    {
        const int offsetX = random2avg(4, 3) + random2(3);
        const int offsetY = random2avg(4, 3) + random2(3);
        const coord_def p(pos.x + (coinflip()? offsetX : -offsetX),
                          pos.y + (coinflip()? offsetY : -offsetY));
        if (!in_bounds(p) || actor_at(p)
            || !feat_compatible(DNGN_FLOOR, grd(p)))
        {
            continue;
        }

        // Got a place, summon the beast.
        monster_type mons = pick_random_monster(level_id(LEVEL_ABYSS));
        if (invalid_monster_type(mons))
            return (false);

        mgen_data mg(mons, beh, 0, 5, 0, p);
        mg.non_actor_summoner = "Lugonu's corruption";

        return create_monster(mg);
    }

    return (false);
}

static void _apply_corruption_effect(map_marker *marker, int duration)
{
    if (!duration)
        return;

    map_corruption_marker *cmark = dynamic_cast<map_corruption_marker*>(marker);
    if (cmark->duration < 1)
        return;

    const int neffects = std::max(div_rand_round(duration, 5), 1);

    for (int i = 0; i < neffects; ++i)
    {
        if (x_chance_in_y(cmark->duration, 4000)
            && !_spawn_corrupted_servant_near(cmark->pos))
        {
            break;
        }
    }
    cmark->duration -= duration;
}

void run_corruption_effects(int duration)
{
    std::vector<map_marker*> markers =
        env.markers.get_all(MAT_CORRUPTION_NEXUS);

    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        map_marker *mark = markers[i];
        if (mark->get_type() != MAT_CORRUPTION_NEXUS)
            continue;

        _apply_corruption_effect(mark, duration);
    }
}

static bool _is_grid_corruptible(const coord_def &c)
{
    if (c == you.pos())
        return (false);

    const dungeon_feature_type feat = grd(c);

    // Stairs and portals cannot be corrupted.
    if (feat_stair_direction(feat) != CMD_NO_CMD)
        return (false);

    switch (feat)
    {
    case DNGN_PERMAROCK_WALL:
    case DNGN_CLEAR_PERMAROCK_WALL:
        return (false);

    case DNGN_METAL_WALL:
    case DNGN_GREEN_CRYSTAL_WALL:
        return (one_chance_in(4));

    case DNGN_STONE_WALL:
    case DNGN_CLEAR_STONE_WALL:
        return (one_chance_in(3));

    case DNGN_ROCK_WALL:
    case DNGN_CLEAR_ROCK_WALL:
        return (!one_chance_in(3));

    default:
        return (true);
    }
}

// Returns true if the square has <= 4 traversable neighbours.
static bool _is_crowded_square(const coord_def &c)
{
    int neighbours = 0;
    for (int xi = -1; xi <= 1; ++xi)
        for (int yi = -1; yi <= 1; ++yi)
        {
            if (!xi && !yi)
                continue;

            const coord_def n(c.x + xi, c.y + yi);
            if (!in_bounds(n) || !feat_is_traversable(grd(n)))
                continue;

            if (++neighbours > 4)
                return (false);
        }

    return (true);
}

// Returns true if the square has all opaque neighbours.
static bool _is_sealed_square(const coord_def &c)
{
    for (adjacent_iterator ai(c); ai; ++ai)
        if (!feat_is_opaque(grd(*ai)))
            return (false);

    return (true);
}

static void _corrupt_square(const corrupt_env &cenv, const coord_def &c)
{
    // To prevent the destruction of, say, branch entries.
    bool preserve_feat = true;
    dungeon_feature_type feat = DNGN_UNSEEN;
    if (feat_altar_god(grd(c)) != GOD_NO_GOD)
    {
        // altars may be safely overwritten, ha!
        preserve_feat = false;
        if (!one_chance_in(3))
            feat = DNGN_ALTAR_LUGONU;
    }
    else
        feat = _abyss_proto_feature();

    if (feat_is_trap(feat, true)
        || feat == DNGN_SECRET_DOOR
        || feat == DNGN_UNSEEN
        || (feat_is_traversable(grd(c)) && !feat_is_traversable(feat)
            && coinflip()))
    {
        feat = DNGN_FLOOR;
    }

    if (feat_is_traversable(grd(c)) && !feat_is_traversable(feat)
        && _is_crowded_square(c))
    {
        return;
    }

    if (!feat_is_traversable(grd(c)) && feat_is_traversable(feat)
        && _is_sealed_square(c))
    {
        return;
    }

    if (feat == DNGN_EXIT_ABYSS)
        feat = DNGN_ENTER_ABYSS;

    dungeon_terrain_changed(c, feat, true, preserve_feat, true);
    if (feat == DNGN_ROCK_WALL)
        env.grid_colours(c) = cenv.rock_colour;
    else if (feat == DNGN_FLOOR)
        env.grid_colours(c) = cenv.floor_colour;

    if (feat == DNGN_ROCK_WALL)
    {
        env.tile_flv(c).wall = TILE_WALL_UNDEAD
            + random2(tile_dngn_count(TILE_WALL_UNDEAD));
    }
    else if (feat == DNGN_FLOOR)
    {
        env.tile_flv(c).floor = TILE_FLOOR_NERVES
            + random2(tile_dngn_count(TILE_FLOOR_NERVES));
    }
}

static void _corrupt_level_features(const corrupt_env &cenv)
{
    std::vector<coord_def> corrupt_seeds;
    std::vector<map_marker*> corrupt_markers =
        env.markers.get_all(MAT_CORRUPTION_NEXUS);

    for (int i = 0, size = corrupt_markers.size(); i < size; ++i)
        corrupt_seeds.push_back(corrupt_markers[i]->pos);

    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        int idistance2 = GXM * GXM + GYM * GYM;
        for (int i = 0, size = corrupt_seeds.size(); i < size; ++i)
        {
            const int idist2 = (*ri - corrupt_seeds[i]).abs();
            if (idist2 < idistance2)
                idistance2 = idist2;
        }

        const int ground_zero_radius2 = 7;

        // Corruption odds are 100% within about 2 squares, decaying to 30%
        // at LOS range (radius 8). Even if the corruption roll is made,
        // the feature still gets a chance to resist if it's a wall.
        const int corrupt_perc_chance =
            idistance2 <= ground_zero_radius2 ? 100 :
            std::max(1, 100 - (idistance2 - ground_zero_radius2) * 70 / 57);

        if (random2(100) < corrupt_perc_chance && _is_grid_corruptible(*ri))
            _corrupt_square(cenv, *ri);
    }
}

static bool _is_level_corrupted()
{
    if (player_in_level_area(LEVEL_ABYSS))
        return (true);

    return (!!env.markers.find(MAT_CORRUPTION_NEXUS));
}

bool is_level_incorruptible()
{
    if (_is_level_corrupted())
    {
        mpr(gettext("This place is already infused with evil and corruption."));
        return (true);
    }

    return (false);
}

static void _corrupt_choose_colours(corrupt_env *cenv)
{
    int colour = BLACK;
    do
        colour = random_uncommon_colour();
    while (colour == env.rock_colour || colour == LIGHTGREY || colour == WHITE);
    cenv->rock_colour = colour;

    do
        colour = random_uncommon_colour();
    while (colour == env.floor_colour || colour == LIGHTGREY
           || colour == WHITE);
    cenv->floor_colour = colour;
}

bool lugonu_corrupt_level(int power)
{
    if (is_level_incorruptible())
        return (false);

    /// 아마도 lugonu의 것이겠지...
    simple_god_message(gettext("'s Hand of Corruption reaches out!"));

    flash_view(MAGENTA);

    _initialise_level_corrupt_seeds(power);

    corrupt_env cenv;
    _corrupt_choose_colours(&cenv);
    _corrupt_level_features(cenv);
    run_corruption_effects(300);

#ifndef USE_TILE_LOCAL
    // Allow extra time for the flash to linger.
    delay(1000);
#endif

    return (true);
}
