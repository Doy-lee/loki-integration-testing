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

char const *LOKI_TESTNET_ADDR[] = // Some fake addresses for use in tests
{
  "T6U4ukY68vohsfrGMryFmqX5yRE4d5EC8E6QbinSo8ssW3heqoNjgNggTeym9NSLW4cnEp3ckpD9RZLW5qDGg3821c9SAtHMD",
  "T6TZgnpJ2uaC1cqS4E6M6u7QmGA79q2G19ToBHnqWHxMMDocNTiw2phg52XjkAmEZH9V5xQUsaR3cbcTnELE1vXP2YkhEqXad",
  "T6SPR75TN9d2x5nYJ1cMN2DWyDpNU2mqvCYb8dceqEKoGqrcKh7sfUAR47HiVrbKsdfibnkLJhW5JexajLrh2ouR177VDj4Lt",
  "T6TbA1iusFDiBwuuRvarSi91o7uVxbqwJirFJiWJvmQ1KnUHoiyXCDh8rBzegzK3oVTECSKLvQrfyAHCbJfhbZui27UufvZJt",
};

char const *LOKI_MAINNET_ADDR[] = // Some fake addresses for use in tests
{
  "LAK9DLQVSLtinNdoRMU9DqQtjMSPAUhz7Cc74dvXBw6j6S4foJDhbUXhTNmA5pLj6cEuV4AG4bVbeD4v3RpG69geLayrya9",
  "LEJfPyQvT5oYeHJWghhiquV8c5svGkeDBBtRJUMzAqcyCnuNtEf7kDKJgK4C3DsQYdFUR4WB6gSa2YsZMBhdEE2r6wTTijh",
  "LDk2LChP8iXPrbmDm3KMQvStHmAWB3gdAVisYbC9uqYXe67L4VE3wJ3N78tSfYi6C7E6YM4ir4JhXfFVQCHpkfpuHGugZjh",
  "LEGbch6JYiUjX3ebUvVZZNiU2wNT3SBD4DZgGH9xN56VGq4obkGsKEF8zGLBXiNnFv5dzQX1Yg1Yx99YSgg4GDaZKw6zxcA",
};

void print_test_results(test_result const *test)
{
  int const TARGET_LEN = 76;

  if (test->failed)
  {
    char const STATUS[]     = "FAILED";
    int total_len           = test->name.len + LOKI_CHAR_COUNT(STATUS);
    int const remaining_len = LOKI_MAX(TARGET_LEN - total_len, 0);

    for (int i = 0; i < remaining_len; ++i) fputc('.', stdout);
    fprintf(stdout, LOKI_ANSI_COLOR_RED "%s" LOKI_ANSI_COLOR_RESET "\n", STATUS);
    fprintf(stdout, "  Message: %s\n\n", test->fail_msg.c_str);
  }
  else
  {
    char const STATUS[]     = "OK!";
    int total_len           = test->name.len + LOKI_CHAR_COUNT(STATUS);
    int const remaining_len = LOKI_MAX(TARGET_LEN - total_len, 0);

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

#define INITIALISE_TEST_CONTEXT(test_result_var) test_result_var.name = loki_buffer<512>(__func__); \

//
// Latest Tests
//

test_result latest__deregistration__1_unresponsive_node()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.load_latest_hardfork_versions();

  int const NUM_DAEMONS                  = (LOKI_QUORUM_SIZE * 2);
  // int const NUM_DAEMONS                  = 2;
  loki_snode_key snode_keys[NUM_DAEMONS] = {};
  daemon_t daemons[NUM_DAEMONS]          = {};

  daemon_t       *register_daemons       = daemons;
  daemon_t       *deregister_daemons     = daemons + LOKI_QUORUM_SIZE;
  loki_snode_key *register_snode_keys    = snode_keys;
  loki_snode_key *deregister_snode_keys  = snode_keys + LOKI_QUORUM_SIZE;
  int             num_deregister_daemons = LOKI_QUORUM_SIZE / 2;
  int             num_register_daemons   = NUM_DAEMONS - num_deregister_daemons;

  create_and_start_multi_daemons(daemons, NUM_DAEMONS, daemon_params);

  for (size_t i = 0; i < NUM_DAEMONS; ++i)
  {
    LOKI_ASSERT(daemon_print_sn_key(daemons + i, snode_keys + i));
  }

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = daemons + 0;
  wallet_t wallet                   = create_and_start_wallet(daemon_params.nettype, wallet_params);
  wallet_set_default_testing_settings(&wallet);

  LOKI_DEFER
  {
    wallet_exit(&wallet);
    for (size_t i = 0; i < (size_t)NUM_DAEMONS; ++i)
      daemon_exit(daemons + i);
  };

  // Prepare blockchain, atleast 100 blocks so we have outputs to pick from
  int const FAKECHAIN_STAKING_REQUIREMENT = 100;
  {
    wallet_mine_atleast_n_blocks(&wallet, 100, LOKI_SECONDS_TO_MS(4));
    wallet_mine_until_unlocked_balance(&wallet, static_cast<int>(NUM_DAEMONS * FAKECHAIN_STAKING_REQUIREMENT)); // TODO(doyle): Assuming staking requirement of 100 fakechain
  }

  // Setup node registration params to come from our single wallet
  {
    daemon_prepare_registration_params register_params                    = {};
    register_params.contributors[register_params.num_contributors].amount = FAKECHAIN_STAKING_REQUIREMENT;
    wallet_address(&wallet, 0, &register_params.contributors[register_params.num_contributors++].addr);

    LOKI_FOR_EACH(i, NUM_DAEMONS) // Register each daemon on the network
    {
      loki_scratch_buf registration_cmd = {};
      daemon_t *daemon                  = daemons + i;
      EXPECT(result, daemon_prepare_registration (daemon, &register_params, &registration_cmd), "Failed to prepare registration");
      EXPECT(result, wallet_register_service_node(&wallet, registration_cmd.c_str),             "Failed to register service node");
    }
  }

  // Daemons to deregister should exit so that they don't ping when they get mined onto the chain
  os_sleep_s(2); // TODO(doyle): Hack: Sleep to give time for txs to propagate
  LOKI_FOR_EACH(i, num_deregister_daemons)
  {
    daemon_t *daemon = deregister_daemons + i;
    daemon_exit(daemon);
  }

  // Mine the registration txs onto the chain and check they have been registered
  {
    wallet_mine_atleast_n_blocks(&wallet, 2, 50/*mining_duration_in_ms*/); // Get onto chain
    os_sleep_s(3); // Give some time for uptime proof to get sent out and propagated

    LOKI_FOR_EACH(i, NUM_DAEMONS)
    {
      loki_snode_key const *snode_key = snode_keys + i;
      daemon_snode_status status = daemon_print_sn(daemons + 0, snode_key);
      EXPECT(result, status.registered, "We expect all daemons to be registered at this point", NUM_DAEMONS);
    }
  }

  // Mine atleast LOKI_REORG_SAFETY_BUFFER. Quorum voting can only start after
  // LOKI_REORG_SAFETY_BUFFER, but also make sure we don't mine too much that
  // the nodes don't naturally expire
  {
    // TODO(doyle): There is abit of randomness here in that if by chance the 10 blocks we mine don't produce a quorum to let us vote off the dead node
    uint64_t old_height= wallet_status(&wallet);
    wallet_mine_atleast_n_blocks(&wallet, LOKI_REORG_SAFETY_BUFFER + 10, 50/*mining_duration_in_ms*/);

    os_sleep_s(30); // Wait for votes to propagate and deregister txs to be formed
    wallet_mine_atleast_n_blocks(&wallet, 5, 50/*mining_duration_in_ms*/);

    uint64_t new_height = wallet_status(&wallet);
    uint64_t blocks_mined = new_height - old_height;
    EXPECT(result,
        blocks_mined < 30 + LOKI_STAKING_EXCESS_BLOCKS,
        "We mined too many blocks everyone has naturally expired meaning we can't check deregistration status! We mined: %d, the maximum blocks we can mine: %d",
        30 + LOKI_STAKING_EXCESS_BLOCKS);
  }

  // Check that there are some nodes that have been deregistered. Not
  // necessarily all of them because we may not have formed a quorum that was to
  // vote them off
  LOKI_FOR_EACH(i, num_register_daemons)
  {
    loki_snode_key *snode_key             = register_snode_keys + i;
    daemon_snode_status node_status       = daemon_print_sn(register_daemons + 0, snode_key);
    EXPECT(result, node_status.registered == true, "Daemon %d should still be online, pingining and alive!", i);
  }

  int total_registered_daemons = num_register_daemons;
  LOKI_FOR_EACH(i, num_deregister_daemons)
  {
    loki_snode_key *snode_key             = deregister_snode_keys + i;
    daemon_snode_status node_status       = daemon_print_sn(register_daemons + 0, snode_key);
    if (node_status.registered) total_registered_daemons++;
  }

  EXPECT(result,
         total_registered_daemons >= num_register_daemons && total_registered_daemons < NUM_DAEMONS,
         "We expect atleast some daemons to deregister. It's possible that not all deregistered if they didn't get assigned a quorum. total_registered_daemons: %d, NUM_DAEMONS: %d",
         total_registered_daemons, NUM_DAEMONS);

  return result;
}

test_result latest__prepare_registration__check_solo_stake()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.load_latest_hardfork_versions();

  daemon_t daemon = create_and_start_daemon(daemon_params);
  LOKI_DEFER { daemon_exit(&daemon); };

  char const *wallet1 = LOKI_MAINNET_ADDR[0];

  daemon_prepare_registration_params registration_params = {};
  registration_params.num_contributors                   = 1;
  registration_params.contributors[0].addr.set_normal_addr(wallet1);
  registration_params.contributors[0].amount = 100; // TODO(doyle): Assumes staking requirement

  loki_scratch_buf registration_cmd = {};
  EXPECT(result, daemon_prepare_registration(&daemon, &registration_params, &registration_cmd), "Failed to prepare registration");

  // Expected Format: register_service_node <operator cut> <address> <fraction> [<address> <fraction> [...]]]
  char const *register_str      = str_find(registration_cmd.c_str, "register_service_node");
  char const *prev              = register_str;

  char const *operator_portions = str_skip_to_next_word(&prev);
  char const *wallet_addr       = str_skip_to_next_word(&prev);
  char const *addr1_portions    = str_skip_to_next_word(&prev);

  EXPECT_STR(result, register_str,      "register_service_node", "Could not find expected str in: %s", register_str);
  EXPECT_STR(result, operator_portions, "18446744073709551612",  "Could not find expected str in: %s", register_str);
  EXPECT_STR(result, wallet_addr,       wallet1,                 "Could not find expected str in: %s", register_str);
  EXPECT_STR(result, addr1_portions,    "18446744073709551612",  "Could not find expected str in: %s", register_str);
  return result;
}

