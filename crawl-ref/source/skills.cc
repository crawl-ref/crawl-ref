/**
 * @file
 * @brief Skill exercising functions.
**/

#include "AppHdr.h"

#include "skills.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <sstream>

#include "ability.h"
#include "clua.h"
#include "describe-god.h"
#include "evoke.h"
#include "files.h"
#include "god-abil.h"
#include "god-conduct.h"
#include "god-passive.h"
#include "hints.h"
#include "item-prop.h"
#include "libutil.h"
#include "message.h"
#include "notes.h"
#include "output.h"
#include "random.h"
#include "religion.h"
#include "skill-menu.h"
#include "sprint.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"

// MAX_COST_LIMIT is the maximum XP amount it will cost to raise a skill
//                by 10 skill points (ie one standard practice).
//
// MAX_SPENDING_LIMIT is the maximum XP amount we allow the player to
//                    spend on a skill in a single raise.
//
// Note that they don't have to be equal, but it is important to make
// sure that they're set so that the spending limit will always allow
// for 1 skill point to be earned.
#define MAX_COST_LIMIT           265
#define MAX_SPENDING_LIMIT       265

static int _train(skill_type exsk, int &max_exp,
                  bool simu = false, bool check_targets = true);
static void _train_skills(int exp, const int cost, const bool simu);
static int _training_target_skill_point_diff(skill_type exsk, int training_target);

// Basic goals for titles:
// The higher titles must come last.
// Referring to the skill itself is fine ("Transmuter") but not impressive.
// No overlaps, high diversity.

// See the map "replacements" below for what @Genus@, @Adj@, etc. do.
// NOTE: Even though @foo@ could be used with most of these, remember that
// the character's race will be listed on the next line. It's only really
// intended for cases where things might be really awkward without it. -- bwr

// NOTE: If a skill name is changed, remember to also adapt the database entry.
static const char *skill_titles[NUM_SKILLS][7] =
{
  //  Skill name        levels 1-7       levels 8-14        levels 15-20       levels 21-26      level 27       skill abbr
    {"Fighting",       "Trooper",       "Fighter",         "Warrior",         "Slayer",         "Conqueror",    "Fgt"},
    {"Short Blades",   "Cutter",        "Slicer",          "Swashbuckler",    "Cutthroat",      "Politician",   "SBl"},
    {"Long Blades",    "Slasher",       "Carver",          "Fencer",          "@Adj@ Blade",    "Swordmaster",  "LBl"},
    {"Axes",           "Chopper",       "Cleaver",         "Severer",         "Executioner",    "Axe Maniac",   "Axs"},
    {"Maces & Flails", "Cudgeller",     "Basher",          "Bludgeoner",      "Shatterer",      "Skullcrusher", "M&F"},
    {"Polearms",       "Poker",         "Spear-Bearer",    "Impaler",         "Phalangite",     "@Adj@ Porcupine", "Pla"},
    {"Staves",         "Twirler",       "Cruncher",        "Stickfighter",    "Pulveriser",     "Chief of Staff", "Stv"},
#if TAG_MAJOR_VERSION == 34
    {"Slings",         "Vandal",        "Slinger",         "Whirler",         "Slingshot",      "@Adj@ Catapult", "Slg"},
#endif
    {"Ranged Weapons", "Shooter",       "Skirmisher",      "Marks@genus@",    "Crack Shot",     "Merry @Genus@",  "Rng"},
#if TAG_MAJOR_VERSION == 34
    {"Crossbows",      "Bolt Thrower",  "Quickloader",     "Sharpshooter",    "Sniper",         "@Adj@ Arbalest", "Crb"},
#endif
    {"Throwing",       "Chucker",       "Thrower",         "Deadly Accurate", "Hawkeye",        "@Adj@ Ballista", "Thr"},
    {"Armour",         "Covered",       "Protected",       "Tortoise",        "Impregnable",    "Invulnerable", "Arm"},
    {"Dodging",        "Ducker",        "Nimble",          "Spry",            "Acrobat",        "Intangible",   "Ddg"},
    {"Stealth",        "Sneak",         "Covert",          "Unseen",          "Imperceptible",  "Ninja",        "Sth"},
#if TAG_MAJOR_VERSION == 34
    {"Stabbing",       "Miscreant",     "Blackguard",      "Backstabber",     "Cutthroat",      "Politician",   "Stb"},
#endif
    {"Shields",        "Shield-Bearer", "Blocker",         "Peltast",         "Hoplite",        "@Adj@ Barricade", "Shd"},
#if TAG_MAJOR_VERSION == 34
    {"Traps",          "Scout",         "Disarmer",        "Vigilant",        "Perceptive",     "Dungeon Master", "Trp"},
#endif
    // STR based fighters, for DEX/martial arts titles see below. Felids get their own category, too.
    {"Unarmed Combat", "Ruffian",       "Grappler",        "Brawler",         "Wrestler",       "@Weight@weight Champion", "UC"},

    {"Spellcasting",   "Magician",      "Thaumaturge",     "Eclecticist",     "Sorcerer",       "Archmage",     "Spc"},
    {"Conjurations",   "Conjurer",      "Destroyer",       "Devastator",      "Ruinous",        "Annihilator",  "Conj"},
    {"Hexes",          "Vexing",        "Jinx",            "Bewitcher",       "Maledictor",     "Spellbinder",  "Hex"},
#if TAG_MAJOR_VERSION == 34
    {"Charms",         "Charmwright",   "Infuser",         "Anointer",        "Gracecrafter",   "Miracle Worker", "Chrm"},
#endif
    {"Summonings",     "Caller",        "Summoner",        "Convoker",        "Worldbinder",    "Planerender",  "Summ"},
    {"Necromancy",     "Grave Robber",  "Reanimator",      "Necromancer",     "Thanatomancer",  "@Genus_Short@ of Death", "Necr"},
    {"Translocations", "Grasshopper",   "Placeless @Genus@", "Blinker",       "Portalist",      "Plane @Walker@", "Tloc"},
    {"Forgecraft",     "Tinkerer",      "Fabricator",      "Mechanist",       "Siegecrafter",   "Architect of Ages", "Frge"},

    {"Fire Magic",     "Firebug",       "Arsonist",        "Scorcher",        "Pyromancer",     "Infernalist",  "Fire"},
    {"Ice Magic",      "Chiller",       "Frost Mage",      "Gelid",           "Cryomancer",     "Englaciator",  "Ice"},
    {"Air Magic",      "Gusty",         "Zephyrmancer",    "Stormcaller",     "Cloud Mage",     "Meteorologist", "Air"},
    {"Earth Magic",    "Digger",        "Geomancer",       "Earth Mage",      "Metallomancer",  "Petrodigitator", "Erth"},
    {"Alchemy",        "Apothecary",    "Toxicologist",    "Hermetic",        "Philosopher",    "Quintessent", "Alch"},

    // These titles apply to atheists only, worshippers of the various gods
    // use the god titles instead, depending on piety or, in Gozag's case, gold.
    // or, in U's case, invocations skill.
    {"Invocations",    "Unbeliever",    "Agnostic",        "Dissident",       "Heretic",        "Apostate",     "Invo"},
    {"Evocations",     "Charlatan",     "Prestidigitator", "Fetichist",       "Evocator",       "Ex Machina",  "Evo"},
    {"Shapeshifting",  "Changeling",    "Mimic",           "Metamorph",       "Skinwalker",     "Shapeless @Genus@", "Shft"},
};

static const char *martial_arts_titles[6] =
    {"Unarmed Combat", "Insei", "Martial Artist", "Black Belt", "Sensei", "Grand Master"};
static const char *claw_and_tooth_titles[6] =
    {"Unarmed Combat", "Scratcher", "Gouger", "Ripper", "Eviscerator", "Sabretooth"};

struct species_skill_aptitude
{
    species_type species;
    skill_type   skill;
    int aptitude;          // -50..50, with 0 for humans

    species_skill_aptitude(species_type _species,
                           skill_type _skill,
                           int _aptitude)
        : species(_species), skill(_skill), aptitude(_aptitude)
    {
    }
};

#include "aptitudes.h"

// Traditionally, Spellcasting and In/Evocations formed the exceptions here:
// Spellcasting skill was more expensive with about 130%, the other two got
// a discount with about 75%.
static int _spec_skills[NUM_SPECIES][NUM_SKILLS];

// The progress of skill_cost_level depends only on total experience points,
// it's independent of species. We try to keep close to the old system
// and use an experience aptitude of 130 as a reference (Tengu).
// This means that for a species with 130 exp apt, skill_cost_level should be
// the same as XL (unless the player has been drained).

// 130 exp apt is midway between +0 and -1 now. -- elliptic
unsigned int skill_cost_needed(int level)
{
    return exp_needed(level, 1) * 13;
}

static const int MAX_SKILL_COST_LEVEL = 27;

// skill_cost_level makes skills more expensive for more experienced characters
int calc_skill_cost(int skill_cost_level)
{
    const int cost[] = { 1, 2, 3, 4, 5,            // 1-5
                         7, 8, 9, 13, 22,         // 6-10
                         37, 48, 73, 98, 125,      // 11-15
                         145, 170, 190, 212, 225,  // 16-20
                         240, 255, 260, 265, 265,  // 21-25
                         265, 265 };
    COMPILE_CHECK(ARRAYSZ(cost) == MAX_SKILL_COST_LEVEL);

    ASSERT_RANGE(skill_cost_level, 1, MAX_SKILL_COST_LEVEL + 1);
    return cost[skill_cost_level - 1];
}

/**
 * The baseline skill cost for the 'cost' interface on the m screen.
 *
 * @returns the XP needed to go from level 0 to level 1 with +0 apt.
 */
int skill_cost_baseline()
{
    return skill_exp_needed(1, SK_FIGHTING, SP_HUMAN)
           - skill_exp_needed(0, SK_FIGHTING, SP_HUMAN);
}

/**
 * The skill cost to increase the given skill from its current level by one.
 *
 * @param sk the skill to check the player's level of
 * @returns the XP needed to increase from floor(level) to ceiling(level)
 */
int one_level_cost(skill_type sk)
{
    if (you.skills[sk] >= MAX_SKILL_LEVEL)
        return 0;
    return skill_exp_needed(you.skills[sk] + 1, sk)
           - skill_exp_needed(you.skills[sk], sk);
}

/**
 * The number displayed in the 'cost' interface on the m screen.
 *
 * @param sk the skill to compute the cost of
 * @returns the cost of raising sk from floor(level) to ceiling(level),
 *          as a multiple of skill_cost_baseline()
 */
