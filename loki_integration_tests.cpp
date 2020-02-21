#include <vector>
#include <atomic>
#include <string.h>

#define STB_SPRINTF_IMPLEMENTATION
#include "external/stb_sprintf.h"

#include "loki_integration_tests.h"
#include "loki_str.cpp"

#include "loki_daemon.h"
#include "loki_wallet.h"

#include "loki_os.cpp"
#include "loki_daemon.cpp"
#include "loki_wallet.cpp"

#include "loki_test_cases.h"
#include "loki_test_cases.cpp"

// -------------------------------------------------------------------------------------------------
//
// itest_ipc
//
// -------------------------------------------------------------------------------------------------
char const DAEMON_IPC_NAME[] = "loki_integration_testing_daemon";
char const WALLET_IPC_NAME[] = "loki_integration_testing_wallet";

void itest_ipc_clean_up(itest_ipc *ipc)
{
  close(ipc->read.fd);
  close(ipc->write.fd);
  unlink(ipc->read.file.str);
  unlink(ipc->write.file.str);
}

FILE_SCOPE void itest_ipc_open_pipes(itest_ipc *ipc)
{
  assert(ipc->read.file.len > 0);
  assert(ipc->write.file.len > 0);
  unlink(ipc->read.file.str);
  unlink(ipc->write.file.str);

  if (mknod(ipc->read.file.str, S_IFIFO | 0666, 0) == -1)
  {
    perror("Failed to initialise read pipe");
    assert(false);
  }

  if (mknod(ipc->write.file.str, S_IFIFO | 0666, 0) == -1)
  {
    perror("Failed to initialise write pipe");
    assert(false);
  }

  ipc->write.fd = open(ipc->write.file.str, O_WRONLY);
  if (ipc->write.fd == -1)
  {
    perror("Failed to open write pipe");
    assert(false);
  }
}

FILE_SCOPE itest_ipc itest_ipc_setup(char const *base_name, int id)
{
  itest_ipc result = {};
  result.read.file = loki_fixed_string<128>("%s%d_stdout", base_name, id);
  result.write.file = loki_fixed_string<128>("%s%d_stdin", base_name, id);
  itest_ipc_open_pipes(&result);
  return result;
}

// -------------------------------------------------------------------------------------------------
//
// start_daemon_params
//
// -------------------------------------------------------------------------------------------------
void start_daemon_params::add_sequential_hardforks_until_version(int version)
{
  for (int height = 0, curr_version = 7; curr_version <= version; ++curr_version, ++height)
    add_hardfork(curr_version, height);
}

