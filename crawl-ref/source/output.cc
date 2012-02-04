/**
 * @file
 * @brief Functions used to print player related info.
**/

#include "AppHdr.h"

#include "output.h"

#include <stdlib.h>
#include <sstream>
#include <math.h>

#include "externs.h"
#include "options.h"
#include "species.h"

#include "abl-show.h"
#include "areas.h"
#include "artefact.h"
#include "branch.h"
#include "cio.h"
#include "colour.h"
#include "coord.h"
#include "describe.h"
#include "format.h"
#include "godabil.h"
#include "initfile.h"
#include "itemname.h"
#include "item_use.h"
#include "menu.h"
#include "message.h"
#include "misc.h"
#include "mon-stuff.h"
#include "mon-info.h"
#include "mon-util.h"
#include "mutation.h"
#include "jobs.h"
#include "ouch.h"
#include "player.h"
#include "place.h"
#include "religion.h"
#include "skills2.h"
#include "status.h"
#include "stuff.h"
#include "tagstring.h"
#include "transform.h"
#include "travel.h"
#include "viewchar.h"
#include "viewgeom.h"
#include "showsymb.h"
#include "spl-transloc.h"

#ifndef USE_TILE_LOCAL
#include "directn.h"
#endif

static std::string _god_powers(bool simple = false);

// Color for captions like 'Health:', 'Str:', etc.
#define HUD_CAPTION_COLOUR Options.status_caption_colour

// Colour for values, which come after captions.
static const short HUD_VALUE_COLOUR = LIGHTGREY;

// ----------------------------------------------------------------------
// colour_bar
// ----------------------------------------------------------------------

class colour_bar
{
    typedef unsigned short color_t;
 public:
    colour_bar(color_t default_colour,
               color_t change_pos,
               color_t change_neg,
               color_t empty)
        : m_default(default_colour), m_change_pos(change_pos),
          m_change_neg(change_neg), m_empty(empty),
          m_old_disp(-1),
          m_request_redraw_after(0)
    {
        // m_old_disp < 0 means it's invalid and needs to be initialised.
    }

    bool wants_redraw() const
    {
        return (m_request_redraw_after
                && you.num_turns >= m_request_redraw_after);
    }

    void draw(int ox, int oy, int val, int max_val)
    {
        ASSERT(val <= max_val);
        if (max_val <= 0)
        {
            m_old_disp = -1;
            return;
        }

        const int width = crawl_view.hudsz.x - (ox-1);
        const int disp  = width * val / max_val;
        const int old_disp = (m_old_disp < 0) ? disp : m_old_disp;
        m_old_disp = disp;

        cgotoxy(ox, oy, GOTO_STAT);

        textcolor(BLACK);
        for (int cx = 0; cx < width; cx++)
        {
#ifdef USE_TILE_LOCAL
            // Maybe this should use textbackground too?
            textcolor(BLACK + m_empty * 16);

            if (cx < disp)
                textcolor(BLACK + m_default * 16);
            else if (old_disp > disp && cx < old_disp)
                textcolor(BLACK + m_change_neg * 16);
            putwch(' ');
#else
            if (cx < disp && cx < old_disp)
            {
                textcolor(m_default);
                putwch('=');
            }
            else if (cx < disp)
            {
                textcolor(m_change_pos);
                putwch('=');
            }
            else if (cx < old_disp)
            {
                textcolor(m_change_neg);
                putwch('-');
            }
            else
            {
                textcolor(m_empty);
                putwch('-');
            }
#endif

            // If some change colour was rendered, redraw in a few
            // turns to clear it out.
            if (old_disp != disp)
                m_request_redraw_after = you.num_turns + 4;
            else
                m_request_redraw_after = 0;
        }

        textcolor(LIGHTGREY);
        textbackground(BLACK);
    }

 private:
    const color_t m_default;
    const color_t m_change_pos;
    const color_t m_change_neg;
    const color_t m_empty;
    int m_old_disp;
    int m_request_redraw_after; // force a redraw at this turn count
};

static colour_bar HP_Bar(LIGHTGREEN, GREEN, RED, DARKGREY);

#ifdef USE_TILE_LOCAL
static colour_bar MP_Bar(BLUE, BLUE, LIGHTBLUE, DARKGREY);
#else
static colour_bar MP_Bar(LIGHTBLUE, BLUE, MAGENTA, DARKGREY);
#endif

// ----------------------------------------------------------------------
// Status display
// ----------------------------------------------------------------------

#ifdef DGL_SIMPLE_MESSAGING
void update_message_status()
{
    static const char *msg = "(Hit _)";
    static const int len = strwidth(msg);
    static const std::string spc(len, ' ');

    textcolor(LIGHTBLUE);

    cgotoxy(crawl_view.hudsz.x - len + 1, 1, GOTO_STAT);
    if (SysEnv.have_messages)
        cprintf(msg);
    else
        cprintf(spc.c_str());
    textcolor(LIGHTGREY);
}
#endif

void update_turn_count()
{
    // Don't update turn counter when running/resting/traveling to
    // prevent pointless screen updates.
    if (!Options.show_gold_turns
        || you.running > 0
        || you.running < 0 && Options.travel_delay == -1)
    {
        return;
    }

    cgotoxy(19+6, 9 + crawl_state.game_is_zotdef(), GOTO_STAT);

    // Show the turn count starting from 1. You can still quit on turn 0.
    textcolor(HUD_VALUE_COLOUR);
    if (Options.show_game_turns)
    {
       cprintf("%.1f (%.1f)%s", you.elapsed_time / 10.0,
               (you.elapsed_time - you.elapsed_time_at_last_input) / 10.0,
               // extra spaces to erase excess if previous output was longer
               "    ");
    }
    else
        cprintf("%d", you.num_turns);
    textcolor(LIGHTGREY);
}

static int _count_digits(int val)
{
    if (val > 999)
        return 4;
    else if (val > 99)
        return 3;
    else if (val > 9)
        return 2;
    return 1;
}

static void _print_stats_mp(int x, int y)
{
    // Calculate colour
    short mp_colour = HUD_VALUE_COLOUR;

    const bool boosted = you.duration[DUR_DIVINE_VIGOUR];

    if (boosted)
        mp_colour = LIGHTBLUE;
    else
    {
        int mp_percent = (you.max_magic_points == 0
                          ? 100
                          : (you.magic_points * 100) / you.max_magic_points);

        for (unsigned int i = 0; i < Options.mp_colour.size(); ++i)
            if (mp_percent <= Options.mp_colour[i].first)
                mp_colour = Options.mp_colour[i].second;
    }

    cgotoxy(x+8, y, GOTO_STAT);
    textcolor(mp_colour);
    cprintf("%d", you.magic_points);
    if (!boosted)
        textcolor(HUD_VALUE_COLOUR);
    cprintf("/%d", you.max_magic_points);
    if (boosted)
        textcolor(HUD_VALUE_COLOUR);

    int col = _count_digits(you.magic_points)
              + _count_digits(you.max_magic_points) + 1;
    for (int i = 11-col; i > 0; i--)
        cprintf(" ");

    MP_Bar.draw(19, y, you.magic_points, you.max_magic_points);
}

static void _print_stats_hp(int x, int y)
{
    const int max_max_hp = get_real_hp(true, true);

    // Calculate colour
    short hp_colour = HUD_VALUE_COLOUR;

    const bool boosted = you.duration[DUR_DIVINE_VIGOUR]
                             || you.berserk();

    if (boosted)
        hp_colour = LIGHTBLUE;
    else
    {
        const int hp_percent =
            (you.hp * 100) / (max_max_hp ? max_max_hp : you.hp);

        for (unsigned int i = 0; i < Options.hp_colour.size(); ++i)
            if (hp_percent <= Options.hp_colour[i].first)
                hp_colour = Options.hp_colour[i].second;
    }

    // 01234567890123456789
    // Health: xxx/yyy (zzz)
    cgotoxy(x, y, GOTO_STAT);
    textcolor(HUD_CAPTION_COLOUR);
    cprintf(max_max_hp != you.hp_max ? "HP: " : "Health: ");
    textcolor(hp_colour);
    cprintf("%d", you.hp);
    if (!boosted)
        textcolor(HUD_VALUE_COLOUR);
    cprintf("/%d", you.hp_max);
    if (max_max_hp != you.hp_max)
        cprintf(" (%d)", max_max_hp);
    if (boosted)
        textcolor(HUD_VALUE_COLOUR);

    int col = wherex() - crawl_view.hudp.x;
    for (int i = 18-col; i > 0; i--)
        cprintf(" ");

    HP_Bar.draw(19, y, you.hp, you.hp_max);
}

static short _get_stat_colour(stat_type stat)
{
    if (you.stat_zero[stat] > 0)
        return (LIGHTRED);

    // Check the stat_colour option for warning thresholds.
    for (unsigned int i = 0; i < Options.stat_colour.size(); ++i)
        if (you.stat(stat) <= Options.stat_colour[i].first)
            return (Options.stat_colour[i].second);

    // Stat is magically increased.
    if (you.duration[DUR_DIVINE_STAMINA]
        || stat == STAT_STR && you.duration[DUR_MIGHT]
        || stat == STAT_STR && you.duration[DUR_BERSERK]
        || stat == STAT_INT && you.duration[DUR_BRILLIANCE]
        || stat == STAT_DEX && you.duration[DUR_AGILITY])
    {
        return (LIGHTBLUE);  // no end of effect warning
    }

    // Stat is degenerated.
    if (you.stat_loss[stat] > 0)
        return (YELLOW);

    return (HUD_VALUE_COLOUR);
}

