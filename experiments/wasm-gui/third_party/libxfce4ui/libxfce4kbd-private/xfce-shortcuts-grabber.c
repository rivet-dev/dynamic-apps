/* vi:set expandtab sw=2 sts=2: */
/*
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>

#include <libxfce4kbd-private/xfce-shortcuts-grabber.h>
#include <libxfce4kbd-private/xfce-shortcuts-marshal.h>



#define MODIFIERS_ERROR ((GdkModifierType)(-1))
#define MODIFIERS_NONE 0

/*
 * It is not clear what the correct behavior is in this regard, so this is disabled rather
 * than removed, in order to preserve the changes that took place during cycle 4.17 and to
 * facilitate a possible reversal of this choice.
 * The arguments for this deactivation are in essence:
 * - Xfce users are used to the previous behavior and see this as a regression
 * - The old behavior better matches the behavior of other keyboard shortcuts in Xfce (xfwm4,
 *   GTK 3 apps)
 * - Adding an option for this is like solving a bug by adding an option and therefore does
 *   not seem to be a good idea
 * See also https://gitlab.xfce.org/xfce/libxfce4ui/-/merge_requests/91
 */
#define TRACK_LAYOUT_CHANGE FALSE


typedef struct _XfceKey XfceKey;



static void            xfce_shortcuts_grabber_constructed      (GObject                   *object);
static void            xfce_shortcuts_grabber_finalize         (GObject                   *object);
static void            xfce_shortcuts_grabber_keys_changed     (GdkKeymap                 *keymap,
                                                                XfceShortcutsGrabber      *grabber);
static void            xfce_shortcuts_grabber_regrab_all       (XfceShortcutsGrabber      *grabber);
static void            xfce_shortcuts_grabber_ungrab_all       (XfceShortcutsGrabber      *grabber);
static void            xfce_shortcuts_grabber_grab             (XfceShortcutsGrabber      *grabber,
                                                                XfceKey                   *key);
static void            xfce_shortcuts_grabber_ungrab           (XfceShortcutsGrabber      *grabber,
                                                                XfceKey                   *key);
static GdkFilterReturn xfce_shortcuts_grabber_event_filter     (GdkXEvent                 *gdk_xevent,
                                                                GdkEvent                  *event,
                                                                gpointer                   data);



struct _XfceShortcutsGrabberPrivate
{
  /* Maps a shortcut string to a pointer to XfceKey */
  GHashTable *keys;

  /* Maps an XfceXGrab to a reference count.
   * The reference count tracks the number of shortcuts that grab the XfceXGrab. */
  GHashTable *grabbed_keycodes;

  gint xkbEventType, xkbStateGroup;
};

struct _XfceKey
{
  guint            keyval;
  GdkModifierType  modifiers;

  /* Information about how the key has been grabbed */
  guint            n_keys;  /* Equals 0 if the key isn't grabbed */
  GdkKeymapKey    *keys;
  GdkModifierType  non_virtual_modifiers;
  guint            numlock_modifier;
};

typedef struct
{
  guint            keycode;
  GdkModifierType  non_virtual_modifiers;
  guint            numlock_modifier;
} XfceXGrab;

typedef guint XfceXGrabRefcount;



G_DEFINE_TYPE_WITH_PRIVATE (XfceShortcutsGrabber, xfce_shortcuts_grabber, G_TYPE_OBJECT)



static void
xfce_shortcuts_grabber_class_init (XfceShortcutsGrabberClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->constructed = xfce_shortcuts_grabber_constructed;
  gobject_class->finalize = xfce_shortcuts_grabber_finalize;

  g_signal_new ("shortcut-activated",
                XFCE_TYPE_SHORTCUTS_GRABBER,
                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                0,
                NULL,
                NULL,
                _xfce_shortcuts_marshal_VOID__STRING_INT,
                G_TYPE_NONE,
                2,
                G_TYPE_STRING, G_TYPE_INT);
}



static void
free_key (gpointer data)
{
  XfceKey *key = data;
  g_free (key->keys);
  g_free (key);
}

