#include "AppHdr.h"

#include "target.h"

#include <cmath>
#include <utility> // swap

#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "fight.h"
#include "god-abil.h"
#include "god-passive.h"
#include "items.h"
#include "libutil.h"
#include "los-def.h"
#include "losglobal.h"
#include "mon-tentacle.h"
#include "religion.h"
#include "ray.h"
#include "spl-damage.h"
#include "spl-goditem.h" // player_is_debuffable
#include "spl-monench.h"
#include "spl-summoning.h"
#include "spl-other.h"
#include "spl-transloc.h"
#include "stringutil.h"
#include "terrain.h"

#define notify_fail(x) (why_not = (x), false)

static string _wallmsg(coord_def c)
{
    ASSERT(map_bounds(c)); // there'd be an information leak
    const char *wall = feat_type_name(env.grid(c));
    return "There is " + article_a(wall) + " there.";
}

bool targeter::set_aim(coord_def a)
{
    // This matches a condition in direction_chooser::move_is_ok().
    if (agent && !can_affect_unseen() &&
            !cell_see_cell(agent->pos(), a, LOS_NO_TRANS))
    {
        return false;
    }

    aim = a;
    return true;
}

bool targeter::preferred_aim(coord_def)
{
    return false;
}

bool targeter::can_affect_outside_range()
{
    return false;
}

bool targeter::can_affect_unseen()
{
    return false;
}

bool targeter::can_affect_walls()
{
    return false;
}

bool targeter::anyone_there(coord_def loc)
{
    if (!map_bounds(loc))
        return false;
    if (agent && agent->is_player())
        return env.map_knowledge(loc).monsterinfo();
    return actor_at(loc);
}

bool targeter::affects_monster(const monster_info& /*mon*/)
{
    return true; //TODO: false
}

bool targeter::harmful_to_player()
{
    return true; // The safest default assumption
}

static inline bool _ti_should_iterate(aff_type cur_aff, aff_type threshold)
{
    return cur_aff == AFF_NO
            || threshold != AFF_MAYBE && cur_aff == AFF_MAYBE;
}

targeting_iterator::targeting_iterator(targeter &t, aff_type _threshold)
            : rectangle_iterator(t.origin, get_los_radius(), true),
            tgt(t), threshold(_threshold)
{
    if (_ti_should_iterate(is_affected(), threshold))
        operator ++();
}

void targeting_iterator::operator ++()
{
    // superclass will still iterate if past the end; not sure why,
    // but mimic that behavior here
    aff_type cur_aff;
    do
    {
        rectangle_iterator::operator++();
        cur_aff = is_affected();
    }
    while (operator bool() && _ti_should_iterate(cur_aff, threshold));
}

aff_type targeting_iterator::is_affected()
{
    return in_bounds(**this) && operator bool()
                                        ? tgt.is_affected(**this) : AFF_NO;
}

// @param threshold AFF_YES: iterate over only AFF_YES squares. AFF_MAYBE:
//                  iterate over both AFF_YES and AFF_MAYBE squares.
targeting_iterator targeter::affected_iterator(aff_type threshold)
{
    return targeting_iterator(*this, threshold);
}

targeter_charge::targeter_charge(const actor *act, int r)
    : targeter(), range(r)
{
    ASSERT(act);
    ASSERT(r > 0);
    agent = act;
    obeys_mesmerise = true; // override superclass constructor
}

bool targeter_charge::valid_aim(coord_def a)
{
    string msg;

    coord_def landing = get_electric_charge_landing_spot(*agent, a, &msg);
    if (landing.origin())
        return notify_fail(msg);

    return true;
}

bool targeter_charge::set_aim(coord_def a)
{
    ray_def ray;
    if (!find_ray(agent->pos(), a, ray, opc_solid))
        return false;

    path_taken.clear();
    while (ray.advance())
    {
        path_taken.push_back(ray.pos());
        if (grid_distance(agent->pos(), ray.pos()) >= range || ray.pos() == a)
            break;
    }
    return true;
}

aff_type targeter_charge::is_affected(coord_def loc)
{
    bool in_path = false;
    for (coord_def a : path_taken)
    {
        if (a == loc)
        {
            in_path = true;
            break;
        }
    }
    if (!in_path)
        return AFF_NO;
    if (path_taken.size() >= 2 && loc == path_taken[path_taken.size() - 1])
        return AFF_MAYBE; // the target of the attack
    if (path_taken.size() >= 2 && loc == path_taken[path_taken.size() - 2])
        return AFF_LANDING;
    return AFF_YES; // a movement space
}

targeter_beam::targeter_beam(const actor *act, int r, zap_type zap,
                               int pow, int min_ex_rad, int max_ex_rad) :
                               min_expl_rad(min_ex_rad),
                               max_expl_rad(max_ex_rad),
                               range(r)
{
    ASSERT(act);
    ASSERT(min_ex_rad >= 0);
    ASSERT(max_ex_rad >= 0);
    ASSERT(max_ex_rad >= min_ex_rad);
    agent = act;
    beam.set_agent(act);
    origin = aim = act->pos();
    beam.attitude = ATT_FRIENDLY;
    zappy(zap, pow, false, beam);
    beam.is_tracer = true;
    beam.is_targeting = true;
    beam.range = range;
    beam.source = origin;
    beam.target = aim;
    beam.dont_stop_player = true;
    beam.friend_info.dont_stop = true;
    beam.foe_info.dont_stop = true;
    beam.ex_size = min_ex_rad;
    beam.aimed_at_spot = true;

    penetrates_targets = beam.pierce;
}

bool targeter_beam::set_aim(coord_def a)
{
    if (!targeter::set_aim(a))
        return false;

    bolt tempbeam = beam;

    tempbeam.target = aim;
    tempbeam.path_taken.clear();
    tempbeam.fire();
    path_taken = tempbeam.path_taken;

    if (max_expl_rad > 0)
        set_explosion_aim(beam);

    return true;
}

void targeter_beam::set_explosion_aim(bolt tempbeam)
{
    set_explosion_target(tempbeam);
    tempbeam.use_target_as_pos = true;
    exp_map_min.init(INT_MAX);
    tempbeam.determine_affected_cells(exp_map_min, coord_def(), 0,
                                      min_expl_rad, true, true);
    if (max_expl_rad == min_expl_rad)
        exp_map_max = exp_map_min;
    else
    {
        exp_map_max.init(INT_MAX);
        tempbeam.determine_affected_cells(exp_map_max, coord_def(), 0,
                                          max_expl_rad, true, true);
    }
}

void targeter_beam::set_explosion_target(bolt &tempbeam)
{
    tempbeam.target = origin;
    for (auto c : path_taken)
    {
        if (cell_is_solid(c) && !tempbeam.can_affect_wall(c))
            break;
        tempbeam.target = c;
        if (anyone_there(c) && !tempbeam.ignores_monster(monster_at(c)))
            break;
    }
}

bool targeter_beam::valid_aim(coord_def a)
{
    if (a != origin && !can_affect_unseen() &&
            !cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        if (agent->see_cell(a))
            return notify_fail("There's something in the way.");
        return notify_fail("You cannot see that place.");
    }
    if ((origin - a).rdist() > range)
        return notify_fail("Out of range.");
    return true;
}

bool targeter_beam::can_affect_outside_range()
{
    // XXX is this everything?
    return max_expl_rad > 0;
}

aff_type targeter_beam::is_affected(coord_def loc)
{
    bool on_path = false;
    int visit_count = 0;
    coord_def c;
    aff_type current = AFF_YES;
    for (auto pc : path_taken)
    {
        if (cell_is_solid(pc)
            && !beam.can_affect_wall(pc)
            && max_expl_rad > 0)
        {
            break;
        }

        c = pc;
        if (c == loc)
        {
            visit_count++;
            if (max_expl_rad > 0)
                on_path = true;
            else if (cell_is_solid(pc))
            {
                bool res = beam.can_affect_wall(pc);
                if (res)
                    return current;
                else
                    return AFF_NO;

            }
            else
                continue;
        }
        if (anyone_there(pc)
            && !penetrates_targets
            && !beam.ignores_monster(monster_at(pc)))
        {
            // We assume an exploding spell will always stop here.
            if (max_expl_rad > 0)
                break;
            current = AFF_MAYBE;
        }
    }
    if (max_expl_rad > 0)
    {
        if ((loc - c).rdist() <= 9)
        {
            bool aff_wall = beam.can_affect_wall(loc);
            if (!cell_is_solid(loc) || aff_wall)
            {
                coord_def centre(9,9);
                if (exp_map_min(loc - c + centre) < INT_MAX)
                {
                    return (!cell_is_solid(loc) || aff_wall)
                           ? AFF_YES : AFF_MAYBE;
                }
                if (exp_map_max(loc - c + centre) < INT_MAX)
                    return AFF_MAYBE;
            }
        }
        else
            return on_path ? AFF_TRACER : AFF_NO;
    }

    return visit_count == 0 ? AFF_NO :
           visit_count == 1 ? AFF_YES :
                              AFF_MULTIPLE;
}

