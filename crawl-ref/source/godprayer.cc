#include "AppHdr.h"

#include "godprayer.h"

#include <cmath>

#include "areas.h"
#include "artefact.h"
#include "coordit.h"
#include "database.h"
#include "effects.h"
#include "env.h"
#include "food.h"
#include "fprop.h"
#include "godabil.h"
#include "goditem.h"
#include "godpassive.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "player.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "monster.h"
#include "notes.h"
#include "options.h"
#include "random.h"
#include "religion.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "view.h"

static bool _offer_items();
static void _zin_donate_gold();

static bool _confirm_pray_sacrifice(god_type god)
{
    if (Options.stash_tracking == STM_EXPLICIT && is_stash(you.pos()))
    {
        mpr("명백한 표식이 있는 물건은 제물로 바칠 수 없다.");
        return (false);
    }

    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (god_likes_item(god, *si)
            && needs_handle_warning(*si, OPER_PRAY))
        {
            std::string prompt = "정말로 ";
            prompt += si->name(true, DESC_NOCAP_A);
            prompt += "가 포함된 아이템 묶음을 제물로 바칩니까?";

            if (!yesno(prompt.c_str(), false, 'n'))
                return (false);
        }
    }

    return (true);
}

std::string god_prayer_reaction()
{
    std::string result = god_name(you.religion).c_str();
    if (crawl_state.player_is_dead())
        result += "은(는) ";
    else
        result += "은(는) ";
    result +=
        (you.piety > 130) ? "당신의 숭배를 자랑스럽게 여기고 있다" :
        (you.piety > 100) ? "당신에게 지극히 만족하고 있다" :
        (you.piety >  70) ? "당신에게 크게 만족하고 있다" :
        (you.piety >  40) ? "당신에게 꽤나 만족하고 있다" :
        (you.piety >  20) ? "당신에게 만족하고 있다" :
        (you.piety >   5) ? "당신에게 특별한 관심을 가지고 있지 않다"
                          : "당신에게 불만이 있다";
    result += ".";

    return (result);
}

bool god_accepts_prayer(god_type god)
{
    if (god_likes_fresh_corpses(god))
        return (true);

    switch (god)
    {
    case GOD_ZIN:
        return (zin_sustenance(false));

    case GOD_JIYVA:
        return (jiyva_can_paralyse_jellies());

    case GOD_ELYVILON:
    case GOD_BEOGH:
    case GOD_NEMELEX_XOBEH:
    case GOD_ASHENZARI:
        return (true);

    default:
        break;
    }

    return (false);
}

