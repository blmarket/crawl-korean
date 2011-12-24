/**
 * @file
 * @brief Dumps character info out to the morgue file.
**/

#include "AppHdr.h"

#include "chardump.h"
#include "clua.h"

#include <string>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#if !defined(__IBMCPP__) && !defined(TARGET_COMPILER_VC)
#include <unistd.h>
#endif
#include <ctype.h>

#include "externs.h"
#include "options.h"

#include "abl-show.h"
#include "artefact.h"
#include "debug.h"
#include "describe.h"
#include "dgn-overview.h"
#include "dungeon.h"
#include "files.h"
#include "godprayer.h"
#include "hiscores.h"
#include "initfile.h"
#include "itemprop.h"
#include "itemname.h"
#include "items.h"
#include "kills.h"
#include "message.h"
#include "menu.h"
#include "mutation.h"
#include "notes.h"
#include "output.h"
#include "place.h"
#include "player.h"
#include "religion.h"
#include "shopping.h"
#include "showsymb.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "env.h"
#include "transform.h"
#include "travel.h"
#include "unicode.h"
#include "view.h"
#include "viewchar.h"
#include "xom.h"

struct dump_params;

static void _sdump_header(dump_params &);
static void _sdump_stats(dump_params &);
static void _sdump_location(dump_params &);
static void _sdump_religion(dump_params &);
static void _sdump_burden(dump_params &);
static void _sdump_hunger(dump_params &);
static void _sdump_transform(dump_params &);
static void _sdump_visits(dump_params &);
static void _sdump_gold(dump_params &);
static void _sdump_misc(dump_params &);
static void _sdump_turns_by_place(dump_params &);
static void _sdump_notes(dump_params &);
static void _sdump_inventory(dump_params &);
static void _sdump_skills(dump_params &);
static void _sdump_spells(dump_params &);
static void _sdump_mutations(dump_params &);
static void _sdump_messages(dump_params &);
static void _sdump_screenshot(dump_params &);
static void _sdump_kills_by_place(dump_params &);
static void _sdump_kills(dump_params &);
static void _sdump_newline(dump_params &);
static void _sdump_overview(dump_params &);
static void _sdump_hiscore(dump_params &);
static void _sdump_monster_list(dump_params &);
static void _sdump_vault_list(dump_params &);
static void _sdump_spell_usage(dump_params &);
static void _sdump_action_counts(dump_params &);
static void _sdump_separator(dump_params &);
#ifdef CLUA_BINDINGS
static void _sdump_lua(dump_params &);
#endif
static bool _write_dump(const std::string &fname, dump_params &,
                        bool print_dump_path = false);

struct dump_section_handler
{
    const char *name;
    void (*handler)(dump_params &);
};

struct dump_params
{
    std::string &text;
    std::string section;
    bool show_prices;
    bool full_id;
    const scorefile_entry *se;

    dump_params(std::string &_text, const std::string &sec = "",
                bool prices = false, bool id = false,
                const scorefile_entry *s = NULL)
        : text(_text), section(sec), show_prices(prices), full_id(id),
          se(s)
    {
    }
};

static dump_section_handler dump_handlers[] = {
    { "header",         _sdump_header        },
    { "stats",          _sdump_stats         },
    { "location",       _sdump_location      },
    { "religion",       _sdump_religion      },
    { "burden",         _sdump_burden        },
    { "hunger",         _sdump_hunger        },
    { "transform",      _sdump_transform     },
    { "visits",         _sdump_visits        },
    { "gold",           _sdump_gold          },
    { "misc",           _sdump_misc          },
    { "turns_by_place", _sdump_turns_by_place},
    { "notes",          _sdump_notes         },
    { "inventory",      _sdump_inventory     },
    { "skills",         _sdump_skills        },
    { "spells",         _sdump_spells        },
    { "mutations",      _sdump_mutations     },
    { "messages",       _sdump_messages      },
    { "screenshot",     _sdump_screenshot    },
    { "kills_by_place", _sdump_kills_by_place},
    { "kills",          _sdump_kills         },
    { "overview",       _sdump_overview      },
    { "hiscore",        _sdump_hiscore       },
    { "monlist",        _sdump_monster_list  },
    { "vaults",         _sdump_vault_list    },
    { "spell_usage",    _sdump_spell_usage   },
    { "action_counts",  _sdump_action_counts },

    // Conveniences for the .crawlrc artist.
    { "",               _sdump_newline       },
    { "-",              _sdump_separator     },

#ifdef CLUA_BINDINGS
    { NULL,             _sdump_lua           }
#else
    { NULL,             NULL                }
#endif
};

static void dump_section(dump_params &par)
{
    for (int i = 0; ; ++i)
    {
        if (!dump_handlers[i].name || par.section == dump_handlers[i].name)
        {
            if (dump_handlers[i].handler)
                (*dump_handlers[i].handler)(par);
            break;
        }
    }
}

bool dump_char(const std::string &fname, bool show_prices, bool full_id,
               const scorefile_entry *se)
{
    // Start with enough room for 100 80 character lines.
    std::string text;
    text.reserve(100 * 80);

    dump_params par(text, "", show_prices, full_id, se);

    for (int i = 0, size = Options.dump_order.size(); i < size; ++i)
    {
        par.section = Options.dump_order[i];
        dump_section(par);
    }

    return _write_dump(fname, par, se == NULL);
}

static void _sdump_header(dump_params &par)
{
    std::string type = crawl_state.game_type_name();
    if (type.empty())
        type = CRAWL;
    else
        type += " DCSS";

    par.text += " " + type + " version " + Version::Long();
    par.text += " character file.\n\n";
}

static void _sdump_stats(dump_params &par)
{
    par.text += dump_overview_screen(par.full_id);
    par.text += "\n\n";
}

static void _sdump_burden(dump_params &par)
{
    std::string verb = par.se? "were" : "are";

    switch (you.burden_state)
    {
    case BS_OVERLOADED:
        par.text += "You " + verb + " overloaded with stuff.\n";
        break;
    case BS_ENCUMBERED:
        par.text += "You " + verb + " encumbered.\n";
        break;
    default:
        break;
    }
}

