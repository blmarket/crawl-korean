/**
 * @file
 * @brief Rendering of map_cell to glyph and colour.
 *
 * This only needs the information within one object of type map_cell.
**/

#include "AppHdr.h"

#include "showsymb.h"

#include "colour.h"
#include "env.h"
#include "libutil.h"
#include "map_knowledge.h"
#include "mon-util.h"
#include "monster.h"
#include "options.h"
#include "show.h"
#include "state.h"
#include "terrain.h"
#include "viewchar.h"
#include "viewgeom.h"
#include "coord.h"

static
unsigned short _cell_feat_show_colour(const map_cell& cell, bool coloured)
{
    dungeon_feature_type feat = cell.feat();
    unsigned short colour = BLACK;
    const feature_def &fdef = get_feature_def(feat);

    // These aren't shown mossy/bloody/slimy.
    const bool norecolour = is_critical_feature(feat) || feat_is_trap(feat);

    if (!coloured)
    {
        if (cell.flags & MAP_EMPHASIZE)
            colour = fdef.seen_em_colour;
        else
            colour = fdef.seen_colour;

        if (colour)
            return colour;
    }

    else if (cell.flags & MAP_EXCLUDED_STAIRS)
        colour = Options.tc_excluded;
    else if (feat >= DNGN_MINMOVE && cell.flags & MAP_WITHHELD)
    {
        // Colour grids that cannot be reached due to beholders
        // dark grey.
        colour = DARKGREY;
    }
    else if (feat >= DNGN_MINMOVE
             && (cell.flags & (MAP_SANCTUARY_1 | MAP_SANCTUARY_2)))
    {
        if (cell.flags & MAP_SANCTUARY_1)
            colour = YELLOW;
        else if (cell.flags & MAP_SANCTUARY_2)
        {
            if (!one_chance_in(4))
                colour = WHITE;     // 3/4
            else if (!one_chance_in(3))
                colour = LIGHTCYAN; // 1/6
            else
                colour = LIGHTGREY; // 1/12
        }
    }
    else if (cell.flags & MAP_BLOODY && !norecolour)
        colour = RED;
    else if (cell.flags & MAP_MOLDY && !norecolour)
        colour = (cell.flags & MAP_GLOWING_MOLDY) ? LIGHTRED : LIGHTGREEN;
    else if (cell.flags & MAP_CORRODING && !norecolour)
        colour = LIGHTGREEN;
    else if (cell.feat_colour())
        colour = cell.feat_colour();
    else
    {
        colour = fdef.colour;

        if (fdef.em_colour && fdef.em_colour != fdef.colour
            && cell.flags & MAP_EMPHASIZE)
        {
            colour = fdef.em_colour;
        }
    }

    if (feat == DNGN_SHALLOW_WATER && player_in_branch(BRANCH_SHOALS))
        colour = ETC_WAVES;

    if (feat_has_solid_floor(feat) && !feat_is_water(feat)
        && cell.flags & MAP_LIQUEFIED)
    {
        colour = ETC_LIQUEFIED;
    }

    if (feat >= DNGN_FLOOR_MIN && feat <= DNGN_FLOOR_MAX)
    {
        if (cell.flags & MAP_HALOED)
        {
            if (cell.flags & MAP_SILENCED && cell.flags & MAP_UMBRAED)
                colour = CYAN; // Default for silence.
            else if (cell.flags & MAP_SILENCED)
                colour = LIGHTCYAN;
            else if (cell.flags & MAP_UMBRAED)
                colour = fdef.colour; // Cancels out!
            else
                colour = YELLOW;
        }
        else if (cell.flags & MAP_UMBRAED)
        {
           if (cell.flags & MAP_SILENCED)
                colour = BLUE; // Silence gets darker
           else
                colour = ETC_DEATH; // If no holy or silence
        }
        else if (cell.flags & MAP_SILENCED)
            colour = CYAN; // Silence but no holy/unholy
        else if (cell.flags & MAP_ORB_HALOED)
            colour = ETC_ORB_GLOW;
    }
    return (colour);
}

