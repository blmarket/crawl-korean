var default_size = { w: 32, h: 32 };

function DungeonViewRenderer(element)
{
    DungeonCellRenderer.call(this, element);

    this.cols = 0;
    this.rows = 0;

    this.view = { x: 0, y: 0 };
    this.view_center = { x: 0, y: 0 };
}

(function() {

    DungeonViewRenderer.prototype = new DungeonCellRenderer();

    $.extend(DungeonViewRenderer.prototype, {
        set_size: function(c, r)
        {
            if ((this.cols == c) && (this.rows == r))
                return;

            this.cols = c;
            this.rows = r;
            this.view.x = this.view_center.x - Math.floor(c / 2);
            this.view.y = this.view_center.y - Math.floor(r / 2);
            this.element.width = c * this.cell_width;
            this.element.height = r * this.cell_height;
            this.init();
        },

        set_view_center: function(x, y)
        {
            this.view_center.x = x;
            this.view_center.y = y;
            var old_view = this.view;
            this.view = {
                x: x - Math.floor(this.cols / 2),
                y: y - Math.floor(this.rows / 2)
            };
            this.shift(this.view.x - old_view.x, this.view.y - old_view.y);
        },

        // Shifts the dungeon view by cx/cy cells.
        shift: function(x, y)
        {
            if ((x == 0) && (y == 0))
                return;

            if (x > this.cols) x = this.cols;
            if (x < -this.cols) x = -this.cols;
            if (y > this.rows) y = this.rows;
            if (y < -this.rows) y = -this.rows;

            var sx, sy, dx, dy;

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

            var cw = this.cell_width;
            var ch = this.cell_height;

            var w = (this.cols - Math.abs(x)) * cw;
            var h = (this.rows - Math.abs(y)) * ch;

            if (w > 0 && h > 0)
            {
                this.ctx.drawImage(this.element,
                                   sx * cw, sy * ch, w, h,
                                   dx * cw, dy * ch, w, h);
            }

            // Render cells that came into view
            for (var cy = 0; cy < dy; cy++)
                for (var cx = 0; cx < this.cols; cx++)
                    this.render_cell(cx + this.view.x, cy + this.view.y,
                                     cx * cw, cy * ch);

            for (var cy = dy; cy < this.rows - sy; cy++)
            {
                for (var cx = 0; cx < dx; cx++)
                    this.render_cell(cx + this.view.x, cy + this.view.y,
                                     cx * cw, cy * ch);
                for (var cx = this.cols - sx; cx < this.cols; cx++)
                    this.render_cell(cx + this.view.x, cy + this.view.y,
                                     cx * cw, cy * ch);
            }

            for (var cy = this.rows - sy; cy < this.rows; cy++)
                for (var cx = 0; cx < this.cols; cx++)
                    this.render_cell(cx + this.view.x, cy + this.view.y,
                                     cx * cw, cy * ch);
        },

        fit_to: function(width, height, min_diameter)
        {
            if ((min_diameter * default_size.w > width)
                || (min_diameter * default_size.h > height))
            {
                var scale = Math.min(width / (min_diameter * default_size.w),
                                     height / (min_diameter * default_size.h));
                this.set_cell_size(Math.floor(default_size.w * scale),
                                   Math.floor(default_size.h * scale));
            }

            var view_width = Math.floor(width / this.cell_width);
            var view_height = Math.floor(height / this.cell_height);
            this.set_size(view_width, view_height);
        },

        in_view: function(cx, cy)
        {
            return (cx >= this.view.x) && (this.view.x + this.cols > cx) &&
                (cy >= this.view.y) && (this.view.y + this.rows > cy);
        },

        clear: function()
        {
            this.ctx.fillStyle = "black";
            this.ctx.fillRect(0, 0, this.element.width, this.element.height);
        },

        render_loc: function(cx, cy, map_cell)
        {
            map_cell = map_cell || map_knowledge.get(cx, cy);
            var cell = map_cell.t;

            // TODO: Extract into MinimapRenderer
            if (cell)
            {
                var minimap_feat = get_cell_map_feature(map_cell);
                set_minimap(cx, cy, minimap_colours[minimap_feat]);
            }

            if (this.in_view(cx, cy))
            {
                var x = (cx - this.view.x) * this.cell_width;
                var y = (cy - this.view.y) * this.cell_height;

                this.render_cell(cx, cy, x, y, map_cell, cell);

                // Need to redraw cell below if it overlapped
                // Redraw the cell below if it overlapped
                if (this.in_view(cx, cy + 1))
                {
                    var cell_below = map_knowledge.get(cx, cy + 1);
                    if (cell_below.t && cell_below.t.sy && (cell_below.t.sy < 0))
                        this.render_loc(cx, cy + 1);
                }
            }
        },

        draw_overlay: function(idx, x, y)
        {
            if (this.in_view(x, y))
                this.draw_main(idx,
                               (x - this.view.x) * this.cell_width,
                               (y - this.view.y) * this.cell_height);
        },
    });
}) ();