test_result latest__prepare_registration__check_100_percent_operator_cut_stake()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.load_latest_hardfork_versions();

  daemon_t daemon = create_and_start_daemon(daemon_params);
  LOKI_DEFER { daemon_exit(&daemon); };

  char const *wallet1 = LOKI_MAINNET_ADDR[0];
  char const *wallet2 = LOKI_MAINNET_ADDR[1];

  daemon_prepare_registration_params registration_params = {};
  registration_params.num_contributors = 2;
  registration_params.open_pool        = true;
  registration_params.owner_fee_percent = 100;
  registration_params.contributors[0].addr.set_normal_addr(wallet1);
  registration_params.contributors[0].amount = 50; // TODO(doyle): Assumes testnet staking requirement of 100
  registration_params.contributors[1].addr.set_normal_addr(wallet2);
  registration_params.contributors[1].amount = 25; // TODO(doyle): Assumes testnet staking requirement of 100

  loki_scratch_buf registration_cmd = {};
  EXPECT(result, daemon_prepare_registration(&daemon, &registration_params, &registration_cmd), "Failed to prepare registration");

  // Expected Format: register_service_node [auto] <operator cut> <address> <fraction> [<address> <fraction> [...]]]
  char const *register_str     = str_find(registration_cmd.c_str, "register_service_node");
  char const *prev             = register_str;

  char const *operator_cut     = str_skip_to_next_word(&prev);
  char const *wallet1_addr     = str_skip_to_next_word(&prev);
  char const *wallet1_portions = str_skip_to_next_word(&prev);
  char const *wallet2_addr     = str_skip_to_next_word(&prev);
  char const *wallet2_portions = str_skip_to_next_word(&prev);

  EXPECT_STR(result, register_str,     "register_service_node", "Could not find expected str in: ", register_str);
  EXPECT_STR(result, operator_cut,     "18446744073709551612",  "Could not find expected str in: ", register_str);
  EXPECT_STR(result, wallet1_addr,     wallet1,                 "Could not find expected str in: ", register_str);
  EXPECT_STR(result, wallet1_portions, "9223372036854775806",   "Could not find expected str in: ", register_str); // exactly 50% of staking portions
  EXPECT_STR(result, wallet2_addr,     wallet2,                 "Could not find expected str in: ", register_str);
  EXPECT_STR(result, wallet2_portions, "4611686018427387903",   "Could not find expected str in: ", register_str); // exactly 25% of staking portions

  return result;
}

test_result latest__print_locked_stakes__check_no_locked_stakes()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon = {};
  wallet_t wallet = {};
  LOKI_DEFER { wallet_exit(&wallet); daemon_exit(&daemon); };

  // Start up daemon and wallet
  {
    start_daemon_params daemon_params = {};
    daemon_params.load_latest_hardfork_versions();
    daemon                            = create_and_start_daemon(daemon_params);

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    wallet                            = create_and_start_wallet(daemon_params.nettype, wallet_params);
    wallet_set_default_testing_settings(&wallet);
  }

  wallet_locked_stakes stakes = wallet_print_locked_stakes(&wallet);
  EXPECT(result, stakes.locked_stakes_len == 0, "We haven't staked, so there should be no locked stakes");
  EXPECT(result, stakes.blacklisted_stakes_len == 0, "We haven't staked, so there should be no locked stakes");
  return result;
}

