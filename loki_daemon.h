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
};

void                daemon_exit                  (daemon_t *daemon);
bool                daemon_prepare_registration  (daemon_t *daemon, daemon_prepare_registration_params const *params, loki_scratch_buf *registration_cmd);
void                daemon_print_checkpoints     (daemon_t *daemon);
uint64_t            daemon_print_height          (daemon_t *daemon);
daemon_snode_status daemon_print_sn              (daemon_t *daemon, loki_snode_key const *key); // TODO(doyle): We can't request the entire sn list because this needs a big buffer and I cbb doing mem management over shared mem
bool                daemon_print_sn_key          (daemon_t *daemon, loki_snode_key *key);
daemon_snode_status daemon_print_sn_status       (daemon_t *daemon); // return: If the node is known on the network (i.e. registered)
uint64_t            daemon_print_sr              (daemon_t *daemon, uint64_t height);
bool                daemon_print_tx              (daemon_t *daemon, char const *tx_id, loki_scratch_buf *output);
void                daemon_print_cn              (daemon_t *daemon);
bool                daemon_relay_tx              (daemon_t *daemon, char const *tx_id);

// NOTE: This command is only available in integration mode, compiled out otherwise in the daemon
void                daemon_relay_votes_and_uptime(daemon_t *daemon);
daemon_status_t     daemon_status                (daemon_t *daemon);

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
  itest_write_to_stdin(&daemon->shared_mem, "exit");
  daemon->shared_mem.clean_up();
}

void daemon_print_checkpoints(daemon_t *daemon)
{
  itest_write_then_read_stdout_until(&daemon->shared_mem, "print_checkpoints", LOKI_STR_LIT("Checkpoint"));
}

