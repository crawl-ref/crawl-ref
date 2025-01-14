/**
 * @file
 * @brief Misc religion related functions.
**/

#include "AppHdr.h"

#include "religion.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <sstream>

#include "ability.h"
#include "acquire.h"
#include "act-iter.h"
#include "areas.h"
#include "attitude-change.h"
#include "branch.h"
#include "chardump.h"
#include "coordit.h"
#include "dactions.h"
#include "database.h"
#include "decks.h"
#include "delay.h"
#include "describe-god.h"
#include "dgn-event.h"
#include "dlua.h"
#include "english.h"
#include "env.h"
#include "fineff.h"
#include "god-abil.h"
#include "god-companions.h"
#include "god-conduct.h"
#include "god-item.h"
#include "god-passive.h"
#include "god-prayer.h"
#include "god-wrath.h"
#include "hints.h"
#include "hiscores.h"
#include "invent.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "level-state-type.h"
#include "libutil.h"
#include "makeitem.h"
#include "message.h"
#include "mon-death.h"
#include "mon-gear.h" // give_shield
#include "mon-pick.h"
#include "mon-place.h"
#include "mon-tentacle.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "notes.h"
#include "output.h"
#include "player-equip.h"
#include "player-stats.h"
#include "prompt.h"
#include "randbook.h"
#include "shopping.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-summoning.h"
#include "sprint.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "transform.h"
#include "view.h"

#ifdef DEBUG_RELIGION
#    define DEBUG_DIAGNOSTICS
#    define DEBUG_GIFTS
#    define DEBUG_SACRIFICE
#    define DEBUG_PIETY
#endif

#define PIETY_HYSTERESIS_LIMIT 1

#define MIN_IGNIS_PIETY_KEY "min_ignis_piety"
#define YRED_SEEN_ZOMBIE_KEY "yred_seen_zombie"

static weapon_type _hepliaklqana_weapon_type(monster_type mc, int HD);
static brand_type _hepliaklqana_weapon_brand(monster_type mc, int HD);
static armour_type _hepliaklqana_shield_type(monster_type mc, int HD);
static special_armour_type _hepliaklqana_shield_ego(int HD);

const vector<vector<god_power>> & get_all_god_powers()
{
    static vector<vector<god_power>> god_powers =
    {
        // no god
        { },

        // Zin
        {   { 1, ABIL_ZIN_RECITE, "recite Zin's Axioms of Law" },
            { 2, ABIL_ZIN_VITALISATION, "call upon Zin for vitalisation" },
            { 3, ABIL_ZIN_IMPRISON, "call upon Zin to imprison the lawless" },
            { 5, ABIL_ZIN_SANCTUARY, "call upon Zin to create a sanctuary" },
            { 6, "Zin will now cleanse your potions of mutation.",
                 "Zin will no longer cleanse your potions of mutation.",
                 "Zin will cleanse your potions of mutation." },
            {-1, ABIL_ZIN_DONATE_GOLD, "donate money to Zin" },
        },

        // TSO
        {   { 1, "You and your allies can now gain power from killing the unholy and evil.",
                 "You and your allies can no longer gain power from killing the unholy and evil.",
                 "You and your allies can gain power from killing the unholy and evil." },
            { 1, ABIL_TSO_DIVINE_SHIELD, "call upon the Shining One for a divine shield" },
            { 3, ABIL_TSO_CLEANSING_FLAME, "channel blasts of cleansing flame", },
            { 5, ABIL_TSO_SUMMON_DIVINE_WARRIOR, "summon a divine warrior" },
            { 7, ABIL_TSO_BLESS_WEAPON,
                 "The Shining One will bless your weapon with holy wrath... once.",
                 "The Shining One is no longer ready to bless your weapon." },
        },

        // Kikubaaqudgha
        {
            { 1, ABIL_KIKU_UNEARTH_WRETCHES, "bring forth the wretched and dying" },
            { 2, "Kikubaaqudgha is now protecting you from necromantic miscasts and death curses.",
                 "Kikubaaqudgha will no longer protect you from necromantic miscasts or death curses.",
                 "Kikubaaqudgha protects you from necromantic miscasts and death curses." },
            { 4, "Kikubaaqudgha is now protecting you from unholy torment.",
                 "Kikubaaqudgha will no longer protect you from unholy torment.",
                 "Kikubaaqudgha protects you from unholy torment." },
            { 5, ABIL_KIKU_SIGN_OF_RUIN, "invoke the sign of ruin" },
            { 7, ABIL_KIKU_BLESS_WEAPON,
                 "Kikubaaqudgha will grant you forbidden knowledge or bloody your weapon with pain... once.",
                 "Kikubaaqudgha is no longer ready to enhance your necromancy." },
            { 7, ABIL_KIKU_GIFT_CAPSTONE_SPELLS,
                 "Kikubaaqudgha will grant you forbidden knowledge.",
                 "Kikubaaqudgha is no longer ready to enhance your necromancy." },
        },

        // Yredelemnul
        {   { 0, ABIL_YRED_LIGHT_THE_TORCH, "light the black torch and reap souls in Yredelemnul's name" },
            { 0, ABIL_YRED_RECALL_UNDEAD_HARVEST, "recall your undead harvest" },
            { 1, "Yredelemnul will now gift you temporary undead servants when you light the torch.",
                 "Yredelemnul will no longer gift you temporary undead servants when you light the torch.",
                 "Yredelemnul will gift you temporary undead servants when you light the torch." },
            { 2, ABIL_YRED_HURL_TORCHLIGHT, "hurl gouts of umbral torchlight" },
            { 4, ABIL_YRED_BIND_SOUL, "bind living souls" },
            { 5, ABIL_YRED_FATHOMLESS_SHACKLES, "engulf your surroundings in Yredelemnul's grip" },
        },

        // Xom
        { },

        // Vehumet
        {   { 1, "gain magical power from killing" },
            { 3, "Vehumet is now aiding your destructive spells.",
                 "Vehumet will no longer aid your destructive spells.",
                 "Vehumet aids your destructive spells." },
            { 4, "Vehumet is now extending the range of your destructive spells.",
                 "Vehumet will no longer extend the range of your destructive spells.",
                 "Vehumet extends the range of your destructive spells." },
        },

        // Okawaru
        {
            {-1, "", "", "Okawaru requires that you fight alone, and prevents you from gaining allies." },
            { 1, ABIL_OKAWARU_HEROISM, "gain great but temporary skills" },
            { 4, ABIL_OKAWARU_FINESSE, "speed up your combat" },
            { 5, ABIL_OKAWARU_DUEL, "enter into single combat with a foe"},
            { 5, "Okawaru will now gift you throwing weapons as you gain piety.",
                 "Okawaru will no longer gift you throwing weapons.",
                 "Okawaru will gift you throwing weapons as you gain piety." },
            { 6, ABIL_OKAWARU_GIFT_WEAPON,
                 "Okawaru will grant you a choice of weapons... once.",
                 "Okawaru is no longer ready to gift you weaponry." },
            { 6, ABIL_OKAWARU_GIFT_ARMOUR,
                 "Okawaru will grant you a choice of armour... once.",
                 "Okawaru is no longer ready to gift you armour." },
        },

        // Makhleb
        {   { 1, "gain health from killing" },
            { 2, ABIL_MAKHLEB_DESTRUCTION,
                 "unleash Makhleb's destructive might" },
            { 3, ABIL_MAKHLEB_INFERNAL_SERVANT,
                 "summon an infernal servant of Makhleb" },
            { 4, "", ""},   // XXX: A marker, replaced by dynamic text in _describe_god_powers()
            { -1, ABIL_MAKHLEB_VESSEL_OF_SLAUGHTER, ""},
            { 7, ABIL_MAKHLEB_BRAND_SELF_1,
                 "Makhleb will allow you to brand your body with an infernal mark... once.",
                 "Mahkleb will no longer allow you to brand your body with an infernal mark."},
            { 7, ABIL_MAKHLEB_BRAND_SELF_2, ""},
            { 7, ABIL_MAKHLEB_BRAND_SELF_3, ""},
        },

        // Sif Muna
        {   { 1, ABIL_SIF_MUNA_CHANNEL_ENERGY,
                 "call upon Sif Muna for magical energy" },
            { 3, ABIL_SIF_MUNA_FORGET_SPELL,
                 "freely open your mind to new spells",
                 "forget spells at will" },
            { 4, ABIL_SIF_MUNA_DIVINE_EXEGESIS,
                 "call upon Sif Muna to cast any spell from your library" },
            { 5, "Sif Muna will now gift you books as you gain piety.",
                 "Sif Muna will no longer gift you books.",
                 "Sif Muna will gift you books as you gain piety." },
        },

        // Trog
        {
            { 1, ABIL_TROG_BERSERK, "go berserk at will" },
            { 2, ABIL_TROG_HAND,
                 "call upon Trog for regeneration and willpower" },
            { 4, ABIL_TROG_BROTHERS_IN_ARMS, "call in reinforcements" },
            { 5, "Trog will now gift you melee weapons as you gain piety.",
                 "Trog will no longer gift you weapons.",
                 "Trog will gift you melee weapons as you gain piety." },
        },

        // Nemelex
        {
            { 0, "draw from decks of power" },
            { 1, "Nemelex will now gift you decks of power as you gain piety.",
                 "Nemelex will no longer gift you decks.",
                 "Nemelex will gift you decks of power as you gain piety." },
            { 3, ABIL_NEMELEX_TRIPLE_DRAW, "choose one out of three cards" },
            { 4, ABIL_NEMELEX_DEAL_FOUR, "deal four cards at a time" },
            { 5, ABIL_NEMELEX_STACK_FIVE, "stack five cards from your decks",
                                        "stack cards" },
        },

        // Elyvilon
        {
            { 1, ABIL_ELYVILON_PURIFICATION, "purify yourself" },
            { 2, ABIL_ELYVILON_HEAL_OTHER, "heal and attempt to pacify others" },
            { 3, ABIL_ELYVILON_HEAL_SELF, "provide healing for yourself" },
            { 5, ABIL_ELYVILON_DIVINE_VIGOUR, "call upon Elyvilon for divine vigour" },
        },

        // Lugonu
        {   {-1, "", "", "Lugonu protects you from the effects of unwielding distortion weapons." },
            {-1, "", "", "Lugonu may banish nearby foes when other gods try to punish you." },
            { 1, ABIL_LUGONU_ABYSS_EXIT,
                 "depart the Abyss",
                 "depart the Abyss at will" },
            { 2, ABIL_LUGONU_BANISH, "banish your foes" },
            { 4, ABIL_LUGONU_CORRUPT, "corrupt the fabric of space" },
            { 5, ABIL_LUGONU_ABYSS_ENTER, "gate yourself to the Abyss" },
            { 7, ABIL_LUGONU_BLESS_WEAPON,
                 "Lugonu will corrupt your weapon with distortion... once.",
                 "Lugonu is no longer ready to corrupt your weapon." },
        },

        // Beogh
        {   { 2, ABIL_BEOGH_SMITING, "smite your foes" },
            { 1, ABIL_BEOGH_RECALL_APOSTLES, "recall your apostles" },
            { 3, "Beogh will now send orc apostles to challenge you in battle as you gain piety.",
                 "Beogh will no longer send orc apostles to challenge you in battle.",
                 "Beogh will send orc apostles to challenge you in battle as you gain piety." },
            { 0, "", "", "You can recruit apostles that you defeat into your service." },
            { 3, "", "", "Your apostles are sometimes healed when you deal damage." },
            { 5, ABIL_BEOGH_BLOOD_FOR_BLOOD, "rally a vengeful horde" },
            { 0, ABIL_BEOGH_RECRUIT_APOSTLE, "" },
            { 0, ABIL_BEOGH_DISMISS_APOSTLE_1, ""},
            { 0, ABIL_BEOGH_DISMISS_APOSTLE_2, ""},
            { 0, ABIL_BEOGH_DISMISS_APOSTLE_3, ""},
            { 5, "walk on water" },
        },

        // Jiyva
        {   { 2, "Jiyva is now protecting you from corrosive effects.",
                 "Jiyva will no longer protect you from corrosive effects.",
                 "Jiyva protects you from corrosive effects." },
            { 3, "Jiyva will now mutate your body as you gain piety.",
                 "Jiyva will no longer mutate your body.",
                 "Jiyva will mutate your body as you gain piety." },
            { 3, ABIL_JIYVA_OOZEMANCY, "call acidic ooze from nearby walls" },
            { 4, ABIL_JIYVA_SLIMIFY, "turn your foes to slime" },
            { 5, "You may now expel jellies when seriously injured.",
                 "You will no longer expel jellies when injured.",
                 "You may expel jellies when seriously injured." },
        },

        // Fedhas
        {
            { 0, "", "", "You can walk through plants and fire through allied plants." },
            { 2, ABIL_FEDHAS_WALL_OF_BRIARS, "encircle yourself with summoned briar patches"},
            { 3, ABIL_FEDHAS_GROW_BALLISTOMYCETE, "grow a ballistomycete" },
            { 4, ABIL_FEDHAS_OVERGROW, "transform dungeon walls and trees into plant allies"},
            { 5, ABIL_FEDHAS_GROW_OKLOB, "grow an oklob plant" },
        },

        // Cheibriados
        {   { 0, "Cheibriados is now slowing the effects of poison on you.",
                 "Cheibriados will no longer slow the effects of poison on you.",
                 "Cheibriados slows the effects of poison on you." },
            { 1, ABIL_CHEIBRIADOS_TIME_BEND, "bend time to slow others" },
            { 3, ABIL_CHEIBRIADOS_DISTORTION, "warp the flow of time around you" },
            { 4, ABIL_CHEIBRIADOS_SLOUCH, "inflict damage on those overly hasty" },
            { 5, ABIL_CHEIBRIADOS_TIME_STEP, "step out of the flow of time" },
        },

        // Ashenzari
        {   { 0, "Ashenzari warns you of distant threats and treasures.\n"
                 "Ashenzari shows you where magical portals lie." },
            { 1, "Ashenzari will now identify your possessions.",
                 "Ashenzari will no longer identify your possesions.",
                 "Ashenzari identifies your possessions." },
            { 2, "Ashenzari will now reveal the unseen.",
                 "Ashenzari will no longer reveal the unseen.",
                 "Ashenzari reveals the unseen." },
            { 3, "Ashenzari will now keep your mind clear.",
                 "Ashenzari will no longer keep your mind clear.",
                 "Ashenzari keeps your mind clear." },
            { 4, "Ashenzari will now protect you from the dungeon's malevolent forces.",
                 "Ashenzari no longer protects you from the dungeon's malevolent forces.",
                 "Ashenzari protects you from the dungeon's malevolent forces." },
            { 4, "Ashenzari reveals the structure of the nearby dungeon.",
                 "Ashenzari no longer reveals the structure of the nearby dungeon." },
        },

        // Dithmenos
        {   { 1, "Dithmenos quiets all noise in your surroundings.",
                 "Dithmenos no longer quiets all noise in your surroundings."},
            { 2, "Your shadow now sometimes tangibly mimics your actions.",
                 "Your shadow no longer tangibly mimics your actions.",
                 "Your shadow sometimes tangibly mimics your actions." },
            { 3, ABIL_DITHMENOS_SHADOWSLIP,
                 "mislead enemies by swapping places with your shadow" },
            { 4, ABIL_DITHMENOS_APHOTIC_MARIONETTE,
                 "unleash an enemy's spells as if they were a marionette"},
            { 5, ABIL_DITHMENOS_PRIMORDIAL_NIGHTFALL,
                 "surround yourself with impenetrable night"},
        },

        // Gozag
        {   { 0, "", "", "Gozag turns your defeated foes' bodies to gold." },
            { 0, "", "", "Your enemies may become distracted by gold." },
            { 0, ABIL_GOZAG_POTION_PETITION, "petition Gozag for potion effects" },
            { 0, ABIL_GOZAG_CALL_MERCHANT,
                 "fund merchants seeking to open stores in the dungeon" },
            { 0, ABIL_GOZAG_BRIBE_BRANCH,
                 "bribe branches to halt enemies' attacks and recruit allies" },
        },

        // Qazlal
        {
            { 0, "Qazlal grants you and your divine allies immunity to clouds." },
            { 1, "You are now surrounded by a storm.",
                 "Your storm dissipates completely.",
                 "You are surrounded by a storm." },
            { 2, ABIL_QAZLAL_UPHEAVAL, "call upon nature to destroy your foes" },
            { 3, ABIL_QAZLAL_ELEMENTAL_FORCE, "give life to nearby clouds" },
            { 4, "The storm surrounding you is now powerful enough to repel missiles.",
                 "The storm surrounding you is now too weak to repel missiles.",
                 "The storm surrounding you is powerful enough to repel missiles." },
            { 4, "You will now adapt resistances upon receiving elemental damage.",
                 "You will no longer adapt resistances upon receiving elemental damage.",
                 "You adapt resistances upon receiving elemental damage." },
            { 5, ABIL_QAZLAL_DISASTER_AREA,
                 "call upon nature's wrath in a wide area around you" },
        },

        // Ru
        {   { 1, "You now exude an aura of power that intimidates your foes.",
                 "You no longer exude an aura of power that intimidates your foes.",
                 "You now exude an aura of power that intimidates your foes." },
            { 2, "Your aura of power can now strike those that harm you.",
                 "Your aura of power no longer strikes those that harm you.",
                 "Your aura of power can strike those that harm you." },
            { 3, ABIL_RU_DRAW_OUT_POWER, "heal your body and restore your magic" },
            { 4, ABIL_RU_POWER_LEAP, "gather your power into a mighty leap" },
            { 5, ABIL_RU_APOCALYPSE, "wreak a terrible wrath on your foes" },
        },

#if TAG_MAJOR_VERSION == 34
        // Pakellas
        {
            { 0, "gain magical power from killing" },
        },
#endif

        // Uskayaw
        {
            { 1, ABIL_USKAYAW_STOMP, "stomp with the beat" },
            { 2, ABIL_USKAYAW_LINE_PASS, "pass through a line of other dancers" },
            { 3, "Uskayaw will force your foes to helplessly watch your dance.",
                 "Uskayaw will no longer force your foes to helplessly watch your dance."},
            { 4, "Uskayaw will force your foes to share their pain.",
                 "Uskayaw will no longer force your foes to share their pain."},
            { 5, ABIL_USKAYAW_GRAND_FINALE, "merge with and destroy a victim" },
        },

        // Hepliaklqana
        {   { 1, "", "", "Your ancestor manifests to aid you." },
            { 1, ABIL_HEPLIAKLQANA_RECALL, "recall your ancestor" },
            { 1, ABIL_HEPLIAKLQANA_IDENTITY, "remember your ancestor's identity" },
            { 3, ABIL_HEPLIAKLQANA_TRANSFERENCE, "swap creatures with your ancestor" },
            { 4, ABIL_HEPLIAKLQANA_IDEALISE, "heal and protect your ancestor" },
            { 5, "You now drain nearby creatures when transferring your ancestor.",
                 "You no longer drain nearby creatures when transferring your ancestor.",
                 "You drain nearby creatures when transferring your ancestor." },
        },

        // Wu Jian
        {   { 0, "perform damaging attacks by moving towards foes",
                 "perform lunging strikes" },
            { 1, "lightly attack monsters by moving around them",
                 "perform spinning attacks" },
            { 2, ABIL_WU_JIAN_WALLJUMP,
                 "perform airborne attacks" },
            { 3, ABIL_WU_JIAN_SERPENTS_LASH, "briefly move at supernatural speeds",
                 "move at supernatural speeds" },
            { 5, ABIL_WU_JIAN_HEAVENLY_STORM,
                 "summon a storm of heavenly clouds to empower your attacks",
                 "summon a storm of heavenly clouds" },
        },

        // Ignis
        {
            // It would be nice to specify explicitly that this is rF+
            // Unfortunately, including parentheses at the end here breaks
            // the UI on Webtiles.
            { 0, "", "", "You are resistant to fire." },
            { 1, ABIL_IGNIS_FIERY_ARMOUR, "armour yourself in flame" },
            { 1, ABIL_IGNIS_FOXFIRE, "call a swarm of foxfires against your foes" },
            { 7, ABIL_IGNIS_RISING_FLAME, "rocket upward and away, once" },
        },
    };
    static bool god_powers_init = false;

    if (!god_powers_init)
    {
        ASSERT(god_powers.size() == NUM_GODS);
        for (int i = 0; i < NUM_GODS; i++)
            for (auto &p : god_powers[i])
                p.god = static_cast<god_type>(i);
        god_powers_init = true;
    }
    return god_powers;
}

