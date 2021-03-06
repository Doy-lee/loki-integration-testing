#ifndef LOKI_WALLET_H
#define LOKI_WALLET_H

// Define LOKI_WALLET_IMPLEMENTATION in one CPP file

//
// Header
//

#include "loki_os.h"
#include "loki_integration_tests.h"
#include "loki_daemon.h"

struct wallet_params
{
  bool     disable_asking_password   = true;
  uint64_t refresh_from_block_height = 0;
};

// TODO(doyle): This function should probably run by default since you almost always want it
void                 wallet_set_default_testing_settings (wallet_t *wallet, wallet_params const params = {});
bool                 wallet_address                      (wallet_t *wallet, int index, loki_addr *addr = nullptr); // Switch to subaddress at index
bool                 wallet_address_new                  (wallet_t *wallet, loki_addr *addr);
uint64_t             wallet_balance                      (wallet_t *wallet, uint64_t *unlocked_balance);
void                 wallet_exit                         (wallet_t *wallet);
bool                 wallet_integrated_address           (wallet_t *wallet, loki_addr *addr);
bool                 wallet_payment_id                   (wallet_t *wallet, loki_payment_id64 *id);

struct wallet_blacklisted_stake
{
  loki_key_image key_image;
  uint64_t       unlock_height;
};

struct wallet_locked_amount
{
  uint64_t amount;
};

struct wallet_locked_stake
{
  loki_snode_key       snode_key;
  uint64_t             unlock_height;
  uint64_t             total_locked_amount;
  uint64_t             locked_amounts[4];
  int                  locked_amounts_len;
};

struct wallet_locked_stakes
{
  wallet_locked_stake      locked_stakes[8];
  int                      locked_stakes_len;

  wallet_blacklisted_stake blacklisted_stakes[8];
  int                      blacklisted_stakes_len;
};

wallet_locked_stakes wallet_print_locked_stakes          (wallet_t *wallet);
bool                 wallet_refresh                      (wallet_t *wallet);
bool                 wallet_set_daemon                   (wallet_t *wallet, struct daemon_t const *daemon);
bool                 wallet_stake                        (wallet_t *wallet, loki_snode_key const *service_node_key, uint64_t amount, loki_transaction_id *tx_id = nullptr);
bool                 wallet_status                       (wallet_t *wallet, uint64_t *height); // height: The current height the wallet is synced at

// TODO(doyle): Need to support integrated address
bool                 wallet_sweep_all                    (wallet_t *wallet, char      const *dest, loki_transaction *tx);
bool                 wallet_transfer                     (wallet_t *wallet, char      const *dest, uint64_t amount, loki_transaction *tx); // TODO(doyle): We only support whole amounts. Not atomic units either.
bool                 wallet_transfer                     (wallet_t *wallet, loki_addr const *dest, uint64_t amount, loki_transaction *tx);

// TODO(doyle): We should be able to roughly calculate how much blocks we need to mine
uint64_t             wallet_mine_until_unlocked_balance  (wallet_t *wallet, daemon_t *daemon, uint64_t desired_unlocked_balance, int blocks_between_check = LOKI_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW);

// TODO(doyle): This should return the transaction
bool                 wallet_request_stake_unlock         (wallet_t *wallet, loki_snode_key const *snode_key, uint64_t *unlock_height = nullptr);
bool                 wallet_register_service_node        (wallet_t *wallet, char const *registration_cmd, loki_transaction *tx = nullptr);

#endif // LOKI_WALLET_H

//
// CPP Implementation
//
#ifdef LOKI_WALLET_IMPLEMENTATION

#include "loki_str.h"
#include <stdint.h>

void wallet_set_default_testing_settings(wallet_t *wallet, wallet_params const params)
{ 
  loki_fixed_string<64> refresh_height("set refresh-from-block-height %zu", params.refresh_from_block_height);
  itest_write_then_read_stdout_until(&wallet->ipc, refresh_height.str, LOKI_STRING("Wallet password"));
  loki_fixed_string<64> ask_password  ("set ask-password %d", params.disable_asking_password ? 0 : 1);
  itest_write_to_stdin(&wallet->ipc, ask_password.str);
}

