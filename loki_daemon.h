#ifndef LOKI_DAEMON_H
#define LOKI_DAEMON_H

// Define LOKI_DAEMON_IMPLEMENTATION in one CPP file

//
// Header
//

struct daemon_prepare_registration_params
{
  static daemon_prepare_registration_params solo_staker(wallet_t wallet);
  bool             open_pool;
  int              owner_fee_percent;
  int              num_contributors;
  loki_contributor contributors[4];
};

struct daemon_status_t
{
  uint64_t         height;
  uint64_t         hf_version;
};

// TODO(doyle): We should fill out this struct when print_sn_status is called. But, the way it's implemented there's not a nice way to read the output
struct daemon_snode_status
{
  bool known_on_the_network;
  bool registered;
  bool decommissioned;
  bool last_uptime_proof_received;
};

struct daemon_checkpoint
{
  bool        service_node_checkpoint;
  uint64_t    height;
  loki_hash64 block_hash;

  bool operator==(daemon_checkpoint const &other) const
  {
    bool result = (service_node_checkpoint == other.service_node_checkpoint && height == other.height &&
                   block_hash == other.block_hash);
    return result;
  }
};

void                           daemon_exit                 (daemon_t *daemon);
bool                           daemon_prepare_registration (daemon_t *daemon, daemon_prepare_registration_params const *params, loki_scratch_buf *registration_cmd);
std::vector<daemon_checkpoint> daemon_print_checkpoints    (daemon_t *daemon);
uint64_t                       daemon_print_height         (daemon_t *daemon);
daemon_snode_status            daemon_print_sn             (daemon_t *daemon, loki_snode_key const *key); // TODO(doyle): We can't request the entire sn list because this needs a big buffer and I cbb doing mem management over shared mem
bool                           daemon_print_sn_key         (daemon_t *daemon, loki_snode_key *key);
daemon_snode_status            daemon_print_sn_status      (daemon_t *daemon); // return: If the node is known on the network (i.e. registered)
uint64_t                       daemon_print_sr             (daemon_t *daemon, uint64_t height);
bool                           daemon_print_tx             (daemon_t *daemon, char const *tx_id, std::string *output);
void                           daemon_print_cn             (daemon_t *daemon);
bool                           daemon_relay_tx             (daemon_t *daemon, char const *tx_id);
bool                           daemon_ban                  (daemon_t *daemon, loki_buffer<32> const *ip);
bool                           daemon_unban                (daemon_t *daemon, loki_buffer<32> const *ip);
bool                           daemon_set_log              (daemon_t *daemon, int level);
daemon_status_t                daemon_status               (daemon_t *daemon);
bool                           daemon_print_block          (daemon_t *daemon, uint64_t height, loki_hash64 *block_hash);

// NOTE: This command is only available in integration mode, compiled out otherwise in the daemon
void                daemon_relay_votes_and_uptime(daemon_t *daemon);

// NOTE: Debug integration_test <sub cmd>, style of commands enabled in integration mode
bool                daemon_mine_n_blocks           (daemon_t *daemon, wallet_t *wallet, int num_blocks);
void                daemon_mine_n_blocks           (daemon_t *daemon, loki_addr const *addr, int num_blocks);
void                daemon_mine_until_height       (daemon_t *daemon, loki_addr const *addr, uint64_t desired_height);
bool                daemon_mine_until_height       (daemon_t *daemon, wallet_t *wallet, uint64_t desired_height);
void                daemon_toggle_checkpoint_quorum(daemon_t *daemon);
void                daemon_toggle_obligation_quorum(daemon_t *daemon);
void                daemon_toggle_obligation_uptime_proof(daemon_t *daemon);
void                daemon_toggle_obligation_checkpointing(daemon_t *daemon);

#endif // LOKI_DAEMON_H

//
// CPP Implementation
//
#ifdef LOKI_DAEMON_IMPLEMENTATION

