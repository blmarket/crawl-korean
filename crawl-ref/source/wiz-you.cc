/**
 * @file
 * @brief Player related debugging functions.
**/

#include "AppHdr.h"

#include "wiz-you.h"

#include "cio.h"
#include "debug.h"
#include "dbg-util.h"
#include "food.h"
#include "godprayer.h"
#include "godwrath.h"
#include "libutil.h"
#include "message.h"
#include "mutation.h"
#include "newgame.h"
#include "ng-setup.h"
#include "output.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "view.h"
#include "xom.h"

#ifdef WIZARD
void wizard_change_species(void)
{
    char specs[80];
    int i;

    msgwin_get_line("What species would you like to be now? " ,
                    specs, sizeof(specs));

    if (specs[0] == '\0')
        return;
    std::string spec = lowercase_string(specs);

    species_type sp = SP_UNKNOWN;

    for (i = 0; i < NUM_SPECIES; ++i)
    {
        const species_type si = static_cast<species_type>(i);
        const std::string sp_name = lowercase_string(species_name(si));

        std::string::size_type pos = sp_name.find(spec);
        if (pos != std::string::npos)
        {
            if (pos == 0)
            {
                // We prefer prefixes over partial matches.
                sp = si;
                break;
            }
            else
                sp = si;
        }
    }

    if (sp == SP_UNKNOWN)
    {
        mpr("That species isn't available.");
        return;
    }

    // Re-scale skill-points.
    for (i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
    {
        skill_type sk = static_cast<skill_type>(i);
        you.skill_points[i] *= species_apt_factor(sk, sp)
                               / species_apt_factor(sk);
    }

    you.species = sp;
    you.is_undead = get_undead_state(sp);

    // Change permanent mutations, but preserve non-permanent ones.
    uint8_t prev_muts[NUM_MUTATIONS];
    for (i = 0; i < NUM_MUTATIONS; ++i)
    {
        if (you.innate_mutations[i] > 0)
        {
            if (you.innate_mutations[i] > you.mutation[i])
                you.mutation[i] = 0;
            else
                you.mutation[i] -= you.innate_mutations[i];

            you.innate_mutations[i] = 0;
        }
        prev_muts[i] = you.mutation[i];
    }
    give_basic_mutations(sp);
    for (i = 0; i < NUM_MUTATIONS; ++i)
    {
        if (prev_muts[i] > you.innate_mutations[i])
            you.innate_mutations[i] = 0;
        else
            you.innate_mutations[i] -= prev_muts[i];
    }

    switch (sp)
    {
    case SP_RED_DRACONIAN:
        if (you.experience_level >= 7)
            perma_mutate(MUT_HEAT_RESISTANCE, 1);
        break;

    case SP_WHITE_DRACONIAN:
        if (you.experience_level >= 7)
            perma_mutate(MUT_COLD_RESISTANCE, 1);
        break;

    case SP_GREEN_DRACONIAN:
        if (you.experience_level >= 7)
            perma_mutate(MUT_POISON_RESISTANCE, 1);
        if (you.experience_level >= 14)
            perma_mutate(MUT_STINGER, 1);
        break;

    case SP_YELLOW_DRACONIAN:
        if (you.experience_level >= 14)
            perma_mutate(MUT_ACIDIC_BITE, 1);
        break;

    case SP_GREY_DRACONIAN:
        if (you.experience_level >= 7)
            perma_mutate(MUT_UNBREATHING, 1);
        break;

    case SP_BLACK_DRACONIAN:
        if (you.experience_level >= 7)
            perma_mutate(MUT_SHOCK_RESISTANCE, 1);
        if (you.experience_level >= 14)
            perma_mutate(MUT_BIG_WINGS, 1);
        break;

    case SP_DEMONSPAWN:
    {
        roll_demonspawn_mutations();
        for (i = 0; i < int(you.demonic_traits.size()); ++i)
        {
            mutation_type m = you.demonic_traits[i].mutation;

            if (you.demonic_traits[i].level_gained > you.experience_level)
                continue;

            ++you.mutation[m];
            ++you.innate_mutations[m];
        }
        break;
    }

    case SP_DEEP_DWARF:
        if (you.experience_level >= 9)
            perma_mutate(MUT_PASSIVE_MAPPING, 1);
        if (you.experience_level >= 14)
            perma_mutate(MUT_NEGATIVE_ENERGY_RESISTANCE, 1);
        if (you.experience_level >= 18)
            perma_mutate(MUT_PASSIVE_MAPPING, 1);
        break;

    case SP_FELID:
        if (you.experience_level >= 6)
            perma_mutate(MUT_SHAGGY_FUR, 1);
        if (you.experience_level >= 12)
            perma_mutate(MUT_SHAGGY_FUR, 1);
        break;

    default:
        break;
    }

    calc_hp();
    calc_mp();

    burden_change();
    update_player_symbol();
#ifdef USE_TILE
    init_player_doll();
#endif
    redraw_screen();
}
#endif

#ifdef WIZARD
// Casts a specific spell by number or name.
void wizard_cast_spec_spell(void)
{
    char specs[80], *end;
    int spell;

    mpr("Cast which spell? ", MSGCH_PROMPT);
    if (cancelable_get_line_autohist(specs, sizeof(specs))
        || specs[0] == '\0')
    {
        canned_msg(MSG_OK);
        crawl_state.cancel_cmd_repeat();
        return;
    }

    spell = strtol(specs, &end, 10);

    if (spell < 0 || end == specs)
    {
        if ((spell = spell_by_name(specs, true)) == SPELL_NO_SPELL)
        {
            mpr("Cannot find that spell.");
            crawl_state.cancel_cmd_repeat();
            return;
        }
    }

    if (your_spells(static_cast<spell_type>(spell), 0, false)
                == SPRET_ABORT)
    {
        crawl_state.cancel_cmd_repeat();
    }
}
#endif

void wizard_heal(bool super_heal)
{
    if (super_heal)
    {
        // Clear more stuff and give a HP boost.
        you.magic_contamination = 0;
        you.duration[DUR_LIQUID_FLAMES] = 0;
        you.clear_beholders();
        inc_max_hp(10);
    }

    // Clear most status ailments.
    you.rotting = 0;
    you.disease = 0;
    you.duration[DUR_CONF]      = 0;
    you.duration[DUR_MISLED]    = 0;
    you.duration[DUR_POISONING] = 0;
    set_hp(you.hp_max);
    set_mp(you.max_magic_points);
    set_hunger(10999, true);
    you.redraw_hit_points = true;
}

void wizard_set_hunger_state()
{
    std::string hunger_prompt =
        "Set hunger state to s(T)arving, (N)ear starving, (V)ery hungry, (H)ungry";
    if (you.species == SP_GHOUL)
        hunger_prompt += " or (S)atiated";
    else
        hunger_prompt += ", (S)atiated, (F)ull, ve(R)y full or (E)ngorged";
    hunger_prompt += "? ";

    mprf(MSGCH_PROMPT, "%s", hunger_prompt.c_str());

    const int c = tolower(getchk());

    switch (c)
    {
    case 't': you.hunger = HUNGER_STARVING;      break;
    case 'n': you.hunger = HUNGER_NEAR_STARVING; break;
    case 'v': you.hunger = HUNGER_VERY_HUNGRY;   break;
    case 'h': you.hunger = HUNGER_HUNGRY;        break;
    case 's': you.hunger = HUNGER_SATIATED;      break;
    case 'f': you.hunger = HUNGER_FULL;          break;
    case 'r': you.hunger = HUNGER_VERY_FULL;     break;
    case 'e': you.hunger = HUNGER_ENGORGED;      break;
    default:  canned_msg(MSG_OK); break;
    }
    --you.hunger;

    food_change();

    if (you.species == SP_GHOUL && you.hunger_state >= HS_SATIATED)
        mpr("Ghouls can never be full or above!");
}

void wizard_set_piety()
{
    if (you.religion == GOD_NO_GOD)
    {
        mpr("You are not religious!");
        return;
    }

    mprf(MSGCH_PROMPT, "Enter new piety value (current = %d, Enter for 0): ",
         you.piety);
    char buf[30];
    if (cancelable_get_line_autohist(buf, sizeof buf))
    {
        canned_msg(MSG_OK);
        return;
    }

    const int newpiety = atoi(buf);
    if (newpiety < 0 || newpiety > 200)
    {
        mpr("Piety needs to be between 0 and 200.");
        return;
    }

    if (you.religion == GOD_XOM)
    {
        you.piety = newpiety;

        // For Xom, also allow setting interest.
        mprf(MSGCH_PROMPT, "Enter new interest (current = %d, Enter for 0): ",
             you.gift_timeout);

        if (cancelable_get_line_autohist(buf, sizeof buf))
        {
            canned_msg(MSG_OK);
            return;
        }
        const int newinterest = atoi(buf);
        if (newinterest >= 0 && newinterest < 256)
            you.gift_timeout = newinterest;
        else
            mpr("Interest must be between 0 and 255.");

        mprf("Set piety to %d, interest to %d.", you.piety, newinterest);

        const std::string new_xom_favour = describe_xom_favour();
        const std::string msg = "You are now " + new_xom_favour;
        god_speaks(you.religion, msg.c_str());
        return;
    }

    if (newpiety < 1)
    {
        if (yesno("Are you sure you want to be excommunicated?", false, 'n'))
        {
            you.piety = 0;
            excommunication();
        }
        else
            canned_msg(MSG_OK);
        return;
    }
    mprf("Setting piety to %d.", newpiety);

    // We have to set the exact piety value this way, because diff may
    // be decreased to account for things like penance and gift timeout.
    int diff;
    do
    {
        diff = newpiety - you.piety;
        if (diff > 0)
            gain_piety(diff, 1, true, false);
        else if (diff < 0)
            lose_piety(-diff);
    }
    while (diff != 0);

    // Automatically reduce penance to 0.
    if (you.penance[you.religion] > 0)
        dec_penance(you.penance[you.religion]);
}

//---------------------------------------------------------------
//
// debug_add_skills
//
//---------------------------------------------------------------
#ifdef WIZARD
void wizard_exercise_skill(void)
{
    skill_type skill = debug_prompt_for_skill("Which skill (by name)? ");

    if (skill == SK_NONE)
        mpr("That skill doesn't seem to exist.");
    else
    {
        mpr("Exercising...");
        exercise(skill, 10);
    }
}
#endif

#ifdef WIZARD
void wizard_set_skill_level(skill_type skill)
{
    if (skill == SK_NONE)
        skill = debug_prompt_for_skill("Which skill (by name)? ");

    if (skill == SK_NONE)
        mpr("That skill doesn't seem to exist.");
    else
    {
        mpr(skill_name(skill));
        int amount = prompt_for_int("To what level? ", true);

        if (amount < 0)
            canned_msg(MSG_OK);
        else
        {
            const int old_amount = you.skills[skill];
            const int points = skill_exp_needed(std::min(amount, 27), skill);

            you.skill_points[skill] = points + 1;
            you.ct_skill_points[skill] = 0;
            you.skills[skill] = amount;

            if (amount == 27)
            {
                you.train[skill] = 0;
                check_selected_skills();
            }

            reset_training();
            calc_total_skill_points();

            redraw_skill(you.your_name, player_title());

            switch (skill)
            {
            case SK_FIGHTING:
                calc_hp();
                break;

            case SK_SPELLCASTING:
            case SK_INVOCATIONS:
            case SK_EVOCATIONS:
                calc_mp();
                break;

            case SK_DODGING:
                you.redraw_evasion = true;
                break;

            case SK_ARMOUR:
                you.redraw_armour_class = true;
                you.redraw_evasion = true;
                break;

            default:
                break;
            }

            mprf("%s %s to skill level %d.",
                 (old_amount < amount ? "Increased" :
                  old_amount > amount ? "Lowered"
                                      : "Reset"),
                 skill_name(skill), amount);

            if (skill == SK_STEALTH && amount == 27)
            {
                mpr("If you set the stealth skill to a value higher than 27, "
                    "hide mode is activated, and monsters won't notice you.");
            }
        }
    }
}
#endif


#ifdef WIZARD
void wizard_set_all_skills(void)
{
    int amount = prompt_for_int("Set all skills to what level? ", true);

    if (amount < 0)             // cancel returns -1 -- bwr
        canned_msg(MSG_OK);
    else
    {
        if (amount > 27)
            amount = 27;

        for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
        {
            skill_type sk = static_cast<skill_type>(i);
            if (is_invalid_skill(sk))
                continue;

            const int points = skill_exp_needed(amount, sk);

            you.skill_points[sk] = points + 1;
            you.ct_skill_points[sk] = 0;
            you.skills[sk] = amount;
        }

        redraw_skill(you.your_name, player_title());

        calc_total_skill_points();

        calc_hp();
        calc_mp();

        you.redraw_armour_class = true;
        you.redraw_evasion = true;
    }
}
#endif

#ifdef WIZARD
bool wizard_add_mutation()
{
    bool success = false;
    char specs[80];

    if (player_mutation_level(MUT_MUTATION_RESISTANCE) > 0
        && !crawl_state.is_replaying_keys())
    {
        const char* msg;

        if (you.mutation[MUT_MUTATION_RESISTANCE] == 3)
            msg = "You are immune to mutations; remove immunity?";
        else
            msg = "You are resistant to mutations; remove resistance?";

        if (yesno(msg, true, 'n'))
        {
            you.mutation[MUT_MUTATION_RESISTANCE] = 0;
            crawl_state.cancel_cmd_repeat();
        }
    }

    int answer = yesnoquit("Force mutation to happen?", true, 'n');
    if (answer == -1)
    {
        canned_msg(MSG_OK);
        return (false);
    }
    const bool force = (answer == 1);

    if (player_mutation_level(MUT_MUTATION_RESISTANCE) == 3 && !force)
    {
        mpr("Can't mutate when immune to mutations without forcing it.");
        crawl_state.cancel_cmd_repeat();
        return (false);
    }

    answer = yesnoquit("Treat mutation as god gift?", true, 'n');
    if (answer == -1)
    {
        canned_msg(MSG_OK);
        return (false);
    }
    const bool god_gift = (answer == 1);

    msgwin_get_line("Which mutation (name, 'good', 'bad', 'any', "
                    "'xom', 'slime')? ",
                    specs, sizeof(specs));

    if (specs[0] == '\0')
        return (false);

    std::string spec = lowercase_string(specs);

    mutation_type mutat = NUM_MUTATIONS;

    if (spec == "good")
        mutat = RANDOM_GOOD_MUTATION;
    else if (spec == "bad")
        mutat = RANDOM_BAD_MUTATION;
    else if (spec == "any")
        mutat = RANDOM_MUTATION;
    else if (spec == "xom")
        mutat = RANDOM_XOM_MUTATION;
    else if (spec == "slime")
        mutat = RANDOM_SLIME_MUTATION;

    if (mutat != NUM_MUTATIONS)
    {
        int old_resist = player_mutation_level(MUT_MUTATION_RESISTANCE);

        success = mutate(mutat, true, force, god_gift);

        if (old_resist < player_mutation_level(MUT_MUTATION_RESISTANCE)
            && !force)
        {
            crawl_state.cancel_cmd_repeat("Your mutation resistance has "
                                          "increased.");
        }
        return (success);
    }

    std::vector<mutation_type> partial_matches;

    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        mutation_type mut = static_cast<mutation_type>(i);
        if (!is_valid_mutation(mut))
            continue;

        const mutation_def& mdef = get_mutation_def(mut);
        if (spec == mdef.wizname)
        {
            mutat = mut;
            break;
        }

        if (strstr(mdef.wizname, spec.c_str()))
            partial_matches.push_back(mut);
    }

    // If only one matching mutation, use that.
    if (mutat == NUM_MUTATIONS && partial_matches.size() == 1)
        mutat = partial_matches[0];

    if (mutat == NUM_MUTATIONS)
    {
        crawl_state.cancel_cmd_repeat();

        if (partial_matches.empty())
            mpr("No matching mutation names.");
        else
        {
            std::vector<std::string> matches;

            for (unsigned int i = 0; i < partial_matches.size(); ++i)
                matches.push_back(get_mutation_def(partial_matches[i]).wizname);

            std::string prefix = "No exact match for mutation '" +
                spec +  "', possible matches are: ";

            // Use mpr_comma_separated_list() because the list
            // might be *LONG*.
            mpr_comma_separated_list(prefix, matches, " and ", ", ",
                                     MSGCH_DIAGNOSTICS);
        }

        return (false);
    }
    else
    {
        mprf("Found #%d: %s (\"%s\")", (int) mutat,
             get_mutation_def(mutat).wizname,
             mutation_name(mutat, 1, false).c_str());

        const int levels =
            prompt_for_int("How many levels to increase or decrease? ",
                                  false);

        if (levels == 0)
        {
            canned_msg(MSG_OK);
            success = false;
        }
        else if (levels > 0)
        {
            for (int i = 0; i < levels; ++i)
                if (mutate(mutat, true, force, god_gift))
                    success = true;
        }
        else
        {
            for (int i = 0; i < -levels; ++i)
                if (delete_mutation(mutat, true, force, god_gift))
                    success = true;
        }
    }

    return (success);
}
#endif

