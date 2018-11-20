#include "loki_test_cases.h"

#define LOKI_WALLET_IMPLEMENTATION
#include "loki_wallet.h"

#define LOKI_STR_IMPLEMENTATION
#include "loki_str.h"

#define LOKI_DAEMON_IMPLEMENTATION
#include "loki_daemon.h"

#define EXPECT_STR(test_result_var, src, expect_str, fmt, ...) \
if (!str_match(src, expect_str)) \
{ \
  test_result_var.failed   = true; \
  test_result_var.fail_msg = loki_scratch_buf("[%s != %s] " fmt, src, expect_str, ## __VA_ARGS__); \
  return test_result_var; \
}

#define EXPECT(test_result_var, expr, fmt, ...) \
if (!(expr)) \
{ \
  test_result_var.failed   = true; \
  test_result_var.fail_msg = loki_scratch_buf("[" #expr "] " fmt, ## __VA_ARGS__); \
  return test_result_var; \
}

#define LOKI_ANSI_COLOR_RED     "\x1b[31m"
#define LOKI_ANSI_COLOR_GREEN   "\x1b[32m"
#define LOKI_ANSI_COLOR_YELLOW  "\x1b[33m"
#define LOKI_ANSI_COLOR_BLUE    "\x1b[34m"
#define LOKI_ANSI_COLOR_MAGENTA "\x1b[35m"
#define LOKI_ANSI_COLOR_CYAN    "\x1b[36m"
#define LOKI_ANSI_COLOR_RESET   "\x1b[0m"

void print_test_results(test_result const *test)
{
  int const TARGET_LEN = 80;

  if (test->failed)
  {
    char const STATUS[]     = "FAILED";
    int total_len           = test->name.len + LOKI_CHAR_COUNT(STATUS);
    int const remaining_len = LOKI_MAX(TARGET_LEN - total_len, 0);

    fprintf(stdout, "%s", test->name.c_str);
    for (int i = 0; i < remaining_len; ++i) fputc('.', stdout);
    fprintf(stdout, LOKI_ANSI_COLOR_RED "%s" LOKI_ANSI_COLOR_RESET "\n", STATUS);
    fprintf(stdout, "  Message: %s\n", test->fail_msg.c_str);
  }
  else
  {
    char const STATUS[]     = "OK!";
    int total_len           = test->name.len + LOKI_CHAR_COUNT(STATUS);
    int const remaining_len = LOKI_MAX(TARGET_LEN - total_len, 0);

    fprintf(stdout, "%s", test->name.c_str);
    for (int i = 0; i < remaining_len; ++i) fputc('.', stdout);
    fprintf(stdout, LOKI_ANSI_COLOR_GREEN "%s" LOKI_ANSI_COLOR_RESET "\n", STATUS);
  }
}

struct loki_err_context
{
  bool              success;
  loki_buffer<1024> err_msg;
  operator bool() { return success; }
};

#define INITIALISE_TEST_CONTEXT(test_result_var) itest_reset_shared_memory(); test_result_var.name = loki_buffer<512>(__func__)

test_result prepare_registration__solo_auto_stake()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon = create_daemon();
  start_daemon(&daemon);

  LOKI_DEFER { itest_write_to_stdin_mem(itest_shared_mem_type::daemon, "exit"); };
  char const wallet1[] = "T6U4ukY68vohsfrGMryFmqX5yRE4d5EC8E6QbinSo8ssW3heqoNjgNggTeym9NSLW4cnEp3ckpD9RZLW5qDGg3821c9SAtHMD";

  itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type::daemon, "prepare_registration");
  itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type::daemon, "y"); // Contribute entire stake?
  itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type::daemon, wallet1); // Operator address
  itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type::daemon, "y"); // Auto restake?
  loki_scratch_buf output = itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type::daemon, "y"); // Confirm information correct?

  // Expected Format: register_service_node [auto] <operator cut> <address> <fraction> [<address> <fraction> [...]]]
  char const *register_str      = str_find(&output, "register_service_node");
  char const *prev              = register_str;

  char const *auto_stake        = str_skip_to_next_word(&prev);
  char const *operator_portions = str_skip_to_next_word(&prev);
  char const *wallet_addr       = str_skip_to_next_word(&prev);
  char const *addr1_portions    = str_skip_to_next_word(&prev);

  EXPECT_STR(result, register_str,      "register_service_node", "Could not find expected str in: %s", output.c_str);
  EXPECT_STR(result, auto_stake,        "auto",                  "Could not find expected str in: %s", output.c_str);
  EXPECT_STR(result, operator_portions, "18446744073709551612",  "Could not find expected str in: %s", output.c_str);
  EXPECT_STR(result, wallet_addr,       wallet1,                 "Could not find expected str in: %s", output.c_str);
  EXPECT_STR(result, addr1_portions,    "18446744073709551612",  "Could not find expected str in: %s", output.c_str);

  return result;
}

