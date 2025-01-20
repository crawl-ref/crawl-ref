/**
 * @file
 * @brief Travel stuff
**/
#pragma once

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "command-type.h"
#include "daction-type.h"
#include "exclude.h"
#include "explore-stop-options.h"
#include "travel-defs.h"

class reader;
class writer;

enum run_check_type
{
    RCHECK_LEFT,
    RCHECK_FRONT,
    RCHECK_RIGHT,
};

enum run_dir_type
{
    RDIR_UP = 0,
    RDIR_UP_RIGHT,
    RDIR_RIGHT,
    RDIR_DOWN_RIGHT,
    RDIR_DOWN,
    RDIR_DOWN_LEFT,
    RDIR_LEFT,
    RDIR_UP_LEFT,
    RDIR_REST,
};

enum run_mode_type
{
    RMODE_INTERLEVEL     = -4, // Interlevel travel
    RMODE_EXPLORE_GREEDY = -3, // Explore + grab items
    RMODE_EXPLORE        = -2, // Exploring
    RMODE_TRAVEL         = -1, // Classic or Plain Old travel
    RMODE_NOT_RUNNING    = 0,  // must remain equal to 0
    RMODE_CONTINUE,
    RMODE_START,
    RMODE_WAIT_DURATION = 100,
    RMODE_REST_DURATION = 9999999, // just rest until fully healed
    RMODE_CONNECTIVITY,        // Pathfinding connectivity check, not running.
};

/* ***********************************************************************
 * Initialises the travel subsystem.
 */
void stop_running(bool clear_delays = true);
void travel_init_load_level();
void travel_init_new_level();

uint8_t is_waypoint(const coord_def &p);
command_type direction_to_command(int x, int y);
bool is_resting();
void explore_pickup_event(int did_pickup, int tried_pickup);
bool feat_is_traversable_now(dungeon_feature_type feat,
                             bool try_fallback = false,
                             bool assume_flight = false);
bool feat_is_traversable(dungeon_feature_type feat, bool try_fallback = false);
bool is_travelsafe_square(const coord_def& c,
                                  bool ignore_hostile = false,
                                  bool ignore_danger = false,
                                  bool try_fallback = false);

bool is_known_branch_id(branch_type branch);
bool is_unknown_stair(const coord_def &p);
bool is_unknown_transporter(const coord_def &p);

void fill_travel_point_distance(const coord_def& youpos,
                     vector<coord_def>* coords = nullptr);

bool is_stair_exclusion(const coord_def &p);

/* ***********************************************************************
 * Initiates explore - the character runs around the level to map it. Note
 * that the caller has to ensure that the level is mappable before calling
 * start_explore. start_explore may lock up the game on unmappable levels.
 * If grab_items is true, greedy explore is triggered - in greedy mode, explore
 * grabs items that are eligible for autopickup and visits (previously
 * unvisited) shops.
 */
void start_explore(bool grab_items = false);
void do_explore_cmd();

struct level_pos;
class level_id;

level_id find_up_level(level_id curr, bool up_branch = false);
level_id find_down_level(level_id curr);

void start_translevel_travel(const level_pos &pos);

void start_travel(const coord_def& p);

command_type travel();

int prevent_travel_to(const string &dungeon_feature_name);
void reset_travel_terrain();

// Sort dungeon features as appropriate.
int level_distance(level_id first, level_id second);
level_id find_deepest_explored(level_id curr);
bool branch_entered(branch_type branch);

bool can_travel_to(const level_id &lid);
bool can_travel_interlevel();

enum translevel_prompt_flags
{
    TPF_NO_FLAGS          = 0,

    TPF_ALLOW_WAYPOINTS   = 0x1,
    TPF_ALLOW_UPDOWN      = 0x2,
    TPF_REMEMBER_TARGET   = 0x4,
    TPF_SHOW_ALL_BRANCHES = 0x8,
    TPF_SHOW_PORTALS_ONLY = 0x10,

    TPF_DEFAULT_OPTIONS   = TPF_ALLOW_WAYPOINTS | TPF_ALLOW_UPDOWN
                                                | TPF_REMEMBER_TARGET,
};

level_pos prompt_translevel_target(int prompt_flags, string& dest_name);

// Magic numbers for point_distance:

// This square is a trap
const int PD_TRAP = -42;

