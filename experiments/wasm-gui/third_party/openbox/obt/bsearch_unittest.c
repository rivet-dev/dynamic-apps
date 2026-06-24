#include "obt/unittest_base.h"

#include "obt/bsearch.h"

#include <glib.h>

static void empty() {
    TEST_START();

    BSEARCH_SETUP();
    int* array = NULL;
    guint array_size = 0;

    /* Search in an empty array. */
    BSEARCH(int, array, 0, array_size, 10);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(0, BSEARCH_AT());

    /* Search in an empty array with a non-zero starting position. */
    BSEARCH(int, array, 10, array_size, -10);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(10, BSEARCH_AT());

    TEST_END();
}

static void single_element() {
    TEST_START();

    BSEARCH_SETUP();
    int array[1];
    guint array_size = 1;

    /* Search for something smaller than the only element. */
    array[0] = 20;
    BSEARCH(int, array, 0, array_size, -10);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(0, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    /* Search for something bigger than the only element. */
    array[0] = 20;
    BSEARCH(int, array, 0, array_size, 30);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(0, BSEARCH_AT());
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND_NEAREST_SMALLER());

    /* Search for something smaller than the only element. */
    array[0] = -20;
    BSEARCH(int, array, 0, array_size, -30);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(0, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    /* Search for something bigger than the only element. */
    array[0] = -20;
    BSEARCH(int, array, 0, array_size, 10);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(0, BSEARCH_AT());
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND_NEAREST_SMALLER());

    /* Search for the only element that exists. */
    array[0] = -20;
    BSEARCH(int, array, 0, array_size, -20);
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(0, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    TEST_END();
}

static void single_element_nonzero_start() {
    TEST_START();

    BSEARCH_SETUP();
    int array[10];
    guint array_start = 9;
    guint array_size = 1;

    /* Search for something smaller than the only element. */
    array[array_start] = 20;
    BSEARCH(int, array, array_start, array_size, -10);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(array_start, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    /* Search for something bigger than the only element. */
    array[array_start] = 20;
    BSEARCH(int, array, array_start, array_size, 30);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(array_start, BSEARCH_AT());
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND_NEAREST_SMALLER());

    /* Search for something smaller than the only element. */
    array[array_start] = -20;
    BSEARCH(int, array, array_start, array_size, -30);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(array_start, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    /* Search for something bigger than the only element. */
    array[array_start] = -20;
    BSEARCH(int, array, array_start, array_size, 10);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(array_start, BSEARCH_AT());
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND_NEAREST_SMALLER());

    /* Search for the only element that exists. */
    array[array_start] = -20;
    BSEARCH(int, array, array_start, array_size, -20);
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(array_start, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    TEST_END();
}

static void present() {
    TEST_START();

    BSEARCH_SETUP();
    int array[5];
    guint array_start = 0;
    guint array_size = 5;

    array[0] = 10;
    array[1] = 12;
    array[2] = 14;
    array[3] = 16;
    array[4] = 18;

    /* Search for something that is in the array. */

    BSEARCH(int, array, array_start, array_size, 10);
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(0, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    BSEARCH(int, array, array_start, array_size, 12);
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(1, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    BSEARCH(int, array, array_start, array_size, 14);
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(2, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    BSEARCH(int, array, array_start, array_size, 16);
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(3, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    BSEARCH(int, array, array_start, array_size, 18);
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(4, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    TEST_END();
}

static void present_nonzero_start() {
    TEST_START();

    BSEARCH_SETUP();
    int array[5];
    guint array_start = 2;
    guint array_size = 3;

    array[0] = 10;
    array[1] = 12;
    array[2] = 14;
    array[3] = 16;
    array[4] = 18;

    /* Search for something that is in the array. */

    BSEARCH(int, array, array_start, array_size, 10);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(array_start, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    BSEARCH(int, array, array_start, array_size, 12);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(array_start, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    BSEARCH(int, array, array_start, array_size, 14);
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(2, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    BSEARCH(int, array, array_start, array_size, 16);
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(3, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    BSEARCH(int, array, array_start, array_size, 18);
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(4, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    TEST_END();
}

static void missing() {
    TEST_START();

    BSEARCH_SETUP();
    int array[5];
    guint array_start = 0;
    guint array_size = 5;

    array[0] = 10;
    array[1] = 12;
    array[2] = 14;
    array[3] = 16;
    array[4] = 18;

    /* Search for something that is _not_ in the array. */

    BSEARCH(int, array, array_start, array_size, 9);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(0, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    BSEARCH(int, array, array_start, array_size, 11);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(0, BSEARCH_AT());
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND_NEAREST_SMALLER());

    BSEARCH(int, array, array_start, array_size, 13);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(1, BSEARCH_AT());
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND_NEAREST_SMALLER());

    BSEARCH(int, array, array_start, array_size, 15);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(2, BSEARCH_AT());
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND_NEAREST_SMALLER());

    BSEARCH(int, array, array_start, array_size, 17);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(3, BSEARCH_AT());
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND_NEAREST_SMALLER());

    BSEARCH(int, array, array_start, array_size, 19);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(4, BSEARCH_AT());
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND_NEAREST_SMALLER());

    TEST_END();
}

static void missing_nonzero_start() {
    TEST_START();

    BSEARCH_SETUP();
    int array[5];
    guint array_start = 2;
    guint array_size = 3;

    array[0] = 10;
    array[1] = 12;
    array[2] = 14;
    array[3] = 16;
    array[4] = 18;

    /* Search for something that is _not_ in the array. */

    BSEARCH(int, array, array_start, array_size, 9);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(array_start, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    BSEARCH(int, array, array_start, array_size, 11);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(array_start, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    BSEARCH(int, array, array_start, array_size, 13);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(array_start, BSEARCH_AT());
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND_NEAREST_SMALLER());

    BSEARCH(int, array, array_start, array_size, 15);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(2, BSEARCH_AT());
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND_NEAREST_SMALLER());

    BSEARCH(int, array, array_start, array_size, 17);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(3, BSEARCH_AT());
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND_NEAREST_SMALLER());

    BSEARCH(int, array, array_start, array_size, 19);
    EXPECT_BOOL_EQ(FALSE, BSEARCH_FOUND());
    EXPECT_UINT_EQ(4, BSEARCH_AT());
    EXPECT_BOOL_EQ(TRUE, BSEARCH_FOUND_NEAREST_SMALLER());

    TEST_END();
}

void run_bsearch_unittest() {
    unittest_start_suite("bsearch");

    empty();
    single_element();
    single_element_nonzero_start();
    present();
    present_nonzero_start();
    missing();
    missing_nonzero_start();

    unittest_end_suite();
}
