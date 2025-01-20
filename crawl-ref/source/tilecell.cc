#include "AppHdr.h"
#ifdef USE_TILE
#include "tilecell.h"

#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "level-state-type.h"
#include "terrain.h"
#include "tile-flags.h"
#include "rltiles/tiledef-dngn.h"
#include "rltiles/tiledef-main.h"
#include "tag-version.h"
#include "viewgeom.h"

void packed_cell::clear()
{
    num_dngn_overlay = 0;
    dngn_overlay.fill(0);
    fg = 0;
    bg = 0;
    cloud = 0;
    map_knowledge.clear();
    icons.clear();

    flv.floor_idx = 0;
    flv.wall_idx = 0;
    flv.feat_idx = 0;
    flv.floor = 0;
    flv.wall = 0;
    flv.feat = 0;
    flv.special = 0;

    is_highlighted_summoner   = false;
    is_bloody        = false;
    is_silenced      = false;
    halo             = HALO_NONE;
    is_sanctuary     = false;
    is_blasphemy     = false;
    is_liquefied     = false;
    mangrove_water = false;
    orb_glow         = 0;
    blood_rotation   = 0;
    old_blood        = false;
    travel_trail     = 0;
    quad_glow        = 0;
    disjunct         = 0;
}

bool packed_cell::operator ==(const packed_cell &other) const
{
    if (fg != other.fg) return false;
    if (bg != other.bg) return false;
    if (cloud != other.cloud) return false;
    if (map_knowledge != other.map_knowledge) return false;

    if (is_bloody != other.is_bloody) return false;
    if (is_silenced != other.is_silenced) return false;
    if (halo != other.halo) return false;
    if (is_sanctuary != other.is_sanctuary) return false;
    if (is_blasphemy != other.is_blasphemy) return false;
    if (is_liquefied != other.is_liquefied) return false;
    if (mangrove_water != other.mangrove_water) return false;
    if (awakened_forest != other.awakened_forest) return false;
    if (orb_glow != other.orb_glow) return false;
    if (blood_rotation != other.blood_rotation) return false;
    if (old_blood != other.old_blood) return false;
    if (travel_trail != other.travel_trail) return false;
    if (quad_glow != other.quad_glow) return false;
    if (disjunct != other.disjunct) return false;

    if (num_dngn_overlay != other.num_dngn_overlay) return false;
    for (int i = 0; i < num_dngn_overlay; ++i)
        if (dngn_overlay[i] != other.dngn_overlay[i]) return false;
    return true;
}

enum wave_type
{
    WV_NONE = 0,
    WV_SHALLOW,
    WV_DEEP,
};


void packed_cell::add_overlay(int tileidx)
{
    // Deduplicate existing identical overlays
    // There's a ton of ways to implement this.
    // This way, which does rely on the empty portion of the array being
    // zeroed, is friendly to auto-vectorization.
    // Clang 19.1.0 happily auto-vectorizes this on -O2
    // auto end = dngn_overlay.begin() + num_dngn_overlay;
    // or something like that might be more efficient for gcc though?
    auto end = dngn_overlay.end();
    bool not_present = find(dngn_overlay.begin(), end, tileidx) == end;
    if (not_present)
    {
        const int insert_pos = num_dngn_overlay++;
        // If we get an assert here, we'll either add a check to ignore
        // additional overlays or increase the overlay array size.
        dngn_overlay[insert_pos] = tileidx;
    }
}

typedef bool (*map_predicate) (const coord_def&, crawl_view_buffer& vbuf);

static coord_def overlay_directions[] =
{
    coord_def(0, -1),
    coord_def(1, 0),
    coord_def(0, 1),
    coord_def(-1, 0),
    coord_def(-1, -1),
    coord_def(1, -1),
    coord_def(1, 1),
    coord_def(-1, 1)
};

static void _add_directional_overlays(const coord_def& gc, crawl_view_buffer& vbuf,
                                      tileidx_t tile, map_predicate pred,
                                      uint8_t tile_mask = 0xFF)
{
    auto& cell = vbuf(gc).tile;

    uint8_t dir_mask = 0;

    for (int i = 0; i < 8; ++i)
    {
        if (!pred(gc + overlay_directions[i], vbuf)
            || feat_is_wall(cell.map_knowledge.feat()))
        {
            continue;
        }

        if (i > 3)
        {
            // Don't overlay corners if there's an overlay for one of the sides
            if (dir_mask & (1 << (i - 4)))
                continue;
            if (dir_mask & (1 << ((i - 1) % 4)))
                continue;
        }

        dir_mask |= 1 << i;
    }

    tileidx_t tileidx = tile;

    for (int i = 0; i < 8; ++i)
    {
        if ((tile_mask & (1 << i)) == 0)
            continue;

        if (dir_mask & (1 << i))
            cell.add_overlay(tileidx);

        tileidx++;
    }
}

