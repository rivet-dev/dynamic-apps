/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/*
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2010 - Jesse van den Kieboom
 * Copyright (C) 2014 - Sébastien Wilmet
 *
 * GtkSourceView is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GtkSourceView is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <string.h>
#include <sys/stat.h>
#include <glib/gprintf.h>
#include <gtksourceview/gtksource.h>

#define ENABLE_REMOTE_TESTS	FALSE

/* linux/bsd has it. others such as Solaris, do not */
#ifndef ACCESSPERMS
#define ACCESSPERMS (S_IRWXU|S_IRWXG|S_IRWXO)
#endif

#define DEFAULT_REMOTE_URI_DIR "sftp://localhost/tmp/"
#define DEFAULT_TEST_TEXT_FILE "gtksourceview-file-saver-test.txt"
#define DEFAULT_TEST_UNOWNED_TEXT_FILE "gtksourceview-file-saver-unowned-group.txt"
#define DEFAULT_UNOWNED_DIR "gtksourceview-file-saver-unowned"

#define DEFAULT_REMOTE_URI DEFAULT_REMOTE_URI_DIR DEFAULT_TEST_TEXT_FILE
#define DEFAULT_CONTENT "hello world!"
#define DEFAULT_CONTENT_RESULT "hello world!\n"

#define UNOWNED_REMOTE_DIRECTORY DEFAULT_REMOTE_URI_DIR DEFAULT_UNOWNED_DIR

typedef struct _SaverTestData SaverTestData;
typedef void (*SavedCallback) (SaverTestData *data);

struct _SaverTestData
{
	GtkSourceFileSaver *saver;
	GFile *location;
	const gchar *expected_file_contents;
	SavedCallback saved_callback;
	gpointer userdata;

	guint file_existed : 1;
};

static const gchar *
read_file (GFile *location)
{
	/* TODO use g_file_load_contents() */
	GError *error = NULL;
	static gchar buffer[4096];
	gsize read;

	GInputStream *stream = G_INPUT_STREAM (g_file_read (location, NULL, &error));

	g_assert_no_error (error);

	g_input_stream_read_all (stream, buffer, sizeof (buffer) - 1, &read, NULL, &error);
	g_assert_no_error (error);

	buffer[read] = '\0';

	g_input_stream_close (stream, NULL, NULL);

	g_object_unref (stream);

	return buffer;
}

static void
save_file_cb (GtkSourceFileSaver *saver,
	      GAsyncResult       *result,
	      SaverTestData      *data)
{
	GError *error = NULL;

	gtk_source_file_saver_save_finish (saver, result, &error);

	g_assert_no_error (error);

	g_assert_cmpstr (data->expected_file_contents, ==, read_file (data->location));

	if (data->saved_callback != NULL)
	{
		data->saved_callback (data);
	}

	if (!data->file_existed)
	{
		g_file_delete (data->location, NULL, NULL);
	}

	/* finished */
	gtk_main_quit ();
}

static void
save_file (SaverTestData *data)
{
	data->file_existed = g_file_query_exists (data->location, NULL);

	gtk_source_file_saver_save_async (data->saver,
					  G_PRIORITY_DEFAULT,
					  NULL, NULL, NULL, NULL,
					  (GAsyncReadyCallback) save_file_cb,
					  data);
}

static void
mount_cb (GFile         *location,
	  GAsyncResult  *result,
	  SaverTestData *data)
{
	GError *error = NULL;

	g_file_mount_enclosing_volume_finish (location, result, &error);

	if (error != NULL && error->code == G_IO_ERROR_ALREADY_MOUNTED)
	{
		g_error_free (error);
	}
	else
	{
		g_assert_no_error (error);
	}

	save_file (data);
}

static void
check_mounted (SaverTestData *data)
{
	GMountOperation *mount_operation;

	if (g_file_is_native (data->location))
	{
		save_file (data);
		return;
	}

	mount_operation = gtk_mount_operation_new (NULL);

	g_file_mount_enclosing_volume (data->location,
	                               G_MOUNT_MOUNT_NONE,
	                               mount_operation,
	                               NULL,
	                               (GAsyncReadyCallback) mount_cb,
	                               data);

	g_object_unref (mount_operation);
}