static gboolean
xgrab_equal (gconstpointer data1, gconstpointer data2)
{
  const XfceXGrab *a = data1;
  const XfceXGrab *b = data2;

  if (a == b)
    return TRUE;

  return a->keycode == b->keycode &&
         a->non_virtual_modifiers == b->non_virtual_modifiers &&
         a->numlock_modifier == b->numlock_modifier;
}

static void
xgrab_free (gpointer data)
{
  XfceXGrab *g = data;
  g_free (g);
}

static guint
xgrab_hash (gconstpointer data)
{
  const XfceXGrab *g = data;
  return g->keycode ^ g->non_virtual_modifiers ^ g->numlock_modifier;
}




static void
xfce_shortcuts_grabber_init (XfceShortcutsGrabber *grabber)
{
  GdkDisplay      *display;
  GdkKeymap       *keymap;

  grabber->priv = xfce_shortcuts_grabber_get_instance_private (grabber);
  grabber->priv->keys = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, free_key);
  grabber->priv->grabbed_keycodes = g_hash_table_new_full (xgrab_hash, xgrab_equal, xgrab_free, g_free);

  /* Workaround: Make sure modmap is up to date
   * There is possibly a bug in GTK+ where virtual modifiers are not
   * mapped because the modmap is not updated. The following function
   * updates it.
   */
  display = gdk_display_get_default ();
  keymap = gdk_keymap_get_for_display (display);
  (void) gdk_keymap_have_bidi_layouts (keymap);
}



static void
xfce_shortcuts_grabber_constructed (GObject *object)
{
  GdkDisplay *display;
  Display    *xdisplay;
  GdkKeymap  *keymap;

  XfceShortcutsGrabber *grabber = XFCE_SHORTCUTS_GRABBER (object);

  display = gdk_display_get_default ();
  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  keymap = gdk_keymap_get_for_display (display);
  g_signal_connect (keymap, "keys-changed", G_CALLBACK (xfce_shortcuts_grabber_keys_changed),
                    grabber);

  if (G_UNLIKELY (!XkbQueryExtension (xdisplay, 0, &grabber->priv->xkbEventType, 0, 0, 0)))
    grabber->priv->xkbEventType = -1;
#if TRACK_LAYOUT_CHANGE
  grabber->priv->xkbStateGroup = -1;
#else
  grabber->priv->xkbStateGroup = 0;
#endif

  /* Flush events before adding the event filter */
  XAllowEvents (xdisplay, AsyncBoth, CurrentTime);

  /* Add event filter */
  gdk_window_add_filter (NULL, xfce_shortcuts_grabber_event_filter, grabber);
}



static void
xfce_shortcuts_grabber_finalize (GObject *object)
{
  XfceShortcutsGrabber *grabber = XFCE_SHORTCUTS_GRABBER (object);

  xfce_shortcuts_grabber_ungrab_all (grabber);
  g_hash_table_unref (grabber->priv->keys);
  g_hash_table_unref (grabber->priv->grabbed_keycodes);

  (*G_OBJECT_CLASS (xfce_shortcuts_grabber_parent_class)->finalize) (object);
}



static void
xfce_shortcuts_grabber_keys_changed (GdkKeymap            *keymap,
                                     XfceShortcutsGrabber *grabber)
{
  g_return_if_fail (XFCE_IS_SHORTCUTS_GRABBER (grabber));

  TRACE ("Keys changed, regrabbing");

  xfce_shortcuts_grabber_regrab_all (grabber);
}