float scaled_skill_cost(skill_type sk)
{
    if (you.skills[sk] == MAX_SKILL_LEVEL || is_useless_skill(sk))
        return 0;
    int baseline = skill_cost_baseline();
    int next_level = one_level_cost(sk);
    if (you.skill_manual_points[sk])
        baseline *= 2;

    return (float)next_level / baseline;
}

/// Ensure that all magic skills are at the same level, e.g. at game start or for save compat.
void cleanup_innate_magic_skills()
{
    unsigned int magic_xp = 0;
    unsigned int n_skills = 0;
    for (skill_type sk = SK_SPELLCASTING; sk <= SK_LAST_MAGIC; sk++)
    {
        if (is_useless_skill(sk))
            continue;
        magic_xp += you.skill_points[sk];
        ++n_skills;
    }
    // Lossy, sorry.
    const unsigned int xp_per = magic_xp / n_skills;

    int lvl = 0;
    while (xp_per > skill_exp_needed(lvl + 1, SK_SPELLCASTING))
        ++lvl;

    for (skill_type sk = SK_SPELLCASTING; sk <= SK_LAST_MAGIC; sk++)
    {
        if (is_useless_skill(sk))
            continue;
        you.skill_points[sk] = xp_per;
        you.skills[sk] = lvl;
    }
}

// Characters are actually granted skill points, not skill levels.
// Here we take racial aptitudes into account in determining final
// skill levels.
void reassess_starting_skills()
{
    // go backwards, need to do Dodging before Armour
    // "sk >= SK_FIRST_SKILL" might be optimised away, so do this differently.
    for (skill_type next = NUM_SKILLS; next > SK_FIRST_SKILL; )
    {
        skill_type sk = --next;
        ASSERT(you.skills[sk] == 0 || !is_useless_skill(sk));

        // Grant the amount of skill points required for a human.
        you.skill_points[sk] = you.skills[sk] ?
            skill_exp_needed(you.skills[sk], sk, SP_HUMAN) + 1 : 0;

        item_def* current_armour = you.slot_item(EQ_BODY_ARMOUR);

        // No one who can't wear mundane heavy armour should start with
        // the Armour skill -- D:1 dragon armour is too unlikely.
        // However, specifically except the special case of Sp/Tr/On
        // wanderers starting with acid dragon scales.
        if (sk == SK_DODGING && you.skills[SK_ARMOUR]
            && (is_useless_skill(SK_ARMOUR)
                || you_can_wear(EQ_BODY_ARMOUR) != true)
            && !(current_armour
                 && current_armour->sub_type == ARM_ACID_DRAGON_ARMOUR))
        {
            you.skill_points[sk] += skill_exp_needed(you.skills[SK_ARMOUR],
                SK_ARMOUR, SP_HUMAN) + 1;
            you.skills[SK_ARMOUR] = 0;
        }

        if (!you.skill_points[sk])
            continue;

        // Find out what level that earns this character.
        you.skills[sk] = 0;

        for (int lvl = 1; lvl <= 8; ++lvl)
        {
            if (you.skill_points[sk] > skill_exp_needed(lvl, sk))
                you.skills[sk] = lvl;
            else
                break;
        }

        // Wanderers get at least 1 level in their skills.
        if (you.char_class == JOB_WANDERER && you.skills[sk] < 1)
        {
            you.skill_points[sk] = skill_exp_needed(1, sk);
            you.skills[sk] = 1;
        }

        // Spellcasters should always have Spellcasting skill.
        if (sk == SK_SPELLCASTING && you.skills[sk] < 1)
        {
            you.skill_points[sk] = skill_exp_needed(1, sk);
            you.skills[sk] = 1;
        }
    }

    if (you.has_mutation(MUT_INNATE_CASTER))
        cleanup_innate_magic_skills();
}

static void _change_skill_level(skill_type exsk, int n)
{
    ASSERT(n != 0);
    bool need_reset = false;

    you.skills[exsk] = max(0, you.skills[exsk] + n);

    take_note(Note(n > 0 ? NOTE_GAIN_SKILL : NOTE_LOSE_SKILL,
                   exsk, you.skills[exsk]));

    // are you drained/crosstrained/ash'd in the relevant skill?
    const bool specify_base = you.skill(exsk, 1) != you.skill(exsk, 1, true);
    if (you.skills[exsk] == MAX_SKILL_LEVEL)
        mprf(MSGCH_INTRINSIC_GAIN, "You have mastered %s!", skill_name(exsk));
    else if (abs(n) == 1 && you.num_turns)
    {
        mprf(MSGCH_INTRINSIC_GAIN, "Your %s%s skill %s to level %d!",
             specify_base ? "base " : "",
             skill_name(exsk), (n > 0) ? "increases" : "decreases",
             you.skills[exsk]);
    }
    else if (you.num_turns)
    {
        mprf(MSGCH_INTRINSIC_GAIN, "Your %s%s skill %s %d levels and is now "
             "at level %d!",
             specify_base ? "base " : "",
             skill_name(exsk),
             (n > 0) ? "gained" : "lost",
             abs(n), you.skills[exsk]);
    }

    if (you.skills[exsk] == n && n > 0)
        hints_gained_new_skill(exsk);

    if (n > 0 && you.num_turns)
        learned_something_new(HINT_SKILL_RAISE);

    if (you.skills[exsk] - n == MAX_SKILL_LEVEL)
    {
        you.train[exsk] = TRAINING_ENABLED;
        need_reset = true;
    }

    if (exsk == SK_SPELLCASTING && you.skills[exsk] == n && n > 0)
        learned_something_new(HINT_GAINED_SPELLCASTING);

    if (need_reset)
        reset_training();

    // calc_hp() has to be called here because it currently doesn't work
    // right if you.skills[] hasn't been updated yet.
    if (exsk == SK_FIGHTING || exsk == SK_SHAPESHIFTING)
        calc_hp(true);
}

// Called whenever a skill is trained.
void redraw_skill(skill_type exsk, skill_type old_best_skill, bool recalculate_order)
{
    const bool trained_form = exsk == SK_SHAPESHIFTING && you.form != transformation::none;
    if (exsk == SK_FIGHTING || trained_form)
        calc_hp(true);

    if (exsk == SK_INVOCATIONS || exsk == SK_SPELLCASTING)
        calc_mp();

    if (exsk == SK_DODGING || exsk == SK_ARMOUR || trained_form)
        you.redraw_evasion = true;

    if (exsk == SK_ARMOUR || exsk == SK_SHIELDS || exsk == SK_ICE_MAGIC
        || exsk == SK_EARTH_MAGIC || trained_form)
    {
        you.redraw_armour_class = true;
    }

    if (recalculate_order)
    {
        // Recalculate this skill's order for tie breaking skills
        // at its new level.   See skills.cc::init_skill_order()
        // for more details.  -- bwr
        you.skill_order[exsk] = 0;
        for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
        {
            if (sk != exsk && you.skill(sk, 10, true) >= you.skill(exsk, 10, true))
                you.skill_order[exsk]++;
        }

        const skill_type best = best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
        if (best != old_best_skill || old_best_skill == exsk)
        {
            you.redraw_title = true;
            // The player symbol depends on best skill title.
            update_player_symbol();
        }
    }
}

int calc_skill_level_change(skill_type sk, int starting_level, int sk_points)
{
    int new_level = starting_level;
    while (1)
    {
        if (new_level < MAX_SKILL_LEVEL
            && sk_points >= (int) skill_exp_needed(new_level + 1, sk))
        {
            ++new_level;
        }
        else if (sk_points < (int) skill_exp_needed(new_level, sk))
        {
            new_level--;
            ASSERT(new_level >= 0);
        }
        else
            break;
    }
    return new_level;
}

void check_skill_level_change(skill_type sk, bool do_level_up)
{
    const int new_level = calc_skill_level_change(sk, you.skills[sk], you.skill_points[sk]);

    if (new_level != you.skills[sk])
    {
        if (do_level_up)
            _change_skill_level(sk, new_level - you.skills[sk]);
        else
            you.skills[sk] = new_level;
    }
}

// Fill a queue in random order with the values of the array.
template <typename T, int SIZE>
static void _init_queue(list<skill_type> &queue, FixedVector<T, SIZE> &array)
{
    ASSERT(queue.empty());

    while (1)
    {
        skill_type sk = (skill_type)random_choose_weighted(array);
        if (is_invalid_skill(sk))
            break;
        queue.push_back(sk);
        --array[sk];
    }

    ASSERT(queue.size() == (unsigned)EXERCISE_QUEUE_SIZE);
}

static void _erase_from_skills_to_hide(const skill_set &can_train)
{
    for (skill_type sk : can_train)
        you.skills_to_hide.erase(sk);
}

/*
 * Check the inventory to see what skills are likely to be useful
 * among the ones in you.skills_to_hide.
 * Useful skills are removed from the set.
 */
static void _check_inventory_skills()
{
    for (const auto &item : you.inv)
    {
        // Exit early if there's no more skill to check.
        if (you.skills_to_hide.empty())
            return;

        skill_set skills;
        if (!item.defined() || !item_skills(item, skills))
            continue;

        _erase_from_skills_to_hide(skills);
    }
}

static void _check_spell_skills()
{
    for (spell_type spell : you.spells)
    {
        // Exit early if there's no more skill to check.
        if (you.skills_to_hide.empty())
            return;

        if (spell == SPELL_NO_SPELL)
            continue;

        skill_set skills;
        spell_skills(spell, skills);
        _erase_from_skills_to_hide(skills);
    }
}

static void _check_abil_skills()
{
    for (ability_type abil : get_god_abilities())
    {
        // Exit early if there's no more skill to check.
        if (you.skills_to_hide.empty())
            return;

        you.skills_to_hide.erase(abil_skill(abil));
    }
}

static void _check_active_talisman_skills()
{
    skill_set skills;
    if (you.active_talisman.defined()
        && item_skills(you.active_talisman, skills))
    {
        _erase_from_skills_to_hide(skills);
    }
}

/// Check to see if the player is a djinn with at least one magic skill
/// un-hidden. If so, unhide all of them.
static void _check_innate_magic_skills()
{
    if (!you.has_mutation(MUT_INNATE_CASTER))
        return;

    bool any_magic = false;
    for (skill_type sk = SK_SPELLCASTING; sk <= SK_LAST_MAGIC; ++sk)
        if (!is_removed_skill(sk) && !you.skills_to_hide.count(sk))
            any_magic = true;
    if (!any_magic)
        return;

    for (skill_type sk = SK_SPELLCASTING; sk <= SK_LAST_MAGIC; ++sk)
        you.skills_to_hide.erase(sk);
}