static void _sdump_hunger(dump_params &par)
{
    if (par.se)
        par.text += "You were ";
    else
        par.text += "You are ";

    par.text += hunger_level();
    par.text += ".\n\n";
}

static void _sdump_transform(dump_params &par)
{
    std::string &text(par.text);
    if (you.form)
    {
        std::string verb = par.se? "were" : "are";

        switch (you.form)
        {
        case TRAN_SPIDER:
            text += "You " + verb + " in spider-form.";
            break;
        case TRAN_BAT:
            text += "You " + verb + " in ";
            if (you.species == SP_VAMPIRE)
                text += "vampire ";
            text += "bat-form.";
            break;
        case TRAN_BLADE_HANDS:
            text += "Your " + blade_parts() + " " + verb + " blades.";
            break;
        case TRAN_STATUE:
            text += "You " + verb + " a stone statue.";
            break;
        case TRAN_ICE_BEAST:
            text += "You " + verb + " a creature of crystalline ice.";
            break;
        case TRAN_DRAGON:
            text += "You " + verb + " a fearsome dragon!";
            break;
        case TRAN_LICH:
            text += "You " + verb + " in lich-form.";
            break;
        case TRAN_PIG:
            text += "You " + verb + " a filthy swine.";
            break;
        case TRAN_APPENDAGE:
            if (you.attribute[ATTR_APPENDAGE] == MUT_TENTACLE_SPIKE)
            {
                text += make_stringf("One of your tentacles %s a temporary spike.",
                                     par.se ? "had" : "has");
            }
            else
            {
                text += make_stringf("You %s grown temporary %s.",
                                     par.se ? "had" : "have", appendage_name());
            }
            break;
        case TRAN_NONE:
            break;
        }

        text += "\n\n";
    }
}

static void _sdump_visits(dump_params &par)
{
    std::string &text(par.text);

    std::string have = "have ";
    std::string seen = "seen";
    if (par.se) // you died -> past tense
    {
        have = "";
        seen = "saw";
    }

    std::vector<PlaceInfo> branches_visited =
        you.get_all_place_info(true, true);

    PlaceInfo branches_total;
    for (unsigned int i = 0; i < branches_visited.size(); i++)
        branches_total += branches_visited[i];

    text += make_stringf("You %svisited %d branch",
                         have.c_str(), (int)branches_visited.size());
    if (branches_visited.size() != 1)
        text += "es";
    text += make_stringf(" of the dungeon, and %s %d of its levels.\n",
                         seen.c_str(), branches_total.levels_seen);

    PlaceInfo place_info = you.get_place_info(LEVEL_PANDEMONIUM);
    if (place_info.num_visits > 0)
    {
        text += make_stringf("You %svisited Pandemonium %d time",
                             have.c_str(), place_info.num_visits);
        if (place_info.num_visits > 1)
            text += "s";
        text += make_stringf(", and %s %d of its levels.\n",
                             seen.c_str(), place_info.levels_seen);
    }

    place_info = you.get_place_info(LEVEL_ABYSS);
    if (place_info.num_visits > 0)
    {
        text += make_stringf("You %svisited the Abyss %d time",
                             have.c_str(), place_info.num_visits);
        if (place_info.num_visits > 1)
            text += "s";
        text += ".\n";
    }

    place_info = you.get_place_info(LEVEL_LABYRINTH);
    if (place_info.num_visits > 0)
    {
        text += make_stringf("You %svisited %d Labyrinth",
                             have.c_str(), place_info.num_visits);
        if (place_info.num_visits > 1)
            text += "s";
        text += ".\n";
    }

    place_info = you.get_place_info(LEVEL_PORTAL_VAULT);
    if (place_info.num_visits > 0)
    {
        CrawlVector &vaults =
            you.props[YOU_PORTAL_VAULT_NAMES_KEY].get_vector();

        int num_bazaars = 0;
        int num_zigs    = 0;
        int zig_levels  = 0;
        std::vector<std::string> misc_portals;

        for (unsigned int i = 0; i < vaults.size(); i++)
        {
            std::string name = vaults[i].get_string();

            if (name.find("Ziggurat") != std::string::npos)
            {
                zig_levels++;

                if (name == "Ziggurat:1")
                    num_zigs++;
            }
            else if (name == "bazaar")
                num_bazaars++;
            else
                misc_portals.push_back(name);
        }

        if (num_bazaars > 0)
        {
            text += make_stringf("You %svisited %d bazaar",
                                 have.c_str(), num_bazaars);

            if (num_bazaars > 1)
                text += "s";
            text += ".\n";
        }

        if (num_zigs > 0)
        {
            text += make_stringf("You %s%s %d Ziggurat",
                                 have.c_str(),
                                 (num_zigs == you.zigs_completed) ? "completed"
                                                                  : "visited",
                                 num_zigs);
            if (num_zigs > 1)
                text += "s";
            if (num_zigs != you.zigs_completed && you.zigs_completed)
                text += make_stringf(" (completing %d)", you.zigs_completed);
            text += make_stringf(", and %s %d of %s levels",
                                 seen.c_str(), zig_levels,
                                 num_zigs > 1 ? "their" : "its");
            if (num_zigs != 1 && !you.zigs_completed)
                text += make_stringf(" (deepest: %d)", you.zig_max);
            text += ".\n";
        }

        if (!misc_portals.empty())
        {
            text += make_stringf("You %svisited %d portal chamber",
                                 have.c_str(), (int)misc_portals.size());
            if (misc_portals.size() > 1)
                text += "s";
            text += ": ";
            text += comma_separated_line(misc_portals.begin(),
                                         misc_portals.end(),
                                         ", ");
            text += ".\n";
        }
    }

    text += "\n";
}

