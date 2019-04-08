#include "loki_integration_tests.h"
#include "loki_test_cases.h"

#define SHOOM_IMPLEMENTATION
#include "external/shoom.h"

#define STB_SPRINTF_IMPLEMENTATION
#include "external/stb_sprintf.h"

#define LOKI_OS_IMPLEMENTATION
#include "loki_os.h"

#include "loki_daemon.h"
#include "loki_str.h"

enum struct terminal_type_t
{
  lxterminal,
  xterm,
};

#define XTERM_CMD 1

#if LXTERMINAL_CMD
char const LOKI_CMD_FMT[]        = "lxterminal -t \"daemon_%d\" -e bash -c \"/home/loki/Loki/Code/loki-integration-testing/bin/lokid %s; %s \"";
char const LOKI_WALLET_CMD_FMT[] = "lxterminal -t \"wallet_%d\" -e bash -c \"/home/loki/Loki/Code/loki-integration-testing/bin/loki-wallet-cli %s; %s \"";
#elif XTERM_CMD
char const LOKI_CMD_FMT[]        = "xterm -T \"daemon_%d\" -e bash -c \"/home/doyle/Loki/Code/loki-integration-testing/bin/lokid %s; %s \"";
char const LOKI_WALLET_CMD_FMT[] = "xterm -T \"wallet_%d\" -e bash -c \"/home/doyle/Loki/Code/loki-integration-testing/bin/loki-wallet-cli %s; %s \"";
#endif

#include <atomic>
struct state_t
{
  std::atomic<int> num_wallets   = 0;
  std::atomic<int> num_daemons   = 0;
  std::atomic<int> free_p2p_port = 1111;
  std::atomic<int> free_rpc_port = 2222;
  std::atomic<int> free_zmq_port = 3333;
};

static state_t global_state;

uint32_t const MSG_MAGIC_BYTES = 0x7428da3f;
static char *parse_message(char *msg_buf, int msg_buf_len)
{
  char *ptr = msg_buf;
  if ((*(uint32_t *)ptr) != MSG_MAGIC_BYTES)
    return nullptr;

  ptr += sizeof(MSG_MAGIC_BYTES);
  LOKI_ASSERT(ptr < msg_buf + msg_buf_len);
  return ptr;
}

void in_out_shared_mem::clean_up()
{
  sem_unlink(this->stdin_semaphore_name.c_str);
  sem_unlink(this->stdout_semaphore_name.c_str);
  sem_unlink(this->stdin_ready_semaphore_name.c_str);
  sem_unlink(this->stdout_ready_semaphore_name.c_str);
}

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
}

void itest_write_to_stdin(in_out_shared_mem *shared_mem, char const *cmd)
{
  int cmd_len   = static_cast<int>(strlen(cmd));
  char *ptr     = reinterpret_cast<char *>(shared_mem->stdin_mem.Data());
  int total_len = static_cast<int>(sizeof(MSG_MAGIC_BYTES) + cmd_len);
  LOKI_ASSERT(total_len < (int)shared_mem->stdin_mem.Size());

  // Write message to shared memory
  sem_wait(shared_mem->stdin_ready_semaphore_handle);
  {
    memcpy(ptr, (char *)&MSG_MAGIC_BYTES, sizeof(MSG_MAGIC_BYTES));
    ptr += sizeof(MSG_MAGIC_BYTES);

    memcpy(ptr, cmd, cmd_len);
    ptr += cmd_len;
    ptr[0] = 0;
  }
  sem_post(shared_mem->stdin_semaphore_handle);
}

itest_read_result itest_write_then_read_stdout(in_out_shared_mem *shared_mem, char const *cmd)
{
  itest_write_to_stdin(shared_mem, cmd);
  itest_read_result result = itest_read_stdout(shared_mem);
  return result;
}

itest_read_result itest_write_then_read_stdout_until(in_out_shared_mem *shared_mem, char const *cmd, itest_read_possible_value const *possible_values, int possible_values_len)
{
  itest_write_to_stdin(shared_mem, cmd);
  itest_read_result result = itest_read_stdout_until(shared_mem, possible_values, possible_values_len);
  return result;
}

itest_read_result itest_write_then_read_stdout_until(in_out_shared_mem *shared_mem, char const *cmd, loki_str_lit find_str)
{
  itest_read_possible_value possible_values[] = { {find_str, false}, };
  itest_read_result result = itest_write_then_read_stdout_until(shared_mem, cmd, possible_values, 1);
  return result;
}

