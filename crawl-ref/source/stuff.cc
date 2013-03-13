/**
 * @file
 * @brief Misc stuff.
**/

#include "AppHdr.h"
#include "stuff.h"

#include <cstdarg>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

#include "abyss.h"
#include "cio.h"
#include "colour.h"
#include "crash.h"
#include "database.h"
#include "delay.h"
#include "dungeon.h"
#include "files.h"
#include "hints.h"
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "message.h"
#include "notes.h"
#include "options.h"
#include "output.h"
#include "player.h"
#include "state.h"
#include "traps.h"
#include "view.h"
#include "viewchar.h"
#include "viewgeom.h"

#ifdef TOUCH_UI
#include "tilepick.h"
#include "tiledef-gui.h"
#endif

#ifdef __ANDROID__
#include <android/log.h>
double log2(double n)
{
    return log(n) / log(2); // :(
}
#endif

// Crude, but functional.
string make_time_string(time_t abs_time, bool terse)
{
    const int days  = abs_time / 86400;
    const int hours = (abs_time % 86400) / 3600;
    const int mins  = (abs_time % 3600) / 60;
    const int secs  = abs_time % 60;

    string buff;
    if (days > 0)
    {
        buff += make_stringf("%d%s ", days, terse ? ","
                             : days > 1 ? "days" : "day");
    }
    return buff + make_stringf("%02d:%02d:%02d", hours, mins, secs);
}

string make_file_time(time_t when)
{
    if (tm *loc = TIME_FN(&when))
    {
        return make_stringf("%04d%02d%02d-%02d%02d%02d",
                 loc->tm_year + 1900,
                 loc->tm_mon + 1,
                 loc->tm_mday,
                 loc->tm_hour,
                 loc->tm_min,
                 loc->tm_sec);
    }
    return "";
}

void set_redraw_status(uint64_t flags)
{
    you.redraw_status_flags |= flags;
}

int get_ch()
{
    mouse_control mc(MOUSE_MODE_PROMPT);
    int gotched = getchm();

    if (gotched == 0)
        gotched = getchm();

    return gotched;
}

void cio_init()
{
    crawl_state.io_inited = true;
    console_startup();
    set_cursor_enabled(false);
    crawl_view.init_geometry();
}

void cio_cleanup()
{
    if (!crawl_state.io_inited)
        return;

    console_shutdown();
    crawl_state.io_inited = false;
}

// Clear some globally defined variables.
static void _clear_globals_on_exit()
{
    clear_rays_on_exit();
    clear_zap_info_on_exit();
    clear_colours_on_exit();
    dgn_clear_vault_placements(env.level_vaults);
    destroy_abyss();
}

#if (defined(TARGET_OS_WINDOWS) && !defined(USE_TILE_LOCAL)) \
     || defined(DGL_PAUSE_AFTER_ERROR)
// Print error message on the screen.
// Ugly, but better than not showing anything at all. (jpeg)
static bool _print_error_screen(const char *message, ...)
{
    if (!crawl_state.io_inited || crawl_state.seen_hups)
        return false;

    // Get complete error message.
    string error_msg;
    {
        va_list arg;
        va_start(arg, message);
        char buffer[1024];
        vsnprintf(buffer, sizeof buffer, message, arg);
        va_end(arg);

        error_msg = string(buffer);
    }
    if (error_msg.empty())
        return false;

    // Escape '<'.
    // NOTE: This assumes that the error message doesn't contain
    //       any formatting!
    error_msg = replace_all(error_msg, "<", "<<");

    error_msg += "\n\n\nHit any key to exit...\n";

    // Break message into correctly sized lines.
    int width = 80;
#ifdef USE_TILE_LOCAL
    width = crawl_view.msgsz.x;
#else
    width = min(80, get_number_of_cols());
#endif
    linebreak_string(error_msg, width);

    // And finally output the message.
    clrscr();
    formatted_string::parse_string(error_msg, false).display();
    getchm();
    return true;
}
#endif