static void
test_saver (const gchar            *filename_or_uri,
	    const gchar            *buffer_contents,
	    const gchar            *expected_file_contents,
	    GtkSourceNewlineType    newline_type,
	    SavedCallback           saved_callback,
	    gpointer                userdata)
{
	GFile *location;
	GtkSourceBuffer *buffer;
	GtkSourceFile *file;
	GtkSourceFileSaver *saver;
	SaverTestData *data;

	location = g_file_new_for_commandline_arg (filename_or_uri);

	buffer = gtk_source_buffer_new (NULL);
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), buffer_contents, -1);

	file = gtk_source_file_new ();
	saver = gtk_source_file_saver_new_with_target (buffer, file, location);

	gtk_source_file_saver_set_newline_type (saver, newline_type);
	gtk_source_file_saver_set_encoding (saver, gtk_source_encoding_get_utf8 ());

	data = g_slice_new (SaverTestData);
	data->saver = saver;
	data->location = location;
	data->expected_file_contents = expected_file_contents;
	data->saved_callback = saved_callback;
	data->userdata = userdata;

	check_mounted (data);
	gtk_main ();

	g_object_unref (location);
	g_object_unref (buffer);
	g_object_unref (file);
	g_object_unref (saver);
	g_slice_free (SaverTestData, data);
}

typedef struct
{
	GtkSourceNewlineType type;
	const gchar *text;
	const gchar *result;
} NewLineTestData;

static NewLineTestData newline_test_data[] = {
	{GTK_SOURCE_NEWLINE_TYPE_LF, "\nhello\nworld", "\nhello\nworld\n"},
	{GTK_SOURCE_NEWLINE_TYPE_LF, "\nhello\nworld\n", "\nhello\nworld\n\n"},
	{GTK_SOURCE_NEWLINE_TYPE_LF, "\nhello\nworld\n\n", "\nhello\nworld\n\n\n"},
	{GTK_SOURCE_NEWLINE_TYPE_LF, "\r\nhello\r\nworld", "\nhello\nworld\n"},
	{GTK_SOURCE_NEWLINE_TYPE_LF, "\r\nhello\r\nworld\r\n", "\nhello\nworld\n\n"},
	{GTK_SOURCE_NEWLINE_TYPE_LF, "\rhello\rworld", "\nhello\nworld\n"},
	{GTK_SOURCE_NEWLINE_TYPE_LF, "\rhello\rworld\r", "\nhello\nworld\n\n"},
	{GTK_SOURCE_NEWLINE_TYPE_LF, "\nhello\r\nworld", "\nhello\nworld\n"},
	{GTK_SOURCE_NEWLINE_TYPE_LF, "\nhello\r\nworld\r", "\nhello\nworld\n\n"},

	{GTK_SOURCE_NEWLINE_TYPE_CR_LF, "\nhello\nworld", "\r\nhello\r\nworld\r\n"},
	{GTK_SOURCE_NEWLINE_TYPE_CR_LF, "\nhello\nworld\n", "\r\nhello\r\nworld\r\n\r\n"},
	{GTK_SOURCE_NEWLINE_TYPE_CR_LF, "\nhello\nworld\n\n", "\r\nhello\r\nworld\r\n\r\n\r\n"},
	{GTK_SOURCE_NEWLINE_TYPE_CR_LF, "\r\nhello\r\nworld", "\r\nhello\r\nworld\r\n"},
	{GTK_SOURCE_NEWLINE_TYPE_CR_LF, "\r\nhello\r\nworld\r\n", "\r\nhello\r\nworld\r\n\r\n"},
	{GTK_SOURCE_NEWLINE_TYPE_CR_LF, "\rhello\rworld", "\r\nhello\r\nworld\r\n"},
	{GTK_SOURCE_NEWLINE_TYPE_CR_LF, "\rhello\rworld\r", "\r\nhello\r\nworld\r\n\r\n"},
	{GTK_SOURCE_NEWLINE_TYPE_CR_LF, "\nhello\r\nworld", "\r\nhello\r\nworld\r\n"},
	{GTK_SOURCE_NEWLINE_TYPE_CR_LF, "\nhello\r\nworld\r", "\r\nhello\r\nworld\r\n\r\n"},

	{GTK_SOURCE_NEWLINE_TYPE_CR, "\nhello\nworld", "\rhello\rworld\r"},
	{GTK_SOURCE_NEWLINE_TYPE_CR, "\nhello\nworld\n", "\rhello\rworld\r\r"},
	{GTK_SOURCE_NEWLINE_TYPE_CR, "\nhello\nworld\n\n", "\rhello\rworld\r\r\r"},
	{GTK_SOURCE_NEWLINE_TYPE_CR, "\r\nhello\r\nworld", "\rhello\rworld\r"},
	{GTK_SOURCE_NEWLINE_TYPE_CR, "\r\nhello\r\nworld\r\n", "\rhello\rworld\r\r"},
	{GTK_SOURCE_NEWLINE_TYPE_CR, "\rhello\rworld", "\rhello\rworld\r"},
	{GTK_SOURCE_NEWLINE_TYPE_CR, "\rhello\rworld\r", "\rhello\rworld\r\r"},
	{GTK_SOURCE_NEWLINE_TYPE_CR, "\nhello\r\nworld", "\rhello\rworld\r"},
	{GTK_SOURCE_NEWLINE_TYPE_CR, "\nhello\r\nworld\r", "\rhello\rworld\r\r"}
};