static bool _feat_has_ink(coord_def gc, crawl_view_buffer& vbuf)
{
    return vbuf(gc).tile.map_knowledge.cloud() == CLOUD_INK;
}

static void _pack_shoal_waves(const coord_def &gc, crawl_view_buffer& vbuf)
{
    auto& cell = vbuf(gc).tile;

    // Add wave tiles on floor adjacent to shallow water.
    const auto feat = cell.map_knowledge.feat();
    const bool feat_has_ink = _feat_has_ink(gc, vbuf);

    if (feat == DNGN_DEEP_WATER && feat_has_ink)
    {
        cell.add_overlay(TILE_WAVE_INK_FULL);
        return;
    }

    // Wave tiles look quite bad over lava and are hidden by most solid features,
    // but allow them to show through solid features that have large amounts of
    // empty space in them.
    if (feat == DNGN_LAVA
        || (feat_is_solid(feat)
                && feat != DNGN_TREE
                && feat != DNGN_GRANITE_STATUE
                && feat != DNGN_METAL_STATUE
                && feat != DNGN_GRATE
                && feat != DNGN_RUNED_CLEAR_DOOR))
    {
        return;
    }

    const bool ink_only = (feat == DNGN_DEEP_WATER);

    wave_type north = WV_NONE, south = WV_NONE,
              east = WV_NONE, west = WV_NONE,
              ne = WV_NONE, nw = WV_NONE,
              se = WV_NONE, sw = WV_NONE;

    bool inkn = false, inks = false, inke = false, inkw = false,
         inkne = false, inknw = false, inkse = false, inksw = false;

    for (adjacent_iterator ri(gc, true); ri; ++ri)
    {
        if (ri->x < 0 || ri->x >= vbuf.size().x || ri->y < 0 || ri->y >= vbuf.size().y)
            continue;
        const map_cell& adj_knowledge = vbuf(*ri).tile.map_knowledge;
        if (!adj_knowledge.seen() && !adj_knowledge.mapped())
            continue;

        const bool ink = _feat_has_ink(*ri, vbuf);
        const auto adj_feat = adj_knowledge.feat();

        wave_type wt = WV_NONE;
        if (adj_feat == DNGN_SHALLOW_WATER)
        {
            // Adjacent shallow water is only interesting for
            // floor cells.
            if (!ink && feat == DNGN_SHALLOW_WATER)
                continue;

            if (feat != DNGN_SHALLOW_WATER)
                wt = WV_SHALLOW;
        }
        else if (adj_feat == DNGN_DEEP_WATER)
            wt = WV_DEEP;
        else
            continue;

        if (!ink_only)
        {
            if (ri->x == gc.x) // orthogonals
            {
                if (ri->y < gc.y)
                    north = wt;
                else
                    south = wt;
            }
            else if (ri->y == gc.y)
            {
                if (ri->x < gc.x)
                    west = wt;
                else
                    east = wt;
            }
            else // diagonals
            {
                if (ri->x < gc.x)
                {
                    if (ri->y < gc.y)
                        nw = wt;
                    else
                        sw = wt;
                }
                else
                {
                    if (ri->y < gc.y)
                        ne = wt;
                    else
                        se = wt;
                }
            }
        }
        if (!feat_has_ink && ink)
        {
            if (ri->x == gc.x) // orthogonals
            {
                if (ri->y < gc.y)
                    inkn = true;
                else
                    inks = true;
            }
            else if (ri->y == gc.y)
            {
                if (ri->x < gc.x)
                    inkw = true;
                else
                    inke = true;
            }
            else // diagonals
            {
                if (ri->x < gc.x)
                {
                    if (ri->y < gc.y)
                        inknw = true;
                    else
                        inksw = true;
                }
                else
                {
                    if (ri->y < gc.y)
                        inkne = true;
                    else
                        inkse = true;
                }
            }
        }
    }

    if (!ink_only)
    {
        // First check for shallow water.
        if (north == WV_SHALLOW)
            cell.add_overlay(TILE_WAVE_N);
        if (south == WV_SHALLOW)
            cell.add_overlay(TILE_WAVE_S);
        if (east == WV_SHALLOW)
            cell.add_overlay(TILE_WAVE_E);
        if (west == WV_SHALLOW)
            cell.add_overlay(TILE_WAVE_W);

        // Then check for deep water, overwriting shallow
        // corner waves, if necessary.

        // TILE_WAVE_DEEP_* is transition tile to floor;
        // deep water has another one for shallow water in TILE_WAVE_DEEP_* + 1
        int mod = (feat == DNGN_SHALLOW_WATER) ? 1 : 0;

        if (north == WV_DEEP)
            cell.add_overlay(TILE_WAVE_DEEP_N + mod);
        if (south == WV_DEEP)
            cell.add_overlay(TILE_WAVE_DEEP_S + mod);
        if (east == WV_DEEP)
            cell.add_overlay(TILE_WAVE_DEEP_E + mod);
        if (west == WV_DEEP)
            cell.add_overlay(TILE_WAVE_DEEP_W + mod);

        if (ne == WV_SHALLOW && !north && !east)
            cell.add_overlay(TILE_WAVE_CORNER_NE);
        else if (ne == WV_DEEP && north != WV_DEEP && east != WV_DEEP)
            cell.add_overlay(TILE_WAVE_DEEP_CORNER_NE + mod);
        if (nw == WV_SHALLOW && !north && !west)
            cell.add_overlay(TILE_WAVE_CORNER_NW);
        else if (nw == WV_DEEP && north != WV_DEEP && west != WV_DEEP)
            cell.add_overlay(TILE_WAVE_DEEP_CORNER_NW + mod);
        if (se == WV_SHALLOW && !south && !east)
            cell.add_overlay(TILE_WAVE_CORNER_SE);
        else if (se == WV_DEEP && south != WV_DEEP && east != WV_DEEP)
            cell.add_overlay(TILE_WAVE_DEEP_CORNER_SE + mod);
        if (sw == WV_SHALLOW && !south && !west)
            cell.add_overlay(TILE_WAVE_CORNER_SW);
        else if (sw == WV_DEEP && south != WV_DEEP && west != WV_DEEP)
            cell.add_overlay(TILE_WAVE_DEEP_CORNER_SW + mod);
    }

    // Overlay with ink sheen, if necessary.
    if (feat_has_ink)
        cell.add_overlay(TILE_WAVE_INK_FULL);
    else
    {
        if (inkn)
            cell.add_overlay(TILE_WAVE_INK_N);
        if (inks)
            cell.add_overlay(TILE_WAVE_INK_S);
        if (inke)
            cell.add_overlay(TILE_WAVE_INK_E);
        if (inkw)
            cell.add_overlay(TILE_WAVE_INK_W);
        if (inkne || inkn || inke)
            cell.add_overlay(TILE_WAVE_INK_CORNER_NE);
        if (inknw || inkn || inkw)
            cell.add_overlay(TILE_WAVE_INK_CORNER_NW);
        if (inkse || inks || inke)
            cell.add_overlay(TILE_WAVE_INK_CORNER_SE);
        if (inksw || inks || inkw)
            cell.add_overlay(TILE_WAVE_INK_CORNER_SW);
    }
}

