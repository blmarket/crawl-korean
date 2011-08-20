#include "AppHdr.h"

#include "status.h"

#include "areas.h"
#include "env.h"
#include "misc.h"
#include "mutation.h"
#include "player.h"
#include "player-stats.h"
#include "skills2.h"
#include "terrain.h"
#include "transform.h"
#include "spl-transloc.h"

// Status defaults for durations that are handled straight-forwardly.
struct duration_def
{
    duration_type dur;
    bool expire;              // whether to do automat expiring transforms
    int         light_colour; // status light base colour
    std::string light_text;   // for the status lights
    std::string short_text;   // for @: line
    std::string long_text ;   // for @ message
};

static duration_def duration_data[] =
{
    { DUR_AGILITY, false,
      0, "", M_("agile"), N_("You are agile.") },
    { DUR_BARGAIN, true,
      BLUE, M_("Brgn"), M_("charismatic"), N_("You get a bargain in shops.") },
    { DUR_BERSERK, true,
      BLUE, M_("Berserk"), M_("berserking"), N_("You are possessed by a berserker rage.") },
    { DUR_BREATH_WEAPON, false,
      YELLOW, M_("Breath"), M_("short of breath"), N_("You are short of breath.") },
    { DUR_BRILLIANCE, false,
      0, "", M_("brilliant"), N_("You are brilliant.") },
    { DUR_CONF, false,
      RED, M_("Conf"), M_("confused"), N_("You are confused.") },
    { DUR_CONFUSING_TOUCH, true,
      BLUE, M_("Touch"), M_("confusing touch"), "" },
    { DUR_CONTROL_TELEPORT, true,
      MAGENTA, M_("cTele"), "", N_("You can control teleportations.") },
    { DUR_DEATH_CHANNEL, true,
      MAGENTA, M_("DChan"), M_("death channel"), N_("You are channeling the dead.") },
    { DUR_DEFLECT_MISSILES, true,
      MAGENTA, M_("DMsl"), M_("deflect missiles"), N_("You deflect missiles.") },
    { DUR_DIVINE_STAMINA, false,
      0, "", M_("divinely fortified"), N_("You are divinely fortified.") },
    { DUR_DIVINE_VIGOUR, false,
      0, "", M_("divinely vigorous"), N_("You are imbued with divine vigour.") },
    { DUR_EXHAUSTED, false,
      YELLOW, M_("Exh"), M_("exhausted"), N_("You are exhausted.") },
    { DUR_FIRE_SHIELD, true,
      BLUE, M_("RoF"), M_("immune to fire clouds"), "" },
    { DUR_ICY_ARMOUR, true,
      0, "", M_("icy armour"), N_("You are protected by a layer of icy armour.") },
    { DUR_LIQUID_FLAMES, false,
      RED, M_("Fire"), M_("liquid flames"), N_("You are covered in liquid flames.") },
    { DUR_LOWERED_MR, false,
      RED, M_("-MR"), "", "" },
    { DUR_MAGIC_SHIELD, false,
      0, "", M_("shielded"), "" },
    { DUR_MIGHT, false,
      0, "", M_("mighty"), N_("You are mighty.") },
    { DUR_MISLED, true,
      LIGHTMAGENTA, M_("Misled"), "", "" },
    { DUR_PARALYSIS, false,
      RED, M_("Para"), M_("paralysed"), N_("You are paralysed.") },
    { DUR_PETRIFIED, false,
      RED, M_("Stone"), M_("petrified"), N_("You are petrified.") },
    { DUR_PETRIFYING, true,
      MAGENTA, M_("Petr"), M_("petrifying"), N_("You are turning to stone.") },
    { DUR_JELLY_PRAYER, false,
      WHITE, M_("Pray"), M_("praying"), N_("You are praying.") },
    { DUR_REPEL_MISSILES, true,
      BLUE, M_("RMsl"), M_("repel missiles"), N_("You are protected from missiles.") },
    { DUR_RESIST_POISON, true,
      LIGHTBLUE, M_("rPois"), "", N_("You resist poison.") },
    { DUR_RESIST_COLD, true,
      LIGHTBLUE, M_("rCold"), "", N_("You resist cold.") },
    { DUR_RESIST_FIRE, true,
      LIGHTBLUE, M_("rFire"), "", N_("You resist fire.") },
    { DUR_SAGE, true,
      BLUE, M_("Sage"), "", "" },
    { DUR_SEE_INVISIBLE, true,
      BLUE, M_("SInv"), "", N_("You can see invisible.") },
    { DUR_SLAYING, false,
      0, "", M_("deadly"), "" },
    { DUR_SLIMIFY, true,
      GREEN, M_("Slime"), M_("slimy"), "" },
    { DUR_SLEEP, false,
      0, "", M_("sleeping"), N_("You are sleeping.") },
#if TAG_MAJOR_VERSION == 32
    { DUR_STONEMAIL, true,
      0, "", M_("stone mail"), N_("You are covered in scales of stone.")},
#endif
    { DUR_STONESKIN, false,
      0, "", M_("stone skin"), N_("Your skin is tough as stone.") },
    { DUR_SWIFTNESS, true,
      BLUE, M_("Swift"), M_("swift"), N_("You can move swiftly.") },
    { DUR_TELEPATHY, false,
      LIGHTBLUE, M_("Emp"), "", "" },
    { DUR_TELEPORT, false,
      LIGHTBLUE, M_("Tele"), M_("about to teleport"), N_("You are about to teleport.") },
    { DUR_DEATHS_DOOR, true,
      LIGHTGREY, M_("DDoor"), M_("death's door"), "" },
    { DUR_INSULATION, true,
      BLUE, M_("Ins"), "", N_("You are insulated.") },
    { DUR_PHASE_SHIFT, true,
      0, "", M_("phasing"), N_("You are out of phase with the material plane.") },
    { DUR_QUAD_DAMAGE, true,
      BLUE, M_("Quad"), "", "" },
    { DUR_SILENCE, true,
      BLUE, M_("Sil"), M_("silence"), N_("You radiate silence.") },
    { DUR_STEALTH, false,
      BLUE, M_("Stlth"), "", "" },
    { DUR_AFRAID, true,
      RED, M_("Fear"), M_("afraid"), N_("You are terrified.") },
    { DUR_MIRROR_DAMAGE, false,
      WHITE, M_("Mirror"), M_("injury mirror"), N_("You mirror injuries.") },
    { DUR_SCRYING, false,
      LIGHTBLUE, M_("Scry"), M_("scrying"),
      N_("Your astral vision lets you see through walls.") },
    { DUR_TORNADO, true,
      LIGHTGREY, M_("Tornado"), M_("tornado"),
      N_("You are in the eye of a mighty hurricane.") },
    { DUR_LIQUEFYING, false,
      YELLOW, M_("Liquid"), M_("liquefying"),
      N_("The ground has become liquefied beneath your feet.") },
    { DUR_HEROISM, false,
      LIGHTBLUE, M_("Hero"), M_("heroism"), N_("You possess the skills of a mighty hero.") },
    { DUR_FINESSE, false,
      LIGHTBLUE, M_("Finesse"), M_("finesse"), N_("Your blows are lightning fast.") },
    { DUR_LIFESAVING, true,
      LIGHTGREY, M_("Prot"), M_("protection"), N_("You ask for being saved.") },
    { DUR_DARKNESS, true,
      BLUE, M_("Dark"), M_("darkness"), N_("You emit darkness.") },
    { DUR_SHROUD_OF_GOLUBRIA, true,
      BLUE, M_("Shroud"), M_("shrouded"), N_("You are protected by a distorting shroud.") },
    { DUR_TORNADO_COOLDOWN, false,
      YELLOW, M_("Tornado"), "", "" ,},
};

