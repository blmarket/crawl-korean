#include "AppHdr.h"

#include "player-stats.h"

#include "artefact.h"
#include "delay.h"
#include "godpassive.h"
#include "files.h"
#include "itemname.h"
#include "item_use.h"
#include "korean.h"
#include "libutil.h"
#include "macro.h"
#include "misc.h"
#include "mon-util.h"
#include "monster.h"
#include "ouch.h"
#include "player.h"
#include "religion.h"
#include "state.h"
#include "transform.h"
#include "hints.h"

int player::stat(stat_type s, bool nonneg) const
{
    const int val = max_stat(s) - stat_loss[s];
    return (nonneg ? std::max(val, 0) : val);
}

int player::strength() const
{
    return (stat(STAT_STR));
}

int player::intel() const
{
    return (stat(STAT_INT));
}

int player::dex() const
{
    return (stat(STAT_DEX));
}

static int _stat_modifier(stat_type stat);

int player::max_stat(stat_type s) const
{
    return (std::min(base_stats[s] + _stat_modifier(s), 72));
}

int player::max_strength() const
{
    return (max_stat(STAT_STR));
}

int player::max_intel() const
{
    return (max_stat(STAT_INT));
}

int player::max_dex() const
{
    return (max_stat(STAT_DEX));
}

static void _handle_stat_change(stat_type stat, const char *aux = NULL,
                                bool see_source = true);
static void _handle_stat_change(const char *aux = NULL, bool see_source = true);

void attribute_increase()
{
    crawl_state.stat_gain_prompt = true;
    mpr(gettext("Your experience leads to an increase in your attributes!"),
        MSGCH_INTRINSIC_GAIN);
    learned_something_new(HINT_CHOOSE_STAT);
    mpr(gettext("Increase (S)trength, (I)ntelligence, or (D)exterity? "), MSGCH_PROMPT);
    mouse_control mc(MOUSE_MODE_MORE);
    // Calling a user-defined lua function here to let players reply to the
    // prompt automatically.
    clua.callfn("choose_stat_gain", 0);

    while (true)
    {
        const int keyin = getchm();

        switch (keyin)
        {
        CASE_ESCAPE
            // [ds] It's safe to save the game here; when the player
            // reloads, the game will re-prompt for their level-up
            // stat gain.
            if (crawl_state.seen_hups)
                sighup_save_and_exit();
            break;

        case 's':
        case 'S':
            modify_stat(STAT_STR, 1, false, "level gain");
            return;

        case 'i':
        case 'I':
            modify_stat(STAT_INT, 1, false, "level gain");
            return;

        case 'd':
        case 'D':
            modify_stat(STAT_DEX, 1, false, "level gain");
            return;
        }
    }
}

// Rearrange stats, biased based on your armour and skills.
void jiyva_stat_action()
{
    int cur_stat[3];
    int stat_total = 0;
    int target_stat[3];
    for (int x = 0; x < 3; ++x)
    {
        cur_stat[x] = you.stat(static_cast<stat_type>(x), false);
        stat_total += cur_stat[x];
    }
    // Try to avoid burdening people or making their armour difficult to use.
    int current_capacity = carrying_capacity(BS_UNENCUMBERED);
    int carrying_strength = cur_stat[0] + (you.burden - current_capacity + 249)/250;
    int evp = you.unadjusted_body_armour_penalty();
    target_stat[0] = std::max(std::max(9, 2 + 3 * evp), 2 + carrying_strength);
    target_stat[1] = 9;
    target_stat[2] = 9;
    int remaining = stat_total - 18 - target_stat[0];
    // Divide up the remaining stat points between Int and either Str or Dex,
    // based on skills.
    if (remaining > 0)
    {
        int magic_weights = 0;
        int other_weights = 0;
        for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; i++)
        {
            skill_type sk = static_cast<skill_type>(i);

            int weight = you.skills[sk];

            // Anyone can get Spellcasting 1. Doesn't prove anything.
            if (sk == SK_SPELLCASTING && weight >= 1)
                weight--;

            if (sk >= SK_SPELLCASTING && sk < SK_INVOCATIONS)
                magic_weights += weight;
            else
                other_weights += weight;
        }
        // If you are in really heavy armour, then you already are getting a
        // lot of Str and more won't help much, so weight magic more.
        other_weights = std::max(other_weights - (evp >= 5 ? 4 : 1) * magic_weights/2, 0);
        magic_weights = div_rand_round(remaining * magic_weights, magic_weights + other_weights);
        other_weights = remaining - magic_weights;
        target_stat[1] += magic_weights;
        // Choose Str or Dex based on how heavy your armour is.
        target_stat[(evp >= 5) ? 0 : 2] += other_weights;
    }
    // Add a little fuzz to the target.
    for (int x = 0; x < 3; ++x)
        target_stat[x] += random2(5) - 2;
    int choices = 0;
    int stat_up_choice = 0;
    int stat_down_choice = 0;
    // Choose a random stat shuffle that doesn't increase the l^2 distance to
    // the (fuzzed) target.
    for (int x = 0; x < 3; ++x)
        for (int y = 0; y < 3; ++y)
        {
            if (x != y && target_stat[x] - cur_stat[x] + cur_stat[y] - target_stat[y] > 0)
            {
                choices++;
                if (one_chance_in(choices))
                {
                    stat_up_choice = x;
                    stat_down_choice = y;
                }
            }
        }
    if (choices)
    {
        simple_god_message(gettext("'s power touches on your attributes."));
        const std::string cause = "the 'helpfulness' of "
                                  + god_name(you.religion);
        modify_stat(static_cast<stat_type>(stat_up_choice), 1, true, cause.c_str());
        modify_stat(static_cast<stat_type>(stat_down_choice), -1, true, cause.c_str());
    }
}