itest_read_result itest_read_stdout(in_out_shared_mem *shared_mem)
{
  char *output = nullptr;
  int output_len = 0;

  itest_read_result result = {};
  for (;;)
  {
    int sem_value = 0;
    sem_getvalue(shared_mem->stdout_semaphore_handle, &sem_value);
    sem_wait(shared_mem->stdout_semaphore_handle);

    shared_mem->stdout_mem.Open();
    output = parse_message(reinterpret_cast<char *>(shared_mem->stdout_mem.Data()), shared_mem->stdout_mem.Size());
    if (output)
    {
      output_len = strlen(output);
      if (output_len > 0) break;
    }

    sem_post(shared_mem->stdout_ready_semaphore_handle);
  }

  result.buf.len = output_len;
  LOKI_ASSERT(result.buf.len <= result.buf.max());

  memcpy(result.buf.data, output, result.buf.len);
  result.buf.data[result.buf.len] = 0;

  sem_post(shared_mem->stdout_ready_semaphore_handle);
  return result;
}

itest_read_result itest_read_stdout_until(in_out_shared_mem *shared_mem, char const *find_str)
{
  itest_read_possible_value possible_values[] = { {find_str, false}, };
  itest_read_result result = itest_read_stdout_until(shared_mem, possible_values, 1);
  return result;
}

void itest_read_stdout_sink(in_out_shared_mem *shared_mem, int milliseconds)
{
  int time_remaining = LOKI_MS_TO_SECONDS(milliseconds);

  timespec time = {};
  if (clock_gettime(CLOCK_REALTIME, &time) == -1)
    LOKI_ASSERT(false);

  time.tv_sec += time_remaining;
  for (; time_remaining > 0;)
  {
    int sem_value = 0;
    sem_getvalue(shared_mem->stdout_semaphore_handle, &sem_value);
    if (sem_timedwait(shared_mem->stdout_semaphore_handle, &time) == -1)
    {
      if (errno == ETIMEDOUT)
        break;
    }

    timespec time_after = {};
    if (clock_gettime(CLOCK_REALTIME, &time_after) == -1)
      LOKI_ASSERT(false);

    time_remaining -= time_after.tv_sec - time.tv_sec;
    sem_post(shared_mem->stdout_ready_semaphore_handle);
  }
}

itest_read_result itest_read_stdout_until(in_out_shared_mem *shared_mem, itest_read_possible_value const *possible_values, int possible_values_len)
{
  for (;;)
  {
    itest_read_result result = itest_read_stdout(shared_mem);

    LOKI_FOR_EACH(i, possible_values_len)
    {
      char const *check = result.buf.c_str;
      if (str_find(check, possible_values[i].literal.str))
      {
        result.matching_find_strs_index = i;
        return result;
      }
    }
  }
}

void itest_read_until_then_write_stdin(in_out_shared_mem *shared_mem, loki_str_lit find_str, char const *cmd)
{
  itest_read_stdout_until(shared_mem, find_str.str);
  itest_write_to_stdin(shared_mem, cmd);
}

void itest_read_stdout_for_ms(in_out_shared_mem *shared_mem, int time_ms)
{
  timespec time = {};
  clock_gettime(CLOCK_REALTIME, &time);
  time.tv_sec += LOKI_MS_TO_SECONDS(time_ms);

  for (;;)
  {
    if (sem_timedwait(shared_mem->stdout_semaphore_handle, &time) == -1 && errno == ETIMEDOUT)
      return;

    sem_post(shared_mem->stdout_ready_semaphore_handle);
  }
}

