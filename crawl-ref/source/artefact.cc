/**
 * @file
 * @brief Random and unrandom artefact functions.
**/

#include "AppHdr.h"

#include "artefact.h"
#include "art-enum.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>

#include "branch.h"
#include "colour.h"
#include "database.h"
#include "english.h"
#include "god-item.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h"
#include "makeitem.h"
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "spl-book.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "transform.h" // form_for_talisman
#include "unicode.h"

// Putting this here since art-enum.h is generated.

// Make sure there's enough room in you.unique_items to hold all
// the unrandarts.
COMPILE_CHECK((NUM_UNRANDARTS < MAX_UNRANDARTS));
// Non-artefact brands and unrandart indexes both go into
// item.special, so make sure they don't overlap.
COMPILE_CHECK(((int) NUM_SPECIAL_WEAPONS < (int) UNRAND_START));

static bool _god_fits_artefact(const god_type which_god, const item_def &item,
                               bool name_check_only = false)
{
    if (which_god == GOD_NO_GOD)
        return false;

    // Jellies can't eat artefacts, so their god won't make any.
    if (which_god == GOD_JIYVA)
        return false;

    // First check the item's base_type and sub_type, then check the
    // item's brand and other randart properties.

    const bool type_bad = !god_likes_item_type(item, which_god);

    if (type_bad && !name_check_only)
    {
        die("%s attempting to gift invalid type of item.",
            god_name(which_god).c_str());
    }

    if (type_bad)
        return false;

    const int brand = get_weapon_brand(item);
    const int ego   = get_armour_ego_type(item);

    if (is_evil_god(which_god) && brand == SPWPN_HOLY_WRATH)
        return false;
    if (is_good_god(which_god)
        && (is_evil_brand(brand) || is_demonic(item)))
    {
        return false;
    }

    switch (which_god)
    {
    case GOD_ZIN:
        // Lawful god: no chaos or mutagenics.
        if (brand == SPWPN_CHAOS)
            return false;

        if (artefact_property(item, ARTP_CONTAM))
            return false;
        break;

    case GOD_SHINING_ONE:
        // Crusader god: holiness, honourable combat.
        if (item.base_type == OBJ_WEAPONS && brand != SPWPN_HOLY_WRATH)
            return false;

        if (artefact_property(item, ARTP_INVISIBLE)
            || artefact_property(item, ARTP_STEALTH) > 0)
        {
            return false;
        }
        break;

    case GOD_LUGONU:
        // Abyss god: corruption.
        if (item.base_type == OBJ_WEAPONS && brand != SPWPN_DISTORTION)
            return false;
        break;

    case GOD_KIKUBAAQUDGHA:
        // Necromancy god.
        if (item.base_type == OBJ_WEAPONS && brand != SPWPN_PAIN)
            return false;
    case GOD_SIF_MUNA:
    case GOD_VEHUMET:
        // The magic gods: no preventing spellcasting.
        if (artefact_property(item, ARTP_PREVENT_SPELLCASTING))
            return false;
        break;

    case GOD_TROG:
        // Limited selection of brands.
        if (brand != SPWPN_HEAVY
            && brand != SPWPN_FLAMING
            && brand != SPWPN_ANTIMAGIC)
        {
            return false;
        }

        if (artefact_property(item, ARTP_MAGICAL_POWER) > 0
            || artefact_property(item, ARTP_INTELLIGENCE) > 0)
        {
            return false;
        }
        break;

    case GOD_CHEIBRIADOS:
        // Slow god: no speed, no berserking.
        if (brand == SPWPN_SPEED || ego == SPARM_RAMPAGING)
            return false;

        if (artefact_property(item, ARTP_ANGRY)
            || artefact_property(item, ARTP_RAMPAGING))
        {
            return false;
        }
        break;

    case GOD_DITHMENOS:
        // No reducing stealth.
        if (artefact_property(item, ARTP_STEALTH) < 0)
            return false;
        break;

    case GOD_IGNIS:
        // Fire god.
        if (item.base_type == OBJ_WEAPONS && brand != SPWPN_FLAMING)
            return false;
        break;

    default:
        break;
    }

    return true;
}

string replace_name_parts(const string &name_in, const item_def& item)
{
    string name = name_in;

    const god_type god_gift = origin_as_god_gift(item);

    // Don't allow "player's Death" type names for god gifts (except
    // for those from Xom).
    if (name.find("@player_death@", 0) != string::npos
        || name.find("@player_doom@", 0) != string::npos)
    {
        if (god_gift == GOD_NO_GOD || god_gift == GOD_XOM)
        {
            name = replace_all(name, "@player_death@",
                               "@player_name@"
                               + getRandNameString("killer_name"));
            name = replace_all(name, "@player_doom@",
                               "@player_name_possessive@ "
                               + getRandNameString("death_or_doom"));
        }
        else
        {
            // Simply overwrite the name with one of type "God's Favour".
            name = "of ";
            name += apostrophise(god_name(god_gift, false));
            name += getRandNameString("divine_esteem");
        }
    }
    name = replace_all(name, "@player_name@", you.your_name);
    name = replace_all(name, "@player_name_possessive@",
                       apostrophise(you.your_name));

    name = replace_all(name, "@player_species@",
                 species::name(you.species, species::SPNAME_GENUS));

    if (name.find("@branch_name@", 0) != string::npos)
    {
        string place = branches[random2(NUM_BRANCHES)].longname;
        if (!place.empty())
            name = replace_all(name, "@branch_name@", place);
    }

    // Occasionally use long name for Xom (see religion.cc).
    name = replace_all(name, "@xom_name@", god_name(GOD_XOM, coinflip()));

    if (name.find("@god_name@", 0) != string::npos
        || name.find("@god_name_possessive@", 0) != string::npos)
    {
        god_type which_god;

        // God gifts will always get the gifting god's name.
        if (god_gift != GOD_NO_GOD)
            which_god = god_gift;
        else
        {
            do
            {
                which_god = random_god();
            }
            while (!_god_fits_artefact(which_god, item, true));
        }

        name = replace_all(name, "@god_name@", god_name(which_god, false));
        name = replace_all(name, "@god_name_possessive@",
                           apostrophise(god_name(which_god, false)));
    }

    return name;
}

// Remember: disallow unrandart creation in Abyss/Pan.

// Functions defined in art-func.h are referenced in art-data.h
#include "art-func.h"

static const unrandart_entry unranddata[] =
{
#include "art-data.h"
};

static const unrandart_entry *_seekunrandart(const item_def &item);

bool is_artefact(const item_def &item)
{
    return item.flags & ISFLAG_ARTEFACT_MASK;
}

// returns true is item is a pure randart
bool is_random_artefact(const item_def &item)
{
    return item.flags & ISFLAG_RANDART;
}

/** Is this an unrandart, and if so which one?
 *
 *  @param item The item to be checked.
 *  @param which The unrand enum to be checked against (default 0).
 *  @returns true if item is an unrand, and if which is not 0, if it is the unrand
 *           specified by enum in which.
 */
bool is_unrandom_artefact(const item_def &item, int which)
{
    return item.flags & ISFLAG_UNRANDART
           && (!which || which == item.unrand_idx);
}

bool is_special_unrandom_artefact(const item_def &item)
{
    return item.flags & ISFLAG_UNRANDART
           && (_seekunrandart(item)->flags & UNRAND_FLAG_SPECIAL);
}

unique_item_status_type get_unique_item_status(int art)
{
    ASSERT_RANGE(art, UNRAND_START + 1, UNRAND_LAST);
    return you.unique_items[art - UNRAND_START];
}

static void _set_unique_item_existence(int art, bool exists)
{
    ASSERT_RANGE(art, UNRAND_START + 1, UNRAND_LAST);

    const unique_item_status_type status = !exists
        ? UNIQ_NOT_EXISTS
        : !crawl_state.generating_level // acquirement, gozag shops, ...
                // treat unrands that generate in these branches as if they
                // were acquired. TODO: there's a potential bug here if every
                // octopus king ring generates and the last is acquired.
                // note that unrands that generate in the abyss and get left
                // there will convert to UNIQ_LOST_IN_ABYSS, but they don't
                // get this automatically.
                || level_id::current().branch == BRANCH_TROVE
                || level_id::current().branch == BRANCH_ABYSS
            ? UNIQ_EXISTS_NONLEVELGEN
            : UNIQ_EXISTS;
    you.unique_items[art - UNRAND_START] = status;
}

void set_unique_item_status(const item_def& item,
                            unique_item_status_type status)
{
    if (item.flags & ISFLAG_UNRANDART)
    {
        ASSERT_RANGE(item.unrand_idx, UNRAND_START + 1, UNRAND_LAST);
        you.unique_items[item.unrand_idx - UNRAND_START] = status;
    }
}

/**
 * Fill out the inherent ARTPs corresponding to a given type of armour.
 *
 * @param arm           The armour_type of the armour in question.
 * @param proprt[out]   The properties list to be populated.
 */
static void _populate_armour_intrinsic_artps(const armour_type arm,
                                             artefact_properties_t &proprt)
{
    proprt[ARTP_FIRE] += armour_type_prop(arm, ARMF_RES_FIRE);
    proprt[ARTP_COLD] += armour_type_prop(arm, ARMF_RES_COLD);
    proprt[ARTP_NEGATIVE_ENERGY] += armour_type_prop(arm, ARMF_RES_NEG);
    proprt[ARTP_POISON] += armour_type_prop(arm, ARMF_RES_POISON);
    proprt[ARTP_ELECTRICITY] += armour_type_prop(arm, ARMF_RES_ELEC);
    proprt[ARTP_RCORR] += armour_type_prop(arm, ARMF_RES_CORR);
    proprt[ARTP_WILLPOWER] += armour_type_prop(arm, ARMF_WILLPOWER);
    proprt[ARTP_STEALTH] += armour_type_prop(arm, ARMF_STEALTH);
    proprt[ARTP_REGENERATION] += armour_type_prop(arm, ARMF_REGENERATION);
}

static map<stave_type, artefact_prop_type> staff_resist_artps = {
    { STAFF_FIRE,    ARTP_FIRE },
    { STAFF_COLD,    ARTP_COLD },
    { STAFF_ALCHEMY, ARTP_POISON },
    { STAFF_DEATH,   ARTP_NEGATIVE_ENERGY },
    { STAFF_AIR,     ARTP_ELECTRICITY },
    // nothing for conj or earth
};

