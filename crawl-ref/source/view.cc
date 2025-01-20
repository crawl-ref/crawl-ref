/**
 * @file
 * @brief Misc function used to render the dungeon.
**/

#include "AppHdr.h"

#include "view.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <sstream>

#include "act-iter.h"
#include "artefact.h"
#include "branch.h"
#include "cio.h"
#include "cloud.h"
#include "clua.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "dgn-overview.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "tile-env.h"
#include "exclude.h"
#include "feature.h"
#include "files.h"
#include "fprop.h"
#include "god-conduct.h"
#include "god-passive.h"
#include "god-wrath.h"
#include "hints.h"
#include "items.h"
#include "item-prop.h" // get_weapon_brand
#include "libutil.h"
#include "macro.h"
#include "map-knowledge.h"
#include "message.h"
#include "misc.h"
#include "mon-abil.h" // boris_covet_orb
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-poly.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "nearby-danger.h"
#include "notes.h"
#include "options.h"
#include "output.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "show.h"
#include "showsymb.h"
#include "spl-transloc.h" // attract_monster
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "target.h"
#include "terrain.h"
#include "tilemcache.h"
#ifdef USE_TILE
 #include "tile-flags.h"
 #include "tilepick.h"
 #include "tilepick-p.h"
 #include "tileview.h"
#endif
#include "tiles-build-specific.h"
#include "traps.h"
#include "travel.h"
#include "unicode.h"
#include "unwind.h"
#include "viewchar.h"
#include "viewmap.h"
#include "xom.h"

static layers_type _layers = LAYERS_ALL;
static layers_type _layers_saved = LAYERS_NONE;

crawl_view_geometry crawl_view;

bool handle_seen_interrupt(monster* mons, vector<string>* msgs_buf)
{
    activity_interrupt_data aid(mons);
    if (mons->seen_context)
        aid.context = mons->seen_context;
    // XXX: Hack to make the 'seen' monster spec flag work.
    else if (testbits(mons->flags, MF_WAS_IN_VIEW)
             || testbits(mons->flags, MF_SEEN))
    {
        aid.context = SC_ALREADY_SEEN;
    }
    else
        aid.context = SC_NEWLY_SEEN;

    if (!mons_is_safe(mons))
    {
        return interrupt_activity(activity_interrupt::see_monster,
                                  aid, msgs_buf);
    }

    return false;
}

void flush_comes_into_view()
{
    if (!you.turn_is_over
        || (!you_are_delayed() && !crawl_state.is_repeating_cmd()))
    {
        return;
    }

    monster* mon = crawl_state.which_mon_acting();

    if (!mon || !mon->alive() || (mon->flags & MF_WAS_IN_VIEW)
        || !you.can_see(*mon))
    {
        return;
    }

    handle_seen_interrupt(mon);
}

void seen_monsters_react(int stealth)
{
    if (you.duration[DUR_TIME_STEP] || crawl_state.game_is_arena())
        return;

    for (monster_near_iterator mi(you.pos()); mi; ++mi)
    {
        if ((mi->asleep() || mi->behaviour == BEH_WANDER)
            && check_awaken(*mi, stealth))
        {
            behaviour_event(*mi, ME_ALERT, &you, you.pos(), false);

            // That might have caused a pacified monster to leave the level.
            if (!(*mi)->alive())
                continue;

            monster_consider_shouting(**mi);
        }

        if (!mi->visible_to(&you))
            continue;

        if (!mi->has_ench(ENCH_FRENZIED) && mi->can_see(you))
        {
            // Trigger Duvessa & Dowan upgrades
            if (mi->props.exists(ELVEN_ENERGIZE_KEY))
            {
                mi->props.erase(ELVEN_ENERGIZE_KEY);
                elven_twin_energize(*mi);
            }
            else if (mi->type == MONS_BORIS && player_has_orb()
                     && !mi->props.exists(BORIS_ORB_KEY))
            {
                mi->props[BORIS_ORB_KEY] = true;
                boris_covet_orb(*mi);
            }
#if TAG_MAJOR_VERSION == 34
            else if (mi->props.exists(OLD_DUVESSA_ENERGIZE_KEY))
            {
                mi->props.erase(OLD_DUVESSA_ENERGIZE_KEY);
                elven_twin_energize(*mi);
            }
            else if (mi->props.exists(OLD_DOWAN_ENERGIZE_KEY))
            {
                mi->props.erase(OLD_DOWAN_ENERGIZE_KEY);
                elven_twin_energize(*mi);
            }
#endif
        }
    }
}

static monster_type _mons_genus_keep_uniques(monster_type mc)
{
    return mons_is_unique(mc) ? mc : mons_genus(mc);
}

typedef struct
{
    const monster *mon;
    string name;
    int count;
    bool genus;
} details;

/**
 * Monster list simplification
 *
 * When too many monsters come into view at once, we group the ones with the
 * same genus, starting with the most represented genus.
 *
 * @param types monster types and the number of monster for each type.
 * @param genera monster genera and the number of monster for each genus.
 */
static void _genus_factoring(map<const string, details> &types,
                             map<monster_type, int> &genera)
{
    monster_type genus = MONS_NO_MONSTER;
    int num = 0;
    // Find the most represented genus.
    for (const auto &entry : genera)
        if (entry.second > num)
        {
            genus = entry.first;
            num = entry.second;
        }

    // The most represented genus has a single member.
    // No more factoring is possible, we're done.
    if (num == 1)
    {
        genera.clear();
        return;
    }

    genera.erase(genus);

    const monster *mon = nullptr;

    auto it = types.begin();
    do
    {
        if (_mons_genus_keep_uniques(it->second.mon->type) != genus)
        {
            ++it;
            continue;
        }

        // This genus has a single monster type. Can't factor.
        if (it->second.count == num)
            return;

        mon = it->second.mon;
        types.erase(it++);
    } while (it != types.end());
    ASSERT(mon); // There is a match as genera contains the monsters in types.

    const auto name = mons_type_name(genus, DESC_PLAIN);
    types[name] = {mon, name, num, true};
}

static bool _is_mon_equipment_worth_listing(const monster_info &mi)
{
    for (unsigned int i = 0; i <= MSLOT_LAST_VISIBLE_SLOT; ++i)
    {
        if (!mi.inv[i])
            continue;

        if (item_is_worth_listing(*mi.inv[i].get()))
            return true;
    }
    return false;
}

/// Return whether or not monster_info::_core_name() describes the inventory
/// for the monster "mon".
static bool _does_core_name_include_inventory(const monster *mon)
{
    return mon->type == MONS_DANCING_WEAPON || mon->type == MONS_SPECTRAL_WEAPON
           || mon->type == MONS_ARMOUR_ECHO;
}

