/**
 * @file
 * @brief Functions for using some of the wackier inventory items.
**/

#include "AppHdr.h"

#include "evoke.h"

#include <algorithm>
#include <cstdlib>
#include <string.h>

#include "externs.h"

#include "areas.h"
#include "artefact.h"
#include "cloud.h"
#include "coordit.h"
#include "decks.h"
#include "effects.h"
#include "env.h"
#include "exercise.h"
#include "fight.h"
#include "food.h"
#include "invent.h"
#include "items.h"
#include "item_use.h"
#include "itemprop.h"
#include "message.h"
#include "mon-place.h"
#include "mgen_data.h"
#include "misc.h"
#include "player-stats.h"
#include "godconduct.h"
#include "skills.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-clouds.h"
#include "spl-summoning.h"
#include "stuff.h"
#include "view.h"
#include "xom.h"

void shadow_lantern_effect()
{
    // Currently only mummies and lich form get more shadows.
    if (x_chance_in_y(player_spec_death() + 1, 8))
    {
        create_monster(mgen_data(MONS_SHADOW, BEH_FRIENDLY, &you, 2, 0,
                                 you.pos(), MHITYOU));

        item_def *lantern = you.weapon();

        // This should only get called when we are wielding a lantern of
        // shadows.
        ASSERT(lantern && lantern->base_type == OBJ_MISCELLANY
               && lantern->sub_type == MISC_LANTERN_OF_SHADOWS);

        bool known = fully_identified(*lantern);
        did_god_conduct(DID_NECROMANCY, 1, known);

        // ID the lantern and refresh the weapon display.
        if (!known)
        {
            set_ident_type(*lantern, ID_KNOWN_TYPE);
            set_ident_flags(*lantern, ISFLAG_IDENT_MASK);

            you.wield_change = true;
        }
    }
}

extern bool apply_berserk_penalty;

static bool _reaching_weapon_attack(const item_def& wpn)
{
    dist beam;

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.mode = TARG_HOSTILE;
    args.range = 2;
    args.top_prompt = "Attack whom?";
    targetter_reach hitfunc(&you, REACH_TWO);
    args.hitfunc = &hitfunc;

    direction(beam, args);

    if (!beam.isValid)
        return (false);

    if (beam.isMe())
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return (false);
    }

    if (you.confused())
    {
        beam.confusion_fuzz(2);
    }

    const coord_def delta = beam.target - you.pos();
    const int x_distance  = abs(delta.x);
    const int y_distance  = abs(delta.y);
    monster* mons = monster_at(beam.target);
    // don't allow targeting of submerged trapdoor spiders
    if (mons && mons->submerged() && feat_is_floor(grd(beam.target)))
        mons = NULL;

    const int x_first_middle = std::min(beam.target.x, you.pos().x)
                            + (x_distance / 2);
    const int y_first_middle = std::min(beam.target.y, you.pos().y)
                            + (y_distance / 2);
    const int x_second_middle = std::max(beam.target.x, you.pos().x)
                            - (x_distance / 2);
    const int y_second_middle = std::max(beam.target.y, you.pos().y)
                            - (y_distance / 2);
    const coord_def first_middle(x_first_middle, y_first_middle);
    const coord_def second_middle(x_second_middle, y_second_middle);

    if (x_distance > 2 || y_distance > 2)
    {
        mpr(gettext("Your weapon cannot reach that far!"));
        return (false); // Shouldn't happen with confused swings
    }
    else if (grd(first_middle) <= DNGN_MAX_NONREACH
             && grd(second_middle) <= DNGN_MAX_NONREACH)
    {
        // Might also be a granite statue/orcish idol which you
        // can reach _past_.
        if (you.confused())
        {
            mpr(gettext("You swing wildly and hit a wall."));
            return (true);
        }
        else
        {
            mpr(gettext("There's a wall in the way."));
            return (false);
        }
    }

    // Failing to hit someone due to a friend blocking is infuriating,
    // shadow-boxing empty space is not (and would be abusable to wait
    // with no penalty).
    if (mons)
        apply_berserk_penalty = false;

    // Choose one of the two middle squares (which might be the same).
    const coord_def middle =
                     (grd(first_middle) <= DNGN_MAX_NONREACH ? second_middle :
                     (grd(second_middle) <= DNGN_MAX_NONREACH ? first_middle :
                     (coinflip() ? first_middle : second_middle)));

    // BCR - Added a check for monsters in the way.  Only checks cardinal
    //       directions.  Knight moves are ignored.  Assume the weapon
    //       slips between the squares.

    // If we're attacking more than a space away...
    if (x_distance > 1 || y_distance > 1)
    {
        bool success = true;
        monster *midmons;
        if ((midmons = monster_at(middle))
            && !midmons->submerged())
        {
            // This chance should possibly depend on your skill with
            // the weapon.
            if (coinflip())
            {
                success = false;
                beam.target = middle;
                mons = midmons;
                if (mons->wont_attack())
                {
                    // Let's assume friendlies cooperate.
                    mpr(gettext("You could not reach far enough!"));
                    return true;
                }
            }
        }
        if (success)
            mpr(gettext("You reach to attack!"));
        else
        {
            mprf(gettext("%s is in the way."),
                 mons->observable() ? gettext(mons->name(DESC_THE).c_str())
                                    : gettext("Something you can't see"));
        }
    }

    if (mons == NULL)
    {
        // Must return true, otherwise you get a free discovery
        // of invisible monsters.
        if (you.confused())
            mprf(gettext("You swing wildly%s"), beam.isMe() ?
                                       gettext(" and almost hit yourself!") : ".");
        else
            mpr(gettext("You attack empty space."));
        return (true);
    }
    else
        fight_melee(&you, mons);

    return (true);
}