static gboolean
xfce_shortcuts_grabber_xgrab (XfceXGrab g, gboolean grab)
{
  GdkDisplay *display = gdk_display_get_default ();
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
  Window root_window;
  guint k;
  gboolean success = TRUE;

  /* Ignorable modifiers */
  const guint mod_masks [] = {
    0,
    GDK_MOD2_MASK,
    g.numlock_modifier | GDK_MOD2_MASK,
    GDK_LOCK_MASK,
    g.numlock_modifier | GDK_LOCK_MASK,
    GDK_MOD5_MASK,
    g.numlock_modifier | GDK_MOD5_MASK,
    GDK_MOD2_MASK | GDK_LOCK_MASK,
    g.numlock_modifier | GDK_MOD2_MASK | GDK_LOCK_MASK,
    GDK_MOD2_MASK | GDK_MOD5_MASK,
    g.numlock_modifier | GDK_MOD2_MASK | GDK_MOD5_MASK,
    GDK_LOCK_MASK | GDK_MOD5_MASK,
    g.numlock_modifier | GDK_LOCK_MASK | GDK_MOD5_MASK,
    GDK_MOD2_MASK | GDK_LOCK_MASK | GDK_MOD5_MASK,
    g.numlock_modifier | GDK_MOD2_MASK | GDK_LOCK_MASK | GDK_MOD5_MASK,
  };

  /* Retrieve the root window of the screen */
  root_window = GDK_WINDOW_XID (gdk_screen_get_root_window (gdk_display_get_default_screen (display)));

  TRACE ("%s keycode %u, non_virtual_modifiers 0x%x",
         grab ? "Grabbing" : "Ungrabbing",
         g.keycode, g.non_virtual_modifiers);

  gdk_x11_display_error_trap_push (display);

  for (k = 0; k < G_N_ELEMENTS (mod_masks); k++)
    {
      /* Take ignorable modifiers into account when grabbing/ungrabbing */
      if (grab)
        XGrabKey (xdisplay,
                  g.keycode,
                  g.non_virtual_modifiers | mod_masks [k],
                  root_window,
                  False, GrabModeAsync, GrabModeAsync);
      else
        XUngrabKey (xdisplay,
                    g.keycode,
                    g.non_virtual_modifiers | mod_masks [k],
                    root_window);
    }

  gdk_display_flush (display);
  if (gdk_x11_display_error_trap_pop (display))
    {
      g_warning ("Failed to %s keycode %u",
                 grab ? "grab" : "ungrab", g.keycode);
      success = FALSE;
    }

  return success;
}



static gboolean
ungrab_key (const gchar          *shortcut,
            XfceKey              *key,
            XfceShortcutsGrabber *grabber)
{
  xfce_shortcuts_grabber_ungrab (grabber, key);
  return FALSE;
}



static void
xfce_shortcuts_grabber_ungrab_all (XfceShortcutsGrabber *grabber)
{
  g_return_if_fail (XFCE_IS_SHORTCUTS_GRABBER (grabber));
  g_hash_table_foreach (grabber->priv->keys,
                        (GHFunc) (void (*)(void)) ungrab_key,
                        grabber);
}



static gboolean
get_entries_for_keyval (GdkKeymap        *keymap,
                        gint              group,
                        guint             keyval,
                        GdkKeymapKey    **keys_out,
                        guint            *n_keys_out)
{
  GdkKeymapKey *keys;
  gint n_keys;

  *keys_out = NULL;
  *n_keys_out = 0;

   /* Get all keys generating keyval */
  if (!gdk_keymap_get_entries_for_keyval (keymap, keyval, &keys, &n_keys))
    {
      TRACE ("Got no keys for keyval");
      return FALSE;
    }

  if (G_UNLIKELY (n_keys <= 0))
    {
      g_free (keys);
      return FALSE;
    }

  /* Filter keys by group */
  {
    gboolean group0_only = TRUE;

    /* For keys such as F12:
     *   keys1[i].group is always 0 (even if n_keys1 >= 2)
     *   and thus n_matches will be zero if group != 0 */
    for (gint i = 0; i < n_keys; i++)
      {
        if (keys[i].group != 0)
          {
            group0_only = FALSE;
            break;
          }
      }

    if (!group0_only)
      {
        /* Remove keys that do not match the group*/
        for (gint i = 0; i < n_keys;)
          if (keys[i].group == group)
            i++;
          else
            keys[i] = keys[--n_keys];
      }
  }

  if (G_UNLIKELY (n_keys == 0))
    {
      g_free (keys);
      return FALSE;
    }

  *keys_out = keys;
  *n_keys_out = n_keys;
  return TRUE;
}