vector<god_power> get_god_powers(god_type god)
{
    vector<god_power> ret;
    for (const auto& power : get_all_god_powers()[god])
    {
        // hack :( don't show fake hp restore
        if (god == GOD_VEHUMET && power.rank == 1
            && you.has_mutation(MUT_HP_CASTING))
        {
            continue;
        }
        if (!(power.abil != ABIL_NON_ABILITY
              && fixup_ability(power.abil) == ABIL_NON_ABILITY))
        {
            ret.push_back(power);
        }
    }
    return ret;
}

/**
 * Print a description of getting/losing this power.
 *
 * @param gaining If true, use this->gain; otherwise, use this->loss.
 * @param fmt  A string containing "%s" that will be used as a format
 *             string with our string as parameter; it is not used if
 *             our string begins with a capital letter. IF THIS DOES
 *             NOT CONTAIN "%s", OR CONTAINS OTHER FORMAT SPECIFIERS,
 *             BEHAVIOUR IS UNDEFINED.
 * @return a string suitable for being read by the user.
 */
void god_power::display(bool gaining, const char* fmt) const
{
    // hack: don't mention the necronomicon alone unless it wasn't
    // already mentioned by the other message
    if (abil == ABIL_KIKU_GIFT_CAPSTONE_SPELLS
        && !you.has_mutation(MUT_NO_GRASPING))
    {
        return;
    }

    // Uskayaw uses short-time-scale piety where the gain/loss messasges
    // are not informative while running
    if (you.running && you_worship(GOD_USKAYAW))
        return;

    const char* str = gaining ? gain : loss;

    // Don't print empty messages about 'hidden' abilities
    if (str[0] == '\0')
        return;

    if (isupper(str[0]))
        god_speaks(you.religion, str);
    else
        god_speaks(you.religion, make_stringf(fmt, str).c_str());
}

static void _place_delayed_monsters();

bool is_evil_god(god_type god)
{
    return god == GOD_KIKUBAAQUDGHA
           || god == GOD_MAKHLEB
           || god == GOD_YREDELEMNUL
           || god == GOD_BEOGH
           || god == GOD_LUGONU;
}

bool is_good_god(god_type god)
{
    return god == GOD_ZIN
           || god == GOD_SHINING_ONE
           || god == GOD_ELYVILON;
}

bool is_chaotic_god(god_type god)
{
    return god == GOD_XOM
           || god == GOD_MAKHLEB
           || god == GOD_NEMELEX_XOBEH
           || god == GOD_LUGONU
           || god == GOD_JIYVA;
}

bool is_unknown_god(god_type god)
{
    return god == GOD_NAMELESS;
}

// Not appearing in new games, but still extant.
static bool _is_disabled_god(god_type god)
{
    switch (god)
    {
#if TAG_MAJOR_VERSION == 34
    // Disabled, pending a rework.
    case GOD_PAKELLAS:
        return true;
#endif
    default:
        return false;
    }
}

bool is_unavailable_god(god_type god)
{
    if (_is_disabled_god(god))
        return true;

    if (god == GOD_JIYVA && jiyva_is_dead())
        return true;

    if (god == GOD_IGNIS && ignis_is_dead())
        return true;

    return false;
}

bool god_has_name(god_type god)
{
    return god != GOD_NO_GOD && god != GOD_NAMELESS;
}

god_type random_god()
{
    god_type god;

    do
    {
        god = static_cast<god_type>(random2(NUM_GODS - 1) + 1);
    }
    while (is_unavailable_god(god));

    return god;
}


god_iterator::god_iterator() :
    i(0) { } // might be ok to start with GOD_ZIN instead?

god_iterator::operator bool() const
{
    return i < NUM_GODS;
}

god_type god_iterator::operator*() const
{
    if (i < NUM_GODS)
        return (god_type)i;
    else
        return GOD_NO_GOD;
}

god_type god_iterator::operator->() const
{
    return **this;
}

god_iterator& god_iterator::operator++()
{
    ++i;
    return *this;
}

god_iterator god_iterator::operator++(int)
{
    god_iterator copy = *this;
    ++(*this);
    return copy;
}

void handle_traitor(god_type god)
{
    if (static_cast<god_type>(you.attribute[ATTR_TRAITOR]) == god)
        {
            you.attribute[ATTR_TRAITOR] = 0;
            you.redraw_title = true;
        }
}

bool active_penance(god_type god)
{
    // Good gods only have active wrath when they hate your current god.
    return player_under_penance(god)
           && !is_unavailable_god(god)
           && god != GOD_ASHENZARI
           && god != GOD_GOZAG
           && god != GOD_RU
           && god != GOD_HEPLIAKLQANA
#if TAG_MAJOR_VERSION == 34
           && god != GOD_PAKELLAS
#endif
           && god != GOD_ELYVILON
           && (god == you.religion && !(is_good_god(god) || god == GOD_BEOGH)
               || god_hates_your_god(god, you.religion));
}

// True for gods whose wrath is passive and expires with XP gain.
bool xp_penance(god_type god)
{
    return player_under_penance(god)
           && !is_unavailable_god(god)
           && (god == GOD_ASHENZARI
               || god == GOD_GOZAG
               || god == GOD_HEPLIAKLQANA
#if TAG_MAJOR_VERSION == 34
               || god == GOD_PAKELLAS
#endif
               || god == GOD_ELYVILON)
           && god_hates_your_god(god, you.religion);
}

void dec_penance(god_type god, int val)
{
    if (val <= 0 || you.penance[god] <= 0)
        return;

#ifdef DEBUG_PIETY
    mprf(MSGCH_DIAGNOSTICS, "Decreasing penance by %d", val);
#endif
    if (you.penance[god] <= val)
    {
        const bool had_halo = have_passive(passive_t::halo);
        const bool had_umbra = have_passive(passive_t::umbra);

        you.penance[god] = 0;

        mark_milestone("god.mollify",
                       "mollified " + god_name(god) + ".");

        if (god == GOD_IGNIS)
        {
            simple_god_message(", with one final cry of rage, "
                               "burns out of existence.", false, god);
            add_daction(DACT_REMOVE_IGNIS_ALTARS);
        }
        else
        {
            const bool dead_jiyva = (god == GOD_JIYVA && jiyva_is_dead());
            simple_god_message(
                make_stringf(" seems mollified%s.",
                             dead_jiyva ? ", and vanishes" : "").c_str(),
                false, god);

            if (dead_jiyva)
                add_daction(DACT_JIYVA_DEAD);
        }


        take_note(Note(NOTE_MOLLIFY_GOD, god));

        if (you_worship(god))
        {
            // Redraw piety display and, in case the best skill is Invocations,
            // redraw the god title.
            you.redraw_title = true;

            // TSO's halo is once more available.
            if (!had_halo && have_passive(passive_t::halo))
            {
                mprf(MSGCH_GOD, "Your divine halo returns!");
                invalidate_agrid(true);
            }
            if (!had_umbra && have_passive(passive_t::umbra))
            {
                mprf(MSGCH_GOD, "Your aura of darkness returns!");
                invalidate_agrid(true);
            }
            if (have_passive(passive_t::sinv))
            {
                mprf(MSGCH_GOD, "Your vision regains its divine sight.");
                autotoggle_autopickup(false);
            }
            if (have_passive(passive_t::stat_boost))
            {
                simple_god_message(" restores the support of your attributes.");
                redraw_screen();
                update_screen();
                notify_stat_change();
            }
            if (have_passive(passive_t::storm_shield))
            {
                mprf(MSGCH_GOD, "A storm instantly forms around you!");
                you.redraw_armour_class = true; // also handles shields
            }
            // When you've worked through all your penance, you get
            // another chance to make hostile slimes strict neutral.

            if (have_passive(passive_t::neutral_slimes))
                add_daction(DACT_SLIME_NEW_ATTEMPT);

            if (have_passive(passive_t::friendly_plants)
                && env.forest_awoken_until)
            {
                // XXX: add a dact here & on-join to handle offlevel
                // awakened forests?
                for (monster_iterator mi; mi; ++mi)
                     mi->del_ench(ENCH_AWAKEN_FOREST);
            }

            if (god == GOD_BEOGH)
                beogh_end_ostracism();
        }
        else if (god == GOD_HEPLIAKLQANA)
        {
            calc_hp(); // frailty ends
            mprf(MSGCH_GOD, god, "Your full life essence returns.");
        }
    }
    else
    {
        you.penance[god] -= val;
        return;
    }

    handle_traitor(god);

    // We only get this far if we just mollified a god.
    // If we just mollified a god, see if we have any angry gods left.
    // If we don't, clear the stored wrath / XP counter.
    god_iterator it;
    for (; it; ++it)
    {
        if (active_penance(*it))
            break;
    }

    if (it)
        return;

    you.attribute[ATTR_GOD_WRATH_COUNT] = 0;
    you.attribute[ATTR_GOD_WRATH_XP] = 0;
}

void dec_penance(int val)
{
    dec_penance(you.religion, val);
}

// TODO: find out what this is duplicating & deduplicate it
static bool _need_water_walking()
{
    return you.ground_level() && !you.has_mutation(MUT_MERTAIL)
           && env.grid(you.pos()) == DNGN_DEEP_WATER;
}

static void _grant_temporary_waterwalk()
{
    mpr("Your water-walking will last only until you reach solid ground.");
    you.props[TEMP_WATERWALK_KEY] = true;
}

bool jiyva_is_dead()
{
    return you.royal_jelly_dead
           && !you_worship(GOD_JIYVA) && !you.penance[GOD_JIYVA];
}

bool ignis_is_dead()
{
    return you.worshipped[GOD_IGNIS]
        && !you_worship(GOD_IGNIS)
        && !you.penance[GOD_IGNIS];
}

/// Is there any penalty from your god for removing an amulet of faith?
bool faith_has_penalty()
{
    return !you.has_mutation(MUT_FAITH)
        && ignore_faith_reason().empty()
        && !you_worship(GOD_XOM)
        && !you_worship(GOD_USKAYAW)
        && !you_worship(GOD_NO_GOD);
}

/// Is an amulet of faith irrelevant to you while you worship your current god?
/// If so, what how would that god explain why?
string ignore_faith_reason()
{
    switch (you.religion)
    {
    case GOD_GOZAG:
        return " cares for nothing but gold!";
    case GOD_ASHENZARI:
        return " cares nothing for such trivial demonstrations of your faith.";
    case GOD_IGNIS:
        // XXX: would it be better to offer a discount..?
        return " already offers you all the fire that remains!";
    case GOD_RU:
        if (you.piety >= piety_breakpoint(5))
        {
            return " says: An ascetic of your devotion"
                   " has no use for such trinkets.";
        }
        break;
    default:
        break;
    }
    return "";
}

void set_penance_xp_timeout()
{
    if (you.attribute[ATTR_GOD_WRATH_XP] > 0)
        return;

    // TODO: make this more random?
    you.attribute[ATTR_GOD_WRATH_XP] +=
        max(div_rand_round(exp_needed(you.experience_level + 1)
                          - exp_needed(you.experience_level),
                          200),
            1);
}

static void _inc_penance(god_type god, int val)
{
    if (val <= 0)
        return;

    if (!player_under_penance(god))
    {
        god_acting gdact(god, true);

        take_note(Note(NOTE_PENANCE, god));

        const bool had_halo = have_passive(passive_t::halo);
        const bool had_umbra = have_passive(passive_t::umbra);

        you.penance[god] += val;
        you.penance[god] = min((uint8_t)MAX_PENANCE, you.penance[god]);

        if (had_halo && !have_passive(passive_t::halo))
        {
            mprf(MSGCH_GOD, god, "Your divine halo fades away.");
            invalidate_agrid();
        }
        if (had_umbra && !have_passive(passive_t::umbra))
        {
            mprf(MSGCH_GOD, god, "Your aura of darkness fades away.");
            invalidate_agrid();
        }

        if (will_have_passive(passive_t::water_walk)
            && _need_water_walking() && !have_passive(passive_t::water_walk))
        {
            _grant_temporary_waterwalk();
        }

        if (will_have_passive(passive_t::stat_boost))
        {
            redraw_screen();
            update_screen();
            notify_stat_change();
        }

        if (god == GOD_TROG)
        {
            if (you.duration[DUR_TROGS_HAND])
                trog_remove_trogs_hand();
            dismiss_divine_allies_fineff::schedule(GOD_TROG);
        }
        else if (god == GOD_ZIN)
        {
            if (you.duration[DUR_DIVINE_STAMINA])
                zin_remove_divine_stamina();
            if (env.sanctuary_time)
                remove_sanctuary();
        }
        else if (god == GOD_SHINING_ONE)
        {
            if (you.duration[DUR_DIVINE_SHIELD])
               you.duration[DUR_DIVINE_SHIELD] = 0;
            dismiss_divine_allies_fineff::schedule(GOD_SHINING_ONE);
        }
        else if (god == GOD_ELYVILON)
        {
            if (you.duration[DUR_DIVINE_VIGOUR])
                elyvilon_remove_divine_vigour();
        }
        else if (god == GOD_JIYVA)
        {
            if (you.duration[DUR_SLIMIFY])
                you.duration[DUR_SLIMIFY] = 0;
            if (you.duration[DUR_OOZEMANCY])
                jiyva_end_oozemancy();
            if (slime_wall_neighbour(you.pos()))
            {
                // lose slime wall immunity
                you.wield_change = true;
                you.redraw_armour_class = true;
            }
        }
        else if (god == GOD_QAZLAL)
        {
            // Can't use have_passive(passive_t::storm_shield) because we
            // just gained penance.
            if (you.piety >= piety_breakpoint(0))
            {
                mprf(MSGCH_GOD, god, "The storm surrounding you dissipates.");
                you.redraw_armour_class = true;
            }
            if (you.duration[DUR_QAZLAL_FIRE_RES])
            {
                mprf(MSGCH_DURATION, "Your resistance to fire fades away.");
                you.duration[DUR_QAZLAL_FIRE_RES] = 0;
            }
            if (you.duration[DUR_QAZLAL_COLD_RES])
            {
                mprf(MSGCH_DURATION, "Your resistance to cold fades away.");
                you.duration[DUR_QAZLAL_COLD_RES] = 0;
            }
            if (you.duration[DUR_QAZLAL_ELEC_RES])
            {
                mprf(MSGCH_DURATION,
                     "Your resistance to electricity fades away.");
                you.duration[DUR_QAZLAL_ELEC_RES] = 0;
            }
            if (you.duration[DUR_QAZLAL_AC])
            {
                mprf(MSGCH_DURATION,
                     "Your resistance to physical damage fades away.");
                you.duration[DUR_QAZLAL_AC] = 0;
                you.redraw_armour_class = true;
            }
        }
        else if (god == GOD_SIF_MUNA)
        {
            if (you.duration[DUR_CHANNEL_ENERGY])
                you.duration[DUR_CHANNEL_ENERGY] = 0;
#if TAG_MAJOR_VERSION == 34
            if (you.attribute[ATTR_DIVINE_ENERGY])
                you.attribute[ATTR_DIVINE_ENERGY] = 0;
#endif
        }
        else if (god == GOD_OKAWARU)
        {
            if (you.duration[DUR_HEROISM])
                okawaru_remove_heroism();
            if (you.duration[DUR_FINESSE])
                okawaru_remove_finesse();
        }
        else if (god == GOD_BEOGH)
            dismiss_divine_allies_fineff::schedule(GOD_BEOGH);
        else if (god == GOD_YREDELEMNUL)
        {
            you.props.erase(YRED_TORCH_POWER_KEY);
            you.duration[DUR_FATHOMLESS_SHACKLES] = 0;
            yred_end_blasphemy();
        }

        if (you_worship(god))
        {
            // Redraw piety display and, in case the best skill is Invocations,
            // redraw the god title.
            you.redraw_title = true;
        }
    }
    else
    {
        you.penance[god] += val;
        you.penance[god] = min((uint8_t)MAX_PENANCE, you.penance[god]);
    }

    set_penance_xp_timeout();
}

static void _inc_penance(int val)
{
    _inc_penance(you.religion, val);
}

static void _set_penance(god_type god, int val)
{
    you.penance[god] = val;
}

static void _set_wrath_penance(god_type god)
{
    _set_penance(god, initial_wrath_penance_for(god));
}

static void _inc_gift_timeout(int val)
{
    if (200 - you.gift_timeout < val)
        you.gift_timeout = 200;
    else
        you.gift_timeout += val;
}

// These are sorted in order of power.
// monsters here come from genera: n, z, V and W
// - Vampire mages are excluded because they worship scholarly Kiku
// - M genus is all Kiku's domain
// - Curse *, putrid mouths, and bloated husks left out as they might
//   do too much collateral damage
static const vector<random_pick_entry<monster_type>> _yred_servants =
{
  { -2,  5,   80, PEAK, MONS_NECROPHAGE },
  { -1,  7,   75, PEAK, MONS_PHANTOM },
  {  2,  9,   70, SEMI, MONS_MARROWCUDA },
  {  4,  11,  145, SEMI, MONS_WIGHT },
  {  6,  13,  90, SEMI, MONS_SHADOWGHAST },
  {  8,  15,  110, SEMI, MONS_WRAITH },
  {  9,  15,  90, SEMI, MONS_VAMPIRE },
  { 10,  16,  110, SEMI, MONS_FREEZING_WRAITH },
  { 13,  20,  150, SEMI, MONS_SKELETAL_WARRIOR },
  { 13,  20,  75, SEMI, MONS_SHADOW_WRAITH },
  { 14,  20,  125, SEMI, MONS_PHANTASMAL_WARRIOR },
  { 13,  22,  60, FLAT, MONS_FLAYED_GHOST },
  { 13,  22,  125, SEMI, MONS_LAUGHING_SKULL },
  { 14,  21,  120, SEMI, MONS_BOG_BODY },
  { 16,  23,  180, SEMI, MONS_JIANGSHI },
  { 18,  25,  120, SEMI, MONS_EIDOLON },
  { 20,  25,  120, SEMI, MONS_VAMPIRE_KNIGHT },
  { 20,  25,  150, SEMI, MONS_GHOUL },
  { 20,  27,   80, SEMI, MONS_REVENANT },
  { 22,  27,   60, FLAT, MONS_DEATH_COB },
  { 23,  27,  180, SEMI, MONS_ANCIENT_CHAMPION },
  { 24,  30,  110, SEMI, MONS_SEARING_WRETCH },
  { 24,  30,  220, SEMI, MONS_PROFANE_SERVITOR },
  { 25,  32,  120, SEMI, MONS_BONE_DRAGON },
};

