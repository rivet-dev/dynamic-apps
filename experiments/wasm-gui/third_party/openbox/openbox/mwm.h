/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   mwm.h for the Openbox window manager
   Copyright (c) 2003-2007   Dana Jansens

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

#ifndef __ob__mwm_h
#define __ob__mwm_h

#include <glib.h>

typedef struct _ObMwmHints ObMwmHints;

/*! The MWM Hints as retrieved from the window property
  This structure only contains 3 elements, even though the Motif 2.0
  structure contains 5. We only use the first 3, so that is all gets
  defined.
*/
struct _ObMwmHints
{
    /*! A bitmask of ObMwmFlags values */
    guint flags;
    /*! A bitmask of ObMwmFunctions values */
    guint functions;
    /*! A bitmask of ObMwmDecorations values */
    guint decorations;
};

/*! The number of elements in the ObMwmHints struct */
#define OB_MWM_ELEMENTS 3

/*! Possible flags for MWM Hints (defined by Motif 2.0) */
typedef enum
{
    OB_MWM_FLAG_FUNCTIONS   = 1 << 0, /*!< The MMW Hints define funcs */
    OB_MWM_FLAG_DECORATIONS = 1 << 1  /*!< The MWM Hints define decor */
} ObMwmFlags;

/*! Possible functions for MWM Hints (defined by Motif 2.0) */
typedef enum
{
    OB_MWM_FUNC_ALL      = 1 << 0, /*!< All functions */
    OB_MWM_FUNC_RESIZE   = 1 << 1, /*!< Allow resizing */
    OB_MWM_FUNC_MOVE     = 1 << 2, /*!< Allow moving */
    OB_MWM_FUNC_ICONIFY  = 1 << 3, /*!< Allow to be iconfied */
    OB_MWM_FUNC_MAXIMIZE = 1 << 4  /*!< Allow to be maximized */
#if 0
    OM_MWM_FUNC_CLOSE    = 1 << 5  /*!< Allow to be closed */
#endif
} ObMwmFunctions;

/*! Possible decorations for MWM Hints (defined by Motif 2.0) */
typedef enum
{
    OB_MWM_DECOR_ALL      = 1 << 0, /*!< All decorations */
    OB_MWM_DECOR_BORDER   = 1 << 1, /*!< Show a border */
    OB_MWM_DECOR_HANDLE   = 1 << 2, /*!< Show a handle (bottom) */
    OB_MWM_DECOR_TITLE    = 1 << 3, /*!< Show a titlebar */
#if 0
    OB_MWM_DECOR_MENU     = 1 << 4, /*!< Show a menu */
#endif
    OB_MWM_DECOR_ICONIFY  = 1 << 5, /*!< Show an iconify button */
    OB_MWM_DECOR_MAXIMIZE = 1 << 6  /*!< Show a maximize button */
} ObMwmDecorations;

#endif
