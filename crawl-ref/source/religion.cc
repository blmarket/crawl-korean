/**
 * @file
 * @brief Misc religion related functions.
**/

#include "AppHdr.h"

#include "religion.h"

#include <algorithm>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cmath>

#include "externs.h"

#include "abl-show.h"
#include "branch.h"
#include "acquire.h"
#include "areas.h"
#include "artefact.h"
#include "attitude-change.h"
#include "beam.h"
#include "chardump.h"
#include "coordit.h"
#include "database.h"
#include "decks.h"
#include "delay.h"
#include "describe.h"
#include "dactions.h"
#include "dgnevent.h"
#include "dlua.h"
#include "effects.h"
#include "env.h"
#include "enum.h"
#include "exercise.h"
#include "files.h"
#include "godabil.h"
#include "goditem.h"
#include "godcompanions.h"
#include "godpassive.h"
#include "godprayer.h"
#include "godwrath.h"
#include "hiscores.h"
#include "invent.h"
#include "itemprop.h"
#include "item_use.h"
#include "items.h"
#include "libutil.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-iter.h"
#include "mon-util.h"
#include "mon-place.h"
#include "mgen_data.h"
#include "mon-stuff.h"
#include "mutation.h"
#include "notes.h"
#include "options.h"
#include "ouch.h"
#include "player.h"
#include "player-stats.h"
#include "shopping.h"
#include "skills.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-miscast.h"
#include "spl-selfench.h"
#include "sprint.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "transform.h"
#include "hints.h"
#include "view.h"
#include "xom.h"

#ifdef DEBUG_RELIGION
#    define DEBUG_DIAGNOSTICS
#    define DEBUG_GIFTS
#    define DEBUG_SACRIFICE
#    define DEBUG_PIETY
#endif

#define PIETY_HYSTERESIS_LIMIT 1

// Item offering messages for the gods:
// & is replaced by "is" or "are" as appropriate for the item.
// % is replaced by "s" or "" as appropriate.
// Text between [] only appears if the item already glows.
// First message is if there's no piety gain; second is if piety gain is
// one; third is if piety gain is more than one.
// FIXME : Total
static const char *_Sacrifice_Messages[NUM_GODS][NUM_PIETY_GAIN] =
{
    // No god
    {
        "은(는) 버그로 사라졌다.",
        "은(는) 버그로 사라졌다.",
        "은(는) 버그로 사라졌다."
    },
    // Zin
    {
        "은(는) 희미하게 빛나더니 사라져버렸다.",
        "은(는) 은색으로 반짝이고는 사라졌다.",
        "은(는) 눈부신 은색으로 번쩍이며 사라졌다.",
    },
    // TSO
    {
        "은(는) 거무칙칙한 금색으로 반짝이고는 사라졌다.",
        "은(는) 빛나는 금색으로 반짝이고는 사라졌다.",
        "은(는) 찬란한 금빛으로 반짝이고는 사자렸다.",
    },
    // Kikubaaqudgha
    {
        "은(는) 눈 깜짝할 사이에 썩어 없어졌다.",
        "은(는) 요란하게 요동치면서 썩어 없어졌다.",
        "은(는) 격렬하게 요동치면서 썩어 없어졌다.",
    },
    // Yredelemnul
    {
        "은(는) 한 줌의 먼지로 천천히 분해되었다.",
        "은(는) 한 줌의 먼지로 분해되었다.",
        "은(는) 한 줌의 먼지로 순식간에 분해되었다.",
    },
    // Xom (no sacrifices) 버그메시지. 정상적인 상황에서 나오지 않음
    {
        "을(를) 좀이 빼앗아 갔다.",
        "을(를) 욕심 많은 츤데레 좀쨔응이 빼앗아서 가지고 놀았다.",
        "은(는) 어케 됐는지 나도 몰라. 라고 좀은 발뺌했다.",
    },
    // Vehumet
    {
        "은(는) 서서히 사라져갔다.",
        "은(는) 불꽃과 함께 사라졌다.",
        "은(는) 폭발과 함께 사라졌다.",
    },
    // Okawaru
    {
        "은(는) 천천히 불타 재로 사라졌다.",
        "은(는) 불꽃에 휩싸여 사라졌다.",
        "은(는) 불꽃의 폭발로 구워져 버렸다.",
    },
    // Makhleb
    {
        "은(는) 두서없이 사라졌다.",
        "은(는) 붉은 불꽃을 내며 타버렸다.",
        "은(는) 피빛의 불꽃을 내며 불타 사라졌다.",
    },
    // Sif Muna
    {
        "은(는) 서서히 사라졌다.",
        "은(는) 잠시 희미한 빛을 내었고, 곧 사라졌다.",
        "은(는) 잠시 반짝였고, 곧 사라졌다.",
    },
    // Trog
    {
        "은(는) 불기둥에 휩싸여 사라졌다.",
        "은(는) 격렬한 불기둥에 휩싸여 사라졌다.",
        "은(는) 굉음을 내며 나타난 불기둥에 휩싸여 사라졌다.",
    },
    // Nemelex
    {
        "은(는) 잠시 반짝이다가 사라졌다.",
        "은(는) 형형색색으로 반짝이다가 사라졌다.",
        "은(는) 기묘한 무지개색으로 반짝이다가 사라졌다.",
    },
    // Elyvilon
    {
        "은(는) 희미한 빛을 내면서, 산산히 부서졌다.",
        "은(는) 밝은 빛을 내면서, 산산히 부서졌다.",
        "은(는) 격렬한 빛을 내면서, 산산히 부서졌다.",
    },
    // Lugonu
    {
        "은(는) 공허 속으로 사라졌다.",
        "은(는) 공허 속으로 집어삼켜졌다.",
        "은(는) 공허 속으로 게걸스럽게 집어삼켜졌다.",
    },
    // Beogh
    {
        "은(는) 천천히 부스러졌다.",
        "은(는) 부스러졌다.",
        "은(는) 산산히 부서졌다.",
    },
    // Jiyva
    {
        "은(는) 점액질로 천천히 용해되었다.",
        "은(는) 점액질로 용해되었다.",
        "은(는) 후루룩 소리를 내며 순식간에 용해되었다.",
    },
    // Fedhas
    {
        "은(는) 천천히 자연 속 한 줌의 흙으로 되돌아갔다.",
        "은(는) 자연 속 한 줌의 흙으로 되돌아가 흡수되었다.",
        "은(는) 순식간에 자연 속 한 줌의 흙으로 되돌아갔다.",
    },
    // Cheibriados (slow god, so better sacrifices are slower)
    {
        "이(가) 순식간에 얼어붙고, 사라졌다.",
        "이(가) 얼어붙었고, 곧 사라졌다.",
        "이(가) 얼어붙고, 천천히 깜박이며 사라졌다.",
    },
    // Ashenzari
    {
        "은(는) 검은 빛으로 깜박였다.",
        " pulsate% black.",          // unused
        " strongly pulsate% black.", // unused
    },
};

/**
 * This corresponds with ::god_abilities, as well as with ::god_lose_power_messages.
 */
const char* god_gain_power_messages[NUM_GODS][MAX_GOD_ABILITIES] =
{
    // no god
    { "", "", "", "", "" },
    // Zin
    { "'진'의 가르침과 섭리를 설교할 수 있다.",
      "'진'의 활력을 얻을 수 있다.",
      "부정한 존재들을 '진'의 이름으로 가둘 수 있다.",
      "",
      "'진'의 이름으로 성지를 생성할 수 있다." },
    // TSO
    { "'샤이닝 원'의 이름으로, 언데드와 악마를 살생함으로써 힘을 얻을 수 있다.",
      "'샤이닝 원'의 성스러운 방패를 사용할 수 있다.",
      "",
      "폭발적인 정화의 불꽃을 시전할 수 있다.",
      "수호천사를 불러낼 수 있다." },
    // Kikubaaqudgha
    { "'키쿠바쿠드하'에게 시체를 하사받을 수 있다.",
      "강령술의 부작용으로부터 '키쿠바쿠드하'의 보호를 받는다.",
      "",
      "지옥의 고통으로부터 '키쿠바쿠드하'의 보호를 받는다.",
      "시체를 희생함으로, 지옥의 고통을 불러올 수 있다." },
    // Yredelemnul
    { "{yred_dead}을 불러 일으킬 수 있다.",
      "당신의 언데드 수하들을 순간적으로 곁으로 소집하고, 당신과 수하들의 고통을 적에게 반사할 수 있다.",
      "[animate {yred_dead}]", //일단보류
      "주변의 생명력을 빨아들일 수 있다.",
      "살아있는 영혼을 지배할 수 있다" },
    // Xom
    { "", "", "", "", "" },
    // Vehumet
    { "'베후메트'의 이름으로 한 살생으로 마력을 얻을 수 있다.",
      "",
      "'베후메트'으로부터 파괴 마법의 도움을 받는다.",
      "'베후메트'의 도움으로 더 먼 거리까지 파괴 마법을 시전할 수 있다.",
      "" },
    // Okawaru
    { "일시적으로 더욱 향상된 전투 기술을 발휘할 수 있다.",
      "",
      "",
      "",
      "당신의 공격 속도를 일시적으로 크게 향상시킬 수 있다." },
    // Makhleb
    { "살육을 통해 생명력을 얻을 수 있다",
      "'마크레브'의 파괴의 힘을 다룰 수 있다.",
      "'마크레브'의 하급 종복을 소환할 수 있다",
      "'마크레브'의 강력한 파괴의 힘을 다룰 수 있다.",
      "'마크레브'의 고위 종복를 소환할 수 있다." },
    // Sif Muna
    { "주변에서 마력을 수집할 수 있다.",
      "새로운 주문을 기억하기 위해 기억을 말소시킬 수 있다",
      "",
      "주문 사용의 부작용으로부터 '시프 무나'의 보호를 받을 수 있다.",
      "" },
    // Trog
    { "당신의 의지대로 광폭화를 할 수 있다.",
      "'트로그'의 힘으로 빠른 재생 능력과 마법 저항을 얻을 수 있다.",
      "",
      "'트로그'의 전사들을 소환할 수 있다.",
      "" },
    // Nemelex
    { "카드를 장비하지 않고도 곧바로 사용할 수 있다.", 
      "카드 덱 중 두 장의 카드를 미리 확인할 수 있다.", 
      "석 장의 카드 중 한 장을 선택하여 사용할 수 있다.", 
      "카드 덱 중 상위 넉 장의 카드를 한 번에 사용할 수 있다.",
      "카드 덱 중 상위 다섯 장의 카드의 순서를 바꿀 수 있다. 덱의 나머지 카드들은 사라진다." },
    // Elyvilon
    { "미약한 치유를 '엘리빌론'에게 요청할 수 있다.",
      "스스로를 정화시킬 수 있다.",
      "강력한 치유를 '엘라이빌론'에게 요청할 수 있다.",
      "",
      "'엘리빌론'의 성스러운 활력을 불러낼 수 있다." },
    // Lugonu
    { "언제든지 어비스를 탈출할 수 있다.",
      "당신 주변의 공간을 왜곡시킬 수 있다.",
      "방해되는 적들을 어비스로 추방시킬 수 있다.",
      "공간 구조를 타락시킬 수 있다.",
      "어느 정도의 대가를 통해 어비스로 곧바로 이동할 수 있다." },
    // Beogh
    { "오크제 장비를 사용함으로써 '베오그'의 도움을 받을 수 있다.",
      "신의 힘으로 적을 징벌할 수 있다.",
      "오크 추종자를 얻을 수 있다.",
      "당신을 따르는 오크 추종자들을 순간적으로 곁으로 소집할 수 있다.",
      "물 위를 걸을 수 있다." },
    // Jiyva
    { "젤리를 요처청할 수 있다",
      "젤리들이 아이템을 먹어치우는 것을 잠시 멈출 수 있다",
      "",
      "적들을 슬라임화시킬 수 있다.",
      "'지이바'의 힘으로, 나쁜 변이를 제거할 수 있다."
    },
    // Fedhas
    { "식물의 진화를 촉진할 수 있다.",
      "던전 내에 한 줄기 햇빛을 불러올 수 있다.",
      "당신 주변을 식물로 둘러쌀 수 있다.",
      "폭발하는 포자를 불러낼 수 있다.",
      "날씨를 변화시킬 수 있다."
    },
    // Cheibriados
    { "주변의 시간의 흐름을 묶어, 느리게 만들 수 있다.",
      "",
      "주변의 시공간을 잠시 비틀 수 있다.",
      "당신보다 빠른 존재들에게 느림의 징벌을 가할 수 있다",
      "시간의 흐름으로부터, 잠시 빠져나올 수 있다"
    },
    // Ashenzari
    { "",
      "스스로를 저주함으로써, 기술에 대한 도움을 받을 수 있다.",
      "'아센자리'를 통해, 당신의 시야와 마음을 맑게 유지할 수 있다.",
      "벽 너머로 투시를 할 수 있다.",
      "당신의 경험이 바탕이 된 기술들을 재구성할 수 있다."
    },
};

/**
 * This corresponds with ::god_abilities, as well as with ::god_gain_power_messages.
 */
const char* god_lose_power_messages[NUM_GODS][MAX_GOD_ABILITIES] =
{
    // no god
    { "", "", "", "", "" },
    // Zin
    { "'진'의 가르침을 설교할 수 없다.",
      "'진'의 활력을 얻을 수 없다.",
      "부정한 존재들을 가둘 수 없다.",
      "",
      "성지를 생성할 수 없다." },
    // TSO
    { "악마나 언데드의 살생으로 힘을 얻을 수 없다.",
      "성스러운 방패를 사용할 수 없다.",
      "",
      "정화의 불꽃을 시전할 수 없다.",
      "수호천사를 불러낼 수 없다." },
    // Kikubaaqudgha
    { "'키쿠바쿠드하'에게 시체를 하사받을 수 없다.",
      "강령술의 부작용으로부터 보호를 받을 수 없다.",
      "",
      "지옥의 고통으로부터 보호를 받을 수 없다.",
      "지옥의 고통을 불러올 수 없다." },
    // Yredelemnul
    { "{yred_dead}을 다룰 수 없다.",
      "언데드 수하들을 소집하거나, 고통을 적에게 반사할 수 없다.",
      "[animate {yred_dead}]", //일단보류
      "생명력을 빨아들일 수 없다.",
      "영혼을 지배할 수 없다" },
    // Xom
    { "", "", "", "", "" },
    // Vehumet
    { "살생으로 마력을 얻을 수 없다.",
      "",
      "파괴 마법의 도움을 받지 못한다.",
      "더 먼 거리까지 파괴 마법을 시전할 수 없다.",
      "" },
    // Okawaru
    { "향상된 전투 기술을 발휘할 수 없다.",
      "",
      "",
      "",
      "공격 속도를 일시적으로 크게 향상시킬 수 없다." },
    // Makhleb
    { "살육을 통해 생명력을 얻을 수 없다",
      "'마크레브'의 파괴의 힘을 다룰 수 없다.",
      "'마크레브'의 하급 종복을 소환할 수 없다",
      "'마크레브'의 강력한 파괴의 힘을 다룰 수 없다.",
      "'마크레브'의 고위 종복를 소환할 수 없다." },
    // Sif Muna
    { "주변에서 마력을 수집할 수 없다.",
      "주문을 망각할 수 없다",
      "",
      "주문 사용의 부작용으로부터 보호를 받을 수 없다.",
      "" },
    // Trog
    { "의지대로 광폭화를 할 수 없다.",
      "빠른 재생 능력과 마법 저항을 얻을 수 없다.",
      "",
      "'트로그'의 전사들을 소환할 수 없다.",
      "" },
    // Nemelex
    { "장비하지 않은 카드를 곧바로 사용할 수 없다.",
      "두 장의 카드를 미리 확인할 수 없다.",
      "석 장의 카드를 선택하여 사용할 수 없다.",
      "넉 장의 카드를 동시에 사용할 수 없다.",
      "다섯 장의 카드를 취할 수 없다." },
    // Elyvilon
    { "미약한 치유를 시전할 수 없다.",
      "스스로를 정화시킬 수 없다.",
      "강력한 치유를 시전할 수 없다.",
      "",
      "성스러운 활력을 불러낼 수 없다." },
    // Lugonu
    { "어비스에서 자유롭게 탈출할 수 없다.",
      "주변 공간을 왜곡시킬 수 없다.",
      "적들을 어비스로 추방시킬 수 없다.",
      "공간 구조를 타락시킬 수 없다.",
      "어비스로 곧바로 이동할 수 없다." },
    // Beogh
    { "오크제 장비를 사용함으로써 보너스를 받을 수 없다.",
      "천벌의 일격을 사용할 수 없다.",
      "오크 추종자를 얻을 수 없다.",
      "오크 추종자들을 소집할 수 없다.",
      "물 위를 걸을 수 없다." },
    // Jiyva
    { "젤리를 요처청할 수 없다",
      "젤리들의 먹성을 통제할 수 없다",
      "",
      "적들을 슬라임화시킬 수 없다.",
      "변이를 제거할 수 없다."
    },
    // Fedhas
    { "진화를 촉진할 수 없다.",
      "햇빛을 불러올 수 없다.",
      "주변을 식물로 둘러쌀 수 없다.",
      "포자를 불러낼 수 없다.",
      "날씨를 변화시킬 수 없다."
    },
    // Cheibriados
    { "주변의 시간의 흐름에 영향을 줄 수 없다.",
      "",
      "주변의 시공간을 비틀 수 없다.",
      "당신보다 빠른 존재들을 벌할 수 없다.",
      "시간의 흐름으로부터 빠져 나올 수 없다."
    },
    // Ashenzari
    { "",
      "스스로를 저주하여도 도움을 받을 수 없다.",
      "당신의 시야와 마음을 맑게 유지할 수 없다.",
      "투시를 할 수 없다.",
      "당신의 된 기술들을 재구성할 수 없다."
    },
};

typedef void (*delayed_callback)(const mgen_data &mg, monster *&mon, int placed);

static void _delayed_monster(const mgen_data &mg,
                             delayed_callback callback = NULL);
static void _delayed_monster_done(string success, string failure,
                                  delayed_callback callback = NULL);
static void _place_delayed_monsters();

