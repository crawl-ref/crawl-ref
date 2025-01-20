#pragma once

#include <algorithm>
#include <unordered_set>
#include <vector>
#include <functional>

#include "ability-type.h"
#include "activity-interrupt-type.h"
#include "char-set-type.h"
#include "confirm-prompt-type.h"
#include "easy-confirm-type.h"
#include "explore-greedy-options.h"
#include "explore-stop-options.h"
#include "feature.h"
#include "fixedp.h"
#include "flang-t.h"
#include "flush-reason-type.h"
#include "kill-dump-options-type.h"
#include "lang-t.h"
#include "level-gen-type.h"
#include "maybe-bool.h"
#include "mon-dam-level-type.h"
#include "mpr.h"
#include "newgame-def.h"
#include "pattern.h"
#include "rc-line-type.h"
#include "screen-mode.h"
#include "skill-focus-mode.h"
#include "slot-select-mode.h"
#include "spell-type.h"
#include "tag-pref.h"
#include "travel-open-doors-type.h"
#include "wizard-option-type.h"

using std::vector;

enum autosac_type
{
    AS_NO,
    AS_YES,
    AS_PROMPT,
    AS_BEFORE_EXPLORE,
    AS_PROMPT_IGNORE,
};

enum monster_list_colour_type
{
    MLC_FRIENDLY,
    MLC_NEUTRAL,
    MLC_GOOD_NEUTRAL,
    MLC_TRIVIAL,
    MLC_EASY,
    MLC_TOUGH,
    MLC_NASTY,
    MLC_UNUSUAL,
    NUM_MLC
};

struct message_filter
{
    int             channel;        // Use -1 to match any channel.
    text_pattern    pattern;        // Empty pattern matches any message

    message_filter()
        : channel(-1), pattern("")
    {
    }

    message_filter(int ch, const string &s)
        : channel(ch), pattern(s)
    {
    }

    message_filter(const text_pattern &p) : channel(-1), pattern(p) { }

    message_filter(const string &s);

    bool operator== (const message_filter &mf) const
    {
        return channel == mf.channel && pattern == mf.pattern;
    }

    bool is_filtered(int ch, const string &s) const
    {
        bool channel_match = ch == channel || channel == -1;
        if (!channel_match || pattern.empty())
            return channel_match;
        return pattern.matches(s);
    }
};

struct sound_mapping
{
    sound_mapping()
        : interrupt_game(false)
    {
    }

    sound_mapping(const string &s);

    text_pattern pattern;
    string       soundfile;
    bool         interrupt_game;

    bool operator== (const sound_mapping &o) const
    {
        return pattern == o.pattern
                && soundfile == o.soundfile
                && interrupt_game == o.interrupt_game;
    }
};

struct colour_mapping
{
    colour_mapping()
        : tag("none"), colour(WHITE)
    {
    }

    colour_mapping(const string &s);

    string tag;
    text_pattern pattern;
    colour_t colour;
    bool operator== (const colour_mapping &o) const
    {
        return tag == o.tag && pattern == o.pattern && colour == o.colour;
    }
};

struct message_colour_mapping
{
    message_colour_mapping()
        : colour(MSGCOL_NONE)
    {
    }

    message_colour_mapping(const message_filter &f, msg_colour_type c)
        : message(f), colour(c)
    {
    }

    bool valid() const { return colour != MSGCOL_NONE; }

    message_colour_mapping(const string &s);

    message_filter message;
    msg_colour_type colour;
    bool operator== (const message_colour_mapping &o) const
    {
        return message == o.message && colour == o.colour;
    }
};

struct mlc_mapping
{
    mlc_mapping()
        : category(NUM_MLC), colour(-1)
    {
    }

    mlc_mapping(monster_list_colour_type t, int c)
        : category(t), colour(c)
    {
    }

    mlc_mapping(const string &s);

    bool operator== (const mlc_mapping &o) const
    {
        return category == o.category
            && (colour == o.colour
                // match -1 to make `-=` work a little more smoothly
                || o.colour == -1
                || colour == -1);
    }

    bool valid() const { return category >= 0 && category < NUM_MLC; }

    monster_list_colour_type category;
    int colour;
};

#ifdef USE_TILE
struct colour_remapping
{
    colour_remapping()
        : colour_index(NUM_TERM_COLOURS), colour_def(0, 0, 0)
    {
    }

