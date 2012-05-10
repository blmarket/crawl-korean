/**
 * @file
 * @brief User-accessible monster info.
**/

#include "AppHdr.h"

#include "l_libs.h"

#include "cluautil.h"
#include "coord.h"
#include "env.h"
#include "libutil.h"
#include "mon-info.h"
#include "player.h"

#define MONINF_METATABLE "monster.info"

void lua_push_moninf(lua_State *ls, monster_info *mi)
{
    monster_info **miref =
        clua_new_userdata<monster_info *>(ls, MONINF_METATABLE);
    *miref = new monster_info(*mi);
}

#define MONINF(ls, n, var) \
    monster_info *var = *(monster_info **) \
        luaL_checkudata(ls, n, MONINF_METATABLE)

#define MIRET1(type, field, cfield) \
    static int moninf_get_##field(lua_State *ls) \
    { \
        MONINF(ls, 1, mi); \
        lua_push##type(ls, mi->cfield); \
        return (1); \
    }

#define MIREG(field) { #field, moninf_get_##field }

MIRET1(number, damage_level, dam)
MIRET1(boolean, is_safe, is(MB_SAFE))
MIRET1(boolean, is_firewood, is(MB_FIREWOOD))
MIRET1(number, holiness, holi)
MIRET1(number, attitude, attitude)
MIRET1(number, threat, threat)
MIRET1(string, mname, mname.c_str())
MIRET1(number, type, type)
MIRET1(number, base_type, base_type)
MIRET1(number, number, number)
MIRET1(number, colour, colour)

LUAFN(moninf_get_is)
{
    MONINF(ls, 1, mi);
    int num = luaL_checknumber(ls, 2);
    if (num < 0 || num >= NUM_MB_FLAGS)
    {
        luaL_argerror(ls, 2, "mb:is() out of bounds");
        return 0;
    }
    lua_pushboolean(ls, mi->is(num));
    return (1);
}

static bool cant_see_you(const monster_info *mi)
{
    if (mons_class_flag(mi->type, M_SEE_INVIS))
        return false;
    if (mons_class_flag(mi->type, M_SENSE_INVIS)
        && (you.pos() - mi->pos).abs() <= 17)
    {
        return false;
    }
    if (you.in_water())
        return false;
    return you.invisible() || mi->is(MB_BLIND);
}

LUAFN(moninf_get_stabbability)
{
    MONINF(ls, 1, mi);
    if (mi->is(MB_DORMANT) || mi->is(MB_SLEEPING) || mi->is(MB_PARALYSED))
        lua_pushnumber(ls, 1.0);
    else if (mi->is(MB_CAUGHT) || mi->is(MB_WEBBED) || mi->is(MB_PETRIFYING)
             || mi->is(MB_PETRIFIED))
    {
        lua_pushnumber(ls, 0.5);
    }
    else if (mi->is(MB_CONFUSED) || mi->is(MB_FLEEING) || cant_see_you(mi))
        lua_pushnumber(ls, 0.25);
    else if (mi->is(MB_DISTRACTED))
        lua_pushnumber(ls, 0.16666666);
    else
        lua_pushnumber(ls, 0);

    return (1);
}

LUAFN(moninf_get_is_constricted)
{
    MONINF(ls, 1, mi);
    lua_pushboolean(ls, !mi->constrictor_name.empty());
    return (1);
}

LUAFN(moninf_get_is_constricting)
{
    MONINF(ls, 1, mi);
    bool any = false;
    for (int i = 0; i < MAX_CONSTRICT; i++)
        if (!mi->constricting_name[i].empty())
        {
            any = true;
            break;
        }
    lua_pushboolean(ls, any);
    return (1);
}

LUAFN(moninf_get_is_constricting_you)
{
    MONINF(ls, 1, mi);
    if (!you.is_constricted())
    {
        lua_pushboolean(ls, false);
        return (1);
    }
    for (int i = 0; i < MAX_CONSTRICT; i++)
        if (mi->constricting_name[i] == "you") // yay the interface
        {
            lua_pushboolean(ls, true);
            return (1);
        }
    lua_pushboolean(ls, false);
    return (1);
}

LUAFN(moninf_get_is_unique)
{
    MONINF(ls, 1, mi);
    // XXX: A bit of a hack to prevent using this to determine which is fake.
    if (mi->type == MONS_MARA_FAKE)
        lua_pushboolean(ls, true);
    else
        lua_pushboolean(ls, mons_is_unique(mi->type));
    return (1);
}


LUAFN(moninf_get_damage_desc)
{
    MONINF(ls, 1, mi);
    std::string s = mi->damage_desc();
    lua_pushstring(ls, s.c_str());
    return (1);
}

LUAFN(moninf_get_desc)
{
    MONINF(ls, 1, mi);
    std::string desc;
    int col;
    mi->to_string(1, desc, col);
    lua_pushstring(ls, desc.c_str());
    return (1);
}

static const struct luaL_reg moninf_lib[] =
{
    MIREG(type),
    MIREG(base_type),
    MIREG(number),
    MIREG(colour),
    MIREG(mname),
    MIREG(is),
    MIREG(is_safe),
    MIREG(is_firewood),
    MIREG(stabbability),
    MIREG(holiness),
    MIREG(attitude),
    MIREG(threat),
    MIREG(is_constricted),
    MIREG(is_constricting),
    MIREG(is_constricting_you),
    MIREG(is_unique),
    MIREG(damage_level),
    MIREG(damage_desc),
    MIREG(desc),

    { NULL, NULL }
};

// XXX: unify with directn.cc/h
// This uses relative coordinates with origin the player.
bool in_show_bounds(const coord_def &s)
{
    return (s.rdist() <= ENV_SHOW_OFFSET);
}

LUAFN(mi_get_monster_at)
{
    COORDSHOW(s, 1, 2)
    coord_def p = player2grid(s);
    if (!you.see_cell(p))
        return (0);
    if (env.mgrid(p) == NON_MONSTER)
        return (0);
    monster* m = &env.mons[env.mgrid(p)];
    if (!m->visible_to(&you))
        return (0);
    monster_info mi(m);
    lua_push_moninf(ls, &mi);
    return (1);
}

static const struct luaL_reg mon_lib[] =
{
    { "get_monster_at", mi_get_monster_at },

    { NULL, NULL }
};

void cluaopen_moninf(lua_State *ls)
{
    clua_register_metatable(ls, MONINF_METATABLE, moninf_lib,
                            lua_object_gc<monster_info>);
    luaL_openlib(ls, "monster", mon_lib, 0);
}