static void _print_stat(stat_type stat, int x, int y)
{
    cgotoxy(x+5, y, GOTO_STAT);

    textcolor(_get_stat_colour(stat));
    cprintf("%d", you.stat(stat, false));

    if (you.stat_loss[stat] > 0)
        cprintf(" (%d)  ", you.max_stat(stat));
    else
        cprintf("       ");
}

static void _print_stats_ac(int x, int y)
{
    // AC:
    cgotoxy(x+4, y, GOTO_STAT);
    if (you.duration[DUR_ICY_ARMOUR] || you.duration[DUR_STONESKIN])
        textcolor(LIGHTBLUE);
    else if (you.duration[DUR_ICEMAIL_DEPLETED] > ICEMAIL_TIME / ICEMAIL_MAX)
        textcolor(RED);
    else
        textcolor(HUD_VALUE_COLOUR);
    std::string ac = make_stringf("%2d ", you.armour_class());
#ifdef WIZARD
    if (you.wizard)
        ac += make_stringf("(%d%%) ", you.gdr_perc());
#endif
    cprintf("%s", ac.c_str());

    // SH: (two lines lower)
    cgotoxy(x+4, y+2, GOTO_STAT);
    if (you.incapacitated() && player_wearing_slot(EQ_SHIELD))
        textcolor(RED);
    else if (you.duration[DUR_CONDENSATION_SHIELD]
             || you.duration[DUR_DIVINE_SHIELD])
    {
        textcolor(LIGHTBLUE);
    }
    else
        textcolor(HUD_VALUE_COLOUR);
    cprintf("%2d ", player_shield_class());
}

static void _print_stats_ev(int x, int y)
{
    cgotoxy(x+4, y, GOTO_STAT);
    textcolor(you.duration[DUR_PHASE_SHIFT] || you.duration[DUR_AGILITY]
              ? LIGHTBLUE : HUD_VALUE_COLOUR);
    cprintf("%2d ", player_evasion());
}

static void _print_stats_wp(int y)
{
    int col;
    std::string text;
    if (you.weapon())
    {
        const item_def& wpn = *you.weapon();

        const std::string prefix = menu_colour_item_prefix(wpn);
        const int prefcol = menu_colour(wpn.name(false, DESC_INVENTORY), prefix);
        if (prefcol != -1)
            col = prefcol;
        else
            col = LIGHTGREY;

        text = wpn.name(true, DESC_INVENTORY, true, false, true);
    }
    else
    {
        const std::string prefix = "-) ";
        col = LIGHTGREY;
        text = "무장하지 않음"; // Default

        if (you.has_usable_claws(true))
            text = "손톱";
        if (you.has_usable_tentacles(true))
            text = "촉수";

        if (you.species == SP_FELID)
            text = "이빨과 손톱";

        switch (you.form)
        {
            case TRAN_SPIDER:
                col = LIGHTGREEN;
                text = "독니";
                break;
            case TRAN_BLADE_HANDS:
                col = RED;
                text = "칼날의 " + blade_parts(true);
                break;
            case TRAN_STATUE:
                col = LIGHTGREY;
                if (you.has_usable_claws(true))
                    text = "바위 손톱";
                else if (you.has_usable_tentacles(true))
                    text = "바위 촉수";
                else
                    text = "바위 주먹";
                break;
            case TRAN_ICE_BEAST:
                col = WHITE;
                text = "얼음 주먹";
                break;
            case TRAN_DRAGON:
                col = GREEN;
                text = "이빨과 발톱";
                break;
            case TRAN_LICH:
                col = MAGENTA;
                text += " (흡수)";
                break;
            case TRAN_BAT:
            case TRAN_PIG:
                col = LIGHTGREY;
                text = "궁뎅이";
                break;
            case TRAN_NONE:
            case TRAN_APPENDAGE:
            default:
                break;
        }

        text = prefix + text;
    }

    cgotoxy(1, y, GOTO_STAT);
    textcolor(Options.status_caption_colour);
    cprintf("장비: ");
    textcolor(col);
    int w = crawl_view.hudsz.x - 4;
    cprintf("%s", chop_string(text, w).c_str());
    textcolor(LIGHTGREY);
}

static void _print_stats_qv(int y)
{
    int col;
    std::string text;

    int q = you.m_quiver->get_fire_item();
    ASSERT(q >= -1 && q < ENDOFPACK);
    if (q != -1 && !fire_warn_if_impossible(true))
    {
        const item_def& quiver = you.inv[q];
        const std::string prefix = menu_colour_item_prefix(quiver);
        const int prefcol =
            menu_colour(quiver.name(false, DESC_INVENTORY), prefix);
        if (prefcol != -1)
            col = prefcol;
        else
            col = LIGHTGREY;
        text = quiver.name(true, DESC_INVENTORY, true);
    }
    else
    {
        const std::string prefix = "-) ";

        if (fire_warn_if_impossible(true))
        {
            col  = DARKGREY;
            text = "사용할 수 없음";
        }
        else
        {
            col  = LIGHTGREY;
            text = "없음";
        }

        text = prefix + text;
    }
    cgotoxy(1, y, GOTO_STAT);
    textcolor(Options.status_caption_colour);
    cprintf("보조: ");
    textcolor(col);
    int w = crawl_view.hudsz.x - 4;
    cprintf("%s", chop_string(text, w).c_str());
    textcolor(LIGHTGREY);
}

struct status_light
{
    status_light(int c, std::string t) : color(c), text(t) {}
    int color;
    std::string text;
};

// The colour scheme for these flags is currently:
//
// - yellow, "orange", red      for bad conditions
// - light grey, white          for god based conditions
// - green, light green         for good conditions
// - blue, light blue           for good enchantments
// - magenta, light magenta     for "better" enchantments (deflect, fly)
//
// Prints burden, hunger,
// pray, holy, teleport, regen, insulation, fly/lev, invis, silence,
//   conf. touch, bargain, sage
// confused, mesmerised, fire, poison, disease, rot, held, glow, swift,
//   fast, slow, breath
//
// Note the usage of bad_ench_colour() correspond to levels that
// can be found in player.cc, ie those that the player can tell by
// using the '@' command.  Things like confusion and sticky flame
// hide their amounts and are thus always the same colour (so
// we're not really exposing any new information). --bwr
static void _get_status_lights(std::vector<status_light>& out)
{
#ifdef DEBUG_DIAGNOSTICS
    {
        static char static_pos_buf[80];
        snprintf(static_pos_buf, sizeof(static_pos_buf),
                 "%2d,%2d", you.pos().x, you.pos().y);
        out.push_back(status_light(LIGHTGREY, static_pos_buf));

        static char static_hunger_buf[80];
        snprintf(static_hunger_buf, sizeof(static_hunger_buf),
                 "(%d:%d)", you.hunger - you.old_hunger, you.hunger);
        out.push_back(status_light(LIGHTGREY, static_hunger_buf));
    }
#endif

    const int statuses[] = {
        STATUS_STR_ZERO, STATUS_INT_ZERO, STATUS_DEX_ZERO,
        STATUS_BURDEN,
        STATUS_HUNGER,
        DUR_PETRIFYING,
        DUR_PETRIFIED,
        DUR_PARALYSIS,
        DUR_JELLY_PRAYER,
        DUR_TELEPORT,
        DUR_DEATHS_DOOR,
        DUR_QUAD_DAMAGE,
        DUR_DEFLECT_MISSILES,
        DUR_REPEL_MISSILES,
        STATUS_REGENERATION,
        DUR_BERSERK,
        DUR_RESIST_POISON,
        DUR_RESIST_COLD,
        DUR_RESIST_FIRE,
        DUR_INSULATION,
        DUR_SEE_INVISIBLE,
        STATUS_AIRBORNE,
        DUR_INVIS,
        DUR_CONTROL_TELEPORT,
        DUR_SILENCE,
        DUR_CONFUSING_TOUCH,
        DUR_BARGAIN,
        DUR_SAGE,
        DUR_FIRE_SHIELD,
        DUR_SLIMIFY,
        DUR_SURE_BLADE,
        DUR_CONF,
        DUR_LOWERED_MR,
        STATUS_BEHELD,
        DUR_LIQUID_FLAMES,
        DUR_MISLED,
        DUR_POISONING,
        STATUS_SICK,
        DUR_NAUSEA,
        STATUS_ROT,
        STATUS_NET,
        STATUS_CONTAMINATION,
        DUR_SWIFTNESS,
        STATUS_SPEED,
        DUR_DEATH_CHANNEL,
        DUR_TELEPATHY,
        DUR_STEALTH,
        DUR_BREATH_WEAPON,
        DUR_EXHAUSTED,
        DUR_POWERED_BY_DEATH,
        DUR_TRANSFORMATION,
        DUR_AFRAID,
        DUR_MIRROR_DAMAGE,
        DUR_SCRYING,
        STATUS_CLINGING,
        DUR_TORNADO,
        DUR_LIQUEFYING,
        DUR_HEROISM,
        DUR_FINESSE,
        DUR_LIFESAVING,
        DUR_DARKNESS,
        STATUS_FIREBALL,
        DUR_SHROUD_OF_GOLUBRIA,
        DUR_TORNADO_COOLDOWN,
        STATUS_BACKLIT,
        STATUS_UMBRA,
        STATUS_CONSTRICTED,
        DUR_DIVINE_STAMINA,
        STATUS_AUGMENTED,
    };

    status_info inf;
    for (unsigned i = 0; i < ARRAYSZ(statuses); ++i)
    {
        fill_status_info(statuses[i], &inf);
        if (!inf.light_text.empty())
        {
            status_light sl(inf.light_colour, inf.light_text);
            out.push_back(sl);
        }
    }
    if (!allow_control_teleport(true) && Options.show_no_ctele)
        out.push_back(status_light(RED,"-cTele"));
}