/// Return a warning for the player about newly-seen monsters, as appropriate.
static string _monster_headsup(const vector<monster*> &monsters,
                               const unordered_set<const monster*> &single,
                               bool divine)
{
    string warning_msg = "";
    for (const monster* mon : monsters)
    {
        monster_info mi(mon);
        const bool zin_ided = mon->props.exists(ZIN_ID_KEY);
        const bool has_interesting_equipment
            = _is_mon_equipment_worth_listing(mi);
        if ((divine && !zin_ided)
            || (!divine && !has_interesting_equipment))
        {
            continue;
        }

        if (!divine && monsters.size() == 1)
            continue; // don't give redundant warnings for enemies

        // Don't repeat inventory. Non-single monsters may be merged, in which
        // case the name is just the name of the monster type.
        if (_does_core_name_include_inventory(mon) && single.count(mon))
            continue;

        if (warning_msg.size())
            warning_msg += " ";

        string monname;
        if (monsters.size() == 1)
            monname = mon->pronoun(PRONOUN_SUBJECTIVE);
        else if (mon->type == MONS_DANCING_WEAPON)
            monname = "There";
        else if (single.count(mon))
            monname = mon->full_name(DESC_THE);
        else
            monname = mon->full_name(DESC_A);
        warning_msg += uppercase_first(monname);

        warning_msg += " ";
        if (monsters.size() == 1)
            warning_msg += conjugate_verb("are", mon->pronoun_plurality());
        else
            warning_msg += "is";

        mons_equip_desc_level_type level = mon->type != MONS_DANCING_WEAPON
            ? DESC_IDENTIFIED : DESC_WEAPON_WARNING;

        if (!divine)
        {
            if (mon->type != MONS_DANCING_WEAPON)
                warning_msg += " ";
            warning_msg += get_monster_equipment_desc(mi, level, DESC_NONE);
            warning_msg += ".";
            continue;
        }

        if (you_worship(GOD_ZIN))
        {
            warning_msg += " a foul ";
            if (mon->has_ench(ENCH_GLOWING_SHAPESHIFTER))
                warning_msg += "glowing ";
            warning_msg += "shapeshifter";
        }
        else
        {
            // TODO: deduplicate
            if (mon->type != MONS_DANCING_WEAPON)
                warning_msg += " ";
            warning_msg += get_monster_equipment_desc(mi, level, DESC_NONE);
        }
        warning_msg += ".";
    }

    return warning_msg;
}

/// Let Ash/Zin warn the player about newly-seen monsters, as appropriate.
static void _divine_headsup(const vector<monster*> &monsters,
                            const unordered_set<const monster*> &single)
{
    const string warnings = _monster_headsup(monsters, single, true);
    if (!warnings.size())
        return;

    const string warning_msg = " warns you: " + warnings;
    simple_god_message(warning_msg.c_str());
#ifndef USE_TILE_LOCAL
    // XXX: should this really be here...?
    if (you_worship(GOD_ZIN))
        update_monster_pane();
#endif
}

static void _secular_headsup(const vector<monster*> &monsters,
                             const unordered_set<const monster*> &single)
{
    const string warnings = _monster_headsup(monsters, single, false);
    if (!warnings.size())
        return;
    mprf(MSGCH_MONSTER_WARNING, "%s", warnings.c_str());
}

/**
 * Calculate a list of monster types and genera from a list of monsters.
 *
 * @param monsters      A list of monsters (who may have just become visible)
 * @param[out] single   A list of the monsters in "monsters" which are to be
 *                      described separately ("a hog", not one of "2 hogs").
 * @param[out] species  A list of the monsters in "monsters" which are to be
 *                      described. Each element contains a monster, the number
 *                      of monsters to be included, and whether to refer to the
 *                      monster using the genus rather than the monster details.
 */
static void _count_monster_types(const vector<monster*> &monsters,
                                    unordered_set<const monster*> &single,
                                 vector<details> &species)
{
    const unsigned int max_types = 4;

    map<monster_type, int> genera; // This is the plural for genus!
    map<const string, details> species_s; // select which species to show
    for (const monster *mon : monsters)
    {
        const string name = mon->name(DESC_PLAIN);
        auto &det = species_s[name];
        det = {mon, name, det.count+1, false};
        genera[_mons_genus_keep_uniques(mon->type)]++;
    }

    // Don't merge named monsters (ghosts and the like). They're exciting!
    while (species_s.size() > max_types && !genera.empty())
        _genus_factoring(species_s, genera);

    map <const monster*, details> species_o; // put species in an order
    for (const auto &sp : species_s)
    {
        const auto det = sp.second;
        species_o[det.mon] = det;
        if (1 == det.count)
            single.insert(det.mon);
    }

    // Build a vector of species/genera sorted by one of the monsters from each.
    for (const auto &sp : species_o)
        species.push_back(sp.second);
}


static string _describe_monsters_from_species(const vector<details> &species)
{
    return comma_separated_fn(species.begin(), species.end(),
        [] (const details &det)
        {
            string name = det.name;
            if (mons_is_unique(det.mon->type)
                || mons_is_specially_named(det.mon->type)
                || !you.can_see(*det.mon))
            {
                return name;
            }
            else if (det.count > 1 && det.genus)
            {
                auto genus = mons_genus(det.mon->type);
                name = " "+pluralise(mons_type_name(genus, DESC_PLAIN));
            }
            else if (det.count > 1)
                name = " "+pluralise(det.name);
            return apply_description(DESC_A, name, det.count);
        });
}

/**
 * Return a string listing monsters in a human readable form.
 * E.g. "a hydra and 2 liches".
 *
 * @param monsters      A list of monsters that just became visible.
 */
string describe_monsters_condensed(const vector<monster*>& monsters)
{
    unordered_set<const monster*> single;
    vector<details> species;
    _count_monster_types(monsters, single, species);

    return _describe_monsters_from_species(species);
}

/**
 * Handle printing "foo comes into view" messages for newly appeared monsters.
 * Also let Ash/Zin warn the player about newly-seen monsters, as appropriate.
 *
 * @param msgs          A list of individual 'comes into view' messages; e.g.
 *                      "the goblin comes into view.", "Mara opens the gate."
 * @param monsters      A list of monsters that just became visible.
 */
static void _handle_comes_into_view(const vector<string> &msgs,
                                    const vector<monster*> monsters)
{
    unordered_set<const monster*> single;
    vector<details> species;
    _count_monster_types(monsters, single, species);

    if (monsters.size() == 1)
        mprf(MSGCH_MONSTER_WARNING, "%s", msgs[0].c_str());
    else
        mprf(MSGCH_MONSTER_WARNING, "%s come into view.",
             _describe_monsters_from_species(species).c_str());

    _divine_headsup(monsters, single);
    _secular_headsup(monsters, single);
}

/// If the player has the shout mutation, maybe shout at newly-seen monsters.
static void _maybe_trigger_shoutitis(const vector<monster*> monsters)
{
    if (!you.get_mutation_level(MUT_SCREAM))
        return;

    for (const monster* mon : monsters)
    {
        if (should_shout_at_mons(*mon))
        {
            yell(mon);
            return;
        }
    }
}

/// If the player has the attractive mutation, maybe attract newly-seen monsters.
static void _maybe_trigger_attractivitis(const vector<monster*> monsters)
{
    for (monster* mon : monsters)
    {
        if (!should_attract_mons(*mon))
            continue;
        const int dist = grid_distance(you.pos(), mon->pos());
        attract_monster(*mon, dist - 2); // never attract adjacent
    }
}