bool targeter_beam::affects_monster(const monster_info& mon)
{
    //XXX: this is a disgusting hack that probably leaks information!
    //     bolt::is_harmless (and transitively, bolt::nasty_to) should
    //     take monster_infos instead.
    const monster* m = monster_at(mon.pos);
    if (!m)
        return false;

    if (beam.is_enchantment() && beam.has_saving_throw()
        && mon.willpower() == WILL_INVULN)
    {
        return false;
    }

    // beckoning is useless against adjacent mons!
    // XXX: this should probably be somewhere else
    if (beam.flavour == BEAM_BECKONING)
        return grid_distance(mon.pos, you.pos()) > 1;

    return !beam.is_harmless(m) || beam.nice_to(mon)
    // Inner flame affects allies without harming or helping them.
           || beam.flavour == BEAM_INNER_FLAME;
}

bool targeter_beam::harmful_to_player()
{
    return !beam.ignores_player() && !beam.harmless_to_player();
}

targeter_view::targeter_view()
{
    origin = aim = you.pos();
}

bool targeter_view::valid_aim(coord_def /*a*/)
{
    return true; // don't reveal map bounds
}

aff_type targeter_view::is_affected(coord_def loc)
{
    if (loc == aim)
        return AFF_YES;

    return AFF_NO;
}

targeter_smite::targeter_smite(const actor* act, int ran,
                                 int exp_min, int exp_max,
                                 bool harmless_to_player,
                                 bool wall_ok, bool monster_ok,
                                 bool (*affects_pos_func)(const coord_def &)):
    exp_range_min(exp_min), exp_range_max(exp_max), range(ran),
    cannot_harm_player(harmless_to_player), affects_walls(wall_ok),
    can_target_monsters(monster_ok),
    affects_pos(affects_pos_func)
{
    ASSERT(act);
    ASSERT(exp_min >= 0);
    ASSERT(exp_max >= 0);
    ASSERT(exp_min <= exp_max);
    agent = act;
    origin = aim = act->pos();
}

bool targeter_smite::valid_aim(coord_def a)
{
    if (a != origin && !cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        // Scrying/glass/tree/grate.
        if (agent && agent->see_cell(a))
            return notify_fail("There's something in the way.");
        else if (agent && agent->is_player())
            return notify_fail("You cannot see that place.");
        // When aiming something from a different actor's perspective
        else
            return notify_fail("Out of range.");
    }
    if ((origin - a).rdist() > range)
        return notify_fail("Out of range.");
    if (!affects_walls && cell_is_solid(a))
        return notify_fail(_wallmsg(a));
    if (!can_target_monsters && monster_at(a) && you.can_see(*monster_at(a))
        // XXX: To let Paragon Tempest be cast without moving the Paragon.
        && monster_at(a) != agent)
    {
        return notify_fail("There's something in the way.");
    }
    return true;
}

bool targeter_smite::set_aim(coord_def a)
{
    if (!targeter::set_aim(a))
        return false;

    if (exp_range_max > 0)
    {
        bolt beam;
        beam.target = a;
        beam.use_target_as_pos = true;
        exp_map_min.init(INT_MAX);
        beam.determine_affected_cells(exp_map_min, coord_def(), 0,
                                      exp_range_min, true, true);
        if (exp_range_min == exp_range_max)
            exp_map_max = exp_map_min;
        else
        {
            exp_map_max.init(INT_MAX);
            beam.determine_affected_cells(exp_map_max, coord_def(), 0,
                    exp_range_max, true, true);
        }
    }
    return true;
}

bool targeter_smite::can_affect_outside_range()
{
    // XXX is this everything?
    return exp_range_max > 0;
}

bool targeter_smite::can_affect_walls()
{
    return affects_walls;
}

bool targeter_smite::harmful_to_player()
{
    return !cannot_harm_player;
}

aff_type targeter_smite::is_affected(coord_def loc)
{
    if (!valid_aim(aim))
        return AFF_NO;

    if (affects_pos && !affects_pos(loc))
        return AFF_NO;

    if (loc == aim)
        return AFF_YES;

    if (exp_range_max <= 0)
        return AFF_NO;

    if ((loc - aim).rdist() > 9)
        return AFF_NO;
    coord_def centre(9,9);
    if (exp_map_min(loc - aim + centre) < INT_MAX)
        return AFF_YES;
    if (exp_map_max(loc - aim + centre) < INT_MAX)
        return AFF_MAYBE;

    return AFF_NO;
}

targeter_walljump::targeter_walljump() :
    targeter_smite(&you, LOS_RADIUS, 1, 1)
{
}

bool targeter_walljump::valid_aim(coord_def a)
{
    return wu_jian_can_wall_jump(a, why_not);
}

aff_type targeter_walljump::is_affected(coord_def loc)
{
    if (!valid_aim(aim))
        return AFF_NO;

    auto wall_jump_direction = (you.pos() - aim).sgn();
    auto wall_jump_landing_spot = (you.pos() + wall_jump_direction
                                   + wall_jump_direction);
    if (loc == wall_jump_landing_spot)
        return AFF_YES;

    auto d = loc.distance_from(wall_jump_landing_spot);
    if (d == 1 && wu_jian_wall_jump_monster_at(loc))
        return AFF_YES;

    return AFF_NO;
}

targeter_passwall::targeter_passwall(int max_range) :
    targeter_smite(&you, max_range, 1, 1, true, true)
{
}

bool targeter_passwall::valid_aim(coord_def a)
{
    passwall_path tmp_path(you, a - you.pos(), range);
    string failmsg;
    tmp_path.is_valid(&failmsg);
    if (!tmp_path.spell_succeeds())
        return notify_fail(failmsg);
    return true;
}

bool targeter_passwall::set_aim(coord_def a)
{
    cur_path = make_unique<passwall_path>(you, a - you.pos(), range);
    return true;
}

aff_type targeter_passwall::is_affected(coord_def loc)
{
    if (!cur_path)
        return AFF_NO;
    // not very efficient...
    for (auto p : cur_path->path)
        if (p == loc)
            return AFF_YES;
    return AFF_NO;
}

bool targeter_passwall::can_affect_outside_range()
{
    return true;
}

bool targeter_passwall::can_affect_unseen()
{
    return true;
}

bool targeter_passwall::affects_monster(const monster_info& /*mon*/)
{
    return false;
}

targeter_dig::targeter_dig(int max_range) :
    targeter_beam(&you, max_range, ZAP_DIG, 0, 0, 0)
{
}

bool targeter_dig::valid_aim(coord_def a)
{
    if (a == origin)
        return notify_fail("Please select a direction to dig.");
    if ((origin - a).rdist() > range || !in_bounds(a))
        return notify_fail("Out of range.");
    int possible_squares_affected;
    if (aim_test_cache.count(a))
        possible_squares_affected = aim_test_cache[a];
    else
    {
        // TODO: maybe shouldn't use set_aim? ugly side-effect, but it does take
        // care of all the beam management.
        if (!set_aim(a))
            possible_squares_affected = -1; // can't happen?
        else
        {
            possible_squares_affected = 0;
            for (auto p : path_taken)
                if (beam.can_affect_wall(p) ||
                        in_bounds(p) && env.map_knowledge(p).feat() == DNGN_UNSEEN)
                {
                    possible_squares_affected++;
                }
        }
        aim_test_cache[a] = possible_squares_affected;
    }
    if (possible_squares_affected == 0)
        return notify_fail("Digging in that direction won't affect any walls.");
    else if (possible_squares_affected < 0)
        return false;
    else
        return true;
}

bool targeter_dig::can_affect_unseen()
{
    return true;
}

bool targeter_dig::affects_monster(const monster_info& /*mon*/)
{
    return false;
}

bool targeter_dig::can_affect_walls()
{
    return true;
}

aff_type targeter_dig::is_affected(coord_def loc)
{
    aff_type current = AFF_YES;
    bool hit_barrier = false;
    for (auto pc : path_taken)
    {
        if (hit_barrier)
            return AFF_NO; // some previous iteration hit a barrier
        current = AFF_YES;
        // uses comparison to DNGN_UNSEEN so that this works sensibly with magic
        // mapping etc. TODO: console tracers use the same symbol/color as
        // mmapped walls.
        if (in_bounds(pc) && env.map_knowledge(pc).feat() != DNGN_UNSEEN)
        {
            if (!cell_is_solid(pc))
                current = AFF_TRACER;
            else if (!beam.can_affect_wall(pc))
            {
                current = AFF_TRACER; // show tracer at the barrier cell
                hit_barrier = true;
            }
            // otherwise, default to AFF_YES
        }
        // unseen squares default to AFF_YES
        if (pc == loc)
            return current;
    }
    // path never intersected loc at all
    return AFF_NO;
}

targeter_transference::targeter_transference(const actor* act, int aoe) :
    targeter_smite(act, LOS_RADIUS, aoe, aoe, true)
{
}

bool targeter_transference::valid_aim(coord_def a)
{
    if (!targeter_smite::valid_aim(a))
        return false;

    const actor *victim = actor_at(a);
    if (victim && you.can_see(*victim))
    {
        if (mons_is_hepliaklqana_ancestor(victim->type))
        {
            return notify_fail("You can't transfer your ancestor with "
                               + victim->pronoun(PRONOUN_REFLEXIVE) + ".");
        }
        if (mons_is_tentacle_or_tentacle_segment(victim->type)
            || victim->is_stationary())
        {
            return notify_fail("You can't transfer that.");
        }
    }
    return true;
}

