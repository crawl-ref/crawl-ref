/**
 * @file
 * @brief Simple reading of an init file and system variables
 * @detailed read_init_file is the main function, but read_option_line does
 * most of the work though. Read through read_init_file to get an overview of
 * how Crawl loads options. This file also contains a large number of utility
 * functions for setting particular options and converting between human
 * readable strings and internal values. (E.g. _weapon_to_str). There is also
 * some code dealing with sorting menus.
**/

#include "AppHdr.h"

#include "initfile.h"

#include "json.h"
#include "json-wrapper.h"

#include <algorithm>
#include <cinttypes>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <set>
#include <string>

#include "ability.h"
#include "branch-data-json.h"
#include "chardump.h"
#include "clua.h"
#include "colour.h"
#include "defines.h"
#include "delay.h"
#include "describe.h"
#include "directn.h"
#include "dlua.h"
#include "end.h"
#include "errors.h"
#include "explore-greedy-options.h"
#include "explore-stop-options.h"
#include "files.h"
#include "game-options.h"
#include "ghost.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "jobs.h"
#include "kills.h"
#include "libutil.h"
#include "macro.h"
#include "mapdef.h"
#include "maps.h"
#include "message.h"
#include "mon-util.h"
#include "monster.h"
#include "newgame.h"
#include "options.h"
#include "playable.h"
#include "player.h"
#include "prompt.h"
#include "slot-select-mode.h"
#include "species.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "stringutil.h"
#include "syscalls.h"
#include "tags.h"
#include "tag-version.h"
#include "throw.h"
#include "travel.h"
#include "unwind.h"
#include "version.h"
#include "viewchar.h"
#include "view.h"
#include "wizard-option-type.h"
#ifdef USE_TILE
#include "tilepick.h"
#include "rltiles/tiledef-player.h"
#endif
#include "tiles-build-specific.h"

// For finding the executable's path
#ifdef TARGET_OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#elif defined(TARGET_OS_MACOSX)
extern char **NXArgv;
#ifndef DATA_DIR_PATH
#include <unistd.h>
#endif
#elif defined(UNIX) || defined(TARGET_COMPILER_MINGW)
#include <unistd.h>
#endif

#ifdef __ANDROID__
#include "SDL_system.h"
#endif

#ifdef __HAIKU__
#include <FindDirectory.h>
#endif

const string game_options::interrupt_prefix = "interrupt_";
system_environment SysEnv;

// TODO:
// because reset_options is called in the constructor, it's a magnet for
// static initialization order issues. We should wrap this in a function per
// https://isocpp.org/wiki/faq/ctors#construct-on-first-use-v2
game_options Options;

game_options &get_default_options()
{
    static game_options default_options;
    return default_options;
}

static string _get_save_path(string subdir);
static bool _set_crawl_dir(const string &d);
static string _resolve_dir(string path, string suffix);
static string _supported_language_listing();

static bool _force_allow_wizard();
static bool _force_allow_explore();

static species_type _str_to_species(const string &str);
static sound_mapping _interrupt_sound_mapping(const string &s);
static pair<text_pattern,string> _slot_mapping(const string &s);

#ifdef USE_TILE
static tag_pref _str_to_tag_pref(const string &opt)
{
    static vector<string> tag_prefs = { "none", "tutorial", "named", "enemy", "auto", "all" };
    ASSERT(tag_prefs.size() == TAGPREF_MAX);
    for (int i = 0; i < TAGPREF_MAX; i++)
    {
        if (opt == tag_prefs[i])
            return (tag_pref)i;
    }
    return TAGPREF_ENEMY;
}
#endif

static monster_list_colour_type _str_to_mlc(const string &s)
{
    static const char * const _monster_list_colour_names[NUM_MLC] =
    {
        "friendly", "neutral", "good_neutral",
        "trivial", "easy", "tough", "nasty", "unusual",
    };
    for (int i = 0; i < NUM_MLC; ++i)
        if (s == _monster_list_colour_names[i])
            return static_cast<monster_list_colour_type>(i);
    return NUM_MLC;
}

mlc_mapping::mlc_mapping(const string &s)
{
    vector<string> thesplit = split_string(":", s, true, true, 1);
    const int scolour = thesplit.size() == 1 ? -1 : str_to_colour(thesplit[1]);

    // let -1 for "" through
    if ((scolour >= 16 || scolour < 0) && (thesplit.size() > 1 && !thesplit[1].empty()))
    {
        mprf(MSGCH_ERROR, "Options error: Bad monster_list_colour: '%s'",
            thesplit[1].c_str());
        return;
    }
    colour = scolour;
    category = _str_to_mlc(thesplit[0]);
    if (category == NUM_MLC)
    {
        mprf(MSGCH_ERROR, "Bad monster_list_colour key: %s\n",
                     thesplit[0].c_str());
    }
}

#ifdef USE_TILE
colour_remapping::colour_remapping(const string &s)
{
    vector<string> thesplit = split_string(":", s, true, true, 1);
    const int scolour = thesplit.size() == 1 ? -1 : str_to_colour(thesplit[0]);

    // let -1 for "" through
    if ((scolour >= 16 || scolour < 0) && (thesplit.size() > 1 && !thesplit[0].empty()))
    {
        mprf(MSGCH_ERROR, "Options error: Bad custom_text_colours key: '%s'",
            thesplit[1].c_str());
        return;
    }

    colour_index = scolour;
    colour_def = str_to_tile_colour(thesplit[1]);
    if (colour_index == NUM_TERM_COLOURS)
    {
        mprf(MSGCH_ERROR, "Bad custom_text_colours: %s\n",
                     thesplit[0].c_str());
    }
}
#endif

static bool _first_less(const pair<int, int> &l, const pair<int, int> &r)
{
    return l.first < r.first;
}

static bool _first_greater(const pair<int, int> &l, const pair<int, int> &r)
{
    return l.first > r.first;
}

const vector<GameOption*> base_game_options::build_options_list()
{
    vector<GameOption*> options;
    return options; // ignored by subclass...
}

#ifndef DGAMELAUNCH
static map<string, game_type> _game_modes()
{
    map<string, game_type> modes = {
        {"normal", GAME_TYPE_NORMAL},
        {"seeded", GAME_TYPE_CUSTOM_SEED},
        {"arena", GAME_TYPE_ARENA},
        {"sprint", GAME_TYPE_SPRINT},
        {"tutorial", GAME_TYPE_TUTORIAL},
        {"hints", GAME_TYPE_HINTS}
    };
    if (Version::ReleaseType == VER_ALPHA)
        modes["descent"] = GAME_TYPE_DESCENT;
    return modes;
}
#endif

