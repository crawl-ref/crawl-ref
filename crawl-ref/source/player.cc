/**
 * @file
 * @brief Player related functions.
**/

#include "AppHdr.h"

#include "player.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "ability.h"
#include "abyss.h"
#include "acquire.h"
#include "act-iter.h"
#include "areas.h"
#include "art-enum.h"
#include "attack.h"
#include "bloodspatter.h"
#include "branch.h"
#include "chardump.h"
#include "cloud.h"
#include "coordit.h"
#include "delay.h"
#include "describe.h" // damage_rating
#include "dgn-overview.h"
#include "dgn-event.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "tile-env.h"
#include "errors.h"
#include "exercise.h"
#include "files.h"
#include "god-abil.h"
#include "god-conduct.h"
#include "god-passive.h"
#include "god-wrath.h"
#include "hints.h"
#include "hiscores.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "item-use.h"
#include "kills.h"
#include "level-state-type.h"
#include "libutil.h"
#include "macro.h"
#include "melee-attack.h"
#include "message.h"
#include "mon-place.h"
#include "movement.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "notes.h"
#include "output.h"
#include "player-equip.h"
#include "player-save-info.h"
#include "player-stats.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "skills.h"
#include "species.h" // random_starting_species
#include "spl-clouds.h" // explode_blastmotes_at
#include "spl-damage.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-other.h"
#include "spl-util.h"
#include "sprint.h"
#include "stairs.h"
#include "stash.h"
#include "state.h"
#include "status.h"
#include "stepdown.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#ifdef USE_TILE
 #include "tilepick.h"
 #include "tileview.h"
#endif
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "wizard-option-type.h"
#include "xom.h"
#include "zot.h" // bezotting_level

static void _pruneify()
{
    if (!you.may_pruneify())
        return;

    mpr("The curse of the Prune overcomes you.");
    did_god_conduct(DID_CHAOS, 10);
}

static void _moveto_maybe_repel_stairs()
{
    const dungeon_feature_type new_grid = env.grid(you.pos());
    const command_type stair_dir = feat_stair_direction(new_grid);

    if (stair_dir == CMD_NO_CMD
        || new_grid == DNGN_ENTER_SHOP
        ||  !you.duration[DUR_REPEL_STAIRS_MOVE])
    {
        return;
    }

    int pct = you.duration[DUR_REPEL_STAIRS_CLIMB] ? 29 : 50;

    // When the effect is still strong, the chance to actually catch
    // a stair is smaller. (Assuming the duration starts out at 1000.)
    const int dur = max(0, you.duration[DUR_REPEL_STAIRS_MOVE] - 700);
    pct += dur/10;

    if (x_chance_in_y(pct, 100))
    {
        const string stair_str = feature_description_at(you.pos(), false,
                                                        DESC_THE);
        const string prep = feat_preposition(new_grid, true, &you);

        if (slide_feature_over(you.pos()))
        {
            mprf("%s slides away as you move %s it!", stair_str.c_str(),
                 prep.c_str());

            if (player_in_a_dangerous_place() && one_chance_in(5))
                xom_is_stimulated(25);
        }
    }
}

bool check_moveto_cloud(const coord_def& p, const string &move_verb,
                        bool *prompted)
{
    if (you.confused())
        return true;

    const cloud_type ctype = env.map_knowledge(p).cloud();
    // Don't prompt if already in a cloud of the same type.
    if (is_damaging_cloud(ctype, true, cloud_is_yours_at(p))
        && ctype != cloud_type_at(you.pos())
        && !crawl_state.disables[DIS_CONFIRMATIONS])
    {
        // Don't prompt for steam unless we're at uncomfortably low hp.
        if (ctype == CLOUD_STEAM)
        {
            int threshold = 20;
            if (player_res_steam(false) < 0)
                threshold = threshold * 3 / 2;
            threshold = threshold * you.time_taken / BASELINE_DELAY;
            // Do prompt if we'd lose icemail, though.
            if (you.hp > threshold && !you.has_mutation(MUT_CONDENSATION_SHIELD))
                return true;
        }
        // Don't prompt for meph if we have clarity
        if (ctype == CLOUD_MEPHITIC && you.clarity(false))
            return true;

        if (prompted)
            *prompted = true;
        string prompt = make_stringf("Really %s into that cloud of %s?",
                                     move_verb.c_str(),
                                     cloud_type_name(ctype).c_str());
        learned_something_new(HINT_CLOUD_WARNING);

        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }
    return true;
}