static map<stave_type, artefact_prop_type> staff_enhancer_artps = {
    { STAFF_FIRE,           ARTP_ENHANCE_FIRE },
    { STAFF_COLD,           ARTP_ENHANCE_ICE },
    { STAFF_ALCHEMY,        ARTP_ENHANCE_ALCHEMY },
    { STAFF_DEATH,          ARTP_ENHANCE_NECRO },
    { STAFF_AIR,            ARTP_ENHANCE_AIR },
    { STAFF_CONJURATION,    ARTP_ENHANCE_CONJ },
    { STAFF_EARTH,          ARTP_ENHANCE_EARTH },
};

static void _populate_staff_intrinsic_artps(stave_type staff,
                                            artefact_properties_t &proprt)
{
    artefact_prop_type *prop = map_find(staff_resist_artps, staff);
    if (prop)
        proprt[*prop] = 1;
    prop = map_find(staff_enhancer_artps, staff);
    if (prop)
        proprt[*prop] = 1;
}

/// The artefact properties corresponding to a given base item.
struct artp_value
{
    /// The artp matching the item (e.g. ARTP_AC for RING_PROTECTION)
    artefact_prop_type  type;
    /// The value of the artp. (E.g. '9' for RING_MAGICAL_POWER.) If set to 0,
    /// uses item.plus instead.
    int                 value;
};

static map<jewellery_type, vector<artp_value>> jewellery_artps = {
    { AMU_REGENERATION, { { ARTP_REGENERATION, 1 } } },
    { AMU_MANA_REGENERATION, { { ARTP_MANA_REGENERATION, 1} } },
    { AMU_REFLECTION, { { ARTP_SHIELDING, AMU_REFLECT_SH / 2} } },
    { AMU_ACROBAT, { { ARTP_ACROBAT, 1 } } },

    { RING_MAGICAL_POWER, { { ARTP_MAGICAL_POWER, 9 } } },
    { RING_WIZARDRY, { { ARTP_WIZARDRY, 1} } },
    { RING_FLIGHT, { { ARTP_FLY, 1 } } },
    { RING_SEE_INVISIBLE, { { ARTP_SEE_INVISIBLE, 1 } } },
    { RING_STEALTH, { { ARTP_STEALTH, 1 } } },

    { RING_PROTECTION_FROM_FIRE, { { ARTP_FIRE, 1 } } },
    { RING_PROTECTION_FROM_COLD, { { ARTP_COLD, 1 } } },
    { RING_POISON_RESISTANCE, { { ARTP_POISON, 1 } } },
    { RING_POSITIVE_ENERGY, { { ARTP_NEGATIVE_ENERGY, 1 } } },
    { RING_WILLPOWER, { { ARTP_WILLPOWER, 1 } } },
    { RING_RESIST_CORROSION, { { ARTP_RCORR, 1 } } },

    { RING_FIRE, { { ARTP_FIRE, 1 }, { ARTP_COLD, -1 },
                   { ARTP_ENHANCE_FIRE, 1} } },
    { RING_ICE, { { ARTP_COLD, 1 }, { ARTP_FIRE, -1 },
                  { ARTP_ENHANCE_ICE, 1} } },

    { RING_STRENGTH, { { ARTP_STRENGTH, 0 } } },
    { RING_INTELLIGENCE, { { ARTP_INTELLIGENCE, 0 } } },
    { RING_DEXTERITY, { { ARTP_DEXTERITY, 0 } } },
    { RING_PROTECTION, { { ARTP_AC, 0 } } },
    { RING_EVASION, { { ARTP_EVASION, 0 } } },
    { RING_SLAYING, { { ARTP_SLAYING, 0 } } },
};

/**
 * Fill out the inherent ARTPs corresponding to a given type of jewellery.
 *
 * @param arm           The jewellery in question.
 * @param props[out]   The properties list to be populated.
 */
static void _populate_jewel_intrinsic_artps(const item_def &item,
                                            artefact_properties_t &props)
{
    const jewellery_type jewel = (jewellery_type)item.sub_type;
    vector<artp_value> *artps = map_find(jewellery_artps, jewel);
    if (!artps)
        return;

    for (const auto &artp : *artps)
        props[artp.type] += artp.value ? artp.value : item.plus;
}

// XXX: Building this directly from form data would be nice.
static map<talisman_type, vector<artp_value>> talisman_artps = {
    { TALISMAN_SERPENT, { { ARTP_POISON, 1 } } },
    { TALISMAN_STATUE, { { ARTP_POISON, 1 }, { ARTP_ELECTRICITY, 1 },
                         { ARTP_NEGATIVE_ENERGY, 1 } } },
    { TALISMAN_DRAGON, { { ARTP_POISON, 1 } } },
    { TALISMAN_STORM, { { ARTP_POISON, 1 }, { ARTP_ELECTRICITY, 1 } } },
    { TALISMAN_DEATH, { { ARTP_POISON, 1 }, { ARTP_NEGATIVE_ENERGY, 3 },
                        { ARTP_COLD, 1 } } },
};

/**
 * Fill out the inherent ARTPs corresponding to a given type of talisman.
 *
 * @param arm           The talisman in question.
 * @param props[out]    The properties list to be populated.
 */
static void _populate_talisman_intrinsic_artps(const item_def &item,
                                               artefact_properties_t &props)
{
    const talisman_type talisman = (talisman_type)item.sub_type;
    vector<artp_value> *artps = map_find(talisman_artps, talisman);
    if (!artps)
        return;

    for (const auto &artp : *artps)
        props[artp.type] += artp.value ? artp.value : item.plus;
}

/**
 * Fill out the inherent ARTPs corresponding to a given item.
 *
 * @param arm          The item in question.
 * @param props[out]   The properties list to be populated.
 */
static void _populate_item_intrinsic_artps(const item_def &item,
                                           artefact_properties_t &props)
{
    switch (item.base_type)
    {
        case OBJ_ARMOUR:
            _populate_armour_intrinsic_artps((armour_type)item.sub_type,
                                             props);
            break;
        case OBJ_STAVES:
            _populate_staff_intrinsic_artps((stave_type)item.sub_type, props);
            break;
        case OBJ_JEWELLERY:
            _populate_jewel_intrinsic_artps(item, props);
            break;
        case OBJ_TALISMANS:
            _populate_talisman_intrinsic_artps(item, props);
            break;

        default:
            break;
    }
}

void artefact_desc_properties(const item_def &item,
                              artefact_properties_t &proprt)
{
    // Randart books have no randart properties.
    if (item.base_type == OBJ_BOOKS)
        return;

    // actual artefact properties
    artefact_properties(item, proprt);

    // fake artefact properties (intrinsics). For talismans, we don't list
    // "intrinsic" resists provided by the form.
    if (item.base_type != OBJ_TALISMANS)
        _populate_item_intrinsic_artps(item, proprt);
}

static void _add_randart_weapon_brand(const item_def &item,
                                    artefact_properties_t &item_props)
{
    const int item_type = item.sub_type;

    if (!is_weapon_brand_ok(item_type, item_props[ARTP_BRAND], true))
        item_props[ARTP_BRAND] = SPWPN_NORMAL;

    if (item_props[ARTP_BRAND] != SPWPN_NORMAL)
        return;

    if (is_blessed_weapon_type(item.sub_type))
        item_props[ARTP_BRAND] = SPWPN_HOLY_WRATH;
    else if (is_range_weapon(item))
    {
        item_props[ARTP_BRAND] = random_choose_weighted(
            8, SPWPN_DRAINING,
            8, SPWPN_HEAVY,
            8, SPWPN_FLAMING,
            8, SPWPN_FREEZING,
            4, SPWPN_ELECTROCUTION,
            3, SPWPN_SPEED,
            2, SPWPN_ANTIMAGIC,
            2, SPWPN_CHAOS,
            1, SPWPN_HOLY_WRATH);

        // Penetration is only allowed on crossbows.
        // This may change in future.
        if (is_crossbow(item) && one_chance_in(6))
            item_props[ARTP_BRAND] = SPWPN_PENETRATION;
    }
    else if (is_demonic(item) && x_chance_in_y(7, 9))
    {
        item_props[ARTP_BRAND] = random_choose(
            SPWPN_DRAINING,
            SPWPN_FLAMING,
            SPWPN_FREEZING,
            SPWPN_ELECTROCUTION,
            SPWPN_VAMPIRISM,
            SPWPN_PAIN,
            SPWPN_VENOM);
        // fall back to regular melee brands 2/9 of the time
    }
    else
    {
        item_props[ARTP_BRAND] = random_choose_weighted(
            47, SPWPN_FLAMING,
            47, SPWPN_FREEZING,
            26, SPWPN_HEAVY,
            26, SPWPN_VENOM,
            26, SPWPN_DRAINING,
            13, SPWPN_HOLY_WRATH,
            13, SPWPN_ELECTROCUTION,
            13, SPWPN_SPEED,
            13, SPWPN_VAMPIRISM,
            13, SPWPN_PAIN,
            13, SPWPN_ANTIMAGIC,
            13, SPWPN_PROTECTION,
            13, SPWPN_SPECTRAL,
             3, SPWPN_DISTORTION,
             3, SPWPN_CHAOS);
    }

    // no brand = magic flag to reject and retry
    if (!is_weapon_brand_ok(item_type, item_props[ARTP_BRAND], true))
        item_props[ARTP_BRAND] = SPWPN_NORMAL;
}

// Are any of the given set of artefact properties in the set of intrinsic and
// extant artefact properties for an item?
static bool _any_artps_in_item_props(const vector<artefact_prop_type> &artps,
                                     const artefact_properties_t &intrinsic_props,
                                     const artefact_properties_t &extant_props)
{
    for (auto prop: artps)
        if (intrinsic_props[prop] || extant_props[prop])
            return true;

    return false;
}