static void _sdump_gold(dump_params &par)
{
    std::string &text(par.text);

    int lines = 0;

    const char* have = "have ";
    if (par.se) // you died -> past tense
        have = "";

    if (you.attribute[ATTR_GOLD_FOUND] > 0)
    {
        lines++;
        text += make_stringf("You %scollected %d gold pieces.\n", have,
                             you.attribute[ATTR_GOLD_FOUND]);
    }

    if (you.attribute[ATTR_PURCHASES] > 0)
    {
        lines++;
        text += make_stringf("You %sspent %d gold pieces at shops.\n", have,
                             you.attribute[ATTR_PURCHASES]);
    }

    if (you.attribute[ATTR_DONATIONS] > 0)
    {
        lines++;
        text += make_stringf("You %sdonated %d gold pieces.\n", have,
                             you.attribute[ATTR_DONATIONS]);
    }

    if (you.attribute[ATTR_MISC_SPENDING] > 0)
    {
        lines++;
        text += make_stringf("You %sused %d gold pieces for miscellaneous "
                             "purposes.\n", have,
                             you.attribute[ATTR_MISC_SPENDING]);
    }

    if (lines > 0)
        text += "\n";
}

static void _sdump_misc(dump_params &par)
{
    _sdump_location(par);
    _sdump_religion(par);
    _sdump_burden(par);
    _sdump_hunger(par);
    _sdump_transform(par);
    _sdump_visits(par);
    _sdump_gold(par);
}

#define TO_PERCENT(x, y) (100.0f * (static_cast<float>(x)) / (static_cast<float>(y)))

static std::string _sdump_turns_place_info(PlaceInfo place_info,
                                           std::string name = "")
{
    PlaceInfo   gi = you.global_info;
    std::string out;

    if (name.empty())
        name = place_info.short_name();

    float a, b, c, d, e, f;
    unsigned int non_interlevel =
        place_info.turns_total - place_info.turns_interlevel;
    unsigned int global_non_interlevel =
        gi.turns_total - gi.turns_interlevel;


    a = TO_PERCENT(place_info.turns_total, gi.turns_total);
    b = TO_PERCENT(non_interlevel, global_non_interlevel);
    c = TO_PERCENT(place_info.turns_interlevel, place_info.turns_total);
    d = TO_PERCENT(place_info.turns_resting, non_interlevel);
    e = TO_PERCENT(place_info.turns_explore, non_interlevel);
    f = static_cast<float>(non_interlevel) /
        static_cast<float>(place_info.levels_seen);

    out =
        make_stringf("%14s | %5.1f | %5.1f | %5.1f | %5.1f | %5.1f | %13.1f\n",
                     name.c_str(), a, b, c , d, e, f);

    out = replace_all(out, " nan ", " N/A ");

    return out;
}

static void _sdump_turns_by_place(dump_params &par)
{
    std::string &text(par.text);

    std::vector<PlaceInfo> all_visited =
        you.get_all_place_info(true);

    text +=
"Table legend:\n"
" A = Turns spent in this place as a percentage of turns spent in the\n"
"     entire game.\n"
" B = Non-inter-level travel turns spent in this place as a percentage of\n"
"     non-inter-level travel turns spent in the entire game.\n"
" C = Inter-level travel turns spent in this place as a percentage of\n"
"     turns spent in this place.\n"
" D = Turns resting spent in this place as a percentage of non-inter-level\n"
"     travel turns spent in this place.\n"
" E = Turns spent auto-exploring this place as a percentage of\n"
"     non-inter-level travel turns spent in this place.\n"
" F = Non-inter-level travel turns spent in this place divided by the\n"
"     number of levels of this place that you've seen.\n\n";

    text += "               ";
    text += "    A       B       C       D       E               F\n";
    text += "               ";
    text += "+-------+-------+-------+-------+-------+----------------------\n";

    text += _sdump_turns_place_info(you.global_info, "Total");

    for (unsigned int i = 0; i < all_visited.size(); i++)
    {
        PlaceInfo pi = all_visited[i];
        text += _sdump_turns_place_info(pi);
    }

    text += "               ";
    text += "+-------+-------+-------+-------+-------+----------------------\n";

    text += "\n";
}

static void _sdump_newline(dump_params &par)
{
    par.text += "\n";
}

static void _sdump_separator(dump_params &par)
{
    par.text += std::string(79, '-') + "\n";
}

#ifdef CLUA_BINDINGS
// Assume this is an arbitrary Lua function name, call the function and
// dump whatever it returns.
static void _sdump_lua(dump_params &par)
{
    std::string luatext;
    if (!clua.callfn(par.section.c_str(), ">s", &luatext)
        && !clua.error.empty())
    {
        par.text += "Lua dump error: " + clua.error + "\n";
    }
    else
        par.text += luatext;
}
#endif

 //---------------------------------------------------------------
 //
 // munge_description
 //
 // word wrap to 80 characters.
 // XXX: should be replaced by some other linewrapping function
 //      now EOL munging is gone
 //---------------------------------------------------------------
std::string munge_description(std::string inStr)
{
    std::string outStr;

    outStr.reserve(inStr.length() + 32);

    const int kIndent = 3;

    if (inStr.empty()) // always at least an empty line
        return "\n";

    while (!inStr.empty())
    {
        outStr += std::string(kIndent, ' ')
                + wordwrap_line(inStr, 79 - kIndent)
                + "\n";
    }

    return (outStr);
}

static void _sdump_messages(dump_params &par)
{
    // A little message history:
    if (Options.dump_message_count > 0)
    {
        par.text += "Message History\n\n";
        par.text += get_last_messages(Options.dump_message_count);
    }
}

static void _sdump_screenshot(dump_params &par)
{
    par.text += screenshot();
    par.text += "\n\n";
}

static void _sdump_notes(dump_params &par)
{
    std::string &text(par.text);
    if (note_list.empty())
        return;

    text += "\nNotes\nTurn   | Place    | Note\n";
    text += "--------------------------------------------------------------\n";
    for (unsigned i = 0; i < note_list.size(); ++i)
    {
        text += note_list[i].describe();
        text += "\n";
    }
    text += "\n";
}

 //---------------------------------------------------------------
 //
 // dump_location
 //
 //---------------------------------------------------------------
