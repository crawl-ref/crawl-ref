#pragma once

#include "beam.h"
#include "beh-type.h"
#include "enum.h"
#include "god-type.h"
#include "item-prop-enum.h"
#include "spl-cast.h"

// How many aut until the next doom hound pops out of doom howl?
#define NEXT_DOOM_HOUND_KEY "next_doom_hound"

#define DRAGON_CALL_POWER_KEY "dragon_call_power"

#define SIMULACRUM_TYPE_KEY "simulacrum_type"

#define HOARFROST_SHOTS_KEY "hoarfrost_shot_count"
constexpr int MAX_HOARFROST_SHOTS = 4;

#define HELLFIRE_PATH_KEY "hellfire_mortar_path"

#define SERVITOR_SPELL_KEY "servitor_spell"

#define CLOCKWORK_BEE_TARGET "clockwork_bee_target"

#define PARAGON_WEAPON_KEY "paragon_weapon"
#define PARAGON_MID_KEY "paragon_mid"

#define SPLINTERFROST_POWER_KEY "splinterfrost_power"

#define PHALANX_BARRIER_POWER_KEY "phalanx_barrier_power"

#define RENDING_BLADE_MP_KEY "rending_blade_mp"
#define RENDING_BLADE_POWER_KEY "rending_blade_power"

constexpr int BOULDER_ABRASION_DAMAGE = 5;
constexpr int PARAGON_FINISHER_MID_CHARGE = 8;
constexpr int PARAGON_FINISHER_MAX_CHARGE = 14;

spret cast_summon_small_mammal(int pow, bool fail);

bool canine_familiar_is_alive();
monster *find_canine_familiar();
spret cast_call_canine_familiar(int pow, bool fail);

spret cast_awaken_armour(int pow, bool fail);
spret cast_summon_ice_beast(int pow, bool fail);
spret cast_summon_cactus(int pow, bool fail);
spret cast_monstrous_menagerie(actor* caster, int pow, bool fail = false);
spret cast_summon_dragon(actor *caster, int pow, bool fail = false);
spret cast_summon_hydra(actor *caster, int pow, bool fail = false);
spret cast_summon_mana_viper(int pow, bool fail);
bool summon_berserker(int pow, actor *caster,
                      monster_type override_mons = MONS_PROGRAM_BUG);
bool summon_holy_warrior(int pow, bool punish);

bool tukima_affects(const actor &target);
void cast_tukimas_dance(int pow, actor *target);
spret cast_conjure_ball_lightning(int pow, bool fail);
int ball_lightning_hd(int pow, bool random = true);
int mons_ball_lightning_hd(int pow, bool random = true);
int mons_ball_lightning_per_cast(int pow, bool random = true);
dice_def lightning_spire_damage(int pow);
spret cast_forge_lightning_spire(int pow, bool fail);

spret cast_call_imp(int pow, bool fail);
spret summon_shadow_creatures();
spret cast_summon_horrible_things(int pow, bool fail);
bool can_cast_malign_gateway();
void create_malign_gateway(coord_def point, beh_type beh, string cause,
                           int pow, bool is_player = false);
spret cast_malign_gateway(actor* caster, int pow, bool fail = false,
                          bool test = false);
coord_def find_gateway_location(actor* caster);
bool is_gateway_target(const actor& caster, coord_def location);
spret cast_summon_forest(actor* caster, int pow, bool fail, bool test=false);
spret cast_forge_blazeheart_golem(int pow, bool fail);

spret cast_dragon_call(int pow, bool fail);
void do_dragon_call(int time);

void doom_howl(int time);

spell_type player_servitor_spell();
bool spell_servitorable(spell_type spell);
void init_servitor(monster* servitor, actor* caster, int pow);
spret cast_spellspark_servitor(int pow, bool fail);
void remove_player_servitor();

monster_type pick_random_wraith();
spret cast_haunt(int pow, const coord_def& where, bool fail);

spret cast_martyrs_knell(const actor* caster, int pow, bool fail);

monster* find_battlesphere(const actor* agent);
spret cast_battlesphere(actor* agent, int pow, bool fail);
void end_battlesphere(monster* mons, bool killed);
bool battlesphere_can_mirror(spell_type spell);
vector<spell_type> player_battlesphere_spells();
bool trigger_battlesphere(actor* agent);
dice_def battlesphere_damage(int pow);