static void
test_new_line (const gchar *filename)
{
	gint i;
	gint num = sizeof (newline_test_data) / sizeof (NewLineTestData);

	for (i = 0; i < num; ++i)
	{
		NewLineTestData *nt = &(newline_test_data[i]);

		test_saver (filename,
		            nt->text,
			    nt->result,
		            nt->type,
			    NULL,
			    NULL);
	}
}

static void
test_local_newline (void)
{
	gchar *default_local_uri;

	default_local_uri = g_build_filename (g_get_tmp_dir (),
	                                      DEFAULT_TEST_TEXT_FILE,
	                                      NULL);
	test_new_line (default_local_uri);
	g_free (default_local_uri);
}

static void
test_local (void)
{
	gchar *default_local_uri;

	default_local_uri = g_build_filename (g_get_tmp_dir (),
	                                      DEFAULT_TEST_TEXT_FILE,
	                                      NULL);
	test_saver (default_local_uri,
	            "hello world",
		    "hello world\n",
	            GTK_SOURCE_NEWLINE_TYPE_LF,
		    NULL,
		    NULL);

	test_saver (default_local_uri,
	            "hello world\r\n",
		    "hello world\n\n",
	            GTK_SOURCE_NEWLINE_TYPE_LF,
		    NULL,
		    NULL);

	test_saver (default_local_uri,
	            "hello world\n",
		    "hello world\n\n",
	            GTK_SOURCE_NEWLINE_TYPE_LF,
		    NULL,
		    NULL);
	g_free (default_local_uri);
}

static void
test_remote_newline (void)
{
	test_new_line (DEFAULT_REMOTE_URI);
}

static void
test_remote (void)
{
	test_saver (DEFAULT_REMOTE_URI,
	            "hello world",
		    "hello world\n",
	            GTK_SOURCE_NEWLINE_TYPE_LF,
		    NULL,
		    NULL);

	test_saver (DEFAULT_REMOTE_URI,
	            "hello world\r\n",
		    "hello world\n\n",
	            GTK_SOURCE_NEWLINE_TYPE_LF,
		    NULL,
		    NULL);

	test_saver (DEFAULT_REMOTE_URI,
	            "hello world\n",
		    "hello world\n\n",
	            GTK_SOURCE_NEWLINE_TYPE_LF,
		    NULL,
		    NULL);
}

#ifndef G_OS_WIN32
static void
check_permissions (GFile *location,
                   guint  permissions)
{
	GError *error = NULL;
	GFileInfo *info;

	info = g_file_query_info (location,
	                          G_FILE_ATTRIBUTE_UNIX_MODE,
	                          G_FILE_QUERY_INFO_NONE,
	                          NULL,
	                          &error);

	g_assert_no_error (error);

	g_assert_cmpint (permissions,
	                 ==,
	                 g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE) & ACCESSPERMS);

	g_object_unref (info);
}

static void
check_permissions_saved (SaverTestData *data)
{
	guint permissions = (guint)GPOINTER_TO_INT (data->userdata);

	check_permissions (data->location, permissions);
}