static bool _bless_weapon(god_type god, brand_type brand, int colour)
{
    item_def& wpn = *you.weapon();

    if (wpn.base_type != OBJ_WEAPONS
        || (is_range_weapon(wpn) && brand != SPWPN_HOLY_WRATH)
        || is_artefact(wpn))
    {
        return (false);
    }

    std::string prompt = "정말로 " + wpn.name(true, DESC_NOCAP_YOUR)
                       + " ";
    if (brand == SPWPN_PAIN)
        prompt += "에 피로 물든 고통을 부여하겠는가";
    else if (brand == SPWPN_DISTORTION)
        prompt += "에 타락과 왜곡의 속성을 부여하겠는가";
    else
        prompt += "에 신성한 축복의 속성을 부여하겠는가";
    prompt += "?";

    if (!yesno(prompt.c_str(), true, 'n'))
        return (false);

    you.duration[DUR_WEAPON_BRAND] = 0;     // just in case

    std::string old_name = wpn.name(true, DESC_NOCAP_A);
    set_equip_desc(wpn, ISFLAG_GLOWING);
    set_item_ego_type(wpn, OBJ_WEAPONS, brand);
    wpn.colour = colour;

    const bool is_cursed = wpn.cursed();

    enchant_weapon(ENCHANT_TO_HIT, true, wpn);

    if (coinflip())
        enchant_weapon(ENCHANT_TO_HIT, true, wpn);

    enchant_weapon(ENCHANT_TO_DAM, true, wpn);

    if (coinflip())
        enchant_weapon(ENCHANT_TO_DAM, true, wpn);

    if (is_cursed)
        do_uncurse_item(wpn, false);

    if (god == GOD_SHINING_ONE)
    {
        convert2good(wpn);

        if (is_blessed_convertible(wpn))
        {
            origin_acquired(wpn, GOD_SHINING_ONE);
            wpn.flags |= ISFLAG_BLESSED_WEAPON;
        }

        burden_change();
    }
    else if (is_evil_god(god))
    {
        convert2bad(wpn);

        wpn.flags &= ~ISFLAG_BLESSED_WEAPON;

        burden_change();
    }

    you.wield_change = true;
    you.num_current_gifts[god]++;
    you.num_total_gifts[god]++;
    std::string desc  = old_name + " ";
            desc += (god == GOD_SHINING_ONE   ? "에 '샤이닝 원'의 축복이 더해졌다" :
                     god == GOD_LUGONU        ? "에 '루고누'의 타락이 더해졌다" :
                     god == GOD_KIKUBAAQUDGHA ? "에 '키쿠바쿠드하'의 피의 고통이 더해졌다"
                                              : "은(는) 신의 힘에 휩싸였다");

    take_note(Note(NOTE_ID_ITEM, 0, 0,
              wpn.name(true, DESC_NOCAP_A).c_str(), desc.c_str()));
    wpn.flags |= ISFLAG_NOTED_ID;

    mpr("당신의 무기는 밝게 빛났다!", MSGCH_GOD);

    flash_view(colour);

	simple_god_message(god == GOD_SHINING_ONE   ? "은(는) 말했다. \"이 무기를 부디 현명하게 사용하라!\"" :
					   god == GOD_LUGONU        ? "은(는) 말했다. \"이 무기로 모든 것을 타락시키라!\"" :
					   god == GOD_KIKUBAAQUDGHA ? "은(는) 말했다. \"이 무기로 모두에 고통을 선사하라!\"" :
											      "은(는) 말했다. \"이 무기를 잘 써라!\"");

    if (god == GOD_SHINING_ONE)
    {
        holy_word(100, HOLY_WORD_TSO, you.pos(), true);

        // Un-bloodify surrounding squares.
        for (radius_iterator ri(you.pos(), 3, true, true); ri; ++ri)
            if (is_bloodcovered(*ri))
                env.pgrid(*ri) &= ~(FPROP_BLOODY);
    }

    if (god == GOD_KIKUBAAQUDGHA)
    {
        you.gift_timeout = 0; // no protection during pain branding weapon

        torment(&you, TORMENT_KIKUBAAQUDGHA, you.pos());

        // Bloodify surrounding squares (75% chance).
        for (radius_iterator ri(you.pos(), 2, true, true); ri; ++ri)
            if (!one_chance_in(4))
                maybe_bloodify_square(*ri);
    }

#ifndef USE_TILE
    // Allow extra time for the flash to linger.
    delay(1000);
#endif

    return (true);
}