// Used by do_crash_dump() to tell if the crash happened during exit() hooks.
// Not a part of crawl_state, since that's a global C++ instance which is
// free'd by exit() hooks when exit() is called, and we don't want to reference
// free'd memory.
bool CrawlIsExiting = false;
bool CrawlIsCrashing = false;

NORETURN void end(int exit_code, bool print_error, const char *format, ...)
{
    disable_other_crashes();

    string error = print_error? strerror(errno) : "";
    if (format)
    {
        va_list arg;
        va_start(arg, format);
        char buffer[1024];
        vsnprintf(buffer, sizeof buffer, format, arg);
        va_end(arg);

        if (error.empty())
            error = string(buffer);
        else
            error = string(buffer) + ": " + error;

        if (!error.empty() && error[error.length() - 1] != '\n')
            error += "\n";
    }

#if (defined(TARGET_OS_WINDOWS) && !defined(USE_TILE_LOCAL)) \
     || defined(DGL_PAUSE_AFTER_ERROR)
    bool need_pause = true;
    if (exit_code && !error.empty())
    {
        if (_print_error_screen("%s", error.c_str()))
            need_pause = false;
    }
#endif

#ifdef USE_TILE_WEB
    tiles.shutdown();
#endif

    cio_cleanup();
    msg::deinitialise_mpr_streams();
    _clear_globals_on_exit();
    databaseSystemShutdown();
#ifdef DEBUG_PROPS
    dump_prop_accesses();
#endif

    if (!error.empty())
    {

#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "Crawl", "%s", error.c_str());
#endif
        fprintf(stderr, "%s", error.c_str());
        error.clear();
    }

#if (defined(TARGET_OS_WINDOWS) && !defined(USE_TILE_LOCAL)) \
     || defined(DGL_PAUSE_AFTER_ERROR)
    if (need_pause && exit_code && !crawl_state.game_is_arena()
        && !crawl_state.seen_hups && !crawl_state.test)
    {
        fprintf(stderr, "Hit Enter to continue...\n");
        getchar();
    }
#endif

    CrawlIsExiting = true;
    if (exit_code)
        CrawlIsCrashing = true;

#ifdef DEBUG_GLOBALS
    delete real_env;         real_env = 0;
    delete real_crawl_state; real_crawl_state = 0;
    delete real_dlua;        real_dlua = 0;
    delete real_clua;        real_clua = 0;
    delete real_you;         real_you = 0;
    delete real_Options;     real_Options = 0;
#endif

    exit(exit_code);
}

NORETURN void game_ended()
{
    if (!crawl_state.seen_hups)
        throw game_ended_condition();
    else
        end(0);
}

NORETURN void game_ended_with_error(const string &message)
{
    if (crawl_state.seen_hups)
        end(1);

    if (Options.restart_after_game)
    {
        if (crawl_state.io_inited)
        {
            mprf(MSGCH_ERROR, "%s", message.c_str());
            more();
        }
        else
        {
            fprintf(stderr, "%s\nHit Enter to continue...\n", message.c_str());
            getchar();
        }
        game_ended();
    }
    else
        end(1, false, "%s", message.c_str());
}

void redraw_screen(void)
{
    if (!crawl_state.need_save)
    {
        // If the game hasn't started, don't do much.
        clrscr();
        return;
    }

#ifdef USE_TILE_WEB
    tiles.close_all_menus();
#endif

    draw_border();

    you.redraw_title        = true;
    you.redraw_hit_points   = true;
    you.redraw_magic_points = true;
    you.redraw_stats.init(true);
    you.redraw_armour_class = true;
    you.redraw_evasion      = true;
    you.redraw_experience   = true;
    you.wield_change        = true;
    you.redraw_quiver       = true;

    set_redraw_status(
        REDRAW_LINE_1_MASK | REDRAW_LINE_2_MASK | REDRAW_LINE_3_MASK);

    print_stats();

    bool note_status = notes_are_active();
    activate_notes(false);
    print_stats_level();
#ifdef DGL_SIMPLE_MESSAGING
    update_message_status();
#endif
    update_turn_count();
    activate_notes(note_status);

    viewwindow();

    // Display the message window at the end because it places
    // the cursor behind possible prompts.
    display_message_window();
    update_screen();
}