test_result latest__print_locked_stakes__check_shows_locked_stakes()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon = {};
  wallet_t wallet = {};
  LOKI_DEFER { wallet_exit(&wallet); daemon_exit(&daemon); };

  // Start up daemon and wallet
  {
    start_daemon_params daemon_params = {};
    daemon_params.load_latest_hardfork_versions();
    daemon                            = create_and_start_daemon(daemon_params);

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    wallet                            = create_and_start_wallet(daemon_params.nettype, wallet_params);
    wallet_set_default_testing_settings(&wallet);
  }

  loki_addr my_addr = {};
  wallet_mine_atleast_n_blocks(&wallet, 100, LOKI_SECONDS_TO_MS(4));
  EXPECT(result, wallet_address(&wallet, 0, &my_addr), "Failed to get the 0th subaddress, i.e the main address of wallet");

  // Register the service node
  uint64_t const half_stake_requirement = LOKI_FAKENET_STAKING_REQUIREMENT / 2;
  {
    daemon_prepare_registration_params params             = {};
    params.open_pool                                      = true;
    params.contributors[params.num_contributors].addr     = my_addr;
    params.contributors[params.num_contributors++].amount = half_stake_requirement;

    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration (&daemon, &params, &registration_cmd), "Failed to prepare registration");
    EXPECT(result, wallet_register_service_node(&wallet, registration_cmd.c_str),    "Failed to register service node");
  }

  wallet_mine_atleast_n_blocks(&wallet, 1); // Get registration onto the chain

  // Stake the remainder to make 2 locked stakes for this node and mine some blocks to get us registered
  loki_snode_key snode_key = {};
  {
    EXPECT(result, daemon_print_sn_key(&daemon, &snode_key), "Failed to print sn key");
    EXPECT(result, wallet_stake(&wallet, &snode_key, half_stake_requirement), "Failed to stake remaining loki to service node");
    wallet_mine_atleast_n_blocks(&wallet, 1); // Get stake onto the chain
  }

  // Check stakes show up as locked
  {
    wallet_locked_stakes stakes = wallet_print_locked_stakes(&wallet);
    EXPECT(result, stakes.locked_stakes_len == 1, "We staked, so we should have locked key images");


    wallet_locked_stake const *stake = stakes.locked_stakes + 0;
    EXPECT(result, stake->total_locked_amount == LOKI_FAKENET_STAKING_REQUIREMENT * LOKI_ATOMIC_UNITS, "Total locked amount: %zu didn't match expected: %zu", stake->total_locked_amount, LOKI_FAKENET_STAKING_REQUIREMENT * LOKI_ATOMIC_UNITS);
    EXPECT(result, stake->snode_key == snode_key, "Locked stake service node key didn't match what we expected, stake->snode_key: %s, snode_key: %s", stake->snode_key.c_str, snode_key.c_str);

    LOKI_FOR_EACH(amount_index, stake->locked_amounts_len)
    {
      uint64_t amount = stake->locked_amounts[amount_index];
      EXPECT(result, amount == half_stake_requirement * LOKI_ATOMIC_UNITS, "Locked stake amount didn't match what we expected, amount: %zu, half_stake_requirement: %zu", amount, half_stake_requirement);
    }

    EXPECT(result, stakes.blacklisted_stakes_len == 0, "We haven't been blacklisted, so there should be no blacklisted stakes");
  }
  return result;
}

test_result latest__register_service_node__allow_4_stakers()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.load_latest_hardfork_versions();

  daemon_t daemon           = create_and_start_daemon(daemon_params);
  wallet_t  stakers[4]      = {};
  loki_addr stakers_addr[4] = {};

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;

  // Mine and unlock funds
  for (int i = 0; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
  {
    stakers[i]       = create_and_start_wallet(daemon_params.nettype, wallet_params);
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
  EXPECT(result, wallet_register_service_node(owner, registration_cmd.c_str), "Failed to register service node");
  wallet_mine_atleast_n_blocks(owner, 1, 100 /*mining_duration*/);

  // Each person stakes their part to the wallet
  for (int i = 1; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
  {
    wallet_t *staker = stakers + i;
    loki_contributor const *contributor = registration_params.contributors + i;
    LOKI_ASSERT(wallet_stake(staker, &snode_key, contributor->amount));
  }

  wallet_mine_atleast_n_blocks(owner, 1);
  daemon_snode_status node_status = daemon_print_sn_status(&daemon);
  EXPECT(result, node_status.registered, "Service node could not be registered");
  return result;
}

test_result latest__register_service_node__disallow_register_twice()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.load_latest_hardfork_versions();
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
    wallet_mine_until_height(&wallet, 100, LOKI_SECONDS_TO_MS(4)/*mining_duration_in_ms*/);
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
    daemon_snode_status node_status = daemon_print_sn_status(&daemon);
    EXPECT(result, node_status.registered, "Service node was not registered properly");
  }

  // Register twice, should be disallowed
  EXPECT(result, wallet_register_service_node(&wallet, registration_cmd.c_str) == false,
      "You should not be allowed to register twice, if detected on the network");

  return result;
}

test_result latest__request_stake_unlock__check_pooled_stake_unlocked()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon        = {};
  wallet_t wallets[4]    = {};
  wallet_t dummy_wallet  = {};
  LOKI_DEFER
  {
    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallets))
      wallet_exit(wallets + i);

    wallet_exit(&dummy_wallet);
    daemon_exit(&daemon);
  };

  // Start up daemon and wallet and mine until sufficient unlocked balances in each
  loki_addr wallet_addrs[LOKI_ARRAY_COUNT(wallets)] = {};
  loki_addr dummy_addr                              = {};
  {
    start_daemon_params daemon_params = {};
    daemon_params.load_latest_hardfork_versions();
    daemon                            = create_and_start_daemon(daemon_params);

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;

    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallets))
    {
      wallets[i] = create_and_start_wallet(daemon_params.nettype, wallet_params);
      EXPECT(result, wallet_address(wallets + i, 0, wallet_addrs + i), "Failed to query wallet: %d's primary address", (int)i);
      wallet_set_default_testing_settings(wallets + i);
    }

    dummy_wallet = create_and_start_wallet(daemon_params.nettype, wallet_params);
    wallet_set_default_testing_settings(&dummy_wallet);
    EXPECT(result, wallet_address(&dummy_wallet, 0, &dummy_addr),  "Failed to get the 0th subaddress, i.e the main address of wallet");

    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallets))
      wallet_mine_atleast_n_blocks(wallets + i, 5, 100);

    wallet_mine_until_height(wallets + 0, 100);
  }

  // Register the service node
  loki_snode_key snode_key = {};
  uint64_t staking_requirement_per_contributor = LOKI_FAKENET_STAKING_REQUIREMENT / LOKI_ARRAY_COUNT(wallet_addrs);
  {
    EXPECT(result, daemon_print_sn_key(&daemon, &snode_key), "Failed to get service node key, did daemon start in service node mode?");

    daemon_prepare_registration_params params = {};
    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallet_addrs))
    {
      params.contributors[params.num_contributors].addr     = wallet_addrs[i];
      params.contributors[params.num_contributors++].amount = staking_requirement_per_contributor;
    }

    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration (&daemon, &params, &registration_cmd), "Failed to prepare registration");
    EXPECT(result, wallet_register_service_node(wallets + 0, registration_cmd.c_str),    "Failed to register service node");
  }
  wallet_mine_atleast_n_blocks(&dummy_wallet, 5); // Get registration onto the chain

  for (int i = 1; i < (int)LOKI_ARRAY_COUNT(wallets); ++i)
  {
    wallet_refresh(wallets + i);
    EXPECT(result, wallet_stake(wallets + i, &snode_key, staking_requirement_per_contributor), "Wallet failed to stake");
  }
  wallet_mine_atleast_n_blocks(&dummy_wallet, 5); // Get stakes onto the chain

  // Check stakes show up as locked
  LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallet_addrs))
  {
    wallet_refresh(wallets + i);
    wallet_locked_stakes const locked_stakes = wallet_print_locked_stakes(wallets + i);
    EXPECT(result, locked_stakes.locked_stakes_len == 1, "We staked, so we should have locked key images");

    wallet_locked_stake const *stake = locked_stakes.locked_stakes + 0;
    EXPECT(result, stake->snode_key           == snode_key,                                               "Locked stake service node key didn't match what we expected, stake->snode_key: %s, snode_key: %s", stake->snode_key.c_str, snode_key.c_str);
    EXPECT(result, stake->total_locked_amount == staking_requirement_per_contributor * LOKI_ATOMIC_UNITS, "Total locked amount: %zu didn't match expected: %zu",                                              stake->total_locked_amount, LOKI_FAKENET_STAKING_REQUIREMENT);

    EXPECT(result, stake->locked_amounts_len == 1, "We only made one contribution, so there should only be one locked amount, locked_amounts_len: %d", stake->locked_amounts_len);
    uint64_t amount = stake->locked_amounts[0];
    EXPECT(result, amount == staking_requirement_per_contributor * LOKI_ATOMIC_UNITS,
        "Locked stake amount didn't match what we expected, amount: %zu, requirement: %zu",
        amount, staking_requirement_per_contributor * LOKI_ATOMIC_UNITS);
  }

  // One person unlocks, the remainder will automatically expire the service node
  uint64_t unlock_height = 0;
  EXPECT(result, wallet_request_stake_unlock(wallets + 0, &snode_key, &unlock_height), "Failed to request stake unlock");
  wallet_mine_atleast_n_blocks(&dummy_wallet, 1, 25); // Get unlocks

  {
    daemon_snode_status status = daemon_print_sn_status(&daemon);
    EXPECT(result, status.registered, "We mined too many blocks, the service node expired before we could check stake unlocking");
  }

  LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallet_addrs))
  {
    wallet_refresh(wallets + i);
    EXPECT(result, !wallet_request_stake_unlock(wallets + i, &snode_key, nullptr), "After 1 contributor has unlocked, all contributor key images will be unlocked automatically");
  }

  // Check no stakes show up as locked
  wallet_mine_until_height(&dummy_wallet, unlock_height + 30);
  LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallet_addrs))
  {
    wallet_refresh(wallets + i);
    wallet_locked_stakes const locked_stakes = wallet_print_locked_stakes(wallets + i);
    EXPECT(result, locked_stakes.locked_stakes_len      == 0, "Node shoud have expired, no stakes should be locked");
    EXPECT(result, locked_stakes.blacklisted_stakes_len == 0, "Node did not get deregistered, so we should not have any blacklisted key images");
  }

  // Sweep to dummy wallet
  LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallet_addrs))
    EXPECT(result, wallet_sweep_all(wallets + i, dummy_addr.buf.c_str, nullptr), "Failed to sweep all on wallet: %d", (int)i);

  // Check the remaining balance is zero
  wallet_mine_money_unlock_time_blocks(&dummy_wallet);
  LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallet_addrs))
  {
    wallet_refresh(wallets + i);
    uint64_t unlocked_balance = 0;
    uint64_t balance = wallet_balance(wallets + i, &unlocked_balance);
    EXPECT(result, unlocked_balance == 0, "After unlocking and expiring the service node. We sweep all our funds out to a dummy wallet, there should be no unlocked balance in this wallet, unlocked_balance: %zu", unlocked_balance);
    EXPECT(result, balance == 0, "After unlocking and expiring the service node. We sweep all our funds out to a dummy wallet, there should be no balance in this wallet, balance: %zu", balance);
  }

  return result;
}