bool is_evil_god(god_type god)
{
    return (god == GOD_KIKUBAAQUDGHA
            || god == GOD_MAKHLEB
            || god == GOD_YREDELEMNUL
            || god == GOD_BEOGH
            || god == GOD_LUGONU);
}

bool is_good_god(god_type god)
{
    return (god == GOD_ZIN
            || god == GOD_SHINING_ONE
            || god == GOD_ELYVILON);
}

bool is_chaotic_god(god_type god)
{
    return (god == GOD_XOM
            || god == GOD_MAKHLEB
            || god == GOD_LUGONU
            || god == GOD_JIYVA);
}

bool is_unknown_god(god_type god)
{
    return (god == GOD_NAMELESS);
}

bool is_unavailable_god(god_type god)
{
    if (god == GOD_JIYVA && jiyva_is_dead())
        return true;

    // Don't allow Fedhas in ZotDef, as his invocations are duplicated, and
    // passives thoroughly overpowered.  Protection for plants, speed-up of
    // oklobs, etc...
    // Basically, ZotDef is Fedhas.
    if (god == GOD_FEDHAS && crawl_state.game_is_zotdef())
        return true;

    // No Ashenzari, too -- nothing to explore, can't use his abilities.
    // We could give some piety for every wave, but there's little point.
    if (god == GOD_ASHENZARI && crawl_state.game_is_zotdef())
        return true;

    return false;
}

god_type random_god(bool available)
{
    god_type god;

    do
        god = static_cast<god_type>(random2(NUM_GODS - 1) + 1);
    while (available && is_unavailable_god(god));

    return god;
}

string get_god_powers(god_type which_god)
{
    // Return early for the special cases.
    if (which_god == GOD_NO_GOD)
        return "";

    string result = getLongDescription(god_name(which_god) + " powers");
    return result;
}

string get_god_likes(god_type which_god, bool verbose)
{
    if (which_god == GOD_NO_GOD || which_god == GOD_XOM)
        return "";

    string text = uppercase_first(_(god_name(which_god).c_str()));
    vector<string> likes;
    vector<string> really_likes;

    // Unique/unusual piety gain methods first.
    switch (which_god)
    {
    case GOD_SIF_MUNA:
        likes.push_back("다양한 주문들을 시전하고, 수련하는 것");
        break;

    case GOD_FEDHAS:
        snprintf(info, INFO_SIZE, "%s근처에 있는 시체들의 자연 부패를 " 
                                  "촉진하는 것",
                 verbose ? " <w>기도(p)</w>를 통해, " : "");
        likes.push_back(info);
        break;

    case GOD_TROG:
        snprintf(info, INFO_SIZE, "%s마법서를 불태우는 것",
                 verbose ? "<w>마법서 불태우기 권능(a)</w>을 이용하여 " : "");
        likes.push_back(info);
        break;

    case GOD_NEMELEX_XOBEH:
        snprintf(info, INFO_SIZE, "%s표시되지 않은 카드를 사용하고, 덱을 끝까지 모두 사용하는 것",
                 verbose ? " (<w>장비(w)</w>와 <w>발동(e)</w>을 통해 " 
                         : "");

        likes.push_back(info);
        break;

    case GOD_ELYVILON:
        snprintf(info, INFO_SIZE, "%s무기를 신에게 바쳐 회수하는 것 (특히 불경스럽거나 " 
                                  "사악한 무기)",
                 verbose ? "<w>기도(p)</w>를 통해 ("
                           "<w>!p</w>를 통해 아이템에 문장을 새겨서, 실수로 중요한 아이템을 바치는 것을 방지할 수 있다)"
                         : "");
        likes.push_back(info);
        likes.push_back("적들을 치유함으로 인해, 적들의 적대감을 가라앉히는 것");
        break;

    case GOD_JIYVA:
        snprintf(info, INFO_SIZE, "%s아이템을 바치는 것",
                 verbose ? "슬라임들이 자연스럽게 아이템들을 먹어치우도록 하여 " : "");
        likes.push_back(info);
        break;

    case GOD_CHEIBRIADOS:
        snprintf(info, INFO_SIZE, "%s빠른 존재들을 제거하는 것", 
                 verbose ? "당신보다 더 "
                         : "");
        likes.push_back(info);
        break;

    case GOD_ASHENZARI:
        likes.push_back("여러 지역들을 탐사하는 것 (가급적이면, 저주받은 아이템으로 "
                        "스스로를 구속한 상태로)");
        break;

    case GOD_SHINING_ONE:
        likes.push_back("근절해야 할 필요가 있다고 판단되는 "
                        "존재들을 만나는 것");
        break;

    case GOD_LUGONU:
        likes.push_back("무엇이든 어비스로 추방하는 것");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_ZIN:
        snprintf(info, INFO_SIZE, "%s헌금을 바치는 것", 
                 verbose ? "제단 위에서 <w>기도(p)</w>를 드림으로써 " : ""); 
        likes.push_back(info);
        break;

    case GOD_BEOGH:
        snprintf(info, INFO_SIZE, "%s죽은 오크에게 축복을 내리는 것", 
                 verbose ? "오크의 시체 위에서 <w>기도(p)</w>를 통해 " : ""); 

        likes.push_back(info);
        break;

    case GOD_NEMELEX_XOBEH:
        snprintf(info, INFO_SIZE, "%s아이템을 바치는 것", 
                 verbose ? "아이템 위에서 <w>기도(p)</w>를 함으로써 " : ""); 
        likes.push_back(info);
        break;

    default:
        break;
    }

    if (god_likes_fresh_corpses(which_god))
    {
        snprintf(info, INFO_SIZE, "%s신선한 시체를 제물로 바치는 것", 
                 verbose ? "시체 위해서 <w>기도(p)</w>를 함으로써 " : "");

        likes.push_back(info);
    }

    switch (which_god)
    {
    case GOD_MAKHLEB:
    case GOD_LUGONU:
        likes.push_back("당신 혹은 당신의 동료가 살아있는 생명체를 처치하는 것");
        break;

    case GOD_TROG:
        likes.push_back("당신 혹은 트로그가 내려준 전우들이 살아있는 생명체를 처치하는 것"); 
        break;

    case GOD_YREDELEMNUL:
    case GOD_KIKUBAAQUDGHA:
        likes.push_back("당신 혹은 당신의 언데드 수하가 살아있는 생명체를 처치하는 것");
        break;

    case GOD_BEOGH:
        likes.push_back("당신 혹은 당신의 오크 동료가 살아있는 생명체를 처치하는 것");
        break;

    case GOD_OKAWARU:
    case GOD_VEHUMET:
        likes.push_back("살아있는 생명체를 처치하는 것");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_ZIN:
        likes.push_back("당신 혹은 당신의 동료가 돌연변이 혹은 혼돈의 존재를 제거하는 것"); 
        break;

    case GOD_SHINING_ONE:
        likes.push_back("당신 혹은 당신의 동료가 악마 혹은 언데드를 제거하는 것");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_SHINING_ONE:
    case GOD_MAKHLEB:
    case GOD_LUGONU:
        likes.push_back("당신 혹은 당신의 동료가 언데드를 제거하는 것"); 
        break;

    case GOD_BEOGH:
        likes.push_back("당신 혹은 당신의 동료가 언데드를 제거하는 것"); 
        break;

    case GOD_OKAWARU:
    case GOD_VEHUMET:
        likes.push_back("언데드를 제거하는 것"); 
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_SHINING_ONE:
    case GOD_MAKHLEB:
    case GOD_LUGONU:
        likes.push_back("당신 혹은 당신의 동료가 악마를 처치하는 것"); 
        break;

    case GOD_TROG:
        likes.push_back("당신 혹은 당신의 동료가 악마를 처치하는 것"); 
        break;

    case GOD_KIKUBAAQUDGHA:
        likes.push_back("당신 혹은 당신의 언데드 수하가 악마를 처치하는 것"); 
        break;

    case GOD_BEOGH:
        likes.push_back("당신 혹은 당신의 오크 동료가 악마를 처치하는 것"); 
        break;

    case GOD_OKAWARU:
    case GOD_VEHUMET:
        likes.push_back("악마를 처치하는 것");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_YREDELEMNUL:
        likes.push_back("당신 혹은 당신의 언데드 수하가 인공적인 생명체를 제거하는 것"); 
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_MAKHLEB:
    case GOD_LUGONU:
        likes.push_back("당신 혹은 당신의 동료가 신성한 존재를 처치하는 것");
        break;

    case GOD_TROG:
        likes.push_back("당신 혹은 당신의 언데드 수하가 신성한 존재를 처치하는 것"); 
        break;

    case GOD_YREDELEMNUL:
        likes.push_back("당신 혹은 당신의 언데드 수하가 신성한 존재를 처치하는 것"); 
        likes.push_back("성스러운 존재의 시체를 훼손하는 것");
        break;

    case GOD_KIKUBAAQUDGHA:
        likes.push_back("당신 혹은 당신의 언데드 수하가 신성한 존재를 처치하는 것"); 
        break;

    case GOD_BEOGH:
        likes.push_back("당신 혹은 당신의 오크 동료가 신성한 존재를 처치하는 것"); 
        break;

    case GOD_OKAWARU:
    case GOD_VEHUMET:
        likes.push_back("신성한 존재를 처치하는 것"); 
        break;

    default:
        break;
    }

    // Especially appreciated kills.
    switch (which_god)
    {
    case GOD_YREDELEMNUL:
        really_likes.push_back("신성한 존재를 처치하는 것"); 
        break;

    case GOD_BEOGH:
        really_likes.push_back("다른 신을 믿는 오크 사제를 처치하는 것"); 
        break;

    case GOD_TROG:
        really_likes.push_back("마법을 사용하는 존재를 처치하는 것"); 
        break;

    default:
        break;
    }

    if (likes.empty() && really_likes.empty())
        text += "이(가) 아무것도 좋아하지 않는다고? 그럼 버그일꺼야. 꼭 제보를 해 줘.";
    else
    {
        text += "은(는) ";
        text += comma_separated_line(likes.begin(), likes.end());
        text += "와(과) 같은 행동에 기뻐할 것이다.";

        if (!really_likes.empty())
        {
            text += " 또한, ";
            text += uppercase_first(_(god_name(which_god).c_str()));

            text += "은(는) 특히 ";
            text += comma_separated_line(really_likes.begin(),
                                         really_likes.end());
            text += "와(과) 같은 행동에는 더욱 기뻐할 것이다.";
        }
    }

    return text;
}

string get_god_dislikes(god_type which_god, bool /*verbose*/)
{
    // Return early for the special cases.
    if (which_god == GOD_NO_GOD || which_god == GOD_XOM)
        return "";

    string text;
    vector<string> dislikes;        // Piety loss
    vector<string> really_dislikes; // Penance

    if (god_hates_cannibalism(which_god))
        really_dislikes.push_back("식인 행위");

    if (is_good_god(which_god))
    {
        really_dislikes.push_back("신성한 존재의 시체를 훼손하는 행위");

        if (which_god == GOD_SHINING_ONE)
            really_dislikes.push_back("피를 마시는 행위");
        else
            dislikes.push_back("피를 마시는 행위");

        really_dislikes.push_back("강령술을 사용하는 행위"); 
        really_dislikes.push_back("부정한 주문이나 아이템을 사용하는 행위");
        really_dislikes.push_back("적대적이지 않은 성스러운 존재를 공격하는 행위"); 
        really_dislikes.push_back("당신 혹은 당신의 동료가 적대적이지 않은 신성한 존재를 살해하는 행위");

        if (which_god == GOD_ZIN)
            dislikes.push_back("중립 몬스터를 공격하는 행위");
        else
            really_dislikes.push_back("중립 몬스터를 공격하는 행위");
    }

    switch (which_god)
    {
    case GOD_ZIN:     case GOD_SHINING_ONE:  case GOD_ELYVILON:
    case GOD_OKAWARU:
        really_dislikes.push_back("동료를 공격하는 행위");
        break;

    case GOD_BEOGH:
        really_dislikes.push_back("동료 오크를 공격하는 행위");
        break;

    case GOD_JIYVA:
        really_dislikes.push_back("슬라임/젤리 친구들을 공격하는 행위");
        break;

    case GOD_FEDHAS:
        dislikes.push_back("당신 혹은 당신의 동료가 식물을 손상시키는 행위");
        dislikes.push_back("동료 식물을 죽게 내버려두는 행위");
        really_dislikes.push_back("시체나 시체로부터의 고기,뼈 등에 강령술을 사용하는 행위");
        break;

    case GOD_SIF_MUNA:
        really_dislikes.push_back("마법책을 파괴하는 행위");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_ELYVILON: case GOD_OKAWARU:
        dislikes.push_back("당신의 동료를 죽게 내버려두는 행위");
        break;

    case GOD_ZIN:
        dislikes.push_back("지성이 있는 동료를 죽게 내버려두는 행위");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_ZIN:
        dislikes.push_back("자기 자신을 고의적으로 변이시키는 행위");
        really_dislikes.push_back("다른 존재를 변이시키는 행위");
        really_dislikes.push_back("고의적으로 변신하는 행위");
        really_dislikes.push_back("불경하거나 혼돈스러운 주문이나 아이템을 사용하는 행위");
        really_dislikes.push_back("지성이 있는 존재의 고기를 먹는 행위");
        dislikes.push_back("성지 위에서, 당신 혹은 당신의 동료가 " 
                           "몬스터를 공격하는 행위");
        break;

    case GOD_SHINING_ONE:
        really_dislikes.push_back("몬스터를 중독시키는 행위");
        really_dislikes.push_back("지성이 있는 몬스터를 상대로, " 
                                  "정정당당하지 못한 전투를 하는 행위");
        break;

    case GOD_ELYVILON:
        really_dislikes.push_back("신의 가호를 받는 동안, 살아있는 존재를 " 
                                  "살해하는 행위");
        break;

    case GOD_YREDELEMNUL:
        really_dislikes.push_back("신성한 주문 혹은 아이템을 사용하는 행위");
        break;

    case GOD_TROG:
        really_dislikes.push_back("주문을 기억하는 행위"); 
        really_dislikes.push_back("주문을 외워 마법을 사용하는 행위"); 
        really_dislikes.push_back("마법 관련 기술들을 수련하는 행위"); 
        break;

    case GOD_BEOGH:
        really_dislikes.push_back("오크의 시체를 훼손하는 행위"); 
        really_dislikes.push_back("오크의 석상을 부수는 행위");
        break;

    case GOD_JIYVA:
        really_dislikes.push_back("슬라임을 죽이는 행위");
        break;

    case GOD_CHEIBRIADOS:
        really_dislikes.push_back("자기자신을 고의적으로 가속하는 행위");
        really_dislikes.push_back("비정상적으로 빠른 아이템을 사용하는 행위"); 
        break;

    default:
        break;
    }

    if (dislikes.empty() && really_dislikes.empty())
        return "";

    if (!dislikes.empty())
    {
        text += uppercase_first(_(god_name(which_god).c_str()));
        text += "은(는) ";
        text += comma_separated_line(dislikes.begin(), dislikes.end(),
                                     " 또는 ", ", ");
        text += "을(를) 싫어한다.";

        if (!really_dislikes.empty())
            text += " ";
    }

    if (!really_dislikes.empty())
    {
        text += uppercase_first(_(god_name(which_god).c_str()));
        text += "은(는) 특히, ";
                text += comma_separated_line(really_dislikes.begin(),
                                             really_dislikes.end(),
                                             " 또는 ", ", ");
        text += "을(를) 무척 싫어한다.";
    }

    return text;
}

void dec_penance(god_type god, int val)
{
    if (val <= 0 || you.penance[god] <= 0)
        return;

#ifdef DEBUG_PIETY
    mprf(MSGCH_DIAGNOSTICS, "Decreasing penance by %d", val);
#endif
    if (you.penance[god] <= val)
    {
        you.penance[god] = 0;

        mark_milestone("god.mollify",
                       _(god_name(god).c_str()) + std::string("의 분노를 누그러트림.")); // "mollified " + god_name(god) + ".");

        const bool dead_jiyva = (god == GOD_JIYVA && jiyva_is_dead());

        simple_god_message(
            make_stringf("은(는) 분노를 누그러트린 것 같다.%s",
                         dead_jiyva ? " (그리고 사라졌다)" : "").c_str(),
            god);

        if (dead_jiyva)
            add_daction(DACT_REMOVE_JIYVA_ALTARS);

        take_note(Note(NOTE_MOLLIFY_GOD, god));

        if (you_worship(god))
        {
            // In case the best skill is Invocations, redraw the god
            // title.
            you.redraw_title = true;
        }

        if (you_worship(god))
        {
            // Orcish bonuses are now once more effective.
            if (god == GOD_BEOGH)
                 you.redraw_armour_class = true;
            // TSO's halo is once more available.
            else if (god == GOD_SHINING_ONE
                     && you.piety >= piety_breakpoint(0))
            {
                mpr("신성한 후광이 다시 돌아왔다!");
                invalidate_agrid(true);
            }
            else if (god == GOD_ASHENZARI
                     && you.piety >= piety_breakpoint(2))
            {
                mpr("당신의 감지력이 다시 돌아왔다.");
                autotoggle_autopickup(false);
            }
            else if (god == GOD_CHEIBRIADOS)
            {
                simple_god_message("은(는) 당신의 능력치를 다시금 지원해주기 시작했다.");
                redraw_screen();
                notify_stat_change("mollifying Cheibriados");
            }

            // When you've worked through all your penance, you get
            // another chance to make hostile holy beings good neutral.
            if (is_good_god(god))
                add_daction(DACT_HOLY_NEW_ATTEMPT);
            // When you've worked through all your penance, you get
            // another chance to make hostile slimes strict neutral.
            else if (god == GOD_JIYVA)
                add_daction(DACT_SLIME_NEW_ATTEMPT);
        }
    }
    else if (god == GOD_NEMELEX_XOBEH && you.penance[god] > 100)
    { // Nemelex's penance works actively only until 100
        if ((you.penance[god] -= val) > 100)
            return;
        mark_milestone("god.mollify",
                       _(god_name(god).c_str()) + std::string("의 분노를 일부 누그러트림.")); // "partially mollified " + god_name(god) + ".");
        simple_god_message("은(는) 분노를 누그러뜨린 것 같다... 아마도.", god);
        take_note(Note(NOTE_MOLLIFY_GOD, god));
    }
    else
        you.penance[god] -= val;
}

