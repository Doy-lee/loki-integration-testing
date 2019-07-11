#include "loki_test_cases.h"

#define LOKI_WALLET_IMPLEMENTATION
#include "loki_wallet.h"

#define LOKI_STR_IMPLEMENTATION
#include "loki_str.h"

#define LOKI_DAEMON_IMPLEMENTATION
#include "loki_daemon.h"

#define EXPECT_STR(test_result_var, src, EXPECT_str, fmt, ...) \
if (!str_match(src, EXPECT_str)) \
{ \
  test_result_var.failed   = true; \
  test_result_var.fail_msg = loki_scratch_buf("[%s != %s] " fmt, src, EXPECT_str, ## __VA_ARGS__); \
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

static bool compare_checkpoints(daemon_t *daemons, int num_daemons)
{
  if (num_daemons <= 1)
    return true;

  std::vector<daemon_checkpoint> reference_checkpoints = daemon_print_checkpoints(daemons + 0);
  for (int daemon_index = 1; daemon_index < num_daemons; ++daemon_index)
  {
    std::vector<daemon_checkpoint> checkpoints = daemon_print_checkpoints(daemons + daemon_index);
    if (reference_checkpoints != checkpoints) return false;
  }

  return true;
}

void print_test_results(test_result const *test)
{
  int const TARGET_LEN = 76;
  char const *STATUS   = (test->failed) ? "FAILED" : "OK";


  loki_scratch_buf buf("%s ", test->name.c_str);
  int total_len           = test->name.len + strlen(STATUS);
  int const remaining_len = LOKI_MAX(TARGET_LEN - total_len, 0);
  for (int i = 0; i < remaining_len; ++i) buf.append(".");

  if (test->failed) buf.append(LOKI_ANSI_COLOR_RED);
  else              buf.append(LOKI_ANSI_COLOR_GREEN);

  buf.append("%s (%05.2fs)" LOKI_ANSI_COLOR_RESET "\n", STATUS, test->duration_ms);

  if (test->failed) buf.append("  Message: %s\n\n", test->fail_msg.c_str);
  fprintf(stdout, "%s", buf.c_str);
}

struct loki_err_context
{
  bool              success;
  loki_buffer<1024> err_msg;
  operator bool() { return success; }
};

// TODO(doyle): Deprecate and remove all helpers except the flexible one.
//
// NOTE: Helpers
//
void helper_setup_blockchain_with_n_blocks(test_result const *context, daemon_t *daemon, wallet_t *wallet, loki_addr *addr, uint64_t num_blocks)
{
  start_daemon_params daemon_params = {};
  daemon_params.load_latest_hardfork_versions();
  *daemon                           = create_and_start_daemon(daemon_params, context->name.c_str);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = daemon;
  *wallet                           = create_and_start_wallet(daemon_params.nettype, wallet_params, context->name.c_str);
  wallet_set_default_testing_settings(wallet);
  daemon_mine_n_blocks(daemon, wallet, num_blocks);
  wallet_address(wallet, 0, addr);
}

test_result helper_setup_blockchain_with_1_service_node(test_result const *context, daemon_t *daemon, wallet_t *wallet, loki_addr *addr, loki_snode_key *snode_key)
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  helper_setup_blockchain_with_n_blocks(context, daemon, wallet, addr, MIN_BLOCKS_IN_BLOCKCHAIN);
  EXPECT(result, daemon_print_sn_key(daemon, snode_key), "Failed to get service node key, did daemon start in service node mode?");

  {
    daemon_prepare_registration_params registration_params = {};
    registration_params.num_contributors                   = 1;
    registration_params.contributors[0].addr               = *addr;
    registration_params.contributors[0].amount             = 100; // TODO(doyle): Assuming testnet

    loki_scratch_buf registration_cmd = {}; // Register the service node
    EXPECT(result, daemon_prepare_registration(daemon, &registration_params, &registration_cmd), "Failed to prepare registration for Service Node");
    wallet_register_service_node(wallet, registration_cmd.c_str);
    daemon_mine_n_blocks(daemon, wallet, 1);

    // Check service node registered
    daemon_snode_status node_status = daemon_print_sn_status(daemon);
    EXPECT(result, node_status.registered, "Service node was not registered properly");
  }

  return result;
}

void helper_block_until_blockchains_are_synced(daemon_t *daemons, int num_daemons)
{
  if (num_daemons < 0)
    return;

  daemon_status_t target_status = {};
  ptrdiff_t target_index = 0;
  LOKI_FOR_EACH(i, num_daemons)
  {
    daemon_t *check_daemon      = daemons + i;
    daemon_status_t check_status = daemon_status(check_daemon);
    if (check_status.height > target_status.height)
    {
      target_index  = i;
      target_status = check_status;
    }
  }

  LOKI_FOR_EACH(i, num_daemons)
  {
    if (i == target_index) continue;
    daemon_t *check_daemon       = daemons + i;
    daemon_status_t check_status = daemon_status(check_daemon);

    int wait_time = 1000;
    while (check_status.height != target_status.height)
    {
      os_sleep_ms(LOKI_MIN(wait_time, 2000));
      check_status = daemon_status(check_daemon);
      // wait_time *= 1.25f;
    }
  }
}

void helper_cleanup_blockchain_environment(helper_blockchain_environment *environment)
{
  for (daemon_t &daemon : environment->all_daemons)
    daemon_exit(&daemon);

  for (wallet_t &wallet : environment->wallets)
    wallet_exit(&wallet);
}

bool helper_setup_blockchain(helper_blockchain_environment *environment,
                             test_result const *context,
                             start_daemon_params daemon_param,
                             int num_service_nodes,
                             int num_daemons,
                             int num_wallets,
                             int wallet_balance)
{
  assert(num_service_nodes + num_daemons > 0);
  assert(num_wallets > 0);
  environment->all_daemons.resize(num_service_nodes + num_daemons);
  environment->daemon_param = daemon_param;

  if (num_service_nodes)
  {
    environment->service_nodes     = environment->all_daemons.data();
    environment->num_service_nodes = num_service_nodes;
    environment->snode_keys.resize(num_service_nodes);
  }

  if (num_daemons)
  {
    environment->daemons     = environment->all_daemons.data() + num_service_nodes;
    environment->num_daemons = num_daemons;
  }

  daemon_t *all_daemons = environment->all_daemons.data();
  // daemon_param.keep_terminal_open = true;
  int total_daemons     = num_service_nodes + num_daemons;
  {
    create_and_start_multi_daemons(all_daemons, total_daemons, &daemon_param, 1, context->name.c_str);
    LOKI_FOR_EACH(daemon_index, num_service_nodes)
    {
      if (!daemon_print_sn_key(environment->service_nodes + daemon_index, &environment->snode_keys[daemon_index]))
        return false;
    }
  }

  environment->wallets.resize(num_wallets);
  environment->wallets_addr.resize(num_wallets);
  LOKI_FOR_EACH (wallet_index, num_wallets)
  {
    wallet_t *wallet       = &environment->wallets[wallet_index];
    loki_addr *wallet_addr = &environment->wallets_addr[wallet_index];

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = all_daemons + 0;
    *wallet = create_and_start_wallet(daemon_param.nettype, wallet_params, context->name.c_str);
    wallet_set_default_testing_settings(wallet);

    if (!wallet_address(wallet, 0, wallet_addr))
      return false;

    wallet_mine_until_unlocked_balance(wallet, all_daemons + 0, wallet_balance);
  }

  // Mine the initial blocks in the blockchain
  {
    int const BLOCKS_TO_BATCH_MINE = 4;
    for (size_t i = 0; i < MIN_BLOCKS_IN_BLOCKCHAIN / BLOCKS_TO_BATCH_MINE; i++)
    {
      daemon_mine_n_blocks(all_daemons + 0, &environment->wallets[0], BLOCKS_TO_BATCH_MINE);
      LOKI_FOR_EACH(daemon_index, total_daemons)
      {
        daemon_t *daemon = all_daemons + daemon_index;
        daemon_relay_votes_and_uptime(daemon);
      }
      helper_block_until_blockchains_are_synced(all_daemons, total_daemons);
    }

    for (size_t i = 0; i < MIN_BLOCKS_IN_BLOCKCHAIN % BLOCKS_TO_BATCH_MINE; i++)
    {
      daemon_mine_n_blocks(all_daemons + 0, &environment->wallets[0], 1);
      LOKI_FOR_EACH(daemon_index, total_daemons)
      {
        daemon_t *daemon = all_daemons + daemon_index;
        daemon_relay_votes_and_uptime(daemon);
      }
      helper_block_until_blockchains_are_synced(all_daemons, total_daemons);
    }
  }

  // Register the service node
  LOKI_FOR_EACH(daemon_index, environment->num_service_nodes)
  {
    daemon_prepare_registration_params params             = {};
    params.contributors[params.num_contributors].addr     = environment->wallets_addr[0];
    params.contributors[params.num_contributors++].amount = 100;

    loki_scratch_buf registration_cmd = {};
    if (!daemon_prepare_registration(&environment->service_nodes[daemon_index], &params, &registration_cmd) ||
        !wallet_register_service_node(&environment->wallets[0], registration_cmd.c_str))
    {
      return false;
    }
  }

  daemon_mine_n_blocks(all_daemons + 0, &environment->wallets[0], 1);
  helper_block_until_blockchains_are_synced(all_daemons, total_daemons);
  return true;
}

bool helper_setup_blockchain_with_n_service_nodes(test_result const *context,
                                                  daemon_t *daemons,
                                                  loki_snode_key *snode_keys,
                                                  start_daemon_params *daemon_params,
                                                  int num_daemons,
                                                  wallet_t *wallet,
                                                  loki_addr *addr)
{
  // Start up daemon and wallet
  {
    create_and_start_multi_daemons(daemons, num_daemons, daemon_params, num_daemons, context->name.c_str);
    LOKI_FOR_EACH(daemon_index, num_daemons)
    {
      if (!daemon_print_sn_key(daemons + daemon_index, snode_keys + daemon_index))
        return false;
    }

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = daemons + 0;
    *wallet = create_and_start_wallet(daemon_params[0].nettype, wallet_params, context->name.c_str);
    wallet_set_default_testing_settings(wallet);
  }

  if (!wallet_address(wallet, 0, addr))
    return false;

  daemon_mine_n_blocks(daemons + 0, wallet, MIN_BLOCKS_IN_BLOCKCHAIN);
  helper_block_until_blockchains_are_synced(daemons, num_daemons);

  // Register the service node
  LOKI_FOR_EACH(daemon_index, num_daemons)
  {
    daemon_prepare_registration_params params             = {};
    params.contributors[params.num_contributors].addr     = *addr;
    params.contributors[params.num_contributors++].amount = 100;

    loki_scratch_buf registration_cmd = {};
    if (!daemon_prepare_registration(daemons + daemon_index, &params, &registration_cmd) ||
        !wallet_register_service_node(wallet, registration_cmd.c_str))
    {
      return false;
    }
  }

  return true;
}

//
// Latest Tests
//
test_result foo()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

#if 0
  loki_addr wallet_addr = {};
  daemon_t daemon       = {};
  wallet_t wallet       = {};
  helper_setup_blockchain_with_n_blocks(&result, &daemon, &wallet, &wallet_addr, 1);
  LOKI_DEFER { daemon_exit(&daemon); wallet_exit(&wallet); };

  daemon_mine_until_height(&daemon, &wallet_addr, 100);

  for (;;)
  {
    daemon_status(&daemon);
    os_sleep_ms(2000);
  }
#else
  const int NUM_DAEMONS = 20;
  daemon_t daemons[NUM_DAEMONS] = {};
  start_daemon_params daemon_params[NUM_DAEMONS]  = {};
  {
    LOKI_FOR_EACH(i, NUM_DAEMONS)
      daemon_params[i].load_latest_hardfork_versions();
    create_and_start_multi_daemons(daemons, NUM_DAEMONS, daemon_params, NUM_DAEMONS, result.name.c_str);
  }

  loki_addr addr = {};
  addr.set_normal_addr(LOKI_MAINNET_ADDR[0]);

  for (;;)
  {
    daemon_mine_n_blocks(daemons + 0, &addr, 1);
    LOKI_FOR_EACH(daemon_index, NUM_DAEMONS)
    {
      daemon_t *daemon = daemons + daemon_index;
      daemon_relay_votes_and_uptime(daemon);
    }
    helper_block_until_blockchains_are_synced(daemons, NUM_DAEMONS);
  }
#endif


  return result;
}

