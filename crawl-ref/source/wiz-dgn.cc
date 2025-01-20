/**
 * @file
 * @brief Dungeon related wizard functions.
**/

#include "AppHdr.h"

#include "wiz-dgn.h"

#include "abyss.h"
#include "act-iter.h"
#include "branch.h"
#include "coordit.h"
#include "dactions.h"
#include "delay.h"
#include "describe.h"
#include "directn.h"
#include "dgn-overview.h"
#include "dungeon.h"
#include "tile-env.h"
#include "files.h"
#include "libutil.h"
#include "maps.h"
#include "message.h"
#include "place.h"
#include "prompt.h"
#include "religion.h"
#include "spl-goditem.h" // detect_items
#include "stairs.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "tileview.h"
#include "tiles-build-specific.h"
#include "traps.h"
#include "view.h"
#include "wiz-mon.h"

#ifdef WIZARD
static dungeon_feature_type _find_appropriate_stairs(bool down)
{
    if (player_in_branch(BRANCH_PANDEMONIUM))
    {
        if (down)
            return DNGN_TRANSIT_PANDEMONIUM;
        else
            return DNGN_EXIT_PANDEMONIUM;
    }

    int depth = you.depth;
    if (down)
        depth++;
    else
        depth--;

    // Can't go down from bottom level of a branch.
    if (depth > brdepth[you.where_are_you])
    {
        mpr("Can't go down from the bottom of a branch.");
        return DNGN_UNSEEN;
    }
    // Going up from top level of branch
    else if (depth == 0)
        return your_branch().exit_stairs;
    // Branch non-edge cases
    else if (depth >= 1)
    {
        if (down)
            return DNGN_STONE_STAIRS_DOWN_I;
        else
            return DNGN_ESCAPE_HATCH_UP;
    }
    else
    {
        mpr("Bug in determining level exit.");
        return DNGN_UNSEEN;
    }
}

void wizard_place_stairs(bool down)
{
    dungeon_feature_type stairs = _find_appropriate_stairs(down);

    if (stairs == DNGN_UNSEEN)
        return;

    mprf("Creating %sstairs.", down ? "down" : "up");
    dungeon_terrain_changed(you.pos(), stairs);
}

static level_id _wizard_level_target = level_id();

void wizard_level_travel(bool down)
{
    dungeon_feature_type stairs = _find_appropriate_stairs(down);

    if (stairs == DNGN_UNSEEN)
        return;

    // This lets us, for example, use &U to exit from Pandemonium and
    // &D to go to the next level.
    command_type real_dir = feat_stair_direction(stairs);
    if (down && real_dir == CMD_GO_UPSTAIRS
        || !down && real_dir == CMD_GO_DOWNSTAIRS)
    {
        down = !down;
    }

    _wizard_level_target = stair_destination(stairs, "", false);

    if (down)
        down_stairs(stairs, false, false);
    else
        up_stairs(stairs, false);

    _wizard_level_target = level_id();
}

bool is_wizard_travel_target(const level_id l)
{
    return _wizard_level_target.is_valid() && l == _wizard_level_target;
}

static void _wizard_go_to_level(const level_pos &pos)
{
    dungeon_feature_type stair_taken =
        absdungeon_depth(pos.id.branch, pos.id.depth) > env.absdepth0 ?
        DNGN_STONE_STAIRS_DOWN_I : DNGN_STONE_STAIRS_UP_I;

    if (pos.id.depth == brdepth[pos.id.branch])
        stair_taken = DNGN_STONE_STAIRS_DOWN_I;

    if (!player_in_branch(pos.id.branch) && pos.id.depth == 1
        && pos.id.branch != BRANCH_DUNGEON)
    {
        stair_taken = branches[pos.id.branch].entry_stairs;
    }

    if (is_connected_branch(pos.id.branch))
        you.level_stack.clear();
    else
    {
        for (int i = you.level_stack.size() - 1; i >= 0; i--)
            if (you.level_stack[i].id == pos.id)
                you.level_stack.resize(i);
        if (!player_in_branch(pos.id.branch))
            you.level_stack.push_back(level_pos::current());
    }

    const level_id old_level = level_id::current();

    you.where_are_you = static_cast<branch_type>(pos.id.branch);
    you.depth         = pos.id.depth;
    _wizard_level_target = pos.id;

    leaving_level_now(stair_taken);
    const bool newlevel = load_level(stair_taken, LOAD_ENTER_LEVEL, old_level);
    tile_new_level(newlevel);
    if (!crawl_state.test)
        save_game_state();
    new_level();
    seen_monsters_react();
    viewwindow();
    update_screen();

    // Tell stash-tracker and travel that we've changed levels.
    trackers_init_new_level();
    _wizard_level_target = level_id();
}