static bool _evoke_horn_of_geryon(item_def &item)
{
    // Note: This assumes that the Vestibule has not been changed.
    bool rc = false;

    if (silenced(you.pos()))
    {
        mpr(gettext("You can't produce a sound!"));
        return false;
    }
    else if (player_in_branch(BRANCH_VESTIBULE_OF_HELL))
    {
        mpr(gettext("You produce a weird and mournful sound."));

        if (you.char_direction == GDT_ASCENDING)
        {
            mpr(gettext("But nothing happens..."));
            return false;
        }

        for (int count_x = 0; count_x < GXM; count_x++)
            for (int count_y = 0; count_y < GYM; count_y++)
            {
                if (grd[count_x][count_y] == DNGN_STONE_ARCH)
                {
                    rc = true;

                    map_marker *marker =
                        env.markers.find(coord_def(count_x, count_y),
                                         MAT_FEATURE);

                    if (marker)
                    {
                        map_feature_marker *featm =
                            dynamic_cast<map_feature_marker*>(marker);
                        // [ds] Ensure we're activating the correct feature
                        // markers. Feature markers are also used for other
                        // things, notably to indicate the return point from
                        // a labyrinth or portal vault.
                        switch (featm->feat)
                        {
                        case DNGN_ENTER_COCYTUS:
                        case DNGN_ENTER_DIS:
                        case DNGN_ENTER_GEHENNA:
                        case DNGN_ENTER_TARTARUS:
                            grd[count_x][count_y] = featm->feat;
                            env.markers.remove(marker);
                            item.plus2++;
                            break;
                        default:
                            break;
                        }
                    }
                }
            }

        if (rc)
            /// 당신의 지옥으로 가는 길은 이미 열려있다.
            /// 게리욘의 뿔피리이기때문에 지옥으로 가는길로 의역했습니다.
            mpr(gettext("Your way has been unbarred."));
    }
    else
    {
        mpr(gettext("You produce a hideous howling noise!"), MSGCH_SOUND);
        create_monster(
            mgen_data::hostile_at(MONS_HELL_BEAST, "the horn of Geryon",
                true, 4, 0, you.pos()));
    }
    return (rc);
}