static int _get_mons_colour(const monster_info& mi)
{
    int col = mi.colour;

    if (mi.type == MONS_SLIME_CREATURE && mi.number > 1)
        col = mons_class_colour(MONS_MERGED_SLIME_CREATURE);

    if (mi.is(MB_BERSERK))
        col = RED;

    if (mi.is(MB_MIRROR_DAMAGE))
        col = ETC_DEATH;

    if (mi.is(MB_INNER_FLAME))
        col = ETC_FIRE;

    if (mi.attitude == ATT_FRIENDLY)
    {
        col |= COLFLAG_FRIENDLY_MONSTER;
    }
    else if (mi.attitude != ATT_HOSTILE)
    {
        col |= COLFLAG_NEUTRAL_MONSTER;
    }
    else if (Options.stab_brand != CHATTR_NORMAL
             && mi.is(MB_STABBABLE))
    {
        col |= COLFLAG_WILLSTAB;
    }
    else if (Options.may_stab_brand != CHATTR_NORMAL
             && mi.is(MB_DISTRACTED))
    {
        col |= COLFLAG_MAYSTAB;
    }
    else if (mons_class_is_stationary(mi.type))
    {
        if (Options.feature_item_brand != CHATTR_NORMAL
            && is_critical_feature(grd(mi.pos))
            && feat_stair_direction(grd(mi.pos)) != CMD_NO_CMD)
        {
            col |= COLFLAG_FEATURE_ITEM;
        }
        else if (Options.heap_brand != CHATTR_NORMAL
                 && you.visible_igrd(mi.pos) != NON_ITEM
                 && !crawl_state.game_is_arena())
        {
            col |= COLFLAG_ITEM_HEAP;
        }
    }

    // Backlit monsters are fuzzy and override brands.
    if (!crawl_state.game_is_arena() &&
        !you.can_see_invisible() && mi.is(MB_INVISIBLE))
    {
        col = DARKGREY;
    }

    return (col);
}

show_class get_cell_show_class(const map_cell& cell,
                               bool only_stationary_monsters)
{
    if (cell.invisible_monster())
        return SH_INVIS_EXPOSED;

    if (cell.monster() != MONS_NO_MONSTER
        && (!only_stationary_monsters
            || mons_class_is_stationary(cell.monster())))
    {
        return SH_MONSTER;
    }

    if (cell.cloud() != CLOUD_NONE && cell.cloud() != CLOUD_GLOOM)
        return SH_CLOUD;

    if (feat_is_trap(cell.feat())
     || is_critical_feature(cell.feat())
     || cell.feat() < DNGN_MINMOVE)
    {
        return SH_FEATURE;
    }

    if (cell.item())
        return SH_ITEM;

    if (cell.feat())
        return SH_FEATURE;

    return SH_NOTHING;
}

static const unsigned short ripple_table[] =
    {BLUE,          // BLACK        => BLUE (default)
     BLUE,          // BLUE         => BLUE
     GREEN,         // GREEN        => GREEN
     CYAN,          // CYAN         => CYAN
     RED,           // RED          => RED
     MAGENTA,       // MAGENTA      => MAGENTA
     BROWN,         // BROWN        => BROWN
     DARKGREY,      // LIGHTGREY    => DARKGREY
     DARKGREY,      // DARKGREY     => DARKGREY
     BLUE,          // LIGHTBLUE    => BLUE
     GREEN,         // LIGHTGREEN   => GREEN
     BLUE,          // LIGHTCYAN    => BLUE
     RED,           // LIGHTRED     => RED
     MAGENTA,       // LIGHTMAGENTA => MAGENTA
     BROWN,         // YELLOW       => BROWN
     LIGHTGREY};    // WHITE        => LIGHTGREY