void wizard_interlevel_travel()
{
    string name;
    const level_pos pos =
        prompt_translevel_target(TPF_ALLOW_UPDOWN | TPF_SHOW_ALL_BRANCHES, name);

    if (pos.id.depth < 1 || pos.id.depth > brdepth[pos.id.branch])
    {
        canned_msg(MSG_OK);
        return;
    }

    _wizard_go_to_level(pos);
}

dungeon_feature_type wizard_select_feature(bool mimic, bool allow_fprop)
{
    char specs[256];
    // TODO: this sub-ui is very annoying to use
    if (mimic)
        mprf(MSGCH_PROMPT, "Create what kind of feature mimic? ");
    else
        mprf(MSGCH_PROMPT, "Create which feature? ");

    if (cancellable_get_line_autohist(specs, sizeof(specs)) || specs[0] == 0)
    {
        canned_msg(MSG_OK);
        return DNGN_UNSEEN;
    }

    dungeon_feature_type feat = DNGN_UNSEEN;

    if (int feat_num = atoi(specs))
        feat = static_cast<dungeon_feature_type>(feat_num);
    else
    {
        string name = lowercase_string(specs);
        name = replace_all(name, " ", "_");
        feat = dungeon_feature_by_name(name);
        if (feat == DNGN_UNSEEN) // no exact match
        {
            vector<string> matches = dungeon_feature_matches(name);

            if (matches.empty())
            {
                const feature_property_type fprop(str_to_fprop(name));
                // TODO: fix so that the ability can place fprops
                if (fprop != FPROP_NONE && allow_fprop)
                {
                    env.pgrid(you.pos()) |= fprop;
                    mprf("Set fprops \"%s\" at (%d,%d)",
                         name.c_str(), you.pos().x, you.pos().y);
                }
                else
                {
                    mprf(MSGCH_DIAGNOSTICS, "No features matching '%s'",
                         name.c_str());
                }
                return DNGN_UNSEEN;
            }

            // Only one possible match, use that.
            if (matches.size() == 1)
            {
                name = matches[0];
                feat = dungeon_feature_by_name(name);
            }
            // Multiple matches, list them to wizard
            else
            {
                string prefix = "No exact match for feature '" +
                                name +  "', possible matches are: ";

                // Use mpr_comma_separated_list() because the list
                // might be *LONG*.
                mpr_comma_separated_list(prefix, matches, " and ", ", ",
                                         MSGCH_DIAGNOSTICS);
                // TODO: no recursion
                feat = wizard_select_feature(mimic);
            }
        }
    }

    if (mimic && !feat_is_mimicable(feat, false)
        && !yesno("This isn't a valid feature mimic. Create it anyway? ",
                  true, 'n'))
    {
        feat = DNGN_UNSEEN;
    }
    if (feat == DNGN_UNSEEN)
        canned_msg(MSG_OK);
    return feat;
}

bool wizard_create_feature(const coord_def& pos, dungeon_feature_type feat, bool mimic)
{
    dist t;
    t.target = pos;
    return wizard_create_feature(t, feat, mimic);
}

