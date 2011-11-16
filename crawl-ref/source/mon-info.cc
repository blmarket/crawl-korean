/**
 * @file
 * @brief Monster information that may be passed to the user.
 *
 * Used to fill the monster pane and to pass monster info to Lua.
**/

#include "AppHdr.h"

#include "mon-info.h"

#include "areas.h"
#include "artefact.h"
#include "coord.h"
#include "env.h"
#include "fight.h"
#include "ghost.h"
#include "itemname.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "mon-iter.h"
#include "mon-util.h"
#include "monster.h"
#include "options.h"
#include "religion.h"
#include "showsymb.h"
#include "skills2.h"
#include "state.h"
#include "tagstring.h"
#include "terrain.h"

#include <algorithm>
#include <sstream>

static monster_info_flags ench_to_mb(const monster& mons, enchant_type ench)
{
    // Suppress silly-looking combinations, even if they're
    // internally valid.
    if (mons.paralysed() && (ench == ENCH_SLOW || ench == ENCH_HASTE
                      || ench == ENCH_SWIFT
                      || ench == ENCH_PETRIFIED
                      || ench == ENCH_PETRIFYING))
        return NUM_MB_FLAGS;

    if (ench == ENCH_HASTE && mons.has_ench(ENCH_SLOW))
        return NUM_MB_FLAGS;

    if (ench == ENCH_SLOW && mons.has_ench(ENCH_HASTE))
        return NUM_MB_FLAGS;

    if (ench == ENCH_PETRIFIED && mons.has_ench(ENCH_PETRIFYING))
        return NUM_MB_FLAGS;

    switch (ench)
    {
    case ENCH_BERSERK:
        return MB_BERSERK;
    case ENCH_POISON:
        return MB_POISONED;
    case ENCH_SICK:
        return MB_SICK;
    case ENCH_ROT:
        return MB_ROTTING;
    case ENCH_CORONA:
    case ENCH_SILVER_CORONA:
        return MB_GLOWING;
    case ENCH_SLOW:
        return MB_SLOWED;
    case ENCH_INSANE:
        return MB_INSANE;
    case ENCH_BATTLE_FRENZY:
        return MB_FRENZIED;
    case ENCH_ROUSED:
        return MB_ROUSED;
    case ENCH_HASTE:
        return MB_HASTED;
    case ENCH_MIGHT:
        return MB_STRONG;
    case ENCH_CONFUSION:
        return MB_CONFUSED;
    case ENCH_INVIS:
    {
        you.seen_invis = true;
        return MB_INVISIBLE;
    }
    case ENCH_CHARM:
        return MB_CHARMED;
    case ENCH_STICKY_FLAME:
        return MB_BURNING;
    case ENCH_HELD:
        return MB_CAUGHT;
    case ENCH_PETRIFIED:
        return MB_PETRIFIED;
    case ENCH_PETRIFYING:
        return MB_PETRIFYING;
    case ENCH_LOWERED_MR:
        return MB_VULN_MAGIC;
    case ENCH_SWIFT:
        return MB_SWIFT;
    case ENCH_SILENCE:
        return MB_SILENCING;
    case ENCH_PARALYSIS:
        return MB_PARALYSED;
    case ENCH_SOUL_RIPE:
        return MB_POSSESSABLE;
    case ENCH_PREPARING_RESURRECT:
        return MB_PREP_RESURRECT;
    case ENCH_FADING_AWAY:
        if ((mons.get_ench(ENCH_FADING_AWAY)).duration < 400) // min dur is 180*20, max dur 230*10
            return MB_MOSTLY_FADED;

        return MB_FADING_AWAY;
    case ENCH_REGENERATION:
        return MB_REGENERATION;
    case ENCH_RAISED_MR:
        return MB_RAISED_MR;
    case ENCH_MIRROR_DAMAGE:
        return MB_MIRROR_DAMAGE;
    case ENCH_FEAR_INSPIRING:
        return MB_FEAR_INSPIRING;
    case ENCH_WITHDRAWN:
        return MB_WITHDRAWN;
    case ENCH_ATTACHED:
        return MB_ATTACHED;
    case ENCH_BLEED:
        return MB_BLEEDING;
    case ENCH_DAZED:
        return MB_DAZED;
    case ENCH_MUTE:
        return MB_MUTE;
    case ENCH_BLIND:
        return MB_BLIND;
    case ENCH_DUMB:
        return MB_DUMB;
    case ENCH_MAD:
        return MB_MAD;
    case ENCH_INNER_FLAME:
        return MB_INNER_FLAME;
    case ENCH_BREATH_WEAPON:
        return MB_BREATH_WEAPON;
    case ENCH_DEATHS_DOOR:
        return MB_DEATHS_DOOR;
    default:
        return NUM_MB_FLAGS;
    }
}

static bool _blocked_ray(const coord_def &where,
                         dungeon_feature_type* feat = NULL)
{
    if (exists_ray(you.pos(), where) || !exists_ray(you.pos(), where, opc_default))
        return (false);
    if (feat == NULL)
        return (true);
    *feat = ray_blocker(you.pos(), where);
    return (true);
}

static bool _is_public_key(std::string key)
{
    if (key == "helpless"
     || key == "wand_known"
     || key == "feat_type"
     || key == "glyph"
     || key == "serpent_of_hell_flavour")
    {
        return true;
    }

    return false;
}