static void _print_status_lights(int y)
{
    std::vector<status_light> lights;
    static int last_number_of_lights = 0;
    _get_status_lights(lights);
    if (lights.empty() && last_number_of_lights == 0)
        return;
    last_number_of_lights = lights.size();

    size_t line_cur = y;
    const size_t line_end = crawl_view.hudsz.y+1;

    cgotoxy(1, line_cur, GOTO_STAT);
    ASSERT_SAVE(wherex() == crawl_view.hudp.x);

    size_t i_light = 0;
    while (true)
    {
        const int end_x = (wherex() - crawl_view.hudp.x)
                + (i_light < lights.size() ? strwidth(lights[i_light].text)
                                           : 10000);

        if (end_x <= crawl_view.hudsz.x)
        {
            textcolor(lights[i_light].color);
            cprintf("%s", lights[i_light].text.c_str());
            if (end_x < crawl_view.hudsz.x)
                cprintf(" ");
            ++i_light;
        }
        else
        {
            clear_to_end_of_line();
            ++line_cur;
            // Careful not to trip the )#(*$ cgotoxy ASSERT
            if (line_cur == line_end)
                break;
            cgotoxy(1, line_cur, GOTO_STAT);
        }
    }
}

#ifdef USE_TILE_LOCAL
static bool _need_stats_printed()
{
    return you.redraw_title
           || you.redraw_hit_points
           || you.redraw_magic_points
           || you.redraw_armour_class
           || you.redraw_evasion
           || you.redraw_stats[STAT_STR]
           || you.redraw_stats[STAT_INT]
           || you.redraw_stats[STAT_DEX]
           || you.redraw_experience
           || you.wield_change
           || you.redraw_quiver;
}
#endif

static void _redraw_title(const std::string &your_name, const std::string &job_name)
{
    std::string title = your_name + " the " + job_name;

    unsigned int in_len = strwidth(title);
    const unsigned int WIDTH = crawl_view.hudsz.x;
    if (in_len > WIDTH)
    {
        in_len -= 3;  // What we're getting back from removing "the".

        const unsigned int name_len = strwidth(your_name);
        std::string trimmed_name = your_name;

        // Squeeze name if required, the "- 8" is to not squeeze too much.
        if (in_len > WIDTH && (name_len - 8) > (in_len - WIDTH))
        {
            trimmed_name = chop_string(trimmed_name,
                                       name_len - (in_len - WIDTH) - 1);
        }

        title = trimmed_name + ", " + job_name;
    }

    // Line 1: Foo the Bar    *WIZARD*
    cgotoxy(1, 1, GOTO_STAT);
    textcolor(YELLOW);
    cprintf("%s", chop_string(title, WIDTH).c_str());
    if (you.wizard)
    {
        textcolor(LIGHTBLUE);
        cgotoxy(1 + crawl_view.hudsz.x-9, 1, GOTO_STAT);
        cprintf(" *WIZARD*");
    }
#ifdef DGL_SIMPLE_MESSAGING
    update_message_status();
#endif

    // Line 2:
    // Minotaur [of God] [Piety]
    textcolor(YELLOW);
    cgotoxy(1, 2, GOTO_STAT);
    std::string species = species_name(you.species);
    nowrap_eol_cprintf("%s", species.c_str());
    if (you.religion != GOD_NO_GOD)
    {
        std::string god = " of ";
        god += you.religion == GOD_JIYVA ? god_name_jiyva(true)
                                         : god_name(you.religion);
        nowrap_eol_cprintf("%s", god.c_str());

        std::string piety = _god_powers(true);
        if (player_under_penance())
            textcolor(RED);
        if ((unsigned int)(strwidth(species) + strwidth(god) + strwidth(piety) + 1)
            <= WIDTH)
        {
            nowrap_eol_cprintf(" %s", piety.c_str());
        }
        else if ((unsigned int)(strwidth(species) + strwidth(god) + strwidth(piety) + 1)
                  == (WIDTH + 1))
        {
            //mottled draconian of TSO doesn't fit by one symbol,
            //so we remove leading space.
            nowrap_eol_cprintf("%s", piety.c_str());
        }
    }
    else if (you.char_class == JOB_MONK && you.species != SP_DEMIGOD
             && !had_gods())
    {
        std::string godpiety = "**....";
        textcolor(DARKGREY);
        if ((unsigned int)(strwidth(species) + strwidth(godpiety) + 1) <= WIDTH)
            nowrap_eol_cprintf(" %s", godpiety.c_str());
    }

    clear_to_end_of_line();

    textcolor(LIGHTGREY);
}

void print_stats(void)
{
    cursor_control coff(false);
    textcolor(LIGHTGREY);

    // Displayed evasion is now tied to dex.
    if (you.redraw_stats[STAT_DEX])
        you.redraw_evasion = true;

    if (HP_Bar.wants_redraw())
        you.redraw_hit_points = true;
    if (MP_Bar.wants_redraw())
        you.redraw_magic_points = true;

#ifdef USE_TILE_LOCAL
    bool has_changed = _need_stats_printed();
#endif

    if (you.redraw_title)
    {
        you.redraw_title = false;
        _redraw_title(you.your_name, player_title());
    }

    if (you.redraw_hit_points)   { you.redraw_hit_points = false;   _print_stats_hp (1, 3); }
    if (you.redraw_magic_points) { you.redraw_magic_points = false; _print_stats_mp (1, 4); }
    if (you.redraw_armour_class) { you.redraw_armour_class = false; _print_stats_ac (1, 5); }
    if (you.redraw_evasion)      { you.redraw_evasion = false;      _print_stats_ev (1, 6); }

    for (int i = 0; i < NUM_STATS; ++i)
        if (you.redraw_stats[i])
            _print_stat(static_cast<stat_type>(i), 19, 5 + i);
    you.redraw_stats.init(false);

    if (you.redraw_experience)
    {
        cgotoxy(1,8, GOTO_STAT);
        textcolor(Options.status_caption_colour);
#ifdef DEBUG_DIAGNOSTICS
        cprintf("XP: ");
        textcolor(HUD_VALUE_COLOUR);
        cprintf("%d/%d (%d) ",
                you.experience_level, you.skill_cost_level, you.exp_available);
#else
        cprintf("레벨:");
        textcolor(HUD_VALUE_COLOUR);
        cprintf("%2d ", you.experience_level);
        if (you.experience_level < 27)
        {
            textcolor(Options.status_caption_colour);
            cprintf("경험: ");
            textcolor(HUD_VALUE_COLOUR);
            cprintf("%2d%% ", get_exp_progress());
        }
#endif
        if (crawl_state.game_is_zotdef())
        {
            cgotoxy(1, 9, GOTO_STAT);
            textcolor(Options.status_caption_colour);
            cprintf("ZP: ");
            textcolor(HUD_VALUE_COLOUR);
            cprintf("%d     ", you.zot_points);
        }
        you.redraw_experience = false;
    }

    int yhack = crawl_state.game_is_zotdef();

    // If Options.show_gold_turns, line 9 is Gold and Turns
    if (Options.show_gold_turns)
    {
        // Increase y-value for all following lines.
        yhack++;
        cgotoxy(1+6, 8 + yhack, GOTO_STAT);
        textcolor(HUD_VALUE_COLOUR);
        cprintf("%-6d", you.gold);
    }

    if (you.wield_change)
    {
        // weapon_change is set in a billion places; probably not all
        // of them actually mean the user changed their weapon.  Calling
        // on_weapon_changed redundantly is normally OK; but if the user
        // is wielding a bow and throwing javelins, the on_weapon_changed
        // will switch them back to arrows, which is annoying.
        // Perhaps there should be another bool besides wield_change
        // that's set in fewer places?
        // Also, it's a little bogus to change simulation state in
        // render code.  We should find a better place for this.
        you.m_quiver->on_weapon_changed();
        _print_stats_wp(9 + yhack);
    }
    you.wield_change  = false;

    if (you.species == SP_FELID)
    {
        // There are no circumstances under which Felids could quiver something.
        // Reduce line counter for status display.y
        yhack -= 1;
    }
    else if (you.redraw_quiver || you.wield_change)
        _print_stats_qv(10 + yhack);

    you.redraw_quiver = false;

    if (you.redraw_status_flags)
    {
        you.redraw_status_flags = 0;
        _print_status_lights(11 + yhack);
    }
    textcolor(LIGHTGREY);

#ifdef USE_TILE_LOCAL
    if (has_changed)
        update_screen();
#else
    update_screen();
#endif
}

static std::string _level_description_string_hud()
{
    const PlaceInfo& place = you.get_place_info();
    std::string short_name = place.short_name();

    if (place.level_type == LEVEL_DUNGEON
        && branches[place.branch].depth > 1)
    {
        short_name += make_stringf(":%d", player_branch_depth());
    }
    // Indefinite articles
    else if (place.level_type == LEVEL_PORTAL_VAULT
             || place.level_type == LEVEL_LABYRINTH)
    {
        if (!you.level_type_name.empty())
        {
            // If the level name is faking a dungeon depth
            // (i.e., "Ziggurat:3") then don't add an article
            if (you.level_type_name.find(":") != std::string::npos)
                short_name = you.level_type_name;
            else
                short_name = article_a(uppercase_first(you.level_type_name),
                                       false);
        }
        else
            short_name.insert(0, "A ");
    }
    // Definite articles
    else if (place.level_type == LEVEL_ABYSS)
        short_name.insert(0, "The ");
    return short_name;
}

void print_stats_level()
{
    cgotoxy(15, 8, GOTO_STAT);
    textcolor(HUD_CAPTION_COLOUR);
    cprintf("장소:  ");

    textcolor(HUD_VALUE_COLOUR);
#ifdef DEBUG_DIAGNOSTICS
    cprintf("(%d) ", you.absdepth0 + 1);
#endif
    cprintf("%s", _level_description_string_hud().c_str());
    clear_to_end_of_line();
}

