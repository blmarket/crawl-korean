﻿/**
 * @file
 * @brief Notetaking stuff
**/
// 이하 notes.cc는 코드 직접 수정으로 처리. (코드 특성상 gettext보다 이쪽이 나을듯합니다.) 
#include "AppHdr.h"

#include <vector>
#include <sstream>
#include <iomanip>

#include "notes.h"

#include "branch.h"
#include "files.h"
#include "kills.h"
#include "hiscores.h"
#include "libutil.h"
#include "message.h"
#include "mutation.h"
#include "options.h"
#include "place.h"
#include "religion.h"
#include "skills2.h"
#include "spl-util.h"
#include "state.h"
#include "tags.h"

#define NOTES_VERSION_NUMBER 1002

vector<Note> note_list;

// return the real number of the power (casting out nonexistent powers),
// starting from 0, or -1 if the power doesn't exist
static int _real_god_power(int religion, int idx)
{
    if (god_gain_power_messages[religion][idx][0] == 0)
        return -1;

    int count = 0;
    for (int j = 0; j < idx; ++j)
        if (god_gain_power_messages[religion][j][0])
            ++count;

    return count;
}

static bool _is_highest_skill(int skill)
{
    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        if (i == skill)
            continue;
        if (you.skills[i] >= you.skills[skill])
            return false;
    }
    return true;
}

static bool _is_noteworthy_hp(int hp, int maxhp)
{
    return (hp > 0 && Options.note_hp_percent
            && hp <= (maxhp * Options.note_hp_percent) / 100);
}

static int _dungeon_branch_depth(uint8_t branch)
{
    if (branch >= NUM_BRANCHES)
        return -1;
    return brdepth[branch];
}

static bool _is_noteworthy_dlevel(unsigned short place)
{
    branch_type branch = place_branch(place);
    int lev = place_depth(place);

    // Entering the Abyss is noted a different way, since we care mostly about
    // the cause.
    if (branch == BRANCH_ABYSS)
        return lev == _dungeon_branch_depth(branch);

    // Other portal levels are always interesting.
    if (!is_connected_branch(static_cast<branch_type>(branch)))
        return true;

    return (lev == _dungeon_branch_depth(branch)
            || branch == BRANCH_MAIN_DUNGEON && (lev % 5) == 0
            || branch == BRANCH_MAIN_DUNGEON && lev == 14
            || branch != BRANCH_MAIN_DUNGEON && lev == 1);
}

// Is a note worth taking?
// This function assumes that game state has not changed since
// the note was taken, e.g. you.* is valid.
static bool _is_noteworthy(const Note& note)
{
    // Always noteworthy.
    if (note.type == NOTE_XP_LEVEL_CHANGE
        || note.type == NOTE_LEARN_SPELL
        || note.type == NOTE_GET_GOD
        || note.type == NOTE_GOD_GIFT
        || note.type == NOTE_GET_MUTATION
        || note.type == NOTE_LOSE_MUTATION
        || note.type == NOTE_GET_ITEM
        || note.type == NOTE_ID_ITEM
        || note.type == NOTE_BUY_ITEM
        || note.type == NOTE_DONATE_MONEY
        || note.type == NOTE_SEEN_MONSTER
        || note.type == NOTE_DEFEAT_MONSTER
        || note.type == NOTE_POLY_MONSTER
        || note.type == NOTE_USER_NOTE
        || note.type == NOTE_MESSAGE
        || note.type == NOTE_LOSE_GOD
        || note.type == NOTE_PENANCE
        || note.type == NOTE_MOLLIFY_GOD
        || note.type == NOTE_DEATH
        || note.type == NOTE_XOM_REVIVAL
        || note.type == NOTE_SEEN_FEAT
        || note.type == NOTE_PARALYSIS
        || note.type == NOTE_NAMED_ALLY
        || note.type == NOTE_ALLY_DEATH
        || note.type == NOTE_FEAT_MIMIC
        || note.type == NOTE_OFFERED_SPELL)
    {
        return true;
    }

    // Never noteworthy, hooked up for fun or future use.
    if (note.type == NOTE_MP_CHANGE
        || note.type == NOTE_MAXHP_CHANGE
        || note.type == NOTE_MAXMP_CHANGE)
    {
        return false;
    }

    // Xom effects are only noteworthy if the option is true.
    if (note.type == NOTE_XOM_EFFECT)
        return Options.note_xom_effects;

    // God powers might be noteworthy if it's an actual power.
    if (note.type == NOTE_GOD_POWER
        && _real_god_power(note.first, note.second) == -1)
    {
        return false;
    }

    // HP noteworthiness is handled in its own function.
    if (note.type == NOTE_HP_CHANGE
        && !_is_noteworthy_hp(note.first, note.second))
    {
        return false;
    }

    // Skills are noteworthy if in the skill value list or if
    // it's a new maximal skill (depending on options).
    if (note.type == NOTE_GAIN_SKILL || note.type == NOTE_LOSE_SKILL)
    {
        if (Options.note_all_skill_levels
            || note.second <= 27 && Options.note_skill_levels[note.second]
            || Options.note_skill_max && _is_highest_skill(note.first))
        {
            return true;
        }
        return false;
    }

    if (note.type == NOTE_DUNGEON_LEVEL_CHANGE)
        return _is_noteworthy_dlevel(note.packed_place);

    for (unsigned i = 0; i < note_list.size(); ++i)
    {
        if (note_list[i].type != note.type)
            continue;

        const Note& rnote(note_list[i]);
        switch (note.type)
        {
        case NOTE_GOD_POWER:
            if (rnote.first == note.first && rnote.second == note.second)
                return false;
            break;

        case NOTE_HP_CHANGE:
            // Not if we have a recent warning
            // unless we've lost half our HP since then.
            if (note.turn - rnote.turn < 5
                && note.first * 2 >= rnote.first)
            {
                return false;
            }
            break;

        default:
            mpr("Buggy note passed: unknown note type");
            // Return now, rather than give a "Buggy note passed" message
            // for each note of the matching type in the note list.
            return true;
        }
    }
    return true;
}

