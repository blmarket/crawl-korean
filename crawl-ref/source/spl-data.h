/**
 * @file
 * @brief Spell definitions and descriptions. See spell_desc struct in
 *             spl-util.cc.
**/

/*
struct spell_desc
{
    enum, spell name,
    spell schools,
    flags,
    level,
    power_cap,
    min_range, max_range, (-1 if not applicable)
    noise_mod,
    target_prompt,
    monster spell: needs tracer?,
    monster spell: utility spell?
}
*/

{
    SPELL_TELEPORT_SELF, M_("Teleport Self"),
     SPTYP_TRANSLOCATION,
     SPFLAG_ESCAPE,
     5,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_CAUSE_FEAR, M_("Cause Fear"),
     SPTYP_HEXES,
     SPFLAG_AREA,
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_MAGIC_DART, M_("Magic Dart"),
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET,
     1,
     25,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_FIREBALL, M_("Fireball"),
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET,
     5,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_APPORTATION, M_("Apportation"),
     SPTYP_TRANSLOCATION,
     SPFLAG_TARG_OBJ | SPFLAG_NOT_SELF,
     1,
     1000,
     LOS_RADIUS, LOS_RADIUS,
     0,
     M_("Apport"),
     false,
     false
},

{
    SPELL_DELAYED_FIREBALL, M_("Delayed Fireball"),
     SPTYP_FIRE | SPTYP_CONJURATION,
     SPFLAG_NONE,
     7,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_CONJURE_FLAME, M_("Conjure Flame"),
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_GRID | SPFLAG_NOT_SELF,
     3,
     100,
     4, 4,
     0,
     NULL,
     false,
     false
},

{
    SPELL_DIG, M_("Dig"),
     SPTYP_EARTH,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF | SPFLAG_NEUTRAL,
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     true
},

{
    SPELL_BOLT_OF_FIRE, M_("Bolt of Fire"),
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET,
     6,
     200,
     7, 7,
     0,
     NULL,
     true,
     false
},

{
    SPELL_BOLT_OF_COLD, M_("Bolt of Cold"),
     SPTYP_CONJURATION | SPTYP_ICE,
     SPFLAG_DIR_OR_TARGET,
     6,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_LIGHTNING_BOLT, M_("Lightning Bolt"),
     SPTYP_CONJURATION | SPTYP_AIR,
     SPFLAG_DIR_OR_TARGET,
     5,
     200,
     5, 12, // capped at LOS, yet this 12 matters since range increases linearly
     0,
     NULL,
     true,
     false
},

{
    SPELL_BOLT_OF_MAGMA, M_("Bolt of Magma"),
     SPTYP_CONJURATION | SPTYP_FIRE | SPTYP_EARTH,
     SPFLAG_DIR_OR_TARGET,
     5,
     200,
     5, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_POLYMORPH, M_("Polymorph"),
     SPTYP_TRANSMUTATION | SPTYP_HEXES,
     SPFLAG_DIR_OR_TARGET | SPFLAG_CHAOTIC,
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SLOW, M_("Slow"),
     SPTYP_HEXES,
     SPFLAG_DIR_OR_TARGET,
     2,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_HASTE, M_("Haste"),
     SPTYP_CHARMS,
     SPFLAG_DIR_OR_TARGET | SPFLAG_HELPFUL | SPFLAG_HASTY,
     6,  // lowered to 6 from 8, since it's easily available from various items
         // and Swiftness is level 2 (and gives a similar effect).  It's also
         // not that much better than Invisibility. - bwr
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     true
},

{
    SPELL_PETRIFY, M_("Petrify"),
     SPTYP_TRANSMUTATION | SPTYP_EARTH,
     SPFLAG_DIR_OR_TARGET,
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_CONFUSE, M_("Confuse"),
     SPTYP_HEXES,
     SPFLAG_DIR_OR_TARGET,
     3,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_INVISIBILITY, M_("Invisibility"),
     SPTYP_HEXES,
     SPFLAG_DIR_OR_TARGET | SPFLAG_HELPFUL,
     6,
     200,
     LOS_RADIUS, LOS_RADIUS,
     -6,
     NULL,
     false,
     true
},

{
    SPELL_THROW_FLAME, M_("Throw Flame"),
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET,
     2,
     50,
     8, 8,
     0,
     NULL,
     true,
     false
},

{
    SPELL_THROW_FROST, M_("Throw Frost"),
     SPTYP_CONJURATION | SPTYP_ICE,
     SPFLAG_DIR_OR_TARGET,
     2,
     50,
     7, 7,
     0,
     NULL,
     true,
     false
},

{
    SPELL_CONTROLLED_BLINK, M_("Controlled Blink"),
     SPTYP_TRANSLOCATION,
     SPFLAG_ESCAPE,
     7,
     0,
     -1, -1,
     -4,   // Just a bit noisier than Blink, to keep this spell relevant
           // for stabbers. [rob]
     NULL,
     false,
     true
},