// **Adding new options**
//
// The main place to put a new option is in the big vector of GameOption
// objects initialized in this function. GameOption objects wrap member vars
// on `game_options` objects, in one of two ways:
//
// * direct wrappers: e.g. a `BoolGameOption` object that corresponds directly
//   to some member of `game_options`. Wrappers exist for various commonly
//   used types, e.g. int, vector, regexes, etc.; see game-options.h for the
//   full complement of wrapper classes available. Some of these are templated,
//   for example, `ListGameOption` supports any class that has a constructor
//   from string, and also accepts a pair of a type and a function that converts
//   from string to that type.
// * indirect wrappers: these use an `on_set` function to turn a string or list
//   into some state that doesn't have a direct wrapper. For example, an option
//   that is parsed into a list of strings as a `ListGameOption<string>`, with
//   an `on_set` function that further converts the strings into a set of int
//   values (this happens to be a case that doesn't have a wrapper class). An
//   `on_set` function *should not* modify state outside of game_options
//   (unless you really know what you're doing) -- these functions are called
//   during initialization of `game_options` and so may overwrite each other
//   if this sort of state is not modified with great care.
//
// Adding a class via a wrapper class in one of these ways will guarantee:
// * orderly initialization, with defaults easily specifiable, and a generic
//   reset function (if the defaults are specified with the constructor)
// * standardized parsing, handling of case, etc without dealing with the parser
//   directly; handling of options with multiple names
// * generic code to set the option from a string, set from another option
// * future support for option menus, generic option serialization code
//
// Nonetheless, if your option is too powerful to be contained in a GameOption
// subclass, there are two more ways that options can be handled. The first
// is the function `game_options::read_custom_option`, which currently deals
// with (a) list options with very complicated handling, (b) options that are
// indirect aliases to other options, and (c) options that support subkeys. If
// at all possible, it is not recommended to add to this function. Third, some
// options are handled entirely in lua using the `chk_lua_option` table in
// `userbase.lua`. See for example `runrest_stop_message` and
// `runrest_ignore_message`. Again, adding new options to this set is
// dispreferred, and should at best be used only for features implemented
// fully in lua code.
const vector<GameOption*> game_options::build_options_list()
{
    const bool USING_DGL =
#if defined(DGAMELAUNCH)
        true;
#else
        false;
#endif
    const bool USING_UNIX =
#if defined(UNIX)
        true;
#else
        false;
#endif

#ifdef USE_TILE
    const bool USING_WEB_TILES =
#if defined(USE_TILE_WEB)
        true;
#else
        false;
#endif
#endif

    // TODO: better organize this list somehow?

    #define SIMPLE_NAME(_opt) _opt, {#_opt}
    #define NEWGAME_NAME(_opt) game._opt, {#_opt}
    // for use where an option has a real value named X with the option name,
    // and a string counterpart that is used by an on_set function,
    // conventionally called X_option:
    #define ON_SET_NAME(_opt) _opt ## _option, {#_opt}
    // ignores superclass stub for this function
    vector<GameOption*> options = {
#if !defined(DGAMELAUNCH) || defined(DGL_REMEMBER_NAME)
        new StringGameOption(NEWGAME_NAME(name), "", true),
#endif
        new BoolGameOption(NEWGAME_NAME(fully_random), false),
        new StringGameOption(NEWGAME_NAME(arena_teams), ""),
        new StringGameOption(NEWGAME_NAME(map), ""),
        new ListGameOption<string>(game.allowed_combos, {"combo"}, {}, true,
            [this]() {
                game.allowed_species.clear();
                game.allowed_jobs.clear();
                game.allowed_weapons.clear();
            }),
        new ListGameOption<species_type, OPTFUN(_str_to_species)>(
            game.allowed_species, {"species", "race"}, {}, true,
            [this]() { game.allowed_combos.clear(); }
            ),
        new ListGameOption<job_type, OPTFUN(str_to_job)>(
            game.allowed_jobs, {"background", "job", "class"}, {}, true,
            [this]() { game.allowed_combos.clear(); }
            ),
        new ListGameOption<weapon_type, OPTFUN(str_to_weapon)>(
            game.allowed_weapons, {"weapon"}, {}, true,
            [this]() { game.allowed_combos.clear(); }
            ),
        new StringGameOption(language_option, {"language"}, "", true,
            [this]()
            {
                if (!set_lang(language_option.c_str()))
                {
                    report_error("No translations for language '%s'.\n"
                                 "Languages with at least partial translation: %s",
                                 language_option.c_str(),
                                 _supported_language_listing().c_str());
                }
            }),
        new StringGameOption(SIMPLE_NAME(fake_lang), "", false,
            [this]() { set_fake_langs(fake_lang); }),

#ifdef DGAMELAUNCH
        // for DGL builds, determined from server config / command line only
        new DisabledGameOption({"crawl_dir", "save_dir", "macro_dir"}),
#else
        // Data/save directories

        // initialization is primarily handled in get_system_environment() and
        // reset_paths(), which are both used regardless of builds. For local
        // builds, these options can be used to override the default/env values
        // via the rc file. Initialization order is quite delicate.

        // possibly the directory options should be part of the superclass?

        // If DATA_DIR_PATH is set (e.g. by a downstream packager), don't allow
        // setting crawl_dir from .crawlrc.
#ifdef DATA_DIR_PATH
        new DisabledGameOption({"crawl_dir"}),
#else
        // default determined by system, see get_system_environment()
        new StringGameOption(ON_SET_NAME(crawl_dir), "", true,
            [this]() {
                // side effect warning! If this is non-default, it will have
                // side effects outside of the options object.
                if (crawl_dir_option.empty()) // set from SysEnv or CLO
                    return;
                if (!_set_crawl_dir(crawl_dir_option))
                {
                    // _set_crawl_dir will attempt to initialize a directory
                    // that isn't currently set as crawl_dir, but the directory
                    // must exist.
                    // if the directory exists but is incomplete / can't be
                    // initialized for some reason, the above call will call
                    // end().
                    report_error("Can't find crawl_dir: '%s'", crawl_dir_option.c_str());
                    crawl_dir_option = "";
                    return;
                }

                // reset all paths in the current options object, so that save_dir
                // and so on will now default to being in `crawl_dir`. If this isn't
                // done immediately, we get a fairly confused situation where save_dir
                // is still the default for part of initialization, and the des cache
                // ends up in the wrong place.
                // XX: should this be done in _set_crawl_dir?
                reset_paths();
            }),
#endif
#ifdef SAVE_DIR_PATH
        new DisabledGameOption({"save_dir", "macro_dir"}),
#else
        // effective default of "saves/", but handled by reset_paths

        new StringGameOption(ON_SET_NAME(save_dir), "", true,
            [this]() {
                // side effect warning! If this is non-default, it will have
                // side effects outside of the options object.
                if (save_dir_option.empty()) // set by reset_paths
                    return;
                // this will create a new directory if it doesn't exist!
                save_dir = _resolve_dir(save_dir_option, "");
#ifndef SHARED_DIR_PATH
                shared_dir = save_dir;
#endif
            }),
        // system-dependent default: see reset_paths
        new StringGameOption(ON_SET_NAME(macro_dir), "", true,
            [this]() {
                // side effect warning! If this is non-default, it will have
                // side effects outside of the options object.
                if (macro_dir_option.empty()) // set by reset_paths
                    return;
                // if this directory doesn't exist, it'll be created if/when
                // the game needs to save macros.txt
                macro_dir = _resolve_dir(macro_dir_option, "");
            }),
#endif // else case of defined(SAVE_DIR_PATH)
#endif // else case of defined(DGAMELAUNCH)

        new BoolGameOption(SIMPLE_NAME(autopickup_starting_ammo), true),
        new MultipleChoiceGameOption<int>(
            autopickup_on, {"default_autopickup"},
            1,
            {{"true", 1}, // XX this would be better as an enum
             {"false", 0}}, true),
        new StringGameOption(SIMPLE_NAME(game_seed), "", false,
            [this]() {
                // special handling because of the large type.
                if (game_seed.empty())
                    return; // for once, seed_from_rc is initialized in the constructor
                uint64_t tmp_seed = 0;
                if (sscanf(game_seed.c_str(), "%" SCNu64, &tmp_seed))
                {
                    // seed_from_rc is only ever set here, or by the CLO. The CLO gets
                    // first crack, so don't overwrite it here.
                    if (!seed_from_rc)
                        seed_from_rc = tmp_seed;
                }
            }),
        new BoolGameOption(SIMPLE_NAME(default_show_all_skills), false),
        new MultipleChoiceGameOption<skill_focus_mode>(
            SIMPLE_NAME(skill_focus),
            SKM_FOCUS_ON,
            {{"true", SKM_FOCUS_ON},
             {"false", SKM_FOCUS_OFF},
             {"toggle", SKM_FOCUS_TOGGLE}}, true),
        new BoolGameOption(SIMPLE_NAME(read_persist_options), false),
        new BoolGameOption(SIMPLE_NAME(auto_switch), false),
        new BoolGameOption(SIMPLE_NAME(suppress_startup_errors), false),
        new BoolGameOption(SIMPLE_NAME(simple_targeting), false),
        new MultipleChoiceGameOption<confirm_prompt_type>(
            SIMPLE_NAME(allow_self_target),
            confirm_prompt_type::prompt,
            {{"true", confirm_prompt_type::none},
             {"false", confirm_prompt_type::cancel},
             {"prompt", confirm_prompt_type::prompt}}, true),
        new BoolGameOption(easy_unequip,
                           { "easy_unequip", "easy_armour", "easy_armor" },
                           true),
        new BoolGameOption(SIMPLE_NAME(equip_unequip), false),
        new BoolGameOption(SIMPLE_NAME(jewellery_prompt), false),
        new BoolGameOption(SIMPLE_NAME(easy_door), true),
        new BoolGameOption(SIMPLE_NAME(warn_hatches), false),
        new BoolGameOption(SIMPLE_NAME(enable_recast_spell), true),
        new BoolGameOption(SIMPLE_NAME(auto_hide_spells), false),
        new BoolGameOption(SIMPLE_NAME(blink_brightens_background), false),
        new MultipleChoiceGameOption<maybe_bool>(
            SIMPLE_NAME(bold_brightens_foreground),
            false, {{"false", false},
                    {"true", maybe_bool::maybe}, // kind of weird
                    {"force", true}}, true),
        new MultipleChoiceGameOption<char_set_type>(
            SIMPLE_NAME(char_set),
            CSET_DEFAULT,
            {{"default", CSET_DEFAULT},
             {"ascii", CSET_ASCII}}),
        new BoolGameOption(SIMPLE_NAME(best_effort_brighten_background), false),
        new BoolGameOption(SIMPLE_NAME(best_effort_brighten_foreground), true),
        new BoolGameOption(SIMPLE_NAME(allow_extended_colours), true),
        new BoolGameOption(SIMPLE_NAME(regex_search), false),
        new BoolGameOption(SIMPLE_NAME(autopickup_search), false),
        new BoolGameOption(SIMPLE_NAME(show_newturn_mark), true),
        new BoolGameOption(SIMPLE_NAME(show_game_time), true),
        new BoolGameOption(SIMPLE_NAME(equip_bar), false),
        new BoolGameOption(SIMPLE_NAME(animate_equip_bar), false),
        new BoolGameOption(SIMPLE_NAME(mouse_input), false),
        new BoolGameOption(SIMPLE_NAME(menu_arrow_control), true),
        new BoolGameOption(mlist_allow_alternate_layout,
                           {"mlist_allow_alternative_layout",
                            "mlist_allow_alternate_layout"}, false),
        new BoolGameOption(SIMPLE_NAME(monster_item_view_coordinates), false),
        new ListGameOption<text_pattern>(SIMPLE_NAME(monster_item_view_features), {}, true),
        new BoolGameOption(SIMPLE_NAME(messages_at_top), false),
        new BoolGameOption(SIMPLE_NAME(msg_condense_repeats), true),
        new BoolGameOption(SIMPLE_NAME(msg_condense_short), true),
        new BoolGameOption(SIMPLE_NAME(view_lock_x), true),
        new BoolGameOption(SIMPLE_NAME(view_lock_y), true),
        new BoolGameOption(SIMPLE_NAME(centre_on_scroll), false),
        new BoolGameOption(SIMPLE_NAME(symmetric_scroll), true),
        new BoolGameOption(SIMPLE_NAME(always_show_exclusions), true),
        new BoolGameOption(SIMPLE_NAME(note_all_skill_levels), false),
        new BoolGameOption(SIMPLE_NAME(note_skill_max), true),
        new BoolGameOption(SIMPLE_NAME(note_xom_effects), true),
        new BoolGameOption(SIMPLE_NAME(note_chat_messages), false),
        new BoolGameOption(SIMPLE_NAME(note_dgl_messages), true),
        new StringGameOption(SIMPLE_NAME(user_note_prefix), "", true),
        new BoolGameOption(SIMPLE_NAME(clear_messages), false),
#ifdef DEBUG
        new BoolGameOption(SIMPLE_NAME(show_more), false),
#else
        new BoolGameOption(SIMPLE_NAME(show_more), true),
#endif
        new BoolGameOption(SIMPLE_NAME(small_more), false),
        new BoolGameOption(SIMPLE_NAME(pickup_thrown), true),
        new BoolGameOption(SIMPLE_NAME(drop_disables_autopickup), false),
        new MaybeBoolGameOption(SIMPLE_NAME(show_god_gift), maybe_bool::maybe,
            {"unid", "unident", "unidentified"}),
        new BoolGameOption(SIMPLE_NAME(show_travel_trail), USING_DGL),
        new BoolGameOption(SIMPLE_NAME(use_fake_cursor), USING_UNIX ),
        new BoolGameOption(SIMPLE_NAME(use_fake_player_cursor), true),
        new BoolGameOption(SIMPLE_NAME(show_player_species), false),
        new BoolGameOption(SIMPLE_NAME(use_modifier_prefix_keys), true),
        new BoolGameOption(SIMPLE_NAME(prompt_menu),
#ifdef USE_TILE_LOCAL
            true
#elif defined (USE_TILE_WEB)
            tiles.is_controlled_from_web()
#else
            false
#endif
            ),
        new BoolGameOption(SIMPLE_NAME(ability_menu), true),
        new BoolGameOption(SIMPLE_NAME(spell_menu), false),
        new BoolGameOption(SIMPLE_NAME(easy_floor_use), false),
        new StringGameOption(ON_SET_NAME(sort_menus), "default", false,
            [this]() {
                for (const string &frag : split_string(";", sort_menus_option))
                    if (!frag.empty())
                        set_menu_sort(frag);
            }),
        new BoolGameOption(SIMPLE_NAME(bad_item_prompt), true),
        new MultipleChoiceGameOption<slot_select_mode>(
            SIMPLE_NAME(assign_item_slot),
            SS_FORWARD,
            {{"forward", SS_FORWARD},
             {"backward", SS_BACKWARD}}),
        new BoolGameOption(SIMPLE_NAME(dos_use_background_intensity), true),
        new BoolGameOption(SIMPLE_NAME(explore_greedy), true),
        new BoolGameOption(SIMPLE_NAME(explore_auto_rest), true),
        new BoolGameOption(SIMPLE_NAME(travel_key_stop), true),
        new ListGameOption<string>(ON_SET_NAME(explore_stop),
            {"item", "stair", "portal", "branch", "shop", "altar",
             "runed_door", "transporter", "greedy_pickup_smart",
             "greedy_visited_item_stack"},
            false,
            [this]() { update_explore_stop_conditions(); }),
        new ListGameOption<string>(ON_SET_NAME(explore_greedy_visit),
            {"glowing", "artefact"},
            false,
            [this]() { update_explore_greedy_visit_conditions(); }),
        new ListGameOption<string>(ON_SET_NAME(travel_avoid_terrain), {}, false,
            [this]() { update_travel_terrain(); }),
        new BoolGameOption(SIMPLE_NAME(travel_one_unsafe_move), false),
        new BoolGameOption(SIMPLE_NAME(dump_on_save), true),
        new BoolGameOption(SIMPLE_NAME(rest_wait_both), false),
        new BoolGameOption(SIMPLE_NAME(rest_wait_ancestor), false),
        new BoolGameOption(SIMPLE_NAME(cloud_status), !is_tiles()),
        new BoolGameOption(SIMPLE_NAME(always_show_zot), false),
        new BoolGameOption(SIMPLE_NAME(always_show_gems), false),
        new BoolGameOption(SIMPLE_NAME(more_gem_info), false),
        new BoolGameOption(SIMPLE_NAME(darken_beyond_range), true),
        new BoolGameOption(SIMPLE_NAME(show_blood), true),
        new ListGameOption<string>(ON_SET_NAME(use_animations),
            {"beam", "range", "hp", "monster_in_sight", "pickup", "monster",
             "player", "branch_entry"},
            false,
            [this]() { update_use_animations(); }),

        new BoolGameOption(SIMPLE_NAME(reduce_animations),
#ifdef USE_TILE_WEB
            // true
            tiles.is_controlled_from_web()
#else
            false
#endif
            ),

        new ListGameOption<text_pattern>(SIMPLE_NAME(unusual_monster_items), {}, true),

        new BoolGameOption(SIMPLE_NAME(arena_dump_msgs), false),
        new BoolGameOption(SIMPLE_NAME(arena_dump_msgs_all), false),
        new BoolGameOption(SIMPLE_NAME(arena_list_eq), false),
        new BoolGameOption(SIMPLE_NAME(default_manual_training), false),
        new BoolGameOption(SIMPLE_NAME(one_SDL_sound_channel), false),
        new BoolGameOption(SIMPLE_NAME(sounds_on), true),
        new BoolGameOption(SIMPLE_NAME(quiver_menu_focus), false),
        new BoolGameOption(SIMPLE_NAME(launcher_autoquiver), true),
        new ColourGameOption(SIMPLE_NAME(tc_reachable), BLUE),
        new ColourGameOption(SIMPLE_NAME(tc_excluded), LIGHTMAGENTA),
        new ColourGameOption(SIMPLE_NAME(tc_exclude_circle), RED),
        new ColourGameOption(SIMPLE_NAME(tc_forbidden), LIGHTCYAN),
        new ColourGameOption(SIMPLE_NAME(tc_dangerous), CYAN),
        new ColourGameOption(SIMPLE_NAME(tc_disconnected), DARKGREY),
        // [ds] Default to jazzy colours.
        new ColourGameOption(SIMPLE_NAME(detected_item_colour), GREEN),
        new ColourGameOption(SIMPLE_NAME(detected_monster_colour), LIGHTRED),
        new ColourGameOption(SIMPLE_NAME(remembered_monster_colour), DARKGREY),
        new ColourGameOption(SIMPLE_NAME(status_caption_colour), BROWN),
        new ColourGameOption(SIMPLE_NAME(background_colour), BLACK),
        new ColourGameOption(SIMPLE_NAME(foreground_colour), LIGHTGREY),
        new BoolGameOption(SIMPLE_NAME(use_terminal_default_colours), false),
        new StringGameOption(enemy_hp_colour_option,
            {"enemy_hp_colour", "enemy_hp_color"}, "default", false,
            [this]() { update_enemy_hp_colour(); }),
        new CursesGameOption(SIMPLE_NAME(friend_highlight),
                             CHATTR_HILITE | (GREEN << 8)),
        new CursesGameOption(SIMPLE_NAME(neutral_highlight),
                             CHATTR_HILITE | (LIGHTGREY << 8)),
        new CursesGameOption(SIMPLE_NAME(unusual_highlight),
                             CHATTR_HILITE | (MAGENTA << 8)),
        new CursesGameOption(SIMPLE_NAME(stab_highlight),
                             CHATTR_HILITE | (BLUE << 8)),
        new CursesGameOption(SIMPLE_NAME(may_stab_highlight),
                             CHATTR_HILITE | (BROWN << 8)),
        new CursesGameOption(SIMPLE_NAME(feature_item_highlight), CHATTR_REVERSE),
        new CursesGameOption(SIMPLE_NAME(trap_item_highlight), CHATTR_REVERSE),
        new CursesGameOption(SIMPLE_NAME(heap_highlight), CHATTR_REVERSE),
        new IntGameOption(SIMPLE_NAME(note_hp_percent), 5, 0, 100),
        new IntGameOption(SIMPLE_NAME(hp_warning), 30, 0, 100),
        new IntGameOption(magic_point_warning, {"mp_warning"}, 0, 0, 100),
        new IntGameOption(SIMPLE_NAME(autofight_warning), 0, 0, 1000),
        // These need to be odd, hence allow +1.
        new IntGameOption(SIMPLE_NAME(view_max_width),
                      max(VIEW_BASE_WIDTH, VIEW_MIN_WIDTH),
                      VIEW_MIN_WIDTH, GXM + 1),
        new IntGameOption(SIMPLE_NAME(view_max_height), max(21, VIEW_MIN_HEIGHT),
                      VIEW_MIN_HEIGHT, GYM + 1),
        new IntGameOption(SIMPLE_NAME(mlist_min_height), 4, 0),
        new IntGameOption(SIMPLE_NAME(msg_min_height), max(7, MSG_MIN_HEIGHT),
                          MSG_MIN_HEIGHT),
        new IntGameOption(SIMPLE_NAME(msg_max_height), max(10, MSG_MIN_HEIGHT),
                          MSG_MIN_HEIGHT),
        new IntGameOption(SIMPLE_NAME(msg_webtiles_height), -1),
        new IntGameOption(SIMPLE_NAME(rest_wait_percent), 100, 0, 100),
        new IntGameOption(SIMPLE_NAME(pickup_menu_limit), 1),
        new IntGameOption(SIMPLE_NAME(view_delay), DEFAULT_VIEW_DELAY, 0),
        new IntGameOption(SIMPLE_NAME(fail_severity_to_confirm), 3, -1, 5),
        new IntGameOption(SIMPLE_NAME(fail_severity_to_quiver), 3, -1, 5),
        new IntGameOption(SIMPLE_NAME(travel_delay), USING_DGL ? -1 : 20,
                          -1, 2000),
        new IntGameOption(SIMPLE_NAME(rest_delay), USING_DGL ? -1 : 0,
                          -1, 2000),
        new IntGameOption(SIMPLE_NAME(explore_delay), -1, -1, 2000),
        new IntGameOption(SIMPLE_NAME(explore_item_greed), 10, -1000, 1000),
        new IntGameOption(SIMPLE_NAME(explore_wall_bias), 0, -1000, 1000),
        new IntGameOption(SIMPLE_NAME(scroll_margin_x), 2, 0),
        new IntGameOption(SIMPLE_NAME(scroll_margin_y), 2, 0),
        new IntGameOption(SIMPLE_NAME(item_stack_summary_minimum), 4),
        new IntGameOption(SIMPLE_NAME(level_map_cursor_step), 7, 1, 50),
        new IntGameOption(SIMPLE_NAME(dump_item_origin_price), -1, -1),
        new IntGameOption(SIMPLE_NAME(dump_message_count), 40),
        new IntGameOption(SIMPLE_NAME(food_snacking_frequency), 40, 0, 100),
        new IntGameOption(SIMPLE_NAME(fountain_line_frequency), 40, 0, 100),
        new MultipleChoiceGameOption<kill_dump_options>(
            SIMPLE_NAME(dump_kill_places),
            KDO_ONE_PLACE,
            {{"none", KDO_NO_PLACES},
             {"false", KDO_NO_PLACES},
             {"all", KDO_ALL_PLACES},
             {"single", KDO_ONE_PLACE},
             {"one", KDO_ONE_PLACE},
             {"true", KDO_ONE_PLACE}}, true),
        new ListGameOption<string>(SIMPLE_NAME(dump_order),
            {"header", "hiscore", "stats", "misc",  "apostles", "inventory",
             "skills", "spells", "overview", "mutations", "messages",
             "screenshot", "monlist", "kills", "notes", "screenshots", "vaults",
             "skill_gains", "action_counts"}),
        new ListGameOption<text_pattern>(SIMPLE_NAME(confirm_action), {}, true),
        new MultipleChoiceGameOption<easy_confirm_type>(
            SIMPLE_NAME(easy_confirm),
            easy_confirm_type::safe,
            {{"none", easy_confirm_type::none},
             {"safe", easy_confirm_type::safe},
             {"all", easy_confirm_type::all}}),
        new ListGameOption<text_pattern>(SIMPLE_NAME(drop_filter), {}, true),
        new ListGameOption<text_pattern>(SIMPLE_NAME(note_monsters), {}, true),
        new ListGameOption<text_pattern>(SIMPLE_NAME(note_messages), {}, true),
        new ListGameOption<text_pattern>(SIMPLE_NAME(note_items), {}, true),
        new ListGameOption<text_pattern>(SIMPLE_NAME(auto_exclude), {}, true),
        new ListGameOption<text_pattern>(SIMPLE_NAME(explore_stop_pickup_ignore), {}, true),

        // defaults handled in dat/defaults/messages.txt:
        new ListGameOption<message_filter>(SIMPLE_NAME(force_more_message), {}, true),
        new ListGameOption<message_filter>(SIMPLE_NAME(flash_screen_message), {}, true),
        new ListGameOption<message_colour_mapping>(message_colour_mappings,
            {"message_colour", "message_color"}, {}, true),
        new ListGameOption<colour_mapping>(menu_colour_mappings,
            {"menu_colour", "menu_color"}, {}, true),
        new ListGameOption<pair<text_pattern,string>, OPTFUN(_slot_mapping)>(auto_item_letters,
            {"item_slot"}, {}, true),
        new ListGameOption<pair<text_pattern,string>, OPTFUN(_slot_mapping)>(auto_spell_letters,
            {"spell_slot"}, {}, true),
        new ListGameOption<pair<text_pattern,string>, OPTFUN(_slot_mapping)>(auto_ability_letters,
            {"ability_slot"}, {}, true),
        new ListGameOption<mlc_mapping>(monster_list_colours_option,
            {"monster_list_colour", "monster_list_color"},
            {{MLC_FRIENDLY, GREEN},
             {MLC_NEUTRAL, BROWN},
             {MLC_GOOD_NEUTRAL, BROWN},
             {MLC_TRIVIAL, DARKGREY},
             {MLC_EASY, LIGHTGREY},
             {MLC_TOUGH, YELLOW},
             {MLC_NASTY, LIGHTRED},
             {MLC_UNUSUAL, LIGHTMAGENTA}
            },
            false,
            [this]()
            {
                // convert the mappings list into a fixed length array for
                // actual use (see mon-info.cc).
                // this doesn't care if there are multiple instances of an MLC
                // around, but later ones simply override earlier ones. A value
                // that isn't set here defaults to LIGHTGRAY.
                monster_list_colours.fill(-1);
                for (const auto &m : monster_list_colours_option)
                    if (m.valid())
                        monster_list_colours[m.category] = m.colour;
            }),

        // these use the same options variable, so their reset functions will
        // interact if they are ever different:
        new ListGameOption<sound_mapping>(sound_mappings, {"sound"}, {}, true),
        new ListGameOption<sound_mapping, OPTFUN(_interrupt_sound_mapping)>(
            sound_mappings, {"hold_sound"}, {}, true),

        new ColourThresholdOption(hp_colour, {"hp_colour", "hp_color"},
                                  "70:yellow, 40:red", _first_greater),
        new ColourThresholdOption(mp_colour, {"mp_colour", "mp_color"},
                                  "50:yellow, 25:red", _first_greater),
        new ColourThresholdOption(stat_colour, {"stat_colour", "stat_color"},
                                  "3:red", _first_less),
        new StringGameOption(SIMPLE_NAME(sound_file_path), "", true),
        new MultipleChoiceGameOption<travel_open_doors_type>(
            SIMPLE_NAME(travel_open_doors), travel_open_doors_type::open,
            {{"avoid", travel_open_doors_type::avoid},
             {"approach", travel_open_doors_type::approach},
             {"open", travel_open_doors_type::open},
             {"false", travel_open_doors_type::_false},
             {"true", travel_open_doors_type::_true}}),

        new MultipleChoiceGameOption<level_gen_type>(
            SIMPLE_NAME(pregen_dungeon),
            level_gen_type::incremental,
            {{"incremental", level_gen_type::incremental},
#ifndef DGAMELAUNCH
             {"true", level_gen_type::full},
             {"full", level_gen_type::full},
#endif
             {"classic", level_gen_type::classic},
             {"false", level_gen_type::classic}
            }, true),
        new BoolGameOption(SIMPLE_NAME(single_column_item_menus), true),

#ifdef DGL_SIMPLE_MESSAGING
        new BoolGameOption(SIMPLE_NAME(messaging), true),
#else
        new DisabledGameOption({"messaging"}),
#endif
#ifdef DGAMELAUNCH
        new DisabledGameOption(
            {"name_bypasses_menu", "restart_after_save", "newgame_after_quit",
             "map_file_name", "morgue_dir"}),
#else
        new MultipleChoiceGameOption<game_type>(NEWGAME_NAME(type),
            GAME_TYPE_NORMAL,
            _game_modes()),
        new BoolGameOption(SIMPLE_NAME(name_bypasses_menu), true),
        new BoolGameOption(SIMPLE_NAME(restart_after_save), true),
        new BoolGameOption(SIMPLE_NAME(newgame_after_quit), false),
        new StringGameOption(SIMPLE_NAME(map_file_name), "", true),
        new StringGameOption(SIMPLE_NAME(morgue_dir),
                             _get_save_path("morgue/"), true),
        new MaybeBoolGameOption(SIMPLE_NAME(restart_after_game),
#ifdef USE_TILE
            // not sure this ifdef makes a lot of sense
            true
#else
            maybe_bool::maybe
#endif
            ),
#endif
        // the following intentionally lack a DisabledGameOption counterpart;
        // make it easier to paste between tiles / non-tiles builds, and also
        // it'd be a pain to maintain without further macros
#ifdef USE_TILE
#ifdef ANDROID
        // android auto-sets these based on resolution, so we initialize them
        // to 0 (an invalid value!)
        new FixedpGameOption(SIMPLE_NAME(tile_viewport_scale), 0.0, 0.2, 16.0),
        new FixedpGameOption(SIMPLE_NAME(tile_map_scale), 0.0, 0.2, 16.0),
#else
        new FixedpGameOption(SIMPLE_NAME(tile_viewport_scale), 1.0, 0.2, 16.0),
        new FixedpGameOption(SIMPLE_NAME(tile_map_scale), 0.6, 0.2, 16.0),
#endif
        new BoolGameOption(SIMPLE_NAME(tile_show_player_species), false,
            [this]() {
                if (tile_show_player_species)
                    set_player_tile("playermons");
                // else, do nothing...should this reset?
            }),
        new StringGameOption(ON_SET_NAME(tile_player_tile), "normal", false,
            [this]() { set_player_tile(tile_player_tile_option); }),
        new StringGameOption(ON_SET_NAME(tile_weapon_offsets), "reset", false,
            [this]() { set_tile_offsets(tile_weapon_offsets_option, false); }),
        new StringGameOption(ON_SET_NAME(tile_shield_offsets), "reset", false,
            [this]() { set_tile_offsets(tile_shield_offsets_option, true); }),
        new StringGameOption(ON_SET_NAME(tile_tag_pref), "auto", false,
            [this]() { tile_tag_pref = _str_to_tag_pref(tile_tag_pref_option); }),

        new BoolGameOption(SIMPLE_NAME(tile_skip_title), false),
        new BoolGameOption(SIMPLE_NAME(tile_menu_icons), true),
        new BoolGameOption(SIMPLE_NAME(tile_filter_scaling), false),
        new BoolGameOption(SIMPLE_NAME(tile_force_overlay), false),
        new TileColGameOption(SIMPLE_NAME(tile_overlay_col), "#646464"),
        new IntGameOption(SIMPLE_NAME(tile_overlay_alpha_percent), 40, 0, 100),
        new BoolGameOption(SIMPLE_NAME(tile_show_minihealthbar), true),
        new BoolGameOption(SIMPLE_NAME(tile_show_minimagicbar), true),
        new BoolGameOption(SIMPLE_NAME(tile_show_demon_tier), false),
        new BoolGameOption(SIMPLE_NAME(tile_grinch), false),
        new StringGameOption(SIMPLE_NAME(tile_show_threat_levels), "nasty, unusual"),
        new StringGameOption(SIMPLE_NAME(tile_show_items), "!?/=([)}:|"),
        // disabled by default due to performance issues
        new BoolGameOption(SIMPLE_NAME(tile_water_anim), !USING_WEB_TILES),
        new BoolGameOption(SIMPLE_NAME(tile_misc_anim), true),
        new IntGameOption(SIMPLE_NAME(tile_font_crt_size), 0, 0, INT_MAX),
        new IntGameOption(SIMPLE_NAME(tile_font_msg_size), 0, 0, INT_MAX),
        new IntGameOption(SIMPLE_NAME(tile_font_stat_size), 0, 0, INT_MAX),
        new IntGameOption(SIMPLE_NAME(tile_font_tip_size), 0, 0, INT_MAX),
        new IntGameOption(SIMPLE_NAME(tile_font_lbl_size), 0, 0, INT_MAX),
        new IntGameOption(SIMPLE_NAME(tile_cell_pixels), 32, 1, INT_MAX),
        new IntGameOption(SIMPLE_NAME(tile_map_pixels), 0, 0, INT_MAX),
#ifndef __ANDROID__
        new IntGameOption(SIMPLE_NAME(tile_tooltip_ms), 500, 0, INT_MAX),
#else
        new IntGameOption(SIMPLE_NAME(tile_tooltip_ms), 0, 0, INT_MAX),
#endif
        new IntGameOption(SIMPLE_NAME(tile_update_rate), 1000, 50, INT_MAX),
        new IntGameOption(SIMPLE_NAME(tile_runrest_rate), 100, 0, INT_MAX),
        // minimap colours
        new TileColGameOption(SIMPLE_NAME(tile_branchstairs_col), "#ff7788"),
        new TileColGameOption(SIMPLE_NAME(tile_deep_water_col), "#001122"),
        new TileColGameOption(SIMPLE_NAME(tile_door_col), "#775544"),
        new TileColGameOption(SIMPLE_NAME(tile_downstairs_col), "#ff00ff"),
        new TileColGameOption(SIMPLE_NAME(tile_excl_centre_col), "#552266"),
        new TileColGameOption(SIMPLE_NAME(tile_excluded_col), "#552266"),
        new TileColGameOption(SIMPLE_NAME(tile_explore_horizon_col), "#6B301B"),
        new TileColGameOption(SIMPLE_NAME(tile_feature_col), "#997700"),
        new TileColGameOption(SIMPLE_NAME(tile_floor_col), "#333333"),
        new TileColGameOption(SIMPLE_NAME(tile_item_col), "#005544"),
        new TileColGameOption(SIMPLE_NAME(tile_lava_col), "#552211"),
        new TileColGameOption(SIMPLE_NAME(tile_mapped_floor_col), "#222266"),
        new TileColGameOption(SIMPLE_NAME(tile_mapped_wall_col), "#444499"),
        new TileColGameOption(SIMPLE_NAME(tile_monster_col), "#660000"),
        new TileColGameOption(SIMPLE_NAME(tile_plant_col), "#446633"),
        new TileColGameOption(SIMPLE_NAME(tile_player_col), "white"),
        new TileColGameOption(SIMPLE_NAME(tile_portal_col), "#ffdd00"),
        new TileColGameOption(SIMPLE_NAME(tile_trap_col), "#aa6644"),
        new TileColGameOption(SIMPLE_NAME(tile_unseen_col), "black"),
        new TileColGameOption(SIMPLE_NAME(tile_upstairs_col), "cyan"),
        new TileColGameOption(SIMPLE_NAME(tile_transporter_col), "#0000ff"),
        new TileColGameOption(SIMPLE_NAME(tile_transporter_landing_col), "#5200aa"),
        new TileColGameOption(SIMPLE_NAME(tile_wall_col), "#666666"),
        new TileColGameOption(SIMPLE_NAME(tile_water_col), "#114455"),
        new TileColGameOption(SIMPLE_NAME(tile_window_col), "#558855"),
        new MultipleChoiceGameOption<string>(
            SIMPLE_NAME(tile_display_mode),
            "tiles",
            {{"tiles", "tiles"},
             {"glyph", "glyphs"},
             {"glyphs", "glyphs"},
             {"hybrid", "hybrid"}}),
        new ListGameOption<string>(SIMPLE_NAME(tile_layout_priority),
            split_string(",", "minimap, inventory, command, "
                              "spell, ability, monster")),
        new ListGameOption<string>(SIMPLE_NAME(tile_player_status_icons),
            split_string(",", "slow, fragile, constr, will/2")),
        new ListGameOption<colour_remapping>(SIMPLE_NAME(custom_text_colours), {}, false),
#endif
#ifdef USE_TILE_LOCAL
# ifndef __ANDROID__
        new IntGameOption(SIMPLE_NAME(game_scale), 1, 1, 8),
# else
        // Android ignores this option and auto-sets the game_scale based on
        // resolution (see TilesFramework::calculate_default_options)
        new DisabledGameOption({"game_scale"}),
# endif
        new IntGameOption(SIMPLE_NAME(tile_key_repeat_delay), 200, 0, INT_MAX),
        new IntGameOption(SIMPLE_NAME(tile_window_width), -90, INT_MIN, INT_MAX),
        new IntGameOption(SIMPLE_NAME(tile_window_height), -90, INT_MIN, INT_MAX),
        new IntGameOption(SIMPLE_NAME(tile_window_ratio), 1618, INT_MIN, INT_MAX),
        new BoolGameOption(SIMPLE_NAME(tile_window_limit_size), true),
        new StringGameOption(SIMPLE_NAME(tile_font_crt_file), MONOSPACED_FONT, true),
        new StringGameOption(SIMPLE_NAME(tile_font_msg_file), MONOSPACED_FONT, true),
        new StringGameOption(SIMPLE_NAME(tile_font_stat_file), MONOSPACED_FONT, true),
        new StringGameOption(SIMPLE_NAME(tile_font_tip_file), MONOSPACED_FONT, true),
        new StringGameOption(SIMPLE_NAME(tile_font_lbl_file), PROPORTIONAL_FONT, true),
        new IntGameOption(SIMPLE_NAME(tile_sidebar_pixels), 32, 1, INT_MAX),
        new MultipleChoiceGameOption<screen_mode>(
            SIMPLE_NAME(tile_full_screen),
            SCREENMODE_AUTO,
            {{"true", SCREENMODE_FULL},
             {"false", SCREENMODE_WINDOW},
             {"maybe", SCREENMODE_AUTO},
             {"auto", SCREENMODE_AUTO}}, true),
        new MaybeBoolGameOption(SIMPLE_NAME(tile_use_small_layout),
                                                maybe_bool::maybe, {"auto"}),
#endif
        // the following intentionally lack a DisabledGameOption counterpart;
        // make it easier to past between tiles / non-tiles builds, and also
        // it'd be a pain to maintain without further macros
#ifdef USE_TILE_WEB
        new BoolGameOption(SIMPLE_NAME(tile_realtime_anim), false),
        new BoolGameOption(SIMPLE_NAME(tile_level_map_hide_messages), true),
        new BoolGameOption(SIMPLE_NAME(tile_level_map_hide_sidebar), false),
        new BoolGameOption(SIMPLE_NAME(tile_web_mouse_control), true),
        new MultipleChoiceGameOption<string>(
            SIMPLE_NAME(tile_web_mobile_input_helper), "auto",
            {{"auto", "auto"}, {"true", "true"}, {"false", "false"}}),
        new StringGameOption(SIMPLE_NAME(tile_font_crt_family), "monospace", true),
        new StringGameOption(SIMPLE_NAME(tile_font_msg_family), "monospace", true),
        new StringGameOption(SIMPLE_NAME(tile_font_stat_family), "monospace", true),
        new StringGameOption(SIMPLE_NAME(tile_font_lbl_family), "monospace", true),
        new StringGameOption(SIMPLE_NAME(glyph_mode_font), "monospace", true),
        new IntGameOption(SIMPLE_NAME(glyph_mode_font_size), 24, 8, 144),
        new BoolGameOption(SIMPLE_NAME(action_panel_show), true),
        new ListGameOption<text_pattern>(SIMPLE_NAME(action_panel_filter), {}, true),
        new BoolGameOption(SIMPLE_NAME(action_panel_show_unidentified), false),
        new StringGameOption(SIMPLE_NAME(action_panel_font_family),
                             "monospace", true),
        new IntGameOption(SIMPLE_NAME(action_panel_font_size), 16),
        new MultipleChoiceGameOption<string>(
            SIMPLE_NAME(action_panel_orientation), "horizontal",
            {{"horizontal", "horizontal"}, {"vertical", "vertical"}}),
        new IntGameOption(SIMPLE_NAME(action_panel_scale), 100, 20, 1600),
        new BoolGameOption(SIMPLE_NAME(action_panel_glyphs), false),
#endif
#ifdef USE_FT
        new BoolGameOption(SIMPLE_NAME(tile_font_ft_light), false),
#endif
        // see post-processing in fixup_options that handles the interaction
        // with CLOs for the following two options:
        new MultipleChoiceGameOption<wizard_option_type>(
            SIMPLE_NAME(wiz_mode),
#if defined(DGAMELAUNCH) || !defined(WIZARD)
            WIZ_NEVER,
#elif defined(DEBUG_DIAGNOSTICS)
            WIZ_YES, // default debug build games to wizmode. Can be overridden in rc
#else
            WIZ_NO,
#endif
#if defined(DGAMELAUNCH) || !defined(WIZARD)
            {}, // setting in rc is disabled
#else
            {{"true", WIZ_YES},
             {"false", WIZ_NO},
             {"never", WIZ_NEVER}},
#endif
             true),
        new MultipleChoiceGameOption<wizard_option_type>(
            SIMPLE_NAME(explore_mode),
            WIZ_NO,
            {{"true", WIZ_YES},
             {"false", WIZ_NO},
             {"never", WIZ_NEVER}},
             true),

#ifdef WIZARD
        new BoolGameOption(SIMPLE_NAME(fsim_csv), false),
        new ListGameOption<string>(SIMPLE_NAME(fsim_scale)),
        new ListGameOption<string>(SIMPLE_NAME(fsim_kit)),
        new StringGameOption(SIMPLE_NAME(fsim_mode), ""),
        new StringGameOption(SIMPLE_NAME(fsim_mons), ""),
        new IntGameOption(SIMPLE_NAME(fsim_rounds), 4000, 1000, 500000),
#endif
#if !defined(DGAMELAUNCH) || defined(DGL_REMEMBER_NAME)
        new BoolGameOption(SIMPLE_NAME(remember_name), true),
#endif
    };

#undef SIMPLE_NAME
    return options;
}

map<string, GameOption*> base_game_options::build_options_map(
    const vector<GameOption*> &options)
{
    map<string, GameOption*> option_map;
    for (GameOption* option : options)
        for (string name : option->getNames())
            option_map[name] = option;
    return option_map;
}