bool check_moveto_trap(const coord_def& p, const string &move_verb,
                       bool *prompted)
{
    // Boldly go into the unknown (for ranged move prompts)
    if (env.map_knowledge(p).trap() == TRAP_UNASSIGNED)
        return true;

    // If there's no trap, let's go.
    trap_def* trap = trap_at(p);
    if (!trap)
        return true;

    if (trap->type == TRAP_ZOT && !trap->is_safe() && !crawl_state.disables[DIS_CONFIRMATIONS])
    {
        string msg = "Do you really want to %s into the Zot trap?";
        string prompt = make_stringf(msg.c_str(), move_verb.c_str());

        if (prompted)
            *prompted = true;
        if (!yes_or_no("%s", prompt.c_str()))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }
    else if (!trap->is_safe() && !crawl_state.disables[DIS_CONFIRMATIONS])
    {
        string prompt;

        if (prompted)
            *prompted = true;
        prompt = make_stringf("Really %s %s that %s?", move_verb.c_str(),
                              (trap->type == TRAP_ALARM
                               || trap->type == TRAP_PLATE) ? "onto"
                              : "into",
                              feature_description_at(p, false, DESC_BASENAME).c_str());
        if (!yesno(prompt.c_str(), true, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }
    return true;
}

static bool _check_moveto_dangerous(const coord_def& p, const string& msg)
{
    if (you.can_swim() && feat_is_water(env.grid(p))
        || you.airborne() || !is_feat_dangerous(env.grid(p)))
    {
        return true;
    }

    if (!msg.empty())
        mpr(msg);
    else if (species::likes_water(you.species) && feat_is_water(env.grid(p)))
    {
        // player normally likes water, but is in a form that doesn't
        mpr("You cannot enter water in your current form.");
    }
    else
        canned_msg(MSG_UNTHINKING_ACT);
    return false;
}

bool check_moveto_terrain(const coord_def& p, const string &move_verb,
                          const string &msg, bool *prompted)
{
    // Boldly go into the unknown (for ranged move prompts)
    if (!env.map_knowledge(p).known())
        return true;

    if (!_check_moveto_dangerous(p, msg))
        return false;
    if (env.grid(p) == DNGN_BINDING_SIGIL && !you.is_binding_sigil_immune())
    {
        string prompt;
        if (prompted)
            *prompted = true;

        if (!msg.empty())
            prompt = msg + " ";

        prompt += "Are you sure you want to " + move_verb
                + " onto a binding sigil?";

        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }
    if (!you.airborne() && !you.duration[DUR_NOXIOUS_BOG]
        && env.grid(you.pos()) != DNGN_TOXIC_BOG
        && env.grid(p) == DNGN_TOXIC_BOG)
    {
        string prompt;

        if (prompted)
            *prompted = true;

        if (!msg.empty())
            prompt = msg + " ";

        prompt += "Are you sure you want to " + move_verb
                + " into a toxic bog?";

        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }
    if (!need_expiration_warning() && need_expiration_warning(p)
        && !crawl_state.disables[DIS_CONFIRMATIONS])
    {
        string prompt;

        if (prompted)
            *prompted = true;

        if (!msg.empty())
            prompt = msg + " ";

        prompt += "Are you sure you want to " + move_verb;

        if (you.ground_level())
            prompt += " into ";
        else
            prompt += " over ";

        prompt += env.grid(p) == DNGN_DEEP_WATER ? "deep water" : "lava";

        prompt += need_expiration_warning(DUR_FLIGHT, p)
            ? " while you are losing your buoyancy?"
            : " while your transformation is expiring?";

        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }
    return true;
}

bool check_moveto_exclusions(const vector<coord_def> &areas,
                             const string &move_verb,
                             bool *prompted)
{
    const bool you_pos_excluded = is_excluded(you.pos())
            && !is_stair_exclusion(you.pos());
    if (you_pos_excluded || crawl_state.disables[DIS_CONFIRMATIONS])
        return true;

    int count = 0;
    for (auto p : areas)
    {
        if (is_excluded(p) && !is_stair_exclusion(p))
            count++;
    }
    if (count == 0)
        return true;
    const string prompt = make_stringf((count == (int) areas.size() ?
                    "Really %s into a travel-excluded area?" :
                    "You might %s into a travel-excluded area, are you sure?"),
                              move_verb.c_str());

    if (prompted)
        *prompted = true;
    if (!yesno(prompt.c_str(), false, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }
    return true;
}

bool check_moveto_exclusion(const coord_def& p, const string &move_verb,
                            bool *prompted)
{
    return check_moveto_exclusions({p}, move_verb, prompted);
}

/**
 * Confirm that the player really does want to go to the indicated place.
 * May give many prompts, or no prompts if the move is safe.
 *
 * @param p          The location the player wants to go to
 * @param move_verb  The method of locomotion the player is using
 * @param physically Whether the player is considered to be "walking" for the
 *                   purposes of barbs causing damage and ice spells expiring
 * @return If true, continue with the move, otherwise cancel it
 */
bool check_moveto(const coord_def& p, const string &move_verb, bool physically)
{
    return !(physically && cancel_harmful_move(physically, move_verb == "rampage"))
           && check_moveto_terrain(p, move_verb, "")
           && check_moveto_cloud(p, move_verb)
           && check_moveto_trap(p, move_verb)
           && check_moveto_exclusion(p, move_verb);
}

// Returns true if this is a valid swap for this monster. If true, then
// the valid location is set in loc. (Otherwise loc becomes garbage.)
bool swap_check(monster* mons, coord_def &loc, bool quiet)
{
    loc = you.pos();

    if (!you.is_motile())
        return false;

    // Don't move onto dangerous terrain.
    if (is_feat_dangerous(env.grid(mons->pos())))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return false;
    }

    if (mons_is_projectile(*mons)
        || mons->type == MONS_BOULDER
        || mons->type == MONS_BLAZEHEART_CORE)
    {
        if (!quiet)
            mpr("It's unwise to walk into this.");
        return false;
    }

    if (mons->caught())
    {
        if (!quiet)
        {
            simple_monster_message(*mons,
                make_stringf(" is %s!", held_status(mons)).c_str());
        }
        return false;
    }

    if (mons->is_constricted())
    {
        if (!quiet)
            simple_monster_message(*mons, " is being constricted!");
        return false;
    }

    // TODO: consider waking up sleeping monsters when you push em?
    // (That's what happens for monsters pushing monsters...)
    if (mons->unswappable() || mons->asleep())
    {
        if (!quiet)
            simple_monster_message(*mons, " cannot move out of your way!");
        return false;
    }

    // prompt when swapping into known zot traps
    if (!quiet && trap_at(loc) && trap_at(loc)->type == TRAP_ZOT
        && !yes_or_no("Do you really want to swap %s into the Zot trap?",
                      mons->name(DESC_YOUR).c_str()))
    {
        return false;
    }

    // First try: move monster onto your position.
    bool swap = !monster_at(loc) && monster_habitable_grid(mons, loc);

    // Choose an appropriate habitat square at random around the target.
    if (!swap)
    {
        int num_found = 0;

        for (adjacent_iterator ai(mons->pos()); ai; ++ai)
            if (!monster_at(*ai) && monster_habitable_grid(mons, *ai)
                && one_chance_in(++num_found))
            {
                loc = *ai;
            }

        if (num_found)
            swap = true;
    }

    if (!swap && !quiet)
    {
        // Might not be ideal, but it's better than insta-killing
        // the monster... maybe try for a short blink instead? - bwr
        simple_monster_message(*mons, " cannot make way for you.");
        // FIXME: activity_interrupt::hit_monster isn't ideal.
        interrupt_activity(activity_interrupt::hit_monster, mons);
    }

    return swap;
}

bool player::slow_in_water() const
{
    return !you.can_swim()
        && you.body_size(PSIZE_BODY) <= SIZE_MEDIUM;
}

static void _maybe_sink(dungeon_feature_type old_feat,
                        dungeon_feature_type new_grid)
{
    if (!you.can_swim()
        && new_grid == DNGN_DEEP_WATER
        && old_feat != DNGN_DEEP_WATER)
    {
        mpr("You sink to the bottom.");
    }
}

static void _enter_water(dungeon_feature_type old_feat,
                         dungeon_feature_type new_grid, bool stepped)
{
    if (you.can_water_walk())
        return;

    // This is ridiculous.
    if (!stepped)
    {
        if (you.can_swim())
            noisy(4, you.pos(), "Floosh!");
        else
            noisy(8, you.pos(), "Splash!");
    }

    // Merfolk special-case most relevant messages.
    if (you.fishtail)
        return;

    // Most of these messages are irrelevant when you're already in the water.
    if (feat_is_water(old_feat))
    {
        _maybe_sink(old_feat, new_grid);
        return;
    }

    if (new_grid == DNGN_TOXIC_BOG)
        mprf("You %s the toxic bog.", stepped ? "enter" : "fall into");
    else
    {
        mprf("You %s the %s water.",
             stepped ? "enter" : "fall into",
             new_grid == DNGN_SHALLOW_WATER ? "shallow" : "deep");
    }

    _maybe_sink(old_feat, new_grid);

    if (you.slow_in_water())
    {
        mpr("Moving in this stuff is going to be slow.");
        if (you.invisible())
            mpr("...and don't expect to remain undetected.");
    } else if (you.invisible())
        mpr("Don't expect to remain undetected while in the water.");
}

void moveto_location_effects(dungeon_feature_type old_feat,
                             bool stepped, const coord_def& old_pos)
{
    const dungeon_feature_type new_grid = env.grid(you.pos());

    // Terrain effects.
    if (is_feat_dangerous(new_grid))
        fall_into_a_pool(new_grid);

    // called after fall_into_a_pool, in case of emergency untransform
    if (you.has_innate_mutation(MUT_MERTAIL))
        merfolk_check_swimming(old_feat, stepped);

    if (you.ground_level())
    {
        if (feat_is_water(new_grid))
            _enter_water(old_feat, new_grid, stepped);
        else if (you.props.exists(TEMP_WATERWALK_KEY))
            you.props.erase(TEMP_WATERWALK_KEY);
    }

    id_floor_items();

    // Falling into a toxic bog, take the damage
    if (old_pos == you.pos() && stepped)
        actor_apply_toxic_bog(&you);

    if (!actor_slime_wall_immune(&you))
    {
        const bool was_slimy = slime_wall_neighbour(old_pos);
        const bool is_slimy = slime_wall_neighbour(you.pos());
        if (was_slimy || is_slimy)
       {
           you.redraw_armour_class = true;
           you.wield_change = true;
           if (!was_slimy)
               mpr("Acid dripping from the walls corrodes you.");
       }
    }

    if (old_pos != you.pos())
    {
        cloud_struct* cloud = cloud_at(you.pos());
        if (cloud && cloud->type == CLOUD_BLASTMOTES)
            explode_blastmotes_at(you.pos()); // schedules a fineff

        // Traps go off.
        // (But not when losing flight - i.e., moving into the same tile)
        trap_def* ptrap = trap_at(you.pos());
        if (ptrap)
            ptrap->trigger(you);

        if (env.grid(you.pos()) == DNGN_BINDING_SIGIL)
            trigger_binding_sigil(you);
    }

    if (stepped)
        _moveto_maybe_repel_stairs();

    // Reveal adjacent mimics.
    for (adjacent_iterator ai(you.pos(), false); ai; ++ai)
        discover_mimic(*ai);

    bool was_running = you.running;

    update_monsters_in_view();
    maybe_update_stashes();
    if (check_for_interesting_features() && you.running.is_explore())
        stop_running();

    // If travel was interrupted, we need to add the last step
    // to the travel trail.
    if (was_running && !you.running)
        env.travel_trail.push_back(you.pos());
}

// Use this function whenever the player enters (or lands and thus re-enters)
// a grid.
//
// stepped     - normal walking moves
void move_player_to_grid(const coord_def& p, bool stepped)
{
    ASSERT(!crawl_state.game_is_arena());
    ASSERT_IN_BOUNDS(p);

    // assuming that entering the same square means coming from above (flight)
    const coord_def old_pos = you.pos();
    const bool from_above = (old_pos == p);
    const dungeon_feature_type old_grid =
        (from_above) ? DNGN_FLOOR : env.grid(old_pos);

    // Really must be clear.
    ASSERT(you.can_pass_through_feat(env.grid(p)));

    ASSERT(!monster_at(p)
           || fedhas_passthrough(monster_at(p))
           || mons_is_wrath_avatar(*monster_at(p)));

    // Move the player to new location.
    you.moveto(p, true);
    viewwindow();
    update_screen();

    moveto_location_effects(old_grid, stepped, old_pos);
}


/**
 * Check if the given terrain feature is safe for the player to move into.
 * (Or, at least, not instantly lethal.)
 *
 * @param grid          The type of terrain feature under consideration.
 * @param permanently   Whether to disregard temporary effects (non-permanent
 *                      flight, forms, etc)
 * @param ignore_flight Whether to ignore all forms of flight (including
 *                      permanent flight)
 * @return              Whether the terrain is safe.
 */
bool is_feat_dangerous(dungeon_feature_type grid, bool permanently,
                       bool ignore_flight)
{
    if (!ignore_flight
        && (you.permanent_flight() || you.airborne() && !permanently))
    {
        return false;
    }
    else if (grid == DNGN_DEEP_WATER && !player_likes_water(permanently)
             || grid == DNGN_LAVA)
    {
        return true;
    }
    else
        return false;
}

bool is_map_persistent()
{
    return !testbits(your_branch().branch_flags, brflag::no_map)
           || env.properties.exists(FORCE_MAPPABLE_KEY);
}

bool player_in_hell(bool vestibule)
{
    return vestibule ? is_hell_branch(you.where_are_you) :
                       is_hell_subbranch(you.where_are_you);
}

bool player_in_connected_branch()
{
    return is_connected_branch(you.where_are_you);
}

bool player_likes_water(bool permanently)
{
    return cur_form(!permanently)->player_can_swim()
           || !permanently && you.can_water_walk();
}

/**
 * Is the player considered to be closely related, if not the same species, to
 * the given monster? (See mon-data.h for species/genus info.)
 *
 * @param mon   The type of monster to be compared.
 * @return      Whether the player's species is related to the one given.
 */
bool is_player_same_genus(const monster_type mon)
{
    return mons_genus(mon) == mons_genus(player_mons(false));
}

void update_player_symbol()
{
    you.symbol = Options.show_player_species ? player_mons() : transform_mons();
}

monster_type player_mons(bool transform)
{
    monster_type mons;

    if (transform)
    {
        mons = transform_mons();
        if (mons != MONS_PLAYER)
            return mons;
    }

    mons = you.mons_species();

    if (mons == MONS_ORC)
    {
        // Orc implies Beogh worship nowadays, but someone might still be
        // playing a Hill Orc from an old save...
        if (you_worship(GOD_BEOGH))
        {
            mons = (you.piety >= piety_breakpoint(4)) ? MONS_ORC_HIGH_PRIEST
                                                      : MONS_ORC_PRIEST;
        }
    }

    return mons;
}

void update_vision_range()
{
    you.normal_vision = LOS_DEFAULT_RANGE;

    // Daystalker gives +1 base LOS. (currently capped to one level for
    // console reasons, a modular hud might someday permit more levels)
    if (you.get_mutation_level(MUT_DAYSTALKER))
        you.normal_vision += you.get_mutation_level(MUT_DAYSTALKER);

    // Nightstalker gives -1/-2/-3 to base LOS.
    if (you.get_mutation_level(MUT_NIGHTSTALKER))
        you.normal_vision -= you.get_mutation_level(MUT_NIGHTSTALKER);

    // Halo and umbra radius scale with you.normal_vision, so to avoid
    // penalizing players with low LOS from items, don't shrink normal_vision.
    you.current_vision = you.normal_vision;

#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_METEORAN)
        you.current_vision -= max(0, (bezotting_level() - 1) * 2); // spooky fx
#endif

    // scarf of shadows gives -1.
    if (you.wearing_ego(OBJ_ARMOUR, SPARM_SHADOWS))
        you.current_vision -= 1;

    // robe of Night.
    if (you.unrand_equipped(UNRAND_NIGHT))
        you.current_vision = you.current_vision * 3 / 4;

    if (you.duration[DUR_PRIMORDIAL_NIGHTFALL])
    {
        // Determine the percentage of Nightfall's max duration that has passed,
        // then use a hermite curve to map this to the actual sight radius value.
        const int max_dur = you.props[NIGHTFALL_INITIAL_DUR_KEY].get_int();
        const int dur = (max_dur - you.duration[DUR_PRIMORDIAL_NIGHTFALL]) * 100 / max_dur;
        const int intensity = (3 * dur * dur / 100) - (2 * dur * dur * dur / 10000);
        int vision = intensity * you.normal_vision / 100;
        // Cap radius 0 sight at no more than 5 turns (otherwise kobolds and/or
        // high invo characters can hold all monsters out of sight for too long)
        if (max_dur - you.duration[DUR_PRIMORDIAL_NIGHTFALL] > 50)
            vision = max(1, vision);

        // Immediately end the effect when it reaches our normal vision level
        if (vision >= you.current_vision)
            you.duration[DUR_PRIMORDIAL_NIGHTFALL] = 1;
        else
            you.current_vision = vision;
    }

    ASSERT(you.current_vision >= 0);
    set_los_radius(you.current_vision);
}

// For slots where characters may be able to equip some, but not all, possible
// items, actually test different items.
static maybe_bool _test_wear_armour_in_slot(equipment_slot slot, bool include_form)
{
    item_def dummy, alternate;
    dummy.base_type = alternate.base_type = OBJ_ARMOUR;
    dummy.sub_type = alternate.sub_type = NUM_ARMOURS;
    dummy.unrand_idx = alternate.unrand_idx = 0;

    switch (slot)
    {
    case SLOT_BODY_ARMOUR:
        // Assume that anything that can wear any armour at all can wear a robe
        // and that anything that can wear CPA can wear all armour.
        dummy.sub_type = ARM_CRYSTAL_PLATE_ARMOUR;
        alternate.sub_type = ARM_ROBE;
        break;

    case SLOT_OFFHAND:
        dummy.sub_type = ARM_TOWER_SHIELD;
        alternate.sub_type = ARM_ORB;
        break;

    case SLOT_HELMET:
        dummy.sub_type = ARM_HELMET;
        alternate.sub_type = ARM_HAT;
        break;

    // All other slots have no restrictions *within* that slot (ie: if you have
    // a glove slot at all, you can equip anything that fits in that slot.)
    default:
        die("unhandled equipment type %d", slot);
        break;
    }

    ASSERT(dummy.sub_type != NUM_ARMOURS);

    if (can_equip_item(dummy, include_form))
        return true;
    else if (alternate.sub_type != NUM_ARMOURS
             && can_equip_item(alternate, include_form))
    {
        return maybe_bool::maybe;
    }
    else
        return false;
}

/**
 * Checks if the player can generally equip any item that requires a given slot,
 * or whether they have restricted access (or none at all).
 *
 * The player is not required to have the *specific* slot in question; any
 * compatible one will do. (eg: Coglins lack a SLOT_OFFHAND but can still put
 * any offhand item into their SLOT_WEAPON_OR_OFFHAND, so SLOT_OFFHAND will
 * still return true.)
 *
 *
 * @param slot            The slot in question.
 * @param include_form    Whether to veto items unwearable in our current form.
 * @return   false             if the player can never use the slot;
 *           maybe_bool::maybe if the player can only use some items for the slot;
 *           true              if the player can use any (fsvo any) item for the slot.
 */
maybe_bool you_can_wear(equipment_slot slot, bool include_form)
{
    ASSERT_RANGE(slot, SLOT_FIRST_STANDARD, SLOT_LAST_STANDARD + 1);

    // The player has no compatible slot, there's no point in further checks.
    if (!you.equipment.has_compatible_slot(slot, include_form))
        return false;

    switch (slot)
    {
        case SLOT_WEAPON:
            if (you.body_size(PSIZE_TORSO, !include_form) < SIZE_MEDIUM)
                return maybe_bool::maybe;
            else
                return true;

        case SLOT_BODY_ARMOUR:
        case SLOT_OFFHAND:
        case SLOT_HELMET:
            return _test_wear_armour_in_slot(slot, include_form);

        // All other slots have no restrictions *within* that slot (ie: if you
        // have a glove slot at all, you can equip anything that fits in that
        // slot.), so there's no need for more thorough testing.
        default:
            return true;
    }
}

// Returns true if the player can wear at least some kind of armour.
// False for species that can't ever armour, and in extreme Octopode cases.
bool player_can_use_armour()
{
    if (you.has_mutation(MUT_NO_ARMOUR))
        return false;

    for (int i = SLOT_MIN_ARMOUR; i <= SLOT_MAX_ARMOUR; i++)
        if (you_can_wear(static_cast<equipment_slot>(i)) != false)
            return true;

    return false;
}

bool player_has_hair(bool temp, bool include_mutations)
{
    if (include_mutations &&
        you.get_mutation_level(MUT_SHAGGY_FUR, temp))
    {
        return true;
    }

    if (temp)
        return form_has_hair(you.form);

    return species::has_hair(you.species);
}

bool player_has_feet(bool temp, bool include_mutations)
{
    if (temp && you.fishtail)
        return false;

    if (include_mutations &&
        (you.get_mutation_level(MUT_HOOVES, temp) == 3
         || you.get_mutation_level(MUT_TALONS, temp) == 3))
    {
        return false;
    }

    if (temp)
        return form_has_feet(you.form);

    return species::has_feet(you.species);
}

bool player_has_ears(bool temp)
{
    if (temp)
        return form_has_ears(you.form);

    return species::has_ears(you.species);
}

// Returns false if the player is wielding a weapon inappropriate for Berserk.
bool berserk_check_wielded_weapon()
{
    const item_def * const wpn = you.weapon();
    bool penance = false;
    if (wpn && wpn->defined()
        && (!is_melee_weapon(*wpn)
            || needs_handle_warning(*wpn, OPER_ATTACK, penance)))
    {
        string prompt = "Do you really want to go berserk while wielding "
                        + wpn->name(DESC_YOUR) + "?";
        if (penance)
            prompt += " This could place you under penance!";

        if (!yesno(prompt.c_str(), true, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }

    return true;
}

int player::wearing(object_class_type obj_type, int sub_type, bool count_plus,
                    bool check_attuned) const
{
    return equipment.wearing(obj_type, sub_type, count_plus, check_attuned);
}

int player::wearing_ego(object_class_type obj_type, int ego) const
{
    return equipment.wearing_ego(obj_type, ego);
}

// Returns true if the indicated unrandart is equipped
bool player::unrand_equipped(int unrand_index, bool include_melded) const
{
    if (include_melded)
        return you.equipment.unrand_equipped.get(unrand_index - UNRAND_START);
    else
        return you.equipment.unrand_active.get(unrand_index - UNRAND_START);
}

bool player_can_hear(const coord_def& p, int hear_distance)
{
    return !silenced(p)
           && !silenced(you.pos())
           && you.pos().distance_from(p) <= hear_distance;
}

int get_teleportitis_level()
{
    ASSERT(!crawl_state.game_is_arena());

    // Teleportitis doesn't trigger in Sprint, Abyss, or Gauntlets.
    if (crawl_state.game_is_sprint() || player_in_branch(BRANCH_GAUNTLET)
        || player_in_branch(BRANCH_ABYSS))
    {
        return 0;
    }

    if (you.stasis())
        return 0;

    return you.get_mutation_level(MUT_TELEPORT) * 6;
}

// Computes bonuses to regeneration from most sources. Does not handle
// slow regeneration, vampireness, or Trog's Hand.
static int _player_bonus_regen()
{
    int rr = 0;

    // Amulets, troll leather armour, and artefacts.
    vector<item_def*> eq = you.equipment.get_slot_items(SLOT_ALL_EQUIPMENT, false, true);
    for (item_def* item : eq)
    {
        if (item->base_type == OBJ_ARMOUR
            && armour_type_prop(item->sub_type, ARMF_REGENERATION))
        {
            rr += REGEN_PIP;
        }
        if (item->is_type(OBJ_JEWELLERY, AMU_REGENERATION))
            rr += REGEN_PIP;
        if (is_artefact(*item))
            rr += REGEN_PIP * artefact_property(*item, ARTP_REGENERATION);
    }

    // Fast heal mutation.
    rr += you.get_mutation_level(MUT_REGENERATION) * REGEN_PIP;

    // Powered By Death mutation, boosts regen by variable strength
    // if the duration of the effect is still active.
    if (you.duration[DUR_POWERED_BY_DEATH])
        rr += you.props[POWERED_BY_DEATH_KEY].get_int() * 100;

    // Rampage healing grants a variable regen boost while active.
    if (you.get_mutation_level(MUT_ROLLPAGE) > 1
        && you.duration[DUR_RAMPAGE_HEAL])
    {
        rr += you.props[RAMPAGE_HEAL_KEY].get_int() * 65;
    }

    return rr;
}

static bool _mons_inhibits_regen(const monster &m)
{
    return mons_is_threatening(m)
                && !m.wont_attack()
                && !m.neutral()
                && you.can_see(m);
}

/// Is the player's hp regeneration inhibited by nearby monsters?
/// If the optional monster argument is provided, instead check whether that
/// specific monster inhibits regeneration.
bool regeneration_is_inhibited(const monster *m)
{
    // used mainly for resting: don't add anything here that can be waited off
    if (you.get_mutation_level(MUT_INHIBITED_REGENERATION) == 1
        || (you.has_mutation(MUT_VAMPIRISM) && !you.vampire_alive))
    {
        if (m)
            return _mons_inhibits_regen(*m);
        else
            for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
                if (_mons_inhibits_regen(**mi))
                    return true;
    }

    return false;
}

int player_regen()
{
    // Note: if some condition can set rr = 0, can't be rested off, and
    // would allow travel, please update is_sufficiently_rested.

    int rr = 20 + you.hp_max / 6;

    // Add in miscellaneous bonuses
    rr += _player_bonus_regen();

    // Before applying other effects, make sure that there's something
    // to heal.
    rr = max(1, rr);

    // Bonus regeneration for alive vampires.
    if (you.has_mutation(MUT_VAMPIRISM) && you.vampire_alive)
        rr += 20;

    if (you.duration[DUR_SICKNESS]
        || !player_regenerates_hp())
    {
        rr = 0;
    }

    // Trog's Hand. This circumvents sickness or inhibited regeneration.
    if (you.duration[DUR_TROGS_HAND])
        rr += 100;

    // Jiyva's passive healing also bypasses sickness, as befits a god.
    if (have_passive(passive_t::jelly_regen))
    {
        // One regen pip at 1* piety, scaling to two pips at 6*.
        // We use piety rank to avoid leaking piety info to the player.
        rr += REGEN_PIP + (REGEN_PIP * (piety_rank(you.piety) - 1)) / 5;
    }

    return rr;
}

int player_mp_regen()
{
    if (you.has_mutation(MUT_HP_CASTING))
        return 0;

    int regen_amount = 7 + you.max_magic_points / 2;

    if (you.get_mutation_level(MUT_MANA_REGENERATION))
        regen_amount *= 2;

    // Amulets and artefacts.
    vector<item_def*> eq = you.equipment.get_slot_items(SLOT_ALL_EQUIPMENT, false, true);
    for (item_def* item : eq)
    {
        if (item->is_type(OBJ_JEWELLERY, AMU_MANA_REGENERATION))
            regen_amount += 40;
        if (is_artefact(*item))
            regen_amount += 40 * artefact_property(*item, ARTP_MANA_REGENERATION);
    }

    // Rampage healing grants a variable regen boost while active.
    if (you.duration[DUR_RAMPAGE_HEAL])
        regen_amount += you.props[RAMPAGE_HEAL_KEY].get_int() * 33;

    if (have_passive(passive_t::jelly_regen))
    {
        // We use piety rank to avoid leaking piety info to the player.
        regen_amount += 40 + (40 * (piety_rank(you.piety) - 1)) / 5;
    }

    return regen_amount;
}

/**
 * How many spell levels does the player have total, including those used up
 * by memorised spells?
 */
int player_total_spell_levels()
{
    return you.experience_level - 1 + you.skill(SK_SPELLCASTING, 2, false, false);
}

/**
 * How many spell levels does the player currently have available for
 * memorising new spells?
 */
int player_spell_levels(bool floored)
{
    int sl = min(player_total_spell_levels(), 99);

    for (const spell_type spell : you.spells)
    {
        if (spell != SPELL_NO_SPELL)
            sl -= spell_difficulty(spell);
    }

    if (sl < 0 && floored)
        sl = 0;

    return sl;
}

// If temp is set to false, temporary sources or resistance won't be counted.
int player_res_fire(bool allow_random, bool temp, bool items)
{
    int rf = 0;

    if (items)
    {
        // rings of fire resistance/fire
        rf += you.wearing_jewellery(RING_PROTECTION_FROM_FIRE);
        rf += you.wearing_jewellery(RING_FIRE);

        // rings of ice
        rf -= you.wearing_jewellery(RING_ICE);

        // Staves
        rf += you.wearing(OBJ_STAVES, STAFF_FIRE);

        // body armour:
        const item_def *body_armour = you.body_armour();
        if (body_armour)
            rf += armour_type_prop(body_armour->sub_type, ARMF_RES_FIRE);

        // ego armours
        rf += you.wearing_ego(OBJ_ARMOUR, SPARM_FIRE_RESISTANCE);
        rf += you.wearing_ego(OBJ_ARMOUR, SPARM_RESISTANCE);

        // randart weapons:
        rf += you.scan_artefacts(ARTP_FIRE);

        // dragonskin cloak: 0.5 to draconic resistances
        if (allow_random && you.unrand_equipped(UNRAND_DRAGONSKIN)
            && coinflip())
        {
            rf++;
        }
    }

    // mutations:
    rf += you.get_mutation_level(MUT_HEAT_RESISTANCE, temp);
    rf -= you.get_mutation_level(MUT_HEAT_VULNERABILITY, temp);
    rf -= you.get_mutation_level(MUT_TEMPERATURE_SENSITIVITY, temp);
    rf += you.get_mutation_level(MUT_MOLTEN_SCALES, temp) == 3 ? 1 : 0;

    // spells:
    if (temp)
    {
        if (you.duration[DUR_RESISTANCE])
            rf++;

        if (you.duration[DUR_QAZLAL_FIRE_RES])
            rf++;
    }

    rf += cur_form(temp)->res_fire();

    if (have_passive(passive_t::resist_fire))
        ++rf;

    if (rf > 3)
        rf = 3;
    if (rf > 0 && you.penance[GOD_IGNIS])
        rf = 0;
    if (temp && you.duration[DUR_FIRE_VULN])
        rf--;
    if (rf < -3)
        rf = -3;

    return rf;
}

int player_res_steam(bool allow_random, bool temp, bool items)
{
    int res = 0;
    const int rf = player_res_fire(allow_random, temp, items);

    res += you.get_mutation_level(MUT_STEAM_RESISTANCE) * 2;

    if (items)
    {
        const item_def *body_armour = you.body_armour();
        if (body_armour)
            res += armour_type_prop(body_armour->sub_type, ARMF_RES_STEAM) * 2;
    }

    res += rf * 2;

    if (res > 2)
        res = 2;

    return res;
}

int player_res_cold(bool allow_random, bool temp, bool items)
{
    int rc = 0;

    if (temp)
    {
        if (you.duration[DUR_RESISTANCE])
            rc++;

        if (you.duration[DUR_QAZLAL_COLD_RES])
            rc++;

        // XX temp?
        if (you.has_mutation(MUT_VAMPIRISM) && !you.vampire_alive)
            rc += 2;
    }

    rc += cur_form(temp)->res_cold();

    if (items)
    {
        // rings of cold resistance/ice
        rc += you.wearing_jewellery(RING_PROTECTION_FROM_COLD);
        rc += you.wearing_jewellery(RING_ICE);

        // rings of fire
        rc -= you.wearing_jewellery(RING_FIRE);

        // Staves
        rc += you.wearing(OBJ_STAVES, STAFF_COLD);

        // body armour:
        const item_def *body_armour = you.body_armour();
        if (body_armour)
            rc += armour_type_prop(body_armour->sub_type, ARMF_RES_COLD);

        // ego armours
        rc += you.wearing_ego(OBJ_ARMOUR, SPARM_COLD_RESISTANCE);
        rc += you.wearing_ego(OBJ_ARMOUR, SPARM_RESISTANCE);

        // randart weapons:
        rc += you.scan_artefacts(ARTP_COLD);

        // dragonskin cloak: 0.5 to draconic resistances
        if (allow_random && you.unrand_equipped(UNRAND_DRAGONSKIN)
            && coinflip())
        {
            rc++;
        }
    }

    // mutations:
    rc += you.get_mutation_level(MUT_COLD_RESISTANCE, temp);
    rc -= you.get_mutation_level(MUT_COLD_VULNERABILITY, temp);
    rc -= you.get_mutation_level(MUT_TEMPERATURE_SENSITIVITY, temp);
    rc += you.get_mutation_level(MUT_ICY_BLUE_SCALES, temp) == 3 ? 1 : 0;
    rc += you.get_mutation_level(MUT_SHAGGY_FUR, temp) == 3 ? 1 : 0;

    if (rc < -3)
        rc = -3;
    else if (rc > 3)
        rc = 3;

    return rc;
}

bool player::res_corr(bool allow_random, bool temp) const
{
    if (temp)
    {
        // dragonskin cloak: 0.5 to draconic resistances
        if (allow_random && you.unrand_equipped(UNRAND_DRAGONSKIN)
            && coinflip())
        {
            return true;
        }

        if (you.duration[DUR_RESISTANCE])
            return true;
    }

    if (cur_form(temp)->res_acid())
        return true;

    if (have_passive(passive_t::resist_corrosion))
        return true;

    if (get_mutation_level(MUT_ACID_RESISTANCE))
        return true;

    // TODO: why doesn't this use the usual form suppression mechanism?
    if (form_keeps_mutations()
        && get_mutation_level(MUT_YELLOW_SCALES) >= 3)
    {
        return true;
    }

    return actor::res_corr(allow_random, temp);
}

int player_res_acid(bool items)
{
    return you.res_corr(items) ? 1 : 0;
}

int player_res_electricity(bool allow_random, bool temp, bool items)
{
    int re = 0;

    if (items)
    {
        // staff
        re += you.wearing(OBJ_STAVES, STAFF_AIR);

        // body armour:
        const item_def *body_armour = you.body_armour();
        if (body_armour)
            re += armour_type_prop(body_armour->sub_type, ARMF_RES_ELEC);

        // randart weapons:
        re += you.scan_artefacts(ARTP_ELECTRICITY);

        // dragonskin cloak: 0.5 to draconic resistances
        if (allow_random && you.unrand_equipped(UNRAND_DRAGONSKIN)
            && coinflip())
        {
            re++;
        }
    }

    // mutations:
    re += you.get_mutation_level(MUT_THIN_METALLIC_SCALES, temp) == 3 ? 1 : 0;
    re += you.get_mutation_level(MUT_SHOCK_RESISTANCE, temp);
    re -= you.get_mutation_level(MUT_SHOCK_VULNERABILITY, temp);

    if (cur_form(temp)->res_elec())
        re++;

    if (temp)
    {
        if (you.duration[DUR_RESISTANCE])
            re++;

        if (you.duration[DUR_QAZLAL_ELEC_RES])
            re++;
    }

    if (re > 1)
        re = 1;

    return re;
}

// Kiku protects you from torment to a degree.
bool player_kiku_res_torment()
{
    // no protection during pain branding weapon
    return have_passive(passive_t::resist_torment)
           && !(you_worship(GOD_KIKUBAAQUDGHA) && you.gift_timeout);
}

// If temp is set to false, temporary sources or resistance won't be counted.
int player_res_poison(bool allow_random, bool temp, bool items, bool forms)
{
    const int form_rp = forms ? cur_form(temp)->res_pois() : 0;
    if (you.is_nonliving(temp, forms)
        || you.is_lifeless_undead(temp)
        || form_rp == 3
        || items && you.unrand_equipped(UNRAND_OLGREB)
        || temp && you.duration[DUR_DIVINE_STAMINA])
    {
        return 3;
    }

    int rp = 0;

    if (items)
    {
        // rings of poison resistance
        rp += you.wearing_jewellery(RING_POISON_RESISTANCE);

        // Staves
        rp += you.wearing(OBJ_STAVES, STAFF_ALCHEMY);

        // ego armour:
        rp += you.wearing_ego(OBJ_ARMOUR, SPARM_POISON_RESISTANCE);

        // body armour:
        const item_def *body_armour = you.body_armour();
        if (body_armour)
            rp += armour_type_prop(body_armour->sub_type, ARMF_RES_POISON);

        // rPois+ artefacts
        rp += you.scan_artefacts(ARTP_POISON);

        // dragonskin cloak: 0.5 to draconic resistances
        if (allow_random && you.unrand_equipped(UNRAND_DRAGONSKIN)
            && coinflip())
        {
            rp++;
        }
    }

    // mutations:
    rp += you.get_mutation_level(MUT_POISON_RESISTANCE, temp);
    rp += you.get_mutation_level(MUT_SLIMY_GREEN_SCALES, temp) == 3 ? 1 : 0;

    if (temp && you.duration[DUR_RESISTANCE])
        rp++;

    if (form_rp > 0)
        rp += form_rp;

    // Cap rPois at + before vulnerability effects are applied
    // (so carrying multiple rPois effects is never useful)
    rp = min(1, rp);

    if (form_rp < 0)
        rp += form_rp; // actually a subtraction

    if (temp && you.duration[DUR_POISON_VULN])
        rp--;

    // don't allow rPois--, etc.
    rp = max(-1, rp);

    return rp;
}

int player_res_sticky_flame()
{
    return you.is_insubstantial();
}

int player_spec_death()
{
    int sd = 0;

    sd += you.wearing(OBJ_STAVES, STAFF_DEATH);

    sd += you.get_mutation_level(MUT_NECRO_ENHANCER);

    sd += you.scan_artefacts(ARTP_ENHANCE_NECRO);

    return sd;
}

int player_spec_fire()
{
    int sf = 0;

    sf += you.wearing(OBJ_STAVES, STAFF_FIRE);

    sf += you.wearing_jewellery(RING_FIRE);

    sf += you.scan_artefacts(ARTP_ENHANCE_FIRE);

    if (you.unrand_equipped(UNRAND_SALAMANDER))
        sf++;

    if (you.unrand_equipped(UNRAND_ELEMENTAL_STAFF))
        sf++;

    return sf;
}

int player_spec_cold()
{
    int sc = 0;

    sc += you.wearing(OBJ_STAVES, STAFF_COLD);

    sc += you.wearing_jewellery(RING_ICE);

    sc += you.scan_artefacts(ARTP_ENHANCE_ICE);

    if (you.unrand_equipped(UNRAND_ELEMENTAL_STAFF))
        sc++;

    return sc;
}

int player_spec_earth()
{
    int se = 0;

    // Staves
    se += you.wearing(OBJ_STAVES, STAFF_EARTH);

    se += you.scan_artefacts(ARTP_ENHANCE_EARTH);

    if (you.unrand_equipped(UNRAND_ELEMENTAL_STAFF))
        se++;

    return se;
}

int player_spec_air()
{
    int sa = 0;

    // Staves
    sa += you.wearing(OBJ_STAVES, STAFF_AIR);

    sa += you.scan_artefacts(ARTP_ENHANCE_AIR);

    if (you.unrand_equipped(UNRAND_ELEMENTAL_STAFF))
        sa++;

    if (you.unrand_equipped(UNRAND_AIR))
        sa++;

    return sa;
}

int player_spec_conj()
{
    int sc = 0;

    sc += you.wearing(OBJ_STAVES, STAFF_CONJURATION);
    sc += you.scan_artefacts(ARTP_ENHANCE_CONJ);

    return sc;
}

int player_spec_hex()
{
    int sh = 0;

    // Demonspawn mutation
    sh += you.get_mutation_level(MUT_HEX_ENHANCER);
    sh += you.scan_artefacts(ARTP_ENHANCE_HEXES);

    return sh;
}

int player_spec_summ()
{
    return you.scan_artefacts(ARTP_ENHANCE_SUMM);
}

int player_spec_forgecraft()
{
    return you.scan_artefacts(ARTP_ENHANCE_FORGECRAFT);
}

int player_spec_alchemy()
{
    int sp = 0;

    sp += you.wearing(OBJ_STAVES, STAFF_ALCHEMY);

    sp += you.scan_artefacts(ARTP_ENHANCE_ALCHEMY);

    if (you.unrand_equipped(UNRAND_OLGREB))
        sp++;

    return sp;
}

int player_spec_tloc()
{
    return you.scan_artefacts(ARTP_ENHANCE_TLOC);
}

// If temp is set to false, temporary sources of resistance won't be
// counted.
int player_prot_life(bool allow_random, bool temp, bool items)
{
    int pl = 0;

    // XX temp?
    if (you.has_mutation(MUT_VAMPIRISM) && !you.vampire_alive)
        pl = 3;

    // piety-based rN doesn't count as temporary (XX why)
    if (you_worship(GOD_SHINING_ONE))
    {
        if (you.piety >= piety_breakpoint(1))
            pl++;
        if (you.piety >= piety_breakpoint(3))
            pl++;
        if (you.piety >= piety_breakpoint(5))
            pl++;
    }

    pl += cur_form(temp)->res_neg();

    // completely stoned, unlike statue which has some life force
    if (temp && you.petrified())
        pl += 3;

    if (items)
    {
        // rings
        pl += you.wearing_jewellery(RING_POSITIVE_ENERGY);

        // armour (checks body armour only)
        pl += you.wearing_ego(OBJ_ARMOUR, SPARM_POSITIVE_ENERGY);

        // pearl dragon counts
        const item_def *body_armour = you.body_armour();
        if (body_armour)
            pl += armour_type_prop(body_armour->sub_type, ARMF_RES_NEG);

        // randart wpns
        pl += you.scan_artefacts(ARTP_NEGATIVE_ENERGY);

        // dragonskin cloak: 0.5 to draconic resistances
        if (allow_random && you.unrand_equipped(UNRAND_DRAGONSKIN)
            && coinflip())
        {
            pl++;
        }

        pl += you.wearing(OBJ_STAVES, STAFF_DEATH);
    }

    // undead/demonic power
    pl += you.get_mutation_level(MUT_NEGATIVE_ENERGY_RESISTANCE, temp);

    pl = min(3, pl);

    return pl;
}

// Even a slight speed advantage is very good... and we certainly don't
// want to go past 6 (see below). -- bwr
int player_movement_speed(bool check_terrain, bool temp)
{
    int mv = form_base_movespeed(you.form);

    if (check_terrain && feat_is_water(env.grid(you.pos())))
    {
        if (you.get_mutation_level(MUT_NIMBLE_SWIMMER) >= 2)
            mv -= 4;
        else if (you.in_water() && you.slow_in_water())
            mv += 6; // Wading through water is very slow.
    }

    // moving on liquefied ground, or while maintaining the
    // effect takes longer
    if (check_terrain && (you.liquefied_ground()
                          || you.duration[DUR_LIQUEFYING]))
    {
        mv += 3;
    }

    // armour
    if (you.unrand_equipped(UNRAND_LIGHTNING_SCALES))
        mv -= 1;

    mv += you.wearing_ego(OBJ_ARMOUR, SPARM_PONDEROUSNESS);

    // Cheibriados
    if (have_passive(passive_t::slowed))
        mv += 2 + min(div_rand_round(you.piety, 20), 8);
    else if (player_under_penance(GOD_CHEIBRIADOS))
        mv += 2 + min(div_rand_round(you.piety_max[GOD_CHEIBRIADOS], 20), 8);

    // Mutations: -2, -3, -4, unless innate and shapechanged.
    if (int fast = you.get_mutation_level(MUT_FAST))
        mv -= fast + 1;

    if (int slow = you.get_mutation_level(MUT_SLOW))
    {
        mv *= 10 + slow * 2;
        mv /= 10;
    }

    if (temp && you.duration[DUR_FROZEN])
        mv = div_rand_round(mv * 3, 2);

    if (temp && you.duration[DUR_SWIFTNESS] > 0)
    {
        if (you.attribute[ATTR_SWIFTNESS] > 0)
          mv = div_rand_round(3*mv, 4);
        else if (mv >= 8)
          mv = div_rand_round(3*mv, 2);
        else if (mv == 7)
          mv = div_rand_round(7*6, 5); // balance for the cap at 6
    }

    // We'll use the old value of six as a minimum, with haste this could
    // end up as a speed of three, which is about as fast as we want
    // the player to be able to go (since 3 is 3.33x as fast and 2 is 5x,
    // which is a bit of a jump, and a bit too fast) -- bwr
    // Currently Haste takes 6 to 4, which is 2.5x as fast as delay 10
    // and still seems plenty fast. -- elliptic
    if (mv < FASTEST_PLAYER_MOVE_SPEED)
        mv = FASTEST_PLAYER_MOVE_SPEED;

    return mv;
}

// This function differs from the above in that it's used to set the
// initial time_taken value for the turn. Everything else (movement,
// spellcasting, combat) applies a ratio to this value.
int player_speed()
{
    int ps = 10;

    // When paralysed, speed is irrelevant.
    if (you.cannot_act())
        return ps;

    if (you.duration[DUR_SLOW] || have_stat_zero())
        ps = haste_mul(ps);

    if (you.duration[DUR_BERSERK] && !have_passive(passive_t::no_haste))
        ps = berserk_div(ps);
    else if (you.duration[DUR_HASTE])
        ps = haste_div(ps);

    if (you.form == transformation::statue || you.duration[DUR_PETRIFYING])
    {
        ps *= 15;
        ps /= 10;
    }

    return ps;
}

bool is_effectively_light_armour(const item_def *item)
{
    return !item
           || (abs(property(*item, PARM_EVASION)) / 10 < 5);
}

bool player_effectively_in_light_armour()
{
    const item_def *armour = you.body_armour();
    return is_effectively_light_armour(armour);
}

// This function returns true if the player has a radically different
// shape... minor changes like blade hands don't count, also note
// that lich transformation doesn't change the character's shape
// (so we end up with Naga-liches, Spriggan-liches, Minotaur-liches)
// it just makes the character undead (with the benefits that implies). - bwr
bool player_is_shapechanged()
{
    // TODO: move into data
    return form_changes_physiology(you.form)
        && you.form != transformation::death;
}

bool player_acrobatic()
{
    return you.wearing_jewellery(AMU_ACROBAT)
        || you.has_mutation(MUT_ACROBATIC)
        || you.scan_artefacts(ARTP_ACROBAT);
}

void update_acrobat_status()
{
    if (!player_acrobatic())
        return;

    // Acrobat duration goes slightly into the next turn, giving the
    // player visual feedback of the EV bonus received.
    // This is assignment and not increment as acrobat duration depends
    // on player action.
    you.duration[DUR_ACROBAT] = you.time_taken+1;
    you.redraw_evasion = true;
}

// An evasion factor based on the player's body size, smaller == higher
// evasion size factor.
static int _player_evasion_size_factor(bool base = false)
{
    // XXX: you.body_size() implementations are incomplete, fix.
    const size_type size = you.body_size(PSIZE_BODY, base);
    return 2 * (SIZE_MEDIUM - size);
}

// Determines racial shield preferences for acquirement. (Formicids get a
// bonus for larger shields compared to other medium-sized races).
// TODO: rethink this
int player_shield_racial_factor()
{
    return you.has_mutation(MUT_QUADRUMANOUS) ? -2 // Same as trolls, etc.
           : _player_evasion_size_factor(true);
}


// The total EV penalty to the player for all their worn armour items
// with a base EV penalty (i.e. EV penalty as a base armour property,
// not as a randart property), EXCEPT body armour. Affects evasion only.
static int _player_aux_evasion_penalty(const int scale)
{
    int piece_armour_evasion_penalty = 0;

    // Some lesser armours have small penalties now (barding).
    vector<item_def*> armour = you.equipment.get_slot_items(SLOT_ALL_AUX_ARMOUR);
    for (item_def* item : armour)
    {
        // [ds] Evasion modifiers for armour are negatives, change
        // those to positive for penalty calc.
        const int penalty = (-property(*item, PARM_EVASION))/3;
        if (penalty > 0)
            piece_armour_evasion_penalty += penalty;
    }

    return piece_armour_evasion_penalty * scale / 10;
}

// Long-term player flat EV bonuses/penalties (eg: evasion rings, EV mutations, forms)
static int _player_base_evasion_modifiers()
{
    int evbonus = 0;

    evbonus += you.wearing_jewellery(RING_EVASION);

    evbonus += you.scan_artefacts(ARTP_EVASION);

    // mutations
    evbonus += you.get_mutation_level(MUT_GELATINOUS_BODY);

    if (you.get_mutation_level(MUT_DISTORTION_FIELD))
        evbonus += you.get_mutation_level(MUT_DISTORTION_FIELD) + 1;

    if (you.has_mutation(MUT_TENGU_FLIGHT))
        evbonus += 4;

    // transformation penalties/bonuses not covered by size alone:
    if (you.get_mutation_level(MUT_SLOW_REFLEXES))
        evbonus -= you.get_mutation_level(MUT_SLOW_REFLEXES) * 5;

    // Consider this a 'permanent' bonus, since players in forms will often
    // remain in that form for a long time. This is slightly untrue for
    // hostile polymorph, however I don't think any of them affect the player's
    // EV in this manner (tree form simply caps it in the same way as paralysis)
    evbonus += get_form()->ev_bonus();

    return evbonus;
}

// Transient player flat EV modifiers
static int _player_temporary_evasion_modifiers()
{
    int evbonus = 0;

    if (you.props.exists(WU_JIAN_HEAVENLY_STORM_KEY))
        evbonus += you.props[WU_JIAN_HEAVENLY_STORM_KEY].get_int();

    if (you.duration[DUR_AGILITY])
        evbonus += AGILITY_BONUS;

    // If you have an active amulet of the acrobat and just moved or waited,
    // get a massive EV bonus.
    if (acrobat_boost_active())
        evbonus += 15;

    if (you.duration[DUR_VERTIGO])
        evbonus -= 5;

    if (you.is_constricted())
        evbonus -= 10;

    return evbonus;
}

// Player EV multipliers for transient effects
static int _player_apply_evasion_multipliers(int prescaled_ev, const int scale)
{
    if (you.duration[DUR_PETRIFYING] || you.caught())
        prescaled_ev /= 2;

    // Merfolk get a 25% evasion bonus near water.
    if (feat_is_water(env.grid(you.pos()))
        && you.get_mutation_level(MUT_NIMBLE_SWIMMER) >= 2)
    {
        const int ev_bonus = max(2 * scale, prescaled_ev / 4);
        return prescaled_ev + ev_bonus;
    }

    return prescaled_ev;
}

/**
 * What is the player's bonus to EV from dodging when not paralysed, after
 * accounting for size & body armour penalties?
 *
 * First, calculate base dodge bonus (linear with dodging * dex),
 * and armour dodge penalty (base armour evp, increased for small races &
 * decreased for large, then with a magic "3" subtracted from it to make the
 * penalties not too harsh).
 *
 * If the player's strength is greater than the armour dodge penalty, return
 *      base dodge * (1 - dodge_pen / (str*2)).
 * E.g., if str is twice dodge penalty, return 3/4 of base dodge. If
 * str = dodge_pen * 4, return 7/8...
 *
 * If str is less than dodge penalty, return
 *      base_dodge * str / (dodge_pen * 2).
 * E.g., if str = dodge_pen / 2, return 1/4 of base dodge. if
 * str = dodge_pen / 4, return 1/8...
 *
 * For either equation, if str = dodge_pen, the result is base_dodge/2.
 *
 * @param scale     A scale to multiply the result by, to avoid precision loss.
 * @return          A bonus to EV, multiplied by the scale.
 */
static int _player_armour_adjusted_dodge_bonus(int scale)
{
    const int dodge_bonus =
        (800 + you.skill(SK_DODGING, 10) * you.dex() * 8) * scale
        / (20 - _player_evasion_size_factor()) / 10 / 10;

    const int armour_dodge_penalty = you.unadjusted_body_armour_penalty() - 3;
    if (armour_dodge_penalty <= 0)
        return dodge_bonus;

    const int str = max(1, you.strength());
    if (armour_dodge_penalty >= str)
        return dodge_bonus * str / (armour_dodge_penalty * 2);
    return dodge_bonus - dodge_bonus * armour_dodge_penalty / (str * 2);
}

// Total EV for player
static int _player_evasion(int final_scale, bool ignore_temporary)
{
    const int size_factor = _player_evasion_size_factor();
    const int scale = 100;
    const int size_base_ev = (10 + size_factor) * scale;

    // Calculate 'base' evasion from all permanent modifiers
    const int natural_evasion =
        size_base_ev
        + _player_armour_adjusted_dodge_bonus(scale)
        - you.adjusted_body_armour_penalty(scale)
        - you.adjusted_shield_penalty(scale)
        - _player_aux_evasion_penalty(scale)
        + _player_base_evasion_modifiers() * scale;

    // Everything below this are transient modifiers
    if (ignore_temporary)
        return (natural_evasion * final_scale) / scale;

    // Apply temporary bonuses, penalties, and multipliers
    int final_evasion =
       _player_apply_evasion_multipliers(natural_evasion, scale)
       + (_player_temporary_evasion_modifiers() * scale);

    // Cap EV at a very low level if the player cannot act or is a tree.
    if ((you.cannot_act() || you.form == transformation::tree))
    {
        final_evasion = min((2 + _player_evasion_size_factor() / 2) * scale,
                            final_evasion);
    }

    return (final_evasion * final_scale) / scale;
}

// Returns the spellcasting penalty (increase in spell failure) for the
// player's worn body armour and shield.
int player_armour_shield_spell_penalty()
{
    const int scale = 100;

    int body_armour_penalty =
        max(19 * you.adjusted_body_armour_penalty(scale), 0);

    // This is actually cutting the base ER of our armour by half (not 1/4th),
    // since that ER has already been squared by this point.
    if (you.has_mutation(MUT_RUNIC_MAGIC))
        body_armour_penalty /= 4;

    const int total_penalty = body_armour_penalty
                 + 19 * you.adjusted_shield_penalty(scale);

    return max(total_penalty, 0) / scale;
}

/**
 * How many spell-success-boosting ('wizardry') effects does the player have?
 *
 * @return    The number of wizardry effects.
 */
int player_wizardry()
{
    return you.wearing_jewellery(RING_WIZARDRY)
           + (you.get_mutation_level(MUT_BIG_BRAIN) == 3 ? 1 : 0)
           + you.scan_artefacts(ARTP_WIZARDRY);
}

int player_channelling()
{
    // Here and elsewhere, let's consider making this work for Dj.
    if (you.has_mutation(MUT_HP_CASTING))
        return 0;

    return 2 * you.unrand_equipped(UNRAND_WUCAD_MU)
           + you.wearing_ego(OBJ_ARMOUR, SPARM_ENERGY);
}

static int _sh_from_shield(const item_def &item)
{
    const int base_shield = property(item, PARM_AC) * 2;

    // bonus applied only to base, see above for effect:
    int shield = base_shield * 50;
    shield += base_shield * you.skill(SK_SHIELDS, 5) / 2;

    shield += item.plus * 200;

    shield += you.skill(SK_SHIELDS, 38);

    shield += 3 * 38;

    shield += you.dex() * 38 * (base_shield + 13) / 26;
    return shield;
}

/**
 * Calculate the SH value used internally.
 *
 * Exactly twice the value displayed to players, for legacy reasons.
 * @param       Whether to include temporary effects like TSO's divine shield.
 * @return      The player's current SH value.
 */
int player_shield_class(int scale, bool random, bool ignore_temporary)
{
    int shield = 0;

    if (!ignore_temporary && you.incapacitated())
        return 0;

    const item_def *shield_item = you.shield();
    if (is_shield(shield_item))
        shield += _sh_from_shield(*shield_item);

    // mutations
    // +4, +6, +8 (displayed values)
    shield += (you.get_mutation_level(MUT_LARGE_BONE_PLATES) > 0
               ? you.get_mutation_level(MUT_LARGE_BONE_PLATES) * 400 + 400
               : 0);

    // Icemail and Ephemeral Shield aren't active all of the time, so consider
    // them temporary; this behaviour is consistent with how icemail's AC
    // is dealt with.
    if (!ignore_temporary
        && you.get_mutation_level(MUT_CONDENSATION_SHIELD) > 0
        && !you.duration[DUR_ICEMAIL_DEPLETED])
    {
        shield += ICEMAIL_MAX * 100;
    }
    if (!ignore_temporary
        && you.get_mutation_level(MUT_EPHEMERAL_SHIELD)
        && you.duration[DUR_EPHEMERAL_SHIELD])
    {
        shield += you.get_mutation_level(MUT_EPHEMERAL_SHIELD) * 1400;
    }

    shield += qazlal_sh_boost() * 100;
    shield += you.wearing_jewellery(AMU_REFLECTION) * AMU_REFLECT_SH * 100;
    shield += you.scan_artefacts(ARTP_SHIELDING) * 200;

    return random ? div_rand_round(shield * scale, 100) : ((shield * scale) / 100);
}

/**
 * Calculate the SH value that should be displayed to players.
 *
 * Exactly half the internal value, for legacy reasons.
 * @param scale           How much to scale the value by (higher scale increases
                          precision, as SH is a number with 2 decimal places)
 * @param bool_temporary  Whether to include temporary effects like
                          TSO's divine shield.
 * @return                The SH value to be displayed.
 */
int player_displayed_shield_class(int scale, bool ignore_temporary)
{
    return player_shield_class(scale, false, ignore_temporary) / 2;
}

/**
 * Does the player have 'omnireflection' (the ability to reflect piercing
 * effects and enchantments)?
 *
 * @return      Whether the player has the Warlock's Mirror equipped.
 */
bool player_omnireflects()
{
    return you.unrand_equipped(UNRAND_WARLOCK_MIRROR);
}

void forget_map(bool rot)
{
    ASSERT(!crawl_state.game_is_arena());

    // If forgetting was intentional, clear the travel trail.
    if (!rot)
        clear_travel_trail();

    const bool rot_resist = player_in_branch(BRANCH_ABYSS)
                            && have_passive(passive_t::map_rot_res_abyss);
    const double geometric_chance = 0.99;
    const int radius = (rot_resist ? 200 : 100);

    const int scalar = 0xFF;
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        const coord_def &p = *ri;
        if (!env.map_knowledge(p).known() || you.see_cell(p))
            continue;

        if (player_in_branch(BRANCH_ABYSS)
            && env.map_knowledge(p).item()
            && env.map_knowledge(p).item()->is_type(OBJ_RUNES, RUNE_ABYSSAL))
        {
            continue;
        }

        if (rot)
        {
            const int dist = grid_distance(you.pos(), p);
            int chance = pow(geometric_chance,
                             max(1, (dist * dist - radius) / 40)) * scalar;
            if (x_chance_in_y(chance, scalar))
                continue;
        }

        if (you.see_cell(p))
            continue;

        env.map_knowledge(p).clear();
        if (env.map_forgotten)
            (*env.map_forgotten)(p).clear();
        StashTrack.update_stash(p);
#ifdef USE_TILE
        tile_forget_map(p);
#endif
    }

    ash_detect_portals(is_map_persistent());
#ifdef USE_TILE
    tiles.update_minimap_bounds();
#endif
}

int get_exp_progress()
{
    if (you.experience_level >= you.get_max_xl())
        return 0;

    const int current = exp_needed(you.experience_level);
    const int next    = exp_needed(you.experience_level + 1);
    if (next == current)
        return 0;
    return (you.experience - current) * 100 / (next - current);
}

static void _recharge_xp_evokers(int exp)
{
    FixedVector<item_def*, NUM_MISCELLANY> evokers(nullptr);
    list_charging_evokers(evokers);

    const int xp_by_xl = exp_needed(you.experience_level+1, 0)
                       - exp_needed(you.experience_level, 0);
    const int skill_denom = 3 + you.skill_rdiv(SK_EVOCATIONS, 1, 9);
    const int xp_factor = max(xp_by_xl / 5, 100) / skill_denom;

    if (you.wearing_ego(OBJ_GIZMOS, SPGIZMO_GADGETEER)
        || you.unrand_equipped(UNRAND_GADGETEER))
    {
        exp = exp * 130 / 100;
    }

    for (int i = 0; i < NUM_MISCELLANY; ++i)
    {
        item_def* evoker = evokers[i];
        if (!evoker)
            continue;

        int &debt = evoker_debt(evoker->sub_type);
        if (debt == 0)
            continue;

        int plus_factor = div_rand_round(5 * xp_factor, 5 + evoker_plus(evoker->sub_type));
        const int old_charges = evoker_charges(i);
        debt = max(0, debt - div_rand_round(exp, plus_factor));
        const int gained = evoker_charges(i) - old_charges;
        if (gained)
            print_xp_evoker_recharge(*evoker, gained, silenced(you.pos()));
    }
}

/// Make progress toward the abyss spawning an exit/stairs.
static void _reduce_abyss_xp_timer(int exp)
{
    if (!player_in_branch(BRANCH_ABYSS))
        return;

    const int xp_factor =
        max(min((int)exp_needed(you.experience_level+1, 0) / 7,
                you.experience_level * 425),
            you.experience_level*2 + 15) / 5;
    // Reduce abyss exit spawns in the Deep Abyss (J:6+) to preserve challenge.
    const int depth_factor = max(1, you.depth - 4);

    if (!you.props.exists(ABYSS_STAIR_XP_KEY))
        you.props[ABYSS_STAIR_XP_KEY] = EXIT_XP_COST;
    const int reqd_xp = you.props[ABYSS_STAIR_XP_KEY].get_int();
    const int new_req = reqd_xp - div_rand_round(exp, xp_factor * depth_factor);
    dprf("reducing xp timer from %d to %d (factor = %d)",
         reqd_xp, new_req, xp_factor);
    you.props[ABYSS_STAIR_XP_KEY].get_int() = new_req;
}

/// update penance for XP based gods
static void _handle_xp_penance(int exp)
{
    vector<god_type> xp_gods;
    for (god_iterator it; it; ++it)
    {
        if (xp_penance(*it))
            xp_gods.push_back(*it);
    }

    if (!xp_gods.empty())
    {
        god_type god = xp_gods[random2(xp_gods.size())];
        reduce_xp_penance(god, exp);
    }
}

/// update temporary mutations
static void _handle_temp_mutation(int exp)
{
    if (!(you.attribute[ATTR_TEMP_MUTATIONS] > 0))
        return;

    you.attribute[ATTR_TEMP_MUT_XP] -= exp;
    if (you.attribute[ATTR_TEMP_MUT_XP] <= 0)
        temp_mutation_wanes();
}

/// update hp drain
static void _handle_hp_drain(int exp)
{
    if (!you.hp_max_adj_temp)
        return;

    const int mul = you.has_mutation(MUT_PERSISTENT_DRAIN) ? 2 : 1;
    int loss = div_rand_round(exp, 4 * mul * calc_skill_cost(you.skill_cost_level));

    // Make it easier to recover from very heavy levels of draining
    // (they're nasty enough as it is)
    loss = loss * (1 + (-you.hp_max_adj_temp / (25.0f * mul)));

    dprf("Lost %d of %d draining points", loss, -you.hp_max_adj_temp);

    you.hp_max_adj_temp += loss;

    const bool drain_removed = you.hp_max_adj_temp >= 0;
    if (drain_removed)
        you.hp_max_adj_temp = 0;

    calc_hp();

    if (drain_removed)
        mprf(MSGCH_RECOVERY, "Your life force feels restored.");
}

static void _handle_breath_recharge(int exp)
{
    if (!species::is_draconian(you.species) || you.experience_level < 7
        || you.props[DRACONIAN_BREATH_USES_KEY].get_int() >= MAX_DRACONIAN_BREATH)
    {
        return;
    }

    if (!you.props.exists(DRACONIAN_BREATH_RECHARGE_KEY))
        you.props[DRACONIAN_BREATH_RECHARGE_KEY] = 50;

    int loss = div_rand_round(exp, calc_skill_cost(you.skill_cost_level));
    you.props[DRACONIAN_BREATH_RECHARGE_KEY].get_int() -= loss;

    if (you.props[DRACONIAN_BREATH_RECHARGE_KEY].get_int() <= 0)
    {
        you.props.erase(DRACONIAN_BREATH_RECHARGE_KEY);
        gain_draconian_breath_uses(1);
        mprf(MSGCH_DURATION, "You feel power welling in your lungs.");
    }
}

static void _handle_god_wrath(int exp)
{
    for (god_iterator it; it; ++it)
    {
        if (active_penance(*it))
        {
            you.attribute[ATTR_GOD_WRATH_XP] -= exp;
            while (you.attribute[ATTR_GOD_WRATH_XP] < 0)
            {
                you.attribute[ATTR_GOD_WRATH_COUNT]++;
                set_penance_xp_timeout();
            }
            break;
        }
    }
}

unsigned int gain_exp(unsigned int exp_gained)
{
    if (crawl_state.game_is_arena())
        return 0;

    you.experience_pool += exp_gained;

    if (player_under_penance(GOD_HEPLIAKLQANA))
        return 0; // no XP for you!

    const unsigned int max_gain = (unsigned int)MAX_EXP_TOTAL - you.experience;
    if (max_gain < exp_gained)
        return max_gain;
    return exp_gained;
}

void apply_exp()
{
    const unsigned int exp_gained = you.experience_pool;
    if (exp_gained == 0)
        return;

    you.experience_pool = 0;

    // xp-gated effects that don't use sprint inflation
    _handle_xp_penance(exp_gained);
    _handle_god_wrath(exp_gained);

    // evolution mutation timer
    if (you.attribute[ATTR_EVOL_XP] > 0 && you.can_safely_mutate())
        you.attribute[ATTR_EVOL_XP] -= exp_gained;

    // modified experience due to sprint inflation
    unsigned int skill_xp = exp_gained;
    if (crawl_state.game_is_sprint())
        skill_xp = sprint_modify_exp(skill_xp);

    // xp-gated effects that use sprint inflation
    _handle_temp_mutation(skill_xp);
    _recharge_xp_evokers(skill_xp);
    _reduce_abyss_xp_timer(skill_xp);
    _handle_hp_drain(skill_xp);
    _handle_breath_recharge(skill_xp);

    if (player_under_penance(GOD_HEPLIAKLQANA))
        return; // no xp for you!

    // handle actual experience gains,
    // i.e. XL and skills

    dprf("gain_exp: %d", exp_gained);

    if (you.experience + exp_gained > (unsigned int)MAX_EXP_TOTAL)
        you.experience = MAX_EXP_TOTAL;
    else
        you.experience += exp_gained;

    you.exp_available += 10 * skill_xp;

    train_skills();
    while (check_selected_skills()
           && you.exp_available >= calc_skill_cost(you.skill_cost_level))
    {
        train_skills();
    }

    level_change();
}

bool will_gain_life(int lev)
{
    if (lev < you.attribute[ATTR_LIFE_GAINED] - 2)
        return false;

    return you.lives - 1 + you.deaths < (lev - 1) / 3;
}

static bool _felid_extra_life()
{
    if (will_gain_life(you.max_level)
        && you.lives < 2)
    {
        you.lives++;
        mprf(MSGCH_INTRINSIC_GAIN, "Extra life!");
        you.attribute[ATTR_LIFE_GAINED] = you.max_level;
        // Should play the 1UP sound from SMB...
        return true;
    }
    return false;
}

static void _gain_and_note_hp_mp()
{
    const int old_mp = you.magic_points;
    const int old_maxmp = you.max_magic_points;

    // recalculate for game
    calc_hp(true);
    calc_mp();

    set_mp(old_maxmp > 0 ? old_mp * you.max_magic_points / old_maxmp
           : you.max_magic_points);

    // Get "real" values for note-taking, i.e. ignore Berserk,
    // transformations or equipped items.
    const int note_maxhp = get_real_hp(false, true);
    const int note_maxmp = get_real_mp(false);

    take_note(Note(NOTE_XP_LEVEL_CHANGE, you.experience_level, 0,
        make_stringf("HP: %d/%d MP: %d/%d",
            min(you.hp, note_maxhp), note_maxhp,
            min(you.magic_points, note_maxmp), note_maxmp)));
}

static int _rest_trigger_level(int max)
{
    return (max * Options.rest_wait_percent) / 100;
}

static bool _should_stop_resting(int cur, int max, bool check_opts=true)
{
    return cur == max || check_opts && cur == _rest_trigger_level(max);
}

/**
 * Calculate max HP changes and scale current HP accordingly.
 */
void calc_hp(bool scale)
{
    // Rounding must be down or Deep Dwarves would abuse certain values.
    // We can reduce errors by a factor of 100 by using partial hp we have.
    int oldhp = you.hp;
    int old_max = you.hp_max;

    you.hp_max = get_real_hp(true, true);

    // hp_max is not serialized, so fixup code that tries to trigger rescaling
    // during load should not actually do it.
    if (scale && old_max > 0)
    {
        int hp = you.hp * 100 + you.hit_points_regeneration;
        int new_max = you.hp_max;
        hp = hp * new_max / old_max;
        if (hp < 100)
            hp = 100;
        set_hp(min(hp / 100, you.hp_max));
        you.hit_points_regeneration = hp % 100;
    }

    you.hp = min(you.hp, you.hp_max);

    if (oldhp != you.hp || old_max != you.hp_max)
    {
        if (_should_stop_resting(you.hp, you.hp_max))
            interrupt_activity(activity_interrupt::full_hp);
        dprf("HP changed: %d/%d -> %d/%d", oldhp, old_max, you.hp, you.hp_max);
        you.redraw_hit_points = true;
    }
}

int xp_to_level_diff(int xp, int scale)
{
    ASSERT(xp >= 0);
    const int adjusted_xp = you.experience + xp;
    int projected_level = you.experience_level;
    while (you.experience >= exp_needed(projected_level + 1))
        projected_level++; // handle xl 27 chars
    int adjusted_level = projected_level;

    // closest whole number level, rounding down
    while (adjusted_xp >= (int) exp_needed(adjusted_level + 1))
        adjusted_level++;
    if (scale > 1)
    {
        // TODO: what is up with all the casts here?

        // decimal scaled version of current level including whatever fractional
        // part scale can handle
        const int cur_level_scaled = projected_level * scale
                + (you.experience - (int) exp_needed(projected_level)) * scale /
                    ((int) exp_needed(projected_level + 1)
                                    - (int) exp_needed(projected_level));

        // decimal scaled version of what adjusted_xp would get you
        const int adjusted_level_scaled = adjusted_level * scale
                + (adjusted_xp - (int) exp_needed(adjusted_level)) * scale /
                    ((int) exp_needed(adjusted_level + 1)
                                    - (int) exp_needed(adjusted_level));
        // TODO: this would be more usable with better rounding behaviour
        return adjusted_level_scaled - cur_level_scaled;
    }
    else
        return adjusted_level - projected_level;
}

static void _gain_innate_spells()
{
    auto &spell_vec = you.props[INNATE_SPELLS_KEY].get_vector();
    // Gain spells at every odd XL, starting at XL 3 and continuing to XL 27.
    for (int i = 0; i < spell_vec.size() && i < (you.experience_level - 1) / 2;
         i++)
    {
        const spell_type spell = (spell_type)spell_vec[i].get_int();
        if (spell == SPELL_NO_SPELL)
            continue; // spell lost due to lack of slots
        auto spindex = find(begin(you.spells), end(you.spells), spell);
        if (spindex != end(you.spells))
            continue; // already learned that one

        // XXX: this shouldn't be able to happen, but rare Wanderer starts
        // could give out too many spells and hit the cap.
        if (you.spell_no >= MAX_KNOWN_SPELLS)
        {
            for (int j = 0; j < i; j++)
            {
                const spell_type oldspell = (spell_type)spell_vec[j].get_int();
                if (oldspell == SPELL_NO_SPELL)
                    continue;
                auto oldindex = find(begin(you.spells), end(you.spells),
                                     oldspell);
                if (oldindex != end(you.spells))
                {
                    mpr("Your capacity for spells is full, and you lose "
                        "access to an earlier spell.");
                    del_spell_from_memory(oldspell);
                    spell_vec[j].get_int() = SPELL_NO_SPELL;
                    break;
                }
            }
            // If somehow a slot can't be freed, give up and lose the spell.
            // This extremely shouldn't happen.
            if (you.spell_no >= MAX_KNOWN_SPELLS)
            {
                spell_vec[i].get_int() = SPELL_NO_SPELL;
                continue;
            }
        }
        mprf("The power to cast %s wells up from within.", spell_title(spell));
        add_spell_to_memory(spell);
    }
}

/**
 * Handle the effects from a player's change in XL.
 * @param aux                     A string describing the cause of the level
 *                                change.
 * @param skip_attribute_increase If true and XL has increased, don't process
 *                                stat gains. Currently only used by wizmode
 *                                commands.
 */
void level_change(bool skip_attribute_increase)
{
    // necessary for the time being, as level_change() is called
    // directly sometimes {dlb}
    you.redraw_experience = true;

    while (you.experience < exp_needed(you.experience_level))
        lose_level();

    while (you.experience_level < you.get_max_xl()
           && you.experience >= exp_needed(you.experience_level + 1))
    {
        bool gained_felid_life = false;
        if (!skip_attribute_increase)
        {
            crawl_state.cancel_cmd_all();

            if (is_processing_macro())
                flush_input_buffer(FLUSH_ABORT_MACRO);
        }

        // [ds] Make sure we increment you.experience_level and apply
        // any stat/hp increases only after we've cleared all prompts
        // for this experience level. If we do part of the work before
        // the prompt, and a player on telnet gets disconnected, the
        // SIGHUP will save Crawl in the in-between state and rob the
        // player of their level-up perks.

        const int new_exp = you.experience_level + 1;
        // some species need to do this at a specific time; most just do it at the end
        bool updated_maxhp = false;

        if (new_exp <= you.max_level)
        {
            mprf(MSGCH_INTRINSIC_GAIN,
                 "Welcome back to level %d!", new_exp);

            // No more prompts for this XL past this point.

            you.experience_level = new_exp;
        }
        else  // Character has gained a new level
        {
            // Don't want to see the dead creature at the prompt.
            redraw_screen();
            update_screen();

            if (new_exp == 27)
                mprf(MSGCH_INTRINSIC_GAIN, "You have reached level 27, the final one!");
            else if (new_exp == you.get_max_xl())
                mprf(MSGCH_INTRINSIC_GAIN, "You have reached level %d, the highest you will ever reach!",
                        you.get_max_xl());
            else
            {
                mprf(MSGCH_INTRINSIC_GAIN, "You have reached level %d!",
                     new_exp);
            }

            const bool manual_stat_level = you.has_mutation(MUT_DIVINE_ATTRS)
                ? (new_exp % 3 == 0)  // 3,6,9,12...
                : (new_exp % 6 == 3); // 3,9,15,21,27

            // Must do this before actually changing experience_level,
            // so we will re-prompt on load if a hup is received.
            if (manual_stat_level && !skip_attribute_increase)
                if (!attribute_increase())
                    return; // abort level gain, the xp is still there

            // Set this after printing, since a more() might clear it.
            you.redraw_experience = true;

            crawl_state.stat_gain_prompt = false;
            you.experience_level = new_exp;
            you.max_level = you.experience_level;

#ifdef USE_TILE_LOCAL
            // In case of intrinsic ability changes.
            tiles.layout_statcol();
            redraw_screen();
            update_screen();
#endif
            if (!skip_attribute_increase)
                species_stat_gain(you.species);

            switch (you.species)
            {
            case SP_NAGA:
                if (!(you.experience_level % 3))
                {
                    mprf(MSGCH_INTRINSIC_GAIN, "Your skin feels tougher.");
                    you.redraw_armour_class = true;
                }
                break;

            case SP_BASE_DRACONIAN:
                if (you.experience_level >= 7)
                {
                    // XX make seed stable by choosing at birth
                    you.species = species::random_draconian_colour();

                    // We just changed our aptitudes, so some skills may now
                    // be at the wrong level (with negative progress); if we
                    // print anything in this condition, we might trigger a
                    // --More--, a redraw, and a crash (#6376 on Mantis).
                    //
                    // Hence we first fix up our skill levels silently (passing
                    // do_level_up = false) but save the old values; then when
                    // we want the messages later, we restore the old skill
                    // levels and call check_skill_level_change() again, this
                    // time passing do_level_up = true.

                    uint8_t saved_skills[NUM_SKILLS];
                    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
                    {
                        saved_skills[sk] = you.skills[sk];
                        check_skill_level_change(sk, false);
                    }
                    // The player symbol depends on species.
                    update_player_symbol();
#ifdef USE_TILE
                    init_player_doll();
#endif
                    mprf(MSGCH_INTRINSIC_GAIN,
                         "Your scales start taking on %s colour.",
                         article_a(species::scale_type(you.species)).c_str());

                    // Produce messages about skill increases/decreases. We
                    // restore one skill level at a time so that at most the
                    // skill being checked is at the wrong level.
                    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
                    {
                        const int oldapt = species_apt(sk, SP_BASE_DRACONIAN);
                        const int newapt = species_apt(sk, you.species);
                        if (oldapt != newapt)
                        {
                            mprf(MSGCH_INTRINSIC_GAIN, "You learn %s %s%s.",
                                 skill_name(sk),
                                 abs(oldapt - newapt) > 1 ? "much " : "",
                                 oldapt > newapt ? "slower" : "quicker");
                        }

                        you.skills[sk] = saved_skills[sk];
                        check_skill_level_change(sk);
                    }

                    // It's possible we passed a training target due to
                    // skills being rescaled to new aptitudes. Thus, we must
                    // check the training targets.
                    check_training_targets();

                    // Tell the player about their new species
                    for (auto &mut : species::fake_mutations(you.species, false))
                        mprf(MSGCH_INTRINSIC_GAIN, "%s", mut.c_str());

                    gain_draconian_breath_uses(2);

                    // needs to be done early here, so HP doesn't look drained
                    // when we redraw the screen
                    _gain_and_note_hp_mp();
                    updated_maxhp = true;
#ifdef USE_TILE_LOCAL
                    tiles.layout_statcol();
#endif
                    redraw_screen();
                    update_screen();
                }
                break;

            case SP_DEMONSPAWN:
            {
                bool gave_message = false;
                int level = 0;
                mutation_type first_body_facet = NUM_MUTATIONS;

                for (const player::demon_trait trait : you.demonic_traits)
                {
                    if (is_body_facet(trait.mutation))
                    {
                        if (first_body_facet < NUM_MUTATIONS
                            && trait.mutation != first_body_facet)
                        {
                            if (you.experience_level == level)
                            {
                                mprf(MSGCH_MUTATION, "You feel monstrous as "
                                     "your demonic heritage exerts itself.");
                                mark_milestone("monstrous", "discovered their "
                                               "monstrous ancestry!");
                                take_note(Note(NOTE_MESSAGE, 0, 0,
                                     "Discovered your monstrous ancestry."));
                            }
                            break;
                        }

                        if (first_body_facet == NUM_MUTATIONS)
                        {
                            first_body_facet = trait.mutation;
                            level = trait.level_gained;
                        }
                    }
                }

                for (const player::demon_trait trait : you.demonic_traits)
                {
                    if (trait.level_gained == you.experience_level)
                    {
                        if (!gave_message)
                        {
                            mprf(MSGCH_INTRINSIC_GAIN,
                                 "Your demonic ancestry asserts itself...");

                            gave_message = true;
                        }
                        perma_mutate(trait.mutation, 1, "demonic ancestry");
                    }
                }

                break;
            }

            case SP_COGLIN:
            {
                switch (you.experience_level)
                {
                    case 3:
                    case 7:
                    case 11:
                        coglin_announce_gizmo_name();
                        break;

                    case COGLIN_GIZMO_XL:
                    {
                        mpr("You feel a burst of inspiration! You are finally "
                            "ready to make a one-of-a-kind gizmo!");
                        mprf("(press <w>%c</w> on the <w>%s</w>bility menu to create your gizmo)",
                                get_talent(ABIL_INVENT_GIZMO, false).hotkey,
                                command_to_string(CMD_USE_ABILITY).c_str());
                    }
                    break;
                }
                break;
            }

            default:
                break;
            }

            if (you.has_mutation(MUT_MULTILIVED))
                gained_felid_life = _felid_extra_life();

            give_level_mutations(you.species, you.experience_level);

        }

        if (species::is_draconian(you.species) && !(you.experience_level % 3))
        {
            mprf(MSGCH_INTRINSIC_GAIN, "Your scales feel tougher.");
            you.redraw_armour_class = true;
        }
        if (!updated_maxhp)
            _gain_and_note_hp_mp();

        if (gained_felid_life)
            take_note(Note(NOTE_GAIN_LIFE, you.lives));

        if (you.has_mutation(MUT_INNATE_CASTER))
            _gain_innate_spells();

        xom_is_stimulated(12);
        if (in_good_standing(GOD_HEPLIAKLQANA))
            upgrade_hepliaklqana_ancestor();

        learned_something_new(HINT_NEW_LEVEL);
    }

    while (you.experience >= exp_needed(you.max_level + 1))
    {
        ASSERT(you.experience_level == you.get_max_xl());
        ASSERT(you.max_level < 127); // marshalled as an 1-byte value
        you.max_level++;
        if (you.has_mutation(MUT_MULTILIVED) && _felid_extra_life())
            take_note(Note(NOTE_GAIN_LIFE, you.lives));
    }

    you.redraw_title = true;

    update_whereis();

    // Hints mode arbitrarily ends at xp 7.
    if (crawl_state.game_is_hints() && you.experience_level >= 7)
        hints_finished();
}

void adjust_level(int diff, bool just_xp)
{
    ASSERT((uint64_t)you.experience <= (uint64_t)MAX_EXP_TOTAL);
    const int max_exp_level = you.get_max_xl();
    if (you.experience_level + diff < 1)
        you.experience = 0;
    else if (you.experience_level + diff >= max_exp_level)
    {
        const unsigned needed = exp_needed(max_exp_level);
        // Level gain when already at max should never reduce player XP;
        // but level loss (diff < 0) should.
        if (diff < 0 || you.experience < needed)
            you.experience = needed;
    }
    else
    {
        while (diff < 0 && you.experience >=
                exp_needed(max_exp_level))
        {
            // Having XP for level 53 and going back to 26 due to a single
            // card would mean your felid is not going to get any extra lives
            // in foreseable future.
            you.experience -= exp_needed(max_exp_level)
                    - exp_needed(max_exp_level - 1);
            diff++;
        }
        int old_min = exp_needed(you.experience_level);
        int old_max = exp_needed(you.experience_level + 1);
        int new_min = exp_needed(you.experience_level + diff);
        int new_max = exp_needed(you.experience_level + 1 + diff);
        dprf("XP before: %d\n", you.experience);
        dprf("%4.2f of %d..%d to %d..%d",
             (you.experience - old_min) * 1.0 / (old_max - old_min),
             old_min, old_max, new_min, new_max);

        you.experience = ((int64_t)(new_max - new_min))
                       * (you.experience - old_min)
                       / (old_max - old_min)
                       + new_min;
        dprf("XP after: %d\n", you.experience);
    }

    ASSERT((uint64_t)you.experience <= (uint64_t)MAX_EXP_TOTAL);

    if (!just_xp)
        level_change();
}

/**
 * Get the player's current stealth value.
 *
 * (Keep in mind, while tweaking this function: the order in which stealth
 * modifiers are applied is significant!)
 *
 * @return  The player's current stealth value.
 */
int player_stealth()
{
    ASSERT(!crawl_state.game_is_arena());
    // Extreme stealthiness can be enforced by wizmode stealth setting.
    if (crawl_state.disables[DIS_MON_SIGHT])
        return 1000;

    // berserking, sacrifice stealth.
    if (you.berserk() || you.get_mutation_level(MUT_NO_STEALTH))
        return 0;

    int stealth = you.dex() * 3;

    stealth += you.skill(SK_STEALTH, 15);

    if (you.confused())
        stealth /= 3;

    const item_def *arm = you.body_armour();
    if (arm)
    {
        // [ds] New stealth penalty formula from rob: SP = 6 * (EP^2)
        // Now 2 * EP^2 / 3 after EP rescaling.
        const int evp = you.unadjusted_body_armour_penalty();
        const int penalty = evp * evp * 2 / 3;
        stealth -= penalty;

        const int pips = armour_type_prop(arm->sub_type, ARMF_STEALTH);
        stealth += pips * STEALTH_PIP;
    }

    stealth += STEALTH_PIP * you.scan_artefacts(ARTP_STEALTH);
    stealth += STEALTH_PIP * you.wearing_ego(OBJ_ARMOUR, SPARM_STEALTH);
    stealth += STEALTH_PIP * you.wearing_jewellery(RING_STEALTH);

    if (you.duration[DUR_STEALTH])
        stealth += STEALTH_PIP * 2;

    // Mutations.
    stealth += STEALTH_PIP * you.get_mutation_level(MUT_NIGHTSTALKER) / 3;
    stealth += STEALTH_PIP * you.get_mutation_level(MUT_THIN_SKELETAL_STRUCTURE);
    stealth += STEALTH_PIP * you.get_mutation_level(MUT_CAMOUFLAGE);
    if (you.has_mutation(MUT_TRANSLUCENT_SKIN))
        stealth += STEALTH_PIP;

    // Radiating silence is the negative complement of shouting all the
    // time... a sudden change from background noise to no noise is going
    // to clue anything in to the fact that something is very wrong...
    // a personal silence spell would naturally be different, but this
    // silence radiates for a distance and prevents monster spellcasting,
    // which pretty much gives away the stealth game.
    if (you.duration[DUR_SILENCE])
        stealth -= STEALTH_PIP;

    // Bloodless vampires are stealthier.
    if (you.has_mutation(MUT_VAMPIRISM) && !you.vampire_alive)
        stealth += STEALTH_PIP * 2;

    if (feat_is_water(env.grid(you.pos())))
    {
        if (you.has_mutation(MUT_NIMBLE_SWIMMER))
            stealth += STEALTH_PIP;
        else if (you.in_water() && !you.can_swim() && !you.extra_balanced())
            stealth /= 2;       // splashy-splashy
    }

    // If you've been tagged with Corona or are Glowing, the glow
    // makes you extremely unstealthy.
    if (you.backlit())
        stealth = stealth * 2 / 5;

    // On the other hand, shrouding has the reverse effect, if you know
    // how to make use of it:
    if (you.umbra() && you.umbra_radius() >= 0)
        stealth = stealth * 3 / 2;

    // If you're surrounded by a storm, you're inherently pretty conspicuous.
    if (have_passive(passive_t::storm_shield))
    {
        stealth = stealth
                  * (MAX_PIETY - min((int)you.piety, piety_breakpoint(5)))
                  / (MAX_PIETY - piety_breakpoint(0));
    }
    // The shifting glow from the Orb, while too unstable to negate invis
    // or affect to-hit, affects stealth even more than regular glow.
    if (player_has_orb() || you.unrand_equipped(UNRAND_CHARLATANS_ORB))
        stealth /= 3;

    // Cap minimum stealth during Nightfall at 100. (0, otherwise.)
    stealth = max(you.duration[DUR_PRIMORDIAL_NIGHTFALL] ? 100 : 0, stealth);

    return stealth;
}

// Is a given duration about to expire?
bool dur_expiring(duration_type dur)
{
    const int value = you.duration[dur];
    if (value <= 0)
        return false;

    // XXX: reconsider using DUR_TRANSFORM here
    if (dur == DUR_TRANSFORMATION && you.form == you.default_form)
        return false;

    return value <= duration_expire_point(dur);
}

static void _display_char_status(int value, const char *fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);

    string msg = vmake_stringf(fmt, argp);

    if (you.wizard)
        mprf("%s (%d).", msg.c_str(), value);
    else
        mprf("%s.", msg.c_str());

    va_end(argp);
}

static void _display_vampire_status()
{
    string msg = "At your current blood state you ";
    vector<const char *> attrib;

    if (!you.vampire_alive)
    {
        attrib.push_back("are immune to poison");
        attrib.push_back("significantly resist cold");
        attrib.push_back("are immune to negative energy");
        attrib.push_back("resist torment");
        attrib.push_back("do not heal with monsters in sight.");
    }
    else
        attrib.push_back("heal quickly.");

    if (!attrib.empty())
    {
        msg += comma_separated_line(attrib.begin(), attrib.end());
        mpr(msg);
    }
}

static void _display_movement_speed()
{
    const int move_cost = (player_speed() * player_movement_speed()) / 10;

    const bool water  = you.in_liquid();
    const bool swim   = you.swimming();

    const bool fly    = you.airborne();
    const bool swift  = (you.duration[DUR_SWIFTNESS] > 0
                         && you.attribute[ATTR_SWIFTNESS] >= 0);
    const bool antiswift = (you.duration[DUR_SWIFTNESS] > 0
                            && you.attribute[ATTR_SWIFTNESS] < 0);

    _display_char_status(move_cost, "Your %s speed is %s%s%s",
          // order is important for these:
          (swim)    ? "swimming" :
          (water)   ? "wading" :
          (fly)     ? "flying"
                    : "movement",

          (swift) ? "aided by the wind" :
          (antiswift) ? "hindered by the wind" : "",

          (swift) ? ((move_cost >= 10) ? ", but still "
                                                 : " and ") :
          (antiswift) ? ((move_cost <= 10) ? ", but still "
                                                     : " and ")
                            : "",

          (move_cost <   8) ? "very quick" :
          (move_cost <  10) ? "quick" :
          (move_cost == 10) ? "average" :
          (move_cost <  13) ? "slow"
                            : "very slow");
}

static void _display_tohit()
{
#ifdef DEBUG_DIAGNOSTICS
    melee_attack attk(&you, nullptr);

    const int to_hit = attk.calc_to_hit(false);

    dprf("To-hit: %d", to_hit);
#endif
}

static int _delay(const item_def *weapon)
{
    if (!weapon || !is_range_weapon(*weapon))
        return you.attack_delay().expected();
    item_def fake_proj;
    populate_fake_projectile(*weapon, fake_proj);
    return you.attack_delay(&fake_proj).expected();
}

static bool _at_min_delay(const item_def *weapon)
{
    return weapon
           && you.skill(item_attack_skill(*weapon))
              >= weapon_min_delay_skill(*weapon);
}

/**
 * Print a message indicating the player's attack delay with their current
 * weapon(s) (if applicable).
 */
static void _display_attack_delay(const item_def *offhand)
{
    const item_def* weapon = you.weapon();
    const int delay = _delay(weapon);
    const bool at_min_delay = _at_min_delay(weapon)
                              && (!offhand || _at_min_delay(offhand));

    // Assume that we never have a shield penalty with an offhand weapon,
    // and we only have an armour penalty with the offhand if we do with
    // the primary.
    const bool shield_penalty = you.adjusted_shield_penalty(2) > 0;
    const bool armour_penalty = is_slowed_by_armour(weapon)
                                && you.adjusted_body_armour_penalty(2) > 0;
    string penalty_msg = "";
    if (shield_penalty || armour_penalty)
    {
        // TODO: add amount, as in item description (see _describe_armour)
        // double parens are awkward
        penalty_msg =
            make_stringf( " (and is slowed by your %s)",
                         shield_penalty && armour_penalty ? "shield and armour" :
                         shield_penalty ? "shield" : "armour");
    }

    mprf("Your attack delay is about %.1f%s%s.",
         (float)delay / 10,
         at_min_delay ?
            " (and cannot be improved with additional weapon skill)" : "",
         penalty_msg.c_str());
}

/**
 * Print a message listing double the player's best-case damage with their current
 * weapon (if applicable), or with unarmed combat (if not).
 */
static void _display_damage_rating(const item_def *weapon)
{
    string weapon_name;
    if (weapon)
        weapon_name = weapon->name(DESC_YOUR);
    else
        weapon_name = "unarmed combat";

    if (weapon && is_unrandom_artefact(*weapon, UNRAND_WOE))
        mpr(uppercase_first(damage_rating(weapon)));
    else
    {
        mprf("Your damage rating with %s is about %s",
             weapon_name.c_str(),
             damage_rating(weapon).c_str());
    }
    return;
}

// forward declaration
static string _constriction_description();

void display_char_status()
{
    const int halo_size = you.halo_radius();
    if (halo_size >= 0)
    {
        if (halo_size > 5)
            mpr("You are illuminated by a large divine halo.");
        else if (halo_size > 3)
            mpr("You are illuminated by a divine halo.");
        else
            mpr("You are illuminated by a small divine halo.");
    }
    else if (you.haloed())
        mpr("An external divine halo illuminates you.");

    if (you.has_mutation(MUT_VAMPIRISM))
        _display_vampire_status();

    status_info inf;
    for (unsigned i = 0; i <= STATUS_LAST_STATUS; ++i)
    {
        if (fill_status_info(i, inf) && !inf.long_text.empty())
            mpr(inf.long_text);
    }
    string cinfo = _constriction_description();
    if (!cinfo.empty())
        mpr(cinfo);

    const item_def* offhand = you.offhand_weapon();
    _display_movement_speed();
    _display_tohit();
    _display_attack_delay(offhand);
    _display_damage_rating(you.weapon());
    if (offhand)
        _display_damage_rating(offhand);

    // Display base attributes, if necessary.
    if (innate_stat(STAT_STR) != you.strength()
        || innate_stat(STAT_INT) != you.intel()
        || innate_stat(STAT_DEX) != you.dex())
    {
        mprf("Your base attributes are Str %d, Int %d, Dex %d.",
             innate_stat(STAT_STR),
             innate_stat(STAT_INT),
             innate_stat(STAT_DEX));
    }
}

bool player::clarity(bool items) const
{
    if (you.get_mutation_level(MUT_CLARITY))
        return true;

    if (have_passive(passive_t::clarity))
        return true;

    return actor::clarity(items);
}

bool player::faith(bool items) const
{
    return you.has_mutation(MUT_FAITH) || actor::faith(items);
}

bool player::reflection(bool items) const
{
    if (you.duration[DUR_DIVINE_SHIELD])
        return true;

    return actor::reflection(items);
}

/// Does the player have permastasis?
bool player::stasis() const
{
    return species == SP_FORMICID;
}

bool player::can_burrow() const
{
    return species == SP_FORMICID;
}

bool player::cloud_immune(bool items) const
{
    return have_passive(passive_t::cloud_immunity)
           || you.duration[DUR_TEMP_CLOUD_IMMUNITY]
           || actor::cloud_immune(items);
}

/**
 * How much XP does it take to reach the given XL from 0?
 *
 *  @param lev          The XL to reach.
 *  @param exp_apt      The XP aptitude to use. If -99, use the current species'.
 *  @return     The total number of XP points needed to get to the given XL.
 */
unsigned int exp_needed(int lev, int exp_apt)
{
    unsigned int level = 0;

    // Note: For historical reasons, all of the following numbers are for a
    // species (like human) with XP aptitude 1, not 0 as one might expect.

    // Basic plan:
    // Section 1: levels  1- 5, second derivative goes 10-10-20-30.
    // Section 2: levels  6-13, second derivative is exponential/doubling.
    // Section 3: levels 14-27, second derivative is constant at 8470.

    // Here's a table:
    //
    // level      xp      delta   delta2
    // =====   =======    =====   ======
    //   1           0        0       0
    //   2          10       10      10
    //   3          30       20      10
    //   4          70       40      20
    //   5         140       70      30
    //   6         270      130      60
    //   7         520      250     120
    //   8        1010      490     240
    //   9        1980      970     480
    //  10        3910     1930     960
    //  11        7760     3850    1920
    //  12       15450     7690    3840
    //  13       26895    11445    3755
    //  14       45585    18690    7245
    //  15       72745    27160    8470
    //  16      108375    35630    8470
    //  17      152475    44100    8470
    //  18      205045    52570    8470
    //  19      266085    61040    8470
    //  20      335595    69510    8470
    //  21      413575    77980    8470
    //  22      500025    86450    8470
    //  23      594945    94920    8470
    //  24      698335    103390   8470
    //  25      810195    111860   8470
    //  26      930525    120330   8470
    //  27     1059325    128800   8470

    // If you've sacrificed experience, XP costs are adjusted as if
    // you were still your original (higher) level.
    if (exp_apt == -99)
        lev += RU_SAC_XP_LEVELS * you.get_mutation_level(MUT_INEXPERIENCED);

    switch (lev)
    {
    case 1:
        level = 1;
        break;
    case 2:
        level = 10;
        break;
    case 3:
        level = 30;
        break;
    case 4:
        level = 70;
        break;

    default:
        if (lev < 13)
        {
            lev -= 4;
            level = 10 + 10 * lev + (60 << lev);
        }
        else
        {
            lev -= 12;
            level = 16675 + 5985 * lev + 4235 * lev * lev;
        }
        break;
    }

    if (exp_apt == -99)
        exp_apt = species::get_exp_modifier(you.species);

    return (unsigned int) ((level - 1) * apt_to_factor(exp_apt - 1));
}

// returns bonuses from rings of slaying, etc.
int slaying_bonus(bool throwing, bool random)
{
    int ret = 0;

    ret += you.wearing_jewellery(RING_SLAYING);
    ret += you.scan_artefacts(ARTP_SLAYING);
    if (you.wearing_ego(OBJ_ARMOUR, SPARM_HURLING) && throwing)
        ret += 4;

    ret += 3 * augmentation_amount();
    ret += you.get_mutation_level(MUT_SHARP_SCALES);

    if (you.duration[DUR_FUGUE])
        ret += you.props[FUGUE_KEY].get_int();

    if (you.duration[DUR_HORROR])
        ret -= you.props[HORROR_PENALTY_KEY].get_int();

    if (you.props.exists(WU_JIAN_HEAVENLY_STORM_KEY))
        ret += you.props[WU_JIAN_HEAVENLY_STORM_KEY].get_int();

    ret += get_form()->slay_bonus(random);

    return ret;
}

// Checks each equip slot for a randart, and adds up all of those with
// a given property. Slow if any randarts are worn, so avoid where
// possible. If `matches' is non-nullptr, items with nonzero property are
// pushed onto *matches.
int player::scan_artefacts(artefact_prop_type which_property,
                           vector<const item_def *> *matches) const
{
    // First, fetch the property total from our cache.
    int retval = you.equipment.get_artprop(which_property);

    // Don't bother iterating through each artefact individually unless we're
    // actually trying to retrieve a list of them.
    if (!matches)
        return retval;

    for (auto& entry : you.equipment.items)
    {
        const item_def& item = entry.get_item();
        if (is_artefact(item) && artefact_property(item, which_property))
            matches->push_back(&item);
    }

    if (active_talisman.defined() && is_artefact(active_talisman)
        && artefact_property(active_talisman, which_property))
    {
        matches->push_back(&active_talisman);
    }

    return retval;
}

bool player::using_talisman(const item_def &talisman) const
{
    if (!active_talisman.defined())
        return false;
    if (talisman.sub_type != active_talisman.sub_type)
        return false;
    const bool using_artefact = is_artefact(active_talisman);
    const bool other_artefact = is_artefact(talisman);
    if (using_artefact != other_artefact)
        return false;
    if (!using_artefact)
        return true;
    // hack alert!
    return get_artefact_name(talisman) == get_artefact_name(active_talisman);
}

/**
 * Is the player wearing an Infusion-branded piece of armour? If so, returns
 * how much MP they can spend per attack, depending on their available MP (or
 * HP if they're a Djinn).
 */
int player::infusion_amount() const
{
    int cost = 0;
    if (you.unrand_equipped(UNRAND_POWER_GLOVES))
        cost = you.has_mutation(MUT_HP_CASTING) ? 0 : 999;
    else
        cost = wearing_ego(OBJ_ARMOUR, SPARM_INFUSION);

    if (you.has_mutation(MUT_HP_CASTING))
        return min(you.hp - 1, cost);
    else
        return min(you.magic_points, cost);
}

void dec_hp(int hp_loss, bool fatal, const char *aux)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!fatal && you.hp < 1)
        you.hp = 1;

    if (!fatal && hp_loss >= you.hp)
        hp_loss = you.hp - 1;

    if (hp_loss < 1)
        return;

    // If it's not fatal, use ouch() so that notes can be taken. If it IS
    // fatal, somebody else is doing the bookkeeping, and we don't want to mess
    // with that.
    if (!fatal && aux)
        ouch(hp_loss, KILLED_BY_SOMETHING, MID_NOBODY, aux);
    else
        you.hp -= hp_loss;

    you.redraw_hit_points = true;
}

