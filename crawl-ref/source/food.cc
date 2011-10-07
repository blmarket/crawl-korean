/**
 * @file
 * @brief Functions for eating and butchering.
**/

#include "AppHdr.h"

#include "food.h"

#include <sstream>

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "externs.h"
#include "options.h"

#include "artefact.h"
#include "cio.h"
#include "clua.h"
#include "command.h"
#include "debug.h"
#include "delay.h"
#include "effects.h"
#include "env.h"
#include "hints.h"
#include "invent.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "macro.h"
#include "mgen_data.h"
#include "message.h"
#include "misc.h"
#include "mon-place.h"
#include "mon-util.h"
#include "mutation.h"
#include "output.h"
#include "player.h"
#include "player-equip.h"
#include "player-stats.h"
#include "potion.h"
#include "random.h"
#include "religion.h"
#include "godconduct.h"
#include "skills2.h"
#include "state.h"
#include "stuff.h"
#include "transform.h"
#include "travel.h"
#include "xom.h"

static corpse_effect_type _determine_chunk_effect(corpse_effect_type chunktype,
                                                  bool rotten_chunk);
static void _eat_chunk(corpse_effect_type chunk_effect, bool cannibal,
                       int mon_intel, bool holy, bool rotten);
static void _eat_permafood(int item_type);
static void _describe_food_change(int hunger_increment);
static bool _vampire_consume_corpse(int slot, bool invent);
static void _heal_from_food(int hp_amt, int mp_amt = 0, bool unrot = false,
                            bool restore_str = false);

/*
 *  BEGIN PUBLIC FUNCTIONS
 */

void make_hungry(int hunger_amount, bool suppress_msg,
                 bool allow_reducing)
{
    if (crawl_state.disables[DIS_HUNGER])
        return;

    if (crawl_state.game_is_zotdef() && you.species == SP_SPRIGGAN)
    {
        you.hunger = 6000;
        you.hunger_state = HS_SATIATED;
        return;
    }

    if (you.is_undead == US_UNDEAD)
        return;

    if (allow_reducing)
        hunger_amount = calc_hunger(hunger_amount);

    if (hunger_amount == 0 && !suppress_msg)
        return;

#ifdef DEBUG_DIAGNOSTICS
    set_redraw_status(REDRAW_HUNGER);
#endif

    you.hunger -= hunger_amount;

    if (you.hunger < 0)
        you.hunger = 0;

    // So we don't get two messages, ever.
    bool state_message = food_change();

    if (!suppress_msg && !state_message)
        _describe_food_change(-hunger_amount);
}

void lessen_hunger(int satiated_amount, bool suppress_msg)
{
    if (you.is_undead == US_UNDEAD)
        return;

    you.hunger += satiated_amount;

    if (you.hunger > HUNGER_ENGORGED)
        you.hunger = HUNGER_ENGORGED;

    // So we don't get two messages, ever.
    bool state_message = food_change();

    if (!suppress_msg && !state_message)
        _describe_food_change(satiated_amount);
}

void set_hunger(int new_hunger_level, bool suppress_msg)
{
    if (you.is_undead == US_UNDEAD)
        return;

    int hunger_difference = (new_hunger_level - you.hunger);

    if (hunger_difference < 0)
        make_hungry(-hunger_difference, suppress_msg);
    else if (hunger_difference > 0)
        lessen_hunger(hunger_difference, suppress_msg);
}

/**
 * More of a "weapon_switch back from butchering" function, switching
 * to a weapon is done using the wield_weapon() code.
 * Special cases like staves of power or other special weps are taken
 * care of by calling wield_effects().    {gdl}
 */
void weapon_switch(int targ)
{
    // Give the player an option to abort.
    if (you.weapon() && !check_old_item_warning(*you.weapon(), OPER_WIELD))
        return;

    if (targ == -1) // Unarmed Combat.
    {
        // Already unarmed?
        if (!you.weapon())
            return;

        mpr(gettext("You switch back to your bare hands."));
    }
    else
    {
        // Possibly not valid anymore (dropped, etc.).
        if (!you.inv[targ].defined())
            return;

        // Already wielding this weapon?
        if (you.equip[EQ_WEAPON] == you.inv[targ].link)
            return;

        if (!can_wield(&you.inv[targ]))
        {
            mprf(gettext("Not switching back to %s."),
                 you.inv[targ].name(true, DESC_INVENTORY).c_str());
            return;
        }
    }

    // Unwield the old weapon and wield the new.
    // XXX This is a pretty dangerous hack;  I don't like it.--GDL
    //
    // Well yeah, but that's because interacting with the wielding
    // code is a mess... this whole function's purpose was to
    // isolate this hack until there's a proper way to do things. -- bwr
    if (you.weapon())
        unwield_item(false);

    if (targ != -1)
        equip_item(EQ_WEAPON, targ, false);

    if (Options.chunks_autopickup || you.species == SP_VAMPIRE)
        autopickup();

    // Same amount of time as normal wielding.
    // FIXME: this duplicated code is begging for a bug.
    if (you.weapon())
        you.time_taken /= 2;
    else                        // Swapping to empty hands is faster.
    {
        you.time_taken *= 3;
        you.time_taken /= 10;
    }

    you.wield_change = true;
    you.turn_is_over = true;
}

static bool _prepare_butchery(bool can_butcher, bool removed_gloves,
                              bool wpn_switch)
{
    // No preparation necessary.
    if (can_butcher)
        return (true);

    // At least one of these has to be true, else what are we doing
    // here?
    ASSERT(removed_gloves || wpn_switch);

    // If you can butcher by taking off your gloves, don't prompt.
    if (removed_gloves)
    {
        // Actually take off the gloves; this creates a delay.  We
        // assume later on that gloves have a 1-turn takeoff delay!
        if (!takeoff_armour(you.equip[EQ_GLOVES]))
            return (false);

        // Ensure that the gloves are taken off by now by finishing the
        // DELAY_ARMOUR_OFF delay started by takeoff_armour() above.
        // FIXME: get rid of this hack!
        finish_last_delay();
    }

    if (wpn_switch
        && !wield_weapon(true, SLOT_BARE_HANDS, false, true, false, false))
    {
        return (false);
    }

    // Switched to a good butchering tool.
    return (true);
}

static bool _butcher_corpse(int corpse_id, int butcher_tool,
                            bool first_corpse = true,
                            bool bottle_blood = false)
{
    ASSERT(corpse_id != -1);

    const bool rotten = food_is_rotten(mitm[corpse_id]);

    if (is_forbidden_food(mitm[corpse_id])
        && !yesno(gettext("Desecrating this corpse would be a sin. Continue anyway?"),
                  false, 'n'))
    {
        return false;
    }
    else if (!bottle_blood && you.species == SP_VAMPIRE
             && !you.has_spell(SPELL_SUBLIMATION_OF_BLOOD)
             && !you.has_spell(SPELL_SIMULACRUM)
             && !yesno(gettext("You'd want to drain or bottle this corpse instead. "
                       "Continue anyway?"), true, 'n'))
    {
        return false;
    }

    // Start work on the first corpse we butcher.
    if (first_corpse)
        mitm[corpse_id].plus2++;

    int work_req = std::max(0, 4 - mitm[corpse_id].plus2);

    delay_type dtype = DELAY_BUTCHER;
    // Sanity checks.
    if (bottle_blood && !rotten
        && can_bottle_blood_from_corpse(mitm[corpse_id].plus))
    {
        dtype = DELAY_BOTTLE_BLOOD;
    }

    start_delay(dtype, work_req, corpse_id, mitm[corpse_id].special,
                butcher_tool);

    you.turn_is_over = true;
    return (true);
}

static void _terminate_butchery(bool wpn_switch, bool removed_gloves,
                                int old_weapon, int old_gloves)
{
    // Switch weapon back.
    if (wpn_switch && you.equip[EQ_WEAPON] != old_weapon
        && (!you.weapon() || !you.weapon()->cursed()))
    {
        start_delay(DELAY_WEAPON_SWAP, 1, old_weapon);
    }

    // Put on the removed gloves.
    if (removed_gloves && you.equip[EQ_GLOVES] != old_gloves)
        start_delay(DELAY_ARMOUR_ON, 1, old_gloves, 1);
}

int count_corpses_in_pack(bool blood_only)
{
    int num = 0;
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        item_def &obj(you.inv[i]);

        if (!obj.defined())
            continue;

        // Only actually count corpses, not skeletons.
        if (obj.base_type != OBJ_CORPSES || obj.sub_type != CORPSE_BODY)
            continue;

        // Only saprovorous characters care about rotten food.
        if (food_is_rotten(obj) && (blood_only
                                    || !player_mutation_level(MUT_SAPROVOROUS)))
        {
            continue;
        }

        if (!blood_only || mons_has_blood(obj.plus))
            num++;
    }

    return num;
}

static bool _have_corpses_in_pack(bool remind, bool bottle_blood = false)
{
    const int num = count_corpses_in_pack(bottle_blood);

    if (num == 0)
        return (false);

    std::string verb = (bottle_blood ? pgettext("food","bottle") : pgettext("food","butcher"));

    std::string noun, pronoun;
    if (num == 1)
    {
        noun    = pgettext("food","corpse");
        pronoun = pgettext("food","it");
    }
    else
    {
        noun    = pgettext("food","corpses");
        pronoun = pgettext("food","them");
    }

    if (remind)
    {
        mprf(gettext("You might want to also %s the %s in your pack."), verb.c_str(),
             noun.c_str());
    }
    else
    {
        mprf(gettext("If you dropped the %s in your pack you could %s %s."),
             noun.c_str(), verb.c_str(), pronoun.c_str());
    }

    return (true);
}

