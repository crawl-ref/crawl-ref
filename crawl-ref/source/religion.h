/**
 * @file
 * @brief Misc religion related functions.
**/

#pragma once

#include <vector>

#include "enum.h"
#include "mgen-data.h"
#include "player.h"
#include "religion-enum.h"

using std::vector;

#define MAX_PIETY      200
#define HALF_MAX_PIETY (MAX_PIETY / 2)

#define MAX_PENANCE    200

#define NUM_VEHUMET_GIFTS 13

#define NUM_PIETY_STARS 6

bool is_evil_god(god_type god);
bool is_good_god(god_type god);
bool is_chaotic_god(god_type god);
bool is_unknown_god(god_type god);
bool god_has_name(god_type god);

// Returns true if the god is not present in the current game. This is
// orthogonal to whether the player can worship the god in question.
bool is_unavailable_god(god_type god);

god_type random_god();

int piety_breakpoint(int i);
string god_name(god_type which_god, bool long_name = false);
string god_name_jiyva(bool second_name = false);
string wu_jian_random_sifu_name();
god_type str_to_god(const string &name, bool exact = true);

int initial_wrath_penance_for(god_type god);
void handle_traitor(god_type god);
bool active_penance(god_type god);
bool xp_penance(god_type god);
void dec_penance(int val);
void dec_penance(god_type god, int val);

void excommunication(bool voluntary = false, god_type new_god = GOD_NO_GOD);
int excom_xp_docked();

bool gain_piety(int pgn, int denominator = 1, bool should_scale_piety = true);
void dock_piety(int pietyloss, int penance, bool no_lecture = false);
void god_speaks(god_type god, const char *mesg);
void lose_piety(int pgn);
void set_piety(int piety);
void handle_god_time(int /*time_delta*/);
int god_colour(god_type god);
colour_t god_message_altar_colour(god_type god);
int gozag_service_fee();
bool player_can_join_god(god_type which_god, bool temp = true);
void join_trog_skills(void);
void join_religion(god_type which_god);
void god_pitch(god_type which_god);
void print_god_rejection(god_type which_god);
god_type choose_god(god_type def_god = NUM_GODS);
vector<god_type> get_ecu_gods(coord_def pos);

static inline bool you_worship(god_type god)
{
    return you.religion == god;
}

static inline int player_under_penance(god_type god = you.religion)
{
    return you.penance[god];
}

/** Is the player in good (enough) standing with a particular god?
 *
 * @param god    The religion being asked about.
 * @param pbreak The minimum piety breakpoint (number of stars minus one) to
 *               consider "enough"; a negative number (the default) checks
 *               only for worship and not for piety.
 * @return true if the player worships the given god, is not under penance,
 *         and has at least (pbreak + 1) stars of piety.
 */
static inline bool in_good_standing(god_type god, int pbreak = -1)
{
    return you_worship(god) && !player_under_penance(god)
           && (pbreak < 0 || you.piety >= piety_breakpoint(pbreak));
}

int had_gods();
int piety_rank(int piety = you.piety);
int piety_scale(int piety_change);
bool god_likes_your_god(god_type god, god_type your_god = you.religion);
bool god_hates_your_god(god_type god, god_type your_god = you.religion);
bool god_hates_killing(god_type god, const monster& mon);
bool god_hates_eating(god_type god, monster_type mc);

bool god_likes_spell(spell_type spell, god_type god);
bool god_hates_spellcasting(god_type god);
bool god_hates_spell(spell_type spell, god_type god, bool fake_spell = false);
string god_spell_warn_string(spell_type spell, god_type god);

void initialize_ashenzari_props();
bool god_protects_from_harm();
bool jiyva_is_dead();
bool ignis_is_dead();
void set_penance_xp_timeout();
bool fedhas_neutralises(const monster& target);
void nemelex_death_message();

string ignore_faith_reason();
bool faith_has_penalty();

void mons_make_god_gift(monster& mon, god_type god = you.religion);
bool mons_is_god_gift(const monster& mon, god_type god = you.religion);

bool yred_random_servant(unsigned int threshold, bool force_hostile = false, int num = 1);
bool yred_reap_chance();
bool is_yred_undead_follower(const monster& mon);
bool is_apostle_follower(const monster& mon);
bool is_fellow_slime(const monster& mon);
bool is_follower(const monster& mon);

// Vehumet gift interface.
bool vehumet_is_offering(spell_type spell, bool only = false);
void vehumet_accept_gift(spell_type spell);

mgen_data hepliaklqana_ancestor_gen_data();
string hepliaklqana_ally_name();
int hepliaklqana_ally_hp();

void upgrade_hepliaklqana_ancestor(bool quiet_force = false);
void upgrade_hepliaklqana_weapon(monster_type mtyp, item_def &item);
void upgrade_hepliaklqana_shield(const monster& ancestor, item_def &item);

void makhleb_initialize_marks();

bool god_hates_attacking_friend(god_type god, const monster& fr);

void religion_turn_start();
void religion_turn_end();

int get_tension(god_type god = you.religion);
int get_monster_tension(const monster& mons, god_type god = you.religion);
int get_fuzzied_monster_difficulty(const monster& mons);

typedef void (*delayed_callback)(const mgen_data &mg, monster *&mon, int placed);

void delayed_monster(const mgen_data &mg, delayed_callback callback = nullptr);
void delayed_monster_done(string success,
                          delayed_callback callback = nullptr);

bool do_god_gift(bool forced = false);

vector<god_type> temple_god_list();
vector<god_type> nontemple_god_list();

class god_iterator
{
public:
    god_iterator();

    operator bool() const;
    god_type operator*() const;
    god_type operator->() const;
    god_iterator& operator++();
    god_iterator operator++(int);

protected:
    int i;
};

struct god_power
{
    god_type god;
    // 1-6 means it unlocks at that many stars of piety;
    // 0 means it is always available when worshipping the god;
    // -1 means it is available even under penance;
    // 7 means it is a capstone.
    int rank;
    ability_type abil;

    // These strings can be one of two types:
    //   - a full sentence, starting with a capital letter and ending with a
    //     a period, that will be used for the relevant description; or
    //   - part of a sentence, starting with a lowercase letter, to which the
    //     following strings will be prepended, depending on which variable is
    //     being used, and "." will be appended, before it is used for a
    //     description:
    //         "You can now "       for `gain`
    //         "You can no longer " for `loss`
    //         "You can "           for `general`.

    // Printed to message log when the piety threshold for the power is reached
    const char* gain;
    // Printed to message log when piety falls below the threshold for the power
    const char* loss;
    // Shown on the god powers (^) screen
    const char* general;

    god_power(int rank_, ability_type abil_, const char* gain_,
              const char* loss_ = "", const char* general_ = "") :
              god(GOD_NO_GOD),
              rank{rank_}, abil{abil_}, gain{gain_},
              loss{*loss_ ? loss_ : gain_},
              general{*general_ ? general_ : gain_}
    { }

    god_power(int rank_, const char* gain_, const char* loss_ = "",
              const char* general_ = "") :
              god_power(rank_, ABIL_NON_ABILITY, gain_, loss_, general_)
    { }

    void display(bool gaining, const char* fmt) const;
};

void set_god_ability_slots();
const vector<vector<god_power>> & get_all_god_powers();
vector<god_power> get_god_powers(god_type god);
const god_power* god_power_from_ability(ability_type abil);
bool god_power_usable(const god_power& power, bool ignore_piety=false, bool ignore_penance=false);