/// Let Gozag's wrath buff newly-seen hostile monsters, maybe.
static void _maybe_gozag_incite(vector<monster*> monsters)
{
    if (!player_under_penance(GOD_GOZAG))
        return;

    counted_monster_list mon_count;
    vector<monster *> incited;
    for (monster* mon : monsters)
    {
        // XXX: some of this is probably redundant with interrupt_activity
        if (mon->wont_attack()
            || mon->is_stationary()
            || mon->is_peripheral())
        {
            continue;
        }

        if (coinflip()
            && mon->get_experience_level() >= random2(you.experience_level))
        {
            mon_count.add(mon);
            incited.push_back(mon);
        }
    }

    if (incited.empty())
        return;

    string msg = make_stringf("%s incites %s against you.",
                              god_name(GOD_GOZAG).c_str(),
                              mon_count.describe().c_str());
    if (strwidth(msg) >= get_number_of_cols() - 2)
    {
        msg = make_stringf("%s incites your enemies against you.",
                           god_name(GOD_GOZAG).c_str());
    }
    mprf(MSGCH_GOD, GOD_GOZAG, "%s", msg.c_str());

    for (monster *mon : incited)
        gozag_incite(mon);
}

void update_monsters_in_view()
{
    int num_hostile = 0;
    vector<string> msgs;
    vector<monster*> monsters;

    for (monster_iterator mi; mi; ++mi)
    {
        if (you.see_cell(mi->pos()))
        {
            if (mi->attitude == ATT_HOSTILE)
                num_hostile++;

            if (mi->visible_to(&you))
            {
                if (handle_seen_interrupt(*mi, &msgs))
                    monsters.push_back(*mi);
                seen_monster(*mi);
            }
            else
                mi->flags &= ~MF_WAS_IN_VIEW;
        }
        else if (!you.turn_is_over)
        {
            if (mi->flags & MF_WAS_IN_VIEW)
            {
                // Reset client id so the player doesn't know (for sure) he
                // has seen this monster before when it reappears.
                mi->reset_client_id();
            }

            mi->flags &= ~MF_WAS_IN_VIEW;

            // If the monster hasn't been seen by the time that the player
            // gets control back then seen_context is out of date.
            mi->seen_context = SC_NONE;
        }
    }

    // Summoners may have lost their summons upon seeing us and converting
    // leaving invalid monsters in this vector.
    monsters.erase(
        std::remove_if(monsters.begin(), monsters.end(),
            [](const monster * m) { return !m->alive(); }),
        monsters.end());

    if (!msgs.empty())
    {
        _handle_comes_into_view(msgs, monsters);
        // XXX: does interrupt_activity() add 'comes into view' messages to
        // 'msgs' in ALL cases we want shoutitis/gozag wrath to trigger?
        _maybe_trigger_shoutitis(monsters);
        _maybe_trigger_attractivitis(monsters);
        _maybe_gozag_incite(monsters);
    }

    // Xom thinks it's hilarious the way the player picks up an ever
    // growing entourage of monsters while running through the Abyss.
    // To approximate this, if the number of hostile monsters in view
    // is greater than it ever was for this particular trip to the
    // Abyss, Xom is stimulated in proportion to the number of
    // hostile monsters. Thus if the entourage doesn't grow, then
    // Xom becomes bored.
    if (player_in_branch(BRANCH_ABYSS)
        && you.attribute[ATTR_ABYSS_ENTOURAGE] < num_hostile)
    {
        you.attribute[ATTR_ABYSS_ENTOURAGE] = num_hostile;
        xom_is_stimulated(12 * num_hostile);
    }
}


// We logically associate a difficulty parameter with each tile on each level,
// to make deterministic passive mapping work. It is deterministic so that the
// reveal order doesn't, for example, change on reload.
//
// This function returns the difficulty parameters for each tile on the current
// level, whose difficulty is less than a certain amount.
//
// Random difficulties are used in the few cases where we want repeated maps
// to give different results; scrolls and cards, since they are a finite
// resource.
static const FixedArray<uint8_t, GXM, GYM>& _tile_difficulties(bool random)
{
    // We will often be called with the same level parameter and cutoff, so
    // cache this (DS with passive mapping autoexploring could be 5000 calls
    // in a second or so).
    static FixedArray<uint8_t, GXM, GYM> cache;
    static int cache_seed = -1;

    if (random)
    {
        cache_seed = -1;
        for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
            for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
                cache[x][y] = random2(100);
        return cache;
    }

    // must not produce the magic value (-1)
    int seed = ((static_cast<int>(you.where_are_you) << 8) + you.depth)
             ^ (you.game_seed & 0x7fffffff);

    if (seed == cache_seed)
        return cache;

    cache_seed = seed;

    for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
            cache[x][y] = hash_with_seed(100, seed, y * GXM + x);

    return cache;
}

static colour_t _feat_default_map_colour(dungeon_feature_type feat)
{
    if (player_in_branch(BRANCH_SEWER) && feat_is_water(feat))
        return feat == DNGN_DEEP_WATER ? GREEN : LIGHTGREEN;
    return BLACK;
}