bool butchery(int which_corpse, bool bottle_blood)
{
    if (you.visible_igrd(you.pos()) == NON_ITEM)
    {
        if (!_have_corpses_in_pack(false))
            mpr(gettext("There isn't anything here!"));
        return (false);
    }

    if (!player_can_reach_floor())
        return (false);

    // Vampires' fangs are optimised for biting, not for tearing flesh.
    // (Not that they really need to.) Other species with this mutation
    // might still benefit from it.
    bool teeth_butcher    = (you.has_usable_fangs() == 3
                             && you.species != SP_VAMPIRE);

    bool birdie_butcher   = (player_mutation_level(MUT_BEAK)
                             && player_mutation_level(MUT_TALONS));

    bool barehand_butcher = (form_can_butcher_barehanded(you.form)
                                 || you.has_claws())
                             && !player_wearing_slot(EQ_GLOVES);

    bool gloved_butcher   = (you.has_claws() && player_wearing_slot(EQ_GLOVES)
                             && !you.inv[you.equip[EQ_GLOVES]].cursed());

    bool knife_butcher    = !barehand_butcher && !you.weapon() && form_can_wield();

    bool can_butcher      = (teeth_butcher || barehand_butcher
                             || birdie_butcher || knife_butcher
                             || you.weapon() && can_cut_meat(*you.weapon()));

    if (!Options.easy_butcher && !can_butcher)
    {
        mpr(gettext("Maybe you should try using a sharper implement."));
        return (false);
    }

    // It makes more sense that you first find out if there's anything
    // to butcher, *then* decide to actually butcher it.
    // The old code did it the other way.
    if (!can_butcher && you.berserk())
    {
        mpr(gettext("You are too berserk to search for a butchering tool!"));
        return (false);
    }

    // First determine how many things there are to butcher.
    int num_corpses = 0;
    int corpse_id   = -1;
    bool prechosen  = (which_corpse != -1);
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY)
        {
            if (bottle_blood && (food_is_rotten(*si)
                                 || !can_bottle_blood_from_corpse(si->plus)))
            {
                continue;
            }

            corpse_id = si->index();
            num_corpses++;

            // Return pre-chosen corpse if it exists.
            if (prechosen && corpse_id == which_corpse)
                break;
        }
    }

    if (num_corpses == 0)
    {
        if (!_have_corpses_in_pack(false, bottle_blood))
        {
            mprf(gettext("There isn't anything to %s here."),
                 bottle_blood ? pgettext("food","bottle") : pgettext("food","butcher"));
        }
        return (false);
    }

    int old_weapon      = you.equip[EQ_WEAPON];
    int old_gloves      = you.equip[EQ_GLOVES];

    bool wpn_switch     = false;
    bool removed_gloves = false;

    if (!can_butcher)
    {
        if (you.weapon() && you.weapon()->cursed() && gloved_butcher)
            removed_gloves = true;
        else
            wpn_switch = true;
    }

    int butcher_tool;

    if (barehand_butcher || removed_gloves)
        butcher_tool = SLOT_CLAWS;
    else if (teeth_butcher)
        butcher_tool = SLOT_TEETH;
    else if (birdie_butcher)
        butcher_tool = SLOT_BIRDIE;
    else if (wpn_switch || knife_butcher)
        butcher_tool = SLOT_BUTCHERING_KNIFE;
    else
        butcher_tool = you.weapon()->link;

    // Butcher pre-chosen corpse, if found, or if there is only one corpse.
    bool success = false;
    if (prechosen && corpse_id == which_corpse
        || num_corpses == 1 && !Options.always_confirm_butcher)
    {
        if (!_prepare_butchery(can_butcher, removed_gloves, wpn_switch))
            return (false);

        success = _butcher_corpse(corpse_id, butcher_tool, true, bottle_blood);
        _terminate_butchery(wpn_switch, removed_gloves, old_weapon, old_gloves);

        // Remind player of corpses in pack that could be butchered or
        // bottled.
        _have_corpses_in_pack(true, bottle_blood);
        return (success);
    }

    // Now pick what you want to butcher. This is only a problem
    // if there are several corpses on the square.
    bool butcher_all   = false;
    bool repeat_prompt = false;
    bool did_weap_swap = false;
    bool first_corpse  = true;
    int keyin;
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (si->base_type != OBJ_CORPSES || si->sub_type != CORPSE_BODY)
            continue;

        if (bottle_blood && (food_is_rotten(*si)
                             || !can_bottle_blood_from_corpse(si->plus)))
        {
            continue;
        }

        if (butcher_all)
            corpse_id = si->index();
        else
        {
            corpse_id = -1;

            std::string corpse_name = si->name(true, DESC_NOCAP_A);

            // We don't need to check for undead because
            // * Mummies can't eat.
            // * Ghouls relish the bad things.
            // * Vampires won't bottle bad corpses.
            if (!you.is_undead)
                corpse_name = get_menu_colour_prefix_tags(*si, DESC_NOCAP_A);

            // Shall we butcher this corpse?
            do
            {
                mprf(MSGCH_PROMPT, gettext("%s %s? [(y)es/(c)hop/(n)o/(a)ll/(q)uit/?]"),
                     bottle_blood ? pgettext("food","Bottle") : pgettext("food","Butcher"),
                     corpse_name.c_str());
                repeat_prompt = false;

                keyin = tolower(getchm(KMC_CONFIRM));
                switch (keyin)
                {
                case 'y':
                case 'c':
                case 'd':
                case 'a':
                    if (!did_weap_swap)
                    {
                        if (_prepare_butchery(can_butcher, removed_gloves,
                                              wpn_switch))
                        {
                            did_weap_swap = true;
                        }
                        else
                            return (false);
                    }
                    corpse_id = si->index();

                    if (keyin == 'a')
                        butcher_all = true;
                    break;

                case 'q':
                CASE_ESCAPE
                    canned_msg(MSG_OK);
                    _terminate_butchery(wpn_switch, removed_gloves, old_weapon,
                                        old_gloves);
                    return (false);

                case '?':
                    show_butchering_help();
                    mesclr();
                    redraw_screen();
                    repeat_prompt = true;
                    break;

                default:
                    break;
                }
            }
            while (repeat_prompt);
        }

        if (corpse_id != -1)
        {
            if (_butcher_corpse(corpse_id, butcher_tool, first_corpse,
                                bottle_blood))
            {
                success = true;
                first_corpse = false;
            }
        }
    }

    if (!butcher_all && corpse_id == -1)
    {
        mprf(gettext("There isn't anything else to %s here."),
             bottle_blood ? pgettext("food","bottle") : pgettext("food","butcher"));
    }
    _terminate_butchery(wpn_switch, removed_gloves, old_weapon, old_gloves);

    if (success)
    {
        // Remind player of corpses in pack that could be butchered or
        // bottled.
        _have_corpses_in_pack(true, bottle_blood);
    }

    return (success);
}

bool prompt_eat_inventory_item(int slot)
{
    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return (false);
    }

    int which_inventory_slot = slot;

    if (slot == -1)
    {
        which_inventory_slot =
                prompt_invent_item(you.species == SP_VAMPIRE ? pgettext("food","Drain what?")
                                                             : pgettext("food","Eat which item?"),
                                   MT_INVLIST,
                                   you.species == SP_VAMPIRE ? (int)OSEL_VAMP_EAT
                                                             : OBJ_FOOD,
                                   true, true, true, 0, -1, NULL,
                                   OPER_EAT);

        if (prompt_failed(which_inventory_slot))
            return (false);
    }

    // This conditional can later be merged into food::can_ingest() when
    // expanded to handle more than just OBJ_FOOD 16mar200 {dlb}
    if (you.species != SP_VAMPIRE)
    {
        if (you.inv[which_inventory_slot].base_type != OBJ_FOOD)
        {
            mpr(gettext("You can't eat that!"));
            return (false);
        }
    }
    else
    {
        if (you.inv[which_inventory_slot].base_type != OBJ_CORPSES
            || you.inv[which_inventory_slot].sub_type != CORPSE_BODY)
        {
            mpr(gettext("You crave blood!"));
            return (false);
        }
    }

    if (!can_ingest(you.inv[which_inventory_slot], false))
        return (false);

    eat_inventory_item(which_inventory_slot);

    burden_change();
    you.turn_is_over = true;

    return (true);
}

static bool _eat_check(bool check_hunger = true, bool silent = false)
{
    if (you.is_undead == US_UNDEAD)
    {
        if (!silent)
        {
            mpr(gettext("You can't eat."));
            crawl_state.zero_turns_taken();
        }
        return (false);
    }

    if (check_hunger && you.hunger >= 11000)
    {
        if (!silent)
        {
            mprf(gettext("You're too full to %s anything."),
                 you.species == SP_VAMPIRE ? pgettext("food","drain") : pgettext("food","eat"));
            crawl_state.zero_turns_taken();
        }
        return (false);
    }
    return (true);
}

// [ds] Returns true if something was eaten.
bool eat_food(int slot)
{
    if (!_eat_check())
        return (false);

    // Skip the prompts if we already know what we're eating.
    if (slot == -1)
    {
        int result = prompt_eat_chunks();
        if (result == 1 || result == -1)
            return (result > 0);

        if (result != -2) // else skip ahead to inventory
        {
            if (you.visible_igrd(you.pos()) != NON_ITEM)
            {
                result = eat_from_floor(true);
                if (result == 1)
                    return (true);
                if (result == -1)
                    return (false);
            }
        }
    }

    return (prompt_eat_inventory_item(slot));
}

//     END PUBLIC FUNCTIONS

static int _maximum_satiation(int preference)
{
    if (preference < 0)
        return 0; // cannot ever eat

    switch (preference)
    {
    case 0:  return HS_NEAR_STARVING;
    case 1:  return HS_VERY_HUNGRY;
    case 2:  return HS_HUNGRY;
    case 3:  return HS_SATIATED;
    case 4:  return HS_FULL;
    default: return HS_VERY_FULL;
    }
}

static float _chunk_satiation_multiplier(int preference)
{
    if (preference < 0)
        return 0;            // 3 levels incompatible

    switch (preference)
    {
    case 0:  return (1.0 / 3.0);  // 2 levels incompatible
    case 1:  return (2.0 / 3.0);  // 1 level incompatible
    case 2:  return 1;            // basic case
    case 3:  return 1.1;          // 1 level compatible
    case 4:  return 1.2;          // 2 levels compatible
    default: return 1.5;          // 3 levels compatible
    }
}

