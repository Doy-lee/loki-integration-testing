#ifndef LOKI_INTEGRATION_TEST_H
#define LOKI_INTEGRATION_TEST_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "external/shoom.h"
#include "external/stb_sprintf.h"

//
// Primitives
//

#define LOKI_ASSERT(expr) \
  if (!(expr)) \
  { \
    fprintf(stderr, "%s:%d [" #expr "] \n", __FILE__, __LINE__); \
    assert(expr); \
  }

#define LOKI_ASSERT_MSG(expr, fmt, ...) \
  if (!(expr)) \
  { \
    fprintf(stderr, "%s:%d [" #expr "] " fmt "\n", __FILE__, __LINE__, ## __VA_ARGS__); \
    assert(expr); \
  }


#define LOKI_ARRAY_COUNT(array) (sizeof(array)/sizeof(array[0]))
#define LOKI_CHAR_COUNT(array) (LOKI_ARRAY_COUNT(array) - 1)
#define LOKI_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define LOKI_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define LOKI_ABS(a) (((a) < 0) ? (-a) : (a))
#define LOKI_MS_TO_SECONDS(val) ((val) / 1000)
#define LOKI_SECONDS_TO_MS(val) ((val) * 1000)
#define LOKI_MINUTES_TO_S(val) ((val) * 60)
#define LOKI_HOURS_TO_S(val) ((val) * LOKI_MINUTES_TO_S(60))
#define LOKI_DAYS_TO_S(val) ((val) * LOKI_HOURS_TO_S(24))

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

  int  max() { return MAX; };

  union
  {
    char data[MAX];
    char c_str[MAX];
  };

  int  len;
};

using loki_scratch_buf = loki_buffer<8192>;

void             os_sleep_s                       (int seconds);
void             os_sleep_ms                      (int ms);
bool             os_file_delete                   (char const *path);

//
// Integration Test Primitives
//

enum struct itest_shared_mem_type  { wallet, daemon };
enum struct itest_reset_type { all, daemon, wallet };
void             itest_write_to_stdin_mem               (itest_shared_mem_type type, char const *cmd, int cmd_len = -1);
loki_scratch_buf itest_read_from_stdout_mem             (itest_shared_mem_type type);
loki_scratch_buf itest_write_to_stdin_mem_and_get_result(itest_shared_mem_type type, char const *cmd, int cmd_len = -1);
void             itest_reset_shared_memory              (itest_reset_type type = itest_reset_type::all);

//
// Loki Blockchain Primitives
//

const int      LOKI_CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE     = 10;
const int      LOKI_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW    = 30;
const uint64_t LOKI_ATOMIC_UNITS                            = 1000000000ULL;
const int      LOKI_TARGET_DIFFICULTY                       = 120;
const int      LOKI_STAKING_EXCESS_BLOCKS                   = 20;
const int      LOKI_STAKING_REQUIREMENT_LOCK_BLOCKS         = LOKI_TARGET_DIFFICULTY / LOKI_DAYS_TO_S(30);
const int      LOKI_STAKING_REQUIREMENT_LOCK_BLOCKS_TESTNET = LOKI_TARGET_DIFFICULTY / LOKI_DAYS_TO_S(2);
using loki_addr                                             = loki_buffer<97 + 1>; // NOTE: Mainnet addresses are 95 chars, testnet addresses are 97 and + 1 for null terminator.
using loki_transaction_id                                   = loki_buffer<64 + 1>;
using loki_snode_key                                        = loki_buffer<64 + 1>;

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
  FILE *proc_handle;
  int   id;
  bool  is_mining;
  int   p2p_port;
  int   rpc_port;
  int   zmq_rpc_port;
};

struct wallet_t
{
  FILE         *proc_handle;
  int           id;
  loki_nettype  nettype;
  uint64_t      balance;
  uint64_t      unlocked_balance;
};

struct start_daemon_params
{
  int           fixed_difficulty = 1; // Set to 0 to disable
  bool          service_node     = true;
  loki_hardfork hardforks[16];        // If hardforks are specified, we run in mainnet/fakechain mode
  int           num_hardforks;
  loki_nettype  nettype          = loki_nettype::testnet;

  void add_hardfork(int version, int height); // NOTE: Sets nettype to mainnet
};

struct start_wallet_params
{
  daemon_t *daemon                          = nullptr;
  bool      allow_mismatched_daemon_version = false;
};

wallet_t create_wallet          (loki_nettype type);
daemon_t create_daemon          ();
void     start_wallet           (wallet_t *wallet, start_wallet_params params = {});
void     start_daemon           (daemon_t *daemon, start_daemon_params params = {});
daemon_t create_and_start_daemon(start_daemon_params params = {});
wallet_t create_and_start_wallet(loki_nettype nettype = loki_nettype::testnet, start_wallet_params params = {});

#endif // LOKI_INTEGRATION_TEST_H
