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

  daemon_t daemon = start_daemon();
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

  daemon_t daemon = start_daemon();
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

  daemon_t daemon          = start_daemon();
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

  loki_transaction_id tx_id = wallet_transfer(subaddr, 30);
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

  loki_scratch_buf output = write_to_stdin_mem_and_get_result(shared_mem_type::wallet, registration_cmd.c_str);
  write_to_stdin_mem_and_get_result(shared_mem_type::wallet, "y"); // Confirm?
  wallet_mine_atleast_n_blocks(1);

  loki_snode_key snode_key = {};
  EXPECT(result, daemon_print_sn_key(&snode_key), "Failed to print sn key");

  wallet_stake(&snode_key, &subaddr, 25);

  return result;
}
