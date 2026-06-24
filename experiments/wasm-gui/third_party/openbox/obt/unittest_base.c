#include <glib.h>

#include "obt/unittest_base.h"

guint g_test_failures = 0;
guint g_test_failures_at_test_start = 0;
const gchar* g_active_test_suite = NULL;
const gchar* g_active_test_name = NULL;

/* Add all test suites here. Keep them sorted. */
extern void run_bsearch_unittest();

gint main(gint argc, gchar **argv)
{
    /* Add all test suites here. Keep them sorted. */
    run_bsearch_unittest();

    return g_test_failures == 0 ? 0 : 1;
}

void unittest_start_suite(const char* suite_name)
{
    g_assert(g_active_test_suite == NULL);
    g_active_test_suite = suite_name;
    printf("[--------] %s\n", suite_name);
}

void unittest_end_suite()
{
    g_assert(g_active_test_suite);
    printf("[--------] %s\n", g_active_test_suite);
    printf("\n");
    g_active_test_suite = NULL;
}

void unittest_start(const char* test_name) 
{
    g_test_failures_at_test_start = g_test_failures;
    g_assert(g_active_test_name == NULL);
    g_active_test_name = test_name;
    printf("[ RUN    ] %s.%s\n", g_active_test_suite, g_active_test_name);
}

void unittest_end()
{
    g_assert(g_active_test_name);
    if (g_test_failures_at_test_start == g_test_failures) {
        printf("[     OK ] %s.%s\n",
               g_active_test_suite, g_active_test_name);
    } else {
        printf("[ FAILED ] %s.%s\n",
               g_active_test_suite, g_active_test_name);
    }
    g_active_test_name = NULL;
}