void calc_mp(bool scale)
{
    int old_max = you.max_magic_points;
    you.max_magic_points = get_real_mp(true);
    if (scale)
    {
        int mp = you.magic_points * 100 + you.magic_points_regeneration;
        int new_max = you.max_magic_points;
        if (old_max)
            mp = mp * new_max / old_max;
        you.magic_points = min(mp / 100, you.max_magic_points);
    }
    else
        you.magic_points = min(you.magic_points, you.max_magic_points);
    you.redraw_magic_points = true;
}

void flush_mp()
{
    if (Options.magic_point_warning
        && you.magic_points < you.max_magic_points
                              * Options.magic_point_warning / 100)
    {
        mprf(MSGCH_DANGER, "* * * LOW MAGIC WARNING * * *");
    }

    take_note(Note(NOTE_MP_CHANGE, you.magic_points, you.max_magic_points));
    you.redraw_magic_points = true;
}

void flush_hp()
{
    if (Options.hp_warning
        && you.hp <= (you.hp_max * Options.hp_warning) / 100)
    {
        flash_view_delay(UA_HP, RED, 50);
        mprf(MSGCH_DANGER, "* * * LOW HITPOINT WARNING * * *");
        dungeon_events.fire_event(DET_HP_WARNING);
    }
    you.redraw_hit_points = true;
}