static bool _efreet_flask(int slot)
{
    bool friendly = x_chance_in_y(300 + you.skill(SK_EVOCATIONS, 10), 600);

    mpr(gettext("You open the flask..."));

    const int mons =
        create_monster(
            mgen_data(MONS_EFREET,
                      friendly ? BEH_FRIENDLY : BEH_HOSTILE,
                      &you, 0, 0, you.pos(),
                      MHITYOU, MG_FORCE_BEH));

    if (mons != -1)
    {
        mpr(gettext("...and a huge efreet comes out."));

        if (player_angers_monster(&menv[mons]))
            friendly = false;

        if (silenced(you.pos()))
        {
            mpr(friendly ? gettext("It nods graciously at you.")
                         : gettext("It snaps in your direction!"), MSGCH_TALK_VISUAL);
        }
        else
        {
            mpr(friendly ? gettext("\"Thank you for releasing me!\"")
                         : gettext("It howls insanely!"), MSGCH_TALK);
        }
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    dec_inv_item_quantity(slot, 1);

    return (true);
}

static bool _is_crystal_ball(const item_def &item)
{
    return (item.base_type == OBJ_MISCELLANY
            && (item.sub_type == MISC_CRYSTAL_BALL_OF_ENERGY));
}

static bool _check_crystal_ball(int subtype, bool known)
{
    if (you.intel() <= 1)
    {
        mpr(gettext("You lack the intelligence to focus on the shapes in the ball."));
        return (false);
    }

    if (you.confused())
    {
        mpr(gettext("You are unable to concentrate on the shapes in the ball."));
        return (false);
    }

    if (subtype == MISC_CRYSTAL_BALL_OF_ENERGY
        && known
        && you.magic_points == you.max_magic_points)
    {
        mpr(gettext("With no energy to recover, the crystal ball of energy is "
            "presently useless to you."));
        return (false);
    }

    if (you.skill(SK_EVOCATIONS) < 2)
    {
        mpr(gettext("You lack the skill to use this item."));
        return false;
    }

    return (true);
}

bool disc_of_storms(bool drac_breath)
{
    const int fail_rate = 30 - you.skill(SK_EVOCATIONS);
    bool rc = false;

    if ((player_res_electricity() || x_chance_in_y(fail_rate, 100))
         && !drac_breath)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
    }
    else if (x_chance_in_y(fail_rate, 100) && !drac_breath)
        mpr(gettext("The disc glows for a moment, then fades."));
    else if (x_chance_in_y(fail_rate, 100) && !drac_breath)
        mpr(gettext("Little bolts of electricity crackle over the disc."));
    else
    {
        if (!drac_breath)
            mpr(gettext("The disc erupts in an explosion of electricity!"));
        rc = true;

        const int disc_count = (drac_breath) ? roll_dice(2, 1 + you.experience_level / 7) :
            roll_dice(2, 1 + you.skill_rdiv(SK_EVOCATIONS, 1, 7));

        for (int i = 0; i < disc_count; ++i)
        {
            bolt beam;
            const zap_type types[] = { ZAP_LIGHTNING, ZAP_ELECTRICITY,
                                       ZAP_ORB_OF_ELECTRICITY };

            const zap_type which_zap = RANDOM_ELEMENT(types);

            // range has no tracer, so randomness is ok
            beam.range = (drac_breath) ? you.experience_level / 3 + 5 :
                you.skill_rdiv(SK_EVOCATIONS, 1, 3) + 5; // 5--14
            beam.source = you.pos();
            beam.target = you.pos() + coord_def(random2(13)-6, random2(13)-6);
            int power = (drac_breath) ? 25 + you.experience_level : 30
                                           + you.skill(SK_EVOCATIONS, 2);
            // Non-controlleable, so no player tracer.
            zapping(which_zap, power, beam);

        }

        if (!drac_breath)
        {
            for (radius_iterator ri(you.pos(), LOS_RADIUS, false); ri; ++ri)
            {
                if (grd(*ri) < DNGN_MAXWALL)
                    continue;

                if (one_chance_in(60 - you.skill(SK_EVOCATIONS)))
                    place_cloud(CLOUD_RAIN, *ri,
                                random2(you.skill(SK_EVOCATIONS)), &you);
            }
        }
    }
    return (rc);
}

