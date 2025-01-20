/**
 * @file
 * @brief Misc function used to render the dungeon.
**/

#pragma once

#include <vector>

#include "defines.h"
#include "options.h"
#include "player.h" // player_stealth()
#include "viewgeom.h"

using std::vector;

bool mon_enemies_around(const monster* mons);
void seen_monsters_react(int stealth = player_stealth());

string describe_monsters_condensed(const vector<monster*>& monsters);

bool magic_mapping(int map_radius, int proportion, bool suppress_msg,
                   bool force = false, bool deterministic = false,
                   bool full_info = false, bool range_falloff = true,
                   coord_def origin = coord_def(-1, -1));

string screenshot();

colour_t viewmap_flash_colour();
bool view_update();
void view_update_at(const coord_def &pos);
class targeter;

static inline void scaled_delay(unsigned int ms)
{
    delay(ms * Options.view_delay / DEFAULT_VIEW_DELAY);
}

void animation_delay(unsigned int ms, bool do_refresh);

// beware, flash_view is broken for USE_TILE_LOCAL
void flash_view(use_animation_type a, colour_t colour,
                targeter *where = nullptr);
void flash_view_delay(use_animation_type a, colour_t colour, int delay,
                      targeter *where = nullptr);

enum animation_type
{
    ANIMATION_SHAKE_VIEWPORT,
    ANIMATION_BANISH,
    ANIMATION_ORB,
    NUM_ANIMATIONS
};

class animation
{
public:
    animation(): frames(10), frame_delay(50) { }
    virtual ~animation() { }

    virtual void init_frame(int frame)
    {
        UNUSED(frame);
    }
    virtual coord_def cell_cb(const coord_def &pos, int &colour) = 0;

    int frames;
    int frame_delay;
};

/**
 * A crawl_view_buffer renderer callback that can be injected into
 * viewwindow(), allowing the addition of new visual elements without
 * adding code directly to viewwindow() itself.
 */
class view_renderer
{
public:
    view_renderer() {}
    virtual ~view_renderer() {}

    virtual void render(crawl_view_buffer &vbuf) = 0;
};

#ifdef USE_TILE
void view_add_tile_overlay(const coord_def &gc, tileidx_t tile);
#endif
void view_add_glyph_overlay(const coord_def &gc, cglyph_t glyph);

void flash_tile(coord_def p, colour_t colour = WHITE, int delay = 50,
                tileidx_t tile = 0);
void draw_ring_animation(const coord_def& center, int radius, colour_t colour,
                         colour_t colour_alt = BLACK, int delay = 50);

void view_clear_overlays();

void run_animation(animation_type anim, use_animation_type type,
                   bool cleanup = true);
void viewwindow(bool show_updates = true, bool tiles_only = false,
                animation *a = nullptr, view_renderer *renderer = nullptr);
void draw_cell(screen_cell_t *cell, const coord_def &gc,
               bool anim_updates, int flash_colour);

void update_monsters_in_view();
bool handle_seen_interrupt(monster* mons, vector<string>* msgs_buf = nullptr);
void flush_comes_into_view();
void toggle_show_terrain();
void reset_show_terrain();

void handle_terminal_resize();