static void _dec_mp(int mp_loss, bool silent)
{
    ASSERT(!crawl_state.game_is_arena());

    if (mp_loss < 1)
        return;

    you.magic_points -= mp_loss;

    you.magic_points = max(0, you.magic_points);
    if (!silent)
        flush_mp();
}

void drain_mp(int mp_loss, bool ignore_resistance)
{
    if (!ignore_resistance && you.has_mutation(MUT_INVIOLATE_MAGIC))
        mp_loss = mp_loss / 3;

    _dec_mp(mp_loss, false);
}

void pay_hp(int cost)
{
    you.hp -= cost;
    ASSERT(you.hp);
}

void pay_mp(int cost)
{
    if (you.has_mutation(MUT_HP_CASTING))
        pay_hp(cost);
    else
        _dec_mp(cost, true);
}

void refund_hp(int cost)
{
    you.hp += cost;
}

void refund_mp(int cost)
{
    if (you.has_mutation(MUT_HP_CASTING))
    {
        you.hp += cost;
        you.redraw_hit_points = true;
    }
    else
    {
        inc_mp(cost, true);
        you.redraw_magic_points = true;
    }
}

void finalize_mp_cost(bool addl_hp_cost)
{
    if (you.has_mutation(MUT_HP_CASTING) || addl_hp_cost)
        flush_hp();
    if (!you.has_mutation(MUT_HP_CASTING))
        flush_mp();
}

bool enough_hp(int minimum, bool suppress_msg, bool abort_macros)
{
    ASSERT(!crawl_state.game_is_arena());

    if (you.duration[DUR_DEATHS_DOOR])
    {
        if (!suppress_msg)
            mpr("You cannot pay life while functionally dead.");

        if (abort_macros)
        {
            crawl_state.cancel_cmd_again();
            crawl_state.cancel_cmd_repeat();
        }
        return false;
    }

    // We want to at least keep 1 HP. -- bwr
    if (you.hp < minimum + 1)
    {
        if (!suppress_msg)
            mpr("You don't have enough health at the moment.");

        if (abort_macros)
        {
            crawl_state.cancel_cmd_again();
            crawl_state.cancel_cmd_repeat();
        }
        return false;
    }

    return true;
}

bool enough_mp(int minimum, bool suppress_msg, bool abort_macros)
{
    ASSERT(!crawl_state.game_is_arena());

    if (you.has_mutation(MUT_HP_CASTING))
        return enough_hp(minimum, suppress_msg, abort_macros);

    if (you.magic_points < minimum)
    {
        if (!suppress_msg)
        {
            if (get_real_mp(true) < minimum)
                mpr("You don't have enough magic capacity.");
            else
                mpr("You don't have enough magic at the moment.");
        }
        if (abort_macros)
        {
            crawl_state.cancel_cmd_again();
            crawl_state.cancel_cmd_repeat();
        }
        return false;
    }

    return true;
}

void inc_mp(int mp_gain, bool silent)
{
    ASSERT(!crawl_state.game_is_arena());

    if (mp_gain < 1 || you.magic_points >= you.max_magic_points)
        return;

    you.magic_points += mp_gain;

    if (you.magic_points > you.max_magic_points)
        you.magic_points = you.max_magic_points;

    if (!silent)
    {
        if (_should_stop_resting(you.magic_points, you.max_magic_points))
            interrupt_activity(activity_interrupt::full_mp);
        you.redraw_magic_points = true;
    }
}

// Note that "max_too" refers to the base potential, the actual
// resulting max value is subject to penalties, bonuses, and scalings.
// To avoid message spam, don't take notes when HP increases.
void inc_hp(int hp_gain, bool silent)
{
    ASSERT(!crawl_state.game_is_arena());

    if (hp_gain < 1 || you.hp >= you.hp_max)
        return;

    you.hp += hp_gain;

    if (you.hp > you.hp_max)
        you.hp = you.hp_max;

    if (!silent)
    {
        if (_should_stop_resting(you.hp, you.hp_max))
            interrupt_activity(activity_interrupt::full_hp);

        you.redraw_hit_points = true;
    }
}

int undrain_hp(int hp_recovered)
{
    int hp_balance = 0;
    if (hp_recovered > -you.hp_max_adj_temp)
    {
        hp_balance = hp_recovered + you.hp_max_adj_temp;
        you.hp_max_adj_temp = 0;
    }
    else
        you.hp_max_adj_temp += hp_recovered;
    calc_hp();

    you.redraw_hit_points = true;
    if (!player_drained())
        you.redraw_magic_points = true;
    return hp_balance;
}

int player_drained()
{
    return -you.hp_max_adj_temp;
}

void rot_mp(int mp_loss)
{
    you.mp_max_adj -= mp_loss;
    calc_mp();

    you.redraw_magic_points = true;
}

void dec_max_hp(int hp_loss)
{
    you.hp_max_adj_perm -= hp_loss;
    calc_hp();

    take_note(Note(NOTE_MAXHP_CHANGE, you.hp_max));
    you.redraw_hit_points = true;
}

void set_hp(int new_amount)
{
    ASSERT(!crawl_state.game_is_arena());

    you.hp = new_amount;

    if (you.hp > you.hp_max)
        you.hp = you.hp_max;

    // Must remain outside conditional, given code usage. {dlb}
    you.redraw_hit_points = true;
}

void set_mp(int new_amount)
{
    ASSERT(!crawl_state.game_is_arena());

    you.magic_points = new_amount;

    if (you.magic_points > you.max_magic_points)
        you.magic_points = you.max_magic_points;

    take_note(Note(NOTE_MP_CHANGE, you.magic_points, you.max_magic_points));

    // Must remain outside conditional, given code usage. {dlb}
    you.redraw_magic_points = true;
}

/**
 * Get the player's max HP
 * @param trans          Whether to include transformations, berserk,
 *                       items etc.
 * @param drained        Whether to include the effects of draining.
 * @return               The player's calculated max HP.
 */
int get_real_hp(bool trans, bool drained)
{
    int hitp;

    hitp  = you.experience_level * 11 / 2 + 8;
    hitp += you.hp_max_adj_perm;
    // Important: we shouldn't add Heroism boosts here.
    // ^ The above is a 2011 comment from 1kb, in 2021 this isn't
    // archaeologied for further explanation, but the below now adds Ash boosts
    // to fighting to the HP calculation while preventing it for Heroism
    // - eb
    hitp += you.experience_level * you.skill(SK_FIGHTING, 5, false, false) / 70
          + (you.skill(SK_FIGHTING, 3, false, false) + 1) / 2;

    // Racial modifier.
    hitp *= 10 + species::get_hp_modifier(you.species);
    hitp /= 10;

    hitp += you.get_mutation_level(MUT_FLAT_HP) * 4;

    const bool hep_frail = have_passive(passive_t::frail)
                           || player_under_penance(GOD_HEPLIAKLQANA);

    // Mutations that increase HP by a percentage
    hitp *= 100 + (you.get_mutation_level(MUT_ROBUST) * 10)
                + (you.get_mutation_level(MUT_RUGGED_BROWN_SCALES) ?
                   you.get_mutation_level(MUT_RUGGED_BROWN_SCALES) * 2 + 1 : 0)
                - (you.get_mutation_level(MUT_FRAIL) * 10)
                - (hep_frail ? 10 : 0)
                - (!you.vampire_alive ? 20 : 0);

    hitp /= 100;

    if (drained)
        hitp += you.hp_max_adj_temp;

    if (trans)
        hitp += you.scan_artefacts(ARTP_HP);

    // Being berserk makes you resistant to damage. I don't know why.
    if (trans && you.berserk())
    {
        if (you.unrand_equipped(UNRAND_BEAR_SPIRIT))
            hitp *= 2;
        else
            hitp = hitp * 3 / 2;
    }

    // TODO: should this also be in an if (trans) block?
    hitp *= 100 + you.attribute[ATTR_DIVINE_VIGOUR] * 5;
    hitp /= 100;

    if (trans)
        hitp = get_form()->mult_hp(hitp);

    return max(1, hitp);
}

int get_real_mp(bool include_items)
{
    if (you.has_mutation(MUT_HP_CASTING))
        return 0;

    const int scale = 100;
    int spellcasting = you.skill(SK_SPELLCASTING, 1 * scale, false, false);
    int scaled_xl = you.experience_level * scale;

    // the first 4 experience levels give an extra .5 mp up to your spellcasting
    // the last 4 give no mp
    int enp = min(23 * scale, scaled_xl);

    int spell_extra = spellcasting; // 100%
    int invoc_extra = you.skill(SK_INVOCATIONS, 1 * scale, false, false) / 2; // 50%
    int highest_skill = max(spell_extra, invoc_extra);
    enp += highest_skill + min(8 * scale, min(highest_skill, scaled_xl)) / 2;

    // Analogous to ROBUST/FRAIL
    enp *= 100 + (you.get_mutation_level(MUT_HIGH_MAGIC) * 10)
               - (you.get_mutation_level(MUT_LOW_MAGIC) * 10);
    enp /= 100 * scale;
//    enp = stepdown_value(enp, 9, 18, 45, 100)
    enp += species::get_mp_modifier(you.species);

    // This is our "rotted" base, applied after multipliers
    enp += you.mp_max_adj;

    // Now applied after scaling so that power items are more useful -- bwr
    if (include_items)
    {
        enp += 9 * you.wearing_jewellery(RING_MAGICAL_POWER);
        enp +=     you.scan_artefacts(ARTP_MAGICAL_POWER);
    }

    enp *= 100 + you.attribute[ATTR_DIVINE_VIGOUR] * 5;
    enp /= 100;

    if (include_items && you.wearing_ego(OBJ_WEAPONS, SPWPN_ANTIMAGIC))
        enp /= 3;

    enp = max(enp, 0);

    return enp;
}

/// Does the player currently regenerate hp? Used for resting.
bool player_regenerates_hp()
{
    return !regeneration_is_inhibited()
#if TAG_MAJOR_VERSION == 34
    && !you.has_mutation(MUT_NO_REGENERATION)
#endif
    ;
}

bool player_regenerates_mp()
{
    // Djinn don't do the whole "mp" thing.
    if (you.has_mutation(MUT_HP_CASTING))
        return false;
#if TAG_MAJOR_VERSION == 34
    // Don't let DD use guardian spirit for free HP, since their
    // damage shaving is enough. (due, dpeg)
    if (you.spirit_shield() && you.species == SP_DEEP_DWARF)
        return false;
#endif
    return true;
}

int get_contamination_level()
{
    const int glow = you.magic_contamination;

    if (glow > 60000)
        return glow / 20000 + 4;
    if (glow > 40000)
        return 6;
    if (glow > 25000)
        return 5;
    if (glow > 15000)
        return 4;
    if (glow > 5000)
        return 3;
    if (glow > 3000) // An indicator that using another contamination-causing
        return 2;    // ability might risk causing yellow glow.
    if (glow > 0)
        return 1;

    return 0;
}

bool player_severe_contamination()
{
    return get_contamination_level() >= SEVERE_CONTAM_LEVEL;
}

/**
 * Provide a description of the given contamination 'level'.
 *
 * @param cont  A contamination 'tier', corresponding to a nonlinear
 *              contamination value; generated by get_contamination_level().
 *
 * @return      A string describing the player when in the given contamination
 *              level.
 */
string describe_contamination(int cont)
{
    /// Mappings from contamination levels to descriptions.
    static const string contam_descriptions[] =
    {
        "",
        "You are very lightly contaminated with residual magic.",
        "You are lightly contaminated with residual magic.",
        "You are contaminated with residual magic.",
        "You are heavily infused with residual magic.",
        "You are practically glowing with residual magic!",
        "Your entire body has taken on an eerie glow!",
        "You are engulfed in a nimbus of crackling magics!",
    };

    ASSERT(cont >= 0);
    return contam_descriptions[min((size_t) cont,
                                   ARRAYSZ(contam_descriptions) - 1)];
}

// Controlled is true if the player actively did something to cause
// contamination.
void contaminate_player(int change, bool controlled, bool msg)
{
    ASSERT(!crawl_state.game_is_arena());

    int old_amount = you.magic_contamination;
    int old_level  = get_contamination_level();
    bool was_glowing = player_severe_contamination();
    int new_level  = 0;

#if TAG_MAJOR_VERSION == 34
    if (change > 0 && you.unrand_equipped(UNRAND_ETHERIC_CAGE))
        change *= 2;
#endif

    you.magic_contamination = max(0, min(250000,
                                         you.magic_contamination + change));

    new_level = get_contamination_level();

    if (you.magic_contamination != old_amount)
        dprf("change: %d  radiation: %d", change, you.magic_contamination);

    if (new_level > old_level)
    {
        if (msg)
        {
            mprf(player_severe_contamination() ? MSGCH_WARN : MSGCH_PLAIN,
                 "%s", describe_contamination(new_level).c_str());
        }
        if (player_severe_contamination())
            xom_is_stimulated(new_level * 25);
    }
    else if (msg && new_level < old_level)
    {
        if (old_level == 1 && new_level == 0)
            mpr("Your magical contamination has completely faded away.");
        else if (player_severe_contamination() || was_glowing)
        {
            mprf(MSGCH_RECOVERY,
                 "You feel less contaminated with magical energies.");
        }

        if (!player_severe_contamination() && was_glowing && you.invisible())
        {
            mpr("You fade completely from view now that you are no longer "
                "glowing from magical contamination.");
        }
    }

    if (you.magic_contamination > 0)
        learned_something_new(HINT_GLOWING);

    // Zin doesn't like mutations or mutagenic radiation.
    if (you_worship(GOD_ZIN))
    {
        // Whenever the glow status is first reached, give a warning message.
        if (!was_glowing && player_severe_contamination())
            did_god_conduct(DID_CAUSE_GLOWING, 0, false);
        // If the player actively did something to increase glowing,
        // Zin is displeased.
        else if (controlled && change > 0 && was_glowing)
            did_god_conduct(DID_CAUSE_GLOWING, 1 + new_level, true);
    }
}

/**
 * Increase the player's confusion duration.
 *
 * @param amount   The number of turns to increase confusion duration by.
 * @param quiet    Whether to suppress messaging on success/failure.
 * @param force    Whether to ignore resistance (used only for intentional
 *                 self-confusion, e.g. via ambrosia).
 * @return         Whether confusion was successful.
 */
bool confuse_player(int amount, bool quiet, bool force)
{
    ASSERT(!crawl_state.game_is_arena());

    if (amount <= 0)
        return false;

    if (!force && you.clarity())
    {
        if (!quiet)
            mpr("You feel momentarily confused.");
        return false;
    }

    if (!force && you.duration[DUR_DIVINE_STAMINA] > 0)
    {
        if (!quiet)
            mpr("Your divine stamina protects you from confusion!");
        return false;
    }

    const int old_value = you.duration[DUR_CONF];
    you.increase_duration(DUR_CONF, amount, 40);

    if (you.duration[DUR_CONF] > old_value)
    {
        you.check_awaken(500);

        if (!quiet)
        {
            mprf(MSGCH_WARN, "You are %sconfused.",
                 old_value > 0 ? "more " : "");
        }

        learned_something_new(HINT_YOU_ENCHANTED);

        xom_is_stimulated((you.duration[DUR_CONF] - old_value)
                           / BASELINE_DELAY);
    }

    return true;
}

void paralyse_player(string source)
{
    const int cur_para = you.duration[DUR_PARALYSIS] / BASELINE_DELAY;
    const int dur = random_range(2, 5 + cur_para);
    you.paralyse(nullptr, dur, source);
}

bool poison_player(int amount, string source, string source_aux, bool force)
{
    ASSERT(!crawl_state.game_is_arena());

    if (crawl_state.disables[DIS_AFFLICTIONS])
        return false;

    if (you.duration[DUR_DIVINE_STAMINA] > 0)
    {
        mpr("Your divine stamina protects you from poison!");
        return false;
    }

    if (player_res_poison() >= 3)
    {
        dprf("Cannot poison, you are immune!");
        return false;
    }
    else if (!force && player_res_poison() > 0 && !one_chance_in(3))
        return false;

    const int old_value = you.duration[DUR_POISONING];
    const bool was_fatal = poison_is_lethal();

    if (player_res_poison() < 0)
        amount *= 2;

    // TODO: support being poisoned by monsters wearing "harm (ha)
    if (you.extra_harm())
        amount *= (100 + incoming_harm_amount(you.extra_harm())) / 100;

    you.duration[DUR_POISONING] += amount * 1000;

    if (you.duration[DUR_POISONING] > old_value)
    {
        if (poison_is_lethal() && !was_fatal)
            mprf(MSGCH_DANGER, "You are lethally poisoned!");
        else
        {
            mprf(MSGCH_WARN, "You are %spoisoned.",
                old_value > 0 ? "more " : "");
        }

        learned_something_new(HINT_YOU_POISON);
    }

    you.props[POISONER_KEY] = source;
    you.props[POISON_AUX_KEY] = source_aux;

    // Display the poisoned segment of our health, in case we take no damage
    you.redraw_hit_points = true;

    return amount;
}

int get_player_poisoning()
{
    if (player_res_poison() >= 3)
        return 0;
#if TAG_MAJOR_VERSION == 34
    // Approximate the effect of damage shaving by giving the first
    // 25 points of poison damage for 'free'
    if (can_shave_damage())
        return max(0, (you.duration[DUR_POISONING] / 1000) - 25);
#endif
    return you.duration[DUR_POISONING] / 1000;
}

// Fraction of current poison removed every 10 aut.
const double poison_denom = 5.0;

// these values are stored relative to dur's scaling, which is
// poison_points * 1000;
// 0.1 HP/aut
const double poison_min_hp_aut  = 100.0;
// 5.0 HP/aut
const double poison_max_hp_aut  = 5000.0;

// The amount of aut needed for poison to end if
// you.duration[DUR_POISONING] == dur, assuming no Chei/DD shenanigans.
// This function gives the following behaviour:
// * 1/poison_denominator of current poison is removed every 10 aut normally
// * but speed of poison is capped between the two parameters
static double _poison_dur_to_aut(double dur)
{
    const double min_speed_dur = poison_denom * poison_min_hp_aut * 10.0;
    const double decay = log(poison_denom / (poison_denom - 1.0));
    // Poison already at minimum speed.
    if (dur < min_speed_dur)
        return dur / poison_min_hp_aut;
    // Poison is not at maximum speed.
    if (dur < poison_denom * poison_max_hp_aut * 10.0)
        return 10.0 * (poison_denom + log(dur / min_speed_dur) / decay);
    return 10.0 * (poison_denom + log(poison_max_hp_aut / poison_min_hp_aut) / decay)
         + (dur - poison_denom * poison_max_hp_aut * 10.0) / poison_max_hp_aut;
}

// The inverse of the above function, i.e. the amount of poison needed
// to last for aut time.
static double _poison_aut_to_dur(double aut)
{
    // Amount of time that poison lasts at minimum speed.
    if (aut < poison_denom * 10.0)
        return aut * poison_min_hp_aut;
    const double decay = log(poison_denom / (poison_denom - 1.0));
    // Amount of time that poison exactly at the maximum speed lasts.
    const double aut_from_max_speed = 10.0 * (poison_denom
        + log(poison_max_hp_aut / poison_min_hp_aut) / decay);
    if (aut < aut_from_max_speed)
    {
        return 10.0 * poison_denom * poison_min_hp_aut
            * exp(decay / 10.0 * (aut - poison_denom * 10.0));
    }
    return poison_denom * 10.0 * poison_max_hp_aut
         + poison_max_hp_aut * (aut - aut_from_max_speed);
}

void handle_player_poison(int delay)
{
    const double cur_dur = you.duration[DUR_POISONING];
    const double cur_aut = _poison_dur_to_aut(cur_dur);

    // If Cheibriados has slowed your life processes, poison affects you less
    // quickly (you take the same total damage, but spread out over a longer
    // period of time).
    const double delay_scaling = have_passive(passive_t::slow_poison)
                               ? 2.0 / 3.0 : 1.0;

    const double new_aut = cur_aut - ((double) delay) * delay_scaling;
    const double new_dur = _poison_aut_to_dur(new_aut);

    const int decrease = you.duration[DUR_POISONING] - (int) new_dur;

    // Transforming into a form with no metabolism merely suspends the poison
    // but doesn't let your body get rid of it.
    if (you.is_nonliving() || you.is_lifeless_undead())
        return;

    // Other sources of immunity (Zin, staff of Olgreb) let poison dissipate.
    bool do_dmg = (player_res_poison() >= 3 ? false : true);

    int dmg = (you.duration[DUR_POISONING] / 1000)
               - ((you.duration[DUR_POISONING] - decrease) / 1000);

#if TAG_MAJOR_VERSION == 34
    // Approximate old damage shaving by giving immunity to small amounts
    // of poison. Stronger poison will do the same damage as for non-DD
    // until it goes below the threshold, which is a bit weird, but
    // so is damage shaving.
    if (can_shave_damage() && you.duration[DUR_POISONING] - decrease < 25000)
    {
        dmg = (you.duration[DUR_POISONING] / 1000)
            - (25000 / 1000);
        if (dmg < 0)
            dmg = 0;
    }
#endif

    msg_channel_type channel = MSGCH_PLAIN;
    const char *adj = "";

    if (dmg > 6)
    {
        channel = MSGCH_DANGER;
        adj = "extremely ";
    }
    else if (dmg > 2)
    {
        channel = MSGCH_WARN;
        adj = "very ";
    }

    if (do_dmg && dmg > 0)
    {
        int oldhp = you.hp;
        ouch(dmg, KILLED_BY_POISON);
        if (you.hp < oldhp)
            mprf(channel, "You feel %ssick.", adj);
    }

    // Now decrease the poison in our system
    reduce_player_poison(decrease);
}