void tome_of_power(int slot)
{
    int powc = 5 + you.skill(SK_EVOCATIONS)
                 + roll_dice(5, you.skill(SK_EVOCATIONS));

    msg::stream << make_stringf(gettext("The book opens to a page covered in %s."), weird_writing().c_str()) << std::endl;

    you.turn_is_over = true;
    if (!item_ident(you.inv[slot], ISFLAG_KNOW_TYPE))
    {
        set_ident_flags(you.inv[slot], ISFLAG_KNOW_TYPE);

        if (!yesno(gettext("Read it?"), false, 'n'))
            return;
    }

    if (player_mutation_level(MUT_BLURRY_VISION) > 0
        && x_chance_in_y(player_mutation_level(MUT_BLURRY_VISION), 4))
    {
        mpr(gettext("The page is too blurry for you to read."));
        return;
    }

    mpr(gettext("You find yourself reciting the magical words!"));
    practise(EX_WILL_READ_TOME);
    count_action(CACT_EVOKE, EVOC_MISC);

    if (x_chance_in_y(7, 50))
    {
        mpr(gettext("A cloud of weird smoke pours from the book's pages!"));
        big_cloud(random_smoke_type(), &you, you.pos(), 20, 10 + random2(8));
        xom_is_stimulated(12);
    }
    else if (x_chance_in_y(2, 43))
    {
        mpr(gettext("A cloud of choking fumes pours from the book's pages!"));
        big_cloud(CLOUD_POISON, &you, you.pos(), 20, 7 + random2(5));
        xom_is_stimulated(50);
    }
    else if (x_chance_in_y(2, 41))
    {
        mpr(gettext("A cloud of freezing gas pours from the book's pages!"));
        big_cloud(CLOUD_COLD, &you, you.pos(), 20, 8 + random2(5));
        xom_is_stimulated(50);
    }
    else if (x_chance_in_y(3, 39))
    {
        if (one_chance_in(5))
        {
            mpr(gettext("The book disappears in a mighty explosion!"));
            dec_inv_item_quantity(slot, 1);
        }

        immolation(15, IMMOLATION_TOME, you.pos(), false, &you);

        xom_is_stimulated(200);
    }
    else if (one_chance_in(36))
    {
        if (create_monster(
                mgen_data::hostile_at(MONS_ABOMINATION_SMALL,
                    gettext("a tome of Destruction"),
                    true, 6, 0, you.pos())) != -1)
        {
            mpr(gettext("A horrible Thing appears!"));
            mpr(gettext("It doesn't look too friendly."));
        }
        xom_is_stimulated(200);
    }
    else
    {
        viewwindow();

        int temp_rand = random2(23) + random2(you.skill_rdiv(SK_EVOCATIONS, 1, 3));

        if (temp_rand > 25)
            temp_rand = 25;

        const spell_type spell_casted =
            ((temp_rand > 24) ? SPELL_LEHUDIBS_CRYSTAL_SPEAR :
             (temp_rand > 21) ? SPELL_BOLT_OF_FIRE :
             (temp_rand > 18) ? SPELL_BOLT_OF_COLD :
             (temp_rand > 16) ? SPELL_LIGHTNING_BOLT :
             (temp_rand > 10) ? SPELL_FIREBALL :
             (temp_rand >  9) ? SPELL_VENOM_BOLT :
             (temp_rand >  8) ? SPELL_BOLT_OF_DRAINING :
             (temp_rand >  7) ? SPELL_BOLT_OF_INACCURACY :
             (temp_rand >  6) ? SPELL_STICKY_FLAME_RANGE :
             (temp_rand >  5) ? SPELL_TELEPORT_SELF :
             (temp_rand >  4) ? SPELL_CIGOTUVIS_DEGENERATION :
             (temp_rand >  3) ? SPELL_POLYMORPH_OTHER :
             (temp_rand >  2) ? SPELL_MEPHITIC_CLOUD :
             (temp_rand >  1) ? SPELL_THROW_FLAME :
             (temp_rand >  0) ? SPELL_THROW_FROST
                              : SPELL_MAGIC_DART);

        your_spells(spell_casted, powc, false);
    }
}