test_result latest__checkpointing__private_chain_reorgs_to_checkpoint_chain()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  int const NUM_SERVICE_NODES = 10;
  int const NUM_DAEMONS       = 1;

  start_daemon_params daemon_params = {};
  daemon_params.load_latest_hardfork_versions();
  daemon_params.keep_terminal_open = false;
  helper_blockchain_environment environment = {};
  EXPECT(result,
         helper_setup_blockchain(&environment,
                                 &result,
                                 daemon_params,
                                 NUM_SERVICE_NODES,
                                 NUM_DAEMONS,
                                 1 /*num wallets*/,
                                 100 /*wallet_balance*/),
         "Failed to setup basic blockchain with service nodes");
  LOKI_DEFER { helper_cleanup_blockchain_environment(&environment); };

  // Naughty daemon disconnects from the main chain by banning everyone
  daemon_t *naughty_daemon                  = environment.daemons + 0;
  daemon_t *service_nodes                   = environment.service_nodes;
  {
    loki_buffer<32> ip("127.0.0.1");
    daemon_ban(naughty_daemon, &ip);
  }

  wallet_t *wallet = environment.wallets.data();
  // Mine registration and become service nodes and mine some blocks to create checkpoint votes
  for (int index = 0; index < ((LOKI_VOTE_LIFETIME / LOKI_CHECKPOINT_INTERVAL) * 1); index++)
  {
    daemon_mine_n_blocks(service_nodes + 0, wallet, LOKI_CHECKPOINT_INTERVAL);
    LOKI_FOR_EACH(daemon_index, NUM_SERVICE_NODES)
    {
      daemon_t *daemon = service_nodes + daemon_index;
      daemon_relay_votes_and_uptime(daemon);
    }
    helper_block_until_blockchains_are_synced(service_nodes, NUM_DAEMONS - 1);
  }

  // Naughty daemon mines their chain secretly ahead of the canonical chain
  uint64_t canonical_chain_height = daemon_status(service_nodes + 0).height;
  uint64_t naughty_daemon_height  = canonical_chain_height + (LOKI_VOTE_LIFETIME);
  {
    start_wallet_params naughty_wallet_params = {};
    naughty_wallet_params.daemon              = naughty_daemon;
    wallet_t naughty_wallet                   = create_and_start_wallet(daemon_params.nettype, naughty_wallet_params, result.name.c_str);
    wallet_set_default_testing_settings(&naughty_wallet);
    daemon_mine_until_height(naughty_daemon, &naughty_wallet, naughty_daemon_height);
    wallet_exit(&naughty_wallet);
  }

  // Mine blocks on the mainchain so that the naughty daemon blocks are invalid
  LOKI_FOR_EACH(i, (LOKI_VOTE_LIFETIME * 1) / LOKI_CHECKPOINT_INTERVAL)
  {
    daemon_mine_n_blocks(service_nodes + 0, wallet, LOKI_CHECKPOINT_INTERVAL);
    LOKI_FOR_EACH(daemon_index, NUM_SERVICE_NODES)
    {
      daemon_t *daemon = service_nodes + daemon_index;
      daemon_relay_votes_and_uptime(daemon);
    }
    helper_block_until_blockchains_are_synced(service_nodes, NUM_SERVICE_NODES);
  }
  daemon_status_t daemon_0_status = daemon_status(service_nodes + 0);

  EXPECT(result,
         compare_checkpoints(environment.service_nodes, NUM_SERVICE_NODES),
         "Checkpoints did not match between daemons to be checked");

  // Naughty daemon reconnects to the chain. All the other nodes should NOT
  // reorg to this chain, but the naughty daemon should no longer be accepted.
  {
    loki_buffer<32> ip("127.0.0.1");
    daemon_unban(naughty_daemon, &ip);
  }

  // TODO(doyle): This could be more robust by checking the top block hash
  daemon_status_t naughty_daemon_status = daemon_status(naughty_daemon);
  uint64_t target_blockchain_height     = daemon_0_status.height;
  uint64_t naughty_height               = naughty_daemon_status.height;

  // NOTE: Retry a couple of times to see if the daemon will reorg
  for (int i = 0; i < 99 && naughty_height != target_blockchain_height; i++)
  {
    os_sleep_ms(1500);
    naughty_daemon_status = daemon_status(naughty_daemon);
    naughty_height        = naughty_daemon_status.height;
  }

  helper_block_until_blockchains_are_synced(environment.all_daemons.data(), NUM_DAEMONS);
  EXPECT(result,
         compare_checkpoints(environment.all_daemons.data(), NUM_SERVICE_NODES + NUM_DAEMONS),
         "Checkpoints did not match between daemons to be checked after private chain reorged to canonical chain");

  EXPECT(result, target_blockchain_height == naughty_height, "%zu == %zu, The naughty daemon who private mined a chain did not reorg to the checkpointed chain!", target_blockchain_height, naughty_height);
  return result;
}

test_result latest__checkpointing__new_peer_syncs_checkpoints()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.load_latest_hardfork_versions();

  int const NUM_DAEMONS                  = (LOKI_CHECKPOINT_QUORUM_SIZE * 2) + 1;
  int const NUM_SERVICE_NODES            = NUM_DAEMONS - 1;
  loki_snode_key snode_keys[NUM_DAEMONS] = {};
  daemon_t daemons[NUM_DAEMONS]          = {};

  // daemon_params.keep_terminal_open = true;
  create_and_start_multi_daemons(daemons, NUM_DAEMONS, &daemon_params, 1, result.name.c_str);
  for (size_t i = 0; i < NUM_SERVICE_NODES; ++i)
    LOKI_ASSERT(daemon_print_sn_key(daemons + i, snode_keys + i));

  LOKI_FOR_EACH(daemon_index, NUM_DAEMONS)
  {
    daemon_t *daemon = daemons + daemon_index;
    daemon_toggle_obligation_quorum(daemon);
  }

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = daemons + 0;
  wallet_t wallet                   = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
  wallet_set_default_testing_settings(&wallet);

  loki_buffer<32> ban_ip("127.0.0.1");
  daemon_t *new_peer = daemons + NUM_DAEMONS - 1;
  daemon_ban(new_peer, &ban_ip);

  LOKI_DEFER
  {
    wallet_exit(&wallet);
    for (size_t i = 0; i < (size_t)NUM_DAEMONS; ++i)
      daemon_exit(daemons + i);
  };

  // Setup node registration params to come from our single wallet
  daemon_mine_n_blocks(daemons + 0, &wallet, MIN_BLOCKS_IN_BLOCKCHAIN);
  {
    daemon_prepare_registration_params register_params                    = {};
    register_params.contributors[register_params.num_contributors].amount = LOKI_FAKENET_STAKING_REQUIREMENT;
    wallet_address(&wallet, 0, &register_params.contributors[register_params.num_contributors++].addr);

    LOKI_FOR_EACH(i, NUM_SERVICE_NODES) // Register each daemon on the network
    {
      loki_scratch_buf registration_cmd = {};
      daemon_t *daemon                  = daemons + i;
      EXPECT(result, daemon_prepare_registration (daemon, &register_params, &registration_cmd), "Failed to prepare registration");

      loki_transaction register_tx = {};
      EXPECT(result, wallet_register_service_node(&wallet, registration_cmd.c_str, &register_tx),"Failed to register service node");
    }
  }

  // Mine registration and become service nodes and mine and generate blockchain + checkpoints
  for (int index = 0; index < (LOKI_VOTE_LIFETIME / LOKI_CHECKPOINT_INTERVAL); index++)
  {
    daemon_mine_n_blocks(daemons + 0, &wallet, LOKI_CHECKPOINT_INTERVAL);
    LOKI_FOR_EACH(daemon_index, NUM_SERVICE_NODES)
    {
      daemon_t *daemon = daemons + daemon_index;
      daemon_relay_votes_and_uptime(daemon);
    }
    helper_block_until_blockchains_are_synced(daemons, NUM_DAEMONS - 1);
  }

  // Unban localhost, restoring connection to the new peer and see if it syncs up
  daemon_status_t daemon_0_status   = daemon_status(daemons + 0);
  daemon_unban(new_peer, &ban_ip);
  daemon_status_t new_peer_status   = daemon_status(new_peer);
  uint64_t target_blockchain_height = daemon_0_status.height;
  uint64_t new_peer_height          = new_peer_status.height;

  // NOTE: Retry a couple of times and wait for the new peer to sync
  for (int i = 0; i < 24 && new_peer_height != target_blockchain_height; i++)
  {
    os_sleep_ms(2000);
    new_peer_status = daemon_status(new_peer);
    new_peer_height = new_peer_status.height;
  }
  EXPECT(result, target_blockchain_height == new_peer_height, "%zu == %zu, The new peer did not sync to the target chain height!", target_blockchain_height, new_peer_height);
  EXPECT(result, compare_checkpoints(daemons, NUM_DAEMONS), "Checkpoints did not match between all daemons");
  return result;
}