// Returns true if it succeeded.
bool magic_mapping(int map_radius, int proportion, bool suppress_msg,
                   bool force, bool deterministic, bool full_info,
                   bool range_falloff, coord_def origin)
{
    if (!force && !is_map_persistent())
    {
        if (!suppress_msg)
            canned_msg(MSG_DISORIENTED);

        return false;
    }

    if (map_radius < 5)
        map_radius = 5;

    // now gradually weaker with distance:
    const int pfar     = map_radius * 7 / 10;
    const int very_far = map_radius * 9 / 10;

    bool did_map = false;
    int  num_altars        = 0;
    int  num_shops_portals = 0;

    const FixedArray<uint8_t, GXM, GYM>& difficulty =
        _tile_difficulties(!deterministic);

    for (radius_iterator ri(in_bounds(origin) ? origin : you.pos(),
                            map_radius, C_SQUARE);
         ri; ++ri)
    {
        coord_def pos = *ri;
        if (range_falloff)
        {
            int threshold = proportion;

            const int dist = grid_distance(you.pos(), pos);

            if (dist > very_far)
                threshold = threshold / 3;
            else if (dist > pfar)
                threshold = threshold * 2 / 3;

            if (difficulty(pos) > threshold)
                continue;
        }

        map_cell& knowledge = env.map_knowledge(pos);

        if (knowledge.changed())
        {
            // If the player has already seen the square, update map
            // knowledge with the new terrain. Otherwise clear what we had
            // before.
            if (knowledge.seen())
            {
                dungeon_feature_type newfeat = env.grid(pos);
                trap_type tr = feat_is_trap(newfeat) ? get_trap_type(pos) : TRAP_UNASSIGNED;
                knowledge.set_feature(newfeat, env.grid_colours(pos), tr);
            }
            else
                knowledge.clear();
        }

        // Don't assume that DNGN_UNSEEN cells ever count as mapped.
        // Because of a bug at one point in map forgetting, cells could
        // spuriously get marked as mapped even when they were completely
        // unseen.
        const bool already_mapped = knowledge.mapped()
                            && knowledge.feat() != DNGN_UNSEEN;

        if (!full_info && (knowledge.seen() || already_mapped))
            continue;

        const dungeon_feature_type feat = env.grid(pos);

        bool open = true;

        if (feat_is_solid(feat) && !feat_is_closed_door(feat))
        {
            open = false;
            for (adjacent_iterator ai(pos); ai; ++ai)
            {
                if (map_bounds(*ai) && (!feat_is_opaque(env.grid(*ai))
                                        || feat_is_closed_door(env.grid(*ai))))
                {
                    open = true;
                    break;
                }
            }
        }

        if (open)
        {
            if (full_info)
            {
                knowledge.set_feature(feat, _feat_default_map_colour(feat),
                    feat_is_trap(env.grid(pos)) ? get_trap_type(pos)
                                           : TRAP_UNASSIGNED);
            }
            else if (!knowledge.feat())
            {
                auto base_feat = magic_map_base_feat(feat);
                auto colour = _feat_default_map_colour(base_feat);
                auto trap = feat_is_trap(env.grid(pos)) ? get_trap_type(pos)
                                                   : TRAP_UNASSIGNED;
                knowledge.set_feature(base_feat, colour, trap);
            }
            if (emphasise(pos))
                knowledge.flags |= MAP_EMPHASIZE;

            if (full_info)
            {
                if (is_notable_terrain(feat))
                    seen_notable_thing(feat, pos);

                set_terrain_seen(pos);
                StashTrack.add_stash(pos);
                show_update_at(pos);
#ifdef USE_TILE
                tile_wizmap_terrain(pos);
#endif
            }
            else
            {
                set_terrain_mapped(pos);

                if (get_cell_map_feature(knowledge) == MF_STAIR_BRANCH)
                    seen_notable_thing(feat, pos);

                if (get_feature_dchar(feat) == DCHAR_ALTAR)
                    num_altars++;
                else if (get_feature_dchar(feat) == DCHAR_ARCH)
                    num_shops_portals++;
            }

            did_map = true;
        }
    }

    if (!suppress_msg)
    {
        if (did_map)
            mpr("You feel aware of your surroundings.");
        else
            canned_msg(MSG_DISORIENTED);

        vector<string> sensed;

        if (num_altars > 0)
        {
            sensed.push_back(make_stringf("%d altar%s", num_altars,
                                          num_altars > 1 ? "s" : ""));
        }

        if (num_shops_portals > 0)
        {
            const char* plur = num_shops_portals > 1 ? "s" : "";
            sensed.push_back(make_stringf("%d shop%s/portal%s",
                                          num_shops_portals, plur, plur));
        }

        if (!sensed.empty())
            mpr_comma_separated_list("You sensed ", sensed);
    }

    return did_map;
}

bool mon_enemies_around(const monster* mons)
{
    // If the monster has a foe, return true.
    if (mons->foe != MHITNOT && mons->foe != MHITYOU)
        return true;

    if (crawl_state.game_is_arena())
    {
        // If the arena-mode code in _handle_behaviour() hasn't set a foe then
        // we don't have one.
        return false;
    }
    else if (mons->wont_attack())
    {
        // Additionally, if an ally is nearby and *you* have a foe,
        // consider it as the ally's enemy too.
        return you.can_see(*mons) && there_are_monsters_nearby(true);
    }
    else
    {
        // For hostile monster* you* are the main enemy.
        return mons->can_see(you);
    }
}

// Returns a string containing a representation of the map. Leading and
// trailing spaces are trimmed from each line. Leading and trailing empty
// lines are also snipped.
string screenshot()
{
    vector<string> lines(crawl_view.viewsz.y);
    unsigned int lsp = GXM;
    for (int y = 0; y < crawl_view.viewsz.y; y++)
    {
        string line;
        for (int x = 0; x < crawl_view.viewsz.x; x++)
        {
            // in grid coords
            const coord_def gc = view2grid(crawl_view.viewp +
                                     coord_def(x, y));
            char32_t ch =
                  (!map_bounds(gc))             ? ' ' :
                  (gc == you.pos())             ? mons_char(you.symbol)
                                                : get_cell_glyph(gc).ch;
            line += stringize_glyph(ch);
        }
        // right-trim the line
        for (int x = line.length() - 1; x >= 0; x--)
            if (line[x] == ' ')
                line.erase(x);
            else
                break;
        // see how much it can be left-trimmed
        for (unsigned int x = 0; x < line.length(); x++)
            if (line[x] != ' ')
            {
                if (lsp > x)
                    lsp = x;
                break;
            }
        lines[y] = line;
    }

    for (string &line : lines)
        line.erase(0, lsp);     // actually trim from the left
    while (!lines.empty() && lines.back().empty())
        lines.pop_back();       // then from the bottom

    ostringstream ss;
    unsigned int y = 0;
    for (y = 0; y < lines.size() && lines[y].empty(); y++)
        ;                       // ... and from the top
    for (; y < lines.size(); y++)
        ss << lines[y] << "\n";
    return ss.str();
}

int viewmap_flash_colour()
{
    // This only shows the fullscreen colour when we're showing all layers,
    // and hides it for parseability's sake when toggling individual layers.
    if ((_layers & LAYERS_ALL) != LAYERS_ALL)
        return BLACK;

    if (you.berserk())
        return RED;
    else if (you.paralysed())
        return LIGHTBLUE;
    else if (you.petrified())
        return LIGHTGRAY;
    else if (you.duration[DUR_VEXED])
        return MAGENTA;

    return BLACK;
}

// Updates one square of the view area. Should only be called for square
// in LOS.
void view_update_at(const coord_def &pos)
{
    if (pos == you.pos())
        return;

    show_update_at(pos);
#ifdef USE_TILE
    tile_draw_map_cell(pos, true);
#endif
#ifdef USE_TILE_WEB
    tiles.mark_for_redraw(pos);
#endif

#ifndef USE_TILE_LOCAL
    if (!env.map_knowledge(pos).visible())
        return;
    cglyph_t g = get_cell_glyph(pos);

    int flash_colour = you.flash_colour == BLACK
        ? viewmap_flash_colour()
        : you.flash_colour;
    monster_type mons = env.map_knowledge(pos).monster();
    int cell_colour =
        flash_colour &&
        (mons == MONS_NO_MONSTER || mons_class_is_firewood(mons))
            ? real_colour(flash_colour)
            : g.col;

    const coord_def vp = grid2view(pos);
    // Don't draw off-screen.
    if (crawl_view.in_viewport_v(vp))
    {
        cgotoxy(vp.x, vp.y, GOTO_DNGN);
        put_colour_ch(cell_colour, g.ch);
    }

    // Force colour back to normal, else clrscr() will flood screen
    // with this colour on DOS.
    textcolour(LIGHTGREY);

#endif
#ifdef USE_TILE_WEB
    tiles.mark_for_redraw(pos);
#endif
}

bool view_update()
{
    if (you.num_turns > you.last_view_update)
    {
        viewwindow();
        update_screen();
        return true;
    }
    return false;
}

void animation_delay(unsigned int ms, bool do_refresh)
{
    // this leaves any Options.use_animations & UA_BEAM checks to the caller;
    // but maybe it could be refactored into here
    if (do_refresh)
    {
        viewwindow(false);
        update_screen();
    }
    scaled_delay(ms);
}