void stop_studying_manual(bool finish)
{
    const skill_type sk = you.manual_skill;
    if (finish)
    {
        mprf(gettext("You have finished your manual of %s and toss it away."),
             gettext(skill_name(sk)));
        dec_inv_item_quantity(you.manual_index, 1);
    }
    else
        mprf(gettext("You stop studying %s."), gettext(skill_name(sk)));

    you.manual_skill = SK_NONE;
    you.manual_index = -1;
    if (training_restricted(sk))
        you.stop_train.insert(sk);
}

void skill_manual(int slot)
{
    item_def& manual(you.inv[slot]);
    const bool known = item_type_known(manual);
    if (!known)
        set_ident_flags(manual, ISFLAG_KNOW_TYPE);
    const skill_type skill = static_cast<skill_type>(manual.plus);

    if (is_useless_skill(skill))
    {
        if (!known)
            mprf("This is a manual of %s.", skill_name(skill));
        mpr("You have no use for it.");
        return;
    }

    if (skill == you.manual_skill)
    {
        stop_studying_manual();
        you.turn_is_over = true;
        return;
    }

    if (!known)
    {
        std::string prompt = make_stringf(gettext("This is a manual of %s. Do you want "
                                          "to study it?"), skill_name(skill));
        if (!yesno(prompt.c_str(), true, 'n'))
        {
            canned_msg(MSG_OK);
            return;
        }
    }

    if (!is_invalid_skill(you.manual_skill))
        stop_studying_manual();

    mprf(gettext("You start studying %s."), gettext(skill_name(skill)));
    you.manual_skill = skill;
    you.manual_index = slot;
    you.start_train.insert(skill);
    you.turn_is_over = true;
}

static bool _box_of_beasts(item_def &box)
{
    bool success = false;

    mpr(gettext("You open the lid..."));

    if (x_chance_in_y(60 + you.skill(SK_EVOCATIONS), 100))
    {
        const monster_type beasts[] = {
            MONS_BAT,       MONS_HOUND,     MONS_JACKAL,
            MONS_RAT,       MONS_ICE_BEAST, MONS_ADDER,
            MONS_YAK,       MONS_BUTTERFLY, MONS_WATER_MOCCASIN,
            MONS_CROCODILE, MONS_HELL_HOUND
        };

        monster_type mon = MONS_NO_MONSTER;

        // If you worship a good god, don't summon an unholy beast (in
        // this case, the hell hound).
        do
            mon = RANDOM_ELEMENT(beasts);
        while (player_will_anger_monster(mon));

        const bool friendly = !x_chance_in_y(100,
                                   you.skill(SK_EVOCATIONS, 100) + 500);

        if (create_monster(
                mgen_data(mon,
                          friendly ? BEH_FRIENDLY : BEH_HOSTILE, &you,
                          2 + random2(4), 0,
                          you.pos(),
                          MHITYOU)) != -1)
        {
            success = true;

            mpr(gettext("...and something leaps out!"));
            xom_is_stimulated(10);
        }
    }
    else
    {
        if (!one_chance_in(6))
            mpr(gettext("...but nothing happens."));
        else
        {
            mpr(gettext("...but the box appears empty."));
            box.sub_type = MISC_EMPTY_EBONY_CASKET;
        }
    }

    return (success);
}

