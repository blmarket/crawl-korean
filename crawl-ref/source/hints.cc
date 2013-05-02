/**
 * @file
 * @brief A hints mode as an introduction on how to play Dungeon Crawl.
**/

#include "AppHdr.h"

#include "cio.h"

#include <cstring>
#include <sstream>

#include "hints.h"

#include "abl-show.h"
#include "artefact.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "command.h"
#include "database.h"
#include "decks.h"
#include "describe.h"
#include "files.h"
#include "food.h"
#include "format.h"
#include "fprop.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-pick.h"
#include "mon-util.h"
#include "mutation.h"
#include "options.h"
#include "ouch.h"
#include "jobs.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "showsymb.h"
#include "skills2.h"
#include "species.h"
#include "spl-book.h"
#include "state.h"
#include "stuff.h"
#include "env.h"
#include "tags.h"
#include "terrain.h"
#include "travel.h"
#include "view.h"
#include "viewchar.h"
#include "viewgeom.h"

static species_type _get_hints_species(unsigned int type);
static job_type     _get_hints_job(unsigned int type);
static bool         _hints_feat_interesting(dungeon_feature_type feat);
static void         _hints_describe_disturbance(int x, int y);
static void         _hints_describe_cloud(int x, int y);
static void         _hints_describe_feature(int x, int y);
static bool         _water_is_disturbed(int x, int y);
static void         _hints_healing_reminder();

static int _get_hints_cols()
{
#ifdef USE_TILE_LOCAL
    return crawl_view.msgsz.x;
#else
    int ncols = get_number_of_cols();
    return (ncols > 80 ? 80 : ncols);
#endif
}

hints_state Hints;

void save_hints(writer& outf)
{
    marshallInt(outf, HINT_EVENTS_NUM);
    marshallShort(outf, Hints.hints_type);
    for (int i = 0; i < HINT_EVENTS_NUM; ++i)
        marshallBoolean(outf, Hints.hints_events[i]);
}

void load_hints(reader& inf)
{
    int version = unmarshallInt(inf);
    // Discard everything if the number doesn't match.
    if (version != HINT_EVENTS_NUM)
        return;

    Hints.hints_type = unmarshallShort(inf);
    for (int i = 0; i < HINT_EVENTS_NUM; ++i)
        Hints.hints_events[i] = unmarshallBoolean(inf);
}

// Override init file definition for some options.
void init_hints_options()
{
    if (!crawl_state.game_is_hints())
        return;

    // Clear possible debug messages before messing
    // with messaging options.
    mesclr(true);
//     Options.clear_messages = true;
    Options.auto_list  = true;
    Options.show_more  = true;
    Options.small_more = false;

#ifdef USE_TILE
    Options.tile_tag_pref = TAGPREF_TUTORIAL;
#endif
}

void init_hints()
{
    // Activate all triggers.
    // This is rather backwards: If (true) an event still needs to be
    // triggered, if (false) the relevant message was already printed.
    Hints.hints_events.init(true);

    // Used to compare which fighting means was used most often.
    // XXX: This gets reset with every save, which seems odd.
    //      On the other hand, it's precisely between saves that
    //      players are most likely to forget these.
    Hints.hints_spell_counter   = 0;
    Hints.hints_throw_counter   = 0;
    Hints.hints_melee_counter   = 0;
    Hints.hints_berserk_counter = 0;

    // Store whether explore, stash search or travelling was used.
    // XXX: Also not stored across save games.
    Hints.hints_explored = true;
    Hints.hints_stashes  = true;
    Hints.hints_travel   = true;

    // For occasional healing reminders.
    Hints.hints_last_healed = 0;

    // Did the player recently see a monster turn invisible?
    Hints.hints_seen_invisible = 0;
}

static void _print_hints_menu(hints_types type)
{
    char letter = 'a' + type;
    char desc[100];

    switch (type)
    {
      case HINT_BERSERK_CHAR:
          strcpy(desc, "(신의 도움을 받는 근접 전투 캐릭터)");
          break;
      case HINT_MAGIC_CHAR:
          strcpy(desc, "(마법 사용 캐릭터)");
          break;
      case HINT_RANGER_CHAR:
          strcpy(desc, "(장거리 공격 캐릭터)");
          break;
      default: // no further choices
          strcpy(desc, "(erroneous character)");
          break;
    }

    cprintf("%c - %s %s %s\n",
            letter, _(species_name(_get_hints_species(type)).c_str()),
                    _(get_job_name(_get_hints_job(type))), desc);
}

// Hints mode selection screen and choice.
void pick_hints(newgame_def* choice)
{
    clrscr();

    cgotoxy(1,1);
    formatted_string::parse_string(
        "<white>던전크롤이 처음이신가 보군요!</white>"
        "\n\n"
        "<cyan>아래 캐릭터 중 하나를 선택하세요:</cyan>"
        "\n").display();

    textcolor(LIGHTGREY);

    for (int i = 0; i < HINT_TYPES_NUM; i++)
        _print_hints_menu((hints_types)i);

    formatted_string::parse_string(
        "<brown>\nEsc - 종료"
        "\n* - 힌트모드 캐릭터 무작위 선택"
        "</brown>\n").display();

    while (true)
    {
        int keyn = getch_ck();

        // Random choice.
        if (keyn == '*' || keyn == '+' || keyn == '!' || keyn == '#')
            keyn = 'a' + random2(HINT_TYPES_NUM);

        // Choose character for hints mode game and set starting values.
        if (keyn >= 'a' && keyn <= 'a' + HINT_TYPES_NUM - 1)
        {
            Hints.hints_type = keyn - 'a';
            choice->species  = _get_hints_species(Hints.hints_type);
            choice->job = _get_hints_job(Hints.hints_type);
            choice->weapon = choice->job == JOB_HUNTER ? WPN_BOW
                                                       : WPN_HAND_AXE; // easiest choice for fighters

            return;
        }

        switch (keyn)
        {
        CASE_ESCAPE
            game_ended();
        case 'X':
            cprintf("\n잘가!");
            end(0);
            return;
        }
    }
}

void hints_load_game()
{
    if (!crawl_state.game_is_hints())
        return;

    learned_something_new(HINT_LOAD_SAVED_GAME);

    // Reinitialise counters for explore, stash search and travelling.
    Hints.hints_explored = Hints.hints_events[HINT_AUTO_EXPLORE];
    Hints.hints_stashes  = true;
    Hints.hints_travel   = true;
}

static species_type _get_hints_species(unsigned int type)
{
    switch (type)
    {
    case HINT_BERSERK_CHAR:
        return SP_MINOTAUR;
    case HINT_MAGIC_CHAR:
        return SP_DEEP_ELF;
    case HINT_RANGER_CHAR:
        return SP_CENTAUR;
    default:
        // Use something fancy for debugging.
        return SP_TENGU;
    }
}

static job_type _get_hints_job(unsigned int type)
{
    switch (type)
    {
    case HINT_BERSERK_CHAR:
        return JOB_BERSERKER;
    case HINT_MAGIC_CHAR:
        return JOB_CONJURER;
    case HINT_RANGER_CHAR:
        return JOB_HUNTER;
    default:
        // Use something fancy for debugging.
        return JOB_NECROMANCER;
    }
}

static void _replace_static_tags(string &text)
{
    size_t p;
    while ((p = text.find("$cmd[")) != string::npos)
    {
        size_t q = text.find("]", p + 5);
        if (q == string::npos)
        {
            text += "<lightred>ERROR: unterminated $cmd</lightred>";
            break;
        }

        string command = text.substr(p + 5, q - p - 5);
        command_type cmd = name_to_command(command);

        command = command_to_string(cmd);
        if (command == "<")
            command += "<";

        text.replace(p, q - p + 1, command);
    }

    while ((p = text.find("$item[")) != string::npos)
    {
        size_t q = text.find("]", p + 6);
        if (q == string::npos)
        {
            text += "<lightred>ERROR: unterminated $item</lightred>";
            break;
        }

        string item = text.substr(p + 6, q - p - 6);
        int type;
        for (type = OBJ_WEAPONS; type < NUM_OBJECT_CLASSES; ++type)
            if (item == item_class_name(type, true))
                break;

        item_def dummy;
        dummy.base_type = static_cast<object_class_type>(type);
        dummy.sub_type = 0;
        if (item == "amulet") // yay shared item classes
            dummy.base_type = OBJ_JEWELLERY, dummy.sub_type = AMU_RAGE;
        item = stringize_glyph(get_item_symbol(show_type(dummy).item));

        if (item == "<")
            item += "<";

        text.replace(p, q - p + 1, item);
    }

    // Brand user-input -related (tutorial) items with <w>[(text here)]</w>.
    while ((p = text.find("<input>")) != string::npos)
    {
        size_t q = text.find("</input>", p + 7);
        if (q == string::npos)
        {
            text += "<lightred>ERROR: unterminated <input></lightred>";
            break;
        }

        string input = text.substr(p + 7, q - p - 7);
        input = "<w>[" + input;
        input += "]</w>";
        text.replace(p, q - p + 8, input);
    }
}

// Prints the hints mode welcome screen.
void hints_starting_screen()
{
    cgotoxy(1, 1);
    clrscr();

    int width = _get_hints_cols();
#ifdef USE_TILE_LOCAL
    // Use a more sensible screen width.
    if (width < 80 && width < crawl_view.msgsz.x + crawl_view.hudsz.x)
        width = crawl_view.msgsz.x + crawl_view.hudsz.x;
    if (width > 80)
        width = 80;
#endif

    string text = getHintString("welcome");
    _replace_static_tags(text);

    linebreak_string(text, width);
    display_tagged_block(text);

    {
        mouse_control mc(MOUSE_MODE_MORE);
        getchm();
    }
    redraw_screen();
}

// Called each turn from _input. Better name welcome.
void hints_new_turn()
{
    if (crawl_state.game_is_hints())
    {
        Hints.hints_just_triggered = false;

        if (you.attribute[ATTR_HELD])
            learned_something_new(HINT_CAUGHT_IN_NET);
        else if (i_feel_safe() && !player_in_branch(BRANCH_ABYSS))
        {
            // We don't want those "Whew, it's safe to rest now" messages
            // if you were just cast into the Abyss. Right?

            if (2 * you.hp < you.hp_max
                || 2 * you.magic_points < you.max_magic_points)
            {
                _hints_healing_reminder();
            }
            else if (!you.running
                     && Hints.hints_events[HINT_SHIFT_RUN]
                     && you.num_turns >= 200
                     && you.hp == you.hp_max
                     && you.magic_points == you.max_magic_points)
            {
                learned_something_new(HINT_SHIFT_RUN);
            }
            else if (!you.running
                     && Hints.hints_events[HINT_MAP_VIEW]
                     && you.num_turns >= 500
                     && you.hp == you.hp_max
                     && you.magic_points == you.max_magic_points)
            {
                learned_something_new(HINT_MAP_VIEW);
            }
            else if (!you.running
                     && Hints.hints_events[HINT_AUTO_EXPLORE]
                     && you.num_turns >= 700
                     && you.hp == you.hp_max
                     && you.magic_points == you.max_magic_points)
            {
                learned_something_new(HINT_AUTO_EXPLORE);
            }
        }
        else
        {
            if (2*you.hp < you.hp_max)
                learned_something_new(HINT_RUN_AWAY);

            if (Hints.hints_type == HINT_MAGIC_CHAR && you.magic_points < 1)
                learned_something_new(HINT_RETREAT_CASTER);
        }
    }
}

static void _print_hint(string key, const string arg1 = "",
                        const string arg2 = "")
{
    string text = getHintString(key);
    if (text.empty())
        return mprf(MSGCH_ERROR, "Error, no hint for '%s'.", key.c_str());

    _replace_static_tags(text);
    text = untag_tiles_console(text);
    text = replace_all(text, "$1", arg1);
    text = replace_all(text, "$2", arg2);

    // "\n" to preserve indented parts, the rest is unwrapped, or split into
    // paragraphs by "\n\n", split_string() will ignore the empty line.
    vector<string> chunks = split_string("\n", text);
    for (size_t i = 0; i < chunks.size(); i++)
        mpr(chunks[i], MSGCH_TUTORIAL);

    stop_running();
}

// Once a hints mode character dies, offer some last playing hints.
void hints_death_screen()
{
    string text;

    _print_hint("death");
    more();

    if (Hints.hints_type == HINT_MAGIC_CHAR
        && Hints.hints_spell_counter < Hints.hints_melee_counter)
    {
        _print_hint("death conjurer melee");
    }
    else if (you.religion == GOD_TROG && Hints.hints_berserk_counter <= 3
             && !you.berserk() && !you.duration[DUR_EXHAUSTED])
    {
        _print_hint("death berserker unberserked");
    }
    else if (Hints.hints_type == HINT_RANGER_CHAR
             && 2*Hints.hints_throw_counter < Hints.hints_melee_counter)
    {
        _print_hint("death ranger melee");
    }
    else
    {
        int hint = random2(6);

        bool skip_first_hint = false;
        // If a character has been unusually busy with projectiles and spells
        // give some other hint rather than the first one.
        if (hint == 0 && Hints.hints_throw_counter + Hints.hints_spell_counter
                          >= Hints.hints_melee_counter)
        {
            hint = random2(5) + 1;
            skip_first_hint = true;
        }
        // FIXME: The hints below could be somewhat less random, so that e.g.
        // the message for fighting several monsters in a corridor only happens
        // if there's more than one monster around and you're not in a corridor,
        // or the one about using consumable objects only if you actually have
        // any (useful or unidentified) scrolls/wands/potions.

        if (hint == 5)
        {
            vector<monster* > visible =
                get_nearby_monsters(false, true, true, false);

            if (visible.size() < 2)
            {
                if (skip_first_hint)
                    hint = random2(4) + 1;
                else
                    hint = random2(5);
            }
        }

        _print_hint(make_stringf("death random %d", hint));
    }
    mpr(untag_tiles_console(text), MSGCH_TUTORIAL, 0);
    more();

    mpr("다음 게임에서 봅시다!", MSGCH_TUTORIAL);

    Hints.hints_events.init(false);
}

// If a character survives until Xp 7, the hints mode is declared finished
// and they get a more advanced playing hint, depending on what they might
// know by now.
void hints_finished()
{
    string text;

    crawl_state.type = GAME_TYPE_NORMAL;

    _print_hint("finished");
    more();

    if (Hints.hints_explored)
        _print_hint("finished explored");
    else if (Hints.hints_travel)
        _print_hint("finished travel");
    else if (Hints.hints_stashes)
        _print_hint("finished stashes");
    else
        _print_hint(make_stringf("finished random %d", random2(4)));
    more();

    Hints.hints_events.init(false);

    // Remove the hints mode file.
    you.save->delete_chunk("tut");
}

// Occasionally remind religious characters of sacrifices.
void hints_dissection_reminder(bool healthy)
{
    if (!crawl_state.game_is_hints())
        return;

    if (Hints.hints_just_triggered)
        return;

    // When hungry, give appropriate message or at least don't suggest
    // sacrifice.
    if (you.hunger_state < HS_SATIATED && healthy)
    {
        learned_something_new(HINT_MAKE_CHUNKS);
        return;
    }

    if (!god_likes_fresh_corpses(you.religion))
        return;

    if (Hints.hints_events[HINT_OFFER_CORPSE])
        learned_something_new(HINT_OFFER_CORPSE);
    else if (one_chance_in(8))
    {
        Hints.hints_just_triggered = true;

        _print_hint("dissection reminder");
    }
}

static bool _advise_use_healing_potion()
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def &obj(you.inv[i]);

        if (!obj.defined())
            continue;

        if (obj.base_type != OBJ_POTIONS)
            continue;

        if (!item_type_known(obj))
            continue;

        if (obj.sub_type == POT_CURING
            || obj.sub_type == POT_HEAL_WOUNDS)
        {
            return true;
        }
    }

    return false;
}

void hints_healing_check()
{
    if (2*you.hp <= you.hp_max
        && _advise_use_healing_potion())
    {
        learned_something_new(HINT_HEALING_POTIONS);
    }
}

// Occasionally remind injured characters of resting.
static void _hints_healing_reminder()
{
    if (!crawl_state.game_is_hints())
        return;

    if (you.duration[DUR_POISONING] && 2*you.hp < you.hp_max)
    {
        if (Hints.hints_events[HINT_NEED_POISON_HEALING])
            learned_something_new(HINT_NEED_POISON_HEALING);
    }
    else if (Hints.hints_seen_invisible > 0
             && you.num_turns - Hints.hints_seen_invisible <= 20)
    {
        // If we recently encountered an invisible monster, we need a
        // special message.
        learned_something_new(HINT_NEED_HEALING_INVIS);
        // If that one was already displayed, don't print a reminder.
    }
    else
    {
        if (Hints.hints_events[HINT_NEED_HEALING])
            learned_something_new(HINT_NEED_HEALING);
        else if (you.num_turns - Hints.hints_last_healed >= 50
                 && !you.duration[DUR_POISONING])
        {
            if (Hints.hints_just_triggered)
                return;

            Hints.hints_just_triggered = true;

            string text;
            text =  "전투 후, 탐험하지 않은 지역으로 이동하기 전에는 "
                    "꼭 제자리에서 휴식을 하여, 체력과 MP를 채우도록 하세요. "
                    "전투 중 도망가야 할 상황이 생긴다면, 탐험하지 않은 검게 가려진 지역보다는 "
                    "몬스터들을 처치해온 이미 지나왔고, 밝혀진 길로 도망가는 것이 현명한 판단일 것입니다. "
                    "탁 트인 곳이 아닌, 구석 지형에서 휴식을 취한다면, 던전을 돌아다니는 몬스터들에게 발각되지 않고 "
                    "휴식을 취할 확률이 높아질 것입니다. 휴식을 취하려면, 키보드 숫자패드의 "
                    "<w>5</w>키 혹은 <w>쉬프트-숫자패드 5</w>"
                    "<tiles>키를 누르거나, 커맨드창의 <w>휴식 아이콘을 마우스 좌클릭</w>하시면 됩니다.</tiles>"
                    ".";

            if (you.hp < you.hp_max && you.religion == GOD_TROG
                && you.can_go_berserk())
            {
              text += "\n또한, '광폭화'를 시전하는 것은 강한 적과의 전투에서, 당신이 너무 많은 체력을 소모하지 않는 것을 "
                      "도와줄 것입니다. 광폭화와 같은 스킬을 시전하려면, "
                      "<w>a</w>키를 누르시면 됩니다.";
            }
            mpr(text, MSGCH_TUTORIAL, 0);


            if (is_resting())
                stop_running();
        }
        Hints.hints_last_healed = you.num_turns;
    }
}

// Give a message if you see, pick up or inspect an item type for the
// first time.
void taken_new_item(object_class_type item_type)
{
    switch (item_type)
    {
    case OBJ_WANDS:
        learned_something_new(HINT_SEEN_WAND);
        break;
    case OBJ_SCROLLS:
        learned_something_new(HINT_SEEN_SCROLL);
        break;
    case OBJ_JEWELLERY:
        learned_something_new(HINT_SEEN_JEWELLERY);
        break;
    case OBJ_POTIONS:
        learned_something_new(HINT_SEEN_POTION);
        break;
    case OBJ_BOOKS:
        learned_something_new(HINT_SEEN_SPBOOK);
        break;
    case OBJ_FOOD:
        learned_something_new(HINT_SEEN_FOOD);
        break;
    case OBJ_CORPSES:
        learned_something_new(HINT_SEEN_CARRION);
        break;
    case OBJ_WEAPONS:
        learned_something_new(HINT_SEEN_WEAPON);
        break;
    case OBJ_ARMOUR:
        learned_something_new(HINT_SEEN_ARMOUR);
        break;
    case OBJ_MISSILES:
        learned_something_new(HINT_SEEN_MISSILES);
        break;
    case OBJ_MISCELLANY:
        learned_something_new(HINT_SEEN_MISC);
        break;
    case OBJ_STAVES:
        learned_something_new(HINT_SEEN_STAFF);
        break;
    case OBJ_RODS:
        learned_something_new(HINT_SEEN_ROD);
        break;
    case OBJ_GOLD:
        learned_something_new(HINT_SEEN_GOLD);
        break;
    default: // nothing to be done
        return;
    }
}

// Give a special message if you gain a skill you didn't have before.
void hints_gained_new_skill(skill_type skill)
{
    if (!crawl_state.game_is_hints())
        return;

    learned_something_new(HINT_SKILL_RAISE);

    switch (skill)
    {
    // Special cases first.
    case SK_FIGHTING:
    case SK_ARMOUR:
    case SK_STEALTH:
    case SK_STABBING:
    case SK_TRAPS:
    case SK_UNARMED_COMBAT:
    case SK_INVOCATIONS:
    case SK_EVOCATIONS:
    case SK_DODGING:
    case SK_SHIELDS:
    case SK_THROWING:
    case SK_SPELLCASTING:
    {
        mpr(get_skill_description(skill), MSGCH_TUTORIAL, 0);
        stop_running();
        break;
    }
    // Only one message for all magic skills (except Spellcasting).
    case SK_CONJURATIONS:
    case SK_CHARMS:
    case SK_HEXES:
    case SK_SUMMONINGS:
    case SK_NECROMANCY:
    case SK_TRANSLOCATIONS:
    case SK_TRANSMUTATIONS:
    case SK_FIRE_MAGIC:
    case SK_ICE_MAGIC:
    case SK_AIR_MAGIC:
    case SK_EARTH_MAGIC:
    case SK_POISON_MAGIC:
        learned_something_new(HINT_GAINED_MAGICAL_SKILL);
        break;

    // Melee skills.
    case SK_SHORT_BLADES:
    case SK_LONG_BLADES:
    case SK_AXES:
    case SK_MACES_FLAILS:
    case SK_POLEARMS:
    case SK_STAVES:
        learned_something_new(HINT_GAINED_MELEE_SKILL);
        break;

    // Ranged skills.
    case SK_SLINGS:
    case SK_BOWS:
    case SK_CROSSBOWS:
        learned_something_new(HINT_GAINED_RANGED_SKILL);
        break;

    default:
        break;
    }
}

#ifndef USE_TILE
// As safely as possible, colourize the passed glyph.
// Stringizes it and handles quoting "<".
static string _colourize_glyph(int col, unsigned ch)
{
    cglyph_t g;
    g.col = col;
    g.ch = ch;
    return glyph_to_tagstr(g);
}
#endif