bool yred_random_servant(unsigned int pow, bool force_hostile, int num)
{
    monster_picker yred_picker;

    monster_type mon_type = yred_picker.pick(_yred_servants, pow, MONS_NECROPHAGE);

    if (mon_type == MONS_WIGHT && x_chance_in_y(pow, 8))
        num *= 2;
    else if (mon_type == MONS_SKELETAL_WARRIOR && x_chance_in_y(pow, 17))
        num *= 2;
    else if (mon_type == MONS_LAUGHING_SKULL)
        num *= random_range(2, 3);

    bool created = false;
    for (int i = 0; i < num; ++i)
    {
        mgen_data mg(mon_type, !force_hostile ? BEH_FRIENDLY : BEH_HOSTILE,
                 you.pos(), MHITYOU, MG_AUTOFOE, GOD_YREDELEMNUL);

        if (force_hostile)
        {
            mg.set_summoned(nullptr, MON_SUMM_WRATH);
            mg.non_actor_summoner = "the anger of Yredelemnul";
            mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);
            mg.set_range(2, you.current_vision);
        }
        else
            mg.set_summoned(&you, MON_SUMM_AID, summ_dur(5));

        if (create_monster(mg))
            created = true;
    }

    return created;
}

bool yred_reap_chance()
{
    // Count how many reaped allies we already have.
    int hd = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (!mi->friendly())
            continue;

        if ((mi->type == MONS_ZOMBIE || mi->type == MONS_SPECTRAL_THING)
             && mi->was_created_by(you, MON_SUMM_REAPING))
        {
            hd += mi->get_experience_level();
        }
    }

    // Always reap if we have no minions. Otherwise, use a sliding scale based
    // on total HD of allies, with a rate boosted by piety.
    if (hd == 0)
        return true;

    // Minimum chance scales from 15% at 0 piety to 40% at 6 stars
    int ratio = min(piety_breakpoint(5), (int)you.piety) * 100 / piety_breakpoint(5);
    int min_chance = 15 + (25 * ratio / 100);

    ratio = min(100, (hd * 100 / you.piety * 4));
    int chance = (ratio * min_chance / 100) + ((100 - ratio));

    return x_chance_in_y(chance, 100);
}

static bool _want_nemelex_gift()
{
    if (you.piety < piety_breakpoint(0))
        return false;
    const int piety_over_one_star = you.piety - piety_breakpoint(0);

    // Nemelex will give at least one gift early.
    if (!you.num_total_gifts[GOD_NEMELEX_XOBEH]
        && x_chance_in_y(piety_over_one_star + 1, piety_breakpoint(1)))
    {
        return true;
    }

    return one_chance_in(3) && x_chance_in_y(piety_over_one_star + 1, MAX_PIETY);
}

static bool _give_nemelex_gift(bool forced = false)
{
    if (!forced && !_want_nemelex_gift())
        return false;

    if (gift_cards())
    {
        simple_god_message(" deals you some cards!");
        mprf(MSGCH_GOD, "You now have %s.", deck_summary().c_str());
    }
    _inc_gift_timeout(5 + random2avg(9, 2));
    you.num_current_gifts[you.religion]++;
    you.num_total_gifts[you.religion]++;
    take_note(Note(NOTE_GOD_GIFT, you.religion));
    return true;
}

static bool _jiyva_mutate()
{
    simple_god_message(" alters your body.");

    bool deleted = false;
    // Go through each level of each existing non-temp, non-innate mutation.
    // Give a 1/4 chance of removing each, or a 1/2 chance for bad mutations.
    // Since we gift 4 mut levels (90% good), this means we stabilize at:
    //
    //      total mut levels = (t.m.l * (0.75 * 0.9 + 0.1 * 0.5)) + 4
    //
    // Which comes out to about 14.5 mut levels.
    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        const mutation_type mut = (mutation_type)i;
        const int lvl = you.get_base_mutation_level(mut, false, false, true);
        if (!lvl)
            continue;
        const int chance = is_bad_mutation(mut) ? 50 : 25;
        const int deletions = binomial(lvl, chance);
        for (int del = 0; del < deletions; ++del)
        {
            deleted = delete_mutation(mut, "Jiyva's grace", true, false, true)
                      || deleted;
        }
    }

    // Try to gift 4 total levels of mutations. Focus on one mutation at a time
    // until capping its level, to maximize impact.
    int to_give = 4;
    for (int attempts = 0; to_give > 0 && attempts < 500; ++attempts)
    {
        const mutation_type cat
            = random_choose_weighted(6, RANDOM_GOOD_MUTATION,
                                     4, RANDOM_SLIME_MUTATION);
        const mutation_type mut = concretize_mut(cat);
        while (to_give > 0 && mutate(mut, "Jiyva's grace", false, false, true))
               --to_give;
    }
    return to_give == 0 || deleted;
}

// Is Vehumet offering this? With "only" only return true if this is the only
// reason the player can learn the spell now.
bool vehumet_is_offering(spell_type spell, bool only)
{
    return you.vehumet_gifts.count(spell)
           && !(only && you.spell_library[spell]);
}

void vehumet_accept_gift(spell_type spell)
{
    if (vehumet_is_offering(spell))
    {
        you.vehumet_gifts.erase(spell);
        you.duration[DUR_VEHUMET_GIFT] = 0;
    }
}

static void _add_to_old_gifts(spell_type spell)
{
    you.old_vehumet_gifts.insert(spell);
}

static bool _is_old_gift(spell_type spell)
{
    return you.old_vehumet_gifts.count(spell);
}

static set<spell_type> _vehumet_eligible_gift_spells(set<spell_type> excluded_spells)
{
    set<spell_type> eligible_spells;

    const int gifts = you.num_total_gifts[you.religion];
    if (gifts >= NUM_VEHUMET_GIFTS)
        return eligible_spells;

    const int min_lev[] = {1,2,2,3,3,4,4,5,5,5,5,6,8};
    const int max_lev[] = {1,3,3,4,5,7,7,7,7,7,7,8,9};
    COMPILE_CHECK(ARRAYSZ(min_lev) == NUM_VEHUMET_GIFTS);
    COMPILE_CHECK(ARRAYSZ(max_lev) == NUM_VEHUMET_GIFTS);
    int min_level = min_lev[gifts];
    int max_level = max_lev[gifts];

    if (min_level > you.experience_level)
        return eligible_spells;

    set<spell_type> backup_spells;
    for (int i = 0; i < NUM_SPELLS; ++i)
    {
        spell_type spell = static_cast<spell_type>(i);
        if (!is_valid_spell(spell))
            continue;

        if (excluded_spells.count(spell))
            continue;

        if (vehumet_supports_spell(spell)
            && !you.has_spell(spell)
            && !you.spell_library[spell]
            && is_player_book_spell(spell)
            && spell_difficulty(spell) <= max_level
            && spell_difficulty(spell) >= min_level)
        {
            if (!_is_old_gift(spell))
                eligible_spells.insert(spell);
            else
                backup_spells.insert(spell);
        }
    }
    // Don't get stuck just because all spells have been seen/offered.
    if (eligible_spells.empty())
    {
        if (backup_spells.empty())
        {
            // This is quite improbable to happen, but in this case just
            // skip the gift and increment the gift counter.
            if (gifts <= 12)
            {
                you.num_current_gifts[you.religion]++;
                you.num_total_gifts[you.religion]++;
            }
        }
        return backup_spells;
    }
    return eligible_spells;
}

static int _vehumet_weighting(spell_type spell)
{
    int bias = 100 + elemental_preference(spell, 10);
    return bias;
}

static spell_type _vehumet_find_spell_gift(set<spell_type> excluded_spells)
{
    set<spell_type> eligible_spells = _vehumet_eligible_gift_spells(excluded_spells);
    spell_type spell = SPELL_NO_SPELL;
    int total_weight = 0;
    int this_weight = 0;
    for (auto elig : eligible_spells)
    {
        this_weight = _vehumet_weighting(elig);
        total_weight += this_weight;
        if (x_chance_in_y(this_weight, total_weight))
            spell = elig;
    }
    return spell;
}

static set<spell_type> _vehumet_get_spell_gifts()
{
    set<spell_type> offers;
    unsigned int num_offers = you.num_total_gifts[you.religion] == 12 ? 3 : 1;
    while (offers.size() < num_offers)
    {
        spell_type offer = _vehumet_find_spell_gift(offers);
        if (offer == SPELL_NO_SPELL)
            break;
        offers.insert(offer);
    }
    return offers;
}

static bool _give_trog_oka_gift(bool forced)
{
    // Break early if giving a gift now means it would be lost.
    if (feat_eliminates_items(env.grid(you.pos())))
        return false;

    // No use for anything below. (No guarantees this will work right if these
    // mutations can ever appear separately.)
    if (you.has_mutation(MUT_NO_GRASPING))
        return false;

    const bool want_weapons = you_worship(GOD_TROG)
                              && (forced || you.piety >= piety_breakpoint(4)
                                            && random2(you.piety) > 120
                                            && one_chance_in(4));
    const bool want_missiles = you_worship(GOD_OKAWARU)
                               && (forced || you.piety >= piety_breakpoint(4)
                                             && random2(you.piety) > 120
                                             && x_chance_in_y(2,5));
    object_class_type gift_type;

    if (want_weapons)
        gift_type = OBJ_WEAPONS;
    else if (want_missiles)
        gift_type = OBJ_MISSILES;
    else
        return false;

    switch (gift_type)
    {
    case OBJ_MISSILES:
        simple_god_message(" grants you throwing weapons!");
        break;
    case OBJ_WEAPONS:
        simple_god_message(" grants you a weapon!");
        break;
    default:
        simple_god_message(" grants you bugs!");
        break;
    }

    const bool success =
        acquirement_create_item(gift_type, you.religion,
                false, you.pos()) != NON_ITEM;
    if (!success)
    {
        mpr("...but nothing appears.");
        return false;
    }
    switch (gift_type)
    {
    case OBJ_MISSILES:
        _inc_gift_timeout(26 + random2avg(19, 2));
        break;
    case OBJ_WEAPONS:
        _inc_gift_timeout(30 + random2avg(19, 2));
        break;
    default:
        break;
    }
    you.num_current_gifts[you.religion]++;
    you.num_total_gifts[you.religion]++;
    take_note(Note(NOTE_GOD_GIFT, you.religion));
    return true;
}

static bool _gift_jiyva_gift(bool forced)
{
    if (forced || you.piety >= piety_breakpoint(2)
                  && random2(you.piety) > 50
                  && one_chance_in(4) && !you.gift_timeout
                  && you.can_safely_mutate())
    {
        if (_jiyva_mutate())
        {
            _inc_gift_timeout(45 + random2avg(30, 2));
            you.num_current_gifts[you.religion]++;
            you.num_total_gifts[you.religion]++;
            return true;
        }
        else
            mpr("You feel as though nothing has changed.");
    }
    return false;
}

static bool _handle_uskayaw_ability_unlocks()
{
    bool success = false;
    // Uskayaw's triggered abilities trigger if you set the timer to -1.
    // We do this so that we trigger at the end of the round instead of
    // at the time we deal damage.
    if (you.piety == piety_breakpoint(2)
        && you.props[USKAYAW_AUDIENCE_TIMER].get_int() == 0)
    {
        you.props[USKAYAW_AUDIENCE_TIMER] = -1;
        success = true;
    }
    else if (you.piety == piety_breakpoint(3)
        && you.props[USKAYAW_BOND_TIMER].get_int() == 0)
    {
        you.props[USKAYAW_BOND_TIMER] = -1;
        success = true;
    }
    return success;
}

static bool _give_sif_gift(bool forced)
{
    // Smokeless fire and books don't get along.
    if (you.has_mutation(MUT_INNATE_CASTER))
        return false;

    // Break early if giving a gift now means it would be lost.
    if (feat_eliminates_items(env.grid(you.pos())))
        return false;

    if (!forced && (you.piety < piety_breakpoint(4)
                    || random2(you.piety) < 121 || one_chance_in(4)))
    {
        return false;
    }

    // Sif Muna special: Keep quiet if acquirement fails
    // because the player already has seen all spells.
    int item_index = acquirement_create_item(OBJ_BOOKS, you.religion,
                                             true, you.pos());
    if (item_index == NON_ITEM)
        return false;

    simple_god_message(" grants you a gift!");
    // included in default force_more_message

    you.num_current_gifts[you.religion]++;
    you.num_total_gifts[you.religion]++;
    const int n_spells = spells_in_book(env.item[item_index]).size();
    _inc_gift_timeout(10 + n_spells * 6 + random2avg(19, 2));
    take_note(Note(NOTE_GOD_GIFT, you.religion));

    return true;
}

static bool _sort_spell_level(spell_type spell1, spell_type spell2)
{
    if (spell_difficulty(spell1) != spell_difficulty(spell2))
        return spell_difficulty(spell1) < spell_difficulty(spell2);

    return strcasecmp(spell_title(spell1), spell_title(spell2)) < 0;
}

static bool _give_kiku_gift(bool forced)
{
    // Djinn can't receive spell gifts.
    if (you.has_mutation(MUT_INNATE_CASTER))
        return false;

    const bool first_gift = !you.num_total_gifts[you.religion];

    // Kikubaaqudgha gives two sets of spells in a quick succession.
    if (!forced && (you.piety < piety_breakpoint(0)
                    || !first_gift && you.piety < piety_breakpoint(2)
                    || you.num_total_gifts[you.religion] > 1))
    {
        return false;
    }

    vector<spell_type> spell_options;
    vector<spell_type> chosen_spells;

    // The first set guarantees the player at least one mid-level spell that
    // benefits from wretches.
    if (first_gift)
    {
        if (coinflip())
            chosen_spells.push_back(SPELL_ANIMATE_DEAD);
        else
            chosen_spells.push_back(SPELL_FUGUE_OF_THE_FALLEN);

        spell_options = {SPELL_SOUL_SPLINTER,
                         SPELL_KISS_OF_DEATH,
                         SPELL_SUBLIMATION_OF_BLOOD,
                         SPELL_GRAVE_CLAW,
                         SPELL_VAMPIRIC_DRAINING};
    }
    else
    {
        spell_options = {SPELL_ANGUISH,
                         SPELL_MARTYRS_KNELL,
                         SPELL_DISPEL_UNDEAD,
                         SPELL_PUTREFACTION,
                         SPELL_CURSE_OF_AGONY,
                         SPELL_BORGNJORS_VILE_CLUTCH,
                         SPELL_DEATH_CHANNEL,
                         SPELL_RIMEBLIGHT};
    }

    shuffle_array(spell_options);
    for (spell_type spell : spell_options)
    {
        if (spell_is_useless(spell, false))
            continue;
        chosen_spells.push_back(spell);
        if (chosen_spells.size() >= 4)
            break;
    }

    bool new_spell = false;
    for (auto spl : chosen_spells)
        if (!you.spell_library[spl])
            new_spell = true;

    if (!new_spell)
        simple_god_message(" has no new spells for you at this time.");
    else
        simple_god_message(" grants you a gift!");
    // included in default force_more_message

    sort(chosen_spells.begin(), chosen_spells.end(), _sort_spell_level);
    library_add_spells(chosen_spells);

    you.num_current_gifts[you.religion]++;
    you.num_total_gifts[you.religion]++;
    take_note(Note(NOTE_GOD_GIFT, you.religion));

    return true;
}

static bool _handle_veh_gift(bool forced)
{
    bool success = false;
    const int gifts = you.num_total_gifts[you.religion];
    if (forced || !you.duration[DUR_VEHUMET_GIFT]
                  && !you.has_mutation(MUT_INNATE_CASTER)
                  && (you.piety >= piety_breakpoint(0) && gifts == 0
                      || you.piety >= piety_breakpoint(0) + random2(6) + 18 * gifts && gifts <= 5
                      || you.piety >= piety_breakpoint(4) && gifts <= 11 && one_chance_in(20)
                      || you.piety >= piety_breakpoint(5) && gifts <= 12 && one_chance_in(20)))
    {
        set<spell_type> offers = _vehumet_get_spell_gifts();
        if (!offers.empty())
        {
            you.vehumet_gifts = offers;
            string prompt = " offers you knowledge of ";
            for (auto it = offers.begin(); it != offers.end(); ++it)
            {
                if (it != offers.begin())
                {
                    if (offers.size() > 2)
                        prompt += ",";
                    prompt += " ";
                    auto next = it;
                    next++;
                    if (next == offers.end())
                        prompt += "and ";
                }
                prompt += spell_title(*it);
                _add_to_old_gifts(*it);
                take_note(Note(NOTE_OFFERED_SPELL, *it));
            }
            prompt += ".";
            if (gifts >= NUM_VEHUMET_GIFTS - 1)
            {
                prompt += " These spells will remain available"
                          " as long as you worship Vehumet.";
            }

            you.duration[DUR_VEHUMET_GIFT] = (100 + random2avg(100, 2)) * BASELINE_DELAY;
            if (gifts >= 5)
                _inc_gift_timeout(30 + random2avg(30, 2));
            you.num_current_gifts[you.religion]++;
            you.num_total_gifts[you.religion]++;

            simple_god_message(prompt.c_str());
            // included in default force_more_message

            success = true;
        }
    }
    return success;
}

void mons_make_god_gift(monster& mon, god_type god)
{
    const god_type acting_god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;

    if (god == GOD_NO_GOD && acting_god == GOD_NO_GOD)
        return;

    if (god == GOD_NO_GOD)
        god = acting_god;

    if (mon.flags & MF_GOD_GIFT)
    {
        dprf("Monster '%s' was already a gift of god '%s', now god '%s'.",
             mon.name(DESC_PLAIN, true).c_str(),
             god_name(mon.god).c_str(),
             god_name(god).c_str());
    }

    mon.god = god;
    mon.flags |= MF_GOD_GIFT;
}

bool mons_is_god_gift(const monster& mon, god_type god)
{
    return (mon.flags & MF_GOD_GIFT) && mon.god == god;
}

bool is_yred_undead_follower(const monster& mon)
{
    return mon.alive() && mon.holiness() & MH_UNDEAD
           && mon.attitude == ATT_FRIENDLY
           && mons_is_god_gift(mon, GOD_YREDELEMNUL);
}

bool is_apostle_follower(const monster& mon)
{
    return mon.alive() && mon.attitude == ATT_FRIENDLY
           && mon.type == MONS_ORC_APOSTLE;
}

bool is_fellow_slime(const monster& mon)
{
    return mon.alive() && mons_is_slime(mon)
           && mon.attitude == ATT_GOOD_NEUTRAL
           && mons_is_god_gift(mon, GOD_JIYVA);
}

static bool _is_plant_follower(const monster* mon)
{
    return mon->alive() && mons_is_plant(*mon)
           && mon->attitude == ATT_FRIENDLY;
}

bool is_follower(const monster& mon)
{
    if (you_worship(GOD_YREDELEMNUL))
        return is_yred_undead_follower(mon);
    else if (you_worship(GOD_BEOGH))
        return is_apostle_follower(mon);
    else if (you_worship(GOD_JIYVA))
        return is_fellow_slime(mon);
    else if (you_worship(GOD_FEDHAS))
        return _is_plant_follower(&mon);
    else
    {
        return mon.alive() && mon.attitude == ATT_FRIENDLY
                           && !mon.is_summoned();
    }
}

/**
 * What's the name of the ally Hepliaklqana granted the player?
 *
 * @return      The ally's name.
 */
string hepliaklqana_ally_name()
{
    return you.props[HEPLIAKLQANA_ALLY_NAME_KEY].get_string();
}

/**
 * How much HD should the ally granted by Hepliaklqana have?
 *
 * @return      The player's xl * 2/3.
 */
static int _hepliaklqana_ally_hd()
{
    if (!crawl_state.need_save) // on main menu or otherwise don't have 'you'
        return 27; // v0v
    // round up
    return (you.experience_level - 1) * 2 / 3 + 1;
}

/**
 * How much max HP should the ally granted by Hepliaklqana have?
 *
 * @return      5/hd from 1-11 HD, 10/hd from 12-18.
 *              (That is, 5 HP at 1 HD, 120 at 18.)
 */
