#include "AppHdr.h"

#include "status.h"

#include "areas.h"
#include "env.h"
#include "evoke.h"
#include "godabil.h"
#include "libutil.h"
#include "misc.h"
#include "mutation.h"
#include "player.h"
#include "player-stats.h"
#include "skills2.h"
#include "terrain.h"
#include "transform.h"
#include "spl-transloc.h"
#include "korean.h"
#include "stuff.h"

// Status defaults for durations that are handled straight-forwardly.
struct duration_def
{
    duration_type dur;
    bool expire;         // whether to do automat expiring transforms
    int    light_colour; // status light base colour
    string light_text;   // for the status lights
    string short_text;   // for @: line
    string long_text ;   // for @ message
};

static duration_def duration_data[] =
{
    { DUR_AGILITY, false,
      0, "", P_("status","agile"), N_("You are agile.") },
    { DUR_ANTIMAGIC, true,
      RED, P_("status","-Mag"), P_("status","antimagic"), N_("You have trouble accessing your magic.") },
    { DUR_BARGAIN, true,
      BLUE, P_("status","Brgn"), P_("status","charismatic"), N_("You get a bargain in shops.") },
    { DUR_BERSERK, true,
      BLUE, P_("status","Berserk"), P_("status","berserking"), N_("You are possessed by a berserker rage.") },
    { DUR_BREATH_WEAPON, false,
      YELLOW, P_("status","Breath"), P_("status","short of breath"), N_("You are short of breath.") },
    { DUR_BRILLIANCE, false,
      0, "", P_("status","brilliant"), N_("You are brilliant.") },
    { DUR_CONF, false,
      RED, P_("status","Conf"), P_("status","confused"), N_("You are confused.") },
    { DUR_CONFUSING_TOUCH, true,
      BLUE, P_("status","Touch"), P_("status","confusing touch"), "" },
    { DUR_CONTROL_TELEPORT, true,
      MAGENTA, P_("status", "cTele"), P_("status","controlling teleports"), N_("You can control teleportations.") },
    { DUR_CORONA, false,
      YELLOW, P_("status", "Corona"), "", "" },
    { DUR_DEATH_CHANNEL, true,
      MAGENTA, P_("status", "DChan"), P_("status", "death channel"), N_("You are channeling the dead.") },
    { DUR_DIVINE_STAMINA, true,
      WHITE, P_("status","Vit"), P_("status","vitalised"), N_("You are divinely vitalised.") },
    { DUR_DIVINE_VIGOUR, false,
      0, "", P_("status","divinely vigorous"), N_("You are imbued with divine vigour.") },
    { DUR_EXHAUSTED, false,
      YELLOW, P_("status","Exh"), P_("status","exhausted"), N_("You are exhausted.") },
    { DUR_FIRE_SHIELD, true,
      BLUE, P_("status","RoF"), P_("status","immune to fire clouds"), "" },
    { DUR_ICY_ARMOUR, true,
      0, "", P_("status","icy armour"), N_("You are protected by a layer of icy armour.") },
    { DUR_LIQUID_FLAMES, false,
      RED, P_("status","Fire"), P_("status","liquid flames"), N_("You are covered in liquid flames.") },
    { DUR_LOWERED_MR, false,
      RED, P_("status","-MR"), P_("status","vulnerable"), "" },
    { DUR_MAGIC_SHIELD, false,
      0, "", P_("status","shielded"), "" },
    { DUR_MIGHT, false,
      0, "", P_("status","mighty"), N_("You are mighty.") },
    { DUR_MISLED, true,
      LIGHTMAGENTA, P_("status","Misled"), P_("status","misled"), "" },
    { DUR_PARALYSIS, false,
      RED, P_("status","Para"), P_("status","paralysed"), N_("You are paralysed.") },
    { DUR_PETRIFIED, false,
      RED, P_("status","Stone"), P_("status","petrified"), N_("You are petrified.") },
    { DUR_PETRIFYING, true,
      MAGENTA, P_("status","Petr"), P_("status","petrifying"), N_("You are turning to stone.") },
    { DUR_JELLY_PRAYER, false,
      WHITE, P_("status", "Pray"), P_("status", "praying"), N_("You are praying.") },
    { DUR_RESISTANCE, true,
      LIGHTBLUE, P_("status","Resist"), P_("status","resistant"), N_("You resist elements.") },
    { DUR_SLAYING, false,
      0, "", P_("status","deadly"), "" },
    { DUR_SLIMIFY, true,
      GREEN, P_("status","Slime"), P_("status","slimy"), "" },
    { DUR_SLEEP, false,
      0, "", P_("status", "sleeping"), N_("You are sleeping.") },
    { DUR_STONESKIN, false,
      0, "", P_("status","stone skin"), N_("Your skin is tough as stone.") },
    { DUR_SWIFTNESS, true,
      BLUE, P_("status","Swift"), P_("status","swift"), N_("You can move swiftly.") },
    { DUR_TELEPATHY, false,
      LIGHTBLUE, P_("status","Emp"), P_("status","empathic"), "" },
    { DUR_TELEPORT, false,
      LIGHTBLUE, P_("status","Tele"), P_("status","about to teleport"), N_("You are about to teleport.") },
    { DUR_DEATHS_DOOR, true,
      LIGHTGREY, P_("status", "DDoor"), P_("status", "death's door"), "" },
    { DUR_PHASE_SHIFT, true,
      0, "", P_("status","phasing"), N_("You are out of phase with the material plane.") },
    { DUR_QUAD_DAMAGE, true,
      BLUE, P_("status","Quad"), P_("status","quad damage"), "" },
    { DUR_SILENCE, true,
      BLUE, P_("status","Sil"), P_("status","silence"), N_("You radiate silence.") },
    { DUR_STEALTH, false,
      BLUE, P_("status","Stlth"), P_("status","especially stealthy"), "" },
    { DUR_AFRAID, true,
      RED, P_("status","Fear"), P_("status","afraid"), N_("You are terrified.") },
    { DUR_MIRROR_DAMAGE, false,
      WHITE, P_("status","Mirror"), P_("status","injury mirror"), N_("You mirror injuries.") },
    { DUR_SCRYING, false,
      LIGHTBLUE, P_("status","Scry"), P_("status","scrying"),
      N_("Your astral vision lets you see through walls.") },
    { DUR_TORNADO, true,
      LIGHTGREY, P_("status","Tornado"), P_("status","tornado"),
      N_("You are in the eye of a mighty hurricane.") },
    { DUR_LIQUEFYING, false,
      LIGHTBLUE, P_("status","Liquid"), P_("status","liquefying"),
      N_("The ground has become liquefied beneath your feet.") },
    { DUR_HEROISM, false,
      LIGHTBLUE, P_("status","Hero"), P_("status","heroism"), N_("You possess the skills of a mighty hero.") },
    { DUR_FINESSE, false,
      LIGHTBLUE, P_("status","Finesse"), P_("status","finesse"), N_("Your blows are lightning fast.") },
    { DUR_LIFESAVING, true,
      LIGHTGREY, P_("status","Prot"), P_("status","protection"), N_("You ask for being saved.") },
    { DUR_DARKNESS, true,
      BLUE, P_("status","Dark"), P_("status","darkness"), N_("You emit darkness.") },
    { DUR_SHROUD_OF_GOLUBRIA, true,
      BLUE, P_("status","Shroud"), P_("status","shrouded"), N_("You are protected by a distorting shroud.") },
    { DUR_TORNADO_COOLDOWN, false,
      YELLOW, P_("status", "Tornado"), "", "" ,},
    { DUR_DISJUNCTION, true,
      BLUE, P_("status", "Disjoin"), P_("status","disjoining"), N_("You are disjoining your surroundings.") },
    { DUR_SENTINEL_MARK, true,
      MAGENTA, P_("status","Mark"), P_("status","marked"), N_("You are marked for hunting.") },
    { DUR_INFUSION, true,
      BLUE, P_("status","Infus"), P_("status","infused"), N_("Your attacks are magically infused.")},
    { DUR_SONG_OF_SLAYING, true,
      BLUE, P_("status","Slay"), P_("status","singing"), N_("Your melee attacks are strengthened by your song.")},
    { DUR_SONG_OF_SHIELDING, true,
      BLUE, P_("status","SShield"), P_("status","shielded"), N_("Your magic is protecting you.")},
    { DUR_FLAYED, true,
      RED, P_("status","Flay"), P_("status","flayed"), N_("You are covered in terrible wounds.") },
    { DUR_RETCHING, true,
      RED, P_("status","Retch"), P_("status","retching"), N_("You are retching with violent nausea.") },
    { DUR_WEAK, false,
      RED, P_("status","Weak"), P_("status","weakened"), N_("Your attacks are enfeebled.") },
    { DUR_DIMENSION_ANCHOR, false,
      RED, P_("status","-Tele"), P_("status","cannot translocate"), N_("You are firmly anchored to this plane.") },
    { DUR_SPIRIT_HOWL, false,
      MAGENTA, P_("status","Howl"), P_("status","spirit howling"), N_("The howling of a spirit pack pursues you.") },
};