static bool _mons_is_highlighted(const monster* mons)
{
    return (mons->friendly()
                && Options.friend_brand != CHATTR_NORMAL
            || mons_looks_stabbable(mons)
                && Options.stab_brand != CHATTR_NORMAL
            || mons_looks_distracted(mons)
                && Options.may_stab_brand != CHATTR_NORMAL);
}

static bool _advise_use_wand()
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def &obj(you.inv[i]);

        if (!obj.defined())
            continue;

        if (obj.base_type != OBJ_WANDS)
            continue;

        // Wand type unknown, might be useful.
        if (!item_type_known(obj))
            return true;

        // Empty wands are no good.
        if (obj.plus2 == ZAPCOUNT_EMPTY
            || item_ident(obj, ISFLAG_KNOW_PLUSES) && obj.plus <= 0)
        {
            continue;
        }

        // Can it be used to fight?
        switch (obj.sub_type)
        {
        case WAND_FLAME:
        case WAND_FROST:
        case WAND_SLOWING:
        case WAND_MAGIC_DARTS:
        case WAND_PARALYSIS:
        case WAND_FIRE:
        case WAND_COLD:
        case WAND_CONFUSION:
        case WAND_FIREBALL:
        case WAND_TELEPORTATION:
        case WAND_LIGHTNING:
        case WAND_ENSLAVEMENT:
        case WAND_DRAINING:
        case WAND_RANDOM_EFFECTS:
        case WAND_DISINTEGRATION:
            return true;
        }
    }

    return false;
}

void hints_monster_seen(const monster& mon)
{
    if (mons_class_flag(mon.type, M_NO_EXP_GAIN))
    {
        hints_event_type et = mon.type == MONS_TOADSTOOL ?
            HINT_SEEN_TOADSTOOL : HINT_SEEN_ZERO_EXP_MON;

        if (Hints.hints_events[et])
        {
            if (Hints.hints_just_triggered)
                return;

            learned_something_new(et, mon.pos());
            return;
        }

        // Don't do HINT_SEEN_MONSTER for zero exp monsters.
        if (Hints.hints_events[HINT_SEEN_MONSTER])
            return;
    }

    if (!Hints.hints_events[HINT_SEEN_MONSTER])
    {
        if (Hints.hints_just_triggered)
            return;

        if (_mons_is_highlighted(&mon))
            learned_something_new(HINT_MONSTER_BRAND, mon.pos());
        if (mon.friendly())
            learned_something_new(HINT_MONSTER_FRIENDLY, mon.pos());

        if (you.religion == GOD_TROG && you.can_go_berserk()
            && one_chance_in(4))
        {
            learned_something_new(HINT_CAN_BERSERK);
        }
        return;
    }

    stop_running();

    Hints.hints_events[HINT_SEEN_MONSTER] = false;
    Hints.hints_just_triggered = true;

    monster_info mi(&mon);
#ifdef USE_TILE
    // need to highlight monster
    const coord_def gc = mon.pos();
    tiles.place_cursor(CURSOR_TUTORIAL, gc);
    tiles.add_text_tag(TAG_TUTORIAL, mi);
#endif

    string text = "저기 보이는 ";

    if (is_tiles())
    {
        text +=
            string("몬스터는 ") +
            mon.name(DESC_PLAIN).c_str() +
			"(이)군요. 게임 초반에 만날수 있는 약한 몬스터들은 쥐, 뉴트, 작은 뱀, "
            "고블린, 코볼트 등이 있죠. 몬스터들의 좀 더 자세한 정보를 얻고 싶으면, "
            "몬스터 위로 마우스를 올리면 됩니다. 좀 더 자세하면서도 보기 편한 몬스터의 정보를 보려면 "
            "몬스터를 <w>마우스 우클릭</w>하시면 됩니다.";
    }
    else
    {
        text +=
            glyph_to_tagstr(get_mons_glyph(mi)) +
            " is a monster, usually depicted by a letter. Some typical "
            "early monsters look like <brown>r</brown>, <green>l</green>, "
            "<brown>K</brown> or <lightgrey>g</lightgrey>. ";
        if (crawl_view.mlistsz.y > 0)
        {
            text += "Your console settings allowing, you'll always see a "
                    "list of monsters somewhere on the screen.\n";
        }
        text += "You can gain information about it by pressing <w>x</w> and "
                "moving the cursor on the monster, and read the monster "
                "description by then pressing <w>v</w>. ";
    }

    text += "\n이러한 몬스터를, 장비하고 있는 무기(혹은 맨손)로 공격하는 방법은 아주 간단합니다. 몬스터가 위치해 있는 "
            "장소로 화살표 혹은 숫자패드, 혹은 마우스 클릭 등을 이용해 이동하면 몬스터에게 근접 공격을 가하게 되죠. ";
    if (is_tiles())
    {
        text +=
            "참고할 점은, 적대적인 몬스터가 시야 내로 들어왔을 때는, 떨어져 있는 다른 장소로 마우스 클릭을 통해 "
            "자동으로 이동하는 것이 불가능하게 바뀝니다. 잘못된 클릭으로 죽는 것을 방지하기 위함이죠.";
            
    }

    mpr(text, MSGCH_TUTORIAL, 0);

    if (Hints.hints_type == HINT_RANGER_CHAR)
    {
        text =  "하지만, 사냥꾼 직업을 가진 당신이라면 직접 공격보다는 장비하고 있는 활로 "
                "몬스터를 공격하고 싶으실 겁니다. 소지품창("
				"<w>i</w>키로 소지품창 열람)을 통해 활의 정보를 보시면, 해당 무기를 어떻게 사용해야 하는지에 대한 설명을 찾을 수 있을 "
                "것입니다. ";

        if (!you.weapon()
            || you.weapon()->base_type != OBJ_WEAPONS
            || you.weapon()->sub_type != WPN_BOW)
        {
            text += "우선 장거리 무기를 장비(<w>w</w>)합니다, 그리고, 이어지는 설명을 따르세요."
                "<tiles>\n좀 더 간편한 방법으로는, 화면 오른쪽 소지품창에 있는, 활 아이콘을 <w>마우스 우클릭</w>하시면, "
                "활에 대한 좀 더 자세한 설명 화면을 열람할 수 있습니다. 그리고 활 아이콘을 <w>마우스 좌클릭k</w>하면, 해당 활을 "
                "장비하게 되죠.</tiles>";
        }
        else
        {
            text += "<tiles>화면 오른쪽 소지품창에 있는, 당신의 활 아이콘을 <w>마우스 우클릭</w>하시면, "
                    "활에 대한 설명 화면을 열람할 수 있습니다.</tiles>";
        }

        mpr(untag_tiles_console(text), MSGCH_TUTORIAL, 0);

    }
    else if (Hints.hints_type == HINT_MAGIC_CHAR)
    {
        text =  "하지만, 사냥꾼 직업을 가진 당신이라면 직접 공격보다는 마법 주문을 사용하여 "
                "몬스터를 공격하고 싶으실 것입니다. 소지품창("
                "<w>i</w>키로 소지품창 열람)을 통해 마법서의 정보를 보시면, 당신의 주문을 어떻게 사용해야 하는지에 대한 설명을 찾을 수 있을 "
                "것입니다."
                "<tiles>\n좀 더 간편한 방법으로는, 화면 오른쪽 소지품창에 있는, 마법서 아이콘을 <w>마우스 우클릭</w>하시면, "
                "마법서에 대한 좀 더 자세한 설명 화면을 열람할 수 있습니다.</tiles>";
        mpr(untag_tiles_console(text), MSGCH_TUTORIAL, 0);

    }
}

void hints_first_item(const item_def &item)
{
    // Happens if monster is standing on dropped corpse or item.
    if (monster_at(item.pos))
        return;

    if (!Hints.hints_events[HINT_SEEN_FIRST_OBJECT]
        || Hints.hints_just_triggered)
    {
        // NOTE: Since a new player might not think to pick up a
        // corpse (and why should they?), HINT_SEEN_CARRION is done when a
        // corpse is first seen.
        if (!Hints.hints_just_triggered
            && item.base_type == OBJ_CORPSES
            && !monster_at(item.pos))
        {
            learned_something_new(HINT_SEEN_CARRION, item.pos);
        }
        return;
    }

    stop_running();

    Hints.hints_events[HINT_SEEN_FIRST_OBJECT] = false;
    Hints.hints_just_triggered = true;

#ifdef USE_TILE
    const coord_def gc = item.pos;
    tiles.place_cursor(CURSOR_TUTORIAL, gc);
    tiles.add_text_tag(TAG_TUTORIAL, item.name(false, DESC_A), gc);
#endif

    _print_hint("HINT_SEEN_FIRST_OBJECT",
                glyph_to_tagstr(get_item_glyph(&item)));
}

// If the player is wielding a cursed non-slicing weapon then butchery
// isn't currently possible.
static bool _cant_butcher()
{
    const item_def *wpn = you.weapon();

    if (!wpn || wpn->base_type != OBJ_WEAPONS)
        return false;

    return (wpn->cursed() && !can_cut_meat(*wpn));
}

static string _describe_portal(const coord_def &gc)
{
    const string desc = feature_description_at(true, gc);

    ostringstream text;

    // Ziggurat entrances can rarely appear as early as DL 3.
    if ((desc.find("zig") != string::npos) || (desc.find("지구") != string::npos))
    {
        text << "은(는) 무시무시한 몬스터들로 가득한, 특수한 장소로 연결되는 "
                "관문입니다. 왕 초보인 당신은 이 관문 안에 무엇이 있을지 생각하지 않고 "
                "무작정 들어가려는 시도는 하지 않으셨겠죠?  뭐, 추가적으로 덧붙이면 지구라트로 들어가기 위해서는 "
                "아주 많은 금화를 지불해야 합니다. 지금까지 던전에서 주워 모은 금화들보다 훨씬 더 많이 말이죠; 그렇다고 "
                "여기 들어가기 위해 금화를 쓰지 말고 아끼지는 마세요. 상점에서 아이템을 구입하는 데에 "
                "금화를 쓰는 것이, 당신의 생존에 더욱 큰 도움을 줄 "
                "테니까요."

                "\n\n이렇게 경고를 했는데도, <w>아직도</w> 여기 들어가고 싶은 마음이 굴뚝같으시고, 입장에 필요한 금화도 "
                "모으셨다면, ";
    }
    // For the sake of completeness, though it's very unlikely that a
    // player will find a bazaar entrance before reaching XL 7.
    else if ((desc.find("bazaar") != string::npos) || (desc.find("시장") != string::npos))
    {
        text << "은(는) 다른 차원에 위치한 시장으로 통하는 관문입니다. 다수의 상점들이 위치한 장소이죠. "
                "이 곳에 들어가지 않고 머뭇거리신다면, 머지 않아 이 관문은 닫혀버리게 됩니다. "
                "서둘러 들어가셔야겠다고요? ";
    }
    // The sewers can appear from DL 3 to DL 6.
    else
    {
        text << "은(는) 특별한 장소로 통하는 관문입니다. 이 곳에는 관문이 있던 층에 있는 몬스터들보다는 평균적으로 "
                "좀 더 강한 몬스터들이 서식하고 있고, 들어가는 순간 입구가 닫혀 몬스터들을 물리치며 출구를 찾아야 하는 "
                "경우도 있죠. 어떤 몬스터가 나올지는, 이 관문 주변에 서식하는 몬스터들이 좋은 참고가 될 것입니다. "
                "하지만, 이런 위험성을 감수하고 이 관문 내를 정복했다면, 좋은 아이템과 다량의 경험치를 획득하는 것도 가능하죠. "
                "이 관문을 그냥 지나쳐도 페널티는 없습니다. 하지만, 이 관문을 지나친다면, 이 관문은 곧 사라질 것입니다. 즉, "
                "위험을 감수하고 이 관문 안으로 들어갈지, 아니면 지나칠지는 당신의 결정의 몫이죠. "
                ;
    }

    text << "관문 안으로 들어가시려면 관문 위로 올라간 후, <w>></w>키를 누르시면 됩니다. 안에서 밖으로 나오는 출구"
		"<tiles>(입구와 보통 비슷하게 생겼죠)을 찾으셨을 경우에는, 출구 위에서 </tiles>"
            "<console>another <w>"
         << stringize_glyph(get_feat_symbol(DNGN_EXIT_PORTAL_VAULT))
         << "</w> (though NOT the ancient stone arch you'll start "
            "out on) </console>"
            "<w><<</w>키를 누르면 원래의 던전으로 돌아오게 됩니다."
            "<tiles>\n다른 방법으로는, 입구 혹은 출구에 마우스 커서를 올린 후"
            "<w>쉬프트 키를 누른 채로, 마우스 왼쪽 버튼</w>을 눌러도, 해당 관문의 입구나 출구로 "
            "들어가실 수 있습니다.</tiles>";

    return text.str();
}

#define DELAY_EVENT \
{ \
    Hints.hints_events[seen_what] = true; \
    return; \
}

// Really rare or important events should get a comment even if
// learned_something_new() was already triggered this turn.
// NOTE: If put off, the SEEN_<feature> variant will be triggered the
//       next turn, so they may be rare but aren't urgent.
static bool _rare_hints_event(hints_event_type event)
{
    switch (event)
    {
    case HINT_FOUND_RUNED_DOOR:
    case HINT_KILLED_MONSTER:
    case HINT_NEW_LEVEL:
    case HINT_YOU_ENCHANTED:
    case HINT_YOU_SICK:
    case HINT_YOU_POISON:
    case HINT_YOU_ROTTING:
    case HINT_YOU_CURSED:
    case HINT_YOU_HUNGRY:
    case HINT_YOU_STARVING:
    case HINT_GLOWING:
    case HINT_CAUGHT_IN_NET:
    case HINT_YOU_SILENCE:
    case HINT_NEED_POISON_HEALING:
    case HINT_INVISIBLE_DANGER:
    case HINT_NEED_HEALING_INVIS:
    case HINT_ABYSS:
    case HINT_RUN_AWAY:
    case HINT_RETREAT_CASTER:
    case HINT_YOU_MUTATED:
    case HINT_NEW_ABILITY_GOD:
    case HINT_NEW_ABILITY_MUT:
    case HINT_NEW_ABILITY_ITEM:
    case HINT_CONVERT:
    case HINT_GOD_DISPLEASED:
    case HINT_EXCOMMUNICATE:
    case HINT_GAINED_MAGICAL_SKILL:
    case HINT_GAINED_MELEE_SKILL:
    case HINT_GAINED_RANGED_SKILL:
    case HINT_CHOOSE_STAT:
    case HINT_AUTO_EXCLUSION:
        return true;
    default:
        return false;
    }
}

// Allow for a few specific hint mode messages.
static bool _tutorial_interesting(hints_event_type event)
{
    switch (event)
    {
    case HINT_AUTOPICKUP_THROWN:
    case HINT_TARGET_NO_FOE:
    case HINT_YOU_POISON:
    case HINT_YOU_SICK:
    case HINT_CONTAMINATED_CHUNK:
    case HINT_NEW_ABILITY_ITEM:
    case HINT_ITEM_RESISTANCES:
    case HINT_FLYING:
    case HINT_INACCURACY:
    case HINT_HEALING_POTIONS:
    case HINT_GAINED_SPELLCASTING:
    case HINT_FUMBLING_SHALLOW_WATER:
#if TAG_MAJOR_VERSION == 34
    case HINT_MEMORISE_FAILURE:
#endif
    case HINT_SPELL_MISCAST:
    case HINT_CLOUD_WARNING:
    case HINT_ANIMATE_CORPSE_SKELETON:
    case HINT_SKILL_RAISE:
        return true;
    default:
        return false;
    }
}

// A few special tutorial explanations require triggers.
// Initialize the corresponding events, so they can get displayed.
void tutorial_init_hints()
{
    Hints.hints_events.init(false);
    for (int i = 0; i < HINT_EVENTS_NUM; ++i)
        if (_tutorial_interesting((hints_event_type) i))
            Hints.hints_events[i] = true;
}