test_result latest__request_stake_unlock__check_unlock_height()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon = {};
  wallet_t wallet = {}, dummy_wallet  = {};
  LOKI_DEFER { wallet_exit(&wallet); wallet_exit(&dummy_wallet); daemon_exit(&daemon); };

  // Start up daemon and wallet
  {
    start_daemon_params daemon_params = {};
    daemon_params.load_latest_hardfork_versions();
    daemon                            = create_and_start_daemon(daemon_params);

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    wallet                            = create_and_start_wallet(daemon_params.nettype, wallet_params);
    dummy_wallet                      = create_and_start_wallet(daemon_params.nettype, wallet_params);

    wallet_set_default_testing_settings(&wallet);
    wallet_set_default_testing_settings(&dummy_wallet);
  }

  wallet_mine_atleast_n_blocks(&wallet, 100, LOKI_SECONDS_TO_MS(4));

  loki_addr wallet_addr, dummy_addr = {};
  EXPECT(result, wallet_address(&wallet,       0, &wallet_addr), "Failed to get the 0th subaddress, i.e the main address of wallet");
  EXPECT(result, wallet_address(&dummy_wallet, 0, &dummy_addr),  "Failed to get the 0th subaddress, i.e the main address of wallet");

  // Register the service node
  loki_snode_key snode_key = {};
  {
    EXPECT(result, daemon_print_sn_key(&daemon, &snode_key), "Failed to get service node key, did daemon start in service node mode?");

    daemon_prepare_registration_params params             = {};
    params.contributors[params.num_contributors].addr     = wallet_addr;
    params.contributors[params.num_contributors++].amount = LOKI_FAKENET_STAKING_REQUIREMENT; // TODO(doyle): Assuming fakechain requirement of 100

    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration (&daemon, &params, &registration_cmd), "Failed to prepare registration");
    EXPECT(result, wallet_register_service_node(&wallet, registration_cmd.c_str),    "Failed to register service node");
  }

  wallet_mine_atleast_n_blocks(&wallet, 1); // Get registration onto the chain

  uint64_t unlock_height = 0;
  EXPECT(result, wallet_request_stake_unlock(&wallet, &snode_key, &unlock_height), "Failed to request stake unlock");
  uint64_t curr_height = wallet_status(&wallet);

  uint64_t expected_unlock_height = curr_height + (LOKI_STAKING_REQUIREMENT_LOCK_BLOCKS_FAKENET / 2);
  EXPECT(result, unlock_height == curr_height + (LOKI_STAKING_REQUIREMENT_LOCK_BLOCKS_FAKENET / 2), "The unlock height: %zu, should be estimated to be the curr height + half of the blacklisting period (aka. old staking period): %zu", unlock_height, expected_unlock_height);
  return result;
}

test_result latest__request_stake_unlock__disallow_request_on_non_existent_node()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  loki_snode_key snode_key = {};
  daemon_t daemon          = {};
  wallet_t wallet          = {};
  LOKI_DEFER { daemon_exit(&daemon); wallet_exit(&wallet); };

  // Setup wallet and daemon
  {
    start_daemon_params daemon_params = {};
    daemon_params.load_latest_hardfork_versions();
    daemon = create_and_start_daemon(daemon_params);
    LOKI_ASSERT(daemon_print_sn_key(&daemon, &snode_key));

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    wallet                            = create_and_start_wallet(daemon_params.nettype, wallet_params);
    wallet_set_default_testing_settings(&wallet);
  }

  EXPECT(result, wallet_request_stake_unlock(&wallet, &snode_key) == false, "We should be unable to request a stake unlock on a node that is not registered");
  return result;
}