static uint64_t amount_to_staking_portions(uint64_t amount)
{
  // TODO(doyle): Assumes staking requirement of 100
  uint64_t const MAX_STAKING_PORTIONS = 0xfffffffffffffffc;
  uint64_t result = (MAX_STAKING_PORTIONS / 100) * amount;
  return result;
}

void daemon_exit(daemon_t *daemon)
{
  itest_write_to_stdin(&daemon->ipc, "exit");
  itest_ipc_clean_up(&daemon->ipc);
}

std::vector<daemon_checkpoint> daemon_print_checkpoints(daemon_t *daemon)
{
  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STR_LIT("No Checkpoints"), true},
    {LOKI_STR_LIT("Type"), false},
  };

  std::vector<daemon_checkpoint> result;
  itest_read_result output = itest_write_then_read_stdout_until(&daemon->ipc, "print_checkpoints", possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (possible_values[output.matching_find_strs_index].is_fail_msg)
    return result;

  char const *ptr = output.buf.c_str();
  for (ptr = str_find(ptr, "Type: "); ptr; ptr = str_find(ptr, "Type: "))
  {
    char const *type_value   = str_skip_to_next_word_inplace(&ptr);
    ptr                      = str_find(ptr, "Height: ");
    char const *height_value = str_skip_to_next_word_inplace(&ptr);
    ptr                      = str_find(ptr, "Hash: ");
    char const *hash_value   = str_skip_to_next_word_inplace(&ptr);

    daemon_checkpoint checkpoint       = {};
    checkpoint.service_node_checkpoint = (strcmp(type_value, "ServiceNode") == 0);
    checkpoint.height                  = atoi(height_value);
    checkpoint.block_hash              = hash_value;

    result.push_back(checkpoint);
  }

  return result;
}

uint64_t daemon_print_height(daemon_t *daemon)
{
  itest_read_result output = itest_write_then_read_stdout(&daemon->ipc, "print_height");
  uint64_t result = static_cast<uint64_t>(atoi(output.buf.c_str()));
  return result;
}