static int duration_index[NUM_DURATIONS];

void init_duration_index()
{
    for (int i = 0; i < NUM_DURATIONS; ++i)
        duration_index[i] = -1;

    for (unsigned i = 0; i < ARRAYSZ(duration_data); ++i)
    {
        duration_type dur = duration_data[i].dur;
        ASSERT_RANGE(dur, 0, NUM_DURATIONS);
        ASSERT(duration_index[dur] == -1);
        duration_index[dur] = i;
    }
}

static const duration_def* _lookup_duration(duration_type dur)
{
    ASSERT_RANGE(dur, 0, NUM_DURATIONS);
    if (duration_index[dur] == -1)
        return NULL;
    else
        return (&duration_data[duration_index[dur]]);
}

static void _reset_status_info(status_info* inf)
{
    inf->light_colour = 0;
    inf->light_text = "";
    inf->short_text = "";
    inf->long_text = "";
};

static int _bad_ench_colour(int lvl, int orange, int red)
{
    if (lvl > red)
        return RED;
    else if (lvl > orange)
        return LIGHTRED;

    return YELLOW;
}

static int _dur_colour(int exp_colour, bool expiring)
{
    if (expiring)
        return exp_colour;
    else
    {
        switch (exp_colour)
        {
        case GREEN:
            return LIGHTGREEN;
        case BLUE:
            return LIGHTBLUE;
        case MAGENTA:
            return LIGHTMAGENTA;
        case LIGHTGREY:
            return WHITE;
        default:
            return exp_colour;
        }
    }
}