test_result latest__request_stake_unlock__disallow_request_twice()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  loki_snode_key snode_key = {};
  daemon_t daemon          = {};
  wallet_t wallet          = {};
  LOKI_DEFER { daemon_exit(&daemon); wallet_exit(&wallet); };

  // Setup wallet and daemon
  {
    start_daemon_params daemon_params = {};
    daemon_params.load_latest_hardfork_versions();
    daemon = create_and_start_daemon(daemon_params);
    LOKI_ASSERT(daemon_print_sn_key(&daemon, &snode_key));

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    wallet                            = create_and_start_wallet(daemon_params.nettype, wallet_params);
    wallet_set_default_testing_settings(&wallet);
  }

  // Prepare blockchain, atleast 100 blocks so we have outputs to pick from
  int const FAKECHAIN_STAKING_REQUIREMENT = 100;
  {
    wallet_mine_atleast_n_blocks(&wallet, 100, LOKI_SECONDS_TO_MS(4));
    wallet_mine_until_unlocked_balance(&wallet, FAKECHAIN_STAKING_REQUIREMENT);
  }

  // Prepare registration, submit register and mine into the chain
  {
    daemon_prepare_registration_params params           = {};
    params.contributors[params.num_contributors].amount = FAKECHAIN_STAKING_REQUIREMENT;
    wallet_address(&wallet, 0, &params.contributors[params.num_contributors++].addr);

    loki_scratch_buf cmd = {};
    EXPECT(result, daemon_prepare_registration (&daemon, &params, &cmd), "Failed to prepare registration");
    EXPECT(result, wallet_register_service_node(&wallet, cmd.c_str),     "Failed to register service node");
    wallet_mine_atleast_n_blocks(&wallet, 10, 50/*mining_duration_in_ms*/); // Mine and become a service node
  }

  daemon_snode_status snode_status = daemon_print_sn_status(&daemon);
  EXPECT(result, snode_status.registered, "Node submitted registration but did not become a service node");

  EXPECT(result, wallet_request_stake_unlock(&wallet, &snode_key), "Failed to request our first stake unlock");
  EXPECT(result, wallet_request_stake_unlock(&wallet, &snode_key) == false, "We've already requested our stake to unlock, the second time should fail");
  return result;
}

test_result latest__service_node_checkpointing()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  int const NUM_DAEMONS = 10;
  loki_snode_key snode_keys[NUM_DAEMONS] = {};
  daemon_t daemons[NUM_DAEMONS]          = {};
  wallet_t wallet                        = {};
  LOKI_DEFER
  {
    wallet_exit(&wallet);
    LOKI_FOR_EACH(daemon_index, NUM_DAEMONS)
      daemon_exit(daemons + daemon_index);
  };

  // Start up daemon and wallet
  {
    start_daemon_params daemon_params[NUM_DAEMONS]  = {};
    LOKI_FOR_EACH(i, NUM_DAEMONS)
    {
      if (i == 0) daemon_params[i].keep_terminal_open = true;
      daemon_params[i].load_latest_hardfork_versions();
      daemon_params[i].custom_cmd_line = "";
    }

    create_and_start_multi_daemons(daemons, NUM_DAEMONS, daemon_params, NUM_DAEMONS);
    LOKI_FOR_EACH(daemon_index, NUM_DAEMONS)
      EXPECT(result, daemon_print_sn_key(daemons + daemon_index, snode_keys + daemon_index), "We should be able to query the service node key, was the daemon launched in --service-node mode?");

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = daemons + 0;
    wallet                            = create_and_start_wallet(daemon_params[0].nettype, wallet_params);
    wallet_set_default_testing_settings(&wallet);
  }

  loki_addr my_addr = {};
  wallet_mine_atleast_n_blocks(&wallet, 100, LOKI_SECONDS_TO_MS(4));
  EXPECT(result, wallet_address(&wallet, 0, &my_addr), "Failed to get the 0th subaddress, i.e the main address of wallet");

  // Register the service node
  LOKI_FOR_EACH(daemon_index, NUM_DAEMONS)
  {
    daemon_prepare_registration_params params             = {};
    params.contributors[params.num_contributors].addr     = my_addr;
    params.contributors[params.num_contributors++].amount = 100;

    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration (daemons + daemon_index, &params, &registration_cmd), "Failed to prepare registration");
    EXPECT(result, wallet_register_service_node(&wallet, registration_cmd.c_str),    "Failed to register service node");
  }

  // Mine registration and become service nodes and mine some blocks to create checkpoints
  wallet_mine_atleast_n_blocks(&wallet, 100, LOKI_SECONDS_TO_MS(4));
  daemon_status(daemons + 0);
  daemon_print_checkpoints(daemons + 0);

  for (;;)
  {
    itest_read_stdout_sink(&daemons[0].shared_mem, LOKI_SECONDS_TO_MS(8));
    daemon_print_checkpoints(daemons + 0);
  }

  return result;
}

test_result latest__stake__allow_incremental_stakes_with_1_contributor()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon = {};
  wallet_t wallet = {};
  LOKI_DEFER { wallet_exit(&wallet); daemon_exit(&daemon); };

  // Start up daemon and wallet
  loki_snode_key snode_key = {};
  {
    start_daemon_params daemon_params = {};
    daemon_params.load_latest_hardfork_versions();
    daemon                            = create_and_start_daemon(daemon_params);

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    wallet                            = create_and_start_wallet(daemon_params.nettype, wallet_params);
    wallet_set_default_testing_settings(&wallet);
    EXPECT(result, daemon_print_sn_key(&daemon, &snode_key), "We should be able to query the service node key, was the daemon launched in --service-node mode?");
  }

  loki_addr my_addr = {};
  wallet_mine_atleast_n_blocks(&wallet, 100, LOKI_SECONDS_TO_MS(4));
  EXPECT(result, wallet_address(&wallet, 0, &my_addr), "Failed to get the 0th subaddress, i.e the main address of wallet");

  // Register the service node
  {
    daemon_prepare_registration_params params             = {};
    params.open_pool                                      = true;
    params.contributors[params.num_contributors].addr     = my_addr;
    params.contributors[params.num_contributors++].amount = 25;

    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration (&daemon, &params, &registration_cmd), "Failed to prepare registration");
    EXPECT(result, wallet_register_service_node(&wallet, registration_cmd.c_str),    "Failed to register service node");
    wallet_mine_atleast_n_blocks(&wallet, 1);
  }

  // Contributor 1, sends in, 35, 20, 25
  {
    wallet_refresh(&wallet);
    EXPECT(result, wallet_stake(&wallet, &snode_key, 35), "Wallet failed to stake amount: 35");
    wallet_mine_atleast_n_blocks(&wallet, 1);

    wallet_refresh(&wallet);
    EXPECT(result, wallet_stake(&wallet, &snode_key, 20), "Wallet failed to stake amount: 20");
    wallet_mine_atleast_n_blocks(&wallet, 1);

    wallet_refresh(&wallet);
    EXPECT(result, wallet_stake(&wallet, &snode_key, 25), "Wallet failed to stake amount: 25");
    wallet_mine_atleast_n_blocks(&wallet, 1);
  }

  daemon_snode_status status = daemon_print_sn_status(&daemon);
  EXPECT(result, status.registered, "Service node was not successfully registered");
  return result;
}