monster_info::monster_info(monster_type p_type, monster_type p_base_type)
{
    mb.reset();
    attitude = ATT_HOSTILE;
    pos = coord_def(0, 0);

    type = p_type;
    base_type = p_base_type;

    draco_type = mons_genus(type) == MONS_DRACONIAN ? MONS_DRACONIAN : type;

    number = 0;
    colour = mons_class_colour(type);

    holi = mons_class_holiness(type);

    mintel = mons_class_intel(type);

    mresists = get_mons_class_resists(type);

    mitemuse = mons_class_itemuse(type);

    mbase_speed = mons_class_base_speed(type);

    fly = mons_class_flies(type);
    if (fly == FL_NONE)
        fly = mons_class_flies(base_type);

    two_weapons = mons_class_wields_two_weapons(type);
    if (!two_weapons)
        two_weapons = mons_class_wields_two_weapons(base_type);

    no_regen = !mons_class_can_regenerate(type);
    if (!no_regen)
        no_regen = !mons_class_can_regenerate(base_type);

    threat = MTHRT_UNDEF;

    dam = MDAM_OKAY;

    fire_blocker = DNGN_UNSEEN;

    if (mons_is_pghost(type))
    {
        u.ghost.species = SP_HUMAN;
        u.ghost.job = JOB_WANDERER;
        u.ghost.religion = GOD_NO_GOD;
        u.ghost.best_skill = SK_FIGHTING;
        u.ghost.best_skill_rank = 2;
        u.ghost.xl_rank = 3;
    }

    if (base_type == MONS_NO_MONSTER)
        base_type = type;

    props.clear();

    client_id = 0;
}