void wizard_set_stats()
{
    char buf[80];
    mprf(MSGCH_PROMPT, "Enter values for Str, Int, Dex (space separated): ");
    if (cancelable_get_line_autohist(buf, sizeof buf))
        return;

    int sstr = you.strength(),
        sdex = you.dex(),
        sint = you.intel();

    sscanf(buf, "%d %d %d", &sstr, &sint, &sdex);

    you.base_stats[STAT_STR] = debug_cap_stat(sstr);
    you.base_stats[STAT_INT] = debug_cap_stat(sint);
    you.base_stats[STAT_DEX] = debug_cap_stat(sdex);
    you.stat_loss.init(0);
    you.redraw_stats.init(true);
    you.redraw_evasion = true;
}

static const char* dur_names[] =
{
    "invis",
    "conf",
    "paralysis",
    "slow",
    "mesmerised",
    "haste",
    "might",
    "brilliance",
    "agility",
    "levitation",
    "berserker",
    "poisoning",
    "confusing touch",
    "sure blade",
    "corona",
    "deaths door",
    "fire shield",
    "building rage",
    "exhausted",
    "liquid flames",
    "icy armour",
    "repel missiles",
    "prayer",
    "piety pool",
    "divine vigour",
    "divine stamina",
    "divine shield",
    "regeneration",
    "swiftness",
#if TAG_MAJOR_VERSION == 32
    "stonemail",
#endif
    "controlled flight",
    "teleport",
    "control teleport",
    "breath weapon",
    "transformation",
    "death channel",
    "deflect missiles",
    "phase shift",
    "see invisible",
    "weapon brand",
    "demonic guardian",
    "pbd",
    "silence",
    "condensation shield",
    "stoneskin",
    "gourmand",
    "bargain",
    "insulation",
    "resist poison",
    "resist fire",
    "resist cold",
    "slaying",
    "stealth",
    "magic shield",
    "sleep",
    "sage",
    "telepathy",
    "petrified",
    "lowered mr",
    "repel stairs move",
    "repel stairs climb",
    "coloured smoke trail",
    "slimify",
    "time step",
    "icemail depleted",
    "misled",
    "quad damage",
    "afraid",
    "mirror damage",
    "scrying",
    "tornado",
    "liquefying",
    "heroism",
    "finesse",
    "lifesaving",
    "paralysis immunity",
    "darkness",
    "petrifying",
    "shrouded",
    "tornado cooldown",
};