double stepdown(double value, double step)
{
    return step * log2(1 + value / step);
}

int stepdown(int value, int step, rounding_type rounding, int max)
{
    double ret = stepdown((double) value, double(step));

    if (max > 0 && ret > max)
        return max;

    // Randomised rounding
    if (rounding == ROUND_RANDOM)
    {
        double intpart;
        double fracpart = modf(ret, &intpart);
        if (random_real() < fracpart)
            ++intpart;
        return intpart;
    }

    return ret + (rounding == ROUND_CLOSE ? 0.5 : 0);
}

// Deprecated definition. Call directly stepdown instead.
int stepdown_value(int base_value, int stepping, int first_step,
                   int last_step, int ceiling_value)
{
    UNUSED(last_step);

    // Disabling max used to be -1.
    if (ceiling_value < 0)
        ceiling_value = 0;

    if (ceiling_value && ceiling_value < first_step)
        return min(base_value, ceiling_value);
    if (base_value < first_step)
        return base_value;

    const int diff = first_step - stepping;
    // Since diff < first_step, we can assume here that ceiling_value > diff
    // or ceiling_value == 0.
    return diff + stepdown(base_value - diff, stepping, ROUND_DOWN,
                           ceiling_value ? ceiling_value - diff : 0);
}

int div_round_up(int num, int den)
{
    return (num / den + (num % den != 0));
}

void canned_msg(canned_message_type which_message)
{
    switch (which_message)
    {
    case MSG_SOMETHING_APPEARS:
        mpr(player_has_feet() ? gettext("Something appears at your feet!"):
            gettext("Something appears before you!"));
        break;
    case MSG_NOTHING_HAPPENS:
        mpr(gettext("Nothing appears to happen."));
        break;
    case MSG_YOU_UNAFFECTED:
        mpr(gettext("You are unaffected."));
        break;
    case MSG_YOU_RESIST:
        mpr(gettext("You resist."));
        learned_something_new(HINT_YOU_RESIST);
        break;
    case MSG_YOU_PARTIALLY_RESIST:
        mpr(gettext("You partially resist."));
        break;
    case MSG_TOO_BERSERK:
        mpr(gettext("You are too berserk!"));
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_PRESENT_FORM:
        mpr(gettext("You can't do that in your present form."));
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_NOTHING_CARRIED:
        mpr(gettext("You aren't carrying anything."));
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_CANNOT_DO_YET:
        mpr(gettext("You can't do that yet."));
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_OK:
        mpr(gettext("Okay, then."), MSGCH_PROMPT);
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_UNTHINKING_ACT:
        mpr(gettext("Why would you want to do that?"));
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_NOTHING_THERE:
        mpr(gettext("There's nothing there!"));
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_NOTHING_CLOSE_ENOUGH:
        mpr(gettext("There's nothing close enough!"));
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_NO_ENERGY:
        mpr(gettext("You don't have the energy to cast that spell."));
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_SPELL_FIZZLES:
        mpr(gettext("The spell fizzles."));
        break;
    case MSG_HUH:
        mpr(gettext("Huh?"), MSGCH_EXAMINE_FILTER);
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_EMPTY_HANDED_ALREADY:
    case MSG_EMPTY_HANDED_NOW:
    {
        const char* when =
            (which_message == MSG_EMPTY_HANDED_ALREADY ? "already" : "now");
        if (you.species == SP_FELID)
            mprf(gettext("Your mouth is %s empty."), when);
        else if (you.has_usable_claws(true))
            mprf(gettext("You are %s empty-clawed."), when);
        else if (you.has_usable_tentacles(true))
            mprf(gettext("You are %s empty-tentacled."), when);
        else
            mprf(gettext("You are %s empty-handed."), when);
        break;
    }
    case MSG_YOU_BLINK:
        mpr(gettext("You blink."));
        break;
    case MSG_STRANGE_STASIS:
        mpr(gettext("You feel a strange sense of stasis."));
        break;
    case MSG_NO_SPELLS:
        mpr(gettext("You don't know any spells."));
        break;
    case MSG_MANA_INCREASE:
        mpr(gettext("You feel your mana capacity increase."));
        break;
    case MSG_MANA_DECREASE:
        mpr(gettext("You feel your mana capacity decrease."));
        break;
    case MSG_DISORIENTED:
        mpr(gettext("You feel momentarily disoriented."));
        break;
    case MSG_TOO_HUNGRY:
        mpr(gettext("You're too hungry."));
        break;
    case MSG_DETECT_NOTHING:
        mpr(gettext("You detect nothing."));
        break;
    case MSG_CALL_DEAD:
        mpr(gettext("You call on the dead to rise..."));
        break;
    case MSG_ANIMATE_REMAINS:
        mpr(gettext("You attempt to give life to the dead..."));
        break;
    case MSG_DECK_EXHAUSTED:
        mpr(gettext("The deck of cards disappears in a puff of smoke."));
        break;
    case MSG_EVOCATION_SUPPRESSED:
        mpr(gettext("You may not evoke while suppressed!"));
        break;
    case MSG_BEING_WATCHED:
        mpr(gettext("You feel you are being watched by something."));
        break;
    }
}