// The user never wants to travel this square
const int PD_EXCLUDED = -20099;

// This square is within LOS radius of an excluded square
const int PD_EXCLUDED_RADIUS = -20100;

// This square has a damaging cloud
const int PD_CLOUD = -20101;

/* ***********************************************************************
 * Array of points on the map, each value being the distance the character
 * would have to travel to get there. Negative distances imply that the point
 * is a) a trap or hostile terrain or b) only reachable by crossing a trap or
 * hostile terrain.
 * ***********************************************************************
 * referenced in: travel - view
 * *********************************************************************** */
extern travel_distance_grid_t travel_point_distance;

////////////////////////////////////////////////////////////////////////////
// Structs for interlevel travel.

// Tracks items discovered by explore in this turn.
class LevelStashes;
class explore_discoveries
{
public:
    explore_discoveries();

    void found_feature(const coord_def &pos, dungeon_feature_type grid);
    void found_item(const coord_def &pos, const item_def &item);

    // Reports discoveries and stops exploration (if necessary).
    bool stop_explore() const;

private:
    template <class Z> struct named_thing
    {
        string name;
        Z thing;

        named_thing(const string &n, Z t) : name(n), thing(t) { }
        operator string () const { return name; }

        bool operator == (const named_thing<Z> &other) const
        {
            return name == other.name;
        }
    };

    bool can_autopickup;
    int es_flags;
    const LevelStashes *current_level;
    vector< named_thing<item_def> > items;
    vector< named_thing<int> > stairs;
    vector< named_thing<int> > portals;
    vector< named_thing<int> > shops;
    vector< named_thing<int> > altars;
    vector< named_thing<int> > runed_doors;
    vector< named_thing<int> > transporters;
    vector< named_thing<int> > runelights;

    vector<string> marker_msgs;
    vector<string> marked_feats;

private:
    template <class C> void say_any(const C &coll, const char *category) const;
    template <class citer> bool has_duplicates(citer, citer) const;

    string cleaned_feature_description(const coord_def &) const;
    void add_item(const item_def &item);
    void add_stair(const named_thing<int> &stair);
    vector<string> apply_quantities(const vector< named_thing<int> > &v) const;
    bool merge_feature(vector< named_thing<int> > &v,
                       const named_thing<int> &feat) const;
};

struct stair_info
{
public:
    enum stair_type
    {
        PHYSICAL,
        PLACEHOLDER,
        MAPPED,
    };

public:
    coord_def position;     // Position of stair
    dungeon_feature_type grid; // Grid feature of the stair.
    level_pos destination;  // The level and the position on the level this
                            // stair leads to. This may be a guess.
    int       distance;     // The distance travelled to reach this stair.
    bool      guessed_pos;  // true if we're not sure that 'destination' is
                            // correct.
    stair_type type;

    stair_info()
        : position(-1, -1), grid(DNGN_FLOOR), destination(),
          distance(-1), guessed_pos(true), type(PHYSICAL)
    {
    }

    void clear_distance() { distance = -1; }

    void save(writer&) const;
    void load(reader&);

    string describe() const;

    bool can_travel() const { return type != PLACEHOLDER; }
};

struct transporter_info
{
public:
    enum transporter_type
    {
        PHYSICAL,
        MAPPED,
    };

public:
    coord_def position;     // Position of transporter
    coord_def destination;  // The position on the level this transporter leads
                            // to.
    transporter_type type;

    transporter_info(const coord_def &p, const coord_def &d,
                     transporter_type t)
        : position(p), destination(d), type(t) {
    }

    transporter_info()
        : position(-1, -1), destination(), type(PHYSICAL) {
    }

    void save(writer&) const;
    void load(reader&);
};

// Information on a level that interlevel travel needs.
struct LevelInfo
{
    LevelInfo() : stairs(), excludes(), stair_distances(), id()
    {
        daction_counters.init(0);
    }

    void save(writer&) const;
    void load(reader&, int minorVersion);

    vector<stair_info> &get_stairs()
    {
        return stairs;
    }

    vector<transporter_info> &get_transporters()
    {
        return transporters;
    }

    stair_info *get_stair(const coord_def &pos);
    transporter_info *get_transporter(const coord_def &pos);
    bool empty() const;
    bool know_stair(const coord_def &pos) const;
    bool know_transporter(const coord_def &pos) const;
    int get_stair_index(const coord_def &pos) const;
    int get_transporter_index(const coord_def &pos) const;