{
    SPELL_DISJUNCTION, M_("Disjunction"),
     SPTYP_TRANSLOCATION,
     SPFLAG_ESCAPE,
     8,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_FREEZING_CLOUD, M_("Freezing Cloud"),
     SPTYP_CONJURATION | SPTYP_ICE | SPTYP_AIR,
     SPFLAG_GRID | SPFLAG_AREA,
     6,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_MEPHITIC_CLOUD, M_("Mephitic Cloud"),
     SPTYP_CONJURATION | SPTYP_POISON | SPTYP_AIR,
     SPFLAG_DIR_OR_TARGET | SPFLAG_AREA | SPFLAG_ALLOW_SELF,
     3,
     100,
     5, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_RING_OF_FLAMES, M_("Ring of Flames"),
     SPTYP_CHARMS | SPTYP_FIRE,
     SPFLAG_AREA,
     7,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_VENOM_BOLT, M_("Venom Bolt"),
     SPTYP_CONJURATION | SPTYP_POISON,
     SPFLAG_DIR_OR_TARGET,
     5,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_OLGREBS_TOXIC_RADIANCE, M_("Olgreb's Toxic Radiance"),
     SPTYP_POISON,
     SPFLAG_AREA | SPFLAG_BATTLE,
     4,
     100,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_TELEPORT_OTHER, M_("Teleport Other"),
     SPTYP_TRANSLOCATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF | SPFLAG_ESCAPE,
     3,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_DEATHS_DOOR, M_("Death's Door"),
     SPTYP_CHARMS | SPTYP_NECROMANCY,
     SPFLAG_NONE,
     8,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_MASS_CONFUSION, M_("Mass Confusion"),
     SPTYP_HEXES,
     SPFLAG_AREA,
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SMITING, M_("Smiting"),
     SPTYP_NONE,
     SPFLAG_TARGET | SPFLAG_NOT_SELF, // divine ability, monsters
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     4,
     M_("Smite"),
     false,
     false
},

{
    SPELL_SUMMON_SMALL_MAMMAL, M_("Summon Small Mammal"),
     SPTYP_SUMMONING,
     SPFLAG_BATTLE,
     1,
     25,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_ABJURATION, M_("Abjuration"),
     SPTYP_SUMMONING,
     SPFLAG_TARGET | SPFLAG_ESCAPE,
     3,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_MASS_ABJURATION, M_("Mass Abjuration"),
     SPTYP_SUMMONING,
     SPFLAG_AREA | SPFLAG_NEUTRAL | SPFLAG_ESCAPE,
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_SCORPIONS, M_("Summon Scorpions"),
     SPTYP_SUMMONING | SPTYP_POISON,
     SPFLAG_BATTLE,
     4,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_BOLT_OF_DRAINING, M_("Bolt of Draining"),
     SPTYP_CONJURATION | SPTYP_NECROMANCY,
     SPFLAG_DIR_OR_TARGET,
     6,
     200,
     6, 6,
     -3, //the beam is silent
     NULL,
     true,
     false
},

{
    SPELL_LEHUDIBS_CRYSTAL_SPEAR, M_("Lehudib's Crystal Spear"),
     SPTYP_CONJURATION | SPTYP_EARTH,
     SPFLAG_DIR_OR_TARGET,
     8,
     200,
     4, 4,
     0,
     NULL,
     true,
     false
},

{
    SPELL_BOLT_OF_INACCURACY, M_("Bolt of Inaccuracy"),
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET, // rod/tome of destruction
     3,
     1000,
     7, 7,
     0,
     NULL,
     true,
     false
},