uint64_t daemon_print_height(daemon_t *daemon)
{
  itest_read_result output = itest_write_then_read_stdout(&daemon->shared_mem, "print_height");
  uint64_t result = static_cast<uint64_t>(atoi(output.buf.c_str));
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
  itest_write_to_stdin(&daemon->shared_mem, "prepare_registration");
  if (params->num_contributors == 1)
  {
    if (params->open_pool)
    {
      itest_read_until_then_write_stdin(&daemon->shared_mem, LOKI_STR_LIT("Will the operator contribute the entire stake?"), "n");
      itest_read_until_then_write_stdin(&daemon->shared_mem, LOKI_STR_LIT("What percentage of the total staking reward would the operator like to reserve as an operator fee"), owner_fee.c_str);
      itest_read_until_then_write_stdin(&daemon->shared_mem, LOKI_STR_LIT("Do you want to reserve portions of the stake for other specific contributors?"), "n");
      itest_read_until_then_write_stdin(&daemon->shared_mem, LOKI_STR_LIT("Enter the loki address for the operator"), owner->addr.buf.c_str);
      itest_read_until_then_write_stdin(&daemon->shared_mem, LOKI_STR_LIT("How much loki does the operator want to reserve in the stake?"), owner_amount.c_str);
      itest_read_until_then_write_stdin(&daemon->shared_mem, LOKI_STR_LIT("You will leave the remaining portion of"), "y");
      itest_read_until_then_write_stdin(&daemon->shared_mem, LOKI_STR_LIT("Do you confirm the information above is correct?"), "y");
      output = itest_read_stdout(&daemon->shared_mem);

      char const *register_str = str_find(&output.buf, "register_service_node");
      char const *ptr          = register_str;
      result                  &= (register_str != nullptr);

      char const *owner_fee_output      = str_skip_to_next_word(&ptr);
      char const *owner_addr_output     = str_skip_to_next_word(&ptr);
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
      itest_read_until_then_write_stdin(&daemon->shared_mem, LOKI_STR_LIT("Will the operator contribute the entire stake?"), "y");
      itest_read_until_then_write_stdin(&daemon->shared_mem, LOKI_STR_LIT("Enter the loki address for the solo staker"), owner->addr.buf.c_str);
      itest_read_until_then_write_stdin(&daemon->shared_mem, LOKI_STR_LIT("Do you confirm the information above is correct?"), "y");
      output = itest_read_stdout(&daemon->shared_mem);

      char const *register_str = str_find(&output.buf, "register_service_node");
      char const *prev         = register_str;
      result                  &= (register_str != nullptr);

      char const *owner_fee_output      = str_skip_to_next_word(&prev);
      char const *owner_addr_output     = str_skip_to_next_word(&prev);
      char const *owner_portions_output = str_skip_to_next_word(&prev);

      result &= str_match(register_str,          "register_service_node");
      result &= str_match(owner_fee_output,      "18446744073709551612");
      result &= str_match(owner_addr_output,     owner->addr.buf.c_str);
      result &= str_match(owner_portions_output, "18446744073709551612");

      register_snode_start_ptr = register_str;
    }
  }
  else
  {
    itest_read_until_then_write_stdin(&daemon->shared_mem, LOKI_STR_LIT("Will the operator contribute the entire stake?"), "n");
    itest_read_until_then_write_stdin(&daemon->shared_mem, LOKI_STR_LIT("What percentage of the total staking reward would the operator like to reserve as an operator fee"), owner_fee.c_str);
    itest_read_until_then_write_stdin(&daemon->shared_mem, LOKI_STR_LIT("Do you want to reserve portions of the stake for other specific contributors?"), "y");

    int num_extra_contribs             = params->num_contributors - 1;
    char const *num_extra_contribs_str = (num_extra_contribs == 1) ? "1" : (num_extra_contribs == 2) ? "2" : "3";
    itest_read_until_then_write_stdin(&daemon->shared_mem, LOKI_STR_LIT("Number of additional contributors"), num_extra_contribs_str);

    itest_read_until_then_write_stdin(&daemon->shared_mem, LOKI_STR_LIT("Enter the loki address for the operator"), owner->addr.buf.c_str);
    itest_read_until_then_write_stdin(&daemon->shared_mem, LOKI_STR_LIT("How much loki does the operator want to reserve in the stake?"), owner_amount.c_str);

    for (int i = 1; i < params->num_contributors; ++i)
    {
      loki_contributor const *contributor = params->contributors + i;
      loki_buffer<32> contributor_amount("%zu", contributor->amount);
      output = itest_write_then_read_stdout(&daemon->shared_mem, contributor->addr.buf.c_str);
      itest_write_then_read_stdout(&daemon->shared_mem, contributor_amount.c_str);
    }

    if (params->open_pool)
      output = itest_write_then_read_stdout(&daemon->shared_mem, "y"); // You will leave remaining portion for open to contribution etc.

    output                   = itest_write_then_read_stdout_until(&daemon->shared_mem, "y", LOKI_STR_LIT("Run this command in the wallet")); // Confirm
    char const *register_str = str_find(&output.buf, "register_service_node");
    char const *ptr          = register_str;
    result                   &= (register_str != nullptr);

    char const *owner_fee_output  = str_skip_to_next_word(&ptr);
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
      char const *addr_output             = str_skip_to_next_word(&ptr);
      result                             &= str_match(addr_output,contributor->addr.buf.c_str);
      char const *portions_output         = str_skip_to_next_word(&ptr);
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
  itest_read_result output = itest_write_then_read_stdout(&daemon->shared_mem, cmd.c_str);
  if (str_find(output.buf.c_str, "No service node is currently known on the network"))
    return result;

  char const *registration_label        = str_find(output.buf.c_str, "Service Node Registration State");
  char const *num_registered_snodes_str = str_skip_to_next_digit(registration_label);
  int num_registered_snodes             = atoi(num_registered_snodes_str);

  result.known_on_the_network = true;
  result.registered           = (num_registered_snodes == 1);
  return result;
}

bool daemon_print_sn_key(daemon_t *daemon, loki_snode_key *key)
{
  itest_read_result output = itest_write_then_read_stdout_until(&daemon->shared_mem, "print_sn_key", LOKI_STR_LIT("Service Node Public Key: "));
  char const *key_ptr = str_find(output.buf.c_str, ":");
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
  itest_read_result output    = itest_write_then_read_stdout(&daemon->shared_mem, "print_sn_status");

  if (str_find(output.buf.c_str, "No service node is currently known on the network"))
    return result;

  result.known_on_the_network    = true;
  char const *registration_label = str_find(output.buf.c_str, "Service Node Registration State");
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
  itest_read_result output = itest_write_then_read_stdout_until(&daemon->shared_mem, cmd.c_str, LOKI_STR_LIT("Staking Requirement: "));
  char const *staking_requirement_str = str_find(output.buf.c_str, ":");
  ++staking_requirement_str;
  uint64_t result = str_parse_loki_amount(staking_requirement_str);
  return result;
}

bool daemon_print_tx(daemon_t *daemon, char const *tx_id, loki_scratch_buf *output)
{
  loki_buffer<256> cmd("print_tx %s +json", tx_id);
  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STR_LIT("Error: Transaction wasn't found"), true},
    {LOKI_STR_LIT("output_unlock_times"), false},
  };

  itest_read_result read_result = itest_write_then_read_stdout_until(&daemon->shared_mem, cmd.c_str, possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (possible_values[read_result.matching_find_strs_index].is_fail_msg)
    return false;

  if (output) *output = read_result.buf;
  return true;
}