static void
test_permissions (const gchar *uri,
                  guint        permissions)
{
	GError *error = NULL;
	GFile *location = g_file_new_for_commandline_arg (uri);
	GFileOutputStream *stream;
	GFileInfo *info;
	guint mode;

	g_file_delete (location, NULL, NULL);
	stream = g_file_create (location, 0, NULL, &error);

	g_assert_no_error (error);

	g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, NULL);
	g_object_unref (stream);

	info = g_file_query_info (location,
	                          G_FILE_ATTRIBUTE_UNIX_MODE,
	                          G_FILE_QUERY_INFO_NONE,
	                          NULL,
	                          &error);

	g_assert_no_error (error);

	mode = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE);
	g_object_unref (info);

	g_file_set_attribute_uint32 (location,
	                             G_FILE_ATTRIBUTE_UNIX_MODE,
	                             (mode & ~ACCESSPERMS) | permissions,
	                             G_FILE_QUERY_INFO_NONE,
	                             NULL,
	                             &error);
	g_assert_no_error (error);

	check_permissions (location, permissions);

	test_saver (uri,
	            DEFAULT_CONTENT,
		    DEFAULT_CONTENT_RESULT,
	            GTK_SOURCE_NEWLINE_TYPE_LF,
		    check_permissions_saved,
		    GINT_TO_POINTER ((gint)permissions));

	g_file_delete (location, NULL, NULL);
	g_object_unref (location);
}

static void
test_local_permissions (void)
{
	gchar *default_local_uri;

	default_local_uri = g_build_filename (g_get_tmp_dir (),
	                                      DEFAULT_TEST_TEXT_FILE,
	                                      NULL);
	test_permissions (default_local_uri, 0600);
	test_permissions (default_local_uri, 0660);
	test_permissions (default_local_uri, 0666);
	test_permissions (default_local_uri, 0760);
	g_free (default_local_uri);
}
#endif

static void
test_local_unowned_directory (void)
{
	gchar *unowned_local_uri;

	unowned_local_uri = g_build_filename (g_get_tmp_dir (),
	                                             DEFAULT_UNOWNED_DIR,
	                                             DEFAULT_TEST_TEXT_FILE,
                                                 NULL);
	test_saver (unowned_local_uri,
	            DEFAULT_CONTENT,
		    DEFAULT_CONTENT_RESULT,
	            GTK_SOURCE_NEWLINE_TYPE_LF,
		    NULL,
		    NULL);
	g_free (unowned_local_uri);
}

static void
test_remote_unowned_directory (void)
{
	gchar *unowned_remote_uri;

	unowned_remote_uri = g_strconcat (UNOWNED_REMOTE_DIRECTORY,
	                                  DEFAULT_TEST_TEXT_FILE,
	                                  NULL);
	test_saver (unowned_remote_uri,
	            DEFAULT_CONTENT,
		    DEFAULT_CONTENT_RESULT,
	            GTK_SOURCE_NEWLINE_TYPE_LF,
		    NULL,
		    NULL);
	g_free (unowned_remote_uri);
}

#ifndef G_OS_WIN32
static void
test_remote_permissions (void)
{
	test_permissions (DEFAULT_REMOTE_URI, 0600);
	test_permissions (DEFAULT_REMOTE_URI, 0660);
	test_permissions (DEFAULT_REMOTE_URI, 0666);
	test_permissions (DEFAULT_REMOTE_URI, 0760);
}

static void
test_unowned_group_permissions (SaverTestData *data)
{
	GError *error = NULL;
	const gchar *group;
	guint32 mode;

	GFileInfo *info = g_file_query_info (data->location,
	                                     G_FILE_ATTRIBUTE_OWNER_GROUP ","
	                                     G_FILE_ATTRIBUTE_UNIX_MODE,
	                                     G_FILE_QUERY_INFO_NONE,
	                                     NULL,
	                                     &error);

	g_assert_no_error (error);

	group = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_OWNER_GROUP);
	g_assert_cmpstr (group, ==, "root");

	mode = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE);

	g_assert_cmpint (mode & ACCESSPERMS, ==, 0660);

	g_object_unref (info);
}

static void
test_unowned_group (const gchar *uri)
{
	test_saver (uri,
	            DEFAULT_CONTENT,
		    DEFAULT_CONTENT_RESULT,
	            GTK_SOURCE_NEWLINE_TYPE_LF,
		    test_unowned_group_permissions,
		    NULL);
}