bool wizard_create_feature(dist &target, dungeon_feature_type feat, bool mimic)
{
    if (feat == DNGN_UNSEEN)
    {
        feat = wizard_select_feature(mimic, target.target == you.pos());
        if (feat == DNGN_UNSEEN)
            return false;
        you.props[WIZ_LAST_FEATURE_TYPE_PROP] = static_cast<int>(feat);
    }

    const bool targeting_mode = target.needs_targeting();

    do
    {
        if (targeting_mode)
        {
            // TODO: should this just toggle xray vision on?
            viewwindow(true); // make sure los is up to date
            direction_chooser_args args;
            args.range = you.wizard_vision ? -1 : LOS_MAX_RANGE;
            args.restricts = DIR_TARGET;
            args.mode = TARG_ANY;
            args.needs_path = false;
            // TODO: a way to switch features while targeting?
            args.top_prompt = make_stringf(
                "Building '<w>%s</w>'.\n"
                "[<w>.</w>] place feature and continue, "
                "[<w>ret</w>] place and exit, [<w>esc</w>] exit.",
                dungeon_feature_name(feat));

            if (in_bounds(target.target))
                args.default_place = target.target; // last placed position
            if (you.wizard_vision)
                args.unrestricted = true; // work with xray vision
            direction(target, args);
            if (target.isCancel || !target.isValid)
                return false;
        }
        coord_def &pos = target.target;

        if (feat == DNGN_ENTER_SHOP)
            return debug_make_shop(pos);

        if (feat_is_trap(feat))
            return debug_make_trap(pos);

        tile_env.flv(pos).feat = 0;
        tile_env.flv(pos).special = 0;
        env.grid_colours(pos) = 0;
        const dungeon_feature_type old_feat = env.grid(pos);
        dungeon_terrain_changed(pos, feat, false, false, false, true);
        // Update gate tiles, if existing.
        if (feat_is_door(old_feat) || feat_is_door(feat))
        {
            const coord_def left  = pos - coord_def(1, 0);
            const coord_def right = pos + coord_def(1, 0);
            if (map_bounds(left) && feat_is_door(env.grid(left)))
                tile_init_flavour(left);
            if (map_bounds(right) && feat_is_door(env.grid(right)))
                tile_init_flavour(right);
        }
        if (pos == you.pos() && cell_is_solid(pos))
            you.wizmode_teleported_into_rock = true;

        if (mimic)
            env.level_map_mask(pos) |= MMT_MIMIC;

        if (you.see_cell(pos))
            view_update_at(pos);
    } while (targeting_mode && target.isEndpoint);

    return true;
}

void wizard_list_branches()
{
    for (branch_iterator it; it; ++it)
    {
        if (parent_branch(it->id) == NUM_BRANCHES)
            continue;
        else if (brentry[it->id].is_valid())
        {
            mprf(MSGCH_DIAGNOSTICS, "Branch %d (%s) is on %s",
                 it->id, it->longname, brentry[it->id].describe().c_str());
        }
        else if (is_random_subbranch(it->id))
        {
            mprf(MSGCH_DIAGNOSTICS, "Branch %d (%s) was not generated "
                 "this game", it->id, it->longname);
        }
    }

    if (!you.props.exists(OVERFLOW_TEMPLES_KEY))
        return;

    mprf(MSGCH_DIAGNOSTICS, "----");
    mprf(MSGCH_DIAGNOSTICS, "Overflow temples: ");

    CrawlVector &levels = you.props[OVERFLOW_TEMPLES_KEY].get_vector();

    vector<string> temple_strings;
    vector<string> god_names;

    for (unsigned int i = 0; i < levels.size(); i++)
    {
        CrawlStoreValue &val = levels[i];

        // Does this level have an overflow temple?
        if (val.get_flags() & SFLAG_UNSET)
            continue;

        CrawlVector &temples = val.get_vector();

        if (temples.empty())
            continue;

        temple_strings.clear();

        for (CrawlHashTable &temple_hash : temples)
        {
            god_names.clear();
            CrawlVector    &gods        = temple_hash[TEMPLE_GODS_KEY];

            for (unsigned int k = 0; k < gods.size(); k++)
            {
                god_type god = (god_type) gods[k].get_byte();

                god_names.push_back(god_name(god));
            }
            temple_strings.push_back(
                comma_separated_line(god_names.begin(), god_names.end()));
        }

        mprf(MSGCH_DIAGNOSTICS, "%u on D:%u (%s)", temples.size(),
             i + 1,
             comma_separated_line(temple_strings.begin(),
                                  temple_strings.end(), "; ", "; ").c_str()
          );
    }
}

