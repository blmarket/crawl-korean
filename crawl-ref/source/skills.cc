/**
 * @file
 * @brief Skill exercising functions.
**/

#include "AppHdr.h"

#include "skills.h"

#include <algorithm>
#include <string.h>
#include <stdlib.h>

#include "evoke.h"
#include "externs.h"
#include "godabil.h"
#include "godconduct.h"
#include "hints.h"
#include "itemprop.h"
#include "notes.h"
#include "output.h"
#include "player.h"
#include "random.h"
#include "random-weight.h"
#include "skills2.h"
#include "spl-cast.h"
#include "sprint.h"
#include "state.h"


// MAX_COST_LIMIT is the maximum XP amount it will cost to raise a skill
//                by 10 skill points (ie one standard practice).
//
// MAX_SPENDING_LIMIT is the maximum XP amount we allow the player to
//                    spend on a skill in a single raise.
//
// Note that they don't have to be equal, but it is important to make
// sure that they're set so that the spending limit will always allow
// for 1 skill point to be earned.
#define MAX_COST_LIMIT           250
#define MAX_SPENDING_LIMIT       250

static int _train(skill_type exsk, int &max_exp, bool simu = false);

int skill_cost_needed(int level)
{
    static bool init = true;
    static int scn[27];

    if (init)
    {
        // The progress of skill_cost_level depends only on total skill points,
        // it's independent of species. We try to keep close to the old system
        // and use minotaur as a reference (exp apt: 140). This means that for
        // a species with 140 exp apt, skill_cost_level will be about the same
        // as XL (a bit lower in the beginning).
        // Changed to exp apt 130 to slightly increase mid and late game prices.
        species_type sp = you.species;
        you.species = SP_KENKU;

        // The average starting skill total is actually lower, but monks get
        // about 1200, and they would start around skill cost level 4 if we
        // used the average.
        scn[0] = 1200;

        for (int i = 1; i < 27; ++i)
        {
            scn[i] = scn[i - 1] + (exp_needed(i + 1) - exp_needed(i)) * 10
                                  / calc_skill_cost(i);
        }

        scn[0] = 0;
        you.species = sp;
        init = false;
    }

    return scn[level - 1];
}

void calc_total_skill_points(void)
{
    int i;

    you.total_skill_points = 0;

    for (i = 0; i < NUM_SKILLS; i++)
        you.total_skill_points += you.skill_points[i];

    for (i = 1; i <= 27; i++)
        if (you.total_skill_points < skill_cost_needed((skill_type)i))
            break;

    you.skill_cost_level = i - 1;

#ifdef DEBUG_DIAGNOSTICS
    you.redraw_experience = true;
#endif
}

// skill_cost_level makes skills more expensive for more experienced characters
int calc_skill_cost(int skill_cost_level)
{
    const int cost[] = { 1, 2, 3, 4, 5,            // 1-5
                         7, 9, 12, 15, 18,         // 6-10
                         28, 40, 56, 76, 100,      // 11-15
                         130, 165, 195, 215, 230,  // 16-20
                         240, 248, 250, 250, 250,  // 21-25
                         250, 250 };

    return cost[skill_cost_level - 1];
}

// Characters are actually granted skill points, not skill levels.
// Here we take racial aptitudes into account in determining final
// skill levels.
void reassess_starting_skills()
{
    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
    {
        skill_type sk = static_cast<skill_type>(i);
        if (you.skills[sk] == 0
            && (you.species != SP_VAMPIRE || sk != SK_UNARMED_COMBAT))
        {
            continue;
        }

        // Grant the amount of skill points required for a human.
        you.skill_points[sk] = skill_exp_needed(you.skills[sk], sk,
        static_cast<species_type>(SP_HUMAN)) + 1;

        // Find out what level that earns this character.
        you.skills[sk] = 0;

        for (int lvl = 1; lvl <= 8; ++lvl)
        {
            if (you.skill_points[sk] > skill_exp_needed(lvl, sk))
                you.skills[sk] = lvl;
            else
                break;
        }

        // Vampires should always have Unarmed Combat skill.
        if (you.species == SP_VAMPIRE && sk == SK_UNARMED_COMBAT
            && you.skills[sk] < 1)
        {
            you.skill_points[sk] = skill_exp_needed(1, sk);
            you.skills[sk] = 1;
        }

        // Wanderers get at least 1 level in their skills.
        if (you.char_class == JOB_WANDERER && you.skills[sk] < 1)
        {
            you.skill_points[sk] = skill_exp_needed(1, sk);
            you.skills[sk] = 1;
        }

        // Spellcasters should always have Spellcasting skill.
        if (sk == SK_SPELLCASTING && you.skills[sk] < 1)
        {
            you.skill_points[sk] = skill_exp_needed(1, sk);
            you.skills[sk] = 1;
        }
    }
}

