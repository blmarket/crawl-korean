/**
 * @file
 * @brief Self-enchantment spells.
**/

#include "AppHdr.h"

#include "spl-selfench.h"
#include "externs.h"

#include "areas.h"
#include "delay.h"
#include "env.h"
#include "godconduct.h"
#include "hints.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "shout.h"
#include "spl-cast.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "stuff.h"
#include "transform.h"
#include "view.h"

int allowed_deaths_door_hp(void)
{
    int hp = you.skill(SK_NECROMANCY) / 2;

    if (you.religion == GOD_KIKUBAAQUDGHA && !player_under_penance())
        hp += you.piety / 15;

    return max(hp, 1);
}

spret_type cast_deaths_door(int pow, bool fail)
{
    if (you.is_undead)
        mpr("당신은 이미 죽은 몸이다!");//mpr("You're already dead!");
    else if (you.duration[DUR_EXHAUSTED])
        mpr("당신이 죽음의 문에 들어가기에는 너무 지쳐있다!");//mpr("You are too exhausted to enter Death's door!");
    else if (you.duration[DUR_DEATHS_DOOR])
        mpr("당신의 생존을 향한 애원이 거부되었다.");//mpr("Your appeal for an extension has been denied.");
    else
    {
        fail_check();
        mpr("당신은 현재 무적상태이다!");//mpr("You feel invincible!");
        mpr("당신은 모래시계에서 모래가 흐르는 소리를 들은거같다...",//mpr("You seem to hear sand running through an hourglass...",
            MSGCH_SOUND);

        set_hp(allowed_deaths_door_hp());
        deflate_hp(you.hp_max, false);

        you.set_duration(DUR_DEATHS_DOOR, 10 + random2avg(13, 3)
                                           + (random2(pow) / 10));

        if (you.duration[DUR_DEATHS_DOOR] > 25 * BASELINE_DELAY)
            you.duration[DUR_DEATHS_DOOR] = (23 + random2(5)) * BASELINE_DELAY;
        return SPRET_SUCCESS;
    }

    return SPRET_ABORT;
}

void remove_ice_armour()
{
    mpr("당신의 얼음 갑옷은 녹아 없어졌다.", MSGCH_DURATION);//mpr("Your icy armour melts away.", MSGCH_DURATION);
    you.redraw_armour_class = true;
    you.duration[DUR_ICY_ARMOUR] = 0;
}
//얼음갑옷 관련인듯.
spret_type ice_armour(int pow, bool fail)
{
    if (!player_effectively_in_light_armour())
    {
        mpr("당신은 더 많은 방어구를 입었다.");//mpr("You are wearing too much armour.");
        return SPRET_ABORT;
    }

    if (player_stoneskin() || you.form == TRAN_STATUE)
    {
        mpr("얼음막은 암석 위로는 통하지 않는다.");
        return SPRET_ABORT;
    }

    if (you.duration[DUR_FIRE_SHIELD])
    {
        mpr("당신의 화염 고리는 얼음을 즉시 녹여버릴 것이다.");
        return SPRET_ABORT;
    }

    fail_check();

    if (you.duration[DUR_ICY_ARMOUR])
        mpr("당신의 얼음 갑옷은 두껍다.");//mpr("Your icy armour thickens.");
    else
    {
        if (you.form == TRAN_ICE_BEAST)
            mpr("당신의 얼음으로 된 몸은 더욱 탄력적임을 느꼈다.");//mpr("Your icy body feels more resilient.");
        else
            mpr("얼음으로 된 가죽이 당신의 몸을 뒤덮는다!");//mpr("A film of ice covers your body!");

        you.redraw_armour_class = true;
    }

    you.increase_duration(DUR_ICY_ARMOUR, 20 + random2(pow) + random2(pow), 50,
                          NULL);

    return SPRET_SUCCESS;
}

spret_type missile_prot(int pow, bool fail)
{
    fail_check();
    you.increase_duration(DUR_REPEL_MISSILES, 8 + roll_dice(2, pow), 100,
                          "당신은 투사체들로부터 보호받는 것을 느꼈다.");//"You feel protected from missiles.");
    return SPRET_SUCCESS;
}

spret_type deflection(int pow, bool fail)
{
    fail_check();
    you.increase_duration(DUR_DEFLECT_MISSILES, 15 + random2(pow), 100,
                          "당신은 투사체들로부터 매우 안전함을 인지했다.");//"You feel very safe from missiles.");
    return SPRET_SUCCESS;
}

void remove_regen(bool divine_ability)
{
    mpr("당신의 피부는 우글거림을 멈췄다.", MSGCH_DURATION);//mpr("Your skin stops crawling.", MSGCH_DURATION);
    you.duration[DUR_REGENERATION] = 0;
    if (divine_ability)
    {
        mpr("당신은 적대적인 마법 효과에 대한 저항력을 잃었다.", MSGCH_DURATION);//mpr("You feel less resistant to hostile enchantments.", MSGCH_DURATION);
        you.attribute[ATTR_DIVINE_REGENERATION] = 0;
    }
}