static gboolean
map_virtual_modifiers (GdkKeymap       *keymap,
                       GdkModifierType  virtual_modifiers,
                       GdkModifierType *non_virtual_modifiers)
{
  /* Map virtual modifiers to non-virtual modifiers */
  GdkModifierType non_virtual = virtual_modifiers;
  if (!gdk_keymap_map_virtual_modifiers (keymap, &non_virtual))
    return FALSE;

  if (non_virtual == virtual_modifiers &&
      (GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK) & non_virtual)
    {
      TRACE ("Failed to map virtual modifiers");
      return FALSE;
    }

  *non_virtual_modifiers = non_virtual;
  return TRUE;
}




static void
xfce_shortcuts_grabber_regrab_all (XfceShortcutsGrabber *grabber)
{
  GdkDisplay     *display;
  Display        *xdisplay;
  GdkKeymap      *keymap;
  guint           numlock_modifier;
  GHashTable     *grabbed_keycodes;
  GHashTableIter  iter;
  gpointer        hash_value;
  guint           n_already_grabbed = 0;
  guint           n_regrab = 0;
  XfceKey       **regrab; /* list of keys to re-grab */
  guint           i;
  gint            group;

  g_return_if_fail (XFCE_IS_SHORTCUTS_GRABBER (grabber));

  display = gdk_display_get_default ();
  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  keymap = gdk_keymap_get_for_display (display);
  numlock_modifier = XkbKeysymToModifiers (xdisplay, GDK_KEY_Num_Lock);
  grabbed_keycodes = grabber->priv->grabbed_keycodes;
  group = grabber->priv->xkbStateGroup;
  if (G_UNLIKELY (group == -1))
    group = 0;

  regrab = g_new (XfceKey *, g_hash_table_size (grabber->priv->keys));

  /* Phase 1: Ungrab all keys that need to be re-grabbed
   *          and collect them into the 'regrab' list */
  g_hash_table_iter_init (&iter, grabber->priv->keys);
  while (g_hash_table_iter_next (&iter, NULL, &hash_value))
  {
    XfceKey         *const key = hash_value;
    GdkKeymapKey    *keys;
    GdkModifierType  non_virtual_modifiers;
    guint            n_keys;
    gboolean         already_grabbed;

    if (!map_virtual_modifiers (keymap, key->modifiers, &non_virtual_modifiers))
      continue;
    if (!get_entries_for_keyval (keymap, group, key->keyval, &keys, &n_keys))
      continue;

    already_grabbed = TRUE;
    if (key->n_keys == n_keys &&
        key->non_virtual_modifiers == non_virtual_modifiers &&
        key->numlock_modifier == numlock_modifier)
      {
        guint j;
        for (j = 0; j < n_keys; j++)
          if (memcmp (&key->keys[j], &keys[j], sizeof(*keys)) != 0)
            {
              already_grabbed = FALSE;
              break;
            }
      }
    else
      already_grabbed = FALSE;

    if (already_grabbed)
      {
        n_already_grabbed++;
        g_free (keys);
      }
    else
      {
        /* Undo current X11 grabs of the key */
        if (key->n_keys != 0)
          xfce_shortcuts_grabber_ungrab (grabber, key);

        /* Set key->keys to the keycodes that need to be grabbed in phase 2 */
        if (G_UNLIKELY (key->keys))
        {
          g_free (key->keys);
          key->keys = NULL;
        }
        key->n_keys = n_keys;
        if (n_keys != 0)
          key->keys = keys;
        else
          g_free (keys);
        key->non_virtual_modifiers = non_virtual_modifiers;
        key->numlock_modifier = numlock_modifier;

        /* Remember to grab the key in phase 2 */
        if (n_keys != 0)
          regrab[n_regrab++] = key;
      }
  }

  TRACE ("n_already_grabbed=%u, n_regrab=%u", n_already_grabbed, n_regrab);

  /* Phase 2: Grab all keys that have been stored in the 'regrab' list */
  for (i = 0; i < n_regrab; i++)
  {
    XfceKey *const key = regrab[i];
    guint    j;

#ifdef DEBUG_TRACE
    {
      gchar *shortcut_name = gtk_accelerator_name (key->keyval, key->non_virtual_modifiers);
      TRACE (key->n_keys==0 ? "Grabbing %s" : "Regrabbing %s", shortcut_name);
      TRACE ("  key->keyval: %d", key->keyval);
      TRACE ("  key->modifiers: 0x%x", key->modifiers);
      TRACE ("  key->non_virtual_modifiers: 0x%x", key->non_virtual_modifiers);
      TRACE ("  key->n_keys: %u", key->n_keys);
      g_free (shortcut_name);
    }
#endif

    /* Grab all hardware keys generating keyval */
    for (j = 0; j < key->n_keys;)
      {
        XfceXGrab g;
        gpointer  refcount;

        g.keycode = key->keys[j].keycode;
        g.non_virtual_modifiers = key->non_virtual_modifiers;
        g.numlock_modifier = key->numlock_modifier;
        if (!g_hash_table_lookup_extended (grabbed_keycodes, &g, NULL, &refcount))
          {
            if (xfce_shortcuts_grabber_xgrab (g, TRUE))
              {
                XfceXGrab *g1 = g_new (XfceXGrab, 1);
                XfceXGrabRefcount *refcount1 = g_new (XfceXGrabRefcount, 1);
                *g1 = g;
                *refcount1 = 1;
                g_hash_table_insert (grabbed_keycodes, g1, refcount1);
                j++;
              }
            else
              /* Failed to grab key->keys[j], remove it from key->keys */
              key->keys[j] = key->keys[--key->n_keys];
          }
        else
          {
            // 'g' has already been grabbed, increment its refcount only
            XfceXGrabRefcount *refcount1 = refcount;
            (*refcount1)++;
            TRACE ("keycode %u, non_virtual_modifiers 0x%x: ++refcount = %u",
                   g.keycode, g.non_virtual_modifiers, *refcount1);
            j++;
          }
      }

    if (key->n_keys == 0 && key->keys != NULL)
      {
        g_free (key->keys);
        key->keys = NULL;
      }
  }

  g_free (regrab);
}