/**
 * Can the given artefact property be placed on the given item?
 * See also _randart_is_conflicting().
 *
 * @param prop              The artefact property in question (e.g.
 *                          ARTP_BLINK).
 * @param prop_val          The artp value the property will have. This
 *                          shouldn't include any part of the property value
 *                          intrinsic to the item (i.e. in intrinsic_props).
 * @param item              The item in question.
 * @param intrinsic_props   The properties intrinsic to the item's base type.
 * @param extant_props      The properties already chosen for the artefact.
 * @return                  True if the property doesn't conflict with any
 *                          chosen or intrinsic properties, and doesn't
 *                          violate any other special constraints (e.g. no
 *                          slaying on weapons); false otherwise.
 */
static bool _artp_can_go_on_item(artefact_prop_type prop, int prop_val,
                                 const item_def &item,
                                 const artefact_properties_t &intrinsic_props,
                                 const artefact_properties_t &extant_props)
{
    // We aren't changing anything.
    if (!prop_val)
        return true;

    // Make sure the new prop value is consistent with the intrinsic one: don't
    // reduce any positive intrinsic value nor decrease any negative one.
    const int intrinsic_val = intrinsic_props[prop];
    if (intrinsic_val
            && (intrinsic_val > 0 && prop_val < 0
                || intrinsic_val < 0 && prop_val > 0))
    {
        return false;
    }

    const object_class_type item_class = item.base_type;
    // Categorise items by whether they're quick to swap or not. Some artefact
    // properties aren't appropriate on easily swappable items.
    const bool non_swappable = item_class == OBJ_ARMOUR
                               || item_class == OBJ_TALISMANS
                               || item_class == OBJ_JEWELLERY
                                  && jewellery_is_amulet(item);

    // warning: using some item calls may not work here, for example,
    // get_weapon_brand; the `item` object is not fully set up.
    switch (prop)
    {
        // weapons already have slaying. feels weird on staves
        case ARTP_SLAYING:
            return item_class != OBJ_WEAPONS && item_class != OBJ_STAVES;
        // prevent properties that conflict with each other
        case ARTP_CORRODE:
            return !_any_artps_in_item_props({ ARTP_RCORR }, intrinsic_props,
                                             extant_props);
        case ARTP_RCORR:
            return !_any_artps_in_item_props({ ARTP_CORRODE }, intrinsic_props,
                                             extant_props);
        case ARTP_MAGICAL_POWER:
            return item_class != OBJ_WEAPONS && item_class != OBJ_STAVES
                   || extant_props[ARTP_BRAND] != SPWPN_ANTIMAGIC;
        case ARTP_BLINK:
            return !_any_artps_in_item_props({ ARTP_PREVENT_TELEPORTATION },
                                             intrinsic_props, extant_props);
        case ARTP_PREVENT_TELEPORTATION:
            return non_swappable
                   && !_any_artps_in_item_props({ ARTP_BLINK },
                                                intrinsic_props, extant_props);
        // only on melee weapons
        case ARTP_ANGRY:
        case ARTP_NOISE:
            return item_class == OBJ_WEAPONS && !is_range_weapon(item);
        case ARTP_PREVENT_SPELLCASTING:
            return non_swappable
                && !item.is_type(OBJ_JEWELLERY, AMU_MANA_REGENERATION)
                && !_any_artps_in_item_props({ ARTP_ENHANCE_CONJ,
                    ARTP_ENHANCE_HEXES, ARTP_ENHANCE_SUMM, ARTP_ENHANCE_NECRO,
                    ARTP_ENHANCE_TLOC, ARTP_ENHANCE_FIRE, ARTP_ENHANCE_ICE,
                    ARTP_ENHANCE_AIR, ARTP_ENHANCE_EARTH, ARTP_ENHANCE_ALCHEMY,
                    ARTP_ENHANCE_FORGECRAFT }, intrinsic_props, extant_props);
        case ARTP_REGENERATION:
            // XXX: regen disabled on talismans because of an untransform crash
            // related to talismans being slotless
            return non_swappable && item_class != OBJ_TALISMANS;
        case ARTP_INVISIBLE:
        case ARTP_HARM:
        case ARTP_RAMPAGING:
            // only on items that can't be quickly swapped
            return non_swappable;
        // prevent on armour/talismans (since they're swapped infrequently) and
        // rings (since 2 slots reduces the pressure to swap)
        case ARTP_FRAGILE:
            return item_class != OBJ_ARMOUR
                   && item_class != OBJ_TALISMANS
                   && (item_class != OBJ_JEWELLERY
                       || jewellery_is_amulet(item));
        case ARTP_DRAIN:
        case ARTP_CONTAM:
            return item_class != OBJ_TALISMANS; // TODO: support..?
        case ARTP_ARCHMAGI:
            return item.is_type(OBJ_ARMOUR, ARM_ROBE);
        case ARTP_ENHANCE_CONJ:
        case ARTP_ENHANCE_HEXES:
        case ARTP_ENHANCE_SUMM:
        case ARTP_ENHANCE_NECRO:
        case ARTP_ENHANCE_TLOC:
        case ARTP_ENHANCE_FIRE:
        case ARTP_ENHANCE_ICE:
        case ARTP_ENHANCE_AIR:
        case ARTP_ENHANCE_EARTH:
        case ARTP_ENHANCE_ALCHEMY:
        case ARTP_ENHANCE_FORGECRAFT:
            // Maybe we should allow these for robes, too?  And hats? And
            // gloves and cloaks and scarves?
            return (item.base_type == OBJ_STAVES
                       || item.is_type(OBJ_ARMOUR, ARM_ORB))
                   && !_any_artps_in_item_props({ ARTP_PREVENT_SPELLCASTING },
                                             intrinsic_props, extant_props);
        default:
            return true;
    }
}

bool are_fixed_props_ok(item_def& item)
{
    if (!item.props.exists(FIXED_PROPS_KEY))
        return true;

    artefact_properties_t intrinsic_props;
    intrinsic_props.init(0);
    _populate_item_intrinsic_artps(item, intrinsic_props);

    artefact_properties_t props;
    props.init(0);

    CrawlHashTable const *fixed_props;
    fixed_props = &item.props[FIXED_PROPS_KEY].get_table();
    for (auto const &kv : *fixed_props)
    {
        const auto prop = artp_type_from_name(kv.first);
        const auto &final_val = kv.second.get_int();
        const auto prop_val = final_val - intrinsic_props[prop];
        if (!_artp_can_go_on_item(prop, prop_val, item, intrinsic_props,
                                  props))
        {
            return false;
        }

        props[prop] = prop_val;
    }

    return true;
}

/// Generation info for a type of artefact property.
struct artefact_prop_data
{
    /// The name of the prop, as displayed on item annotations, etc.
    const char *name;
    /// The types of values this prop can have (e.g. bool, positive int, int)
    artefact_value_type value_types;
    /// Weight in randart selection (higher = more common)
    int weight;
    /// Randomly generate a 'good' value; null if this prop is never good
    function<int ()> gen_good_value;
    /// Randomly generate a 'bad' value; null if this prop is never bad
    function<int ()> gen_bad_value;
    /// The value beyond which the artp should not be repeatedly applied.
    int max_dup;
    /// The amount to increment the odds of a property being reapplied
    int odds_inc;
};

/// Generate 'good' values for stat artps (e.g. ARTP_STRENGTH)
static int _gen_good_stat_artp() { return 1 + coinflip() + one_chance_in(4); }

/// Generate 'bad' values for stat artps (e.g. ARTP_STRENGTH)
static int _gen_bad_stat_artp() { return -2 - random2(4); }

/// Generate 'good' values for resist-ish artps (e.g. ARTP_FIRE)
static int _gen_good_res_artp() { return 1; }

/// Generate 'bad' values for resist-ish artps (e.g. ARTP_FIRE)
static int _gen_bad_res_artp() { return -1; }

/// Generate 'good' values for ARTP_HP/ARTP_MAGICAL_POWER
static int _gen_good_hpmp_artp() { return random_range(4, 9) +
                                          (one_chance_in(3) ? random_range(1, 3)
                                                            : 0); }

/// Generate 'bad' values for ARTP_HP/ARTP_MAGICAL_POWER
static int _gen_bad_hpmp_artp() { return -_gen_good_hpmp_artp(); }