void draw_border(void)
{
    textcolor(HUD_CAPTION_COLOUR);
    clrscr();

    textcolor(Options.status_caption_colour);

    //cgotoxy(1, 3, GOTO_STAT); cprintf("Hp:");
    cgotoxy(1, 4, GOTO_STAT); cprintf("MP   :");
    cgotoxy(1, 5, GOTO_STAT); cprintf("AC:");
    cgotoxy(1, 6, GOTO_STAT); cprintf("EV:");
    cgotoxy(1, 7, GOTO_STAT); cprintf("SH:");

    cgotoxy(19, 5, GOTO_STAT); cprintf("힘  :");
    cgotoxy(19, 6, GOTO_STAT); cprintf("지능:");
    cgotoxy(19, 7, GOTO_STAT); cprintf("민첩:");

    if (Options.show_gold_turns)
    {
        int yhack = crawl_state.game_is_zotdef();
        cgotoxy(1, 9 + yhack, GOTO_STAT); cprintf("소지금:");
        cgotoxy(19, 9 + yhack, GOTO_STAT); cprintf("턴:");
    }
    // Line 8 is exp pool, Level
}

// ----------------------------------------------------------------------
// Monster pane
// ----------------------------------------------------------------------

static std::string _get_monster_name(const monster_info& mi,
                                     int count, bool fullname)
{
    std::string desc = "";

    bool adj = false;
    if (mi.attitude == ATT_FRIENDLY)
    {
        desc += "friendly ";
        adj = true;
    }
    else if (mi.attitude != ATT_HOSTILE)
    {
        desc += "neutral ";
        adj = true;
    }

    std::string monpane_desc;
    int col;
    mi.to_string(count, monpane_desc, col, fullname);

#ifndef KR
    if (count == 1) 
    {
        if (!mi.is(MB_NAME_THE))
        {
            desc = ((!adj && is_vowel(monpane_desc[0])) ? "an "
                                                        : "a ")
                   + desc;
        }
        else if (adj)
            desc = "the " + desc;
    } 
#endif

    desc += monpane_desc;
    return (desc);
}

// If past is true, the messages should be printed in the past tense
// because they're needed for the morgue dump.
std::string mpr_monster_list(bool past)
{
    // Get monsters via the monster_pane_info, sorted by difficulty.
    std::vector<monster_info> mons;
    get_monster_info(mons);

    std::string msg = "";
    if (mons.empty())
    {
        //msg  = "There ";
        //msg += (past ? "were" : "are");
        //msg += " no monsters in sight!";
        msg += make_stringf("시야 내에 몬스터가 %s!",(past ? "없었다" : "없다"));
        return (msg);
    }

    std::vector<std::string> describe;

    int count = 0;
    for (unsigned int i = 0; i < mons.size(); ++i)
    {
        if (i > 0 && monster_info::less_than(mons[i-1], mons[i]))
        {
            describe.push_back(_get_monster_name(mons[i-1], count, true).c_str());
            count = 0;
        }
        count++;
    }

    describe.push_back(_get_monster_name(mons[mons.size()-1], count, true).c_str());

    msg = "당신은 ";
	
    if (describe.size() == 1)
        msg += describe[0];
    else
        msg += comma_separated_line(describe.begin(), describe.end());
	msg += (past ? "을(를) 볼 수 있었다." : "을(를) 보았다."); //msg += ".";

    return (msg);
}

#ifndef USE_TILE_LOCAL
static void _print_next_monster_desc(const std::vector<monster_info>& mons,
                                     int& start, bool zombified = false,
                                     int idx = -1)
{
    // Skip forward to past the end of the range of identical monsters.
    unsigned int end;
    for (end = start + 1; end < mons.size(); ++end)
    {
        // Array is sorted, so if !(m1 < m2), m1 and m2 are "equal".
        if (monster_info::less_than(mons[start], mons[end], zombified, zombified))
            break;
    }
    // Postcondition: all monsters in [start, end) are "equal"

    // Print info on the monsters we've found.
    {
        int printed = 0;

        // for targetting
        if (idx >= 0)
        {
            textcolor(WHITE);
            cprintf(stringize_glyph(mlist_index_to_letter(idx)).c_str());
            cprintf(" - ");
            printed += 4;
        }

        // One glyph for each monster.
        for (unsigned int i_mon = start; i_mon < end; i_mon++)
        {
            monster_info mi = mons[i_mon];
            glyph g = get_mons_glyph(mi);
            textcolor(g.col);
            cprintf("%s", stringize_glyph(g.ch).c_str());
            ++printed;

            // Printing too many looks pretty bad, though.
            if (i_mon > 6)
                break;
        }
        textcolor(LIGHTGREY);

        const int count = (end - start);

        if (count == 1)
        {
            // Print an "icon" representing damage level.
            monster_info mi = mons[start];

            int dam_color;
            switch (mi.dam)
            {
            // NOTE: In os x, light versions of foreground colors are OK,
            //       but not background colors.  So stick wth standards.
            case MDAM_DEAD:
            case MDAM_ALMOST_DEAD:
            case MDAM_SEVERELY_DAMAGED:   dam_color = RED;       break;
            case MDAM_HEAVILY_DAMAGED:    dam_color = MAGENTA;   break;
            case MDAM_MODERATELY_DAMAGED: dam_color = BROWN;     break;
            case MDAM_LIGHTLY_DAMAGED:    dam_color = GREEN;     break;
            case MDAM_OKAY:               dam_color = GREEN;     break;
            default:                      dam_color = CYAN;      break;
            }

            if (!mons_class_can_display_wounds(mi.type))
                dam_color = BLACK;

            cprintf(" ");
            textbackground(dam_color);
            textcolor(dam_color);
            // FIXME: On Windows, printing a blank space here
            // doesn't give us the correct colours. So use and
            // underscore instead. Is this a bug with our interface
            // or with Windows?
            cprintf("_");
            textbackground(BLACK);
            textcolor(LIGHTGREY);
            cprintf(" ");
            printed += 3;
        }
        else
        {
            textcolor(LIGHTGREY);
            cprintf("  ");
            printed += 2;
        }

        if (printed < crawl_view.mlistsz.x)
        {
            int desc_color;
            std::string desc;
            mons[start].to_string(count, desc, desc_color, zombified);
            textcolor(desc_color);
            desc.resize(crawl_view.mlistsz.x-printed, ' ');
            cprintf("%s", desc.c_str());
        }
    }

    // Set start to the next un-described monster.
    start = end;
    textcolor(LIGHTGREY);
}
#endif

#ifndef USE_TILE_LOCAL
// #define BOTTOM_JUSTIFY_MONSTER_LIST
// Returns -1 if the monster list is empty, 0 if there are so many monsters
// they have to be consolidated, and 1 otherwise.
int update_monster_pane()
{
    if (!map_bounds(you.pos()) && !crawl_state.game_is_arena())
        return (-1);

    const int max_print = crawl_view.mlistsz.y;
    textbackground(BLACK);

    if (max_print <= 0)
        return (-1);

    std::vector<monster_info> mons;
    get_monster_info(mons);

    // Count how many groups of monsters there are.
    unsigned int lines_needed = mons.size();
    for (unsigned int i = 1; i < mons.size(); i++)
        if (!monster_info::less_than(mons[i-1], mons[i]))
            --lines_needed;

    bool full_info = true;
    if (lines_needed > (unsigned int) max_print)
    {
        full_info = false;

        // Use type names rather than full names ("small zombie" vs
        // "rat zombie") in order to take up fewer lines.

        lines_needed = mons.size();
        for (unsigned int i = 1; i < mons.size(); i++)
            if (!monster_info::less_than(mons[i-1], mons[i], false, false))
                --lines_needed;
    }

#ifdef BOTTOM_JUSTIFY_MONSTER_LIST
    const int skip_lines = std::max<int>(0, crawl_view.mlistsz.y-lines_needed);
#else
    const int skip_lines = 0;
#endif

    // Print the monsters!
    std::string blank;
    blank.resize(crawl_view.mlistsz.x, ' ');
    int i_mons = 0;
    for (int i_print = 0; i_print < max_print; ++i_print)
    {
        cgotoxy(1, 1 + i_print, GOTO_MLIST);
        // i_mons is incremented by _print_next_monster_desc
        if (i_print >= skip_lines && i_mons < (int) mons.size())
        {
             int idx = crawl_state.mlist_targetting ? i_print - skip_lines : -1;
             _print_next_monster_desc(mons, i_mons, full_info, idx);
        }
        else
            cprintf("%s", blank.c_str());
    }

    if (i_mons < (int)mons.size())
    {
        // Didn't get to all of them.
        cgotoxy(crawl_view.mlistsz.x - 4, crawl_view.mlistsz.y, GOTO_MLIST);
        cprintf(" ... ");
    }

    if (mons.empty())
        return (-1);

    return (full_info);
}
#else
// FIXME: Implement this for Tiles!
int update_monster_pane()
{
    return (false);
}
#endif

static const char* _itosym1(int stat)
{
    return ((stat >= 1) ? "+  " :
            (stat == 0) ? ".  " :
                          "x  ");
}

static const char* _itosym2(int stat)
{
    return ((stat >= 2) ? "+ +" :
            (stat == 1) ? "+ ." :
                          ". .");
}

static const char* _itosym3(int stat)
{
    return ((stat >=  3) ? "+ + +" :
            (stat ==  2) ? "+ + ." :
            (stat ==  1) ? "+ . ." :
            (stat ==  0) ? ". . ." :
            (stat == -1) ? "x . ." :
            (stat == -2) ? "x x ." :
                           "x x x");
}

