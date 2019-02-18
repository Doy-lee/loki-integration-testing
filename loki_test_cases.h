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

//
// Latest
//
test_result latest__deregistration__1_unresponsive_node();

test_result latest__prepare_registration__check_solo_stake();
test_result latest__prepare_registration__check_100_percent_operator_cut_stake();

// TODO(doyle): We don't have any tests for blacklisted key images, because
// I haven't got node deregistration working reliably in integration tests
test_result latest__print_locked_stakes__check_no_locked_stakes();
test_result latest__print_locked_stakes__check_shows_locked_stakes();

test_result latest__register_service_node__allow_4_stakers();
test_result latest__register_service_node__disallow_register_twice();

test_result latest__request_stake_unlock__check_pooled_stake_unlocked();
test_result latest__request_stake_unlock__check_unlock_height();
test_result latest__request_stake_unlock__disallow_request_on_non_existent_node();
test_result latest__request_stake_unlock__disallow_request_twice();

test_result latest__stake__allow_incremental_stakes_with_1_contributor();
test_result latest__stake__check_incremental_stakes_decreasing_min_contribution();
test_result latest__stake__check_transfer_doesnt_used_locked_key_images();
test_result latest__stake__disallow_staking_less_than_minimum_in_pooled_node();
test_result latest__stake__disallow_staking_when_all_amounts_reserved();
test_result latest__stake__disallow_to_non_registered_node();

test_result latest__service_node_checkpointing();

test_result latest__transfer__check_fee_amount_bulletproofs();

//
// V10
//

test_result v10__register_service_node__check_gets_payed_expires_and_returns_funds();
test_result v10__register_service_node__check_grace_period();

test_result v10__stake__allow_incremental_staking_until_node_active();
test_result v10__stake__allow_insufficient_stake_w_reserved_contributor();
test_result v10__stake__disallow_insufficient_stake_w_not_reserved_contributor();

//
// V09
//
test_result v09__transfer__check_fee_amount();

#endif // LOKI_TEST_CASES_H