    colour_remapping(int c, VColour col)
        : colour_index(c), colour_def(col)
    {
    }

    colour_remapping(const string &s);

    bool operator== (const colour_remapping &o) const
    {
        return colour_index == o.colour_index
                && colour_def.r == o.colour_def.r
                && colour_def.g == o.colour_def.g
                && colour_def.b == o.colour_def.b;
    }

    bool valid() const { return colour_index >= 0 && colour_index < NUM_TERM_COLOURS; }

    int colour_index;
    VColour colour_def;
};
#endif

struct flang_entry
{
    flang_t lang_id;
    int value;
};

enum use_animation_type
{
    UA_NONE             = 0,
    // projectile animations, from throwing weapons, fireball, etc
    UA_BEAM             = (1 << 0),
    // flashes the screen when trying to cast a spell beyond its range
    UA_RANGE            = (1 << 1),
    // flashes the screen on low hitpoint warning
    UA_HP               = (1 << 2),
    // flashes the screen on attempt to travel or rest with a monster in view
    UA_MONSTER_IN_SIGHT = (1 << 3),
    // various animations for picking up runes, gems, and the orb
    UA_PICKUP           = (1 << 4),
    // various monster spell/ability effects (slime creature merging, etc)
    UA_MONSTER          = (1 << 5),
    // various player spell/ability animation effects (shatter, etc)
    UA_PLAYER           = (1 << 6),
    // animation when entering certain branches (abyss, zot, etc)
    UA_BRANCH_ENTRY     = (1 << 7),
    // animations that can't be turned off (please don't use unless they can
    // be turned off some other way)
    UA_ALWAYS_ON        = (1 << 8),
};
DEF_BITFIELD(use_animations_type, use_animation_type);

class LineInput;
class GameOption;

struct opt_parse_state
{
    string raw; // raw input line
    bool valid = false;

    string key;
    string subkey;
    string raw_field; // keeps case
    string field; // normalized for case

    rc_line_type line_type = RCFILE_LINE_EQUALS;

    bool plus_equal() const
    {
        return line_type == RCFILE_LINE_PLUS;
    }

    bool caret_equal() const
    {
        return line_type == RCFILE_LINE_CARET;
    }

    void ignore_prepend()
    {
        if (line_type == RCFILE_LINE_CARET)
            line_type = RCFILE_LINE_PLUS;
    }

    bool add_equal() const
    {
        return line_type == RCFILE_LINE_PLUS || line_type == RCFILE_LINE_CARET;
    }

    bool minus_equal() const
    {
        return line_type == RCFILE_LINE_MINUS;
    }

    bool plain() const
    {
        return line_type == RCFILE_LINE_EQUALS;
    }

    bool is_valid_option_line() const
    {
        return valid && line_type != RCFILE_LINE_DIRECTIVE;
    }

    // for some reason lua uses a completely distinct set of enum values, I've
    // just pasted this slightly weird code in mostly unmodified
    int lua_mode() const
    {
        int setmode = 0;
        if (plus_equal())
            setmode = 1;
        if (minus_equal())
            setmode = -1;
        if (caret_equal())
            setmode = 2;
        return setmode;
    }
};

/// This class is used to separate out general option handling (parsing,
/// meta-state, include file loading, lua handling) from specific option field
/// handling; the latter should be implemented on the subclass `game_options`.
struct base_game_options
{
    // actual game state goes in game_options, option parsing and state here

    base_game_options();
    ~base_game_options();
    virtual void reset_options();

    base_game_options(base_game_options const& other);
    base_game_options(base_game_options &&other) noexcept;
    base_game_options& operator=(base_game_options const& other);

    void read_options(LineInput &, bool runscripts,
                      bool clear_aliases = true);
    void read_option_line(const string &s, bool runscripts = false);
    virtual bool read_custom_option(opt_parse_state &state, bool runscripts);
    opt_parse_state parse_option_line(const string &str);

    // store for option settings that do not match a defined option value, and
    // are not handled by `c_process_lua_option`. They are still stored, and
    // can be used by lua scripts.
    map<string, string> named_options;

    bool prefs_dirty;
    string      filename;     // The name of the file containing options.
    string      basefilename; // Base (pathless) file name
    int         line_num;     // Current line number being processed.