static void _mark_expiring(status_info* inf, bool expiring)
{
    if (expiring)
    {
        if (!inf->short_text.empty())
            inf->short_text += " (expiring)";
        if (!inf->long_text.empty())
            inf->long_text = "Expiring: " + inf->long_text;
    }
}

static void _describe_airborne(status_info* inf);
static void _describe_burden(status_info* inf);
static void _describe_glow(status_info* inf);
static void _describe_hunger(status_info* inf);
static void _describe_regen(status_info* inf);
static void _describe_rotting(status_info* inf);
static void _describe_sickness(status_info* inf);
static void _describe_speed(status_info* inf);
static void _describe_sage(status_info* inf);
static void _describe_poison(status_info* inf);
static void _describe_transform(status_info* inf);
static void _describe_stat_zero(status_info* inf, stat_type st);
static void _describe_terrain(status_info* inf);
static void _describe_missiles(status_info* inf);

bool fill_status_info(int status, status_info* inf)
{
    _reset_status_info(inf);

    bool found = false;

    // Sort out inactive durations, and fill in data from duration_data
    // for the simple durations.
    if (status >= 0 && status < NUM_DURATIONS)
    {
        duration_type dur = static_cast<duration_type>(status);

        if (!you.duration[dur])
            return false;

        const duration_def* ddef = _lookup_duration(dur);
        if (ddef)
        {
            found = true;
            inf->light_colour = ddef->light_colour;
            inf->light_text   = pgettext_expr("status", ddef->light_text.c_str());
            inf->short_text   = pgettext_expr("status", ddef->short_text.c_str());
            inf->long_text    = gettext(ddef->long_text.c_str());
            if (ddef->expire)
            {
                inf->light_colour = _dur_colour(inf->light_colour,
                                                 dur_expiring(dur));
                _mark_expiring(inf, dur_expiring(dur));
            }
        }
    }

    // Now treat special status types and durations, possibly
    // completing or overriding the defaults set above.
    switch (status)
    {
    case DUR_CONTROL_TELEPORT:
        if (!allow_control_teleport(true))
            inf->light_colour = DARKGREY;
        break;

    case DUR_SWIFTNESS:
        if (you.in_water() || you.liquefied_ground())
            inf->light_colour = DARKGREY;
        break;

    case STATUS_AIRBORNE:
        _describe_airborne(inf);
        break;

    case STATUS_BEHELD:
        if (you.beheld())
        {
            inf->light_colour = RED;
            inf->light_text   = gettext(M_("Mesm"));
            inf->short_text   = gettext(M_("mesmerised"));
            inf->long_text    = gettext("You are mesmerised.");
        }
        break;

    case STATUS_BURDEN:
        _describe_burden(inf);
        break;

    case STATUS_CONTAMINATION:
        _describe_glow(inf);
        break;

    case STATUS_BACKLIT:
        if (you.backlit())
        {
            inf->short_text = _(M_("glowing"));
            inf->long_text  = _("You are glowing.");
        }
        break;

    case STATUS_UMBRA:
        if (you.umbra())
        {
            inf->light_colour = MAGENTA;
            inf->light_text   = _(M_("Umbra"));
            inf->short_text   = _("wreathed by umbra");
            inf->long_text    = _("You are wreathed by an unholy umbra.");
        }
        break;

    case STATUS_SUPPRESSED:
        if (you.suppressed())
        {
            inf->light_colour = LIGHTGREEN;
            inf->light_text   = _(M_("Suppress"));
            inf->short_text   = _("magically suppressed");
            inf->long_text    = _("You are enveloped in a field of magical suppression.");
        }
        break;

    case STATUS_NET:
        if (you.attribute[ATTR_HELD])
        {
            inf->light_colour = RED;
            inf->light_text   = _(M_("Held"));
            inf->short_text   = _(M_("held"));
            inf->long_text    = make_stringf(_("You are %s."), held_status());
        }
        break;

    case STATUS_HUNGER:
        _describe_hunger(inf);
        break;

    case STATUS_REGENERATION:
        // DUR_REGENERATION + some vampire and non-healing stuff
        _describe_regen(inf);
        break;

    case STATUS_ROT:
        _describe_rotting(inf);
        break;

    case STATUS_SICK:
        _describe_sickness(inf);
        break;

    case STATUS_SPEED:
        _describe_speed(inf);
        break;

    case STATUS_LIQUEFIED:
    {
        if (you.liquefied_ground())
        {
            inf->light_colour = BROWN;
            inf->light_text   = pgettext("status","SlowM");
            inf->short_text   = pgettext("status","slowed movement");
            inf->long_text    = _("Your movement is slowed on this liquid ground.");
        }
        break;
    }

    case STATUS_SAGE:
        _describe_sage(inf);
        break;

    case STATUS_AUGMENTED:
    {
        int level = augmentation_amount();

        if (level > 0)
        {
            inf->light_colour = (level == 3) ? WHITE :
                                (level == 2) ? LIGHTBLUE
                                             : BLUE;

            inf->light_text = "Aug";
        }
        break;
    }

    case DUR_CONFUSING_TOUCH:
    {
        const int dur = you.duration[DUR_CONFUSING_TOUCH];
        const int high = 40 * BASELINE_DELAY;
        const int low  = 20 * BASELINE_DELAY;
        inf->long_text = make_stringf(gettext("Your %s are glowing %sred."),
                            _(you.hand_name(true).c_str()),
                            dur > high ? pgettext("glowing", "an extremely bright ") :
                            dur > low ? pgettext("glowing", "bright ") :
                            pgettext("glowing", "a soft "));
        break;
    }

    case DUR_FIRE_SHIELD:
    {
        // Might be better to handle this with an extra virtual status.
        const bool exp = dur_expiring(DUR_FIRE_SHIELD);
        if (exp)
            inf->long_text += gettext("Expiring: ");
        inf->long_text += gettext("You are surrounded by a ring of flames.\n");
        if (exp)
            inf->long_text += gettext("Expiring: ");
        inf->long_text += gettext("You are immune to clouds of flame.");
        break;
    }

    case DUR_INVIS:
        if (you.attribute[ATTR_INVIS_UNCANCELLABLE])
            inf->light_colour = _dur_colour(BLUE, dur_expiring(DUR_INVIS));
        else
            inf->light_colour = _dur_colour(MAGENTA, dur_expiring(DUR_INVIS));
        inf->light_text   = gettext(M_("Invis"));
        inf->short_text   = gettext(M_("invisible"));
        if (you.backlit())
        {
            inf->light_colour = DARKGREY;
            inf->short_text += gettext(" (but backlit and visible)");
        }
        inf->long_text = gettext("You are ") + inf->short_text + ".";
        _mark_expiring(inf, dur_expiring(DUR_INVIS));
        break;

    case DUR_POISONING:
        _describe_poison(inf);
        break;

    case DUR_POWERED_BY_DEATH:
        if (handle_pbd_corpses(false) > 0)
        {
            inf->light_colour = LIGHTMAGENTA;
            inf->light_text   = gettext(M_("Regen+"));
        }
        break;

    case STATUS_MISSILES:
        _describe_missiles(inf);
        break;

    case STATUS_MANUAL:
    {
        string skills = manual_skill_names();
        if (!skills.empty())
        {
            inf->short_text = make_stringf(_("studying %s"), manual_skill_names(true).c_str());
            inf->long_text = make_stringf(_("You are studying %s."), skills.c_str());
        }
        break;
    }

    case DUR_SURE_BLADE:
    {
        inf->light_colour = BLUE;
        inf->light_text   = pgettext("status", "Blade");
        inf->short_text   = pgettext("status", "bonded with blade");
        string desc;
        if (you.duration[DUR_SURE_BLADE] > 15 * BASELINE_DELAY)
            desc = pgettext("sureblade", "strong ");
        else if (you.duration[DUR_SURE_BLADE] >  5 * BASELINE_DELAY)
            desc = "";
        else
            desc = pgettext("sureblade", "weak ");
        inf->long_text = make_stringf(gettext("You have a %sbond with your blade."),
                desc.c_str());
        break;
    }

    case DUR_TRANSFORMATION:
        _describe_transform(inf);
        break;

    case STATUS_CLINGING:
        if (you.is_wall_clinging())
        {
            inf->light_text   = gettext(M_("Cling"));
            inf->short_text   = gettext(M_("clinging"));
            inf->long_text    = gettext("You cling to the nearby walls.");
            const dungeon_feature_type feat = grd(you.pos());
            if (is_feat_dangerous(feat))
                inf->light_colour = LIGHTGREEN;
            else if (feat == DNGN_LAVA || feat_is_water(feat))
                inf->light_colour = GREEN;
            else
                inf->light_colour = DARKGREY;
            if (you.form == TRAN_SPIDER)
                _mark_expiring(inf, dur_expiring(DUR_TRANSFORMATION));
        }
        break;

    case STATUS_HOVER:
        if (is_hovering())
        {
            inf->light_colour = RED;
            inf->light_text   = _(M_("Hover"));
            inf->short_text   = _(M_("hovering above liquid"));
            inf->long_text    = _("You are exerting yourself to hover high above the liquid.");
        }
        break;

    case STATUS_STR_ZERO:
        _describe_stat_zero(inf, STAT_STR);
        break;
    case STATUS_INT_ZERO:
        _describe_stat_zero(inf, STAT_INT);
        break;
    case STATUS_DEX_ZERO:
        _describe_stat_zero(inf, STAT_DEX);
        break;

    case STATUS_FIREBALL:
        if (you.attribute[ATTR_DELAYED_FIREBALL])
        {
            inf->light_colour = LIGHTMAGENTA;
            inf->light_text   = gettext(M_("Fball"));
            inf->short_text   = gettext(M_("delayed fireball"));
            inf->long_text    = gettext("You have a stored fireball ready to release.");
        }
        break;

    case STATUS_CONSTRICTED:
        if (you.is_constricted())
        {
            inf->light_colour = YELLOW;
            inf->light_text   = you.held == HELD_MONSTER ? _(M_("Held")) : _(M_("Constr"));
            inf->short_text   = you.held == HELD_MONSTER ? _(M_("held")) : _(M_("constricted"));
        }
        break;

    case STATUS_TERRAIN:
        _describe_terrain(inf);
        break;

    // Silenced by an external source.
    case STATUS_SILENCE:
        if (silenced(you.pos()) && !you.duration[DUR_SILENCE])
        {
            inf->light_colour = LIGHTMAGENTA;
            inf->light_text   = _(M_("Sil"));
            inf->short_text   = _(M_("silenced"));
            inf->long_text    = _("You are silenced.");
        }
        break;

    case DUR_SONG_OF_SLAYING:
        inf->light_text = make_stringf("Slay (%u)",
                                       you.props["song_of_slaying_bonus"].get_int());
        break;

    case STATUS_NO_CTELE:
        if (!allow_control_teleport(true))
        {
            inf->light_colour = RED;
            inf->light_text = _(M_("-cTele"));
        }
        break;

    case STATUS_BEOGH:
        if (env.level_state & LSTATE_BEOGH && can_convert_to_beogh())
        {
            inf->light_colour = WHITE;
            inf->light_text = pgettext("status","Beogh");
        }
        break;

    case STATUS_RECALL:
        if (you.attribute[ATTR_NEXT_RECALL_INDEX] > 0)
        {
            inf->light_colour = WHITE;
            inf->light_text   = _(M_("Recall"));
            inf->short_text   = _(M_("recalling"));
            inf->long_text    = _("You are recalling your allies.");
        }
        break;

    case DUR_WATER_HOLD:
        inf->light_text   = _(M_("Engulf"));
        if (you.res_water_drowning())
        {
            inf->short_text   = _(M_("engulfed"));
            inf->long_text    = _("You are engulfed in water.");
            if (you.can_swim())
                inf->light_colour = DARKGREY;
            else
                inf->light_colour = YELLOW;
        }
        else
        {
            inf->short_text   = _(M_("engulfed (cannot breathe)"));
            inf->long_text    = _("You are engulfed in water and unable to breathe.");
            inf->light_colour = RED;
        }
        break;

    default:
        if (!found)
        {
            inf->light_colour = RED;
            inf->light_text   = "Missing";
            inf->short_text   = "missing status";
            inf->long_text    = _("Missing status description.");
            return false;
        }
        else
            break;
    }
    return true;
}