// When a skill is gained, we insert in the queue the exercises left in the
// training array.
void gain_skill(skill_type sk)
{

    if (you.religion == GOD_TROG && sk == SK_SPELLCASTING)
        you.training[sk] = 0;
    else
    {
        // We insert the rest of the exercises in the queue.
        while (you.training[sk] > 0)
        {
            you.exercises.pop_front();
            int pos = you.training[sk]
                      + random2(you.exercises.size() - you.training[sk]);
            std::list<skill_type>::iterator it = you.exercises.begin();
            while (pos--)
                ++it;
            you.exercises.insert(it, sk);
            --you.training[sk];
        }
    }
}

// When a skill is lost, we clear the queue of leftover exercises and put then
// in the training array.
void lose_skill(skill_type sk)
{
    int num_exercises = 0;
    you.training[sk] = 0;
    you.train[sk] = 1;
    for (std::list<skill_type>::iterator it = you.exercises.begin();
         it != you.exercises.end(); ++it)
    {
        if (sk == *it)
            ++num_exercises;
    }

    if (!num_exercises)
        return;

    you.exercises.remove(sk);

    FixedVector<unsigned int, NUM_SKILLS> training = you.training;
    for (int i = 0; i < NUM_SKILLS; ++i)
        if (!skill_known(i))
            training[i] = 0;

    for (int i = 0; i < num_exercises; ++i)
    {
        you.exercises.push_back(static_cast<skill_type>(
                                random_choose_weighted(training)));
    }
    you.training[sk] = num_exercises;
}

static void _change_skill_level(skill_type exsk, int n)
{
    ASSERT(n != 0);
    skill_type old_best_skill = best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
    bool need_reset = false;

    if (-n > you.skills[exsk])
        n = -you.skills[exsk];
    you.skills[exsk] += n;

    if (n > 0)
        take_note(Note(NOTE_GAIN_SKILL, exsk, you.skills[exsk]));
    else
        take_note(Note(NOTE_LOSE_SKILL, exsk, you.skills[exsk]));

    if (you.skills[exsk] == 27)
    {
        mprf(MSGCH_INTRINSIC_GAIN, gettext("You have mastered %s!"), gettext(skill_name(exsk)));
    }
    else if (you.skills[exsk] == 1 && n > 0)
    {
        mprf(MSGCH_INTRINSIC_GAIN, gettext("You have gained %s skill!"), gettext(skill_name(exsk)));
        hints_gained_new_skill(exsk);
    }
    else if (!you.skills[exsk])
    {
        mprf(MSGCH_INTRINSIC_GAIN, gettext("You have lost %s skill!"), gettext(skill_name(exsk)));
        lose_skill(exsk);
        need_reset = true;
    }
    else if (abs(n) == 1 && you.num_turns)
    {
        mprf(MSGCH_INTRINSIC_GAIN, gettext("Your %s skill %s to level %d!"),
             gettext(skill_name(exsk)), 
             (n > 0) ? pgettext("_change_skill_level", "increases") 
                     : pgettext("_change_skill_level", "decreases"),
             you.skills[exsk]);
    }
    else if (you.num_turns)
    {
        mprf(MSGCH_INTRINSIC_GAIN, 
             gettext("Your %s skill %s %d levels and is now at level %d!"), 
             gettext(skill_name(exsk)), 
             (n > 0) ? pgettext("_change_skill_level", "gained") 
                     : pgettext("_change_skill_level", "lost"),
             abs(n), you.skills[exsk]);
    }

    if (n > 0 && you.num_turns)
        learned_something_new(HINT_SKILL_RAISE);

    if (you.skills[exsk] - n == 27)
    {
        you.train[exsk] = 1;
        need_reset = true;
    }

    // Recalculate this skill's order for tie breaking skills
    // at its new level.   See skills2.cc::init_skill_order()
    // for more details.  -- bwr
    you.skill_order[exsk] = 0;
    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
    {
        skill_type sk = static_cast<skill_type>(i);
        if (sk != exsk && you.skills[sk] >= you.skills[exsk])
            you.skill_order[exsk]++;
    }

    if (exsk == SK_FIGHTING)
        calc_hp();

    if (exsk == SK_INVOCATIONS || exsk == SK_SPELLCASTING)
        calc_mp();

    if (exsk == SK_DODGING || exsk == SK_ARMOUR)
        you.redraw_evasion = true;

    if (exsk == SK_ARMOUR || exsk == SK_SHIELDS
        || exsk == SK_ICE_MAGIC || exsk == SK_EARTH_MAGIC
        || you.duration[DUR_TRANSFORMATION] > 0)
    {
        you.redraw_armour_class = true;
    }

    const skill_type best_spell = best_skill(SK_SPELLCASTING,
                                             SK_LAST_MAGIC);
    if (exsk == SK_SPELLCASTING && you.skills[exsk] == 1
        && best_spell == SK_SPELLCASTING && n > 0)
    {
        mpr(gettext("You're starting to get the hang of this magic thing."));
        learned_something_new(HINT_GAINED_SPELLCASTING);
    }

    const skill_type best = best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
    if (best != old_best_skill || old_best_skill == exsk)
        redraw_skill(you.your_name, player_title());

    if (need_reset)
        reset_training();
    // TODO: also identify rings of wizardry.
}