// Here most of the hints mode messages for various triggers are handled.
void learned_something_new(hints_event_type seen_what, coord_def gc)
{
    if (!crawl_state.game_is_hints_tutorial())
        return;

    // Already learned about that.
    if (!Hints.hints_events[seen_what])
        return;

    // Don't trigger twice in the same turn.
    // Not required in the tutorial.
    if (crawl_state.game_is_hints() && Hints.hints_just_triggered
        && !_rare_hints_event(seen_what))
    {
        return;
    }

    ostringstream text;
    vector<command_type> cmd;

    Hints.hints_just_triggered    = true;
    Hints.hints_events[seen_what] = false;

    switch (seen_what)
    {
    case HINT_SEEN_POTION:
        text << "물약을 획득하셨습니다. "
                "<console> ('<w>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_POTION))
             << "</w>'). Use </console>"
                "<tiles>. 이러한 물약은, 오른쪽 아이템창에서 물약 아이콘을 <w>마우스 왼쪽 버튼</w>으로 클릭하거나, "
                "</tiles>"
                "게임 상에서 <w>%</w>키를 눌러 물약을 마시는 것이 가능하죠.\n"
                "유의할 점은 물약을 마신다고 무조건 긍정적인 효과가 발생하는 건 아니라는 겁니다. 때때로 치명적인 "
                "피해를 주는 물약들도 있으니까요. 이러한 확인되지 않은 물약으로부터의 피해를 방지하기 위해선, 나쁜 물약을 마셔도 "
                "어느정도 감안할 수 있을 정도로 캐릭터가 성장했을 때 확인하는 방법도 있지만, 그렇게 물약을 아낀다면 좋은 효과의 물약을 제 때 "
                "사용할 수 없다는 문제도 있죠. 뭐.. 이건 당신의 판단력이 필요한 문제겠네요.";
        cmd.push_back(CMD_QUAFF);
        break;

    case HINT_SEEN_SCROLL:
        text << "처음으로 마법 두루마리를 주워 보시는군요. "
                "<console> ('<w>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_SCROLL))
             << "</w>'). Type </console>"
                "<tiles>. 마법 두루마리를 사용하는 방법은, 간단히 우측 아이템창에서 두루마리 아이콘을 <w>마우스 왼쪽 버튼</w>으로 클릭하거나, "
                "</tiles>"
                "<w>%</w>키를 눌러 마법 두루마리를 읽는 것이 가능하죠.";
        cmd.push_back(CMD_READ);
        break;

    case HINT_SEEN_WAND:
        text << "다양한 마법들이 깃들어있는 마법봉을 획득하셨습니다! "
                "<console> ('<w>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_WAND))
             << "</w>'). Type </console>"
                "<tiles>. 마법봉을 사용하는 방법은 간단합니다. 우측 아이템창에서 마법봉 <w>마우스 왼쪽 버튼</w>으로 클릭하거나, "
                "</tiles>"
                "<w>%</w>키를 누른 후, 마법봉을 사용할 대상을 마우스 혹은 키보드로 선택한 후 역시 마우스 버튼이나 엔터키를 누르면 되죠.";
        cmd.push_back(CMD_EVOKE);
        break;

    case HINT_SEEN_SPBOOK:
        text << "마법서를 발견하셨군요! "
                "<console> ('<w>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_BOOK))
             << "'</w>) "
             << "<w>%</w>키를 눌러, 마법서에 들어 있는 마법 주문들을 확인하실 수 있습니다. "
                "마법서에 깃들어 있는 마법 주문을 배우려면, "
                "<w>%</w>키를 누른 후 마법서를 선택하시면 되고, 이렇게 배운 마법을 시전하려면 <w>%</w>키를 누르면 되죠.</console>"
                "<tiles>. 우측 아이템창의 마법서 아이콘을 <w>마우스 오른쪽 버튼k</w>으로 클릭하면, 마법서의 설명과 함께 "
                "마법서에 들어 있는 주문들을 확인하실 수 있고, 마법서의 주문들을 배우시기 위해서는 <w>마우스 왼쪽 버튼</w>으로 마법서 아이콘을 클릭하시면, 마법 주문 배우기 화면이 나오죠. </tiles>";
        cmd.push_back(CMD_READ);
        cmd.push_back(CMD_MEMORISE_SPELL);
        cmd.push_back(CMD_CAST_SPELL);

        if (you.religion == GOD_TROG)
        {
            text << "\n그런데 당신은 '"
                 << _(god_name(GOD_TROG).c_str())
                 << "'의 신자군요. 트로그는 마법 사용은 물론 마법을 배우는 것조차 매우 싫어합니다. 하지만 트로그의 신자라면 "
				 "이러한 마법책을 특수능력 사용 (<w>%</w>키)을 통해 불태움으로써, 마법을 싫어하는 신의 총애를 더 받을 수 "
                 "있음과 동시에, 일종의 공격용으로도 사용하는 것이 가능하죠.";
            cmd.push_back(CMD_USE_ABILITY);
        }
        text << "\n힌트 모드에서는, 언제든지"
                "우측 아이템창의 마법서 아이콘을 "
                "<console>having a look in your <w>%</w>nventory at the item in "
                "question.</console>"
                "<tiles><w>마우스 오른쪽 버튼</w>으로 클릭하면, 이러한 정보들을 다시 볼 수 있는게 가능합니다.</tiles>";
        cmd.push_back(CMD_DISPLAY_INVENTORY);
        break;

    case HINT_SEEN_WEAPON:
        text << "무기 아이템을 처음으로 주워보셨군요. "
                "<console>('<w>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_WEAPON))
             << "</w>') </console>"
                "무기를 습득하셨다면, <w>%</w>키를 누르시거나, 혹은 "
                "<tiles>화면 오른쪽 아이템창의 무기 아이콘을 <w>마우스 왼쪽 버튼</w>으로 클릭하면 </tiles>"
                "해당 무기를 장비하게 되죠. 다만 주의하세요! 아무 무기나 줍는다고 잘 쓸 수 있는게 아닙니다. "
                "각각의 무기들은 무기의 종류별로 '무기 스킬 레벨'이라는 일종의 숙련도를 가지고 있고, 익숙하지 않은 종류의 무기는 "
				"서투르게 사용할 수 밖에 없습니다. 무기가 어떠한 부류에 속하는지, 그리고 좀 더 자세한 정보는, 소지품창 화면(<w>%</w>키)"
                "을 여시거나, 혹은 무기 아이콘을 <w>마우스 오른쪽 버튼</w>으로 클릭함으로써 보는 것이 가능합니다."
                ".";

        cmd.push_back(CMD_WIELD_WEAPON);
        cmd.push_back(CMD_DISPLAY_INVENTORY);

        if (Hints.hints_type == HINT_BERSERK_CHAR)
        {
            text << "\n아마도 당신은 도끼류 무기에 익숙해져 있을겁니다. 도끼 기술 스킬이 다른 무기 스킬보다 높다는 이야기이죠. 즉, 다른 무기를 사용하기보다는"
                    "도끼류의 무기를 찾아 사용하는 것이 훨씬 좋습니다. 던전을 탐험하면서, 좀 더 강한 도끼, 마법 효과가 붙은 도끼, 혹은 좀 더 높은 "
                    "강화가 되어 있는 도끼들을 찾아보는 것이 어떨까요?";
        }
        else if (Hints.hints_type == HINT_MAGIC_CHAR)
        {
            text << "\n그러고 보니 당신은 마법사군요. 마법사는 보통 전투시에는 무기를 사용하지 않습니다. "
                    "다만 예비용으로 무기를 들고 다니는것은 때때로 유용하죠. 방어력이나 저항력을 높여주는 효과가 걸린 무기를 드는 것은 생존에 도움이 된답니다.";
        }
        break;

    case HINT_SEEN_MISSILES:
        text << "처음으로 발사체류 아이템을 획득하셨습니다. "
                "<console>('<w>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_MISSILE))
             << "</w>') </console>"
                "이러한 발사체에는 여러 종류가 있는데, 다트나 투척용 그물의 경우는 "
                "별다른 도구가 필요 없이 손으로 던져 사용할 수 있지만, 화살과 같은 발사체 아이템은 "
                "이러한 발사체를 사용할 도구가 필요하죠. 화살이라면 활이 필요하듯이요. 물론 발사체를 좀 더 효과적으로 "
                "사용하기 위해서는, 이러한 발사 도구 무기 스킬도 수련할 필요가 있을 것이고요. "
#ifdef USE_TILE_LOCAL
				"<w>마우스 오른쪽 버튼</w>"
#else
                "Selecting "
#endif
				"으로 아이템창(<w>%</w>키)의 발사체 아이템을 클릭하면, 해당 아이템과, 그 발사 도구 무기에 대한"
                "좀 더 자세한 정보를 얻으실 수 있습니다.";

        cmd.push_back(CMD_DISPLAY_INVENTORY);

        if (Hints.hints_type == HINT_RANGER_CHAR)
        {
            text << "\n아마도 당신은 활 사용에 익숙해져 있을겁니다.활 스킬이 다른 무기 스킬보다 높다는 이야기죠. 즉, 다른 무기를 사용하기보다는 "
                    "활을 계속 사용하는것이 더욱 도움이 됩니다. 이 이야기는, 던전 곳곳에 떨어져 있는 화살 아이템들도 착실하게 주워야 한다는 이야기죠.";
        }
        else if (Hints.hints_type == HINT_MAGIC_CHAR)
        {
            text << "\n그런데 당신은 마법사군요. 아마 당신은 이러한 장거리 공격 도구가 "
                    "크게 필요하지는 않을 겁니다. 특별히 공격력이 더 높거나, 더 좋은 효과를 "
                    "줄 수 있거나 하지 않는다면 말이죠.";
        }
        else
        {
            text << "\n별다른 장거리 무기 스킬이 없는 당신에게는, 손쉽게 던져 사용할 수 있는 다트류나 "
                    "돌멩이류가 현재 쓰기에 가장 적당한 투척 무기일거에요.";
        }
        break;

    case HINT_SEEN_ARMOUR:
        text << "갑옷류 아이템을 "
                "<console>('<w>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_ARMOUR))
             << "</w>') </console>"
                "발견하셨습니다. "
                "<tiles>아이템창의 이와 같은 갑옷/의류 아이템을 클릭하면 이것을 입게 되고, 다시 한번 클릭하면 "
                "벗을 수 있죠. <w>마우스 오른쪽 버튼k</w>으로 클릭하면, 해당 아이템에 대한 "
                "좀 더 자세한 정보를 확인하는 것이 가능합니다. </tiles>"
                "그 외로는 <w>%</w>키를 통해 아이템을 입을 수 있고, 반대로 <w>%</w>키를 눌러 벗을 수 있죠. "
                "아이템에 대한 좀 더 자세한 정보는 소지품 화면(<w>%</w>)에서도 확인 가능합니다.";
        cmd.push_back(CMD_WEAR_ARMOUR);
        cmd.push_back(CMD_REMOVE_ARMOUR);
        cmd.push_back(CMD_DISPLAY_INVENTORY);

        if (you.species == SP_CENTAUR || you.species == SP_MINOTAUR)
        {
            text << "\n종족에 따라 입을 수 없는 방어구가 있는 경우도 있습니다. 당신은 " << _(species_name(you.species).c_str())
                 << "군요." << _(species_name(you.species).c_str()) << " 종족은 " 
				 << (you.species == SP_CENTAUR ? "발굽으로 인해 부츠가 맞지 않는답니다." : "머리에 난 커다란 뿔 때문에, 투구를 쓸 수 없고 모자류만 착용이 가능하죠.");
                 
        }
        break;

    case HINT_SEEN_RANDART:
        text << "이렇게 흰색 이름으로 표시되는, 이상한 이름의 무기나 방어구는 "
                "일반적인 아이템보다 좀 더 많은 마법 효과를 동시에 지니고 있거나, 처음부터 높은 강화 수치를 "
                "가지고 있을 수도 있습니다. 꼭 좋은 아이템은 아니에요. 반대로 좋지 않은 마법 효과나 매우 낮은 강화수치를 가질 수도 있으니까요.";
        break;

    case HINT_SEEN_FOOD:
        text << "이건 음식이군요"
                "<console> ('<w>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_FOOD))
             << "</w>')</console>"
                ". <w>e</w>키를 눌러 먹기 메뉴를 부르거나, "
                "<tiles><w>마우스 왼쪽 버튼</w>으로 우측 소지품창의 음식 아이콘을 클릭하는 것으로</tiles>"
                "음식을 먹을 수 있습니다. 알아 둘 것은, 보존 식량이나 과일과 같은 음식들은 상하지 않고, 어떤 종족도 배가 고픔과 상관 없이 먹는 것이 "
                "가능하지만, 이러한 보존식량은 충분히 나오지 않습니다. 아껴서 먹을 필요가 있다는 것이죠. 꼭 필요한 경우가 아니면, 일반적으로 몬스터의 시체를 해체해서 나오는 고기를 먹는 것이 좋습니다.";
        break;

    case HINT_SEEN_CARRION:
        // NOTE: This is called when a corpse is first seen as well as when
        //       first picked up, since a new player might not think to pick
        //       up a corpse.
        // TODO: Specialcase skeletons and rotten corpses!

        if (gc.x <= 0 || gc.y <= 0)
            text << "아, 시체군요!";
        else
        {
            int i = you.visible_igrd(gc);
            if (i == NON_ITEM)
                text << "아, 시체군요!";
            else
            {
                text << "저건 <console>";
                string glyph = glyph_to_tagstr(get_item_glyph(&mitm[i]));
                const string::size_type found = glyph.find("%");
                if (found != string::npos)
                    glyph.replace(found, 1, "percent");
                text << glyph;
                text << " </console>시체군요.";
#ifdef USE_TILE
                tiles.place_cursor(CURSOR_TUTORIAL, gc);
                tiles.add_text_tag(TAG_TUTORIAL, mitm[i].name(false, DESC_A), gc);
#endif
            }
        }

        text << " 몬스터를 처치하면 종종 시체가 바닥에 놓입니다. 이러한 시체는 "
                "<w>%</w>키를 눌러서 해체하여 고기 조각으로 만들 수 있죠. ";
        cmd.push_back(CMD_BUTCHER);

        if (_cant_butcher())
        {
            text << " 그런데 불행하게도, 당신은 지금 들고 있는 저주받은 무기를 손에서 뗄 수 없기에, "
                    "이 시체를 해체할 수 없군요. "
                    "이 무기의 저주를 해제하기 전까지는, 아깝지만 보존 식량등으로 버티다가, 최대한 빨리 저주 해제 두루마리를 "
                    "찾는 수 밖에요. 어쨌든,";
        }
        text << "이러한 고기조각은 먹기에 썩 유쾌한 것들은 아닐겁니다. 하지만 배가 고프다면 어떻게든 이런 것이라도 먹어야겠죠. '배고픔'상태 혹은 그보다 더 배고픈 상태일때는 <w>%</w>키를 눌러 해당 고기를 "
                "먹을 수 있습니다. 그 고기가 몸에 얼마나 좋냐, 나쁘냐는 따지지 않고 처묵처묵할수 있죠.";
        cmd.push_back(CMD_EAT);

        text << "<tiles> 타일 버전에서는, 마우스 클릭으로도 시체 해체를 할 수 있습니다. "
                "시체 위로 캐릭터를 위치시키면, 오른쪽 소지품창 아래에 몬스터의 시체 아이콘이 생기는 걸 보실 수 있을겁니다. "
                "그럼 <w>Shift</w>키를 누른 상태로, 그 시체 아이콘을 <w>마우스 왼쪽 버튼</w>으로 클릭하시는 것으로도 "
                "시체를 해체하실 수 있습니다. 이렇게 해체한 고기는 아이템 창으로 들어오는데, <w>마우스 왼쪽 버튼"
                "</w>으로 클릭하면 먹을 수 있죠.</tiles>";

        if (god_likes_fresh_corpses(you.religion))
        {
            text << "\n그러고 보니, '"
                 << _(god_name(you.religion).c_str())
                 << "' 신은 시체를 제물로 바치는 것을 좋아합니다. 시체 위에서 신에게 기도(<w>%</w>키)를 드리는 것으로 신에게 시체를 바칠 수 있습니다. 알아 둘 것은 "
                    "신들은 보통 썩은 시체를 제물로 받아주지 않는다는 점이죠. 깨끗한 시체들을 정성스레 공양하세요.";
            cmd.push_back(CMD_PRAY);
        }
        text << "\n힌트 모드에서는, 언제든지"
                "우측 아이템창 아이템의 아이콘을 "
                "<tiles><w>마우스 오른쪽 버튼</w>으로 클릭하면, 이러한 정보들을 다시 보는 것이 가능합니다.</tiles>";
        cmd.push_back(CMD_DISPLAY_INVENTORY);
        break;

    case HINT_SEEN_JEWELLERY:
        text << "당신은 장신구 아이템을 발견하셨습니다. 장신구 아이템은 반지와 "
             << "<console> ('<w>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_RING))
             << "</w>')</console>"
             << "부적으로 "
             //<< "<console> ('<w>"
             //<< stringize_glyph(get_item_symbol(SHOW_ITEM_AMULET))
             //<< "</w>')"
             << "나뉘죠. 이러한 장신구는 <w>%</w>키를 눌러 착용할 수 있고, <w>%</w>키를 눌러 벗을 수 있습니다. "
			    "장신구에 대한 좀 더 자세한 정보는 소지품창(<w>%</w>키)을 통해 확인하는 것이 가능하죠."
             << "<tiles>. 타일 버전에서는 화면 우측 아이템창의 장신구 아이콘을 클릭하여 장신구를 "
                "착용할 수 있고, 벗을 수도 있습니다. 이 장신구가 감정이 되어 있거나, 혹은 착용하자마자 효과를 볼 수 있는 종류의 종류라면, "
				"<w>마우스 우측 버튼</w>으로 우측 소지품창의 해당 장신구 아이템을 클릭하여 아이템의 세부 정보를 확인함으로써,</tiles>"
             << "해당 장신구가 어떠한 효과를 가지고 있는지를 알 수 있습니다."
                "";
        cmd.push_back(CMD_WEAR_JEWELLERY);
        cmd.push_back(CMD_REMOVE_JEWELLERY);
        cmd.push_back(CMD_DISPLAY_INVENTORY);
        break;

    case HINT_SEEN_MISC:
        text << "이건 정말 흥미로운 아이템이군요. "
                "이 아이템을 "
                "<tiles>아이템창에서 클릭해보거나, 혹은 <w>%</w>키로 발동시켜보는 것은 </tiles>"
                "<console>e<w>%</w>oking </console>"
				"어떨까요? 이러한 아이템들중 일부는 먼저 장비(<w>%</w>키)한 후, "
				"발동(<w>%</w>키)해야 하는 것들도 있습니다. 보통의 경우, 이러한 아이템들이 어떠한 효과가 있는지는 "
                "소지품창(<w>%</w>키)을 통해 아이템의 좀 더 자세한 정보를 보는 것으로 알 수 있죠.";
        cmd.push_back(CMD_EVOKE);
        cmd.push_back(CMD_WIELD_WEAPON);
        cmd.push_back(CMD_EVOKE_WIELDED);
        cmd.push_back(CMD_DISPLAY_INVENTORY);
        break;

    case HINT_SEEN_ROD:
        text << "마법 막대 아이템을 획득하셨습니다."
                "<console> ('<w>";
        text << stringize_glyph(get_item_symbol(SHOW_ITEM_STAVE))
             << "</w>', like staves)</console>"
                " 마법 막대 아이템은 <w>%</w>키를 눌러 장비한 후 사용할 수 있죠. "
                "마법 막대는 마법 사용에 대한 지식이 없어도, "
                "마법을 간단하게 사용할 수 있게 해 주는 도구입니다. "
                "<w>%</w>키를 눌러 발동시킬 수 있죠. 마법 막대에서 시전하는 마법의 위력은 "
                "사용자의 발동술 스킬에 비례하여 강해집니다. 또한 마법 막대를 직접 휘둘러 적을 공격하는 것도 가능합니다. "
                "이 때 마법 막대의 공격력은 마법 막대의 재충전 수치 (-1, +3과 같은)와 같죠. 무기의 강화 수치와 같다고 생각하시면 됩니다.";
        cmd.push_back(CMD_WIELD_WEAPON);
        cmd.push_back(CMD_EVOKE_WIELDED);

        text << "<tiles>마법 막대를 장비하고 발동하는것은, 우측 아이템창의 "
                "마법 막대 아이콘을 <w>마우스 왼쪽 버튼</w>으로 클릭함으로써도 할 수 있습니다.</tiles>"
				"\n힌트 모드에서는, 언제든지 소지품창(<w>%</w>키) 혹은 "
				"화면 우측 소지품창의 아이템 아이콘을 <w>마우스 우클릭</w>함으로써,"
                "이러한 정보들을 다시 보는 것이 가능합니다.</tiles>";
        cmd.push_back(CMD_DISPLAY_INVENTORY);
        break;

    case HINT_SEEN_STAFF:
        text << "마법 지팡이를 획득하셨습니다."
                "<console> ('<w>";

        text << stringize_glyph(get_item_symbol(SHOW_ITEM_STAVE))
             << "</w>', like rods)</console>"
                ". 이 아이템을 사용하려면, <w>%</w>키를 눌러 일단 장비를 먼저 해야하죠. "
                "마법사들은, 이와 같은 마법 지팡이를 사용으로써, 이 마법 지팡이의 종류에 따라 더욱 강한 특정 학파의 마법을 "
                "사용할 수 있습니다. 마법 강화 외로도 마법을 좀 더 쉽게 사용하거나, 주변의 마력을 수집하는 등의 다른 기능이 있는 지팡이도 있죠. "
                "물론 지팡이를 휘둘러 공격용으로 쓰는 것도 가능합니다. <tiles>마법 지팡이를 사용하거나, 아니면 특수한 발동 능력이 있는 일부 마법 지팡이를 발동시키려면 "
                "간단히 화면 오른쪽의 소지품창에서 <w>왼쪽 마우스 버튼</w>으로 지팡이 아이콘을 클릭하시면 됩니다.</tiles>"
				"\n힌트 모드에서는, 언제든지 소지품창(<w>%</w>키) 혹은 "
                "화면 우측 소지품창의 아이템 아이콘을 <w>마우스 우클릭</w>함으로써,"
                "이러한 정보들을 다시 보는 것이 가능합니다.</tiles>";
        cmd.push_back(CMD_WIELD_WEAPON);
        cmd.push_back(CMD_DISPLAY_INVENTORY);
        break;

    case HINT_SEEN_GOLD:
        text << "처음으로 금화를 획득하셨군요"
                "<console> ('<yellow>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_GOLD))
             << "</yellow>')</console>"
                ". 다른 아이템들과는 다르게, 금화는 소지품창으로 들어가지 않습니다. "
                "즉, 소지품창을 차지하지도 않고, 따로 무게를 차지하지도 않습니다. "
                "그리고 다시 땅에 버릴 수도 없죠. 금화는 주로 상점에서 아이템을 구입하는데 "
                "사용됩니다. 그 외로는 몇몇 신들에게 헌금으로 바치거나, 혹은 일부 특별한 장소로 이동하는데 통행료로 사용하기도 하죠. ";

        if (!Options.show_gold_turns)
        {
            text << "골드를 주울 때마다, 메시지창에서는 얼마나 골드를"
                    "얻었는지, 그리고 현재 얼만큼의 골드를 모았는지가 꼬박꼬박 표시됩니다."
                    "<w>%</w>키를 누르면 언제든지 가지고 있는 골드가 얼마나 되는지를 확인할 수 있습니다."
                    "<w>%</w> 화면에서도 가능하고요. ";
            cmd.push_back(CMD_LIST_GOLD);
            cmd.push_back(CMD_RESISTS_SCREEN);
        }
        break;

    case HINT_SEEN_STAIRS:
        // Don't give this information during the first turn, to give
        // the player time to have a look around.
        if (you.num_turns < 1)
            DELAY_EVENT;

        text << "저기 보이는 ";
#ifndef USE_TILE
        // Is a monster blocking the view?
        if (monster_at(gc))
            DELAY_EVENT;

        text << glyph_to_tagstr(get_cell_glyph(gc)) << " ";
#else
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Stairs", gc);
#endif
        text << "은(는) 아래로 내려가는 계단입니다. 던전의 좀 더 깊은 곳으로 가시려면, "
                "계단 위에서 <w>%</w>키를 눌러, 던전의 아랫층으로 내려갈 수 있죠. "
                "다시 윗층으로 돌아오시려면, 위로 향하는 계단 위에서 <w>%</w>키를 누르시면  "
                "되고요.";
        cmd.push_back(CMD_GO_DOWNSTAIRS);
        cmd.push_back(CMD_GO_UPSTAIRS);

#ifdef USE_TILE
        text << "\n다른 방법으로는, <w>시프트키</w>를 누른 상태에서 <w>왼쪽 마우스 버튼</w>으로 "
                "계단을 누르셔도, 던전의 윗층이나 아래층으로 내려가실 수 있답니다. "
                "";
#endif
        break;

    case HINT_SEEN_ESCAPE_HATCH:
        if (you.num_turns < 1)
            DELAY_EVENT;

        // monsters standing on stairs
        if (monster_at(gc))
            DELAY_EVENT;

        text << "저기 보이는 ";
#ifndef USE_TILE
        text << glyph_to_tagstr(get_cell_glyph(gc));
        text << " ";
#else
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Escape hatch", gc);
#endif
        text << "은(는) 비상구의 한 종류입니다. 위험한 상황 등이 닥쳤을 때, 비상구 위에서 "
                "<w>%</w>키, <w>%</w>키를 눌러 이러한 비상구를 이용하면 "
                "윗층이나 아래층으로 피신할 수 있죠. "
#ifdef USE_TILE
                " (또는 <w>마우스 왼쪽 버튼</w>과 "
                "<w>쉬프트키</w>의 조합으로도 사용할수 있고요.)"
#endif
                "하지만, 비상구는 계단과 달리 내려간 후 곧바로 다시 올라오거나 내려갈 수 없답니다. 꼭 필요한 경우가 아니면, 층을 이동할떄는 비상구보단 계단을 이용하는게 좋아요.";
        cmd.push_back(CMD_GO_UPSTAIRS);
        cmd.push_back(CMD_GO_DOWNSTAIRS);
        break;

    case HINT_SEEN_BRANCH:
        text << "아, 저기 보이는 ";
#ifndef USE_TILE
        // Is a monster blocking the view?
        if (monster_at(gc))
            DELAY_EVENT;

        // FIXME: Branch entrance character is not being colored yellow.
        text << glyph_to_tagstr(get_cell_glyph(gc)) << " ";
#else
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Branch stairs", gc);
#endif
        text << "은(는) 던전 내의 색다른 장소로 통하는 입구입니다. 흔히 '서브던전'이라고 불리는 곳이죠. "
                "서브던전은 메인 던전과는 다른 분위기의 지형과 구조, 그리고 "
                "그에 맞는 몬스터들이 등장하는 곳입니다. 극히 일부 서브던전은 단층 구조로 "
                "되어 있지만, 대부분의 서브던전은 여러 층으로 이루어져 있습니다. "
                "그리고 서브 던전 내에 또 다른 서브 던전의 입구가 있는 경우도 "
                "존재하죠."

                "\n\n당신이 처음으로 발견하게 되는 서브던전의 입구는 아마도 3곳중 한 곳이 될 것입니다. "
                "만신전, 오크 광산, 짐승 소굴 이렇게 세 곳이죠. 오크 광산과 "
                "짐승 소굴은, 초보 모험가들에게는 위험한 장소가 될 수 있습니다만, "
                "만신전은 완전히 안전한 장소입니다. 그리고 만신전에는, 당신이 새로운 신을 모실수 있도록, "
                "여러 종류의 신들의 제단들이 들어서 있죠.";
        break;

    case HINT_SEEN_PORTAL:
        // Delay in the unlikely event that a player still in hints mode
        // creates a portal with a Trowel card, since a portal vault
        // entry's description doesn't seem to get set properly until
        // after the vault is done being placed.
        if (you.pos() == gc)
            DELAY_EVENT;

        text << "이것";
#ifndef USE_TILE
        // Is a monster blocking the view?
        if (monster_at(gc))
            DELAY_EVENT;

        text << glyph_to_tagstr(get_cell_glyph(gc)) << " ";
#else
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Portal", gc);
#endif
        text << _describe_portal(gc);
        break;

    case HINT_STAIR_BRAND:
        // Monster or player standing on stairs.
        if (actor_at(gc))
            DELAY_EVENT;

#ifdef USE_TILE
        text << "계단 하단에 작은 빗금 마크가 있는 것이 보이시나요? 저 빗금 마크는 "
                "계단이 있는 지형에, 다른 아이템이 놓여 있다는 의미랍니다. 언제든지 저 위로 가서 아이템을 확인하고, 획득할 수 있죠.";
#else
        text << "If any items are covering stairs or an escape hatch, then "
                "that will be indicated by highlighting the <w><<</w> or "
                "<w>></w> symbol, instead of hiding the stair symbol with "
                "an item glyph.";
#endif
        break;

    case HINT_HEAP_BRAND:
        // Monster or player standing on heap.
        if (actor_at(gc))
            DELAY_EVENT;

#ifdef USE_TILE
        text << "아이템 하단에 작은 빗금 마크가 있는 것이 보이시나요? 저 빗금 마크는 "
                "아이템이 있는 바닥 위에, 저 아이템 말고도 하나 이상의 아이템이 쌓여 있다는 의미랍니다. "
                "언제든지 저 위로 가서 아이템을 확인하고, 획득할 수 있죠.";
        break;
#else
        text << "If two or more items are on a single square, then the square "
                "will be highlighted, and the symbol for the item on the top "
                "of the heap will be shown.";
#endif
        break;

    case HINT_TRAP_BRAND:
#ifdef USE_TILE
        // Tiles show both the trap and the item heap.
        return;
#else
        // Monster or player standing on trap.
        if (actor_at(gc))
            DELAY_EVENT;

        text << "If any items are covering a trap, then that will be "
                "indicated by highlighting the <w>^</w> symbol, instead of "
                "hiding the trap symbol with an item glyph.";
#endif
        break;

    case HINT_SEEN_TRAP:
        if (you.pos() == gc)
            text << "이런... 당신은 방금 함정을 작동시키셨습니다. ";
        else
            text << "당신은 방금 함정을 발견하셨습니다. ";

        text << "부주의한 모험가들은 때때로 이러한 위험한 장치에 "
                "걸려 버리기 마련이죠. ";
#ifndef USE_TILE
        {
            cglyph_t g = get_cell_glyph(gc);

            if (g.ch == ' ' || g.col == BLACK)
                g.col = LIGHTCYAN;

            text << " depicted by " << _colourize_glyph(g.col, '^');
        }
#endif
        text << "이러한 함정들은 물리적인 피해를 주거나 (다크나 바늘 등이 "
                "튀어 나오는 식으로), 다른 방법으로는 마법적인 효과를 주는 경우도 있습니다. 임의의 장소로 "
                "공간이동을 시켜 버린다던가... 이러한 함정들은 함정 옆 칸에 서서, <w>Ctrl + 방향키</w> "
                ""
#ifdef USE_TILE
                "혹은 <w>Ctrl + 마우스 왼쪽 버튼</w> 클릭으"
#endif
                "로 함정 해제를 시도할 수 있습니다.";
        break;

    case HINT_SEEN_ALTAR:
        text << "That ";
#ifndef USE_TILE
        // Is a monster blocking the view?
        if (monster_at(gc))
            DELAY_EVENT;

        text << glyph_to_tagstr(get_cell_glyph(gc)) << " ";
#else
        {
            tiles.place_cursor(CURSOR_TUTORIAL, gc);
            string altar = "An altar to ";
            altar += god_name(feat_altar_god(grd(gc)));
            tiles.add_text_tag(TAG_TUTORIAL, altar, gc);
        }
#endif
        text << "is an altar. "
#ifdef USE_TILE
                "By <w>rightclicking</w> on it with your mouse "
#else
                "If you target the altar with <w>x</w>, then press <w>v</w> "
#endif
                "you can get a short description.\n"
                "Press <w>%</w> while standing on the square to join the faith "
                "or read some information about the god in question. Before "
                "taking up the corresponding faith you'll be asked for "
                "confirmation.";
        cmd.push_back(CMD_PRAY);

        if (you.religion == GOD_NO_GOD
            && Hints.hints_type == HINT_MAGIC_CHAR)
        {
            text << "\n\nThe best god for an unexperienced conjurer is "
                    "probably Vehumet, though Sif Muna is a good second "
                    "choice.";
        }
        break;

    case HINT_SEEN_SHOP:
#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, shop_name(gc), gc);
#else
        // Is a monster blocking the view?
        if (monster_at(gc))
            DELAY_EVENT;
#endif
        text << "That "
#ifndef USE_TILE
             << glyph_to_tagstr(get_cell_glyph(gc)) << " "
#endif
                "is a shop. You can enter it by typing <w>%</w> or <w>%</w>"
#ifdef USE_TILE
                ", or by pressing <w>Shift</w> and clicking on it with your "
                "<w>left mouse button</w> "
#endif
                "while standing on the square.";
        cmd.push_back(CMD_GO_UPSTAIRS);
        cmd.push_back(CMD_GO_DOWNSTAIRS);

        text << "\n\nIf there's anything you want which you can't afford yet "
                "you can select those items and press <w>@</w> to put them "
                "on your shopping list. The game will then remind you when "
                "you gather enough gold to buy the items on your list.";
        break;

    case HINT_SEEN_DOOR:
        if (you.num_turns < 1)
            DELAY_EVENT;

#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Closed door", gc);
#endif

        text << "That "
#ifndef USE_TILE
             << glyph_to_tagstr(get_cell_glyph(gc)) << " "
#endif
                "is a closed door. You can open it by walking into it. "
                "Sometimes it is useful to close a door. Do so by pressing "
                "<w>%</w> while standing next to it. If there are several "
                "doors, you will then be prompted for a direction. "
                "Alternatively, you can also use <w>Ctrl-Direction</w>.";
        cmd.push_back(CMD_CLOSE_DOOR);

#ifdef USE_TILE
        text << "\nIn Tiles, the same can be achieved by clicking on an "
                "adjacent door square.";
#endif
        if (!Hints.hints_explored)
        {
            text << "\nTo avoid accidentally opening a door you'd rather "
                    "remain closed during travel or autoexplore, you can mark "
                    "it with an exclusion from the map view (<w>%</w>) with "
                    "<w>ee</w> while your cursor is on the grid in question. "
                    "Such an exclusion will prevent autotravel from ever "
                    "entering that grid until you remove the exclusion with "
                    "another press of <w>%e</w>.";
            cmd.push_back(CMD_DISPLAY_MAP);
            cmd.push_back(CMD_DISPLAY_MAP);
        }
        break;

    case HINT_FOUND_RUNED_DOOR:
#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Runed door", gc);
#endif
        text << "That ";
#ifndef USE_TILE
        text << glyph_to_tagstr(get_cell_glyph(gc)) << " ";
#endif
        text << "is a runed door. The runic writings covering it are a "
                "warning of nearby danger. Other denizens of the dungeon will "
                "typically leave it alone, but other than that it functions "
                "no differently from a normal door. You may elect to disregard "
                "the warning and open it anyway. Doing so will break the runes.";
        break;

    case HINT_KILLED_MONSTER:
        text << "Congratulations, your character just gained some experience "
                "by killing this monster! This will raise some of your skills, "
                "making you more deadly.";
        // A more detailed description of skills is given when you go past an
        // integer point.

        if (you.religion == GOD_TROG)
        {
            text << " Also, kills of demons and living creatures grant you "
                    "favour in the eyes of Trog.";
        }
        break;

    case HINT_NEW_LEVEL:
        if (you.skills[SK_SPELLCASTING])
        {
            if (!crawl_state.game_is_hints())
            {
                text << "Gaining an experience level allows you to learn more "
                        "difficult spells. However, you don't have any level "
                        "two spells in your current spellbook, so you'll just "
                        "have to keep exploring!";
                break;
            }
            text << "Gaining an experience level allows you to learn more "
                    "difficult spells. Time to memorise your second spell "
                    "with <w>%</w>"
#ifdef USE_TILE
                 << " or by <w>clicking</w> on it in the memorisation tab"
#endif
                 << ".";
            cmd.push_back(CMD_MEMORISE_SPELL);
        }
        else
        {
            text << "Well done! Reaching a new experience level is always a "
                    "nice event: you get more health and magic points, and "
                    "occasionally increases to your attributes (strength, "
                    "intelligence, dexterity).";
        }

        if (Hints.hints_type == HINT_MAGIC_CHAR)
        {
            text << "\nAlso, new experience levels let you learn more spells "
                    "(the Spellcasting skill also does this). For now, you "
                    "should try to memorise the second spell of your "
                    "starting book with <w>%a</w>, which can then be zapped "
                    "with <w>%b</w>.";
#ifdef USE_TILE
            text << " Memorising is also possible by doing a <w>left "
                    "click</w> on the book in your inventory, or by clicking "
                    "on the <w>spellbook tab</w> to the left of your "
                    "inventory.";
#endif
            cmd.push_back(CMD_MEMORISE_SPELL);
            cmd.push_back(CMD_CAST_SPELL);
        }
        break;

    case HINT_SKILL_RAISE:

        text << "One of your skills just passed a whole integer point. The "
                "skills you use are automatically trained whenever you gain "
                "experience (by killing monsters). By default, experience goes "
                "towards skills you actively use, although you may choose "
                "otherwise. To view or manage your skill set, type <w>%</w>.";

        cmd.push_back(CMD_DISPLAY_SKILLS);
        break;

    case HINT_GAINED_MAGICAL_SKILL:
        text << "Being skilled in a magical \"school\" makes it easier to "
                "learn and cast spells of this school. Many spells belong to "
                "a combination of several schools, in which case the average "
                "skill in these schools will decide on spellcasting success "
                "and power.";
        break;

    case HINT_GAINED_MELEE_SKILL:
        text << "Being skilled with a particular type of weapon will make it "
                "easier to fight with all weapons of this type, and make you "
                "deal more damage with them. It is generally recommended to "
                "concentrate your efforts on one or two weapon types to become "
                "more powerful in them. Some weapons are closely related, and "
                "being trained in one will ease training the other. This is "
                "true for the following pairs: Short Blades/Long Blades, "
                "Axes/Polearms, Polearms/Staves, Axes/Maces and Maces/Staves.";
        break;

    case HINT_GAINED_RANGED_SKILL:
        text << "Being skilled in a particular type of ranged attack will let "
                "you deal more damage when using the appropriate weapons. It "
                "is usually best to concentrate on one type of ranged attack "
                "(including spells), and to add another one as back-up.";
        break;

    case HINT_CHOOSE_STAT:
        text << "Every third level you get to choose a stat to raise: "
                "Strength, Intelligence, or Dexterity. "
                "<w>Strength</w> affects the amount you can carry and makes it "
                "easier to wear heavy armour. "
                "<w>Intelligence</w> makes it easier to cast spells and "
                "reduces the amount by which you hunger when you do so. "
                "<w>Dexterity</w> increases your evasion "
                "and makes it easier to dodge attacks or traps.\n";
        break;

    case HINT_YOU_ENCHANTED:
        text << "Enchantments of all types can befall you temporarily. "
                "Brief descriptions of these appear at the lower end of the "
                "stats area. Press <w>%</w> for more details. A list of all "
                "possible enchantments is in the manual (<w>%5</w>).";
        cmd.push_back(CMD_DISPLAY_CHARACTER_STATUS);
        cmd.push_back(CMD_DISPLAY_COMMANDS);
        break;

    case HINT_CONTAMINATED_CHUNK:
        text << "Chunks that are described as <brown>contaminated</brown> will "
                "occasionally make you nauseated when eaten. However, since food is "
                "scarce in the dungeon, you'll often have to risk it.\n"
                "While nauseated, you can't stomach anything else unless you're "
                "almost starving.";
            break;

    case HINT_YOU_SICK:
        if (crawl_state.game_is_hints())
        {
            // Hack: reset hints_just_triggered, to force recursive calling of
            // learned_something_new(). Don't do this for the tutorial!
            Hints.hints_just_triggered = false;
            learned_something_new(HINT_YOU_ENCHANTED);
            Hints.hints_just_triggered = true;
        }
        text << "While sick, your hitpoints won't regenerate and your attributes "
                "may decrease. Sickness wears off with time, so you should wait it "
                "out with %.";
        cmd.push_back(CMD_REST);
        break;

    case HINT_YOU_POISON:
        if (crawl_state.game_is_hints())
        {
            // Hack: reset hints_just_triggered, to force recursive calling of
            // learned_something_new(). Don't do this for the tutorial!
            Hints.hints_just_triggered = false;
            learned_something_new(HINT_YOU_ENCHANTED);
            Hints.hints_just_triggered = true;
        }
        text << "Poison will slowly reduce your HP. You can try to wait it out "
                "with <w>%</w>, but if you're low on hit points it's usually safer "
                "to quaff a potion of curing.";
        cmd.push_back(CMD_REST);
        break;

    case HINT_YOU_ROTTING:
        // Hack: Reset hints_just_triggered, to force recursive calling of
        //       learned_something_new().
        Hints.hints_just_triggered = false;
        learned_something_new(HINT_YOU_ENCHANTED);
        Hints.hints_just_triggered = true;

        text << "Ugh, your flesh is rotting! Not only does this slowly "
                "reduce your HP, it also slowly reduces your <w>maximum</w> "
                "HP (your usual maximum HP will be indicated by a number in "
                "parentheses).\n"
                "While you can wait it out, you'll probably want to stop the "
                "rotting as soon as possible by <w>%</w>uaffing a potion of "
                "curing, since the longer you wait the more your maximum HP "
                "will be reduced. Once you've stopped rotting you can restore "
                "your maximum HP to normal by drinking potions of curing and "
                "heal wounds while fully healed.";
        cmd.push_back(CMD_QUAFF);
        break;

    case HINT_YOU_CURSED:
        text << "Cursed equipment, once worn or wielded, cannot be dropped or "
                "removed. Curses can be removed by reading certain scrolls.";
        break;

    case HINT_REMOVED_CURSE:
        text << "All the curses on your worn equipment just got cancelled. "
                "Since cursed items are more likely to have bad properties, "
                "the game makes a note of this, so you don't forget about it.";
        break;

    case HINT_YOU_HUNGRY:
        text << "There are two ways to overcome hunger: food you started "
                "with or found, and self-made chunks from corpses. To get the "
                "latter, all you need to do is <w>%</w>hop up a corpse. ";
        cmd.push_back(CMD_BUTCHER);

        if (_cant_butcher())
        {
            text << "Unfortunately you can't butcher corpses right now, "
                    "since the cursed weapon you're wielding can't slice up "
                    "meat, and you can't let go of it to wield your pocket "
                    "knife. ";
        }
        else
        {
            text << "Luckily, all adventurers carry a pocket knife with them "
                    "which is perfect for butchering. ";
        }

        text << "Try to dine on chunks in order to save permanent food.";

        if (Hints.hints_type == HINT_BERSERK_CHAR)
            text << "\nNote that you cannot Berserk while very hungry or worse.";
        break;

    case HINT_YOU_STARVING:
        text << "You are now suffering from terrible hunger. You'll need to "
                "<w>%</w>at something quickly, or you'll die. The safest "
                "way to deal with this is to simply eat something from your "
                "inventory, rather than wait for a monster to leave a corpse. "
                "In an emergency, potions can also provide a very small amount "
                "of nutrition.";
        cmd.push_back(CMD_EAT);

        if (Hints.hints_type == HINT_MAGIC_CHAR)
            text << "\nNote that you cannot cast spells while starving.";
        break;

    case HINT_MULTI_PICKUP:
        text << "There are a lot of items here. You can pick them up one by one, "
                "but you can also choose them from a menu: Type <w>%</w><w>%</w> "
#ifdef USE_TILE
                "or <w>click</w> on the player doll "
#endif
                "to enter the pickup menu. To leave the menu, confirm your "
                "selection with <w>Enter</w>.";
        cmd.push_back(CMD_PICKUP);
        cmd.push_back(CMD_PICKUP);
        break;

    case HINT_HEAVY_LOAD:
        if (you.burden_state != BS_UNENCUMBERED)
        {
            text << "It is not usually a good idea to run around encumbered; "
                    "it slows you down and increases your hunger.";
        }
        else
        {
            text << "Sadly, your inventory is limited to 52 items, and it "
                    "appears your knapsack is full.";
        }

        text << " However, this is easy enough to rectify: simply "
                "<w>%</w>rop some of the stuff you don't need or that's too "
                "heavy to lug around permanently.";
        cmd.push_back(CMD_DROP);

#ifdef USE_TILE
        text << " In the drop menu you can then comfortably select which "
                "items to drop by pressing their inventory letter, or by "
                "clicking on them.";
#endif

        if (Hints.hints_stashes)
        {
            text << "\n\nYou can easily find items you've left on the floor "
                    "with the <w>%</w> command, which will let you "
                    "seach for all known items in the dungeon. For example, "
                    "<w>% \"dagger\"</w> will list all daggers. You can "
                    "can then travel to one of the spots.";
            Hints.hints_stashes = false;
            cmd.push_back(CMD_SEARCH_STASHES);
            cmd.push_back(CMD_SEARCH_STASHES);
        }

        text << "\n\nBe warned that items that you leave on the floor can "
                "be picked up and used by monsters.";
        break;

    case HINT_ROTTEN_FOOD:
        if (!crawl_state.game_is_hints())
        {
            text << "One or more of the chunks or corpses you carry has "
                    "started to rot. While some species can eat rotten "
                    "meat, you can't.";
            break;
        }
        text << "One or more of the chunks or corpses you carry has started "
                "to rot. Few species can digest these, so you might just as "
                "well <w>%</w>rop them now. "
                "When selecting items from a menu, there's a shortcut "
                "(<w>,</w>) to select all items in your inventory at once "
                "that are useless to you.";
        cmd.push_back(CMD_DROP);
        break;

    case HINT_MAKE_CHUNKS:
        text << "How lucky! That monster left a corpse which you can now "
                "<w>%</w>hop up";
        cmd.push_back(CMD_BUTCHER);

        if (_cant_butcher())
        {
            text << "(or which you <w>could</w> chop up if it weren't for "
                    "the fact that you can't let go of your cursed "
                    "non-chopping weapon)";
        }
        text << ". One or more chunks will appear that you can then "
                "<w>%</w>at. Beware that some chunks may be, sometimes or "
                "always, hazardous. You can find out whether that might be the "
                "case by ";
        cmd.push_back(CMD_EAT);

#ifdef USE_TILE
        text << "hovering your mouse over the corpse or chunk.";
#else
        text << "<w>v</w>iewing a corpse or chunk on the floor or by having a "
                "look at it in your <w>%</w>nventory.";
        cmd.push_back(CMD_DISPLAY_INVENTORY);
#endif
        break;

    case HINT_OFFER_CORPSE:
        if (!god_likes_fresh_corpses(you.religion)
            || you.hunger_state < HS_SATIATED)
        {
            return;
        }

        text << "Hey, that monster left a corpse! If you don't need it for "
                "food or other purposes, you can sacrifice it to "
             << god_name(you.religion)
             << " by <w>%</w>raying over it to offer it. ";
        cmd.push_back(CMD_PRAY);
        break;

    case HINT_SHIFT_RUN:
        text << "Walking around takes fewer keystrokes if you press "
                "<w>Shift-direction</w> or <w>/ direction</w>. "
                "That will let you run until a monster comes into sight or "
                "your character sees something interesting.";
        break;

    case HINT_MAP_VIEW:
        text << "As you explore a level, orientation can become difficult. "
                "Press <w>%</w> to bring up the level map. Typing <w>?</w> "
                "shows the list of level map commands. "
                "Most importantly, moving the cursor to a spot and pressing "
                "<w>.</w> or <w>Enter</w> lets your character move there on "
                "its own.";
        cmd.push_back(CMD_DISPLAY_MAP);

#ifdef USE_TILE
        text << "\nAlso, clicking on the right-side minimap with your "
                "<w>right mouse button</w> will zoom into that dungeon area. "
                "Clicking with the <w>left mouse button</w> instead will let "
                "you move there.";
#endif
        break;

    case HINT_AUTO_EXPLORE:
        if (!Hints.hints_explored)
            return;

        text << "Fully exploring a level and picking up all the interesting "
                "looking items can be tedious. To save on this tedium you "
                "can press <w>%</w> to auto-explore, which will "
                "automatically explore unmapped regions, automatically pick "
                "up interesting items, and stop if a monster or interesting "
                "dungeon feature (stairs, altar, etc.) is encountered.";
        cmd.push_back(CMD_EXPLORE);
        Hints.hints_explored = false;
        break;

    case HINT_DONE_EXPLORE:
        // XXX: You'll only get this message if you're using auto exploration.
        text << "Hey, you've finished exploring the dungeon on this level! "
                "You can search for stairs from the level map (<w>%</w>) "
                "by pressing <w>></w>. The cursor will jump to the nearest "
                "staircase, and by pressing <w>.</w> or <w>Enter</w> your "
                "character can move there, too. Each level of Crawl has three "
#ifndef USE_TILE
                "white "
#endif
                "up and three "
#ifndef USE_TILE
                "white "
#endif
                "down stairs. Unexplored parts can often be accessed via "
                "another level.";
        cmd.push_back(CMD_DISPLAY_MAP);
        break;

    case HINT_AUTO_EXCLUSION:
        // In the highly unlikely case the player encounters a
        // hostile statue or oklob plant during the hints mode...
        if (Hints.hints_explored)
        {
            // Hack: Reset hints_just_triggered, to force recursive calling of
            //       learned_something_new().
            Hints.hints_just_triggered = false;
            learned_something_new(HINT_AUTO_EXPLORE);
            Hints.hints_just_triggered = true;
        }
        text << "\nTo prevent autotravel or autoexplore taking you into "
                "dangerous territory, you can set travel exclusions by "
                "entering the map view (<w>%</w>) and then toggling the "
                "exclusion radius on the monster position with <w>e</w>. "
                "To make this easier some immobile monsters listed in the "
                "<w>auto_exclude</w> option (such as this one) are considered "
                "dangerous enough to warrant an automatic setting of an "
                "exclusion. It will be automatically cleared if you manage to "
                "kill the monster. You could also manually remove the "
                "exclusion with <w>%ee</w> but unless you remove this monster "
                "from the auto_exclude list, the exclusion will be reset the "
                "next turn.";
        cmd.push_back(CMD_DISPLAY_MAP);
        cmd.push_back(CMD_DISPLAY_MAP);
        break;

    case HINT_HEALING_POTIONS:
        text << "Your hit points are getting dangerously low. Retreat and/or "
                "quaffing a potion of heal wounds or curing might be a good idea.";
        break;

    case HINT_NEED_HEALING:
        text << "If you're low on hitpoints or magic and there's no urgent "
                "need to move, you can rest for a bit. Ideally, you should "
                "retreat to an area you've already explored and cleared "
                "of monsters before resting, since resting on the edge of "
                "the explored terrain increases the risk of rest being "
                "interrupted by a wandering monster. Press <w>%</w> or "
                "<w>shift-numpad-5</w>"
#ifdef USE_TILE
                ", or click into the stat area"
#endif
                " to do so.";
        cmd.push_back(CMD_REST);
        break;

    case HINT_NEED_POISON_HEALING:
        text << "Your poisoning could easily kill you, so now would be a "
                "good time to <w>%</w>uaff a potion of heal wounds or, "
                "better yet, a potion of curing. If you have seen neither "
                "of these so far, try unknown ones in your inventory. Good "
                "luck!";
        cmd.push_back(CMD_QUAFF);
        break;

    case HINT_INVISIBLE_DANGER:
        text << "Fighting against a monster you cannot see is difficult. "
                "Your best bet is probably a strategic retreat, be it via "
                "teleportation or by getting off the level. "
                "Or else, luring the monster into a corridor should at least "
                "make it easier for you to hit it.";

        // To prevent this text being immediately followed by the next one...
        Hints.hints_last_healed = you.num_turns - 30;
        break;

    case HINT_NEED_HEALING_INVIS:
        text << "You recently noticed an invisible monster, so unless you "
                "killed it or left the scene resting might not be safe. If you "
                "still need to replenish your hitpoints or magic, you'll have "
                "to quaff an appropriate potion. For normal resting you will "
                "first have to get away from the danger.";

        Hints.hints_last_healed = you.num_turns;
        break;

    case HINT_CAN_BERSERK:
        // Don't print this information if the player already knows it.
        if (Hints.hints_berserk_counter)
            return;

        text << "Against particularly difficult foes, you should use your "
                "Berserk <w>%</w>bility. Berserk will last longer if you "
                "kill a lot of monsters.";
        cmd.push_back(CMD_USE_ABILITY);
        break;

    case HINT_POSTBERSERK:
        text << "Berserking is extremely exhausting! It burns a lot of "
                "nutrition, and afterwards you are slowed down and "
                "occasionally even pass out. Press <w>%</w> to see your "
                "current exhaustion status.";
        cmd.push_back(CMD_DISPLAY_CHARACTER_STATUS);
        break;

    case HINT_RUN_AWAY:
        text << "Whenever you've got only a few hitpoints left and you're in "
                "danger of dying, check your options carefully. Often, "
                "retreat or use of some item might be a viable alternative "
                "to fighting on.";

        if (you.species == SP_CENTAUR)
        {
            text << " As a four-legged centaur you are particularly quick - "
                    "running is an option!";
        }

        text << " If retreating to another level, keep in mind that monsters "
                "may follow you if they're standing right next to you when "
                "you start climbing or descending the stairs. And even if "
                "you've managed to shake them off, they'll still be there when "
                "you come back, so you might want to use a different set of "
                "stairs when you return.";

        if (you.religion == GOD_TROG && you.can_go_berserk())
        {
            text << "\nAlso, with "
                 << apostrophise(god_name(you.religion))
                 << " support you can use your Berserk ability (<w>%</w>) "
                    "to temporarily gain more hitpoints and greater "
                    "strength. Bear in mind that berserking at the last "
                    "minute is often risky, and prevents you from using "
                    "items to escape!";
            cmd.push_back(CMD_USE_ABILITY);
        }
        break;

    case HINT_RETREAT_CASTER:
        text << "Without magical power you're unable to cast spells. While "
                "melee is a possibility, that's not where your strengths "
                "lie, so retreat (if possible) might be the better option.";

        if (_advise_use_wand())
        {
            text << "\n\nOr you could e<w>%</w>oke a wand to deal damage.";
            cmd.push_back(CMD_EVOKE);
        }
        break;

    case HINT_YOU_MUTATED:
        text << "Mutations can be obtained from several sources, among them "
                "potions, spell miscasts, and overuse of strong enchantments "
                "like invisibility. The only reliable way to get rid of "
                "mutations is with potions of cure mutation. There are about "
                "as many harmful as beneficial mutations, and some of them "
                "have multiple levels. Check your mutations with <w>%</w>.";
        cmd.push_back(CMD_DISPLAY_MUTATIONS);
        break;

    case HINT_NEW_ABILITY_GOD:
        switch (you.religion)
        {
        // Gods where first granted ability is active.
        case GOD_KIKUBAAQUDGHA: case GOD_YREDELEMNUL: case GOD_NEMELEX_XOBEH:
        case GOD_ZIN:           case GOD_OKAWARU:     case GOD_SIF_MUNA:
        case GOD_TROG:          case GOD_ELYVILON:    case GOD_LUGONU:
            text << "You just gained a divine ability. Press <w>%</w> to "
                    "take a look at your abilities or to use one of them.";
            cmd.push_back(CMD_USE_ABILITY);
            break;

        // Gods where first granted ability is passive.
        default:
            text << "You just gained a divine ability. Press <w>%</w> "
#ifdef USE_TILE
                    "or press <w>Shift</w> and <w>right-click</w> on the "
                    "player tile "
#endif
                    "to take a look at your abilities.";
            cmd.push_back(CMD_DISPLAY_RELIGION);
            break;
        }
        break;

    case HINT_NEW_ABILITY_MUT:
        text << "That mutation granted you a new ability. Press <w>%</w> to "
                "take a look at your abilities or to use one of them.";
        cmd.push_back(CMD_USE_ABILITY);
        break;

    case HINT_NEW_ABILITY_ITEM:
        // Specialcase flight because it's a guaranteed trigger in the
        // tutorial.
        if (you.evokable_flight())
        {
            text << "Flight will allow you to cross deep water or lava. To "
                    "activate it, select the corresponding ability in the ability "
                    "menu (<w>%</w>"
#ifdef USE_TILE
                    " or via <w>mouseclick</w> in the <w>command panel</w>"
#endif
                    "). Once flying, keep an eye on the status line and messages "
                    "as it will eventually time out and may cause you to fall "
                    "into water and drown.";
        }
        else
        {
            text << "That item you just equipped granted you a new ability. "
                    "Press <w>%</w> "
#ifdef USE_TILE
                    "(or <w>click</w> in the <w>command panel</w>) "
#endif
                    "to take a look at your abilities or to use one of them.";
        }
        cmd.push_back(CMD_USE_ABILITY);
        break;

    case HINT_ITEM_RESISTANCES:
        text << "Equipping this item affects your resistances. Check the "
                "overview screen (<w>%</w>"
#ifdef USE_TILE
                " or click on the <w>character overview button</w> in the "
                "command panel"
#endif
                ") for details.";
        cmd.push_back(CMD_RESISTS_SCREEN);
        break;

    case HINT_FLYING:
        if (you.evokable_flight())
        {
            text << "To stop flying, use the corresponding ability "
                    "in the ability menu (<w>%</w>).";
            cmd.push_back(CMD_USE_ABILITY);
        }
        break;

    case HINT_INACCURACY:
        text << "Not all items are useful, and some of them are outright "
                "harmful. Press <w>%</w> ";
#ifdef USE_TILE
        text << "or <w>click</w> on your equipped amulet to remove it.";
#else
        text << "to remove your amulet.";
#endif
        cmd.push_back(CMD_REMOVE_JEWELLERY);
        break;

    case HINT_CONVERT:
        if (you.religion == GOD_XOM)
            return _print_hint("HINT_CONVERT Xom");

        _print_hint("HINT_CONVERT");
        break;

    case HINT_GOD_DISPLEASED:
        text << "Uh-oh, " << god_name(you.religion) << " is growing "
                "displeased because your piety is running low. Possibly this "
                "is the case because you're committing heretic acts, "
                "because " << god_name(you.religion) << " finds your "
                "worship lacking, or a combination of the two. "
                "If your piety goes to zero, then you'll be excommunicated. "
                "Better get cracking on raising your piety, and/or stop "
                "annoying your god. ";

        text << "In any case, you'd better reread the religious description. "
                "To do so, type <w>%</w>"
#ifdef USE_TILE
                " or press <w>Shift</w> and <w>right-click</w> on your avatar"
#endif
                ".";
        cmd.push_back(CMD_DISPLAY_RELIGION);
        break;

    case HINT_EXCOMMUNICATE:
    {
        const god_type new_god   = (god_type) gc.x;
        const int      old_piety = gc.y;

        god_type old_god = GOD_NO_GOD;
        for (int i = 0; i < NUM_GODS; i++)
            if (you.worshipped[i] > 0)
            {
                old_god = (god_type) i;
                break;
            }

        const string old_god_name  = god_name(old_god);
        const string new_god_name  = god_name(new_god);

        if (new_god == GOD_NO_GOD)
        {
            if (old_piety < 1)
            {
                text << "Uh-oh, " << old_god_name << " just excommunicated you "
                        "for running out of piety (your divine favour went "
                        "to nothing). Maybe you repeatedly violated the "
                        "religious rules, or maybe you failed to please your "
                        "deity often enough, or some combination of the two. "
                        "If you can find an altar dedicated to "
                     << old_god_name;
            }
            else
            {
                text << "Should you decide that abandoning " << old_god_name
                     << "wasn't such a smart move after all, and you'd like to "
                        "return to your old faith, you'll have to find an "
                        "altar dedicated to " << old_god_name << " where";
            }
            text << " you can re-convert, and all will be well. Otherwise "
                    "you'll have to weather this god's displeasure until all "
                    "divine wrath is spent.";

        }
        else
        {
            bool angry = false;
            if (is_good_god(old_god))
            {
                if (is_good_god(new_god))
                {
                    text << "Fortunately, it seems that " << old_god_name <<
                            " didn't mind your converting to " << new_god_name
                         << ". ";

                    if (old_piety > 30)
                        text << "You even kept some of your piety! ";

                    text << "Note that this kind of alliance only exists "
                            "between the three good gods, so don't expect this "
                            "to be the norm.";
                }
                else if (!god_hates_your_god(old_god))
                {
                    text << "Fortunately, it seems that " << old_god_name <<
                            " didn't mind your converting to " << new_god_name
                         << ". That's because " << old_god_name << " is one of "
                            "the good gods who generally are rather forgiving "
                            "about change of faith - unless you switch over to "
                            "the path of evil, in which case their retribution "
                            "can be nasty indeed!";
                }
                else
                {
                    text << "Looks like " << old_god_name << " didn't "
                            "appreciate your converting to " << new_god_name
                         << "! But really, changing from one of the good gods "
                            "to an evil one, what did you expect!? For any god "
                            "not on the opposing side of the faith, "
                         << old_god_name << " would have been much more "
                            "forgiving. ";

                    angry = true;
                }
            }
            else
            {
                text << "Looks like " << old_god_name << " didn't appreciate "
                        "your converting to " << new_god_name << "! (Actually, "
                        "only the three good gods will sometimes be forgiving "
                        "about this kind of faithlessness.) ";

                angry = true;
            }

            if (angry)
            {
                text << "Unfortunately, while converting back would appease "
                     << old_god_name << ", it would annoy " << new_god_name
                     << ", so you're stuck with having to suffer the wrath of "
                        "one god or another.";
            }
        }

        break;
    }

    case HINT_WIELD_WEAPON:
    {
        int wpn = you.equip[EQ_WEAPON];
        if (wpn != -1
            && you.inv[wpn].base_type == OBJ_WEAPONS
            && you.inv[wpn].cursed())
        {
            // Don't trigger if the wielded weapon is cursed.
            Hints.hints_events[seen_what] = true;
            return;
        }

        if (Hints.hints_type == HINT_RANGER_CHAR && wpn != -1
            && you.inv[wpn].base_type == OBJ_WEAPONS
            && you.inv[wpn].sub_type == WPN_BOW)
        {
            text << "You can easily switch between weapons in slots a and "
                    "b by pressing <w>%</w>.";
            cmd.push_back(CMD_WEAPON_SWAP);
        }
        else
        {
            text << "You can easily switch back to your weapon in slot a by "
                    "pressing <w>%</w>. To change the slot of an item, type "
                    "<w>%i</w> and choose the appropriate slots.";
            cmd.push_back(CMD_WEAPON_SWAP);
            cmd.push_back(CMD_ADJUST_INVENTORY);
        }
        break;
    }
    case HINT_FLEEING_MONSTER:
        if (Hints.hints_type != HINT_BERSERK_CHAR)
            return;

        text << "Now that monster is scared of you! Note that you do not "
                "absolutely have to follow it. Rather, you can let it run "
                "away. Sometimes, though, it can be useful to attack a "
                "fleeing creature by throwing something after it. If you "
                "have any darts or stones in your <w>%</w>nventory, you can "
                "look at one of them to read an explanation of how to do this.";
        cmd.push_back(CMD_DISPLAY_INVENTORY);
        break;

    case HINT_MONSTER_BRAND:
#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        if (const monster* m = monster_at(gc))
            tiles.add_text_tag(TAG_TUTORIAL, m->name(DESC_A), gc);
#endif
        text << "That monster looks a bit unusual. You might wish to examine "
                "it a bit more closely by "
#ifdef USE_TILE
                "hovering your mouse over its tile.";
#else
                "pressing <w>%</w> and moving the cursor onto its square.";
        cmd.push_back(CMD_LOOK_AROUND);
#endif
        break;

    case HINT_MONSTER_FRIENDLY:
    {
        const monster* m = monster_at(gc);

        if (!m)
            DELAY_EVENT;

#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, m->name(DESC_A), gc);
#endif
        text << "That monster is friendly to you and will attack your "
                "enemies, though you'll get only part of the experience for "
                "monsters damaged by allies, compared to what you'd get for "
                "doing all the work yourself. You can command your allies by "
                "pressing <w>%</w> to talk to them.";
        cmd.push_back(CMD_SHOUT);

        if (!mons_att_wont_attack(m->attitude))
        {
            text << "\nHowever, it is only <w>temporarily</w> friendly, and "
                    "will become dangerous again when this friendliness "
                    "wears off.";
        }
        break;
    }

    case HINT_MONSTER_SHOUT:
    {
        const monster* m = monster_at(gc);

        if (!m)
            DELAY_EVENT;

        // "Shouts" from zero experience monsters are boring, ignore
        // them.
        if (mons_class_flag(m->type, M_NO_EXP_GAIN))
        {
            Hints.hints_events[HINT_MONSTER_SHOUT] = true;
            return;
        }

        const bool vis = you.can_see(m);

#ifdef USE_TILE
        if (vis)
        {
            tiles.place_cursor(CURSOR_TUTORIAL, gc);
            tiles.add_text_tag(TAG_TUTORIAL, m->name(DESC_A), gc);
        }
#endif
        if (!vis)
        {
            text << "Uh-oh, some monster noticed you, either one that's "
                    "around a corner or one that's invisible. Plus, the "
                    "noise it made will alert other monsters in the "
                    "vicinity, who will come to check out what the commotion "
                    "was about.";
        }
        else if (mons_shouts(m->type, false) == S_SILENT)
        {
            text << "Uh-oh, that monster noticed you! Fortunately, it "
                    "didn't make any noise, but many monsters do make "
                    "noise when they notice you, which alerts other monsters "
                    "in the area, who will come to check out what the "
                    "commotion was about.";
        }
        else
        {
            text << "Uh-oh, that monster noticed you! Plus, the "
                    "noise it made will alert other monsters in the "
                    "vicinity, who will come to check out what the commotion "
                    "was about.";
        }
        break;
    }

    case HINT_MONSTER_LEFT_LOS:
    {
        const monster* m = monster_at(gc);

        if (!m || !you.can_see(m))
            DELAY_EVENT;

        text << m->name(DESC_THE, true) << " didn't vanish, but merely "
                "moved onto a square which you can't currently see. It's still "
                "nearby, unless something happens to it in the short amount of "
                "time it's out of sight.";
        break;
    }

    case HINT_SEEN_MONSTER:
    case HINT_SEEN_FIRST_OBJECT:
        // Handled in special functions.
        break;

    case HINT_SEEN_TOADSTOOL:
    {
        const monster* m = monster_at(gc);

        if (!m || !you.can_see(m))
            DELAY_EVENT;

        text << "Sometimes toadstools will grow on decaying corpses, and "
                "will wither away soon after appearing. Worshippers of "
                "Fedhas Madash, the plant god, can make use of them, "
                "but to everyone else they're just ugly dungeon decoration.";
        break;
    }

    case HINT_SEEN_ZERO_EXP_MON:
    {
        const monster_info* mi = env.map_knowledge(gc).monsterinfo();

        if (!mi)
            DELAY_EVENT;

        text << "That ";
#ifdef USE_TILE
        // need to highlight monster
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, *mi);

        text << "is a ";
#else
        text << glyph_to_tagstr(get_mons_glyph(*mi)) << " is a ";
#endif
        text << mi->proper_name(DESC_PLAIN).c_str() << ". ";

        text << "While <w>technically</w> a monster, it's more like "
                "dungeon furniture, since it's harmless and doesn't move. "
                "If it's in your way you can attack and kill it like other "
                "monsters, but you won't get any experience for doing so. ";
        break;
    }

    case HINT_ABYSS:
        text << "Uh-oh, you've wound up in the Abyss! The Abyss is a special "
                "place where you cannot remember or map where you've been; it "
                "is filled with nasty monsters, and you're probably going to "
                "die.\n";
        text << "To increase your chances of survival until you can find the "
                "exit"
#ifndef USE_TILE
                " (a flickering <w>"
             << stringize_glyph(get_feat_symbol(DNGN_EXIT_ABYSS))
             << "</w>)"
#endif
                ", keep moving, don't fight any of the monsters, and don't "
                "bother picking up any items on the ground. If you're "
                "encumbered or overburdened, then lighten up your load, and if "
                "the monsters are closing in, try to use items of speed to get "
                "away.";
        break;

    case HINT_SPELL_MISCAST:
    {
        // Don't give at the beginning of your spellcasting career.
        if (you.max_magic_points <= 2)
            DELAY_EVENT;

        if (!crawl_state.game_is_hints())
        {
            text << "Miscasting a spell can have nasty consequences, "
                    "particularly for the more difficult spells. Your chance "
                    "of successfully casting a spell increases with your magic "
                    "skills, and can also be improved with the help of some "
                    "items. Use the <w>%</w> command "
#ifdef USE_TILE
                    "or mouse over the spell tiles "
#endif
                    "to check your current success rates.";
            cmd.push_back(CMD_DISPLAY_SPELLS);
            break;
        }
        text << "You just miscast a spell. ";

        const item_def *shield = you.slot_item(EQ_SHIELD, false);
        if (!player_effectively_in_light_armour() || shield)
        {
            text << "Wearing heavy body armour or using a shield, especially a "
                    "large one, can severely hamper your spellcasting "
                    "abilities. You can check the effect of this by comparing "
                    "the success rates on the <w>%\?</w> screen with and "
                    "without the item being worn.\n\n";
            cmd.push_back(CMD_CAST_SPELL);
        }

        text << "If the spellcasting success chance is high (which can be "
                "checked by entering <w>%\?</w> or <w>%</w>) then a miscast "
                "merely means the spell is not working, along with a harmless "
                "side effect. "
                "However, for spells with a low success rate, there's a chance "
                "of contaminating yourself with magical energy, plus a chance "
                "of an additional harmful side effect. Normally this isn't a "
                "problem, since magical contamination bleeds off over time, "
                "but if you're repeatedly contaminated in a short amount of "
                "time you'll mutate or suffer from other ill side effects.\n\n";
        cmd.push_back(CMD_CAST_SPELL);
        cmd.push_back(CMD_DISPLAY_SPELLS);

        text << "Note that a miscast spell will still consume the full amount "
                "of MP and nutrition that a successfully cast spell would.";
        break;
    }
    case HINT_SPELL_HUNGER:
        text << "The spell you just cast made you hungrier; you can see how "
                "hungry spells make you by "
#ifdef USE_TILE
                "examining your spells in the spell display, or by "
#endif
                "entering <w>%\?!</w> or <w>%I</w>. "
                "The amount of nutrition consumed increases with the level of "
                "the spell and decreases depending on your intelligence stat "
                "and your Spellcasting skill. If both of these are high "
                "enough a spell might even not cost you any nutrition at all.";
        cmd.push_back(CMD_CAST_SPELL);
        cmd.push_back(CMD_DISPLAY_SPELLS);
        break;

    case HINT_GLOWING:
        text << "You've accumulated so much magical contamination that you're "
                "glowing! You usually acquire magical contamination from using "
                "some powerful magics, like invisibility, haste or potions of "
                "resistance. ";

        if (get_contamination_level() < 2)
        {
            text << "As long as the status only shows in grey nothing will "
                    "actually happen as a result of it, but as you continue "
                    "suffusing yourself with magical contamination you'll "
                    "eventually start glowing for real, which ";
        }
        else
        {
            text << "This normally isn't a problem as contamination slowly "
                    "bleeds off on its own, but it seems that you've "
                    "accumulated so much contamination over a short amount of "
                    "time that it ";
        }
        text << "can have nasty effects, such as mutating you or dealing direct "
                "damage. In addition, glowing is going to make you much more "
                "noticeable.";
        break;

    case HINT_YOU_RESIST:
        text << "There are many dangers in Crawl. Luckily, there are ways to "
                "(at least partially) resist some of them, if you are "
                "fortunate enough to find them. There are two basic variants "
                "of resistances: the innate magic resistance that depends on "
                "your species, grows with the experience level, and protects "
                "against hostile enchantments; and the specific resistances "
                "against certain types of magic and also other effects, e.g. "
                "fire or draining.\n"
                "You can find items in the dungeon or gain mutations that will "
                "increase (or lower) one or more of your resistances. To view "
                "your current set of resistances, "
#ifdef USE_TILE
                "<w>right-click</w> on the player avatar.";
#else
                "press <w>%</w>.";
        cmd.push_back(CMD_RESISTS_SCREEN);
#endif
        break;

    case HINT_CAUGHT_IN_NET:
        text << "While you are held in a net, you cannot move around or engage "
                "monsters in combat. Instead, any movement you take is counted "
                "as an attempt to struggle free from the net. With a wielded "
                "bladed weapon you will be able to cut the net faster";

        if (Hints.hints_type == HINT_BERSERK_CHAR)
            text << ", especially if you're berserking while doing so";

        text << ". Small species may also wriggle out of a net, only damaging "
                "it a bit, so as to then <w>%</w>ire it at a monster.";
        cmd.push_back(CMD_FIRE);

        if (Hints.hints_type == HINT_MAGIC_CHAR)
        {
            text << " Note that casting spells is still very much possible, "
                    "as is using wands, scrolls and potions.";
        }
        else
        {
            text << " Note that using wands, scrolls and potions is still "
                    "very much possible.";
        }
        break;

    case HINT_YOU_SILENCE:
        redraw_screen();
        text << "While you are silenced, you cannot cast spells, read scrolls "
                "or use divine invocations. The same is true for any monster "
                "within the effect radius. The field of silence (recognizable "
                "by "
#ifdef USE_TILE
                "the special-looking frame tiles"
#else
                "different-coloured floor squares"
#endif
                ") is always centered on you and will move along with you. "
                "The radius will gradually shrink, eventually making you the "
                "only one affected, before the effect fades entirely.";
        break;

    case HINT_LOAD_SAVED_GAME:
    {
        text << "Welcome back! If it's been a while since you last played this "
                "character, you should take some time to refresh your memory "
                "of your character's progress. It is recommended to at least "
                "have a look through your <w>%</w>nventory, but you should "
                "also check ";
        cmd.push_back(CMD_DISPLAY_INVENTORY);

        vector<string> listed;
        if (you.spell_no > 0)
        {
            listed.push_back("your spells (<w>%?</w>)");
            cmd.push_back(CMD_CAST_SPELL);
        }
        if (!your_talents(false).empty())
        {
            listed.push_back("your <w>%</w>bilities");
            cmd.push_back(CMD_USE_ABILITY);
        }
        if (Hints.hints_type != HINT_MAGIC_CHAR || how_mutated())
        {
            listed.push_back("your set of mutations (<w>%</w>)");
            cmd.push_back(CMD_DISPLAY_MUTATIONS);
        }
        if (you.religion != GOD_NO_GOD)
        {
            listed.push_back("your religious standing (<w>%</w>)");
            cmd.push_back(CMD_DISPLAY_RELIGION);
        }

        listed.push_back("the message history (<w>%</w>)");
        listed.push_back("the character overview screen (<w>%</w>)");
        listed.push_back("the dungeon overview screen (<w>%</w>)");
        text << comma_separated_line(listed.begin(), listed.end()) << ".";
        cmd.push_back(CMD_REPLAY_MESSAGES);
        cmd.push_back(CMD_RESISTS_SCREEN);
        cmd.push_back(CMD_DISPLAY_OVERMAP);

        text << "\nAlternatively, you can dump all information pertaining to "
                "your character into a text file with the <w>%</w> command. "
                "You can then find said file in the <w>morgue/</w> directory (<w>"
             << you.your_name << ".txt</w>) and read it at your leisure. Also, "
                "such a file will automatically be created upon death (the "
                "filename will then also contain the date) but that won't be "
                "of much use to you now.";
        cmd.push_back(CMD_CHARACTER_DUMP);
        break;
    }
    case HINT_AUTOPICKUP_THROWN:
        text << "When stepping on items you've thrown, they will be "
                "picked up automatically.";
        break;
    case HINT_GAINED_SPELLCASTING:
        text << "As your Spellcasting skill increases, you will be able to "
             << "memorise more spells, and will suffer less hunger and "
             << "somewhat fewer failures when you cast them.\n"
             << "Press <w>%</w> "
#ifdef USE_TILE
             << "(or click on the <w>skill button</w> in the command panel) "
#endif
             << "to have a look at your skills and manage their training.";
        cmd.push_back(CMD_DISPLAY_SKILLS);
        break;
#if TAG_MAJOR_VERSION == 34
    case HINT_MEMORISE_FAILURE:
        text << "At low skills, spells may be difficult to learn or cast. "
                "For now, just keep trying!";
        break;
#endif
    case HINT_FUMBLING_SHALLOW_WATER:
        text << "Fighting in shallow water will sometimes cause you to slip "
                "and fumble your attack. If possible, try to fight on "
                "firm ground.";
        break;
    case HINT_CLOUD_WARNING:
        text << "Rather than step into this cloud and hurt yourself, you should "
                "try to step around it or wait it out with <w>%</w> or <w>%</w>.";
        cmd.push_back(CMD_MOVE_NOWHERE);
        cmd.push_back(CMD_REST);
        break;
    case HINT_ANIMATE_CORPSE_SKELETON:
        text << "As long as a monster has a skeleton, Animate Skeleton also "
                "works on unskeletalized corpses.";
        break;
    default:
        text << "You've found something new (but I don't know what)!";
    }

    if (!text.str().empty())
    {
        string output = text.str();
        if (!cmd.empty())
            insert_commands(output, cmd);
        mpr(output, MSGCH_TUTORIAL);

        stop_running();
    }
}

