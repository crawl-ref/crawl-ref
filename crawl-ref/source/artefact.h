/**
 * @file
 * @brief Random and unrandom artefact functions.
**/

#pragma once

#include "artefact-prop-type.h"
#include "unique-item-status-type.h"

#define ART_PROPERTIES ARTP_NUM_PROPERTIES

#define ARTEFACT_PROPS_KEY  "artefact_props"
#define ARTEFACT_NAME_KEY   "artefact_name"
#define ARTEFACT_APPEAR_KEY "artefact_appearance"
#define FIXED_PROPS_KEY     "artefact_fixed_props"

#define DAMNATION_BOLT_KEY "damnation_bolt"
#define EMBRACE_ARMOUR_KEY "embrace_armour"
#define VICTORY_STAT_KEY    "victory_stat"
#define VICTORY_CONDUCT_KEY "victory_conduct"

struct bolt;

enum unrand_flag_type
{
    UNRAND_FLAG_NONE             = 0x00,
    UNRAND_FLAG_SPECIAL          = 0x01,
    UNRAND_FLAG_HOLY             = 0x02,
                              // = 0x04,  // was UNRAND_FLAG_UNHOLY
    UNRAND_FLAG_EVIL             = 0x08,
    UNRAND_FLAG_UNCLEAN          = 0x10,
    UNRAND_FLAG_CHAOTIC          = 0x20,
                              // = 0x40,  // was UNRAND_FLAG_CORPSE_VIOLATING
    UNRAND_FLAG_NOGEN            = 0x80,
                              // =0x100,  // was UNRAND_FLAG_RANDAPP
    UNRAND_FLAG_UNIDED           =0x200,
    UNRAND_FLAG_SKIP_EGO         =0x400,
    UNRAND_FLAG_DELETED          =0x800,
    // Please make sure it fits in unrandart_entry.flags (currently 16 bits).
};

struct unrandart_entry
{
    const char *name;        // true name of unrandart
    const char *unid_name;   // un-id'd name of unrandart
    const char *type_name;   // custom item type
    const char *inscrip;     // extra inscription
    const char *dbrand;      // description of extra brand
    const char *descrip;     // description of extra power

    object_class_type base_type;
    uint8_t           sub_type;
    object_class_type fallback_base_type;
    uint8_t           fallback_sub_type;
    int               fallback_brand;
    short             plus;
    short             plus2;
    colour_t          colour;

    short         value;
    uint16_t      flags;

    short prpty[ART_PROPERTIES];

    void (*equip_func)(item_def* item, bool* show_msgs, bool unmeld);
    void (*unequip_func)(item_def* item, bool* show_msgs);
    void (*world_reacts_func)(item_def* item);
    void (*melee_effects)(item_def* item, actor* attacker,
                          actor* defender, bool mondied, int damage);
    void (*launch)(bolt* beam);
    void (*death_effects)(item_def* item, monster* mons, killer_type killer);
};

bool is_artefact(const item_def &item);
bool is_random_artefact(const item_def &item);
bool is_unrandom_artefact(const item_def &item, int which = 0);
bool is_special_unrandom_artefact(const item_def &item);

void artefact_fixup_props(item_def &item);

unique_item_status_type get_unique_item_status(int unrand_index);
void set_unique_item_status(const item_def& item,
                            unique_item_status_type status);

string get_artefact_base_name(const item_def &item, bool terse = false);
string get_artefact_name(const item_def &item, bool force_known = false);

void set_artefact_name(item_def &item, const string &name);

string make_artefact_name(const item_def &item, bool appearance = false);
string replace_name_parts(const string &name_in, const item_def& item);

int find_okay_unrandart(uint8_t aclass, uint8_t atype, int item_level,
                        bool in_abyss);

typedef FixedVector< int, ART_PROPERTIES >  artefact_properties_t;

void artefact_desc_properties(const item_def         &item,
                              artefact_properties_t  &proprt);

void artefact_properties(const item_def &item,
                              artefact_properties_t  &proprt);

int artefact_property(const item_def &item, artefact_prop_type prop);

bool make_item_randart(item_def &item, bool force_mundane = false);
void make_ashenzari_randart(item_def &item);
bool make_item_unrandart(item_def &item, int unrand_index);
void setup_unrandart(item_def &item, bool creating = true);

void fill_gizmo_properties(CrawlVector& gizmos);

bool randart_is_bad(const item_def &item);
bool randart_is_bad(const item_def &item, artefact_properties_t &proprt);

int find_unrandart_index(const item_def& artefact);

const unrandart_entry* get_unrand_entry(int unrand_index);

void artefact_set_property(item_def           &item,
                           artefact_prop_type  prop,
                           int                 val);

/// Type for the value of an artefact property
enum artefact_value_type
{
    ARTP_VAL_BOOL,  ///< bool (e.g. Fly)
    ARTP_VAL_POS,   ///< Positive integer (e.g. x% chance to get angry)
    ARTP_VAL_BRAND, ///< Brand (e.g. flaming, heavy).
                    ///      See \ref brand_type in item-prop-enum.h
    ARTP_VAL_ANY,   ///< int (e.g. dex-4, AC+4, SH+8)
};
artefact_value_type artp_value_type(artefact_prop_type prop);
bool artp_value_is_valid(artefact_prop_type prop, int value);

const char *artp_name(artefact_prop_type prop);
artefact_prop_type artp_type_from_name(const string &name);
bool artp_potentially_good(artefact_prop_type prop);
bool artp_potentially_bad(artefact_prop_type prop);

int get_unrandart_num(const char *name);
int extant_unrandart_by_exact_name(string name);

void unrand_reacts();
void unrand_death_effects(monster* mons, killer_type killer);

bool item_type_can_be_artefact(object_class_type typ);

bool are_fixed_props_ok(item_def& item);