bool targeter_transference::affects_monster(const monster_info& mon)
{
    return !mons_is_hepliaklqana_ancestor(mon.type)
            && !mons_class_is_stationary(mon.type)
            && !mons_is_tentacle_or_tentacle_segment(mon.type);
}

targeter_permafrost::targeter_permafrost(const actor &act, int power) :
    targeter_smite(&act)
{
    possible_centres = permafrost_targets(act, power, false);
    for (coord_def t : possible_centres)
    {
        targets.insert(t);
        for (adjacent_iterator ai(t); ai; ++ai)
            if (!cell_is_solid(*ai))
                targets.insert(*ai);
    }
    single_target = possible_centres.size() <= 1;
}

aff_type targeter_permafrost::is_affected(coord_def loc)
{
    if (targets.count(loc))
    {
        if (possible_centres.count(loc))
            return single_target ? AFF_MULTIPLE : AFF_YES;
        return single_target ? AFF_YES : AFF_MAYBE;
    }
    return AFF_NO;
}

targeter_inner_flame::targeter_inner_flame(const actor* act, int r) :
    targeter_smite(act, r)
{
}

bool targeter_inner_flame::valid_aim(coord_def a)
{
    if (!targeter_smite::valid_aim(a))
        return false;

    if (!mons_inner_flame_immune_reason(monster_at(a)).empty())
        return notify_fail(mons_inner_flame_immune_reason(monster_at(a)));

    return true;
}

targeter_simulacrum::targeter_simulacrum(const actor* act, int r) :
    targeter_smite(act, r)
{
}

bool targeter_simulacrum::valid_aim(coord_def a)
{
    if (!targeter_smite::valid_aim(a))
        return false;

    if (!mons_simulacrum_immune_reason(monster_at(a)).empty())
        return notify_fail(mons_simulacrum_immune_reason(monster_at(a)));

    return true;
}

targeter_unravelling::targeter_unravelling()
    : targeter_smite(&you, LOS_RADIUS, 1, 1)
{
}

/**
 * Will a casting of Violent Unravelling explode a target at the given loc?
 *
 * @param c     The location in question.
 * @return      Whether, to the player's knowledge, there's a valid target for
 *              Violent Unravelling at the given coordinate.
 */
static bool _unravelling_explodes_at(const coord_def c)
{
    if (you.pos() == c && player_is_debuffable())
        return true;

    const monster_info* mi = env.map_knowledge(c).monsterinfo();
    return mi && mi->unravellable();
}

bool targeter_unravelling::valid_aim(coord_def a)
{
    if (!targeter_smite::valid_aim(a))
        return false;

    const monster* mons = monster_at(a);
    if (mons && you.can_see(*mons) && !_unravelling_explodes_at(a))
    {
        return notify_fail(mons->name(DESC_THE) + " has no magical effects to "
                           "unravel.");
    }

    if (mons && you.can_see(*mons) && _unravelling_explodes_at(a)
        && never_harm_monster(&you, mons))
    {
        return notify_fail("You cannot do harm to " + mons->name(DESC_THE));
    }

    return true;
}

bool targeter_unravelling::set_aim(coord_def a)
{
    if (!targeter::set_aim(a))
        return false;

    if (_unravelling_explodes_at(a))
    {
        exp_range_min = 1;
        exp_range_max = 1;
    }
    else
    {
        exp_range_min = exp_range_max = 0;
        return false;
    }

    bolt beam;
    beam.target = a;
    beam.use_target_as_pos = true;
    exp_map_min.init(INT_MAX);
    beam.determine_affected_cells(exp_map_min, coord_def(), 0,
                                  exp_range_min, true, true);
    exp_map_max = exp_map_min;

    return true;
}

targeter_airstrike::targeter_airstrike()
{
    agent = &you;
    origin = aim = you.pos();
}

aff_type targeter_airstrike::is_affected(coord_def loc)
{
    if (loc == aim)
    {
        // Show how much bonus damage airstrike will do
        // against the target.
        const int space = airstrike_space_around(aim, false);
        if (space <= 3)
            return AFF_MAYBE;
        if (space < 6)
            return AFF_YES;
        return AFF_MULTIPLE;
    }

    // Show the surrounding empty spaces.
    if (!adjacent(loc, aim) || you.pos() == loc)
        return AFF_NO;
    const auto knowledge = env.map_knowledge(loc);
    if (!knowledge.seen()
        || feat_is_solid(knowledge.feat())
        || env.map_knowledge(loc).monsterinfo())
    {
        return AFF_NO;
    }
    return AFF_LANDING;

}

bool targeter_airstrike::valid_aim(coord_def a)
{
    // XXX: copied from targeter_beam :(
    if (a != origin && !cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        if (you.see_cell(a))
            return notify_fail("There's something in the way.");
        return notify_fail("You cannot see that place.");
    }
    return true;
}

targeter_passage::targeter_passage(int _range)
    : targeter_smite(&you, _range)
{ }

aff_type targeter_passage::is_affected(coord_def loc)
{
    if (!valid_aim(aim))
        return AFF_NO;

    if (golubria_valid_cell(loc, true))
    {
        bool p1 = grid_distance(loc, origin) <= GOLUBRIA_FUZZ_RANGE;
        bool p2 = grid_distance(loc, aim) <= GOLUBRIA_FUZZ_RANGE
                  && loc != you.pos();

        if (p1 && p2)
            return AFF_MULTIPLE;
        else if (p1 || p2)
            return AFF_MAYBE;
    }

    return AFF_NO;
}

targeter_fragment::targeter_fragment(const actor* act, int power, int ran) :
    targeter_smite(act, ran, 1, 1, false, true),
    pow(power)
{
}

bool targeter_fragment::valid_aim(coord_def a)
{
    if (!targeter_smite::valid_aim(a))
        return false;

    bolt tempbeam;
    bool temp;
    if (!setup_fragmentation_beam(tempbeam, pow, agent, a, true, nullptr, temp))
        return notify_fail("You cannot affect that.");
    return true;
}

bool targeter_fragment::set_aim(coord_def a)
{
    if (!targeter::set_aim(a))
        return false;

    bolt tempbeam;
    bool temp;

    if (setup_fragmentation_beam(tempbeam, pow, agent, a, true, nullptr, temp))
    {
        exp_range_min = tempbeam.ex_size;
        exp_range_max = tempbeam.ex_size;
    }
    else
    {
        exp_range_min = exp_range_max = 0;
        return false;
    }

    tempbeam.use_target_as_pos = true;
    exp_map_min.init(INT_MAX);
    tempbeam.determine_affected_cells(exp_map_min, coord_def(), 0,
            exp_range_min, true, true);

    // Min and max ranges are always identical.
    exp_map_max = exp_map_min;

    return true;
}

targeter_reach::targeter_reach(const actor* act, reach_type ran) :
    range(ran)
{
    ASSERT(act);
    agent = act;
    origin = aim = act->pos();
}

bool targeter_reach::valid_aim(coord_def a)
{
    if (!cell_see_cell(origin, a, LOS_DEFAULT))
        return notify_fail("You cannot see that place.");
    if (!agent->see_cell_no_trans(a))
        return notify_fail("You can't get through.");

    int dist = (origin - a).rdist();

    if (dist > range)
        return notify_fail("Your weapon can't reach that far!");

    return true;
}

aff_type targeter_reach::is_affected(coord_def loc)
{
    if (!valid_aim(loc))
        return AFF_NO;

    if (loc == aim)
        return AFF_YES;

    // hacks: REACH_THREE entails smite targeting, because it exists entirely
    // for the sake of UNRAND_RIFT. So, don't show the tracer.
    if (range == REACH_TWO
        && ((loc - origin) * 2 - (aim - origin)).abs() < 1
        && feat_is_reachable_past(env.grid(loc)))
    {
        return AFF_TRACER;
    }

    return AFF_NO;
}

targeter_cleave::targeter_cleave(const actor* act, coord_def target, int rng)
{
    ASSERT(act);
    agent = act;
    origin = act->pos();
    range = rng;
    set_aim(target);
}

bool targeter_cleave::valid_aim(coord_def a)
{
    const coord_def delta = a - origin;
    if (delta.rdist() > range)
        return notify_fail("Your weapon can't reach that far!");
    if (range == 2)
    {
        const coord_def first_middle(origin + delta / 2);
        const coord_def second_middle(a - delta / 2);
        if (!feat_is_reachable_past(env.grid(first_middle))
             && !feat_is_reachable_past(env.grid(second_middle)))
        {
            return notify_fail("There's something in the way.");
        }
    }
    return true;
}


bool targeter_cleave::set_aim(coord_def target)
{
    aim = target;
    targets.clear();
    list<actor*> act_targets;
    get_cleave_targets(*agent, target, act_targets);
    while (!act_targets.empty())
    {
        actor *potential_target = act_targets.front();
        if (agent->can_see(*potential_target))
            targets.insert(potential_target->pos());
        act_targets.pop_front();
    }
    return true;
}

aff_type targeter_cleave::is_affected(coord_def loc)
{
    return targets.count(loc) ? AFF_YES : AFF_NO;
}