    // Fix option values if necessary, specifically file paths.
    void reset_loaded_state();
    vector<GameOption*> get_option_behaviour() const
    {
        return option_behaviour;
    }
    void merge(const base_game_options &other);
    GameOption *option_from_name(string name) const
    {
        auto o = options_by_name.find(name);
        if (o == options_by_name.end())
            return nullptr;
        return o->second;
    }

    // silly c++ standard approach to has_key
    int count(const string &name) const
    {
        return options_by_name.count(name);
    }

    GameOption &operator[] (const string &name) const
    {
        auto o = option_from_name(name);
        ASSERT(o);
        return *o;
    }

    virtual void reset_aliases(bool clear=true);
    void include(const string &file, bool resolve, bool runscripts);
    string resolve_include(const string &file, const char *type = "");
    bool was_included(const string &file) const;
    static string resolve_include(string including_file, string included_file,
                            const vector<string> *rcdirs = nullptr);
    void set_from_defaults(const string &opt);

    void report_error(PRINTF(1, ));

    // arguable which class this should be on
    vector<string> terp_files; // Lua files to load for luaterp
    vector<string> additional_macro_files;

protected:
    map<string, string> aliases;
    map<string, string> variables;
    set<string> constants; // Variables that can't be changed
    set<string> included;  // Files we've included already.

    vector<GameOption*> option_behaviour;
    map<string, GameOption*> options_by_name;
    virtual const vector<GameOption*> build_options_list();
    map<string, GameOption*> build_options_map(const vector<GameOption*> &opts);

    string unalias(const string &key) const;
    string expand_vars(const string &field) const;
    void add_alias(const string &alias, const string &name);
    void set_option_fragment(const string &s, bool prepend);
};

/// This class implements most of crawl's options as well as their state.
/// (Parsing, option handling state, etc. is separated out into the superclass.)
/// An instance with the current options is canonically available via the static
/// `Options`.
struct game_options : public base_game_options
{
public:
    game_options();
    void reset_options() override;
    void reset_paths();
    void reset_aliases(bool clear=true) override;
    void fixup_options();
    const vector<GameOption*> build_options_list() override;

    bool read_custom_option(opt_parse_state &state, bool runscripts) override;

    void split_parse(const opt_parse_state &state,
                    const string &separator,
                    void (game_options::*add)(const string &, bool),
                    void (game_options::*remove)(const string &),
                    bool case_sensitive);

#ifdef USE_TILE_WEB
    void write_webtiles_options(const string &name);
#endif
    void write_prefs(FILE *f); // should be in superclass

public:
    // View options
    map<dungeon_feature_type, feature_def> feature_colour_overrides;
    map<dungeon_feature_type, FixedVector<char32_t, 2> > feature_symbol_overrides;
    map<monster_type, cglyph_t> mon_glyph_overrides;
    char32_t cset_override[NUM_DCHAR_TYPES];
    typedef pair<string, cglyph_t> item_glyph_override_type;
    vector<item_glyph_override_type > item_glyph_overrides;
    map<string, cglyph_t> item_glyph_cache;


    string crawl_dir_option;
    string save_dir_option;
    string macro_dir_option;
    string      save_dir;       // Directory where saves and bones go.
    string      macro_dir;      // Directory containing macro.txt
    string      morgue_dir;     // Directory where character dumps and morgue
                                // dumps are saved. Overrides crawl_dir.
    string      shared_dir;     // Directory where the logfile, scores and bones
                                // are stored. On a multi-user system, this dir
                                // should be accessible by different people.

    uint64_t    seed;           // Non-random games.
    string game_seed; // string version of the rc option
    uint64_t    seed_from_rc;
    level_gen_type pregen_dungeon;

#ifdef DGL_SIMPLE_MESSAGING
    bool        messaging;      // Check for messages.
#endif

    bool        suppress_startup_errors;

    bool        mouse_input;
    bool        menu_arrow_control;

    int         view_max_width;
    int         view_max_height;
    int         mlist_min_height;
    int         msg_min_height;
    int         msg_max_height;
    int         msg_webtiles_height;
    bool        mlist_allow_alternate_layout;
    bool        monster_item_view_coordinates;
    vector<text_pattern> monster_item_view_features;
    bool        messages_at_top;
    bool        msg_condense_repeats;
    bool        msg_condense_short;