static const char *s_equip_slot_names[] =
{
    "무기", "망토", "투구", "장갑", "신발", //"Weapon", "Cloak",  "Helmet", "Gloves", "Boots",
    "방패", "갑옷", "왼손", "오른손", "부적", // "Shield", "Armour", "Left Ring", "Right Ring", "Amulet",
    "반지1", "반지2", "반지3", "반지4", //	"First Ring", "Second Ring", "Third Ring", "Fourth Ring",
    "반지5", "반지6", "반지7", "반지8", //"Fifth Ring", "Sixth Ring", "Seventh Ring", "Eighth Ring",
};

const char *equip_slot_to_name(int equip)
{
    if (equip == EQ_RINGS
        || equip == EQ_LEFT_RING || equip == EQ_RIGHT_RING
        || equip >= EQ_RING_ONE && equip <= EQ_RING_EIGHT)
    {
        return "반지";
    }

    if (equip == EQ_BOOTS
        && (you.species == SP_CENTAUR || you.species == SP_NAGA))
    {
        return "마갑";
    }

    if (equip < 0 || equip >= NUM_EQUIP)
        return "";

    return s_equip_slot_names[equip];
}

int equip_name_to_slot(const char *s)
{
    for (int i = 0; i < NUM_EQUIP; ++i)
        if (!strcasecmp(s_equip_slot_names[i], s))
            return i;

    return -1;
}

// Colour the string according to the level of an ability/resistance.
// Take maximum possible level into account.
static const char* _determine_colour_string(int level, int max_level)
{
    switch (level)
    {
    case 3:
    case 2:
        if (max_level > 1)
            return "<lightgreen>";
        // else fall-through
    case 1:
        return "<green>";
    case -2:
    case -3:
        if (max_level > 1)
            return "<lightred>";
        // else fall-through
    case -1:
        return "<red>";
    default:
        return "<lightgrey>";
    }
}

static std::string _status_mut_abilities(int sw);

// helper for print_overview_screen
static void _print_overview_screen_equip(column_composer& cols,
                                         std::vector<char>& equip_chars)
{
    const int e_order[] =
    {
        EQ_WEAPON, EQ_BODY_ARMOUR, EQ_SHIELD, EQ_HELMET, EQ_CLOAK,
        EQ_GLOVES, EQ_BOOTS, EQ_AMULET, EQ_RIGHT_RING, EQ_LEFT_RING,
        EQ_RING_ONE, EQ_RING_TWO, EQ_RING_THREE, EQ_RING_FOUR,
        EQ_RING_FIVE, EQ_RING_SIX, EQ_RING_SEVEN, EQ_RING_EIGHT,
    };

    char buf[100];
    for (int i = 0; i < NUM_EQUIP; i++)
    {
        int eqslot = e_order[i];

        if (you.species == SP_OCTOPODE
            && e_order[i] != EQ_WEAPON
            && !you_can_wear(e_order[i], true))
        {
            continue;
        }

        if (you.species == SP_OCTOPODE && (eqslot == EQ_RIGHT_RING
                                       || eqslot == EQ_LEFT_RING))
        {
            continue;
        }

        if (you.species != SP_OCTOPODE && eqslot > EQ_AMULET)
            continue;

        const std::string slot_name_lwr = lowercase_string(equip_slot_to_name(eqslot));

        char slot[15] = "";
        // uncomment (and change 42 to 33) to bring back slot names
        // snprintf(slot, sizeof slot, "%-7s: ", equip_slot_to_name(eqslot);

        if (you.equip[ e_order[i] ] != -1)
        {
            // The player has something equipped.
            const int item_idx   = you.equip[e_order[i]];
            const item_def& item = you.inv[item_idx];
            const bool melded    = !player_wearing_slot(e_order[i]);
            const std::string prefix = menu_colour_item_prefix(item);
            const int prefcol = menu_colour(item.name(false, DESC_INVENTORY), prefix);
            const int col = prefcol == -1 ? LIGHTGREY : prefcol;

            // Colour melded equipment dark grey.
            const char* colname  = melded ? "darkgrey"
                                          : colour_to_str(col).c_str();

            const char equip_char = index_to_letter(item_idx);

            snprintf(buf, sizeof buf,
                     "%s<w>%c</w> - <%s>%s%s</%s>",
                     slot,
                     equip_char,
                     colname,
                     melded ? "melded " : "",
                     chop_string(item.name(true, DESC_PLAIN, true), 42, false).c_str(),
                     colname);
            equip_chars.push_back(equip_char);
        }
        else if (e_order[i] == EQ_WEAPON
                 && you.skill(SK_UNARMED_COMBAT))
        {
            snprintf(buf, sizeof buf, "%s  - 무장하지 않음", slot);
        }
        else if (e_order[i] == EQ_WEAPON
                 && you.form == TRAN_BLADE_HANDS)
        {
            snprintf(buf, sizeof buf, "%s  - 칼날의 손", slot);
        }
        else if (e_order[i] == EQ_BOOTS
                 && (you.species == SP_NAGA || you.species == SP_CENTAUR))
        {
            snprintf(buf, sizeof buf,
                     gettext("<darkgrey>(no %s)</darkgrey>"), slot_name_lwr.c_str());
        }
        else if (!you_can_wear(e_order[i], true))
        {
            snprintf(buf, sizeof buf,
                     gettext("<darkgrey>(%s unavailable)</darkgrey>"), slot_name_lwr.c_str());
        }
        else if (!you_tran_can_wear(e_order[i], true))
        {
            snprintf(buf, sizeof buf,
                     gettext("<darkgrey>(%s currently unavailable)</darkgrey>"),
                     slot_name_lwr.c_str());
        }
        else if (!you_can_wear(e_order[i]))
        {
            snprintf(buf, sizeof buf,
                     gettext("<darkgrey>(%s restricted)</darkgrey>"), slot_name_lwr.c_str());
        }
        else
        {
            snprintf(buf, sizeof buf,
                     gettext("<darkgrey>(no %s)</darkgrey>"), slot_name_lwr.c_str());
        }
        cols.add_formatted(2, buf, false);
    }
}

static std::string _overview_screen_title(int sw)
{
    char species_job[50];
    snprintf(species_job, sizeof species_job,
             "%s %s ",
             gettext(species_name(you.species).c_str()),
             gettext(you.class_name.c_str()));

    char title[50];
    snprintf(title, sizeof title, "%s ", gettext(player_title().c_str()));

    char time_turns[50] = "";

    handle_real_time();
    snprintf(time_turns, sizeof time_turns,
             "총 턴수: %d, 플레이 시간: %s",
             you.num_turns, make_time_string(you.real_time, true).c_str());

    int linelength = strwidth(you.your_name) + strwidth(title)
                     + strwidth(species_job) + strwidth(time_turns);
    for (int count = 0; linelength >= sw && count < 2;
         count++)
    {
        switch (count)
        {
          case 0:
              snprintf(species_job, sizeof species_job,
                       "(%s%s)",
                       get_species_abbrev(you.species),
                       get_job_abbrev(you.char_class));
              break;
          case 1:
              strcpy(title, "");
              break;
          default:
              break;
        }
        linelength = strwidth(you.your_name) + strwidth(title)
                     + strwidth(species_job) + strwidth(time_turns);
    }

    std::string text;
    text = "<yellow>";
	text += title;
    text += species_job;
    text += you.your_name;


    const int num_spaces = sw - linelength - 1;
    if (num_spaces > 0)
        text += std::string(num_spaces, ' ');

    text += time_turns;
    text += "</yellow>\n";

    return text;
}

#ifdef WIZARD
static std::string _wiz_god_powers()
{
    std::string godpowers = god_name(you.religion);
    return (make_stringf("%s %d (%d)", god_name(you.religion).c_str(),
                                       you.piety,
                                       you.duration[DUR_PIETY_POOL]));
}
#endif

static std::string _god_powers(bool simple)
{
    std::string godpowers = simple ? "" : god_name(you.religion) ;
    if (you.religion == GOD_XOM)
    {
        if (!you.gift_timeout)
            godpowers += simple ? "- 지루함" : " - 지루함";
        else if (you.gift_timeout == 1)
            godpowers += simple ? "- 지루해짐" : " - 지루해짐";
        return (simple ? godpowers
                       : colour_string(godpowers, god_colour(you.religion)));
    }
    else if (you.religion != GOD_NO_GOD)
    {
        if (player_under_penance())
            return (simple ? "*"
                           : colour_string("*" + godpowers, RED));
        else
        {
            // piety rankings
            int prank = piety_rank() - 1;
            if (prank < 0 || you.religion == GOD_XOM)
                prank = 0;

            // Careful about overflow. We erase some of the god's name
            // if necessary.
            std::string asterisks = std::string(prank, '*')
                                    + std::string(6 - prank, '.');
            if (simple)
                return (asterisks);
            godpowers = chop_string(godpowers, 20, false)
                      + " [" + asterisks + "]";
            return (colour_string(godpowers, god_colour(you.religion)));
        }
    }
    return "";
}