// Prayer at your god's altar.
static bool _altar_prayer()
{
    // Different message from when first joining a religion.
    mpr("당신은 제단에 무릎꿇고, 기도를 드렸다.");

    god_acting gdact;

    bool did_bless = false;

    // donate gold to gain piety distributed over time
    if (you.religion == GOD_ZIN)
        _zin_donate_gold();

    // TSO blesses weapons with holy wrath, and long blades and demon
    // whips specially.
    if (you.religion == GOD_SHINING_ONE
        && !you.num_total_gifts[GOD_SHINING_ONE]
        && !player_under_penance()
        && you.piety > 160)
    {
        item_def *wpn = you.weapon();

        if (wpn
            && (get_weapon_brand(*wpn) != SPWPN_HOLY_WRATH
                || is_blessed_convertible(*wpn)))
        {
            did_bless = _bless_weapon(GOD_SHINING_ONE, SPWPN_HOLY_WRATH,
                                      YELLOW);
        }
    }

    // Lugonu blesses weapons with distortion.
    if (you.religion == GOD_LUGONU
        && !you.num_total_gifts[GOD_LUGONU]
        && !player_under_penance()
        && you.piety > 160)
    {
        item_def *wpn = you.weapon();

        if (wpn && get_weapon_brand(*wpn) != SPWPN_DISTORTION)
            did_bless = _bless_weapon(GOD_LUGONU, SPWPN_DISTORTION, MAGENTA);
    }

    // Kikubaaqudgha blesses weapons with pain, or gives you a Necronomicon.
    if (you.religion == GOD_KIKUBAAQUDGHA
        && !you.num_total_gifts[GOD_KIKUBAAQUDGHA]
        && !player_under_penance()
        && you.piety > 160)
    {
        simple_god_message(
            "은(는) 당신의 무기에 피의 고통을 부여하거나, 혹은 강령술의 비서를 내려줄 것이다.");

        bool kiku_did_bless_weapon = false;

        item_def *wpn = you.weapon();

        // Does the player want a pain branding?
        if (wpn && get_weapon_brand(*wpn) != SPWPN_PAIN)
        {
            kiku_did_bless_weapon =
                _bless_weapon(GOD_KIKUBAAQUDGHA, SPWPN_PAIN, RED);
            did_bless = kiku_did_bless_weapon;
        }
        else
            mpr("피의 고통을 부여할 무기가 없다.");

        // If not, ask if the player wants a Necronomicon.
        if (!kiku_did_bless_weapon)
        {
            if (!yesno("마법서 '강령술의 비서'를 받습니까?", true, 'n'))
                return (false);

            int thing_created = items(1, OBJ_BOOKS, BOOK_NECRONOMICON, true, 1,
                                      MAKE_ITEM_RANDOM_RACE,
                                      0, 0, you.religion);

            if (thing_created == NON_ITEM)
                return (false);

            move_item_to_grid(&thing_created, you.pos());

            if (thing_created != NON_ITEM)
            {
                simple_god_message("은(는) 당신에게 선물을 하사했다!");
                more();

                you.num_current_gifts[you.religion]++;
                you.num_total_gifts[you.religion]++;
                did_bless = true;
                take_note(Note(NOTE_GOD_GIFT, you.religion));
                mitm[thing_created].inscription = "god gift";
            }
        }

        // Return early so we don't offer our Necronomicon to Kiku.
        return (did_bless);
    }

    return (did_bless);
}

void pray()
{
    if (silenced(you.pos()))
    {
        mpr("당신은 소리를 낼 수 없다!");
        return;
    }

    // only successful prayer takes time
    you.turn_is_over = false;

    bool something_happened = false;
    const god_type altar_god = feat_altar_god(grd(you.pos()));
    if (altar_god != GOD_NO_GOD)
    {
        if (!player_can_reach_floor("altar"))
            return;

        if (you.religion != GOD_NO_GOD && altar_god == you.religion)
        {
            something_happened = _altar_prayer();
            // at least "prostrating" took time
            you.turn_is_over = true;
        }
        else if (altar_god != GOD_NO_GOD)
        {
            if (you.species == SP_DEMIGOD)
            {
                mpr("유감이지만, 반신족인 당신은 이 곳에서 기도를 통해 신을 섬길 수 없다.");
                return;
            }

            god_pitch(feat_altar_god(grd(you.pos())));
            you.turn_is_over = true;
            return;
        }
    }

    if (you.religion == GOD_NO_GOD)
    {
        const mon_holy_type holi = you.holiness();
        mprf(MSGCH_PRAY,
             "당신은 잠깐, %s",
             holi == MH_NONLIVING || holi == MH_UNDEAD ? "존재의 의미에 대해 생각했다." : "삶의 의미에 대해 생각했다.");

        // Zen meditation is timeless.
        return;
    }

    mprf(MSGCH_PRAY, "당신은 %s에게 기도를 %s드렸다.",
         god_name(you.religion).c_str(),
	 you.duration[DUR_JELLY_PRAYER] ? "이어서 " : "");

    switch(you.religion)
    {
    case GOD_ZIN:
        //jmf: this "good" god will feed you (a la Nethack)
        if (do_zin_sustenance())
            something_happened = true;
        break;

    case GOD_JIYVA:
        you.duration[DUR_JELLY_PRAYER] = 200;

        if (jiyva_can_paralyse_jellies())
            jiyva_paralyse_jellies();
        something_happened = true;
        break;

    case GOD_FEDHAS:
        if (fedhas_fungal_bloom())
            something_happened = true;
        break;

    default:
        ;
    }

    // All sacrifices affect items you're standing on.
    something_happened |= _offer_items();

    if (you.religion == GOD_XOM)
        mpr(getSpeakString("Xom prayer"), MSGCH_GOD);
    else if (player_under_penance())
        simple_god_message("은(는) 속죄를 요구하고 있다!");
    else
        mpr(god_prayer_reaction().c_str(), MSGCH_PRAY, you.religion);

    if (something_happened)
        you.turn_is_over = true;
    dprf("piety: %d (-%d)", you.piety, you.piety_hysteresis);
}