    // The view lock variables force centering the viewport around the PC @
    // at all times (the default). If view locking is not enabled, the viewport
    // scrolls only when the PC hits the edge of it.
    bool        view_lock_x;
    bool        view_lock_y;

    // For an unlocked viewport, this will centre the viewport when scrolling.
    bool        centre_on_scroll;

    // If symmetric_scroll is set, for diagonal moves, if the view
    // scrolls at all, it'll scroll diagonally.
    bool        symmetric_scroll;

    // How far from the viewport edge is scrolling forced.
    int         scroll_margin_x;
    int         scroll_margin_y;

    // Whether exclusions and exclusion radius are visible in the viewport.
    bool        always_show_exclusions;

    int         autopickup_on; // can be -1, 0, or 1. XX refactor as enum
    bool        autopickup_starting_ammo;
    bool        default_manual_training;
    bool        default_show_all_skills;

    bool        show_newturn_mark;// Show underscore prefix in messages for new turn
    bool        show_game_time; // Show game time instead of player turns.
    bool        equip_bar; // Show equip bar instead of noise bar.
    bool        animate_equip_bar; // Animate colours in equip bar.

    FixedBitVector<NUM_OBJECT_CLASSES> autopickups; // items to autopickup
    bool        auto_switch;     // switch melee&ranged weapons according to enemy range
    travel_open_doors_type travel_open_doors; // open doors while exploring
    bool        easy_unequip;    // allow auto-removing of armour / jewellery
    bool        equip_unequip;   // Make 'W' = 'T', and 'P' = 'R'.
    bool        jewellery_prompt; // Always prompt for slot when changing jewellery.
    bool        easy_door;       // 'O', 'C' don't prompt with just one door.
    bool        warn_hatches;    // offer a y/n prompt when the player uses an escape hatch
    bool        enable_recast_spell; // Allow recasting spells with 'z' Enter.
    skill_focus_mode skill_focus; // is the focus skills available
    bool        auto_hide_spells; // hide new spells

    bool        note_all_skill_levels;  // take note for all skill levels (1-27)
    bool        note_skill_max;   // take note when skills reach new max
    string      user_note_prefix; // Prefix for user notes
    int         note_hp_percent;  // percentage hp for notetaking
    bool        note_xom_effects; // take note of all Xom effects
    bool        note_chat_messages; // log chat in Webtiles
    bool        note_dgl_messages; // log chat in DGL
    easy_confirm_type easy_confirm;    // make yesno() confirming easier
    confirm_prompt_type allow_self_target;      // yes, no, prompt
    bool        simple_targeting; // disable smart spell targeting
    bool        always_use_static_spell_targeters; // whether to always use
                                                   // static spell targeters
    bool        always_use_static_ability_targeters; // whether to always use
                                                     // static ability targeters

    int         colour[16];      // macro fg colours to other colours
    unsigned    background_colour; // select default background colour
    unsigned    foreground_colour; // select default foreground colour
    bool        use_terminal_default_colours; // inherit default colors from terminal
    msg_colour_type channels[NUM_MESSAGE_CHANNELS];  // msg channel colouring
    vector<string> use_animations_option;
    use_animations_type use_animations; // which animations to show
    bool        darken_beyond_range; // whether to darken squares out of range
    bool        show_blood; // whether to show blood or not
    int         food_snacking_frequency; // how often walking on food makes one eat
    int         fountain_line_frequency; // how often walking on fountains causes commentary
    bool        reduce_animations;   // if true, don't show interim steps for animations
    bool        drop_disables_autopickup;   // if true, automatically remove drops from autopickup

    vector<text_pattern> unusual_monster_items; // which monster items to
                                                // highlight as unusual