char const DAEMON_SHARED_MEM_NAME[] = "loki_integration_testing_daemon";
void start_daemon(daemon_t *daemons, int num_daemons, start_daemon_params *params, int num_params)
{
  LOKI_ASSERT(num_params == num_daemons || num_params == 1);
  for (int curr_daemon_index = 0; curr_daemon_index < num_daemons; ++curr_daemon_index)
  {
    start_daemon_params param = (num_params == 1) ? params[0] : params[curr_daemon_index];
    daemon_t *curr_daemon = daemons + curr_daemon_index;

    loki_scratch_buf arg_buf = {};
    arg_buf.append("%s ",                            param.custom_cmd_line.c_str);
    arg_buf.append("--p2p-bind-port %d ",            curr_daemon->p2p_port);
    arg_buf.append("--rpc-bind-port %d ",            curr_daemon->rpc_port);
    arg_buf.append("--zmq-rpc-bind-port %d ",        curr_daemon->zmq_rpc_port);
    arg_buf.append("--data-dir ./output/daemon_%d ", curr_daemon->id);

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

    loki_buffer<128> name("%s%d", DAEMON_SHARED_MEM_NAME, curr_daemon->id);
    arg_buf.append("--integration-test-shared-mem-name %s ", name.c_str);

    for (int other_daemon_index = 0; other_daemon_index < num_daemons; ++other_daemon_index)
    {
      if (curr_daemon_index == other_daemon_index)
        continue;

      daemon_t const *other_daemon = daemons + other_daemon_index;
      arg_buf.append("--add-exclusive-node 127.0.0.1:%d ", other_daemon->p2p_port);
    }

    itest_reset_shared_memory(&curr_daemon->shared_mem);
    if (curr_daemon->id == 0)
    {
      // continue;
    }

    loki_scratch_buf cmd_buf = {};
    if (param.keep_terminal_open) cmd_buf = loki_scratch_buf(LOKI_CMD_FMT, curr_daemon->id, arg_buf.data, "bash");
    else                          cmd_buf = loki_scratch_buf(LOKI_CMD_FMT, curr_daemon->id, arg_buf.data, "");

    curr_daemon->proc_handle = os_launch_process(cmd_buf.data);
    daemon_status(curr_daemon);
  }
}

// L6cuiMjoJs7hbv5qWYxCnEB1WJG827jrDAu4wEsrsnkYVu3miRHFrBy9pGcYch4TDn6dgatqJugUafWxCAELiq461p5mrSa
// T6U4ukY68vohsfrGMryFmqX5yRE4d5EC8E6QbinSo8ssW3heqoNjgNggTeym9NSLW4cnEp3ckpD9RZLW5qDGg3821c9SAtHMD

daemon_t create_and_start_daemon(start_daemon_params params)
{
  daemon_t result = create_daemon();
  start_daemon(&result, 1, &params, 1);
  return result;
}

void create_and_start_multi_daemons(daemon_t *daemons, int num_daemons, start_daemon_params params)
{
  for (int i = 0; i < num_daemons; ++i)
    daemons[i] = create_daemon();
  start_daemon(daemons, num_daemons, &params, 1);
}

void create_and_start_multi_daemons(daemon_t *daemons, int num_daemons, start_daemon_params *params, int num_params)
{
  LOKI_ASSERT(num_params == num_daemons || num_params == 1);

  for (int i = 0; i < num_daemons; ++i)
    daemons[i] = create_daemon();
  start_daemon(daemons, num_daemons, params, num_params);
}

char const WALLET_SHARED_MEM_NAME[] = "loki_integration_testing_wallet";
static in_out_shared_mem setup_shared_mem(char const *base_name, int id)
{
  in_out_shared_mem result = {};
  loki_buffer<128> stdin_name ("%s%d_stdin", base_name, id);
  loki_buffer<128> stdout_name("%s%d_stdout", base_name, id);
  result.stdin_mem             = shoom::Shm(stdin_name.c_str, 32768);
  result.stdout_mem            = shoom::Shm(stdout_name.c_str, 32768);
  result.stdin_semaphore_name  = loki_buffer<128>("/%s%d_stdin_semaphore", base_name, id);
  result.stdout_semaphore_name = loki_buffer<128>("/%s%d_stdout_semaphore", base_name, id);
  result.stdin_ready_semaphore_name  = loki_buffer<128>("/%s%d_stdin_ready_semaphore", base_name, id);
  result.stdout_ready_semaphore_name = loki_buffer<128>("/%s%d_stdout_ready_semaphore", base_name, id);
  return result;
}