const char* held_status(actor *act)
{
    if (get_trapping_net(act->pos(), true) != NON_ITEM)
        return "held in a net";
    else
        return "caught in a web";
}

// Like yesno, but requires a full typed answer.
// Unlike yesno, prompt should have no trailing space.
// Returns true if the user typed "yes", false if something else or cancel.
bool yes_or_no(const char* fmt, ...)
{
    char buf[200];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    buf[sizeof(buf)-1] = 0;

    mprf(MSGCH_PROMPT, "%s? (Confirm with \"yes\".) ", buf);

    if (cancelable_get_line(buf, sizeof buf))
        return false;
    if (strcasecmp(buf, "yes") != 0)
        return false;

    return true;
}

// jmf: general helper (should be used all over in code)
//      -- idea borrowed from Nethack
bool yesno(const char *str, bool safe, int safeanswer, bool clear_after,
           bool interrupt_delays, bool noprompt,
           const explicit_keymap *map, GotoRegion region)
{
    bool message = (region == GOTO_MSG);
    if (interrupt_delays && !crawl_state.is_repeating_cmd())
        interrupt_activity(AI_FORCE_INTERRUPT);

    string prompt = make_stringf("%s ", str ? str : "Buggy prompt?");

#ifdef TOUCH_UI
    Popup *pop = new Popup(prompt);
    MenuEntry *status = new MenuEntry("", MEL_SUBTITLE);
    pop->push_entry(new MenuEntry(prompt, MEL_TITLE));
    pop->push_entry(status);
    MenuEntry *me = new MenuEntry("Yes", MEL_ITEM, 0, 'Y', false);
    me->add_tile(tile_def(TILEG_PROMPT_YES, TEX_GUI));
    pop->push_entry(me);
    me = new MenuEntry("No", MEL_ITEM, 0, 'N', false);
    me->add_tile(tile_def(TILEG_PROMPT_NO, TEX_GUI));
    pop->push_entry(me);
#endif
    mouse_control mc(MOUSE_MODE_YESNO);
    while (true)
    {
#ifdef TOUCH_UI
        int tmp = pop->pop();
#else
        if (!noprompt)
        {
            if (message)
                mpr(prompt.c_str(), MSGCH_PROMPT);
            else
                cprintf("%s", prompt.c_str());
        }

        int tmp = getchm(KMC_CONFIRM);
#endif

        // Prevent infinite loop if Curses HUP signal handling happens;
        // if there is no safe answer, then just save-and-exit immediately,
        // since there's no way to know if it would be better to return
        // true or false.
        if (crawl_state.seen_hups && !safeanswer)
            sighup_save_and_exit();

        if (map && map->find(tmp) != map->end())
            tmp = map->find(tmp)->second;

        if (safeanswer
            && (tmp == ' ' || key_is_escape(tmp)
                || tmp == '\r' || tmp == '\n' || crawl_state.seen_hups))
        {
            tmp = safeanswer;
        }

        if (Options.easy_confirm == CONFIRM_ALL_EASY
            || tmp == safeanswer
            || Options.easy_confirm == CONFIRM_SAFE_EASY && safe)
        {
            tmp = toupper(tmp);
        }

        if (clear_after && message)
            mesclr();

        if (tmp == 'N')
            return false;
        else if (tmp == 'Y')
            return true;
        else if (!noprompt)
        {
            bool upper = (!safe && crawl_state.game_is_hints_tutorial());
            const string pr = make_stringf("%s[Y]es or [N]o only, please.",
                                           upper ? "Uppercase " : "");
#ifdef TOUCH_UI
            status->text = pr;
#else
            if (message)
                mpr(pr);
            else
                cprintf(("\n" + pr + "\n").c_str());
#endif
        }
    }
}

