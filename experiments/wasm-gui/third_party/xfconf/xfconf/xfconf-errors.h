/*
 *  xfconf
 *
 *  Copyright (c) 2007 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; version 2
 *  of the License ONLY.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __XFCONF_ERRORS_H__
#define __XFCONF_ERRORS_H__

#if !defined(LIBXFCONF_COMPILATION) && !defined(__XFCONF_IN_XFCONF_H__)
#error "Do not include xfconf-errors.h, as this file may change or disappear in the future.  Include <xfconf/xfconf.h> instead."
#endif

#include <glib-object.h>

#define XFCONF_TYPE_ERROR  (xfconf_error_get_type())
#define XFCONF_ERROR       (xfconf_get_error_quark())

G_BEGIN_DECLS

/**
 * XfconfError:
 * @XFCONF_ERROR_UNKNOWN: An unknown error occurred
 * @XFCONF_ERROR_CHANNEL_NOT_FOUND: The specified channel does not exist
 * @XFCONF_ERROR_PROPERTY_NOT_FOUND: The specified property does not exist on the channel
 * @XFCONF_ERROR_READ_FAILURE: There was a failure reading from the configuration store
 * @XFCONF_ERROR_WRITE_FAILURE: There was a failure writing to the configuration store
 * @XFCONF_ERROR_PERMISSION_DENIED: The user is not allowed to read or write to the channel or property
 * @XFCONF_ERROR_INTERNAL_ERROR: An internal error (likely a bug in xfconf) occurred
 * @XFCONF_ERROR_NO_BACKEND: No backends were found, or those found could not be loaded
 * @XFCONF_ERROR_INVALID_PROPERTY: The property name specified was invalid
 * @XFCONF_ERROR_INVALID_CHANNEL: The channel name specified was invalid
 *
 * An enumeration listing the different kinds of errors under the XFCONF_ERROR domain.
 *
 **/
typedef enum _XfconfError
{
    XFCONF_ERROR_UNKNOWN = 0,
    XFCONF_ERROR_CHANNEL_NOT_FOUND,
    XFCONF_ERROR_PROPERTY_NOT_FOUND,
    XFCONF_ERROR_READ_FAILURE,
    XFCONF_ERROR_WRITE_FAILURE,
    XFCONF_ERROR_PERMISSION_DENIED,
    XFCONF_ERROR_INTERNAL_ERROR,
    XFCONF_ERROR_NO_BACKEND,
    XFCONF_ERROR_INVALID_PROPERTY,
    XFCONF_ERROR_INVALID_CHANNEL,
} XfconfError;

GType xfconf_error_get_type(void) G_GNUC_CONST;
GQuark xfconf_get_error_quark(void);

G_END_DECLS

#endif  /* __XFCONF_ERRORS_H__ */