    int         hp_warning;      // percentage hp for danger warning
    int         magic_point_warning;    // percentage mp for danger warning
    bool        clear_messages;   // clear messages each turn
    bool        show_more;        // Show message-full more prompts.
    bool        small_more;       // Show one-char more prompts.
    unsigned    friend_highlight;     // Attribute for highlighting friendly monsters
    unsigned    neutral_highlight;    // Attribute for highlighting neutral monsters
    unsigned    unusual_highlight;    // Attribute for highlighting hostile
                                      // monsters with unusual items
    bool        blink_brightens_background; // Assume blink will brighten bg.
    maybe_bool  bold_brightens_foreground; // Assume bold will brighten fg.
    bool        best_effort_brighten_background; // Allow bg brighten attempts.
    bool        best_effort_brighten_foreground; // Allow fg brighten attempts.
    bool        allow_extended_colours; // Use more than 8 terminal colours.
    bool        macro_meta_entry; // Allow user to use numeric sequences when
                                  // creating macros
    int         autofight_warning;      // Amount of real time required between
                                        // two autofight commands
    bool        cloud_status;     // Whether to show a cloud status light
    bool        always_show_zot;  // Whether to always show the Zot timer
    bool        always_show_gems; // Whether to always show gem timers
    bool        more_gem_info;    // Whether to show gems breaking

#ifdef USE_TILE_WEB
    vector<object_class_type> action_panel;   // types of items to show on the panel
    vector<text_pattern> action_panel_filter; // what should be filtered out
    bool        action_panel_show_unidentified; // whether to show unidentified items
    bool        action_panel_show;
    int         action_panel_scale;       // the scale factor for resizing the panel
    string      action_panel_orientation; // whether to place the panel horizontally
    string      action_panel_font_family; // font used to display the quantities
    int         action_panel_font_size;
    bool        action_panel_glyphs;
#endif

    int         fire_items_start; // index of first item for fire command
    vector<unsigned> fire_order;  // missile search order for 'f' command
    unordered_set<spell_type, hash<int>> fire_order_spell;
    unordered_set<ability_type, hash<int>> fire_order_ability;
    bool        quiver_menu_focus;
    bool        launcher_autoquiver;

    unordered_set<int> force_spell_targeter; // spell types to always use a
                                             // targeter for
    unordered_set<int> force_ability_targeter; // ability types to always use a
                                               // targeter for

    bool        flush_input[NUM_FLUSH_REASONS]; // when to flush input buff

    bool        sounds_on;              // Allow sound effects to play
    bool        one_SDL_sound_channel;  // Limit to one SDL sound at once

    char_set_type  char_set;
    FixedVector<char32_t, NUM_DCHAR_TYPES> char_table;

    wizard_option_type wiz_mode;      // no, never, start in wiz mode
    wizard_option_type explore_mode;  // no, never, start in explore mode

    bool           no_save;    // don't use persistent save files
    bool           no_player_bones;   // don't save player's info in bones files

    // internal use only:
    int         sc_entries;      // # of score entries
    int         sc_format;       // Format for score entries

    vector<pair<int, int> > hp_colour;
    vector<pair<int, int> > mp_colour;
    vector<pair<int, int> > stat_colour;
    string enemy_hp_colour_option;
    FixedVector<int, MDAM_DEAD> enemy_hp_colour;

    string map_file_name;   // name of mapping file to use
    vector<pair<text_pattern, bool> > force_autopickup;
    vector<text_pattern> note_monsters;  // Interesting monsters
    vector<text_pattern> note_messages;  // Interesting messages
    vector<pair<text_pattern, string> > autoinscriptions;
    vector<text_pattern> note_items;     // Objects to note
    // Skill levels to note
    FixedBitVector<MAX_SKILL_LEVEL + 1> note_skill_levels;
    vector<pair<text_pattern, string>> auto_spell_letters;
    vector<pair<text_pattern, string>> auto_item_letters;
    vector<pair<text_pattern, string>> auto_ability_letters;

    bool        pickup_thrown;  // Pickup thrown missiles
    int         travel_delay;   // How long to pause between travel moves
    int         explore_delay;  // How long to pause between explore moves
    int         rest_delay;     // How long to pause between rest moves

    vector<string> travel_avoid_terrain_option;
    // Map of terrain types that are forbidden.
    FixedVector<int8_t,NUM_FEATURES> travel_avoid_terrain;


    bool        show_travel_trail;

    int         view_delay;

    bool        arena_dump_msgs;
    bool        arena_dump_msgs_all;
    bool        arena_list_eq;

    vector<message_filter> force_more_message;
    vector<message_filter> flash_screen_message;
    vector<text_pattern> confirm_action;