static std::vector<formatted_string> _get_overview_stats()
{
    char buf[1000];

    // 4 columns
    column_composer cols1(4, 18, 28, 40);

    const bool boosted_hp  = you.duration[DUR_DIVINE_VIGOUR]
                                || you.berserk();
    const bool boosted_mp  = you.duration[DUR_DIVINE_VIGOUR];
    const bool boosted_str = you.duration[DUR_DIVINE_STAMINA]
                                || you.duration[DUR_MIGHT]
                                || you.duration[DUR_BERSERK];
    const bool boosted_int = you.duration[DUR_DIVINE_STAMINA]
                                || you.duration[DUR_BRILLIANCE];
    const bool boosted_dex = you.duration[DUR_DIVINE_STAMINA]
                                || you.duration[DUR_AGILITY];

    if (!player_rotted())
    {
        if (boosted_hp)
        {
            snprintf(buf, sizeof buf, "HP <lightblue>%3d/%d</lightblue>",
                     you.hp, you.hp_max);
        }
        else
            snprintf(buf, sizeof buf, "HP %3d/%d", you.hp, you.hp_max);
    }
    else
    {
        if (boosted_hp)
        {
            snprintf(buf, sizeof buf, "HP <lightblue>%3d/%d (%d)</lightblue>",
                     you.hp, you.hp_max, get_real_hp(true, true));
        }
        else
        {
            snprintf(buf, sizeof buf, "HP %3d/%d (%d)",
                     you.hp, you.hp_max, get_real_hp(true, true));
        }
    }
    cols1.add_formatted(0, buf, false);

    if (boosted_mp)
    {
        snprintf(buf, sizeof buf, "MP <lightblue>%3d/%d</lightblue>",
                 you.magic_points, you.max_magic_points);
    }
    else
    {
        snprintf(buf, sizeof buf, "MP %3d/%d",
                 you.magic_points, you.max_magic_points);
    }
    cols1.add_formatted(0, buf, false);

    snprintf(buf, sizeof buf, "소지금 %d", you.gold);
    cols1.add_formatted(0, buf, false);

    snprintf(buf, sizeof buf, "AC %2d" , you.armour_class());
    cols1.add_formatted(1, buf, false);

    if (you.duration[DUR_AGILITY])
    {
        snprintf(buf, sizeof buf, "EV <lightblue>%2d</lightblue>",
                 player_evasion());
    }
    else
        snprintf(buf, sizeof buf, "EV %2d", player_evasion());
    cols1.add_formatted(1, buf, false);

    snprintf(buf, sizeof buf, "SH %2d", player_shield_class());
    cols1.add_formatted(1, buf, false);

    if (you.strength() == you.max_strength())
    {
        if (boosted_str)
        {
            snprintf(buf, sizeof buf, "힘   <lightblue>%2d</lightblue>",
                     you.strength());
        }
        else
            snprintf(buf, sizeof buf, "힘   %2d", you.strength());
    }
    else
    {
        if (boosted_str)
        {
            snprintf(buf, sizeof buf, "힘   <lightblue>%2d (%d)</lightblue>",
                     you.strength(), you.max_strength());
        }
        else
            snprintf(buf, sizeof buf, "힘   <yellow>%2d</yellow> (%d)",
                     you.strength(), you.max_strength());
    }
    cols1.add_formatted(2, buf, false);

    if (you.intel() == you.max_intel())
    {
        if (boosted_int)
        {
            snprintf(buf, sizeof buf, "지능 <lightblue>%2d</lightblue>",
                     you.intel());
        }
        else
            snprintf(buf, sizeof buf, "지능 %2d", you.intel());
    }
    else
    {
        if (boosted_int)
        {
            snprintf(buf, sizeof buf, "지능 <lightblue>%2d (%d)</lightblue>",
                     you.intel(), you.max_intel());
        }
        else
            snprintf(buf, sizeof buf, "지능 <yellow>%2d</yellow> (%d)",
                     you.intel(), you.max_intel());
    }
    cols1.add_formatted(2, buf, false);

    if (you.dex() == you.max_dex())
    {
        if (boosted_dex)
        {
            snprintf(buf, sizeof buf, "민첩 <lightblue>%2d</lightblue>",
                     you.dex());
        }
        else
            snprintf(buf, sizeof buf, "민첩 %2d", you.dex());
    }
    else
    {
        if (boosted_dex)
        {
            snprintf(buf, sizeof buf, "민첩 <lightblue>%2d (%d)</lightblue>",
                     you.dex(), you.max_dex());
        }
        else
            snprintf(buf, sizeof buf, "민첩 <yellow>%2d</yellow> (%d)",
                     you.dex(), you.max_dex());
    }
    cols1.add_formatted(2, buf, false);

    std::string godpowers = _god_powers(false);
#ifdef WIZARD
    if (you.wizard)
        godpowers = _wiz_god_powers();
#endif

    char lives[40];
    if (you.species == SP_FELID)
    {
        snprintf(lives, sizeof(lives), "생명: %d, 죽은 횟수: %d",
                 you.lives, you.deaths);
    }
    else
        lives[0] = 0;

    snprintf(buf, sizeof buf,
             "레벨: %d%s\n"
             "신앙: %s\n"
             "주문: %2d레벨 기억함, %2d레벨%s 남음\n"
             "%s",
             you.experience_level,
             (you.experience_level < 27 ? make_stringf(gettext("   Next: %2d%%"),
                                                   get_exp_progress()).c_str()
                                        : ""),
             godpowers.c_str(),
             you.spell_no, player_spell_levels(),
             (player_spell_levels() == 1) ? "" : "",
             lives);
    cols1.add_formatted(3, buf, false);

    return cols1.formatted_lines();
}

static std::vector<formatted_string> _get_overview_resistances(
    std::vector<char> &equip_chars,
    bool calc_unid = false)
{
    char buf[1000];

    // 3 columns, splits at columns 21, 39
    column_composer cols(3, 21, 39);

    // Don't show unreliable resistances granted by the cloak.  We could mark
    // them somehow, but for now this will do.
    bool dragonskin = player_equip_unrand(UNRAND_DRAGONSKIN);
    bool cloak_was_melded = you.melded[EQ_CLOAK];
    if (dragonskin)
        you.melded[EQ_CLOAK] = true; // hack!

    const int rfire = player_res_fire(calc_unid);
    const int rcold = player_res_cold(calc_unid);
    const int rlife = player_prot_life(calc_unid);
    const int racid = player_res_acid(calc_unid);
    const int rpois = player_res_poison(calc_unid);
    const int relec = player_res_electricity(calc_unid);
    const int rsust = player_sust_abil(calc_unid);
    const int rmuta = (wearing_amulet(AMU_RESIST_MUTATION, calc_unid)
                       || player_mutation_level(MUT_MUTATION_RESISTANCE) == 3
                       || you.religion == GOD_ZIN && you.piety >= 150);
    const int rrott = (you.res_rotting()
                       || you.religion == GOD_ZIN && you.piety >= 150);

    snprintf(buf, sizeof buf,
             "%s화염 저항 : %s\n"
             "%s냉기 저항 : %s\n"
             "%s약화 저항 : %s\n"
             "%s산성 저항 : %s\n"
             "%s독소 저항 : %s\n"
             "%s전기 저항 : %s\n"
             "%s능력 유지 : %s\n"
             "%s변이 저항 : %s\n"
             "%s부패 저항 : %s\n",
             _determine_colour_string(rfire, 3), _itosym3(rfire),
             _determine_colour_string(rcold, 3), _itosym3(rcold),
             _determine_colour_string(rlife, 3), _itosym3(rlife),
             _determine_colour_string(racid, 3), _itosym3(racid),
             _determine_colour_string(rpois, 1), _itosym1(rpois),
             _determine_colour_string(relec, 1), _itosym1(relec),
             _determine_colour_string(rsust, 2), _itosym2(rsust),
             _determine_colour_string(rmuta, 1), _itosym1(rmuta),
             _determine_colour_string(rrott, 1), _itosym1(rrott));
    cols.add_formatted(0, buf, false);

    int saplevel = player_mutation_level(MUT_SAPROVOROUS);
    const char* pregourmand;
    const char* postgourmand;

    if (wearing_amulet(AMU_THE_GOURMAND, calc_unid))
    {
        pregourmand = "대식가    : ";
        postgourmand = _itosym1(1);
        saplevel = 1;
    }
    else
    {
        pregourmand = "육식성    : ";
        postgourmand = _itosym3(saplevel);
    }
    snprintf(buf, sizeof buf, "%s%s%s",
             _determine_colour_string(saplevel, 3), pregourmand, postgourmand);
    cols.add_formatted(0, buf, false);


    const int rinvi = you.can_see_invisible(calc_unid);
    const int rward = player_warding(calc_unid);
    const int rcons = player_item_conserve(calc_unid);
    const int rcorr = player_res_corr(calc_unid);
    const int rclar = player_mental_clarity(calc_unid);
    const int rspir = player_spirit_shield(calc_unid);
    snprintf(buf, sizeof buf,
             "%s투명 보기  : %s\n"
             "%s결계       : %s\n"
             "%s물품 보호  : %s\n"
             "%s부식 저지  : %s\n"
             "%s명석함     : %s\n"
             "%s수호 정령  : %s\n"
             ,
             _determine_colour_string(rinvi, 1), _itosym1(rinvi),
             _determine_colour_string(rward, 2), _itosym2(rward),
             _determine_colour_string(rcons, 1), _itosym1(rcons),
             _determine_colour_string(rcorr, 1), _itosym1(rcorr),
             _determine_colour_string(rclar, 1), _itosym1(rclar),
             _determine_colour_string(rspir, 1), _itosym1(rspir));
    cols.add_formatted(1, buf, false);

    const int stasis = wearing_amulet(AMU_STASIS, calc_unid);
    const int notele = scan_artefacts(ARTP_PREVENT_TELEPORTATION, calc_unid)
                       || crawl_state.game_is_zotdef() && orb_haloed(you.pos());
    const int rrtel = !!player_teleport(calc_unid);
    if (notele && !stasis)
    {
        snprintf(buf, sizeof buf, "%s전이방해   : %s",
                 _determine_colour_string(-1, 1), _itosym1(1));
    }
    else
    if (rrtel && !stasis)
    {
        snprintf(buf, sizeof buf, "%s랜덤이동   : %s",
                 _determine_colour_string(-1, 1), _itosym1(1));
    }
    else
    {
        snprintf(buf, sizeof buf, "%s정체       : %s",
                 _determine_colour_string(stasis, 1), _itosym1(stasis));
    }
    cols.add_formatted(1, buf, false);

    int rctel = player_control_teleport(calc_unid);
    rctel = allow_control_teleport(true) ? rctel : -1;
    const int rlevi = you.airborne();
    const int rcfli = wearing_amulet(AMU_CONTROLLED_FLIGHT, calc_unid);
    snprintf(buf, sizeof buf,
             "%s좌표이동   : %s\n"
             "%s공중부양   : %s\n"
             "%s비행제어   : %s\n",
             _determine_colour_string(rctel, 1), _itosym1(rctel),
             _determine_colour_string(rlevi, 1), _itosym1(rlevi),
             _determine_colour_string(rcfli, 1), _itosym1(rcfli));
    cols.add_formatted(1, buf, false);

    you.melded[EQ_CLOAK] = cloak_was_melded;

    _print_overview_screen_equip(cols, equip_chars);

    return cols.formatted_lines();
}