/// Generation info for artefact properties.
static const artefact_prop_data artp_data[] =
{
    { "Brand", ARTP_VAL_BRAND, 0, nullptr, nullptr, 0, 0 }, // ARTP_BRAND,
    { "AC", ARTP_VAL_ANY, 0, nullptr, nullptr, 0, 0}, // ARTP_AC,
    { "EV", ARTP_VAL_ANY, 0, nullptr, nullptr, 0, 0 }, // ARTP_EVASION,
    { "Str", ARTP_VAL_ANY, 100,     // ARTP_STRENGTH,
        _gen_good_stat_artp, _gen_bad_stat_artp, 7, 1 },
    { "Int", ARTP_VAL_ANY, 100,     // ARTP_INTELLIGENCE,
        _gen_good_stat_artp, _gen_bad_stat_artp, 7, 1 },
    { "Dex", ARTP_VAL_ANY, 100,     // ARTP_DEXTERITY,
        _gen_good_stat_artp, _gen_bad_stat_artp, 7, 1 },
    { "rF", ARTP_VAL_ANY, 60,       // ARTP_FIRE,
        _gen_good_res_artp, _gen_bad_res_artp, 2, 4},
    { "rC", ARTP_VAL_ANY, 60,       // ARTP_COLD,
        _gen_good_res_artp, _gen_bad_res_artp, 2, 4 },
    { "rElec", ARTP_VAL_BOOL, 55,   // ARTP_ELECTRICITY,
        []() { return 1; }, nullptr, 0, 0  },
    { "rPois", ARTP_VAL_BOOL, 55,   // ARTP_POISON,
        []() { return 1; }, nullptr, 0, 0 },
    { "rN", ARTP_VAL_ANY, 55,       // ARTP_NEGATIVE_ENERGY,
        _gen_good_res_artp, nullptr, 2, 4 },
    { "Will", ARTP_VAL_ANY, 50,       // ARTP_WILLPOWER,
        _gen_good_res_artp, _gen_bad_res_artp, 2, 4 },
    { "SInv", ARTP_VAL_BOOL, 30,    // ARTP_SEE_INVISIBLE,
        []() { return 1; }, nullptr, 0, 0 },
    { "+Inv", ARTP_VAL_BOOL, 15,    // ARTP_INVISIBLE,
        []() { return 1; }, nullptr, 0, 0 },
    { "Fly", ARTP_VAL_BOOL, 15,    // ARTP_FLY,
        []() { return 1; }, nullptr, 0, 0 },
    { "+Blink", ARTP_VAL_BOOL, 15,  // ARTP_BLINK,
        []() { return 1; }, nullptr, 0, 0 },
#if TAG_MAJOR_VERSION == 34
    { "+Rage", ARTP_VAL_BOOL, 0,   // ARTP_BERSERK,
        []() { return 1; }, nullptr, 0, 0 },
#endif
    { "*Noise", ARTP_VAL_POS, 30,    // ARTP_NOISE,
        nullptr, []() { return 2; }, 0, 0 },
    { "-Cast", ARTP_VAL_BOOL, 25,   // ARTP_PREVENT_SPELLCASTING,
        nullptr, []() { return 1; }, 0, 0 },
#if TAG_MAJOR_VERSION == 34
    { "*Tele", ARTP_VAL_BOOL,  0,   // ARTP_CAUSE_TELEPORTATION,
        nullptr, []() { return 1; }, 0, 0 },
#endif
    { "-Tele", ARTP_VAL_BOOL, 25,   // ARTP_PREVENT_TELEPORTATION,
        nullptr, []() { return 1; }, 0, 0 },
    { "*Rage", ARTP_VAL_POS, 30,    // ARTP_ANGRY,
        nullptr, []() { return 20; }, 0, 0 },
#if TAG_MAJOR_VERSION == 34
    { "Hungry", ARTP_VAL_POS, 0, nullptr, nullptr, 0, 0 },// ARTP_METABOLISM,
#endif
    { "^Contam", ARTP_VAL_POS, 20,   // ARTP_CONTAM
        nullptr, []() { return 1; }, 0, 0 },
#if TAG_MAJOR_VERSION == 34
    { "Acc", ARTP_VAL_ANY, 0, nullptr, nullptr, 0, 0 }, // ARTP_ACCURACY,
#endif
    { "Slay", ARTP_VAL_ANY, 30,     // ARTP_SLAYING,
      []() { return 2 + random2(2); },
      []() { return -(2 + random2(5)); }, 3, 2 },
#if TAG_MAJOR_VERSION == 34
    { "*Curse", ARTP_VAL_POS, 0, nullptr, nullptr, 0 }, // ARTP_CURSE,
#endif
    { "Stlth", ARTP_VAL_ANY, 40,    // ARTP_STEALTH,
        _gen_good_res_artp, _gen_bad_res_artp, 0, 0 },
    { "MP", ARTP_VAL_ANY, 15,       // ARTP_MAGICAL_POWER,
        _gen_good_hpmp_artp, _gen_bad_hpmp_artp, 0, 0 },
    { "Delay", ARTP_VAL_ANY, 0, nullptr, nullptr, 0, 0 }, // ARTP_BASE_DELAY,
    { "HP", ARTP_VAL_ANY, 0,       // ARTP_HP,
        _gen_good_hpmp_artp, _gen_bad_hpmp_artp, 0, 0 },
    { "Clar", ARTP_VAL_BOOL, 0, nullptr, nullptr, 0, 0 }, // ARTP_CLARITY,
    { "BAcc", ARTP_VAL_ANY, 0, nullptr, nullptr, 0, 0 },  // ARTP_BASE_ACC,
    { "BDam", ARTP_VAL_ANY, 0, nullptr, nullptr, 0, 0 },  // ARTP_BASE_DAM,
    { "RMsl", ARTP_VAL_BOOL, 0, nullptr, nullptr, 0, 0 }, // ARTP_RMSL,
#if TAG_MAJOR_VERSION == 34
    { "+Fog", ARTP_VAL_BOOL, 0, nullptr, nullptr, 0, 0 }, // ARTP_FOG,
#endif
    { "Regen", ARTP_VAL_BOOL, 35,   // ARTP_REGENERATION,
        []() { return 1; }, nullptr, 0, 0 },
#if TAG_MAJOR_VERSION == 34
    { "SustAt", ARTP_VAL_BOOL, 0, nullptr, nullptr, 0, 0 }, // ARTP_SUSTAT,
#endif
    { "nupgr", ARTP_VAL_BOOL, 0, nullptr, nullptr, 0, 0 },// ARTP_NO_UPGRADE,
    { "rCorr", ARTP_VAL_BOOL, 40,   // ARTP_RCORR,
        []() { return 1; }, nullptr, 0, 0 },
    { "rMut", ARTP_VAL_BOOL, 0, nullptr, nullptr, 0, 0 }, // ARTP_RMUT,
#if TAG_MAJOR_VERSION == 34
    { "+Twstr", ARTP_VAL_BOOL, 0,   // ARTP_TWISTER,
        []() { return 1; }, nullptr, 0, 0 },
#endif
    { "*Corrode", ARTP_VAL_BOOL, 25, // ARTP_CORRODE,
        nullptr, []() { return 1; }, 0, 0 },
    { "^Drain", ARTP_VAL_BOOL, 25, // ARTP_DRAIN,
        nullptr, []() { return 1; }, 0, 0 },
    { "*Slow", ARTP_VAL_BOOL, 25, // ARTP_SLOW,
        nullptr, []() { return 1; }, 0, 0 },
    { "^Fragile", ARTP_VAL_BOOL, 30, // ARTP_FRAGILE,
        nullptr, []() { return 1; }, 0, 0 },
    { "SH", ARTP_VAL_ANY, 0, nullptr, nullptr, 0, 0 }, // ARTP_SHIELDING,
    { "Harm", ARTP_VAL_BOOL, 25, // ARTP_HARM,
        []() {return 1;}, nullptr, 0, 0},
    { "Rampage", ARTP_VAL_BOOL, 25, // ARTP_RAMPAGING,
        []() {return 1;}, nullptr, 0, 0},
    { "Archmagi", ARTP_VAL_BOOL, 40, // ARTP_ARCHMAGI,
        []() {return 1;}, nullptr, 0, 0},
    { "Conj", ARTP_VAL_BOOL, 20, // ARTP_ENHANCE_CONJ,
        []() {return 1;}, nullptr, 0, 0},
    { "Hexes", ARTP_VAL_BOOL, 20, // ARTP_ENHANCE_HEXES,
        []() {return 1;}, nullptr, 0, 0},
    { "Summ", ARTP_VAL_BOOL, 20, // ARTP_ENHANCE_SUMM,
        []() {return 1;}, nullptr, 0, 0},
    { "Necro", ARTP_VAL_BOOL, 20, // ARTP_ENHANCE_NECRO,
        []() {return 1;}, nullptr, 0, 0},
    { "Tloc", ARTP_VAL_BOOL, 20, // ARTP_ENHANCE_TLOC,
        []() {return 1;}, nullptr, 0, 0},
#if TAG_MAJOR_VERSION == 34
    { "Tmut", ARTP_VAL_BOOL, 0, // ARTP_ENHANCE_TMUT,
        []() {return 1;}, nullptr, 0, 0},
#endif
    { "Fire", ARTP_VAL_BOOL, 20, // ARTP_ENHANCE_FIRE,
        []() {return 1;}, nullptr, 0, 0},
    { "Ice", ARTP_VAL_BOOL, 20, // ARTP_ENHANCE_ICE,
        []() {return 1;}, nullptr, 0, 0},
    { "Air", ARTP_VAL_BOOL, 20, // ARTP_ENHANCE_AIR,
        []() {return 1;}, nullptr, 0, 0},
    { "Earth", ARTP_VAL_BOOL, 20, // ARTP_ENHANCE_EARTH,
        []() {return 1;}, nullptr, 0, 0},
    { "Alch", ARTP_VAL_BOOL, 20, // ARTP_ENHANCE_ALCHEMY,
        []() {return 1;}, nullptr, 0, 0},

    { "Acrobat", ARTP_VAL_BOOL, 0, // ARTP_ACROBAT,
        []() {return 1;}, nullptr, 0, 0},
    { "RegenMP", ARTP_VAL_BOOL, 0,   // ARTP_MANA_REGENERATION,
        []() { return 1; }, nullptr, 0, 0 },
    { "Wiz", ARTP_VAL_BOOL, 0,   // ARTP_WIZARDRY,
        []() { return 1; }, nullptr, 0, 0 },
    { "Forge", ARTP_VAL_BOOL, 20, // ARTP_ENHANCE_FORGECRAFT,
        []() {return 1;}, nullptr, 0, 0},
};
COMPILE_CHECK(ARRAYSZ(artp_data) == ARTP_NUM_PROPERTIES);
// weights sum to 1000


/**
 * Is it possible for the given artp to be generated with 'good' values
 * (generally helpful to the player?
 * E.g. ARTP_SLAYING, ARTP_FIRE, ARTP_REGENERATION, etc.
 *
 * @param prop      The artefact property in question.
 * @return      true if the ARTP is ever generated as a 'good' prop; false
 *              otherwise.
 */
bool artp_potentially_good(artefact_prop_type prop)
{
    return artp_data[prop].gen_good_value != nullptr;
}

/**
 * Is it possible for the given artp to be generated with 'bad' values
 * (generally harmful to the player?
 * E.g. ARTP_SLAYING, ARTP_AC, ARTP_CONTAM, etc.
 *
 * @param prop      The artefact property in question.
 * @return      true if the ARTP is ever generated as a 'bad' prop; false
 *              otherwise.
 */
bool artp_potentially_bad(artefact_prop_type prop)
{
    return artp_data[prop].gen_bad_value != nullptr;
}

/**
 * What type of value does this prop have?
 *
 * There should be a better way of expressing this...
 *
 * @param prop      The prop type in question.
 * @return          An artefact_value_type describing that values the prop
 *                  accepts.
 */
artefact_value_type artp_value_type(artefact_prop_type prop)
{
    ASSERT_RANGE(prop, 0, ARRAYSZ(artp_data));
    return artp_data[prop].value_types;
}

/**
 * Is the given value for the given prop in the range allowed for the prop
 * across all item types? For the brand prop/value type, we can't check validity without
 *
 * @param prop      The prop type in question.
 * @param value     The value in question.
 * @return          True is the value is in the valid range for the prop, false
 *                  otherwise.
 */