static const char* _number_to_ordinal(int number)
{
    const char* ordinals[5] = { "첫 번째", "두 번째", "세 번째", "네 번째", "다섯 번째" }; //{ "first", "second", "third", "fourth", "fifth" };

    if (number < 1)
        return "[unknown ordinal (too small)]";
    if (number > 5)
        return "[unknown ordinal (too big)]";
    return ordinals[number-1];
}

string Note::describe(bool when, bool where, bool what) const
{
    ostringstream result;

    if (when)
        result << setw(6) << turn << " ";

    if (where)
    {
        result << "| " << chop_string(short_place_name(packed_place),
                                      MAX_NOTE_PLACE_LEN) << " | ";
    }

    if (what)
    {
        switch (type)
        {
        case NOTE_HP_CHANGE:
            // [ds] Shortened HP change note from "Had X hitpoints" to
            // accommodate the cause for the loss of hitpoints.
            result << "HP: " << first << "/" << second
                   << " [" << name << "]";
            break;
        case NOTE_XOM_REVIVAL:
            result << "좀에 의해 부활함"; // "Xom revived you"; 
            break;
        case NOTE_MP_CHANGE:
            result << "MP: " << first << "/" << second; // << "Mana: " << first << "/" << second; 
            break;
        case NOTE_MAXHP_CHANGE:
            result << "최대 HP " << first << "에 도달."; // << "Reached " << first << " max hit points"; 
            break;
        case NOTE_MAXMP_CHANGE:
            result << "최대 MP " << first << "에 도달."; // << "Reached " << first << " max mana"; 
            break;
        case NOTE_XP_LEVEL_CHANGE:
            result << "레벨 " << first << " 달성. " << name; // << "Reached XP level " << first << ". " << name; 
            break;
        case NOTE_DUNGEON_LEVEL_CHANGE:
            if (!desc.empty())
                result << desc;
            else
                result << place_name(packed_place, true, true) << "에 입장."; // "Entered " << place_name(packed_place, true, true);
            break;
        case NOTE_LEARN_SPELL:
            result << spell_difficulty(static_cast<spell_type>(first))     // "Learned a level "
                   << "레벨 주문 '"                                        // spell_difficulty(static_cast<spell_type>(first))
                   << _(spell_title(static_cast<spell_type>(first)))       // " spell: "
                   << "'을(를) 기억.";                                      // spell_title(static_cast<spell_type>(first));
            break;
        case NOTE_GET_GOD:
            result // << "Became a worshipper of "
                   << _(god_name(static_cast<god_type>(first), true).c_str()) << "의 신자가 됨."; 
            break;
        case NOTE_LOSE_GOD:
            result // << "Fell from the grace of "
                   << _(god_name(static_cast<god_type>(first)).c_str()) << "(으)로부터 떠남."; 
            break;
        case NOTE_PENANCE:
            result // << "Was placed under penance by "
                   << _(god_name(static_cast<god_type>(first)).c_str()) << "에게 속죄를 요구받음."; 
            break;
        case NOTE_MOLLIFY_GOD:
            result // << "Was forgiven by "
                   << _(god_name(static_cast<god_type>(first)).c_str()) << "(으)로부터 용서받음."; 
            break;
        case NOTE_GOD_GIFT:
            result // << "Received a gift from "
                   << _(god_name(static_cast<god_type>(first)).c_str()) << "에게 선물을 하사받음."; 
            break;
        case NOTE_ID_ITEM:
            result << name << "을(를) 감정함."; // << "Identified " << name; 
            if (!desc.empty())
                result << " (" << desc << ")";
            break;
        case NOTE_GET_ITEM:
            result << name << "을(를) 획득함."; // << "Got " << name; 
            break;
        case NOTE_BUY_ITEM:
            result << "금화 " << first << "(으)로, " << name << "을(를) 구입함."; // << "Bought " << name << " for " << first << " gold piece"
                   // << (first == 1 ? "" : "s");
            break;
        case NOTE_DONATE_MONEY:
            result << "금화 " << first << "을(를) 기부함.";  // << "Donated " << first << " gold piece"
                   // << (first == 1 ? "" : "s") << " to Zin";
            break;
        case NOTE_GAIN_SKILL:
            result << _(skill_name(static_cast<skill_type>(first))) // << "Reached skill level " << second 
                   << "이(가) 레벨 " << second << "에 도달함.";     // << " in " << skill_name(static_cast<skill_type>(first));
            break;
        case NOTE_LOSE_SKILL:
            result << _(skill_name(static_cast<skill_type>(first))) // << "Reduced skill "
                   << "이(가) 레벨 " << second                      // << skill_name(static_cast<skill_type>(first))
                   << "(으)로 감소함." ;                            // << " to level " << second;
            break;
        case NOTE_SEEN_MONSTER:
            result << name << "와(과) 조우함."; // "Noticed " << name;
            break;
        case NOTE_DEFEAT_MONSTER:
            if (second)
                result << "동료 " << name << "이(가) 죽거나 무력화됨."; // << name << " (ally) was " << desc; 
            else
                result << name << desc; // << uppercase_first(desc) << " " << name; 
            break;
        case NOTE_POLY_MONSTER:
            result << name << "을(를) " << desc << "(으)로 변신시킴."; // << name << " changed into " << desc; 
            break;
        case NOTE_GOD_POWER:
            result // << "Acquired "
                   << _(god_name(static_cast<god_type>(first)).c_str()) << "의" 
                   << " "
                   << _number_to_ordinal(_real_god_power(first, second)+1)
                   << " 권능을 얻음."; 
            break;
        case NOTE_GET_MUTATION:
            result << "변이를 얻음: " 
                   << mutation_name(static_cast<mutation_type>(first),
                                    second == 0 ? 1 : second);
            if (!name.empty())
                result << " [" << name << "]";
            break;
        case NOTE_LOSE_MUTATION:
            result << "변이를 잃음: " 
                   << mutation_name(static_cast<mutation_type>(first),
                                    second == 3 ? 3 : second+1);
            if (!name.empty())
                result << " [" << name << "]";
            break;
        case NOTE_DEATH:
            result << name;
            break;
        case NOTE_USER_NOTE:
            result << Options.user_note_prefix << name;
            break;
        case NOTE_MESSAGE:
            result << name;
            break;
        case NOTE_SEEN_FEAT:
            result << name << "을(를) 발견."; // "Found " << name; 
            break;
        case NOTE_FEAT_MIMIC:
            result << name << "은(는) 미믹이었음."; // name <<" was a mimic."; 
            break;
        case NOTE_XOM_EFFECT:
            result << "좀의 장난: " << name;
#if defined(DEBUG_XOM) || defined(NOTE_DEBUG_XOM)
            // If debugging, also take note of piety and tension.
            result << " (piety: " << first;
            if (second >= 0)
                result << ", tension: " << second;
            result << ")";
#endif
            break;
        case NOTE_PARALYSIS:
            result <<  name << "에 의해 " << first << "턴간 마비됨.";  // "Paralysed by " << name << " for " << first << " turns";
            break;
        case NOTE_NAMED_ALLY:
            result << "동료 " << name << "을(를) 얻음."; // "Gained " << name << " as an ally";
            break;
        case NOTE_ALLY_DEATH:
            result << "동료 " << name << "이(가) 사망함."; // "Your ally " << name << " died";
            break;
        case NOTE_OFFERED_SPELL:
            result << "베후메트로부터 주문 '"
                   << _(spell_title(static_cast<spell_type>(first)))
                   << "'을(를) 제공받음.";
            break;
        default:
            result << "버그: 알 수 없는 이력 타입"; //"Buggy note description: unknown note type";
            break;
        }
    }

    if (type == NOTE_SEEN_MONSTER || type == NOTE_DEFEAT_MONSTER)
    {
        if (what && first == MONS_PANDEMONIUM_LORD)
            result << "(판데모니움의 군주)"; // " the pandemonium lord"; 
    }
    return result.str();
}