test_result prepare_registration__100_percent_operator_cut_auto_stake()
{

  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon = create_daemon();
  start_daemon(&daemon);
  LOKI_DEFER { itest_write_to_stdin_mem(itest_shared_mem_type::daemon, "exit"); };

  char const wallet1[] = "T6U4ukY68vohsfrGMryFmqX5yRE4d5EC8E6QbinSo8ssW3heqoNjgNggTeym9NSLW4cnEp3ckpD9RZLW5qDGg3821c9SAtHMD";
  char const wallet2[] = "T6TZgnpJ2uaC1cqS4E6M6u7QmGA79q2G19ToBHnqWHxMMDocNTiw2phg52XjkAmEZH9V5xQUsaR3cbcTnELE1vXP2YkhEqXad";

  itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type::daemon, "prepare_registration");
  itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type::daemon, "n"); // Contribute entire stake?
  itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type::daemon, "100%"); // Operator cut
  itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type::daemon, "50"); // How much loki to reserve?
  itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type::daemon, "y"); // Do you want to reserve portions of the stake for other contribs?
  itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type::daemon, "1"); // Number of additional contributors
  itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type::daemon, wallet1); // Operator address
  itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type::daemon, "25"); // How much loki to reserve for contributor 1
  itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type::daemon, wallet2); // Contrib 1 address
  itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type::daemon, "y"); // How much loki to reserve for contributor 1
  itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type::daemon, "y"); // Autostake
  loki_scratch_buf output = itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type::daemon, "y"); // Confirm

  // Expected Format: register_service_node [auto] <operator cut> <address> <fraction> [<address> <fraction> [...]]]
  char const *register_str     = str_find(&output, "register_service_node");
  char const *prev             = register_str;

  char const *auto_stake       = str_skip_to_next_word(&prev);
  char const *operator_cut     = str_skip_to_next_word(&prev);
  char const *wallet1_addr     = str_skip_to_next_word(&prev);
  char const *wallet1_portions = str_skip_to_next_word(&prev);
  char const *wallet2_addr     = str_skip_to_next_word(&prev);
  char const *wallet2_portions = str_skip_to_next_word(&prev);

  EXPECT_STR(result, register_str,     "register_service_node", "Could not find expected str in: ", output.c_str);
  EXPECT_STR(result, auto_stake,       "auto",                  "Could not find expected str in: ", output.c_str);
  EXPECT_STR(result, operator_cut,     "18446744073709551612",  "Could not find expected str in: ", output.c_str);
  EXPECT_STR(result, wallet1_addr,     wallet1,                 "Could not find expected str in: ", output.c_str);
  EXPECT_STR(result, wallet1_portions, "9223372036854775806",   "Could not find expected str in: ", output.c_str); // exactly 50% of staking portions
  EXPECT_STR(result, wallet2_addr,     wallet2,                 "Could not find expected str in: ", output.c_str);
  EXPECT_STR(result, wallet2_portions, "4611686018427387903",   "Could not find expected str in: ", output.c_str); // exactly 25% of staking portions

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
    stakers[i] = create_wallet(loki_nettype::testnet);
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
  wallet_register_service_node(registration_cmd.c_str);
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
    LOKI_ASSERT(wallet_stake(&snode_key, &contributor->addr, contributor->amount));
  }

  start_wallet(owner, wallet_params);
  wallet_mine_atleast_n_blocks(1);
  EXPECT(result, daemon_print_sn_status(), "Service node could not be registered");
  wallet_exit();
  return result;
}