static float _permafood_satiation_multiplier(int preference)
{
    if (preference < 2)
        return 0;           // 3 levels incompatible

    switch (preference)
    {
    case 2:  return (1.0 / 3.0); // 2 levels incompatible
    case 3:  return (2.0 / 3.0); // 1 level incompatible
    case 4:  return 1;           // basic case
    case 5:  return (4.0 / 3.0); // 1 level compatible
    case 6:  return (5.0 / 3.0); // 2 levels compatible
    default: return 2;           // 3 levels compatible
    }
}

// Returns 1 for herbivores, -1 for carnivores and 0 for either.
static int _plant_content(int type)
{
    switch (type)
    {
    case FOOD_BREAD_RATION:
    case FOOD_PEAR:
    case FOOD_APPLE:
    case FOOD_CHOKO:
    case FOOD_SNOZZCUMBER:
    case FOOD_APRICOT:
    case FOOD_ORANGE:
    case FOOD_BANANA:
    case FOOD_STRAWBERRY:
    case FOOD_RAMBUTAN:
    case FOOD_LEMON:
    case FOOD_GRAPE:
    case FOOD_SULTANA:
    case FOOD_LYCHEE:
        return 1;

    case FOOD_CHUNK:
    case FOOD_MEAT_RATION:
    case FOOD_SAUSAGE:
    case FOOD_BEEF_JERKY:
        return -1;

    case FOOD_HONEYCOMB:
    case FOOD_ROYAL_JELLY:
    case FOOD_AMBROSIA:
    case FOOD_CHEESE:
    case FOOD_PIZZA:
        return 0;

    case NUM_FOODS:
        mpr("Bad food type", MSGCH_ERROR);
        return 0;
    }

    mprf(MSGCH_ERROR, "Couldn't handle food type: %d", type);
    return 0;
}

static bool _player_has_enough_food()
{
    int food_value = 0;
    item_def item;
    for (unsigned slot = 0; slot < ENDOFPACK; ++slot)
    {
        item = you.inv[slot];
        if (!item.defined())
            continue;

        if (!can_ingest(item, true, true, false))
            continue;

        if (food_is_rotten(item) && !player_mutation_level(MUT_SAPROVOROUS))
            continue;

        if (is_bad_food(item))
            continue;

        // Vampires can only drain corpses.
        if (you.species == SP_VAMPIRE)
            food_value += 3;
        else
        {
            if (item.base_type != OBJ_FOOD)
                continue;

            switch (item.sub_type)
            {
            case FOOD_CHUNK:
                if (!player_mutation_level(MUT_HERBIVOROUS))
                    food_value += 2 * item.quantity;
                break;
            case FOOD_MEAT_RATION:
                if (!player_mutation_level(MUT_HERBIVOROUS))
                    food_value += 3 * item.quantity;
                break;
            case FOOD_BREAD_RATION:
                if (!player_mutation_level(MUT_CARNIVOROUS))
                    food_value += 3 * item.quantity;
                break;
            default:
                // Only count snacks if we really like them
                if (is_preferred_food(item))
                    food_value += item.quantity;
                break;
            }
        }
    }

    // You have "enough" food if you have, e.g.
    //  1 meat ration + 1 chunk, or 2 chunks for carnivores, or
    //  5 items of fruit, or 1 bread ration and 2 fruit items as a herbivore.
    return (food_value > 5);
}

static std::string _how_hungry()
{
    if (you.hunger_state > HS_SATIATED)
        return (pgettext("food","full"));
    else if (you.species == SP_VAMPIRE)
        return (pgettext("food","thirsty"));
    return (pgettext("food","hungry"));
}

bool food_change(bool suppress_message)
{
    uint8_t newstate = HS_ENGORGED;
    bool state_changed = false;
    bool less_hungry   = false;

    you.hunger = std::max(you_min_hunger(), you.hunger);
    you.hunger = std::min(you_max_hunger(), you.hunger);

    // Get new hunger state.
    if (you.hunger <= HUNGER_STARVING)
        newstate = HS_STARVING;
    else if (you.hunger <= HUNGER_NEAR_STARVING)
        newstate = HS_NEAR_STARVING;
    else if (you.hunger <= HUNGER_VERY_HUNGRY)
        newstate = HS_VERY_HUNGRY;
    else if (you.hunger <= HUNGER_HUNGRY)
        newstate = HS_HUNGRY;
    else if (you.hunger < HUNGER_SATIATED)
        newstate = HS_SATIATED;
    else if (you.hunger < HUNGER_FULL)
        newstate = HS_FULL;
    else if (you.hunger < HUNGER_VERY_FULL)
        newstate = HS_VERY_FULL;

    if (newstate != you.hunger_state)
    {
        state_changed = true;
        if (newstate > you.hunger_state)
            less_hungry = true;

        you.hunger_state = newstate;
        set_redraw_status(REDRAW_HUNGER);

        if (newstate < HS_SATIATED)
            interrupt_activity(AI_HUNGRY);

        if (you.species == SP_VAMPIRE)
        {
            if (newstate <= HS_SATIATED)
            {
                if (you.duration[DUR_BERSERK] > 1)
                {
                    mpr(gettext("Your blood-deprived body can't sustain your rage any "
                        "longer."), MSGCH_DURATION);
                    you.duration[DUR_BERSERK] = 1;
                }
                if (you.form != TRAN_NONE && you.form != TRAN_BAT
                    && you.duration[DUR_TRANSFORMATION] > 2 * BASELINE_DELAY)
                {
                    mpr(gettext("Your blood-deprived body can't sustain your "
                        "transformation much longer."), MSGCH_DURATION);
                    you.set_duration(DUR_TRANSFORMATION, 2);
                }
            }
            else if (player_in_bat_form()
                     && you.duration[DUR_TRANSFORMATION] > 5)
            {
                print_stats();
                mpr(gettext("Your blood-filled body can't sustain your transformation "
                    "much longer."), MSGCH_WARN);

                // Give more time because suddenly stopping flying can be fatal.
                you.set_duration(DUR_TRANSFORMATION, 5);
            }
            else if (newstate == HS_ENGORGED && is_vampire_feeding()) // Alive
            {
                print_stats();
                mpr(gettext("You can't stomach any more blood right now."));
            }
        }

        if (!suppress_message)
        {
            std::string msg = pgettext("food","You ");
            switch (you.hunger_state)
            {
            case HS_STARVING:
                if (you.species == SP_VAMPIRE)
                    msg += gettext("feel devoid of blood!");
                else
                    msg += gettext("are starving!");

                mprf(MSGCH_FOOD, less_hungry, "%s", msg.c_str());

                // Xom thinks this is funny if you're in a labyrinth
                // and are low on food.
                if (you.level_type == LEVEL_LABYRINTH
                    && !_player_has_enough_food())
                {
                    xom_is_stimulated(50);
                }

                learned_something_new(HINT_YOU_STARVING);
                you.check_awaken(500);
                break;

            case HS_NEAR_STARVING:
                if (you.species == SP_VAMPIRE)
                    msg += gettext("feel almost devoid of blood!");
                else
                    msg += gettext("are near starving!");

                mprf(MSGCH_FOOD, less_hungry, "%s", msg.c_str());

                learned_something_new(HINT_YOU_HUNGRY);
                break;

            case HS_VERY_HUNGRY:
            case HS_HUNGRY:
                msg += gettext("are feeling ");
                if (you.hunger_state == HS_VERY_HUNGRY)
                    msg += pgettext("food","very ");
                msg += _how_hungry();
                msg += pgettext("food",".");

                mprf(MSGCH_FOOD, less_hungry, "%s", msg.c_str());

                learned_something_new(HINT_YOU_HUNGRY);
                break;

            default:
                return (state_changed);
            }
        }
    }

    return (state_changed);
}

// food_increment is positive for eating, negative for hungering
static void _describe_food_change(int food_increment)
{
    int magnitude = (food_increment > 0)?food_increment:(-food_increment);
    std::string msg;

    if (magnitude == 0)
        return;

    msg = pgettext("food","You feel ");

    if (magnitude <= 100)
        msg += pgettext("food","slightly ");
    else if (magnitude <= 350)
        msg += pgettext("food","somewhat ");
    else if (magnitude <= 800)
        msg += pgettext("food","quite a bit ");
    else
        msg += pgettext("food","a lot ");

    msg += _how_hungry().c_str();
    if ((you.hunger_state > HS_SATIATED) ^ (food_increment < 0))
        msg += pgettext("food","more ");
    else
        msg += pgettext("food","less ");

    msg += pgettext("food_change",".");
    mpr(msg.c_str());
}

// Should really be merged into function below. -- FIXME
void eat_inventory_item(int which_inventory_slot)
{
    item_def& food(you.inv[which_inventory_slot]);
    if (food.base_type == OBJ_CORPSES && food.sub_type == CORPSE_BODY)
    {
        _vampire_consume_corpse(which_inventory_slot, true);
        you.turn_is_over = true;
        return;
    }

    if (food.base_type != OBJ_FOOD)
        return;

    if (food.sub_type == FOOD_CHUNK)
    {
        const int mons_type  = food.plus;
        const bool cannibal  = is_player_same_species(mons_type);
        const bool holy      = (mons_class_holiness(mons_type) == MH_HOLY);
        const int intel      = mons_class_intel(mons_type) - I_ANIMAL;
        const bool rotten    = food_is_rotten(food);
        const corpse_effect_type chunk_type = mons_corpse_effect(mons_type);

        if (!can_ingest(food, true))
            return;

        _eat_chunk(chunk_type, cannibal, intel, holy, rotten);
    }
    else
        _eat_permafood(food.sub_type);

    you.turn_is_over = true;
    dec_inv_item_quantity(which_inventory_slot, 1);
}

void eat_floor_item(int item_link)
{
    item_def& food(mitm[item_link]);
    if (food.base_type == OBJ_CORPSES && food.sub_type == CORPSE_BODY)
    {
        if (you.species != SP_VAMPIRE)
            return;

        if (_vampire_consume_corpse(item_link, false))
            you.turn_is_over = true;

        return;
    }

    if (food.base_type != OBJ_FOOD)
        return;

    if (food.sub_type == FOOD_CHUNK)
    {
        const int intel      = mons_class_intel(food.plus) - I_ANIMAL;
        const bool cannibal  = is_player_same_species(food.plus);
        const bool rotten    = food_is_rotten(food);
        const bool holy      = (mons_class_holiness(food.plus) == MH_HOLY);
        const corpse_effect_type chunk_type = mons_corpse_effect(food.plus);

        if (!can_ingest(food, true))
            return;

        _eat_chunk(chunk_type, cannibal, intel, holy, rotten);
    }
    else
        _eat_permafood(food.sub_type);

    you.turn_is_over = true;

    dec_mitm_item_quantity(item_link, 1);
}