void start_daemon_params::add_hardfork(int version, int height)
{
  LOKI_ASSERT_MSG(this->num_hardforks < (int)LOKI_ARRAY_COUNT(this->hardforks), "Out of space in hardfork storage, bump up the hardfork array size, current size: %zu", LOKI_ARRAY_COUNT(this->hardforks));

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

void start_daemon_params::load_latest_hardfork_versions()
{
  LOKI_ASSERT(this->num_hardforks == 0);
  this->add_hardfork(7, 0);
  this->add_hardfork(8, 1);
  this->add_hardfork(9, 2);
  this->add_hardfork(10, 3);
  this->add_hardfork(11, 4);
  this->add_hardfork(12, 5);
  this->add_hardfork(13, 6);
}

uint32_t const MSG_PACKET_MAGIC = 0x27befd93;
struct msg_packet
{
  uint32_t magic = MSG_PACKET_MAGIC;
  char buf[1024];
  int  len;
  bool has_more;
};

static char const *make_msg_packet(char const *src, int *len, msg_packet *dest)
{
  *dest                   = {};
  int const max_size      = static_cast<int>(sizeof(dest->buf));
  int const bytes_to_copy = (*len > max_size) ? max_size : *len;

  memcpy(dest->buf, src, bytes_to_copy);
  dest->len = bytes_to_copy;
  *len -= bytes_to_copy;

  char const *result = (*len == 0) ? nullptr : src + bytes_to_copy;
  dest->has_more     = result != nullptr;
  return result;
}

// -------------------------------------------------------------------------------------------------
//
// itest
//
// -------------------------------------------------------------------------------------------------
void itest_write_to_stdin(itest_ipc *ipc, char const *src)
{
  int src_len = static_cast<int>(strlen(src));
  while (src_len > 0)
  {
    msg_packet packet = {};
    src               = make_msg_packet(src, &src_len, &packet);
    int num_bytes_written = write(ipc->write.fd, static_cast<void *>(&packet), sizeof(packet));
    if (num_bytes_written == -1)
    {
      if (errno == EBADF)
      {
        static thread_local bool printed_once = false;
        if (!printed_once)
        {
          perror("Error returned from write(...)");
          printed_once = true;
        }
        return;
      }
      else
      {
        perror("Error returned from write(...)");
      }
    }
  }
}

itest_read_result itest_write_then_read_stdout(itest_ipc *ipc, char const *src)
{
  itest_write_to_stdin(ipc, src);
  itest_read_result result = itest_read_stdout(ipc);
  return result;
}

itest_read_result itest_write_then_read_stdout_until(itest_ipc *ipc, char const *src, itest_read_possible_value const *possible_values, int possible_values_len)
{
  itest_write_to_stdin(ipc, src);
  itest_read_result result = itest_read_stdout_until(ipc, possible_values, possible_values_len);
  return result;
}

itest_read_result itest_write_then_read_stdout_until(itest_ipc *ipc, char const *cmd, loki_string find_str)
{
  itest_read_possible_value possible_values[] = { {find_str, false}, };
  itest_read_result result = itest_write_then_read_stdout_until(ipc, cmd, possible_values, 1);
  return result;
}

itest_read_result itest_read_stdout(itest_ipc *ipc)
{
  if (ipc->read.fd == 0)
  {
    ipc->read.fd = open(ipc->read.file.str, O_RDONLY);
    if (ipc->read.fd == -1)
    {
      perror("Failed to open write pipe");
      assert(false);
    }
  }

  itest_read_result result = {};
  for (;;)
  {
    msg_packet packet = {};
    int bytes_read    = read(ipc->read.fd, reinterpret_cast<void *>(&packet), sizeof(packet));
    if (bytes_read == -1)
    {
      if (errno == EBADF)
      {
        static thread_local bool printed_once = false;
        if (!printed_once)
        {
          perror("Error returned from write(...)");
          printed_once = true;
        }
        return result;
      }
      else
      {
        perror("Error returned from write(...)");
      }
    }

    if (bytes_read < static_cast<int>(sizeof(packet)))
    {
      fprintf(stderr, "Error reading packet from pipe expected=%zu, read=%d, possible that the pipe was cut mid-transmission\n", sizeof(packet), bytes_read);
      exit(-1);
    }

    if (packet.magic != MSG_PACKET_MAGIC)
    {
      fprintf(stderr, "Packet magic value=%x, does not match expected=%x\n", packet.magic, MSG_PACKET_MAGIC);
      exit(-1);
    }

#if 0
    fprintf(stdout, "---- Read packet, len=%d msg=\"%.*s\"\n", packet.len, packet.len, packet.buf);
#endif
    result.buf.append(packet.buf, packet.len);
    if (!packet.has_more) break;
  }
  return result;
}

itest_read_result itest_read_stdout_until(itest_ipc *ipc, char const *find_str)
{
  itest_read_possible_value possible_values[] = { {find_str, false}, };
  itest_read_result result = itest_read_stdout_until(ipc, possible_values, 1);
  return result;
}

void itest_read_stdout_sink(itest_ipc *pipe, int seconds)
{
  // TODO(doyle): implement
}

itest_read_result itest_read_stdout_until(itest_ipc *ipc, itest_read_possible_value const *possible_values, int possible_values_len)
{
  for (;;)
  {
    itest_read_result result = itest_read_stdout(ipc);
    LOKI_FOR_EACH(i, possible_values_len)
    {
      char const *check = result.buf.c_str();
      if (str_find(check, possible_values[i].literal.str))
      {
        result.matching_find_strs_index = i;
        return result;
      }
    }
  }
}

void itest_read_until_then_write_stdin(itest_ipc *ipc, loki_string find_str, char const *cmd)
{
  itest_read_stdout_until(ipc, find_str.str);
  itest_write_to_stdin(ipc, cmd);
}

// -------------------------------------------------------------------------------------------------
//
// daemon
//
// -------------------------------------------------------------------------------------------------
#define XTERM_CMD 1
#if LXTERMINAL_CMD
char const LOKI_CMD_FMT[]        = "lxterminal -t \"daemon_%d %s\" -e bash -c \"./lokid %s; %s \"";
char const LOKI_WALLET_CMD_FMT[] = "lxterminal -t \"wallet_%d %s\" -e bash -c \"./loki-wallet-cli %s; %s \"";
#elif XTERM_CMD
char const LOKI_CMD_FMT[]        = "xterm -T \"daemon_%d %s\" -e bash -c \"./lokid %s; %s \"";
char const LOKI_WALLET_CMD_FMT[] = "xterm -T \"wallet_%d %s\" -e bash -c \"./loki-wallet-cli %s; %s \"";
#endif

struct state_t
{
  std::atomic<int> num_wallets         = 0;
  std::atomic<int> num_daemons         = 0;
  std::atomic<int> free_p2p_port       = 1111;
  std::atomic<int> free_rpc_port       = 2222;
  std::atomic<int> free_zmq_port       = 3333;
  std::atomic<int> free_quorumnet_port = 4444;
};
FILE_SCOPE state_t global_state;

daemon_t create_daemon()
{
  daemon_t result       = {};
  result.id             = global_state.num_daemons++;
  result.p2p_port       = global_state.free_p2p_port++;
  result.rpc_port       = global_state.free_rpc_port++;
  result.zmq_rpc_port   = global_state.free_zmq_port++;
  result.quorumnet_port = global_state.free_quorumnet_port++;
  return result;
}

void start_daemon(daemon_t *daemons, int num_daemons, start_daemon_params *params, int num_params, char const *terminal_name)
{
  // TODO(doyle): Not very efficient
  std::vector<std::thread> threads;
  threads.reserve(num_daemons);

  LOKI_ASSERT(num_params == num_daemons || num_params == 1);
  for (int curr_daemon_index = 0; curr_daemon_index < num_daemons; ++curr_daemon_index)
  {
    start_daemon_params param = (num_params == 1) ? params[0] : params[curr_daemon_index];
    daemon_t *curr_daemon = daemons + curr_daemon_index;

    loki_fixed_string<> arg_buf = {};
    arg_buf.append("--p2p-bind-port %d ",            curr_daemon->p2p_port);
    arg_buf.append("--rpc-bind-port %d ",            curr_daemon->rpc_port);
    arg_buf.append("--zmq-rpc-bind-port %d ",        curr_daemon->zmq_rpc_port);
    arg_buf.append("--quorumnet-port %d ",           curr_daemon->quorumnet_port);
    arg_buf.append("--data-dir ./output/daemon_%d ", curr_daemon->id);
    arg_buf.append("--storage-server-port 4444 ");
    arg_buf.append("--service-node-public-ip 123.123.123.123 ");
    arg_buf.append("--dev-allow-local-ips ");

    if (num_daemons == 1)
      arg_buf.append("--offline ");

    if (param.service_node)
      arg_buf.append("--service-node ");

    if (param.fixed_difficulty > 0)
      arg_buf.append("--fixed-difficulty %d ", param.fixed_difficulty);

    if (param.num_hardforks > 0)
    {
      arg_buf.append("--integration-test-hardforks-override \\\"");
      for (int i = 0; i < param.num_hardforks; ++i)
      {
        loki_hardfork hardfork = param.hardforks[i];
        arg_buf.append("%d:%d", hardfork.version, hardfork.height);
        if (i != (param.num_hardforks - 1)) arg_buf.append(", ");
      }

      arg_buf.append("\\\" ");
      LOKI_ASSERT(param.nettype == loki_nettype::fakenet);
    }
    else
    {
      if (param.nettype == loki_nettype::testnet)
        arg_buf.append("--testnet ");
      else if (param.nettype == loki_nettype::stagenet)
        arg_buf.append("--stagnet ");
    }

    loki_fixed_string<128> name("%s%d", DAEMON_IPC_NAME, curr_daemon->id);
    arg_buf.append("--integration-test-pipe-name %s ", name.str);

    for (int other_daemon_index = 0; other_daemon_index < num_daemons; ++other_daemon_index)
    {
      if (curr_daemon_index == other_daemon_index)
        continue;

      daemon_t const *other_daemon = daemons + other_daemon_index;
      arg_buf.append("--add-exclusive-node 127.0.0.1:%d ", other_daemon->p2p_port);
    }

    arg_buf.append("%s ", param.custom_cmd_line.str);
    if (curr_daemon->id == 1)
    {
      // continue;
    }

    loki_fixed_string<> cmd_buf = {};
    if (param.keep_terminal_open) cmd_buf = loki_fixed_string<>(LOKI_CMD_FMT, curr_daemon->id, terminal_name, arg_buf.str, "bash");
    else                          cmd_buf = loki_fixed_string<>(LOKI_CMD_FMT, curr_daemon->id, terminal_name, arg_buf.str, "");

    threads.push_back(std::thread([curr_daemon, cmd_buf]()
    {
      curr_daemon->proc_handle = os_launch_process(cmd_buf.str);
      curr_daemon->ipc = itest_ipc_setup(DAEMON_IPC_NAME, curr_daemon->id);
      daemon_status(curr_daemon);
    }));
  }

  for (std::thread &daemon_launching_thread : threads)
    daemon_launching_thread.join();
}

daemon_t create_and_start_daemon(start_daemon_params params, char const *terminal_name)
{
  daemon_t result = create_daemon();
  start_daemon(&result, 1, &params, 1, terminal_name);
  return result;
}

void create_and_start_multi_daemons(daemon_t *daemons, int num_daemons, start_daemon_params *params, int num_params, char const *terminal_name)
{
  for (int i = 0; i < num_daemons; ++i)
    daemons[i] = create_daemon();
  start_daemon(daemons, num_daemons, params, num_params, terminal_name);
}

// -------------------------------------------------------------------------------------------------
//
// wallet
//
// -------------------------------------------------------------------------------------------------
wallet_t create_and_start_wallet(loki_nettype type, start_wallet_params params, char const *terminal_name)
{
  wallet_t result = {};
  result.id       = global_state.num_wallets++;
  result.nettype  = type;

  loki_fixed_string<> arg_buf = {};
  if (result.nettype == loki_nettype::testnet)       arg_buf.append("--testnet ");
  else if (result.nettype == loki_nettype::fakenet)  arg_buf.append("--regtest ");
  else if (result.nettype == loki_nettype::stagenet) arg_buf.append("--stagenet ");

  arg_buf.append("--generate-new-wallet ./output/wallet_%d ", result.id);
  arg_buf.append("--password '' ");
  arg_buf.append("--mnemonic-language English ");

  if (params.allow_mismatched_daemon_version)
    arg_buf.append("--allow-mismatched-daemon-version ");

  if (params.daemon)
    arg_buf.append("--daemon-address 127.0.0.1:%d ", params.daemon->rpc_port);

  loki_fixed_string<128> name("%s%d", WALLET_IPC_NAME, result.id);
  arg_buf.append("--integration-test-pipe-name %s ", name.str);

#if 1
  loki_fixed_string<> cmd_buf = {};
  if (params.keep_terminal_open) cmd_buf = loki_fixed_string<>(LOKI_WALLET_CMD_FMT, result.id, terminal_name, arg_buf.str, "bash");
  else                           cmd_buf = loki_fixed_string<>(LOKI_WALLET_CMD_FMT, result.id, terminal_name, arg_buf.str, "");
  result.proc_handle = os_launch_process(cmd_buf.str);

  result.ipc = itest_ipc_setup(WALLET_IPC_NAME, result.id);
  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STRING("Error: refresh failed"), true},
    {LOKI_STRING("Error: refresh failed: unexpected error: proxy exception in refresh thread"), true},
    {LOKI_STRING("Balance"),               false},
  };

  itest_read_possible_value const *proxy_exception_error = possible_values + 1;
  itest_read_result read_result = itest_read_stdout_until(&result.ipc, possible_values, LOKI_ARRAY_COUNT(possible_values));
  LOKI_ASSERT_MSG(!str_find(read_result.buf.c_str(), proxy_exception_error->literal.str), "This shows up when you launch the daemon in the incorrect nettype and the wallet tries to forcefully refresh from it");
