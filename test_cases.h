#ifndef LOKI_TEST_CASES_H
#define LOKI_TEST_CASES_H

#include "integration_tests.h"

struct test_result
{
  loki_buffer<512>  name;
  bool              failed;

  loki_scratch_buf  captured_stdout;
  loki_buffer<1024> received_str;
  loki_buffer<1024> expected_str;

  void print_result();
};

test_result prepare_registration__solo_auto_stake                    ();
test_result prepare_registration__100_percent_operator_cut_auto_stake();
test_result stake__from_subaddress();

#endif // LOKI_TEST_CASES_H
