#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-text.h"

#include "tilefont.h"
#include "unicode.h"

int TextRegion::print_x;
int TextRegion::print_y;
TextRegion *TextRegion::text_mode = NULL;
int TextRegion::text_col = 0;

TextRegion *TextRegion::cursor_region= NULL;
int TextRegion::cursor_flag = 0;
int TextRegion::cursor_x;
int TextRegion::cursor_y;

TextRegion::TextRegion(FontWrapper *font) :
    cbuf(NULL),
    abuf(NULL),
    cx_ofs(0),
    cy_ofs(0),
    m_font(font)
{
    ASSERT(font);

    dx = m_font->char_width();
    dy = m_font->char_height();
}

void TextRegion::on_resize()
{
    delete[] cbuf;
    delete[] abuf;

    int size = mx * my;
    cbuf = new ucs_t[size];
    abuf = new uint8_t[size];

    for (int i = 0; i < size; i++)
    {
        cbuf[i] = ' ';
        abuf[i] = 0;
    }
}

TextRegion::~TextRegion()
{
    delete[] cbuf;
    delete[] abuf;
}

void TextRegion::adjust_region(int *x1, int *x2, int y)
{
    *x2 = *x2 + 1;
}

void TextRegion::addstr(const char *buffer)
{
    ucs_t buf2[1024], c;

    int j = 0;

    int clen;
    do
    {
        buffer += clen = utf8towc(&c, buffer);
        bool newline = false;

        if (c == '\r')
            continue;

        if (c == '\n')
        {
            c = 0;
            newline = true;
        }
        // TODO: use wcwidth() to handle widths!=1:
        // *  2 for CJK chars -- add a zero-width blank?
        // *  0 for combining characters -- would need extra support
        // * -1 for non-printable stuff -- assert or ignore
        buf2[j] = c;
        j++;

        if (c == 0 || j == ARRAYSZ(buf2))
        {
            if (j-1 != 0)
                addstr_aux(buf2, j - 1); // draw it
            if (newline)
            {
                print_x = cx_ofs;
                print_y++;
                j = 0;

                if (print_y - cy_ofs == my)
                    scroll();
            }
        }
    } while (clen);
    if (cursor_flag)
        cgotoxy(print_x+1, print_y+1);
}

void TextRegion::addstr_aux(const ucs_t *buffer, int len)
{
    int x = print_x - cx_ofs;
    int y = print_y - cy_ofs;
    int adrs = y * mx;
    int head = x;
    int tail = x + len - 1;

    // XXX: What does this even do?
    adjust_region(&head, &tail, y);

    for (int i = 0; i < len && x + i < mx; i++)
    {
        cbuf[adrs+x+i] = buffer[i];
        abuf[adrs+x+i] = text_col;
    }
    print_x += len;
}

void TextRegion::clear_to_end_of_line()
{
    int cx = print_x - cx_ofs;
    int cy = print_y - cy_ofs;
    int col = text_col;
    int adrs = cy * mx;

    ASSERT(adrs + mx - 1 < mx * my);
    for (int i = cx; i < mx; i++)
    {
        cbuf[adrs+i] = ' ';
        abuf[adrs+i] = col;
    }
}

void TextRegion::putwch(ucs_t ch)
{
    // special case: check for '0' char: map to space
    if (ch == 0)
        ch = ' ';
    addstr_aux(&ch, 1);
}

void TextRegion::textcolor(int color)
{
    text_col = color;
}

void TextRegion::textbackground(int col)
{
    textcolor(col*16 + (text_col & 0xf));
}

void TextRegion::cgotoxy(int x, int y)
{
    ASSERT(x >= 1);
    ASSERT(y >= 1);
    print_x = x-1;
    print_y = y-1;

#if 0
    if (cursor_region != NULL && cursor_flag)
    {
        cursor_x = -1;
        cursor_y = -1;
        cursor_region = NULL;
    }
#endif
    if (cursor_flag)
    {
        cursor_x = print_x;
        cursor_y = print_y;
        cursor_region = text_mode;
    }
}

int TextRegion::wherex()
{
    return print_x + 1;
}

int TextRegion::wherey()
{
    return print_y + 1;
}

void TextRegion::_setcursortype(int curstype)
{
    cursor_flag = curstype;
    if (cursor_region != NULL)
    {
        cursor_x = -1;
        cursor_y = -1;
    }

    if (curstype)
    {
        cursor_x = print_x;
        cursor_y = print_y;
        cursor_region = text_mode;
    }
}

void TextRegion::render()
{
#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering TextRegion\n");
#endif
    if (this == TextRegion::cursor_region && cursor_x > 0 && cursor_y > 0)
    {
        int idx = cursor_x + mx * cursor_y;

        ucs_t   char_back = cbuf[idx];
        uint8_t col_back  = abuf[idx];

        cbuf[idx] = '_';
        abuf[idx] = WHITE;

        m_font->render_textblock(sx + ox, sy + oy, cbuf, abuf, mx, my);

        cbuf[idx] = char_back;
        abuf[idx] = col_back;
    }
    else
        m_font->render_textblock(sx + ox, sy + oy, cbuf, abuf, mx, my);
}

void TextRegion::clear()
{
    for (int i = 0; i < mx * my; i++)
    {
        cbuf[i] = ' ';
        abuf[i] = 0;
    }
}

void TextRegion::scroll()
{
    for (int idx = 0; idx < mx*(my-1); idx++)
    {
        cbuf[idx] = cbuf[idx + mx];
        abuf[idx] = abuf[idx + mx];
    }

    for (int idx = mx*(my-1); idx < mx*my; idx++)
    {
        cbuf[idx] = ' ';
        abuf[idx] = 0;
    }

    if (print_y > 0)
        print_y -= 1;
    if (cursor_y > 0)
        cursor_y -= 1;
}

#endif
