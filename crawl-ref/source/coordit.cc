/**
 * @file
 * @brief Coordinate iterators.
**/

#include "AppHdr.h"

#include "coordit.h"

#include "coord-circle.h"
#include "coord.h"
#include "random.h"
#include "player.h"

rectangle_iterator::rectangle_iterator(const coord_def& corner1,
                                        const coord_def& corner2)
{
    topleft.x = min(corner1.x, corner2.x);
    topleft.y = min(corner1.y, corner2.y); // not really necessary
    bottomright.x = max(corner1.x, corner2.x);
    bottomright.y = max(corner1.y, corner2.y);
    current = topleft;
}

rectangle_iterator::rectangle_iterator(int x_border_dist, int y_border_dist)
{
    if (y_border_dist < 0)
        y_border_dist = x_border_dist;

    topleft.set(x_border_dist, y_border_dist);
    bottomright.set(GXM - x_border_dist - 1, GYM - y_border_dist - 1);
    current = topleft;
}

rectangle_iterator::operator bool() const
{
    return (current.y <= bottomright.y);
}

coord_def rectangle_iterator::operator *() const
{
    return current;
}

const coord_def* rectangle_iterator::operator->() const
{
    return &current;
}

void rectangle_iterator::operator ++()
{
    if (current.x == bottomright.x)
    {
        current.x = topleft.x;
        current.y++;
    }
    else
        current.x++;
}

void rectangle_iterator::operator++(int dummy)
{
    ++(*this);
}


random_rectangle_iterator::random_rectangle_iterator(const coord_def& corner1,
                                                     const coord_def& corner2)
{
    int left   = min(corner1.x, corner2.x);
    int right  = max(corner1.x, corner2.x);
    int top    = min(corner1.y, corner2.y);
    int bottom = max(corner1.y, corner2.y);

    top_left.x = left;
    top_left.y = top;

    for (int y = top; y <= bottom; y++)
        for (int x = left; x <= right; x++)
            remaining.push_back(coord_def(x, y));

    if (remaining.empty())
        current = 0;
    else
        current = random2(remaining.size());
}

random_rectangle_iterator::random_rectangle_iterator(int x_border_dist,
                                                     int y_border_dist)
{
    if (y_border_dist < 0)
        y_border_dist = x_border_dist;

    int right  = GXM - x_border_dist - 1;
    int bottom = GYM - y_border_dist - 1;

    top_left.x = x_border_dist;
    top_left.y = y_border_dist;

    for (int y = y_border_dist; y <= bottom; y++)
        for (int x = x_border_dist; x <= right; x++)
            remaining.push_back(coord_def(x, y));

    if (remaining.empty())
        current = 0;
    else
        current = random2(remaining.size());
}

random_rectangle_iterator::operator bool() const
{
    return !remaining.empty();
}

coord_def random_rectangle_iterator::operator *() const
{
    if (remaining.empty())
        return top_left;
    else
        return remaining[current];
}

const coord_def* random_rectangle_iterator::operator->() const
{
    if (remaining.empty())
        return &top_left;
    else
        return &(remaining[current]);
}

void random_rectangle_iterator::operator ++()
{
    if (!remaining.empty())
    {
        remaining[current] = remaining.back();
        remaining.pop_back();
        if (!remaining.empty())
            current = random2(remaining.size());
    }
}

void random_rectangle_iterator::operator++(int dummy)
{
    ++(*this);
}


/*
 *  circle iterator
 */

circle_iterator::circle_iterator(const circle_def &circle_)
    : circle(circle_), iter(circle_.get_bbox().iter())
{
    while (iter && !circle.contains(*iter))
        ++iter;
}

circle_iterator::operator bool() const
{
    return iter;
}

coord_def circle_iterator::operator*() const
{
    return *iter;
}

void circle_iterator::operator++()
{
    do
        ++iter;
    while (iter && !circle.contains(*iter));
}

void circle_iterator::operator++(int)
{
    ++(*this);
}