Note::Note()
{
    turn         = you.num_turns;
    packed_place = get_packed_place();
}

Note::Note(NOTE_TYPES t, int f, int s, const char* n, const char* d) :
    type(t), first(f), second(s)
{
    if (n)
        name = string(n);
    if (d)
        desc = string(d);

    turn         = you.num_turns;
    packed_place = get_packed_place();
}

void Note::check_milestone() const
{
    if (crawl_state.game_is_arena())
        return;

    if (type == NOTE_DUNGEON_LEVEL_CHANGE)
    {
        const int br = place_branch(packed_place),
                 dep = place_depth(packed_place);

        // Wizlabs report their milestones on their own.
        if (br != -1 && br != BRANCH_WIZLAB)
        {
            ASSERT(br >= 0 && br < NUM_BRANCHES);
            string branch = place_name(packed_place, true, false).c_str();
            if (branch.find("The ") == 0)
                branch[0] = tolower(branch[0]);

            if (dep == 1)
            {
                mark_milestone(br == BRANCH_ZIGGURAT ? "zig.enter" : "br.enter",
                               std::string(_(branch.c_str())) + "에 입장.", "parent"); //"entered " + branch + ".", "parent");
            }
            else if (dep == _dungeon_branch_depth(br) || dep == 14
                     || br == BRANCH_ZIGGURAT)
            {
                string level = place_name(packed_place, true, true);
                if (level.find("Level ") == 0)
                    level[0] = tolower(level[0]);

                ostringstream branch_finale;
                branch_finale << "reached " << level << ".";
                mark_milestone(br == BRANCH_ZIGGURAT ? "zig" :
                               dep == 14 ? "br.mid" : "br.end",
                               branch_finale.str());
            }
        }
    }
}

