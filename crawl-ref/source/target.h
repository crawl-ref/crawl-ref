#pragma once

#include <vector>

#include "beam.h"
#include "coordit.h"
#include "los-type.h"
#include "reach-type.h"

using std::vector;

struct passwall_path;

enum aff_type // sign and non-zeroness matters
{
    AFF_TRACER = -1,
    AFF_NO      = 0,
    AFF_MAYBE   = 1, // can possibly affect
    AFF_YES,         // intended/likely to affect
    // If you want to extend this to pass the probability somehow, feel free to,
    // just keep AFF_YES the minimal "bright" value.
    AFF_LANDING,     // Valid shadow step landing site
    AFF_MULTIPLE,    // Passes through multiple times
};

class targeter;

// radius_iterator might be better, but the code is too incomprehensible
// to subclass
class targeting_iterator : public rectangle_iterator
{
public:
    targeting_iterator(targeter &t, aff_type _threshold);
    void operator ++() override;
    aff_type is_affected();

private:
    targeter &tgt;
    aff_type threshold;
};

class targeter
{
public:
    targeter() :  agent(nullptr), obeys_mesmerise(false) {};
    virtual ~targeter() {};

    coord_def origin;
    coord_def aim;
    const actor* agent;
    string why_not;
    bool obeys_mesmerise; // whether the rendering of ranges should take into account mesmerise effects

    virtual bool set_aim(coord_def a);
    virtual bool valid_aim(coord_def a) = 0;
    virtual bool preferred_aim(coord_def a);
    virtual bool can_affect_outside_range();
    virtual bool can_affect_walls();

    virtual aff_type is_affected(coord_def loc) = 0;
    virtual bool can_affect_unseen();
    virtual bool affects_monster(const monster_info& mon);
    virtual bool harmful_to_player();

    targeting_iterator affected_iterator(aff_type threshold = AFF_YES);
protected:
    bool anyone_there(coord_def loc);
};

class targeter_beam : public targeter
{
public:
    targeter_beam(const actor *act, int range, zap_type zap, int pow,
                   int min_expl_rad, int max_expl_rad);
    bolt beam;
    virtual bool set_aim(coord_def a) override;
    bool valid_aim(coord_def a) override;
    bool can_affect_outside_range() override;
    virtual aff_type is_affected(coord_def loc) override;
    virtual bool affects_monster(const monster_info& mon) override;
    bool harmful_to_player() override;
protected:
    vector<coord_def> path_taken; // Path beam took.
    void set_explosion_aim(bolt tempbeam);
    void set_explosion_target(bolt &tempbeam);
    int min_expl_rad, max_expl_rad;
    int range;
private:
    bool penetrates_targets;
    explosion_map exp_map_min, exp_map_max;
};

class targeter_view : public targeter
{
public:
    targeter_view();
    bool valid_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
};

class targeter_smite : public targeter
{
public:
    targeter_smite(const actor *act, int range = LOS_RADIUS,
                    int exp_min = 0, int exp_max = 0,
                    bool harmless_to_player = false,
                    bool wall_ok = false, bool monster_okay = true,
                    bool (*affects_pos_func)(const coord_def &) = 0);
    virtual bool set_aim(coord_def a) override;
    virtual bool valid_aim(coord_def a) override;
    virtual bool can_affect_outside_range() override;
    bool can_affect_walls() override;
    aff_type is_affected(coord_def loc) override;
    bool harmful_to_player() override;
protected:
    // assumes exp_map is valid only if >0, so let's keep it private
    int exp_range_min, exp_range_max;
    explosion_map exp_map_min, exp_map_max;
    int range;
private:
    bool cannot_harm_player;
    bool affects_walls;
    bool can_target_monsters;
    bool (*affects_pos)(const coord_def &);
};

class targeter_permafrost : public targeter_smite
{
public:
    targeter_permafrost(const actor &act, int power);
    aff_type is_affected(coord_def loc) override;
private:
    set<coord_def> targets;
    set<coord_def> possible_centres;
    bool single_target;
};

class targeter_walljump : public targeter_smite
{
public:
    targeter_walljump();
    aff_type is_affected(coord_def loc) override;
    bool valid_aim(coord_def a) override;
};

class targeter_transference : public targeter_smite
{
public:
    targeter_transference(const actor *act, int aoe);
    bool valid_aim(coord_def a) override;
    bool affects_monster(const monster_info& mon) override;
};

class targeter_inner_flame : public targeter_smite
{
public:
    targeter_inner_flame(const actor *act, int range);
    bool valid_aim(coord_def a) override;
};

class targeter_simulacrum : public targeter_smite
{
public:
    targeter_simulacrum(const actor *act, int range);
    bool valid_aim(coord_def a) override;
};