void reduce_player_poison(int amount)
{
    if (amount <= 0)
        return;

    you.duration[DUR_POISONING] -= amount;

    // Less than 1 point of damage remaining, so just end the poison
    if (you.duration[DUR_POISONING] < 1000)
        you.duration[DUR_POISONING] = 0;

    if (you.duration[DUR_POISONING] <= 0)
    {
        you.duration[DUR_POISONING] = 0;
        you.props.erase(POISONER_KEY);
        you.props.erase(POISON_AUX_KEY);
        mprf(MSGCH_RECOVERY, "You are no longer poisoned.");
    }

    you.redraw_hit_points = true;
}

// Takes *current* regeneration rate into account. Might sometimes be
// incorrect, but hopefully if so then the player is surviving with 1 HP.
bool poison_is_lethal()
{
    if (you.hp <= 0)
        return get_player_poisoning();
    if (get_player_poisoning() < you.hp)
        return false;
    return poison_survival() <= 0;
}

// Try to predict the minimum value of the player's health in the coming
// turns given the current poison amount and regen rate.
int poison_survival()
{
    if (!get_player_poisoning())
        return you.hp;
    const int rr = player_regen();
    const bool chei = have_passive(passive_t::slow_poison);
#if TAG_MAJOR_VERSION == 34
    const bool dd = can_shave_damage();
#endif
    const int amount = you.duration[DUR_POISONING];
    const double full_aut = _poison_dur_to_aut(amount);
    // Calculate the poison amount at which regen starts to beat poison.
    double min_poison_rate = poison_min_hp_aut;
#if TAG_MAJOR_VERSION == 34
    if (dd)
        min_poison_rate = 25.0/poison_denom;
#endif
    if (chei)
        min_poison_rate /= 1.5;
    int regen_beats_poison;
    if (rr <= (int) min_poison_rate)
    {
        regen_beats_poison =
#if TAG_MAJOR_VERSION == 34
         dd ? 25000 :
#endif
              0;
    }
    else
    {
        regen_beats_poison = poison_denom * 10.0 * rr;
        if (chei)
            regen_beats_poison = 3 * regen_beats_poison / 2;
    }

    if (rr == 0)
        return min(you.hp, you.hp - amount / 1000 + regen_beats_poison / 1000);

    // Calculate the amount of time until regen starts to beat poison.
    double poison_duration = full_aut - _poison_dur_to_aut(regen_beats_poison);

    if (poison_duration < 0)
        poison_duration = 0;
    if (chei)
        poison_duration *= 1.5;

    // Worst case scenario is right before natural regen gives you a point of
    // HP, so consider the nearest two such points.
    const int predicted_regen = (int) ((((double) you.hit_points_regeneration) + rr * poison_duration / 10.0) / 100.0);
    double test_aut1 = (100.0 * predicted_regen - 1.0 - ((double) you.hit_points_regeneration)) / (rr / 10.0);
    double test_aut2 = (100.0 * predicted_regen + 99.0 - ((double) you.hit_points_regeneration)) / (rr / 10.0);

    if (chei)
    {
        test_aut1 /= 1.5;
        test_aut2 /= 1.5;
    }

    // Don't do any correction if there's not much poison left
    const int test_amount1 = test_aut1 < full_aut ?
                             _poison_aut_to_dur(full_aut - test_aut1) : 0;
    const int test_amount2 = test_aut2 < full_aut ?
                             _poison_aut_to_dur(full_aut - test_aut2) : 0;

    int prediction1 = you.hp;
    int prediction2 = you.hp;

    // Don't look backwards in time.
    if (test_aut1 > 0.0)
        prediction1 -= (amount / 1000 - test_amount1 / 1000 - (predicted_regen - 1));
    prediction2 -= (amount / 1000 - test_amount2 / 1000 - predicted_regen);

    return min(prediction1, prediction2);
}

bool miasma_player(actor *who, string source_aux)
{
    ASSERT(!crawl_state.game_is_arena());

    if (you.res_miasma() || you.duration[DUR_DEATHS_DOOR])
        return false;

    if (you.duration[DUR_DIVINE_STAMINA] > 0)
    {
        mpr("Your divine stamina protects you from the miasma!");
        return false;
    }

    bool success = poison_player(5 + roll_dice(3, 12),
                                 who ? who->name(DESC_A) : "",
                                 source_aux);

    if (one_chance_in(3))
    {
        slow_player(10 + random2(5));
        success = true;
    }

    return success;
}

bool sticky_flame_player(int intensity, int duration, string source, string source_aux)
{
    ASSERT(!crawl_state.game_is_arena());

    if (player_res_sticky_flame() || duration <= 0 || you.duration[DUR_WATER_HOLD]
        || feat_is_water(env.grid(you.pos())))
    {
        return false;
    }

    const int old_pow = (you.duration[DUR_STICKY_FLAME] > 0
                            ? you.props[STICKY_FLAME_POWER_KEY].get_int()
                            : 0);

    // We use the greater of old power and new power, but always add duration.
    if (intensity > old_pow)
    {
        // Only assign blame if the new sticky flame source is stronger than
        // any previous one we had on ourselves.
        you.props[STICKY_FLAMER_KEY] = source;
        you.props[STICKY_FLAME_AUX_KEY] = source_aux;
        you.props[STICKY_FLAME_POWER_KEY] = intensity;
    }

    const string intensity_str = max(intensity, old_pow) > 5 ? "intense " : "";

    if (you.duration[DUR_STICKY_FLAME] > 0)
    {
        mprf(MSGCH_WARN, "You are even more covered in %sliquid fire!",
                          intensity_str.c_str());
    }
    else
    {
        mprf(MSGCH_WARN, "You are covered in %sliquid fire! Move or burn!",
                         intensity_str.c_str());
    }

    you.increase_duration(DUR_STICKY_FLAME, duration, 35);
    learned_something_new(HINT_ON_FIRE);

    return true;
}

void dec_sticky_flame_player(int delay)
{
    delay = min(delay, you.duration[DUR_STICKY_FLAME]);

    if (feat_is_water(env.grid(you.pos())))
    {
        mprf(MSGCH_RECOVERY, "You dip into the water, and the flames go out!");
        end_sticky_flame_player();
        return;
    }

    int base_damage = roll_dice(2, you.props[STICKY_FLAME_POWER_KEY].get_int());
    int damage = resist_adjust_damage(&you, BEAM_FIRE, base_damage);

    mprf(MSGCH_WARN, "The liquid fire burns you%s!",
         damage > base_damage ? " terribly" : "");

    damage = div_rand_round(damage * delay, BASELINE_DELAY);

    you.expose_to_element(BEAM_STICKY_FLAME, 2);
    maybe_melt_player_enchantments(BEAM_STICKY_FLAME, damage);
    ouch(damage, KILLED_BY_BURNING);

    you.duration[DUR_STICKY_FLAME] =
        max(0, you.duration[DUR_STICKY_FLAME] - delay);

    if (you.duration[DUR_STICKY_FLAME == 0])
    {
        mprf(MSGCH_RECOVERY, "The liquid fire finally exhausts itself.");
        end_sticky_flame_player();
    }
}

// Greatly reduce the remaining duration whenever the player moves.
void shake_off_sticky_flame()
{
    int &dur = you.duration[DUR_STICKY_FLAME];
    if (dur <= 0)
        return;
    // Lose 6 turns of duration for each move
    dur = max(0, dur - 60);

    // End it slightly early if it's ALMOST gone, just to avoid the common
    // situation of losing most of its duration to movement and then it
    // expiring 'normally' immediately afterward, without giving the message
    // that the player helped put it out.
    //
    // (20 aut is picked fairly arbitrarily to include even most slowed actions)
    if (dur <= 20)
    {
        mprf(MSGCH_RECOVERY, "You shake off the liquid fire.");
        end_sticky_flame_player();
    }
    else
        mpr("You shake off some of the fire as you move.");
}

// End the sticky flame status entirely
void end_sticky_flame_player()
{
    you.duration[DUR_STICKY_FLAME] = 0;
    you.props.erase(STICKY_FLAMER_KEY);
    you.props.erase(STICKY_FLAME_AUX_KEY);
    you.props.erase(STICKY_FLAME_POWER_KEY);
}

bool slow_player(int turns)
{
    ASSERT(!crawl_state.game_is_arena());

    if (turns <= 0)
        return false;

    if (you.stasis())
    {
        mpr("Your stasis prevents you from being slowed.");
        return false;
    }

    // Multiplying these values because moving while slowed takes longer than
    // the usual delay.
    turns = haste_mul(turns);
    int threshold = haste_mul(100);

    if (you.duration[DUR_SLOW] >= threshold * BASELINE_DELAY)
        mpr("You already are as slow as you could be.");
    else
    {
        if (you.duration[DUR_SLOW] == 0)
            mprf(MSGCH_WARN, "You feel yourself slow down.");
        else
            mprf(MSGCH_WARN, "You feel as though you will be slow longer.");

        you.increase_duration(DUR_SLOW, turns, threshold);
        learned_something_new(HINT_YOU_ENCHANTED);
    }

    return true;
}

void dec_slow_player(int delay)
{
    if (!you.duration[DUR_SLOW])
        return;

    if (you.duration[DUR_SLOW] > BASELINE_DELAY)
    {
        // Make slowing and hasting effects last as long.
        you.duration[DUR_SLOW] -= you.duration[DUR_HASTE]
            ? haste_mul(delay) : delay;
    }

    if (aura_is_active_on_player(TORPOR_SLOWED_KEY))
    {
        you.duration[DUR_SLOW] = max(1, you.duration[DUR_SLOW]);
        return;
    }

    if (you.duration[DUR_SLOW] <= BASELINE_DELAY)
    {
        you.duration[DUR_SLOW] = 0;
        if (!have_stat_zero())
            mprf(MSGCH_DURATION, "You feel yourself speed up.");
    }
}

void barb_player(int turns, int pow)
{
    ASSERT(!crawl_state.game_is_arena());

    if (turns <= 0 || pow <= 0)
        return;

    const int max_turns = 12;
    const int max_pow = 6;

    if (!you.duration[DUR_BARBS])
    {
        mpr("Barbed spikes become lodged in your body.");
        you.set_duration(DUR_BARBS, min(max_turns, turns));
        you.attribute[ATTR_BARBS_POW] = min(max_pow, pow);
    }
    else
    {
        mpr("More barbed spikes become lodged in your body.");
        you.increase_duration(DUR_BARBS, turns, max_turns);
        you.attribute[ATTR_BARBS_POW] =
            min(max_pow, you.attribute[ATTR_BARBS_POW]++);
    }
}

void crystallize_player()
{
    if (x_chance_in_y(3, 4))
    {
        if (!you.duration[DUR_VITRIFIED])
            mpr("Your body becomes as fragile as glass!");
        else
            mpr("You feel your fragility will last longer.");
        you.increase_duration(DUR_VITRIFIED, random_range(8, 18), 50);
    }
}

/**
 * Players are rather more susceptible to dazzling: only those who can't
 * be blinded are immune.
 */
bool player::can_be_dazzled() const
{
    return can_be_blinded();
}

/**
 * Players can be blinded only if they're not undead.
 */
bool player::can_be_blinded() const
{
    return !is_lifeless_undead();
}

/**
 * Increase the player's blindness duration.
 *
 * @param amount   The number of turns to increase blindness duration by.
 */
void blind_player(int amount, colour_t flavour_colour)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!you.can_be_dazzled())
    {
        mpr("Your vision flashes for a moment.");
        return;
    }

    if (amount <= 0)
        return;

    const int current = you.duration[DUR_BLIND];
    you.increase_duration(DUR_BLIND, amount, 50);

    if (you.duration[DUR_BLIND] > current)
    {
        you.props[BLIND_COLOUR_KEY] = flavour_colour;
        if (current > 0)
            mpr("You are blinded for an even longer time.");
        else
            mpr("You are blinded!");
        learned_something_new(HINT_YOU_ENCHANTED);
        xom_is_stimulated((you.duration[DUR_BLIND] - current) / BASELINE_DELAY);
    }
}

// Returns the percentage chance any attack or dodgeable beam will miss at a
// given distance. (ie: 30%, 45%, 60%, 75%, 90%)
int player_blind_miss_chance(int distance)
{
    if (you.duration[DUR_BLIND])
        return min(90, 15 + (distance * 15));
    else
        return 0;
}

void dec_berserk_recovery_player(int delay)
{
    if (!you.duration[DUR_BERSERK_COOLDOWN])
        return;

    if (you.duration[DUR_BERSERK_COOLDOWN] > BASELINE_DELAY)
    {
        you.duration[DUR_BERSERK_COOLDOWN] -= you.duration[DUR_HASTE]
                                              ? haste_mul(delay) : delay;
    }

    if (you.duration[DUR_BERSERK_COOLDOWN] <= BASELINE_DELAY)
    {
        mprf(MSGCH_DURATION, "You recover from your berserk rage.");
        you.duration[DUR_BERSERK_COOLDOWN] = 0;
    }
}

bool haste_player(int turns, bool rageext)
{
    ASSERT(!crawl_state.game_is_arena());

    if (turns <= 0)
        return false;

    if (you.stasis())
    {
        mpr("Your stasis prevents you from being hasted.");
        return false;
    }
    else if (have_passive(passive_t::no_haste))
    {
        simple_god_message(" protects you from inadvertent hurry.");
        return false;
    }

    // Cutting the nominal turns in half since hasted actions take half the
    // usual delay.
    turns = haste_div(turns);
    const int threshold = 40;

    if (!you.duration[DUR_HASTE])
        mpr("You feel yourself speed up.");
    else if (you.duration[DUR_HASTE] > threshold * BASELINE_DELAY)
        mpr("You already have as much speed as you can handle.");
    else if (!rageext)
        mpr("You feel as though your hastened speed will last longer.");

    you.increase_duration(DUR_HASTE, turns, threshold);

    return true;
}

void dec_haste_player(int delay)
{
    if (!you.duration[DUR_HASTE])
        return;

    if (you.duration[DUR_HASTE] > BASELINE_DELAY)
    {
        int old_dur = you.duration[DUR_HASTE];

        you.duration[DUR_HASTE] -= delay;

        int threshold = 6 * BASELINE_DELAY;
        // message if we cross the threshold
        if (old_dur > threshold && you.duration[DUR_HASTE] <= threshold)
        {
            mprf(MSGCH_DURATION, "Your extra speed is starting to run out.");
            if (coinflip())
                you.duration[DUR_HASTE] -= BASELINE_DELAY;
        }
    }
    else if (you.duration[DUR_HASTE] <= BASELINE_DELAY)
    {
        if (!you.duration[DUR_BERSERK])
            mprf(MSGCH_DURATION, "You feel yourself slow down.");
        you.duration[DUR_HASTE] = 0;
    }
}

void dec_elixir_player(int delay)
{
    if (!you.duration[DUR_ELIXIR])
        return;

    you.duration[DUR_ELIXIR] -= delay;
    if (you.duration[DUR_ELIXIR] < 0)
        you.duration[DUR_ELIXIR] = 0;

    const int hp = (delay * you.hp_max / 10) / BASELINE_DELAY;
    if (!you.duration[DUR_DEATHS_DOOR])
        inc_hp(hp);

    const int mp = (delay * you.max_magic_points / 10) / BASELINE_DELAY;
    inc_mp(mp);
}

void dec_ambrosia_player(int delay)
{
    if (!you.duration[DUR_AMBROSIA])
        return;

    // ambrosia ends when confusion does.
    if (!you.confused())
        you.duration[DUR_AMBROSIA] = 0;

    you.duration[DUR_AMBROSIA] = max(0, you.duration[DUR_AMBROSIA] - delay);

    // 3-5 per turn, 9-50 over (3-10) turns
    const int hp_restoration = div_rand_round(delay*(3 + random2(3)), BASELINE_DELAY);
    const int mp_restoration = div_rand_round(delay*(3 + random2(3)), BASELINE_DELAY);

    if (!you.duration[DUR_DEATHS_DOOR])
    {
        int heal = you.scale_potion_healing(hp_restoration);
        inc_hp(heal);
    }

    inc_mp(you.scale_potion_mp_healing(mp_restoration));

    if (!you.duration[DUR_AMBROSIA])
        mpr("You feel less invigorated.");
}

void dec_channel_player(int delay)
{
    if (!you.duration[DUR_CHANNEL_ENERGY])
        return;

    you.duration[DUR_CHANNEL_ENERGY] =
        max(0, you.duration[DUR_CHANNEL_ENERGY] - delay);

    // 3-5 per turn, 9-50 over (3-10) turns
    const int mp_restoration = div_rand_round(delay*(3 + random2(3)),
                                              BASELINE_DELAY);
    inc_mp(mp_restoration);

    if (!you.duration[DUR_CHANNEL_ENERGY])
        mpr("You feel less invigorated.");
}

void dec_frozen_ramparts(int delay)
{
    if (!you.duration[DUR_FROZEN_RAMPARTS])
        return;

    you.duration[DUR_FROZEN_RAMPARTS] =
        max(0, you.duration[DUR_FROZEN_RAMPARTS] - delay);

    if (!you.duration[DUR_FROZEN_RAMPARTS])
    {
        end_frozen_ramparts();
        mpr("The frozen ramparts melt away.");
    }
}

void reset_rampage_heal_duration()
{
    const int heal_dur = random_range(3, 6);
    you.set_duration(DUR_RAMPAGE_HEAL, heal_dur);
}

void apply_rampage_heal()
{
    if (!you.has_mutation(MUT_ROLLPAGE))
        return;

    reset_rampage_heal_duration();

    const int heal = you.props[RAMPAGE_HEAL_KEY].get_int();
    if (heal < RAMPAGE_HEAL_MAX)
        you.props[RAMPAGE_HEAL_KEY] = heal + 1;
}

bool invis_allowed(bool quiet, string *fail_reason, bool temp)
{
    string msg;
    bool success = true;

    if (you.backlit(true, false))
    {
        msg = "Your body glows too brightly to become invisible.";
        success = false;
    }
    else if (you.haloed() && you.halo_radius() != -1)
    {
        vector<string> sources;

        if (temp && you.unrand_equipped(UNRAND_EOS))
            sources.push_back("weapon");

        if (temp && you.unrand_equipped(UNRAND_VAINGLORY))
            sources.push_back("crown");

        if (temp && you.wearing_ego(OBJ_ARMOUR, SPARM_LIGHT))
            sources.push_back("orb");

        if (temp && you.props.exists(WU_JIAN_HEAVENLY_STORM_KEY)
            || you.religion == GOD_SHINING_ONE) // non-temp
        {
            sources.push_back("divine halo");
        }

        if (sources.empty())
            success = true;
        else
        {
            msg = "Your " + comma_separated_line(sources.begin(), sources.end())
                  + " glow" + (sources.size() == 1 ? "s" : "")
                  + " too brightly for you to become invisible.";
            success = false;
        }
    }
    else if (you.backlit(true, temp))
    {
        msg = "You are backlit; invisibility will do you no good right now";
        if (quiet)
            success = false;
        else if (!quiet && !yesno((msg + "; use anyway?").c_str(), false, 'n'))
        {
            // XX this shouldn't be here. Currently used only for evoke invis.
            canned_msg(MSG_OK);
            success = false;
            quiet = true; // since we just said something
        }
        msg += ".";
    }

    if (!success)
    {
        if (fail_reason)
            *fail_reason = msg;
        if (!quiet)
            mpr(msg);
    }
    return success;
}

bool flight_allowed(bool quiet, string *fail_reason)
{
    string msg;
    bool success = true;

    if (get_form()->forbids_flight())
    {
        msg = you.form == transformation::tree ? "Your roots keep you in place."
            : "You can't fly in this form.";
        success = false;
    }

    if (!success)
    {
        if (fail_reason)
            *fail_reason = msg;
        if (!quiet)
            mpr(msg);
    }
    return success;
}

void float_player()
{
    if (you.fishtail)
    {
        mpr("Your tail turns into legs as you fly out of the water.");
        merfolk_stop_swimming();
    }
    else if (you.has_mutation(MUT_TENGU_FLIGHT))
        mpr("You swoop lightly up into the air.");
    else
        mpr("You fly up into the air.");
}

void fly_player(int pow, bool already_flying)
{
    if (!flight_allowed())
        return;

    bool standing = !you.airborne() && !already_flying;
    if (!already_flying)
        mprf(MSGCH_DURATION, "You feel %s buoyant.", standing ? "very" : "more");

    you.increase_duration(DUR_FLIGHT, 25 + random2(pow), 100);

    if (standing)
        float_player();
}

void enable_emergency_flight()
{
    mprf("You can't survive in this terrain! You fly above the %s, but the "
         "process is draining.",
         (env.grid(you.pos()) == DNGN_LAVA)       ? "lava" :
         (env.grid(you.pos()) == DNGN_DEEP_WATER) ? "water"
                                             : "buggy terrain");

    you.props[EMERGENCY_FLIGHT_KEY] = true;
}

/**
 * Handle the player's flight ending. Apply emergency flight if needed.
 *
 * @param quiet         Should we notify the player flight is ending?
 * @return              If flight was ended.
 */
bool land_player(bool quiet)
{
    // re-update the equipment cache: any sources of flight from equipment?
    you.attribute[ATTR_PERM_FLIGHT] = you.equip_flight() ? 1 : 0;

    // there was another source keeping you aloft
    if (you.airborne())
        return false;

    // Handle landing on (formerly) instakill terrain
    if (is_feat_dangerous(env.grid(you.pos())))
    {
        fall_into_a_pool(env.grid(you.pos()));
        return false;
    }

    if (!quiet)
        mpr("You float gracefully downwards.");

    // Re-enter the terrain.
    move_player_to_grid(you.pos(), false);
    return true;
}

static void _end_water_hold()
{
    you.duration[DUR_WATER_HOLD] = 0;
    you.props.erase(WATER_HOLDER_KEY);
    you.props.erase(WATER_HOLD_SUBSTANCE_KEY);
}

static string _water_hold_substance()
{
    return you.props[WATER_HOLD_SUBSTANCE_KEY].get_string();
}

bool player::clear_far_engulf(bool force, bool moved)
{
    if (!you.duration[DUR_WATER_HOLD])
        return false;

    monster* const mons = monster_by_mid(you.props[WATER_HOLDER_KEY].get_int());
    if (force || !mons || !mons->alive() || !adjacent(mons->pos(), you.pos()))
    {
        if (moved)
        {
            mprf("You slip free of the %s engulfing you.",
                 _water_hold_substance().c_str());
        }
        else
        {
            mprf("The %s engulfing you falls away.",
                 _water_hold_substance().c_str());
        }

        if (!you.res_water_drowning())
            mpr("You gasp with relief as air once again reaches your lungs.");

        _end_water_hold();
        return true;
    }
    return false;
}

void handle_player_drowning(int delay)
{
    if (you.clear_far_engulf())
        return;
    if (you.res_water_drowning())
    {
        // Reset so damage doesn't ramp up while able to breathe
        you.duration[DUR_WATER_HOLD] = 10;
    }
    else
    {
        you.duration[DUR_WATER_HOLD] += delay;
        int dam =
            div_rand_round((28 + stepdown((float)you.duration[DUR_WATER_HOLD], 28.0))
                            * delay,
                            BASELINE_DELAY * 10);
        ouch(dam, KILLED_BY_WATER, you.props[WATER_HOLDER_KEY].get_int());
        mprf(MSGCH_WARN, "Your lungs strain for air!");
    }
}

player::player()
{
    // warning: this constructor is called for `you` in an indeterminate order
    // with respect to other globals, and so anything that depends on a global
    // you should not do here. This includes things like `branches`, as well as
    // any const static string prop name -- any object that needs to call a
    // constructor is risky, and may or may not have called it yet. E.g. strings
    // could be empty, branches could have every branch set as the dungeon, etc.
    // One candidate location is startup.cc:_initialize, which is nearly the
    // first things called in the launch game loop.

    chr_god_name.clear();
    chr_species_name.clear();
    chr_class_name.clear();
    // Permanent data:
    your_name.clear();
    species          = SP_UNKNOWN;
    char_class       = JOB_UNKNOWN;
    type             = MONS_PLAYER;
    mid              = MID_PLAYER;
    position.reset();

#ifdef WIZARD
    wizard = Options.wiz_mode == WIZ_YES;
    explore = Options.explore_mode == WIZ_YES;
#else
    wizard = false;
    explore = false;
#endif
    suppress_wizard = false;

    birth_time       = time(0);

    // Long-term state:
    elapsed_time     = 0;
    elapsed_time_at_last_input = 0;

    hp               = 0;
    hp_max           = 0;
    hp_max_adj_temp  = 0;
    hp_max_adj_perm  = 0;

    magic_points     = 0;
    max_magic_points = 0;
    mp_max_adj       = 0;

    base_stats.init(0);

    max_level       = 1;
    hit_points_regeneration   = 0;
    magic_points_regeneration = 0;
    experience       = 0;
    total_experience = 0;
    experience_level = 1;
    experience_pool  = 0;
    gold             = 0;
    zigs_completed   = 0;
    zig_max          = 0;

    last_unequip = -1;

    symbol          = MONS_PLAYER;
    form            = transformation::none;
    default_form    = transformation::none;
    active_talisman.clear();

    for (auto &item : inv)
        item.clear();
    runes.reset();
    obtainable_runes = 15;

    gems_found.reset();
    gems_shattered.reset();
    for (int &t : gem_time_spent)
        t = 0;

    spell_library.reset();
    spells.init(SPELL_NO_SPELL);
    old_vehumet_gifts.clear();
    spell_no        = 0;
    vehumet_gifts.clear();
    chapter  = CHAPTER_ORB_HUNTING;
    royal_jelly_dead = false;
    transform_uncancellable = false;
    fishtail = false;
    vampire_alive = true;

    pet_target      = MHITNOT;

    duration.init(0);
    apply_berserk_penalty = false;
    berserk_penalty = 0;
    attribute.init(0);

    last_timer_effect.init(0);
    next_timer_effect.init(20 * BASELINE_DELAY);

    pending_revival = false;
    lives = 0;
    deaths = 0;

    wizard_vision = false;

    init_skills();

    skill_menu_do = SKM_NONE;
    skill_menu_view = SKM_NONE;

    skill_cost_level = 1;
    exp_available = 0;

    item_description.init(255);
    unique_items.init(UNIQ_NOT_EXISTS);
    unique_creatures.reset();
    force_autopickup.init(0);

    kills = KillMaster();

    where_are_you    = BRANCH_DUNGEON;
    depth            = 1;

    religion         = GOD_NO_GOD;
    jiyva_second_name.clear();
    piety            = 0;
    piety_hysteresis = 0;
    gift_timeout     = 0;
    saved_good_god_piety = 0;
    previous_good_god = GOD_NO_GOD;
    penance.init(0);
    worshipped.init(0);
    num_current_gifts.init(0);
    num_total_gifts.init(0);
    one_time_ability_used.reset();
    piety_max.init(0);
    exp_docked.init(0);
    exp_docked_total.init(0);

    mutation.init(0);
    innate_mutation.init(0);
    temp_mutation.init(0);
    demonic_traits.clear();
    sacrifices.init(0);
    sacrifice_piety.init(0);

    magic_contamination = 0;

    seen_weapon.init(0);
    seen_armour.init(0);
    seen_misc.reset();
    seen_talisman.reset();

    octopus_king_rings = 0x00;

    normal_vision    = LOS_DEFAULT_RANGE;
    current_vision   = LOS_DEFAULT_RANGE;

    rampage_hints.clear();

    real_time_ms     = chrono::milliseconds::zero();
    real_time_delta  = chrono::milliseconds::zero();
    num_turns        = 0;
    exploration      = 0;

    trapped            = false;
    triggered_spectral = false;

    last_view_update = 0;

    spell_letter_table.init(-1);
    ability_letter_table.init(ABIL_NON_ABILITY);

    uniq_map_tags.clear();
    uniq_map_names.clear();
    uniq_map_tags_abyss.clear();
    uniq_map_names_abyss.clear();
    vault_list.clear();

    global_info = PlaceInfo();
    global_info.assert_validity();

    m_quiver_history = quiver::ammo_history();
    quiver_action = quiver::action_cycler();

    props.clear();

    beholders.clear();
    fearmongers.clear();
    dactions.clear();
    level_stack.clear();
    type_ids.init(false);

    banished_by.clear();
    banished_power = 0;

    last_mid = 0;
    last_cast_spell = SPELL_NO_SPELL;

    // Non-saved UI state:
    prev_targ        = MID_NOBODY;
    prev_grd_targ.reset();
    divine_exegesis  = false;

    travel_x         = 0;
    travel_y         = 0;
    travel_z         = level_id();

    running.clear();
    received_weapon_warning = false;
    received_noskill_warning = false;
    wizmode_teleported_into_rock = false;
    skill_boost.clear();
    digging = false;

    delay_queue.clear();

    last_keypress_time = chrono::system_clock::now();

    action_count.clear();

    branches_left.reset();

    // Volatile (same-turn) state:
    turn_is_over     = false;
    banished         = false;

    wield_change         = false;
    gear_change          = false;
    redraw_noise         = false;
    redraw_quiver        = false;
    redraw_status_lights = false;
    redraw_hit_points    = false;
    redraw_magic_points  = false;
    redraw_stats.init(false);
    redraw_experience    = false;
    redraw_armour_class  = false;
    redraw_evasion       = false;
    redraw_title         = false;

    flash_colour        = BLACK;
    flash_where         = nullptr;

    time_taken          = 0;
    shield_blocks       = 0;

    abyss_speed         = 0;
    game_seed           = 0;
    fully_seeded        = true;
    deterministic_levelgen = true;

    los_noise_level     = 0;        ///< temporary slot for loud noise levels
    los_noise_last_turn = 0;
    ///< loudest noise heard on the last turn, for HUD display

    transit_stair       = DNGN_UNSEEN;
    entering_level      = false;

    reset_escaped_death();
    on_current_level    = true;
    seen_portals        = 0;
    frame_no            = 0;

    save                = nullptr;
    prev_save_version.clear();

    clear_constricted();
    constricting = 0;

    // Protected fields:
    clear_place_info();
}

void player::init_skills()
{
    auto_training = !(Options.default_manual_training);
    skills.init(0);
    train.init(TRAINING_DISABLED);
    train_alt.init(TRAINING_DISABLED);
    training.init(0);
    can_currently_train.reset();
    skill_points.init(0);
    skill_order.init(MAX_SKILL_ORDER);
    skill_manual_points.init(0);
    training_targets.init(0);
    exercises.clear();
    exercises_all.clear();
}

player_save_info& player_save_info::operator=(const player& rhs)
{
    // TODO: maybe seed, version?
    name             = rhs.your_name;
    prev_save_version = rhs.prev_save_version;
    species          = rhs.species;
    job              = rhs.char_class;
    experience_level = rhs.experience_level;
    class_name       = rhs.chr_class_name;
    religion         = rhs.religion;
    jiyva_second_name = rhs.jiyva_second_name;
    wizard           = rhs.wizard || rhs.suppress_wizard;
    species_name     = rhs.chr_species_name;
    god_name         = rhs.chr_god_name;
    explore          = rhs.explore;

    // doll data used only for startup menu, ignore?

    // [ds] Perhaps we should move game type to player?
    saved_game_type  = crawl_state.type;
    map = crawl_state.map;

    return *this;
}

