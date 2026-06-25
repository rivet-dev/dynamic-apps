/*
 * Copyright (C) 2001,2002,2003,2009,2010 Red Hat, Inc.
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>

#include "vtemacros.h"
#include "vteenums.h"

G_BEGIN_DECLS

_VTE_PUBLIC
char *vte_get_user_shell(void) _VTE_CXX_NOEXCEPT;

_VTE_PUBLIC
const char *vte_get_features (void) _VTE_CXX_NOEXCEPT;

_VTE_PUBLIC
VteFeatureFlags vte_get_feature_flags(void) _VTE_CXX_NOEXCEPT;

#define VTE_TEST_FLAGS_NONE (G_GUINT64_CONSTANT(0))
#define VTE_TEST_FLAGS_ALL (~G_GUINT64_CONSTANT(0))

_VTE_PUBLIC
void vte_set_test_flags(guint64 flags) _VTE_CXX_NOEXCEPT;

G_END_DECLS
