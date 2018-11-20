#include "loki_integration_tests.h"
#include "loki_test_cases.h"

#define SHOOM_IMPLEMENTATION
#include "external/shoom.h"

#include <chrono>
#include <thread>

#define STB_SPRINTF_IMPLEMENTATION
#include "external/stb_sprintf.h"

#include "loki_daemon.h"

static char global_temp_buf[8192];

FILE *os_launch_process(char const *cmd_line)
{
  FILE *result = nullptr;
#ifdef _WIN32
  result = _popen(cmd_line, "r");
#else
  result = popen(cmd_line, "r");
#endif
  return result;
}

// TODO(loki): This doesn't work.
void os_kill_process(FILE *process)
{
#ifdef _WIN32
  _pclose(process);
#else
  pclose(process);
#endif
}

#ifdef _WIN32
char const LOKI_CMD_FMT[]        = "start bin/lokid.exe %s";
char const LOKI_WALLET_CMD_FMT[] = "start bin/loki-wallet-cli.exe %s";
#else
char const LOKI_CMD_FMT[]        = "lxterminal -e bash -c \"/home/loki/Loki/Code/loki-integration-test/bin/lokid %s; bash\"";
char const LOKI_WALLET_CMD_FMT[] = "lxterminal -e bash -c \"/home/loki/Loki/Code/loki-integration-test/bin/loki-wallet-cli %s; bash\"";
#endif

static state_t global_state;
uint32_t const MSG_MAGIC_BYTES = 0x7428da3f;

static void make_message(char *msg_buf, int msg_buf_len, char const *msg_data, int msg_data_len)
{
  uint64_t timestamp = time(nullptr);
  int total_len      = static_cast<int>(sizeof(timestamp) + sizeof(MSG_MAGIC_BYTES) + msg_data_len);
  LOKI_ASSERT(total_len < msg_buf_len);

  char *ptr = msg_buf;
  memcpy(ptr, &timestamp, sizeof(timestamp));
  ptr += sizeof(timestamp);

  memcpy(ptr, (char *)&MSG_MAGIC_BYTES, sizeof(MSG_MAGIC_BYTES));
  ptr += sizeof(MSG_MAGIC_BYTES);

  memcpy(ptr, msg_data, msg_data_len);
  ptr += sizeof(msg_data);

  msg_buf[total_len] = 0;
}

static char const *parse_message(char const *msg_buf, int msg_buf_len, uint64_t *timestamp)
{
  char const *ptr = msg_buf;
  *timestamp = *((uint64_t const *)ptr);
  ptr += sizeof(*timestamp);

  if ((*(uint32_t const *)ptr) != MSG_MAGIC_BYTES)
    return nullptr;

  ptr += sizeof(MSG_MAGIC_BYTES);
  LOKI_ASSERT(ptr < msg_buf + msg_buf_len);
  return ptr;
}

void start_daemon_params::add_hardfork(int version, int height)
{
  LOKI_ASSERT_MSG(this->num_hardforks < LOKI_ARRAY_COUNT(this->hardforks), "Out of space in hardfork storage, bump up the hardfork array size, current size: %zu", LOKI_ARRAY_COUNT(this->hardforks));

  if (this->num_hardforks > 0)
  {
    loki_hardfork const *prev_hardfork = this->hardforks + this->num_hardforks;
    LOKI_ASSERT_MSG(prev_hardfork->version < version, "Expected new hardfork version to be less than the previously added hardfork, prev version: %d, new: %d", prev_hardfork->version, version);
    LOKI_ASSERT_MSG(prev_hardfork->height  < height,  "Expected new hardfork height to be less than the previously added hardfork, prev height: %d, new: %d", prev_hardfork->height, height);
  }

  this->hardforks[this->num_hardforks++] = {version, height};


  // TODO(doyle): This sucks. But, right now overriding hard forks in the daemon
  // means we need to go into fakenet mode. There's a refresh error I haven't
  // figured out yet.
  this->nettype = loki_nettype::fakenet;
}

void itest_write_to_stdin_mem(itest_shared_mem_type type, char const *cmd, int cmd_len)
{
  shoom::Shm *shared_mem = (type == itest_shared_mem_type::wallet) ? &global_state.wallet_stdin_shared_mem : &global_state.daemon_stdin_shared_mem;

  if (cmd_len == -1) cmd_len = static_cast<int>(strlen(cmd));
  LOKI_ASSERT(cmd_len < shared_mem->Size());
  make_message((char *)shared_mem->Data(), shared_mem->Size(), cmd, cmd_len);

  // TODO(doyle): Make it so we never need to sleep, or reduce this number as
  // much as possible. This is our bottleneck. But right now, if we write and
  // read too fast, we might skip certain outputs and get blocked waiting for
  // a result.
  os_sleep_ms(600);
}

