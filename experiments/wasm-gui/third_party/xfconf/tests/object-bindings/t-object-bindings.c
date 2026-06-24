/*
 *  xfconf
 *
 *  Copyright (c) 2009 Nick Schermer <nick@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "tests-common.h"

enum
{
    PROP_0,
    PROP_TEST
};

typedef struct _TestObject TestObject;
typedef struct _TestObjectClass TestObjectClass;

struct _TestObjectClass
{
    GObjectClass __parent__;
};

struct _TestObject
{
    GObject __parent__;

    gboolean test;
};

GType test_object_get_type(void) G_GNUC_CONST;
static void test_object_get_property(GObject *object,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec);
static void test_object_set_property(GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec);

G_DEFINE_TYPE(TestObject, test_object, G_TYPE_OBJECT)

static gboolean was_set = FALSE;

static void
test_object_class_init(TestObjectClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->get_property = test_object_get_property;
    gobject_class->set_property = test_object_set_property;

    g_object_class_install_property(gobject_class,
                                    PROP_TEST,
                                    g_param_spec_boolean("test",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE));
}

static void
test_object_init(TestObject *object)
{
}

static void
test_object_get_property(GObject *object,
                         guint prop_id,
                         GValue *value,
                         GParamSpec *pspec)
{
    TestObject *test = (TestObject *)object;

    switch(prop_id) {
        case PROP_TEST:
            g_value_set_boolean(value, test->test);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
test_object_set_property(GObject *object,
                         guint prop_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
    TestObject *test = (TestObject *)object;

    switch(prop_id) {
        case PROP_TEST:
            test->test = g_value_get_boolean(value);
            was_set = TRUE;
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}


int
main(int argc,
     char **argv)
{
    XfconfChannel *channel;
    GObject *object;
    gulong id;
    gboolean initial_property_was_set;
    gboolean property_was_changed;

    if(!xfconf_tests_start())
        return 1;

    channel = xfconf_channel_new(TEST_CHANNEL_NAME);

    {
        TEST_OPERATION(xfconf_channel_set_bool(channel, "/bindings/test", TRUE));

        object = g_object_new(test_object_get_type(), NULL);

        /* set if we set the property when connecting the binding */
        was_set = FALSE;
        id = xfconf_g_property_bind(channel, "/bindings/test",
                                    G_TYPE_BOOLEAN, object, "test");
        initial_property_was_set = was_set;
        TEST_OPERATION(initial_property_was_set);

        /* change channel property, see if binding works */
        was_set = FALSE;
        xfconf_channel_set_bool(channel, "/bindings/test", FALSE);
        property_was_changed = was_set;
        TEST_OPERATION(property_was_changed);

        /* unbind, object should not get the new channel value */
        xfconf_g_property_unbind(id);
        was_set = FALSE;
        xfconf_channel_set_bool(channel, "/bindings/test", TRUE);
        property_was_changed = was_set;
        TEST_OPERATION(!property_was_changed);

        /* reconnect binding a couple of times */
        xfconf_g_property_bind(channel, "/bindings/test1",
                               G_TYPE_BOOLEAN, object, "test");
        xfconf_g_property_bind(channel, "/bindings/test2",
                               G_TYPE_BOOLEAN, object, "test");
        xfconf_g_property_bind(channel, "/bindings/test3",
                               G_TYPE_BOOLEAN, object, "test");

        /* test unbind all on object */
        xfconf_g_property_unbind_all(object);
        was_set = FALSE;
        xfconf_channel_set_bool(channel, "/bindings/test", FALSE);
        property_was_changed = was_set;
        TEST_OPERATION(!property_was_changed);

        /* reconnect binding a couple of times */
        xfconf_g_property_bind(channel, "/bindings/test1",
                               G_TYPE_BOOLEAN, object, "test");
        xfconf_g_property_bind(channel, "/bindings/test2",
                               G_TYPE_BOOLEAN, object, "test");
        xfconf_g_property_bind(channel, "/bindings/test3",
                               G_TYPE_BOOLEAN, object, "test");

        /* test unbind all on channel */
        xfconf_g_property_unbind_all(channel);
        was_set = FALSE;
        xfconf_channel_set_bool(channel, "/bindings/test", TRUE);
        property_was_changed = was_set;
        TEST_OPERATION(!property_was_changed);

        /* reconnect */
        xfconf_g_property_bind(channel, "/bindings/test",
                               G_TYPE_BOOLEAN, object, "test");

        /* unbind property by name */
        xfconf_g_property_unbind_by_property(channel, "/bindings/test",
                                             object, "test");
        was_set = FALSE;
        xfconf_channel_set_bool(channel, "/bindings/test", FALSE);
        property_was_changed = was_set;
        TEST_OPERATION(!property_was_changed);

        g_object_unref(G_OBJECT(object));
    }

    g_object_unref(G_OBJECT(channel));

    xfconf_tests_end();

    return 0;
}