void player::init_from_save_info(const player_save_info &s)
{
    your_name         = s.name;
    prev_save_version = s.prev_save_version;
    species           = s.species;
    char_class        = s.job;
    experience_level  = s.experience_level;
    chr_class_name    = s.class_name;
    religion          = s.religion;
    jiyva_second_name = s.jiyva_second_name;
    wizard            = s.wizard;
    chr_species_name  = s.species_name;
    chr_god_name      = s.god_name;
    explore           = s.explore;
    // do not copy doll data?

    // side effect alert!
    crawl_state.type = s.saved_game_type;
    crawl_state.map = s.map;
}

bool player_save_info::operator<(const player_save_info& rhs) const
{
    return experience_level > rhs.experience_level
           || (experience_level == rhs.experience_level && name < rhs.name);
}

string player_save_info::really_short_desc() const
{
    ostringstream desc;
    desc << name << " the " << species_name << ' ' << class_name;

    return desc.str();
}

string player_save_info::short_desc(bool use_qualifier) const
{
    ostringstream desc;

    const string qualifier = use_qualifier
                    ? game_state::game_type_name_for(saved_game_type)
                    : "";
    if (!qualifier.empty())
        desc << "[" << qualifier << "] ";

    desc << name << ", a level " << experience_level << ' '
         << species_name << ' ' << class_name;

    if (religion == GOD_JIYVA)
        desc << " of " << god_name << " " << jiyva_second_name;
    else if (religion != GOD_NO_GOD)
        desc << " of " << god_name;

#ifdef WIZARD
    if (wizard)
        desc << " (WIZ)";
#endif

    return desc.str();
}

player::~player()
{
    if (CrawlIsCrashing && save)
    {
        save->abort();
        delete save;
        save = nullptr;
    }
    ASSERT(!save); // the save file should be closed or deleted
}

bool player::airborne() const
{
    if (get_form()->forbids_flight())
        return false;

    return you.duration[DUR_FLIGHT]   // potions, polar vortex
        || you.props[EMERGENCY_FLIGHT_KEY].get_bool()
        || you.duration[DUR_RISING_FLAME] // flavour
        || permanent_flight(true)
        || get_form()->enables_flight();
}

bool player::rampaging() const
{
    return you.unrand_equipped(UNRAND_SEVEN_LEAGUE_BOOTS)
            || you.has_mutation(MUT_ROLLPAGE)
            || actor::rampaging();
}

bool player::is_banished() const
{
    return banished;
}

bool player::is_sufficiently_rested(bool starting) const
{
    // Only return false if resting will actually help. Anything here should
    // explicitly trigger an appropriate activity interrupt to prevent infinite
    // resting (and shouldn't just rely on a message interrupt).
    // if an interrupt is disabled, we don't count it at all for resting. (So
    // if someone disables all these interrupts, resting becomes impossible.)
    const bool hp_interrupts = Options.activity_interrupts["rest"][
                                static_cast<int>(activity_interrupt::full_hp)];
    const bool mp_interrupts = Options.activity_interrupts["rest"][
                                static_cast<int>(activity_interrupt::full_mp)];
    const bool can_freely_move = you.is_motile() && !you.duration[DUR_BARBS];

    return (!player_regenerates_hp()
                || _should_stop_resting(hp, hp_max, !starting)
                || !hp_interrupts)
        && (!player_regenerates_mp()
                || _should_stop_resting(magic_points, max_magic_points, !starting)
                || !mp_interrupts)
        && (can_freely_move || !hp_interrupts);
}

bool player::in_water() const
{
    return ground_level() && !you.can_water_walk() && feat_is_water(env.grid(pos()));
}

bool player::in_liquid() const
{
    return in_water() || liquefied_ground();
}

bool player::can_swim(bool permanently) const
{
    return cur_form(!permanently)->player_can_swim();
}

/// Can the player do a passing imitation of a notorious Palestinian?
bool player::can_water_walk() const
{
    return have_passive(passive_t::water_walk)
           || you.props.exists(TEMP_WATERWALK_KEY);
}

int player::visible_igrd(const coord_def &where) const
{
    if (feat_eliminates_items(env.grid(where)))
        return NON_ITEM;

    return env.igrid(where);
}

bool player::has_spell(spell_type spell) const
{
    return find(begin(spells), end(spells), spell) != end(spells);
}

bool player::cannot_speak() const
{
    if (silenced(pos()))
        return true;

    if (paralysed() || petrified()) // we allow talking during sleep ;)
        return true;

    // No transform that prevents the player from speaking yet.
    // ... yet setting this would prevent saccing junk and similar activities
    // for no good reason.
    return false;
}

/**
 * What verb should be used to describe the player's shouting?
 *
 * @param directed      Whether you're shouting at anyone in particular.
 * @return              A shouty kind of verb.
 */
string player::shout_verb(bool directed) const
{
    if (!get_form()->shout_verb.empty())
        return get_form()->shout_verb;

    const int screaminess = get_mutation_level(MUT_SCREAM);
    return species::shout_verb(you.species, screaminess, directed);
}

/**
 * How loud are the player's shouts?
 *
 * @return The noise produced by a single player shout.
 */
int player::shout_volume() const
{
    const int base_noise = 12 + get_form()->shout_volume_modifier;

    return base_noise + 2 * (get_mutation_level(MUT_SCREAM));
}

void player::god_conduct(conduct_type thing_done, int level)
{
    ::did_god_conduct(thing_done, level);
}

void player::banish(const actor* /*agent*/, const string &who, const int power,
                    bool force)
{
    ASSERT(!crawl_state.game_is_arena());
    if (brdepth[BRANCH_ABYSS] == -1)
        return;

    if (player_in_branch(BRANCH_ARENA))
    {
        simple_god_message(" prevents your banishment from the Arena!",
                           false, GOD_OKAWARU);
        return;
    }

    if (you.duration[DUR_BEOGH_DIVINE_CHALLENGE])
    {
        simple_god_message(" refuses to let the Abyss claim you during a challenge!",
                           false, GOD_BEOGH);

        return;
    }

    if (elapsed_time <= attribute[ATTR_BANISHMENT_IMMUNITY])
    {
        mpr("You resist the pull of the Abyss.");
        return;
    }

    if (!force && player_in_branch(BRANCH_ABYSS)
        && x_chance_in_y(you.depth, brdepth[BRANCH_ABYSS]))
    {
        mpr("You wobble for a moment.");
        return;
    }

    banished    = true;
    banished_by = who;
    banished_power = power;
}

/*
 * Approximate the loudest noise the player heard in the last
 * turn, possibly rescaling. This gets updated every
 * `world_reacts`. If `adjusted` is set to true, this rescales
 * noise on a 0-1000 scale according to some breakpoints that
 * I have hand-calibrated. Otherwise, it returns the raw noise
 * value (approximately from 0 to 40). The breakpoints aim to
 * approximate 1x los radius, 2x los radius, and 3x los radius
 * relative to an open area.
 *
 * @param adjusted      Whether to rescale the noise level.
 *
 * @return The (scaled or unscaled) noise level heard by the player.
 */
int player::get_noise_perception(bool adjusted) const
{
    // los_noise_last_turn is already normalized for the branch's ambient
    // noise.
    const int level = los_noise_last_turn;
    static const int BAR_MAX = 1000; // TODO: export to output.cc & webtiles
    if (!adjusted)
         return (level + 500) / BAR_MAX;

    static const vector<int> NOISE_BREAKPOINTS = { 0, 6000, 13000, 29000 };
    const int BAR_FRAC = BAR_MAX / (NOISE_BREAKPOINTS.size() - 1);
    for (size_t i = 1; i < NOISE_BREAKPOINTS.size(); ++i)
    {
        const int breakpoint = NOISE_BREAKPOINTS[i];
        if (level > breakpoint)
            continue;
        // what fragment of this breakpoint does the noise fill up?
        const int prev_break = NOISE_BREAKPOINTS[i-1];
        const int break_width = breakpoint - prev_break;
        const int in_segment = (level - prev_break) * BAR_FRAC / break_width;
        // that fragment + previous breakpoints passed is our total noise.
        return in_segment + (i - 1) * BAR_FRAC;
        // example: 10k noise. that's 4k past the 6k breakpoint
        // ((10k-6k) * 333 / (13k - 6k)) + 333, or a bit more than half the bar
    }

    return BAR_MAX;
}

bool player::paralysed() const
{
    return duration[DUR_PARALYSIS];
}

bool player::cannot_act() const
{
    return asleep() || paralysed() || petrified();
}

bool player::confused() const
{
    return duration[DUR_CONF];
}

bool player::caught() const
{
    return attribute[ATTR_HELD];
}

bool player::petrifying() const
{
    return duration[DUR_PETRIFYING];
}

bool player::petrified() const
{
    return duration[DUR_PETRIFIED];
}

bool player::liquefied_ground() const
{
    return liquefied(pos())
           && you.species != SP_GREY_DRACONIAN
           && ground_level() && !is_insubstantial();
}

/**
 * Returns whether the player currently has any kind of shield.
 *
 * XXX: why does this function exist?
 */
bool player::shielded() const
{
    return shield()
           || duration[DUR_DIVINE_SHIELD]
           || duration[DUR_EPHEMERAL_SHIELD]
           || get_mutation_level(MUT_LARGE_BONE_PLATES) > 0
           || qazlal_sh_boost() > 0
           || you.wearing_jewellery(AMU_REFLECTION)
           || you.scan_artefacts(ARTP_SHIELDING)
           || (get_mutation_level(MUT_CONDENSATION_SHIELD)
                && !you.duration[DUR_ICEMAIL_DEPLETED]);
}

int player::shield_bonus() const
{
    const int shield_class = player_shield_class();
    if (shield_class <= 0)
        return -100;

    return random2avg(shield_class * 2, 2) / 3 - 1;
}

int player::shield_bypass_ability(int tohit) const
{
    return 15 + tohit / 2;
}

void player::shield_block_succeeded(actor *attacker)
{
    actor::shield_block_succeeded(attacker);

    if (!you.duration[DUR_DIVINE_SHIELD])
        shield_blocks++;

    practise_shield_block();
    if (is_shield(shield()))
        count_action(CACT_BLOCK, shield()->sub_type);
    else
        count_action(CACT_BLOCK, -1, BLOCK_OTHER); // non-shield block
}

bool player::missile_repulsion() const
{
    return get_mutation_level(MUT_DISTORTION_FIELD) == 3
        || you.wearing_ego(OBJ_ARMOUR, SPARM_REPULSION)
        || scan_artefacts(ARTP_RMSL)
        || have_passive(passive_t::upgraded_storm_shield);
}

/**
 * What's the base value of the penalties the player receives from their
 * body armour?
 *
 * Used as the base for adjusted armour penalty calculations, as well as for
 * stealth penalty calculations.
 *
 * @return  The player's body armour's PARM_EVASION, if any, taking into account
 *          the sturdy frame mutation that reduces encumbrance.
 */
int player::unadjusted_body_armour_penalty() const
{
    const item_def *body_armour = you.body_armour();
    if (!body_armour)
        return 0;

    // PARM_EVASION is always less than or equal to 0
    return max(0, -property(*body_armour, PARM_EVASION) / 10
                  - get_mutation_level(MUT_STURDY_FRAME) * 2);
}

/**
 * The encumbrance penalty to the player for their worn body armour.
 *
 * @param scale     A scale to multiply the result by, to avoid precision loss.
 * @return          A penalty to EV based quadratically on body armour
 *                  encumbrance.
 */
int player::adjusted_body_armour_penalty(int scale) const
{
    const int base_ev_penalty = unadjusted_body_armour_penalty();

    // New formula for effect of str on aevp: (2/5) * evp^2 / (str+3)
    return 2 * base_ev_penalty * base_ev_penalty * (450 - skill(SK_ARMOUR, 10))
           * scale / (5 * (strength() + 3)) / 450;
}

/**
 * The encumbrance penalty to the player for their worn shield.
 *
 * @param scale     A scale to multiply the result by, to avoid precision loss.
 * @return          A penalty to EV based on shield weight.
 */
int player::adjusted_shield_penalty(int scale) const
{
    const item_def *shield_l = shield();
    if (!shield_l)
        return 0;

    const int base_shield_penalty = -property(*shield_l, PARM_EVASION) / 10;
    return 2 * base_shield_penalty * base_shield_penalty
           * (270 - skill(SK_SHIELDS, 10)) * scale
           / (25 + 5 * strength()) / 270;
}

/**
 * Get the player's skill level for sk.
 *
 * @param scale a scale factor to multiply by.
 * @param real whether to return the real value, or modified value.
 * @param temp whether to include modification by other temporary factors (e.g. heroism)
 */
int player::skill(skill_type sk, int scale, bool real, bool temp) const
{
    // If you add another enhancement/reduction, be sure to change
    // SkillMenuSwitch::get_help() to reflect that

    // wizard racechange, or upgraded old save
    if (is_useless_skill(sk))
        return 0;

    // skills[sk] might not be updated yet if this is in the middle of
    // skill training, so make sure to use the correct value.
    int actual_skill = skills[sk];
    unsigned int effective_points = skill_points[sk];
    if (!real)
        effective_points += get_crosstrain_points(sk);
    effective_points = min(effective_points, skill_exp_needed(MAX_SKILL_LEVEL, sk));
    actual_skill = calc_skill_level_change(sk, actual_skill, effective_points);

    int level = actual_skill * scale
      + get_skill_progress(sk, actual_skill, effective_points, scale);
    if (real)
        return level;

    if (you.unrand_equipped(UNRAND_HERMITS_PENDANT))
    {
        if (sk == SK_INVOCATIONS)
            return 14 * scale;
        if (sk == SK_EVOCATIONS)
            return 0;
    }

    if (penance[GOD_ASHENZARI])
    {
        if (temp)
            level = max(level - 4 * scale, level / 2);
    }
    else if (ash_has_skill_boost(sk))
            level = ash_skill_boost(sk, scale);

    if (you.unrand_equipped(UNRAND_CHARLATANS_ORB) && sk != SK_EVOCATIONS)
        level += skill(SK_EVOCATIONS, 10, true, false) * scale / 50;

    if (temp && duration[DUR_HEROISM] && sk <= SK_LAST_MUNDANE)
        level += 5 * scale;

    if (level > MAX_SKILL_LEVEL * scale)
        level = MAX_SKILL_LEVEL * scale;

    return level;
}

int player_icemail_armour_class()
{
    if (!you.has_mutation(MUT_ICEMAIL))
        return 0;

    return you.duration[DUR_ICEMAIL_DEPLETED] ? 0
            : you.get_mutation_level(MUT_ICEMAIL) * ICEMAIL_MAX / 2;
}

int player_condensation_shield_class()
{
    if (!you.has_mutation(MUT_CONDENSATION_SHIELD))
        return 0;

    return you.duration[DUR_ICEMAIL_DEPLETED] ? 0 : ICEMAIL_MAX / 2;
}

/**
 * How many points of AC does the player get from their sanguine armour, if
 * they have any?
 *
 * @return      The AC bonus * 100. (For scaling.)
 */
int sanguine_armour_bonus()
{
    if (!you.duration[DUR_SANGUINE_ARMOUR])
        return 0;

    const int mut_lev = you.get_mutation_level(MUT_SANGUINE_ARMOUR);
    // like iridescent, but somewhat moreso (when active)
    return 300 + mut_lev * 300;
}

/**
 * How much AC does the player get from an unenchanted version of the given
 * armour?
 *
 * @param armour    The armour in question.
 * @param scale     A value to multiply the result by. (Used to avoid integer
 *                  rounding.)
 * @return          The AC from that armour, including armour skill, mutations
 *                  & divine blessings, but not enchantments or egos.
 */
int player::base_ac_from(const item_def &armour, int scale) const
{
    const int base = property(armour, PARM_AC) * scale;

    // [ds] effectively: ac_value * (22 + Arm) / 22, where Arm = Armour Skill.
    const int AC = base * (440 + skill(SK_ARMOUR, 20)) / 440;

    // The deformed don't fit into body armour very well.
    // (This includes nagas and armataurs.)
    if (get_armour_slot(armour) != SLOT_BODY_ARMOUR)
        return AC;

    int penalty = get_form()->get_base_ac_penalty(base);
    if (get_mutation_level(MUT_DEFORMED) || get_mutation_level(MUT_PSEUDOPODS))
        penalty += base / 2; // Should we double this if you have both?
    return max(0, AC - penalty);
}

/**
 * What bonus AC are you getting from your species?
 *
 * Does not account for any real mutations, such as scales or thick skin, that
 * you may have as a result of your species.
 * @param temp Whether to account for transformations.
 * @returns how much AC you are getting from your species "fake mutations" * 100
 */
int player::racial_ac(bool temp) const
{
    // drac scales suppressed in all serious forms, except dragon
    if (species::is_draconian(species)
        && (!player_is_shapechanged() || form == transformation::dragon
            || !temp))
    {
        int AC = 400 + 100 * experience_level / 3;  // max 13
        return AC;
    }

    if (!(player_is_shapechanged() && temp))
    {
        if (species == SP_NAGA)
            return 100 * experience_level / 3;              // max 9
        else if (species == SP_GARGOYLE)
        {
            return 200 + 100 * experience_level * 2 / 5     // max 20
                       + 100 * max(0, experience_level - 7) * 2 / 5;
        }
    }

    return 0;
}

// Each instance of this class stores a mutation which might change a
// player's AC and how much their AC should change if the player has
// said mutation.
class mutation_ac_changes{
    public:
        /**
         * The AC a player gains from a given mutation. If the player
         * lacks said mutation, return 0.
         *
         * @return How much AC to give the player for the handled
         *         mutation.
         */
        int get_ac_change_for_mutation(){
            int ac_change = 0;

            int mutation_level = you.get_mutation_level(mut, mutation_activation_threshold);

            switch (mutation_level){
                case 0:
                    ac_change = 0;
                    break;
                case 1:
                case 2:
                case 3:
                    ac_change = ac_changes[mutation_level - 1];
                    break;
            }

            // The output for this function is scaled differently than the UI.
            return ac_change * 100;
        }

        mutation_ac_changes(mutation_type mut_aug,
                            mutation_activity_type mutation_activation_threshold_aug,
                            vector<int> ac_changes_aug)
        : mut (mut_aug),
          mutation_activation_threshold (mutation_activation_threshold_aug),
          ac_changes (ac_changes_aug)
        {
        }

    private:
        mutation_type mut;
        mutation_activity_type mutation_activation_threshold;
        vector<int> ac_changes;
};

// Constant vectors for the most common mutation ac results used in
// all_mutation_ac_changes
const vector<int> ONE_TWO_THREE  = {1,2,3};
const vector<int> TWO_THREE_FOUR = {2,3,4};

vector<mutation_ac_changes> all_mutation_ac_changes = {
     mutation_ac_changes(MUT_GELATINOUS_BODY,        mutation_activity_type::PARTIAL, ONE_TWO_THREE)
    ,mutation_ac_changes(MUT_TOUGH_SKIN,             mutation_activity_type::PARTIAL, ONE_TWO_THREE)
    ,mutation_ac_changes(MUT_SHAGGY_FUR,             mutation_activity_type::PARTIAL, ONE_TWO_THREE)
    ,mutation_ac_changes(MUT_PHYSICAL_VULNERABILITY, mutation_activity_type::PARTIAL, {-5,-10,-15})
    // Scale mutations are more easily disabled (forms etc.). This appears to be for flavour reasons.
    // Preserved behaviour from before mutation ac was turned to data.
    ,mutation_ac_changes(MUT_IRIDESCENT_SCALES,      mutation_activity_type::FULL,    {2, 4, 6})
    ,mutation_ac_changes(MUT_RUGGED_BROWN_SCALES,    mutation_activity_type::FULL,    ONE_TWO_THREE)
    ,mutation_ac_changes(MUT_ICY_BLUE_SCALES,        mutation_activity_type::FULL,    TWO_THREE_FOUR)
    ,mutation_ac_changes(MUT_MOLTEN_SCALES,          mutation_activity_type::FULL,    TWO_THREE_FOUR)
    ,mutation_ac_changes(MUT_SLIMY_GREEN_SCALES,     mutation_activity_type::FULL,    TWO_THREE_FOUR)
    ,mutation_ac_changes(MUT_THIN_METALLIC_SCALES,   mutation_activity_type::FULL,    TWO_THREE_FOUR)
    ,mutation_ac_changes(MUT_YELLOW_SCALES,          mutation_activity_type::FULL,    TWO_THREE_FOUR)
    ,mutation_ac_changes(MUT_SHARP_SCALES,           mutation_activity_type::FULL,    ONE_TWO_THREE)
    ,mutation_ac_changes(MUT_IRON_FUSED_SCALES,      mutation_activity_type::FULL,    {5, 5, 5})
};

/**
 * The AC changes the player has from mutations.
 *
 * Mostly additions from things like scales, but the physical vulnerability
 * mutation is also accounted for.
 *
 * @return  The player's AC gain from mutation, with 100 scaling (i.e,
 *          the returned result 100 times the UI shows as of Jan 2020)
*/
int player::ac_changes_from_mutations() const
{

    int AC = 0;

    for (vector<mutation_ac_changes>::iterator it =
            all_mutation_ac_changes.begin();
            it != all_mutation_ac_changes.end(); ++it)
    {
        AC += it->get_ac_change_for_mutation();
    }

    return AC;
}

/**
 * The player's "base" armour class, before transitory buffs are applied.
 *
 * (This is somewhat arbitrarily defined - forms, for example, are considered
 * to be long-lived for these purposes.)
 *
 * @param   A scale by which the player's base AC is multiplied.
 * @return  The player's AC, multiplied by the given scale.
 */
int player::base_ac(int scale) const
{
    int AC = 0;

    for (const player_equip_entry& entry : equipment.items)
    {
        if (entry.melded || entry.is_overflow)
            continue;

        const item_def& item = entry.get_item();
        if (item.base_type != OBJ_ARMOUR)
            continue;

        // Shield plusses are for SH, not AC.
        if (get_armour_slot(item) != SLOT_OFFHAND)
        {
            AC += base_ac_from(item, 100);
            AC += item.plus * 100;
        }

        if (item.brand == SPARM_PROTECTION)
            AC += 300;
    }

    AC += wearing_jewellery(RING_PROTECTION) * 100;

    AC += scan_artefacts(ARTP_AC) * 100;

    AC += get_form()->get_ac_bonus();

    AC += racial_ac(true);

    AC += ac_changes_from_mutations();

    return AC * scale / 100;
}

int player::corrosion_amount() const
{
    int corrosion = 0;

    if (duration[DUR_CORROSION])
        corrosion += you.props[CORROSION_KEY].get_int();

    if (you.on_current_level && env.level_state & LSTATE_SLIMY_WALL)
        corrosion += slime_wall_corrosion(&you);

    if (player_in_branch(BRANCH_DIS))
        corrosion += 8;

    return corrosion;
}

static int _meek_bonus()
{
    const int scale_bottom = 27; // full bonus given at this HP and below
    const int hp_per_ac = 4;
    const int max_ac = 7;
    const int scale_top = scale_bottom + hp_per_ac * max_ac;
    return min(max(0, (scale_top - you.hp) / hp_per_ac), max_ac);
}

int player::armour_class() const
{
    return div_rand_round(armour_class_scaled(100), 100);
}

int player::armour_class_scaled(int scale) const
{
    int AC = base_ac(100);

    if (duration[DUR_ICY_ARMOUR])
    {
        AC += max(0, 500 + you.props[ICY_ARMOUR_KEY].get_int() * 8
                     - unadjusted_body_armour_penalty() * 50);
    }

    if (has_mutation(MUT_ICEMAIL))
        AC += 100 * player_icemail_armour_class();

    if (duration[DUR_FIERY_ARMOUR])
        AC += 700;

    if (duration[DUR_QAZLAL_AC])
        AC += 300;

    if (duration[DUR_SPWPN_PROTECTION])
    {
        AC += 700;
        if (you.unrand_equipped(UNRAND_MEEK))
            AC += _meek_bonus() * 100;
    }

    if (you.wearing_ego(OBJ_GIZMOS, SPGIZMO_PARRYREV))
    {
        const static int rev_bonus[] = {0, 200, 400, 500};
        AC += rev_bonus[you.rev_tier()];
    }

    if (you.props.exists(PASSWALL_ARMOUR_KEY))
        AC += you.props[PASSWALL_ARMOUR_KEY].get_int() * 100;

    if (you.duration[DUR_PHALANX_BARRIER])
        AC += you.props[PHALANX_BARRIER_POWER_KEY].get_int();

    AC -= 100 * corrosion_amount();

    AC += sanguine_armour_bonus();

    return AC * scale / 100;
}

void player::refresh_rampage_hints()
{
    rampage_hints.clear();
    if (you.rampaging())
        for (coord_def delta : Compass)
            if ((delta.x || delta.y) && get_rampage_target(delta))
                you.rampage_hints.insert(you.pos() + delta);
}

 /**
  * Guaranteed damage reduction.
  *
  * The percentage of the damage received that is guaranteed to be reduced
  * by the armour. As the AC roll is done before GDR is applied, GDR is only
  * useful when the AC roll is inferior to it. Therefore a higher GDR means
  * more damage reduced, but also more often.
  *
  * \f[ GDR = 16 \times (AC)^\frac{1}{4} \f]
  *
  * \return GDR as a percentage.
  **/
int player::gdr_perc(bool random) const
{
    int ac = random ? armour_class() : armour_class_scaled(1);
    return (int)(16 * sqrt(sqrt(max(0, ac))));
}

/**
 * What is the player's actual, current EV, possibly relative to an attacker,
 * including various temporary penalties?
 *
 * @param ignore_temporary Whether to ignore temporary modifiers.
 * @param act              The creature that the player is attempting to evade,
                           if any. May be null.
 * @return                 The player's relevant EV.
 */
int player::evasion(bool ignore_temporary, const actor* act) const
{
    int base_evasion = div_rand_round(_player_evasion(100, ignore_temporary), 100);

    const bool attacker_invis = act && !act->visible_to(this);
    const int invis_penalty
        = attacker_invis && !ignore_temporary ? 10 : 0;

    return base_evasion - invis_penalty;
}

int player::evasion_scaled(int scale, bool ignore_temporary, const actor* act) const
{
    int base_evasion = _player_evasion(scale, ignore_temporary);

    const bool attacker_invis = act && !act->visible_to(this);
    const int invis_penalty
        = attacker_invis && !ignore_temporary ? 10 : 0;

    return base_evasion - invis_penalty * scale;
}

/**
 * What would our natural AC/EV/SH and fail rate for all known spells be if we
 * wore a given piece of equipment instead of whatever might be in that slot
 * currently (if anything)?
 *
 * Note: non-artefact rings of evasion/protection and amulets of reflection
 * are excepted from using this function.
 *
 * @param new_item  The equipment item in question.
 * @param ac        The player's AC if this item were equipped.
 * @param ev        The player's EV if this item were equipped.
 * @param sh        The player's SH if this item were equipped.
 * @param fail      The player's raw spell fail for all spells if this item
 *                  were equipped.
 */
void player::preview_stats_with_specific_item(int scale, const item_def& new_item,
                                              int *ac, int *ev, int *sh,
                                              FixedVector<int, MAX_KNOWN_SPELLS> *fail)
{
    // Since there are a lot of things which can affect the calculation of
    // EV/SH/fail, including artifact properties on either the item we're
    // equipped or the one we're swapping out for it, we check by very briefly
    // 'putting on' the new item and calling the normal calculation functions.

    // Save the current state of player equipment, so that we can rewind once
    // we're done.
    unwind_var<player_equip_set> rewind_eq(you.equipment);

    // Players can only equip items that are currently in their inventory, so if
    // we're trying to preview an item *not* in our inventory, we must copy it
    // into the hidden 'preview' slot before calling any subsequent functions.
    if (!in_inventory(new_item))
    {
        you.inv[ENDOFPACK] = new_item;
        you.inv[ENDOFPACK].pos = ITEM_IN_INVENTORY;
        you.inv[ENDOFPACK].link = ENDOFPACK;
    }
    item_def& item = in_inventory(new_item) ? you.inv[new_item.link] : you.inv[ENDOFPACK];

    // Figure out where this item should be equipped. If there is an empty slot,
    // use that. If there are no actually *choices* in what to remove to make
    // room for it (ie: a single item occupying the only slot), swap those out
    // automatically. Otherwise, if the player would have to make a choice over
    // what to remove (eg: which ring to switch out), we cannot do that
    // automatically, so pretend we have a free slot and just tell the player
    // the change that would happen if they used this item *on top* of their
    // current equipment.

    vector<vector<item_def*>> removal_candidates;  // Items eligible to be removed
    vector<item_def*> to_remove;                   // List of items chosen to swap out
    equipment_slot slot = you.equipment.find_slot_to_equip_item(item, removal_candidates, true);

    // Check if the required removals involve any decisions. If they do not,
    // gather list of items to swap out.
    bool needs_choice = false;
    if (!removal_candidates.empty())
    {
        for (vector<item_def*> slot_candidates : removal_candidates)
        {
            if (slot_candidates.size() > 1)
            {
                needs_choice = true;
                break;
            }
        }
    }

    if (!needs_choice)
        for (vector<item_def*> slot_candidates : removal_candidates)
            to_remove.push_back(slot_candidates[0]);

    // Place this in its 'default' slot (should be good enough for preview purposes)
    if (slot == SLOT_UNUSED)
        slot = get_all_item_slots(item)[0];

    // Swap out old gear.
    for (item_def* _item : to_remove)
        you.equipment.remove(*_item);

    // Now actually equip the item.
    you.equipment.add(item, slot);
    you.equipment.update();

    // Now, simply calculate AC/EV/SH without temporary boosts.
    *ac = base_ac(scale);
    *ev = evasion_scaled(scale, true);
    *sh = player_displayed_shield_class(scale, true);

    for (int i = 0; i < MAX_KNOWN_SPELLS; ++i)
        (*fail)[i] = raw_spell_fail(spells[i]);

    // Clear out our preview item. (Other equipment state will be unwound
    // automatically.)
    you.inv[ENDOFPACK].clear();
}

void player::preview_stats_without_specific_item(int scale,
                                                 const item_def& item_to_remove,
                                                 int *ac, int *ev, int *sh,
                                                 FixedVector<int, MAX_KNOWN_SPELLS> *fail)
{
    // Verify that the item is currently equipped
    // (or this function will give bogus info)
    ASSERT(item_is_equipped(item_to_remove));

    // Save the current state of player equipment, so that we can rewind once
    // we're done.
    unwind_var<player_equip_set> rewind_eq(you.equipment);

    // Remove item and calculate resulting stats.
    you.equipment.remove(item_to_remove);
    you.equipment.update();

    *ac = base_ac(scale);
    *ev = evasion_scaled(scale, true);
    *sh = player_displayed_shield_class(scale, true);
    for (int i = 0; i < MAX_KNOWN_SPELLS; ++i)
        (*fail)[i] = raw_spell_fail(spells[i]);
}

bool player::heal(int amount)
{
    int oldhp = hp;
    ::inc_hp(amount);
    return oldhp < hp;
}

/**
 * What is the player's (current) mon_holy_type category?
 * Stays up to date with god for evil/unholy
 * Nonliving (statues, etc), undead, or alive.
 *
 * @param temp      Whether to consider temporary effects.
 * @param incl_forms Whether to take the player's current form into account.
 * @return          The player's holiness category.
 */
mon_holy_type player::holiness(bool temp, bool incl_form) const
{
    mon_holy_type holi;

    // Lich form takes precedence over a species' base holiness
    // Alive Vampires are MH_NATURAL
    if (is_lifeless_undead(temp))
        holi = MH_UNDEAD;
    else if (species::is_nonliving(you.species))
        holi = MH_NONLIVING;
    else
        holi = MH_NATURAL;

    if (incl_form)
    {
        // TODO: move this into data
        const transformation f = temp ? form : default_form;
        if (f == transformation::statue
             || f == transformation::wisp
             || f == transformation::storm)
        {
            holi = MH_NONLIVING;
        }
        else if (f == transformation::slaughter)
            holi = MH_DEMONIC;
    }

    // Petrification takes precedence over base holiness and lich form
    if (temp && petrified())
        holi = MH_NONLIVING;

    // possible XXX: Monsters get evil/unholy bits set on spell selection
    //  should players?
    return holi;
}

