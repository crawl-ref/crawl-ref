#pragma once

#include "tag-version.h"

enum spell_type : int
{
    SPELL_NO_SPELL,
#if TAG_MAJOR_VERSION == 34
    SPELL_TELEPORT_SELF,
    SPELL_FIRST_SPELL = SPELL_TELEPORT_SELF,
#endif
    SPELL_CAUSE_FEAR,
#if TAG_MAJOR_VERSION > 34
    SPELL_FIRST_SPELL = SPELL_CAUSE_FEAR,
#endif
    SPELL_MAGIC_DART,
    SPELL_FIREBALL,
    SPELL_APPORTATION,
#if TAG_MAJOR_VERSION == 34
    SPELL_DELAYED_FIREBALL,
    SPELL_STRIKING,
    SPELL_CONJURE_FLAME,
#endif
    SPELL_DIG,
    SPELL_BOLT_OF_FIRE,
    SPELL_BOLT_OF_COLD,
    SPELL_LIGHTNING_BOLT,
    SPELL_BOLT_OF_MAGMA,
    SPELL_POLYMORPH,
    SPELL_SLOW,
    SPELL_HASTE,
    SPELL_PARALYSE,
    SPELL_CONFUSE,
    SPELL_INVISIBILITY,
    SPELL_THROW_FLAME,
    SPELL_THROW_FROST,
#if TAG_MAJOR_VERSION == 34
    SPELL_CONTROLLED_BLINK,
#endif
    SPELL_FREEZING_CLOUD,
    SPELL_MEPHITIC_CLOUD,
#if TAG_MAJOR_VERSION == 34
    SPELL_RING_OF_FLAMES,
#endif
    SPELL_VENOM_BOLT,
    SPELL_OLGREBS_TOXIC_RADIANCE,
    SPELL_TELEPORT_OTHER,
    SPELL_MINOR_HEALING,
    SPELL_MAJOR_HEALING,
    SPELL_DEATHS_DOOR,
    SPELL_MASS_CONFUSION,
    SPELL_SMITING,
    SPELL_SUMMON_SMALL_MAMMAL,
    SPELL_ABJURATION,
    SPELL_SUMMON_SCORPIONS,
    SPELL_BOLT_OF_DRAINING,
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
#if TAG_MAJOR_VERSION == 34
    SPELL_BOLT_OF_INACCURACY,
#endif
    SPELL_POISONOUS_CLOUD,
    SPELL_FIRE_STORM,
    SPELL_BLINK,
    SPELL_ISKENDERUNS_MYSTIC_BLAST,
#if TAG_MAJOR_VERSION == 34
    SPELL_SUMMON_SWARM,
#endif
    SPELL_SUMMON_HORRIBLE_THINGS,
    SPELL_CHARMING,
    SPELL_ANIMATE_DEAD,
    SPELL_PAIN,
#if TAG_MAJOR_VERSION == 34
    SPELL_CONTROL_UNDEAD,
#endif
    SPELL_ANIMATE_SKELETON,
    SPELL_VAMPIRIC_DRAINING,
    SPELL_HAUNT,
    SPELL_BORGNJORS_REVIVIFICATION,
    SPELL_FREEZE,
#if TAG_MAJOR_VERSION == 34
    SPELL_SUMMON_ELEMENTAL,
#endif
    SPELL_OZOCUBUS_REFRIGERATION,
    SPELL_STICKY_FLAME,
    SPELL_SUMMON_ICE_BEAST,
    SPELL_OZOCUBUS_ARMOUR,
    SPELL_CALL_IMP,
    SPELL_REPEL_MISSILES,
    SPELL_BERSERKER_RAGE,
    SPELL_DISPEL_UNDEAD,
#if TAG_MAJOR_VERSION == 34
    SPELL_FULSOME_DISTILLATION,
#endif
    SPELL_POISON_ARROW,
#if TAG_MAJOR_VERSION == 34
    SPELL_TWISTED_RESURRECTION,
    SPELL_REGENERATION,
#endif
    SPELL_BANISHMENT,
#if TAG_MAJOR_VERSION == 34
    SPELL_CIGOTUVIS_DEGENERATION,
#endif
    SPELL_STING,
    SPELL_SUBLIMATION_OF_BLOOD,
    SPELL_TUKIMAS_DANCE,
    SPELL_HURL_DAMNATION,
    SPELL_SUMMON_DEMON,
#if TAG_MAJOR_VERSION == 34
    SPELL_DEMONIC_HORDE,
#endif
    SPELL_SUMMON_GREATER_DEMON,
#if TAG_MAJOR_VERSION == 34
    SPELL_CORPSE_ROT,
    SPELL_FIRE_BRAND,
    SPELL_FREEZING_AURA,
    SPELL_LETHAL_INFUSION,
#endif
    SPELL_IRON_SHOT,
    SPELL_STONE_ARROW,
    SPELL_SHOCK,
    SPELL_SWIFTNESS,
#if TAG_MAJOR_VERSION == 34
    SPELL_FLY,
    SPELL_INSULATION,
    SPELL_CURE_POISON,
    SPELL_CONTROL_TELEPORT,
    SPELL_POISON_WEAPON,
#endif
    SPELL_DEBUGGING_RAY,
#if TAG_MAJOR_VERSION == 34
    SPELL_RECALL,
#endif
    SPELL_AGONY,
    SPELL_SPIDER_FORM,
    SPELL_MINDBURST,
    SPELL_BLADE_HANDS,
    SPELL_STATUE_FORM,
    SPELL_ICE_FORM,
    SPELL_DRAGON_FORM,
#if TAG_MAJOR_VERSION > 34
    SPELL_IRRADIATE,
#endif
    SPELL_NECROMUTATION,
    SPELL_DEATH_CHANNEL,
    SPELL_SYMBOL_OF_TORMENT,
#if TAG_MAJOR_VERSION == 34
    SPELL_DEFLECT_MISSILES,
#endif
    SPELL_THROW_ICICLE,
    SPELL_GLACIATE,
    SPELL_AIRSTRIKE,
    SPELL_SHADOW_CREATURES,
    SPELL_CONFUSING_TOUCH,
#if TAG_MAJOR_VERSION == 34
    SPELL_SURE_BLADE,
    SPELL_FLAME_TONGUE,
#endif
    SPELL_PASSWALL,
    SPELL_IGNITE_POISON,
#if TAG_MAJOR_VERSION == 34
    SPELL_STICKS_TO_SNAKES,
#endif
    SPELL_CALL_CANINE_FAMILIAR,
    SPELL_SUMMON_DRAGON,
    SPELL_HIBERNATION,
    SPELL_ENGLACIATION,
#if TAG_MAJOR_VERSION == 34
    SPELL_SEE_INVISIBLE,
    SPELL_PHASE_SHIFT,
    SPELL_SUMMON_BUTTERFLIES,
    SPELL_WARP_BRAND,
#endif
    SPELL_SILENCE,
    SPELL_SHATTER,
    SPELL_DISPERSAL,
    SPELL_DISCHARGE,
    SPELL_CORONA,
    SPELL_INTOXICATE,
#if TAG_MAJOR_VERSION == 34
    SPELL_EVAPORATE,
#endif
    SPELL_LRD,
    SPELL_SANDBLAST,
#if TAG_MAJOR_VERSION == 34
    SPELL_CONDENSATION_SHIELD,
    SPELL_STONESKIN,
#endif
    SPELL_SIMULACRUM,
    SPELL_CONJURE_BALL_LIGHTNING,
    SPELL_CHAIN_LIGHTNING,
#if TAG_MAJOR_VERSION == 34
    SPELL_EXCRUCIATING_WOUNDS,
#endif
    SPELL_PORTAL_PROJECTILE,
    SPELL_MONSTROUS_MENAGERIE,
    SPELL_PETRIFY,
    SPELL_GOLUBRIAS_PASSAGE,