bool wallet_address(wallet_t *wallet, int index, loki_addr *addr)
{
  if (addr) *addr = {};

  loki_fixed_string<64> cmd("address %d", index);

  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STRING("Error: <index_min> is out of bound"), true},
    {LOKI_STRING("Primary address"), false},
    {LOKI_STRING("Untitled address"), false},
  };

  itest_read_result output = itest_write_then_read_stdout_until(&wallet->ipc, cmd.str, possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (possible_values[output.matching_find_strs_index].is_fail_msg)
    return false;

  // Example
  // 1  TRr6hE8JxT1K8TCpQYbaN3Wm3A6MQpG9xQ3ryP7j7sUEgxLhk6b5soijjrhvuK2ZkZRnpeUdnVddzR1u5DYGBY1K2tZRn43zd  (Untitled address)
  char const *ptr = output.buf.c_str();

  if (addr)
  {
    loki_fixed_string<128> find = loki_fixed_string<128>("%d  ", index);
    char const *start     = str_find(ptr, find.str);
    start += find.len;
    char const *end   = str_skip_to_next_whitespace(start);
    int len           = static_cast<int>(end - start);
    LOKI_ASSERT(len > 0);
    addr->set_normal_addr(start);
  }

  return true;
}

bool wallet_address_new(wallet_t *wallet, loki_addr *addr)
{
  // Example
  // 1  TRr6hE8JxT1K8TCpQYbaN3Wm3A6MQpG9xQ3ryP7j7sUEgxLhk6b5soijjrhvuK2ZkZRnpeUdnVddzR1u5DYGBY1K2tZRn43zd  (Untitled address)
  itest_read_result output = itest_write_then_read_stdout(&wallet->ipc, "address new");
  char const *ptr          = output.buf.c_str();
  char const *addr_str     = str_skip_to_next_word_inplace(&ptr);
  char const *addr_name    = str_skip_to_next_word_inplace(&ptr);
  assert(str_match(addr_name, "(Untitled address)"));
  addr->set_normal_addr(addr_str);
  return true;
}

uint64_t wallet_balance(wallet_t *wallet, uint64_t *unlocked_balance)
{
  // Example
  // Balance: 0.000000000, unlocked balance: 0.000000000
  itest_read_result output = itest_write_then_read_stdout_until(&wallet->ipc, "balance", LOKI_STRING("Balance: "));
  char const *ptr           = output.buf.c_str();

  loki_string balance_lit = LOKI_STRING("Balance: ");
  char const *balance_str  = str_find(ptr, balance_lit.str);
  balance_str              = str_skip_to_next_digit(balance_str);
  uint64_t result          = str_parse_loki_amount(balance_str);

  if (unlocked_balance)
  {
    char const *unlocked_balance_label = str_find(ptr, "unlocked balance: ");
    LOKI_ASSERT(unlocked_balance_label);
    char const *unlocked_balance_str = str_skip_to_next_digit(unlocked_balance_label);
    *unlocked_balance                = str_parse_loki_amount(unlocked_balance_str);
  }

  return result;
}

void wallet_exit(wallet_t *wallet)
{
  itest_write_to_stdin(&wallet->ipc, "exit");
  itest_ipc_clean_up(&wallet->ipc);
}

bool wallet_integrated_address(wallet_t *wallet, loki_addr *addr)
{
  // Example
  // Random payment ID: <d774b8dbac3b1c72>
  // Matching integrated address: TGAv1vAJmWm64Agf5uPZp87FaHsSxGMKbJpF5RTqcoNCGMciaqKcw8bVv4p5XeY9kZ7RnRjVpdrtLMosWHH85Kt1crWcWwZH9YN1FAg7NR2d 
  itest_read_result output = itest_write_then_read_stdout_until(&wallet->ipc, "integrated_address", LOKI_STRING("Matching integrated address"));
  LOKI_ASSERT_MSG(str_find(output.buf.c_str(), "Matching integrated address: "), "Failed to match in: %s", output.buf.c_str());
  char const *start = str_find(output.buf.c_str(), "Matching integrated address:");
  start             = str_find(start, ":");
  start             = str_skip_to_next_alphanum(start);

  addr->set_integrated_addr(start);
  return true;
}

