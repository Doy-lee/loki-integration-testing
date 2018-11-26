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

#define INITIALISE_TEST_CONTEXT(test_result_var) test_result_var.name = loki_buffer<512>(__func__)

test_result prepare_registration__solo_auto_stake()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon = create_and_start_daemon();
  LOKI_DEFER { daemon_exit(&daemon); };

  char const wallet1[] = "T6U4ukY68vohsfrGMryFmqX5yRE4d5EC8E6QbinSo8ssW3heqoNjgNggTeym9NSLW4cnEp3ckpD9RZLW5qDGg3821c9SAtHMD";

  itest_write_to_stdin_mem_and_get_result(&daemon.shared_mem, "prepare_registration");
  itest_write_to_stdin_mem_and_get_result(&daemon.shared_mem, "y"); // Contribute entire stake?
  itest_write_to_stdin_mem_and_get_result(&daemon.shared_mem, wallet1); // Operator address
  itest_write_to_stdin_mem_and_get_result(&daemon.shared_mem, "y"); // Auto restake?
  loki_scratch_buf output = itest_write_to_stdin_mem_and_get_result(&daemon.shared_mem, "y"); // Confirm information correct?

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

  daemon_t daemon = create_and_start_daemon();
  LOKI_DEFER { daemon_exit(&daemon); };

  char const wallet1[] = "T6U4ukY68vohsfrGMryFmqX5yRE4d5EC8E6QbinSo8ssW3heqoNjgNggTeym9NSLW4cnEp3ckpD9RZLW5qDGg3821c9SAtHMD";
  char const wallet2[] = "T6TZgnpJ2uaC1cqS4E6M6u7QmGA79q2G19ToBHnqWHxMMDocNTiw2phg52XjkAmEZH9V5xQUsaR3cbcTnELE1vXP2YkhEqXad";

  itest_write_to_stdin_mem_and_get_result(&daemon.shared_mem, "prepare_registration");
  itest_write_to_stdin_mem_and_get_result(&daemon.shared_mem, "n"); // Contribute entire stake?
  itest_write_to_stdin_mem_and_get_result(&daemon.shared_mem, "100%"); // Operator cut
  itest_write_to_stdin_mem_and_get_result(&daemon.shared_mem, "50"); // How much loki to reserve?
  itest_write_to_stdin_mem_and_get_result(&daemon.shared_mem, "y"); // Do you want to reserve portions of the stake for other contribs?
  itest_write_to_stdin_mem_and_get_result(&daemon.shared_mem, "1"); // Number of additional contributors
  itest_write_to_stdin_mem_and_get_result(&daemon.shared_mem, wallet1); // Operator address
  itest_write_to_stdin_mem_and_get_result(&daemon.shared_mem, "25"); // How much loki to reserve for contributor 1
  itest_write_to_stdin_mem_and_get_result(&daemon.shared_mem, wallet2); // Contrib 1 address
  itest_write_to_stdin_mem_and_get_result(&daemon.shared_mem, "y"); // How much loki to reserve for contributor 1
  itest_write_to_stdin_mem_and_get_result(&daemon.shared_mem, "y"); // Autostake
  loki_scratch_buf output = itest_write_to_stdin_mem_and_get_result(&daemon.shared_mem, "y"); // Confirm

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

  daemon_t daemon           = create_and_start_daemon();
  wallet_t  stakers[4]      = {};
  loki_addr stakers_addr[4] = {};

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;

  // Mine and unlock funds
  for (int i = 0; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
  {
    stakers[i]       = create_and_start_wallet(loki_nettype::testnet, wallet_params);
    wallet_t *staker = stakers + i;

    wallet_set_default_testing_settings(staker);
    wallet_address(staker, 0, stakers_addr + i);
    wallet_mine_atleast_n_blocks(staker, 2);
  }

  LOKI_DEFER
  {
    daemon_exit(&daemon);
    for (int i = 0; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
      wallet_exit(stakers + i);
  };

  wallet_t *owner = stakers + 0;
  wallet_mine_atleast_n_blocks(owner, 100);

  loki_snode_key snode_key = {};
  daemon_print_sn_key(&daemon, &snode_key);

  // Register the service node and put it on the chain
  daemon_prepare_registration_params registration_params = {};
  registration_params.num_contributors = (int)LOKI_ARRAY_COUNT(stakers);
  for (int i = 0; i < registration_params.num_contributors; ++i)
  {
    registration_params.contributors[i].addr   = stakers_addr[i];
    registration_params.contributors[i].amount = 25; // TODO(doyle): Assumes testnet staking requirement of 100
  }

  loki_scratch_buf registration_cmd = {};
  daemon_prepare_registration (&daemon, &registration_params, &registration_cmd);
  wallet_register_service_node(owner, registration_cmd.c_str);
  wallet_mine_atleast_n_blocks(owner, 1, 100 /*mining_duration*/);


  // Each person stakes their part to the wallet
  for (int i = 1; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
  {
    wallet_t *staker = stakers + i;
    loki_contributor const *contributor = registration_params.contributors + i;
    LOKI_ASSERT(wallet_stake(staker, &snode_key, &contributor->addr, contributor->amount));
  }

  wallet_mine_atleast_n_blocks(owner, 1);
  EXPECT(result, daemon_print_sn_status(&daemon), "Service node could not be registered");
  return result;
}

test_result register_service_node__cant_register_twice()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.nettype             = loki_nettype::testnet;
  daemon_t daemon                   = create_and_start_daemon(daemon_params);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;
  wallet_t wallet                   = create_and_start_wallet(daemon_params.nettype, wallet_params);
  LOKI_DEFER { daemon_exit(&daemon); wallet_exit(&wallet); };
  wallet_set_default_testing_settings(&wallet);

  // Mine enough funds for a registration
  loki_addr my_addr = {};
  {
    wallet_address(&wallet, 0, &my_addr);
    wallet_mine_until_unlocked_balance(&wallet, 100 * LOKI_ATOMIC_UNITS, LOKI_SECONDS_TO_MS(4)/*mining_duration_in_ms*/);
  }

  // Register the service node
  loki_scratch_buf registration_cmd = {};
  {
    daemon_prepare_registration_params registration_params = {};
    registration_params.num_contributors                   = 1;
    registration_params.contributors[0].addr               = my_addr;
    registration_params.contributors[0].amount             = 100; // TODO(doyle): Assuming testnet

    EXPECT(result, daemon_prepare_registration(&daemon, &registration_params, &registration_cmd), "Failed to prepare registration for Service Node");
    wallet_register_service_node(&wallet, registration_cmd.c_str);
    wallet_mine_atleast_n_blocks(&wallet, 1, 50 /*mining_duration_in_ms*/);

    // Check service node registered
    EXPECT(result, daemon_print_sn_status(&daemon), "Service node was not registered properly, print_sn_status returned data that wasn't parsable");
  }

  // Register twice, should be disallowed
  EXPECT(result, wallet_register_service_node(&wallet, registration_cmd.c_str) == false,
      "You should not be allowed to register twice, if detected on the network");

  return result;
}

test_result register_service_node__gets_payed_expires_and_returns_funds()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_t daemon                   = create_and_start_daemon(daemon_params);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;
  wallet_t master_wallet            = create_and_start_wallet(daemon_params.nettype, wallet_params);
  wallet_t staker1                  = create_and_start_wallet(daemon_params.nettype, wallet_params);
  wallet_t staker2                  = create_and_start_wallet(daemon_params.nettype, wallet_params);

  LOKI_DEFER
  {
    daemon_exit(&daemon);
    wallet_exit(&master_wallet);
    wallet_exit(&staker1);
    wallet_exit(&staker2);
  };

  // Get wallet addresses
  loki_addr staker1_addr, staker2_addr;
  LOKI_ASSERT(wallet_address(&staker1, 0, &staker1_addr));
  LOKI_ASSERT(wallet_address(&staker2, 0, &staker2_addr));

  wallet_set_default_testing_settings(&master_wallet);
  wallet_set_default_testing_settings(&staker1);
  wallet_set_default_testing_settings(&staker2);

  // Fund the stakers wallet
  wallet_mine_atleast_n_blocks(&master_wallet, 100, LOKI_SECONDS_TO_MS(4));
  wallet_mine_until_unlocked_balance(&master_wallet, 150 * LOKI_ATOMIC_UNITS);
  wallet_transfer(&master_wallet, staker1_addr.c_str, 50 + 1);
  wallet_transfer(&master_wallet, staker2_addr.c_str, 50 + 1);
  wallet_mine_atleast_n_blocks(&master_wallet, 20, 500/*mining_duration_in_ms*/); // Get TX's onto the chain and let unlock it for the stakers

  wallet_refresh(&staker1);
  wallet_refresh(&staker2);
  // Staker 1 registers the service node
  {
    // NOTE: The owner takes 100% of the fee, so we can check payouts for staker
    // 1 and check that around ~50 loki returns to the 2nd staker, i.e so
    // payouts don't mess with the 2nd staker too much and we can verify the
    // locked funds returned by checking they have 50 or so loki (barring test
    // fee)
    daemon_prepare_registration_params params             = {};
    params.owner_fee_percent                              = 100;
    params.contributors[params.num_contributors].addr     = staker1_addr;
    params.contributors[params.num_contributors++].amount = 50; // TODO(doyle): Assuming fakechain requirement of 100
    params.contributors[params.num_contributors].addr     = staker2_addr;
    params.contributors[params.num_contributors++].amount = 50;

    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration (&daemon, &params, &registration_cmd), "Failed to prepare registration");
    EXPECT(result, wallet_register_service_node(&staker1, registration_cmd.c_str),    "Failed to register service node");
  }

  // Staker 2 fulfils the service node contribution
  {
    loki_snode_key snode_key = {};
    daemon_print_sn_key(&daemon, &snode_key);
    wallet_mine_atleast_n_blocks(&master_wallet, 5, 100/*mining_duration_in_ms*/); // Get registration onto the chain
    EXPECT(result, wallet_stake(&staker2, &snode_key, &staker2_addr, 50), "Staker 2 failed to stake to service node");
  }

  // TODO(doyle): assuming fakechain staking duration is 30 blocks + lock blocks excess.
  wallet_mine_atleast_n_blocks(&master_wallet, 5 + 30 + LOKI_STAKING_EXCESS_BLOCKS); // Get node onto chain and mine until expiry

  // Check balances
  {
    wallet_refresh(&staker1);
    wallet_refresh(&staker2);
    uint64_t staker1_balance = wallet_balance(&staker1, nullptr);
    uint64_t staker2_balance = wallet_balance(&staker2, nullptr);
    EXPECT(result, staker1_balance > 500 * LOKI_ATOMIC_UNITS,                                               "Service node operator took 100\% of the fees, they should have a lot of rewards. Certainly more than 500, we have: %zu", staker1_balance);
    EXPECT(result, staker2_balance >= 45  * LOKI_ATOMIC_UNITS && staker2_balance <= 55 * LOKI_ATOMIC_UNITS, "The contributor should of received no payouts and received their stake back, so around ~50LOKI not including fees: %zu", staker2_balance);
  }
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
  LOKI_DEFER { daemon_exit(&daemon); wallet_exit(&wallet); };
  wallet_set_default_testing_settings(&wallet);

  // Mine enough funds for a registration
  loki_addr my_addr = {};
  {
    wallet_mine_until_unlocked_balance(&wallet, 100 * LOKI_ATOMIC_UNITS, LOKI_SECONDS_TO_MS(4)/*mining_duration_in_ms*/);
    wallet_address(&wallet, 0, &my_addr);
    wallet_sweep_all(&wallet, my_addr.c_str);
    wallet_mine_until_unlocked_balance(&wallet, 100 * LOKI_ATOMIC_UNITS, 100/*mining_duration_in_ms*/);
  }

  // Register the service node
  daemon_prepare_registration_params registration_params = {};
  {
    registration_params.num_contributors                   = 1;
    registration_params.contributors[0].addr               = my_addr;
    registration_params.contributors[0].amount             = 100; // TODO(doyle): Assuming fakechain

    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration(&daemon, &registration_params, &registration_cmd), "Failed to prepare registration for Service Node");
    wallet_register_service_node(&wallet, registration_cmd.c_str);
    wallet_mine_atleast_n_blocks(&wallet, 1, 50 /*mining_duration_in_ms*/);

    // Check service node registered
    EXPECT(result, daemon_print_sn_status(&daemon), "Service node was not registered properly, print_sn_status returned data that wasn't parsable");
  }

  // TODO(doyle): Assumes fakechain
  // Mine until within the grace re-registration range
  {
    int const STAKING_DURATION = 30; // TODO(doyle): Workaround for inability to print_sn_status
    daemon_status_t status     = daemon_status(&daemon);
    int blocks_to_mine         = STAKING_DURATION - LOKI_STAKING_EXCESS_BLOCKS;
    wallet_mine_atleast_n_blocks(&wallet, blocks_to_mine, 50/*mining_duration_in_ms*/);

    uint64_t wallet_height = wallet_status(&wallet);
    EXPECT(result, wallet_height <= status.height + STAKING_DURATION,
        "We mined too many blocks! We need to mine within %d blocks of expiry and re-register within that time frame to test the grace period. The current wallet height: %d, node expired at height: %d",
        LOKI_STAKING_EXCESS_BLOCKS, wallet_height, status.height + STAKING_DURATION);
  }

  // Re-register within this grace period, re-use the registration params
  {
    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration(&daemon, &registration_params, &registration_cmd), "Failed to prepare registration for Service Node");
    wallet_register_service_node(&wallet, registration_cmd.c_str);
    wallet_mine_atleast_n_blocks(&wallet, 1, 100 /*mining_duration_in_ms*/);
    EXPECT(result, daemon_print_sn_status(&daemon), "Service node was not re-registered properly, print_sn_status returned data that wasn't parsable");
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
  loki_addr main_addr  = {};
  wallet_t owner       = create_and_start_wallet(daemon_params.nettype, wallet_params);
  wallet_t contributor = create_and_start_wallet(daemon_params.nettype, wallet_params);
  LOKI_DEFER { daemon_exit(&daemon); wallet_exit(&owner); wallet_exit(&contributor); };

  {
    wallet_set_default_testing_settings(&owner);
    wallet_address(&owner, 0, &main_addr);
    wallet_mine_atleast_n_blocks(&owner, 100, LOKI_SECONDS_TO_MS(4));
    loki_scratch_buf registration_cmd = {};
    {
      daemon_prepare_registration_params params = {};
      params.open_pool                          = true;
      params.num_contributors                   = 1;
      params.contributors[0].addr               = main_addr;
      params.contributors[0].amount             = 25;
      daemon_prepare_registration(&daemon, &params, &registration_cmd);
    }

    wallet_register_service_node(&owner, registration_cmd.c_str);
    wallet_mine_atleast_n_blocks(&owner, 1, 100 /*mining_duration_in_ms*/);
    EXPECT(result, daemon_print_sn_status(&daemon), "Service node failed to register");
  }

  loki_snode_key snode_key = {};
  EXPECT(result, daemon_print_sn_key(&daemon, &snode_key), "Failed to print service node key");

  // Wallet 2 try to stake with insufficient balance
  {
    wallet_set_default_testing_settings(&contributor);

    loki_addr contributor_addr = {};
    wallet_address(&contributor, 0, &contributor_addr);
    wallet_mine_until_unlocked_balance(&contributor, 20, LOKI_SECONDS_TO_MS(1)/*mining_duration_in_ms*/);

    EXPECT(result, wallet_stake(&contributor, &snode_key, &contributor_addr, 10) == false,
        "An open service node registration should disallow an insufficient stake, otherwise we lock up the funds for no reason.");
  }

  return result;
}

