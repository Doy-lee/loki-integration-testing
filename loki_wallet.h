#ifndef LOKI_WALLET_H
#define LOKI_WALLET_H

// Define LOKI_WALLET_IMPLEMENTATION in one CPP file

//
// Header
//
#include "loki_integration_tests.h"

struct wallet_params
{
  bool     disable_asking_password   = true;
  uint64_t refresh_from_block_height = 0;
};

void             wallet_set_default_testing_settings(wallet_params const params = {});

bool             wallet_address                     (int index, loki_addr *addr = nullptr); // Switch to subaddress at index
loki_addr        wallet_address_new                 ();
void             wallet_exit                        ();
uint64_t         wallet_get_balance                 (uint64_t *unlocked_balance);
void             wallet_refresh                     (int refresh_wait_time_in_ms = 2000);
bool             wallet_set_daemon                  (struct daemon_t const *daemon);
bool             wallet_stake                       (loki_snode_key const *service_node_key, loki_addr const *contributor_addr, uint64_t amount, loki_transaction_id *tx_id = nullptr);
uint64_t         wallet_status                      (); // return: The current height the wallet is synced at
loki_transaction wallet_transfer                    (char      const *dest, uint64_t amount); // TODO(doyle): We only support whole amounts. Not atomic units either.
loki_transaction wallet_transfer                    (loki_addr const *dest, uint64_t amount);
void             wallet_mine_atleast_n_blocks       (int num_blocks, int mining_duration_in_ms = 500);
inline void      wallet_mine_unlock_time_blocks     () { wallet_mine_atleast_n_blocks(30); }
void             wallet_mine_for_n_milliseconds     (int milliseconds);
uint64_t         wallet_mine_until_unlocked_balance (uint64_t desired_unlocked_balance, int mining_duration_in_ms = 500); // return: The actual unlocked balance, can vary by abit.
bool             wallet_register_service_node       (loki_scratch_buf const *registration_cmd);

#endif // LOKI_WALLET_H

//
// CPP Implementation
//
#ifdef LOKI_WALLET_IMPLEMENTATION

#include "loki_str.h"

#include <stdint.h>

#define LOKI_ATOMIC_UNITS 1000000000ULL

void wallet_set_default_testing_settings(wallet_params const params)
{ 
  shared_mem_type const wallet = shared_mem_type::wallet;
  // TODO(doyle): HACK: The sleeps are here because these cmds complete too quickly
  // and we might get blocked waiting reading the results of them too quickly
  // and skipping the other
  loki_buffer<64> ask_password("set ask-password %d", params.disable_asking_password ? 0 : 1);
  write_to_stdin_mem(wallet, ask_password.c_str);
  os_sleep_ms(1000);
  read_from_stdout_mem(wallet);

  loki_buffer<64> refresh_height("set refresh-from-block-height %zu", params.refresh_from_block_height);
  write_to_stdin_mem(wallet, refresh_height.c_str);
  os_sleep_ms(1000);
  read_from_stdout_mem(wallet);
}

bool wallet_address(int index, loki_addr *addr)
{
  if (addr) *addr = {};

  // Example
  // 1  TRr6hE8JxT1K8TCpQYbaN3Wm3A6MQpG9xQ3ryP7j7sUEgxLhk6b5soijjrhvuK2ZkZRnpeUdnVddzR1u5DYGBY1K2tZRn43zd  (Untitled address)
  loki_buffer<64> cmd("address %d", index);
  loki_scratch_buf output = write_to_stdin_mem_and_get_result(shared_mem_type::wallet, cmd.c_str);

  if (str_match(output.c_str, "Error: <index_min> is out of bound"))
    return false;

  char const *ptr = output.c_str;
  if (addr) *addr = str_skip_to_next_word(&ptr);
  return true;
}