static void _sdump_location(dump_params &par)
{
    if (you.absdepth0 == -1
        && you.where_are_you == BRANCH_MAIN_DUNGEON
        && you.level_type == LEVEL_DUNGEON)
    {
        par.text += "You escaped";
    }
    else if (par.se)
        par.text += "You were " + prep_branch_level_name();
    else
        par.text += "You are " + prep_branch_level_name();

    par.text += ".";
    par.text += "\n";
}

static void _sdump_religion(dump_params &par)
{
    std::string &text(par.text);
    if (you.religion != GOD_NO_GOD)
    {
        if (par.se)
            text += "You worshipped ";
        else
            text += "You worship ";
        text += god_name(you.religion);
        text += ".\n";

        if (you.religion != GOD_XOM)
        {
            if (!player_under_penance())
            {
                text += god_prayer_reaction();
                text += "\n";
            }
            else
            {
                std::string verb = par.se ? "was" : "is";

                text += god_name(you.religion);
                text += " " + verb + " demanding penance.\n";
            }
        }
        else
        {
            if (par.se)
                text += "You were ";
            else
                text += "You are ";
            text += describe_xom_favour(false);
            text += "\n";
        }
    }
}

static bool _dump_item_origin(const item_def &item, int value)
{
#define fs(x) (flags & (x))
    const int flags = Options.dump_item_origins;
    if (flags == IODS_EVERYTHING)
        return (true);

    if (fs(IODS_ARTEFACTS)
        && is_artefact(item) && item_ident(item, ISFLAG_KNOW_PROPERTIES))
    {
        return (true);
    }
    if (fs(IODS_EGO_ARMOUR) && item.base_type == OBJ_ARMOUR
        && item_type_known(item))
    {
        const int spec_ench = get_armour_ego_type(item);
        return (spec_ench != SPARM_NORMAL);
    }

    if (fs(IODS_EGO_WEAPON) && item.base_type == OBJ_WEAPONS
        && item_type_known(item))
    {
        return (get_weapon_brand(item) != SPWPN_NORMAL);
    }

    if (fs(IODS_JEWELLERY) && item.base_type == OBJ_JEWELLERY)
        return (true);

    if (fs(IODS_RUNES) && item.base_type == OBJ_MISCELLANY
        && item.sub_type == MISC_RUNE_OF_ZOT)
    {
        return (true);
    }

    if (fs(IODS_RODS) && item.base_type == OBJ_STAVES
        && item_is_rod(item))
    {
        return (true);
    }

    if (fs(IODS_STAVES) && item.base_type == OBJ_STAVES
        && !item_is_rod(item))
    {
        return (true);
    }

    if (fs(IODS_BOOKS) && item.base_type == OBJ_BOOKS)
        return (true);

    const int refpr = Options.dump_item_origin_price;
    if (refpr == -1)
        return (false);
    if (value == -1)
        value = item_value(item, false);
    return (value >= refpr);
#undef fs
}

 //---------------------------------------------------------------
 //
 // dump_inventory
 //
 //---------------------------------------------------------------
static void _sdump_inventory(dump_params &par)
{
    int i, j;

    std::string &text(par.text);
    std::string text2;

    int inv_class2[NUM_OBJECT_CLASSES];
    int inv_count = 0;

    for (i = 0; i < NUM_OBJECT_CLASSES; i++)
        inv_class2[i] = 0;

    for (i = 0; i < ENDOFPACK; i++)
    {
        if (you.inv[i].defined())
        {
            // adds up number of each class in invent.
            inv_class2[you.inv[i].base_type]++;
            inv_count++;
        }
    }

    if (!inv_count)
    {
        text += "You aren't carrying anything.";
        text += "\n";
    }
    else
    {
        text += "Inventory:\n\n";

        for (i = 0; i < NUM_OBJECT_CLASSES; i++)
        {
            if (inv_class2[i] != 0)
            {
                switch (i)
                {
                case OBJ_WEAPONS:    text += "Hand weapons";    break;
                case OBJ_MISSILES:   text += "Missiles";        break;
                case OBJ_ARMOUR:     text += "Armour";          break;
                case OBJ_WANDS:      text += "Magical devices"; break;
                case OBJ_FOOD:       text += "Comestibles";     break;
                case OBJ_SCROLLS:    text += "Scrolls";         break;
                case OBJ_JEWELLERY:  text += "Jewellery";       break;
                case OBJ_POTIONS:    text += "Potions";         break;
                case OBJ_BOOKS:      text += "Books";           break;
                case OBJ_STAVES:     text += "Magical staves";  break;
                case OBJ_ORBS:       text += "Orbs of Power";   break;
                case OBJ_MISCELLANY: text += "Miscellaneous";   break;
                case OBJ_CORPSES:    text += "Carrion";         break;

                default:
                    die("Bad item class");
                }
                text += "\n";

                for (j = 0; j < ENDOFPACK; j++)
                {
                    if (you.inv[j].defined() && you.inv[j].base_type == i)
                    {
                        text += " ";
                        text += you.inv[j].name(true, DESC_INVENTORY_EQUIP);

                        inv_count--;

                        int ival = -1;
                        if (par.show_prices)
                        {
                            text += make_stringf(" (%d gold)",
                                        ival = item_value(you.inv[j], true));
                        }

                        if (origin_describable(you.inv[j])
                            && _dump_item_origin(you.inv[j], ival))
                        {
                            text += "\n" "   (" + origin_desc(you.inv[j]) + ")";
                        }

                        if (is_dumpable_artefact(you.inv[j], false)
                            || Options.dump_book_spells
                               && you.inv[j].base_type == OBJ_BOOKS)
                        {
                            text2 = get_item_description(you.inv[j],
                                                          false,
                                                          true);

                            text += munge_description(text2);
                        }
                        else
                        {
                            text += "\n";
                        }
                    }
                }
            }
        }
    }
    text += "\n\n";
}

//---------------------------------------------------------------
//
// dump_skills
//
//---------------------------------------------------------------
static void _sdump_skills(dump_params &par)
{
    std::string &text(par.text);

    text += "   Skills:\n";

    dump_skills(text);
    text += "\n";
    text += "\n";
}

