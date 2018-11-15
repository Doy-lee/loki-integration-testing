#include "loki_test_cases.h"

#define LOKI_WALLET_IMPLEMENTATION
#include "loki_wallet.h"

#define LOKI_STR_IMPLEMENTATION
#include "loki_str.h"

#define LOKI_DAEMON_IMPLEMENTATION
#include "loki_daemon.h"

#include <chrono>
#include <thread>

void test_result::print_result()
{
  if (failed)
  {
    printf("  %s: FAILED\n",       name.data);
    printf("      Expected: %s\n", expected_str.data);
    printf("      Received: %s\n", received_str.data);
    printf("      Context:  %s\n", captured_stdout.data);
  }
  else
  {
    printf("  %s: PASSED\n", name.data);
  }
}

struct loki_err_context
{
  bool              success;
  loki_buffer<1024> err_msg;
  operator bool() { return success; }
};

#define INITIALISE_TEST_CONTEXT(test_result_var) reset_shared_memory(); test_result_var.name = loki_buffer<512>(__func__)

test_result prepare_registration__solo_auto_stake()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon = create_daemon();
  start_daemon(&daemon);

  LOKI_DEFER { write_to_stdin_mem(shared_mem_type::daemon, "exit"); };
  char const wallet1[] = "T6U4ukY68vohsfrGMryFmqX5yRE4d5EC8E6QbinSo8ssW3heqoNjgNggTeym9NSLW4cnEp3ckpD9RZLW5qDGg3821c9SAtHMD";

  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "prepare_registration");
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y"); // Contribute entire stake?
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, wallet1); // Operator address
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y"); // Auto restake?
  result.captured_stdout = write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y"); // Confirm information correct?

  // Expected Format: register_service_node [auto] <operator cut> <address> <fraction> [<address> <fraction> [...]]]
  char const *register_str      = str_find(&result.captured_stdout, "register_service_node");
  char const *prev              = register_str;

  char const *auto_stake        = str_skip_to_next_word(&prev);
  char const *operator_portions = str_skip_to_next_word(&prev);
  char const *wallet_addr       = str_skip_to_next_word(&prev);
  char const *addr1_portions    = str_skip_to_next_word(&prev);

  STR_EXPECT(result, register_str,      "register_service_node");
  STR_EXPECT(result, auto_stake,        "auto");
  STR_EXPECT(result, operator_portions, "18446744073709551612");
  STR_EXPECT(result, wallet_addr,       wallet1);
  STR_EXPECT(result, addr1_portions,    "18446744073709551612");

  return result;
}

test_result prepare_registration__100_percent_operator_cut_auto_stake()
{

  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon = create_daemon();
  start_daemon(&daemon);
  LOKI_DEFER { write_to_stdin_mem(shared_mem_type::daemon, "exit"); };

  char const wallet1[] = "T6U4ukY68vohsfrGMryFmqX5yRE4d5EC8E6QbinSo8ssW3heqoNjgNggTeym9NSLW4cnEp3ckpD9RZLW5qDGg3821c9SAtHMD";
  char const wallet2[] = "T6TZgnpJ2uaC1cqS4E6M6u7QmGA79q2G19ToBHnqWHxMMDocNTiw2phg52XjkAmEZH9V5xQUsaR3cbcTnELE1vXP2YkhEqXad";

  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "prepare_registration");
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "n"); // Contribute entire stake?
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "100%"); // Operator cut
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "50"); // How much loki to reserve?
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y"); // Do you want to reserve portions of the stake for other contribs?
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "1"); // Number of additional contributors
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, wallet1); // Operator address
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "25"); // How much loki to reserve for contributor 1
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, wallet2); // Contrib 1 address
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y"); // How much loki to reserve for contributor 1
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y"); // Autostake
  result.captured_stdout = write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y"); // Confirm

  // Expected Format: register_service_node [auto] <operator cut> <address> <fraction> [<address> <fraction> [...]]]
  char const *register_str     = str_find(&result.captured_stdout, "register_service_node");
  char const *prev             = register_str;

  char const *auto_stake       = str_skip_to_next_word(&prev);
  char const *operator_cut     = str_skip_to_next_word(&prev);
  char const *wallet1_addr     = str_skip_to_next_word(&prev);
  char const *wallet1_portions = str_skip_to_next_word(&prev);
  char const *wallet2_addr     = str_skip_to_next_word(&prev);
  char const *wallet2_portions = str_skip_to_next_word(&prev);

  STR_EXPECT(result, register_str,     "register_service_node");
  STR_EXPECT(result, auto_stake,       "auto");
  STR_EXPECT(result, operator_cut,     "18446744073709551612");
  STR_EXPECT(result, wallet1_addr,     wallet1);
  STR_EXPECT(result, wallet1_portions, "9223372036854775806"); // exactly 50% of staking portions
  STR_EXPECT(result, wallet2_addr,     wallet2);
  STR_EXPECT(result, wallet2_portions, "4611686018427387903"); // exactly 25% of staking portions

  return result;
}