formatted_string hints_abilities_info()
{
    ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = "이 창은 당신의 특수 능력들을 보여줍니다. "
        "당신은 어떤 아이템이나 종교, 돌연변이를 통해 새로운 능력을 "
        "얻을 수 있습니다. 이러한 능력들의 사용에는 보통 대가가 필요합니다. "
        "예를 들자면, 만복도 또는 마나가 소모되죠. '<w>!</w>' 또는 '<w>?</w>' 키를  "
        "눌러서 특수 능력을 사용하거나 그 설명을 볼 수 있습니다.";
    linebreak_string(broken, _get_hints_cols());
    text << broken;

    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return formatted_string::parse_string(text.str(), false);
}

// Explains the basics of the skill screen. Don't bother the player with the
// aptitude information.
string hints_skills_info()
{
    textcolor(channel_to_colour(MSGCH_TUTORIAL));
    ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = "이 창은 당신의 스킬 목록을 보여줍니다. "
        "능력 옆에 있는 숫자는 현재 스킬의 레벨이며, "
        "높을수록 좋습니다. <brown>갈색 퍼센트 수치</brows>는 "
        "그 스킬을 기르기 위해 할당된 경험치를 나타냅니다. "
        "원하는 스킬을 숙련하려면 "
		"해당하는 문자를 누르세요. <darkgrey>회색의</darkgrey> 스킬은 "
        "숙련도를 높일 수 없으며 다른 스킬의 숙련도로 경험치가 분배됩니다. "
        "<w>!</w> 키를 눌러 숙련도에 대해 배울 수 있고 <w>?</w>키를 눌러"
        "당신의 스킬에 대한 설명을 볼 수 있습니다.";
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return text.str();
}

