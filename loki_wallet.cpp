// -------------------------------------------------------------------------------------------------
//
// implementation
//
// -------------------------------------------------------------------------------------------------
void wallet_set_default_testing_settings(wallet_t *wallet, wallet_params const params)
{
  loki_fixed_string<64> refresh_height("set refresh-from-block-height %zu", params.refresh_from_block_height);
  itest_write_then_read_stdout_until(&wallet->ipc, refresh_height.str, LOKI_STRING("Wallet password"));
  loki_fixed_string<64> ask_password  ("set ask-password %d", params.disable_asking_password ? 0 : 1);
  itest_write_to_stdin(&wallet->ipc, ask_password.str);
}

itest_ipc_result wallet_address(wallet_t *wallet, int index, loki_addr *addr)
{
  if (addr) *addr = {};

  loki_fixed_string<64> cmd("address %d", index);
  LOCAL_PERSIST itest_read_possible_value const possible_values[] =
  {
    {LOKI_STRING("Error: <index_min> is out of bound"), true},
    {LOKI_STRING("Primary address"), false},
    {LOKI_STRING("Untitled address"), false},
  };

  itest_ipc_result ipc_result = itest_write_then_read_stdout_until(&wallet->ipc, cmd.str, possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (ipc_result)
  {
    // Example
    // 1  TRr6hE8JxT1K8TCpQYbaN3Wm3A6MQpG9xQ3ryP7j7sUEgxLhk6b5soijjrhvuK2ZkZRnpeUdnVddzR1u5DYGBY1K2tZRn43zd  (Untitled address)
    char const *ptr = ipc_result.output.c_str();

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
  }

  return ipc_result;
}

static bool wallet__parse_address_line(char const *src, bool primary_address, loki_addr *addr)
{
#if 0
  1  TRr6hE8JxT1K8TCpQYbaN3Wm3A6MQpG9xQ3ryP7j7sUEgxLhk6b5soijjrhvuK2ZkZRnpeUdnVddzR1u5DYGBY1K2tZRn43zd  (Untitled address)
#endif

  loki_string const PRIMARY_LABEL = LOKI_STRING("Primary address");
  loki_string const DEFAULT_LABEL = LOKI_STRING("(Untitled address)");
  loki_string const *LABEL        = primary_address ? &PRIMARY_LABEL : &DEFAULT_LABEL;

  char const *label = str_find(src, LABEL->str);
  if (!label)
      return false;

  if (addr)
  {
    char const *line_start = src;
    for (char const *ptr = label - 1; ptr >= src; ptr--)
    {
      if (ptr[0] == '\n')
        line_start = ptr + 1;
    }

      char const *address_value = str_skip_to_next_word(line_start);
      int address_len           = str_skip_to_next_whitespace(address_value) - address_value;
      addr->buf.append("%.*s", address_len, address_value);
  }
  return true;
}

bool wallet_address_index(wallet_t *wallet, int index, loki_addr *addr)
{
  LOCAL_PERSIST itest_read_possible_value const possible_values[] =
  {
    {LOKI_STRING("address"), false},
    {LOKI_STRING("Error: "), true},
  };

  loki_fixed_string<64> cmd("address %d", index);
  itest_ipc_result ipc_result = itest_write_then_read_stdout_until(&wallet->ipc, cmd.str, possible_values, LOKI_ARRAY_COUNT(possible_values));
  bool result = ipc_result && wallet__parse_address_line(ipc_result.output.c_str(), index == 0, addr);
  return result;
}

bool wallet_address_new(wallet_t *wallet, loki_addr *addr)
{
  itest_ipc_result ipc_result = itest_write_then_read_stdout(&wallet->ipc, "address new");
  bool result = ipc_result && wallet__parse_address_line(ipc_result.output.c_str(), false /*primary_address*/, addr);
  return result;
}

void wallet_account_new(wallet_t *wallet)
{
#if 0
[wallet T6UF17 (no daemon)]: account new
Untagged accounts:
          Account               Balance      Unlocked balance                 Label
         0 T6UF17     5122339.388601590     5122195.348165670       Primary account
 *       1 TRr45t           0.000000000           0.000000000    (Untitled account)
----------------------------------------------------------------------------------
          Total     5122339.388601590     5122195.348165670
#endif
  // TODO(loki): Doesn't work if the account label uses Total
  itest_ipc_result ipc_result = itest_write_then_read_stdout_until(&wallet->ipc, "account new", LOKI_STRING("Total"));
}

bool wallet_account_switch(wallet_t *wallet, int index)
{
#if 0
[wallet TRr45t (no daemon)]: account switch 0
Currently selected account: [0] Primary account
Tag: (No tag assigned)
Balance: 5122339.388601590, unlocked balance: 5122195.348165670 (29 block(s) to unlock)
#endif

  LOCAL_PERSIST itest_read_possible_value const possible_values[] =
  {
    {LOKI_STRING("Balance: "), false},
    {LOKI_STRING("Error: "), true},
  };

  LOKI_ASSERT(index >= 0);
  loki_fixed_string<64> cmd("account switch %d", index);
  itest_ipc_result ipc_result = itest_write_then_read_stdout_until(&wallet->ipc, cmd.str, possible_values, LOKI_ARRAY_COUNT(possible_values));
  return ipc_result;
}

uint64_t wallet_balance(wallet_t *wallet, uint64_t *unlocked_balance)
{
  // Example
  // Balance: 0.000000000, unlocked balance: 0.000000000
  itest_ipc_result ipc_result = itest_write_then_read_stdout_until(&wallet->ipc, "balance", LOKI_STRING("Balance: "));
  char const *ptr           = ipc_result.output.c_str();

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
  itest_ipc_result ipc_result = itest_write_then_read_stdout_until(&wallet->ipc, "integrated_address", LOKI_STRING("Matching integrated address"));
  LOKI_ASSERT_MSG(str_find(ipc_result.output.c_str(), "Matching integrated address: "), "Failed to match in: %s", ipc_result.output.c_str());
  char const *start = str_find(ipc_result.output.c_str(), "Matching integrated address:");
  start             = str_find(start, ":");
  start             = str_skip_to_next_alphanum(start);

  addr->set_integrated_addr(start);
  return true;
}

bool wallet_payment_id(wallet_t *wallet, loki_payment_id64 *id)
{
  itest_ipc_result ipc_result = itest_write_then_read_stdout(&wallet->ipc, "payment_id");
  // Example
  // Random payment ID: <73e4d298a578a80187c0894971a53f7a997ff1ca63b709cc9d387df92344f96f>
  LOKI_ASSERT(str_match(ipc_result.output.c_str(), "Random payment ID: "));
  char const *start = str_find(ipc_result.output.c_str(), "<");
  start++;

  id->append("%.*s", LOKI_MIN(ipc_result.output.size(), sizeof(id->str)), start);
  return true;
}

wallet_locked_stakes wallet_print_locked_stakes(wallet_t *wallet)
{
  LOCAL_PERSIST itest_read_possible_value const possible_values[] =
  {
    {LOKI_STRING("No locked stakes known for this wallet on the network"), false},
    {LOKI_STRING("Unlock Height"), false},
  };

  itest_ipc_result ipc_result = itest_write_then_read_stdout_until(&wallet->ipc, "print_locked_stakes", possible_values, LOKI_ARRAY_COUNT(possible_values));
  wallet_locked_stakes result = {};

  char const *output_ptr = ipc_result.output.c_str();
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
  LOCAL_PERSIST itest_read_possible_value const possible_values[] =
  {
    {LOKI_STRING("Error: refresh failed"), true},
    {LOKI_STRING("Error: refresh failed unexpected error: proxy exception in refresh thread"), true},
    {LOKI_STRING("Balance"),               false},
  };

  itest_ipc_result result = itest_write_then_read_stdout_until(&wallet->ipc, "refresh", possible_values, LOKI_ARRAY_COUNT(possible_values));
  return result;
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
    LOCAL_PERSIST itest_read_possible_value const possible_values[] =
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

    itest_ipc_result ipc_result = itest_write_then_read_stdout_until(&wallet->ipc, cmd.str, possible_values, LOKI_ARRAY_COUNT(possible_values));
    if (!ipc_result)
      return false;
  }

  itest_ipc_result ipc_result = itest_write_then_read_stdout_until(&wallet->ipc, "y", LOKI_STRING("You can check its status by using the `show_transfers` command"));
  if (!str_find(ipc_result.output.c_str(), "Transaction successfully submitted, transaction <"))
    return false;

  if (tx_id)
  {
    *tx_id = {};
    char const *tx_id_start = str_find(ipc_result.output.c_str(), "<");
    tx_id_start++;
    tx_id->append("%.*s", tx_id->max(), tx_id_start);
  }
  return true;
}

