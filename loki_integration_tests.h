#ifndef LOKI_INTEGRATION_TEST_H
#define LOKI_INTEGRATION_TEST_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "external/shoom.h"
#include "external/stb_sprintf.h"

#include <semaphore.h>

//
// Primitives
//

#define LOKI_ASSERT(expr) \
  if (!(expr)) \
  { \
    fprintf(stderr, "%s:%d assert(" #expr "): \n", __FILE__, __LINE__); \
    assert(expr); \
  }

#define LOKI_ASSERT_MSG(expr, fmt, ...) \
  if (!(expr)) \
  { \
    fprintf(stderr, "%s:%d assert(" #expr "): " fmt "\n", __FILE__, __LINE__, ## __VA_ARGS__); \
    assert(expr); \
  }

using isize = ptrdiff_t;
using usize = size_t;

#define LOKI_ARRAY_COUNT(array) (sizeof(array)/sizeof(array[0]))
#define LOKI_CHAR_COUNT(array) (LOKI_ARRAY_COUNT(array) - 1)
#define LOKI_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define LOKI_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define LOKI_ABS(a) (((a) < 0) ? (-a) : (a))
#define LOKI_MS_TO_NANOSECONDS(val) ((val) * 1000000ULL)
#define LOKI_MS_TO_SECONDS(val) ((val) / 1000)
#define LOKI_SECONDS_TO_MS(val) ((val) * 1000)
#define LOKI_MINUTES_TO_S(val) ((val) * 60)
#define LOKI_HOURS_TO_S(val) ((val) * LOKI_MINUTES_TO_S(60))
#define LOKI_DAYS_TO_S(val) ((val) * LOKI_HOURS_TO_S(24))
#define LOKI_FOR_EACH(iterator, limit) for (isize iterator = 0; iterator < (isize)limit; ++iterator)

template <typename procedure>
struct loki_defer_
{
     loki_defer_(procedure proc) : proc(proc) { }
    ~loki_defer_()                            { proc(); }
    procedure proc;
};

struct loki_defer_helper_
{
    template <typename procedure>
    loki_defer_<procedure> operator+(procedure proc) { return loki_defer_<procedure>(proc); };
};

#define LOKI_TOKEN_COMBINE(a, b) LOKI_TOKEN_COMBINE2(a, b)
#define LOKI_TOKEN_COMBINE2(a, b) a ## b
#define LOKI_DEFER const auto LOKI_TOKEN_COMBINE(defer_lambda_, __COUNTER__) = loki_defer_helper_() + [&]()

template <int MAX>
struct loki_buffer
{
  loki_buffer() : len(0) { data[0] = 0; }
  loki_buffer(char const *fmt, ...)     { va_list va; va_start(va, fmt); len =  stbsp_vsnprintf(data, MAX, fmt, va);                  va_end(va); LOKI_ASSERT(len   < MAX); }
  void append(char const *fmt, ...)     { va_list va; va_start(va, fmt); int extra = stbsp_vsnprintf(data + len, MAX - len, fmt, va); va_end(va); LOKI_ASSERT(extra < MAX - len); len += extra; }

  void clear() { len = 0; data[0] = 0; }
  int  max()   { return MAX; };

  union
  {
    char data[MAX];
    char c_str[MAX];
  };

  int  len;
};

using loki_scratch_buf = loki_buffer<32768>;

struct loki_str_lit
{
  char const *str;
  int         len;
  bool operator ==(loki_str_lit const &other) const
  {
    if (len != other.len) return false;
    return (strcmp(str, other.str) == 0);
  }
};
#define LOKI_STR_LIT(str) {str, LOKI_CHAR_COUNT(str)}

//
// Integration Test Primitives
//

struct in_out_shared_mem
{
  shoom::Shm stdin_mem;
  shoom::Shm stdout_mem;

  loki_buffer<128> stdin_semaphore_name;
  loki_buffer<128> stdout_semaphore_name;
  loki_buffer<128> stdin_ready_semaphore_name;
  loki_buffer<128> stdout_ready_semaphore_name;
  sem_t           *stdin_semaphore_handle;
  sem_t           *stdout_semaphore_handle;
  sem_t           *stdin_ready_semaphore_handle;
  sem_t           *stdout_ready_semaphore_handle;

  void clean_up();
};

struct itest_read_result
{
  int              matching_find_strs_index;
  loki_scratch_buf buf;
};

struct itest_read_possible_value
{
  loki_str_lit literal;
  bool         is_fail_msg;
};

const int ITEST_DEFAULT_TIMEOUT_MS = 100;
const int ITEST_INFINITE_TIMEOUT   = -1;
void              itest_write_to_stdin              (in_out_shared_mem *shared_mem, char const *cmd);
itest_read_result itest_write_then_read_stdout      (in_out_shared_mem *shared_mem, char const *cmd);
itest_read_result itest_write_then_read_stdout_until(in_out_shared_mem *shared_mem, char const *cmd, loki_str_lit                     find_str);
itest_read_result itest_write_then_read_stdout_until(in_out_shared_mem *shared_mem, char const *cmd, itest_read_possible_value const *possible_values, int possible_values_len);
itest_read_result itest_read_stdout                 (in_out_shared_mem *shared_mem);
itest_read_result itest_read_stdout_until           (in_out_shared_mem *shared_mem, char const *find_str);
itest_read_result itest_read_stdout_until           (in_out_shared_mem *shared_mem, itest_read_possible_value const *possible_values, int possible_values_len);
void              itest_read_until_then_write_stdin (in_out_shared_mem *shared_mem, loki_str_lit find_str, char const *cmd);
void              itest_read_stdout_for_ms          (in_out_shared_mem *shared_mem, int time_ms);
void              itest_reset_shared_memory         (in_out_shared_mem *shared_mem);

//
// Loki Blockchain Primitives
//

const int      LOKI_CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE     = 10;
const int      LOKI_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW    = 30;
const uint64_t LOKI_ATOMIC_UNITS                            = 1000000000ULL;
const uint64_t LOKI_FAKENET_STAKING_REQUIREMENT             = 100;
const int      LOKI_TARGET_DIFFICULTY                       = 120;
const int      LOKI_STAKING_EXCESS_BLOCKS                   = 20;
const int      LOKI_STAKING_REQUIREMENT_LOCK_BLOCKS         = LOKI_TARGET_DIFFICULTY / LOKI_DAYS_TO_S(30);
const int      LOKI_STAKING_REQUIREMENT_LOCK_BLOCKS_TESTNET = LOKI_TARGET_DIFFICULTY / LOKI_DAYS_TO_S(2);
const int      LOKI_REORG_SAFETY_BUFFER                     = 20;
const int      LOKI_QUORUM_SIZE                             = 10;

using loki_transaction_id                                   = loki_buffer<64  + 1>;
using loki_snode_key                                        = loki_buffer<64  + 1>;
using loki_payment_id16                                     = loki_buffer<16  + 1>;
using loki_payment_id64                                     = loki_buffer<64  + 1>;

struct loki_addr
{
  enum type_t
  {
    normal,
    integrated,
  };

  type_t type;
  void set_normal_addr    (char const *src) { type = type_t::normal;     buf.clear(); if (src[95] == ' ') buf.append("%.*s", 95, src); else buf.append("%.*s", 97, src); }
  void set_integrated_addr(char const *src) { type = type_t::integrated; buf.clear(); buf.append("%.*s", buf.max(), src); }

  // NOTE: Mainnet addresses are 95 chars, testnet addresses are 97 and + 1 for null terminator.
  // Integrated addresses are 108 characters
  // TODO(doyle): How long are testnet integrated addresses?
  loki_buffer<108 + 1> buf;
};

struct loki_contributor
{
  loki_addr addr;
  uint64_t  amount;
};

struct loki_transaction
{
  loki_transaction_id id;
  loki_addr           dest;
  uint64_t            atomic_amount;
  uint64_t            fee;
};


struct loki_hardfork
{
  int version;
  int height;
};

enum struct loki_nettype
{
  mainnet,
  fakenet,
  testnet,
  stagenet,
};

//
// Process Handling
//
struct daemon_t
{
  FILE             *proc_handle;
  int               id;
  bool              is_mining;
  int               p2p_port;
  int               rpc_port;
  int               zmq_rpc_port;
  in_out_shared_mem shared_mem;
};

struct wallet_t
{
  FILE         *proc_handle;
  int           id;
  loki_nettype  nettype;
  uint64_t      balance;
  uint64_t      unlocked_balance;
  in_out_shared_mem shared_mem;
};

struct start_daemon_params
{
  int           fixed_difficulty = 1; // Set to 0 to disable
  bool          service_node     = true;
  loki_hardfork hardforks[16];        // If hardforks are specified, we run in mainnet/fakechain mode
  int           num_hardforks;
  loki_nettype  nettype          = loki_nettype::testnet;

  void add_hardfork                          (int version, int height); // TODO: Sets daemon mode to fakechain sadly, can't keep testnet. We should fix this
  void add_sequential_hardforks_until_version(int version);
  void load_latest_hardfork_versions();
};

struct start_wallet_params
{
  daemon_t *daemon                          = nullptr;
  bool      allow_mismatched_daemon_version = false;
};

wallet_t create_wallet                 (loki_nettype type);
daemon_t create_daemon                 ();
void     start_wallet                  (wallet_t *wallet, start_wallet_params params = {});
void     start_daemon                  (daemon_t *daemons, int num_daemons = 1, start_daemon_params params = {});
daemon_t create_and_start_daemon       (start_daemon_params params = {});
void     create_and_start_multi_daemons(daemon_t *daemons, int num_daemons = 1, start_daemon_params params = {});

// TODO(doyle): The create and start wallet function launches the wallet
// separately to create on the commandline and immediately exits. Then launches
// again for the start part. This is wasteful, we can speed up tests by making
// it just reuse the same instance it did for creating.
wallet_t create_and_start_wallet(loki_nettype nettype = loki_nettype::testnet, start_wallet_params params = {});

#endif // LOKI_INTEGRATION_TEST_H
