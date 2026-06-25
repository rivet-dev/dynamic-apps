#include <gtksourceview/gtksource.h>

static GHashTable *skipped;

static void
test_func (gconstpointer data)
{
	GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default ();
	const char *language_id = data;
	GtkSourceLanguage *l;
	GtkSourceBuffer *buffer;

	g_assert_true (GTK_SOURCE_IS_LANGUAGE_MANAGER (lm));
	g_assert_nonnull (data);

	if (g_hash_table_contains (skipped, language_id))
	{
		char *message = g_strdup_printf ("skipping known failure for %s", language_id);
		g_test_skip (message);
		g_free (message);
		return;
	}

	l = gtk_source_language_manager_get_language (lm, language_id);
	g_assert_nonnull (l);

	buffer = g_object_new (GTK_SOURCE_TYPE_BUFFER,
			       "language", l,
			       "highlight-syntax", TRUE,
			       NULL);

	g_object_unref (buffer);
}

int
main (int   argc,
      char *argv[])
{
	GtkSourceLanguageManager *lm;
	const char * const *ids;
	char *language_specs;
	char *search_path[2];
	const char *srcdir = g_getenv ("G_TEST_SRCDIR");
	GPtrArray *strings;
	int ret;

	g_assert_nonnull (srcdir);

	language_specs = g_build_filename (srcdir, "..", "data", "language-specs", NULL);
	search_path[0] = language_specs;
	search_path[1] = 0;

	g_test_init (&argc, &argv, NULL);
	gtk_source_init ();

	skipped = g_hash_table_new (g_str_hash, g_str_equal);

	g_hash_table_add (skipped, (char *)"gdb-log");
	g_hash_table_add (skipped, (char *)"jsdoc");
	g_hash_table_add (skipped, (char *)"js-expr");
	g_hash_table_add (skipped, (char *)"js-fn");
	g_hash_table_add (skipped, (char *)"js-lit");
	g_hash_table_add (skipped, (char *)"js-mod");
	g_hash_table_add (skipped, (char *)"js-st");
	g_hash_table_add (skipped, (char *)"js-val");
	g_hash_table_add (skipped, (char *)"typescript-js-expr");
	g_hash_table_add (skipped, (char *)"typescript-js-fn");
	g_hash_table_add (skipped, (char *)"typescript-js-lit");
	g_hash_table_add (skipped, (char *)"typescript-js-mod");
	g_hash_table_add (skipped, (char *)"typescript-js-st");
	g_hash_table_add (skipped, (char *)"typescript-type-expr");
	g_hash_table_add (skipped, (char *)"typescript-type-gen");
	g_hash_table_add (skipped, (char *)"typescript-type-lit");
	g_hash_table_add (skipped, (char *)"gtk-doc");
	g_hash_table_add (skipped, (char *)"testv1");

	strings = g_ptr_array_new_with_free_func (g_free);
	lm = gtk_source_language_manager_get_default ();
	gtk_source_language_manager_set_search_path (lm, search_path);

	ids = gtk_source_language_manager_get_language_ids (lm);

	for (guint i = 0; ids[i]; i++)
	{
		char *path = g_strdup_printf ("/Language/%s/load", ids[i]);
		g_ptr_array_add (strings, path);
		g_test_add_data_func (path, ids[i], test_func);
	}

	ret = g_test_run ();

	g_ptr_array_unref (strings);
	g_hash_table_unref (skipped);
	g_free (language_specs);

	return ret;
}