monster_info::monster_info(const monster* m, int milev)
{
    mb.reset();
    attitude = ATT_HOSTILE;
    pos = grid2player(m->pos());

    attitude = mons_attitude(m);

    bool type_known = false;
    bool nomsg_wounds = false;

    if (m->props.exists("mislead_as") && you.misled())
    {
        type = m->get_mislead_type();
        threat = mons_threat_level(&m->props["mislead_as"].get_monster());
    }
    // friendly fake Rakshasas/Maras are known
    else if (attitude != ATT_FRIENDLY && m->props.exists("faking"))
    {
        type = m->props["faking"].get_monster().type;
        threat = mons_threat_level(&m->props["faking"].get_monster());
    }
    else
    {
        type_known = true;
        type = m->type;
        threat = mons_threat_level(m);
    }

    props.clear();
    if (!m->props.empty())
    {
        CrawlHashTable::hash_map_type::const_iterator i = m->props.begin();
        for (; i != m->props.end(); ++i)
            if (_is_public_key(i->first))
                props[i->first] = i->second;
    }

    if (type_known)
    {
        draco_type =
            mons_genus(type) == MONS_DRACONIAN ? ::draco_subspecies(m) : type;

        if (!mons_can_display_wounds(m)
            || !mons_class_can_display_wounds(type))
        {
            nomsg_wounds = true;
        }

        base_type = m->base_monster;
        if (base_type == MONS_NO_MONSTER)
            base_type = type;

        // these use number for internal information
        if (type == MONS_MANTICORE
            || type == MONS_SIXFIRHY
            || type == MONS_SHEDU
            || type == MONS_KRAKEN_TENTACLE
            || type == MONS_KRAKEN_TENTACLE_SEGMENT
            || type == MONS_ELDRITCH_TENTACLE_SEGMENT)
        {
            number = 0;
        }
        else
            number = m->number;
        colour = m->colour;

        if (m->is_summoned())
            mb.set(MB_SUMMONED);

        if (testbits(m->flags, MF_HARD_RESET) && testbits(m->flags, MF_NO_REWARD))
            mb.set(MB_PERM_SUMMON);
    }
    else
    {
        base_type = type;
        number = 0;
        colour = mons_class_colour(type);
    }

    if (mons_is_unique(type))
    {
        if (type == MONS_LERNAEAN_HYDRA
            || type == MONS_ROYAL_JELLY
            || type == MONS_SERPENT_OF_HELL)
        {
            mb.set(MB_NAME_THE);
        }
        else
        {
            mb.set(MB_NAME_UNQUALIFIED);
            mb.set(MB_NAME_THE);
        }
    }

    mname = m->mname;

    if ((m->flags & MF_NAME_MASK) == MF_NAME_SUFFIX)
        mb.set(MB_NAME_SUFFIX);
    else if ((m->flags & MF_NAME_MASK) == MF_NAME_ADJECTIVE)
        mb.set(MB_NAME_ADJECTIVE);
    else if ((m->flags & MF_NAME_MASK) == MF_NAME_REPLACE)
        mb.set(MB_NAME_REPLACE);

    const bool need_name_desc =
        (m->flags & MF_NAME_SUFFIX)
            || (m->flags & MF_NAME_ADJECTIVE)
            || (m->flags & MF_NAME_DEFINITE);

    if (!mname.empty()
        && !(m->flags & MF_NAME_DESCRIPTOR)
        && !need_name_desc)
    {
        mb.set(MB_NAME_UNQUALIFIED);
        mb.set(MB_NAME_THE);
    }
    else if (m->flags & MF_NAME_DEFINITE)
        mb.set(MB_NAME_THE);
    if (m->flags & MF_NAME_ZOMBIE)
        mb.set(MB_NAME_ZOMBIE);

    if (milev <= MILEV_NAME)
    {
        if (type_known && type == MONS_DANCING_WEAPON
            && m->inv[MSLOT_WEAPON] != NON_ITEM)
        {
            inv[MSLOT_WEAPON].reset(
                new item_def(get_item_info(mitm[m->inv[MSLOT_WEAPON]])));
        }
        if (type_known && mons_is_item_mimic(type))
        {
            ASSERT(m->inv[MSLOT_MISCELLANY] != NON_ITEM);
            inv[MSLOT_MISCELLANY].reset(
                new item_def(get_item_info(mitm[m->inv[MSLOT_MISCELLANY]])));
        }
        return;
    }

    holi = m->holiness();

    // don't give away mindlessness of monsters you're misled about
    if (type_known)
        mintel = mons_intel(m);
    else
        mintel = mons_class_intel(type);

    // don't give away resistances of monsters you're misled about
    if (type_known)
        mresists = get_mons_resists(m);
    else
        mresists = get_mons_class_resists(type);

    mitemuse = mons_itemuse(m);

    // don't give away base speed of monsters you're misled about
    if (type_known)
        mbase_speed = mons_base_speed(m);
    else
        mbase_speed = mons_class_base_speed(type);

    // consider randarts too, since flight should be visually obvious
    if (type_known)
        fly = mons_flies(m);
    else
        fly = mons_class_flies(type);

    two_weapons = mons_wields_two_weapons(m);

    // don't give away regeneration of monsters you're misled about
    if (type_known)
        no_regen = !mons_can_regenerate(m);
    else
        no_regen = !mons_class_can_regenerate(type);

    if (m->haloed() && !m->umbraed())
        mb.set(MB_HALOED);
    if (!m->haloed() && m->umbraed())
        mb.set(MB_UMBRAED);
    if (mons_looks_stabbable(m))
        mb.set(MB_STABBABLE);
    if (mons_looks_distracted(m))
        mb.set(MB_DISTRACTED);
    if (liquefied(m->pos()) && m->ground_level() && !m->is_insubstantial())
        mb.set(MB_SLOWED);
    if (m->is_wall_clinging())
        mb.set(MB_CLINGING);

    dam = mons_get_damage_level(m);

    // If no messages about wounds, don't display damage level either.
    if (nomsg_wounds)
        dam = MDAM_OKAY;

    if (mons_behaviour_perceptible(m))
    {
        if (m->asleep())
        {
            if (!m->can_hibernate(true))
                mb.set(MB_DORMANT);
            else
                mb.set(MB_SLEEPING);
        }
        // Applies to both friendlies and hostiles
        else if (mons_is_fleeing(m))
        {
            mb.set(MB_FLEEING);
        }
        else if (mons_is_wandering(m) && !mons_is_batty(m))
        {
            if (mons_is_stationary(m))
                mb.set(MB_UNAWARE);
            else
                mb.set(MB_WANDERING);
        }
        // TODO: is this ever needed?
        else if (m->foe == MHITNOT && !mons_is_batty(m)
                 && m->attitude == ATT_HOSTILE)
        {
            mb.set(MB_UNAWARE);
        }
    }

    for (mon_enchant_list::const_iterator e = m->enchantments.begin();
         e != m->enchantments.end(); ++e)
    {
        monster_info_flags flag = ench_to_mb(*m, e->first);
        if (flag != NUM_MB_FLAGS)
            mb.set(flag);
    }

    // fake enchantment (permanent)
    if (mons_class_flag(type, M_DEFLECT_MISSILES))
        mb.set(MB_DEFLECT_MSL);

    if (type == MONS_SILENT_SPECTRE)
        mb.set(MB_SILENCING);

    if (you.beheld_by(m))
        mb.set(MB_MESMERIZING);

    // Evilness of attacking
    switch (attitude)
    {
    case ATT_NEUTRAL:
    case ATT_HOSTILE:
        if (you.religion == GOD_SHINING_ONE
            && !tso_unchivalric_attack_safe_monster(m)
            && is_unchivalric_attack(&you, m))
        {
            mb.set(MB_EVIL_ATTACK);
        }
        break;
    default:
        break;
    }

    if (testbits(m->flags, MF_ENSLAVED_SOUL))
        mb.set(MB_ENSLAVED);

    if (m->is_shapeshifter() && (m->flags & MF_KNOWN_SHIFTER))
        mb.set(MB_SHAPESHIFTER);

    if (m->is_known_chaotic())
        mb.set(MB_CHAOTIC);

    if (m->submerged())
        mb.set(MB_SUBMERGED);

    // these are excluded as mislead types
    if (mons_is_pghost(type))
    {
        ASSERT(m->ghost.get());
        ghost_demon& ghost = *m->ghost;
        u.ghost.species = ghost.species;
        if (species_genus(u.ghost.species) == GENPC_DRACONIAN && ghost.xl < 7)
            u.ghost.species = SP_BASE_DRACONIAN;
        u.ghost.job = ghost.job;
        u.ghost.religion = ghost.religion;
        u.ghost.best_skill = ghost.best_skill;
        u.ghost.best_skill_rank = get_skill_rank(ghost.best_skill_level);
        u.ghost.xl_rank = ghost_level_to_rank(ghost.xl);
    }

    for (unsigned i = 0; i <= MSLOT_LAST_VISIBLE_SLOT; ++i)
    {
        bool ok;
        if (m->inv[i] == NON_ITEM)
            ok = false;
        else if (i == MSLOT_MISCELLANY)
            ok = mons_is_mimic(type);
        else if (attitude == ATT_FRIENDLY)
            ok = true;
        else if (i == MSLOT_WAND)
            ok = props.exists("wand_known") && props["wand_known"];
        else if (m->props.exists("ash_id") && item_type_known(mitm[m->inv[i]]))
            ok = true;
        else if (i == MSLOT_ALT_WEAPON)
            ok = two_weapons;
        else if (i == MSLOT_MISSILE)
            ok = false;
        else
            ok = true;
        if (ok)
            inv[i].reset(new item_def(get_item_info(mitm[m->inv[i]])));
    }

    fire_blocker = DNGN_UNSEEN;
    if (!crawl_state.arena_suspended
        && m->pos() != you.pos())
    {
        _blocked_ray(m->pos(), &fire_blocker);
    }

    if (m->props.exists("quote"))
        quote = m->props["quote"].get_string();

    if (m->props.exists("description"))
        description = m->props["description"].get_string();

    // this must be last because it provides this structure to Lua code
    if (milev > MILEV_SKIP_SAFE)
    {
        if (mons_is_safe(m))
            mb.set(MB_SAFE);
        else
            mb.set(MB_UNSAFE);
    }

    client_id = m->get_client_id();
}