// With temp (default true), report temporary effects such as lichform.
bool player::undead_or_demonic(bool temp) const
{
    // This is only for TSO-related stuff, so demonspawn are included.
    return undead_state(temp) || species == SP_DEMONSPAWN;
}

bool player::evil() const
{
    return is_evil_god(religion)
        || species == SP_DEMONSPAWN
        || you.has_mutation(MUT_VAMPIRISM)
        || actor::evil();
}

bool player::is_holy() const
{
    return bool(holiness() & MH_HOLY) || is_good_god(religion);
}

bool player::is_nonliving(bool temp, bool incl_form) const
{
    return bool(holiness(temp, incl_form) & MH_NONLIVING);
}

// This is a stub. Check is used only for silver damage. Worship of chaotic
// gods should probably be checked in the non-existing player::is_unclean,
// which could be used for something Zin-related (such as a priestly monster).
int player::how_chaotic(bool /*check_spells_god*/) const
{
    return 0;
}

/**
 * Does the player need to breathe?
 *
 * Pretty much only matters for confusing spores and drowning damage.
 *
 * @return  Whether the player has no need to breathe.
 */
bool player::is_unbreathing() const
{
    return is_nonliving() || is_lifeless_undead()
           || form == transformation::tree;
}

bool player::is_insubstantial() const
{
    return form == transformation::wisp
        || form == transformation::storm;
}

bool player::is_amorphous() const
{
    // Maybe this'll change some day?...
    return false;
}

int player::res_acid() const
{
    return player_res_acid();
}

int player::res_fire() const
{
    return player_res_fire();
}

int player::res_steam() const
{
    return player_res_steam();
}

int player::res_cold() const
{
    return player_res_cold();
}

int player::res_elec() const
{
    return player_res_electricity();
}

bool player::res_water_drowning() const
{
    return is_unbreathing()
           || cur_form(true)->player_can_swim()
           || you.species == SP_GREY_DRACONIAN && draconian_dragon_exception();
}

int player::res_poison(bool temp) const
{
    return player_res_poison(true, temp);
}

bool player::res_miasma(bool temp) const
{
    if (has_mutation(MUT_FOUL_STENCH)
        || is_nonliving(temp)
        || cur_form(temp)->res_miasma())
    {
        return true;
    }

    if (unrand_equipped(UNRAND_EMBRACE))
        return true;

    return is_lifeless_undead();
}


bool player::res_sticky_flame() const
{
    return player_res_sticky_flame();
}

int player::res_holy_energy() const
{
    if (undead_or_demonic())
        return -1;

    if (is_holy())
        return 3;

    return 0;
}

int player::res_foul_flame() const
{
    if (undead_or_demonic())
        return 1;

    if (is_holy())
        return -1;

    return 0;
}

int player::res_negative_energy(bool intrinsic_only) const
{
    return player_prot_life(true, true, !intrinsic_only);
}

bool player::res_torment() const
{
    if (you.get_mutation_level(MUT_TORMENT_RESISTANCE) >= 2)
        return true;

    return get_form()->res_neg() == 3
           || you.has_mutation(MUT_VAMPIRISM) && !you.vampire_alive
           || you.petrified()
    // This should probably be (you.holiness & MH_PLANT), but
    // transforming doesn't currently make you a plant, and I suspect
    // changing that would cause other bugs. (For example, being able
    // to wield holy weapons as a demonspawn in treeform & keep them
    // while untransformed?)
           || you.form == transformation::tree
           || you.form == transformation::fungus
#if TAG_MAJOR_VERSION == 34
           || you.unrand_equipped(UNRAND_ETERNAL_TORMENT)
#endif
           ;
}

bool player::res_polar_vortex() const
{
    // Full control of the winds around you can negate a hostile polar vortex.
    return duration[DUR_VORTEX] ? 1 : 0;
}

bool player::res_petrify(bool temp) const
{
    return get_mutation_level(MUT_PETRIFICATION_RESISTANCE)
           || cur_form(temp)->res_petrify();
}

bool player::res_constrict() const
{
    return is_insubstantial()
           || get_mutation_level(MUT_SPINY)
           || you.unrand_equipped(UNRAND_SLICK_SLIPPERS)
           || you.duration[DUR_CONSTRICTION_IMMUNITY];
}

int player::willpower() const
{
    return player_willpower();
}

int player_willpower(bool temp)
{
    if (temp && you.form == transformation::slaughter)
        return WILL_INVULN;

    if (you.unrand_equipped(UNRAND_FOLLY))
        return 0;

    int rm = you.experience_level * species::get_wl_modifier(you.species);

    // randarts
    rm += WL_PIP * you.scan_artefacts(ARTP_WILLPOWER);

    // body armour
    const item_def *body_armour = you.body_armour();
    if (body_armour)
        rm += armour_type_prop(body_armour->sub_type, ARMF_WILLPOWER) * WL_PIP;

    // ego armours
    rm += WL_PIP * you.wearing_ego(OBJ_ARMOUR, SPARM_WILLPOWER);

    rm -= 2 * WL_PIP * you.wearing_ego(OBJ_ARMOUR, SPARM_GUILE);

    // rings of willpower
    rm += WL_PIP * you.wearing_jewellery(RING_WILLPOWER);

    // Mutations
    rm += WL_PIP * you.get_mutation_level(MUT_STRONG_WILLED);
    rm += WL_PIP * you.get_mutation_level(MUT_DEMONIC_WILL);
    rm -= WL_PIP * you.get_mutation_level(MUT_WEAK_WILLED);

    if (you.form == transformation::death &&
        (temp || you.default_form == transformation::death))
    {
        rm += WL_PIP;
    }

    // In this moment, you are euphoric.
    if (you.duration[DUR_ENLIGHTENED])
        rm += WL_PIP;

    // Trog's Hand
    if (you.duration[DUR_TROGS_HAND] && temp)
        rm += WL_PIP * 2;

    const int max_will = MAX_WILL_PIPS * WL_PIP;
    if (rm > max_will)
        rm = max_will;

    // Enchantment/environment effect
    if ((you.duration[DUR_LOWERED_WL]
         || player_in_branch(BRANCH_TARTARUS)) && temp)
    {
        rm /= 2;
    }

    if (rm < 0)
        rm = 0;

    return rm;
}

/**
 * Is the player prevented from teleporting? If so, why?
 *
 * @param blinking      Are you blinking or teleporting?
 * @param temp          Are you being prevented by a temporary effect?
 * @return              Why the player is prevented from teleporting, if they
 *                      are; else, the empty string.
 */
string player::no_tele_reason(bool blinking, bool temp) const
{
    if (stasis())
        return "Your stasis prevents you from teleporting.";

    if (!blinking)
    {
        if (crawl_state.game_is_sprint())
            return "Long-range teleportation is disallowed in Dungeon Sprint.";
        else if (player_in_branch(BRANCH_GAUNTLET))
        {
            return "A magic seal in the Gauntlet prevents long-range "
                "teleports.";
        }
    }

    vector<string> problems;

    if (temp && duration[DUR_DIMENSION_ANCHOR])
        problems.emplace_back("locked down by a dimension anchor");

    if (temp && form == transformation::tree)
        problems.emplace_back("held in place by your roots");

    vector<const item_def *> notele_items;
    if (has_notele_item(&notele_items))
    {
        vector<string> worn_notele;
        bool found_nonartefact = false;

        for (const auto item : notele_items)
        {
            if (item->base_type == OBJ_WEAPONS)
            {
                problems.push_back(make_stringf("wielding %s",
                                                item->name(DESC_A).c_str()));
            }
            else
                worn_notele.push_back(item->name(DESC_A));
        }

        if (worn_notele.size() > (problems.empty() ? 3 : 1))
        {
            problems.push_back(
                make_stringf("wearing %s %s preventing teleportation",
                             number_in_words(worn_notele.size()).c_str(),
                             found_nonartefact ? "items": "artefacts"));
        }
        else if (!worn_notele.empty())
        {
            problems.push_back(
                make_stringf("wearing %s",
                             comma_separated_line(worn_notele.begin(),
                                                  worn_notele.end()).c_str()));
        }
    }

    if (problems.empty())
        return ""; // no problem

    return make_stringf("You cannot %s because you are %s.",
                        blinking ? "blink" : "teleport",
                        comma_separated_line(problems.begin(),
                                             problems.end()).c_str());
}

/**
 * Is the player prevented from teleporting/blinking right now?
 *
 * @param blinking      Are you blinking or teleporting?
 * @param temp          Are you being prevented by a temporary effect?
 * @return              Whether the player is prevented from teleportation.
 */
bool player::no_tele(bool blinking, bool temp) const
{
    return !no_tele_reason(blinking, temp).empty();
}

bool player::racial_permanent_flight() const
{
    return has_mutation(MUT_TENGU_FLIGHT)
        || get_mutation_level(MUT_BIG_WINGS)
        || has_mutation(MUT_FLOAT);
}

/**
 * Check for sources of flight from species and (optionally) equipment.
 */
bool player::permanent_flight(bool include_equip) const
{
    if (get_form()->forbids_flight())
        return false;

    return include_equip && attribute[ATTR_PERM_FLIGHT] // equipment
        || racial_permanent_flight()                 // species muts
        || get_form()->enables_flight()
           && get_form(you.default_form)->enables_flight();
}

/**
 * Returns true if player spellcasting is considered unholy.
 *
 * Checks to see if the player is wielding the Majin-Bo.
 *
 * @return          Whether player spellcasting is an unholy act.
 */
bool player::spellcasting_unholy() const
{
    return you.unrand_equipped(UNRAND_MAJIN);
}

/**
 * What is the player's (current) place on the Undead Spectrum?
 * (alive, semi-undead (vampire), or very dead (ghoul, mummy, lich)
 *
 * @param temp  Whether to consider temporary effects (lichform)
 * @return      The player's undead state.
 */
undead_state_type player::undead_state(bool temp) const
{
    if (temp && form == transformation::death)
        return US_UNDEAD;
    return species::undead_type(species);
}

bool player::nightvision() const
{
    return have_passive(passive_t::nightvision)
           || has_mutation(MUT_FOUL_SHADOW)
           || you.unrand_equipped(UNRAND_BRILLIANCE)
           || you.unrand_equipped(UNRAND_SHADOWS);
}

reach_type player::reach_range() const
{
    const item_def *wpn = weapon();
    const item_def *off = offhand_weapon();
    const reach_type wpn_reach = wpn ? weapon_reach(*wpn) : REACH_NONE;
    const reach_type off_reach = off ? weapon_reach(*off) : REACH_NONE;
    return max(wpn_reach, off_reach);
}

monster_type player::mons_species(bool /*zombie_base*/) const
{
    return species::to_mons_species(species);
}

bool player::poison(actor *agent, int amount, bool force)
{
    return ::poison_player(amount, agent? agent->name(DESC_A, true) : "", "",
                           force);
}

void player::expose_to_element(beam_type element, int _strength,
                               bool slow_cold_blood)
{
    ::expose_player_to_element(element, _strength, slow_cold_blood);
}

void player::blink(bool ignore_stasis)
{
    uncontrolled_blink(ignore_stasis);
}

void player::teleport(bool now, bool wizard_tele)
{
    ASSERT(!crawl_state.game_is_arena());
    if (now)
        you_teleport_now(wizard_tele);
    else
        you_teleport();
}

int player::hurt(const actor *agent, int amount, beam_type flavour,
                 kill_method_type kill_type, string source, string aux,
                 bool /*cleanup_dead*/, bool /*attacker_effects*/)
{
    // We ignore cleanup_dead here.
    if (!agent)
    {
        // FIXME: This can happen if a deferred_damage_fineff does damage
        // to a player from a dead monster. We should probably not do that,
        // but it could be tricky to fix, so for now let's at least avoid
        // a crash even if it does mean funny death messages.
        ouch(amount, kill_type, MID_NOBODY, aux.c_str(), false, source.c_str());
    }
    else
    {
        ouch(amount, kill_type, agent->mid, aux.c_str(),
             agent->visible_to(this), source.c_str());
    }

    if ((flavour == BEAM_DESTRUCTION || flavour == BEAM_MINDBURST)
        && has_blood())
    {
        blood_spray(pos(), type, amount / 5);
    }

    return amount;
}

/**
 * Checks to see whether the player can be dislodged by physical effects.
 * This only accounts for the "mountain boots" unrand, not being stationary, etc.
 *
 * @param event A message to be printed if the player cannot be dislodged.
 *               If empty, nothing will be printed.
 * @return Whether the player can be moved.
 */
bool player::resists_dislodge(string event) const
{
    if (!you.unrand_equipped(UNRAND_MOUNTAIN_BOOTS))
        return false;
    if (!event.empty())
        mprf("Your boots keep you from %s.", event.c_str());
    return true;
}

bool player::corrode_equipment(const char* corrosion_source, int degree)
{
    // always increase duration, but...
    increase_duration(DUR_CORROSION, 10 + roll_dice(2, 4), 50,
                      make_stringf("%s corrodes you!",
                                   corrosion_source).c_str());

    // the more corrosion you already have, the lower the odds of more
    // Static environmental corrosion doesn't factor in
    // Reduce corrosion amount by 50% if you have resistance
    int prev_corr = props[CORROSION_KEY].get_int();
    bool did_corrode = false;
    for (int i = 0; i < degree; i++)
        if (!x_chance_in_y(prev_corr, prev_corr + 28))
        {
            prev_corr += res_corr() ? 2 : 4;
            props[CORROSION_KEY] = prev_corr;
            did_corrode = true;
        }

    if (did_corrode)
    {
        redraw_armour_class = true;
        wield_change = true;
    }
    return true;
}

/**
 * Attempts to apply corrosion to the player and deals acid damage.
 *
 * @param evildoer the cause of this acid splash.
 */
void player::splash_with_acid(actor* evildoer)
{
    acid_corrode(3);

    const int dam = roll_dice(4, 3);
    const int post_res_dam = resist_adjust_damage(&you, BEAM_ACID, dam);

    mprf("You are splashed with acid%s%s",
         post_res_dam > 0 ? "" : " but take no damage",
         attack_strength_punctuation(post_res_dam).c_str());
    if (post_res_dam > 0)
    {
        if (post_res_dam < dam)
            canned_msg(MSG_YOU_RESIST);

        ouch(post_res_dam, KILLED_BY_ACID,
             evildoer ? evildoer->mid : MID_NOBODY);
    }
}

void player::acid_corrode(int acid_strength)
{
    if (binomial(3, acid_strength + 1, 30))
        corrode_equipment();
}

bool player::drain(const actor */*who*/, bool quiet, int pow)
{
    return drain_player(pow, !quiet);
}

void player::confuse(actor */*who*/, int str)
{
    confuse_player(str);
}

/**
 * Paralyse the player for str turns.
 *
 *  Duration is capped at 13.
 *
 * @param who Pointer to the actor who paralysed the player.
 * @param str The number of turns the paralysis will last.
 * @param source Description of the source of the paralysis.
 */
void player::paralyse(const actor *who, int str, string source)
{
    ASSERT(!crawl_state.game_is_arena());

    if (stasis())
    {
        mpr("Your stasis prevents you from being paralysed.");
        return;
    }

    // The who check has an effect in a few cases, most notably making
    // Death's Door + Borg's paralysis unblockable.
    if (who && (duration[DUR_PARALYSIS] || duration[DUR_STUN_IMMUNITY]))
    {
        mpr("You shrug off the repeated attempt to disable you.");
        return;
    }

    int &paralysis(duration[DUR_PARALYSIS]);

    const bool use_actor_name = source.empty() && who != nullptr;
    if (use_actor_name)
        source = who->name(DESC_A);

    if (!paralysis && !source.empty())
    {
        take_note(Note(NOTE_PARALYSIS, str, 0, source));
        // use the real name here even for invisible monsters
        props[DISABLED_BY_KEY] = use_actor_name ? who->name(DESC_A, true)
                                               : source;
    }
    else
        props.erase(DISABLED_BY_KEY);

    if (asleep())
        you.awaken();

    mpr("You suddenly lose the ability to move!");
    _pruneify();

    paralysis = min(str, 13) * BASELINE_DELAY;

    stop_delay(true, true);
    stop_directly_constricting_all(false);
    stop_channelling_spells();
    redraw_armour_class = true;
    redraw_evasion = true;
}

void player::petrify(const actor *who, bool force)
{
    ASSERT(!crawl_state.game_is_arena());

    if (res_petrify() && !force)
    {
        canned_msg(MSG_YOU_UNAFFECTED);
        return;
    }

    if (duration[DUR_DIVINE_STAMINA] > 0)
    {
        mpr("Your divine stamina protects you from petrification!");
        return;
    }

    // Petrification always wakes you up
    if (asleep())
        you.awaken();

    if (petrifying())
    {
        mpr("Your limbs have turned to stone.");
        duration[DUR_PETRIFYING] = 1;
        return;
    }

    if (petrified())
        return;

    duration[DUR_PETRIFYING] = 3 * BASELINE_DELAY;

    if (who)
        props[DISABLED_BY_KEY] = who->name(DESC_A, true);
    else
        props.erase(DISABLED_BY_KEY);

    redraw_evasion = true;
    mprf(MSGCH_WARN, "You are slowing down.");
}

bool player::fully_petrify(bool /*quiet*/)
{
    duration[DUR_PETRIFIED] = 6 * BASELINE_DELAY
                        + random2(4 * BASELINE_DELAY);
    redraw_armour_class = true;
    redraw_evasion = true;
    mpr("You have turned to stone.");
    _pruneify();

    stop_delay(true, true);
    stop_channelling_spells();

    return true;
}

bool player::vex(const actor* source, int dur)
{
    if (you.clarity())
    {
        mprf("Your clarity prevents you from becoming vexed.");
        return false;
    }
    else if (duration[DUR_STUN_IMMUNITY])
    {
        mpr("You shrug off the repeated attempt to disable you.");
        return false;
    }
    else if (you.duration[DUR_VEXED])
        return false;

    mprf(MSGCH_WARN, "You feel overwhelmed by frustration!");
    you.duration[DUR_VEXED] = dur * BASELINE_DELAY;

    if (source)
        props[DISABLED_BY_KEY] = source->name(DESC_A, true);
    else
        props.erase(DISABLED_BY_KEY);

    stop_delay(true, true);
    stop_directly_constricting_all(false);
    stop_channelling_spells();

    return true;
}

void player::give_stun_immunity(int dur)
{
    const int immunity = dur * BASELINE_DELAY;
    duration[DUR_STUN_IMMUNITY] = immunity;
    if (you.petrified())
    {
        // no chain paralysis + petrification combos!
        duration[DUR_STUN_IMMUNITY] += duration[DUR_PETRIFIED];
        return;
    }
}

void player::slow_down(actor */*foe*/, int str)
{
    ::slow_player(str);
}


int player::has_claws(bool allow_tran) const
{
    if (allow_tran)
    {
        // these transformations bring claws with them
        if (form == transformation::dragon)
            return DRAGON_CLAWS;
    }

    return get_mutation_level(MUT_CLAWS, allow_tran);
}

bool player::has_usable_claws(bool allow_tran) const
{
    return has_claws(allow_tran)
           && !you.equipment.slot_is_fully_covered(SLOT_GLOVES);
}

int player::has_talons(bool allow_tran) const
{
    // XXX: Do merfolk in water belong under allow_tran?
    if (fishtail)
        return 0;

    return get_mutation_level(MUT_TALONS, allow_tran);
}

bool player::has_usable_talons(bool allow_tran) const
{
    return has_talons(allow_tran)
           && !you.equipment.slot_is_fully_covered(SLOT_BOOTS);
}

int player::has_hooves(bool allow_tran) const
{
    // XXX: Do merfolk in water belong under allow_tran?
    if (fishtail)
        return 0;

    return get_mutation_level(MUT_HOOVES, allow_tran);
}

bool player::has_usable_hooves(bool allow_tran) const
{
    return has_hooves(allow_tran)
           && !you.equipment.slot_is_fully_covered(SLOT_BOOTS);
}

int player::has_fangs(bool allow_tran) const
{
    if (allow_tran && form == transformation::dragon)
        return DRAGON_FANGS;

    return get_mutation_level(MUT_FANGS, allow_tran);
}

int player::has_usable_fangs(bool allow_tran) const
{
    return has_fangs(allow_tran);
}

bool player::has_tail(bool allow_tran) const
{
    if (allow_tran)
    {
        // these transformations bring a tail with them
        if (form == transformation::serpent
            || form == transformation::dragon)
        {
            return true;
        }

        // Most transformations suppress a tail.
        if (!form_keeps_mutations())
            return false;
    }

    // XXX: Do merfolk in water belong under allow_tran?
    if (species::is_draconian(species)
        || species == SP_FELID
        || has_mutation(MUT_CONSTRICTING_TAIL, allow_tran)
        || fishtail // XX respect allow_tran
        || get_mutation_level(MUT_ARMOURED_TAIL, allow_tran)
        || get_mutation_level(MUT_STINGER, allow_tran)
        || get_mutation_level(MUT_WEAKNESS_STINGER, allow_tran))
    {
        return true;
    }

    return false;
}

// Whether the player has a usable offhand for the
// purpose of punching.
bool player::has_usable_offhand() const
{
    return !you.equipment.slot_is_fully_covered(SLOT_OFFHAND)
            && !you.equipment.slot_is_fully_covered(SLOT_WEAPON_OR_OFFHAND);
}

bool player::has_usable_tentacle() const
{
    return usable_tentacles();
}

int player::usable_tentacles() const
{
    int numtentacle = has_usable_tentacles();

    if (numtentacle == 0)
        return false;

    int free_tentacles = numtentacle - num_constricting();

    if (shield())
        free_tentacles -= 2;

    const item_def* wp = you.equipment.get_first_slot_item(SLOT_WEAPON);
    if (wp)
    {
        hands_reqd_type hands_req = hands_reqd(*wp);
        free_tentacles -= 2 * hands_req + 2;
    }

    return free_tentacles;
}

int player::has_pseudopods(bool allow_tran) const
{
    return get_mutation_level(MUT_PSEUDOPODS, allow_tran);
}

int player::has_usable_pseudopods(bool allow_tran) const
{
    return has_pseudopods(allow_tran);
}

int player::arm_count() const
{
    // XX transformations? arm count per se isn't used by much though.

    return species::arm_count(species)
                    - get_mutation_level(MUT_MISSING_HAND);
}

int player::has_tentacles(bool allow_tran) const
{
    // tentacles count as a mutation for these purposes. (TODO: realmut?)
    if (you.has_mutation(MUT_TENTACLE_ARMS, allow_tran))
        return arm_count();

    return 0;
}

int player::has_usable_tentacles(bool allow_tran) const
{
    return has_tentacles(allow_tran);
}

bool player::sicken(int amount)
{
    ASSERT(!crawl_state.game_is_arena());

    if (res_miasma() || amount <= 0)
        return false;

    if (duration[DUR_DIVINE_STAMINA] > 0)
    {
        mpr("Your divine stamina protects you from disease!");
        return false;
    }

    mpr("You feel ill.");
    increase_duration(DUR_SICKNESS, amount, 210);

    return true;
}

/// Can the player see invisible things?
bool player::can_see_invisible() const
{
    if (crawl_state.game_is_arena())
        return true;

    if (wearing_jewellery(RING_SEE_INVISIBLE)
        || wearing_ego(OBJ_ARMOUR, SPARM_SEE_INVISIBLE)
        // randart gear
        || scan_artefacts(ARTP_SEE_INVISIBLE) > 0
        || you.duration[DUR_REVELATION])
    {
        return true;
    }

    return innate_sinv();
}

/// Can the player see invisible things without needing items' help?
bool player::innate_sinv() const
{
    if (has_mutation(MUT_ACUTE_VISION))
        return true;

    // antennae give sInvis at 3
    if (get_mutation_level(MUT_ANTENNAE) == 3)
        return true;

    if (get_mutation_level(MUT_EYEBALLS) == 3)
        return true;

    if (have_passive(passive_t::sinv))
        return true;

    return false;
}

bool player::invisible() const
{
    return duration[DUR_INVIS] && !backlit();
}

bool player::visible_to(const actor *looker) const
{
    if (crawl_state.game_is_arena())
        return false;

    const bool invis_to = invisible() && !looker->can_see_invisible()
                          && !in_water();
    if (this == looker)
        return !invis_to;

    const monster* mon = looker->as_monster();
    return mon->friendly()
        || (!mon->has_ench(ENCH_BLIND) && !invis_to);
}

/**
 * Is the player backlit?
 *
 * @param self_halo If false, ignore the player's self-halo.
 * @param temp If true, include temporary sources of being backlit.
 * @returns True if the player is backlit.
*/
bool player::backlit(bool self_halo, bool temp) const
{
    return temp && (player_severe_contamination()
                    || duration[DUR_CORONA]
                    || duration[DUR_STICKY_FLAME]
                    || duration[DUR_QUAD_DAMAGE]
                    || !umbraed() && haloed()
                       && (self_halo || halo_radius() == -1))
           || self_halo && you.form == transformation::flux;
    // TODO: find some way to mark !invis for autopickup while
    // fluxing while still marking it temp-useless (and while
    // marking it perma-useless for meteors)
}

bool player::umbra() const
{
    return !backlit() && umbraed() && !haloed();
}

// This is the imperative version.
void player::backlight()
{
    if (!duration[DUR_INVIS])
    {
        if (duration[DUR_CORONA])
            mpr("You glow brighter.");
        else
            mpr("You are outlined in light.");
    }
    else
        mpr("You feel strangely conspicuous.");

    increase_duration(DUR_CORONA, random_range(15, 35), 250);
}

bool player::can_mutate() const
{
    return true;
}

/**
 * Can the player be mutated without max HP drain instead?
 *
 * @param temp      Whether to consider temporary modifiers (lichform)
 * @return Whether the player will mutate when mutated, instead of draining
 *         max HP.
 */
bool player::can_safely_mutate(bool temp) const
{
    if (!can_mutate())
        return false;

    return undead_state(temp) == US_ALIVE
           || undead_state(temp) == US_SEMI_UNDEAD;
}

// Is the player too undead to bleed, rage, or polymorph?
bool player::is_lifeless_undead(bool temp) const
{
    if (temp && undead_state() == US_SEMI_UNDEAD)
        return !you.vampire_alive;
    else
        return undead_state(temp) == US_UNDEAD;
}

bool player::can_polymorph() const
{
    return !(transform_uncancellable || is_lifeless_undead());
}

bool player::has_blood(bool temp) const
{
    if (is_lifeless_undead(temp))
        return false;

    if (temp)
    {
        if (petrified())
            return false;

        return form_has_blood(form);
    }

    return species::has_blood(you.species);
}

bool player::has_bones(bool temp) const
{
    if (temp)
        return form_has_bones(you.form);

    return species::has_bones(you.species);
}

bool player::can_drink(bool temp) const
{
    if (temp && (you.form == transformation::death
                    || you.duration[DUR_NO_POTIONS]))
    {
        return false;
    }
    return !you.has_mutation(MUT_NO_DRINK);

}

bool player::is_stationary() const
{
    return form == transformation::tree;
}

bool player::is_motile() const
{
    return !is_stationary() && !you.duration[DUR_NO_MOMENTUM]
                            && !you.duration[DUR_FORTRESS_BLAST_TIMER];
}

bool player::malmutate(const string &reason)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!can_mutate())
        return false;

    const mutation_type mut_quality = one_chance_in(5) ? RANDOM_MUTATION
                                                       : RANDOM_BAD_MUTATION;
    if (mutate(mut_quality, reason))
    {
        learned_something_new(HINT_YOU_MUTATED);
        return true;
    }
    return false;
}

bool player::polymorph(int pow, bool allow_immobile)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!can_polymorph())
        return false;

    transformation f = transformation::none;

    vector<transformation> forms = {
        transformation::bat,
        transformation::wisp,
        transformation::pig,
    };
    if (allow_immobile)
    {
        forms.emplace_back(transformation::tree);
        forms.emplace_back(transformation::fungus);
    }

    for (int tries = 0; tries < 3; tries++)
    {
        f = forms[random2(forms.size())];

        // need to do a dry run first, as Zin's protection has a random factor
        if (cant_transform_reason(f, true).empty())
            break;

        f = transformation::none;
    }

    if (f != transformation::none && transform(pow, f, true))
    {
        transform_uncancellable = true;
        return true;
    }
    return false;
}

bool player::is_icy() const
{
    return false;
}

bool player::is_fiery() const
{
    return false;
}

bool player::is_skeletal() const
{
    return false;
}

void player::shiftto(const coord_def &c)
{
    crawl_view.shift_player_to(c);
    set_position(c);
    clear_invalid_constrictions();
}

bool player::asleep() const
{
    return duration[DUR_SLEEP];
}

bool player::can_feel_fear(bool include_unknown) const
{
    return (you.holiness() & (MH_NATURAL | MH_DEMONIC | MH_HOLY))
           && (!include_unknown || (!you.clarity() && !you.berserk()));
}

bool player::can_throw_large_rocks() const
{
    return species::can_throw_large_rocks(species);
}

bool player::can_smell() const
{
    return !you.is_lifeless_undead(true);
}

bool player::can_sleep(bool holi_only) const
{
    return !you.duration[DUR_STUN_IMMUNITY]
           && actor::can_sleep(holi_only)
           && !(you.form == transformation::fungus)
           && !(you.form == transformation::tree);
}

/**
 * Attempts to put the player to sleep.
 *
 * @param source    The actor that put the player to sleep (if any).
 * @param power     The power of the effect putting the player to sleep.
 * @param hibernate Whether the player is being put to sleep by 'ensorcelled
 *                  hibernation' (doesn't affect characters with rC, ignores
 *                  power), or by a normal sleep effect.
 */
void player::put_to_sleep(actor* source, int power, bool hibernate)
{
    ASSERT(!crawl_state.game_is_arena());

    const bool valid_target = hibernate ? can_hibernate() : can_sleep();
    if (!valid_target)
    {
        canned_msg(MSG_YOU_UNAFFECTED);
        return;
    }

    if (duration[DUR_STUN_IMMUNITY])
    {
        mpr("You shrug off repeated attempt to disable you.");
        return;
    }

    if (duration[DUR_PARALYSIS]
        || duration[DUR_PETRIFIED]
        || duration[DUR_PETRIFYING])
    {
        mpr("You can't fall asleep in your current state!");
        return;
    }

    if (source)
        props[DISABLED_BY_KEY] = source->name(DESC_A, true).c_str();
    else
        props.erase(DISABLED_BY_KEY);

    mpr("You fall asleep.");
    _pruneify();

    stop_directly_constricting_all(false);
    stop_channelling_spells();
    stop_delay(true, true);
    flash_view(UA_MONSTER, DARKGREY);

    // As above, do this after redraw.
    const int dur = hibernate ? 3 + random2avg(5, 2) :
                                5 + random2avg(power/10, 5);
    set_duration(DUR_SLEEP, dur);
    redraw_armour_class = true;
    redraw_evasion = true;
}

void player::awaken()
{
    ASSERT(!crawl_state.game_is_arena());

    duration[DUR_SLEEP] = 0;
    give_stun_immunity(random_range(3, 5));
    mpr("You wake up.");
    flash_view(UA_MONSTER, BLACK);
    redraw_armour_class = true;
    redraw_evasion = true;
}

void player::check_awaken(int disturbance)
{
    if (asleep() && x_chance_in_y(disturbance + 1, 50))
    {
        awaken();
        dprf("Disturbance of intensity %d awoke player", disturbance);
    }
}

bool player::may_pruneify() const {
    return you.unrand_equipped(UNRAND_PRUNE)
        && you.undead_state() == US_ALIVE;
}

int player::beam_resists(bolt &beam, int hurted, bool doEffects, string source)
{
    return check_your_resists(hurted, beam.flavour, source, &beam, doEffects);
}

