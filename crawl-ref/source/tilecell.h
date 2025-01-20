#ifdef USE_TILE
#pragma once

#include "map-cell.h"
#include "tag-version.h"

enum halo_type: uint8_t
{
    HALO_NONE = 0,
    HALO_RANGE = 1,
    HALO_UMBRA = 2,
};

struct packed_cell
{
    // For anything that requires multiple dungeon tiles (such as waves)
    // These tiles will be placed directly on top of the bg tile.
    static constexpr size_t MAX_DNGN_OVERLAY = 16;
    FixedVector<int, MAX_DNGN_OVERLAY> dngn_overlay;
    // SSE2 / XMM registers = 128 bits or 16 bytes
    // Current size of 16x int32 == 512 bits aligns nicely
    COMPILE_CHECK(sizeof(FixedVector<int, MAX_DNGN_OVERLAY>) % 16 == 0);

    // This is directly copied from env.map_knowledge by viewwindow()
    // Here for packing / alignment purposes
    map_cell map_knowledge;
    // Logically should go with dngn_overlay, but that would pack worse
    short int num_dngn_overlay;
    halo_type halo;
    bool quad_glow: 1;
    bool old_blood: 1;
    bool is_highlighted_summoner: 1;
    bool is_bloody: 1;
    bool is_silenced: 1;
    bool is_sanctuary: 1;
    bool is_blasphemy: 1;
    bool is_liquefied: 1;
    bool mangrove_water: 1;
    bool awakened_forest: 1;
    bool has_bfb_corpse: 1;

    // Note: placed here to pack better
    uint8_t orb_glow;
    char blood_rotation;
    uint8_t travel_trail;
    uint8_t disjunct;

    tile_flavour flv;
    tileidx_t fg;
    tileidx_t bg;
    tileidx_t cloud;
    set<tileidx_t> icons;

    bool operator ==(const packed_cell &other) const;
    bool operator !=(const packed_cell &other) const { return !(*this == other); }

    packed_cell() : num_dngn_overlay(0), halo(HALO_NONE), quad_glow(false),
                    old_blood(false), is_highlighted_summoner(false),
                    is_bloody(false), is_silenced(false), is_sanctuary(false),
                    is_blasphemy(false), is_liquefied(false),
                    mangrove_water(false), awakened_forest(false),
                    has_bfb_corpse(false), orb_glow(0), blood_rotation(0),
                    travel_trail(0), disjunct(false), fg(0), bg(0), cloud(0)
                    {
                        dngn_overlay.fill(0);
                    }

    packed_cell(const packed_cell* c) : dngn_overlay(c->dngn_overlay),
                                        map_knowledge(c->map_knowledge),
                                        num_dngn_overlay(c->num_dngn_overlay),
                                        halo(c->halo),
                                        quad_glow(c->quad_glow),
                                        old_blood(c->old_blood),
                                        is_highlighted_summoner(c->is_highlighted_summoner),
                                        is_bloody(c->is_bloody),
                                        is_silenced(c->is_silenced),
                                        is_sanctuary(c->is_sanctuary),
                                        is_blasphemy(c->is_blasphemy),
                                        is_liquefied(c->is_liquefied),
                                        mangrove_water(c->mangrove_water),
                                        awakened_forest(c->awakened_forest),
                                        has_bfb_corpse(c->has_bfb_corpse),
                                        orb_glow(c->orb_glow),
                                        blood_rotation(c->blood_rotation),
                                        travel_trail(c->travel_trail),
                                        disjunct(c->disjunct),
                                        flv(c->flv),
                                        fg(c->fg),
                                        bg(c->bg),
                                        cloud(c->cloud)
                                        {}

    void clear();
    // Add an overlay if not already present
    void add_overlay(int tileidx);
};

class crawl_view_buffer;

// For a given location, pack any waves/ink/wall shadow tiles
// that require knowledge of the surrounding env cells.
void pack_cell_overlays(const coord_def &gc, crawl_view_buffer &vbuf);

#endif // TILECELL_H