wallet_t create_and_start_wallet(loki_nettype type, start_wallet_params params)
{
  wallet_t result = {};
  result.id       = global_state.num_wallets++;
  result.nettype  = type;

  loki_scratch_buf arg_buf = {};
  if (result.nettype == loki_nettype::testnet)       arg_buf.append("--testnet ");
  else if (result.nettype == loki_nettype::fakenet)  arg_buf.append("--fakenet ");
  else if (result.nettype == loki_nettype::stagenet) arg_buf.append("--stagenet ");

  arg_buf.append("--generate-new-wallet ./output/wallet_%d ", result.id);
  arg_buf.append("--password '' ");
  arg_buf.append("--mnemonic-language English ");

  if (params.allow_mismatched_daemon_version)
    arg_buf.append("--allow-mismatched-daemon-version ");

  if (params.daemon)
    arg_buf.append("--daemon-address 127.0.0.1:%d ", params.daemon->rpc_port);

  loki_buffer<128> name("%s%d", WALLET_SHARED_MEM_NAME, result.id);
  arg_buf.append("--integration-test-shared-mem-name %s ", name.c_str);

  result.shared_mem = setup_shared_mem(WALLET_SHARED_MEM_NAME, result.id);
  itest_reset_shared_memory(&result.shared_mem);

#if 1
  loki_scratch_buf cmd_buf = {};
  if (params.keep_terminal_open) cmd_buf = loki_scratch_buf(LOKI_WALLET_CMD_FMT, result.id, arg_buf.data, "bash");
  else                           cmd_buf = loki_scratch_buf(LOKI_WALLET_CMD_FMT, result.id, arg_buf.data, "");
  result.proc_handle = os_launch_process(cmd_buf.data);

  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STR_LIT("Error: refresh failed"), true},
    {LOKI_STR_LIT("Error: refresh failed: unexpected error: proxy exception in refresh thread"), true},
    {LOKI_STR_LIT("Balance"),               false},
  };
  
  itest_read_possible_value const *proxy_exception_error = possible_values + 1;
  itest_read_result read_result = itest_read_stdout_until(&result.shared_mem, possible_values, LOKI_ARRAY_COUNT(possible_values));
  LOKI_ASSERT_MSG(!str_find(read_result.buf.c_str, proxy_exception_error->literal.str), "This shows up when you launch the daemon in the incorrect nettype and the wallet tries to forcefully refresh from it");
#endif
  return result;
}

daemon_t create_daemon()
{
  daemon_t result     = {};
  result.id           = global_state.num_daemons++;
  result.p2p_port     = global_state.free_p2p_port++;
  result.rpc_port     = global_state.free_rpc_port++;
  result.zmq_rpc_port = global_state.free_zmq_port++;
  result.shared_mem   = setup_shared_mem(DAEMON_SHARED_MEM_NAME, result.id);
  return result;
}

static void reset_semaphore(sem_t *semaphore)
{
  int value = 0;
  for (;;)
  {
    sem_getvalue(semaphore, &value);
    if (value == 0) break;
    else if (value > 0) sem_wait(semaphore);
    else if (value < 0) sem_post(semaphore);
  }
}

void itest_reset_shared_memory(in_out_shared_mem *shared_mem)
{
  shared_mem->stdin_mem.Create (shoom::Flag::create | shoom::Flag::clear_on_create);
  shared_mem->stdout_mem.Create(shoom::Flag::create | shoom::Flag::clear_on_create);
  shared_mem->stdout_mem.Open  ();

  shared_mem->clean_up();
  shared_mem->stdin_semaphore_handle = sem_open(shared_mem->stdin_semaphore_name.c_str, O_CREAT, 0600, 0);
  if (shared_mem->stdin_semaphore_handle == SEM_FAILED) perror("Failed to initialise stdin semaphore");

  shared_mem->stdout_semaphore_handle = sem_open(shared_mem->stdout_semaphore_name.c_str, O_CREAT, 0600, 0);
  if (shared_mem->stdout_semaphore_handle == SEM_FAILED) perror("Failed to initialise stdout semaphore");

  shared_mem->stdout_ready_semaphore_handle = sem_open(shared_mem->stdout_ready_semaphore_name.c_str, O_CREAT, 0600, 0);
  if (shared_mem->stdout_ready_semaphore_handle == SEM_FAILED) perror("Failed to initialise stdout ready semaphore");

  shared_mem->stdin_ready_semaphore_handle = sem_open(shared_mem->stdin_ready_semaphore_name.c_str, O_CREAT, 0600, 0);
  if (shared_mem->stdin_ready_semaphore_handle == SEM_FAILED) perror("Failed to initialise stdin ready semaphore");

  reset_semaphore(shared_mem->stdin_semaphore_handle);
  reset_semaphore(shared_mem->stdout_semaphore_handle);
  reset_semaphore(shared_mem->stdout_ready_semaphore_handle);
  reset_semaphore(shared_mem->stdin_ready_semaphore_handle);
  sem_post(shared_mem->stdout_ready_semaphore_handle);
  sem_post(shared_mem->stdin_ready_semaphore_handle);
}

#include <iostream>
#include <vector>
#include <thread>

typedef test_result(itest_scenario)(void);
struct work_queue
{
  std::atomic<size_t>           job_index;
  std::vector<itest_scenario *> jobs;
  std::atomic<size_t>           num_jobs_succeeded;
};