string skill_names(const skill_set &skills)
{
    return comma_separated_fn(begin(skills), end(skills), skill_name);
}

static void _check_skills_to_show()
{
    for (skill_type sk : you.skills_to_show)
    {
        if (is_invalid_skill(sk) || is_useless_skill(sk))
            continue;

        you.should_show_skill.set(sk);
    }

    reset_training();
    you.skills_to_show.clear();
}

static void _check_skills_to_hide()
{
    // Gnolls can't stop training skills.
    if (you.has_mutation(MUT_DISTRIBUTED_TRAINING))
        return;

    _check_inventory_skills();
    _check_spell_skills();
    _check_abil_skills();
    _check_active_talisman_skills();
    _check_innate_magic_skills();

    if (you.skills_to_hide.empty())
        return;

    skill_set skills;
    for (skill_type sk : you.skills_to_hide)
    {
        if (is_invalid_skill(sk))
            continue;
        if (you.skill_manual_points[sk])
            continue;

        if (skill_trained(sk) && you.training[sk])
            skills.insert(sk);
        you.should_show_skill.set(sk, false);
    }

    reset_training();
    you.skills_to_hide.clear();
}

void update_can_currently_train()
{
    if (!you.skills_to_show.empty())
        _check_skills_to_show();

    if (!you.skills_to_hide.empty())
        _check_skills_to_hide();
}

bool skill_default_shown(skill_type sk)
{
    if (you.has_mutation(MUT_DISTRIBUTED_TRAINING))
        return true;

    switch (sk)
    {
    case SK_FIGHTING:
    case SK_ARMOUR:
    case SK_DODGING:
    case SK_STEALTH:
    case SK_UNARMED_COMBAT:
    case SK_SPELLCASTING:
        return !is_harmful_skill(sk);
    default:
        return false;
    }
}

/*
 * Init the can_currently_train array by examining inventory and spell list to
 * see which skills can be trained.
 */
void init_can_currently_train()
{
    // Clear everything out, in case this isn't the first game.
    you.skills_to_show.clear();
    you.skills_to_hide.clear();
    you.can_currently_train.reset();

    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        const skill_type sk = skill_type(i);

        if (is_useless_skill(sk))
            continue;

        you.can_currently_train.set(sk);
        you.should_show_skill.set(sk);
        if (!skill_default_shown(sk))
            you.skills_to_hide.insert(sk);
    }

    _check_skills_to_hide();
}

void init_train()
{
    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        if (you.can_currently_train[i] && you.skill_points[i])
            you.train[i] = you.train_alt[i] = TRAINING_ENABLED;
        else
        {
            const bool gnoll_enable = you.has_mutation(MUT_DISTRIBUTED_TRAINING)
                                        && !is_removed_skill((skill_type) i);
            // Skills are on by default in auto mode and off in manual.
            you.train[i] = (training_status) (gnoll_enable
                                                || you.auto_training);
            you.train_alt[i] =
                (training_status) (gnoll_enable || !you.auto_training);
        }
    }
}

static bool _cmp_rest(const pair<skill_type, int64_t>& a,
                      const pair<skill_type, int64_t>& b)
{
    return a.second < b.second;
}

/**
 * Scale an array.
 *
 * @param array The array to be scaled.
 * @param scale The new scale of the array.
 * @param exact When true, make sure that the sum of the array elements
 *              is equal to the scale.
 */
template <typename T, int SIZE>
static void _scale_array(FixedVector<T, SIZE> &array, int scale, bool exact)
{
    int64_t total = 0;
    // First, we calculate the sum of the values to be scaled.
    for (int i = 0; i < NUM_SKILLS; ++i)
        total += array[i];

    vector<pair<skill_type, int64_t> > rests;
    int scaled_total = 0;

    // All skills disabled, nothing to do.
    if (!total)
        return;

    // Now we scale the values.
    for (int i = 0; i < NUM_SKILLS; ++i)
        if (array[i] > 0)
        {
            int64_t result = (int64_t)array[i] * (int64_t)scale;
            const int64_t rest = result % total;
            if (rest)
                rests.emplace_back(skill_type(i), rest);
            array[i] = (int)(result / total);
            scaled_total += array[i];
        }

    ASSERT(scaled_total <= scale);

    if (!exact || scaled_total == scale)
        return;

    // We ensure that the percentage always add up to 100 by increasing the
    // training for skills which had the higher rest from the above scaling.
    sort(rests.begin(), rests.end(), _cmp_rest);
    for (auto &rest : rests)
    {
        if (scaled_total >= scale)
            break;

        ++array[rest.first];
        ++scaled_total;
    }

    ASSERT(scaled_total == scale);
}

static int _calc_skill_cost_level(int xp, int start)
{
    while (start < MAX_SKILL_COST_LEVEL
           && xp >= (int) skill_cost_needed(start + 1))
    {
        ++start;
    }
    while (start > 0
           && xp < (int) skill_cost_needed(start))
    {
        --start;
    }
    return start;
}

/*
 * Init the training array by scaling down the skill_points array to 100.
 * Used at game setup, when upgrading saves and when loading dump files.
 */
void init_training()
{
    FixedVector<unsigned int, NUM_SKILLS> skills;
    skills.init(0);
    for (int i = 0; i < NUM_SKILLS; ++i)
        if (skill_trained(i))
            skills[i] = sqr(you.skill_points[i]);

    _scale_array(skills, EXERCISE_QUEUE_SIZE, true);
    _init_queue(you.exercises, skills);

    for (int i = 0; i < NUM_SKILLS; ++i)
        skills[i] = sqr(you.skill_points[i]);

    _scale_array(skills, EXERCISE_QUEUE_SIZE, true);
    _init_queue(you.exercises_all, skills);

    reset_training();
}

bool skills_being_trained()
{
    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        skill_type sk = static_cast<skill_type>(i);
        if (skill_trained(sk))
            return true;
    }
    return false;
}

// Make sure at least one skill is selected.
// If not, go to the skill menu and return true.
bool check_selected_skills()
{
    if (skills_being_trained())
        return false;
    if (!trainable_skills())
    {
        if (!you.received_noskill_warning)
        {
            you.received_noskill_warning = true;
            mpr("You cannot train any new skills!");
        }
        // It's possible to have no selectable skills, if they are all
        // untrainable or level 27, so we don't assert.
        return false;
    }

    // Calling a user lua function here to allow enabling skills without user
    // prompt (much like the callback auto_experience for the case of potion of
    // experience).
    if (clua.callbooleanfn(false, "skill_training_needed", nullptr))
    {
        // did the callback do anything?
        if (skills_being_trained())
            return true;
    }

    if (crawl_state.seen_hups)
    {
        save_game(true, "Game saved, see you later!");
        return false;
    }

    mpr("You need to enable at least one skill for training.");
    // Training will be fixed up on load if this ASSERT triggers.
    ASSERT(!you.has_mutation(MUT_DISTRIBUTED_TRAINING));
    more();
    reset_training();
    skill_menu();
    redraw_screen();
    update_screen();
    return true;
}

/// Set all magic skills to the same training value.
static void _balance_magic_training()
{
    // To minimize int rounding issues with manual training,
    // scale up skill training numbers here. We'll scale em back down later.
    for (int i = 0; i < NUM_SKILLS; ++i)
        you.training[i] *= 100;

    int n_skills = 0;
    int train_total = 0;
    for (skill_type sk = SK_SPELLCASTING; sk <= SK_LAST_MAGIC; ++sk)
    {
        if (is_removed_skill(sk) || you.skills[sk] >= MAX_SKILL_LEVEL)
        {
            you.training[sk] = 0;
            continue;
        }
        n_skills++;
        train_total += you.training[sk];
    }
    if (!train_total)
        return;

    ASSERT(n_skills > 0);
    // Total training for all magic skills should be the base average,
    // divided between each skill.
    const int to_train = max(train_total / (n_skills * n_skills), 1);
    for (skill_type sk = SK_SPELLCASTING; sk <= SK_LAST_MAGIC; ++sk)
        if (!is_removed_skill(sk) && you.skills[sk] < MAX_SKILL_LEVEL)
            you.training[sk] = to_train;
}

/**
 * Reset the training array. Disabled skills are skipped.
 * In automatic mode, we use values from the exercise queue.
 * In manual mode, all enabled skills are set to the same value.
 * Result is scaled back to 100.
 */
void reset_training()
{
    // Disable this here since we don't want any autotraining related skilling
    // changes for Gnolls.
    if (you.has_mutation(MUT_DISTRIBUTED_TRAINING))
        you.auto_training = false;

    // We clear the values in the training array. In auto mode they are set
    // to 0 (and filled later with the content of the queue), in manual mode,
    // the trainable ones are set to 1 (or 2 for focus).
    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        // skill_trained doesn't work for gnolls, but all existent skills
        // will be set as enabled here.
        if (!you.has_mutation(MUT_DISTRIBUTED_TRAINING)
            && (you.auto_training || !skill_trained(i)))
        {
            you.training[i] = 0;
        }
        else
            you.training[i] = you.train[i];
    }

    bool empty = true;
    // In automatic mode, we fill the array with the content of the queue.
    if (you.auto_training)
    {
        for (auto sk : you.exercises)
        {
            if (skill_trained(sk))
            {
                you.training[sk] += you.train[sk];
                empty = false;
            }
        }

        // We count the practise events in the other queue.
        FixedVector<unsigned int, NUM_SKILLS> exer_all;
        exer_all.init(0);
        for (auto sk : you.exercises_all)
        {
            if (skill_trained(sk))
            {
                exer_all[sk] += you.train[sk];
                empty = false;
            }
        }

        // We keep the highest of the 2 numbers.
        for (int sk = 0; sk < NUM_SKILLS; ++sk)
            you.training[sk] = max(you.training[sk], exer_all[sk]);

        // The selected skills have not been exercised recently. Give them all
        // a default weight of 1 (or 2 for focus skills).
        if (empty)
        {
            for (int sk = 0; sk < NUM_SKILLS; ++sk)
                if (skill_trained(sk))
                    you.training[sk] = you.train[sk];
        }

        // Focused skills get at least 20% training.
        for (int sk = 0; sk < NUM_SKILLS; ++sk)
            if (you.train[sk] == 2 && you.training[sk] < 20
                && you.can_currently_train[sk])
            {
                you.training[sk] += 5 * (5 - you.training[sk] / 4);
            }
    }

    if (you.has_mutation(MUT_INNATE_CASTER))
        _balance_magic_training();

    _scale_array(you.training, 100, !you.has_mutation(MUT_INNATE_CASTER));
    if (you.has_mutation(MUT_DISTRIBUTED_TRAINING)
        || you.has_mutation(MUT_INNATE_CASTER))
    {
        // we use the full set of skills to calculate gnoll/dj percentages,
        // but they don't actually get to train sacrificed skills.
        for (int i = 0; i < NUM_SKILLS; ++i)
            if (is_useless_skill((skill_type) i))
                you.training[i] = 0;
    }
}