object_class_type item_class_by_sym(char32_t c)
{
    switch (c)
    {
    case ')':
        return OBJ_WEAPONS;
    case '(':
    case U'\x27b9': //➹
        return OBJ_MISSILES;
    case '[':
        return OBJ_ARMOUR;
    case '/':
        return OBJ_WANDS;
    case '?':
        return OBJ_SCROLLS;
    case '"': // Make the amulet symbol equiv to ring -- bwross
    case '=':
    case U'\xb0': //°
        return OBJ_JEWELLERY;
    case '!':
        return OBJ_POTIONS;
    case ':':
    case '+': // ??? -- was the only symbol working for tile order up to 0.10,
              // so keeping it for compat purposes (user configs).
    case U'\x221e': //∞
        return OBJ_BOOKS;
    case '|':
        return OBJ_STAVES;
    case '0':
        return OBJ_ORBS;
    case '}':
        return OBJ_MISCELLANY;
    case '%':
        return OBJ_TALISMANS;
    case '&':
    case 'X':
    case 'x':
        return OBJ_CORPSES;
    case '$':
    case U'\x20ac': //€
    case U'\xa3': //£
    case U'\xa5': //¥ // FR: support more currencies
        return OBJ_GOLD;
#if TAG_MAJOR_VERSION == 34
    case '\\': // Compat break: used to be staves (why not '|'?).
        return OBJ_RODS;
#endif
    case U'\x2666': // ♦
        return OBJ_GEMS;
    default:
        return NUM_OBJECT_CLASSES;
    }
}

// Returns MSGCOL_NONE if unmatched else returns 0-15.
static msg_colour_type _str_to_channel_colour(const string &str)
{
    int col = str_to_colour(str);
    msg_colour_type ret = MSGCOL_NONE;
    if (col == -1)
    {
        if (str == "mute")
            ret = MSGCOL_MUTED;
        else if (str == "plain" || str == "off")
            ret = MSGCOL_PLAIN;
        else if (str == "default" || str == "on")
            ret = MSGCOL_DEFAULT;
        else if (str == "alternative" || str == "alternate")
            ret = MSGCOL_ALTERNATE;
    }
    else
        ret = msg_colour(col);

    return ret;
}

static const string message_channel_names[] =
{
    "plain", "friend_action", "prompt", "god", "duration", "danger", "warning",
    "recovery", "sound", "talk", "talk_visual", "intrinsic_gain",
    "mutation", "monster_spell", "monster_enchant", "friend_spell",
    "friend_enchant", "monster_damage", "monster_target", "banishment",
    "equipment", "floor", "multiturn", "examine", "examine_filter", "diagnostic",
    "error", "tutorial", "orb", "timed_portal", "hell_effect", "monster_warning",
    "dgl_message", "decor_flavour", "monster_timeout"
};

// returns -1 if unmatched else returns 0--(NUM_MESSAGE_CHANNELS-1)
int str_to_channel(const string &str)
{
    COMPILE_CHECK(ARRAYSZ(message_channel_names) == NUM_MESSAGE_CHANNELS);

    // widespread aliases
    if (str == "visual")
        return MSGCH_TALK_VISUAL;
    else if (str == "spell")
        return MSGCH_MONSTER_SPELL;

    for (int ret = 0; ret < NUM_MESSAGE_CHANNELS; ret++)
    {
        if (str == message_channel_names[ret])
            return ret;
    }

    return -1;
}

string channel_to_str(int channel)
{
    if (channel < 0 || channel >= NUM_MESSAGE_CHANNELS)
        return "";

    return message_channel_names[channel];
}


// The map used to interpret a crawlrc entry as a starting weapon
// type. For most entries, we can just look up which weapon has the entry as
// its name; this map contains the exceptions.
// This should be const, but operator[] on maps isn't const.
static map<string, weapon_type> _special_weapon_map = {

    // "staff" normally refers to a magical staff, but here we want to
    // interpret it as a quarterstaff.
    {"staff",       WPN_QUARTERSTAFF},

    // Pseudo-weapons.
    {"unarmed",     WPN_UNARMED},
    {"claws",       WPN_UNARMED},

    {"random",      WPN_RANDOM},

    {"viable",      WPN_VIABLE},
};

/**
 * Interpret a crawlrc entry as a starting weapon type.
 *
 * @param str   The value of the crawlrc entry.
 * @return      The weapon the string refers to, or WPN_UNKNOWN if invalid
 */
weapon_type str_to_weapon(const string &str)
{
    string str_nospace = str;
    remove_whitespace(str_nospace);

    // Synonyms and pseudo-weapons.
    if (_special_weapon_map.count(str_nospace))
        return _special_weapon_map[str_nospace];

    // Real weapons referred to by their standard names.
    return name_nospace_to_weapon(str_nospace);
}

static string _weapon_to_str(weapon_type wpn_type)
{
    if (wpn_type >= 0 && wpn_type < NUM_WEAPONS)
        return weapon_base_name(wpn_type);

    switch (wpn_type)
    {
    case WPN_UNARMED:
        return "claws";
    case WPN_VIABLE:
        return "viable";
    case WPN_RANDOM:
    default:
        return "random";
    }
}

// Summon types can be any of mon_summon_type (enum.h), or a relevant summoning
// spell.
int str_to_summon_type(const string &str)
{
    if (str == "clone")
        return MON_SUMM_CLONE;
    if (str == "animate")
        return MON_SUMM_ANIMATE;
    if (str == "chaos")
        return MON_SUMM_CHAOS;
    if (str == "miscast")
        return MON_SUMM_MISCAST;
    if (str == "zot")
        return MON_SUMM_ZOT;
    if (str == "wrath")
        return MON_SUMM_WRATH;
    if (str == "aid")
        return MON_SUMM_AID;

    return spell_by_name(str);
}

static fire_type _str_to_fire_types(const string &str)
{
    if (str == "stone")
        return FIRE_STONE;
    else if (str == "rock")
        return FIRE_ROCK;
    else if (str == "javelin")
        return FIRE_JAVELIN;
    else if (str == "boomerang")
        return FIRE_BOOMERANG;
    else if (str == "dart")
        return FIRE_DART;
    else if (str == "net")
        return FIRE_NET;
    else if (str == "throwing" || str == "ammo")
        return FIRE_THROWING;
    else if (str == "launcher")
        return FIRE_LAUNCHER;
    else if (str == "inscribed")
        return FIRE_INSCRIBED;
    else if (str == "spell")
        return FIRE_SPELL;
    else if (str == "evokable" || str == "evocable")
        return FIRE_EVOKABLE;
    else if (str == "ability")
        return FIRE_ABILITY;

    return FIRE_NONE;
}

string gametype_to_str(game_type type)
{
    switch (type)
    {
    case GAME_TYPE_NORMAL:
        return "normal";
    case GAME_TYPE_CUSTOM_SEED:
        return "seeded";
    case GAME_TYPE_TUTORIAL:
        return "tutorial";
    case GAME_TYPE_ARENA:
        return "arena";
    case GAME_TYPE_SPRINT:
        return "sprint";
    case GAME_TYPE_HINTS:
        return "hints";
    case GAME_TYPE_DESCENT:
        return "descent";
    default:
        return "none";
    }
}

// XX move to species.cc?
static string _species_to_str(species_type sp)
{
    if (sp == SP_RANDOM)
        return "random";
    else if (sp == SP_VIABLE)
        return "viable";
    else
        return species::name(sp);
}

// XX move to species.cc?
static species_type _str_to_species(const string &str)
{
    if (str == "random")
        return SP_RANDOM;
    else if (str == "viable")
        return SP_VIABLE;

    species_type ret = SP_UNKNOWN;
    if (str.length() == 2) // scan abbreviations
        ret = species::from_abbrev(str.c_str());

    // if we don't have a match, scan the full names
    if (ret == SP_UNKNOWN && str.length() >= 2)
        ret = species::from_str_loose(str, true);

    if (!species::is_starting_species(ret))
        ret = SP_UNKNOWN;

    if (ret == SP_UNKNOWN)
        mprf(MSGCH_ERROR, "Unknown species choice: %s\n", str.c_str());

    return ret;
}

static string _job_to_str(job_type job)
{
    if (job == JOB_RANDOM)
        return "random";
    else if (job == JOB_VIABLE)
        return "viable";
    else
        return get_job_name(job);
}

job_type str_to_job(const string &str)
{
    if (str == "random")
        return JOB_RANDOM;
    else if (str == "viable")
        return JOB_VIABLE;

    job_type job = JOB_UNKNOWN;

    if (str.length() == 2) // scan abbreviations
        job = get_job_by_abbrev(str.c_str());

    // if we don't have a match, scan the full names
    if (job == JOB_UNKNOWN)
        job = get_job_by_name(str.c_str());

    if (!is_starting_job(job))
        job = JOB_UNKNOWN;

    if (job == JOB_UNKNOWN)
        mprf(MSGCH_ERROR, "Unknown background choice: %s\n", str.c_str());

    return job;
}

// read a value which can be either a boolean (in which case return
// 0 for true, -1 for false), or a string of the form PREFIX:NUMBER
// (e.g., auto:7), in which case return NUMBER as an int.
static int _read_bool_or_number(const string &field, int def_value,
                                const string& num_prefix)
{
    int ret = def_value;

    if (field == "true" || field == "1" || field == "yes")
        ret = 0;

    if (field == "false" || field == "0" || field == "no")
        ret = -1;

    if (starts_with(field, num_prefix))
        ret = atoi(field.c_str() + num_prefix.size());

    return ret;
}

void game_options::update_enemy_hp_colour()
{
    // this is: full health, lightly, moderately, heavily, severely, almost dead
    enemy_hp_colour = { GREEN, GREEN, BROWN, BROWN, MAGENTA, RED };

    vector<string> colour_list = split_string(" ", enemy_hp_colour_option, true, true);
    for (size_t i = 0; i < min(colour_list.size(), enemy_hp_colour.size()); i++)
    {
        if (colour_list[i] == "default")
            continue;
        const int col = str_to_colour(colour_list[i]);
        if (col < 0)
        {
            report_error("Bad enemy_hp_colour: %s\n", colour_list[i].c_str());
            continue;
        }
        enemy_hp_colour[i] = col;
    }
    if (colour_list.size() > enemy_hp_colour.size())
    {
        report_error("Extraneous enemy hp color values ignored in '%s'",
            enemy_hp_colour_option.c_str());
    }
}

static string _correct_spelling(const string& str)
{
    if (str == "armor_on")
        return "armour_on";
    if (str == "armor_off")
        return "armour_off";
    if (str == "memorize")
        return "memorise";
    if (str == "jewelry_on")
        return "jewellery_on";
    return str;
}

void game_options::set_default_activity_interrupts()
{
    const char *default_activity_interrupts[] =
    {
        "interrupt_armour_on = hp_loss, monster_attack, monster, mimic",
        "interrupt_armour_off = interrupt_armour_on",
        "interrupt_drop_item = interrupt_armour_on",
        "interrupt_jewellery_on = interrupt_armour_on",
        "interrupt_transform = interrupt_armour_on",
        "interrupt_memorise = hp_loss, monster_attack, stat",
        "interrupt_butcher = interrupt_armour_on, teleport, stat",
        "interrupt_exsanguinate = interrupt_butcher",
        "interrupt_revivify = interrupt_butcher",
        "interrupt_imbue_servitor = interrupt_butcher",
        "interrupt_multidrop = hp_loss, monster_attack, teleport, stat",
        "interrupt_macro = interrupt_multidrop",
        "interrupt_travel = interrupt_butcher, hit_monster, sense_monster, ally_attacked, abyss_exit_spawned",
        "interrupt_run = interrupt_travel, message",
        "interrupt_rest = interrupt_run, full_hp, full_mp, ancestor_hp",

        // Stair ascents/descents cannot be interrupted except by
        // teleportation. Attempts to interrupt the delay will just
        // trash all queued delays, including travel.
        "interrupt_ascending_stairs = teleport",
        "interrupt_descending_stairs = teleport",
        // These are totally uninterruptible by default, since it's
        // impossible for them to be interrupted anyway.
        "interrupt_drop_item = ",
        "interrupt_jewellery_off =",
    };

    for (const char* line : default_activity_interrupts)
        read_option_line(line, false);
}

void game_options::set_activity_interrupt(
        FixedBitVector<NUM_ACTIVITY_INTERRUPTS> &eints,
        const string &interrupt)
{
    if (starts_with(interrupt, interrupt_prefix))
    {
        string delay_name =
            _correct_spelling(interrupt.substr(interrupt_prefix.length()));
        if (!activity_interrupts.count(delay_name))
            return report_error("Unknown delay: %s\n", delay_name.c_str());

        FixedBitVector<NUM_ACTIVITY_INTERRUPTS> &refints =
            activity_interrupts[delay_name];

        eints |= refints;
        return;
    }

    activity_interrupt ai = get_activity_interrupt(interrupt);
    if (ai == activity_interrupt::COUNT)
    {
        return report_error("Delay interrupt name \"%s\" not recognised.\n",
                            interrupt.c_str());
    }

    eints.set(static_cast<int>(ai));
}

void game_options::set_activity_interrupt(const string &activity_name,
                                          const string &interrupt_names,
                                          bool append_interrupts,
                                          bool remove_interrupts)
{
    vector<string> interrupts = split_string(",", interrupt_names);
    auto & eints = activity_interrupts[_correct_spelling(activity_name)];

    if (remove_interrupts)
    {
        FixedBitVector<NUM_ACTIVITY_INTERRUPTS> refints;
        for (const string &interrupt : interrupts)
            set_activity_interrupt(refints, interrupt);

        for (int i = 0; i < NUM_ACTIVITY_INTERRUPTS; ++i)
            if (refints[i])
                eints.set(i, false);
    }
    else
    {
        if (!append_interrupts)
            eints.reset();

        for (const string &interrupt : interrupts)
            set_activity_interrupt(eints, interrupt);
    }

    eints.set(static_cast<int>(activity_interrupt::force));
}

#if defined(DGAMELAUNCH)
static string _resolve_dir(string path, string suffix)
{
    UNUSED(suffix);
    return catpath(path, "");
}
#else

static string _user_home_dir()
{
#ifdef TARGET_OS_WINDOWS
    wchar_t home[MAX_PATH];
    if (SHGetFolderPathW(0, CSIDL_APPDATA, 0, 0, home))
        return "./";
    else
        return utf16_to_8(home);
#else
    const char *home = getenv("HOME");
    if (!home || !*home)
        return "./";
    else
        return mb_to_utf8(home);
#endif
}

static string _user_home_subpath(const string &subpath)
{
    return catpath(_user_home_dir(), subpath);
}

static string _resolve_dir(string path, string suffix)
{
    if (path[0] != '~')
        return catpath(string(path), suffix);
    else
        return _user_home_subpath(catpath(path.substr(1), suffix));
}
#endif

static string _get_save_path(string subdir)
{
    return _resolve_dir(SysEnv.crawl_dir, subdir);
}

/**
 * Reset options paths based on SysEnv values. Should be called if these get
 * changed, but will override several options values.
 */
void game_options::reset_paths()
{
    macro_dir = SysEnv.macro_dir;

    save_dir = _get_save_path("saves/");
    morgue_dir = _get_save_path("morgue/");


#ifndef DGAMELAUNCH
    save_dir_option = "";
    if (macro_dir.empty())
    {
        macro_dir_option = ""; // maybe unnecessary?
#ifdef UNIX
        macro_dir = _user_home_subpath(".crawl");
#else
        macro_dir = "settings/";
#endif
    }
#endif

#if defined(TARGET_OS_MACOSX)
    if (SysEnv.macro_dir.empty())
    {
        macro_dir_option = "";
        macro_dir  = _get_save_path("");
    }
#endif

#if defined(SHARED_DIR_PATH)
    shared_dir = _resolve_dir(SHARED_DIR_PATH, "");
#else
    shared_dir = save_dir;
#endif

}

void game_options::reset_options()
{
    // XXX: do we really need to rebuild the list and map every time?
    // Will they ever change within a single execution of Crawl?
    // GameOption::value's value will change of course, but not the reference.
    base_game_options::reset_options();

    // reset save_dir, morgue_dir, and macro_dir base on the current crawl_dir
    // do this before building the options list, so that any directory-related
    // options have the right base paths to work with, and don't get
    // overwritten
    reset_paths();

    option_behaviour = build_options_list();
    options_by_name = build_options_map(option_behaviour);
    for (GameOption* option : option_behaviour)
        option->reset();

    // some option default values set in dat/defaults

    set_default_activity_interrupts();

#ifdef DEBUG_DIAGNOSTICS
    quiet_debug_messages.reset();
    quiet_debug_messages.set(DIAG_BEAM);
#ifdef DEBUG_MONSPEAK
    quiet_debug_messages.set(DIAG_SPEECH);
#endif
# ifdef DEBUG_MONINDEX
    quiet_debug_messages.set(DIAG_MONINDEX);
# endif
#endif

    game = newgame_def();

#ifdef DGAMELAUNCH
    // somewhat weird looking, but: if the reset object is Options, this just
    // sets it to the default value for newgame_def, and lets CLO override
    // the value. If it's *not* Options, this then inherits whatever was set
    // at CLO.
    game.type = Options.game.type;
#endif

    // set it to the .crawlrc default
    autopickups.reset();
    autopickups.set(OBJ_GOLD);
    autopickups.set(OBJ_SCROLLS);
    autopickups.set(OBJ_POTIONS);
    autopickups.set(OBJ_BOOKS);
    autopickups.set(OBJ_JEWELLERY);
    autopickups.set(OBJ_WANDS);
    autopickups.set(OBJ_GEMS);

    dump_item_origins      = IODS_ARTEFACTS;

    flush_input[ FLUSH_ON_FAILURE ]     = true;
    flush_input[ FLUSH_BEFORE_COMMAND ] = false;
    flush_input[ FLUSH_ON_MESSAGE ]     = false;
    flush_input[ FLUSH_LUA ]            = true;

    fire_items_start       = 0;           // start at slot 'a'

    // Clear fire_order and set up the defaults.
    set_fire_order("launcher, throwing, inscribed, spell, evokable, ability",
                   false, false);
    set_fire_order_spell("all", false, false);
    set_fire_order_ability("all", false, false);

    fire_order_ability.erase(ABIL_TROG_BERSERK);
    fire_order_ability.erase(ABIL_REVIVIFY);
    fire_order_ability.erase(ABIL_IGNIS_FIERY_ARMOUR);
    fire_order_ability.erase(ABIL_IGNIS_FOXFIRE);
    fire_order_ability.erase(ABIL_IGNIS_RISING_FLAME);
#ifdef WIZARD
    // makes testing quiver stuff impossible
    fire_order_ability.erase(ABIL_WIZ_BUILD_TERRAIN);
    fire_order_ability.erase(ABIL_WIZ_SET_TERRAIN);
    fire_order_ability.erase(ABIL_WIZ_CLEAR_TERRAIN);
#endif

    force_spell_targeter =
        { SPELL_HAILSTORM, SPELL_STARBURST, SPELL_FROZEN_RAMPARTS,
          SPELL_IGNITION, SPELL_NOXIOUS_BOG, SPELL_ANGUISH,
          SPELL_CAUSE_FEAR, SPELL_INTOXICATE, SPELL_DISCORD, SPELL_DISPERSAL,
          SPELL_ENGLACIATION, SPELL_DAZZLING_FLASH, SPELL_FLAME_WAVE,
          SPELL_PLASMA_BEAM, SPELL_PILEDRIVER, SPELL_DIAMOND_SAWBLADES,
          SPELL_FORTRESS_BLAST };
    always_use_static_spell_targeters = false;

    force_ability_targeter =
        { ABIL_ZIN_SANCTUARY, ABIL_TSO_CLEANSING_FLAME, ABIL_WORD_OF_CHAOS,
          ABIL_ZIN_RECITE, ABIL_QAZLAL_ELEMENTAL_FORCE, ABIL_JIYVA_OOZEMANCY,
          ABIL_GALVANIC_BREATH, ABIL_YRED_FATHOMLESS_SHACKLES,
          ABIL_CHEIBRIADOS_SLOUCH, ABIL_QAZLAL_DISASTER_AREA,
          ABIL_RU_APOCALYPSE, ABIL_LUGONU_CORRUPT, ABIL_IGNIS_FOXFIRE,
          ABIL_SIPHON_ESSENCE, ABIL_DITHMENOS_SHADOWSLIP };
    always_use_static_ability_targeters = false;

#ifdef DGAMELAUNCH
    // not settable via rc on DGL, so no Options object to initialize them
    restart_after_game = false;
    restart_after_save = false;
    newgame_after_quit = false;
    name_bypasses_menu = true;
#endif

#ifdef USE_TILE_WEB
    action_panel.clear();
    action_panel.emplace_back(OBJ_WANDS);
    action_panel.emplace_back(OBJ_SCROLLS);
    action_panel.emplace_back(OBJ_POTIONS);
    action_panel.emplace_back(OBJ_MISCELLANY);
#endif

    // map each colour to itself as default
    for (int i = 0; i < (int)ARRAYSZ(colour); ++i)
        colour[i] = i;

    // map each channel to plain (well, default for now since I'm testing)
    for (int i = 0; i < NUM_MESSAGE_CHANNELS; ++i)
        channels[i] = MSGCOL_DEFAULT;

    // Currently enabled by default for testing in trunk.
    if (Version::ReleaseType == VER_ALPHA)
        dump_order.push_back("turns_by_place");

    use_animations = (UA_BEAM | UA_RANGE | UA_HP | UA_MONSTER_IN_SIGHT
                      | UA_PICKUP | UA_MONSTER | UA_PLAYER | UA_BRANCH_ENTRY
                      | UA_ALWAYS_ON);

    force_autopickup.clear();
    autoinscriptions.clear();
    note_skill_levels.reset();
    note_skill_levels.set(1);
    note_skill_levels.set(5);
    note_skill_levels.set(10);
    note_skill_levels.set(15);
    note_skill_levels.set(27);

    clear_cset_overrides();

    clear_feature_overrides();
    mon_glyph_overrides.clear();
    item_glyph_overrides.clear();
    item_glyph_cache.clear();

    // Map each category to itself. The user can override in init.txt
    kill_map[KC_YOU] = KC_YOU;
    kill_map[KC_FRIENDLY] = KC_FRIENDLY;
    kill_map[KC_OTHER] = KC_OTHER;
}

void game_options::clear_cset_overrides()
{
    memset(cset_override, 0, sizeof cset_override);
}

void game_options::clear_feature_overrides()
{
    feature_colour_overrides.clear();
    feature_symbol_overrides.clear();
}

char32_t get_glyph_override(int c)
{
    if (c < 0)
        c = -c;
    if (wcwidth(c) != 1)
    {
        mprf(MSGCH_ERROR, "Invalid glyph override: %X", c);
        c = 0;
    }
    return c;
}

static int read_symbol(string s)
{
    if (s.empty())
        return 0;

    if (s.length() > 1 && s[0] == '\\')
        s = s.substr(1);

    {
        char32_t c;
        const char *nc = s.c_str();
        nc += utf8towc(&c, nc);
        // no control, combining or CJK characters, please
        if (!*nc && wcwidth(c) == 1)
            return c;
    }

    int base = 10;
    if (s.length() > 1 && s[0] == 'x')
    {
        s = s.substr(1);
        base = 16;
    }

    char *tail;
    return strtoul(s.c_str(), &tail, base);
}

void game_options::set_fire_order_ability(const string &s, bool append, bool remove)
{
    if (!append && !remove)
        fire_order_ability.clear();
    if (s == "all")
    {
        if (remove)
            fire_order_ability.clear();
        else
            for (const auto &a : get_defined_abilities())
                fire_order_ability.insert(a);
        return;
    }
    if (s == "attack")
    {
        for (const auto &a : get_defined_abilities())
            if (quiver::is_autofight_combat_ability(a))
                if (remove)
                    fire_order_ability.erase(a);
                else
                    fire_order_ability.insert(a);
        return;
    }
    vector<string> slots = split_string(",", s);
    for (const string &slot : slots)
    {
        ability_type abil = ability_by_name(slot);
        if (abil == ABIL_NON_ABILITY)
        {
            report_error("Unknown ability '%s'\n", slot.c_str());
            return;
        }
        if (remove)
            fire_order_ability.erase(abil);
        else
            fire_order_ability.insert(abil);
    }
}

void game_options::set_fire_order_spell(const string &s, bool append, bool remove)
{
    if (!spell_data_initialized()) // not ready in the first read
        return;
    if (!append && !remove)
        fire_order_spell.clear();
    if (s == "all")
    {
        if (remove)
            fire_order_ability.clear();
        else
            for (int i = SPELL_NO_SPELL; i < NUM_SPELLS; i++)
                if (is_valid_spell(static_cast<spell_type>(i)))
                    fire_order_spell.insert(static_cast<spell_type>(i));
        return;
    }
    if (s == "attack")
    {
        for (int i = SPELL_NO_SPELL; i < NUM_SPELLS; i++)
        {
            auto sp = static_cast<spell_type>(i);
            if (quiver::is_autofight_combat_spell(sp))
                if (remove)
                    fire_order_spell.erase(sp);
                else
                    fire_order_spell.insert(sp);
        }
        return;
    }
    vector<string> slots = split_string(",", s);
    for (const string &slot : slots)
    {
        spell_type spell = spell_by_name(slot);
        if (is_valid_spell(spell))
        {
            if (remove)
                fire_order_spell.erase(spell);
            else
                fire_order_spell.insert(spell);
        }
        else
            report_error("Unknown spell '%s'\n", slot.c_str());
    }
}