int hepliaklqana_ally_hp()
{
    const int HD = _hepliaklqana_ally_hd();
    return HD * 5 + max(0, (HD - 12) * 5);
}

/**
 * Choose an antique name for a Hepliaklqana-granted ancestor.
 *
 * @param gender    The ancestor's gender.
 * @return          An appropriate name; e.g. Hrodulf, Citali, Aat.
 */
static string _make_ancestor_name(gender_type gender)
{
    const string gender_name = gender == GENDER_MALE ? " male " :
                               gender == GENDER_FEMALE ? " female " : " ";
    const string suffix = gender_name + "name";
    const string name = getRandNameString("ancestor", suffix);
    return name.empty() ? make_name() : name;
}

/// Setup when gaining a Hepliaklqana ancestor.
static void _setup_hepliaklqana_ancestor()
{
    // initial setup.
    if (!you.props.exists(HEPLIAKLQANA_ALLY_NAME_KEY))
    {
        const gender_type gender = random_choose(GENDER_NEUTRAL,
                                                 GENDER_MALE,
                                                 GENDER_FEMALE);

        you.props[HEPLIAKLQANA_ALLY_NAME_KEY] = _make_ancestor_name(gender);
        you.props[HEPLIAKLQANA_ALLY_GENDER_KEY] = gender;
    }
}

/**
 * Creates a mgen_data with the information needed to create the ancestor
 * granted by Hepliaklqana.
 *
 * XXX: should this be populating a mgen_data passed by reference, rather than
 * returning one on the stack?
 *
 * @return    The mgen_data that creates a hepliaklqana ancestor.
 */
mgen_data hepliaklqana_ancestor_gen_data()
{
    _setup_hepliaklqana_ancestor();
    const monster_type type = you.props.exists(HEPLIAKLQANA_ALLY_TYPE_KEY) ?
        (monster_type)you.props[HEPLIAKLQANA_ALLY_TYPE_KEY].get_int() :
        MONS_ANCESTOR;
    mgen_data mg(type, BEH_FRIENDLY, you.pos(), MHITYOU, MG_AUTOFOE,
                 GOD_HEPLIAKLQANA);
    mg.hd = _hepliaklqana_ally_hd();
    mg.hp = hepliaklqana_ally_hp();
    mg.extra_flags |= MF_NO_REWARD;
    mg.mname = hepliaklqana_ally_name();
    mg.props[MON_GENDER_KEY]
        = you.props[HEPLIAKLQANA_ALLY_GENDER_KEY].get_int();
    return mg;
}

/// Print a message for an ancestor's *something* being gained.
static void _regain_memory(const monster &ancestor, string memory)
{
    mprf("%s regains the memory of %s %s.",
         ancestor.name(DESC_YOUR, true).c_str(),
         ancestor.pronoun(PRONOUN_POSSESSIVE, true).c_str(),
         memory.c_str());
}

static string _item_ego_name(object_class_type base_type, int brand)
{
    switch (base_type)
    {
    case OBJ_WEAPONS:
    {
        // 'remembers... draining' reads better than 'drain', but 'flame'
        // reads better than 'flaming'
        const bool terse = brand == SPWPN_FLAMING
                           || brand == SPWPN_ANTIMAGIC;
        return brand_type_name((brand_type) brand, terse);
    }
    case OBJ_ARMOUR:
        // XXX: hack
        return "reflection";
    default:
        die("unsupported object type");
    }
}

/// Print a message for an ancestor's item being gained/type upgraded.
static void _regain_item_memory(const monster &ancestor,
                                object_class_type base_type,
                                int sub_type,
                                int brand)
{
    const string base_name = item_base_name(base_type, sub_type);
    if (!brand)
    {
        _regain_memory(ancestor, base_name);
        return;
    }

    const string ego_name = _item_ego_name(base_type, brand);
    const string item_name
        = make_stringf("%s of %s",
                       item_base_name(base_type, sub_type).c_str(),
                       ego_name.c_str());
    _regain_memory(ancestor, item_name);
}

/**
 * Update the ancestor's stats after the player levels up. Upgrade HD and HP,
 * and give appropriate messaging for that and any other notable upgrades
 * (spells, resists, etc).
 *
 * @param quiet_force     Whether to squash messages & force upgrades,
 *                        even if the HD is unchanged.
 */
void upgrade_hepliaklqana_ancestor(bool quiet_force)
{
    monster* ancestor = hepliaklqana_ancestor_mon();
    if (!ancestor || !ancestor->alive())
        return;

    // housekeeping
    ancestor->mname = hepliaklqana_ally_name();
    ancestor->props[MON_GENDER_KEY]
        = you.props[HEPLIAKLQANA_ALLY_GENDER_KEY].get_int();

    const int old_hd = ancestor->get_experience_level();
    const int hd = _hepliaklqana_ally_hd();
    ancestor->set_hit_dice(hd);
    if (old_hd == hd && !quiet_force)
        return; // assume nothing changes except at different HD

    const int old_mhp = ancestor->max_hit_points;
    ancestor->max_hit_points = hepliaklqana_ally_hp();
    ancestor->props[KNOWN_MAX_HP_KEY] = ancestor->max_hit_points;
    ancestor->hit_points =
        div_rand_round(ancestor->hit_points * ancestor->max_hit_points,
                       old_mhp);

    if (!quiet_force)
    {
        mprf("%s remembers more of %s old skill.",
             ancestor->name(DESC_YOUR, true).c_str(),
             ancestor->pronoun(PRONOUN_POSSESSIVE, true).c_str());
    }

    set_ancestor_spells(*ancestor, !quiet_force);

    const bool ancestor_offlevel = companion_is_elsewhere(ancestor->mid);
    if (ancestor_offlevel)
        add_daction(DACT_UPGRADE_ANCESTOR);

    // assumption: ancestors can lose weapons (very rarely - tukima's),
    // and it's weird for them to just reappear, so only upgrade existing ones
    if (ancestor->weapon())
    {
        if (!ancestor_offlevel)
            upgrade_hepliaklqana_weapon(ancestor->type, *ancestor->weapon());

        const weapon_type wpn = _hepliaklqana_weapon_type(ancestor->type, hd);
        const brand_type brand = _hepliaklqana_weapon_brand(ancestor->type, hd);
        if (wpn != _hepliaklqana_weapon_type(ancestor->type, old_hd)
            && !quiet_force)
        {
            _regain_item_memory(*ancestor, OBJ_WEAPONS, wpn, brand);
        }
        else if (brand != _hepliaklqana_weapon_brand(ancestor->type, old_hd)
                 && !quiet_force)
        {
            mprf("%s remembers %s %s %s.",
                 ancestor->name(DESC_YOUR, true).c_str(),
                 ancestor->pronoun(PRONOUN_POSSESSIVE, true).c_str(),
                 apostrophise(item_base_name(OBJ_WEAPONS, wpn)).c_str(),
                 brand_type_name(brand, brand != SPWPN_DRAINING));
        }
    }
    // but shields can't be lost, and *can* be gained (knight at hd 5)
    // so give them out as appropriate
    if (!ancestor_offlevel)
    {
        if (ancestor->shield())
            upgrade_hepliaklqana_shield(*ancestor, *ancestor->shield());
        else
            give_shield(ancestor);
    }

    const armour_type shld = _hepliaklqana_shield_type(ancestor->type, hd);
    if (shld != _hepliaklqana_shield_type(ancestor->type, old_hd)
        && !quiet_force)
    {
        // doesn't currently support egos varying separately from shield types
        _regain_item_memory(*ancestor, OBJ_ARMOUR, shld,
                            _hepliaklqana_shield_ego(hd));
    }

    if (quiet_force)
        return;
}

/**
 * What type of weapon should an ancestor of the given HD have?
 *
 * @param mc   The type of ancestor in question.
 * @param HD   The HD of the ancestor in question.
 * @return     An appropriate weapon_type.
 */
static weapon_type _hepliaklqana_weapon_type(monster_type mc, int HD)
{
    switch (mc)
    {
    case MONS_ANCESTOR_HEXER:
        return HD < 16 ? WPN_DAGGER : WPN_QUICK_BLADE;
    case MONS_ANCESTOR_KNIGHT:
        return HD < 10 ? WPN_FLAIL : WPN_BROAD_AXE;
    case MONS_ANCESTOR_BATTLEMAGE:
        return HD < 13 ? WPN_QUARTERSTAFF : WPN_LAJATANG;
    default:
        return NUM_WEAPONS; // should never happen
    }
}

/**
 * What brand should an ancestor of the given HD's weapon have, if any?
 *
 * @param mc   The type of ancestor in question.
 * @param HD   The HD of the ancestor in question.
 * @return     An appropriate weapon_type.
 */
static brand_type _hepliaklqana_weapon_brand(monster_type mc, int HD)
{
    switch (mc)
    {
        case MONS_ANCESTOR_HEXER:
            return HD < 16 ?   SPWPN_DRAINING :
                               SPWPN_ANTIMAGIC;
        case MONS_ANCESTOR_KNIGHT:
            return HD < 10 ?   SPWPN_NORMAL :
                   HD < 16 ?   SPWPN_FLAMING :
                               SPWPN_SPEED;
        case MONS_ANCESTOR_BATTLEMAGE:
            return HD < 13 ?   SPWPN_NORMAL :
                               SPWPN_FREEZING;
        default:
            return SPWPN_NORMAL;
    }
}

/**
 * Setup an ancestor's weapon after their class is chosen, when the player
 * levels up, or after they're resummoned (or initially created for wrath).
 *
 * @param[in]   mtyp          The ancestor for whom the weapon is intended.
 * @param[out]  item          The item to be configured.
 * @param       notify        Whether messages should be printed when something
 *                            changes. (Weapon type or brand.)
 */
void upgrade_hepliaklqana_weapon(monster_type mtyp, item_def &item)
{
    ASSERT(mons_is_hepliaklqana_ancestor(mtyp));
    if (mtyp == MONS_ANCESTOR)
        return; // bare-handed!

    item.base_type = OBJ_WEAPONS;
    item.sub_type = _hepliaklqana_weapon_type(mtyp,
                                              _hepliaklqana_ally_hd());
    item.brand = _hepliaklqana_weapon_brand(mtyp,
                                            _hepliaklqana_ally_hd());
    item.plus = 0;
    item.flags |= ISFLAG_IDENTIFIED | ISFLAG_SUMMONED;
}

/**
 * What kind of shield should an ancestor of the given HD be given?
 *
 * @param mc        The type of ancestor in question.
 * @param HD        The HD (XL) of the ancestor in question.
 * @return          An appropriate type of shield, or NUM_ARMOURS.
 */
static armour_type _hepliaklqana_shield_type(monster_type mc, int HD)
{
    if (mc != MONS_ANCESTOR_KNIGHT)
        return NUM_ARMOURS;
    if (HD < 13)
        return ARM_KITE_SHIELD;
    return ARM_TOWER_SHIELD;
}

static special_armour_type _hepliaklqana_shield_ego(int HD)
{
    return HD < 13 ? SPARM_NORMAL : SPARM_REFLECTION;
}

/**
 * Setup an ancestor's weapon after their class is chosen, when the player
 * levels up, or after they're resummoned (or initially created for wrath).
 *
 * @param[in]   ancestor      The ancestor for whom the weapon is intended.
 * @param[out]  item          The item to be configured.
 * @return                    True iff the ancestor should have a weapon.
 */
void upgrade_hepliaklqana_shield(const monster &ancestor, item_def &item)
{
    ASSERT(mons_is_hepliaklqana_ancestor(ancestor.type));
    const int HD = ancestor.get_experience_level();
    const armour_type shield_type = _hepliaklqana_shield_type(ancestor.type,
                                                              HD);
    if (shield_type == NUM_ARMOURS)
        return; // no shield yet!

    item.base_type = OBJ_ARMOUR;
    item.sub_type = shield_type;
    item.brand = _hepliaklqana_shield_ego(HD);
    item.plus = 0;
    item.flags |= ISFLAG_IDENTIFIED | ISFLAG_SUMMONED;
    item.quantity = 1;
}

///////////////////////////////
bool do_god_gift(bool forced)
{
    ASSERT(!you_worship(GOD_NO_GOD));

    god_acting gdact;

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_GIFTS)
    int old_num_current_gifts = you.num_current_gifts[you.religion];
    int old_num_total_gifts = you.num_total_gifts[you.religion];
#endif

    bool success = false;

    // Consider a gift if we don't have a timeout and aren't under penance
    if (forced || !player_under_penance() && !you.gift_timeout)
    {
        // Remember to check for water/lava.
        switch (you.religion)
        {
        default:
            break;

        case GOD_NEMELEX_XOBEH:
            success = _give_nemelex_gift(forced);
            break;

        case GOD_OKAWARU:
        case GOD_TROG:
            success = _give_trog_oka_gift(forced);
            break;

        case GOD_JIYVA:
            success = _gift_jiyva_gift(forced);
            break;

        case GOD_USKAYAW:
            success = _handle_uskayaw_ability_unlocks();
            break;

        case GOD_KIKUBAAQUDGHA:
            success = _give_kiku_gift(forced);
            break;

        case GOD_SIF_MUNA:
            success = _give_sif_gift(forced);
            break;

        case GOD_VEHUMET:
            success = _handle_veh_gift(forced);
            break;
        }                       // switch (you.religion)
    }                           // End of gift giving.

    if (success)
        stop_running(false);

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_GIFTS)
    if (old_num_current_gifts < you.num_current_gifts[you.religion])
    {
        mprf(MSGCH_DIAGNOSTICS, "Current number of gifts from this god: %d",
             you.num_current_gifts[you.religion]);
    }
    if (old_num_total_gifts < you.num_total_gifts[you.religion])
    {
        mprf(MSGCH_DIAGNOSTICS, "Total number of gifts from this god: %d",
             you.num_total_gifts[you.religion]);
    }
#endif
    return success;
}

string god_name(god_type which_god, bool long_name)
{
    if (which_god == GOD_JIYVA)
    {
        return god_name_jiyva(long_name) +
               (long_name? " the Shapeless" : "");
    }

    if (long_name)
    {
        const string shortname = god_name(which_god, false);
        const string longname = getMiscString(shortname + " lastname");
        return longname.empty()? shortname : longname;
    }

    switch (which_god)
    {
    case GOD_NO_GOD:        return "No God";
    case GOD_RANDOM:        return "random";
    case GOD_NAMELESS:      return "nameless";
    case GOD_ZIN:           return "Zin";
    case GOD_SHINING_ONE:   return "the Shining One";
    case GOD_KIKUBAAQUDGHA: return "Kikubaaqudgha";
    case GOD_YREDELEMNUL:   return "Yredelemnul";
    case GOD_VEHUMET:       return "Vehumet";
    case GOD_OKAWARU:       return "Okawaru";
    case GOD_MAKHLEB:       return "Makhleb";
    case GOD_SIF_MUNA:      return "Sif Muna";
    case GOD_TROG:          return "Trog";
    case GOD_NEMELEX_XOBEH: return "Nemelex Xobeh";
    case GOD_ELYVILON:      return "Elyvilon";
    case GOD_LUGONU:        return "Lugonu";
    case GOD_BEOGH:         return "Beogh";
    case GOD_FEDHAS:        return "Fedhas";
    case GOD_CHEIBRIADOS:   return "Cheibriados";
    case GOD_XOM:           return "Xom";
    case GOD_ASHENZARI:     return "Ashenzari";
    case GOD_DITHMENOS:     return "Dithmenos";
    case GOD_GOZAG:         return "Gozag";
    case GOD_QAZLAL:        return "Qazlal";
    case GOD_RU:            return "Ru";
#if TAG_MAJOR_VERSION == 34
    case GOD_PAKELLAS:      return "Pakellas";
#endif
    case GOD_USKAYAW:       return "Uskayaw";
    case GOD_HEPLIAKLQANA:  return "Hepliaklqana";
    case GOD_WU_JIAN:       return "Wu Jian";
    case GOD_IGNIS:         return "Ignis";
    case GOD_JIYVA: // This is handled at the beginning of the function
    case GOD_ECUMENICAL:    return "an unknown god";
    case NUM_GODS:          return "Buggy";
    }
    return "";
}

string god_name_jiyva(bool second_name)
{
    string name = "Jiyva";
    if (second_name)
        name += " " + you.jiyva_second_name;

    return name;
}

string wu_jian_random_sifu_name()
{
    switch (random2(7))
    {
        case 0: return "Yunchang";
        case 1: return "Lu Zhishen";
        case 2: return "Xiang Ba";
        case 3: return "Ma Yunglu";
        case 4: return "Hu Sanniang";
        case 5: return "Gene Jian Bin";
        case 6: return "Cai Fang";
        default: return "Bug";
    }
}

god_type str_to_god(const string &_name, bool exact)
{
    string target(_name);
    trim_string(target);
    lowercase(target);

    if (target.empty())
        return GOD_NO_GOD;

    int      num_partials = 0;
    god_type partial      = GOD_NO_GOD;
    for (god_iterator it; it; ++it)
    {
        god_type god = *it;
        string name = lowercase_string(god_name(god, false));

        if (name == target)
            return god;

        if (!exact && name.find(target) != string::npos)
        {
            // Return nothing for ambiguous partial names.
            num_partials++;
            if (num_partials > 1)
                return GOD_NO_GOD;
            partial = god;
        }
    }

    if (!exact && num_partials == 1)
        return partial;

    return GOD_NO_GOD;
}

void god_speaks(god_type god, const char *mesg)
{
    ASSERT(!crawl_state.game_is_arena());

    int orig_mon = env.mgrid(you.pos());

    monster fake_mon;
    fake_mon.type       = MONS_PROGRAM_BUG;
    fake_mon.mid        = MID_NOBODY;
    fake_mon.hit_points = 1;
    fake_mon.god        = god;
    fake_mon.set_position(you.pos());
    fake_mon.foe        = MHITYOU;
    fake_mon.mname      = "FAKE GOD MONSTER";

    mprf(MSGCH_GOD, god, "%s", do_mon_str_replacements(mesg, fake_mon).c_str());

    fake_mon.reset();
    env.mgrid(you.pos()) = orig_mon;
}

void religion_turn_start()
{
    if (you.turn_is_over)
        religion_turn_end();

    crawl_state.clear_god_acting();
}

void religion_turn_end()
{
    ASSERT(you.turn_is_over);
    _place_delayed_monsters();
}

/** Punish the character for some divine transgression.
 *
 * @param piety_loss The amount of penance imposed; may be scaled.
 * @param penance The amount of penance imposed; may be scaled.
 */
void dock_piety(int piety_loss, int penance, bool no_lecture)
{
    static int last_piety_lecture   = -1;
    static int last_penance_lecture = -1;

    if (piety_loss <= 0 && penance <= 0)
        return;

    piety_loss = piety_scale(piety_loss);
    penance    = piety_scale(penance);

    if (piety_loss)
    {
        if (last_piety_lecture != you.num_turns)
        {
            // output guilt message:
            mprf("You feel%sguilty.",
                 (piety_loss == 1) ? " a little " :
                 (piety_loss <  5) ? " " :
                 (piety_loss < 10) ? " very "
                                   : " extremely ");
        }

        last_piety_lecture = you.num_turns;
        lose_piety(piety_loss);
    }

    if (you.piety < 1)
        excommunication();
    else if (penance)       // only if still in religion
    {
        if (last_penance_lecture != you.num_turns && !no_lecture)
        {
            god_speaks(you.religion,
                       you.religion == GOD_JIYVA ? "Furious gurgling surrounds you!"
                       : "\"You will pay for your transgression, mortal!\"");
            last_penance_lecture = you.num_turns;
        }

        _inc_penance(penance);
    }
}

