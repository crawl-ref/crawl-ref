#ifndef BRANCHDATA_H
#define BRANCHDATA_H

#include "colour.h"
#include "enum.h"
#include "mon-pick.h"

const Branch branches[NUM_BRANCHES] = {
    // Branch struct:
    //  branch id, parent branch, mindepth, maxdepth, depth, absdepth,
    //  branch flags, level flags
    //  entry stairs, exit stairs, short name, long name, abbrev name
    //  entry message
    //  has_uniques, floor colour, rock colour
    //  mons rarity function, mons level function
    //  travel shortcut, dangerous branch end, ambient noise level

    { BRANCH_MAIN_DUNGEON, NUM_BRANCHES, 0, 0, 27, 0,
      0, 0,
      NUM_FEATURES, DNGN_EXIT_DUNGEON,
      "Dungeon", "the Dungeon", "D",
      NULL,
      true, LIGHTGREY, BROWN,
      'D', false, 0 },

    { BRANCH_ECUMENICAL_TEMPLE, BRANCH_MAIN_DUNGEON, 4, 7, 1, 5,
      BFLAG_NO_ITEMS, 0,
      DNGN_ENTER_TEMPLE, DNGN_RETURN_FROM_TEMPLE,
      "Temple", "the Ecumenical Temple", "Temple",
      NULL,
      false, LIGHTGREY, BROWN,
      'T', false, 0 },

    { BRANCH_ORCISH_MINES, BRANCH_MAIN_DUNGEON, 6, 11, 4, 8,
      0, 0,
      DNGN_ENTER_ORCISH_MINES, DNGN_RETURN_FROM_ORCISH_MINES,
      "Orcish Mines", "the Orcish Mines", "Orc",
      NULL,
      true, BROWN, BROWN,
      'O', false, 4 },

    { BRANCH_ELVEN_HALLS, BRANCH_ORCISH_MINES, 3, 4, 3, 15,
      0, 0,
      DNGN_ENTER_ELVEN_HALLS, DNGN_RETURN_FROM_ELVEN_HALLS,
      "Elven Halls", "the Elven Halls", "Elf",
      NULL,
      true, WHITE, ETC_ELVEN_BRICK,
      'E', true, 0 },

    { BRANCH_DWARVEN_HALL, BRANCH_ELVEN_HALLS, 2, 2, 1, 17,
      0, 0,
      DNGN_ENTER_DWARVEN_HALL, DNGN_RETURN_FROM_DWARVEN_HALL,
      "Dwarven Hall", "the Dwarven Hall", "Dwarf",
      NULL,
      true, BROWN, BROWN,
      'K', false, 0 },

    { BRANCH_LAIR, BRANCH_MAIN_DUNGEON, 8, 13, 8, 10,
      0, 0,
      DNGN_ENTER_LAIR, DNGN_RETURN_FROM_LAIR,
      "Lair", "the Lair of Beasts", "Lair",
      NULL,
      true, GREEN, BROWN,
      'L', false, 4 },

    { BRANCH_SWAMP, BRANCH_LAIR, 3, 6, 5, 15,
      0, 0,
      DNGN_ENTER_SWAMP, DNGN_RETURN_FROM_SWAMP,
      "Swamp", "the Swamp", "Swamp",
      NULL,
      true, BROWN, BROWN,
      'S', true, 0 },

    { BRANCH_SHOALS, BRANCH_LAIR, 3, 6, 5, 15,
      BFLAG_ISLANDED, 0,
      DNGN_ENTER_SHOALS, DNGN_RETURN_FROM_SHOALS,
      "Shoals", "the Shoals", "Shoals",
      NULL,
      true, BROWN, BROWN,
      'A', true, 3 },

    { BRANCH_SNAKE_PIT, BRANCH_LAIR, 3, 6, 5, 15,
      0, 0,
      DNGN_ENTER_SNAKE_PIT, DNGN_RETURN_FROM_SNAKE_PIT,
      "Snake Pit", "the Snake Pit", "Snake",
      NULL,
      true, LIGHTGREEN, YELLOW,
      'P', true, 0 },

    { BRANCH_SPIDER_NEST, BRANCH_LAIR, 3, 6, 5, 15,
      0, 0,
      DNGN_ENTER_SPIDER_NEST, DNGN_RETURN_FROM_SPIDER_NEST,
      "Spider Nest", "the Spider Nest", "Spider",
      NULL,
      true, BROWN, YELLOW,
      'N', true, 0 },

    { BRANCH_SLIME_PITS, BRANCH_LAIR, 6, 8, 6, 17,
      BFLAG_NO_ITEMS, 0,
      DNGN_ENTER_SLIME_PITS, DNGN_RETURN_FROM_SLIME_PITS,
      "Slime Pits", "the Pits of Slime", "Slime",
      NULL,
      true, GREEN, BROWN,
      'M', true, -5 },

    { BRANCH_VAULTS, BRANCH_MAIN_DUNGEON, 15, 20, 5, 19,
      0, 0,
      DNGN_ENTER_VAULTS, DNGN_RETURN_FROM_VAULTS,
      "Vaults", "the Vaults", "Vaults",
      NULL,
      true, LIGHTGREY, BROWN,
      'V', true, 0 },

    { BRANCH_HALL_OF_BLADES, BRANCH_VAULTS, 3, 4, 1, 21,
      BFLAG_NO_ITEMS, 0,
      DNGN_ENTER_HALL_OF_BLADES, DNGN_RETURN_FROM_HALL_OF_BLADES,
      "Hall of Blades", "the Hall of Blades", "Blade",
      NULL,
      true, LIGHTGREY, BROWN,
      'B', false, -7 },

    { BRANCH_CRYPT, BRANCH_VAULTS, 2, 3, 5, 19,
      0, 0,
      DNGN_ENTER_CRYPT, DNGN_RETURN_FROM_CRYPT,
      "Crypt", "the Crypt", "Crypt",
      NULL,
      true, LIGHTGREY, BROWN,
      'C', false, -20 },

    { BRANCH_TOMB, BRANCH_CRYPT, 2, 3, 3, 21,
      BFLAG_ISLANDED, LFLAG_NO_TELE_CONTROL,
      DNGN_ENTER_TOMB, DNGN_RETURN_FROM_TOMB,
      "Tomb", "the Tomb of the Ancients", "Tomb",
      NULL,
      true, BROWN, BROWN,
      'W', true, -10 },

    { BRANCH_VESTIBULE_OF_HELL, NUM_BRANCHES, 27, 27, 1, 27,
      BFLAG_NO_ITEMS, 0,
      DNGN_ENTER_HELL, DNGN_EXIT_HELL, // sentinel
      "Hell", "the Vestibule of Hell", "Hell",
      NULL,
      true, LIGHTGREY, LIGHTRED,
      'U', false, 0 },

    { BRANCH_DIS, BRANCH_VESTIBULE_OF_HELL, 1, 1, 7, 28,
      BFLAG_ISLANDED | BFLAG_NO_ITEMS, 0,
      DNGN_ENTER_DIS, NUM_FEATURES, // sentinel
      "Dis", "the Iron City of Dis", "Dis",
      NULL,
      false, CYAN, BROWN,
      'I', true, 0 },

    { BRANCH_GEHENNA, BRANCH_VESTIBULE_OF_HELL, 1, 1, 7, 28,
      BFLAG_ISLANDED | BFLAG_NO_ITEMS, 0,
      DNGN_ENTER_GEHENNA, NUM_FEATURES, // sentinel
      "Gehenna", "Gehenna", "Geh",
      NULL,
      false, BROWN, RED,
      'G', true, 0 },

    { BRANCH_COCYTUS, BRANCH_VESTIBULE_OF_HELL, 1, 1, 7, 28,
      BFLAG_ISLANDED | BFLAG_NO_ITEMS, 0,
      DNGN_ENTER_COCYTUS, NUM_FEATURES, // sentinel
      "Cocytus", "Cocytus", "Coc",
      NULL,
      false, LIGHTBLUE, LIGHTCYAN,
      'X', true, 0 },

    { BRANCH_TARTARUS, BRANCH_VESTIBULE_OF_HELL, 1, 1, 7, 28,
      BFLAG_ISLANDED | BFLAG_NO_ITEMS, 0,
      DNGN_ENTER_TARTARUS, NUM_FEATURES, // sentinel
      "Tartarus", "Tartarus", "Tar",
      NULL,
      false, MAGENTA, MAGENTA,
      'Y', true, 0 },

    { BRANCH_HALL_OF_ZOT, BRANCH_MAIN_DUNGEON, 27, 27, 5, 27,
      0, 0,
      DNGN_ENTER_ZOT, DNGN_RETURN_FROM_ZOT,
      "Zot", "the Realm of Zot", "Zot",
      NULL,
      true, BLACK, BLACK, // set per-map
      'Z', true, 0 },

    { BRANCH_FOREST, BRANCH_MAIN_DUNGEON, 3, 6, 5, 16,
      0, 0,
      DNGN_ENTER_FOREST, DNGN_RETURN_FROM_FOREST,
      "Forest", "the Enchanted Forest", "Forest",
      NULL,
      true, BROWN, BROWN,
      'F', true, 0 },

    { BRANCH_ABYSS, NUM_BRANCHES, -1, -1, 5, 24,
      BFLAG_NO_XLEV_TRAVEL, LFLAG_NO_TELE_CONTROL | LFLAG_NO_MAP,
      DNGN_ENTER_ABYSS, DNGN_EXIT_ABYSS,
      "Abyss", "the Abyss", "Abyss",
      NULL,
      false, BLACK, BLACK, // set specially
      'J', false, 0 },

    { BRANCH_PANDEMONIUM, NUM_BRANCHES, -1, -1, 1, 24,
      BFLAG_NO_XLEV_TRAVEL, 0,
      DNGN_ENTER_PANDEMONIUM, DNGN_EXIT_PANDEMONIUM,
      "Pandemonium", "Pandemonium", "Pan",
      NULL,
      true, BLACK, BLACK, // set specially from monster set
      'R', false, 0 },

    { BRANCH_ZIGGURAT, NUM_BRANCHES, -1, -1, 27, 27,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS, 0,
      DNGN_ENTER_PORTAL_VAULT, DNGN_EXIT_PORTAL_VAULT,
      "Ziggurat", "a ziggurat", "Zig",
      "You land on top of a ziggurat so tall you cannot make out the ground.",
      false, BLACK, BLACK,
      'Q', false /* maybe, but Zig:26 is almost as bad */, 0 },

    { BRANCH_LABYRINTH, NUM_BRANCHES, -1, -1, 1, 15,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS, LFLAG_NO_TELE_CONTROL | LFLAG_NO_MAP,
      DNGN_ENTER_LABYRINTH, DNGN_ESCAPE_HATCH_UP,
      "Labyrinth", "a labyrinth", "Lab",
      NULL,
      false, BLACK, BLACK,
      '0', false, 0 },

    { BRANCH_BAZAAR, NUM_BRANCHES, -1, -1, 1, 18,
          BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS, 0,
      DNGN_ENTER_PORTAL_VAULT, DNGN_EXIT_PORTAL_VAULT,
      "Bazaar", "a bazaar", "Bazaar",
      "You enter an inter-dimensional bazaar!",
      false, BLUE, YELLOW,
      '1', false, 0 },

    { BRANCH_TROVE, NUM_BRANCHES, -1, -1, 1, 18,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS, 0,
      DNGN_ENTER_PORTAL_VAULT, DNGN_EXIT_PORTAL_VAULT,
      "Trove", "a treasure trove", "Trove",
      NULL,
      false, DARKGREY, BLUE,
      '2', false, 0 },

    { BRANCH_SEWER, NUM_BRANCHES, -1, -1, 1, 4,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS, 0,
      DNGN_ENTER_PORTAL_VAULT, DNGN_EXIT_PORTAL_VAULT,
      "Sewer", "a sewer", "Sewer",
      NULL,
      false, LIGHTGREY, DARKGREY,
      '3', false, 0 },

    { BRANCH_OSSUARY, NUM_BRANCHES, -1, -1, 1, 6,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS, 0,
      DNGN_ENTER_PORTAL_VAULT, DNGN_EXIT_PORTAL_VAULT,
      "Ossuary", "an ossuary", "Ossuary",
      NULL,
      false, WHITE, YELLOW,
      '4', false, 0 },

    { BRANCH_BAILEY, NUM_BRANCHES, -1, -1, 1, 11,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS, 0,
      DNGN_ENTER_PORTAL_VAULT, DNGN_EXIT_PORTAL_VAULT,
      "Bailey", "a bailey", "Bailey",
      NULL,
      false, WHITE, LIGHTGREY,
      '5', false, 0 },

    { BRANCH_ICE_CAVE, NUM_BRANCHES, -1, -1, 1, 15,
          BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS, 0,
      DNGN_ENTER_PORTAL_VAULT, DNGN_EXIT_PORTAL_VAULT,
      "Ice Cave", "an ice cave", "IceCv",
      NULL,
      false, BLUE, WHITE,
      '6', false, 0 },

    { BRANCH_VOLCANO, NUM_BRANCHES, -1, -1, 1, 14,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS, 0,
      DNGN_ENTER_PORTAL_VAULT, DNGN_EXIT_PORTAL_VAULT,
      "Volcano", "a volcano", "Volcano",
      NULL,
      false, RED, RED,
      '7', false, 0 },

    { BRANCH_WIZLAB, NUM_BRANCHES, -1, -1, 1, 24,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS, 0,
      DNGN_ENTER_PORTAL_VAULT, DNGN_EXIT_PORTAL_VAULT,
      "Wizlab", "a wizard's laboratory", "WizLab",
      NULL,
      false, LIGHTGREY, BROWN, // set per-map
      '8', false, 0 },
};

#endif
