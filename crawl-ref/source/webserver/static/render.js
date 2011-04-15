var overlaid_locs = [];
var cursor_locs = [];

// Debug helper
function mark_cell(x, y, mark)
{
    mark = mark || "m";

    if (get_tile_cache(x, y))
        get_tile_cache(x, y).mark = mark;

    dungeon_ctx.fillStyle = "red";
    dungeon_ctx.font = "12px monospace";
    dungeon_ctx.textAlign = "center";
    dungeon_ctx.textBaseline = "middle";
    dungeon_ctx.fillText(mark,
                         (x + 0.5) * dungeon_cell_w, (y + 0.5) * dungeon_cell_h);
}
function mark_all()
{
    for (x = 0; x < dungeon_cols; x++)
        for (y = 0; y < dungeon_rows; y++)
            mark_cell(x, y, x + "/" + y);
}

// This gets called by the messages sent by crawl
function c(x, y, cell)
{
    set_tile_cache(x, y, cell);

    render_cell(x, y);
}

function add_overlay(idx, x, y)
{
    draw_main(idx, x, y);
    overlaid_locs.push({x: x, y: y});
}

function clear_overlays()
{
    $.each(overlaid_locs, function(i, loc)
           {
               render_cell(loc.x, loc.y);
           });
    overlaid_locs = [];
}

function place_cursor(type, x, y)
{
    var old_loc = undefined;
    if (cursor_locs[type])
    {
        old_loc = cursor_locs[type];
    }

    cursor_locs[type] =
        {x: x, y: y};

    if (old_loc)
        render_cell(old_loc.x, old_loc.y);

    render_cursors(x, y);
}

function remove_cursor(type)
{
    var old_loc = undefined;
    if (cursor_locs[type])
    {
        old_loc = cursor_locs[type];
    }

    cursor_locs[type] = undefined;

    if (old_loc)
        render_cell(old_loc.x, old_loc.y);
}

// Render functions

function render_cursors(x, y)
{
    $.each(cursor_locs, function (type, loc)
           {
               if (loc && (loc.x == x) && (loc.y == y))
               {
                   var idx;

                   switch (type)
                   {
                   case CURSOR_TUTORIAL:
                       idx = TILEI_TUTORIAL_CURSOR;
                       break;
                   case CURSOR_MOUSE:
                       idx = TILEI_CURSOR;
                       // TODO: TILEI_CURSOR2 if not visible
                       // But do we even need a server-side mouse cursor?
                       break;
                   case CURSOR_MAP:
                       idx = TILEI_CURSOR;
                       break;
                   }

                   draw_icon(idx, x, y);
               }
           });
}

function render_cell(x, y)
{
    try
    {
        dungeon_ctx.fillStyle = "black";
        dungeon_ctx.fillRect(x * dungeon_cell_w, y * dungeon_cell_h,
                             dungeon_cell_w, dungeon_cell_h);

        var cell = get_tile_cache(x, y);

        if (!cell)
            return;

        cell.flv.s = cell.flv.s || 0;

        // cell is basically a packed_cell + doll + mcache entries
        draw_background(x, y, cell);

        var fg_idx = cell.fg & TILE_FLAG_MASK;
        var is_in_water = in_water(cell);

        // Canvas doesn't support applying an alpha gradient to an image while drawing;
        // so to achieve the same effect as in local tiles, it would probably be best
        // to pregenerate water tiles with the (inverse) alpha gradient built in.
        // This simply draws the lower half with increased transparency; for now,
        // it looks good enough.

        var draw_dolls = function () {
            if (cell.doll)
            {
                $.each(cell.doll, function (i, doll_part)
                       {
                           draw_player(doll_part[0], x, y, 0, 0, doll_part[1]);
                       });
            }

            if (cell.mcache)
            {
                $.each(cell.mcache, function (i, mcache_part)
                       {
                           draw_player(mcache_part[0], x, y, mcache_part[1], mcache_part[2]);
                       });
            }
        }

        if (is_in_water)
        {
            dungeon_ctx.save();
            dungeon_ctx.globalAlpha = cell.trans ? 0.7 : 1.0;

            set_nonsubmerged_clip(x, y, 20);

            draw_dolls();

            dungeon_ctx.restore();

            dungeon_ctx.save();
            dungeon_ctx.globalAlpha = cell.trans ? 0.1 : 0.3;
            set_submerged_clip(x, y, 20);

            draw_dolls();

            dungeon_ctx.restore();
        }
        else
        {
            dungeon_ctx.save();
            dungeon_ctx.globalAlpha = cell.trans ? 0.7 : 1.0;

            draw_dolls();

            dungeon_ctx.restore();
        }

        draw_foreground(x, y, cell);

        render_cursors(x, y);

        // Debug helper
        if (cell.mark)
        {
            dungeon_ctx.fillStyle = "red";
            dungeon_ctx.font = "12px monospace";
            dungeon_ctx.textAlign = "center";
            dungeon_ctx.textBaseline = "middle";
            dungeon_ctx.fillText(mark,
                                 (x + 0.5) * dungeon_cell_w, (y + 0.5) * dungeon_cell_h);
        }
    }
    catch (err)
    {
        console.error("Error while drawing cell " + obj_to_str(cell)
                      + " at " + x + "/" + y + ": " + err);
    }
}

