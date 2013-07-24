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
#include "viewchar.h"
#include "viewgeom.h"
#include "viewmap.h"

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

/**
 * Look up and display a hint message from the database. Is usable from dlua,
 * so wizard mode in-game Lua interpreter can be used to test the messages.
 * @param arg1 A string that can be inserted into the hint message.
 * @param arg2 Another string that can be inserted into the hint message.
 */
void print_hint(string key, const string arg1, const string arg2)
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

    print_hint("death");
    more();

    if (Hints.hints_type == HINT_MAGIC_CHAR
        && Hints.hints_spell_counter < Hints.hints_melee_counter)
    {
        print_hint("death conjurer melee");
    }
    else if (you.religion == GOD_TROG && Hints.hints_berserk_counter <= 3
             && !you.berserk() && !you.duration[DUR_EXHAUSTED])
    {
        print_hint("death berserker unberserked");
    }
    else if (Hints.hints_type == HINT_RANGER_CHAR
             && 2*Hints.hints_throw_counter < Hints.hints_melee_counter)
    {
        print_hint("death ranger melee");
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

        print_hint(make_stringf("death random %d", hint));
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

    print_hint("finished");
    more();

    if (Hints.hints_explored)
        print_hint("finished explored");
    else if (Hints.hints_travel)
        print_hint("finished travel");
    else if (Hints.hints_stashes)
        print_hint("finished stashes");
    else
        print_hint(make_stringf("finished random %d", random2(4)));
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

        print_hint("dissection reminder");
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

    print_hint("HINT_SEEN_FIRST_OBJECT",
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
                " (or by using your <w>left mouse button</w> in combination "
                "with the <w>Shift key</w>)"
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
                "but you can also choose them from a menu: type <w>%</w><w>%</w> "
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
            return print_hint("HINT_CONVERT Xom");

        print_hint("HINT_CONVERT");
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
    string broken = "This screen shows your character's set of talents. "
        "You can gain new abilities via certain items, through religion or by "
        "way of mutations. Activation of an ability usually comes at a cost, "
        "e.g. nutrition or Magic power. Press '<w>!</w>' or '<w>?</w>' to "
        "toggle between ability selection and description.";
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
    string broken = "This screen shows the skill set of your character. "
        "The number next to the skill is your current level, the higher the "
        "better. The <brown>brown percent value</brows> shows how much "
        "experience is allocated to go towards that skill. "
        "You can toggle which skills to train by "
        "pressing their slot letters. A <darkgrey>greyish</darkgrey> skill "
        "will not be trained and ease the training of others. "
        "Press <w>!</w> to learn about skill training and <w>?</w> to read "
        "your skills' descriptions.";
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return text.str();
}

string hints_skill_training_info()
{
    textcolor(channel_to_colour(MSGCH_TUTORIAL));
    ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = "The training percentage (in <brown>brown</brown>) "
        "shows the relative amount of the experience gained which will be "
        "used to train each skill. It is automatically set depending on "
        "which skills you have used recently. Disabling a skill sets the "
        "training rate to 0.";
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return text.str();
}

string hints_skills_description_info()
{
    textcolor(channel_to_colour(MSGCH_TUTORIAL));
    ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    string broken = "This screen shows the skill set of your character. "
                    "Press the letter of a skill to read its description, or "
                    "press <w>?</w> again to return to the skill selection.";

    linebreak_string(broken, _get_hints_cols());
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return text.str();
}

// A short explanation of Crawl's target mode and its most important commands.
static string _hints_target_mode(bool spells = false)
{
    string result;
    result = "then be taken to target mode with the nearest monster or "
             "previous target already targeted. You can also cycle through "
             "all hostile monsters in sight with <w>+</w> or <w>-</w>. "
             "Once you're aiming at the correct monster, simply hit "
             "<w>f</w>, <w>Enter</w> or <w>.</w> to shoot at it. "
             "If you miss, <w>";

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

    result += "</w> fires at the same target again.";
    insert_commands(result, cmd, 0);

    return result;
}

static string _hints_abilities(const item_def& item)
{
    string str = "To do this, ";

    vector<command_type> cmd;
    if (!item_is_equipped(item))
    {
        switch (item.base_type)
        {
        case OBJ_WEAPONS:
            str += "first <w>%</w>ield it";
            cmd.push_back(CMD_WIELD_WEAPON);
            break;
        case OBJ_ARMOUR:
            str += "first <w>%</w>ear it";
            cmd.push_back(CMD_WEAR_ARMOUR);
            break;
        case OBJ_JEWELLERY:
            str += "first <w>%</w>ut it on";
            cmd.push_back(CMD_WEAR_JEWELLERY);
            break;
        default:
            str += "<r>(BUG! this item shouldn't give an ability)</r>";
            break;
        }
        str += ", then ";
    }
    str += "enter the ability menu with <w>%</w>, and then "
           "choose the corresponding ability. Note that such an attempt of "
           "activation, especially by the untrained, is likely to fail.";
    cmd.push_back(CMD_USE_ABILITY);

    insert_commands(str, cmd);
    return str;
}

static string _hints_throw_stuff(const item_def &item)
{
    string result;

    result  = "To do this, type <w>%</w> to fire, then <w>";
    result += item.slot;
    result += "</w> for ";
    result += (item.quantity > 1 ? "these" : "this");
    result += " ";
    result += item_base_name(item);
    result += (item.quantity > 1? "s" : "");
    result += ". You'll ";
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
                    ostr << "When wielded, some weapons (such as this one) "
                            "offer certain abilities you can activate. ";
                    ostr << _hints_abilities(item);
                    break;
                }
                else if (gives_resistance(item)
                         && wherey() <= get_number_of_lines() - 3)
                {
                    // It grants a resistance.
                    ostr << "\nThis weapon offers its wearer protection from "
                            "certain sources. For an overview of your "
                            "resistances (among other things) type <w>%</w>"
#ifdef USE_TILE
                            " or click on your avatar with the <w>right mouse "
                            "button</w>"
#endif
                            ".";
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
                ostr << "You can wield this weapon with <w>%</w>, or use "
                        "<w>%</w> to switch between the weapons in slot "
                        "a and b. (Use <w>%i</w> to adjust item slots.)";
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
                    ostr << "\nOn second look, you've been training in <w>"
                         << skill_name(best_wpskill)
                         << "</w> for a while, so maybe you should "
                            "continue training that rather than <w>"
                         << skill_name(curr_wpskill)
                         << "</w>. (Type <w>%</w> to see the skill "
                            "management screen for the actual numbers.)";

                    cmd.push_back(CMD_DISPLAY_SKILLS);
                    long_text = true;
                }
            }
            else // wielded weapon
            {
                if (is_range_weapon(item))
                {
                    ostr << "To attack a monster, ";
#ifdef USE_TILE
                    ostr << "if you have appropriate ammo quivered you can "
                            "<w>left mouse click</w> on the monster while "
                            "prssing the <w>Shift key</w>. Alternatively, "
                            "you can <w>left mouse click</w> on the tile for "
                            "the ammo you wish to fire, and then <w>left "
                            "mouse click</w> on the monster.\n\n";
                    ostr << "To launch ammunition using the keyboard, ";
#endif
                    ostr << "you only need to "
                            "<w>%</w>ire the appropriate type of ammunition. "
                            "You'll ";
                    ostr << _hints_target_mode();
                    cmd.push_back(CMD_FIRE);
                }
                else
                    ostr << "To attack a monster, you can simply walk into it.";
            }

            if (is_throwable(&you, item) && !long_text)
            {
                ostr << "\n\nSome weapons (including this one), can also be "
                        "<w>%</w>ired. ";
                cmd.push_back(CMD_FIRE);
                ostr << _hints_throw_stuff(item);
                long_text = true;
            }
            if (!item_type_known(item)
                && (is_artefact(item)
                    || get_equip_desc(item) != ISFLAG_NO_DESC))
            {
                ostr << "\n\nWeapons and armour that have unusual descriptions "
                     << "like this are much more likely to be of higher "
                     << "enchantment or have special properties, good or bad.";

                Hints.hints_events[HINT_SEEN_RANDART] = false;
            }
            if (item_known_cursed(item) && !long_text)
            {
                ostr << "\n\nOnce wielded, a cursed weapon won't leave your "
                        "hands again until the curse has been lifted by "
                        "reading a scroll of remove curse or one of the "
                        "enchantment scrolls.";

                if (!wielded && is_throwable(&you, item))
                    ostr << " (Throwing it is safe, though.)";

                Hints.hints_events[HINT_YOU_CURSED] = false;
            }
            Hints.hints_events[HINT_SEEN_WEAPON] = false;
            break;
        }
        case OBJ_MISSILES:
            if (is_throwable(&you, item))
            {
                ostr << item.name(true, DESC_YOUR)
                     << gettext(" can be <w>%</w>ired without the use of a launcher. ");
                ostr << _hints_throw_stuff(item);
                cmd.push_back(CMD_FIRE);
            }
            else if (is_launched(&you, you.weapon(), item))
            {
                ostr << "As you're already wielding the appropriate launcher, "
                        "you can simply ";
#ifdef USE_TILE
                ostr << "<w>left mouse click</w> on the monster you want "
                        "to hit while pressing the <w>Shift key</w>. "
                        "Alternatively, you can <w>left mouse click</w> on "
                        "this tile of the ammo you want to fire, and then "
                        "<w>left mouse click</w> on the monster you want "
                        "to hit.\n\n"

                        "To launch this ammo using the keyboard, you can "
                        "simply ";
#endif

                ostr << "<w>%</w>ire "
                     << (item.quantity > 1 ? "these" : "this")
                     << " " << item.name(false, DESC_BASENAME)
                     << (item.quantity > 1? "s" : "")
                     << ". You'll ";
                ostr << _hints_target_mode();
                cmd.push_back(CMD_FIRE);
            }
            else
            {
                ostr << "To shoot "
                     << (item.quantity > 1 ? "these" : "this")
                     << " " << item.name(false, DESC_BASENAME)
                     << (item.quantity > 1? "s" : "")
                     << ", first you need to <w>%</w>ield an appropriate "
                        "launcher.";
                cmd.push_back(CMD_WIELD_WEAPON);
            }
            Hints.hints_events[HINT_SEEN_MISSILES] = false;
            break;

        case OBJ_ARMOUR:
        {
            bool wearable = true;
            if (you.species == SP_CENTAUR && item.sub_type == ARM_BOOTS)
            {
                ostr << "As a Centaur you cannot wear boots. "
                        "(Type <w>%</w> to see a list of your mutations and "
                        "innate abilities.)";
                cmd.push_back(CMD_DISPLAY_MUTATIONS);
                wearable = false;
            }
            else if (you.species == SP_MINOTAUR && is_hard_helmet(item))
            {
                ostr << "As a Minotaur you cannot wear helmets. "
                        "(Type <w>%</w> to see a list of your mutations and "
                        "innate abilities.)";
                cmd.push_back(CMD_DISPLAY_MUTATIONS);
                wearable = false;
            }
            else if (item.sub_type == ARM_CENTAUR_BARDING)
            {
                ostr << "Only centaurs can wear centaur barding.";
                wearable = false;
            }
            else if (item.sub_type == ARM_NAGA_BARDING)
            {
                ostr << "Only nagas can wear naga barding.";
                wearable = false;
            }
            else
            {
                ostr << "You can wear pieces of armour with <w>%</w> and take "
                        "them off again with <w>%</w>"
#ifdef USE_TILE
                        ", or, alternatively, simply click on their tiles to "
                        "perform either action."
#endif
                        ".";
                cmd.push_back(CMD_WEAR_ARMOUR);
                cmd.push_back(CMD_REMOVE_ARMOUR);
            }

            if (Hints.hints_type == HINT_MAGIC_CHAR
                && get_armour_slot(item) == EQ_BODY_ARMOUR
                && !is_effectively_light_armour(&item))
            {
                ostr << "\nNote that body armour with high evasion penalties "
                        "may hinder your ability to learn and cast spells. "
                        "Light armour such as robes, leather armour or any "
                        "elven armour will be generally safe for any aspiring "
                        "spellcaster.";
            }
            else if (Hints.hints_type == HINT_MAGIC_CHAR
                     && is_shield(item))
            {
                ostr << "\nNote that shields will hinder you ability to "
                        "cast spells; the larger the shield, the bigger "
                        "the penalty.";
            }
            else if (Hints.hints_type == HINT_RANGER_CHAR
                     && is_shield(item))
            {
                ostr << "\nNote that wearing a shield will greatly decrease "
                        "the speed at which you can shoot arrows.";
            }

            if (!item_type_known(item)
                && (is_artefact(item)
                    || get_equip_desc(item) != ISFLAG_NO_DESC))
            {
                ostr << "\n\nWeapons and armour that have unusual descriptions "
                     << "like this are much more likely to be of higher "
                     << "enchantment or have special properties, good or bad.";

                Hints.hints_events[HINT_SEEN_RANDART] = false;
            }
            if (wearable)
            {
                if (item_known_cursed(item))
                {
                    ostr << "\nA cursed piece of armour, once worn, cannot be "
                            "removed again until the curse has been lifted by "
                            "reading a scroll of remove curse or enchant "
                            "armour.";
                }
                if (gives_resistance(item))
                {
                    ostr << "\n\nThis armour offers its wearer protection from "
                            "certain sources. For an overview of your"
                            " resistances (among other things) type <w>%</w>"
#ifdef USE_TILE
                            " or click on your avatar with the <w>right mouse "
                            "button</w>"
#endif
                            ".";
                    cmd.push_back(CMD_RESISTS_SCREEN);
                }
                if (gives_ability(item))
                {
                    ostr << "\n\nWhen worn, some types of armour (such as "
                            "this one) offer certain <w>%</w>bilities you can "
                            "activate. ";
                    ostr << _hints_abilities(item);
                    cmd.push_back(CMD_USE_ABILITY);
                }
            }
            Hints.hints_events[HINT_SEEN_ARMOUR] = false;
            break;
        }
        case OBJ_WANDS:
            ostr << "The magic within can be unleashed by evoking "
                    "(<w>%</w>) it.";
            cmd.push_back(CMD_EVOKE);