void wizard_map_level()
{
    if (!is_map_persistent())
    {
        if (!yesno("Force level to be mappable?", true, 'n'))
        {
            canned_msg(MSG_OK);
            return;
        }
        env.properties[FORCE_MAPPABLE_KEY] = true;
    }

    mpr("Mapping level.");
    magic_mapping(GDM, 100, true, true, false, true, false);

    for (rectangle_iterator ri(BOUNDARY_BORDER - 1); ri; ++ri)
    {
        update_item_at(*ri, true);
        show_update_at(*ri, Layer::ITEMS);

#ifdef USE_TILE
        tiles.update_minimap(*ri);
        tile_draw_map_cell(*ri, true);
#endif
#ifdef USE_TILE_WEB
        tiles.mark_for_redraw(*ri);
#endif
    }
}

bool debug_make_trap(const coord_def& pos)
{
    if (env.grid(pos) != DNGN_FLOOR)
    {
        mpr("You need to be on a floor square to make a trap.");
        return false;
    }

    vector<WizardEntry> options;
    for (int i = TRAP_FIRST_TRAP; i < NUM_TRAPS; ++i)
    {
        auto name = trap_name(static_cast<trap_type>(i));
        options.emplace_back(WizardEntry(name, i));
    }
    sort(options.begin(), options.end());
    options.emplace_back(WizardEntry('*', "any", TRAP_RANDOM));

    auto menu = WizardMenu("Make which kind of trap?", options);
    if (!menu.run(true))
        return false;

    auto trap = static_cast<trap_type>(menu.result());
    place_specific_trap(you.pos(), trap);

    mprf("Created %s.",
         (trap == TRAP_RANDOM)
            ? "a random trap"
            : trap_at(you.pos())->name(DESC_A).c_str());

    if (trap == TRAP_SHAFT && !is_valid_shaft_level())
        mpr("NOTE: Shaft traps aren't valid on this level.");

    return true;
}

bool debug_make_shop(const coord_def& pos)
{
    if (env.grid(pos) != DNGN_FLOOR)
    {
        mpr("Insufficient floor-space for new Wal-Mart.");
        return false;
    }

    vector<WizardEntry> options;
    for (int i = 0; i < NUM_SHOPS; ++i)
    {
        auto name = shoptype_to_str(static_cast<shop_type>(i));
        if (str_to_shoptype(name) == i) // Don't offer to make "removed" shops.
            options.emplace_back(WizardEntry(name, i));
    }
    sort(options.begin(), options.end());
    options.emplace_back(WizardEntry('*', "any", SHOP_RANDOM));

    auto menu = WizardMenu("Make which kind of shop?", options);
    if (!menu.run(true))
        return false;

    place_spec_shop(pos, static_cast<shop_type>(menu.result()));
    mpr("Done.");
    return true;
}

static void _free_all_vaults()
{
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
        env.level_map_ids(*ri) = INVALID_MAP_INDEX;

    for (auto &vp : env.level_vaults)
        vp->seen = false;

    dgn_erase_unused_vault_placements();
}