void dec_penance(int val)
{
    dec_penance(you.religion, val);
}

static bool _need_water_walking()
{
    return (you.ground_level() && you.species != SP_MERFOLK
            && grd(you.pos()) == DNGN_DEEP_WATER);
}

bool jiyva_is_dead()
{
    return (you.royal_jelly_dead
            && !you_worship(GOD_JIYVA) && !you.penance[GOD_JIYVA]);
}

static void _inc_penance(god_type god, int val)
{
    if (val <= 0)
        return;

    if (!player_under_penance(god))
    {
        god_acting gdact(god, true);

        take_note(Note(NOTE_PENANCE, god));

        you.penance[god] += val;
        you.penance[god] = min((uint8_t)MAX_PENANCE, you.penance[god]);

        // Orcish bonuses don't apply under penance.
        if (god == GOD_BEOGH)
        {
            you.redraw_armour_class = true;

            if (_need_water_walking() && !beogh_water_walk())
                fall_into_a_pool(you.pos(), true, grd(you.pos()));
        }
        // Neither does Trog's regeneration or magic resistance.
        else if (god == GOD_TROG)
        {
            if (you.attribute[ATTR_DIVINE_REGENERATION])
                remove_regen(true);

            make_god_gifts_disappear(); // only on level
        }
        // Neither does Zin's divine stamina.
        else if (god == GOD_ZIN)
        {
            if (you.duration[DUR_DIVINE_STAMINA])
                zin_remove_divine_stamina();
        }
        // Neither does TSO's halo or divine shield.
        else if (god == GOD_SHINING_ONE)
        {
            if (you.haloed())
                mpr("당신의 성스러운 후광이 사라졌다.");

            if (you.duration[DUR_DIVINE_SHIELD])
                tso_remove_divine_shield();

            make_god_gifts_disappear(); // only on level
            invalidate_agrid();
        }
        // Neither does Ely's divine vigour.
        else if (god == GOD_ELYVILON)
        {
            if (you.duration[DUR_DIVINE_VIGOUR])
                elyvilon_remove_divine_vigour();
        }
        else if (god == GOD_JIYVA)
        {
            if (you.duration[DUR_SLIMIFY])
                you.duration[DUR_SLIMIFY] = 0;
        }
        else if (god == GOD_CHEIBRIADOS)
        {
            redraw_screen();
            notify_stat_change("falling into Cheibriados' penance");
        }

        if (you_worship(god))
        {
            // In case the best skill is Invocations, redraw the god
            // title.
            you.redraw_title = true;
        }
    }
    else
    {
        you.penance[god] += val;
        you.penance[god] = min((uint8_t)MAX_PENANCE, you.penance[god]);
    }
}

static void _inc_penance(int val)
{
    _inc_penance(you.religion, val);
}

static void _set_penance(god_type god, int val)
{
    you.penance[god] = val;
}

static void _inc_gift_timeout(int val)
{
    if (200 - you.gift_timeout < val)
        you.gift_timeout = 200;
    else
        you.gift_timeout += val;
}

// These are sorted in order of power.
static monster_type _yred_servants[] =
{
    MONS_MUMMY, MONS_WIGHT, MONS_FLYING_SKULL, MONS_WRAITH,
    MONS_FREEZING_WRAITH, MONS_FLAMING_CORPSE, MONS_PHANTASMAL_WARRIOR,
    MONS_SKELETAL_WARRIOR, MONS_FLAYED_GHOST, MONS_DEATH_COB,
    MONS_GHOUL, MONS_BONE_DRAGON, MONS_PROFANE_SERVITOR,
};

#define MIN_YRED_SERVANT_THRESHOLD 3
#define MAX_YRED_SERVANT_THRESHOLD ARRAYSZ(_yred_servants)

int yred_random_servants(unsigned int threshold, bool force_hostile)
{
    if (threshold == 0)
        threshold = ARRAYSZ(_yred_servants);
    else
    {
        threshold = min(static_cast<unsigned int>(ARRAYSZ(_yred_servants)),
                        threshold);
    }

    const unsigned int servant = random2(threshold);
    if ((servant + 2) * 2 < threshold && !force_hostile)
        return -1;

    monster_type mon_type = _yred_servants[servant];
    int how_many = (mon_type == MONS_FLYING_SKULL) ? 2 + random2(4)
                                                   : 1;

    mgen_data mg(mon_type, !force_hostile ? BEH_FRIENDLY : BEH_HOSTILE,
                 !force_hostile ? &you : 0, 0, 0, you.pos(), MHITYOU, 0,
                 GOD_YREDELEMNUL);

    if (force_hostile)
        mg.non_actor_summoner = "the anger of Yredelemnul";

    int created = 0;
    if (force_hostile)
    {
        mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

        for (; how_many > 0; --how_many)
        {
            if (create_monster(mg))
                created++;
        }
    }
    else
    {
        for (; how_many > 0; --how_many)
            _delayed_monster(mg);
    }

    return created;
}

static const item_def* _find_missile_launcher(int skill)
{
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!you.inv[i].defined())
            continue;

        const item_def &item = you.inv[i];
        if (is_range_weapon(item)
            && range_skill(item) == skill_type(skill))
        {
            return &item;
        }
    }
    return NULL;
}

static bool _need_missile_gift(bool forced)
{
    const int best_missile_skill = best_skill(SK_SLINGS, SK_THROWING);
    const item_def *launcher = _find_missile_launcher(best_missile_skill);
    return ((forced || you.piety >= piety_breakpoint(2)
                       && random2(you.piety) > 70
                       && one_chance_in(8))
            && you.skills[ best_missile_skill ] >= 8
            && (launcher || best_missile_skill == SK_THROWING));
}

void get_pure_deck_weights(int weights[])
{
    weights[NEM_GIFT_ESCAPE]      = you.sacrifice_value[OBJ_ARMOUR] + 1;
    weights[NEM_GIFT_DESTRUCTION] = you.sacrifice_value[OBJ_WEAPONS]
                                    + you.sacrifice_value[OBJ_STAVES]
                                    + you.sacrifice_value[OBJ_RODS]
                                    + you.sacrifice_value[OBJ_MISSILES] + 1;
    weights[NEM_GIFT_DUNGEONS]    = you.sacrifice_value[OBJ_MISCELLANY]
                                    + you.sacrifice_value[OBJ_JEWELLERY]
                                    + you.sacrifice_value[OBJ_BOOKS];
    weights[NEM_GIFT_SUMMONING]   = you.sacrifice_value[OBJ_CORPSES] / 2;
    weights[NEM_GIFT_WONDERS]     = you.sacrifice_value[OBJ_POTIONS]
                                    + you.sacrifice_value[OBJ_SCROLLS]
                                    + you.sacrifice_value[OBJ_WANDS]
                                    + you.sacrifice_value[OBJ_FOOD];
}

static void _update_sacrifice_weights(int which)
{
    switch (which)
    {
    case NEM_GIFT_ESCAPE:
        you.sacrifice_value[OBJ_ARMOUR] /= 5;
        you.sacrifice_value[OBJ_ARMOUR] *= 4;
        break;
    case NEM_GIFT_DESTRUCTION:
        you.sacrifice_value[OBJ_WEAPONS]  /= 5;
        you.sacrifice_value[OBJ_STAVES]   /= 5;
        you.sacrifice_value[OBJ_RODS]     /= 5;
        you.sacrifice_value[OBJ_MISSILES] /= 5;
        you.sacrifice_value[OBJ_WEAPONS]  *= 4;
        you.sacrifice_value[OBJ_STAVES]   *= 4;
        you.sacrifice_value[OBJ_RODS]     *= 4;
        you.sacrifice_value[OBJ_MISSILES] *= 4;
        break;
    case NEM_GIFT_DUNGEONS:
        you.sacrifice_value[OBJ_MISCELLANY] /= 5;
        you.sacrifice_value[OBJ_JEWELLERY]  /= 5;
        you.sacrifice_value[OBJ_BOOKS]      /= 5;
        you.sacrifice_value[OBJ_MISCELLANY] *= 4;
        you.sacrifice_value[OBJ_JEWELLERY]  *= 4;
        you.sacrifice_value[OBJ_BOOKS]      *= 4;
    case NEM_GIFT_SUMMONING:
        you.sacrifice_value[OBJ_CORPSES] /= 5;
        you.sacrifice_value[OBJ_CORPSES] *= 4;
        break;
    case NEM_GIFT_WONDERS:
        you.sacrifice_value[OBJ_POTIONS] /= 5;
        you.sacrifice_value[OBJ_SCROLLS] /= 5;
        you.sacrifice_value[OBJ_WANDS]   /= 5;
        you.sacrifice_value[OBJ_FOOD]    /= 5;
        you.sacrifice_value[OBJ_POTIONS] *= 4;
        you.sacrifice_value[OBJ_SCROLLS] *= 4;
        you.sacrifice_value[OBJ_WANDS]   *= 4;
        you.sacrifice_value[OBJ_FOOD]    *= 4;
        break;
    }
}

#if defined(DEBUG_GIFTS) || defined(DEBUG_CARDS)
static void _show_pure_deck_chances()
{
    int weights[5];

    get_pure_deck_weights(weights);

    float total = 0;
    for (int i = 0; i < NUM_NEMELEX_GIFT_TYPES; ++i)
        total += (float) weights[i];

    mprf(MSGCH_DIAGNOSTICS, "Pure cards chances: "
         "escape %0.2f%%, destruction %0.2f%%, dungeons %0.2f%%,"
         "summoning %0.2f%%, wonders %0.2f%%",
         (float)weights[0] / total * 100.0,
         (float)weights[1] / total * 100.0,
         (float)weights[2] / total * 100.0,
         (float)weights[3] / total * 100.0,
         (float)weights[4] / total * 100.0);
}
#endif

static misc_item_type _gift_type_to_deck(int gift)
{
    switch (gift)
    {
    case NEM_GIFT_ESCAPE:      return MISC_DECK_OF_ESCAPE;
    case NEM_GIFT_DESTRUCTION: return MISC_DECK_OF_DESTRUCTION;
    case NEM_GIFT_DUNGEONS:    return MISC_DECK_OF_DUNGEONS;
    case NEM_GIFT_SUMMONING:   return MISC_DECK_OF_SUMMONING;
    case NEM_GIFT_WONDERS:     return MISC_DECK_OF_WONDERS;
    }
    die("invalid gift card type");
}

static bool _give_nemelex_gift(bool forced = false)
{
    // But only if you're not flying over deep water.
    if (!(feat_has_solid_floor(grd(you.pos()))
          || feat_is_watery(grd(you.pos())) && species_likes_water(you.species)))
    {
        return false;
    }

    // Nemelex will give at least one gift early.
    if (forced
        || !you.num_total_gifts[GOD_NEMELEX_XOBEH]
           && x_chance_in_y(you.piety + 1, piety_breakpoint(1))
        || one_chance_in(3) && x_chance_in_y(you.piety + 1, MAX_PIETY)
           && !you.attribute[ATTR_CARD_COUNTDOWN])
    {
        misc_item_type gift_type;

        // Make a pure deck.
        int weights[5];
        get_pure_deck_weights(weights);
        const int choice = choose_random_weighted(weights, weights+5);
        gift_type = _gift_type_to_deck(choice);
#if defined(DEBUG_GIFTS) || defined(DEBUG_CARDS)
        _show_pure_deck_chances();
#endif
        int thing_created = items(1, OBJ_MISCELLANY, gift_type,
                                   true, 1, MAKE_ITEM_RANDOM_RACE,
                                   0, 0, GOD_NEMELEX_XOBEH);

        move_item_to_grid(&thing_created, you.pos(), true);

        if (thing_created != NON_ITEM)
        {
            _update_sacrifice_weights(choice);

            // Piety|Common  | Rare  |Legendary
            // --------------------------------
            //     0:  95.00%,  5.00%,  0.00%
            //    20:  86.00%, 10.50%,  3.50%
            //    40:  77.00%, 16.00%,  7.00%
            //    60:  68.00%, 21.50%, 10.50%
            //    80:  59.00%, 27.00%, 14.00%
            //   100:  50.00%, 32.50%, 17.50%
            //   120:  41.00%, 38.00%, 21.00%
            //   140:  32.00%, 43.50%, 24.50%
            //   160:  23.00%, 49.00%, 28.00%
            //   180:  14.00%, 54.50%, 31.50%
            //   200:   5.00%, 60.00%, 35.00%
            int common_weight = 95 - (90 * you.piety / MAX_PIETY);
            int rare_weight   = 5  + (55 * you.piety / MAX_PIETY);
            int legend_weight = 0  + (35 * you.piety / MAX_PIETY);

            deck_rarity_type rarity = random_choose_weighted(
                common_weight, DECK_RARITY_COMMON,
                rare_weight,   DECK_RARITY_RARE,
                legend_weight, DECK_RARITY_LEGENDARY,
                0);

            item_def &deck(mitm[thing_created]);

            deck.special = rarity;
            deck.colour  = deck_rarity_to_colour(rarity);

            simple_god_message("은(는) 당신에게 선물을 하사했다!");
            more();
            canned_msg(MSG_SOMETHING_APPEARS);

            you.attribute[ATTR_CARD_COUNTDOWN] = 5;
            _inc_gift_timeout(5 + random2avg(9, 2));
            you.num_current_gifts[you.religion]++;
            you.num_total_gifts[you.religion]++;
            take_note(Note(NOTE_GOD_GIFT, you.religion));
        }
        return true;
    }

    return false;
}

void mons_make_god_gift(monster* mon, god_type god)
{
    const god_type acting_god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;

    if (god == GOD_NO_GOD && acting_god == GOD_NO_GOD)
        return;

    if (god == GOD_NO_GOD)
        god = acting_god;

    if (mon->flags & MF_GOD_GIFT)
    {
        dprf("Monster '%s' was already a gift of god '%s', now god '%s'.",
             mon->name(DESC_PLAIN, true).c_str(),
             god_name(mon->god).c_str(),
             god_name(god).c_str());
    }

    mon->god = god;
    mon->flags |= MF_GOD_GIFT;
}

bool mons_is_god_gift(const monster* mon, god_type god)
{
    return ((mon->flags & MF_GOD_GIFT) && mon->god == god);
}

bool is_yred_undead_slave(const monster* mon)
{
    return (mon->alive() && mon->holiness() == MH_UNDEAD
            && mon->attitude == ATT_FRIENDLY
            && mons_is_god_gift(mon, GOD_YREDELEMNUL));
}

bool is_orcish_follower(const monster* mon)
{
    return (mon->alive() && mons_genus(mon->type) == MONS_ORC
            && mon->attitude == ATT_FRIENDLY
            && mons_is_god_gift(mon, GOD_BEOGH));
}

bool is_fellow_slime(const monster* mon)
{
    return (mon->alive() && mons_is_slime(mon)
            && mon->attitude == ATT_STRICT_NEUTRAL
            && mons_is_god_gift(mon, GOD_JIYVA));
}

static bool _is_plant_follower(const monster* mon)
{
    return (mon->alive() && mons_is_plant(mon)
            && mon->attitude == ATT_FRIENDLY);
}

static bool _has_jelly()
{
    ASSERT(you_worship(GOD_JIYVA));

    for (monster_iterator mi; mi; ++mi)
        if (mons_is_god_gift(*mi, GOD_JIYVA))
            return true;
    return false;
}

bool is_follower(const monster* mon)
{
    if (you_worship(GOD_YREDELEMNUL))
        return is_yred_undead_slave(mon);
    else if (you_worship(GOD_BEOGH))
        return is_orcish_follower(mon);
    else if (you_worship(GOD_JIYVA))
        return is_fellow_slime(mon);
    else if (you_worship(GOD_FEDHAS))
        return _is_plant_follower(mon);
    else
        return (mon->alive() && mon->friendly());
}

static bool _blessing_wpn(monster* mon)
{
    // Pick a monster's weapon.
    const int weapon = mon->inv[MSLOT_WEAPON];
    const int alt_weapon = mon->inv[MSLOT_ALT_WEAPON];

    if (weapon == NON_ITEM && alt_weapon == NON_ITEM
        || mon->type == MONS_DANCING_WEAPON)
    {
        return false;
    }

    int slot;

    do
        slot = (coinflip()) ? weapon : alt_weapon;
    while (slot == NON_ITEM);

    item_def& wpn(mitm[slot]);

    // And enchant or uncurse it.
    int which = random2(2);
    if (!enchant_weapon(wpn, which, 1 - which, NULL))
        return false;

    item_set_appearance(wpn);
    return true;
}

static bool _blessing_AC(monster* mon)
{
    // Pick either a monster's armour or its shield.
    const int armour = mon->inv[MSLOT_ARMOUR];
    const int shield = mon->inv[MSLOT_SHIELD];

    if (armour == NON_ITEM && shield == NON_ITEM)
        return false;

    int slot;

    do
        slot = (coinflip()) ? armour : shield;
    while (slot == NON_ITEM);

    item_def& arm(mitm[slot]);

    int ac_change;

    // And enchant or uncurse it.
    if (!enchant_armour(ac_change, true, arm))
        return false;

    item_set_appearance(arm);
    return true;
}

static bool _blessing_balms(monster* mon)
{
    // Remove poisoning, sickness, confusion, and rotting, like a potion
    // of curing, but without the healing. Also, remove slowing and
    // fatigue.
    bool success = false;

    if (mon->del_ench(ENCH_POISON, true))
        success = true;

    if (mon->del_ench(ENCH_SICK, true))
        success = true;

    if (mon->del_ench(ENCH_CONFUSION, true))
        success = true;

    if (mon->del_ench(ENCH_ROT, true))
        success = true;

    if (mon->del_ench(ENCH_SLOW, true))
        success = true;

    if (mon->del_ench(ENCH_FATIGUE, true))
        success = true;

    return success;
}