    unsigned    tc_reachable;   // Colour for squares that are reachable
    unsigned    tc_excluded;    // Colour for excluded squares.
    unsigned    tc_exclude_circle; // Colour for squares in the exclusion radius
    // Colour for squares which are safely traversable but that travel
    // considers forbidden, either because they are themselves forbidden by
    // travel options or exclusions or because they are only accessible through
    // such squares.
    unsigned    tc_forbidden;
    // Colour for squares that are not safely traversable (e.g. deep water,
    // lava, or traps)
    unsigned    tc_dangerous;
    unsigned    tc_disconnected;// Areas that are completely disconnected.
    vector<text_pattern> auto_exclude; // Automatically set an exclusion
                                       // around certain monsters.

    unsigned    evil_colour; // Colour for things player's god disapproves

    unsigned    remembered_monster_colour;  // Colour of remembered monsters
    unsigned    detected_monster_colour;    // Colour of detected monsters
    unsigned    detected_item_colour;       // Colour of detected items
    unsigned    status_caption_colour;      // Colour of captions in HUD.

    unsigned    heap_highlight;         // Highlight heaps of items
    unsigned    stab_highlight;         // Highlight monsters that are stabbable
    unsigned    may_stab_highlight;     // Highlight potential stab candidates
    unsigned    feature_item_highlight; // Highlight features covered by items.
    unsigned    trap_item_highlight;    // Highlight traps covered by items.

    // What is the minimum number of items in a stack for which
    // you show summary (one-line) information
    int         item_stack_summary_minimum;

    // explore_stop etc is stored as a simple vector of strings, and on updating
    // is parsed down into a bitfield
    vector<string> explore_stop_option;
    int         explore_stop;      // Stop exploring if a previously unseen
                                   // item comes into view
    vector<string> explore_greedy_visit_option;
    int explore_greedy_visit; // Set what type of items explore_greedy visits.

    // Don't stop greedy explore when picking up an item which matches
    // any of these patterns.
    vector<text_pattern> explore_stop_pickup_ignore;

    bool        explore_greedy;    // Explore goes after items as well.

    // How much more eager greedy-explore is for items than to explore.
    int         explore_item_greed;

    // How much autoexplore favours visiting squares next to walls.
    int         explore_wall_bias;

    // Wait for rest wait percent HP and MP before exploring.
    bool        explore_auto_rest;

    bool        travel_key_stop;   // Travel stops on keypress.

    bool        travel_one_unsafe_move; // Allow one unsafe move of auto travel

    vector<sound_mapping> sound_mappings;
    string sound_file_path;
    vector<colour_mapping> menu_colour_mappings;
    vector<message_colour_mapping> message_colour_mappings;
    vector<mlc_mapping> monster_list_colours_option;
    FixedVector<int, NUM_MLC> monster_list_colours;

    string sort_menus_option;
    vector<menu_sort_condition> sort_menus;

    bool        single_column_item_menus;

    bool        dump_on_save;       // Automatically dump character when saving.
    kill_dump_options dump_kill_places;   // How to dump place information for kills.
    int         dump_message_count; // How many old messages to dump

    int         dump_item_origins;  // Show where items came from?
    int         dump_item_origin_price;

    // Order of sections in the character dump.
    vector<string> dump_order;

    int         pickup_menu_limit;  // Over this number of items, menu for
                                    // pickup
    bool        prompt_menu;        // yesno prompt uses a menu popup
    bool        ability_menu;       // 'a'bility starts with a full-screen menu
    bool        spell_menu;         // 'z' starts with a full-screen menu
    bool        easy_floor_use;     // , selects the floor item if there's 1
    bool        bad_item_prompt;    // Confirm before using a bad consumable

    slot_select_mode assign_item_slot;   // How free slots are assigned
    maybe_bool  show_god_gift;      // Show {god gift} in item names

    maybe_bool  restart_after_game; // If true, Crawl will not close on game-end
                                    // If maybe, Crawl will restart only if the
                                    // CL options would bring up the startup
                                    // menu.
    bool        restart_after_save; // .. or on save
    bool        newgame_after_quit; // override the restart_after_game behaviour
                                    // to always start a new game on quit.

    bool        name_bypasses_menu; // should the menu be skipped if there is
                                    // a name set on game start
    bool        read_persist_options; // If true, Crawl will try to load
                                      // options from c_persist.options

    vector<text_pattern> drop_filter;