// New scrollable status overview screen, including stats, mutations etc.
static char _get_overview_screen_results()
{
    bool calc_unid = false;
    formatted_scroller overview;
    overview.set_flags(MF_SINGLESELECT | MF_ALWAYS_SHOW_MORE | MF_NOWRAP);
    overview.set_more(formatted_string::parse_string(
// FIXME later
#ifdef USE_TILE_LOCAL
                        "<cyan>[ +/L-click : Page down.   - : Page up."
                        "           Esc/R-click exits.]"));
#else
                        "<cyan>[ + : Page down.   - : Page up."
                        "                           Esc exits.]"));
#endif
    overview.set_tag("resists");

    overview.add_text(_overview_screen_title(get_number_of_cols()));

    {
        std::vector<formatted_string> blines = _get_overview_stats();
        for (unsigned int i = 0; i < blines.size(); ++i)
            overview.add_item_formatted_string(blines[i]);
        overview.add_text(" ");
    }


    {
        std::vector<char> equip_chars;
        std::vector<formatted_string> blines =
            _get_overview_resistances(equip_chars, calc_unid);

        for (unsigned int i = 0; i < blines.size(); ++i)
        {
            // Kind of a hack -- we don't really care what items these
            // hotkeys go to.  So just pick the first few.
            const char hotkey = (i < equip_chars.size()) ? equip_chars[i] : 0;
            overview.add_item_formatted_string(blines[i], hotkey);
        }
    }

    overview.add_text(" ");
    overview.add_text(_status_mut_abilities(get_number_of_cols()));

    std::vector<MenuEntry *> results = overview.show();
    return (!results.empty()) ? results[0]->hotkeys[0] : 0;
}

std::string dump_overview_screen(bool full_id)
{
    std::string text = formatted_string::parse_string(_overview_screen_title(80));
    text += "\n";

    std::vector<formatted_string> blines = _get_overview_stats();
    for (unsigned int i = 0; i < blines.size(); ++i)
    {
        text += blines[i];
        text += "\n";
    }
    text += "\n";

    std::vector<char> equip_chars;
    blines = _get_overview_resistances(equip_chars, full_id);
    for (unsigned int i = 0; i < blines.size(); ++i)
    {
        text += blines[i];
        text += "\n";
    }
    text += "\n";

    text += formatted_string::parse_string(_status_mut_abilities(80));
    text += "\n";

    return text;
}

void print_overview_screen()
{
    while (true)
    {
        char c = _get_overview_screen_results();
        if (!c)
        {
            redraw_screen();
            break;
        }

        item_def& item = you.inv[letter_to_index(c)];
        describe_item(item, true);
        // loop around for another go.
    }
}

std::string stealth_desc(int stealth)
{
    std::string prefix =
         (stealth <  10) ? "극히 미미한" :
         (stealth <  30) ? "매우 약한" :
         (stealth <  60) ? "약한" :
         (stealth <  90) ? "보통의 " :
         (stealth < 120) ? "" :
         (stealth < 160) ? "상당한 " :
         (stealth < 220) ? "강한 " :
         (stealth < 300) ? "아주 강한 " :
         (stealth < 400) ? "대단히 강한 " :
         (stealth < 520) ? "극히 강한 "
                         : "거의 보이지 않는 ";
    return (prefix + "은밀함");
}

std::string magic_res_adjective(int mr)
{
    if (mr == MAG_IMMUNE)
        return "면역";

    return ((mr <  10) ? "저항력이 없다" :
            (mr <  30) ? "약간의 저항력이 있다" :
            (mr <  60) ? "어느정도 저항력이 있다" :
            (mr <  90) ? "꽤 저항력이 있다" :
            (mr < 120) ? "큰 저항력이 있다" :
            (mr < 150) ? "아주 큰 저항력이 있다" :
            (mr < 190) ? "대단히 큰 저항력이 있다" :
            (mr < 240) ? "극도로 큰 저항력이 있다 " :
            (mr < 300) ? "거의 면역이다"
                       : "면역이다");
}

static std::string _annotate_form_based(std::string desc, bool suppressed)
{
    if (suppressed)
        return ("<darkgrey>(" + desc + ")</darkgrey>");
    else
        return (desc);
}

static std::string _dragon_abil(std::string desc)
{
    const bool supp = form_changed_physiology() && you.form != TRAN_DRAGON;
    return _annotate_form_based(desc, supp);
}

