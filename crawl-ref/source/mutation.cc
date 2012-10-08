/**
 * @file
 * @brief Functions for handling player mutations.
**/


//변이 관련 번역 시작.

#include "AppHdr.h"
#include "mutation.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sstream>

#include "externs.h"

#include "abl-show.h"
#include "cio.h"
#include "coordit.h"
#include "delay.h"
#include "defines.h"
#include "dactions.h"
#include "effects.h"
#include "env.h"
#include "godabil.h"
#include "godpassive.h"
#include "itemprop.h"
#include "items.h"
#include "korean.h"
#include "menu.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-place.h"
#include "mon-iter.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "notes.h"
#include "ouch.h"
#include "player.h"
#include "player-stats.h"
#include "religion.h"
#include "random.h"
#include "skills2.h"
#include "stuff.h"
#include "transform.h"
#include "hints.h"
#include "xom.h"

static int _body_covered();

static mutation_def mut_data[] = {

#include "mutation-data.h"

};

static const body_facet_def _body_facets[] =
{
    //{ EQ_NONE, MUT_FANGS, 1 },
    { EQ_HELMET, MUT_HORNS, 1 },
    { EQ_HELMET, MUT_ANTENNAE, 1 },
    //{ EQ_HELMET, MUT_BEAK, 1 },
    { EQ_GLOVES, MUT_CLAWS, 3 },
    { EQ_BOOTS, MUT_HOOVES, 3 },
    { EQ_BOOTS, MUT_TALONS, 3 }
};

equipment_type beastly_slot(int mut)
{
    switch (mut)
    {
    case MUT_HORNS:
    case MUT_ANTENNAE:
    case MUT_BEAK:
        return EQ_HELMET;
    case MUT_CLAWS:
        return EQ_GLOVES;
    case MUT_HOOVES:
    case MUT_TALONS:
    case MUT_TENTACLE_SPIKE:
        return EQ_BOOTS;
    default:
        return EQ_NONE;
    }
}

static int mut_index[NUM_MUTATIONS];

void init_mut_index()
{
    for (int i = 0; i < NUM_MUTATIONS; ++i)
        mut_index[i] = -1;

    for (unsigned int i = 0; i < ARRAYSZ(mut_data); ++i)
    {
        const mutation_type mut = mut_data[i].mutation;
        ASSERT(mut >= 0 && mut < NUM_MUTATIONS);
        ASSERT(mut_index[mut] == -1);
        mut_index[mut] = i;
    }
}

static mutation_def* _seek_mutation(mutation_type mut)
{
    ASSERT(mut >= 0 && mut < NUM_MUTATIONS);
    if (mut_index[mut] == -1)
        return NULL;
    else
        return (&mut_data[mut_index[mut]]);
}

bool is_valid_mutation(mutation_type mut)
{
    return (mut >= 0 && mut < NUM_MUTATIONS
            && _seek_mutation(mut));
}

static const mutation_type _all_scales[] = {
    MUT_DISTORTION_FIELD,           MUT_ICY_BLUE_SCALES,
    MUT_IRIDESCENT_SCALES,          MUT_LARGE_BONE_PLATES,
    MUT_MOLTEN_SCALES,              MUT_ROUGH_BLACK_SCALES,
    MUT_RUGGED_BROWN_SCALES,        MUT_SLIMY_GREEN_SCALES,
    MUT_THIN_METALLIC_SCALES,       MUT_THIN_SKELETAL_STRUCTURE,
    MUT_YELLOW_SCALES,
};

static bool _is_covering(mutation_type mut)
{
    for (unsigned i = 0; i < ARRAYSZ(_all_scales); ++i)
        if (_all_scales[i] == mut)
            return true;

    return false;
}

bool is_body_facet(mutation_type mut)
{
    for (unsigned i = 0; i < ARRAYSZ(_body_facets); i++)
    {
        if (_body_facets[i].mut == mut)
            return true;
    }

    return false;
}

const mutation_def& get_mutation_def(mutation_type mut)
{
    ASSERT(is_valid_mutation(mut));
    return (*_seek_mutation(mut));
}

void fixup_mutations()
{
    if (player_genus(GENPC_DRACONIAN))
    {
        ASSERT(is_valid_mutation(MUT_STINGER));
        _seek_mutation(MUT_STINGER)->rarity = 8;
        ASSERT(is_valid_mutation(MUT_BIG_WINGS));
        _seek_mutation(MUT_BIG_WINGS)->rarity = 4;
    }

    if (you.species == SP_NAGA)
    {
        ASSERT(is_valid_mutation(MUT_STINGER));
        _seek_mutation(MUT_STINGER)->rarity = 8;
    }

    if (you.species == SP_OCTOPODE)
    {
        ASSERT(is_valid_mutation(MUT_TENTACLE_SPIKE));
        _seek_mutation(MUT_TENTACLE_SPIKE)->rarity = 10;
    }
}

mutation_activity_type mutation_activity_level(mutation_type mut)
{
    // First make sure the player's form permits the mutation.
    if (mut == MUT_BREATHE_POISON)
    {
        if (form_changed_physiology() && you.form != TRAN_SPIDER)
            return MUTACT_INACTIVE;
    }
    else if (!form_keeps_mutations())
    {
        if (you.form == TRAN_DRAGON)
        {
            monster_type drag = dragon_form_dragon_type();
            if (mut == MUT_SHOCK_RESISTANCE && drag == MONS_STORM_DRAGON)
                return MUTACT_FULL;
            if (mut == MUT_UNBREATHING && drag == MONS_IRON_DRAGON)
                return MUTACT_FULL;
        }
        // Dex and HP changes are kept in all forms.
        if (mut == MUT_ROUGH_BLACK_SCALES || mut == MUT_RUGGED_BROWN_SCALES)
            return MUTACT_PARTIAL;
        else if (get_mutation_def(mut).form_based)
            return MUTACT_INACTIVE;
    }

    if (you.form == TRAN_STATUE)
    {
        // Statues get all but the AC benefit from scales, but are not affected
        // by other changes in body material or speed.  We assume here that
        // scale mutations are physical and therefore do not need the vampire
        // checks below.
        switch (mut)
        {
        case MUT_GELATINOUS_BODY:
        case MUT_TOUGH_SKIN:
        case MUT_SHAGGY_FUR:
        case MUT_FAST:
        case MUT_SLOW:
        case MUT_IRIDESCENT_SCALES:
            return MUTACT_INACTIVE;
        case MUT_LARGE_BONE_PLATES:
        case MUT_ROUGH_BLACK_SCALES:
        case MUT_RUGGED_BROWN_SCALES:
            return MUTACT_PARTIAL;
        case MUT_YELLOW_SCALES:
        case MUT_ICY_BLUE_SCALES:
        case MUT_MOLTEN_SCALES:
        case MUT_SLIMY_GREEN_SCALES:
        case MUT_THIN_METALLIC_SCALES:
            return (you.mutation[mut] > 2 ? MUTACT_PARTIAL : MUTACT_INACTIVE);
        default:
            break;
        }
    }

    // Vampires may find their mutations suppressed by thirst.
    if (you.is_undead == US_SEMI_UNDEAD)
    {
        // Innate mutations are always active
        if (you.innate_mutations[mut])
            return MUTACT_FULL;

        // ... as are all mutations for semi-undead who are fully alive
        if (you.hunger_state == HS_ENGORGED)
            return MUTACT_FULL;

        // ... as are physical mutations.
        if (get_mutation_def(mut).physical)
            return MUTACT_FULL;

        // Other mutations are partially active at satiated and above.
        if (you.hunger_state >= HS_SATIATED)
            return MUTACT_HUNGER;
        else
            return MUTACT_INACTIVE;
    }
    else
        return MUTACT_FULL;
}

// Counts of various statuses/types of mutations from the current/most
// recent call to describe_mutations.  TODO: eliminate
static int _num_full_suppressed = 0;
static int _num_part_suppressed = 0;
static int _num_form_based = 0;
static int _num_hunger_based = 0;

// Can the player transform?  Returns true if the player is ever capable
// of transforming (i.e. not a mummy or ghoul) and either: is transformed
// (ignoring blade hands and appendage), is a vampire of sufficient level
// to use bat form, or has a form-change spell (again, other than blade hands
// and beastly appendage) memorised.
static bool _player_can_transform()
{
    if (you.species == SP_MUMMY || you.species == SP_GHOUL)
        return false;

    if (form_changed_physiology())
        return true;

    // Bat form
    if (you.species == SP_VAMPIRE && you.experience_level >= 3)
        return true;

    for (int i = 0; i < MAX_KNOWN_SPELLS; i++)
    {
        switch (you.spells[i])
        {
        case SPELL_SPIDER_FORM:
        case SPELL_ICE_FORM:
        case SPELL_STATUE_FORM:
        case SPELL_DRAGON_FORM:
        case SPELL_NECROMUTATION:
            return true;
        default:
            break;
        }
    }

    return false;
}

static std::string _annotate_form_based(std::string desc, bool suppressed)
{
    if (suppressed)
    {
        desc = "<darkgrey>((" + desc + "))</darkgrey>";
        ++_num_full_suppressed;
    }

    if (_player_can_transform())
    {
        ++_num_form_based;
        desc += "<yellow>*</yellow>";
    }

    return desc + "\n";
}

static std::string _dragon_abil(std::string desc)
{
    const bool supp = form_changed_physiology() && you.form != TRAN_DRAGON;
    return _annotate_form_based(desc, supp);
}