test_result latest__stake__check_incremental_stakes_decreasing_min_contribution()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon        = {};
  wallet_t wallets[4]    = {};
  wallet_t dummy_wallet  = {};
  LOKI_DEFER
  {
    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallets))
      wallet_exit(wallets + i);

    wallet_exit(&dummy_wallet);
    daemon_exit(&daemon);
  };

  // Start up daemon and wallet and mine until sufficient unlocked balances in each
  loki_addr wallet_addrs[LOKI_ARRAY_COUNT(wallets)] = {};
  loki_addr dummy_addr                              = {};
  {
    start_daemon_params daemon_params = {};
    daemon_params.load_latest_hardfork_versions();
    daemon                            = create_and_start_daemon(daemon_params);

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;

    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallets))
    {
      wallets[i] = create_and_start_wallet(daemon_params.nettype, wallet_params);
      EXPECT(result, wallet_address(wallets + i, 0, wallet_addrs + i), "Failed to query wallet: %d's primary address", (int)i);
      wallet_set_default_testing_settings(wallets + i);
    }

    dummy_wallet = create_and_start_wallet(daemon_params.nettype, wallet_params);
    wallet_set_default_testing_settings(&dummy_wallet);
    EXPECT(result, wallet_address(&dummy_wallet, 0, &dummy_addr),  "Failed to get the 0th subaddress, i.e the main address of wallet");

    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallets))
      wallet_mine_atleast_n_blocks(wallets + i, 5, 100);

    wallet_mine_until_height(wallets + 0, 100);
  }

  // Register the service node
  loki_snode_key snode_key = {};
  {
    EXPECT(result, daemon_print_sn_key(&daemon, &snode_key), "Failed to get service node key, did daemon start in service node mode?");
    daemon_prepare_registration_params params             = {};
    params.open_pool                                      = true;
    params.contributors[params.num_contributors].addr     = wallet_addrs[0];
    params.contributors[params.num_contributors++].amount = 50;

    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration (&daemon, &params, &registration_cmd), "Failed to prepare registration");
    EXPECT(result, wallet_register_service_node(wallets + 0, registration_cmd.c_str),    "Failed to register service node");
  }
  wallet_mine_atleast_n_blocks(&dummy_wallet, 5); // Get registration onto the chain

  // Wallet 1 contributes 40, the minimum contribution should be (50 loki remaining/ 3 key images remaining)
  {
    wallet_refresh(wallets + 1);
    EXPECT(result, wallet_stake(wallets + 1, &snode_key, 40), "Wallet failed to stake");
    wallet_mine_atleast_n_blocks(&dummy_wallet, 5);
  }

  // Wallet 2 contributes 8, the minimum contribution should be (10 loki remaining/ 2 key images remaining)
  {
    wallet_refresh(wallets + 2);
    EXPECT(result, wallet_stake(wallets + 2, &snode_key, 8), "Wallet failed to stake");
    wallet_mine_atleast_n_blocks(&dummy_wallet, 5);
  }

  // Wallet 3 tries to contribute 1 loki, but fails, the minimum contribution should be (2 loki remaining/ 1 key images remaining)
  {
    wallet_refresh(wallets + 3);
    EXPECT(result, !wallet_stake(wallets + 3, &snode_key, 1), "Wallet should not be able to stake below the min contribution amount which should be 2 loki");
  }

  // Wallet 3 contributes 2 loki, the minimum contribution should be (2 loki remaining/ 1 key images remaining)
  {
    wallet_refresh(wallets + 3);
    EXPECT(result, wallet_stake(wallets + 3, &snode_key, 2), "Wallet failed to stake");
    wallet_mine_atleast_n_blocks(&dummy_wallet, 5);
  }

  // Check stakes show up as locked
  LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallet_addrs))
  {
    wallet_refresh(wallets + i);
    wallet_locked_stakes const locked_stakes = wallet_print_locked_stakes(wallets + i);
    EXPECT(result, locked_stakes.locked_stakes_len == 1, "We staked, so we should have locked key images");

    wallet_locked_stake const *stake = locked_stakes.locked_stakes + 0;
    EXPECT(result, stake->snode_key           == snode_key, "Locked stake service node key didn't match what we expected, stake->snode_key: %s, snode_key: %s", stake->snode_key.c_str, snode_key.c_str);
    EXPECT(result, stake->locked_amounts_len  == 1,         "We only made one contribution, so there should only be one locked amount, locked_amounts_len: %d", stake->locked_amounts_len);

    uint64_t amount = stake->locked_amounts[0];
    uint64_t expected_amount = 0;
    if      (i == 0) expected_amount = 50;
    else if (i == 1) expected_amount = 40;
    else if (i == 2) expected_amount = 8;
    else             expected_amount = 2;

    EXPECT(result, stake->total_locked_amount == expected_amount * LOKI_ATOMIC_UNITS, "Total locked amount: %zu didn't match expected: %zu", stake->total_locked_amount, expected_amount * LOKI_ATOMIC_UNITS);
    EXPECT(result, amount == expected_amount * LOKI_ATOMIC_UNITS,
        "Locked stake amount didn't match what we expected, amount: %zu, requirement: %zu",
        amount, expected_amount * LOKI_ATOMIC_UNITS);
  }

  // One person unlocks, the remainder will automatically expire the service node
  uint64_t unlock_height = 0;
  EXPECT(result, wallet_request_stake_unlock(wallets + 0, &snode_key, &unlock_height), "Failed to request stake unlock");
  wallet_mine_atleast_n_blocks(&dummy_wallet, 1, 25); // Get unlocks

  {
    daemon_snode_status status = daemon_print_sn_status(&daemon);
    EXPECT(result, status.registered, "We mined too many blocks, the service node expired before we could check stake unlocking");
  }

  LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallet_addrs))
  {
    wallet_refresh(wallets + i);
    EXPECT(result, !wallet_request_stake_unlock(wallets + i, &snode_key, nullptr), "After 1 contributor has unlocked, all contributor key images will be unlocked automatically");
  }

  // Check no stakes show up as locked
  wallet_mine_until_height(&dummy_wallet, unlock_height + 30);
  LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallet_addrs))
  {
    wallet_refresh(wallets + i);
    wallet_locked_stakes const locked_stakes = wallet_print_locked_stakes(wallets + i);
    EXPECT(result, locked_stakes.locked_stakes_len      == 0, "Node shoud have expired, no stakes should be locked");
    EXPECT(result, locked_stakes.blacklisted_stakes_len == 0, "Node did not get deregistered, so we should not have any blacklisted key images");
  }

  // Sweep to dummy wallet
  LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallet_addrs))
    EXPECT(result, wallet_sweep_all(wallets + i, dummy_addr.buf.c_str, nullptr), "Failed to sweep all on wallet: %d", (int)i);

  // Check the remaining balance is zero
  wallet_mine_money_unlock_time_blocks(&dummy_wallet);
  LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallet_addrs))
  {
    wallet_refresh(wallets + i);
    uint64_t unlocked_balance = 0;
    uint64_t balance = wallet_balance(wallets + i, &unlocked_balance);
    EXPECT(result, unlocked_balance == 0, "After unlocking and expiring the service node. We sweep all our funds out to a dummy wallet, there should be no unlocked balance in this wallet, unlocked_balance: %zu", unlocked_balance);
    EXPECT(result, balance == 0, "After unlocking and expiring the service node. We sweep all our funds out to a dummy wallet, there should be no balance in this wallet, balance: %zu", balance);
  }

  return result;
}

test_result latest__stake__check_transfer_doesnt_used_locked_key_images()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon = {};
  wallet_t wallet = {};
  LOKI_DEFER { wallet_exit(&wallet); daemon_exit(&daemon); };

  // Start up daemon and wallet
  {
    start_daemon_params daemon_params = {};
    daemon_params.load_latest_hardfork_versions();
    daemon                            = create_and_start_daemon(daemon_params);

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    wallet                            = create_and_start_wallet(daemon_params.nettype, wallet_params);
    wallet_set_default_testing_settings(&wallet);
  }

  loki_addr my_addr = {};
  wallet_mine_atleast_n_blocks(&wallet, 100, LOKI_SECONDS_TO_MS(4));
  EXPECT(result, wallet_address(&wallet, 0, &my_addr), "Failed to get the 0th subaddress, i.e the main address of wallet");

  // Register the service node
  {
    daemon_prepare_registration_params params             = {};
    params.contributors[params.num_contributors].addr     = my_addr;
    params.contributors[params.num_contributors++].amount = LOKI_FAKENET_STAKING_REQUIREMENT; // TODO(doyle): Assuming fakechain requirement of 100

    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration (&daemon, &params, &registration_cmd), "Failed to prepare registration");
    EXPECT(result, wallet_register_service_node(&wallet, registration_cmd.c_str),    "Failed to register service node");
  }

  wallet_mine_atleast_n_blocks(&wallet, 1); // Get registration onto the chain
  EXPECT(result, wallet_sweep_all(&wallet, my_addr.buf.c_str, nullptr), "Sweeping all outputs should avoid any key images/outputs that are locked and so should succeed.");
  return result;
}

