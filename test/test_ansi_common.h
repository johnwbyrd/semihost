/*
 * ANSI Backend Tests - Common Utilities
 *
 * Shared test helpers, temp directory utilities, and test macros.
 */

#ifndef TEST_ANSI_COMMON_H
#define TEST_ANSI_COMMON_H

#ifndef ZBC_HOST
#define ZBC_HOST
#endif
#include "zbc_semihost.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*------------------------------------------------------------------------
 * Test tracking (local to each test file)
 *------------------------------------------------------------------------*/

static int tests_run = 0;
static int tests_passed = 0;

/*------------------------------------------------------------------------
 * Portable temp directory handling
 *------------------------------------------------------------------------*/

#define MAX_TEMP_DIR_LEN 256
#define MAX_FILENAME_LEN 128

static char g_temp_dir[MAX_TEMP_DIR_LEN];

static void init_temp_dir(void)
{
    const char *tmp;
    size_t len;

    /* Try standard environment variables */
    tmp = getenv("TMPDIR");
    if (!tmp) tmp = getenv("TMP");
    if (!tmp) tmp = getenv("TEMP");

#ifdef _WIN32
    if (!tmp) tmp = "C:\\Windows\\Temp";
#else
    if (!tmp) tmp = "/tmp";
#endif

    len = strlen(tmp);
    if (len >= MAX_TEMP_DIR_LEN) {
        len = MAX_TEMP_DIR_LEN - 1;
    }
    memcpy(g_temp_dir, tmp, len);
    g_temp_dir[len] = '\0';
}

/*------------------------------------------------------------------------
 * Test macros
 *------------------------------------------------------------------------*/

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        return 0; \
    } \
} while(0)

#define RUN_TEST(name) do { \
    int result; \
    printf("  %s... ", #name); \
    fflush(stdout); \
    tests_run++; \
    result = test_##name(); \
    if (result) { \
        tests_passed++; \
        printf("PASS\n"); \
    } \
} while(0)

/*------------------------------------------------------------------------
 * Stress test constants
 *------------------------------------------------------------------------*/

#define STRESS_FILE_COUNT_SMALL  64   /* Maximum for fixed-size array */
#define ANSI_FIRST_FD 3               /* First available FD after stdio */

#endif /* TEST_ANSI_COMMON_H */