static dungeon_feature_type _safe_feat(coord_def gc, crawl_view_buffer& vbuf)
{
    if (gc.x < 0 || gc.x >= vbuf.size().x || gc.y < 0 || gc.y >= vbuf.size().y)
        return DNGN_UNSEEN;

    return vbuf(gc).tile.map_knowledge.feat();
}

static bool _is_seen_land(coord_def gc, crawl_view_buffer& vbuf)
{
    const auto feat = _safe_feat(gc, vbuf);

    return feat != DNGN_UNSEEN && !feat_is_water(feat) && !feat_is_lava(feat)
           && feat != DNGN_MANGROVE;
}

static bool _is_seen_shallow(coord_def gc, crawl_view_buffer& vbuf)
{
    const auto feat = _safe_feat(gc, vbuf);

    if (!vbuf(gc).tile.map_knowledge.seen())
        return false;

    return feat == DNGN_SHALLOW_WATER || feat == DNGN_MANGROVE;
}

static tileidx_t _base_wave_tile(colour_t colour)
{
    switch (colour)
    {
        case BLACK: return TILE_DNGN_WAVE_N;
        case GREEN: return TILE_MURKY_WAVE_N;
        default: die("no %s deep water wave tiles", colour_to_str(colour).c_str());
    }
}

static void _pack_default_waves(const coord_def &gc, crawl_view_buffer& vbuf)
{
    auto& cell = vbuf(gc).tile;
    // Any tile on water with an adjacent solid tile will get an extra
    // bit of shoreline.
    auto feat = cell.map_knowledge.feat();
    auto colour = cell.map_knowledge.feat_colour();

    // Treat mangroves as though they were shallow water.
    if (cell.mangrove_water && feat == DNGN_MANGROVE)
        feat = DNGN_SHALLOW_WATER;

    if (!feat_is_water(feat) && !feat_is_lava(feat))
        return;

    if (feat == DNGN_DEEP_WATER && (colour == BLACK || colour == GREEN))
    {
        // +7 and -- reverse the iteration order
        int tile = _base_wave_tile(colour) + 7;
        for (adjacent_iterator ai(gc); ai; ++ai, --tile)
        {
            if (ai->x < 0 || ai->x >= vbuf.size().x || ai->y < 0 || ai->y >= vbuf.size().y)
                continue;
            if (_is_seen_shallow(*ai, vbuf))
                cell.add_overlay(tile);
        }
    }

    bool north = _is_seen_land(coord_def(gc.x, gc.y - 1), vbuf);
    bool west  = _is_seen_land(coord_def(gc.x - 1, gc.y), vbuf);
    bool east  = _is_seen_land(coord_def(gc.x + 1, gc.y), vbuf);

    if (north || west || east && (colour == BLACK || colour == LIGHTGREEN))
    {
        if (north)
            cell.add_overlay(TILE_SHORE_N);
        if (west)
            cell.add_overlay(TILE_SHORE_W);
        if (east)
            cell.add_overlay(TILE_SHORE_E);
        if (north && west)
            cell.add_overlay(TILE_SHORE_NW);
        if (north && east)
            cell.add_overlay(TILE_SHORE_NE);
    }
}