    void clear_distances();
    void set_level_excludes();

    const exclude_set &get_excludes() const
    {
        return excludes;
    }

    // Returns the travel distance between two stairs. If either stair is nullptr,
    // or does not exist in our list of stairs, returns 0.
    int distance_between(const stair_info *s1, const stair_info *s2) const;

    void update_excludes();
    void update();              // Update LevelInfo to be correct for the
                                // current level.

    // Updates/creates a StairInfo for the stair at stairpos in grid coordinates
    void update_stair(const coord_def& stairpos, const level_pos &p,
                      bool guess = false);
    void update_transporter(const coord_def& transpos, const coord_def &dest);

    // Clears all stair info for stairs matching this grid type.
    void clear_stairs(dungeon_feature_type grid);

    // Returns true if the given branch is known to be accessible from the
    // current level.
    bool is_known_branch(uint8_t branch) const;

    FixedVector<int, NUM_DACTION_COUNTERS> daction_counters;

private:
    // Gets a list of coordinates of all player-known stairs on the current
    // level.
    static void get_stairs(vector<coord_def> &stairs);
    static void get_transporters(vector<coord_def> &transporters);

    void correct_stair_list(const vector<coord_def> &s);
    void correct_transporter_list(const vector<coord_def> &s);
    void update_stair_distances();
    void sync_all_branch_stairs();
    void sync_branch_stairs(const stair_info *si);
    void set_distance_between_stairs(int a, int b, int dist);
    void fixup();

private:
    vector<stair_info> stairs;
    vector<transporter_info> transporters;

    // Squares that are not safe to travel to.
    exclude_set excludes;

    vector<short> stair_distances;  // Dist between stairs
    level_id id;

    friend class TravelCache;

private:
    void create_placeholder_stair(const coord_def &, const level_pos &);
    void resize_stair_distances();
};

const int TRAVEL_WAYPOINT_COUNT = 100;
// Tracks all levels that the player has seen.
class TravelCache
{
public:
    void clear_distances();

    LevelInfo& get_level_info(const level_id &lev)
    {
        LevelInfo &li = levels[lev];
        li.id = lev;
        return li;
    }

    LevelInfo *find_level_info(const level_id &lev)
    {
        map<level_id, LevelInfo>::iterator i = levels.find(lev);
        return i != levels.end()? &i->second : nullptr;
    }

    void erase_level_info(const level_id& lev)
    {
        levels.erase(lev);
    }

    bool know_stair(const coord_def &c);
    bool know_transporter(const coord_def &c);
    bool know_level(const level_id &lev) const
    {
        return levels.count(lev);
    }
    vector<level_id> known_levels() const;

    const level_pos &get_waypoint(int number) const
    {
        return waypoints[number];
    }

    int get_waypoint_count() const;

    void set_level_excludes();

    void add_waypoint(int x = -1, int y = -1);
    void set_waypoint(int waynum, int x, int y);
    void delete_waypoint();
    uint8_t is_waypoint(const level_pos &lp) const;
    vector<string> get_waypoint_descs() const;
    void list_waypoints() const;
    void flush_invalid_waypoints();
    void update_waypoints() const;
    bool is_valid_waypoint(int waynum) const;

    void update_excludes();
    void update();
    void update_transporter(const coord_def &c);

    void save(writer&) const;
    void load(reader&, int minorVersion);

    bool is_known_branch(uint8_t branch) const;

    void update_daction_counters(); // of the current level

    unsigned int query_daction_counter(daction_type c);
    void clear_daction_counter(daction_type c);

private:
    void update_stone_stair(const coord_def &c);
    void fixup_levels();

private:
    typedef map<level_id, LevelInfo> travel_levels_map;
    travel_levels_map levels;
    level_pos waypoints[TRAVEL_WAYPOINT_COUNT];
};

// Handles travel and explore floodfill pathfinding. Does not do interlevel
// travel pathfinding directly (but is used internally by interlevel travel).
// * All coordinates are grid coords.
// * Do not reuse one travel_pathfind for different runmodes.
class travel_pathfind
{
public:
    travel_pathfind();
    virtual ~travel_pathfind();