test_result stake__allow_insufficient_stake_w_reserved_contributor()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);
  start_daemon_params daemon_params = {};
  daemon_t daemon                   = create_and_start_daemon(daemon_params);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;

  wallet_t wallet1 = create_and_start_wallet(daemon_params.nettype, wallet_params);
  wallet_t wallet2 = create_and_start_wallet(daemon_params.nettype, wallet_params);
  LOKI_DEFER { daemon_exit(&daemon); wallet_exit(&wallet1); wallet_exit(&wallet2); };

  // Collect funds for each wallet
  loki_addr wallet1_addr = {};
  loki_addr wallet2_addr = {};
  {
    wallet_set_default_testing_settings(&wallet1);
    wallet_address(&wallet1, 0, &wallet1_addr);
    wallet_mine_atleast_n_blocks(&wallet1, 50, LOKI_SECONDS_TO_MS(1));

    wallet_set_default_testing_settings(&wallet2);
    wallet_address(&wallet2, 0, &wallet2_addr);
    wallet_mine_atleast_n_blocks(&wallet2, 50, LOKI_SECONDS_TO_MS(1));
  }

  // Put the registration onto the blockchain
  {
    loki_scratch_buf registration_cmd                     = {};
    daemon_prepare_registration_params params             = {};
    params.open_pool                                      = true;
    params.contributors[params.num_contributors].addr     = wallet1_addr;
    params.contributors[params.num_contributors++].amount = 25;
    params.contributors[params.num_contributors].addr     = wallet2_addr;
    params.contributors[params.num_contributors++].amount = 50;
    daemon_prepare_registration(&daemon, &params, &registration_cmd);

    wallet_register_service_node(&wallet1, registration_cmd.c_str);
    wallet_mine_atleast_n_blocks(&wallet1, 1, 100 /*mining_duration_in_ms*/);
    EXPECT(result, daemon_print_sn_status(&daemon), "Service node failed to register");
  }

  loki_snode_key snode_key = {};
  EXPECT(result, daemon_print_sn_key(&daemon, &snode_key), "Failed to print service node key");

  // Wallet 2 try to stake with insufficient balance, should be allowed if they have reserved a spot
  EXPECT(result, wallet_stake(&wallet2, &snode_key, &wallet2_addr, 10),
      "A service node registration with a reserved contributor should allow staking in insufficient parts.");

  // TODO(doyle): You should mine, then parse the results of print_sn_status to make sure the 10 loki was accepted
  // Right now this only checks that the wallet allows you to, it will warn you, that you need to contribute more
  // but it will still let you and it should update the registration status with the 10 loki.
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
  LOKI_DEFER { wallet_exit(&operator_wallet); daemon_exit(&daemon); };

  wallet_set_default_testing_settings(&operator_wallet);
  EXPECT(result, wallet_set_daemon(&operator_wallet, &daemon), "Could not set wallet with daemon at port 127.0.0.1:%d", daemon.rpc_port);
  wallet_mine_atleast_n_blocks(&operator_wallet, 100, LOKI_SECONDS_TO_MS(4));

  loki_addr subaddr = wallet_address_new(&operator_wallet);
  {
    loki_addr check = {};
    wallet_address(&operator_wallet, 1, &check);
    EXPECT(result, strcmp(subaddr.c_str, check.c_str) == 0, "Subaddresses did not match, expected: %s, received: %s", subaddr.c_str, check.c_str);
  }

  loki_addr main_addr = {};
  wallet_address(&operator_wallet, 0, &main_addr);

  wallet_transfer(&operator_wallet, &subaddr, 30);
  wallet_address(&operator_wallet, 1);
  for (uint64_t subaddr_unlocked_bal = 0; subaddr_unlocked_bal < 30;)
  {
    wallet_mine_atleast_n_blocks(&operator_wallet, 1);
    wallet_balance(&operator_wallet, &subaddr_unlocked_bal);
  }

  loki_scratch_buf registration_cmd = {};
  {
    daemon_prepare_registration_params params = {};
    params.open_pool                          = true;
    params.num_contributors                   = 1;
    params.contributors[0].addr               = main_addr;
    params.contributors[0].amount             = 25;
    daemon_prepare_registration(&daemon, &params, &registration_cmd);
  }

  wallet_register_service_node(&operator_wallet, registration_cmd.c_str);
  wallet_mine_atleast_n_blocks(&operator_wallet, 1);

  loki_snode_key snode_key = {};
  EXPECT(result, daemon_print_sn_key(&daemon, &snode_key), "Failed to print sn key");

  wallet_stake(&operator_wallet, &snode_key, &subaddr, 25);
  return result;
}

