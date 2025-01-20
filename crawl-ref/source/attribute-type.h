#pragma once

#include "tag-version.h"

enum attribute_type
{
#if TAG_MAJOR_VERSION == 34
    ATTR_DIVINE_LIGHTNING_PROTECTION,
    ATTR_DIVINE_REGENERATION,
#endif
    ATTR_DIVINE_DEATH_CHANNEL,
    ATTR_CARD_COUNTDOWN,
    ATTR_BANISHMENT_IMMUNITY,   // banishment immunity until
#if TAG_MAJOR_VERSION == 34
    ATTR_DELAYED_FIREBALL,      // bwr: reserve fireballs
#endif
    ATTR_HELD,                  // caught in a net or web
    ATTR_ABYSS_ENTOURAGE,       // maximum number of hostile monsters in
                                // sight of the player while in the Abyss.
    ATTR_DIVINE_VIGOUR,         // strength of Ely's Divine Vigour
    ATTR_DIVINE_STAMINA,        // strength of Zin's Divine Stamina
#if TAG_MAJOR_VERSION == 34
    ATTR_DIVINE_SHIELD,         // strength of TSO's Divine Shield
    ATTR_WEAPON_SWAP_INTERRUPTED,
#endif
    ATTR_GOLD_FOUND,
    ATTR_PURCHASES,            // Gold amount spent at shops.
    ATTR_DONATIONS,            // Gold amount donated to Zin.
    ATTR_MISC_SPENDING,        // Spending for things like ziggurats.
#if TAG_MAJOR_VERSION == 34
    ATTR_UNUSED1,              // was ATTR_RND_LVL_BOOKS
    ATTR_NOISES,
    ATTR_SHADOWS,              // Lantern of shadows effect.
    ATTR_UNUSED2,              // was ATTR_FRUIT_FOUND
    ATTR_FLIGHT_UNCANCELLABLE, // Potion of enlightenment is in effect.
    ATTR_INVIS_UNCANCELLABLE,  // Spell/potion of invis is in effect.
#endif
    ATTR_PERM_FLIGHT,          // Cache for flight from equipment
    ATTR_SEEN_INVIS_TURN,      // Last turn you saw something invisible.
    ATTR_SEEN_INVIS_SEED,      // Random seed for invis monster positions.
#if TAG_MAJOR_VERSION == 34
    ATTR_UNUSED3,            // old Beastly Appendage
#endif
    ATTR_TITHE_BASE,           // Remainder of untithed gold.
    ATTR_EVOL_XP,              // XP needed for next [d]evolution mutation
    ATTR_LIFE_GAINED,          // XL when a felid gained a life.
    ATTR_TEMP_MUTATIONS,       // Number of temporary mutations the player has.
    ATTR_TEMP_MUT_XP,          // Amount of XP remaining before some temp muts
                               // will be removed
#if TAG_MAJOR_VERSION == 34
    ATTR_NEXT_RECALL_TIME,     // aut remaining until next ally will be recalled
    ATTR_NEXT_RECALL_INDEX,    // index+1 into recall_list for next recall
    ATTR_EVOKER_XP,            // How much xp remaining until next evoker charge
#endif
    ATTR_SEEN_BEOGH,           // Did an orc priest already offer conversion?
    ATTR_CHANNELLED_SPELL,     // Which spell are we currently channelling?
    ATTR_CHANNEL_DURATION,     // How many turns have we been channelling it?
    ATTR_RECITE_TYPE,          // Recitation type.
    ATTR_RECITE_SEED,          // Recite text seed.
#if TAG_MAJOR_VERSION == 34
    ATTR_RECITE_HP,            // HP on start of recitation.
#endif
    ATTR_SWIFTNESS,            // Duration of future antiswiftness.
#if TAG_MAJOR_VERSION == 34
    ATTR_BARBS_MSG,            // Have we already printed a message on move?
#endif
    ATTR_BARBS_POW,            // How badly we are currently skewered
#if TAG_MAJOR_VERSION == 34
    ATTR_REPEL_MISSILES,       // Repel missiles active
    ATTR_DEFLECT_MISSILES,     // Deflect missiles active
    ATTR_PORTAL_PROJECTILE,    // Accuracy bonus during portal projectile
#endif
    ATTR_GOD_WRATH_XP,         // How much XP before our next god wrath check?
    ATTR_GOD_WRATH_COUNT,      // Number of stored retributions
    ATTR_NEXT_DRAGON_TIME,     // aut remaining until Dragon's Call summons another
    ATTR_GOLD_GENERATED,       // Count gold generated on non-Abyss levels this game.
#if TAG_MAJOR_VERSION == 34
    ATTR_GOZAG_POTIONS,        // Number of times you've bought potions from Gozag.
#endif
    ATTR_GOZAG_SHOPS,          // Number of shops you've funded from Gozag.
    ATTR_GOZAG_SHOPS_CURRENT,  // As above, but since most recent time worshipping.
#if TAG_MAJOR_VERSION == 34
    ATTR_DIVINE_FIRE_RES,      // Divine fire resistance (Qazlal).
    ATTR_DIVINE_COLD_RES,      // Divine cold resistance (Qazlal).
    ATTR_DIVINE_ELEC_RES,      // Divine electricity resistance (Qazlal).
    ATTR_DIVINE_AC,            // Divine AC bonus (Qazlal).
#endif
    ATTR_GOZAG_GOLD_USED,      // Gold spent for Gozag abilities.
#if TAG_MAJOR_VERSION == 34
    ATTR_BONE_ARMOUR,          // Current amount of bony armour (from the spell)
    ATTR_LAST_FLIGHT_STATUS,   // Whether SPARM_FLIGHT should be restored after form change
    ATTR_GOZAG_FIRST_POTION,   // Gozag's free first usage of Potion Petition.
#endif
    ATTR_STAT_ZERO,            // Whether one of the player's stats has fallen
                               // to zero or below (and they have been notified).
#if TAG_MAJOR_VERSION == 34
    ATTR_SURGE_REMOVED,        // Was surge power applied to next evocation.
    ATTR_PAKELLAS_EXTRA_MP,    // MP to be collected to get a !magic from P
    ATTR_DIVINE_ENERGY,        // Divine energy from Sif to cast with no MP.
#endif
    ATTR_SERPENTS_LASH,        // Remaining instant movement actions.
#if TAG_MAJOR_VERSION == 34
    ATTR_HEAVENLY_STORM,       // Strength of Heavenly Storm slaying.
    ATTR_WALL_JUMP_READY,      // Ready to perform a wall jump.
#endif
    ATTR_DEATHS_DOOR_HP,       // How much HP we should have under Death's Door
    ATTR_VOUCHER,              // How many shop vouchers you have
    ATTR_TRAITOR,              // God who has been abandoned with an invocations title
    NUM_ATTRIBUTES
};