test_result latest__checkpointing__deregister_non_participating_peer()
{
  // NOTE: Setup environment
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  int const NUM_SERVICE_NODES = LOKI_MAX(LOKI_CHECKPOINT_QUORUM_SIZE, LOKI_STATE_CHANGE_QUORUM_SIZE) * 2;
  int const NUM_DAEMONS       = 0;
  int const NUM_WALLETS       = 1;
  int const WALLET_BALANCE    = 100;

  start_daemon_params daemon_params = {};
  // daemon_params.keep_terminal_open = true;
  daemon_params.load_latest_hardfork_versions();
  helper_blockchain_environment environment = {};
  EXPECT(result,
         helper_setup_blockchain(&environment, &result, daemon_params, NUM_SERVICE_NODES, NUM_DAEMONS, NUM_WALLETS, WALLET_BALANCE),
         "Failed to setup basic blockchain with service nodes");
  LOKI_DEFER { helper_cleanup_blockchain_environment(&environment); };
  wallet_t *wallet      = environment.wallets.data();

  // NOTE: Test Start
  int const NUM_GOOD_SERVICE_NODES = NUM_SERVICE_NODES - 5;
  daemon_t *service_nodes          = environment.service_nodes;

  int const NUM_NAUGHTY_SERVICE_NODES = NUM_SERVICE_NODES - NUM_GOOD_SERVICE_NODES;
  daemon_t *naughty_daemons           = environment.service_nodes + NUM_GOOD_SERVICE_NODES;

  // NOTE: Naughty daemon does not participate in the checkpointing quorums
  LOKI_FOR_EACH(i, NUM_NAUGHTY_SERVICE_NODES)
  {
    daemon_toggle_checkpoint_quorum(naughty_daemons + i);
    loki_buffer<32> ip("127.0.0.1");
    daemon_ban(naughty_daemons + i, &ip);
  }

  for (daemon_t &daemon : environment.all_daemons)
  {
    daemon_toggle_obligation_uptime_proof(&daemon);
    daemon_relay_votes_and_uptime(&daemon);
  }

  int blocks_to_mine = 120;
  LOKI_FOR_EACH(i, blocks_to_mine / LOKI_CHECKPOINT_INTERVAL)
  {
    daemon_mine_n_blocks(service_nodes, wallet, LOKI_CHECKPOINT_INTERVAL);
    helper_block_until_blockchains_are_synced(service_nodes, NUM_GOOD_SERVICE_NODES);

    LOKI_FOR_EACH(j, NUM_GOOD_SERVICE_NODES)
      daemon_relay_votes_and_uptime(service_nodes + j);
    os_sleep_ms(250);
  }

  EXPECT(result, compare_checkpoints(service_nodes, NUM_GOOD_SERVICE_NODES), "Checkpoints do not match between the good service nodes");

  int registered_count   = 0;
  int deregistered_count = 0;
  for (loki_snode_key &snode_key : environment.snode_keys)
  {
    daemon_snode_status status = daemon_print_sn(service_nodes + 0, &snode_key);
    if (status.registered) registered_count++;
    else deregistered_count++;
  }

  EXPECT(result,
         deregistered_count > 0,
         "The naughty daemons disabled their checkpoint quorum, so they should not be participating in quorums and "
         "hence should be deregistered");
  return result;
}

test_result latest__decommission__recommission_on_uptime_proof()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  int const NUM_SERVICE_NODES = 10;
  int const NUM_DAEMONS       = 1;

  start_daemon_params daemon_params = {};
  daemon_params.load_latest_hardfork_versions();
  daemon_params.keep_terminal_open = false;
  helper_blockchain_environment environment = {};
  EXPECT(result,
         helper_setup_blockchain(&environment,
                                 &result,
                                 daemon_params,
                                 NUM_SERVICE_NODES,
                                 NUM_DAEMONS,
                                 1 /*num wallets*/,
                                 100 /*wallet_balance*/),
         "Failed to setup basic blockchain with service nodes");
  LOKI_DEFER { helper_cleanup_blockchain_environment(&environment); };

  // Naughty daemon disconnects from the main chain
  daemon_t *bad_service_node       = environment.service_nodes + 0;
  loki_snode_key *bad_snode_key    = environment.snode_keys.data();
  daemon_t *good_service_nodes     = environment.service_nodes + 1;
  int const NUM_GOOD_SERVICE_NODES = NUM_SERVICE_NODES - 1;

  loki_buffer<32> const ip("127.0.0.1");
  daemon_ban(bad_service_node, &ip);

  wallet_t *wallet = environment.wallets.data();
  daemon_mine_n_blocks(good_service_nodes + 0, wallet, LOKI_DECOMMISSION_INITIAL_CREDIT);
  helper_block_until_blockchains_are_synced(good_service_nodes, NUM_GOOD_SERVICE_NODES);

  daemon_snode_status status = {};
  for (status = daemon_print_sn(good_service_nodes + 0, bad_snode_key);
      !status.decommissioned;
       status = daemon_print_sn(good_service_nodes + 0, bad_snode_key))
  {
    daemon_mine_n_blocks(good_service_nodes + 0, wallet, 1);
    LOKI_FOR_EACH(daemon_index, NUM_GOOD_SERVICE_NODES)
    {
      daemon_t *daemon = good_service_nodes + daemon_index;
      daemon_relay_votes_and_uptime(daemon);
    }
    helper_block_until_blockchains_are_synced(good_service_nodes, NUM_GOOD_SERVICE_NODES);
  }

  EXPECT(result,
         !status.last_uptime_proof_received,
         "The node should not have any uptime proof relayed yet, we want to be in decommission, enter a quorum and be "
         "recommissioned");

  daemon_unban(bad_service_node, &ip);
  helper_block_until_blockchains_are_synced(environment.service_nodes, environment.num_service_nodes);

  // NOTE: Relay uptime proof and try a couple of times to see if our uptime proof got received by the service node
  daemon_relay_votes_and_uptime(bad_service_node);
  for (int tries = 0; tries < 8; ++tries)
  {
    status = daemon_print_sn(good_service_nodes + 0, bad_snode_key);
    if (status.last_uptime_proof_received) break;
    os_sleep_ms(1000);
  }

  EXPECT(result,
         status.last_uptime_proof_received,
         "The service node reconnected and submitted an uptime proof to the network, this should be received by the other behaving nodes");

  // NOTE: Continue mining until the service node gets recommissioned, which it should for submitting the uptime proof
  for (status = daemon_print_sn(good_service_nodes + 0, bad_snode_key);
       status.decommissioned;
       status = daemon_print_sn(good_service_nodes + 0, bad_snode_key))
  {
    daemon_mine_n_blocks(good_service_nodes + 0, wallet, 1);
    helper_block_until_blockchains_are_synced(environment.service_nodes, environment.num_service_nodes);
  }

  return result;
}