void game_options::set_fire_order(const string &s, bool append, bool prepend)
{
    if (!append && !prepend)
        fire_order.clear();
    vector<string> slots = split_string(",", s);
    if (prepend)
        reverse(slots.begin(), slots.end());
    for (const string &slot : slots)
        add_fire_order_slot(slot, prepend);
}

void game_options::add_fire_order_slot(const string &s, bool prepend)
{
    unsigned flags = 0;
    for (const string &alt : split_string("/", s))
        flags |= _str_to_fire_types(alt);

    if (flags)
    {
        if (prepend)
            fire_order.insert(fire_order.begin(), flags);
        else
            fire_order.push_back(flags);
    }
}

void game_options::add_force_spell_targeter(const string &s, bool)
{
    if (lowercase_string(s) == "all")
    {
        always_use_static_spell_targeters = true;
        return;
    }
    auto spell = spell_by_name(s, true);
    if (is_valid_spell(spell))
        force_spell_targeter.insert(spell);
    else
        report_error("Unknown spell '%s'\n", s.c_str());
}

void game_options::remove_force_spell_targeter(const string &s)
{
    if (lowercase_string(s) == "all")
    {
        always_use_static_spell_targeters = false;
        return;
    }
    auto spell = spell_by_name(s, true);
    if (is_valid_spell(spell))
        force_spell_targeter.erase(spell);
    else
        report_error("Unknown spell '%s'\n", s.c_str());
}

void game_options::add_force_ability_targeter(const string &s, bool)
{
    if (lowercase_string(s) == "all")
    {
        always_use_static_ability_targeters = true;
        return;
    }
    auto abil = ability_by_name(s);
    if (abil == ABIL_NON_ABILITY)
        report_error("Unknown ability '%s'\n", s.c_str());
    else
        force_ability_targeter.insert(abil);
}

void game_options::remove_force_ability_targeter(const string &s)
{
    if (lowercase_string(s) == "all")
    {
        always_use_static_ability_targeters = false;
        return;
    }
    auto abil = ability_by_name(s);
    if (abil == ABIL_NON_ABILITY)
        report_error("Unknown ability '%s'\n", s.c_str());
    else
        force_ability_targeter.erase(abil);
}

static monster_type _mons_class_by_string(const string &name)
{
    const string match = lowercase_string(name);
    for (monster_type i = MONS_0; i < NUM_MONSTERS; ++i)
    {
        const monsterentry *me = get_monster_data(i);
        if (!me || me->mc == MONS_PROGRAM_BUG)
            continue;

        if (lowercase_string(me->name) == match)
            return i;
    }
    return MONS_0;
}

static set<monster_type> _mons_classes_by_glyph(const char letter)
{
    set<monster_type> matches;
    for (monster_type i = MONS_0; i < NUM_MONSTERS; ++i)
    {
        const monsterentry *me = get_monster_data(i);
        if (!me || me->mc == MONS_PROGRAM_BUG)
            continue;

        if (me->basechar == letter)
            matches.insert(i);
    }
    return matches;
}

cglyph_t game_options::parse_mon_glyph(const string &s) const
{
    cglyph_t md;
    md.col = 0;
    vector<string> phrases = split_string(" ", s);
    for (const string &p : phrases)
    {
        const int col = str_to_colour(p, -1, false);
        if (col != -1)
            md.col = col;
        else
            md.ch = p == "_"? ' ' : read_symbol(p);
    }
    return md;
}

void game_options::remove_mon_glyph_override(const string &text)
{
    vector<string> override = split_string(":", text);

    set<monster_type> matches;
    if (override[0].length() == 1)
        matches = _mons_classes_by_glyph(override[0][0]);
    else
    {
        const monster_type m = _mons_class_by_string(override[0]);
        if (m == MONS_0)
        {
            report_error("Unknown monster: \"%s\"", text.c_str());
            return;
        }
        matches.insert(m);
    }
    for (monster_type m : matches)
        mon_glyph_overrides.erase(m);;
}

void game_options::add_mon_glyph_override(const string &text, bool /*prepend*/)
{
    vector<string> override = split_string(":", text);
    if (override.size() != 2u)
        return;

    set<monster_type> matches;
    if (override[0].length() == 1)
        matches = _mons_classes_by_glyph(override[0][0]);
    else
    {
        const monster_type m = _mons_class_by_string(override[0]);
        if (m == MONS_0)
        {
            report_error("Unknown monster: \"%s\"", text.c_str());
            return;
        }
        matches.insert(m);
    }

    cglyph_t mdisp;

    // Look for monsters first so that "blue devil" works right.
    const monster_type n = _mons_class_by_string(override[1]);
    if (n != MONS_0)
    {
        const monsterentry *me = get_monster_data(n);
        mdisp.ch = me->basechar;
        mdisp.col = me->colour;
    }
    else
        mdisp = parse_mon_glyph(override[1]);

    if (mdisp.ch || mdisp.col)
        for (monster_type m : matches)
            mon_glyph_overrides[m] = mdisp;
}

void game_options::remove_item_glyph_override(const string &text)
{
    string key = text;
    trim_string(key);

    erase_if(item_glyph_overrides,
             [&key](const item_glyph_override_type& arg)
             { return key == arg.first; });
}

void game_options::add_item_glyph_override(const string &text, bool prepend)
{
    vector<string> override = split_string(":", text);
    if (override.size() != 2u)
        return;

    cglyph_t mdisp = parse_mon_glyph(override[1]);
    if (mdisp.ch || mdisp.col)
    {
        if (prepend)
        {
            item_glyph_overrides.emplace(item_glyph_overrides.begin(),
                                               override[0],mdisp);
        }
        else
            item_glyph_overrides.emplace_back(override[0], mdisp);
    }
}

void game_options::remove_feature_override(const string &text)
{
    string fname;
    string::size_type epos = text.rfind("}");
    if (epos != string::npos)
        fname = text.substr(0, text.rfind("{",epos));
    else
        fname = text;

    trim_string(fname);

    vector<dungeon_feature_type> feats = features_by_desc(text_pattern(fname));
    for (dungeon_feature_type f : feats)
    {
        feature_colour_overrides.erase(f);
        feature_symbol_overrides.erase(f);
    }
}

void game_options::add_feature_override(const string &text, bool /*prepend*/)
{
    string::size_type epos = text.rfind("}");
    if (epos == string::npos)
        return;

    string::size_type spos = text.rfind("{", epos);
    if (spos == string::npos)
        return;

    string fname = text.substr(0, spos);
    string props = text.substr(spos + 1, epos - spos - 1);
    vector<string> iprops = split_string(",", props, true, true);

    if (iprops.size() < 1 || iprops.size() > 7)
        return;

    if (iprops.size() < 7)
        iprops.resize(7);

    trim_string(fname);
    vector<dungeon_feature_type> feats = features_by_desc(text_pattern(fname));
    if (feats.empty())
        return;

    for (const dungeon_feature_type feat : feats)
    {
        if (feat >= NUM_FEATURES)
            continue; // TODO: handle other object types.

#define SYM(n, field) if (char32_t s = read_symbol(iprops[n])) \
                          feature_symbol_overrides[feat][n] = s; \
                      else \
                          feature_symbol_overrides[feat][n] = '\0';
        SYM(0, symbol);
        SYM(1, magic_symbol);
#undef SYM
        feature_def &fov(feature_colour_overrides[feat]);
#define COL(n, field) if (colour_t c = str_to_colour(iprops[n], BLACK)) \
                          fov.field = c;
        COL(2, dcolour);
        COL(3, unseen_dcolour);
        COL(4, seen_dcolour);
        COL(5, em_dcolour);
        COL(6, seen_em_dcolour);
#undef COL
    }
}

void game_options::add_cset_override(dungeon_char_type dc, int symbol)
{
    cset_override[dc] = get_glyph_override(symbol);
}

string find_crawlrc()
{
    const char* locations_data[][2] =
    {
        { SysEnv.crawl_dir.c_str(), "init.txt" },
#ifdef UNIX
        { SysEnv.home.c_str(), ".crawl/init.txt" },
        { SysEnv.home.c_str(), ".crawlrc" },
        { SysEnv.home.c_str(), "init.txt" },
#endif
#ifndef DATA_DIR_PATH
        { "", "init.txt" },
        { "..", "init.txt" },
        { "../settings", "init.txt" },
#endif
    };

    // We'll look for these files in any supplied -rcdirs.
    static const char *rc_dir_filenames[] =
    {
        ".crawlrc",
        "init.txt",
    };

    // -rc option always wins.
    if (!SysEnv.crawl_rc.empty())
        return SysEnv.crawl_rc;

    // If we have any rcdirs, look in them for files from the
    // rc_dir_names list.
    for (const string &rc_dir : SysEnv.rcdirs)
    {
        for (const string rc_fn : rc_dir_filenames)
        {
            const string rc(catpath(rc_dir, rc_fn));
            if (file_exists(rc))
                return rc;
        }
    }

    // Check all possibilities for init.txt
    for (const auto& row : locations_data)
    {
        const string rc = catpath(row[0], row[1]);
        if (file_exists(rc))
            return rc;
    }

    // Last attempt: pick up init.txt from datafile_path, which will
    // also search the settings/ directory.
    return datafile_path("init.txt", false, false);
}

static const char* lua_builtins[] =
{
    "clua/stash.lua",
    "clua/delays.lua",
    "clua/autofight.lua",
    "clua/automagic.lua",
    "clua/kills.lua",
};

static const char* config_defaults[] =
{
    "defaults/autopickup_exceptions.txt",
    "defaults/runrest_messages.txt",
    "defaults/standard_colours.txt",
    "defaults/menu_colours.txt",
    "defaults/glyph_colours.txt",
    "defaults/messages.txt",
    "defaults/misc.txt",
};

void base_game_options::reset_loaded_state()
{
    for (auto *o : option_behaviour)
        o->loaded = false;
}

void base_game_options::merge(const base_game_options &other)
{
    for (auto *o : option_behaviour)
    {
        if (o->was_loaded())
            continue; // skip explicitly set values
        GameOption *other_o = other.option_from_name(o->name());
        if (!other_o || !other_o->was_loaded())
            continue;
        o->set_from(other_o);
        // this function is used to merge preferences from the sticky prefs
        // file, so in that context we want to mark this now as loaded so it
        // won't get overridden again
        o->loaded = true;
    }
}


void read_init_file(bool runscripts)
{
    unwind_bool parsing_state(crawl_state.parsing_rc, true);

    Options.reset_options();
    // XX why didn't this clear first
    Options.reset_aliases(false);

    // Load Lua builtins.
    if (runscripts)
    {
        for (const char *builtin : lua_builtins)
        {
            clua.execfile(builtin, false, false);
            if (!clua.error.empty())
                mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
        }
    }

    // Load default options.
    for (const char *def_file : config_defaults)
        Options.include(datafile_path(def_file), false, runscripts);

    // don't count anything up to here as customized
    Options.reset_loaded_state();
    // save the default state for everything
    // TODO: handle lua options here
    get_default_options() = Options;

    // Load early binding extra options from the command line BEFORE init.txt.
    Options.filename     = "extra opts first";
    Options.basefilename = "extra opts first";
    Options.line_num     = 0;
    for (const string &extra : SysEnv.extra_opts_first)
    {
        Options.line_num++;
        Options.read_option_line(extra, true);
    }

    // Load init.txt.
    const string init_file_name = find_crawlrc();
    const string base_file_name = get_base_filename(init_file_name);

    /**
     Mac OS X apps almost always put their user-modifiable configuration files
     in the Application Support directory. On Mac OS X when DATA_DIR_PATH is
     not defined, place a symbolic link to the init.txt file in crawl_dir
     (probably "~/Library/Application Support/Dungeon Crawl Stone Soup") where
     the user is likely to go looking for it.
     */
#if defined(TARGET_OS_MACOSX) && !defined(DATA_DIR_PATH)
    char *cwd = getcwd(NULL, 0);
    if (cwd)
    {
        const string absolute_crawl_rc
            = is_absolute_path(init_file_name) ? init_file_name : catpath(cwd, init_file_name);
        char *resolved = realpath(absolute_crawl_rc.c_str(), NULL);
        if (resolved)
        {
            const string crawl_dir_init = catpath(SysEnv.crawl_dir.c_str(), "init.txt");
            symlink(resolved, crawl_dir_init.c_str());
            free(resolved);
        }
        free(cwd);
    }
#endif

    FileLineInput f(init_file_name.c_str());

    Options.filename = init_file_name;
    Options.basefilename = base_file_name;
    Options.line_num = 0;

    if (f.error())
        return;
    Options.read_options(f, runscripts);

    if (Options.read_persist_options)
    {
        // Read options from a .persist file if one exists.
        clua.load_persist();
        clua.pushglobal("c_persist.options");
        if (lua_isstring(clua, -1))
            read_options(lua_tostring(clua, -1), runscripts);
        lua_pop(clua, 1);
    }

    // Load late binding extra options from the command line AFTER init.txt.
    Options.filename     = "extra opts last";
    Options.basefilename = "extra opts last";
    Options.line_num     = 0;
    for (const string &extra : SysEnv.extra_opts_last)
    {
        Options.line_num++;
        Options.read_option_line(extra, true);
    }

    Options.filename     = init_file_name;
    Options.basefilename = base_file_name;
    Options.line_num     = -1;

#ifdef DEBUG_DIAGNOSTICS
    vector<string> modified;
    for (const auto *o : Options.get_option_behaviour())
        if (o->was_loaded())
            modified.push_back(o->name());
    if (modified.size())
    {
        dprf("Modified regular options after loading rc file: %s",
            join_strings(modified.begin(), modified.end(), ", ").c_str());
    }
#endif

}

newgame_def read_startup_prefs()
{
    // see `write_newgame_options_file` for a long comment that attempts to
    // explain why this needs to be here, but the short answer is that without
    // it, crawl will read from the wrong file some of the time.
    unwind_var<game_type> gt(crawl_state.type, Options.game.type);

    const string filename = get_prefs_filename();
    FileLineInput fl(filename.c_str());
    if (fl.error())
        return newgame_def();

    game_options temp;
    temp.filename = filename;
    temp.basefilename = filename;
    temp.read_options(fl, false);

#ifndef DGAMELAUNCH
    const bool manual_game_type = Options["type"].was_loaded();
#endif

    // !!side effect warning!!
    {
        // don't overwrite whatever is currently in Options with anything from
        // the prefs file; these are handled unlike regular options in this
        // file. Instead, we want to return temp.game and let the caller handle
        // them as needed. (This is not very elegant..)
        unwind_var<newgame_def> cur_game_opts(Options.game);
        Options.merge(temp);
    }

#ifndef DGAMELAUNCH
    // override the startup prefs file with this
    if (manual_game_type)
        temp.game.type = Options.game.type;
#endif

    if (!temp.game.allowed_species.empty())
        temp.game.species = temp.game.allowed_species[0];
    if (!temp.game.allowed_jobs.empty())
        temp.game.job = temp.game.allowed_jobs[0];
    if (!temp.game.allowed_weapons.empty())
        temp.game.weapon = temp.game.allowed_weapons[0];
    if (!Options.seed_from_rc)
        Options.seed = temp.seed_from_rc;
    if (!Options.remember_name)
        temp.game.name = "";
    return temp.game;
}

/**
 * Serialize preferences that should be stored in the sticky prefs file and
 * used across games automatically.
 */
void game_options::write_prefs(FILE *f)
{
    // TODO: generalize, probably some polymorphic functions on GameOption
    // classes. Not worth doing until more stuff is serialized though...
    fprintf(f, "default_manual_training = %s\n",
                        default_manual_training ? "yes" : "no");
    fprintf(f, "quiver_menu_focus = %s\n",
                        quiver_menu_focus ? "true" : "false");
#ifdef USE_TILE_WEB
    fprintf(f, "action_panel_orientation = %s\n",
                        action_panel_orientation.c_str());
    fprintf(f, "action_panel_show = %s\n",
                        action_panel_show ? "yes" : "no");
    fprintf(f, "action_panel_scale = %d\n", action_panel_scale);
    fprintf(f, "action_panel_font_size = %d\n", action_panel_font_size);
#endif
    // TODO: this variable is extremely coarse, maybe something better? Per
    // opts setting? comparison of serializable values like for newgame_def?
    prefs_dirty = false;
}

/**
 * Serialize into a format that can be read with a game_options object.
 */
void newgame_def::write_prefs(FILE *f) const
{
    // TODO: generalize whatever of this writing code can be generalized
#ifndef DGAMELAUNCH
    if (type != NUM_GAME_TYPE)
        fprintf(f, "type = %s\n", gametype_to_str(type).c_str());
#endif
    if (!map.empty())
        fprintf(f, "map = %s\n", map.c_str());
    if (!arena_teams.empty())
        fprintf(f, "arena_teams = %s\n", arena_teams.c_str());
#ifndef DGAMELAUNCH
    fprintf(f, "name = %s\n", name.c_str());
#endif
    if (species != SP_UNKNOWN)
        fprintf(f, "species = %s\n", _species_to_str(species).c_str());
    if (job != JOB_UNKNOWN)
        fprintf(f, "background = %s\n", _job_to_str(job).c_str());
    if (weapon != WPN_UNKNOWN)
        fprintf(f, "weapon = %s\n", _weapon_to_str(weapon).c_str());
    if (seed != 0)
        fprintf(f, "game_seed = %" PRIu64 "\n", seed);
    fprintf(f, "fully_random = %s\n", fully_random ? "yes" : "no");

}

void write_newgame_options_file(const newgame_def& prefs)
{
    // [ds] Saving startup prefs should work like this:
    //
    // 1. If the game is started without specifying a game type, always
    //    save startup preferences in the base savedir.
    // 2. If the game is started with a game type (Sprint), save startup
    //    preferences in the game-specific savedir.
    //
    // The idea is that public servers can use one instance of Crawl
    // but present Crawl and Sprint as two separate games in the
    // server-specific game menu -- the startup prefs file for Crawl and
    // Sprint should never collide, because the public server config will
    // specify the game type on the command-line.
    //
    // For normal users, startup prefs should always be saved in the
    // same base savedir so that when they start Crawl with "./crawl"
    // or the equivalent, their last game choices will be remembered,
    // even if they chose a Sprint game.
    //
    // Yes, this is unnecessarily complex. Better ideas welcome.
    //

    // TODO: this is probably worse now that more than simple newgame char
    // prefs are stored in this file
    unwind_var<game_type> gt(crawl_state.type, Options.game.type);

    if (Options.no_save)
        return;

    string fn = get_prefs_filename();
    FILE *f = fopen_u(fn.c_str(), "w");
    if (!f)
        return;
    prefs.write_prefs(f);
    Options.write_prefs(f);
    fclose(f);
}

// save only the player name -- used to keep it in sync with whatever the last
// loaded save is
void save_player_name()
{
    // Read other preferences
    newgame_def prefs = read_startup_prefs();
    prefs.name = Options.remember_name ? you.your_name : "";

    // And save
    write_newgame_options_file(prefs);
}

// TODO: update all newgame prefs based on the current char, in this function?
void save_game_prefs()
{
    if (!crawl_state.game_saves_prefs() || Options.no_save)
        return;
    // Read existing preferences
    const newgame_def old_prefs = read_startup_prefs();
    newgame_def ng_prefs = old_prefs;
    // make some updates
    ng_prefs.name = Options.remember_name ? you.your_name : "";
    // update seed here only if the char is finished or the game type is seeded,
    // even for offline games.
    if (crawl_state.player_is_dead()
        || crawl_state.type == GAME_TYPE_CUSTOM_SEED)
    {
        ng_prefs.seed = crawl_state.seed;
    }
    else
        ng_prefs.seed = 0;

    // And save
    if (ng_prefs != old_prefs || Options.prefs_dirty)
        write_newgame_options_file(ng_prefs);
}

void read_options(const string &s, bool runscripts, bool clear_aliases)
{
    StringLineInput st(s);
    Options.read_options(st, runscripts, clear_aliases);
}

base_game_options::base_game_options()
    : prefs_dirty(false),
      filename("unknown"),
      basefilename("unknown"),
      line_num(-1)
{
    // no explicit reset_options call in base class
}

void base_game_options::reset_options()
{
    deleteAll(option_behaviour);
    options_by_name.clear();
    aliases.clear();
    variables.clear();
    constants.clear();
    included.clear();
    terp_files.clear();
    additional_macro_files.clear();
    named_options.clear();
    prefs_dirty = false;
    filename = "unknown";
    basefilename = "unknown";
    line_num = -1;

}

base_game_options::base_game_options(base_game_options const& other)
{
    *this = other;
}

base_game_options::base_game_options(base_game_options &&other) noexcept
    : base_game_options()
{
    swap(*this, other);
}

base_game_options& base_game_options::operator=(base_game_options const& other)
{
    if (this != &other)
    {
        // note: simply don't mess with option_behaviour. If there's ever
        // more than one subclass, this could matter.

        aliases = other.aliases;
        variables = other.variables;
        constants = other.constants;
        included = other.included;
        filename = other.filename;
        basefilename = other.basefilename;
        line_num = other.line_num;
        prefs_dirty = other.prefs_dirty; // ??
    }
    return *this;
}

base_game_options::~base_game_options()
{
    deleteAll(option_behaviour);
}

game_options::game_options()
    : base_game_options(),
    seed(0), seed_from_rc(0),
    no_save(false), no_player_bones(false),
    sc_entries(0), sc_format(-1),
    language(lang_t::EN),
    lang_name(nullptr)
{
    reset_options();
}

void base_game_options::reset_aliases(bool clear)
{
    if (clear)
        aliases.clear();
}

void game_options::reset_aliases(bool clear)
{
    base_game_options::reset_aliases(clear);
    // Aus compatibility:
    Options.add_alias("center_on_scroll", "centre_on_scroll");
    // Backwards compatibility:
    Options.add_alias("friend_brand", "friend_highlight");
    Options.add_alias("neutral_brand", "neutral_highlight");
    Options.add_alias("unusual_brand", "unusual_highlight");
    Options.add_alias("stab_brand", "stab_highlight");
    Options.add_alias("may_stab_brand", "may_stab_highlight");
    Options.add_alias("heap_brand", "heap_highlight");
    Options.add_alias("feature_item_brand", "feature_item_highlight");
    Options.add_alias("trap_item_brand", "trap_item_highlight");
    Options.add_alias("tile_single_column_menus", "single_column_item_menus");

}