static bool _ball_of_energy(void)
{
    bool ret = false;

    mpr(gettext("You gaze into the crystal ball."));

    int use = random2(you.skill(SK_EVOCATIONS, 6));

    if (use < 2)
        lose_stat(STAT_INT, 1 + random2avg(7, 2), false, gettext("using a ball of energy"));
    else if (use < 5 && enough_mp(1, true))
    {
        mpr(gettext("You feel your power drain away!"));
        set_mp(0);
    }
    else if (use < 10)
    {
        confuse_player(10 + random2(10));
    }
    else
    {
        int proportional = (you.magic_points * 100) / you.max_magic_points;

        if (random2avg(77 - you.skill(SK_EVOCATIONS, 2), 4) > proportional
            || one_chance_in(25))
        {
            mpr(gettext("You feel your power drain away!"));
            set_mp(0);
        }
        else
        {
            mpr(gettext("You are suffused with power!"));
            inc_mp(5 + random2avg(you.skill(SK_EVOCATIONS), 2));

            ret = true;
        }
    }

    return (ret);
}

bool evoke_item(int slot)
{
    if (you.berserk() && (slot == -1
                       || slot != you.equip[EQ_WEAPON]
                       || !weapon_reach(*you.weapon())))
    {
        canned_msg(MSG_TOO_BERSERK);
        return (false);
    }

    if (!player_can_handle_equipment())
    {
        canned_msg(MSG_PRESENT_FORM);
        return (false);
    }

    if (slot == -1)
    {
        slot = prompt_invent_item(gettext("Evoke which item? (* to show all)"),
                                   MT_INVLIST,
                                   OSEL_EVOKABLE, true, true, true, 0, -1,
                                   NULL, OPER_EVOKE);

        if (prompt_failed(slot))
            return (false);
    }
    else if (!check_warning_inscriptions(you.inv[slot], OPER_EVOKE))
        return (false);

    ASSERT(slot >= 0);

#ifdef ASSERTS // Used only by an assert
    const bool wielded = (you.equip[EQ_WEAPON] == slot);
#endif /* DEBUG */

    item_def& item = you.inv[slot];
    // Also handles messages.
    if (!item_is_evokable(item, true, false, false, true))
        return (false);

    int pract = 0; // By how much Evocations is practised.
    bool did_work   = false;  // Used for default "nothing happens" message.
    bool unevokable = false;
    bool ident      = false;

    const unrandart_entry *entry = is_unrandom_artefact(item)
        ? get_unrand_entry(item.special) : NULL;

    if (entry && entry->evoke_func)
    {
        ASSERT(item_is_equipped(item));
        if (entry->evoke_func(&item, &pract, &did_work, &unevokable))
        {
            count_action(CACT_EVOKE, EVOC_MISC);
            return (did_work);
        }
    }
    else switch (item.base_type)
    {
    case OBJ_WANDS:
        zap_wand(slot);
        return (true);

    case OBJ_WEAPONS:
        ASSERT(wielded);

        if (weapon_reach(item))
        {
            if (_reaching_weapon_attack(item))
            {
                pract    = 0;
                did_work = true;
            }
            else
                return (false);
        }
        else
            unevokable = true;
        break;

    case OBJ_STAVES:
        ASSERT(wielded);

        if (item_is_rod(item))
        {
            pract = staff_spell(slot);
            // [ds] Early exit, no turns are lost.
            if (pract == -1)
                return (false);

            did_work = true;  // staff_spell() will handle messages
            count_action(CACT_EVOKE, EVOC_ROD);
        }
        else if (item.sub_type == STAFF_CHANNELING)
        {
            if (item_type_known(item)
                && !you.is_undead && you.hunger_state == HS_STARVING)
            {
                canned_msg(MSG_TOO_HUNGRY);
                return (false);
            }
            else if (you.magic_points < you.max_magic_points
                     && x_chance_in_y(you.skill(SK_EVOCATIONS, 100) + 1100, 4000))
            {
                mpr(gettext("You channel some magical energy."));
                inc_mp(1 + random2(3));
                make_hungry(50, false, true);
                pract = 1;
                did_work = true;
                ident = true;
                count_action(CACT_EVOKE, EVOC_MISC);
            }
        }
        else
        {
            unevokable = true;
        }
        break;

    case OBJ_MISCELLANY:
        did_work = true; // easier to do it this way for misc items

        if (is_deck(item))
        {
            ASSERT(wielded);
            evoke_deck(item);
            pract = 1;
            count_action(CACT_EVOKE, EVOC_DECK);
            break;
        }

        if (_is_crystal_ball(item)
            && !_check_crystal_ball(item.sub_type, item_type_known(item)))
        {
            unevokable = true;
            break;
        }

        switch (item.sub_type)
        {
        case MISC_BOTTLED_EFREET:
            if (_efreet_flask(slot))
                pract = 2;
            break;

#if TAG_MAJOR_VERSION == 32
        case MISC_CRYSTAL_BALL_OF_SEEING:
            mpr("Nothing happens.");
            pract = 0, ident = true;
            break;
#endif

        case MISC_AIR_ELEMENTAL_FAN:
            if (!x_chance_in_y(you.skill(SK_EVOCATIONS, 100), 3000))
                canned_msg(MSG_NOTHING_HAPPENS);
            else
            {
                cast_summon_elemental(100, GOD_NO_GOD, MONS_AIR_ELEMENTAL, 4, 3);
                pract = (one_chance_in(5) ? 1 : 0);
                ident = true;
            }
            break;

        case MISC_LAMP_OF_FIRE:
            if (!x_chance_in_y(you.skill(SK_EVOCATIONS, 100), 3000))
                canned_msg(MSG_NOTHING_HAPPENS);
            else
            {
                cast_summon_elemental(100, GOD_NO_GOD, MONS_FIRE_ELEMENTAL, 4, 3);
                pract = (one_chance_in(5) ? 1 : 0);
                ident = true;
            }
            break;

        case MISC_STONE_OF_EARTH_ELEMENTALS:
            if (!x_chance_in_y(you.skill(SK_EVOCATIONS, 100), 3000))
                canned_msg(MSG_NOTHING_HAPPENS);
            else
            {
                cast_summon_elemental(100, GOD_NO_GOD, MONS_EARTH_ELEMENTAL, 4, 5);
                pract = (one_chance_in(5) ? 1 : 0);
                ident = true;
            }
            break;

        case MISC_HORN_OF_GERYON:
            if (_evoke_horn_of_geryon(item))
                pract = 1;
            break;

        case MISC_BOX_OF_BEASTS:
            if (_box_of_beasts(item))
                pract = 1, ident = true;
            break;

        case MISC_CRYSTAL_BALL_OF_ENERGY:
            if (_ball_of_energy())
                pract = 1, ident = true;
            break;

#if TAG_MAJOR_VERSION == 32
        case MISC_CRYSTAL_BALL_OF_FIXATION:
            mpr(gettext("Nothing happens."));
            pract = 0, ident = true;
            break;
#endif

        case MISC_DISC_OF_STORMS:
            if (disc_of_storms())
                pract = (coinflip() ? 2 : 1), ident = true;
            break;

        case MISC_QUAD_DAMAGE:
            mpr(gettext("QUAD DAMAGE!"));
            you.duration[DUR_QUAD_DAMAGE] = 30 * BASELINE_DELAY;
            ASSERT(in_inventory(item));
            dec_inv_item_quantity(item.link, 1);
            break;

        default:
            did_work = false;
            unevokable = true;
            break;
        }
        if (did_work)
            count_action(CACT_EVOKE, EVOC_MISC);
        break;

    default:
        unevokable = true;
        break;
    }

    if (!did_work)
        canned_msg(MSG_NOTHING_HAPPENS);
    else if (pract > 0)
        practise(EX_DID_EVOKE_ITEM, pract);

    if (ident && !item_type_known(item))
    {
        set_ident_type(item.base_type, item.sub_type, ID_KNOWN_TYPE);
        set_ident_flags(item, ISFLAG_KNOW_TYPE);

        mprf(gettext("You are wielding %s."),
             item.name(true, DESC_A).c_str());

        you.wield_change = true;
    }

    if (!unevokable)
        you.turn_is_over = true;
    else
        crawl_state.zero_turns_taken();

    return (did_work);
}