// Scales a piety number, applying modifiers (faith).
int piety_scale(int piety)
{
    return piety + (you.faith() * div_rand_round(piety, 4));
}

/** Gain or lose piety to reach a certain value.
 *
 * If the player cannot gain piety (because they worship Xom, Gozag, or
 * no god), their piety will be unchanged.
 *
 * @param piety The new piety value.
 * @pre piety is between 0 and MAX_PIETY, inclusive.
 */
void set_piety(int piety)
{
    ASSERT(piety >= 0);
    ASSERT(piety <= MAX_PIETY);

    // Ru max piety is 6*
    if (you_worship(GOD_RU) && piety > piety_breakpoint(5))
        piety = piety_breakpoint(5);

    // We have to set the exact piety value this way, because diff may
    // be decreased to account for things like penance and gift timeout.
    int diff;
    do
    {
        diff = piety - you.piety;
        if (diff > 0)
        {
            if (!gain_piety(diff, 1, false))
                break;
        }
        else if (diff < 0)
            lose_piety(-diff);
    }
    while (diff != 0);
}

static void _gain_piety_point()
{
    // check to see if we owe anything first
    if (player_under_penance(you.religion))
    {
        dec_penance(1);
        return;
    }
    else if (you.gift_timeout > 0)
    {
        you.gift_timeout--;

        // Slow down piety gain to account for the fact that gifts
        // no longer have a piety cost for getting them.
        if (!one_chance_in(4) && !you_worship(GOD_JIYVA)
            && !you_worship(GOD_NEMELEX_XOBEH)
            && !you_worship(GOD_ELYVILON)
            && !you_worship(GOD_BEOGH))
        {
#ifdef DEBUG_PIETY
            mprf(MSGCH_DIAGNOSTICS, "Piety slowdown due to gift timeout.");
#endif
            return;
        }
    }

    // Increment our progress to the next companion resurrection, as well as
    // next apostle challenge. (Note: Does NOT happen slower at higher piety.)
    if (you_worship(GOD_BEOGH))
    {
        beogh_progress_resurrection(1);
        you.props[BEOGH_CHALLENGE_PROGRESS_KEY].get_int() += 1;
    }

    // slow down gain at upper levels of piety
    if (!you_worship(GOD_RU))
    {
        if (you.piety >= MAX_PIETY
            || you.piety >= piety_breakpoint(5) && one_chance_in(3)
            || you.piety >= piety_breakpoint(3) && one_chance_in(3))
        {
            do_god_gift();
            return;
        }
    }
    else
    {
        // Ru piety doesn't modulate or taper and Ru doesn't give gifts.
        // Ru max piety is 160 (6*)
        if (you.piety >= piety_breakpoint(5))
            return;
    }

    int old_piety = you.piety;
    // Apply hysteresis.
    // piety_hysteresis is the amount of _loss_ stored up, so this
    // may look backwards.
    if (you.piety_hysteresis)
        you.piety_hysteresis--;
    else if (you.piety < MAX_PIETY)
        you.piety++;

    if (piety_rank() > piety_rank(old_piety))
    {
        // Redraw piety display and, in case the best skill is Invocations,
        // redraw the god title.
        you.redraw_title = true;

        const int rank = piety_rank();
        take_note(Note(NOTE_PIETY_RANK, you.religion, rank));

        // For messaging reasons, we want to get our ancestor before
        // we get the associated recall / rename powers.
        if (rank == rank_for_passive(passive_t::frail))
        {
            calc_hp(); // adjust for frailty
            // In exchange for your hp, you get an ancestor!
            const mgen_data mg = hepliaklqana_ancestor_gen_data();
            delayed_monster(mg);
            simple_god_message(make_stringf(" forms a fragment of your life essence"
                                            " into the memory of your ancestor, %s!",
                                            mg.mname.c_str()).c_str());
        }

        for (const auto& power : get_god_powers(you.religion))
        {

            if (power.rank == rank
                || power.rank == 7 && can_do_capstone_ability(you.religion))
            {
                power.display(true, "You can now %s.");
#ifdef USE_TILE_LOCAL
                tiles.layout_statcol();
                redraw_screen();
                update_screen();
#endif
                learned_something_new(HINT_NEW_ABILITY_GOD);
            }
        }
        if (rank == rank_for_passive(passive_t::halo))
            mprf(MSGCH_GOD, "A divine halo surrounds you!");
        if (rank == rank_for_passive(passive_t::umbra))
            mprf(MSGCH_GOD, "You are shrouded in an aura of darkness!");
        if (rank == rank_for_passive(passive_t::jelly_regen))
        {
            simple_god_message(" begins accelerating your health and magic "
                               "regeneration.");
        }
        if (rank == rank_for_passive(passive_t::sinv))
            autotoggle_autopickup(false);
        if (rank == rank_for_passive(passive_t::clarity))
        {
            // Inconsistent with donning amulets, but matches the
            // message better and is not abusable.
            you.duration[DUR_CONF] = 0;
        }
        if (rank >= rank_for_passive(passive_t::identify_items))
            ash_id_inventory();

        // TODO: add one-time ability check in have_passive
        if (have_passive(passive_t::unlock_slime_vaults)
            && can_do_capstone_ability(you.religion))
        {
            simple_god_message(" will now unseal the treasures of the "
                               "Slime Pits.");
            dlua.callfn("dgn_set_persistent_var", "sb",
                        "fix_slime_vaults", true);
            // If we're on Slime:$, pretend we just entered the level
            // in order to bring down the vault walls.
            if (level_id::current() == level_id(BRANCH_SLIME,
                                                brdepth[BRANCH_SLIME]))
            {
                dungeon_events.fire_event(DET_ENTERED_LEVEL);
            }

            you.one_time_ability_used.set(you.religion);
        }
        if (you_worship(GOD_HEPLIAKLQANA)
            && rank == 2 && !you.props.exists(HEPLIAKLQANA_ALLY_TYPE_KEY))
        {
           god_speaks(you.religion,
                      "You may now remember your ancestor's life.");
        }
        // Qualify for an immediate apostle challenge upon hitting 3* the first time
        if (you_worship(GOD_BEOGH)
            && rank == 2 && you.num_current_gifts[you.religion] == 0)
        {
            you.props[BEOGH_CHALLENGE_PROGRESS_KEY] = 25;
            beogh_increase_orcification();
        }
        else if (you_worship(GOD_BEOGH) && rank == 5
                 && you.props[ORCIFICATION_LEVEL_KEY].get_int() < 2)
        {
            beogh_increase_orcification();
        }

        if (you_worship(GOD_MAKHLEB) && rank == 4
            && !you.has_mutation(MUT_MAKHLEB_DESTRUCTION_GEH)
            && !you.has_mutation(MUT_MAKHLEB_DESTRUCTION_COC)
            && !you.has_mutation(MUT_MAKHLEB_DESTRUCTION_TAR)
            && !you.has_mutation(MUT_MAKHLEB_DESTRUCTION_DIS))
        {
            mutation_type mut = random_choose(MUT_MAKHLEB_DESTRUCTION_GEH,
                                              MUT_MAKHLEB_DESTRUCTION_COC,
                                              MUT_MAKHLEB_DESTRUCTION_TAR,
                                              MUT_MAKHLEB_DESTRUCTION_DIS);

            perma_mutate(mut, 1, "Makhleb's blessing");
        }
    }

    // The player's symbol depends on Beogh piety.
    if (you_worship(GOD_BEOGH))
        update_player_symbol();

    if (have_passive(passive_t::stat_boost)
        && chei_stat_boost(old_piety) < chei_stat_boost())
    {
        string msg = " raises the support of your attributes";
        if (have_passive(passive_t::slowed))
            msg += " as your movement slows";
        msg += ".";
        simple_god_message(msg.c_str());
        notify_stat_change();
    }

    if (you_worship(GOD_QAZLAL)
        && qazlal_sh_boost(old_piety) != qazlal_sh_boost())
    {
        you.redraw_armour_class = true;
    }

    if (have_passive(passive_t::halo) || have_passive(passive_t::umbra))
    {
        // Piety change affects halo / umbra radius.
        invalidate_agrid(true);
    }

    do_god_gift();
}

/**
 * Gain an amount of piety.
 *
 * @param original_gain The numerator of the nominal piety gain.
 * @param denominator The denominator of the nominal piety gain.
 * @param should_scale_piety Should the piety gain be scaled by faith/Sprint?
 * @return True if something happened, or if another call with the same
 *   arguments might cause something to happen (because of random number
 *   rolls).
 */
bool gain_piety(int original_gain, int denominator, bool should_scale_piety)
{
    if (original_gain <= 0)
        return false;

    // Xom uses piety differently; Gozag doesn't at all.
    if (you_worship(GOD_NO_GOD)
        || you_worship(GOD_XOM)
        || you_worship(GOD_GOZAG))
    {
        return false;
    }

    int pgn = should_scale_piety ? piety_scale(original_gain) : original_gain;

    if (crawl_state.game_is_sprint() && should_scale_piety)
        pgn = sprint_modify_piety(pgn);

    pgn = div_rand_round(pgn, denominator);
    while (pgn-- > 0)
        _gain_piety_point();
    // Note down the first time you hit 6* piety with a given god,
    // excepting Ignis, since it's not really meaningful there.
    if (you.piety > you.piety_max[you.religion] && !you_worship(GOD_IGNIS))
    {
        if (you.piety >= piety_breakpoint(5)
            && you.piety_max[you.religion] < piety_breakpoint(5))
        {
            mark_milestone("god.maxpiety", "became the Champion of "
                           + god_name(you.religion) + ".");
        }
        you.piety_max[you.religion] = you.piety;
    }
    return true;
}

/** Reduce piety and handle side-effects.
 *
 * Appropriate for cases where the player has not sinned, but must lose piety
 * anyway, such as costs for abilities.
 *
 * @param pgn The precise amount of piety lost.
 */
void lose_piety(int pgn)
{
    if (pgn <= 0)
        return;

    const int old_piety = you.piety;

    // Apply hysteresis.
    const int old_hysteresis = you.piety_hysteresis;
    you.piety_hysteresis = min<int>(PIETY_HYSTERESIS_LIMIT,
                                    you.piety_hysteresis + pgn);
    const int pgn_borrowed = (you.piety_hysteresis - old_hysteresis);
    pgn -= pgn_borrowed;
#ifdef DEBUG_PIETY
    mprf(MSGCH_DIAGNOSTICS,
         "Piety decreasing by %d (and %d added to hysteresis)",
         pgn, pgn_borrowed);
#endif

    if (you.piety - pgn < 0)
        you.piety = 0;
    else
        you.piety -= pgn;

    // Don't bother printing out these messages if you're under
    // penance, you wouldn't notice since all these abilities
    // are withheld.
    if (!player_under_penance()
        && piety_rank(old_piety) > piety_rank())
    {
        // Redraw piety display and, in case the best skill is Invocations,
        // redraw the god title.
        you.redraw_title = true;

        const int old_rank = piety_rank(old_piety);

        for (const auto& power : get_god_powers(you.religion))
        {
            if (power.rank == old_rank
                || power.rank == 7 && old_rank == 6
                   && !you.one_time_ability_used[you.religion])
            {
                power.display(false, "You can no longer %s.");
#if TAG_MAJOR_VERSION == 34
                // Deactivate the toggle
                if (power.abil == ABIL_SIF_MUNA_DIVINE_ENERGY)
                    you.attribute[ATTR_DIVINE_ENERGY] = 0;
#endif
            }
        }
#ifdef USE_TILE_LOCAL
        tiles.layout_statcol();
        redraw_screen();
        update_screen();
#endif

        if (will_have_passive(passive_t::frail) && !have_passive(passive_t::frail))
        {
            // hep: just lost 1*
            // remove companion (gained at same tier as frail)
            add_daction(DACT_ALLY_HEPLIAKLQANA);
            remove_all_companions(GOD_HEPLIAKLQANA);
            calc_hp(); // adjust for frailty
        }
    }

    if (you.piety > 0 && you.piety <= 5)
        learned_something_new(HINT_GOD_DISPLEASED);

    if (will_have_passive(passive_t::water_walk) && _need_water_walking()
        && !have_passive(passive_t::water_walk))
    {
        _grant_temporary_waterwalk();
    }
    if (will_have_passive(passive_t::stat_boost)
        && chei_stat_boost(old_piety) > chei_stat_boost())
    {
        string msg = " lowers the support of your attributes";
        if (will_have_passive(passive_t::slowed))
            msg += " as your movement quickens";
        msg += ".";
        simple_god_message(msg.c_str());
        notify_stat_change();
    }

    if (you_worship(GOD_QAZLAL)
        && qazlal_sh_boost(old_piety) != qazlal_sh_boost())
    {
        you.redraw_armour_class = true;
    }

    if (will_have_passive(passive_t::halo)
        || will_have_passive(passive_t::umbra))
    {
        // Piety change affects halo / umbra radius.
        invalidate_agrid(true);
    }

    you.props[MIN_IGNIS_PIETY_KEY] = you.piety;
}

/// Whether Fedhas would set `target` to a neutral attitude
bool fedhas_neutralises(const monster& target)
{
    return mons_is_plant(target)
           && target.holiness() & MH_PLANT
           && target.type != MONS_SNAPLASHER_VINE
           && target.type != MONS_SNAPLASHER_VINE_SEGMENT;
}

static string _god_hates_your_god_reaction(god_type god, god_type your_god)
{
    if (god_hates_your_god(god, your_god))
    {
        // Non-good gods always hate your current god.
        if (!is_good_god(god))
            return "";

        // Zin hates chaotic gods.
        if (god == GOD_ZIN && is_chaotic_god(your_god))
            return " for chaos";

        if (is_evil_god(your_god))
            return " for evil";
    }

    return "";
}

/**
 * When you abandon this god, how much penance do you gain? (How long does the
 * wrath last?
 *
 * @param god   The god in question.
 * @return      The initial penance for the given god's wrath.
 */
int initial_wrath_penance_for(god_type god)
{
    // TODO: transform to data (tables)
    switch (god)
    {
        case GOD_ASHENZARI:
        case GOD_BEOGH:
        case GOD_ELYVILON:
        case GOD_GOZAG:
        case GOD_HEPLIAKLQANA:
        case GOD_LUGONU:
        case GOD_NEMELEX_XOBEH:
        case GOD_TROG:
        case GOD_XOM:
            return 50;
        case GOD_FEDHAS:
        case GOD_KIKUBAAQUDGHA:
        case GOD_JIYVA:
        case GOD_SHINING_ONE:
        case GOD_SIF_MUNA:
        case GOD_YREDELEMNUL:
            return 30;
        case GOD_CHEIBRIADOS:
        case GOD_DITHMENOS:
        case GOD_MAKHLEB:
#if TAG_MAJOR_VERSION == 34
        case GOD_PAKELLAS:
#endif
        case GOD_QAZLAL:
        case GOD_VEHUMET:
        case GOD_ZIN:
        default:
            return 25;
        case GOD_IGNIS:
            return 15; // baby wrath!
        case GOD_RU:
            return 0;
    }
}

static void _ash_uncurse()
{
    bool uncursed = false;
    // iterate backwards so we shatter a ring on the macabre finger
    // necklace before the amulet
    for (int eq_typ = NUM_EQUIP - 1; eq_typ >= EQ_FIRST_EQUIP; eq_typ--)
    {
        const int slot = you.equip[eq_typ];
        if (slot == -1)
            continue;
        if (!you.inv[slot].cursed())
            continue;
        if (!uncursed)
        {
            mprf(MSGCH_GOD, GOD_ASHENZARI, "Your curses shatter.");
            uncursed = true;
        }
        unequip_item(static_cast<equipment_type>(eq_typ));
    }
}

int excom_xp_docked()
{
    const int max_xl = you.get_max_xl();
    return exp_needed(min<int>(you.max_level, max_xl) + 1)
         - exp_needed(min<int>(you.max_level, max_xl));
}