void check_skill_level_change(skill_type sk, bool do_level_up)
{
    int new_level = you.skills[sk];
    while (1)
    {
        const unsigned int prev = skill_exp_needed(new_level, sk);
        const unsigned int next = skill_exp_needed(new_level + 1, sk);

        if (you.skill_points[sk] >= next)
        {
            if (++new_level >= 27)
            {
                new_level = 27;
                break;
            }
        }
        else if (you.skill_points[sk] < prev)
        {
            new_level--;
            ASSERT(new_level >= 0);
        }
        else
            break;
    }

    if (new_level != you.skills[sk])
        if (do_level_up)
            _change_skill_level(sk, new_level - you.skills[sk]);
        else
            you.skills[sk] = new_level;
}

/*
 * Fill the exercise queue with random values in proportion to the training
 * array.
 */
static void _init_exercise_queue()
{
    ASSERT(you.exercises.empty());
    FixedVector<unsigned int, NUM_SKILLS> prac = you.training;

    // We remove unknown skills, since we don't want then in the queue.
    for (int i = 0; i < NUM_SKILLS; ++i)
        if (!skill_known(i))
            prac[i] = 0;

    for (int i = 0; i < EXERCISE_QUEUE_SIZE; ++i)
    {
        skill_type sk = static_cast<skill_type>(random_choose_weighted(prac));
        if (is_invalid_skill(sk))
            sk = static_cast<skill_type>(random_choose_weighted(you.training));

        if (!skill_known(sk))
            continue;

        you.exercises.push_back(sk);
        --prac[sk];
    }
}

/*
 * Init the training array by scaling down the skill_points array to 100.
 * Used at game setup, and when upgrading saves.
 */
void init_training()
{
    int total = 0;
    for (int i = 0; i < NUM_SKILLS; ++i)
        if (you.train[i] && skill_known(i))
            total += you.skill_points[i];

    // If no trainable skills, exit.
    if (!total)
        return;

    for (int i = 0; i < NUM_SKILLS; ++i)
        if (skill_known(i))
            you.training[i] = you.skill_points[i] * 100 / total;

    _init_exercise_queue();
}

static bool _cmp_rest(const std::pair<skill_type,int>& a,
                      const std::pair<skill_type,int>& b)
{
    return a.second < b.second;
}

// Make sure at least one skill is selected.
void check_selected_skills()
{
    skill_type first_selectable = SK_NONE;
    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        skill_type sk = static_cast<skill_type>(i);
        if (you.train[sk] && skill_known(sk))
            return;
        if (!skill_known(sk) || you.skills[sk] == 27)
            continue;
        if (is_invalid_skill(first_selectable))
            first_selectable = sk;
    }

    if (!is_invalid_skill(first_selectable))
        you.train[first_selectable] = 1;

    // It's possible to have no selectable skills, if they are all at level 0
    // or level 27, so we don't assert. XP will just accumulate in the pool.
}

/*
 * Scale the training array.
 *
 * @param scale The new scale of the array.
 * @param known Are we scaling known or unknown skills? Never do both at the
 *              same time.
 * @param exact When true, we'll make sure that the sum of the scaled skills
 *              is equal to the scale.
 */