spret cast_fulminating_prism(actor* caster, int pow,
                                  const coord_def& where, bool fail,
                                  bool is_shadow = false);
int prism_hd(int pow, bool random = true);

monster* find_spectral_weapon(const actor* agent);
void end_spectral_weapon(monster* mons, bool killed, bool quiet = false);
void check_spectral_weapon(actor &agent);

spret cast_infestation(int pow, bolt &beam, bool fail);

void summoned_monster(const monster* mons, const actor* caster,
                      spell_type spell);
bool summons_are_capped(spell_type spell);
int summons_limit(spell_type spell, bool player);
int count_summons(const actor *summoner, spell_type spell);

vector<coord_def> find_briar_spaces(bool just_check = false);
void fedhas_wall_of_briars();
spret fedhas_grow_ballistomycete(const coord_def& target, bool fail);
spret fedhas_overgrow(bool fail);
spret fedhas_grow_oklob(const coord_def& target, bool fail);

void kiku_unearth_wretches();

spret cast_foxfire(actor &agent, int pow, bool fail,
                   bool marshlight = false);
spret foxfire_swarm();
bool summon_hell_out_of_bat(const actor &agent, coord_def pos);
bool summon_spider(const actor &agent, coord_def pos, spell_type spell, int pow);
spret summon_spiders(actor &agent, int pow, bool fail = false);
bool summon_swarm_clone(const monster& agent, coord_def target_pos);

spret summon_butterflies();

int barrelling_boulder_hp(int pow);
spret cast_broms_barrelling_boulder(actor& agent, coord_def pos, int pow, bool fail);

string mons_simulacrum_immune_reason(const monster *mons);
spret cast_simulacrum(coord_def target, int pow, bool fail);

dice_def hoarfrost_cannonade_damage(int pow, bool finale);
spret cast_hoarfrost_cannonade(const actor& agent, int pow, bool fail);

dice_def hellfire_mortar_damage(int pow);
spret cast_hellfire_mortar(const actor& agent, bolt& beam, int pow, bool fail);

bool make_soul_wisp(const actor& agent, actor& victim);

spret cast_clockwork_bee(coord_def target, bool fail);
void handle_clockwork_bee_spell(int turn);
void clockwork_bee_go_dormant(monster& bee);
bool clockwork_bee_recharge(monster& bee);
void clockwork_bee_pick_new_target(monster& bee);

dice_def diamond_sawblade_damage(int power);
vector<coord_def> diamond_sawblade_spots(bool actual);
spret cast_diamond_sawblades(int power, bool fail);

string surprising_crocodile_unusable_reason(const actor& agent,
                                            const coord_def& target, bool actual);
bool surprising_crocodile_can_drag(const actor& agent, const coord_def& target,
                                   bool actual);
spret cast_surprising_crocodile(actor& agent, const coord_def& targ,
                                int pow, bool fail);

spret cast_platinum_paragon(const coord_def& target, int pow, bool fail);
void paragon_attack_trigger();
monster* find_player_paragon();
int paragon_charge_level(const monster& paragon);
void paragon_charge_up(monster& paragon);
bool paragon_defense_bonus_active();

spret cast_walking_alembic(const actor& agent, int pow, bool fail);
void alembic_brew_potion(monster& mons);

spret cast_monarch_bomb(const actor& agent, int pow, bool fail);
bool monarch_deploy_bomblet(monster& original, const coord_def& target,
                            bool quiet = false);
vector<coord_def> get_monarch_detonation_spots(const actor& agent);
spret monarch_detonation(const actor& agent, int pow);

spret cast_splinterfrost_shell(const actor& agent, const coord_def& aim, int pow,
                             bool fail);
vector<coord_def> get_splinterfrost_block_spots(const actor& agent,
                                              const coord_def& aim, int num_walls);
bool splinterfrost_block_fragment(monster& block, const coord_def& aim);

spret cast_summon_seismosaurus_egg(const actor& agent, int pow, bool fail);

spret cast_phalanx_beetle(const actor& agent, int pow, bool fail);

dice_def rending_blade_damage(int power, bool include_mp);
spret cast_rending_blade(int pow, bool fail);
void trigger_rending_blade();
