/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   overlap.c for the Openbox window manager
   Copyright (c) 2011, 2013 Ian Zimmerman

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

#include "config.h"
#include "geom.h"
#include "place_overlap.h"
#include "obt/bsearch.h"

#include <glib.h>
#include <stdlib.h>

static void make_grid(const Rect* client_rects,
                      int n_client_rects,
                      const Rect* monitor,
                      int* x_edges,
                      int* y_edges,
                      int max_edges);

static int best_direction(const Point* grid_point,
                          const Rect* client_rects,
                          int n_client_rects,
                          const Rect* monitor,
                          const Size* req_size,
                          Point* best_top_left);

static int total_overlap(const Rect* client_rects,
                         int n_client_rects,
                         const Rect* proposed_rect);

static void center_in_field(Point* grid_point,
                            const Size* req_size,
                            const Rect *monitor,
                            const Rect* client_rects,
                            int n_client_rects,
                            const int* x_edges,
                            const int* y_edges,
                            int max_edges);

/* Choose the placement on a grid with least overlap */

void place_overlap_find_least_placement(const Rect* client_rects,
                                        int n_client_rects,
                                        const Rect *monitor,
                                        const Size* req_size,
                                        Point* result)
{
    POINT_SET(*result, monitor->x, monitor->y);
    int overlap = G_MAXINT;
    int max_edges = 2 * (n_client_rects + 1);

    int x_edges[max_edges];
    int y_edges[max_edges];
    make_grid(client_rects, n_client_rects, monitor,
            x_edges, y_edges, max_edges);
    int i;
    for (i = 0; i < max_edges; ++i) {
        if (x_edges[i] == G_MAXINT)
            break;
        int j;
        for (j = 0; j < max_edges; ++j) {
            if (y_edges[j] == G_MAXINT)
                break;
            Point grid_point = {.x = x_edges[i], .y = y_edges[j]};
            Point best_top_left;
            int this_overlap =
                best_direction(&grid_point, client_rects, n_client_rects,
                        monitor, req_size, &best_top_left);
            if (this_overlap < overlap) {
                overlap = this_overlap;
                *result = best_top_left;
            }
            if (overlap == 0)
                break;
        }
        if (overlap == 0)
            break;
    }
    if (config_place_center && overlap == 0) {
        center_in_field(result,
                        req_size,
                        monitor,
                        client_rects,
                        n_client_rects,
                        x_edges,
                        y_edges,
                        max_edges);
    }
}

static int compare_ints(const void* a,
                        const void* b)
{
    const int* ia = (const int*)a;
    const int* ib = (const int*)b;
    return *ia - *ib;
}

static void uniquify(int* edges,
                     int n_edges)
{
    int i = 0;
    int j = 0;

    while (j < n_edges) {
        int last = edges[j++];
        edges[i++] = last;
        while (j < n_edges && edges[j] == last)
            ++j;
    }
    /* fill the rest with nonsense */
    for (; i < n_edges; ++i)
        edges[i] = G_MAXINT;
}

static void make_grid(const Rect* client_rects,
                      int n_client_rects,
                      const Rect* monitor,
                      int* x_edges,
                      int* y_edges,
                      int max_edges)
{
    int i;
    int n_edges = 0;
    for (i = 0; i < n_client_rects; ++i) {
        if (!RECT_INTERSECTS_RECT(client_rects[i], *monitor))
            continue;
        x_edges[n_edges] = client_rects[i].x;
        y_edges[n_edges++] = client_rects[i].y;
        x_edges[n_edges] = client_rects[i].x + client_rects[i].width;
        y_edges[n_edges++] = client_rects[i].y + client_rects[i].height;
    }
    x_edges[n_edges] = monitor->x;
    y_edges[n_edges++] = monitor->y;
    x_edges[n_edges] = monitor->x + monitor->width;
    y_edges[n_edges++] = monitor->y + monitor->height;
    for (i = n_edges; i < max_edges; ++i)
        x_edges[i] = y_edges[i] = G_MAXINT;
    qsort(x_edges, n_edges, sizeof(int), compare_ints);
    uniquify(x_edges, n_edges);
    qsort(y_edges, n_edges, sizeof(int), compare_ints);
    uniquify(y_edges, n_edges);
}

static int total_overlap(const Rect* client_rects,
                         int n_client_rects,
                         const Rect* proposed_rect)
{
    int overlap = 0;
    int i;
    for (i = 0; i < n_client_rects; ++i) {
        if (!RECT_INTERSECTS_RECT(*proposed_rect, client_rects[i]))
            continue;
        Rect rtemp;
        RECT_SET_INTERSECTION(rtemp, *proposed_rect, client_rects[i]);
        overlap += RECT_AREA(rtemp);
    }
    return overlap;
}

static int find_first_grid_position_greater_or_equal(int search_value,
                                                     const int* edges,
                                                     int max_edges)
{
    g_assert(max_edges >= 2);
    g_assert(search_value >= edges[0]);
    g_assert(search_value <= edges[max_edges - 1]);

    BSEARCH_SETUP();
    BSEARCH(int, edges, 0, max_edges, search_value);

    if (BSEARCH_FOUND())
        return BSEARCH_AT();

    g_assert(BSEARCH_FOUND_NEAREST_SMALLER());
    /* Get the nearest larger instead. */
    return BSEARCH_AT() + 1;
}                         