std::string describe_mutations(bool center_title)
{
    std::string result;
    bool have_any = false;
    const char *mut_title = "선천적, 불가사의한 & 돌연변이 능력";
    std::string scale_type = "plain brown";

    _num_full_suppressed = _num_part_suppressed = 0;
    _num_form_based = _num_hunger_based = 0;

    if (center_title)
    {
        int offset = 39 - strwidth(mut_title) / 2;
        if (offset < 0) offset = 0;

        result += std::string(offset, ' ');
    }

    result += "<white>";
    result += mut_title;
    result += "</white>\n\n";

    // Innate abilities which don't fit as mutations.
    // TODO: clean these up with respect to transformations.  Currently
    // we handle only Naga/Draconian AC and Yellow Draconian rAcid.
    result += "<lightblue>";
    switch (you.species)
    {
    case SP_MERFOLK:
        result += _annotate_form_based(
            gettext("You revert to your normal form in water."),
            form_changed_physiology());
        result += _annotate_form_based(
            gettext("You are very nimble and swift while swimming."),
            form_changed_physiology());
        have_any = true;
        break;

    case SP_MINOTAUR:
        result += _annotate_form_based(
            gettext("You reflexively headbutt those who attack you in melee."),
            !form_keeps_mutations());
        have_any = true;
        break;

    case SP_NAGA:
        result += gettext("You cannot wear boots.\n");

        // Breathe poison replaces spit poison.
        if (!player_mutation_level(MUT_BREATHE_POISON))
            result += _("You can spit poison.\n");

        if (you.experience_level > 12)
        {
            result += _annotate_form_based(
                _("You can use your snake-like lower body to constrict enemies."),
                !form_keeps_mutations());
        }

        if (you.experience_level > 2)
        {
            std::ostringstream num;
            num << you.experience_level/3;
            const std::string acstr = gettext("Your serpentine skin is tough (AC +")
                                      + num.str() + ").";

            result += _annotate_form_based(acstr, player_is_shapechanged());
        }
        have_any = true;
        break;

    case SP_GHOUL:

//      result += "Your body is rotting away.\n";
        result += "당신의 몸은 썩어 문드러진다.\n";
        have_any = true;
        break;

    case SP_TENGU:
        if (you.experience_level > 4)
        {
            std::string msg = gettext("You can fly.");
            if (you.experience_level > 14)
                msg = gettext("You can fly continuously.");
            msg += ".\n";

            result += msg;
            have_any = true;
        }
        break;

    case SP_MUMMY:

		//머미의 변이
        //result += "You do not eat or drink.\n";
        //result += "Your flesh is vulnerable to fire.\n";
        result += "당신은 먹거나 마실 수 없다.\n";
        result += "당신의 육체는 불에 취약하다.\n";
        //12레벨이 넘어가면
		
		if (you.experience_level > 12)
        {

			//result += "You are";
            result += "당신은";
            if (you.experience_level > 25)
				//result += " strongly";
				result += " 강한";
//result += " in touch with the powers of death.\n";
            result += " 죽음의 힘으로 부여받은 손길이 있다.\n";
//	result +=
//      "You can restore your body by infusing magical energy.\n";
			result +=
                "당신은 마법 에너지를 불어넣어 육체를 회복할 수 있다.\n";
        }
        have_any = true;
        break;

		//각종 색색 드라코들
    case SP_GREEN_DRACONIAN:
        result += _dragon_abil(gettext("You can breathe blasts of noxious fumes."));
        have_any = true;

//끔찍한 녹색?          scale_type = "lurid green";
        scale_type = "끔찍한 녹색";
        break;

    case SP_GREY_DRACONIAN:
		//        result += "You can walk through water.\n";
        result += "당신은 물 속을 걸을 수 있다.\n";
        have_any = true;
        /// GREY_DRACONIAN 종족의 색깔.
        scale_type = _(M_("dull iron-grey"));
        break;

    case SP_RED_DRACONIAN:
        result += _dragon_abil(gettext("You can breathe blasts of fire."));
        have_any = true;
		//scale_type = "fiery red";
        scale_type = "불타는 듯한 빨강";
        break;

    case SP_WHITE_DRACONIAN:
        result += _dragon_abil(gettext("You can breathe waves of freezing cold."));
        result += _dragon_abil(gettext("You can buffet flying creatures when you breathe cold."));
        scale_type = gettext(M_("icy white"));
        have_any = true;
        break;

    case SP_BLACK_DRACONIAN:
        result += _dragon_abil(gettext("You can breathe wild blasts of lightning."));
        scale_type = gettext(M_("glossy black"));
        have_any = true;
        break;

    case SP_YELLOW_DRACONIAN:
        result += _dragon_abil(gettext("You can spit globs of acid."));
        result += _dragon_abil(gettext("You can corrode armour when you spit acid."));
        result += _annotate_form_based(gettext("You are resistant to acid."),
                      !form_keeps_mutations() && you.form != TRAN_DRAGON);
        scale_type = gettext(M_("golden yellow"));
        have_any = true;
        break;

    case SP_PURPLE_DRACONIAN:
        result += _dragon_abil(gettext("You can breathe bolts of energy."));
        result += _dragon_abil(gettext("You can dispel enchantments when you breathe energy."));
        scale_type = gettext("rich purple");
        have_any = true;
        break;

    case SP_MOTTLED_DRACONIAN:
        result += _dragon_abil(gettext("You can spit globs of burning liquid."));
        result += _dragon_abil(gettext("You can ignite nearby creatures when you spit burning liquid."));
        scale_type = gettext("weird mottled");
        have_any = true;
        break;

    case SP_PALE_DRACONIAN:
        result += _dragon_abil(gettext("You can breathe blasts of scalding steam."));
        scale_type = gettext("pale cyan-grey");
        have_any = true;
        break;

    case SP_KOBOLD:
		//코볼트 관련
        //result += "You recuperate from illness quickly.\n";
		result += "당신은 질병으로부터 빠르게 회복된다. .\n";
        have_any = true;
        break;

    case SP_VAMPIRE:
		//뱀파이어 관련 
        have_any = true;
//순서대로 저장
            //result += "<green>You do not heal naturally.</green>\n";
            //result += "<green>Your natural rate of healing is extremely fast.</green>\n";
            //result += "<green>You heal slowly.</green>\n";
            //result += "<green>Your natural rate of healing is unusually fast.</green>\n";

        if (you.hunger_state == HS_STARVING)
            result += "<green>당신은 자연적으로 회복되지 않는다.</green>\n";
        else if (you.hunger_state == HS_ENGORGED)
            result += "<green>당신의 치유 속도는 매우 빠르다.</green>\n";
        else if (you.hunger_state < HS_SATIATED)
            result += "<green>당신은 느리게 회복된다.</green>\n";
        else if (you.hunger_state >= HS_FULL)
            result += "<green>당신의 치유 속도는 비정상적으로 빠르다.</green>\n";
        else
            have_any = false;

        if (you.experience_level >= 6)
        {
			//   렙업하면 피를 병에 담을수 있다져         result += "You can bottle blood from corpses.\n";
            result += "당신은 시체로부터 피가 담긴 병을 만들 수 있다.\n";
            have_any = true;
        }
        break;


		//이젠 딥뒆
    case SP_DEEP_DWARF:

        //result += "You are resistant to damage.\n";
        //result += "You can recharge devices by infusing magical energy.\n";

        result += "당신은 모든 피해에 대한 저항력을 가지고 있다.\n";
        result += "당신은 마법 에너지를 불어넣어 장치를 충전할 수 있다.\n";
        have_any = true;
        break;

		//고양이
    case SP_FELID:
        result += _("You cannot wear armour.\n");
        result += _("You are incapable of any advanced item manipulation.\n");
        result += _annotate_form_based(_("Your paws have sharp claws."),
            !form_keeps_mutations() || you.form == TRAN_BLADE_HANDS);
        have_any = true;
        break;

		//무너무너
    case SP_OCTOPODE:
        result += _("You cannot wear most types of armour.\n");
        result += _("You are amphibious.\n");
        result += _annotate_form_based(
            _("You can wear up to eight rings at the same time."),
            !form_keeps_mutations() && you.form != TRAN_SPIDER);
        result += _annotate_form_based(
            _("You can use your tentacles to constrict many enemies at once."),
            !form_keeps_mutations());
        have_any = true;
        break;

    default:
        break;
    }

	//이건 뭐지..
    switch (you.body_size(PSIZE_TORSO, true))
    {
    case SIZE_LITTLE:
        if (you.species == SP_FELID)
            break;

		//보니까 고양이 아머 못낄때 뜨는 메시지 같네요
		//result += "You are tiny and cannot use many weapons and most armour.\n";
        result += "당신은 아주 작기에 많은 무기와 대부분의 갑옷을 사용할 수 없다.\n";
        have_any = true;
        break;
    case SIZE_SMALL:
		//이건 그냥 체형이 작은 종족들인듯.
		//result += "You are small and have problems with some larger weapons.\n";
        result += "당신은 작아서 몇몇 거대한 무기들을 사용하는 데에 문제가 있다..\n";
        have_any = true;
        break;
    case SIZE_LARGE:
		//      result += "You are too large for most types of armour.\n";
        result += "당신은 대부분의 갑옷을 입기에는 너무 크다.\n";
        have_any = true;
        break;
    default:
        break;
    }
//뭐지이건.. 드라코들의 방어 관련인가보군요
    if (player_genus(GENPC_DRACONIAN))
    {
        // Draconians are large for the purposes of armour, but only medium for
        // weapons and carrying capacity.
        std::ostringstream num;
        num << 4 + you.experience_level / 3
                 + (you.species == SP_GREY_DRACONIAN ? 5 : 0);

        const std::string msg = make_stringf(_("Your %s scales are %shard (AC +%s)."),
            scale_type.c_str(),
            (you.species == SP_GREY_DRACONIAN ? _(M_("very ")) : ""),
            num.str().c_str());

        result += _annotate_form_based(msg,
                      player_is_shapechanged() && you.form != TRAN_DRAGON);

        result += gettext("Your body does not fit into most forms of armour.\n");
        have_any = true;
    }

    result += "</lightblue>";

    if (beogh_water_walk())
    {
		//        result += "<green>You can walk on water.</green>\n";

        result += "<green>당신은 물에서 걸을 수 있다.</green>\n";
        have_any = true;
    }

    if (you.duration[DUR_FIRE_SHIELD])
    {
		//        result += "<green>You are immune to clouds of flame.</green>\n";
        result += "<green>당신은 화염의 구름에 면역인 상태이다.</green>\n";
        have_any = true;
    }

    textcolor(LIGHTGREY);

    // First add (non-removable) inborn abilities and demon powers.
    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        if (you.mutation[i] != 0 && you.innate_mutations[i])
        {
            mutation_type mut_type = static_cast<mutation_type>(i);
            result += mutation_name(mut_type, -1, true);
            result += "\n";
            have_any = true;
        }
    }

    // Now add removable mutations.
    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        if (you.mutation[i] != 0 && !you.innate_mutations[i])
        {
            mutation_type mut_type = static_cast<mutation_type>(i);
            result += mutation_name(mut_type, -1, true);
            result += "\n";
            have_any = true;
        }
    }

    if (!have_any)
        result += gettext("You are rather mundane.\n");

    return result;
}