bool daemon_prepare_registration(daemon_t *daemon, daemon_prepare_registration_params const *params, loki_scratch_buf *registration_cmd)
{
  bool result = true;
  assert(params->num_contributors >= 1 && params->num_contributors <= 4);

  if (params->open_pool)
    assert(params->num_contributors != 4);

  loki_contributor const *owner = params->contributors + 0;
  loki_buffer<32> owner_portions("%zu", amount_to_staking_portions(owner->amount));
  loki_buffer<32> owner_fee     ("%d",  params->owner_fee_percent);
  loki_buffer<32> owner_amount  ("%zu", owner->amount);

  // TODO(doyle): Handle fee properly

  // Expected Format: register_service_node <owner cut> <address> <fraction> [<address> <fraction> [...]]]
  itest_read_result output                     = {};
  char const *register_snode_start_ptr = nullptr;
  itest_write_to_stdin(&daemon->ipc, "prepare_registration");
  if (params->num_contributors == 1)
  {
    if (params->open_pool)
    {
      itest_read_until_then_write_stdin(&daemon->ipc, LOKI_STR_LIT("Will the operator contribute the entire stake?"), "n");
      itest_read_until_then_write_stdin(&daemon->ipc, LOKI_STR_LIT("Enter operator fee as a percentage of the total staking reward"), owner_fee.c_str);
      itest_read_until_then_write_stdin(&daemon->ipc, LOKI_STR_LIT("Do you want to reserve portions of the stake for other specific contributors?"), "n");
      itest_read_until_then_write_stdin(&daemon->ipc, LOKI_STR_LIT("Enter the loki address for the operator"), owner->addr.buf.c_str);
      itest_read_until_then_write_stdin(&daemon->ipc, LOKI_STR_LIT("How much loki does the operator want to reserve in the stake?"), owner_amount.c_str);
      itest_read_until_then_write_stdin(&daemon->ipc, LOKI_STR_LIT("You will leave the remaining portion of"), "y");
      itest_read_until_then_write_stdin(&daemon->ipc, LOKI_STR_LIT("Do you confirm the information above is correct?"), "y");
      output = itest_read_stdout(&daemon->ipc);

      char const *register_str = str_find(output.buf.c_str(), "register_service_node");
      char const *ptr          = register_str;
      result                  &= (register_str != nullptr);

      char const *owner_fee_output      = str_skip_to_next_word_inplace(&ptr);
      char const *owner_addr_output     = str_skip_to_next_word_inplace(&ptr);
      // char const *owner_portions_output = str_skip_to_next_word(&ptr);

      result &= str_match(owner_fee_output,      owner_fee.c_str);
      result &= str_match(owner_addr_output,     owner->addr.buf.c_str);

      // TODO(doyle): Validate amounts, because we don't use the same portions
      // calculation as loki daemon we can be off by small amounts.
      // result &= str_match(owner_portions_output, owner_portions.c_str);

      register_snode_start_ptr = register_str;
    }
    else
    {
      itest_read_until_then_write_stdin(&daemon->ipc, LOKI_STR_LIT("Will the operator contribute the entire stake?"), "y");
      itest_read_until_then_write_stdin(&daemon->ipc, LOKI_STR_LIT("Enter the loki address for the solo staker"), owner->addr.buf.c_str);
      itest_read_until_then_write_stdin(&daemon->ipc, LOKI_STR_LIT("Do you confirm the information above is correct?"), "y");
      output = itest_read_stdout(&daemon->ipc);

      char const *register_str = str_find(output.buf.c_str(), "register_service_node");
      char const *prev         = register_str;
      result                  &= (register_str != nullptr);

      char const *owner_fee_output      = str_skip_to_next_word_inplace(&prev);
      char const *owner_addr_output     = str_skip_to_next_word_inplace(&prev);
      char const *owner_portions_output = str_skip_to_next_word_inplace(&prev);

      result &= str_match(register_str,          "register_service_node");
      result &= str_match(owner_fee_output,      "18446744073709551612");
      result &= str_match(owner_addr_output,     owner->addr.buf.c_str);
      result &= str_match(owner_portions_output, "18446744073709551612");

      register_snode_start_ptr = register_str;
    }
  }
  else
  {
    itest_read_until_then_write_stdin(&daemon->ipc, LOKI_STR_LIT("Will the operator contribute the entire stake?"), "n");
    itest_read_until_then_write_stdin(&daemon->ipc, LOKI_STR_LIT("Enter operator fee as a percentage of the total staking reward"), owner_fee.c_str);
    itest_read_until_then_write_stdin(&daemon->ipc, LOKI_STR_LIT("Do you want to reserve portions of the stake for other specific contributors?"), "y");

    int num_extra_contribs             = params->num_contributors - 1;
    char const *num_extra_contribs_str = (num_extra_contribs == 1) ? "1" : (num_extra_contribs == 2) ? "2" : "3";
    itest_read_until_then_write_stdin(&daemon->ipc, LOKI_STR_LIT("Number of additional contributors"), num_extra_contribs_str);

    itest_read_until_then_write_stdin(&daemon->ipc, LOKI_STR_LIT("Enter the loki address for the operator"), owner->addr.buf.c_str);
    itest_read_until_then_write_stdin(&daemon->ipc, LOKI_STR_LIT("How much loki does the operator want to reserve in the stake?"), owner_amount.c_str);

    for (int i = 1; i < params->num_contributors; ++i)
    {
      loki_contributor const *contributor = params->contributors + i;
      loki_buffer<32> contributor_amount("%zu", contributor->amount);
      output = itest_write_then_read_stdout(&daemon->ipc, contributor->addr.buf.c_str);
      itest_write_then_read_stdout(&daemon->ipc, contributor_amount.c_str);
    }

    if (params->open_pool)
      output = itest_write_then_read_stdout(&daemon->ipc, "y"); // You will leave remaining portion for open to contribution etc.

    output                   = itest_write_then_read_stdout_until(&daemon->ipc, "y", LOKI_STR_LIT("Run this command in the wallet")); // Confirm
    char const *register_str = str_find(output.buf.c_str(), "register_service_node");
    char const *ptr          = register_str;
    result                   &= (register_str != nullptr);

    char const *owner_fee_output  = str_skip_to_next_word_inplace(&ptr);
    // TODO(doyle): Hack handle owner fees better
    if (params->owner_fee_percent == 100)
    {
      result &= str_match(owner_fee_output, "18446744073709551612");
    }
    else
    {
      result &= str_match(owner_fee_output,  owner_fee.c_str);
    }

    for (int i = 0; i < params->num_contributors; ++i)
    {
      loki_contributor const *contributor = params->contributors + i;
      char const *addr_output             = str_skip_to_next_word_inplace(&ptr);
      result                             &= str_match(addr_output,contributor->addr.buf.c_str);
      char const *portions_output         = str_skip_to_next_word_inplace(&ptr);
      loki_buffer<32> contributor_portions("%zu", amount_to_staking_portions(contributor->amount));
      (void)portions_output; (void)contributor_portions;

      // TODO(doyle): Validate amounts, because we don't use the same portions
      // calculation as loki daemon we can be off by small amounts.
      // result &= str_match(portions_output, contributor_portions.c_str);
    }

    register_snode_start_ptr = register_str;
  }

  if (result)
  {
    char const *start = register_snode_start_ptr;
    char const *end   = start;
    while (end[0] && end[0] != '\n')
      ++end;

    int len = static_cast<int>(end - start);
    registration_cmd->append("%.*s", len, start);
  }

  return result;
}