static bool _blessing_healing(monster* mon)
{
    const int healing = mon->max_hit_points / 4 + 1;

    // Heal a monster.
    if (mon->heal(healing + random2(healing + 1)))
    {
        // A high-HP monster might get a unique name.
        if (x_chance_in_y(mon->max_hit_points + 1, 100))
            give_monster_proper_name(mon);
        return true;
    }

    return false;
}

static bool _increase_ench_duration(monster* mon,
                                    mon_enchant ench,
                                    const int increase)
{
    // Durations are saved as 16-bit signed ints, so clamp at the largest such.
    const int MARSHALL_MAX = (1 << 15) - 1;

    const int newdur = min(ench.duration + increase, MARSHALL_MAX);
    if (ench.duration >= newdur)
        return false;

    ench.duration = newdur;
    mon->update_ench(ench);

    return true;
}

static bool _tso_blessing_extend_stay(monster* mon)
{
    if (!mon->has_ench(ENCH_ABJ))
        return false;

    mon_enchant abj = mon->get_ench(ENCH_ABJ);

    // These numbers are tenths of a player turn. Holy monsters get a
    // much bigger boost than random beasties, which get at most double
    // their current summon duration.
    if (mon->is_holy())
        return _increase_ench_duration(mon, abj, 1100 + random2(1100));
    else
        return _increase_ench_duration(mon, abj, min(abj.duration,
                                                     500 + random2(500)));
}

static bool _tso_blessing_friendliness(monster* mon)
{
    if (!mon->has_ench(ENCH_CHARM))
        return false;

    // [ds] Just increase charm duration, no permanent friendliness.
    const int base_increase = 700;
    return _increase_ench_duration(mon, mon->get_ench(ENCH_CHARM),
                                   base_increase + random2(base_increase));
}

static void _beogh_reinf_callback(const mgen_data &mg, monster *&mon, int placed)
{
    ASSERT(mg.god == GOD_BEOGH);

    // Beogh tries a second time to place reinforcements.
    if (!mon)
        mon = create_monster(mg);

    if (!mon)
        return;

    mon->flags |= MF_ATT_CHANGE_ATTEMPT;

    bool high_level = (mon->type == MONS_ORC_PRIEST
                       || mon->type == MONS_ORC_WARRIOR
                       || mon->type == MONS_ORC_KNIGHT);

    // For high level orcs, there's a chance of being named.
    if (high_level && one_chance_in(5))
        give_monster_proper_name(mon);
}

// If you don't currently have any followers, send a small band to help
// you out.
static void _beogh_blessing_reinforcements()
{
    // Possible reinforcement.
    const monster_type followers[] = {
        MONS_ORC, MONS_ORC, MONS_ORC_WIZARD, MONS_ORC_PRIEST
    };

    const monster_type high_xl_followers[] = {
        MONS_ORC_PRIEST, MONS_ORC_WARRIOR, MONS_ORC_KNIGHT
    };

    // Send up to four followers.
    int how_many = random2(4) + 1;

    monster_type follower_type;
    for (int i = 0; i < how_many; ++i)
    {
        if (random2(you.experience_level) >= 9 && coinflip())
            follower_type = RANDOM_ELEMENT(high_xl_followers);
        else
            follower_type = RANDOM_ELEMENT(followers);

        _delayed_monster(
            mgen_data(follower_type, BEH_FRIENDLY, &you, 0, 0,
                      you.pos(), MHITYOU, 0, GOD_BEOGH),
            _beogh_reinf_callback);
    }
}

static bool _beogh_blessing_priesthood(monster* mon)
{
    monster_type priest_type = MONS_PROGRAM_BUG;

    // Possible promotions.
    if (mon->type == MONS_ORC)
        priest_type = MONS_ORC_PRIEST;

    if (priest_type != MONS_PROGRAM_BUG)
    {
        // Turn an ordinary monster into a priestly monster.
        mon->upgrade_type(priest_type, true, true);
        give_monster_proper_name(mon);

        return true;
    }

    return false;
}

// Bless the follower indicated in follower, if any.  If there isn't
// one, bless a random follower within sight of the player, if any, or,
// with decreasing chances, any follower on the level.
// Blessing can be enforced with a wizard mode command.
bool bless_follower(monster* follower,
                    god_type god,
                    bool (*suitable)(const monster* mon),
                    bool force)
{
    int chance = (force ? coinflip() : random2(20));
    string result;

    // If a follower was specified, and it's suitable, pick it.
    // Otherwise, pick a random follower.
    if (!follower || (!force && !suitable(follower)))
    {
        // Only Beogh blesses random followers.
        if (god != GOD_BEOGH)
            return false;

        if (chance > 2)
            return false;

        // Choose a random follower in LOS, preferably a named or
        // priestly one (10% chance).
        follower = choose_random_nearby_monster(0, suitable, true, true, true);

        if (!follower)
        {
            if (coinflip())
                return false;

            // Try again, without the LOS restriction (5% chance).
            follower = choose_random_nearby_monster(0, suitable, false, true,
                                                    true);

            if (!follower)
            {
                if (coinflip())
                    return false;

                // Try *again*, on the entire level (2.5% chance).
                follower = choose_random_monster_on_level(0, suitable, false,
                                                          false, true, true);

                if (!follower)
                {
                    // If no follower was found, attempt to send
                    // reinforcements.
                    _beogh_blessing_reinforcements();

                    // Possibly send more reinforcements.
                    if (coinflip())
                        _beogh_blessing_reinforcements();

                    _delayed_monster_done("Beogh blesses you with "
                                          "reinforcements.", "");

                    // Return true, even though the reinforcements might
                    // not be placed.
                    return true;
                }
            }
        }
    }
    ASSERT(follower);

    if (chance <= 1 && god == GOD_BEOGH) // 10% chance of priesthood
    {
        // Turn a monster into a priestly monster, if possible.
        if (_beogh_blessing_priesthood(follower))
        {
            result = "사제직";
            goto blessing_done;
        }
        else if (force)
            mpr("몬스터의 사제직을 촉진할 수 없다."); 
    }

    // Enchant a monster's weapon or armour/shield by one point, or at
    // least uncurse it, if possible (10% chance).
    // This will happen if the above blessing attempts are unsuccessful.
    if (chance <= 1)
    {
        if (coinflip())
        {
            if (_blessing_wpn(follower))
            {
                result = "추가 공격력";
                give_monster_proper_name(follower);
                goto blessing_done;
            }
            else if (force)
                mpr("몬스터의 무기는 강화할 수 없다.");
        }
        else
        {
            if (_blessing_AC(follower))
            {
                result = "추가 방어력";
                give_monster_proper_name(follower);
                goto blessing_done;
            }
            else if (force)
                mpr("몬스터의 방어구는 강화할 수 없다.");
        }
    }

    // These effects happen if no other blessing was chosen (90%),
    // or if the above attempts were all unsuccessful.
    switch (god)
    {
        case GOD_SHINING_ONE:
        {
            // Extend a monster's stay if it's abjurable, or extend charm
            // duration. If neither is possible, deliberately fall through.
            bool more_time = _tso_blessing_extend_stay(follower);
            bool friendliness = false;

            if (!more_time || coinflip())
                friendliness = _tso_blessing_friendliness(follower);

            result = "";

            if (friendliness)
            {
                result += "더 높은 충성심";
                if (more_time)
                    result += " 그리고 ";
            }

            if (more_time)
                result += "이 세계에 더 오래 머무는 것";

            if (more_time || friendliness)
                break;

            if (force)
                mpr("몬스터의 충성심 혹은 소환 시간을 연장하지 못했다.");
        }

        // Deliberate fallthrough for the healing effects.
        case GOD_BEOGH:
        {
            // Remove harmful ailments from a monster, or heal it, if
            // possible.
            if (coinflip())
            {
                if (_blessing_balms(follower))
                {
                    result = "성스러운 연고";
                    goto blessing_done;
                }
                else if (force)
                    mpr("연고를 적용하지 못했다.");
            }

            bool healing = _blessing_healing(follower);

            if (!healing || coinflip())
            {
                if (_blessing_healing(follower))
                    healing = true;
            }

            if (healing)
            {
                result += "치유";
                break;
            }
            else if (force)
                mpr("몬스터를 치료할 수 없다.");

            return false;
        }

        default:
            break;
    }

blessing_done:

    string whom = "";
    if (!follower)
        whom = "당신";
    else
    {
        if (you.can_see(follower))
            whom = follower->name(DESC_THE);
        else
            whom = "추종자";
    }

    simple_god_message(
        make_stringf("은(는) %s에게 %s의 축복을 내렸다.",
                     whom.c_str(), result.c_str()).c_str(),
        god);

#ifndef USE_TILE_LOCAL
    flash_monster_colour(follower, god_colour(god), 200);
#endif

    return true;
}

static void _delayed_gift_callback(const mgen_data &mg, monster *&mon,
                                   int placed)
{
    if (placed <= 0)
        return;

    // Make sure monsters are shown.
    viewwindow();
    more();
    _inc_gift_timeout(4 + random2avg(7, 2));
    you.num_current_gifts[you.religion]++;
    you.num_total_gifts[you.religion]++;
    take_note(Note(NOTE_GOD_GIFT, you.religion));
}

static bool _jiyva_mutate()
{
    simple_god_message("은(는) 당신의 몸에 변화를 일으켰다."); 

    const int rand = random2(100);

    if (rand < 10)
        return delete_mutation(RANDOM_SLIME_MUTATION, "Jiyva's grace", true, false, true);
    else if (rand < 30)
        return delete_mutation(RANDOM_NON_SLIME_MUTATION, "Jiyva's grace", true, false, true);
    else if (rand < 55)
        return mutate(RANDOM_MUTATION, "Jiyva's grace", true, false, true);
    else if (rand < 75)
        return mutate(RANDOM_SLIME_MUTATION, "Jiyva's grace", true, false, true);
    else
        return mutate(RANDOM_GOOD_MUTATION, "Jiyva's grace", true, false, true);
}

bool vehumet_is_offering(spell_type spell)
{
    return (find(you.vehumet_gifts.begin(), you.vehumet_gifts.end(), spell)
            != you.vehumet_gifts.end());
}

void vehumet_accept_gift(spell_type spell)
{
    set<spell_type>::iterator it =
        find(you.vehumet_gifts.begin(), you.vehumet_gifts.end(), spell);
    if (it != you.vehumet_gifts.end())
    {
        you.vehumet_gifts.erase(it);
        you.seen_spell.set(spell);
        you.duration[DUR_VEHUMET_GIFT] = 0;
    }
}

static void _add_to_old_gifts(spell_type spell)
{
    you.old_vehumet_gifts.insert(spell);
}

static bool _is_old_gift(spell_type spell)
{
    return (find(you.old_vehumet_gifts.begin(),
                 you.old_vehumet_gifts.end(), spell)
            != you.old_vehumet_gifts.end());
}

static set<spell_type> _vehumet_eligible_gift_spells(set<spell_type> excluded_spells)
{
    set<spell_type> eligible_spells;

    const int gifts = you.num_total_gifts[you.religion];
    if (gifts >= 13)
        return eligible_spells;

    int min_lev[13] = {1,1,2,3,3,4,4,5,5,6,6,6,8};
    int max_lev[13] = {1,2,3,4,5,7,7,7,7,7,7,7,9};
    int min_level = min_lev[gifts];
    int max_level = max_lev[gifts];

    if (min_level > you.experience_level)
        return eligible_spells;

    set<spell_type> backup_spells;
    for (int i = 0; i < NUM_SPELLS; ++i)
    {
        spell_type spell = static_cast<spell_type>(i);
        if (!is_valid_spell(spell))
            continue;

        if (find(excluded_spells.begin(), excluded_spells.end(), spell)
            != excluded_spells.end())
        {
            continue;
        }

        if (vehumet_supports_spell(spell)
            && !you.has_spell(spell)
            && is_player_spell(spell)
            && spell_difficulty(spell) <= max_level
            && spell_difficulty(spell) >= min_level)
        {
            if (!you.seen_spell[spell] && !_is_old_gift(spell))
                eligible_spells.insert(spell);
            else
                backup_spells.insert(spell);
        }
    }
    // Don't get stuck just because all spells have been seen/offered.
    if (eligible_spells.empty())
    {
        if (backup_spells.empty())
        {
            // This is quite improbable to happen, but in this case just
            // skip the gift and increment the gift counter.
            if (gifts <= 12)
            {
                you.num_current_gifts[you.religion]++;
                you.num_total_gifts[you.religion]++;
            }
        }
        return backup_spells;
    }
    return eligible_spells;
}

static int _vehumet_weighting(spell_type spell)
{
    int bias = 100 + elemental_preference(spell, 10);
    bias = min(max(bias, 10), 190);
    return bias;
}

static spell_type _vehumet_find_spell_gift(set<spell_type> excluded_spells)
{
    set<spell_type> eligible_spells = _vehumet_eligible_gift_spells(excluded_spells);
    spell_type spell = SPELL_NO_SPELL;
    int total_weight = 0;
    int this_weight = 0;
    for (set<spell_type>::iterator it = eligible_spells.begin();
         it != eligible_spells.end(); ++it)
    {
        this_weight = _vehumet_weighting(*it);
        total_weight += this_weight;
        if (x_chance_in_y(this_weight, total_weight))
            spell = *it;
    }
    return spell;
}

static set<spell_type> _vehumet_get_spell_gifts()
{
    set<spell_type> offers;
    unsigned int num_offers = you.num_total_gifts[you.religion] == 12 ? 3 : 1;
    while (offers.size() < num_offers)
    {
        spell_type offer = _vehumet_find_spell_gift(offers);
        if (offer == SPELL_NO_SPELL)
            break;
        offers.insert(offer);
    }
    return offers;
}

///////////////////////////////
bool do_god_gift(bool forced)
{
    ASSERT(!you_worship(GOD_NO_GOD));

    god_acting gdact;

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_GIFTS)
    int old_num_current_gifts = you.num_current_gifts[you.religion];
    int old_num_total_gifts = you.num_total_gifts[you.religion];
#endif

    bool success = false;

    // Consider a gift if we don't have a timeout and weren't already
    // praying when we prayed.
    if (forced || !player_under_penance() && !you.gift_timeout)
    {
        // Remember to check for water/lava.
        switch (you.religion)
        {
        default:
            break;

        case GOD_NEMELEX_XOBEH:
            success = _give_nemelex_gift(forced);
            break;

        case GOD_OKAWARU:
        case GOD_TROG:
        {
            // Break early if giving a gift now means it would be lost.
            if (!(feat_has_solid_floor(grd(you.pos()))
                || feat_is_watery(grd(you.pos())) && species_likes_water(you.species)))
            {
                break;
            }

            // Should gift catnip instead.
            if (you.species == SP_FELID)
                break;

            const bool need_missiles = _need_missile_gift(forced);
            object_class_type gift_type;

            if (forced && (!need_missiles || one_chance_in(4))
                || (!forced && you.piety >= piety_breakpoint(4)
                    && random2(you.piety) > 120
                    && one_chance_in(4)))
            {
                if (you_worship(GOD_TROG)
                    || (you_worship(GOD_OKAWARU) && coinflip()))
                {
                    gift_type = OBJ_WEAPONS;
                }
                else
                    gift_type = OBJ_ARMOUR;
            }
            else if (need_missiles)
                gift_type = OBJ_MISSILES;
            else
                break;

            success = acquirement(gift_type, you.religion);
            if (success)
            {
                simple_god_message("은(는) 당신에게 선물을 하사했다!");
                more();

                if (gift_type == OBJ_MISSILES)
                    _inc_gift_timeout(4 + roll_dice(2, 4));
                else
                {
                    // Okawaru charges extra for armour acquirements.
                    if (you_worship(GOD_OKAWARU) && gift_type == OBJ_ARMOUR)
                        _inc_gift_timeout(30 + random2avg(15, 2));

                    _inc_gift_timeout(30 + random2avg(19, 2));
                }
                you.num_current_gifts[you.religion]++;
                you.num_total_gifts[you.religion]++;
                take_note(Note(NOTE_GOD_GIFT, you.religion));
            }
            break;
        }

        case GOD_YREDELEMNUL:
            if (forced
                || (random2(you.piety) >= piety_breakpoint(2)
                    && one_chance_in(4)))
            {
                unsigned int threshold = MIN_YRED_SERVANT_THRESHOLD
                                         + you.num_current_gifts[you.religion] / 2;
                threshold = max(threshold,
                    static_cast<unsigned int>(MIN_YRED_SERVANT_THRESHOLD));
                threshold = min(threshold,
                    static_cast<unsigned int>(MAX_YRED_SERVANT_THRESHOLD));

                if (yred_random_servants(threshold) != -1)
                {
                    _delayed_monster_done("은(는) 당신에게 언데드 수하를 내려주었다!", 
                                          "", _delayed_gift_callback);
                    success = true;
                }
            }
            break;

        case GOD_JIYVA:
            if (forced || you.piety >= piety_breakpoint(2)
                          && random2(you.piety) > 50
                          && one_chance_in(4) && !you.gift_timeout
                          && you.can_safely_mutate())
            {
                if (_jiyva_mutate())
                {
                    _inc_gift_timeout(15 + roll_dice(2, 4));
                    you.num_current_gifts[you.religion]++;
                    you.num_total_gifts[you.religion]++;
                }
                else
                    mpr("아무것도 변화하지 않은 느낌이다.");
            }
            break;

        case GOD_KIKUBAAQUDGHA:
        case GOD_SIF_MUNA:
            int gift;
            gift = NUM_BOOKS;
            // Break early if giving a gift now means it would be lost.
            if (!feat_has_solid_floor(grd(you.pos())))
                break;

            // Kikubaaqudgha gives the lesser Necromancy books in a quick
            // succession.
            if (you_worship(GOD_KIKUBAAQUDGHA))
            {
                if (you.piety >= piety_breakpoint(0)
                    && you.num_total_gifts[you.religion] == 0)
                {
                    gift = BOOK_NECROMANCY;
                }
                else if (you.piety >= piety_breakpoint(2)
                         && you.num_total_gifts[you.religion] == 1)
                {
                    gift = BOOK_DEATH;
                }
            }
            else if (forced || you.piety >= piety_breakpoint(5)
                               && random2(you.piety) > 100)
            {
                if (you_worship(GOD_SIF_MUNA))
                    gift = OBJ_RANDOM;
            }

            if (gift != NUM_BOOKS)
            {
                if (gift == OBJ_RANDOM)
                {
                    // Sif Muna special: Keep quiet if acquirement fails
                    // because the player already has seen all spells.
                    success = acquirement(OBJ_BOOKS, you.religion, true);
                }
                else
                {
                    int thing_created = items(1, OBJ_BOOKS, gift, true, 1,
                                              MAKE_ITEM_RANDOM_RACE,
                                              0, 0, you.religion);
                    // Replace a Kiku gift by a custom-random book.
                    if (you_worship(GOD_KIKUBAAQUDGHA))
                    {
                        make_book_Kiku_gift(mitm[thing_created],
                                            gift == BOOK_NECROMANCY);
                    }
                    if (thing_created == NON_ITEM)
                        return false;

                    // Mark the book type as known to avoid duplicate
                    // gifts if players don't read their gifts for some
                    // reason.
                    mark_had_book(gift);

                    move_item_to_grid(&thing_created, you.pos(), true);

                    if (thing_created != NON_ITEM)
                        success = true;
                }

                if (success)
                {
                    simple_god_message("은(는) 당신에게 선물을 하사했다!");
                    more();

                    you.num_current_gifts[you.religion]++;
                    you.num_total_gifts[you.religion]++;
                    // Timeouts are meaningless for Kiku.
                    if (!you_worship(GOD_KIKUBAAQUDGHA))
                        _inc_gift_timeout(40 + random2avg(19, 2));
                    take_note(Note(NOTE_GOD_GIFT, you.religion));
                }
            }                   // End of giving books.
            break;              // End of book gods.

        case GOD_VEHUMET:
            const int gifts = you.num_total_gifts[you.religion];
            if (forced || !you.duration[DUR_VEHUMET_GIFT]
                          && (you.piety >= piety_breakpoint(0) && gifts == 0
                              || you.piety >= piety_breakpoint(0) + random2(6) + 18 * gifts && gifts <= 5
                              || you.piety >= piety_breakpoint(4) && gifts <= 11 && one_chance_in(20)
                              || you.piety >= piety_breakpoint(5) && gifts <= 12 && one_chance_in(20)))
            {
                set<spell_type> offers = _vehumet_get_spell_gifts();
                if (!offers.empty())
                {
                    you.vehumet_gifts = offers;
                    string prompt = "은(는) 당신에게 '";
                    for (set<spell_type>::iterator it = offers.begin();
                         it != offers.end(); ++it)
                    {
                        if (it != offers.begin())
                        {
                            if (offers.size() > 2)
                                prompt += ", ";
                            prompt += " ";
                            set<spell_type>::iterator next = it;
                            next++;
                            if (next == offers.end())
                                prompt += ", ";
                        }
                        prompt += _(spell_title(*it));
                        _add_to_old_gifts(*it);
                        take_note(Note(NOTE_OFFERED_SPELL, *it));
                    }
                    prompt += "' 주문에 대한 지식을 하사했다.";

                    you.duration[DUR_VEHUMET_GIFT] = (100 + random2avg(100, 2)) * BASELINE_DELAY;
                    if (gifts >= 5)
                        _inc_gift_timeout(30 + random2avg(30, 2));
                    you.num_current_gifts[you.religion]++;
                    you.num_total_gifts[you.religion]++;

                    simple_god_message(prompt.c_str());
                    more();

                    success = true;
                }
                else
                    success = false;
            }
            break;
        }                       // switch (you.religion)
    }                           // End of gift giving.

    if (success)
        you.running.stop();

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_GIFTS)
    if (old_num_current_gifts < you.num_current_gifts[you.religion])
    {
        mprf(MSGCH_DIAGNOSTICS, "Current number of gifts from this god: %d",
             you.num_current_gifts[you.religion]);
    }
    if (old_num_total_gifts < you.num_total_gifts[you.religion])
    {
        mprf(MSGCH_DIAGNOSTICS, "Total number of gifts from this god: %d",
             you.num_total_gifts[you.religion]);
    }