#endif
  return result;
}

// -------------------------------------------------------------------------------------------------
//
// loki_integration_tests
//
// -------------------------------------------------------------------------------------------------
#include <vector>
#include <thread>

typedef test_result(itest_scenario)(void);
struct work_queue
{
  std::atomic<size_t>           job_index;
  std::vector<itest_scenario *> jobs;
  std::atomic<size_t>           num_jobs_succeeded;
};

FILE_SCOPE work_queue global_work_queue;
void thread_to_task_dispatcher()
{
  for (;;)
  {
    size_t selected_job_index = global_work_queue.job_index.load();
    if (selected_job_index >= global_work_queue.jobs.size())
      break;

    size_t next_job_index = selected_job_index + 1;
    if (global_work_queue.job_index.compare_exchange_strong(selected_job_index,
                                                            next_job_index,
                                                            std::memory_order_acq_rel))
    {
      itest_scenario *run_scenario = global_work_queue.jobs[selected_job_index];
      test_result result = run_scenario();
      print_test_results(&result);

      if (!result.failed)
        global_work_queue.num_jobs_succeeded++;
    }
  }
}

FILE_SCOPE void print_help()
{
  fprintf(stdout, "Integration Test Startup Flags\n\n");
  fprintf(stdout, "  --generate-blockchain         |                Generate a blockchain instead of running tests. Flags below are optionally specified.\n");
  fprintf(stdout, "    --service-nodes     <value> | (Default: 0)   How many service nodes to generate in the blockchain\n");
  fprintf(stdout, "    --daemons           <value> | (Default: 1)   How many normal daemons to generate in the blockchain\n");
  fprintf(stdout, "    --wallets           <value> | (Default: 1)   How many wallets to generate for the blockchain\n");
  fprintf(stdout, "    --wallet-balance    <value> | (Default: 100) How much Loki each wallet should have (non-atomic units)\n");
  fprintf(stdout, "    --fixed-difficulty  <value> | (Default: 1)   Blocks should be mined with set difficulty, 0 to use the normal difficulty algorithm\n");
  // fprintf(stdout, "  --num-blocks    <value> | (Default: 100) How many blocks to generate in the blockchain, minimum 100\n");
}