daemon_snode_status daemon_print_sn(daemon_t *daemon, loki_snode_key const *key)
{
  daemon_snode_status result = {};
  loki_buffer<256> cmd("print_sn %s", key->c_str);

  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STR_LIT("No service node is currently known on the network"), true},
    {LOKI_STR_LIT("Service Node Registration State"), false},
  };

  itest_read_result output = itest_write_then_read_stdout_until(&daemon->ipc, cmd.c_str, possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (possible_values[output.matching_find_strs_index].is_fail_msg)
    return result;

  char const *ptr                       = output.buf.c_str();
  char const *registration_label        = str_find(ptr, "Service Node Registration State");
  char const *num_registered_snodes_str = str_skip_to_next_digit(registration_label);
  int num_registered_snodes             = atoi(num_registered_snodes_str);
  char const *decommissioned_str        = str_find(ptr, "Current Status: DECOMMISSIONED");

  result.known_on_the_network       = true;
  result.last_uptime_proof_received = (str_find(ptr, "Last Uptime Proof Received: (Awaiting confirmation from network)") == nullptr);
  result.registered                 = (num_registered_snodes == 1);
  result.decommissioned             = (decommissioned_str != nullptr);
  return result;
}

bool daemon_print_sn_key(daemon_t *daemon, loki_snode_key *key)
{
  itest_read_result output = itest_write_then_read_stdout_until(&daemon->ipc, "print_sn_key", LOKI_STR_LIT("Service Node Public Key: "));
  char const *key_ptr = str_find(output.buf.c_str(), ":");
  key_ptr = str_skip_to_next_alphanum(key_ptr);

  if (key)
  {
    key->clear();
    key->append("%.*s", key->max(), key_ptr);
  }

  return true;
}

