/**
 * @file
 * @brief Misc functions.
**/

#include "AppHdr.h"

#include "itemname.h"

#include <sstream>
#include <iomanip>
#include <ctype.h>
#include <string.h>

#include "clua.h"

#include "externs.h"
#include "options.h"

#include "artefact.h"
#include "colour.h"
#include "decks.h"
#include "food.h"
#include "goditem.h"
#include "invent.h"
#include "item_use.h"
#include "itemprop.h"
#include "items.h"
#include "macro.h"
#include "makeitem.h"
#include "mon-util.h"
#include "notes.h"
#include "player.h"
#include "religion.h"
#include "quiver.h"
#include "shopping.h"
#include "showsymb.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-summoning.h"
#include "state.h"
#include "stuff.h"
#include "env.h"
#include "transform.h"


static bool _is_random_name_space(char let);
static bool _is_random_name_vowel(char let);

static char _random_vowel(int seed);
static char _random_cons(int seed);

static bool translate_flag;

static const char * check_gettext(const char *ptr)
{
    if(translate_flag == false || ptr[0] == 0) return ptr;
    return gettext(ptr);
}

bool is_vowel(const ucs_t chr)
{
    const char low = towlower(chr);
    return (low == 'a' || low == 'e' || low == 'i' || low == 'o' || low == 'u');
}

// quant_name is useful since it prints out a different number of items
// than the item actually contains.
std::string quant_name(const item_def &item, int quant,
                       description_level_type des, bool terse)
{
    // item_name now requires a "real" item, so we'll mangle a tmp
    item_def tmp = item;
    tmp.quantity = quant;

    return tmp.name(false, des, terse);
}