void daemon_print_cn(daemon_t *daemon)
{
  itest_write_to_stdin(&daemon->shared_mem, "print_cn");
  itest_read_stdout_sink(&daemon->shared_mem, 1000/*ms*/);
}

bool daemon_relay_tx(daemon_t *daemon, char const *tx_id)
{
  loki_buffer<256> cmd("relay_tx %s", tx_id);
  itest_read_possible_value const possible_values[] =
  {
    {LOKI_STR_LIT("Unsuccessful -- transaction not found in pool"), true},
    {LOKI_STR_LIT("Transaction successfully relayed"), false},
  };

  itest_read_result read_result = itest_write_then_read_stdout_until(&daemon->shared_mem, cmd.c_str, possible_values, LOKI_ARRAY_COUNT(possible_values));
  if (possible_values[read_result.matching_find_strs_index].is_fail_msg)
    return false;

  return true;
}

void daemon_relay_votes_and_uptime(daemon_t *daemon)
{
  itest_write_then_read_stdout_until(&daemon->shared_mem, "relay_votes_and_uptime", LOKI_STR_LIT("Votes and uptime relayed"));
}

daemon_status_t daemon_status(daemon_t *daemon)
{
  // Example:
  // Height: 67/67 (100.0%) on testnet, not mining, net hash 4 H/s, v9, up to date, 0(out)+0(in) connections, uptime 0d 0h 0m 0s

  daemon_status_t result  = {};
  itest_read_result output = itest_write_then_read_stdout_until(&daemon->shared_mem, "status", LOKI_STR_LIT("Height: "));
  char const *ptr        = output.buf.c_str;
  char const *height_str = str_skip_to_next_digit_inplace(&ptr);
  result.height          = atoi(height_str);

  //                                        V <- ptr is here, ++ptr to move it past the comma
  ptr = (str_find(ptr, ","));   // <nettype>,
  ptr = (str_find(++ptr, ",")); // <mining status>,
  ptr = (str_find(++ptr, ",")); // <hash rate>,
  ptr = (str_find(++ptr, "v")); // v<hardfork version>
  ptr++;
  assert(ptr && char_is_num(ptr[0]));
  result.hf_version = atoi(ptr);
  return result;
}

#endif // LOKI_DAEMON_IMPLEMENTATION
