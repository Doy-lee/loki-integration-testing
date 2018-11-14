#ifndef LOKI_DAEMON_H
#define LOKI_DAEMON_H

// Define LOKI_DAEMON_IMPLEMENTATION in one CPP file

//
// Header
//

struct loki_contributor
{
  loki_addr addr;
  uint64_t  amount;
};

struct daemon_prepare_registration_params
{
  bool             auto_stake;
  bool             open_pool;
  int              owner_fee_percent;
  int              num_contributors;
  loki_contributor contributors[4];
};

bool daemon_prepare_registration(daemon_prepare_registration_params const *params, loki_scratch_buf *registration_cmd);
bool daemon_print_sn_key        (loki_snode_key *key);

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

bool daemon_prepare_registration(daemon_prepare_registration_params const *params, loki_scratch_buf *registration_cmd)
{
  bool result = true;
  assert(params->num_contributors >= 1 && params->num_contributors <= 4);

  if (params->open_pool)
    assert(params->num_contributors != 4);

  char const *auto_stake_str  = (params->auto_stake) ? "y" : "n";
  loki_contributor const *owner = params->contributors + 0;

  loki_buffer<32> owner_portions("%zu", amount_to_staking_portions(owner->amount));
  loki_buffer<8>  owner_fee     ("%d",  params->owner_fee_percent);
  loki_buffer<8>  owner_amount  ("%zu", owner->amount);

  // Expected Format: register_service_node [auto] <owner cut> <address> <fraction> [<address> <fraction> [...]]]
  loki_scratch_buf output                     = {};
  char const *register_service_node_start_ptr = nullptr;
  if (params->num_contributors == 1)
  {
    if (params->open_pool)
    {
      write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "prepare_registration");
      write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "n");                  // Contribute entire stake?
      write_to_stdin_mem_and_get_result(shared_mem_type::daemon, owner_fee.c_str);
      write_to_stdin_mem_and_get_result(shared_mem_type::daemon, owner_amount.c_str);
      write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "n");                  // Reserve for other contributors?
      write_to_stdin_mem_and_get_result(shared_mem_type::daemon, owner->addr.c_str);
      write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y");                  // You will leave this open for other people. Is this okay?
      write_to_stdin_mem_and_get_result(shared_mem_type::daemon, auto_stake_str);     // Auto restake?
      output = write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y");         // Confirm?

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
      char const *owner_portions_output = str_skip_to_next_word(&ptr);

      result &= str_match(owner_fee_output,      owner_fee.c_str);
      result &= str_match(owner_addr_output,     owner->addr.c_str);
      result &= str_match(owner_portions_output, owner_portions.c_str);

      register_service_node_start_ptr = register_str;
    }
    else
    {
      write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "prepare_registration");
      write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y");                   // Contribute entire stake?
      write_to_stdin_mem_and_get_result(shared_mem_type::daemon, owner->addr.c_str);     // Operator address
      write_to_stdin_mem_and_get_result(shared_mem_type::daemon, auto_stake_str);      // Auto restake?
      output = write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y"); // Confirm

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
      result &= str_match(owner_addr_output,     owner->addr.c_str);
      result &= str_match(owner_portions_output, "18446744073709551612");

      register_service_node_start_ptr = register_str;
    }
  }
  else
  {
    // TODO(doyle): Complete implementation
    assert(false);
    write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "n");                // Contribute entire stake?
    write_to_stdin_mem_and_get_result(shared_mem_type::daemon, owner_fee.c_str);    // Operator cut
    write_to_stdin_mem_and_get_result(shared_mem_type::daemon, owner_amount.c_str); // How much loki to reserve?

    int num_extra_contribs             = params->num_contributors - 1;
    char const *num_extra_contribs_str = (num_extra_contribs == 2) ? "2" : "3";
    write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y");                    // Do you want to reserve portions of the stake for other contribs?
    write_to_stdin_mem_and_get_result(shared_mem_type::daemon, num_extra_contribs_str); // Number of additional contributors
    write_to_stdin_mem_and_get_result(shared_mem_type::daemon, owner->addr.c_str);      // Operator address

    for (int i = 0; i < params->num_contributors; ++i)
    {
      loki_contributor const *contributor = params->contributors + i;
      loki_buffer<32> contributor_amount("%zu", contributor->amount);
      write_to_stdin_mem_and_get_result(shared_mem_type::daemon, contributor_amount.c_str); // How much loki to reserve for contributor
      write_to_stdin_mem_and_get_result(shared_mem_type::daemon, contributor->addr.c_str);  // Contrib address
    }

    write_to_stdin_mem_and_get_result(shared_mem_type::daemon, auto_stake_str); // Autostake

    output                    = write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y"); // Confirm
    char const *register_str  = str_find(&output, "register_service_node");
    char const *prev          = register_str;
    result                   &= (register_str != nullptr);

    char const *auto_stake_output = str_skip_to_next_word(&prev);
    char const *owner_fee_output  = str_skip_to_next_word(&prev);

    result &= str_match(auto_stake_output, "auto");
    result &= str_match(owner_fee_output,  owner_fee.c_str);

    for (int i = 0; i < params->num_contributors; ++i)
    {
      loki_contributor const *contributor = params->contributors + i;

      char const *addr_output     = str_skip_to_next_word(&prev);
      char const *portions_output = str_skip_to_next_word(&prev);

      loki_buffer<32> contributor_portions("%zu", amount_to_staking_portions(contributor->amount));
      result &= str_match(addr_output,     contributor->addr.c_str);
      result &= str_match(portions_output, contributor_portions.c_str); // exactly 50% of staking portions
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

bool daemon_print_sn_key(loki_snode_key *key)
{
  loki_scratch_buf output = write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "print_sn_key");
  if (!str_match(output.c_str, "Service Node Public Key"))
    return false;

  char const *key_ptr = str_find(output.c_str, ":");
  key_ptr = str_skip_to_next_alphanum(key_ptr);

  if (key) *key = key_ptr;
  return true;
}

#endif // LOKI_DAEMON_IMPLEMENTATION
