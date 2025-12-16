/*
 * ZBC Target Test Harness
 *
 * Minimal test framework for bare-metal on-target testing.
 * No libc dependencies - uses only ZBC semihosting for output.
 *
 * Usage:
 *   TARGET_INIT()                  - Initialize test harness
 *   TARGET_PRINT("message")        - Print via SH_SYS_WRITE0
 *   TARGET_ASSERT(cond)            - Assert condition, record failure
 *   TARGET_ASSERT_EQ(a, b)         - Assert equality
 *   TARGET_SKIP("reason")          - Mark test as skipped
 *   TARGET_BEGIN_TEST("name")      - Start a named test
 *   TARGET_END_TEST()              - End current test
 *   TARGET_EXIT()                  - Exit with pass/fail status
 */

#ifndef ZBC_TARGET_HARNESS_H
#define ZBC_TARGET_HARNESS_H

#ifndef ZBC_CLIENT
#define ZBC_CLIENT
#endif
#include "zbc_semihost.h"

/*------------------------------------------------------------------------
 * Global test state (static allocation)
 *------------------------------------------------------------------------*/

static zbc_client_state_t g_target_client;
static uint8_t g_target_riff_buf[512];

static int g_target_tests_run;
static int g_target_tests_passed;
static int g_target_tests_failed;
static int g_target_tests_skipped;
static int g_target_current_test_failed;
static const char *g_target_current_test_name;

/*------------------------------------------------------------------------
 * Platform-specific semihost base address
 *
 * ZBC Memory Layout Formula:
 *   addr_bits = sizeof(void*) * 8
 *   reserved_start = (1 << addr_bits) - (1 << (addr_bits/2))
 *   semihost_addr = reserved_start - 512 - 32
 *------------------------------------------------------------------------*/

static inline volatile uint8_t *target_get_semihost_base(void)
{
    const unsigned int half_bits = sizeof(void*) * 4;
    const uintptr_t reserved_start = (uintptr_t)0 - ((uintptr_t)1 << half_bits);
    return (volatile uint8_t *)(reserved_start - 512 - 32);
}

/*------------------------------------------------------------------------
 * Helper: Print hex value (no libc printf)
 *------------------------------------------------------------------------*/

static void target_print_hex(uintptr_t val)
{
    static const char hex[] = "0123456789abcdef";
    char buf[20];
    int i = 0;
    int started = 0;
    int shift;

    buf[i++] = '0';
    buf[i++] = 'x';

    for (shift = (int)((sizeof(uintptr_t) * 8) - 4); shift >= 0; shift -= 4) {
        int digit = (int)((val >> shift) & 0xf);
        if (digit != 0 || started || shift == 0) {
            buf[i++] = hex[digit];
            started = 1;
        }
    }
    buf[i] = '\0';

    /* Print via WRITE0 */
    {
        uintptr_t args[1];
        args[0] = (uintptr_t)buf;
        zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                     SH_SYS_WRITE0, (uintptr_t)args);
    }
}

static void target_print_int(int val)
{
    char buf[16];
    int i = 0;
    int neg = 0;
    unsigned int uval;
    int j, k;
    char tmp;

    if (val < 0) {
        neg = 1;
        uval = (unsigned int)(-(val + 1)) + 1;
    } else {
        uval = (unsigned int)val;
    }

    /* Build string in reverse using subtraction instead of division */
    do {
        unsigned int digit = 0;
        while (uval >= 10) {
            uval -= 10;
            digit++;
        }
        buf[i++] = '0' + (char)uval;
        uval = digit;
    } while (uval > 0);

    if (neg) buf[i++] = '-';

    /* Reverse and null-terminate using subtraction for midpoint */
    k = i;
    for (j = 0; k > 1; j++, k -= 2) {
        tmp = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = tmp;
    }
    buf[i] = '\0';

    /* Print via WRITE0 */
    {
        uintptr_t args[1];
        args[0] = (uintptr_t)buf;
        zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf),
                     SH_SYS_WRITE0, (uintptr_t)args);
    }
}

/*------------------------------------------------------------------------
 * Core macros
 *------------------------------------------------------------------------*/

