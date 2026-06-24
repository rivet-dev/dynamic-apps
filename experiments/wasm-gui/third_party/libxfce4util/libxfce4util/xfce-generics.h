/* $Id$ */
/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */

#if !defined(_LIBXFCE4UTIL_INSIDE_LIBXFCE4UTIL_H) && !defined(LIBXFCE4UTIL_COMPILATION)
#error "Only <libxfce4util/libxfce4util.h> can be included directly, this file may disappear or change contents"
#endif

#ifndef __XFCE_GENERICS_H__
#define __XFCE_GENERICS_H__

#include <glib.h>

G_BEGIN_DECLS

/**
 * SECTION:xfce-generics
 * @title: Xfce Generics
 * @short_description: Generic data types and related functions.
 * @stability: Stable
 * @include: libxfce4util/libxfce4util.h
 *
 * Xfce-dialogs are a collection of helper dialogs to display
 * the help dialog with link to the docs website, warning, info, and
 * error dialogs and more.
 *
 * Using a generic stack
 * |[<!-- language="C" -->
 *   typedef XFCE_GENERIC_STACK(int) IntStack;
 *   IntStack *stack = xfce_stack_new (IntStack);
 *   xfce_stack_push (stack, 0);
 *   xfce_stack_push (stack, 1);
 *   printf ("Top is %d\n", xfce_stack_top (stack));
 *   xfce_stack_pop (stack);
 *   printf ("Top is %d\n", xfce_stack_top (stack));
 *   xfce_stack_free (stack);
 * ]|
 *
 */

/**
 * XFCE_GENERIC_STACK:
 * @Type: Data type of the elements that should be handled by the stack. Can be any valid data type from simple int's to complex structures.
 *
 *
 * This macro is used to create a new stack data type which elements are of
 * @Type. For example, to create a stack type that handles elements of type
 * %double, you'd write the following
 *
 * |[<!-- language="C" -->
 * typedef XFCE_GENERIC_STACK(double) MyDoubleStack;
 * ]|
 * and furtheron refer to your stack type as `MyDoubleStack`.
 *
 */

#define XFCE_GENERIC_STACK(Type)                                            \
  struct                                                                    \
  {                                                                         \
    Type  *elements;                                                        \
    gint   nelements;                                                       \
    gint   top;                                                             \
  }


/**
 * xfce_stack_new:
 * @StackType: Type of stack declared with #XFCE_GENERIC_STACK.
 *
 * Creates a new instance of @StackType and returns a pointer to the newly
 * created instance. For example, imagine you declared a type `MyDoubleStack`
 * as shown above, you can instantiate this type with
 *
 * |[<!-- language="C" -->
 * MyDoubleStack *my_stack = xfce_stack_new (MyDoubleStack);
 * ]|
 *
 */

#ifdef __GNUC__

#define xfce_stack_new(StackType)                                           \
  ({                                                                        \
    StackType *stack;                                                       \
                                                                            \
    stack            = g_new (StackType, 1);                                \
    stack->elements  = g_malloc (20 * sizeof (*(stack->elements)));         \
    stack->nelements = 20;                                                  \
    stack->top       = -1;                                                  \
                                                                            \
    stack;                                                                  \
  })
#else
static inline gpointer
xfce_stack_alloc (gsize element_size)
{
  typedef struct { gpointer elements; gint nelements; gint top; } Stack;
  Stack *stack = g_new (Stack, 1);
  stack->elements = g_malloc (20 * element_size);
  stack->nelements = 20;
  stack->top = -1;
  return stack;
}
#define xfce_stack_new(StackType)                                           \
  ((StackType *) xfce_stack_alloc (sizeof (*(((StackType *) 0)->elements))))
#endif


/**
 * xfce_stack_free:
 * @stack: A stack object.
 *
 * Frees a stack, that was allocated using #xfce_stack_new.
 *
 */

#define xfce_stack_free(stack)                                              \
  G_STMT_START                                                              \
    {                                                                       \
      g_free (stack->elements);                                             \
      g_free (stack);                                                       \
    }                                                                       \
  G_STMT_END

/**
 * xfce_stack_top:
 * @stack: A stack object.
 *
 * Removes the top element from @stack.
 */

#ifdef __GNUC__
#define xfce_stack_top(stack)                                               \
  ({                                                                        \
    g_assert (stack->top >= 0);                                             \
    stack->elements[stack->top];                                            \
  })
#else
#define xfce_stack_top(stack) ((stack)->elements[(stack)->top])
#endif

/**
 * xfce_stack_pop:
 * @stack: A stack object.
 *
 *  Removes the top element from @stack.
 */

#define xfce_stack_pop(stack)                                               \
  G_STMT_START                                                              \
    {                                                                       \
      g_assert (stack->top > 0);                                            \
      stack->top--;                                                         \
    }                                                                       \
  G_STMT_END

/**
 * xfce_stack_push:
 * @stack: A stack object.
 * @value: the value to push
 *
 * Pushes a new @value on top of @stack.
 */

#define xfce_stack_push(stack, value)                                       \
  G_STMT_START                                                              \
    {                                                                       \
      stack->top++;                                                         \
                                                                            \
      if (G_UNLIKELY (stack->top >= stack->nelements))                      \
        {                                                                   \
          stack->nelements *= 2;                                            \
          stack->elements = g_realloc (stack->elements,                     \
                                       stack->nelements * sizeof(value));   \
        }                                                                   \
                                                                            \
      stack->elements[stack->top] = value;                                  \
    }                                                                       \
  G_STMT_END

G_END_DECLS

#endif /* !__XFCE_GENERICS_H__ */