bool wallet_payment_id(wallet_t *wallet, loki_payment_id64 *id)
{
  itest_read_result output = itest_write_then_read_stdout(&wallet->ipc, "payment_id");
  // Example
  // Random payment ID: <73e4d298a578a80187c0894971a53f7a997ff1ca63b709cc9d387df92344f96f>
  LOKI_ASSERT(str_match(output.buf.c_str(), "Random payment ID: "));
  char const *start = str_find(output.buf.c_str(), "<");
  start++;

  id->append("%.*s", LOKI_MIN(output.buf.size(), sizeof(id->str)), start);
  return true;
}

wallet_locked_stakes wallet_print_locked_stakes(wallet_t *wallet)
{
  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STRING("No locked stakes known for this wallet on the network"), false},
    {LOKI_STRING("Unlock Height"), false},
  };

  itest_read_result output = itest_write_then_read_stdout_until(&wallet->ipc, "print_locked_stakes", possible_values, LOKI_ARRAY_COUNT(possible_values));
  wallet_locked_stakes result = {};

  char const *output_ptr = output.buf.c_str();
  while(char const *service_node_str = str_find(output_ptr, "Service Node: ")) // Parse out the locked stakes
  {
    LOKI_ASSERT(result.locked_stakes_len < (int)LOKI_ARRAY_COUNT(result.locked_stakes));
    wallet_locked_stake *stake = result.locked_stakes + result.locked_stakes_len++;
    service_node_str = str_skip_to_next_word(service_node_str);
    service_node_str = str_skip_to_next_word(service_node_str);
    stake->snode_key = service_node_str;

    char const *height_label = str_find(service_node_str, "Unlock Height: ");
    char const *height_str   = str_skip_to_next_word(height_label);
    height_str               = str_skip_to_next_word(height_str);
    stake->unlock_height     = atoi(height_str);

    char const *total_locked_label = str_find(height_str, "Total Locked: ");
    char const *total_locked_str   = str_skip_to_next_digit(total_locked_label);
    stake->total_locked_amount     = str_parse_loki_amount(total_locked_str);

    char const *amount_label = str_find(total_locked_str, "Amount/Key Image");
    char const *amount_str   = str_skip_to_next_digit(amount_label);
    stake->locked_amounts[stake->locked_amounts_len++] = str_parse_loki_amount(amount_str);
    for(;;)
    {
      while(amount_str[0] != '\n') amount_str++;
      amount_str = str_skip_whitespace(amount_str);

      output_ptr = amount_str;
      if (!char_is_num(amount_str[0]))
        break;

      LOKI_ASSERT(stake->locked_amounts_len < (int)LOKI_ARRAY_COUNT(stake->locked_amounts));
      stake->locked_amounts[stake->locked_amounts_len++] = str_parse_loki_amount(amount_str);
    }
  }

  while(char const *blacklist_str = str_find(output_ptr, "Blacklisted Stakes")) // Parse out the blacklisted stakes
  {
    LOKI_ASSERT(result.blacklisted_stakes_len < (int)LOKI_ARRAY_COUNT(result.blacklisted_stakes));
    wallet_blacklisted_stake *stake = result.blacklisted_stakes + result.blacklisted_stakes_len++;

    char const *label        = str_find(blacklist_str, "Unlock Height/Key Image: ");
    char const *height_str   = str_skip_to_next_digit(label);
    stake->unlock_height     = atoi(height_str);

    char const *key_image_str = height_str;
    while(key_image_str[0] != '/') key_image_str++;
    key_image_str++;

    stake->key_image = key_image_str;
    output_ptr = key_image_str;
  }

  return result;
}