bool artp_value_is_valid(artefact_prop_type prop, int value)
{
    switch (artp_value_type(prop))
    {
    case ARTP_VAL_BOOL:
        return value == 0 || value == 1;
    case ARTP_VAL_POS:
    case ARTP_VAL_BRAND:
        return value >= 0;
    case ARTP_VAL_ANY:
        return true;
    default:
        die("Buggy artefact_value_type");
    }
}

/**
 * Return the name for a given artefact property.
 *
 * @param prop      The artp in question.
 * @return          A name; e.g. rCorr, Slay, HP, etc.
 */
const char *artp_name(artefact_prop_type prop)
{
    ASSERT_RANGE(prop, 0, ARRAYSZ(artp_data));
    return artp_data[prop].name;
}

/**
 * Return the property type for a given artefact property name.
 *
 * @param name      The name of the artp.
 * @return          The type of artp. The value ARTP_NUM_PROPERTIES is returned
 *                  if there is no match.
 */
artefact_prop_type artp_type_from_name(const string &name)
{
    const auto prop_name = lowercase_string(name);
    for (int i = 0; i < ARTP_NUM_PROPERTIES; ++i)
    {
        const auto prop = static_cast<artefact_prop_type>(i);
        const string pname = artp_name(prop);
        if (lowercase_string(pname) == prop_name)
            return prop;
    }

    return ARTP_NUM_PROPERTIES;
}

/**
 * Try to add a 'good' version of a given prop to the given set of item props.
 * The property may already exist in the set; if so, increase its value.
 *
 * @param prop[in]              The prop to be added.
 * @param intrinsic_props[in]   The list props intrinsic to the item.
 * @param item_props[out]       The list of item props to be added to.
 * @return bool                 True if the property could be added, false
 *                              otherwise.
 */
static bool _add_good_randart_prop(artefact_prop_type prop, const item_def &item,
                                   const artefact_properties_t &intrinsic_props,
                                   artefact_properties_t &item_props)
{
    int prop_val = item_props[prop];
    // Add one to the starting value for stat bonuses.
    if ((prop == ARTP_STRENGTH
         || prop == ARTP_INTELLIGENCE
         || prop == ARTP_DEXTERITY)
        && prop_val == 0)
    {
        prop_val++;
    }

    prop_val += artp_data[prop].gen_good_value();
    if (!_artp_can_go_on_item(prop, prop_val, item, intrinsic_props,
                              item_props))
    {
        return false;
    }

    item_props[prop] = prop_val;
    return true;
}

/**
 * Generate the properties for a randart. We start with a quality level, then
 * determine number of good and bad properties from that level. The more good
 * properties we have, the more likely we are to add some bad properties in
 * to make it a bit more interesting.
 *
 * For each "good" and "bad" property we want, we randomly pick from the total
 * list of good or bad properties; if the picked property already exists and
 * can have variable levels, we increment the levels by a certain amount. There
 * is a maximum number of distinct properties that can be applied to a randart
 * (for legibility, mostly), so overflow gets picked as increments to
 * variable-level properties when possible.
 *
 * @param item          The item to apply properties to.
 * @param item_props    The properties of that item.
 */
static void _get_randart_properties(const item_def &item,
                                    artefact_properties_t &item_props)
{
    const object_class_type item_class = item.base_type;
    artefact_properties_t intrinsic_props;
    intrinsic_props.init(0);
    _populate_item_intrinsic_artps(item, intrinsic_props);

    // For any fixed properties, initialize our item with their values and
    // count how many good and bad properties we've fixed to an increased
    // value.
    CrawlHashTable const *fixed_props = nullptr;
    int fixed_bad = 0, fixed_good = 0;
    if (item.props.exists(FIXED_PROPS_KEY))
    {
        fixed_props = &item.props[FIXED_PROPS_KEY].get_table();
        for (auto const &kv : *fixed_props)
        {
            const auto prop = artp_type_from_name(kv.first);
            const auto &final_val = kv.second.get_int();
            const auto prop_val = final_val - intrinsic_props[prop];
            if (!_artp_can_go_on_item(prop, prop_val, item, intrinsic_props,
                                      item_props))
            {
                item_def item_copy = item;
                item_copy.flags |= ISFLAG_IDENTIFIED;
                mprf(MSGCH_ERROR, "Ignoring fixed artefact property %s with"
                     " value %d that's incompatible with the item %s.",
                     artp_name(prop), final_val,
                     item_copy.name(DESC_PLAIN).c_str());
                continue;
            }

            const bool ever_good = artp_potentially_good(prop);
            if (ever_good && prop_val > 0)
                fixed_good += 1;
            else if (artp_potentially_bad(prop)
                    && (ever_good && prop_val < 0
                        || !ever_good && prop_val > 0))
                fixed_bad += 1;

            item_props[prop] = prop_val;
        }
    }

    // Each point of quality lets us add or enhance a good property.
    const int max_quality = 7;
    const int quality = 1 + binomial(max_quality - 1, 21);

    // We'll potentially add up to 2 bad properties, also considering any fixed
    // bad properties.
    int bad = 0;
    if (fixed_bad < 2)
        bad = binomial(2 - fixed_bad,  21);

    // For each point of quality and for each bad property added, we'll add or
    // enhance one good property.
    int good = max(quality + fixed_bad + bad - fixed_good, 0);

    // We want to avoid generating more than 4-ish properties or things
    // get spammy. Extra "good" properties will be used to enhance properties
    // only, not to add more distinct properties. There's still a small chance
    // of >4 properties.
    int max_properties = 4 + one_chance_in(20);
    // sequence point
    max_properties += one_chance_in(40);

    int enhance = 0;
    if (good + bad > max_properties)
    {
        enhance = good + bad - max_properties;
        good = max_properties - bad;
    }

    // Initialize a vector of weighted artefact properties. Don't add any
    // properties that are already fixed.
    vector<pair<artefact_prop_type, int>> art_prop_weights;
    for (int i = 0; i < ARTP_NUM_PROPERTIES; ++i)
    {
        const artefact_prop_type prop = static_cast<artefact_prop_type>(i);
        const string prop_name = artp_name(prop);
        if (!fixed_props || !fixed_props->exists(prop_name))
            art_prop_weights.emplace_back(prop, artp_data[i].weight);
    }

    // Make sure all weapons have a brand.
    if (item_class == OBJ_WEAPONS)
        _add_randart_weapon_brand(item, item_props);
    else if (item_class == OBJ_ARMOUR
             && item_always_has_ego(item)
             && item_props[ARTP_BRAND] == SPARM_NORMAL)
    {
        item_props[ARTP_BRAND] =
            choose_armour_ego(static_cast<armour_type>(item.sub_type));
    }

    // Randomly pick properties from the list, choose an appropriate value,
    // then subtract them from the good/bad/enhance count as needed the
    // 'enhance' count is not guaranteed to be used.
    while (good > 0 || bad > 0)
    {
        const artefact_prop_type *prop_ptr
            = random_choose_weighted(art_prop_weights);
        ASSERTM(prop_ptr, "all %d randart properties have weight 0?",
                (int) art_prop_weights.size());
        const artefact_prop_type prop = *prop_ptr;

        // Should we try to generate a good or bad version of the prop?
        const bool can_gen_good = good > 0
            && artp_potentially_good(prop)
            // When assigning a good property, we don't ever want to increase
            // the already negative level of an assigned bad property.
            && item_props[prop] >= 0;
        const bool can_gen_bad = bad > 0
            && artp_potentially_bad(prop)
            // When assigning a bad property, we don't ever want to:
            //   1) lower the already negative level of an assigned bad property.
            //   2) lower the already positive level of an assigned good property.
            && item_props[prop] == 0;
        const bool gen_good = can_gen_good && (!can_gen_bad || coinflip());

        if (gen_good)
        {
            // Potentially increment the value of the property more than once,
            // using up a good property each time. Always do so if there's any
            // 'enhance' left, if possible.
            bool bad_prop = false;
            for (int chance_denom = 1;
                 intrinsic_props[prop] + item_props[prop]
                     <= artp_data[prop].max_dup
                 && (enhance > 0
                     || good > 0 && one_chance_in(chance_denom));
                 chance_denom += artp_data[prop].odds_inc)
            {
                if (!_add_good_randart_prop(prop, item, intrinsic_props,
                                            item_props))
                {
                    bad_prop = true;
                    break;
                }

                if (enhance > 0 && chance_denom > 1)
                    --enhance;
                else
                    --good;
            }

            if (bad_prop)
                continue;
        }
        else if (can_gen_bad)
        {
            int prop_val = artp_data[prop].gen_bad_value();
            if (!_artp_can_go_on_item(prop, prop_val, item, intrinsic_props,
                        item_props))
            {
                continue;
            }

            item_props[prop] = prop_val;
            --bad;
        }
        else
            continue;

        // Don't choose the same prop twice.
        const auto weight_tuple = make_pair(prop, artp_data[prop].weight);
        const auto old_end = art_prop_weights.end();
        const auto new_end = std::remove(art_prop_weights.begin(), old_end,
                                         weight_tuple);
        // That should have removed exactly one entry.
        ASSERT(new_end == old_end - 1);
        art_prop_weights.erase(new_end, old_end);
    }
}

void setup_unrandart(item_def &item, bool creating)
{
    ASSERT(is_unrandom_artefact(item));
    CrawlVector &rap = item.props[ARTEFACT_PROPS_KEY].get_vector();
    const unrandart_entry *unrand = _seekunrandart(item);

    if (unrand->prpty[ARTP_NO_UPGRADE] && !creating)
        return; // don't mangle mutable items

    for (int i = 0; i < ART_PROPERTIES; i++)
        rap[i] = static_cast<short>(unrand->prpty[i]);

    item.base_type = unrand->base_type;
    item.sub_type  = unrand->sub_type;
    item.plus      = unrand->plus;
}

static bool _init_artefact_properties(item_def &item)
{
    ASSERT(is_artefact(item));

    if (is_unrandom_artefact(item))
    {
        setup_unrandart(item);
        return true;
    }

    CrawlVector &rap = item.props[ARTEFACT_PROPS_KEY].get_vector();
    for (vec_size i = 0; i < ART_PROPERTIES; i++)
        rap[i] = static_cast<short>(0);

    ASSERT(item.base_type != OBJ_BOOKS);

    artefact_properties_t prop;
    prop.init(0);
    _get_randart_properties(item, prop);

    for (int i = 0; i < ART_PROPERTIES; i++)
        rap[i] = static_cast<short>(prop[i]);

    return true;
}