test_result stake__from_subaddress()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon = create_daemon();
  start_daemon(&daemon);
  wallet_t operator_wallet = create_wallet();

  start_wallet(&operator_wallet);
  LOKI_DEFER { wallet_exit(); };

  wallet_set_default_testing_settings();
  EXPECT(result, wallet_set_daemon(&daemon), "Could not set wallet with daemon at port %d", daemon.rpc_port);
  wallet_mine_atleast_n_blocks(100);

  loki_addr subaddr = wallet_address_new();
  {
    loki_addr check = {};
    wallet_address(1, &check);
    EXPECT(result, strcmp(subaddr.c_str, check.c_str) == 0, "Subaddresses did not match, expected: %s, received: %s", subaddr.c_str, check.c_str);
  }

  loki_addr main_addr = {};
  wallet_address(0, &main_addr);

  wallet_transfer(&subaddr, 30);
  wallet_address(1);
  for (uint64_t subaddr_unlocked_bal = 0; subaddr_unlocked_bal < 30;)
  {
    wallet_mine_atleast_n_blocks(1);
    wallet_get_balance(&subaddr_unlocked_bal);
  }

  loki_scratch_buf registration_cmd = {};
  {
    daemon_prepare_registration_params params = {};
    params.open_pool                          = true;
    params.num_contributors                   = 1;
    params.contributors[0].addr               = main_addr;
    params.contributors[0].amount             = 25;
    daemon_prepare_registration(&params, &registration_cmd);
  }

  wallet_register_service_node(registration_cmd.c_str);
  wallet_mine_atleast_n_blocks(1);

  loki_snode_key snode_key = {};
  EXPECT(result, daemon_print_sn_key(&snode_key), "Failed to print sn key");

  wallet_stake(&snode_key, &subaddr, 25);

  return result;
}

test_result register_service_node__4_stakers()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon            = create_daemon();
  start_daemon(&daemon);
  wallet_t  stakers[4]       = {};
  loki_addr stakers_addr[4]  = {};

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;

  // Mine and unlock funds
  for (int i = 0; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
  {
    stakers[i] = create_wallet();
    start_wallet(stakers + i, wallet_params);
    wallet_set_default_testing_settings();

    LOKI_DEFER { wallet_exit(); };

    wallet_address(0, stakers_addr + i);
    wallet_mine_atleast_n_blocks(2);
  }

  wallet_t *owner = stakers + 0;
  start_wallet(owner, wallet_params);
  wallet_mine_unlock_time_blocks();

  // Register the service node
  loki_snode_key snode_key = {};
  daemon_print_sn_key(&snode_key);

  daemon_prepare_registration_params registration_params = {};
  registration_params.num_contributors = (int)LOKI_ARRAY_COUNT(stakers);
  for (int i = 0; i < registration_params.num_contributors; ++i)
  {
    registration_params.contributors[i].addr   = stakers_addr[i];
    registration_params.contributors[i].amount = 25; // TODO(doyle): Assumes testnet staking requirement of 100
  }

  loki_scratch_buf registration_cmd = {};
  daemon_prepare_registration (&registration_params, &registration_cmd);
  assert(wallet_register_service_node(registration_cmd.c_str));
  wallet_mine_atleast_n_blocks(1, 100 /*mining_duration*/);

  // Each person stakes their part to the wallet
  wallet_exit();
  daemon_exit();

  // TODO(doyle): HACK. For some reason the 3rd wallet gets stuck and doesn't
  // connect to the daemon. Maybe a max connection limit?
  start_daemon(&daemon);
  LOKI_DEFER { daemon_exit(); };

  for (int i = 1; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
  {
    start_wallet(stakers + i, wallet_params);
    LOKI_DEFER { wallet_exit(); };

    loki_contributor const *contributor = registration_params.contributors + i;
    assert(wallet_stake(&snode_key, &contributor->addr, contributor->amount));
  }

  start_wallet(owner, wallet_params);
  wallet_mine_atleast_n_blocks(1);
  EXPECT(result, daemon_print_sn_status(), "Service node could not be registered");
  wallet_exit();
  return result;
}

test_result transfer__expect_fee_amount()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon = create_daemon();
  wallet_t wallet = create_wallet();

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;

  start_daemon(&daemon);
  start_wallet(&wallet, wallet_params);
  LOKI_DEFER { daemon_exit(); wallet_exit(); };

  wallet_set_default_testing_settings();
  wallet_mine_atleast_n_blocks(100);
  loki_transaction tx = wallet_transfer("T6TUmu1R3eX6YJdkDqtHGrM2H25V5exPPiizVdrfze6XCaWqVvQUCKC2fPkwvfxZP5JmUTvHVDaJMcGQfHP2xUeu2YjjuCiMX", 25);

  int64_t const fee_estimate = 71639960;
  int64_t const fee          = static_cast<int64_t>(tx.fee);

  int64_t delta         = LOKI_ABS(fee_estimate - fee);
  int64_t const epsilon = 10000000;

  EXPECT(result, delta < epsilon, "Unexpected fee"); // TODO(doyle): Improve macro to take __va_args__
  return result;
}