bool wallet_refresh(wallet_t *wallet)
{
  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STRING("Error: refresh failed"), true},
    {LOKI_STRING("Error: refresh failed unexpected error: proxy exception in refresh thread"), true},
    {LOKI_STRING("Balance"),               false},
  };

  itest_read_result output = itest_write_then_read_stdout_until(&wallet->ipc, "refresh", possible_values, LOKI_ARRAY_COUNT(possible_values));
  return possible_values[output.matching_find_strs_index].is_fail_msg;
}

bool wallet_set_daemon(wallet_t *wallet, daemon_t const *daemon)
{
  loki_fixed_string<64> cmd("set_daemon 127.0.0.1:%d", daemon->rpc_port);
  itest_write_then_read_stdout_until(&wallet->ipc, cmd.str, LOKI_STRING("Daemon set to"));
  return true;
}

bool wallet_stake(wallet_t *wallet, loki_snode_key const *service_node_key, uint64_t amount, loki_transaction_id *tx_id)
{
  loki_fixed_string<512> cmd("stake %s %zu", service_node_key->str, amount);

  // Staking 25.000000000 for 1460 blocks a total fee of 0.076725600.  Is this okay?  (Y/Yes/N/No):
  {
    itest_read_possible_value const possible_values[] =
    {
      {LOKI_STRING("Exception thrown, staking process could not be completed"), true},
      {LOKI_STRING("Payment IDs cannot be used in a staking transaction"), true},
      {LOKI_STRING("Subaddresses cannot be used in a staking transaction"), true},
      {LOKI_STRING("The specified address must be owned by this wallet and be the primary address of the wallet"), true},
      {LOKI_STRING("Failed to query daemon for service node list"), true},
      {LOKI_STRING("Could not find service node in service node list, please make sure it is registered first."), true},
      {LOKI_STRING("Could not query the current network version, try later"), true},
      {LOKI_STRING("Could not query the current network block height, try later"), true},
      {LOKI_STRING("The service node cannot receive any more Loki from this wallet"), true},
      {LOKI_STRING("The service node already has the maximum number of participants and this wallet is not one of them"), true},
      {LOKI_STRING("You must contribute at least"), true},
      {LOKI_STRING("Constructed too many transations, please sweep_all first"), true},
      {LOKI_STRING("Error: No outputs found, or daemon is not ready"), true},
      {LOKI_STRING("Is this okay?"), false},
    };

    itest_read_result output = itest_write_then_read_stdout_until(&wallet->ipc, cmd.str, possible_values, LOKI_ARRAY_COUNT(possible_values));
    if (possible_values[output.matching_find_strs_index].is_fail_msg)
      return false;
  }

  itest_read_result output = itest_write_then_read_stdout_until(&wallet->ipc, "y", LOKI_STRING("You can check its status by using the `show_transfers` command"));
  if (!str_find(output.buf.c_str(), "Transaction successfully submitted, transaction <"))
    return false;

  if (tx_id)
  {
    *tx_id = {};
    char const *tx_id_start = str_find(output.buf.c_str(), "<");
    tx_id_start++;
    tx_id->append("%.*s", tx_id->max(), tx_id_start);
  }
  return true;
}

uint64_t wallet_status(wallet_t *wallet)
{
  // Refreshed N/K, synced, daemon RPC vX.Y
  itest_read_result output = itest_write_then_read_stdout_until(&wallet->ipc, "status", LOKI_STRING("Refreshed"));
  char const *ptr        = output.buf.c_str();
  char const *height_str = str_skip_to_next_digit(ptr);
  uint64_t result        = static_cast<uint64_t>(atoi(height_str));
  return result;
}