static int _food_preference(int type,
                            bool nutrition = false,
                            corpse_effect_type effect_type = CE_CLEAN,
                            bool is_rotten = false)
{
    int preference = 2;
    const int saprovorous = player_mutation_level(MUT_SAPROVOROUS);
    const int how_herbivorous = player_mutation_level(MUT_HERBIVOROUS);
    const int how_carnivorous = player_mutation_level(MUT_CARNIVOROUS);

    const int plant_content =  _plant_content(type);

    // bonus/penalty of your mutation level for matching/conflicting food type
    preference += how_herbivorous * plant_content;
    preference -= how_carnivorous * plant_content;

    // Royal Jelly can always be eaten. It needs a total of +3 to guarantee this
    if (type == FOOD_ROYAL_JELLY)
        preference += 1;

    if (type == FOOD_CHUNK)
    {
        // unclean chunks get a penalty (If you are immune to the effect, clean
        // is passed even if the original chunk wasn't clean [except rotting])
        switch (effect_type)
        {
        case CE_ROT:
            preference -= 3;
            break;
        case CE_POISON_CONTAM:
            if (!you.res_poison())
                preference -= 1;
            // fall through
        case CE_CONTAMINATED:
            if (saprovorous == 3 && !is_rotten)
                preference += 1;
            if (!saprovorous)
                preference -=1;
            break;
        case CE_POISONOUS:
            if (!you.res_poison())
                preference -=1;
            break;
        case CE_MUTAGEN_RANDOM:
        case CE_MUTAGEN_GOOD:
        case CE_MUTAGEN_BAD:
            preference -= 1;
            break;
        case CE_ROTTEN:
            is_rotten = true;
            break;
        default:
            ;
        }

        // handle rotten food
        if (is_rotten)
            preference -= 2 - saprovorous;

        if (!nutrition && you.mutation[MUT_GOURMAND])
            preference += 3;
        // gourmand - (only affects chunks)
        else if (you.duration[DUR_GOURMAND] >= GOURMAND_MAX)
            preference++;
    }
    else
    {
        // permafood gets a +2 bonus for being delicious
        preference += 2;
    }

    return preference;
}

static int _food_preference(const item_def &food, bool nutrition = false)
{
    if (food.sub_type != FOOD_CHUNK)
        return _food_preference(food.sub_type, nutrition);

    return _food_preference(FOOD_CHUNK, nutrition,
                            mons_corpse_effect(food.plus),
                            food_is_rotten(food));
}

// Returns which of two food items is older (true for first, else false).
class compare_by_freshness
{
public:
    bool operator()(item_def *food1, item_def *food2)
    {
        ASSERT(food1->base_type == OBJ_CORPSES || food1->base_type == OBJ_FOOD);
        ASSERT(food2->base_type == OBJ_CORPSES || food2->base_type == OBJ_FOOD);
        ASSERT(food1->base_type == food2->base_type);

        if (is_inedible(*food1))
            return (false);

        if (is_inedible(*food2))
            return (true);

        if (food1->base_type == OBJ_FOOD)
        {
            // Prefer chunks to non-chunks. (Herbivores handled above.)
            if (food1->sub_type != FOOD_CHUNK && food2->sub_type == FOOD_CHUNK)
                return (false);
            if (food2->sub_type != FOOD_CHUNK && food1->sub_type == FOOD_CHUNK)
                return (true);
        }

        // Both food types are edible.
        if (food1->base_type == OBJ_CORPSES
            || food1->sub_type == FOOD_CHUNK && food2->sub_type == FOOD_CHUNK)
        {
            // Always offer poisonous/mutagenic chunks last.
            if (is_bad_food(*food1) && !is_bad_food(*food2))
                return (false);
            if (is_bad_food(*food2) && !is_bad_food(*food1))
                return (true);

            if (Options.prefer_safe_chunks && !you.is_undead)
            {
                // Sort by preference.
                const int pref1 = _food_preference(*food1);
                const int pref2 = _food_preference(*food2);
                if (pref1 != pref2)
                    return (pref1 > pref2);
            }
        }

        return (food1->special < food2->special);
    }
};

// Returns -1 for cancel, 1 for eaten, 0 for not eaten.
int eat_from_floor(bool skip_chunks)
{
    if (!_eat_check())
        return (false);

    if (you.flight_mode() == FL_LEVITATE)
        return (0);

    // Corpses should have been handled before.
    if (you.species == SP_VAMPIRE && skip_chunks)
        return (0);

    bool need_more = false;
    int unusable_corpse = 0;
    int inedible_food = 0;
    item_def wonteat;
    bool found_valid = false;

    std::vector<item_def *> food_items;
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (you.species == SP_VAMPIRE)
        {
            if (si->base_type != OBJ_CORPSES || si->sub_type != CORPSE_BODY)
                continue;

            if (!mons_has_blood(si->plus))
            {
                unusable_corpse++;
                continue;
            }
        }
        else
        {
            if (si->base_type != OBJ_FOOD)
                continue;

            // Chunks should have been handled before.
            if (skip_chunks && si->sub_type == FOOD_CHUNK)
                continue;

            if (is_bad_food(*si))
                continue;
        }

        if (!can_ingest(*si, true))
        {
            if (!inedible_food)
            {
                wonteat = *si;
                inedible_food++;
            }
            else
            {
                // Increase only if we're dealing with different subtypes.
                // FIXME: Use a common check for herbivorous/carnivorous
                //        dislikes, for e.g. "Blech! You need blood!"
                ASSERT(wonteat.defined());
                if (wonteat.sub_type != si->sub_type)
                    inedible_food++;
            }

            continue;
        }

        found_valid = true;
        food_items.push_back(&(*si));
    }

    if (found_valid)
    {
        std::sort(food_items.begin(), food_items.end(), compare_by_freshness());
        for (unsigned int i = 0; i < food_items.size(); ++i)
        {
            item_def *item = food_items[i];
            std::string item_name = get_menu_colour_prefix_tags(*item,
                                                                DESC_NOCAP_A);

            mprf(MSGCH_PROMPT, gettext("%s %s%s? (ye/n/q/i?)"),
                 (you.species == SP_VAMPIRE ? pgettext("food","Drink blood from") : pgettext("food","Eat")),
                 ((item->quantity > 1) ? pgettext("food","one of ") : ""),
                 item_name.c_str());

            int keyin = tolower(getchm(KMC_CONFIRM));
            switch (keyin)
            {
            case 'q':
            CASE_ESCAPE
                canned_msg(MSG_OK);
                return -1;
            case 'e':
            case 'y':
                if (!check_warning_inscriptions(*item, OPER_EAT))
                    break;

                if (can_ingest(*item, false))
                {
                    int ilink = item_on_floor(*item, you.pos());

                    if (ilink != NON_ITEM)
                    {
                        eat_floor_item(ilink);
                        return (true);
                    }
                    return (false);
                }
                need_more = true;
                break;
            case 'i':
            case '?':
                // Directly skip ahead to inventory.
                return (0);
            default:
                // Else no: try next one.
                break;
            }
        }
    }
    else
    {
        // Give a message about why these food items can not actually be eaten.
        if (unusable_corpse)
        {
            mprf(gettext("%s devoid of blood."),
                 unusable_corpse == 1 ? gettext("This corpse is") : gettext("These corpses are"));
            need_more = true;
        }
        else if (inedible_food)
        {
            if (inedible_food == 1)
            {
                ASSERT(wonteat.defined());
                // Use the normal cannot ingest message.
                if (can_ingest(wonteat, false))
                {
                    mprf(MSGCH_DIAGNOSTICS, "Error: Can eat %s after all?",
                         wonteat.name(false, DESC_PLAIN).c_str());
                }
            }
            else // Several different food items.
                mpr(gettext("You refuse to eat these food items."));
            need_more = true;
        }
    }

    if (need_more && Options.auto_list)
        more();

    return (0);
}

bool eat_from_inventory()
{
    if (!_eat_check())
        return (false);

    // Corpses should have been handled before.
    if (you.species == SP_VAMPIRE)
        return 0;

    int unusable_corpse = 0;
    int inedible_food = 0;
    item_def *wonteat = NULL;
    bool found_valid = false;

    std::vector<item_def *> food_items;
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!you.inv[i].defined())
            continue;

        item_def *item = &you.inv[i];
        if (you.species == SP_VAMPIRE)
        {
            if (item->base_type != OBJ_CORPSES || item->sub_type != CORPSE_BODY)
                continue;

            if (!mons_has_blood(item->plus))
            {
                unusable_corpse++;
                continue;
            }
        }
        else
        {
            // Chunks should have been handled before.
            if (item->base_type != OBJ_FOOD || item->sub_type == FOOD_CHUNK)
                continue;
        }

        if (is_bad_food(*item))
            continue;

        if (!can_ingest(*item, true))
        {
            if (!inedible_food)
            {
                wonteat = item;
                inedible_food++;
            }
            else
            {
                // Increase only if we're dealing with different subtypes.
                // FIXME: Use a common check for herbivorous/carnivorous
                //        dislikes, for e.g. "Blech! You need blood!"
                ASSERT(wonteat->defined());
                if (wonteat->sub_type != item->sub_type)
                    inedible_food++;
            }
            continue;
        }

        found_valid = true;
        food_items.push_back(item);
    }

    if (found_valid)
    {
        std::sort(food_items.begin(), food_items.end(), compare_by_freshness());
        for (unsigned int i = 0; i < food_items.size(); ++i)
        {
            item_def *item = food_items[i];
            std::string item_name = get_menu_colour_prefix_tags(*item,
                                                                DESC_NOCAP_A);

            mprf(MSGCH_PROMPT, gettext("%s %s%s? (ye/n/q)"),
                 (you.species == SP_VAMPIRE ? pgettext("food","Drink blood from") : pgettext("food","Eat")),
                 ((item->quantity > 1) ? pgettext("food","one of ") : ""),
                 item_name.c_str());

            int keyin = tolower(getchm(KMC_CONFIRM));
            switch (keyin)
            {
            case 'q':
            CASE_ESCAPE
                canned_msg(MSG_OK);
                return (false);
            case 'e':
            case 'y':
                if (can_ingest(*item, false))
                {
                    eat_inventory_item(item->link);
                    return (true);
                }
                break;
            default:
                // Else no: try next one.
                break;
            }
        }
    }
    else
    {
        // Give a message about why these food items can not actually be eaten.
        if (unusable_corpse)
        {
            mprf(gettext("%s devoid of blood."),
                    unusable_corpse == 1 ? gettext("The corpse you are carrying is")
                    : gettext("The corpses you are carrying are"));
        }
        else if (inedible_food)
        {
            if (inedible_food == 1)
            {
                ASSERT(wonteat->defined());
                // Use the normal cannot ingest message.
                if (can_ingest(*wonteat, false))
                {
                    mprf(MSGCH_DIAGNOSTICS, "Error: Can eat %s after all?",
                         wonteat->name(false, DESC_PLAIN).c_str());
                }
            }
            else // Several different food items.
                mpr(gettext("You refuse to eat these food items."));
        }
    }

    return (false);
}