void exercise(skill_type exsk, int deg)
{
    if (you.skills[exsk] >= MAX_SKILL_LEVEL)
        return;

    dprf(DIAG_SKILLS, "Exercise %s by %d.", skill_name(exsk), deg);

    // push first in case queues are empty, like during -test
    while (deg > 0)
    {
        if (skill_trained(exsk))
        {
            you.exercises.push_back(exsk);
            you.exercises.pop_front();
        }
        you.exercises_all.push_back(exsk);
        you.exercises_all.pop_front();
        deg--;
    }
    reset_training();
}

// Check if we should stop training this skill immediately.
// We look at skill points because actual level up comes later.
static bool _level_up_check(skill_type sk, bool simu)
{
    // Don't train past level 27.
    if (you.skill_points[sk] >= skill_exp_needed(MAX_SKILL_LEVEL, sk))
    {
        you.training[sk] = 0;
        if (!simu)
        {
            you.train[sk] = TRAINING_DISABLED;
            you.train_alt[sk] = TRAINING_DISABLED;
        }
        return true;
    }

    return false;
}

bool is_magic_skill(skill_type sk)
{
    return sk > SK_LAST_MUNDANE && sk <= SK_LAST_MAGIC;
}

int _gnoll_total_skill_cost();

static int _magic_training()
{
    for (skill_type sk = SK_SPELLCASTING; sk <= SK_LAST_MAGIC; ++sk)
        if (you.training[sk])
            return you.training[sk];
    return 0;
}

static const int NO_TRAINING = 10000;

/// What is the skill you are currently training at the lowest non-zero percent?
static unsigned int _min_training_level()
{
    unsigned int min = NO_TRAINING;
    for (skill_type i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
        if (you.training[i] && you.training[i] < min)
            min = you.training[i];
    return min;
}

static bool _is_sacrificed_skill(skill_type skill);

/// If you want to raise each currently-trained skill by an amount
/// proportionate to their training %s, how many points do you need total?
static int _min_points_to_raise_all(int min_training)
{
    int points = 0;
    for (skill_type i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
        if (you.training[i])
            points += you.training[i] / min_training;

    // If the player sacrificed a magic skill, if we're training any magic,
    // waste skill point(s) on that skill.
    const int magic_training = _magic_training();
    if (magic_training)
        for (skill_type sk = SK_SPELLCASTING; sk <= SK_LAST_MAGIC; ++sk)
            if (_is_sacrificed_skill(sk))
                points += magic_training / min_training;

    return points;
}

/// Is your current `exp_available` enough to raise all of your skills
/// proportionately to you.training?
static bool _xp_available_for_skill_points(int points)
{
    int cost_level = you.skill_cost_level;
    int cost = calc_skill_cost(cost_level);
    int xp_needed = 0;
    // XXX: could do this more efficiently
    for (int i = 0; i < points; ++i)
    {
        xp_needed += cost;
        if (xp_needed > you.exp_available)
            return false;

        const int total_xp = you.total_experience + xp_needed;
        const int new_level = _calc_skill_cost_level(total_xp, cost_level);
        if (new_level != cost_level)
        {
            cost_level = new_level;
            cost = calc_skill_cost(cost_level);
        }
    }
    return true;
}

/**
 * Train Djinn skills such that the same amount of experience is put into all
 * spellcasting skills. This means that we need to make sure we have enough
 * xp to level *all* selected skills proportionate to you.training before we put
 * xp into *any* skill.
 */
static void _train_with_innate_casting(bool simu)
{
    while (true) {
        const int min = _min_training_level();
        if (min == NO_TRAINING) // no skills set to train
            return;

        const int points = _min_points_to_raise_all(min);
        if (!_xp_available_for_skill_points(points))
            break;

        // OK, we should be able to train everything.
        for (int i = 0; i < NUM_SKILLS; ++i)
        {
            if (!you.training[i])
                continue;
            const int p = you.training[i] / min;
            int xp = calc_skill_cost(you.skill_cost_level) * p;
            // We don't want to disable training for magic skills midway.
            // Finish training all skills and check targets afterward.
            const auto sk = static_cast<skill_type>(i);
            _train(sk, xp, simu, false);
            _level_up_check(sk, simu);
        }

        // If the player sacrificed a magic skill, if we're training any magic,
        // waste skill point(s) on that skill.
        const int magic_training = _magic_training();
        if (magic_training)
        {
            for (skill_type sk = SK_SPELLCASTING; sk <= SK_LAST_MAGIC; ++sk)
            {
                if (!_is_sacrificed_skill(sk))
                    continue;
                const int p = magic_training / min;
                const int xp = calc_skill_cost(you.skill_cost_level) * p;
                you.exp_available -= xp;
                you.total_experience += xp;
            }
        }

        check_training_targets();
    }
}

void train_skills(bool simu)
{
    int cost, exp;
    if (you.has_mutation(MUT_DISTRIBUTED_TRAINING))
    {
        do
        {
            exp = you.exp_available;
            cost = _gnoll_total_skill_cost();
            if (exp >= cost)
            {
                _train_skills(exp, calc_skill_cost(you.skill_cost_level), simu);
                dprf(DIAG_SKILLS,
                    "Trained all gnoll skills by 1 at total cost %d.", cost);
            }
        }
        while (exp != you.exp_available);
    }
    else if (you.has_mutation(MUT_INNATE_CASTER))
        _train_with_innate_casting(simu);
    else
    {
        do
        {
            cost = calc_skill_cost(you.skill_cost_level);
            exp = you.exp_available;
            if (you.skill_cost_level == MAX_SKILL_COST_LEVEL)
                _train_skills(exp, cost, simu);
            else
            {
                // Amount of experience points needed to reach the next skill cost level
                const int next_level = skill_cost_needed(you.skill_cost_level + 1)
                                       - you.total_experience;
                ASSERT(next_level > 0);
                _train_skills(min(exp, next_level + cost - 1), cost, simu);
            }
        }
        while (you.exp_available >= cost && exp != you.exp_available);
    }

    for (int i = 0; i < NUM_SKILLS; ++i)
        check_skill_level_change(static_cast<skill_type>(i), !simu);

    // We might have disabled some skills on level up.
    reset_training();
}

//#define DEBUG_TRAINING_COST
static void _train_skills(int exp, const int cost, const bool simu)
{
    bool skip_first_phase = false;
    int magic_gain = 0;
    FixedVector<int, NUM_SKILLS> sk_exp;
    sk_exp.init(0);
    vector<skill_type> training_order;
#ifdef DEBUG_DIAGNOSTICS
    FixedVector<int, NUM_SKILLS> total_gain;
    total_gain.init(0);
#endif
#ifdef DEBUG_TRAINING_COST
    int exp_pool = you.exp_available;
    dprf(DIAG_SKILLS,
         "skill cost level: %d, cost: %dxp/skp, max XP usable: %d.",
         you.skill_cost_level, cost, exp);
#endif

    // pre-check training targets -- may disable some skills.
    if (!simu)
        check_training_targets();

    // We scale the training array to the amount of XP available in the pool.
    // That gives us the amount of XP available to train each skill.
    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        if (you.training[i] > 0)
        {
            sk_exp[i] = you.training[i] * exp / 100;
            if (sk_exp[i] < cost && !you.has_mutation(MUT_DISTRIBUTED_TRAINING))
            {
                // One skill has a too low training to be trained at all.
                // We skip the first phase and go directly to the random
                // phase so it has a chance to be trained.
                skip_first_phase = true;
                break;
            }
            training_order.push_back(static_cast<skill_type>(i));
        }
    }

    if (!skip_first_phase)
    {
        // We randomize the order, to avoid a slight bias to first skills.
        // Being trained first can make a difference if skill cost increases.
        if (you.has_mutation(MUT_DISTRIBUTED_TRAINING))
            reverse(training_order.begin(), training_order.end());
        else
            shuffle_array(training_order);
        for (auto sk : training_order)
        {
            int gain = 0;

            if (you.has_mutation(MUT_DISTRIBUTED_TRAINING))
                sk_exp[sk] = exp;
            while (sk_exp[sk] >= cost && you.training[sk])
            {
                exp -= sk_exp[sk];
                gain += _train(sk, sk_exp[sk], simu);
                exp += sk_exp[sk];
                ASSERT(exp >= 0);
                if (_level_up_check(sk, simu))
                    sk_exp[sk] = 0;
                if (you.has_mutation(MUT_DISTRIBUTED_TRAINING))
                    break;
            }

            if (gain && is_magic_skill(sk))
                magic_gain += gain;

#ifdef DEBUG_DIAGNOSTICS
           total_gain[sk] += gain;
#endif
        }
    }
    // If there's enough xp in the pool, we use it to train skills selected
    // with random_choose_weighted.
    while (exp >= cost)
    {
        if (you.has_mutation(MUT_DISTRIBUTED_TRAINING))
            break;
        int gain;
        skill_type sk = SK_NONE;
        if (!skip_first_phase)
            sk = static_cast<skill_type>(random_choose_weighted(sk_exp));
        if (is_invalid_skill(sk) || !you.train[sk])
            sk = static_cast<skill_type>(random_choose_weighted(you.training));
        if (!is_invalid_skill(sk))
        {
            gain = _train(sk, exp, simu);
            ASSERT(exp >= 0);
            sk_exp[sk] = 0;
        }
        else
        {
            // No skill to train. Can happen if all skills are at 27.
            break;
        }

        _level_up_check(sk, simu);

        if (gain && is_magic_skill(sk))
            magic_gain += gain;

#ifdef DEBUG_DIAGNOSTICS
        total_gain[sk] += gain;
#endif
    }

    // clean up any cross-training effects
    if (!simu)
        check_training_targets();

#ifdef DEBUG_DIAGNOSTICS
    if (!crawl_state.script)
    {
#ifdef DEBUG_TRAINING_COST
        int total = 0;
#endif
        for (int i = 0; i < NUM_SKILLS; ++i)
        {
            skill_type sk = static_cast<skill_type>(i);
            if (total_gain[sk] && !simu
                && !you.has_mutation(MUT_DISTRIBUTED_TRAINING))
            {
                dprf(DIAG_SKILLS, "Trained %s by %d.",
                     skill_name(sk), total_gain[sk]);
            }
#ifdef DEBUG_TRAINING_COST
            total += total_gain[sk];
        }
        dprf(DIAG_SKILLS, "Total skill points gained: %d, cost: %d XP.",
             total, exp_pool - you.exp_available);
#else
        }
#endif
    }
#endif

    // Avoid doubly rewarding spell practise in sprint
    // (by inflated XP and inflated piety gain)
    if (crawl_state.game_is_sprint())
        magic_gain = sprint_modify_exp_inverse(magic_gain);

    if (magic_gain && !simu)
        did_god_conduct(DID_SPELL_PRACTISE, div_rand_round(magic_gain, 10));
}