radius_iterator::radius_iterator(const coord_def& center, int param,
                                 circle_type ctype,
                                 const los_base* _los,
                                 bool _exclude_center)
    : circle(center, param, ctype),
      iter(circle.iter()),
      exclude_center(_exclude_center),
      los(_los)
{
    advance(true);
}

radius_iterator::radius_iterator(const coord_def& _center, int _radius,
                                 bool roguelike, bool _require_los,
                                 bool _exclude_center)
    : circle(_center, _radius, roguelike ? C_SQUARE : C_POINTY),
      iter(circle.iter()),
      exclude_center(_exclude_center),
      los(_require_los ? you.get_los() : NULL)
{
    advance(true);
}

radius_iterator::radius_iterator(const los_base* los_,
                                 bool _exclude_center)
    : circle(los_->get_bounds()),
      iter(circle.iter()),
      exclude_center(_exclude_center),
      los(los_)
{
    advance(true);
}

void radius_iterator::advance(bool may_stay)
{
    if (!may_stay)
        ++iter;
    while (iter && !is_valid_square(*iter))
        ++iter;
    current = *iter;
}

radius_iterator::operator bool() const
{
    return iter;
}

coord_def radius_iterator::operator *() const
{
    return current;
}

const coord_def* radius_iterator::operator->() const
{
    return &current;
}

bool radius_iterator::is_valid_square(const coord_def &p) const
{
    if (exclude_center && p == circle.get_center())
        return false;
    if (los && !los->see_cell(p))
        return false;
    return true;
}

void radius_iterator::operator++()
{
    advance(false);
}

void radius_iterator::operator++(int dummy)
{
    ++(*this);
}

/*
 *  spiral iterator
 */
distance_iterator::distance_iterator(const coord_def& _center, bool _fair,
                                 bool exclude_center, int _max_radius) :
    center(_center), current(_center), r(0), max_radius(_max_radius),
    threshold(0), icur(0), iend(0), fair(_fair)
{
    vcur  = lists + 0;
    vnear = lists + 1;
    vfar  = lists + 2;

    for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++)
            if (dx || dy)
                vnear->push_back(coord_def(dx, dy));

    if (exclude_center)
        advance();
}

bool distance_iterator::advance()
{
again:
    if (++icur >= vcur->size())
        icur = 0;
    if (icur == iend)
    {
        // Advance to the next radius.
        vector<coord_def> *tmp = vcur;
        vcur = vnear;
        vnear = vfar;
        vfar = tmp;
        tmp->clear();

        if (!vcur->size())
            return false;
        // Randomize the order various directions are returned.
        // Just the initial angle is enough.
        if (fair)
            icur = iend = random2(vcur->size());
        else
            icur = iend = 0; // randomness is costly

        if (r++ >= max_radius)
        {
            vcur->clear();
            return false;
        }
        threshold = (r+1) * (r+1) + 1;
    }

    coord_def d = (*vcur)[icur];
    if (!in_bounds(current = center + d))
        goto again;

    ASSERT(d.x || d.y);
    if (!d.y)
        push_neigh(d, sgn(d.x), 0);
    if (!d.x)
        push_neigh(d, 0, sgn(d.y));
    if (d.x <= 0)
    {
        if (d.y <= 0)
            push_neigh(d, -1, -1);
        if (d.y >= 0)
            push_neigh(d, -1, +1);
    }
    if (d.x >= 0)
    {
        if (d.y <= 0)
            push_neigh(d, +1, -1);
        if (d.y >= 0)
            push_neigh(d, +1, +1);
    }

    return true;
}

void distance_iterator::push_neigh(coord_def d, int dx, int dy)
{
    d.x += dx;
    d.y += dy;
    ((d.abs() <= threshold) ? vnear : vfar)->push_back(d);
}

distance_iterator::operator bool() const
{
    return in_bounds(current) && r <= max_radius;
}

coord_def distance_iterator::operator *() const
{
    return current;
}

const coord_def* distance_iterator::operator->() const
{
    return &current;
}

const distance_iterator& distance_iterator::operator++()
{
    advance();
    return *this;
}

void distance_iterator::operator++(int dummy)
{
    ++(*this);
}

int distance_iterator::radius() const
{
    return r;
}