std::string item_def::name(bool allow_translate,
                           description_level_type descrip,
                           bool terse, bool ident,
                           bool with_inscription,
                           bool quantity_in_words,
                           iflags_t ignore_flags) const
{
    if (crawl_state.game_is_arena())
    {
        ignore_flags |= ISFLAG_KNOW_PLUSES | ISFLAG_KNOW_CURSE
                        | ISFLAG_COSMETIC_MASK;
    }

    if (descrip == DESC_NONE)
        return ("");

    translate_flag = allow_translate;

    std::ostringstream buff;

    const std::string auxname = this->name_aux(descrip, terse, ident,
                                               with_inscription, ignore_flags);

    const bool startvowel     = is_vowel(auxname[0]);

    if (descrip == DESC_INVENTORY_EQUIP || descrip == DESC_INVENTORY)
    {
        if (in_inventory(*this)) // actually in inventory
        {
            buff << index_to_letter(this->link);
            if (terse)
                buff << ") ";
            else
                buff << " - ";
        }
        else
            descrip = DESC_CAP_A;
    }

    if (base_type == OBJ_BOOKS && (ident || item_type_known(*this))
        && book_has_title(*this))
    {
        if (descrip != DESC_DBNAME)
            descrip = DESC_PLAIN;
    }

    if (terse && descrip != DESC_DBNAME)
        descrip = DESC_PLAIN;

    monster_flag_type corpse_flags;

    if (base_type == OBJ_CORPSES && is_named_corpse(*this)
#if TAG_MAJOR_VERSION == 32
        && !(((corpse_flags = (int64_t)props[CORPSE_NAME_TYPE_KEY])
#else
        && !(((corpse_flags = props[CORPSE_NAME_TYPE_KEY].get_int64())
#endif
               & MF_NAME_SPECIES)
             && !(corpse_flags & MF_NAME_DEFINITE))
             && !(corpse_flags & MF_NAME_SUFFIX)
        && !starts_with(get_corpse_name(*this), "shaped "))
    {
        switch (descrip)
        {
        case DESC_CAP_A:
        case DESC_CAP_YOUR:
            descrip = DESC_CAP_THE;
            break;

        case DESC_NOCAP_A:
        case DESC_NOCAP_YOUR:
        case DESC_NOCAP_ITS:
        case DESC_INVENTORY_EQUIP:
        case DESC_INVENTORY:
            descrip = DESC_NOCAP_THE;
            break;

        default:
            break;
        }
    }

    if (item_is_orb(*this)
        || (ident || item_type_known(*this))
            && (this->base_type == OBJ_MISCELLANY
                   && this->sub_type == MISC_HORN_OF_GERYON
                || is_artefact(*this)))
    {
        // Artefacts always get "the" unless we just want the plain name.
        switch (descrip)
        {
        case DESC_CAP_A:
        case DESC_CAP_YOUR:
        case DESC_CAP_THE:
            buff << "The ";
            break;
        case DESC_NOCAP_A:
        case DESC_NOCAP_YOUR:
        case DESC_NOCAP_THE:
        case DESC_NOCAP_ITS:
        case DESC_INVENTORY_EQUIP:
        case DESC_INVENTORY:
            buff << "the ";
            break;
        default:
        case DESC_PLAIN:
            break;
        }
    }
    else if (this->quantity > 1)
    {
        switch (descrip)
        {
        case DESC_CAP_THE:    buff << "The "; break;
        case DESC_NOCAP_THE:  buff << "the "; break;
        case DESC_CAP_YOUR:   buff << "Your "; break;
        case DESC_NOCAP_YOUR: buff << "your "; break;
        case DESC_NOCAP_ITS:  buff << "its "; break;
        case DESC_CAP_A:
        case DESC_NOCAP_A:
        case DESC_INVENTORY_EQUIP:
        case DESC_INVENTORY:
        case DESC_PLAIN:
        default:
            break;
        }

        if (descrip != DESC_BASENAME && descrip != DESC_QUALNAME
            && descrip != DESC_DBNAME)
        {
            if (quantity_in_words)
                buff << number_in_words(this->quantity) << " ";
            else
                buff << this->quantity << " ";
        }
    }
    else
    {
        switch (descrip)
        {
        case DESC_CAP_THE:    buff << "The "; break;
        case DESC_NOCAP_THE:  buff << "the "; break;
        case DESC_CAP_A:      buff << (startvowel ? "An " : "A "); break;

        case DESC_CAP_YOUR:   buff << "Your "; break;
        case DESC_NOCAP_YOUR: buff << "your "; break;
        case DESC_NOCAP_ITS:  buff << "its "; break;

        case DESC_NOCAP_A:
        case DESC_INVENTORY_EQUIP:
        case DESC_INVENTORY:
                              buff << (startvowel ? "an " : "a "); break;

        case DESC_PLAIN:
        default:
            break;
        }
    }

    buff << auxname;

    bool equipped = false;
    if (descrip == DESC_INVENTORY_EQUIP)
    {
        equipment_type eq = item_equip_slot(*this);
        if (eq != EQ_NONE)
        {
            if (you.melded[eq])
                buff << " (melded)";
            else
            {
                switch (eq)
                {
                case EQ_WEAPON:
                    if (this->base_type == OBJ_WEAPONS || item_is_staff(*this))
                        buff << " (weapon)";
                    else if (you.species == SP_FELID)
                        buff << " (in mouth)";
                    else
                        buff << " (in " << you.hand_name(false) << ")";
                    break;
                case EQ_CLOAK:
                case EQ_HELMET:
                case EQ_GLOVES:
                case EQ_BOOTS:
                case EQ_SHIELD:
                case EQ_BODY_ARMOUR:
                    buff << " (worn)";
                    break;
                case EQ_LEFT_RING:
                case EQ_RIGHT_RING:
                    buff << " (";
                    buff << (eq == EQ_LEFT_RING ? "left" : "right");
                    buff << " ";
                    buff << you.hand_name(false);
                    buff << ")";
                    break;
                case EQ_AMULET:
                    if (you.species == SP_OCTOPODE)
                        buff << " (around mantle)";
                    else
                        buff << " (around neck)";
                    break;
                case EQ_RING_ONE:
                case EQ_RING_TWO:
                case EQ_RING_THREE:
                case EQ_RING_FOUR:
                    if (you.form == TRAN_SPIDER)
                    {
                        buff << " (on front leg)";
                        break;
                    }
                case EQ_RING_FIVE:
                case EQ_RING_SIX:
                case EQ_RING_SEVEN:
                case EQ_RING_EIGHT:
                    if (you.form == TRAN_SPIDER)
                    {
                        buff << " (on hind leg)";
                        break;
                    }
                    buff << " (on tentacle)";
                    break;
                default:
                    die("Item in an invalid slot");
                }
            }
        }
        else if (item_is_quivered(*this))
        {
            equipped = true;
            buff << " (quivered)";
        }
    }

    if (descrip != DESC_BASENAME && descrip != DESC_DBNAME && with_inscription)
    {
        const bool  tried  =  !ident && !equipped && item_type_tried(*this);
        std::string tried_str;

        if (tried)
        {
            item_type_id_state_type id_type = get_ident_type(*this);

            if (id_type == ID_MON_TRIED_TYPE)
                tried_str = "tried by monster";
            else if (id_type == ID_TRIED_ITEM_TYPE)
            {
                tried_str = "tried on item";
                if (base_type == OBJ_SCROLLS)
                {
                    if (sub_type == SCR_IDENTIFY
                        && you.type_id_props.exists("SCR_ID"))
                    {
                        tried_str = "tried on " +
                                    you.type_id_props["SCR_ID"].get_string();
                    }
                    else if (sub_type == SCR_RECHARGING
                             && you.type_id_props.exists("SCR_RC"))
                    {
                        tried_str = "tried on " +
                                    you.type_id_props["SCR_RC"].get_string();
                    }
                    else if (sub_type == SCR_ENCHANT_ARMOUR
                             && you.type_id_props.exists("SCR_EA"))
                    {
                        tried_str = "tried on " +
                                    you.type_id_props["SCR_EA"].get_string();
                    }
                }
            }
            else
                tried_str = "tried";
        }

        if (with_inscription && !(this->inscription.empty()))
        {
            buff << " {";
            if (tried)
                buff << tried_str << ", ";
            buff << this->inscription << "}";
        }
        else if (tried)
            buff << " {" << tried_str << "}";
    }

    return buff.str();
}

static bool _missile_brand_is_prefix(special_missile_type brand)
{
    switch (brand)
    {
    case SPMSL_POISONED:
    case SPMSL_CURARE:
    case SPMSL_EXPLODING:
    case SPMSL_STEEL:
    case SPMSL_SILVER:
        return (true);
    default:
        return (false);
    }
}

static bool _missile_brand_is_postfix(special_missile_type brand)
{
    return (brand != SPMSL_NORMAL && !_missile_brand_is_prefix(brand));
}

enum mbn_type
{
    MBN_TERSE, // terse brand name
    MBN_NAME,  // brand name for item naming (adj for prefix, noun for postfix)
    MBN_BRAND, // plain brand name
};

const char* missile_brand_name(special_missile_type brand, mbn_type t)
{
    switch (brand)
    {
    case SPMSL_FLAME:
        return M_("flame");
    case SPMSL_FROST:
        return M_("frost");
    case SPMSL_POISONED:
        return (t == MBN_NAME ? M_("poisoned") : M_("poison"));
    case SPMSL_CURARE:
        return (t == MBN_NAME ? M_("curare-tipped") : M_("curare"));
    case SPMSL_EXPLODING:
        return (t == MBN_TERSE ? M_("explode") : M_("exploding"));
    case SPMSL_STEEL:
        return M_("steel");
    case SPMSL_SILVER:
        return M_("silver");
    case SPMSL_PARALYSIS:
        return M_("paralysis");
    case SPMSL_SLOW:
        return (t == MBN_TERSE ? M_("slow") : M_("slowing"));
    case SPMSL_SLEEP:
        return (t == MBN_TERSE ? M_("sleep") : M_("sleeping"));
    case SPMSL_CONFUSION:
        return (t == MBN_TERSE ? M_("conf") : M_("confusion"));
    case SPMSL_SICKNESS:
        return (t == MBN_TERSE ? M_("sick") : M_("sickness"));
    case SPMSL_RAGE:
        return M_("frenzy");
    case SPMSL_RETURNING:
        return (t == MBN_TERSE ? M_("return") : M_("returning"));
    case SPMSL_CHAOS:
        return M_("chaos");
    case SPMSL_PENETRATION:
        return (t == MBN_TERSE ? M_("penet") : M_("penetration"));
    case SPMSL_REAPING:
        return (t == MBN_TERSE ? M_("reap") : M_("reaping"));
    case SPMSL_DISPERSAL:
        return (t == MBN_TERSE ? M_("disperse") : M_("dispersal"));
    case SPMSL_NORMAL:
    default:
        return "";
    }
}

const char* weapon_brand_name(const item_def& item, bool terse)
{
    switch (get_weapon_brand(item))
    {
    case SPWPN_NORMAL: return "";
    case SPWPN_FLAMING: return ((terse) ? N_(" (flame)") : N_(" of flaming"));
    case SPWPN_FREEZING: return ((terse) ? N_(" (freeze)") : N_(" of freezing"));
    case SPWPN_HOLY_WRATH: return ((terse) ? N_(" (holy)") : N_(" of holy wrath"));
    case SPWPN_ELECTROCUTION: return ((terse) ? N_(" (elec)"):N_(" of electrocution"));
    case SPWPN_ORC_SLAYING: return ((terse) ? N_(" (slay orc)"):N_(" of orc slaying"));
    case SPWPN_DRAGON_SLAYING: return ((terse) ? N_(" (slay drac)"):N_(" of dragon slaying"));
    case SPWPN_VENOM: return ((terse) ? N_(" (venom)") : N_(" of venom"));
    case SPWPN_PROTECTION: return ((terse) ? N_(" (protect)") : N_(" of protection"));
    case SPWPN_EVASION: return ((terse) ? N_(" (evade)") : N_(" of evasion"));
    case SPWPN_DRAINING: return ((terse) ? N_(" (drain)") : N_(" of draining"));
    case SPWPN_SPEED: return ((terse) ? N_(" (speed)") : N_(" of speed"));
    case SPWPN_PAIN: return ((terse) ? N_(" (pain)") : N_(" of pain"));
    case SPWPN_DISTORTION: return ((terse) ? N_(" (distort)") : N_(" of distortion"));
    case SPWPN_REACHING: return ((terse) ? N_(" (reach)") : N_(" of reaching"));
    case SPWPN_RETURNING: return ((terse) ? N_(" (return)") : N_(" of returning"));

    case SPWPN_VAMPIRICISM:
        return ((terse) ? N_(" (vamp)") : ""); // non-terse already handled

    case SPWPN_VORPAL:
        if (is_range_weapon(item))
            return ((terse) ? N_(" (velocity)") : N_(" of velocity"));
        else
        {
            switch (get_vorpal_type(item))
            {
            case DVORP_CRUSHING: return ((terse) ? N_(" (crush)") :N_(" of crushing"));
            case DVORP_SLICING:  return ((terse) ? N_(" (slice)") : N_(" of slicing"));
            case DVORP_PIERCING: return ((terse) ? N_(" (pierce)"):N_(" of piercing"));
            case DVORP_CHOPPING: return ((terse) ? N_(" (chop)") : N_(" of chopping"));
            case DVORP_SLASHING: return ((terse) ? N_(" (slash)") :N_(" of slashing"));
            case DVORP_STABBING: return ((terse) ? N_(" (stab)") : N_(" of stabbing"));
            default:             return "";
            }
        }
    case SPWPN_ANTIMAGIC: return terse ? N_(" (antimagic)") : ""; // non-terse
                                                      // handled elsewhere

    // ranged weapon brands
    case SPWPN_FLAME: return ((terse) ? N_(" (flame)") : N_(" of flame"));
    case SPWPN_FROST: return ((terse) ? N_(" (frost)") : N_(" of frost"));
    case SPWPN_PENETRATION: return ((terse) ? N_(" (penet)") : N_(" of penetration"));
    case SPWPN_REAPING: return ((terse) ? N_(" (reap)") : N_(" of reaping"));

    // both ranged and non-ranged
    case SPWPN_CHAOS: return ((terse) ? N_(" (chaos)") : N_(" of chaos"));

    // randart brands
    default: return "";
    }
}


const char* armour_ego_name(const item_def& item, bool terse)
{
    if (!terse)
    {
        switch (get_armour_ego_type(item))
        {
        case SPARM_NORMAL:            return "";
        case SPARM_RUNNING:           return M_("running");
        case SPARM_FIRE_RESISTANCE:   return M_("fire resistance");
        case SPARM_COLD_RESISTANCE:   return M_("cold resistance");
        case SPARM_POISON_RESISTANCE: return M_("poison resistance");
        case SPARM_SEE_INVISIBLE:     return M_("see invisible");
        case SPARM_DARKNESS:          return M_("darkness");
        case SPARM_STRENGTH:          return M_("strength");
        case SPARM_DEXTERITY:         return M_("dexterity");
        case SPARM_INTELLIGENCE:      return M_("intelligence");
        case SPARM_PONDEROUSNESS:     return M_("ponderousness");
        case SPARM_LEVITATION:        return M_("levitation");
        case SPARM_MAGIC_RESISTANCE:  return M_("magic resistance");
        case SPARM_PROTECTION:        return M_("protection");
        case SPARM_STEALTH:           return M_("stealth");
        case SPARM_RESISTANCE:        return M_("resistance");
        case SPARM_POSITIVE_ENERGY:   return M_("positive energy");
        case SPARM_ARCHMAGI:          return M_("the Archmagi");
        case SPARM_PRESERVATION:      return M_("preservation");
        case SPARM_REFLECTION:        return M_("reflection");
        case SPARM_SPIRIT_SHIELD:     return M_("spirit shield");
        case SPARM_ARCHERY:           return M_("archery");
        default:                      return M_("bugginess");
        }
    }
    else
    {
        switch (get_armour_ego_type(item))
        {
        case SPARM_NORMAL:            return "";
        case SPARM_RUNNING:           return " {run}";
        case SPARM_FIRE_RESISTANCE:   return " {rF+}";
        case SPARM_COLD_RESISTANCE:   return " {rC+}";
        case SPARM_POISON_RESISTANCE: return " {rPois}";
        case SPARM_SEE_INVISIBLE:     return " {SInv}";
        case SPARM_DARKNESS:          return " {darkness}";
        case SPARM_STRENGTH:          return " {Str+3}";
        case SPARM_DEXTERITY:         return " {Dex+3}";
        case SPARM_INTELLIGENCE:      return " {Int+3}";
        case SPARM_PONDEROUSNESS:     return " {ponderous}";
        case SPARM_LEVITATION:        return " {Lev}";
        case SPARM_MAGIC_RESISTANCE:  return " {MR}";
        case SPARM_PROTECTION:        return " {AC+3}";
        case SPARM_STEALTH:           return " {Stlth+}";
        case SPARM_RESISTANCE:        return " {rC+ rF+}";
        case SPARM_POSITIVE_ENERGY:   return " {rN+}";
        case SPARM_ARCHMAGI:          return " {Archmagi}";
        case SPARM_PRESERVATION:      return " {rCorr, Cons}";
        case SPARM_REFLECTION:        return " {rflct}";
        case SPARM_SPIRIT_SHIELD:     return " {Spirit}";
        case SPARM_ARCHERY:           return " {archer}";
        default:                      return " {buggy}";
        }
    }
}

const char* wand_type_name(int wandtype)
{
    switch (static_cast<wand_type>(wandtype))
    {
    case WAND_FLAME:           return M_("flame");
    case WAND_FROST:           return M_("frost");
    case WAND_SLOWING:         return M_("slowing");
    case WAND_HASTING:         return M_("hasting");
    case WAND_MAGIC_DARTS:     return M_("magic darts");
    case WAND_HEALING:         return M_("healing");
    case WAND_PARALYSIS:       return M_("paralysis");
    case WAND_FIRE:            return M_("fire");
    case WAND_COLD:            return M_("cold");
    case WAND_CONFUSION:       return M_("confusion");
    case WAND_INVISIBILITY:    return M_("invisibility");
    case WAND_DIGGING:         return M_("digging");
    case WAND_FIREBALL:        return M_("fireball");
    case WAND_TELEPORTATION:   return M_("teleportation");
    case WAND_LIGHTNING:       return M_("lightning");
    case WAND_POLYMORPH_OTHER: return M_("polymorph other");
    case WAND_ENSLAVEMENT:     return M_("enslavement");
    case WAND_DRAINING:        return M_("draining");
    case WAND_RANDOM_EFFECTS:  return M_("random effects");
    case WAND_DISINTEGRATION:  return M_("disintegration");
    default:                   return M_("bugginess");
    }
}

static const char* wand_secondary_string(int s)
{
    switch (s)
    {
    case 0:  return "";
    case 1:  return M_("jewelled ");
    case 2:  return M_("curved ");
    case 3:  return M_("long ");
    case 4:  return M_("short ");
    case 5:  return M_("twisted ");
    case 6:  return M_("crooked ");
    case 7:  return M_("forked ");
    case 8:  return M_("shiny ");
    case 9:  return M_("blackened ");
    case 10: return M_("tapered ");
    case 11: return M_("glowing ");
    case 12: return M_("worn ");
    case 13: return M_("encrusted ");
    case 14: return M_("runed ");
    case 15: return M_("sharpened ");
    default: return M_("buggily ");
    }
}

static const char* wand_primary_string(int p)
{
    switch (p)
    {
    case 0:  return M_("iron");
    case 1:  return M_("brass");
    case 2:  return M_("bone");
    case 3:  return M_("wooden");
    case 4:  return M_("copper");
    case 5:  return M_("gold");
    case 6:  return M_("silver");
    case 7:  return M_("bronze");
    case 8:  return M_("ivory");
    case 9:  return M_("glass");
    case 10: return M_("lead");
    case 11: return M_("fluorescent");
    default: return M_("buggy");
    }
}

static const char* potion_type_name(int potiontype)
{
    switch (static_cast<potion_type>(potiontype))
    {
    case POT_HEALING:           return M_("healing");
    case POT_HEAL_WOUNDS:       return M_("heal wounds");
    case POT_SPEED:             return M_("speed");
    case POT_MIGHT:             return M_("might");
    case POT_AGILITY:           return M_("agility");
    case POT_BRILLIANCE:        return M_("brilliance");
    case POT_GAIN_STRENGTH:     return M_("gain strength");
    case POT_GAIN_DEXTERITY:    return M_("gain dexterity");
    case POT_GAIN_INTELLIGENCE: return M_("gain intelligence");
    case POT_LEVITATION:        return M_("levitation");
    case POT_POISON:            return M_("poison");
    case POT_SLOWING:           return M_("slowing");
    case POT_PARALYSIS:         return M_("paralysis");
    case POT_CONFUSION:         return M_("confusion");
    case POT_INVISIBILITY:      return M_("invisibility");
    case POT_PORRIDGE:          return M_("porridge");
    case POT_DEGENERATION:      return M_("degeneration");
    case POT_DECAY:             return M_("decay");
    case POT_WATER:             return M_("water");
    case POT_EXPERIENCE:        return M_("experience");
    case POT_MAGIC:             return M_("magic");
    case POT_RESTORE_ABILITIES: return M_("restore abilities");
    case POT_STRONG_POISON:     return M_("strong poison");
    case POT_BERSERK_RAGE:      return M_("berserk rage");
    case POT_CURE_MUTATION:     return M_("cure mutation");
    case POT_MUTATION:          return M_("mutation");
    case POT_BLOOD:             return M_("blood");
    case POT_BLOOD_COAGULATED:  return M_("coagulated blood");
    case POT_RESISTANCE:        return M_("resistance");
    case POT_FIZZING:           return M_("fizzing liquid");
    default:                    return M_("bugginess");
    }
}

static const char* scroll_type_name(int scrolltype)
{
    switch (static_cast<scroll_type>(scrolltype))
    {
#if TAG_MAJOR_VERSION == 32
    case SCR_PAPER:              return M_("old paper");
#endif
    case SCR_IDENTIFY:           return M_("identify");
    case SCR_TELEPORTATION:      return M_("teleportation");
    case SCR_FEAR:               return M_("fear");
    case SCR_NOISE:              return M_("noise");
    case SCR_REMOVE_CURSE:       return M_("remove curse");
    case SCR_DETECT_CURSE:       return M_("detect curse");
    case SCR_SUMMONING:          return M_("summoning");
    case SCR_ENCHANT_WEAPON_I:   return M_("enchant weapon I");
    case SCR_ENCHANT_ARMOUR:     return M_("enchant armour");
    case SCR_TORMENT:            return M_("torment");
    case SCR_RANDOM_USELESSNESS: return M_("random uselessness");
    case SCR_CURSE_WEAPON:       return M_("curse weapon");
    case SCR_CURSE_ARMOUR:       return M_("curse armour");
    case SCR_CURSE_JEWELLERY:    return M_("curse jewellery");
    case SCR_IMMOLATION:         return M_("immolation");
    case SCR_BLINKING:           return M_("blinking");
    case SCR_MAGIC_MAPPING:      return M_("magic mapping");
    case SCR_FOG:                return M_("fog");
    case SCR_ACQUIREMENT:        return M_("acquirement");
    case SCR_ENCHANT_WEAPON_II:  return M_("enchant weapon II");
    case SCR_VORPALISE_WEAPON:   return M_("vorpalise weapon");
    case SCR_RECHARGING:         return M_("recharging");
    case SCR_ENCHANT_WEAPON_III: return M_("enchant weapon III");
    case SCR_HOLY_WORD:          return M_("holy word");
    case SCR_VULNERABILITY:      return M_("vulnerability");
    case SCR_SILENCE:            return M_("silence");
    case SCR_AMNESIA:            return M_("amnesia");
    default:                     return M_("bugginess");
    }
}

static const char* jewellery_type_name(int jeweltype)
{
    switch (static_cast<jewellery_type>(jeweltype))
    {
    case RING_REGENERATION:          return M_("ring of regeneration");
    case RING_PROTECTION:            return M_("ring of protection");
    case RING_PROTECTION_FROM_FIRE:  return M_("ring of protection from fire");
    case RING_POISON_RESISTANCE:     return M_("ring of poison resistance");
    case RING_PROTECTION_FROM_COLD:  return M_("ring of protection from cold");
    case RING_STRENGTH:              return M_("ring of strength");
    case RING_SLAYING:               return M_("ring of slaying");
    case RING_SEE_INVISIBLE:         return M_("ring of see invisible");
    case RING_INVISIBILITY:          return M_("ring of invisibility");
    case RING_HUNGER:                return M_("ring of hunger");
    case RING_TELEPORTATION:         return M_("ring of teleportation");
    case RING_EVASION:               return M_("ring of evasion");
    case RING_SUSTAIN_ABILITIES:     return M_("ring of sustain abilities");
    case RING_SUSTENANCE:            return M_("ring of sustenance");
    case RING_DEXTERITY:             return M_("ring of dexterity");
    case RING_INTELLIGENCE:          return M_("ring of intelligence");
    case RING_WIZARDRY:              return M_("ring of wizardry");
    case RING_MAGICAL_POWER:         return M_("ring of magical power");
    case RING_LEVITATION:            return M_("ring of levitation");
    case RING_LIFE_PROTECTION:       return M_("ring of life protection");
    case RING_PROTECTION_FROM_MAGIC: return M_("ring of protection from magic");
    case RING_FIRE:                  return M_("ring of fire");
    case RING_ICE:                   return M_("ring of ice");
    case RING_TELEPORT_CONTROL:      return M_("ring of teleport control");
    case AMU_RAGE:              return M_("amulet of rage");
    case AMU_CLARITY:           return M_("amulet of clarity");
    case AMU_WARDING:           return M_("amulet of warding");
    case AMU_RESIST_CORROSION:  return M_("amulet of resist corrosion");
    case AMU_THE_GOURMAND:      return M_("amulet of the gourmand");
    case AMU_CONSERVATION:      return M_("amulet of conservation");
    case AMU_CONTROLLED_FLIGHT: return M_("amulet of controlled flight");
    case AMU_INACCURACY:        return M_("amulet of inaccuracy");
    case AMU_RESIST_MUTATION:   return M_("amulet of resist mutation");
    case AMU_GUARDIAN_SPIRIT:   return M_("amulet of guardian spirit");
    case AMU_FAITH:             return M_("amulet of faith");
    case AMU_STASIS:            return M_("amulet of stasis");
    default: return M_("buggy jewellery");
    }
}

static const char* ring_secondary_string(int s)
{
    switch (s)
    {
    case 1:  return M_("encrusted ");
    case 2:  return M_("glowing ");
    case 3:  return M_("tubular ");
    case 4:  return M_("runed ");
    case 5:  return M_("blackened ");
    case 6:  return M_("scratched ");
    case 7:  return M_("small ");
    case 8:  return M_("large ");
    case 9:  return M_("twisted ");
    case 10: return M_("shiny ");
    case 11: return M_("notched ");
    case 12: return M_("knobbly ");
    default: return "";
    }
}



static const char* ring_primary_string(int p)
{
    switch (p)
    {
    case 0:  return M_("wooden");
    case 1:  return M_("silver");
    case 2:  return M_("golden");
    case 3:  return M_("iron");
    case 4:  return M_("steel");
    case 5:  return M_("bronze");
    case 6:  return M_("brass");
    case 7:  return M_("copper");
    case 8:  return M_("granite");
    case 9:  return M_("ivory");
    case 10: return M_("bone");
    case 11: return M_("marble");
    case 12: return M_("jade");
    case 13: return M_("glass");
    default: return M_("buggy");
    }
}

static const char* amulet_secondary_string(int s)
{
    switch (s)
    {
    case 0:  return M_("dented ");
    case 1:  return M_("square ");
    case 2:  return M_("thick ");
    case 3:  return M_("thin ");
    case 4:  return M_("runed ");
    case 5:  return M_("blackened ");
    case 6:  return M_("glowing ");
    case 7:  return M_("small ");
    case 8:  return M_("large ");
    case 9:  return M_("twisted ");
    case 10: return M_("tiny ");
    case 11: return M_("triangular ");
    case 12: return M_("lumpy ");
    default: return "";
    }
}

static const char* amulet_primary_string(int p)
{
    switch (p)
    {
    case 0:  return M_("zirconium");
    case 1:  return M_("sapphire");
    case 2:  return M_("golden");
    case 3:  return M_("emerald");
    case 4:  return M_("garnet");
    case 5:  return M_("bronze");
    case 6:  return M_("brass");
    case 7:  return M_("copper");
    case 8:  return M_("ruby");
    case 9:  return M_("ivory");
    case 10: return M_("bone");
    case 11: return M_("platinum");
    case 12: return M_("jade");
    case 13: return M_("fluorescent");
    default: return M_("buggy");
    }
}

const char* rune_type_name(int p)
{
    switch (static_cast<rune_type>(p))
    {
    case RUNE_DIS:         return M_("iron");
    case RUNE_GEHENNA:     return M_("obsidian");
    case RUNE_COCYTUS:     return M_("icy");
    case RUNE_TARTARUS:    return M_("bone");
    case RUNE_SLIME_PITS:  return M_("slimy");
    case RUNE_VAULTS:      return M_("silver");
    case RUNE_SNAKE_PIT:   return M_("serpentine");
    case RUNE_ELVEN_HALLS: return M_("elven");
    case RUNE_TOMB:        return M_("golden");
    case RUNE_SWAMP:       return M_("decaying");
    case RUNE_SHOALS:      return M_("barnacled");
    case RUNE_SPIDER_NEST: return M_("gossamer");
    case RUNE_FOREST:      return M_("mossy");

    // pandemonium and abyss runes:
    case RUNE_DEMONIC:     return M_("demonic");
    case RUNE_ABYSSAL:     return M_("abyssal");

    // special pandemonium runes:
    case RUNE_MNOLEG:      return M_("glowing");
    case RUNE_LOM_LOBON:   return M_("magical");
    case RUNE_CEREBOV:     return M_("fiery");
    case RUNE_GLOORX_VLOQ: return M_("dark");
    default:               return "buggy";
    }
}

static const char* deck_rarity_name(deck_rarity_type rarity)
{
    switch (rarity)
    {
    case DECK_RARITY_COMMON:    return M_("plain");
    case DECK_RARITY_RARE:      return M_("ornate");
    case DECK_RARITY_LEGENDARY: return M_("legendary");
    default:                    return M_("buggy rarity");
    }
}

static const char* misc_type_name(int type, bool known)
{
    if (!known)
    {
        if (type >= MISC_FIRST_DECK && type <= MISC_LAST_DECK)
            return "deck of cards";
        if (type == MISC_CRYSTAL_BALL_OF_ENERGY
            || type == MISC_CRYSTAL_BALL_OF_SEEING)
        {
            return M_("crystal ball");
        }
    }

    switch (static_cast<misc_item_type>(type))
    {
    case MISC_DECK_OF_ESCAPE:      return M_("deck of escape");
    case MISC_DECK_OF_DESTRUCTION: return M_("deck of destruction");
    case MISC_DECK_OF_DUNGEONS:    return M_("deck of dungeons");
    case MISC_DECK_OF_SUMMONING:   return M_("deck of summonings");
    case MISC_DECK_OF_WONDERS:     return M_("deck of wonders");
    case MISC_DECK_OF_PUNISHMENT:  return M_("deck of punishment");
    case MISC_DECK_OF_WAR:         return M_("deck of war");
    case MISC_DECK_OF_CHANGES:     return M_("deck of changes");
    case MISC_DECK_OF_DEFENCE:     return M_("deck of defence");

    case MISC_CRYSTAL_BALL_OF_ENERGY:   return M_("crystal ball of energy");
#if TAG_MAJOR_VERSION == 32
    case MISC_CRYSTAL_BALL_OF_FIXATION: return "old crystal ball";
#endif
    case MISC_CRYSTAL_BALL_OF_SEEING:   return M_("crystal ball of seeing");
    case MISC_BOX_OF_BEASTS:            return M_("box of beasts");
    case MISC_EMPTY_EBONY_CASKET:       return M_("empty ebony casket");
    case MISC_AIR_ELEMENTAL_FAN:        return M_("air elemental fan");
    case MISC_LAMP_OF_FIRE:             return M_("lamp of fire");
    case MISC_LANTERN_OF_SHADOWS:       return M_("lantern of shadows");
    case MISC_HORN_OF_GERYON:           return M_("horn of Geryon");
    case MISC_DISC_OF_STORMS:           return M_("disc of storms");
    case MISC_BOTTLED_EFREET:           return M_("bottled efreet");
    case MISC_STONE_OF_EARTH_ELEMENTALS:
        return M_("stone of earth elementals");
    case MISC_QUAD_DAMAGE:              return M_("quad damage");

    case MISC_RUNE_OF_ZOT:
    default:
        return M_("buggy miscellaneous item");
    }
}

static const char* book_secondary_string(int s)
{
    switch (s)
    {
    case 0:  return "";
    case 1:  return M_("chunky ");
    case 2:  return M_("thick ");
    case 3:  return M_("thin ");
    case 4:  return M_("wide ");
    case 5:  return M_("glowing ");
    case 6:  return M_("dog-eared ");
    case 7:  return M_("oblong ");
    case 8:  return M_("runed ");
    case 9:  return "";
    case 10: return "";
    case 11: return "";
    default: return "buggily ";
    }
}

static const char* book_primary_string(int p)
{
    switch (p)
    {
    case 0:  return M_("paperback ");
    case 1:  return M_("hardcover ");
    case 2:  return M_("leatherbound ");
    case 3:  return M_("metal-bound ");
    case 4:  return M_("papyrus ");
    case 5:  return "";
    case 6:  return "";
    default: return "buggy ";
    }
}

static const char* _book_type_name(int booktype)
{
    switch (static_cast<book_type>(booktype))
    {
    case BOOK_MINOR_MAGIC:
#if TAG_MAJOR_VERSION == 32
    case BOOK_MINOR_MAGIC_II:
    case BOOK_MINOR_MAGIC_III:
#endif
        return M_("Minor Magic");
#if TAG_MAJOR_VERSION == 32
    case BOOK_CONJURATIONS_I:
#endif
    case BOOK_CONJURATIONS_II:
        return M_("Conjurations");
    case BOOK_FLAMES:                 return M_("Flames");
    case BOOK_FROST:                  return M_("Frost");
    case BOOK_SUMMONINGS:             return M_("Summonings");
    case BOOK_FIRE:                   return M_("Fire");
    case BOOK_ICE:                    return M_("Ice");
    case BOOK_SPATIAL_TRANSLOCATIONS: return M_("Spatial Translocations");
    case BOOK_ENCHANTMENTS:           return M_("Enchantments");
    case BOOK_TEMPESTS:               return M_("the Tempests");
    case BOOK_DEATH:                  return M_("Death");
    case BOOK_HINDERANCE:             return M_("Hinderance");
    case BOOK_CHANGES:                return M_("Changes");
    case BOOK_TRANSFIGURATIONS:       return M_("Transfigurations");
    case BOOK_WAR_CHANTS:             return M_("War Chants");
    case BOOK_CLOUDS:                 return M_("Clouds");
    case BOOK_NECROMANCY:             return M_("Necromancy");
    case BOOK_CALLINGS:               return M_("Callings");
    case BOOK_MALEDICT:               return M_("Maledictions");
    case BOOK_AIR:                    return M_("Air");
    case BOOK_SKY:                    return M_("the Sky");
    case BOOK_WARP:                   return M_("the Warp");
    case BOOK_ENVENOMATIONS:          return M_("Envenomations");
    case BOOK_ANNIHILATIONS:          return M_("Annihilations");
    case BOOK_UNLIFE:                 return M_("Unlife");
    case BOOK_CONTROL:                return M_("Control");
    case BOOK_MUTATIONS:              return M_("Morphology");
    case BOOK_GEOMANCY:               return M_("Geomancy");
    case BOOK_EARTH:                  return M_("the Earth");
    case BOOK_WIZARDRY:               return M_("Wizardry");
    case BOOK_POWER:                  return M_("Power");
    case BOOK_CANTRIPS:               return M_("Cantrips");
    case BOOK_PARTY_TRICKS:           return M_("Party Tricks");
    case BOOK_STALKING:               return M_("Stalking");
    case BOOK_DEBILITATION:           return M_("Debilitation");
    case BOOK_DRAGON:                 return M_("the Dragon");
    case BOOK_BURGLARY:               return M_("Burglary");
    case BOOK_DREAMS:                 return M_("Dreams");
    case BOOK_ALCHEMY:                return M_("Alchemy");
    case BOOK_BEASTS:                 return M_("Beasts");
    case BOOK_RANDART_LEVEL:          return M_("Fixed Level");
    case BOOK_RANDART_THEME:          return M_("Fixed Theme");
    default:                          return M_("Bugginess");
    }
}

static const char* staff_secondary_string(int p)
{
    switch (p) // general descriptions
    {
    case 0:  return "crooked ";
    case 1:  return "knobbly ";
    case 2:  return "weird ";
    case 3:  return "gnarled ";
    case 4:  return "thin ";
    case 5:  return "curved ";
    case 6:  return "twisted ";
    case 7:  return "thick ";
    case 8:  return "long ";
    case 9:  return "short ";
    default: return "buggily ";
    }
}

static const char* staff_primary_string(int p)
{
    switch (p) // special attributes
    {
    case 0:  return "glowing ";
    case 1:  return "jewelled ";
    case 2:  return "runed ";
    case 3:  return "smoking ";
    default: return "buggy ";
    }
}

static const char* staff_type_name(int stafftype)
{
    switch (static_cast<stave_type>(stafftype))
    {
    // staves
    case STAFF_WIZARDRY:    return "wizardry";
    case STAFF_POWER:       return "power";
    case STAFF_FIRE:        return "fire";
    case STAFF_COLD:        return "cold";
    case STAFF_POISON:      return "poison";
    case STAFF_ENERGY:      return "energy";
    case STAFF_DEATH:       return "death";
    case STAFF_CONJURATION: return "conjuration";
    case STAFF_ENCHANTMENT: return "enchantment";
    case STAFF_AIR:         return "air";
    case STAFF_EARTH:       return "earth";
    case STAFF_SUMMONING:   return "summoning";

    // rods
    case STAFF_SPELL_SUMMONING: return "summoning";
    case STAFF_CHANNELING:      return "channeling";
    case STAFF_WARDING:         return "warding";
    case STAFF_SMITING:         return "smiting";
    case STAFF_STRIKING:        return "striking";
    case STAFF_DEMONOLOGY:      return "demonology";
    case STAFF_VENOM:           return "venom";

    case STAFF_DESTRUCTION_I:
    case STAFF_DESTRUCTION_II:
    case STAFF_DESTRUCTION_III:
    case STAFF_DESTRUCTION_IV:
        return "destruction";

    default: return "bugginess";
    }
}

const char* racial_description_string(const item_def& item, bool terse)
{
    switch (get_equip_race(item))
    {
    case ISFLAG_ORCISH:
        return terse ? N_("orc ") : N_("orcish ");
    case ISFLAG_ELVEN:
        return terse ? N_("elf ") : N_("elven ");
    case ISFLAG_DWARVEN:
        return terse ? N_("dwarf ") : N_("dwarven ");
    default:
        return "";
    }
}

std::string base_type_string (const item_def &item, bool known)
{
    return base_type_string(item.base_type, known);
}

std::string base_type_string (object_class_type type, bool known)
{
    switch (type)
    {
    case OBJ_WEAPONS: return "weapon";
    case OBJ_MISSILES: return "missile";
    case OBJ_ARMOUR: return "armour";
    case OBJ_WANDS: return "wand";
    case OBJ_FOOD: return "food";
    case OBJ_SCROLLS: return "scroll";
    case OBJ_JEWELLERY: return "jewellery";
    case OBJ_POTIONS: return "potion";
    case OBJ_BOOKS: return "book";
    case OBJ_STAVES: return "staff";
    case OBJ_ORBS: return "orb";
    case OBJ_MISCELLANY: return "miscellaneous";
    case OBJ_CORPSES: return "corpse";
    case OBJ_GOLD: return "gold";
    default: return "";
    }
}

std::string sub_type_string (const item_def &item, bool known)
{
    return sub_type_string(item.base_type, item.sub_type, known, item.plus);
}

std::string sub_type_string(object_class_type type, int sub_type,
                            bool known, int plus)
{
    switch (type)
    {
    case OBJ_WEAPONS:  // deliberate fall through, as XXX_prop is a local
    case OBJ_MISSILES: // variable to itemprop.cc.
    case OBJ_ARMOUR:
        return item_base_name(type, sub_type);
    case OBJ_WANDS: return wand_type_name(sub_type);
    case OBJ_FOOD: return food_type_name(sub_type);
    case OBJ_SCROLLS: return scroll_type_name(sub_type);
    case OBJ_JEWELLERY: return jewellery_type_name(sub_type);
    case OBJ_POTIONS: return potion_type_name(sub_type);
    case OBJ_BOOKS:
    {
        if (sub_type == BOOK_MANUAL)
        {
            std::string bookname = "manual of ";
            bookname += skill_name(static_cast<skill_type>(plus));
            return bookname;
        }
        else if (sub_type == BOOK_NECRONOMICON)
            return "Necronomicon";
        else if (sub_type == BOOK_GRAND_GRIMOIRE)
            return "Grand Grimoire";
        else if (sub_type == BOOK_DESTRUCTION)
            return "tome of Destruction";
        else if (sub_type == BOOK_YOUNG_POISONERS)
            return "Young Poisoner's Handbook";

        return _book_type_name(sub_type);
    }
    case OBJ_STAVES: return staff_type_name(sub_type);
    case OBJ_MISCELLANY:
        if (sub_type == MISC_RUNE_OF_ZOT)
            return "rune of Zot";
        else
            return misc_type_name(sub_type, known);
    // these repeat as base_type_string
    case OBJ_ORBS: return "orb of Zot"; break;
    case OBJ_CORPSES: return "corpse"; break;
    case OBJ_GOLD: return "gold"; break;
    default: return "";
    }
}

std::string ego_type_string (const item_def &item)
{
    switch (item.base_type)
    {
    case OBJ_ARMOUR:
        return armour_ego_name(item, false);
    case OBJ_WEAPONS:
        // this is specialcased out of weapon_brand_name
        // ("vampiric hand axe", etc)
        if (get_weapon_brand(item) == SPWPN_VAMPIRICISM)
            return "vampiricism";
        else if (get_weapon_brand(item) == SPWPN_ANTIMAGIC)
            return "anti-magic";
        else if (get_weapon_brand(item) != SPWPN_NORMAL)
            return std::string(weapon_brand_name(item, false)).substr(4);
        else
            return "";
    case OBJ_MISSILES:
        return missile_brand_name(get_ammo_brand(item), MBN_BRAND);
    default:
        return "";
    }
}

// gcc (and maybe the C standard) says that if you output
// 0, even with showpos set, you get 0, not +0. This is a workaround.
static void output_with_sign(std::ostream& os, int val)
{
    if (val >= 0)
        os << '+';
    os << val;
}

// Note that "terse" is only currently used for the "in hand" listing on
// the game screen.
std::string item_def::name_aux(description_level_type desc,
                               bool terse, bool ident, bool with_inscription,
                               iflags_t ignore_flags) const
{
    // Shortcuts
    const int item_typ   = sub_type;
    const int it_plus    = plus;
    const int item_plus2 = plus2;

    const bool know_type = ident || item_type_known(*this);

    const bool dbname   = (desc == DESC_DBNAME);
    const bool basename = (desc == DESC_BASENAME || (dbname && !know_type));
    const bool qualname = (desc == DESC_QUALNAME);

    const bool know_curse =
        !basename && !qualname && !dbname
        && !testbits(ignore_flags, ISFLAG_KNOW_CURSE)
        && (ident || item_ident(*this, ISFLAG_KNOW_CURSE));

    const bool __know_pluses =
        !basename && !qualname && !dbname
        && !testbits(ignore_flags, ISFLAG_KNOW_PLUSES)
        && (ident || item_ident(*this, ISFLAG_KNOW_PLUSES));

    const bool know_brand =
        !basename && !qualname && !dbname
        && !testbits(ignore_flags, ISFLAG_KNOW_TYPE)
        && (ident || item_type_known(*this));

    const bool know_ego = know_brand;

    // Display runed/glowing/embroidered etc?
    // Only display this if brand is unknown or item is unbranded.
    const bool show_cosmetic = !__know_pluses && !terse && !basename
        && !qualname && !dbname
        && (!know_brand || !special)
        && !(ignore_flags & ISFLAG_COSMETIC_MASK);

    // So that show_cosmetic won't be affected by ignore_flags.
    const bool know_pluses = __know_pluses
        && !testbits(ignore_flags, ISFLAG_KNOW_PLUSES);

    const bool know_racial = !(ignore_flags & ISFLAG_RACIAL_MASK);

    const bool need_plural = !basename && !dbname;

    std::ostringstream buff;

    switch (base_type)
    {
    case OBJ_WEAPONS:
        if (know_curse && !terse)
        {
            // We don't bother printing "uncursed" if the item is identified
            // for pluses (its state should be obvious), this is so that
            // the weapon name is kept short (there isn't a lot of room
            // for the name on the main screen).  If you're going to change
            // this behaviour, *please* make it so that there is an option
            // that maintains this behaviour. -- bwr
            // Nor for artefacts. Again, the state should be obvious. --jpeg
            if (cursed())
                buff << check_gettext("cursed ");
            else if (Options.show_uncursed && !know_pluses
                     && (!know_type || !is_artefact(*this)))
                buff << check_gettext("uncursed ");
        }

        if (know_pluses)
        {
            if ((terse && it_plus == item_plus2) || sub_type == WPN_BLOWGUN)
                output_with_sign(buff, it_plus);
            else
            {
                output_with_sign(buff, it_plus);
                buff << ',';
                output_with_sign(buff, item_plus2);
            }
            buff << " ";
        }

        if (is_artefact(*this) && !dbname)
        {
            buff << check_gettext(get_artefact_name(*this).c_str());
            break;
        }
        else if (flags & ISFLAG_BLESSED_WEAPON && !dbname)
        {   // Since Angels and Daevas can get blessed base items, we
            // need a separate flag for this, so they can still have
            // their holy weapons.
            buff << check_gettext("Blessed ");
            if (weapon_skill(*this) == SK_MACES_FLAILS)
                buff << check_gettext(M_("Scourge"));
            else if (weapon_skill(*this) == SK_POLEARMS)
                buff << check_gettext(M_("Trishula"));
            else
                buff << check_gettext(M_("Blade"));
            break;
        }

        if (show_cosmetic)
        {
            switch (get_equip_desc(*this))
            {
            case ISFLAG_RUNED:
                if (!testbits(ignore_flags, ISFLAG_RUNED))
                    buff << check_gettext(M_("runed "));
                break;
            case ISFLAG_GLOWING:
                if (!testbits(ignore_flags, ISFLAG_GLOWING))
                    buff << check_gettext(M_("glowing "));
                break;
            }
        }

        if (!basename && !dbname && know_racial)
        {
            // Always give racial type (it does have game effects).
            buff << racial_description_string(*this, terse);
        }

        if (know_brand && !terse)
        {
            int brand = get_weapon_brand(*this);
            if (brand == SPWPN_VAMPIRICISM)
                buff << check_gettext(M_("vampiric "));
            else if (brand == SPWPN_ANTIMAGIC)
                buff << check_gettext(M_("anti-magic "));
        }

        /// 1. 무기 이름 2. 브랜드( of flaming)의 번역형 3. 저주가 걸려있으면 (curse)
        buff << make_stringf(check_gettext("%s%s%s"),
            check_gettext(item_base_name(*this).c_str()),
            know_brand ? weapon_brand_name(*this, terse) : "",
            (know_curse && cursed() && terse) ? check_gettext(" (curse)") : "");
        break;

    case OBJ_MISSILES:
    {
        special_missile_type brand  = get_ammo_brand(*this);

        if (show_cosmetic
            && get_equip_desc(*this) == ISFLAG_RUNED
            && !testbits(ignore_flags, ISFLAG_RUNED))
        {
            buff << check_gettext(M_("runed "));
        }

        if (know_brand && !terse && _missile_brand_is_prefix(brand))
            buff << missile_brand_name(brand, MBN_NAME) << ' ';

        if (know_pluses)
        {
            output_with_sign(buff, it_plus);
            buff << ' ';
        }

        if (!basename && !dbname)
            buff << racial_description_string(*this, terse);

        buff << check_gettext(ammo_name(static_cast<missile_type>(item_typ)));

        if (know_brand && brand != SPMSL_NORMAL)
        {
            if (terse)
                buff << " (" <<  missile_brand_name(brand, MBN_TERSE) << ")";
            else if (_missile_brand_is_postfix(brand))
                buff << " of " << missile_brand_name(brand, MBN_NAME);
        }

        // Show "runed" in the quiver (== terse) so you know if
        // you're quivering possibly branded ammo.
        // Ammo in quiver differs from weapon wielded since the latter
        // necessarily has a known brand.
        if (terse && !know_brand && !dbname
            && get_equip_desc(*this) == ISFLAG_RUNED
            && !testbits(ignore_flags, ISFLAG_RUNED))
        {
            buff << check_gettext(" (runed)");
        }

        break;
    }
    case OBJ_ARMOUR:
        if (know_curse && !terse)
        {
            if (cursed())
                buff << check_gettext("cursed ");
            else if (Options.show_uncursed && !know_pluses)
                buff << check_gettext("uncursed ");
        }

        if (know_pluses)
        {
            output_with_sign(buff, it_plus);
            buff << ' ';
        }

        if (item_typ == ARM_GLOVES || item_typ == ARM_BOOTS)
            buff << check_gettext("pair of ");

        // When asking for the base item name, randartism is ignored.
        if (is_artefact(*this) && !basename && !dbname)
        {
            buff << check_gettext(get_artefact_name(*this).c_str());
            break;
        }

        if (show_cosmetic)
        {
            switch (get_equip_desc(*this))
            {
            case ISFLAG_EMBROIDERED_SHINY:
                if (testbits(ignore_flags, ISFLAG_EMBROIDERED_SHINY))
                    break;
                if (item_typ == ARM_ROBE || item_typ == ARM_CLOAK
                    || item_typ == ARM_GLOVES || item_typ == ARM_BOOTS
                    || get_armour_slot(*this) == EQ_HELMET
                       && !is_hard_helmet(*this))
                {
                    buff << check_gettext(M_("embroidered "));
                }
                else if (item_typ != ARM_LEATHER_ARMOUR
                         && item_typ != ARM_ANIMAL_SKIN)
                {
                    buff << check_gettext(M_("shiny "));
                }
                else
                    buff << check_gettext(M_("dyed "));
                break;

            case ISFLAG_RUNED:
                if (!testbits(ignore_flags, ISFLAG_RUNED))
                    buff << check_gettext(M_("runed "));
                break;

            case ISFLAG_GLOWING:
                if (!testbits(ignore_flags, ISFLAG_GLOWING))
                    buff << check_gettext(M_("glowing "));
                break;
            }
        }

        if (!basename && !dbname)
        {
            // always give racial description (has game effects)
            buff << racial_description_string(*this, terse);
        }

        if (!basename && !dbname && is_hard_helmet(*this))
        {
            const short dhelm = get_helmet_desc(*this);

            buff <<
                   ((dhelm == THELM_DESC_PLAIN)    ? "" :
                    (dhelm == THELM_DESC_WINGED)   ? check_gettext(M_("winged "))  :
                    (dhelm == THELM_DESC_HORNED)   ? check_gettext(M_("horned "))  :
                    (dhelm == THELM_DESC_CRESTED)  ? check_gettext(M_("crested ")) :
                    (dhelm == THELM_DESC_PLUMED)   ? check_gettext(M_("plumed "))  :
                    (dhelm == THELM_DESC_SPIKED)   ? check_gettext(M_("spiked "))  :
                    (dhelm == THELM_DESC_VISORED)  ? check_gettext(M_("visored ")) :
                    (dhelm == THELM_DESC_GOLDEN)   ? check_gettext(M_("golden "))
                                                   : check_gettext(M_("buggy ")));
        }

        if (!basename && item_typ == ARM_GLOVES)
        {
            const short dglov = get_gloves_desc(*this);

            buff <<
                   check_gettext((dglov == TGLOV_DESC_GLOVES)    ? M_("gloves") :
                           (dglov == TGLOV_DESC_GAUNTLETS) ? M_("gauntlets") :
                           (dglov == TGLOV_DESC_BRACERS)   ? M_("bracers") :
                                                             M_("bug-ridden gloves"));
        }
        else
            buff << item_base_name(*this);

        if (know_ego && !is_artefact(*this))
        {
            const special_armour_type sparm = get_armour_ego_type(*this);

            if (sparm != SPARM_NORMAL)
            {
                // "naga barding of running" doesn't make any sense, and yes,
                // they are possible.
                if (sub_type == ARM_NAGA_BARDING && sparm == SPARM_RUNNING)
                    buff << (terse ? "speed" : " of speedy slithering");
                else
                {
                    if(!terse)
                        buff << make_stringf(check_gettext(" of %s"), check_gettext(armour_ego_name(*this, terse)));
                    else
                        buff << check_gettext(armour_ego_name(*this, terse));
                }
            }
        }

        if (know_curse && cursed() && terse)
            buff << " (curse)";
        break;

    case OBJ_WANDS:
        if (basename)
        {
            buff << check_gettext(M_("wand"));
            break;
        }

        if (know_type)
        {
            buff << make_stringf(check_gettext("wand of %s"), check_gettext(wand_type_name(item_typ)));
        }
        else
        {
            buff << make_stringf(check_gettext("%s%s wand"), 
                check_gettext(wand_secondary_string(this->special / NDSC_WAND_PRI)),
                check_gettext(wand_primary_string(this->special % NDSC_WAND_PRI)));
        }

        if (know_pluses)
            buff << " (" << it_plus << ")";
        else if (!dbname && with_inscription)
        {
            if (item_plus2 == ZAPCOUNT_EMPTY)
                buff << check_gettext(" {empty}");
            else if (item_plus2 == ZAPCOUNT_MAX_CHARGED)
                buff << check_gettext(" {fully recharged}");
            else if (item_plus2 == ZAPCOUNT_RECHARGED)
                buff << check_gettext(" {recharged}");
            else if (item_plus2 > 0)
                buff << make_stringf(check_gettext(" {zapped: %d}"), item_plus2);
        }
        break;

    case OBJ_POTIONS:
        if (basename)
        {
            buff << check_gettext(M_("potion"));
            break;
        }

        if (know_type)
            /// 1. healing, heal wounds 등 포션에 붙는 말이 번역되서 들어옴. 이를테면
            /// healing = 회복, heal wounds = 상처 치료
            buff << make_stringf(check_gettext("potion of %s"), 
                check_gettext(potion_type_name(item_typ)));
        else
        {
            const int pqual   = PQUAL(this->plus);
            const int pcolour = PCOLOUR(this->plus);

            static const char *potion_qualifiers[] = {
                "",  M_("bubbling "), M_("fuming "), M_("fizzy "), M_("viscous "), M_("lumpy "),
                M_("smoky "), M_("glowing "), M_("sedimented "), M_("metallic "), M_("murky "),
                M_("gluggy "), M_("oily "), M_("slimy "), M_("emulsified ")
            };
            COMPILE_CHECK(ARRAYSZ(potion_qualifiers) == PDQ_NQUALS);

            static const char *potion_colours[] = {
                M_("clear"), M_("blue"), M_("black"), M_("silvery"), M_("cyan"), M_("purple"),
                M_("orange"), M_("inky"), M_("red"), M_("yellow"), M_("green"), M_("brown"), M_("pink"),
                M_("white")
            };
            COMPILE_CHECK(ARRAYSZ(potion_colours) == PDC_NCOLOURS);

            const char *qualifier =
                (pqual < 0 || pqual >= PDQ_NQUALS) ? "bug-filled "
                                    : check_gettext(potion_qualifiers[pqual]);

            const char *clr =  (pcolour < 0 || pcolour >= PDC_NCOLOURS) ?
                                   "bogus" : check_gettext(potion_colours[pcolour]);

            /// 1. fuming, slimy 등의 수식어가 번역되서 들어옴. eg> 연기나는, 끈적이는
            /// 2. clear, blue 등의 색깔이 번역되서 들어옴. eg> 맑은, 푸른
            buff << make_stringf(check_gettext("%s%s potion"), qualifier, clr);
        }
        break;

    case OBJ_FOOD:
        switch (item_typ)
        {
        case FOOD_MEAT_RATION: buff << check_gettext(M_("meat ration")); break;
        case FOOD_BREAD_RATION: buff << check_gettext(M_("bread ration")); break;
        case FOOD_PEAR: buff << check_gettext(M_("pear")); break;
        case FOOD_APPLE: buff << check_gettext(M_("apple")); break;
        case FOOD_CHOKO: buff << check_gettext(M_("choko")); break;
        case FOOD_HONEYCOMB: buff << check_gettext(M_("honeycomb")); break;
        case FOOD_ROYAL_JELLY: buff << check_gettext(M_("royal jelly")); break;
        case FOOD_SNOZZCUMBER: buff << check_gettext(M_("snozzcumber")); break;
        case FOOD_PIZZA: buff << check_gettext(M_("slice of pizza")); break;
        case FOOD_APRICOT: buff << check_gettext(M_("apricot")); break;
        case FOOD_ORANGE: buff << check_gettext(M_("orange")); break;
        case FOOD_BANANA: buff << check_gettext(M_("banana")); break;
        case FOOD_STRAWBERRY: buff << check_gettext(M_("strawberry")); break;
        case FOOD_RAMBUTAN: buff << check_gettext(M_("rambutan")); break;
        case FOOD_LEMON: buff << check_gettext(M_("lemon")); break;
        case FOOD_GRAPE: buff << check_gettext(M_("grape")); break;
        case FOOD_SULTANA: buff << check_gettext(M_("sultana")); break;
        case FOOD_LYCHEE: buff << check_gettext(M_("lychee")); break;
        case FOOD_BEEF_JERKY: buff << check_gettext(M_("beef jerky")); break;
        case FOOD_CHEESE: buff << check_gettext(M_("cheese")); break;
        case FOOD_SAUSAGE: buff << check_gettext(M_("sausage")); break;
        case FOOD_AMBROSIA: buff << check_gettext(M_("piece of ambrosia")); break;
        case FOOD_CHUNK:
            if (!basename && !dbname)
            {
                buff << make_stringf(check_gettext("%schunk of %s flesh"), 
                    (food_is_rotten(*this) && it_plus != MONS_ROTTING_HULK) ? check_gettext(M_("rotting ")) : "",
                    check_gettext(mons_type_name(it_plus, DESC_PLAIN).c_str()));
            }
            else
                buff << check_gettext("chunk of flesh");
            break;
        }

        break;

    case OBJ_SCROLLS:
        if (basename)
        {
            buff << check_gettext(M_("scroll"));
            break;
        }

        if (know_type)
        {
            /// 1. 스크롤의 type이 번역되어 들어옴. eg> 공포, 소음, 보석 저주, 무기 저주, 무기 강화 I
            buff << make_stringf(check_gettext("scroll of %s"), check_gettext(scroll_type_name(item_typ)));
        }
        else
        {
            const uint32_t sseed =
                this->special
                + (static_cast<uint32_t>(it_plus) << 8)
                + (static_cast<uint32_t>(OBJ_SCROLLS) << 16);
            buff << make_stringf(check_gettext("scroll labeled %s"), make_name(sseed, true).c_str());
        }
        break;

    case OBJ_JEWELLERY:
    {
        if (basename)
        {
            if (jewellery_is_amulet(*this))
                buff << check_gettext(M_("amulet"));
            else
                buff << check_gettext(M_("ring"));

            break;
        }

        const bool is_randart = is_artefact(*this);

        if (know_curse)
        {
            if (cursed())
                buff << check_gettext("cursed ");
            else if (Options.show_uncursed && !terse && desc != DESC_PLAIN
                     && (!is_randart || !know_type)
                     && (!ring_has_pluses(*this) || !know_pluses)
                     // If the item is worn, its curse status is known,
                     // no need to belabour the obvious.
                     && get_equip_slot(this) == -1)
            {
                buff << check_gettext("uncursed ");
            }
        }

        if (is_randart && !dbname)
        {
            buff << check_gettext(get_artefact_name(*this).c_str());
            break;
        }

        if (know_type)
        {
            if (know_pluses && ring_has_pluses(*this))
            {
                output_with_sign(buff, it_plus);

                if (ring_has_pluses(*this) == 2)
                {
                    buff << ',';
                    output_with_sign(buff, item_plus2);
                }
                buff << ' ';
            }

            buff << check_gettext(jewellery_type_name(item_typ));
        }
        else
        {
            if (jewellery_is_amulet(*this))
            {
                buff << make_stringf(check_gettext("%s%s amulet"), 
                    check_gettext(amulet_secondary_string(this->special / NDSC_JEWEL_PRI)),
                    check_gettext(amulet_primary_string(this->special % NDSC_JEWEL_PRI)));
            }
            else  // i.e., a ring
            {
                buff << make_stringf(check_gettext("%s%s ring"),
                    check_gettext(ring_secondary_string(this->special / NDSC_JEWEL_PRI)),
                    check_gettext(ring_primary_string(this->special % NDSC_JEWEL_PRI)));
            }
        }
        break;
    }
    case OBJ_MISCELLANY:
        if (item_typ == MISC_RUNE_OF_ZOT)
        {
            if (!dbname)
                buff << check_gettext(rune_type_name(it_plus)) << " ";
            buff << check_gettext(M_("rune of Zot"));
        }
        else
        {
            if (is_deck(*this))
            {
                if (basename)
                {
                    buff << check_gettext(M_("deck of cards"));
                    break;
                }
                else if (bad_deck(*this))
                {
                    buff << "BUGGY deck of cards";
                    break;
                }
                if (!dbname)
                    buff << check_gettext(deck_rarity_name(deck_rarity(*this))) << ' ';
            }
            buff << check_gettext(misc_type_name(item_typ, know_type));
            if (is_deck(*this) && !dbname
                && (top_card_is_known(*this) || this->plus2 != 0))
            {
                buff << " {";
                // A marked deck!
                if (top_card_is_known(*this))
                    buff << check_gettext(card_name(top_card(*this)));

                // How many cards have been drawn, or how many are
                // left.
                if (this->plus2 != 0)
                {
                    if (top_card_is_known(*this))
                        buff << ", ";

                    if (this->plus2 > 0)
                        buff << check_gettext("drawn: ");
                    else
                        buff << check_gettext("left: ");

                    buff << abs(this->plus2);
                }

                buff << "}";
            }
        }
        break;

    case OBJ_BOOKS:
        if (is_random_artefact(*this) && !dbname && !basename)
        {
            buff << check_gettext(get_artefact_name(*this).c_str());
            if (!know_type)
                buff << check_gettext(M_("book"));
            break;
        }
        if (basename)
            buff << (item_typ == BOOK_MANUAL ? check_gettext(M_("manual")) : check_gettext(M_("book")));
        else if (!know_type)
        {
            buff << make_stringf( check_gettext(item_typ == BOOK_MANUAL ? N_("%s%smanual") : N_("%s%sbook")),
                check_gettext(book_secondary_string(this->special / NDSC_BOOK_PRI)),
                check_gettext(book_primary_string(this->special % NDSC_BOOK_PRI)));
        }
        else if (item_typ == BOOK_MANUAL)
        {
            if (dbname)
                buff << check_gettext(M_("manual"));
            else
                buff << make_stringf(check_gettext("manual of %s"), check_gettext(skill_name((skill_type)it_plus)));
        }
        else if (item_typ == BOOK_NECRONOMICON)
            buff << check_gettext(M_("Necronomicon"));
        else if (item_typ == BOOK_GRAND_GRIMOIRE)
            buff << check_gettext(M_("Grand Grimoire"));
        else if (item_typ == BOOK_DESTRUCTION)
            buff << check_gettext(M_("tome of Destruction"));
        else if (item_typ == BOOK_YOUNG_POISONERS)
            buff << check_gettext(M_("Young Poisoner's Handbook"));
        else
            /// 1. 책의 종류가 번역되어 들어옴. eg> 화염, 냉기, 대기, 추적, 천공
            buff << make_stringf(check_gettext("book of %s"), check_gettext(_book_type_name(item_typ)));
        break;

    case OBJ_STAVES:
        if (know_curse && !terse)
        {
            if (cursed())
                buff << "cursed ";
            else if (Options.show_uncursed && desc != DESC_PLAIN
                     && !know_pluses
                     && (!know_type || !is_artefact(*this)))
            {
                buff << "uncursed ";
            }
        }

        if (!know_type)
        {
            if (!basename)
            {
                buff << staff_secondary_string(this->special / NDSC_STAVE_PRI)
                     << staff_primary_string(this->special % NDSC_STAVE_PRI);
            }

            buff << (item_is_rod(*this) ? "rod" : "staff");
        }
        else
        {
            if (item_is_rod(*this) && know_type && know_pluses
                && !basename && !qualname && !dbname)
            {
                short rmod = 0;
                if (props.exists("rod_enchantment"))
                    rmod = props["rod_enchantment"];

                output_with_sign(buff, rmod);
                buff << " ";
            }

            buff << (item_is_rod(*this) ? "rod" : "staff")
                 << " of " << staff_type_name(item_typ);
        }

        if (know_curse && cursed() && terse)
            buff << " (curse)";
        break;

    // rearranged 15 Apr 2000 {dlb}:
    case OBJ_ORBS:
        buff.str("Orb of Zot");
        break;

    case OBJ_GOLD:
        buff << "gold piece";
        break;

    case OBJ_CORPSES:
    {
        if (food_is_rotten(*this) && !dbname && it_plus != MONS_ROTTING_HULK)
            buff << "rotting ";

        uint64_t name_type, name_flags = 0;

        const std::string _name  = get_corpse_name(*this, &name_flags);
        const bool        shaped = starts_with(_name, "shaped ");
        name_type = (name_flags & MF_NAME_MASK);

        if (!_name.empty() && name_type == MF_NAME_ADJECTIVE)
            buff << _name << " ";

        if ((name_flags & MF_NAME_SPECIES) && name_type == MF_NAME_REPLACE)
            buff << _name << " ";
        else if (!dbname && !starts_with(_name, "the "))
        {
            buff << mons_type_name(it_plus, DESC_PLAIN) << ' ';

            if (!_name.empty() && shaped)
                buff << _name << ' ';
        }

        if (item_typ == CORPSE_BODY)
            buff << "corpse";
        else if (item_typ == CORPSE_SKELETON)
            buff << "skeleton";
        else
            buff << "corpse bug";

        if (!_name.empty() && !shaped && name_type != MF_NAME_ADJECTIVE
            && !(name_flags & MF_NAME_SPECIES) && name_type != MF_NAME_SUFFIX)
        {
            buff << " of " << _name;
        }
        break;
    }

    default:
        buff << "!";
    }

    // One plural to rule them all.
    if (need_plural && this->quantity > 1 && !basename && !qualname)
        buff.str(pluralise(buff.str()));

    // Disambiguation.
    if (!terse && !basename && !dbname && know_type
        && !is_artefact(*this))
    {
        switch (this->base_type)
        {
        case OBJ_STAVES:
            switch (item_typ)
            {
            case STAFF_DESTRUCTION_I:
                buff << " [fire]";
                break;
            case STAFF_DESTRUCTION_II:
                buff << " [ice]";
                break;
            case STAFF_DESTRUCTION_III:
                buff << " [lightning,iron,fireball]";
                break;
            case STAFF_DESTRUCTION_IV:
                buff << " [inacc,magma,cold]";
                break;
            }
            break;
#if TAG_MAJOR_VERSION == 32
        case OBJ_BOOKS:
            switch (item_typ)
            {
            case BOOK_MINOR_MAGIC_II:
                buff << " [frost]";
                break;
            case BOOK_MINOR_MAGIC_III:
                buff << " [summ]";
                break;
            case BOOK_CONJURATIONS_I:
                buff << " [fire+earth]";
                break;
            }
            break;
#endif

        default:
            break;
        }
    }

    // Rod charges.
    if (item_is_rod(*this) && know_type && know_pluses
        && !basename && !qualname && !dbname)
    {
        buff << " (" << (this->plus / ROD_CHARGE_MULT)
             << "/"  << (this->plus2 / ROD_CHARGE_MULT)
             << ")";
    }

    // debugging output -- oops, I probably block it above ... dang! {dlb}
    if (buff.str().length() < 3)
    {
        buff << "bad item (cl:" << static_cast<int>(this->base_type)
             << ",ty:" << item_typ << ",pl:" << it_plus
             << ",pl2:" << item_plus2 << ",sp:" << this->special
             << ",qu:" << this->quantity << ")";
    }

    return buff.str();
}

bool item_type_has_ids(object_class_type base_type)
{
    COMPILE_CHECK(NUM_WANDS     <= MAX_SUBTYPES);
    COMPILE_CHECK(NUM_SCROLLS   <= MAX_SUBTYPES);
    COMPILE_CHECK(NUM_JEWELLERY <= MAX_SUBTYPES);
    COMPILE_CHECK(NUM_POTIONS   <= MAX_SUBTYPES);
    COMPILE_CHECK(NUM_STAVES    <= MAX_SUBTYPES);

    return base_type == OBJ_WANDS || base_type == OBJ_SCROLLS
        || base_type == OBJ_JEWELLERY || base_type == OBJ_POTIONS
        || base_type == OBJ_STAVES;
}

bool item_type_known(const item_def& item)
{
    if (item_ident(item, ISFLAG_KNOW_TYPE))
        return (true);

    // Artefacts have different descriptions from other items,
    // so we can't use general item knowledge for them.
    if (is_artefact(item))
        return (false);

    if (item.base_type == OBJ_MISSILES
        && missile_brand_obvious(get_ammo_brand(item)))
    {
        return (true);
    }

    if (item.base_type == OBJ_MISCELLANY
        && (item.sub_type < MISC_FIRST_DECK || item.sub_type > MISC_LAST_DECK)
        && item.sub_type != MISC_CRYSTAL_BALL_OF_SEEING
        && item.sub_type != MISC_CRYSTAL_BALL_OF_ENERGY)
    {
        return (true);
    }

    if (item.base_type == OBJ_BOOKS && item.sub_type == BOOK_DESTRUCTION)
        return (true);

    if (!item_type_has_ids(item.base_type))
        return false;
    return (you.type_ids[item.base_type][item.sub_type] == ID_KNOWN_TYPE);
}

bool item_type_unknown(const item_def& item)
{
    if (item_type_known(item))
        return (false);

    if (is_artefact(item))
        return (true);

    return item_type_has_ids(item.base_type);
}

bool item_type_known(const object_class_type base_type, const int sub_type)
{
    if (!item_type_has_ids(base_type))
        return false;
    return (you.type_ids[base_type][sub_type] == ID_KNOWN_TYPE);
}

bool item_type_tried(const item_def& item)
{
    if (item_type_known(item))
        return (false);

    if (is_artefact(item) && item.base_type == OBJ_JEWELLERY)
    {
        if (item.base_type == OBJ_JEWELLERY
            && item.props.exists("jewellery_tried")
            && item.props["jewellery_tried"].get_bool())
        {
            return (true);
        }
        return (false);
    }

    if (!item_type_has_ids(item.base_type))
        return false;
    return (you.type_ids[item.base_type][item.sub_type] != ID_UNKNOWN_TYPE);
}

void set_ident_type(item_def &item, item_type_id_state_type setting,
                    bool force)
{
    if (is_artefact(item) || crawl_state.game_is_arena())
        return;

    item_type_id_state_type old_setting = get_ident_type(item);
    set_ident_type(item.base_type, item.sub_type, setting, force);

    if (in_inventory(item))
        shopping_list.cull_identical_items(item);

    if (setting == ID_KNOWN_TYPE && old_setting != ID_KNOWN_TYPE
        && notes_are_active() && is_interesting_item(item)
        && !(item.flags & (ISFLAG_NOTED_ID | ISFLAG_NOTED_GET)))
    {
        // Make a note of it.
        take_note(Note(NOTE_ID_ITEM, 0, 0, item.name(true, DESC_NOCAP_A).c_str(),
                       origin_desc(item).c_str()));

        // Sometimes (e.g. shops) you can ID an item before you get it;
        // don't note twice in those cases.
        item.flags |= (ISFLAG_NOTED_ID | ISFLAG_NOTED_GET);
    }
}

void set_ident_type(object_class_type basetype, int subtype,
                     item_type_id_state_type setting, bool force)
{
    preserve_quiver_slots p;
    // Don't allow overwriting of known type with tried unless forced.
    if (!force
        && (setting == ID_MON_TRIED_TYPE || setting == ID_TRIED_TYPE)
        && setting <= get_ident_type(basetype, subtype))
    {
        return;
    }

    if (!item_type_has_ids(basetype))
        return;

    if (you.type_ids[basetype][subtype] != setting)
    {
        you.type_ids[basetype][subtype] = setting;
        request_autoinscribe();
    }
}

item_type_id_state_type get_ident_type(const item_def &item)
{
    if (is_artefact(item))
        return ID_UNKNOWN_TYPE;

    return get_ident_type(item.base_type, item.sub_type);
}

item_type_id_state_type get_ident_type(object_class_type basetype, int subtype)
{
    if (!item_type_has_ids(basetype))
        return ID_UNKNOWN_TYPE;
    ASSERT(subtype < MAX_SUBTYPES);
    return you.type_ids[basetype][subtype];
}

class DiscEntry : public InvEntry
{
public:
    DiscEntry(InvEntry* inv) : InvEntry(*inv->item)
    {
    }

    virtual std::string get_text(const bool = false) const
    {
        int flags = item->base_type == OBJ_WANDS ? 0 : ISFLAG_KNOW_PLUSES;
        return std::string(" ") + item->name(false, DESC_PLAIN, false, true,
                                             false, false, flags);
    }
};

static MenuEntry *discoveries_item_mangle(MenuEntry *me)
{
    InvEntry *ie = dynamic_cast<InvEntry*>(me);
    DiscEntry *newme = new DiscEntry(ie);
    delete me;

    return (newme);
}

bool identified_item_names(const item_def *it1,
                            const item_def *it2)
{
    int flags = it1->base_type == OBJ_WANDS ? 0 : ISFLAG_KNOW_PLUSES;
    return it1->name(false, DESC_PLAIN, false, true, false, false, flags)
         < it2->name(false, DESC_PLAIN, false, true, false, false, flags);
}

void check_item_knowledge(bool unknown_items)
{
    std::vector<const item_def*> items;

    bool needs_inversion = false;
    for (int ii = 0; ii < NUM_OBJECT_CLASSES; ii++)
    {
        object_class_type i = (object_class_type)ii;
        if (!item_type_has_ids(i))
            continue;
        for (int j = 0; j < get_max_subtype(i); j++)
        {
#if TAG_MAJOR_VERSION == 32
            if (i == OBJ_SCROLLS && j == SCR_PAPER)
                continue;
#endif
            if (i == OBJ_JEWELLERY && j >= NUM_RINGS && j < AMU_FIRST_AMULET)
                continue;

            // Potions of fizzing liquid are not something that
            // need to be identified, because they never randomly
            // generate! [due]
            if (i == OBJ_POTIONS && j == POT_FIZZING)
                continue;

            if (unknown_items ? you.type_ids[i][j] != ID_KNOWN_TYPE
                              : you.type_ids[i][j] == ID_KNOWN_TYPE)
            {
                item_def* ptmp = new item_def;
                if (ptmp != 0)
                {
                    ptmp->base_type = i;
                    ptmp->sub_type  = j;
                    ptmp->colour    = 1;
                    ptmp->quantity  = 1;
                    if (i == OBJ_WANDS)
                        ptmp->plus = wand_max_charges(j);
                    items.push_back(ptmp);
                }
            }
            else
                needs_inversion = true;
        }
    }

    // runes are shown only if known
    if (!unknown_items)
        for (int i = 0; i < NUM_RUNE_TYPES; i++)
            if (you.runes[i])
            {
                item_def* ptmp = new item_def;
                if (ptmp != 0)
                {
                    ptmp->base_type = OBJ_MISCELLANY;
                    ptmp->sub_type  = MISC_RUNE_OF_ZOT;
                    ptmp->quantity  = 1;
                    ptmp->plus      = i;
                    item_colour(*ptmp);
                    items.push_back(ptmp);
                }
            }

    if (!unknown_items && items.empty())
    {
        // Directly skip ahead to unknown items.
        check_item_knowledge(true);
        return;
    }

    std::sort(items.begin(), items.end(), identified_item_names);
    InvMenu menu;

    if (unknown_items)
    {
        menu.set_title(make_stringf("Items not yet recognised: %s",
                                    needs_inversion ? "(toggle with -)"
                                                    : ""));
    }
    else if (needs_inversion)
        menu.set_title("You recognise: (toggle with -)");
    else
        menu.set_title("You recognise all items:");

    menu.set_flags(MF_NOSELECT);
    menu.set_type(MT_KNOW);
    menu.load_items(items, discoveries_item_mangle);
    menu.show(true);
    char last_char = menu.getkey();
    redraw_screen();

    for (std::vector<const item_def*>::iterator iter = items.begin();
         iter != items.end(); ++iter)
    {
         delete *iter;
    }
    if (last_char == '-' && needs_inversion)
        check_item_knowledge(!unknown_items);
}

void display_runes()
{
    std::vector<const item_def*> items;
    for (int i = 0; i < NUM_RUNE_TYPES; i++)
    {
        if (!you.runes[i])
            continue;

        item_def* ptmp = new item_def;
        if (ptmp != 0)
        {
            ptmp->base_type = OBJ_MISCELLANY;
            ptmp->sub_type  = MISC_RUNE_OF_ZOT;
            ptmp->quantity  = 1;
            ptmp->plus      = i;
            item_colour(*ptmp);
            items.push_back(ptmp);
        }
    }

    if (items.empty())
    {
        mpr("You haven't found any rune yet.");
        return;
    }

    InvMenu menu;

    menu.set_title("Runes of Zot");
    menu.set_flags(MF_NOSELECT);
    menu.set_type(MT_RUNES);
    menu.load_items(items, discoveries_item_mangle);
    menu.show();
    menu.getkey();
    redraw_screen();

    for (std::vector<const item_def*>::iterator iter = items.begin();
         iter != items.end(); ++iter)
    {
         delete *iter;
    }
}

// Used for: Pandemonium demonlords, shopkeepers, scrolls, random artefacts
std::string make_name(uint32_t seed, bool all_cap, int maxlen, char start)
{
    char name[ITEMNAME_SIZE];
    int  numb[17]; // contains the random seeds used for the name

    int i = 0;
    bool want_vowel = false; // Keep track of whether we want a vowel next.
    bool has_space  = false; // Keep track of whether the name contains a space.

    for (i = 0; i < ITEMNAME_SIZE; ++i)
        name[i] = '\0';

    const int var1 = (seed & 0xFF);
    const int var2 = ((seed >>  8) & 0xFF);
    const int var3 = ((seed >> 16) & 0xFF);
    const int var4 = ((seed >> 24) & 0xFF);

    numb[0]  = 373 * var1 + 409 * var2 + 281 * var3;
    numb[1]  = 163 * var4 + 277 * var2 + 317 * var3;
    numb[2]  = 257 * var1 + 179 * var4 +  83 * var3;
    numb[3]  =  61 * var1 + 229 * var2 + 241 * var4;
    numb[4]  =  79 * var1 + 263 * var2 + 149 * var3;
    numb[5]  = 233 * var4 + 383 * var2 + 311 * var3;
    numb[6]  = 199 * var1 + 211 * var4 + 103 * var3;
    numb[7]  = 139 * var1 + 109 * var2 + 349 * var4;
    numb[8]  =  43 * var1 + 389 * var2 + 359 * var3;
    numb[9]  = 367 * var4 + 101 * var2 + 251 * var3;
    numb[10] = 293 * var1 +  59 * var4 + 151 * var3;
    numb[11] = 331 * var1 + 107 * var2 + 307 * var4;
    numb[12] =  73 * var1 + 157 * var2 + 347 * var3;
    numb[13] = 379 * var4 + 353 * var2 + 227 * var3;
    numb[14] = 181 * var1 + 173 * var4 + 193 * var3;
    numb[15] = 131 * var1 + 167 * var2 +  53 * var4;
    numb[16] = 313 * var1 + 127 * var2 + 401 * var3 + 337 * var4;

    int len = 3 + numb[0] % 5 + ((numb[1] % 5 == 0) ? numb[2] % 6 : 1);

    if (all_cap)   // scrolls have longer names
        len += 6;

    if (maxlen != -1 && len > maxlen)
        len = maxlen;

    ASSERT(len > 0);
    ASSERT(len <= ITEMNAME_SIZE);

    int j = numb[3] % 17;
    const int k = numb[4] % 17;

    int count = 0;
    for (i = 0; i < len; ++i)
    {
        j = (j + 1) % 17;
        if (j == 0)
        {
            count++;
            if (count > 9)
                break;
        }

        if (i == 0 && start != 0)
        {
            // Start the name with a predefined letter.
            name[i] = start;
            want_vowel = _is_random_name_vowel(start);
        }
        else if (!has_space && i > 5 && i < len - 4
                 && (numb[(k + 10 * j) % 17] % 5) != 3) // 4/5 chance of a space
        {
            // Hand out a space.
            want_vowel = true;
            name[i] = ' ';
        }
        else if (i > 0
                 && (want_vowel
                     || (i > 1
                         && _is_random_name_vowel(name[i - 1])
                         && !_is_random_name_vowel(name[i - 2])
                         && (numb[(k + 4 * j) % 17] % 5) <= 1))) // 2/5 chance
        {
            // Place a vowel.
            want_vowel = true;
            name[i] = _random_vowel(numb[(k + 7 * j) % 17]);

            if (_is_random_name_space(name[i]))
            {
                if (i == 0) // Shouldn't happen.
                {
                    want_vowel = false;
                    name[i]    = _random_cons(numb[(k + 14 * j) % 17]);
                }
                else if (len < 7
                         || i <= 2 || i >= len - 3
                         || _is_random_name_space(name[i - 1])
                         || (i > 1 && _is_random_name_space(name[i - 2]))
                         || i > 2
                            && !_is_random_name_vowel(name[i - 1])
                            && !_is_random_name_vowel(name[i - 2]))
                {
                    // Replace the space with something else if ...
                    // * the name is really short
                    // * we're close to the begin/end of the name
                    // * we just got a space, or
                    // * the last two letters were consonants
                    i--;
                    continue;
                }
            }
            else if (i > 1
                     && name[i] == name[i - 1]
                     && (name[i] == 'y' || name[i] == 'i'
                         || (numb[(k + 12 * j) % 17] % 5) <= 1))
            {
                // Replace the vowel with something else if the previous
                // letter was the same, and it's a 'y', 'i' or with 2/5 chance.
                i--;
                continue;
            }
        }
        else // We want a consonant.
        {
            // Use one of number of predefined letter combinations.
            if ((len > 3 || i != 0)
                && (numb[(k + 13 * j) % 17] % 7) <= 1 // 2/7 chance
                && (i < len - 2
                    || i > 0 && !_is_random_name_space(name[i - 1])))
            {
                // Are we at start or end of the (sub) name?
                const bool beg = (i < 1 || _is_random_name_space(name[i - 1]));
                const bool end = (i >= len - 2);

                const int first = (beg ?  0 : (end ? 14 :  0));
                const int last  = (beg ? 27 : (end ? 56 : 67));

                const int num = last - first;

                i++;

                // Pick a random combination of consonants from the set below.
                //   begin  -> [0,27]
                //   middle -> [0,67]
                //   end    -> [14,56]

                switch (numb[(k + 11 * j) % 17] % num + first)
                {
                // start, middle
                case  0: strcat(name, "kl"); break;
                case  1: strcat(name, "gr"); break;
                case  2: strcat(name, "cl"); break;
                case  3: strcat(name, "cr"); break;
                case  4: strcat(name, "fr"); break;
                case  5: strcat(name, "pr"); break;
                case  6: strcat(name, "tr"); break;
                case  7: strcat(name, "tw"); break;
                case  8: strcat(name, "br"); break;
                case  9: strcat(name, "pl"); break;
                case 10: strcat(name, "bl"); break;
                case 11: strcat(name, "str"); i++; len++; break;
                case 12: strcat(name, "shr"); i++; len++; break;
                case 13: strcat(name, "thr"); i++; len++; break;
                // start, middle, end
                case 14: strcat(name, "sm"); break;
                case 15: strcat(name, "sh"); break;
                case 16: strcat(name, "ch"); break;
                case 17: strcat(name, "th"); break;
                case 18: strcat(name, "ph"); break;
                case 19: strcat(name, "pn"); break;
                case 20: strcat(name, "kh"); break;
                case 21: strcat(name, "gh"); break;
                case 22: strcat(name, "mn"); break;
                case 23: strcat(name, "ps"); break;
                case 24: strcat(name, "st"); break;
                case 25: strcat(name, "sk"); break;
                case 26: strcat(name, "sch"); i++; len++; break;
                // middle, end
                case 27: strcat(name, "ts"); break;
                case 28: strcat(name, "cs"); break;
                case 29: strcat(name, "xt"); break;
                case 30: strcat(name, "nt"); break;
                case 31: strcat(name, "ll"); break;
                case 32: strcat(name, "rr"); break;
                case 33: strcat(name, "ss"); break;
                case 34: strcat(name, "wk"); break;
                case 35: strcat(name, "wn"); break;
                case 36: strcat(name, "ng"); break;
                case 37: strcat(name, "cw"); break;
                case 38: strcat(name, "mp"); break;
                case 39: strcat(name, "ck"); break;
                case 40: strcat(name, "nk"); break;
                case 41: strcat(name, "dd"); break;
                case 42: strcat(name, "tt"); break;
                case 43: strcat(name, "bb"); break;
                case 44: strcat(name, "pp"); break;
                case 45: strcat(name, "nn"); break;
                case 46: strcat(name, "mm"); break;
                case 47: strcat(name, "kk"); break;
                case 48: strcat(name, "gg"); break;
                case 49: strcat(name, "ff"); break;
                case 50: strcat(name, "pt"); break;
                case 51: strcat(name, "tz"); break;
                case 52: strcat(name, "dgh"); i++; len++; break;
                case 53: strcat(name, "rgh"); i++; len++; break;
                case 54: strcat(name, "rph"); i++; len++; break;
                case 55: strcat(name, "rch"); i++; len++; break;
                // middle only
                case 56: strcat(name, "cz"); break;
                case 57: strcat(name, "xk"); break;
                case 58: strcat(name, "zx"); break;
                case 59: strcat(name, "xz"); break;
                case 60: strcat(name, "cv"); break;
                case 61: strcat(name, "vv"); break;
                case 62: strcat(name, "nl"); break;
                case 63: strcat(name, "rh"); break;
                case 64: strcat(name, "dw"); break;
                case 65: strcat(name, "nw"); break;
                case 66: strcat(name, "khl"); i++; len++; break;
                default:
                    i--;
                    break;
                }
            }
            else // Place a single letter instead.
            {
                if (i == 0)
                {
                    // Start with any letter.
                    name[i] = 'a' + (numb[(k + 8 * j) % 17] % 26);
                    want_vowel = _is_random_name_vowel(name[i]);
                }
                else
                {
                    // Pick a random consonant.
                    name[i] = _random_cons(numb[(k + 3 * j) % 17]);
                }
            }
        }

        // No letter chosen?
        if (name[i] == '\0')
        {
            i--;
            continue;
        }

        // Picked wrong type?
        if (want_vowel && !_is_random_name_vowel(name[i])
            || !want_vowel && _is_random_name_vowel(name[i]))
        {
            i--;
            continue;
        }

        if (_is_random_name_space(name[i]))
            has_space = true;

        // If we just got a vowel, we want a consonant next, and vice versa.
        want_vowel = !_is_random_name_vowel(name[i]);
    }

    // Catch break and try to give a final letter.
    if (i > 0
        && !_is_random_name_space(name[i - 1])
        && name[i - 1] != 'y'
        && _is_random_name_vowel(name[i - 1])
        && (count > 9 || (i < 8 && numb[16] % 3)))
    {
        // 2/3 chance of ending in a consonant
        name[i] = _random_cons(numb[j]);
    }

    len = strlen(name);

    if (len)
    {
        for (i = len - 1; i > 0; i--)
        {
            if (!isspace(name[i]))
                break;
            else
            {
                name[i] = '\0';
                len--;
            }
        }
    }

    // Fallback if the name was too short.
    if (len < 4)
    {
        strcpy(name, "plog");
        len = 4;
    }

    for (i = 0; i < len; i++)
        if (all_cap || i == 0 || name[i - 1] == ' ')
            name[i] = toupper(name[i]);

    return name;
}

static bool _is_random_name_space(char let)
{
    return (let == ' ');
}

// Returns true for vowels, 'y' or space.
static bool _is_random_name_vowel(char let)
{
    return (let == 'a' || let == 'e' || let == 'i' || let == 'o' || let == 'u'
            || let == 'y' || let == ' ');
}

// Returns a random vowel (a, e, i, o, u with equal probability) or space
// or 'y' with lower chances.
static char _random_vowel(int seed)
{
    static const char vowels[] = "aeiouaeiouaeiouy  ";
    return (vowels[ seed % (sizeof(vowels) - 1) ]);
}

// Returns a random consonant with not quite equal probability.
// Does not include 'y'.
static char _random_cons(int seed)
{
    static const char consonants[] = "bcdfghjklmnpqrstvwxzcdfghlmnrstlmnrst";
    return (consonants[ seed % (sizeof(consonants) - 1) ]);
}

bool is_interesting_item(const item_def& item)
{
    if (fully_identified(item) && is_artefact(item))
        return (true);

    const std::string iname = menu_colour_item_prefix(item, false) + " "
                              + item.name(false, DESC_PLAIN);
    for (unsigned i = 0; i < Options.note_items.size(); ++i)
        if (Options.note_items[i].matches(iname))
            return (true);

    return (false);
}

// Returns true if an item is a potential life saver in an emergency
// situation.
bool is_emergency_item(const item_def &item)
{
    if (!item_type_known(item))
        return (false);

    switch (item.base_type)
    {
    case OBJ_WANDS:
        switch (item.sub_type)
        {
        case WAND_HASTING:
            if (you.religion == GOD_CHEIBRIADOS)
                return (false);
        case WAND_HEALING:
        case WAND_TELEPORTATION:
            return (true);
        default:
            return (false);
        }
    case OBJ_SCROLLS:
        switch (item.sub_type)
        {
        case SCR_TELEPORTATION:
        case SCR_BLINKING:
        case SCR_FEAR:
        case SCR_FOG:
            return (true);
        default:
            return (false);
        }
    case OBJ_POTIONS:
        if (you.species == SP_MUMMY)
            return (false);

        switch (item.sub_type)
        {
        case POT_SPEED:
            if (you.religion == GOD_CHEIBRIADOS)
                return (false);
        case POT_HEALING:
        case POT_HEAL_WOUNDS:
        case POT_RESISTANCE:
        case POT_MAGIC:
            return (true);
        default:
            return (false);
        }
    default:
        return (false);
    }
}

// Returns true if an item can be considered particularly good.
bool is_good_item(const item_def &item)
{
    if (!item_type_known(item))
        return (false);

    if (is_emergency_item(item))
        return (true);

    switch (item.base_type)
    {
    case OBJ_SCROLLS:
        return (item.sub_type == SCR_ACQUIREMENT);
    case OBJ_POTIONS:
        switch (item.sub_type)
        {
        case POT_CURE_MUTATION:
        case POT_GAIN_STRENGTH:
        case POT_GAIN_INTELLIGENCE:
        case POT_GAIN_DEXTERITY:
        case POT_EXPERIENCE:
            return (true);
        default:
            return (false);
        }
    default:
        return (false);
    }
}

// Returns true if using an item only has harmful effects.
bool is_bad_item(const item_def &item, bool temp)
{
    if (!item_type_known(item))
        return (false);

    switch (item.base_type)
    {
    case OBJ_SCROLLS:
        switch (item.sub_type)
        {
        case SCR_CURSE_ARMOUR:
        case SCR_CURSE_JEWELLERY:
        case SCR_CURSE_WEAPON:
            return (you.religion != GOD_ASHENZARI);
        case SCR_SUMMONING:
            // Summoning will always produce hostile monsters if you
            // worship a good god. (Use temp to allow autopickup to
            // prevent monsters from reading it.)
            return (temp && is_good_god(you.religion));
        default:
            return (false);
        }
    case OBJ_POTIONS:
        // Can't be bad if you can't use them.
        if (you.species == SP_MUMMY)
            return (false);

        switch (item.sub_type)
        {
        case POT_CONFUSION:
        case POT_SLOWING:
        case POT_DEGENERATION:
        case POT_DECAY:
        case POT_PARALYSIS:
            return (true);
        case POT_POISON:
        case POT_STRONG_POISON:
            // Poison is not that bad if you're poison resistant.
            return (!player_res_poison(false)
                    || !temp && you.species == SP_VAMPIRE);
        case POT_MUTATION:
            return (you.is_undead
                    && (temp || you.species != SP_VAMPIRE
                        || you.hunger_state < HS_SATIATED));
        default:
            return (false);
        }
    case OBJ_JEWELLERY:
        switch (item.sub_type)
        {
        case AMU_INACCURACY:
            return (true);
        case RING_HUNGER:
            // Even Vampires can use this ring.
            return (!you.is_undead || you.is_undead == US_HUNGRY_DEAD);
        case RING_EVASION:
        case RING_PROTECTION:
        case RING_STRENGTH:
        case RING_DEXTERITY:
        case RING_INTELLIGENCE:
            return (item_ident(item, ISFLAG_KNOW_PLUSES) && item.plus <= 0);
        case RING_SLAYING:
            return (item_ident(item, ISFLAG_KNOW_PLUSES) && item.plus <= 0
                    && item.plus2 <= 0);
        default:
            return (false);
        }

    default:
        return (false);
    }
}

// Returns true if using an item is risky but may occasionally be
// worthwhile.
bool is_dangerous_item(const item_def &item, bool temp)
{
    if (!item_type_known(item))
    {
        // Use-IDing these is extremely dangerous!
        if (item.base_type == OBJ_MISCELLANY
            && (item.sub_type == MISC_CRYSTAL_BALL_OF_SEEING
                || item.sub_type == MISC_CRYSTAL_BALL_OF_ENERGY))
        {
            return (true);
        }
        return (false);
    }

    switch (item.base_type)
    {
    case OBJ_SCROLLS:
        if (!item_type_known(item))
            return (false);

        switch (item.sub_type)
        {
        case SCR_IMMOLATION:
        case SCR_NOISE:
            return (true);
        case SCR_TORMENT:
            return (!player_mutation_level(MUT_TORMENT_RESISTANCE)
                    || !temp && you.species == SP_VAMPIRE);
        case SCR_HOLY_WORD:
            return (you.undead_or_demonic());
        default:
            return (false);
        }

    case OBJ_POTIONS:
        if (!item_type_known(item))
            return (false);

        switch (item.sub_type)
        {
        case POT_MUTATION:
            // Only living characters can mutate.
            return (!you.is_undead
                    || temp && you.species == SP_VAMPIRE
                       && you.hunger_state >= HS_SATIATED);
        default:
            return (false);
        }

    case OBJ_BOOKS:
        // The Tome of Destruction is certainly risky.
        return (item.sub_type == BOOK_DESTRUCTION
                || is_dangerous_spellbook(item));

    default:
        return (false);
    }
}

bool is_useless_item(const item_def &item, bool temp)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        if (you.species == SP_FELID)
            return (true);

        if (!you.could_wield(item, true)
            && !is_throwable(&you, item))
        {
            // Weapon is too large (or small) to be wielded and cannot
            // be thrown either.
            return (true);
        }

        if (!item_type_known(item))
            return (false);

        if (you.undead_or_demonic() && is_holy_item(item))
        {
            if (!temp && you.form == TRAN_LICH
                && you.species != SP_DEMONSPAWN)
            {
                return (false);
            }
            return (true);
        }

        return (false);

    case OBJ_MISSILES:
        if ((you.has_spell(SPELL_STICKS_TO_SNAKES)
             || !you.num_turns
                && you.char_class == JOB_TRANSMUTER)
            && item_is_snakable(item)
            || you.has_spell(SPELL_SANDBLAST)
               && (item.sub_type == MI_STONE
                || item.sub_type == MI_LARGE_ROCK))
        {
            return false;
        }

        // Save for the above spells, all missiles are useless for felids.
        if (you.species == SP_FELID)
            return true;

        // These are the same checks as in is_throwable(), except that
        // we don't take launchers into account.
        switch (item.sub_type)
        {
        case MI_LARGE_ROCK:
            return (you.body_size(PSIZE_BODY, !temp) < SIZE_LARGE
                    || !you.can_throw_large_rocks());
        case MI_JAVELIN:
        case MI_THROWING_NET:
            return (you.body_size(PSIZE_BODY, !temp) < SIZE_MEDIUM);
        }

        return (false);

    case OBJ_ARMOUR:
        return (!can_wear_armour(item, false, true));

    case OBJ_SCROLLS:
        // No scroll is useless if you haven't learned spellcasting yet,
        // except if you worship Trog.
        if (!item_type_known(item)
            || !you.skills[SK_SPELLCASTING] && you.religion != GOD_TROG)
        {
            return (false);
        }

        // A bad item is always useless.
        if (is_bad_item(item, temp))
            return (true);

        switch (item.sub_type)
        {
        case SCR_RANDOM_USELESSNESS:
            return (true);
        case SCR_TELEPORTATION:
            return (crawl_state.game_is_sprint());
        case SCR_AMNESIA:
            return (you.religion == GOD_TROG);
        case SCR_RECHARGING:
        case SCR_CURSE_WEAPON: // for non-Ashenzari, already handled
        case SCR_CURSE_ARMOUR:
        case SCR_ENCHANT_WEAPON_I:
        case SCR_ENCHANT_WEAPON_II:
        case SCR_ENCHANT_WEAPON_III:
        case SCR_ENCHANT_ARMOUR:
        case SCR_VORPALISE_WEAPON:
            return (you.species == SP_FELID);
        case SCR_DETECT_CURSE:
            return (you.religion == GOD_ASHENZARI);
        default:
            return (false);
        }
    case OBJ_WANDS:
        if (you.species == SP_FELID)
            return (true);

        return (item.plus2 == ZAPCOUNT_EMPTY)
               || item_ident(item, ISFLAG_KNOW_PLUSES) && !item.plus;

    case OBJ_POTIONS:
    {
        // No potion is useless if it can be used for Evaporate.
        if ((you.char_class == JOB_TRANSMUTER || you.char_class == JOB_STALKER)
             && !you.num_turns
            || you.has_spell(SPELL_EVAPORATE))
        {
            return (false);
        }

        // Apart from Evaporate, mummies can't use potions.
        if (you.species == SP_MUMMY)
            return (true);

        if (!item_type_known(item))
            return (false);

        // A bad item is always useless.
        if (is_bad_item(item, temp))
            return (true);

        switch (item.sub_type)
        {
        case POT_BERSERK_RAGE:
            return (you.is_undead
                        && (you.species != SP_VAMPIRE
                            || temp && you.hunger_state <= HS_SATIATED));

        case POT_CURE_MUTATION:
        case POT_GAIN_STRENGTH:
        case POT_GAIN_INTELLIGENCE:
        case POT_GAIN_DEXTERITY:
            return (you.is_undead
                        && (you.species != SP_VAMPIRE
                            || temp && you.hunger_state < HS_SATIATED));

        case POT_LEVITATION:
            return (you.permanent_levitation() || you.permanent_flight());

        case POT_PORRIDGE:
        case POT_WATER:
        case POT_BLOOD:
        case POT_BLOOD_COAGULATED:
            return (!can_ingest(item, true, true, false));
        case POT_POISON:
        case POT_STRONG_POISON:
            // If you're poison resistant, poison is only useless.
            // Spriggans could argue, but it's too small of a gain for
            // possible player confusion.
            return (player_res_poison(false));

        case POT_INVISIBILITY:
            // If you're Corona'd or a TSO-ite, this is always useless.
            return (temp ? you.backlit(true) : you.haloed());

        }

        return (false);
    }
    case OBJ_JEWELLERY:
        if (!item_type_known(item))
            return (false);

        // Potentially useful.
        if (is_artefact(item))
            return (false);

        if (is_bad_item(item, temp))
            return (true);

        switch (item.sub_type)
        {
        case AMU_RAGE:
            return (you.is_undead
                        && (you.species != SP_VAMPIRE
                            || temp && you.hunger_state <= HS_SATIATED));

        case AMU_THE_GOURMAND:
            return (player_likes_chunks(true)
                      && player_mutation_level(MUT_SAPROVOROUS) == 3
                      && you.species != SP_GHOUL // makes clean chunks
                                                 // contaminated
                    || player_mutation_level(MUT_HERBIVOROUS) == 3
                    || you.species == SP_MUMMY);

        case AMU_FAITH:
            return (you.species == SP_DEMIGOD);

        case RING_LIFE_PROTECTION:
            return (player_prot_life(false, temp, false) == 3);

        case RING_HUNGER:
        case RING_SUSTENANCE:
            return (you.species == SP_MUMMY
                    || temp && you.species == SP_VAMPIRE
                       && you.hunger_state == HS_STARVING);

        case RING_REGENERATION:
            return ((player_mutation_level(MUT_SLOW_HEALING) == 3)
                    || temp && you.species == SP_VAMPIRE
                       && you.hunger_state == HS_STARVING);

        case RING_SEE_INVISIBLE:
            return (player_mutation_level(MUT_ACUTE_VISION));

        case RING_POISON_RESISTANCE:
            return (player_res_poison(false, temp, false)
                    && (temp || you.species != SP_VAMPIRE));

        case AMU_CONTROLLED_FLIGHT:
            return (player_genus(GENPC_DRACONIAN)
                    || (you.species == SP_KENKU && you.experience_level >= 5));

        case RING_WIZARDRY:
            return (you.religion == GOD_TROG);

        case RING_TELEPORT_CONTROL:
            return (player_control_teleport(true, temp, false));

        case RING_TELEPORTATION:
            return (crawl_state.game_is_sprint());

        case RING_INVISIBILITY:
            return (temp && you.backlit(true));

        case RING_LEVITATION:
            return (you.permanent_levitation() || you.permanent_flight());

        default:
            return (false);
        }

    case OBJ_STAVES:
        if (you.species == SP_FELID)
            return (true);
        if (you.religion == GOD_TROG && !item_is_rod(item))
            return (true);
        if (!item_type_known(item))
            return (false);
        if (item.sub_type == STAFF_ENERGY && you.species == SP_MUMMY)
            return (true);
        if (item.sub_type == STAFF_ENERGY && temp && (you.form == TRAN_LICH
            || you.species == SP_VAMPIRE && you.hunger_state == HS_STARVING))
        {
            return (true);
        }
        break;

    case OBJ_FOOD:
        if (!is_inedible(item))
            return (false);

        if (item.sub_type == FOOD_CHUNK
            && (you.has_spell(SPELL_SUBLIMATION_OF_BLOOD)
                || you.has_spell(SPELL_SIMULACRUM)
                || !temp && you.form == TRAN_LICH))
        {
            return (false);
        }

        if (is_fruit(item) && you.religion == GOD_FEDHAS)
            return (false);

        return (true);

    case OBJ_CORPSES:
        if (item.sub_type != CORPSE_SKELETON)
            return (false);

        if (you.has_spell(SPELL_ANIMATE_DEAD)
            || you.has_spell(SPELL_ANIMATE_SKELETON)
            || you.religion == GOD_YREDELEMNUL && !you.penance[GOD_YREDELEMNUL]
               && you.piety >= piety_breakpoint(0))
        {
            return (false);
        }

        return (true);

    case OBJ_MISCELLANY:
        switch (item.sub_type)
        {
        case MISC_EMPTY_EBONY_CASKET:
            return (item_type_known(item));
        case MISC_DISC_OF_STORMS:
            return (player_res_electricity(false, temp) > 0);
        case MISC_LAMP_OF_FIRE:
            return (!you.skill(SK_FIRE_MAGIC));
        case MISC_AIR_ELEMENTAL_FAN:
            return (!you.skill(SK_AIR_MAGIC));
        default:
            return (false);
        }

    default:
        return (false);
    }
    return (false);
}

