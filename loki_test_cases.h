#ifndef LOKI_TEST_CASES_H
#define LOKI_TEST_CASES_H

struct test_result
{
  loki_fixed_string<512> name;
  bool                   failed;
  loki_fixed_string<>    fail_msg;
  float                  duration_ms;
};

#define INITIALISE_TEST_CONTEXT(test_result_var)                               \
  test_result_var.name = loki_fixed_string<512>(__func__);                           \
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

// NOTE: The minimum amount such that spending funds is reliable and doesn't
// error out with insufficient outputs to select from.
// NOTE: This used to be much lower, like 100, but after the output selection
// algorithm changed, the fake outputs lineup commit this seems to be a lot
// stricter now
const int MIN_BLOCKS_IN_BLOCKCHAIN = 100;
#endif // LOKI_TEST_CASES_H