bool player::shaftable() const
{
    return is_valid_shaft_level()
        && feat_is_shaftable(env.grid(pos()))
        // Prevent shafting the player during an apostle challenge; that would be a bit unfair.
        && !you.duration[DUR_BEOGH_DIVINE_CHALLENGE];
}

// Used for falling into traps and other bad effects, but is a slightly
// different effect from the player invokable ability.
bool player::do_shaft()
{
    // disabled in descent mode
    if (crawl_state.game_is_descent())
        return false;

    if (!shaftable()
        || resists_dislodge("falling into an unexpected shaft"))
    {
        return false;
    }
    if (you.species == SP_FORMICID)
    {
        mpr("Your tunneler's instincts keep you from falling into a shaft!");
        return false;
    }
    if (you_worship(GOD_YREDELEMNUL) && yred_torch_is_raised())
    {
        mpr("Yredelemnul refuses to let your conquest be stopped by a trick of"
            " the earth!");
        return false;
    }

    // Ensure altars, items, and shops discovered at the moment
    // the player gets shafted are correctly registered.
    maybe_update_stashes();

    down_stairs(DNGN_TRAP_SHAFT);

    return true;
}

bool player::can_do_shaft_ability(bool quiet) const
{
    if (!form_keeps_mutations())
    {
        if (!quiet)
            mpr("You can't shaft yourself in your current form.");
        return false;
    }

    if (attribute[ATTR_HELD])
    {
        if (!quiet)
            mprf("You can't shaft yourself while %s.", held_status());
        return false;
    }

    if (!you.is_motile())
    {
        if (!quiet)
            mpr("You can't shaft yourself while stuck.");
        return false;
    }

    if (feat_is_shaftable(env.grid(pos())))
    {
        if (!is_valid_shaft_level(false))
        {
            if (!quiet)
                mpr("You can't shaft yourself on this level.");
            return false;
        }
    }
    else
    {
        if (!quiet)
            mpr("You can't shaft yourself on this terrain.");
        return false;
    }

    return true;
}

// Like do_shaft, but forced by the player.
// It has a slightly different set of rules.
bool player::do_shaft_ability()
{
    if (can_do_shaft_ability(true))
    {
        mpr("A shaft appears beneath you!");
        down_stairs(DNGN_TRAP_SHAFT, true);
        return true;
    }
    else
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        redraw_screen();
        update_screen();
        return false;
    }
}

bool player::did_escape_death() const
{
    return escaped_death_cause != NUM_KILLBY;
}

void player::reset_escaped_death()
{
    escaped_death_cause = NUM_KILLBY;
    escaped_death_aux   = "";
}

void player::add_gold(int delta)
{
    set_gold(gold + delta);
}

void player::del_gold(int delta)
{
    set_gold(gold - delta);
}

void player::set_gold(int amount)
{
    ASSERT(amount >= 0);

    if (amount != gold)
    {
        const int old_gold = gold;
        gold = amount;
        shopping_list.gold_changed(old_gold, gold);

        // XXX: this might benefit from being in its own function
        if (you_worship(GOD_GOZAG))
        {
            for (const auto& power : get_god_powers(you.religion))
            {
                const int cost = get_gold_cost(power.abil);
                if (gold >= cost && old_gold < cost)
                    power.display(true, "You now have enough gold to %s.");
                else if (old_gold >= cost && gold < cost)
                    power.display(false, "You no longer have enough gold to %s.");
            }
            you.redraw_title = true;
        }
    }
}

void player::increase_duration(duration_type dur, int turns, int cap,
                               const char* msg)
{
    if (msg)
        mpr(msg);
    cap *= BASELINE_DELAY;

    // If we are already over the cap, do not increase or decrease duration.
    if (cap && duration[dur] > cap)
        return;

    duration[dur] += turns * BASELINE_DELAY;
    if (cap && duration[dur] > cap)
        duration[dur] = cap;
}

void player::set_duration(duration_type dur, int turns,
                          int cap, const char * msg)
{
    duration[dur] = 0;
    increase_duration(dur, turns, cap, msg);
}

void player::goto_place(const level_id &lid)
{
    where_are_you = static_cast<branch_type>(lid.branch);
    depth = lid.depth;
    ASSERT_RANGE(depth, 1, brdepth[where_are_you] + 1);
}

static int _constriction_escape_chance(int attempts)
{
    static int escape_chance[] = {40, 75, 100};
    return escape_chance[min(3, attempts) - 1];
}

bool player::attempt_escape()
{
    monster *themonst;

    if (!is_constricted())
        return true;

    themonst = monster_by_mid(constricted_by);
    ASSERT(themonst);
    escape_attempts += 1;

    const auto constr_typ = get_constrict_type();
    const string object
        = constr_typ == CONSTRICT_ROOTS ? "the roots'"
          : constr_typ == CONSTRICT_BVC ? "the zombie hands'"
                                        : themonst->name(DESC_ITS, true);

    if (x_chance_in_y(_constriction_escape_chance(escape_attempts), 100))
    {
        mprf("You escape %s grasp.", object.c_str());

        // Stun the monster we struggled again and prevent the player from being
        // constricted for several turns (so that they are guaranteed to be able
        // to make it up the stairs after pulling away this way)
        if (constr_typ == CONSTRICT_MELEE)
        {
            themonst->speed_increment -= 10;
            you.duration[DUR_CONSTRICTION_IMMUNITY] = 20;
        }

        stop_being_constricted(true);

        return true;
    }
    else
    {
        mprf("%s grasp on you weakens, but your attempt to escape fails.",
             object.c_str());
        turn_is_over = true;
        return false;
    }
}

void player::sentinel_mark(bool trap)
{
    flash_tile(you.pos(), YELLOW, 120, TILE_BOLT_SENTINEL_MARK);
    if (duration[DUR_SENTINEL_MARK])
    {
        mpr("The mark upon you grows brighter.");
        increase_duration(DUR_SENTINEL_MARK, random_range(20, 40), 180);
    }
    else
    {
        mprf(MSGCH_WARN, "A sentinel's mark forms upon you.");
        increase_duration(DUR_SENTINEL_MARK, trap ? random_range(25, 40)
                                                  : random_range(35, 60),
                          250);
    }
}

/*
 * Is the player too terrified to move (because of fungusform)?
 *
 * @return true iff there is an alarming monster anywhere near a fungusform player.
 */
bool player::is_nervous()
{
    if (form != transformation::fungus)
        return false;
    for (monster_near_iterator mi(&you); mi; ++mi)
    {
        if (made_nervous_by(*mi))
            return true;
    }
    return false;
}

/*
 * Does monster `mons` make the player nervous (in fungusform)?
 *
 * @param mons  the monster to check
 * @return      true iff mons is non-null, player is fungal, and `mons` is a threatening monster.
 */
bool player::made_nervous_by(const monster *mons)
{
    if (form != transformation::fungus)
        return false;
    if (!mons)
        return false;
    if (!mons_is_wandering(*mons)
        && !mons->asleep()
        && !mons->confused()
        && !mons->cannot_act()
        && mons_is_threatening(*mons)
        && !mons->wont_attack()
        && !mons->neutral())
    {
        return true;
    }
    return false;
}

void player::weaken(const actor */*attacker*/, int pow)
{
    if (!duration[DUR_WEAK])
        mprf(MSGCH_WARN, "You feel your attacks grow feeble.");
    else
        mprf(MSGCH_WARN, "You feel as though you will be weak longer.");

    increase_duration(DUR_WEAK, pow + random2(pow + 3), 50);
}

bool player::strip_willpower(actor */*attacker*/, int dur, bool quiet)
{
    // Only prints a message when you gain this status for the first time,
    // replicating old behavior. Should this change?
    if (!quiet && !you.duration[DUR_LOWERED_WL])
        mpr("Your willpower is stripped away!");

    you.increase_duration(DUR_LOWERED_WL, dur, 40);

    return true;
}

/**
 * Check if the player is about to die from flight/form expiration.
 *
 * Check whether the player is on a cell which would be deadly if not for some
 * temporary condition, and if such condition is expiring. In that case, we
 * give a strong warning to the player. The actual message printing is done
 * by the caller.
 *
 * @param dur the duration to check for dangerous expiration.
 * @param p the coordinates of the cell to check. Defaults to player position.
 * @return whether the player is in immediate danger.
 */
bool need_expiration_warning(duration_type dur, dungeon_feature_type feat)
{
    if (!is_feat_dangerous(feat, true) || !dur_expiring(dur))
        return false;

    if (dur == DUR_FLIGHT)
        return true;
    else if (dur == DUR_TRANSFORMATION
             && (form_can_swim()) || form_can_fly())
    {
        return true;
    }
    return false;
}

bool need_expiration_warning(duration_type dur, coord_def p)
{
    return need_expiration_warning(dur, env.grid(p));
}

bool need_expiration_warning(dungeon_feature_type feat)
{
    return need_expiration_warning(DUR_FLIGHT, feat)
           || need_expiration_warning(DUR_TRANSFORMATION, feat);
}

bool need_expiration_warning(coord_def p)
{
    return need_expiration_warning(env.grid(p));
}

static string _constriction_description()
{
    string cinfo = "";
    vector<string> c_name;

    const int num_free_tentacles = you.usable_tentacles();
    if (num_free_tentacles)
    {
        cinfo += make_stringf("You have %d tentacle%s available for constriction.",
                              num_free_tentacles,
                              num_free_tentacles > 1 ? "s" : "");
    }

    const auto constr_typ = you.get_constrict_type();
    if (constr_typ == CONSTRICT_MELEE)
    {
        const monster * const constrictor = monster_by_mid(you.constricted_by);
        ASSERT(constrictor);

        if (!cinfo.empty())
            cinfo += "\n";

        cinfo += make_stringf("You are being constricted by %s.",
                              constrictor->name(DESC_A).c_str());
    }

    if (you.is_constricting())
    {
        for (const auto &entry : *you.constricting)
        {
            monster *whom = monster_by_mid(entry);
            ASSERT(whom);

            if (whom->get_constrict_type() != CONSTRICT_MELEE)
                continue;

            c_name.push_back(whom->name(DESC_A));
        }

        if (!c_name.empty())
        {
            if (!cinfo.empty())
                cinfo += "\n";

            cinfo += "You are constricting ";
            cinfo += comma_separated_line(c_name.begin(), c_name.end());
            cinfo += ".";
        }
    }

    return cinfo;
}

/**
 *   The player's radius of monster detection.
 *   @return   the radius in which a player can detect monsters.
**/
int player_monster_detect_radius()
{
    int radius = you.get_mutation_level(MUT_ANTENNAE) * 2;

    if (you.unrand_equipped(UNRAND_HOOD_ASSASSIN))
        radius = max(radius, 4);
    if (have_passive(passive_t::detect_montier))
        radius = max(radius, you.piety / 20);
    return min(radius, LOS_MAX_RANGE);
}

/**
 * Return true if the player has angered Pandemonium by picking up or moving
 * the Orb of Zot.
 */
bool player_on_orb_run()
{
    return you.chapter == CHAPTER_ESCAPING
           || you.chapter == CHAPTER_ANGERED_PANDEMONIUM;
}

/**
 * Return true if the player has the Orb of Zot.
 * @return  True if the player has the Orb, false otherwise.
 */
bool player_has_orb()
{
    return you.chapter == CHAPTER_ESCAPING;
}

bool player::form_uses_xl() const
{
    // No body parts that translate in any way to something fisticuffs could
    // matter to, the attack mode is different. Plus, it's weird to have
    // users of one particular [non-]weapon be effective for this
    // unintentional form while others can just run or die. I believe this
    // should apply to more forms, too.  [1KB]
    return !get_form()->get_unarmed_uses_skill();
}

bool player::can_wear_barding(bool temp) const
{
    if (temp && get_form()->slot_is_blocked(SLOT_BARDING))
        return false;

    return species::wears_barding(species);
}

static int _get_potion_heal_factor(bool temp=true)
{
    // healing factor is expressed in halves, so default is 2/2 -- 100%.
    int factor = 2;

    // start with penalties
    if (temp)
        factor -= you.unrand_equipped(UNRAND_VINES) ? 2 : 0;
    factor -= you.mutation[MUT_NO_POTION_HEAL];

    // then apply bonuses - Kryia's doubles potion healing
    if (temp)
        factor *= you.unrand_equipped(UNRAND_KRYIAS) ? 2 : 1;

    if (you.mutation[MUT_DOUBLE_POTION_HEAL])
        factor *= 2;

    // make sure we don't turn healing negative.
    return max(0, factor);
}

void print_potion_heal_message()
{
    // Don't give multiple messages in weird cases with both enhanced
    // and reduced healing.
    if (_get_potion_heal_factor() > 2)
    {
        if (you.unrand_equipped(UNRAND_KRYIAS))
        {
            mprf("%s enhances the healing.",
                 you.body_armour()->name(DESC_THE, false, false, false).c_str());
        }
        else if (you.has_mutation(MUT_DOUBLE_POTION_HEAL))
            mpr("You savour every drop.");
        else
            mpr("The healing is enhanced."); // bad message, but this should
                                             // never be possible anyway
    }
    else if (_get_potion_heal_factor() == 0)
        mpr("Your system rejects the healing.");
    else if (_get_potion_heal_factor() < 2)
        mpr("Your system partially rejects the healing.");
}

bool player::can_potion_heal(bool temp)
{
    return _get_potion_heal_factor(temp) > 0;
}

int player::scale_potion_healing(int healing_amount)
{
    return div_rand_round(healing_amount * _get_potion_heal_factor(), 2);
}

int player::scale_potion_mp_healing(int healing_amount)
{
    // Slightly ugly to partially duplicate the logic of _get_potion_heal_factor()
    // but vine stalkers shouldn't be unable to get value out of !magic, and so
    // this must ignore MUT_NO_POTION_HEAL
    if (you.unrand_equipped(UNRAND_KRYIAS))
        healing_amount *= 2;

    if (you.mutation[MUT_DOUBLE_POTION_HEAL])
        healing_amount *= 2;

    return healing_amount;
}

#define REV_PERCENT_KEY "rev_percent"

int player::rev_percent() const
{
    if (!you.props.exists(REV_PERCENT_KEY))
        return 0;
    return you.props[REV_PERCENT_KEY].get_int();
}

int player::rev_tier() const
{
    const int rev = rev_percent();
    if (rev >= FULL_REV_PERCENT)
        return 3;
    else if (rev >= FULL_REV_PERCENT / 2)
        return 2;
    else if (rev > 0)
        return 1;

    return 0;
}

void player::rev_down(int dur)
{
    // Drop from 100% to 0 in about 12 normal turns (120 aut).
    const int perc_lost = div_rand_round(dur * 5, 6);
    you.props[REV_PERCENT_KEY] = max(0, you.rev_percent() - perc_lost);

    if (you.wearing_ego(OBJ_GIZMOS, SPGIZMO_PARRYREV))
        you.redraw_armour_class = true;
}

void player::rev_up(int dur)
{
    // We want to hit 66% rev, where penalties vanish, in 40 aut on average.
    // Over that time, we'll lose 40*5/6 = ~34% to rev_down().
    // So we want to gain an average of (66+34)/40 = ~5/2 rev% per aut.
    // Fuzz it between 4/2 and 6/2 (ie 2x to 3x) to avoid tracking.
    const int perc_gained = random_range(dur * 2, dur * 3);
    you.props[REV_PERCENT_KEY] = min(100, you.rev_percent() + perc_gained);

    if (you.wearing_ego(OBJ_GIZMOS, SPGIZMO_PARRYREV))
        you.redraw_armour_class = true;
}

void player_open_door(coord_def doorpos)
{
    // Finally, open the closed door!
    set<coord_def> all_door;
    find_connected_identical(doorpos, all_door);
    const char *adj, *noun;
    get_door_description(all_door.size(), &adj, &noun);

    const string door_desc_adj  =
        env.markers.property_at(doorpos, MAT_ANY, "door_description_adjective");
    const string door_desc_noun =
        env.markers.property_at(doorpos, MAT_ANY, "door_description_noun");
    if (!door_desc_adj.empty())
        adj = door_desc_adj.c_str();
    if (!door_desc_noun.empty())
        noun = door_desc_noun.c_str();

    if (!you.confused())
    {
        string door_open_prompt =
            env.markers.property_at(doorpos, MAT_ANY, "door_open_prompt");

        bool ignore_exclude = false;

        if (!door_open_prompt.empty())
        {
            door_open_prompt += " (y/N)";
            if (!yesno(door_open_prompt.c_str(), true, 'n', true, false))
            {
                if (is_exclude_root(doorpos))
                    canned_msg(MSG_OK);
                else
                {
                    if (yesno("Put travel exclusion on door? (Y/n)",
                              true, 'y'))
                    {
                        // Zero radius exclusion right on top of door.
                        set_exclude(doorpos, 0);
                    }
                }
                interrupt_activity(activity_interrupt::force);
                return;
            }
            ignore_exclude = true;
        }

        if (!ignore_exclude && is_exclude_root(doorpos))
        {
            string prompt = make_stringf("This %s%s is marked as excluded! "
                                         "Open it anyway?", adj, noun);

            if (!yesno(prompt.c_str(), true, 'n', true, false))
            {
                canned_msg(MSG_OK);
                interrupt_activity(activity_interrupt::force);
                return;
            }
        }
    }

    const int skill = 8 + you.skill_rdiv(SK_STEALTH, 4, 3);

    string berserk_open = env.markers.property_at(doorpos, MAT_ANY,
                                                  "door_berserk_verb_open");
    string berserk_adjective = env.markers.property_at(doorpos, MAT_ANY,
                                                       "door_berserk_adjective");
    string door_open_creak = env.markers.property_at(doorpos, MAT_ANY,
                                                     "door_noisy_verb_open");
    string door_airborne = env.markers.property_at(doorpos, MAT_ANY,
                                                   "door_airborne_verb_open");
    string door_open_verb = env.markers.property_at(doorpos, MAT_ANY,
                                                    "door_verb_open");

    if (you.berserk())
    {
        // XXX: Better flavour for larger doors?
        if (silenced(you.pos()))
        {
            if (!berserk_open.empty())
            {
                berserk_open += ".";
                mprf(berserk_open.c_str(), adj, noun);
            }
            else
                mprf("The %s%s flies open!", adj, noun);
        }
        else
        {
            if (!berserk_open.empty())
            {
                if (!berserk_adjective.empty())
                    berserk_open += " " + berserk_adjective;
                else
                    berserk_open += ".";
                mprf(MSGCH_SOUND, berserk_open.c_str(), adj, noun);
            }
            else
                mprf(MSGCH_SOUND, "The %s%s flies open with a bang!", adj, noun);
            noisy(15, you.pos());
        }
    }
    else if (one_chance_in(skill) && !silenced(you.pos()))
    {
        if (!door_open_creak.empty())
            mprf(MSGCH_SOUND, door_open_creak.c_str(), adj, noun);
        else
        {
            mprf(MSGCH_SOUND, "As you open the %s%s, it creaks loudly!",
                 adj, noun);
        }
        noisy(10, you.pos());
    }
    else
    {
        const char* verb;
        if (you.airborne())
        {
            if (!door_airborne.empty())
                verb = door_airborne.c_str();
            else
                verb = "You reach down and open the %s%s.";
        }
        else
        {
            if (!door_open_verb.empty())
               verb = door_open_verb.c_str();
            else
               verb = "You open the %s%s.";
        }

        mprf(verb, adj, noun);
    }

    vector<coord_def> excludes;
    for (const auto &dc : all_door)
    {
        if (cell_is_runed(dc))
            explored_tracked_feature(env.grid(dc));
        dgn_open_door(dc);
        set_terrain_changed(dc);
        dungeon_events.fire_position_event(DET_DOOR_OPENED, dc);

        // Even if some of the door is out of LOS, we want the entire
        // door to be updated. Hitting this case requires a really big
        // door!
        if (env.map_knowledge(dc).seen())
        {
            env.map_knowledge(dc).set_feature(env.grid(dc));
#ifdef USE_TILE
            tile_env.bk_bg(dc) = tileidx_feature_base(env.grid(dc));
#endif
        }

        if (is_excluded(dc))
            excludes.push_back(dc);
    }

    update_exclusion_los(excludes);
    viewwindow();
    update_screen();
    you.turn_is_over = true;
}

void player_close_door(coord_def doorpos)
{
    // Finally, close the opened door!
    string berserk_close = env.markers.property_at(doorpos, MAT_ANY,
                                                "door_berserk_verb_close");
    const string berserk_adjective = env.markers.property_at(doorpos, MAT_ANY,
                                                "door_berserk_adjective");
    const string door_close_creak = env.markers.property_at(doorpos, MAT_ANY,
                                                "door_noisy_verb_close");
    const string door_airborne = env.markers.property_at(doorpos, MAT_ANY,
                                                "door_airborne_verb_close");
    const string door_close_verb = env.markers.property_at(doorpos, MAT_ANY,
                                                "door_verb_close");
    const string door_desc_adj  = env.markers.property_at(doorpos, MAT_ANY,
                                                "door_description_adjective");
    const string door_desc_noun = env.markers.property_at(doorpos, MAT_ANY,
                                                "door_description_noun");
    set<coord_def> all_door;
    find_connected_identical(doorpos, all_door);
    const auto door_vec = vector<coord_def>(all_door.begin(), all_door.end());

    const char *adj, *noun;
    get_door_description(all_door.size(), &adj, &noun);
    const string waynoun_str = make_stringf("%sway", noun);
    const char *waynoun = waynoun_str.c_str();

    if (!door_desc_adj.empty())
        adj = door_desc_adj.c_str();
    if (!door_desc_noun.empty())
    {
        noun = door_desc_noun.c_str();
        waynoun = noun;
    }

    for (const coord_def& dc : all_door)
    {
        if (monster* mon = monster_at(dc))
        {
            const bool mons_unseen = !you.can_see(*mon);
            if (mons_unseen || (mon->holiness() & MH_NONLIVING))
            {
                mprf("Something is blocking the %s!", waynoun);
                // No free detection!
                if (mons_unseen)
                    you.turn_is_over = true;
            }
            else
                mprf("There's a creature in the %s!", waynoun);
            return;
        }

        if (env.igrid(dc) != NON_ITEM)
        {
            if (!has_push_spaces(dc, false, &door_vec))
            {
                mprf("There's something jamming the %s.", waynoun);
                return;
            }
        }

        // messaging with gateways will be inconsistent if this isn't last
        if (you.pos() == dc)
        {
            mprf("There's a thick-headed creature in the %s!", waynoun);
            return;
        }
    }
    const int you_old_top_item = env.igrid(you.pos());

    bool items_moved = false;
    for (const coord_def& dc : all_door)
        items_moved |= push_items_from(dc, &door_vec);

    // TODO: if only one thing moved, use that item's name
    // TODO: handle des-derived strings.  (Better yet, find a way to not have
    // format strings in des...)
    const char *items_msg = items_moved ? ", pushing everything out of the way"
                                        : "";

    const int skill = 8 + you.skill_rdiv(SK_STEALTH, 4, 3);

    if (you.berserk())
    {
        if (silenced(you.pos()))
        {
            if (!berserk_close.empty())
            {
                berserk_close += ".";
                mprf(berserk_close.c_str(), adj, noun);
            }
            else
                mprf("You slam the %s%s shut%s!", adj, noun, items_msg);
        }
        else
        {
            if (!berserk_close.empty())
            {
                if (!berserk_adjective.empty())
                    berserk_close += " " + berserk_adjective;
                else
                    berserk_close += ".";
                mprf(MSGCH_SOUND, berserk_close.c_str(), adj, noun);
            }
            else
            {
                mprf(MSGCH_SOUND, "You slam the %s%s shut with a bang%s!",
                                  adj, noun, items_msg);
            }

            noisy(15, you.pos());
        }
    }
    else if (one_chance_in(skill) && !silenced(you.pos()))
    {
        if (!door_close_creak.empty())
            mprf(MSGCH_SOUND, door_close_creak.c_str(), adj, noun);
        else
        {
            mprf(MSGCH_SOUND, "As you close the %s%s%s, it creaks loudly!",
                              adj, noun, items_msg);
        }

        noisy(10, you.pos());
    }
    else
    {
        if (you.airborne())
        {
            if (!door_airborne.empty())
                mprf(door_airborne.c_str(), adj, noun);
            else
                mprf("You reach down and close the %s%s%s.", adj, noun, items_msg);
        }
        else
        {
            if (!door_close_verb.empty())
                mprf(door_close_verb.c_str(), adj, noun);
            else
                mprf("You close the %s%s%s.", adj, noun, items_msg);
        }
    }

    vector<coord_def> excludes;
    for (const coord_def& dc : all_door)
    {
        // Once opened, formerly runed doors become normal doors.
        dgn_close_door(dc);
        set_terrain_changed(dc);
        dungeon_events.fire_position_event(DET_DOOR_CLOSED, dc);

        // Even if some of the door is out of LOS once it's closed
        // (or even if some of it is out of LOS when it's open), we
        // want the entire door to be updated.
        if (env.map_knowledge(dc).seen())
        {
            env.map_knowledge(dc).set_feature(env.grid(dc));
#ifdef USE_TILE
            tile_env.bk_bg(dc) = tileidx_feature_base(env.grid(dc));
#endif
        }

        if (is_excluded(dc))
            excludes.push_back(dc);
    }

    update_exclusion_los(excludes);

    // item pushing may have moved items under the player
    if (env.igrid(you.pos()) != you_old_top_item)
        item_check();
    you.turn_is_over = true;
}

/**
 * Return a string describing the player's hand(s) taking a given verb.
 *
 * @param plural_verb    A plural-agreeing verb. ("Smoulders", "are", etc.)
 * @return               A string describing the action.
 *                       E.g. "tentacles smoulder", "paw is", etc.
 */
string player::hands_verb(const string &plural_verb) const
{
    bool plural;
    const string hand = hand_name(true, &plural);
    return hand + " " + conjugate_verb(plural_verb, plural);
}

// Is this a character that would not normally have a preceding space when
// it follows a word?
static bool _is_end_punct(char c)
{
    switch (c)
    {
    case ' ': case '.': case '!': case '?':
    case ',': case ':': case ';': case ')':
        return true;
    }
    return false;
}

/**
 * Return a string describing the player's hand(s) (or equivalent) taking the
 * given action (verb).
 *
 * @param plural_verb   The plural-agreeing verb corresponding to the action to
 *                      take. E.g., "smoulder", "glow", "gain", etc.
 * @param object        The object or predicate complement of the action,
 *                      including any sentence-final punctuation. E.g. ".",
 *                      "new energy.", etc.
 * @return              A string describing the player's hands taking the
 *                      given action. E.g. "Your tentacle gains new energy."
 */
string player::hands_act(const string &plural_verb,
                         const string &object) const
{
    const bool space = !object.empty() && !_is_end_punct(object[0]);
    return "Your " + hands_verb(plural_verb) + (space ? " " : "") + object;
}

int player::inaccuracy() const
{
    int degree = actor::inaccuracy();
    if (get_mutation_level(MUT_MISSING_EYE))
        degree++;
    return degree;
}

/**
 * Handle effects that occur after the player character stops berserking.
 */
void player_end_berserk()
{
    if (!you.duration[DUR_PARALYSIS] && !you.petrified())
        mprf(MSGCH_WARN, "You are exhausted.");

    you.berserk_penalty = 0;

    int dur = 12 + roll_dice(2, 12);
    // Slow durations are multiplied by haste_mul (3/2), exhaustion lasts
    // slightly longer.
    you.increase_duration(DUR_BERSERK_COOLDOWN, dur * 2);

    // Don't trigger too many hints mode messages.
    const bool hints_slow = Hints.hints_events[HINT_YOU_ENCHANTED];
    Hints.hints_events[HINT_YOU_ENCHANTED] = false;

    if (you.unrand_equipped(UNRAND_BEAR_SPIRIT))
        dur = div_rand_round(dur * 2, 3);
    slow_player(dur);

    //Un-apply Berserk's +50% Current/Max HP
    calc_hp(true);

    learned_something_new(HINT_POSTBERSERK);
    Hints.hints_events[HINT_YOU_ENCHANTED] = hints_slow;
    quiver::set_needs_redraw();
}

/**
 * Does the player have the Sanguine Armour mutation (not suppressed by a form)
 * while being at a low enough HP (<67%) for its benefits to trigger?
 *
 * @return Whether Sanguine Armour should be active.
 */
bool sanguine_armour_valid()
{
    // why does this need to specify the activity type explicitly?
    return you.hp <= you.hp_max * 2 / 3
           && you.get_mutation_level(MUT_SANGUINE_ARMOUR, mutation_activity_type::FULL);
}

/// Trigger sanguine armour, updating the duration & messaging as appropriate.
void activate_sanguine_armour()
{
    const bool was_active = you.duration[DUR_SANGUINE_ARMOUR];
    you.duration[DUR_SANGUINE_ARMOUR] = random_range(60, 100);
    if (!was_active)
    {
        mpr("Your blood congeals into armour.");
        you.redraw_armour_class = true;
    }
}

/**
 * Refreshes the protective aura around the player after striking with
 * a weapon of protection. The duration is very short.
 */
void refresh_weapon_protection()
{
    if (!you.duration[DUR_SPWPN_PROTECTION])
        mpr("Your weapon exudes an aura of protection.");

    you.increase_duration(DUR_SPWPN_PROTECTION, 3 + random2(2), 5);
    you.redraw_armour_class = true;
}

void refresh_meek_bonus()
{
    const string MEEK_KEY = "meek_ac_key";
    const bool meek_possible = you.duration[DUR_SPWPN_PROTECTION]
                            && you.unrand_equipped(UNRAND_MEEK);
    const int bonus_ac = _meek_bonus();
    if (!meek_possible || !bonus_ac)
    {
        if (you.props.exists(MEEK_KEY))
        {
            you.props.erase(MEEK_KEY);
            you.redraw_armour_class = true;
        }
        return;
    }

    const int last_bonus = you.props[MEEK_KEY].get_int();
    if (last_bonus == bonus_ac)
        return;

    you.props[MEEK_KEY] = bonus_ac;
    you.redraw_armour_class = true;
}

// Is the player immune to a particular hex because of their
// intrinsic properties?
bool player::immune_to_hex(const spell_type hex) const
{
    switch (hex)
    {
    case SPELL_PARALYSIS_GAZE:
    case SPELL_PARALYSE:
    case SPELL_SLOW:
        return stasis();
    case SPELL_CHARMING:
    case SPELL_CONFUSE:
    case SPELL_CONFUSION_GAZE:
    case SPELL_MASS_CONFUSION:
        return clarity() || you.duration[DUR_DIVINE_STAMINA] > 0;
    case SPELL_TELEPORT_OTHER:
    case SPELL_BLINK_OTHER:
    case SPELL_BLINK_OTHER_CLOSE:
        return no_tele();
    case SPELL_MESMERISE:
    case SPELL_AVATAR_SONG:
    case SPELL_SIREN_SONG:
        return clarity() || berserk();
    case SPELL_CAUSE_FEAR:
        return clarity() || !(holiness() & MH_NATURAL) || berserk();
    case SPELL_PETRIFY:
        return res_petrify();
    case SPELL_POLYMORPH:
    case SPELL_PORKALATOR:
        return is_lifeless_undead();
    case SPELL_VIRULENCE:
        return res_poison() == 3;
    // don't include the hidden "sleep immunity" duration
    case SPELL_SLEEP:
    case SPELL_DREAM_DUST:
        return !actor::can_sleep();
    case SPELL_HIBERNATION:
        return !can_hibernate();
    default:
        return false;
    }
}

// Activate DUR_AGILE.
void player::be_agile(int pow)
{
    const bool were_agile = you.duration[DUR_AGILITY] > 0;
    mprf(MSGCH_DURATION, "You feel %sagile all of a sudden.",
         were_agile ? "more " : "");

    you.increase_duration(DUR_AGILITY, 35 + random2(pow), 80);
    if (!were_agile)
        you.redraw_evasion = true;
}

bool player::allies_forbidden()
{
    return get_mutation_level(MUT_NO_LOVE)
           || have_passive(passive_t::no_allies);
}