string hints_skill_training_info()
{
    textcolor(channel_to_colour(MSGCH_TUTORIAL));
    ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = "숙련 백분율(in <brown>brown</brown>)은 "
        "스킬을 숙련할 때 사용될 획득 경험치의 상대적인 "
        "수치를 보여줍니다. 이것은 당신이 최근에 어떤 스킬을"
        "사용했는지에 따라 자동으로 정해집니다. 스킬 잠금은 숙련도를 "
        "0으로 조정합니다.";
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return text.str();
}

string hints_skills_description_info()
{
    textcolor(channel_to_colour(MSGCH_TUTORIAL));
    ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = "이 창은 당신의 스킬 목록를 보여줍니다. "
                    "스킬의 글자를 누르면 그것에 대한 설명을 볼 수 있으며, "
                    "다시 <w>?</w>키를 누르면 스킬 선택창으로 돌아갈 수 있습니다.";

    linebreak_string(broken, _get_hints_cols());
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return text.str();
}

// A short explanation of Crawl's target mode and its most important commands.
static string _hints_target_mode(bool spells = false)
{
    string result;
    result = "가장 가까운 몬스터 또는 이전 표적이 "
             "타겟으로 설정됩니다. 또한 당신은 시야 내의 모든 적대적인 몬스터들을 "
             "<w>+</w> 또는 <w>-</w>로 순차 표적 설정할 수 있습니다. "
             "올바른 대상을 겨누었다면, "
             "<w>f</w>, <w>Enter</w> 또는 <w>.</w>키를 눌러 발사합시다. "
             "만약 공격이 빗나가면, <w>";

    command_type cmd;
    if (spells)
    {
        result += "%ap";
        cmd = CMD_CAST_SPELL;
    }
    else
    {
        result += "%f";
        cmd = CMD_FIRE;
    }

    result += "</w>(으)로 동일 대상에게 다시 발사합니다.";
    insert_commands(result, cmd, 0);

    return result;
}