static int _gold_to_donation(int gold)
{
    return static_cast<int>((gold * log((float)gold)) / MAX_PIETY);
}

static void _zin_donate_gold()
{
    if (you.gold == 0)
    {
        mpr("당신은 제물로 바칠 것이 없다.");
        return;
    }

    if (!yesno("소지금의 절반을, 제단에 기부합니까?", true, 'n'))
        return;

    const int donation_cost = (you.gold / 2) + 1;
    const int donation = _gold_to_donation(donation_cost);

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_SACRIFICE) || defined(DEBUG_PIETY)
    mprf(MSGCH_DIAGNOSTICS, "A donation of $%d amounts to an "
         "increase of piety by %d.", donation_cost, donation);
#endif
    // Take a note of the donation.
    take_note(Note(NOTE_DONATE_MONEY, donation_cost));

    you.attribute[ATTR_DONATIONS] += donation_cost;

    you.del_gold(donation_cost);

    if (donation < 1)
    {
        simple_god_message("은(는) 당신의 가난을 가엾어했다.");
        return;
    }

    you.duration[DUR_PIETY_POOL] += donation;
    if (you.duration[DUR_PIETY_POOL] > 30000)
        you.duration[DUR_PIETY_POOL] = 30000;

    const int estimated_piety =
        std::min(MAX_PENANCE + MAX_PIETY,
                 you.piety + you.duration[DUR_PIETY_POOL]);

    if (player_under_penance())
    {
        if (estimated_piety >= you.penance[GOD_ZIN])
            mpr("'진'은 당신의 죄를 용서해주었다.");
        else
            mpr("당신의 죄가, 좀 더 가벼워졌음을 느꼈다.");
        return;
    }
    std::string result = "진은 당신";

    result +=
        (estimated_piety > 130) ? "의 숭배를 자랑스럽게 여겼다" :
        (estimated_piety > 100) ? "에게 지극히 만족했다" :
        (estimated_piety >  70) ? "에게 크게 만족했다" :
        (estimated_piety >  40) ? "에게 꽤 만족했다" :
        (estimated_piety >  20) ? "에게 만족했다" :
        (estimated_piety >   5) ? "에게 관심을 보이지 않았다"
                                : "에게 화를 냈다";
    result += (donation >= 30 && you.piety <= 170) ? "!" : ".";

    mpr(result.c_str());
}

static int _leading_sacrifice_group()
{
    int weights[5];
    get_pure_deck_weights(weights);
    int best_i = -1, maxweight = -1;
    for (int i = 0; i < 5; ++i)
    {
        if (best_i == -1 || weights[i] > maxweight)
        {
            maxweight = weights[i];
            best_i = i;
        }
    }
    return best_i;
}

static void _give_sac_group_feedback(int which)
{
    ASSERT(which >= 0 && which < 5);
    const char* names[] = {
        "탈출", "파괴", "던전", "소환", "불가사의"
    };
    mprf(MSGCH_GOD, "눈앞에 있던 %s의 상징이 융합했고, 곧 사라졌다.",
         names[which]);
}

static void _ashenzari_sac_scroll(const item_def& item)
{
    int scr = (you.species == SP_FELID) ? SCR_CURSE_JEWELLERY :
              random_choose(SCR_CURSE_WEAPON, SCR_CURSE_ARMOUR,
                            SCR_CURSE_JEWELLERY, -1);
    int it = items(0, OBJ_SCROLLS, scr, true, 0, MAKE_ITEM_NO_RACE,
                   0, 0, GOD_ASHENZARI);
    if (it == NON_ITEM)
    {
        mpr("세상을 등진 느낌이 들었다.");
        return;
    }

    mitm[it].quantity = 1;
    if (!move_item_to_grid(&it, you.pos(), true))
        destroy_item(it, true); // can't happen
}