#endif
    return success;
}

string god_name(god_type which_god, bool long_name)
{
    if (which_god == GOD_JIYVA)
        return (god_name_jiyva(long_name) +
                (long_name? " the Shapeless" : ""));

    if (long_name)
    {
        const string shortname = god_name(which_god, false);
        const string longname = getMiscString(shortname + " lastname");
        return (longname.empty()? shortname : longname);
    }

    switch (which_god)
    {
    case GOD_NO_GOD:        return "No God";
    case GOD_RANDOM:        return "random";
    case GOD_NAMELESS:      return "nameless";
    case GOD_VIABLE:        return "viable";
    case GOD_ZIN:           return "Zin";
    case GOD_SHINING_ONE:   return "the Shining One";
    case GOD_KIKUBAAQUDGHA: return "Kikubaaqudgha";
    case GOD_YREDELEMNUL:   return "Yredelemnul";
    case GOD_VEHUMET:       return "Vehumet";
    case GOD_OKAWARU:       return "Okawaru";
    case GOD_MAKHLEB:       return "Makhleb";
    case GOD_SIF_MUNA:      return "Sif Muna";
    case GOD_TROG:          return "Trog";
    case GOD_NEMELEX_XOBEH: return "Nemelex Xobeh";
    case GOD_ELYVILON:      return "Elyvilon";
    case GOD_LUGONU:        return "Lugonu";
    case GOD_BEOGH:         return "Beogh";
    case GOD_JIYVA:
    {
        return (long_name ? god_name_jiyva(true) + " the Shapeless"
                          : god_name_jiyva(false));
    }
    case GOD_FEDHAS:        return "Fedhas";
    case GOD_CHEIBRIADOS:   return "Cheibriados";
    case GOD_XOM:           return "Xom";
    case GOD_ASHENZARI:     return "Ashenzari";
    case NUM_GODS:          return "Buggy";
    }
    return "";
}

string god_name_jiyva(bool second_name)
{
    string name = "Jiyva";
    if (second_name)
        name += " " + you.jiyva_second_name;

    return name;
}

god_type str_to_god(const string &_name, bool exact)
{
    string target(_name);
    trim_string(target);
    lowercase(target);

    if (target.empty())
        return GOD_NO_GOD;

    int      num_partials = 0;
    god_type partial      = GOD_NO_GOD;
    for (int i = 0; i < NUM_GODS; ++i)
    {
        god_type    god  = static_cast<god_type>(i);
        string name = lowercase_string(god_name(god, false));

        if (name == target)
            return god;

        if (!exact && name.find(target) != string::npos)
        {
            // Return nothing for ambiguous partial names.
            num_partials++;
            if (num_partials > 1)
                return GOD_NO_GOD;
            partial = god;
        }
    }

    if (!exact && num_partials == 1)
        return partial;

    return GOD_NO_GOD;
}

void god_speaks(god_type god, const char *mesg)
{
    ASSERT(!crawl_state.game_is_arena());

    int orig_mon = mgrd(you.pos());

    monster fake_mon;
    fake_mon.type       = MONS_PROGRAM_BUG;
    fake_mon.hit_points = 1;
    fake_mon.god        = god;
    fake_mon.set_position(you.pos());
    fake_mon.foe        = MHITYOU;
    fake_mon.mname      = "FAKE GOD MONSTER";

    mpr(do_mon_str_replacements(mesg, &fake_mon).c_str(), MSGCH_GOD, god);

    fake_mon.reset();
    mgrd(you.pos()) = orig_mon;
}

void religion_turn_start()
{
    if (you.turn_is_over)
        religion_turn_end();

    crawl_state.clear_god_acting();
}

void religion_turn_end()
{
    ASSERT(you.turn_is_over);
    _place_delayed_monsters();
}

static void _replace(string& s, const string &find, const string &repl)
{
    string::size_type start = 0;
    string::size_type found;

    while ((found = s.find(find, start)) != string::npos)
    {
        s.replace(found, find.length(), repl);
        start = found + repl.length();
    }
}

static void _erase_between(string& s, const string &left, const string &right)
{
    string::size_type left_pos;
    string::size_type right_pos;

    while ((left_pos = s.find(left)) != string::npos
           && (right_pos = s.find(right, left_pos + left.size())) != string::npos)
    {
        s.erase(s.begin() + left_pos, s.begin() + right_pos + right.size());
    }
}

string adjust_abil_message(const char *pmsg, bool allow_upgrades)
{
    if (brdepth[BRANCH_ABYSS] == -1 && strstr(pmsg, "Abyss"))
        return "";

    string pm = pmsg;

    // Message portions in [] sections are ability upgrades.
    if (allow_upgrades)
    {
        _replace(pm, "[", "");
        _replace(pm, "]", "");
    }
    else
        _erase_between(pm, "[", "]");

    int pos;

    if ((pos = pm.find("{yred_dead}")) != -1)
    {
        if (yred_can_animate_dead())
            pm.replace(pos, 11, "죽음의 군단들");
        else
            pm.replace(pos, 11, "시체들");
    }

    return pm;
}

static bool _abil_chg_message(const char *pmsg, const char *youcanmsg,
                              int breakpoint)
{
    if (!*pmsg)
        return false;

    // Set piety to the passed-in piety breakpoint value when getting
    // the ability message.  If we have an ability upgrade, which will
    // change description based on current piety, and current piety has
    // gone up more than one breakpoint, this will ensure that all
    // ability upgrade descriptions display in the proper sequence.
    int old_piety = you.piety;
    you.piety = piety_breakpoint(breakpoint);

    string pm = adjust_abil_message(pmsg);
    if (pm.empty())
        return false;

    you.piety = old_piety;

    if (isupper(pmsg[0]))
        god_speaks(you.religion, pm.c_str());
    else
    {
        god_speaks(you.religion,
                   make_stringf(youcanmsg, pm.c_str()).c_str());
    }

    return true;
}

void dock_piety(int piety_loss, int penance)
{
    static int last_piety_lecture   = -1;
    static int last_penance_lecture = -1;

    if (piety_loss <= 0 && penance <= 0)
        return;

    piety_loss = piety_scale(piety_loss);
    penance    = piety_scale(penance);

    if (piety_loss)
    {
        if (last_piety_lecture != you.num_turns)
        {
            // output guilt message:
            mprf("당신은 %s죄의식을 느꼈다.",
                 (piety_loss == 1) ? "약간 " :
                 (piety_loss <  5) ? "" :
                 (piety_loss < 10) ? "강한 "
                                   : "극심한 ");
        }

        last_piety_lecture = you.num_turns;
        lose_piety(piety_loss);
    }

    if (you.piety < 1)
        excommunication();
    else if (penance)       // only if still in religion
    {
        if (last_penance_lecture != you.num_turns) // (130203) godspeak.txt의 신들의 대사(성격)와 같아지도록 이 부분은 따로 수정한 부분.
        {										   // 영문판은 어투가 따로 없지만 한글판의 경우는 존재하니 이 부분이 어색하더라고요.
            god_speaks(you.religion,
					   (you.religion == GOD_TROG || you.religion == GOD_BEOGH || you.religion == GOD_SHINING_ONE) ? "\"네 놈은 지금 한 짓거리의 대가를 똑똑히 치를 것이다!\"" : (you.religion == GOD_ZIN || you.religion == GOD_SIF_MUNA) ? "\"자네는 자네의 업보의 대가를 치르게 될 것이네!\"" : (you.religion == GOD_ELYVILON ? "\"넌 이제 업보의 대가를 치르게 될 것이다!\"" : "\"너는 네 업보의 대가를 치르게 될 것이다!\""));
        }
        last_penance_lecture = you.num_turns;
        _inc_penance(penance);
    }
}

// Scales a piety number, applying boosters (amulet of faith).
int piety_scale(int piety)
{
    if (piety < 0)
        return -piety_scale(-piety);

    if (you.faith())
        return (piety + div_rand_round(piety, 3));

    return piety;
}

static void _gain_piety_point();
void gain_piety(int original_gain, int denominator, bool force, bool should_scale_piety)
{
    if (original_gain <= 0)
        return;

    // Xom uses piety differently...
    if (you_worship(GOD_NO_GOD) || you_worship(GOD_XOM))
        return;

    int pgn = should_scale_piety? piety_scale(original_gain) : original_gain;

    if (crawl_state.game_is_sprint() && should_scale_piety)
        pgn = sprint_modify_piety(pgn);

    pgn = div_rand_round(pgn, denominator);
    while (pgn-- > 0)
        _gain_piety_point();
    if (you.piety > you.piety_max[you.religion])
    {
        if (you.piety >= piety_breakpoint(5)
            && you.piety_max[you.religion] < piety_breakpoint(5))
        {
            mark_milestone("god.maxpiety", // "became the Champion of "
                           _(god_name(you.religion).c_str()) + std::string("의 대변자가 됨.")); //+ god_name(you.religion) + ".");
        }
        you.piety_max[you.religion] = you.piety;
    }
}

static void _gain_piety_point()
{
    // check to see if we owe anything first
    if (player_under_penance(you.religion))
    {
        dec_penance(1);
        return;
    }
    else if (you.gift_timeout > 0)
    {
        you.gift_timeout--;

        // Slow down piety gain to account for the fact that gifts
        // no longer have a piety cost for getting them.
        // Jiyva is an exception because there's usually a time-out and
        // the gifts aren't that precious.
        if (!one_chance_in(4) && !you_worship(GOD_JIYVA))
        {
#ifdef DEBUG_PIETY
            mprf(MSGCH_DIAGNOSTICS, "Piety slowdown due to gift timeout.");
#endif
            return;
        }
    }

    // slow down gain at upper levels of piety
    if (!you_worship(GOD_SIF_MUNA))
    {
        if (you.piety >= MAX_PIETY
            || you.piety >= piety_breakpoint(5) && one_chance_in(3)
            || you.piety >= piety_breakpoint(3) && one_chance_in(3))
        {
            do_god_gift();
            return;
        }
    }
    else
    {
        // Sif Muna has a gentler taper off because training becomes
        // naturally slower as the player gains in spell skills.
        if (you.piety >= MAX_PIETY
            || you.piety >= piety_breakpoint(5) && one_chance_in(5))
        {
            do_god_gift();
            return;
        }
    }

    int old_piety = you.piety;
    // Apply hysteresis.
    // piety_hysteresis is the amount of _loss_ stored up, so this
    // may look backwards.
    if (you.piety_hysteresis)
        you.piety_hysteresis--;
    else if (you.piety < MAX_PIETY)
        you.piety++;

    for (int i = 0; i < MAX_GOD_ABILITIES; ++i)
    {
        if (you.piety >= piety_breakpoint(i)
            && old_piety < piety_breakpoint(i))
        {
            take_note(Note(NOTE_GOD_POWER, you.religion, i));

            // In case the best skill is Invocations, redraw the god
            // title.
            you.redraw_title = true;

            gain_god_ability(i);

            if (_abil_chg_message(god_gain_power_messages[you.religion][i],
                                  "당신은 이제 %s", i))
            {
#ifdef USE_TILE_LOCAL
                tiles.layout_statcol();
                redraw_screen();
#endif
                learned_something_new(HINT_NEW_ABILITY_GOD);
            }

            if (you_worship(GOD_SHINING_ONE) && i == 0)
                mpr("주변으로 성스러운 후광이 빛나기 시작했다!");

            if (you_worship(GOD_ASHENZARI))
            {
                if (i == 2)
                {
                    autotoggle_autopickup(false);
                    // Inconsistent with donning amulets, but matches the
                    // message better and is not abusable.
                    you.duration[DUR_CONF] = 0;
                }

                auto_id_inventory();
            }

            // When you gain a piety level, you get another chance to
            // make hostile holy beings good neutral.
            if (is_good_god(you.religion))
                add_daction(DACT_HOLY_NEW_ATTEMPT);
        }
    }

    if (you_worship(GOD_BEOGH))
    {
        // Every piety level change also affects AC from orcish gear.
        you.redraw_armour_class = true;
        // The player's symbol depends on Beogh piety.
        update_player_symbol();
    }

    if (you_worship(GOD_CHEIBRIADOS)
        && chei_stat_boost(old_piety) < chei_stat_boost())
    {
        simple_god_message("은(는) 당신의 움직임이 느려짐에 따라, 당신의 능력치를 증가시켜주었다.");
        notify_stat_change("Cheibriados piety gain");
    }

    if (you_worship(GOD_SHINING_ONE))
    {
        // Piety change affects halo radius.
        invalidate_agrid(true);
    }

    if (you.piety >= piety_breakpoint(5) && old_piety < piety_breakpoint(5))
    {
        // In case the best skill is Invocations, redraw the god title.
        you.redraw_title = true;

        if (!you.one_time_ability_used[you.religion])
        {
            switch (you.religion)
            {
                case GOD_ZIN:
                    simple_god_message("은(는) 단 한번, 당신의 모든 변이를 치료해줄 것이다.");
                    break;
                case GOD_SHINING_ONE:
                    if (you.species == SP_FELID)
                        break;
                    simple_god_message("은(는) 단 한번, 제단에 있는 당신의 무기에 축복을 부여해 줄 것이다.");
                    break;
                case GOD_KIKUBAAQUDGHA:
                    simple_god_message("은(는) 단 한번, 제단에 있는 당신의 강령술을 강화시켜줄 것이다.");
                    break;
                case GOD_LUGONU:
                    if (you.species == SP_FELID)
                        break;
                    simple_god_message("은(는) 단 한번, 제단에 있는 당신의 무기를 왜곡시켜줄 것이다.");
                    break;
                case GOD_JIYVA:
                    simple_god_message("은, 슬라임 구덩이의 가장 깊은 곳에 있는 보물창고의 봉인을 풀어줄 것이다.");
                    dlua.callfn("dgn_set_persistent_var", "sb", "fix_slime_vaults", true);
                    // If we're on Slime:6, pretend we just entered the level
                    // in order to bring down the vault walls.
                    if (level_id::current() == level_id(BRANCH_SLIME_PITS, 6))
                        dungeon_events.fire_event(DET_ENTERED_LEVEL);

                    you.one_time_ability_used.set(you.religion);
                    break;
                default:
                    break;
            }
        }

        // When you gain piety of more than 160, you get another chance
        // to make hostile holy beings good neutral.
        if (is_good_god(you.religion))
            add_daction(DACT_HOLY_NEW_ATTEMPT);
    }

    do_god_gift();
}