static string _hints_abilities(const item_def& item)
{
    string str = "그러기 위해서는, ";

    vector<command_type> cmd;
    if (!item_is_equipped(item))
    {
        switch (item.base_type)
        {
        case OBJ_WEAPONS:
            str += "먼저 그것을 장착(<w>%</w>)하세요";
            cmd.push_back(CMD_WIELD_WEAPON);
            break;
        case OBJ_ARMOUR:
            str += "먼저 그것을 장비(<w>%</w>)하세요";
            cmd.push_back(CMD_WEAR_ARMOUR);
            break;
        case OBJ_JEWELLERY:
            str += "먼저 그것을 착용(<w>%</w>)하세요";
            cmd.push_back(CMD_WEAR_JEWELLERY);
            break;
        default:
            str += "<r>(BUG! this item shouldn't give an ability)</r>";
            break;
        }
        str += ". 그러면, ";
    }
    str += "<w>%</w>키로 능력 메뉴에 들어갑니다. "
           "알맞은 능력을 고르세요. 특히 숙련되지 않은 능력으로 "
           "스킬을 발동시키는 것은 실패할 가능성이 큽니다.";
    cmd.push_back(CMD_USE_ABILITY);

    insert_commands(str, cmd);
    return str;
}

static string _hints_throw_stuff(const item_def &item)
{
    string result;

    result  = "그러기 위해서는, <w>%</w>키를 입력하여 발사하세요, <w>";
    result += item.slot;
    result += "</w>키를 눌러 ";
    result += (item.quantity > 1 ? "이 " : "이 ");
    result += " ";
    result += item_base_name(item);
    result += (item.quantity > 1? "들" : "");
    result += "(으)로, ";
    result += _hints_target_mode();

    insert_commands(result, CMD_FIRE, 0);
    return result;
}

// num_old_talents describes the number of activatable abilities you had
// before putting on this item.
void check_item_hint(const item_def &item, unsigned int num_old_talents)
{
    if (item.cursed())
        learned_something_new(HINT_YOU_CURSED);
    else if (Hints.hints_events[HINT_NEW_ABILITY_ITEM]
             && your_talents(false).size() > num_old_talents)
    {
        learned_something_new(HINT_NEW_ABILITY_ITEM);
    }
    else if (Hints.hints_events[HINT_ITEM_RESISTANCES]
             && gives_resistance(item))
    {
        learned_something_new(HINT_ITEM_RESISTANCES);
    }
}