// Is the destroyed weapon valuable enough to gain piety by doing so?
// Unholy and evil weapons are handled specially.
static bool _destroyed_valuable_weapon(int value, int type)
{
    // Artefacts, including most randarts.
    if (random2(value) >= random2(250))
        return (true);

    // Medium valuable items are more likely to net piety at low piety,
    // more so for missiles, since they're worth less as single items.
    if (random2(value) >= random2((type == OBJ_MISSILES) ? 10 : 100)
        && one_chance_in(1 + you.piety / 50))
    {
        return (true);
    }

    // If not for the above, missiles shouldn't yield piety.
    if (type == OBJ_MISSILES)
        return (false);

    // Weapons, on the other hand, are always acceptable to boost low
    // piety.
    if (you.piety < piety_breakpoint(0) || player_under_penance())
        return (true);

    return (false);
}

static piety_gain_t _sac_corpse(const item_def& item)
{
#ifdef NEW_OKAWARU_PIETY
    if (you.religion == GOD_OKAWARU)
    {
        monster dummy;
        dummy.type = (monster_type)(item.orig_monnum ? item.orig_monnum - 1 : item.plus);
        if (item.props.exists(MONSTER_HIT_DICE))
            dummy.hit_dice = item.props[MONSTER_HIT_DICE].get_short();
        if (item.props.exists(MONSTER_NUMBER))
            dummy.number   = item.props[MONSTER_NUMBER].get_short();
        define_monster(&dummy);
        int gain = get_fuzzied_monster_difficulty(&dummy);
        dprf("fuzzied corpse difficulty: %4.2f", gain*0.01);

        gain_piety(gain, 700);
        gain = div_rand_round(gain, 700);
        return (gain <= 0) ? PIETY_NONE : (gain < 4) ? PIETY_SOME : PIETY_LOTS;
    }
#endif

    gain_piety(13, 19);

    // The feedback is not accurate any longer on purpose; it only reveals
    // the rate you get piety at.
    return x_chance_in_y(13, 19) ? PIETY_SOME : PIETY_NONE;
}

// God effects of sacrificing one item from a stack (e.g., a weapon, one
// out of 20 arrows, etc.).  Does not modify the actual item in any way.
static piety_gain_t _sacrifice_one_item_noncount(const item_def& item,
       int *js, bool first)
{
    // XXX: this assumes that there's no overlap between
    //      item-accepting gods and corpse-accepting gods.
    if (god_likes_fresh_corpses(you.religion))
        return _sac_corpse(item);

    // item_value() multiplies by quantity.
    const int shop_value = item_value(item) / item.quantity;
    // Since the god is taking the items as a sacrifice, they must have at
    // least minimal value, otherwise they wouldn't be taken.
    const int value = (is_worthless_consumable(item) ? 1 : shop_value);

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_SACRIFICE)
        mprf(MSGCH_DIAGNOSTICS, "Sacrifice item value: %d", value);