void lose_piety(int pgn)
{
    if (pgn <= 0)
        return;

    const int old_piety = you.piety;

    // Apply hysteresis.
    const int old_hysteresis = you.piety_hysteresis;
    you.piety_hysteresis = min<int>(PIETY_HYSTERESIS_LIMIT,
                                    you.piety_hysteresis + pgn);
    const int pgn_borrowed = (you.piety_hysteresis - old_hysteresis);
    pgn -= pgn_borrowed;
#ifdef DEBUG_PIETY
    mprf(MSGCH_DIAGNOSTICS,
         "Piety decreasing by %d (and %d added to hysteresis)",
         pgn, pgn_borrowed);
#endif

    if (you.piety - pgn < 0)
        you.piety = 0;
    else
        you.piety -= pgn;

    // Don't bother printing out these messages if you're under
    // penance, you wouldn't notice since all these abilities
    // are withheld.
    if (!player_under_penance() && you.piety != old_piety)
    {
        if (you.piety < piety_breakpoint(5)
            && old_piety >= piety_breakpoint(5)
            && !you.one_time_ability_used[you.religion])
        {
            // In case the best skill is Invocations, redraw the god
            // title.
            you.redraw_title = true;

            if (you_worship(GOD_ZIN))
            {
                simple_god_message(
                    "은(는) 이제 변이를 치유해줄 준비가 되어 있지 않다.");
            }
            else if (you_worship(GOD_SHINING_ONE) && you.species != SP_FELID)
            {
                simple_god_message(
                    "은(는) 이제 무기에 축복을 부여할 준비가 되어 있지 않다.");
            }
            else if (you_worship(GOD_KIKUBAAQUDGHA))
            {
                simple_god_message(
                    "은(는) 이제 당신의 강령술을 강화시킬 준비가 되어 있지 않다.");
            }
            else if (you_worship(GOD_LUGONU) && you.species != SP_FELID)
            {
                simple_god_message(
                    "은(는) 이제 무기에 왜곡을 부여할 준비가 되어 있지 않다.");
            }
        }

        for (int i = 0; i < MAX_GOD_ABILITIES; ++i)
        {
            if (you.piety < piety_breakpoint(i)
                && old_piety >= piety_breakpoint(i))
            {
                // In case the best skill is Invocations, redraw the god
                // title.
                you.redraw_title = true;

                lose_god_ability(i);
                _abil_chg_message(god_lose_power_messages[you.religion][i],
                                  "당신은 이제 더이상 %s", i);

                if (_need_water_walking() && !beogh_water_walk())
                    fall_into_a_pool(you.pos(), true, grd(you.pos()));
            }
        }

#ifdef USE_TILE_LOCAL
        if (you.redraw_title)
        {
            tiles.layout_statcol();
            redraw_screen();
        }
#endif
    }

    if (you.piety > 0 && you.piety <= 5)
        learned_something_new(HINT_GOD_DISPLEASED);

    if (you_worship(GOD_BEOGH))
    {
        // Every piety level change also affects AC from orcish gear.
        you.redraw_armour_class = true;
    }

    if (you_worship(GOD_CHEIBRIADOS)
        && chei_stat_boost(old_piety) > chei_stat_boost())
    {
        simple_god_message("은(는) 당신의 움직임이 빨라진 만큼, 당신의 능력치 보정을 줄였다.");
        notify_stat_change("Cheibriados piety loss");
    }

    if (you_worship(GOD_SHINING_ONE))
    {
        // Piety change affects halo radius.
        invalidate_agrid(true);
    }
}

// Fedhas worshipers are on the hook for most plants and fungi
//
// If fedhas worshipers kill a protected monster they lose piety,
// if they attack a friendly one they get penance,
// if a friendly one dies they lose piety.
static bool _fedhas_protects_species(monster_type mc)
{
    return (mons_class_is_plant(mc)
            && mons_class_holiness(mc) == MH_PLANT
            && mc != MONS_GIANT_SPORE
            && mc != MONS_SNAPLASHER_VINE
            && mc != MONS_SNAPLASHER_VINE_SEGMENT);
}

bool fedhas_protects(const monster* target)
{
    return (target && _fedhas_protects_species(mons_base_type(target)));
}

// Fedhas neutralises most plants and fungi
bool fedhas_neutralises(const monster* target)
{
    return (target && mons_is_plant(target)
            && target->holiness() == MH_PLANT
            && target->type != MONS_SNAPLASHER_VINE
            && target->type != MONS_SNAPLASHER_VINE_SEGMENT);
}

static string _god_hates_your_god_reaction(god_type god, god_type your_god)
{
    if (god_hates_your_god(god, your_god))
    {
        // Non-good gods always hate your current god.
        if (!is_good_god(god))
            return "한"; // 바로 아래 50여줄 아래에서 사용하는 부분 

        // Zin hates chaotic gods.
        if (god == GOD_ZIN && is_chaotic_god(your_god))
            return "하고, 혼돈 성향의 신으로 개종한"; 

        if (is_evil_god(your_god))
            return "하고, 사악한 신으로 개종한"; 
    }

    return "";
}

void excommunication(god_type new_god)
{
    const god_type old_god = you.religion;
    ASSERT(old_god != new_god);
    ASSERT(old_god != GOD_NO_GOD);

    const bool was_haloed = you.haloed();
    const int  old_piety  = you.piety;

    god_acting gdact(old_god, true);

    take_note(Note(NOTE_LOSE_GOD, old_god));

    vector<ability_type> abilities = get_god_abilities(true);
    for (unsigned int i = 0; i < abilities.size(); ++i)
    {
        you.stop_train.insert(abil_skill(abilities[i]));
        if (abilities[i] == ABIL_TSO_DIVINE_SHIELD)
            you.stop_train.insert(SK_SHIELDS);
    }

    you.duration[DUR_PIETY_POOL] = 0; // your loss
    you.duration[DUR_RECITE] = 0;
    you.piety = 0;
    you.piety_hysteresis = 0;
    if (old_god == GOD_ASHENZARI)
        ash_init_bondage(&you);

    you.num_current_gifts[old_god] = 0;

    you.religion = GOD_NO_GOD;

    you.redraw_title = true;

    // Renouncing may have changed the conducts on our wielded or
    // quivered weapons, so refresh the display.
    you.wield_change = true;
    you.redraw_quiver = true;

    mpr("당신은 지금까지의 신앙을 잃었다!");
    more();

    if (old_god == GOD_BEOGH)
    {
        // The player's symbol depends on Beogh worship.
        update_player_symbol();
    }

    mark_milestone("god.renounce", _(god_name(old_god).c_str()) + std::string("을(를) 저버림.")); // mark_milestone("god.renounce", "abandoned " + god_name(old_god) + ".");
#ifdef DGL_WHEREIS
    whereis_record();
#endif

    if (god_hates_your_god(old_god, new_god))
    {
        simple_god_message(
            make_stringf("은(는) 당신이 신앙을 파기%s 것을 불쾌해했다!!",
                         _god_hates_your_god_reaction(old_god, new_god).c_str()).c_str(),
            old_god);
    }

    switch (old_god)
    {
    case GOD_XOM:
        _set_penance(old_god, 50);
        break;

    case GOD_KIKUBAAQUDGHA:
        mpr("무언가가 썩는 것을 느꼈다."); // in the state of Denmark?
        add_daction(DACT_ROT_CORPSES);
        _set_penance(old_god, 30);
        break;

    case GOD_YREDELEMNUL:
        you.duration[DUR_MIRROR_DAMAGE] = 0;
        if (query_da_counter(DACT_ALLY_YRED_SLAVE))
        {
            simple_god_message("은(는) 당신의 모든 언데드 수하를 거두어들였다!",
                               GOD_YREDELEMNUL);
            add_daction(DACT_ALLY_YRED_SLAVE);
            remove_all_companions(GOD_YREDELEMNUL);
        }
        _set_penance(old_god, 30);
        break;

    case GOD_VEHUMET:
        you.vehumet_gifts.clear();
        you.duration[DUR_VEHUMET_GIFT] = 0;
        _set_penance(old_god, 25);
        break;

    case GOD_MAKHLEB:
        _set_penance(old_god, 25);
        break;

    case GOD_TROG:
        if (you.attribute[ATTR_DIVINE_REGENERATION])
            remove_regen(true);

        add_daction(DACT_ALLY_TROG);

        _set_penance(old_god, 50);
        break;

    case GOD_BEOGH:
        // You might have lost water walking at a bad time...
        if (_need_water_walking())
            fall_into_a_pool(you.pos(), true, grd(you.pos()));

        if (query_da_counter(DACT_ALLY_BEOGH))
        {
            simple_god_message("은(는) 격노하며 소리쳤다. \"대체 넌 뭐 하는 놈인가?\"", GOD_BEOGH);
            mpr("당신의 모든 오크 추종자들이, 당신을 더이상 따르지 않기로 결심했다!",
                MSGCH_MONSTER_ENCHANT);
            add_daction(DACT_ALLY_BEOGH);
            remove_all_companions(GOD_BEOGH);
        }

        env.level_state |= LSTATE_BEOGH;

        _set_penance(old_god, 50);
        break;

    case GOD_SIF_MUNA:
        _set_penance(old_god, 50);
        break;

    case GOD_NEMELEX_XOBEH:
        nemelex_shuffle_decks();
        _set_penance(old_god, 150); // Nemelex penance is special
        break;

    case GOD_LUGONU:
        _set_penance(old_god, 50);
        break;

    case GOD_SHINING_ONE:
        if (was_haloed)
            mpr("성스러운 후광이 사라졌다.");

        if (you.duration[DUR_DIVINE_SHIELD])
            tso_remove_divine_shield();

        // Leaving TSO for a non-good god will make all your followers
        // abandon you.  Leaving him for a good god will make your holy
        // followers (daeva and angel servants) indifferent.
        if (!is_good_god(new_god))
            add_daction(DACT_ALLY_HOLY);
        else
            add_daction(DACT_HOLY_PETS_GO_NEUTRAL);

        _set_penance(old_god, 30);
        break;

    case GOD_ZIN:
        if (you.duration[DUR_DIVINE_STAMINA])
            zin_remove_divine_stamina();

        if (env.sanctuary_time)
            remove_sanctuary();

        // Leaving Zin for a non-good god will make neutral holies
        // (originally from TSO) abandon you.
        if (!is_good_god(new_god))
            add_daction(DACT_ALLY_HOLY);

        _set_penance(old_god, 25);
        break;

    case GOD_ELYVILON:
        you.duration[DUR_LIFESAVING] = 0;
        if (you.duration[DUR_DIVINE_VIGOUR])
            elyvilon_remove_divine_vigour();

        // Leaving Elyvilon for a non-good god will make neutral holies
        // (originally from TSO) abandon you.
        if (!is_good_god(new_god))
            add_daction(DACT_ALLY_HOLY);

        _set_penance(old_god, 30);
        break;

    case GOD_JIYVA:
        // Actually, doesn't unparalyse jellies.
        you.duration[DUR_JELLY_PRAYER] = 0;

        if (you.duration[DUR_SLIMIFY])
            you.duration[DUR_SLIMIFY] = 0;

        if (query_da_counter(DACT_ALLY_SLIME))
        {
            mpr("던전의 슬라임들은 이제 더 이상 당신의 동료가 아니다!",
                MSGCH_MONSTER_ENCHANT);
            add_daction(DACT_ALLY_SLIME);
        }

        _set_penance(old_god, 30);
        break;

    case GOD_FEDHAS:
        if (query_da_counter(DACT_ALLY_PLANT))
        {
            mpr("던전의 식물들은 이제 더 이상 당신의 동료가 아니다!",
                MSGCH_MONSTER_ENCHANT);
            add_daction(DACT_ALLY_PLANT);
        }
        _set_penance(old_god, 30);
        break;

    case GOD_ASHENZARI:
        if (you.transfer_skill_points > 0)
            ashenzari_end_transfer(false, true);
        you.duration[DUR_SCRYING] = 0;
        you.exp_docked = exp_needed(min<int>(you.max_level, 27)  + 1)
                       - exp_needed(min<int>(you.max_level, 27));
        you.exp_docked_total = you.exp_docked;
        _set_penance(old_god, 50);
        break;

    case GOD_CHEIBRIADOS:
    default:
        _set_penance(old_god, 25);
        break;
    }

    // When you start worshipping a non-good god, or no god, you make
    // all non-hostile holy beings that worship a good god hostile.
    if (!is_good_god(new_god) && query_da_counter(DACT_ALLY_HOLY))
    {
        mpr("성스러운 존재들이 당신을 저버리고 배신했다.", MSGCH_MONSTER_ENCHANT);
        add_daction(DACT_ALLY_HOLY);
    }

#ifdef USE_TILE_LOCAL
    tiles.layout_statcol();
    redraw_screen();
#endif

    // Evil hack.
    learned_something_new(HINT_EXCOMMUNICATE,
                          coord_def((int)new_god, old_piety));

    // Perhaps we abandoned Trog with everything but Spellcasting maxed out.
    check_selected_skills();
}

static string _sacrifice_message(string msg, const string& itname, bool glowing,
                                 bool plural, piety_gain_t piety_gain)
{
    if (glowing)
    {
        _replace(msg, "[", "");
        _replace(msg, "]", "");
    }
    else
        _erase_between(msg, "[", "]");
    _replace(msg, "%", (plural ? "" : "s"));
    _replace(msg, "&", (plural ? "are" : "is"));

    const char *tag_start, *tag_end;
    switch (piety_gain)
    {
    case PIETY_NONE:
        tag_start = "<lightgrey>";
        tag_end = "</lightgrey>";
        break;
    default:
    case PIETY_SOME:
        tag_start = tag_end = "";
        break;
    case PIETY_LOTS:
        tag_start = "<white>";
        tag_end = "</white>";
        break;
    }

    msg.insert(0, itname);
    msg = tag_start + msg + tag_end;

    return msg;
}

void print_sacrifice_message(god_type god, const item_def &item,
                             piety_gain_t piety_gain, bool your)
{
    if (god == GOD_ELYVILON && get_weapon_brand(item) == SPWPN_HOLY_WRATH)
    {
        // Weapons blessed by TSO don't get destroyed but are instead
        // returned whence they came. (jpeg)
        simple_god_message("은(는), 당신이 '엘리빌론'에게 바친 축복받은 아이템을 되가져갔다.",GOD_SHINING_ONE);
        return;
    }
    const string itname = item.name(true, your ? DESC_YOUR : DESC_THE);
    mpr(_sacrifice_message(_Sacrifice_Messages[god][piety_gain], itname,
                           itname.find("glowing") != string::npos,
                           item.quantity > 1,
                           piety_gain),
        MSGCH_GOD, god);
}

void nemelex_death_message()
{
    const piety_gain_t piety_gain = static_cast<piety_gain_t>
            (min(random2(you.piety) / 30, (int)PIETY_LOTS));
    mpr(_sacrifice_message(_Sacrifice_Messages[GOD_NEMELEX_XOBEH][piety_gain],
                           "당신의 육체", you.backlit(), false, piety_gain));
}

bool god_hates_attacking_friend(god_type god, const actor *fr)
{
    if (!fr || fr->kill_alignment() != KC_FRIENDLY)
        return false;

    monster_type species = fr->mons_species();

    if (mons_is_object(species))
        return false;
    switch (god)
    {
        case GOD_ZIN:
        case GOD_SHINING_ONE:
        case GOD_ELYVILON:
        case GOD_OKAWARU:
            return true;
        case GOD_BEOGH: // added penance to avoid killings for loot
            return (mons_genus(species) == MONS_ORC);
        case GOD_JIYVA:
            return mons_class_is_slime(species);
        case GOD_FEDHAS:
            return _fedhas_protects_species(species);
        default:
            return false;
    }
}

bool god_likes_items(god_type god, bool greedy_explore)
{
    if (greedy_explore && (!(Options.explore_stop & ES_GREEDY_SACRIFICEABLE)
                           || you_worship(GOD_ASHENZARI)))
        // Ash's sacrifice isn't trading items for piety so it shouldn't make
        // explore greedy for ?RC
    {
        return false;
    }

    if (god_likes_fresh_corpses(god))
        return true;

    switch (god)
    {
    case GOD_BEOGH:
    case GOD_NEMELEX_XOBEH:
    case GOD_ASHENZARI:
    case GOD_ELYVILON:
        return true;

    case NUM_GODS: case GOD_RANDOM: case GOD_NAMELESS:
        mprf(MSGCH_ERROR, "형편없는 신이다. 과자 하나 안주잖아! %d", static_cast<int>(god));

    default:
        return false;
    }
}

bool god_likes_item(god_type god, const item_def& item)
{
    if (!god_likes_items(god))
        return false;

    if (god_likes_fresh_corpses(god))
    {
        return (item.base_type == OBJ_CORPSES
                && item.sub_type == CORPSE_BODY
                && !food_is_rotten(item));
    }

    switch (god)
    {
    case GOD_ELYVILON:
        if (item_is_stationary(item)) // Held in a net?
            return false;
        return (item.base_type == OBJ_WEAPONS
                || item.base_type == OBJ_STAVES
                || item.base_type == OBJ_RODS
                || item.base_type == OBJ_MISSILES)
               // Once you've reached *** once, don't accept mundane weapon
               // sacrifices ever again just because of value.
               && (is_unholy_item(item) || is_evil_item(item)
                   || you.piety_max[GOD_ELYVILON] < piety_breakpoint(2));

    case GOD_BEOGH:
        return (item.base_type == OBJ_CORPSES
                && mons_genus(item.mon_type) == MONS_ORC);

    case GOD_NEMELEX_XOBEH:
        return (!is_deck(item)
                && !item.is_critical()
                && !item_is_rune(item)
                && item.base_type != OBJ_GOLD
                && (item.base_type != OBJ_MISCELLANY
                    || item.sub_type != MISC_HORN_OF_GERYON
                    || item.plus2));

    case GOD_ASHENZARI:
        return (item.base_type == OBJ_SCROLLS
                && item.sub_type == SCR_REMOVE_CURSE);

    default:
        return false;
    }
}