// Explains the most important commands necessary to use an item, and mentions
// special effects, etc.
// NOTE: For identified artefacts don't give all this information!
//       (The screen is likely to overflow.) Artefacts need special information
//       if they are evokable or grant resistances.
//       In any case, check whether we still have enough space for the
//       inscription prompt and answer.
void hints_describe_item(const item_def &item)
{
    ostringstream ostr;
    ostr << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    vector<command_type> cmd;

    switch (item.base_type)
    {
        case OBJ_WEAPONS:
        {
            if (is_artefact(item) && item_type_known(item))
            {
                if (gives_ability(item)
                    && wherey() <= get_number_of_lines() - 5)
                {
                    // You can activate it.
                    ostr << "어떤 무기들은, 착용할 때 (이것과 같은) "
                            "특수 능력을 제공하기도 합니다. ";
                    ostr << _hints_abilities(item);
                    break;
                }
                else if (gives_resistance(item)
                         && wherey() <= get_number_of_lines() - 3)
                {
                    // It grants a resistance.
                    ostr << "\n이 무기는 착용자에게 어느 근원에서 오는 방어 효과를"
                            "제공합니다. 당신의 저항력을 "
                            "살펴보려면 (다른 것들 사이의) <w>%</w>을 입력"
#ifdef USE_TILE
                            "하시거나, 당신의 캐릭터를 마우스 <w>오른쪽 버튼으로 클릭"
#endif
                            "하세요.";
                    cmd.push_back(CMD_RESISTS_SCREEN);
                    break;
                }
                return;
            }

            item_def *weap = you.slot_item(EQ_WEAPON, false);
            bool wielded = (weap && (*weap).slot == item.slot);
            bool long_text = false;

            if (!wielded)
            {
                ostr << "당신은 이 무기를 <w>%</w>키로 장착할 수 있고 또는 "
                        "<w>%</w>키를 사용하여 a와 b 슬롯에 있는 무기로"
                        "손쉽게 바꿀 수 있습니다. (<w>%i</w>를 사용하여 아이템 슬롯을 조정할 수 있습니다.)";
                cmd.push_back(CMD_WIELD_WEAPON);
                cmd.push_back(CMD_WEAPON_SWAP);
                cmd.push_back(CMD_ADJUST_INVENTORY);

                // Weapon skill used by this weapon and the best weapon skill.
                skill_type curr_wpskill, best_wpskill;

                // Maybe this is a launching weapon?
                if (is_range_weapon(item))
                {
                    // Then only compare with other launcher skills.
                    curr_wpskill = range_skill(item);
                    best_wpskill = best_skill(SK_SLINGS, SK_THROWING);
                }
                else
                {
                    // Compare with other melee weapons.
                    curr_wpskill = weapon_skill(item);
                    best_wpskill = best_skill(SK_SHORT_BLADES, SK_STAVES);
                    // Maybe unarmed is better.
                    if (you.skills[SK_UNARMED_COMBAT] > you.skills[best_wpskill])
                        best_wpskill = SK_UNARMED_COMBAT;
                }

                if (you.skills[curr_wpskill] + 2 < you.skills[best_wpskill])
                {
                    ostr << "\n다시 한 번 보자면, 당신은 <w>"
                         << skill_name(best_wpskill)
                         << "</w>을(를) 수련하고 있습니다. 이제 당분간은, "
                            "<w>"
                         << skill_name(curr_wpskill)
                         << "</w>을(를) 수련하는 것이 현명할 것입니다. (<w>%</w>을 입력하여"
                            " 스킬 관리창을 실제 수치로 볼 수 있습니다.)";

                    cmd.push_back(CMD_DISPLAY_SKILLS);
                    long_text = true;
                }
            }
            else // wielded weapon
            {
                if (is_range_weapon(item))
                {
                    ostr << "몬스터를 공격하려면, ";
#ifdef USE_TILE
                    ostr << "그리고 만약 당신이 적당한 발사체를 갖고 있다면"
                            "당신은 몬스터를 <w>마우스 좌클릭</w>할 수 있습니다. "
							"(<w>쉬프트키</w>를 누른 채로) 다른 방법으로는, "
                            "당신은 곳을 <w>마우스 좌클릭</w>하여"
                            "발사할수도 있고, 또 몬스터에 <w>아우스 "
                            "좌클릭</w>하여 발사하는 것도 가능합니다.\n\n";
                    ostr << "탄약을 발사하려면 키보드를 이용하세요. ";
#endif
                    ostr << "당신은 오직 "
					     	"발사 도구에 맞는 발사체만을 발사(<w>%</w>)할 수 있습니다. "
                            "그러면, ";
                    ostr << _hints_target_mode();
                    cmd.push_back(CMD_FIRE);
                }
                else
                    ostr << "몬스터를 공격하려면, 몬스터를 향해 걸어가기만 하면 됩니다.";
            }

            if (is_throwable(&you, item) && !long_text)
            {
                ostr << "\n\n어떤 무기들은 (이것도 포함), "
                        "발사(<w>%</w>)하는 것이 가능합니다. ";
                cmd.push_back(CMD_FIRE);
                ostr << _hints_throw_stuff(item);
                long_text = true;
            }
            if (!item_type_known(item)
                && (is_artefact(item)
                    || get_equip_desc(item) != ISFLAG_NO_DESC))
            {
                ostr << "\n\n이것처럼 특이한 설명을 가진 무기와 방어구들은 "
                     << "더 높은 수준의 강화가 이루어질 수 있으며 "
                     << "또는 좋든 나쁘든 특별한 속성을 갖고 있을 수 있습니다.";

                Hints.hints_events[HINT_SEEN_RANDART] = false;
            }
            if (item_known_cursed(item) && !long_text)
            {
                ostr << "\n\n일단 착용하면, 저주받은 무기는 해제할 수 없으며 "
                        "강화 마법 두루마리나 저주 해제 마법 두루마리를 사용하여 "
                        "저주를 풀어주어야 "
                        "장비 해제가 가능합니다. ";

                if (!wielded && is_throwable(&you, item))
                    ostr << " (이것을 던지는 건 안전합니다, 그래도.)";

                Hints.hints_events[HINT_YOU_CURSED] = false;
            }
            Hints.hints_events[HINT_SEEN_WEAPON] = false;
            break;
        }
        case OBJ_MISSILES:
            if (is_throwable(&you, item))
            {
                ostr << item.name(true, DESC_YOUR)
                     << "은(는) 발사도구가 없어도 발사(<w>%</w>)할 수 있습니다. ";
                ostr << _hints_throw_stuff(item);
                cmd.push_back(CMD_FIRE);
            }
            else if (is_launched(&you, you.weapon(), item))
            {
                ostr << "당신은 이미 적당한 발사도구를 착용하고 있어서, "
                        "그저";
#ifdef USE_TILE
                ostr << "공격하고 싶은 몬스터를 <w>왼쪽 마우스 버튼 클릭</w>하기만 하면 됩니다. "
                        "<w>쉬프트키</w>를 누른 상태에서. "
                        "대신에, 당신은 <w>마우스 왼쪽 클릭</w>해서 "
                        "발사하고 싶은 장소를 골라, "
                        "<w>마우스 왼쪽 클릭</w> 하여 몬스터를"
                        "공격할 수 있습니다.\n\n"

                        "키보드를 사용하여 탄약을 몬스터에게 발사하려면 "
                        "간단히";
#endif

                ostr << "발사 키(<w>%</w>)을 (눌러) "
                     << (item.quantity > 1 ? "이 " : "이 ")
                     << " " << item.name(true, DESC_BASENAME)
                     << (item.quantity > 1? "을(를) " : "을(를) ")
                     << "발사할 수 있습니다. 그럼 ";
                ostr << _hints_target_mode();
                cmd.push_back(CMD_FIRE);
            }
            else
            {
                ostr << " "
                     << (item.quantity > 1 ? "이 " : "이 ")
                     << " " << item.name(true, DESC_BASENAME)
                     << (item.quantity > 1? "을(를) " : "을(를) ")
                     << "발사하기 위해서는, 당신은 적절한 발사체를 착용(<w>%</w>) "
                        "해야 합니다.";
                cmd.push_back(CMD_WIELD_WEAPON);
            }
            Hints.hints_events[HINT_SEEN_MISSILES] = false;
            break;

        case OBJ_ARMOUR:
        {
            bool wearable = true;
            if (you.species == SP_CENTAUR && item.sub_type == ARM_BOOTS)
            {
                ostr << "켄타우로스인 당신은 신발을 신을 수 없습니다. "
                        "(<w>%</w>를 입력하여 당신의 돌연변이와 고유 능력을 "
                        "볼 수 있습니다.)";
                cmd.push_back(CMD_DISPLAY_MUTATIONS);
                wearable = false;
            }
            else if (you.species == SP_MINOTAUR && is_hard_helmet(item))
            {
                ostr << "미노타우루스인 당신은 투구를 착용할 수 없습니다. "
                        "(<w>%</w> 를 입력하여 당신의 돌연변이와 고유 능력을"
                        "볼 수 있습니다.)";
                cmd.push_back(CMD_DISPLAY_MUTATIONS);
                wearable = false;
            }
            else if (item.sub_type == ARM_CENTAUR_BARDING)
            {
                ostr << "켄타우로스만이 켄타우로스 마갑을 입을 수 있습니다.";
                wearable = false;
            }
            else if (item.sub_type == ARM_NAGA_BARDING)
            {
                ostr << "나가만이 나가 꼬리갑옷을 입을 수 있습니다.";
                wearable = false;
            }
            else
            {
                ostr << "당신은 <w>%</w>으로 부위별 방어구를 입을 수 있고"
                        "<w>%</w>으로 그것들을 다시 벗을 수 있습니다"
#ifdef USE_TILE
                        ", 또는, 대신에, 그것들의 아이콘을 클릭하는 것만으로"
                        "이러한 행동을 수행할 수 있습니다."
#endif
                        ".";
                cmd.push_back(CMD_WEAR_ARMOUR);
                cmd.push_back(CMD_REMOVE_ARMOUR);
            }

            if (Hints.hints_type == HINT_MAGIC_CHAR
                && get_armour_slot(item) == EQ_BODY_ARMOUR
                && !is_effectively_light_armour(&item))
            {
                ostr << "\n회피율 감소 수치가 큰 방어구들은"
                        "당신이 주문을 배우거나 외우는 것을 방해할 수 있습니다. "
                        "로브나, 가죽 갑옷, 엘븐제 갑옷과 같은 경갑은"
                        "대개 주문 시전술사 지망자들이 "
                        "큰 무리 없이 주문을 외우도록 해 줄 것입니다.";
            }
            else if (Hints.hints_type == HINT_MAGIC_CHAR
                     && is_shield(item))
            {
                ostr << "\n방패는 당신의 주문 시전을 방해할 것이며 "
                        "방패가 크고 무거울수록 "
                        "페널티도 커진다는 것을 알아두세요.";
            }
            else if (Hints.hints_type == HINT_RANGER_CHAR
                     && is_shield(item))
            {
                ostr << "\n방패 착용은 당신이 활을 쏘는 속도를 "
                        "대폭 감소시킨다는 것을 알아두세요.";
            }

            if (!item_type_known(item)
                && (is_artefact(item)
                    || get_equip_desc(item) != ISFLAG_NO_DESC))
            {
                ostr << "\n\n이것처럼 특이한 설명을 가진 무기와 방어구들은 "
                     << "더 높은 수준의 강화가 이루어질 수 있으며, "
                     << "또는 좋든 나쁘든 특별한 속성을 갖고 있을 수 있습니다.";

                Hints.hints_events[HINT_SEEN_RANDART] = false;
            }
            if (wearable)
            {
                if (item_known_cursed(item))
                {
                    ostr << "\n저주받은 방어구는 한 번 착용하면 "
                            "해제할 수 없으며, 저주 해제스크롤이나 "
                            "방어구 강화 스크롤을 사용하여 저주를 해제해야 "
                            "방어구를 해제할 수 있습니다.";
                }
                if (gives_resistance(item))
                {
                    ostr << "\n\n이 방어구는 착용자에게 원소 공격에 대한 방어 효과를 "
                            "제공합니다. 당신의 저항력을 살펴보려면, "
                            "<w>%</w>키를 입력하세요"
#ifdef USE_TILE
                            "또는 당신의 캐릭터를<w>마우스 오른쪽"
                            "버튼</w>으로 클릭하셔도 됩니다."
#endif
                            ".";
                    cmd.push_back(CMD_RESISTS_SCREEN);
                }
                if (gives_ability(item))
                {
                    ostr << "\n\n이 것과 같은 어떠한 방어구들은 "
						"<w>%</w>키를 눌러 당신이 사용할 수 있는 능력을 "
                            "부여합니다. ";
                    ostr << _hints_abilities(item);
                    cmd.push_back(CMD_USE_ABILITY);
                }
            }
            Hints.hints_events[HINT_SEEN_ARMOUR] = false;
            break;
        }
        case OBJ_WANDS:
            ostr << "(<w>%</w>)키를 눌러, 마법봉에 담겨 있는 마법을 "
                    "사용할 수 있습니다.";
            cmd.push_back(CMD_EVOKE);
#ifdef USE_TILE
            ostr << "또한, 당신은 1) <w>마우스 왼쪽 버튼 클릭</w>하여 "
                    "쉬프트 키를 누른 상태에서 몬스터를 (또는 당신 캐릭터 자신을 "
                    "표적으로 삼거나) 표적으로 삼을 수 있습니다. (<w>";
#ifdef USE_TILE_WEB
            ostr << "컨트롤 + 쉬프트 키";
#else
#if defined(UNIX) && defined(USE_TILE_LOCAL)
            if (!tiles.is_fullscreen())
              ostr << "컨트롤 + 쉬프트 키";
            else
#endif
              ostr << "알트 키";
#endif
            ostr << "</w>) 그리고 메뉴에서 지팡이를 고르거나, 또는 2) "
                    "<w>마우스 왼쪽 버튼 클릭</w>하여 마법봉의 아이콘을 선택하고 "
                    "표적을 <w>마우스 왼쪽 버튼 클릭</w>하세요.";
#endif
            Hints.hints_events[HINT_SEEN_WAND] = false;
            break;

        case OBJ_FOOD:
            if (food_is_rotten(item) && !player_mutation_level(MUT_SAPROVOROUS))
            {
                ostr << "당신은 썩은 고기를 먹을 수 없습니다. "
                        "이 고기는 ";
                if (!in_inventory(item))
                    ostr << "무시하는 게 좋겠군요. ";
                else
                {
                    ostr << "<w>%</w>키를 눌러, 이 고기를 버립시다. <w>%&</w>키를 눌러 당신의"
                            "소지품창에 있는 모든 뼈, 시체, 썩은 고기를"
                            "선택할 수 있습니다. ";
                    cmd.push_back(CMD_DROP);
                    cmd.push_back(CMD_DROP);
                }
            }
            else
            {
                ostr << "또한 음식을 먹으려면(<w>%</w>키), "
#ifdef USE_TILE
                        "음식의 아이콘을 <w>마우스 왼쪽 버튼 클릭</w "
                        "하셔도 됩니다."
#endif
                        ". ";
                cmd.push_back(CMD_EAT);

                if (item.sub_type == FOOD_CHUNK)
                {
                    ostr << "대부분의 종족은 날고기를 먹기 싫어한다는 것을 알아두세요."
                            "정말로 배고프지 않는 이상은 말이죠. ";

                    if (food_is_rotten(item))
                    {
                        ostr << "일부 소수 종족은 썩은 고기를 소화해낼 수 있지만,"
                             "당신은 아마 그러한 부류는 아닐겁니다.";
                    }
                }
            }
            Hints.hints_events[HINT_SEEN_FOOD] = false;
            break;

        case OBJ_SCROLLS:
            ostr << "<w>%</w>키를 입력하여 두루마리를 읽거나"	
#ifdef USE_TILE
                    "<w>마우스 왼쪽 버튼 클릭</w>하셔서 읽으실 수 있습니다"
#endif
                    ".";
            cmd.push_back(CMD_READ);

            Hints.hints_events[HINT_SEEN_SCROLL] = false;
            break;

        case OBJ_JEWELLERY:
        {
            ostr << "장신구는 착용(<w>%</w>)하거나 다시 해제<w>%</w>할 수"
                    "있고"
#ifdef USE_TILE
                    ", 당신의 소지품창에 있는 아이콘을 클릭하는 것으로"
                    "탈착할 수 있습니다"
#endif
                    ".";
            cmd.push_back(CMD_WEAR_JEWELLERY);
            cmd.push_back(CMD_REMOVE_JEWELLERY);

            if (item_known_cursed(item))
            {
                ostr << "\n저주받은 장신구는 불운한 착용자의 목이나 "
                        "손가락에 착 달라붙어서 저주 해제 두루마리를 "
                        "사용할 때까지는 계속 "
                        "달라 붙어있을 겁니다.";
            }
            if (gives_resistance(item))
            {
                    ostr << "\n\n이 "
                         << (item.sub_type < NUM_RINGS ? "반지" : "부적")
                         << "은(는) 착용자에게 원소 공격으로부터의 "
                            "저항력을 제공합니다. 당신의 저항력을"
                            "살펴보려면 <w>%</w>키를 입력하세요."
#ifdef USE_TILE
                            " 혹은 당신의 캐릭터를 <w>마우스 오른쪽"
                            "버튼 클릭</w>하셔도 됩니다"
#endif
                            ".";
                cmd.push_back(CMD_RESISTS_SCREEN);
            }
            if (gives_ability(item))
            {
                ostr << "\n\n이것과 같은, 특정한 종류의 장신구들은 "
                        "<w>%</w>키를 눌러 사용할 수 있는 특수한 능력을 제공합니다. ";
                cmd.push_back(CMD_USE_ABILITY);
                ostr << _hints_abilities(item);
            }
            Hints.hints_events[HINT_SEEN_JEWELLERY] = false;
            break;
        }
        case OBJ_POTIONS:
            ostr << "<w>%</w>키를 입력하여 이 물약을 마실 수 있습니다"
#ifdef USE_TILE
                    ". 또는 <w>마우스 왼쪽 버튼</w>으로 물약을 클릭하기만 해도 되죠"
#endif
                    ".";
            cmd.push_back(CMD_QUAFF);
            Hints.hints_events[HINT_SEEN_POTION] = false;
            break;

        case OBJ_BOOKS:
            if (item.sub_type == BOOK_MANUAL)
            {
                ostr << "스킬 설명서는 당신의 스킬 숙련에 굉장한 도움을 줄 수 있습니다. "
					"사용하려면, 그냥 읽으면(<w>%</w>키) 됩니다. 당신이 설명서를 "
                        "오래 소지할수록, 배우기 어려운 스킬들도 더욱 효율적으로 수련하고"
                        "더 빠르게 스킬의 레벨을 향상시킬 수 있을 것입니다..";
                cmd.push_back(CMD_READ);
            }
            else // It's a spellbook!
            {
                if (you.religion == GOD_TROG
                    && (item.sub_type != BOOK_DESTRUCTION
                        || !item_ident(item, ISFLAG_KNOW_TYPE)))
                {
                    if (!item_ident(item, ISFLAG_KNOW_TYPE))
                    {
                        ostr << "이것은 책입니다. 당신은 이것을 <w>%</w>키를 입력하여 읽을 수 있습니다.";
                        cmd.push_back(CMD_READ);
                    }
                    else
                    {
                        ostr << "이건 마법서였군요! 당신은 몇몇 주문을 기억(<w>%</w>키)하고 "
                                "<w>%</w>키를 눌러 기억한 주문을 시전할 수 있습니다.";
                        cmd.push_back(CMD_MEMORISE_SPELL);
                        cmd.push_back(CMD_CAST_SPELL);
                    }
                    ostr << "\n 당신은"
                         << _(god_name(GOD_TROG).c_str())
                         << "을 숭배하는군요. 당신은 주문을 외우는 대신, 당신의 신이 혐오하는 이러한 마법서들을"
                            "붙태워버리고 싶을지도 모릅니다."
							"(<w>%</w>키)"
                            "이는 바닥에 놓여있는 책에게만 효과가 있으며"
                            "당신이 현재 있는 자리에는 효과가 없다는 것을 알아두세요. ";
                    cmd.push_back(CMD_USE_ABILITY);
                }
                else if (!item_ident(item, ISFLAG_KNOW_TYPE))
                {
                    ostr << "이것은 책입니다. 당신은 이것을 읽을(<w>%</w>키)수 있습니다."
#ifdef USE_TILE
                            "그리고 우측 소지품창의 책 아이콘을 클릭하셔도"
                            "읽는 것이 가능합니다."
#endif
                            ".";
                    cmd.push_back(CMD_READ);
                }
                else if (item.sub_type == BOOK_DESTRUCTION)
                {
                    ostr << "이 마법서에는 강한 위력의 파괴 마법들이 수록되어 있군요."
                            "이러한 마법은 적 뿐만 아니라, 당신이나 동료에게도 큰 피해를 줄 수 있습니다. 조심히 사용하세요!";
                }
                else
                {
                    if (player_can_memorise(item))
                    {
                        ostr << "이러한 <lightblue>강조된 "
							"주문</lightblue>은 당장에라도 기억(<w>%</w>키)할 수"
                                "있습니다. ";
                        cmd.push_back(CMD_MEMORISE_SPELL);
                    }
                    else
                    {
                        ostr << "당신은 현재 어떠한 주문도"
                             << (you.spell_no ? "더 이상 " : "")
                             << "기억할 수 없습니다. 경험치를 쌓아 "
                                "플레이어 레벨과 주문시전 레벨이 올라갈수록, 더 많은 주문을 기억할 수 있습니다.";
                    }

                    if (you.spell_no)
                    {
                        ostr << "\n\n주문을 시전하려면, ";
#ifdef USE_TILE
                        ostr << "당신은 <w>마우스 왼쪽 버튼/w>을 클릭하여"
                                "표적으로 삼을 몬스터를 지정합니다. (또는"
                                "자기 자신을 클릭하여 자신에게 주문을 "
                                "시전할 수 있습니다.) <w>Ctrl"
                                "키</w>를 누르고 있어야합니다. 그런 후, 메뉴에서 주문을 "
                                "고릅니다. 또는 당신은 우측 커맨드 인터페이스에서, "
                                "알맞은 <w>탭</w>을 클릭하여 가지고 있는 주문 아이콘을 클릭한 후,"
                                "해당 주문 아이콘을 클릭하고 대상을 선택하여 주문을 시전할 수도 있습니다."
                                "\n\n또는, ";
#endif
                        ostr << "<w>%</w>키를 눌러 주문을 선택하고 "
                                "시전하실 수 있습니다. 예를 들면 <w>a</w> (<w>?</w>로 확인합니다.)를 눌러"
                                "당신이 시전할 주문을 확인할 수 있죠.";
                        cmd.push_back(CMD_CAST_SPELL);
                        ostr << _hints_target_mode(true);
                    }
                }
            }
            ostr << "\n";
            Hints.hints_events[HINT_SEEN_SPBOOK] = false;
            break;

        case OBJ_CORPSES:
            Hints.hints_events[HINT_SEEN_CARRION] = false;

            if (item.sub_type == CORPSE_SKELETON)
            {
                ostr << "뼈는 특정한 강령술 주문에 필요한 재료로 쓰일 수 있습니다. "
                        "사실 뼈는 강령술 주문에 사용하는 것 외에는 "
                        "거의 다른 용도가 없죠.";

                if (in_inventory(item))
                {
                    ostr << " 아이템 버리기 메뉴에서 당신은"
                            "모든 뼈, 시체, 썩은 고기를"
                            "<w>%&</w>를 눌러 한번에 선택할 수 있습니다.";
                    cmd.push_back(CMD_DROP);
                }
                break;
            }

            ostr << "바닥에 놓인 시체는 <w>%</w>키를 눌러 해체하여, "
                    "고기로 만들 수 있습니다. 시체가 독이 있거나 오염된 시체와는 관계가 없이 해체가 가능하죠";
            cmd.push_back(CMD_BUTCHER);

            if (!food_is_rotten(item) && god_likes_fresh_corpses(you.religion))
			{

				ostr << ". 아니면, 시체를 공양받는 신에게 시체를 제물로 바칠 수 있습니다. 마침 당신이 믿고 있는 신 "
					<< _(god_name(you.religion).c_str())
					<< "은(는) 신선한 시체를 제물로 받는군요, 시체 위에서 기도(<w>%</w>키)하여 그 시체를 제물로 바칠 수 있습니다";
				cmd.push_back(CMD_PRAY);
			}
			ostr << ". ";

            if (food_is_rotten(item))
            {
                ostr << "썩은 시체는 당신에게 아무 쓸모도 없을 것이니, "
                        "무시하고 지나치도록 ";
                if (!in_inventory(item))
                    ostr << "합시다. ";
                else
                {
                    ostr << "합시다. 이미 주운 썩은 시체는 <w>%</w>키를 눌러 버립시다. <w>%&</w>키를 입력하여"
                            "당신의 인벤토리에 있는 모든 뼈, 썩은 고기, 시체를 "
                            "고를 수 있습니다. ";
                    cmd.push_back(CMD_DROP);
                    cmd.push_back(CMD_DROP);
                }
                ostr << "어떠한 신도 그런 썩은 제물은 받아들이지 않을 겁니다.";
            }
            else
            {
#ifdef USE_TILE
                ostr << " 당신의 소지품창에 있는 시체 한 구를 가장 간단하게 "
                        "해체하는 방법은, <w>쉬프트키</w>를 누른 상태에서"
                        "시체 아이콘을 <w>마우스 왼쪽 버튼 클릭</w>하여 떨어뜨린 후,"
                        "바닥에 놓인 시체에 "
                        "같은 명령어를 반복하면 됩니다.";
#endif
            }
            if (!in_inventory(item))
                break;

            ostr << "\n\n만약 당신의 소지품 중 떨어뜨리고 싶은 물건들이 있다면,  "
				"가장 간편한 방법은 아이템 버리기 메뉴(<w>%</w>키)를 이용하는 것입니다."
                    "관련된 사항으로, 바닥에 놓인 시체를"
                    "해체하는 것은 <w>%</w>키를 입력하는 것으로도 가능합니다. ";
            cmd.push_back(CMD_DROP);
            cmd.push_back(CMD_BUTCHER);
            break;

        case OBJ_RODS:
            if (!item_ident(item, ISFLAG_KNOW_TYPE))
            {
                ostr << "\n\n이 막대가 어떤 성능을 보여줄지 알아내려면, 당신은"
                        "이것을 <w>%</w>키로 장착하여 이 막대에 숨겨진 마법을 확인할 수 있습니다."
                        "한 번, 이 막대를 발동("
						"<w>%</w>키)하여, 어떠한 마법들이 들어 있는지 확인해보세요"
#ifdef USE_TILE
                        ". 막대 아이콘을 마우스 왼쪽 버튼으로 클릭하는 것으로도 가능합니다"
#endif
                        ".";
            }
            else
            {
                ostr << "\n\n당신은 이 막대의 마법을"
					"착용(<w>%</w>키)하고, 발동(<w>%</w>키)시켜서 사용할 수 있습니다"
#ifdef USE_TILE
                        ". 막대 아이콘을 마우스 왼쪽 버튼으로 클릭하는 것으로도 가능합니다"
#endif
                        ".";
            }
            cmd.push_back(CMD_WIELD_WEAPON);
            cmd.push_back(CMD_EVOKE_WIELDED);
            Hints.hints_events[HINT_SEEN_ROD] = false;
            break;

        case OBJ_STAVES:
            ostr << "이 지팡이는 당신의 주문 시전 능력을 강화하며,"
                    "특정 마법 학파의 마법을 더욱 강력하게 만들어 주거나, "
                    "어려운 마법을 쉽게 사용할 수 있게 만드는 등의 기능을 합니다";

            if (gives_resistance(item))
            {
                ostr << "이것은 또한 착용자에게 특정한 원소 공격에 대한 "
                        "방어 효과를 제공합니다. 당신의 저항력을 "
                        "확인하려면 <w>%</w>키를 입력하거나"
#ifdef USE_TILE
                        "당신의 캐릭터를 <w>마우스 오른쪽"
                        "버튼</w>으로 클릭하면 됩니다"
#endif
                        ".";

                cmd.push_back(CMD_RESISTS_SCREEN);
            }
            else if (you.religion == GOD_TROG)
            {
                ostr << "\n\n당신은 "
                     << _(god_name(GOD_TROG, false).c_str())
                     << "이(가) 마법 사용을 금지하는 것을 알고 있을 것입니다. "
                        "즉, 이 지팡이는 지금 당신에게는 도움이 되지 못합니다. 때에 따라선 이것을"
                        "버리는(<w>%</w>키)것도 고려해 보세요.";
                cmd.push_back(CMD_DROP);
            }
            Hints.hints_events[HINT_SEEN_STAFF] = false;
            break;

        case OBJ_MISCELLANY:
            if (is_deck(item))
            {
                ostr << "카드 덱은 강력하지만, 위험한"
					"마법아이템입니다. 한 번 착용(<w>%</w>키)하거나 발동(<w>%</w>)시켜 보세요."
#ifdef USE_TILE
                        ", 마우스 아이콘으로 카드 덱을 클릭하는 것도 같은 기능을 합니다."
#endif
                        " 감정의 두루마리를 카드 덱에 사용하면, "
                        "윗 장에 어떠한 카드가 있는지를 알아낼 수 있음과 동시에 "
                        "해당 카드 덱이 어떠한 종류인지를 알 수 있습니다. 이것은 카드를 뽑을 때의 위험을 "
                        "줄이고 카드를 적재적소에 사용하는 데 도움을 주죠. 각각 카드가 어떠한 효과를 가지는지는 "
                        "<w>%/c</w>키를 눌러 카드의 DB를 확인해보세요.";
                cmd.push_back(CMD_WIELD_WEAPON);
                cmd.push_back(CMD_EVOKE_WIELDED);
                cmd.push_back(CMD_DISPLAY_COMMANDS);
            }
            else
            {
                ostr << "다양한 아이템들은 이따금씩 마법의 힘을 품고 있습니다."
                        "이러한 아이템들은 <w>%</w>키를 눌러 발동시켜 사용할 수 있습니다.";
                cmd.push_back(CMD_EVOKE);
            }

            Hints.hints_events[HINT_SEEN_MISC] = false;
            break;

        default:
            return;
    }

    ostr << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = ostr.str();
    if (!cmd.empty())
        insert_commands(broken, cmd);
    linebreak_string(broken, _get_hints_cols());
    cgotoxy(1, wherey() + 2);
    display_tagged_block(broken);
}

void hints_inscription_info(string prompt)
{
    // Don't print anything if there's not enough space.
    if (wherey() >= get_number_of_lines() - 1)
        return;

    ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    bool longtext = false;
    if (wherey() <= get_number_of_lines() - 8)
    {
        text << "\n"
         "아이템에 문장을 새기는 것은 던전 크롤의 특징 중 하나입니다.\n"
         "당신은 아이템에 간단한 글을 새겨 다른 아이템과 구분할 수 있으며, \n"
         "몇몇 특정한 문장을 새기는 것으로, 아이템의 사용이나 다른 상호작용 규칙을 바꾸는 것도 가능합니다. \n"
         "이는 아주 고급 기능이니, 던전 크롤에 아직 익숙하지 않으시다면 무난하게 이 특징을 무시하셔도 지장은 없습니다.";

        longtext = true;
    }

    text << "\n"
       "(주 화면에서, <w>?6</w>키를 눌러 더 많은 정보를 확인해보세요.)\n";
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    formatted_string::parse_string(text.str()).display();

    // Ask a second time, if it's been a longish interruption.
    if (longtext && !prompt.empty() && wherey() <= get_number_of_lines() - 2)
        formatted_string::parse_string(prompt).display();
}

// FIXME: With the new targetting system, the hints for interesting monsters
//        and features ("right-click/press v for more information") are no
//        longer getting displayed.
//        Players might still end up e'x'aming and particularly clicking on
//        but it's a lot more hit'n'miss now.
bool hints_pos_interesting(int x, int y)
{
    return (cloud_type_at(coord_def(x, y)) != CLOUD_NONE
            || _water_is_disturbed(x, y)
            || _hints_feat_interesting(grd[x][y]));
}

static bool _hints_feat_interesting(dungeon_feature_type feat)
{
    // Altars and branch entrances are always interesting.
    if (feat_is_altar(feat))
        return true;
    if (feat >= DNGN_ENTER_FIRST_BRANCH && feat <= DNGN_ENTER_LAST_BRANCH)
        return true;

    switch (feat)
    {
    // So are statues, traps, and stairs.
    case DNGN_ORCISH_IDOL:
    case DNGN_GRANITE_STATUE:
    case DNGN_TRAP_MAGICAL:
    case DNGN_TRAP_MECHANICAL:
    case DNGN_TRAP_NATURAL:
    case DNGN_TRAP_WEB:
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_ESCAPE_HATCH_DOWN:
    case DNGN_ESCAPE_HATCH_UP:
    case DNGN_ENTER_PORTAL_VAULT:
        return true;
    default:
        return false;
    }
}

void hints_describe_pos(int x, int y)
{
    cgotoxy(1, wherey());
    _hints_describe_disturbance(x, y);
    _hints_describe_cloud(x, y);
    _hints_describe_feature(x, y);
}