static void _scale_training(int scale, bool known, bool exact)
{
    int total = 0;
    // First, we calculate the sum of the values to be scaled.
    for (int i = 0; i < NUM_SKILLS; ++i)
        if (known == skill_known(i) && you.training[i] > 0)
            total += you.training[i];

    std::vector<std::pair<skill_type,int> > rests;
    int scaled_total = 0;

    // All skills disabled, nothing to do.
    if (!total)
        return;

    // Now we scale the values.
    for (int i = 0; i < NUM_SKILLS; ++i)
        if (known == skill_known(i) && you.training[i] > 0)
        {
            int result = you.training[i] * scale;
            const int rest = result % total;
            if (rest)
                rests.push_back(std::pair<skill_type,int>(skill_type(i), rest));
            you.training[i] = result / total;
            scaled_total += you.training[i];
        }

    ASSERT(scaled_total <= scale);

    if (!exact || scaled_total == scale)
        return;

    // We ensure that the percentage always add up to 100 by increasing the
    // training for skills which had the higher rest from the above scaling.
    std::sort(rests.begin(), rests.end(), _cmp_rest);
    std::vector<std::pair<skill_type,int> >::iterator it = rests.begin();
    while (scaled_total < scale && it != rests.end())
    {
        ++you.training[it->first];
        ++scaled_total;
        ++it;
    }

    ASSERT(scaled_total == scale);
}

/*
 * Reset the training array. Unknown skills are not touched and disabled ones
 * are skipped. In automatic mode, we use values from the exercise queue.
 * In manual mode, all enabled skills are set to the same value.
 * Result is scaled back to 100.
 */
void reset_training()
{
    const int MAX_TRAINING_UNKNOWN = 50;
    int total_unknown = 0;

    // We clear the values of known skills in the training array. In auto mode
    // they are set to 0 (and filled later with the content of the queue), in
    // manual mode, they are all set to 1.
    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        if (!skill_known(i))
            total_unknown += you.training[i];
        else if (you.auto_training)
            you.training[i] = 0;
        else
            you.training[i] = you.train[i];
    }

    bool empty = true;
    // In automatic mode, we fill the array with the content of the queue.
    if (you.auto_training)
    {
        for (std::list<skill_type>::iterator it = you.exercises.begin();
             it != you.exercises.end(); ++it)
        {
            skill_type sk = *it;
            if (you.train[sk])
            {
                // Only known skills should be in the queue.
                ASSERT(skill_known(sk));
                you.training[sk] += you.train[sk];
                empty = false;
            }
        }

        // The selected skills have not been exercised recently. Give them all
        // a default weight of 1 (or 2 for focus skills).
        if (empty)
            for (int sk = 0; sk < NUM_SKILLS; ++sk)
                if (you.train[sk] && skill_known(sk))
                    you.training[sk] = you.train[sk];

        // Focused skills get at least 20% training.
        for (int sk = 0; sk < NUM_SKILLS; ++sk)
            if (you.train[sk] == 2 && you.training[sk] < 20)
                you.training[sk] += 5 * (5 - you.training[sk] / 4);
    }

    if (total_unknown > MAX_TRAINING_UNKNOWN)
    {
        _scale_training(MAX_TRAINING_UNKNOWN, false, true);
        total_unknown = MAX_TRAINING_UNKNOWN;
    }

    _scale_training(100 - total_unknown, true, you.auto_training);
}

// returns total number of skill points gained
void exercise(skill_type exsk, int deg)
{
    if (you.skills[exsk] >= 27 || !you.train[exsk])
        return;

    dprf("Exercise %s by %d.", skill_name(exsk), deg);

    if (!skill_known(exsk))
        you.training[exsk] += deg;
    else
        while (deg > 0)
        {
            you.exercises.pop_front();
            you.exercises.push_back(exsk);
            deg--;
        }
    reset_training();
}

// Check if we should stop training this skill immediately.
// We look at skill points because actual level up comes later.
static bool _level_up_check(skill_type sk)
{
    // New skill learned.
    const bool skill_learned  = !skill_known(sk)
                    && you.skill_points[sk] >= skill_exp_needed(1, sk);

    if (skill_learned)
        gain_skill(sk);

    // Don't train past level 27.
    // In manual mode, we stop training and automatically disable new skills.
    if (you.skill_points[sk] >= skill_exp_needed(27, sk)
        || skill_learned && !you.auto_training)
    {
        you.train[sk] = you.training[sk] = 0;
        if (!skill_learned)
            check_selected_skills();
        return true;
    }

    return false;
}