    // Mostly monster-only spells after this point:
    SPELL_CALL_DOWN_DAMNATION,
#if TAG_MAJOR_VERSION == 34
    SPELL_VAMPIRE_SUMMON,
#endif
    SPELL_BRAIN_FEED,
#if TAG_MAJOR_VERSION == 34
    SPELL_FAKE_RAKSHASA_SUMMON,
#endif
    SPELL_STEAM_BALL,
    SPELL_SUMMON_UFETUBUS,
    SPELL_SUMMON_HELL_BEAST,
    SPELL_ENERGY_BOLT,
    SPELL_SPIT_POISON,
    SPELL_SUMMON_UNDEAD,
    SPELL_CANTRIP,
    SPELL_QUICKSILVER_BOLT,
    SPELL_METAL_SPLINTERS,
    SPELL_MIASMA_BREATH,
    SPELL_SUMMON_DRAKES,
    SPELL_BLINK_OTHER,
    SPELL_SUMMON_MUSHROOMS,
    SPELL_SPIT_ACID,
    SPELL_ACID_SPLASH,
    SPELL_FIRE_BREATH,
    SPELL_COLD_BREATH,
#if TAG_MAJOR_VERSION == 34
    SPELL_DRACONIAN_BREATH,
#endif
    SPELL_WATER_ELEMENTALS,
    SPELL_PORKALATOR,
    SPELL_CREATE_TENTACLES,
#if TAG_MAJOR_VERSION == 34
    SPELL_TOMB_OF_DOROKLOHE,
#endif
    SPELL_SUMMON_EYEBALLS,
    SPELL_HASTE_OTHER,
    SPELL_FIRE_ELEMENTALS,
    SPELL_EARTH_ELEMENTALS,
    SPELL_AIR_ELEMENTALS,
    SPELL_SLEEP,
    SPELL_BLINK_OTHER_CLOSE,
    SPELL_BLINK_CLOSE,
    SPELL_BLINK_RANGE,
    SPELL_BLINK_AWAY,
#if TAG_MAJOR_VERSION == 34
    SPELL_MISLEAD,
#endif
    SPELL_FAKE_MARA_SUMMON,
#if TAG_MAJOR_VERSION == 34
    SPELL_SUMMON_RAKSHASA,
#endif
    SPELL_SUMMON_ILLUSION,
    SPELL_PRIMAL_WAVE,
    SPELL_CALL_TIDE,
    SPELL_IOOD,
    SPELL_INK_CLOUD,
    SPELL_MIGHT,
#if TAG_MAJOR_VERSION == 34
    SPELL_SUNRAY,
#endif
    SPELL_AWAKEN_FOREST,
    SPELL_DRUIDS_CALL,
#if TAG_MAJOR_VERSION == 34
    SPELL_IRON_ELEMENTALS,
#endif
    SPELL_SUMMON_SPECTRAL_ORCS,
#if TAG_MAJOR_VERSION == 34
    SPELL_RESURRECT,
    SPELL_HOLY_LIGHT,
    SPELL_HOLY_WORD,
#endif
    SPELL_SUMMON_HOLIES,
    SPELL_HEAL_OTHER,
#if TAG_MAJOR_VERSION == 34
    SPELL_SACRIFICE,
#endif
    SPELL_HOLY_FLAMES,
    SPELL_HOLY_BREATH,
    SPELL_TROGS_HAND,
    SPELL_BROTHERS_IN_ARMS,
    SPELL_INJURY_MIRROR,
    SPELL_DRAIN_LIFE,
#if TAG_MAJOR_VERSION == 34
    SPELL_MIASMA_CLOUD,
    SPELL_POISON_CLOUD,
    SPELL_FIRE_CLOUD,
    SPELL_STEAM_CLOUD,
#endif
    SPELL_MALIGN_GATEWAY,
    SPELL_NOXIOUS_CLOUD,
    SPELL_POLAR_VORTEX,
    SPELL_STICKY_FLAME_RANGE,
    SPELL_LEDAS_LIQUEFACTION,
#if TAG_MAJOR_VERSION == 34
    SPELL_HOMUNCULUS,
#endif
    SPELL_SUMMON_HYDRA,
    SPELL_DARKNESS,
    SPELL_MESMERISE,
#if TAG_MAJOR_VERSION == 34
    SPELL_MELEE, // like SPELL_NO_SPELL, but doesn't cause a re-roll
#endif
    SPELL_FIRE_SUMMON,
#if TAG_MAJOR_VERSION == 34
    SPELL_SHROUD_OF_GOLUBRIA,
#endif
    SPELL_INNER_FLAME,
    SPELL_PETRIFYING_CLOUD,
#if TAG_MAJOR_VERSION == 34
    SPELL_AURA_OF_ABJURATION,
#endif
    SPELL_BEASTLY_APPENDAGE,
#if TAG_MAJOR_VERSION == 34
    SPELL_SILVER_BLAST,
#endif
    SPELL_ENSNARE,
    SPELL_THUNDERBOLT,
    SPELL_SUMMON_MINOR_DEMON,
    SPELL_DISJUNCTION,
    SPELL_CHAOS_BREATH,
#if TAG_MAJOR_VERSION == 34
    SPELL_FRENZY,
    SPELL_SUMMON_TWISTER,
#endif
    SPELL_BATTLESPHERE,
    SPELL_FULMINANT_PRISM,
    SPELL_DAZZLING_FLASH,
    SPELL_FORCE_LANCE,
    SPELL_MALMUTATE,
    SPELL_MIGHT_OTHER,
    SPELL_SENTINEL_MARK,
    SPELL_WORD_OF_RECALL,
    SPELL_INJURY_BOND,
    SPELL_SPECTRAL_CLOUD,
    SPELL_GHOSTLY_FIREBALL,
    SPELL_CALL_LOST_SOUL,
    SPELL_DIMENSION_ANCHOR,
    SPELL_BLINK_ALLIES_ENCIRCLE,
    SPELL_AWAKEN_VINES,
#if TAG_MAJOR_VERSION == 34
    SPELL_CONTROL_WINDS,
#endif
    SPELL_THORN_VOLLEY,
    SPELL_WALL_OF_BRAMBLES,
    SPELL_WATERSTRIKE,
#if TAG_MAJOR_VERSION == 34
    SPELL_HASTE_PLANTS,
#endif
    SPELL_WIND_BLAST,
    SPELL_STRIP_WILLPOWER,
#if TAG_MAJOR_VERSION == 34
    SPELL_INFUSION,
#endif
    SPELL_WEREBLOOD,
#if TAG_MAJOR_VERSION == 34
    SPELL_SPECTRAL_WEAPON,
    SPELL_SONG_OF_SHIELDING,
#endif
    SPELL_SUMMON_VERMIN,
    SPELL_MALIGN_OFFERING,
    SPELL_SEARING_RAY,
    SPELL_DISCORD,
#if TAG_MAJOR_VERSION == 34
    SPELL_SHAFT_SELF,
#endif
    SPELL_BLINKBOLT,
    SPELL_INVISIBILITY_OTHER,
    SPELL_VIRULENCE,
#if TAG_MAJOR_VERSION == 34
    SPELL_IGNITE_POISON_SINGLE,
#endif
    SPELL_ORB_OF_ELECTRICITY,
#if TAG_MAJOR_VERSION == 34
    SPELL_EXPLOSIVE_BOLT,
#endif
    SPELL_FLASH_FREEZE,
    SPELL_LEGENDARY_DESTRUCTION,
#if TAG_MAJOR_VERSION == 34
    SPELL_EPHEMERAL_INFUSION,
#endif
    SPELL_FORCEFUL_INVITATION,
    SPELL_PLANEREND,
    SPELL_CHAIN_OF_CHAOS,
    SPELL_CALL_OF_CHAOS,
    SPELL_BLACK_MARK,
#if TAG_MAJOR_VERSION == 34
    SPELL_GRAND_AVATAR,
#endif
    SPELL_SAP_MAGIC,
#if TAG_MAJOR_VERSION == 34
    SPELL_CORRUPT_BODY,
    SPELL_REARRANGE_PIECES,
#endif
    SPELL_MAJOR_DESTRUCTION,
    SPELL_BLINK_ALLIES_AWAY,
    SPELL_SHADOW_SHARD,
    SPELL_SHADOW_BOLT,
    SPELL_CRYSTAL_BOLT,
    SPELL_SUMMON_FOREST,
    SPELL_SUMMON_LIGHTNING_SPIRE,
    SPELL_SUMMON_GUARDIAN_GOLEM,
#if TAG_MAJOR_VERSION == 34
    SPELL_RANDOM_BOLT,
    SPELL_CLOUD_CONE,
    SPELL_WEAVE_SHADOWS,
#endif
    SPELL_DRAGON_CALL,
    SPELL_SPELLFORGED_SERVITOR,
#if TAG_MAJOR_VERSION == 34
    SPELL_FORCEFUL_DISMISSAL,
#endif
    SPELL_SUMMON_MANA_VIPER,
    SPELL_PHANTOM_MIRROR,
    SPELL_DRAIN_MAGIC,
    SPELL_CORROSIVE_BOLT,
#if TAG_MAJOR_VERSION == 34
    SPELL_SERPENT_OF_HELL_BREATH_REMOVED,
#endif
    SPELL_SUMMON_EMPEROR_SCORPIONS,
#if TAG_MAJOR_VERSION == 34
    SPELL_HYDRA_FORM,
    SPELL_IRRADIATE,
#endif
    SPELL_SPIT_LAVA,
    SPELL_ELECTRICAL_BOLT,
    SPELL_FLAMING_CLOUD,
    SPELL_THROW_BARBS,
    SPELL_BATTLECRY,
    SPELL_WARNING_CRY,
    SPELL_SEAL_DOORS,
    SPELL_FLAY,
    SPELL_BERSERK_OTHER,
#if TAG_MAJOR_VERSION == 34
    SPELL_THROW,
#endif
    SPELL_CORRUPTING_PULSE,
    SPELL_SIREN_SONG,
    SPELL_AVATAR_SONG,
    SPELL_PARALYSIS_GAZE,
    SPELL_CONFUSION_GAZE,
    SPELL_DRAINING_GAZE,
    SPELL_DEATH_RATTLE,
    SPELL_SUMMON_SCARABS,
#if TAG_MAJOR_VERSION == 34
    SPELL_HUNTING_CRY,
#endif
    SPELL_SEARING_BREATH,
    SPELL_CHILLING_BREATH,
#if TAG_MAJOR_VERSION == 34
    SPELL_SCATTERSHOT,
#endif
    SPELL_CLEANSING_FLAME,
    SPELL_THROW_ALLY,
#if TAG_MAJOR_VERSION == 34
    SPELL_CIGOTUVIS_EMBRACE,
    SPELL_SINGULARITY,
#endif
    SPELL_GRAVITAS,
#if TAG_MAJOR_VERSION == 34
    SPELL_CHANT_FIRE_STORM,
#endif
    SPELL_ENTROPIC_WEAVE,
    SPELL_SUMMON_EXECUTIONERS,
    SPELL_VIOLENT_UNRAVELLING,
    SPELL_DOOM_HOWL,
    SPELL_AWAKEN_EARTH,
    SPELL_AURA_OF_BRILLIANCE,
    SPELL_ICEBLAST,
    SPELL_SLUG_DART,
    SPELL_SPRINT,
    SPELL_GREATER_SERVANT_MAKHLEB,
    SPELL_SERPENT_OF_HELL_GEH_BREATH,
    SPELL_SERPENT_OF_HELL_COC_BREATH,
    SPELL_SERPENT_OF_HELL_TAR_BREATH,
    SPELL_SERPENT_OF_HELL_DIS_BREATH,
    SPELL_BIND_SOULS,
    SPELL_INFESTATION,
    SPELL_STILL_WINDS,
    SPELL_RESONANCE_STRIKE,
    SPELL_GHOSTLY_SACRIFICE,
    SPELL_DREAM_DUST,
    SPELL_BECKONING,
    SPELL_UPHEAVAL,
    SPELL_RANDOM_EFFECTS,
    SPELL_POISONOUS_VAPOURS,
#if TAG_MAJOR_VERSION == 34
    SPELL_RING_OF_THUNDER,
#endif
    SPELL_IGNITION,
#if TAG_MAJOR_VERSION == 34
    SPELL_VORTEX,
#endif
    SPELL_BORGNJORS_VILE_CLUTCH,
    SPELL_HARPOON_SHOT,
    SPELL_GRASPING_ROOTS,
    SPELL_SONIC_WAVE,
    SPELL_THROW_PIE,
    SPELL_SPORULATE,
    SPELL_STARBURST,
    SPELL_FOXFIRE,
    SPELL_HAILSTORM,
    SPELL_NOXIOUS_BOG,
    SPELL_AGONY_RANGE,
    SPELL_DISPEL_UNDEAD_RANGE,
    SPELL_FROZEN_RAMPARTS,
    SPELL_MAXWELLS_COUPLING,
    SPELL_ROLL,
    SPELL_SPLINTERSPRAY,
    SPELL_WOODWEAL,
    SPELL_HURL_SLUDGE,
    SPELL_MARSHLIGHT,
    SPELL_SUMMON_TZITZIMITL,
    SPELL_SUMMON_HELL_SENTINEL,
    SPELL_ANIMATE_ARMOUR,
    SPELL_MANIFOLD_ASSAULT,
    SPELL_CREEPING_FROST,
    SPELL_CALL_DOWN_LIGHTNING,
#if TAG_MAJOR_VERSION == 34
    SPELL_GOAD_BEASTS,
#endif
    SPELL_CONCENTRATE_VENOM,
    SPELL_ERUPTION,
    SPELL_PYROCLASTIC_SURGE,
    SPELL_WEAKENING_GAZE,
    SPELL_STORM_FORM,
    SPELL_STUNNING_BURST,
    SPELL_CORRUPT_LOCALE,
    SPELL_SUMMON_CACTUS,
    SPELL_STOKE_FLAMES,
    SPELL_SERACFALL,
    SPELL_SCORCH,
    SPELL_FLAME_WAVE,
    SPELL_CONJURE_LIVING_SPELLS,
    SPELL_ENFEEBLE,
    SPELL_SUMMON_SPIDERS,
    SPELL_ANGUISH,
    SPELL_SHEZAS_DANCE,
    SPELL_NECROTISE,
    SPELL_BOLT_OF_LIGHT,
    SPELL_HUNTING_CALL,
    SPELL_FASTROOT,
    SPELL_BLASTMOTE,
    SPELL_ROT,
    SPELL_ARCJOLT,
    SPELL_KISS_OF_DEATH,
    SPELL_MOMENTUM_STRIKE,
    SPELL_ELECTRIC_CHARGE,
    SPELL_MEPHITIC_BREATH,
    SPELL_PLASMA_BEAM,
    SPELL_MINOR_DESTRUCTION,
    SPELL_UNMAKING,
    NUM_SPELLS
};