static void
test_local_unowned_group (void)
{
	gchar *unowned_group_local_uri;

	unowned_group_local_uri = g_build_filename (g_get_tmp_dir (),
	                                            DEFAULT_TEST_UNOWNED_TEXT_FILE,
	                                            NULL);
	test_unowned_group (unowned_group_local_uri);
	g_free (unowned_group_local_uri);
}

#if 0
static void
test_remote_unowned_group (void)
{
	g_char *unowned_group_remote_uri;

	unowned_group_remote_uri = g_strconcat (DEFAULT_REMOTE_URI_DIR,
	                                        DEFAULT_TEST_UNOWNED_TEXT_FILE,
	                                        NULL);
	test_unowned_group ();
	g_free (unowned_group_remote_uri);
}
#endif

#endif

static gboolean
check_unowned_directory (void)
{
	gchar *unowned_local_directory, *unowned_local_uri;
	GFile *unowned, *unowned_file;
	GFileInfo *info;
	GError *error = NULL;

	unowned_local_directory = g_build_filename (g_get_tmp_dir (),
	                                            DEFAULT_UNOWNED_DIR,
	                                            NULL);
	unowned = g_file_new_for_path (unowned_local_directory);
	g_free (unowned_local_directory);
	g_printf ("*** Checking for unowned directory test... ");

	info = g_file_query_info (unowned,
	                          G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
	                          G_FILE_QUERY_INFO_NONE,
	                          NULL,
	                          &error);

	if (error)
	{
		g_object_unref (unowned);
		g_printf ("NO: directory does not exist\n");

		g_error_free (error);
		return FALSE;
	}

	if (g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
	{
		g_object_unref (unowned);

		g_printf ("NO: directory is writable\n");
		g_object_unref (info);
		return FALSE;
	}

	g_object_unref (info);
	g_object_unref (unowned);
	unowned_local_uri = g_build_filename (g_get_tmp_dir (),
	                                      DEFAULT_UNOWNED_DIR,
	                                      DEFAULT_TEST_TEXT_FILE,
	                                      NULL);
	unowned_file = g_file_new_for_commandline_arg (unowned_local_uri);

	info = g_file_query_info (unowned_file,
	                          G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
	                          G_FILE_QUERY_INFO_NONE,
	                          NULL,
	                          &error);
	g_free (unowned_local_uri);

	if (error)
	{
		g_object_unref (unowned_file);
		g_error_free (error);

		g_printf ("NO: file does not exist\n");
		return FALSE;
	}

	if (!g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
	{
		g_object_unref (unowned_file);

		g_printf ("NO: file is not writable\n");
		g_object_unref (info);
		return FALSE;
	}

	g_object_unref (info);
	g_object_unref (unowned_file);

	g_printf ("YES\n");
	return TRUE;
}

static gboolean
check_unowned_group (void)
{
	gchar *unowned_group_local_uri;
	GFile *unowned;
	GFileInfo *info;
	GError *error = NULL;

	unowned_group_local_uri = g_build_filename (g_get_tmp_dir (),
	                                            DEFAULT_TEST_UNOWNED_TEXT_FILE,
	                                            NULL);
	unowned = g_file_new_for_path (unowned_group_local_uri);

	g_printf ("*** Checking for unowned group test... ");

	info = g_file_query_info (unowned,
	                          G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE ","
	                          G_FILE_ATTRIBUTE_OWNER_GROUP ","
	                          G_FILE_ATTRIBUTE_UNIX_MODE,
	                          G_FILE_QUERY_INFO_NONE,
	                          NULL,
	                          &error);
	g_free (unowned_group_local_uri);

	if (error)
	{
		g_object_unref (unowned);
		g_printf ("NO: file does not exist\n");

		g_error_free (error);
		return FALSE;
	}

	if (!g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
	{
		g_object_unref (unowned);

		g_printf ("NO: file is not writable\n");
		g_object_unref (info);
		return FALSE;
	}

	if (g_strcmp0 (g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_OWNER_GROUP),
	               "root") != 0)
	{
		g_object_unref (unowned);

		g_printf ("NO: group is not root (%s)\n", g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_OWNER_GROUP));
		g_object_unref (info);
		return FALSE;
	}

#ifndef G_OS_WIN32
	if ((g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE) & ACCESSPERMS) != 0660)
	{
		g_object_unref (unowned);

		g_printf ("NO: file has wrong permissions\n");
		g_object_unref (info);
		return FALSE;
	}
#endif

	g_object_unref (info);
	g_object_unref (unowned);

	g_printf ("YES\n");
	return TRUE;
}