monster* monster_info::mon() const
{
    int m = env.mgrid(player2grid(pos));
    ASSERT(m >= 0);
#ifdef USE_TILE
    if (m == NON_MONSTER)
        return NULL;
#endif
    return &env.mons[m];
}

std::string monster_info::db_name() const
{
    if (type == MONS_DANCING_WEAPON && inv[MSLOT_WEAPON].get())
    {
        iflags_t ignore_flags = ISFLAG_KNOW_CURSE | ISFLAG_KNOW_PLUSES;
        bool     use_inscrip  = false;
        return (inv[MSLOT_WEAPON]->name(false, DESC_DBNAME, false, false, use_inscrip, false,
                          ignore_flags));
    }

    return get_monster_data(type)->name;
}

std::string monster_info::_core_name() const
{
    monster_type nametype = type;

    switch (type)
    {
    case MONS_ZOMBIE_SMALL:     case MONS_ZOMBIE_LARGE:
    case MONS_SKELETON_SMALL:   case MONS_SKELETON_LARGE:
    case MONS_SIMULACRUM_SMALL: case MONS_SIMULACRUM_LARGE:
    case MONS_SPECTRAL_THING:   case MONS_PILLAR_OF_SALT:
        nametype = base_type;
        break;

    default:
        break;
    }

    std::string s;

    if (is(MB_NAME_REPLACE))
        s = gettext(mname.c_str());
    else if (nametype == MONS_LERNAEAN_HYDRA)
        s = gettext(M_("Lernaean hydra")); // TODO: put this into mon-data.h
    else if (nametype == MONS_ROYAL_JELLY)
        s = gettext(M_("royal jelly"));
    else if (nametype == MONS_SERPENT_OF_HELL)
        s = gettext(M_("Serpent of Hell"));
    else if (mons_is_mimic(nametype))
        s = mimic_name();
    else if (invalid_monster_type(nametype) && nametype != MONS_PROGRAM_BUG)
        s = gettext(M_("INVALID MONSTER"));
    else
    {
        const char* slime_sizes[] = {M_("buggy "), "", M_("large "), M_("very large "),
                                               M_("enormous "), M_("titanic ")};
        s = gettext(get_monster_data(nametype)->name);

        switch (type)
        {
        case MONS_SLIME_CREATURE:
            ASSERT(number <= 5);
            s = slime_sizes[number] + s;
            break;
        case MONS_UGLY_THING:
        case MONS_VERY_UGLY_THING:
            s = ugly_thing_colour_name(colour) + " " + s;
            break;

        case MONS_LABORATORY_RAT:
            s = adjective_for_labrat_colour(colour) + " " + s;
            break;

        case MONS_DRACONIAN_CALLER:
        case MONS_DRACONIAN_MONK:
        case MONS_DRACONIAN_ZEALOT:
        case MONS_DRACONIAN_SHIFTER:
        case MONS_DRACONIAN_ANNIHILATOR:
        case MONS_DRACONIAN_KNIGHT:
        case MONS_DRACONIAN_SCORCHER:
            if (base_type != MONS_NO_MONSTER)
                s = draconian_colour_name(base_type) + " " + s;
            break;

        case MONS_DANCING_WEAPON:
            if (inv[MSLOT_WEAPON].get())
            {
                iflags_t ignore_flags = ISFLAG_KNOW_CURSE | ISFLAG_KNOW_PLUSES;
                bool     use_inscrip  = true;
                const item_def& item = *inv[MSLOT_WEAPON];
                s = (item.name(false, DESC_PLAIN, false, false, use_inscrip, false,
                                  ignore_flags));
            }
            break;

        case MONS_PLAYER_GHOST:
            s = apostrophise(mname) + gettext(" ghost");
            break;
        case MONS_PLAYER_ILLUSION:
            s = apostrophise(mname) + gettext(" illusion");
            break;
        case MONS_PANDEMONIUM_LORD:
            s = gettext(mname.c_str());
            break;
        default:
            break;
        }
    }

    if (is(MB_NAME_SUFFIX))
        s += " " + std::string(gettext(mname.c_str()));
    else if (is(MB_NAME_ADJECTIVE))
        s = std::string(gettext(mname.c_str())) + " " + s;

    return s;
}