spret_type cast_regen(int pow, bool divine_ability, bool fail)
{
    fail_check();
    you.increase_duration(DUR_REGENERATION, 5 + roll_dice(2, pow / 3 + 1), 100,
                          "당신의 피부는 우글거린다");//"Your skin crawls.");

    if (divine_ability)
    {
        mpr("당신은 적대적인 마법 효과에 대한 저항력을 느꼇다..");//mpr("You feel resistant to hostile enchantments.");
        you.attribute[ATTR_DIVINE_REGENERATION] = 1;
    }
    return SPRET_SUCCESS;
}

spret_type cast_revivification(int pow, bool fail)
{
    if (you.hp == you.hp_max)
        canned_msg(MSG_NOTHING_HAPPENS);
    else if (you.hp_max < 21)
        mpr("당신은 이 마법을 영창할 체력이 모자라다.");//mpr("You lack the resilience to cast this spell.");
    else
    {
        fail_check();
        mpr("당신의 신체는 굉장히 고통스런 방법으로 치료되었다.");//mpr("Your body is healed in an amazingly painful way.");

        int loss = 2;
        for (int i = 0; i < 9; ++i)
            if (x_chance_in_y(8, pow))
                loss++;

        dec_max_hp(loss * you.hp_max / 100);
        set_hp(you.hp_max);

        if (you.duration[DUR_DEATHS_DOOR])
        {
            mpr("당신의 삶은 다시한번 당신의 손에 쥐어졌다.", MSGCH_DURATION);//mpr("Your life is in your own hands once again.", MSGCH_DURATION);
            // XXX: better cause name?
            paralyse_player("Death's Door abortion", 5 + random2(5));
            confuse_player(10 + random2(10));
            you.duration[DUR_DEATHS_DOOR] = 0;
        }
        return SPRET_SUCCESS;
    }

    return SPRET_ABORT;
}

spret_type cast_swiftness(int power, bool fail)
{
    if (you.form == TRAN_TREE)
    {
        canned_msg(MSG_CANNOT_MOVE);
        return SPRET_ABORT;
    }

    if (you.in_water())
    {
        mpr("물에서 거품이 일었다!");
        return SPRET_ABORT;
    }

    if (you.liquefied_ground())
    {
        mpr("액화된 땅에서 거품이 일었다!");
        return SPRET_ABORT;
    }

    if (!you.duration[DUR_SWIFTNESS] && player_movement_speed() <= 6)
    {
        mpr("당신은 지금보다 더 빨라질 수 없다.");//mpr("You can't move any more quickly.");
        return SPRET_ABORT;
    }

    fail_check();

    // [dshaligram] Removed the on-your-feet bit.  Sounds odd when
    // you're flying, for instance.
    you.increase_duration(DUR_SWIFTNESS, 20 + random2(power), 100,
                          "당신은 민첩해짐을 느꼈다.");//"You feel quick.");
    did_god_conduct(DID_HASTY, 8, true);

    return SPRET_SUCCESS;
}

spret_type cast_fly(int power, bool fail)
{
    if (you.form == TRAN_TREE)
    {
        mpr("뿌리가 박혀 날아오를 수 없다.", MSGCH_WARN);
        return SPRET_ABORT;
    }

    if (you.liquefied_ground())
    {
        mpr("이런 작은 마법은 중력으로부터 당신을 당길 수 없다!", MSGCH_WARN);//mpr("Such puny magic can't pull you from the ground!", MSGCH_WARN);
        return SPRET_ABORT;//그런 후잡한 마법으로는 레다쨩의 마법을 벗어날 수 없다능! by 군발트
    }

    fail_check();
    const int dur_change = 25 + random2(power) + random2(power);
    const bool was_flying = you.airborne();

    you.increase_duration(DUR_FLIGHT, dur_change, 100);
    you.attribute[ATTR_FLIGHT_UNCANCELLABLE] = 1;

    if (!was_flying)
        float_player();
    else
        mpr("당신은 부력을 느꼈다.");//mpr("You feel more buoyant.");
    return SPRET_SUCCESS;
}

spret_type cast_teleport_control(int power, bool fail)
{
    fail_check();
    if (allow_control_teleport(true))
        mpr("당신은 이제 공간이동을 제어할 수 있다.");
    else
        mpr("여기서는 공간이동을 제어할 수 없을 것 같다.");

    if (you.duration[DUR_TELEPORT] && !player_control_teleport())
    {
        mpr(_("You feel your translocation being delayed."));
        you.increase_duration(DUR_TELEPORT, 1 + random2(3));
    }

    you.increase_duration(DUR_CONTROL_TELEPORT, 10 + random2(power), 50);

    return SPRET_SUCCESS;
}