void excommunication(bool voluntary, god_type new_god)
{
    const god_type old_god = you.religion;
    ASSERT(old_god != new_god);
    ASSERT(old_god != GOD_NO_GOD);

    const bool had_halo       = have_passive(passive_t::halo);
    const bool had_umbra      = have_passive(passive_t::umbra);
    const bool had_water_walk = have_passive(passive_t::water_walk);
    const bool had_stat_boost = have_passive(passive_t::stat_boost);
    const int  old_piety      = you.piety;

    god_acting gdact(old_god, true);

    take_note(Note(NOTE_LOSE_GOD, old_god));

    you.duration[DUR_PIETY_POOL] = 0; // your loss
    you.duration[DUR_RECITE] = 0;
    you.piety = 0;
    you.piety_hysteresis = 0;

    // so that the player isn't punished for "switching" between good gods via aX
    if (is_good_god(old_god) && voluntary)
    {
        you.saved_good_god_piety = old_piety;
        you.previous_good_god = old_god;
    }
    else
    {
        you.saved_good_god_piety = 0;
        you.previous_good_god = GOD_NO_GOD;
    }

    you.num_current_gifts[old_god] = 0;

    you.religion = GOD_NO_GOD;

    if (best_skill(SK_FIRST_SKILL, SK_LAST_SKILL) == SK_INVOCATIONS
       && you.attribute[ATTR_TRAITOR] == 0)
    {
        you.attribute[ATTR_TRAITOR] = static_cast<int>(old_god);
    }

    you.redraw_title = true;

    // Renouncing may have changed the conducts on our wielded or
    // quivered weapons, so refresh the display.
    you.wield_change = true;
    quiver::set_needs_redraw();

    mpr("You have lost your religion!");
    // included in default force_more_message

    if (old_god == GOD_BEOGH)
    {
        // The player's symbol depends on Beogh worship.
        update_player_symbol();
    }

    mark_milestone("god.renounce", "abandoned " + god_name(old_god) + ".");
    update_whereis();

    if (old_god == GOD_IGNIS)
        simple_god_message(" blazes with a vengeful fury!", false, old_god);
    else if (god_hates_your_god(old_god, new_god))
    {
        simple_god_message(
            make_stringf(" does not appreciate desertion%s!",
                         _god_hates_your_god_reaction(old_god, new_god).c_str()).c_str(),
            false, old_god);
    }

    if (had_halo)
    {
        mprf(MSGCH_GOD, old_god, "Your divine halo fades away.");
        invalidate_agrid(true);
    }
    if (had_umbra)
    {
        mprf(MSGCH_GOD, old_god, "Your aura of darkness fades away.");
        invalidate_agrid(true);
    }
    // You might have lost water walking at a bad time...
    if (had_water_walk && _need_water_walking())
        _grant_temporary_waterwalk();
    if (had_stat_boost)
    {
        redraw_screen();
        update_screen();
        notify_stat_change();
    }

    switch (old_god)
    {
    case GOD_KIKUBAAQUDGHA:
        mprf(MSGCH_GOD, old_god, "You sense decay."); // in the state of Denmark
        add_daction(DACT_ROT_CORPSES);
        break;

    case GOD_YREDELEMNUL:
        you.props.erase(YRED_TORCH_POWER_KEY);
        you.duration[DUR_FATHOMLESS_SHACKLES] = 0;
        yred_end_blasphemy();
        for (monster_iterator mi; mi; ++mi)
            if (is_yred_undead_follower(**mi))
            {
                // Bound souls should still drop their equipment
                monster_die(**mi, KILL_RESET_KEEP_ITEMS, NON_MONSTER);
            }
        remove_all_companions(GOD_YREDELEMNUL);
        add_daction(DACT_OLD_CHARMD_SOULS_POOF);
        break;

    case GOD_VEHUMET:
        you.vehumet_gifts.clear();
        you.duration[DUR_VEHUMET_GIFT] = 0;
        break;

    case GOD_MAKHLEB:
        dismiss_divine_allies_fineff::schedule(GOD_MAKHLEB);
        if (you.form == transformation::slaughter)
            untransform();
        break;

    case GOD_TROG:
        if (you.duration[DUR_TROGS_HAND])
            trog_remove_trogs_hand();
        dismiss_divine_allies_fineff::schedule(GOD_TROG);
        you.skills_to_show.insert(SK_SPELLCASTING);
        break;

    case GOD_BEOGH:
        simple_god_message(" voice booms out: Traitor to your kin!", true, old_god);
        mprf(MSGCH_MONSTER_ENCHANT, "All of your followers decide to abandon you.");

        add_daction(DACT_ALLY_BEOGH);
        remove_all_companions(GOD_BEOGH);

        // End statuses without normal messages.
        // (This will let a hostile apostle stay around, but that is intentional)
        you.duration[DUR_BEOGH_DIVINE_CHALLENGE] = 0;
        you.duration[DUR_BLOOD_FOR_BLOOD] = 0;
        you.duration[DUR_BEOGH_SEEKING_VENGEANCE] = 0;
        you.duration[DUR_BEOGH_CAN_RECRUIT] = 0;
        add_daction(DACT_BEOGH_VENGEANCE_CLEANUP);

        env.level_state |= LSTATE_BEOGH;
        break;

    case GOD_SIF_MUNA:
        if (you.duration[DUR_CHANNEL_ENERGY])
            you.duration[DUR_CHANNEL_ENERGY] = 0;
#if TAG_MAJOR_VERSION == 34
        if (you.attribute[ATTR_DIVINE_ENERGY])
            you.attribute[ATTR_DIVINE_ENERGY] = 0;
#endif
        break;

    case GOD_NEMELEX_XOBEH:
        reset_cards();
        mprf(MSGCH_GOD, old_god, "Your access to %s's decks is revoked.",
             god_name(old_god).c_str());
        break;

    case GOD_SHINING_ONE:
        if (you.duration[DUR_DIVINE_SHIELD])
            you.duration[DUR_DIVINE_SHIELD] = 0;

        dismiss_divine_allies_fineff::schedule(GOD_SHINING_ONE);
        break;

    case GOD_ZIN:
        if (you.duration[DUR_DIVINE_STAMINA])
            zin_remove_divine_stamina();

        if (env.sanctuary_time)
            remove_sanctuary();
        break;

    case GOD_ELYVILON:
        if (you.duration[DUR_DIVINE_VIGOUR])
            elyvilon_remove_divine_vigour();
        you.exp_docked[old_god] = excom_xp_docked();
        you.exp_docked_total[old_god] = you.exp_docked[old_god];
        break;

    case GOD_JIYVA:
        if (you.duration[DUR_SLIMIFY])
            you.duration[DUR_SLIMIFY] = 0;
        if (you.duration[DUR_OOZEMANCY])
            jiyva_end_oozemancy();

        if (query_daction_counter(DACT_ALLY_SLIME))
        {
            mprf(MSGCH_MONSTER_ENCHANT, "All of your fellow slimes turn on you.");
            add_daction(DACT_ALLY_SLIME);
        }
        break;

    case GOD_FEDHAS:
        if (query_daction_counter(DACT_ALLY_PLANT))
        {
            mprf(MSGCH_MONSTER_ENCHANT, "The plants of the dungeon turn on you.");
            add_daction(DACT_ALLY_PLANT);
        }
        break;

    case GOD_ASHENZARI:
        you.exp_docked[old_god] = excom_xp_docked();
        you.exp_docked_total[old_god] = you.exp_docked[old_god];
        _ash_uncurse();
        break;

    case GOD_DITHMENOS:
        break;

    case GOD_GOZAG:
        if (you.attribute[ATTR_GOZAG_SHOPS_CURRENT])
        {
            mprf(MSGCH_GOD, old_god, "Your funded stores close, unable to pay "
                                     "their debts without your funds.");
            you.attribute[ATTR_GOZAG_SHOPS_CURRENT] = 0;
        }
        you.duration[DUR_GOZAG_GOLD_AURA] = 0;
        for (branch_iterator it; it; ++it)
            branch_bribe[it->id] = 0;
        add_daction(DACT_BRIBE_TIMEOUT);
        add_daction(DACT_REMOVE_GOZAG_SHOPS);
        shopping_list.remove_dead_shops();
        you.exp_docked[old_god] = excom_xp_docked();
        you.exp_docked_total[old_god] = you.exp_docked[old_god];
        break;

    case GOD_QAZLAL:
        if (old_piety >= piety_breakpoint(0))
        {
            mprf(MSGCH_GOD, old_god, "Your storm instantly dissipates.");
            you.redraw_armour_class = true;
        }
        if (you.duration[DUR_QAZLAL_FIRE_RES])
        {
            mprf(MSGCH_DURATION, "Your resistance to fire fades away.");
            you.duration[DUR_QAZLAL_FIRE_RES] = 0;
        }
        if (you.duration[DUR_QAZLAL_COLD_RES])
        {
            mprf(MSGCH_DURATION, "Your resistance to cold fades away.");
            you.duration[DUR_QAZLAL_COLD_RES] = 0;
        }
        if (you.duration[DUR_QAZLAL_ELEC_RES])
        {
            mprf(MSGCH_DURATION,
                 "Your resistance to electricity fades away.");
            you.duration[DUR_QAZLAL_ELEC_RES] = 0;
        }
        if (you.duration[DUR_QAZLAL_AC])
        {
            mprf(MSGCH_DURATION,
                 "Your resistance to physical damage fades away.");
            you.duration[DUR_QAZLAL_AC] = 0;
            you.redraw_armour_class = true;
        }
        break;

#if TAG_MAJOR_VERSION == 34
    case GOD_PAKELLAS:
        you.exp_docked[old_god] = excom_xp_docked();
        you.exp_docked_total[old_god] = you.exp_docked[old_god];
        break;
#endif

    case GOD_CHEIBRIADOS:
        simple_god_message(" continues to slow your movements.", false,
                           old_god);
        break;

    case GOD_HEPLIAKLQANA:
        add_daction(DACT_ALLY_HEPLIAKLQANA);
        remove_all_companions(GOD_HEPLIAKLQANA);

        you.exp_docked[old_god] = excom_xp_docked();
        you.exp_docked_total[old_god] = you.exp_docked[old_god];
        break;

    case GOD_WU_JIAN:
        you.attribute[ATTR_SERPENTS_LASH] = 0;
        if (you.props.exists(WU_JIAN_HEAVENLY_STORM_KEY))
            wu_jian_end_heavenly_storm();
        break;

    case GOD_OKAWARU:
        if (you.duration[DUR_HEROISM])
            okawaru_remove_heroism();
        if (you.duration[DUR_FINESSE])
            okawaru_remove_finesse();
        if (player_in_branch(BRANCH_ARENA))
            okawaru_end_duel(true);
        break;

    case GOD_IGNIS:
        simple_god_message(" burns away your resistance to fire.", false,
                           old_god);
        if (you.duration[DUR_FIERY_ARMOUR])
        {
            you.duration[DUR_FIERY_ARMOUR] = 0;
            mpr("Your cloak of flame burns out.");
            you.redraw_armour_class = true;
        }
        if (you.duration[DUR_RISING_FLAME])
        {
            you.duration[DUR_RISING_FLAME] = 0;
            mpr("Your rising flame fizzles out.");
        }
        break;

    case GOD_RU:
        if (!you.props[AVAILABLE_SAC_KEY].get_vector().empty())
            ru_reset_sacrifice_timer();
        break;

    default:
        break;
    }

    _set_wrath_penance(old_god);

#ifdef USE_TILE_LOCAL
    tiles.layout_statcol();
    redraw_screen();
    update_screen();
#endif

    // Evil hack.
    learned_something_new(HINT_EXCOMMUNICATE,
                          coord_def((int)new_god, old_piety));

    for (ability_type abil : get_god_abilities())
        you.skills_to_hide.insert(abil_skill(abil));

    update_can_currently_train();
    reset_training();

    // Perhaps we abandoned Trog with everything but Spellcasting maxed out.
    check_selected_skills();
}

void nemelex_death_message()
{
    const int rank = min(random2(you.piety) / 30, 2);

    static const char *messages[NUM_PIETY_GAIN] =
    {
        "<lightgrey>Your body disappears without a glow.</lightgrey>",
        "Your body glows slightly and disappears.",
        "<white>Your body glows with a rainbow of weird colours and disappears.</white>",
    };

    static const char *glowing_messages[NUM_PIETY_GAIN] =
    {
        "<lightgrey>Your body disappears without additional glow.</lightgrey>",
        "Your body glows slightly brighter and disappears.",
        "<white>Your body glows with a rainbow of weird colours and disappears.</white>",
    };

    mpr((you.backlit() ? glowing_messages : messages)[rank]);
}

bool god_hates_attacking_friend(god_type god, const monster& fr)
{
    if (fr.kill_alignment() != KC_FRIENDLY)
        return false;

    monster_type species = fr.mons_species();

    // Nobody minds you hurting inanimate objects
    if ((fr.holiness() & MH_NONLIVING) && mons_intel(fr) == I_BRAINLESS)
        return false;
    switch (god)
    {
        case GOD_ZIN:
        case GOD_SHINING_ONE:
        case GOD_ELYVILON:
            return true;
        case GOD_BEOGH: // added penance to avoid killings for loot
            return mons_genus(species) == MONS_ORC;
        default:
            return false;
    }
}

static bool _transformed_player_can_join_god(god_type which_god)
{
    if (which_god == GOD_ZIN && you.form != transformation::none)
        return false; // zin hates everything

    if (is_good_god(which_god) && you.form == transformation::death)
        return false;

    return true;
}

int gozag_service_fee()
{
    if (you.char_class == JOB_MONK && had_gods() == 0)
        return 0;

    if (crawl_state.game_is_sprint())
        return 0;

    const int gold = you.attribute[ATTR_GOLD_GENERATED];
    int fee = (int)(gold - gold / log10(gold + 10.0))/2;

    dprf("found %d gold, fee %d", gold, fee);
    return fee;
}

static bool _god_rejects_loveless(god_type god)
{
    switch (god)
    {
    case GOD_BEOGH:
    case GOD_JIYVA:
    case GOD_HEPLIAKLQANA:
    case GOD_FEDHAS:
    case GOD_YREDELEMNUL:
        return true;
    default:
        return false;
    }
}

/**
 * Return true if the player can worship which_god.
 *
 * @param which_god  god to query
 * @param temp       If true (default), test if you can worship which_god now.
 *                   If false, test if you may ever be able to worship the god.
 * @return           Whether you can worship which_god.
 */
bool player_can_join_god(god_type which_god, bool temp)
{
    if (you.has_mutation(MUT_FORLORN))
        return false;

    if (is_good_god(which_god) && you.undead_or_demonic(temp))
        return false;

    if (you.has_mutation(MUT_INNATE_CASTER)
        && (which_god == GOD_SIF_MUNA
            || which_god == GOD_VEHUMET
            || which_god == GOD_KIKUBAAQUDGHA))
    {
        return false;
    }

    if (which_god == GOD_GOZAG && temp && you.gold < gozag_service_fee())
        return false;

    if (you.get_base_mutation_level(MUT_NO_LOVE, true, temp, temp)
        && _god_rejects_loveless(which_god))
    {
        return false;
    }

    return !temp || _transformed_player_can_join_god(which_god);
}

// Handle messaging and identification for items/equipment on conversion.
static void _god_welcome_handle_gear()
{
    // Check for amulets of faith.
    item_def *amulet = you.slot_item(EQ_AMULET, false);
    if (amulet && amulet->sub_type == AMU_FAITH
        && !you.has_mutation(MUT_FAITH)
        && ignore_faith_reason().empty())
    {
        mprf(MSGCH_GOD, "Your amulet flashes!");
        flash_view_delay(UA_PLAYER, god_colour(you.religion), 300);
    }

    if (have_passive(passive_t::identify_items))
        ash_id_inventory();

    if (have_passive(passive_t::detect_portals))
        ash_detect_portals(true);

    // Give a reminder to remove any disallowed equipment.
    for (int i = EQ_FIRST_EQUIP; i < NUM_EQUIP; i++)
    {
        const item_def* item = you.slot_item(static_cast<equipment_type>(i));
        if (item && god_hates_item(*item))
        {
            mprf(MSGCH_GOD, "%s warns you to remove %s.",
                 uppercase_first(god_name(you.religion)).c_str(),
                 item->name(DESC_YOUR, false, false, false).c_str());
        }
    }

    if (you.props.exists(PARAGON_WEAPON_KEY))
    {
        item_def wpn = you.props[PARAGON_WEAPON_KEY].get_item();
        if (god_hates_item(wpn))
        {
            mprf(MSGCH_GOD, "%s removes the imprint of %s from your paragon.",
                 god_name(you.religion).c_str(),
                 wpn.name(DESC_THE).c_str());
            you.props.erase(PARAGON_WEAPON_KEY);
        }
    }

    // Refresh wielded/quivered weapons in case we have a new conduct
    // on them.
    you.wield_change = true;
    quiver::set_needs_redraw();
}

/* Make a CrawlStoreValue an empty vector with the requested item type.
 * It is an error if the value already had a different type.
 */
static void _make_empty_vec(CrawlStoreValue &v, store_val_type vectype)
{
    const store_val_type currtype = v.get_type();
    ASSERT(currtype == SV_NONE || currtype == SV_VEC);

    if (currtype == SV_NONE)
        v.new_vector(vectype);
    else
    {
        CrawlVector &vec = v.get_vector();
        const store_val_type old_vectype = vec.get_type();
        ASSERT(old_vectype == SV_NONE || old_vectype == vectype);
        vec.clear();
    }
}

// Note: we're trying for a behaviour where the player gets
// to keep their assigned invocation slots if they get excommunicated
// and then rejoin (but if they spend time with another god we consider
// the old invocation slots void and erase them). We also try to
// protect any bindings the character might have made into the
// traditional invocation slots (a-e and X). -- bwr
void set_god_ability_slots()
{
    ASSERT(!you_worship(GOD_NO_GOD));

    if (find(begin(you.ability_letter_table), end(you.ability_letter_table),
             ABIL_RENOUNCE_RELIGION) == end(you.ability_letter_table)
        && you.ability_letter_table[letter_to_index('X')] == ABIL_NON_ABILITY)
    {
        you.ability_letter_table[letter_to_index('X')] = ABIL_RENOUNCE_RELIGION;
    }

    // Clear out other god invocations.
    for (ability_type& slot : you.ability_letter_table)
    {
        for (god_iterator it; it; ++it)
        {
            if (slot == ABIL_NON_ABILITY)
                break;
            if (*it == you.religion)
                continue;
            for (const god_power& power : get_all_god_powers()[*it])
                if (slot == power.abil)
                    slot = ABIL_NON_ABILITY;
        }
    }

    int num = letter_to_index('a');
    // Not using get_god_powers, so that hotkeys remain stable across games
    // even if you can't use a particular ability in a given game.
    for (const god_power& power : get_all_god_powers()[you.religion])
    {
        if (power.abil != ABIL_NON_ABILITY
            // hep ident goes to G, so don't take b for it (hack alert)
            && power.abil != ABIL_HEPLIAKLQANA_IDENTITY
            && find(begin(you.ability_letter_table),
                    end(you.ability_letter_table), power.abil)
               == end(you.ability_letter_table)
            && you.ability_letter_table[num] == ABIL_NON_ABILITY)
        {
            // Assign sequentially: first ability 'a', third ability 'c',
            // etc., even if one is remapped.
            if (find_ability_slot(power.abil, index_to_letter(num)) < 0)
            {
                you.ability_letter_table[num] = power.abil;
                auto_assign_ability_slot(num);
            }
            ++num;
        }
    }
}

/// Check if the monk's joining bonus should be given. (Except Gozag's.)
static void _apply_monk_bonus()
{
    if (you.char_class != JOB_MONK || had_gods() > 0)
        return;

    // monks get bonus piety for first god
    if (you_worship(GOD_RU))
        you.props[RU_SACRIFICE_PROGRESS_KEY] = 9999;
    else if (you_worship(GOD_ASHENZARI))
    {
        // two curses in (somewhat) rapid succession
        ashenzari_offer_new_curse();
        you.props[ASHENZARI_CURSE_PROGRESS_KEY] = 19;
    }
    else if (you_worship(GOD_USKAYAW))  // Gaining piety past this point does nothing
        gain_piety(15, 1, false);       // of value with this god and looks weird.
    else
        gain_piety(35, 1, false);
}

/// Transfer some piety from an old good god to a new one, if applicable.
static void _transfer_good_god_piety()
{
    if (!is_good_god(you.religion))
        return;

    const god_type old_god = you.previous_good_god;
    const uint8_t old_piety = you.saved_good_god_piety;

    if (!is_good_god(old_god))
        return;

    if (you.religion != old_god)
    {
        static const map<god_type, const char*> farewell_messages = {
            { GOD_ELYVILON, "aid the meek" },
            { GOD_SHINING_ONE, "vanquish evil" },
            { GOD_ZIN, "enforce order" },
        };

        // Some feedback that piety moved over.
        simple_god_message(make_stringf(" says: Farewell. Go and %s with %s.",
                                        lookup(farewell_messages, you.religion,
                                               "become a bug"),
                                        god_name(you.religion).c_str()).c_str(),

                           false, old_god);
    }

    // Give a piety bonus when switching between good gods, or back to the
    // same good god.
    if (old_piety > piety_breakpoint(0))
        gain_piety(old_piety - piety_breakpoint(0), 2, false);
}


/**
 * Give an appropriate message for the given good god to give in response to
 * the player joining a god that brings down their wrath.
 *
 * @param good_god    The good god in question.
 */
static string _good_god_wrath_message(god_type good_god)
{
    switch (good_god)
    {
        case GOD_ELYVILON:
            return "Your evil deeds will not go unpunished";
        case GOD_SHINING_ONE:
            return "You will pay for your evil ways, mortal";
        case GOD_ZIN:
            return make_stringf("You will suffer for embracing such %s",
                                is_chaotic_god(you.religion) ? "chaos"
                                                             : "evil");
        default:
            return "You will be buggily punished for this";
    }
}

/**
 * Check if joining the current god will cause wrath for any previously-
 * worshipped good gods. If so, message & set penance timeouts.
 *
 * @param old_god    The previous god worshipped; may be GOD_NO_GOD.
 */
static void _check_good_god_wrath(god_type old_god)
{
    for (god_type good_god : { GOD_ELYVILON, GOD_SHINING_ONE, GOD_ZIN })
    {
        if (old_god == good_god || !you.penance[good_god]
            || !god_hates_your_god(good_god, you.religion))
        {
            continue;
        }

        const string wrath_message
            = make_stringf(" says: %s!",
                           _good_god_wrath_message(good_god).c_str());
        simple_god_message(wrath_message.c_str(), false, good_god);
        set_penance_xp_timeout();
    }
}

void initialize_ashenzari_props()
{
    if (!you.props.exists(ASHENZARI_CURSE_PROGRESS_KEY))
        you.props[ASHENZARI_CURSE_PROGRESS_KEY] = 0;
    if (!you.props.exists(ASHENZARI_CURSE_DELAY_KEY))
    {
        int delay = 20;
        if (crawl_state.game_is_sprint())
            delay /= SPRINT_MULTIPLIER;
        you.props[ASHENZARI_CURSE_DELAY_KEY] = delay;
    }
}