static void _describe_hunger(status_info* inf)
{
    const bool vamp = (you.species == SP_VAMPIRE);

    switch (you.hunger_state)
    {
    case HS_ENGORGED:
        inf->light_colour = LIGHTGREEN;
        inf->light_text   = (vamp ? gettext(M_("Alive")) : gettext(M_("Engorged")));
        break;
    case HS_VERY_FULL:
        inf->light_colour = GREEN;
        inf->light_text   = gettext(M_("Very Full"));
        break;
    case HS_FULL:
        inf->light_colour = GREEN;
        inf->light_text   = gettext(M_("Full"));
        break;
    case HS_HUNGRY:
        inf->light_colour = YELLOW;
        inf->light_text   = (vamp ? gettext(M_("Thirsty")) : gettext(M_("Hungry")));
        break;
    case HS_VERY_HUNGRY:
        inf->light_colour = YELLOW;
        inf->light_text   = (vamp ? gettext(M_("Very Thirsty")) : gettext(M_("Very Hungry")));
        break;
    case HS_NEAR_STARVING:
        inf->light_colour = YELLOW;
        inf->light_text   = (vamp ? gettext(M_("Near Bloodless")) : gettext(M_("Near Starving")));
        break;
    case HS_STARVING:
        inf->light_colour = RED;
        inf->light_text   = (vamp ? gettext(M_("Bloodless")) : gettext(M_("Starving")));
        inf->short_text   = (vamp ? gettext(M_("bloodless")) : gettext(M_("starving")));
        break;
    case HS_SATIATED: // no status light
    default:
        break;
    }
}