static const std::string _vampire_Ascreen_footer = gettext(
    "Press '<w>!</w>'"
#ifdef USE_TILE_LOCAL
    " or <w>Right-click</w>"
#endif
    " to toggle between mutations and properties depending on your\n"
    "hunger status.\n");

static void _display_vampire_attributes()
{
    ASSERT(you.species == SP_VAMPIRE);

    std::string result;

	//화면에 표시되는 글자인듯 한데 일단은 다 변경시켜두어 보겠습니다.
    const int lines = 15;
    std::string column[lines][7] =
    {
       {"                     ", "<lightgreen>살아있는</lightgreen>      ", "<green>가득찬</green>    ",
        "만족한  ", "<yellow>목마른</yellow>  ", "<yellow>아슬아슬한...</yellow>  ",
        "<lightred>Bloodless</lightred>"},
                                //Alive          Full       Satiated      Thirsty   Near...      Bloodless
       {"신진대사             ", "매우 빠른  ", "빠른    ", "빠른      ", "보통     ", "느림     ", "없음  "},

       {"자연 치유력          ", "매우 빠른  ", "빠른    ", "보통      ", "느림     ", "느림     ", "없음  "},

       {"추가 은신 보너스     ", "없음       ", "없음    ", "없음      ", "약간     ", "큰       ", "엄청난"},

       {"마법에 쓰이는 허기   ", "전부       ", "전부    ", "전부      ", "반절     ", "없음     ", "없음  "},

       {"\n<w>저항</w>\n"
        "독저항               ", "           ", "        ", "          ", " +       ", " +       ", " +    "},

       {"냉기 저항            ", "           ", "        ", "          ", " +       ", " ++      ", " ++   "},

       {"음에너지 저항        ", "           ", "        ", " +        ", " ++      ", " +++     ", " +++  "},

       {"부패 저항            ", "           ", "        ", "          ", " +       ", " +       ", " +    "},

       {"고통 저항            ", "           ", "        ", "          ", "         ", "         ", " +    "},

       {"\n<w>다른 효과들</w>\n"
        "변이 기회            ", "항상       ", "자주    ", "가끔      ", "절대없음 ", "절대없음 ", "절대없음"},

       {"물리적이지 않은 \n"
        "변이 효과들          ", "전부       ", "한정적인", "한정적인  ", "절대없음 ", "절대없음 ", "절대없음"},

       {"물약 효과            ", "그대로     ", "그대로  ", "그대로    ", "반절     ", "반절     ", "반절  "},

       {"박쥐 변신            ", "불가능     ", "불가능  ", "가능      ", "가능     ", "가능     ", "가능  "},

       {"다른 변신  \n"
        "이나 광폭화 상태     ", "가능       ", "가능    ", "불가능    ", "불가능   ", "불가능   ", "불가능"}
    };

    int current = 0;
    switch (you.hunger_state)
    {
    case HS_ENGORGED:
        current = 1;
        break;
    case HS_VERY_FULL:
    case HS_FULL:
        current = 2;
        break;
    case HS_SATIATED:
        current = 3;
        break;
    case HS_HUNGRY:
    case HS_VERY_HUNGRY:
        current = 4;
        break;
    case HS_NEAR_STARVING:
        current = 5;
        break;
    case HS_STARVING:
        current = 6;
    }

    for (int y = 0; y < lines; y++)  // lines   (properties)
    {
        for (int x = 0; x < 7; x++)  // columns (hunger states)
        {
             if (y > 0 && x == current)
                 result += "<w>";
             result += column[y][x];
             if (y > 0 && x == current)
                 result += "</w>";
        }
        result += "\n";
    }

    result += "\n";
    result += _vampire_Ascreen_footer;

    formatted_scroller attrib_menu;
    attrib_menu.add_text(result);

    attrib_menu.show();
    if (attrib_menu.getkey() == '!'
        || attrib_menu.getkey() == CK_MOUSE_CMD)
    {
        display_mutations();
    }
}

void display_mutations()
{
    std::string mutation_s = describe_mutations(true);

    std::string extra = "";
    if (_num_part_suppressed)
        extra += "<brown>()</brown>  : Partially suppressed.\n";
    if (_num_full_suppressed)
        extra += "<darkgrey>(())</darkgrey>: Completely suppressed.\n";
    if (_num_form_based) // TODO: check for form spells?
        extra += "<yellow>*</yellow>   : Suppressed by some changes of form.\n";
    if (_num_hunger_based)
        extra += "<lightred>+</lightred>   : Suppressed by thirst.\n";
    if (you.species == SP_VAMPIRE)
    {
        if (!extra.empty())
            extra += "\n";

        extra += _vampire_Ascreen_footer;
    }

    if (!extra.empty())
    {
        mutation_s += "\n\n\n\n";
        mutation_s += extra;
    }

    formatted_scroller mutation_menu;
    mutation_menu.add_text(mutation_s);

    mouse_control mc(MOUSE_MODE_MORE);

    mutation_menu.show();

    if (you.species == SP_VAMPIRE
        && (mutation_menu.getkey() == '!'
            || mutation_menu.getkey() == CK_MOUSE_CMD))
    {
        _display_vampire_attributes();
    }
}

static int _calc_mutation_amusement_value(mutation_type which_mutation)
{
    int amusement = 12 * (11 - get_mutation_def(which_mutation).rarity);

    switch (which_mutation)
    {
    case MUT_STRONG:
    case MUT_CLEVER:
    case MUT_AGILE:
    case MUT_POISON_RESISTANCE:
    case MUT_SHOCK_RESISTANCE:
    case MUT_REGENERATION:
    case MUT_SLOW_METABOLISM:
    case MUT_TELEPORT_CONTROL:
    case MUT_MAGIC_RESISTANCE:
    case MUT_CLARITY:
    case MUT_MUTATION_RESISTANCE:
    case MUT_ROBUST:
    case MUT_HIGH_MAGIC:
        amusement /= 2;  // not funny
        break;

    case MUT_CARNIVOROUS:
    case MUT_HERBIVOROUS:
    case MUT_SLOW_HEALING:
    case MUT_FAST_METABOLISM:
    case MUT_WEAK:
    case MUT_DOPEY:
    case MUT_CLUMSY:
    case MUT_TELEPORT:
    case MUT_FAST:
    case MUT_DEFORMED:
    case MUT_SPIT_POISON:
    case MUT_BREATHE_FLAMES:
    case MUT_BLINK:
    case MUT_HORNS:
    case MUT_BEAK:
    case MUT_SCREAM:
    case MUT_BERSERK:
    case MUT_DETERIORATION:
    case MUT_BLURRY_VISION:
    case MUT_FRAIL:
    case MUT_CLAWS:
    case MUT_FANGS:
    case MUT_HOOVES:
    case MUT_TALONS:
    case MUT_TENTACLE_SPIKE:
    case MUT_BREATHE_POISON:
    case MUT_STINGER:
    case MUT_BIG_WINGS:
    case MUT_LOW_MAGIC:
    case MUT_SLOW:
    case MUT_EVOLUTION:
        amusement *= 2; // funny!
        break;

    default:
        break;
    }

    return amusement;
}

static bool _accept_mutation(mutation_type mutat, bool ignore_rarity = false)
{
    if (!is_valid_mutation(mutat))
        return false;

    const mutation_def& mdef = get_mutation_def(mutat);

    if (you.mutation[mutat] >= mdef.levels)
        return false;

    if (ignore_rarity)
        return true;

    const int rarity = mdef.rarity + you.innate_mutations[mutat];

    // Low rarity means unlikely to choose it.
    return x_chance_in_y(rarity, 10);
}