void base_game_options::read_options(LineInput &il, bool runscripts,
                                bool clear_aliases)
{
    unsigned int line = 0;

    bool inscriptblock = false;
    bool inscriptcond  = false;
    bool isconditional = false;

    bool l_init        = false;

    if (clear_aliases)
        reset_aliases(true);

    dlua_chunk luacond(filename);
    dlua_chunk luacode(filename);

#ifndef CLUA_BINDINGS
    bool clua_error_printed = false;
#endif

    while (!il.eof())
    {
        line_num++;
        string s   = il.get_line();
        string str = s;
        line++;

        trim_string(str);

        // This is to make some efficient comments
        if ((str.empty() || str[0] == '#') && !inscriptcond && !inscriptblock)
            continue;

        if (!inscriptcond && str[0] == ':')
        {
            // The init file is now forced into isconditional mode.
            isconditional = true;
            str = str.substr(1);
            if (!str.empty() && runscripts)
            {
                // If we're in the middle of an option block, close it.
                if (!luacond.empty() && l_init)
                {
                    luacond.add(line - 1, "]==])");
                    l_init = false;
                }
                luacond.add(line, str);
            }
            continue;
        }
        if (!inscriptcond && (starts_with(str, "L<") || starts_with(str, "<")))
        {
            // The init file is now forced into isconditional mode.
            isconditional = true;
            inscriptcond  = true;

            str = str.substr(starts_with(str, "L<") ? 2 : 1);
            // Is this a one-liner?
            if (!str.empty() && str.back() == '>')
            {
                inscriptcond = false;
                str = str.substr(0, str.length() - 1);
            }

            if (!str.empty() && runscripts)
            {
                // If we're in the middle of an option block, close it.
                if (!luacond.empty() && l_init)
                {
                    luacond.add(line - 1, "]==])");
                    l_init = false;
                }
                luacond.add(line, str);
            }
            continue;
        }
        else if (inscriptcond && !str.empty()
                 && (str.find(">") == str.length() - 1 || str == ">L"))
        {
            inscriptcond = false;
            str = str.substr(0, str.length() - 1);
            if (!str.empty() && runscripts)
                luacond.add(line, str);
            continue;
        }
        else if (inscriptcond)
        {
            if (runscripts)
                luacond.add(line, s);
            continue;
        }

        // Handle blocks of Lua
        if (!inscriptblock
            && (starts_with(str, "Lua{") || starts_with(str, "{")))
        {
            inscriptblock = true;
            luacode.clear();
            luacode.set_file(filename);

            // Strip leading Lua[
            str = str.substr(starts_with(str, "Lua{") ? 4 : 1);

            if (!str.empty() && str.find("}") == str.length() - 1)
            {
                str = str.substr(0, str.length() - 1);
                inscriptblock = false;
            }

            if (!str.empty())
                luacode.add(line, str);

            if (!inscriptblock && runscripts)
            {
#ifdef CLUA_BINDINGS
                if (luacode.run(clua))
                {
                    mprf(MSGCH_ERROR, "Lua error: %s",
                         luacode.orig_error().c_str());
                }
                luacode.clear();
#else
                if (!clua_error_printed)
                {
                    mprf(MSGCH_ERROR, "User lua is disabled in this build! `%s`", str.c_str());
                    clua_error_printed = true;
                }
#endif
            }

            continue;
        }
        else if (inscriptblock && (str == "}Lua" || str == "}"))
        {
            inscriptblock = false;
#ifdef CLUA_BINDINGS
            if (runscripts)
            {
                if (luacode.run(clua))
                {
                    mprf(MSGCH_ERROR, "Lua error: %s",
                         luacode.orig_error().c_str());
                }
            }
#else
            if (!clua_error_printed)
            {
                mprf(MSGCH_ERROR, "User lua is disabled in this build! `%s`", str.c_str());
                clua_error_printed = true;
            }
#endif
            luacode.clear();
            continue;
        }
        else if (inscriptblock)
        {
            luacode.add(line, s);
            continue;
        }

        if (isconditional && runscripts)
        {
            if (!l_init)
            {
                luacond.add(line, "crawl.setopt([==[");
                l_init = true;
            }

            luacond.add(line, s);
            continue;
        }

        read_option_line(str, runscripts);
    }

    if (runscripts && !luacond.empty())
    {
#ifdef CLUA_BINDINGS
        if (l_init)
            luacond.add(line, "]==])");
        if (luacond.run(clua))
            mprf(MSGCH_ERROR, "Lua error: %s", luacond.orig_error().c_str());
#else
        if (!clua_error_printed)
        {
            mprf(MSGCH_ERROR, "User lua is disabled in this build! (file: %s)", filename.c_str());
            clua_error_printed = true;
        }
#endif
    }
}

// Note the distinction between:
// 1. aliases "ae := autopickup_exception" "ae += useless_item"
//    stored in game_options.aliases.
// 2. variables "$slots := abc" "spell_slots += Dispel undead:$slots"
//    stored in game_options.variables.
// 3. constant variables "$slots = abc", "constant = slots".
//    stored in game_options.variables, but with an extra entry in
//    game_options.constants.
void base_game_options::add_alias(const string &key, const string &val)
{
    if (key[0] == '$')
    {
        string name = key.substr(1);
        // Don't alter if it's a constant.
        if (constants.count(name))
            return;
        variables[name] = val;
    }
    else
        aliases[key] = val;
}

string base_game_options::unalias(const string &key) const
{
    return lookup(aliases, key, key);
}

#define IS_VAR_CHAR(c) (isaalpha(c) || c == '_' || c == '-')

string base_game_options::expand_vars(const string &field) const
{
    string field_out = field;

    string::size_type curr_pos = 0;

    // Only try 100 times, so as to not get stuck in infinite recursion.
    for (int i = 0; i < 100; i++)
    {
        string::size_type dollar_pos = field_out.find("$", curr_pos);

        if (dollar_pos == string::npos || field_out.size() == (dollar_pos + 1))
            break;

        string::size_type start_pos = dollar_pos + 1;

        if (!IS_VAR_CHAR(field_out[start_pos]))
            continue;

        string::size_type end_pos;
        for (end_pos = start_pos; end_pos + 1 < field_out.size(); end_pos++)
        {
            if (!IS_VAR_CHAR(field_out[end_pos + 1]))
                break;
        }

        string var_name = field_out.substr(start_pos, end_pos - start_pos + 1);

        auto x = variables.find(var_name);

        if (x == variables.end())
        {
            curr_pos = end_pos + 1;
            continue;
        }

        string dollar_plus_name = "$";
        dollar_plus_name += var_name;

        field_out = replace_all(field_out, dollar_plus_name, x->second);

        // Start over at beginning
        curr_pos = 0;
    }

    return field_out;
}



void game_options::fixup_options()
{
    // Validate save_dir
    if (!check_mkdir("Save directory", &save_dir))
        end(1, false, "Cannot create save directory '%s'", save_dir.c_str());

    // TODO: why is morgue_dir, and only morgue_dir, reset to SysEnv here?
    if (!SysEnv.morgue_dir.empty())
        morgue_dir = SysEnv.morgue_dir;

    if (!check_mkdir("Morgue directory", &morgue_dir))
        end(1, false, "Cannot create morgue directory '%s'", morgue_dir.c_str());

#ifdef WIZARD
    // Let CLOs override wiz/explore disabling (including the option being
    // disabled on dgamelaunch builds)
    if (_force_allow_wizard() && wiz_mode == WIZ_NEVER)
        wiz_mode = WIZ_NO;
    if ((_force_allow_wizard() || _force_allow_explore())
                                            && explore_mode == WIZ_NEVER)
    {
        explore_mode = WIZ_NO;
    }
#endif
}

static int _str_to_killcategory(const string &s)
{
    static const char *kc[] =
    {
        "you",
        "friend",
        "other",
    };

    for (unsigned i = 0; i < ARRAYSZ(kc); ++i)
        if (s == kc[i])
            return i;

    return -1;
}

#ifdef USE_TILE
void game_options::set_player_tile(const string &field)
{
    if (field == "normal")
    {
        tile_use_monster = MONS_0;
        tile_player_tile = 0;
        return;
    }
    else if (field == "playermons")
    {
        tile_use_monster = MONS_PLAYER;
        tile_player_tile = 0;
        return;
    }

    vector<string> fields = split_string(":", field);
    // Handle tile:<tile-name> values
    if (fields.size() == 2 && fields[0] == "tile")
    {
        // A variant tile. We have to find the base tile to look this up inthe
        // tile index.
        if (isdigit(*(fields[1].rbegin())))
        {
            string base_tname = fields[1];
            size_t found = base_tname.rfind('_');
            int offset = 0;
            tileidx_t base_tile = 0;
            if (found != std::string::npos
                && parse_int(fields[1].substr(found + 1).c_str(), offset))
            {
                base_tname = base_tname.substr(0, found);
                if (!tile_player_index(base_tname.c_str(), &base_tile))
                {
                    report_error("Can't find base tile \"%s\" of variant "
                                 "tile \"%s\"", base_tname.c_str(),
                                 fields[1].c_str());
                    return;
                }
                tile_player_tile = tileidx_mon_clamp(base_tile, offset);
            }
        }
        else if (!tile_player_index(fields[1].c_str(), &tile_player_tile))
        {
            report_error("Unknown tile: \"%s\"", fields[1].c_str());
            return;
        }
        tile_use_monster = MONS_PLAYER;
    }
    else if (fields.size() == 2 && fields[0] == "mons")
    {
        // Handle mons:<monster-name> values
        const monster_type m = _mons_class_by_string(fields[1]);
        if (m == MONS_0)
            report_error("Unknown monster: \"%s\"", fields[1].c_str());
        else if (mons_class_is_animated_object(m) || mons_is_sensed(m))
        {
            report_error(
                "Can't use that monster as a mons: player tile, sorry: \"%s\"",
                fields[1].c_str());
        }
        else
        {
            tile_use_monster = m;
            tile_player_tile = 0;
        }
    }
    else
    {
        report_error("Invalid setting for tile_player_tile: \"%s\"",
                     field.c_str());
    }
}

void game_options::set_tile_offsets(const string &field, bool set_shield)
{
    bool error = false;
    pair<int, int> *offsets;
    if (set_shield)
        offsets = &tile_shield_offsets;
    else
        offsets = &tile_weapon_offsets;

    if (field == "reset")
    {
        offsets->first = INT_MAX;
        offsets->second = INT_MAX;
        return;
    }

    vector<string> offs = split_string(",", field);
    if (offs.size() != 2
        || !parse_int(offs[0].c_str(), offsets->first)
        || abs(offsets->first) > 32
        || !parse_int(offs[1].c_str(), offsets->second)
        || abs(offsets->second) > 32)
    {
        report_error("Invalid %s tile offsets: \"%s\"",
                     set_shield ? "shield" : "weapon", field.c_str());
        error = true;
    }

    if (error)
    {
        offsets->first = INT_MAX;
        offsets->second = INT_MAX;
    }
}
#endif // USE_TILE

void game_options::do_kill_map(const string &from, const string &to)
{
    int ifrom = _str_to_killcategory(from),
        ito   = _str_to_killcategory(to);
    if (ifrom != -1 && ito != -1)
        kill_map[ifrom] = ito;
}

// Given a dungeon feature description, returns the feature number. This is a
// crude hack and currently recognises only (deep/shallow) water. (XXX)
//
// Returns -1 if the feature named is not recognised, else returns the feature
// number (guaranteed to be 0-255).
static int _get_feature_type(const string &feature)
{
    if (feature.find("deep water") != string::npos)
        return DNGN_DEEP_WATER;
    if (feature.find("shallow water") != string::npos)
        return DNGN_SHALLOW_WATER;
    return -1;
}

void game_options::update_travel_terrain()
{
    travel_avoid_terrain.init(0);
    for (const string &t : travel_avoid_terrain_option)
    {
        const int tfeat = _get_feature_type(t);
        if (tfeat >= 0)
            travel_avoid_terrain[tfeat] = 1;
        else
            report_error("Unknown terrain type '%s'", t.c_str());
    }
}


void game_options::update_use_animations()
{
    static const std::map<const string, use_animation_type> ANIMATION_TYPES =
    {
        {  "beam", UA_BEAM },
        {  "range", UA_RANGE },
        {  "hp", UA_HP },
        {  "monster_in_sight", UA_MONSTER_IN_SIGHT },
        {  "pickup", UA_PICKUP },
        {  "monster", UA_MONSTER },
        {  "player", UA_PLAYER },
        {  "branch_entry", UA_BRANCH_ENTRY }
    };

    use_animations_type animations = UA_ALWAYS_ON; // not 0!
    for (const string &type : use_animations_option)
    {
        const auto search = ANIMATION_TYPES.find(type);
        if (search != ANIMATION_TYPES.end())
            animations |= search->second;
        else
            report_error("Unknown animation type '%s'", type.c_str());
    }

    use_animations = animations;
}

void game_options::update_explore_stop_conditions()
{
    // Note: if altering these, make sure the same alternative spellings appear
    // in update_explore_greedy_visit_conditions if relevant.
    static const std::map<const string, explore_stop_options> STOP_CONDITIONS =
    {
        { "item", ES_ITEM }, { "items", ES_ITEM },
        { "greedy_pickup", ES_GREEDY_PICKUP },
        { "greedy_pickup_gold", ES_GREEDY_PICKUP_GOLD },
        { "greedy_pickup_smart", ES_GREEDY_PICKUP_SMART },
        { "greedy_pickup_thrown", ES_GREEDY_PICKUP_THROWN },
        { "shop" , ES_SHOP }, { "shops", ES_SHOP },
        { "stair", ES_STAIR }, { "stairs", ES_STAIR },
        { "branch" , ES_BRANCH }, { "branches", ES_BRANCH },
        { "portal", ES_PORTAL }, { "portals", ES_PORTAL },
        { "altar", ES_ALTAR }, { "altars", ES_ALTAR },
        { "runed_door", ES_RUNED_DOOR }, { "runed_doors", ES_RUNED_DOOR },
        { "transporter", ES_TRANSPORTER }, { "transporters", ES_TRANSPORTER },
        { "greedy_item", ES_GREEDY_ITEM }, { "greedy_items", ES_GREEDY_ITEM },
        { "greedy_visited_item_stack", ES_GREEDY_VISITED_ITEM_STACK },
        { "glowing" , ES_GLOWING_ITEM }, { "glowing_item", ES_GLOWING_ITEM },
        { "glowing_items", ES_GLOWING_ITEM },
        { "artefact", ES_ARTEFACT }, { "artefacts", ES_ARTEFACT },
        { "artifact", ES_ARTEFACT }, { "artifacts", ES_ARTEFACT },
        { "rune", ES_RUNE }, { "runes", ES_RUNE },
    };
    // convert the options list into a bitfield, and save it by side-effect
    // into game_options::explore_stop. List processing is handled by the
    // option update code, and we can ignore it here.
    int conditions = ES_NONE;
    for (const string &stop : explore_stop_option)
    {
        const string c = replace_all_of(stop, " ", "_");
        const auto search = STOP_CONDITIONS.find(c);
        if (search != STOP_CONDITIONS.end())
            conditions |= search->second;
        else
            report_error("Unknown explore stop condition '%s'", c.c_str());
    }
    explore_stop = conditions;
}

void game_options::update_explore_greedy_visit_conditions()
{
    // Note: if altering these, make sure the same alternative spellings appear
    // in update_explore_stop_conditions if relevant.
    static const std::map<const string, explore_greedy_options> GREEDY_CONDITIONS =
    {
        { "glowing", EG_GLOWING },{ "glowing_item", EG_GLOWING },
        { "glowing_items", EG_GLOWING },
        { "artefact", EG_ARTEFACT }, { "artefacts", EG_ARTEFACT },
        { "artifact", EG_ARTEFACT }, { "artifacts", EG_ARTEFACT },
        { "stack", EG_STACK }, { "stacks", EG_STACK },
        { "pile", EG_STACK }, { "piles", EG_STACK },
    };
    int conditions = EG_NONE;
    for (const string &stop : explore_greedy_visit_option)
    {
        const string c = replace_all_of(stop, " ", "_");
        const auto search = GREEDY_CONDITIONS.find(c);
        if (search != GREEDY_CONDITIONS.end())
            conditions |= search->second;
        else
            report_error("Unknown greedy visit condition '%s'", c.c_str());
    }
    explore_greedy_visit = conditions;
}

message_filter::message_filter(const string &filter)
    : message_filter()
{
    vector<string> splits = split_string(":", filter, true, true, 1, true);
    if (splits.size() > 1)
    {
        // legacy behavior from before escaping `:` was implemented: if the
        // prefix is not a valid channel, treat it as escaped rather than
        // an error. This maybe should be removed for consistency, but would
        // potentially break many rc files.
        int ch = str_to_channel(splits[0]);
        if (ch != -1 || splits[0] == "any")
        {
            pattern = text_pattern(splits[1], true);
            channel = ch;
            return;
        }
        splits[0] += ":";
        splits[0] += splits[1]; // reuse our escaping work
    }
    pattern = text_pattern(splits[0], true);
}

message_colour_mapping::message_colour_mapping(const string &s)
    : message_colour_mapping()
{
    // note: leave all other escape sequences (including `\\`) intact.
    vector<string> cmap = split_string(":", s, true, true, 1, true);

    if (cmap.size() != 2)
    {
        mprf(MSGCH_ERROR, "Options error: Missing colour in message_colour setting: '%s'", s.c_str());
        return; // XX better error handling??
    }

    const int col = str_to_colour(cmap[0]);
    if (cmap[0] == "mute")
        colour = MSGCOL_MUTED;
    else if (col < 0 || col >= NUM_TERM_COLOURS)
    {
        mprf(MSGCH_ERROR, "Options error: Bad colour '%s' in message_colour setting: '%s'",
            cmap[0].c_str(), s.c_str());
        return;
    }
    else
        colour = msg_colour(col);
    message = message_filter(cmap[1]);
}

colour_mapping::colour_mapping(const string &s)
    : colour_mapping()
{
    // Format is "tag:colour:pattern" or "colour:pattern" (default tag).
    vector<string> subseg = split_string(":", s, false, false, 2, true);
    string tagname, colname;
    if (subseg.size() < 2)
    {
        mprf(MSGCH_ERROR, "Options error: Missing tag/color in colour mapping: '%s'", s.c_str());
        return;
    }
    else if (subseg.size() == 3)
    {
        tagname = subseg[0];
        colname = subseg[1];
        pattern = subseg[2];
    }
    else // length 2
    {
        // no tag, treat as "any"
        tagname = "";
        colname = subseg[0];
        pattern = subseg[1];
    }
    const int col = str_to_colour(colname);
    if (col < 0 || col > NUM_TERM_COLOURS)
    {
        mprf(MSGCH_ERROR, "Options error: Unknown color in colour mapping: '%s'", s.c_str());
        return;
    }

    // valid, so now we can actually set the remaining member variables
    tag = tagname;
    colour = col;
}

static sound_mapping _interrupt_sound_mapping(const string &s)
{
    sound_mapping m(s);
    m.interrupt_game = true;
    return m;
}

sound_mapping::sound_mapping(const string &s)
    : sound_mapping()
{
    string::size_type cpos = s.find(":", 0); // TODO: allow escaping?
    if (cpos == string::npos)
    {
        mprf(MSGCH_ERROR, "Options error: invalid sound mapping '%s'", s.c_str());
        return;
    }
#if !defined(USE_SOUND) || !defined(WINMM_PLAY_SOUNDS) || !defined(SOUND_PLAY_COMMAND) || !defined(USE_SDL)
    static bool _sound_warning_issued = false;
    if (!_sound_warning_issued)
    {
        // an rc with sound mappings will have a lot, so only say this once.
        // This might be a bit non-ideal, as it only shows up in the main menu
        // message log...
        _sound_warning_issued = true;
        mprf(MSGCH_PLAIN, "Options warning: `sound` will have no effect on this build.");
    }
#endif

    pattern = s.substr(0, cpos);
    // path resolution is handled when it's time to play the sound, not now
    soundfile = s.substr(cpos + 1);
}

static pair<text_pattern,string> _slot_mapping(const string &s)
{
    vector<string> thesplit = split_string(":", s, true, false, 1);
    if (thesplit.size() != 2)
    {
        mprf(MSGCH_ERROR, "Error parsing slot mapping: '%s'\n",
                            s.c_str());
        return make_pair(text_pattern(), ""); // pattern is marked as invalid
    }
    return make_pair(text_pattern(thesplit[0], true), thesplit[1]);
}

// Option syntax is:
// sort_menu = [menu_type:]yes|no|auto:n[:sort_conditions]
void game_options::set_menu_sort(const string &field)
{
    if (field.empty())
        return;

    if (field == "default")
    {
        sort_menus.clear();
        set_menu_sort("pickup: true");
        set_menu_sort("inv: true : equipped, charged");
        return;
    }

    menu_sort_condition cond(field);

    // Overrides all previous settings.
    if (cond.mtype == menu_type::any)
        sort_menus.clear();

    // Override existing values, if necessary.
    for (menu_sort_condition &m_cond : sort_menus)
        if (m_cond.mtype == cond.mtype)
        {
            m_cond.sort = cond.sort;
            m_cond.cmp  = cond.cmp;
            return;
        }

    sort_menus.push_back(cond);
}

void base_game_options::set_option_fragment(const string &s, bool /*prepend*/)
{
    if (s.empty())
        return;

    string::size_type st = s.find(':');
    if (st == string::npos)
    {
        // Boolean option.
        if (s[0] == '!')
            read_option_line(s.substr(1) + " = false", true);
        else
            read_option_line(s + " = true", true);
    }
    else
    {
        // key:val option.
        read_option_line(s.substr(0, st) + " = " + s.substr(st + 1), true);
    }
}

static bool _set_crawl_dir(const string &d)
{
    const string new_crawl_dir = _resolve_dir(d, "");
    if (!dir_exists(new_crawl_dir))
        return false;
    mprf("Setting crawl_dir to `%s`.", new_crawl_dir.c_str());
    SysEnv.crawl_dir = new_crawl_dir;
    // need to double check that a valid data directory can still be found,so
    // revalidate.
    // note: this will end on failure. (Can this be made more friendly for the
    // rc file case?)
    validate_basedirs();
    return true;
}

// Not a method of the game_options class since keybindings aren't
// stored in that class.
static void _bindkey(string field)
{
    const size_t start_bracket = field.find_first_of('[');
    const size_t end_bracket   = field.find_last_of(']');

    if (start_bracket == string::npos
        || end_bracket == string::npos
        || start_bracket > end_bracket)
    {
        mprf(MSGCH_ERROR, "Bad bindkey bracketing in '%s'",
             field.c_str());
        return;
    }

    const string key_str = field.substr(start_bracket + 1,
                                        end_bracket - start_bracket - 1);
    const char *s = key_str.c_str();

    char32_t wc;
    vector<char32_t> wchars;
    while (int l = utf8towc(&wc, s))
    {
        s += l;
        wchars.push_back(wc);
    }


    int key;

    if (wchars.size() == 0)
    {
        mprf(MSGCH_ERROR, "No key in bindkey directive '%s'",
             field.c_str());
        return;
    }
    else if (wchars.size() == 1)
        key = wchars[0];
    else if (wchars[0] == '\\')
    {
        // does this need to validate non-widechars?
        keyseq ks = parse_keyseq(key_str);
        if (ks.size() != 1)
        {
            mprf(MSGCH_ERROR, "Invalid keyseq '%s' in bindkey directive '%s'",
                key_str.c_str(), field.c_str());
        }
        key = ks[0];
    }
    else // if (wchars.size() > 1)
    {
        // XX probably would be safe to directly check for numbers?
        // don't use the wide char version for this part
        // Try to read a human-readable keycode. `^` sequences handled here.
        key = read_key_code(key_str);
        if (key == CK_NO_KEY)
        {
            mprf(MSGCH_ERROR, "Invalid key '%s' in bindkey directive '%s'",
                 key_str.c_str(), field.c_str());
            return;
        }
    }

    const size_t start_name = field.find_first_not_of(' ', end_bracket + 1);
    if (start_name == string::npos)
    {
        mprf(MSGCH_ERROR, "No command name for bindkey directive '%s'",
             field.c_str());
        return;
    }

    const string       name = field.substr(start_name);
    const command_type cmd  = name_to_command(name);
    if (cmd == CMD_NO_CMD)
    {
        mprf(MSGCH_ERROR, "No command named '%s'", name.c_str());
        return;
    }

    bind_command_to_key(cmd, key);
}

static bool _is_autopickup_ban(pair<text_pattern, bool> entry)
{
    return !entry.second;
}

opt_parse_state base_game_options::parse_option_line(const string &str)
{
    opt_parse_state state;

    const int first_equals = str.find('=');

    // all lines with no equal-signs we ignore
    if (first_equals < 0)
        return state;

    state.raw = str;
    state.field = str.substr(first_equals + 1);
    state.field = expand_vars(state.field);

    string prequal = trimmed_string(str.substr(0, first_equals));

    // Is this a case of key += val?
    if (prequal.length() && prequal[prequal.length() - 1] == '+')
    {
        state.line_type = RCFILE_LINE_PLUS;
        prequal = prequal.substr(0, prequal.length() - 1);
        trim_string(prequal);
    }
    else if (prequal.length() && prequal[prequal.length() - 1] == '-')
    {
        state.line_type = RCFILE_LINE_MINUS;
        prequal = prequal.substr(0, prequal.length() - 1);
        trim_string(prequal);
    }
    else if (prequal.length() && prequal[prequal.length() - 1] == '^')
    {
        state.line_type = RCFILE_LINE_CARET;
        prequal = prequal.substr(0, prequal.length() - 1);
        trim_string(prequal);
    }
    else if (prequal.length() && prequal[prequal.length() - 1] == ':')
    {
        prequal = prequal.substr(0, prequal.length() - 1);
        trim_string(prequal);
        trim_string(state.field);

        add_alias(prequal, state.field);
        state.line_type = RCFILE_LINE_DIRECTIVE;
        // done, no need for further parsing
        state.valid = true;
        return state;
    }

    prequal = unalias(prequal);

    const string::size_type first_dot = prequal.find('.');
    if (first_dot != string::npos)
    {
        state.key    = prequal.substr(0, first_dot);
        state.subkey = prequal.substr(first_dot + 1);
    }
    else
    {
        // no subkey (dots are okay in value field)
        state.key    = prequal;
    }

    // Clean up our data...
    lowercase(trim_string(state.key));
    lowercase(trim_string(state.subkey));

    // some fields want capitals... none care about external spaces
    trim_string(state.field);

    // Keep cased version of `field`, some options need it
    state.raw_field = state.field;
    lowercase(state.field);

    state.valid = true;
    return state;
}

// ugh, this is still very messy. Calling this is quite verbose, hence the
// wrapper below.
static void _base_split_parse(const string &s,
                    const string &separator,
                    function<void(const string &, bool)> modify,
                    bool prepend=false)
{
    // Lots of things use split parse, for some ^= and += should do different things,
    // for others they should not. Split parse just passes them along.
    // this does not allow escaping `separator`, because it doesn't seem like
    // any of the callers currently should need it
    const vector<string> defs = split_string(separator, s);
    if (prepend)
    {
        for (auto it = defs.rbegin(); it != defs.rend(); ++it)
            modify(*it, prepend);
    }
    else
    {
        for (auto it = defs.begin(); it != defs.end(); ++it)
            modify(*it, prepend);
    }
}