static void debug_load_map_by_name(string name, bool primary)
{
    if (primary)
        _free_all_vaults();

    const bool place_on_us = !primary && strip_tag(name, "*", true);

    level_clear_vault_memory();
    const map_def *toplace = find_map_by_name(name);
    if (!toplace)
    {
        vector<string> matches = find_map_matches(name);

        if (matches.empty())
        {
            mprf("Can't find map named '%s'.", name.c_str());
            return;
        }
        else if (matches.size() == 1)
        {
            string prompt = "Only match is '";
            prompt += matches[0];
            prompt += "', use that?";
            if (!yesno(prompt.c_str(), true, 'y'))
            {
                canned_msg(MSG_OK);
                return;
            }

            toplace = find_map_by_name(matches[0]);
        }
        else
        {
            string prompt = "No exact matches for '";
            prompt += name;
            prompt += "', possible matches are: ";
            mpr_comma_separated_list(prompt, matches);
            return;
        }
    }

    coord_def where(-1, -1);
    if (place_on_us)
    {
        if (toplace->orient == MAP_FLOAT || toplace->orient == MAP_NONE)
        {
            coord_def size = toplace->map.size();
            coord_def tl   = you.pos() - (size / 2) - coord_def(-1, -1);
            coord_def br   = you.pos() + (size / 2) + coord_def(-1, -1);

            for (rectangle_iterator ri(tl, br); ri; ++ri)
            {
                if (!in_bounds(*ri))
                {
                    mprf("Placing %s on top of you would put part of the "
                         "map outside of the level, cancelling.",
                         toplace->name.c_str());
                    return;
                }
            }
            // We're okay.
            where = you.pos();
        }
        else
        {
            mprf("%s decides where it goes, can't place where you are.",
                 toplace->name.c_str());
        }
    }

    if (primary)
    {
        if (toplace->orient == MAP_ENCOMPASS
            && !toplace->is_usable_in(level_id::current())
            && !toplace->place.is_usable_in(level_id::current())
            && !yesno("Warning: this is an encompass vault not designed "
                       "for this location; placing it with &P may result in "
                       "crashes and save corruption. Continue?", true, 'y'))
        {
            mpr("Ok; try placing with &L or go to the relevant location to "
                "safely place with &P.");
            return;
        }
        if (toplace->is_minivault())
            you.props[FORCE_MINIVAULT_KEY] = toplace->name;
        else
            you.props[FORCE_MAP_KEY] = toplace->name;
        wizard_recreate_level();
        you.props.erase(FORCE_MINIVAULT_KEY);
        you.props.erase(FORCE_MAP_KEY);

        // We just saved with you.props[FORCE_MAP_KEY] set; save again in
        // case we crash (lest we have the property forever).
        if (!crawl_state.test)
            save_game_state();
    }
    else
    {
        unwind_var<string_set> um(you.uniq_map_names, string_set());
        unwind_var<string_set> umt(you.uniq_map_tags, string_set());
        unwind_var<string_set> um_a(you.uniq_map_names_abyss, string_set());
        unwind_var<string_set> umt_a(you.uniq_map_tags_abyss, string_set());
        unwind_var<string_set> lum(env.level_uniq_maps, string_set());
        unwind_var<string_set> lumt(env.level_uniq_map_tags, string_set());
        if (dgn_place_map(toplace, false, false, where))
        {
            mprf("Successfully placed %s.", toplace->name.c_str());
            // Fix up doors from vaults and any changes to the default walls
            // and floors from the vault.
            tile_init_flavour();
            // Transporters would normally be made from map markers by the
            // builder.
            dgn_make_transporters_from_markers();
        }
        else
        {
            mprf("Failed to place %s; last builder error: %s",
                toplace->name.c_str(), crawl_state.last_builder_error.c_str());
        }
    }
}

static input_history mini_hist(10), primary_hist(10);

void debug_place_map(bool primary)
{
    char what_to_make[100];
    clear_messages();
    mprf(MSGCH_PROMPT, primary ? "Enter map name: " :
         "Enter map name (prefix it with * for local placement): ");
    if (cancellable_get_line(what_to_make, sizeof what_to_make,
                            primary ? &primary_hist : &mini_hist))
    {
        canned_msg(MSG_OK);
        return;
    }

    string what = what_to_make;
    trim_string(what);
    if (what.empty())
    {
        canned_msg(MSG_OK);
        return;
    }

    debug_load_map_by_name(what, primary);
}

static void _debug_kill_traps()
{
    for (rectangle_iterator ri(1); ri; ++ri)
        if (feat_is_trap(env.grid(*ri)))
            destroy_trap(*ri);
}

static int _debug_time_explore()
{
    viewwindow();
    update_screen();
    start_explore(false);

    unwind_var<int> es(Options.explore_stop, 0);

    const int start = you.num_turns;
    while (you_are_delayed())
    {
        you.turn_is_over = false;
        handle_delay();
        you.num_turns++;
    }

    // Elapsed time might not match up if explore had to go through
    // shallow water.
    PlaceInfo& pi = you.get_place_info();
    pi.elapsed_total = (pi.elapsed_explore + pi.elapsed_travel +
                        pi.elapsed_interlevel + pi.elapsed_resting +
                        pi.elapsed_other);

    PlaceInfo& pi2 = you.global_info;
    pi2.elapsed_total = (pi2.elapsed_explore + pi2.elapsed_travel +
                         pi2.elapsed_interlevel + pi2.elapsed_resting +
                         pi2.elapsed_other);

    return you.num_turns - start;
}

static void _debug_destroy_doors()
{
    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
        {
            const dungeon_feature_type feat = env.grid[x][y];
            if (feat_is_closed_door(feat))
                env.grid[x][y] = DNGN_FLOOR;
        }
}