targeter_cloud::targeter_cloud(const actor* act, cloud_type ct, int r,
                               int count_min, int count_max) :
    ctype(ct), range(r), cnt_min(count_min), cnt_max(count_max)
{
    ASSERT(cnt_min > 0);
    ASSERT(cnt_max > 0);
    ASSERT(cnt_min <= cnt_max);
    agent = act;
    if (agent)
        origin = aim = act->pos();
}

static bool _cloudable(coord_def loc, cloud_type ctype)
{
    const cloud_struct *cloud = cloud_at(loc);
    return in_bounds(loc)
           && !cell_is_solid(loc)
           && (!cloud || cloud_is_stronger(ctype, *cloud))
           && (!is_sanctuary(loc) || is_harmless_cloud(ctype))
           && cell_see_cell(you.pos(), loc, LOS_NO_TRANS);
}

bool targeter_cloud::valid_aim(coord_def a)
{
    if (agent && (origin - a).rdist() > range)
        return notify_fail("Out of range.");
    if (!map_bounds(a)
        || agent
           && origin != a
           && !cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        // Scrying/glass/tree/grate.
        if (agent && agent->see_cell(a))
            return notify_fail("There's something in the way.");
        return notify_fail("You cannot see that place.");
    }
    if (cell_is_solid(a))
        return notify_fail(_wallmsg(a));
    if (agent)
    {
        const cloud_struct *cloud = cloud_at(a);
        if (cloud && !cloud_is_stronger(ctype, *cloud))
            return notify_fail("There's already a cloud there.");
        else if (is_sanctuary(a) && !is_harmless_cloud(ctype))
        {
            return notify_fail("You can't place harmful clouds in a "
                               "sanctuary.");
        }
        ASSERT(_cloudable(a, ctype));
    }
    return true;
}

bool targeter_cloud::set_aim(coord_def a)
{
    if (!targeter::set_aim(a))
        return false;

    seen.clear();
    queue.clear();
    queue.emplace_back();

    int placed = 0;
    queue[0].push_back(a);

    for (unsigned int d1 = 0; d1 < queue.size() && placed < cnt_max; d1++)
    {
        placed += queue[d1].size();

        for (coord_def c : queue[d1])
        {
            for (adjacent_iterator ai(c); ai; ++ai)
                if (_cloudable(*ai, ctype) && !seen.count(*ai))
                {
                    unsigned int d2 = d1 + 1;
                    if (d2 >= queue.size())
                        queue.resize(d2 + 1);
                    queue[d2].push_back(*ai);
                    seen[*ai] = AFF_TRACER;
                }

            seen[c] = placed <= cnt_min ? AFF_YES : AFF_MAYBE;
        }
    }

    return true;
}

bool targeter_cloud::can_affect_outside_range()
{
    return true;
}

aff_type targeter_cloud::is_affected(coord_def loc)
{
    if (!valid_aim(aim))
        return AFF_NO;

    if (aff_type *aff = map_find(seen, loc))
    {
        if (*aff > 0) // AFF_TRACER is used privately
            return *aff;
    }
    return AFF_NO;
}


targeter_splash::targeter_splash(const actor *act, int r, int pow)
    : targeter_beam(act, r, ZAP_COMBUSTION_BREATH, pow, 0, 0)
{
}

aff_type targeter_splash::is_affected(coord_def loc)
{
    bool on_path = false;
    coord_def c;
    for (auto pc : path_taken)
    {
        if (cell_is_solid(pc))
            break;

        c = pc;
        if (pc == loc)
            on_path = true;

        if (anyone_there(pc) && !beam.ignores_monster(monster_at(pc)))
            break;
    }

    if (loc == c)
        return AFF_YES;

    // self-spit doesn't splash
    if (aim == origin)
        return AFF_NO;

    // it splashes around only upon hitting someone
    if (anyone_there(c))
    {
        if (grid_distance(loc, c) > 1)
            return on_path ? AFF_YES : AFF_NO;

        // you're safe from being splashed by own spit
        if (loc == origin)
            return AFF_NO;

        return anyone_there(loc) ? AFF_YES : AFF_MAYBE;
    }

    return on_path ? AFF_YES : AFF_NO;
}

targeter_radius::targeter_radius(const actor *act, los_type _los,
                             int ran, int ran_max, int ran_min, int ran_maybe):
    range(ran), range_max(ran_max), range_min(ran_min), range_maybe(ran_maybe)
{
    ASSERT(act);
    agent = act;
    origin = aim = act->pos();
    los = _los;
    if (!range_max)
        range_max = range;
    if (!range_maybe)
        range_maybe = range;
    ASSERT(range_max >= range);
}

bool targeter_radius::valid_aim(coord_def a)
{
    if ((a - origin).rdist() > range_max || (a - origin).rdist() < range_min)
        return notify_fail("Out of range.");
    // If this message ever becomes used, please improve it. I did not
    // bother adding complexity just for monsters and "hit allies" prompts
    // which don't need it.
    if (!is_affected(a))
        return notify_fail("The effect is blocked.");
    return true;
}

aff_type targeter_radius::is_affected(coord_def loc)
{
    if ((loc - origin).rdist() > range_max
        || (loc - origin).rdist() < range_min)
    {
        return AFF_NO;
    }

    if (!cell_see_cell(loc, origin, los))
        return AFF_NO;

    if ((loc - origin).rdist() > range_maybe)
        return AFF_MAYBE;

    return AFF_YES;
}

targeter_flame_wave::targeter_flame_wave(int _range)
    : targeter_radius(&you, LOS_NO_TRANS, _range, 0, 1)
{ }

aff_type targeter_flame_wave::is_affected(coord_def loc)
{
    const aff_type base_aff = targeter_radius::is_affected(loc);
    if (base_aff == AFF_NO)
        return AFF_NO;
    const int dist = (loc - origin).rdist();
    if (dist == 1)
        return AFF_YES;
    return AFF_MAYBE;
}

targeter_siphon_essence::targeter_siphon_essence()
    : targeter_radius(&you, LOS_NO_TRANS, 2, 0, 1)
{ }

aff_type targeter_siphon_essence::is_affected(coord_def loc)
{
    const aff_type base_aff = targeter_radius::is_affected(loc);
    if (base_aff == AFF_NO)
        return AFF_NO;
    monster* mons = monster_at(loc);
    if (!mons || !you.can_see(*mons))
        return AFF_MAYBE;
    if (!siphon_essence_affects(*mons))
        return AFF_NO;
    return AFF_YES;
}

aff_type targeter_shatter::is_affected(coord_def loc)
{
    if (loc == origin)
        return AFF_NO; // Shatter doesn't affect the caster.

    if (!cell_see_cell(loc, origin, LOS_ARENA))
        return AFF_NO; // No shattering through glass... without work.

    monster* mons = monster_at(loc);
    if (!mons || !you.can_see(*mons))
    {
        const int terrain_chance = terrain_shatter_chance(loc, you);
        if (terrain_chance == 100)
            return AFF_YES;
        else if (terrain_chance > 0)
            return AFF_MAYBE;
        return AFF_NO;
    }

    const dice_def dam = shatter_damage(200, mons);
    const int dice = dam.num;
    if (dice == DEFAULT_SHATTER_DICE)
        return AFF_YES;
    if (dice > DEFAULT_SHATTER_DICE)
        return AFF_MULTIPLE;
    return AFF_MAYBE;
}

targeter_thunderbolt::targeter_thunderbolt(const actor *act, int r,
                                             coord_def _prev)
{
    ASSERT(act);
    agent = act;
    origin = act->pos();
    prev = _prev;
    ASSERT(prev != origin);
    aim = prev.origin() ? origin : prev;
    range = r;
}

bool targeter_thunderbolt::valid_aim(coord_def a)
{
    if (a != origin && !cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        // Scrying/glass/tree/grate.
        if (agent->see_cell(a))
            return notify_fail("There's something in the way.");
        return notify_fail("You cannot see that place.");
    }
    if ((origin - a).rdist() > range)
        return notify_fail("Out of range.");
    return true;
}

static void _make_ray(ray_def &ray, coord_def a, coord_def b)
{
    // Like beams, we need to allow picking the "better" ray if one is blocked
    // by a wall.
    if (!find_ray(a, b, ray, opc_solid_see))
        ray = ray_def(geom::ray(a.x + 0.5, a.y + 0.5, b.x - a.x, b.y - a.y));
}

static bool left_of(coord_def a, coord_def b)
{
    return a.x * b.y > a.y * b.x;
}

