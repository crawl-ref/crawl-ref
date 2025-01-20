#include "AppHdr.h"

#include "tilepick.h"

#include "ability.h"
#include "artefact.h"
#include "art-enum.h"
#include "branch.h" // vaults_is_locked
#include "cloud.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "describe.h"
#include "debug.h"
#include "duration-type.h"
#include "env.h"
#include "tile-env.h"
#include "files.h"
#include "god-companions.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "level-state-type.h"
#include "libutil.h"
#include "misc.h" // december_holidays
#include "mon-death.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "options.h"
#include "player.h"
#include "shopping.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "rltiles/tiledef-dngn.h"
#include "tile-flags.h"
#include "rltiles/tiledef-gui.h"
#include "rltiles/tiledef-icons.h"
#include "rltiles/tiledef-main.h"
#include "rltiles/tiledef-player.h"
#include "rltiles/tiledef-unrand.h"
#include "tag-version.h"
#include "tilemcache.h"
#include "tilepick-p.h"
#include "tileview.h"
#include "transform.h"
#include "traps.h"
#include "viewgeom.h"

// This should not be changed.
COMPILE_CHECK(TILE_DNGN_UNSEEN == 0);

// NOTE: If one of the following asserts fail, it's because the corresponding
// enum in item-prop-enum.h was modified, but rltiles/dc-item.txt was not
// modified in parallel.

// These brands start with "normal" which there's no tile for, so subtract 1.
COMPILE_CHECK(NUM_REAL_SPECIAL_WEAPONS - 1
              == TILE_BRAND_WEP_LAST - TILE_BRAND_WEP_FIRST + 1);
COMPILE_CHECK(NUM_REAL_SPECIAL_ARMOURS - 1
              == TILE_BRAND_ARM_LAST - TILE_BRAND_ARM_FIRST + 1);

COMPILE_CHECK(NUM_RINGS == TILE_RING_ID_LAST - TILE_RING_ID_FIRST + 1);
COMPILE_CHECK(NUM_JEWELLERY - AMU_FIRST_AMULET
              == TILE_AMU_ID_LAST - TILE_AMU_ID_FIRST + 1);
COMPILE_CHECK(NUM_SCROLLS == TILE_SCR_ID_LAST - TILE_SCR_ID_FIRST + 1);
COMPILE_CHECK(NUM_STAVES == TILE_STAFF_ID_LAST - TILE_STAFF_ID_FIRST + 1);
#if TAG_MAJOR_VERSION == 34
COMPILE_CHECK(NUM_RODS == TILE_ROD_ID_LAST - TILE_ROD_ID_FIRST + 1);
#endif
COMPILE_CHECK(NUM_WANDS == TILE_WAND_ID_LAST - TILE_WAND_ID_FIRST + 1);
COMPILE_CHECK(NUM_POTIONS == TILE_POT_ID_LAST - TILE_POT_ID_FIRST + 1);

#ifdef USE_TILE
TextureID get_tile_texture(tileidx_t idx)
{
    if (idx < TILE_FLOOR_MAX)
        return TEX_FLOOR;
    else if (idx < TILE_WALL_MAX)
        return TEX_WALL;
    else if (idx < TILE_FEAT_MAX)
        return TEX_FEAT;
    else if (idx < TILE_MAIN_MAX)
        return TEX_DEFAULT;
    else if (idx < TILEP_PLAYER_MAX)
        return TEX_PLAYER;
    else if (idx < TILEG_GUI_MAX)
        return TEX_GUI;
    else if (idx < TILEI_ICONS_MAX)
        return TEX_ICONS;
    else
        die("Cannot get texture for bad tileidx %" PRIu64, idx);
}
#endif

tileidx_t tileidx_trap(trap_type type)
{
    switch (type)
    {
#if TAG_MAJOR_VERSION == 34
    case TRAP_SPEAR:
        return TILE_DNGN_TRAP_SPEAR;
    case TRAP_BOLT:
        return TILE_DNGN_TRAP_BOLT;
#endif
    case TRAP_DISPERSAL:
        return TILE_DNGN_TRAP_DISPERSAL;
    case TRAP_TELEPORT:
        return TILE_DNGN_TRAP_TELEPORT;
    case TRAP_TELEPORT_PERMANENT:
        return TILE_DNGN_TRAP_TELEPORT_PERMANENT;
    case TRAP_TYRANT:
        return TILE_DNGN_TRAP_TYRANT;
    case TRAP_ARCHMAGE:
        return TILE_DNGN_TRAP_ARCHMAGE;
    case TRAP_HARLEQUIN:
        return TILE_DNGN_TRAP_HARLEQUIN;
    case TRAP_DEVOURER:
        return TILE_DNGN_TRAP_DEVOURER;
    case TRAP_ALARM:
        return TILE_DNGN_TRAP_ALARM;
    case TRAP_NET:
        return TILE_DNGN_TRAP_NET;
    case TRAP_ZOT:
        return TILE_DNGN_TRAP_ZOT;
    case TRAP_SHAFT:
        return TILE_DNGN_TRAP_SHAFT;
    case TRAP_GOLUBRIA:
        return TILE_DNGN_TRAP_GOLUBRIA;
    case TRAP_PLATE:
        return TILE_DNGN_TRAP_PLATE;
    case TRAP_WEB:
        return TILE_DNGN_TRAP_WEB;
    default:
        return TILE_DNGN_ERROR;
    }
}

tileidx_t tileidx_shop(const shop_struct *shop)
{
    if (!shop)
        return TILE_DNGN_ERROR;

    switch (shop->type)
    {
        case SHOP_WEAPON:
        case SHOP_WEAPON_ANTIQUE:
            return TILE_SHOP_WEAPONS;
        case SHOP_ARMOUR:
        case SHOP_ARMOUR_ANTIQUE:
            return TILE_SHOP_ARMOUR;
        case SHOP_JEWELLERY:
            return TILE_SHOP_JEWELLERY;
#if TAG_MAJOR_VERSION == 34
        case SHOP_EVOKABLES:
            return TILE_SHOP_GADGETS;
        case SHOP_FOOD:
            return TILE_SHOP_FOOD;
#endif
        case SHOP_BOOK:
            return TILE_SHOP_BOOKS;
        case SHOP_SCROLL:
            return TILE_SHOP_SCROLLS;
        case SHOP_DISTILLERY:
            return TILE_SHOP_POTIONS;
        case SHOP_GENERAL:
        case SHOP_GENERAL_ANTIQUE:
            return TILE_SHOP_GENERAL;
        default:
            return TILE_DNGN_ERROR;
    }
}

tileidx_t tileidx_feature_base(dungeon_feature_type feat)
{
    switch (feat)
    {
    case DNGN_UNSEEN:
        return TILE_DNGN_UNSEEN;
    case DNGN_ROCK_WALL:
        return TILE_WALL_NORMAL;
    case DNGN_PERMAROCK_WALL:
        return TILE_WALL_PERMAROCK;
    case DNGN_SLIMY_WALL:
        return TILE_WALL_SLIME;
    case DNGN_RUNED_DOOR:
        return TILE_DNGN_RUNED_DOOR;
    case DNGN_RUNED_CLEAR_DOOR:
        return TILE_DNGN_RUNED_CLEAR_DOOR;
    case DNGN_SEALED_DOOR:
        return TILE_DNGN_SEALED_DOOR;
    case DNGN_SEALED_CLEAR_DOOR:
        return TILE_DNGN_SEALED_CLEAR_DOOR;
    case DNGN_GRATE:
        return TILE_DNGN_GRATE;
    case DNGN_CLEAR_ROCK_WALL:
        return TILE_DNGN_TRANSPARENT_WALL;
    case DNGN_CLEAR_STONE_WALL:
        return TILE_DNGN_TRANSPARENT_STONE;
    case DNGN_CLEAR_PERMAROCK_WALL:
        return TILE_WALL_PERMAROCK_CLEAR;
    case DNGN_STONE_WALL:
        return TILE_DNGN_STONE_WALL;
    case DNGN_CLOSED_DOOR:
        return TILE_DNGN_CLOSED_DOOR;
    case DNGN_CLOSED_CLEAR_DOOR:
        return TILE_DNGN_CLOSED_CLEAR_DOOR;
    case DNGN_METAL_WALL:
        return TILE_DNGN_METAL_WALL;
    case DNGN_CRYSTAL_WALL:
        return TILE_DNGN_CRYSTAL_WALL;
    case DNGN_ORCISH_IDOL:
        return TILE_DNGN_ORCISH_IDOL;
    case DNGN_TREE:
        return TILE_DNGN_TREE;
    case DNGN_MANGROVE:
        return TILE_DNGN_MANGROVE;
    case DNGN_PETRIFIED_TREE:
        return TILE_DNGN_PETRIFIED_TREE;
    case DNGN_DEMONIC_TREE:
        return TILE_DNGN_DEMONIC_TREE;
    case DNGN_METAL_STATUE:
        return TILE_DNGN_METAL_STATUE;
    case DNGN_GRANITE_STATUE:
        return TILE_DNGN_GRANITE_STATUE;
    case DNGN_LAVA:
        return TILE_DNGN_LAVA;
    case DNGN_LAVA_SEA:
        return TILE_DNGN_LAVA_SEA;
    case DNGN_DEEP_WATER:
        return TILE_DNGN_DEEP_WATER;
    case DNGN_SHALLOW_WATER:
        return TILE_DNGN_SHALLOW_WATER;
    case DNGN_OPEN_SEA:
        return TILE_DNGN_OPEN_SEA;
    case DNGN_TOXIC_BOG:
        return TILE_DNGN_TOXIC_BOG;
    case DNGN_MUD:
        return TILE_LIQUEFACTION;
    case DNGN_FLOOR:
        return TILE_FLOOR_NORMAL;
    case DNGN_ENDLESS_SALT:
        return TILE_DNGN_ENDLESS_SALT;
    case DNGN_ENTER_HELL:
        return TILE_DNGN_ENTER_HELL;
    case DNGN_EXIT_DIS:
    case DNGN_EXIT_GEHENNA:
    case DNGN_EXIT_COCYTUS:
    case DNGN_EXIT_TARTARUS:
        return TILE_DNGN_RETURN_VESTIBULE;
    case DNGN_OPEN_DOOR:
        return TILE_DNGN_OPEN_DOOR;
    case DNGN_BROKEN_DOOR:
        return TILE_DNGN_BROKEN_DOOR;
    case DNGN_OPEN_CLEAR_DOOR:
        return TILE_DNGN_OPEN_CLEAR_DOOR;
    case DNGN_BROKEN_CLEAR_DOOR:
        return TILE_DNGN_BROKEN_CLEAR_DOOR;
#if TAG_MAJOR_VERSION == 34
    case DNGN_TRAP_MECHANICAL:
        return TILE_DNGN_TRAP_BOLT;
    case DNGN_TRAP_SPEAR:
        return TILE_DNGN_TRAP_SPEAR;
    case DNGN_TRAP_BOLT:
        return TILE_DNGN_TRAP_BOLT;
#endif
    case DNGN_TRAP_NET:
        return TILE_DNGN_TRAP_NET;
    case DNGN_TRAP_PLATE:
        return TILE_DNGN_TRAP_PLATE;
    case DNGN_TRAP_DISPERSAL:
        return TILE_DNGN_TRAP_DISPERSAL;
    case DNGN_TRAP_TELEPORT:
        return TILE_DNGN_TRAP_TELEPORT;
    case DNGN_TRAP_TELEPORT_PERMANENT:
        return TILE_DNGN_TRAP_TELEPORT_PERMANENT;
    case DNGN_TRAP_TYRANT:
        return TILE_DNGN_TRAP_TYRANT;
    case DNGN_TRAP_ARCHMAGE:
        return TILE_DNGN_TRAP_ARCHMAGE;
    case DNGN_TRAP_HARLEQUIN:
        return TILE_DNGN_TRAP_HARLEQUIN;
    case DNGN_TRAP_DEVOURER:
        return TILE_DNGN_TRAP_DEVOURER;
    case DNGN_TRAP_ALARM:
        return TILE_DNGN_TRAP_ALARM;
    case DNGN_TRAP_ZOT:
        return TILE_DNGN_TRAP_ZOT;
    case DNGN_TRAP_SHAFT:
        return TILE_DNGN_TRAP_SHAFT;
    case DNGN_TRAP_WEB:
        return TILE_DNGN_TRAP_WEB;
#if TAG_MAJOR_VERSION == 34
    case DNGN_TELEPORTER:
        return TILE_DNGN_TRAP_GOLUBRIA;
#endif
    case DNGN_TRANSPORTER:
        return TILE_DNGN_TRANSPORTER;
    case DNGN_TRANSPORTER_LANDING:
        return TILE_DNGN_TRANSPORTER_LANDING;
    case DNGN_ENTER_SHOP:
        return TILE_SHOP_GENERAL;
    case DNGN_ABANDONED_SHOP:
        return TILE_DNGN_ABANDONED_SHOP;
    case DNGN_ENTER_GAUNTLET:
        return TILE_DNGN_PORTAL_GAUNTLET;
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
        return TILE_DNGN_STONE_STAIRS_DOWN;
    case DNGN_ESCAPE_HATCH_DOWN:
        return TILE_DNGN_ESCAPE_HATCH_DOWN;
    case DNGN_SEALED_STAIRS_DOWN:
        return TILE_DNGN_SEALED_STAIRS_DOWN;
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
        return TILE_DNGN_STONE_STAIRS_UP;
    case DNGN_EXIT_GAUNTLET:
    case DNGN_ESCAPE_HATCH_UP:
        return TILE_DNGN_ESCAPE_HATCH_UP;
    case DNGN_SEALED_STAIRS_UP:
        return TILE_DNGN_SEALED_STAIRS_UP;
    case DNGN_EXIT_DUNGEON:
        return TILE_DNGN_EXIT_DUNGEON;
    case DNGN_ENTER_DIS:
        return TILE_DNGN_ENTER_DIS;
    case DNGN_ENTER_GEHENNA:
        return TILE_DNGN_ENTER_GEHENNA;
    case DNGN_ENTER_COCYTUS:
        return TILE_DNGN_ENTER_COCYTUS;
    case DNGN_ENTER_TARTARUS:
        return TILE_DNGN_ENTER_TARTARUS;
    case DNGN_ENTER_ABYSS:
    case DNGN_EXIT_THROUGH_ABYSS:
        return TILE_DNGN_ENTER_ABYSS;
    case DNGN_ABYSSAL_STAIR:
        return TILE_DNGN_ABYSSAL_STAIR;
    case DNGN_EXIT_HELL:
        return TILE_DNGN_RETURN_HELL;
    case DNGN_EXIT_ABYSS:
        return TILE_DNGN_EXIT_ABYSS;
    case DNGN_STONE_ARCH:
        if (player_in_branch(BRANCH_VESTIBULE))
            return TILE_DNGN_STONE_ARCH_HELL;
        return TILE_DNGN_STONE_ARCH;
    case DNGN_ENTER_PANDEMONIUM:
        return TILE_DNGN_ENTER_PANDEMONIUM;
    case DNGN_TRANSIT_PANDEMONIUM:
        return TILE_DNGN_TRANSIT_PANDEMONIUM;
    case DNGN_EXIT_PANDEMONIUM:
        return TILE_DNGN_EXIT_PANDEMONIUM;

    // branch entry stairs
#if TAG_MAJOR_VERSION == 34
    case DNGN_ENTER_DWARF:
    case DNGN_ENTER_FOREST:
    case DNGN_ENTER_BLADE:
        return TILE_DNGN_ENTER;
#endif
    case DNGN_ENTER_TEMPLE:
        return TILE_DNGN_ENTER_TEMPLE;
    case DNGN_ENTER_ORC:
        return TILE_DNGN_ENTER_ORC;
    case DNGN_ENTER_ELF:
        return TILE_DNGN_ENTER_ELF;
    case DNGN_ENTER_LAIR:
        return TILE_DNGN_ENTER_LAIR;
    case DNGN_ENTER_SNAKE:
        return TILE_DNGN_ENTER_SNAKE;
    case DNGN_ENTER_SWAMP:
        return TILE_DNGN_ENTER_SWAMP;
    case DNGN_ENTER_SPIDER:
        return TILE_DNGN_ENTER_SPIDER;
    case DNGN_ENTER_SHOALS:
        return TILE_DNGN_ENTER_SHOALS;
    case DNGN_ENTER_SLIME:
        return TILE_DNGN_ENTER_SLIME;
    case DNGN_ENTER_DEPTHS:
        return TILE_DNGN_ENTER_DEPTHS;
    case DNGN_ENTER_VAULTS:
        return TILE_DNGN_ENTER_VAULTS;
    case DNGN_ENTER_CRYPT:
        return TILE_DNGN_ENTER_CRYPT;
    case DNGN_ENTER_TOMB:
        return TILE_DNGN_ENTER_TOMB;
    case DNGN_ENTER_ZOT:
        return you.level_visited(level_id(BRANCH_ZOT, 1)) ? TILE_DNGN_ENTER_ZOT_OPEN
                              : TILE_DNGN_ENTER_ZOT_CLOSED;
    case DNGN_ENTER_ZIGGURAT:
        return TILE_DNGN_PORTAL_ZIGGURAT;
    case DNGN_ENTER_BAZAAR:
        return TILE_DNGN_PORTAL_BAZAAR;
    case DNGN_ENTER_TROVE:
        return TILE_DNGN_PORTAL_TROVE;
    case DNGN_ENTER_SEWER:
        return TILE_DNGN_PORTAL_SEWER;
    case DNGN_ENTER_OSSUARY:
        return TILE_DNGN_PORTAL_OSSUARY;
    case DNGN_ENTER_BAILEY:
        return TILE_DNGN_PORTAL_BAILEY;
    case DNGN_ENTER_ICE_CAVE:
        return TILE_DNGN_PORTAL_ICE_CAVE;
    case DNGN_ENTER_VOLCANO:
        return TILE_DNGN_PORTAL_VOLCANO;
    case DNGN_ENTER_WIZLAB:
        return TILE_DNGN_PORTAL_WIZARD_LAB;
    case DNGN_ENTER_DESOLATION:
        return TILE_DNGN_PORTAL_DESOLATION;

    // branch exit stairs
#if TAG_MAJOR_VERSION == 34
    case DNGN_EXIT_DWARF:
    case DNGN_EXIT_FOREST:
    case DNGN_EXIT_BLADE:
        return TILE_DNGN_RETURN;
#endif
    case DNGN_EXIT_TEMPLE:
        return TILE_DNGN_EXIT_TEMPLE;
    case DNGN_EXIT_ORC:
        return TILE_DNGN_EXIT_ORC;
    case DNGN_EXIT_ELF:
        return TILE_DNGN_EXIT_ELF;
    case DNGN_EXIT_LAIR:
        return TILE_DNGN_EXIT_LAIR;
    case DNGN_EXIT_SNAKE:
        return TILE_DNGN_EXIT_SNAKE;
    case DNGN_EXIT_SWAMP:
        return TILE_DNGN_EXIT_SWAMP;
    case DNGN_EXIT_SPIDER:
        return TILE_DNGN_EXIT_SPIDER;
    case DNGN_EXIT_SHOALS:
        return TILE_DNGN_EXIT_SHOALS;
    case DNGN_EXIT_SLIME:
        return TILE_DNGN_EXIT_SLIME;
    case DNGN_EXIT_DEPTHS:
        return TILE_DNGN_RETURN_DEPTHS;
    case DNGN_EXIT_VAULTS:
        return vaults_is_locked() ? TILE_DNGN_EXIT_VAULTS_CLOSED
                                  : TILE_DNGN_EXIT_VAULTS_OPEN;
    case DNGN_EXIT_CRYPT:
        return TILE_DNGN_EXIT_CRYPT;
    case DNGN_EXIT_TOMB:
        return TILE_DNGN_EXIT_TOMB;
    case DNGN_EXIT_ZOT:
        return TILE_DNGN_RETURN_ZOT;

    case DNGN_EXIT_ZIGGURAT:
    case DNGN_EXIT_BAZAAR:
    case DNGN_EXIT_TROVE:
    case DNGN_EXIT_SEWER:
    case DNGN_EXIT_OSSUARY:
    case DNGN_EXIT_BAILEY:
    case DNGN_EXIT_DESOLATION:
    case DNGN_EXIT_ARENA:
    case DNGN_EXIT_CRUCIBLE:
        return TILE_DNGN_PORTAL;
    case DNGN_EXIT_ICE_CAVE:
        return TILE_DNGN_PORTAL_ICE_CAVE;
    case DNGN_EXIT_VOLCANO:
        return TILE_DNGN_EXIT_VOLCANO;
    case DNGN_EXIT_WIZLAB:
        return TILE_DNGN_PORTAL_WIZARD_LAB;

#if TAG_MAJOR_VERSION == 34
    case DNGN_ENTER_PORTAL_VAULT:
    case DNGN_EXIT_PORTAL_VAULT:
        return TILE_DNGN_PORTAL;
#endif
    case DNGN_EXPIRED_PORTAL:
        return TILE_DNGN_PORTAL_EXPIRED;
    case DNGN_MALIGN_GATEWAY:
        return TILE_DNGN_STARRY_PORTAL;

    // altars
    case DNGN_ALTAR_ZIN:
        return TILE_DNGN_ALTAR_ZIN;
    case DNGN_ALTAR_SHINING_ONE:
        return TILE_DNGN_ALTAR_SHINING_ONE;
    case DNGN_ALTAR_KIKUBAAQUDGHA:
        return TILE_DNGN_ALTAR_KIKUBAAQUDGHA;
    case DNGN_ALTAR_YREDELEMNUL:
        return TILE_DNGN_ALTAR_YREDELEMNUL;
    case DNGN_ALTAR_XOM:
        return TILE_DNGN_ALTAR_XOM;
    case DNGN_ALTAR_VEHUMET:
        return TILE_DNGN_ALTAR_VEHUMET;
    case DNGN_ALTAR_OKAWARU:
        return TILE_DNGN_ALTAR_OKAWARU;
    case DNGN_ALTAR_MAKHLEB:
        return TILE_DNGN_ALTAR_MAKHLEB;
    case DNGN_ALTAR_SIF_MUNA:
        return TILE_DNGN_ALTAR_SIF_MUNA;
    case DNGN_ALTAR_TROG:
        return TILE_DNGN_ALTAR_TROG;
    case DNGN_ALTAR_NEMELEX_XOBEH:
        return TILE_DNGN_ALTAR_NEMELEX_XOBEH;
    case DNGN_ALTAR_ELYVILON:
        return TILE_DNGN_ALTAR_ELYVILON;
    case DNGN_ALTAR_LUGONU:
        return TILE_DNGN_ALTAR_LUGONU;
    case DNGN_ALTAR_BEOGH:
        return TILE_DNGN_ALTAR_BEOGH;
    case DNGN_ALTAR_JIYVA:
        return TILE_DNGN_ALTAR_JIYVA;
    case DNGN_ALTAR_FEDHAS:
        return TILE_DNGN_ALTAR_FEDHAS;
    case DNGN_ALTAR_CHEIBRIADOS:
        return TILE_DNGN_ALTAR_CHEIBRIADOS;
    case DNGN_ALTAR_ASHENZARI:
        return TILE_DNGN_ALTAR_ASHENZARI;
    case DNGN_ALTAR_DITHMENOS:
        return TILE_DNGN_ALTAR_DITHMENOS;
    case DNGN_ALTAR_GOZAG:
        return TILE_DNGN_ALTAR_GOZAG;
    case DNGN_ALTAR_QAZLAL:
        return TILE_DNGN_ALTAR_QAZLAL;
    case DNGN_ALTAR_RU:
        return TILE_DNGN_ALTAR_RU;
#if TAG_MAJOR_VERSION == 34
    case DNGN_ALTAR_PAKELLAS:
        return TILE_DNGN_ALTAR_PAKELLAS;
#endif
    case DNGN_ALTAR_USKAYAW:
        return TILE_DNGN_ALTAR_USKAYAW;
    case DNGN_ALTAR_HEPLIAKLQANA:
        return TILE_DNGN_ALTAR_HEPLIAKLQANA;
    case DNGN_ALTAR_WU_JIAN:
        return TILE_DNGN_ALTAR_WU_JIAN;
    case DNGN_ALTAR_IGNIS:
        return TILE_DNGN_ALTAR_IGNIS;
    case DNGN_ALTAR_ECUMENICAL:
        return TILE_DNGN_ALTAR_ECUMENICAL;
    case DNGN_FOUNTAIN_BLUE:
        return TILE_DNGN_BLUE_FOUNTAIN;
    case DNGN_FOUNTAIN_SPARKLING:
        return TILE_DNGN_SPARKLING_FOUNTAIN;
    case DNGN_FOUNTAIN_BLOOD:
        return TILE_DNGN_BLOOD_FOUNTAIN;
    case DNGN_FOUNTAIN_EYES:
        return TILE_DNGN_EYES_FOUNTAIN;
    case DNGN_DRY_FOUNTAIN:
        return TILE_DNGN_DRY_FOUNTAIN;
    case DNGN_CACHE_OF_FRUIT:
        return TILE_DNGN_CACHE_OF_FRUIT;
    case DNGN_CACHE_OF_MEAT:
        return TILE_DNGN_CACHE_OF_MEAT;
    case DNGN_CACHE_OF_BAKED_GOODS:
        return TILE_DNGN_CACHE_OF_BAKED_GOODS;
    case DNGN_RUNELIGHT:
        return TILE_DNGN_RUNELIGHT;
    case DNGN_PASSAGE_OF_GOLUBRIA:
        return TILE_DNGN_TRAP_GOLUBRIA;
    case DNGN_UNKNOWN_ALTAR:
        return TILE_DNGN_UNKNOWN_ALTAR;
    case DNGN_UNKNOWN_PORTAL:
        return TILE_DNGN_UNKNOWN_PORTAL;
    case DNGN_BINDING_SIGIL:
        return TILE_DNGN_BINDING_SIGIL;
    case DNGN_ORB_DAIS:
        return TILE_DNGN_ORB_DAIS;
    case DNGN_SPIKE_LAUNCHER:
        return TILE_DNGN_SPIKE_LAUNCHER;
    default:
        return TILE_DNGN_ERROR;
    }
}