static mutation_type _get_random_slime_mutation()
{
    const mutation_type slime_muts[] = {
        MUT_GELATINOUS_BODY, MUT_EYEBALLS, MUT_TRANSLUCENT_SKIN,
        MUT_PSEUDOPODS, MUT_FOOD_JELLY, MUT_ACIDIC_BITE
    };

    return RANDOM_ELEMENT(slime_muts);
}

static mutation_type _delete_random_slime_mutation()
{
    mutation_type mutat;

    while (true)
    {
        mutat = _get_random_slime_mutation();

        if (you.mutation[mutat] > 0)
            break;

        if (one_chance_in(500))
        {
            mutat = NUM_MUTATIONS;
            break;
        }
    }

    return mutat;
}

static bool _is_slime_mutation(mutation_type m)
{
    return (m == MUT_GELATINOUS_BODY || m == MUT_EYEBALLS
            || m == MUT_TRANSLUCENT_SKIN || m == MUT_PSEUDOPODS
            || m == MUT_FOOD_JELLY || m == MUT_ACIDIC_BITE);
}

static mutation_type _get_random_xom_mutation()
{
    const mutation_type bad_muts[] = {
        MUT_WEAK,          MUT_DOPEY,
        MUT_CLUMSY,        MUT_DEFORMED,      MUT_SCREAM,
        MUT_DETERIORATION, MUT_BLURRY_VISION, MUT_FRAIL
    };

    mutation_type mutat = NUM_MUTATIONS;

    do
    {
        mutat = static_cast<mutation_type>(random2(NUM_MUTATIONS));

        if (one_chance_in(1000))
            return NUM_MUTATIONS;
        else if (one_chance_in(5))
            mutat = RANDOM_ELEMENT(bad_muts);
    }
    while (!_accept_mutation(mutat, false));

    return mutat;
}

static bool _mut_matches_class(mutation_type mutclass, const mutation_def& mdef)
{
    switch (mutclass)
    {
    case RANDOM_MUTATION:
        return true;
    case RANDOM_BAD_MUTATION:
        return mdef.bad;
    case RANDOM_GOOD_MUTATION:
        return (!mdef.bad);
    default:
		// die("invalid mutation class: %d", mutclass);
        die("인식 불가능한 변이 등급: %d", mutclass);
    }
}

static mutation_type _get_random_mutation(mutation_type mutclass)
{
    int cweight = 0;
    mutation_type chosen = NUM_MUTATIONS;
    for (unsigned i = 0; i < NUM_MUTATIONS; ++i)
    {
        const mutation_type curr = static_cast<mutation_type>(i);
        if (!is_valid_mutation(curr))
            continue;

        const mutation_def& mdef = get_mutation_def(curr);
        if (!mdef.rarity)
            continue;

        if (!_mut_matches_class(mutclass, mdef))
            continue;

        if (!_accept_mutation(curr, true))
            continue;

        cweight += mdef.rarity;

        if (x_chance_in_y(mdef.rarity, cweight))
            chosen = curr;
    }

    return chosen;
}

// Tries to give you the mutation by deleting a conflicting
// one, or clears out conflicting mutations if we should give
// you the mutation anyway.
// Return:
//  1 if we should stop processing (success);
//  0 if we should continue processing;
// -1 if we should stop processing (failure).
static int _handle_conflicting_mutations(mutation_type mutation,
                                         bool override,
                                         const std::string &reason)
{
    const int conflict[][3] = {
        { MUT_REGENERATION,     MUT_SLOW_METABOLISM,  0},
        { MUT_REGENERATION,     MUT_SLOW_HEALING,     0},
        { MUT_ACUTE_VISION,     MUT_BLURRY_VISION,    0},
        { MUT_FAST,             MUT_SLOW,             0},
        { MUT_FANGS,            MUT_BEAK,            -1},
        { MUT_ANTENNAE,         MUT_HORNS,           -1},
        { MUT_HOOVES,           MUT_TALONS,          -1},
        { MUT_TRANSLUCENT_SKIN, MUT_CAMOUFLAGE,      -1},
        { MUT_STRONG,           MUT_WEAK,             1},
        { MUT_CLEVER,           MUT_DOPEY,            1},
        { MUT_AGILE,            MUT_CLUMSY,           1},
        { MUT_STRONG_STIFF,     MUT_FLEXIBLE_WEAK,    1},
        { MUT_ROBUST,           MUT_FRAIL,            1},
        { MUT_HIGH_MAGIC,       MUT_LOW_MAGIC,        1},
        { MUT_CARNIVOROUS,      MUT_HERBIVOROUS,      1},
        { MUT_SLOW_METABOLISM,  MUT_FAST_METABOLISM,  1},
        { MUT_REGENERATION,     MUT_SLOW_HEALING,     1},
        { MUT_ACUTE_VISION,     MUT_BLURRY_VISION,    1},
        { MUT_FAST,             MUT_SLOW,             1},
        { MUT_MUTATION_RESISTANCE, MUT_EVOLUTION,    -1},
        };

    // If we have one of the pair, delete all levels of the other,
    // and continue processing.
    for (unsigned i = 0; i < ARRAYSZ(conflict); ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            const mutation_type a = (mutation_type)conflict[i][j];
            const mutation_type b = (mutation_type)conflict[i][1-j];

            if (mutation == a && you.mutation[b] > 0)
            {
                if (you.innate_mutations[b] >= you.mutation[b])
                    return -1;

                int res = conflict[i][2];
                switch (res)
                {
                case -1:
                    // Fail if not forced, otherwise override.
                    if (!override)
                        return -1;
                case 0:
                    // Ignore if not forced, otherwise override.
                    // All cases but regen:slowmeta will currently trade off.
                    if (override)
                    {
                        while (delete_mutation(b, reason, true, true))
                            ;
                    }
                    break;
                case 1:
                    // If we have one of the pair, delete a level of the
                    // other, and that's it.
                    delete_mutation(b, reason, true, true);
                    return 1;     // Nothing more to do.
                default:
			//	die("bad mutation conflict resulution");
					
					die("나쁜 변이가 해결되었다");
                }
            }
        }
    }

    return 0;
}

static int _body_covered()
{
    // Check how much of your body is covered by scales, etc.
    int covered = 0;

    if (you.species == SP_NAGA)
        covered++;

    if (player_genus(GENPC_DRACONIAN))
        covered += 3;

    for (unsigned i = 0; i < ARRAYSZ(_all_scales); ++i)
        covered += you.mutation[_all_scales[i]];

    return covered;
}