bool targeter_thunderbolt::set_aim(coord_def a)
{
    aim = a;
    zapped.clear();

    if (a == origin)
        return false;

    arc_length.init(0);

    ray_def ray;
    coord_def p; // ray.pos() does lots of processing, cache it

    // For consistency with beams, we need to
    _make_ray(ray, origin, aim);
    while ((origin - (p = ray.pos())).rdist() <= range
           && map_bounds(p) && opc_solid_see(p) < OPC_OPAQUE)
    {
        if (p != origin && zapped[p] <= 0)
        {
            zapped[p] = AFF_YES;
            arc_length[origin.distance_from(p)]++;
        }
        ray.advance();
    }

    if (prev.origin())
        return true;

    _make_ray(ray, origin, prev);
    while ((origin - (p = ray.pos())).rdist() <= range
           && map_bounds(p) && opc_solid_see(p) < OPC_OPAQUE)
    {
        if (p != origin && zapped[p] <= 0)
        {
            zapped[p] = AFF_MAYBE; // fully affected, we just want to highlight cur
            arc_length[origin.distance_from(p)]++;
        }
        ray.advance();
    }

    coord_def a1 = prev - origin;
    coord_def a2 = aim - origin;
    if (left_of(a2, a1))
        swap(a1, a2);

    for (int x = -LOS_RADIUS; x <= LOS_RADIUS; ++x)
        for (int y = -LOS_RADIUS; y <= LOS_RADIUS; ++y)
        {
            if (max(abs(x), abs(y)) > range)
                continue;
            coord_def r(x, y);
            if (left_of(a1, r) && left_of(r, a2))
            {
                (p = r) += origin;
                if (!zapped.count(p))
                    arc_length[r.rdist()]++;
                if (zapped[p] <= 0 && cell_see_cell(origin, p, LOS_NO_TRANS))
                    zapped[p] = AFF_MAYBE;
            }
        }

    zapped[origin] = AFF_NO;

    return true;
}

aff_type targeter_thunderbolt::is_affected(coord_def loc)
{
    if (loc == aim)
        return zapped[loc] ? AFF_YES : AFF_TRACER;

    if ((loc - origin).rdist() > range)
        return AFF_NO;

    return zapped[loc];
}

aff_type targeter_refrig::is_affected(coord_def loc)
{
    if (!targeter_radius::is_affected(loc))
        return AFF_NO;
    const actor* act = actor_at(loc);
    if (!act || act == agent || !agent->can_see(*act))
        return AFF_NO;
    if (never_harm_monster(agent, act->as_monster()))
        return AFF_NO;
    switch (adjacent_huddlers(loc, true))
    {
    case 0:  return AFF_MULTIPLE;
    case 1:  return AFF_YES;
    default: return AFF_MAYBE;
    }
}

targeter_cone::targeter_cone(const actor *act, int r)
{
    ASSERT(act);
    agent = act;
    origin = act->pos();
    aim = origin;
    ASSERT_RANGE(r, 1 + 1, you.current_vision + 1);
    range = r;
}

bool targeter_cone::valid_aim(coord_def a)
{
    if (a != origin && !cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        // Scrying/glass/tree/grate.
        if (agent->see_cell(a))
            return notify_fail("There's something in the way.");
        return notify_fail("You cannot see that place.");
    }
    if ((origin - a).rdist() > range)
        return notify_fail("Out of range.");
    return true;
}

static bool left_of_eq(coord_def a, coord_def b)
{
    return a.x * b.y >= a.y * b.x;
}

// Ripped off from targeter_thunderbolt::set_aim.
bool targeter_cone::set_aim(coord_def a)
{
    aim = a;
    zapped.clear();
    for (int i = 0; i < LOS_RADIUS + 1; i++)
        sweep[i].clear();

    if (a == origin)
        return false;

    const coord_def delta = a - origin;
    const double arc = PI/4;
    coord_def l, r;
    l.x = origin.x + (cos(-arc) * delta.x - sin(-arc) * delta.y + 0.5);
    l.y = origin.y + (sin(-arc) * delta.x + cos(-arc) * delta.y + 0.5);
    r.x = origin.x + (cos( arc) * delta.x - sin( arc) * delta.y + 0.5);
    r.y = origin.y + (sin( arc) * delta.x + cos( arc) * delta.y + 0.5);

    coord_def p;

    coord_def a1 = l - origin;
    coord_def a2 = r - origin;
    if (left_of(a2, a1))
        swap(a1, a2);

    for (int x = -LOS_RADIUS; x <= LOS_RADIUS; ++x)
        for (int y = -LOS_RADIUS; y <= LOS_RADIUS; ++y)
        {
            if (max(abs(x), abs(y)) > range)
                continue;
            coord_def q(x, y);
            if (left_of_eq(a1, q) && left_of_eq(q, a2))
            {
                (p = q) += origin;
                if (zapped[p] <= 0
                    && map_bounds(p)
                    && opc_solid_see(p) < OPC_OPAQUE
                    && cell_see_cell(origin, p, LOS_NO_TRANS))
                {
                    zapped[p] = AFF_YES;
                    sweep[(origin - p).rdist()][p] = AFF_YES;
                }
            }
        }

    zapped[origin] = AFF_NO;
    sweep[0].clear();

    return true;
}

aff_type targeter_cone::is_affected(coord_def loc)
{
    if (loc == aim)
        return zapped[loc] ? AFF_YES : AFF_TRACER;

    if ((loc - origin).rdist() > range)
        return AFF_NO;

    return zapped[loc];
}

targeter_monster_sequence::targeter_monster_sequence(const actor *act, int pow, int r) :
                          targeter_beam(act, r, ZAP_DEBUGGING_RAY, pow, 0, 0)
{
    // for `path_taken` to be set properly, the beam needs to be piercing, and
    // ZAP_DEBUGGING_RAY is not.
    beam.pierce = true;
}

bool targeter_monster_sequence::set_aim(coord_def a)
{
    if (!targeter_beam::set_aim(a))
        return false;

    bolt tempbeam = beam;
    tempbeam.target = origin;
    bool last_cell_has_mons = true;
    bool passed_through_mons = false;
    for (auto c : path_taken)
    {
        if (!last_cell_has_mons)
            return false; // we must have an uninterrupted chain of monsters

        if (cell_is_solid(c))
            break;

        tempbeam.target = c;
        if (anyone_there(c))
        {
            passed_through_mons = true;
            tempbeam.use_target_as_pos = true;
            exp_map.init(INT_MAX);
            tempbeam.determine_affected_cells(exp_map, coord_def(), 0,
                                              0, true, true);
        }
        else
            last_cell_has_mons = false;
    }

    return passed_through_mons;
}

bool targeter_monster_sequence::valid_aim(coord_def a)
{
    if (!targeter_beam::set_aim(a))
        return false;

    bool last_cell_has_mons = true;
    bool passed_through_mons = false;
    for (auto c : path_taken)
    {
        if (!last_cell_has_mons)
            return false; // we must have an uninterrupted chain of monsters

        if (cell_is_solid(c))
            return false;

        if (!anyone_there(c))
            last_cell_has_mons = false;
        else
            passed_through_mons = true;
    }

    return passed_through_mons;
}

aff_type targeter_monster_sequence::is_affected(coord_def loc)
{
    bool on_path = false;
    for (auto c : path_taken)
    {
        if (cell_is_solid(c))
            break;
        if (c == loc)
            on_path = true;
        if (anyone_there(c)
            && !beam.ignores_monster(monster_at(c))
            && (loc - c).rdist() <= 9)
        {
            coord_def centre(9,9);
            if (exp_map(loc - c + centre) < INT_MAX
                && !cell_is_solid(loc))
            {
                return AFF_YES;
            }
        }
    }

    return on_path ? AFF_TRACER : AFF_NO;
}

targeter_overgrow::targeter_overgrow()
{
    agent = &you;
    origin = you.pos();
}

bool targeter_overgrow::overgrow_affects_pos(const coord_def &p)
{
    if (!in_bounds(p))
        return false;
    if (env.markers.property_at(p, MAT_ANY, "veto_destroy") == "veto")
        return false;

    const dungeon_feature_type feat = env.grid(p);
    if (feat_is_open_door(feat))
    {
        const monster* const mons = monster_at(p);
        if (mons && agent && agent->can_see(*mons))
            return false;

        return true;
    }

    return feat_is_diggable(feat)
        || feat_is_closed_door(feat)
        || feat_is_tree(feat)
        || (feat_is_wall(feat) && !feat_is_permarock(feat));
}

aff_type targeter_overgrow::is_affected(coord_def loc)
{
    if (affected_positions.count(loc))
        return AFF_YES;

    return AFF_NO;
}

bool targeter_overgrow::valid_aim(coord_def a)
{
    if (a != origin && !cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        if (agent && agent->see_cell(a))
            return notify_fail("There's something in the way.");
        return notify_fail("You cannot see that place.");
    }

    if (!overgrow_affects_pos(a))
        return notify_fail("You cannot grow anything here.");

    return true;
}

bool targeter_overgrow::set_aim(coord_def a)
{
    affected_positions.clear();

    if (!targeter::set_aim(a))
        return false;

    if (overgrow_affects_pos(a))
        affected_positions.insert(a);
    else
        return false;

    for (adjacent_iterator ai(a, true); ai; ++ai)
    {
        if (overgrow_affects_pos(*ai) && you.see_cell_no_trans(*ai))
            affected_positions.insert(*ai);
    }

    return affected_positions.size();
}

targeter_multiposition::targeter_multiposition(const actor *a,
            vector<coord_def> seeds, aff_type _positive)
    : targeter(), positive(_positive)
{
    agent = a;
    if (agent)
        origin = agent->pos();
    for (auto &c : seeds)
        affected_positions.insert(c);
}

targeter_multiposition::targeter_multiposition(const actor *a,
            vector<monster *> seeds, aff_type _positive)
    : targeter(), positive(_positive)
{
    agent = a;
    if (agent)
        origin = agent->pos();
    for (monster *m : seeds)
        if (m)
            affected_positions.insert(m->pos());
}