test_result latest__stake__disallow_staking_less_than_minimum_in_pooled_node()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.load_latest_hardfork_versions();

  daemon_t daemon           = create_and_start_daemon(daemon_params);
  wallet_t  stakers[2]      = {};
  loki_addr stakers_addr[2] = {};

  // Mine and unlock funds
  for (int i = 0; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
  {
    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    stakers[i]                        = create_and_start_wallet(daemon_params.nettype, wallet_params);
    wallet_t *staker                  = stakers + i;

    wallet_set_default_testing_settings(staker);
    wallet_address(staker, 0, stakers_addr + i);
    wallet_mine_atleast_n_blocks(staker, 10);
  }

  LOKI_DEFER
  {
    daemon_exit(&daemon);
    for (int i = 0; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
      wallet_exit(stakers + i);
  };

  wallet_t *owner = stakers + 0;
  wallet_mine_until_height(owner, 100);

  loki_snode_key snode_key = {};
  daemon_print_sn_key(&daemon, &snode_key);

  // Register the service node and put it on the chain
  uint64_t half_stake_requirement = LOKI_FAKENET_STAKING_REQUIREMENT / 2;
  daemon_prepare_registration_params registration_params = {};
  {
    registration_params.num_contributors = (int)LOKI_ARRAY_COUNT(stakers);
    for (int i = 0; i < registration_params.num_contributors; ++i)
    {
      registration_params.contributors[i].addr   = stakers_addr[i];
      registration_params.contributors[i].amount = half_stake_requirement; // TODO(doyle): Assumes testnet staking requirement of 100
    }

    loki_scratch_buf registration_cmd = {};
    daemon_prepare_registration (&daemon, &registration_params, &registration_cmd);
    EXPECT(result, wallet_register_service_node(owner, registration_cmd.c_str), "Failed to register service node");
    wallet_mine_atleast_n_blocks(owner, 1, 100 /*mining_duration*/);
  }

  uint64_t num_locked_contributions                = 1;
  uint64_t min_node_contribution                   = half_stake_requirement / (LOKI_MAX_LOCKED_KEY_IMAGES - num_locked_contributions);
  uint64_t amount_reserved_but_not_contributed_yet = registration_params.contributors[1].amount;

  wallet_refresh(stakers + 1);
  EXPECT(result, !wallet_stake(stakers + 1, &snode_key, 1), "Reserved contributor, but, cannot stake lower than the reserved amount/min node amount, which is MAX((staking_requirement/(4 max_key_images)), reserved_amount)");
  EXPECT(result, !wallet_stake(stakers + 1, &snode_key, min_node_contribution), "Reserved contributor, but, cannot stake lower than the reserved amount/min node amount, which is MAX((staking_requirement/(4 max_key_images)), reserved_amount)");
  EXPECT(result, wallet_stake(stakers + 1, &snode_key, amount_reserved_but_not_contributed_yet), "Reserved contributor failed to stake the correct amount");

  wallet_mine_atleast_n_blocks(stakers + 1, 1);
  daemon_snode_status snode_status = daemon_print_sn_status(&daemon);
  EXPECT(result, snode_status.registered, "Service node failed to become registered");
  return result;
}

test_result latest__stake__disallow_staking_when_all_amounts_reserved()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.load_latest_hardfork_versions();

  daemon_t daemon           = create_and_start_daemon(daemon_params);
  wallet_t  stakers[2]      = {};
  loki_addr stakers_addr[2] = {};

  // Mine and unlock funds
  for (int i = 0; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
  {
    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    stakers[i]                        = create_and_start_wallet(daemon_params.nettype, wallet_params);
    wallet_t *staker                  = stakers + i;

    wallet_set_default_testing_settings(staker);
    wallet_address(staker, 0, stakers_addr + i);
    wallet_mine_atleast_n_blocks(staker, 10);
  }

  LOKI_DEFER
  {
    daemon_exit(&daemon);
    for (int i = 0; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
      wallet_exit(stakers + i);
  };

  wallet_t *owner = stakers + 0;
  wallet_mine_until_height(owner, 100);

  loki_snode_key snode_key = {};
  daemon_print_sn_key(&daemon, &snode_key);

  // Register the service node and put it on the chain
  {
    daemon_prepare_registration_params registration_params = {};
    registration_params.num_contributors = (int)LOKI_ARRAY_COUNT(stakers);
    for (int i = 0; i < registration_params.num_contributors; ++i)
    {
      registration_params.contributors[i].addr   = stakers_addr[i];
      registration_params.contributors[i].amount = LOKI_FAKENET_STAKING_REQUIREMENT/registration_params.num_contributors; // TODO(doyle): Assumes testnet staking requirement of 100
    }

    loki_scratch_buf registration_cmd = {};
    daemon_prepare_registration (&daemon, &registration_params, &registration_cmd);
    EXPECT(result, wallet_register_service_node(owner, registration_cmd.c_str), "Failed to register service node");
    wallet_mine_atleast_n_blocks(owner, 1, 100 /*mining_duration*/);
  }

  EXPECT(result, !wallet_stake(owner, &snode_key, 50), "The node is completely reserved, the owner should not be able to contribute more into the node.");
  return result;
}

test_result latest__stake__disallow_to_non_registered_node()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.load_latest_hardfork_versions();
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
  EXPECT(result, wallet_stake(&wallet, &snode_key, 15) == false, "You should not be able to stake to a node that is not registered yet!");

  return result;
}

test_result latest__transfer__check_fee_amount_bulletproofs()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.load_latest_hardfork_versions();
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
    loki_transaction tx = {};
    EXPECT(result, wallet_transfer(&src_wallet, dest_addr.buf.c_str, 50, &tx), "Failed to construct TX");
    wallet_mine_atleast_n_blocks(&src_wallet, 30);

    int64_t fee                = static_cast<int64_t>(tx.fee);
    int64_t delta              = LOKI_ABS(fee_estimate - fee);
    EXPECT(result, delta < epsilon, "Unexpected difference in fee estimate: %zd, fee: %zd, with delta: %zd >= epsilon: %zd", fee_estimate, fee, delta, epsilon);
  }

  // Transfer from dest to wallet
  {
    wallet_refresh(&dest_wallet);
    loki_transaction tx = {};
    EXPECT(result, wallet_transfer(&dest_wallet, src_addr.buf.c_str, 45, &tx), "Failed to construct TX");

    int64_t fee   = static_cast<int64_t>(tx.fee);
    int64_t delta = LOKI_ABS(fee_estimate - fee);
    EXPECT(result, delta < epsilon, "Unexpected difference in fee estimate: %zd, fee: %zd, with delta: %zd >= epsilon: %zd", fee_estimate, fee, delta, epsilon);
  }

  return result;
}

//
// V10 Tests
//

test_result v10__register_service_node__check_gets_payed_expires_and_returns_funds()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.add_sequential_hardforks_until_version(10);
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
  wallet_transfer(&master_wallet, staker1_addr.buf.c_str, 50 + 1, nullptr);
  wallet_transfer(&master_wallet, staker2_addr.buf.c_str, 50 + 1, nullptr);
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
    EXPECT(result, wallet_stake(&staker2, &snode_key, 50), "Staker 2 failed to stake to service node");
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