//---------------------------------------------------------------
//
// Return string of the i-th spell type, with slash if required
//
//---------------------------------------------------------------
static std::string spell_type_shortname(int spell_class, bool slash)
{
    std::string ret;

    if (slash)
        ret = "/";

    ret += spelltype_short_name(spell_class);

    return (ret);
}

//---------------------------------------------------------------
//
// dump_spells
//
//---------------------------------------------------------------
static void _sdump_spells(dump_params &par)
{
    std::string &text(par.text);

// This array helps output the spell types in the traditional order.
// this can be tossed as soon as I reorder the enum to the traditional order {dlb}
    const int spell_type_index[] =
    {
        SPTYP_HOLY,
        SPTYP_POISON,
        SPTYP_FIRE,
        SPTYP_ICE,
        SPTYP_EARTH,
        SPTYP_AIR,
        SPTYP_CONJURATION,
        SPTYP_HEXES,
        SPTYP_CHARMS,
        SPTYP_DIVINATION,
        SPTYP_TRANSLOCATION,
        SPTYP_SUMMONING,
        SPTYP_TRANSMUTATION,
        SPTYP_NECROMANCY,
        0
    };

    int spell_levels = player_spell_levels();

    std::string verb = par.se? "had" : "have";

    if (spell_levels == 1)
        text += "You " + verb + " one spell level left.";
    else if (spell_levels == 0)
    {
        verb = par.se? "couldn't" : "cannot";

        text += "You " + verb + " memorise any spells.";
    }
    else
    {
        if (par.se)
            text += "You had ";
        else
            text += "You have ";
        text += make_stringf("%d spell levels left.", spell_levels);
    }

    text += "\n";

    if (!you.spell_no)
    {
        verb = par.se? "didn't" : "don't";

        text += "You " + verb + " know any spells.\n\n";
    }
    else
    {
        verb = par.se? "knew" : "know";

        text += "You " + verb + " the following spells:\n\n";

        text += " Your Spells              Type           Power        Success   Level  Hunger" "\n";

        for (int j = 0; j < 52; j++)
        {
            const char letter = index_to_letter(j);
            const spell_type spell  = get_spell_by_letter(letter);

            if (spell != SPELL_NO_SPELL)
            {
                std::string spell_line;

                spell_line += letter;
                spell_line += " - ";
                spell_line += gettext(spell_title(spell));

                spell_line = chop_string(spell_line, 24);
                spell_line += "  ";

                bool already = false;

                for (int i = 0; spell_type_index[i] != 0; i++)
                {
                    if (spell_typematch(spell, spell_type_index[i]))
                    {
                        spell_line += spell_type_shortname(spell_type_index[i],
                                                           already);
                        already = true;
                    }
                }

                spell_line = chop_string(spell_line, 41);

                spell_line += spell_power_string(spell);

                spell_line = chop_string(spell_line, 54);

                spell_line += gettext(failure_rate_to_string(spell_fail(spell)));

                spell_line = chop_string(spell_line, 66);

                spell_line += make_stringf("%-5d", spell_difficulty(spell));

                spell_line += spell_hunger_string(spell);
                spell_line += "\n";

                text += spell_line;
            }
        }
        text += "\n\n";
    }
}

static void _sdump_kills(dump_params &par)
{
    par.text += you.kills->kill_info();
}

static std::string _sdump_kills_place_info(PlaceInfo place_info,
                                          std::string name = "")
{
    std::string out;

    if (name.empty())
        name = place_info.short_name();

    unsigned int global_total_kills = 0;
    for (int i = 0; i < KC_NCATEGORIES; i++)
        global_total_kills += you.global_info.mon_kill_num[i];

    unsigned int total_kills = 0;
    for (int i = 0; i < KC_NCATEGORIES; i++)
        total_kills += place_info.mon_kill_num[i];

    // Skip places where nothing was killed.
    if (total_kills == 0)
        return "";

    float a, b, c, d, e, f;

    a = TO_PERCENT(total_kills, global_total_kills);
    b = TO_PERCENT(place_info.mon_kill_num[KC_YOU],
                   you.global_info.mon_kill_num[KC_YOU]);
    c = TO_PERCENT(place_info.mon_kill_num[KC_FRIENDLY],
                   you.global_info.mon_kill_num[KC_FRIENDLY]);
    d = TO_PERCENT(place_info.mon_kill_num[KC_OTHER],
                   you.global_info.mon_kill_num[KC_OTHER]);
    e = TO_PERCENT(place_info.mon_kill_exp,
                   you.global_info.mon_kill_exp);

    f = float(place_info.mon_kill_exp) / place_info.levels_seen;

    out =
        make_stringf("%14s | %5.1f | %5.1f | %5.1f | %5.1f | %5.1f |"
                     " %13.1f\n",
                     name.c_str(), a, b, c , d, e, f);

    out = replace_all(out, " nan ", " N/A ");

    return out;
}

static void _sdump_kills_by_place(dump_params &par)
{
    std::string &text(par.text);

    std::vector<PlaceInfo> all_visited =
        you.get_all_place_info(true);

    std::string result = "";

    std::string header =
    "Table legend:\n"
    " A = Kills in this place as a percentage of kills in entire the game.\n"
    " B = Kills by you in this place as a percentage of kills by you in\n"
    "     the entire game.\n"
    " C = Kills by friends in this place as a percentage of kills by\n"
    "     friends in the entire game.\n"
    " D = Other kills in this place as a percentage of other kills in the\n"
    "     entire game.\n"
    " E = Experience gained in this place as a percentage of experience\n"
    "     gained in the entire game.\n"
    " F = Experience gained in this place divided by the number of levels of\n"
    "     this place that you have seen.\n\n";

    header += "               ";
    header += "    A       B       C       D       E               F\n";
    header += "               ";
    header += "+-------+-------+-------+-------+-------+----------------------\n";

    std::string footer = "               ";
    footer += "+-------+-------+-------+-------+-------+----------------------\n";

    result += _sdump_kills_place_info(you.global_info, "Total");

    for (unsigned int i = 0; i < all_visited.size(); i++)
    {
        PlaceInfo pi = all_visited[i];
        result += _sdump_kills_place_info(pi);
    }

    if (!result.empty())
        text += header + result + footer + "\n";
}