test_result register_service_node__grace_period()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.add_hardfork(7,   0);
  daemon_params.add_hardfork(8,  10);
  daemon_params.add_hardfork(9,  20);
  daemon_params.add_hardfork(10, 30);
  daemon_t daemon = create_and_start_daemon(daemon_params);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;
  wallet_t wallet                   = create_and_start_wallet(daemon_params.nettype, wallet_params);
  LOKI_DEFER { daemon_exit(); wallet_exit(); };
  wallet_set_default_testing_settings();

  // Mine enough funds for a registration
  loki_addr my_addr = {};
  {
    wallet_mine_until_unlocked_balance(100 * LOKI_ATOMIC_UNITS, LOKI_SECONDS_TO_MS(4)/*mining_duration_in_ms*/);
    wallet_address(0, &my_addr);
    wallet_sweep_all(my_addr.c_str);
    os_sleep_ms(2000); // TODO(doyle): Hack. Sweep all is inevitably going to construct multiple tx's and will take time to write out to stdout
    wallet_mine_until_unlocked_balance(100 * LOKI_ATOMIC_UNITS, 100/*mining_duration_in_ms*/);
  }

  // Register the service node
  daemon_prepare_registration_params registration_params = {};
  {
    registration_params.num_contributors                   = 1;
    registration_params.contributors[0].addr               = my_addr;
    registration_params.contributors[0].amount             = 100; // TODO(doyle): Assuming testnet

    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration(&registration_params, &registration_cmd), "Failed to prepare registration for Service Node");
    wallet_register_service_node(registration_cmd.c_str);
    wallet_mine_atleast_n_blocks(1, 50 /*mining_duration_in_ms*/);

    // Check service node registered
    EXPECT(result, daemon_print_sn_status(), "Service node was not registered properly, print_sn_status returned data that wasn't parsable");
  }

  // Mine until within the grace re-registration range
  {
    int const STAKING_DURATION = 30; // TODO(doyle): Workaround for inability to print_sn_status
    daemon_status_t status     = daemon_status();
    int blocks_to_mine         = STAKING_DURATION - LOKI_STAKING_EXCESS_BLOCKS;
    wallet_mine_atleast_n_blocks(blocks_to_mine, 50/*mining_duration_in_ms*/);

    uint64_t wallet_height = wallet_status();
    EXPECT(result, wallet_height <= status.height + STAKING_DURATION,
        "We mined too many blocks! We need to mine within %d blocks of expiry and re-register within that time frame to test the grace period. The current wallet height: %d, node expired at height: %d",
        LOKI_STAKING_EXCESS_BLOCKS, wallet_height, status.height + STAKING_DURATION);
  }

  // Re-register within this grace period, re-use the registration params
  {
    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration(&registration_params, &registration_cmd), "Failed to prepare registration for Service Node");
    wallet_register_service_node(registration_cmd.c_str);
    wallet_mine_atleast_n_blocks(1, 100 /*mining_duration_in_ms*/);
    EXPECT(result, daemon_print_sn_status(), "Service node was not re-registered properly, print_sn_status returned data that wasn't parsable");
  }

  return result;
}


test_result stake__disallow_insufficient_stake_w_not_reserved_contributor()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);
  start_daemon_params daemon_params = {};
  daemon_t daemon                   = create_and_start_daemon(daemon_params);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;

  // Setup wallet 1 service node
  loki_addr main_addr = {};
  {
    wallet_t wallet = create_and_start_wallet(daemon_params.nettype, wallet_params);
    LOKI_DEFER { wallet_exit(); };
    wallet_set_default_testing_settings();

    wallet_address(0, &main_addr);
    wallet_mine_atleast_n_blocks(100, LOKI_SECONDS_TO_MS(4));
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
    wallet_mine_atleast_n_blocks(1, 100 /*mining_duration_in_ms*/);
    EXPECT(result, daemon_print_sn_status(), "Service node failed to register");
  }

  loki_snode_key snode_key = {};
  EXPECT(result, daemon_print_sn_key(&snode_key), "Failed to print service node key");

  // Wallet 2 try to stake with insufficient balance
  {
    wallet_t contributor = create_and_start_wallet(daemon_params.nettype, wallet_params);
    LOKI_DEFER { wallet_exit(); };
    wallet_set_default_testing_settings();

    loki_addr contributor_addr = {};
    wallet_address(0, &contributor_addr);
    wallet_mine_until_unlocked_balance(20, LOKI_SECONDS_TO_MS(1)/*mining_duration_in_ms*/);

    EXPECT(result, wallet_stake(&snode_key, &contributor_addr, 10) == false,
        "An open service node registration should disallow an insufficient stake, otherwise we lock up the funds for no reason.");
  }

  return result;
}

test_result stake__from_subaddress()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_t daemon                   = create_and_start_daemon(daemon_params);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;
  wallet_t operator_wallet          = create_and_start_wallet(daemon_params.nettype, wallet_params);
  LOKI_DEFER { wallet_exit(); daemon_exit(); };

  wallet_set_default_testing_settings();
  EXPECT(result, wallet_set_daemon(&daemon), "Could not set wallet with daemon at port 127.0.0.1:%d", daemon.rpc_port);
  wallet_mine_atleast_n_blocks(100, LOKI_SECONDS_TO_MS(4));

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

