#ifndef LOKI_INTEGRATION_TEST_H
#define LOKI_INTEGRATION_TEST_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <string>

// -------------------------------------------------------------------------------------------------
//
// macros & helpers
//
// -------------------------------------------------------------------------------------------------
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

#define LOCAL_PERSIST static
#define FILE_SCOPE static

template <typename T, size_t N>    constexpr size_t    array_count  (T (&)[N]) { return N; }
template <typename T, ptrdiff_t N> constexpr ptrdiff_t array_count_i(T (&)[N]) { return N; }
template <typename T, size_t N>    constexpr size_t    char_count  (T (&)[N]) { return N - 1; }
template <typename T, ptrdiff_t N> constexpr ptrdiff_t char_count_i(T (&)[N]) { return N - 1; }

#define LOKI_ARRAY_COUNT(array) (sizeof(array)/sizeof(array[0]))
#define LOKI_ARRAY_COUNT(array) (sizeof(array)/sizeof(array[0]))
#define LOKI_CHAR_COUNT(array) (LOKI_ARRAY_COUNT(array) - 1)
#define LOKI_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define LOKI_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define LOKI_ABS(a) (((a) < 0) ? -(a) : (a))
#define LOKI_MS_TO_NANOSECONDS(val) ((val) * 1000000ULL)
#define LOKI_MS_TO_SECONDS(val) ((val) / 1000)
#define LOKI_SECONDS_TO_MS(val) ((val) * 1000)
#define LOKI_MINUTES_TO_S(val) ((val) * 60)
#define LOKI_HOURS_TO_S(val) ((val) * LOKI_MINUTES_TO_S(60))
#define LOKI_DAYS_TO_S(val) ((val) * LOKI_HOURS_TO_S(24))
#define LOKI_FOR_EACH(iterator, limit) for (isize iterator = 0; iterator < (isize)(limit); ++iterator)
#define LOKI_FOR_ITERATOR(it, array ,count) for (auto it = array, end_it = array + count; it != end_it; it++)


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

// -------------------------------------------------------------------------------------------------
//
// strings
//
// -------------------------------------------------------------------------------------------------
struct loki_string
{
  union {
    char const *const_str;
    char *str;
  };
  int len;

  bool operator==(loki_string const &other) const
  {
    if (len != other.len) return false;
    return (strcmp(str, other.str) == 0);
  }
};
#define LOKI_STRING(str) {str, LOKI_CHAR_COUNT(str)}

template <int MAX = 8192>
struct loki_fixed_string
{
  loki_fixed_string() : len(0) { str[0] = 0; }
  loki_fixed_string(char const *fmt, ...)
  {
    va_list va;
    va_start(va, fmt);
    len = stbsp_vsnprintf(str, MAX, fmt, va);
    va_end(va);
    LOKI_ASSERT(len < MAX);
  }

  void append(char const *fmt, ...)
  {
    va_list va;
    va_start(va, fmt);
    int extra = stbsp_vsnprintf(str + len, MAX - len, fmt, va);
    va_end(va);
    LOKI_ASSERT(extra < MAX - len);
    len += extra;
  }

  void clear() { len = 0; str[0] = 0; }
  int  max()   { return MAX; };

  bool operator==(loki_fixed_string const &other) const
  {
    if (len != other.len) return false;
    return (memcmp(str, other.str, other.len) == 0);
  }

  loki_string to_string() const
  {
    loki_string result = {str, len};
    return result;
  }

  char str[MAX];
  int  len;
};

// -------------------------------------------------------------------------------------------------
//
// itest_ipc
//
// -------------------------------------------------------------------------------------------------
struct itest_ipc_pipe
{
  int              fd;
  loki_fixed_string<128> file;
};

struct itest_ipc
{
  itest_ipc_pipe read;
  itest_ipc_pipe write;
};
void itest_ipc_clean_up(itest_ipc *ipc);

struct itest_ipc_result
{
  bool                    failed;
  loki_fixed_string<1024> fail_string;
  std::string             output;
  operator bool() const { return !failed; }
};

// -------------------------------------------------------------------------------------------------
//
// itest
//
// -------------------------------------------------------------------------------------------------
struct itest_read_possible_value
{
  loki_string literal;
  bool        is_fail_msg;
};

const int ITEST_DEFAULT_TIMEOUT_MS = 100;
const int ITEST_INFINITE_TIMEOUT   = -1;
itest_ipc_result itest_write_to_stdin              (itest_ipc *ipc, char const *src);
itest_ipc_result itest_write_then_read_stdout      (itest_ipc *ipc, char const *src);
itest_ipc_result itest_write_then_read_stdout_until(itest_ipc *ipc, char const *src, loki_string find_str);
itest_ipc_result itest_write_then_read_stdout_until(itest_ipc *ipc, char const *src, itest_read_possible_value const *possible_values, int possible_values_len);
void             itest_read_stdout_sink            (itest_ipc *ipc, int seconds);
itest_ipc_result itest_read_stdout                 (itest_ipc *ipc);
itest_ipc_result itest_read_stdout_until           (itest_ipc *ipc, char const *find_str);
itest_ipc_result itest_read_stdout_until           (itest_ipc *ipc, itest_read_possible_value const *possible_values, int possible_values_len);
void             itest_read_until_then_write_stdin (itest_ipc *ipc, loki_string find_str, char const *src);