bool wallet_sweep_all(wallet_t *wallet, char const *dest, loki_transaction *tx)
{
  // TODO(doyle): Failure states
  // Example:
  // Transaction 1/1: ...
  // Sweeping 30298.277954908 for a total fee of 2.237828840.  Is this okay?  (Y/Yes/N/No):
  loki_fixed_string<128> cmd("sweep_all %s", dest);
  itest_write_to_stdin(&wallet->ipc, cmd.str);
  itest_read_result output = itest_read_stdout_until(&wallet->ipc, "Transaction 1/");

  // Sending amount
  char const *tx_divisor   = str_find(output.buf.c_str(), "/");
  char const *num_txs_str  = str_skip_to_next_digit(tx_divisor);
  char const *amount_label = str_find(num_txs_str, "Sweeping ");
  char const *amount_str   = str_skip_to_next_digit(amount_label);
  uint64_t atomic_amount   = str_parse_loki_amount(amount_str);

  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STRING("was rejected by daemon"), true},
    {LOKI_STRING("You can check its status by using the `show_transfers` command"), false},
  };

  output = itest_write_then_read_stdout_until(&wallet->ipc, "y", possible_values, LOKI_ARRAY_COUNT(possible_values));

  if (tx)
  {
    // TODO(doyle): Incomplete
    tx->dest.set_normal_addr(dest);
    tx->atomic_amount = atomic_amount;
  }

  return true;
}

bool wallet_transfer(wallet_t *wallet, char const *dest, uint64_t amount, loki_transaction *tx)
{
  loki_fixed_string<256> cmd("transfer %s %zu", dest, amount);
  itest_read_result output = itest_write_then_read_stdout_until(&wallet->ipc, cmd.str, LOKI_STRING("Is this okay?"));

  // NOTE: Payment ID deprecated
#if 0
  // NOTE: No payment ID requested if sending to subaddress
  bool requested_payment_id = str_find(output.buf.c_str(), "No payment id is included with this transaction. Is this okay?");
  if (requested_payment_id)
  {
    // Confirm no payment id
    output = itest_write_then_read_stdout_until(&wallet->ipc, "y", LOKI_STRING("Is this okay?"));
  }
#endif

  // Example:
  // Transaction 1/1:
  // Spending from address index 0
  // Sending 29.000000000.  The transaction fee is 0.071312220
  // Is this okay?  (Y/Yes/N/No):

  // Sanity check sending amount
  char const *amount_label = str_find(output.buf.c_str(), "Sending ");
  char const *amount_str   = str_skip_to_next_digit(amount_label);
  assert(amount_label);
  uint64_t atomic_amount = str_parse_loki_amount(amount_str);
  assert(atomic_amount == (amount * LOKI_ATOMIC_UNITS));

  if (tx)
  {
    tx->atomic_amount = atomic_amount;
    tx->dest.set_normal_addr(dest);

    // Extract fee
    {
      char const *fee_label = str_find(output.buf.c_str(), "The transaction fee is");
      if (!fee_label) fee_label = str_find(output.buf.c_str(), "a total fee of");

      char const *fee_str   = str_skip_to_next_digit(fee_label);
      LOKI_ASSERT_MSG(fee_label, "Could not find the fee label in: %s", output.buf.c_str());
      tx->fee = str_parse_loki_amount(fee_str);
    }
  }

  output = itest_write_then_read_stdout_until(&wallet->ipc, "y", LOKI_STRING("You can check its status by using the `show_transfers` command"));
  if (tx) // Extract TX ID
  {
    assert(str_find(output.buf.c_str(), "Transaction successfully submitted, transaction <"));
    char const *id_start = str_find(output.buf.c_str(), "<");
    tx->id.append("%.*s", tx->id.max(), ++id_start);
  }

  return true;
}

bool wallet_transfer(wallet_t *wallet, loki_addr const *dest, uint64_t amount, loki_transaction *tx)
{
  bool result = wallet_transfer(wallet, dest->buf.str, amount, tx);
  return result;
}