static kill_method_type _statloss_killtype(stat_type stat)
{
    switch (stat)
    {
    case STAT_STR:
        return KILLED_BY_WEAKNESS;
    case STAT_INT:
        return KILLED_BY_STUPIDITY;
    case STAT_DEX:
        return KILLED_BY_CLUMSINESS;
    default:
        die("unknown stat");
    }
}

static const char* descs[NUM_STATS][NUM_STAT_DESCS] = {
    { M_("strength"), P_("stat", "weakened"), P_("stat", "weaker"), P_("stat", "stronger") },
    { M_("intelligence"), P_("stat", "dopey"), P_("stat", "stupid"), P_("stat", "clever") },
    { M_("dexterity"), P_("stat", "clumsy"), P_("stat", "clumsy"), P_("stat", "agile") }
};

const char* stat_desc(stat_type stat, stat_desc_type desc)
{
    return (descs[stat][desc]);
}

void modify_stat(stat_type which_stat, int amount, bool suppress_msg,
                 const char *cause, bool see_source)
{
    ASSERT(!crawl_state.game_is_arena());

    // sanity - is non-zero amount?
    if (amount == 0)
        return;

    // Stop delays if a stat drops.
    if (amount < 0)
        interrupt_activity(AI_STAT_CHANGE);

    if (which_stat == STAT_RANDOM)
        which_stat = static_cast<stat_type>(random2(NUM_STATS));

    if (!suppress_msg)
    {
        mprf((amount > 0) ? MSGCH_INTRINSIC_GAIN : MSGCH_WARN,
             pgettext("stat", "You feel %s."),
             pgettext_expr("stat", stat_desc(which_stat, (amount > 0) ? SD_INCREASE : SD_DECREASE)));
    }

    you.base_stats[which_stat] += amount;

    _handle_stat_change(which_stat, cause, see_source);
}

void notify_stat_change(stat_type which_stat, int amount, bool suppress_msg,
                        const char *cause, bool see_source)
{
    ASSERT(!crawl_state.game_is_arena());

    // sanity - is non-zero amount?
    if (amount == 0)
        return;

    // Stop delays if a stat drops.
    if (amount < 0)
        interrupt_activity(AI_STAT_CHANGE);

    if (which_stat == STAT_RANDOM)
        which_stat = static_cast<stat_type>(random2(NUM_STATS));

    if (!suppress_msg)
    {
        mprf((amount > 0) ? MSGCH_INTRINSIC_GAIN : MSGCH_WARN,
             pgettext("stat", "You feel %s."),
             pgettext_expr("stat", stat_desc(which_stat, (amount > 0) ? SD_INCREASE : SD_DECREASE)));
    }

    _handle_stat_change(which_stat, cause, see_source);
}