bool skill_trained(int i)
{
    return you.can_currently_train[i] && you.train[i];
}

/**
 * Is the training target, if any, met or exceeded for skill sk?
 *
 * @param sk the skill to check. This checks crosstraining and ash bonuses,
 * but not other skill modifiers.
 * @param target the target to check against. Defaults to you.training_targets[sk]
 *
 * @return whether the skill target has been met.
 */
bool target_met(skill_type sk, unsigned int target)
{
    return you.skill(sk, 10, false, false) >= (int) target;
}

bool target_met(skill_type sk)
{
    return target_met(sk, you.training_targets[sk]);
}

/**
 * Check the training target (if any) for skill sk, and change state
 * appropriately. If the target has been met or exceeded, this will turn off
 * targeting for that skill, and stop training it. This does *not* reset the
 * training percentages, though, so if it's used mid-training, you need to take
 * care of that.
 *
 * @param sk the skill to check.
 * @return whether a target was reached.
 */
bool check_training_target(skill_type sk)
{
    if (you.training_targets[sk] && target_met(sk))
    {
        bool base = you.skill(sk, 10, false, false) != you.skill(sk, 10);
        auto targ = you.training_targets[sk];
        you.training_targets[sk] = 0;
        if (you.has_mutation(MUT_INNATE_CASTER) && is_magic_skill(sk))
            set_magic_training(TRAINING_DISABLED);
        else
            you.train_alt[sk] = you.train[sk] = TRAINING_DISABLED;
        mprf("%sraining target %d.%d for %s reached!",
            base ? "Base t" : "T", targ / 10, targ % 10, skill_name(sk));
        return true;
    }
    return false;
}

/**
 * Check the training target (if any) for all skills, and change state
 * appropriately.
 *
 * @return whether any target was reached.
 *
 * @see check_training_target
 */
bool check_training_targets()
{
    bool change = false;
    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
        change |= check_training_target(sk);
    if (change)
        reset_training();
    return change;
}

void check_skill_cost_change()
{
#ifdef DEBUG_TRAINING_COST
    int initial_cost = you.skill_cost_level;
#endif

    you.skill_cost_level = _calc_skill_cost_level(you.total_experience, you.skill_cost_level);

#ifdef DEBUG_TRAINING_COST
    if (initial_cost != you.skill_cost_level)
        dprf("Adjusting skill cost level to %d", you.skill_cost_level);
#endif
}

static int _useless_skill_count()
{
    int count = 0;
    for (skill_type skill = SK_FIRST_SKILL; skill < NUM_SKILLS; ++skill)
    {
        if (is_removed_skill(skill))
            continue;
        if (is_useless_skill(skill))
            count++;
    }
    return count;
}

static int _total_skill_count()
{
    int count = 0;
    for (skill_type skill = SK_FIRST_SKILL; skill < NUM_SKILLS; ++skill)
    {
        if (is_removed_skill(skill))
            continue;
        count++;
    }
    return count;
}

// The current cost of raising each skill by one skill point, taking the
// gnoll penalty for useless skills into account and rounding up for all
// computations. Used to ensure that gnoll skills rise evenly - we don't
// train anything unless we have this much xp to spend.
int _gnoll_total_skill_cost()
{
    int this_cost;
    int total_cost = 0;
    int cur_cost_level = you.skill_cost_level;
    const int useless_count = _useless_skill_count();
    const int total_count = _total_skill_count();
    const int num = total_count;
    const int denom = total_count - useless_count;
    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        if (!you.training[i])
            continue;
        cur_cost_level = _calc_skill_cost_level(you.total_experience + total_cost, cur_cost_level);
        this_cost = calc_skill_cost(cur_cost_level);
        if (num != denom)
            this_cost = (num * this_cost + denom - 1) / denom;
        total_cost += this_cost;
    }
    return total_cost;
}

void change_skill_points(skill_type sk, int points, bool do_level_up)
{
    if (static_cast<int>(you.skill_points[sk]) < -points)
        points = -(int)you.skill_points[sk];

    you.skill_points[sk] += points;

    check_skill_level_change(sk, do_level_up);
}

// Calculates the skill points required to reach the training target
// Does not currently consider Ashenzari skill boost for experience currently being gained
// so this may still result in some overtraining
static int _training_target_skill_point_diff(skill_type exsk, int training_target)
{
    int target_level = training_target / 10;
    int target_fractional = training_target % 10;
    int target_skill_points;

    if (target_level == MAX_SKILL_LEVEL)
        target_skill_points = skill_exp_needed(target_level, exsk);
    else
    {
        int target_level_points = skill_exp_needed(target_level, exsk);
        int target_next_level_points = skill_exp_needed(target_level + 1, exsk);
        // Round up for any remainder to ensure target is hit
        target_skill_points = target_level_points
            + div_round_up((target_next_level_points - target_level_points)
                            * target_fractional, 10);
    }

    int you_skill_points = you.skill_points[exsk] + get_crosstrain_points(exsk);
    if (ash_has_skill_boost(exsk))
        you_skill_points += ash_skill_point_boost(exsk, training_target);

    int target_skill_point_diff = target_skill_points - you_skill_points;

    int manual_charges = you.skill_manual_points[exsk];
    if (manual_charges > 0)
        target_skill_point_diff -= min(manual_charges, target_skill_point_diff / 2);

    return target_skill_point_diff;
}

static int _train(skill_type exsk, int &max_exp, bool simu, bool check_targets)
{
    // This will be added to you.skill_points[exsk];
    int skill_inc = 1;

    // This will be deducted from you.exp_available.
    int cost = calc_skill_cost(you.skill_cost_level);

    if (you.has_mutation(MUT_DISTRIBUTED_TRAINING))
    {
        int useless_count = _useless_skill_count();
        int total_count = _total_skill_count();
        int num = total_count;
        int denom = total_count - useless_count;
        if (num != denom)
            cost = div_rand_round(num * cost, denom);
    }
    else
    {
        // Scale cost and skill_inc to available experience.
        const int spending_limit = min(10 * MAX_SPENDING_LIMIT, max_exp);
        skill_inc = spending_limit / cost;

        int training_target = you.training_targets[exsk];
        if (training_target > you.skill(exsk, 10, false, false))
        {
            int target_skill_point_diff = _training_target_skill_point_diff(exsk, training_target);
            if (target_skill_point_diff > 0)
                skill_inc = min(skill_inc, target_skill_point_diff);
        }
        cost = skill_inc * cost;
    }

    if (skill_inc <= 0 || cost > max_exp)
        return 0;

    // Bonus from manual
    int bonus_left = skill_inc;
    if (you.skill_manual_points[exsk])
    {
        const int bonus = min<int>(bonus_left, you.skill_manual_points[exsk]);
        skill_inc += bonus;
        bonus_left -= bonus;
        you.skill_manual_points[exsk] -= bonus;
        if (!you.skill_manual_points[exsk] && !simu && !crawl_state.simulating_xp_gain)
        {
            mprf("You have finished your manual of %s and %stoss it away.",
                 skill_name(exsk),
                 exsk == SK_THROWING ? "skilfully " : "");
        }
    }

    const skill_type old_best_skill = best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
    const int old_level = you.skill(exsk, 10, true);
    you.skill_points[exsk] += skill_inc;
    you.exp_available -= cost;
    you.total_experience += cost;
    max_exp -= cost;

    if (!simu)
    {
        if (check_targets)
            check_training_targets();
        redraw_skill(exsk, old_best_skill, (you.skill(exsk, 10, true) > old_level));
    }

    check_skill_cost_change();
    ASSERT(you.exp_available >= 0);
    ASSERT(max_exp >= 0);
    you.redraw_experience = true;

    return skill_inc;
}

/**
 * Calculate the difference in skill points and xp for new skill level `amount`.
 *
 * If `base_only` is false, this will produce an estimate only. Otherwise, it is
 * exact. This function is used in both estimating skill targets, and in
 * actually changing skills. It will work both for cases where the new skill
 * level is greater, and where it is less, than the current level.
 *
 * @param skill the skill to calculate for.
 * @param amount the new skill level for `skill`.
 * @param scaled_training how to scale the training values (for estimating what
 *          happens when other skills are being trained).
 * @param base_only whether to calculate on actual unmodified skill levels, or
 *          use various bonuses that apply on top of these (crosstraining, ash,
 *          manuals).
 *
 * @return a pair consisting of the difference in skill points, and the
 *          difference in xp.
 */