static string _list_alternative_yes(char yes1, char yes2, bool lowered = false,
                                    bool brackets = false)
{
    string help = "";
    bool print_yes = false;
    if (yes1 != 'Y')
    {
        if (lowered)
            help += toalower(yes1);
        else
            help += yes1;
        print_yes = true;
    }

    if (yes2 != 'Y' && yes2 != yes1)
    {
        if (print_yes)
            help += "/";

        if (lowered)
            help += toalower(yes2);
        else
            help += yes2;
        print_yes = true;
    }

    if (print_yes)
    {
        if (brackets)
            help = " (" + help + ")";
        else
            help = "/" + help;
    }

    return help;
}

static string _list_allowed_keys(char yes1, char yes2, bool lowered = false,
                                 bool allow_all = false)
{
    string result = " [";
    result += (lowered ? "(y)es" : "(Y)es");
    result += _list_alternative_yes(yes1, yes2, lowered);
    if (allow_all)
        result += (lowered? "/(a)ll" : "/(A)ll");
    result += (lowered ? "/(n)o/(q)uit" : "/(N)o/(Q)uit");
    result += "]";

    return result;
}

// Like yesno(), but returns 0 for no, 1 for yes, and -1 for quit.
// alt_yes and alt_yes2 allow up to two synonyms for 'Y'.
// FIXME: This function is shaping up to be a monster. Help!
int yesnoquit(const char* str, bool safe, int safeanswer, bool allow_all,
              bool clear_after, char alt_yes, char alt_yes2)
{
    if (!crawl_state.is_repeating_cmd())
        interrupt_activity(AI_FORCE_INTERRUPT);

    mouse_control mc(MOUSE_MODE_YESNO);

    string prompt =
        make_stringf("%s%s ", str ? str : "Buggy prompt?",
                     _list_allowed_keys(alt_yes, alt_yes2,
                                        safe, allow_all).c_str());
    while (true)
    {
        mpr(prompt.c_str(), MSGCH_PROMPT);

        int tmp = getchm(KMC_CONFIRM);

        if (key_is_escape(tmp) || tmp == 'q' || tmp == 'Q'
            || crawl_state.seen_hups)
        {
            return -1;
        }

        if ((tmp == ' ' || tmp == '\r' || tmp == '\n') && safeanswer)
            tmp = safeanswer;

        if (Options.easy_confirm == CONFIRM_ALL_EASY
            || tmp == safeanswer
            || safe && Options.easy_confirm == CONFIRM_SAFE_EASY)
        {
            tmp = toupper(tmp);
        }

        if (clear_after)
            mesclr();

        if (tmp == 'N')
            return 0;
        else if (tmp == 'Y' || tmp == alt_yes || tmp == alt_yes2)
            return 1;
        else if (allow_all)
        {
            if (tmp == 'A')
                return 2;
            else
            {
                bool upper = (!safe && crawl_state.game_is_hints_tutorial());
                mprf("Choose %s[Y]es%s, [N]o, [Q]uit, or [A]ll!",
                     upper ? "uppercase " : "",
                     _list_alternative_yes(alt_yes, alt_yes2, false, true).c_str());
            }
        }
        else
        {
            bool upper = (!safe && crawl_state.game_is_hints_tutorial());
            mprf("%s[Y]es%s, [N]o or [Q]uit only, please.",
                 upper ? "Uppercase " : "",
                 _list_alternative_yes(alt_yes, alt_yes2, false, true).c_str());
        }
    }
}