void notify_stat_change(stat_type which_stat, int amount, bool suppress_msg,
                        const item_def &cause, bool removed)
{
    std::string name = cause.name(true, DESC_THE, false, true, false, false,
                                  ISFLAG_KNOW_CURSE | ISFLAG_KNOW_PLUSES);
    std::string verb;

    switch (cause.base_type)
    {
    case OBJ_ARMOUR:
    case OBJ_JEWELLERY:
        if (removed)
            verb = "removing";
        else
            verb = "wearing";
        break;

    case OBJ_WEAPONS:
    case OBJ_STAVES:
        if (removed)
            verb = "unwielding";
        else
            verb = "wielding";
        break;

    case OBJ_WANDS:   verb = "zapping";  break;
    case OBJ_FOOD:    verb = "eating";   break;
    case OBJ_SCROLLS: verb = "reading";  break;
    case OBJ_POTIONS: verb = "drinking"; break;
    default:          verb = "using";
    }

    notify_stat_change(which_stat, amount, suppress_msg,
                       (verb + " " + name).c_str(), true);
}

void notify_stat_change(const char* cause)
{
    _handle_stat_change(cause);
}

static int _strength_modifier()
{
    int result = 0;

    if (you.duration[DUR_MIGHT] || you.duration[DUR_BERSERK])
        result += 5;

    if (you.duration[DUR_DIVINE_STAMINA])
        result += you.attribute[ATTR_DIVINE_STAMINA];

    result += che_stat_boost();

    // ego items of strength
    result += 3 * count_worn_ego(SPARM_STRENGTH);

    // rings of strength
    result += player_equip(EQ_RINGS_PLUS, RING_STRENGTH);

    // randarts of strength
    result += scan_artefacts(ARTP_STRENGTH);

    // mutations
    result += player_mutation_level(MUT_STRONG)
              - player_mutation_level(MUT_WEAK);
    result += player_mutation_level(MUT_STRONG_STIFF)
              - player_mutation_level(MUT_FLEXIBLE_WEAK);
    result -= player_mutation_level(MUT_THIN_SKELETAL_STRUCTURE);

    // transformations
    switch (you.form)
    {
    case TRAN_STATUE:          result +=  2; break;
    case TRAN_DRAGON:          result += 10; break;
    case TRAN_LICH:            result +=  3; break;
    case TRAN_BAT:             result -=  5; break;
    default:                                 break;
    }

    return (result);
}

static int _int_modifier()
{
    int result = 0;

    if (you.duration[DUR_BRILLIANCE])
        result += 5;

    if (you.duration[DUR_DIVINE_STAMINA])
        result += you.attribute[ATTR_DIVINE_STAMINA];

    result += che_stat_boost();

    // ego items of intelligence
    result += 3 * count_worn_ego(SPARM_INTELLIGENCE);

    // rings of intelligence
    result += player_equip(EQ_RINGS_PLUS, RING_INTELLIGENCE);

    // randarts of intelligence
    result += scan_artefacts(ARTP_INTELLIGENCE);

    // mutations
    result += player_mutation_level(MUT_CLEVER)
              - player_mutation_level(MUT_DOPEY);

    return (result);
}

static int _dex_modifier()
{
    int result = 0;

    if (you.duration[DUR_AGILITY])
        result += 5;

    if (you.duration[DUR_DIVINE_STAMINA])
        result += you.attribute[ATTR_DIVINE_STAMINA];

    result += che_stat_boost();

    // ego items of dexterity
    result += 3 * count_worn_ego(SPARM_DEXTERITY);

    // rings of dexterity
    result += player_equip(EQ_RINGS_PLUS, RING_DEXTERITY);

    // randarts of dexterity
    result += scan_artefacts(ARTP_DEXTERITY);

    // mutations
    result += player_mutation_level(MUT_AGILE)
              - player_mutation_level(MUT_CLUMSY);
    result += player_mutation_level(MUT_FLEXIBLE_WEAK)
              - player_mutation_level(MUT_STRONG_STIFF);

    result += 2 * player_mutation_level(MUT_THIN_SKELETAL_STRUCTURE);
    result -= player_mutation_level(MUT_ROUGH_BLACK_SCALES);

    // transformations
    switch (you.form)
    {
    case TRAN_SPIDER: result +=  5; break;
    case TRAN_STATUE: result -=  2; break;
    case TRAN_BAT:    result +=  5; break;
    default:                        break;
    }

    return (result);
}

static int _stat_modifier(stat_type stat)
{
    switch (stat)
    {
    case STAT_STR: return _strength_modifier();
    case STAT_INT: return _int_modifier();
    case STAT_DEX: return _dex_modifier();
    default:
        mprf(MSGCH_ERROR, "Bad stat: %d", stat);
        return 0;
    }
}