loki_addr wallet_address_new()
{
  // Example
  // 1  TRr6hE8JxT1K8TCpQYbaN3Wm3A6MQpG9xQ3ryP7j7sUEgxLhk6b5soijjrhvuK2ZkZRnpeUdnVddzR1u5DYGBY1K2tZRn43zd  (Untitled address)
  loki_scratch_buf output = write_to_stdin_mem_and_get_result(shared_mem_type::wallet, "address new");
  char const *ptr         = output.c_str;
  char const *addr        = str_skip_to_next_word(&ptr);
  char const *addr_name   = str_skip_to_next_word(&ptr);
  assert(str_match(addr_name, "(Untitled address)"));

  loki_addr result = {};
  result.append("%.*s", result.max() - 1, addr);
  return result;
}

void wallet_exit()
{
  write_to_stdin_mem(shared_mem_type::wallet, "exit");
}

uint64_t wallet_get_balance(uint64_t *unlocked_balance)
{
  loki_scratch_buf src = write_to_stdin_mem_and_get_result(shared_mem_type::wallet, "balance");

  // Example
  // Balance: 0.000000000, unlocked balance: 0.000000000
  char const *ptr           = str_skip_whitespace(src.c_str);
  char const *balance_label = ptr;
  assert(str_match(ptr, "Balance: "));

  char const *balance_str = str_skip_to_next_word(&ptr);
  uint64_t balance        = str_parse_loki_amount(balance_str);

  char const *unlocked_balance_label = str_skip_to_next_word(&ptr);
  assert(str_match(ptr, "unlocked balance: "));

  os_sleep_ms(500);
  char const *unlocked_balance_str = str_skip_to_next_word(&ptr);
  *unlocked_balance                = str_parse_loki_amount(unlocked_balance_str);

  return balance;
}

void wallet_refresh(int refresh_wait_time_in_ms)
{
  write_to_stdin_mem_and_get_result(shared_mem_type::wallet, "refresh");
  // TODO(doyle): HACK because this spams the console with message_writers()
  // async, so we want to give it enough time to finish printing before moving
  // onto the next command, otherwise it will overwrite subsequent output in an
  // async manner
  os_sleep_ms(refresh_wait_time_in_ms);
}

bool wallet_stake(loki_snode_key const *service_node_key, loki_addr const *contributor_addr, uint64_t amount, loki_transaction_id *tx_id)
{
  loki_buffer<512> cmd("stake %s %s %zu", service_node_key->c_str, contributor_addr->c_str, amount);
  write_to_stdin_mem(shared_mem_type::wallet, cmd.c_str);

  // TODO(doyle): Skip this. Stake actually triggers a refresh in the wallet, so
  // it's going to spam some irrelevant output to us most likely and this will
  // overwrite what we receive in stdin shared mem.

  // To fix this, we need to make shared mem smarter and have a queue of
  // stdout lines to process. Because of this, just sleep and hope the refresh
  // is done in time.
  os_sleep_s(2);
#if 0
  if (!str_find(output.c_str, "Spending from address index "))
    return false;
#endif

  loki_scratch_buf output = write_to_stdin_mem_and_get_result(shared_mem_type::wallet, "y"); // Confirm?
  assert(str_find(output.c_str, "Transaction successfully submitted, transaction <"));

  if (tx_id)
  {
    char const *tx_id_start = str_find(output.c_str, "<");
    tx_id_start++;

    tx_id->append("%.*s", tx_id->max(), tx_id_start);
  }

  return true;
}

uint64_t wallet_status()
{
  loki_buffer<64> cmd("status");
  loki_scratch_buf output = write_to_stdin_mem_and_get_result(shared_mem_type::wallet, cmd.c_str);

  char const *ptr    = output.c_str;
  assert(str_match(ptr, "Refreshed"));

  char const *height = str_skip_to_next_word(&ptr);
  uint64_t result    = static_cast<uint64_t>(atoi(height));
  return result;
}