void game_options::split_parse(const opt_parse_state &state,
                    const string &separator,
                    void (game_options::*add)(const string &, bool),
                    void (game_options::*remove)(const string &),
                    bool case_sensitive)
{
    const string &s = case_sensitive ? state.raw_field : state.field;

    if (remove && state.minus_equal())
    {
        _base_split_parse(s, separator,
            [this, remove](const string &x, bool)
            {
                (this->*remove)(x);
            });
    }

    if (add && (state.add_equal() || state.plain()))
    {
        _base_split_parse(s, separator,
            [this, add](const string &x, bool b)
            {
                (this->*add)(x, b);
            },
            state.caret_equal());
    }
}

void base_game_options::set_from_defaults(const string &opt)
{
    // don't call this during the game_options constructor...
    // more generally, on first call, the return of this function will be
    // initialized to a fresh game_options object, but without any of the lua
    // default code having been run. After this code is run, it is updated.
    // So this function will only have complete behavior after that point.
    game_options &defaults = get_default_options();

    if (!defaults.count(opt))
    {
        report_error("Unknown option name for default: '%s'", opt.c_str());
        return;
    }

    if (constants.count(opt))
        constants.erase(opt);

    (*this)[opt].set_from(defaults[opt]);
}

/// Parse an option line. Meta-fields are handled directly in this function,
/// e.g. fields that deal with option parsing state, loading of other files,
/// etc. This function calls out to `read_custom_option` and any options
/// specified in the options list. Since the base class is never directly
/// instantiated, this is always called in the context of
/// `game_options::build_options_list`. This does *not* handle things like
/// lua code, comments, etc (see `base_game_options::read_options`) -- at the
/// point where this function is called we should have a valid options line.
///
/// @str a raw option line to parse
/// @runscripts whether lua code should should be run or not. This is false on
///             the initial options parse, and true on the "real" call when
///             starting up a game.
void base_game_options::read_option_line(const string &str, bool runscripts)
{
    opt_parse_state state = parse_option_line(str);
    if (!state.is_valid_option_line())
        return; // either invalid, or already handled directive

    // handle a bunch of option parsing directives that use an `=` syntax
    // should macro file loading be here?
    if (state.key == "include")
    {
        include(state.raw_field, true, runscripts);
        return;
    }
    else if (state.key == "opt" || state.key == "option")
    {
        // does this need the ability to escape `,` for some reason?
        _base_split_parse(state.raw_field, ",",
                [this](const string & s, bool b) { set_option_fragment(s, b); });
        return;
    }
    else if (state.key == "lua_max_memory")
    {
#ifdef DGAMELAUNCH
        report_error("Option 'lua_max_memory' is disabled in this build.");
#else
        if (!sscanf(state.field.c_str(), "%" SCNu64, &crawl_state.clua_max_memory_mb))
            report_error("Couldn't parse integer option lua_max_memory: \"%s\"", state.field.c_str());
#endif
    }
    else if (state.key == "lua_file")
    {
#ifdef CLUA_BINDINGS
        if (runscripts)
        {
            clua.execfile(state.raw_field.c_str(), false, false);
            if (!clua.error.empty())
                mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
        }
#else
        mprf(MSGCH_ERROR, "lua_file failed: clua not enabled on this build!");
#endif
        return;
    }
    else if (state.key == "terp_file")
    {
        if (runscripts)
            terp_files.push_back(state.raw_field);
        return;
    }
    else if (state.key == "constant")
    {
        // should this really be case insensitive?
        if (!variables.count(state.field))
            report_error("No variable named '%s' to make constant", state.field.c_str());
        else if (constants.count(state.field))
            report_error("'%s' is already a constant", state.field.c_str());
        else
            constants.insert(state.field);
        return;
    }
    else if (state.key == "additional_macro_file")
    {
        // TODO: this option could probably be improved. For now, keep the
        // "= means append" behaviour, and don't allow clearing the list;
        // if we rename to "additional_macro_files" then it could work like
        // other list options.
        const string resolved = resolve_include(state.raw_field, "macro ");
        if (!resolved.empty())
            additional_macro_files.push_back(resolved);
        return;
    }
    else if (state.key == "macros")
    {
        // orig_field because this function wants capitals
        const string possible_error = read_rc_file_macro(state.raw_field);

        if (!possible_error.empty())
            report_error(possible_error.c_str(), state.raw_field.c_str());
        return;
    }
    else if (state.key == "bindkey" && runscripts)
    {
        _bindkey(state.raw_field);
        return;
    }
    else if (state.key == "default")
    {
        set_from_defaults(state.field);
        return;
    }

    GameOption *const *option = map_find(options_by_name, state.key);
    if (option)
    {
        const string error = (*option)->loadFromParseState(state);
        if (!error.empty())
            report_error("%s", error.c_str());
        return;
    }

    if (read_custom_option(state, runscripts))
        return;

    // Catch-all else, copies option into map
    if (runscripts)
    {
        if (!clua.callbooleanfn(false, "c_process_lua_option", "ssd",
                state.key.c_str(), state.raw_field.c_str(), state.lua_mode()))
        {
            if (!clua.error.empty())
                mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
            named_options[state.key] = state.raw_field;
        }
    }
}

bool base_game_options::read_custom_option(opt_parse_state &, bool)
{
    return false;
}

// Handle options that have custom parsing code
// return true if we should stop processing the line
//
// Please don't add to this function if you can help it. See the comment
// "Adding new options" in this file above `game_options::build_options_list()`.
bool game_options::read_custom_option(opt_parse_state &state, bool runscripts)
{
    const string key = state.key; // weak
    if (key == "autopickup")
    {
        // clear out autopickup
        autopickups.reset();

        char32_t c;
        for (const char* tp = state.field.c_str(); int s = utf8towc(&c, tp); tp += s)
        {
            object_class_type type = item_class_by_sym(c);

            if (type < NUM_OBJECT_CLASSES)
                autopickups.set(type);
            else
                report_error("Bad object type '%*s' for autopickup.\n", s, tp);
        }
        return true;
    }
    else if (key == "colour" || key == "color")
    {
        const int orig_col   = str_to_colour(state.subkey);
        const int result_col = str_to_colour(state.field);

        if (orig_col != -1 && result_col != -1)
            colour[orig_col] = result_col;
        else
        {
            report_error("Bad colour -- %s=%d or %s=%d\n",
                     state.subkey.c_str(), orig_col, state.field.c_str(), result_col);
        }
        return true;
    }
    else if (key == "channel")
    {
        const int chnl = str_to_channel(state.subkey);
        const msg_colour_type col  = _str_to_channel_colour(state.field);

        if (chnl != -1 && col != MSGCOL_NONE)
            channels[chnl] = col;
        else if (chnl == -1)
            report_error("Bad channel -- %s", state.subkey.c_str());
        else if (col == MSGCOL_NONE)
            report_error("Bad colour -- %s", state.field.c_str());
        return true;
    }
    else if (starts_with(key, interrupt_prefix))
    {
        set_activity_interrupt(key.substr(interrupt_prefix.length()),
                               state.field,
                               state.add_equal(),
                               state.minus_equal());
        return true;
    }
    else if (key == "display_char"
             || starts_with(key, "cset")) // compatibility with old rcfiles
    {
        for (const string &over : split_string(",", state.raw_field))
        {
            vector<string> mapping = split_string(":", over);
            if (mapping.size() != 2)
                continue;

            dungeon_char_type dc = dchar_by_name(mapping[0]);
            if (dc == NUM_DCHAR_TYPES)
                continue;

            add_cset_override(dc, read_symbol(mapping[1]));
        }
        return true;
    }
    else if (key == "feature" || key == "dungeon")
    {
        if (state.plain())
            clear_feature_overrides();

        state.ignore_prepend();
        split_parse(state, ";",
            &game_options::add_feature_override,
            &game_options::remove_feature_override,
            true);
        return true;
    }
    else if (key == "mon_glyph")
    {
        if (state.plain())
            mon_glyph_overrides.clear();

        state.ignore_prepend();
        split_parse(state, ",",
            &game_options::add_mon_glyph_override,
            &game_options::remove_mon_glyph_override,
            true);

        return true;
    }
    else if (key == "item_glyph")
    {
        if (state.plain())
        {
            item_glyph_overrides.clear();
            item_glyph_cache.clear();
        }

        // supports ^=
        split_parse(state, ",",
            &game_options::add_item_glyph_override,
            &game_options::remove_item_glyph_override,
            true);
        return true;
    }
    else if (key == "fire_items_start")
    {
        if (isaalpha(state.raw_field[0]))
            fire_items_start = letter_to_index(state.raw_field[0]);
        else
            report_error("Bad fire item start index: %s\n", state.raw_field.c_str());
        return true;
    }
    else if (key == "fire_order")
    {
        set_fire_order(state.field, state.plus_equal(), state.caret_equal());
        return true;
    }
    else if (key == "fire_order_spell" && runscripts)
    {
        set_fire_order_spell(state.field, state.add_equal(), state.minus_equal());
        return true;
    }
    else if (key == "fire_order_ability" && runscripts)
    {
        set_fire_order_ability(state.field, state.add_equal(), state.minus_equal());
        return true;
    }
    else if (key == "view_lock")
    {
        // alias, not a true option
        const bool lock = read_bool(state.field, true);
        view_lock_x = view_lock_y = lock;
        return true;
    }
    else if (key == "scroll_margin")
    {
        // alias, not a true option
        const int scrollmarg = max(0, atoi(state.field.c_str()));
        scroll_margin_x = scroll_margin_y = scrollmarg;
        return true;
    }
    else if (key == "flush")
    {
        if (state.subkey == "failure")
        {
            flush_input[FLUSH_ON_FAILURE]
                = read_bool(state.field, flush_input[FLUSH_ON_FAILURE]);
        }
        else if (state.subkey == "command")
        {
            flush_input[FLUSH_BEFORE_COMMAND]
                = read_bool(state.field, flush_input[FLUSH_BEFORE_COMMAND]);
        }
        else if (state.subkey == "message")
        {
            flush_input[FLUSH_ON_MESSAGE]
                = read_bool(state.field, flush_input[FLUSH_ON_MESSAGE]);
        }
        else if (state.subkey == "lua")
        {
            flush_input[FLUSH_LUA]
                = read_bool(state.field, flush_input[FLUSH_LUA]);
        }
        return true;
    }
    else if (key == "ban_pickup")
    {
        // Only remove negative, not positive, exceptions.
        if (state.plain())
            erase_if(force_autopickup, _is_autopickup_ban);

        vector<pair<text_pattern, bool> > new_entries;
        for (const string &s : split_string(",", state.raw_field))
        {
            if (s.empty())
                continue;

            const pair<text_pattern, bool> f_a(s, false);

            if (state.minus_equal())
                remove_matching(force_autopickup, f_a);
            else
                new_entries.push_back(f_a);
        }
        merge_lists(force_autopickup, new_entries, state.caret_equal());
        return true;
    }
    else if (key == "autopickup_exceptions")
    {
        if (state.plain())
            force_autopickup.clear();

        vector<pair<text_pattern, bool> > new_entries;
        for (const string &s : split_string(",", state.raw_field))
        {
            if (s.empty())
                continue;

            pair<text_pattern, bool> f_a;

            if (s[0] == '>')
                f_a = make_pair(s.substr(1), false);
            else if (s[0] == '<')
                f_a = make_pair(s.substr(1), true);
            else
                f_a = make_pair(s, false);

            if (state.minus_equal())
                remove_matching(force_autopickup, f_a);
            else
                new_entries.push_back(f_a);
        }
        merge_lists(force_autopickup, new_entries, state.caret_equal());
        return true;
    }
#ifndef _MSC_VER
    // break if-else chain on broken Microsoft compilers with stupid nesting limits
    else
#endif

    if (key == "autoinscribe")
    {
        if (state.plain())
            autoinscriptions.clear();

        const size_t first = state.raw_field.find_first_of(':');
        const size_t last  = state.raw_field.find_last_of(':');
        if (first == string::npos || first != last)
        {
            report_error("Autoinscribe string must have exactly "
                                "one colon: %s\n", state.raw_field.c_str());
            return true;
        }

        if (first == 0)
        {
            report_error("Autoinscribe pattern is empty: %s\n",
                state.raw_field.c_str());
            return true;
        }

        if (last == state.raw_field.length() - 1)
        {
            report_error("Autoinscribe result is empty: %s\n",
                state.raw_field.c_str());
            return true;
        }

        vector<string> thesplit = split_string(":", state.raw_field);

        if (thesplit.size() != 2)
        {
            report_error("Error parsing autoinscribe string: %s\n",
                         state.raw_field.c_str());
            return true;
        }

        pair<text_pattern,string> entry(thesplit[0], thesplit[1]);

        if (state.minus_equal())
            remove_matching(autoinscriptions, entry);
        else if (state.caret_equal())
            autoinscriptions.insert(autoinscriptions.begin(), entry);
        else
            autoinscriptions.push_back(entry);
        return true;
    }
    else if (key == "note_skill_levels")
    {
        if (state.plain())
            note_skill_levels.reset();
        vector<string> thesplit = split_string(",", state.field);
        for (unsigned i = 0; i < thesplit.size(); ++i)
        {
            int num = atoi(thesplit[i].c_str());
            if (num > 0 && num <= 27)
                note_skill_levels.set(num, !state.minus_equal());
            else
            {
                report_error("Bad skill level to note -- %s\n",
                             thesplit[i].c_str());
                continue;
            }
        }
        return true;
    }
    else if (key == "force_spell_targeter")
    {
        // first pass through the rc file happens before the spell name cache
        // is initialized, just skip it
        if (spell_data_initialized())
        {
            if (state.plain())
            {
                always_use_static_spell_targeters = false;
                force_spell_targeter.clear();
            }

            state.ignore_prepend();
            split_parse(state, ",",
                &game_options::add_force_spell_targeter,
                &game_options::remove_force_spell_targeter,
                false);
        }
        return true;
    }
    else if (key == "force_ability_targeter")
    {
        if (state.plain())
        {
            always_use_static_ability_targeters = false;
            force_ability_targeter.clear();
        }

        state.ignore_prepend();
        split_parse(state, ",",
            &game_options::add_force_ability_targeter,
            &game_options::remove_force_ability_targeter,
            false);
        return true;
    }
#ifndef TARGET_COMPILER_VC
    // MSVC has a limit on how many if/else if can be chained together.
    else
#endif
    if (key == "kill_map")
    {
        // TODO: treat this as a map option (e.g. kill_map.you = friendly)
        if (state.plain() && state.field.empty())
        {
            kill_map[KC_YOU] = KC_YOU;
            kill_map[KC_FRIENDLY] = KC_FRIENDLY;
            kill_map[KC_OTHER] = KC_OTHER;
        }

        for (const string &s : split_string(",", state.field))
        {
            string::size_type cpos = s.find(":", 0);
            if (cpos != string::npos)
            {
                string from = s.substr(0, cpos);
                string to   = s.substr(cpos + 1);
                do_kill_map(from, to);
            }
        }
        return true;
    }
    else if (key == "dump_item_origins")
    {
        if (state.plain())
            dump_item_origins = IODS_PRICE;

        for (const string &ch : split_string(",", state.field))
        {
            if (ch == "artefacts" || ch == "artifacts"
                || ch == "artefact" || ch == "artifact")
            {
                dump_item_origins |= IODS_ARTEFACTS;
            }
            else if (ch == "ego_arm" || ch == "ego armour"
                     || ch == "ego_armour" || ch == "ego armor"
                     || ch == "ego_armor")
            {
                dump_item_origins |= IODS_EGO_ARMOUR;
            }
            else if (ch == "ego_weap" || ch == "ego weapon"
                     || ch == "ego_weapon" || ch == "ego weapons"
                     || ch == "ego_weapons")
            {
                dump_item_origins |= IODS_EGO_WEAPON;
            }
            else if (ch == "jewellery" || ch == "jewelry")
                dump_item_origins |= IODS_JEWELLERY;
            else if (ch == "runes")
                dump_item_origins |= IODS_RUNES;
            else if (ch == "staves")
                dump_item_origins |= IODS_STAVES;
            else if (ch == "books")
                dump_item_origins |= IODS_BOOKS;
            else if (ch == "all" || ch == "everything")
                dump_item_origins = IODS_EVERYTHING;
        }
        return true;
    }
#ifdef USE_TILE_WEB
    else if (key == "action_panel")
    {
        // clear out the default list
        action_panel.clear();

        char32_t c;
        for (const char* tp = state.raw_field.c_str(); int s = utf8towc(&c, tp); tp += s)
        {
            object_class_type type = item_class_by_sym(c);

            if (type == OBJ_SCROLLS
                || type == OBJ_POTIONS
                || type == OBJ_WANDS
                || type == OBJ_MISCELLANY)
            {
                action_panel.emplace_back(type);
            }
            else
            {
                report_error("Bad item type '%*s' for action_panel.\n",
                             s, tp);
            }
        }
        return true;
    }
#endif

    return false;
}

static const flang_t *_get_fake_lang(const string &name)
{
    static const map<string, flang_t> fake_lang_names = {
        { "dwarven", flang_t::dwarven },
        { "dwarf", flang_t::dwarven },

        { "jäger", flang_t::jagerkin },
        { "jägerkin", flang_t::jagerkin },
        { "jager", flang_t::jagerkin },
        { "jagerkin", flang_t::jagerkin },
        { "jaeger", flang_t::jagerkin },
        { "jaegerkin", flang_t::jagerkin },

        // Due to a historical conflict with actual german, slang names are
        // supported. Not the really rude ones, though.
        { "de", flang_t::kraut },
        { "german", flang_t::kraut },
        { "kraut", flang_t::kraut },
        { "jerry", flang_t::kraut },
        { "fritz", flang_t::kraut },

        { "futhark", flang_t::futhark },
        { "runes", flang_t::futhark },
        { "runic", flang_t::futhark },

        { "wide", flang_t::wide },
        { "doublewidth", flang_t::wide },
        { "fullwidth", flang_t::wide },

        { "grunt", flang_t::grunt },
        { "sgrunt", flang_t::grunt },
        { "!!!", flang_t::grunt },

        { "butt", flang_t::butt },
        { "buttbot", flang_t::butt },
        { "tef", flang_t::butt },
    };

    return map_find(fake_lang_names, name);
}

struct language_def
{
    lang_t lang;
    const char *code;
    set<string> names;
};

static const vector<language_def> get_lang_data()
{
    static const vector<language_def> lang_data =
    {
        // Use null, not "en", for English so we don't try to look up translations.
        { lang_t::EN, nullptr, { "english", "en", "c" } },
        { lang_t::CS, "cs", { "czech", "český", "cesky" } },
        { lang_t::DA, "da", { "danish", "dansk" } },
        { lang_t::DE, "de", { "german", "deutsch" } },
        { lang_t::EL, "el", { "greek", "ελληνικά", "ελληνικα" } },
        { lang_t::ES, "es", { "spanish", "español", "espanol" } },
        { lang_t::FI, "fi", { "finnish", "suomi" } },
        { lang_t::FR, "fr", { "french", "français", "francais" } },
        { lang_t::HU, "hu", { "hungarian", "magyar" } },
        { lang_t::IT, "it", { "italian", "italiano" } },
        // The last of these for compatibility, since it has been accepted ever
        // since Japanese support was added.
        { lang_t::JA, "ja", { "japanese", "日本語", "日本人" } },
        { lang_t::KO, "ko", { "korean", "한국의" } },
        { lang_t::LT, "lt", { "lithuanian", "lietuvos" } },
        { lang_t::LV, "lv", { "latvian", "lettish", "latvijas", "latviešu",
                              "latvieshu", "latviesu" } },
        { lang_t::NL, "nl", { "dutch", "nederlands" } },
        { lang_t::PL, "pl", { "polish", "polski" } },
        { lang_t::PT, "pt", { "portuguese", "português", "portugues" } },
        { lang_t::RU, "ru", { "russian", "русский", "русскии" } },
        { lang_t::SV, "sv", { "swedish", "svenska" } },
        { lang_t::ZH, "zh", { "chinese", "中国的", "中國的" } },
    };
    return lang_data;
}

static string _supported_language_listing()
{
    const auto &data = get_lang_data();
    return comma_separated_fn(&data[0], &data[data.size()],
                              [](language_def ld){return ld.code ? ld.code : "en";},
                              ",", ",",
                              [](language_def){return true;});
}

bool game_options::set_lang(const char *lc)
{
    if (!lc)
        return false;

    if (!lc[0])
        return true; // do nothing

    if (lc[0] && lc[1] && (lc[2] == '_' || lc[2] == '-'))
        return set_lang(string(lc, 2).c_str());

    const string l = lowercase_string(lc); // Windows returns it capitalized.
    for (const auto &ldef : get_lang_data())
    {
        if ((ldef.code && l == ldef.code) || ldef.names.count(l))
        {
            language = ldef.lang;
            lang_name = ldef.code;
            return true;
        }
    }

    if (const flang_t * const flang = _get_fake_lang(l))
    {
        // Handle fake languages for backwards-compatibility with old rcs.
        // Override rather than stack, because that's how it used to work.
        fake_langs = { { *flang, -1 } };
        return true;
    }

    return false;
}

/**
 * Apply the player's fake language settings.
 *
 * @param input     The value of the "fake_lang" field.
 */
void game_options::set_fake_langs(const string &input)
{
    fake_langs.clear();
    for (const string &flang_text : split_string(",", input))
    {
        const int MAX_LANGS = 3;
        if (fake_langs.size() >= (size_t) MAX_LANGS)
        {
            report_error("Too many fake langs; maximum is %d", MAX_LANGS);
            break;
        }

        const vector<string> split_flang = split_string(":", flang_text);
        const string flang_name = split_flang[0];
        if (split_flang.size() > 2)
        {
            report_error("Invalid fake-lang format: %s", flang_text.c_str());
            continue;
        }

        int tval = -1;
        const int value = split_flang.size() >= 2
                          && parse_int(split_flang[1].c_str(), tval) ? tval : -1;

        const flang_t *flang = _get_fake_lang(flang_name);
        if (flang)
        {
            if (split_flang.size() >= 2)
            {
                if (*flang != flang_t::butt)
                {
                    report_error("Lang %s doesn't take a value",
                                 flang_name.c_str());
                    continue;
                }

                if (value == -1)
                {
                    report_error("Invalid value '%s' provided for lang",
                                 split_flang[1].c_str());
                    continue;
                }
            }

            fake_langs.push_back({*flang, value});
        }
        else
            report_error("Unknown language %s!", flang_name.c_str());

    }
}

// Checks an include file name for safety and resolves it to a readable path.
// If file cannot be resolved, returns the empty string (this does not throw!)
// If file can be resolved, returns the resolved path.
/// @throws unsafe_path if included_file fails the safety check.
string base_game_options::resolve_include(string parent_file, string included_file,
                                     const vector<string> *rcdirs)
{
    // Before we start, make sure we convert forward slashes to the platform's
    // favoured file separator.
    parent_file   = canonicalise_file_separator(parent_file);
    included_file = canonicalise_file_separator(included_file);

    // How we resolve include paths:
    // 1. If it's an absolute path, use it directly.
    // 2. Try the name relative to the parent filename, if supplied.
    // 3. Try the name relative to any of the provided rcdirs.
    // 4. Try locating the name as a regular data file (also checks for the
    //    file name relative to the current directory).
    // 5. Fail, and return empty string.

    assert_read_safe_path(included_file);

    // There's only so much you can do with an absolute path.
    // Note: absolute paths can only get here if we're not on a
    // multiuser system. On multiuser systems assert_read_safe_path()
    // will throw an exception if it sees absolute paths.
    if (is_absolute_path(included_file))
        return file_exists(included_file)? included_file : "";

    if (!parent_file.empty())
    {
        const string candidate = get_path_relative_to(parent_file,
                                                      included_file);
        if (file_exists(candidate))
            return candidate;
    }

    if (rcdirs)
    {
        for (const string &dir : *rcdirs)
        {
            const string candidate(catpath(dir, included_file));
            if (file_exists(candidate))
                return candidate;
        }
    }

    return datafile_path(included_file, false, true);
}

string base_game_options::resolve_include(const string &file, const char *type)
{
    try
    {
        const string resolved = resolve_include(filename, file, &SysEnv.rcdirs);

        if (resolved.empty())
            report_error("Cannot find %sfile \"%s\".", type, file.c_str());
        return resolved;
    }
    catch (const unsafe_path &err)
    {
        report_error("Cannot include %sfile: %s", type, err.what());
        return "";
    }
}

bool base_game_options::was_included(const string &file) const
{
    return included.count(file);
}