static bool _transformed_player_can_join_god(god_type which_god)
{
    if ((is_good_god(which_god) || which_god == GOD_FEDHAS)
        && you.form == TRAN_LICH)
    {
        return false;
    }

    if (which_god == GOD_ZIN && you.form != TRAN_NONE)
        return false;

    if (which_god == GOD_YREDELEMNUL
        && (you.form == TRAN_STATUE || you.petrified()))
    {   // it's rather hard to pray while petrified, though
        return false;
    }

    return true;
}

bool player_can_join_god(god_type which_god)
{
    if (you.species == SP_DEMIGOD)
        return false;

    if (is_good_god(which_god) && you.undead_or_demonic())
        return false;

    if (which_god == GOD_YREDELEMNUL && you.is_artificial())
        return false;

    if (which_god == GOD_BEOGH && !player_genus(GENPC_ORCISH))
        return false;

    // Fedhas hates undead, but will accept demonspawn.
    if (which_god == GOD_FEDHAS && you.holiness() == MH_UNDEAD)
        return false;

    if (which_god == GOD_SIF_MUNA && !you.spell_no)
        return false;

    return _transformed_player_can_join_god(which_god);
}

// Identify any interesting equipment when the player signs up with a
// new Service Pro^W^Wdeity.
static void _god_welcome_identify_gear()
{
    // Check for amulets of faith.
    item_def *amulet = you.slot_item(EQ_AMULET, false);
    if (amulet && amulet->sub_type == AMU_FAITH)
    {
        // The flash happens independent of item id.
        mpr("당신의 부적이 반짝였다!", MSGCH_GOD);
        flash_view_delay(god_colour(you.religion), 300);
        set_ident_type(*amulet, ID_KNOWN_TYPE);
        set_ident_flags(*amulet, ISFLAG_KNOW_TYPE);
    }

    if (you_worship(GOD_ASHENZARI))
    {
        // Seemingly redundant with auto_id_inventory(), but we don't want to
        // announce items where the only new information is their cursedness.
        for (int i = 0; i < ENDOFPACK; i++)
            if (you.inv[i].defined())
                you.inv[i].flags |= ISFLAG_KNOW_CURSE;

        set_ident_type(OBJ_SCROLLS, SCR_REMOVE_CURSE, ID_KNOWN_TYPE);
        set_ident_type(OBJ_SCROLLS, SCR_CURSE_WEAPON, ID_KNOWN_TYPE);
        set_ident_type(OBJ_SCROLLS, SCR_CURSE_ARMOUR, ID_KNOWN_TYPE);
        set_ident_type(OBJ_SCROLLS, SCR_CURSE_JEWELLERY, ID_KNOWN_TYPE);
        auto_id_inventory();
        ash_detect_portals(true);
    }

    // detect evil weapons
    if (you_worship(GOD_ELYVILON))
        auto_id_inventory();
}

void god_pitch(god_type which_god)
{
    if (which_god == GOD_BEOGH && grd(you.pos()) != DNGN_ALTAR_BEOGH)
        mpr("당신은 베오그의 선교자에게 절을 올렸다.");
    else
    {
        mprf("당신은 %s의 제단%s, 기도를 드렸다.", _(god_name(which_god).c_str()),
         you.form == TRAN_WISP   ? " 주위를 맴돌며," :
         you.form == TRAN_BAT    ? " 위에 걸터앉아" :
         you.flight_mode()       ? " 위에서 침착히 떠다니며" :
         you.form == TRAN_SPIDER ? "에 매달려" :
         you.form == TRAN_STATUE ? "앞에서 자세를 잡고" :
         you.form == TRAN_ICE_BEAST
             || you.form == TRAN_DRAGON
             || you.form == TRAN_PIG    ? "앞에 머리숙여" :
         you.form == TRAN_TREE   ? "앞에서 가지를 일렁이며" :
         you.form == TRAN_FUNGUS ? "앞에서 포자를 흘리며" :
         you.form == TRAN_PORCUPINE ? "앞에서 가시를 말아 웅크리고" :
         you.form == TRAN_JELLY  ? "앞에서 침착하게 몸을 떨며" :
         you.species == SP_NAGA  ? "에 몸을 말고 앉아" :
         // < TGWi> you curl up on the altar and go to sleep
         you.species == SP_FELID ? "앞에 앉아" :
         // duplicated because of forms
         you.species == SP_DJINNI ? "앞에 엄숙하게 떠서" :
                                   "에 무릎꿇어",
         god_name(which_god).c_str());
    }
    more();

    // Note: using worship we could make some gods not allow followers to
    // return, or not allow worshippers from other religions. - bwr

    // Gods can be racist...
    if (!player_can_join_god(which_god))
    {
        you.turn_is_over = false;
        if (which_god == GOD_SIF_MUNA)
        {
            simple_god_message("은(는) 당신과 같은 무지한 자로부터의 숭배는 받아들이지 않는다!",
                               which_god);
        }
        else if (!_transformed_player_can_join_god(which_god))
        {
            simple_god_message("은(는) 말했다. \"어찌 그러한 몰골로 나를 숭배하려 하는가! "
                               "썩 물러가라!\"",
                               which_god);
        }
        else
        {
            simple_god_message("은(는) 당신 같은 존재로부터의 숭배는 받아들이지 않는다!"
                               "",
                               which_god);
        }
        return;
    }

    if (which_god == GOD_LUGONU && you.penance[GOD_LUGONU])
    {
        you.turn_is_over = false;
        simple_god_message("은(는) 당신을 쉽게 용서해 주려 하지 않는다!", which_god);
        return;
    }

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "god_pitch");
#endif

    describe_god(which_god, false);

    snprintf(info, INFO_SIZE, "당신은 이 종파를 %s신앙하는 것을 희망합니까?",
             (you.worshipped[which_god]) ? "다시 " : "");

    cgotoxy(1, 18, GOTO_CRT);
    textcolor(channel_to_colour(MSGCH_PROMPT));
    if (!yesno(info, false, 'n', true, true, false, NULL, GOTO_CRT))
    {
        you.turn_is_over = false; // Okay, opt out.
        redraw_screen();
        return;
    }

    // OK, so join the new religion.
    redraw_screen();

    const god_type old_god = you.religion;
    const int old_piety = you.piety;

    // Leave your prior religion first.
    if (!you_worship(GOD_NO_GOD))
        excommunication(which_god);

    // Welcome to the fold!
    you.religion = static_cast<god_type>(which_god);

    if (you_worship(GOD_XOM))
    {
        // Xom uses piety and gift_timeout differently.
        you.piety = HALF_MAX_PIETY;
        you.gift_timeout = random2(40) + random2(40);
    }
    else
    {
        you.piety = 15; // to prevent near instant excommunication
        if (you.piety_max[you.religion] < 15)
            you.piety_max[you.religion] = 15;
        you.piety_hysteresis = 0;
        you.gift_timeout = 0;
    }

    set_god_ability_slots();    // remove old god's slots, reserve new god's
#ifdef DGL_WHEREIS
    whereis_record();
#endif

    mark_milestone("god.worship", _(god_name(you.religion).c_str()) + std::string("의 신자가 됨.")); // "became a worshipper of "
                   // + god_name(you.religion) + ".");

    simple_god_message(
        make_stringf("은(는) 당신의 입교를 %s환영했다!",
                     you.worshipped[which_god] ? "다시 " : "").c_str());
    more();
    if (crawl_state.game_is_tutorial())
    {
        // Tutorial needs minor destruction usable.
        gain_piety(35, 1, true, false);
    }

    if (you_worship(GOD_BEOGH))
    {
        // The player's symbol depends on Beogh worship.
        update_player_symbol();
    }

    _god_welcome_identify_gear();
    ash_check_bondage();

    // Chei worshippers start their stat gain immediately.
    if (you_worship(GOD_CHEIBRIADOS))
    {
        simple_god_message("은(는) 당신의 움직임이 느려진 만큼, 당신의 능력치를 보정해줬다."
                           "");
        notify_stat_change("Cheibriados worship");
    }

    // We disable all magical skills to avoid accidentally angering Trog.
    if (you_worship(GOD_TROG))
    {
        for (int sk = SK_SPELLCASTING; sk <= SK_LAST_MAGIC; ++sk)
            if (you.skills[sk])
                you.train[sk] = 0;
    }

    // Elyvilon gives you invocations immediately.
    if (you_worship(GOD_ELYVILON))
        you.start_train.insert(SK_INVOCATIONS);

    if (you_worship(GOD_OKAWARU)
        && player_mutation_level(MUT_DEMONIC_GUARDIAN))
    {
        mpr("Your demonic guardian will not assist you as long as you worship "
            "Okawaru.", MSGCH_GOD);
    }

    // When you start worshipping a good god, you make all non-hostile
    // unholy and evil beings hostile; when you start worshipping Zin,
    // you make all non-hostile unclean and chaotic beings hostile; and
    // when you start worshipping Trog, you make all non-hostile magic
    // users hostile.
    if (is_good_god(you.religion)
        && query_da_counter(DACT_ALLY_UNHOLY_EVIL))
    {
        add_daction(DACT_ALLY_UNHOLY_EVIL);
        mpr("당신의 언데드와 악마 동료들이 당신을 저버리고 배신했다.", MSGCH_MONSTER_ENCHANT);
    }

    if (you_worship(GOD_ZIN)
        && query_da_counter(DACT_ALLY_UNCLEAN_CHAOTIC))
    {
        add_daction(DACT_ALLY_UNCLEAN_CHAOTIC);
        mpr("당신의 혼돈의 동료들이 당신을 저버리고 배신했다.",
            MSGCH_MONSTER_ENCHANT);
    }
    else if (you_worship(GOD_TROG)
             && query_da_counter(DACT_ALLY_SPELLCASTER))
    {
        add_daction(DACT_ALLY_SPELLCASTER);
        mpr("당신의 마법사용자 동료들이 당신을 저버리고 배신했다.", MSGCH_MONSTER_ENCHANT);
    }

    if (you_worship(GOD_ELYVILON))
    {
        mpr("당신은 이제 엘리빌론에게 기도를 드림으로, 바닥에 있는 무기들을 회수할 수 있다"
            ".", MSGCH_GOD);
        mpr("당신은 이제 다른 존재들에게 미약한 치유를 행할 수 있다.", MSGCH_GOD);
    }
    else if (you_worship(GOD_TROG))
    {
        mpr("당신은 트로그의 힘으로, 주변의 마법서들을 불사를 수 있다.", MSGCH_GOD);
    }
    else if (you_worship(GOD_FEDHAS))
    {
        mpr("당신은 페다스에게 기도를 드림으로, 시체들의 부패를 촉진시킬 수 있다.",
            MSGCH_GOD);
        mpr("던전의 식물들이, 당신에게 더 이상 적대감을 보이지 않는다.",
            MSGCH_MONSTER_ENCHANT);
        if (env.forest_awoken_until)
            for (monster_iterator mi; mi; ++mi)
                mi->del_ench(ENCH_AWAKEN_FOREST);
    }

    if (you.worshipped[you.religion] < 100)
        you.worshipped[you.religion]++;

    take_note(Note(NOTE_GET_GOD, you.religion));

    // Currently, penance is just zeroed.  This could be much more
    // interesting.
    you.penance[you.religion] = 0;

    if (is_good_god(you.religion) && is_good_god(old_god))
    {
		std::string good_god_temp1 = ((old_god == GOD_ELYVILON) ? "니라" : ((old_god == GOD_SHINING_ONE) ? "군" : "네"));
		std::string good_god_temp2 = ((old_god == GOD_ELYVILON) ? "어라" : ((old_god == GOD_SHINING_ONE) ? "거라" : "게나"));
        // Some feedback that piety moved over.
        switch (you.religion)
        {
        case GOD_ELYVILON:
			simple_god_message(("은(는) 말했다. \"작별이" + good_god_temp1 + ". 이제 " 
                               + std::string(_(god_name(you.religion).c_str())) + "와(과) 함께, 순수한 자들을 도와주" + good_god_temp2 + ".\"").c_str(), old_god);
            break;
        case GOD_SHINING_ONE:
			simple_god_message(("은(는) 말했다. \"작별이" + good_god_temp1 + ". 이제 " 
                               + std::string(_(god_name(you.religion).c_str())) + "와(과) 함께, 악한 자들을 무찌르" + good_god_temp2 + ".\"").c_str(), old_god);
            break;
        case GOD_ZIN:
            simple_god_message(("은(는) 말했다. \"작별이" + good_god_temp1 + ". 이제 " 
                               + std::string(_(god_name(you.religion).c_str())) + "와(과) 함께, 그의 가르침을 전파하" + good_god_temp2 + ".\"").c_str(), old_god);
            break;
        default:
            mpr("Unknown good god.", MSGCH_ERROR);
        }
        // Give a piety bonus when switching between good gods.
        if (old_piety > piety_breakpoint(0))
            gain_piety(old_piety - piety_breakpoint(0), 2, true, false);
    }

    // Warn if a good god is starting wrath now.
    if (old_god != GOD_ELYVILON && you.penance[GOD_ELYVILON]
        && god_hates_your_god(GOD_ELYVILON, you.religion))
    {
        simple_god_message("은(는) 말했다. \"네 악행은 용서받지 못할 것이니라!\"",
                           GOD_ELYVILON);
    }
    if (old_god != GOD_SHINING_ONE && you.penance[GOD_SHINING_ONE]
        && god_hates_your_god(GOD_SHINING_ONE, you.religion))
    {
        simple_god_message("은(는) 말했다. \"넌 네 악행의 대가를 치르게 될 것이다!\"",
                           GOD_SHINING_ONE);
    }
    if (old_god != GOD_ZIN && you.penance[GOD_ZIN]
        && god_hates_your_god(GOD_ZIN, you.religion))
    {
        /// chaos 혹은 evil
        simple_god_message(make_stringf("은(는) 말했다. \"자네는 곧 %s 신을 믿은 대가로 고통을 받을 것이네!\"",
                                        is_chaotic_god(you.religion) ? "혼돈스러운" : "사악한").c_str(),
                           GOD_ZIN);
    }

    // Note that you.worshipped[] has already been incremented.
    if (you.char_class == JOB_MONK && had_gods() <= 1)
    {
        // monks get bonus piety for first god
        gain_piety(35, 1, true, false);
    }

    if (you_worship(GOD_LUGONU) && you.worshipped[GOD_LUGONU] == 1)
        gain_piety(20, 1, true, false);  // allow instant access to first power

    // Complimentary jelly upon joining.
    if (you_worship(GOD_JIYVA))
    {
        if (!_has_jelly())
        {
            monster_type mon = MONS_JELLY;
            mgen_data mg(mon, BEH_STRICT_NEUTRAL, &you, 0, 0, you.pos(),
                         MHITNOT, 0, GOD_JIYVA);

            _delayed_monster(mg);
            simple_god_message("은(는) 당신에게 젤리를 선물했다!");
        }
    }

    // Need to pay St. Peters.
    if (you_worship(GOD_ZIN) && you.attribute[ATTR_DONATIONS] * 9 < you.gold)
    {
        item_def lucre;
        lucre.base_type = OBJ_GOLD;
        // If you worshipped Zin before, the already tithed for part is fine.
        lucre.quantity = you.gold - you.attribute[ATTR_DONATIONS] * 9;
        // Use the harsh acquirement pricing -- with a cap at +50 piety.
        // We don't want you get max piety at start just because you're filthy
        // rich.  In that case, you have to donate again more...  That the poor
        // widow is not spared doesn't mean the rich can't be milked for more.
        lucre.props["acquired"] = 0;
        you.gold -= zin_tithe(lucre, lucre.quantity, false, true);
    }

    // Refresh wielded/quivered weapons in case we have a new conduct
    // on them.
    you.wield_change = true;
    you.redraw_quiver = true;

    you.redraw_title = true;

#ifdef USE_TILE_LOCAL
    tiles.layout_statcol();
    redraw_screen();
#endif

    learned_something_new(HINT_CONVERT);
}

int had_gods()
{
    int count = 0;
    for (int i = 0; i < NUM_GODS; i++)
        count += you.worshipped[i];
    return count;
}

bool god_likes_your_god(god_type god, god_type your_god)
{
    return (is_good_god(god) && is_good_god(your_god));
}

bool god_hates_your_god(god_type god, god_type your_god)
{
    // Gods do not hate themselves.
    if (god == your_god)
        return false;

    // Non-good gods always hate your current god.
    if (!is_good_god(god))
        return true;

    // Zin hates chaotic gods.
    if (god == GOD_ZIN && is_chaotic_god(your_god))
        return true;

    return is_evil_god(your_god);
}

bool god_hates_cannibalism(god_type god)
{
    return (is_good_god(god) || god == GOD_BEOGH);
}

bool god_hates_killing(god_type god, const monster* mon)
{
    // Must be at least a creature of sorts.  Smacking down an enchanted
    // weapon or disrupting a lightning doesn't count.  Technically, this
    // might raise a concern about necromancy but zombies traditionally
    // count as creatures and that's the average person's (even if not ours)
    // intuition.
    if (mons_is_object(mon->type))
        return false;

    bool retval = false;
    const mon_holy_type holiness = mon->holiness();

    switch (holiness)
    {
        case MH_HOLY:
            retval = (is_good_god(god));
            break;
        case MH_NATURAL:
            retval = (god == GOD_ELYVILON);
            break;
        default:
            break;
    }

    if (god == GOD_FEDHAS)
        retval = (fedhas_protects(mon));

    return retval;
}

bool god_likes_fresh_corpses(god_type god)
{
    if (god == GOD_LUGONU)
        return !player_in_branch(BRANCH_ABYSS);

    return (god == GOD_OKAWARU
            || god == GOD_MAKHLEB
            || god == GOD_TROG);
}

bool god_likes_spell(spell_type spell, god_type god)
{
    switch (god)
    {
    case GOD_VEHUMET:
        return vehumet_supports_spell(spell);
    case GOD_KIKUBAAQUDGHA:
        return spell_typematch(spell, SPTYP_NECROMANCY);
    default: // quash unhandled constants warnings
        return false;
    }
}

