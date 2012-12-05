#ifdef USE_TILE_LOCAL
#ifndef TILEREG_MENU_H
#define TILEREG_MENU_H

#include "format.h"
#include "tilebuf.h"
#include "tilereg.h"
#include "fixedvector.h"
#include <vector>

class MenuEntry;

class MenuRegion : public ControlRegion
{
public:
    MenuRegion(ImageManager *im, FontWrapper *entry);

    virtual int handle_mouse(MouseEvent &event);
    virtual void render();
    virtual void clear();

    int maxpagesize() const;
    void set_entry(int index, const string &s, int colour, const MenuEntry *me,
                   bool mark_selected = true);
    void set_offset(int lines);
    void set_more(const formatted_string &more);
    void set_num_columns(int columns);
protected:
    virtual void on_resize();
    virtual void place_entries();
    int mouse_entry(int x, int y);

    struct MenuRegionEntry
    {
        formatted_string text;
        int colour; // keep it separate from text
        int sx, ex, sy, ey;
        bool selected;
        bool heading;
        char key;
        bool valid;
        vector<tile_def> tiles;
    };

    ImageManager *m_image;
    FontWrapper *m_font_entry;
    formatted_string m_more;
    int m_mouse_idx;
    int m_max_columns;
    bool m_dirty;
#ifdef TOUCH_UI
    int m_more_region_start;
#endif

    virtual void run() {};
    void _place_entries(const int left_offset, const int top_offset,
                        const int menu_width);
    void _clear_buffers();

    vector<MenuRegionEntry> m_entries;

    // TODO enne - remove this?
    ShapeBuffer m_shape_buf;
    LineBuffer m_line_buf;
    FontBuffer m_font_buf;
    FixedVector<TileBuffer, TEX_MAX> m_tile_buf;
};

#endif
#endif