void base_game_options::include(const string &rawfilename, bool resolve,
                           bool runscripts)
{
    const string include_file = resolve ? resolve_include(rawfilename)
                                        : rawfilename;

    if (was_included(include_file))
        return;

    included.insert(include_file);

    // Change filename to the included filename while we're reading it.
    unwind_var<string> optfile(filename, include_file);
    unwind_var<string> basefile(basefilename, rawfilename);

    // Save current line number
    unwind_var<int> currlinenum(line_num, 0);

    // Also unwind any aliases defined in included files.
    unwind_var<map<string, string>> unalias(aliases);

    FileLineInput fl(include_file.c_str());
    if (!fl.error())
        read_options(fl, runscripts, false);
}

void base_game_options::report_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    string error = vmake_stringf(format, args);
    va_end(args);

    mprf(MSGCH_ERROR, "Options error: %s (%s:%d)", error.c_str(),
         basefilename.c_str(), line_num);
}

static string check_string(const char *s)
{
    return s ? s : "";
}

void get_system_environment()
{
    // The player's name
    SysEnv.crawl_name = check_string(getenv("CRAWL_NAME"));

    // The directory which contains init.txt, macro.txt, morgue.txt
    // This should end with the appropriate path delimiter.
    SysEnv.crawl_dir = check_string(getenv("CRAWL_DIR"));

#if defined(TARGET_OS_MACOSX) && !defined(DGAMELAUNCH)
    if (SysEnv.crawl_dir.empty())
    {
        SysEnv.crawl_dir
            = _user_home_subpath("Library/Application Support/" CRAWL);
    }
#endif

#ifdef __HAIKU__
    if (SysEnv.crawl_dir.empty())
    {
        char path[B_PATH_NAME_LENGTH];
        find_directory(B_USER_SETTINGS_DIRECTORY,
                        0,
                        false,
                        path,
                        B_PATH_NAME_LENGTH);

        SysEnv.crawl_dir = catpath(std::string(path), "/crawl");
    }
#endif

#ifdef SAVE_DIR_PATH
    if (SysEnv.crawl_dir.empty())
        SysEnv.crawl_dir = SAVE_DIR_PATH;
#endif

#ifdef __ANDROID__
    if (SysEnv.crawl_dir.empty())
    {
        SysEnv.crawl_dir = SDL_AndroidGetExternalStoragePath();
        chdir(SysEnv.crawl_dir.c_str());
    }
#endif

#ifdef DGL_SIMPLE_MESSAGING
    // Enable DGL_SIMPLE_MESSAGING only if SIMPLEMAIL and MAIL are set.
    const char *simplemail = getenv("SIMPLEMAIL");
    if (simplemail && strcmp(simplemail, "0"))
    {
        const char *mail = getenv("MAIL");
        SysEnv.messagefile = mail? mail : "";
    }
#endif

    // The full path to the init file -- this overrides CRAWL_DIR.
    SysEnv.crawl_rc = check_string(getenv("CRAWL_RC"));

#ifdef UNIX
    // The user's home directory (used to look for ~/.crawlrc file)
    SysEnv.home = check_string(getenv("HOME"));
#endif
}

static void set_crawl_base_dir(const char *arg)
{
    if (!arg)
        return;

    SysEnv.crawl_base = get_parent_directory(arg);
}

// parse args, filling in Options and game environment as we go.
// returns true if no unknown or malformed arguments were found.

// Keep this in sync with the option names.
enum commandline_option_type
{
    CLO_SCORES,
    CLO_NAME,
    CLO_RACE,
    CLO_CLASS,
    CLO_DIR,
    CLO_RC,
    CLO_RCDIR,
    CLO_TSCORES,
    CLO_VSCORES,
    CLO_SCOREFILE,
    CLO_MORGUE,
    CLO_MACRO,
    CLO_MAPSTAT,
    CLO_MAPSTAT_DUMP_DISCONNECT,
    CLO_OBJSTAT,
    CLO_ITERATIONS,
    CLO_FORCE_MAP,
    CLO_ARENA,
    CLO_DUMP_MAPS,
    CLO_TEST,
    CLO_SCRIPT,
    CLO_BUILDDB,
    CLO_HELP,
    CLO_VERSION,
    CLO_SEED,
    CLO_PREGEN,
    CLO_SAVE_VERSION,
    CLO_SPRINT,
    CLO_EXTRA_OPT_FIRST,
    CLO_EXTRA_OPT_LAST,
    CLO_SPRINT_MAP,
    CLO_EDIT_SAVE,
    CLO_PRINT_CHARSET,
    CLO_TUTORIAL,
    CLO_WIZARD,
    CLO_EXPLORE,
    CLO_NO_SAVE,
    CLO_NO_PLAYER_BONES,
    CLO_GDB,
    CLO_NO_GDB, CLO_NOGDB,
    CLO_THROTTLE,
    CLO_NO_THROTTLE,
    CLO_CLUA_MAX_MEMORY,
    CLO_PLAYABLE_JSON, // JSON metadata for species, jobs, combos.
    CLO_BRANCHES_JSON, // JSON metadata for branches.
    CLO_SAVE_JSON,
    CLO_GAMETYPES_JSON,
    CLO_EDIT_BONES,
    CLO_DESCENT,
#if defined(UNIX) || defined(USE_TILE_LOCAL)
    CLO_HEADLESS,
#endif
#ifdef USE_TILE_WEB
    CLO_WEBTILES_SOCKET,
    CLO_AWAIT_CONNECTION,
    CLO_PRINT_WEBTILES_OPTIONS,
#endif
    CLO_RESET_CACHE,

    CLO_NOPS
};

// CLOs that will work ok in headless mode.
static set<commandline_option_type> clo_headless_ok = {
// ok in all builds
    CLO_SCORES,
    CLO_BUILDDB,
    CLO_RESET_CACHE,
    CLO_HELP,
    CLO_VERSION,
    CLO_PLAYABLE_JSON, // JSON metadata for species, jobs, combos.
    CLO_BRANCHES_JSON, // JSON metadata for branches.
    CLO_EDIT_BONES,
    CLO_MAPSTAT,
    CLO_MAPSTAT_DUMP_DISCONNECT,
    CLO_OBJSTAT,
#ifndef USE_TILE_LOCAL
// TODO: still too crashy in local tiles to enable
    CLO_RC,
#endif
    CLO_ARENA,
    CLO_TEST,
    CLO_SCRIPT,
#ifdef USE_TILE_WEB
    CLO_WEBTILES_SOCKET,
    CLO_AWAIT_CONNECTION,
    CLO_PRINT_WEBTILES_OPTIONS,
    CLO_SAVE_JSON,
    CLO_GAMETYPES_JSON,
#endif
};

static const char *cmd_ops[] =
{
    "scores", "name", "species", "background", "dir", "rc", "rcdir", "tscores",
    "vscores", "scorefile", "morgue", "macro", "mapstat", "dump-disconnect",
    "objstat", "iters", "force-map", "arena", "dump-maps", "test", "script",
    "builddb", "help", "version", "seed", "pregen", "save-version", "sprint",
    "extra-opt-first", "extra-opt-last", "sprint-map", "edit-save",
    "print-charset", "tutorial", "wizard", "explore", "no-save",
    "no-player-bones", "gdb", "no-gdb", "nogdb", "throttle", "no-throttle",
    "lua-max-memory", "playable-json", "branches-json", "save-json",
    "gametypes-json", "bones", "descent",
#if defined(UNIX) || defined(USE_TILE_LOCAL)
    "headless",
#endif
#ifdef USE_TILE_WEB
    "webtiles-socket", "await-connection", "print-webtiles-options",
#endif
    "reset-cache",
};


#define num_cmd_ops CLO_NOPS

static bool& _arg_seen(commandline_option_type c)
{
    static bool arg_seen[num_cmd_ops];
    static bool arg_seen_init = false;
    if (!arg_seen_init)
    {
        for (int i = 0; i < num_cmd_ops; i++)
            arg_seen[i] = false;
        arg_seen_init = true;
    }
    return arg_seen[c];
}

// set these checks up so they can be forward-declared without reordering the
// whole file
static bool _force_allow_wizard()
{
    return _arg_seen(CLO_WIZARD);
}

static bool _force_allow_explore()
{
    return _arg_seen(CLO_EXPLORE);
}

static string _find_executable_path()
{
    // A lot of OSes give ways to find the location of the running app's
    // binary executable. This is useful, because argv[0] can be relative
    // when we really need an absolute path in order to locate the game's
    // resources.
#if defined (TARGET_OS_WINDOWS)
    wchar_t tempPath[MAX_PATH];
    if (GetModuleFileNameW(nullptr, tempPath, MAX_PATH))
        return utf16_to_8(tempPath);
    else
        return "";
#elif defined (UNIX)
    char tempPath[2048];
    const ssize_t rsize =
        readlink("/proc/self/exe", tempPath, sizeof(tempPath) - 1);
    if (rsize > 0)
    {
        tempPath[rsize] = 0;
        return mb_to_utf8(tempPath);
    }
    return "";
#elif defined (TARGET_OS_MACOSX)
    return mb_to_utf8(NXArgv[0]);
#else
    // We don't know how to find the executable's path on this OS.
    return "";
#endif
}

static void _print_version()
{
    printf("Crawl version %s%s", Version::Long, "\n");
    printf("Save file version %d.%d%s", TAG_MAJOR_VERSION, TAG_MINOR_VERSION, "\n");
    printf("%s", compilation_info);
}

static void _print_save_version(char *name)
{
    // Ensure that the savedir option is set correctly on the first parse_args
    // pass.
    // TODO: read initfile for local games?
    Options.reset_paths();
    try
    {
        string filename = name;
        // Check for the exact filename first, then go by char name.
        if (!file_exists(filename))
            filename = get_savedir_filename(filename);
        package save(filename.c_str(), false);
        reader chrf(&save, "chr");

        save_version v = get_save_version(chrf);
        if (!v.valid())
            fail("Save file is invalid.");
        else
            printf("Save file version for %s is %d.%d\n", name, v.major, v.minor);
    }
    catch (ext_fail_exception &fe)
    {
        fprintf(stderr, "Error: %s\n", fe.what());
    }
}

enum es_command_type
{
    ES_LS,
    ES_RM,
    ES_GET,
    ES_PUT,
    ES_REPACK,
    ES_INFO,
    NUM_ES
};

enum eb_command_type
{
    EB_LS,
    EB_MERGE,
    EB_RM,
    EB_REWRITE,
    NUM_EB
};

template <typename T> struct edit_command
{
    T cmd;
    const char* name;
    bool rw;
    int min_args, max_args;
};

static edit_command<es_command_type> es_commands[] =
{
    { ES_LS,      "ls",      false, 0, 0, },
    { ES_GET,     "get",     false, 1, 2, },
    { ES_PUT,     "put",     true,  1, 2, },
    { ES_RM,      "rm",      true,  1, 1, },
    { ES_REPACK,  "repack",  false, 0, 0, },
    { ES_INFO,    "info",    false, 0, 0, },
};

static edit_command<eb_command_type> eb_commands[] =
{
    { EB_LS,       "ls",      false, 0, 2, },
    { EB_MERGE,    "merge",   false, 1, 1, },
    { EB_RM,       "rm",      true,  1, 1 },
    { EB_REWRITE,  "rewrite", true,  0, 1 },
};

#define FAIL(...) do { fprintf(stderr, __VA_ARGS__); return; } while (0)
static void _edit_save(int argc, char **argv)
{
    if (argc <= 1 || !strcmp(argv[1], "help"))
    {
        printf("Usage: crawl --edit-save <name> <command>, where <command> may be:\n"
               "  ls                          list the chunks\n"
               "  info                        list detailed chunk and frag info\n"
               "  get <chunk> [<chunkfile>]   extract a chunk into <chunkfile>\n"
               "  put <chunk> [<chunkfile>]   import a chunk from <chunkfile>\n"
               "     <chunkfile> defaults to \"chunk\"; use \"-\" for stdout/stdin\n"
               "  rm <chunk>                  delete a chunk\n"
               "  repack                      defrag and reclaim unused space\n"
             );
        return;
    }
    const char *name = argv[0];
    const char *cmdn = argv[1];

    es_command_type cmd = NUM_ES;
    bool rw;

    for (const auto &ec : es_commands)
        if (!strcmp(ec.name, cmdn))
        {
            if (argc < ec.min_args + 2)
                FAIL("Too few arguments for %s.\n", cmdn);
            else if (argc > ec.max_args + 2)
                FAIL("Too many arguments for %s.\n", cmdn);
            cmd = ec.cmd;
            rw = ec.rw;
            break;
        }
    if (cmd == NUM_ES)
        FAIL("Unknown command: %s.\n", cmdn);

    try
    {
        string filename = name;
        // Check for the exact filename first, then go by char name.
        if (!file_exists(filename))
            filename = get_savedir_filename(filename);
        package save(filename.c_str(), rw);

        if (cmd == ES_LS)
        {
            vector<string> list = save.list_chunks();
            sort(list.begin(), list.end(), numcmpstr);
            for (const string &s : list)
                printf("%s\n", s.c_str());
        }
        else if (cmd == ES_GET)
        {
            const char *chunk = argv[2];
            if (!*chunk || strlen(chunk) > MAX_CHUNK_NAME_LENGTH)
                FAIL("Invalid chunk name \"%s\".\n", chunk);
            if (!save.has_chunk(chunk))
                FAIL("No such chunk in the save file.\n");
            chunk_reader inc(&save, chunk);

            const char *file = (argc == 4) ? argv[3] : "chunk";
            FILE *f;
            if (strcmp(file, "-"))
                f = fopen_u(file, "wb");
            else
                f = stdout;
            if (!f)
                sysfail("Can't open \"%s\" for writing", file);

            char buf[16384];
            while (size_t s = inc.read(buf, sizeof(buf)))
                if (fwrite(buf, 1, s, f) != s)
                    sysfail("Error writing \"%s\"", file);

            if (f != stdout)
                if (fclose(f))
                    sysfail("Write error on close of \"%s\"", file);
        }
        else if (cmd == ES_PUT)
        {
            const char *chunk = argv[2];
            if (!*chunk || strlen(chunk) > MAX_CHUNK_NAME_LENGTH)
                FAIL("Invalid chunk name \"%s\".\n", chunk);

            const char *file = (argc == 4) ? argv[3] : "chunk";
            FILE *f;
            if (strcmp(file, "-"))
                f = fopen_u(file, "rb");
            else
                f = stdin;
            if (!f)
                sysfail("Can't read \"%s\"", file);
            chunk_writer outc(&save, chunk);

            char buf[16384];
            while (size_t s = fread(buf, 1, sizeof(buf), f))
                outc.write(buf, s);
            if (ferror(f))
                sysfail("Error reading \"%s\"", file);

            if (f != stdin)
                fclose(f);
        }
        else if (cmd == ES_RM)
        {
            const char *chunk = argv[2];
            if (!*chunk || strlen(chunk) > MAX_CHUNK_NAME_LENGTH)
                FAIL("Invalid chunk name \"%s\".\n", chunk);
            if (!save.has_chunk(chunk))
                FAIL("No such chunk in the save file.\n");

            save.delete_chunk(chunk);
        }
        else if (cmd == ES_REPACK)
        {
            package save2((filename + ".tmp").c_str(), true, true);
            for (const string &chunk : save.list_chunks())
            {
                char buf[16384];

                chunk_reader in(&save, chunk);
                chunk_writer out(&save2, chunk);

                while (plen_t s = in.read(buf, sizeof(buf)))
                    out.write(buf, s);
            }
            save2.commit();
            save.unlink();
            rename_u((filename + ".tmp").c_str(), filename.c_str());
        }
        else if (cmd == ES_INFO)
        {
            vector<string> list = save.list_chunks();
            sort(list.begin(), list.end(), numcmpstr);
            plen_t nchunks = list.size();
            plen_t frag = save.get_chunk_fragmentation("");
            plen_t flen = save.get_size();
            plen_t slack = save.get_slack();
            printf("Chunks: (size compressed/uncompressed, fragments, name)\n");
            for (const string &chunk : list)
            {
                int cfrag = save.get_chunk_fragmentation(chunk);
                frag += cfrag;
                int cclen = save.get_chunk_compressed_length(chunk);

                char buf[16384];
                chunk_reader in(&save, chunk);
                plen_t clen = 0;
                while (plen_t s = in.read(buf, sizeof(buf)))
                    clen += s;
                printf("%7d/%7d %3u %s\n", cclen, clen, cfrag, chunk.c_str());
            }
            // the directory is not a chunk visible from the outside
            printf("Fragmentation:    %u/%u (%4.2f)\n", frag, nchunks + 1,
                   ((float)frag) / (nchunks + 1));
            printf("Unused space:     %u/%u (%u%%)\n", slack, flen,
                   100 - (100 * (flen - slack) / flen));
            // there's also wasted space due to fragmentation, but since
            // it's linear, there's no need to print it
        }
    }
    catch (ext_fail_exception &fe)
    {
        fprintf(stderr, "Error: %s\n", fe.what());
    }
}

static save_version _read_bones_version(const string &filename)
{
    reader inf(filename);
    if (!inf.valid())
    {
        string error = "File doesn't exist: " + filename;
        throw corrupted_save(error);
    }

    inf.set_safe_read(true); // don't die on 0-byte bones
    // use lower-level call here, because read_ghost_header fixes up the version
    save_version version = get_save_version(inf);
    inf.close();
    return version;
}

static void _write_bones(const string &filename, vector<ghost_demon> ghosts)
{
    // TODO: duplicates some logic in files.cc
    FILE* ghost_file = lk_open_exclusive(filename);
    if (!ghost_file)
    {
        string error = "Couldn't write to bones file " + filename;
        throw corrupted_save(error);
    }
    writer outw(filename, ghost_file);

    write_ghost_version(outw);
    tag_write_ghosts(outw, ghosts);

    lk_close(ghost_file);
}

static void _bones_ls(const string &filename, const string name_match,
                                                            bool long_output)
{
    save_version v = _read_bones_version(filename);
    cout << "Bones file '" << filename << "', version " << v.major << "."
         << v.minor << ":\n";
    const vector<ghost_demon> ghosts = load_bones_file(filename, false);
    monster m;
    if (long_output)
    {
        init_monsters(); // no monster is valid without this
        init_spell_descs();
        init_spell_name_cache();
        m.reset();
        m.type = MONS_PROGRAM_BUG;
        m.base_monster = MONS_PHANTOM;
    }
    int count = 0;
    for (auto g : ghosts)
    {
        // TODO: partial name matching?
        if (name_match.size() && name_match != lowercase_string(g.name))
            continue;
        count++;
        if (long_output)
        {
            // TODO: line wrapping, some elements of this aren't meaningful at
            // the command line
            describe_info inf;
            m.set_ghost(g);
            m.ghost_init(false);
            m.type = MONS_PLAYER_GHOST;
            monster_info mi(&m);
            bool has_stat_desc = false;
            get_monster_db_desc(mi, inf, has_stat_desc);
            cout << "#######################\n"
                 << inf.title << "\n"
                 << inf.body.str() << "\n"
                 << inf.footer << "\n";
        }
        else
        {
            cout << std::setw(12) << std::left << g.name
                 << " XL" << std::setw(2) << g.xl << " "
                 << combo_type{species_type(g.species), job_type(g.job)}.abbr()
                 << "\n";
        }
    }
    if (!count)
    {
        if (name_match.size())
            cout << "No matching ghosts for " << name_match << ".\n";
        else
            cout << "Empty ghost file.\n";
    }
    else
        cout << count << " ghosts total\n";
}

static void _bones_rewrite(const string filename, const string remove, bool dedup)
{
    const vector<ghost_demon> ghosts = load_bones_file(filename, false);

    vector<ghost_demon> out;
    bool matched = false;
    const string remove_lower = lowercase_string(remove);
    map<string, int> ghosts_by_name;
    int dups = 0;

    for (auto g : ghosts)
    {
        if (dedup && ghosts_by_name.count(g.name)
                                            && ghosts_by_name[g.name] == g.xl)
        {
            dups++;
            continue;
        }
        if (lowercase_string(g.name) == remove_lower)
        {
            matched = true;
            continue;
        }
        out.push_back(g);
        ghosts_by_name[g.name] = g.xl;
    }
    if (matched || remove.size() == 0)
    {
        cout << "Rewriting '" << filename << "'";
        if (matched)
            cout << " without ghost '" << remove_lower << "'";
        if (dups)
            cout << ", " << dups << " duplicates removed";
        cout << "\n";
        unlink(filename.c_str());
        _write_bones(filename, out);
    }
    else
        cout << "No matching ghosts for '" << remove_lower << "'\n";
}

static void _bones_merge(const vector<string> files, const string out_name)
{
    vector<ghost_demon> out;
    for (auto filename : files)
    {
        auto ghosts = load_bones_file(filename, false);
        auto end = ghosts.end();
        if (out.size() + ghosts.size() > static_cast<unsigned int>(MAX_GHOSTS))
        {
            //cout << "ghosts " << out.size() + ghosts.size() - MAX_GHOSTS;
            cout << "Too many ghosts! Capping merge at " << MAX_GHOSTS << "\n";
            end = ghosts.begin() + (MAX_GHOSTS - out.size());
        }
        out.insert(out.end(), ghosts.begin(), end);
        if (end != ghosts.end())
            break;
    }
    if (file_exists(out_name))
        unlink(out_name.c_str());
    if (out.size() == 0)
        cout << "Writing empty bones file";
    else
        cout << "Writing " << out.size() << " ghosts";
    cout << " to " << out_name << "\n";
    _write_bones(out_name, out);
}

static void _edit_bones(int argc, char **argv)
{
    if (argc <= 1 || !strcmp(argv[1], "help"))
    {
        printf("Usage: crawl --bones <command> ARGS, where <command> may be:\n"
               "  ls <file> [<name>] [--long] List the ghosts in <file>\n"
               "                              --long shows full monster descriptions\n"
               "  merge <file1> <file2>       Merge two bones files together, rewriting into\n"
               "                              <file2>. Capped at %d; read in reverse order.\n"
               "  rm <file> <name>            Rewrite a ghost file without <name>\n"
               "  rewrite <file> [--dedup]    Rewrite a ghost file, fixing up version etc.\n",
               MAX_GHOSTS
             );
        return;
    }
    const char *cmdn = argv[0];
    const char *name = argv[1];

    eb_command_type cmd = NUM_EB;

    for (const auto &ec : eb_commands)
        if (!strcmp(ec.name, cmdn))
        {
            if (argc < ec.min_args + 2)
                FAIL("Too few arguments for %s.\n", cmdn);
            else if (argc > ec.max_args + 2)
                FAIL("Too many arguments for %s.\n", cmdn);
            cmd = ec.cmd;
            break;
        }
    if (cmd == NUM_EB)
        FAIL("Unknown command: %s.\n", cmdn);

    try
    {
        if (!file_exists(name))
            FAIL("'%s' doesn't exist!\n", name);

        if (cmd == EB_LS)
        {
            const bool long_out =
                           argc == 3 && !strcmp(argv[2], "--long")
                        || argc == 4 && !strcmp(argv[3], "--long");
            if (argc == 4 && !long_out)
                FAIL("Unknown extra option to ls: '%s'\n", argv[3]);
            const string name_match = argc == 3 && !long_out || argc == 4
                                    ? string(argv[2])
                                    : "";
            _bones_ls(name, lowercase_string(name_match), long_out);
        }
        else if (cmd == EB_REWRITE)
        {
            const bool dedup = argc == 3 && !strcmp(argv[2], "--dedup");
            if (argc == 3 && !dedup)
                FAIL("Unknown extra argument to rewrite: '%s'\n", argv[2]);
            _bones_rewrite(name, "", dedup);
        }
        else if (cmd == EB_RM)
        {
            const string name_match = argv[2];
            _bones_rewrite(name, name_match, false);
        }
        else if (cmd == EB_MERGE)
        {
            const string out_name = argv[2];
            _bones_merge({out_name, name}, out_name);
        }
    }
    catch (corrupted_save &err)
    {
        // not a corrupted save per se, just from the future. Try to load the
        // versioned bones file if it exists.
        if (err.version.valid() && err.version.is_future())
        {
            FAIL("Bones file '%s' is from the future (%d.%d), this instance of "
                 "crawl needs %d.%d.\n", name,
                    err.version.major, err.version.minor,
                    save_version::current_bones().major,
                    save_version::current_bones().minor);
        }
        else
            FAIL("Error: %s\n", err.what());
    }
    catch (ext_fail_exception &fe)
    {
        FAIL("Error: %s\n", fe.what());
    }
}

#undef FAIL

#ifdef USE_TILE_WEB
static void _write_colour_list(const vector<pair<int, int> > variable,
        const string &name)
{
    tiles.json_open_array(name);
    for (const auto &entry : variable)
    {
        tiles.json_open_object();
        tiles.json_write_int("value", entry.first);
        tiles.json_write_string("colour", colour_to_str(entry.second));
        tiles.json_close_object();
    }
    tiles.json_close_array();
}