    map<string, FixedBitVector<NUM_ACTIVITY_INTERRUPTS>> activity_interrupts;
#ifdef DEBUG_DIAGNOSTICS
    FixedBitVector<NUM_DIAGNOSTICS> quiet_debug_messages;
#endif

    // Previous startup options
    bool        remember_name;      // Remember and reprompt with last name

    bool        dos_use_background_intensity;

    bool        use_fake_cursor;    // Draw a fake cursor instead of relying
                                    // on the term's own cursor.
    bool        use_fake_player_cursor;

    bool        show_player_species;

    int         level_map_cursor_step;  // The cursor increment in the level
                                        // map.

    bool        use_modifier_prefix_keys; // Treat '/' as SHIFT and '*' as CTRL

    // If the player prefers to merge kill records, this option can do that.
    int         kill_map[KC_NCATEGORIES];

    bool        rest_wait_both; // Stop resting only when both HP and MP are
                                // fully restored.

    bool        rest_wait_ancestor;// Stop resting only if the ancestor's HP
                                   // is fully restored.

    int         rest_wait_percent; // Stop resting after restoring this
                                   // fraction of HP or MP

    bool        regex_search; // whether to default to regex search for ^F
    bool        autopickup_search; // whether to annotate stash items with
                                   // autopickup status

    string language_option;
    lang_t              language;         // Translation to use.
    const char*         lang_name;        // Database name of the language.
    string fake_lang;
    vector<flang_entry> fake_langs;       // The fake language(s) in use.
    bool has_fake_lang(flang_t flang)
    {
        return any_of(begin(fake_langs), end(fake_langs),
                      [flang] (const flang_entry &f)
                      { return f.lang_id == flang; });
    }

    // -1 and 0 mean no confirmation, other possible values are 1,2,3 (see fail_severity())
    int         fail_severity_to_confirm;
    int         fail_severity_to_quiver;
#ifdef WIZARD
    // Parameters for fight simulations.
    string      fsim_mode;
    bool        fsim_csv;
    int         fsim_rounds;
    string      fsim_mons;
    vector<string> fsim_scale;
    vector<string> fsim_kit;
#endif  // WIZARD

#ifdef USE_TILE
    // TODO: have these present but ignored in non-tile builds
    string      tile_show_items; // show which item types in tile inventory
    bool        tile_skip_title; // wait for a key at title screen?
    bool        tile_menu_icons; // display icons in menus?

    // minimap colours
    VColour     tile_unseen_col;
    VColour     tile_floor_col;
    VColour     tile_wall_col;
    VColour     tile_mapped_floor_col;
    VColour     tile_mapped_wall_col;
    VColour     tile_door_col;
    VColour     tile_item_col;
    VColour     tile_monster_col;
    VColour     tile_plant_col;
    VColour     tile_upstairs_col;
    VColour     tile_downstairs_col;
    VColour     tile_branchstairs_col;
    VColour     tile_feature_col;
    VColour     tile_water_col;
    VColour     tile_lava_col;
    VColour     tile_trap_col;
    VColour     tile_excl_centre_col;
    VColour     tile_excluded_col;
    VColour     tile_player_col;
    VColour     tile_deep_water_col;
    VColour     tile_portal_col;
    VColour     tile_transporter_col;
    VColour     tile_transporter_landing_col;
    VColour     tile_explore_horizon_col;

    vector<colour_remapping> custom_text_colours;

    string      tile_display_mode;

    bool tile_show_player_species;
    tileidx_t   tile_player_tile;
    string tile_player_tile_option;
    pair<int, int> tile_weapon_offsets;
    pair<int, int> tile_shield_offsets;
    string tile_weapon_offsets_option;
    string tile_shield_offsets_option;
    string tile_tag_pref_option;
    tag_pref tile_tag_pref;