static void _describe_glow(status_info* inf)
{
    const int cont = get_contamination_level();
    if (cont > 0)
    {
        inf->light_colour = DARKGREY;
        if (cont > 1)
            inf->light_colour = _bad_ench_colour(cont, 2, 3);
        if (cont > 1 || you.species != SP_DJINNI)
            inf->light_text = gettext(M_("Contam"));
    }

    if (cont > 0)
    {
        inf->short_text =
                 (cont == 1) ? pgettext("glowing", "very slightly ") :
                 (cont == 2) ? pgettext("glowing", "slightly ") :
                 (cont == 3) ? "" :
                 (cont == 4) ? pgettext("glowing", "moderately ") :
                 (cont == 5) ? pgettext("glowing", "heavily ")
                             : pgettext("glowing", "really heavily ");
        inf->short_text += pgettext("glowing", "contaminated");
        inf->long_text = describe_contamination(cont);
    }
}

static void _describe_regen(status_info* inf)
{
    const bool regen = (you.duration[DUR_REGENERATION] > 0);
    const bool no_heal =
            (you.species == SP_VAMPIRE && you.hunger_state == HS_STARVING)
            || (player_mutation_level(MUT_SLOW_HEALING) == 3);
    // Does vampire hunger level affect regeneration rate significantly?
    const bool vampmod = !no_heal && !regen && you.species == SP_VAMPIRE
                         && you.hunger_state != HS_SATIATED;

    if (regen)
    {
        inf->light_colour = _dur_colour(BLUE, dur_expiring(DUR_REGENERATION));
        inf->light_text   = gettext(M_("Regen"));
        if (you.attribute[ATTR_DIVINE_REGENERATION])
            inf->light_text += gettext(M_(" MR"));
        else if (no_heal)
            inf->light_colour = DARKGREY;
    }

    if ((you.disease && !regen) || no_heal)
       inf->short_text = _(M_("non-regenerating"));
    else if (regen)
    {
        if (you.disease)
        {
            inf->short_text = gettext(M_("recuperating"));
            inf->long_text  = gettext("You are recuperating from your illness.");
        }
        else
        {
            inf->short_text = gettext(M_("regenerating"));
            inf->long_text  = gettext("You are regenerating.");
        }
        _mark_expiring(inf, dur_expiring(DUR_REGENERATION));
    }
    else if (vampmod)
    {
        inf->short_text = make_stringf(pgettext("Regen", "%s%s"),
                                       you.disease ? pgettext("Regen", "recuperating")
                                                   : pgettext("Regen", "regenerating"),
                                       you.hunger_state < HS_SATIATED ? pgettext("Regen", " slowly") :
                                       you.hunger_state < HS_ENGORGED ? pgettext("Regen", " quickly") :
                                       pgettext("Regen", " very quickly"));
    }
}