enum struct daemon_type
{
  normal,
  service_node,
};

FILE_SCOPE void write_daemon_launch_script(helper_blockchain_environment const *environment, daemon_type type)
{
  daemon_t const *from  = nullptr;
  int num_from          = 0;
  daemon_t const *other = nullptr;
  int num_other         = 0;

  if (type == daemon_type::normal)
  {
    from  = environment->daemons;
    other = environment->service_nodes;

    num_from  = environment->num_daemons;
    num_other = environment->num_service_nodes;
  }
  else
  {
    from  = environment->service_nodes;
    other = environment->daemons;

    num_from  = environment->num_service_nodes;
    num_other = environment->num_daemons;
  }

  LOKI_FOR_EACH(daemon_index, num_from)
  {
    loki_fixed_string<> cmd_line("./lokid ");
    daemon_t const *daemon = from + daemon_index;
    cmd_line.append("--data-dir daemon_%d ",  daemon->id);
    cmd_line.append("--fixed-difficulty %d ", environment->daemon_param.fixed_difficulty);
    cmd_line.append("--p2p-bind-port %d ", daemon->p2p_port);
    cmd_line.append("--rpc-bind-port %d ", daemon->rpc_port);
    cmd_line.append("--zmq-rpc-bind-port %d ", daemon->zmq_rpc_port);
    cmd_line.append("--quorumnet--port %d ", daemon->quorumnet_port);
    cmd_line.append("--log-level 1 ");
    cmd_line.append("--dev-allow-local-ips ");

    if (environment->daemon_param.nettype      == loki_nettype::testnet)  cmd_line.append("--testnet ");
    else if (environment->daemon_param.nettype == loki_nettype::stagenet) cmd_line.append("--stagenet ");

    if (type == daemon_type::service_node) 
    {
      cmd_line.append("--service-node ");
      cmd_line.append("--service-node-public-ip 123.123.123.123 ");
      cmd_line.append("--storage-server-port 8080 ");
    }

    LOKI_FOR_EACH(index, num_from)
    {
      daemon_t const *other_daemon = from + index;
      if (index == daemon_index) continue;
      cmd_line.append("--add-exclusive-node 127.0.0.1:%d ", other_daemon->p2p_port);
    }

    LOKI_FOR_EACH(index, num_other)
    {
      daemon_t const *other_daemon = other + index;
      cmd_line.append("--add-exclusive-node 127.0.0.1:%d ", other_daemon->p2p_port);
    }

    loki_fixed_string<> file_name("./output/");
    file_name.append("daemon_");
    file_name.append("%d", daemon->id);
    if (type == daemon_type::service_node)
      file_name.append("_service_node_%d", daemon_index);
    file_name.append(".sh");

    if (!os_write_file(file_name.str, cmd_line.str, cmd_line.len))
    {
      fprintf(stderr, "Failed to create daemon launcher script file: %s\n", file_name.str);
      continue;
    }
  }
}

