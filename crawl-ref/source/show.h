#pragma once

#include "enum.h"
#include "dungeon-feature-type.h"
#include "tag-version.h"

// same order as DCHAR_*
enum show_item_type
{
    SHOW_ITEM_NONE,
    SHOW_ITEM_DETECTED,
    SHOW_ITEM_ORB,
    SHOW_ITEM_RUNE,
    SHOW_ITEM_WEAPON,
    SHOW_ITEM_ARMOUR,
    SHOW_ITEM_WAND,
#if TAG_MAJOR_VERSION == 34
    SHOW_ITEM_FOOD,
#endif
    SHOW_ITEM_SCROLL,
    SHOW_ITEM_RING,
    SHOW_ITEM_POTION,
    SHOW_ITEM_MISSILE,
    SHOW_ITEM_BOOK,
    SHOW_ITEM_STAFF,
#if TAG_MAJOR_VERSION == 34
    SHOW_ITEM_ROD,
#endif
    SHOW_ITEM_TALISMAN,
    SHOW_ITEM_MISCELLANY,
    SHOW_ITEM_CORPSE,
    SHOW_ITEM_SKELETON,
    SHOW_ITEM_GOLD,
    SHOW_ITEM_GEM,
    SHOW_ITEM_AMULET,
    NUM_SHOW_ITEMS
};

enum show_class
{
    SH_NOTHING,
    SH_FEATURE,
    SH_ITEM,
    SH_CLOUD,
    SH_INVIS_EXPOSED,
    SH_MONSTER,
    NUM_SHOW_CLASSES
};

struct show_type
{
    show_class cls;
    dungeon_feature_type feat;
    show_item_type item;
    monster_type mons;
    colour_t colour;

    show_type();
    show_type(dungeon_feature_type f);
    show_type(const item_def &item);
    show_type(show_item_type itemtype);
    show_type(monster_type montype);

    operator bool() const { return cls != SH_NOTHING; }

    bool operator < (const show_type &other) const;
    bool is_cleanable_monster() const;
};

struct show_info
{
    dungeon_feature_type feat;
    show_item_type item;
    monster_type mons;
};

class monster;

enum class Layer
{
    None            = 0,
    MONSTERS        = (1 << 0),
    PLAYER          = (1 << 1),
    ITEMS           = (1 << 2),
    CLOUDS          = (1 << 3),
    MONSTER_WEAPONS = (1 << 4),
    MONSTER_HEALTH  = (1 << 5),
};
DEF_BITFIELD(layers_type, Layer, 5);
constexpr layers_type LAYERS_ALL = Layer::MONSTERS | Layer::PLAYER
                                 | Layer::ITEMS | Layer::CLOUDS;

void show_init(layers_type layers = LAYERS_ALL);
void update_item_at(const coord_def &gp, bool wizard = false);
void show_update_at(const coord_def &gp, layers_type layers = LAYERS_ALL);
void force_show_update_at(const coord_def &gp, layers_type layers = LAYERS_ALL);
void show_update_emphasis();
