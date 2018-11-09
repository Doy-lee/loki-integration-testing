#include "integration_tests.h"
#include "test_cases.h"

#define SHOOM_IMPLEMENTATION
#include "external/shoom.h"

#include <chrono>
#include <thread>

#define STB_SPRINTF_IMPLEMENTATION
#include "external/stb_sprintf.h"

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
  assert(total_len < msg_buf_len);

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
  assert(ptr < msg_buf + msg_buf_len);
  return ptr;
}

void write_to_stdin_mem(shared_mem_type type, char const *cmd, int cmd_len)
{
  shoom::Shm *shared_mem = (type == shared_mem_type::wallet) ? &global_state.wallet_stdin_shared_mem : &global_state.daemon_stdin_shared_mem;

  if (cmd_len == -1) cmd_len = static_cast<int>(strlen(cmd));
  assert(cmd_len < shared_mem->Size());
  make_message((char *)shared_mem->Data(), shared_mem->Size(), cmd, cmd_len);
}

loki_scratch_buf read_from_stdout_mem(shared_mem_type type)
{
  static uint64_t last_timestamp = 0;
  uint64_t timestamp             = 0;
  char const *output             = nullptr;

  shoom::Shm *shared_mem = (type == shared_mem_type::wallet) ? &global_state.wallet_stdout_shared_mem : &global_state.daemon_stdout_shared_mem;
  for(;;)
  {
    shared_mem->Open();
    char *data = reinterpret_cast<char *>(shared_mem->Data());
    output = parse_message(data, shared_mem->Size(), &timestamp);
    if (output && last_timestamp != timestamp)
    {
      last_timestamp = timestamp;
      break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
  }

  loki_scratch_buf result = {};
  result.len         = strlen(output);
  assert(result.len <= result.max());
  memcpy(result.data, output, result.len);
  return result;
}

loki_scratch_buf write_to_stdin_mem_and_get_result(shared_mem_type type, char const *cmd, int cmd_len)
{
  write_to_stdin_mem(type, cmd, cmd_len);
  loki_scratch_buf result = read_from_stdout_mem(type);
  return result;
}

daemon_t start_daemon()
{
  char arg_buf[1024], cmd_buf[1024];
  daemon_t result     = {};
  result.p2p_port     = global_state.free_p2p_port++;
  result.rpc_port     = global_state.free_rpc_port++;
  result.zmq_rpc_port = global_state.free_zmq_rpc_port++;
  stbsp_snprintf(arg_buf, LOKI_ARRAY_COUNT(arg_buf), "--testnet --p2p-bind-port %d --rpc-bind-port %d --zmq-rpc-bind-port %d --data-dir Bin/daemon%d --offline --service-node --fixed-difficulty 25",
                 result.p2p_port, result.rpc_port, result.zmq_rpc_port, global_state.num_daemons++);
  stbsp_snprintf(cmd_buf, LOKI_ARRAY_COUNT(cmd_buf), LOKI_CMD_FMT, arg_buf);

  result.proc_handle = os_launch_process(cmd_buf);
  return result;
}

wallet_t create_wallet()
{
  char arg_buf[1024], cmd_buf[1024];
  wallet_t result    = {};
  result.id          = global_state.num_wallets++;
  stbsp_snprintf(arg_buf, LOKI_ARRAY_COUNT(arg_buf), "--generate-new-wallet Bin/wallet_%d --testnet --password '' --mnemonic-language English save", result.id);
  stbsp_snprintf(cmd_buf, LOKI_ARRAY_COUNT(cmd_buf), LOKI_WALLET_CMD_FMT, arg_buf);
  return result;
}

void start_wallet(wallet_t *wallet)
{
  char arg_buf[1024], cmd_buf[1024];
  stbsp_snprintf(arg_buf, LOKI_ARRAY_COUNT(arg_buf), "--wallet-file Bin/wallet_%d --testnet --password '' --mnemonic-language English", wallet->id);
  stbsp_snprintf(cmd_buf, LOKI_ARRAY_COUNT(cmd_buf), LOKI_WALLET_CMD_FMT, arg_buf);
  wallet->proc_handle = os_launch_process(cmd_buf);
}

void reset_testing_framework()
{
  global_state.daemon_stdin_shared_mem.Create (shoom::Flag::create | shoom::Flag::clear_on_create);
  global_state.wallet_stdin_shared_mem.Create (shoom::Flag::create | shoom::Flag::clear_on_create);
  global_state.daemon_stdout_shared_mem.Create(shoom::Flag::create | shoom::Flag::clear_on_create);
  global_state.wallet_stdout_shared_mem.Create(shoom::Flag::create | shoom::Flag::clear_on_create);
  printf("Shared memory reset! Integration test framework starting.\n");
}

#include <iostream>
int main(int, char **)
{
  reset_testing_framework();
  daemon_t daemon = start_daemon();
  // TODO(doyle): Give enough time for daemon/wallet to start up and see that
  // the shared_mem_region has been zeroed out. Otherwise this will power on
  // forward and write to stdin before it initialises, and then the daemon will
  // block waiting for it to be cleared out before proceeding.
  std::this_thread::sleep_for(std::chrono::milliseconds(2 * 1000));
  printf("\n");
#if 0
  std::string line;
  for (;;line.clear())
  {
    std::getline(std::cin, line);
    loki_scratch_buf result = write_to_stdin_mem_and_get_result(shared_mem_type::wallet, line.c_str(), line.size());
    printf("%s\n", result.data);
  }
#else
  // prepare_registration__solo_auto_stake().print_result();
  //prepare_registration__100_percent_operator_cut_auto_stake().print_result();
  stake__from_subaddress().print_result();
#endif

  write_to_stdin_mem(shared_mem_type::daemon, "exit");
  write_to_stdin_mem(shared_mem_type::wallet, "exit");
  return 0;
}