static bool _is_seen_wall(coord_def gc, crawl_view_buffer& vbuf)
{
    const auto feat = _safe_feat(gc, vbuf);
    return (feat_is_opaque(feat) || feat_is_wall(feat))
           && !feat_is_tree(feat) && feat != DNGN_UNSEEN;
}

static void _pack_wall_shadows(const coord_def &gc, crawl_view_buffer& vbuf,
                               tileidx_t tile)
{
    if (_is_seen_wall(gc, vbuf) || _safe_feat(gc, vbuf) == DNGN_GRATE)
        return;

    auto& cell = vbuf(gc).tile;

    bool ne = 0;
    bool nw = 0;
    int offset;

    // orthogonals
    if (_is_seen_wall(coord_def(gc.x - 1, gc.y), vbuf))
    {
        offset = _is_seen_wall(coord_def(gc.x - 1, gc.y - 1), vbuf) ? 0 : 5;
        cell.add_overlay(tile + offset);
        nw = 1;
    }
    if (_is_seen_wall(coord_def(gc.x, gc.y - 1), vbuf))
    {
        cell.add_overlay(tile + 2);
        ne = 1;
        nw = 1;
    }
    if (_is_seen_wall(coord_def(gc.x + 1, gc.y), vbuf))
    {
        offset = _is_seen_wall(coord_def(gc.x + 1, gc.y - 1), vbuf) ? 4 : 6;
        cell.add_overlay(tile + offset);
        ne = 1;
    }

    // corners
    if (nw == 0 && _is_seen_wall(coord_def(gc.x - 1, gc.y - 1), vbuf))
        cell.add_overlay(tile + 1);
    if (ne == 0 && _is_seen_wall(coord_def(gc.x + 1, gc.y - 1), vbuf))
        cell.add_overlay(tile + 3);
}

static bool _is_seen_slimy_wall(const coord_def& gc, crawl_view_buffer &vbuf)
{
    return _safe_feat(gc, vbuf) == DNGN_SLIMY_WALL;
}

static bool _is_seen_icy_wall(const coord_def& gc, crawl_view_buffer &vbuf)
{
    return feat_is_wall(_safe_feat(gc, vbuf))
           && vbuf(gc).tile.map_knowledge.flags & MAP_ICY;
}

void pack_cell_overlays(const coord_def &gc, crawl_view_buffer &vbuf)
{
    auto& cell = vbuf(gc).tile;

    if (cell.map_knowledge.feat() == DNGN_UNSEEN)
        return; // Don't put overlays on unseen tiles

    if (player_in_branch(BRANCH_SHOALS))
        _pack_shoal_waves(gc, vbuf);
    else
        _pack_default_waves(gc, vbuf);

    if (env.level_state & LSTATE_SLIMY_WALL
        && cell.map_knowledge.feat() != DNGN_SLIMY_WALL)
    {
        _add_directional_overlays(gc, vbuf, TILE_SLIME_OVERLAY,
                                  _is_seen_slimy_wall);
    }
    else if (env.level_state & LSTATE_ICY_WALL
             && cell.map_knowledge.flags & MAP_ICY)
    {
        if (feat_is_wall(cell.map_knowledge.feat()))
        {
            const int count = tile_dngn_count(TILE_DNGN_WALL_ICY_WALL_OVERLAY);
            cell.add_overlay(TILE_DNGN_WALL_ICY_WALL_OVERLAY
                    + (gc.y * GXM + gc.x) % count);
        }
        else
        {
            _add_directional_overlays(gc, vbuf, TILE_ICE_OVERLAY,
                                      _is_seen_icy_wall);
        }
    }
    else
    {
        tileidx_t shadow_tile = TILE_DNGN_WALL_SHADOW;
        if (player_in_branch(BRANCH_CRYPT) || player_in_branch(BRANCH_DEPTHS))
            shadow_tile = TILE_DNGN_WALL_SHADOW_DARK;
        _pack_wall_shadows(gc, vbuf, shadow_tile);
    }
}
#endif //TILECELL.CC