static void expand_width(Rect* r, int by)
{
    r->width += by;
}

static void expand_height(Rect* r, int by)
{
    r->height += by;
}

typedef void ((*ExpandByMethod)(Rect*, int));

/* This structure packs most of the parametars for expand_field() in
   order to save pushing the same parameters twice. */
typedef struct _ExpandInfo {
    const Point* top_left;
    int orig_width;
    int orig_height;
    const Rect* monitor;
    const Rect* client_rects;
    int n_client_rects;
    int max_edges;
} ExpandInfo;

static int expand_field(int orig_edge_index,
                        const int* edges,
                        ExpandByMethod expand_by,
                        const ExpandInfo* i)
{
    Rect field;
    RECT_SET(field,
             i->top_left->x,
             i->top_left->y,
             i->orig_width,
             i->orig_height);
    int edge_index = orig_edge_index;
    while (edge_index < i->max_edges - 1) {
        int next_edge_index = edge_index + 1;
        (*expand_by)(&field, edges[next_edge_index] - edges[edge_index]);
        int overlap = total_overlap(i->client_rects, i->n_client_rects, &field);
        if (overlap != 0 || !RECT_CONTAINS_RECT(*(i->monitor), field))
            break;
        edge_index = next_edge_index;
    }
    return edge_index;
}

/* The algortihm used for centering a rectangle in a grid field: First
   find the smallest rectangle of grid lines that enclose the given
   rectangle.  By definition, there is no overlap with any of the other
   windows if the given rectangle is centered within this minimal
   rectangle.  Then, try extending the minimal rectangle in either
   direction (x and y) by picking successively further grid lines for
   the opposite edge.  If the minimal rectangle can be extended in *one*
   direction (x or y) but *not* the other, extend it as far as possible.
   Otherwise, just use the minimal one.  */

static void center_in_field(Point* top_left,
                            const Size* req_size,
                            const Rect *monitor,
                            const Rect* client_rects,
                            int n_client_rects,
                            const int* x_edges,
                            const int* y_edges,
                            int max_edges)
{
    /* Find minimal rectangle. */
    int orig_right_edge_index =
        find_first_grid_position_greater_or_equal(
            top_left->x + req_size->width, x_edges, max_edges);
    int orig_bottom_edge_index =
        find_first_grid_position_greater_or_equal(
            top_left->y + req_size->height, y_edges, max_edges);
    ExpandInfo i = {
        .top_left = top_left,
        .orig_width = x_edges[orig_right_edge_index] - top_left->x,
        .orig_height = y_edges[orig_bottom_edge_index] - top_left->y,
        .monitor = monitor,
        .client_rects = client_rects,
        .n_client_rects = n_client_rects,
        .max_edges = max_edges};
    /* Try extending width. */
    int right_edge_index =
        expand_field(orig_right_edge_index, x_edges, expand_width, &i);
    /* Try extending height. */
    int bottom_edge_index =
        expand_field(orig_bottom_edge_index, y_edges, expand_height, &i);

    int final_width = x_edges[orig_right_edge_index] - top_left->x;
    int final_height = y_edges[orig_bottom_edge_index] - top_left->y;
    if (right_edge_index == orig_right_edge_index &&
        bottom_edge_index != orig_bottom_edge_index)
        final_height = y_edges[bottom_edge_index] - top_left->y;
    else if (right_edge_index != orig_right_edge_index &&
             bottom_edge_index == orig_bottom_edge_index)
        final_width = x_edges[right_edge_index] - top_left->x;

    /* Now center the given rectangle within the field */
    top_left->x += (final_width - req_size->width) / 2;
    top_left->y += (final_height - req_size->height) / 2;
}

/* Given a list of Rect RECTS, a Point PT and a Size size, determine the
   direction from PT which results in the least total overlap with RECTS
   if a rectangle is placed in that direction.  Return the top/left
   Point of such rectangle and the resulting overlap amount.  Only
   consider placements within BOUNDS. */

#define NUM_DIRECTIONS 4

static int best_direction(const Point* grid_point,
                          const Rect* client_rects,
                          int n_client_rects,
                          const Rect* monitor,
                          const Size* req_size,
                          Point* best_top_left)
{
    static const Size directions[NUM_DIRECTIONS] = {
        {0, 0}, {0, -1}, {-1, 0}, {-1, -1}
    };
    int overlap = G_MAXINT;
    int i;
    for (i = 0; i < NUM_DIRECTIONS; ++i) {
        Point pt = {
            .x = grid_point->x + (req_size->width * directions[i].width),
            .y = grid_point->y + (req_size->height * directions[i].height)
        };
        Rect r;
        RECT_SET(r, pt.x, pt.y, req_size->width, req_size->height);
        if (!RECT_CONTAINS_RECT(*monitor, r))
            continue;
        int this_overlap = total_overlap(client_rects, n_client_rects, &r);
        if (this_overlap < overlap) {
            overlap = this_overlap;
            *best_top_left = pt;
        }
        if (overlap == 0)
            break;
    }
    return overlap;
}