static void
xfce_shortcuts_grabber_grab (XfceShortcutsGrabber *grabber, XfceKey *key)
{
  GdkDisplay      *display;
  Display         *xdisplay;
  GdkKeymap       *keymap;
  guint            numlock_modifier;
  GHashTable      *grabbed_keycodes;
  GdkKeymapKey    *keys;
  GdkModifierType  non_virtual_modifiers;
  guint            i, n_keys;
  gint             group;

  display = gdk_display_get_default ();
  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  keymap = gdk_keymap_get_for_display (display);
  numlock_modifier = XkbKeysymToModifiers (xdisplay, GDK_KEY_Num_Lock);
  grabbed_keycodes = grabber->priv->grabbed_keycodes;
  group = grabber->priv->xkbStateGroup;
  if (G_UNLIKELY (group == -1))
    group = 0;

  if (!map_virtual_modifiers (keymap, key->modifiers, &non_virtual_modifiers))
    return;
  if (!get_entries_for_keyval (keymap, group, key->keyval, &keys, &n_keys))
    return;

#ifdef DEBUG_TRACE
  {
    char *shortcut_name = gtk_accelerator_name (key->keyval, non_virtual_modifiers);
    TRACE (key->n_keys==0 ? "Grabbing %s" : "Regrabbing %s", shortcut_name);
    TRACE ("  key->keyval: %d", key->keyval);
    TRACE ("  key->modifiers: 0x%x", key->modifiers);
    TRACE ("  non_virtual_modifiers: 0x%x", non_virtual_modifiers);
    TRACE ("  n_keys: %u", n_keys);
    g_free (shortcut_name);
  }
#endif

  /* Undo old grabs (just in case there are some old grabs) */
  if (G_UNLIKELY (key->n_keys != 0))
    {
      g_warning ("keyval %u already grabbed", key->keyval);
      xfce_shortcuts_grabber_ungrab (grabber, key);
    }

  /* Grab all hardware keys generating keyval */
  for (i = 0; i < n_keys;)
    {
      XfceXGrab g;
      gpointer  refcount;

      g.keycode = keys[i].keycode;
      g.non_virtual_modifiers = non_virtual_modifiers;
      g.numlock_modifier = numlock_modifier;
      if (!g_hash_table_lookup_extended (grabbed_keycodes, &g, NULL, &refcount))
        {
          if (xfce_shortcuts_grabber_xgrab (g, TRUE))
            {
              XfceXGrab *g1 = g_new (XfceXGrab, 1);
              XfceXGrabRefcount *refcount1 = g_new (XfceXGrabRefcount, 1);
              *g1 = g;
              *refcount1 = 1;
              g_hash_table_insert (grabbed_keycodes, g1, refcount1);
              TRACE ("group %d, keycode %u, non_virtual_modifiers 0x%x: refcount := %u",
                     keys[i].group, g.keycode, g.non_virtual_modifiers, *refcount1);
              i++;
            }
          else
            /* Failed to grab keys[i], remove it from keys */
            keys[i] = keys[--n_keys];
        }
      else
        {
          // 'g' has already been grabbed, increment its refcount only
          XfceXGrabRefcount *refcount1 = refcount;
          (*refcount1)++;
          TRACE ("group %d, keycode %u, non_virtual_modifiers 0x%x: ++refcount = %u",
                 keys[i].group, g.keycode, g.non_virtual_modifiers, *refcount1);
          i++;
        }
    }

  /* Set key->keys to the list of keys that been succesfully grabbed */
  g_free (key->keys);
  key->keys = NULL;
  key->n_keys = n_keys;
  if (n_keys != 0)
    key->keys = keys;
  else
    g_free (keys);
  key->non_virtual_modifiers = non_virtual_modifiers;
  key->numlock_modifier = numlock_modifier;
}

