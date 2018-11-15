#ifndef LOKI_TEST_CASES_H
#define LOKI_TEST_CASES_H

#include "loki_integration_tests.h"

struct test_result
{
  loki_buffer<512>  name;
  bool              failed;
  loki_scratch_buf  fail_msg;
};

void        print_test_results(test_result const *results);

test_result prepare_registration__solo_auto_stake                    ();
test_result prepare_registration__100_percent_operator_cut_auto_stake();
test_result stake__from_subaddress();
test_result register_service_node__4_stakers();
test_result transfer__expect_fee_amount();

#endif // LOKI_TEST_CASES_H