static int duration_index[NUM_DURATIONS];

void init_duration_index()
{
    for (int i = 0; i < NUM_DURATIONS; ++i)
        duration_index[i] = -1;

    for (unsigned i = 0; i < ARRAYSZ(duration_data); ++i)
    {
        duration_type dur = duration_data[i].dur;
        ASSERT(dur >= 0 && dur < NUM_DURATIONS);
        ASSERT(duration_index[dur] == -1);
        duration_index[dur] = i;
    }
}

static const duration_def* _lookup_duration(duration_type dur)
{
    ASSERT(dur >= 0 && dur < NUM_DURATIONS);
    if (duration_index[dur] == -1)
        return (NULL);
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
        return (RED);
    else if (lvl > orange)
        return (LIGHTRED);

    return (YELLOW);
}

int dur_colour(int exp_colour, bool expiring)
{
    if (expiring)
    {
        return (exp_colour);
    }
    else
    {
        switch (exp_colour)
        {
        case GREEN:
            return (LIGHTGREEN);
        case BLUE:
            return (LIGHTBLUE);
        case MAGENTA:
            return (LIGHTMAGENTA);
        case LIGHTGREY:
            return (WHITE);
        default:
            return (exp_colour);
        }
    }
}

static void _mark_expiring(status_info* inf, bool expiring)
{
    if (expiring)
    {
        if (!inf->short_text.empty())
            inf->short_text += " (expires)";
        if (!inf->long_text.empty())
            inf->long_text = "Expiring: " + inf->long_text;
    }
}