static void _describe_poison(status_info* inf)
{
    int pois = you.duration[DUR_POISONING];
    inf->light_colour = (player_res_poison(false) >= 3
                         ? DARKGREY : _bad_ench_colour(pois, 5, 10));
    inf->light_text   = pgettext("status", "Pois");
    const string adj =
         (pois > 10) ? pgettext("Pois", "extremely") :
         (pois > 5)  ? pgettext("Pois", "very") :
         (pois > 3)  ? pgettext("Pois", "quite")
                     : pgettext("Pois", "mildly");
    inf->short_text   = adj + pgettext("Pois", " poisoned");
    inf->long_text    = gettext("You are ") + inf->short_text + ".";
}

static void _describe_speed(status_info* inf)
{
    if (you.duration[DUR_SLOW] && you.duration[DUR_HASTE])
    {
        inf->light_colour = MAGENTA;
        inf->light_text   = gettext(M_("Fast+Slow"));
        inf->short_text   = gettext(M_("hasted and slowed"));
        inf->long_text = gettext("You are under both slowing and hasting effects.");
    }
    else if (you.duration[DUR_SLOW])
    {
        inf->light_colour = RED;
        inf->light_text   = gettext(M_("Slow"));
        inf->short_text   = gettext(M_("slowed"));
        inf->long_text    = gettext("You are slowed.");
    }
    else if (you.duration[DUR_HASTE])
    {
        inf->light_colour = _dur_colour(BLUE, dur_expiring(DUR_HASTE));
        inf->light_text   = gettext(M_("Fast"));
        inf->short_text = gettext(M_("hasted"));
        inf->long_text = gettext("Your actions are hasted.");
        _mark_expiring(inf, dur_expiring(DUR_HASTE));
    }
}