test_result latest__deregistration__n_unresponsive_node()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.fixed_difficulty    = 1;
  daemon_params.keep_terminal_open  = false;
  daemon_params.load_latest_hardfork_versions();

  int const NUM_DAEMONS                  = (LOKI_STATE_CHANGE_QUORUM_SIZE * 2);
  loki_snode_key snode_keys[NUM_DAEMONS] = {};
  daemon_t daemons[NUM_DAEMONS]          = {};
  int num_deregister_daemons             = LOKI_STATE_CHANGE_QUORUM_SIZE / 2;
  int num_register_daemons               = NUM_DAEMONS - num_deregister_daemons;

  loki_snode_key *register_snode_keys    = snode_keys;
  daemon_t       *deregister_daemons     = daemons    + num_register_daemons;
  loki_snode_key *deregister_snode_keys  = snode_keys + num_register_daemons;

  // daemon_params.keep_terminal_open = true;
  create_and_start_multi_daemons(daemons, NUM_DAEMONS, &daemon_params, 1, result.name.c_str);
  for (size_t i = 0; i < NUM_DAEMONS; ++i)
  {
    LOKI_ASSERT(daemon_print_sn_key(daemons + i, snode_keys + i));
  }

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = daemons + 0;
  wallet_t wallet                   = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
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
    daemon_mine_n_blocks(daemons + 0, &wallet, MIN_BLOCKS_IN_BLOCKCHAIN);
    wallet_mine_until_unlocked_balance(&wallet, daemons + 0, static_cast<int>(NUM_DAEMONS * FAKECHAIN_STAKING_REQUIREMENT)); // TODO(doyle): Assuming staking requirement of 100 fakechain
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

      loki_transaction register_tx = {};
      EXPECT(result, wallet_register_service_node(&wallet, registration_cmd.c_str, &register_tx),"Failed to register service node");
      daemon_relay_tx(daemons + 0, register_tx.id.c_str);

      daemon_toggle_checkpoint_quorum(daemon);
      daemon_toggle_obligation_checkpointing(daemon);
    }
  }

  // Daemons to deregister should exit so that they don't ping when they get mined onto the chain
  LOKI_FOR_EACH(i, num_deregister_daemons)
  {
    daemon_t *daemon = deregister_daemons + i;
#if 0
    loki_buffer<32> ip("127.0.0.1");
    daemon_ban(daemon, &ip);
#else
    daemon_exit(daemon);
#endif
  }

  // Mine the registration txs onto the chain and check they have been registered
  {
    daemon_mine_n_blocks(daemons + 0, &wallet, 2); // Get onto chain
    helper_block_until_blockchains_are_synced(daemons, num_register_daemons);

    LOKI_FOR_EACH(j, 2)
    {
      LOKI_FOR_EACH(i, num_register_daemons)
      {
        daemon_t *daemon = daemons + i;
        daemon_relay_votes_and_uptime(daemon);
      }
    }

    LOKI_FOR_EACH(i, NUM_DAEMONS)
    {
      // loki_snode_key const *snode_key = snode_keys + i;
      // daemon_snode_status status      = daemon_print_sn(daemons + 0, snode_key);
      // EXPECT(result, status.registered, "We EXPECT all daemons to be registered at this point", NUM_DAEMONS);
    }
  }

  // Mine atleast LOKI_REORG_SAFETY_BUFFER. Quorum voting can only start after LOKI_REORG_SAFETY_BUFFER
  {
    daemon_mine_n_blocks(daemons + 0, &wallet, LOKI_REORG_SAFETY_BUFFER);
    helper_block_until_blockchains_are_synced(daemons, num_register_daemons);
    LOKI_FOR_EACH(i, num_register_daemons)
    {
      daemon_t *daemon = daemons + i;
      daemon_relay_votes_and_uptime(daemon);
    }

    // Mine blocks to deregister daemons
    int block_batch = 1;
    LOKI_FOR_EACH(i, 90 / block_batch)
    {
      daemon_mine_n_blocks(daemons + 0, &wallet, block_batch);
      LOKI_FOR_EACH(j, num_register_daemons)
      {
        daemon_t *daemon = daemons + j;
        daemon_relay_votes_and_uptime(daemon);
      }
      helper_block_until_blockchains_are_synced(daemons, num_register_daemons);
    }
  }

  // Check that there are some nodes that have been deregistered. Not
  // necessarily all of them because we may not have formed a quorum that was to
  // vote them off
  LOKI_FOR_EACH(i, num_register_daemons)
  {
    loki_snode_key *snode_key             = register_snode_keys + i;
    daemon_snode_status node_status       = daemon_print_sn(daemons + 0, snode_key);
    EXPECT(result, node_status.registered == true, "Daemon %d should still be online, pingining and alive!", i);
  }

  int total_registered_daemons = num_register_daemons;
  LOKI_FOR_EACH(i, num_deregister_daemons)
  {
    loki_snode_key *snode_key             = deregister_snode_keys + i;
    daemon_snode_status node_status       = daemon_print_sn(daemons + 0, snode_key);
    if (node_status.registered) total_registered_daemons++;
  }

  EXPECT(result,
         total_registered_daemons >= num_register_daemons && total_registered_daemons < NUM_DAEMONS,
         "We EXPECT atleast some daemons to deregister. It's possible that not all deregistered if they didn't get assigned a quorum. total_registered_daemons: %d, NUM_DAEMONS: %d",
         total_registered_daemons, NUM_DAEMONS);

  return result;
}

test_result latest__prepare_registration__check_all_solo_stake_forms_valid_registration()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.load_latest_hardfork_versions();

  daemon_t daemon = create_and_start_daemon(daemon_params, result.name.c_str);
  LOKI_DEFER { daemon_exit(&daemon); };

  // NOTE(loki): Mine enough to reach the latest fork
  {
    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    wallet_t wallet                   = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
    wallet_set_default_testing_settings(&wallet);
    daemon_mine_n_blocks(&daemon, &wallet, 32);
    wallet_exit(&wallet);
  }

  char const *wallet1 = LOKI_MAINNET_ADDR[0];

  daemon_prepare_registration_params registration_params = {};
  registration_params.num_contributors                   = 1;
  registration_params.contributors[0].addr.set_normal_addr(wallet1);

  for (int i = 0; i < 101; ++i) // TODO(doyle): Assumes staking requirement
  {
    registration_params.contributors[0].amount = i;
    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration(&daemon, &registration_params, &registration_cmd), "Failed to prepare registration");

    // Expected Format: register_service_node <operator cut> <address> <fraction> [<address> <fraction> [...]]]
    char const *register_str      = str_find(registration_cmd.c_str, "register_service_node");
    char const *prev              = register_str;

    char const *operator_portions = str_skip_to_next_word_inplace(&prev);
    char const *wallet_addr       = str_skip_to_next_word_inplace(&prev);
    char const *addr1_portions    = str_skip_to_next_word_inplace(&prev);

    EXPECT_STR(result, register_str,      "register_service_node", "Could not find expected str in: %s", register_str);
    EXPECT_STR(result, operator_portions, "18446744073709551612",  "Could not find expected str in: %s", register_str);
    EXPECT_STR(result, wallet_addr,       wallet1,                 "Could not find expected str in: %s", register_str);
    EXPECT_STR(result, addr1_portions,    "18446744073709551612",  "Could not find expected str in: %s", register_str);
  }
  return result;
}