static void _describe_airborne(status_info* inf);
static void _describe_burden(status_info* inf);
static void _describe_glow(status_info* inf);
static void _describe_backlit(status_info* inf);
static void _describe_hunger(status_info* inf);
static void _describe_regen(status_info* inf);
static void _describe_rotting(status_info* inf);
static void _describe_sickness(status_info* inf);
static void _describe_speed(status_info* inf);
static void _describe_poison(status_info* inf);
static void _describe_transform(status_info* inf);
static void _describe_stat_zero(status_info* inf, stat_type st);

void fill_status_info(int status, status_info* inf)
{
    _reset_status_info(inf);

    bool found = false;

    // Sort out inactive durations, and fill in data from duration_data
    // for the simple durations.
    if (status >= 0 && status < NUM_DURATIONS)
    {
        duration_type dur = static_cast<duration_type>(status);

        if (!you.duration[dur])
            return;

        const duration_def* ddef = _lookup_duration(dur);
        if (ddef)
        {
            found = true;
            inf->light_colour = ddef->light_colour;
            inf->light_text   = gettext(ddef->light_text.c_str());
            inf->short_text   = gettext(ddef->short_text.c_str());
            inf->long_text    = gettext(ddef->long_text.c_str());
            if (ddef->expire)
            {
                inf->light_colour = dur_colour(inf->light_colour,
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
        if(!allow_control_teleport(true))
            inf->light_colour = DARKGREY;
        break;

    case DUR_SWIFTNESS:
        if(you.in_water())
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
            _describe_backlit(inf);
        break;

    case STATUS_UMBRA:
        if (you.umbra())
        {
            inf->light_colour = MAGENTA;
            inf->light_text   = "Umbra";
            inf->short_text   = "wreathed by umbra";
            inf->long_text    = "You are wreathed by an unholy umbra.";
        }
        break;

    case STATUS_NET:
        if (you.attribute[ATTR_HELD])
        {
            inf->light_colour = RED;
            inf->light_text   = gettext(M_("Held"));
            inf->short_text   = gettext(M_("held"));
            inf->long_text    = gettext("You are held in a net.");
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

    case DUR_CONFUSING_TOUCH:
    {
        const int dur = you.duration[DUR_CONFUSING_TOUCH];
        const int high = 40 * BASELINE_DELAY;
        const int low  = 20 * BASELINE_DELAY;
        inf->long_text = make_stringf(gettext("Your %s are glowing %sred."),
                            you.hand_name(true).c_str(),
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
            inf->light_colour = dur_colour(BLUE, dur_expiring(DUR_INVIS));
        else
            inf->light_colour = dur_colour(MAGENTA, dur_expiring(DUR_INVIS));
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

    case DUR_REPEL_MISSILES:
        // no status light or short text when also deflecting
        if (you.duration[DUR_DEFLECT_MISSILES])
        {
            inf->light_colour = 0;
            inf->light_text   = "";
            inf->short_text   = "";
        }
        break;

    case DUR_SAGE:
    {
        std::string sk = skill_name(you.sage_bonus_skill);
        inf->short_text = pgettext("sage", "studying ") + sk;
        inf->long_text = make_stringf(gettext("You are studying %s."),
                            sk.c_str());
        _mark_expiring(inf, dur_expiring(DUR_SAGE));
        break;
    }

    case DUR_SURE_BLADE:
    {
        inf->light_colour = BLUE;
        inf->light_text   = gettext(M_("Blade"));
        inf->short_text   = gettext(M_("bonded with blade"));
        std::string desc;
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
            _mark_expiring(inf, dur_expiring(DUR_TRANSFORMATION));
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

    default:
        if (!found)
        {
            inf->light_colour = RED;
            inf->light_text   = gettext(M_("Missing"));
            inf->short_text   = gettext(M_("missing status"));
            inf->long_text    = gettext("Missing status description.");
        }
        break;
    }
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

static void _describe_backlit(status_info* inf)
{
    if (get_contamination_level() > 1)
        inf->light_colour = _bad_ench_colour(get_contamination_level(), 2, 3);
    else if (you.duration[DUR_QUAD_DAMAGE])
        inf->light_colour = BLUE;
    else if (you.duration[DUR_CORONA])
        inf->light_colour = LIGHTBLUE;
    else if (you.duration[DUR_LIQUID_FLAMES])
        inf->light_colour = RED;
    else if (!you.antihaloed() && you.haloed())
        inf->light_colour = YELLOW;

    inf->light_text   = "Glow";
    inf->short_text   = "glowing";
    inf->long_text    = "You are glowing.";

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
        inf->light_colour = dur_colour(BLUE, dur_expiring(DUR_REGENERATION));
        inf->light_text   = gettext(M_("Regen"));
        if (you.attribute[ATTR_DIVINE_REGENERATION])
            inf->light_text += gettext(M_(" MR"));
        else if (no_heal)
            inf->light_colour = DARKGREY;
    }

    if ((you.disease && !regen) || no_heal)
    {
       inf->short_text = gettext(M_("non-regenerating"));
    }
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
    inf->light_colour = _bad_ench_colour(pois, 5, 10);
    inf->light_text   = gettext(M_("Pois"));
    const std::string adj =
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
        inf->light_colour = dur_colour(BLUE, dur_expiring(DUR_HASTE));
        inf->light_text   = gettext(M_("Fast"));
        inf->short_text = gettext(M_("hasted"));
        inf->long_text = gettext("Your actions are hasted.");
        _mark_expiring(inf, dur_expiring(DUR_HASTE));
    }
    if (liquefied(you.pos(), true) && you.ground_level())
    {
        inf->light_colour = BROWN;
        inf->light_text   = gettext(M_("SlowM"));
        inf->short_text   = gettext(M_("slowed movement"));
        inf->long_text    = gettext("Your movement is slowed in this liquid ground.");
    }
}

static void _describe_airborne(status_info* inf)
{
    if (!you.airborne())
        return;

    const bool perm     = you.permanent_flight() || you.permanent_levitation();
    const bool expiring = (!perm && dur_expiring(DUR_LEVITATION));
    const bool uncancel = you.attribute[ATTR_LEV_UNCANCELLABLE];

    if (wearing_amulet(AMU_CONTROLLED_FLIGHT))
    {
        inf->light_colour = you.light_flight() ? BLUE : MAGENTA;
        inf->light_text   = gettext(M_("Fly"));
        inf->short_text   = gettext(M_("flying"));
        inf->long_text    = gettext("You are flying.");
    }
    else
    {
        inf->light_colour = perm ? WHITE : uncancel ? BLUE : MAGENTA;
        inf->light_text   = gettext(M_("Lev"));
        inf->short_text   = gettext(M_("levitating"));
        inf->long_text    = gettext("You are hovering above the floor.");
    }
    inf->light_colour = dur_colour(inf->light_colour, expiring);
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
        if (you.rotting > 15)
            inf->long_text = gettext("Your flesh is rotting before your eyes.");
        else if (you.rotting > 8)
            inf->long_text = gettext("Your flesh is rotting away quickly");
        else if (you.rotting > 4)
            inf->long_text = gettext("Your flesh is rotting badly");
        else if (you.species == SP_GHOUL)
            inf->long_text = gettext("Your flesh is rotting faster than usual");
        else
            inf->long_text = gettext("Your flesh is rotting.");
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

        std::string mod = (you.disease > high) ? pgettext("Sick", "badly ")  :
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
        if (you.species == SP_KENKU && you.flight_mode() == FL_FLY)
        {
            if (you.travelling_light())
                inf->long_text = gettext("Your small burden allows quick flight.");
            else
                inf->long_text = gettext("Your heavy burden is slowing your flight.");
        }
        break;
    }
}

static void _describe_transform(status_info* inf)
{
    const bool vampbat = (you.species == SP_VAMPIRE && player_in_bat_form());
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
    case TRAN_NONE:
        break;
    }

    inf->light_colour = dur_colour(GREEN, expire);
    _mark_expiring(inf, expire);
}

static const char* s0_names[NUM_STATS] = { M_("Collapse"), M_("Brainless"), M_("Clumsy"), };

static void _describe_stat_zero(status_info* inf, stat_type st)
{
    if (you.stat_zero[st])
    {
        inf->light_colour = you.stat(st) ? LIGHTRED : RED;
        inf->light_text   = gettext(s0_names[st]);
        inf->short_text   = make_stringf(gettext("lost %s"), stat_desc(st, SD_NAME));
        inf->long_text    = make_stringf(you.stat(st) ?
                gettext("You are recovering from loss of %s.") : gettext("You have no %s!"),
                stat_desc(st, SD_NAME));
    }
}