/// Handle basic god piety & related setup for a new-joined god.
static void _set_initial_god_piety()
{
    // Currently, penance is just zeroed. This could be much more
    // interesting.
    you.penance[you.religion] = 0;

    switch (you.religion)
    {
    case GOD_XOM:
        // Xom uses piety and gift_timeout differently.
        you.piety = HALF_MAX_PIETY;
        you.gift_timeout = random2(40) + random2(40);
        break;

    case GOD_ASHENZARI:
        you.piety = ASHENZARI_BASE_PIETY;
        you.piety_hysteresis = 0;
        you.gift_timeout = 0;
        initialize_ashenzari_props();
        break;

    case GOD_RU:
        you.piety = 10; // one moderate sacrifice should get you to *.
        you.piety_hysteresis = 0;
        you.gift_timeout = 0;

        // I'd rather this be in on_join(), but then it overrides the
        // monk bonus...
        you.props[RU_SACRIFICE_PROGRESS_KEY] = 0;
        // offer the first sacrifice faster than normal
        if (!you.props.exists(RU_SACRIFICE_DELAY_KEY))
        {
            int delay = 50;
            if (crawl_state.game_is_sprint())
                delay /= SPRINT_MULTIPLIER;
            you.props[RU_SACRIFICE_DELAY_KEY] = delay;
            you.props[RU_SACRIFICE_PENALTY_KEY] = 0;
        }
        break;

    case GOD_IGNIS:
        // Don't allow leaving & rejoining to reset piety
        // XXX: maybe this logic should all be in on_join?
        if (you.props.exists(MIN_IGNIS_PIETY_KEY))
            you.piety = you.props[MIN_IGNIS_PIETY_KEY].get_int();
        else
            you.piety = 130; // matches zealot with ecu bonus
        you.piety_hysteresis = 0;
        you.gift_timeout = 0;
        break;

    default:
        you.piety = 15; // to prevent near instant excommunication
        if (you.piety_max[you.religion] < 15)
            you.piety_max[you.religion] = 15;
        you.piety_hysteresis = 0;
        you.gift_timeout = 0;
        break;
    }

    handle_traitor(you.religion);

    // Tutorial needs berserk usable.
    if (crawl_state.game_is_tutorial())
        gain_piety(30, 1, false);

    _apply_monk_bonus();
    _transfer_good_god_piety();
}

/// Setup when joining the greedy magnates of Gozag.
static void _join_gozag()
{
    // Handle the fee.
    const int fee = gozag_service_fee();
    if (fee > 0)
    {
        ASSERT(you.gold >= fee);
        mprf(MSGCH_GOD, "You pay a service fee of %d gold.", fee);
        you.gold -= fee;
        you.attribute[ATTR_GOZAG_GOLD_USED] += fee;
    }
    else
        simple_god_message(" waives the service fee.");

    // Note relevant powers.
    bool needs_redraw = false;
    for (const auto& power : get_god_powers(you.religion))
    {
        if (you.gold >= get_gold_cost(power.abil))
        {
            power.display(true, "You have enough gold to %s.");
            needs_redraw = true;
        }
    }

    if (needs_redraw)
    {
#ifdef USE_TILE_LOCAL
        tiles.layout_statcol();
        redraw_screen();
        update_screen();
#else
        ;
#endif
    }

    // Move gold to top of piles.
    add_daction(DACT_GOLD_ON_TOP);
}

static void _join_okawaru()
{
    bool needs_message = false;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->was_created_by(you))
        {
            mi->del_ench(ENCH_SUMMON_TIMER);
            needs_message = true;
        }
    }
    if (needs_message)
        mpr("Your summoned allies are dismissed!");
}

/// Setup when joining the sacred cult of Ru.
static void _join_ru()
{
    _make_empty_vec(you.props[AVAILABLE_SAC_KEY], SV_INT);
    _make_empty_vec(you.props[HEALTH_SAC_KEY], SV_INT);
    _make_empty_vec(you.props[ESSENCE_SAC_KEY], SV_INT);
    _make_empty_vec(you.props[PURITY_SAC_KEY], SV_INT);
    _make_empty_vec(you.props[ARCANA_SAC_KEY], SV_INT);
}

/// Setup for joining the furious barbarians of Trog.
void join_trog_skills()
{
    if (!you.has_mutation(MUT_DISTRIBUTED_TRAINING))
        set_magic_training(TRAINING_DISABLED);
    you.skills_to_hide.insert(SK_SPELLCASTING);
}

// Setup for joining the orderly ascetics of Zin.
static void _join_zin()
{
    // Need to pay St. Peters.
    if (you.attribute[ATTR_DONATIONS] * 9 < you.gold)
    {
        item_def lucre;
        lucre.base_type = OBJ_GOLD;
        // If you worshipped Zin before, the already tithed for part is fine.
        lucre.quantity = you.gold - you.attribute[ATTR_DONATIONS] * 9;
        // Use the harsh acquirement pricing -- with a cap at +50 piety.
        // We don't want you get max piety at start just because you're filthy
        // rich. In that case, you have to donate again more... That the poor
        // widow is not spared doesn't mean the rich can't be milked for more.
        lucre.props[ACQUIRE_KEY] = 0;
        you.gold -= zin_tithe(lucre, lucre.quantity, true);
    }

    if (you.props.exists(ORCIFICATION_LEVEL_KEY))
    {
        mprf(MSGCH_GOD, "Zin cleanses your body of Beogh's taint.");
        you.props.erase(ORCIFICATION_LEVEL_KEY);
    }
}

// Setup for joining the easygoing followers of Cheibriados.
static void _join_cheibriados()
{
    simple_god_message(" begins to support your attributes as your "
                       "movement slows.");
    notify_stat_change();
}

static void _join_makhleb()
{
    // Re-active our Mark, if we gained one, then abandoned and rejoined.
    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        if (you.innate_mutation[i] && is_makhleb_mark((mutation_type)i))
            mprf("Your %s burns with power once more.", mutation_name((mutation_type)i));
    }

    makhleb_initialize_marks();
}

// Initialize what Marks the player will eventually the offered.
void makhleb_initialize_marks()
{
    vector<mutation_type> muts =
    {
        MUT_MAKHLEB_MARK_HAEMOCLASM,
        MUT_MAKHLEB_MARK_LEGION,
        MUT_MAKHLEB_MARK_CARNAGE,
        MUT_MAKHLEB_MARK_ANNIHILATION,
        MUT_MAKHLEB_MARK_TYRANT,
        MUT_MAKHLEB_MARK_CELEBRANT,
        MUT_MAKHLEB_MARK_EXECUTION,
        MUT_MAKHLEB_MARK_ATROCITY,
        MUT_MAKHLEB_MARK_FANATIC,
    };
    shuffle_array(muts);

    CrawlVector& marks = you.props[MAKHLEB_OFFERED_MARKS_KEY].get_vector();
    for (int i = 0; i < 3; ++i)
        marks.push_back(muts[i]);
}

/// What special things happen when you join a god?
static const map<god_type, function<void ()>> on_join = {
    { GOD_BEOGH, update_player_symbol },
    { GOD_CHEIBRIADOS, _join_cheibriados },
    { GOD_FEDHAS, []() {
        mprf(MSGCH_MONSTER_ENCHANT, "The plants of the dungeon cease their "
             "hostilities.");
        if (env.forest_awoken_until)
            for (monster_iterator mi; mi; ++mi)
                mi->del_ench(ENCH_AWAKEN_FOREST);
    }},
    { GOD_GOZAG, _join_gozag },
    { GOD_LUGONU, []() {
        if (!player_in_branch(BRANCH_ABYSS)) return;
        // If this is your first time with Lucy, jump straight to 2* for a big power boost.
        // Otherwise, give just enough for 'exit the abyss'.
        const int bonus = you.worshipped[GOD_LUGONU] ? 20 : 40;
        gain_piety(bonus, 1, false);
    }},
    { GOD_OKAWARU, _join_okawaru },
    { GOD_RU, _join_ru },
    { GOD_TROG, join_trog_skills },
    { GOD_ZIN, _join_zin },
    { GOD_MAKHLEB, _join_makhleb },
    { GOD_JIYVA, []() { you.redraw_armour_class = true; /* slime wall immunity */ }}
};

static void _print_good_god_brand_changes(item_def *weapon, bool joining_good)
{
    if (!weapon
        || get_weapon_brand(*weapon) != SPWPN_FOUL_FLAME)
    {
        return;
    }
    if (joining_good)
    {
        mprf("%s goes dull and lifeless in your grasp.",
             weapon->name(DESC_YOUR).c_str());
    }
    else
    {
        mprf("%s glows horrifically with a foul blackness!",
             uppercase_first(weapon->name(DESC_YOUR)).c_str());
    }
}

void join_religion(god_type which_god)
{
    ASSERT(which_god != GOD_NO_GOD);
    ASSERT(which_god != GOD_ECUMENICAL);
    ASSERT(!you.has_mutation(MUT_FORLORN));

    redraw_screen();
    update_screen();

    const god_type old_god = you.religion;
    if (you.previous_good_god == GOD_NO_GOD)
    {
        you.previous_good_god = old_god;
        you.saved_good_god_piety = you.piety;
        // doesn't matter if old_god isn't actually a good god; we check later
        // and then wipe it at the end of the function regardless
    }

    // Leave your prior religion first.
    if (!you_worship(GOD_NO_GOD))
        excommunication(true, which_god);

    // Welcome to the fold!
    you.religion = static_cast<god_type>(which_god);
    set_god_ability_slots();    // remove old god's slots, reserve new god's

    // included in default force_more_message
    simple_god_message(make_stringf(" welcomes you%s!",
                                    you.worshipped[which_god] ? " back"
                                                              : "").c_str());
    update_whereis();

    _set_initial_god_piety();

    // Only mark the milestone now that piety has been set due to invo titles.
    mark_milestone("god.worship", "became a worshipper of "
                   + god_name(you.religion) + ".");
    take_note(Note(NOTE_GET_GOD, you.religion));

    const function<void ()> *join_effect = map_find(on_join, you.religion);
    if (join_effect != nullptr)
        (*join_effect)();

    if (!is_good_god(old_god) && is_good_god(you.religion))
    {
        _print_good_god_brand_changes(you.weapon(), true);
        _print_good_god_brand_changes(you.offhand_weapon(), true);
    }
    else if (is_good_god(old_god) && !is_good_god(you.religion))
    {
        _print_good_god_brand_changes(you.weapon(), false);
        _print_good_god_brand_changes(you.offhand_weapon(), false);
    }

    // after join_effect() so that gozag's service fee is right for monks
    if (you.worshipped[you.religion] < 100)
        you.worshipped[you.religion]++;

    // after join_effect so that ru is initialized correctly for get_abilities
    // when flash_view_delay redraws the screen in local tiles
    _god_welcome_handle_gear();

    // Warn if a good god is starting wrath now.
    _check_good_god_wrath(old_god);

    if (!you_worship(GOD_GOZAG))
        for (const auto& power : get_god_powers(you.religion))
            if (power.rank <= 0)
                power.display(true, "You can now %s.");

    // Allow training all divine ability skills immediately.
    vector<ability_type> abilities = get_god_abilities();
    for (ability_type abil : abilities)
        you.skills_to_show.insert(abil_skill(abil));
    update_can_currently_train();

    // now that you have a god, you can't save any piety from your prev god
    you.previous_good_god = GOD_NO_GOD;
    you.saved_good_god_piety = 0;

    you.redraw_title = true;

#ifdef USE_TILE_LOCAL
    tiles.layout_statcol();
    redraw_screen();
    update_screen();
#endif

    learned_something_new(HINT_CONVERT);
}

void god_pitch(god_type which_god)
{
    if (which_god == GOD_BEOGH && env.grid(you.pos()) != DNGN_ALTAR_BEOGH)
        mpr("You bow before the missionary of Beogh.");
    else
    {
        mprf("You %s the altar of %s.",
             get_form()->player_prayer_action().c_str(),
             god_name(which_god).c_str());
    }
    // these are included in default force_more_message

    // Note: using worship we could make some gods not allow followers to
    // return, or not allow worshippers from other religions. - bwr

    // Gods can be racist...
    if (!player_can_join_god(which_god))
    {
        you.turn_is_over = false;
        print_god_rejection(which_god);
        return;
    }

    if (which_god == GOD_LUGONU && you.penance[GOD_LUGONU])
    {
        you.turn_is_over = false;
        simple_god_message(" refuses to forgive you so easily!", false,
                           which_god);
        return;
    }

    if (describe_god_with_join(which_god))
        join_religion(which_god);
    else
    {
        you.turn_is_over = false;
        redraw_screen();
        update_screen();
    }
}

void print_god_rejection(god_type which_god)
{

    if (which_god == GOD_GOZAG)
    {
        simple_god_message(" does not accept service from beggars like you!",
                           false, which_god);
        const int fee = gozag_service_fee();
        if (you.gold == 0)
        {
            mprf("The service fee for joining is currently %d gold; you have"
                 " none.", fee);
        }
        else
        {
            mprf("The service fee for joining is currently %d gold; you only"
                 " have %d.", fee, you.gold);
        }
        return;
    }
    if (you.get_mutation_level(MUT_NO_LOVE) && _god_rejects_loveless(which_god))
    {
        simple_god_message(" does not accept worship from the loveless!",
                           false, which_god);
        return;
    }
    if (!_transformed_player_can_join_god(which_god))
    {
        simple_god_message(" says: How dare you approach in such a loathsome "
                           "form!", false, which_god);
        return;
    }

    simple_god_message(" does not accept worship from those such as you!",
                       false, which_god);
}

/** Ask the user for a god by name.
 *
 * @param def_god The god to use if the user presses just ENTER
 *                (default NUM_GODS).
 * @return the best match for the user's input, or def_god if the user
 *         pressed ENTER without providing input, or NUM_GODS if the
 *         user cancelled or there was no match.
 */
god_type choose_god(god_type def_god)
{
    char specs[80];

    string help = def_god == NUM_GODS ? "by name"
                                      : "default " + god_name(def_god);
    string prompt = make_stringf("Which god (%s)? ", help.c_str());

    if (msgwin_get_line(prompt, specs, sizeof(specs)) != 0)
        return NUM_GODS; // FIXME: distinguish cancellation from no match

    // If they pressed enter with no input.
    if (specs[0] == '\0')
        return def_god;

    string spec = lowercase_string(specs);

    return find_earliest_match(spec, GOD_NO_GOD, NUM_GODS,
                               always_true<god_type>,
                               bind(god_name, placeholders::_1, false));
}

static void _choose_ecu_gods(CrawlVector &gods)
{
    vector<god_type> possible_gods;
    for (int i = GOD_NO_GOD + 1; i < NUM_GODS; ++i)
    {
        const god_type god = static_cast<god_type>(i);
        if (!is_unavailable_god(god) && player_can_join_god(god, false))
            possible_gods.push_back(god);
    }
    shuffle_array(possible_gods); // inefficient but who cares
    for (int i = 0; i < 3 && i < int(possible_gods.size()); ++i)
        gods.push_back(possible_gods[i]);
}

/// Returns a list of gods the player might get from the ecu altar at the given position.
vector<god_type> get_ecu_gods(coord_def pos)
{
    const string key = make_stringf("ecu-%d,%d", pos.x, pos.y);
    CrawlVector &saved_gods = env.properties[key].get_vector();
    if (saved_gods.empty())
        _choose_ecu_gods(saved_gods);
    vector<god_type> gods;
    for (int i = 0; i < int(saved_gods.size()); ++i)
        gods.push_back(static_cast<god_type>(saved_gods[i].get_int()));
    return gods;
}

int had_gods()
{
    int count = 0;
    for (god_iterator it; it; ++it)
        count += you.worshipped[*it];
    return count;
}

bool god_likes_your_god(god_type god, god_type your_god)
{
    return is_good_god(god) && is_good_god(your_god);
}

bool god_hates_your_god(god_type god, god_type your_god)
{
    // Ru doesn't care.
    if (god == GOD_RU)
        return false;

    // Gods do not hate themselves.
    if (god == your_god)
        return false;

    // Non-good gods always hate your current god.
    if (!is_good_god(god))
        return true;

    // Zin hates chaotic gods.
    if (god == GOD_ZIN && is_chaotic_god(your_god))
        return true;

    return is_evil_god(your_god);
}

bool god_hates_killing(god_type god, const monster& mon)
{
    if (invalid_monster(&mon))
        return false;

    // kill as many illusions as you want.
    if (mon.is_illusion())
        return false;

    bool retval = false;
    const mon_holy_type holiness = mon.holiness();

    if (holiness & MH_HOLY)
        retval = (is_good_god(god));
    else if (holiness & MH_NATURAL)
        retval = (god == GOD_ELYVILON);

    return retval;
}

bool god_likes_spell(spell_type spell, god_type god)
{
    switch (god)
    {
    case GOD_VEHUMET:
        return vehumet_supports_spell(spell);
    case GOD_KIKUBAAQUDGHA:
        return spell_typematch(spell, spschool::necromancy);
    default: // quash unhandled constants warnings
        return false;
    }
}

/**
 * Does your god hate spellcasting?
 *
 * @param god           The god to check against
 * @return              Whether the god hates spellcasting
 */
bool god_hates_spellcasting(god_type god)
{
    return god == GOD_TROG;
}

/**
 * Will your god put you under penance if you actually cast spell?
 *
 * @param spell         The spell to check against
 * @param god           The god to check against
 * @param fake_spell    true if the spell is evoked or from an innate or divine ability
 *                      false if it is a spell being cast normally.
 * @return              true if the god hates the spell
 */
bool god_hates_spell(spell_type spell, god_type god, bool fake_spell)
{
    if (god_hates_spellcasting(god))
        return !fake_spell;

    if (god_punishes_spell(spell, god))
        return true;

    // (this is literally only Discord as of July 2022... simplify?)
    return god == GOD_CHEIBRIADOS && is_hasty_spell(spell);
}

/**
 * Checks to see if your god hates this spell (or spellcasting generally).
 * Returns a warning string if so.
 *
 * @param spell         The spell to check against
 * @param god           The god to check against
 * @return              Warning string if god has strong opinions on spell
 *                      Empty string if god doesn't care about spell
 */
string god_spell_warn_string(spell_type spell, god_type god)
{
    if (god_hates_spellcasting(god))
        return "Your god hates spellcasting!";
    if (god_hates_spell(spell, god))
        return "Your god hates this spell!";
    return "";
}

bool god_protects_from_harm()
{
    if ((have_passive(passive_t::protect_from_harm)
         || have_passive(passive_t::lifesaving))
         && x_chance_in_y(100 + min(piety_breakpoint(5), (int)you.piety), 1000))
    {
        return true;
    }

    if (!you.gift_timeout && have_passive(passive_t::lifesaving)
        && x_chance_in_y(you.piety, 160))
    {
        _inc_gift_timeout(20 + random2avg(10, 2));
        return true;
    }

    return false;
}