// Returns -1 for cancel, 1 for eaten, 0 for not eaten,
//         -2 for skip to inventory.
int prompt_eat_chunks(bool only_auto)
{
    // Full herbivores cannot eat chunks.
    if (player_mutation_level(MUT_HERBIVOROUS) == 3)
        return (0);

    bool found_valid = false;
    std::vector<item_def *> chunks;

    // First search the stash on the floor, unless levitating.
    if (you.flight_mode() != FL_LEVITATE)
    {
        for (stack_iterator si(you.pos(), true); si; ++si)
        {
            if (you.species == SP_VAMPIRE)
            {
                if (si->base_type != OBJ_CORPSES || si->sub_type != CORPSE_BODY)
                    continue;

                if (!mons_has_blood(si->plus))
                    continue;
            }
            else if (si->base_type != OBJ_FOOD || si->sub_type != FOOD_CHUNK)
                continue;

            if (!can_ingest(*si, true))
                continue;

            // Don't prompt for bad food types.
            if (is_bad_food(*si))
                continue;

            // You have to be hungry enough to consider eating a chunk
            if (you.hunger_state > _maximum_satiation(_food_preference(*si))
                && you.species != SP_VAMPIRE)
            {
                continue;
            }

            found_valid = true;
            chunks.push_back(&(*si));
        }
    }

    // Then search through the inventory.
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!you.inv[i].defined())
            continue;

        item_def *item = &you.inv[i];
        if (you.species == SP_VAMPIRE)
        {
            if (item->base_type != OBJ_CORPSES || item->sub_type != CORPSE_BODY)
                continue;

            if (!mons_has_blood(item->plus))
                continue;
        }
        else if (item->base_type != OBJ_FOOD || item->sub_type != FOOD_CHUNK)
            continue;

        if (!can_ingest(*item, true))
            continue;

        // Don't prompt for bad food types.
        if (is_bad_food(*item))
            continue;

        // You have to be hungry enough to consider eating a chunk
        if (you.hunger_state > _maximum_satiation(_food_preference(*item))
            && you.species != SP_VAMPIRE)
        {
            continue;
        }

        found_valid = true;
        chunks.push_back(item);
    }

    const bool easy_eat = Options.easy_eat_chunks && !you.is_undead;
    const bool easy_contam = easy_eat
        && (Options.easy_eat_gourmand && wearing_amulet(AMU_THE_GOURMAND)
            || Options.easy_eat_contaminated);

    if (found_valid)
    {
        std::sort(chunks.begin(), chunks.end(), compare_by_freshness());
        for (unsigned int i = 0; i < chunks.size(); ++i)
        {
            bool autoeat = false;
            item_def *item = chunks[i];
            std::string item_name = get_menu_colour_prefix_tags(*item,
                                                                DESC_NOCAP_A);

            const bool contam = is_contaminated(*item);
            const bool bad    = is_bad_food(*item);
            // Exempt undead from auto-eating since:
            //  * Mummies don't eat.
            //  * Vampire feeding takes a lot more time than eating a chunk
            //    and may have unintended consequences.
            //  * Ghouls may want to wait until chunks become rotten
            //    or until they have some hp rot to heal.
            if (easy_eat && !bad && !contam)
            {
                // If this chunk is safe to eat, just do so without prompting.
                autoeat = true;
            }
            else if (easy_contam && contam && !bad)
                autoeat = true;
            else if (only_auto)
                return 0;
            else
            {
                mprf(MSGCH_PROMPT, gettext("%s %s%s? (ye/n/q/i?)"),
                     (you.species == SP_VAMPIRE ? pgettext("food","Drink blood from") : pgettext("food","Eat")),
                     ((item->quantity > 1) ? pgettext("food","one of ") : ""),
                     item_name.c_str());
            }

            int keyin = autoeat ? 'y' : tolower(getchm(KMC_CONFIRM));
            switch (keyin)
            {
            case 'q':
            CASE_ESCAPE
                canned_msg(MSG_OK);
                return (-1);
            case 'i':
            case '?':
                // Skip ahead to the inventory.
                return (-2);
            case 'e':
            case 'y':
                if (can_ingest(*item, false))
                {
                    if (autoeat)
                    {
                        mprf(pgettext("food","%s %s%s."),
                             (you.species == SP_VAMPIRE ? pgettext("food","Drinking blood from")
                                                        : pgettext("food","Eating")),
                             ((item->quantity > 1) ? pgettext("food","one of ") : ""),
                             item_name.c_str());
                    }

                    if (in_inventory(*item))
                    {
                        eat_inventory_item(item->link);
                        return (1);
                    }
                    else
                    {
                        int ilink = item_on_floor(*item, you.pos());

                        if (ilink != NON_ITEM)
                        {
                            eat_floor_item(ilink);
                            return (1);
                        }
                        return (0);
                    }
                }
                break;
            default:
                // Else no: try next one.
                break;
            }
        }
    }

    return (0);
}

static const char *_chunk_flavour_phrase(bool likes_chunks)
{
    const char *phrase;
    const int level = player_mutation_level(MUT_SAPROVOROUS);

    switch (level)
    {
    case 1:
    case 2:
        phrase = gettext("tastes unpleasant.");
        break;

    case 3:
        phrase = gettext("tastes good.");
        break;

    default:
        phrase = gettext("tastes terrible.");
        break;
    }

    if (likes_chunks)
        phrase = gettext("tastes great.");
    else
    {
        const int gourmand = you.duration[DUR_GOURMAND];
        if (gourmand >= GOURMAND_MAX)
        {
            phrase = one_chance_in(1000) ? gettext("tastes like chicken!")
                                         : gettext("tastes great.");
        }
        else if (gourmand > GOURMAND_MAX * 75 / 100)
            phrase = gettext("tastes very good.");
        else if (gourmand > GOURMAND_MAX * 50 / 100)
            phrase = gettext("tastes good.");
        else if (gourmand > GOURMAND_MAX * 25 / 100)
            phrase = gettext("is not very appetising.");
    }

    return (phrase);
}

void chunk_nutrition_message(int nutrition)
{
    int perc_nutrition = nutrition * 100 / CHUNK_BASE_NUTRITION;
    if (perc_nutrition < 15)
        mpr(gettext("That was extremely unsatisfying."));
    else if (perc_nutrition < 35)
        mpr(gettext("That was not very filling."));
}

static void _say_chunk_flavour(bool likes_chunks)
{
    mprf(gettext("This raw flesh %s"), _chunk_flavour_phrase(likes_chunks));
}

// Never called directly - chunk_effect values must pass
// through food::_determine_chunk_effect() first. {dlb}:
static void _eat_chunk(corpse_effect_type chunk_type, bool cannibal,
                       int mon_intel, bool holy, bool rotten)
{
    bool likes_chunks = player_likes_chunks(true);
    int preference    = _food_preference(FOOD_CHUNK, true, chunk_type, rotten);
    int hp_amt        = 0;
    bool suppress_msg = false; // do we display the chunk nutrition message?
    int nutrition = CHUNK_BASE_NUTRITION * _chunk_satiation_multiplier(preference);
    corpse_effect_type chunk_effect = _determine_chunk_effect(chunk_type, rotten);

    if (you.species == SP_GHOUL)
    {
        nutrition    = CHUNK_BASE_NUTRITION;
        hp_amt       = 1 + random2(5) + random2(1 + you.experience_level);
        suppress_msg = true;
    }

    switch (chunk_effect)
    {
    case CE_MUTAGEN_RANDOM:
        mpr(gettext("This meat tastes really weird."));
        mutate(RANDOM_MUTATION);
        did_god_conduct(DID_DELIBERATE_MUTATING, 10);
        xom_is_stimulated(100);
        break;

    case CE_MUTAGEN_BAD:
        mpr(gettext("This meat tastes *really* weird."));
        give_bad_mutation();
        did_god_conduct(DID_DELIBERATE_MUTATING, 10);
        xom_is_stimulated(random2(200));
        break;

    case CE_ROT:
        you.rot(&you, 10 + random2(10));
        if (you.sicken(50 + random2(100)))
            xom_is_stimulated(random2(100));
        break;

    case CE_POISONOUS:
    case CE_POISON_CONTAM:
        mpr(gettext("Yeeuch - this meat is poisonous!"));
        if (poison_player(3 + random2(4), "", "poisonous meat"))
            xom_is_stimulated(random2(100));

        if (chunk_effect == CE_POISONOUS)
            break;
        // else fall through

    case CE_ROTTEN:
    case CE_CONTAMINATED:
        if (player_mutation_level(MUT_SAPROVOROUS) == 3)
        {
            mprf(gettext("This %sflesh tastes delicious!"),
                chunk_effect == CE_ROTTEN ? pgettext("food","rotting ") : "");

            if (you.species == SP_GHOUL)
                _heal_from_food(hp_amt, 0, !one_chance_in(4), one_chance_in(5));
        }
        else
        {
            mpr(gettext("There is something wrong with this meat."));
            if (you.sicken(50 + random2(100), false))
            {
                learned_something_new(HINT_CONTAMINATED_CHUNK);
                xom_is_stimulated(random2(100));
            }
        }
        break;

    case CE_CLEAN:
        if (player_mutation_level(MUT_SAPROVOROUS) == 3)
        {
            mpr(gettext("This raw flesh tastes good."));

            if (you.species == SP_GHOUL)
            {
                _heal_from_food((!one_chance_in(5) ? hp_amt : 0), 0,
                                !one_chance_in(3));
            }
        }
        else
            _say_chunk_flavour(likes_chunks);

        break;

    case CE_MUTAGEN_GOOD:
    case CE_NOCORPSE:
    case CE_RANDOM:
        mprf(MSGCH_ERROR, "This flesh (%d) tastes buggy!", chunk_effect);
        break;
    }

    if (cannibal)
        did_god_conduct(DID_CANNIBALISM, 10);
    else if (mon_intel > 0)
        did_god_conduct(DID_EAT_SOULED_BEING, mon_intel);

    if (holy)
        did_god_conduct(DID_VIOLATE_HOLY_CORPSE, 2);

    start_delay(DELAY_EAT, 2, (suppress_msg) ? 0 : nutrition, -1);
    lessen_hunger(nutrition, true);
    dprf("Eat chunk, nutrition:%d, preference:%d.", nutrition, preference);
}

