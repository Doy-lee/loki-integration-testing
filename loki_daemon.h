#ifndef LOKI_DAEMON_H
#define LOKI_DAEMON_H

// Define LOKI_DAEMON_IMPLEMENTATION in one CPP file

//
// Header
//

struct daemon_prepare_registration_params
{
  static daemon_prepare_registration_params solo_staker(wallet_t wallet);

  bool             auto_stake;
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
struct daemon_service_node_status_t
{
  bool known_on_the_network;
  bool registered;
};

void                         daemon_exit                (daemon_t *daemon);
bool                         daemon_prepare_registration(daemon_t *daemon, daemon_prepare_registration_params const *params, loki_scratch_buf *registration_cmd);
uint64_t                     daemon_print_height        (daemon_t *daemon);
bool                         daemon_print_sn_key        (daemon_t *daemon, loki_snode_key *key);
daemon_service_node_status_t daemon_print_sn_status     (daemon_t *daemon); // return: If the node is known on the network (i.e. registered)
uint64_t                     daemon_print_sr            (daemon_t *daemon, uint64_t height);
daemon_status_t              daemon_status              (daemon_t *daemon);

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
  itest_write_to_stdin_mem(&daemon->shared_mem, "exit");
  os_sleep_ms(500);
}

uint64_t daemon_print_height(daemon_t *daemon)
{
  loki_scratch_buf output = itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, "print_height");
  uint64_t result         = static_cast<uint64_t>(atoi(output.c_str));
  return result;
}

bool daemon_prepare_registration(daemon_t *daemon, daemon_prepare_registration_params const *params, loki_scratch_buf *registration_cmd)
{
  bool result = true;
  assert(params->num_contributors >= 1 && params->num_contributors <= 4);

  if (params->open_pool)
    assert(params->num_contributors != 4);

  char const *auto_stake_str  = (params->auto_stake) ? "y" : "n";
  loki_contributor const *owner = params->contributors + 0;

  loki_buffer<32> owner_portions("%zu", amount_to_staking_portions(owner->amount));
  loki_buffer<32> owner_fee     ("%d",  params->owner_fee_percent);
  loki_buffer<32> owner_amount  ("%zu", owner->amount);

  // TODO(doyle): Handle fee properly

  // Expected Format: register_service_node [auto] <owner cut> <address> <fraction> [<address> <fraction> [...]]]
  loki_scratch_buf output                     = {};
  char const *register_service_node_start_ptr = nullptr;
  if (params->num_contributors == 1)
  {
    if (params->open_pool)
    {
      itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, "prepare_registration");
      itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, "n");                  // Contribute entire stake?
      itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, owner_fee.c_str);
      itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, owner_amount.c_str);
      itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, "n");                  // Reserve for other contributors?
      itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, owner->addr.buf.c_str);
      itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, "y");                  // You will leave this open for other people. Is this okay?
      itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, auto_stake_str);     // Auto restake?
      output = itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, "y");         // Confirm?

      char const *register_str = str_find(&output, "register_service_node");
      char const *ptr          = register_str;
      result                  &= (register_str != nullptr);

      if (params->auto_stake)
      {
        char const *auto_stake_output = str_skip_to_next_word(&ptr);
        result &= str_match(auto_stake_output, "auto");
      }

      char const *owner_fee_output      = str_skip_to_next_word(&ptr);
      char const *owner_addr_output     = str_skip_to_next_word(&ptr);
      // char const *owner_portions_output = str_skip_to_next_word(&ptr);

      result &= str_match(owner_fee_output,      owner_fee.c_str);
      result &= str_match(owner_addr_output,     owner->addr.buf.c_str);

      // TODO(doyle): Validate amounts, because we don't use the same portions
      // calculation as loki daemon we can be off by small amounts.
      // result &= str_match(owner_portions_output, owner_portions.c_str);

      register_service_node_start_ptr = register_str;
    }
    else
    {
      itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, "prepare_registration");
      itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, "y");                   // Contribute entire stake?
      itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, owner->addr.buf.c_str); // Operator address
      itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, auto_stake_str);      // Auto restake?
      output = itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, "y"); // Confirm

      char const *register_str = str_find(&output, "register_service_node");
      char const *prev         = register_str;
      result                  &= (register_str != nullptr);

      if (params->auto_stake)
      {
        char const *auto_stake = str_skip_to_next_word(&prev);
        result &= str_match(auto_stake, "auto");
      }

      char const *owner_fee_output      = str_skip_to_next_word(&prev);
      char const *owner_addr_output     = str_skip_to_next_word(&prev);
      char const *owner_portions_output = str_skip_to_next_word(&prev);

      result &= str_match(register_str,          "register_service_node");
      result &= str_match(owner_fee_output,      "18446744073709551612");
      result &= str_match(owner_addr_output,     owner->addr.buf.c_str);
      result &= str_match(owner_portions_output, "18446744073709551612");

      register_service_node_start_ptr = register_str;
    }
  }
  else
  {
    itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, "prepare_registration");
    itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, "n");                // Contribute entire stake?
    itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, owner_fee.c_str);    // Operator cut
    itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, owner_amount.c_str); // How much loki to reserve?

    int num_extra_contribs             = params->num_contributors - 1;
    char const *num_extra_contribs_str = (num_extra_contribs == 1) ? "1" : (num_extra_contribs == 2) ? "2" : "3";
    itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, "y");                    // Do you want to reserve portions of the stake for other contribs?
    itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, num_extra_contribs_str); // Number of additional contributors
    itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, owner->addr.buf.c_str); // Operator address

    for (int i = 1; i < params->num_contributors; ++i)
    {
      loki_contributor const *contributor = params->contributors + i;
      loki_buffer<32> contributor_amount("%zu", contributor->amount);
      itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, contributor_amount.c_str); // How much loki to reserve for contributor
      output = itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, contributor->addr.buf.c_str);  // Contrib address
    }

    if (params->open_pool)
    {
      itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, "y"); // You will leave remaining portion for open to contribution etc.
    }

    itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, auto_stake_str); // Autostake

    output                    = itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, "y"); // Confirm
    char const *register_str  = str_find(&output, "register_service_node");
    char const *ptr          = register_str;
    result                   &= (register_str != nullptr);
    if (params->auto_stake)
    {
      char const *auto_stake_output = str_skip_to_next_word(&ptr);
      result &= str_match(auto_stake_output, "auto");
    }

    char const *owner_fee_output  = str_skip_to_next_word(&ptr);
    // TODO(doyle): Hack handle owner fees better
    if (params->owner_fee_percent == 100)
    {
      result &= str_match(owner_fee_output,      "18446744073709551612");
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

    register_service_node_start_ptr = register_str;
  }

  if (result)
  {
    char const *start = register_service_node_start_ptr;
    char const *end   = start;
    while (end[0] && end[0] != '\n')
      ++end;

    int len = static_cast<int>(end - start);
    registration_cmd->append("%.*s", len, start);
  }

  return result;
}