#define TARGET_INIT() \
    do { \
        zbc_client_init(&g_target_client, target_get_semihost_base()); \
        g_target_tests_run = 0; \
        g_target_tests_passed = 0; \
        g_target_tests_failed = 0; \
        g_target_tests_skipped = 0; \
        g_target_current_test_failed = 0; \
        g_target_current_test_name = (const char *)0; \
    } while (0)

#define TARGET_PRINT(msg) \
    do { \
        uintptr_t _args[1]; \
        _args[0] = (uintptr_t)(msg); \
        zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf), \
                     SH_SYS_WRITE0, (uintptr_t)_args); \
    } while (0)

#define TARGET_BEGIN_TEST(name) \
    do { \
        g_target_current_test_name = (name); \
        g_target_current_test_failed = 0; \
        TARGET_PRINT("  "); \
        TARGET_PRINT(name); \
        TARGET_PRINT("... "); \
    } while (0)

#define TARGET_END_TEST() \
    do { \
        g_target_tests_run++; \
        if (g_target_current_test_failed) { \
            g_target_tests_failed++; \
            TARGET_PRINT("[FAIL]\n"); \
        } else { \
            g_target_tests_passed++; \
            TARGET_PRINT("[PASS]\n"); \
        } \
        g_target_current_test_name = (const char *)0; \
    } while (0)

#define TARGET_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            g_target_current_test_failed = 1; \
            TARGET_PRINT("\n    ASSERT FAILED: " #cond " "); \
        } \
    } while (0)

#define TARGET_ASSERT_EQ(a, b) \
    do { \
        uintptr_t _a = (uintptr_t)(a); \
        uintptr_t _b = (uintptr_t)(b); \
        if (_a != _b) { \
            g_target_current_test_failed = 1; \
            TARGET_PRINT("\n    ASSERT_EQ FAILED: " #a " != " #b " ("); \
            target_print_hex(_a); \
            TARGET_PRINT(" != "); \
            target_print_hex(_b); \
            TARGET_PRINT(") "); \
        } \
    } while (0)

#define TARGET_ASSERT_NEQ(a, b) \
    do { \
        uintptr_t _a = (uintptr_t)(a); \
        uintptr_t _b = (uintptr_t)(b); \
        if (_a == _b) { \
            g_target_current_test_failed = 1; \
            TARGET_PRINT("\n    ASSERT_NEQ FAILED: " #a " == " #b " ("); \
            target_print_hex(_a); \
            TARGET_PRINT(") "); \
        } \
    } while (0)

#define TARGET_SKIP(reason) \
    do { \
        g_target_tests_skipped++; \
        g_target_tests_run++; \
        TARGET_PRINT("[SKIP: " reason "]\n"); \
        g_target_current_test_name = (const char *)0; \
    } while (0)

/*------------------------------------------------------------------------
 * Exit with test results
 *------------------------------------------------------------------------*/

#define TARGET_EXIT() \
    do { \
        uintptr_t _exit_args[2]; \
        TARGET_PRINT("\n========================================\n"); \
        TARGET_PRINT("Tests: "); \
        target_print_int(g_target_tests_run); \
        TARGET_PRINT("  Passed: "); \
        target_print_int(g_target_tests_passed); \
        TARGET_PRINT("  Failed: "); \
        target_print_int(g_target_tests_failed); \
        TARGET_PRINT("  Skipped: "); \
        target_print_int(g_target_tests_skipped); \
        TARGET_PRINT("\n"); \
        if (g_target_tests_failed > 0) { \
            TARGET_PRINT("RESULT: FAIL\n"); \
        } else { \
            TARGET_PRINT("RESULT: PASS\n"); \
        } \
        TARGET_PRINT("========================================\n"); \
        /* Exit with 0 on success, 1 on any failure */ \
        _exit_args[0] = (g_target_tests_failed > 0) ? 1 : 0; \
        _exit_args[1] = 0; \
        zbc_semihost(&g_target_client, g_target_riff_buf, sizeof(g_target_riff_buf), \
                     SH_SYS_EXIT, (uintptr_t)_exit_args); \
        /* Never reached */ \
        for (;;) {} \
    } while (0)

#endif /* ZBC_TARGET_HARNESS_H */