class targeter_unravelling : public targeter_smite
{
public:
    targeter_unravelling();
    bool valid_aim(coord_def a) override;
    bool set_aim(coord_def a) override;
};

class targeter_fragment : public targeter_smite
{
public:
    targeter_fragment(const actor *act, int power, int range = LOS_RADIUS);
    bool set_aim(coord_def a) override;
    bool valid_aim(coord_def a) override;
private:
    int pow;
};

class targeter_airstrike : public targeter
{
public:
    targeter_airstrike();
    aff_type is_affected(coord_def loc) override;
    bool valid_aim(coord_def a) override;
    bool can_affect_outside_range() override { return false; };
    bool can_affect_walls() override { return false; };
    bool can_affect_unseen() override { return true; }; // show empty space outside LOS
};

class targeter_passage : public targeter_smite
{
public:
    targeter_passage(int _range);
    aff_type is_affected(coord_def loc) override;
};

class targeter_reach : public targeter
{
public:
    targeter_reach(const actor* act, reach_type ran = REACH_NONE);
    reach_type range;
    bool valid_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
};

class targeter_cleave : public targeter
{
public:
    targeter_cleave(const actor* act, coord_def target, int range);
    aff_type is_affected(coord_def loc) override;
    bool valid_aim(coord_def) override;
    bool set_aim(coord_def a) override;
private:
    set<coord_def> targets;
    int range;
};

class targeter_cloud : public targeter
{
public:
    targeter_cloud(const actor* act, cloud_type ctype, int range = LOS_RADIUS,
                   int count_min = 8, int count_max = 10);
    bool set_aim(coord_def a) override;
    bool valid_aim(coord_def a) override;
    bool can_affect_outside_range() override;
    aff_type is_affected(coord_def loc) override;
    cloud_type ctype;
    int range;
    int cnt_min, cnt_max;
    map<coord_def, aff_type> seen;
    vector<vector<coord_def> > queue;
};

class targeter_splash : public targeter_beam
{
public:
    targeter_splash(const actor *act, int ran, int pow);
    aff_type is_affected(coord_def loc) override;
};

class targeter_radius : public targeter
{
public:
    targeter_radius(const actor *act, los_type _los = LOS_DEFAULT,
                    int ran = LOS_RADIUS, int ran_max = 0, int ran_min = 0,
                    int ran_maybe = 0);
    bool valid_aim(coord_def a) override;
    virtual aff_type is_affected(coord_def loc) override;
private:
    los_type los;
    int range, range_max, range_min, range_maybe;
};

// like targeter_radius, but converts all AFF_YESes to AFF_MAYBE
class targeter_maybe_radius : public targeter_radius
{
public:
    targeter_maybe_radius(const actor *act, los_type _los = LOS_DEFAULT,
                  int ran = LOS_RADIUS, int ran_max = 0, int ran_min = 0)
        : targeter_radius(act, _los, ran, ran_max, ran_min)
    { }

    virtual aff_type is_affected(coord_def loc) override
    {
        if (targeter_radius::is_affected(loc))
            return AFF_MAYBE;
        else
            return AFF_NO;
    }
};

class targeter_refrig : public targeter_radius
{
public:
    targeter_refrig(actor *act)
        : targeter_radius(act, LOS_NO_TRANS, LOS_RADIUS, 0, 1)
    { }

    aff_type is_affected(coord_def loc) override;
};

class targeter_flame_wave : public targeter_radius
{
public:
    targeter_flame_wave(int _range);
    aff_type is_affected(coord_def loc) override;
};

class targeter_siphon_essence : public targeter_radius
{
public:
    targeter_siphon_essence();
    aff_type is_affected(coord_def loc) override;
};

class targeter_thunderbolt : public targeter
{
public:
    targeter_thunderbolt(const actor *act, int r, coord_def _prev);

    bool valid_aim(coord_def a) override;
    bool set_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
    map<coord_def, aff_type> zapped;
    FixedVector<int, LOS_RADIUS + 1> arc_length;
private:
    coord_def prev;
    int range;
};

class targeter_cone : public targeter
{
public:
    targeter_cone(const actor *act, int r);

    bool valid_aim(coord_def a) override;
    bool set_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
    map<coord_def, aff_type> zapped;
    FixedVector< map<coord_def, aff_type>, LOS_RADIUS + 1 > sweep;
private:
    int range;
};

#define CLOUD_CONE_BEAM_COUNT 11

class targeter_monster_sequence : public targeter_beam
{
public:
    targeter_monster_sequence(const actor *act, int pow, int range);
    bool set_aim(coord_def a);
    bool valid_aim(coord_def a);
    aff_type is_affected(coord_def loc);
private:
    explosion_map exp_map;
};