void handle_god_time(int /*time_delta*/)
{
    if (you.attribute[ATTR_GOD_WRATH_COUNT] > 0)
    {
        vector<god_type> angry_gods;
        // First count the number of gods to whom we owe penance.
        for (god_iterator it; it; ++it)
        {
            if (active_penance(*it))
                angry_gods.push_back(*it);
        }
        if (x_chance_in_y(angry_gods.size(), 20))
        {
            // This should be guaranteed; otherwise the god wouldn't have
            // appeared in the angry_gods list.
            const bool succ = divine_retribution(*random_iterator(angry_gods));
            ASSERT(succ);
        }
        you.attribute[ATTR_GOD_WRATH_COUNT]--;
    }

    // Update the god's opinion of the player.
    if (!you_worship(GOD_NO_GOD))
    {
        int delay;
        int sacrifice_count;
        switch (you.religion)
        {
        case GOD_TROG:
        case GOD_OKAWARU:
        case GOD_MAKHLEB:
        case GOD_LUGONU:
        case GOD_DITHMENOS:
        case GOD_QAZLAL:
        case GOD_KIKUBAAQUDGHA:
        case GOD_VEHUMET:
        case GOD_ZIN:
#if TAG_MAJOR_VERSION == 34
        case GOD_PAKELLAS:
#endif
        case GOD_JIYVA:
        case GOD_WU_JIAN:
        case GOD_SIF_MUNA:
        case GOD_YREDELEMNUL:
            if (one_chance_in(17))
                lose_piety(1);
            break;

        case GOD_ELYVILON:
        case GOD_HEPLIAKLQANA:
        case GOD_FEDHAS:
        case GOD_CHEIBRIADOS:
        case GOD_SHINING_ONE:
        case GOD_NEMELEX_XOBEH:
            if (one_chance_in(35))
                lose_piety(1);
            break;

        case GOD_BEOGH:
            if (one_chance_in(17))
                lose_piety(1);
            maybe_generate_apostle_challenge();
            break;

        case GOD_ASHENZARI:
            ASSERT(you.props.exists(ASHENZARI_CURSE_PROGRESS_KEY));
            ASSERT(you.props.exists(ASHENZARI_CURSE_DELAY_KEY));

            if (you.props[ASHENZARI_CURSE_PROGRESS_KEY].get_int()
                >= you.props[ASHENZARI_CURSE_DELAY_KEY].get_int())
            {
                ashenzari_offer_new_curse();
            }
            break;

        case GOD_RU:
            ASSERT(you.props.exists(RU_SACRIFICE_PROGRESS_KEY));
            ASSERT(you.props.exists(RU_SACRIFICE_DELAY_KEY));
            ASSERT(you.props.exists(AVAILABLE_SAC_KEY));

            delay = you.props[RU_SACRIFICE_DELAY_KEY].get_int();
            sacrifice_count = you.props[AVAILABLE_SAC_KEY].get_vector().size();

            // 6* is max piety for Ru
            if (sacrifice_count == 0 && you.piety < piety_breakpoint(5)
                && you.props[RU_SACRIFICE_PROGRESS_KEY].get_int() >= delay)
            {
              ru_offer_new_sacrifices();
            }

            break;

        case GOD_IGNIS:
            // Losing piety over time would be extremely annoying for people
            // trying to get polytheist with Ignis. Almost impossible.
        case GOD_USKAYAW:
            // We handle Uskayaw elsewhere because this func gets called rarely
        case GOD_GOZAG:
        case GOD_XOM:
            // Gods without normal piety do nothing each tick.
            return;

        case GOD_NO_GOD:
        case GOD_RANDOM:
        case GOD_ECUMENICAL:
        case GOD_NAMELESS:
        case NUM_GODS:
            die("Bad god, no bishop!");
            return;

        }

        if (you.piety < 1)
            excommunication();
    }

    if (player_in_branch(BRANCH_CRUCIBLE))
        makhleb_handle_crucible_of_flesh();
}

int god_colour(god_type god) // mv - added
{
    switch (god)
    {
    case GOD_ZIN:
    case GOD_SHINING_ONE:
    case GOD_ELYVILON:
    case GOD_OKAWARU:
    case GOD_FEDHAS:
        return CYAN;

    case GOD_KIKUBAAQUDGHA:
    case GOD_MAKHLEB:
    case GOD_VEHUMET:
    case GOD_TROG:
    case GOD_BEOGH:
    case GOD_LUGONU:
    case GOD_ASHENZARI:
    case GOD_WU_JIAN:
        return LIGHTRED;

    case GOD_GOZAG:
    case GOD_XOM:
    case GOD_IGNIS:
        return YELLOW;

    case GOD_NEMELEX_XOBEH:
        return LIGHTMAGENTA;

    case GOD_SIF_MUNA:
        return LIGHTBLUE;

    case GOD_JIYVA:
        return GREEN;

    case GOD_CHEIBRIADOS:
    case GOD_HEPLIAKLQANA:
        return LIGHTCYAN;

    case GOD_YREDELEMNUL:
    case GOD_DITHMENOS:
    case GOD_USKAYAW:
        return MAGENTA;

    case GOD_QAZLAL:
    case GOD_RU:
        return BROWN;

#if TAG_MAJOR_VERSION == 34
    case GOD_PAKELLAS:
        return LIGHTGREEN;
#endif

    case GOD_NO_GOD:
    case NUM_GODS:
    case GOD_RANDOM:
    case GOD_NAMELESS:
    default:
        break;
    }

    return YELLOW;
}

colour_t god_message_altar_colour(god_type god)
{
    int rnd;

    switch (god)
    {
    case GOD_SHINING_ONE:
        return YELLOW;

    case GOD_ZIN:
        return WHITE;

    case GOD_ELYVILON:
        return LIGHTBLUE;     // Really, LIGHTGREY but that's plain text.

    case GOD_OKAWARU:
        return CYAN;

    case GOD_YREDELEMNUL:
        return random_choose(DARKGREY, RED);

    case GOD_BEOGH:
        return random_choose(BROWN, LIGHTRED);

    case GOD_KIKUBAAQUDGHA:
        return DARKGREY;

    case GOD_FEDHAS:
        return random_choose(BROWN, GREEN);

    case GOD_XOM:
        return random2(15) + 1;

    case GOD_VEHUMET:
        rnd = random2(3);
        return (rnd == 0) ? LIGHTMAGENTA :
               (rnd == 1) ? LIGHTRED
                          : LIGHTBLUE;

    case GOD_MAKHLEB:
        rnd = random2(3);
        return (rnd == 0) ? RED :
               (rnd == 1) ? LIGHTRED
                          : YELLOW;

    case GOD_TROG:
        return RED;

    case GOD_NEMELEX_XOBEH:
        return LIGHTMAGENTA;

    case GOD_SIF_MUNA:
        return BLUE;

    case GOD_LUGONU:
    case GOD_WU_JIAN:
        return LIGHTRED;

    case GOD_CHEIBRIADOS:
        return LIGHTCYAN;

    case GOD_JIYVA:
        return random_choose(GREEN, LIGHTGREEN);

    case GOD_DITHMENOS:
        return MAGENTA;

    case GOD_GOZAG:
        return random_choose(YELLOW, BROWN);

    case GOD_QAZLAL:
    case GOD_RU:
        return BROWN;

#if TAG_MAJOR_VERSION == 34
    case GOD_PAKELLAS:
        return random_choose(LIGHTMAGENTA, LIGHTGREEN, LIGHTCYAN);
#endif

    case GOD_USKAYAW:
        return random_choose(RED, MAGENTA);

    case GOD_HEPLIAKLQANA:
        return random_choose(LIGHTGREEN, LIGHTBLUE);

    case GOD_IGNIS:
        return random_choose(WHITE, YELLOW);

    default:
        return YELLOW;
    }
}

int piety_rank(int piety)
{
    // XXX: this seems to be used only in dat/database/godspeak.txt?
    if (you_worship(GOD_XOM))
    {
        const int breakpoints[] = { 20, 50, 80, 120, 180, INT_MAX };
        for (unsigned int i = 0; i < ARRAYSZ(breakpoints); ++i)
            if (piety <= breakpoints[i])
                return i + 1;
        die("INT_MAX is no good");
    }

    for (int i = NUM_PIETY_STARS; i >= 1; --i)
        if (piety >= piety_breakpoint(i - 1))
            return i;

    return 0;
}

int piety_breakpoint(int i)
{
    int breakpoints[NUM_PIETY_STARS] = { 30, 50, 75, 100, 120, 160 };
    if (i >= NUM_PIETY_STARS || i < 0)
        return 255;
    else
        return breakpoints[i];
}

int get_monster_tension(const monster& mons, god_type god)
{
    if (!mons.alive())
        return 0;

    if (you.see_cell(mons.pos()))
    {
        if (!mons_can_hurt_player(&mons))
            return 0;
    }

    const mon_attitude_type att = mons_attitude(mons);

    if (mons.cannot_act())
        return 0;

    int exper = exper_value(mons);

    // XXX: It's hard to entirely figure out how strong a projectile
    // or bomb is offhand, but they should count for _some_ minimal tension.
    if (exper <= 0)
    {
        if (mons.is_peripheral())
            exper = 50;
        else
            return 0;
    }

    // Almost dead monsters might die the next turn, but
    // they're also still entirely capable of killing you.
    if (att == ATT_HOSTILE || att == ATT_NEUTRAL)
        exper = exper * (10 + (mons.hit_points * 10 / mons.max_hit_points)) / 30;
    else
        exper = exper * (10 + (mons.hit_points * 10 / mons.max_hit_points)) / 50;

    bool gift = false;

    if (god != GOD_NO_GOD)
        gift = mons_is_god_gift(mons, god);

    // FIXME: God gift checking is fine for Xom, weird for almost everything else.
    // Need to look at the Beogh apostle thresholds before adjusting it there.
    if (att == ATT_HOSTILE)
    {
        // God is punishing you with a hostile gift, so it doesn't
        // count towards tension.
        if (gift)
            return 0;
    }
    else if (att == ATT_FRIENDLY)
    {
        // Friendly monsters being around to help you reduce tension.
        exper = -exper;

        // If it's a god gift, it reduces tension even more, since
        // the god is already helping you out.
        if (gift)
            exper *= 2;
    }
    else if (att == ATT_NEUTRAL)
    {
        // Might hit you, might hit something else. Unreliable in threat.
        exper = exper / 2;
    }
    else if (att == ATT_GOOD_NEUTRAL)
    {
        // Unreliable in its help versus wandering around or leaving entirely.
        exper = -exper / 2;
    }

    if (mons.asleep() || mons_is_fleeing(mons))
        exper /= 20;

    if (att != ATT_FRIENDLY && att != ATT_GOOD_NEUTRAL)
    {
        if (!you.visible_to(&mons))
            exper = exper * 2 / 3;
        if (!mons.visible_to(&you))
            exper *= 2;
    }

    if (mons.is_silenced() && (mons.is_actual_spellcaster() || mons.is_priest()))
        exper = exper * 2 / 3;

    if (mons.confused() || mons.caught())
        exper /= 2;

    if (mons.has_ench(ENCH_SLOW))
        exper = exper * 2 / 3;

    if (mons.has_ench(ENCH_HASTE))
        exper = exper * 3 / 2;

    if (mons.has_ench(ENCH_MIGHT))
        exper = exper * 5 / 4;

    if (mons.has_ench(ENCH_EMPOWERED_SPELLS))
        exper = exper * 5 / 4;

    if (mons.has_ench(ENCH_WORD_OF_RECALL))
        exper = exper * 5 / 4;

    if (mons.has_ench(ENCH_ARMED))
        exper = exper * 5 / 4;

    // Health boost stacks further on top of haste and might bonuses
    if (mons.berserk_or_frenzied())
        exper = exper * 3 / 2;

    return exper;
}

int get_tension(god_type god)
{
    int total = 0;

    bool nearby_monster = false;
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        const monster* mon = monster_at(*ri);

        if (mon && mon->alive() && you.can_see(*mon))
        {
            int exper = get_monster_tension(*mon, god);

            if (!mon->wont_attack())
                nearby_monster = true;

            total += exper;
        }
    }

    // At least one monster has to be nearby, for tension to count.
    if (!nearby_monster)
        return 0;

    // XXX: Maybe too low?
    const int scale = 1;

    int tension = total;

    // Tension goes up inversely proportional to the percentage of your max HP.
    tension *= (scale + 1) * you.hp_max;
    tension /= max(you.hp_max + scale * you.hp, 1);

    // Divides by 1 at level 1, 200 at level 27.
    const int exp_lev  = you.get_experience_level();
    const int exp_need = exp_needed(exp_lev);
    const int factor   = (int)ceil(sqrt(exp_need / 30.0));
    const int div      = 1 + factor;

    tension /= div;

    // Indefinite spawns and unreliable / delayed escape.
    if (player_in_branch(BRANCH_ABYSS))
    {
        if (tension < 2)
            tension = 2;
        else
            tension = tension * 3 / 2;
    }
    else if (player_in_branch(BRANCH_PANDEMONIUM))
        tension = tension * 9 / 8;

    if (player_on_orb_run())
    {
        if (tension < 3)
            tension = 3;
        else
            tension = tension * 2;
    }

    // Effects that'll still affect sleep / para / petrify go before those.
    if (you.duration[DUR_VITRIFIED])
        tension = tension * 4 / 3;

    if (you.form == transformation::bat ||
        you.form == transformation::wisp)
    {
        tension = tension * 3 / 2;
    }
    else if (you.form == transformation::fungus ||
             you.form == transformation::pig ||
             you.form == transformation::tree)
    {
        tension = tension * 5 / 3;
    }

    if (you.duration[DUR_CORROSION])
        tension = tension * (10 + you.props[CORROSION_KEY].get_int() / 4) / 10;

    if (you.cannot_act())
    {
        tension *= 10;
        tension = max(1, tension);

        return tension;
    }

    // Other effects are listed from highest influence to lowest to help with
    // rounding on more minor effects.
    if (you.confused())
    {
        // Later on, one only stays confused if the fight doesn't matter
        // or if they can't cure it, so scale this slowly with XL and
        // acknowledge its badness specifically when it's uncurable.
        if (player_in_branch(BRANCH_COCYTUS) || you.can_drink() == false)
            tension = tension * 5 / 2;
        else
            tension = tension * (9 - (you.experience_level / 10)) / 4;
    }

    if (you.caught())
        tension *= 2;

    if (silenced(you.pos()))
        tension = tension * 5 / 3;

    if (you.duration[DUR_SLOW])
        tension = tension * 3 / 2;

    if (you.duration[DUR_ATTRACTIVE])
        tension = tension * 3 / 2;

    if (you.duration[DUR_NO_CAST])
        tension = tension * 3 / 2;

    if (you.duration[DUR_SENTINEL_MARK])
        tension = tension * 3 / 2;

    if (you.duration[DUR_NO_POTIONS])
        tension = tension * 4 / 3;

    if (you.duration[DUR_NO_SCROLLS])
        tension = tension * 4 / 3;

    if (you.duration[DUR_MESMERISED])
        tension = tension * 6 / 5;

    if (you.duration[DUR_AFRAID])
        tension = tension * 6 / 5;

    if (you.magic_points <= you.max_magic_points / 10)
        tension = tension * 9 / 8;

    if (you.duration[DUR_HASTE])
        tension = tension * 2 / 3;

    return max(0, tension);
}

int get_fuzzied_monster_difficulty(const monster& mons)
{
    double factor = sqrt(exp_needed(you.experience_level) / 30.0);
    int exp = exper_value(mons) * 100;
    exp = random2(exp) + random2(exp);
    return exp / (1 + factor);
}

/////////////////////////////////////////////////////////////////////////////
// Stuff for placing god gift monsters after the player's turn has ended.
/////////////////////////////////////////////////////////////////////////////

static vector<mgen_data>       _delayed_data;
static deque<delayed_callback> _delayed_callbacks;
static deque<unsigned int>     _delayed_done_trigger_pos;
static deque<delayed_callback> _delayed_done_callbacks;
static deque<string>      _delayed_success;

void delayed_monster(const mgen_data &mg, delayed_callback callback)
{
    _delayed_data.push_back(mg);
    _delayed_callbacks.push_back(callback);
}

void delayed_monster_done(string success, delayed_callback callback)
{
    const unsigned int size = _delayed_data.size();
    ASSERT(size > 0);

    _delayed_done_trigger_pos.push_back(size - 1);
    _delayed_success.push_back(success);
    _delayed_done_callbacks.push_back(callback);
}

static void _place_delayed_monsters()
{
    // Last monster that was successfully placed (so far).
    monster *lastmon  = nullptr;
    int      placed   = 0;
    god_type prev_god = GOD_NO_GOD;

    for (unsigned int i = 0; i < _delayed_data.size(); i++)
    {
        mgen_data &mg          = _delayed_data[i];
        delayed_callback cback = _delayed_callbacks[i];

        if (prev_god != mg.god)
        {
            lastmon  = nullptr;
            placed   = 0;
            prev_god = mg.god;
        }

        monster *mon = create_monster(mg);

        if (cback)
            (*cback)(mg, mon, placed);

        if (mon)
        {
            if (you_worship(GOD_YREDELEMNUL)
                || you_worship(GOD_HEPLIAKLQANA)
                || have_passive(passive_t::convert_orcs))
            {
                add_companion(mon);
            }
            lastmon = mon;
            placed++;
        }

        if (!_delayed_done_trigger_pos.empty()
            && _delayed_done_trigger_pos[0] == i)
        {
            cback = _delayed_done_callbacks[0];

            string msg;
            if (lastmon)
            {
                ASSERT(placed > 0);
                msg = replace_all(_delayed_success[0], "@servant@",
                                  placed == 1
                                      ? lastmon->name(DESC_A)
                                      : pluralise(lastmon->name(DESC_PLAIN)));
            }
            else
                ASSERT(placed == 0);

            prev_god = GOD_NO_GOD;
            _delayed_done_trigger_pos.pop_front();
            _delayed_success.pop_front();
            _delayed_done_callbacks.pop_front();

            if (msg.empty())
            {
                if (cback)
                    (*cback)(mg, lastmon, placed);
                continue;
            }

            // Fake its coming from simple_god_message().
            if (msg[0] == ' ' || msg[0] == '\'')
                msg = uppercase_first(god_name(mg.god)) + msg;

            trim_string(msg);

            god_speaks(mg.god, msg.c_str());

            if (cback)
                (*cback)(mg, lastmon, placed);
        }
    }

    _delayed_data.clear();
    _delayed_callbacks.clear();
    _delayed_done_trigger_pos.clear();
    _delayed_success.clear();
}

static bool _is_god(god_type god)
{
    return god > GOD_NO_GOD && god < NUM_GODS;
}

static bool _is_temple_god(god_type god)
{
    if (!_is_god(god) || _is_disabled_god(god))
        return false;

    switch (god)
    {
    case GOD_NO_GOD:
    case GOD_LUGONU:
    case GOD_BEOGH:
    case GOD_JIYVA:
    case GOD_IGNIS:
        return false;

    default:
        return true;
    }
}

static bool _is_nontemple_god(god_type god)
{
    return _is_god(god) && !_is_temple_god(god) && !_is_disabled_god(god);
}

static bool _cmp_god_by_name(god_type god1, god_type god2)
{
    return god_name(god1, false) < god_name(god2, false);
}

// Vector of temple gods.
// Sorted by name for the benefit of the overview.
vector<god_type> temple_god_list()
{
    vector<god_type> god_list;
    for (god_iterator it; it; ++it)
        if (_is_temple_god(*it))
            god_list.push_back(*it);
    sort(god_list.begin(), god_list.end(), _cmp_god_by_name);
    return god_list;
}

// Vector of non-temple gods.
// Sorted by name for the benefit of the overview.
vector<god_type> nontemple_god_list()
{
    vector<god_type> god_list;
    for (god_iterator it; it; ++it)
        if (_is_nontemple_god(*it))
            god_list.push_back(*it);
    sort(god_list.begin(), god_list.end(), _cmp_god_by_name);
    return god_list;
}

bool god_power_usable(const god_power& power, bool ignore_piety, bool ignore_penance)
{
    // not an activated power
    if (power.abil == ABIL_NON_ABILITY)
        return false;
    const ability_type abil = fixup_ability(power.abil);
    ASSERT(abil != ABIL_NON_ABILITY);
    return power.god == you.religion
            && (power.rank <= 0
                || power.rank == 7 && can_do_capstone_ability(you.religion)
                || piety_rank() >= power.rank
                || ignore_piety)
            && (!player_under_penance()
                || power.rank == -1
                || ignore_penance);
}

const god_power* god_power_from_ability(ability_type abil)
{
    for (int god = GOD_NO_GOD; god < NUM_GODS; god++)
    {
        for (const auto& power : get_all_god_powers()[god])
        {
            if (power.abil == abil)
                return &power;
        }
    }
    return nullptr;
}