// Creates rows of short descriptions for current
// status, mutations and abilities.
static std::string _status_mut_abilities(int sw)
{
    //----------------------------
    // print status information
    //----------------------------
    std::string text = "<w>@:</w> ";
    std::vector<std::string> status;

    const int statuses[] = {
        DUR_TRANSFORMATION,
        DUR_PARALYSIS,
        DUR_PETRIFIED,
        DUR_SLEEP,
        DUR_PETRIFYING,
        STATUS_BURDEN,
        STATUS_STR_ZERO, STATUS_INT_ZERO, STATUS_DEX_ZERO,
        DUR_BREATH_WEAPON,
        STATUS_BEHELD,
        DUR_LIQUID_FLAMES,
        DUR_ICY_ARMOUR,
        DUR_DEFLECT_MISSILES,
        DUR_REPEL_MISSILES,
        DUR_JELLY_PRAYER,
        STATUS_REGENERATION,
        DUR_DEATHS_DOOR,
        DUR_STONESKIN,
        DUR_TELEPORT,
        DUR_DEATH_CHANNEL,
        DUR_PHASE_SHIFT,
        DUR_SILENCE,
        DUR_INVIS,
        DUR_CONF,
        DUR_EXHAUSTED,
        DUR_MIGHT,
        DUR_BRILLIANCE,
        DUR_AGILITY,
        DUR_DIVINE_VIGOUR,
        DUR_DIVINE_STAMINA,
        DUR_BERSERK,
        STATUS_AIRBORNE,
        DUR_BARGAIN,
        DUR_SLAYING,
        STATUS_MANUAL,
        DUR_SAGE,
        DUR_MAGIC_SHIELD,
        DUR_FIRE_SHIELD,
        DUR_POISONING,
        STATUS_SICK,
        DUR_NAUSEA,
        STATUS_CONTAMINATION,
        STATUS_ROT,
        DUR_CONFUSING_TOUCH,
        DUR_SLIMIFY,
        DUR_SURE_BLADE,
        STATUS_NET,
        STATUS_SPEED,
        DUR_AFRAID,
        DUR_MIRROR_DAMAGE,
        DUR_SCRYING,
        DUR_TORNADO,
        DUR_HEROISM,
        DUR_FINESSE,
        DUR_LIFESAVING,
        DUR_DARKNESS,
        STATUS_FIREBALL,
        DUR_SHROUD_OF_GOLUBRIA,
        DUR_TORNADO_COOLDOWN,
        STATUS_BACKLIT,
        STATUS_UMBRA,
        STATUS_CONSTRICTED,
        STATUS_AUGMENTED,
    };

    status_info inf;
    for (unsigned i = 0; i < ARRAYSZ(statuses); ++i)
    {
        fill_status_info(statuses[i], &inf);
        if (!inf.short_text.empty())
            status.push_back(inf.short_text);
    }

    int move_cost = (player_speed() * player_movement_speed()) / 10;
    if (move_cost != 10)
    {
        std::string help = (move_cost <   8) ? "매우 빠름" :
                           (move_cost <  10) ? "빠름" :
                           (move_cost <  13) ? "느림"
                                             : "매우 느람";

        status.push_back(help);
    }

    status.push_back("적대적인 주술에 대해 " +
					 magic_res_adjective(player_res_magic(false))
                     );

    // character evaluates their ability to sneak around:
    status.push_back(stealth_desc(check_stealth()));

    text += comma_separated_line(status.begin(), status.end(), ", ", ", ");
    text += "\n";

    //----------------------------
    // print mutation information
    //----------------------------
    text += "<w>A:</w> ";

    int AC_change  = 0;
    int EV_change  = 0;
    int SH_change  = 0;
    int Str_change = 0;
    int Int_change = 0;
    int Dex_change = 0;

    std::vector<std::string> mutations;

    switch (you.species)   //mv: following code shows innate abilities - if any
    {
      case SP_MERFOLK:
          mutations.push_back(_annotate_form_based(_("change form in water"),
                                                   form_changed_physiology()));
          mutations.push_back(_annotate_form_based(_("swift swim"),
                                                   form_changed_physiology()));
          break;

      case SP_MINOTAUR:
          mutations.push_back(_annotate_form_based(_("retaliatory headbutt"),
                                                   !form_keeps_mutations()));
          break;

      case SP_NAGA:
          // breathe poison replaces spit poison:
          if (!player_mutation_level(MUT_BREATHE_POISON))
              mutations.push_back("독 뱉기");
          else
              mutations.push_back(_("breathe poison"));
          mutations.push_back(_annotate_form_based(_("constrict 1"),
                                                   !form_keeps_mutations()));
          break;

      case SP_GHOUL:
          mutations.push_back("rotting body");
          break;

      case SP_TENGU:
          if (you.experience_level > 4)
          {
              std::string help = "비행 가능";
              if (you.experience_level > 14)
                  help = make_stringf(_("%s continuously"), help.c_str());
              mutations.push_back(_annotate_form_based(help,
                                                       player_is_shapechanged()));
          }
          break;

      case SP_MUMMY:
          mutations.push_back("물약,음식 섭취 불가");
          mutations.push_back("불에 취약함");
          if (you.experience_level > 12)
          {
              std::string help = "죽음의 힘에 특화";
              if (you.experience_level > 25)
                  help = "강한 " + help;
              mutations.push_back(help);
          }
          mutations.push_back("restore body");
          break;

      case SP_KOBOLD:
          mutations.push_back("질병 저항");
          break;

      case SP_VAMPIRE:
          if (you.experience_level >= 6)
              mutations.push_back("bottle blood");
          break;

      case SP_DEEP_DWARF:
          mutations.push_back(_("damage resistance"));
          mutations.push_back(_("recharge devices"));
          break;

      case SP_FELID:
          mutations.push_back("날카로운 발톱");
          break;

      case SP_RED_DRACONIAN:
          mutations.push_back(_dragon_abil(_("breathe fire")));
          break;

      case SP_WHITE_DRACONIAN:
          mutations.push_back(_dragon_abil(_("breathe frost")));
          break;

      case SP_GREEN_DRACONIAN:
          mutations.push_back(_dragon_abil(_("breathe noxious fumes")));
          break;

      case SP_YELLOW_DRACONIAN:
          mutations.push_back(_dragon_abil(_("spit acid")));
          mutations.push_back(_annotate_form_based(_("acid resistance"),
                                                   !form_keeps_mutations()
                                                    && you.form != TRAN_DRAGON));
          break;

      case SP_GREY_DRACONIAN:
          mutations.push_back("물 위 걷기");
          break;

      case SP_BLACK_DRACONIAN:
          mutations.push_back(_dragon_abil(_("breathe lightning")));
          break;

      case SP_PURPLE_DRACONIAN:
          mutations.push_back(_dragon_abil(_("breathe power")));
          break;

      case SP_MOTTLED_DRACONIAN:
          mutations.push_back(_dragon_abil(_("breathe sticky flames")));
          break;

      case SP_PALE_DRACONIAN:
          mutations.push_back(_dragon_abil(_("breathe steam")));
          break;

      default:
          break;
    }                           //end switch - innate abilities

    // a bit more stuff
    if (you.species == SP_OGRE || you.species == SP_TROLL
        || player_genus(GENPC_DRACONIAN) || you.species == SP_SPRIGGAN)
    {
        mutations.push_back("방어구가 맞지 않음");
    }

    if (you.species == SP_FELID)
    {
        mutations.push_back("방어구 사용 불가");
        mutations.push_back("고급 소모템 사용불가");
    }

    if (you.species == SP_OCTOPODE)
    {
        mutations.push_back(_("almost no armour"));
        mutations.push_back(_("amphibious"));
        mutations.push_back(_annotate_form_based(
            make_stringf(_("constrict %d"), std::min(MAX_CONSTRICT, 8)),
            !form_keeps_mutations()));
    }

    if (beogh_water_walk())
        mutations.push_back("물 위 걷기");

    std::string current;
    for (unsigned i = 0; i < NUM_MUTATIONS; ++i)
    {
        if (!you.mutation[i])
            continue;

        int level = player_mutation_level((mutation_type) i);

        const bool lowered = (level < you.mutation[i]);
        const mutation_def& mdef = get_mutation_def((mutation_type) i);

        current = "";

        if (mdef.short_desc)
        {
            current += gettext(mdef.short_desc);

            if (mdef.levels > 1)
            {
                std::ostringstream ostr;
                ostr << ' ' << level;

                current += ostr.str();
            }
        }
        else if (level)
        {
            switch (i)
            {
            case MUT_TOUGH_SKIN:
                AC_change += level;
                break;
            case MUT_SHAGGY_FUR:
                AC_change += level;
                break;
            case MUT_STRONG:
                Str_change += level;
                break;
            case MUT_CLEVER:
                Int_change += level;
                break;
            case MUT_AGILE:
                Dex_change += level;
                break;
            case MUT_WEAK:
                Str_change -= level;
                break;
            case MUT_DOPEY:
                Int_change -= level;
                break;
            case MUT_CLUMSY:
                Dex_change -= level;
                break;
            case MUT_STRONG_STIFF:
                Str_change += level;
                Dex_change -= level;
                break;
            case MUT_FLEXIBLE_WEAK:
                Str_change -= level;
                Dex_change += level;
                break;
            case MUT_FRAIL:
                snprintf(info, INFO_SIZE, "-%d%% hp", level*10);
                current = info;
                break;
            case MUT_ROBUST:
                snprintf(info, INFO_SIZE, "+%d%% hp", level*10);
                current = info;
                break;
            case MUT_LOW_MAGIC:
                snprintf(info, INFO_SIZE, "-%d%% mp", level*10);
                current = info;
                break;
            case MUT_HIGH_MAGIC:
                snprintf(info, INFO_SIZE, "+%d%% mp", level*10);
                current = info;
                break;
            case MUT_STOCHASTIC_TORMENT_RESISTANCE:
                snprintf(info, INFO_SIZE, "%d%% 고통 저항", level*20);
                current = info;
                break;
            case MUT_ICEMAIL:
                AC_change += player_icemail_armour_class();
                break;
            case MUT_EYEBALLS:
                 snprintf(info, INFO_SIZE, "+%d 명중률", level*2+1);
                 current = info;
                 break;

            // scales -> calculate sum of AC bonus
            case MUT_DISTORTION_FIELD:
                EV_change += level + 1;
                if (level == 3)
                    current = "발사체 방어";
                break;
            case MUT_ICY_BLUE_SCALES:
                AC_change += level;
                EV_change -= level > 1 ? 1 : 0;
                break;
            case MUT_IRIDESCENT_SCALES:
                AC_change += 2*level+2;
                break;
            case MUT_LARGE_BONE_PLATES:
                AC_change += level + 1;
                SH_change += level + 1;
                break;
            case MUT_MOLTEN_SCALES:
                AC_change += level;
                EV_change -= level > 1 ? 1 : 0;
                break;
            case MUT_ROUGH_BLACK_SCALES:
                AC_change  += 3*level+1;
                Dex_change -= level;
                break;
            case MUT_RUGGED_BROWN_SCALES:
                AC_change += 2;
                break;
            case MUT_SLIMY_GREEN_SCALES:
                AC_change += level;
                break;
            case MUT_THIN_METALLIC_SCALES:
                AC_change += level;
                break;
            case MUT_THIN_SKELETAL_STRUCTURE:
                Dex_change += 2 * level;
                Str_change -= level;
                break;
            case MUT_YELLOW_SCALES:
                AC_change += level;
                break;
            case MUT_GELATINOUS_BODY:
                AC_change += (level == 3) ? 2 : 1;
                EV_change += level - 1;
                break;
            default:
                die("mutation without a short desc: %d", i);
            }
        }

        if (!current.empty())
        {
            if (level == 0)
                current = "(" + current + ")";
            if (lowered)
                current = "<darkgrey>" + current + "</darkgrey>";
            mutations.push_back(current);
        }
    }

    // Statue form does not get AC benefits from scales etc.  It does
    // get changes to EV and SH.
    if (AC_change && you.form != TRAN_STATUE)
    {
        snprintf(info, INFO_SIZE, "AC %s%d", (AC_change > 0 ? "+" : ""), AC_change);
        mutations.push_back(info);
    }
    if (EV_change)
    {
        snprintf(info, INFO_SIZE, "EV %s%d", (EV_change > 0 ? "+" : ""), EV_change);
        mutations.push_back(info);
    }
    if (SH_change)
    {
        snprintf(info, INFO_SIZE, "SH %s%d", (SH_change > 0 ? "+" : ""), SH_change);
        mutations.push_back(info);
    }
    if (Str_change)
    {
        snprintf(info, INFO_SIZE, "힘 %s%d", (Str_change > 0 ? "+" : ""), Str_change);
        mutations.push_back(info);
    }
    if (Int_change)
    {
        snprintf(info, INFO_SIZE, "지능 %s%d", (Int_change > 0 ? "+" : ""), Int_change);
        mutations.push_back(info);
    }
    if (Dex_change)
    {
        snprintf(info, INFO_SIZE, "민첩 %s%d", (Dex_change > 0 ? "+" : ""), Dex_change);
        mutations.push_back(info);
    }

    if (mutations.empty())
        text +=  "변이 없음";
    else
    {
        text += comma_separated_line(mutations.begin(), mutations.end(),
                                     ", ", ", ");
    }

    //----------------------------
    // print ability information
    //----------------------------

    text += print_abilities();

    //--------------
    // print runes
    //--------------
    std::vector<std::string> runes;
    for (int i = 0; i < NUM_RUNE_TYPES; i++)
        if (you.runes[i])
            runes.push_back(gettext(rune_type_name(i)));
    if (!runes.empty())
    {
        text += make_stringf("\n<w>%s:</w> %d/%d 룬%s: %s",
                    stringize_glyph(get_item_symbol(SHOW_ITEM_MISCELLANY)).c_str(),
                    (int)runes.size(), you.obtainable_runes,
                    you.obtainable_runes == 1 ? "" : "s",
                    comma_separated_line(runes.begin(), runes.end(),
                                         ", ", ", ").c_str());
    }

    linebreak_string(text, sw);

    return text;
}
