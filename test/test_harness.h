/*
 * ZBC Semihosting Test Harness
 *
 * Simple test framework with no external dependencies.
 * Portable across Windows, macOS, Linux, and embedded platforms.
 */

#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include "zbc_semi_common.h"
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*------------------------------------------------------------------------
 * Global test state
 *------------------------------------------------------------------------*/

extern int g_test_passed;
extern int g_tests_passed;
extern int g_tests_failed;

/*------------------------------------------------------------------------
 * Test assertion macros
 *------------------------------------------------------------------------*/

void test_fail(const char *file, int line, const char *expr);
void test_fail_eq(const char *file, int line, const char *a_expr, long a_val,
                  const char *b_expr, long b_val);
void test_fail_mem(const char *file, int line, const char *a_expr,
                   const char *b_expr, size_t n);

#define TEST_ASSERT(cond)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      test_fail(__FILE__, __LINE__, #cond);                                    \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define TEST_ASSERT_EQ(a, b)                                                   \
  do {                                                                         \
    long _a = (long)(a);                                                       \
    long _b = (long)(b);                                                       \
    if (_a != _b) {                                                            \
      test_fail_eq(__FILE__, __LINE__, #a, _a, #b, _b);                        \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define TEST_ASSERT_NEQ(a, b)                                                  \
  do {                                                                         \
    long _a = (long)(a);                                                       \
    long _b = (long)(b);                                                       \
    if (_a == _b) {                                                            \
      test_fail_eq(__FILE__, __LINE__, #a " != " #b, _a, "equal", _b);         \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define TEST_ASSERT_MEM_EQ(a, b, n)                                            \
  do {                                                                         \
    if (memcmp((a), (b), (n)) != 0) {                                          \
      test_fail_mem(__FILE__, __LINE__, #a, #b, (n));                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

/*------------------------------------------------------------------------
 * Test runner macros
 *------------------------------------------------------------------------*/

#define RUN_TEST(name)                                                         \
  do {                                                                         \
    g_test_passed = 1;                                                         \
    printf("  %-50s ", #name);                                                 \
    fflush(stdout);                                                            \
    test_##name();                                                             \
    if (g_test_passed) {                                                       \
      printf("[PASS]\n");                                                      \
      g_tests_passed++;                                                        \
    } else {                                                                   \
      printf("[FAIL]\n");                                                      \
      g_tests_failed++;                                                        \
    }                                                                          \
  } while (0)

#define BEGIN_SUITE(name) printf("\nSuite: %s\n", name)

#define END_SUITE()                                                            \
  do {                                                                         \
  } while (0)

/*------------------------------------------------------------------------
 * Guarded buffer macros (stack-allocated)
 *
 * Places canary bytes before and after the usable buffer region.
 * Detects buffer overflows and underflows.
 *------------------------------------------------------------------------*/

#define CANARY_SIZE 16
#define CANARY_BYTE 0xDE

/*
 * Declare a guarded buffer on the stack.
 * Creates: name_storage (full array), name (pointer to usable area), name_size
 */
#define GUARDED_BUF(name, sz)                                                  \
  uint8_t name##_storage[CANARY_SIZE + (sz) + CANARY_SIZE];                    \
  uint8_t *name = name##_storage + CANARY_SIZE;                                \
  const size_t name##_size = (sz)

/*
 * Initialize canary bytes. Call after GUARDED_BUF.
 */
#define GUARDED_INIT(name)                                                     \
  do {                                                                         \
    size_t _gi;                                                                \
    for (_gi = 0; _gi < CANARY_SIZE; _gi++) {                                  \
      name##_storage[_gi] = CANARY_BYTE;                                       \
      name##_storage[CANARY_SIZE + name##_size + _gi] = CANARY_BYTE;           \
    }                                                                          \
  } while (0)

/*
 * Check canary bytes. Returns 0 if intact, non-zero if stomped.
 */
#define GUARDED_CHECK(name) guarded_check_canaries(name##_storage, name##_size)

/* Returns 0 if canaries intact, 1 if pre-canary stomped, 2 if post-canary
 * stomped */
int guarded_check_canaries(const uint8_t *storage, size_t data_size);

/*------------------------------------------------------------------------
 * Test summary
 *------------------------------------------------------------------------*/

void print_test_summary(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_HARNESS_H */