test_result stake__incremental_staking_until_node_active()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_t daemon                   = create_and_start_daemon(daemon_params);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;
  wallet_t wallet                   = create_and_start_wallet(daemon_params.nettype, wallet_params);

  wallet_set_default_testing_settings(&wallet);
  LOKI_DEFER { wallet_exit(&wallet); daemon_exit(&daemon); };

  loki_addr my_addr = {};
  wallet_mine_until_unlocked_balance(&wallet, 100, LOKI_SECONDS_TO_MS(4));
  EXPECT(result, wallet_address(&wallet, 0, &my_addr), "Failed to get the 0th subaddress, i.e the main address of wallet");

  loki_scratch_buf registration_cmd = {};
  {
    daemon_prepare_registration_params params = {};
    params.open_pool                          = true;
    loki_contributor *contributor             = params.contributors + params.num_contributors++;
    contributor->addr                         = my_addr;
    contributor->amount                       = 50;
    EXPECT(result, daemon_prepare_registration(&daemon, &params, &registration_cmd), "Failed to prepare registration for service node");
  }

  loki_snode_key snode_key = {};
  daemon_print_sn_key(&daemon, &snode_key);
  EXPECT(result, wallet_register_service_node(&wallet, registration_cmd.c_str), "Failed to submit registration for service node with command: %s", registration_cmd.c_str);
  EXPECT(result, wallet_stake(&wallet, &snode_key, &my_addr, 10) == false,      "We should not be able to stake for service node: %s just yet, it should still be sitting in the mempool.", snode_key.c_str);
  wallet_mine_atleast_n_blocks(&wallet, 5, 100 /*mining_duration_in_ms*/);      // Get registration onto chain

  EXPECT(result, wallet_stake(&wallet, &snode_key, &my_addr, 15), "Failed to stake 15 loki to node: %s", snode_key.c_str);
  wallet_mine_atleast_n_blocks(&wallet, 5, 100 /*mining_duration_in_ms*/);      // Get registration onto chain
  EXPECT(result, wallet_stake(&wallet, &snode_key, &my_addr, 15), "Failed to stake 15 loki to node: %s", snode_key.c_str);
  wallet_mine_atleast_n_blocks(&wallet, 5, 100 /*mining_duration_in_ms*/);      // Get registration onto chain
  EXPECT(result, wallet_stake(&wallet, &snode_key, &my_addr, 20), "Failed to stake 20 loki to node: %s", snode_key.c_str);
  wallet_mine_atleast_n_blocks(&wallet, 5, 100 /*mining_duration_in_ms*/);      // Get registration onto chain

  EXPECT(result, wallet_stake(&wallet, &snode_key, &my_addr, 20) == false, "The node: %s should already be registered, staking further should be disallowed!", snode_key.c_str);
  return result;
}

