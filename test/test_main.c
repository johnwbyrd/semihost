/*
 * ZBC Semihosting Test Runner
 *
 * Main entry point for all tests.
 */

#include "test_harness.h"
#include <stdio.h>
#include <string.h>

/* Test suite runners */
extern void run_client_builder_tests(void);
extern void run_roundtrip_tests(void);

int main(int argc, char **argv) {
  const char *filter = NULL;

  /* Optional filter argument */
  if (argc > 1) {
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
      printf("Usage: %s [filter]\n", argv[0]);
      printf("\nFilters:\n");
      printf("  builder    - Run client builder tests only\n");
      printf("  roundtrip  - Run round-trip integration tests only\n");
      printf("  (none)     - Run all tests\n");
      return 0;
    }
    filter = argv[1];
  }

  printf("ZBC Semihosting Library Tests\n");
  printf("=============================\n");

  /* Run test suites */
  if (!filter || strcmp(filter, "builder") == 0) {
    run_client_builder_tests();
  }

  if (!filter || strcmp(filter, "roundtrip") == 0) {
    run_roundtrip_tests();
  }

  /* Print summary */
  print_test_summary();

  return (g_tests_failed > 0) ? 1 : 0;
}