function set_submerged_clip(cx, cy, water_level)
{
    var x = dungeon_cell_w * cx;
    var y = dungeon_cell_h * cy;

    dungeon_ctx.beginPath();
    dungeon_ctx.rect(0, y + water_level, dungeon_cell_w * dungeon_cols,
                     dungeon_cell_h * dungeon_cols - y - water_level);
    dungeon_ctx.clip();
}

function set_nonsubmerged_clip(cx, cy, water_level)
{
    var x = dungeon_cell_w * cx;
    var y = dungeon_cell_h * cy;

    dungeon_ctx.beginPath();
    dungeon_ctx.rect(0, 0, dungeon_cell_w * dungeon_cols, y + water_level);
    dungeon_ctx.clip();
}

// Much of the following is more or less directly copied from tiledgnbuf.cc
function in_water(cell)
{
    return ((cell.bg & TILE_FLAG_WATER) && !(cell.fg & TILE_FLAG_FLYING));
}

function draw_blood_overlay(x, y, cell, is_wall)
{
    if (cell.liquefied)

    {
        offset = cell.flv.s % tile_dngn_count(TILE_LIQUEFACTION);
        draw_dngn(TILE_LIQUEFACTION + offset, x, y);
    }
    else if (cell.bloody)

    {
        if (is_wall)

        {
            basetile = TILE_WALL_BLOOD_S + tile_dngn_count(TILE_WALL_BLOOD_S)
                * cell.bloodrot;
        }
        else
            basetile = TILE_BLOOD;
        offset = cell.flv.s % tile_dngn_count(basetile);
        draw_dngn(basetile + offset, x, y);
    }
    else if (cell.moldy)
    {
        offset = cell.flv.s % tile_dngn_count(TILE_MOLD);
        draw_dngn(TILE_MOLD + offset, x, y);
    }
    else if (cell.glowing_mold)
    {
        offset = cell.flv.special % tile_dngn_count(TILE_GLOWING_MOLD);
        draw_dngn(TILE_GLOWING_MOLD + offset, x, y);
    }
}

function draw_background(x, y, cell)
{
    bg = cell.bg;
    bg_idx = cell.bg & TILE_FLAG_MASK;

    if (cell.swtree && bg_idx > TILE_DNGN_UNSEEN)
        draw_dngn(TILE_DNGN_SHALLOW_WATER, x, y);

    if (bg_idx >= TILE_DNGN_WAX_WALL)
        draw_dngn(cell.flv.f, x, y); // f = floor

    // Draw blood beneath feature tiles.
    if (bg_idx > TILE_WALL_MAX)
        draw_blood_overlay(x, y, cell);

    draw_dngn(bg_idx, x, y, cell.swtree);

    if (bg_idx > TILE_DNGN_UNSEEN)

    {
        if (bg & TILE_FLAG_WAS_SECRET)
            draw_dngn(TILE_DNGN_DETECTED_SECRET_DOOR, x, y);

        // Draw blood on top of wall tiles.
        if (bg_idx <= TILE_WALL_MAX)
            draw_blood_overlay(x, y, cell, bg_idx >= TILE_FLOOR_MAX);

        // Draw overlays
        if (cell.ov)
        {
            $.each(cell.ov, function (i, overlay)
                   {
                       draw_dngn(overlay, x, y);
                   });
        }

        if (!(bg & TILE_FLAG_UNSEEN))

        {
            if (bg & TILE_FLAG_KRAKEN_NW)
                draw_dngn(TILE_KRAKEN_OVERLAY_NW, x, y);
            else if (bg & TILE_FLAG_ELDRITCH_NW)
                draw_dngn(TILE_ELDRITCH_OVERLAY_NW, x, y);
            if (bg & TILE_FLAG_KRAKEN_NE)
                draw_dngn(TILE_KRAKEN_OVERLAY_NE, x, y);
            else if (bg & TILE_FLAG_ELDRITCH_NE)
                draw_dngn(TILE_ELDRITCH_OVERLAY_NE, x, y);
            if (bg & TILE_FLAG_KRAKEN_SE)
                draw_dngn(TILE_KRAKEN_OVERLAY_SE, x, y);
            else if (bg & TILE_FLAG_ELDRITCH_SE)
                draw_dngn(TILE_ELDRITCH_OVERLAY_SE, x, y);
            if (bg & TILE_FLAG_KRAKEN_SW)
                draw_dngn(TILE_KRAKEN_OVERLAY_SW, x, y);
            else if (bg & TILE_FLAG_ELDRITCH_SW)
                draw_dngn(TILE_ELDRITCH_OVERLAY_SW, x, y);
        }

        if (cell.haloed)
            draw_dngn(TILE_HALO, x, y);

        if (!(bg & TILE_FLAG_UNSEEN))

        {
            if (cell.sanctuary)
                draw_dngn(TILE_SANCTUARY, x, y);
            if (cell.silenced)
                draw_dngn(TILE_SILENCED, x, y);

            // Apply the travel exclusion under the foreground if the cell is
            // visible.  It will be applied later if the cell is unseen.
            if (bg & TILE_FLAG_EXCL_CTR)
                draw_dngn(TILE_TRAVEL_EXCLUSION_CENTRE_BG, x, y);
            else if (bg & TILE_FLAG_TRAV_EXCL)
                draw_dngn(TILE_TRAVEL_EXCLUSION_BG, x, y);
        }

        if (bg & TILE_FLAG_RAY)
            draw_dngn(TILE_RAY, x, y);
        else if (bg & TILE_FLAG_RAY_OOR)
            draw_dngn(TILE_RAY_OUT_OF_RANGE, x, y);
    }
}