// sigh, necessary to allow empty initializer lists with the above two
// constructors
targeter_multiposition::targeter_multiposition(const actor *a,
            initializer_list<coord_def> seeds, aff_type _positive)
    : targeter_multiposition(a, vector<coord_def>(seeds.begin(), seeds.end()),
         _positive)
{
}

aff_type targeter_multiposition::is_affected(coord_def loc)
{
    if ((cell_is_solid(loc) && !can_affect_walls())
        || !cell_see_cell(agent->pos(), loc, LOS_NO_TRANS))
    {
        return AFF_NO;
    }

    // is this better with maybe or yes?
    return affected_positions.count(loc) > 0 ? positive : AFF_NO;
}

targeter_scorch::targeter_scorch(const actor &a, int _range, bool affect_invis)
    : targeter_multiposition(&a,
                        find_near_hostiles(_range, affect_invis, a), AFF_MAYBE),
      range(_range)
{ }

bool targeter_scorch::valid_aim(coord_def a)
{
    if ((a - origin).rdist() > range)
        return notify_fail("Out of range.");
    return true;
}

targeter_chain_lightning::targeter_chain_lightning()
{
    vector<coord_def> target_set = chain_lightning_targets();
    int min_dist = 999;
    for (coord_def pos : target_set)
    {
        potential_victims.insert(pos);
        const int dist = grid_distance(pos, you.pos());
        if (dist && dist < min_dist)
            min_dist = dist;
    }

    for (coord_def pos : target_set)
    {
        const int dist = grid_distance(pos, you.pos());
        if (dist && dist <= min_dist)
            closest_victims.insert(pos);
    }
}

aff_type targeter_chain_lightning::is_affected(coord_def loc)
{
    if (closest_victims.find(loc) != closest_victims.end())
        return AFF_YES;
    if (potential_victims.find(loc) != potential_victims.end())
        return AFF_MAYBE;
    return AFF_NO;
}

targeter_maxwells_coupling::targeter_maxwells_coupling()
    : targeter_multiposition(&you, find_maxwells_possibles())
{
    if (affected_positions.size() == 1)
        positive = AFF_YES;
}

targeter_multifireball::targeter_multifireball(const actor *a, vector<coord_def> seeds)
    : targeter_multiposition(a, seeds)
{
    vector <coord_def> bursts;
    for (auto &c : seeds)
    {
        if (affected_positions.count(c)) // did the parent constructor like this pos?
            for (adjacent_iterator ai(c); ai; ++ai)
                bursts.push_back(*ai);
    }

    for (auto &c : bursts)
    {
        actor * act = actor_at(c);
        if (act && mons_aligned(agent, act))
            continue;
        affected_positions.insert(c);
    }
}

targeter_walls::targeter_walls(const actor *a, vector<coord_def> seeds)
    : targeter_multiposition(a, seeds)
{
    for (auto &c : seeds)
    {
        affected_positions.insert(c);
        for (adjacent_iterator ai(c); ai; ++ai)
            if (!cell_is_solid(*ai)) // don't add any walls not in `seeds`
                affected_positions.insert(*ai);
    }
}

aff_type targeter_walls::is_affected(coord_def loc)
{
    if (!affected_positions.count(loc)
        || !cell_see_cell(agent->pos(), loc, LOS_NO_TRANS))
    {
        return AFF_NO;
    }

    return cell_is_solid(loc) ? AFF_YES : AFF_MAYBE;
}

// note: starburst is not in spell_to_zap
targeter_starburst_beam::targeter_starburst_beam(const actor *a, int _range, int pow, const coord_def &offset)
    : targeter_beam(a, _range, ZAP_BOLT_OF_FIRE, pow, 0, 0)
{
    set_aim(a->pos() + offset);
}

targeter_starburst::targeter_starburst(const actor *a, int range, int pow)
    : targeter()
{
    agent = a ? a : &you;
    // XX code duplication with cast_starburst
    const vector<coord_def> offsets = { coord_def(range, 0),
                                        coord_def(range, range),
                                        coord_def(0, range),
                                        coord_def(-range, range),
                                        coord_def(-range, 0),
                                        coord_def(-range, -range),
                                        coord_def(0, -range),
                                        coord_def(range, -range) };

    // extremely brute force...
    for (auto &o : offsets)
        beams.push_back(targeter_starburst_beam(agent, range, pow, o));
}

aff_type targeter_starburst::is_affected(coord_def loc)
{
    for (auto &t : beams)
        if (auto r = t.is_affected(loc))
            return r;
    return AFF_NO;
}

targeter_bog::targeter_bog(const actor *a, int pow)
    : targeter_multiposition(a, { })
{
    auto seeds = find_bog_locations(a->pos(), pow);
    for (auto &c : seeds)
        affected_positions.insert(c);
}

targeter_ignite_poison::targeter_ignite_poison(actor *a)
    : targeter_multiposition(a, { })
{
    for (radius_iterator ri(a->pos(), LOS_SOLID_SEE); ri; ++ri)
        if (ignite_poison_affects_cell(*ri, a))
            affected_positions.insert(*ri);
}

targeter_multimonster::targeter_multimonster(const actor *a)
    : targeter()
{
    agent = a;
}

aff_type targeter_multimonster::is_affected(coord_def loc)
{
    if ((cell_is_solid(loc) && !can_affect_walls())
        || !cell_see_cell(agent->pos(), loc, LOS_NO_TRANS))
    {
        return AFF_NO;
    }

    //if (agent && act && !agent->can_see(*act))
    //    return AFF_NO;

    // Any special checks from our inheritors
    const monster_info *mon = env.map_knowledge(loc).monsterinfo();
    if (!mon || !affects_monster(*mon))
        return AFF_NO;

    return AFF_YES;
}

targeter_drain_life::targeter_drain_life()
    : targeter_multimonster(&you)
{
}

bool targeter_drain_life::affects_monster(const monster_info& mon)
{
    return get_resist(mon.resists(), MR_RES_NEG) < 3
           && !mons_atts_aligned(agent->temp_attitude(), mon.attitude);
}

targeter_discord::targeter_discord()
    : targeter_multimonster(&you)
{
}

bool targeter_discord::affects_monster(const monster_info& mon)
{
    return mon.willpower() != WILL_INVULN && mon.can_go_frenzy;
}

targeter_englaciate::targeter_englaciate()
    : targeter_multimonster(&you)
{
}

bool targeter_englaciate::affects_monster(const monster_info& mon)
{
    return get_resist(mon.resists(), MR_RES_COLD) <= 0
           && !mons_class_is_peripheral(mon.type);
}

targeter_fear::targeter_fear()
    : targeter_multimonster(&you)
{
}

bool targeter_fear::affects_monster(const monster_info& mon)
{
    return mon.willpower() != WILL_INVULN
           && mon.can_feel_fear
           && !mons_atts_aligned(agent->temp_attitude(), mon.attitude);
}

targeter_intoxicate::targeter_intoxicate()
    : targeter_multimonster(&you)
{
}

bool targeter_intoxicate::affects_monster(const monster_info& mon)
{
    return !(mon.mintel < I_HUMAN
             || get_resist(mon.resists(), MR_RES_POISON) >= 3);
}

targeter_anguish::targeter_anguish()
    : targeter_multimonster(&you)
{
}

bool targeter_anguish::affects_monster(const monster_info& mon)
{
    return mon.mintel > I_BRAINLESS
        && mon.willpower() != WILL_INVULN
        && !mons_atts_aligned(agent->temp_attitude(), mon.attitude)
        && !mon.is(MB_ANGUISH);
}

targeter_boulder::targeter_boulder(const actor* caster, int boulder_hp)
    : targeter_beam(caster, LOS_MAX_RANGE, ZAP_IOOD, 0, 0, 0), hp(boulder_hp)
{
}

bool targeter_boulder::set_aim(coord_def a)
{
    if (!targeter::set_aim(a))
        return false;

    bolt tempbeam = beam;

    tempbeam.target = a;
    tempbeam.aimed_at_spot = false;
    tempbeam.path_taken.clear();
    tempbeam.fire();
    path_taken = tempbeam.path_taken;

    boulder_sim.clear();

    // Now simulate rolling a boulder down the given path, and see how quickly
    // it is possible for it to get abraded by walls (to indicate what part of
    // the path it will always roll, and which is more questionable.)
    int _hp = hp;
    for (auto pos : path_taken)
    {
        if (cell_is_solid(pos))
            continue;

        // If we've already passed the point of possible destruction, no
        // need to calculate anything else.
        if (_hp <= 0)
        {
            boulder_sim[pos] = AFF_MAYBE;
            continue;
        }

        int solid_count = 0;
        for (adjacent_iterator ai(pos); ai; ++ai)
        {
            // Assume that unseen tiles are walls.
            if (env.map_knowledge(pos).feat() == DNGN_UNSEEN
                || cell_is_solid(*ai))
            {
                ++solid_count;
            }
        }

        // Assume maximum damage is taken
        _hp -= (solid_count * BOULDER_ABRASION_DAMAGE);

        if (_hp <= 0)
            boulder_sim[pos] = AFF_MAYBE;
        else
            boulder_sim[pos] = AFF_YES;
    }

    return true;
}