static bool _is_magic_skill(skill_type sk)
{
    // Learning new skills doesn't count for Trog because punishment has
    // already been given for casting. And we don't want to punish
    // learning spellcasting from scrolls.
    if (you.religion == GOD_TROG && !skill_known(sk))
        return false;

    return (sk > SK_LAST_MUNDANE && sk <= SK_LAST_MAGIC);
}

void train_skills(bool simu)
{
    int cost, exp;
    do
    {
        cost = calc_skill_cost(you.skill_cost_level);
        exp = you.exp_available;
        if (you.skill_cost_level == 27)
            train_skills(exp, cost, simu);
        else
        {
            // Amount of skill points needed to reach the next skill cost level
            // divided by 10 (integer divison rounded up).
            const int next_level = (skill_cost_needed(you.skill_cost_level + 1)
                                    - you.total_skill_points + 9) / 10;
            ASSERT(next_level > 0);
            train_skills(std::min(exp, cost * next_level), cost, simu);
        }
    }
    while (you.exp_available >= cost && exp != you.exp_available);

    for (int i = 0; i < NUM_SKILLS; ++i)
        check_skill_level_change(static_cast<skill_type>(i), !simu);

    // We might have disabled some skills on level up.
    reset_training();
}

//#define DEBUG_TRAINING_COST
void train_skills(int exp, const int cost, const bool simu)
{
    bool skip_first_phase = false;
    int magic_gain = 0;
    FixedVector<int, NUM_SKILLS> sk_exp;
    sk_exp.init(0);
    std::vector<skill_type> training_order;
#ifdef DEBUG_DIAGNOSTICS
    FixedVector<int, NUM_SKILLS> total_gain;
    total_gain.init(0);
#endif
#ifdef DEBUG_TRAINING_COST
    int exp_pool = you.exp_available;
    dprf("skill cost level: %d, cost: %dxp/10skp, max XP usable: %d.",
         you.skill_cost_level, cost, exp);
#endif


    // We scale the training array to the amount of XP available in the pool.
    // That gives us the amount of XP available to train each skill.
    for (int i = 0; i < NUM_SKILLS; ++i)
        if (you.training[i] > 0)
        {
            sk_exp[i] = you.training[i] * exp / 100;
            if (sk_exp[i] < cost)
            {
                // One skill has a too low training to be trained at all.
                // We skip the first phase and go directly to the random
                // phase so it has a chance to be trained.
                skip_first_phase = true;
                break;
            }
            training_order.push_back(static_cast<skill_type>(i));
        }

    if (!skip_first_phase)
    {
        // We randomize the order, to avoid a slight bias to first skills.
        // Being trained first can make a difference if skill cost increases.
        std::random_shuffle(training_order.begin(), training_order.end());
        for (std::vector<skill_type>::iterator it = training_order.begin();
             it != training_order.end(); ++it)
        {
            skill_type sk = *it;
            int gain = 0;

            while (sk_exp[sk] >= cost && you.training[sk])
            {
                exp -= sk_exp[sk];
                gain += _train(sk, sk_exp[sk], simu);
                exp += sk_exp[sk];
                ASSERT(exp >= 0);
                if (_level_up_check(sk))
                    sk_exp[sk] = 0;
            }

            if (gain && _is_magic_skill(sk))
                magic_gain += gain;

#ifdef DEBUG_DIAGNOSTICS
           total_gain[sk] += gain;
#endif
        }
    }
    // If there's enough xp in the pool, we use it to train skills selected
    // with random_choose_weighted.
    while (exp >= cost)
    {
        int gain;
        skill_type sk = SK_NONE;
        if (!skip_first_phase)
            sk = static_cast<skill_type>(random_choose_weighted(sk_exp));
        if (is_invalid_skill(sk))
            sk = static_cast<skill_type>(random_choose_weighted(you.training));
        if (!is_invalid_skill(sk))
        {
            gain = _train(sk, exp, simu);
            ASSERT(exp >= 0);
            sk_exp[sk] = 0;
        }
        else
        {
            // No skill to train. Can happen if all skills are at 27.
            break;
        }

        _level_up_check(sk);

        if (gain && _is_magic_skill(sk))
            magic_gain += gain;

#ifdef DEBUG_DIAGNOSTICS
        total_gain[sk] += gain;
#endif
    }

#ifdef DEBUG_DIAGNOSTICS
    if (!crawl_state.script)
    {
#ifdef DEBUG_TRAINING_COST
        int total = 0;
#endif
        for (int i = 0; i < NUM_SKILLS; ++i)
        {
            skill_type sk = static_cast<skill_type>(i);
            if (total_gain[sk] && !simu)
                dprf("Trained %s by %d.", skill_name(sk), total_gain[sk]);
#ifdef DEBUG_TRAINING_COST
            total += total_gain[sk];
        }
        dprf("Total skill points gained: %d, cost: %d XP.",
             total, exp_pool - you.exp_available);
#else
        }
#endif
    }