bool lose_stat(stat_type which_stat, int stat_loss, bool force,
               const char *cause, bool see_source)
{
    if (which_stat == STAT_RANDOM)
        which_stat = static_cast<stat_type>(random2(NUM_STATS));

    // scale modifier by player_sust_abil() - right-shift
    // permissible because stat_loss is unsigned: {dlb}
    if (!force)
    {
        int sust = player_sust_abil();
        stat_loss >>= sust;

        if (sust && !stat_loss && !player_sust_abil(false))
        {
            item_def *ring = get_only_unided_ring();
            if (ring && !is_artefact(*ring)
                && ring->sub_type == RING_SUSTAIN_ABILITIES)
            {
                wear_id_type(*ring);
            }
        }
    }

    mprf(stat_loss > 0 ? MSGCH_WARN : MSGCH_PLAIN,
         pgettext("stat", "You feel %s%s."),
         pgettext_expr("stat", stat_desc(which_stat, SD_LOSS)),
         stat_loss > 0 ? "" : gettext(" for a moment"));

    if (stat_loss > 0)
    {
        you.stat_loss[which_stat] = std::min<int>(100,
                                        you.stat_loss[which_stat] + stat_loss);
        _handle_stat_change(which_stat, cause, see_source);
        return (true);
    }
    else
        return (false);
}

bool lose_stat(stat_type which_stat, int stat_loss, bool force,
               const std::string cause, bool see_source)
{
    return lose_stat(which_stat, stat_loss, force, cause.c_str(), see_source);
}

bool lose_stat(stat_type which_stat, int stat_loss,
               const monster* cause, bool force)
{
    if (cause == NULL || invalid_monster(cause))
        return lose_stat(which_stat, stat_loss, force, NULL, true);

    bool        vis  = you.can_see(cause);
    std::string name = cause->name(DESC_A, true);

    if (cause->has_ench(ENCH_SHAPESHIFTER))
        name += " (shapeshifter)";
    else if (cause->has_ench(ENCH_GLOWING_SHAPESHIFTER))
        name += " (glowing shapeshifter)";

    return lose_stat(which_stat, stat_loss, force, name, vis);
}

bool lose_stat(stat_type which_stat, int stat_loss,
               const item_def &cause, bool removed, bool force)
{
    std::string name = cause.name(true, DESC_THE, false, true, false, false,
                                  ISFLAG_KNOW_CURSE | ISFLAG_KNOW_PLUSES);
    std::string verb;

    switch (cause.base_type)
    {
    case OBJ_ARMOUR:
    case OBJ_JEWELLERY:
        if (removed)
            verb = "removing";
        else
            verb = "wearing";
        break;

    case OBJ_WEAPONS:
    case OBJ_STAVES:
        if (removed)
            verb = "unwielding";
        else
            verb = "wielding";
        break;

    case OBJ_WANDS:   verb = "zapping";  break;
    case OBJ_FOOD:    verb = "eating";   break;
    case OBJ_SCROLLS: verb = "reading";  break;
    case OBJ_POTIONS: verb = "drinking"; break;
    default:          verb = "using";
    }

    return lose_stat(which_stat, stat_loss, force, verb + " " + name, true);
}

static std::string _stat_name(stat_type stat)
{
    switch (stat)
    {
    case STAT_STR:
        return (M_("strength"));
    case STAT_INT:
        return (M_("intelligence"));
    case STAT_DEX:
        return (M_("dexterity"));
    default:
        die("invalid stat");
    }
}

static stat_type _random_lost_stat()
{
    stat_type choice = NUM_STATS;
    int found = 0;
    for (int i = 0; i < NUM_STATS; ++i)
        if (you.stat_loss[i] > 0)
        {
            found++;
            if (one_chance_in(found))
                choice = static_cast<stat_type>(i);
        }
    return (choice);
}

