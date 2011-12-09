
define(["jquery", "comm", "client", "./dungeon_renderer", "./display", "./minimap",
        "./settings",
        "./text", "./menu"],
function ($, comm, client, dungeon_renderer, display, minimap, settings) {
    var layout_parameters;

    function init()
    {
        layout_parameters = null;
    }

    $(document).bind("game_init", init);

    function layout_params_differ(old_params, new_params)
    {
        if (!old_params) return true;
        for (var param in new_params)
        {
            if (old_params.hasOwnProperty(param) &&
                old_params[param] != new_params[param])
                return true;
        }
        return false;
    }

    function layout(params, force)
    {
        var window_width = params.window_width = $(window).width();
        var window_height = params.window_height = $(window).height();
        log(params);

        if (!force && !layout_params_differ(layout_parameters, params))
            return false;

        layout_parameters = params;

        var layer = current_layer;
        set_layer("normal");

        // Determine width of stats area
        var old_html = $("#stats").html();
        var s = "";
        for (var i = 0; i < layout_parameters.stat_width; i++)
            s = s + "&nbsp;";
        $("#stats").html(s);
        var stat_width_px = $("#stats").outerWidth();
        $("#stats").html(old_html);

        // Determine height of messages area
        old_html = $("#messages").html();
        s = "";
        for (var i = 0; i < layout_parameters.msg_height; i++)
            s = s + "<br>";
        $("#messages").html(s);
        var msg_height_px = $("#messages").outerHeight();
        $("#messages").html(old_html);

        var remaining_width = window_width - stat_width_px;
        var remaining_height = window_height - msg_height_px;

        // Position controls
        set_layer("normal");
        dungeon_renderer.fit_to(remaining_width, remaining_height,
                                layout_parameters.show_diameter);

        minimap.fit_to(stat_width_px, layout_parameters);

        $("#monster_list").width(stat_width_px);

        // Go back to the old layer, re-hide the minimap if necessary
        set_layer(layer);

        // Update the view
        display.invalidate(true);
        display.display();
        minimap.update_overlay();
    }

    function handle_delay(data)
    {
        client.delay(data.t);
    }

    var game_version;
    function handle_version(data)
    {
        game_version = data;
        document.title = data.text;
    }

    var glyph_mode_settings = {
        glyph_mode: false,
        glyph_mode_font_size: 24,
        glyph_mode_font: "monospace"
    };

    settings.set_defaults(glyph_mode_settings);
    $.extend(dungeon_renderer, glyph_mode_settings);

    $(document).off("settings_changed.game");
    $(document).on("settings_changed.game", function (ev, map) {
        var relayout = false;
        for (key in glyph_mode_settings)
        {
            if (key in map)
            {
                dungeon_renderer[key] = settings.get(key);
                relayout = true;
            }
        }
        if (relayout && layout_parameters)
            layout(layout_parameters, true);
    });

    $(document).ready(function () {
        $(window).resize(function () {
            var params = $.extend({}, layout_parameters);
            layout(params);
        });
    });

    comm.register_handlers({
        "layout": layout,
        "delay": handle_delay,
        "version": handle_version,
    });
});