test_result latest__prepare_registration__check_solo_stake()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.load_latest_hardfork_versions();
  daemon_params.keep_terminal_open = false;

  daemon_t daemon = create_and_start_daemon(daemon_params, result.name.c_str);
  LOKI_DEFER { daemon_exit(&daemon); };

  // NOTE(loki): Mine enough to reach the latest fork
  {
    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    wallet_t wallet                   = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
    wallet_set_default_testing_settings(&wallet);
    daemon_mine_n_blocks(&daemon, &wallet, 32);
    wallet_exit(&wallet);
  }

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

  char const *operator_portions = str_skip_to_next_word_inplace(&prev);
  char const *wallet_addr       = str_skip_to_next_word_inplace(&prev);
  char const *addr1_portions    = str_skip_to_next_word_inplace(&prev);

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

  daemon_t daemon = create_and_start_daemon(daemon_params, result.name.c_str);
  LOKI_DEFER { daemon_exit(&daemon); };

  // NOTE(loki): Mine enough to reach the latest fork
  {
    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    wallet_t wallet                   = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
    wallet_set_default_testing_settings(&wallet);
    daemon_mine_n_blocks(&daemon, &wallet, 32);
    wallet_exit(&wallet);
  }

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

  char const *operator_cut     = str_skip_to_next_word_inplace(&prev);
  char const *wallet1_addr     = str_skip_to_next_word_inplace(&prev);
  char const *wallet1_portions = str_skip_to_next_word_inplace(&prev);
  char const *wallet2_addr     = str_skip_to_next_word_inplace(&prev);
  char const *wallet2_portions = str_skip_to_next_word_inplace(&prev);

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
    daemon                            = create_and_start_daemon(daemon_params, result.name.c_str);

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    wallet                            = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
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
    daemon                            = create_and_start_daemon(daemon_params, result.name.c_str);

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    wallet                            = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
    wallet_set_default_testing_settings(&wallet);
  }

  loki_addr my_addr = {};
  daemon_mine_n_blocks(&daemon, &wallet, 100);
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

  daemon_mine_n_blocks(&daemon, &wallet, 1); // Get registration onto the chain

  // Stake the remainder to make 2 locked stakes for this node and mine some blocks to get us registered
  loki_snode_key snode_key = {};
  {
    EXPECT(result, daemon_print_sn_key(&daemon, &snode_key), "Failed to print sn key");
    EXPECT(result, wallet_stake(&wallet, &snode_key, half_stake_requirement), "Failed to stake remaining loki to service node");
    daemon_mine_n_blocks(&daemon, &wallet, 1); // Get stake onto the chain
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

  daemon_t daemon           = create_and_start_daemon(daemon_params, result.name.c_str);
  wallet_t  stakers[4]      = {};
  loki_addr stakers_addr[4] = {};

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;

  // Mine and unlock funds
  for (int i = 0; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
  {
    stakers[i]       = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
    wallet_t *staker = stakers + i;

    wallet_set_default_testing_settings(staker);
    wallet_address(staker, 0, stakers_addr + i);
    daemon_mine_n_blocks(&daemon, staker, 2);
  }

  LOKI_DEFER
  {
    daemon_exit(&daemon);
    for (int i = 0; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
      wallet_exit(stakers + i);
  };

  wallet_t *owner = stakers + 0;
  daemon_mine_n_blocks(&daemon, owner, 100);

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
  daemon_mine_n_blocks(&daemon, owner, 1);

  // Each person stakes their part to the wallet
  for (int i = 1; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
  {
    wallet_t *staker = stakers + i;
    wallet_refresh(staker);
    loki_contributor const *contributor = registration_params.contributors + i;
    LOKI_ASSERT(wallet_stake(staker, &snode_key, contributor->amount));
  }

  daemon_mine_n_blocks(&daemon, owner, 1);
  daemon_snode_status node_status = daemon_print_sn_status(&daemon);
  EXPECT(result, node_status.registered, "Service node could not be registered");
  return result;
}

test_result latest__register_service_node__allow_70_20_and_10_open_for_contribution()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.load_latest_hardfork_versions();

  daemon_t daemon           = create_and_start_daemon(daemon_params, result.name.c_str);
  wallet_t  stakers[2]      = {};
  loki_addr stakers_addr[2] = {};

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;

  // Mine and unlock funds
  for (int i = 0; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
  {
    stakers[i]       = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
    wallet_t *staker = stakers + i;

    wallet_set_default_testing_settings(staker);
    wallet_address(staker, 0, stakers_addr + i);
    daemon_mine_n_blocks(&daemon, staker, 2);
  }

  LOKI_DEFER
  {
    daemon_exit(&daemon);
    for (int i = 0; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
      wallet_exit(stakers + i);
  };

  wallet_t *owner = stakers + 0;
  daemon_mine_n_blocks(&daemon, owner, 100);

  loki_snode_key snode_key = {};
  daemon_print_sn_key(&daemon, &snode_key);

  // Register the service node and put it on the chain
  daemon_prepare_registration_params registration_params = {};
  registration_params.open_pool        = true;
  registration_params.num_contributors = (int)LOKI_ARRAY_COUNT(stakers);
  {
    registration_params.contributors[0].addr   = stakers_addr[0];
    registration_params.contributors[0].amount = 70; // TODO(doyle): Assumes testnet staking requirement of 100
    registration_params.contributors[1].addr   = stakers_addr[1];
    registration_params.contributors[1].amount = 20; // TODO(doyle): Assumes testnet staking requirement of 100
  }

  loki_scratch_buf registration_cmd = {};
  daemon_prepare_registration (&daemon, &registration_params, &registration_cmd);
  EXPECT(result, wallet_register_service_node(owner, registration_cmd.c_str), "Failed to register service node");
  daemon_mine_n_blocks(&daemon, owner, 1);

  // Each person stakes their part to the wallet
  for (int i = 1; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
  {
    wallet_t *staker = stakers + i;
    loki_contributor const *contributor = registration_params.contributors + i;
    LOKI_ASSERT(wallet_stake(staker, &snode_key, contributor->amount));
  }

  daemon_mine_n_blocks(&daemon, owner, 1);
  daemon_snode_status node_status = daemon_print_sn_status(&daemon);
  EXPECT(result, !node_status.registered, "Service node should NOT be registered yet, should be 10 loki remaining");
  return result;
}

test_result latest__register_service_node__allow_43_23_13_21_reserved_contribution()
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
    daemon                            = create_and_start_daemon(daemon_params, result.name.c_str);

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;

    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallets))
    {
      wallets[i] = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
      EXPECT(result, wallet_address(wallets + i, 0, wallet_addrs + i), "Failed to query wallet: %d's primary address", (int)i);
      wallet_set_default_testing_settings(wallets + i);
    }

    dummy_wallet = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
    wallet_set_default_testing_settings(&dummy_wallet);
    EXPECT(result, wallet_address(&dummy_wallet, 0, &dummy_addr),  "Failed to get the 0th subaddress, i.e the main address of wallet");

    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallets))
      daemon_mine_n_blocks(&daemon, wallets + i, 5);

    daemon_mine_until_height(&daemon, wallets + 0, 200);
  }

  // Register the service node, wallet 0 contributes a prime number, which means there'll be dust in the portions
  const uint64_t wallet0_contributes = 43;
  const uint64_t wallet1_contributes = 31;
  const uint64_t wallet2_contributes = 17;
  const uint64_t wallet3_contributes = 9;
  loki_snode_key snode_key = {};
  {
    EXPECT(result, daemon_print_sn_key(&daemon, &snode_key), "Failed to get service node key, did daemon start in service node mode?");
    daemon_prepare_registration_params params             = {};
    params.contributors[params.num_contributors].addr     = wallet_addrs[0];
    params.contributors[params.num_contributors++].amount = wallet0_contributes;

    params.contributors[params.num_contributors].addr     = wallet_addrs[1];
    params.contributors[params.num_contributors++].amount = wallet1_contributes;

    params.contributors[params.num_contributors].addr     = wallet_addrs[2];
    params.contributors[params.num_contributors++].amount = wallet2_contributes;

    params.contributors[params.num_contributors].addr     = wallet_addrs[3];
    params.contributors[params.num_contributors++].amount = wallet3_contributes;

    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration (&daemon, &params, &registration_cmd), "Failed to prepare registration");
    EXPECT(result, wallet_register_service_node(wallets + 0, registration_cmd.c_str),    "Failed to register service node");
  }
  daemon_mine_n_blocks(&daemon, &dummy_wallet, 5); // Get registration onto the chain

  // Wallet 1 contributes a prime number, which means there'll be dust in the portions
  {
    wallet_refresh(wallets + 1);
    EXPECT(result, wallet_stake(wallets + 1, &snode_key, wallet1_contributes), "Wallet failed to stake");
    daemon_mine_n_blocks(&daemon, &dummy_wallet, 5);
  }

  // Wallet 2 contributes a prime number, which means there'll be dust in the portions
  {
    wallet_refresh(wallets + 2);
    EXPECT(result, wallet_stake(wallets + 2, &snode_key, wallet2_contributes), "Wallet failed to stake");
    daemon_mine_n_blocks(&daemon, &dummy_wallet, 5);
  }

  // Wallet 3 contributes remainder
  {
    wallet_refresh(wallets + 3);
    EXPECT(result, wallet_stake(wallets + 3, &snode_key, wallet3_contributes), "Wallet failed to stake");
    daemon_mine_n_blocks(&daemon, &dummy_wallet, 5);
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
    if      (i == 0) expected_amount = wallet0_contributes;
    else if (i == 1) expected_amount = wallet1_contributes;
    else if (i == 2) expected_amount = wallet2_contributes;
    else if (i == 3) expected_amount = wallet3_contributes;

    EXPECT(result, stake->total_locked_amount == expected_amount * LOKI_ATOMIC_UNITS, "Total locked amount: %zu didn't match expected: %zu", stake->total_locked_amount, expected_amount * LOKI_ATOMIC_UNITS);
    EXPECT(result, amount == expected_amount * LOKI_ATOMIC_UNITS,
        "Locked stake amount didn't match what we expected, amount: %zu, requirement: %zu",
        amount, expected_amount * LOKI_ATOMIC_UNITS);
  }
  return result;
}

test_result latest__register_service_node__allow_87_13_reserved_contribution()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon        = {};
  wallet_t wallets[2]    = {};
  wallet_t dummy_wallet  = {};
  LOKI_DEFER
  {
    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallets))
      wallet_exit(wallets + i);

    wallet_exit(&dummy_wallet);
    daemon_exit(&daemon);
  };

  // start up daemon and wallet and mine until sufficient unlocked balances in each
  loki_addr wallet_addrs[LOKI_ARRAY_COUNT(wallets)] = {};
  loki_addr dummy_addr                              = {};
  {
    start_daemon_params daemon_params = {};
    daemon_params.load_latest_hardfork_versions();
    daemon                            = create_and_start_daemon(daemon_params, result.name.c_str);

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;

    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallets))
    {
      wallets[i] = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
      EXPECT(result, wallet_address(wallets + i, 0, wallet_addrs + i), "failed to query wallet: %d's primary address", (int)i);
      wallet_set_default_testing_settings(wallets + i);
    }

    dummy_wallet = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
    wallet_set_default_testing_settings(&dummy_wallet);
    EXPECT(result, wallet_address(&dummy_wallet, 0, &dummy_addr),  "failed to get the 0th subaddress, i.e the main address of wallet");

    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallets))
      daemon_mine_n_blocks(&daemon, wallets + i, 5);

    daemon_mine_until_height(&daemon, wallets + 0, 200);
  }

  // register the service node, wallet 0 contributes a prime number, which means there'll be dust in the portions
  loki_snode_key snode_key = {};
  {
    EXPECT(result, daemon_print_sn_key(&daemon, &snode_key), "failed to get service node key, did daemon start in service node mode?");
    daemon_prepare_registration_params params             = {};
    params.open_pool                                      = true;
    params.contributors[params.num_contributors].addr     = wallet_addrs[0];
    params.contributors[params.num_contributors++].amount = 87;

    params.contributors[params.num_contributors].addr     = wallet_addrs[1];
    params.contributors[params.num_contributors++].amount = 13;

    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration (&daemon, &params, &registration_cmd), "failed to prepare registration");
    EXPECT(result, wallet_register_service_node(wallets + 0, registration_cmd.c_str),    "failed to register service node");
  }
  daemon_mine_n_blocks(&daemon, &dummy_wallet, 5); // get registration onto the chain

  // wallet 1 contributes a prime number, which means there'll be dust in the portions
  {
    wallet_refresh(wallets + 1);
    EXPECT(result, wallet_stake(wallets + 1, &snode_key, 13), "wallet failed to stake");
    daemon_mine_n_blocks(&daemon, &dummy_wallet, 1);
  }

  // check stakes show up as locked
  LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallet_addrs))
  {
    wallet_refresh(wallets + i);
    wallet_locked_stakes const locked_stakes = wallet_print_locked_stakes(wallets + i);
    EXPECT(result, locked_stakes.locked_stakes_len == 1, "we staked, so we should have locked key images");

    wallet_locked_stake const *stake = locked_stakes.locked_stakes + 0;
    EXPECT(result, stake->snode_key           == snode_key, "locked stake service node key didn't match what we expected, stake->snode_key: %s, snode_key: %s", stake->snode_key.c_str, snode_key.c_str);
    EXPECT(result, stake->locked_amounts_len  == 1,         "we only made one contribution, so there should only be one locked amount, locked_amounts_len: %d", stake->locked_amounts_len);

    uint64_t amount = stake->locked_amounts[0];
    uint64_t expected_amount = 0;
    if      (i == 0) expected_amount = 87;
    else if (i == 1) expected_amount = 13;

    EXPECT(result, stake->total_locked_amount == expected_amount * LOKI_ATOMIC_UNITS, "total locked amount: %zu didn't match expected: %zu", stake->total_locked_amount, expected_amount * LOKI_ATOMIC_UNITS);
    EXPECT(result, amount == expected_amount * LOKI_ATOMIC_UNITS,
        "locked stake amount didn't match what we expected, amount: %zu, requirement: %zu",
        amount, expected_amount * LOKI_ATOMIC_UNITS);
  }
  return result;
}