std::string monster_info::_apply_adjusted_description(description_level_type desc, const std::string& s) const
{
    if (desc == DESC_NOCAP_ITS)
        desc = DESC_NOCAP_THE;
    if (is(MB_NAME_THE))
    {
        if (desc == DESC_CAP_A)
            desc = DESC_CAP_THE;
        if (desc == DESC_NOCAP_A)
            desc = DESC_NOCAP_THE;
    }
    if (attitude == ATT_FRIENDLY)
    {
        if (desc == DESC_CAP_THE)
            desc = DESC_CAP_YOUR;
        if (desc == DESC_NOCAP_THE)
            desc = DESC_NOCAP_YOUR;
    }
    return apply_description(desc, s);
}

std::string monster_info::common_name(description_level_type desc) const
{
    std::ostringstream ss;

    if (props.exists("helpless"))
	/// 뒤에 계속 뭐가 붙는 형태가 됩니다.
	/// 이를테면 helpless submerged spectral electric-eel
        ss << gettext("helpless ");

    if (is(MB_SUBMERGED))
        ss << gettext("submerged ");

    if (type == MONS_SPECTRAL_THING && !is(MB_NAME_ZOMBIE))
        ss << gettext("spectral ");

    if (type == MONS_BALLISTOMYCETE)
        ss << (number ? gettext("active ") : "");

    if ((mons_genus(type) == MONS_HYDRA
            || mons_genus(base_type) == MONS_HYDRA)
        && number > 0)
    {
        if (number < 11)
        {
#ifdef KR
            const char* cardinals[] = {"한", "두", "세", "네", "다섯",
                                       "여섯", "일곱", "여덟", "아홉", "열"};
#else
            const char* cardinals[] = {"one", "two", "three", "four", "five",
                                       "six", "seven", "eight", "nine", "ten"};
#endif
            ss << cardinals[number - 1];
        }
        else
            ss << make_stringf("%d", number);

        ss << gettext("-headed ");
    }

    /// _core
    std::string core = _core_name();
    ss << core;

    // Add suffixes.
    switch (type)
    {
    case MONS_ZOMBIE_SMALL:
    case MONS_ZOMBIE_LARGE:
        if (!is(MB_NAME_ZOMBIE))
            ss << gettext(" zombie");
        break;
    case MONS_SKELETON_SMALL:
    case MONS_SKELETON_LARGE:
        if (!is(MB_NAME_ZOMBIE))
            ss << gettext(" skeleton");
        break;
    case MONS_SIMULACRUM_SMALL:
    case MONS_SIMULACRUM_LARGE:
        if (!is(MB_NAME_ZOMBIE))
            ss << gettext(" simulacrum");
        break;
    case MONS_PILLAR_OF_SALT:
        ss << gettext(" shaped pillar of salt");
        break;
    default:
        break;
    }

    if (is(MB_SHAPESHIFTER))
    {
        // If momentarily in original form, don't display "shaped
        // shifter".
        if (mons_genus(type) != MONS_SHAPESHIFTER)
            ss << gettext(" shaped shifter");
    }

    std::string s;
    // only respect unqualified if nothing was added ("Sigmund" or "The spectral Sigmund")
    if (!is(MB_NAME_UNQUALIFIED) || has_proper_name() || ss.str() != core)
        s = _apply_adjusted_description(desc, ss.str());
    else
        s = ss.str();

    if (desc == DESC_NOCAP_ITS)
        s = apostrophise(s);

    return (s);
}

dungeon_feature_type monster_info::get_mimic_feature() const
{
    if (!props.exists("feat_type"))
        return DNGN_UNSEEN;
    return static_cast<dungeon_feature_type>(props["feat_type"].get_short());
}

std::string monster_info::mimic_name() const
{
    std::string s;
    if (props.exists("feat_type"))
        s = feat_type_name(get_mimic_feature());
    else if (item_def* item = inv[MSLOT_MISCELLANY].get())
    {
        if (item->base_type == OBJ_GOLD)
            s = "pile of gold";
        else if (item->base_type == OBJ_MISCELLANY
                 && item->sub_type == MISC_RUNE_OF_ZOT)
        {
            s = "rune";
        }
        else
            s = item->name(false, DESC_BASENAME);
    }

    if (!s.empty())
        s += " ";

    return (s + "mimic");
}

bool monster_info::has_proper_name() const
{
    return !mname.empty() && !mons_is_ghost_demon(type)
            && !is(MB_NAME_REPLACE) && !is(MB_NAME_ADJECTIVE) && !is(MB_NAME_SUFFIX);
}