static const std::string _item_prefix(const item_def &item, bool temp,
                                      bool filter)
{
    std::vector<std::string> prefixes;

    // No identified/unidentified for filtering, since the user might
    // want to filter on "ident" to find scrolls of identify.
    if (filter)
        ;
    else if (item_ident(item, ISFLAG_KNOW_TYPE))
        prefixes.push_back("identified");
    else
    {
        if (get_ident_type(item) == ID_KNOWN_TYPE)
        {
            // Wands are only fully identified if we know the number of
            // charges.
            if (item.base_type == OBJ_WANDS)
                prefixes.push_back("known");

            // Rings are fully identified simply by knowing their type,
            // unless the ring has plusses, like a ring of dexterity.
            else if (item.base_type == OBJ_JEWELLERY
                     && !jewellery_is_amulet(item))
            {
                if (item.plus == 0 && item.plus2 == 0)
                    prefixes.push_back("identified");
                else
                    prefixes.push_back("known");
            }
            // All other types of magical items are fully identified
            // simply by knowing the type.
            else
                prefixes.push_back("identified");
        }
        else
            prefixes.push_back("unidentified");
    }

    if (good_god_hates_item_handling(item) || god_hates_item_handling(item))
        prefixes.push_back("evil_item");

    if (is_emergency_item(item))
        prefixes.push_back("emergency_item");
    if (is_good_item(item))
        prefixes.push_back("good_item");
    if (is_dangerous_item(item, temp))
        prefixes.push_back("dangerous_item");
    if (is_bad_item(item, temp))
        prefixes.push_back("bad_item");
    if (is_useless_item(item, temp))
        prefixes.push_back("useless_item");

    switch (item.base_type)
    {
    case OBJ_CORPSES:
        // Skeletons cannot be eaten.
        if (item.sub_type == CORPSE_SKELETON)
        {
            prefixes.push_back("inedible");
            break;
        }
        // intentional fall-through
    case OBJ_FOOD:
        if (is_forbidden_food(item))
            prefixes.push_back("evil_eating");

        if (is_inedible(item))
            prefixes.push_back("inedible");
        else if (is_preferred_food(item))
            prefixes.push_back("preferred");

        // Don't include these for filtering, since the user might want
        // to use "muta" to search for "potion of cure mutation", and
        // similar.
        if (filter)
            ;
        else if (is_poisonous(item))
            prefixes.push_back("poisonous");
        else if (is_mutagenic(item))
            prefixes.push_back("mutagenic");
        else if (is_contaminated(item))
            prefixes.push_back("contaminated");
        else if (causes_rot(item))
            prefixes.push_back("rot-inducing");
        break;

    case OBJ_POTIONS:
        if (is_good_god(you.religion) && item_type_known(item)
            && is_blood_potion(item))
        {
            prefixes.push_back("evil_eating");
        }
        if (is_preferred_food(item))
            prefixes.push_back("preferred");
        break;

    case OBJ_WEAPONS:
    case OBJ_ARMOUR:
    case OBJ_JEWELLERY:
        if (is_artefact(item))
            prefixes.push_back("artefact");
        // fall through

    case OBJ_STAVES:
    case OBJ_MISSILES:
        if (item_is_equipped(item, true))
            prefixes.push_back("equipped");
        break;

    default:
        break;
    }

    if (Options.menu_colour_prefix_class && !filter)
        prefixes.push_back(item_class_name(item.base_type, true));

    std::string result = comma_separated_line(prefixes.begin(), prefixes.end(),
                                              " ", " ");

    return result;
}

