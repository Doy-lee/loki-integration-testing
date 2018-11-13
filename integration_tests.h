#ifndef LOKI_INTEGRATION_TEST_H
#define LOKI_INTEGRATION_TEST_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "external/shoom.h"
#include "external/stb_sprintf.h"

#define LOKI_ARRAY_COUNT(array) (sizeof(array)/sizeof(array[0]))
#define LOKI_CHAR_COUNT(array) (LOKI_ARRAY_COUNT(array) - 1)
#define LOKI_MIN(a, b) (((a) < (b)) ? (a) : (b))

struct state_t
{
    int num_wallets       = 0;
    int num_daemons       = 0;
    int free_p2p_port     = 1111;
    int free_rpc_port     = 2222;
    int free_zmq_rpc_port = 3333;

    shoom::Shm wallet_stdout_shared_mem{"loki_integration_testing_wallet_stdout", 8192};
    shoom::Shm wallet_stdin_shared_mem {"loki_integration_testing_wallet_stdin",  8192};
    shoom::Shm daemon_stdout_shared_mem{"loki_integration_testing_daemon_stdout", 8192};
    shoom::Shm daemon_stdin_shared_mem {"loki_integration_testing_daemon_stdin",  8192};
};

struct daemon_t
{
    FILE *proc_handle;

    bool is_mining;
    int p2p_port;
    int rpc_port;
    int zmq_rpc_port;
    char data_dir[128];
};

struct wallet_t
{
    FILE    *proc_handle;

    int      id;
    uint64_t balance;
    uint64_t unlocked_balance;
};

template <int MAX>
struct loki_buffer
{
  loki_buffer() : len(0) { data[0] = 0; }
  loki_buffer(char const *fmt, ...)     { va_list va; va_start(va, fmt); len =  stbsp_vsnprintf(data, MAX, fmt, va);                                va_end(va); assert(len   < MAX); }
  void append(char const *fmt, ...)     { va_list va; va_start(va, fmt); int extra = stbsp_vsnprintf(data + len, MAX - len, fmt, va); len += extra; va_end(va); assert(extra < MAX - len); }

  int  max() { return MAX; };

  union
  {
    char data[MAX];
    char c_str[MAX];
  };

  int  len;
};

using loki_scratch_buf = loki_buffer<8192>;


enum struct shared_mem_type { wallet, daemon };
void             write_to_stdin_mem               (shared_mem_type type, char const *cmd, int cmd_len = -1);
loki_scratch_buf read_from_stdout_mem             (shared_mem_type type);
loki_scratch_buf write_to_stdin_mem_and_get_result(shared_mem_type type, char const *cmd, int cmd_len = -1);

void             os_sleep_ms                      (int ms);
bool             os_file_delete                   (char const *path);

enum struct reset_type { all, daemon, wallet };
void             reset_shared_memory(reset_type type = reset_type::all);

wallet_t         create_wallet();
void             start_wallet(wallet_t *wallet);
daemon_t         start_daemon();

#endif // LOKI_INTEGRATION_TEST_H