static void
all_tests (void)
{
	gboolean have_unowned;
#ifndef G_OS_WIN32
	gboolean have_unowned_group;
#endif

	g_printf ("\n***\n");
	have_unowned = check_unowned_directory ();
#ifndef G_OS_WIN32
	have_unowned_group = check_unowned_group ();
#endif
	g_printf ("***\n\n");

	g_test_trap_subprocess ("/file-saver/subprocess/local",
				0,
				G_TEST_SUBPROCESS_INHERIT_STDERR);
	g_test_trap_assert_passed ();

	g_test_trap_subprocess ("/file-saver/subprocess/local-new-line",
				0,
				G_TEST_SUBPROCESS_INHERIT_STDERR);
	g_test_trap_assert_passed ();

	if (have_unowned)
	{
		g_test_trap_subprocess ("/file-saver/subprocess/local-unowned-directory",
					0,
					G_TEST_SUBPROCESS_INHERIT_STDERR);
		g_test_trap_assert_passed ();
	}

	if (ENABLE_REMOTE_TESTS)
	{
		g_test_trap_subprocess ("/file-saver/subprocess/remote",
					0,
					G_TEST_SUBPROCESS_INHERIT_STDERR);
		g_test_trap_assert_passed ();

		g_test_trap_subprocess ("/file-saver/subprocess/remote-new-line",
					0,
					G_TEST_SUBPROCESS_INHERIT_STDERR);
		g_test_trap_assert_passed ();

		if (have_unowned)
		{
			g_test_trap_subprocess ("/file-saver/subprocess/remote-unowned-directory",
						0,
						G_TEST_SUBPROCESS_INHERIT_STDERR);
			g_test_trap_assert_passed ();
		}

		/*
		if (have_unowned_group)
		{
			g_test_trap_subprocess ("/file-saver/subprocess/remote-unowned-group",
						0,
						G_TEST_SUBPROCESS_INHERIT_STDERR);
			g_test_trap_assert_passed ();
		}
		*/
	}

#ifndef G_OS_WIN32
	g_test_trap_subprocess ("/file-saver/subprocess/local-permissions",
				0,
				G_TEST_SUBPROCESS_INHERIT_STDERR);
	g_test_trap_assert_passed ();

	if (have_unowned_group)
	{
		g_test_trap_subprocess ("/file-saver/subprocess/local-unowned-group",
					0,
					G_TEST_SUBPROCESS_INHERIT_STDERR);
		g_test_trap_assert_passed ();
	}

	if (ENABLE_REMOTE_TESTS)
	{
		g_test_trap_subprocess ("/file-saver/subprocess/remote-permissions",
					0,
					G_TEST_SUBPROCESS_INHERIT_STDERR);
		g_test_trap_assert_passed ();
	}
#endif
}

gint
main (gint   argc,
      gchar *argv[])
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/file-saver", all_tests);

	g_test_add_func ("/file-saver/subprocess/local", test_local);
	g_test_add_func ("/file-saver/subprocess/local-new-line", test_local_newline);
	g_test_add_func ("/file-saver/subprocess/local-unowned-directory", test_local_unowned_directory);

	if (ENABLE_REMOTE_TESTS)
	{
		g_test_add_func ("/file-saver/subprocess/remote", test_remote);
		g_test_add_func ("/file-saver/subprocess/remote-new-line", test_remote_newline);
		g_test_add_func ("/file-saver/subprocess/remote-unowned-directory", test_remote_unowned_directory);

		/* FIXME: there is a bug in gvfs sftp which doesn't pass this test */
		/* g_test_add_func ("/file-saver/subprocess/remote-unowned-group", test_remote_unowned_group); */
	}

#ifndef G_OS_WIN32
	g_test_add_func ("/file-saver/subprocess/local-permissions", test_local_permissions);
	g_test_add_func ("/file-saver/subprocess/local-unowned-group", test_local_unowned_group);

	if (ENABLE_REMOTE_TESTS)
	{
		g_test_add_func ("/file-saver/subprocess/remote-permissions", test_remote_permissions);
	}
#endif

	return g_test_run ();
}