static void _eat_permafood(int item_type)
{
    int food_value = 0;

    // apply base sustenance {dlb}:
    switch (item_type)
    {
    case FOOD_MEAT_RATION:
    case FOOD_ROYAL_JELLY:
        food_value = 5000;
        break;
    case FOOD_BREAD_RATION:
        food_value = 4400;
        break;
    case FOOD_AMBROSIA:
        food_value = 2500;
        break;
    case FOOD_HONEYCOMB:
        food_value = 2000;
        break;
    case FOOD_SNOZZCUMBER:  // Maybe a nasty side-effect from RD's book?
                            // I'd like that, but I don't dare. (jpeg)
    case FOOD_PIZZA:
    case FOOD_BEEF_JERKY:
        food_value = 1500;
        break;
    case FOOD_CHEESE:
    case FOOD_SAUSAGE:
        food_value = 1200;
        break;
    case FOOD_ORANGE:
    case FOOD_BANANA:
    case FOOD_LEMON:
        food_value = 1000;
        break;
    case FOOD_PEAR:
    case FOOD_APPLE:
    case FOOD_APRICOT:
        food_value = 700;
        break;
    case FOOD_CHOKO:
    case FOOD_RAMBUTAN:
    case FOOD_LYCHEE:
        food_value = 600;
        break;
    case FOOD_STRAWBERRY:
        food_value = 200;
        break;
    case FOOD_GRAPE:
        food_value = 100;
        break;
    case FOOD_SULTANA:
        food_value = 70;     // Will not save you from starvation.
        break;
    default:
        break;
    }

    // multiply the base satiation by a value determined by how much
    // the player likes the food
    const int preference = _food_preference(item_type, true);
    food_value *= _permafood_satiation_multiplier(preference);

    if (food_value > 0)
    {
        int duration = 1;
        if (item_type == FOOD_MEAT_RATION || item_type == FOOD_BREAD_RATION)
            duration = 3;

        start_delay(DELAY_EAT, duration, 0, item_type);
        lessen_hunger(food_value, true);
        dprf("Eat permafood, nutrition:%d, preference:%d.", food_value,
             preference);

        if (player_mutation_level(MUT_FOOD_JELLY)
            && x_chance_in_y(food_value, 12000))
        {
            mgen_data mg(MONS_JELLY, BEH_STRICT_NEUTRAL, 0, 0, 0,
                         you.pos(), MHITNOT, 0, you.religion);

            if (create_monster(mg) != -1)
                mprf(gettext("A jelly spawns from your body."));
        }
    }
}

// Handle messaging at the end of eating.
// Some food types may not get a message.
void finished_eating_message(int food_type)
{
    bool herbivorous = player_mutation_level(MUT_HERBIVOROUS) > 0;
    bool carnivorous = player_mutation_level(MUT_CARNIVOROUS) > 0;

    if (herbivorous)
    {
        switch (food_type)
        {
        case FOOD_MEAT_RATION:
        case FOOD_BEEF_JERKY:
        case FOOD_SAUSAGE:
            mpr(gettext("Blech - you need greens!"));
            return;
        default:
            break;
        }
    }
    else
    {
        switch (food_type)
        {
        case FOOD_MEAT_RATION:
            mpr(gettext("That meat ration really hit the spot!"));
            return;
        case FOOD_BEEF_JERKY:
            mprf(gettext("That beef jerky was %s!"),
                 one_chance_in(4) ? gettext("jerk-a-riffic")
                                  : gettext("delicious"));
            return;
        case FOOD_SAUSAGE:
            mpr(gettext("That sausage was delicious!"));
            return;
        default:
            break;
        }
    }

    if (carnivorous)
    {
        switch (food_type)
        {
        case FOOD_BREAD_RATION:
        case FOOD_BANANA:
        case FOOD_ORANGE:
        case FOOD_LEMON:
        case FOOD_PEAR:
        case FOOD_APPLE:
        case FOOD_APRICOT:
        case FOOD_CHOKO:
        case FOOD_SNOZZCUMBER:
        case FOOD_RAMBUTAN:
        case FOOD_LYCHEE:
        case FOOD_STRAWBERRY:
        case FOOD_GRAPE:
        case FOOD_SULTANA:
            mpr(gettext("Blech - you need meat!"));
            return;
        default:
            break;
        }
    }
    else
    {
        switch (food_type)
        {
        case FOOD_BREAD_RATION:
            mpr(gettext("That bread ration really hit the spot!"));
            return;
        case FOOD_PEAR:
        case FOOD_APPLE:
        case FOOD_APRICOT:
            mprf(gettext("Mmmm... Yummy %s."),
                (food_type == FOOD_APPLE)   ? pgettext("food","apple") :
                (food_type == FOOD_PEAR)    ? pgettext("food","pear") :
                (food_type == FOOD_APRICOT) ? pgettext("food","apricot")
                                            : pgettext("food","fruit"));
            return;
        case FOOD_CHOKO:
            mpr(gettext("That choko was very bland."));
            return;
        case FOOD_SNOZZCUMBER:
            mpr(gettext("That snozzcumber tasted truly putrid!"));
            return;
        case FOOD_ORANGE:
            mprf(gettext("That orange was delicious!%s"),
                 one_chance_in(8) ? gettext(" Even the peel tasted good!") : "");
            return;
        case FOOD_BANANA:
            mprf(gettext("That banana was delicious!%s"),
                 one_chance_in(8) ? gettext(" Even the peel tasted good!") : "");
            return;
        case FOOD_STRAWBERRY:
            mpr(gettext("That strawberry was delicious!"));
            return;
        case FOOD_RAMBUTAN:
            mpr(gettext("That rambutan was delicious!"));
            return;
        case FOOD_LEMON:
            mpr(gettext("That lemon was rather sour... but delicious nonetheless!"));
            return;
        case FOOD_GRAPE:
            mpr(gettext("That grape was delicious!"));
            return;
        case FOOD_SULTANA:
            mpr(gettext("That sultana was delicious... but very small."));
            return;
        case FOOD_LYCHEE:
            mpr(gettext("That lychee was delicious!"));
            return;
        default:
            break;
        }
    }

    switch (food_type)
    {
    case FOOD_HONEYCOMB:
        mpr(gettext("That honeycomb was delicious!"));
        break;
    case FOOD_ROYAL_JELLY:
        mpr(gettext("That royal jelly was delicious!"));
        restore_stat(STAT_ALL, 0, false);
        break;
    case FOOD_AMBROSIA:                       // XXX: could put some more
        mpr(gettext("That ambrosia tasted strange.")); // inspired messages here --evk
        potion_effect(POT_CONFUSION, 0, false, false);
        potion_effect(POT_MAGIC, 0, false, false);
        break;
    case FOOD_PIZZA:
        if (!Options.pizza.empty() && !one_chance_in(3))
            mprf(gettext("Mmm... %s."), Options.pizza.c_str());
        else
        {
            int temp_rand = random2(9);
            mprf("%s %s.",
                (carnivorous && temp_rand >= 6
                 || herbivorous && temp_rand <= 4
                 || temp_rand == 3) ? gettext("Yeuchh!") : gettext("Mmm..."),
                (temp_rand == 0) ? gettext("Ham and pineapple") :
                (temp_rand == 1) ? gettext("Super Supreme") :
                (temp_rand == 2) ? gettext("Pepperoni") :
                (temp_rand == 3) ? gettext("Anchovies") :
                (temp_rand == 4) ? gettext("Chicken") :
                (temp_rand == 5) ? gettext("Cheesy") :
                (temp_rand == 6) ? gettext("Vegetable") :
                (temp_rand == 7) ? gettext("Peppers")
                                 : pgettext("food","Mushroom"));
        }
        break;
    case FOOD_CHEESE:
    {
        int temp_rand = random2(9);
        mprf(gettext("Mmm...%s."),
             (temp_rand == 0) ? gettext("Cheddar") :
             (temp_rand == 1) ? gettext("Edam") :
             (temp_rand == 2) ? gettext("Wensleydale") :
             (temp_rand == 3) ? gettext("Camembert") :
             (temp_rand == 4) ? gettext("Goat cheese") :
             (temp_rand == 5) ? gettext("Fruit cheese") :
             (temp_rand == 6) ? gettext("Mozzarella") :
             (temp_rand == 7) ? gettext("Sheep cheese")
                              : gettext("Yak cheese"));
        break;
    }
    default:
        break;
    }
}

// Divide full nutrition by duration, so that each turn you get the same
// amount of nutrition. Also, experimentally regenerate 1 hp per feeding turn
// - this is likely too strong.
// feeding is -1 at start, 1 when finishing, and 0 else