#endif

    piety_gain_t relative_piety_gain = PIETY_NONE;
    switch (you.religion)
    {
    case GOD_ELYVILON:
    {
        const bool valuable_weapon =
            _destroyed_valuable_weapon(value, item.base_type);
        const bool unholy_weapon = is_unholy_item(item);
        const bool evil_weapon = is_evil_item(item);

        if (valuable_weapon || unholy_weapon || evil_weapon)
        {
            if (unholy_weapon || evil_weapon)
            {
                const char *desc_weapon = evil_weapon ? "사악한 " : "불경스런 ";

                // Print this in addition to the above!
                if (first)
                {
                    simple_god_message(make_stringf(
                             "은(는) 당신이 그런 %s%s무기를 제거한 것을 기뻐했다%s.",
                             item.quantity == 1 ? "" : "",
                             desc_weapon,
                             item.quantity == 1 ? ""     : "").c_str(),
                             GOD_ELYVILON);
                }
            }

            gain_piety(1);
            relative_piety_gain = PIETY_SOME;
        }
        break;
    }

    case GOD_BEOGH:
    {
        const int item_orig = item.orig_monnum - 1;

        int chance = 4;

        if (item_orig == MONS_SAINT_ROKA)
            chance += 12;
        else if (item_orig == MONS_ORC_HIGH_PRIEST)
            chance += 8;
        else if (item_orig == MONS_ORC_PRIEST)
            chance += 4;

        if (food_is_rotten(item))
            chance--;
        else if (item.sub_type == CORPSE_SKELETON)
            chance -= 2;

        gain_piety(chance, 20);
        if (x_chance_in_y(chance, 20))
            relative_piety_gain = PIETY_SOME;
        break;
    }

    case GOD_NEMELEX_XOBEH:
    {
        if (you.attribute[ATTR_CARD_COUNTDOWN] && x_chance_in_y(value, 800))
        {
            you.attribute[ATTR_CARD_COUNTDOWN]--;
#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_CARDS) || defined(DEBUG_SACRIFICE)
            mprf(MSGCH_DIAGNOSTICS, "Countdown down to %d",
                 you.attribute[ATTR_CARD_COUNTDOWN]);
#endif
        }
        // Nemelex piety gain is fairly fast... at least when you
        // have low piety.
        int piety_change, piety_denom;
        if (item.base_type == OBJ_CORPSES)
        {
            piety_change = 1;
            piety_denom = 2 + you.piety/50;
        }
        else
        {
            piety_change = value/2 + 1;
            if (is_artefact(item))
                piety_change *= 2;
            piety_denom = 30 + you.piety/2;
        }

        gain_piety(piety_change, piety_denom);

        // Preserving the old behaviour of giving the big message for
        // artifacts and artifacts only.
        relative_piety_gain = x_chance_in_y(piety_change, piety_denom) ?
                                is_artefact(item) ?
                                  PIETY_LOTS : PIETY_SOME : PIETY_NONE;

        if (item.base_type == OBJ_FOOD && item.sub_type == FOOD_CHUNK
            || is_blood_potion(item))
        {
            // Count chunks and blood potions towards decks of
            // Summoning.
            you.sacrifice_value[OBJ_CORPSES] += value;
        }
        else if (item.base_type == OBJ_CORPSES)
        {
#if defined(DEBUG_GIFTS) || defined(DEBUG_CARDS) || defined(DEBUG_SACRIFICE)
            mprf(MSGCH_DIAGNOSTICS, "Corpse mass is %d",
                 item_mass(item));
#endif
            you.sacrifice_value[item.base_type] += item_mass(item);
        }
        else
            you.sacrifice_value[item.base_type] += value;
        break;
    }

    case GOD_JIYVA:
    {
        // compress into range 0..250
        const int stepped = stepdown_value(value, 50, 50, 200, 250);
        gain_piety(stepped, 50);
        relative_piety_gain = (piety_gain_t)std::min(2,
                                div_rand_round(stepped, 50));
        jiyva_slurp_bonus(div_rand_round(stepped, 50), js);
        break;
    }

    case GOD_ASHENZARI:
        _ashenzari_sac_scroll(item);
        break;

    default:
        break;
    }

    return (relative_piety_gain);
}

piety_gain_t sacrifice_item_stack(const item_def& item, int *js)
{
    piety_gain_t relative_gain = PIETY_NONE;
    for (int j = 0; j < item.quantity; ++j)
    {
        const piety_gain_t gain = _sacrifice_one_item_noncount(item, js, !j);

        // Update piety gain if necessary.
        if (gain != PIETY_NONE)
        {
            if (relative_gain == PIETY_NONE)
                relative_gain = gain;
            else            // some + some = lots
                relative_gain = PIETY_LOTS;
        }
    }
    return (relative_gain);
}

static bool _check_nemelex_sacrificing_item_type(const item_def& item)
{
    switch (item.base_type)
    {
    case OBJ_ARMOUR:
        return (you.nemelex_sacrificing[NEM_GIFT_ESCAPE]);

    case OBJ_WEAPONS:
    case OBJ_STAVES:
    case OBJ_MISSILES:
        return (you.nemelex_sacrificing[NEM_GIFT_DESTRUCTION]);

    case OBJ_CORPSES:
        return (you.nemelex_sacrificing[NEM_GIFT_SUMMONING]);

    case OBJ_POTIONS:
        if (is_blood_potion(item))
            return (you.nemelex_sacrificing[NEM_GIFT_SUMMONING]);
        return (you.nemelex_sacrificing[NEM_GIFT_WONDERS]);

    case OBJ_FOOD:
        if (item.sub_type == FOOD_CHUNK)
            return (you.nemelex_sacrificing[NEM_GIFT_SUMMONING]);
    // else fall through
    case OBJ_WANDS:
    case OBJ_SCROLLS:
        return (you.nemelex_sacrificing[NEM_GIFT_WONDERS]);

    case OBJ_JEWELLERY:
    case OBJ_BOOKS:
    case OBJ_MISCELLANY:
        return (you.nemelex_sacrificing[NEM_GIFT_DUNGEONS]);

    default:
        return false;
    }
}