bool targeter_boulder::valid_aim(coord_def a)
{
    if (!in_bounds(a))
        return false;

    if (a == agent->pos())
        return notify_fail("There's a thick-headed creature in the way.");

    const coord_def delta = a - agent->pos();
    if (delta.x && delta.y && abs(delta.x) != abs(delta.y))
        return notify_fail("You can only roll a boulder in a compass direction.");

    ray_def ray;
    if (!find_ray(agent->pos(), a, ray, opc_solid))
        return notify_fail("There's something in the way.");
    if (!ray.advance())
        return notify_fail("You cannot conjure a boulder there.");

    const coord_def start = ray.pos();
    actor* act = actor_at(start);
    if (feat_is_solid(env.grid(start)) || (act && you.can_see(*act)))
        return notify_fail("You cannot conjure a boulder in an occupied space.");
    if (env.grid(start) == DNGN_LAVA)
        return notify_fail("You cannot conjure a boulder there.");

    return true;
}

aff_type targeter_boulder::is_affected(coord_def loc)
{
    if (boulder_sim.count(loc))
        return boulder_sim[loc];

    return AFF_NO;
}

targeter_chain::targeter_chain(const actor* caster, int r, zap_type ztype)
    : targeter_beam(caster, r, ztype, 0, 0, 0)
{
}

bool targeter_chain::set_aim(coord_def a)
{
    if (!targeter::set_aim(a))
        return false;

    bolt tempbeam = beam;

    tempbeam.target = a;
    tempbeam.aimed_at_spot = false;
    tempbeam.path_taken.clear();
    tempbeam.fire();
    path_taken = tempbeam.path_taken;

    chain_targ.clear();

    const coord_def pos = path_taken[path_taken.size() - 1];
    monster* targ = monster_at(pos);
    if (!targ || !agent->can_see(*targ))
        return true;

    vector<coord_def> chain_targs;
    fill_chain_targets(tempbeam, pos, chain_targs, false);
    chain_targ.insert(chain_targs.begin(), chain_targs.end());
    return true;
}

aff_type targeter_chain::is_affected(coord_def loc)
{
    for (auto pc : path_taken)
    {
        if (pc == loc)
        {
            if (cell_is_solid(pc))
                return AFF_NO;

            return AFF_YES;
        }
    }

    if (chain_targ.count(loc) > 0)
        return AFF_MAYBE;
    return AFF_NO;
}

targeter_bind_soul::targeter_bind_soul() :
    targeter_smite(&you, LOS_MAX_RANGE)
{
}

bool targeter_bind_soul::valid_aim(coord_def a)
{
    if (!targeter_smite::valid_aim(a))
        return false;

    monster* targ = monster_at(a);
    if (!targ || !agent->can_see(*targ))
        return notify_fail("You can't see anything there.");

    if (!yred_can_bind_soul(targ))
    {
        if (targ->friendly())
            return notify_fail("You cannot bind the soul of an ally.");
        else if (targ->type == MONS_PANDEMONIUM_LORD)
            return notify_fail("You are unable to grasp such an alien soul.");
        else if (targ->is_summoned())
            return notify_fail("You cannot bind the soul of a summoned being.");
        else
            return notify_fail("That does not possess a soul you can bind.");
    }

    if (mons_get_damage_level(*targ) > MDAM_LIGHTLY_DAMAGED)
        return notify_fail("Their soul is too badly injured.");

    return true;
}

targeter_explosive_beam::targeter_explosive_beam(const actor *act,
                                                 zap_type ztype,
                                                 int pow, int r,
                                                 bool _explode_on_monsters,
                                                 bool _always_explode) :
                          targeter_beam(act, r, ztype, pow, 0, 0),
                          explode_on_monsters(_explode_on_monsters),
                          always_explode(_always_explode)
{
}

bool targeter_explosive_beam::set_aim(coord_def a)
{
    if (!targeter_beam::set_aim(a))
        return false;

    bolt tempbeam = beam;
    tempbeam.target = origin;
    for (auto c : path_taken)
    {
        if (cell_is_solid(c))
            break;

        tempbeam.target = c;
        if (always_explode || (explode_on_monsters && anyone_there(c)))
        {
            tempbeam.use_target_as_pos = true;
            exp_map.init(INT_MAX);
            tempbeam.determine_affected_cells(exp_map, coord_def(), 0,
                                              1, true, true);
        }
    }

    return true;
}

aff_type targeter_explosive_beam::is_affected(coord_def loc)
{
    // First check the main beam path
    for (auto c : path_taken)
    {
        if (cell_is_solid(c))
            break;
        if (c == loc)
            return AFF_YES;
    }

    // Then check the explosion radius
    for (auto c : path_taken)
    {
        if ((always_explode
             || (explode_on_monsters && anyone_there(c)
                 && !beam.ignores_monster(monster_at(c))))
            && (loc - c).rdist() <= 9)
        {
            coord_def centre(9,9);
            if (exp_map(loc - c + centre) < INT_MAX
                && !cell_is_solid(loc))
            {
                return AFF_MAYBE;
            }
        }
    }

    return AFF_NO;
}

targeter_galvanic::targeter_galvanic(const actor *act, int pow, int r) :
                        targeter_beam(act, r, ZAP_GALVANIC_BREATH, pow, 0, 0)
{
}

bool targeter_galvanic::set_aim(coord_def a)
{
    jolt_targets.clear();

    if (!targeter_beam::set_aim(a))
        return false;

    bolt tempbeam = beam;
    tempbeam.target = origin;

    const coord_def pos = path_taken[path_taken.size() - 1];
    monster* targ = monster_at(pos);
    if (!targ || !agent->can_see(*targ))
        return true;

    jolt_targets = galvanic_targets(*agent, pos, false);

    return true;
}

aff_type targeter_galvanic::is_affected(coord_def loc)
{
    if (targeter_beam::is_affected(loc) == AFF_YES)
        return AFF_YES;

    for (coord_def pos : jolt_targets)
    {
        if (pos == loc)
            return AFF_YES;
    }

    return AFF_NO;
}

targeter_gavotte::targeter_gavotte(const actor* caster)
    : targeter_beam(caster, 1, ZAP_IOOD, 0, 0, 0)
{
}

bool targeter_gavotte::set_aim(coord_def a)
{
    if (!targeter::set_aim(a))
        return false;

    affected_monsters.clear();
    path_taken.clear();

    if (!valid_aim(a))
        return false;

    bolt tempbeam = beam;
    tempbeam.target = a;
    tempbeam.aimed_at_spot = false;
    tempbeam.range = you.is_stationary() || you.stasis() ? 0 : GAVOTTE_DISTANCE;
    tempbeam.path_taken.clear();
    tempbeam.fire();
    path_taken = tempbeam.path_taken;

    vector<monster*> affected = gavotte_affected_monsters(a - you.pos());
    for (monster* mon : affected)
        affected_monsters.push_back(mon->pos());

    return true;
}

bool targeter_gavotte::valid_aim(coord_def a)
{
    if (!targeter_beam::valid_aim(a))
        return false;

    if (grid_distance(a, you.pos()) > 1 || a == you.pos())
        return false;

    if (a == agent->pos())
        return notify_fail("Gravity is already pointing downward.");

    // make sure it's a true cardinal
    const coord_def delta = a - agent->pos();
    if (delta.x && delta.y && abs(delta.x) != abs(delta.y))
        return notify_fail("You can only reorient gravity in a cardinal direction.");

    return true;
}

aff_type targeter_gavotte::is_affected(coord_def loc)
{
    for (coord_def pos : affected_monsters)
        if (pos == loc)
            return AFF_YES;

    for (auto pc : path_taken)
        if (pc == loc)
            return cell_is_solid(pc) ? AFF_NO : AFF_MAYBE;

    return AFF_NO;
}

targeter_magnavolt::targeter_magnavolt(const actor* act, int _range) :
    targeter_smite(act, _range)
{
}

bool targeter_magnavolt::valid_aim(coord_def a)
{
    if (!targeter_smite::valid_aim(a))
        return false;

    if (!monster_at(a) || !you.can_see(*monster_at(a)))
        return notify_fail("You don't see a valid target there.");

    return true;
}

bool targeter_magnavolt::preferred_aim(coord_def a)
{
    return !monster_at(a)->has_ench(ENCH_MAGNETISED);
}

bool targeter_magnavolt::set_aim(coord_def a)
{
    beam_targets.clear();
    beam_paths.clear();

    if (!targeter_smite::set_aim(a))
        return false;

    beam_targets = get_magnavolt_targets();
    beam_targets.push_back(a);
    beam_paths = get_magnavolt_beam_paths(beam_targets);

    return true;
}

aff_type targeter_magnavolt::is_affected(coord_def loc)
{
    for (coord_def pos : beam_targets)
    {
        if (loc == pos)
            return AFF_YES;
    }

    for (coord_def pos : beam_paths)
    {
        if (loc == pos)
            return AFF_MAYBE;
    }

    return AFF_NO;
}

targeter_mortar::targeter_mortar(const actor* act, int max_range) :
    targeter_beam(act, max_range, ZAP_HELLFIRE_MORTAR_DIG, 0, 0, 0)
{
    beam.origin_spell = SPELL_HELLFIRE_MORTAR;
}