daemon_snode_status daemon_print_sn_status(daemon_t *daemon)
{

  daemon_snode_status result = {};
  itest_read_result output    = itest_write_then_read_stdout(&daemon->ipc, "print_sn_status");

  if (str_find(output.buf.c_str(), "No service node is currently known on the network"))
    return result;

  result.known_on_the_network    = true;
  char const *registration_label = str_find(output.buf.c_str(), "Service Node Registration State");
  if (!registration_label)
    return result;

  char const *num_registered_snodes_str = str_skip_to_next_digit(registration_label);
  int num_registered_snodes             = atoi(num_registered_snodes_str);

  result.registered           = (num_registered_snodes == 1);
  return result;

  // TODO(doyle): We should completely parse the output result, it looks like the below
#if 0
  if (status) *status = {};
  Example
    Service Node Unregistered State[0]

    Service Node Registration State[1]
      [0] Service Node: 500385da3592017384965040ee6ec82409cdc0cae8f6e80daf797513fca93265
          Total Contributed/Staking Requirement: 100.000000000/100.000000000
          Total Reserved: 100.000000000
          Registration Height/Expiry Height: 98/148 (in 31 blocks)
          Expiry Date (Estimated UTC): 2018-11-20 07:40:10 AM (62 minutes in the future)
          Last Reward At (Block Height/TX Index): 116 / 4294967295
          Operator Cut (% Of Reward): 100%
          Operator Address: L6vruB65jVwPf8XnwfKiV6K8BWwSNUhTEQi4rd6Xzanc7AESBDsFPP8fVb2X5onqvxJVFXtV25YcHGV3aPKTJGKDF7qbdsE
          Last Uptime Proof Received: Not Received Yet

          [0] Contributor: L6vruB65jVwPf8XnwfKiV6K8BWwSNUhTEQi4rd6Xzanc7AESBDsFPP8fVb2X5onqvxJVFXtV25YcHGV3aPKTJGKDF7qbdsE
              Amount / Reserved: 100.000000000 / 100.000000000

  char const *registration_height_label = str_find(output.c_str, "Registration Height/Expiry Height");
  char const *registration_height       = str_skip_to_next_digit(registration_height_label);
  status->registration_height           = atoi(registration_height);

  char const *expiry_height = str_find(registration_height, "/");
  expiry_height++;
  status->expiry_height     = atoi(expiry_height);

  char const *blocks_till_expiry = expiry_height;
  str_skip_to_next_whitespace_inplace(&blocks_till_expiry);
  str_skip_to_next_digit_inplace(&blocks_till_expiry);
  status->blocks_till_expiry     = atoi(blocks_till_expiry);
#endif
}

uint64_t daemon_print_sr(daemon_t *daemon, uint64_t height)
{
  loki_buffer<32> cmd("print_sr %zu", height);
  itest_read_result output = itest_write_then_read_stdout_until(&daemon->ipc, cmd.c_str, LOKI_STR_LIT("Staking Requirement: "));
  char const *staking_requirement_str = str_find(output.buf.c_str(), ":");
  ++staking_requirement_str;
  uint64_t result = str_parse_loki_amount(staking_requirement_str);
  return result;
}

bool daemon_print_tx(daemon_t *daemon, char const *tx_id, std::string *output)
{
  loki_buffer<256> cmd("print_tx %s +json", tx_id);
  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STR_LIT("Error: Transaction wasn't found"), true},
    {LOKI_STR_LIT("output_unlock_times"), false},
  };

  itest_read_result read_result = itest_write_then_read_stdout_until(&daemon->ipc, cmd.c_str, possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (possible_values[read_result.matching_find_strs_index].is_fail_msg)
    return false;

  if (output) *output = std::move(read_result.buf);
  return true;
}

void daemon_print_cn(daemon_t *daemon)
{
  itest_write_to_stdin(&daemon->ipc, "print_cn");
  itest_read_stdout_sink(&daemon->ipc, 1000/*ms*/);
}