#ifdef USE_TILE
bool is_door_tile(tileidx_t tile)
{
    return tile >= TILE_DNGN_CLOSED_DOOR &&
        tile < TILE_DNGN_STONE_ARCH;
}
#endif

tileidx_t tileidx_feature(const coord_def &gc)
{
    dungeon_feature_type feat = env.map_knowledge(gc).feat();

    tileidx_t override = tile_env.flv(gc).feat;
    bool can_override = !feat_is_door(feat)
                        && feat != DNGN_FLOOR
                        && feat != DNGN_UNSEEN
                        && feat != DNGN_PASSAGE_OF_GOLUBRIA
                        && feat != DNGN_MALIGN_GATEWAY
                        && feat != DNGN_BINDING_SIGIL
                        && feat != DNGN_UNKNOWN_PORTAL
                        && feat != DNGN_TREE; // summon forest spell
    if (override && can_override)
        return override;

    // Any grid-specific tiles.
    switch (feat)
    {
    case DNGN_FLOOR:
        if (env.level_state & LSTATE_SLIMY_WALL)
            for (adjacent_iterator ai(gc); ai; ++ai)
                if (env.map_knowledge(*ai).feat() == DNGN_SLIMY_WALL)
                    return TILE_FLOOR_SLIME_ACIDIC;

        if (env.level_state & LSTATE_ICY_WALL
            && env.map_knowledge(gc).flags & MAP_ICY)
        {
            return TILE_FLOOR_ICY;
        }
        // deliberate fall-through
    case DNGN_ROCK_WALL:
    case DNGN_CLEAR_ROCK_WALL:
    case DNGN_STONE_WALL:
    case DNGN_CRYSTAL_WALL:
    case DNGN_PERMAROCK_WALL:
    case DNGN_CLEAR_PERMAROCK_WALL:
    {
        unsigned colour = env.map_knowledge(gc).feat_colour();
        if (colour == 0)
        {
            colour = feat == DNGN_FLOOR     ? env.floor_colour :
                     feat == DNGN_ROCK_WALL ? env.rock_colour
                                            : 0; // meh
        }
        if (colour >= ETC_FIRST)
        {
            tileidx_t idx = (feat == DNGN_FLOOR) ? tile_env.flv(gc).floor :
                (feat == DNGN_ROCK_WALL) ? tile_env.flv(gc).wall
                : tileidx_feature_base(feat);

#ifdef USE_TILE
            if (feat == DNGN_STONE_WALL)
                apply_variations(tile_env.flv(gc), &idx, gc);
#endif

            tileidx_t base = tile_dngn_basetile(idx);
            tileidx_t spec = idx - base;
            unsigned rc = real_colour(colour, gc);
            return tile_dngn_coloured(base, rc) + spec; // XXX
        }
        // If there's an unseen change here, the old (remembered) flavour is
        // available in the terrain change marker
        if (!you.see_cell(gc))
            if (map_marker *mark = env.markers.find(gc, MAT_TERRAIN_CHANGE))
            {
                map_terrain_change_marker *marker =
                    dynamic_cast<map_terrain_change_marker*>(mark);
                if (marker->flv_old_feature)
                    return marker->flv_old_feature;
            }
        return tileidx_feature_base(feat);
    }

#if TAG_MAJOR_VERSION == 34
    // New trap-type-specific features are handled in default case.
    case DNGN_TRAP_MECHANICAL:
    case DNGN_TRAP_TELEPORT:
        return tileidx_trap(env.map_knowledge(gc).trap());
#endif

    case DNGN_TRAP_WEB:
    {
        /*
        trap_type this_trap_type = get_trap_type(gc);
        // There's room here to have different types of webs (acid? fire? ice? different strengths?)
        if (this_trap_type==TRAP_WEB) {*/

        // Determine web connectivity on all sides
        const coord_def neigh[4] =
        {
            coord_def(gc.x, gc.y - 1),
            coord_def(gc.x + 1, gc.y),
            coord_def(gc.x, gc.y + 1),
            coord_def(gc.x - 1, gc.y),
        };
        int solid = 0;
        for (int i = 0; i < 4; i++)
            if (feat_is_solid(env.map_knowledge(neigh[i]).feat())
                || env.map_knowledge(neigh[i]).trap() == TRAP_WEB)
            {
                solid |= 1 << i;
            }
        if (solid)
            return TILE_DNGN_TRAP_WEB_N - 1 + solid;
        return TILE_DNGN_TRAP_WEB;
    }
    case DNGN_ENTER_SHOP:
        return tileidx_shop(shop_at(gc));

    case DNGN_DEEP_WATER:
        if (env.map_knowledge(gc).feat_colour() == GREEN
            || env.map_knowledge(gc).feat_colour() == LIGHTGREEN)
        {
            return TILE_DNGN_DEEP_WATER_MURKY;
        }
        else if (player_in_branch(BRANCH_SHOALS))
            return TILE_SHOALS_DEEP_WATER;

        return TILE_DNGN_DEEP_WATER;
    case DNGN_SHALLOW_WATER:
        {
            tileidx_t t = TILE_DNGN_SHALLOW_WATER;
            if (env.map_knowledge(gc).feat_colour() == GREEN
                || env.map_knowledge(gc).feat_colour() == LIGHTGREEN)
            {
                t = TILE_DNGN_SHALLOW_WATER_MURKY;
            }
            else if (player_in_branch(BRANCH_SHOALS))
                t = TILE_SHOALS_SHALLOW_WATER;

            if (env.map_knowledge(gc).invisible_monster())
            {
                // Add disturbance to tile.
                t += tile_dngn_count(t);
            }

            return t;
        }
    default:
        return tileidx_feature_base(feat);
    }
}

static tileidx_t _mon_random(tileidx_t tile, int mon_id)
{
    int count = tile_player_count(tile);
    return tile + hash_with_seed(count, mon_id, you.frame_no);
}

#ifdef USE_TILE
static bool _mons_is_kraken_tentacle(const int mtype)
{
    return mtype == MONS_KRAKEN_TENTACLE
           || mtype == MONS_KRAKEN_TENTACLE_SEGMENT;
}
#endif