test_result latest__register_service_node__allow_87_13_contribution()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon        = {};
  wallet_t wallets[2]    = {};
  wallet_t dummy_wallet  = {};
  LOKI_DEFER
  {
    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallets))
      wallet_exit(wallets + i);

    wallet_exit(&dummy_wallet);
    daemon_exit(&daemon);
  };

  // start up daemon and wallet and mine until sufficient unlocked balances in each
  loki_addr wallet_addrs[LOKI_ARRAY_COUNT(wallets)] = {};
  loki_addr dummy_addr                              = {};
  {
    start_daemon_params daemon_params = {};
    daemon_params.load_latest_hardfork_versions();
    daemon                            = create_and_start_daemon(daemon_params, result.name.c_str);

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;

    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallets))
    {
      wallets[i] = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
      EXPECT(result, wallet_address(wallets + i, 0, wallet_addrs + i), "failed to query wallet: %d's primary address", (int)i);
      wallet_set_default_testing_settings(wallets + i);
    }

    dummy_wallet = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
    wallet_set_default_testing_settings(&dummy_wallet);
    EXPECT(result, wallet_address(&dummy_wallet, 0, &dummy_addr),  "failed to get the 0th subaddress, i.e the main address of wallet");

    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallets))
      daemon_mine_n_blocks(&daemon, wallets + i, 5);

    daemon_mine_until_height(&daemon, wallets + 0, 200);
  }

  // register the service node, wallet 0 contributes 83, a prime number, which means there'll be dust in the portions
  loki_snode_key snode_key = {};
  {
    EXPECT(result, daemon_print_sn_key(&daemon, &snode_key), "failed to get service node key, did daemon start in service node mode?");
    daemon_prepare_registration_params params             = {};
    params.open_pool                                      = true;
    params.contributors[params.num_contributors].addr     = wallet_addrs[0];
    params.contributors[params.num_contributors++].amount = 87;

    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration (&daemon, &params, &registration_cmd), "failed to prepare registration");
    EXPECT(result, wallet_register_service_node(wallets + 0, registration_cmd.c_str),    "failed to register service node");
  }
  daemon_mine_n_blocks(&daemon, &dummy_wallet, 1); // get registration onto the chain

  // wallet 1 contributes 17, a prime number, which means there'll be dust in the portions
  {
    wallet_refresh(wallets + 1);
    EXPECT(result, wallet_stake(wallets + 1, &snode_key, 13), "wallet failed to stake");
    daemon_mine_n_blocks(&daemon, &dummy_wallet, 1);
  }

  // check stakes show up as locked
  LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallet_addrs))
  {
    wallet_refresh(wallets + i);
    wallet_locked_stakes const locked_stakes = wallet_print_locked_stakes(wallets + i);
    EXPECT(result, locked_stakes.locked_stakes_len == 1, "we staked, so we should have locked key images");

    wallet_locked_stake const *stake = locked_stakes.locked_stakes + 0;
    EXPECT(result, stake->snode_key           == snode_key, "locked stake service node key didn't match what we expected, stake->snode_key: %s, snode_key: %s", stake->snode_key.c_str, snode_key.c_str);
    EXPECT(result, stake->locked_amounts_len  == 1,         "we only made one contribution, so there should only be one locked amount, locked_amounts_len: %d", stake->locked_amounts_len);

    uint64_t amount = stake->locked_amounts[0];
    uint64_t expected_amount = 0;
    if      (i == 0) expected_amount = 87;
    else if (i == 1) expected_amount = 13;

    EXPECT(result, stake->total_locked_amount == expected_amount * LOKI_ATOMIC_UNITS, "total locked amount: %zu didn't match expected: %zu", stake->total_locked_amount, expected_amount * LOKI_ATOMIC_UNITS);
    EXPECT(result, amount == expected_amount * LOKI_ATOMIC_UNITS,
        "locked stake amount didn't match what we expected, amount: %zu, requirement: %zu",
        amount, expected_amount * LOKI_ATOMIC_UNITS);
  }
  return result;
}

test_result latest__register_service_node__disallow_register_twice()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.load_latest_hardfork_versions();
  daemon_t daemon                   = create_and_start_daemon(daemon_params, result.name.c_str);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;
  wallet_t wallet                   = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
  LOKI_DEFER { daemon_exit(&daemon); wallet_exit(&wallet); };
  wallet_set_default_testing_settings(&wallet);

  // Mine enough funds for a registration
  loki_addr my_addr = {};
  {
    wallet_address(&wallet, 0, &my_addr);
    daemon_mine_until_height(&daemon, &wallet, 200);
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
    daemon_mine_n_blocks(&daemon, &wallet, 1);

    // Check service node registered
    daemon_snode_status node_status = daemon_print_sn_status(&daemon);
    EXPECT(result, node_status.registered, "Service node was not registered properly");
  }

  // Register twice, should be disallowed
  EXPECT(result, wallet_register_service_node(&wallet, registration_cmd.c_str) == false,
      "You should not be allowed to register twice, if detected on the network");

  return result;
}

test_result latest__register_service_node__check_unlock_time_is_0()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  loki_addr wallet_addr = {};
  daemon_t daemon       = {};
  wallet_t wallet       = {};
  helper_setup_blockchain_with_n_blocks(&result, &daemon, &wallet, &wallet_addr, 200);
  LOKI_DEFER { daemon_exit(&daemon); wallet_exit(&wallet); };

  // Register the service node
  loki_transaction register_tx = {};
  {
    daemon_prepare_registration_params registration_params = {};
    registration_params.num_contributors                   = 1;
    registration_params.contributors[0].addr               = wallet_addr;
    registration_params.contributors[0].amount             = 100; // TODO(doyle): Assuming testnet

    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration(&daemon, &registration_params, &registration_cmd), "Failed to prepare registration for Service Node");
    wallet_register_service_node(&wallet, registration_cmd.c_str, &register_tx);
    daemon_mine_n_blocks(&daemon, &wallet, 1);

    // Check service node registered
    daemon_snode_status node_status = daemon_print_sn_status(&daemon);
    EXPECT(result, node_status.registered, "Service node was not registered properly");
  }

  loki_scratch_buf tx_output = {};
  daemon_print_tx(&daemon, register_tx.id.c_str, &tx_output);

  char const *output_unlock_times_label = str_find(&tx_output, "output_unlock_times");
  EXPECT(result, output_unlock_times_label, "Failed to find output_unlock_times label in print_tx");

  char const *first_output_unlock_str  = str_skip_to_next_digit(output_unlock_times_label);
  char const *second_output_unlock_str = str_skip_to_next_digit(first_output_unlock_str);
  int first_unlock_time = atoi(first_output_unlock_str);
  int second_unlock_time = atoi(second_output_unlock_str);
  EXPECT(result, first_unlock_time  == 0, "Expected register TX output unlock to be 0 in Infinite Staking, (default lock time)");
  EXPECT(result, second_unlock_time == 0, "Expected register TX output unlock to be 0 in Infinite Staking, (default lock time)");
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
    daemon                            = create_and_start_daemon(daemon_params, result.name.c_str);

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;

    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallets))
    {
      wallets[i] = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
      EXPECT(result, wallet_address(wallets + i, 0, wallet_addrs + i), "Failed to query wallet: %d's primary address", (int)i);
      wallet_set_default_testing_settings(wallets + i);
    }

    dummy_wallet = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
    wallet_set_default_testing_settings(&dummy_wallet);
    EXPECT(result, wallet_address(&dummy_wallet, 0, &dummy_addr),  "Failed to get the 0th subaddress, i.e the main address of wallet");

    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallets))
      daemon_mine_n_blocks(&daemon, wallets + i, 5);

    daemon_mine_until_height(&daemon, wallets + 0, MIN_BLOCKS_IN_BLOCKCHAIN);
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
  daemon_mine_n_blocks(&daemon, &dummy_wallet, 1); // Get registration onto the chain

  for (int i = 1; i < (int)LOKI_ARRAY_COUNT(wallets); ++i)
  {
    wallet_refresh(wallets + i);
    EXPECT(result, wallet_stake(wallets + i, &snode_key, staking_requirement_per_contributor), "Wallet failed to stake");
  }
  daemon_mine_n_blocks(&daemon, &dummy_wallet, 1); // Get stakes onto the chain

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
  daemon_mine_n_blocks(&daemon, &dummy_wallet, 1); // Get unlocks

  {
    daemon_snode_status status = daemon_print_sn_status(&daemon);
    EXPECT(result, status.registered, "We mined too many blocks, the service node expired before we could check stake unlocking");
  }

  LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallet_addrs))
  {
    wallet_refresh(wallets + i);
    EXPECT(result, !wallet_request_stake_unlock(wallets + i, &snode_key, nullptr), "After 1 contributor has unlocked, all contributor key images will be unlocked automatically");
  }

  // Check no stakes show up as locked, NOTE: Also mine so that all the rewards get unlocked.
  daemon_mine_until_height(&daemon, &dummy_wallet, unlock_height + LOKI_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW + 1);
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

  // Check the remaining balance is zero, NOTE: +2 to get the sweep transaction on chain
  daemon_mine_n_blocks(&daemon, &dummy_wallet, LOKI_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW + 2);
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
    daemon                            = create_and_start_daemon(daemon_params, result.name.c_str);

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    wallet                            = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
    dummy_wallet                      = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);

    wallet_set_default_testing_settings(&wallet);
    wallet_set_default_testing_settings(&dummy_wallet);
  }

  daemon_mine_n_blocks(&daemon, &wallet, 100);

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

  daemon_mine_n_blocks(&daemon, &wallet, 1); // Get registration onto the chain

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
    daemon = create_and_start_daemon(daemon_params, result.name.c_str);
    LOKI_ASSERT(daemon_print_sn_key(&daemon, &snode_key));

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    wallet                            = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
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
    daemon = create_and_start_daemon(daemon_params, result.name.c_str);
    LOKI_ASSERT(daemon_print_sn_key(&daemon, &snode_key));

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    wallet                            = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
    wallet_set_default_testing_settings(&wallet);
  }

  // Prepare blockchain, atleast 100 blocks so we have outputs to pick from
  int const FAKECHAIN_STAKING_REQUIREMENT = 100;
  {
    daemon_mine_n_blocks(&daemon, &wallet, 100);
    daemon_mine_until_height(&daemon, &wallet, FAKECHAIN_STAKING_REQUIREMENT);
  }

  // Prepare registration, submit register and mine into the chain
  {
    daemon_prepare_registration_params params           = {};
    params.contributors[params.num_contributors].amount = FAKECHAIN_STAKING_REQUIREMENT;
    wallet_address(&wallet, 0, &params.contributors[params.num_contributors++].addr);

    loki_scratch_buf cmd = {};
    EXPECT(result, daemon_prepare_registration (&daemon, &params, &cmd), "Failed to prepare registration");
    EXPECT(result, wallet_register_service_node(&wallet, cmd.c_str),     "Failed to register service node");
    daemon_mine_n_blocks(&daemon, &wallet, 1); // Mine and become a service node
  }

  daemon_snode_status snode_status = daemon_print_sn_status(&daemon);
  EXPECT(result, snode_status.registered, "Node submitted registration but did not become a service node");

  EXPECT(result, wallet_request_stake_unlock(&wallet, &snode_key), "Failed to request our first stake unlock");
  EXPECT(result, wallet_request_stake_unlock(&wallet, &snode_key) == false, "We've already requested our stake to unlock, the second time should fail");
  return result;
}