void flash_view(use_animation_type a, colour_t colour, targeter *where)
{
    if (crawl_state.need_save && Options.use_animations & a)
    {
#ifndef USE_TILE_LOCAL
        save_cursor_pos save;
#endif

        you.flash_colour = colour;
        you.flash_where = where;
        viewwindow(false);
        update_screen();
    }
}

void flash_view_delay(use_animation_type a, colour_t colour, int flash_delay,
                      targeter *where)
{
    if (crawl_state.need_save && Options.use_animations & a)
    {
        flash_view(a, colour, where);
        scaled_delay(flash_delay);
        flash_view(a, 0);
    }
}

static void _do_explore_healing()
{
    // Full heal in, on average, 420 tiles. (270 for MP.)
    const int healing = div_rand_round(random2(you.hp_max), 210);
    inc_hp(healing);
    const int mp = div_rand_round(random2(you.max_magic_points), 135);
    inc_mp(mp);
}

enum class update_flag
{
    affect_excludes = (1 << 0),
    added_exclude   = (1 << 1),
};
DEF_BITFIELD(update_flags, update_flag);

// Do various updates when the player sees a cell. Returns whether
// exclusion LOS might have been affected.
static update_flags player_view_update_at(const coord_def &gc)
{
    maybe_remove_autoexclusion(gc);
    update_flags ret;

    // Set excludes in a radius of 1 around harmful clouds generated
    // by neither monsters nor the player.
    const cloud_struct* cloud = cloud_at(gc);
    if (cloud && !crawl_state.game_is_arena())
    {
        const cloud_struct &cl = *cloud;

        bool did_exclude = false;
        if (!cl.temporary() && cloud_damages_over_time(cl.type, false))
        {
            int size = cl.exclusion_radius();

            // Steam clouds are less dangerous than the other ones,
            // so don't exclude the neighbour cells.
            if (cl.type == CLOUD_STEAM && size == 1)
                size = 0;

            bool was_exclusion = is_exclude_root(gc);
            set_exclude(gc, size, true, false, true);
            if (!did_exclude && !was_exclusion)
                ret |= update_flag::added_exclude;
        }
    }

    // Print hints mode messages for features in LOS.
    if (crawl_state.game_is_hints())
        hints_observe_cell(gc);

    if (env.map_knowledge(gc).changed() || !env.map_knowledge(gc).seen())
        ret |= update_flag::affect_excludes;

    set_terrain_visible(gc);

    if (!(env.pgrid(gc) & FPROP_SEEN_OR_NOEXP))
    {
        if (!crawl_state.game_is_arena()
            && !(branches[you.where_are_you].branch_flags & brflag::fully_map)
            && you.has_mutation(MUT_EXPLORE_REGEN))
        {
            _do_explore_healing();
        }
        if (!crawl_state.game_is_arena()
            && cell_triggers_conduct(gc)
            && !(branches[you.where_are_you].branch_flags & brflag::fully_map)
            && !(player_in_branch(BRANCH_SLIME) && you_worship(GOD_JIYVA)))
        {
            did_god_conduct(DID_EXPLORATION, 2500);
            const int density = env.density ? env.density : 2000;
            you.exploration += div_rand_round(1<<16, density);
            roll_trap_effects();
        }
        env.pgrid(gc) |= FPROP_SEEN_OR_NOEXP;
    }

#ifdef USE_TILE
    const coord_def ep = grid2show(gc);

    // We remove any references to mcache when
    // writing to the background.
    tile_env.bk_fg(gc) = tile_env.fg(ep);
    tile_env.bk_bg(gc) = tile_env.bg(ep);
    tile_env.bk_cloud(gc) = tile_env.cloud(ep);
#endif

    return ret;
}

static void player_view_update()
{
    if (crawl_state.game_is_arena())
    {
        for (rectangle_iterator ri(crawl_view.vgrdc, LOS_MAX_RANGE); ri; ++ri)
            player_view_update_at(*ri);
        // no need to do excludes on the arena
        return;
    }

    vector<coord_def> update_excludes;
    bool need_update = false;

    for (vision_iterator ri(you); ri; ++ri)
    {
        update_flags flags = player_view_update_at(*ri);
        if (flags & update_flag::affect_excludes)
            update_excludes.push_back(*ri);
        if (flags & update_flag::added_exclude)
            need_update = true;
    }
    // Update exclusion LOS for possibly affected excludes.
    update_exclusion_los(update_excludes);
    // Catch up on deferred updates for cloud excludes.
    if (need_update)
        deferred_exclude_update();
}

static void _draw_out_of_bounds(screen_cell_t *cell)
{
    cell->glyph  = ' ';
    cell->colour = DARKGREY;
#ifdef USE_TILE
    cell->tile.fg = 0;
    cell->tile.bg = tileidx_out_of_bounds(you.where_are_you);
#endif
}

static void _draw_outside_los(screen_cell_t *cell, const coord_def &gc,
                                    const coord_def &ep)
{
    // Outside the env.show area.
    cglyph_t g = get_cell_glyph(gc);
    cell->glyph  = g.ch;
    cell->colour = g.col;

#ifdef USE_TILE
    // this is just for out-of-los rays, but I don't see a more efficient way..
    if (in_bounds(gc))
        cell->tile.bg = tile_env.bg(ep);

    tileidx_out_of_los(&cell->tile.fg, &cell->tile.bg, &cell->tile.cloud, gc);
#else
    UNUSED(ep);
#endif
}

static void _draw_player(screen_cell_t *cell,
                         const coord_def &gc, const coord_def &ep,
                         bool anim_updates)
{
    // Player overrides everything in cell.
    cell->glyph  = mons_char(you.symbol);
    cell->colour = mons_class_colour(you.symbol);
    if (you.swimming())
    {
        if (env.grid(gc) == DNGN_DEEP_WATER)
            cell->colour = BLUE;
        else
            cell->colour = CYAN;
    }
#ifndef USE_TILE_LOCAL
    if (Options.use_fake_player_cursor)
#endif
        cell->colour |= COLFLAG_REVERSE;

    cell->colour = real_colour(cell->colour);

#ifdef USE_TILE
    cell->tile.fg = tile_env.fg(ep) = tileidx_player();
    cell->tile.bg = tile_env.bg(ep);
    cell->tile.cloud = tile_env.cloud(ep);
    cell->tile.icons = status_icons_for_player();
    if (anim_updates)
        tile_apply_animations(cell->tile.bg, &tile_env.flv(gc));
#else
    UNUSED(ep, anim_updates);
#endif
}

static void _draw_los(screen_cell_t *cell,
                      const coord_def &gc, const coord_def &ep,
                      bool anim_updates)
{
    cglyph_t g = get_cell_glyph(gc);
    cell->glyph  = g.ch;
    cell->colour = g.col;

#ifdef USE_TILE
    cell->tile.fg = tile_env.fg(ep);
    cell->tile.bg = tile_env.bg(ep);
    cell->tile.cloud = tile_env.cloud(ep);
    cell->tile.icons = tile_env.icons[ep];
    if (anim_updates)
        tile_apply_animations(cell->tile.bg, &tile_env.flv(gc));
#else
    UNUSED(ep, anim_updates);
#endif
}

class shake_viewport_animation: public animation
{
public:
    shake_viewport_animation() { frames = 5; frame_delay = 40; }

