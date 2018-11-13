#ifndef LOKI_TEST_CASES_H
#define LOKI_TEST_CASES_H

#include "loki_integration_tests.h"

struct test_result
{
  loki_buffer<512>  name;
  bool              failed;

  loki_scratch_buf  captured_stdout;
  loki_buffer<1024> received_str;
  loki_buffer<1024> expected_str;

  void print_result();
};

#define STR_EXPECT(test_result_var, src, expect_str) \
if (!str_match(src, expect_str)) \
{ \
  test_result_var.failed = true; \
  test_result_var.received_str = src; \
  test_result_var.expected_str = expect_str; \
  return test_result_var; \
}

#define EXPECT(test_result_var, expr, fmt, ...) \
if (!expr) \
{ \
  test_result_var.failed = true; \
  test_result_var.received_str = #expr; \
  test_result_var.expected_str = loki_buffer<1024>(fmt, ## __VA_ARGS__); \
  return test_result_var; \
}

test_result prepare_registration__solo_auto_stake                    ();
test_result prepare_registration__100_percent_operator_cut_auto_stake();
test_result stake__from_subaddress();

#endif // LOKI_TEST_CASES_H