#endif

    // Avoid doubly rewarding spell practise in sprint
    // (by inflated XP and inflated piety gain)
    if (crawl_state.game_is_sprint())
        magic_gain = sprint_modify_exp_inverse(magic_gain);

    if (magic_gain && !simu)
        did_god_conduct(DID_SPELL_PRACTISE, magic_gain);
}

void train_skill(skill_type skill, int exp)
{
    const int cost = calc_skill_cost(you.skill_cost_level);
    int gain = 0;

    while (exp >= cost)
        gain += _train(skill, exp);

    dprf("Trained %s by %d.", skill_name(skill), gain);
}

static int _stat_mult(skill_type exsk, int skill_inc)
{
    int stat = 10;

    if ((exsk >= SK_FIGHTING && exsk <= SK_STAVES) || exsk == SK_ARMOUR)
    {
        // These skills are Easier for the strong.
        stat = you.strength();
    }
    else if (exsk >= SK_SLINGS && exsk <= SK_UNARMED_COMBAT)
    {
        // These skills are easier for the dexterous.
        // Note: Armour is handled above.
        stat = you.dex();
    }
    else if (exsk >= SK_SPELLCASTING && exsk <= SK_LAST_MAGIC)
    {
        // These skills are easier for the smart.
        stat = you.intel();
    }

    return (skill_inc * std::max<int>(5, stat) / 10);
}

void check_skill_cost_change()
{
    if (you.skill_cost_level < 27
        && you.total_skill_points
           >= skill_cost_needed(you.skill_cost_level + 1))
    {
        you.skill_cost_level++;
    }
    else if (you.skill_cost_level > 0
        && you.total_skill_points
           < skill_cost_needed(you.skill_cost_level))
    {
        you.skill_cost_level--;
    }
}

void change_skill_points(skill_type sk, int points, bool do_level_up)
{
    if (static_cast<int>(you.skill_points[sk]) < -points)
        points = -(int)you.skill_points[sk];

    you.skill_points[sk] += points;
    you.total_skill_points += points;

    check_skill_level_change(sk, do_level_up);
}

static int _train(skill_type exsk, int &max_exp, bool simu)
{
    // This will be added to you.skill_points[exsk];
    int skill_inc = 10;

    // This will be deducted from you.exp_available.
    int cost = calc_skill_cost(you.skill_cost_level);

    // Being good at some weapons makes others easier to learn.
    if (exsk < SK_ARMOUR)
        skill_inc *= crosstrain_bonus(exsk);

    // Starting to learn skills is easier if the appropriate stat is high.
        // We check skill points in case skill level hasn't been updated yet
    if (you.skill_points[exsk] < skill_exp_needed(1, exsk))
        skill_inc = _stat_mult(exsk, skill_inc);

    if (is_antitrained(exsk))
        cost *= ANTITRAIN_PENALTY;

    // Scale cost and skill_inc to available experience.
    const int spending_limit = std::min(MAX_SPENDING_LIMIT, max_exp);
    if (cost > spending_limit)
    {
        int frac = (spending_limit * 10) / cost;
        cost = spending_limit;
        skill_inc = (skill_inc * frac) / 10;
    }

    if (skill_inc <= 0)
        return (0);

    // Bonus from manual
    if (exsk == you.manual_skill)
    {
        item_def& manual(you.inv[you.manual_index]);
        const int bonus = std::min<int>(skill_inc, manual.plus2);
        skill_inc += bonus;
        manual.plus2 -= bonus;
        if (!manual.plus2 && !simu)
            stop_studying_manual(true);
    }

    if (!skill_known(exsk) && you.training[exsk] > 0
        && x_chance_in_y(skill_inc, 10))
    {
        --you.training[exsk];
    }

    you.skill_points[exsk] += skill_inc;
    you.ct_skill_points[exsk] += (1 - 1 / crosstrain_bonus(exsk))
                                 * skill_inc;
    you.exp_available -= cost;
    max_exp -= cost;
    you.total_skill_points += skill_inc;

    check_skill_cost_change();
    ASSERT(you.exp_available >= 0);
    ASSERT(max_exp >= 0);
    you.redraw_experience = true;

    return (skill_inc);
}