static bool _offer_items()
{
    if (you.religion == GOD_NO_GOD || !god_likes_items(you.religion))
        return false;

    if (!_confirm_pray_sacrifice(you.religion))
        return false;

    int i = you.visible_igrd(you.pos());

    god_acting gdact;

    int num_sacced = 0;
    int num_disliked = 0;
    item_def *disliked_item = 0;

    const int old_leading = _leading_sacrifice_group();

    while (i != NON_ITEM)
    {
        item_def &item(mitm[i]);
        const int next = item.link;  // in case we can't get it later.
        const bool disliked = !god_likes_item(you.religion, item);

        if (item_is_stationary(item) || disliked)
        {
            i = next;
            if (disliked)
            {
                num_disliked++;
                disliked_item = &item;
            }
            continue;
        }

        // Skip items you don't want to sacrifice right now.
        if (you.religion == GOD_NEMELEX_XOBEH
            && !_check_nemelex_sacrificing_item_type(item))
        {
            i = next;
            continue;
        }

        // Ignore {!D} inscribed items.
        if (!check_warning_inscriptions(item, OPER_DESTROY))
        {
            mpr("{!D} 문장이 새겨진 아이템을 제물로 바치지 않는다.");
            i = next;
            continue;
        }

        if (god_likes_item(you.religion, item)
            && (item.inscription.find("=p") != std::string::npos))
        {
            const std::string msg =
                  "정말로 " + item.name(true, DESC_NOCAP_A) + "을(를) 제물로 바칩니까?";

            if (!yesno(msg.c_str(), false, 'n'))
            {
                i = next;
                continue;
            }
        }


        piety_gain_t relative_gain = sacrifice_item_stack(item);
        print_sacrifice_message(you.religion, mitm[i], relative_gain);
        item_was_destroyed(mitm[i]);
        destroy_item(i);
        i = next;
        num_sacced++;
    }

    if (num_sacced > 0 && you.religion == GOD_NEMELEX_XOBEH)
    {
        const int new_leading = _leading_sacrifice_group();
        if (old_leading != new_leading || one_chance_in(50))
            _give_sac_group_feedback(new_leading);

#if defined(DEBUG_GIFTS) || defined(DEBUG_CARDS) || defined(DEBUG_SACRIFICE)
        _show_pure_deck_chances();
#endif
    }

    // Explanatory messages if nothing the god likes is sacrificed.
    else if (num_sacced == 0 && num_disliked > 0)
    {
        ASSERT(disliked_item);

        if (item_is_orb(*disliked_item))
            simple_god_message("은(는) 당신이 그 오브의 힘을 지상에서 사용하길 원한다!");
        else if (item_is_rune(*disliked_item))
            simple_god_message("은(는) 당신이 그 룬을 소장하고 자랑스럽게 여기길 원한다!.");
        else if (disliked_item->base_type == OBJ_MISCELLANY
                 && disliked_item->sub_type == MISC_HORN_OF_GERYON)
            simple_god_message("은(는) 그 아이템을 바침으로, 당신의 길이 막히는걸 원하지 않았다.");
        // Zin was handled above, and the other gods don't care about
        // sacrifices.
        else if (god_likes_fresh_corpses(you.religion))
            simple_god_message("은(는) 신선한 시체만을 원한다!");
        else if (you.religion == GOD_BEOGH)
            simple_god_message("은(는) 오크의 시체만을 원한다!");
        else if (you.religion == GOD_NEMELEX_XOBEH)
            if (disliked_item->base_type == OBJ_GOLD)
                simple_god_message("은(는) 금화를 원하지 않는다!");
            else
                simple_god_message("은(는) 카드 덱을 사용하길 원하지, 바치길 원하지 않는다!");
        else if (you.religion == GOD_ASHENZARI)
            simple_god_message("은(는) 저주 해제의 마법 두루마리만 바꿔줄 수 있다.");
    }
    if (num_sacced == 0 && you.religion == GOD_ELYVILON)
        mpr("파괴할 무기가 없다!");

    return (num_sacced > 0);
}
