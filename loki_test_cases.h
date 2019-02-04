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

test_result latest__deregistration__1_unresponsive_node();
test_result latest__prepare_registration__check_solo_stake();
test_result latest__prepare_registration__check_100_percent_operator_cut_stake();
test_result latest__register_service_node__allow_4_stakers();
test_result latest__register_service_node__disallow_register_twice();
test_result latest__stake__disallow_to_non_registered_node();
test_result latest__stake__disallow_request_stake_unlock_twice();
test_result latest__transfer__check_fee_amount_bulletproofs();

test_result v10__register_service_node__check_gets_payed_expires_and_returns_funds();
test_result v10__register_service_node__check_grace_period();
test_result v10__stake__allow_incremental_staking_until_node_active();
test_result v10__stake__allow_insufficient_stake_w_reserved_contributor();
test_result v10__stake__disallow_insufficient_stake_w_not_reserved_contributor();

test_result v09__transfer__check_fee_amount();

#endif // LOKI_TEST_CASES_H