// Turns off greedy explore, then:
// a) Destroys all traps on the level.
// b) Kills all monsters on the level.
// c) Suppresses monster generation.
// d) Converts all closed doors to floor.
// e) Forgets map.
// f) Counts number of turns needed to explore the level.
void debug_test_explore()
{
    wizard_dismiss_all_monsters(true);
    _debug_kill_traps();
    _debug_destroy_doors();

    forget_map();

    // Remember where we are now.
    const coord_def where = you.pos();

    const int explore_turns = _debug_time_explore();

    // Return to starting point.
    you.moveto(where);

    mprf("Explore took %d turns.", explore_turns);
}

void wizard_list_levels()
{
    if (!you.level_stack.empty())
    {
        mpr("Level stack:");
        for (unsigned int i = 0; i < you.level_stack.size(); i++)
        {
            mprf(MSGCH_DIAGNOSTICS, i+1, // inhibit merging
                 "%-10s (%d,%d)", you.level_stack[i].id.describe().c_str(),
                 you.level_stack[i].pos.x, you.level_stack[i].pos.y);
        }
    }

    travel_cache.update_daction_counters();

    vector<level_id> levs = travel_cache.known_levels();

    mpr("Known levels:");
    for (unsigned int i = 0; i < levs.size(); i++)
    {
        const LevelInfo* lv = travel_cache.find_level_info(levs[i]);
        ASSERT(lv);

        string cnts;
        for (int j = 0; j < NUM_DACTION_COUNTERS; j++)
            cnts += make_stringf("%d/", lv->daction_counters[j]);

        mprf(MSGCH_DIAGNOSTICS, i+1, // inhibit merging
             "%-10s : %s", levs[i].describe().c_str(), cnts.c_str());
    }

    string cnts = "";
    for (int j = 0; j < NUM_DACTION_COUNTERS; j++)
        cnts += make_stringf("%d/", query_daction_counter((daction_type)j));

    mprf("%-10s : %s", "`- total", cnts.c_str());
}

void wizard_recreate_level()
{
    mpr("Regenerating level.");

    // Need to allow reuse of vaults, otherwise we'd run out of them fast.
    #ifndef DEBUG_VETO_RESUME
    _free_all_vaults();
    #endif

    for (monster_iterator mi; mi; ++mi)
    {
        if (mons_is_unique(mi->type))
        {
            remove_unique_annotation(*mi);
            you.unique_creatures.set(mi->type, false);
            mi->flags |= MF_TAKING_STAIRS; // no Abyss transit
        }
    }

    level_id lev = level_id::current();
    _wizard_level_target = lev;
    dungeon_feature_type stair_taken = DNGN_STONE_STAIRS_DOWN_I;

    if (lev.branch == BRANCH_ABYSS)
    {
        // abyss generation is different, and regenerating the abyss
        // with the below code leads to strange behavior
        abyss_teleport(true);
        return;
    }

    if (lev.depth == 1 && lev != BRANCH_DUNGEON)
        stair_taken = branches[lev.branch].entry_stairs;

    leaving_level_now(stair_taken);
    delete_level(lev);
    const bool newlevel = load_level(stair_taken, LOAD_START_GAME, lev);
    if (you.get_place_info().levels_seen > 1)
        you.get_place_info().levels_seen--; // this getting to 0 -> crashes

    tile_new_level(newlevel);
    if (!crawl_state.test)
        save_game_state();
    new_level();
    seen_monsters_react();
    viewwindow();
    update_screen();

    trackers_init_new_level();
}

void wizard_clear_used_vaults()
{
    you.uniq_map_tags.clear();
    you.uniq_map_names.clear();
    you.uniq_map_tags_abyss.clear();
    you.uniq_map_names_abyss.clear();
    env.level_uniq_maps.clear();
    env.level_uniq_map_tags.clear();
    mpr("All vaults are now eligible for [re]use.");
}

void wizard_abyss_speed()
{
    char specs[256];
    mprf(MSGCH_PROMPT, "Set Abyss speed to what? (now %d, higher value = "
                       "higher speed) ", you.abyss_speed);

    if (!cancellable_get_line(specs, sizeof(specs)))
    {
        const int speed = atoi(specs);
        if (speed || specs[0] == '0')
        {
            mprf("Setting Abyss speed to %i.", speed);
            you.abyss_speed = speed;
            return;
        }
    }

    canned_msg(MSG_OK);
}
#endif