test_result latest__stake__allow_incremental_stakes_with_1_contributor()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon = {};
  wallet_t wallet = {};
  LOKI_DEFER { wallet_exit(&wallet); daemon_exit(&daemon); };

  loki_addr my_addr = {};
  helper_setup_blockchain_with_n_blocks(&result, &daemon, &wallet, &my_addr, MIN_BLOCKS_IN_BLOCKCHAIN);

  loki_snode_key snode_key = {};
  EXPECT(result, daemon_print_sn_key(&daemon, &snode_key), "We should be able to query the service node key, was the daemon launched in --service-node mode?");

  // Register the service node
  {
    daemon_prepare_registration_params params             = {};
    params.open_pool                                      = true;
    params.contributors[params.num_contributors].addr     = my_addr;
    params.contributors[params.num_contributors++].amount = 25;

    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration (&daemon, &params, &registration_cmd), "Failed to prepare registration");
    EXPECT(result, wallet_register_service_node(&wallet, registration_cmd.c_str),    "Failed to register service node");
    daemon_mine_n_blocks(&daemon, &wallet, 1);
  }

  // Contributor 1, sends in, 35, 20, 25
  {
    wallet_refresh(&wallet);
    EXPECT(result, wallet_stake(&wallet, &snode_key, 35), "Wallet failed to stake amount: 35");
    daemon_mine_n_blocks(&daemon, &wallet, 1);

    wallet_refresh(&wallet);
    EXPECT(result, wallet_stake(&wallet, &snode_key, 20), "Wallet failed to stake amount: 20");
    daemon_mine_n_blocks(&daemon, &wallet, 1);

    wallet_refresh(&wallet);
    EXPECT(result, wallet_stake(&wallet, &snode_key, 25), "Wallet failed to stake amount: 25");
    daemon_mine_n_blocks(&daemon, &wallet, 1);
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
    daemon                            = create_and_start_daemon(daemon_params, result.name.c_str);

    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;

    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallets))
    {
      wallets[i] = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
      EXPECT(result, wallet_address(wallets + i, 0, wallet_addrs + i), "Failed to query wallet: %d's primary address", (int)i);
      wallet_set_default_testing_settings(wallets + i);
    }

    dummy_wallet = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
    wallet_set_default_testing_settings(&dummy_wallet);
    EXPECT(result, wallet_address(&dummy_wallet, 0, &dummy_addr),  "Failed to get the 0th subaddress, i.e the main address of wallet");

    LOKI_FOR_EACH(i, LOKI_ARRAY_COUNT(wallets))
      daemon_mine_n_blocks(&daemon, wallets + i, 5);

    daemon_mine_until_height(&daemon, wallets + 0, 200);
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
  daemon_mine_n_blocks(&daemon, &dummy_wallet, 1); // Get registration onto the chain

  // Wallet 1 contributes 40, the minimum contribution should be (50 loki remaining/ 3 key images remaining)
  {
    wallet_refresh(wallets + 1);
    EXPECT(result, wallet_stake(wallets + 1, &snode_key, 40), "Wallet failed to stake");
    daemon_mine_n_blocks(&daemon, &dummy_wallet, 1);
  }

  // Wallet 2 contributes 8, the minimum contribution should be (10 loki remaining/ 2 key images remaining)
  {
    wallet_refresh(wallets + 2);
    EXPECT(result, wallet_stake(wallets + 2, &snode_key, 8), "Wallet failed to stake");
    daemon_mine_n_blocks(&daemon, &dummy_wallet, 1);
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
    daemon_mine_n_blocks(&daemon, &dummy_wallet, 1);
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
  daemon_mine_n_blocks(&daemon, &dummy_wallet, 1); // Get unlocks

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
  daemon_mine_until_height(&daemon, &dummy_wallet, unlock_height + LOKI_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW + 1);
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
  daemon_mine_n_blocks(&daemon, &dummy_wallet, LOKI_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW + 2); // NOTE: +1 to get tx on chain
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

  daemon_t daemon          = {};
  wallet_t wallet          = {};
  loki_addr my_addr        = {};
  loki_snode_key snode_key = {};
  LOKI_DEFER { wallet_exit(&wallet); daemon_exit(&daemon); };
  helper_setup_blockchain_with_1_service_node(&result, &daemon, &wallet, &my_addr, &snode_key);

  daemon_mine_n_blocks(&daemon, &wallet, 1); // Get registration onto the chain
  EXPECT(result, wallet_sweep_all(&wallet, my_addr.buf.c_str, nullptr), "Sweeping all outputs should avoid any key images/outputs that are locked and so should succeed.");
  return result;
}

test_result latest__stake__disallow_staking_less_than_minimum_in_pooled_node()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.load_latest_hardfork_versions();

  daemon_t daemon           = create_and_start_daemon(daemon_params, result.name.c_str);
  wallet_t  stakers[2]      = {};
  loki_addr stakers_addr[2] = {};

  // Mine and unlock funds
  for (int i = 0; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
  {
    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    stakers[i]                        = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
    wallet_t *staker                  = stakers + i;

    wallet_set_default_testing_settings(staker);
    wallet_address(staker, 0, stakers_addr + i);
    daemon_mine_n_blocks(&daemon, staker, 10);
  }

  LOKI_DEFER
  {
    daemon_exit(&daemon);
    for (int i = 0; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
      wallet_exit(stakers + i);
  };

  wallet_t *owner = stakers + 0;
  daemon_mine_until_height(&daemon, owner, 200);

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
    daemon_mine_n_blocks(&daemon, owner, 1);
  }

  uint64_t num_locked_contributions                = 1;
  uint64_t min_node_contribution                   = half_stake_requirement / (LOKI_MAX_LOCKED_KEY_IMAGES - num_locked_contributions);
  uint64_t amount_reserved_but_not_contributed_yet = registration_params.contributors[1].amount;

  wallet_refresh(stakers + 1);
  EXPECT(result, !wallet_stake(stakers + 1, &snode_key, 1), "Reserved contributor, but, cannot stake lower than the reserved amount/min node amount, which is MAX((staking_requirement/(4 max_key_images)), reserved_amount)");
  EXPECT(result, !wallet_stake(stakers + 1, &snode_key, min_node_contribution), "Reserved contributor, but, cannot stake lower than the reserved amount/min node amount, which is MAX((staking_requirement/(4 max_key_images)), reserved_amount)");
  EXPECT(result, wallet_stake(stakers + 1, &snode_key, amount_reserved_but_not_contributed_yet), "Reserved contributor failed to stake the correct amount");

  daemon_mine_n_blocks(&daemon, stakers + 1, 1);
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

  daemon_t daemon           = create_and_start_daemon(daemon_params, result.name.c_str);
  wallet_t  stakers[2]      = {};
  loki_addr stakers_addr[2] = {};

  // Mine and unlock funds
  for (int i = 0; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
  {
    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    stakers[i]                        = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
    wallet_t *staker                  = stakers + i;

    wallet_set_default_testing_settings(staker);
    wallet_address(staker, 0, stakers_addr + i);
    daemon_mine_n_blocks(&daemon, staker, 10);
  }

  LOKI_DEFER
  {
    daemon_exit(&daemon);
    for (int i = 0; i < (int)LOKI_ARRAY_COUNT(stakers); ++i)
      wallet_exit(stakers + i);
  };

  wallet_t *owner = stakers + 0;
  daemon_mine_until_height(&daemon, owner, 200);

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
    daemon_mine_n_blocks(&daemon, owner, 1);
  }

  EXPECT(result, !wallet_stake(owner, &snode_key, 50), "The node is completely reserved, the owner should not be able to contribute more into the node.");
  return result;
}