skill_diff skill_level_to_diffs(skill_type skill, double amount,
                            int scaled_training,
                            bool base_only)
{
    // TODO: should this use skill_state?
    // TODO: can `amount` be converted to fixed point?
    double level;
    double fractional = modf(amount, &level);
    if (level >= MAX_SKILL_LEVEL)
    {
        level = MAX_SKILL_LEVEL;
        fractional = 0;
    }

    unsigned int target = skill_exp_needed(level, skill);
    if (fractional)
    {
        target += (skill_exp_needed(level + 1, skill)
                  - skill_exp_needed(level, skill)) * fractional + 1;
    }

    // We're calculating you.skill_points[skill] and calculating the new
    // you.total_experience to update skill cost.

    unsigned int you_skill = you.skill_points[skill];

    if (!base_only)
    {
        // Factor in crosstraining bonus at the time of the query.
        // This will not address the case where some cross-training skills are
        // also being trained.
        you_skill += get_crosstrain_points(skill);

        // Estimate the ash bonus, based on current skill levels and piety.
        // This isn't perfectly accurate, because the boost changes as
        // skill increases. TODO: exact solution.
        // It also assumes that piety won't change.
        if (ash_has_skill_boost(skill))
            you_skill += ash_skill_point_boost(skill, you.skills[skill] * 10);

        if (you.skill_manual_points[skill])
            target = you_skill + (target - you_skill) / 2;
    }

    if (target == you_skill)
        return skill_diff();

    // Do we need to increase or decrease skill points/xp?
    // XXX: reducing with ash bonuses in play could lead to weird results.
    const bool decrease_skill = target < you_skill;

    int you_xp = you.total_experience;
    int you_skill_cost_level = you.skill_cost_level;

#ifdef DEBUG_TRAINING_COST
    dprf(DIAG_SKILLS, "target skill points: %d.", target);
#endif
    while (you_skill != target)
    {
        // each loop is the max skill points that can be gained at the
        // current skill cost level, up to `target`.

        // If we are decreasing, find the xp needed to get to the current skill
        // cost level. Otherwise, find the xp needed to get to the next one.
        const int next_level = skill_cost_needed(you_skill_cost_level +
                                                    (decrease_skill ? 0 : 1));

        // max xp that can be added (or subtracted) in one pass of the loop
        int max_xp = abs(next_level - you_xp);

        // When reducing, we don't want to stop right at the limit, unless
        // we're at skill cost level 0.
        if (decrease_skill && you_skill_cost_level)
            ++max_xp;

        const int cost = calc_skill_cost(you_skill_cost_level);
        // Maximum number of skill points to transfer in one go.
        // It's max_xp/cost rounded up.
        const int max_skp = max((max_xp + cost - 1) / cost, 1);

        skill_diff delta;
        delta.skill_points = min<int>(abs((int)(target - you_skill)),
                                 max_skp);
        delta.experience = delta.skill_points * cost;

        if (decrease_skill)
        {
            // We are decreasing skill points / xp to reach the target. Ensure
            // that the delta is negative but won't result in negative skp or xp
            delta.skill_points = -min<int>(delta.skill_points, you_skill);
            delta.experience = -min<int>(delta.experience, you_xp);
        }

#ifdef DEBUG_TRAINING_COST
        dprf(DIAG_SKILLS, "cost level: %d, total experience: %d, "
             "next level: %d, skill points: %d, delta_skp: %d, delta_xp: %d.",
             you_skill_cost_level, you_xp, next_level,
             you_skill, delta.skill_points, delta.experience);
#endif
        you_skill += (delta.skill_points * scaled_training
                                        + (decrease_skill ? -99 : 99)) / 100;
        you_xp += delta.experience;
        you_skill_cost_level = _calc_skill_cost_level(you_xp, you_skill_cost_level);
    }

    return skill_diff(you_skill - you.skill_points[skill],
                                you_xp - you.total_experience);
}

void set_skill_level(skill_type skill, double amount)
{
    double level;
    modf(amount, &level);

    skill_diff diffs = skill_level_to_diffs(skill, amount);

    you.skills[skill] = level;
    you.skill_points[skill] += diffs.skill_points;
    you.total_experience += diffs.experience;
#ifdef DEBUG_TRAINING_COST
    dprf("Change (total): %d skp (%d), %d xp (%d)",
        diffs.skill_points, you.skill_points[skill],
        diffs.experience, you.total_experience);
#endif

    check_skill_cost_change();

    // need to check them all, to handle crosstraining.
    check_training_targets();
}

int get_skill_progress(skill_type sk, int level, int points, int scale)
{
    if (level >= MAX_SKILL_LEVEL)
        return 0;

    const int needed = skill_exp_needed(level + 1, sk);
    const int prev_needed = skill_exp_needed(level, sk);
    if (needed == 0) // invalid race, legitimate at startup
        return 0;
    // A scale as small as 92 would overflow with 31 bits if skill_rdiv()
    // is involved: needed can be 91985, skill_rdiv() multiplies by 256.
    const int64_t amt_done = points - prev_needed;
    int prog = amt_done * scale / (needed - prev_needed);

    ASSERT(prog >= 0);

    return prog;
}

int get_skill_progress(skill_type sk, int scale)
{
    return get_skill_progress(sk, you.skills[sk], you.skill_points[sk], scale);
}

int get_skill_percentage(const skill_type x)
{
    return get_skill_progress(x, 100);
}

/**
 * Get the training target for a skill.
 *
 * @param sk the skill to set
 * @return the current target, scaled by 10 -- so between 0 and 270.
 *         0 means no target.
 */
int player::get_training_target(const skill_type sk) const
{
    ASSERT_LESS(training_targets[sk], 271);
    return training_targets[sk];
}

/**
 * Set the training target for a skill.
 *
 * @param sk the skill to set
 * @param target the new target, between 0.0 and 27.0.  0.0 means no target.
 */
bool player::set_training_target(const skill_type sk, const double target, bool announce)
{
    return set_training_target(sk, (int) round(target * 10), announce);
}

void player::clear_training_targets()
{
    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
        set_training_target(sk, 0);
}

/**
 * Set the training target for a skill, scaled by 10.
 *
 * @param sk the skill to set
 * @param target the new target, scaled by ten, so between 0 and 270.  0 means
 *               no target.
 *
 * @return whether setting the target succeeded.
 */
bool player::set_training_target(const skill_type sk, const int target, bool announce)
{
    if (target > 270) // if target is above 270, reject with an error
    {
        mpr("Your training target must be 27 or below!");
        return false;
    }
    const int ranged_target = min(max((int) target, 0), 270);
    if (announce && ranged_target != (int) training_targets[sk])
    {
        if (you.has_mutation(MUT_DISTRIBUTED_TRAINING))
            mpr("You can't set training targets!");
        else if (ranged_target == 0)
            mprf("Clearing the skill training target for %s.", skill_name(sk));
        else
        {
            mprf("Setting a skill training target for %s at %d.%d.", skill_name(sk),
                                    ranged_target / 10, ranged_target % 10);
        }
    }
    if (!can_enable_skill(sk)) // checks for gnolls
    {
        training_targets[sk] = 0;
        return false;
    }
    training_targets[sk] = ranged_target;
    return true;
}

const char *skill_name(skill_type which_skill)
{
    ASSERT(which_skill < NUM_SKILLS);
    return skill_titles[which_skill][0];
}

const char * skill_abbr(skill_type which_skill)
{
    ASSERT(which_skill < NUM_SKILLS);
    return skill_titles[which_skill][6];
}

/**
 * Get a skill_type from an (exact, case-insensitive) skill name.
 *
 * @return a valid skill_type, or SK_NONE on failure.
 *
 * @see skill_from_name for a non-exact version.
 */
skill_type str_to_skill(const string &skill)
{
    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
        if (lowercase_string(skill) == lowercase_string(skill_titles[sk][0]))
            return sk;

    return SK_NONE;
}

/**
 * Get a skill_type from a skill name.
 *
 * @return a valid skill_type, or SK_FIGHTING on failure.
 */
skill_type str_to_skill_safe(const string &skill)
{
    // legacy behaviour -- ensure that a valid skill is returned.
    skill_type sk = str_to_skill(skill);
    if (sk == SK_NONE)
        return SK_FIGHTING;
    else
        return sk;
}

static string _stk_weight(species_type species)
{
    if (species::size(species) == SIZE_LARGE || species == SP_COGLIN)
        return "Heavy";
    else if (species::size(species, PSIZE_BODY) == SIZE_LARGE)
        return "Cruiser";
    else if (species::size(species) == SIZE_SMALL || species == SP_TENGU)
        return "Feather";
    else if (species::size(species) == SIZE_LITTLE)
        return "Fly";
    else if (species::is_elven(species))
        return "Light";
    else
        return "Middle";
}

unsigned get_skill_rank(unsigned skill_lev)
{
    // Translate skill level into skill ranking {dlb}:
    return (skill_lev <= 7)  ? 0 :
                           (skill_lev <= 14) ? 1 :
                           (skill_lev <= 20) ? 2 :
                           (skill_lev <= 26) ? 3
                           /* level 27 */    : 4;
}

// XX should at least some of this be in species.cc?

/**
 * What title will the player get at the given rank of the given skill?
 *
 * @param best_skill    The skill used to determine the title.
 * @param skill_rank    The player's rank in the given skill.
 * @param species       The player's species.
 * @param dex_better    Whether the player's dexterity is higher than strength.
 * @param god           The god_type of the god the player follows.
 * @param piety         The player's piety with the given god.
 * @return              An appropriate and/or humorous title.
 */