bool daemon_print_sn_key(daemon_t *daemon, loki_snode_key *key)
{
  loki_scratch_buf output = itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, "print_sn_key");
  if (!str_match(output.c_str, "Service Node Public Key"))
    return false;

  char const *key_ptr = str_find(output.c_str, ":");
  key_ptr = str_skip_to_next_alphanum(key_ptr);

  if (key) *key = key_ptr;
  return true;
}

daemon_service_node_status_t daemon_print_sn_status(daemon_t *daemon)
{

  daemon_service_node_status_t result = {};
  loki_scratch_buf output             = itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, "print_sn_status");

  if (str_find(output.c_str, "No service node is known on the network for"))
    return result;

  char const *registration_label               = str_find(output.c_str, "Service Node Registration State");
  char const *num_registered_service_nodes_str = str_skip_to_next_digit(registration_label);
  int num_registered_service_nodes             = atoi(num_registered_service_nodes_str);

  result.known_on_the_network = true;
  result.registered           = (num_registered_service_nodes == 1);
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
  loki_scratch_buf output = itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, cmd.c_str);
  assert(str_match(output.c_str, "Staking Requirement: "));

  char const *staking_requirement_str = str_find(output.c_str, ":");
  assert(staking_requirement_str);
  ++staking_requirement_str;

  uint64_t result = str_parse_loki_amount(staking_requirement_str);
  return result;
}

daemon_status_t daemon_status(daemon_t *daemon)
{
  // Example:
  // Height: 67/67 (100.0%) on testnet, not mining, net hash 4 H/s, v9, up to date, 0(out)+0(in) connections, uptime 0d 0h 0m 0s

  daemon_status_t result  = {};
  loki_scratch_buf output = itest_write_to_stdin_mem_and_get_result(&daemon->shared_mem, "status").c_str;

  char const *ptr = output.c_str;
  assert(str_match(ptr, "Height: "));
  char const *height_str = str_skip_to_next_digit_inplace(&ptr);
  result.height = atoi(height_str);

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