bool physiology_mutation_conflict(mutation_type mutat)
{
    // If demonspawn, and mutat is a scale, see if they were going
    // to get it sometime in the future anyway; otherwise, conflict.
    if (you.species == SP_DEMONSPAWN && _is_covering(mutat)
        && std::find(_all_scales, _all_scales+ARRAYSZ(_all_scales), mutat) !=
                _all_scales+ARRAYSZ(_all_scales))
    {
        bool found = false;

        for (unsigned j = 0; j < you.demonic_traits.size(); ++j)
        {
            if (you.demonic_traits[j].mutation == mutat)
                found = true;
        }

        return (!found);
    }

    // Strict 3-scale limit.
    if (_is_covering(mutat) && _body_covered() >= 3)
        return true;

    // Only Nagas and Draconians can get this one.
    if (you.species != SP_NAGA && !player_genus(GENPC_DRACONIAN)
        && mutat == MUT_STINGER)
    {
        return true;
    }

    // Need tentacles to grow something on them.
    if (you.species != SP_OCTOPODE && mutat == MUT_TENTACLE_SPIKE)
        return true;

    // No bones.
    if (you.species == SP_OCTOPODE && mutat == MUT_THIN_SKELETAL_STRUCTURE)
        return true;

    // No feet.
    if (!player_has_feet(false)
        && (mutat == MUT_HOOVES || mutat == MUT_TALONS))
    {
        return true;
    }

    // Only Nagas can get this upgrade.
    if (you.species != SP_NAGA && mutat == MUT_BREATHE_POISON)
        return true;

    // Red Draconians can already breathe flames.
    if (you.species == SP_RED_DRACONIAN && mutat == MUT_BREATHE_FLAMES)
        return true;

    // Green Draconians can breathe mephitic, poison is not really redundant
    // but its name might confuse players a bit ("noxious" vs "poison").
    if (you.species == SP_GREEN_DRACONIAN && mutat == MUT_SPIT_POISON)
        return true;

    // Only Draconians can get wings.
    if (!player_genus(GENPC_DRACONIAN) && mutat == MUT_BIG_WINGS)
        return true;

    // Vampires' healing and thirst rates depend on their blood level.
    if (you.species == SP_VAMPIRE
        && (mutat == MUT_CARNIVOROUS || mutat == MUT_HERBIVOROUS
            || mutat == MUT_REGENERATION || mutat == MUT_SLOW_HEALING
            || mutat == MUT_FAST_METABOLISM || mutat == MUT_SLOW_METABOLISM))
    {
        return true;
    }

    // Felids have innate claws, and unlike trolls/ghouls, there are no
    // increases for them.
    if (you.species == SP_FELID && mutat == MUT_CLAWS)
        return true;

    // Merfolk have no feet in the natural form, and we never allow mutations
    // that show up only in a certain transformation.
    if (you.species == SP_MERFOLK
        && (mutat == MUT_TALONS || mutat == MUT_HOOVES))
    {
        return true;
    }

    equipment_type eq_type = EQ_NONE;

    // Mutations of the same slot conflict
    if (is_body_facet(mutat))
    {
        // Find equipment slot of attempted mutation
        for (unsigned i = 0; i < ARRAYSZ(_body_facets); i++)
            if (mutat == _body_facets[i].mut)
                eq_type = _body_facets[i].eq;

        if (eq_type != EQ_NONE)
        {
            for (unsigned i = 0; i < ARRAYSZ(_body_facets); i++)
            {
                if (eq_type == _body_facets[i].eq
                    && mutat != _body_facets[i].mut
                    && player_mutation_level(_body_facets[i].mut, false))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

static const char* _stat_mut_desc(mutation_type mut, bool gain)
{
    stat_type stat = STAT_STR;
    bool positive = gain;
    switch (mut)
    {
    case MUT_WEAK:
        positive = !positive;
    case MUT_STRONG:
        stat = STAT_STR;
        break;

    case MUT_DOPEY:
        positive = !positive;
    case MUT_CLEVER:
        stat = STAT_INT;
        break;

    case MUT_CLUMSY:
        positive = !positive;
    case MUT_AGILE:
        stat = STAT_DEX;
        break;

    default:
        die("invalid stat mutation: %d", mut);
    }
    return stat_desc(stat, positive ? SD_INCREASE : SD_DECREASE);
}

bool mutate(mutation_type which_mutation, const std::string &reason,
            bool failMsg,
            bool force_mutation, bool god_gift, bool stat_gain_potion,
            bool demonspawn, bool no_rot)
{
    if (!god_gift)
    {
        const god_type god =
            (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                          : GOD_NO_GOD;

        if (god != GOD_NO_GOD)
            god_gift = true;
    }

    if (demonspawn)
        force_mutation = true;

    mutation_type mutat = which_mutation;

    if (!force_mutation)
    {
        // God gifts override all sources of mutation resistance other
        // than divine protection, and stat gain potions override all
        // sources of mutation resistance other than the mutation
        // resistance mutation.
        if (!god_gift)
        {
            if ((player_res_mutation_from_item()
                 && !one_chance_in(10) && !stat_gain_potion)
                || player_mutation_level(MUT_MUTATION_RESISTANCE) == 3
                || (player_mutation_level(MUT_MUTATION_RESISTANCE)
                    && !one_chance_in(3)))
            {
                if (failMsg)
                {
                    mpr(_("You feel odd for a moment."), MSGCH_MUTATION);
                    maybe_id_resist(BEAM_POLYMORPH);
                }
                return false;
            }
        }

        // Zin's protection.
        if (you.religion == GOD_ZIN
            && (x_chance_in_y(you.piety, MAX_PIETY)
                || x_chance_in_y(you.piety, MAX_PIETY + 22))
            && !stat_gain_potion)
        {
            simple_god_message(" protects your body from mutation!");
            return false;
        }
    }

    bool rotting = you.is_undead;

    if (you.is_undead == US_SEMI_UNDEAD)
    {
        // The stat gain mutations always come through at Satiated or
        // higher (mostly for convenience), and, for consistency, also
        // their negative counterparts.
        if (which_mutation == MUT_STRONG || which_mutation == MUT_CLEVER
            || which_mutation == MUT_AGILE || which_mutation == MUT_WEAK
            || which_mutation == MUT_DOPEY || which_mutation == MUT_CLUMSY)
        {
            if (you.hunger_state >= HS_SATIATED)
                rotting = false;
        }
        else
        {
            // Else, chances depend on hunger state.
            switch (you.hunger_state)
            {
            case HS_SATIATED:  rotting = !one_chance_in(3); break;
            case HS_FULL:      rotting = coinflip();        break;
            case HS_VERY_FULL: rotting = one_chance_in(3);  break;
            case HS_ENGORGED:  rotting = false;             break;
            default: ;
            }
        }
    }

    // Undead bodies don't mutate, they fall apart. -- bwr
    // except for demonspawn (or other permamutations) in lichform -- haranp
    if (rotting && !demonspawn)
    {
        if (no_rot)
            return false;

        mpr(gettext("Your body decomposes!"), MSGCH_MUTATION);

        if (coinflip())
            lose_stat(STAT_RANDOM, 1, false, reason);
        else
        {
            ouch(3, NON_MONSTER, KILLED_BY_ROTTING, reason.c_str());
            rot_hp(roll_dice(1, 3));
        }

        xom_is_stimulated(50);
        return true;
    }

    if (which_mutation == RANDOM_MUTATION
        || which_mutation == RANDOM_XOM_MUTATION)
    {
        // If already heavily mutated, remove a mutation instead.
        if (x_chance_in_y(how_mutated(false, true), 15))
        {
            // God gifts override mutation loss due to being heavily
            // mutated.
            if (!one_chance_in(3) && !god_gift && !force_mutation)
                return false;
            else
                return (delete_mutation(RANDOM_MUTATION, reason, failMsg,
                                        force_mutation, false));
        }
    }

    switch (which_mutation)
    {
    case RANDOM_MUTATION:
    case RANDOM_GOOD_MUTATION:
    case RANDOM_BAD_MUTATION:
        mutat = _get_random_mutation(which_mutation);
        break;
    case RANDOM_XOM_MUTATION:
        mutat = _get_random_xom_mutation();
        break;
    case RANDOM_SLIME_MUTATION:
        mutat = _get_random_slime_mutation();
        break;
    default:
        break;
    }

    if (!is_valid_mutation(mutat))
        return false;

    // [Cha] don't allow teleportitis in sprint
    if (mutat == MUT_TELEPORT && crawl_state.game_is_sprint())
        return false;

    if (you.species == SP_NAGA)
    {
        // gdl: Spit poison 'upgrades' to breathe poison.  Why not...
        if (mutat == MUT_SPIT_POISON)
        {
            if (coinflip())
                return false;

            mutat = MUT_BREATHE_POISON;

            // Breathe poison replaces spit poison (so it takes the slot).
            for (int i = 0; i < 52; ++i)
                if (you.ability_letter_table[i] == ABIL_SPIT_POISON)
                    you.ability_letter_table[i] = ABIL_BREATHE_POISON;
        }
    }

    if (physiology_mutation_conflict(mutat))
        return false;

    const mutation_def& mdef = get_mutation_def(mutat);

    if (you.mutation[mutat] >= mdef.levels)
    {
        bool found = false;
        if (you.species == SP_DEMONSPAWN)
            for (unsigned i = 0; i < you.demonic_traits.size(); ++i)
                if (you.demonic_traits[i].mutation == mutat)
                {
                    // This mutation is about to be re-gained, so there is
                    // no need to redraw any stats or print any messages.
                    found = true;
                    you.mutation[mutat]--;
                    break;
                }
        if (!found)
            return false;
    }

    // God gifts and forced mutations clear away conflicting mutations.
    int rc = _handle_conflicting_mutations(mutat, god_gift || force_mutation, reason);
    if (rc == 1)
        return true;
    if (rc == -1)
        return false;

    ASSERT(rc == 0);

    const unsigned int old_talents = your_talents(false).size();

    bool gain_msg = true;

    you.mutation[mutat]++;

    // More than three messages, need to give them by hand.
    switch (mutat)
    {
    case MUT_STRONG: case MUT_AGILE:  case MUT_CLEVER:
    case MUT_WEAK:   case MUT_CLUMSY: case MUT_DOPEY:
        mprf(MSGCH_MUTATION, gettext("You feel %s."), pgettext_expr("stat", _stat_mut_desc(mutat, true)));
        gain_msg = false;
        break;

    case MUT_LARGE_BONE_PLATES:
        {
            const char *arms;
            if (you.species == SP_FELID)
                arms = "legs";
            else if (you.species == SP_OCTOPODE)
                arms = "tentacles";
            else
                break;
            mpr(replace_all(mdef.gain[you.mutation[mutat]-1], "arms",
                            arms).c_str(), MSGCH_MUTATION);
            gain_msg = false;
        }
        break;

    default:
        break;
    }

    // For all those scale mutations.

	//  원문  notify_stat_change("gaining a mutation");
    you.redraw_armour_class = true;

    notify_stat_change("변이를 얻었다");

    if (gain_msg)
        mpr(mdef.gain[you.mutation[mutat]-1], MSGCH_MUTATION);

    // Do post-mutation effects.
    switch (mutat)
    {
    case MUT_FRAIL:
    case MUT_ROBUST:
    case MUT_RUGGED_BROWN_SCALES:
        calc_hp();
        break;

    case MUT_LOW_MAGIC:
    case MUT_HIGH_MAGIC:
        calc_mp();
        break;

    case MUT_PASSIVE_MAPPING:
        add_daction(DACT_REAUTOMAP);
        break;

    case MUT_HOOVES:
    case MUT_TALONS:
        // Hooves and talons force boots off at 3.
        if (you.mutation[mutat] >= 3 && !you.melded[EQ_BOOTS])
            remove_one_equip(EQ_BOOTS, false, true);
        break;

    case MUT_CLAWS:
        // Claws force gloves off at 3.
        if (you.mutation[mutat] >= 3 && !you.melded[EQ_GLOVES])
            remove_one_equip(EQ_GLOVES, false, true);
        break;

    case MUT_HORNS:
    case MUT_ANTENNAE:
        // Horns & Antennae 3 removes all headgear.  Same algorithm as with
        // glove removal.
        if (you.mutation[mutat] >= 3 && !you.melded[EQ_HELMET])
            remove_one_equip(EQ_HELMET, false, true);
        // Intentional fall-through
    case MUT_BEAK:
        // Horns, beaks, and antennae force hard helmets off.
        if (you.equip[EQ_HELMET] != -1
            && is_hard_helmet(you.inv[you.equip[EQ_HELMET]])
            && !you.melded[EQ_HELMET])
        {
            remove_one_equip(EQ_HELMET, false, true);
        }
        break;

    case MUT_ACUTE_VISION:
        // We might have to turn autopickup back on again.
        autotoggle_autopickup(false);
        break;

    case MUT_NIGHTSTALKER:
        update_vision_range();
        break;

    default:
        break;
    }

    // Amusement value will be 12 * (11-rarity) * Xom's-sense-of-humor.
    xom_is_stimulated(_calc_mutation_amusement_value(mutat));

    take_note(Note(NOTE_GET_MUTATION, mutat, you.mutation[mutat], reason.c_str()));

#ifdef USE_TILE_LOCAL
    if (your_talents(false).size() != old_talents)
    {
        tiles.layout_statcol();
        redraw_screen();
    }
#endif
    if (crawl_state.game_is_hints()
        && your_talents(false).size() > old_talents)
    {
        learned_something_new(HINT_NEW_ABILITY_MUT);
    }
    return true;
}

static bool _delete_single_mutation_level(mutation_type mutat,
                                          const std::string &reason)
{
    if (you.mutation[mutat] == 0)
        return false;

    if (you.innate_mutations[mutat] >= you.mutation[mutat])
        return false;

    const mutation_def& mdef = get_mutation_def(mutat);

    bool lose_msg = true;

    you.mutation[mutat]--;

	//스위치 문이라서 밖에다 저장
	//        mprf(MSGCH_MUTATION, "You feel %s.", _stat_mut_desc(mutat, false));
    switch (mutat)
    {
    case MUT_STRONG: case MUT_AGILE:  case MUT_CLEVER:
    case MUT_WEAK:   case MUT_CLUMSY: case MUT_DOPEY:
        mprf(MSGCH_MUTATION, gettext("You feel %s."), pgettext_expr("stat", _stat_mut_desc(mutat, false)));
        lose_msg = false;
        break;

    case MUT_BREATHE_POISON:
        // can't be removed yet, but still covered:
        if (you.species == SP_NAGA)
        {
            // natural ability to spit poison retakes the slot
            for (int i = 0; i < 52; ++i)
            {
                if (you.ability_letter_table[i] == ABIL_BREATHE_POISON)
                    you.ability_letter_table[i] = ABIL_SPIT_POISON;
            }
        }
        break;

    case MUT_NIGHTSTALKER:
        update_vision_range();
        break;

    default:
        break;
    }

    // For all those scale mutations.
    you.redraw_armour_class = true;
	
	//    notify_stat_change("losing a mutation");
    notify_stat_change("돌연변이를 잃어버렸다");

    if (lose_msg)
        mpr(mdef.lose[you.mutation[mutat]], MSGCH_MUTATION);

    // Do post-mutation effects.
    if (mutat == MUT_FRAIL || mutat == MUT_ROBUST
        || mutat == MUT_RUGGED_BROWN_SCALES)
    {
        calc_hp();
    }
    if (mutat == MUT_LOW_MAGIC || mutat == MUT_HIGH_MAGIC)
        calc_mp();

    take_note(Note(NOTE_LOSE_MUTATION, mutat, you.mutation[mutat], reason.c_str()));

    return true;
}

bool delete_mutation(mutation_type which_mutation, const std::string &reason,
                     bool failMsg,
                     bool force_mutation, bool god_gift,
                     bool disallow_mismatch)
{
    if (!god_gift)
    {
        const god_type god =
            (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                          : GOD_NO_GOD;

        if (god != GOD_NO_GOD)
            god_gift = true;
    }

    mutation_type mutat = which_mutation;


	
    if (!force_mutation)
    {
        if (!god_gift)
        {
            if (player_mutation_level(MUT_MUTATION_RESISTANCE) > 1
                && (player_mutation_level(MUT_MUTATION_RESISTANCE) == 3
                    || coinflip()))
            {
                if (failMsg)
#ifdef JP
					mpr("당신은 잠깐 이상함을 느꼈다.", MSGCH_MUTATION);
#else
                    mpr("You feel rather odd for a moment.", MSGCH_MUTATION);
#endif
                return false;
            }
        }
    }

    if (which_mutation == RANDOM_MUTATION
        || which_mutation == RANDOM_XOM_MUTATION
        || which_mutation == RANDOM_GOOD_MUTATION
        || which_mutation == RANDOM_BAD_MUTATION
        || which_mutation == RANDOM_NON_SLIME_MUTATION)
    {
        while (true)
        {
            if (one_chance_in(1000))
                return false;

            mutat = static_cast<mutation_type>(random2(NUM_MUTATIONS));

            if (you.mutation[mutat] == 0
                && mutat != MUT_STRONG
                && mutat != MUT_CLEVER
                && mutat != MUT_AGILE
                && mutat != MUT_WEAK
                && mutat != MUT_DOPEY
                && mutat != MUT_CLUMSY)
            {
                continue;
            }

            if (which_mutation == RANDOM_NON_SLIME_MUTATION
                && _is_slime_mutation(mutat))
            {
                continue;
            }

            if (you.innate_mutations[mutat] >= you.mutation[mutat])
                continue;

            // MUT_ANTENNAE is 0, and you.attribute[] is initialized to 0.
            if (mutat && mutat == you.attribute[ATTR_APPENDAGE])
                continue;

            const mutation_def& mdef = get_mutation_def(mutat);

            if (random2(10) >= mdef.rarity && !_is_slime_mutation(mutat))
                continue;

            const bool mismatch =
                (which_mutation == RANDOM_GOOD_MUTATION && mdef.bad)
                    || (which_mutation == RANDOM_BAD_MUTATION && !mdef.bad);

            if (mismatch && (disallow_mismatch || !one_chance_in(10)))
                continue;

            break;
        }
    }
    else if (which_mutation == RANDOM_SLIME_MUTATION)
    {
        mutat = _delete_random_slime_mutation();

        if (mutat == NUM_MUTATIONS)
            return false;
    }

    return _delete_single_mutation_level(mutat, reason);
}

bool delete_all_mutations(const std::string &reason)
{
    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        while (_delete_single_mutation_level(static_cast<mutation_type>(i), reason))
            ;
    }

    return !how_mutated();
}

// Return a string describing the mutation.
// If colour is true, also add the colour annotation.
std::string mutation_name(mutation_type mut, int level, bool colour)
{
    // Ignore the player's forms, etc.
    const bool ignore_player = (level != -1);

    const mutation_activity_type active = mutation_activity_level(mut);
    const bool lowered = you.mutation[mut] > player_mutation_level(mut);
    const bool partially_active = (active == MUTACT_PARTIAL
        || active == MUTACT_HUNGER && lowered);
    const bool fully_inactive = (active == MUTACT_INACTIVE);

    // level == -1 means default action of current level
    if (level == -1)
    {
        if (!fully_inactive)
            level = player_mutation_level(mut);
        else // give description of fully active mutation
            level = you.mutation[mut];
    }

    std::string result;
    bool innate_upgrade = (mut == MUT_BREATHE_POISON && you.species == SP_NAGA);

    const mutation_def& mdef = get_mutation_def(mut);

    if (mut == MUT_STRONG || mut == MUT_CLEVER
        || mut == MUT_AGILE || mut == MUT_WEAK
        || mut == MUT_DOPEY || mut == MUT_CLUMSY)
    {
        std::ostringstream ostr;
        ostr << gettext(mdef.have[0]) << level << ").";
        result = ostr.str();
    }
    else if (mut == MUT_ICEMAIL)
    {
        std::ostringstream ostr;
        ostr << gettext(mdef.have[0]) << player_icemail_armour_class() << ").";
        result = ostr.str();
    }
    else if (mut == MUT_DEFORMED && is_useless_skill(SK_ARMOUR))
        result = "Your body is misshapen.";
    else if (result.empty() && level > 0)
        result = gettext(mdef.have[level - 1]);

    if (!ignore_player)
    {
        if (fully_inactive)
        {
            result = "((" + result + "))";
            ++_num_full_suppressed;
        }
        else if (partially_active)
        {
            result = "(" + result + ")";
            ++_num_part_suppressed;
        }

        if (mdef.form_based && _player_can_transform())
        {
            ++_num_form_based;
            result += colour ? "<yellow>*</yellow>" : "*";
        }

        if (you.species == SP_VAMPIRE && !mdef.physical
            && !you.innate_mutations[mut])
        {
            ++_num_hunger_based;
            result += colour ? "<lightred>+</lightred>" : "+";
        }
    }

    if (colour)
    {
        const char* colourname = (mdef.bad ? "red" : "lightgrey");
        const bool permanent   = (you.innate_mutations[mut] > 0);

        if (innate_upgrade)
        {
            if (fully_inactive)
                colourname = "darkgrey";
            else if (partially_active)
                colourname = "blue";
            else
                colourname = "cyan";
        }
        else if (permanent)
        {
            const bool demonspawn = (you.species == SP_DEMONSPAWN);
            const bool extra = (you.mutation[mut] > you.innate_mutations[mut]);

            if (fully_inactive)
                colourname = "darkgrey";
            else if (partially_active)
                colourname = demonspawn ? "yellow"   : "blue";
            else if (extra)
                colourname = demonspawn ? "lightmagenta" : "cyan";
            else
                colourname = demonspawn ? "magenta"      : "lightblue";
        }
        else if (fully_inactive)
            colourname = "darkgrey";
        else if (partially_active)
            colourname = "brown";
        else if (you.form == TRAN_APPENDAGE && you.attribute[ATTR_APPENDAGE] == mut)
            colourname = "lightgreen";
        else if (_is_slime_mutation(mut))
            colourname = "green";

        // Build the result
        std::ostringstream ostr;
        ostr << '<' << colourname << '>' << result
             << "</" << colourname << '>';
        result = ostr.str();
    }

    return result;
}

// The "when" numbers indicate the range of times in which the mutation tries
// to place itself; it will be approximately placed between when% and
// (when + 100)% of the way through the mutations. For example, you should
// usually get all your body slot mutations in the first 2/3 of your
// mutations and you should usually only start your tier 3 facet in the second
// half of your mutations. See _order_ds_mutations() for details.
static const facet_def _demon_facets[] =
{
    // Body Slot facets
    { 0,  { MUT_CLAWS, MUT_CLAWS, MUT_CLAWS },
      { -33, -33, -33 } },
    { 0, { MUT_HORNS, MUT_HORNS, MUT_HORNS },
      { -33, -33, -33 } },
    { 0, { MUT_ANTENNAE, MUT_ANTENNAE, MUT_ANTENNAE },
      { -33, -33, -33 } },
    { 0, { MUT_HOOVES, MUT_HOOVES, MUT_HOOVES },
      { -33, -33, -33 } },
    { 0, { MUT_TALONS, MUT_TALONS, MUT_TALONS },
      { -33, -33, -33 } },
    // Scale mutations
    { 1, { MUT_DISTORTION_FIELD, MUT_DISTORTION_FIELD, MUT_DISTORTION_FIELD },
      { -33, -33, 0 } },
    { 1, { MUT_ICY_BLUE_SCALES, MUT_ICY_BLUE_SCALES, MUT_ICY_BLUE_SCALES },
      { -33, -33, 0 } },
    { 1, { MUT_IRIDESCENT_SCALES, MUT_IRIDESCENT_SCALES, MUT_IRIDESCENT_SCALES },
      { -33, -33, 0 } },
    { 1, { MUT_LARGE_BONE_PLATES, MUT_LARGE_BONE_PLATES, MUT_LARGE_BONE_PLATES },
      { -33, -33, 0 } },
    { 1, { MUT_MOLTEN_SCALES, MUT_MOLTEN_SCALES, MUT_MOLTEN_SCALES },
      { -33, -33, 0 } },
    { 1, { MUT_ROUGH_BLACK_SCALES, MUT_ROUGH_BLACK_SCALES, MUT_ROUGH_BLACK_SCALES },
      { -33, -33, 0 } },
    { 1, { MUT_RUGGED_BROWN_SCALES, MUT_RUGGED_BROWN_SCALES,
        MUT_RUGGED_BROWN_SCALES },
      { -33, -33, 0 } },
    { 1, { MUT_SLIMY_GREEN_SCALES, MUT_SLIMY_GREEN_SCALES, MUT_SLIMY_GREEN_SCALES },
      { -33, -33, 0 } },
    { 1, { MUT_THIN_METALLIC_SCALES, MUT_THIN_METALLIC_SCALES,
        MUT_THIN_METALLIC_SCALES },
      { -33, -33, 0 } },
    { 1, { MUT_THIN_SKELETAL_STRUCTURE, MUT_THIN_SKELETAL_STRUCTURE,
        MUT_THIN_SKELETAL_STRUCTURE },
      { -33, -33, 0 } },
    { 1, { MUT_YELLOW_SCALES, MUT_YELLOW_SCALES, MUT_YELLOW_SCALES },
      { -33, -33, 0 } },
    // Tier 2 facets
    { 2, { MUT_CONSERVE_SCROLLS, MUT_HEAT_RESISTANCE, MUT_IGNITE_BLOOD },
      { -33, 0, 0 } },
    { 2, { MUT_COLD_RESISTANCE, MUT_CONSERVE_POTIONS, MUT_ICEMAIL },
      { -33, 0, 0 } },
    { 2, { MUT_POWERED_BY_DEATH, MUT_POWERED_BY_DEATH, MUT_POWERED_BY_DEATH },
      { -33, 0, 0 } },
    { 2, { MUT_DEMONIC_GUARDIAN, MUT_DEMONIC_GUARDIAN, MUT_DEMONIC_GUARDIAN },
      { -33, 33, 66 } },
    { 2, { MUT_NIGHTSTALKER, MUT_NIGHTSTALKER, MUT_NIGHTSTALKER },
      { -33, 0, 0 } },
    { 2, { MUT_SPINY, MUT_SPINY, MUT_SPINY },
      { -33, 0, 0 } },
    { 2, { MUT_POWERED_BY_PAIN, MUT_POWERED_BY_PAIN, MUT_POWERED_BY_PAIN },
      { -33, 0, 0 } },
    { 2, { MUT_SAPROVOROUS, MUT_FOUL_STENCH, MUT_FOUL_STENCH },
      { -33, 0, 0 } },
    // Tier 3 facets
    { 3, { MUT_CONSERVE_SCROLLS, MUT_HEAT_RESISTANCE, MUT_HURL_HELLFIRE },
      { 50, 50, 50 } },
    { 3, { MUT_COLD_RESISTANCE, MUT_CONSERVE_POTIONS, MUT_PASSIVE_FREEZE },
      { 50, 50, 50 } },
    { 3, { MUT_ROBUST, MUT_ROBUST, MUT_ROBUST },
      { 50, 50, 50 } },
    { 3, { MUT_NEGATIVE_ENERGY_RESISTANCE, MUT_NEGATIVE_ENERGY_RESISTANCE,
          MUT_STOCHASTIC_TORMENT_RESISTANCE },
      { 50, 50, 50 } },
    { 3, { MUT_AUGMENTATION, MUT_AUGMENTATION, MUT_AUGMENTATION },
      { 50, 50, 50 } },
};

static bool _works_at_tier(const facet_def& facet, int tier)
{
    return facet.tier == tier;
}

#define MUTS_IN_SLOT ARRAYSZ(((facet_def*)0)->muts)
static bool _slot_is_unique(const mutation_type mut[MUTS_IN_SLOT],
                            std::set<const facet_def *> facets_used)
{
    std::set<const facet_def *>::const_iterator iter;
    equipment_type eq[MUTS_IN_SLOT];

    int k = 0;
    // find the equipment slot(s) used by mut
    for (unsigned i = 0; i < ARRAYSZ(_body_facets); i++)
    {
        for (unsigned j = 0; j < MUTS_IN_SLOT; j++)
        {
            if (_body_facets[i].mut == mut[j])
                eq[k++] = _body_facets[i].eq;
        }
    }

    if (k == 0)
        return true;

    for (iter = facets_used.begin() ; iter != facets_used.end() ; ++iter)
    {
        for (unsigned i = 0; i < ARRAYSZ(_body_facets); i++)
        {
            if (_body_facets[i].mut == (*iter)->muts[0])
            {
                for (unsigned j = 0; j < ARRAYSZ(eq); j++)
                {
                    if (_body_facets[i].eq == eq[j])
                        return false;
                }
            }
        }
    }

    return true;
}

static std::vector<demon_mutation_info> _select_ds_mutations()
{
    int ct_of_tier[] = { 1, 1, 2, 1 };
    // 1 in 10 chance to create a monstrous set
    if (one_chance_in(10))
    {
        ct_of_tier[0] = 3;
        ct_of_tier[1] = 0;
    }

try_again:
    std::vector<demon_mutation_info> ret;

    ret.clear();
    int absfacet = 0;
    int scales = 0;
    int ice_elemental = 0;
    int fire_elemental = 0;
    int cloud_producing = 0;

    std::set<const facet_def *> facets_used;

    for (int tier = ARRAYSZ(ct_of_tier) - 1; tier >= 0; --tier)
    {
        for (int nfacet = 0; nfacet < ct_of_tier[tier]; ++nfacet)
        {
            const facet_def* next_facet;

            do
            {
                next_facet = &RANDOM_ELEMENT(_demon_facets);
            }
            while (!_works_at_tier(*next_facet, tier)
                   || facets_used.find(next_facet) != facets_used.end()
                   || !_slot_is_unique(next_facet->muts, facets_used));

            facets_used.insert(next_facet);

            for (int i = 0; i < 3; ++i)
            {
                mutation_type m = next_facet->muts[i];

                ret.push_back(demon_mutation_info(m, next_facet->when[i],
                                                  absfacet));

                if (_is_covering(m))
                    ++scales;

                if (m == MUT_COLD_RESISTANCE)
                    ice_elemental++;

                if (m == MUT_CONSERVE_SCROLLS)
                    fire_elemental++;

                if (m == MUT_SAPROVOROUS || m == MUT_IGNITE_BLOOD)
                    cloud_producing++;
            }

            ++absfacet;
        }
    }

    if (scales > 3)
        goto try_again;

    if (ice_elemental + fire_elemental > 1)
        goto try_again;

    if (cloud_producing > 1)
        goto try_again;

    return ret;
}

static std::vector<mutation_type>
_order_ds_mutations(std::vector<demon_mutation_info> muts)
{
    std::vector<mutation_type> out;
    std::vector<int> times;
    FixedVector<int, 1000> time_slots;
    time_slots.init(-1);
    for (unsigned int i = 0; i < muts.size(); i++)
    {
        int first = std::max(0, muts[i].when);
        int last = std::min(100, muts[i].when + 100);
        int k;
        do
        {
            k = 10 * first + random2(10 * (last - first));
        }
        while (time_slots[k] >= 0);
        time_slots[k] = i;
        times.push_back(k);

        // Don't reorder mutations within a facet.
        for (unsigned int j = i; j > 0; j--)
        {
            if (muts[j].facet == muts[j-1].facet && times[j] < times[j-1])
            {
                int earlier = times[j];
                int later = times[j-1];
                time_slots[earlier] = j-1;
                time_slots[later] = j;
                times[j-1] = earlier;
                times[j] = later;
            }
            else
                break;
        }
    }

    for (int time = 0; time < 1000; time++)
        if (time_slots[time] >= 0)
            out.push_back(muts[time_slots[time]].mut);

    return out;
}

static std::vector<player::demon_trait>
_schedule_ds_mutations(std::vector<mutation_type> muts)
{
    std::list<mutation_type> muts_left(muts.begin(), muts.end());

    std::list<int> slots_left;

    std::vector<player::demon_trait> out;

    for (int level = 2; level <= 27; ++level)
    {
        int ct = coinflip() ? 2 : 1;

        for (int i = 0; i < ct; ++i)
            slots_left.push_back(level);
    }

// 데몬스폰 변이 얻는 레벨
    while (!muts_left.empty())
    {
        if (x_chance_in_y(muts_left.size(), slots_left.size()))
        {
            player::demon_trait dt;

            dt.level_gained = slots_left.front();
            dt.mutation     = muts_left.front();

            dprf("Demonspawn will gain %s at level %d",
            get_mutation_def(dt.mutation).wizname, dt.level_gained);

            out.push_back(dt);

            muts_left.pop_front();
        }
        slots_left.pop_front();
    }

    return out;
}

void roll_demonspawn_mutations()
{
    you.demonic_traits = _schedule_ds_mutations(
                         _order_ds_mutations(
                         _select_ds_mutations()));
}

bool perma_mutate(mutation_type which_mut, int how_much,
                  const std::string &reason)
{
    ASSERT(is_valid_mutation(which_mut));

    int cap = get_mutation_def(which_mut).levels;
    how_much = std::min(how_much, cap);

    int rc = 1;
    // clear out conflicting mutations
    int count = 0;
    while (rc == 1 && ++count < 100)
        rc = _handle_conflicting_mutations(which_mut, true, reason);
    ASSERT(rc == 0);

    int levels = 0;
    while (how_much-- > 0)
    {
        if (you.mutation[which_mut] < cap
            && !mutate(which_mut, reason, false, true, false, false, true))
        {
            return levels; // a partial success was still possible
        }
        levels++;
    }
    you.innate_mutations[which_mut] += levels;

    return (levels > 0);
}

//변이 정도 관련? 인듯

int how_mutated(bool all, bool levels)
{
    int j = 0;

    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        if (you.mutation[i])
        {
            if (!all && you.innate_mutations[i] >= you.mutation[i])
                continue;

            if (levels)
            {
                // These allow for 14 levels.
                if (i == MUT_STRONG || i == MUT_CLEVER || i == MUT_AGILE
                    || i == MUT_WEAK || i == MUT_DOPEY || i == MUT_CLUMSY)
                {
                    j += (you.mutation[i] / 5 + 1);
                }
                else
                    j += you.mutation[i];
            }
            else
                j++;
        }
    }
// 여긴 잘 모르니 놔둠..
    dprf("how_mutated(): all = %u, levels = %u, j = %d", all, levels, j);

    return j;
}

// Return whether current tension is balanced
static bool _balance_demonic_guardian()
{
    const int mutlevel = player_mutation_level(MUT_DEMONIC_GUARDIAN);

    int tension = get_tension(GOD_NO_GOD), mons_val = 0, total = 0;
    monster_iterator mons;

    // tension is unfavorably high, perhaps another guardian should spawn
    if (tension*3/4 > mutlevel*6 + random2(mutlevel*mutlevel*2))
        return false;

    for (int i = 0; mons && i <= 20/mutlevel; ++mons)
    {
        mons_val = get_monster_tension(*mons, GOD_NO_GOD);
        const mon_attitude_type att = mons_attitude(*mons);

        if (testbits(mons->flags, MF_DEMONIC_GUARDIAN)
            && total < random2(mutlevel * 5)
            && att == ATT_FRIENDLY
            && !one_chance_in(3)
            && !mons->has_ench(ENCH_LIFE_TIMER))
        {
            mpr(mons->name(DESC_THE) + " "
                + summoned_poof_msg(*mons) + "!", MSGCH_PLAIN);
            monster_die(*mons, KILL_NONE, NON_MONSTER);
        }
        else
            total += mons_val;
    }

    return true;
}

// Primary function to handle and balance demonic guardians, if the tension
// is unfavorably high and a guardian was not recently spawned, a new guardian
// will be made, if tension is below a threshold (determined by the mutations
// level and a bit of randomness), guardians may be dismissed in
// _balance_demonic_guardian()
void check_demonic_guardian()
{
    const int mutlevel = player_mutation_level(MUT_DEMONIC_GUARDIAN);

    if (!_balance_demonic_guardian() &&
        you.duration[DUR_DEMONIC_GUARDIAN] == 0)
    {
        monster_type mt;

        switch (mutlevel)
        {
        case 1:
            mt = random_choose(MONS_WHITE_IMP, MONS_LEMURE, MONS_UFETUBUS,
                               MONS_IRON_IMP, MONS_CRIMSON_IMP, -1);
            break;
        case 2:
            mt = random_choose(MONS_SIXFIRHY, MONS_SMOKE_DEMON, MONS_SOUL_EATER,
                               MONS_SUN_DEMON, MONS_ICE_DEVIL, -1);
            break;
        case 3:
            mt = random_choose(MONS_EXECUTIONER, MONS_BALRUG, MONS_REAPER,
                               MONS_CACODEMON, -1);
            break;
        default:
	      //die("Invalid demonic guardian level : %d", mutlevel);
            die("병약한 악마 수호자의 레벨 : %d", mutlevel);
        }

        monster *guardian = create_monster(mgen_data(mt, BEH_FRIENDLY, &you,
                                                     2, 0, you.pos(),
                                                     MHITYOU, MG_FORCE_BEH));

        if (!guardian)
            return;

        guardian->flags |= MF_NO_REWARD;
        guardian->flags |= MF_DEMONIC_GUARDIAN;

        guardian->add_ench(ENCH_LIFE_TIMER);

        // no more guardians for mutlevel+1 to mutlevel+20 turns
        you.duration[DUR_DEMONIC_GUARDIAN] = 10*(mutlevel + random2(20));
    }
}

void check_antennae_detect()
{
    int radius = player_mutation_level(MUT_ANTENNAE) * 2;
    if (you.religion == GOD_ASHENZARI && !player_under_penance())
        radius = std::max(radius, you.piety / 20);
    if (radius <= 0)
        return;
    radius = std::min(radius, LOS_RADIUS);

    for (radius_iterator ri(you.pos(), radius, C_ROUND); ri; ++ri)
    {
        discover_mimic(*ri, false);
        monster* mon = monster_at(*ri);
        map_cell& cell = env.map_knowledge(*ri);
        if (!mon)
        {
            if (cell.detected_monster())
                cell.clear_monster();
        }
        else if (!mons_is_firewood(mon))
        {
            // [ds] If the PC remembers the correct monster at this
            // square, don't trample it with MONS_SENSED. Forgetting
            // legitimate monster memory affects travel, which can
            // path around mimics correctly only if it can actually
            // *see* them in monster memory -- overwriting the mimic
            // with MONS_SENSED causes travel to bounce back and
            // forth, since every time it leaves LOS of the mimic, the
            // mimic is forgotten (replaced by MONS_SENSED).
            const monster_type remembered_monster = cell.monster();
            if (remembered_monster != mon->type)
            {
                monster_type mc = MONS_SENSED;
                if (you.religion == GOD_ASHENZARI && !player_under_penance())
                    mc = ash_monster_tier(mon);
                env.map_knowledge(*ri).set_detected_monster(mc);

                if (mc == MONS_SENSED_TRIVIAL || mc == MONS_SENSED_EASY
                    || mc == MONS_SENSED_FRIENDLY
                    || testbits(mon->flags, MF_SENSED))
                {
                    continue;
                }

                for (radius_iterator ri2(mon->pos(), 2, C_SQUARE); ri2; ++ri2)
                    if (you.see_cell(*ri2))
                    {
                        mon->flags |= MF_SENSED;
                        interrupt_activity(AI_SENSE_MONSTER);
                    }
            }
        }
    }
}

int handle_pbd_corpses(bool do_rot)
{
    int corpse_count = 0;

    for (radius_iterator ri(you.pos(),
         player_mutation_level(MUT_POWERED_BY_DEATH) * 3); ri; ++ri)
    {
        for (stack_iterator j(*ri); j; ++j)
        {
            if (j->base_type == OBJ_CORPSES && j->sub_type == CORPSE_BODY
                && j->special > 50)
            {
                ++corpse_count;

                int chance = player_mutation_level(MUT_POWERED_BY_DEATH)*16;
                if (do_rot && x_chance_in_y(you.duration[DUR_POWERED_BY_DEATH],
                                chance))
                {
                    j->special -= random2(3);
                }

                if (corpse_count == 7)
                    break;
            }
        }
    }

    return corpse_count;
}

int augmentation_amount()
{
    int amount = 0;
    const int level = player_mutation_level(MUT_AUGMENTATION) + 1;

    for (int i = 1; i < level; ++i)
    {
        if (you.hp <= (i * you.hp_max) / level)
            amount++;
    }

    return amount;
}