std::string menu_colour_item_prefix(const item_def &item, bool temp)
{
    return _item_prefix(item, temp, false);
}

std::string filtering_item_prefix(const item_def &item, bool temp)
{
    return _item_prefix(item, temp, true);
}

std::string get_menu_colour_prefix_tags(const item_def &item,
                                        description_level_type desc)
{
    std::string cprf       = menu_colour_item_prefix(item);
    std::string colour     = "";
    std::string colour_off = "";
    std::string item_name  = item.name(false, desc);
    int col = menu_colour(item_name, cprf, "pickup");

    if (col != -1)
        colour = colour_to_str(col);

    if (!colour.empty())
    {
        // Order is important here.
        colour_off  = "</" + colour + ">";
        colour      = "<" + colour + ">";
        item_name = colour + item_name + colour_off;
    }

    return (item_name);
}

std::string get_message_colour_tags(const item_def &item,
                                    description_level_type desc,
                                    msg_channel_type channel)
{
    std::string cprf       = menu_colour_item_prefix(item);
    std::string colour     = "";
    std::string colour_off = "";
    std::string item_name  = item.name(false, desc);
    cprf += " " + item_name;

    int col = -1;
    const std::vector<message_colour_mapping>& mcm
               = Options.message_colour_mappings;
    typedef std::vector<message_colour_mapping>::const_iterator mcmci;

    for (mcmci ci = mcm.begin(); ci != mcm.end(); ++ci)
    {
        if (ci->message.is_filtered(channel, cprf))
        {
            col = ci->colour;
            break;
        }
    }

    if (col != -1)
        colour = colour_to_str(col);

    if (!colour.empty())
    {
        // Order is important here.
        colour_off  = "</" + colour + ">";
        colour      = "<" + colour + ">";
        item_name   = colour + item_name + colour_off;
    }

    return (item_name);
}