void wizard_edit_durations(void)
{
    COMPILE_CHECK(ARRAYSZ(dur_names) == NUM_DURATIONS);
    std::vector<int> durs;
    size_t max_len = 0;

    for (int i = 0; i < NUM_DURATIONS; ++i)
    {
        if (!you.duration[i])
            continue;

        max_len = std::max(strlen(dur_names[i]), max_len);
        durs.push_back(i);
    }

    if (!durs.empty())
    {
        for (unsigned int i = 0; i < durs.size(); ++i)
        {
            int dur = durs[i];
            mprf(MSGCH_PROMPT, "%c) %-*s : %d", 'a' + i, (int)max_len,
                 dur_names[dur], you.duration[dur]);
        }
        mpr("", MSGCH_PROMPT);
        mpr("Edit which duration (letter or name)? ", MSGCH_PROMPT);
    }
    else
        mpr("Edit which duration (name)? ", MSGCH_PROMPT);

    char buf[80];

    if (cancelable_get_line_autohist(buf, sizeof buf) || !*buf)
    {
        canned_msg(MSG_OK);
        return;
    }

    if (!strlcpy(buf, lowercase_string(trimmed_string(buf)).c_str(), sizeof(buf)))
    {
        canned_msg(MSG_OK);
        return;
    }

    int choice = -1;

    if (strlen(buf) == 1)
    {
        if (durs.empty())
        {
            mpr("No existing durations to choose from.", MSGCH_PROMPT);
            return;
        }
        choice = buf[0] - 'a';

        if (choice < 0 || choice >= (int) durs.size())
        {
            mpr("Invalid choice.", MSGCH_PROMPT);
            return;
        }
        choice = durs[choice];
    }
    else
    {
        std::vector<int>         matches;
        std::vector<std::string> match_names;
        max_len = 0;

        for (int i = 0; i < NUM_DURATIONS; ++i)
        {
            if (strcmp(dur_names[i], buf) == 0)
            {
                choice = i;
                break;
            }
            if (strstr(dur_names[i], buf) != NULL)
            {
                matches.push_back(i);
                match_names.push_back(dur_names[i]);
            }
        }
        if (choice != -1)
            ;
        else if (matches.size() == 1)
            choice = matches[0];
        else if (matches.empty())
        {
            mprf(MSGCH_PROMPT, "No durations matching '%s'.", buf);
            return;
        }
        else
        {
            std::string prefix = "No exact match for duration '";
            prefix += buf;
            prefix += "', possible matches are: ";

            mpr_comma_separated_list(prefix, match_names, " and ", ", ",
                                     MSGCH_DIAGNOSTICS);
            return;
        }
    }

    snprintf(buf, sizeof(buf), "Set '%s' to: ", dur_names[choice]);
    int num = prompt_for_int(buf, false);

    if (num == 0)
    {
        mpr("Can't set duration directly to 0, setting it to 1 instead.",
            MSGCH_PROMPT);
        num = 1;
    }
    you.duration[choice] = num;
}