static void _sdump_overview(dump_params &par)
{
    std::string overview =
        formatted_string::parse_string(overview_description_string(false));
    trim_string(overview);
    par.text += overview;
    par.text += "\n\n";
}

static void _sdump_hiscore(dump_params &par)
{
    if (!par.se)
        return;

    std::string hiscore = hiscores_format_single_long(*(par.se), true);
    trim_string(hiscore);
    par.text += hiscore;
    par.text += "\n\n";
}

static void _sdump_monster_list(dump_params &par)
{
    std::string monlist = mpr_monster_list(par.se);
    trim_string(monlist);
    par.text += monlist;
    par.text += "\n\n";
}

static void _sdump_vault_list(dump_params &par)
{
    if (par.full_id || par.se
#ifdef WIZARD
        || you.wizard
#endif
     )
    {
        par.text += "Vault maps used:\n\n";
        par.text += dump_vault_maps();
    }
}

static bool _sort_by_first(std::pair<int, FixedVector<int, 28> > a,
                           std::pair<int, FixedVector<int, 28> > b)
{
    for (int i = 0; i < 27; i++)
    {
        if (a.second[i] > b.second[i])
            return true;
        else if (a.second[i] < b.second[i])
            return false;
    }
    return false;
}

static void _sdump_spell_usage(dump_params &par)
{
    std::vector<std::pair<int, FixedVector<int, 28> > > usage_vec;
    for (std::map<std::pair<caction_type, int>, FixedVector<int, 27> >::const_iterator sp = you.action_count.begin(); sp != you.action_count.end(); ++sp)
    {
        if (sp->first.first != CACT_CAST)
            continue;
        FixedVector<int, 28> v;
        v[27] = 0;
        for (int i = 0; i < 27; i++)
        {
            v[i] = sp->second[i];
            v[27] += v[i];
        }
        usage_vec.push_back(std::pair<int, FixedVector<int, 28> >(sp->first.second, v));
    }
    if (usage_vec.empty())
        return;
    std::sort(usage_vec.begin(), usage_vec.end(), _sort_by_first);

    int max_lt = (std::min<int>(you.max_level, 27) - 1) / 3;

    // Don't show both a total and 1..3 when there's only one tier.
    if (max_lt)
        max_lt++;

    par.text += make_stringf("\n%-24s", "Spells cast");
    for (int lt = 0; lt < max_lt; lt++)
        par.text += make_stringf(" | %2d-%2d", lt * 3 + 1, lt * 3 + 3);
    par.text += make_stringf(" || %5s", "total");
    par.text += "\n-------------------------";
    for (int lt = 0; lt < max_lt; lt++)
        par.text += "+-------";
    par.text += "++-------\n";

    for (std::vector<std::pair<int, FixedVector<int, 28> > >::const_iterator sp =
         usage_vec.begin(); sp != usage_vec.end(); ++sp)
    {
        par.text += chop_string(spell_title((spell_type)sp->first), 24);

        for (int lt = 0; lt < max_lt; lt++)
        {
            int ltotal = 0;
            for (int i = lt * 3; i < lt * 3 + 3; i++)
                ltotal += sp->second[i];
            if (ltotal)
                par.text += make_stringf(" |%6d", ltotal);
            else
                par.text += " |      ";
        }
        par.text += make_stringf(" ||%6d", sp->second[27]);
        par.text += "\n";
    }

    par.text += "\n";
}

static std::string _describe_action(caction_type type)
{
    switch (type)
    {
    case CACT_MELEE:
        return "Melee";
    case CACT_FIRE:
        return " Fire";
    case CACT_THROW:
        return "Throw";
    case CACT_CAST:
        return " Cast";
    case CACT_INVOKE:
        return "Invok";
    case CACT_EVOKE:
        return "Evoke";
    case CACT_USE:
        return "  Use";
    default:
        return "Error";
    }
}

static std::string _describe_action_subtype(caction_type type, int subtype)
{
    switch (type)
    {
    case CACT_MELEE:
    case CACT_FIRE:
        return ((subtype == -1) ? "Unarmed"
                : uppercase_first(item_base_name(OBJ_WEAPONS, subtype)));
    case CACT_THROW:
        return uppercase_first(item_base_name(OBJ_MISSILES, subtype));
    case CACT_CAST:
        return spell_title((spell_type)subtype);
    case CACT_INVOKE:
        return ability_name((ability_type)subtype);
    case CACT_EVOKE:
        switch ((evoc_type)subtype)
        {
        case EVOC_ABIL:
            return "Ability";
        case EVOC_WAND:
            return "Wand";
        case EVOC_ROD:
            return "Rod";
        case EVOC_DECK:
            return "Deck";
        case EVOC_MISC:
            return "Miscellaneous";
        default:
            return "Error";
        }
    case CACT_USE:
        return uppercase_first(base_type_string((object_class_type)subtype));
    default:
        return "Error";
    }
}