bool daemon_relay_tx(daemon_t *daemon, char const *tx_id)
{
  loki_buffer<256> cmd("relay_tx %s", tx_id);
  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STR_LIT("Unsuccessful -- transaction not found in pool"), true},
    {LOKI_STR_LIT("Transaction successfully relayed"), false},
  };

  itest_read_result read_result = itest_write_then_read_stdout_until(&daemon->ipc, cmd.c_str, possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (possible_values[read_result.matching_find_strs_index].is_fail_msg)
    return false;

  return true;
}

bool daemon_ban(daemon_t *daemon, loki_buffer<32> const *ip)
{
  loki_buffer<64> cmd("ban %s", ip->c_str);

  // TODO(doyle): This relies on not muting the log levels in the integration binaries by setting the log categories to ""
  // which we would parse to determine if the ban was successful for not
#if 1
  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STR_LIT("Error: Invalid IP"), true},
    {LOKI_STR_LIT("blocked"), false},
  };
  itest_read_result read_result = itest_write_then_read_stdout_until(&daemon->ipc, cmd.c_str, possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (possible_values[read_result.matching_find_strs_index].is_fail_msg)
    return false;
#else
  itest_write_to_stdin(&daemon->ipc, cmd.c_str);
  os_sleep_ms(100);
#endif

  return true;
}

bool daemon_unban(daemon_t *daemon, loki_buffer<32> const *ip)
{
  loki_buffer<64> cmd("unban %s", ip->c_str);
#if 1
  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STR_LIT("Error: Invalid IP"), true},
    {LOKI_STR_LIT("unblocked"), false},
  };

  itest_read_result read_result = itest_write_then_read_stdout_until(&daemon->ipc, cmd.c_str, possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (possible_values[read_result.matching_find_strs_index].is_fail_msg)
    return false;
#else
  itest_write_to_stdin(&daemon->ipc, cmd.c_str);
  os_sleep_ms(100);
#endif

  return true;
}

bool daemon_set_log(daemon_t *daemon, int level)
{
  loki_buffer<64> cmd("set_log %d", level);

  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STR_LIT("Log level is now"), false},
  };

  itest_read_result read_result = itest_write_then_read_stdout_until(&daemon->ipc, cmd.c_str, possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (possible_values[read_result.matching_find_strs_index].is_fail_msg)
    return false;

  return true;
}

void daemon_relay_votes_and_uptime(daemon_t *daemon)
{
  itest_write_then_read_stdout_until(&daemon->ipc, "relay_votes_and_uptime", LOKI_STR_LIT("Votes and uptime relayed"));
}

daemon_status_t daemon_status(daemon_t *daemon)
{
  // Example:
  // Height: 67/67 (100.0%) on testnet, not mining, net hash 4 H/s, v9, up to date, 0(out)+0(in) connections, uptime 0d 0h 0m 0s
  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STR_LIT("Error: Problem fetching info -- "), true},
    {LOKI_STR_LIT("Height: "), false},
  };

  itest_read_result output = itest_write_then_read_stdout_until(&daemon->ipc, "status", possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (possible_values[output.matching_find_strs_index].is_fail_msg)
    return {};

  daemon_status_t result = {};
  char const *ptr        = output.buf.c_str();
  char const *height_str = str_skip_to_next_digit_inplace(&ptr);
  result.height          = atoi(height_str);

  loki_str_lit literal = LOKI_STR_LIT("net v");
  ptr                  = (str_find(ptr, "net v")); // net v<hardfork version>
  ptr += literal.len;
  assert(ptr && char_is_num(ptr[0]));
  result.hf_version = atoi(ptr);
  return result;
}

bool daemon_print_block(daemon_t *daemon, uint64_t height, loki_hash64 *block_hash)
{
  loki_buffer<64> cmd("print_block %zu", height);
  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STR_LIT("Error: Unsuccessful --"), true},
    {LOKI_STR_LIT("timestamp: "), false},
  };

  itest_read_result read_result = itest_write_then_read_stdout_until(&daemon->ipc, cmd.c_str, possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (possible_values[read_result.matching_find_strs_index].is_fail_msg)
    return false;

  char const *ptr        = read_result.buf.c_str();
  char const *hash_label = str_find(ptr, "hash: ");
  char const *hash       = str_skip_to_next_word(hash_label);
  *block_hash            = hash;
  return true;
}