FILE_SCOPE void delete_old_blockchain_files()
{
  if (os_file_exists("./output/"))
  {
    os_file_dir_delete("./output");
    os_file_dir_make("./output");
  }
}

int main(int argc, char **argv)
{
  // TODO(doyle):
  //  - locked transfers unlock after the locked time
  //  - show_transfers distinguishes payments
  //  - creating accounts and subaddresses
  //  - transferring big amounts of loki

  //  - when a test fails using the EXPECT macro we should preserve the error
  //    message from the program, because right now we auto close the terminals
  //    which means when it fails, we need to step into the debugger and inspect
  //    the program to figure out why it failed.

  if (argc > 1)
  {
    for (int i = 1; i < argc; i++)
    {
      char const *arg       = argv[i];
      int arg_len           = strlen(arg);
      char const HELP_ARG[] = "--help";

      if (arg_len == char_count_i(HELP_ARG) && strncmp(arg, HELP_ARG, arg_len) == 0)
      {
        print_help();
        return true;
      }

      char const GENERATE_ARG[] = "--generate-blockchain";
      if (arg_len == char_count_i(GENERATE_ARG) && strncmp(arg, GENERATE_ARG, arg_len) == 0)
      {
        break;
      }
      else
      {
        fprintf(stderr, "Unrecognised argument %s\n\n", arg);
        print_help();
        return false;
      }
    }

    int num_options = argc - 2;
    if ((num_options % 2) != 0)
    {
      fprintf(stderr, "Invalid number of options, each --<option> should have a value associated with it, i.e. --option <value>\n");
      return false;
    }

    int num_daemons       = 1;
    int num_service_nodes = 0;
    // int num_blocks        = MIN_BLOCKS_IN_BLOCKCHAIN;
    int num_wallets            = 1;
    int initial_wallet_balance = 1;
    int fixed_difficulty       = 1;

    for (int i = 2; i < argc; i += 2)
    {
      char const *arg                 = argv[i];
      int arg_len                     = strlen(arg);
      char const SNODE_ARG[]          = "--service-nodes";
      char const DAEMON_ARG[]         = "--daemons";
      char const BLOCKS_ARG[]         = "--num-blocks";
      char const WALLET_ARG[]         = "--wallets";
      char const WALLET_BALANCE_ARG[] = "--wallet-balance";
      char const FIXED_DIFF_ARG[]     = "--fixed-difficulty";

      char const *arg_val_str = argv[i + 1];
      int arg_val             = atoi(arg_val_str);

      enum struct arg_type
      {
        invalid,
        service_nodes,
        daemons,
        num_blocks,
        wallets,
        wallet_balance,
        fixed_difficulty,
      };

      arg_type type = arg_type::invalid;
      if (arg_len == char_count_i(SNODE_ARG) && strncmp(arg, SNODE_ARG, arg_len) == 0)
      {
        type = arg_type::service_nodes;
      }
      else if (arg_len == char_count_i(DAEMON_ARG) && strncmp(arg, DAEMON_ARG, arg_len) == 0)
      {
        type = arg_type::daemons;
      }
      else if (arg_len == char_count_i(BLOCKS_ARG) && strncmp(arg, BLOCKS_ARG, arg_len) == 0)
      {
        type = arg_type::num_blocks;
      }
      else if (arg_len == char_count_i(WALLET_ARG) && strncmp(arg, WALLET_ARG, arg_len) == 0)
      {
        type = arg_type::wallets;
      }
      else if (arg_len == char_count_i(WALLET_BALANCE_ARG) && strncmp(arg, WALLET_BALANCE_ARG, arg_len) == 0)
      {
        type = arg_type::wallet_balance;
      }
      else if (arg_len == char_count_i(FIXED_DIFF_ARG) && strncmp(arg, FIXED_DIFF_ARG, arg_len) == 0)
      {
        type = arg_type::fixed_difficulty;
      }
      else
      {
        fprintf(stderr, "Unrecognised argument %s with value %s\n", arg, arg_val_str);
        return false;
      }

      if (arg_val < 0)
      {
        if (type == arg_type::fixed_difficulty && arg_val == 0) // TODO(doyle): oh boi. getting messy
        {
        }
        else
        {
          fprintf(stderr, "Argument %s has invalid value %s\n", arg, arg_val_str);
          return false;
        }
      }

      if (type == arg_type::service_nodes)
      {
        num_service_nodes = arg_val;
      }
      else if (type == arg_type::daemons)
      {
        num_daemons = arg_val;
      }
      else if (type == arg_type::num_blocks)
      {
        if (arg_val < 100)
        {
          fprintf(stdout, "Warning: Num blocks specified less than 100: %d, the minimum is 100. Overriding to 100\n", arg_val);
          arg_val = 100;
        }
        // num_blocks = arg_val;
      }
      else if (type == arg_type::wallets)
      {
        num_wallets = arg_val;
      }
      else if (type == arg_type::wallet_balance)
      {
        initial_wallet_balance = arg_val;
      }
      else if (type == arg_type::fixed_difficulty)
      {
        fixed_difficulty = arg_val;
      }
    }

    delete_old_blockchain_files();
    test_result context = {};
    INITIALISE_TEST_CONTEXT(context);

    start_daemon_params params                = {};
    params.fixed_difficulty                   = fixed_difficulty;
    helper_blockchain_environment environment = helper_setup_blockchain(&context, params, num_service_nodes, num_daemons, num_wallets, initial_wallet_balance);
    helper_cleanup_blockchain_environment(&environment);

    write_daemon_launch_script(&environment, daemon_type::normal);
    write_daemon_launch_script(&environment, daemon_type::service_node);

    for (wallet_t &wallet : environment.wallets)
    {
      loki_fixed_string<> cmd_line("./loki-wallet-cli --daemon-address 127.0.0.1:2222 --wallet-file wallet_%d --password '' ", wallet.id);
      if (environment.daemon_param.nettype      == loki_nettype::testnet)  cmd_line.append("--testnet ");
      else if (environment.daemon_param.nettype == loki_nettype::stagenet) cmd_line.append("--stagenet ");

      loki_fixed_string<> file_name("./output/wallet_%d.sh", wallet.id);
      if (!os_write_file(file_name.str, cmd_line.str, cmd_line.len))
      {
        fprintf(stderr, "Failed to create daemon launcher script file: %s\n", file_name.str);
        return false;
      }
    }

    os_launch_process("chmod +x ./output/daemon_*.sh");
    os_launch_process("chmod +x ./output/wallet_*.sh");
    return true;
  }

  delete_old_blockchain_files();
  printf("\n");
#if 1
  int const NUM_THREADS = LOKI_MIN((int)std::thread::hardware_concurrency(), 16);
#else
  int const NUM_THREADS = 1;
#endif

  auto start_time = std::chrono::high_resolution_clock::now();
#if 0
  global_work_queue.jobs.push_back(checkpointing__deregister_non_participating_peer);
  global_work_queue.jobs.push_back(checkpointing__new_peer_syncs_checkpoints);
  global_work_queue.jobs.push_back(checkpointing__private_chain_reorgs_to_checkpoint_chain);

  // NOTE(doyle): Doesn't work
  // global_work_queue.jobs.push_back(decommission__recommission_on_uptime_proof);

  global_work_queue.jobs.push_back(deregistration__n_unresponsive_node);

  global_work_queue.jobs.push_back(prepare_registration__check_100_percent_operator_cut_stake);
  global_work_queue.jobs.push_back(prepare_registration__check_all_solo_stake_forms_valid_registration);
  global_work_queue.jobs.push_back(prepare_registration__check_solo_stake);

  global_work_queue.jobs.push_back(print_locked_stakes__check_no_locked_stakes);
  global_work_queue.jobs.push_back(print_locked_stakes__check_shows_locked_stakes);

  global_work_queue.jobs.push_back(register_service_node__allow_43_23_13_21_reserved_contribution);
  global_work_queue.jobs.push_back(register_service_node__allow_4_stakers);
  global_work_queue.jobs.push_back(register_service_node__allow_70_20_and_10_open_for_contribution);
  global_work_queue.jobs.push_back(register_service_node__allow_87_13_contribution);
  global_work_queue.jobs.push_back(register_service_node__allow_87_13_reserved_contribution);
  global_work_queue.jobs.push_back(register_service_node__check_unlock_time_is_0);
  global_work_queue.jobs.push_back(register_service_node__disallow_register_twice);

  global_work_queue.jobs.push_back(request_stake_unlock__check_pooled_stake_unlocked);
  global_work_queue.jobs.push_back(request_stake_unlock__check_unlock_height);
  global_work_queue.jobs.push_back(request_stake_unlock__disallow_request_on_non_existent_node);
  global_work_queue.jobs.push_back(request_stake_unlock__disallow_request_twice);

  global_work_queue.jobs.push_back(stake__allow_incremental_stakes_with_1_contributor);
  global_work_queue.jobs.push_back(stake__check_incremental_stakes_decreasing_min_contribution);
  global_work_queue.jobs.push_back(stake__check_transfer_doesnt_used_locked_key_images);
  global_work_queue.jobs.push_back(stake__disallow_staking_less_than_minimum_in_pooled_node);
  global_work_queue.jobs.push_back(stake__disallow_staking_when_all_amounts_reserved);
  global_work_queue.jobs.push_back(stake__disallow_to_non_registered_node);

  global_work_queue.jobs.push_back(transfer__check_fee_amount_80x_increase);

  global_work_queue.jobs.push_back(v11__transfer__check_fee_amount_bulletproofs);
#else
  // global_work_queue.jobs.push_back(decommission__recommission_on_uptime_proof);
  global_work_queue.jobs.push_back(wallet__buy_lns_mapping__session);
#endif

  std::vector<std::thread> threads;
  threads.reserve(NUM_THREADS);

  for (int i = 0; i < NUM_THREADS; ++i)
    threads.push_back(std::thread(thread_to_task_dispatcher));

  for (int i = 0; i < NUM_THREADS; ++i)
    threads[i].join();

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
  printf("\nTests passed %zu/%zu (using %d threads) in %5.2fs\n\n", global_work_queue.num_jobs_succeeded.load(), global_work_queue.jobs.size(), NUM_THREADS, duration / 1000.f);

  return 0;
}