{
    SPELL_TORNADO, M_("Tornado"),
     SPTYP_AIR,
     SPFLAG_AREA,
     9,
     200,
     TORNADO_RADIUS, TORNADO_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_POISONOUS_CLOUD, M_("Poisonous Cloud"),
     SPTYP_CONJURATION | SPTYP_POISON | SPTYP_AIR,
     SPFLAG_GRID | SPFLAG_AREA | SPFLAG_ALLOW_SELF,
     6,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_FIRE_STORM, M_("Fire Storm"),
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_GRID | SPFLAG_AREA,
     9,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_HELLFIRE_BURST, M_("Hellfire Burst"),
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_GRID | SPFLAG_AREA | SPFLAG_UNHOLY,
     9,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_BLINK, M_("Blink"),
     SPTYP_TRANSLOCATION,
     SPFLAG_ESCAPE,
     2,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_BLINK_RANGE, M_("Blink Range"), // XXX needs better name
     SPTYP_TRANSLOCATION,
     SPFLAG_ESCAPE | SPFLAG_MONSTER,
     2,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_BLINK_AWAY, M_("Blink Away"),
     SPTYP_TRANSLOCATION,
     SPFLAG_ESCAPE | SPFLAG_MONSTER,
     2,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_BLINK_CLOSE, M_("Blink Close"),
     SPTYP_TRANSLOCATION,
     SPFLAG_MONSTER,
     2,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

// The following name was found in the hack.exe file of an early version
// of PCHACK - credit goes to its creator (whoever that may be):
{
    SPELL_ISKENDERUNS_MYSTIC_BLAST, M_("Iskenderun's Mystic Blast"),
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET,
     4,
     100,
     7, 7,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SUMMON_SWARM, M_("Summon Swarm"),
     SPTYP_SUMMONING,
     SPFLAG_BATTLE,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_HORRIBLE_THINGS, M_("Summon Horrible Things"),
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY | SPFLAG_BATTLE | SPFLAG_CHAOTIC,
     8,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_MALIGN_GATEWAY, M_("Malign Gateway"),
     SPTYP_SUMMONING | SPTYP_TRANSLOCATION,
     SPFLAG_UNHOLY | SPFLAG_BATTLE | SPFLAG_CHAOTIC,
     7,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_ENSLAVEMENT, M_("Enslavement"),
     SPTYP_HEXES,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_ANIMATE_DEAD, M_("Animate Dead"),
     SPTYP_NECROMANCY,
     SPFLAG_AREA | SPFLAG_NEUTRAL | SPFLAG_CORPSE_VIOLATING,
     4,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_PAIN, M_("Pain"),
     SPTYP_NECROMANCY,
     SPFLAG_DIR_OR_TARGET | SPFLAG_BATTLE,
     1,
     25,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_CONTROL_UNDEAD, M_("Control Undead"),
     SPTYP_NECROMANCY,
     SPFLAG_NONE,
     4,
     200,
     -1, -1,
     0,
     NULL,
     true,
     false
},

{
    SPELL_ANIMATE_SKELETON, M_("Animate Skeleton"),
     SPTYP_NECROMANCY,
     SPFLAG_CORPSE_VIOLATING,
     1,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_VAMPIRIC_DRAINING, M_("Vampiric Draining"),
     SPTYP_NECROMANCY,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF | SPFLAG_BATTLE,
     3,
     200,
     1, 1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_HAUNT, M_("Haunt"),
     SPTYP_SUMMONING | SPTYP_NECROMANCY,
     SPFLAG_TARGET | SPFLAG_NOT_SELF,
     7,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_BORGNJORS_REVIVIFICATION, M_("Borgnjor's Revivification"),
     SPTYP_NECROMANCY,
     SPFLAG_NONE,
     7,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_FREEZE, M_("Freeze"),
     SPTYP_ICE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF | SPFLAG_BATTLE,
     1,
     25,
     1, 1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_ELEMENTAL, M_("Summon Elemental"),
     SPTYP_SUMMONING,
     SPFLAG_BATTLE,
     4,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_OZOCUBUS_REFRIGERATION, M_("Ozocubu's Refrigeration"),
     SPTYP_ICE,
     SPFLAG_AREA,
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_STICKY_FLAME, M_("Sticky Flame"),
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET,
     4,
     100,
     1, 1,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SUMMON_ICE_BEAST, M_("Summon Ice Beast"),
     SPTYP_ICE | SPTYP_SUMMONING,
     SPFLAG_BATTLE,
     4,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_OZOCUBUS_ARMOUR, M_("Ozocubu's Armour"),
     SPTYP_CHARMS | SPTYP_ICE,
     SPFLAG_NONE,
     3,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_CALL_IMP, M_("Call Imp"),
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY | SPFLAG_BATTLE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_REPEL_MISSILES, M_("Repel Missiles"),
     SPTYP_CHARMS | SPTYP_AIR,
     SPFLAG_NONE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_BERSERKER_RAGE, M_("Berserker Rage"),
     SPTYP_CHARMS,
     SPFLAG_HASTY | SPFLAG_MONSTER,
     3,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_FRENZY, M_("Frenzy"),
     SPTYP_CHARMS,
     SPFLAG_HASTY | SPFLAG_MONSTER,
     3,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},
#endif

{
    SPELL_DISPEL_UNDEAD, M_("Dispel Undead"),
     SPTYP_NECROMANCY,
     SPFLAG_DIR_OR_TARGET,
     5,
     100,
     5, 5,
     0,
     NULL,
     true,
     false
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_FULSOME_DISTILLATION, M_("Fulsome Distillation"),
     SPTYP_TRANSMUTATION | SPTYP_NECROMANCY,
     SPFLAG_CORPSE_VIOLATING,
     1,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},
#endif

{
    SPELL_POISON_ARROW, M_("Poison Arrow"),
     SPTYP_CONJURATION | SPTYP_POISON,
     SPFLAG_DIR_OR_TARGET,
     6,
     200,
     7, 7,
     0,
     NULL,
     true,
     false
},

{
    SPELL_TWISTED_RESURRECTION, M_("Twisted Resurrection"),
     SPTYP_NECROMANCY,
     SPFLAG_CHAOTIC | SPFLAG_CORPSE_VIOLATING,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_REGENERATION, M_("Regeneration"),
     SPTYP_CHARMS | SPTYP_NECROMANCY,
     SPFLAG_NONE,
     3,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

// Monster-only, players can use Lugonu's ability
{
    SPELL_BANISHMENT, M_("Banishment"),
     SPTYP_TRANSLOCATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_UNHOLY | SPFLAG_CHAOTIC | SPFLAG_MONSTER,
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_CIGOTUVIS_DEGENERATION, M_("Cigotuvi's Degeneration"),
     SPTYP_TRANSMUTATION | SPTYP_NECROMANCY,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF | SPFLAG_CHAOTIC,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},
#endif

{
    SPELL_STING, M_("Sting"),
     SPTYP_CONJURATION | SPTYP_POISON,
     SPFLAG_DIR_OR_TARGET,
     1,
     25,
     7, 7,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SUBLIMATION_OF_BLOOD, M_("Sublimation of Blood"),
     SPTYP_NECROMANCY,
     SPFLAG_CORPSE_VIOLATING,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_TUKIMAS_DANCE, M_("Tukima's Dance"),
     SPTYP_HEXES,
     SPFLAG_NONE,
     5,
     100,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_DEMON, M_("Summon Demon"),
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY | SPFLAG_BATTLE,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_DEMONIC_HORDE, M_("Demonic Horde"),
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY | SPFLAG_BATTLE,
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_GREATER_DEMON, M_("Summon Greater Demon"),
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY | SPFLAG_BATTLE,
     7,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_CORPSE_ROT, M_("Corpse Rot"),
     SPTYP_NECROMANCY,
     SPFLAG_AREA | SPFLAG_NEUTRAL | SPFLAG_UNCLEAN,
     2,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_FIRE_BRAND, M_("Fire Brand"),
     SPTYP_CHARMS | SPTYP_FIRE,
     SPFLAG_HELPFUL | SPFLAG_BATTLE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_FREEZING_AURA, M_("Freezing Aura"),
     SPTYP_CHARMS | SPTYP_ICE,
     SPFLAG_HELPFUL | SPFLAG_BATTLE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_LETHAL_INFUSION, M_("Lethal Infusion"),
     SPTYP_CHARMS | SPTYP_NECROMANCY,
     SPFLAG_HELPFUL | SPFLAG_BATTLE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_IRON_SHOT, M_("Iron Shot"),
     SPTYP_CONJURATION | SPTYP_EARTH,
     SPFLAG_DIR_OR_TARGET,
     6,
     200,
     5, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_STONE_ARROW, M_("Stone Arrow"),
     SPTYP_CONJURATION | SPTYP_EARTH,
     SPFLAG_DIR_OR_TARGET,
     3,
     50,
     5, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SHOCK, M_("Shock"),
     SPTYP_CONJURATION | SPTYP_AIR,
     SPFLAG_DIR_OR_TARGET,
     1,
     25,
     8, 8,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SWIFTNESS, M_("Swiftness"),
     SPTYP_CHARMS | SPTYP_AIR,
     SPFLAG_HASTY,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_FLY, M_("Flight"),
     SPTYP_CHARMS | SPTYP_AIR,
     SPFLAG_NONE,
     3,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_INSULATION, M_("Insulation"),
     SPTYP_CHARMS | SPTYP_AIR,
     SPFLAG_NONE,
     4,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},
#endif

{
    SPELL_CURE_POISON, M_("Cure Poison"),
     SPTYP_POISON,
     SPFLAG_RECOVERY | SPFLAG_HELPFUL,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_CONTROL_TELEPORT, M_("Control Teleport"),
     SPTYP_CHARMS | SPTYP_TRANSLOCATION,
     SPFLAG_HELPFUL,
     4,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_POISON_WEAPON, M_("Poison Weapon"),
     SPTYP_CHARMS | SPTYP_POISON,
     SPFLAG_HELPFUL | SPFLAG_BATTLE,
     3,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_DEBUGGING_RAY, M_("Debugging Ray"),
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_TESTING,
     7,
     100,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_RECALL, M_("Recall"),
     SPTYP_SUMMONING | SPTYP_TRANSLOCATION,
     SPFLAG_NONE,
     3,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_AGONY, M_("Agony"),
     SPTYP_NECROMANCY,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SPIDER_FORM, M_("Spider Form"),
     SPTYP_TRANSMUTATION | SPTYP_POISON,
     SPFLAG_HELPFUL | SPFLAG_CHAOTIC,
     3,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_DISINTEGRATE, M_("Disintegrate"),
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     6,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_BLADE_HANDS, M_("Blade Hands"),
     SPTYP_TRANSMUTATION,
     SPFLAG_HELPFUL | SPFLAG_BATTLE | SPFLAG_CHAOTIC,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_STATUE_FORM, M_("Statue Form"),
     SPTYP_TRANSMUTATION | SPTYP_EARTH,
     SPFLAG_HELPFUL | SPFLAG_CHAOTIC,
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_ICE_FORM, M_("Ice Form"),
     SPTYP_ICE | SPTYP_TRANSMUTATION,
     SPFLAG_HELPFUL | SPFLAG_CHAOTIC,
     4,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_DRAGON_FORM, M_("Dragon Form"),
     SPTYP_TRANSMUTATION,
     SPFLAG_HELPFUL | SPFLAG_CHAOTIC,
     7,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_NECROMUTATION, M_("Necromutation"),
     SPTYP_TRANSMUTATION | SPTYP_NECROMANCY,
     SPFLAG_HELPFUL | SPFLAG_CORPSE_VIOLATING | SPFLAG_CHAOTIC,
     8,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_DEATH_CHANNEL, M_("Death Channel"),
     SPTYP_NECROMANCY,
     SPFLAG_HELPFUL,
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

// Monster-only, players can use Kiku's ability
{
    SPELL_SYMBOL_OF_TORMENT, M_("Symbol of Torment"),
     SPTYP_NECROMANCY,
     SPFLAG_AREA | SPFLAG_MONSTER,
     6,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_DEFLECT_MISSILES, M_("Deflect Missiles"),
     SPTYP_CHARMS | SPTYP_AIR,
     SPFLAG_HELPFUL,
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_THROW_ICICLE, M_("Throw Icicle"),
     SPTYP_CONJURATION | SPTYP_ICE,
     SPFLAG_DIR_OR_TARGET,
     4,
     100,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_ICE_STORM, M_("Ice Storm"),
     SPTYP_CONJURATION | SPTYP_ICE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_AREA,
     9,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_AIRSTRIKE, M_("Airstrike"),
     SPTYP_AIR,
     SPFLAG_TARGET | SPFLAG_NOT_SELF | SPFLAG_BATTLE,
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SHADOW_CREATURES, M_("Shadow Creatures"),
     SPTYP_SUMMONING,
     SPFLAG_NONE,
     5,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_CONFUSING_TOUCH, M_("Confusing Touch"),
     SPTYP_HEXES,
     SPFLAG_NONE,
     1,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SURE_BLADE, M_("Sure Blade"),
     SPTYP_HEXES | SPTYP_CHARMS,
     SPFLAG_HELPFUL | SPFLAG_BATTLE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_FLAME_TONGUE, M_("Flame Tongue"),
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     1,
     40,                           // cap for range; damage cap is at 25
     2, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_PASSWALL, M_("Passwall"),
     SPTYP_TRANSMUTATION | SPTYP_EARTH,
     SPFLAG_DIR | SPFLAG_ESCAPE | SPFLAG_NOT_SELF,
     3,
     200,
     1, 1,
     -3,        // make silent to keep passwall a viable stabbing spell [rob]
     NULL,
     false,
     true
},

{
    SPELL_IGNITE_POISON, M_("Ignite Poison"),
     SPTYP_FIRE | SPTYP_TRANSMUTATION,
     SPFLAG_AREA | SPFLAG_BATTLE,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_STICKS_TO_SNAKES, M_("Sticks to Snakes"),
     SPTYP_TRANSMUTATION,
     SPFLAG_BATTLE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_CALL_CANINE_FAMILIAR, M_("Call Canine Familiar"),
     SPTYP_SUMMONING,
     SPFLAG_NONE,
     3,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_DRAGON, M_("Summon Dragon"),
     SPTYP_SUMMONING,
     SPFLAG_NONE,
     9,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_HIBERNATION, M_("Ensorcelled Hibernation"),
     SPTYP_HEXES | SPTYP_ICE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     2,
     56,
     LOS_RADIUS, LOS_RADIUS,
     -2, //putting a monster to sleep should be silent
     NULL,
     true,
     false
},

{
    SPELL_ENGLACIATION, M_("Metabolic Englaciation"),
     SPTYP_HEXES | SPTYP_ICE,
     SPFLAG_AREA,
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_SEE_INVISIBLE, M_("See Invisible"),
     SPTYP_CHARMS,
     SPFLAG_HELPFUL,
     4,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},
#endif

{
    SPELL_PHASE_SHIFT, M_("Phase Shift"),
     SPTYP_TRANSLOCATION,
     SPFLAG_HELPFUL,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_SUMMON_BUTTERFLIES, M_("Summon Butterflies"),
     SPTYP_SUMMONING,
     SPFLAG_NONE,
     1,
     100,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_WARP_BRAND, M_("Warp Weapon"),
     SPTYP_CHARMS | SPTYP_TRANSLOCATION,
     SPFLAG_HELPFUL | SPFLAG_BATTLE,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_SILENCE, M_("Silence"),
     SPTYP_HEXES | SPTYP_AIR,
     SPFLAG_AREA,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SHATTER, M_("Shatter"),
     SPTYP_EARTH,
     SPFLAG_AREA,
     9,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_DISPERSAL, M_("Dispersal"),
     SPTYP_TRANSLOCATION,
     SPFLAG_AREA | SPFLAG_ESCAPE,
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_DISCHARGE, M_("Static Discharge"),
     SPTYP_CONJURATION | SPTYP_AIR,
     SPFLAG_AREA,
     3,
     100,
     1, 1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_CORONA, M_("Corona"),
     SPTYP_HEXES,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     1,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_INTOXICATE, M_("Alistair's Intoxication"),
     SPTYP_TRANSMUTATION | SPTYP_POISON,
     SPFLAG_NONE,
     4,
     100,
     -1, -1,
     0,
     NULL,
     false,
     false
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_EVAPORATE, M_("Evaporate"),
     SPTYP_FIRE | SPTYP_TRANSMUTATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_AREA | SPFLAG_ALLOW_SELF,
     2,   // XXX: level 2 or 3, what should it be now? -- bwr
     50,
     6, 6,
     0,
     NULL,
     true,
     false
},
#endif

{
    SPELL_LRD, M_("Lee's Rapid Deconstruction"),
     SPTYP_EARTH,
     SPFLAG_GRID,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     N_("Fragment what (e.g. wall or brittle monster)?"),
     false,
     false
},

{
    SPELL_SANDBLAST, M_("Sandblast"),
     SPTYP_EARTH,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF | SPFLAG_BATTLE,
     1,
     50,
     2, 2, // more with wielded stones
     0,
     NULL,
     true,
     false
},

{
    SPELL_CONDENSATION_SHIELD, M_("Condensation Shield"),
     SPTYP_ICE,
     SPFLAG_HELPFUL,
     4,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_STONESKIN, M_("Stoneskin"),
     SPTYP_EARTH | SPTYP_TRANSMUTATION, // was ench -- bwr
     SPFLAG_HELPFUL,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_SIMULACRUM, M_("Simulacrum"),
     SPTYP_ICE | SPTYP_NECROMANCY,
     SPFLAG_CORPSE_VIOLATING,
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_CONJURE_BALL_LIGHTNING, M_("Conjure Ball Lightning"),
     SPTYP_AIR | SPTYP_CONJURATION,
     SPFLAG_NONE,
     7,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_CHAIN_LIGHTNING, M_("Chain Lightning"),
     SPTYP_AIR | SPTYP_CONJURATION,
     SPFLAG_AREA,
     8,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_EXCRUCIATING_WOUNDS, M_("Excruciating Wounds"),
     SPTYP_CHARMS | SPTYP_NECROMANCY,
     SPFLAG_HELPFUL | SPFLAG_BATTLE,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_PORTAL_PROJECTILE, M_("Portal Projectile"),
     SPTYP_TRANSLOCATION,
     SPFLAG_TARGET | SPFLAG_BATTLE,
     2,
     50,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_UGLY_THING, M_("Summon Ugly Thing"),
     SPTYP_SUMMONING,
     SPFLAG_CHAOTIC,
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_GOLUBRIAS_PASSAGE, M_("Passage of Golubria"),
     SPTYP_TRANSLOCATION,
     SPFLAG_GRID | SPFLAG_NEUTRAL | SPFLAG_ESCAPE,
     4,
     0,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_FULMINANT_PRISM, M_("Fulminant Prism"),
     SPTYP_CONJURATION | SPTYP_HEXES,
     SPFLAG_GRID | SPFLAG_AREA | SPFLAG_NOT_SELF,
     5,
     200,
     5, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_PARALYSE, M_("Paralyse"),
     SPTYP_HEXES,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER,
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_MINOR_HEALING, M_("Minor Healing"),
     SPTYP_NONE,
     SPFLAG_RECOVERY | SPFLAG_HELPFUL | SPFLAG_MONSTER,
     2,
     0,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     true
},

{
    SPELL_MAJOR_HEALING, M_("Major Healing"),
     SPTYP_NONE,
     SPFLAG_RECOVERY | SPFLAG_HELPFUL | SPFLAG_MONSTER,
     6,
     0,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     true
},

{
    SPELL_HELLFIRE, M_("Hellfire"),
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_UNHOLY | SPFLAG_MONSTER,
        // plus DS ability, staff of Dispater & Sceptre of Asmodeus
     9,
     200,
     7, 7,
     0,
     NULL,
     true,
     false
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_VAMPIRE_SUMMON, M_("Vampire Summon"),
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY | SPFLAG_MONSTER,
     3,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},
#endif

{
    SPELL_BRAIN_FEED, M_("Brain Feed"),
     SPTYP_NECROMANCY,
     SPFLAG_TARGET | SPFLAG_MONSTER,
     3,
     0,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_FAKE_RAKSHASA_SUMMON, M_("Rakshasa Summon"),
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY | SPFLAG_MONSTER,
     3,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_NOXIOUS_CLOUD, M_("Noxious Cloud"),
     SPTYP_CONJURATION | SPTYP_POISON | SPTYP_AIR,
     SPFLAG_GRID | SPFLAG_AREA | SPFLAG_MONSTER,
     5,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_STEAM_BALL, M_("Steam Ball"),
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER,
     4,
     0,
     7, 7,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SUMMON_UFETUBUS, M_("Summon Ufetubus"),
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY | SPFLAG_MONSTER,
     4,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_HELL_BEAST, M_("Summon Hell Beast"),
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY | SPFLAG_MONSTER,
     4,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_ENERGY_BOLT, M_("Energy Bolt"),
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER,
     4,
     0,
     8, 8,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SPIT_POISON, M_("Spit Poison"),
     SPTYP_POISON,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER | SPFLAG_INNATE | SPFLAG_NOISY,
     2,
     0,
     7, 7,
     2,
     NULL,
     true,
     false
},

{
    SPELL_SUMMON_UNDEAD, M_("Summon Undead"),
     SPTYP_SUMMONING | SPTYP_NECROMANCY,
     SPFLAG_MONSTER,
     7,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false,
},

{
    SPELL_CANTRIP, M_("Cantrip"),
     SPTYP_NONE,
     SPFLAG_MONSTER,
     1,
     0,
     -1, -1,
     1,
     NULL,
     false,
     false
},

{
    SPELL_QUICKSILVER_BOLT, M_("Quicksilver Bolt"),
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER,
     5,
     0,
     8, 8,
     0,
     NULL,
     true,
     false
},

{
    SPELL_METAL_SPLINTERS, M_("Metal Splinters"),
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER,
     5,
     0,
     5, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_MIASMA_BREATH, M_("Miasma Breath"),
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_UNCLEAN | SPFLAG_MONSTER,
     6,
     0,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SUMMON_DRAKES, M_("Summon Drakes"),
     SPTYP_SUMMONING | SPTYP_NECROMANCY, // since it can summon shadow dragons
     SPFLAG_UNCLEAN | SPFLAG_MONSTER,
     6,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_BLINK_OTHER, M_("Blink Other"),
     SPTYP_TRANSLOCATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF | SPFLAG_ESCAPE | SPFLAG_MONSTER,
     2,
     0,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_BLINK_OTHER_CLOSE, M_("Blink Other Close"),
     SPTYP_TRANSLOCATION,
     SPFLAG_TARGET | SPFLAG_NOT_SELF | SPFLAG_MONSTER,
     2,
     0,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SUMMON_MUSHROOMS, M_("Summon Mushrooms"),
     SPTYP_SUMMONING,
     SPFLAG_MONSTER,
     4,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SPIT_ACID, M_("Spit Acid"),
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER | SPFLAG_INNATE | SPFLAG_NOISY,
     5,
     0,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

// Monster version of the spell (with full range)
{
    SPELL_STICKY_FLAME_RANGE, M_("Sticky Flame Range"),
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER,
     4,
     100,
     5, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_STICKY_FLAME_SPLASH, M_("Sticky Flame Splash"),
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER | SPFLAG_INNATE | SPFLAG_NOISY,
     4,
     100,
     5, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_FIRE_BREATH, M_("Fire Breath"),
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER | SPFLAG_INNATE | SPFLAG_NOISY,
     5,
     0,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_CHAOS_BREATH, M_("Chaos Breath"),
     SPTYP_CONJURATION | SPTYP_RANDOM,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER | SPFLAG_INNATE | SPFLAG_NOISY,
     5,
     0,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_COLD_BREATH, M_("Cold Breath"),
     SPTYP_CONJURATION | SPTYP_ICE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER | SPFLAG_INNATE | SPFLAG_NOISY,
     5,
     0,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_DRACONIAN_BREATH, M_("Draconian Breath"),
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER | SPFLAG_INNATE | SPFLAG_NOISY,
     8,
     0,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_WATER_ELEMENTALS, M_("Summon Water Elementals"),
     SPTYP_SUMMONING,
     SPFLAG_MONSTER,
     5,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_PORKALATOR, M_("Porkalator"),
     SPTYP_HEXES | SPTYP_TRANSMUTATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_CHAOTIC,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_CREATE_TENTACLES, M_("Spawn Tentacles"),
     SPTYP_SUMMONING,
     SPFLAG_MONSTER,
     5,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_TOMB_OF_DOROKLOHE, M_("Tomb of Doroklohe"),
     SPTYP_EARTH,
     SPFLAG_MONSTER,
     5,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_EYEBALLS, M_("Summon Eyeballs"),
     SPTYP_SUMMONING,
     SPFLAG_MONSTER,
     5,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_HASTE_OTHER, M_("Haste Other"),
     SPTYP_HEXES,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF | SPFLAG_HELPFUL | SPFLAG_HASTY,
     6,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     true
},

{
    SPELL_EARTH_ELEMENTALS, M_("Summon Earth Elementals"),
     SPTYP_SUMMONING,
     SPFLAG_MONSTER,
     5,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_AIR_ELEMENTALS, M_("Summon Air Elementals"),
     SPTYP_SUMMONING,
     SPFLAG_MONSTER,
     5,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_FIRE_ELEMENTALS, M_("Summon Fire Elementals"),
     SPTYP_SUMMONING,
     SPFLAG_MONSTER,
     5,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_IRON_ELEMENTALS, M_("Summon Iron Elementals"),
     SPTYP_SUMMONING,
     SPFLAG_MONSTER,
     5,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SLEEP, M_("Sleep"),
     SPTYP_HEXES,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_FAKE_MARA_SUMMON, M_("Mara Summon"),
     SPTYP_SUMMONING,
     SPFLAG_MONSTER,
     5,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_SUMMON_RAKSHASA, M_("Summon Rakshasa"),
     SPTYP_SUMMONING,
     SPFLAG_MONSTER,
     5,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},
#endif

{
    SPELL_MISLEAD, M_("Mislead"),
     SPTYP_HEXES,
     SPFLAG_TARGET | SPFLAG_NOT_SELF,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_ILLUSION, M_("Summon Illusion"),
     SPTYP_SUMMONING,
     SPFLAG_MONSTER,
     5,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_PRIMAL_WAVE, M_("Primal Wave"),
     SPTYP_CONJURATION | SPTYP_ICE,
     SPFLAG_DIR_OR_TARGET,
     6,
     200,
     7, 7,
     0,
     NULL,
     true,
     false
},

{
    SPELL_CALL_TIDE, M_("Call Tide"),
     SPTYP_TRANSLOCATION,
     SPFLAG_MONSTER,
     7,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_IOOD, M_("Orb of Destruction"),
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     7,
     200,
     9, 9,
     0,
     NULL,
     true,
     false
},

{
    SPELL_INK_CLOUD, M_("Ink Cloud"),
     SPTYP_CONJURATION | SPTYP_ICE, // it's a water spell
     SPFLAG_MONSTER,
     7,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_MIGHT, M_("Might"),
     SPTYP_CHARMS,
     SPFLAG_DIR_OR_TARGET | SPFLAG_HELPFUL,
     3,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     true
},

{
    SPELL_MIGHT_OTHER, M_("Might Other"),
     SPTYP_CHARMS,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF | SPFLAG_HELPFUL,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     true
},

{
    SPELL_SUNRAY, M_("Sunray"),
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET,
     6,
     200,
     9, 9,
     -9,
     NULL,
     true,
     false
},

{
    SPELL_AWAKEN_FOREST, M_("Awaken Forest"),
     SPTYP_HEXES,
     SPFLAG_AREA,
     6,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_DRUIDS_CALL, M_("Druid's Call"),
     SPTYP_CHARMS,
     SPFLAG_MONSTER,
     6,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_BROTHERS_IN_ARMS, M_("Brothers in Arms"),
     SPTYP_SUMMONING,
     SPFLAG_MONSTER,
     6,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_TROGS_HAND, M_("Trog's Hand"),
     SPTYP_NONE,
     SPFLAG_MONSTER,
     3,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_SPECTRAL_ORCS, M_("Summon Spectral Orcs"),
     SPTYP_NECROMANCY,
     SPFLAG_MONSTER | SPFLAG_TARGET,
     4,
     0,
     LOS_RADIUS, LOS_RADIUS,
     1,
     NULL,
     false,
     false,
},

{
    SPELL_HOLY_LIGHT, M_("Holy Light"),
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET,
     6,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_HEAL_OTHER, M_("Heal Other"),
     SPTYP_NONE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF | SPFLAG_HELPFUL,
     6,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     true
},

{
    SPELL_HOLY_FLAMES, M_("Holy Flames"),
     SPTYP_NONE,
     SPFLAG_TARGET | SPFLAG_NOT_SELF | SPFLAG_BATTLE,
     7,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_HOLY_BREATH, M_("Holy Breath"),
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_AREA,
     5,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_INJURY_MIRROR, M_("Injury Mirror"),
     SPTYP_NONE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_HELPFUL,
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     true
},

{
    SPELL_DRAIN_LIFE, M_("Drain Life"),
     SPTYP_NECROMANCY,
     SPFLAG_AREA,
     6,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_LEDAS_LIQUEFACTION, M_("Leda's Liquefaction"),
     SPTYP_EARTH | SPTYP_HEXES,
     SPFLAG_AREA,
     4,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_HYDRA, M_("Summon Hydra"),
     SPTYP_SUMMONING,
     SPFLAG_NONE,
     7,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_DARKNESS, M_("Darkness"),
     SPTYP_HEXES,
     SPFLAG_NONE,
     7,
     200,
     -1, -1,
     -4,
     NULL,
     false,
     false
},

{
    SPELL_MESMERISE, M_("Mesmerise"),
     SPTYP_HEXES,
     SPFLAG_AREA,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_MELEE, M_("Melee"),
     0,
     SPFLAG_BATTLE,
     1,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_FIRE_SUMMON, M_("Fire Summon"),
     SPTYP_SUMMONING | SPTYP_FIRE,
     SPFLAG_MONSTER,
     8,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_PETRIFYING_CLOUD, M_("Petrifying Cloud"),
    SPTYP_CONJURATION | SPTYP_EARTH | SPTYP_AIR,
    SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    NULL,
    true,
    false
},

{
    SPELL_SHROUD_OF_GOLUBRIA, M_("Shroud of Golubria"),
     SPTYP_CHARMS | SPTYP_TRANSLOCATION,
     SPFLAG_NONE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_INNER_FLAME, M_("Inner Flame"),
     SPTYP_HEXES | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     3,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_BEASTLY_APPENDAGE, M_("Beastly Appendage"),
     SPTYP_TRANSMUTATION,
     SPFLAG_HELPFUL | SPFLAG_CHAOTIC,
     1,
     50,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_SILVER_BLAST, M_("Silver Blast"),
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET,
     5,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_ENSNARE, M_("Ensnare"),
     SPTYP_CONJURATION | SPTYP_HEXES,
     SPFLAG_DIR_OR_TARGET,
     6,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_THUNDERBOLT, M_("Thunderbolt"),
     SPTYP_CONJURATION | SPTYP_AIR,
     SPFLAG_DIR_OR_TARGET,
     2, // 2-5
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_BATTLESPHERE, M_("Iskenderun's Battlesphere"),
      SPTYP_CONJURATION | SPTYP_CHARMS,
      SPFLAG_NONE,
      4,
      100,
      -1, -1,
      0,
      NULL,
      false,
      true
},

{
    SPELL_SUMMON_MINOR_DEMON, M_("Summon Minor Demon"),
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY | SPFLAG_BATTLE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_MALMUTATE, M_("Malmutate"),
     SPTYP_TRANSMUTATION | SPTYP_HEXES,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF | SPFLAG_CHAOTIC,
     6,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SUMMON_TWISTER, M_("Summon Twister"),
     SPTYP_SUMMONING | SPTYP_AIR,
     SPFLAG_UNCLEAN | SPFLAG_MONSTER,
     9,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_DAZZLING_SPRAY, M_("Dazzling Spray"),
     SPTYP_CONJURATION | SPTYP_HEXES,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     3,
     100,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_FORCE_LANCE, M_("Force Lance"),
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET,
     5,
     100,
     4, 4,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SENTINEL_MARK, M_("Sentinel's Mark"),
     SPTYP_HEXES,
     SPFLAG_DIR_OR_TARGET,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

// Ironbrand Convoker version (delayed activation, recalls only humanoids)
{
    SPELL_WORD_OF_RECALL, M_("Word of Recall"),
     SPTYP_SUMMONING | SPTYP_TRANSLOCATION,
     SPFLAG_NONE,
     3,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_INJURY_BOND, M_("Injury Bond"),
     SPTYP_CHARMS,
     SPFLAG_AREA | SPFLAG_HELPFUL,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_GHOSTLY_FLAMES, "Ghostly Flames",
     SPTYP_CONJURATION | SPTYP_NECROMANCY,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER | SPFLAG_UNHOLY,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_GHOSTLY_FIREBALL, "Ghostly Fireball",
     SPTYP_CONJURATION | SPTYP_NECROMANCY,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER | SPFLAG_UNHOLY,
     5,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_CALL_LOST_SOUL, "Call Lost Soul",
     SPTYP_SUMMONING | SPTYP_NECROMANCY,
     SPFLAG_UNHOLY | SPFLAG_BATTLE | SPFLAG_MONSTER,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_DIMENSION_ANCHOR, "Dimension Anchor",
     SPTYP_TRANSLOCATION | SPTYP_HEXES,
     SPFLAG_DIR_OR_TARGET,
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_BLINK_ALLIES_ENCIRCLE, "Blink Allies Encircling",
     SPTYP_TRANSLOCATION,
     SPFLAG_AREA,
     6,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_AWAKEN_VINES, "Awaken Vines",
     SPTYP_HEXES,
     SPFLAG_AREA | SPFLAG_MONSTER,
     6,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_CONTROL_WINDS, "Control Winds",
     SPTYP_CHARMS | SPTYP_AIR,
     SPFLAG_AREA | SPFLAG_MONSTER,
     6,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_THORN_VOLLEY, "Volley of Thorns",
     SPTYP_CONJURATION | SPTYP_EARTH,
     SPFLAG_DIR_OR_TARGET,
     4,
     100,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_WALL_OF_BRAMBLES, "Wall of Brambles",
     SPTYP_CONJURATION | SPTYP_EARTH,
     SPFLAG_AREA | SPFLAG_MONSTER,
     5,
     100,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_WATERSTRIKE, "Waterstrike",
     SPTYP_ICE,
     SPFLAG_TARGET | SPFLAG_NOT_SELF | SPFLAG_BATTLE | SPFLAG_MONSTER,
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_HASTE_PLANTS, "Haste Plants",
     SPTYP_CHARMS,
     SPFLAG_AREA | SPFLAG_HELPFUL,
     6,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_WIND_BLAST, "Wind Blast",
     SPTYP_AIR,
     SPFLAG_AREA,
     3,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_STRIP_RESISTANCE, "Strip Resistance",
     SPTYP_HEXES,
     SPFLAG_DIR_OR_TARGET,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_INFUSION, "Infusion",
     SPTYP_CHARMS,
     SPFLAG_BATTLE,
     1,
     50,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_SONG_OF_SLAYING, "Song of Slaying",
     SPTYP_CHARMS,
     SPFLAG_BATTLE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_SONG_OF_SHIELDING, "Song of Shielding",
     SPTYP_CHARMS,
     SPFLAG_BATTLE,
     4,
     100,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_SPECTRAL_WEAPON, "Spectral Weapon",
     SPTYP_HEXES | SPTYP_CHARMS,
     SPFLAG_BATTLE,
     3,
     100,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_SUMMON_VERMIN, "Summon Vermin",
     SPTYP_SUMMONING,
     SPFLAG_MONSTER,
     5,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_MALIGN_OFFERING, "Malign Offering",
     SPTYP_NECROMANCY,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SEARING_RAY, "Searing Ray",
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET,
     2,
     50,
     4, 4,
     0,
     NULL,
     true,
     false
},

{
    SPELL_DISCORD, "Discord",
     SPTYP_HEXES,
     SPFLAG_AREA | SPFLAG_HASTY,
     8,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_NO_SPELL, "nonexistent spell",
     0,
     SPFLAG_TESTING,
     1,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},