static work_queue global_work_queue;
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

int main(int, char **)
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

  // TODO(doyle): We can multithread dispatch these tests now with multidaemon/multiwallet support.
  if (os_file_exists("./output/"))
  {
    os_file_dir_delete("./output");
    os_file_dir_make("./output");
  }

  printf("\n");
  bool const MULTITHREAD = true;
  if (MULTITHREAD)
  {
    global_work_queue.jobs.push_back(latest__deregistration__n_unresponsive_node);
    global_work_queue.jobs.push_back(latest__prepare_registration__check_solo_stake);
    global_work_queue.jobs.push_back(latest__prepare_registration__check_all_solo_stake_forms_valid_registration);
    global_work_queue.jobs.push_back(latest__prepare_registration__check_100_percent_operator_cut_stake);
    global_work_queue.jobs.push_back(latest__print_locked_stakes__check_no_locked_stakes);
    global_work_queue.jobs.push_back(latest__print_locked_stakes__check_shows_locked_stakes);
    global_work_queue.jobs.push_back(latest__register_service_node__allow_4_stakers);
    global_work_queue.jobs.push_back(latest__register_service_node__allow_70_20_and_10_open_for_contribution);
    global_work_queue.jobs.push_back(latest__register_service_node__allow_43_23_13_21_reserved_contribution);
    global_work_queue.jobs.push_back(latest__register_service_node__allow_87_13_reserved_contribution);
    global_work_queue.jobs.push_back(latest__register_service_node__allow_87_13_contribution);
    global_work_queue.jobs.push_back(latest__register_service_node__check_unlock_time_is_0);
    global_work_queue.jobs.push_back(latest__register_service_node__disallow_register_twice);
    global_work_queue.jobs.push_back(latest__request_stake_unlock__check_pooled_stake_unlocked);
    global_work_queue.jobs.push_back(latest__request_stake_unlock__check_unlock_height);
    global_work_queue.jobs.push_back(latest__request_stake_unlock__disallow_request_on_non_existent_node);
    global_work_queue.jobs.push_back(latest__request_stake_unlock__disallow_request_twice);
    global_work_queue.jobs.push_back(latest__stake__allow_incremental_stakes_with_1_contributor);
    global_work_queue.jobs.push_back(latest__stake__check_incremental_stakes_decreasing_min_contribution);
    global_work_queue.jobs.push_back(latest__stake__check_transfer_doesnt_used_locked_key_images);
    global_work_queue.jobs.push_back(latest__stake__disallow_staking_less_than_minimum_in_pooled_node);
    global_work_queue.jobs.push_back(latest__stake__disallow_staking_when_all_amounts_reserved);
    global_work_queue.jobs.push_back(latest__stake__disallow_to_non_registered_node);
    global_work_queue.jobs.push_back(latest__transfer__check_fee_amount_bulletproofs);
    global_work_queue.jobs.push_back(v10__prepare_registration__check_all_solo_stake_forms_valid_registration);
    global_work_queue.jobs.push_back(v10__register_service_node__check_gets_payed_expires_and_returns_funds);
    global_work_queue.jobs.push_back(v10__register_service_node__check_grace_period);
    global_work_queue.jobs.push_back(v10__stake__allow_incremental_staking_until_node_active);
    global_work_queue.jobs.push_back(v10__stake__allow_insufficient_stake_w_reserved_contributor);
    global_work_queue.jobs.push_back(v10__stake__disallow_insufficient_stake_w_not_reserved_contributor);
    global_work_queue.jobs.push_back(v09__transfer__check_fee_amount);

    std::thread threads[16];
    for (size_t i = 0; i < LOKI_ARRAY_COUNT(threads); ++i)
      threads[i] = std::thread(thread_to_task_dispatcher);

    for (size_t i = 0; i < LOKI_ARRAY_COUNT(threads); ++i)
      threads[i].join();

    printf("\nTests passed %zu/%zu\n\n", global_work_queue.num_jobs_succeeded.load(), global_work_queue.jobs.size());
  }
  else
  {
#define RUN_TEST(test_function) \
  fprintf(stdout, "%03zu " #test_function, results_index + 1); \
  fflush(stdout); \
  results[results_index++] = test_function(); \
  print_test_results(&results[results_index-1])

    test_result results[128] = {};
    size_t results_index     = 0;
    RUN_TEST(latest__prepare_registration__check_solo_stake);
  }

  return 0;
}