// -------------------------------------------------------------------------------------------------
//
// Loki Blockchain Primitives
//
// -------------------------------------------------------------------------------------------------
const int      LOKI_CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE     = 10;
const int      LOKI_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW    = 30;
const uint64_t LOKI_ATOMIC_UNITS                            = 1000000000ULL;
const uint64_t LOKI_FAKENET_STAKING_REQUIREMENT             = 100;
const int      LOKI_TARGET_DIFFICULTY                       = 120;
const int      LOKI_STAKING_EXCESS_BLOCKS                   = 20;
const int      LOKI_STAKING_REQUIREMENT_LOCK_BLOCKS         = LOKI_TARGET_DIFFICULTY / LOKI_DAYS_TO_S(30);
const int      LOKI_STAKING_REQUIREMENT_LOCK_BLOCKS_FAKENET = 30;
const int      LOKI_STAKING_REQUIREMENT_LOCK_BLOCKS_TESTNET = LOKI_TARGET_DIFFICULTY / LOKI_DAYS_TO_S(2);
const int      LOKI_REORG_SAFETY_BUFFER                     = 20;
const int      LOKI_STATE_CHANGE_QUORUM_SIZE                = 5;
const int      LOKI_CHECKPOINT_QUORUM_SIZE                  = 5;
const int      LOKI_MAX_NUM_CONTRIBUTORS                    = 4;
const int      LOKI_MAX_LOCKED_KEY_IMAGES                   = LOKI_MAX_NUM_CONTRIBUTORS * 1;
const int      LOKI_VOTE_LIFETIME                           = 60;
const int      LOKI_CHECKPOINT_INTERVAL                     = 4;
const int      LOKI_DECOMMISSION_INITIAL_CREDIT             = 60;

using loki_key_image                                        = loki_fixed_string<64  + 1>;
using loki_transaction_id                                   = loki_fixed_string<64  + 1>;
using loki_snode_key                                        = loki_fixed_string<64  + 1>;
using loki_payment_id16                                     = loki_fixed_string<16  + 1>;
using loki_payment_id64                                     = loki_fixed_string<64  + 1>;
using loki_hash64                                           = loki_fixed_string<64  + 1>;

struct loki_addr
{
  enum type_t { normal, integrated, };
  type_t type;
  void set_normal_addr    (char const *src) { type = type_t::normal;     buf.clear(); if (src[95] == ' ') buf.append("%.*s", 95, src); else buf.append("%.*s", 97, src); }
  void set_integrated_addr(char const *src) { type = type_t::integrated; buf.clear(); buf.append("%.*s", buf.max(), src); }
  // NOTE: Mainnet addresses are 95 chars, testnet addresses are 97 and + 1 for null terminator.
  // Integrated addresses are 108 characters
  // TODO(doyle): How long are testnet integrated addresses?
  loki_fixed_string<108 + 1> buf;
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

// -------------------------------------------------------------------------------------------------
//
// daemon
//
// -------------------------------------------------------------------------------------------------
struct start_daemon_params
{
  int                     fixed_difficulty = 1; // Set to 0 to disable
  bool                    service_node     = true;
  loki_hardfork           hardforks[16];        // If hardforks are specified, we run in mainnet/fakechain mode
  int                     num_hardforks;
  loki_nettype            nettype = loki_nettype::testnet;
  bool                    keep_terminal_open;
  loki_fixed_string<2048> custom_cmd_line;

  void add_hardfork                          (int version, int height); // TODO: Sets daemon mode to fakechain sadly, can't keep testnet. We should fix this
  void add_sequential_hardforks_until_version(int version);
  void load_latest_hardfork_versions();
};

struct daemon_t
{
  FILE     *proc_handle;
  int       id;
  bool      is_mining;
  int       p2p_port;
  int       rpc_port;
  int       zmq_rpc_port;
  int       quorumnet_port;
  itest_ipc ipc;
};

daemon_t create_daemon                 ();
void     start_daemon                  (daemon_t *daemons, int num_daemons, start_daemon_params *params, int num_params, char const *terminal_name);
daemon_t create_and_start_daemon       (start_daemon_params params, char const *terminal_name);
void     create_and_start_multi_daemons(daemon_t *daemons, int num_daemons, start_daemon_params *params, int num_params, char const *terminal_name);

// -------------------------------------------------------------------------------------------------
//
// wallet
//
// -------------------------------------------------------------------------------------------------
struct start_wallet_params
{
  daemon_t *daemon                          = nullptr;
  bool      allow_mismatched_daemon_version = false;
  bool      keep_terminal_open;
};

struct wallet_t
{
  FILE         *proc_handle;
  int           id;
  loki_nettype  nettype;
  uint64_t      balance;
  uint64_t      unlocked_balance;
  itest_ipc     ipc;
};

// TODO(doyle): The create and start wallet function launches the wallet
// separately to create on the commandline and immediately exits. Then launches
// again for the start part. This is wasteful, we can speed up tests by making
// it just reuse the same instance it did for creating.
wallet_t create_and_start_wallet(loki_nettype nettype, start_wallet_params params, char const *terminal_name);

#endif // LOKI_INTEGRATION_TEST_H
