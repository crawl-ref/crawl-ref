var current_layer = "crt";

var dungeon_ctx;
var dungeon_cell_w = 32, dungeon_cell_h = 32;
var dungeon_cols = 0, dungeon_rows = 0;

var socket;

var log_messages = false;

var delay_timeout = undefined;
var message_queue = [];

function delay(ms)
{
    clearTimeout(delay_timeout);
    delay_timeout = setTimeout(delay_ended, ms);
}

function delay_ended()
{
    delay_timeout = undefined;
    while (message_queue.length && !delay_timeout)
    {
        msg = message_queue.shift();
        try
        {
            eval(msg);
        } catch (err)
        {
            console.error("Error in message: " + msg.data + " - " + err);
        }
    }
}

function set_layer(layer)
{
    console.log("Setting layer: " + layer);
    if (layer == "crt")
    {
        $("#crt").show();
        $("#dungeon").hide();
        $("#stats").hide();
        $("#messages").hide();
    }
    else if (layer == "normal")
    {
        $("#crt").hide();
        $("#dungeon").show();
        // jQuery should restore display correctly -- but doesn't
        $("#stats").css("display", "inline-block");
        $("#messages").show();
    }
    current_layer = layer;
}

function get_img(id)
{
    return $("#" + id)[0];
}

function view_size(cols, rows)
{
    if ((cols == dungeon_cols) && (rows == dungeon_rows))
        return;

    console.log("Changing view size to: " + cols + "/" + rows);
    dungeon_cols = cols;
    dungeon_rows = rows;
    canvas = $("#dungeon")[0];
    canvas.width = dungeon_cols * dungeon_cell_w;
    canvas.height = dungeon_rows * dungeon_cell_h;
    dungeon_ctx = canvas.getContext("2d");

    clear_tile_cache();
}

function shift(cx, cy)
{
    var x = cx * dungeon_cell_w;
    var y = cy * dungeon_cell_h;

    var w = dungeon_cols, h = dungeon_rows;

    if (x > 0)
    {
        sx = x;
        dx = 0;
    }
    else
    {
        sx = 0;
        dx = -x;
    }
    if (y > 0)
    {
        sy = y;
        dy = 0;
    }
    else
    {
        sy = 0;
        dy = -y;
    }
    w = (w * dungeon_cell_w - abs(x));
    h = (h * dungeon_cell_h - abs(y));

    dungeon_ctx.drawImage($("#dungeon")[0], sx, sy, w, h, dx, dy, w, h);

    dungeon_ctx.fillStyle = "black";
    dungeon_ctx.fillRect(0, 0, w * dungeon_cell_w, dy);
    dungeon_ctx.fillRect(0, dy, dx, h);
    dungeon_ctx.fillRect(w, 0, sx, h * dungeon_cell_h);
    dungeon_ctx.fillRect(0, h, w, sy);

    // Shift the tile cache
    shift_tile_cache(cx, cy);

    // Shift cursors
    $.each(cursor_locs, function(type, loc)
           {
               if (loc)
               {
                   loc.x -= cx;
                   loc.y -= cy;
               }
           });

    // Shift overlays
    $.each(overlaid_locs, function(i, loc)
           {
               if (loc)
               {
                   loc.x -= cx;
                   loc.y -= cy;
               }
           });
}

function handle_keypress(e)
{
    if (delay_timeout) return; // TODO: Do we want to capture keys during delay?

    s = String.fromCharCode(e.which);
    if (s == "\\")
    {
        socket.send("\\92\n");
    } else if (s == "^")
    {
        socket.send("\\94\n");
    }
    else
        socket.send(s);
}

function handle_keydown(e)
{
    if (delay_timeout) return; // TODO: Do we want to capture keys during delay?

    if (e.which in key_conversion)
    {
        socket.send("\\" + key_conversion[e.which] + "\n");
    }
    else
        console.log("Key: " + e.which);
}

$(document).ready(
    function()
    {
        set_layer("crt");

        // Key handler
        $(document).bind('keypress.client', handle_keypress);
        $(document).bind('keydown.client', handle_keydown);

        socket = new WebSocket("ws://localhost:8080/socket");

        socket.onopen = function()
        {
            // Currently nothing needs to be done here
        };

        socket.onmessage = function(msg)
        {
            if (log_messages)
            {
                console.log("Message: " + msg.data);
            }
            if (delay_timeout)
            {
                message_queue.push(msg.data);
            } else
            {
                try
                {
                    eval(msg.data);
                } catch (err)
                {
                    console.error("Error in message: " + msg.data + " - " + err);
                }
            }
        };

        socket.onclose = function()
        {
            // TODO: Handle this
            // Redirect to lobby or something?
        };
    });

function abs(x)
{
    return x > 0 ? x : -x;
}
function assert(cond)
{
    if (!cond)
        console.log("Assertion failed!");
}