// Here are some values for nutrition (quantity * 1000) and duration:
//    max_chunks      quantity    duration
//     1               1           1
//     2               1           1
//     3               1           2
//     4               1           2
//     5               1           2
//     6               2           3
//     7               2           3
//     8               2           3
//     9               2           4
//    10               2           4
//    12               3           5
//    15               3           5
//    20               4           6
//    25               4           6
//    30               5           7

void vampire_nutrition_per_turn(const item_def &corpse, int feeding)
{
    const int mons_type  = corpse.plus;
    const int chunk_type = _determine_chunk_effect(
                                mons_corpse_effect(mons_type), false);

    // Duration depends on corpse weight.
    const int max_chunks = get_max_corpse_chunks(mons_type);
    int chunk_amount     = 1 + max_chunks/3;
        chunk_amount     = stepdown_value(chunk_amount, 6, 6, 12, 12);

    // Add 1 for the artificial extra call at the start of draining.
    const int duration   = 1 + chunk_amount;

    // Use number of potions per corpse to calculate total nutrition, which
    // then gets distributed over the entire duration.
    int food_value = CHUNK_BASE_NUTRITION
                     * num_blood_potions_from_corpse(mons_type, chunk_type);

    bool start_feeding   = false;
    bool end_feeding     = false;

    if (feeding < 0)
        start_feeding = true;
    else if (feeding > 0)
        end_feeding = true;

    switch (mons_type)
    {
        case MONS_HUMAN:
            food_value += random2avg((you.experience_level * 10)/duration, 2);

            // Human may give a bit of healing during feeding as long as
            // the corpse is still fairly fresh.
            if (corpse.special > 150)
            {
                int hp_amt = 1 + you.experience_level/3;

                if (!end_feeding)
                {
                    if (start_feeding)
                        mpr(gettext("This warm blood tastes really delicious!"));

                    if (hp_amt >= duration)
                        hp_amt /= duration;
                    else if (x_chance_in_y(hp_amt, duration))
                        hp_amt = 1;

                    _heal_from_food(hp_amt);
                }
                else
                {
                    // Give the remainder of healing at the end.
                    if (hp_amt > duration)
                        _heal_from_food(hp_amt % duration);
                }
            }
            break;

        case MONS_ELF:
            food_value += random2avg((you.experience_level * 10) / duration, 2);

            // Elven blood gives a bit of mana at the end of feeding,
            // but only from fairly fresh corpses.
            if (corpse.special > 150)
            {
                if (end_feeding)
                {
                    const int mp_amt = 1 + random2(3);
                    _heal_from_food(1, mp_amt);
                }
                else if (start_feeding)
                    mpr(gettext("This warm blood tastes magically delicious!"));
            }
            break;

        default:
            switch (chunk_type)
            {
                case CE_CLEAN:
                    if (start_feeding)
                    {
                        mprf(gettext("This %sblood tastes delicious!"),
                             mons_class_flag(mons_type, M_WARM_BLOOD) ? gettext("warm ")
                                                                      : "");
                    }
                    else if (end_feeding && corpse.special > 150)
                        _heal_from_food(1);
                    break;

                case CE_CONTAMINATED:
                    food_value /= 2;
                    if (start_feeding)
                        mpr(gettext("Somehow this blood was not very filling!"));
                    else if (end_feeding && corpse.special > 150)
                        _heal_from_food(1);
                    break;

                case CE_POISONOUS:
                case CE_POISON_CONTAM:
                    make_hungry(food_value / duration / 2, false);
                    // Always print this message - maybe you lost poison
                    // resistance due to feeding.
                    mpr(gettext("Blech - this blood tastes nasty!"));
                    if (poison_player(1 + random2(3), "", "poisonous blood"))
                        xom_is_stimulated(random2(100));
                    stop_delay();
                    return;

                case CE_MUTAGEN_RANDOM:
                    food_value /= 2;
                    if (start_feeding)
                        mpr(gettext("This blood tastes really weird!"));
                    mutate(RANDOM_MUTATION);
                    did_god_conduct(DID_DELIBERATE_MUTATING, 10);
                    xom_is_stimulated(100);
                    // Sometimes heal by one hp.
                    if (end_feeding && corpse.special > 150 && coinflip())
                        _heal_from_food(1);
                    break;

                case CE_MUTAGEN_BAD:
                    food_value /= 2;
                    if (start_feeding)
                        mpr(gettext("This blood tastes *really* weird!"));
                    give_bad_mutation();
                    did_god_conduct(DID_DELIBERATE_MUTATING, 10);
                    xom_is_stimulated(random2(200));
                    // No healing from bad mutagenic blood.
                    break;

                case CE_ROT:
                    you.rot(&you, 5 + random2(5));
                    if (you.sicken(50 + random2(100)))
                        xom_is_stimulated(random2(100));
                    stop_delay();
                    break;
            }
    }

    if (!end_feeding)
        lessen_hunger(food_value / duration, !start_feeding);
}

bool is_bad_food(const item_def &food)
{
    return (is_poisonous(food) || is_mutagenic(food)
            || is_forbidden_food(food) || causes_rot(food));
}

// Returns true if a food item (also corpses) is poisonous AND the player
// is not (known to be) poison resistant.
bool is_poisonous(const item_def &food)
{
    if (food.base_type != OBJ_FOOD && food.base_type != OBJ_CORPSES)
        return (false);

    if (player_res_poison(false))
        return (false);

    return (chunk_is_poisonous(mons_corpse_effect(food.plus)));
}

// Returns true if a food item (also corpses) is mutagenic.
bool is_mutagenic(const item_def &food)
{
    if (food.base_type != OBJ_FOOD && food.base_type != OBJ_CORPSES)
        return (false);

    // Has no effect on ghouls.
    if (you.species == SP_GHOUL)
        return (false);

    return (mons_corpse_effect(food.plus) == CE_MUTAGEN_RANDOM);
}

// Returns true if a food item (also corpses) may cause sickness.
bool is_contaminated(const item_def &food)
{
    if ((food.base_type != OBJ_FOOD || food.sub_type != FOOD_CHUNK)
            && food.base_type != OBJ_CORPSES)
        return (false);

    const corpse_effect_type chunk_type = mons_corpse_effect(food.plus);
    return (chunk_type == CE_CONTAMINATED
            || (player_res_poison(false) && chunk_type == CE_POISON_CONTAM)
            || food_is_rotten(food)
               && player_mutation_level(MUT_SAPROVOROUS) < 3);
}

// Returns true if a food item (also corpses) will cause rotting.
bool causes_rot(const item_def &food)
{
    if (food.base_type != OBJ_FOOD && food.base_type != OBJ_CORPSES)
        return (false);

    // Has no effect on ghouls.
    if (you.species == SP_GHOUL)
        return (false);

    return (mons_corpse_effect(food.plus) == CE_ROT);
}

// Returns true if an item of basetype FOOD or CORPSES cannot currently
// be eaten (respecting species and mutations set).
bool is_inedible(const item_def &item)
{
    // Mummies don't eat.
    if (you.is_undead == US_UNDEAD)
        return (true);

    if (item.base_type == OBJ_FOOD && !can_ingest(item, true, true, false))
        return (true);

    if (item.base_type == OBJ_CORPSES
        && (item.sub_type == CORPSE_SKELETON
            || you.species == SP_VAMPIRE && !mons_has_blood(item.plus)))
    {
        return (true);
    }

    return (false);
}

// As we want to avoid autocolouring the entire food selection, this should
// be restricted to the absolute highlights, even though other stuff may
// still be edible or even delicious.
bool is_preferred_food(const item_def &food)
{
    // Mummies don't eat.
    if (you.is_undead == US_UNDEAD)
        return (false);

    // Vampires don't really have a preferred food type, but they really
    // like blood potions.
    if (you.species == SP_VAMPIRE)
        return (is_blood_potion(food));

    if (food.base_type == OBJ_POTIONS && food.sub_type == POT_PORRIDGE
        && item_type_known(food))
    {
        return (!player_mutation_level(MUT_CARNIVOROUS));
    }

    if (food.base_type != OBJ_FOOD)
        return (false);

    // Poisoned, mutagenic, etc. food should never be marked as "preferred".
    if (is_bad_food(food))
        return (false);

    // Honeycombs are tasty for everyone.
    if (food.sub_type == FOOD_HONEYCOMB || food.sub_type == FOOD_ROYAL_JELLY)
        return (true);

    // Ghouls specifically like rotten food.
    if (you.species == SP_GHOUL)
        return (food_is_rotten(food));

    if (player_mutation_level(MUT_CARNIVOROUS) == 3)
        return (_plant_content(food.sub_type) < 0);

    if (player_mutation_level(MUT_HERBIVOROUS) == 3)
        return (_plant_content(food.sub_type) > 0);

    // No food preference.
    return (false);
}

bool is_forbidden_food(const item_def &food)
{
    if (food.base_type != OBJ_CORPSES
        && (food.base_type != OBJ_FOOD || food.sub_type != FOOD_CHUNK))
    {
        return (false);
    }

    // Some gods frown upon cannibalistic behaviour.
    if (god_hates_cannibalism(you.religion)
        && is_player_same_species(food.plus))
    {
        return (true);
    }

    // Holy gods do not like it if you are eating holy creatures
    if (is_good_god(you.religion)
        && mons_class_holiness(food.plus) == MH_HOLY)
    {
        return (true);
    }

    // Zin doesn't like it if you eat beings with a soul.
    if (you.religion == GOD_ZIN && mons_class_intel(food.plus) >= I_NORMAL)
        return (true);

    // Everything else is allowed.
    return (false);
}

bool can_ingest(const item_def &food, bool suppress_msg, bool reqid,
                bool check_hunger)
{
    return can_ingest(food.base_type, food.sub_type, suppress_msg, reqid,
                      check_hunger, mons_corpse_effect(food.plus),
                      food_is_rotten(food));
}