static void
xfce_shortcuts_grabber_ungrab (XfceShortcutsGrabber *grabber, XfceKey *key)
{
  GHashTable *grabbed_keycodes;
  guint       i;

  grabbed_keycodes = grabber->priv->grabbed_keycodes;

#ifdef DEBUG_TRACE
  {
    gchar *shortcut_name = gtk_accelerator_name (key->keyval, key->non_virtual_modifiers);
    TRACE ("Ungrabbing %s", shortcut_name);
    TRACE ("  key->keyval: %d", key->keyval);
    TRACE ("  key->modifiers: 0x%x", key->modifiers);
    TRACE ("  key->non_virtual_modifiers: 0x%x", key->non_virtual_modifiers);
    TRACE ("  key->n_keys: %u", key->n_keys);
    g_free (shortcut_name);
  }
#endif

  for (i = 0; i < key->n_keys; i++)
    {
      XfceXGrab g;
      gpointer  refcount;

      g.keycode = key->keys[i].keycode;
      g.non_virtual_modifiers = key->non_virtual_modifiers;
      g.numlock_modifier = key->numlock_modifier;
      if (G_LIKELY (g_hash_table_lookup_extended (grabbed_keycodes, &g, NULL, &refcount)))
        {
          XfceXGrabRefcount *refcount1 = refcount;
          if (G_LIKELY (*refcount1 != 0))
            {
              (*refcount1)--;
              TRACE ("group %d, keycode %u, non_virtual_modifiers 0x%x: --refcount = %u",
                     key->keys[i].group, g.keycode, g.non_virtual_modifiers, *refcount1);
              if(*refcount1 == 0)
                {
                  xfce_shortcuts_grabber_xgrab (g, FALSE);
                  g_hash_table_remove (grabbed_keycodes, &g);
                }
            }
          else
            {
              g_warning ("corrupted refcount");
            }
        }
      else
        {
          g_warning ("corrupted hashtable");
        }
    }

  g_free (key->keys);
  key->keys = NULL;
  key->n_keys = 0;
  key->non_virtual_modifiers = 0;
  key->numlock_modifier = 0;
}