static void _describe_sage(status_info* inf)
{
    if (you.sage_skills.empty())
        return;

    vector<const char*> sages;
    for (unsigned long i = 0; i < you.sage_skills.size(); ++i)
        sages.push_back(_(skill_name(you.sage_skills[i])));

    inf->light_colour = LIGHTBLUE;
    inf->light_text   = _(M_("Sage"));
    inf->short_text   = _(M_("sage [")) + comma_separated_line(sages.begin(),
                        sages.end(), ", ") + "]";
    inf->long_text    = _("You feel studious about ") + comma_separated_line(
                        sages.begin(), sages.end()) + pgettext("sagestatus",".");
}

static void _describe_airborne(status_info* inf)
{
    if (!you.airborne())
        return;

    const bool perm     = you.permanent_flight();
    const bool expiring = (!perm && dur_expiring(DUR_FLIGHT));

    inf->light_colour = you.tengu_flight() ? BLUE : perm ? WHITE : MAGENTA;
    inf->light_text   = pgettext("status", "Fly");
    inf->short_text   = pgettext("status", "flying");
    inf->long_text    = _("You are flying.");
    inf->light_colour = _dur_colour(inf->light_colour, expiring);
    _mark_expiring(inf, expiring);
}

static void _describe_rotting(status_info* inf)
{
    if (you.rotting)
    {
        inf->light_colour = _bad_ench_colour(you.rotting, 4, 8);
        inf->light_text   = gettext(M_("Rot"));
    }

    if (you.rotting || you.species == SP_GHOUL)
    {
        inf->short_text = gettext(M_("rotting"));
        inf->long_text = gettext("Your flesh is rotting.");
        int rot = you.rotting;
        if (you.species == SP_GHOUL)
            rot += 1 + (1 << max(0, HS_SATIATED - you.hunger_state));
        if (rot > 15)
            inf->long_text = gettext("Your flesh is rotting before your eyes.");
        else if (rot > 8)
            inf->long_text = gettext("Your flesh is rotting away quickly.");
        else if (rot > 4)
            inf->long_text = gettext("Your flesh is rotting badly.");
        else if (you.species == SP_GHOUL)
            if (rot > 2)
				inf->long_text = gettext("Your flesh is rotting faster than usual.");
            else
				inf->long_text = gettext("Your flesh is rotting at the usual pace.");
    }
}

static void _describe_sickness(status_info* inf)
{
    if (you.disease)
    {
        const int high = 120 * BASELINE_DELAY;
        const int low  =  40 * BASELINE_DELAY;

        inf->light_colour   = _bad_ench_colour(you.disease, low, high);
        inf->light_text     = gettext(M_("Sick"));

        string mod = (you.disease > high) ? pgettext("Sick", "badly ")  :
                     (you.disease >  low) ? ""        :
                                            pgettext("Sick", "mildly ");

        inf->short_text = mod + pgettext("Sick", "diseased");
        inf->long_text  = make_stringf(gettext("You are %sdiseased."), mod.c_str());
    }
}

static void _describe_burden(status_info* inf)
{
    switch (you.burden_state)
    {
    case BS_OVERLOADED:
        inf->light_colour = RED;
        inf->light_text   = gettext(M_("Overloaded"));
        inf->short_text   = gettext(M_("overloaded"));
        inf->long_text    = gettext("You are overloaded with stuff.");
        break;
    case BS_ENCUMBERED:
        inf->light_colour = LIGHTRED;
        inf->light_text   = gettext(M_("Burdened"));
        inf->short_text   = gettext(M_("burdened"));
        inf->long_text    = gettext("You are burdened.");
        break;
    case BS_UNENCUMBERED:
        break;
    }
}