test_result transfer__expect_fee_amount()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.add_hardfork(7, 0);
  daemon_params.add_hardfork(8, 1);
  daemon_params.add_hardfork(9, 2);
  daemon_t daemon = create_and_start_daemon(daemon_params);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;
  wallet_t wallet                   = create_and_start_wallet(daemon_params.nettype, wallet_params);

  LOKI_DEFER { daemon_exit(); wallet_exit(); };
  wallet_set_default_testing_settings();
  wallet_mine_atleast_n_blocks(100, 4000);

  loki_addr wallet_addr = {};
  LOKI_ASSERT(wallet_address(0, &wallet_addr));
  wallet_exit();

  wallet_t dest = create_and_start_wallet(daemon_params.nettype, wallet_params);
  wallet_set_default_testing_settings();
  loki_addr dest_addr = {};
  LOKI_ASSERT(wallet_address(0, &dest_addr));
  wallet_exit();

  int64_t const fee_estimate = 71639960;
  int64_t const epsilon      = 10000000;
  // Transfer from wallet to dest
  {
    start_wallet(&wallet, wallet_params);
    loki_transaction tx = wallet_transfer(dest_addr.c_str, 50);
    wallet_mine_atleast_n_blocks(30);
    wallet_exit();

    int64_t fee                = static_cast<int64_t>(tx.fee);
    int64_t delta              = LOKI_ABS(fee_estimate - fee);
    EXPECT(result, delta < epsilon, "Unexpected difference in fee estimate: %zd, fee: %zd, with delta: %zd >= epsilon: %zd", fee_estimate, fee, delta, epsilon);
  }

  // Transfer from dest to wallet
  {
    start_wallet(&dest, wallet_params);
    wallet_refresh();
    loki_transaction tx = wallet_transfer(wallet_addr.c_str, 45);
    wallet_exit();

    int64_t fee   = static_cast<int64_t>(tx.fee);
    int64_t delta = LOKI_ABS(fee_estimate - fee);
    EXPECT(result, delta < epsilon, "Unexpected difference in fee estimate: %zd, fee: %zd, with delta: %zd >= epsilon: %zd", fee_estimate, fee, delta, epsilon);
  }

  return result;
}

test_result transfer__expect_fee_amount_bulletproofs()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.add_hardfork(7, 0);
  daemon_params.add_hardfork(8, 1);
  daemon_params.add_hardfork(9, 2);
  daemon_params.add_hardfork(10, 3);
  daemon_t daemon = create_and_start_daemon(daemon_params);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;
  wallet_t wallet                   = create_and_start_wallet(daemon_params.nettype, wallet_params);

  LOKI_DEFER { daemon_exit(); wallet_exit(); };
  wallet_set_default_testing_settings();
  wallet_mine_atleast_n_blocks(100, 4000);

  loki_addr wallet_addr = {};
  LOKI_ASSERT(wallet_address(0, &wallet_addr));
  wallet_exit();

  wallet_t dest = create_and_start_wallet(daemon_params.nettype, wallet_params);
  wallet_set_default_testing_settings();
  loki_addr dest_addr = {};
  LOKI_ASSERT(wallet_address(0, &dest_addr));
  wallet_exit();

  int64_t const fee_estimate = 2170050;
  int64_t const epsilon      = 1000000;
  // Transfer from wallet to dest
  {
    start_wallet(&wallet, wallet_params);
    loki_transaction tx = wallet_transfer(dest_addr.c_str, 50);
    wallet_mine_atleast_n_blocks(30);
    wallet_exit();

    int64_t fee                = static_cast<int64_t>(tx.fee);
    int64_t delta              = LOKI_ABS(fee_estimate - fee);
    EXPECT(result, delta < epsilon, "Unexpected difference in fee estimate: %zd, fee: %zd, with delta: %zd >= epsilon: %zd", fee_estimate, fee, delta, epsilon);
  }

  // Transfer from dest to wallet
  {
    start_wallet(&dest, wallet_params);
    wallet_refresh();
    loki_transaction tx = wallet_transfer(wallet_addr.c_str, 45);
    wallet_exit();

    int64_t fee   = static_cast<int64_t>(tx.fee);
    int64_t delta = LOKI_ABS(fee_estimate - fee);
    EXPECT(result, delta < epsilon, "Unexpected difference in fee estimate: %zd, fee: %zd, with delta: %zd >= epsilon: %zd", fee_estimate, fee, delta, epsilon);
  }

  return result;
}