static void _sdump_action_counts(dump_params &par)
{
    if (you.action_count.empty())
        return;
    int max_lt = (std::min<int>(you.max_level, 27) - 1) / 3;

    // Don't show both a total and 1..3 when there's only one tier.
    if (max_lt)
        max_lt++;

    par.text += make_stringf("\n%-24s", "Action");
    for (int lt = 0; lt < max_lt; lt++)
        par.text += make_stringf(" | %2d-%2d", lt * 3 + 1, lt * 3 + 3);
    par.text += make_stringf(" || %5s", "total");
    par.text += "\n-------------------------";
    for (int lt = 0; lt < max_lt; lt++)
        par.text += "+-------";
    par.text += "++-------\n";

    for (int cact = 0; cact < NUM_CACTIONS; cact++)
    {
        std::vector<std::pair<int, FixedVector<int, 28> > > action_vec;
        for (std::map<std::pair<caction_type, int>, FixedVector<int, 27> >::const_iterator ac = you.action_count.begin(); ac != you.action_count.end(); ++ac)
        {
            if (ac->first.first != cact)
                continue;
            FixedVector<int, 28> v;
            v[27] = 0;
            for (int i = 0; i < 27; i++)
            {
                v[i] = ac->second[i];
                v[27] += v[i];
            }
            action_vec.push_back(std::pair<int, FixedVector<int, 28> >(ac->first.second, v));
        }
        std::sort(action_vec.begin(), action_vec.end(), _sort_by_first);

        for (std::vector<std::pair<int, FixedVector<int, 28> > >::const_iterator ac = action_vec.begin(); ac != action_vec.end(); ++ac)
        {
            if (ac == action_vec.begin())
            {
                par.text += _describe_action(caction_type(cact));
                par.text += ": ";
            }
            else
                par.text += "       ";
            par.text += chop_string(_describe_action_subtype(caction_type(cact), ac->first), 17);
            for (int lt = 0; lt < max_lt; lt++)
            {
                int ltotal = 0;
                for (int i = lt * 3; i < lt * 3 + 3; i++)
                    ltotal += ac->second[i];
                if (ltotal)
                    par.text += make_stringf(" |%6d", ltotal);
                else
                    par.text += " |      ";
            }
            par.text += make_stringf(" ||%6d", ac->second[27]);
            par.text += "\n";
        }
    }
    par.text += "\n";
}

static void _sdump_mutations(dump_params &par)
{
    std::string &text(par.text);

    if (how_mutated(true, false))
    {
        text += "\n";
        text += (formatted_string::parse_string(describe_mutations()));
        text += "\n\n";
    }
}

// ========================================================================
//      Public Functions
// ========================================================================

const char *hunger_level(void)
{
    const bool vamp = (you.species == SP_VAMPIRE);

    return ((you.hunger <= 1000) ? (vamp ? "bloodless" : "starving") :
            (you.hunger <= 1533) ? (vamp ? "near bloodless" : "near starving") :
            (you.hunger <= 2066) ? (vamp ? "very thirsty" : "very hungry") :
            (you.hunger <= 2600) ? (vamp ? "thirsty" : "hungry") :
            (you.hunger <  7000) ? (vamp ? "not thirsty" : "not hungry") :
            (you.hunger <  9000) ? "full" :
            (you.hunger < 11000) ? "very full"
                                 : (vamp ? "almost alive" : "completely stuffed"));
}

std::string morgue_directory()
{
    std::string dir = (!Options.morgue_dir.empty() ? Options.morgue_dir :
                       !SysEnv.crawl_dir.empty()   ? SysEnv.crawl_dir
                                                   : "");

    if (!dir.empty() && dir[dir.length() - 1] != FILE_SEPARATOR)
        dir += FILE_SEPARATOR;

    return (dir);
}

void dump_map(FILE *fp, bool debug, bool dist)
{
    if (debug)
    {
        // Write the whole map out without checking for mappedness. Handy
        // for debugging level-generation issues.
        for (int y = 0; y < GYM; ++y)
        {
            for (int x = 0; x < GXM; ++x)
            {
                if (you.pos() == coord_def(x, y))
                    fputc('@', fp);
                else if (testbits(env.pgrid[x][y], FPROP_HIGHLIGHT))
                    fputc('?', fp);
                else if (dist && grd[x][y] == DNGN_FLOOR
                         && travel_point_distance[x][y] > 0
                         && travel_point_distance[x][y] < 10)
                {
                    fputc('0' + travel_point_distance[x][y], fp);
                }
                else
                {
                    fputs(OUTS(stringize_glyph(
                               get_feature_def(grd[x][y]).symbol)), fp);
                }
            }
            fputc('\n', fp);
        }
    }
    else
    {
        int min_x = GXM-1, max_x = 0, min_y = GYM-1, max_y = 0;

        for (int i = X_BOUND_1; i <= X_BOUND_2; i++)
            for (int j = Y_BOUND_1; j <= Y_BOUND_2; j++)
                if (env.map_knowledge[i][j].known())
                {
                    if (i > max_x) max_x = i;
                    if (i < min_x) min_x = i;
                    if (j > max_y) max_y = j;
                    if (j < min_y) min_y = j;
                }

        for (int y = min_y; y <= max_y; ++y)
        {
            for (int x = min_x; x <= max_x; ++x)
            {
                fputs(OUTS(stringize_glyph(
                           get_cell_glyph(coord_def(x, y)).ch)), fp);
            }

            fputc('\n', fp);
        }
    }
}

void dump_map(const char* fname, bool debug, bool dist)
{
    FILE* fp = fopen_replace(fname);
    if (!fp)
        return;

    dump_map(fp, debug, dist);

    fclose(fp);
}

static bool _write_dump(const std::string &fname, dump_params &par,
                        bool print_dump_path)
{
    bool succeeded = false;

    std::string file_name = morgue_directory();

    file_name += strip_filename_unsafe_chars(fname);

    StashTrack.update_corpses();

    std::string stash_file_name;
    stash_file_name = file_name;
    stash_file_name += ".lst";
    StashTrack.dump(stash_file_name.c_str(), par.full_id);

    std::string map_file_name = file_name + ".map";
    dump_map(map_file_name.c_str());

    file_name += ".txt";
    FILE *handle = fopen_replace(file_name.c_str());

    dprf("File name: %s", file_name.c_str());

    if (handle != NULL)
    {
        fputs(OUTS(par.text), handle);
        fclose(handle);
        succeeded = true;
        if (print_dump_path)
#ifdef DGAMELAUNCH
            mprf("Char dumped successfully.");
#else
            mprf("Char dumped to '%s'.", file_name.c_str());
#endif
    }
    else
        mprf(MSGCH_ERROR, "Error opening file '%s'", file_name.c_str());

    return (succeeded);
}