tileidx_t tileidx_tentacle(const monster_info& mon)
{
    ASSERT(mons_is_tentacle_or_tentacle_segment(mon.type));

    // Get tentacle position.
    const coord_def t_pos = mon.pos;
    // No parent tentacle, or the connection to the head is unknown.
    bool no_head_connect  = !mon.props.exists(INWARDS_KEY);
    coord_def h_pos       = coord_def(); // head position
    if (!no_head_connect)
    {
        // Get the parent tentacle's location.
        h_pos = t_pos + mon.props[INWARDS_KEY].get_coord();
    }
    if (no_head_connect && (mon.type == MONS_SNAPLASHER_VINE
                            || mon.type == MONS_SNAPLASHER_VINE_SEGMENT))
    {
        // Find an adjacent tree to pretend we're connected to.
        for (adjacent_iterator ai(t_pos); ai; ++ai)
        {
            if (feat_is_tree(env.grid(*ai)))
            {
                h_pos = *ai;
                no_head_connect = false;
                break;
            }
        }
    }

    // Is there a connection to the given direction?
    // (either through head or next)
    bool north = false, east = false,
        south = false, west = false,
        north_east = false, south_east = false,
        south_west = false, north_west = false;

    if (!no_head_connect)
    {
        if (h_pos.x == t_pos.x)
        {
            if (h_pos.y < t_pos.y)
                north = true;
            else
                south = true;
        }
        else if (h_pos.y == t_pos.y)
        {
            if (h_pos.x < t_pos.x)
                west = true;
            else
                east = true;
        }
        else if (h_pos.x < t_pos.x)
        {
            if (h_pos.y < t_pos.y)
                north_west = true;
            else
                south_west = true;
        }
        else if (h_pos.x > t_pos.x)
        {
            if (h_pos.y < t_pos.y)
                north_east = true;
            else
                south_east = true;
        }
    }

    // Tentacle only requires checking of head position.
    if (mons_is_tentacle(mon.type))
    {
        if (no_head_connect)
        {
            tileidx_t tile;
            switch (mon.type)
            {
                case MONS_KRAKEN_TENTACLE: tile = TILEP_MONS_KRAKEN_TENTACLE_WATER; break;
                case MONS_STARSPAWN_TENTACLE: tile = TILEP_MONS_STARSPAWN_TENTACLE_S; break;
                case MONS_ELDRITCH_TENTACLE: tile = TILEP_MONS_ELDRITCH_TENTACLE_PORTAL; break;
                case MONS_SNAPLASHER_VINE: tile = TILEP_MONS_VINE_S; break;
                default: die("bad tentacle type");
            }

            bool vary = !(mon.props.exists(FAKE_MON_KEY) && mon.props[FAKE_MON_KEY].get_bool());
            return vary ? _mon_random(tile, t_pos.y*GXM + t_pos.x) : tile;
        }

        // Different handling according to relative positions.
        if (north)
            return TILEP_MONS_KRAKEN_TENTACLE_N;
        if (south)
            return TILEP_MONS_KRAKEN_TENTACLE_S;
        if (west)
            return TILEP_MONS_KRAKEN_TENTACLE_W;
        if (east)
            return TILEP_MONS_KRAKEN_TENTACLE_E;
        if (north_west)
            return TILEP_MONS_KRAKEN_TENTACLE_NW;
        if (south_west)
            return TILEP_MONS_KRAKEN_TENTACLE_SW;
        if (north_east)
            return TILEP_MONS_KRAKEN_TENTACLE_NE;
        if (south_east)
            return TILEP_MONS_KRAKEN_TENTACLE_SE;
        die("impossible kraken direction");
    }
    // Only tentacle segments from now on.
    ASSERT(mons_is_tentacle_segment(mon.type));

    // For segments, we also need the next segment (or end piece).
    coord_def n_pos;
    bool no_next_connect = !mon.props.exists(OUTWARDS_KEY);
    if (!no_next_connect)
        n_pos = t_pos + mon.props[OUTWARDS_KEY].get_coord();

    if (no_head_connect && no_next_connect)
    {
        // Both head and next are submerged.
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_WATER;
    }

    if (!no_next_connect)
    {
        if (n_pos.x == t_pos.x)
        {
            if (n_pos.y < t_pos.y)
                north = true;
            else
                south = true;
        }
        else if (n_pos.y == t_pos.y)
        {
            if (n_pos.x < t_pos.x)
                west = true;
            else
                east = true;
        }
        else if (n_pos.x < t_pos.x)
        {
            if (n_pos.y < t_pos.y)
                north_west = true;
            else
                south_west = true;
        }
        else if (n_pos.x > t_pos.x)
        {
            if (n_pos.y < t_pos.y)
                north_east = true;
            else
                south_east = true;
        }
    }

    if (no_head_connect || no_next_connect)
    {
        // One segment end goes into water, the other
        // into the direction of head or next.

        if (north)
            return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N;
        if (south)
            return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S;
        if (west)
            return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W;
        if (east)
            return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E;
        if (north_west)
            return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NW;
        if (south_west)
            return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_SW;
        if (north_east)
            return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE;
        if (south_east)
            return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_SE;
        die("impossible kraken direction");
    }

    // Okay, neither head nor next are submerged.
    // Compare all three positions.

    // Straight lines first: Vertical.
    if (north && south)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_S;
    // Horizontal.
    if (east && west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_W;
    // Diagonals.
    if (north_west && south_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NW_SE;
    if (north_east && south_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE_SW;

    // Curved segments.
    if (east && north)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_N;
    if (east && south)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_S;
    if (south && west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_W;
    if (north && west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_W;
    if (north_east && north_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE_NW;
    if (south_east && south_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_SE_SW;
    if (north_west && south_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NW_SW;
    if (north_east && south_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE_SE;

    // Connect corners and edges.
    if (north && south_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_SW;
    if (north && south_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_SE;
    if (south && north_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_NW;
    if (south && north_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_NE;
    if (west && north_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_NE;
    if (west && south_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_SE;
    if (east && north_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_NW;
    if (east && south_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_SW;

    // Connections at acute angles; can currently only happen for vines.
    if (north && north_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_NW;
    if (north && north_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_NE;
    if (east && north_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_NE;
    if (east && south_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_SE;
    if (south && south_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_SE;
    if (south && south_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_SW;
    if (west && south_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_SW;
    if (west && north_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_NW;

    return TILEP_MONS_PROGRAM_BUG;
}

#ifdef USE_TILE
tileidx_t tileidx_out_of_bounds(int branch)
{
    if (branch == BRANCH_SHOALS)
        return TILE_DNGN_OPEN_SEA | TILE_FLAG_UNSEEN;
    else
        return TILE_DNGN_UNSEEN | TILE_FLAG_UNSEEN;
}

void tileidx_out_of_los(tileidx_t *fg, tileidx_t *bg, tileidx_t *cloud, const coord_def& gc)
{
    // Player memory.
    tileidx_t mem_fg = tile_env.bk_fg(gc);
    tileidx_t mem_bg = tile_env.bk_bg(gc);
    tileidx_t mem_cloud = tile_env.bk_cloud(gc);

    // Detected info is just stored in map_knowledge and doesn't get
    // written to what the player remembers. We'll feather that in here.

    // save any rays, which will get overwritten by mapped terrain
    auto rays = *bg & (TILE_FLAG_RAY_MULTI | TILE_FLAG_RAY_OOR | TILE_FLAG_RAY
                        | TILE_FLAG_LANDING);

    const map_cell &cell = env.map_knowledge(gc);

    // Override terrain for magic mapping.
    if (!cell.seen() && env.map_knowledge(gc).mapped())
        *bg = tileidx_feature_base(cell.feat());
    else
        *bg = mem_bg;
    *bg |= tileidx_unseen_flag(gc);

    // if out-of-los rays are getting shown that shouldn't be, the bug isn't
    // here -- fix it in the targeter
    *bg |= rays;

    // Override foreground for monsters/items
    if (env.map_knowledge(gc).detected_monster())
    {
        ASSERT(cell.monster() == MONS_SENSED);
        *fg = tileidx_monster_base(cell.monsterinfo()->base_type, 0);
    }
    else if (env.map_knowledge(gc).detected_item())
        *fg = tileidx_item(*cell.item());
    else
        *fg = mem_fg;

    *cloud = mem_cloud;
}

static tileidx_t _zombie_tile_to_spectral(const tileidx_t z_tile)
{
    switch (z_tile)
    {
    case TILEP_MONS_ZOMBIE_SMALL:
    case TILEP_MONS_ZOMBIE_SPRIGGAN:
    case TILEP_MONS_ZOMBIE_GOBLIN:
    case TILEP_MONS_ZOMBIE_HOBGOBLIN:
    case TILEP_MONS_ZOMBIE_GNOLL:
    case TILEP_MONS_ZOMBIE_KOBOLD:
    case TILEP_MONS_ZOMBIE_ORC:
    case TILEP_MONS_ZOMBIE_HUMAN:
    case TILEP_MONS_ZOMBIE_DRACONIAN:
    case TILEP_MONS_ZOMBIE_ELF:
    case TILEP_MONS_ZOMBIE_FAUN:
    case TILEP_MONS_ZOMBIE_MERFOLK:
    case TILEP_MONS_ZOMBIE_MINOTAUR:
    case TILEP_MONS_ZOMBIE_MONKEY:
        return TILEP_MONS_SPECTRAL_SMALL;
    case TILEP_MONS_ZOMBIE_LARGE:
    case TILEP_MONS_ZOMBIE_OGRE:
    case TILEP_MONS_ZOMBIE_TROLL:
    case TILEP_MONS_ZOMBIE_JUGGERNAUT:
    case TILEP_MONS_ZOMBIE_UGLY_THING:
        return TILEP_MONS_SPECTRAL_LARGE;
    case TILEP_MONS_ZOMBIE_QUADRUPED_SMALL:
    case TILEP_MONS_ZOMBIE_RAT:
    case TILEP_MONS_ZOMBIE_QUOKKA:
    case TILEP_MONS_ZOMBIE_JACKAL:
    case TILEP_MONS_ZOMBIE_HOUND:
    case TILEP_MONS_ZOMBIE_CRAB:
    case TILEP_MONS_ZOMBIE_TURTLE:
    case TILEP_MONS_ZOMBIE_BEAR:
        return TILEP_MONS_SPECTRAL_QUADRUPED_SMALL;
    case TILEP_MONS_ZOMBIE_QUADRUPED_LARGE:
    case TILEP_MONS_ZOMBIE_ELEPHANT:
    case TILEP_MONS_ZOMBIE_YAK:
    case TILEP_MONS_ZOMBIE_QUADRUPED_WINGED:
        return TILEP_MONS_SPECTRAL_QUADRUPED_LARGE;
    case TILEP_MONS_ZOMBIE_FROG:
        return TILEP_MONS_SPECTRAL_FROG;
    case TILEP_MONS_ZOMBIE_BAT:
    case TILEP_MONS_ZOMBIE_BIRD: /* no bird spectral tile */
    case TILEP_MONS_ZOMBIE_HARPY:
        return TILEP_MONS_SPECTRAL_BAT;
    case TILEP_MONS_ZOMBIE_BEE:
    case TILEP_MONS_ZOMBIE_MELIAI:
    case TILEP_MONS_ZOMBIE_HORNET:
        return TILEP_MONS_SPECTRAL_BEE;
    case TILEP_MONS_ZOMBIE_BEETLE:
    case TILEP_MONS_ZOMBIE_ROACH:
    case TILEP_MONS_ZOMBIE_BUG:
        return TILEP_MONS_SPECTRAL_BUG;
    case TILEP_MONS_ZOMBIE_FISH:
    case TILEP_MONS_ZOMBIE_SKYSHARK:
        return TILEP_MONS_SPECTRAL_FISH;
    case TILEP_MONS_ZOMBIE_CENTAUR:
    case TILEP_MONS_ZOMBIE_YAKTAUR:
        return TILEP_MONS_SPECTRAL_CENTAUR;
    case TILEP_MONS_ZOMBIE_NAGA:
    case TILEP_MONS_ZOMBIE_GUARDIAN_SERPENT:
    case TILEP_MONS_ZOMBIE_SALAMANDER:
        return TILEP_MONS_SPECTRAL_NAGA;
    case TILEP_MONS_ZOMBIE_SNAKE:
    case TILEP_MONS_ZOMBIE_ADDER:
    case TILEP_MONS_ZOMBIE_WORM:
    case TILEP_MONS_ZOMBIE_LINDWURM:
    case TILEP_MONS_ZOMBIE_SNAIL:
        return TILEP_MONS_SPECTRAL_SNAKE;
    case TILEP_MONS_ZOMBIE_LIZARD:
        return TILEP_MONS_SPECTRAL_LIZARD;
    case TILEP_MONS_ZOMBIE_SCORPION:
    case TILEP_MONS_ZOMBIE_SPIDER_LARGE:
    case TILEP_MONS_ZOMBIE_SPIDER_SMALL:
        return TILEP_MONS_SPECTRAL_SPIDER;
    case TILEP_MONS_ZOMBIE_DRAGON:
    case TILEP_MONS_ZOMBIE_IRON_DRAGON:
    case TILEP_MONS_ZOMBIE_GOLDEN_DRAGON:
    case TILEP_MONS_ZOMBIE_QUICKSILVER_DRAGON:
        return TILEP_MONS_SPECTRAL_DRAGON;
    case TILEP_MONS_ZOMBIE_DRAKE:
    case TILEP_MONS_ZOMBIE_WYVERN:
        return TILEP_MONS_SPECTRAL_DRAKE;
    case TILEP_MONS_ZOMBIE_KRAKEN:
        return TILEP_MONS_SPECTRAL_KRAKEN;
    default:
        if (tile_player_basetile(z_tile) == TILEP_MONS_ZOMBIE_HYDRA)
        {
            return TILEP_MONS_SPECTRAL_HYDRA
                   + (z_tile - TILEP_MONS_ZOMBIE_HYDRA);
        }
    }
    return TILEP_MONS_SPECTRAL_SMALL;
}

static tileidx_t _zombie_tile_to_bound_soul(const tileidx_t z_tile)
{
    switch (z_tile)
    {
    case TILEP_MONS_ZOMBIE_SMALL:
    case TILEP_MONS_ZOMBIE_SPRIGGAN:
    case TILEP_MONS_ZOMBIE_GOBLIN:
    case TILEP_MONS_ZOMBIE_HOBGOBLIN:
    case TILEP_MONS_ZOMBIE_GNOLL:
    case TILEP_MONS_ZOMBIE_KOBOLD:
    case TILEP_MONS_ZOMBIE_ORC:
    case TILEP_MONS_ZOMBIE_HUMAN:
    case TILEP_MONS_ZOMBIE_DRACONIAN:
    case TILEP_MONS_ZOMBIE_ELF:
    case TILEP_MONS_ZOMBIE_FAUN:
    case TILEP_MONS_ZOMBIE_MERFOLK:
    case TILEP_MONS_ZOMBIE_MINOTAUR:
    case TILEP_MONS_ZOMBIE_MONKEY:
    case TILEP_MONS_ZOMBIE_LARGE:
    case TILEP_MONS_ZOMBIE_OGRE:
    case TILEP_MONS_ZOMBIE_TROLL:
    case TILEP_MONS_ZOMBIE_JUGGERNAUT:
    case TILEP_MONS_ZOMBIE_CENTAUR:
    case TILEP_MONS_ZOMBIE_YAKTAUR:
    case TILEP_MONS_ZOMBIE_NAGA:
    case TILEP_MONS_ZOMBIE_SALAMANDER:
    case TILEP_MONS_ZOMBIE_HARPY:
        return TILEP_MONS_BOUND_HUMANOID;
    case TILEP_MONS_ZOMBIE_QUADRUPED_SMALL:
    case TILEP_MONS_ZOMBIE_RAT:
    case TILEP_MONS_ZOMBIE_QUOKKA:
    case TILEP_MONS_ZOMBIE_JACKAL:
    case TILEP_MONS_ZOMBIE_HOUND:
    case TILEP_MONS_ZOMBIE_CRAB:
    case TILEP_MONS_ZOMBIE_TURTLE:
    case TILEP_MONS_ZOMBIE_BEAR:
    case TILEP_MONS_ZOMBIE_DREAM_SHEEP:
    case TILEP_MONS_ZOMBIE_QUADRUPED_LARGE:
    case TILEP_MONS_ZOMBIE_ELEPHANT:
    case TILEP_MONS_ZOMBIE_YAK:
    case TILEP_MONS_ZOMBIE_QUADRUPED_WINGED:
    case TILEP_MONS_ZOMBIE_FROG:
    case TILEP_MONS_ZOMBIE_BAT:
    case TILEP_MONS_ZOMBIE_BIRD:
    case TILEP_MONS_ZOMBIE_FISH:
    case TILEP_MONS_ZOMBIE_SKYSHARK:
    case TILEP_MONS_ZOMBIE_GUARDIAN_SERPENT:
    case TILEP_MONS_ZOMBIE_SNAKE:
    case TILEP_MONS_ZOMBIE_ADDER:
    case TILEP_MONS_ZOMBIE_LINDWURM:
    case TILEP_MONS_ZOMBIE_LIZARD:
    case TILEP_MONS_ZOMBIE_DRAGON:
    case TILEP_MONS_ZOMBIE_IRON_DRAGON:
    case TILEP_MONS_ZOMBIE_GOLDEN_DRAGON:
    case TILEP_MONS_ZOMBIE_QUICKSILVER_DRAGON:
    case TILEP_MONS_ZOMBIE_DRAKE:
    case TILEP_MONS_ZOMBIE_WYVERN:
    case TILEP_MONS_ZOMBIE_KRAKEN:
    case TILEP_MONS_ZOMBIE_SNAIL:
        return TILEP_MONS_BOUND_ANIMAL;
    case TILEP_MONS_ZOMBIE_UGLY_THING:
    case TILEP_MONS_ZOMBIE_BEE:
    case TILEP_MONS_ZOMBIE_MELIAI:
    case TILEP_MONS_ZOMBIE_HORNET:
    case TILEP_MONS_ZOMBIE_BEETLE:
    case TILEP_MONS_ZOMBIE_ROACH:
    case TILEP_MONS_ZOMBIE_BUG:
    case TILEP_MONS_ZOMBIE_WORM:
    case TILEP_MONS_ZOMBIE_SCORPION:
    case TILEP_MONS_ZOMBIE_SPIDER_LARGE:
    case TILEP_MONS_ZOMBIE_SPIDER_SMALL:
        return TILEP_MONS_BOUND_STRANGE;
    default:
        if (tile_player_basetile(z_tile) == TILEP_MONS_ZOMBIE_HYDRA)
            return TILEP_MONS_BOUND_ANIMAL;
    }
    return TILEP_MONS_BOUND_HUMANOID;
}


static tileidx_t _zombie_tile_to_simulacrum(const tileidx_t z_tile)
{
    switch (z_tile)
    {
    case TILEP_MONS_ZOMBIE_SMALL:
    case TILEP_MONS_ZOMBIE_SPRIGGAN:
    case TILEP_MONS_ZOMBIE_GOBLIN:
    case TILEP_MONS_ZOMBIE_HOBGOBLIN:
    case TILEP_MONS_ZOMBIE_GNOLL:
    case TILEP_MONS_ZOMBIE_KOBOLD:
    case TILEP_MONS_ZOMBIE_ORC:
    case TILEP_MONS_ZOMBIE_HUMAN:
    case TILEP_MONS_ZOMBIE_DRACONIAN:
    case TILEP_MONS_ZOMBIE_ELF:
    case TILEP_MONS_ZOMBIE_FAUN:
    case TILEP_MONS_ZOMBIE_MERFOLK:
    case TILEP_MONS_ZOMBIE_MINOTAUR:
    case TILEP_MONS_ZOMBIE_MONKEY:
        return TILEP_MONS_SIMULACRUM_SMALL;
    case TILEP_MONS_ZOMBIE_LARGE:
    case TILEP_MONS_ZOMBIE_OGRE:
    case TILEP_MONS_ZOMBIE_TROLL:
    case TILEP_MONS_ZOMBIE_JUGGERNAUT:
    case TILEP_MONS_ZOMBIE_UGLY_THING:
        return TILEP_MONS_SIMULACRUM_LARGE;
    case TILEP_MONS_ZOMBIE_QUADRUPED_SMALL:
    case TILEP_MONS_ZOMBIE_BEAR:
    case TILEP_MONS_ZOMBIE_DREAM_SHEEP:
    case TILEP_MONS_ZOMBIE_RAT:
    case TILEP_MONS_ZOMBIE_QUOKKA:
    case TILEP_MONS_ZOMBIE_JACKAL:
    case TILEP_MONS_ZOMBIE_HOUND:
    case TILEP_MONS_ZOMBIE_CRAB:
    case TILEP_MONS_ZOMBIE_TURTLE:
        return TILEP_MONS_SIMULACRUM_QUADRUPED_SMALL;
    case TILEP_MONS_ZOMBIE_QUADRUPED_LARGE:
    case TILEP_MONS_ZOMBIE_FROG:
    case TILEP_MONS_ZOMBIE_ELEPHANT:
    case TILEP_MONS_ZOMBIE_YAK:
    case TILEP_MONS_ZOMBIE_QUADRUPED_WINGED:
        return TILEP_MONS_SIMULACRUM_QUADRUPED_LARGE;
    case TILEP_MONS_ZOMBIE_BAT:
    case TILEP_MONS_ZOMBIE_HARPY: // much less dangerous than shrikes
        return TILEP_MONS_SIMULACRUM_BAT;
    case TILEP_MONS_ZOMBIE_BIRD:
        return TILEP_MONS_SIMULACRUM_SHRIKE;
    case TILEP_MONS_ZOMBIE_BEE:
    case TILEP_MONS_ZOMBIE_MELIAI:
    case TILEP_MONS_ZOMBIE_HORNET:
        return TILEP_MONS_SIMULACRUM_BEE;
    case TILEP_MONS_ZOMBIE_BEETLE:
    case TILEP_MONS_ZOMBIE_ROACH:
    case TILEP_MONS_ZOMBIE_BUG:
        return TILEP_MONS_SIMULACRUM_BUG;
    case TILEP_MONS_ZOMBIE_FISH:
    case TILEP_MONS_ZOMBIE_SKYSHARK:
        return TILEP_MONS_SIMULACRUM_FISH;
    case TILEP_MONS_ZOMBIE_CENTAUR:
    case TILEP_MONS_ZOMBIE_YAKTAUR:
        return TILEP_MONS_SIMULACRUM_CENTAUR;
    case TILEP_MONS_ZOMBIE_NAGA:
    case TILEP_MONS_ZOMBIE_GUARDIAN_SERPENT:
    case TILEP_MONS_ZOMBIE_SALAMANDER:
        return TILEP_MONS_SIMULACRUM_NAGA;
    case TILEP_MONS_ZOMBIE_SNAKE:
    case TILEP_MONS_ZOMBIE_ADDER:
    case TILEP_MONS_ZOMBIE_WORM:
    case TILEP_MONS_ZOMBIE_LINDWURM:
    case TILEP_MONS_ZOMBIE_SNAIL:
        return TILEP_MONS_SIMULACRUM_SNAKE;
    case TILEP_MONS_ZOMBIE_LIZARD:
        return TILEP_MONS_SIMULACRUM_LIZARD;
    case TILEP_MONS_ZOMBIE_SCORPION:
    case TILEP_MONS_ZOMBIE_SPIDER_LARGE:
    case TILEP_MONS_ZOMBIE_SPIDER_SMALL:
        return TILEP_MONS_SIMULACRUM_SPIDER;
    case TILEP_MONS_ZOMBIE_DRAGON:
    case TILEP_MONS_ZOMBIE_IRON_DRAGON:
    case TILEP_MONS_ZOMBIE_GOLDEN_DRAGON:
    case TILEP_MONS_ZOMBIE_QUICKSILVER_DRAGON:
        return TILEP_MONS_SIMULACRUM_DRAGON;
    case TILEP_MONS_ZOMBIE_DRAKE:
    case TILEP_MONS_ZOMBIE_WYVERN:
        return TILEP_MONS_SIMULACRUM_DRAKE;
    case TILEP_MONS_ZOMBIE_KRAKEN:
        return TILEP_MONS_SIMULACRUM_KRAKEN;
    case TILEP_MONS_ZOMBIE_JELLY:
        return TILEP_MONS_SIMULACRUM_SLIME;
    case TILEP_MONS_ZOMBIE_ORB:
        return TILEP_MONS_SIMULACRUM_EYE;
    case TILEP_MONS_ZOMBIE_X:
        return TILEP_MONS_SIMULACRUM_X;
    default:
        if (tile_player_basetile(z_tile) == TILEP_MONS_ZOMBIE_HYDRA)
        {
            return TILEP_MONS_SIMULACRUM_HYDRA
                   + (z_tile - TILEP_MONS_ZOMBIE_HYDRA);
        }
    }
    return TILEP_MONS_SIMULACRUM_SMALL;
}

static tileidx_t _zombie_tile_to_skeleton(const tileidx_t z_tile)
{
    switch (z_tile)
    {
    case TILEP_MONS_ZOMBIE_SMALL:
    case TILEP_MONS_ZOMBIE_SPRIGGAN:
    case TILEP_MONS_ZOMBIE_GOBLIN:
    case TILEP_MONS_ZOMBIE_KOBOLD:
    case TILEP_MONS_ZOMBIE_MONKEY:
        return TILEP_MONS_SKELETON_SMALL;
    case TILEP_MONS_ZOMBIE_HOBGOBLIN:
    case TILEP_MONS_ZOMBIE_GNOLL:
    case TILEP_MONS_ZOMBIE_ORC:
    case TILEP_MONS_ZOMBIE_HUMAN:
    case TILEP_MONS_ZOMBIE_ELF:
    case TILEP_MONS_ZOMBIE_MERFOLK:
    case TILEP_MONS_ZOMBIE_MINOTAUR:
    case TILEP_MONS_ZOMBIE_FAUN:
        return TILEP_MONS_SKELETON_MEDIUM;
    case TILEP_MONS_ZOMBIE_TROLL:
        return TILEP_MONS_SKELETON_TROLL;
    case TILEP_MONS_ZOMBIE_LARGE:
    case TILEP_MONS_ZOMBIE_OGRE:
        return TILEP_MONS_SKELETON_LARGE;
    case TILEP_MONS_ZOMBIE_JUGGERNAUT:
        return TILEP_MONS_SKELETON_JUGGERNAUT;
    case TILEP_MONS_ZOMBIE_QUADRUPED_SMALL:
    case TILEP_MONS_ZOMBIE_RAT:
    case TILEP_MONS_ZOMBIE_QUOKKA:
    case TILEP_MONS_ZOMBIE_JACKAL:
    case TILEP_MONS_ZOMBIE_HOUND:
    case TILEP_MONS_ZOMBIE_BEETLE:
    case TILEP_MONS_ZOMBIE_ROACH:
    case TILEP_MONS_ZOMBIE_BEAR:
    case TILEP_MONS_ZOMBIE_DREAM_SHEEP:
    case TILEP_MONS_ZOMBIE_BUG:
        return TILEP_MONS_SKELETON_QUADRUPED_SMALL;
    case TILEP_MONS_ZOMBIE_LIZARD:
    case TILEP_MONS_ZOMBIE_CRAB:
        return TILEP_MONS_SKELETON_LIZARD;
    case TILEP_MONS_ZOMBIE_TURTLE:
        return TILEP_MONS_SKELETON_TURTLE;
    case TILEP_MONS_ZOMBIE_QUADRUPED_LARGE:
    case TILEP_MONS_ZOMBIE_ELEPHANT:
    case TILEP_MONS_ZOMBIE_YAK:
        return TILEP_MONS_SKELETON_QUADRUPED_LARGE;
    case TILEP_MONS_ZOMBIE_FROG:
        return TILEP_MONS_SKELETON_FROG;
    case TILEP_MONS_ZOMBIE_QUADRUPED_WINGED:
        return TILEP_MONS_SKELETON_QUADRUPED_WINGED;
    case TILEP_MONS_ZOMBIE_BAT:
        return TILEP_MONS_SKELETON_BAT;
    case TILEP_MONS_ZOMBIE_HARPY:
    case TILEP_MONS_ZOMBIE_BIRD:
        return TILEP_MONS_SKELETON_BIRD;
    case TILEP_MONS_ZOMBIE_FISH:
    case TILEP_MONS_ZOMBIE_SKYSHARK:
        return TILEP_MONS_SKELETON_FISH;
    case TILEP_MONS_ZOMBIE_CENTAUR:
    case TILEP_MONS_ZOMBIE_YAKTAUR:
        return TILEP_MONS_SKELETON_CENTAUR;
    case TILEP_MONS_ZOMBIE_NAGA:
    case TILEP_MONS_ZOMBIE_GUARDIAN_SERPENT:
    case TILEP_MONS_ZOMBIE_SALAMANDER:
        return TILEP_MONS_SKELETON_NAGA;
    case TILEP_MONS_ZOMBIE_SNAKE:
    case TILEP_MONS_ZOMBIE_ADDER:
    case TILEP_MONS_ZOMBIE_WORM:
    case TILEP_MONS_ZOMBIE_LINDWURM:
    case TILEP_MONS_ZOMBIE_SNAIL: // but they have no skeletons...
        return TILEP_MONS_SKELETON_SNAKE;
    case TILEP_MONS_ZOMBIE_DRAGON:
    case TILEP_MONS_ZOMBIE_IRON_DRAGON:
    case TILEP_MONS_ZOMBIE_GOLDEN_DRAGON:
    case TILEP_MONS_ZOMBIE_QUICKSILVER_DRAGON:
        return TILEP_MONS_SKELETON_DRAGON;
    case TILEP_MONS_ZOMBIE_DRAKE:
    case TILEP_MONS_ZOMBIE_WYVERN:
        return TILEP_MONS_SKELETON_DRAKE;
    case TILEP_MONS_ZOMBIE_UGLY_THING:
        return TILEP_MONS_SKELETON_UGLY_THING;
    case TILEP_MONS_ZOMBIE_DRACONIAN:
        return TILEP_MONS_SKELETON_DRACONIAN;
    default:
        if (tile_player_basetile(z_tile) == TILEP_MONS_ZOMBIE_HYDRA)
        {
            return TILEP_MONS_SKELETON_HYDRA
                   + (z_tile - TILEP_MONS_ZOMBIE_HYDRA);
        }
        if (tile_player_basetile(z_tile) == TILEP_MONS_LERNAEAN_HYDRA_ZOMBIE)
        {
            return TILEP_MONS_LERNAEAN_HYDRA_SKELETON
                   + (z_tile - TILEP_MONS_LERNAEAN_HYDRA_ZOMBIE);
        }
    }
    return TILEP_MONS_SKELETON_SMALL;
}

/**
 * For a given monster, what tile is appropriate for that monster if it's a
 * zombie?
 *
 * If it's another kind of derived undead (e.g. a skeleton), the actual tile to
 * be used will be derived from the zombie tile we return here.
 *
 * @param mon   The monster in question.
 * @return      An appropriate zombie tile; e.g. TILEP_MONS_ZOMBIE_DRAGON.
 */
static tileidx_t _mon_to_zombie_tile(const monster_info &mon)
{
    const monster_type subtype = mon.base_type;

    // hydras get special casing

    if (subtype == MONS_LERNAEAN_HYDRA && (mon.type == MONS_ZOMBIE
                                           || mon.type == MONS_SKELETON))
    {
        // Step down the number of heads to get the appropriate tile:
        // for the last five heads, use tiles 1-5, for greater amounts
        // use the next tile for every 5 more heads.
        return tileidx_mon_clamp(TILEP_MONS_LERNAEAN_HYDRA_ZOMBIE,
                                 mon.number <= 5 ?
                                 mon.number - 1 :
                                 4 + (mon.number - 1)/5);
    }
    if (mons_genus(subtype) == MONS_HYDRA)
        return TILEP_MONS_ZOMBIE_HYDRA + min(mon.num_heads, 5) - 1;

    // specific per-species zombies - use to override genuses
    static const map<monster_type, tileidx_t> species_tiles = {
        { MONS_JUGGERNAUT,              TILEP_MONS_ZOMBIE_JUGGERNAUT },
        { MONS_ACID_DRAGON,             TILEP_MONS_ZOMBIE_DRAKE },
        { MONS_STEAM_DRAGON,            TILEP_MONS_ZOMBIE_DRAKE },
        { MONS_JACKAL,                  TILEP_MONS_ZOMBIE_JACKAL },
        { MONS_ADDER,                   TILEP_MONS_ZOMBIE_ADDER },
        { MONS_WOLF_SPIDER,             TILEP_MONS_ZOMBIE_SPIDER_LARGE },
        { MONS_EMPEROR_SCORPION,        TILEP_MONS_ZOMBIE_SPIDER_LARGE },
        { MONS_HOWLER_MONKEY,           TILEP_MONS_ZOMBIE_MONKEY },
        { MONS_IRON_DRAGON,             TILEP_MONS_ZOMBIE_IRON_DRAGON },
        { MONS_GOLDEN_DRAGON,           TILEP_MONS_ZOMBIE_GOLDEN_DRAGON },
        { MONS_QUICKSILVER_DRAGON,      TILEP_MONS_ZOMBIE_QUICKSILVER_DRAGON },
        { MONS_LINDWURM,                TILEP_MONS_ZOMBIE_LINDWURM, },
        { MONS_MELIAI,                  TILEP_MONS_ZOMBIE_MELIAI, },
        { MONS_HORNET,                  TILEP_MONS_ZOMBIE_HORNET, },
        { MONS_SKYSHARK,                TILEP_MONS_ZOMBIE_SKYSHARK, },
        { MONS_DREAM_SHEEP,             TILEP_MONS_ZOMBIE_DREAM_SHEEP, },
    };
    // per-genus zombies - use by default
    static const map<monster_type, tileidx_t> genus_tiles = {
        { MONS_GOBLIN,                  TILEP_MONS_ZOMBIE_GOBLIN },
        { MONS_HOBGOBLIN,               TILEP_MONS_ZOMBIE_HOBGOBLIN },
        { MONS_GNOLL,                   TILEP_MONS_ZOMBIE_GNOLL },
        { MONS_HUMAN,                   TILEP_MONS_ZOMBIE_HUMAN },
        { MONS_GHOUL,                   TILEP_MONS_ZOMBIE_HUMAN }, // for skel
        { MONS_KOBOLD,                  TILEP_MONS_ZOMBIE_KOBOLD },
        { MONS_ORC,                     TILEP_MONS_ZOMBIE_ORC },
        { MONS_TROLL,                   TILEP_MONS_ZOMBIE_TROLL },
        { MONS_OGRE,                    TILEP_MONS_ZOMBIE_OGRE },
        { MONS_HARPY,                   TILEP_MONS_ZOMBIE_HARPY },
        { MONS_DRACONIAN,               TILEP_MONS_ZOMBIE_DRACONIAN },
        { MONS_DRAGON,                  TILEP_MONS_ZOMBIE_DRAGON },
        { MONS_WYVERN,                  TILEP_MONS_ZOMBIE_WYVERN },
        { MONS_DRAKE,                   TILEP_MONS_ZOMBIE_DRAKE },
        { MONS_GIANT_LIZARD,            TILEP_MONS_ZOMBIE_LIZARD },
        { MONS_CROCODILE,               TILEP_MONS_ZOMBIE_LIZARD },
        { MONS_RAT,                     TILEP_MONS_ZOMBIE_RAT },
        { MONS_QUOKKA,                  TILEP_MONS_ZOMBIE_QUOKKA },
        { MONS_HOUND,                   TILEP_MONS_ZOMBIE_HOUND },
        { MONS_FROG,                    TILEP_MONS_ZOMBIE_FROG },
        { MONS_CRAB,                    TILEP_MONS_ZOMBIE_CRAB },
        { MONS_SNAPPING_TURTLE,         TILEP_MONS_ZOMBIE_TURTLE },
        { MONS_RIBBON_WORM,             TILEP_MONS_ZOMBIE_WORM },
        { MONS_GIANT_COCKROACH,         TILEP_MONS_ZOMBIE_ROACH },
        { MONS_SCORPION,                TILEP_MONS_ZOMBIE_SCORPION },
        { MONS_KRAKEN,                  TILEP_MONS_ZOMBIE_KRAKEN },
        { MONS_OCTOPODE,                TILEP_MONS_ZOMBIE_OCTOPODE },
        { MONS_UGLY_THING,              TILEP_MONS_ZOMBIE_UGLY_THING },
        { MONS_ELEPHANT,                TILEP_MONS_ZOMBIE_ELEPHANT },
        { MONS_ELF,                     TILEP_MONS_ZOMBIE_ELF },
        { MONS_FAUN,                    TILEP_MONS_ZOMBIE_FAUN },
        { MONS_SATYR,                   TILEP_MONS_ZOMBIE_FAUN },
        { MONS_GUARDIAN_SERPENT,        TILEP_MONS_ZOMBIE_GUARDIAN_SERPENT },
        { MONS_MERFOLK,                 TILEP_MONS_ZOMBIE_MERFOLK },
        { MONS_MINOTAUR,                TILEP_MONS_ZOMBIE_MINOTAUR },
        { MONS_SALAMANDER,              TILEP_MONS_ZOMBIE_SALAMANDER },
        { MONS_SPRIGGAN,                TILEP_MONS_ZOMBIE_SPRIGGAN },
        { MONS_YAKTAUR,                 TILEP_MONS_ZOMBIE_YAKTAUR },
        { MONS_YAK,                     TILEP_MONS_ZOMBIE_YAK },
        { MONS_BEAR,                    TILEP_MONS_ZOMBIE_BEAR },
        { MONS_ELEPHANT_SLUG,           TILEP_MONS_ZOMBIE_SNAIL },
    };

    struct shape_size_tiles
    {
        tileidx_t small; ///< Z_SMALL and default tile
        tileidx_t big;   ///< Z_BIG tile
    };
    const shape_size_tiles GENERIC_ZOMBIES = { TILEP_MONS_ZOMBIE_SMALL,
                                               TILEP_MONS_ZOMBIE_LARGE };
    static const map<mon_body_shape, shape_size_tiles> shape_tiles = {
        { MON_SHAPE_CENTAUR,            {TILEP_MONS_ZOMBIE_CENTAUR} },
        { MON_SHAPE_NAGA,               {TILEP_MONS_ZOMBIE_NAGA} },
        { MON_SHAPE_QUADRUPED_WINGED,   {TILEP_MONS_ZOMBIE_QUADRUPED_WINGED} },
        { MON_SHAPE_BAT,                {TILEP_MONS_ZOMBIE_BAT} },
        { MON_SHAPE_BIRD,               {TILEP_MONS_ZOMBIE_BIRD} },
        { MON_SHAPE_SNAKE,              {TILEP_MONS_ZOMBIE_SNAKE} },
        { MON_SHAPE_SNAIL,              {TILEP_MONS_ZOMBIE_SNAKE} },
        { MON_SHAPE_FISH,               {TILEP_MONS_ZOMBIE_FISH} },
        { MON_SHAPE_INSECT,             {TILEP_MONS_ZOMBIE_BUG} },
        { MON_SHAPE_CENTIPEDE,          {TILEP_MONS_ZOMBIE_BUG} },
        { MON_SHAPE_INSECT_WINGED,      {TILEP_MONS_ZOMBIE_BEE} },
        { MON_SHAPE_ARACHNID,           {TILEP_MONS_ZOMBIE_SPIDER_SMALL} },
        { MON_SHAPE_QUADRUPED_TAILLESS, {TILEP_MONS_ZOMBIE_QUADRUPED_SMALL,
                                         TILEP_MONS_ZOMBIE_QUADRUPED_LARGE} },
        { MON_SHAPE_QUADRUPED,          {TILEP_MONS_ZOMBIE_QUADRUPED_SMALL,
                                         TILEP_MONS_ZOMBIE_QUADRUPED_LARGE} },
        { MON_SHAPE_BLOB,               {TILEP_MONS_ZOMBIE_JELLY}},
        { MON_SHAPE_ORB,                {TILEP_MONS_ZOMBIE_ORB}},
        { MON_SHAPE_MISC,               {TILEP_MONS_ZOMBIE_X}},
        { MON_SHAPE_HUMANOID,           GENERIC_ZOMBIES },
        { MON_SHAPE_HUMANOID_WINGED,    GENERIC_ZOMBIES },
        { MON_SHAPE_HUMANOID_TAILED,    GENERIC_ZOMBIES },
        { MON_SHAPE_HUMANOID_WINGED_TAILED,   GENERIC_ZOMBIES },
    };

    const tileidx_t *subtype_tile = map_find(species_tiles, subtype);
    if (subtype_tile)
        return *subtype_tile;

    const tileidx_t *genus_tile = map_find(genus_tiles, mons_genus(subtype));
    if (genus_tile)
        return *genus_tile;

    const int z_size = mons_zombie_size(subtype);
    const shape_size_tiles *shape_tile_pair
        = map_find(shape_tiles, get_mon_shape(subtype));
    if (shape_tile_pair)
    {
        if (z_size == Z_BIG && shape_tile_pair->big)
            return shape_tile_pair->big;
        return shape_tile_pair->small;
    }

    return TILEP_ERROR;
}

/// What tile should be used for a given derived undead monster?
static tileidx_t _tileidx_monster_zombified(const monster_info& mon)
{
    const tileidx_t zombie_tile = _mon_to_zombie_tile(mon);
    switch (mon.type)
    {
        case MONS_SKELETON:
            return _zombie_tile_to_skeleton(zombie_tile);
        case MONS_BOUND_SOUL:
            return _zombie_tile_to_bound_soul(zombie_tile);
        case MONS_SPECTRAL_THING:
            return _zombie_tile_to_spectral(zombie_tile);
        case MONS_SIMULACRUM:
            return _zombie_tile_to_simulacrum(zombie_tile);
        default:
            if (zombie_tile == TILEP_ERROR)
                return TILEP_MONS_ZOMBIE_LARGE; // XXX: assert..?
            return zombie_tile;
    }
}

// Special case for *taurs which have a different tile
// for when they have a bow.
static bool _bow_offset(const monster_info& mon)
{
    if (!mon.inv[MSLOT_WEAPON])
        return true;

    switch (mon.inv[MSLOT_WEAPON]->sub_type)
    {
    case WPN_SHORTBOW:
    case WPN_ORCBOW:
    case WPN_LONGBOW:
    case WPN_ARBALEST:
        return false;
    default:
        return true;
    }
}

static tileidx_t _mon_mod(tileidx_t tile, int offset)
{
    int count = tile_player_count(tile);
    return tile + offset % count;
}
#endif // USE_TILE

tileidx_t tileidx_mon_clamp(tileidx_t tile, int offset)
{
    int count = tile_player_count(tile);
    return tile + min(max(offset, 0), count - 1);
}

#ifdef USE_TILE
static tileidx_t _mon_cycle(tileidx_t tile, int offset)
{
    int count = tile_player_count(tile);
    return tile + ((offset + you.frame_no) % count);
}
#endif

static tileidx_t _modrng(int mod, tileidx_t first, tileidx_t last)
{
    return first + mod % (last - first + 1);
}

#ifdef USE_TILE
// This function allows for getting a monster from "just" the type.
// To avoid needless duplication of a cases in tileidx_monster, some
// extra parameters that have reasonable defaults for monsters where
// only the type is known are pushed here.
tileidx_t tileidx_monster_base(int type, int mon_id, bool in_water, int colour,
                               int number, int tile_num_prop, bool vary)
{
    switch (type)
    {
    case MONS_UGLY_THING:
    case MONS_VERY_UGLY_THING:
    {
        const tileidx_t ugly_tile = (type == MONS_VERY_UGLY_THING) ?
            TILEP_MONS_VERY_UGLY_THING : TILEP_MONS_UGLY_THING;
        int colour_offset = ugly_thing_colour_offset(colour);
        return tileidx_mon_clamp(ugly_tile, colour_offset);
    }

    case MONS_BLINK_FROG:
        if (!hash_with_seed(100, mon_id, you.frame_no))
            return TILEP_MONS_BLINK_FROG_BLINKING;
        break;

    case MONS_GLOBE_OF_ANNIHILATION:
    {
        // Max size isn't reached until distance 4 (distance 2-3 look the same)
        if (tile_num_prop == 3)
            tile_num_prop--;

        tileidx_t base_tile;
        switch (colour)
        {
            default:
            case LIGHTRED: base_tile = TILEP_MONS_GLOBE_OF_ANNIHILATION_GEH; break;
            case LIGHTBLUE: base_tile = TILEP_MONS_GLOBE_OF_ANNIHILATION_COC; break;
            case LIGHTGREY: base_tile = TILEP_MONS_GLOBE_OF_ANNIHILATION_DIS; break;
            case CYAN: base_tile = TILEP_MONS_GLOBE_OF_ANNIHILATION_TAR; break;
        }

        return tileidx_mon_clamp(base_tile, tile_num_prop - 1);
    }

    case MONS_HYDRA:
        // Number of heads
        return tileidx_mon_clamp(TILEP_MONS_HYDRA, number - 1);
    case MONS_SLIME_CREATURE:
    case MONS_MERGED_SLIME_CREATURE:
        return tileidx_mon_clamp(TILEP_MONS_SLIME_CREATURE, number - 1);
    case MONS_LERNAEAN_HYDRA:
        // Step down the number of heads to get the appropriate tile:
        // For the last five heads, use tiles 1-5, for greater amounts
        // use the next tile for every 5 more heads.
        return tileidx_mon_clamp(TILEP_MONS_LERNAEAN_HYDRA,
                                 number <= 5 ?
                                 number - 1 : 4 + (number - 1)/5);

    case MONS_SEISMOSAURUS_EGG:
        return tileidx_mon_clamp(TILEP_MONS_SEISMOSAURUS_EGG, number);

    // draconian ('d')
    case MONS_TIAMAT:
    {
        int offset = 0;
        switch (colour)
        {
        case LIGHTRED:      offset = 0; break;
        case WHITE:         offset = 1; break;
        case BLUE:          offset = 2; break; // black
        case GREEN:         offset = 3; break;
        case MAGENTA:       offset = 4; break;
        case YELLOW:        offset = 5; break;
        }

        return TILEP_MONS_TIAMAT + offset;
    }
    }

    const monster_type mtype = static_cast<monster_type>(type);
    const tileidx_t base_tile = get_mon_base_tile(mtype);
    const mon_type_tile_variation vary_type = vary ? get_mon_tile_variation(mtype) : TVARY_NONE;
    switch (vary_type)
    {
    case TVARY_NONE:
        return base_tile;
    case TVARY_MOD:
        return _mon_mod(base_tile, tile_num_prop);
    case TVARY_CYCLE:
        return _mon_cycle(base_tile, tile_num_prop);
    case TVARY_RANDOM:
        return _mon_random(base_tile, mon_id);
    case TVARY_WATER:
        return base_tile + (in_water ? 1 : 0);
    default:
        die("Unknown tile variation type %d for mon %d!", vary_type, mtype);
    }
}

enum class main_dir
{
    north = 0,
    east,
    south,
    west
};

enum class tentacle_type
{
    kraken = 0,
    eldritch = 1,
    starspawn = 2,
    vine = 3,
    zombie_kraken = 4,
    simulacrum_kraken = 5,
    spectral_kraken = 6,
};

static void _add_tentacle_overlay(const coord_def pos,
                                  const main_dir dir,
                                  tentacle_type type)
{
    /* This adds the corner overlays; e.g. in the following situation:
         .#
         #.
        we need to add corners to the floor tiles since the tentacle
        will otherwise look weird. So when placing the upper tentacle
        tile, this function is called with dir main_dir::west, so the upper
        floor tile gets a corner in the south-east; and similarly,
        when placing the lower tentacle tile, we get called with dir
        main_dir::east to give the lower floor tile a NW overlay.
     */
    coord_def next = pos;
    switch (dir)
    {
        case main_dir::north: next += coord_def( 0, -1); break;
        case main_dir::east:  next += coord_def( 1,  0); break;
        case main_dir::south: next += coord_def( 0,  1); break;
        case main_dir::west:  next += coord_def(-1,  0); break;
        default:
            die("invalid direction");
    }
    if (!in_bounds(next))
        return;

    const coord_def next_showpos(grid2show(next));
    if (!show_bounds(next_showpos))
        return;

    tile_flags flag;
    switch (dir)
    {
        case main_dir::north: flag = TILE_FLAG_TENTACLE_SW; break;
        case main_dir::east: flag = TILE_FLAG_TENTACLE_NW; break;
        case main_dir::south: flag = TILE_FLAG_TENTACLE_NE; break;
        case main_dir::west: flag = TILE_FLAG_TENTACLE_SE; break;
        default: die("invalid direction");
    }
    tile_env.bg(next_showpos) |= flag;

    switch (type)
    {
        case tentacle_type::eldritch: flag = TILE_FLAG_TENTACLE_ELDRITCH; break;
        case tentacle_type::starspawn: flag = TILE_FLAG_TENTACLE_STARSPAWN; break;
        case tentacle_type::vine: flag = TILE_FLAG_TENTACLE_VINE; break;
        case tentacle_type::zombie_kraken: flag = TILE_FLAG_TENTACLE_ZOMBIE_KRAKEN; break;
        case tentacle_type::simulacrum_kraken: flag = TILE_FLAG_TENTACLE_SIMULACRUM_KRAKEN; break;
        case tentacle_type::spectral_kraken: flag = TILE_FLAG_TENTACLE_SPECTRAL_KRAKEN; break;
        default: flag = TILE_FLAG_TENTACLE_KRAKEN;
    }
    tile_env.bg(next_showpos) |= flag;
}

static void _handle_tentacle_overlay(const coord_def pos,
                                     const tileidx_t tile,
                                     tentacle_type type)
{
    switch (tile)
    {
    case TILEP_MONS_KRAKEN_TENTACLE_NW:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NW:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_NW:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_NW:
        _add_tentacle_overlay(pos, main_dir::north, type);
        break;
    case TILEP_MONS_KRAKEN_TENTACLE_NE:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_NE:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_NE:
        _add_tentacle_overlay(pos, main_dir::east, type);
        break;
    case TILEP_MONS_KRAKEN_TENTACLE_SE:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_SE:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_SE:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_SE:
        _add_tentacle_overlay(pos, main_dir::south, type);
        break;
    case TILEP_MONS_KRAKEN_TENTACLE_SW:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_SW:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_SW:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_SW:
        _add_tentacle_overlay(pos, main_dir::west, type);
        break;
    // diagonals
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NW_SE:
        _add_tentacle_overlay(pos, main_dir::north, type);
        _add_tentacle_overlay(pos, main_dir::south, type);
        break;
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE_SW:
        _add_tentacle_overlay(pos, main_dir::east, type);
        _add_tentacle_overlay(pos, main_dir::west, type);
        break;
    // other
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE_NW:
        _add_tentacle_overlay(pos, main_dir::north, type);
        _add_tentacle_overlay(pos, main_dir::east, type);
        break;
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE_SE:
        _add_tentacle_overlay(pos, main_dir::east, type);
        _add_tentacle_overlay(pos, main_dir::south, type);
        break;
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_SE_SW:
        _add_tentacle_overlay(pos, main_dir::south, type);
        _add_tentacle_overlay(pos, main_dir::west, type);
        break;
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NW_SW:
        _add_tentacle_overlay(pos, main_dir::north, type);
        _add_tentacle_overlay(pos, main_dir::west, type);
        break;
    }
}

static tentacle_type _get_tentacle_type(const monster_info& mon)
{
    switch (mon.type)
    {
        case MONS_KRAKEN_TENTACLE:
        case MONS_KRAKEN_TENTACLE_SEGMENT:
            switch (mon.base_type)
            {
                case MONS_ZOMBIE:
                    return tentacle_type::zombie_kraken;
                case MONS_SIMULACRUM:
                    return tentacle_type::simulacrum_kraken;
                case MONS_BOUND_SOUL:
                case MONS_SPECTRAL_THING:
                    return tentacle_type::spectral_kraken;
                default:
                    return tentacle_type::kraken;
            }
        case MONS_ELDRITCH_TENTACLE:
        case MONS_ELDRITCH_TENTACLE_SEGMENT:
            return tentacle_type::eldritch;
        case MONS_STARSPAWN_TENTACLE:
        case MONS_STARSPAWN_TENTACLE_SEGMENT:
            return tentacle_type::starspawn;
        case MONS_SNAPLASHER_VINE:
        case MONS_SNAPLASHER_VINE_SEGMENT:
            return tentacle_type::vine;

        default:
            die("Invalid tentacle type!");
            return tentacle_type::kraken; // Silence a warning
    }
}

static bool _tentacle_tile_not_flying(tileidx_t tile)
{
    // All tiles between these two enums feature tentacles
    // emerging from water.
    return tile >= TILEP_FIRST_TENTACLE_IN_WATER
           && tile <= TILEP_LAST_TENTACLE_IN_WATER
        || tile >= TILEP_FIRST_ZOMBIE_TENTACLE_IN_WATER
           && tile <= TILEP_LAST_ZOMBIE_TENTACLE_IN_WATER
        || tile >= TILEP_FIRST_SIMULACRUM_TENTACLE_IN_WATER
           && tile <= TILEP_LAST_SIMULACRUM_TENTACLE_IN_WATER
        || tile >= TILEP_FIRST_SPECTRAL_TENTACLE_IN_WATER
           && tile <= TILEP_LAST_SPECTRAL_TENTACLE_IN_WATER;
}

static tileidx_t _tileidx_monster_no_props(const monster_info& mon)
{
    const bool in_water = feat_is_water(env.map_knowledge(mon.pos).feat());

    if (mon.props.exists(MONSTER_TILE_KEY))
        return mon.props[MONSTER_TILE_KEY].get_int();

    // Show only base class for detected monsters.
    if (mons_class_is_zombified(mon.type))
        return _tileidx_monster_zombified(mon);

    int tile_num = 0;
    if (mon.props.exists(TILE_NUM_KEY))
        tile_num = mon.props[TILE_NUM_KEY].get_short();

    bool vary = !(mon.props.exists(FAKE_MON_KEY) && mon.props[FAKE_MON_KEY].get_bool());
    const tileidx_t base = tileidx_monster_base(mon.type,
                                                mon.pos.y*GXM + mon.pos.x,
                                                in_water, mon.colour(true),
                                                mon.number, tile_num, vary);

    switch (mon.type)
    {
        // use a different tile not using a standard ranged weapon.
        case MONS_CENTAUR:
        case MONS_CENTAUR_WARRIOR:
        case MONS_YAKTAUR:
        case MONS_YAKTAUR_CAPTAIN:
            return base + (_bow_offset(mon) ? 1 : 0);

        case MONS_CEREBOV:
        case MONS_SERAPH:
            return base + (mon.inv[MSLOT_WEAPON] ? 0 : 1);

        case MONS_BALLISTOMYCETE:
            return base + (mon.is_active ? 1 : 0);

        case MONS_DUVESSA:
        case MONS_DOWAN:
            return mon.props.exists(ELVEN_IS_ENERGIZED_KEY) ? base + 1 : base;

        case MONS_ARACHNE:
        {
            // Arachne normally is drawn with her staff wielded two-handed,
            // but will use a regular stance if she picks up a shield
            // (enhancer staves are compatible with those).
            const item_def* weapon = mon.inv[MSLOT_WEAPON].get();
            if (!mon.inv[MSLOT_SHIELD] && weapon
                && (weapon->is_type(OBJ_STAVES, STAFF_ALCHEMY)
                    || is_unrandom_artefact(*weapon, UNRAND_OLGREB)))
            {
                return base;
            }
            else
                return base + 1;
        }

        case MONS_AGNES:
        {
            // For if Agnes loses her lajatang
            const item_def * const weapon = mon.inv[MSLOT_WEAPON].get();
            if (weapon && weapon->is_type(OBJ_WEAPONS, WPN_LAJATANG))
                return TILEP_MONS_AGNES;
            else
                return TILEP_MONS_AGNES_STAVELESS;
        }

        case MONS_EDMUND:
        {
            // For if Edmund loses his weapon
            const item_def * const weapon = mon.inv[MSLOT_WEAPON].get();
            if (weapon && (weapon->is_type(OBJ_WEAPONS, WPN_DIRE_FLAIL)
                            || weapon->is_type(OBJ_WEAPONS, WPN_FLAIL)))
            {
                return TILEP_MONS_EDMUND;
            }
            else
                return TILEP_MONS_EDMUND_WEAPONLESS;
        }

        case MONS_ERICA:
        {
            // For if Erica loses her flaming scimitar
            const item_def * const weapon = mon.inv[MSLOT_WEAPON].get();
            if (weapon
                && weapon->is_type(OBJ_WEAPONS, WPN_SCIMITAR)
                && get_weapon_brand(*weapon) == SPWPN_FLAMING)
            {
                return TILEP_MONS_ERICA;
            }
            else
                return TILEP_MONS_ERICA_SWORDLESS;
        }

        case MONS_SIGMUND:
        {
            const item_def * const weapon = mon.inv[MSLOT_WEAPON].get();
            if (weapon && weapon->is_type(OBJ_WEAPONS, WPN_HALBERD)) // scythe
            {
                if (december_holidays())
                    return TILEP_MONS_XMAS_SIGMUND;
                return TILEP_MONS_SIGMUND;
            }
            return TILEP_MONS_SIGMUND_SCYTHELESS;
        }

        case MONS_QUOKKA:
            if (today_is_serious())
                return TILEP_MONS_QUOKKA_LORD_OF_SKULLS;
            return base;

        case MONS_OGRE:
            if (today_is_serious())
                return TILEP_MONS_SWAMP_OGRE;
            return base;

        case MONS_SUN_DEMON:
            if (today_is_serious())
                return TILEP_MONS_SUNNIER_DEMON;
            return base;

        case MONS_NESSOS:
        {
            const item_def * const weapon = mon.inv[MSLOT_WEAPON].get();
            if (weapon && weapon->is_type(OBJ_WEAPONS, WPN_LONGBOW))
                return TILEP_MONS_NESSOS;
            return TILEP_MONS_NESSOS_BOWLESS;
        }

        case MONS_BUSH:
            if (env.map_knowledge(mon.pos).cloud() == CLOUD_FIRE)
                return TILEP_MONS_BURNING_BUSH;
            return base;

        case MONS_BOULDER_BEETLE:
            return mon.is(MB_ROLLING)
                   ? _mon_random(TILEP_MONS_BOULDER_BEETLE_ROLLING, mon.number)
                   : base;

        case MONS_DANCING_WEAPON:
        {
            // Use item tile.
            ASSERT(mon.inv[MSLOT_WEAPON]);
            const item_def& item = *mon.inv[MSLOT_WEAPON];
            return tileidx_item(item);
        }

        case MONS_SPECTRAL_WEAPON:
        {
            // TODO: it would be good to show the TILE_FLAG_ANIM_OBJ icon with
            // these too, but most are oriented NW-SE and it looks bad
            if (!mon.inv[MSLOT_WEAPON])
                return TILEP_MONS_SPECTRAL_SBL;

            // Tiles exist for each class of weapon.
            const item_def& item = *mon.inv[MSLOT_WEAPON];
            switch (item_attack_skill(item))
            {
            case SK_LONG_BLADES:
                return TILEP_MONS_SPECTRAL_LBL;
            case SK_AXES:
                return TILEP_MONS_SPECTRAL_AXE;
            case SK_POLEARMS:
                return TILEP_MONS_SPECTRAL_SPEAR;
            case SK_STAVES:
                return TILEP_MONS_SPECTRAL_STAFF;
            case SK_MACES_FLAILS:
                {
                    const weapon_type wt = (weapon_type)item.sub_type;
                    return (wt == WPN_WHIP || wt == WPN_FLAIL
                            || wt == WPN_DIRE_FLAIL || wt == WPN_DEMON_WHIP
                            || wt == WPN_SACRED_SCOURGE)
                        ? TILEP_MONS_SPECTRAL_WHIP
                        : TILEP_MONS_SPECTRAL_MACE;
                }
            default:
                return TILEP_MONS_SPECTRAL_SBL;
            }
        }

        case MONS_KRAKEN_TENTACLE:
        case MONS_KRAKEN_TENTACLE_SEGMENT:
        case MONS_ELDRITCH_TENTACLE:
        case MONS_ELDRITCH_TENTACLE_SEGMENT:
        case MONS_STARSPAWN_TENTACLE:
        case MONS_STARSPAWN_TENTACLE_SEGMENT:
        case MONS_SNAPLASHER_VINE:
        case MONS_SNAPLASHER_VINE_SEGMENT:
        {
            tileidx_t tile = tileidx_tentacle(mon);
            _handle_tentacle_overlay(mon.pos, tile, _get_tentacle_type(mon));

            if (!_mons_is_kraken_tentacle(mon.type)
                && tile >= TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N
                && (tile <= TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_SE
                    || tile <= TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_NW &&
                       mon.type == MONS_SNAPLASHER_VINE_SEGMENT))
            {
                tile += TILEP_MONS_ELDRITCH_TENTACLE_PORTAL_N;
                tile -= TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N;

                if (mon.type == MONS_STARSPAWN_TENTACLE
                    || mon.type == MONS_STARSPAWN_TENTACLE_SEGMENT)
                {
                    tile += TILEP_MONS_STARSPAWN_TENTACLE_N;
                    tile -= TILEP_MONS_ELDRITCH_TENTACLE_N;
                }
                else if (mon.type == MONS_SNAPLASHER_VINE
                         || mon.type == MONS_SNAPLASHER_VINE_SEGMENT)
                {
                    tile += TILEP_MONS_VINE_N;
                    tile -= TILEP_MONS_ELDRITCH_TENTACLE_N;
                }
            }

            if (_mons_is_kraken_tentacle(mon.type) && mon.base_type == MONS_ZOMBIE)
            {
                tile += TILEP_MONS_KRAKEN_ZOMBIE_TENTACLE_SEGMENT_N;
                tile -= TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N;
            }
            if (_mons_is_kraken_tentacle(mon.type) && mon.base_type == MONS_SIMULACRUM)
            {
                tile += TILEP_MONS_KRAKEN_SIMULACRUM_TENTACLE_SEGMENT_N;
                tile -= TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N;
            }
            if (_mons_is_kraken_tentacle(mon.type)
                && (mon.base_type == MONS_SPECTRAL_THING || mon.base_type == MONS_BOUND_SOUL))
            {
                tile += TILEP_MONS_KRAKEN_SPECTRAL_TENTACLE_SEGMENT_N;
                tile -= TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N;
            }

            return tile;
        }

        case MONS_SENSED:
        {
            // Should be always out of LOS, though...
            if (base == TILEP_MONS_PROGRAM_BUG)
                return TILE_UNSEEN_MONSTER;
            return base;
        }

        default:
            return base;
    }
}

tileidx_t tileidx_monster(const monster_info& mons)
{
    tileidx_t ch = _tileidx_monster_no_props(mons);

    if ((!mons.ground_level() && !_tentacle_tile_not_flying(ch))
        || mons.type == MONS_ORC_APOSTLE || mons.type == MONS_SACRED_LOTUS)
    {
        ch |= TILE_FLAG_FLYING;
    }
    if (mons.is(MB_CAUGHT))
        ch |= TILE_FLAG_NET;
    if (mons.is(MB_WEBBED))
        ch |= TILE_FLAG_WEB;
    if (mons.is(MB_POISONED))
        ch |= TILE_FLAG_POISON;
    else if (mons.is(MB_MORE_POISONED))
        ch |= TILE_FLAG_MORE_POISON;
    else if (mons.is(MB_MAX_POISONED))
        ch |= TILE_FLAG_MAX_POISON;

    if (mons.attitude == ATT_FRIENDLY)
        ch |= TILE_FLAG_PET;
    else if (mons.attitude == ATT_GOOD_NEUTRAL)
        ch |= TILE_FLAG_GD_NEUTRAL;
    else if (mons.neutral())
        ch |= TILE_FLAG_NEUTRAL;
    else if (Options.tile_show_threat_levels.find("unusual") != string::npos
             && mons.has_unusual_items())
        ch |= TILE_FLAG_UNUSUAL;
    else if (mons.type == MONS_PLAYER_GHOST)
    {
       // Threat is always displayed for ghosts, with different tiles,
        // to make them more easily visible.
        ch |= TILE_FLAG_GHOST;
        switch (mons.threat)
        {
        case MTHRT_TRIVIAL:
            ch |= TILE_FLAG_TRIVIAL;
            break;
        case MTHRT_EASY:
            ch |= TILE_FLAG_EASY;
            break;
        case MTHRT_TOUGH:
            ch |= TILE_FLAG_TOUGH;
            break;
        case MTHRT_NASTY:
            ch |= TILE_FLAG_NASTY;
            break;
        default:
            break;
        }
    }
    else
        switch (mons.threat)
        {
        case MTHRT_TRIVIAL:
            if (Options.tile_show_threat_levels.find("trivial") != string::npos)
                ch |= TILE_FLAG_TRIVIAL;
            break;
        case MTHRT_EASY:
            if (Options.tile_show_threat_levels.find("easy") != string::npos)
                ch |= TILE_FLAG_EASY;
            break;
        case MTHRT_TOUGH:
            if (Options.tile_show_threat_levels.find("tough") != string::npos)
                ch |= TILE_FLAG_TOUGH;
            break;
        case MTHRT_NASTY:
            if (Options.tile_show_threat_levels.find("nasty") != string::npos)
                ch |= TILE_FLAG_NASTY;
            break;
        default:
            break;
        }

    if (mons.is(MB_PARALYSED))
        ch |= TILE_FLAG_PARALYSED;
    else if (mons.is(MB_FLEEING))
        ch |= TILE_FLAG_FLEEING;
    else if (mons.is(MB_STABBABLE) || mons.is(MB_SLEEPING)
             || mons.is(MB_DORMANT))
    {
        // XXX: should we have different tile flags for "stabbable" versus
        // "sleeping"?
        ch |= TILE_FLAG_STAB;
    }
    // Should petrify show the '?' symbol?
    else if (mons.is(MB_DISTRACTED) && !mons.is(MB_PETRIFYING)
            || mons.attitude == ATT_FRIENDLY && mons.is(MB_CONFUSED))
    {
        ch |= TILE_FLAG_MAY_STAB;
    }

    mon_dam_level_type damage_level = mons.dam;

    switch (damage_level)
    {
    case MDAM_DEAD:
    case MDAM_ALMOST_DEAD:
        ch |= TILE_FLAG_MDAM_ADEAD;
        break;
    case MDAM_SEVERELY_DAMAGED:
        ch |= TILE_FLAG_MDAM_SEV;
        break;
    case MDAM_HEAVILY_DAMAGED:
        ch |= TILE_FLAG_MDAM_HEAVY;
        break;
    case MDAM_MODERATELY_DAMAGED:
        ch |= TILE_FLAG_MDAM_MOD;
        break;
    case MDAM_LIGHTLY_DAMAGED:
        ch |= TILE_FLAG_MDAM_LIGHT;
        break;
    case MDAM_OKAY:
    default:
        // no flag for okay.
        break;
    }

#ifdef USE_TILE_LOCAL
    // handled on client side in WebTiles
    if (Options.tile_show_demon_tier)
    {
#endif
        // FIXME: non-linear bits suck, should be a simple addition
        switch (mons_demon_tier(mons.type))
        {
        case 1:
            ch |= TILE_FLAG_DEMON_1;
            break;
        case 2:
            ch |= TILE_FLAG_DEMON_2;
            break;
        case 3:
            ch |= TILE_FLAG_DEMON_3;
            break;
        case 4:
            ch |= TILE_FLAG_DEMON_4;
            break;
        case 5:
            ch |= TILE_FLAG_DEMON_5;
            break;
        }
#ifdef USE_TILE_LOCAL
    }
#endif

    return ch;
}
#endif

static const map<monster_info_flags, tileidx_t> monster_status_icons = {
    { MB_BURNING, TILEI_STICKY_FLAME },
    { MB_INNER_FLAME, TILEI_INNER_FLAME },
    { MB_BERSERK, TILEI_BERSERK },
    { MB_SLOWED, TILEI_SLOWED },
    { MB_MIRROR_DAMAGE, TILEI_PAIN_MIRROR },
    { MB_HASTED, TILEI_HASTED },
    { MB_STRONG, TILEI_MIGHT },
    { MB_PETRIFYING, TILEI_PETRIFYING },
    { MB_PETRIFIED, TILEI_PETRIFIED },
    { MB_BLIND, TILEI_BLIND },
    { MB_ABJURABLE, TILEI_SUMMONED },
    { MB_MINION, TILEI_MINION },
    { MB_UNREWARDING, TILEI_UNREWARDING },
    { MB_WORD_OF_RECALL, TILEI_RECALL },
    { MB_LIGHTLY_DRAINED, TILEI_DRAIN },
    { MB_HEAVILY_DRAINED, TILEI_DRAIN },
    { MB_IDEALISED, TILEI_IDEALISED },
    { MB_WRETCHED, TILEI_MALMUTATED },
    { MB_BOUND_SOUL, TILEI_BOUND_SOUL },
    { MB_INFESTATION, TILEI_INFESTED },
    { MB_CORROSION, TILEI_CORRODED },
    { MB_SWIFT, TILEI_SWIFT },
    { MB_VILE_CLUTCH, TILEI_VILE_CLUTCH },
    { MB_GRASPING_ROOTS, TILEI_CONSTRICTED }, // XXX placeholder
    { MB_POSSESSABLE, TILEI_POSSESSABLE },
    { MB_WITHERING, TILEI_SLOWLY_DYING },
    { MB_CRUMBLING, TILEI_SLOWLY_DYING },
    { MB_FIRE_CHAMPION, TILEI_FIRE_CHAMP },
    { MB_ANGUISH, TILEI_ANGUISH },
    { MB_WEAK, TILEI_WEAKENED },
    { MB_WATERLOGGED, TILEI_WATERLOGGED },
    { MB_STILL_WINDS, TILEI_STILL_WINDS },
    { MB_ANTIMAGIC, TILEI_ANTIMAGIC },
    { MB_DAZED, TILEI_DAZED },
    { MB_PARTIALLY_CHARGED, TILEI_PARTIALLY_CHARGED },
    { MB_FULLY_CHARGED, TILEI_FULLY_CHARGED },
    { MB_FIRE_VULN, TILEI_FIRE_VULN },
    { MB_CONCENTRATE_VENOM, TILEI_CONC_VENOM },
    { MB_REPEL_MSL, TILEI_REPEL_MISSILES },
    { MB_INJURY_BOND, TILEI_INJURY_BOND },
    { MB_TELEPORTING, TILEI_TELEPORTING },
    { MB_EMPOWERED_SPELLS, TILEI_BRILLIANCE },
    { MB_RESISTANCE, TILEI_RESISTANCE },
    { MB_CONTAM_LIGHT, TILEI_GLOW_LIGHT },
    { MB_CONTAM_HEAVY, TILEI_GLOW_HEAVY },
    { MB_PAIN_BOND, TILEI_PAIN_BOND },
    { MB_BOUND, TILEI_BIND },
    { MB_BULLSEYE_TARGET, TILEI_BULLSEYE},
    { MB_VITRIFIED, TILEI_VITRIFIED},
    { MB_CURSE_OF_AGONY, TILEI_CURSE_OF_AGONY},
    { MB_SPECTRALISED, TILEI_GHOSTLY},
    { MB_REGENERATION, TILEI_REGENERATION },
    { MB_RETREATING, TILEI_RETREAT  },
    { MB_TOUCH_OF_BEOGH, TILEI_TOUCH_OF_BEOGH },
    { MB_VENGEANCE_TARGET, TILEI_VENGEANCE_TARGET },
    { MB_MAGNETISED, TILEI_BULLSEYE },  // Placeholder
    { MB_RIMEBLIGHT, TILEI_RIMEBLIGHT },
    { MB_ARMED, TILEI_UNDYING_ARMS },
    { MB_SHADOWLESS, TILEI_SHADOWLESS },
    { MB_LOWERED_WL, TILEI_WEAK_WILLED },
    { MB_SIGN_OF_RUIN, TILEI_SIGN_OF_RUIN },
    { MB_DOUBLED_VIGOUR, TILEI_DOUBLED_VIGOUR },
    { MB_KINETIC_GRAPNEL, TILEI_KINETIC_GRAPNEL },
    { MB_TEMPERED, TILEI_TEMPERED },
    { MB_HATCHING, TILEI_HEART },
    { MB_BLINKITIS, TILEI_UNSTABLE },
    { MB_CHAOS_LACE, TILEI_LACED_WITH_CHAOS },
    { MB_VEXED, TILEI_VEXED },
};

set<tileidx_t> status_icons_for(const monster_info &mons)
{
    set<tileidx_t> icons;
    if (mons.type == MONS_DANCING_WEAPON)
        icons.insert(TILEI_ANIMATED_WEAPON);
    if (!mons.constrictor_name.empty())
        icons.insert(TILEI_CONSTRICTED);
    for (auto status : monster_status_icons)
        if (mons.is(status.first))
            icons.insert(status.second);
    return icons;
}

static const map<duration_type, pair<tileidx_t, string>> player_status_icons = {
    // Default displayed statuses
    { DUR_SLOW, {TILEI_SLOWED, "slow"} },
    { DUR_VITRIFIED, {TILEI_VITRIFIED, "fragile"} },
    { DUR_LOWERED_WL, {TILEI_WEAK_WILLED, "will/2"} },

    // Less critical or positive effects (or ones already covered by a default
    // force_more_message). Not enabled by default.
    { DUR_HASTE, {TILEI_HASTED, "haste"} },
    { DUR_WEAK, {TILEI_WEAKENED, "weak"} },
    { DUR_CORROSION, {TILEI_CORRODED, "corr"} },
    { DUR_MIGHT, {TILEI_MIGHT, "might"} },
    { DUR_BRILLIANCE, {TILEI_BRILLIANCE, "brill"} },
    { DUR_NO_MOMENTUM, {TILEI_BIND, "-move"} },
    { DUR_PETRIFYING, {TILEI_PETRIFYING, "petr"} },
    { DUR_SENTINEL_MARK, {TILEI_BULLSEYE, "mark"} },
};

#ifdef USE_TILE
static bool _should_show_player_status_icon(const string& name)
{
    return find (Options.tile_player_status_icons.begin(),
                 Options.tile_player_status_icons.end(), name)
                    != Options.tile_player_status_icons.end();
}
#endif

set<tileidx_t> status_icons_for_player()
{
    set<tileidx_t> icons;
#ifdef USE_TILE
    if (you.is_constricted() && _should_show_player_status_icon("constr"))
        icons.insert(TILEI_CONSTRICTED);
    for (auto status : player_status_icons)
    {
        if (you.duration[status.first]
            && _should_show_player_status_icon(status.second.second))
        {
            icons.insert(status.second.first);
        }
    }
#endif
    return icons;
}

static tileidx_t tileidx_draco_base(monster_type draco)
{
    return TILEP_DRACO_BASE + (draco - MONS_DRACONIAN);
}

tileidx_t tileidx_draco_base(const monster_info& mon)
{
    return tileidx_draco_base(mon.draconian_subspecies());
}

tileidx_t tileidx_draco_job(const monster_info& mon)
{
    if (mons_is_draconian_job(mon.type))
        return get_mon_base_tile(mon.type);
    return 0;
}

#ifdef USE_TILE
/**
 * Return the monster tile used for the player based on a monster type.
 *
 * When using the player species monster or a monster in general instead of an
 * explicit tile name, this function cleans up the tiles for certain monsters
 * where there's an alternate tile that's better than the base one for doll
 * purposes.
 * @returns The tile id of the tile that will be used.
*/
tileidx_t tileidx_player_mons()
{
    ASSERT(player_uses_monster_tile());

    if (you.duration[DUR_EXECUTION])
        return TILEP_MONS_EXECUTIONER;

    if (you.may_pruneify() && you.cannot_act())
        return TILEP_MONS_PRUNE;

    monster_type mons;
    if (Options.tile_player_tile)
        return Options.tile_player_tile;

    if (Options.tile_use_monster == MONS_PLAYER)
        mons = player_mons(false);
    else
        mons = Options.tile_use_monster;

    if (mons_is_base_draconian(mons))
        return tileidx_draco_base(mons);

    switch (mons)
    {
    case MONS_CENTAUR:         return TILEP_MONS_CENTAUR_MELEE;
    case MONS_CENTAUR_WARRIOR: return TILEP_MONS_CENTAUR_WARRIOR_MELEE;
    case MONS_YAKTAUR:         return TILEP_MONS_YAKTAUR_MELEE;
    case MONS_YAKTAUR_CAPTAIN: return TILEP_MONS_YAKTAUR_CAPTAIN_MELEE;
    default:                   return tileidx_monster_base(mons, 0, you.in_water());
    }
}

tileidx_t tileidx_player_shadow()
{
    if (species::is_draconian(you.species))
        return TILEP_MONS_PLAYER_SHADOW_DRACONIAN;

    switch (you.species)
    {
        case SP_ARMATAUR:       return TILEP_MONS_PLAYER_SHADOW_ARMATAUR;
        case SP_BARACHI:        return TILEP_MONS_PLAYER_SHADOW_BARACHI;
        case SP_COGLIN:         return TILEP_MONS_PLAYER_SHADOW_COGLIN;
        case SP_DEMIGOD:        return TILEP_MONS_PLAYER_SHADOW_DEMIGOD;
        case SP_DEMONSPAWN:     return TILEP_MONS_PLAYER_SHADOW_DEMONSPAWN;
        case SP_DJINNI:         return TILEP_MONS_PLAYER_SHADOW_DJINN;
        case SP_MOUNTAIN_DWARF: return TILEP_MONS_PLAYER_SHADOW_DWARF;
        case SP_DEEP_ELF:       return TILEP_MONS_PLAYER_SHADOW_ELF;
        case SP_FELID:          return TILEP_MONS_PLAYER_SHADOW_FELID;
        case SP_FORMICID:       return TILEP_MONS_PLAYER_SHADOW_FORMICID;
        case SP_GARGOYLE:       return TILEP_MONS_PLAYER_SHADOW_GARGOYLE;
        case SP_GHOUL:          return TILEP_MONS_PLAYER_SHADOW_GHOUL;
        case SP_GNOLL:          return TILEP_MONS_PLAYER_SHADOW_GNOLL;
        case SP_HUMAN:          return TILEP_MONS_PLAYER_SHADOW_HUMAN;
        case SP_KOBOLD:         return TILEP_MONS_PLAYER_SHADOW_KOBOLD;
        case SP_MERFOLK:        return TILEP_MONS_PLAYER_SHADOW_MERFOLK;
        case SP_MINOTAUR:       return TILEP_MONS_PLAYER_SHADOW_MINOTAUR;
        case SP_MUMMY:          return TILEP_MONS_PLAYER_SHADOW_MUMMY;
        case SP_NAGA:           return TILEP_MONS_PLAYER_SHADOW_NAGA;
        case SP_OCTOPODE:       return TILEP_MONS_PLAYER_SHADOW_OCTOPODE;
        case SP_ONI:            return TILEP_MONS_PLAYER_SHADOW_ONI;
        case SP_SPRIGGAN:       return TILEP_MONS_PLAYER_SHADOW_SPRIGGAN;
        case SP_TENGU:          return TILEP_MONS_PLAYER_SHADOW_TENGU;
        case SP_TROLL:          return TILEP_MONS_PLAYER_SHADOW_TROLL;
        case SP_VAMPIRE:        return TILEP_MONS_PLAYER_SHADOW_VAMPIRE;
        case SP_VINE_STALKER:   return TILEP_MONS_PLAYER_SHADOW_VINE_STALKER;

        default:
            return TILEP_MONS_PROGRAM_BUG;
    }
}
#endif // USE_TILE

static tileidx_t _tileidx_unrand_artefact(int idx)
{
    const tileidx_t tile = unrandart_to_tile(idx);
    return tile ? tile : tileidx_t{TILE_TODO};
}

static tileidx_t _tileidx_wyrmbane(int plus)
{
    if (plus < 10)
        return TILE_UNRAND_WYRMBANE;
    else if (plus < 12)
        return TILE_UNRAND_WYRMBANE1;
    else if (plus < 15)
        return TILE_UNRAND_WYRMBANE2;
    else if (plus < 18)
        return TILE_UNRAND_WYRMBANE3;
    else
        return TILE_UNRAND_WYRMBANE4;
}

static tileidx_t _tileidx_weapon_base(const item_def &item)
{
    if (item.props.exists(ITEM_TILE_KEY))
        return item.props[ITEM_TILE_KEY].get_short();
    switch (item.sub_type)
    {
    case WPN_DAGGER:                return TILE_WPN_DAGGER;
    case WPN_SHORT_SWORD:           return TILE_WPN_SHORT_SWORD;
    case WPN_QUICK_BLADE:           return TILE_WPN_QUICK_BLADE;
    case WPN_RAPIER:                return TILE_WPN_RAPIER;
    case WPN_FALCHION:              return TILE_WPN_FALCHION;
    case WPN_LONG_SWORD:            return TILE_WPN_LONG_SWORD;
    case WPN_GREAT_SWORD:           return TILE_WPN_GREAT_SWORD;
    case WPN_SCIMITAR:              return TILE_WPN_SCIMITAR;
    case WPN_DOUBLE_SWORD:          return TILE_WPN_DOUBLE_SWORD;
    case WPN_TRIPLE_SWORD:          return TILE_WPN_TRIPLE_SWORD;
    case WPN_HAND_AXE:              return TILE_WPN_HAND_AXE;
    case WPN_WAR_AXE:               return TILE_WPN_WAR_AXE;
    case WPN_BROAD_AXE:             return TILE_WPN_BROAD_AXE;
    case WPN_BATTLEAXE:             return TILE_WPN_BATTLEAXE;
    case WPN_EXECUTIONERS_AXE:      return TILE_WPN_EXECUTIONERS_AXE;
#if TAG_MAJOR_VERSION == 34
    case WPN_BLOWGUN:               return TILE_WPN_BLOWGUN;
#endif
    case WPN_SLING:                 return TILE_WPN_SLING;
    case WPN_SHORTBOW:              return TILE_WPN_SHORTBOW;
    case WPN_ORCBOW:                return TILE_WPN_ORCBOW;
    case WPN_HAND_CANNON:           return TILE_WPN_HAND_CANNON;
    case WPN_ARBALEST:              return TILE_WPN_ARBALEST;
    case WPN_TRIPLE_CROSSBOW:       return TILE_WPN_TRIPLE_CROSSBOW;
    case WPN_SPEAR:                 return TILE_WPN_SPEAR;
    case WPN_TRIDENT:               return TILE_WPN_TRIDENT;
    case WPN_HALBERD:               return TILE_WPN_HALBERD;
    case WPN_GLAIVE:                return TILE_WPN_GLAIVE;
    case WPN_PARTISAN:              return TILE_WPN_PARTISAN;
#if TAG_MAJOR_VERSION == 34
    case WPN_STAFF:                 return TILE_WPN_STAFF;
#endif
    case WPN_QUARTERSTAFF:          return TILE_WPN_QUARTERSTAFF;
    case WPN_CLUB:                  return TILE_WPN_CLUB;
    case WPN_MACE:                  return TILE_WPN_MACE;
    case WPN_FLAIL:                 return TILE_WPN_FLAIL;
    case WPN_GREAT_MACE:            return TILE_WPN_GREAT_MACE;
    case WPN_DIRE_FLAIL:            return TILE_WPN_DIRE_FLAIL;
    case WPN_MORNINGSTAR:           return TILE_WPN_MORNINGSTAR;
    case WPN_EVENINGSTAR:           return TILE_WPN_EVENINGSTAR;
    case WPN_GIANT_CLUB:            return TILE_WPN_GIANT_CLUB;
    case WPN_GIANT_SPIKED_CLUB:     return TILE_WPN_GIANT_SPIKED_CLUB;
    case WPN_WHIP:                  return TILE_WPN_WHIP;
    case WPN_DEMON_BLADE:           return TILE_WPN_DEMON_BLADE;
    case WPN_EUDEMON_BLADE:         return TILE_WPN_BLESSED_BLADE;
    case WPN_DEMON_WHIP:            return TILE_WPN_DEMON_WHIP;
    case WPN_SACRED_SCOURGE:        return TILE_WPN_SACRED_SCOURGE;
    case WPN_DEMON_TRIDENT:         return TILE_WPN_DEMON_TRIDENT;
    case WPN_TRISHULA:              return TILE_WPN_TRISHULA;
    case WPN_LONGBOW:               return TILE_WPN_LONGBOW;
    case WPN_LAJATANG:              return TILE_WPN_LAJATANG;
    case WPN_BARDICHE:              return TILE_WPN_BARDICHE;
    }

    return TILE_ERROR;
}

static tileidx_t _tileidx_weapon(const item_def &item)
{
    tileidx_t tile = _tileidx_weapon_base(item);
    return tileidx_enchant_equ(item, tile);
}

static tileidx_t _tileidx_missile_base(const item_def &item)
{
    int brand = item.brand;
    // 0 indicates no ego at all

    switch (item.sub_type)
    {
    case MI_STONE:        return TILE_MI_STONE;
    case MI_LARGE_ROCK:   return TILE_MI_LARGE_ROCK;
    case MI_THROWING_NET: return TILE_MI_THROWING_NET;
    case MI_BOOMERANG:
        switch (brand)
        {
        default:             return TILE_MI_BOOMERANG + 1;
        case 0:              return TILE_MI_BOOMERANG;
        case SPMSL_SILVER:   return TILE_MI_BOOMERANG_SILVER;
        }

    case MI_DART:
        switch (brand)
        {
        default:             return TILE_MI_DART + 1;
        case 0:              return TILE_MI_DART;
        case SPMSL_POISONED: return TILE_MI_DART_POISONED;
        case SPMSL_CURARE:   return TILE_MI_DART_CURARE;
        case SPMSL_BLINDING: return TILE_MI_DART_BLINDING;
        case SPMSL_FRENZY:   return TILE_MI_DART_FRENZY;
        }

    case MI_ARROW:
        switch (brand)
        {
        default:             return TILE_MI_ARROW + 1;
        case 0:              return TILE_MI_ARROW;
#if TAG_MAJOR_VERSION == 34
        case SPMSL_STEEL:    return TILE_MI_ARROW_STEEL;
#endif
        case SPMSL_SILVER:   return TILE_MI_ARROW_SILVER;
        }

    case MI_BOLT:
        switch (brand)
        {
        default:             return TILE_MI_BOLT + 1;
        case 0:              return TILE_MI_BOLT;
#if TAG_MAJOR_VERSION == 34
        case SPMSL_STEEL:    return TILE_MI_BOLT_STEEL;
#endif
        case SPMSL_SILVER:   return TILE_MI_BOLT_SILVER;
        }

    case MI_SLUG:
    case MI_SLING_BULLET:
        switch (brand)
        {
        default:             return TILE_MI_SLING_BULLET + 1;
        case 0:              return TILE_MI_SLING_BULLET;
#if TAG_MAJOR_VERSION == 34
        case SPMSL_STEEL:    return TILE_MI_SLING_BULLET_STEEL;
#endif
        case SPMSL_SILVER:   return TILE_MI_SLING_BULLET_SILVER;
        }

    case MI_JAVELIN:
        switch (brand)
        {
        default:             return TILE_MI_JAVELIN + 1;
        case 0:              return TILE_MI_JAVELIN;
#if TAG_MAJOR_VERSION == 34
        case SPMSL_STEEL:    return TILE_MI_JAVELIN_STEEL;
#endif
        case SPMSL_SILVER:   return TILE_MI_JAVELIN_SILVER;
        }
    }

    return TILE_ERROR;
}

static tileidx_t _tileidx_missile(const item_def &item)
{
    int tile = _tileidx_missile_base(item);
    return tileidx_enchant_equ(item, tile);
}

static tileidx_t _tileidx_armour_base(const item_def &item)
{
    if (item.props.exists(ITEM_TILE_KEY))
        return item.props[ITEM_TILE_KEY].get_short();

    int type  = item.sub_type;
    switch (type)
    {
    case ARM_ROBE:
        return TILE_ARM_ROBE;

    case ARM_LEATHER_ARMOUR:
        return TILE_ARM_LEATHER_ARMOUR;

    case ARM_RING_MAIL:
        return TILE_ARM_RING_MAIL;

    case ARM_SCALE_MAIL:
        return TILE_ARM_SCALE_MAIL;

    case ARM_CHAIN_MAIL:
        return TILE_ARM_CHAIN_MAIL;

    case ARM_PLATE_ARMOUR:
        return TILE_ARM_PLATE_ARMOUR;

    case ARM_CRYSTAL_PLATE_ARMOUR:
        return TILE_ARM_CRYSTAL_PLATE_ARMOUR;

    case ARM_KITE_SHIELD:
        return TILE_ARM_KITE_SHIELD;

    case ARM_CLOAK:
        return TILE_ARM_CLOAK;

    case ARM_SCARF:
        return TILE_ARM_SCARF;

    case ARM_HAT:
        return TILE_THELM_HAT;

#if TAG_MAJOR_VERSION == 34
    case ARM_CAP:
        return TILE_THELM_CAP;
#endif

    case ARM_HELMET:
        return TILE_THELM_HELM;

    case ARM_GLOVES:
        return TILE_ARM_GLOVES;

    case ARM_BOOTS:
        return TILE_ARM_BOOTS;

    case ARM_ORB:
        return TILE_ARM_ORB;

    case ARM_BUCKLER:
        return TILE_ARM_BUCKLER;

    case ARM_TOWER_SHIELD:
        return TILE_ARM_TOWER_SHIELD;

    case ARM_BARDING:
        return TILE_ARM_BARDING;

    case ARM_ANIMAL_SKIN:
        return TILE_ARM_ANIMAL_SKIN;

    case ARM_TROLL_LEATHER_ARMOUR:
        return TILE_ARM_TROLL_LEATHER_ARMOUR;

    case ARM_FIRE_DRAGON_ARMOUR:
        return TILE_ARM_FIRE_DRAGON_ARMOUR;

    case ARM_ICE_DRAGON_ARMOUR:
        return TILE_ARM_ICE_DRAGON_ARMOUR;

    case ARM_STEAM_DRAGON_ARMOUR:
        return TILE_ARM_STEAM_DRAGON_ARMOUR;

    case ARM_ACID_DRAGON_ARMOUR:
        return TILE_ARM_ACID_DRAGON_ARMOUR;

    case ARM_QUICKSILVER_DRAGON_ARMOUR:
        return TILE_ARM_QUICKSILVER_DRAGON_ARMOUR;

    case ARM_STORM_DRAGON_ARMOUR:
        return TILE_ARM_STORM_DRAGON_ARMOUR;

    case ARM_SHADOW_DRAGON_ARMOUR:
        return TILE_ARM_SHADOW_DRAGON_ARMOUR;

    case ARM_GOLDEN_DRAGON_ARMOUR:
        return TILE_ARM_GOLDEN_DRAGON_ARMOUR;

    case ARM_PEARL_DRAGON_ARMOUR:
        return TILE_ARM_PEARL_DRAGON_ARMOUR;

    case ARM_SWAMP_DRAGON_ARMOUR:
        return TILE_ARM_SWAMP_DRAGON_ARMOUR;
    }

    return TILE_ERROR;
}

static tileidx_t _tileidx_armour(const item_def &item)
{
    tileidx_t tile = _tileidx_armour_base(item);
    return tileidx_enchant_equ(item, tile);
}

// Returns index of skeleton tiles.
// Parameter item holds the skeleton.
static tileidx_t _tileidx_bone(const item_def &item)
{
    const monster_type mc = item.mon_type;
    const size_type st = get_monster_data(mc)->size;
    const int cs = max(0, st - SIZE_MEDIUM + 1);

    switch (get_mon_shape(item.mon_type))
    {
    case MON_SHAPE_HUMANOID:
    case MON_SHAPE_HUMANOID_TAILED:
    case MON_SHAPE_HUMANOID_WINGED:
    case MON_SHAPE_HUMANOID_WINGED_TAILED:
        return TILE_FOOD_BONE_HUMANOID + cs;
    default:
        return TILE_FOOD_BONE + cs;
    }
}

// Returns index of corpse tiles.
// Parameter item holds the corpse.
static tileidx_t _tileidx_corpse(const item_def &item)
{
    const int type = item.plus;
    const tileidx_t base = get_mon_base_corpse_tile((monster_type)type);

    switch (type)
    {
    case MONS_KILLER_KLOWN:
    {
        const int count = tile_main_count(TILE_CORPSE_KILLER_KLOWN);
        return base + ui_random(count);
    }

    case MONS_UGLY_THING:
    case MONS_VERY_UGLY_THING:
    {
        int colour_offset = ugly_thing_colour_offset(item.get_colour());
        if (colour_offset == -1)
            colour_offset = 0;
        return base + colour_offset;
    }

    default:
        return base;
    }
}

static tileidx_t _tileidx_gem_base(const item_def &item)
{
    if (item.quantity <= 0)
        return TILE_GEM_GENERIC;

    switch (item.sub_type)
    {
    default:
    case GEM_DUNGEON: return TILE_GEM_DUNGEON;
#if TAG_MAJOR_VERSION == 34
    case GEM_ORC:     return TILE_GEM_ORC;
#endif
    case GEM_ELF:     return TILE_GEM_ELF;
    case GEM_LAIR:    return TILE_GEM_LAIR;
    case GEM_SWAMP:   return TILE_GEM_SWAMP;
    case GEM_SHOALS:  return TILE_GEM_SHOALS;
    case GEM_SNAKE:   return TILE_GEM_SNAKE;
    case GEM_SPIDER:  return TILE_GEM_SPIDER;
    case GEM_SLIME:   return TILE_GEM_SLIME;
    case GEM_VAULTS:  return TILE_GEM_VAULTS;
    case GEM_CRYPT:   return TILE_GEM_CRYPT;
    case GEM_TOMB:    return TILE_GEM_TOMB;
    case GEM_DEPTHS:  return TILE_GEM_DEPTHS;
    case GEM_ZOT:     return TILE_GEM_ZOT;
    }
}

static tileidx_t _tileidx_gem(const item_def &item)
{
    const tileidx_t base = _tileidx_gem_base(item);
    if (item.sub_type < NUM_GEM_TYPES && you.gems_shattered[item.sub_type]
        && (Options.more_gem_info || !you.gems_found[item.sub_type]))
    {
        return base + 1;
    }
    return base;
}

static tileidx_t _tileidx_uncollected_rune(const item_def &item)
{
    switch (item.sub_type)
    {
    // the hell runes:
    case RUNE_DIS:         return TILE_UNCOLLECTED_RUNE_DIS;
    case RUNE_GEHENNA:     return TILE_UNCOLLECTED_RUNE_GEHENNA;
    case RUNE_COCYTUS:     return TILE_UNCOLLECTED_RUNE_COCYTUS;
    case RUNE_TARTARUS:    return TILE_UNCOLLECTED_RUNE_TARTARUS;

    // special pandemonium runes:
    case RUNE_MNOLEG:      return TILE_UNCOLLECTED_RUNE_MNOLEG;
    case RUNE_LOM_LOBON:   return TILE_UNCOLLECTED_RUNE_LOM_LOBON;
    case RUNE_CEREBOV:     return TILE_UNCOLLECTED_RUNE_CEREBOV;
    case RUNE_GLOORX_VLOQ: return TILE_UNCOLLECTED_RUNE_GLOORX_VLOQ;

    case RUNE_DEMONIC:     return TILE_UNCOLLECTED_RUNE_DEMONIC;
    case RUNE_ABYSSAL:     return TILE_UNCOLLECTED_RUNE_ABYSS;

    case RUNE_SNAKE:       return TILE_UNCOLLECTED_RUNE_SNAKE;
    case RUNE_SPIDER:      return TILE_UNCOLLECTED_RUNE_SPIDER;
    case RUNE_SLIME:       return TILE_UNCOLLECTED_RUNE_SLIME;
    case RUNE_VAULTS:      return TILE_UNCOLLECTED_RUNE_VAULTS;
    case RUNE_TOMB:        return TILE_UNCOLLECTED_RUNE_TOMB;
    case RUNE_SWAMP:       return TILE_UNCOLLECTED_RUNE_SWAMP;
    case RUNE_SHOALS:      return TILE_UNCOLLECTED_RUNE_SHOALS;
    case RUNE_ELF:         return TILE_UNCOLLECTED_RUNE_ELVEN;

    case RUNE_FOREST:
    default:               return TILE_MISC_UNCOLLECTED_RUNE_OF_ZOT;
    }
}

static tileidx_t _tileidx_rune(const item_def &item)
{
    switch (item.sub_type)
    {
    // the hell runes:
    case RUNE_DIS:         return TILE_RUNE_DIS;
    case RUNE_GEHENNA:     return TILE_RUNE_GEHENNA;
    case RUNE_COCYTUS:     return TILE_RUNE_COCYTUS;
    case RUNE_TARTARUS:    return TILE_RUNE_TARTARUS;

    // special pandemonium runes:
    case RUNE_MNOLEG:      return TILE_RUNE_MNOLEG;
    case RUNE_LOM_LOBON:   return TILE_RUNE_LOM_LOBON;
    case RUNE_CEREBOV:     return TILE_RUNE_CEREBOV;
    case RUNE_GLOORX_VLOQ: return TILE_RUNE_GLOORX_VLOQ;

    case RUNE_DEMONIC:     return TILE_RUNE_DEMONIC
        + ((uint32_t)item.rnd) % tile_main_count(TILE_RUNE_DEMONIC);
    case RUNE_ABYSSAL:     return TILE_RUNE_ABYSS;

    case RUNE_SNAKE:       return TILE_RUNE_SNAKE;
    case RUNE_SPIDER:      return TILE_RUNE_SPIDER;
    case RUNE_SLIME:       return TILE_RUNE_SLIME;
    case RUNE_VAULTS:      return TILE_RUNE_VAULTS;
    case RUNE_TOMB:        return TILE_RUNE_TOMB;
    case RUNE_SWAMP:       return TILE_RUNE_SWAMP;
    case RUNE_SHOALS:      return TILE_RUNE_SHOALS;
    case RUNE_ELF:         return TILE_RUNE_ELVEN;

    case RUNE_FOREST:
    default:               return TILE_MISC_RUNE_OF_ZOT;
    }
}

static tileidx_t _tileidx_talisman(const item_def &item)
{
    switch (item.sub_type)
    {
    case TALISMAN_BEAST:    return TILE_TALISMAN_BEAST;
    case TALISMAN_FLUX:    return TILE_TALISMAN_FLUX;
    case TALISMAN_SERPENT:  return TILE_TALISMAN_SNAKE;
    case TALISMAN_MAW:      return TILE_TALISMAN_MAW;
    case TALISMAN_BLADE:    return TILE_TALISMAN_BLADE;
    case TALISMAN_STATUE:   return TILE_TALISMAN_STATUE;
    case TALISMAN_DRAGON:   return TILE_TALISMAN_DRAGON;
    case TALISMAN_STORM:    return TILE_TALISMAN_STORM;
    case TALISMAN_DEATH:    return TILE_TALISMAN_DEATH;
    default: return TILE_ERROR;
    }
}

static tileidx_t _tileidx_misc(const item_def &item)
{
    switch (item.sub_type)
    {
#if TAG_MAJOR_VERSION == 34
    case MISC_BOTTLED_EFREET:
        return TILE_MISC_BOTTLED_EFREET;

    case MISC_FAN_OF_GALES:
        return TILE_MISC_FAN_OF_GALES_INERT;

    case MISC_LAMP_OF_FIRE:
        return TILE_MISC_LAMP_OF_FIRE_INERT;

    case MISC_STONE_OF_TREMORS:
        return TILE_MISC_STONE_OF_TREMORS_INERT;
#endif

    case MISC_PHIAL_OF_FLOODS:
        return evoker_charges(item.sub_type) ? TILE_MISC_PHIAL_OF_FLOODS
                                             : TILE_MISC_PHIAL_OF_FLOODS_INERT;

    case MISC_TIN_OF_TREMORSTONES:
        return evoker_charges(item.sub_type) ? TILE_MISC_TIN_OF_TREMORSTONES
                                             : TILE_MISC_TIN_OF_TREMORSTONES_INERT;

    case MISC_CONDENSER_VANE:
            return evoker_charges(item.sub_type) ? TILE_MISC_CONDENSER_VANE
                                                 : TILE_MISC_CONDENSER_VANE_INERT;

#if TAG_MAJOR_VERSION == 34
    case MISC_BUGGY_LANTERN_OF_SHADOWS:
        return TILE_MISC_LANTERN_OF_SHADOWS;
#endif

    case MISC_HORN_OF_GERYON:
        return TILE_MISC_HORN_OF_GERYON;

    case MISC_BOX_OF_BEASTS:
        return evoker_charges(item.sub_type) ? TILE_MISC_BOX_OF_BEASTS
                                             : TILE_MISC_BOX_OF_BEASTS_INERT;

#if TAG_MAJOR_VERSION == 34
    case MISC_CRYSTAL_BALL_OF_ENERGY:
        return TILE_MISC_CRYSTAL_BALL_OF_ENERGY;
#endif

    case MISC_LIGHTNING_ROD:
        return evoker_charges(item.sub_type) ? TILE_MISC_LIGHTNING_ROD
                                             : TILE_MISC_LIGHTNING_ROD_INERT;

    case MISC_SACK_OF_SPIDERS:
        return TILE_MISC_SACK_OF_SPIDERS;

    case MISC_GRAVITAMBOURINE:
        return evoker_charges(item.sub_type) ? TILE_MISC_TAMBOURINE
                                             : TILE_MISC_TAMBOURINE_INERT;

    // Default for summary menus
    case NUM_MISCELLANY:
    case MISC_PHANTOM_MIRROR:
        return TILE_MISC_PHANTOM_MIRROR;

    case MISC_ZIGGURAT:
        return TILE_MISC_ZIGGURAT;

    case MISC_QUAD_DAMAGE:
        return TILE_MISC_QUAD_DAMAGE;

    case MISC_SHOP_VOUCHER:
        return TILE_MISC_SHOP_VOUCHER;
    }

    return TILE_ERROR;
}

static tileidx_t _tileidx_gold(const item_def &item)
{
    if (item.quantity >= 1 && item.quantity <= 10)
        return TILE_GOLD01 + item.quantity - 1;
    if (item.quantity < 20)
        return TILE_GOLD16;
    if (item.quantity < 30)
        return TILE_GOLD19;
    if (item.quantity < 100)
        return TILE_GOLD23;
    return TILE_GOLD25;
}

tileidx_t tileidx_item(const item_def &item)
{
    const int clas        = item.base_type;
    const int type        = item.sub_type;
    const int subtype_rnd = item.subtype_rnd;
    const int rnd         = item.rnd;

    if (item.props.exists(ITEM_TILE_KEY)
        && clas != OBJ_WEAPONS && clas != OBJ_ARMOUR)
    {
        return item.props[ITEM_TILE_KEY].get_short();
    }

    switch (clas)
    {
    case OBJ_WEAPONS:
        if (is_unrandom_artefact(item, UNRAND_WYRMBANE))
            return _tileidx_wyrmbane(item.plus);
        else if (is_unrandom_artefact(item))
            return _tileidx_unrand_artefact(find_unrandart_index(item));
        else
            return _tileidx_weapon(item);

    case OBJ_MISSILES:
        return _tileidx_missile(item);

    case OBJ_ARMOUR:
        if (is_unrandom_artefact(item))
            return _tileidx_unrand_artefact(find_unrandart_index(item));
        else
            return _tileidx_armour(item);

    case OBJ_WANDS:
        if (item.is_identified())
            return TILE_WAND_ID_FIRST + type;
        else
            return TILE_WAND_OFFSET + subtype_rnd % NDSC_WAND_PRI;

    case OBJ_SCROLLS:
        if (item.is_identified())
            return TILE_SCR_ID_FIRST + type;
        return TILE_SCROLL;

    case OBJ_GOLD:
        return _tileidx_gold(item);

    case OBJ_JEWELLERY:
        if (is_unrandom_artefact(item))
            return _tileidx_unrand_artefact(find_unrandart_index(item));

        // rings
        if (!jewellery_is_amulet(item))
        {
            if (is_artefact(item))
            {
                const int offset = item.rnd
                                   % tile_main_count(TILE_RING_RANDART_OFFSET);
                return TILE_RING_RANDART_OFFSET + offset;
            }

            if (item.is_identified())
            {
                return TILE_RING_ID_FIRST + type - RING_FIRST_RING
#if TAG_MAJOR_VERSION == 34
                       + 1 // we have a save-compat ring tile before FIRST_RING
#endif
                    ;
            }

            return TILE_RING_NORMAL_OFFSET + subtype_rnd % NDSC_JEWEL_PRI;
        }

        // amulets
        if (is_artefact(item))
        {
            const int offset = item.rnd
                               % tile_main_count(TILE_AMU_RANDART_OFFSET);
            return TILE_AMU_RANDART_OFFSET + offset;
        }

        if (item.is_identified())
            return TILE_AMU_ID_FIRST + type - AMU_FIRST_AMULET;
        return TILE_AMU_NORMAL_OFFSET + subtype_rnd % NDSC_JEWEL_PRI;

    case OBJ_POTIONS:
        if (item.is_identified())
            return TILE_POT_ID_FIRST + type;
        else
            return TILE_POTION_OFFSET + item.subtype_rnd % NDSC_POT_PRI;

    case OBJ_BOOKS:
        if (is_random_artefact(item))
        {
            const int offset = rnd % tile_main_count(TILE_BOOK_RANDART_OFFSET);
            return TILE_BOOK_RANDART_OFFSET + offset;
        }

        if (item.sub_type == BOOK_MANUAL)
            return TILE_BOOK_MANUAL + rnd % tile_main_count(TILE_BOOK_MANUAL);

        return TILE_BOOK_OFFSET
               + rnd % tile_main_count(TILE_BOOK_OFFSET);

    case OBJ_STAVES:
        if (is_artefact(item))
        {
            const int off = rnd % tile_main_count(TILE_STAFF_RANDART_OFFSET);
            return TILE_STAFF_RANDART_OFFSET + off;
        }

        if (item.is_identified())
            return TILE_STAFF_ID_FIRST + type;

        return TILE_STAFF_OFFSET
               + (subtype_rnd / NDSC_STAVE_PRI) % NDSC_STAVE_SEC;

#if TAG_MAJOR_VERSION == 34
    case OBJ_RODS:
        return TILE_ROD + item.rnd % tile_main_count(TILE_ROD);
#endif

    case OBJ_CORPSES:
        if (!Options.show_blood || item.sub_type == CORPSE_SKELETON)
            return _tileidx_bone(item);
        else
            return _tileidx_corpse(item);

    case OBJ_ORBS:
        if (item.quantity <= 0)
            return TILE_UNCOLLECTED_ORB;
        return TILE_ORB + (you.frame_no % tile_main_count(TILE_ORB));

    case OBJ_MISCELLANY:
        return _tileidx_misc(item);

    case OBJ_TALISMANS:
        return _tileidx_talisman(item);

    case OBJ_RUNES:
        if (item.quantity <= 0)
            return _tileidx_uncollected_rune(item);
        return _tileidx_rune(item);

    case OBJ_GEMS:
        return _tileidx_gem(item);

    case OBJ_GIZMOS:
        return TILE_GIZMO + item.rnd % tile_main_count(TILE_GIZMO);

    case OBJ_DETECTED:
        return TILE_UNSEEN_ITEM;

    default:
        return TILE_ERROR;
    }
}

#ifdef USE_TILE
//  Determine Octant of missile direction
//   .---> X+
//   |
//   |  701
//   Y  6O2
//   +  543
//
// The octant boundary slope tan(pi/8)=sqrt(2)-1 = 0.414 is approximated by 2/5.
static int _tile_bolt_dir(int dx, int dy)
{
    int ax = abs(dx);
    int ay = abs(dy);

    if (5*ay < 2*ax)
        return (dx > 0) ? 2 : 6;
    else if (5*ax < 2*ay)
        return (dy > 0) ? 4 : 0;
    else if (dx > 0)
        return (dy > 0) ? 3 : 1;
    else
        return (dy > 0) ? 5: 7;
}

tileidx_t tileidx_item_throw(const item_def &item, int dx, int dy)
{
    if (item.base_type == OBJ_MISSILES)
    {
        int ch = -1;
        int dir = _tile_bolt_dir(dx, dy);

        // Thrown items with multiple directions
        switch (item.sub_type)
        {
            case MI_ARROW:
                ch = TILE_MI_ARROW0;
                break;
            case MI_BOLT:
                ch = TILE_MI_BOLT0;
                break;
            case MI_DART:
                ch = TILE_MI_DART0;
                break;
            case MI_JAVELIN:
                ch = TILE_MI_JAVELIN0;
                break;
            case MI_THROWING_NET:
                ch = TILE_MI_THROWING_NET0;
                break;
            case MI_SLUG:
                ch = TILE_MI_SLUG0;
                break;
            default:
                break;
        }
        if (ch != -1)
            return ch + dir;

        // Thrown items with a single direction
        switch (item.sub_type)
        {
            case MI_STONE:
                ch = TILE_MI_STONE0;
                break;
            case MI_SLING_BULLET:
                switch (item.brand)
                {
                default:
                    ch = TILE_MI_SLING_BULLET0;
                    break;
#if TAG_MAJOR_VERSION == 34
                case SPMSL_STEEL:
                    ch = TILE_MI_SLING_BULLET_STEEL0;
                    break;
#endif
                case SPMSL_SILVER:
                    ch = TILE_MI_SLING_BULLET_SILVER0;
                    break;
                }
                break;
            case MI_LARGE_ROCK:
                ch = TILE_MI_LARGE_ROCK0;
                break;
            case MI_THROWING_NET:
                ch = TILE_MI_THROWING_NET0;
                break;
            case MI_BOOMERANG:
                ch = TILE_MI_BOOMERANG0;
            default:
                break;
        }
        if (ch != -1)
            return tileidx_enchant_equ(item, ch);
    }

    // If not a special case, just return the default tile.
    return tileidx_item(item);
}
#endif // USE_TILE

// For items with randomized descriptions, only the overlay label is
// placed in the tile page. This function looks up what the base item
// is based on the randomized description. It returns 0 if there is none.
tileidx_t tileidx_known_base_item(tileidx_t label)
{
    if (label >= TILE_POT_ID_FIRST && label <= TILE_POT_ID_LAST)
    {
        int type = label - TILE_POT_ID_FIRST;
        int desc = you.item_description[IDESC_POTIONS][type] % NDSC_POT_PRI;

        if (!type_is_identified(OBJ_POTIONS, type))
            return TILE_UNSEEN_POTION;
        else
            return TILE_POTION_OFFSET + desc;
    }

    if (label >= TILE_RING_ID_FIRST && label <= TILE_RING_ID_LAST)
    {
        int type = label - TILE_RING_ID_FIRST + RING_FIRST_RING
#if TAG_MAJOR_VERSION == 34
                   - 1 // we have a save-compat ring tile before FIRST_RING
#endif
            ;
        int desc = you.item_description[IDESC_RINGS][type] % NDSC_JEWEL_PRI;

        if (!type_is_identified(OBJ_JEWELLERY, type))
            return TILE_UNSEEN_RING;
        else
            return TILE_RING_NORMAL_OFFSET + desc;
    }

    if (label >= TILE_AMU_ID_FIRST && label <= TILE_AMU_ID_LAST)
    {
        int type = label - TILE_AMU_ID_FIRST + AMU_FIRST_AMULET;
        int desc = you.item_description[IDESC_RINGS][type] % NDSC_JEWEL_PRI;

        if (!type_is_identified(OBJ_JEWELLERY, type))
            return TILE_UNSEEN_AMULET;
        else
            return TILE_AMU_NORMAL_OFFSET + desc;
    }

    if (label >= TILE_SCR_ID_FIRST && label <= TILE_SCR_ID_LAST)
        return TILE_SCROLL;

    if (label >= TILE_WAND_ID_FIRST && label <= TILE_WAND_ID_LAST)
    {
        int type = label - TILE_WAND_ID_FIRST;
        int desc = you.item_description[IDESC_WANDS][type] % NDSC_WAND_PRI;

        if (!type_is_identified(OBJ_WANDS, type))
            return TILE_UNSEEN_WAND;
        else
            return TILE_WAND_OFFSET + desc;
    }

    if (label >= TILE_STAFF_ID_FIRST && label <= TILE_STAFF_ID_LAST)
    {
        int type = label - TILE_STAFF_ID_FIRST;
        int desc = you.item_description[IDESC_STAVES][type];
        desc = (desc / NDSC_STAVE_PRI) % NDSC_STAVE_SEC;

        if (!type_is_identified(OBJ_STAVES, type))
            return TILE_UNSEEN_STAFF;
        else
            return TILE_STAFF_OFFSET + desc;
    }

    return 0;
}

tileidx_t tileidx_cloud(const cloud_info &cl)
{
    const cloud_type type  = cl.type;
    const int colour = cl.colour;
    const unsigned int dur = cl.duration;

    tileidx_t ch = cl.tile;

    if (ch == 0)
    {
        const cloud_tile_info &tile_info = cloud_type_tile_info(type);

        switch (tile_info.variation)
        {
            case CTVARY_NONE:
                ch = tile_info.base;
                break;
            case CTVARY_DUR:
                ch = tile_info.base + min(dur,
                                          tile_main_count(tile_info.base) - 1);
                break;
            case CTVARY_RANDOM:
                ch = tile_info.base + hash_with_seed(
                        tile_main_count(tile_info.base),
                        cl.pos.y * GXM + cl.pos.x, you.frame_no);
                break;
        }

        if (!ch || ch == TILE_ERROR)
            ch = TILE_CLOUD_GREY_SMOKE;

        switch (type)
        {
            case CLOUD_MUTAGENIC:
                ch = (dur == 0 ? TILE_CLOUD_MUTAGENIC_0 :
                      dur == 1 ? TILE_CLOUD_MUTAGENIC_1
                               : TILE_CLOUD_MUTAGENIC_2);
                ch += ui_random(tile_main_count(ch));
                break;

            case CLOUD_VORTEX:
                ch = get_vortex_phase(cl.pos) ? TILE_CLOUD_FREEZING_WINDS_0
                                               : TILE_CLOUD_FREEZING_WINDS_1;
                break;

            default:
                break;
        }
    }

    if (colour != -1)
        ch = tile_main_coloured(ch, colour);

    return ch;
}

#ifdef USE_TILE
tileidx_t vary_bolt_tile(tileidx_t tile, const coord_def& origin,
                         const coord_def& target, const coord_def& pos)
{
    const coord_def diff = target - origin;
    const int dir = _tile_bolt_dir(diff.x, diff.y);
    const int dist = (pos - origin).rdist();

    return vary_bolt_tile(tile, dir, dist);
}

tileidx_t vary_bolt_tile(tileidx_t tile, int dir, int dist)
{
    switch (tile)
    {
    case TILE_BOLT_STONE_ARROW:
    case TILE_BOLT_CRYSTAL_SPEAR:
    case TILE_BOLT_ICICLE:
    case TILE_BOLT_ICICLE_SALVO:
    case TILE_BOLT_LIGHT:
    case TILE_BOLT_IRON_SHOT:
    case TILE_BOLT_BLOOD_ARROW:
    case TILE_BOLT_SPLINTERSPRAY:
    case TILE_BOLT_PIE:
    case TILE_BOLT_POISON_ARROW:
    case TILE_BOLT_SHADOW_SHOT:
    case TILE_BOLT_FORCE_LANCE:
    case TILE_BOLT_HARPOON_SHOT:
    case TILE_BOLT_METAL_SPLINTERS:
        return tile + dir;

    case TILE_BOLT_ZAP:
    case TILE_BOLT_SHADOW_BEAM:
        return tile + dir % tile_main_count(tile);

    case TILE_BOLT_FROST:
    case TILE_BOLT_MAGIC_DART:
    case TILE_BOLT_SANDBLAST:
    case TILE_BOLT_STING:
    case TILE_BOLT_MEPHITIC_FLASK:
        return tile + (dist - 1) % tile_main_count(tile);

    case TILE_BOLT_FLAME:
    case TILE_BOLT_MAGMA:
    case TILE_BOLT_ICEBLAST:
    case TILE_BOLT_ALEMBIC_POTION:
    case TILE_BOLT_IRRADIATE:
    case TILE_BOLT_POTION_PETITION:
    case TILE_BOLT_SHADOW_BLAST:
    case TILE_BOLT_HAEMOCLASM:
    case TILE_BOLT_GHOSTLY_FIREBALL:
    case TILE_BOLT_BOMBLET_BLAST:
    case TILE_BOLT_MANIFOLD_ASSAULT:
    case TILE_BOLT_PARAGON_TEMPEST:
    case TILE_BOLT_CHAOS_BUFF:
        return tile + ui_random(tile_main_count(tile));

    case TILE_MI_BOOMERANG0:
        return tile + ui_random(4);

    default:
        return tile;
    }
}

tileidx_t tileidx_zap(int colour)
{
    switch (colour)
    {
    case ETC_HOLY:
        colour = YELLOW;
        break;
    default:
        colour = element_colour(colour);
        break;
    }

    if (colour < 1)
        colour = 7;
    else if (colour > 8)
        colour -= 8;

    return TILE_SYM_BOLT_OFS - 1 + colour;
}
#endif // USE_TILE

tileidx_t tileidx_spell(spell_type spell)
{
    if (spell == NUM_SPELLS)
        return TILEG_MEMORISE; // XXX: Hack!
    return get_spell_tile(spell);
}

/**
 * Get the appropriate tile for the given skill @ the given training level.
 *
 * @param skill     The skill in question; e.g. SK_FIGHTING.
 * @param train     The training_status to render at; e.g. TRAINING_DISABLED.
 * @return          An appropriate tile; e.g. TILEG_FIGHTING_OFF>
 */
tileidx_t tileidx_skill(skill_type skill, int train)
{
    tileidx_t ch;
    switch (skill)
    {
    case SK_FIGHTING:       ch = TILEG_FIGHTING_ON; break;
    case SK_SHORT_BLADES:   ch = TILEG_SHORT_BLADES_ON; break;
    case SK_LONG_BLADES:    ch = TILEG_LONG_BLADES_ON; break;
    case SK_AXES:           ch = TILEG_AXES_ON; break;
    case SK_MACES_FLAILS:   ch = TILEG_MACES_FLAILS_ON; break;
    case SK_POLEARMS:       ch = TILEG_POLEARMS_ON; break;
    case SK_STAVES:         ch = TILEG_STAVES_ON; break;
    case SK_RANGED_WEAPONS: ch = TILEG_RANGED_WEAPONS_ON; break;
    case SK_THROWING:       ch = TILEG_THROWING_ON; break;
    case SK_ARMOUR:         ch = TILEG_ARMOUR_ON; break;
    case SK_DODGING:        ch = TILEG_DODGING_ON; break;
    case SK_STEALTH:        ch = TILEG_STEALTH_ON; break;
    case SK_SHIELDS:        ch = TILEG_SHIELDS_ON; break;
    case SK_UNARMED_COMBAT:
        {
            const string hand = you.hand_name(false);
            if (hand == "hand")
                ch = TILEG_UNARMED_COMBAT_ON;
            else if (hand == "paw")
                ch = TILEG_UNARMED_COMBAT_PAW_ON;
            else if (hand == "tentacle")
                ch = TILEG_UNARMED_COMBAT_TENTACLE_ON;
            else
                ch = TILEG_UNARMED_COMBAT_CLAW_ON;
        }
        break;
    case SK_SPELLCASTING:   ch = TILEG_SPELLCASTING_ON; break;
    case SK_CONJURATIONS:   ch = TILEG_CONJURATIONS_ON; break;
    case SK_HEXES:          ch = TILEG_HEXES_ON; break;
    case SK_SUMMONINGS:     ch = TILEG_SUMMONINGS_ON; break;
    case SK_FORGECRAFT:     ch = TILEG_FORGECRAFT_ON; break;
    case SK_NECROMANCY:
        ch = you.religion == GOD_KIKUBAAQUDGHA ? TILEG_NECROMANCY_K_ON
                                               : TILEG_NECROMANCY_ON; break;
    case SK_TRANSLOCATIONS: ch = TILEG_TRANSLOCATIONS_ON; break;
    case SK_FIRE_MAGIC:     ch = TILEG_FIRE_MAGIC_ON; break;
    case SK_ICE_MAGIC:      ch = TILEG_ICE_MAGIC_ON; break;
    case SK_AIR_MAGIC:      ch = TILEG_AIR_MAGIC_ON; break;
    case SK_EARTH_MAGIC:    ch = TILEG_EARTH_MAGIC_ON; break;
    case SK_ALCHEMY:        ch = TILEG_TRANSMUTATIONS_ON; break;
    case SK_EVOCATIONS:     ch = TILEG_EVOCATIONS_ON; break;
    case SK_SHAPESHIFTING:  ch = TILEG_SHAPESHIFTING_ON; break;
    case SK_INVOCATIONS:
        {
            switch (you.religion)
            {
            // Gods who use invo get a unique tile.
            case GOD_SHINING_ONE:
                ch = TILEG_INVOCATIONS_1_ON; break;
            case GOD_BEOGH:
                ch = TILEG_INVOCATIONS_B_ON; break;
            case GOD_CHEIBRIADOS:
                ch = TILEG_INVOCATIONS_C_ON; break;
            case GOD_DITHMENOS:
                ch = TILEG_INVOCATIONS_D_ON; break;
            case GOD_ELYVILON:
                ch = TILEG_INVOCATIONS_E_ON; break;
            case GOD_FEDHAS:
                ch = TILEG_INVOCATIONS_F_ON; break;
            case GOD_HEPLIAKLQANA:
                ch = TILEG_INVOCATIONS_H_ON; break;
            case GOD_LUGONU:
                ch = TILEG_INVOCATIONS_L_ON; break;
            case GOD_MAKHLEB:
                ch = TILEG_INVOCATIONS_M_ON; break;
            case GOD_NEMELEX_XOBEH:
                ch = TILEG_INVOCATIONS_N_ON; break;
            case GOD_OKAWARU:
                ch = TILEG_INVOCATIONS_O_ON; break;
            case GOD_QAZLAL:
                ch = TILEG_INVOCATIONS_Q_ON; break;
            case GOD_SIF_MUNA:
                ch = TILEG_INVOCATIONS_S_ON; break;
            case GOD_USKAYAW:
                ch = TILEG_INVOCATIONS_U_ON; break;
            case GOD_YREDELEMNUL:
                ch = TILEG_INVOCATIONS_Y_ON; break;
            case GOD_ZIN:
                ch = TILEG_INVOCATIONS_Z_ON; break;
            default:
                ch = TILEG_INVOCATIONS_ON;
            }
        }
        break;
    default:                return TILEG_TODO;
    }

    switch (train)
    {
    case TRAINING_DISABLED:
        return ch + TILEG_FIGHTING_OFF - TILEG_FIGHTING_ON;
    case TRAINING_INACTIVE:
        return ch + TILEG_FIGHTING_INACTIVE - TILEG_FIGHTING_ON;
    case TRAINING_ENABLED:
        return ch;
    case TRAINING_FOCUSED:
        return ch + TILEG_FIGHTING_FOCUS - TILEG_FIGHTING_ON;
    case TRAINING_MASTERED:
        return ch + TILEG_FIGHTING_MAX - TILEG_FIGHTING_ON;
    default:
        die("invalid skill tile type");
    }

}

tileidx_t tileidx_command(const command_type cmd)
{
    switch (cmd)
    {
    case CMD_REST:
        return TILEG_CMD_REST;
    case CMD_EXPLORE:
        return TILEG_CMD_EXPLORE;
    case CMD_INTERLEVEL_TRAVEL:
        return TILEG_CMD_INTERLEVEL_TRAVEL;
    case CMD_AUTOFIGHT:
        return TILEG_CMD_AUTOFIGHT;
    case CMD_WAIT:
        return TILEG_CMD_WAIT;
    case CMD_USE_ABILITY:
        return TILEG_CMD_USE_ABILITY;
    case CMD_SEARCH_STASHES:
        return TILEG_CMD_SEARCH_STASHES;
    case CMD_REPLAY_MESSAGES:
        return TILEG_CMD_REPLAY_MESSAGES;
    case CMD_RESISTS_SCREEN:
        return TILEG_CMD_RESISTS_SCREEN;
    case CMD_DISPLAY_OVERMAP:
        return TILEG_CMD_DISPLAY_OVERMAP;
    case CMD_DISPLAY_RELIGION:
        return TILEG_CMD_DISPLAY_RELIGION;
    case CMD_DISPLAY_MUTATIONS:
        return TILEG_CMD_DISPLAY_MUTATIONS;
    case CMD_MACRO_ADD: // this tile is a fairly generic + despite the name
    case CMD_MACRO_MENU:
    case CMD_DISPLAY_SKILLS:
        return TILEG_CMD_DISPLAY_SKILLS;
    case CMD_SHOW_CHARACTER_DUMP:
    case CMD_DISPLAY_CHARACTER_STATUS:
        return TILEG_CMD_DISPLAY_CHARACTER_STATUS;
    case CMD_DISPLAY_KNOWN_OBJECTS:
        return TILEG_CMD_KNOWN_ITEMS;
    case CMD_SAVE_GAME_NOW:
    case CMD_SAVE_GAME:
        return TILEG_CMD_SAVE_GAME_NOW;
#ifdef USE_TILE
    case CMD_EDIT_PLAYER_TILE:
        return TILEG_CMD_EDIT_PLAYER_TILE;
#endif
    case CMD_DISPLAY_COMMANDS:
        return TILEG_CMD_DISPLAY_COMMANDS;
    case CMD_LOOKUP_HELP:
        return TILEG_CMD_LOOKUP_HELP;
    case CMD_CHARACTER_DUMP:
        return TILEG_CMD_CHARACTER_DUMP;
    case CMD_DISPLAY_INVENTORY:
        return TILEG_CMD_DISPLAY_INVENTORY;
    case CMD_CAST_SPELL:
        return TILEG_CMD_CAST_SPELL;
    case CMD_MEMORISE_SPELL:
        return TILEG_CMD_MEMORISE_SPELL;
    case CMD_DROP:
        return TILEG_CMD_DROP;
    case CMD_DISPLAY_MAP:
        return TILEG_CMD_DISPLAY_MAP;
    case CMD_MAP_EXIT_MAP:
        return TILEG_CMD_MAP_EXIT_MAP;
    case CMD_MAP_GOTO_TARGET:
        return TILEG_CMD_MAP_GOTO_TARGET;
    case CMD_MAP_NEXT_LEVEL:
        return TILEG_CMD_MAP_NEXT_LEVEL;
    case CMD_MAP_PREV_LEVEL:
        return TILEG_CMD_MAP_PREV_LEVEL;
    case CMD_MAP_GOTO_LEVEL:
        return TILEG_CMD_MAP_GOTO_LEVEL;
    case CMD_MAP_EXCLUDE_AREA:
        return TILEG_CMD_MAP_EXCLUDE_AREA;
    case CMD_MAP_FIND_EXCLUDED:
        return TILEG_CMD_MAP_FIND_EXCLUDED;
    case CMD_MAP_CLEAR_EXCLUDES:
        return TILEG_CMD_MAP_CLEAR_EXCLUDES;
    case CMD_MAP_ADD_WAYPOINT:
        return TILEG_CMD_MAP_ADD_WAYPOINT;
    case CMD_MAP_FIND_WAYPOINT:
        return TILEG_CMD_MAP_FIND_WAYPOINT;
    case CMD_MAP_FIND_UPSTAIR:
        return TILEG_CMD_MAP_FIND_UPSTAIR;
    case CMD_MAP_FIND_DOWNSTAIR:
        return TILEG_CMD_MAP_FIND_DOWNSTAIR;
    case CMD_MAP_FIND_YOU:
        return TILEG_CMD_MAP_FIND_YOU;
    case CMD_MAP_FIND_PORTAL:
        return TILEG_CMD_MAP_FIND_PORTAL;
    case CMD_MAP_FIND_TRAP:
        return TILEG_CMD_MAP_FIND_TRAP;
    case CMD_MAP_FIND_ALTAR:
        return TILEG_CMD_MAP_FIND_ALTAR;
    case CMD_MAP_FIND_STASH:
        return TILEG_CMD_MAP_FIND_STASH;
    case CMD_QUIT:
        return TILEG_SYMBOL_OF_TORMENT;
    case CMD_GAME_MENU:
        return TILEG_CMD_GAME_MENU;
#ifdef __ANDROID__
    case CMD_TOGGLE_KEYBOARD:
        return TILEG_CMD_KEYBOARD;
#endif
    default:
        return TILEG_TODO;
    }
}

#ifdef USE_TILE_LOCAL
tileidx_t tileidx_gametype(const game_type gtype)
{
    switch (gtype)
    {
    case GAME_TYPE_NORMAL:
    case GAME_TYPE_CUSTOM_SEED:
        return TILEG_STARTUP_STONESOUP;
    case GAME_TYPE_DESCENT:
        return TILEG_STARTUP_IRONSOUP;
    case GAME_TYPE_TUTORIAL:
        return TILEG_STARTUP_TUTORIAL;
    case GAME_TYPE_HINTS:
        return TILEG_STARTUP_HINTS;
    case GAME_TYPE_SPRINT:
        return TILEG_STARTUP_SPRINT;
    case GAME_TYPE_INSTRUCTIONS:
        return TILEG_STARTUP_INSTRUCTIONS;
    case GAME_TYPE_ARENA:
        return TILEG_STARTUP_ARENA;
    case GAME_TYPE_HIGH_SCORES:
        return TILEG_STARTUP_HIGH_SCORES;
    default:
        return TILEG_ERROR;
    }
}
#endif

tileidx_t tileidx_ability(const ability_type ability)
{
    switch (ability)
    {
    // Innate abilities and (Demonspaw) mutations.
    case ABIL_SPIT_POISON:
        return TILEG_ABILITY_SPIT_POISON;
    case ABIL_BREATHE_FIRE:
    case ABIL_COMBUSTION_BREATH:
        return TILEG_ABILITY_BREATHE_FIRE;
    case ABIL_GLACIAL_BREATH:
        return TILEG_ABILITY_BREATHE_FROST;
    case ABIL_BREATHE_POISON:
        return TILEG_ABILITY_BREATHE_POISON;
    case ABIL_GALVANIC_BREATH:
        return TILEG_ABILITY_BREATHE_LIGHTNING;
    case ABIL_NULLIFYING_BREATH:
        return TILEG_ABILITY_BREATHE_ENERGY;
    case ABIL_STEAM_BREATH:
        return TILEG_ABILITY_BREATHE_STEAM;
    case ABIL_NOXIOUS_BREATH:
        return TILEG_ABILITY_BREATHE_MEPHITIC;
    case ABIL_CAUSTIC_BREATH:
        return TILEG_ABILITY_BREATHE_ACID;
    case ABIL_MUD_BREATH:
        return TILEG_ABILITY_BREATHE_MUD;
    case ABIL_HOP:
        return TILEG_ABILITY_HOP;
    case ABIL_BLINKBOLT:
        return TILEG_ABILITY_BLINKBOLT;
    case ABIL_SIPHON_ESSENCE:
        return TILEG_ABILITY_SIPHON_ESSENCE;
    case ABIL_INVENT_GIZMO:
        return TILEG_ABILITY_INVENT_GIZMO;
    case ABIL_IMBUE_SERVITOR:
        return TILEG_ABILITY_IMBUE_SERVITOR;
    case ABIL_IMPRINT_WEAPON:
        return TILEG_ABILITY_IMPRINT_WEAPON;

    // Others
    case ABIL_END_TRANSFORMATION:
    case ABIL_BEGIN_UNTRANSFORM:
        return TILEG_ABILITY_END_TRANSFORMATION;

    // Species-specific abilities.
    // Demonspawn-only
    case ABIL_DAMNATION:
        return TILEG_ABILITY_HURL_DAMNATION;
    case ABIL_WORD_OF_CHAOS:
        return TILEG_ABILITY_WORD_OF_CHAOS;
    // Vampires
    case ABIL_TRAN_BAT:
        return TILEG_ABILITY_BAT_FORM;
    case ABIL_EXSANGUINATE:
        return TILEG_ABILITY_EXSANGUINATE;
    case ABIL_REVIVIFY:
        return TILEG_ABILITY_REVIVIFY;
#if TAG_MAJOR_VERSION == 34
    // Deep Dwarves
    case ABIL_HEAL_WOUNDS:
        return TILEG_ABILITY_HEAL_WOUNDS;
#endif
    // Formicids
    case ABIL_DIG:
        return TILEG_ABILITY_DIG;
    case ABIL_SHAFT_SELF:
        return TILEG_ABILITY_SHAFT_SELF;

    // Evoking items.
    case ABIL_EVOKE_BLINK:
        return TILEG_ABILITY_BLINK;
    case ABIL_EVOKE_DISPATER:
        return TILEG_ABILITY_EVOKE_DISPATER;
    case ABIL_EVOKE_OLGREB:
        return TILEG_ABILITY_EVOKE_OLGREB;
    case ABIL_EVOKE_TURN_INVISIBLE:
        return TILEG_ABILITY_EVOKE_INVISIBILITY;

    // Divine abilities
    // Zin
    case ABIL_ZIN_RECITE:
        return TILEG_ABILITY_ZIN_RECITE;
    case ABIL_ZIN_VITALISATION:
        return TILEG_ABILITY_ZIN_VITALISATION;
    case ABIL_ZIN_IMPRISON:
        return TILEG_ABILITY_ZIN_IMPRISON;
    case ABIL_ZIN_SANCTUARY:
        return TILEG_ABILITY_ZIN_SANCTUARY;
    case ABIL_ZIN_DONATE_GOLD:
        return TILEG_ABILITY_ZIN_DONATE_GOLD;
    // TSO
    case ABIL_TSO_DIVINE_SHIELD:
        return TILEG_ABILITY_TSO_DIVINE_SHIELD;
    case ABIL_TSO_CLEANSING_FLAME:
        return TILEG_ABILITY_TSO_CLEANSING_FLAME;
    case ABIL_TSO_SUMMON_DIVINE_WARRIOR:
        return TILEG_ABILITY_TSO_DIVINE_WARRIOR;
    case ABIL_TSO_BLESS_WEAPON:
        return TILEG_ABILITY_TSO_BLESS_WEAPON;
    // Kiku
    case ABIL_KIKU_UNEARTH_WRETCHES:
        return TILEG_ABILITY_KIKU_UNEARTH_WRETCHES;
    case ABIL_KIKU_SIGN_OF_RUIN:
        return TILEG_ABILITY_KIKU_SIGN_OF_RUIN;
    case ABIL_KIKU_BLESS_WEAPON:
        return TILEG_ABILITY_KIKU_BLESS_WEAPON;
    case ABIL_KIKU_GIFT_CAPSTONE_SPELLS:
        return TILEG_ABILITY_KIKU_NECRONOMICON;
    // Yredelemnul
    case ABIL_YRED_LIGHT_THE_TORCH:
        return TILEG_ABILITY_YRED_LIGHT_THE_TORCH;
    case ABIL_YRED_RECALL_UNDEAD_HARVEST:
        return TILEG_ABILITY_YRED_RECALL;
    case ABIL_YRED_HURL_TORCHLIGHT:
        return TILEG_ABILITY_YRED_HURL_TORCHLIGHT;
    case ABIL_YRED_BIND_SOUL:
        return TILEG_ABILITY_YRED_BIND_SOUL;
    case ABIL_YRED_FATHOMLESS_SHACKLES:
        return TILEG_ABILITY_YRED_FATHOMLESS_SHACKLES;
    // Xom, Vehumet = 90
    // Okawaru
    case ABIL_OKAWARU_HEROISM:
        return TILEG_ABILITY_OKAWARU_HEROISM;
    case ABIL_OKAWARU_FINESSE:
        return TILEG_ABILITY_OKAWARU_FINESSE;
    case ABIL_OKAWARU_DUEL:
        return TILEG_ABILITY_OKAWARU_DUEL;
    case ABIL_OKAWARU_GIFT_WEAPON:
        return TILEG_ABILITY_OKAWARU_GIFT_WEAPON;
    case ABIL_OKAWARU_GIFT_ARMOUR:
        return TILEG_ABILITY_OKAWARU_GIFT_ARMOUR;
    // Makhleb
    case ABIL_MAKHLEB_DESTRUCTION:
        if (you.has_mutation(MUT_MAKHLEB_DESTRUCTION_COC))
            return TILEG_ABILITY_MAKHLEB_MAJOR_DESTRUCTION_COC;
        else if (you.has_mutation(MUT_MAKHLEB_DESTRUCTION_DIS))
            return TILEG_ABILITY_MAKHLEB_MAJOR_DESTRUCTION_DIS;
        else if (you.has_mutation(MUT_MAKHLEB_DESTRUCTION_GEH))
            return TILEG_ABILITY_MAKHLEB_MAJOR_DESTRUCTION_GEH;
        else if (you.has_mutation(MUT_MAKHLEB_DESTRUCTION_TAR))
            return TILEG_ABILITY_MAKHLEB_MAJOR_DESTRUCTION_TAR;
        else
            return TILEG_ABILITY_MAKHLEB_MINOR_DESTRUCTION;
    case ABIL_MAKHLEB_ANNIHILATION:
        return TILEG_ABILITY_MAKHLEB_GLOBE_OF_ANNIHILATION;
    case ABIL_MAKHLEB_INFERNAL_SERVANT:
        return TILEG_ABILITY_MAKHLEB_LESSER_SERVANT;
    case ABIL_MAKHLEB_INFERNAL_LEGION:
        return TILEG_ABILITY_MAKHLEB_LESSER_SERVANT;
    case ABIL_MAKHLEB_VESSEL_OF_SLAUGHTER:
        return TILEG_ABILITY_MAKHLEB_VESSEL_OF_SLAUGHTER;
    case ABIL_MAKHLEB_BRAND_SELF_1:
    case ABIL_MAKHLEB_BRAND_SELF_2:
    case ABIL_MAKHLEB_BRAND_SELF_3:
    {
        switch (makhleb_ability_to_mutation(ability))
        {
            case MUT_MAKHLEB_MARK_HAEMOCLASM:
                return TILEG_ABILITY_MAKHLEB_MARK_OF_HAEMOCLASM;
            case MUT_MAKHLEB_MARK_LEGION:
                return TILEG_ABILITY_MAKHLEB_MARK_OF_THE_LEGION;
            case MUT_MAKHLEB_MARK_CARNAGE:
                return TILEG_ABILITY_MAKHLEB_MARK_OF_CARNAGE;
            case MUT_MAKHLEB_MARK_ANNIHILATION:
                return TILEG_ABILITY_MAKHLEB_MARK_OF_ANNIHILATION;
            case MUT_MAKHLEB_MARK_TYRANT:
                return TILEG_ABILITY_MAKHLEB_MARK_OF_THE_TYRANT;
            case MUT_MAKHLEB_MARK_CELEBRANT:
                return TILEG_ABILITY_MAKHLEB_MARK_OF_THE_CELEBRANT;
            case MUT_MAKHLEB_MARK_EXECUTION:
                return TILEG_ABILITY_MAKHLEB_MARK_OF_EXECUTION;
            case MUT_MAKHLEB_MARK_ATROCITY:
                return TILEG_ABILITY_MAKHLEB_MARK_OF_ATROCITY;
            case MUT_MAKHLEB_MARK_FANATIC:
                return TILEG_ABILITY_MAKHLEB_MARK_OF_THE_FANATIC;
            default:
                return TILEG_ERROR;
        }
    }
    // Sif Muna
    case ABIL_SIF_MUNA_CHANNEL_ENERGY:
        return TILEG_ABILITY_SIF_MUNA_CHANNEL;
    case ABIL_SIF_MUNA_FORGET_SPELL:
        return TILEG_ABILITY_SIF_MUNA_AMNESIA;
    case ABIL_SIF_MUNA_DIVINE_EXEGESIS:
        return TILEG_ABILITY_SIF_MUNA_EXEGESIS;
    // Trog
    case ABIL_TROG_BERSERK:
        return TILEG_ABILITY_TROG_BERSERK;
    case ABIL_TROG_HAND:
        return TILEG_ABILITY_TROG_HAND;
    case ABIL_TROG_BROTHERS_IN_ARMS:
        return TILEG_ABILITY_TROG_BROTHERS_IN_ARMS;
    // Elyvilon
    case ABIL_ELYVILON_PURIFICATION:
        return TILEG_ABILITY_ELYVILON_PURIFICATION;
    case ABIL_ELYVILON_HEAL_SELF:
        return TILEG_ABILITY_ELYVILON_HEAL_SELF;
    case ABIL_ELYVILON_HEAL_OTHER:
        return TILEG_ABILITY_ELYVILON_HEAL_OTHER;
    case ABIL_ELYVILON_DIVINE_VIGOUR:
        return TILEG_ABILITY_ELYVILON_DIVINE_VIGOUR;
    // Lugonu
    case ABIL_LUGONU_ABYSS_EXIT:
        return TILEG_ABILITY_LUGONU_EXIT_ABYSS;
    case ABIL_LUGONU_BANISH:
        return TILEG_ABILITY_LUGONU_BANISH;
    case ABIL_LUGONU_CORRUPT:
        return TILEG_ABILITY_LUGONU_CORRUPT;
    case ABIL_LUGONU_ABYSS_ENTER:
        return TILEG_ABILITY_LUGONU_ENTER_ABYSS;
    case ABIL_LUGONU_BLESS_WEAPON:
        return TILEG_ABILITY_LUGONU_BLESS_WEAPON;
    // Nemelex
    case ABIL_NEMELEX_TRIPLE_DRAW:
        return TILEG_ABILITY_NEMELEX_TRIPLE_DRAW;
    case ABIL_NEMELEX_DEAL_FOUR:
        return TILEG_ABILITY_NEMELEX_DEAL_FOUR;
    case ABIL_NEMELEX_STACK_FIVE:
        return TILEG_ABILITY_NEMELEX_STACK_FIVE;
    case ABIL_NEMELEX_DRAW_ESCAPE:
        return TILEG_ABILITY_NEMELEX_DRAW_ESCAPE;
    case ABIL_NEMELEX_DRAW_DESTRUCTION:
        return TILEG_ABILITY_NEMELEX_DRAW_DESTRUCTION;
    case ABIL_NEMELEX_DRAW_SUMMONING:
        return TILEG_ABILITY_NEMELEX_DRAW_SUMMONING;
    case ABIL_NEMELEX_DRAW_STACK:
        return TILEG_ABILITY_NEMELEX_DRAW_STACK;
    // Beogh
    case ABIL_BEOGH_SMITING:
        return TILEG_ABILITY_BEOGH_SMITE;
    case ABIL_BEOGH_RECALL_APOSTLES:
        return TILEG_ABILITY_BEOGH_RECALL;
    case ABIL_CONVERT_TO_BEOGH:
        return TILEG_ABILITY_CONVERT_TO_BEOGH;
    case ABIL_BEOGH_RECRUIT_APOSTLE:
        return TILEG_ABILITY_BEOGH_RECRUIT_APOSTLE;
    case ABIL_BEOGH_DISMISS_APOSTLE_1:
        if (!beogh_apostle_is_alive(1))
            return TILEG_ABILITY_BEOGH_DISMISS_APOSTLE_DEAD;
        else
            return TILEG_ABILITY_BEOGH_DISMISS_APOSTLE;
    case ABIL_BEOGH_DISMISS_APOSTLE_2:
        if (!beogh_apostle_is_alive(2))
            return TILEG_ABILITY_BEOGH_DISMISS_APOSTLE_DEAD;
        else
            return TILEG_ABILITY_BEOGH_DISMISS_APOSTLE;
    case ABIL_BEOGH_DISMISS_APOSTLE_3:
        if (!beogh_apostle_is_alive(3))
            return TILEG_ABILITY_BEOGH_DISMISS_APOSTLE_DEAD;
        else
            return TILEG_ABILITY_BEOGH_DISMISS_APOSTLE;
    case ABIL_BEOGH_BLOOD_FOR_BLOOD:
        return TILEG_ABILITY_BEOGH_BLOOD_FOR_BLOOD;
    // Jiyva
    case ABIL_JIYVA_OOZEMANCY:
        return TILEG_ABILITY_JIYVA_OOZEMANCY;
    case ABIL_JIYVA_SLIMIFY:
        return TILEG_ABILITY_JIYVA_SLIMIFY;
    // Fedhas
    case ABIL_FEDHAS_WALL_OF_BRIARS:
        return TILEG_ABILITY_FEDHAS_WALL_OF_BRIARS;
    case ABIL_FEDHAS_GROW_BALLISTOMYCETE:
        return TILEG_ABILITY_FEDHAS_GROW_BALLISTOMYCETE;
    case ABIL_FEDHAS_OVERGROW:
        return TILEG_ABILITY_FEDHAS_OVERGROW;
    case ABIL_FEDHAS_GROW_OKLOB:
        return TILEG_ABILITY_FEDHAS_GROW_OKLOB;
    // Cheibriados
    case ABIL_CHEIBRIADOS_TIME_STEP:
        return TILEG_ABILITY_CHEIBRIADOS_TIME_STEP;
    case ABIL_CHEIBRIADOS_TIME_BEND:
        return TILEG_ABILITY_CHEIBRIADOS_BEND_TIME;
    case ABIL_CHEIBRIADOS_SLOUCH:
        return TILEG_ABILITY_CHEIBRIADOS_SLOUCH;
    case ABIL_CHEIBRIADOS_DISTORTION:
        return TILEG_ABILITY_CHEIBRIADOS_TEMPORAL_DISTORTION;
    // Ashenzari
    case ABIL_ASHENZARI_CURSE:
        return TILEG_ABILITY_ASHENZARI_CURSE;
    case ABIL_ASHENZARI_UNCURSE:
        return TILEG_ABILITY_ASHENZARI_UNCURSE;
    // Dithmenos
    case ABIL_DITHMENOS_SHADOWSLIP:
        return TILEG_ABILITY_DITHMENOS_SHADOWSLIP;
    case ABIL_DITHMENOS_APHOTIC_MARIONETTE:
        return TILEG_ABILITY_DITHMENOS_PANTOMIME;
    case ABIL_DITHMENOS_PRIMORDIAL_NIGHTFALL:
        return TILEG_ABILITY_DITHMENOS_NIGHTFALL;
    // Gozag
    case ABIL_GOZAG_POTION_PETITION:
        return TILEG_ABILITY_GOZAG_POTION_PETITION;
    case ABIL_GOZAG_CALL_MERCHANT:
        return TILEG_ABILITY_GOZAG_CALL_MERCHANT;
    case ABIL_GOZAG_BRIBE_BRANCH:
        return TILEG_ABILITY_GOZAG_BRIBE_BRANCH;
    // Qazlal
    case ABIL_QAZLAL_UPHEAVAL:
        return TILEG_ABILITY_QAZLAL_UPHEAVAL;
    case ABIL_QAZLAL_ELEMENTAL_FORCE:
        return TILEG_ABILITY_QAZLAL_ELEMENTAL_FORCE;
    case ABIL_QAZLAL_DISASTER_AREA:
        return TILEG_ABILITY_QAZLAL_DISASTER_AREA;
    // Ru
    case ABIL_RU_DRAW_OUT_POWER:
        return TILEG_ABILITY_RU_DRAW_OUT_POWER;
    case ABIL_RU_POWER_LEAP:
        return TILEG_ABILITY_RU_POWER_LEAP;
    case ABIL_RU_APOCALYPSE:
        return TILEG_ABILITY_RU_APOCALYPSE;

    case ABIL_RU_SACRIFICE_PURITY:
        return TILEG_ABILITY_RU_SACRIFICE_PURITY;
    case ABIL_RU_SACRIFICE_WORDS:
        return TILEG_ABILITY_RU_SACRIFICE_WORDS;
    case ABIL_RU_SACRIFICE_DRINK:
        return TILEG_ABILITY_RU_SACRIFICE_DRINK;
    case ABIL_RU_SACRIFICE_ESSENCE:
        return TILEG_ABILITY_RU_SACRIFICE_ESSENCE;
    case ABIL_RU_SACRIFICE_HEALTH:
        return TILEG_ABILITY_RU_SACRIFICE_HEALTH;
    case ABIL_RU_SACRIFICE_STEALTH:
        return TILEG_ABILITY_RU_SACRIFICE_STEALTH;
    case ABIL_RU_SACRIFICE_ARTIFICE:
        return TILEG_ABILITY_RU_SACRIFICE_ARTIFICE;
    case ABIL_RU_SACRIFICE_LOVE:
        return TILEG_ABILITY_RU_SACRIFICE_LOVE;
    case ABIL_RU_SACRIFICE_COURAGE:
        return TILEG_ABILITY_RU_SACRIFICE_COURAGE;
    case ABIL_RU_SACRIFICE_ARCANA:
        return TILEG_ABILITY_RU_SACRIFICE_ARCANA;
    case ABIL_RU_SACRIFICE_NIMBLENESS:
        return TILEG_ABILITY_RU_SACRIFICE_NIMBLENESS;
    case ABIL_RU_SACRIFICE_DURABILITY:
        return TILEG_ABILITY_RU_SACRIFICE_DURABILITY;
    case ABIL_RU_SACRIFICE_HAND:
        return TILEG_ABILITY_RU_SACRIFICE_HAND;
    case ABIL_RU_SACRIFICE_EXPERIENCE:
        return TILEG_ABILITY_RU_SACRIFICE_EXPERIENCE;
    case ABIL_RU_SACRIFICE_SKILL:
        return TILEG_ABILITY_RU_SACRIFICE_SKILL;
    case ABIL_RU_SACRIFICE_EYE:
        return TILEG_ABILITY_RU_SACRIFICE_EYE;
    case ABIL_RU_SACRIFICE_RESISTANCE:
        return TILEG_ABILITY_RU_SACRIFICE_RESISTANCE;
    case ABIL_RU_SACRIFICE_FORMS:
        return TILEG_ABILITY_RU_SACRIFICE_FORMS;
    case ABIL_RU_REJECT_SACRIFICES:
        return TILEG_ABILITY_RU_REJECT_SACRIFICES;
    // Hepliaklqana
    case ABIL_HEPLIAKLQANA_RECALL:
        return TILEG_ABILITY_HEP_RECALL;
    case ABIL_HEPLIAKLQANA_IDEALISE:
        return TILEG_ABILITY_HEP_IDEALISE;
    case ABIL_HEPLIAKLQANA_TRANSFERENCE:
        return TILEG_ABILITY_HEP_TRANSFERENCE;
    case ABIL_HEPLIAKLQANA_IDENTITY:
        return TILEG_ABILITY_HEP_IDENTITY;
    case ABIL_HEPLIAKLQANA_TYPE_KNIGHT:
        return TILEG_ABILITY_HEP_KNIGHT;
    case ABIL_HEPLIAKLQANA_TYPE_BATTLEMAGE:
        return TILEG_ABILITY_HEP_BATTLEMAGE;
    case ABIL_HEPLIAKLQANA_TYPE_HEXER:
        return TILEG_ABILITY_HEP_HEXER;
    // usk
   case ABIL_USKAYAW_STOMP:
        return TILEG_ABILITY_USKAYAW_STOMP;
   case ABIL_USKAYAW_LINE_PASS:
        return TILEG_ABILITY_USKAYAW_LINE_PASS;
   case ABIL_USKAYAW_GRAND_FINALE:
        return TILEG_ABILITY_USKAYAW_GRAND_FINALE;
     // Wu Jian
    case ABIL_WU_JIAN_WALLJUMP:
        return TILEG_ABILITY_WU_JIAN_WALL_JUMP;
    case ABIL_WU_JIAN_SERPENTS_LASH:
        return TILEG_ABILITY_WU_JIAN_SERPENTS_LASH;
    case ABIL_WU_JIAN_HEAVENLY_STORM:
        return TILEG_ABILITY_WU_JIAN_HEAVENLY_STORM;
    // Ignis
    case ABIL_IGNIS_FIERY_ARMOUR:
        return TILEG_ABILITY_IGNIS_FIERY_ARMOUR;
    case ABIL_IGNIS_FOXFIRE:
        return TILEG_ABILITY_IGNIS_FOXFIRE;
    case ABIL_IGNIS_RISING_FLAME:
        return TILEG_ABILITY_IGNIS_RISING_FLAME;

    // General divine (pseudo) abilities.
    case ABIL_RENOUNCE_RELIGION:
        return TILEG_ABILITY_RENOUNCE_RELIGION;

#ifdef WIZARD
    case ABIL_WIZ_BUILD_TERRAIN:
        return TILEG_ABILITY_WIZ_BUILD_TERRAIN;
    case ABIL_WIZ_SET_TERRAIN:
        return TILEG_ABILITY_WIZ_SET_TERRAIN;
    case ABIL_WIZ_CLEAR_TERRAIN:
        return TILEG_ABILITY_WIZ_CLEAR_TERRAIN;
#endif

    default:
        return TILEG_ERROR;
    }
}

tileidx_t tileidx_branch(const branch_type br)
{
    switch (br)
    {
    case BRANCH_DUNGEON:
        return TILE_DNGN_EXIT_DUNGEON;
    case BRANCH_TEMPLE:
        return TILE_DNGN_ENTER_TEMPLE;
    case BRANCH_ORC:
        return TILE_DNGN_ENTER_ORC;
    case BRANCH_ELF:
        return TILE_DNGN_ENTER_ELF;
    case BRANCH_LAIR:
        return TILE_DNGN_ENTER_LAIR;
    case BRANCH_SWAMP:
        return TILE_DNGN_ENTER_SWAMP;
    case BRANCH_SHOALS:
        return TILE_DNGN_ENTER_SHOALS;
    case BRANCH_SNAKE:
        return TILE_DNGN_ENTER_SNAKE;
    case BRANCH_SPIDER:
        return TILE_DNGN_ENTER_SPIDER;
    case BRANCH_SLIME:
        return TILE_DNGN_ENTER_SLIME;
    case BRANCH_VAULTS:
        return TILE_DNGN_ENTER_VAULTS;
    case BRANCH_CRYPT:
        return TILE_DNGN_ENTER_CRYPT;
    case BRANCH_TOMB:
        return TILE_DNGN_ENTER_TOMB;
    case BRANCH_DEPTHS:
        return TILE_DNGN_ENTER_DEPTHS;
    case BRANCH_VESTIBULE:
        return TILE_DNGN_ENTER_HELL;
    case BRANCH_DIS:
        return TILE_DNGN_ENTER_DIS;
    case BRANCH_GEHENNA:
        return TILE_DNGN_ENTER_GEHENNA;
    case BRANCH_COCYTUS:
        return TILE_DNGN_ENTER_COCYTUS;
    case BRANCH_TARTARUS:
        return TILE_DNGN_ENTER_TARTARUS;
    case BRANCH_ZOT:
        return TILE_DNGN_ENTER_ZOT_OPEN;
    case BRANCH_ABYSS:
        return TILE_DNGN_ENTER_ABYSS;
    case BRANCH_PANDEMONIUM:
        return TILE_DNGN_ENTER_PANDEMONIUM;
    case BRANCH_ZIGGURAT:
        return TILE_DNGN_PORTAL_ZIGGURAT;
    case BRANCH_GAUNTLET:
        return TILE_DNGN_PORTAL_GAUNTLET;
    case BRANCH_BAZAAR:
        return TILE_DNGN_PORTAL_BAZAAR;
    case BRANCH_TROVE:
        return TILE_DNGN_PORTAL_TROVE;
    case BRANCH_SEWER:
        return TILE_DNGN_PORTAL_SEWER;
    case BRANCH_OSSUARY:
        return TILE_DNGN_PORTAL_OSSUARY;
    case BRANCH_BAILEY:
        return TILE_DNGN_PORTAL_BAILEY;
    case BRANCH_ICE_CAVE:
        return TILE_DNGN_PORTAL_ICE_CAVE;
    case BRANCH_VOLCANO:
        return TILE_DNGN_PORTAL_VOLCANO;
    case BRANCH_WIZLAB:
        return TILE_DNGN_PORTAL_WIZARD_LAB_7; /* I like this colour */
    case BRANCH_DESOLATION:
        return TILE_DNGN_PORTAL_DESOLATION;
    case BRANCH_ARENA:
        return TILE_DNGN_ALTAR_OKAWARU;
    case BRANCH_CRUCIBLE:
        return TILE_DNGN_ALTAR_MAKHLEB;

    default:
        return TILEG_ERROR;
    }
}

static tileidx_t _tileidx_player_job_base(const job_type job)
{
    switch (job)
    {
        case JOB_FIGHTER:
            return TILEG_JOB_FIGHTER;
        case JOB_HEDGE_WIZARD:
            return TILEG_JOB_HEDGE_WIZARD;
        case JOB_GLADIATOR:
            return TILEG_JOB_GLADIATOR;
        case JOB_NECROMANCER:
            return TILEG_JOB_NECROMANCER;
        case JOB_BRIGAND:
            return TILEG_JOB_BRIGAND;
        case JOB_BERSERKER:
            return TILEG_JOB_BERSERKER;
        case JOB_HUNTER:
            return TILEG_JOB_HUNTER;
        case JOB_CONJURER:
            return TILEG_JOB_CONJURER;
        case JOB_ENCHANTER:
            return TILEG_JOB_ENCHANTER;
        case JOB_FIRE_ELEMENTALIST:
            return TILEG_JOB_FIRE_ELEMENTALIST;
        case JOB_ICE_ELEMENTALIST:
            return TILEG_JOB_ICE_ELEMENTALIST;
        case JOB_SUMMONER:
            return TILEG_JOB_SUMMONER;
        case JOB_FORGEWRIGHT:
            return TILEG_JOB_FORGEWRIGHT;
        case JOB_AIR_ELEMENTALIST:
            return TILEG_JOB_AIR_ELEMENTALIST;
        case JOB_EARTH_ELEMENTALIST:
            return TILEG_JOB_EARTH_ELEMENTALIST;
        case JOB_ALCHEMIST:
            return TILEG_JOB_ALCHEMIST;
        case JOB_CHAOS_KNIGHT:
            return TILEG_JOB_CHAOS_KNIGHT;
        case JOB_SHAPESHIFTER:
            return TILEG_JOB_SHAPESHIFTER;
        case JOB_MONK:
            return TILEG_JOB_MONK;
        case JOB_WARPER:
            return TILEG_JOB_WARPER;
        case JOB_WANDERER:
            return TILEG_JOB_WANDERER;
        case JOB_ARTIFICER:
            return TILEG_JOB_ARTIFICER;
        case JOB_DELVER:
            return TILEG_JOB_DELVER;
        case JOB_HEXSLINGER:
            return TILEG_JOB_HEXSLINGER;
        case JOB_REAVER:
            return TILEG_JOB_REAVER;
        case JOB_CINDER_ACOLYTE:
            return TILEG_JOB_CINDER_ACOLYTE;
        default:
            return TILEG_ERROR;
    }
}

static tileidx_t _tileidx_player_species_base(const species_type species)
{
    switch (species)
    {
        case SP_HUMAN:
#if TAG_MAJOR_VERSION == 34
        case SP_DEEP_DWARF:
        case SP_HILL_ORC:
#endif
            return TILEG_SP_HUMAN;
        case SP_MOUNTAIN_DWARF:
            return TILEG_SP_MOUNTAIN_DWARF;
        case SP_DEEP_ELF:
            return TILEG_SP_DEEP_ELF;
        case SP_KOBOLD:
            return TILEG_SP_KOBOLD;
        case SP_MUMMY:
            return TILEG_SP_MUMMY;
        case SP_NAGA:
            return TILEG_SP_NAGA;
        case SP_ONI:
            return TILEG_SP_ONI;
        case SP_TROLL:
            return TILEG_SP_TROLL;
        case SP_BASE_DRACONIAN:
            return TILEG_SP_DRACONIAN;
        case SP_ARMATAUR:
            return TILEG_SP_ARMATAUR;
        case SP_DEMIGOD:
            return TILEG_SP_DEMIGOD;
        case SP_SPRIGGAN:
            return TILEG_SP_SPRIGGAN;
        case SP_MINOTAUR:
            return TILEG_SP_MINOTAUR;
        case SP_DEMONSPAWN:
            return TILEG_SP_DEMONSPAWN;
        case SP_GHOUL:
            return TILEG_SP_GHOUL;
        case SP_TENGU:
#if TAG_MAJOR_VERSION == 34
        case SP_MAYFLYTAUR:
#endif
            return TILEG_SP_TENGU;
        case SP_MERFOLK:
            return TILEG_SP_MERFOLK;
        case SP_VAMPIRE:
            return TILEG_SP_VAMPIRE;
        case SP_FELID:
            return TILEG_SP_FELID;
        case SP_OCTOPODE:
            return TILEG_SP_OCTOPODE;
        case SP_GARGOYLE:
            return TILEG_SP_GARGOYLE;
        case SP_FORMICID:
            return TILEG_SP_FORMICID;
        case SP_VINE_STALKER:
            return TILEG_SP_VINE_STALKER;
        case SP_BARACHI:
            return TILEG_SP_BARACHI;
        case SP_GNOLL:
            return TILEG_SP_GNOLL;
        case SP_DJINNI:
            return TILEG_SP_DJINNI;
#if TAG_MAJOR_VERSION == 34
        case SP_METEORAN:
            return TILEG_SP_METEORAN;
#endif
        case SP_COGLIN:
            return TILEG_SP_COGLIN;
        default:
            return TILEP_ERROR;
    }
}

tileidx_t tileidx_player_species(const species_type species, bool recommended)
{
    tileidx_t off = recommended ? -TILEG_LAST_SPECIES+TILEG_LAST_RECOMMENDED_SPECIES: 0;
    return _tileidx_player_species_base(species) + off;
}

tileidx_t tileidx_player_job(const job_type job, bool recommended)
{
    tileidx_t off = recommended ? -TILEG_LAST_JOB+TILEG_LAST_RECOMMENDED_JOB: 0;
    return _tileidx_player_job_base(job) + off;
}

tileidx_t tileidx_known_brand(const item_def &item)
{
    if (!item.is_identified())
        return 0;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
            const int brand = get_weapon_brand(item);
            // XXX HACK: The weapon enums list is a scattered mess of removed
            // brands presuming a messy false fixed order, and bleeds into
            // missile brands otherwise. Maybe this should just be a full
            // switch case fix-up instead.
            if (brand == SPWPN_ACID)
                return TILE_BRAND_ACID;
            else if (brand == SPWPN_FOUL_FLAME)
                return TILE_BRAND_FOUL_FLAME;
            else if (brand != SPWPN_NORMAL)
                return TILE_BRAND_WEP_FIRST + get_weapon_brand(item) - 1;
            break;
        }
    case OBJ_ARMOUR:
        {
            const int brand = get_armour_ego_type(item);
            if (brand != SPARM_NORMAL)
                return TILE_BRAND_ARM_FIRST + get_armour_ego_type(item) - 1;
            break;
        }
    case OBJ_MISSILES:
        switch (get_ammo_brand(item))
        {
#if TAG_MAJOR_VERSION == 34
        case SPMSL_FLAME:
            return TILE_BRAND_FLAME;
        case SPMSL_FROST:
            return TILE_BRAND_FROST;
#endif
        case SPMSL_POISONED:
            return TILE_BRAND_POISONED;
        case SPMSL_CURARE:
            return TILE_BRAND_CURARE;
#if TAG_MAJOR_VERSION == 34
        case SPMSL_RETURNING:
            return TILE_BRAND_RETURNING;
#endif
        case SPMSL_CHAOS:
            return TILE_BRAND_CHAOS;
#if TAG_MAJOR_VERSION == 34
        case SPMSL_PENETRATION:
            return TILE_BRAND_PENETRATION;
#endif
        case SPMSL_DISPERSAL:
        case SPMSL_DISJUNCTION:
            return TILE_BRAND_DISPERSAL;
        case SPMSL_EXPLODING:
            return TILE_BRAND_EXPLOSION;
#if TAG_MAJOR_VERSION == 34
        case SPMSL_CONFUSION:
            return TILE_BRAND_CONFUSION;
        case SPMSL_PARALYSIS:
            return TILE_BRAND_PARALYSIS;
        case SPMSL_SLOW:
            return TILE_BRAND_SLOWING;
        case SPMSL_SICKNESS:
            return TILE_BRAND_SICKNESS;
        case SPMSL_SLEEP:
            return TILE_BRAND_SLEEP;
#endif
        case SPMSL_FRENZY:
            return TILE_BRAND_FRENZY;
        case SPMSL_BLINDING:
            return TILE_BRAND_BLINDING;
        default:
            break;
        }
        break;
    case OBJ_STAVES:
        if (is_random_artefact(item)) // normal staff brands handled elsewhere
            return TILE_STAFF_ID_FIRST + item.sub_type;
        break;
    #if TAG_MAJOR_VERSION == 34
    case OBJ_RODS:
        // Technically not a brand, but still handled here
        return TILE_ROD_ID_FIRST + item.sub_type;
    #endif
    default:
        break;
    }
    return 0;
}

#ifdef USE_TILE
tileidx_t tileidx_unseen_flag(const coord_def &gc)
{
    if (!map_bounds(gc))
        return TILE_FLAG_UNSEEN;
    else if (env.map_knowledge(gc).known()
                && !env.map_knowledge(gc).seen()
             || env.map_knowledge(gc).detected_item()
             || env.map_knowledge(gc).detected_monster()
           )
    {
        return TILE_FLAG_MM_UNSEEN;
    }
    else
        return TILE_FLAG_UNSEEN;
}
# endif

int enchant_to_int(const item_def &item)
{
    if (is_random_artefact(item))
        return 4;

    // Dragon scales and troll hides can't have egos.
    // Only apply their special tiles to randarts.
    if (armour_is_hide(item))
        return 0;

    switch (item.flags & ISFLAG_COSMETIC_MASK)
    {
        default:
            return 0;
        case ISFLAG_EMBROIDERED_SHINY:
            return 1;
        case ISFLAG_RUNED:
            return 2;
        case ISFLAG_GLOWING:
            return 3;
    }
}

tileidx_t tileidx_enchant_equ(const item_def &item, tileidx_t tile, bool player)
{
    static const int etable[5][5] =
    {
      {0, 0, 0, 0, 0},  // all variants look the same
      {0, 1, 1, 1, 1},  // normal, ego/randart
      {0, 1, 1, 1, 2},  // normal, ego, randart
      {0, 1, 1, 2, 3},  // normal, ego (shiny/runed), ego (glowing), randart
      {0, 1, 2, 3, 4}   // normal, shiny, runed, glowing, randart
    };

    const int etype = enchant_to_int(item);

    // XXX: only helmets, hats, orbs, robes and boots have variants, but it would be nice
    // if this weren't hardcoded.
    if (tile == TILE_THELM_HAT)
    {
        switch (etype)
        {
            case 1:
            case 2:
            case 3:
                tile = _modrng(item.rnd, TILE_THELM_HAT_EGO_FIRST, TILE_THELM_HAT_EGO_LAST);
                break;
            case 4:
                if (item.rnd % 2 && december_holidays())
                    tile = TILE_THELM_HAT_SANTA;
                else
                    tile = _modrng(item.rnd, TILE_THELM_HAT_ART_FIRST, TILE_THELM_HAT_ART_LAST);
                break;
            default:
                tile = _modrng(item.rnd, TILE_THELM_HAT_FIRST, TILE_THELM_HAT_LAST);
        }
    }

    if (tile == TILE_THELM_HELM)
    {
        switch (etype)
        {
            case 1:
            case 2:
            case 3:
                tile = _modrng(item.rnd, TILE_THELM_EGO_FIRST, TILE_THELM_EGO_LAST);
                break;
            case 4:
                tile = _modrng(item.rnd, TILE_THELM_ART_FIRST, TILE_THELM_ART_LAST);
                break;
            default:
                tile = _modrng(item.rnd, TILE_THELM_FIRST, TILE_THELM_LAST);
        }
        return tile;
    }

    if (tile == TILE_ARM_ORB && etype == 4)
        return _modrng(item.rnd, TILE_ARM_ORB_ART_FIRST, TILE_ARM_ORB_ART_LAST);

    if (tile == TILE_ARM_ROBE)
    {
        switch (etype)
        {
            case 1:
            case 2:
            case 3:
                tile = _modrng(item.rnd, TILE_ARM_ROBE_EGO_FIRST, TILE_ARM_ROBE_EGO_LAST);
                break;
            case 4:
                tile = _modrng(item.rnd, TILE_ARM_ROBE_ART_FIRST, TILE_ARM_ROBE_ART_LAST);
                break;
            default:
                tile = _modrng(item.rnd, TILE_ARM_ROBE_FIRST, TILE_ARM_ROBE_LAST);
        }
        return tile;
    }

    if (tile == TILE_ARM_BOOTS)
    {
        switch (etype)
        {
            case 1:
            case 2:
            case 3:
                tile = _modrng(item.rnd, TILE_ARM_BOOTS_EGO_FIRST, TILE_ARM_BOOTS_EGO_LAST);
                break;
            case 4:
                tile = _modrng(item.rnd, TILE_ARM_BOOTS_ART_FIRST, TILE_ARM_BOOTS_ART_LAST);
                break;
            default:
                tile = _modrng(item.rnd, TILE_ARM_BOOTS_FIRST, TILE_ARM_BOOTS_LAST);
        }
        return tile;
    }

    int idx;
    if (player)
        idx = tile_player_count(tile) - 1;
    else
        idx = tile_main_count(tile) - 1;
    ASSERT(idx < 5);

    tile += etable[idx][etype];

    return tile;
}

#ifdef USE_TILE
string tile_debug_string(tileidx_t fg, tileidx_t bg, char prefix)
{
    tileidx_t fg_idx = fg & TILE_FLAG_MASK;
    tileidx_t bg_idx = bg & TILE_FLAG_MASK;

    string fg_name;
    if (fg_idx < TILE_FLOOR_MAX)
        fg_name = tile_floor_name(fg_idx);
    else if (fg_idx < TILE_WALL_MAX)
        fg_name = tile_wall_name(fg_idx);
    else if (fg_idx < TILE_FEAT_MAX)
        fg_name = tile_feat_name(fg_idx);
    else if (fg_idx < TILE_DNGN_MAX)
        fg_name = tile_dngn_name(fg_idx);
    else if (fg_idx < TILE_MAIN_MAX)
        fg_name = tile_main_name(fg_idx);
    else if (fg_idx < TILEP_MCACHE_START)
        fg_name = (tile_player_name(fg_idx));
    else
    {
        fg_name = "mc:";
        mcache_entry *entry = mcache.get(fg_idx);
        if (entry)
        {
            tile_draw_info dinfo[mcache_entry::MAX_INFO_COUNT];
            unsigned int count = entry->info(&dinfo[0]);
            for (unsigned int i = 0; i < count; ++i)
            {
                tileidx_t mc_idx = dinfo[i].idx;
                if (mc_idx < TILE_MAIN_MAX)
                    fg_name += tile_main_name(mc_idx);
                else if (mc_idx < TILEP_PLAYER_MAX)
                    fg_name += tile_player_name(mc_idx);
                else
                    fg_name += "[invalid index]";

                if (i < count - 1)
                    fg_name += ", ";
            }
        }
        else
            fg_name += "[not found]";
    }

    string tile_string = make_stringf(
        "%cFG: %4" PRIu64" | 0x%8" PRIx64" (%s)\n"
        "%cBG: %4" PRIu64" | 0x%8" PRIx64" (%s)\n",
        prefix,
        fg_idx,
        fg & ~TILE_FLAG_MASK,
        fg_name.c_str(),
        prefix,
        bg_idx,
        bg & ~TILE_FLAG_MASK,
        tile_dngn_name(bg_idx));

    return tile_string;
}
#endif // USE_TILE

void bind_item_tile(item_def &item)
{
    if (item.props.exists(ITEM_TILE_NAME_KEY))
    {
        string tile = item.props[ITEM_TILE_NAME_KEY].get_string();
        dprf("Binding non-worn item tile: \"%s\".", tile.c_str());
        tileidx_t index;
        if (!tile_main_index(tile.c_str(), &index))
        {
            // If invalid tile name, complain and discard the props.
            dprf("bad tile name: \"%s\".", tile.c_str());
            item.props.erase(ITEM_TILE_NAME_KEY);
            item.props.erase(ITEM_TILE_KEY);
        }
        else
            item.props[ITEM_TILE_KEY] = short(index);
    }

    if (item.props.exists(WORN_TILE_NAME_KEY))
    {
        string tile = item.props[WORN_TILE_NAME_KEY].get_string();
        dprf("Binding worn item tile: \"%s\".", tile.c_str());
        tileidx_t index;
        if (!tile_player_index(tile.c_str(), &index))
        {
            // If invalid tile name, complain and discard the props.
            dprf("bad tile name: \"%s\".", tile.c_str());
            item.props.erase(WORN_TILE_NAME_KEY);
            item.props.erase(WORN_TILE_KEY);
        }
        else
            item.props[WORN_TILE_KEY] = short(index);
    }
}

void tile_init_props(monster* mon)
{
    // Only monsters using mon_mod or mon_cycle need a tile_num.
    switch (get_mon_tile_variation(mon->type))
    {
        case TVARY_MOD:
        case TVARY_CYCLE:
            break;
        default:
            return;
    }

    // Already overridden or set.
    if (mon->props.exists(MONSTER_TILE_KEY) || mon->props.exists(TILE_NUM_KEY))
        return;

    mon->props[TILE_NUM_KEY] = short(random2(256));
}