#ifdef USE_TILE
            ostr << " Alternatively, you can 1) <w>left mouse click</w> on "
                    "the monster you wish to target (or your player character "
                    "to target yourself) while pressing the <w>";
#ifdef USE_TILE_WEB
            ostr << "Ctrl + Shift keys";
#else
#if defined(UNIX) && defined(USE_TILE_LOCAL)
            if (!tiles.is_fullscreen())
              ostr << "Ctrl + Shift keys";
            else
#endif
              ostr << "Alt key";
#endif
            ostr << "</w> and pick the wand from the menu, or 2) "
                    "<w>left mouse click</w> on the wand tile and then "
                    "<w>left mouse click</w> on your target.";
#endif
            Hints.hints_events[HINT_SEEN_WAND] = false;
            break;

        case OBJ_FOOD:
            if (food_is_rotten(item) && !player_mutation_level(MUT_SAPROVOROUS))
            {
                ostr << "You can't eat rotten chunks, so you might just as "
                        "well ";
                if (!in_inventory(item))
                    ostr << "ignore them. ";
                else
                {
                    ostr << "<w>%</w>rop this. Use <w>%&</w> to select all "
                            "skeletons, corpses and rotten chunks in your "
                            "inventory. ";
                    cmd.push_back(CMD_DROP);
                    cmd.push_back(CMD_DROP);
                }
            }
            else
            {
                ostr << "Food can simply be <w>%</w>aten"
#ifdef USE_TILE
                        ", something you can also do by <w>left clicking</w> "
                        "on it"
#endif
                        ". ";
                cmd.push_back(CMD_EAT);

                if (item.sub_type == FOOD_CHUNK)
                {
                    ostr << "Note that most species refuse to eat raw meat "
                            "unless hungry. ";

                    if (food_is_rotten(item))
                    {
                        ostr << "Even fewer can safely digest rotten meat, and "
                             "you're probably not part of that group.";
                    }
                }
            }
            Hints.hints_events[HINT_SEEN_FOOD] = false;
            break;

        case OBJ_SCROLLS:
            ostr << "Type <w>%</w> to read this scroll"