loki_scratch_buf itest_read_from_stdout_mem(itest_shared_mem_type type)
{
  static uint64_t wallet_last_timestamp = 0;
  static uint64_t daemon_last_timestamp = 0;
  static loki_scratch_buf last_output   = {};
  uint64_t *last_timestamp = nullptr;

  shoom::Shm *shared_mem = (type == itest_shared_mem_type::wallet) ? &global_state.wallet_stdout_shared_mem : &global_state.daemon_stdout_shared_mem;

  if (type == itest_shared_mem_type::wallet) last_timestamp = &wallet_last_timestamp;
  else                                 last_timestamp = &daemon_last_timestamp;

  char const *output       = nullptr;
  for(uint64_t timestamp = 0;; timestamp = 0)
  {
    // TODO(doyle): Make it so we never need to sleep, or reduce this number as
    // much as possible. This is our bottleneck. But right now, if we write and
    // read too fast, we might skip certain outputs and get blocked waiting for
    // a result.
    os_sleep_ms(600);
    shared_mem->Open();

    // NOTE: If commands are completed quickly the timestamps may still be the same.
    // TODO(doyle): We should just do an atomic cmd count variable

    char *data = reinterpret_cast<char *>(shared_mem->Data());
    output = parse_message(data, shared_mem->Size(), &timestamp);
    if (output && ((*last_timestamp) != timestamp || strcmp(output, last_output.c_str) != 0))
    {
      (*last_timestamp) = timestamp;
      last_output       = output;
      break;
    }
  }

  loki_scratch_buf result = {};
  result.len         = strlen(output);
  LOKI_ASSERT(result.len <= result.max());
  memcpy(result.data, output, result.len);
  return result;
}

loki_scratch_buf itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type type, char const *cmd, int cmd_len)
{
  itest_write_to_stdin_mem(type, cmd, cmd_len);
  loki_scratch_buf result = itest_read_from_stdout_mem(type);
  return result;
}

void os_sleep_s(int seconds)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(seconds * 1000));
}

void os_sleep_ms(int ms)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

bool os_file_delete(char const *path)
{
#if defined(_WIN32)
    bool result = DeleteFileA(path);
#else
    bool result = (unlink(path) == 0);
#endif
    return result;
}

void start_daemon(daemon_t *daemon, start_daemon_params params)
{
  loki_scratch_buf arg_buf = {};
  arg_buf.append("--p2p-bind-port %d ",            daemon->p2p_port);
  arg_buf.append("--rpc-bind-port %d ",            daemon->rpc_port);
  arg_buf.append("--zmq-rpc-bind-port %d ",        daemon->zmq_rpc_port);
  arg_buf.append("--data-dir ./output/daemon_%d ", daemon->id);
  arg_buf.append("--offline ");

  if (params.service_node)
    arg_buf.append("--service-node ");

  if (params.fixed_difficulty > 0)
    arg_buf.append("--fixed-difficulty %d ", params.fixed_difficulty);

  if (params.num_hardforks > 0)
  {
    arg_buf.append("--integration-test-hardforks-override \\\"");
    for (int i = 0; i < params.num_hardforks; ++i)
    {
      loki_hardfork hardfork = params.hardforks[i];
      arg_buf.append("%d:%d", hardfork.version, hardfork.height);
      if (i != (params.num_hardforks - 1)) arg_buf.append(", ");
    }

    arg_buf.append("\\\" ");
    LOKI_ASSERT(params.nettype == loki_nettype::fakenet);
  }
  else
  {
    if (params.nettype == loki_nettype::testnet)
      arg_buf.append("--testnet ");
    else if (params.nettype == loki_nettype::stagenet)
      arg_buf.append("--stagnet ");
  }

  loki_scratch_buf cmd_buf(LOKI_CMD_FMT, arg_buf.data);
  itest_reset_shared_memory(itest_reset_type::daemon);
  daemon->proc_handle = os_launch_process(cmd_buf.data);
  os_sleep_ms(1000); // TODO(doyle): HACK to let enough time for the daemon to init
}

// L6cuiMjoJs7hbv5qWYxCnEB1WJG827jrDAu4wEsrsnkYVu3miRHFrBy9pGcYch4TDn6dgatqJugUafWxCAELiq461p5mrSa
// T6U4ukY68vohsfrGMryFmqX5yRE4d5EC8E6QbinSo8ssW3heqoNjgNggTeym9NSLW4cnEp3ckpD9RZLW5qDGg3821c9SAtHMD

daemon_t create_and_start_daemon(start_daemon_params params)
{
  daemon_t result = create_daemon();
  start_daemon(&result, params);
  return result;
}

wallet_t create_and_start_wallet(loki_nettype type, start_wallet_params params)
{
  wallet_t result = create_wallet(type);
  start_wallet(&result, params);
  return result;
}