uint64_t wallet_mine_until_unlocked_balance(wallet_t *wallet, daemon_t *daemon, uint64_t desired_unlocked_balance, int blocks_between_check)
{
  uint64_t unlocked_balance = 0;

  loki_addr addr = {};
  if (wallet_address(wallet, 0, &addr))
  {
    for (wallet_balance(wallet, &unlocked_balance); unlocked_balance < desired_unlocked_balance;)
    {
      daemon_mine_n_blocks(daemon, &addr, blocks_between_check);
      wallet_refresh(wallet);
      wallet_balance(wallet, &unlocked_balance);
    }
  }

  return unlocked_balance;
}

bool wallet_request_stake_unlock(wallet_t *wallet, loki_snode_key const *snode_key, uint64_t *unlock_height)
{
  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STRING("Failed to generate signature to sign request. The key image: "), true},
    {LOKI_STRING("Failed to parse hex representation of key image"), true},
    {LOKI_STRING("unable to get network blockchain height from daemon: "), true},
    {LOKI_STRING("No contributions recognised by this wallet in service node: "), true},
    {LOKI_STRING("Unexpected 0 contributions in service node for this wallet "), true},
    {LOKI_STRING("No service node is known for: "), true},
    {LOKI_STRING("has already been requested to be unlocked, unlocking at height: "), true},
    {LOKI_STRING("You will continue receiving rewards until"), false},
  };

  loki_fixed_string<256> cmd("request_stake_unlock %s", snode_key->str);
  itest_read_result output = itest_write_then_read_stdout_until(&wallet->ipc, cmd.str, possible_values, LOKI_ARRAY_COUNT(possible_values));

  if (possible_values[output.matching_find_strs_index].is_fail_msg)
    return false;

  if (unlock_height)
  {
    char const *expiring_str      = str_find(output.buf.c_str(), "You will continue receiving rewards until the service node expires at the estimated height: ");
    char const *unlock_height_str = str_skip_to_next_digit(expiring_str);
    *unlock_height = static_cast<uint64_t>(atoi(unlock_height_str));
  }

  itest_read_possible_value const possible_values2[] =
  {
    {LOKI_STRING("Error: Reason: "), true},
    {LOKI_STRING("You can check its status by using the `show_transfers` command"), false},
  };
  output = itest_write_then_read_stdout_until(&wallet->ipc, "y", possible_values2, LOKI_ARRAY_COUNT(possible_values2));
  return !possible_values2[output.matching_find_strs_index].is_fail_msg;
}

bool wallet_register_service_node(wallet_t *wallet, char const *registration_cmd, loki_transaction *tx)
{
  // Staking X for X blocks a total fee of X. Is this okay?
  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STRING("Is this okay?"), false},
    {LOKI_STRING("Error: This service node is already registered"), true},
  };

  itest_read_result output = itest_write_then_read_stdout_until(&wallet->ipc, registration_cmd, possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (possible_values[output.matching_find_strs_index].is_fail_msg)
    return false;

  if (tx)
  {
    char const *amount_label = str_find(output.buf.c_str(), "Staking ");
    char const *amount_str   = str_skip_to_next_digit(amount_label);
    assert(amount_label);
    uint64_t atomic_amount = str_parse_loki_amount(amount_str);

    tx->atomic_amount = atomic_amount;
    // Extract fee
    {
      char const *fee_label = str_find(output.buf.c_str(), "The transaction fee is");
      if (!fee_label) fee_label = str_find(output.buf.c_str(), "a total fee of");

      char const *fee_str   = str_skip_to_next_digit(fee_label);
      LOKI_ASSERT_MSG(fee_label, "Could not find the fee label in: %s", output.buf.c_str());
      tx->fee = str_parse_loki_amount(fee_str);
    }
  }

  output = itest_write_then_read_stdout_until(&wallet->ipc, "y", LOKI_STRING("Transaction successfully submitted, transaction <"));
  if (tx) // Extract TX ID
  {
    char const *id_start = str_find(output.buf.c_str(), "<");
    tx->id.append("%.*s", tx->id.max(), ++id_start);
  }

  return true;
}

#endif