std::string monster_info::proper_name(description_level_type desc) const
{
    if (has_proper_name())
    {
        if (desc == DESC_NOCAP_ITS)
            return apostrophise(mname);
        else
            return gettext(mname.c_str());
    }
    else
        return common_name(desc);
}

std::string monster_info::full_name(description_level_type desc, bool use_comma) const
{
    if (desc == DESC_NONE)
        return ("");

    if (has_proper_name())
    {
        std::string s = mname + (use_comma ? ", the " : " the ") + common_name();
        if (desc == DESC_NOCAP_ITS)
            s = apostrophise(s);
        return s;
    }
    else
        return common_name(desc);
}

// Needed because gcc 4.3 sort does not like comparison functions that take
// more than 2 arguments.
bool monster_info::less_than_wrapper(const monster_info& m1,
                                     const monster_info& m2)
{
    return monster_info::less_than(m1, m2, true);
}

// Sort monsters by (in that order):    attitude, difficulty, type, brand
bool monster_info::less_than(const monster_info& m1, const monster_info& m2,
                             bool zombified, bool fullname)
{
    if (m1.attitude < m2.attitude)
        return (true);
    else if (m1.attitude > m2.attitude)
        return (false);

    // Force plain but different coloured draconians to be treated like the
    // same sub-type.
    if (!zombified && m1.type >= MONS_DRACONIAN
        && m1.type <= MONS_PALE_DRACONIAN
        && m2.type >= MONS_DRACONIAN
        && m2.type <= MONS_PALE_DRACONIAN)
    {
        return (false);
    }

    int diff_delta = mons_avg_hp(m1.type) - mons_avg_hp(m2.type);

    // By descending difficulty
    if (diff_delta > 0)
        return (true);
    else if (diff_delta < 0)
        return (false);

    // Force mimics of different types to be treated like the same one.
    if (mons_is_mimic(m1.type) && mons_is_mimic(m2.type))
        return (false);

    if (m1.type < m2.type)
        return (true);
    else if (m1.type > m2.type)
        return (false);

    // Never distinguish between dancing weapons.
    // The above checks guarantee that *both* monsters are of this type.
    if (m1.type == MONS_DANCING_WEAPON)
        return (false);

    if (m1.type == MONS_SLIME_CREATURE)
        return (m1.number > m2.number);

    if (m1.type == MONS_BALLISTOMYCETE)
        return ((m1.number > 0) > (m2.number > 0));

    if (zombified)
    {
        if (mons_class_is_zombified(m1.type))
        {
            // Because of the type checks above, if one of the two is zombified, so
            // is the other, and of the same type.
            if (m1.base_type < m2.base_type)
                return (true);
            else if (m1.base_type > m2.base_type)
                return (false);
        }

        // Both monsters are hydras or hydra zombies, sort by number of heads.
        if (mons_genus(m1.type) == MONS_HYDRA || mons_genus(m1.base_type) == MONS_HYDRA)
        {
            if (m1.number > m2.number)
                return (true);
            else if (m1.number < m2.number)
                return (false);
        }
    }

    if (fullname || mons_is_pghost(m1.type))
        return (m1.mname < m2.mname);

#if 0 // for now, sort mb together.
    // By descending mb, so no mb sorts to the end
    if (m1.mb > m2.mb)
        return (true);
    else if (m1.mb < m2.mb)
        return (false);
#endif

    return (false);
}

static std::string _verbose_info0(const monster_info& mi)
{
    if (mi.is(MB_BERSERK))
        return ("berserk");
    if (mi.is(MB_FRENZIED))
        return ("frenzied");
    if (mi.is(MB_ROUSED))
        return ("roused");
    if (mi.is(MB_INNER_FLAME))
        return ("inner flame");
    if (mi.is(MB_DUMB))
        return ("dumb");
    if (mi.is(MB_PARALYSED))
        return ("paralysed");
    if (mi.is(MB_CAUGHT))
        return ("caught");
    if (mi.is(MB_PETRIFIED))
        return ("petrified");
    if (mi.is(MB_PETRIFYING))
        return ("petrifying");
    if (mi.is(MB_MAD))
        return ("mad");
    if (mi.is(MB_CONFUSED))
        return ("confused");
    if (mi.is(MB_FLEEING))
        return ("fleeing");
    if (mi.is(MB_DORMANT))
        return ("dormant");
    if (mi.is(MB_SLEEPING))
        return ("sleeping");
    if (mi.is(MB_UNAWARE))
        return ("unaware");
    if (mi.is(MB_WITHDRAWN))
        return ("withdrawn");
    if (mi.is(MB_DAZED))
        return ("dazed");
    if (mi.is(MB_MUTE))
        return ("mute");
    if (mi.is(MB_BLIND))
        return ("blind");
    // avoid jelly (wandering) (fellow slime)
    if (mi.is(MB_WANDERING) && mi.attitude != ATT_STRICT_NEUTRAL)
        return ("wandering");
    if (mi.is(MB_BURNING))
        return ("burning");
    if (mi.is(MB_ROTTING))
        return ("rotting");
    if (mi.is(MB_BLEEDING))
        return ("bleeding");
    if (mi.is(MB_INVISIBLE))
        return ("invisible");

    return ("");
}

