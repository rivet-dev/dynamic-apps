#ifndef __obt_unittest_base_h
#define __obt_unittest_base_h

#include <glib.h>
#include <stdio.h>

G_BEGIN_DECLS

extern guint g_test_failures;
extern guint g_test_failures_at_test_start;
extern const gchar* g_active_test_suite;
extern const gchar* g_active_test_name;

#define ADD_FAILURE() { ++g_test_failures; }

#define FAILURE_AT() \
    fprintf(stderr, "Failure at %s:%u\n", __FILE__, __LINE__); \
    ADD_FAILURE();

#define EXPECT_BOOL_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        FAILURE_AT(); \
        fprintf(stderr, "Expected: %s\nActual: %s\n", \
               ((expected) ? "true" : "false"), \
               ((actual) ? "true" : "false"));  \
    }

#define EXPECT_CHAR_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        FAILURE_AT(); \
        fprintf(stderr, "Expected: %c\nActual: %c\n", (expected), (actual)); \
    }

#define EXPECT_INT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        FAILURE_AT(); \
        fprintf(stderr, "Expected: %d\nActual: %d\n", (expected), (actual)); \
    }

#define EXPECT_UINT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        FAILURE_AT(); \
        fprintf(stderr, "Expected: %u\nActual: %u\n", (expected), (actual)); \
    }

#define EXPECT_STRING_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        FAILURE_AT(); \
        fprintf(stderr, "Expected: %s\nActual: %s\n", \
               ((expected) ? (expected) : "NULL"), \
               ((actual) ? (actual) : NULL)); \
    }

void unittest_start_suite(const char* suite_name);
void unittest_end_suite();

void unittest_start(const char* test_name);
void unittest_end();

#define TEST_START() unittest_start(__func__);
#define TEST_END() unittest_end();

G_END_DECLS

#endif