#ifdef USE_TILE
                    "or simply click on it with your <w>left mouse button</w>"
#endif
                    ".";
            cmd.push_back(CMD_READ);

            Hints.hints_events[HINT_SEEN_SCROLL] = false;
            break;

        case OBJ_JEWELLERY:
        {
            ostr << "Jewellery can be <w>%</w>ut on or <w>%</w>emoved "
                    "again"
#ifdef USE_TILE
                    ", though in Tiles, either can be done by clicking on the "
                    "item in your inventory"
#endif
                    ".";
            cmd.push_back(CMD_WEAR_JEWELLERY);
            cmd.push_back(CMD_REMOVE_JEWELLERY);

            if (item_known_cursed(item))
            {
                ostr << "\nA cursed piece of jewellery will cling to its "
                        "unfortunate wearer's neck or fingers until the curse "
                        "is finally lifted when he or she reads a scroll of "
                        "remove curse.";
            }
            if (gives_resistance(item))
            {
                    ostr << "\n\nThis "
                         << (item.sub_type < NUM_RINGS ? "ring" : "amulet")
                         << " offers its wearer protection "
                            "from certain sources. For an overview of your "
                            "resistances (among other things) type <w>%</w>"
#ifdef USE_TILE
                            " or click on your avatar with the <w>right mouse "
                            "button</w>"
#endif
                            ".";
                cmd.push_back(CMD_RESISTS_SCREEN);
            }
            if (gives_ability(item))
            {
                ostr << "\n\nWhen worn, some types of jewellery (such as this "
                        "one) offer certain <w>%</w>bilities you can activate. ";
                cmd.push_back(CMD_USE_ABILITY);
                ostr << _hints_abilities(item);
            }
            Hints.hints_events[HINT_SEEN_JEWELLERY] = false;
            break;
        }
        case OBJ_POTIONS:
            ostr << "Type <w>%</w> to quaff this potion"