    void init_frame(int /*frame*/) override
    {
        offset = coord_def();
        offset.x = random2(3) - 1;
        offset.y = random2(3) - 1;
    }

    coord_def cell_cb(const coord_def &pos, int &/*colour*/) override
    {
        return pos + offset;
    }

private:
    coord_def offset;
};

class banish_animation: public animation
{
public:
    banish_animation(): remaining(false) { }

    void init_frame(int frame) override
    {
        current_frame = frame;

        if (!frame)
        {
            frames = 10;
            hidden.clear();
            remaining = true;
        }

        if (remaining)
            frames = frame + 2;
        else
            frames = frame;

        remaining = false;
    }

    coord_def cell_cb(const coord_def &pos, int &/*colour*/) override
    {
        if (pos == you.pos())
            return pos;

        if (bool *found = map_find(hidden, pos))
        {
            if (*found)
                return coord_def(-1, -1);
        }

        if (!random2(10 - current_frame))
        {
            hidden.insert(make_pair(pos, true));
            return coord_def(-1, -1);
        }

        remaining = true;
        return pos;
    }

    bool remaining;
    map<coord_def, bool> hidden;
    int current_frame;
};

class orb_animation: public animation
{
public:
    void init_frame(int frame) override
    {
        current_frame = frame;
        range = current_frame > 5
            ? (10 - current_frame)
            : current_frame;
        frame_delay = 3 * (6 - range) * (6 - range);
    }

    coord_def cell_cb(const coord_def &pos, int &colour) override
    {
        const coord_def diff = pos - you.pos();
        const int dist = diff.x * diff.x * 4 / 9 + diff.y * diff.y;
        const int min = range * range;
        const int max = (range + 2) * (range  + 2);
        if (dist >= min && dist < max)
            if (is_tiles())
                colour = MAGENTA;
            else
                colour = DARKGREY;
        else
            colour = 0;

        return pos;
    }

    int range;
    int current_frame;
};

static shake_viewport_animation shake_viewport;
static banish_animation banish;
static orb_animation orb;

static animation *animations[NUM_ANIMATIONS] = {
    &shake_viewport,
    &banish,
    &orb
};

void run_animation(animation_type anim, use_animation_type type, bool cleanup)
{
    if (Options.use_animations & type)
    {
        animation *a = animations[anim];

        viewwindow();
        update_screen();

        for (int i = 0; i < a->frames; ++i)
        {
            a->init_frame(i);
            viewwindow(false, false, a);
            update_screen();
            delay(a->frame_delay);
        }

        if (cleanup)
        {
            viewwindow();
            update_screen();
        }
    }
}

static bool _view_is_updating = false;

crawl_view_buffer view_dungeon(animation *a, bool anim_updates, view_renderer *renderer);

static bool _viewwindow_should_render()
{
    if (you.asleep())
        return false;
    if (mouse_control::current_mode() != MOUSE_MODE_NORMAL)
        return true;
    if (you.running && you.running.is_rest())
        return Options.rest_delay != -1;
    const bool run_dont_draw = you.running && Options.travel_delay < 0
                && (!you.running.is_explore() || Options.explore_delay < 0);
    return !run_dont_draw;
}

/**
 * Draws the main window using the character set returned
 * by get_show_glyph().
 *
 * @param show_updates if true, env.show and dependent structures
 *                     are updated. Should be set if anything in
 *                     view has changed.
 * @param tiles_only if true, only the tile view will be updated. This
 *                   is only relevant for Webtiles.
 * @param a[in] the animation to be showing, if any.
 * @param renderer[in] A view renderer used to inject extra visual elements.
 */
void viewwindow(bool show_updates, bool tiles_only, animation *a, view_renderer *renderer)
{
    if (_view_is_updating)
    {
        // recursive calls to this function can lead to memory corruption or
        // crashes, depending on the circumstance, because some functions
        // called from here (e.g. show_init) will wipe out a whole bunch of
        // map data that will still be lurking around higher on the call stack
        // as references. Because the call paths are so complicated, it's hard
        // to find a principled / non-brute-force way of preventing recursion
        // here -- though it's still better to prevent it by other means if
        // possible.
        dprf("Recursive viewwindow call attempted!");
        return;
    }

    {
        unwind_bool updating(_view_is_updating, true);

#ifndef USE_TILE_LOCAL
        save_cursor_pos save;

        if (crawl_state.smallterm)
        {
            smallterm_warning();
            update_screen();
            return;
        }
#endif

        // The player could be at (0,0) if we are called during level-gen; this can
        // happen via mpr -> interrupt_activity -> stop_delay -> runrest::stop
        if (you.duration[DUR_TIME_STEP] || you.pos().origin())
            return;

        // Update the animation of cells only once per turn.
        const bool anim_updates = (you.last_view_update != you.num_turns);

        if (anim_updates)
            you.frame_no++;

#ifdef USE_TILE
        tiles.clear_text_tags(TAG_NAMED_MONSTER);

        if (show_updates)
            mcache.clear_nonref();
#endif

        if (show_updates || _layers != LAYERS_ALL)
        {
            if (!is_map_persistent())
                ash_detect_portals(false);

            // TODO: why on earth is this called from here? It seems like it
            // should be called directly on changing location, or something
            // like that...
            if (you.on_current_level)
                show_init(_layers);

#ifdef USE_TILE
            tile_draw_floor();
            tile_draw_map_cells();
#endif
            view_clear_overlays();
        }

        if (show_updates)
            player_view_update();

        if (_viewwindow_should_render())
        {
            const auto vbuf = view_dungeon(a, anim_updates, renderer);

            you.last_view_update = you.num_turns;
#ifndef USE_TILE_LOCAL
            if (!tiles_only)
            {
                puttext(crawl_view.viewp.x, crawl_view.viewp.y, vbuf);
                update_monster_pane();
            }
#else
            UNUSED(tiles_only);
#endif
#ifdef USE_TILE
            tiles.set_need_redraw(you.running ? Options.tile_runrest_rate : 0);
            tiles.load_dungeon(vbuf, crawl_view.vgrdc);
            tiles.update_tabs();
#endif

            // Leaving it this way because short flashes can occur in long ones,
            // and this simply works without requiring a stack.
            you.flash_colour = BLACK;
            you.flash_where = 0;
        }

        // Reset env.show if we munged it.
        if (_layers != LAYERS_ALL)
            show_init();
    }
}

#ifdef USE_TILE
struct tile_overlay
{
    coord_def gc;
    tileidx_t tile;
};
static vector<tile_overlay> tile_overlays;
static unsigned int tile_overlay_i;

void view_add_tile_overlay(const coord_def &gc, tileidx_t tile)
{
    tile_overlays.push_back({gc, tile});
}
#endif

struct glyph_overlay
{
    coord_def gc;
    cglyph_t glyph;
};
static vector<glyph_overlay> glyph_overlays;
static unsigned int glyph_overlay_i;

void view_add_glyph_overlay(const coord_def &gc, cglyph_t glyph)
{
    glyph_overlays.push_back({gc, glyph});
}