static std::string _verbose_info(const monster_info& mi)
{
    std::string inf = _verbose_info0(mi);
    if (!inf.empty())
        inf = " (" + inf + ")";
    return inf;
}

std::string monster_info::pluralized_name(bool fullname) const
{
    // Don't pluralise uniques, ever.  Multiple copies of the same unique
    // are unlikely in the dungeon currently, but quite common in the
    // arena.  This prevens "4 Gra", etc. {due}
    // Unless it's Mara, who summons illusions of himself.
    if (mons_is_unique(type) && type != MONS_MARA)
    {
        return common_name();
    }
    // Specialcase mimics, so they don't get described as piles of gold
    // when that would be inappropriate. (HACK)
    else if (mons_is_mimic(type))
    {
        return "mimics";
    }
    else if (mons_genus(type) == MONS_DRACONIAN)
    {
        return pluralise(PLU_DEFAULT, mons_type_name(MONS_DRACONIAN, DESC_PLAIN));
    }
    else if (type == MONS_UGLY_THING || type == MONS_VERY_UGLY_THING
             || type == MONS_DANCING_WEAPON || type == MONS_LABORATORY_RAT
             || !fullname)
    {
        return pluralise(PLU_DEFAULT, mons_type_name(type, DESC_PLAIN));
    }
    else
    {
        return pluralise(PLU_DEFAULT, common_name());
    }
}

void monster_info::to_string(int count, std::string& desc,
                                  int& desc_color, bool fullname) const
{
    std::ostringstream out;

    if (count == 1)
        out << full_name();
    else
    {
        // TODO: this should be done in a much cleaner way, with code to merge multiple monster_infos into a single common structure
        out << count << " " << pluralized_name(fullname);
    }

#ifdef DEBUG_DIAGNOSTICS
    out << " av" << mons_avg_hp(type);
#endif

    if (count == 1)
       out << _verbose_info(*this);

    // Friendliness
    switch (attitude)
    {
    case ATT_FRIENDLY:
        //out << " (friendly)";
        desc_color = GREEN;
        break;
    case ATT_GOOD_NEUTRAL:
    case ATT_NEUTRAL:
        //out << " (neutral)";
        desc_color = BROWN;
        break;
    case ATT_STRICT_NEUTRAL:
         out << " (fellow slime)";
         desc_color = BROWN;
         break;
    case ATT_HOSTILE:
        // out << " (hostile)";
        switch (threat)
        {
        case MTHRT_TRIVIAL: desc_color = DARKGREY;  break;
        case MTHRT_EASY:    desc_color = LIGHTGREY; break;
        case MTHRT_TOUGH:   desc_color = YELLOW;    break;
        case MTHRT_NASTY:   desc_color = LIGHTRED;  break;
        default:;
        }
        break;
    }

    if (count == 1 && is(MB_EVIL_ATTACK))
        desc_color = Options.evil_colour;

    desc = out.str();
}

std::vector<std::string> monster_info::attributes() const
{
    std::vector<std::string> v;
    if (is(MB_POISONED))
        v.push_back("poisoned");
    if (is(MB_SICK))
        v.push_back("sick");
    if (is(MB_ROTTING))
        v.push_back("rotting away"); //jmf: "covered in sores"?
    if (is(MB_GLOWING))
        v.push_back("softly glowing");
    if (is(MB_SLOWED))
        v.push_back("moving slowly");
    if (is(MB_INSANE))
        v.push_back("frenzied and insane");
    if (is(MB_BERSERK))
        v.push_back("berserk");
    if (is(MB_FRENZIED))
        v.push_back("consumed by blood-lust");
    if (is(MB_ROUSED))
        v.push_back("roused with righteous anger");
    if (is(MB_HASTED))
        v.push_back("moving very quickly");
    if (is(MB_STRONG))
        v.push_back("unusually strong");
    if (is(MB_CONFUSED))
        v.push_back("bewildered and confused");
    if (is(MB_INVISIBLE))
        v.push_back("slightly transparent");
    if (is(MB_CHARMED))
        v.push_back("in your thrall");
    if (is(MB_BURNING))
        v.push_back("covered in liquid flames");
    if (is(MB_CAUGHT))
        v.push_back("entangled in a net");
    if (is(MB_PETRIFIED))
        v.push_back("petrified");
    if (is(MB_PETRIFYING))
        v.push_back("slowly petrifying");
    if (is(MB_VULN_MAGIC))
        v.push_back("susceptible to magic");
    if (is(MB_SWIFT))
        v.push_back("moving somewhat quickly");
    if (is(MB_SILENCING))
        v.push_back("radiating silence");
    if (is(MB_PARALYSED))
        v.push_back("paralysed");
    if (is(MB_BLEEDING))
        v.push_back("bleeding");
    if (is(MB_DEFLECT_MSL))
        v.push_back("deflecting missiles");
    if (is(MB_PREP_RESURRECT))
        v.push_back("quietly preparing");
    if (is(MB_FADING_AWAY))
        v.push_back("slowly fading away");
    if (is(MB_MOSTLY_FADED))
        v.push_back("mostly faded away");
    if (is(MB_FEAR_INSPIRING))
        v.push_back("inspiring fear");
    if (is(MB_BREATH_WEAPON))
    {
        v.push_back(std::string("catching ")
                    + pronoun(PRONOUN_NOCAP_POSSESSIVE) + " breath");
    }
    if (is(MB_WITHDRAWN))
    {
        v.push_back("regenerating health quickly");
        v.push_back(std::string("protected by ")
                    + pronoun(PRONOUN_NOCAP_POSSESSIVE) + " shell");
    }
    if (is(MB_ATTACHED))
        v.push_back("attached and sucking blood");
    if (is(MB_DAZED))
        v.push_back("dazed");
    if (is(MB_MUTE))
        v.push_back("permanently mute");
    if (is(MB_BLIND))
        v.push_back("permanently blind");
    if (is(MB_DUMB))
        v.push_back("stupefied");
    if (is(MB_MAD))
        v.push_back("lost in madness");
    if (is(MB_DEATHS_DOOR))
       v.push_back("standing in death's doorway");
    if (is(MB_REGENERATION))
       v.push_back("regenerating");
    return v;
}

