/*
 * ZBC Semihosting Test Harness - Implementation
 */

#include "test_harness.h"

/*------------------------------------------------------------------------
 * Global test state
 *------------------------------------------------------------------------*/

int g_test_passed = 1;
int g_tests_passed = 0;
int g_tests_failed = 0;

/*------------------------------------------------------------------------
 * Failure reporting
 *------------------------------------------------------------------------*/

void test_fail(const char *file, int line, const char *expr) {
  g_test_passed = 0;
  printf("\n    FAILED at %s:%d\n", file, line);
  printf("    Assertion: %s\n", expr);
}

void test_fail_eq(const char *file, int line, const char *a_expr, int a_val,
                  const char *b_expr, int b_val) {
  g_test_passed = 0;
  printf("\n    FAILED at %s:%d\n", file, line);
  printf("    Expected: %s == %s\n", a_expr, b_expr);
  printf("    Got: %d vs %d\n", a_val, b_val);
}

void test_fail_mem(const char *file, int line, const char *a_expr,
                   const char *b_expr, size_t n) {
  g_test_passed = 0;
  printf("\n    FAILED at %s:%d\n", file, line);
  printf("    Memory mismatch: %s != %s (size %lu)\n", a_expr, b_expr,
         (unsigned long)n);
}

/*------------------------------------------------------------------------
 * Guarded buffer check
 *------------------------------------------------------------------------*/

int guarded_check_canaries(const uint8_t *storage, size_t data_size) {
  size_t i;

  /* Check pre-canary */
  for (i = 0; i < CANARY_SIZE; i++) {
    if (storage[i] != CANARY_BYTE) {
      return 1; /* Pre-canary stomped */
    }
  }

  /* Check post-canary */
  for (i = 0; i < CANARY_SIZE; i++) {
    if (storage[CANARY_SIZE + data_size + i] != CANARY_BYTE) {
      return 2; /* Post-canary stomped */
    }
  }

  return 0; /* All canaries intact */
}

/*------------------------------------------------------------------------
 * Test summary
 *------------------------------------------------------------------------*/

void print_test_summary(void) {
  printf("\n=============================\n");
  printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);

  if (g_tests_failed == 0) {
    printf("All tests passed!\n");
  }
}