daemon_t create_daemon()
{
  static int p2p_port = 1111;
  static int rpc_port = 2222;
  static int zmq_port = 3333;

  daemon_t result     = {};
  result.id           = global_state.num_daemons++;
  result.p2p_port     = p2p_port++;
  result.rpc_port     = rpc_port++;
  result.zmq_rpc_port = zmq_port++;
  return result;
}

wallet_t create_wallet(loki_nettype nettype)
{
  wallet_t result = {};
  result.id       = global_state.num_daemons++;
  result.nettype  = nettype;

  loki_scratch_buf arg_buf = {};
  if (result.nettype == loki_nettype::testnet)
    arg_buf.append("--testnet ");
  else if (result.nettype == loki_nettype::fakenet)
    arg_buf.append("--fakenet ");
  else if (result.nettype == loki_nettype::stagenet)
    arg_buf.append("--stagenet ");

  arg_buf.append("--generate-new-wallet ./output/wallet_%d ", result.id);
  arg_buf.append("--password '' ");
  arg_buf.append("--mnemonic-language English ");
  arg_buf.append("save ");

  itest_reset_shared_memory(itest_reset_type::wallet);
  loki_scratch_buf cmd_buf(LOKI_WALLET_CMD_FMT, arg_buf.data);
  os_launch_process(cmd_buf.data);
  os_sleep_ms(2000); // TODO(doyle): HACK to let enough time for the wallet to init, save and close.
  return result;
}

void start_wallet(wallet_t *wallet, start_wallet_params params)
{
  loki_scratch_buf arg_buf = {};
  arg_buf.append("--wallet-file ./output/wallet_%d ", wallet->id);
  arg_buf.append("--password '' ");
  arg_buf.append("--mnemonic-language English ");

  if (params.allow_mismatched_daemon_version)
    arg_buf.append("--allow-mismatched-daemon-version ");

  if (params.daemon)
    arg_buf.append("--daemon-address 127.0.0.1:%d ", params.daemon->rpc_port);

  if (wallet->nettype == loki_nettype::testnet)
    arg_buf.append("--testnet ");
  else if (wallet->nettype == loki_nettype::fakenet)
    arg_buf.append("--fakenet ");
  else if (wallet->nettype == loki_nettype::stagenet)
    arg_buf.append("--stagenet ");


  loki_scratch_buf cmd_buf(LOKI_WALLET_CMD_FMT, arg_buf.data);
  wallet->proc_handle = os_launch_process(cmd_buf.data);
  itest_reset_shared_memory(itest_reset_type::wallet);
  os_sleep_ms(2000); // TODO(doyle): HACK to let enough time for the wallet to init, save and close.
}

void itest_reset_shared_memory(itest_reset_type type)
{
  if (type == itest_reset_type::all || type == itest_reset_type::daemon)
  {
    global_state.daemon_stdin_shared_mem.Create (shoom::Flag::create | shoom::Flag::clear_on_create);
    global_state.daemon_stdout_shared_mem.Create(shoom::Flag::create | shoom::Flag::clear_on_create);
    global_state.daemon_stdout_shared_mem.Open();
  }

  if (type == itest_reset_type::all || type == itest_reset_type::wallet)
  {
    global_state.wallet_stdin_shared_mem.Create (shoom::Flag::create | shoom::Flag::clear_on_create);
    global_state.wallet_stdout_shared_mem.Create(shoom::Flag::create | shoom::Flag::clear_on_create);
    global_state.wallet_stdout_shared_mem.Open();
  }

  // TODO(doyle): This is a workaround, for some reason if I start trying to use
  // this shared memory too quick(?) The daemon/wallet isn't able to see that it
  // was initialised.
  std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
}

#include <iostream>
int main(int, char **)
{
  printf("\n");
#if 0
  itest_reset_shared_memory();
  std::string line;
  for (;;line.clear())
  {
    std::getline(std::cin, line);
    loki_scratch_buf result = itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type::wallet, line.c_str(), line.size());
    printf("%s\n", result.data);
  }
#else
  test_result results[128];
  int results_index = 0;

#define RUN_TEST(test_function) \
  results[results_index++] = test_function(); \
  print_test_results(&results[results_index-1])

  // RUN_TEST(prepare_registration__100_percent_operator_cut_auto_stake);
  // RUN_TEST(prepare_registration__solo_auto_stake);
  // RUN_TEST(register_service_node__4_stakers);
  // RUN_TEST(register_service_node__grace_period);
  RUN_TEST(stake__disallow_insufficient_stake_w_not_reserved_contributor);
  // RUN_TEST(stake__from_subaddress);
  // RUN_TEST(transfer__expect_fee_amount);
  // RUN_TEST(transfer__expect_fee_amount_bulletproofs);

  int num_tests_passed = 0;
  for (int i = 0; i < results_index; ++i)
    num_tests_passed += static_cast<int>(!results[i].failed);

  printf("\nTests passed %d/%d\n\n", num_tests_passed, results_index);
#endif

  return 0;
}