static void _hints_describe_feature(int x, int y)
{
    const dungeon_feature_type feat = grd[x][y];
    const coord_def            where(x, y);

    ostringstream ostr;
    ostr << "\n\n<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    bool boring = false;

    switch (feat)
    {
       case DNGN_ORCISH_IDOL:
       case DNGN_GRANITE_STATUE:
            ostr << "이건 단순히 무해한 석상입니다. 아닌가?\n이것은 일단 "
                    "위험한 석상은 아니지만, 몇몇 석상은 위험하고, 자동으로 탐험 예외지역이"
                    "석상 주변으로 설정됩니다. 이러한 석상은 종종 보물을 숨기고 있기도 합니다.";
            break;

       case DNGN_TRAP_MAGICAL:
       case DNGN_TRAP_MECHANICAL:
            ostr << "이 위험한 구조물들은 물리 공격을 가할 수 있습니다. (예를 들면, "
                    "다트나 바늘 등.) 또는 마법 효과를 가진 공격을"
                    "할 수도 있죠. ";

            if (feat == DNGN_TRAP_MECHANICAL)
            {
                ostr << "당신은 이러한 기계 장치들을 작동 해제시킬 수 있습니다."
                        "에 서서 <w>컨트롤</w>키를 누른 채로 방향키를 함정 쪽으로 누르세요 "
                        "다만, 함정를 해제하는 도중 함정이 발동될 수도 있으니 "
                        "때에 따라 꽤 위험한 일이 될 수도 있음을 "
                        "알아두세요.\n\n"

                        "당신은 안전하게 함정을 지나칠 수 있습니다. "
                        "당신이 날고 있다면요.";
            }
            else
            {
                ostr << "마법 함정은 기계장치 함정과는 달리, "
                        "작동을 멈출 수 없으며, 함정 위로 날아서 피할 수도 "
                        "없습니다.";
            }
            Hints.hints_events[HINT_SEEN_TRAP] = false;
            break;

       case DNGN_TRAP_NATURAL: // only shafts for now
            ostr << "던전은 수많은 자연적 장애물을 갖고 있습니다. "
                    "가령, 구멍난 바닥에 빠질 경우, 현재 층에서 3층 아래로 떨어지게 됩니다. "
                    "생각만 해도 아찔하죠? 이러한 자연적인 함정은 해제할 수 없지만, "
                    "구멍이 있다는 것을 알면 안전하게 "
                    "위로 지나갈 수 있습니다. \n"
                    "만약 구멍 아래로 내려가고 싶으시다면, <w>></w>키로 구멍 아래로 내려갈 수 있습니다만, "
					"아주 위험한 일이 된다는 것을 명심하세요.";
            Hints.hints_events[HINT_SEEN_TRAP] = false;
            break;

       case DNGN_TRAP_WEB:
            ostr << "던전의 어떤 영역, 가령 거미 소굴과 같은 곳은 "
                    "때때로 커다란 거미줄에 당신을 걸리게 하여, 당신의 이동을 제약하고 "
                    "근처에 있는 거미들에게 당신의 위치를 알리게 됩니다. "
                    "당신은 이러한 거미줄을 헤쳐나갈 수 있습니다. "
                    "옆에 서서 <w>컨트롤</w>키를 누른 채로 방향키를 거미줄 방향으로 누르세요. "
                    "거미줄을 해체하는 도중, 거미줄이 당신을 휘감을 수도 있기 때문에 "
                    "때에 따라 꽤 위험한 일이 될 수도 있다는 것을 "
                    "알아두세요. \n\n"

                    "거미 형태의 플레이어는 안전하게 거미줄을 지나갈 수 있습니다. "
                    "혹은 일정한 형체가 없는 슬라임이나 젤리류로 변신 중일때도 마찬가지입니다. ";
            Hints.hints_events[HINT_SEEN_WEB] = false;
            break;

       case DNGN_STONE_STAIRS_DOWN_I:
       case DNGN_STONE_STAIRS_DOWN_II:
       case DNGN_STONE_STAIRS_DOWN_III:
            ostr << "당신은 다음 층으로 (더 깊은 곳으로) 입장할 수 있습니다."
                    "(<w>></w>)키를 눌러, 계단 아래로 내려갈 수 있습니다. 원래의 층으로 다시 돌아오려면, "
                    "내려갔던 계단 위에 서서 <w><<</w>키를 누르세요.";
#ifdef USE_TILE
            ostr << "또한, 당신은 <w>쉬프트</w>를 누른 상태에서"
                    "<w>마우스 왼쪽 버튼</w>을 클릭하여"
                    "똑같은 동작을 수행할 수 있습니다. ";
#endif

            if (is_unknown_stair(where))
            {
                ostr << "\n\n당신은 아직 이러한 특정 계단을"
                        "지나오지 않았습니다. ";
            }

            Hints.hints_events[HINT_SEEN_STAIRS] = false;
            break;

       case DNGN_EXIT_DUNGEON:
            ostr << "이 계단은 던전 밖으로 이어집니다. 던전 크롤 밖으로 나가면, "
				    "다시는 던전으로 돌아올 수 없습니다. 즉, 게임을 끝내려면 "
                    "이 출구로 나가 도망칠 수 있습니다. 게임에서 승리하는 방법은 오직"
                    "전설의 '조트의 오브'를 던전 어딘가에서 찾아, 밖으로 갖고 나가는 것입니다.";
            break;

       case DNGN_STONE_STAIRS_UP_I:
       case DNGN_STONE_STAIRS_UP_II:
       case DNGN_STONE_STAIRS_UP_III:
            ostr << "당신은 윗층(더 얕은 층)으로 입장할 수 있습니다. 이 계단을"
                    "따라가세요. (<w><<</w>키). 윗층으로 다시 이동하는 것은, 후퇴하거나, "
                    "휴식을 취할 만한 안전한 곳을 확보하는 데 좋습니다."
                    "그리고 몬스터들은 보통 "
                    "당신 바로 옆에 서있지 않는 이상, 당신을 따라"
                    "올라갈 수 없습니다. 원래의 층으로"
                    "돌아오려면, 내려가는 계단 위에 서서 <w>></w>키를"
                    "누르시면 됩니다. ";
#ifdef USE_TILE
            ostr << "또한, 당신은 <w>쉬프트</w>를 누른 상태에서"
                    "<w>마우스 왼쪽 버튼</w>을 클릭하여"
                    "똑같은 동작을 수행할 수 있습니다. ";
#endif
            if (is_unknown_stair(where))
            {
                ostr << "\n\n당신은 아직 이러한 특정 계단을"
                        "지나오지 않았습니다. ";
            }
            Hints.hints_events[HINT_SEEN_STAIRS] = false;
            break;

       case DNGN_ESCAPE_HATCH_DOWN:
       case DNGN_ESCAPE_HATCH_UP:
            ostr << "비상구는 계단과 같이 <w><<</w>키나 <w>></w>키로 오르내릴 수 있으며, "
                    "위급한 상황에서 빠르게 다른 층으로 피신할 때 사용할 수 있습니다. 계단과 다르게, "
                    "비상구로 층을 이동하였을 경우 곧바로 다시 되돌아올 수 없다는 것을 알아두세요. ";

            Hints.hints_events[HINT_SEEN_ESCAPE_HATCH] = false;
            break;

       case DNGN_ENTER_PORTAL_VAULT:
            ostr << "저기 보이는 관문" << _describe_portal(where);
            Hints.hints_events[HINT_SEEN_PORTAL] = false;
            break;

       case DNGN_CLOSED_DOOR:
       case DNGN_RUNED_DOOR:
            if (!Hints.hints_explored)
            {
                ostr << "\n자동 탐험 중에 실수로 문을 여는 것을 방지하기 위해,"
                        "당신은 맵에 탐험 제외 지역을  "
                        "설정하실 수 있습니다. <w>X</w>키를 눌러, 지도 탐색 모드로 전환한 후, 당신의 마우스 커서를 "
                        "문제가 되는 지형 위에 둔 채로 <w>e</w>키를 한 번 혹은 두 번 누릅니다."
                        "그러면 탐험 예외지역이 설정되며, 자동 탐험/이동 중 탐험 "
                        "예외 지역 안으로 자동으로 이동하는 것을 방지할 수 있습니다. "
                        "같은 방법으로, X키와 e키를 이용하여 탐험 예외 지역을 해제하는 것도 가능합니다.";
            }
            break;

       default:
            if (feat_is_altar(feat))
            {
                god_type altar_god = feat_altar_god(feat);

                // I think right now Sif Muna is the only god for whom
                // you can find altars early and who may refuse to accept
                // worship by one of the hint mode characters. (jpeg)
                if (altar_god == GOD_SIF_MUNA
                    && !player_can_join_god(altar_god))
                {
                    ostr << "<w>p</w>키를 눌러 신전 위에서 기도를 드리면, 신 '"
                         << _(god_name(altar_god).c_str()) << "'은 마법을 전혀 모르는 "
                            "자들을 자신의 신도로 허락하지 않는다는 것을 알 수 있습니다. "
                            "이 신에 대해 조금 더 자세한 정보를 얻고 싶으시다면, "
                            "w>?/g</w>키를 눌러 도움말 DB를 찾아보세요.\n"
                            "대부분의 신들의 경우, 그들의 제단 위에서 "
                            "<w>p</w>키를 눌러, 기도를 드림으로써 그들의 신도가 될 수 있습니다.";
                }
                else if (you.religion == GOD_NO_GOD)
                {
                    ostr << "당신이 신앙을 가질 좋은 기회가 왔군요!"
                            "보통, 신들은 숭배자들에게 각자 자신이 가진 권능을 "
                            "부여하여 그들을 돕습니다. 하지만 "
                            "대부분의 신들은 살아있는 제물이나, 정기적인 "
                            "공물 또는 보답으로 그들을 즐겁게 해주기를 원합니다.\n"
                            "이 제단에 해당하는 신, '<w>"
                         << _(god_name(altar_god).c_str())
                         << "</w>에 대한 정보를 얻고 싶으시다면, 제단 위에서 일단 <w>p</w>키를 눌러 기도를 드리세요. "
                            "단순히 기도를 드리는 것만으로 이 신의 신도가 되지는 않습니다. 신앙을 갖기 전에, "
                            "당신은 자신의 신앙을 확실히 결정했는지 재차 질문 받습니다.";
                }
                else if (you.religion == altar_god)
                {
                    // If we don't have anything to say, return early.
                    return;
                }
                else
                {
                    ostr << _(god_name(you.religion).c_str())
                         << "은(는) 아마도 당신이 다른 신으로 종교를 갈아타는 것을 "
                            "반기지는 않을 것입니다만, 한번 쯤 구경만 하는것은 괜찮습니다. "
                            "<w>'";
                    ostr << _(god_name(altar_god).c_str());
                    ostr << "'</w>에 관한 정보를 더 얻고 싶으시다면, 신전 위에서 <w>p</w>키를 "
                            "눌러 기도를 드리세요. 신앙을 갖기 전에(그리고 "
                            "지금 신앙을 파기할 때!) 당신은 "
                            "확실히 종교를 갖거나 버리기를 결정했는지 재차 질문을 받습니다."
                            "\n당신이 믿고 있는 신에 대한 정보를 확인하려면, "
                         
                         << "<w>^</w>키를 누르거나"
#ifdef USE_TILE
                            ", 또는 당신 캐릭터에 <w>마우스 오른쪽 버튼</w>을 "
                            "<w>쉬프트</w>를 누른 상태에서 누르세요"
#endif
                            ".";
                }
                Hints.hints_events[HINT_SEEN_ALTAR] = false;
                break;
            }
            else if (feat >= DNGN_ENTER_FIRST_BRANCH
                     && feat <= DNGN_ENTER_LAST_BRANCH)
            {
                ostr << "던전 크롤의 수많은 서브던전들 중 하나로 이어지는"
                        "입구의 통로. ";
                if (feat != DNGN_ENTER_TEMPLE)
                    ostr << "조심하세요, 이러한 곳으로 입장하는 것은 때로는 매우 위험할 수 있습니다!";
                break;
            }
            else
            {
                // Describe blood-stains even for boring features.
                if (!is_bloodcovered(where))
                    return;
                boring = true;
            }
    }

    if (is_bloodcovered(where))
    {
        if (!boring)
            ostr << "\n\n";

        ostr << "대부분의 근접 전투나, 몇몇 마법 공격은 "
                "주변에 피를 튀길 것입니다. 물론 공격을 받는 플레이어나 몬스터가 "
                "붉은 피를 갖고 있을 경우에 한해서지만요. 일부 몬스터는 멀리서 "
                "피 냄새를 맡아, 피를 흘린 근원을 추적해올 수도 있습니다.";
    }

    ostr << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    string broken = ostr.str();
    linebreak_string(broken, _get_hints_cols());
    display_tagged_block(broken);
}

static void _hints_describe_cloud(int x, int y)
{
    cloud_type ctype = cloud_type_at(coord_def(x, y));
    if (ctype == CLOUD_NONE)
        return;

    string cname = cloud_name_at_index(env.cgrid(coord_def(x, y)));

    ostringstream ostr;

    ostr << "\n\n<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    ostr << "이 " << _(cname.c_str()) << " ";

    if (ends_with(cname, "s"))
        ostr << "들은 ";
    else
        ostr << "은(는) ";

    bool need_cloud = false;
    switch (ctype)
    {
    case CLOUD_BLACK_SMOKE:
    case CLOUD_GREY_SMOKE:
    case CLOUD_BLUE_SMOKE:
    case CLOUD_TLOC_ENERGY:
    case CLOUD_PURPLE_SMOKE:
    case CLOUD_MIST:
    case CLOUD_MAGIC_TRAIL:
    case CLOUD_DUST_TRAIL:
        ostr << "일단은 무해합니다. ";
        break;

    default:
        if (!is_damaging_cloud(ctype, true))
        {
            ostr << "지금은 당신에게 무해하지만, 때에 따라 유해할수도, 무해할수도 있습니다. "
                    "(<w>%</w>)키를 눌러, 캐릭터 정보 창에서 "
                    "당신의 저항력을 확인해 보세요.";
            need_cloud = true;
        }
        else
        {
            ostr << "대부분 위험하며, 될 수 있는대로 이러한 구름들을 "
                    "피해야 합니다. ";
        }
    }

    if (is_opaque_cloud(env.cgrid[x][y]))
    {
        ostr << (need_cloud? "\n이 구름" : "이것")
             << "은(는) 아주 자욱하여, 당신의 시야를 차단합니다. 이러한 구름이 둘 또는 그 이상이 당신을 "
                "둘러싸고 있으면 당신은 이 구름 너머로 "
                "아무것도 볼 수 없을 것입니다.";
    }

    ostr << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    string broken = ostr.str();
    linebreak_string(broken, _get_hints_cols());
    display_tagged_block(broken);
}

static void _hints_describe_disturbance(int x, int y)
{
    if (!_water_is_disturbed(x, y))
        return;

    ostringstream ostr;

    ostr << "\n\n<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    ostr << "'보이지 않는 적대적인 존재'는 몬스터가 수면 아래에 "
            "숨어있다는 것을 의미합니다. 원거리 공격을 할 때 "
            "시야에 보이는 표적이 있어도, 물 속에 숨은 몬스터는 "
            "자동 표적의 대상이 되지 않습니다. 물론 원한다면, 수동으로 "
            "물 속 대상을 조준할 수는 있지만요.";

    ostr << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    string broken = ostr.str();
    linebreak_string(broken, _get_hints_cols());
    display_tagged_block(broken);
}

static bool _water_is_disturbed(int x, int y)
{
    const coord_def c(x,y);
    const monster* mon = monster_at(c);

    if (!mon || grd(c) != DNGN_SHALLOW_WATER || !you.see_cell(c))
        return false;

    return (!mon->visible_to(&you) && !mons_flies(mon));
}

bool hints_monster_interesting(const monster* mons)
{
    if (mons_is_unique(mons->type) || mons->type == MONS_PLAYER_GHOST)
        return true;

    // Highlighted in some way.
    if (_mons_is_highlighted(mons))
        return true;

    // Dangerous.
    return (mons_threat_level(mons) == MTHRT_NASTY);
}

void hints_describe_monster(const monster_info& mi, bool has_stat_desc)
{
    cgotoxy(1, wherey());
    ostringstream ostr;
    ostr << "\n\n<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    bool dangerous = false;
    if (mons_is_unique(mi.type))
    {
        ostr << "당신만이 이 던전의 유일한 모험가라 생각했나요? "
                "그렇다면 그것은 틀린 생각입니다! 당신 외에도 조트의 오브를 찾으러 던전에 들어온 모험가들이 많습니다."
				"이러한 라이벌 모험가들은 거의 모두 당신에게 적대적일 것이며, 일반 몬스터들에게서는 보기 힘든 "
                "독특한 스킬이나 마법등을 사용해오는 경우도 있으니 조심하는 것이 좋습니다."
                "\n\n";
        dangerous = true;
    }
    else if (mi.type == MONS_PLAYER_GHOST)
    {
        ostr << "죽은 모험가의 유령은, 당신을 자신과 같은 죽은 망령으로 "
                "만드는 생각밖에 하지 않습니다.\n\n";
        dangerous = true;
    }
    else
    {
        const char ch = mons_base_char(mi.type);
        if (ch >= '1' && ch <= '5')
        {
            ostr << "이 몬스터는 "
                 << (ch == '1' ? "최상급의 1급 악마" :
                     ch == '2' ? "고위급의 2급 악마" :
                     ch == '3' ? "중간급의 3급 악마" :
                     ch == '4' ? "하위급의 4급 악마" :
                     ch == '5' ? "최하위급의 5급 악마"
                               : "buggy")
                 << " 몬스터입니다.\n\n";
        }

        // Don't call friendly monsters dangerous.
        if (!mons_att_wont_attack(mi.attitude))
        {
            if (mi.threat == MTHRT_NASTY)
            {
                ostr << "이 몬스터는 당신에게는 엄청나게 위험해 보이는군요!\n";
                dangerous = true;
            }
            else if (mi.threat == MTHRT_TOUGH)
            {
                ostr << "이 몬스터는 당신이 상대하기에 꽤 위험한 몬스터입니다.\n";
                dangerous = true;
            }
        }
    }

    if (mi.is(MB_BERSERK))
    {
        ostr << "광폭화된 몬스터는 피를 갈구하며 미친듯이 싸웁니다. "
                "유저의 광폭화와 마찬가지로, 광폭화된 몬스터는 원래보다 훨씬 강하고 빠릅니다!\n\n";
        dangerous = true;
    }

    // Monster is highlighted.
    if (mi.attitude == ATT_FRIENDLY)
    {
        ostr << "우호적인 몬스터는 당신 주변을 맴돌며, 당신의 전투를 "
                "도우려고 할 것입니다. 당신은 당신의 동료들을 지휘할 수 있습니다. 그들에게 <w>t</w>키를 눌러 "
                "명령을 내려보세요.";

        if (!mons_att_wont_attack(mi.attitude))
        {
            ostr << "\n\n그러나, 이 몬스터는 <w>일시적으로</w> 우호적인 상태이며, "
                    "언제금 우호적인 태도를 버리고, 다시 적대심을 드러낼 수 있으니 "
                    "조심하는 것이 좋습니다.";
        }
    }
    else if (dangerous)
    {
        if (!Hints.hints_explored && (mi.is(MB_WANDERING) || mi.is(MB_UNAWARE)))
        {
            ostr << "당신은 손쉽게 특정 지형에 위험 표시, '탐험 예외 지역'을 설정할 수 있습니다. "
                    "이는 자동탐험 도중에 실수로 그 지역에 들어서는 것을 방지합니다. "
                    "텀험 예외 지역을 설정하려면, <w>x</w>키를 눌러 타게팅 모드로 전환하여, "
                    "당신의 마우스 커서를 몬스터 위에 두고 <w>e</w>키를 "
                    "누르면 됩니다. 그러면 "
					"이 몬스터나 지형 주위로 X표시가 생기고, 해당 표시로부터 반경 8(시야)의 거리 내로 \"차단된\" 지역이 되어 탐험할 때 "
                    "\"탐험 예외 지역\"이 되어, 자동 탐험이나 자동 이동 시 "
                    "이 안으로 들어가지 않을 것입니다.";
        }
        else
        {
            ostr << "도망가기 좋은 시간인 것 같군요";

            if (you.religion == GOD_TROG && you.can_go_berserk())
                ostr << ". 아니면 광폭화 능력(<w>a</w>키)을 쓰세요";
            ostr << ".";
        }
    }
    else if (Options.stab_brand != CHATTR_NORMAL
             && mi.is(MB_STABBABLE))
    {
        ostr << "저기 보이는 몬스터는 아직 당신을 인식하지 못했습니다. "
                "눈에 보이는 모든 몬스터와 교전한다는 건 어리석은 생각입니다. "
                "신중하게 행동하는 것은 저돌적인 태도보다 여러모로 좋습니다.";
    }
    else if (Options.may_stab_brand != CHATTR_NORMAL
             && mi.is(MB_DISTRACTED))
    {
        ostr << "보아하니 무언가가 저 몬스터는. You could "
                "use this opportunity to sneak up on this monster - or to "
                "sneak away.";
    }

    if (!dangerous && !has_stat_desc)
    {
        ostr << "\nThis monster doesn't appear to have any resistances or "
                "susceptibilities. It cannot fly and is of average speed. "
                "Examining other, possibly more high-level monsters can give "
                "important clues as to how to deal with them.";
    }

    ostr << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    string broken = ostr.str();
    linebreak_string(broken, _get_hints_cols());
    display_tagged_block(broken);
}

void hints_observe_cell(const coord_def& gc)
{
    if (feat_is_escape_hatch(grd(gc)))
        learned_something_new(HINT_SEEN_ESCAPE_HATCH, gc);
    else if (feat_is_branch_stairs(grd(gc)))
        learned_something_new(HINT_SEEN_BRANCH, gc);
    else if (is_feature('>', gc))
        learned_something_new(HINT_SEEN_STAIRS, gc);
    else if (is_feature('_', gc))
        learned_something_new(HINT_SEEN_ALTAR, gc);
    else if (is_feature('^', gc))
        learned_something_new(HINT_SEEN_TRAP, gc);
    else if (feat_is_closed_door(grd(gc)))
        learned_something_new(HINT_SEEN_DOOR, gc);
    else if (grd(gc) == DNGN_ENTER_SHOP)
        learned_something_new(HINT_SEEN_SHOP, gc);
    else if (grd(gc) == DNGN_ENTER_PORTAL_VAULT)
        learned_something_new(HINT_SEEN_PORTAL, gc);

    const int it = you.visible_igrd(gc);
    if (it != NON_ITEM)
    {
        const item_def& item(mitm[it]);

        if (Options.feature_item_brand != CHATTR_NORMAL
            && (is_feature('>', gc) || is_feature('<', gc)))
        {
            learned_something_new(HINT_STAIR_BRAND, gc);
        }
        else if (Options.trap_item_brand != CHATTR_NORMAL
                 && is_feature('^', gc))
        {
            learned_something_new(HINT_TRAP_BRAND, gc);
        }
        else if (Options.heap_brand != CHATTR_NORMAL && item.link != NON_ITEM)
            learned_something_new(HINT_HEAP_BRAND, gc);
    }
}

void tutorial_msg(const char *key, bool end)
{
    string text = getHintString(key);
    if (text.empty())
        return mprf(MSGCH_ERROR, "Error, no message for '%s'.", key);

    _replace_static_tags(text);
    text = untag_tiles_console(text);

    if (end)
        screen_end_game(replace_all(text, "\n\n", "\n"));

    // "\n" to preserve indented parts, the rest is unwrapped, or split into
    // paragraphs by "\n\n", split_string() will ignore the empty line.
    vector<string> chunks = split_string("\n", text);
    for (size_t i = 0; i < chunks.size(); i++)
        mpr(chunks[i], MSGCH_TUTORIAL);

    stop_running();
}