string skill_title_by_rank(skill_type best_skill, uint8_t skill_rank,
                           species_type species, bool dex_better,
                           god_type god, int piety)
{

    // paranoia
    if (is_invalid_skill(best_skill))
        return "Adventurer";

    // Increment rank by one to "skip" skill name in array {dlb}:
    ++skill_rank;

    string result;

    if (best_skill < NUM_SKILLS)
    {
        switch (best_skill)
        {
        case SK_FIGHTING:
            if (species == SP_HUMAN && skill_rank == 5 && god == GOD_MAKHLEB)
                result = "Hell Knight";
            break;

        case SK_POLEARMS:
            if (species == SP_ARMATAUR && skill_rank == 5)
                result = "Prickly Pangolin";
            break;

        case SK_UNARMED_COMBAT:
            if (species == SP_MUMMY && skill_rank == 5)
                result = "Pharaoh";
            else if (species == SP_FELID)
                result = claw_and_tooth_titles[skill_rank];
            else if (species == SP_OCTOPODE && skill_rank == 5)
                result = "Crusher";
            else if (!dex_better && species == SP_DJINNI && skill_rank == 5)
                result = "Weightless Champion";
            else
            {
                result = dex_better ? martial_arts_titles[skill_rank]
                                    : skill_titles[best_skill][skill_rank];
            }
            break;

        case SK_SHORT_BLADES:
            if (species::is_elven(species) && skill_rank == 5)
            {
                result = "Blademaster";
                break;
            }
            break;

        case SK_LONG_BLADES:
            if (species == SP_MERFOLK && skill_rank == 5)
            {
                result = "Swordfish";
                break;
            }
            break;

        case SK_ARMOUR:
            if (species == SP_TROLL && skill_rank == 5)
                result = "Iron Troll";
            else if (species == SP_COGLIN && skill_rank == 5)
                result = "Iron Golem";
            break;

        case SK_SHIELDS:
            if (species == SP_SPRIGGAN && skill_rank > 3)
                result = "Defender";
            break;

        case SK_RANGED_WEAPONS:
            if (species::is_elven(species) && skill_rank == 5)
                result = "Master Archer";
            break;

        case SK_SPELLCASTING:
            if (species == SP_DJINNI && skill_rank == 5)
                result = "Wishgranter";
            else if (species == SP_COGLIN && skill_rank == 5)
                result = "Cogmind";
            break;

        case SK_CONJURATIONS:
            // Stay safe, Winslem :(
            if (species == SP_TROLL && skill_rank > 3)
                result = "Wallbreaker";
            break;

        // For all draconian titles, intentionally don't restrict by drac
        // colour to avoid frustrating players deliberately trying for these
        case SK_HEXES:
            if (species::is_draconian(species) && skill_rank == 5)
                result = "Faerie Dragon";
            break;

        case SK_NECROMANCY:
            if (species == SP_SPRIGGAN && skill_rank == 5)
                result = "Petite Mort";
            else if (species == SP_VINE_STALKER && skill_rank == 5)
                result = "Corpseflower";
            else if (god == GOD_KIKUBAAQUDGHA)
                result = god_title(god, species, piety);
            break;

        case SK_SUMMONINGS:
            if (is_evil_god(god))
            {
                // retro goody-bag for decidedly non-goodies
                if (skill_rank == 4)
                    result = "Demonologist";
                else if (skill_rank == 5)
                    result = "Hellbinder";
            }
            break;

        case SK_TRANSLOCATIONS:
            if (species == SP_FORMICID && skill_rank == 5)
                result = "Teletunneler";
            break;

        case SK_ALCHEMY:
            if (species::is_draconian(species) && skill_rank == 5)
                result = "Swamp Dragon";
            break;

        case SK_FIRE_MAGIC:
            if (species::is_draconian(species) && skill_rank == 5)
                result = "Fire Dragon";
            else if (species == SP_MUMMY && skill_rank == 5)
                result = "Highly Combustible";
            else if (species == SP_GHOUL && skill_rank == 5)
                result = "Searing Wretch";
            else if (species == SP_GARGOYLE && skill_rank == 5)
                result = "Molten";
            else if (species == SP_DJINNI && skill_rank == 5)
                result = "Smokeless Flame";
            break;

        case SK_ICE_MAGIC:
            if (species::is_draconian(species) && skill_rank == 5)
                result = "Ice Dragon";
            else if (species == SP_DJINNI && skill_rank == 5)
                result = "Marid";
            break;

        case SK_AIR_MAGIC:
            if (species::is_draconian(species) && skill_rank == 5)
                result = "Storm Dragon";
            break;

        case SK_EARTH_MAGIC:
            if (species::is_draconian(species) && skill_rank == 5)
                result = "Iron Dragon";
            break;

        case SK_INVOCATIONS:
            if (species == SP_MUMMY && skill_rank == 5 && god == GOD_GOZAG)
                result = "Royal Mummy";
            else if (species == SP_MUMMY && skill_rank == 5 && god == GOD_NEMELEX_XOBEH)
                result = "Forbidden One";
            else if (species == SP_DEMONSPAWN && skill_rank == 5 && is_evil_god(god))
                result = "Blood Saint";
            else if (species == SP_MUMMY && skill_rank == 5 && god == GOD_USKAYAW)
                result = "Necrodancer";
            else if (species::is_draconian(species) && skill_rank == 5 && is_good_god(god))
                result = "Pearl Dragon";
            else if (species == SP_GARGOYLE && skill_rank == 5 && god == GOD_JIYVA)
                result = "Rockslime";
            else if (species == SP_VINE_STALKER && skill_rank == 5 && god == GOD_NEMELEX_XOBEH)
                result = "Black Lotus";
            else if (species == SP_VINE_STALKER && skill_rank == 5 && god == GOD_DITHMENOS)
                result = "Nightshade";
            else if (species == SP_ARMATAUR && skill_rank == 5 && god == GOD_QAZLAL)
                result = "Rolling Thunder";
            else if (species == SP_ARMATAUR && skill_rank == 5 && is_good_god(god))
                result = "Holy Roller";
            else if (species == SP_COGLIN && skill_rank == 5 && god == GOD_FEDHAS)
                result = "Cobgoblin"; // hm.
            else if (god != GOD_NO_GOD)
                result = god_title(god, species, piety);
            else if (species == SP_BARACHI)
            {
                // C.f. the barachi species lore, true believers!
                result = "God-Hated";
            }
            break;

#if TAG_MAJOR_VERSION == 34
        case SK_EVOCATIONS:
            if (god == GOD_PAKELLAS)
                result = god_title(god, species, piety);
            break;
#endif

        default:
            break;
        }
        // Make sure the traitor induced title overrides under penance
        if (you.attribute[ATTR_TRAITOR] > 0)
        {
            god_type betrayed_god = static_cast<god_type>(
                you.attribute[ATTR_TRAITOR]);
            result = god_title(betrayed_god, species, piety);
        }

        if (result.empty())
            result = skill_titles[best_skill][skill_rank];
    }

    const map<string, string> replacements =
    {
        { "Adj", species::name(species, species::SPNAME_ADJ) },
        { "Genus", species::name(species, species::SPNAME_GENUS) },
        { "genus", lowercase_string(species::name(species, species::SPNAME_GENUS)) },
        { "Genus_Short", species == SP_DEMIGOD ? "God" :
                           species::name(species, species::SPNAME_GENUS) },
        { "Walker", species::walking_title(species) + "er" },
        { "Weight", _stk_weight(species) },
    };

    return replace_keys(result, replacements);
}

/** What is the player's current title.
 *
 *  @param the whether to prepend a definite article.
 *  @returns the title.
 */
string player_title(bool the)
{
    const skill_type best = best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
    const string title =
            skill_title_by_rank(best, get_skill_rank(you.skills[best]));
    const string article = !the ? ""
                                : title == "Petite Mort" ? "La "
                                : title == "Who Hides the Stars" ? ", "
                                : "the ";
    return article + title;
}

skill_type best_skill(skill_type min_skill, skill_type max_skill,
                      skill_type excl_skill)
{
    ASSERT(min_skill < NUM_SKILLS);
    ASSERT(max_skill < NUM_SKILLS);
    skill_type ret = SK_FIGHTING;
    unsigned int best_skill_level = 0;
    unsigned int best_position = 1000;

    for (int i = min_skill; i <= max_skill; i++)
    {
        skill_type sk = static_cast<skill_type>(i);
        if (sk == excl_skill)
            continue;

        const unsigned int skill_level = you.skill(sk, 10, true);
        if (skill_level > best_skill_level)
        {
            ret = sk;
            best_skill_level = skill_level;
            best_position = you.skill_order[sk];

        }
        else if (skill_level == best_skill_level
                 && you.skill_order[sk] < best_position)
        {
            ret = sk;
            best_position = you.skill_order[sk];
        }
    }

    return ret;
}

// Calculate the skill_order array from scratch.
//
// The skill order array is used for breaking ties in best_skill.
// This is done by ranking each skill by the order in which it
// has attained its current level (the values are the number of
// skills at or above that level when the current skill reached it).
//
// In this way, the skill which has been at a level for the longest
// is judged to be the best skill (thus, nicknames are sticky)...
// other skills will have to attain the next level higher to be
// considered a better skill (thus, the first skill to reach level 27
// becomes the characters final nickname). -- bwr
void init_skill_order()
{
    for (skill_type si = SK_FIRST_SKILL; si < NUM_SKILLS; ++si)
    {
        const unsigned int i_points = you.skill_points[si]
                                      / species_apt_factor(si);

        you.skill_order[si] = 0;

        for (skill_type sj = SK_FIRST_SKILL; sj < NUM_SKILLS; ++sj)
        {
            if (si == sj)
                continue;

            const unsigned int j_points = you.skill_points[sj]
                                          / species_apt_factor(sj);

            if (you.skills[sj] == you.skills[si]
                && (j_points > i_points
                    || (j_points == i_points && sj > si)))
            {
                you.skill_order[si]++;
            }
        }
    }
}

bool is_removed_skill(skill_type skill)
{
#if TAG_MAJOR_VERSION == 34
    switch (skill)
    {
    case SK_STABBING:
    case SK_TRAPS:
    case SK_CHARMS:
    case SK_SLINGS:
    case SK_CROSSBOWS:
        return true;
    default:
        break;
    }
#else
    UNUSED(skill);
#endif
    return false;
}

static map<skill_type, mutation_type> skill_sac_muts = {
    { SK_AIR_MAGIC,      MUT_NO_AIR_MAGIC },
    { SK_FIRE_MAGIC,     MUT_NO_FIRE_MAGIC },
    { SK_EARTH_MAGIC,    MUT_NO_EARTH_MAGIC },
    { SK_ICE_MAGIC,      MUT_NO_ICE_MAGIC },
    { SK_ALCHEMY,        MUT_NO_ALCHEMY_MAGIC },
    { SK_HEXES,          MUT_NO_HEXES_MAGIC },
    { SK_TRANSLOCATIONS, MUT_NO_TRANSLOCATION_MAGIC },
    { SK_CONJURATIONS,   MUT_NO_CONJURATION_MAGIC },
    { SK_NECROMANCY,     MUT_NO_NECROMANCY_MAGIC },
    { SK_FORGECRAFT,     MUT_NO_FORGECRAFT_MAGIC },
    { SK_SUMMONINGS,     MUT_NO_SUMMONING_MAGIC },

    { SK_DODGING,        MUT_NO_DODGING },
    { SK_ARMOUR,         MUT_NO_ARMOUR_SKILL },
    { SK_EVOCATIONS,     MUT_NO_ARTIFICE },
    { SK_STEALTH,        MUT_NO_STEALTH },
    { SK_SHAPESHIFTING,  MUT_NO_FORMS },
};

bool can_sacrifice_skill(mutation_type mut)
{
    for (auto sac : skill_sac_muts)
        if (sac.second == mut)
            return species_apt(sac.first) != UNUSABLE_SKILL;
    return true;
}

static bool _is_sacrificed_skill(skill_type skill)
{
    auto mut = skill_sac_muts.find(skill);
    if (mut != skill_sac_muts.end() && you.has_mutation(mut->second))
        return true;
    // shields isn't in the big map because shields being useless doesn't
    // imply that missing hand is meaningless. likewise for summoning magic
    // vs. ability to have friendlies at all.
    if (skill == SK_SHIELDS && you.get_mutation_level(MUT_MISSING_HAND)
        || skill == SK_SUMMONINGS && you.get_mutation_level(MUT_NO_LOVE))
    {
        return true;
    }
    return false;
}