bool god_hates_spell(spell_type spell, god_type god, bool rod_spell)
{
    if (is_good_god(god) && (is_unholy_spell(spell) || is_evil_spell(spell)))
        return true;

    if (god == GOD_TROG && !rod_spell)
        return true;

    unsigned int disciplines = get_spell_disciplines(spell);

    switch (god)
    {
    case GOD_ZIN:
        if (is_unclean_spell(spell) || is_chaotic_spell(spell))
            return true;
        break;
    case GOD_SHINING_ONE:
        // TSO hates using poison, but is fine with curing it.
        if ((disciplines & SPTYP_POISON) && spell != SPELL_CURE_POISON)
            return true;
        break;
    case GOD_YREDELEMNUL:
        if (spell == SPELL_STATUE_FORM)
            return true;
        break;
    case GOD_FEDHAS:
        if (is_corpse_violating_spell(spell))
            return true;
        break;
    case GOD_CHEIBRIADOS:
        if (is_hasty_spell(spell))
            return true;
        break;
    default:
        break;
    }
    return false;
}

bool god_loathes_spell(spell_type spell, god_type god)
{
    if (spell == SPELL_NECROMUTATION && is_good_god(god))
        return true;
    if (spell == SPELL_STATUE_FORM && god == GOD_YREDELEMNUL)
        return true;
    return false;
}

bool god_can_protect_from_harm(god_type god)
{
    switch (god)
    {
    case GOD_BEOGH:
        return !you.penance[god];
    case GOD_ZIN:
    case GOD_SHINING_ONE:
    case GOD_ELYVILON:
        return true;
    default:
        return false;
    }
}

int elyvilon_lifesaving()
{
    if (!you_worship(GOD_ELYVILON))
        return 0;

    if (you.piety < piety_breakpoint(0))
        return 0;

    return you.piety > 130 ? 2 : 1;
}


bool god_protects_from_harm()
{
    if (you.duration[DUR_LIFESAVING])
        switch (elyvilon_lifesaving())
        {
        case 1:
            if (random2(piety_scale(you.piety)) >= piety_breakpoint(0))
                return true;
            break;
        case 2:
            // Reliable lifesaving is costly.
            lose_piety(21 + random2(20));
            return true;
        }

    if (god_can_protect_from_harm(you.religion)
        && (one_chance_in(10) || x_chance_in_y(piety_scale(you.piety), 1000)))
    {
        return true;
    }

    return false;
}

//jmf: moved stuff from effects::handle_time()
void handle_god_time()
{
    // First count the number of gods to whom we owe penance.
    unsigned int penance_count = 0;
    for (int i = GOD_NO_GOD; i < NUM_GODS; ++i)
    {
        // Nemelex penance is special: it's only "active"
        // when penance > 100, else it's passive.
        if (player_under_penance((god_type) i) && (i != GOD_NEMELEX_XOBEH
                               || you.penance[i] > 100))
        {
            penance_count++;
        }
    }
    // Now roll to see whether we get retribution and from which god.
    const unsigned int which_penance = random2(100);
    if (which_penance < penance_count)
    {
        unsigned int count = 0;
        for (int i = GOD_NO_GOD; i < NUM_GODS; ++i)
        {
            if (player_under_penance((god_type) i) && (i != GOD_NEMELEX_XOBEH
                                   || you.penance[i] > 100))
            {
                if (count == which_penance)
                {
                    divine_retribution((god_type)i);
                    break;
                }
                count++;
            }
        }
    }

    // Update the god's opinion of the player.
    if (!you_worship(GOD_NO_GOD))
    {
        switch (you.religion)
        {
        case GOD_XOM:
// Moved to _player_reacts()
//            xom_tick();
            return;

        case GOD_ELYVILON:
            if (one_chance_in(50))
                lose_piety(1);
            return;

        case GOD_SHINING_ONE:
            if (one_chance_in(35))
                lose_piety(1);
            break;

        case GOD_JIYVA:
            if (one_chance_in(20))
                lose_piety(1);
            break;

        case GOD_YREDELEMNUL:
        case GOD_KIKUBAAQUDGHA:
        case GOD_VEHUMET:
        case GOD_ZIN:
            if (one_chance_in(17))
                lose_piety(1);
            break;

        // These gods accept corpses, so they time-out faster.
        case GOD_OKAWARU:
        case GOD_TROG:
            if (one_chance_in(14))
                lose_piety(1);
            break;

        case GOD_MAKHLEB:
        case GOD_BEOGH:
        case GOD_LUGONU:
            if (one_chance_in(16))
                lose_piety(1);
            break;

        case GOD_NEMELEX_XOBEH:
            // Nemelex is relatively patient.
            if (one_chance_in(35))
                lose_piety(1);
            break;

        case GOD_SIF_MUNA:
            // [dshaligram] Sif Muna is now very patient - has to be
            // to make up for the new spell training requirements, else
            // it's practically impossible to get Master of Arcane status.
            if (one_chance_in(100))
                lose_piety(1);
            break;

        case GOD_FEDHAS:
        case GOD_CHEIBRIADOS:
            // Fedhas' piety is stable over time but we need a case here to
            // avoid the error message below.
            break;

        case GOD_ASHENZARI:
            if (one_chance_in(25))
                lose_piety(1);
            break;

        default:
            die("형편없는 신이다. 주교님도 안 계시잖아!");
            return;
        }

        if (you.piety < 1)
            excommunication();
    }
}

// yet another wrapper for mpr() {dlb}:
void simple_god_message(const char *event, god_type which_deity)
{
    string msg = uppercase_first(_(god_name(which_deity).c_str())) + event;
    msg = apostrophise_fixup(msg);
    god_speaks(which_deity, msg.c_str());
}

int god_colour(god_type god) // mv - added
{
    switch (god)
    {
    case GOD_ZIN:
    case GOD_SHINING_ONE:
    case GOD_ELYVILON:
    case GOD_OKAWARU:
    case GOD_FEDHAS:
        return CYAN;

    case GOD_YREDELEMNUL:
    case GOD_KIKUBAAQUDGHA:
    case GOD_MAKHLEB:
    case GOD_VEHUMET:
    case GOD_TROG:
    case GOD_BEOGH:
    case GOD_LUGONU:
    case GOD_ASHENZARI:
        return LIGHTRED;

    case GOD_XOM:
        return YELLOW;

    case GOD_NEMELEX_XOBEH:
        return LIGHTMAGENTA;

    case GOD_SIF_MUNA:
        return LIGHTBLUE;

    case GOD_JIYVA:
        return GREEN;

    case GOD_CHEIBRIADOS:
        return LIGHTCYAN;

    case GOD_NO_GOD:
    case NUM_GODS:
    case GOD_RANDOM:
    case GOD_NAMELESS:
    default:
        break;
    }

    return YELLOW;
}

colour_t god_message_altar_colour(god_type god)
{
    int rnd;

    switch (god)
    {
    case GOD_SHINING_ONE:
        return YELLOW;

    case GOD_ZIN:
        return WHITE;

    case GOD_ELYVILON:
        return LIGHTBLUE;     // Really, LIGHTGREY but that's plain text.

    case GOD_OKAWARU:
        return CYAN;

    case GOD_YREDELEMNUL:
        return (coinflip() ? DARKGREY : RED);

    case GOD_BEOGH:
        return (coinflip() ? BROWN : LIGHTRED);

    case GOD_KIKUBAAQUDGHA:
        return DARKGREY;

    case GOD_FEDHAS:
        return (coinflip() ? BROWN : GREEN);

    case GOD_XOM:
        return (random2(15) + 1);

    case GOD_VEHUMET:
        rnd = random2(3);
        return ((rnd == 0) ? LIGHTMAGENTA :
                (rnd == 1) ? LIGHTRED
                           : LIGHTBLUE);

    case GOD_MAKHLEB:
        rnd = random2(3);
        return ((rnd == 0) ? RED :
                (rnd == 1) ? LIGHTRED
                           : YELLOW);

    case GOD_TROG:
        return RED;

    case GOD_NEMELEX_XOBEH:
        return LIGHTMAGENTA;

    case GOD_SIF_MUNA:
        return BLUE;

    case GOD_LUGONU:
        return LIGHTRED;

    case GOD_CHEIBRIADOS:
        return LIGHTCYAN;

    case GOD_JIYVA:
        return (coinflip() ? GREEN : LIGHTGREEN);

    default:
        return YELLOW;
    }
}

int piety_rank(int piety)
{
    if (piety < 0)
        piety = you.piety;

    if (you_worship(GOD_XOM))
    {
        const int breakpoints[] = { 20, 50, 80, 120, 180, INT_MAX };
        for (unsigned int i = 0; i < ARRAYSZ(breakpoints); ++i)
            if (piety <= breakpoints[i])
                return i + 1;
        die("INT_MAX is no good");
    }

    const int breakpoints[] = { 160, 120, 100, 75, 50, 30, 1 };
    const int numbreakpoints = ARRAYSZ(breakpoints);

    for (int i = 0; i < numbreakpoints; ++i)
    {
        if (piety >= breakpoints[i])
            return numbreakpoints - i;
    }

    return 0;
}

int piety_breakpoint(int i)
{
    int breakpoints[MAX_GOD_ABILITIES + 1] = { 30, 50, 75, 100, 120, 160 };
    if (i >= MAX_GOD_ABILITIES + 1 || i < 0)
        return 255;
    else
        return breakpoints[i];
}

// Returns true if the Shining One doesn't mind your using unchivalric
// attacks on this creature.
bool tso_unchivalric_attack_safe_monster(const monster* mon)
{
    const mon_holy_type holiness = mon->holiness();
    return (mons_intel(mon) < I_NORMAL
            || mon->undead_or_demonic()
            || mon->is_shapeshifter() && (mon->flags & MF_KNOWN_SHIFTER)
            || !mon->is_holy() && holiness != MH_NATURAL);
}

int get_monster_tension(const monster* mons, god_type god)
{
    if (!mons->alive())
        return 0;

    if (you.see_cell(mons->pos()))
    {
        if (!mons_can_hurt_player(mons))
            return 0;
    }

    const mon_attitude_type att = mons_attitude(mons);
    if (att == ATT_GOOD_NEUTRAL || att == ATT_NEUTRAL)
        return 0;

    if (mons->cannot_act() || mons->asleep() || mons_is_fleeing(mons))
        return 0;

    int exper = exper_value(mons);
    if (exper <= 0)
        return 0;

    // Almost dead monsters don't count as much.
    exper *= mons->hit_points;
    exper /= mons->max_hit_points;

    bool gift = false;

    if (god != GOD_NO_GOD)
        gift = mons_is_god_gift(mons, god);

    if (att == ATT_HOSTILE)
    {
        // God is punishing you with a hostile gift, so it doesn't
        // count towards tension.
        if (gift)
            return 0;
    }
    else if (att == ATT_FRIENDLY)
    {
        // Friendly monsters being around to help you reduce
        // tension.
        exper = -exper;

        // If it's a god gift, it reduces tension even more, since
        // the god is already helping you out.
        if (gift)
            exper *= 2;
    }

    if (att != ATT_FRIENDLY)
    {
        if (!you.visible_to(mons))
            exper /= 2;
        if (!mons->visible_to(&you))
            exper *= 2;
    }

    if (mons->confused() || mons->caught())
        exper /= 2;

    if (mons->has_ench(ENCH_SLOW))
    {
        exper *= 2;
        exper /= 3;
    }

    if (mons->has_ench(ENCH_HASTE))
    {
        exper *= 3;
        exper /= 2;
    }

    if (mons->has_ench(ENCH_MIGHT))
    {
        exper *= 5;
        exper /= 4;
    }

    if (mons->berserk_or_insane())
    {
        // in addition to haste and might bonuses above
        exper *= 3;
        exper /= 2;
    }

    return exper;
}

int get_tension(god_type god)
{
    int total = 0;

    bool nearby_monster = false;
    for (radius_iterator ri(you.get_los()); ri; ++ri)
    {
        const monster* mon = monster_at(*ri);

        if (mon && mon->alive() && you.can_see(mon))
        {
            int exper = get_monster_tension(mon, god);

            if (!mon->wont_attack() && !mon->withdrawn())
                nearby_monster = true;

            total += exper;
        }
    }

    // At least one monster has to be nearby, for tension to count.
    if (!nearby_monster)
        return 0;

    const int scale = 1;

    int tension = total;

    // Tension goes up inversely proportional to the percentage of max
    // hp you have.
    tension *= (scale + 1) * you.hp_max;
    tension /= max(you.hp_max + scale * you.hp, 1);

    // Divides by 1 at level 1, 200 at level 27.
    const int exp_lev  = you.get_experience_level();
    const int exp_need = exp_needed(exp_lev);
    const int factor   = (int)ceil(sqrt(exp_need / 30.0));
    const int div      = 1 + factor;

    tension /= div;

    if (player_in_branch(BRANCH_ABYSS))
    {
        if (tension < 2)
            tension = 2;
        else
        {
            tension *= 3;
            tension /= 2;
        }
    }

    if (you.cannot_act())
    {
        tension *= 10;
        tension  = max(1, tension);

        return tension;
    }

    if (you.confused())
        tension *= 2;

    if (you.caught())
        tension *= 2;

    if (you.duration[DUR_SLOW])
    {
        tension *= 3;
        tension /= 2;
    }

    if (you.duration[DUR_HASTE])
    {
        tension *= 2;
        tension /= 3;
    }

    return max(0, tension);
}

int get_fuzzied_monster_difficulty(const monster *mons)
{
    double factor = sqrt(exp_needed(you.experience_level) / 30.0);
    int exp = exper_value(mons) * 100;
    exp = random2(exp) + random2(exp);
    return exp / (1 + factor);
}

/////////////////////////////////////////////////////////////////////////////
// Stuff for placing god gift monsters after the player's turn has ended.
/////////////////////////////////////////////////////////////////////////////

static vector<mgen_data>       _delayed_data;
static deque<delayed_callback> _delayed_callbacks;
static deque<unsigned int>     _delayed_done_trigger_pos;
static deque<delayed_callback> _delayed_done_callbacks;
static deque<string>      _delayed_success;
static deque<string>      _delayed_failure;

static void _delayed_monster(const mgen_data &mg, delayed_callback callback)
{
    _delayed_data.push_back(mg);
    _delayed_callbacks.push_back(callback);
}

static void _delayed_monster_done(string success, string failure,
                                  delayed_callback callback)
{
    const unsigned int size = _delayed_data.size();
    ASSERT(size > 0);

    _delayed_done_trigger_pos.push_back(size - 1);
    _delayed_success.push_back(success);
    _delayed_failure.push_back(failure);
    _delayed_done_callbacks.push_back(callback);
}

static void _place_delayed_monsters()
{
    int      placed   = 0;
    god_type prev_god = GOD_NO_GOD;
    for (unsigned int i = 0; i < _delayed_data.size(); i++)
    {
        mgen_data &mg          = _delayed_data[i];
        delayed_callback cback = _delayed_callbacks[i];

        if (prev_god != mg.god)
        {
            placed   = 0;
            prev_god = mg.god;
        }

        monster *mon = create_monster(mg);

        if (cback)
            (*cback)(mg, mon, placed);

        if (mon)
        {
            if (you_worship(GOD_YREDELEMNUL) || you_worship(GOD_BEOGH))
                add_companion(mon);
            placed++;
        }

        if (!_delayed_done_trigger_pos.empty()
            && _delayed_done_trigger_pos[0] == i)
        {
            cback = _delayed_done_callbacks[0];

            string msg;
            if (placed > 0)
                msg = _delayed_success[0];
            else
                msg = _delayed_failure[0];

            if (placed == 1)
            {
                msg = replace_all(msg, "@a@", "a");
                msg = replace_all(msg, "@an@", "an");
            }
            else
            {
                msg = replace_all(msg, " @a@", "");
                msg = replace_all(msg, " @an@", "");
            }

            if (placed == 1)
                msg = replace_all(msg, "@s@", "");
            else
                msg = replace_all(msg, "@s@", "s");

            prev_god = GOD_NO_GOD;
            _delayed_done_trigger_pos.pop_front();
            _delayed_success.pop_front();
            _delayed_failure.pop_front();
            _delayed_done_callbacks.pop_front();

            if (msg == "")
            {
                if (cback)
                    (*cback)(mg, mon, placed);
                continue;
            }

            // Fake its coming from simple_god_message().
            if (msg[0] == ' ' || msg[0] == '\'')
                msg = uppercase_first(_(god_name(mg.god).c_str())) + msg;

            msg = apostrophise_fixup(msg);
            trim_string(msg);

            god_speaks(mg.god, msg.c_str());

            if (cback)
                (*cback)(mg, mon, placed);
        }
    }

    _delayed_data.clear();
    _delayed_callbacks.clear();
    _delayed_done_trigger_pos.clear();
    _delayed_success.clear();
    _delayed_failure.clear();
}


static bool _is_god(god_type god)
{
    return (god > GOD_NO_GOD && god < NUM_GODS);
}

static bool _is_temple_god(god_type god)
{
    if (!_is_god(god))
        return false;

    switch (god)
    {
    case GOD_NO_GOD:
    case GOD_LUGONU:
    case GOD_BEOGH:
    case GOD_JIYVA:
        return false;

    default:
        return true;
    }
}

static bool _is_nontemple_god(god_type god)
{
    return (_is_god(god) && !_is_temple_god(god));
}

static bool _cmp_god_by_name(god_type god1, god_type god2)
{
    return (god_name(god1, false) < god_name(god2, false));
}

// Vector of temple gods.
// Sorted by name for the benefit of the overview.
vector<god_type> temple_god_list()
{
    vector<god_type> god_list;
    for (int i = 0; i < NUM_GODS; i++)
    {
        god_type god = static_cast<god_type>(i);
        if (_is_temple_god(god))
            god_list.push_back(god);
    }
    sort(god_list.begin(), god_list.end(), _cmp_god_by_name);
    return god_list;
}

// Vector of non-temple gods.
// Sorted by name for the benefit of the overview.
vector<god_type> nontemple_god_list()
{
    vector<god_type> god_list;
    for (int i = 0; i < NUM_GODS; i++)
    {
        god_type god = static_cast<god_type>(i);
        if (_is_nontemple_god(god))
            god_list.push_back(god);
    }
    sort(god_list.begin(), god_list.end(), _cmp_god_by_name);
    return god_list;
}