int cast_selective_amnesia(string *pre_msg)
{
    if (you.spell_no == 0)
    {
        canned_msg(MSG_NO_SPELLS);
        return 0;
    }

    int keyin = 0;
    spell_type spell;
    int slot;

    // Pick a spell to forget.
    while (true)
    {
        mpr("어느 마법을 기억에서 지울 것인가 ([?*] 목록 [ESC] 취소)? ", MSGCH_PROMPT);//mpr("Forget which spell ([?*] list [ESC] exit)? ", MSGCH_PROMPT);
        keyin = get_ch();

        if (key_is_escape(keyin))
        {
            canned_msg(MSG_OK);
            return -1;
        }

        if (keyin == '?' || keyin == '*')
        {
            keyin = list_spells(false, false, false);
            redraw_screen();
        }

        if (!isaalpha(keyin))
        {
            mesclr();
            continue;
        }

        spell = get_spell_by_letter(keyin);
        slot = get_spell_slot_by_letter(keyin);

        if (spell == SPELL_NO_SPELL)
            mpr("당신은 그 마법을 모른다");//mpr("You don't know that spell.");
        else
            break;
    }

    if (pre_msg)
        mpr(pre_msg->c_str());

    const int ep_gain = spell_mana(spell);
    del_spell_from_memory_by_slot(slot);

    if (ep_gain > 0)
    {
        inc_mp(ep_gain);
        mpr("주문을 해방하여 그 잠재에너지를 당신에게 되돌린다");//mpr("The spell releases its latent energy back to you as "
            //"it unravels.");by크롤러.
    }

    return 1;
}

spret_type cast_infusion(int pow, bool fail)
{
    fail_check();
    if (!you.duration[DUR_INFUSION])
        mpr("Your attacks are magically infused.");
    else
        mpr("Your attacks are magically infused for longer.");

    you.increase_duration(DUR_INFUSION,  8 + roll_dice(2, pow), 100);
    you.props["infusion_power"] = pow;

    return SPRET_SUCCESS;
}

spret_type cast_song_of_slaying(int pow, bool fail)
{
    fail_check();

    if (you.duration[DUR_SONG_OF_SLAYING])
        mpr("You start a new song!");
    else
        mpr("You start singing a song of slaying.");

    you.increase_duration(DUR_SONG_OF_SLAYING, 20 + pow / 3, 20 + pow / 3);

    noisy(12, you.pos());

    you.props["song_of_slaying_bonus"] = 0;
    return SPRET_SUCCESS;
}

spret_type cast_song_of_shielding(int pow, bool fail)
{
    fail_check();
    you.increase_duration(DUR_SONG_OF_SHIELDING, 10 + random2(pow) / 3, 40);
    mpr("You are being protected by your magic.");
    return SPRET_SUCCESS;
}

spret_type cast_silence(int pow, bool fail)
{
    fail_check();
    mpr("깊은 정적이 당신을 감쌌다.");

    you.increase_duration(DUR_SILENCE, 10 + pow/4 + random2avg(pow/2, 2), 100);
    invalidate_agrid(true);

    if (you.beheld())
        you.update_beholders();

    learned_something_new(HINT_YOU_SILENCE);
    return SPRET_SUCCESS;
}

spret_type cast_liquefaction(int pow, bool fail)
{
    if (!you.stand_on_solid_ground())
    {
        if (!you.ground_level())
            mpr("당신은 땅에 닿지 않은 상태로 이 마법을 영창할 수 없다.");//mpr("You can't cast this spell without touching the ground.");
        else
            mpr("이 주문을 외우기 위해선 빈 공간의 땅이 필요하다.");
        return SPRET_ABORT;
    }

    if (you.duration[DUR_LIQUEFYING] || liquefied(you.pos()))
    {
        mpr("이 땅은 이미 액화되었다. 기다려야 할 것이다.");
        return SPRET_ABORT;
    }

    fail_check();
    flash_view_delay(BROWN, 80);
    flash_view_delay(YELLOW, 80);
    flash_view_delay(BROWN, 140);

    mpr("당신 주변의 땅은 액화상태로 바뀌었다!");//mpr("The ground around you becomes liquefied!");

    you.increase_duration(DUR_LIQUEFYING, 10 + random2avg(pow, 2), 100);
    invalidate_agrid(true);
    return SPRET_SUCCESS;
}

spret_type cast_shroud_of_golubria(int pow, bool fail)
{
    fail_check();
    if (you.duration[DUR_SHROUD_OF_GOLUBRIA])
        mpr("당신은 당신의 장막을 갱신시켰다.");//mpr("You renew your shroud.");
    else
        mpr("당신의 몸은 공간 왜곡의 얇은 장막으로 뒤덮였다.");//mpr("Space distorts slightly along a thin shroud covering your body.");

    you.increase_duration(DUR_SHROUD_OF_GOLUBRIA, 7 + roll_dice(2, pow), 50);
    return SPRET_SUCCESS;
}

spret_type cast_transform(int pow, transformation_type which_trans, bool fail)
{
    if (!transform(pow, which_trans, false, true))
        return SPRET_ABORT;

    fail_check();
    transform(pow, which_trans);
    return SPRET_SUCCESS;
}