bool is_useless_skill(skill_type skill)
{
    return is_removed_skill(skill)
       || _is_sacrificed_skill(skill)
       || species_apt(skill) == UNUSABLE_SKILL;
}

bool is_harmful_skill(skill_type skill)
{
    return is_magic_skill(skill) && you_worship(GOD_TROG);
}

/**
 * Does the player have trainable skills?
 *
 * @param check_all If true, also consider skills that are harmful and/or
 *        currently untrainable. Useless skills are never considered.
 *        Defaults to false.
 */
bool trainable_skills(bool check_all)
{
    for (skill_type i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
    {
        skill_type sk = static_cast<skill_type>(i);
        if (can_enable_skill(sk, check_all))
            return true;
    }

    return false;
}

// Currently only for ABIL_BEOGH_SMITING, see ability.cc::_beogh_smiting_power
int skill_bump(skill_type skill, int scale, bool allow_random)
{
    const int sk = allow_random ? you.skill_rdiv(skill, scale)
                                : you.skill(skill, scale);
    return sk + min(sk, 3 * scale);
}

// What aptitude value corresponds to doubled skill learning
// (i.e., old-style aptitude 50).
#define APT_DOUBLE 4

float apt_to_factor(int apt)
{
    return 1 / exp(log(2) * apt / APT_DOUBLE);
}

static int _modulo_skill_cost(int modulo_level)
{
    return 25 * modulo_level * (modulo_level + 1);
}

static bool exp_costs_initialized = false;
static int _get_skill_cost_for(int level)
{
    static int skill_cost_table[28];
    const int breakpoints[3] = { 9, 18, 26 };
    if (!exp_costs_initialized)
    {
        for (int skill_level = 0; skill_level < 28; skill_level++)
        {
            skill_cost_table[skill_level] = _modulo_skill_cost(skill_level);
            for (int break_idx = 0; break_idx < (int)ARRAYSZ(breakpoints); ++break_idx)
            {
                const int breakpoint = breakpoints[break_idx];
                if (skill_level <= breakpoint)
                    break;
                skill_cost_table[skill_level] += _modulo_skill_cost(skill_level - breakpoint) / 2;
            }
        }
        exp_costs_initialized = true;
    }
    return skill_cost_table[level];
}

unsigned int skill_exp_needed(int lev, skill_type sk, species_type sp)
{
    ASSERT_RANGE(lev, 0, MAX_SKILL_LEVEL + 1);
    return _get_skill_cost_for(lev) * species_apt_factor(sk, sp);
}

int species_apt(skill_type skill, species_type species)
{
    static bool spec_skills_initialised = false;

    if (skill >= NUM_SKILLS)
        return UNUSABLE_SKILL;
    if (!spec_skills_initialised)
    {
        // Setup sentinel values to find errors more easily.
        const int sentinel = -20; // this gives cost 3200
        for (int sp = 0; sp < NUM_SPECIES; ++sp)
            for (int sk = 0; sk < NUM_SKILLS; ++sk)
                _spec_skills[sp][sk] = sentinel;
        for (const species_skill_aptitude &ssa : species_skill_aptitudes)
        {
            ASSERT(_spec_skills[ssa.species][ssa.skill] == sentinel);
            _spec_skills[ssa.species][ssa.skill] = ssa.aptitude;
        }
        spec_skills_initialised = true;
    }

    return max(UNUSABLE_SKILL, _spec_skills[species][skill]
                               - you.get_mutation_level(MUT_UNSKILLED));
}

float species_apt_factor(skill_type sk, species_type sp)
{
    return apt_to_factor(species_apt(sk, sp));
}

vector<skill_type> get_crosstrain_skills(skill_type sk)
{
    // Gnolls do not have crosstraining.
    if (you.has_mutation(MUT_DISTRIBUTED_TRAINING))
        return {};

    switch (sk)
    {
    case SK_SHORT_BLADES:
        return { SK_LONG_BLADES };
    case SK_LONG_BLADES:
        return { SK_SHORT_BLADES };
    case SK_AXES:
    case SK_STAVES:
        return { SK_POLEARMS, SK_MACES_FLAILS };
    case SK_MACES_FLAILS:
    case SK_POLEARMS:
        return { SK_AXES, SK_STAVES };
    default:
        return {};
    }
}

/**
 * Calculate the current crosstraining bonus for skill `sk`, in skill points.
 */
int get_crosstrain_points(skill_type sk)
{
    int points = 0;
    for (skill_type cross : get_crosstrain_skills(sk))
        points += you.skill_points[cross] * 2 / 5;
    return points;

}

/**
 * Is the provided skill one of the elemental spellschools?
 *
 * @param sk    The skill in question.
 * @return      Whether it is fire, ice, earth, or air.
 */
static bool _skill_is_elemental(skill_type sk)
{
    return sk == SK_FIRE_MAGIC || sk == SK_EARTH_MAGIC
           || sk == SK_AIR_MAGIC || sk == SK_ICE_MAGIC;
}

/**
 * How skilled is the player at the elemental components of a spell?
 *
 * @param spell     The type of spell in question.
 * @param scale     Scaling factor for skill.
 * @return          The player's skill at the elemental parts of a given spell.
 */
int elemental_preference(spell_type spell, int scale)
{
    skill_set skill_list;
    spell_skills(spell, skill_list);
    int preference = 0;
    for (skill_type sk : skill_list)
        if (_skill_is_elemental(sk))
            preference += you.skill(sk, scale);
    return preference;
}

void dump_skills(string &text)
{
    for (uint8_t i = 0; i < NUM_SKILLS; i++)
    {
        int real = you.skill((skill_type)i, 10, true);
        int cur  = you.skill((skill_type)i, 10);
        if (real > 0 || (!you.auto_training && you.train[i] > 0))
        {
            text += make_stringf(" %c Level %.*f%s %s\n",
                                 real == 270       ? 'O' :
                                 !you.can_currently_train[i] ? ' ' :
                                 you.train[i] == 2 ? '*' :
                                 you.train[i]      ? '+' :
                                                     '-',
                                 real == 270 ? 0 : 1,
                                 real * 0.1,
                                 real != cur
                                     ? make_stringf("(%.*f)",
                                           cur == 270 ? 0 : 1,
                                           cur * 0.1).c_str()
                                     : "",
                                 skill_name(static_cast<skill_type>(i)));
        }
    }
}

skill_state::skill_state() :
        skill_cost_level(0), total_experience(0), auto_training(true),
        exp_available(0), saved(false)
{
}

void skill_state::save()
{
    can_currently_train = you.can_currently_train;
    skills              = you.skills;
    train               = you.train;
    training            = you.training;
    skill_points        = you.skill_points;
    training_targets    = you.training_targets;
    skill_cost_level    = you.skill_cost_level;
    skill_order         = you.skill_order;
    auto_training       = you.auto_training;
    exp_available       = you.exp_available;
    total_experience    = you.total_experience;
    skill_manual_points = you.skill_manual_points;
    for (int i = 0; i < NUM_SKILLS; i++)
    {
        real_skills[i] = you.skill((skill_type)i, 10, true);
        changed_skills[i] = you.skill((skill_type)i, 10);
    }
    saved = true;
}

bool skill_state::state_saved() const
{
    return saved;
}

void skill_state::restore_levels()
{
    you.skills                      = skills;
    you.skill_points                = skill_points;
    you.skill_cost_level            = skill_cost_level;
    you.skill_order                 = skill_order;
    you.exp_available               = exp_available;
    you.total_experience            = total_experience;
    you.skill_manual_points         = skill_manual_points;
}

void skill_state::restore_training()
{
    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
    {
        // Don't resume training if it's impossible or a target was met
        // after our backup was made.
        if (you.skills[sk] < MAX_SKILL_LEVEL)
        {
            you.train[sk] = train[sk];
            you.training_targets[sk] = training_targets[sk];
        }
    }

    you.can_currently_train         = can_currently_train;
    you.auto_training               = auto_training;
    reset_training();
    check_training_targets();
}

// Sanitize skills after an upgrade, racechange, etc.
void fixup_skills()
{
    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
    {
        if (is_useless_skill(sk))
        {
            you.skill_points[sk] = 0;
            // gnolls have everything existent enabled, so that the
            // training percentage is calculated correctly. (Useless
            // skills still won't be trained for them.)
            if (you.has_mutation(MUT_DISTRIBUTED_TRAINING)
                && !is_removed_skill(sk))
            {
                you.train[sk] = TRAINING_ENABLED;
            }
            else
                you.train[sk] = TRAINING_DISABLED;
        }
        else if (you.has_mutation(MUT_DISTRIBUTED_TRAINING))
            you.train[sk] = TRAINING_ENABLED;
        you.skill_points[sk] = min(you.skill_points[sk],
                                   skill_exp_needed(MAX_SKILL_LEVEL, sk));
        check_skill_level_change(sk);
    }
    init_can_currently_train();
    reset_training();

    if (you.exp_available >= 10 * calc_skill_cost(you.skill_cost_level)
        && !you.has_mutation(MUT_DISTRIBUTED_TRAINING)
        && !you.has_mutation(MUT_INNATE_CASTER))
    {
        skill_menu(SKMF_EXPERIENCE);
    }

    check_training_targets();
}

/** Can the player enable training for this skill?
 *
 * @param sk The skill to check.
 * @param override if true, don't consider whether the skill is currently
 *                 untrainable / harmful.
 * @returns True if the skill can be enabled for training, false otherwise.
 */
bool can_enable_skill(skill_type sk, bool override)
{
    // TODO: should this check you.skill_points or you.skills?
    return !you.has_mutation(MUT_DISTRIBUTED_TRAINING)
       && you.skills[sk] < MAX_SKILL_LEVEL
       && !is_useless_skill(sk)
       && (override || (you.can_currently_train[sk] && !is_harmful_skill(sk)));
}

void set_training_status(skill_type sk, training_status st)
{
    if (you.has_mutation(MUT_INNATE_CASTER) && is_magic_skill(sk))
        set_magic_training(st);
    else
        you.train[sk] = st;
}

void set_magic_training(training_status st)
{
    for (skill_type sk = SK_SPELLCASTING; sk <= SK_LAST_MAGIC; ++sk)
        if (!is_removed_skill(sk))
            you.train[sk] = you.train_alt[sk] = st;
}
