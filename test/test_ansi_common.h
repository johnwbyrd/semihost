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

static int make_temp_path(char *buf, size_t buf_size, const char *filename)
{
    size_t dir_len = strlen(g_temp_dir);
    size_t file_len = strlen(filename);
    size_t total = dir_len + 1 + file_len + 1;  /* dir + sep + filename + nul */

    if (total > buf_size) {
        buf[0] = '\0';
        return -1;
    }

#ifdef _WIN32
    sprintf(buf, "%s\\%s", g_temp_dir, filename);
#else
    sprintf(buf, "%s/%s", g_temp_dir, filename);
#endif
    return 0;
}

static int make_indexed_temp_path(char *buf, size_t buf_size,
                                  const char *prefix, int index)
{
    char filename[MAX_FILENAME_LEN];
    sprintf(filename, "%s_%04d.tmp", prefix, index);
    return make_temp_path(buf, buf_size, filename);
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

/*------------------------------------------------------------------------
 * Cleanup helper
 *------------------------------------------------------------------------*/

static void cleanup_temp_files(const zbc_backend_t *be, void *ctx,
                               int *fds, int fds_count,
                               const char *prefix, int close_first)
{
    int i;
    char path[512];

    for (i = 0; i < fds_count; i++) {
        if (close_first && fds[i] >= 0) {
            be->close(ctx, fds[i]);
            fds[i] = -1;
        }
        make_indexed_temp_path(path, sizeof(path), prefix, i);
        be->remove(ctx, path, strlen(path));
    }
}

#endif /* TEST_ANSI_COMMON_H */