#ifdef USE_TILE
                    "or simply click on it with your <w>left mouse button</w>"
#endif
                    ".";
            cmd.push_back(CMD_QUAFF);
            Hints.hints_events[HINT_SEEN_POTION] = false;
            break;

        case OBJ_BOOKS:
            if (item.sub_type == BOOK_MANUAL)
            {
                ostr << "A manual can greatly help you in training a skill. "
                        "To use it, just <w>%</w>ead it. As long as you are "
                        "carrying it, the skill in question will be trained "
                        "more efficiently and will level up faster.";
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
                        ostr << "It's a book, you can <w>%</w>ead it.";
                        cmd.push_back(CMD_READ);
                    }
                    else
                    {
                        ostr << "A spellbook! You could <w>%</w>emorise some "
                                "spells and then cast them with <w>%</w>.";
                        cmd.push_back(CMD_MEMORISE_SPELL);
                        cmd.push_back(CMD_CAST_SPELL);
                    }
                    ostr << "\nAs a worshipper of "
                         << god_name(GOD_TROG)
                         << ", though, you might instead wish to burn this "
                            "tome of hated magic by using the corresponding "
                            "<w>%</w>bility. "
                            "Note that this only works on books that are lying "
                            "on the floor and not on your current square. ";
                    cmd.push_back(CMD_USE_ABILITY);
                }
                else if (!item_ident(item, ISFLAG_KNOW_TYPE))
                {
                    ostr << "It's a book, you can <w>%</w>ead it"
#ifdef USE_TILE
                            ", something that can also be achieved by clicking "
                            "on its tile in your inventory."
#endif
                            ".";
                    cmd.push_back(CMD_READ);
                }
                else if (item.sub_type == BOOK_DESTRUCTION)
                {
                    ostr << "This magical item can cause great destruction "
                            "- to you, or your surroundings. Use with care!";
                }
                else
                {
                    if (player_can_memorise(item))
                    {
                        ostr << "Such a <lightblue>highlighted "
                                "spell</lightblue> can be <w>%</w>emorised "
                                "right away. ";
                        cmd.push_back(CMD_MEMORISE_SPELL);
                    }
                    else
                    {
                        ostr << "You cannot memorise any "
                             << (you.spell_no ? "more " : "")
                             << "spells right now. This will change as you "
                                "grow in levels and Spellcasting proficiency. ";
                    }

                    if (you.spell_no)
                    {
                        ostr << "\n\nTo do magic, ";
#ifdef USE_TILE
                        ostr << "you can <w>left mouse click</w> on the "
                                "monster you wish to target (or on your "
                                "player character to cast a spell on "
                                "yourself) while pressing the <w>Control "
                                "key</w>, and then select a spell from the "
                                "menu. Or you can switch to the spellcasting "
                                "display by <w>clicking on the</w> "
                                "corresponding <w>tab</w>."
                                "\n\nAlternatively, ";
#endif
                        ostr << "you can type <w>%</w> and choose a "
                                "spell, e.g. <w>a</w> (check with <w>?</w>). "
                                "For attack spells you'll ";
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
                ostr << "Skeletons can be used as components for certain "
                        "necromantic spells. Apart from that, they are "
                        "largely useless.";

                if (in_inventory(item))
                {
                    ostr << " In the drop menu you can select all skeletons, "
                            "corpses, and rotten chunks in your inventory "
                            "at once with <w>%&</w>.";
                    cmd.push_back(CMD_DROP);
                }
                break;
            }

            ostr << "Corpses lying on the floor can be <w>%</w>hopped up to "
                    "produce chunks for food (though they may be unhealthy)";
            cmd.push_back(CMD_BUTCHER);

            if (!food_is_rotten(item) && god_likes_fresh_corpses(you.religion))
            {
                ostr << ", or offered as a sacrifice to "
                     << god_name(you.religion)
                     << " by <w>%</w>raying over them";
                cmd.push_back(CMD_PRAY);
            }
            ostr << ". ";

            if (food_is_rotten(item))
            {
                ostr << "Rotten corpses won't be of any use to you, though, so "
                        "you might just as well ";
                if (!in_inventory(item))
                    ostr << "ignore them. ";
                else
                {
                    ostr << "<w>%</w>rop this. Use <w>%&</w> to select all "
                            "skeletons and rotten chunks or corpses in your "
                            "inventory. ";
                    cmd.push_back(CMD_DROP);
                    cmd.push_back(CMD_DROP);
                }
                ostr << "No god will accept such rotten sacrifice, either.";
            }
            else
            {
#ifdef USE_TILE
                ostr << " For an individual corpse in your inventory, the most "
                        "practical way to chop it up is to drop it by clicking "
                        "on it with your <w>left mouse button</w> while "
                        "<w>Shift</w> is pressed, and then repeat that command "
                        "for the corpse tile now lying on the floor.";
#endif
            }
            if (!in_inventory(item))
                break;

            ostr << "\n\nIf there are several items in your inventory you'd "
                    "like to drop, the most convenient way is to use the "
                    "<w>%</w>rop menu. On a related note, butchering "
                    "several corpses on a floor square is facilitated by "
                    "using the <w>%</w>hop prompt where <w>c</w> is a "
                    "valid synonym for <w>y</w>es or you can directly chop "
                    "<w>a</w>ll corpses.";
            cmd.push_back(CMD_DROP);
            cmd.push_back(CMD_BUTCHER);
            break;

        case OBJ_RODS:
            if (!item_ident(item, ISFLAG_KNOW_TYPE))
            {
                ostr << "\n\nTo find out what this rod might do, you have "
                        "to <w>%</w>ield it to see if you can use the "
                        "spells hidden within, then e<w>%</w>oke it to "
                        "actually do so"
#ifdef USE_TILE
                        ", both of which can be done by clicking on it"
#endif
                        ".";
            }
            else
            {
                ostr << "\n\nYou can use this rod's magic by "
                        "<w>%</w>ielding and e<w>%</w>oking it"
#ifdef USE_TILE
                        ", both of which can be achieved by clicking on it"
#endif
                        ".";
            }
            cmd.push_back(CMD_WIELD_WEAPON);
            cmd.push_back(CMD_EVOKE_WIELDED);
            Hints.hints_events[HINT_SEEN_ROD] = false;
            break;

        case OBJ_STAVES:
            ostr << "This staff can enhance your spellcasting, possibly "
                    "making a certain spell school more powerful, or "
                    "making difficult magic easier to cast. ";

            if (gives_resistance(item))
            {
                ostr << "It also offers its wielder protection from "
                        "certain sources. For an overview of your "
                        "resistances (among other things) type <w>%</w>"
#ifdef USE_TILE
                        " or click on your avatar with the <w>right mouse "
                        "button</w>"
#endif
                        ".";

                cmd.push_back(CMD_RESISTS_SCREEN);
            }
            else if (you.religion == GOD_TROG)
            {
                ostr << "\n\nSeeing how "
                     << god_name(GOD_TROG, false)
                     << " frowns upon the use of magic, this staff will be "
                        "of little use to you and you might just as well "
                        "<w>%</w>rop it now.";
                cmd.push_back(CMD_DROP);
            }
            Hints.hints_events[HINT_SEEN_STAFF] = false;
            break;

        case OBJ_MISCELLANY:
            if (is_deck(item))
            {
                ostr << "Decks of cards are powerful but dangerous magical "
                        "items. Try <w>%</w>ielding and e<w>%</w>oking it"
#ifdef USE_TILE
                        ", either of which can be done by clicking on it"
#endif
                        ". If you use a scroll of identify on the deck you "
                        "can discover the name of the top card, making the "
                        "deck less risky to draw from. You can read about the "
                        "effect of a card by searching the game's database "
                        "with <w>%/c</w>.";
                cmd.push_back(CMD_WIELD_WEAPON);
                cmd.push_back(CMD_EVOKE_WIELDED);
                cmd.push_back(CMD_DISPLAY_COMMANDS);
            }
            else
            {
                ostr << "Miscellaneous items sometimes harbour magical powers "
                        "that can be harnessed by e<w>%</w>oking the item.";
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
         "Inscriptions are a powerful concept of Dungeon Crawl.\n"
         "You can inscribe items to differentiate them, or to comment on them, \n"
         "but also to set rules for item interaction. If you are new to Crawl, \n"
         "you can safely ignore this feature, though.";

        longtext = true;
    }

    text << "\n"
       "(In the main screen, press <w>?6</w> for more information.)\n";
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
            ostr << "It's just a harmless statue - or is it?\nEven if not "
                    "a danger by themselves, statues often mark special "
                    "areas, dangerous ones or ones harbouring treasure.";
            break;

       case DNGN_TRAP_MAGICAL:
       case DNGN_TRAP_MECHANICAL:
            ostr << "These nasty constructions can do physical damage (with "
                    "darts or needles, for example) or have other, more "
                    "magical effects. ";

            if (feat == DNGN_TRAP_MECHANICAL)
            {
                ostr << "You can attempt to deactivate the mechanical type by "
                        "standing next to it and then pressing <w>Ctrl</w> "
                        "and the direction of the trap. Note that this usually "
                        "causes the trap to go off, so it can be quite a "
                        "dangerous task.\n\n"

                        "You can safely pass over a mechanical trap if "
                        "you're flying.";
            }
            else
            {
                ostr << "Magical traps can't be disarmed, and unlike "
                        "mechanical traps you can't avoid tripping them "
                        "by flying over them.";
            }
            Hints.hints_events[HINT_SEEN_TRAP] = false;
            break;

       case DNGN_TRAP_NATURAL: // only shafts for now
            ostr << "The dungeon contains a number of natural obstacles such "
                    "as shafts, which lead one to three levels down. They "
                    "can't be disarmed, but once you know the shaft is there, "
                    "you can safely step over it.\n"
                    "If you want to jump down there, use <w>></w> to do so. "
                    "Be warned that getting back here might be difficult.";
            Hints.hints_events[HINT_SEEN_TRAP] = false;
            break;

       case DNGN_TRAP_WEB:
            ostr << "Some areas of the dungeon, such as Spiders Nest, may "
                    "be strewn with giant webs that may ensnare you for a short "
                    "time and notify nearby spiders of your location. "
                    "You can attempt to clear away the web by "
                    "standing next to it and then pressing <w>Ctrl</w> "
                    "and the direction of the web. Note that this often "
                    "results in just getting entangled anyway, so it can be "
                    "quite a dangerous task.\n\n"

                    "Players in Spider Form can safely navigate the webs (as "
                    "can incorporeal entities and various oozes). ";
            Hints.hints_events[HINT_SEEN_WEB] = false;
            break;

       case DNGN_STONE_STAIRS_DOWN_I:
       case DNGN_STONE_STAIRS_DOWN_II:
       case DNGN_STONE_STAIRS_DOWN_III:
            ostr << "You can enter the next (deeper) level by following them "
                    "down (<w>></w>). To get back to this level again, "
                    "press <w><<</w> while standing on the upstairs.";
#ifdef USE_TILE
            ostr << " In Tiles, you can achieve the same, in either direction, "
                    "by clicking the <w>left mouse button</w> while pressing "
                    "<w>Shift</w>. ";
#endif

            if (is_unknown_stair(where))
            {
                ostr << "\n\nYou have not yet passed through this particular "
                        "set of stairs. ";
            }

            Hints.hints_events[HINT_SEEN_STAIRS] = false;
            break;

       case DNGN_EXIT_DUNGEON:
            ostr << "These stairs lead out of the dungeon. Following them "
                    "will end the game. The only way to win is to "
                    "transport the fabled Orb of Zot outside.";
            break;

       case DNGN_STONE_STAIRS_UP_I:
       case DNGN_STONE_STAIRS_UP_II:
       case DNGN_STONE_STAIRS_UP_III:
            ostr << "You can enter the previous (shallower) level by "
                    "following these up (<w><<</w>). This is ideal for "
                    "retreating or finding a safe resting spot, since the "
                    "previous level will have less monsters and monsters "
                    "on this level can't follow you up unless they're "
                    "standing right next to you. To get back to this "
                    "level again, press <w>></w> while standing on the "
                    "downstairs.";
#ifdef USE_TILE
            ostr << " In Tiles, you can perform either action simply by "
                    "clicking the <w>left mouse button</w> while pressing "
                    "<w>Shift</w> instead. ";
#endif
            if (is_unknown_stair(where))
            {
                ostr << "\n\nYou have not yet passed through this "
                        "particular set of stairs. ";
            }
            Hints.hints_events[HINT_SEEN_STAIRS] = false;
            break;

       case DNGN_ESCAPE_HATCH_DOWN:
       case DNGN_ESCAPE_HATCH_UP:
            ostr << "Escape hatches can be used to quickly leave a level with "
                    "<w><<</w> and <w>></w>, respectively. Note that you will "
                    "usually be unable to return right away.";

            Hints.hints_events[HINT_SEEN_ESCAPE_HATCH] = false;
            break;

       case DNGN_ENTER_PORTAL_VAULT:
            ostr << "This " << _describe_portal(where);
            Hints.hints_events[HINT_SEEN_PORTAL] = false;
            break;

       case DNGN_CLOSED_DOOR:
       case DNGN_RUNED_DOOR:
            if (!Hints.hints_explored)
            {
                ostr << "\nTo avoid accidentally opening a door you'd rather "
                        "remain closed during travel or autoexplore, you can "
                        "mark it with an exclusion from the map view "
                        "(<w>X</w>) with <w>ee</w> while your cursor is on the "
                        "grid in question. Such an exclusion will prevent "
                        "autotravel from ever entering that grid until you "
                        "remove the exclusion with another press of <w>Xe</w>.";
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
                    ostr << "As <w>p</w>raying on the altar will tell you, "
                         << god_name(altar_god) << " only accepts worship from "
                            "those who have already dabbled in magic. You can "
                            "find out more about this god by searching the "
                            "database with <w>?/g</w>.\n"
                            "For other gods, you'll be able to join the faith "
                            "by <w>p</w>raying at their altar.";
                }
                else if (you.religion == GOD_NO_GOD)
                {
                    ostr << "This is your chance to join a religion! In "
                            "general, the gods will help their followers, "
                            "bestowing powers of all sorts upon them, but many "
                            "of them demand a life of dedication, constant "
                            "tributes or entertainment in return.\n"
                            "You can get information about <w>"
                         << god_name(altar_god)
                         << "</w> by pressing <w>p</w> while standing on the "
                            "altar. Before taking up the responding faith "
                            "you'll be asked for confirmation.";
                }
                else if (you.religion == altar_god)
                {
                    // If we don't have anything to say, return early.
                    return;
                }
                else
                {
                    ostr << god_name(you.religion)
                         << " probably won't like it if you switch allegiance, "
                            "but having a look won't hurt: to get information "
                            "on <w>";
                    ostr << god_name(altar_god);
                    ostr << "</w>, press <w>p</w> while standing on the "
                            "altar. Before taking up the responding faith (and "
                            "abandoning your current one!) you'll be asked for "
                            "confirmation."
                            "\nTo see your current standing with "
                         << god_name(you.religion)
                         << " press <w>^</w>"
#ifdef USE_TILE
                            ", or click with your <w>right mouse button</w> "
                            "on your avatar while pressing <w>Shift</w>"
#endif
                            ".";
                }
                Hints.hints_events[HINT_SEEN_ALTAR] = false;
                break;
            }
            else if (feat >= DNGN_ENTER_FIRST_BRANCH
                     && feat <= DNGN_ENTER_LAST_BRANCH)
            {
                ostr << "An entryway into one of the many dungeon branches in "
                        "Crawl. ";
                if (feat != DNGN_ENTER_TEMPLE)
                    ostr << "Beware, sometimes these can be deadly!";
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

        ostr << "Many forms of combat and some forms of magical attack "
                "will splatter the surroundings with blood (if the victim has "
                "any blood, that is). Some monsters can smell blood from "
                "a distance and will come looking for whatever the blood "
                "was spilled from.";
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

    ostr << "The " << cname << " ";

    if (ends_with(cname, "s"))
        ostr << "are ";
    else
        ostr << "is ";

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
        ostr << "harmless. ";
        break;

    default:
        if (!is_damaging_cloud(ctype, true))
        {
            ostr << "currently harmless, but that could change at some point. "
                    "Check the overview screen (<w>%</w>) to view your "
                    "resistances.";
            need_cloud = true;
        }
        else
        {
            ostr << "probably dangerous, and you should stay out of it if you "
                    "can. ";
        }
    }

    if (is_opaque_cloud(env.cgrid[x][y]))
    {
        ostr << (need_cloud? "\nThis cloud" : "It")
             << " is opaque. If two or more opaque clouds are between "
                "you and a square you won't be able to see anything in that "
                "square.";
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

    ostr << "The strange disturbance means that there's a monster hiding "
            "under the surface of the shallow water. Submerged monsters will "
            "not be autotargeted when doing a ranged attack while there are "
            "other, visible targets in sight. Of course you can still target "
            "it manually if you wish to.";

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
        ostr << "Did you think you were the only adventurer in the dungeon? "
                "Well, you thought wrong! These unique adversaries often "
                "possess skills that normal monster wouldn't, so be "
                "careful.\n\n";
        dangerous = true;
    }
    else if (mi.type == MONS_PLAYER_GHOST)
    {
        ostr << "The ghost of a deceased adventurer, it would like nothing "
                "better than to send you the same way.\n\n";
        dangerous = true;
    }
    else
    {
        const char ch = mons_base_char(mi.type);
        if (ch >= '1' && ch <= '5')
        {
            ostr << "This monster is a demon of the "
                 << (ch == '1' ? "highest" :
                     ch == '2' ? "second-highest" :
                     ch == '3' ? "middle" :
                     ch == '4' ? "second-lowest" :
                     ch == '5' ? "lowest"
                               : "buggy")
                 << " tier.\n\n";
        }

        // Don't call friendly monsters dangerous.
        if (!mons_att_wont_attack(mi.attitude))
        {
            if (mi.threat == MTHRT_NASTY)
            {
                ostr << "This monster appears to be really dangerous!\n";
                dangerous = true;
            }
            else if (mi.threat == MTHRT_TOUGH)
            {
                ostr << "This monster appears to be quite dangerous.\n";
                dangerous = true;
            }
        }
    }

    if (mi.is(MB_BERSERK))
    {
        ostr << "A berserking monster is bloodthirsty and fighting madly. "
                "Such a blood rage makes it particularly dangerous!\n\n";
        dangerous = true;
    }

    // Monster is highlighted.
    if (mi.attitude == ATT_FRIENDLY)
    {
        ostr << "Friendly monsters will follow you around and attempt to aid "
                "you in battle. You can order your allies by <w>t</w>alking "
                "to them.";

        if (!mons_att_wont_attack(mi.attitude))
        {
            ostr << "\n\nHowever, it is only <w>temporarily</w> friendly, "
                    "and will become dangerous again when this friendliness "
                    "wears off.";
        }
    }
    else if (dangerous)
    {
        if (!Hints.hints_explored && (mi.is(MB_WANDERING) || mi.is(MB_UNAWARE)))
        {
            ostr << "You can easily mark its square as dangerous to avoid "
                    "accidentally entering into its field of view when using "
                    "auto-explore or auto-travel. To do so, enter targetting "
                    "mode with <w>x</w> and then press <w>e</w> when your "
                    "cursor is hovering over the monster's grid. Doing so will "
                    "mark this grid and all surrounding ones within a radius "
                    "of 8 as \"excluded\" ones that explore or travel modes "
                    "won't enter.";
        }
        else
        {
            ostr << "This might be a good time to run away";

            if (you.religion == GOD_TROG && you.can_go_berserk())
                ostr << " or apply your Berserk <w>a</w>bility";
            ostr << ".";
        }
    }
    else if (Options.stab_brand != CHATTR_NORMAL
             && mi.is(MB_STABBABLE))
    {
        ostr << "Apparently it has not noticed you - yet. Note that you do "
                "not have to engage every monster you meet. Sometimes, "
                "discretion is the better part of valour.";
    }
    else if (Options.may_stab_brand != CHATTR_NORMAL
             && mi.is(MB_DISTRACTED))
    {
        ostr << "Apparently it has been distracted by something. You could "
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