class targeter_passwall : public targeter_smite
{
public:
    targeter_passwall(int max_range);
    bool set_aim(coord_def a) override;
    bool valid_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
    bool can_affect_outside_range() override;
    bool can_affect_unseen() override;
    bool affects_monster(const monster_info& mon) override;

private:
    unique_ptr<passwall_path> cur_path;
};

class targeter_dig : public targeter_beam
{
public:
    targeter_dig(int max_range);
    bool valid_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
    bool can_affect_unseen() override;
    bool can_affect_walls() override;
    bool affects_monster(const monster_info& mon) override;
private:
    map<coord_def, int> aim_test_cache;
};

class targeter_overgrow: public targeter
{
public:
    targeter_overgrow();
    bool can_affect_walls() override { return true; }
    bool valid_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
    bool set_aim(coord_def a) override;
    set<coord_def> affected_positions;
private:
    bool overgrow_affects_pos(const coord_def &p);
};

class targeter_charge : public targeter
{
public:
    targeter_charge(const actor *act, int range);
    bool valid_aim(coord_def a) override;
    bool set_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
private:
    int range;
    vector<coord_def> path_taken; // Path the charge took.
};

// a fixed los targeter matching how it is called for shatter, with a custom
// tweak to affect walls.
class targeter_shatter : public targeter_radius
{
public:
    targeter_shatter(const actor *act) : targeter_radius(act, LOS_ARENA) { }
    bool can_affect_walls() override { return true; }
    aff_type is_affected(coord_def loc) override;
};

// A fixed targeter for multi-position attacks, i.e. los stuff that
// affects various squares, or monsters non-locally. For los stuff that
// affects only monsters on a per-monster case basis see targeter_multimonster
class targeter_multiposition : public targeter
{
public:
    targeter_multiposition(const actor *a, vector<coord_def> seeds,
                           aff_type _positive=AFF_MAYBE);
    targeter_multiposition(const actor *a, vector<monster *> seeds,
                           aff_type _positive=AFF_MAYBE);
    targeter_multiposition(const actor *a, initializer_list<coord_def> seeds,
                           aff_type _positive=AFF_MAYBE);

    void add_position(const coord_def &c, bool force=false);
    bool valid_aim(coord_def) override { return true; }
    bool can_affect_walls() override { return true; }
    aff_type is_affected(coord_def loc) override;

protected:
    set<coord_def> affected_positions;
    aff_type positive;
};

class targeter_scorch : public targeter_multiposition
{
public:
    targeter_scorch(const actor &a, int _range, bool affect_invis);
    bool valid_aim(coord_def c) override;

protected:
    int range;
};

class targeter_chain_lightning : public targeter
{
public:
    targeter_chain_lightning();
    bool valid_aim(coord_def) override { return true; }
    aff_type is_affected(coord_def loc) override;
private:
    set<coord_def> potential_victims;
    set<coord_def> closest_victims;
};

// A static targeter for Maxwell's Coupling
// that finds the closest monster using the absolute zero code.
class targeter_maxwells_coupling : public targeter_multiposition
{
public:
    targeter_maxwells_coupling();
};

class targeter_multifireball : public targeter_multiposition
{
public:
    targeter_multifireball(const actor *a, vector<coord_def> seeds);
};

// this is implemented a bit like multifireball, but with some tweaks
class targeter_walls : public targeter_multiposition
{
public:
    targeter_walls(const actor *a, vector<coord_def> seeds);

    aff_type is_affected(coord_def loc) override;
    bool can_affect_walls() override { return true; }
};

// a class for fixed beams at some offset from the player
class targeter_starburst_beam : public targeter_beam
{
public:
    targeter_starburst_beam(const actor *a, int _range, int pow, const coord_def &offset);
    // this is a bit of ui hack: lets us set starburst beams even when the
    // endpoint would be out of los
    bool can_affect_unseen() override { return true; }
    bool valid_aim(coord_def) override { return true; }
};

class targeter_starburst : public targeter
{
public:
    targeter_starburst(const actor *a, int range, int pow);
    bool valid_aim(coord_def) override { return true; }
    aff_type is_affected(coord_def loc) override;
    vector<targeter_starburst_beam> beams;
};

// A targeter for Eringya's Noxious Bog that finds cells that can be bogged.
class targeter_bog : public targeter_multiposition
{
public:
    targeter_bog(const actor *a, int pow);
};

class targeter_ignite_poison : public targeter_multiposition
{
public:
    targeter_ignite_poison(actor *a);
};

class targeter_multimonster : public targeter
{
public:
    targeter_multimonster(const actor *a);

    bool valid_aim(coord_def) override { return true; }
    aff_type is_affected(coord_def loc) override;
protected:
    bool check_monster;
};