uint64_t wallet_status(wallet_t *wallet)
{
  // Refreshed N/K, synced, daemon RPC vX.Y
  itest_ipc_result ipc_result = itest_write_then_read_stdout_until(&wallet->ipc, "status", LOKI_STRING("Refreshed"));
  char const *ptr        = ipc_result.output.c_str();
  char const *height_str = str_skip_to_next_digit(ptr);
  uint64_t result        = static_cast<uint64_t>(atoi(height_str));
  return result;
}

itest_ipc_result wallet_sweep_all(wallet_t *wallet, char const *dest, loki_transaction *tx)
{
  LOCAL_PERSIST itest_read_possible_value const possible_values[] =
  {
    {LOKI_STRING("Transaction 1/"), false},
    {LOKI_STRING("Error"), true},
  };

  // TODO(doyle): Failure states
  // Example:
  // Transaction 1/1: ...
  // Sweeping 30298.277954908 for a total fee of 2.237828840.  Is this okay?  (Y/Yes/N/No):
  loki_fixed_string<128> cmd("sweep_all %s", dest);
  itest_ipc_result ipc_result = itest_write_then_read_stdout_until(&wallet->ipc, cmd.str, possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (!ipc_result)
      return ipc_result;

  // Sending amount
  char const *tx_divisor   = str_find(ipc_result.output.c_str(), "/");
  char const *num_txs_str  = str_skip_to_next_digit(tx_divisor);
  char const *amount_label = str_find(num_txs_str, "Sweeping ");
  char const *amount_str   = str_skip_to_next_digit(amount_label);
  uint64_t atomic_amount   = str_parse_loki_amount(amount_str);

  LOCAL_PERSIST itest_read_possible_value const possible_values2[] =
  {
    {LOKI_STRING("was rejected by daemon"), true},
    {LOKI_STRING("You can check its status by using the `show_transfers` command"), false},
  };
  ipc_result = itest_write_then_read_stdout_until(&wallet->ipc, "y", possible_values2, LOKI_ARRAY_COUNT(possible_values2));

  if (ipc_result && tx)
  {
    // TODO(doyle): Incomplete
    tx->dest.set_normal_addr(dest);
    tx->atomic_amount = atomic_amount;
  }

  return ipc_result;
}

FILE_SCOPE itest_ipc_result wallet__submit_and_parse_transfer_output(itest_ipc *ipc, loki_string cmd, char const *dest, loki_transaction *tx)
{
  LOCAL_PERSIST itest_read_possible_value const possible_values[] =
  {
    {LOKI_STRING("Is this okay?"), false},
    {LOKI_STRING("Error"), true},
  };

  itest_ipc_result ipc_result = itest_write_then_read_stdout_until(ipc, cmd.str, possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (!ipc_result)
    return ipc_result;

  // NOTE: Payment ID deprecated
#if 0
  // NOTE: No payment ID requested if sending to subaddress
  bool requested_payment_id = str_find(ipc_result.output.c_str(), "No payment id is included with this transaction. Is this okay?");
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
  char const *amount_label = str_find(ipc_result.output.c_str(), "Sending ");
  char const *amount_str   = str_skip_to_next_digit(amount_label);
  LOKI_ASSERT(amount_label);
  uint64_t atomic_amount = str_parse_loki_amount(amount_str);

  if (tx)
  {
    tx->atomic_amount = atomic_amount;
    tx->dest.set_normal_addr(dest);

    // Extract fee
    {
      char const *fee_label = str_find(ipc_result.output.c_str(), "The transaction fee is");
      if (!fee_label) fee_label = str_find(ipc_result.output.c_str(), "a total fee of");

      char const *fee_str   = str_skip_to_next_digit(fee_label);
      LOKI_ASSERT_MSG(fee_label, "Could not find the fee label in: %s", ipc_result.output.c_str());
      tx->fee = str_parse_loki_amount(fee_str);
    }
  }

  LOCAL_PERSIST itest_read_possible_value const possible_values2[] =
  {
    {LOKI_STRING("You can check its status by using the `show_transfers` command"), false},
    {LOKI_STRING("Error"), true},
  };

  ipc_result = itest_write_then_read_stdout_until(ipc, "y", possible_values2, LOKI_ARRAY_COUNT(possible_values2));
  if (ipc_result && tx) // Extract TX ID
  {
    assert(str_find(ipc_result.output.c_str(), "Transaction successfully submitted, transaction <"));
    char const *id_start = str_find(ipc_result.output.c_str(), "<");
    tx->id.append("%.*s", tx->id.max()-1, ++id_start);
  }

  return ipc_result;
}

itest_ipc_result wallet_transfer(wallet_t *wallet, char const *dest, uint64_t amount, loki_transaction *tx)
{
  loki_fixed_string<256> cmd("transfer %s %zu", dest, amount);
  itest_ipc_result result = wallet__submit_and_parse_transfer_output(&wallet->ipc, cmd.to_string(), dest, tx);
  return result;
}

itest_ipc_result wallet_transfer(wallet_t *wallet, loki_addr const *dest, uint64_t amount, loki_transaction *tx)
{
  itest_ipc_result result = wallet_transfer(wallet, dest->buf.str, amount, tx);
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
  LOCAL_PERSIST itest_read_possible_value const possible_values[] =
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
  itest_ipc_result ipc_result = itest_write_then_read_stdout_until(&wallet->ipc, cmd.str, possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (!ipc_result)
    return ipc_result;

  if (unlock_height)
  {
    char const *expiring_str      = str_find(ipc_result.output.c_str(), "You will continue receiving rewards until the service node expires at the estimated height: ");
    char const *unlock_height_str = str_skip_to_next_digit(expiring_str);
    *unlock_height = static_cast<uint64_t>(atoi(unlock_height_str));
  }

  LOCAL_PERSIST itest_read_possible_value const possible_values2[] =
  {
    {LOKI_STRING("Error: Reason: "), true},
    {LOKI_STRING("You can check its status by using the `show_transfers` command"), false},
  };
  ipc_result = itest_write_then_read_stdout_until(&wallet->ipc, "y", possible_values2, LOKI_ARRAY_COUNT(possible_values2));
  return ipc_result;
}

bool wallet_register_service_node(wallet_t *wallet, char const *registration_cmd, loki_transaction *tx)
{
  // Staking X for X blocks a total fee of X. Is this okay?
  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STRING("Is this okay?"), false},
    {LOKI_STRING("Error: This service node is already registered"), true},
  };

  itest_ipc_result ipc_result = itest_write_then_read_stdout_until(&wallet->ipc, registration_cmd, possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (!ipc_result)
    return false;

  if (tx)
  {
    char const *amount_label = str_find(ipc_result.output.c_str(), "Staking ");
    char const *amount_str   = str_skip_to_next_digit(amount_label);
    assert(amount_label);
    uint64_t atomic_amount = str_parse_loki_amount(amount_str);

    tx->atomic_amount = atomic_amount;
    // Extract fee
    {
      char const *fee_label = str_find(ipc_result.output.c_str(), "The transaction fee is");
      if (!fee_label) fee_label = str_find(ipc_result.output.c_str(), "a total fee of");

      char const *fee_str   = str_skip_to_next_digit(fee_label);
      LOKI_ASSERT_MSG(fee_label, "Could not find the fee label in: %s", ipc_result.output.c_str());
      tx->fee = str_parse_loki_amount(fee_str);
    }
  }

  ipc_result = itest_write_then_read_stdout_until(&wallet->ipc, "y", LOKI_STRING("Transaction successfully submitted, transaction <"));
  if (tx) // Extract TX ID
  {
    char const *id_start = str_find(ipc_result.output.c_str(), "<");
    tx->id.append("%.*s", tx->id.max(), ++id_start);
  }

  return true;
}

itest_ipc_result wallet_lns_buy_mapping(wallet_t *wallet, loki_string const *owner, loki_string const *backup_owner, loki_string type, loki_string name, loki_string value, loki_transaction *tx)
{
  (void)type; // TODO: Type not support yet, we default to Session
  loki_fixed_string<1024> cmd("lns_buy_mapping ");
  if (owner) cmd.append("owner=%.*s ", owner->len, owner->str);
  if (backup_owner) cmd.append("backup_owner=%.*s ", backup_owner->len, backup_owner->str);
  cmd.append("%.*s %.*s ", name.len, name.str, value.len, value.str);
  loki_addr address       = {};
  itest_ipc_result result = wallet_address(wallet, 0, &address);
  if (result) result = wallet__submit_and_parse_transfer_output(&wallet->ipc, cmd.to_string(), address.buf.str, tx);
  return result;
}

itest_ipc_result wallet_lns_update_mapping(wallet_t *wallet, loki_string type, loki_string name, loki_string const *value, loki_string const *owner, loki_string const *backup_owner, loki_string const *signature, loki_transaction *tx)
{
  (void)type; // TODO: Type not support yet, we default to Session

  loki_fixed_string<1024> cmd("lns_update_mapping ");
  if (value) cmd.append("value=%.*s ", value->len, value->str);
  if (owner) cmd.append("owner=%.*s ", owner->len, owner->str);
  if (backup_owner) cmd.append("backup_owner=%.*s ", backup_owner->len, backup_owner->str);
  if (signature) cmd.append("signature=%.*s ", signature->len, signature->str);
  cmd.append("%.*s", name.len, name.str);

  loki_addr address       = {};
  itest_ipc_result result = wallet_address(wallet, 0, &address);
  if (result) result = wallet__submit_and_parse_transfer_output(&wallet->ipc, cmd.to_string(), address.buf.str, tx);
  return result;
}

wallet_lns_entries wallet_lns_print_owners_to_name(wallet_t *wallet, loki_string const *owner)
{
  loki_fixed_string<1024> cmd("lns_print_owners_to_names ");
  if (owner) cmd.append("%.*s ", owner->len, owner->str);

  wallet_lns_entries result = {};
  itest_ipc_result ipc_result = itest_write_then_read_stdout(&wallet->ipc, cmd.str);
  if (ipc_result)
  {
    loki_string owner_str        = LOKI_STRING("owner=");
    loki_string backup_owner_str = LOKI_STRING("backup_owner=");
    loki_string type_str         = LOKI_STRING("type=");
    loki_string height_str       = LOKI_STRING("height=");
    loki_string name_hash_str    = LOKI_STRING("name_hash=");
    loki_string encrypted_value_str = LOKI_STRING("encrypted_value=");
    loki_string prev_txid_str    = LOKI_STRING("prev_txid=");

    char const *ptr        = ipc_result.output.c_str();
    char const *output_end = ipc_result.output.c_str() + ipc_result.output.size();
    for (;;)
    {
      wallet_lns_entry entry = {};

      ptr = str_find(ptr, owner_str.str);
      if (!ptr) break;

      ptr += owner_str.len;
      char const *end = str_find(ptr, ",");
      isize len       = (ptrdiff_t)(end - ptr);
      entry.owner.append("%.*s", len, ptr);

      ptr = str_find(ptr, backup_owner_str.str);
      ptr += backup_owner_str.len;
      end = str_find(ptr, ",");
      len = (ptrdiff_t)(end - ptr);
      assert(len <= entry.backup_owner.max() - 1);
      entry.backup_owner.append("%.*s", len, ptr);

      ptr = str_find(ptr, type_str.str);
      ptr += type_str.len;
      end = str_find(ptr, ",");
      len = (ptrdiff_t)(end - ptr);
      assert(len <= entry.type.max() - 1);
      entry.type.append("%.*s", len, ptr);

      ptr = str_find(ptr, height_str.str);
      ptr += height_str.len;
      entry.height = atoi(ptr);

      ptr = str_find(ptr, name_hash_str.str);
      ptr += name_hash_str.len;
      end = str_find(ptr, ",");
      len = (ptrdiff_t)(end - ptr);
      assert(len <= entry.name_hash.max() - 1);
      entry.name_hash.append("%.*s", len, ptr);

      ptr = str_find(ptr, encrypted_value_str.str);
      ptr += encrypted_value_str.len;
      end = str_find(ptr, ",");
      len = (ptrdiff_t)(end - ptr);
      // assert(len == entry.encrypted_value.max() - 1);
      entry.encrypted_value.append("%.*s", len, ptr);

      ptr = str_find(ptr, prev_txid_str.str);
      ptr += prev_txid_str.len;

      isize remaining = (isize)(output_end - ptr);
      if (remaining >= (entry.prev_txid.max() - 1))
        entry.prev_txid.append("%.*s", (entry.prev_txid.max() - 1), ptr);

      result.array[result.array_len++] = entry;
    }
  }
  return result;
}

wallet_lns_entries wallet_lns_print_name_to_owner(wallet_t *wallet, loki_string const *type, loki_string name)
{
  loki_fixed_string<1024> cmd("lns_print_name_to_owners ");
  if (type) cmd.append("type=%.*s ", type->len, type->str);
  cmd.append("%.*s ", name.len, name.str);

  wallet_lns_entries result = {};
  itest_ipc_result ipc_result = itest_write_then_read_stdout(&wallet->ipc, cmd.str);
  if (ipc_result)
  {
    loki_string name_hash_str       = LOKI_STRING("name_hash=");
    loki_string type_str            = LOKI_STRING("type=");
    loki_string owner_str           = LOKI_STRING("owner=");
    loki_string backup_owner_str    = LOKI_STRING("backup_owner=");
    loki_string height_str          = LOKI_STRING("height=");
    loki_string encrypted_value_str = LOKI_STRING("encrypted_value=");
    loki_string value_str           = LOKI_STRING("value=");
    loki_string prev_txid_str       = LOKI_STRING("prev_txid=");

    char const *ptr        = ipc_result.output.c_str();
    char const *output_end = ipc_result.output.c_str() + ipc_result.output.size();
    for (;;)
    {
      wallet_lns_entry entry = {};

      ptr = str_find(ptr, name_hash_str.str);
      if (!ptr) break;

      ptr += name_hash_str.len;
      char const *end = str_find(ptr, ",");
      isize len = (ptrdiff_t)(end - ptr);
      assert(len <= entry.name_hash.max() - 1);
      entry.name_hash.append("%.*s", len, ptr);

      ptr = str_find(ptr, type_str.str);
      ptr += type_str.len;
      end = str_find(ptr, ",");
      len = (ptrdiff_t)(end - ptr);
      assert(len <= entry.type.max() - 1);
      entry.type.append("%.*s", len, ptr);

      ptr = str_find(ptr, owner_str.str);
      ptr += owner_str.len;
      end = str_find(ptr, ",");
      len = (ptrdiff_t)(end - ptr);
      assert(len <= entry.owner.max() - 1);
      entry.owner.append("%.*s", len, ptr);

      ptr = str_find(ptr, backup_owner_str.str);
      ptr += backup_owner_str.len;
      end = str_find(ptr, ",");
      len = (ptrdiff_t)(end - ptr);
      entry.backup_owner.append("%.*s", len, ptr);

      ptr = str_find(ptr, height_str.str);
      ptr += height_str.len;
      entry.height = atoi(ptr);

      ptr = str_find(ptr, encrypted_value_str.str);
      ptr += encrypted_value_str.len;
      end = str_find(ptr, ",");
      len = (ptrdiff_t)(end - ptr);
      entry.encrypted_value.append("%.*s", len, ptr);

      ptr = str_find(ptr, value_str.str);
      ptr += value_str.len;
      end = str_find(ptr, ",");
      len = (ptrdiff_t)(end - ptr);
      entry.value.append("%.*s", len, ptr);

      ptr = str_find(ptr, prev_txid_str.str);
      ptr += prev_txid_str.len;
      isize remaining = (isize)(output_end - ptr);
      if (remaining >= (entry.prev_txid.max() - 1))
        entry.prev_txid.append("%.*s", (entry.prev_txid.max() - 1), ptr);

      result.array[result.array_len++] = entry;
    }
  }

  return result;
}

loki_hash64 wallet_spendkey(wallet_t *wallet, loki_hash64 *public_key)
{
  loki_hash64 result = {};
  if (0) // TODO(loki): Not implemented yet/doesn't work
  {
    loki_string secret_label    = LOKI_STRING("secret: ");
    itest_ipc_result ipc_result = itest_write_then_read_stdout_until(&wallet->ipc, "spendkey", secret_label);
    if (!ipc_result) return {};
    char const *ptr = ipc_result.output.data();
    char const *secret = str_find(ptr, secret_label.str) + secret_label.len;
    result.append("%.*s", result.max() - 1, secret);
  }

  if (public_key)
  {
    loki_string public_label    = LOKI_STRING("public: ");
    itest_ipc_result ipc_result = itest_write_then_read_stdout_until(&wallet->ipc, "spendkey", public_label);

    char const *ptr        = ipc_result.output.data();
    char const *public_str = str_find(ptr, public_label.str) + public_label.len;
    public_key->append("%.*s", public_key->max() - 1, public_str);
  }
  return result;
}

bool wallet_lns_make_update_mapping_signature(wallet_t *wallet, loki_string name, loki_string const *value, loki_string const *owner, loki_string const *backup_owner, loki_hash128 *signature)
{
  LOCAL_PERSIST itest_read_possible_value const possible_values[] =
  {
    {LOKI_STRING("Error:"), true},
    {LOKI_STRING("signature="), false},
  };

  loki_fixed_string<1024> cmd("lns_make_update_mapping_signature ");
  if (value) cmd.append("value=%.*s ", value->len, value->str);
  if (owner) cmd.append("owner=%.*s ", owner->len, owner->str);
  if (backup_owner) cmd.append("backup_owner=%.*s ", backup_owner->len, backup_owner->str);
  cmd.append("%.*s", name.len, name.str);

  itest_ipc_result result = itest_write_then_read_stdout_until(&wallet->ipc, cmd.str, possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (result && signature)
  {
    char const *ptr = result.output.data();
    loki_string label = LOKI_STRING("signature=");
    char const *signature_str = str_find(ptr, label.str) + label.len;
    signature->append("%.*s", signature->max() - 1, signature_str);
  }
  return result;
}