test_result stake__to_non_registered_node_disallowed()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_t daemon                   = create_and_start_daemon(daemon_params);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;
  wallet_t wallet                   = create_and_start_wallet(daemon_params.nettype, wallet_params);
  LOKI_DEFER { wallet_exit(&wallet); daemon_exit(&daemon); };

  loki_addr my_addr = {};
  wallet_mine_atleast_n_blocks(&wallet, 100, LOKI_SECONDS_TO_MS(4));
  EXPECT(result, wallet_address(&wallet, 0, &my_addr), "Failed to get the 0th subaddress, i.e the main address of wallet");

  loki_snode_key snode_key = {};
  daemon_print_sn_key(&daemon, &snode_key);
  EXPECT(result, wallet_stake(&wallet, &snode_key, &my_addr, 15) == false, "You should not be able to stake to a node that is not registered yet!");

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
  wallet_t src_wallet               = create_and_start_wallet(daemon_params.nettype, wallet_params);
  wallet_t dest_wallet              = create_and_start_wallet(daemon_params.nettype, wallet_params);
  LOKI_DEFER { daemon_exit(&daemon); wallet_exit(&src_wallet); wallet_exit(&dest_wallet); };

  wallet_set_default_testing_settings(&src_wallet);
  wallet_set_default_testing_settings(&dest_wallet);
  wallet_mine_atleast_n_blocks(&src_wallet, 100, 4000);

  loki_addr src_addr = {}, dest_addr = {};
  LOKI_ASSERT(wallet_address(&src_wallet,  0, &src_addr));
  LOKI_ASSERT(wallet_address(&dest_wallet, 0, &dest_addr));

  int64_t const fee_estimate = 71639960;
  int64_t const epsilon      = 10000000;

  // Transfer from wallet to dest
  {
    loki_transaction tx = wallet_transfer(&src_wallet, dest_addr.c_str, 50);
    wallet_mine_atleast_n_blocks(&src_wallet, 30);

    int64_t fee                = static_cast<int64_t>(tx.fee);
    int64_t delta              = LOKI_ABS(fee_estimate - fee);
    EXPECT(result, delta < epsilon, "Unexpected difference in fee estimate: %zd, fee: %zd, with delta: %zd >= epsilon: %zd", fee_estimate, fee, delta, epsilon);
  }

  // Transfer from dest to wallet
  {
    wallet_refresh(&dest_wallet);
    loki_transaction tx = wallet_transfer(&dest_wallet, src_addr.c_str, 45);

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
  wallet_t src_wallet               = create_and_start_wallet(daemon_params.nettype, wallet_params);
  wallet_t dest_wallet              = create_and_start_wallet(daemon_params.nettype, wallet_params);
  LOKI_DEFER { daemon_exit(&daemon); wallet_exit(&src_wallet); wallet_exit(&dest_wallet); };

  wallet_set_default_testing_settings(&src_wallet);
  wallet_set_default_testing_settings(&dest_wallet);
  wallet_mine_atleast_n_blocks(&src_wallet, 100, 4000);

  loki_addr src_addr = {}, dest_addr = {};
  LOKI_ASSERT(wallet_address(&src_wallet, 0,  &src_addr));
  LOKI_ASSERT(wallet_address(&dest_wallet, 0, &dest_addr));

  int64_t const fee_estimate = 2170050;
  int64_t const epsilon      = 1000000;

  // Transfer from wallet to dest
  {
    loki_transaction tx = wallet_transfer(&src_wallet, dest_addr.c_str, 50);
    wallet_mine_atleast_n_blocks(&src_wallet, 30);

    int64_t fee                = static_cast<int64_t>(tx.fee);
    int64_t delta              = LOKI_ABS(fee_estimate - fee);
    EXPECT(result, delta < epsilon, "Unexpected difference in fee estimate: %zd, fee: %zd, with delta: %zd >= epsilon: %zd", fee_estimate, fee, delta, epsilon);
  }

  // Transfer from dest to wallet
  {
    wallet_refresh(&dest_wallet);
    loki_transaction tx = wallet_transfer(&dest_wallet, src_addr.c_str, 45);

    int64_t fee   = static_cast<int64_t>(tx.fee);
    int64_t delta = LOKI_ABS(fee_estimate - fee);
    EXPECT(result, delta < epsilon, "Unexpected difference in fee estimate: %zd, fee: %zd, with delta: %zd >= epsilon: %zd", fee_estimate, fee, delta, epsilon);
  }

  return result;
}