function draw_foreground(x, y, cell)
{
    fg = cell.fg;
    bg = cell.bg;
    fg_idx = cell.fg & TILE_FLAG_MASK;
    is_in_water = in_water(cell);

    if (fg_idx && fg_idx <= TILE_MAIN_MAX)
    {
        base_idx = cell.base;
        if (is_in_water)
        {
            dungeon_ctx.save();
            dungeon_ctx.globalAlpha = cell.trans ? 0.7 : 1.0;

            set_nonsubmerged_clip(x, y, 20);

            if (base_idx)
                draw_main(base_idx, x, y);

            draw_main(fg_idx, x, y);

            dungeon_ctx.restore();

            dungeon_ctx.save();
            dungeon_ctx.globalAlpha = cell.trans ? 0.1 : 0.3;
            set_submerged_clip(x, y, 20);

            if (base_idx)
                draw_main(base_idx, x, y);

            draw_main(fg_idx, x, y);

            dungeon_ctx.restore();
        }
        else
        {
            if (base_idx)
                draw_main(base_idx, x, y);

            draw_main(fg_idx, x, y);
        }
    }

    if (fg & TILE_FLAG_NET)
        draw_icon(TILEI_TRAP_NET, x, y);

    if (fg & TILE_FLAG_S_UNDER)
        draw_icon(TILEI_SOMETHING_UNDER, x, y);

    status_shift = 0;
    if (fg & TILE_FLAG_MIMIC)
        draw_icon(TILEI_MIMIC, x, y);

    if (fg & TILE_FLAG_BERSERK)
    {
        draw_icon(TILEI_BERSERK, x, y);
        status_shift += 10;
    }

    // Pet mark
    if (fg & TILE_FLAG_ATT_MASK)
    {
        att_flag = fg & TILE_FLAG_ATT_MASK;
        if (att_flag == TILE_FLAG_PET)
        {
            draw_icon(TILEI_HEART, x, y);
            status_shift += 10;
        }
        else if (att_flag == TILE_FLAG_GD_NEUTRAL)
        {
            draw_icon(TILEI_GOOD_NEUTRAL, x, y);
            status_shift += 8;
        }
        else if (att_flag == TILE_FLAG_NEUTRAL)
        {
            draw_icon(TILEI_NEUTRAL, x, y);
            status_shift += 8;
        }
    }
    else if (fg & TILE_FLAG_STAB)
    {
        draw_icon(TILEI_STAB_BRAND, x, y);
        status_shift += 15;
    }
    else if (fg & TILE_FLAG_MAY_STAB)
    {
        draw_icon(TILEI_MAY_STAB_BRAND, x, y);
        status_shift += 8;
    }

    if (fg & TILE_FLAG_POISON)
    {
        draw_icon(TILEI_POISON, x, y, -status_shift, 0);
        status_shift += 5;
    }
    if (fg & TILE_FLAG_FLAME)
    {
        draw_icon(TILEI_FLAME, x, y, -status_shift, 0);
        status_shift += 5;
    }

    if (fg & TILE_FLAG_ANIM_WEP)
        draw_icon(TILEI_ANIMATED_WEAPON, x, y);

    if (bg & TILE_FLAG_UNSEEN && (bg != TILE_FLAG_UNSEEN || fg))
        draw_icon(TILEI_MESH, x, y);

    if (bg & TILE_FLAG_OOR && (bg != TILE_FLAG_OOR || fg))
        draw_icon(TILEI_OOR_MESH, x, y);

    if (bg & TILE_FLAG_MM_UNSEEN && (bg != TILE_FLAG_MM_UNSEEN || fg))
        draw_icon(TILEI_MAGIC_MAP_MESH, x, y);

    // Don't let the "new stair" icon cover up any existing icons, but
    // draw it otherwise.
    if (bg & TILE_FLAG_NEW_STAIR && status_shift == 0)
        draw_icon(TILEI_NEW_STAIR, x, y);

    if (bg & TILE_FLAG_EXCL_CTR && (bg & TILE_FLAG_UNSEEN))
        draw_icon(TILEI_TRAVEL_EXCLUSION_CENTRE_FG, x, y);
    else if (bg & TILE_FLAG_TRAV_EXCL && (bg & TILE_FLAG_UNSEEN))
        draw_icon(TILEI_TRAVEL_EXCLUSION_FG, x, y);

    // Tutorial cursor takes precedence over other cursors.
    if (bg & TILE_FLAG_TUT_CURSOR)
    {
        draw_icon(TILEI_TUTORIAL_CURSOR, x, y);
    }
    else if (bg & TILE_FLAG_CURSOR)
    {
        type = ((bg & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR1) ?
            TILEI_CURSOR : TILEI_CURSOR2;

        if ((bg & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR3)
            type = TILEI_CURSOR3;

        draw_icon(type, x, y);
    }

    if (fg & TILE_FLAG_MDAM_MASK)
    {
        mdam_flag = fg & TILE_FLAG_MDAM_MASK;
        if (mdam_flag == TILE_FLAG_MDAM_LIGHT)
            draw_icon(TILEI_MDAM_LIGHTLY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_MOD)
            draw_icon(TILEI_MDAM_MODERATELY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_HEAVY)
            draw_icon(TILEI_MDAM_HEAVILY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_SEV)
            draw_icon(TILEI_MDAM_SEVERELY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_ADEAD)
            draw_icon(TILEI_MDAM_ALMOST_DEAD, x, y);
    }

    if (fg & TILE_FLAG_DEMON)
    {
        demon_flag = fg & TILE_FLAG_DEMON;
        if (demon_flag == TILE_FLAG_DEMON_1)
            draw_icon(TILEI_DEMON_NUM1, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_2)
            draw_icon(TILEI_DEMON_NUM2, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_3)
            draw_icon(TILEI_DEMON_NUM3, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_4)
            draw_icon(TILEI_DEMON_NUM4, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_5)
            draw_icon(TILEI_DEMON_NUM5, x, y);
    }
}


// Helper functions for drawing from specific textures
// TODO: Center tiles that are smaller than 32x32?
// TODO: Handle redrawing of cells above higher-than-32 tiles
// (e.g. the lernaean hydra leaves a trail of heads)

function draw_tile(idx, cx, cy, img, info_func, ofsx, ofsy, y_max)
{
    x = dungeon_cell_w * cx;
    y = dungeon_cell_h * cy;
    info = info_func(idx);
    img = get_img(img);
    if (!info)
    {
        throw ("Tile not found: " + idx);
    }
    w = info.ex - info.sx;
    h = info.ey - info.sy;
    if (h > dungeon_cell_h)
        y -= (h - dungeon_cell_h);
    if (y_max)
        h -= (dungeon_cell_h - y_max);
    dungeon_ctx.drawImage(img, info.sx, info.sy, w, h,
                          x + info.ox + (ofsx || 0), y + info.oy + (ofsy || 0), w, h);
}

function draw_dngn(idx, cx, cy)
{
    draw_tile(idx, cx, cy, get_dngn_img(idx), get_dngn_tile_info);
}

function draw_main(idx, cx, cy)
{
    draw_tile(idx, cx, cy, "main", get_main_tile_info);
}

function draw_player(idx, cx, cy, ofsx, ofsy, y_max)
{
    draw_tile(idx, cx, cy, "player", get_player_tile_info, ofsx, ofsy, y_max);
}

function draw_icon(idx, cx, cy, ofsx, ofsy)
{
    draw_tile(idx, cx, cy, "icons", get_icons_tile_info, ofsx, ofsy);
}

function obj_to_str (o)
{
    var parse = function (_o)
    {
        var a = [], t;

        for (var p in _o)
        {
            if (_o.hasOwnProperty(p))
            {
                t = _o[p];

                if (t && typeof t == "object")
                {
                    a[a.length]= p + ":{ " + arguments.callee(t).join(", ") + "}";
                }
                else
                {
                    if (typeof t == "string")
                    {
                        a[a.length] = [ p+ ": \"" + t.toString() + "\"" ];
                    }
                    else
                    {
                        a[a.length] = [ p+ ": " + t.toString()];
                    }
                }
            }
        }
        return a;
    }
    return "{" + parse(o).join(", ") + "}";
}