struct EventKeyFindContext
{
  GdkModifierType modifiers;
  guint           keyval;
  const gchar    *result;
};



static gboolean
find_event_key (const gchar                *shortcut,
                XfceKey                    *key,
                struct EventKeyFindContext *context)
{
  g_return_val_if_fail (context != NULL, FALSE);

  TRACE ("Comparing to %s", shortcut);

  if ((key->modifiers & (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK | GDK_SUPER_MASK))
      == (context->modifiers)
      && (key->keyval == context->keyval))
    {
      context->result = shortcut;

      TRACE ("Positive match for %s", context->result);
      return TRUE;
    }

  return FALSE;
}



static gboolean
is_modifier_key (struct EventKeyFindContext context)
{
  if (context.modifiers == 0)
    {
      switch (context.keyval)
        {
          case GDK_KEY_Control_L:
          case GDK_KEY_Control_R:
          case GDK_KEY_Alt_L:
          case GDK_KEY_Alt_R:
          case GDK_KEY_Super_L:
          case GDK_KEY_Super_R:
            return TRUE;
        }
    }

  return FALSE;
}



static GdkFilterReturn
xfce_shortcuts_grabber_event_filter (GdkXEvent *gdk_xevent,
                                     GdkEvent  *event,
                                     gpointer   data)
{
  XfceShortcutsGrabber       *const grabber = data;
  struct EventKeyFindContext  context;
  GdkKeymap                  *keymap;
  GdkModifierType             consumed, modifiers;
  GdkDisplay                 *display;
  XEvent                     *xevent;
  guint                       keyval, mod_mask;
  gchar                      *raw_shortcut_name;
  gint                        timestamp;

  /* We only activate single modifier keys on release event to allow combinations
   * such as Super to open a menu and Super+T to open a terminal: see
   * https://gitlab.xfce.org/xfce/libxfce4ui/-/issues/1 */
  static gboolean             single_modifier_down = FALSE;

  g_return_val_if_fail (XFCE_IS_SHORTCUTS_GRABBER (grabber), GDK_FILTER_CONTINUE);

  xevent = (XEvent *) gdk_xevent;

#if TRACK_LAYOUT_CHANGE
  if (xevent->type == grabber->priv->xkbEventType)
    {
      const XkbEvent *e = (const XkbEvent*) xevent;
      if (e->any.xkb_type == XkbStateNotify)
        {
          if (grabber->priv->xkbStateGroup != e->state.group)
            {
              TRACE ("xkb event: any.xkb_type=XkbStateNotify, state.group=%d", e->state.group);
              grabber->priv->xkbStateGroup = e->state.group;
              xfce_shortcuts_grabber_regrab_all (grabber);
            }
        }
    }
#endif

  if (xevent->type != KeyPress && !(xevent->type == KeyRelease && single_modifier_down))
    return GDK_FILTER_CONTINUE;

  context.result = NULL;
  timestamp = xevent->xkey.time;

  /* Get the keyboard state */
  display = gdk_display_get_default ();
  gdk_x11_display_error_trap_push (display);
  keymap = gdk_keymap_get_for_display (display);
  mod_mask = gtk_accelerator_get_default_mod_mask ();
  modifiers = xevent->xkey.state;

  /* Remove modifier added to single modifier key on release event */
  if (xevent->type == KeyRelease && single_modifier_down)
    modifiers = 0;

  gdk_keymap_translate_keyboard_state (keymap, xevent->xkey.keycode,
                                       modifiers,
                                       grabber->priv->xkbStateGroup,
                                       &keyval, NULL, NULL, &consumed);

  /* We want Alt + Print to be Alt + Print not SysReq. See bug #7897 */
  if (keyval == GDK_KEY_Sys_Req && (modifiers & GDK_MOD1_MASK) != 0)
    {
      consumed = 0;
      keyval = GDK_KEY_Print;
    }

  /* Get the modifiers */

  /* If Shift was used when translating the keyboard state, we remove it
   * from the consumed bit because gtk_accelerator_{name,parse} fail to
   * handle this correctly. This allows us to have shortcuts with Shift
   * as a modifier key (see bug #8744). */
  if ((modifiers & GDK_SHIFT_MASK) && (consumed & GDK_SHIFT_MASK))
    consumed &= ~GDK_SHIFT_MASK;

  /*
   * !!! FIX ME !!!
   * Turn MOD4 into SUPER key press events. Although it is not clear if
   * this is a proper solution, it fixes bug #10373 which some people
   * experience without breaking functionality for other users.
   */
  if (modifiers & GDK_MOD4_MASK)
    {
      modifiers &= ~GDK_MOD4_MASK;
      modifiers |= GDK_SUPER_MASK;
      consumed &= ~GDK_MOD4_MASK;
      consumed &= ~GDK_SUPER_MASK;
    }

  modifiers &= ~consumed;
  modifiers &= mod_mask;

  /* Use the keyval and modifiers values of gtk_accelerator_parse. We
   * will compare them with values we also get from this function and as
   * it has its own logic, it's easier and safer to do so.
   * See bug #8744 for a "live" example. */
  raw_shortcut_name = gtk_accelerator_name (keyval, modifiers);
  gtk_accelerator_parse (raw_shortcut_name, &context.keyval, &context.modifiers);

  TRACE ("Looking for %s", raw_shortcut_name);
  g_free (raw_shortcut_name);

  g_hash_table_find (grabber->priv->keys,
                     (GHRFunc) (void (*)(void)) find_event_key,
                     &context);

  single_modifier_down = FALSE;
  if (G_LIKELY (context.result != NULL))
    {
      /* We had a positive match */
      if (xevent->type == KeyPress && is_modifier_key (context))
        {
          /* Will be activated on release event */
          single_modifier_down = TRUE;
        }
      else
        {
          g_signal_emit_by_name (grabber, "shortcut-activated",
                                 context.result, timestamp);
        }
    }

  gdk_display_flush (display);
  gdk_x11_display_error_trap_pop_ignored (display);

  return GDK_FILTER_CONTINUE;
}