void Note::save(writer& outf) const
{
    marshallInt(outf, type);
    marshallInt(outf, turn);
    marshallShort(outf, packed_place);
    marshallInt(outf, first);
    marshallInt(outf, second);
    marshallString4(outf, name);
    marshallString4(outf, desc);
}

void Note::load(reader& inf)
{
    type = static_cast<NOTE_TYPES>(unmarshallInt(inf));
    turn = unmarshallInt(inf);
    packed_place = unmarshallShort(inf);
    first  = unmarshallInt(inf);
    second = unmarshallInt(inf);
    unmarshallString4(inf, name);
    unmarshallString4(inf, desc);
}

static bool notes_active = false;

bool notes_are_active()
{
    return notes_active;
}

void take_note(const Note& note, bool force)
{
    if (notes_active && (force || _is_noteworthy(note)))
    {
        note_list.push_back(note);
        note.check_milestone();
    }
}

void activate_notes(bool active)
{
    notes_active = active;
}

void save_notes(writer& outf)
{
    marshallInt(outf, NOTES_VERSION_NUMBER);
    marshallInt(outf, note_list.size());
    for (unsigned i = 0; i < note_list.size(); ++i)
        note_list[i].save(outf);
}

void load_notes(reader& inf)
{
    if (unmarshallInt(inf) != NOTES_VERSION_NUMBER)
        return;

    const int num_notes = unmarshallInt(inf);
    for (int i = 0; i < num_notes; ++i)
    {
        Note new_note;
        new_note.load(inf);
        note_list.push_back(new_note);
    }
}

void make_user_note()
{
    char buf[400];
    bool validline = !msgwin_get_line("이력: ", buf, sizeof(buf)); // "Enter note: " 
    if (!validline || (!*buf))
        return;
    Note unote(NOTE_USER_NOTE);
    unote.name = buf;
    take_note(unote);
}