    // Finds travel direction or explore target.
    // If fallback_explore is set, will consider temporary obstructions like
    // sealed doors as traversable
    coord_def pathfind(run_mode_type rt, bool fallback_explore = false);

    // For flood-fills (explore), sets starting (seed) square.
    void set_floodseed(const coord_def &seed, bool double_flood = false);

    // For regular travel, set starting point (usually the character's current
    // position) and destination.
    void set_src_dst(const coord_def &src, const coord_def &dst);

    // Set feature vector to use; if non-nullptr, also sets annotate_map to
    // true.
    void set_feature_vector(vector<coord_def> *features);

    // Extract features without pathfinding
    void get_features();

    // The next square to go to to move towards the travel destination. Return
    // value is undefined if pathfind was not called with RMODE_TRAVEL.
    const coord_def travel_move() const;

    // Square to go to for (greedy) explore. Return value is undefined if
    // pathfind was not called with RMODE_EXPLORE or RMODE_EXPLORE_GREEDY.
    const coord_def explore_target() const;

    inline void set_ignore_danger()
    {
        ignore_danger = true;
    }

    // Determine if the level is fully explored, when called after pathfind().
    int explore_status();

protected:
    bool is_greed_inducing_square(const coord_def &c) const;
    bool path_examine_point(const coord_def &c);
    virtual bool point_traverse_delay(const coord_def &c);
    virtual bool path_flood(const coord_def &c, const coord_def &dc);
    bool square_slows_movement(const coord_def &c);
    void check_square_greed(const coord_def &c);
    void good_square(const coord_def &c);

protected:
    static const int UNFOUND_DIST  = -30000;
    static const int INFINITE_DIST =  INFINITE_DISTANCE;

protected:
    run_mode_type runmode;

    // Where pathfinding starts, and the destination. Note that dest is not
    // relevant for explore!
    coord_def start, dest;

    // This is the square adjacent to the starting position to move
    // along the shortest path to the destination. Does *not* apply
    // for explore!
    coord_def next_travel_move;

    // True if flooding outwards from start square for explore.
    bool floodout, double_flood;

    // Set true in the second part of a double floodfill to completely ignore
    // hostile squares.
    bool ignore_hostile;

    // Set to true for Tiles mode clicking, so you can move one step
    // at a time through excluded areas and around stationary monsters.
    bool ignore_danger;

    // If true, use magic numbers in point distance array which can be
    // used to colour the level-map.
    bool annotate_map;

    // Stashes on this level (needed for greedy explore and to populate the
    // feature vector with stashes on the X level-map).
    const LevelStashes *ls;

    // Are we greedy exploring?
    bool need_for_greed;

    // Can we autopickup?
    bool autopickup;

    // Targets for explore and greedy explore.
    coord_def unexplored_place, greedy_place;

    // How far from player's location unexplored_place and greedy_place are.
    int unexplored_dist, greedy_dist;

    const int *refdist;

    // For double-floods, the points to restart floodfill from at the end of
    // the first flood.
    vector<coord_def> reseed_points;

    vector<coord_def> *features;

    // List of unexplored and unreachable points.
    set<coord_def> unreachables;

    travel_distance_col *point_distance;

    // How many points we'll consider next iteration.
    int next_iter_points;

    // How far we've travelled from (start_x, start_y), in moves (a diagonal move
    // is no longer than an orthogonal move).
    int traveled_distance;

    // Which index of the circumference array are we currently looking at?
    int circ_index;

    // Used by all instances of travel_pathfind. Happily, we do not need to be
    // re-entrant or thread-safe.
    static FixedVector<coord_def, GXM * GYM> circumference[2];

    // Attempt to path through temporary obstructions (like sealed doors)
    // due to the possibility they are no longer obstructing us
    bool try_fallback;
};

extern TravelCache travel_cache;

void do_interlevel_travel();

// Travel from a mouse click. Take one step if not safe. Attack if adjacent.
// If force is true, then the player will attack empty squares/open doors.
#ifdef USE_TILE
bool click_travel_safe(const coord_def &gc);
int click_travel(const coord_def &gc, bool force);
#endif

bool check_for_interesting_features();
void clear_level_target();

void clear_travel_trail();
int travel_trail_index(const coord_def& gc);

bool stairs_destination_is_excluded(const stair_info &si);