bool daemon_mine_n_blocks(daemon_t *daemon, wallet_t *wallet, int num_blocks)
{
  loki_addr addr = {};
  bool result    = wallet_address(wallet, 0, &addr);
  if (result)
  {
    daemon_mine_n_blocks(daemon, &addr, num_blocks);
    wallet_refresh(wallet);
  }
  return result;
}

void daemon_mine_n_blocks(daemon_t *daemon, loki_addr const *addr, int num_blocks)
{
  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STR_LIT("Mining stopped in daemon"), false},
    {LOKI_STR_LIT("integration_test invalid command"), true},
  };

  // TODO(doyle): This is a hacky workaround, seems to be some race condition in
  // that sometimes the mining log strings from the daemon gets read from shared
  // memory and put into the command, like so and produces invalid command result

  // integration_test debug_mine_n_blocks Height 209, txid <497d5d8fd990625a70f39c3dd25d0f4e3365e182c29c673476bc1dc85afcd78a>, 57.499509760 1
  // integration_test invalid command

  for (;;)
  {
      loki_buffer<256> cmd("integration_test debug_mine_n_blocks %s %d", addr->buf.c_str, num_blocks);
      itest_read_result output = itest_write_then_read_stdout_until(&daemon->ipc, cmd.c_str, possible_values, LOKI_ARRAY_COUNT(possible_values));
      if (!possible_values[output.matching_find_strs_index].is_fail_msg)
      {
          break;
      }
  }

}

void daemon_mine_until_height(daemon_t *daemon, loki_addr const *addr, uint64_t desired_height)
{
  daemon_status_t status = daemon_status(daemon);
  if (desired_height < status.height)
    return;

  uint64_t blocks_to_mine = desired_height - status.height;
  daemon_mine_n_blocks(daemon, addr, blocks_to_mine);
}

bool daemon_mine_until_height(daemon_t *daemon, wallet_t *wallet, uint64_t desired_height)
{
  loki_addr addr = {};
  bool result    = wallet_address(wallet, 0, &addr);
  if (result)
  {
    daemon_mine_until_height(daemon, &addr, desired_height);
    wallet_refresh(wallet);
  }
  return result;
}

void daemon_toggle_checkpoint_quorum(daemon_t *daemon)
{
  loki_buffer<256> cmd("integration_test toggle_checkpoint_quorum");
  itest_write_then_read_stdout_until(&daemon->ipc, cmd.c_str, LOKI_STR_LIT("toggle_checkpoint_quorum toggled"));
}

void daemon_toggle_obligation_quorum(daemon_t *daemon)
{
  loki_buffer<256> cmd("integration_test toggle_obligation_quorum");
  itest_write_then_read_stdout_until(&daemon->ipc, cmd.c_str, LOKI_STR_LIT("toggle_obligation_quorum toggled"));
}

void daemon_toggle_obligation_uptime_proof(daemon_t *daemon)
{
  loki_buffer<256> cmd("integration_test toggle_obligation_uptime_proof");
  itest_write_then_read_stdout_until(&daemon->ipc, cmd.c_str, LOKI_STR_LIT("toggle_obligation_uptime_proof toggled"));
}

void daemon_toggle_obligation_checkpointing(daemon_t *daemon)
{
  loki_buffer<256> cmd("integration_test toggle_obligation_checkpointing");
  itest_write_then_read_stdout_until(&daemon->ipc, cmd.c_str, LOKI_STR_LIT("toggle_obligation_checkpointing toggled"));
}

#endif // LOKI_DAEMON_IMPLEMENTATION
