/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-
 
   obt/bsearch.h for the Openbox window manager
   Copyright (c) 2010        Dana Jansens
 
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

#ifndef __obt_bsearch_h
#define __obt_bsearch_h

#include <glib.h>

G_BEGIN_DECLS

/*! Setup to do a binary search on an array. */
#define BSEARCH_SETUP() \
    register guint l_BSEARCH, r_BSEARCH, out_BSEARCH; \
    register gboolean nearest_BSEARCH; (void)nearest_BSEARCH;

/*! Helper macro that just returns the input */
#define BSEARCH_IS_T(t) (t)

/*! Search an array @ar holding elements of type @t, starting at index @start,
  with @size elements, looking for value @val. */
#define BSEARCH(t, ar, start, size, val) \
    BSEARCH_CMP(t, ar, start, size, val, BSEARCH_IS_T)

/*! Search an array @ar, starting at index @start,
  with @size elements, looking for value @val of type @t,
  using the macro @to_t to convert values in the arrau @ar to type @t.

  If all elements in @ar are less than @val, then BSEARCH_AT() will be
  the last element in @ar.
  If all elements in @ar are greater than @val, then BSEARCH_AT() will be
  the first element in @ar.
  Otherwise, if the element @val is not found then BSEARCH_AT() will be set to
  the position before the first element larger than @val.
*/
#define BSEARCH_CMP(t, ar, start, size, val, to_t)  \
do { \
    out_BSEARCH = (start); \
    l_BSEARCH = (size) > 0 ? (start) : (start + 1); \
    r_BSEARCH = (size) > 0 ? (start)+(size)-1 : (start); \
    nearest_BSEARCH = FALSE; \
    while (l_BSEARCH <= r_BSEARCH) { \
        /* m is in the middle, but to the left if there's an even number \
           of elements */ \
        out_BSEARCH = l_BSEARCH + (r_BSEARCH - l_BSEARCH)/2;      \
        if ((val) == to_t((ar)[out_BSEARCH])) {             \
            break; \
        } \
        else if ((val) < to_t((ar)[out_BSEARCH])) { \
            if (out_BSEARCH > start) { \
                r_BSEARCH = out_BSEARCH-1; /* search to the left side */ \
            } else { \
                /* reached the start of the array */ \
                r_BSEARCH = out_BSEARCH; \
                l_BSEARCH = out_BSEARCH + 1; \
            } \
        } \
        else { \
            l_BSEARCH = out_BSEARCH+1; /* search to the right side */ \
        } \
    } \
    if ((size) > 0 && (val) != to_t((ar)[out_BSEARCH])) { \
        if ((val) > to_t((ar)[out_BSEARCH])) { \
            nearest_BSEARCH = TRUE; \
        } \
        else if (out_BSEARCH > start) { \
            --out_BSEARCH; \
            nearest_BSEARCH = (val) > to_t((ar)[out_BSEARCH]); \
        } \
    } \
} while (0)

/*! Returns true if the element last searched for was found in the array */
#define BSEARCH_FOUND() (l_BSEARCH <= r_BSEARCH)

/*! Returns the position in the array at which the element last searched for
  was found. */
#define BSEARCH_AT() (out_BSEARCH)

/*! Returns true if the element at BSEARCH_AT() in the array is the largest
  value in the array smaller than the search key. Returns false if there was
  nothing in the array smaller than the search key.

  Should only be used when BSEARCH_FOUND() is false.
*/
#define BSEARCH_FOUND_NEAREST_SMALLER() (!BSEARCH_FOUND() && nearest_BSEARCH)

G_END_DECLS

#endif