static void _describe_transform(status_info* inf)
{
    const bool vampbat = (you.species == SP_VAMPIRE && you.form == TRAN_BAT);
    const bool expire  = dur_expiring(DUR_TRANSFORMATION) && !vampbat;

    switch (you.form)
    {
    case TRAN_BAT:
        inf->light_text     = gettext(M_("Bat"));
        inf->short_text     = gettext(M_("bat-form"));
        if (vampbat)
            inf->long_text = gettext("You are in vampire bat-form.");
        else
            inf->long_text = gettext("You are in bat-form.");
        break;
    case TRAN_BLADE_HANDS:
        inf->light_text = gettext(M_("Blades"));
        inf->short_text = pgettext("Blades", "blade ") + blade_parts(true);
        inf->long_text  = pgettext("Blades", "You have blades for ") + blade_parts() + ".";
        break;
    case TRAN_DRAGON:
        inf->light_text = gettext(M_("Dragon"));
        inf->short_text = gettext(M_("dragon-form"));
        inf->long_text  = gettext("You are in dragon-form.");
        break;
    case TRAN_ICE_BEAST:
        inf->light_text = gettext(M_("Ice"));
        inf->short_text = gettext(M_("ice-form"));
        inf->long_text  = gettext("You are an ice creature.");
        break;
    case TRAN_LICH:
        inf->light_text = gettext(M_("Lich"));
        inf->short_text = gettext(M_("lich-form"));
        inf->long_text  = gettext("You are in lich-form.");
        break;
    case TRAN_PIG:
        inf->light_text = gettext(M_("Pig"));
        inf->short_text = gettext(M_("pig-form"));
        inf->long_text  = gettext("You are a filthy swine.");
        break;
    case TRAN_SPIDER:
        inf->light_text = gettext(M_("Spider"));
        inf->short_text = gettext(M_("spider-form"));
        inf->long_text  = gettext("You are in spider-form.");
        break;
    case TRAN_STATUE:
        inf->light_text = gettext(M_("Statue"));
        inf->short_text = gettext(M_("statue-form"));
        inf->long_text  = gettext("You are a statue.");
        break;
    case TRAN_APPENDAGE:
        inf->light_text = gettext(M_("App"));
        inf->short_text = gettext(M_("appendage"));
        inf->long_text  = gettext("You have a beastly appendage.");
        break;
    case TRAN_FUNGUS:
        inf->light_text = "Fungus";
        inf->short_text = "fungus-form";
        inf->long_text  = "You are a sentient fungus.";
        break;
    case TRAN_TREE:
        inf->light_text = _(M_("Tree"));
        inf->short_text = _(M_("tree-form"));
        inf->long_text  = _("You are an animated tree.");
        break;
    case TRAN_JELLY:
        inf->light_text = _(M_("Jelly"));
        inf->short_text = _(M_("jelly-form"));
        inf->long_text  = _("You are a lump of jelly.");
        break;
    case TRAN_PORCUPINE:
        inf->light_text = _(M_("Porc"));
        inf->short_text = _(M_("porcupine-form"));
        inf->long_text  = _("You are a porcupine.");
        break;
    case TRAN_WISP:
        inf->light_text = _(M_("Wisp"));
        inf->short_text = _(M_("wisp-form"));
        inf->long_text  = _("You are an insubstantial wisp.");
        break;
    case TRAN_NONE:
        break;
    }

    inf->light_colour = _dur_colour(GREEN, expire);
    _mark_expiring(inf, expire);
}

static const char* s0_names[NUM_STATS] = { M_("Collapse"), M_("Brainless"), M_("Clumsy"), };

static void _describe_stat_zero(status_info* inf, stat_type st)
{
    if (you.stat_zero[st])
    {
        inf->light_colour = you.stat(st) ? LIGHTRED : RED;
        inf->light_text   = gettext(s0_names[st]);
        inf->short_text   = make_stringf(gettext("lost %s"), gettext(stat_desc(st, SD_NAME)));
        inf->long_text    = make_stringf(you.stat(st) ?
                gettext("You are recovering from loss of %s.") : gettext("You have no %s!"),
                gettext(stat_desc(st, SD_NAME)));
    }
}

static void _describe_terrain(status_info* inf)
{
    switch (grd(you.pos()))
    {
    case DNGN_SHALLOW_WATER:
        inf->light_colour = LIGHTBLUE;
        inf->light_text = _(M_("Water"));
        break;
    case DNGN_DEEP_WATER:
        inf->light_colour = BLUE;
        inf->light_text = _(M_("Water"));
        break;
    case DNGN_LAVA:
        inf->light_colour = RED;
        inf->light_text = _(M_("Lava"));
        break;
    default:
        ;
    }
}

static void _describe_missiles(status_info* inf)
{
    const int level = you.missile_deflection();
    if (!level)
        return;

    bool expiring;
    if (level > 1)
    {
        inf->light_colour = MAGENTA;
        inf->light_text   = _(M_("DMsl"));
        inf->short_text   = _(M_("deflect missiles"));
        inf->long_text    = _("You deflect missiles.");
        expiring = dur_expiring(DUR_DEFLECT_MISSILES);
    }
    else
    {
        bool perm = player_mutation_level(MUT_DISTORTION_FIELD) == 3
                    || !you.suppressed() && you.scan_artefacts(ARTP_RMSL);
        inf->light_colour = perm ? WHITE : BLUE;
        inf->light_text   = _(M_("RMsl"));
        inf->short_text   = _(M_("repel missiles"));
        inf->long_text    = _("You repel missiles.");
        expiring = (!perm && dur_expiring(DUR_REPEL_MISSILES));
    }

    inf->light_colour = _dur_colour(inf->light_colour, expiring);
    _mark_expiring(inf, expiring);
}