class targeter_drain_life : public targeter_multimonster
{
public:
    targeter_drain_life();
    bool affects_monster(const monster_info& mon) override;
};

class targeter_discord : public targeter_multimonster
{
public:
    targeter_discord();
    bool affects_monster(const monster_info& mon) override;
};

class targeter_englaciate : public targeter_multimonster
{
public:
    targeter_englaciate();
    bool affects_monster(const monster_info& mon) override;
};

class targeter_fear : public targeter_multimonster
{
public:
    targeter_fear();
    bool affects_monster(const monster_info& mon) override;
};

class targeter_intoxicate : public targeter_multimonster
{
public:
    targeter_intoxicate();
    bool affects_monster(const monster_info& mon) override;
};

class targeter_anguish : public targeter_multimonster
{
public:
    targeter_anguish();
    bool affects_monster(const monster_info& mon) override;
};
class targeter_boulder : public targeter_beam
{
public:
    targeter_boulder(const actor* caster, int boulder_hp);
    bool valid_aim(coord_def a) override;
    bool set_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
private:
    int hp;
    map<coord_def, aff_type> boulder_sim;
};

class targeter_chain : public targeter_beam
{
public:
    targeter_chain(const actor *act, int r, zap_type ztype);
    bool set_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;

private:
    set<coord_def> chain_targ;
};

class targeter_bind_soul : public targeter_smite
{
public:
    targeter_bind_soul();
    bool valid_aim(coord_def a) override;
};

class targeter_explosive_beam : public targeter_beam
{
public:
    targeter_explosive_beam(const actor *act, zap_type ztype,
                            int pow, int range,
                            bool explode_on_monsters = true,
                            bool always_explode = false);
    bool set_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
private:
    explosion_map exp_map;
    bool explode_on_monsters;
    bool always_explode;
};

class targeter_galvanic : public targeter_beam
{
public:
    targeter_galvanic(const actor *act, int pow, int range);
    bool set_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
private:
    vector<coord_def> jolt_targets;
};

class targeter_gavotte : public targeter_beam
{
public:
    targeter_gavotte(const actor* caster);
    bool valid_aim(coord_def a) override;
    bool set_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
    bool harmful_to_player() override { return false; };
private:
    vector<coord_def> affected_monsters;
};

class targeter_magnavolt : public targeter_smite
{
public:
    targeter_magnavolt(const actor *act, int range);
    bool valid_aim(coord_def a) override;
    bool preferred_aim(coord_def a) override;
    bool set_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
private:
    vector<coord_def> beam_targets;
    vector<coord_def> beam_paths;
};

class targeter_mortar : public targeter_beam
{
public:
    targeter_mortar(const actor* act, int max_range);
    bool valid_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
    bool can_affect_unseen() override;
    bool can_affect_walls() override;
    bool affects_monster(const monster_info& mon) override;
};

class targeter_slouch : public targeter_radius
{
public:
    targeter_slouch();
    virtual aff_type is_affected(coord_def loc) override;
};

class targeter_marionette : public targeter_smite
{
public:
    targeter_marionette();
    bool valid_aim(coord_def a) override;
};

class targeter_putrefaction : public targeter_smite
{
public:
    targeter_putrefaction(int range);
    bool valid_aim(coord_def a) override;
};

class targeter_soul_splinter : public targeter_beam
{
public:
    targeter_soul_splinter(const actor *act, int r);
    bool affects_monster(const monster_info& mon) override;
};

class targeter_surprising_crocodile : public targeter_smite
{
public:
    targeter_surprising_crocodile(const actor* caster);
    bool valid_aim(coord_def a) override;
    bool set_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
private:
    vector<coord_def> landing_spots;
};

class targeter_wall_arc : public targeter_smite
{
public:
    targeter_wall_arc(const actor* caster, int size);
    bool set_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
private:
    int wall_num;
    vector<coord_def> spots;
};

class targeter_tempering : public targeter_smite
{
public:
    targeter_tempering();
    bool valid_aim(coord_def a) override;
    bool preferred_aim(coord_def a) override;
};

class targeter_piledriver : public targeter_smite
{
public:
    targeter_piledriver();
    bool valid_aim(coord_def a) override;
    bool set_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;

private:
    int piledriver_lengths[8];
    vector<coord_def> spots;
};

class targeter_teleport_other : public targeter_smite
{
public:
    targeter_teleport_other(const actor *act, int range);
    bool valid_aim(coord_def a) override;
};

class targeter_malign_gateway : public targeter
{
public:
    targeter_malign_gateway(actor& caster);
    aff_type is_affected(coord_def loc) override;
    bool valid_aim(coord_def) override { return true; }
};