XfceShortcutsGrabber *
xfce_shortcuts_grabber_new (void)
{
  return g_object_new (XFCE_TYPE_SHORTCUTS_GRABBER, NULL);
}



void
xfce_shortcuts_grabber_add (XfceShortcutsGrabber *grabber,
                            const gchar          *shortcut)
{
  XfceKey *key;

  g_return_if_fail (XFCE_IS_SHORTCUTS_GRABBER (grabber));
  g_return_if_fail (shortcut != NULL);

  key = g_new0 (XfceKey, 1);

  gtk_accelerator_parse (shortcut, &key->keyval, &key->modifiers);
  TRACE ("parse %s -> keyval=0x%x, modifiers=0x%x", shortcut, key->keyval, key->modifiers);

  if (G_LIKELY (key->keyval != 0))
    {
      xfce_shortcuts_grabber_grab (grabber, key);
      g_hash_table_insert (grabber->priv->keys, g_strdup (shortcut), key);
    }
  else
    {
      free_key (key);
    }
}



void
xfce_shortcuts_grabber_remove (XfceShortcutsGrabber *grabber,
                               const gchar          *shortcut)
{
  XfceKey *key;

  g_return_if_fail (XFCE_IS_SHORTCUTS_GRABBER (grabber));
  g_return_if_fail (shortcut != NULL);

  key = g_hash_table_lookup (grabber->priv->keys, shortcut);

  if (G_LIKELY (key != NULL))
    {
      xfce_shortcuts_grabber_ungrab (grabber, key);
      g_hash_table_remove (grabber->priv->keys, shortcut);
    }
}