test_result latest__stake__disallow_to_non_registered_node()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon = {};
  wallet_t wallet = {};
  LOKI_DEFER { wallet_exit(&wallet); daemon_exit(&daemon); };

  loki_addr my_addr = {};
  helper_setup_blockchain_with_n_blocks(&result, &daemon, &wallet, &my_addr, 200);

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
  daemon_t daemon = create_and_start_daemon(daemon_params, result.name.c_str);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;
  wallet_t src_wallet               = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
  wallet_t dest_wallet              = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
  LOKI_DEFER { daemon_exit(&daemon); wallet_exit(&src_wallet); wallet_exit(&dest_wallet); };

  wallet_set_default_testing_settings(&src_wallet);
  wallet_set_default_testing_settings(&dest_wallet);
  daemon_mine_n_blocks(&daemon, &src_wallet, 100);

  loki_addr src_addr = {}, dest_addr = {};
  LOKI_ASSERT(wallet_address(&src_wallet, 0,  &src_addr));
  LOKI_ASSERT(wallet_address(&dest_wallet, 0, &dest_addr));

  int64_t const fee_estimate = 2170050;
  int64_t const epsilon      = 1000000;

  // Transfer from wallet to dest
  {
    loki_transaction tx = {};
    EXPECT(result, wallet_transfer(&src_wallet, dest_addr.buf.c_str, 50, &tx), "Failed to construct TX");
    daemon_mine_n_blocks(&daemon, &src_wallet, LOKI_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW + 1); // NOTE: +1 to get TX on chain

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
test_result v10__prepare_registration__check_all_solo_stake_forms_valid_registration()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.add_sequential_hardforks_until_version(10);

  daemon_t daemon = create_and_start_daemon(daemon_params, result.name.c_str);
  LOKI_DEFER { daemon_exit(&daemon); };

  // NOTE(loki): Mine enough to reach the latest fork
  {
    start_wallet_params wallet_params = {};
    wallet_params.daemon              = &daemon;
    wallet_t wallet                   = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
    wallet_set_default_testing_settings(&wallet);
    daemon_mine_n_blocks(&daemon, &wallet, 32);
    wallet_exit(&wallet);
  }

  char const *wallet1 = LOKI_MAINNET_ADDR[0];

  daemon_prepare_registration_params registration_params = {};
  registration_params.num_contributors                   = 1;
  registration_params.contributors[0].addr.set_normal_addr(wallet1);

  for (int i = 0; i < 101; ++i) // TODO(doyle): Assumes staking requirement
  {
    registration_params.contributors[0].amount = i;
    loki_scratch_buf registration_cmd = {};
    EXPECT(result, daemon_prepare_registration(&daemon, &registration_params, &registration_cmd), "Failed to prepare registration");

    // Expected Format: register_service_node <operator cut> <address> <fraction> [<address> <fraction> [...]]]
    char const *register_str      = str_find(registration_cmd.c_str, "register_service_node");
    char const *prev              = register_str;

    char const *operator_portions = str_skip_to_next_word_inplace(&prev);
    char const *wallet_addr       = str_skip_to_next_word_inplace(&prev);
    char const *addr1_portions    = str_skip_to_next_word_inplace(&prev);

    EXPECT_STR(result, register_str,      "register_service_node", "Could not find expected str in: %s", register_str);
    EXPECT_STR(result, operator_portions, "18446744073709551612",  "Could not find expected str in: %s", register_str);
    EXPECT_STR(result, wallet_addr,       wallet1,                 "Could not find expected str in: %s", register_str);
    EXPECT_STR(result, addr1_portions,    "18446744073709551612",  "Could not find expected str in: %s", register_str);
  }
  return result;
}

test_result v10__register_service_node__check_gets_payed_expires_and_returns_funds()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  start_daemon_params daemon_params = {};
  daemon_params.add_sequential_hardforks_until_version(10);
  daemon_t daemon                   = create_and_start_daemon(daemon_params, result.name.c_str);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;
  wallet_t master_wallet            = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
  wallet_t staker1                  = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
  wallet_t staker2                  = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);

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
  daemon_mine_n_blocks(&daemon, &master_wallet, 100);
  wallet_mine_until_unlocked_balance(&master_wallet, &daemon, 150 * LOKI_ATOMIC_UNITS);
  wallet_transfer(&master_wallet, staker1_addr.buf.c_str, 50 + 1, nullptr);
  wallet_transfer(&master_wallet, staker2_addr.buf.c_str, 50 + 1, nullptr);
  daemon_mine_n_blocks(&daemon, &master_wallet, LOKI_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW + 1); // Get TX's onto the chain and let unlock it for the stakers

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
    daemon_mine_n_blocks(&daemon, &master_wallet, 1); // Get registration onto the chain
    EXPECT(result, wallet_stake(&staker2, &snode_key, 50), "Staker 2 failed to stake to service node");
  }

  // TODO(doyle): assuming fakechain staking duration is 30 blocks + lock blocks excess.
  daemon_mine_n_blocks(&daemon, &master_wallet, 5 + 30 + LOKI_STAKING_EXCESS_BLOCKS); // Get node onto chain and mine until expiry

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
  daemon_params.fixed_difficulty    = 3;
  daemon_params.add_hardfork(7,   0);
  daemon_params.add_hardfork(8,  10);
  daemon_params.add_hardfork(9,  20);
  daemon_params.add_hardfork(10, 30);
  daemon_t daemon = create_and_start_daemon(daemon_params, result.name.c_str);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;
  wallet_t wallet                   = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
  LOKI_DEFER { daemon_exit(&daemon); wallet_exit(&wallet); };
  wallet_set_default_testing_settings(&wallet);

  // Mine enough funds for a registration
  loki_addr my_addr = {};
  {
    daemon_mine_until_height(&daemon, &wallet, MIN_BLOCKS_IN_BLOCKCHAIN);
    wallet_address(&wallet, 0, &my_addr);
    wallet_sweep_all(&wallet, my_addr.buf.c_str, nullptr);
    wallet_mine_until_unlocked_balance(&wallet, &daemon, 100 * LOKI_ATOMIC_UNITS);
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
    daemon_mine_n_blocks(&daemon, &wallet, 1);

    // Check service node registered
    daemon_snode_status node_status = daemon_print_sn_status(&daemon);
    EXPECT(result, node_status.registered, "Service node was not registered properly");
  }

  // TODO(doyle): Assumes fakechain
  // Mine until within the grace re-registration range
  {
    int const STAKING_DURATION = 30; // TODO(doyle): Workaround for inability to print_sn_status and see the actual expiry
    daemon_status_t status     = daemon_status(&daemon);
    daemon_mine_until_height(&daemon, &wallet, status.height + STAKING_DURATION - 1);

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
    daemon_mine_n_blocks(&daemon, &wallet, 1);

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
  daemon_t daemon                   = create_and_start_daemon(daemon_params, result.name.c_str);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;
  wallet_t wallet                   = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);

  wallet_set_default_testing_settings(&wallet);
  LOKI_DEFER { wallet_exit(&wallet); daemon_exit(&daemon); };

  loki_addr my_addr = {};
  daemon_mine_n_blocks(&daemon, &wallet, 100);
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
  daemon_mine_n_blocks(&daemon, &wallet, 1);      // Get registration onto chain

  EXPECT(result, wallet_stake(&wallet, &snode_key, 15), "Failed to stake 15 loki to node: %s", snode_key.c_str);
  daemon_mine_n_blocks(&daemon, &wallet, 1);      // Get registration onto chain
  EXPECT(result, wallet_stake(&wallet, &snode_key, 15), "Failed to stake 15 loki to node: %s", snode_key.c_str);
  daemon_mine_n_blocks(&daemon, &wallet, 1);      // Get registration onto chain
  EXPECT(result, wallet_stake(&wallet, &snode_key, 20), "Failed to stake 20 loki to node: %s", snode_key.c_str);
  daemon_mine_n_blocks(&daemon, &wallet, 1);      // Get registration onto chain

  EXPECT(result, wallet_stake(&wallet, &snode_key, 20) == false, "The node: %s should already be registered, staking further should be disallowed!", snode_key.c_str);
  return result;
}

test_result v10__stake__allow_insufficient_stake_w_reserved_contributor()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);
  start_daemon_params daemon_params = {};
  daemon_params.add_sequential_hardforks_until_version(10);
  daemon_t daemon                   = create_and_start_daemon(daemon_params, result.name.c_str);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;

  wallet_t wallet1 = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
  wallet_t wallet2 = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
  LOKI_DEFER { daemon_exit(&daemon); wallet_exit(&wallet1); wallet_exit(&wallet2); };

  // Collect funds for each wallet
  loki_addr wallet1_addr = {};
  loki_addr wallet2_addr = {};
  {
    wallet_set_default_testing_settings(&wallet1);
    wallet_address(&wallet1, 0, &wallet1_addr);
    daemon_mine_n_blocks(&daemon, &wallet1, 50);

    wallet_set_default_testing_settings(&wallet2);
    wallet_address(&wallet2, 0, &wallet2_addr);
    daemon_mine_n_blocks(&daemon, &wallet2, 50);
  }

  // Put the registration onto the blockchain
  {
    wallet_refresh(&wallet1);
    loki_scratch_buf registration_cmd                     = {};
    daemon_prepare_registration_params params             = {};
    params.open_pool                                      = true;
    params.contributors[params.num_contributors].addr     = wallet1_addr;
    params.contributors[params.num_contributors++].amount = 25;
    params.contributors[params.num_contributors].addr     = wallet2_addr;
    params.contributors[params.num_contributors++].amount = 50;
    daemon_prepare_registration(&daemon, &params, &registration_cmd);

    wallet_register_service_node(&wallet1, registration_cmd.c_str);
    daemon_mine_n_blocks(&daemon, &wallet1, 1);

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
  daemon_t daemon                   = create_and_start_daemon(daemon_params, result.name.c_str);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;

  // Setup wallet 1 service node
  loki_addr main_addr  = {};
  wallet_t owner       = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
  wallet_t contributor = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
  LOKI_DEFER { daemon_exit(&daemon); wallet_exit(&owner); wallet_exit(&contributor); };

  {
    wallet_set_default_testing_settings(&owner);
    wallet_address(&owner, 0, &main_addr);
    daemon_mine_n_blocks(&daemon, &owner, 100);
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
    daemon_mine_n_blocks(&daemon, &owner, 1);

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
    wallet_mine_until_unlocked_balance(&contributor, &daemon, 20);

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
  daemon_t daemon = create_and_start_daemon(daemon_params, result.name.c_str);

  start_wallet_params wallet_params = {};
  wallet_params.daemon              = &daemon;
  wallet_t src_wallet               = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
  wallet_t dest_wallet              = create_and_start_wallet(daemon_params.nettype, wallet_params, result.name.c_str);
  LOKI_DEFER { daemon_exit(&daemon); wallet_exit(&src_wallet); wallet_exit(&dest_wallet); };

  wallet_set_default_testing_settings(&src_wallet);
  wallet_set_default_testing_settings(&dest_wallet);
  daemon_mine_n_blocks(&daemon, &src_wallet, 100);

  loki_addr src_addr = {}, dest_addr = {};
  LOKI_ASSERT(wallet_address(&src_wallet,  0, &src_addr));
  LOKI_ASSERT(wallet_address(&dest_wallet, 0, &dest_addr));

  int64_t const fee_estimate = 71639960;
  int64_t const epsilon      = 10000000;

  // Transfer from wallet to dest
  {
    loki_transaction tx = {};
    EXPECT(result, wallet_transfer(&src_wallet, dest_addr.buf.c_str, 50, &tx), "Failed to construct TX");
    daemon_mine_n_blocks(&daemon, &src_wallet, LOKI_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW + 1); // NOTE: +1 to get TX on chain

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