typedef std::map<std::string, item_types_pair> item_names_map;
static item_names_map item_names_cache;

typedef std::map<unsigned, std::vector<std::string> > item_names_by_glyph_map;
static item_names_by_glyph_map item_names_by_glyph_cache;

void init_item_name_cache()
{
    item_names_cache.clear();
    item_names_by_glyph_cache.clear();

    for (int i = 0; i < NUM_OBJECT_CLASSES; i++)
    {
        object_class_type base_type = static_cast<object_class_type>(i);
        const int num_sub_types = get_max_subtype(base_type);

        for (int sub_type = 0; sub_type < num_sub_types; sub_type++)
        {
            if (base_type == OBJ_BOOKS)
            {
                if (sub_type == BOOK_RANDART_LEVEL
                    || sub_type == BOOK_RANDART_THEME)
                {
                    // These are randart only and have no fixed names.
                    continue;
                }
            }

            item_def item;
            item.base_type = base_type;
            item.sub_type = sub_type;
            if (is_deck(item))
            {
                item.plus = 1;
                item.special = DECK_RARITY_COMMON;
                init_deck(item);
            }
            std::string name = item.name(false, DESC_DBNAME, true, true);
            lowercase(name);
            glyph g = get_item_glyph(&item);

            if (base_type == OBJ_JEWELLERY && name == "buggy jewellery")
                continue;
            else if (name.find("buggy") != std::string::npos)
            {
                crawl_state.add_startup_error("Bad name for item name "
                                              " cache: " + name);
                continue;
            }

            if (item_names_cache.find(name) == item_names_cache.end())
            {
                item_names_cache[name].base_type = base_type;
                item_names_cache[name].sub_type = sub_type;
                if (g.ch)
                    item_names_by_glyph_cache[g.ch].push_back(name);
            }
        }
    }

    ASSERT(!item_names_cache.empty());
}