static glyph _get_cell_glyph_with_class(const map_cell& cell,
                                        const coord_def& loc,
                                        const show_class cls,
                                        int colour_mode)
{
    const bool coloured = colour_mode == 0 ? cell.visible() : (colour_mode > 0);
    glyph g;
    show_type show;

    const cloud_type cell_cloud = cell.cloud();
    const bool gloom = cell_cloud == CLOUD_GLOOM;

    if (gloom)
    {
        if (coloured)
            g.col = cell.cloud_colour();
        else
            g.col = DARKGREY;
    }

    switch (cls)
    {
    case SH_INVIS_EXPOSED:
        if (!cell.invisible_monster())
            return g;

        show.cls = SH_INVIS_EXPOSED;
        if (cell_cloud != CLOUD_NONE)
            g.col = cell.cloud_colour();
        else
            g.col = ripple_table[cell.feat_colour() & 0xf];
        break;

    case SH_MONSTER:
        if (cell.monster() == MONS_NO_MONSTER)
            return g;

        show = cell.monster();
        if (cell.detected_monster())
        {
            const monster_info* mi = cell.monsterinfo();
            ASSERT(mi);
            ASSERT(mi->type == MONS_SENSED);
            if (mons_is_sensed(mi->base_type))
                g.col = mons_class_colour(mi->base_type);
            else
                g.col = Options.detected_monster_colour;
        }
        else if (!coloured)
            g.col = DARKGRAY;
        else
        {
            const monster_info* mi = cell.monsterinfo();
            ASSERT(mi);
            g.col = _get_mons_colour(*mi);
        }
        break;

    case SH_CLOUD:
        if (!cell_cloud)
            return g;

        show.cls = SH_CLOUD;
        if (coloured)
            g.col = cell.cloud_colour();
        else
            g.col = DARKGRAY;
        break;

    case SH_FEATURE:
        if (!cell.feat())
            return g;

        show = cell.feat();

        if (!gloom)
            g.col = _cell_feat_show_colour(cell, coloured);

        if (cell.item())
        {
            if (Options.feature_item_brand
                && (is_critical_feature(cell.feat())
                 || cell.feat() < DNGN_MINMOVE))
            {
                g.col |= COLFLAG_FEATURE_ITEM;
            }
            else if (Options.trap_item_brand && feat_is_trap(cell.feat()))
                g.col |= COLFLAG_TRAP_ITEM;
        }
        break;

    case SH_ITEM:
        if (cell.item())
        {
            const item_info* eitem = cell.item();
            show = *eitem;

            if (!gloom)
            {
                if (!feat_is_water(cell.feat()))
                    g.col = eitem->colour;
                else
                    g.col = _cell_feat_show_colour(cell, coloured);

                // monster(mimic)-owned items have link = NON_ITEM+1+midx
                if (cell.flags & MAP_MORE_ITEMS)
                    g.col |= COLFLAG_ITEM_HEAP;
            }
        }
        else
            return g;
        break;

    case SH_NOTHING:
    case NUM_SHOW_CLASSES:
    default:
        return g;
    }

    if (cls == SH_MONSTER)
    {
        const monster_info* mi = cell.monsterinfo();
        const bool override = Options.mon_glyph_overrides.find(mi->type)
                              != Options.mon_glyph_overrides.end();
        if (mi->props.exists("glyph") && !override)
            g.ch = mi->props["glyph"].get_int();
        else if (show.mons == MONS_SENSED)
            g.ch = mons_char(mi->base_type);
        else
            g.ch = mons_char(show.mons);

        if (mi->props.exists("glyph") && override)
            g.col = mons_class_colour(mi->type);
    }
    else
    {
        const feature_def &fdef = get_feature_def(show);
        g.ch = cell.seen() ? fdef.symbol : fdef.magic_symbol;
    }

    if (g.col)
        g.col = real_colour(g.col, loc);

    return g;
}

glyph get_cell_glyph(const coord_def& loc, bool only_stationary_monsters,
                     int colour_mode)
{
    const map_cell& cell = env.map_knowledge(loc);
    const show_class cell_show_class =
        get_cell_show_class(cell, only_stationary_monsters);
    return _get_cell_glyph_with_class(cell, loc, cell_show_class, colour_mode);
}

ucs_t get_feat_symbol(dungeon_feature_type feat)
{
    return (get_feature_def(feat).symbol);
}

ucs_t get_item_symbol(show_item_type it)
{
    return (get_feature_def(show_type(it)).symbol);
}

glyph get_item_glyph(const item_def *item)
{
    glyph g;
    g.ch = get_feature_def(show_type(*item)).symbol;
    g.col = item->colour;
    return (g);
}

glyph get_mons_glyph(const monster_info& mi)
{
    glyph g;
    const bool override = Options.mon_glyph_overrides.find(mi.type)
                          != Options.mon_glyph_overrides.end();
    if (mi.props.exists("glyph") && !override)
        g.ch = mi.props["glyph"].get_int();
    else if (mi.type == MONS_SLIME_CREATURE && mi.number > 1)
        g.ch = mons_char(MONS_MERGED_SLIME_CREATURE);
    else if (mi.type == MONS_SENSED)
        g.ch = mons_char(mi.base_type);
    else
        g.ch = mons_char(mi.type);

    if (mi.props.exists("glyph") && override)
        g.col = mons_class_colour(mi.type);
    else
        g.col = _get_mons_colour(mi);
    g.col = real_colour(g.col);
    return (g);
}

std::string glyph_to_tagstr(const glyph& g)
{
    std::string col = colour_to_str(g.col);
    std::string ch = stringize_glyph(g.ch);
    if (g.ch == '<')
        ch += "<";
    return make_stringf("<%s>%s</%s>", col.c_str(), ch.c_str(), col.c_str());
}