    VColour     tile_window_col;
#ifdef USE_TILE_LOCAL
    int         game_scale;
    // font settings
    string      tile_font_crt_file;
    string      tile_font_msg_file;
    string      tile_font_stat_file;
    string      tile_font_lbl_file;
    string      tile_font_tip_file;
#endif
#ifdef USE_TILE_WEB
    string      tile_font_crt_family;
    string      tile_font_msg_family;
    string      tile_font_stat_family;
    string      tile_font_lbl_family;
    string      glyph_mode_font;
    int         glyph_mode_font_size;
#endif
    int         tile_font_crt_size;
    int         tile_font_msg_size;
    int         tile_font_stat_size;
    int         tile_font_lbl_size;
    int         tile_font_tip_size;
#ifdef USE_TILE_LOCAL
#ifdef USE_FT
    bool        tile_font_ft_light;
#endif
    // window settings
    screen_mode tile_full_screen;
    int         tile_window_width;
    int         tile_window_height;
    int         tile_window_ratio;
    bool        tile_window_limit_size;
    maybe_bool  tile_use_small_layout;
#endif
    int         tile_sidebar_pixels;
    int         tile_cell_pixels;
    fixedp<>    tile_viewport_scale;
    fixedp<>    tile_map_scale;
    bool        tile_filter_scaling;
    int         tile_map_pixels;

    bool        tile_force_overlay;
    VColour     tile_overlay_col;           // Background color for message overlay
    int         tile_overlay_alpha_percent; // Background alpha percent for message overlay
    // display settings
    int         tile_update_rate;
    int         tile_runrest_rate;
    int         tile_key_repeat_delay;
    int         tile_tooltip_ms;

    bool        tile_show_minihealthbar;
    bool        tile_show_minimagicbar;
    bool        tile_show_demon_tier;
    string      tile_show_threat_levels;
    bool        tile_water_anim;
    bool        tile_misc_anim;
    vector<string> tile_layout_priority;
    monster_type tile_use_monster;
    bool        tile_grinch;
    vector<string> tile_player_status_icons;
#ifdef USE_TILE_WEB
    bool        tile_realtime_anim;
    bool        tile_level_map_hide_messages;
    bool        tile_level_map_hide_sidebar;
    bool        tile_web_mouse_control;
    string      tile_web_mobile_input_helper;
#endif
#endif // USE_TILE

    newgame_def game;      // Choices for new game.

private:
    void clear_feature_overrides();
    void clear_cset_overrides();
    void add_cset_override(dungeon_char_type dc, int symbol);
    void add_feature_override(const string &, bool prepend);
    void remove_feature_override(const string &);

    void add_message_colour_mappings(const string &, bool, bool);
    void add_message_colour_mapping(const string &, bool, bool);

    void set_default_activity_interrupts();
    void set_activity_interrupt(FixedBitVector<NUM_ACTIVITY_INTERRUPTS> &eints,
                                const string &interrupt);
    void set_activity_interrupt(const string &activity_name,
                                const string &interrupt_names,
                                bool append_interrupts,
                                bool remove_interrupts);
    void set_fire_order(const string &full, bool append, bool prepend);
    void add_fire_order_slot(const string &s, bool prepend);
    void set_fire_order_spell(const string &s, bool append, bool remove);
    void set_fire_order_ability(const string &s, bool append, bool remove);
    void set_menu_sort(const string &field);
    void update_enemy_hp_colour();
    void new_dump_fields(const string &text, bool add = true,
                         bool prepend = false);
    void do_kill_map(const string &from, const string &to);

    void update_explore_stop_conditions();
    void update_explore_greedy_visit_conditions();
    void update_use_animations();
    void update_travel_terrain();

    void add_mon_glyph_override(const string &, bool prepend);
    void remove_mon_glyph_override(const string &);
    cglyph_t parse_mon_glyph(const string &s) const;
    void add_item_glyph_override(const string &, bool prepend);
    void remove_item_glyph_override(const string &);
    bool set_lang(const char *s);
    void set_fake_langs(const string &input);
    void set_player_tile(const string &s);
    void set_tile_offsets(const string &s, bool set_shield);
    void add_force_spell_targeter(const string &s, bool prepend);
    void remove_force_spell_targeter(const string &s);
    void add_force_ability_targeter(const string &s, bool prepend);
    void remove_force_ability_targeter(const string &s);

    static const string interrupt_prefix;

};

char32_t get_glyph_override(int c);
object_class_type item_class_by_sym(char32_t c);

#ifdef DEBUG_GLOBALS
#define Options (*real_Options)
#endif
extern game_options  Options;

game_options &get_default_options();

static inline short macro_colour(short col)
{
    ASSERTM(col < NUM_TERM_COLOURS, "invalid color %hd", col);
    return col < 0 ? col : Options.colour[ col ];
}
