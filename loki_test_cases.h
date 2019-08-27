#ifndef LOKI_TEST_CASES_H
#define LOKI_TEST_CASES_H

#include <chrono>
#include <vector>

#include "loki_integration_tests.h"

struct test_result
{
  loki_buffer<512>  name;
  bool              failed;
  loki_scratch_buf  fail_msg;
  float             duration_ms;
};

#define INITIALISE_TEST_CONTEXT(test_result_var)                               \
  test_result_var.name = loki_buffer<512>(__func__);                           \
  auto start_time = std::chrono::high_resolution_clock::now();                 \
  LOKI_DEFER {                                                                 \
    auto end_time = std::chrono::high_resolution_clock::now();                 \
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time) .count(); \
    test_result_var.duration_ms = duration / 1000.f; \
  }

// TODO(doyle): Switch most setup to using this helper, since it covers almost all use cases
struct helper_blockchain_environment
{
  start_daemon_params         daemon_param;
  std::vector<daemon_t>       all_daemons;
  daemon_t                   *service_nodes;
  int                         num_service_nodes;
  daemon_t                   *daemons;
  int                         num_daemons;
  std::vector<loki_snode_key> snode_keys;
  std::vector<wallet_t>       wallets;
  std::vector<loki_addr>      wallets_addr;
};
void helper_cleanup_blockchain_environment(helper_blockchain_environment *environment);
bool helper_setup_blockchain(helper_blockchain_environment *environment,
                             test_result const *context,
                             start_daemon_params daemon_param,
                             int num_service_nodes,
                             int num_daemons,
                             int num_wallets,
                             int wallet_balance);

// NOTE: The minimum amount such that spending funds is reliable and doesn't
// error out with insufficient outputs to select from.
// NOTE: This used to be much lower, like 100, but after the output selection
// algorithm changed, the fake outputs lineup commit this seems to be a lot
// stricter now
const int MIN_BLOCKS_IN_BLOCKCHAIN = 100;

void        print_test_results(test_result const *results);
//
// Latest
//
test_result foo();

test_result latest__checkpointing__private_chain_reorgs_to_checkpoint_chain();
test_result latest__checkpointing__new_peer_syncs_checkpoints();
test_result latest__checkpointing__deregister_non_participating_peer();

test_result latest__decommission__recommission_on_uptime_proof();

test_result latest__deregistration__n_unresponsive_node();

test_result latest__prepare_registration__check_solo_stake();
test_result latest__prepare_registration__check_all_solo_stake_forms_valid_registration();
test_result latest__prepare_registration__check_100_percent_operator_cut_stake();

// TODO(doyle): We don't have any tests for blacklisted key images, because
// I haven't got node deregistration working reliably in integration tests
test_result latest__print_locked_stakes__check_no_locked_stakes();
test_result latest__print_locked_stakes__check_shows_locked_stakes();

test_result latest__register_service_node__allow_4_stakers();
test_result latest__register_service_node__allow_70_20_and_10_open_for_contribution();
test_result latest__register_service_node__allow_43_23_13_21_reserved_contribution();
test_result latest__register_service_node__allow_87_13_reserved_contribution();
test_result latest__register_service_node__allow_87_13_contribution();
test_result latest__register_service_node__disallow_register_twice();
test_result latest__register_service_node__check_unlock_time_is_0();

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
test_result latest__transfer__check_fee_amount_80x_increase();

//
// V11
//
test_result v11__transfer__check_fee_amount_bulletproofs();

//
// V10
//
test_result v10__prepare_registration__check_all_solo_stake_forms_valid_registration();
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