std::string monster_info::wounds_description_sentence() const
{
    const std::string wounds = wounds_description();
    if (wounds.empty())
        return "";
    else
        return std::string(pronoun(PRONOUN_CAP)) + " is " + wounds + ".";
}

std::string monster_info::wounds_description(bool use_colour) const
{
    if (dam == MDAM_OKAY)
        return "";

    std::string desc = get_damage_level_string(holi, dam);
    if (use_colour)
    {
        const int col = channel_to_colour(MSGCH_MONSTER_DAMAGE, dam);
        desc = colour_string(desc, col);
    }
    return desc;
}

int monster_info::randarts(artefact_prop_type ra_prop) const
{
    int ret = 0;

    if (itemuse() >= MONUSE_STARTING_EQUIPMENT)
    {
        item_def* weapon = inv[MSLOT_WEAPON].get();
        item_def* second = inv[MSLOT_ALT_WEAPON].get(); // Two-headed ogres, etc.
        item_def* armour = inv[MSLOT_ARMOUR].get();
        item_def* shield = inv[MSLOT_SHIELD].get();

        if (weapon && weapon->base_type == OBJ_WEAPONS && is_artefact(*weapon))
            ret += artefact_wpn_property(*weapon, ra_prop);

        if (second && second->base_type == OBJ_WEAPONS && is_artefact(*second))
            ret += artefact_wpn_property(*second, ra_prop);

        if (armour && armour->base_type == OBJ_ARMOUR && is_artefact(*armour))
            ret += artefact_wpn_property(*armour, ra_prop);

        if (shield && shield->base_type == OBJ_ARMOUR && is_artefact(*shield))
            ret += artefact_wpn_property(*shield, ra_prop);
    }

    return (ret);
}

int monster_info::res_magic() const
{
    int mr = (get_monster_data(type))->resist_magic;
    if (mr == MAG_IMMUNE)
        return (MAG_IMMUNE);

    // Negative values get multiplied with monster hit dice.
    if (mr < 0)
        mr = mons_class_hit_dice(type) * (-mr) * 4 / 3;

    // Randarts have a multiplicative effect.
    mr *= (randarts(ARTP_MAGIC) + 100);
    mr /= 100;

    // ego armour resistance
    if (inv[MSLOT_ARMOUR].get()
        && get_armour_ego_type(*inv[MSLOT_ARMOUR]) == SPARM_MAGIC_RESISTANCE)
    {
        mr += 30;
    }

    if (inv[MSLOT_SHIELD].get()
        && get_armour_ego_type(*inv[MSLOT_SHIELD]) == SPARM_MAGIC_RESISTANCE)
    {
        mr += 30;
    }

    if (is(MB_VULN_MAGIC))
        mr /= 2;

    return (mr);
}

size_type monster_info::body_size() const
{
    const monsterentry *e = get_monster_data(type);
    size_type ret = (e ? e->size : SIZE_MEDIUM);

    // Slime creature size is increased by the number merged.
    if (type == MONS_SLIME_CREATURE)
    {
        if (number == 2)
            ret = SIZE_MEDIUM;
        else if (number == 3)
            ret = SIZE_LARGE;
        else if (number == 4)
            ret = SIZE_BIG;
        else if (number == 5)
            ret = SIZE_GIANT;
    }
    else if (mons_is_item_mimic(type))
    {
        const int mass = item_mass(*inv[MSLOT_MISCELLANY].get());
        if (mass < 50)
            ret = SIZE_TINY;
        else if (mass < 100)
            ret = SIZE_LITTLE;
        else
            ret = SIZE_SMALL;
    }

    return (ret);
}

void get_monster_info(std::vector<monster_info>& mons)
{
    std::vector<monster* > visible;
    if (crawl_state.game_is_arena())
    {
        for (monster_iterator mi; mi; ++mi)
            visible.push_back(*mi);
    }
    else
        visible = get_nearby_monsters();

    for (unsigned int i = 0; i < visible.size(); i++)
    {
        if (!mons_class_flag(visible[i]->type, M_NO_EXP_GAIN)
            || visible[i]->type == MONS_KRAKEN_TENTACLE
            || visible[i]->type == MONS_BALLISTOMYCETE
                && visible[i]->number > 0)
        {
            mons.push_back(monster_info(visible[i]));
        }
    }
    std::sort(mons.begin(), mons.end(), monster_info::less_than_wrapper);
}
