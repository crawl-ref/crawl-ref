/**
 * @file
 * @brief Throwing and launching stuff.
**/

#ifndef THROW_H
#define THROW_H

#include <string>
#include "externs.h"
#include "enum.h"

struct bolt;

enum fire_type
{
    FIRE_NONE      = 0x0000,
    FIRE_LAUNCHER  = 0x0001,
    FIRE_DART      = 0x0002,
    FIRE_STONE     = 0x0004,
    FIRE_DAGGER    = 0x0008,
    FIRE_JAVELIN   = 0x0010,
    FIRE_SPEAR     = 0x0020,
    FIRE_HAND_AXE  = 0x0040,
    FIRE_CLUB      = 0x0080,
    FIRE_ROCK      = 0x0100,
    FIRE_NET       = 0x0200,
    FIRE_RETURNING = 0x0400,
    FIRE_INSCRIBED = 0x0800,   // Only used for _get_fire_order
};

struct bolt;
class dist;

bool elemental_missile_beam(int launcher_brand, int ammo_brand);
bool item_is_quivered(const item_def &item);
bool fire_warn_if_impossible(bool silent = false);
int get_next_fire_item(int current, int offset);
int get_ammo_to_shoot(int item, dist &target, bool teleport = false);
void fire_thing(int item = -1);
void throw_item_no_quiver(void);

bool setup_missile_beam(const actor *actor, bolt &beam, item_def &item,
                        std::string &ammo_name, bool &returning);

void throw_noise(actor* act, const bolt &pbolt, const item_def &ammo);

bool throw_it(bolt &pbolt, int throw_2, bool teleport = false,
              int acc_bonus = 0, dist *target = NULL);

bool thrown_object_destroyed(item_def *item, const coord_def& where);
int launcher_final_speed(const item_def &launcher,
                         const item_def *shield, bool scaled = true);

void setup_monster_throw_beam(monster* mons, struct bolt &beam);
bool mons_throw(monster* mons, struct bolt &beam, int msl);
#endif