bool can_ingest(int what_isit, int kindof_thing, bool suppress_msg,
                bool reqid, bool check_hunger, corpse_effect_type effect_type,
                bool is_rotten)
{
    bool survey_says = false;

    // [ds] These redundant checks are now necessary - Lua might be calling us.
    if (!_eat_check(check_hunger, suppress_msg))
        return (false);

    if (you.species == SP_VAMPIRE)
    {
        if (what_isit == OBJ_CORPSES && kindof_thing == CORPSE_BODY)
            return (true);

        if (what_isit == OBJ_POTIONS
            && (kindof_thing == POT_BLOOD
                || kindof_thing == POT_BLOOD_COAGULATED))
        {
            return (true);
        }

        if (!suppress_msg)
            mpr(gettext("Blech - you need blood!"));

        return (false);
    }

    bool ur_carnivorous = player_mutation_level(MUT_CARNIVOROUS) == 3;
    bool ur_herbivorous = player_mutation_level(MUT_HERBIVOROUS) == 3;

    switch (what_isit)
    {
    case OBJ_FOOD:
    {
        if (you.species == SP_VAMPIRE)
        {
            if (!suppress_msg)
                mpr(gettext("Blech - you need blood!"));
             return (false);
        }

        // First, we calculate how much we like this thing we are considering eating
        // Then, based on that, we decide how hungry we would have to be to eat it
        const int preference = _food_preference(kindof_thing, false,
                                                effect_type, is_rotten);
        const bool inedible = preference < (kindof_thing == FOOD_CHUNK ? 0: 2);
        const bool willing_to_eat =
                    (!check_hunger
                     || you.hunger_state <= _maximum_satiation(preference));

        const int plant_content = _plant_content(kindof_thing);

        if (plant_content > 0) // Herbivorous food.
        {
            if (ur_carnivorous)
            {
                if (!suppress_msg)
                    mpr(gettext("Sorry, you're a carnivore."));
                return (false);
            }
        }
        else if (plant_content < 0) // Carnivorous food.
        {
            if (ur_herbivorous)
            {
                if (!suppress_msg)
                    mpr(gettext("Sorry, you're a herbivore."));
                return (false);
            }
            else if (kindof_thing == FOOD_CHUNK)
            {
                if (inedible)
                {
                    if (!suppress_msg)
                    {
                        if (is_rotten)
                            mpr("You refuse to eat that rotten meat.");
                        else
                            mpr("You refuse to eat this disgusting meat.");
                    }
                    return (false);
                }

                if (willing_to_eat)
                    return (true);

                if (!suppress_msg)
                    mpr(gettext("You aren't quite hungry enough to eat that!"));

                return (false);
            }
        }
        if (willing_to_eat)  // Food which is neither.
            return (true);
        else
        {
            if (!suppress_msg)
                mpr("You aren't quite hungry enough to eat that!");

            return (false);
        }
    }

    case OBJ_CORPSES:
        if (you.species == SP_VAMPIRE)
        {
            if (kindof_thing == CORPSE_BODY)
                return (true);
            else
            {
                if (!suppress_msg)
                    mpr(gettext("Blech - you need blood!"));
                return (false);
            }
        }
        return (false);

    case OBJ_POTIONS: // called by lua
        if (get_ident_type(OBJ_POTIONS, kindof_thing) != ID_KNOWN_TYPE)
            return (true);

        switch (kindof_thing)
        {
            case POT_BLOOD:
            case POT_BLOOD_COAGULATED:
                if (ur_herbivorous)
                {
                    if (!suppress_msg)
                        mpr(gettext("Sorry, you're a herbivore."));
                    return (false);
                }
                return (true);
            case POT_WATER:
                if (you.species == SP_VAMPIRE)
                {
                    if (!suppress_msg)
                        mpr(gettext("Blech - you need blood!"));
                    return (false);
                }
                return (true);
             case POT_PORRIDGE:
                if (you.species == SP_VAMPIRE)
                {
                    if (!suppress_msg)
                        mpr(gettext("Blech - you need blood!"));
                    return (false);
                }
                else if (ur_carnivorous)
                {
                    if (!suppress_msg)
                        mpr(gettext("Sorry, you're a carnivore."));
                    return (false);
                }
             default:
                return (true);
        }

    // Other object types are set to return false for now until
    // someone wants to recode the eating code to permit consumption
    // of things other than just food.
    default:
        return (false);
    }

    return (survey_says);
}

bool chunk_is_poisonous(int chunktype)
{
    return (chunktype == CE_POISONOUS || chunktype == CE_POISON_CONTAM);
}

// See if you can follow along here -- except for the amulet of the gourmand
// addition (long missing and requested), what follows is an expansion of how
// chunks were handled in the codebase up to this date ... {dlb}
static corpse_effect_type _determine_chunk_effect(corpse_effect_type chunktype,
                                                  bool rotten_chunk)
{
    // Determine the initial effect of eating a particular chunk. {dlb}
    switch (chunktype)
    {
    case CE_ROT:
    case CE_MUTAGEN_RANDOM:
        if (you.species == SP_GHOUL)
            chunktype = CE_CLEAN;
        break;

    case CE_POISONOUS:
        if (player_res_poison() > 0)
            chunktype = CE_CLEAN;
        break;

    case CE_POISON_CONTAM:
        if (player_res_poison() <= 0)
            break;
        else
        {
            chunktype = CE_CONTAMINATED;
            // and fall through
        }
    case CE_CONTAMINATED:
        switch (player_mutation_level(MUT_SAPROVOROUS))
        {
        case 1:
            if (!one_chance_in(15))
                chunktype = CE_CLEAN;
            break;

        case 2:
            if (!one_chance_in(45))
                chunktype = CE_CLEAN;
            break;

        default:
            if (!one_chance_in(5))
                chunktype = CE_CLEAN;
            break;
        }
        break;

    default:
        break;
    }

    // Determine effects of rotting on base chunk effect {dlb}:
    if (rotten_chunk)
    {
        switch (chunktype)
        {
        case CE_CLEAN:
        case CE_CONTAMINATED:
            chunktype = CE_ROTTEN;
            break;
        case CE_MUTAGEN_RANDOM:
            chunktype = CE_MUTAGEN_BAD;
            break;
        default:
            break;
        }
    }

    // One last chance for some species to safely eat rotten food. {dlb}
    if (chunktype == CE_ROTTEN)
    {
        switch (player_mutation_level(MUT_SAPROVOROUS))
        {
        case 1:
            if (!one_chance_in(5))
                chunktype = CE_CLEAN;
            break;

        case 2:
            if (!one_chance_in(15))
                chunktype = CE_CLEAN;
            break;

        default:
            break;
        }
    }

    return (chunktype);
}

static bool _vampire_consume_corpse(int slot, bool invent)
{
    ASSERT(you.species == SP_VAMPIRE);

    item_def &corpse = (invent ? you.inv[slot]
                               : mitm[slot]);

    ASSERT(corpse.base_type == OBJ_CORPSES);
    ASSERT(corpse.sub_type == CORPSE_BODY);

    if (!mons_has_blood(corpse.plus))
    {
        mpr(gettext("There is no blood in this body!"));
        return (false);
    }

    if (food_is_rotten(corpse))
    {
        mpr(gettext("It's not fresh enough."));
        return (false);
    }

    // The delay for eating a chunk (mass 1000) is 2
    // Here the base nutrition value equals that of chunks,
    // but the delay should be smaller.
    const int max_chunks = get_max_corpse_chunks(corpse.plus);
    int duration = 1 + max_chunks / 3;
    duration = stepdown_value(duration, 6, 6, 12, 12);

    // Get some nutrition right away, in case we're interrupted.
    // (-1 for the starting message.)
    vampire_nutrition_per_turn(corpse, -1);

    // The draining delay doesn't have a start action, and we only need
    // the continue/finish messages if it takes longer than 1 turn.
    start_delay(DELAY_FEED_VAMPIRE, duration, invent, slot);

    return (true);
}

static void _heal_from_food(int hp_amt, int mp_amt, bool unrot,
                            bool restore_str)
{
    if (hp_amt > 0)
        inc_hp(hp_amt);

    if (mp_amt > 0)
        inc_mp(mp_amt);

    if (unrot && player_rotted())
    {
        mpr(gettext("You feel more resilient."));
        unrot_hp(1);
    }

    if (restore_str && you.strength() < you.max_strength())
        restore_stat(STAT_STR, 1, false);

    calc_hp();
    calc_mp();
}

int you_max_hunger()
{
    // This case shouldn't actually happen.
    if (you.is_undead == US_UNDEAD)
        return (6000);

    // Ghouls can never be full or above.
    if (you.species == SP_GHOUL)
        return (6999);

    return (40000);
}

int you_min_hunger()
{
    // This case shouldn't actually happen.
    if (you.is_undead == US_UNDEAD)
        return (6000);

    // Vampires can never starve to death.  Ghouls will just rot much faster.
    if (you.is_undead)
        return (701);

    return (0);
}

void handle_starvation()
{
    if (you.is_undead != US_UNDEAD && !you.duration[DUR_DEATHS_DOOR]
        && you.hunger <= 500)
    {
        if (!you.cannot_act() && one_chance_in(40))
        {
            mpr(gettext("You lose consciousness!"), MSGCH_FOOD);
            stop_running();

            you.increase_duration(DUR_PARALYSIS, 5 + random2(8), 13);
            if (you.religion == GOD_XOM)
                xom_is_stimulated(get_tension() > 0 ? 200 : 100);
        }

        if (you.hunger <= 100)
        {
            mpr(gettext("You have starved to death."), MSGCH_FOOD);
            ouch(INSTANT_DEATH, NON_MONSTER, KILLED_BY_STARVATION);
        }
    }
}

const char* hunger_cost_string(const int hunger)
{
    if (you.is_undead == US_UNDEAD)
        return ("N/A");

    // Spell hunger is "Fruit" if casting the spell five times costs at
    // most one "Fruit".
    const char* hunger_descriptions[] = {
        M_("None"), M_("Sultana"), M_("Strawberry"), M_("Choko"), M_("Honeycomb"), M_("Ration")
    };

    const int breakpoints[] = { 1, 15, 41, 121, 401 };

    return gettext(hunger_descriptions[breakpoint_rank(hunger, breakpoints,
                                                ARRAYSZ(breakpoints))]);
}