test_result v10__register_service_node__check_grace_period()
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
    wallet_sweep_all(&wallet, my_addr.buf.c_str, nullptr);
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
    daemon_snode_status node_status = daemon_print_sn_status(&daemon);
    EXPECT(result, node_status.registered, "Service node was not registered properly");
  }

  // TODO(doyle): Assumes fakechain
  // Mine until within the grace re-registration range
  {
    int const STAKING_DURATION = 30; // TODO(doyle): Workaround for inability to print_sn_status and see the actual expiry
    daemon_status_t status     = daemon_status(&daemon);
    wallet_mine_atleast_n_blocks(&wallet, STAKING_DURATION, 10/*mining_duration_in_ms*/);

    uint64_t wallet_height = wallet_status(&wallet);
    EXPECT(result, wallet_height < status.height + STAKING_DURATION,
        "We mined too many blocks! We need to mine within %d blocks of expiry and re-register within that time frame to test the grace period. The current wallet height: %d, node expired at height: %d",
        LOKI_STAKING_EXCESS_BLOCKS, wallet_height, status.height + STAKING_DURATION);

    daemon_snode_status node_status = daemon_print_sn_status(&daemon);
    EXPECT(result, node_status.registered, "We should still be registered, i.e. within the re-register grace period");
  }

  // Re-register within this grace period, re-use the registration params
  {
    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration(&daemon, &registration_params, &registration_cmd), "Failed to prepare registration for Service Node");
    EXPECT(result, wallet_register_service_node(&wallet, registration_cmd.c_str), "Failed to reregister the service node");
    wallet_mine_atleast_n_blocks(&wallet, 1, 100 /*mining_duration_in_ms*/);

    // TODO(doyle): We should check the registration height has been "refreshed"
    daemon_snode_status node_status = daemon_print_sn_status(&daemon);
    EXPECT(result, node_status.registered, "Service node was not re-registered properly");
  }

  return result;
}

test_result v10__stake__allow_incremental_staking_until_node_active()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.add_sequential_hardforks_until_version(10);
  daemon_t daemon                   = create_and_start_daemon(daemon_params);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;
  wallet_t wallet                   = create_and_start_wallet(daemon_params.nettype, wallet_params);

  wallet_set_default_testing_settings(&wallet);
  LOKI_DEFER { wallet_exit(&wallet); daemon_exit(&daemon); };

  loki_addr my_addr = {};
  wallet_mine_atleast_n_blocks(&wallet, 100, LOKI_SECONDS_TO_MS(4));
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
  EXPECT(result, wallet_stake(&wallet, &snode_key, 10) == false, "We should not be able to stake for service node: %s just yet, it should still be sitting in the mempool.", snode_key.c_str);
  wallet_mine_atleast_n_blocks(&wallet, 5, 100 /*mining_duration_in_ms*/);      // Get registration onto chain

  EXPECT(result, wallet_stake(&wallet, &snode_key, 15), "Failed to stake 15 loki to node: %s", snode_key.c_str);
  wallet_mine_atleast_n_blocks(&wallet, 5, 100 /*mining_duration_in_ms*/);      // Get registration onto chain
  EXPECT(result, wallet_stake(&wallet, &snode_key, 15), "Failed to stake 15 loki to node: %s", snode_key.c_str);
  wallet_mine_atleast_n_blocks(&wallet, 5, 100 /*mining_duration_in_ms*/);      // Get registration onto chain
  EXPECT(result, wallet_stake(&wallet, &snode_key, 20), "Failed to stake 20 loki to node: %s", snode_key.c_str);
  wallet_mine_atleast_n_blocks(&wallet, 5, 100 /*mining_duration_in_ms*/);      // Get registration onto chain

  EXPECT(result, wallet_stake(&wallet, &snode_key, 20) == false, "The node: %s should already be registered, staking further should be disallowed!", snode_key.c_str);
  return result;
}

test_result v10__stake__allow_insufficient_stake_w_reserved_contributor()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);
  start_daemon_params daemon_params = {};
  daemon_params.add_sequential_hardforks_until_version(10);
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
    wallet_mine_atleast_n_blocks(&wallet1, 1);

    daemon_snode_status node_status = daemon_print_sn_status(&daemon);
    EXPECT(result, node_status.known_on_the_network, "Service node registration didn't make it into the blockchain?");
  }

  loki_snode_key snode_key = {};
  EXPECT(result, daemon_print_sn_key(&daemon, &snode_key), "Failed to print service node key");

  // Wallet 2 try to stake with insufficient balance, should be allowed if they have reserved a spot
  wallet_refresh(&wallet2);
  EXPECT(result, wallet_stake(&wallet2, &snode_key, 10),
      "A service node registration with a reserved contributor should allow staking in insufficient parts.");

  // TODO(doyle): You should mine, then parse the results of print_sn_status to make sure the 10 loki was accepted
  // Right now this only checks that the wallet allows you to, it will warn you, that you need to contribute more
  // but it will still let you and it should update the registration status with the 10 loki.
  return result;
}

test_result v10__stake__disallow_insufficient_stake_w_not_reserved_contributor()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);
  start_daemon_params daemon_params = {};
  daemon_params.add_sequential_hardforks_until_version(10);
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

    EXPECT(result, wallet_register_service_node(&owner, registration_cmd.c_str), "Service node failed to register");
    wallet_mine_atleast_n_blocks(&owner, 1, 100 /*mining_duration_in_ms*/);

    daemon_snode_status node_status = daemon_print_sn_status(&daemon);
    EXPECT(result, node_status.known_on_the_network, "Registration was submitted but not mined into the blockchain?");
  }

  loki_snode_key snode_key = {};
  EXPECT(result, daemon_print_sn_key(&daemon, &snode_key), "Failed to print service node key");

  // Wallet 2 try to stake with insufficient balance
  {
    wallet_set_default_testing_settings(&contributor);

    loki_addr contributor_addr = {};
    wallet_address(&contributor, 0, &contributor_addr);
    wallet_mine_until_unlocked_balance(&contributor, 20, LOKI_SECONDS_TO_MS(1)/*mining_duration_in_ms*/);

    EXPECT(result, wallet_stake(&contributor, &snode_key, 10) == false,
        "An open service node registration should disallow an insufficient stake, otherwise we lock up the funds for no reason.");
  }

  return result;
}

//
// V9 Tests
//

test_result v09__transfer__check_fee_amount()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.add_sequential_hardforks_until_version(9);
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
    loki_transaction tx = {};
    EXPECT(result, wallet_transfer(&src_wallet, dest_addr.buf.c_str, 50, &tx), "Failed to construct TX");
    wallet_mine_atleast_n_blocks(&src_wallet, 30);

    int64_t fee                = static_cast<int64_t>(tx.fee);
    int64_t delta              = LOKI_ABS(fee_estimate - fee);
    EXPECT(result, delta < epsilon, "Unexpected difference in fee estimate: %zd, fee: %zd, with delta: %zd >= epsilon: %zd", fee_estimate, fee, delta, epsilon);
  }

  // Transfer from dest to wallet
  {
    wallet_refresh(&dest_wallet);
    loki_transaction tx = {};
    EXPECT(result, wallet_transfer(&dest_wallet, src_addr.buf.c_str, 45, &tx), "Failed to construct TX");

    int64_t fee   = static_cast<int64_t>(tx.fee);
    int64_t delta = LOKI_ABS(fee_estimate - fee);
    EXPECT(result, delta < epsilon, "Unexpected difference in fee estimate: %zd, fee: %zd, with delta: %zd >= epsilon: %zd", fee_estimate, fee, delta, epsilon);
  }

  return result;
}