// Restore the stat in which_stat by the amount in stat_gain, displaying
// a message if suppress_msg is false, and doing so in the recovery
// channel if recovery is true.  If stat_gain is 0, restore the stat
// completely.
bool restore_stat(stat_type which_stat, int stat_gain,
                  bool suppress_msg, bool recovery)
{
    // A bit hackish, but cut me some slack, man! --
    // Besides, a little recursion never hurt anyone {dlb}:
    if (which_stat == STAT_ALL)
    {
        bool stat_restored = false;
        for (int i = 0; i < NUM_STATS; ++i)
            if (restore_stat(static_cast<stat_type>(i), stat_gain, suppress_msg))
                stat_restored = true;

        return (stat_restored);
    }

    if (which_stat == STAT_RANDOM)
        which_stat = _random_lost_stat();

    if (which_stat >= NUM_STATS || you.stat_loss[which_stat] == 0)
        return (false);

    if (!suppress_msg)
    {
        mprf(recovery ? MSGCH_RECOVERY : MSGCH_PLAIN,
             gettext("You feel your %s returning."),
             gettext(_stat_name(which_stat).c_str()));
    }

    if (stat_gain == 0 || stat_gain > you.stat_loss[which_stat])
        stat_gain = you.stat_loss[which_stat];

    you.stat_loss[which_stat] -= stat_gain;
    _handle_stat_change(which_stat);
    return (true);
}

static void _normalize_stat(stat_type stat)
{
    ASSERT(you.stat_loss[stat] >= 0);
    // XXX: this doesn't prevent effective stats over 72.
    you.base_stats[stat] = std::min<int8_t>(you.base_stats[stat], 72);
}

// Number of turns of stat at zero you start with.
#define STAT_ZERO_START 10
// Number of turns of stat at zero you can survive.
#define STAT_DEATH_TURNS 100
// Number of turns of stat at zero after which random paralysis starts.
#define STAT_DEATH_START_PARA 50

static void _handle_stat_change(stat_type stat, const char* cause, bool see_source)
{
    ASSERT(stat >= 0 && stat < NUM_STATS);

    if (you.stat(stat) <= 0 && you.stat_zero[stat] == 0)
    {
        you.stat_zero[stat] = STAT_ZERO_START;
        you.stat_zero_cause[stat] = cause;
        mprf(MSGCH_WARN, gettext("You have lost your %s."), gettext(stat_desc(stat, SD_NAME)));
        // 2 to 5 turns of paralysis (XXX: decremented right away?)
        you.increase_duration(DUR_PARALYSIS, 2 + random2(3));
    }

    you.redraw_stats[stat] = true;
    _normalize_stat(stat);

    switch (stat)
    {
    case STAT_STR:
        burden_change();
        you.redraw_armour_class = true; // includes shields
        break;

    case STAT_INT:
        break;

    case STAT_DEX:
        you.redraw_evasion = true;
        you.redraw_armour_class = true; // includes shields
        break;

    default:
        break;
    }
}

static void _handle_stat_change(const char* aux, bool see_source)
{
    for (int i = 0; i < NUM_STATS; ++i)
        _handle_stat_change(static_cast<stat_type>(i), aux, see_source);
}

// Called once per turn.
void update_stat_zero()
{
    stat_type para_stat = NUM_STATS;
    int num_para = 0;
    for (int i = 0; i < NUM_STATS; ++i)
    {
        stat_type s = static_cast<stat_type>(i);
        if (you.stat(s) <= 0)
            you.stat_zero[s]++;
        else if (you.stat_zero[s] > 0)
        {
            you.stat_zero[s]--;
            if (you.stat_zero[s] == 0)
            {
                mprf(gettext("Your %s has recovered."), gettext(stat_desc(s, SD_NAME)));
                you.redraw_stats[s] = true;
                if (i == STAT_STR)
                    burden_change();
            }
        }
        else // no stat penalty at all
            continue;

        if (you.stat_zero[i] > STAT_DEATH_TURNS)
        {
            ouch(INSTANT_DEATH, NON_MONSTER,
                 _statloss_killtype(s), you.stat_zero_cause[i].c_str());
        }

        int paramax = STAT_DEATH_TURNS - STAT_DEATH_START_PARA;
        int paradiff = std::max(you.stat_zero[i] - STAT_DEATH_START_PARA, 0);
        if (x_chance_in_y(paradiff*paradiff, 2*paramax*paramax))
        {
            para_stat = s;
            num_para++;
        }
    }

    switch (num_para)
    {
    case 0:
        break;
    case 1:
        if (you.duration[DUR_PARALYSIS])
            break;
        mprf(MSGCH_WARN, gettext("You faint for lack of %s."),
                         gettext(stat_desc(para_stat, SD_NAME)));
        you.increase_duration(DUR_PARALYSIS, 1 + roll_dice(1,3));
        break;
    default:
        if (you.duration[DUR_PARALYSIS])
            break;
        mpr(gettext("Your lost attributes cause you to faint."), MSGCH_WARN);
        you.increase_duration(DUR_PARALYSIS, 1 + roll_dice(num_para, 3));
        break;
    }
}
