/**
 * @file
 * @brief Spell definitions and descriptions. See spell_desc struct in
 *        spl-util.cc.
 * Flag descriptions are in spl-cast.h.
**/

#pragma once

/*
struct spell_desc
{
    enum, spell name,
    spell schools,
    flags,
    level,
    power_cap,
    min_range, max_range, (-1 if not applicable)
    effect_noise
    tile
}
*/

#include "tag-version.h"

static const struct spell_desc spelldata[] =
{

{
    SPELL_CAUSE_FEAR, "Cause Fear",
    spschool::hexes,
    spflag::area | spflag::WL_check,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_CAUSE_FEAR,
},

{
    SPELL_MAGIC_DART, "Magic Dart",
    spschool::conjuration,
    spflag::dir_or_target | spflag::needs_tracer,
    1,
    25,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_MAGIC_DART,
},

{
    SPELL_FIREBALL, "Fireball",
    spschool::conjuration | spschool::fire,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    200,
    5, 5,
    0,
    TILEG_FIREBALL,
},

{
    SPELL_APPORTATION, "Apportation",
    spschool::translocation,
    spflag::target | spflag::obj | spflag::not_self,
    1,
    50,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_APPORTATION,
},

{
    SPELL_BLASTMOTE, "Volatile Blastmotes",
    spschool::fire | spschool::translocation,
    spflag::destructive,
    3,
    50,
    -1, -1,
    0,
    TILEG_BLASTMOTE,
},

{
    SPELL_DIG, "Dig",
    spschool::earth,
    spflag::dir_or_target | spflag::not_self | spflag::aim_at_space
        | spflag::utility,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4,
    TILEG_DIG,
},

{
    SPELL_BOLT_OF_FIRE, "Bolt of Fire",
    spschool::conjuration | spschool::fire,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    6,
    200,
    6, 6,
    0,
    TILEG_BOLT_OF_FIRE,
},

{
    SPELL_BOLT_OF_COLD, "Bolt of Cold",
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    6,
    200,
    5, 5,
    0,
    TILEG_BOLT_OF_COLD,
},

{
    SPELL_LIGHTNING_BOLT, "Lightning Bolt",
    spschool::conjuration | spschool::air,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    5,
    200,
    4, 11, // capped at LOS, yet this 11 matters since range increases linearly
    20,
    TILEG_LIGHTNING_BOLT,
},

{
    SPELL_ARCJOLT, "Arcjolt",
    spschool::conjuration | spschool::air,
    spflag::area,
    5,
    200,
    2, 2,
    10,
    TILEG_ARCJOLT,
},

{
    SPELL_PLASMA_BEAM, "Plasma Beam",
    spschool::fire | spschool::air,
    spflag::noisy | spflag::destructive,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    20,
    TILEG_PLASMA_BEAM,
},

{
    SPELL_PERMAFROST_ERUPTION, "Permafrost Eruption",
    spschool::ice | spschool::earth,
    spflag::destructive,
    6,
    200,
    6, 6, // reduce cases of hitting something outside LOS
    0,
    TILEG_PERMAFROST_ERUPTION,
},

{
    SPELL_BLINKBOLT, "Blinkbolt",
    spschool::air | spschool::translocation,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_BLINKBOLT,
},

{
    SPELL_ELECTRIC_CHARGE, "Vhi's Electric Charge",
    spschool::air | spschool::translocation,
    spflag::noisy | spflag::dir_or_target, // hack - should have spflag::needs_tracer
                   // and maybe spflag::hasty?
    4,
    50,
    4, 4,
    0,
    TILEG_ELECTRIC_CHARGE,
},

{
    SPELL_ELECTROLUNGE, "Vhi's Electrolunge",
    spschool::air | spschool::translocation,
    spflag::noisy | spflag::dir_or_target | spflag::monster,
    4,
    100,
    5, 5,
    0,
    TILEG_ELECTRIC_CHARGE,
},

{
    SPELL_BOLT_OF_MAGMA, "Bolt of Magma",
    spschool::conjuration | spschool::fire | spschool::earth,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    5,
    200,
    4, 4,
    0,
    TILEG_BOLT_OF_MAGMA,
},

{
    SPELL_POLYMORPH, "Polymorph",
    spschool::alchemy | spschool::hexes,
    spflag::dir_or_target | spflag::chaotic
        | spflag::needs_tracer | spflag::WL_check,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_POLYMORPH,
},

{
    SPELL_SLOW, "Slow",
    spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::WL_check,
    1,
    25,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SLOW,
},

{
    SPELL_HASTE, "Haste",
    spschool::hexes,
    spflag::helpful | spflag::hasty | spflag::selfench | spflag::utility
                    | spflag::monster,
    6,
    200,
    -1, -1,
    0,
    TILEG_HASTE,
},

{
    SPELL_PETRIFY, "Petrify",
    spschool::alchemy | spschool::earth,
    spflag::dir_or_target | spflag::needs_tracer | spflag::WL_check,
    4,
    200,
    6, 6,
    0,
    TILEG_PETRIFY,
},

{
    SPELL_CONFUSE, "Confuse",
    spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::WL_check
        | spflag::monster,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_CONFUSE,
},

{
    SPELL_INVISIBILITY, "Invisibility",
    spschool::hexes,
    spflag::helpful | spflag::selfench | spflag::escape | spflag::monster,
    6,
    200,
    -1, -1,
    0,
    TILEG_INVISIBILITY,
},

{
    SPELL_THROW_FLAME, "Throw Flame",
    spschool::conjuration | spschool::fire,
    spflag::dir_or_target | spflag::needs_tracer,
    2,
    50,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_THROW_FLAME,
},

{
    SPELL_THROW_FROST, "Throw Frost",
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    2,
    50,
    6, 6,
    0,
    TILEG_THROW_FROST,
},

{
    SPELL_DISJUNCTION, "Disjunction",
    spschool::translocation,
    spflag::escape | spflag::utility,
    8,
    200,
    4, 4,
    0,
    TILEG_DISJUNCTION,
},

{
    SPELL_FREEZING_CLOUD, "Freezing Cloud",
    spschool::conjuration | spschool::ice | spschool::air,
    spflag::target | spflag::area | spflag::needs_tracer
        | spflag::cloud,
    5,
    200,
    5, 5,
    2,
    TILEG_FREEZING_CLOUD,
},

{
    SPELL_MEPHITIC_CLOUD, "Mephitic Cloud",
    spschool::conjuration | spschool::alchemy | spschool::air,
    spflag::dir_or_target | spflag::area
        | spflag::needs_tracer | spflag::cloud,
    3,
    100,
    4, 4,
    0,
    TILEG_MEPHITIC_CLOUD,
},

{
    SPELL_VENOM_BOLT, "Venom Bolt",
    spschool::conjuration | spschool::alchemy,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    5,
    200,
    5, 5,
    0,
    TILEG_VENOM_BOLT,
},

{
    SPELL_OLGREBS_TOXIC_RADIANCE, "Olgreb's Toxic Radiance",
    spschool::alchemy,
    spflag::area | spflag::destructive,
    4,
    100,
    -1, -1,
    0,
    TILEG_OLGREBS_TOXIC_RADIANCE,
},

{
    SPELL_TELEPORT_OTHER, "Teleport Other",
    spschool::translocation,
    spflag::target | spflag::not_self | spflag::escape | spflag::WL_check,
    3,
    100,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_TELEPORT_OTHER,
},

{
    SPELL_DEATHS_DOOR, "Death's Door",
    spschool::necromancy,
    spflag::utility | spflag::no_ghost,
    9,
    200,
    -1, -1,
    0,
    TILEG_DEATHS_DOOR,
},

{
    SPELL_MASS_CONFUSION, "Mass Confusion",
    spschool::hexes,
    spflag::area | spflag::WL_check | spflag::monster,
    6,
    200,
    -1, -1,
    0,
    TILEG_MASS_CONFUSION,
},

{
    SPELL_SMITING, "Smiting",
    spschool::none,
    spflag::target | spflag::not_self,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SMITING,
},

{
    SPELL_SUMMON_SMALL_MAMMAL, "Summon Small Mammal",
    spschool::summoning,
    spflag::none,
    1,
    25,
    -1, -1,
    0,
    TILEG_SUMMON_SMALL_MAMMAL,
},

// Used indirectly, by monsters abjuring via other summon spells.
// And used directly by summoning miscast monsters (nameless horrors).
{
    SPELL_ABJURATION, "Abjuration",
    spschool::summoning,
    spflag::monster,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_MASS_ABJURATION,
},

{
    SPELL_BOLT_OF_DRAINING, "Bolt of Draining",
    spschool::conjuration | spschool::necromancy,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    5,
    200,
    5, 5,
    0,
    TILEG_BOLT_OF_DRAINING,
},

{
    SPELL_LEHUDIBS_CRYSTAL_SPEAR, "Lehudib's Crystal Spear",
    spschool::conjuration | spschool::earth,
    spflag::dir_or_target | spflag::needs_tracer,
    8,
    200,
    3, 3,
    0,
    TILEG_LEHUDIBS_CRYSTAL_SPEAR,
},

{
    SPELL_POLAR_VORTEX, "Polar Vortex",
    spschool::ice,
    spflag::area | spflag::destructive,
    9,
    200,
    POLAR_VORTEX_RADIUS, POLAR_VORTEX_RADIUS,
    0,
    TILEG_POLAR_VORTEX,
},

{
    SPELL_POISONOUS_CLOUD, "Poisonous Cloud",
    spschool::conjuration | spschool::alchemy | spschool::air,
    spflag::target | spflag::area | spflag::needs_tracer | spflag::cloud
                   | spflag::monster,
    5,
    200,
    5, 5,
    2,
    TILEG_POISONOUS_CLOUD,
},

{
    SPELL_FIRE_STORM, "Fire Storm",
    spschool::conjuration | spschool::fire,
    spflag::target | spflag::area | spflag::needs_tracer,
    9,
    200,
    5, 5,
    0,
    TILEG_FIRE_STORM,
},

{
    SPELL_CALL_DOWN_DAMNATION, "Call Down Damnation",
    spschool::conjuration,
    spflag::target | spflag::area | spflag::unholy | spflag::needs_tracer
                   | spflag::monster,
    9,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_CALL_DOWN_DAMNATION,
},

{
    SPELL_CALL_DOWN_LIGHTNING, "Call Down Lightning",
    spschool::conjuration | spschool::air,
    spflag::target | spflag::monster,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_CALL_DOWN_LIGHTNING,
},

{
    SPELL_BLINK, "Blink",
    spschool::translocation,
    spflag::escape | spflag::selfench | spflag::utility,
    2,
    50,
    -1, -1,
    0,
    TILEG_BLINK,
},

{
    SPELL_BLINK_RANGE, "Blink Range", // XXX needs better name
    spschool::translocation,
    spflag::escape | spflag::monster | spflag::selfench,
    2,
    0,
    -1, -1,
    0,
    TILEG_BLINK,
},

{
    SPELL_BLINK_AWAY, "Blink Away",
    spschool::translocation,
    spflag::escape | spflag::monster | spflag::selfench,
    2,
    0,
    -1, -1,
    0,
    TILEG_BLINK,
},

{
    SPELL_BLINK_CLOSE, "Blink Close",
    spschool::translocation,
    spflag::monster | spflag::target,
    2,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_BLINK,
},

// The following name was found in the hack.exe file of an early version
// of PCHACK - credit goes to its creator (whoever that may be):
{
    SPELL_ISKENDERUNS_MYSTIC_BLAST, "Iskenderun's Mystic Blast",
    spschool::conjuration | spschool::translocation,
    spflag::area,
    4,
    100,
    2, 2,
    10,
    TILEG_ISKENDERUNS_MYSTIC_BLAST,
},

{
    SPELL_SUMMON_HORRIBLE_THINGS, "Summon Horrible Things",
    spschool::summoning,
    spflag::unholy | spflag::chaotic | spflag::mons_abjure,
    8,
    200,
    -1, -1,
    0,
    TILEG_SUMMON_HORRIBLE_THINGS,
},

{
    SPELL_MALIGN_GATEWAY, "Malign Gateway",
    spschool::summoning | spschool::translocation,
    spflag::unholy | spflag::chaotic,
    7,
    200,
    -1, -1,
    15,
    TILEG_MALIGN_GATEWAY,
},

{
    SPELL_CHARMING, "Charm",
    spschool::hexes,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::WL_check,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_CHARMING,
},

{
    SPELL_ANIMATE_DEAD, "Animate Dead",
    spschool::necromancy,
    spflag::helpful | spflag::utility
        | spflag::no_ghost,
    4,
    100,
    -1, -1,
    0,
    TILEG_ANIMATE_DEAD,
},

{
    SPELL_PAIN, "Pain",
    spschool::necromancy,
    spflag::dir_or_target | spflag::needs_tracer | spflag::WL_check
        | spflag::monster,
    1,
    25,
    5, 5,
    0,
    TILEG_PAIN,
},

{
    SPELL_SOUL_SPLINTER, "Soul Splinter",
    spschool::necromancy,
    spflag::dir_or_target | spflag::needs_tracer | spflag::WL_check
        | spflag::not_self,
    1,
    25,
    5, 5,
    0,
    TILEG_NECROTISE,
},

{
    SPELL_VAMPIRIC_DRAINING, "Vampiric Draining",
    spschool::necromancy,
    spflag::dir_or_target | spflag::not_self,
    3,
    100,
    1, 1,
    0,
    TILEG_VAMPIRIC_DRAINING,
},

{
    SPELL_HAUNT, "Haunt",
    spschool::summoning | spschool::necromancy,
    spflag::target | spflag::not_self | spflag::mons_abjure,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_HAUNT,
},

{
    SPELL_MARTYRS_KNELL, "Martyr's Knell",
    spschool::summoning | spschool::necromancy,
    spflag::none,
    4,
    100,
    -1, -1,
    0,
    TILEG_MARTYRS_KNELL,
},

{
    SPELL_BORGNJORS_REVIVIFICATION, "Borgnjor's Revivification",
    spschool::necromancy,
    spflag::utility,
    8,
    200,
    -1, -1,
    0,
    TILEG_BORGNJORS_REVIVIFICATION,
},

{
    SPELL_FREEZE, "Freeze",
    spschool::ice,
    spflag::dir_or_target | spflag::not_self | spflag::destructive,
    1,
    25,
    1, 1,
    0,
    TILEG_FREEZE,
},

{
    SPELL_OZOCUBUS_REFRIGERATION, "Ozocubu's Refrigeration",
    spschool::ice,
    spflag::area | spflag::destructive,
    7,
    200,
    -1, -1,
    0,
    TILEG_OZOCUBUS_REFRIGERATION,
},

{
    SPELL_STICKY_FLAME, "Sticky Flame",
    spschool::alchemy | spschool::fire,
    spflag::dir_or_target | spflag::needs_tracer | spflag::destructive,
    4,
    100,
    1, 1,
    0,
    TILEG_STICKY_FLAME,
},

{
    SPELL_SUMMON_ICE_BEAST, "Summon Ice Beast",
    spschool::ice | spschool::summoning,
    spflag::none,
    3,
    100,
    -1, -1,
    0,
    TILEG_SUMMON_ICE_BEAST,
},

{
    SPELL_OZOCUBUS_ARMOUR, "Ozocubu's Armour",
    spschool::ice,
    spflag::no_ghost,
    3,
    100,
    -1, -1,
    0,
    TILEG_OZOCUBUS_ARMOUR,
},

{
    SPELL_CALL_IMP, "Call Imp",
    spschool::summoning,
    spflag::unholy,
    2,
    50,
    -1, -1,
    0,
    TILEG_CALL_IMP,
},

{
    SPELL_REPEL_MISSILES, "Repel Missiles",
    spschool::air,
    spflag::monster | spflag::selfench,
    2,
    50,
    -1, -1,
    0,
    TILEG_REPEL_MISSILES,
},

{
    SPELL_BERSERKER_RAGE, "Berserker Rage",
    spschool::earth,
    spflag::hasty | spflag::monster | spflag::selfench,
    3,
    0,
    -1, -1,
    0,
    TILEG_BERSERKER_RAGE,
},

{
    SPELL_DISPEL_UNDEAD, "Dispel Undead",
    spschool::necromancy,
    spflag::dir_or_target | spflag::needs_tracer,
    4,
    100,
    1, 1,
    0,
    TILEG_DISPEL_UNDEAD,
},

{
    SPELL_POISON_ARROW, "Poison Arrow",
    spschool::conjuration | spschool::alchemy,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    6,
    200,
    6, 6,
    0,
    TILEG_POISON_ARROW,
},

// Monster-only, players can use Lugonu's ability
{
    SPELL_BANISHMENT, "Banishment",
    spschool::translocation,
    spflag::dir_or_target | spflag::unholy | spflag::chaotic | spflag::monster
        | spflag::needs_tracer | spflag::WL_check,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_BANISHMENT,
},

{
    SPELL_STING, "Sting",
    spschool::conjuration | spschool::alchemy,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    1,
    25,
    4, 4,
    0,
    TILEG_STING,
},

{
    SPELL_SUBLIMATION_OF_BLOOD, "Sublimation of Blood",
    spschool::necromancy,
    spflag::utility,
    2,
    100,
    -1, -1,
    0,
    TILEG_SUBLIMATION_OF_BLOOD,
},

{
    SPELL_TUKIMAS_DANCE, "Tukima's Dance",
    spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::WL_check
        | spflag::not_self,
    3,
    100,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_TUKIMAS_DANCE,
},

{
    SPELL_SUMMON_DEMON, "Summon Demon",
    spschool::summoning,
    spflag::unholy
    | spflag::mons_abjure | spflag::monster,
    5,
    200,
    -1, -1,
    0,
    TILEG_SUMMON_DEMON,
},

{
    SPELL_SUMMON_GREATER_DEMON, "Summon Greater Demon",
    spschool::summoning,
    spflag::unholy
    | spflag::mons_abjure  | spflag::monster,
    7,
    200,
    -1, -1,
    0,
    TILEG_SUMMON_GREATER_DEMON,
},

{
    SPELL_PUTREFACTION, "Cigotuvi's Putrefaction",
    spschool::necromancy | spschool::air,
    spflag::target | spflag::unclean,
    4,
    100,
    5, 5,
    0,
    TILEG_CORPSE_ROT,
},

{
    SPELL_IRON_SHOT, "Iron Shot",
    spschool::conjuration | spschool::earth,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    6,
    200,
    4, 4,
    0,
    TILEG_IRON_SHOT,
},

{
    SPELL_BOMBARD, "Bombard",
    spschool::conjuration | spschool::earth,
    spflag::dir_or_target | spflag::needs_tracer,
    6,
    200,
    4, 4,
    0,
    TILEG_IRON_SHOT,
},

{
    SPELL_STONE_ARROW, "Stone Arrow",
    spschool::conjuration | spschool::earth,
    spflag::dir_or_target | spflag::needs_tracer,
    3,
    50,
    4, 4,
    0,
    TILEG_STONE_ARROW,
},

{
    SPELL_SHOCK, "Shock",
    spschool::conjuration | spschool::air,
    spflag::dir_or_target | spflag::needs_tracer,
    1,
    25,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SHOCK,
},

{
    SPELL_SWIFTNESS, "Swiftness",
    spschool::air,
    spflag::hasty | spflag::selfench | spflag::utility,
    3,
    100,
    -1, -1,
    0,
    TILEG_SWIFTNESS,
},

{
    SPELL_DEBUGGING_RAY, "Debugging Ray",
    spschool::conjuration,
    spflag::dir_or_target | spflag::testing,
    7,
    100,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_DEBUGGING_RAY,
},

{
    SPELL_AGONISING_TOUCH, "Agonising Touch",
    spschool::necromancy,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::WL_check | spflag::monster,
    5,
    200,
    1, 1,
    0,
    TILEG_AGONY,
},

{
    SPELL_CURSE_OF_AGONY, "Curse of Agony",
    spschool::necromancy,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::WL_check,
    5,
    100,
    3, 3,
    0,
    TILEG_AGONY,
},

{
    SPELL_MINDBURST, "Mindburst",
    spschool::conjuration,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::WL_check,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    6,
    TILEG_MINDBURST,
},

{
    SPELL_DEATH_CHANNEL, "Death Channel",
    spschool::necromancy,
    spflag::helpful | spflag::utility | spflag::selfench,
    6,
    200,
    -1, -1,
    0,
    TILEG_DEATH_CHANNEL,
},

// Monster-only, players can use Kiku's ability
{
    SPELL_SYMBOL_OF_TORMENT, "Symbol of Torment",
    spschool::necromancy,
    spflag::area | spflag::monster,
    6,
    0,
    -1, -1,
    0,
    TILEG_SYMBOL_OF_TORMENT,
},

{
    SPELL_SIPHON_ESSENCE, "Siphon Essence",
    spschool::necromancy,
    spflag::area | spflag::monster,
    7,
    0,
    2, 2,
    0,
    TILEG_SIPHON_ESSENCE,
},

{
    SPELL_THROW_ICICLE, "Throw Icicle",
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    4,
    100,
    5, 5,
    0,
    TILEG_THROW_ICICLE,
},

{
    SPELL_AIRSTRIKE, "Airstrike",
    spschool::air,
    spflag::target | spflag::not_self | spflag::destructive,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4,
    TILEG_AIRSTRIKE,
},

{
    SPELL_MOMENTUM_STRIKE, "Momentum Strike",
    spschool::conjuration | spschool::translocation,
    spflag::target | spflag::not_self,
    2,
    50,
    4, 4,
    0,
    TILEG_MOMENTUM_STRIKE,
},

{
    SPELL_SHADOW_CREATURES, "Shadow Creatures",
    spschool::summoning,
    spflag::mons_abjure | spflag::monster,
    6,
    0,
    -1, -1,
    0,
    TILEG_SUMMON_SHADOW_CREATURES,
},

{
    SPELL_CONFUSING_TOUCH, "Confusing Touch",
    spschool::hexes,
    spflag::selfench | spflag::WL_check, // Show success in the static targeter
    3,
    100,
    -1, -1,
    0,
    TILEG_CONFUSING_TOUCH,
},

{
    SPELL_PASSWALL, "Passwall",
    spschool::earth,
    spflag::target | spflag::escape | spflag::not_self | spflag::utility
        | spflag::silent,
    3,
    100,
    3, 3,
    0,
    TILEG_PASSWALL,
},

{
    SPELL_IGNITE_POISON, "Ignite Poison",
    spschool::fire | spschool::alchemy,
    spflag::area | spflag::destructive,
    4,
    100,
    -1, -1,
    0,
    TILEG_IGNITE_POISON,
},

{
    SPELL_CALL_CANINE_FAMILIAR, "Call Canine Familiar",
    spschool::summoning,
    spflag::none,
    3,
    100,
    -1, -1,
    0,
    TILEG_CALL_CANINE_FAMILIAR,
},

{
    SPELL_SUMMON_DRAGON, "Summon Dragon", // see also, SPELL_DRAGON_CALL
    spschool::summoning,
    spflag::mons_abjure | spflag::monster,
    9,
    200,
    -1, -1,
    0,
    TILEG_SUMMON_DRAGON,
},

{
    SPELL_HIBERNATION, "Ensorcelled Hibernation",
    spschool::hexes | spschool::ice,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::WL_check | spflag::silent,
    2,
    50,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_ENSORCELLED_HIBERNATION,
},

{
    SPELL_ENGLACIATION, "Metabolic Englaciation",
    spschool::hexes | spschool::ice,
    spflag::area,
    5,
    200,
    -1, -1,
    0,
    TILEG_METABOLIC_ENGLACIATION,
},

{
    SPELL_SILENCE, "Silence",
    spschool::hexes | spschool::air,
    spflag::area | spflag::silent, // of course!
    5,
    200,
    -1, -1,
    0,
    TILEG_SILENCE,
},

{
    SPELL_SHATTER, "Shatter",
    spschool::earth,
    spflag::area | spflag::destructive,
    9,
    200,
    -1, -1,
    30,
    TILEG_SHATTER,
},

{
    SPELL_DISPERSAL, "Dispersal",
    spschool::translocation,
    spflag::area | spflag::escape,
    6,
    200,
    1, 4,
    0,
    TILEG_DISPERSAL,
},

{
    SPELL_DISCHARGE, "Static Discharge",
    spschool::conjuration | spschool::air,
    spflag::area,
    2,
    50,
    1, 1,
    0,
    TILEG_STATIC_DISCHARGE,
},

{
    SPELL_CORONA, "Corona",
    spschool::hexes,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::WL_check | spflag::monster,
    1,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_CORONA,
},

{
    SPELL_INTOXICATE, "Alistair's Intoxication",
    spschool::alchemy,
    spflag::none,
    5,
    150,
    -1, -1,
    0,
    TILEG_ALISTAIRS_INTOXICATION,
},

{
    SPELL_LRD, "Lee's Rapid Deconstruction",
    spschool::earth,
    spflag::target | spflag::destructive,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_LEES_RAPID_DECONSTRUCTION,
},

{
    SPELL_SANDBLAST, "Sandblast",
    spschool::earth,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::destructive,
    1,
    50,
    4, 4,
    0,
    TILEG_SANDBLAST,
},

{
    SPELL_SIMULACRUM, "Sculpt Simulacrum",
    spschool::ice | spschool::alchemy,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::unholy,
    6,
    200,
    1, 1,
    0,
    TILEG_SIMULACRUM,
},

{
    SPELL_CONJURE_BALL_LIGHTNING, "Conjure Ball Lightning",
    spschool::air | spschool::conjuration,
    spflag::none,
    6,
    200,
    -1, -1,
    0,
    TILEG_CONJURE_BALL_LIGHTNING,
},

{
    SPELL_CHAIN_LIGHTNING, "Chain Lightning",
    spschool::air | spschool::conjuration,
    spflag::area,
    9,
    200,
    -1, -1,
    25,
    TILEG_CHAIN_LIGHTNING,
},

{
    SPELL_PORTAL_PROJECTILE, "Portal Projectile",
    spschool::translocation | spschool::hexes,
    spflag::monster,
    3,
    50,
    -1, -1,
    0,
    TILEG_PORTAL_PROJECTILE,
},

{
    SPELL_MONSTROUS_MENAGERIE, "Monstrous Menagerie",
    spschool::summoning,
    spflag::mons_abjure,
    7,
    200,
    -1, -1,
    0,
    TILEG_MONSTROUS_MENAGERIE,
},

{
    SPELL_GOLUBRIAS_PASSAGE, "Passage of Golubria",
    spschool::translocation,
    spflag::target | spflag::aim_at_space | spflag::escape | spflag::selfench,
    4,
    100,
    2, LOS_RADIUS,
    8, // when it closes
    TILEG_PASSAGE_OF_GOLUBRIA,
},

{
    SPELL_FULMINANT_PRISM, "Fulminant Prism",
    spschool::conjuration | spschool::alchemy,
    spflag::target | spflag::area | spflag::not_self | spflag::no_ghost,
    4,
    200,
    4, 4,
    0,
    TILEG_FULMINANT_PRISM,
},

{
    SPELL_PARALYSE, "Paralyse",
    spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer
        | spflag::WL_check,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_PARALYSE,
},

{
    SPELL_MINOR_HEALING, "Minor Healing",
    spschool::necromancy,
    spflag::recovery | spflag::helpful | spflag::monster | spflag::selfench
        | spflag::utility | spflag::not_evil,
    2,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_MINOR_HEALING,
},

{
    SPELL_MAJOR_HEALING, "Major Healing",
    spschool::necromancy,
    spflag::recovery | spflag::helpful | spflag::monster | spflag::selfench
        | spflag::utility | spflag::not_evil,
    6,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_MAJOR_HEALING,
},

{
    SPELL_WOODWEAL, "Woodweal",
    spschool::necromancy,
    spflag::recovery | spflag::helpful | spflag::monster | spflag::selfench
        | spflag::utility | spflag::not_evil,
    4,
    0,
    1, 1,
    0,
    TILEG_WOODWEAL,
},

{
    SPELL_HURL_DAMNATION, "Hurl Damnation",
    spschool::conjuration,
    spflag::dir_or_target | spflag::unholy
        | spflag::needs_tracer,
    // plus DS ability, staff of Dispater & Sceptre of Asmodeus
    9,
    200,
    6, 6,
    0,
    TILEG_HURL_DAMNATION,
},

{
    SPELL_BRAIN_BITE, "Brain Bite",
    spschool::necromancy | spschool::hexes,
    spflag::target | spflag::monster,
    3,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_BRAIN_BITE,
},

{
    SPELL_NOXIOUS_CLOUD, "Noxious Cloud",
    spschool::conjuration | spschool::alchemy | spschool::air,
    spflag::target | spflag::area | spflag::monster | spflag::needs_tracer
        | spflag::cloud,
    5,
    200,
    5, 5,
    2,
    TILEG_NOXIOUS_CLOUD,
},

{
    SPELL_STEAM_BALL, "Steam Ball",
    spschool::conjuration,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    4,
    0,
    6, 6,
    0,
    TILEG_STEAM_BALL,
},

{
    SPELL_SUMMON_UFETUBUS, "Summon Ufetubus",
    spschool::summoning,
    spflag::unholy | spflag::monster,
    4,
    0,
    -1, -1,
    0,
    TILEG_SUMMON_UFETUBUS,
},

{
    SPELL_SUMMON_SIN_BEAST, "Summon Sin Beast",
    spschool::summoning,
    spflag::unholy | spflag::monster,
    4,
    0,
    -1, -1,
    0,
    TILEG_SUMMON_SIN_BEAST,
},

{
    SPELL_BOLT_OF_DEVASTATION, "Bolt of Devastation",
    spschool::conjuration,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_ENERGY_BOLT,
},

{
    SPELL_SPIT_POISON, "Spit Poison",
    spschool::alchemy,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    2,
    0,
    6, 6,
    0,
    TILEG_SPIT_POISON,
},

{
    SPELL_SUMMON_UNDEAD, "Summon Undead",
    spschool::summoning | spschool::necromancy,
    spflag::monster | spflag::mons_abjure,
    7,
    0,
    -1, -1,
    0,
    TILEG_SUMMON_UNDEAD,
},

{
    SPELL_CANTRIP, "Cantrip",
    spschool::none,
    spflag::monster,
    1,
    0,
    -1, -1,
    0,
    TILEG_CANTRIP,
},

{
    SPELL_QUICKSILVER_BOLT, "Quicksilver Bolt",
    spschool::conjuration,
    spflag::dir_or_target | spflag::needs_tracer | spflag::not_self,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_QUICKSILVER_BOLT,
},

{
    SPELL_METAL_SPLINTERS, "Metal Splinters",
    spschool::conjuration,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    5,
    0,
    4, 4,
    0,
    TILEG_METAL_SPLINTERS,
},

{
    SPELL_SPLINTERSPRAY, "Splinterspray",
    spschool::conjuration,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    4,
    0,
    3, 3,
    0,
    TILEG_METAL_SPLINTERS, // close enough
},

{
    SPELL_MIASMA_BREATH, "Miasma Breath",
    spschool::conjuration,
    spflag::dir_or_target | spflag::unclean | spflag::monster
        | spflag::needs_tracer | spflag::cloud,
    6,
    0,
    5, 5,
    2,
    TILEG_MIASMA_BREATH,
},

{
    SPELL_SUMMON_DRAKES, "Summon Drakes",
    spschool::summoning | spschool::necromancy, // since it can summon shadow dragons
    spflag::unclean | spflag::monster | spflag::mons_abjure,
    6,
    0,
    -1, -1,
    0,
    TILEG_SUMMON_DRAKES,
},

{
    SPELL_BLINK_OTHER, "Blink Other",
    spschool::translocation,
    spflag::dir_or_target | spflag::not_self | spflag::escape | spflag::monster
        | spflag::needs_tracer,
    2,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_BLINK_OTHER,
},

{
    SPELL_BLINK_OTHER_CLOSE, "Blink Other Close",
    spschool::translocation,
    spflag::target | spflag::not_self | spflag::monster | spflag::needs_tracer,
    2,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_BLINK_OTHER,
},

{
    SPELL_SUMMON_MUSHROOMS, "Summon Mushrooms",
    spschool::summoning,
    spflag::monster | spflag::mons_abjure,
    4,
    0,
    -1, -1,
    0,
    TILEG_SUMMON_MUSHROOMS,
},

{
    SPELL_SPIT_ACID, "Spit Acid",
    spschool::alchemy,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SPIT_ACID,
},

{
    SPELL_CAUSTIC_BREATH, "Caustic Breath",
    spschool::conjuration | spschool::alchemy,
    spflag::dir_or_target | spflag::noisy | spflag::needs_tracer,
    5,
    0,
    6, 6,
    0,
    TILEG_SPIT_ACID,
},

{
    SPELL_PYRE_ARROW, "Pyre Arrow",
    spschool::conjuration | spschool::fire,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    4,
    100,
    5, 5,
    0,
    TILEG_STICKY_FLAME_RANGE,
},

{
    SPELL_FIRE_BREATH, "Fire Breath",
    spschool::conjuration | spschool::fire,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    5, 5,
    0,
    TILEG_FIRE_BREATH,
},

{
    SPELL_SEARING_BREATH, "Searing Breath",
    spschool::conjuration | spschool::fire,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    5, 5,
    0,
    TILEG_FIRE_BREATH,
},

{
    SPELL_CHAOS_BREATH, "Chaos Breath",
    spschool::conjuration | spschool::random,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer | spflag::cloud,
    5,
    0,
    5, 5,
    2,
    TILEG_CHAOS_BREATH,
},

{
    SPELL_COLD_BREATH, "Cold Breath",
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    5, 5,
    0,
    TILEG_COLD_BREATH,
},

{
    SPELL_GLACIAL_BREATH, "Glacial Breath",
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::noisy | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_COLD_BREATH,
},

{
    SPELL_WATER_ELEMENTALS, "Summon Water Elementals",
    spschool::summoning,
    spflag::monster | spflag::mons_abjure,
    5,
    0,
    -1, -1,
    0,
    TILEG_WATER_ELEMENTALS,
},

{
    SPELL_PORKALATOR, "Porkalator",
    spschool::hexes | spschool::alchemy,
    spflag::dir_or_target | spflag::chaotic | spflag::needs_tracer
        | spflag::WL_check | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_PORKALATOR,
},

{
    SPELL_CREATE_TENTACLES, "Spawn Tentacles",
    spschool::none,
    spflag::monster,
    5,
    0,
    -1, -1,
    0,
    TILEG_CREATE_TENTACLES,
},

{
    SPELL_SUMMON_EYEBALLS, "Summon Eyeballs",
    spschool::summoning,
    spflag::monster | spflag::mons_abjure,
    5,
    0,
    -1, -1,
    0,
    TILEG_SUMMON_EYEBALLS,
},

{
    SPELL_HASTE_OTHER, "Haste Other",
    spschool::hexes,
    spflag::dir_or_target | spflag::not_self | spflag::helpful
        | spflag::hasty | spflag::needs_tracer | spflag::utility
        | spflag::monster,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_HASTE_OTHER,
},

{
    SPELL_EARTH_ELEMENTALS, "Summon Earth Elementals",
    spschool::summoning,
    spflag::monster | spflag::mons_abjure,
    5,
    0,
    -1, -1,
    0,
    TILEG_EARTH_ELEMENTALS,
},

{
    SPELL_AIR_ELEMENTALS, "Summon Air Elementals",
    spschool::summoning,
    spflag::monster | spflag::mons_abjure,
    5,
    0,
    -1, -1,
    0,
    TILEG_AIR_ELEMENTALS,
},

{
    SPELL_FIRE_ELEMENTALS, "Summon Fire Elementals",
    spschool::summoning,
    spflag::monster | spflag::mons_abjure,
    5,
    0,
    -1, -1,
    0,
    TILEG_FIRE_ELEMENTALS,
},

{
    SPELL_SLEEP, "Sleep",
    spschool::hexes,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::WL_check | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SLEEP,
},

{
    SPELL_FAKE_MARA_SUMMON, "Mara Summon",
    spschool::summoning,
    spflag::monster,
    5,
    0,
    -1, -1,
    0,
    TILEG_FAKE_MARA_SUMMON,
},

{
    SPELL_SUMMON_ILLUSION, "Summon Illusion",
    spschool::summoning,
    spflag::monster | spflag::target,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SUMMON_ILLUSION,
},

{
    SPELL_PRIMAL_WAVE, "Primal Wave",
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    6,
    200,
    6, 6,
    25,
    TILEG_PRIMAL_WAVE,
},

{
    SPELL_CALL_TIDE, "Call Tide",
    spschool::translocation,
    spflag::monster,
    7,
    0,
    -1, -1,
    0,
    TILEG_CALL_TIDE,
},

{
    SPELL_IOOD, "Orb of Destruction",
    spschool::conjuration,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer,
    7,
    200,
    8, 8,
    0,
    TILEG_IOOD,
},

{
    SPELL_INK_CLOUD, "Ink Cloud",
    spschool::conjuration | spschool::ice, // it's a water spell
    spflag::monster | spflag::escape | spflag::utility,
    7,
    0,
    -1, -1,
    0,
    TILEG_INK_CLOUD,
},

{
    SPELL_MIGHT, "Might",
    spschool::hexes,
    spflag::helpful | spflag::selfench | spflag::utility
                    | spflag::monster,
    3,
    200,
    -1, -1,
    0,
    TILEG_MIGHT,
},

{
    SPELL_MIGHT_OTHER, "Might Other",
    spschool::hexes,
    spflag::dir_or_target | spflag::not_self | spflag::helpful
        | spflag::needs_tracer | spflag::utility | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_MIGHT,
},

{
    SPELL_AWAKEN_FOREST, "Awaken Forest",
    spschool::hexes | spschool::summoning,
    spflag::area | spflag::monster,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_AWAKEN_FOREST,
},

{
    SPELL_DRUIDS_CALL, "Druid's Call",
    spschool::summoning,
    spflag::monster,
    6,
    0,
    -1, -1,
    0,
    TILEG_DRUIDS_CALL,
},

{
    SPELL_BROTHERS_IN_ARMS, "Brothers in Arms",
    spschool::summoning,
    spflag::monster,
    6,
    0,
    -1, -1,
    0,
    TILEG_BROTHERS_IN_ARMS,
},

{
    SPELL_TROGS_HAND, "Trog's Hand",
    spschool::none,
    spflag::monster | spflag::selfench,
    3,
    0,
    -1, -1,
    0,
    TILEG_TROGS_HAND,
},

{
    SPELL_SUMMON_MORTAL_CHAMPION, "Summon Mortal Champion",
    spschool::summoning,
    spflag::monster,
    7,
    0,
    -1, -1,
    0,
    TILEG_SUMMON_MORTAL_CHAMPION,
},

{
    SPELL_VANQUISHED_VANGUARD, "Vanquished Vanguard",
    spschool::necromancy | spschool::summoning,
    spflag::monster | spflag::target,
    4,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SUMMON_SPECTRAL_ORCS,
},

{
    SPELL_SUMMON_HOLIES, "Summon Holies",
    spschool::summoning,
    spflag::monster | spflag::mons_abjure | spflag::holy,
    5,
    0,
    -1, -1,
    0,
    TILEG_SUMMON_HOLIES,
},

{
    SPELL_HEAL_OTHER, "Heal Other",
    spschool::necromancy,
    spflag::dir_or_target | spflag::not_self | spflag::helpful
        | spflag::needs_tracer | spflag::utility | spflag::not_evil
        | spflag::monster,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_HEAL_OTHER,
},

{
    SPELL_HOLY_FLAMES, "Holy Flames",
    spschool::none,
    spflag::target | spflag::not_self | spflag::holy | spflag::monster,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_HOLY_FLAMES,
},

{
    SPELL_HOLY_BREATH, "Holy Breath",
    spschool::conjuration,
    spflag::dir_or_target | spflag::area | spflag::needs_tracer | spflag::cloud
        | spflag::holy | spflag::monster,
    5,
    200,
    5, 5,
    2,
    TILEG_HOLY_BREATH,
},

{
    SPELL_INJURY_MIRROR, "Injury Mirror",
    spschool::none,
    spflag::dir_or_target | spflag::helpful | spflag::selfench
        | spflag::utility | spflag::monster,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_INJURY_MIRROR,
},

{
    SPELL_DRAIN_LIFE, "Drain Life",
    spschool::necromancy,
    spflag::area | spflag::monster,
    6,
    0,
    -1, -1,
    0,
    TILEG_DRAIN_LIFE,
},

{
    SPELL_LEDAS_LIQUEFACTION, "Leda's Liquefaction",
    spschool::earth | spschool::alchemy,
    spflag::area,
    4,
    200,
    -1, -1,
    0,
    TILEG_LEDAS_LIQUEFACTION,
},

{
    SPELL_SUMMON_HYDRA, "Summon Hydra",
    spschool::summoning,
    spflag::mons_abjure,
    7,
    200,
    -1, -1,
    0,
    TILEG_SUMMON_HYDRA,
},

{
    SPELL_MESMERISE, "Mesmerise",
    spschool::hexes,
    spflag::area | spflag::WL_check | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_MESMERISE,
},

{
    SPELL_FIRE_SUMMON, "Fire Summon",
    spschool::summoning | spschool::fire,
    spflag::monster | spflag::mons_abjure,
    8,
    0,
    -1, -1,
    0,
    TILEG_FIRE_SUMMON,
},

{
    SPELL_PETRIFYING_CLOUD, "Petrifying Cloud",
    spschool::conjuration | spschool::earth | spschool::air,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    2,
    TILEG_PETRIFYING_CLOUD,
},

{
    SPELL_INNER_FLAME, "Inner Flame",
    spschool::hexes | spschool::fire,
    spflag::target | spflag::not_self | spflag::WL_check | spflag::destructive,
    3,
    100,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_INNER_FLAME,
},

{
    SPELL_ENSNARE, "Ensnare",
    spschool::conjuration | spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    6,
    200,
    5, 5,
    0,
    TILEG_ENSNARE,
},

{
    SPELL_GREATER_ENSNARE, "Greater Ensnare",
    spschool::conjuration | spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    6,
    200,
    5, 5,
    0,
    TILEG_ENSNARE,
},

{
    SPELL_THUNDERBOLT, "Thunderbolt",
    spschool::conjuration | spschool::air,
    spflag::dir_or_target | spflag::not_self,
    2, // 2-5, sort of
    200,
    5, 5,
    15,
    TILEG_THUNDERBOLT,
},

{
    SPELL_BATTLESPHERE, "Iskenderun's Battlesphere",
    spschool::conjuration | spschool::forgecraft,
    spflag::utility,
    4,
    100,
    -1, -1,
    0,
    TILEG_BATTLESPHERE,
},

{
    SPELL_SUMMON_MINOR_DEMON, "Summon Minor Demon",
    spschool::summoning,
    spflag::unholy | spflag::monster,
    2,
    200,
    -1, -1,
    0,
    TILEG_SUMMON_MINOR_DEMON,
},

{
    SPELL_STICKS_TO_SNAKES, "Sticks to Snakes",
    spschool::summoning,
    spflag::monster,
    2,
    200,
    -1, -1,
    0,
    TILEG_STICKS_TO_SNAKES,
},

{
    SPELL_MALMUTATE, "Malmutate",
    spschool::alchemy | spschool::hexes,
    spflag::dir_or_target | spflag::not_self | spflag::chaotic
        | spflag::needs_tracer | spflag::monster,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_MALMUTATE,
},

{
    SPELL_DAZZLING_FLASH, "Dazzling Flash",
    spschool::hexes | spschool::fire,
    spflag::area,
    3,
    50,
    2, 3,
    0,
    TILEG_DAZZLING_FLASH,
},

{
    SPELL_FORCE_LANCE, "Force Lance",
    spschool::conjuration | spschool::translocation,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    4,
    100,
    3, 3,
    0,
    TILEG_FORCE_LANCE,
},

{
    SPELL_SENTINEL_MARK, "Sentinel's Mark",
    spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::WL_check
                          | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SENTINEL_MARK,
},

// Ironbrand Convoker version (delayed activation, recalls only humanoids)
{
    SPELL_WORD_OF_RECALL, "Word of Recall",
    spschool::summoning | spschool::translocation,
    spflag::utility | spflag::monster,
    3,
    0,
    -1, -1,
    0,
    TILEG_RECALL,
},

{
    SPELL_INJURY_BOND, "Injury Bond",
    spschool::hexes,
    spflag::area | spflag::helpful | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_INJURY_BOND,
},

{
    SPELL_SPECTRAL_CLOUD, "Spectral Cloud",
    spschool::conjuration | spschool::necromancy,
    spflag::dir_or_target | spflag::monster | spflag::cloud,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SPECTRAL_CLOUD,
},

{
    SPELL_GHOSTLY_FIREBALL, "Ghostly Fireball",
    spschool::conjuration | spschool::necromancy,
    spflag::dir_or_target | spflag::monster | spflag::unholy
        | spflag::needs_tracer,
    5,
    200,
    5, 5,
    0,
    TILEG_GHOSTLY_FIREBALL,
},

{
    SPELL_CALL_LOST_SOULS, "Call Lost Souls",
    spschool::summoning | spschool::necromancy,
    spflag::unholy | spflag::monster,
    5,
    200,
    -1, -1,
    0,
    TILEG_CALL_LOST_SOULS,
},

{
    SPELL_DIMENSION_ANCHOR, "Dimension Anchor",
    spschool::translocation | spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::WL_check
                          | spflag::monster,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_DIMENSIONAL_ANCHOR,
},

{
    SPELL_BLINK_ALLIES_ENCIRCLE, "Blink Allies Encircling",
    spschool::translocation,
    spflag::area | spflag::target | spflag::monster,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_BLINK_ALLIES_ENCIRCLING,
},

{
    SPELL_AWAKEN_VINES, "Awaken Vines",
    spschool::hexes | spschool::summoning,
    spflag::area | spflag::monster | spflag::target,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_AWAKEN_VINES,
},

{
    SPELL_THORN_VOLLEY, "Volley of Thorns",
    spschool::conjuration | spschool::earth,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    4,
    100,
    5, 5,
    0,
    TILEG_THORN_VOLLEY,
},

{
    SPELL_WALL_OF_BRAMBLES, "Wall of Brambles",
    spschool::conjuration | spschool::earth,
    spflag::area | spflag::monster,
    5,
    100,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_WALL_OF_BRAMBLES,
},

{
    SPELL_WATERSTRIKE, "Waterstrike",
    spschool::ice,
    spflag::target | spflag::not_self | spflag::monster,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_WATERSTRIKE,
},

{
    SPELL_WIND_BLAST, "Wind Blast",
    spschool::air,
    spflag::area | spflag::target | spflag::monster, // wind blast is targeted when used as a (monster) spell, but not from the storm card
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_WIND_BLAST,
},

{
    SPELL_STRIP_WILLPOWER, "Strip Willpower",
    spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::WL_check
                          | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_STRIP_WILLPOWER,
},

{
    SPELL_FUGUE_OF_THE_FALLEN, "Fugue of the Fallen",
    spschool::necromancy,
    spflag::selfench,
    3,
    100,
    -1, -1,
    8,
    TILEG_FUGUE_OF_THE_FALLEN,
},

{
    SPELL_SUMMON_VERMIN, "Summon Vermin",
    spschool::summoning,
    spflag::monster | spflag::unholy | spflag::mons_abjure,
    5,
    0,
    -1, -1,
    0,
    TILEG_SUMMON_VERMIN,
},

{
    SPELL_MALIGN_OFFERING, "Malign Offering",
    spschool::necromancy,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
                          | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_MALIGN_OFFERING,
},

{
    SPELL_SEARING_RAY, "Searing Ray",
    spschool::conjuration,
    spflag::dir_or_target | spflag::needs_tracer,
    2,
    50,
    4, 4,
    0,
    TILEG_SEARING_RAY,
},

{
    SPELL_DISCORD, "Discord",
    spschool::hexes,
    spflag::area | spflag::hasty | spflag::WL_check,
    8,
    200,
    -1, -1,
    0,
    TILEG_DISCORD,
},

{
    SPELL_INVISIBILITY_OTHER, "Invisibility Other",
    spschool::hexes,
    spflag::dir_or_target | spflag::not_self | spflag::helpful
                          | spflag::monster,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_INVISIBILITY,
},

{
    SPELL_VIRULENCE, "Virulence",
    spschool::alchemy | spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::WL_check
                          | spflag::monster,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_VIRULENCE,
},

{
    SPELL_ORB_OF_ELECTRICITY, "Orb of Electricity",
    spschool::conjuration | spschool::air,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_ORB_OF_ELECTRICITY,
},

{
    SPELL_FLASH_FREEZE, "Flash Freeze",
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_FLASH_FREEZE,
},

{
    SPELL_CREEPING_FROST, "Creeping Frost",
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_CREEPING_FROST,
},

{
    SPELL_LEGENDARY_DESTRUCTION, "Legendary Destruction",
    spschool::conjuration,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    8,
    200,
    5, 5,
    0,
    TILEG_LEGENDARY_DESTRUCTION,
},

{
    SPELL_FORCEFUL_INVITATION, "Forceful Invitation",
    spschool::summoning,
    spflag::monster,
    4,
    200,
    -1, -1,
    0,
    TILEG_FORCEFUL_INVITATION,
},

{
    SPELL_PLANEREND, "Plane Rend",
    spschool::summoning,
    spflag::monster,
    8,
    200,
    -1, -1,
    0,
    TILEG_PLANE_REND,
},

{
    SPELL_CHAIN_OF_CHAOS, "Chain of Chaos",
    spschool::conjuration,
    spflag::area | spflag::monster | spflag::chaotic,
    8,
    200,
    -1, -1,
    0,
    TILEG_CHAIN_OF_CHAOS,
},

{
    SPELL_CALL_OF_CHAOS, "Call of Chaos",
    spschool::hexes,
    spflag::area | spflag::chaotic | spflag::monster,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_CALL_OF_CHAOS,
},

{
    SPELL_SIGN_OF_RUIN, "Sign of Ruin",
    spschool::necromancy,
    spflag::monster,
    7,
    200,
    -1, -1,
    0,
    TILEG_ABILITY_KIKU_SIGN_OF_RUIN,
},

{
    SPELL_SAP_MAGIC, "Sap Magic",
    spschool::hexes,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SAP_MAGIC,
},

{
    SPELL_MAJOR_DESTRUCTION, "Major Destruction",
    spschool::conjuration,
    spflag::dir_or_target | spflag::chaotic | spflag::needs_tracer
                          | spflag::monster,
    7,
    200,
    6, 6,
    0,
    TILEG_MAJOR_DESTRUCTION,
},

{
    SPELL_BLINK_ALLIES_AWAY, "Blink Allies Away",
    spschool::translocation,
    spflag::area | spflag::target | spflag::monster,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_BLINK_ALLIES_AWAY,
},

{
    SPELL_SUMMON_FOREST, "Summon Forest",
    spschool::summoning | spschool::translocation,
    spflag::none,
    5,
    200,
    -1, -1,
    10,
    TILEG_SUMMON_FOREST,
},

{
    SPELL_FORGE_LIGHTNING_SPIRE, "Forge Lightning Spire",
    spschool::forgecraft | spschool::air,
    spflag::none,
    4,
    100,
    -1, -1,
    0,
    TILEG_FORGE_LIGHTNING_SPIRE,
},

{
    SPELL_FORGE_BLAZEHEART_GOLEM, "Forge Blazeheart Golem",
    spschool::forgecraft | spschool::fire,
    spflag::none,
    4,
    100,
    -1, -1,
    0,
    TILEG_FORGE_BLAZEHEART_GOLEM,
},

{
    SPELL_REBOUNDING_BLAZE, "Rebounding Blaze",
    spschool::conjuration | spschool::fire,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    7,
    200,
    6, 6,
    0,
    TILEG_REBOUNDING_BLAZE,
},

{
    SPELL_REBOUNDING_CHILL, "Rebounding Chill",
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    7,
    200,
    6, 6,
    0,
    TILEG_REBOUNDING_CHILL,
},

{
    SPELL_GLACIATE, "Glaciate",
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::area | spflag::not_self
        | spflag:: monster,
    9,
    200,
    6, 6,
    25,
    TILEG_ICE_STORM,
},

{
    SPELL_DRAGON_CALL, "Dragon's Call",
    spschool::summoning,
    spflag::none,
    9,
    200,
    -1, -1,
    15,
    TILEG_SUMMON_DRAGON,
},

{
    SPELL_SPELLSPARK_SERVITOR, "Spellspark Servitor",
    spschool::conjuration | spschool::forgecraft,
    spflag::none,
    7,
    200,
    -1, -1,
    0,
    TILEG_SPELLSPARK_SERVITOR,
},

{
    SPELL_SUMMON_MANA_VIPER, "Summon Mana Viper",
    spschool::summoning | spschool::hexes,
    spflag::mons_abjure,
    5,
    100,
    -1, -1,
    0,
    TILEG_SUMMON_MANA_VIPER,
},

{
    SPELL_PHANTOM_MIRROR, "Phantom Mirror",
    spschool::hexes,
    spflag::helpful,
    5,
    200,
    -1, -1,
    0,
    TILEG_PHANTOM_MIRROR,
},

{
    SPELL_DRAIN_MAGIC, "Drain Magic",
    spschool::hexes,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer
        | spflag::WL_check,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_DRAIN_MAGIC,
},

{
    SPELL_CORROSIVE_BOLT, "Corrosive Bolt",
    spschool::conjuration | spschool::alchemy,
    spflag::dir_or_target | spflag::needs_tracer,
    6,
    200,
    5, 5,
    0,
    TILEG_CORROSIVE_BOLT,
},

{
    SPELL_BOLT_OF_LIGHT, "Bolt of Light",
    spschool::conjuration | spschool::fire | spschool::air,
    spflag::dir_or_target | spflag::needs_tracer,
    6,
    200,
    5, 5,
    0,
    TILEG_BOLT_OF_LIGHT,
},

{
    SPELL_SERPENT_OF_HELL_GEH_BREATH, "gehenna serpent of hell breath",
    spschool::conjuration,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_FIRE_BREATH,
},

{
    SPELL_SERPENT_OF_HELL_COC_BREATH, "cocytus serpent of hell breath",
    spschool::conjuration,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_COLD_BREATH,
},

{
    SPELL_SERPENT_OF_HELL_DIS_BREATH, "dis serpent of hell breath",
    spschool::conjuration,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_LEHUDIBS_CRYSTAL_SPEAR,
},

{
    SPELL_SERPENT_OF_HELL_TAR_BREATH, "tartarus serpent of hell breath",
    spschool::conjuration,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_MIASMA_BREATH,
},

{
    SPELL_SUMMON_EMPEROR_SCORPIONS, "Summon Emperor Scorpions",
    spschool::summoning | spschool::alchemy,
    spflag::mons_abjure | spflag::monster,
    7,
    100,
    -1, -1,
    0,
    TILEG_SUMMON_EMPEROR_SCORPIONS,
},

{
    SPELL_IRRADIATE, "Irradiate",
    spschool::conjuration | spschool::alchemy,
    spflag::area | spflag::chaotic,
    5,
    200,
    1, 1,
    0,
    TILEG_IRRADIATE,
},

{
    SPELL_SPIT_LAVA, "Spit Lava",
    spschool::conjuration | spschool::fire | spschool::earth,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    5, 5,
    0,
    TILEG_SPIT_LAVA,
},

{
    SPELL_ELECTRICAL_BOLT, "Electrical Bolt",
    spschool::conjuration | spschool::air,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_LIGHTNING_BOLT,
},

{
    SPELL_FLAMING_CLOUD, "Flaming Cloud",
    spschool::conjuration | spschool::fire,
    spflag::target | spflag::area | spflag::monster | spflag::needs_tracer
        | spflag::cloud,
    5,
    0,
    5, 5,
    0,
    TILEG_FLAMING_CLOUD,
},

{
    SPELL_THROW_BARBS, "Throw Barbs",
    spschool::conjuration,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    5, 5,
    0,
    TILEG_THROW_BARBS,
},

{
    SPELL_BATTLECRY, "Battlecry",
    spschool::hexes,
    spflag::area | spflag::monster | spflag::selfench,
    6,
    0,
    -1, -1,
    0,
    TILEG_BATTLECRY,
},

{
    SPELL_WARNING_CRY, "Warning Cry",
    spschool::hexes,
    spflag::area | spflag::monster | spflag::selfench | spflag::noisy,
    6,
    0,
    -1, -1,
    25,
    TILEG_WARNING_CRY,
},

{
    SPELL_HUNTING_CALL, "Hunting Call",
    spschool::hexes,
    spflag::area | spflag::monster | spflag::selfench,
    6,
    0,
    -1, -1,
    0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_FUNERAL_DIRGE, "Funeral Dirge",
    spschool::necromancy,
    spflag::area | spflag::monster,
    4,
    200,
    -1, -1,
    0,
    TILEG_FUNERAL_DIRGE,
},

{
    SPELL_SEAL_DOORS, "Seal Doors",
    spschool::hexes,
    spflag::area | spflag::monster | spflag::selfench,
    6,
    0,
    -1, -1,
    0,
    TILEG_SEAL_DOORS,
},

{
    SPELL_FLAY, "Flay",
    spschool::necromancy,
    spflag::target | spflag::not_self | spflag::monster,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_FLAY,
},

{
    SPELL_BERSERK_OTHER, "Berserk Other",
    spschool::hexes,
    spflag::hasty | spflag::monster | spflag::not_self | spflag::helpful,
    3,
    0,
    3, 3,
    0,
    TILEG_BERSERK_OTHER,
},

{
    SPELL_CORRUPTING_PULSE, "Corrupting Pulse",
    spschool::hexes | spschool::alchemy,
    spflag::area | spflag::monster,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_CORRUPTING_PULSE,
},

{
    SPELL_SIREN_SONG, "Siren Song",
    spschool::hexes,
    spflag::area | spflag::WL_check | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SIREN_SONG,
},

{
    SPELL_AVATAR_SONG, "Avatar Song",
    spschool::hexes,
    spflag::area | spflag::WL_check | spflag::monster,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_AVATAR_SONG,
},

{
    SPELL_PARALYSIS_GAZE, "Paralysis Gaze",
    spschool::hexes,
    spflag::target | spflag::not_self | spflag::monster,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_PARALYSIS_GAZE,
},

{
    SPELL_CONFUSION_GAZE, "Confusion Gaze",
    spschool::hexes,
    spflag::target | spflag::not_self | spflag::monster | spflag::WL_check,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_CONFUSION_GAZE,
},

{
    SPELL_DRAINING_GAZE, "Draining Gaze",
    spschool::hexes,
    spflag::target | spflag::not_self | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_DRAINING_GAZE,
},

{
    SPELL_WEAKENING_GAZE, "Weakening Gaze",
    spschool::hexes,
    spflag::target | spflag::not_self | spflag::monster,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_WEAKENING_GAZE,
},

{
    SPELL_MOURNING_WAIL, "Mourning Wail",
    spschool::necromancy,
    spflag::dir_or_target | spflag::monster,
    3,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_MOURNING_WAIL,
},

{
    SPELL_DEATH_RATTLE, "Death Rattle",
    spschool::conjuration | spschool::necromancy | spschool::air,
    spflag::dir_or_target | spflag::monster,
    7,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_DEATH_RATTLE,
},

{
    SPELL_MARCH_OF_SORROWS, "March of Sorrows",
    spschool::conjuration | spschool::necromancy | spschool::air,
    spflag::dir_or_target | spflag::monster,
    7,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_MARCH_OF_SORROWS,
},

{
    SPELL_SUMMON_SCARABS, "Summon Scarabs",
    spschool::summoning | spschool::necromancy,
    spflag::mons_abjure | spflag::monster,
    7,
    100,
    -1, -1,
    0,
    TILEG_SUMMON_SCARABS,
},

{
    SPELL_THROW_ALLY, "Throw Ally",
    spschool::translocation,
    spflag::target | spflag::monster | spflag::not_self,
    2,
    50,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_THROW_ALLY,
},

{
    SPELL_CLEANSING_FLAME, "Cleansing Flame",
    spschool::none,
    spflag::area | spflag::monster | spflag::holy,
    8,
    200,
    -1, -1,
    0,
    TILEG_CLEANSING_FLAME,
},

// Evoker-only now
{
    SPELL_GRAVITAS, "Gell's Gravitas",
    spschool::translocation,
    spflag::target | spflag::needs_tracer | spflag::no_ghost,
    3,
    100,
    LOS_RADIUS, LOS_RADIUS,
    8,
    TILEG_GRAVITAS,
},

{
    SPELL_VIOLENT_UNRAVELLING, "Yara's Violent Unravelling",
    spschool::hexes | spschool::alchemy,
    spflag::target | spflag::no_ghost | spflag::chaotic | spflag::destructive,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_VIOLENT_UNRAVELLING,
},

{
    SPELL_ENTROPIC_WEAVE, "Entropic Weave",
    spschool::hexes,
    spflag::utility | spflag::target | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_ENTROPIC_WEAVE,
},

{
    SPELL_SUMMON_EXECUTIONERS, "Summon Executioners",
    spschool::summoning,
    spflag::unholy | spflag::mons_abjure | spflag::monster,
    9,
    200,
    -1, -1,
    0,
    TILEG_SUMMON_EXECUTIONERS,
},

{
    SPELL_DOOM_HOWL, "Doom Howl",
    spschool::translocation | spschool::hexes,
    spflag::dir_or_target | spflag::monster | spflag::WL_check,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    15,
    TILEG_DOOM_HOWL,
},

{
    SPELL_PRAYER_OF_BRILLIANCE, "Prayer of Brilliance",
    spschool::conjuration,
    spflag::area | spflag::monster,
    5,
    200,
    -1, -1,
    0,
    TILEG_PRAYER_OF_BRILLIANCE,
},

{
    SPELL_ICEBLAST, "Iceblast",
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    200,
    5, 5,
    0,
    TILEG_ICEBLAST,
},

{
    SPELL_SLUG_DART, "Slug Dart",
    spschool::conjuration,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    1,
    25,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SLUG_DART,
},

{
    SPELL_SPRINT, "Sprint",
    spschool::hexes,
    spflag::hasty | spflag::selfench | spflag::utility | spflag::monster,
    2,
    100,
    -1, -1,
    0,
    TILEG_SPRINT,
},

{
    SPELL_GREATER_SERVANT_MAKHLEB, "Infernal Servant",
    spschool::summoning,
    spflag::unholy | spflag::mons_abjure | spflag::monster,
    7,
    200,
    -1, -1,
    0,
    TILEG_ABILITY_MAKHLEB_GREATER_SERVANT,
},

{
    SPELL_BIND_SOULS, "Bind Souls",
    spschool::necromancy | spschool::ice,
    spflag::area | spflag::monster,
    6,
    200,
    -1, -1,
    0,
    TILEG_DEATH_CHANNEL,
},

{
    SPELL_INFESTATION, "Infestation",
    spschool::necromancy,
    spflag::target | spflag::unclean,
    8,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4,
    TILEG_INFESTATION,
},

{
    SPELL_STILL_WINDS, "Still Winds",
    spschool::hexes | spschool::air,
    spflag::monster | spflag::selfench,
    6,
    200,
    -1, -1,
    0,
    TILEG_STILL_WINDS,
},

{
    SPELL_RESONANCE_STRIKE, "Resonance Strike",
    spschool::earth,
    spflag::target | spflag::not_self | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_RESONANCE_STRIKE,
},

{
    SPELL_GHOSTLY_SACRIFICE, "Ghostly Sacrifice",
    spschool::necromancy,
    spflag::target | spflag::monster,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_GHOSTLY_FIREBALL,
},

{
    SPELL_DREAM_DUST, "Dream Dust",
    spschool::hexes,
    spflag::target | spflag::not_self | spflag::monster,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_DREAM_DUST,
},

{
    SPELL_BECKONING, "Lesser Beckoning",
    spschool::translocation,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer,
    2,
    50,
    3, 5,
    0,
    TILEG_BECKONING,
},

// Monster-only, players can use Qazlal's ability
{
    SPELL_UPHEAVAL, "Upheaval",
    spschool::conjuration,
    spflag::target | spflag::not_self | spflag::needs_tracer | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_ABILITY_QAZLAL_UPHEAVAL,
},

{
    SPELL_MERCURY_ARROW, "Mercury Arrow",
    spschool::alchemy | spschool::conjuration,
    spflag::dir_or_target | spflag::needs_tracer,
    2,
    50,
    4, 4,
    0,
    TILEG_STING,
},

{
    SPELL_POISONOUS_VAPOURS, "Poisonous Vapours",
    spschool::alchemy | spschool::air,
    spflag::target | spflag::destructive | spflag::not_self,
    1,
    25,
    3, 3,
    0,
    TILEG_POISONOUS_CLOUD,
},

{
    SPELL_IGNITION, "Ignition",
    spschool::fire,
    spflag::area | spflag::destructive,
    8,
    200,
    -1, -1,
    0,
    TILEG_IGNITION,
},

{
    SPELL_BORGNJORS_VILE_CLUTCH, "Borgnjor's Vile Clutch",
    spschool::necromancy | spschool::earth,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer,
    5,
    200,
    6, 6,
    5,
    TILEG_BORGNJORS_VILE_CLUTCH,
},

{
    SPELL_FASTROOT, "Fastroot",
    spschool::hexes | spschool::earth,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    200,
    5, 5,
    0,
    TILEG_GRASPING_ROOTS,
},

{
    SPELL_WARP_SPACE, "Warp Space",
    spschool::translocation,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    200,
    5, 5,
    0,
    TILEG_WARP_SPACE,
},

{
    SPELL_SOJOURNING_BOLT, "Sojourning Bolt",
    spschool::conjuration | spschool::translocation,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    6,
    200,
    5, 5,
    0,
    TILEG_SOJOURNING_BOLT,
},

{
    SPELL_HARPOON_SHOT, "Harpoon Shot",
    spschool::conjuration | spschool::earth,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    4,
    200,
    6, 6,
    0,
    TILEG_HARPOON_SHOT,
},

{
    SPELL_GRASPING_ROOTS, "Grasping Roots",
    spschool::earth,
    spflag::target | spflag::not_self | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_GRASPING_ROOTS,
},

{
    SPELL_THROW_BOLAS, "Throw Bolas",
    spschool::conjuration,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    4,
    200,
    6, 6,
    0,
    TILEG_THROW_BOLAS,
},


{
    SPELL_THROW_PIE, "Throw Klown Pie",
    spschool::conjuration | spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_THROW_KLOWN_PIE,
},

{
    SPELL_SPORULATE, "Sporulate",
    spschool::conjuration | spschool::earth,
    spflag::monster,
    5,
    200,
    -1, -1,
    0,
    TILEG_SPORULATE,
},

{
    SPELL_STARBURST, "Starburst",
    spschool::conjuration | spschool::fire,
    spflag::area,
    6,
    200,
    5, 5,
    0,
    TILEG_STARBURST,
},

{
    SPELL_FOXFIRE, "Foxfire",
    spschool::conjuration | spschool::fire,
    spflag::none,
    1,
    25,
    -1, -1,
    0,
    TILEG_FOXFIRE,
},

{
    SPELL_MARSHLIGHT, "Marshlight",
    spschool::conjuration | spschool::fire,
    spflag::monster,
    4,
    200,
    -1, -1,
    0,
    TILEG_FOXFIRE,
},

{
    SPELL_HAILSTORM, "Hailstorm",
    spschool::conjuration | spschool::ice,
    spflag::area,
    3,
    100,
    3, 3, // Range special-cased in describe-spells
    0,
    TILEG_HAILSTORM,
},

{
    SPELL_NOXIOUS_BOG, "Eringya's Noxious Bog",
    spschool::alchemy,
    spflag::area | spflag::no_ghost | spflag::destructive,
    6,
    200,
    4, 4,
    0,
    TILEG_NOXIOUS_BOG,
},

{
    SPELL_AGONY, "Agony",
    spschool::necromancy,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::monster | spflag::WL_check,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_AGONY,
},

{
    SPELL_DISPEL_UNDEAD_RANGE, "Dispel Undead Range",
    spschool::necromancy,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    5,
    100,
    4, 4,
    0,
    TILEG_DISPEL_UNDEAD,
},

{
    SPELL_FROZEN_RAMPARTS, "Frozen Ramparts",
    spschool::ice,
    spflag::area | spflag::no_ghost | spflag::destructive,
    3,
    50,
    2, 2,
    8,
    TILEG_FROZEN_RAMPARTS,
},

{
    SPELL_MAXWELLS_COUPLING, "Maxwell's Capacitive Coupling",
    spschool::air,
    spflag::no_ghost | spflag::destructive,
    8,
    200,
    LOS_RADIUS, LOS_RADIUS,
    25,
    TILEG_MAXWELLS_COUPLING,
},

{
    // This "spell" is implemented in a way that ignores all this information,
    // and it is never triggered the way spells usually are, but it still has
    // a spell-type enum entry. So, use fake data in order to have a valid
    // entry here. If it ever were to be castable, this would need some updates.
    SPELL_SONIC_WAVE, "Sonic wave",
    spschool::none,
    spflag::noisy,
    7, 0, -1, -1, 0, TILEG_ERROR
},

{
    SPELL_ROLL, "Roll",
    spschool::earth,
    spflag::monster,
    5,
    0,
    -1, -1,
    0,
    TILEG_ROLL
},

{
    SPELL_HURL_SLUDGE, "Hurl Sludge",
    spschool::alchemy | spschool::conjuration,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    6,
    200,
    5, 5,
    0,
    TILEG_HURL_SLUDGE
},

{
    SPELL_SUMMON_TZITZIMITL, "Summon Tzitzimitl",
    spschool::summoning | spschool::necromancy,
    spflag::monster | spflag::mons_abjure,
    8,
    0,
    -1, -1,
    0,
    TILEG_SUMMON_TZITZIMITL,
},

{
    SPELL_SUMMON_HELL_SENTINEL, "Summon Hell Sentinel",
    spschool::summoning,
    spflag::monster | spflag::mons_abjure,
    8,
    0,
    -1, -1,
    0,
    TILEG_SUMMON_HELL_SENTINEL,
},

{
    SPELL_AWAKEN_ARMOUR, "Awaken Armour",
    spschool::forgecraft | spschool::earth,
    spflag::none,
    4,
    50,
    -1, -1,
    0,
    TILEG_AWAKEN_ARMOUR,
},

{
    SPELL_MANIFOLD_ASSAULT, "Manifold Assault",
    spschool::translocation,
    spflag::none,
    7,
    200,
    -1, -1,
    0,
    TILEG_MANIFOLD_ASSAULT,
},

{
    SPELL_CONCENTRATE_VENOM, "Concentrate Venom",
    spschool::alchemy,
    spflag::dir_or_target | spflag::not_self | spflag::helpful
        | spflag::needs_tracer | spflag::utility | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_CONCENTRATE_VENOM,
},

{
    SPELL_ERUPTION, "Eruption",
    spschool::conjuration | spschool::fire | spschool::earth,
    spflag::target | spflag::not_self | spflag::needs_tracer | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_ERUPTION,
},

{
    SPELL_PYROCLASTIC_SURGE, "Pyroclastic Surge",
    spschool::conjuration | spschool::fire | spschool::earth,
    spflag::dir_or_target | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_PYROCLASTIC_SURGE,
},

{
    SPELL_STUNNING_BURST, "Stunning Burst",
    spschool::conjuration | spschool::air,
    spflag::target | spflag::needs_tracer | spflag::monster,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_STUNNING_BURST,
},

{
    SPELL_CORRUPT_LOCALE, "Corrupt",
    spschool::translocation,
    spflag::monster | spflag::area,
    7,
    0,
    -1, -1,
    0,
    TILEG_CORRUPT,
},

{
    SPELL_CONJURE_LIVING_SPELLS, "Conjure Living Spells",
    spschool::conjuration,
    spflag::monster,
    6,
    200,
    -1, -1,
    0,
    TILEG_CONJURE_LIVING_SPELLS,
},

{
    SPELL_SUMMON_CACTUS, "Summon Cactus Giant",
    spschool::summoning,
    spflag::none,
    6,
    200,
    -1, -1,
    0,
    TILEG_SUMMON_CACTUS_GIANT,
},

{
    SPELL_STOKE_FLAMES, "Stoke Flames",
    spschool::fire | spschool::conjuration,
    spflag::monster,
    8,
    0,
    -1, -1,
    0,
    TILEG_STOKE_FLAMES,
},

{
    SPELL_SERACFALL, "Seracfall",
    spschool::conjuration | spschool::ice,
    spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_ICEBLAST,
},

{
    SPELL_SCORCH, "Scorch",
    spschool::fire,
    spflag::destructive,
    2,
    50,
    3, 3,
    8,
    TILEG_SCORCH,
},

{
    SPELL_FLAME_WAVE, "Flame Wave",
    spschool::conjuration | spschool::fire,
    spflag::area,
    4,
    100,
    3, 3, // sort of...
    12, // increases as it's channeled
    TILEG_FLAME_WAVE,
},

{
    SPELL_ENFEEBLE, "Enfeeble",
    spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::WL_check,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_ENFEEBLE,
},

{
    SPELL_SUMMON_SPIDERS, "Summon Spiders",
    spschool::summoning | spschool::alchemy,
    spflag::monster,
    6,
    200,
    -1, -1,
    0,
    TILEG_SUMMON_SPIDERS,
},

{
    SPELL_ANGUISH, "Anguish",
    spschool::hexes | spschool::necromancy,
    spflag::area | spflag::WL_check,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_ANGUISH,
},

{
    SPELL_SUMMON_SCORPIONS, "Summon Scorpions",
    spschool::summoning | spschool::alchemy,
    spflag::mons_abjure | spflag::monster,
    4,
    200,
    -1, -1,
    0,
    TILEG_SUMMON_SCORPIONS,
},

{
    SPELL_SHEZAS_DANCE, "Sheza's Dance",
    spschool::summoning | spschool::earth,
    spflag::mons_abjure | spflag::monster,
    5,
    200,
    -1, -1,
    0,
    TILEG_SHEZAS_DANCE,
},

{
    SPELL_DIVINE_ARMAMENT, "Divine Armament",
    spschool::summoning,
    spflag::monster,
    4,
    200,
    -1, -1,
    0,
    TILEG_DIVINE_ARMAMENT,
},

{
    SPELL_KISS_OF_DEATH, "Kiss of Death",
    spschool::conjuration | spschool::necromancy,
    spflag::dir_or_target | spflag::needs_tracer | spflag::not_self,
    1,
    25,
    1, 1,
    0,
    TILEG_KISS_OF_DEATH,
},

{
    SPELL_JINXBITE, "Jinxbite",
    spschool::hexes,
    spflag::selfench,
    2,
    50,
    -1, -1,
    0,
    TILEG_JINXBITE,
},

{
    SPELL_SIGIL_OF_BINDING, "Sigil of Binding",
    spschool::hexes,
    spflag::none,
    3,
    100,
    -1, -1,
    0,
    TILEG_SIGIL_OF_BINDING,
},

{
    SPELL_DIMENSIONAL_BULLSEYE, "Dimensional Bullseye",
    spschool::translocation | spschool::hexes,
    spflag::target | spflag::not_self | spflag::prefer_farthest,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_PORTAL_PROJECTILE,
},

{
    SPELL_BOULDER, "Brom's Barrelling Boulder",
    spschool::earth | spschool::conjuration,
    spflag::target | spflag::not_self,
    4,
    100,
    1, 1,
    0,
    TILEG_BOULDER,
},

{
    SPELL_VITRIFY, "Vitrify",
    spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::WL_check
                          | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_VITRIFY,
},

{
    SPELL_VITRIFYING_GAZE, "Vitrifying Gaze",
    spschool::hexes,
    spflag::target | spflag::not_self | spflag::monster,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_VITRIFYING_GAZE,
},

{
    SPELL_CRYSTALLIZING_SHOT, "Crystallizing Shot",
    spschool::conjuration | spschool::earth | spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    6,
    50,
    4, 4,
    0,
    TILEG_CRYSTALLIZING_SHOT,
},

{
    SPELL_TREMORSTONE, "Tremorstone",
    spschool::earth,
    spflag::area,
    2,
    200,
    -1, -1,
    15,
    TILEG_LEES_RAPID_DECONSTRUCTION, // close enough
},

{
    SPELL_REGENERATE_OTHER, "Regenerate Other",
    spschool::necromancy,
    spflag::monster | spflag::not_self | spflag::helpful,
    4,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_REGENERATION,
},

{
    SPELL_MASS_REGENERATION, "Mass Regeneration",
    spschool::necromancy,
    spflag::monster  | spflag::helpful,
    7,
    0,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_REGENERATION,
},

{
    SPELL_NOXIOUS_BREATH, "Noxious Breath",
    spschool::conjuration | spschool::air | spschool:: alchemy,
    spflag::dir_or_target | spflag::noisy | spflag::needs_tracer,
    5,
    0,
    6, 6,
    0,
    TILEG_NOXIOUS_CLOUD,
},

// Dummy spell for the Makhleb ability.
{
    SPELL_UNLEASH_DESTRUCTION, "Unleash Destruction",
    spschool::conjuration,
    spflag::dir_or_target | spflag::chaotic | spflag::needs_tracer,
    3,
    0,
    LOS_RADIUS, LOS_RADIUS,
    6,
    TILEG_ERROR,
},

{
    SPELL_HURL_TORCHLIGHT, "Hurl Torchlight",
    spschool::conjuration | spschool::necromancy,
    spflag::dir_or_target | spflag::needs_tracer,
    4,
    200,
    5, 5,
    0,
    TILEG_ERROR,
},

{
    SPELL_COMBUSTION_BREATH, "Combustion Breath",
    spschool::conjuration | spschool::fire,
    spflag::dir_or_target | spflag::noisy | spflag::needs_tracer,
    5,
    0,
    5, 5,
    0,
    TILEG_FIRE_BREATH,
},

{
    SPELL_NULLIFYING_BREATH, "Nullifying Breath",
    spschool::conjuration,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5,
    TILEG_QUICKSILVER_BOLT,
},

{
    SPELL_STEAM_BREATH, "Steam Breath",
    spschool::conjuration,
    spflag::dir_or_target | spflag::needs_tracer,
    4,
    0,
    6, 6,
    0,
    TILEG_STEAM_BALL,
},

{
    SPELL_MUD_BREATH, "Mud Breath",
    spschool::conjuration | spschool::earth,
    spflag::dir_or_target | spflag::noisy | spflag::needs_tracer,
    5,
    0,
    6, 6,
    10,
    TILEG_LEDAS_LIQUEFACTION,
},

{
    SPELL_GALVANIC_BREATH, "Galvanic Breath",
    spschool::conjuration | spschool::air,
    spflag::dir_or_target | spflag::noisy | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    10,
    TILEG_ARCJOLT,
},

{
    SPELL_PILEDRIVER, "Maxwell's Portable Piledriver",
    spschool::translocation,
    spflag::target,
    3,
    100,
    5, 5,
    0,
    TILEG_PILEDRIVER,
},

{
    SPELL_GELLS_GAVOTTE, "Gell's Gavotte",
    spschool::translocation,
    spflag::target | spflag::aim_at_space,
    6,
    200,
    1, 1,
    0,
    TILEG_GAVOTTE,
},

{
    SPELL_MAGNAVOLT, "Magnavolt",
    spschool::air | spschool::earth,
    spflag::target | spflag::needs_tracer | spflag::destructive
    | spflag::prefer_farthest,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_MAGNAVOLT,
},

{
    SPELL_FULSOME_FUSILLADE, "Fulsome Fusillade",
    spschool::alchemy | spschool::conjuration,
    spflag::area | spflag::destructive | spflag::chaotic,
    8,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_FULSOME_FUSILLADE,
},

{
    SPELL_RIMEBLIGHT, "Rimeblight",
    spschool::necromancy | spschool::ice,
    spflag::dir_or_target | spflag::unclean | spflag::destructive
    | spflag::not_self,
    7,
    200,
    5, 5,
    0,
    TILEG_RIMEBLIGHT,
},

{
    SPELL_HOARFROST_CANNONADE, "Hoarfrost Cannonade",
    spschool::forgecraft | spschool::ice,
    spflag::none,
    5,
    200,
    -1, -1,
    0,
    TILEG_HOARFROST_CANNONADE,
},

{
    SPELL_SEISMIC_STOMP, "Seismic Stomp",
    spschool::earth,
    spflag::monster,
    5,
    200,
    4, 4,
    8,
    TILEG_SEISMIC_STOMP,
},

{
    SPELL_HOARFROST_BULLET, "Hoarfrost Bullet",
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    5,
    200,
    6, 6,
    0,
    TILEG_HOARFROST_BULLET,
},

{
    SPELL_FLASHING_BALESTRA, "Flashing Balestra",
    spschool::conjuration,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    5,
    200,
    6, 6,
    0,
    TILEG_TUKIMAS_DANCE,
},

{
    SPELL_PHANTOM_BLITZ, "Phantom Blitz",
    spschool::conjuration | spschool::summoning,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    7,
    200,
    7, 7,
    0,
    TILEG_PHANTOM_BLITZ,
},

{
    SPELL_BESTOW_ARMS, "Bestow Arms",
    spschool::hexes,
    spflag::area | spflag::utility | spflag::monster,
    5,
    200,
    6, 6,
    0,
    TILEG_BESTOW_ARMS,
},

{
    SPELL_HELLFIRE_MORTAR, "Hellfire Mortar",
    spschool::earth | spschool::fire | spschool::forgecraft,
    spflag::dir_or_target | spflag::destructive,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    20,
    TILEG_HELLFIRE_MORTAR,
},

// Dithmenos shadow mimic spells
{
    SPELL_SHADOW_SHARD, "Shadow Shard",
    spschool::earth,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer
    | spflag::silent,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SHADOW_SHARD,
},

{
    SPELL_SHADOW_BEAM, "Shadow Beam",
    spschool::conjuration,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer
    | spflag::silent,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SHADOW_BEAM,
},

{
    SPELL_SHADOW_BALL, "Shadowball",
    spschool::fire,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer
    | spflag::silent,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SHADOWBALL,
},

{
    SPELL_CREEPING_SHADOW, "Creeping Shadow",
    spschool::ice,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer
    | spflag::silent,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_CREEPING_SHADOW,
},

{
    SPELL_SHADOW_TEMPEST, "Shadow Tempest",
    spschool::air,
    spflag::area | spflag::monster | spflag::needs_tracer | spflag::silent,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SHADOW_TEMPEST,
},

{
    SPELL_SHADOW_PRISM, "Shadow Prism",
    spschool::alchemy,
    spflag::target | spflag::area | spflag::not_self | spflag::monster
    | spflag::needs_tracer | spflag::silent,
    5,
    200,
    4, 4,
    0,
    TILEG_SHADOW_PRISM,
},


{
    SPELL_SHADOW_PUPPET, "Shadow Puppet",
    spschool::summoning,
    spflag::monster | spflag::silent,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SHADOW_PUPPET,
},

{
    SPELL_SHADOW_TURRET, "Shadow Turret",
    spschool::forgecraft,
    spflag::monster | spflag::silent,
    5,
    200,
    -1, -1,
    0,
    TILEG_SHADOW_TURRET,
},

{
    SPELL_SHADOW_SHOT, "Shadow Shot",
    spschool::forgecraft,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer
    | spflag::silent,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SHADOW_SHARD,
},

{
    SPELL_SHADOW_BIND, "Shadow Bind",
    spschool::translocation,
    spflag::target | spflag::monster | spflag::silent,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SHADOW_BIND,
},

{
    SPELL_SHADOW_TORPOR, "Shadow Torpor",
    spschool::hexes,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer
    | spflag::silent,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SHADOW_TORPOR,
},

{
    SPELL_SHADOW_DRAINING, "Shadow Draining",
    spschool::necromancy,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer
    | spflag::silent,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_SHADOW_DRAINING,
},

{
    SPELL_GRAVE_CLAW, "Grave Claw",
    spschool::necromancy,
    spflag::target | spflag::not_self,
    2,
    50,
    4, 4,
    0,
    TILEG_GRAVE_CLAW,
},

{
    SPELL_CLOCKWORK_BEE, "Launch Clockwork Bee",
    spschool::forgecraft,
    spflag::target | spflag::not_self,
    3,
    100,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_CLOCKWORK_BEE,
},

{
    SPELL_SPIKE_LAUNCHER, "Construct Spike Launcher",
    spschool::forgecraft,
    spflag::none,
    2,
    50,
    -1, -1,
    0,
    TILEG_SPIKE_LAUNCHER,
},

{
    SPELL_KINETIC_GRAPNEL, "Kinetic Grapnel",
    spschool::forgecraft,
    spflag::dir_or_target | spflag::needs_tracer | spflag::destructive,
    1,
    25,
    4, 4,
    0,
    TILEG_KINETIC_GRAPNEL,
},

{
    SPELL_DIAMOND_SAWBLADES, "Diamond Sawblades",
    spschool::forgecraft,
    spflag::none,
    7,
    200,
    -1, -1,
    0,
    TILEG_DIAMOND_SAWBLADES,
},

{
    SPELL_SHRED, "Shred",
    spschool::forgecraft,
    // XXX: This isn't really a 'utility' spell, but this is the easiest way to
    //      avoid sawblades refusing to use it if they somehow end up targeting
    //      the player.
    spflag::monster | spflag::utility,
    1,
    200,
    1, 1,
    0,
    TILEG_DIAMOND_SAWBLADES,
},

{
    SPELL_SURPRISING_CROCODILE, "Eringya's Surprising Crocodile",
    spschool::summoning,
    spflag::target | spflag::not_self,
    4,
    100,
    1, 1,
    0,
    TILEG_SURPRISING_CROCODILE,
},

{
    SPELL_PLATINUM_PARAGON, "Platinum Paragon",
    spschool::forgecraft,
    spflag::target | spflag::not_self,
    9,
    200,
    3, 3,
    10,
    TILEG_PLATINUM_PARAGON,
},

{
    SPELL_WALKING_ALEMBIC, "Alistair's Walking Alembic",
    spschool::forgecraft | spschool::alchemy,
    spflag::none,
    5,
    100,
    -1, -1,
    0,
    TILEG_WALKING_ALEMBIC,
},

{
    SPELL_MONARCH_BOMB, "Forge Monarch Bomb",
    spschool::forgecraft | spschool::fire,
    spflag::none,
    6,
    200,
    -1, -1,
    0,
    TILEG_MONARCH_BOMB,
},

{
    SPELL_DEPLOY_BOMBLET, "Launch Bomblet",
    spschool::forgecraft | spschool::fire,
    spflag::target | spflag::monster,
    6,
    200,
    4, 4,
    0,
    TILEG_LAUNCH_BOMBLET,
},

{
    SPELL_SPLINTERFROST_SHELL, "Splinterfrost Shell",
    spschool::forgecraft | spschool::ice,
    spflag::target | spflag::not_self,
    7,
    200,
    1, 1,
    0,
    TILEG_SPLINTERFROST_SHELL,
},

{
    SPELL_PERCUSSIVE_TEMPERING, "Nazja's Percussive Tempering",
    spschool::forgecraft,
    spflag::target | spflag::helpful | spflag::not_self | spflag::destructive,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_PERCUSSIVE_TEMPERING,
},

{
    SPELL_FORTRESS_BLAST, "Fortress Blast",
    spschool::forgecraft,
    spflag::area | spflag::destructive,
    6,
    75,
    2, 2,
    20,
    TILEG_FORTRESS_BLAST,
},

{
    SPELL_SUMMON_SEISMOSAURUS_EGG, "Summon Seismosaurus Egg",
    spschool::summoning | spschool::earth,
    spflag::none,
    4,
    100,
    -1, -1,
    0,
    TILEG_SUMMON_SEISMOSAURUS_EGG,
},

{
    SPELL_PHALANX_BEETLE, "Forge Phalanx Beetle",
    spschool::forgecraft,
    spflag::none,
    6,
    200,
    -1, -1,
    0,
    TILEG_PHALANX_BEETLE,
},

{
    SPELL_RENDING_BLADE, "Rending Blade",
    spschool::conjuration | spschool::forgecraft,
    spflag::utility,
    4,
    100,
    -1, -1,
    0,
    TILEG_RENDING_BLADE,
},

{
    SPELL_MAGMA_BARRAGE, "Magma Barrage",
    spschool::conjuration | spschool::fire | spschool::earth,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    5,
    200,
    5, 5,
    0,
    TILEG_MAGMA_BARRAGE,
},

{
    SPELL_VEX, "Vex",
    spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer
        | spflag::WL_check | spflag::monster,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0,
    TILEG_VEX,
},

{
    SPELL_NO_SPELL, "nonexistent spell",
    spschool::none,
    spflag::testing,
    1,
    0,
    -1, -1,
    0,
    TILEG_ERROR,
},

#if TAG_MAJOR_VERSION == 34
#define AXED_SPELL(tag, name) \
    { tag, name, spschool::none, spflag::none, 7, 0, -1, -1, 0, TILEG_ERROR },

AXED_SPELL(SPELL_AURA_OF_ABJURATION, "Aura of Abjuration")
AXED_SPELL(SPELL_BOLT_OF_INACCURACY, "Bolt of Inaccuracy")
AXED_SPELL(SPELL_CHANT_FIRE_STORM, "Chant Fire Storm")
AXED_SPELL(SPELL_CIGOTUVIS_DEGENERATION, "Cigotuvi's Degeneration")
AXED_SPELL(SPELL_CIGOTUVIS_EMBRACE, "Cigotuvi's Embrace")
AXED_SPELL(SPELL_CONDENSATION_SHIELD, "Condensation Shield")
AXED_SPELL(SPELL_CONTROLLED_BLINK, "Controlled Blink")
AXED_SPELL(SPELL_CONTROL_TELEPORT, "Control Teleport")
AXED_SPELL(SPELL_CONTROL_UNDEAD, "Control Undead")
AXED_SPELL(SPELL_CONTROL_WINDS, "Control Winds")
AXED_SPELL(SPELL_CORRUPT_BODY, "Corrupt Body")
AXED_SPELL(SPELL_CURE_POISON, "Cure Poison")
AXED_SPELL(SPELL_DARKNESS, "Darkness")
AXED_SPELL(SPELL_DEFLECT_MISSILES, "Deflect Missiles")
AXED_SPELL(SPELL_DELAYED_FIREBALL, "Delayed Fireball")
AXED_SPELL(SPELL_DEMONIC_HORDE, "Demonic Horde")
AXED_SPELL(SPELL_DRACONIAN_BREATH, "Draconian Breath")
AXED_SPELL(SPELL_EPHEMERAL_INFUSION, "Ephemeral Infusion")
AXED_SPELL(SPELL_EVAPORATE, "Evaporate")
AXED_SPELL(SPELL_EXPLOSIVE_BOLT, "Explosive Bolt")
AXED_SPELL(SPELL_FAKE_RAKSHASA_SUMMON, "Rakshasa Summon")
AXED_SPELL(SPELL_FIRE_BRAND, "Fire Brand")
AXED_SPELL(SPELL_FLY, "Flight")
AXED_SPELL(SPELL_FORCEFUL_DISMISSAL, "Forceful Dismissal")
AXED_SPELL(SPELL_FREEZING_AURA, "Freezing Aura")
AXED_SPELL(SPELL_FRENZY, "Frenzy")
AXED_SPELL(SPELL_FULSOME_DISTILLATION, "Fulsome Distillation")
AXED_SPELL(SPELL_GRAND_AVATAR, "Grand Avatar")
AXED_SPELL(SPELL_HASTE_PLANTS, "Haste Plants")
AXED_SPELL(SPELL_HOLY_LIGHT, "Holy Light")
AXED_SPELL(SPELL_HUNTING_CRY, "Hunting Cry")
AXED_SPELL(SPELL_IGNITE_POISON_SINGLE, "Localized Ignite Poison")
AXED_SPELL(SPELL_INSULATION, "Insulation")
AXED_SPELL(SPELL_INFUSION, "Infusion")
AXED_SPELL(SPELL_IRON_ELEMENTALS, "Summon Iron Elementals")
AXED_SPELL(SPELL_LETHAL_INFUSION, "Lethal Infusion")
AXED_SPELL(SPELL_MELEE, "Melee")
AXED_SPELL(SPELL_MISLEAD, "Mislead")
AXED_SPELL(SPELL_PHASE_SHIFT, "Phase Shift")
AXED_SPELL(SPELL_POISON_WEAPON, "Poison Weapon")
AXED_SPELL(SPELL_RANDOM_BOLT, "Random Bolt")
AXED_SPELL(SPELL_REARRANGE_PIECES, "Rearrange the Pieces")
AXED_SPELL(SPELL_RECALL, "Recall")
AXED_SPELL(SPELL_REGENERATION, "Regeneration")
AXED_SPELL(SPELL_RING_OF_FLAMES, "Ring of Flames")
AXED_SPELL(SPELL_SEE_INVISIBLE, "See Invisible")
AXED_SPELL(SPELL_SHAFT_SELF, "Shaft Self")
AXED_SPELL(SPELL_SHROUD_OF_GOLUBRIA, "Shroud of Golubria")
AXED_SPELL(SPELL_SILVER_BLAST, "Silver Blast")
AXED_SPELL(SPELL_SINGULARITY, "Singularity")
AXED_SPELL(SPELL_SONG_OF_SHIELDING, "Song of Shielding")
AXED_SPELL(SPELL_SPECTRAL_WEAPON, "Spectral Weapon")
AXED_SPELL(SPELL_STONESKIN, "Stoneskin")
AXED_SPELL(SPELL_SUMMON_BUTTERFLIES, "Summon Butterflies")
AXED_SPELL(SPELL_SUMMON_ELEMENTAL, "Summon Elemental")
AXED_SPELL(SPELL_SUMMON_RAKSHASA, "Summon Rakshasa")
AXED_SPELL(SPELL_SUMMON_TWISTER, "Summon Twister")
AXED_SPELL(SPELL_SUNRAY, "Sunray")
AXED_SPELL(SPELL_SURE_BLADE, "Sure Blade")
AXED_SPELL(SPELL_THROW, "Throw")
AXED_SPELL(SPELL_VAMPIRE_SUMMON, "Vampire Summon")
AXED_SPELL(SPELL_WARP_BRAND, "Warp Weapon")
AXED_SPELL(SPELL_WEAVE_SHADOWS, "Weave Shadows")
AXED_SPELL(SPELL_STRIKING, "Striking")
AXED_SPELL(SPELL_RESURRECT, "Resurrect")
AXED_SPELL(SPELL_HOLY_WORD, "Holy word")
AXED_SPELL(SPELL_SACRIFICE, "Sacrifice")
AXED_SPELL(SPELL_MIASMA_CLOUD, "Miasma cloud")
AXED_SPELL(SPELL_POISON_CLOUD, "Poison cloud")
AXED_SPELL(SPELL_FIRE_CLOUD, "Fire cloud")
AXED_SPELL(SPELL_STEAM_CLOUD, "Steam cloud")
AXED_SPELL(SPELL_HOMUNCULUS, "Homunculus")
AXED_SPELL(SPELL_SERPENT_OF_HELL_BREATH_REMOVED, "Old serpent of hell breath")
AXED_SPELL(SPELL_SCATTERSHOT, "Scattershot")
AXED_SPELL(SPELL_SUMMON_SWARM, "Summon swarm")
AXED_SPELL(SPELL_CLOUD_CONE, "Cloud Cone")
AXED_SPELL(SPELL_RING_OF_THUNDER, "Ring of Thunder")
AXED_SPELL(SPELL_TWISTED_RESURRECTION, "Twisted Resurrection")
AXED_SPELL(SPELL_RANDOM_EFFECTS, "Random Effects")
AXED_SPELL(SPELL_HYDRA_FORM, "Hydra Form")
AXED_SPELL(SPELL_VORTEX, "Vortex")
AXED_SPELL(SPELL_GOAD_BEASTS, "Goad Beasts")
AXED_SPELL(SPELL_TELEPORT_SELF, "Teleport Self")
AXED_SPELL(SPELL_TOMB_OF_DOROKLOHE, "Tomb of Doroklohe")
AXED_SPELL(SPELL_EXCRUCIATING_WOUNDS, "Excruciating Wounds")
AXED_SPELL(SPELL_CONJURE_FLAME, "Conjure Flame")
AXED_SPELL(SPELL_CORPSE_ROT, "Corpse Rot")
AXED_SPELL(SPELL_FLAME_TONGUE, "Flame Tongue")
AXED_SPELL(SPELL_BEASTLY_APPENDAGE, "Beastly Appendage")
AXED_SPELL(SPELL_SPIDER_FORM, "Spider Form")
AXED_SPELL(SPELL_ICE_FORM, "Ice Form")
AXED_SPELL(SPELL_BLADE_HANDS, "Blade Hands")
AXED_SPELL(SPELL_STATUE_FORM, "Statue Form")
AXED_SPELL(SPELL_STORM_FORM, "Storm Form")
AXED_SPELL(SPELL_DRAGON_FORM, "Dragon Form")
AXED_SPELL(SPELL_NECROMUTATION, "Necromutation")
AXED_SPELL(SPELL_AWAKEN_EARTH, "Awaken Earth")
AXED_SPELL(SPELL_ANIMATE_SKELETON, "Animate Skeleton")
#endif

};