// Simple helper function to reduce duplication with repeatedly used animation code
void flash_tile(coord_def p, colour_t colour, int delay, tileidx_t tile)
{
    if (!(Options.use_animations & UA_BEAM))
        return;

    if (!in_los_bounds_v(grid2view(p)))
        return;

#ifdef USE_TILE
        // Use a bolt tile if one is specified. Otherwise, just use the default
        // for the colour provied.
        if (tile > 0)
            view_add_tile_overlay(p, vary_bolt_tile(tile));
        else
            view_add_tile_overlay(p, tileidx_zap(colour));
#else
        UNUSED(tile);
#endif
        view_add_glyph_overlay(p, {dchar_glyph(DCHAR_FIRED_ZAP),
                                   static_cast<unsigned short>(colour)});

    if (delay > 0)
        animation_delay(delay, true);
}

void view_clear_overlays()
{
#ifdef USE_TILE
    tile_overlays.clear();
#endif
    glyph_overlays.clear();
}

void draw_ring_animation(const coord_def& center, int radius, colour_t colour,
                         colour_t colour_alt, int delay)
{
    for (int i = radius; i >= 0; --i)
    {
        for (distance_iterator di(center, false, false, i); di; ++di)
        {
            if (grid_distance(center, *di) == i && !feat_is_solid(env.grid(*di))
                && you.see_cell_no_trans(*di))
            {
                colour_t draw_colour = colour_alt != BLACK ? coinflip() ? colour
                                                                        : colour_alt
                                                           : colour;
                flash_tile(*di, draw_colour, 0);
            }
        }

        animation_delay(delay, true);
        view_clear_overlays();
    }
}

/**
 * Comparison function for coord_defs that orders coords based on the ordering
 * used by rectangle_iterator.
 */
static bool _coord_def_cmp(const coord_def& l, const coord_def& r)
{
    return l.y < r.y || (l.y == r.y && l.x < r.x);
}

static void _sort_overlays()
{
    /* Stable sort is needed so that we don't swap draw order within cells. */
#ifdef USE_TILE
    stable_sort(begin(tile_overlays), end(tile_overlays),
                [](const tile_overlay &left, const tile_overlay &right) {
                    return _coord_def_cmp(left.gc, right.gc);
                });
    tile_overlay_i = 0;
#endif
    stable_sort(begin(glyph_overlays), end(glyph_overlays),
                [](const glyph_overlay &left, const glyph_overlay &right) {
                    return _coord_def_cmp(left.gc, right.gc);
                });
    glyph_overlay_i = 0;
}

static void add_overlays(const coord_def& gc, screen_cell_t* cell)
{
#ifdef USE_TILE
    while (tile_overlay_i < tile_overlays.size()
           && _coord_def_cmp(tile_overlays[tile_overlay_i].gc, gc))
    {
        tile_overlay_i++;
    }
    while (tile_overlay_i < tile_overlays.size()
           && tile_overlays[tile_overlay_i].gc == gc)
    {
        const auto &overlay = tile_overlays[tile_overlay_i];
        cell->tile.add_overlay(overlay.tile);
        tile_overlay_i++;
    }
#endif
    while (glyph_overlay_i < glyph_overlays.size()
           && _coord_def_cmp(glyph_overlays[glyph_overlay_i].gc, gc))
    {
        glyph_overlay_i++;
    }
    while (glyph_overlay_i < glyph_overlays.size()
           && glyph_overlays[glyph_overlay_i].gc == gc)
    {
        const auto &overlay = glyph_overlays[glyph_overlay_i];
        cell->glyph = overlay.glyph.ch;
        cell->colour = overlay.glyph.col;
        glyph_overlay_i++;
    }
}

/**
 * Constructs the main dungeon view, rendering it into a new crawl_view_buffer.
 *
 * @param a[in] the animation to be showing, if any.
 * @return A new view buffer with the rendered content.
 */
crawl_view_buffer view_dungeon(animation *a, bool anim_updates, view_renderer *renderer)
{
    crawl_view_buffer vbuf(crawl_view.viewsz);

    screen_cell_t *cell(vbuf);

    cursor_control cs(false);

    _sort_overlays();

    int flash_colour = you.flash_colour;
    if (flash_colour == BLACK)
        flash_colour = viewmap_flash_colour();

    const coord_def tl = coord_def(1, 1);
    const coord_def br = vbuf.size();
    for (rectangle_iterator ri(tl, br); ri; ++ri)
    {
        // in grid coords
        const coord_def gc = a
            ? a->cell_cb(view2grid(*ri), flash_colour)
            : view2grid(*ri);

        if (you.flash_where && you.flash_where->is_affected(gc) <= 0)
            draw_cell(cell, gc, anim_updates, 0);
        else
            draw_cell(cell, gc, anim_updates, flash_colour);

        cell++;
    }

    if (renderer)
        renderer->render(vbuf);

    return vbuf;
}

void draw_cell(screen_cell_t *cell, const coord_def &gc,
               bool anim_updates, int flash_colour)
{
#ifdef USE_TILE
    cell->tile.clear();
#endif
    const coord_def ep = grid2show(gc);

    if (!map_bounds(gc))
        _draw_out_of_bounds(cell);
    else if (!crawl_view.in_los_bounds_g(gc))
        _draw_outside_los(cell, gc, coord_def());
    else if (gc == you.pos() && you.on_current_level
             && _layers & LAYER_PLAYER
             && !crawl_state.game_is_arena()
             && !crawl_state.arena_suspended)
    {
        _draw_player(cell, gc, ep, anim_updates);
    }
    else if (you.see_cell(gc) && you.on_current_level)
        _draw_los(cell, gc, ep, anim_updates);
    else
        _draw_outside_los(cell, gc, ep); // in los bounds but not visible

#ifdef USE_TILE
    cell->tile.map_knowledge = map_bounds(gc) ? env.map_knowledge(gc) : map_cell();
    cell->flash_colour = BLACK;
    cell->flash_alpha = 0;
#endif

    // Don't hide important information by recolouring monsters.
    bool allow_mon_recolour = query_map_knowledge(true, gc, [](const map_cell& m) {
        return m.monster() == MONS_NO_MONSTER || mons_class_is_firewood(m.monster());
    });

    // Is this cell excluded from movement by mesmerise-related statuses?
    // MAP_WITHHELD is set in `show.cc:_update_feat_at`.
    bool mesmerise_excluded = (gc != you.pos() // for fungus form
                               && map_bounds(gc)
                               && you.on_current_level
                               && (env.map_knowledge(gc).flags & MAP_WITHHELD)
                               && !feat_is_solid(env.grid(gc)));

#ifdef USE_TILE
    if (you.duration[DUR_BLIND] && you.see_cell(gc))
    {
        cell->flash_colour = real_colour(you.props[BLIND_COLOUR_KEY].get_int());
        // Using a square curve for the alpha is nicer on the eyes than a straight multiple.
        // The effect of alpha is already disproportionate depending on the flash colour
        // and may need to be revised: for a white flash the effect is already extreme by
        // around alpha 80, but for darker colours it is way dimmer and needs more like 150.
        const int alpha_base = gc.distance_from(you.pos());
        cell->flash_alpha = max(1, alpha_base * alpha_base);
    }
#endif

    // Alter colour if flashing the characters vision.
    if (flash_colour)
    {
        if (!you.see_cell(gc))
            cell->colour = DARKGREY;
        else if (gc != you.pos() && allow_mon_recolour)
            cell->colour = real_colour(flash_colour);
#ifdef USE_TILE
        if (you.see_cell(gc)) {
            cell->flash_colour = real_colour(flash_colour);
            cell->flash_alpha = 0;
        }
#endif
    }
    else if (crawl_state.darken_range)
    {
        if ((crawl_state.darken_range->obeys_mesmerise && mesmerise_excluded)
            || (!crawl_state.darken_range->valid_aim(gc)))
        {
            if (allow_mon_recolour)
                cell->colour = DARKGREY;
#ifdef USE_TILE
            if (you.see_cell(gc))
                cell->tile.bg |= TILE_FLAG_OOR;
#endif
        }
    }
    else if (crawl_state.flash_monsters)
    {
        bool found = gc == you.pos();

        if (!found)
            for (auto mon : *crawl_state.flash_monsters)
            {
                if (gc == mon->pos())
                {
                    found = true;
                    break;
                }
            }

        if (!found)
            cell->colour = DARKGREY;
    }
    else if (mesmerise_excluded) // but no range limits in place
    {
        if (allow_mon_recolour)
            cell->colour = DARKGREY;

#ifdef USE_TILE
        // Only grey out tiles within LOS; out-of-LOS tiles are already
        // darkened.
        if (you.see_cell(gc))
            cell->tile.bg |= TILE_FLAG_OOR;
#endif
    }

#ifdef USE_TILE
    tile_apply_properties(gc, cell->tile);
#endif

    if ((_layers != LAYERS_ALL || Options.always_show_exclusions)
        && you.on_current_level
        && map_bounds(gc)
        && (_layers == LAYERS_NONE
            || gc != you.pos()
               && (env.map_knowledge(gc).monster() == MONS_NO_MONSTER
                   || !you.see_cell(gc)))
        && travel_colour_override(gc))
    {
        if (is_exclude_root(gc))
            cell->colour = Options.tc_excluded;
        else if (is_excluded(gc))
            cell->colour = Options.tc_exclude_circle;
    }

    add_overlays(gc, cell);
}