static void _write_vcolour(const string &name, VColour colour)
{
    tiles.json_open_object(name);
    tiles.json_write_int("r", colour.r);
    tiles.json_write_int("g", colour.g);
    tiles.json_write_int("b", colour.b);
    if (colour.a != 255)
        tiles.json_write_int("a", colour.a);
    tiles.json_close_object();
}

static void _write_custom_colours(const string &name, const vector<colour_remapping> colours)
{
    tiles.json_open_array(name);
    for (const auto &entry : colours)
    {
        tiles.json_open_object();
        tiles.json_write_int("index", entry.colour_index);
        tiles.json_write_int("r", entry.colour_def.r);
        tiles.json_write_int("g", entry.colour_def.g);
        tiles.json_write_int("b", entry.colour_def.b);
        tiles.json_close_object();
    }
    tiles.json_close_array();
}

static void _write_minimap_colours()
{
    _write_vcolour("tile_unseen_col", Options.tile_unseen_col);
    _write_vcolour("tile_floor_col", Options.tile_floor_col);
    _write_vcolour("tile_wall_col", Options.tile_wall_col);
    _write_vcolour("tile_mapped_floor_col", Options.tile_mapped_floor_col);
    _write_vcolour("tile_mapped_wall_col", Options.tile_mapped_wall_col);
    _write_vcolour("tile_door_col", Options.tile_door_col);
    _write_vcolour("tile_item_col", Options.tile_item_col);
    _write_vcolour("tile_monster_col", Options.tile_monster_col);
    _write_vcolour("tile_plant_col", Options.tile_plant_col);
    _write_vcolour("tile_upstairs_col", Options.tile_upstairs_col);
    _write_vcolour("tile_downstairs_col", Options.tile_downstairs_col);
    _write_vcolour("tile_branchstairs_col", Options.tile_branchstairs_col);
    _write_vcolour("tile_feature_col", Options.tile_feature_col);
    _write_vcolour("tile_water_col", Options.tile_water_col);
    _write_vcolour("tile_lava_col", Options.tile_lava_col);
    _write_vcolour("tile_trap_col", Options.tile_trap_col);
    _write_vcolour("tile_excl_centre_col", Options.tile_excl_centre_col);
    _write_vcolour("tile_excluded_col", Options.tile_excluded_col);
    _write_vcolour("tile_player_col", Options.tile_player_col);
    _write_vcolour("tile_deep_water_col", Options.tile_deep_water_col);
    _write_vcolour("tile_portal_col", Options.tile_portal_col);
    _write_vcolour("tile_transporter_col", Options.tile_transporter_col);
    _write_vcolour("tile_transporter_landing_col", Options.tile_transporter_landing_col);
    _write_vcolour("tile_explore_horizon_col", Options.tile_explore_horizon_col);

    _write_vcolour("tile_window_col", Options.tile_window_col);
}

void game_options::write_webtiles_options(const string& name)
{
    tiles.json_open_object(name);

    _write_colour_list(hp_colour, "hp_colour");
    _write_colour_list(mp_colour, "mp_colour");
    _write_colour_list(stat_colour, "stat_colour");

    _write_custom_colours("custom_text_colours", custom_text_colours);

    tiles.json_write_bool("tile_show_minihealthbar",
                          tile_show_minihealthbar);
    tiles.json_write_bool("tile_show_minimagicbar",
                          tile_show_minimagicbar);
    tiles.json_write_bool("tile_show_demon_tier",
                          tile_show_demon_tier);

    tiles.json_write_int("tile_map_pixels", tile_map_pixels);

    tiles.json_write_string("tile_display_mode", tile_display_mode);
    tiles.json_write_int("tile_cell_pixels", tile_cell_pixels);
    // 100 is the default scale, but make the cast explicit for clarity about
    // what is happening, and for future-proofing
    tiles.json_write_int("tile_viewport_scale", fixedp<int,100>(tile_viewport_scale).to_scaled());
    tiles.json_write_int("tile_map_scale", fixedp<int,100>(tile_map_scale).to_scaled());
    tiles.json_write_bool("tile_filter_scaling", tile_filter_scaling);
    tiles.json_write_bool("tile_water_anim", tile_water_anim);
    tiles.json_write_bool("tile_misc_anim", tile_misc_anim);
    tiles.json_write_bool("tile_realtime_anim", tile_realtime_anim);
    tiles.json_write_bool("tile_level_map_hide_messages",
            tile_level_map_hide_messages);
    tiles.json_write_bool("tile_level_map_hide_sidebar",
            tile_level_map_hide_sidebar);
    tiles.json_write_bool("tile_web_mouse_control", tile_web_mouse_control);
    tiles.json_write_string("tile_web_mobile_input_helper", tile_web_mobile_input_helper);
    tiles.json_write_bool("tile_menu_icons", tile_menu_icons);

    tiles.json_write_string("tile_font_crt_family",
            tile_font_crt_family);
    tiles.json_write_string("tile_font_stat_family",
            tile_font_stat_family);
    tiles.json_write_string("tile_font_msg_family",
            tile_font_msg_family);
    tiles.json_write_string("tile_font_lbl_family",
            tile_font_lbl_family);
    tiles.json_write_int("tile_font_crt_size", tile_font_crt_size);
    tiles.json_write_int("tile_font_stat_size", tile_font_stat_size);
    tiles.json_write_int("tile_font_msg_size", tile_font_msg_size);
    tiles.json_write_int("tile_font_lbl_size", tile_font_lbl_size);

    tiles.json_write_string("glyph_mode_font", glyph_mode_font);
    tiles.json_write_int("glyph_mode_font_size", glyph_mode_font_size);

    tiles.json_write_bool("show_game_time", show_game_time);

    // TODO: convert action_panel_show into a yes/no/never option. It would be
    // better to have a more straightforward way of disabling the panel
    // completely
    tiles.json_write_bool("action_panel_disabled",
            action_panel.empty());
    tiles.json_write_bool("action_panel_show",
            action_panel_show);
    tiles.json_write_int("action_panel_scale",
            action_panel_scale);
    tiles.json_write_string("action_panel_orientation",
            action_panel_orientation);
    tiles.json_write_string("action_panel_font_family",
            action_panel_font_family);
    tiles.json_write_int("action_panel_font_size",
            action_panel_font_size);
    tiles.json_write_bool("action_panel_glyphs", action_panel_glyphs);

    _write_minimap_colours();

    tiles.json_close_object();
}

static void _print_webtiles_options()
{
    Options.write_webtiles_options("");
    printf("%s\n", tiles.get_message().c_str());
}
#endif

static string _gametype_to_clo(game_type g)
{
    switch (g)
    {
    case GAME_TYPE_CUSTOM_SEED:
        return cmd_ops[CLO_SEED];
    case GAME_TYPE_TUTORIAL:
        return cmd_ops[CLO_TUTORIAL];
    case GAME_TYPE_ARENA:
        return cmd_ops[CLO_ARENA];
    case GAME_TYPE_SPRINT:
        return cmd_ops[CLO_SPRINT];
    case GAME_TYPE_DESCENT: // no CLO?
        return cmd_ops[CLO_DESCENT];
    case GAME_TYPE_HINTS: // no CLO?
    case GAME_TYPE_NORMAL:
    default:
        return "";
    }
}

static void _print_gametypes_json()
{
    JsonWrapper json(json_mkobject());

    for (int i = 0; i < NUM_GAME_TYPE; ++i)
    {
        auto gt = static_cast<game_type>(i);
        string c = _gametype_to_clo(gt);
        if (c != "")
            c = "-" + c;
        if (c != "" || gt == GAME_TYPE_NORMAL)
        {
            json_append_member(json.node, gametype_to_str(gt).c_str(),
                                                        json_mkstring(c));
        }
    }
    fprintf(stdout, "%s", json.to_string().c_str());
}

static bool _check_extra_opt(char* _opt)
{
    string opt(_opt);
    trim_string(opt);

    if (opt[0] == ':' || opt[0] == '<' || opt[0] == '{'
        || starts_with(opt, "L<") || starts_with(opt, "Lua{"))
    {
        fprintf(stderr, "An extra option can't use Lua (%s)\n",
                _opt);
        return false;
    }

    if (opt[0] == '#')
    {
        fprintf(stderr, "An extra option can't be a comment (%s)\n",
                _opt);
        return false;
    }

    if (opt.find_first_of('=') == string::npos)
    {
        fprintf(stderr, "An extra opt must contain a '=' (%s)\n",
                _opt);
        return false;
    }

    vector<string> parts = split_string(opt, "=");
    if (opt.find_first_of('=') == 0 || parts[0].length() == 0)
    {
        fprintf(stderr, "An extra opt must have an option name (%s)\n",
                _opt);
        return false;
    }

    return true;
}

bool parse_args(int argc, char **argv, bool rc_only)
{
    COMPILE_CHECK(ARRAYSZ(cmd_ops) == CLO_NOPS);

#ifndef DEBUG_STATISTICS
    const char *dbg_stat_err = "mapstat and objstat are available only in "
                               "DEBUG_STATISTICS builds.\n";
#endif

    if (crawl_state.command_line_arguments.empty())
    {
        crawl_state.command_line_arguments.insert(
            crawl_state.command_line_arguments.end(),
            argv, argv + argc);
    }

    string exe_path = _find_executable_path();

    if (!exe_path.empty())
        set_crawl_base_dir(exe_path.c_str());
    else
        set_crawl_base_dir(argv[0]);

    SysEnv.crawl_exe = get_base_filename(argv[0]);

    SysEnv.rcdirs.clear();
    SysEnv.map_gen_iters = 0;

    if (argc < 2)           // no args!
        return true;

    char *arg, *next_arg;
    int current = 1;
    bool nextUsed = false;
    int ecount;

    // initialise
    for (int i = 0; i < num_cmd_ops; i++)
         _arg_seen(static_cast<commandline_option_type>(i)) = false;

    if (SysEnv.cmd_args.empty())
    {
        for (int i = 1; i < argc; ++i)
            SysEnv.cmd_args.emplace_back(argv[i]);
    }

    bool seen_headless_ok = false;

    while (current < argc)
    {
        // get argument
        arg = argv[current];

        // next argument (if there is one)
        if (current+1 < argc)
            next_arg = argv[current+1];
        else
            next_arg = nullptr;

        nextUsed = false;

        // arg MUST begin with '-'
        char c = arg[0];
        if (c != '-')
        {
            fprintf(stderr,
                    "Option '%s' is invalid; options must be prefixed "
                    "with -\n\n", arg);
            return false;
        }

        // Look for match (we accept both -option and --option).
        if (arg[1] == '-')
            arg = &arg[2];
        else
            arg = &arg[1];

        // Mac app bundle executables get a process serial number
        if (strncmp(arg, "psn_", 4) == 0)
        {
            current++;
            continue;
        }

        int o;
        for (o = 0; o < num_cmd_ops; o++)
            if (strcasecmp(cmd_ops[o], arg) == 0)
                break;

        // Print the list of commandline options for "--help".
        if (o == CLO_HELP)
            return false;

        if (o == num_cmd_ops)
        {
            fprintf(stderr,
                    "Unknown option: %s\n\n", argv[current]);
            return false;
        }

        // Disallow options specified more than once.
        if (_arg_seen(static_cast<commandline_option_type>(o)))
        {
            fprintf(stderr, "Duplicate option: %s\n\n", argv[current]);
            return false;
        }

        // Set arg to 'seen'.
        _arg_seen(static_cast<commandline_option_type>(o)) = true;

        // Partially parse next argument.
        bool next_is_param = false;
        if (next_arg != nullptr
            && (next_arg[0] != '-' || strlen(next_arg) == 1))
        {
            next_is_param = true;
        }

        if (clo_headless_ok.count(static_cast<commandline_option_type>(o)) > 0)
            seen_headless_ok = true;

        // Take action according to the cmd chosen.
        switch (o)
        {
        case CLO_SCORES:
        case CLO_TSCORES:
        case CLO_VSCORES:
            if (!next_is_param)
                ecount = -1;            // default
            else // optional number given
            {
                ecount = atoi(next_arg);
                if (ecount < 1)
                    ecount = 1;

                if (ecount > SCORE_FILE_ENTRIES)
                    ecount = SCORE_FILE_ENTRIES;

                nextUsed = true;
            }

            if (!rc_only)
            {
                Options.sc_entries = ecount;

                if (o == CLO_TSCORES)
                    Options.sc_format = SCORE_TERSE;
                else if (o == CLO_VSCORES)
                    Options.sc_format = SCORE_VERBOSE;
                else if (o == CLO_SCORES)
                    Options.sc_format = SCORE_REGULAR;
            }
            break;

        case CLO_MAPSTAT:
        case CLO_OBJSTAT:
#ifdef DEBUG_STATISTICS
            if (o == CLO_MAPSTAT)
                crawl_state.map_stat_gen = true;
            else
                crawl_state.obj_stat_gen = true;
            enter_headless_mode();

            if (!SysEnv.map_gen_iters)
                SysEnv.map_gen_iters = 100;
            if (next_is_param)
            {
                SysEnv.map_gen_range.reset(new depth_ranges);
                try
                {
                    *SysEnv.map_gen_range =
                        depth_ranges::parse_depth_ranges(next_arg);
                }
                catch (const bad_level_id &err)
                {
                    end(1, false, "Error parsing depths: %s\n", err.what());
                }
                nextUsed = true;
            }
            break;
#else
            end(1, false, "%s", dbg_stat_err);
#endif
        case CLO_MAPSTAT_DUMP_DISCONNECT:
#ifdef DEBUG_STATISTICS
            crawl_state.map_stat_dump_disconnect = true;
#else
            end(1, false, "%s", dbg_stat_err);
#endif
        case CLO_ITERATIONS:
#ifdef DEBUG_STATISTICS
            if (!next_is_param || !isadigit(*next_arg))
                end(1, false, "Integer argument required for -%s\n", arg);
            else
            {
                SysEnv.map_gen_iters = atoi(next_arg);
                if (SysEnv.map_gen_iters < 1)
                    SysEnv.map_gen_iters = 1;
                else if (SysEnv.map_gen_iters > 10000)
                    SysEnv.map_gen_iters = 10000;
                nextUsed = true;
            }
#else
            end(1, false, "%s", dbg_stat_err);
#endif
            break;

        case CLO_FORCE_MAP:
#ifdef DEBUG_STATISTICS
            if (!next_is_param)
                end(1, false, "String argument required for -%s\n", arg);
            else
            {
                crawl_state.force_map = next_arg;
                nextUsed = true;
            }
#else
            end(1, false, "%s", dbg_stat_err);
#endif
            break;

        case CLO_ARENA:
            if (!rc_only)
            {
                Options.game.type = GAME_TYPE_ARENA;
                Options.restart_after_game = false;
            }
            if (next_is_param)
            {
                if (!rc_only)
                    Options.game.arena_teams = next_arg;
                nextUsed = true;
            }
            break;

        case CLO_DUMP_MAPS:
            crawl_state.dump_maps = true;
            break;

        case CLO_PLAYABLE_JSON:
            fprintf(stdout, "%s", playable_metadata_json().c_str());
            end(0);

        case CLO_BRANCHES_JSON:
            fprintf(stdout, "%s", branch_data_json().c_str());
            end(0);

        case CLO_TEST:
            // TODO: are there any tests/scripts that make sense without
            // headless mode in console?
            enter_headless_mode();
            crawl_state.test = true;
            if (next_is_param)
            {
                if (!(crawl_state.test_list = !strcmp(next_arg, "list")))
                    crawl_state.tests_selected = split_string(",", next_arg);
                nextUsed = true;
            }
            break;

#if defined(UNIX) || defined(USE_TILE_LOCAL)
        case CLO_HEADLESS:
            enter_headless_mode();
#ifdef USE_TILE_LOCAL
            break;
#else
            if (!next_is_param)
                break;
            // intentional fallthrough: let -headless optionally take a script
            // name (for non-local-tiles)
            seen_headless_ok = true;
#endif
#endif

        case CLO_SCRIPT:
            // TODO: are there any tests/scripts that make sense without
            // headless mode in console?
            enter_headless_mode();

            crawl_state.test   = true;
            crawl_state.script = true;
            crawl_state.script_args.clear();
            if (current < argc - 1)
            {
                crawl_state.tests_selected = split_string(",", next_arg);
                for (int extra = current + 2; extra < argc; ++extra)
                    crawl_state.script_args.emplace_back(argv[extra]);
                current = argc;
            }
            else
            {
                // should be unreachable for CLO_HEADLESS
                end(1, false,
                    "-script must specify comma-separated script names");
            }
            break;

        case CLO_BUILDDB:
            if (next_is_param)
                return false;
            crawl_state.build_db = true;
            enter_headless_mode();
            break;

        case CLO_RESET_CACHE:
            if (next_is_param)
                return false;
            crawl_state.use_des_cache = false;
            break;

        case CLO_GDB:
            crawl_state.no_gdb = 0;
            break;

        case CLO_NO_GDB:
        case CLO_NOGDB:
            crawl_state.no_gdb = "GDB disabled via the command line.";
            break;

        case CLO_MACRO:
            if (!next_is_param)
                return false;
            SysEnv.macro_dir = next_arg;
            nextUsed = true;
            break;

        case CLO_MORGUE:
            if (!next_is_param)
                return false;
            if (!rc_only)
                SysEnv.morgue_dir = next_arg;
            nextUsed = true;
            break;

        case CLO_SCOREFILE:
            if (!next_is_param)
                return false;
            if (!rc_only)
                SysEnv.scorefile = next_arg;
            nextUsed = true;
            break;

        case CLO_NAME:
            if (!next_is_param)
                return false;
            if (!rc_only)
            {
                Options.game.name = next_arg;
                crawl_state.default_startup_name = Options.game.name;
            }
            nextUsed = true;
            break;

        case CLO_RACE:
        case CLO_CLASS:
            if (!next_is_param)
                return false;

            if (!rc_only)
            {
                if (o == 2)
                    Options.game.species = _str_to_species(string(next_arg));

                if (o == 3)
                    Options.game.job = str_to_job(string(next_arg));
            }
            nextUsed = true;
            break;

        case CLO_RCDIR:
            // Always parse.
            if (!next_is_param)
                return false;

            SysEnv.add_rcdir(next_arg);
            nextUsed = true;
            break;

        case CLO_DIR:
            // Always parse.
            if (!next_is_param)
                return false;

            // n.b. this is overridden by an rc file setting for this -- maybe
            // it shouldn't be?
            _set_crawl_dir(next_arg);

            nextUsed = true;
            break;

        case CLO_RC:
            // Always parse.
            if (!next_is_param)
                return false;

            SysEnv.crawl_rc = next_arg;
            nextUsed = true;
            break;

        case CLO_HELP:
            // Shouldn't happen.
            return false;

        case CLO_VERSION:
            _print_version();
            end(0);

        case CLO_SAVE_VERSION:
            // Always parse.
            if (!next_is_param)
                return false;

            _print_save_version(next_arg);
            end(0);

        case CLO_SAVE_JSON:
            // Always parse.
            if (!next_is_param)
                return false;

            print_save_json(next_arg);
            end(0);

        case CLO_GAMETYPES_JSON:
            // Always parse.
            _print_gametypes_json();
            end(0);

        case CLO_EDIT_SAVE:
            // Always parse.
            _edit_save(argc - current - 1, argv + current + 1);
            end(0);

        case CLO_EDIT_BONES:
            _edit_bones(argc - current - 1, argv + current + 1);
            end(0);

        case CLO_SEED:
            if (!next_is_param)
            {
                // show seed choice menu
                Options.game.type = GAME_TYPE_CUSTOM_SEED;
                break;
            }

            if (!sscanf(next_arg, "%" SCNu64, &Options.seed_from_rc))
                return false;
            Options.seed = Options.seed_from_rc;
            nextUsed = true;
            break;

        case CLO_PREGEN:
            Options.pregen_dungeon = level_gen_type::full;
            break;

        case CLO_SPRINT:
            if (!rc_only)
                Options.game.type = GAME_TYPE_SPRINT;
            break;

        case CLO_DESCENT:
            if (!rc_only)
                Options.game.type = GAME_TYPE_DESCENT;
            break;

        case CLO_SPRINT_MAP:
            if (!next_is_param)
                return false;

            nextUsed               = true;
            crawl_state.sprint_map = next_arg;
            Options.game.map       = next_arg;
            break;

        case CLO_TUTORIAL:
            if (!rc_only)
                Options.game.type = GAME_TYPE_TUTORIAL;
            break;

        case CLO_NO_SAVE:
            if (!rc_only)
                Options.no_save = true;
            break;

        case CLO_NO_PLAYER_BONES:
            if (!rc_only)
                Options.no_player_bones = true;
            break;

#ifdef USE_TILE_WEB
        case CLO_WEBTILES_SOCKET:
            nextUsed          = true;
            tiles.m_sock_name = next_arg;
            break;

        case CLO_AWAIT_CONNECTION:
            tiles.m_await_connection = true;
            break;

        case CLO_PRINT_WEBTILES_OPTIONS:
            if (!rc_only)
            {
                _print_webtiles_options();
                end(0);
            }
            break;
#endif

        case CLO_PRINT_CHARSET:
            if (rc_only)
                break;
#ifdef DGAMELAUNCH
            // Tell DGL we don't use ancient charsets anymore. The glyph set
            // doesn't matter here, just the encoding.
            printf("UNICODE\n");
#else
            printf("This option is for DGL use only.\n");
#endif
            end(0);
            break;

        case CLO_THROTTLE:
            crawl_state.throttle = true;
            break;

        case CLO_NO_THROTTLE:
            crawl_state.throttle = false;
            break;

        case CLO_CLUA_MAX_MEMORY:
            if (!next_is_param)
                return false;

            if (!sscanf(next_arg, "%" SCNu64, &crawl_state.clua_max_memory_mb))
                return false;
            nextUsed = true;
            break;

        case CLO_EXTRA_OPT_FIRST:
            if (!next_is_param)
                return false;

            // Don't print the help message if the opt was wrong
            if (!_check_extra_opt(next_arg))
                return true;

            SysEnv.extra_opts_first.emplace_back(next_arg);
            nextUsed = true;

            // Can be used multiple times.
            _arg_seen(static_cast<commandline_option_type>(o)) = false;
            break;

        case CLO_EXTRA_OPT_LAST:
            if (!next_is_param)
                return false;

            // Don't print the help message if the opt was wrong
            if (!_check_extra_opt(next_arg))
                return true;

            SysEnv.extra_opts_last.emplace_back(next_arg);
            nextUsed = true;

            // Can be used multiple times.
            _arg_seen(static_cast<commandline_option_type>(o)) = false;
            break;
        }

        // Update position.
        current++;
        if (nextUsed)
            current++;
    }
    return !in_headless_mode() || seen_headless_ok;
}

///////////////////////////////////////////////////////////////////////
// system_environment

void system_environment::add_rcdir(const string &dir)
{
    string cdir = canonicalise_file_separator(dir);
    if (dir_exists(cdir))
        rcdirs.push_back(cdir);
    else
        end(1, false, "Cannot find -rcdir \"%s\"", cdir.c_str());
}

///////////////////////////////////////////////////////////////////////
// menu_sort_condition

menu_sort_condition::menu_sort_condition(menu_type _mt, int _sort)
    : mtype(_mt), sort(_sort), cmp()
{
}

menu_sort_condition::menu_sort_condition(const string &s)
    : mtype(menu_type::any), sort(-1), cmp()
{
    string cp = s;
    set_menu_type(cp);
    set_sort(cp);
    set_comparators(cp);
}

bool menu_sort_condition::matches(menu_type mt) const
{
    return mtype == menu_type::any || mtype == mt;
}

void menu_sort_condition::set_menu_type(string &s)
{
    static struct
    {
        const string mname;
        menu_type mtype;
    } menu_type_map[] =
      {
          { "any:",    menu_type::any       },
          { "inv:",    menu_type::invlist   },
          { "drop:",   menu_type::drop      },
          { "pickup:", menu_type::pickup    },
          { "know:",   menu_type::know      }
      };

    for (const auto &mi : menu_type_map)
    {
        const string &name = mi.mname;
        if (starts_with(s, name))
        {
            s = s.substr(name.length());
            mtype = mi.mtype;
            break;
        }
    }
}

void menu_sort_condition::set_sort(string &s)
{
    // Strip off the optional sort clauses and get the primary sort condition.
    string::size_type trail_pos = s.find(':');
    if (starts_with(s, "auto:"))
        trail_pos = s.find(':', trail_pos + 1);

    string sort_cond = trail_pos == string::npos? s : s.substr(0, trail_pos);

    trim_string(sort_cond);
    sort = _read_bool_or_number(sort_cond, sort, "auto:");

    if (trail_pos != string::npos)
        s = s.substr(trail_pos + 1);
    else
        s.clear();
}

void menu_sort_condition::set_comparators(string &s)
{
    init_item_sort_comparators(
        cmp,
        s.empty()? "equipped, basename, qualname, curse, qty" : s);
}