void artefact_properties(const item_def &item,
                         artefact_properties_t  &proprt)
{
    ASSERT(is_artefact(item));
    ASSERT(item.base_type != OBJ_BOOKS);
    ASSERT(item.props.exists(ARTEFACT_PROPS_KEY) || is_unrandom_artefact(item));

    if (item.props.exists(ARTEFACT_PROPS_KEY))
    {
        const CrawlVector &rap_vec =
            item.props[ARTEFACT_PROPS_KEY].get_vector();
        ASSERT(rap_vec.get_type()     == SV_SHORT);
        ASSERT(rap_vec.size()         == ART_PROPERTIES);
        ASSERT(rap_vec.get_max_size() == ART_PROPERTIES);

        for (vec_size i = 0; i < ART_PROPERTIES; i++)
            proprt[i] = rap_vec[i].get_short();
    }
    else // if (is_unrandom_artefact(item))
    {
        const unrandart_entry *unrand = _seekunrandart(item);

        for (int i = 0; i < ART_PROPERTIES; i++)
            proprt[i] = static_cast<short>(unrand->prpty[i]);
    }
}

int artefact_property(const item_def &item, artefact_prop_type prop)
{
    ASSERT(is_artefact(item));
    ASSERT(item.base_type != OBJ_BOOKS);
    ASSERT(item.props.exists(ARTEFACT_PROPS_KEY) || is_unrandom_artefact(item));
    if (item.props.exists(ARTEFACT_PROPS_KEY))
    {
        const CrawlVector &rap_vec =
            item.props[ARTEFACT_PROPS_KEY].get_vector();
        return rap_vec[prop].get_short();
    }
    else // if (is_unrandom_artefact(item))
    {
        const unrandart_entry *unrand = _seekunrandart(item);
        return static_cast<short>(unrand->prpty[prop]);

    }
}

static int _artefact_num_props(const artefact_properties_t &proprt)
{
    int num = 0;

    // Count all properties.
    for (int i = 0; i < ARTP_NUM_PROPERTIES; ++i)
        if (proprt[i] != 0)
            num++;

    return num;
}

static string _get_artefact_type(const item_def &item, bool appear = false)
{
    switch (item.base_type)
    {
    case OBJ_BOOKS:
        return "book";
    case OBJ_WEAPONS:
    case OBJ_STAVES: // XXX: consider a separate section?
        return "weapon";
    case OBJ_ARMOUR:
        return "armour";
    case OBJ_TALISMANS:
        return "talisman";
    case OBJ_JEWELLERY:
        // Distinguish between amulets and rings only in appearance.
        if (!appear)
            return "jewellery";

        if (jewellery_is_amulet(item))
            return "amulet";
        else
            return "ring";
    default:
        return "artefact";
    }
}

static bool _pick_db_name(const item_def &item)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    case OBJ_ARMOUR:
    case OBJ_STAVES:
    case OBJ_TALISMANS:
        return coinflip();
    case OBJ_JEWELLERY:
        return one_chance_in(5);
    default:
        return false;
    }
}

static string _artefact_name_lookup(const item_def &item, const string &lookup)
{
    const string name = getRandNameString(lookup);
    return name.empty() ? name : replace_name_parts(name, item);
}

static bool _artefact_name_lookup(string &result, const item_def &item,
                                  const string &lookup)
{
    result = _artefact_name_lookup(item, lookup);
    return !result.empty();
}

bool item_type_can_be_artefact(object_class_type typ)
{
    switch (typ)
    {
    case OBJ_WEAPONS:
    case OBJ_STAVES:
    case OBJ_ARMOUR:
    case OBJ_JEWELLERY:
    case OBJ_BOOKS:
    case OBJ_TALISMANS:
        return true;
    default:
        return false;
    }
}

static string _base_name(const item_def &item)
{
    if (item.is_type(OBJ_TALISMANS, TALISMAN_DEATH))
        return "death talisman"; // no talismans of death of foo
    return item_base_name(item);
}

string make_artefact_name(const item_def &item, bool appearance)
{
    ASSERT(is_artefact(item));

    ASSERT(item_type_can_be_artefact(item.base_type));

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry *unrand = _seekunrandart(item);
        if (!appearance)
            return unrand->name;
        return unrand->unid_name;
    }

    string lookup;
    string result;

    // Use prefix of gifting god, if applicable.
    bool god_gift = false;
    int item_orig = 0;
    if (!appearance)
    {
        // Divine gifts don't look special, so this is only necessary
        // for actually naming an item.
        item_orig = item.orig_monnum;
        if (item_orig < 0)
            item_orig = -item_orig;
        else
            item_orig = 0;

        if (item_orig > GOD_NO_GOD && item_orig < NUM_GODS)
        {
            lookup += god_name(static_cast<god_type>(item_orig), false) + " ";
            god_gift = true;
        }
    }

    // get base type
    lookup += _get_artefact_type(item, appearance);

    const string base_name = _base_name(item);
    if (appearance)
    {
        string appear = getRandNameString(lookup, " appearance");
        if (appear.empty())
        {
            appear = getRandNameString("general appearance");
            if (appear.empty()) // still nothing found?
                appear = "non-descript";
        }

        result += appear;
        result += " ";
        result += base_name;
        return result;
    }

    if (_pick_db_name(item))
    {
        result += base_name + " ";

        int tries = 100;
        string name;
        do
        {
            (_artefact_name_lookup(name, item, lookup)

             // If nothing found, try god name alone.
             || (god_gift
                 && _artefact_name_lookup(name, item,
                                          god_name(
                                             static_cast<god_type>(item_orig),
                                             false)))

             // If still nothing found, try base type alone.
             || _artefact_name_lookup(name, item, _get_artefact_type(item)));
        }
        while (--tries > 0 && strwidth(name) > 25);

        if (name.empty()) // still nothing found?
            result += "of Bugginess";
        else
            result += name;
    }
    else
    {
        // construct a unique name
        const string st_p = make_name();
        result += base_name;

        if (one_chance_in(3))
        {
            result += " of ";
            result += st_p;
        }
        else
        {
            result += " \"";
            result += st_p;
            result += "\"";
        }
    }

    return result;
}

static const unrandart_entry *_seekunrandart(const item_def &item)
{
    return get_unrand_entry(item.unrand_idx);
}

string get_artefact_base_name(const item_def &item, bool terse)
{
    string base_name = _base_name(item);
    const char* custom_type = _seekunrandart(item)->type_name;
    if (custom_type)
        base_name = custom_type;
    if (terse)
    {
        base_name = replace_all(base_name, "executioner's axe", "exec axe");
        base_name = replace_all(base_name, "giant spiked club", "g.spiked club");
        base_name = replace_all(base_name, "triple crossbow", "triple xbow");
    }
    return base_name;
}

string get_artefact_name(const item_def &item, bool force_known)
{
    ASSERT(is_artefact(item));

    if (item.is_identified() || force_known)
    {
        // print artefact's real name, if that's set
        if (item.props.exists(ARTEFACT_NAME_KEY))
            return item.props[ARTEFACT_NAME_KEY].get_string();
        // other unrands don't use cached names
        if (is_unrandom_artefact(item))
            return _seekunrandart(item)->name;
        return make_artefact_name(item, false);
    }
    // print artefact appearance
    if (item.props.exists(ARTEFACT_APPEAR_KEY))
        return item.props[ARTEFACT_APPEAR_KEY].get_string();
    return make_artefact_name(item, true);
}

void set_artefact_name(item_def &item, const string &name)
{
    ASSERT(is_artefact(item));
    ASSERT(!name.empty());
    item.props[ARTEFACT_NAME_KEY].get_string() = name;
}

int find_unrandart_index(const item_def& artefact)
{
    return artefact.unrand_idx;
}

const unrandart_entry* get_unrand_entry(int unrand_index)
{
    unrand_index -= UNRAND_START;

    if (unrand_index <= -1 || unrand_index >= NUM_UNRANDARTS)
        return &unranddata[0];  // dummy unrandart
    else
        return &unranddata[unrand_index];
}

static int _preferred_max_level(int unrand_index)
{
    // TODO: turn this into a max preferred level field in art-data.txt
    switch (unrand_index)
    {
    case UNRAND_DELATRAS_GLOVES:
        return 6;
    case UNRAND_WOODCUTTERS_AXE:
    case UNRAND_THROATCUTTER:
    case UNRAND_HERMITS_PENDANT:
        return 9;
    case UNRAND_DEVASTATOR:
    case UNRAND_RATSKIN_CLOAK:
    case UNRAND_KRYIAS:
    case UNRAND_LEAR:
    case UNRAND_OCTOPUS_KING:
    case UNRAND_AUGMENTATION:
    case UNRAND_MEEK:
    case UNRAND_ELEMENTAL_VULNERABILITY:
    case UNRAND_MISFORTUNE:
    case UNRAND_FORCE_LANCE:
    case UNRAND_VICTORY:
        return 11;
    default:
        return -1;
    }
}

static int _unrand_weight(int unrand_index, int item_level)
{
    // Early-game unrands (with a preferred max depth != -1) are
    // weighted higher within their depth and lower past it.
    // Normal unrands have a flat weight at all depths.
    const int pref_max_level = _preferred_max_level(unrand_index);
    if (pref_max_level == -1)
        return 10;
    return item_level <= pref_max_level ? 100 : 1;
}