static void debug_uptick_xl(int newxl)
{
    you.experience = exp_needed(newxl);
    level_change(true);
}

static void debug_downtick_xl(int newxl)
{
    you.hp = you.hp_max;
    you.hp_max_perm += 1000; // boost maxhp so we don't die if heavily rotted
    you.experience = exp_needed(newxl);
    level_change();

    // restore maxhp loss
    you.hp_max_perm -= 1000;
    calc_hp();
    if (you.hp_max <= 0)
    {
        // ... but remove it completely if unviable
        you.hp_max_temp = std::max(you.hp_max_temp, 0);
        you.hp_max_perm = std::max(you.hp_max_perm, 0);
        calc_hp();
    }

    you.hp       = std::max(1, you.hp);
}

void wizard_set_xl()
{
    mprf(MSGCH_PROMPT, "Enter new experience level: ");
    char buf[30];
    if (cancelable_get_line_autohist(buf, sizeof buf))
    {
        canned_msg(MSG_OK);
        return;
    }

    const int newxl = atoi(buf);
    if (newxl < 1 || newxl > 27 || newxl == you.experience_level)
    {
        canned_msg(MSG_OK);
        return;
    }

    no_messages mx;
    if (newxl < you.experience_level)
        debug_downtick_xl(newxl);
    else
        debug_uptick_xl(newxl);
}

void wizard_get_god_gift (void)
{
    if (you.religion == GOD_NO_GOD)
    {
        mpr("You are not religious!");
        return;
    }

    if (!do_god_gift(true))
        mpr("Nothing happens.");
}

void wizard_toggle_xray_vision()
{
    you.xray_vision = !you.xray_vision;
    viewwindow(true);
}

void wizard_god_wrath()
{
    if (you.religion == GOD_NO_GOD)
    {
        mpr("You suffer the terrible wrath of No God.");
        return;
    }

    if (!divine_retribution(you.religion, true, true))
        // Currently only dead Jiyva.
        mpr("You're not eligible for wrath.");
}

void wizard_god_mollify()
{
    for (int i = GOD_NO_GOD; i < NUM_GODS; ++i)
    {
        if (you.penance[i])
            dec_penance((god_type) i, you.penance[i]);
    }
}