bool wallet_set_daemon(daemon_t const *daemon)
{
  loki_buffer<64> cmd("set_daemon 127.0.0.1:%d", daemon->rpc_port);
  loki_scratch_buf output = write_to_stdin_mem_and_get_result(shared_mem_type::wallet, cmd.c_str);
  bool result = str_match(output.c_str, "Daemon set to 127.0.0.1");
  return result;
}

loki_transaction wallet_transfer(char const *dest, uint64_t amount)
{
  loki_buffer<256> cmd("transfer %s %zu", dest, amount);
  loki_scratch_buf output = write_to_stdin_mem_and_get_result(shared_mem_type::wallet, cmd.c_str);

  // NOTE: No payment ID requested if sending to subaddress
  if (str_match(output.c_str, "No payment id is included with this transaction. Is this okay?"))
    output = write_to_stdin_mem_and_get_result(shared_mem_type::wallet, "y");

  loki_transaction result = {};
  result.dest             = dest;

  // TODO(doyle): Add an atomic amount to printable money string and check the amount here
  assert(str_find(output.c_str, "Spending from address index "));

  // Sanity check sending amount
  {
    char const *amount_label = str_find(output.c_str, "Sending ");
    char const *amount_str   = str_skip_to_next_digit(amount_label);
    assert(amount_label);
    result.atomic_amount = str_parse_loki_amount(amount_str);
    assert(result.atomic_amount == (amount * LOKI_ATOMIC_UNITS));
  }

  // Extract fee
  {
    char const *fee_label = str_find(output.c_str, "The transaction fee is");
    char const *fee_str   = str_skip_to_next_digit(fee_label);
    assert(fee_label);
    result.fee = str_parse_loki_amount(fee_str);
  }

  output = write_to_stdin_mem_and_get_result(shared_mem_type::wallet, "y"); // Is this okay?

  // Extract TX ID
  {
    assert(str_find(output.c_str, "Transaction successfully submitted, transaction <"));
    char const *id_start = str_find(output.c_str, "<");
    result.id.append("%.*s", result.id.max(), ++id_start);
  }

  return result;
}

loki_transaction wallet_transfer(loki_addr const *dest, uint64_t amount)
{
  loki_transaction result = wallet_transfer(dest->c_str, amount);
  return result;
}

void wallet_mine_atleast_n_blocks(int num_blocks, int mining_duration_in_ms)
{
  for (uint64_t start_height = wallet_status();;)
  {
    wallet_mine_for_n_milliseconds(mining_duration_in_ms);
    wallet_refresh(mining_duration_in_ms * 2 /*refresh_wait_time_in_ms*/);
    uint64_t delta_height = wallet_status() - start_height;
    if (delta_height > num_blocks)
      break;
  }
}

void wallet_mine_for_n_milliseconds(int milliseconds)
{
  loki_scratch_buf output = write_to_stdin_mem_and_get_result(shared_mem_type::wallet, "start_mining");
  assert(str_match(output.c_str, "Mining started in daemon"));

  os_sleep_ms(milliseconds);

  output = write_to_stdin_mem_and_get_result(shared_mem_type::wallet, "stop_mining");
  assert(str_match(output.c_str, "Mining stopped in daemon"));
}

uint64_t wallet_mine_until_unlocked_balance(uint64_t desired_unlocked_balance, int mining_duration_in_ms)
{
  uint64_t unlocked_balance = 0;
  for (;unlocked_balance < desired_unlocked_balance;)
  {
    wallet_mine_for_n_milliseconds(mining_duration_in_ms);
    wallet_refresh                (mining_duration_in_ms * 2 /*refresh_wait_time_in_ms*/);
    wallet_get_balance            (&unlocked_balance);
  }

  return unlocked_balance;
}

bool wallet_register_service_node(char const *registration_cmd)
{
  loki_scratch_buf output = write_to_stdin_mem_and_get_result(shared_mem_type::wallet, registration_cmd);
  output                  = write_to_stdin_mem_and_get_result(shared_mem_type::wallet, "y"); // Confirm?
  return true;
}

#endif