int find_okay_unrandart(uint8_t aclass, uint8_t atype, int item_level, bool in_abyss)
{
    int chosen_unrand_idx = -1;

    // Pick randomly among unrandarts with the proper
    // base_type and sub_type. This will rule out unrands that have already
    // placed as part of levelgen, but may find unrands that have been acquired.
    // because of this, the caller needs to properly set up a fallback randart
    // in some cases: see makeitem.cc:_setup_fallback_randart.
    for (int i = 0, seen_weight = 0; i < NUM_UNRANDARTS; i++)
    {
        const int              index = i + UNRAND_START;
        const unrandart_entry* entry = &unranddata[i];

        // Skip dummy entries.
        if (entry->base_type == OBJ_UNASSIGNED)
            continue;

        const unique_item_status_type status =
            get_unique_item_status(index);

        if ((!in_abyss || status != UNIQ_LOST_IN_ABYSS)
            && status != UNIQ_NOT_EXISTS
            // for previously acquired items, allow generation of the index
            // here; a fallback artefact will replace them in later steps.
            // This is needed for seed stability.
            && status != UNIQ_EXISTS_NONLEVELGEN)
        {
            continue;
        }

        if (in_abyss && status == UNIQ_LOST_IN_ABYSS
            && index == UNRAND_OCTOPUS_KING_RING
            && you.octopus_king_rings == 0xff)
        {
            // the last octopus ring is lost in the abyss. We don't have the
            // machinery to bring back the correct one, and it doesn't seem
            // worth implementing. So just skip it. (TODO: clear the flag for
            // a lost octoring on losing it?)
            continue;
        }

        // If an item does not generate randomly, we can only produce its index
        // here if it was lost in the abyss
        if ((!in_abyss || status != UNIQ_LOST_IN_ABYSS)
            && entry->flags & (UNRAND_FLAG_NOGEN | UNRAND_FLAG_DELETED))
        {
            continue;
        }

        if (entry->base_type != aclass
            || atype != OBJ_RANDOM && entry->sub_type != atype
               // Acquirement.
               && (aclass != OBJ_WEAPONS
                   || item_attack_skill(entry->base_type, atype) !=
                      item_attack_skill(entry->base_type, entry->sub_type)
                   || hands_reqd(&you, entry->base_type,
                                 atype) !=
                      hands_reqd(&you, entry->base_type,
                                 entry->sub_type)))
        {
            continue;
        }

        const int weight = _unrand_weight(index, item_level);
        seen_weight += weight;
        if (x_chance_in_y(weight, seen_weight))
            chosen_unrand_idx = index;
    }

    return chosen_unrand_idx;
}

int get_unrandart_num(const char *name)
{
    string uname = name;
    uname = replace_all(uname, " ", "_");
    uname = replace_all(uname, "'", "");
    lowercase(uname);
    string quoted = "\"";
    quoted += uname;
    quoted += "\"";

    for (unsigned int i = 0; i < ARRAYSZ(unranddata); ++i)
    {
        string art = unranddata[i].name;
        art = replace_all(art, " ", "_");
        art = replace_all(art, "'", "");
        lowercase(art);
        if (art == uname || art.find(quoted) != string::npos)
            return UNRAND_START + i;
    }
    return 0;
}

int extant_unrandart_by_exact_name(string name)
{
    static map<string,int> cache;
    if (cache.empty())
    {
        for (unsigned int i = 0; i < ARRAYSZ(unranddata); ++i)
        {
            const int id = UNRAND_START + i;
            if (unranddata[i].flags & UNRAND_FLAG_DELETED)
                continue;
            cache[lowercase_string(unranddata[i].name)] = id;
        }
    }
    return lookup(cache, lowercase(name), 0);
}

static bool _armour_ego_conflicts(artefact_properties_t &proprt)
{
    switch (proprt[ARTP_BRAND])
    {
    // Opposite effect.
    case SPARM_LIGHT:
        return proprt[ARTP_INVISIBLE];
    case SPARM_GUILE:
        return proprt[ARTP_WILLPOWER];
    case SPARM_ENERGY:
        return proprt[ARTP_PREVENT_SPELLCASTING];

    // Duplicate effect.
    case SPARM_RAMPAGING:
        return proprt[ARTP_RAMPAGING];
    case SPARM_HARM:
        return proprt[ARTP_HARM];
    case SPARM_RESISTANCE:
        return proprt[ARTP_FIRE] || proprt[ARTP_COLD];
    case SPARM_RAGE:
        return proprt[ARTP_ANGRY];
    case SPARM_INVISIBILITY:
        return proprt[ARTP_INVISIBLE];

    default:
        return false;
    }
}

// See also _artp_can_go_on_item
static bool _randart_is_conflicting(const item_def &item,
                                     artefact_properties_t &proprt)
{
    if (proprt[ARTP_PREVENT_SPELLCASTING]
        && (proprt[ARTP_INTELLIGENCE] > 0
            || proprt[ARTP_MAGICAL_POWER] > 0
            || proprt[ARTP_ARCHMAGI]
            || item.base_type == OBJ_STAVES))
    {
        return true;
    }

    if (item.base_type == OBJ_WEAPONS
        && get_weapon_brand(item) == SPWPN_HOLY_WRATH
        && is_demonic(item))
    {
        return true;
    }

    if (item.is_type(OBJ_JEWELLERY, RING_WIZARDRY)
        && proprt[ARTP_INTELLIGENCE] < 0)
    {
        return true;
    }

    if (item.base_type == OBJ_ARMOUR && _armour_ego_conflicts(proprt))
        return true;

    return false;
}

bool randart_is_bad(const item_def &item, artefact_properties_t &proprt)
{
    if (item.base_type == OBJ_BOOKS)
        return false;

    if (_artefact_num_props(proprt) == 0)
        return true;

    // Weapons must have a brand and at least one other property.
    if (item.base_type == OBJ_WEAPONS
        && (proprt[ARTP_BRAND] == SPWPN_NORMAL
            || _artefact_num_props(proprt) < 2))
    {
        return true;
    }

    return _randart_is_conflicting(item, proprt);
}

bool randart_is_bad(const item_def &item)
{
    artefact_properties_t proprt;
    proprt.init(0);
    artefact_properties(item, proprt);

    return randart_is_bad(item, proprt);
}

static void _artefact_setup_prop_vectors(item_def &item)
{
    CrawlHashTable &props = item.props;
    if (!props.exists(ARTEFACT_PROPS_KEY))
        props[ARTEFACT_PROPS_KEY].new_vector(SV_SHORT).resize(ART_PROPERTIES);

    CrawlVector &rap = props[ARTEFACT_PROPS_KEY].get_vector();
    rap.set_max_size(ART_PROPERTIES);

    for (vec_size i = 0; i < ART_PROPERTIES; i++)
        rap[i].get_short() = 0;
}

// If force_mundane is true, normally mundane items are forced to
// nevertheless become artefacts.
bool make_item_randart(item_def &item, bool force_mundane)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    case OBJ_ARMOUR:
    case OBJ_JEWELLERY:
    case OBJ_STAVES:
    case OBJ_TALISMANS:
        break;
    default:
        return false;
    }

    // This item already is a randart.
    if (item.flags & ISFLAG_RANDART)
        return true;

    // Not a truly random artefact.
    if (item.flags & ISFLAG_UNRANDART)
        return false;

    // Mundane items are much less likely to be artefacts.
    if (!force_mundane && item.is_mundane() && !one_chance_in(5))
        return false;

    _artefact_setup_prop_vectors(item);
    item.flags |= ISFLAG_RANDART;

    const god_type god_gift = origin_as_god_gift(item);

    int randart_tries = 500;
    do
    {
        // Now that we found something, initialise the props array.
        if (--randart_tries <= 0 || !_init_artefact_properties(item))
        {
            // Something went wrong that no amount of rerolling will fix.
            item.unrand_idx = 0;
            item.props.erase(ARTEFACT_PROPS_KEY);
            item.flags &= ~ISFLAG_RANDART;
            return false;
        }
    }
    while (randart_is_bad(item)
           || god_gift != GOD_NO_GOD && !_god_fits_artefact(god_gift, item));

    // get true artefact name
    if (item.props.exists(ARTEFACT_NAME_KEY))
        ASSERT(item.props[ARTEFACT_NAME_KEY].get_type() == SV_STR);
    else
        set_artefact_name(item, make_artefact_name(item, false));

    // get artefact appearance
    if (item.props.exists(ARTEFACT_APPEAR_KEY))
        ASSERT(item.props[ARTEFACT_APPEAR_KEY].get_type() == SV_STR);
    else
        item.props[ARTEFACT_APPEAR_KEY].get_string() =
            make_artefact_name(item, true);

    return true;
}

static string _ashenzari_artefact_name(item_def &item)
{
    const int old_orig = item.orig_monnum;

    item.orig_monnum = -GOD_ASHENZARI;

    int tries = 100;
    string name;
    do
    {
        name = _artefact_name_lookup(item, "Ashenzari");
    }
    while (--tries > 0 && strwidth(name) > 25);

    item.orig_monnum = old_orig;

    return _base_name(item) + " " + (name.empty() ? "of Ashenzari" : name);
}

void make_ashenzari_randart(item_def &item)
{
    if (item.base_type != OBJ_WEAPONS
        && item.base_type != OBJ_ARMOUR
        && item.base_type != OBJ_JEWELLERY
        && item.base_type != OBJ_STAVES)
    {
        return;
    }

    // This item already is a randart, just rename
    if (item.flags & ISFLAG_RANDART)
    {
        set_artefact_name(item, _ashenzari_artefact_name(item));
        return;
    }

    // Too special, will just be cursed
    if (item.flags & ISFLAG_UNRANDART)
        return;

    const auto brand = item.brand;

    // Ash randarts get no props
    _artefact_setup_prop_vectors(item);
    item.flags |= ISFLAG_RANDART;
    item.flags |= ISFLAG_IDENTIFIED;

    if (brand != SPWPN_NORMAL)
        set_artefact_brand(item, brand);

    set_artefact_name(item, _ashenzari_artefact_name(item));
    item.props[ARTEFACT_APPEAR_KEY].get_string() =
        make_artefact_name(item, true);

}

enum gizmo_prop_type
{
    GIZMO_REGEN,
    GIZMO_REPEL,
    GIZMO_RAMPAGE,
    GIZMO_GADGETEER,
    GIZMO_PARRYREV,
    GIZMO_SPELLMOTOR,
    GIZMO_AUTODAZZLE,
    LAST_RARE_GIZMO = GIZMO_AUTODAZZLE,

    GIZMO_RF,
    GIZMO_RC,
    GIZMO_RELEC,
    GIZMO_RPOIS,
    GIZMO_SLAY,
    GIZMO_WIZ,

    NUM_GIZMO_PROPS,
};