void display_notes()
{
    formatted_scroller scr;
    scr.set_flags(MF_START_AT_END);
    scr.set_tag("notes");
    scr.set_highlighter(new MenuHighlighter);
    scr.set_title(new MenuEntry("Turn   | Place    | Note"));
    for (unsigned int i = 0; i < note_list.size(); ++i)
    {
        std::string prefix = note_list[i].describe(true, true, false);
        std::string suffix = note_list[i].describe(false, false, true);
        if (suffix.empty())
            continue;

        int spaceleft = get_number_of_cols() - prefix.length() - 1;
        if (spaceleft <= 0)
            return;

        linebreak_string(suffix, spaceleft);
        std::vector<std::string> parts = split_string("\n", suffix);
        if (parts.empty()) // Disregard pure-whitespace notes.
            continue;

        scr.add_entry(new MenuEntry(prefix + parts[0]));
        for (unsigned int j = 1; j < parts.size(); ++j)
        {
            scr.add_entry(new MenuEntry(std::string(prefix.length()-2, ' ') +
                                        std::string("| ") + parts[j]));
        }
    }
    scr.show();
    redraw_screen();
}

#ifdef DGL_WHEREIS
///////////////////////////////////////////////////////////////////////////
// whereis player
void whereis_record(const char *status)
{
    const std::string file_name =
        morgue_directory()
        + strip_filename_unsafe_chars(you.your_name)
        + std::string(".where");

    if (FILE *handle = fopen_replace(file_name.c_str()))
    {
        // no need to bother with supporting ancient charsets for DGL
        fprintf(handle, "%s:status=%s\n",
                xlog_status_line().c_str(),
                status? status : "");
        fclose(handle);
    }
}
#endif


///////////////////////////////////////////////////////////////////////////////
// Turn timestamps
//
// For DGL installs, write a timestamp at regular intervals into a file in
// the morgue directory. The timestamp file is named
// "timestamp-<player>-<starttime>.ts". All timestamps are standard Unix
// time_t, but currently only the low 4 bytes are saved even on systems
// with 64-bit time_t.
//
// Timestamp files are append only, and Crawl will check and handle cases
// where a previous Crawl process crashed at a higher turn count on the same
// game.
//
// Having timestamps associated with the game allows for much easier seeking
// within Crawl ttyrecs by external tools such as FooTV.

#ifdef DGL_TURN_TIMESTAMPS

#include "syscalls.h"
#include <sys/stat.h>

// File-format version for timestamp files. Crawl will never append to a
const uint32_t DGL_TIMESTAMP_VERSION = 1;
const int VERSION_SIZE = sizeof(DGL_TIMESTAMP_VERSION);
const int TIMESTAMP_SIZE = sizeof(uint32_t);

// Returns the name of the timestamp file based on the morgue_dir,
// character name and the game start time.
std::string dgl_timestamp_filename()
{
    const std::string filename =
        ("timestamp-" + you.your_name + "-" + make_file_time(you.birth_time));
    return morgue_directory() + strip_filename_unsafe_chars(filename) + ".ts";
}

// Returns true if the given file exists and is not a timestamp file
// of a known version.
bool dgl_unknown_timestamp_file(const std::string &filename)
{
    if (FILE *inh = fopen_u(filename.c_str(), "rb"))
    {
        reader r(inh);
        const uint32_t file_version = unmarshallInt(r);
        fclose(inh);
        return (file_version != DGL_TIMESTAMP_VERSION);
    }
    return (false);
}

// Returns a filehandle to use to write turn timestamps, NULL if
// timestamps should not be written.
FILE *dgl_timestamp_filehandle()
{
    static FILE *timestamp_file;
    static bool opened_file = false;
    if (!opened_file)
    {
        opened_file = true;

        const std::string filename = dgl_timestamp_filename();
        // First check if there's already a timestamp file. If it exists
        // but has a different version, we cannot safely modify it, so bail.
        if (!dgl_unknown_timestamp_file(filename))
            timestamp_file = fopen_u(filename.c_str(), "ab");
    }
    return timestamp_file;
}

// Records a timestamp in the .ts file at the given offset. If no timestamp
// file exists, a new file will be created.
void dgl_record_timestamp(unsigned long file_offset, time_t time)
{
    static bool timestamp_first_write = true;
    if (FILE *ftimestamp = dgl_timestamp_filehandle())
    {
        writer w(dgl_timestamp_filename(), ftimestamp, true);
        if (timestamp_first_write)
        {
            unsigned long ts_size = file_size(ftimestamp);
            if (!ts_size)
            {
                marshallInt(w, DGL_TIMESTAMP_VERSION);
                ts_size += sizeof(DGL_TIMESTAMP_VERSION);
            }

            // It's possible that the file we want to write is already
            // larger than the offset we expect if the game previously
            // crashed. When the game crashes, turn count is
            // effectively rewound to the point of the last save. In
            // such cases, we should not add timestamps until we reach
            // the correct turn count again.
            if (ts_size && ts_size > file_offset)
                return;

            if (file_offset > ts_size)
            {
                const int backlog =
                    (file_offset - ts_size) / TIMESTAMP_SIZE;
                for (int i = 0; i < backlog; ++i)
                    marshallInt(w, 0);
            }

            timestamp_first_write = false;
        }
        fseek(ftimestamp, 0, SEEK_END);
        // [ds] FIXME: Eventually switch to 8 byte timestamps.
        marshallInt(w, static_cast<uint32_t>(time));
        fflush(ftimestamp);
    }
}

// Record timestamps every so many turns:
const int TIMESTAMP_TURN_INTERVAL = 100;
// Stop recording timestamps after this turncount.
const long TIMESTAMP_TURN_MAX = 500000L;
void dgl_record_timestamp(long turn)
{
    if (turn && turn < TIMESTAMP_TURN_MAX && !(turn % TIMESTAMP_TURN_INTERVAL))
    {
        const time_t now = time(NULL);
        const unsigned long offset =
            (VERSION_SIZE +
             (turn / TIMESTAMP_TURN_INTERVAL - 1) * TIMESTAMP_SIZE);
        dgl_record_timestamp(offset, now);
    }
}

#endif

// Records a timestamp for the current player turn if appropriate.
void record_turn_timestamp()
{
#ifdef DGL_TURN_TIMESTAMPS
    if (crawl_state.need_save)
        dgl_record_timestamp(you.num_turns);
#endif
}