char index_to_letter(int the_index)
{
    ASSERT(the_index >= 0 && the_index < ENDOFPACK);
    return (the_index + ((the_index < 26) ? 'a' : ('A' - 26)));
}

int letter_to_index(int the_letter)
{
    if (the_letter >= 'a' && the_letter <= 'z')
        return (the_letter - 'a'); // returns range [0-25] {dlb}
    else if (the_letter >= 'A' && the_letter <= 'Z')
        return (the_letter - 'A' + 26); // returns range [26-51] {dlb}

    die("slot not a letter: %s (%d)", the_letter ?
        stringize_glyph(the_letter).c_str() : "null", the_letter);
}

maybe_bool frombool(bool b)
{
    return (b ? B_TRUE : B_FALSE);
}

bool tobool(maybe_bool mb)
{
    ASSERT(mb != B_MAYBE);
    return (mb == B_TRUE);
}

bool tobool(maybe_bool mb, bool def)
{
    switch (mb)
    {
    case B_TRUE:
        return true;
    case B_FALSE:
        return false;
    case B_MAYBE:
    default:
        return def;
    }
}

//---------------------------------------------------------------
//
// prompt_for_quantity
//
// Returns -1 if ; or enter is pressed (pickup all).
// Else, returns quantity.
//---------------------------------------------------------------
int prompt_for_quantity(const char *prompt)
{
    msgwin_prompt(prompt);

    int ch = getch_ck();
    if (ch == CK_ENTER || ch == ';')
        return -1;
    else if (ch == CK_ESCAPE)
        return 0;

    macro_buf_add(ch);
    return prompt_for_int("", false);
}

//---------------------------------------------------------------
//
// prompt_for_int
//
// If nonneg, then it returns a non-negative number or -1 on fail
// If !nonneg, then it returns an integer, and 0 on fail
//
//---------------------------------------------------------------
int prompt_for_int(const char *prompt, bool nonneg)
{
    char specs[80];

    msgwin_get_line(prompt, specs, sizeof(specs));

    if (specs[0] == '\0')
        return (nonneg ? -1 : 0);

    char *end;
    int   ret = strtol(specs, &end, 10);

    if (ret < 0 && nonneg || ret == 0 && end == specs)
        ret = (nonneg ? -1 : 0);

    return ret;
}

double prompt_for_float(const char* prompt)
{
    char specs[80];

    msgwin_get_line(prompt, specs, sizeof(specs));

    if (specs[0] == '\0')
        return -1;

    char *end;
    double ret = strtod(specs, &end);

    if (ret == 0 && end == specs)
        ret = -1;

    return ret;

}