item_types_pair item_types_by_name(std::string name)
{
    lowercase(name);

    item_names_map::iterator i = item_names_cache.find(name);

    if (i != item_names_cache.end())
        return (i->second);

    item_types_pair err = {OBJ_UNASSIGNED, 0};

    return (err);
}

std::vector<std::string> item_name_list_for_glyph(unsigned glyph)
{
    item_names_by_glyph_map::iterator i;
    i = item_names_by_glyph_cache.find(glyph);

    if (i != item_names_by_glyph_cache.end())
        return (i->second);

    std::vector<std::string> empty;
    return empty;
}

bool is_named_corpse(const item_def &corpse)
{
    ASSERT(corpse.base_type == OBJ_CORPSES);

    return (corpse.props.exists(CORPSE_NAME_KEY));
}

std::string get_corpse_name(const item_def &corpse, uint64_t *name_type)
{
    ASSERT(corpse.base_type == OBJ_CORPSES);

    if (!corpse.props.exists(CORPSE_NAME_KEY))
        return ("");

    if (name_type != NULL)
#if TAG_MAJOR_VERSION == 32
        *name_type = (int64_t)corpse.props[CORPSE_NAME_TYPE_KEY];
#else
        *name_type = corpse.props[CORPSE_NAME_TYPE_KEY].get_int64();
#endif

    return (corpse.props[CORPSE_NAME_KEY].get_string());
}