// Hide view layers. The player can toggle certain layers back on
// and the resulting configuration will be remembered for the
// remainder of the game session.
static void _config_layers_menu()
{
    bool exit = false;

    _layers = _layers_saved;
    crawl_state.viewport_weapons    = !!(_layers & LAYER_MONSTER_WEAPONS);
    crawl_state.viewport_monster_hp = !!(_layers & LAYER_MONSTER_HEALTH);

    msgwin_set_temporary(true);
    while (!exit)
    {
        viewwindow();
        update_screen();
        mprf(MSGCH_PROMPT, "Select layers to display:\n"
                           "<%s>(m)onsters</%s>|"
                           "<%s>(p)layer</%s>|"
                           "<%s>(i)tems</%s>|"
                           "<%s>(c)louds</%s>"
#ifndef USE_TILE_LOCAL
                           "|"
                           "<%s>monster (w)eapons</%s>|"
                           "<%s>monster (h)ealth</%s>"
#endif
                           ,
           _layers & LAYER_MONSTERS        ? "lightgrey" : "darkgrey",
           _layers & LAYER_MONSTERS        ? "lightgrey" : "darkgrey",
           _layers & LAYER_PLAYER          ? "lightgrey" : "darkgrey",
           _layers & LAYER_PLAYER          ? "lightgrey" : "darkgrey",
           _layers & LAYER_ITEMS           ? "lightgrey" : "darkgrey",
           _layers & LAYER_ITEMS           ? "lightgrey" : "darkgrey",
           _layers & LAYER_CLOUDS          ? "lightgrey" : "darkgrey",
           _layers & LAYER_CLOUDS          ? "lightgrey" : "darkgrey"
#ifndef USE_TILE_LOCAL
           ,
           _layers & LAYER_MONSTER_WEAPONS ? "lightgrey" : "darkgrey",
           _layers & LAYER_MONSTER_WEAPONS ? "lightgrey" : "darkgrey",
           _layers & LAYER_MONSTER_HEALTH  ? "lightgrey" : "darkgrey",
           _layers & LAYER_MONSTER_HEALTH  ? "lightgrey" : "darkgrey"
#endif
        );
        mprf(MSGCH_PROMPT, "Press 'a' to toggle all layers. "
                           "Press any other key to exit.");

        switch (get_ch())
        {
        case 'm': _layers_saved = _layers ^= LAYER_MONSTERS;        break;
        case 'p': _layers_saved = _layers ^= LAYER_PLAYER;          break;
        case 'i': _layers_saved = _layers ^= LAYER_ITEMS;           break;
        case 'c': _layers_saved = _layers ^= LAYER_CLOUDS;          break;
#ifndef USE_TILE_LOCAL
        case 'w': _layers_saved = _layers ^= LAYER_MONSTER_WEAPONS;
                  if (_layers & LAYER_MONSTER_WEAPONS)
                      _layers_saved = _layers |= LAYER_MONSTERS;
                  break;
        case 'h': _layers_saved = _layers ^= LAYER_MONSTER_HEALTH;
                  if (_layers & LAYER_MONSTER_HEALTH)
                      _layers_saved = _layers |= LAYER_MONSTERS;
                  break;
#endif
        case 'a': if (_layers)
                      _layers_saved = _layers = LAYERS_NONE;
                  else
                  {
#ifndef USE_TILE_LOCAL
                      _layers_saved = _layers = LAYERS_ALL
                                      | LAYER_MONSTER_WEAPONS
                                      | LAYER_MONSTER_HEALTH;
#else
                      _layers_saved = _layers = LAYERS_ALL;
#endif
                  }
                  break;
        default:
            _layers = LAYERS_ALL;
            crawl_state.viewport_weapons    = !!(_layers & LAYER_MONSTER_WEAPONS);
            crawl_state.viewport_monster_hp = !!(_layers & LAYER_MONSTER_HEALTH);
            exit = true;
            break;
        }

        crawl_state.viewport_weapons    = !!(_layers & LAYER_MONSTER_WEAPONS);
        crawl_state.viewport_monster_hp = !!(_layers & LAYER_MONSTER_HEALTH);

        msgwin_clear_temporary();
    }
    msgwin_set_temporary(false);

    canned_msg(MSG_OK);
}

void toggle_show_terrain()
{
    if (_layers == LAYERS_ALL)
        _config_layers_menu();
    else
        reset_show_terrain();
}

void reset_show_terrain()
{
    if (_layers != LAYERS_ALL)
        mprf(MSGCH_PROMPT, "Restoring view layers.");

    _layers = LAYERS_ALL;
    crawl_state.viewport_weapons    = !!(_layers & LAYER_MONSTER_WEAPONS);
    crawl_state.viewport_monster_hp = !!(_layers & LAYER_MONSTER_HEALTH);
}

////////////////////////////////////////////////////////////////////////////
// Term resize handling (generic).

void handle_terminal_resize()
{
    crawl_state.terminal_resized = false;

    if (crawl_state.terminal_resize_handler)
        (*crawl_state.terminal_resize_handler)();
    else
        crawl_view.init_geometry();

    redraw_screen();
    update_screen();
}