bool targeter_mortar::can_affect_unseen()
{
    return true;
}

bool targeter_mortar::affects_monster(const monster_info& /*mon*/)
{
    return false;
}

bool targeter_mortar::can_affect_walls()
{
    return true;
}

bool targeter_mortar::valid_aim(coord_def a)
{
    if (!in_bounds(a))
        return notify_fail("Out of range.");
    else
        return targeter_beam::valid_aim(a);
}

aff_type targeter_mortar::is_affected(coord_def loc)
{
    aff_type current = AFF_YES;
    bool hit_barrier = false;
    for (auto pc : path_taken)
    {
        if (hit_barrier)
            return AFF_NO; // some previous iteration hit a barrier
        current = AFF_YES;
        // uses comparison to DNGN_UNSEEN so that this works sensibly with magic
        // mapping etc. TODO: console tracers use the same symbol/color as
        // mmapped walls.
        if (in_bounds(pc) && env.map_knowledge(pc).feat() != DNGN_UNSEEN)
        {
            if (cell_is_solid(pc) && !beam.can_affect_wall(pc)
                || (monster_at(pc) && you.can_see(*monster_at(pc))
                    && !beam.ignores_monster(monster_at(pc))))
            {
                current = AFF_NO;
                hit_barrier = true;
            }
            else if (!cell_is_solid(pc))
                current = AFF_TRACER;

            // otherwise, default to AFF_YES
        }
        // unseen squares default to AFF_YES
        if (pc == loc)
            return current;
    }
    // path never intersected loc at all
    return AFF_NO;
}

targeter_slouch::targeter_slouch()
    : targeter_radius(&you, LOS_NO_TRANS, LOS_RADIUS)
{
}

aff_type targeter_slouch::is_affected(coord_def loc)
{
    if (targeter_radius::is_affected(loc) == AFF_NO)
        return AFF_NO;

    if (!monster_at(loc) || !you.can_see(*monster_at(loc))
        || !is_slouchable(loc))
    {
        return AFF_NO;
    }

    return AFF_YES;
}

targeter_marionette::targeter_marionette() :
    targeter_smite(&you, LOS_RADIUS)
{
}

bool targeter_marionette::valid_aim(coord_def a)
{
    if (!targeter_smite::valid_aim(a))
        return false;

    monster* mons = monster_at(a);
    if (!mons || !you.can_see(*mons) || mons->is_firewood() || mons->friendly())
        return notify_fail("");

    if (mons->has_ench(ENCH_SHADOWLESS))
        return notify_fail("Their shadow is too faded to take hold of.");

    if (mons->is_summoned())
        return notify_fail("A summoned shadow is too ephemeral to take hold of.");

    if (mons->props[DITHMENOS_MARIONETTE_SPELLS_KEY].get_int() <= 0)
        return notify_fail("They have no useful spells to cast right now.");

    return true;
}

targeter_putrefaction::targeter_putrefaction(int r) :
    targeter_smite(&you, r)
{
}

bool targeter_putrefaction::valid_aim(coord_def a)
{
    if (!targeter_smite::valid_aim(a))
        return false;

    const monster* mon = monster_at(a);
    if (!mon || !you.can_see(*mon))
        return false;

    if (mons_aligned(&you, mon))
        return notify_fail("You cannot putrefy a friendly monster.");

    if (!(mon->holiness() & MH_NATURAL))
        return notify_fail("You can only putrefy natural creatures.");

    if (mons_get_damage_level(*mon) < MDAM_HEAVILY_DAMAGED)
        return notify_fail("This isn't wounded badly enough.");

    return true;
}

targeter_soul_splinter::targeter_soul_splinter(const actor* caster, int r)
    : targeter_beam(caster, r, ZAP_SOUL_SPLINTER, 0, 0, 0)
{
}

bool targeter_soul_splinter::affects_monster(const monster_info& mon)
{
    if (!targeter_beam::affects_monster(mon))
        return false;

    if (mon.is(MB_SOUL_SPLINTERED))
        return false;

    if (!mons_can_be_spectralised(*monster_at(mon.pos), true, true))
        return false;

    if (agent->is_player())
    {
        for (adjacent_iterator ai(mon.pos); ai; ++ai)
        {
            if (!cell_is_solid(*ai) && !actor_at(*ai)
                && agent->see_cell_no_trans(*ai))
            {
                return true;
            }
        }
    }

    // No visible free spots found
    return false;
}

targeter_surprising_crocodile::targeter_surprising_crocodile(const actor* caster)
    : targeter_smite(caster, 1)
{
}

bool targeter_surprising_crocodile::valid_aim(coord_def a)
{
    if (!targeter_smite::valid_aim(a))
        return false;

    string msg = surprising_crocodile_unusable_reason(*agent, a, false);
    if (!msg.empty())
        return notify_fail(msg);

    return true;
}

bool targeter_surprising_crocodile::set_aim(coord_def a)
{
    landing_spots.clear();
    if (!targeter_smite::set_aim(a) || !valid_aim(a))
        return false;

    const coord_def drag_shift = -(a - agent->pos()).sgn();
    landing_spots.push_back(agent->pos() + drag_shift);

    if (surprising_crocodile_can_drag(*agent, a, false))
        landing_spots.push_back(agent->pos() + drag_shift + drag_shift);

    return true;
}

aff_type targeter_surprising_crocodile::is_affected(coord_def loc)
{
    for (coord_def spot : landing_spots)
        if (spot == loc)
            return AFF_YES;

    return AFF_NO;
}

targeter_wall_arc::targeter_wall_arc(const actor* caster, int size)
    : targeter_smite(caster, 1, 0, 0, true, true), wall_num(size)
{
}

bool targeter_wall_arc::set_aim(coord_def a)
{
    spots.clear();
    if (!targeter_smite::set_aim(a) || !valid_aim(a) || a == agent->pos())
        return false;

    spots = get_splinterfrost_block_spots(*agent, a, 4);

    return true;
}

aff_type targeter_wall_arc::is_affected(coord_def loc)
{
    for (coord_def spot : spots)
        if (spot == loc)
            return AFF_YES;

    return AFF_NO;
}

targeter_tempering::targeter_tempering() :
    targeter_smite(&you, LOS_RADIUS, 1, 1)
{
}

bool targeter_tempering::valid_aim(coord_def a)
{
    if (!targeter_smite::valid_aim(a))
        return false;

    monster* mons = monster_at(a);
    if (!mons || !you.can_see(*mons) || !mons->friendly())
        return false;

    if (mons->has_ench(ENCH_TEMPERED))
        return notify_fail("You cannot target a construct which is already augmented.");

    if (!is_valid_tempering_target(*mons, *agent))
        return notify_fail("You can only target your own Forgecraft constructs.");

    return true;
}

bool targeter_tempering::preferred_aim(coord_def a)
{
    // Look for at least one visible hostile monster next to this target.
    for (adjacent_iterator ai(a); ai; ++ai)
    {
        if (!you.see_cell_no_trans(*ai))
            continue;

        if (monster* mon = monster_at(*ai))
        {
            if (you.can_see(*mon) && !mon->wont_attack()
                && !mon->is_peripheral())
            {
                return true;
            }
        }
    }

    return false;
}

targeter_piledriver::targeter_piledriver()
    : targeter_smite(&you, 1)
{
    // Cache length of piledriver line in all directions
    for (int i = 0; i < 8; ++i)
        piledriver_lengths[i] = piledriver_path_distance(you.pos() + Compass[i], false);
}

bool targeter_piledriver::valid_aim(coord_def a)
{
    if (!targeter_smite::valid_aim(a))
        return false;

    coord_def delta = (a - you.pos());
    for (int i = 0; i < 8; ++i)
    {
        if (Compass[i] == delta)
        {
            if (piledriver_lengths[i] > 0)
                return true;
            else
                return notify_fail("You see nothing there you can launch.");
        }
    }

    return false;
}

bool targeter_piledriver::set_aim(coord_def a)
{
    spots.clear();

    coord_def delta = (a - you.pos());
    for (int i = 0; i < 8; ++i)
    {
        if (Compass[i] == delta)
        {
            for (int j = 1; j < piledriver_lengths[i] + 2; ++j)
                spots.push_back(agent->pos() + delta * j);
        }
    }

    return true;
}

aff_type targeter_piledriver::is_affected(coord_def loc)
{
    for (coord_def p : spots)
        if (loc == p)
            return AFF_YES;

    return AFF_NO;
}

targeter_teleport_other::targeter_teleport_other(const actor* act, int r) :
    targeter_smite(act, r)
{
}

bool targeter_teleport_other::valid_aim(coord_def a)
{
    if (!targeter_smite::valid_aim(a))
        return false;

    const monster_info* mi = env.map_knowledge(a).monsterinfo();

    if (!mi)
        return false;

    if (mi->is(MB_NO_TELE))
        return notify_fail("That cannot be teleported.");

    if (mi->willpower() == WILL_INVULN)
        return false;

    return true;
}

targeter_malign_gateway::targeter_malign_gateway(actor& caster)
{
    agent = &caster;
}

aff_type targeter_malign_gateway::is_affected(coord_def loc)
{
    if (is_gateway_target(*agent, loc))
        return AFF_MAYBE;

    return AFF_NO;
}