static void _apply_gizmo_prop(item_def& gizmo, gizmo_prop_type prop)
{
    switch (prop)
    {
        // Common props
        case GIZMO_RF:
            artefact_set_property(gizmo, ARTP_FIRE, 1);
            break;

        case GIZMO_RC:
            artefact_set_property(gizmo, ARTP_COLD, 1);
            break;

        case GIZMO_RELEC:
            artefact_set_property(gizmo, ARTP_ELECTRICITY, 1);
            break;

        case GIZMO_RPOIS:
            artefact_set_property(gizmo, ARTP_POISON, 1);
            artefact_set_property(gizmo, ARTP_RCORR, 1);
            break;

        case GIZMO_SLAY:
            artefact_set_property(gizmo, ARTP_SLAYING, 3);
            break;

        case GIZMO_WIZ:
            artefact_set_property(gizmo, ARTP_WIZARDRY, 1);
            artefact_set_property(gizmo, ARTP_MAGICAL_POWER, 4);
            break;

        // Rare props
        case GIZMO_REGEN:
            artefact_set_property(gizmo, ARTP_REGENERATION, 1);
            artefact_set_property(gizmo, ARTP_MANA_REGENERATION, 1);
            break;

        case GIZMO_REPEL:
            artefact_set_property(gizmo, ARTP_RMSL, 1);
            artefact_set_property(gizmo, ARTP_CLARITY, 1);
            break;

        case GIZMO_RAMPAGE:
            artefact_set_property(gizmo, ARTP_RAMPAGING, 1);
            artefact_set_property(gizmo, ARTP_ACROBAT, 1);
            break;

        case GIZMO_GADGETEER:
            gizmo.brand = SPGIZMO_GADGETEER;
            break;

        case GIZMO_PARRYREV:
            gizmo.brand = SPGIZMO_PARRYREV;
            break;

        case GIZMO_SPELLMOTOR:
            gizmo.brand = SPGIZMO_SPELLMOTOR;
            break;

        case GIZMO_AUTODAZZLE:
            gizmo.brand = SPGIZMO_AUTODAZZLE;
            break;

        default:
            break;
    }
}

// Takes a list of existing gizmos in a CrawlVector (having been created by
// acquirement) and fills out their properties.
void fill_gizmo_properties(CrawlVector& gizmos)
{
    // Shuffle all props
    vector<gizmo_prop_type> rare_props;
    for (int i = 0; i <= LAST_RARE_GIZMO; ++i)
    {
        const gizmo_prop_type ptype = static_cast<gizmo_prop_type>(i);

        // Don't generate the gizmo prop for Chei worshippers, since the rampage
        // will prevent them from using the entire thing.
        if (you_worship(GOD_CHEIBRIADOS) && ptype == GIZMO_RAMPAGE)
            continue;

        rare_props.push_back(ptype);
    }
    shuffle_array(rare_props);

    vector<gizmo_prop_type> common_props;
    for (int i = LAST_RARE_GIZMO + 1; i < NUM_GIZMO_PROPS; ++i)
        common_props.push_back(static_cast<gizmo_prop_type>(i));
    shuffle_array(common_props);

    int num = min(3, (int)gizmos.size());
    for (int i = 0; i < num; ++i)
    {
        item_def& gizmo = gizmos[i].get_item();
        _artefact_setup_prop_vectors(gizmo);
        gizmo.flags |= ISFLAG_RANDART;

        // Apply 1 rare prop and 2 common props to each gizmo.
        // (We are gauranteed to see all common props each run, but only a few
        // of the rare props)
        _apply_gizmo_prop(gizmo, rare_props[i]);
        _apply_gizmo_prop(gizmo, common_props[i * 2]);
        _apply_gizmo_prop(gizmo, common_props[i * 2 + 1]);
    }
}

static void _make_faerie_armour(item_def &item)
{
    item_def doodad;
    for (int i=0; i<100; i++)
    {
        doodad.clear();
        doodad.base_type = item.base_type;
        doodad.sub_type = item.sub_type;
        if (!make_item_randart(doodad))
        {
            i--; // Forbidden props are not absolute, artefactness is.
            continue;
        }

        // -Cast makes no sense on someone called "the Enchantress".
        if (artefact_property(doodad, ARTP_PREVENT_SPELLCASTING))
            continue;

        if (one_chance_in(20))
            artefact_set_property(doodad, ARTP_CLARITY, 1);
        if (one_chance_in(20))
            artefact_set_property(doodad, ARTP_MAGICAL_POWER, 1 + random2(10));
        if (one_chance_in(20))
            artefact_set_property(doodad, ARTP_HP, random2(16) - 5);

        break;
    }
    ASSERT(is_artefact(doodad));
    ASSERT(doodad.sub_type == item.sub_type);

    doodad.props[ARTEFACT_APPEAR_KEY].get_string()
        = item.props[ARTEFACT_APPEAR_KEY].get_string();
    doodad.props.erase(ARTEFACT_NAME_KEY);
    item.props = doodad.props;

    // On body armour, an enchantment of less than 0 is never viable.
    int high_plus = random2(6) - 2;
    high_plus += random2(6);
    item.plus = max(high_plus, random2(2));
}

static jewellery_type octoring_types[8] =
{
    RING_SEE_INVISIBLE, RING_PROTECTION_FROM_FIRE, RING_PROTECTION_FROM_COLD,
    RING_RESIST_CORROSION, RING_FLIGHT, RING_WIZARDRY, RING_MAGICAL_POWER,
    RING_POSITIVE_ENERGY
};

static void _make_octoring(item_def &item)
{
    if (you.octopus_king_rings == 0xff)
    {
        // possible this is too narrow: if this causes unexpected wizmode
        // crashes, just back off to asserting you.wizard.
        ASSERT(crawl_state.prev_cmd == CMD_WIZARD);
        item.sub_type = octoring_types[random2(8)];
        return;
    }

    int which = 0;
    do which = random2(8); while (you.octopus_king_rings & (1 << which));

    item.sub_type = octoring_types[which];

    // Save that we've found that particular type
    you.octopus_king_rings |= 1 << which;

    // If there are any types left, unset the 'already found' flag
    if (you.octopus_king_rings != 0xff)
        _set_unique_item_existence(UNRAND_OCTOPUS_KING_RING, false);
}

bool make_item_unrandart(item_def &item, int unrand_index)
{
    ASSERT_RANGE(unrand_index, UNRAND_START + 1, (UNRAND_START + NUM_UNRANDARTS));
    rng::subgenerator item_rng; // for safety's sake, this is sometimes called
                                // directly

    item.unrand_idx = unrand_index;

    const unrandart_entry *unrand = &unranddata[unrand_index - UNRAND_START];

    item.flags |= ISFLAG_UNRANDART;
    _artefact_setup_prop_vectors(item);
    _init_artefact_properties(item);

    // get artefact appearance
    ASSERT(!item.props.exists(ARTEFACT_APPEAR_KEY));
    item.props[ARTEFACT_APPEAR_KEY].get_string() = unrand->unid_name;

    _set_unique_item_existence(unrand_index, true);

    if (unrand_index == UNRAND_FAERIE)
        _make_faerie_armour(item);
    else if (unrand_index == UNRAND_OCTOPUS_KING_RING)
        _make_octoring(item);
    else if (unrand_index == UNRAND_WOE && !you.has_mutation(MUT_NO_GRASPING)
             && !you.could_wield(item, true, true))
    {
        // always wieldable, always 2-handed
        item.sub_type = WPN_BROAD_AXE;
    }

    if (!(unrand->flags & UNRAND_FLAG_UNIDED))
        item.flags |= ISFLAG_IDENTIFIED;

    return true;
}

void unrand_reacts()
{
    for (int i = 0; i < NUM_EQUIP; i++)
    {
        if (!you.unrand_reacts[i])
            continue;

        item_def&        item  = you.inv[you.equip[i]];
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);
        ASSERT(entry);

        entry->world_reacts_func(&item);
    }
}

void unrand_death_effects(monster* mons, killer_type killer)
{
    for (int i = 0; i < NUM_EQUIP; i++)
    {
        item_def* item = you.slot_item(static_cast<equipment_type>(i));

        if (item && is_unrandom_artefact(*item))
        {
            const unrandart_entry* entry = get_unrand_entry(item->unrand_idx);

            if (entry->death_effects)
                entry->death_effects(item, mons, killer);
        }
    }
}

void artefact_set_property(item_def &item, artefact_prop_type prop, int val)
{
    ASSERT(is_artefact(item));
    ASSERT(item.props.exists(ARTEFACT_PROPS_KEY));

    CrawlVector &rap_vec = item.props[ARTEFACT_PROPS_KEY].get_vector();
    ASSERT(rap_vec.get_type()     == SV_SHORT);
    ASSERT(rap_vec.size()         == ART_PROPERTIES);
    ASSERT(rap_vec.get_max_size() == ART_PROPERTIES);

    rap_vec[prop].get_short() = val;
}

template<typename Z>
static inline void artefact_pad_store_vector(CrawlVector &vec, Z value)
{
    if (vec.get_max_size() < ART_PROPERTIES)
    {
        // Authentic tribal dance to propitiate the asserts in store.cc:
        const int old_size = vec.get_max_size();
        vec.set_max_size(VEC_MAX_SIZE);
        vec.resize(ART_PROPERTIES);
        vec.set_max_size(ART_PROPERTIES);
        for (int i = old_size; i < ART_PROPERTIES; ++i)
            vec[i] = value;
    }
}

void artefact_fixup_props(item_def &item)
{
    CrawlHashTable &props = item.props;
    if (props.exists(ARTEFACT_PROPS_KEY))
        artefact_pad_store_vector(props[ARTEFACT_PROPS_KEY], short(0));

    // As of 0.30, it seems like there is some rare circumstance that can
    // cause a Hepliaklqana ancestor's weapon to become a half-baked artefact -
    // ISFLAG_RANDART set, but ARTEFACT_PROPS_KEY not. Until we understand
    // what's happening, fix things here to salvage broken saves.
    // (This seems to be related to
    // https://crawl.develz.org/mantis/view.php?id=11756 - see also abyss.cc.
    if (item.base_type == OBJ_WEAPONS
        && (item.flags & (ISFLAG_SUMMONED | ISFLAG_RANDART))
        && !item.props.exists(ARTEFACT_PROPS_KEY))
    {
        item.flags &= ~ISFLAG_RANDART;
    }
}
