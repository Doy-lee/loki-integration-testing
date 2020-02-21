#ifndef LOKI_DAEMON_H
#define LOKI_DAEMON_H
// -------------------------------------------------------------------------------------------------
//
// header
//
// -------------------------------------------------------------------------------------------------
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

void                           daemon_exit                           (daemon_t *daemon);
bool                           daemon_prepare_registration           (daemon_t *daemon, daemon_prepare_registration_params const *params, loki_fixed_string<> *registration_cmd);
std::vector<daemon_checkpoint> daemon_print_checkpoints              (daemon_t *daemon);
uint64_t                       daemon_print_height                   (daemon_t *daemon);
daemon_snode_status            daemon_print_sn                       (daemon_t *daemon, loki_snode_key const *key); // TODO(doyle): We can't request the entire sn list because this needs a big buffer and I cbb doing mem management over shared mem
bool                           daemon_print_sn_key                   (daemon_t *daemon, loki_snode_key *key);
daemon_snode_status            daemon_print_sn_status                (daemon_t *daemon); // return: If the node is known on the network (i.e. registered)
uint64_t                       daemon_print_sr                       (daemon_t *daemon, uint64_t height);
bool                           daemon_print_tx                       (daemon_t *daemon, char const *tx_id, std::string *output);
void                           daemon_print_cn                       (daemon_t *daemon);
bool                           daemon_relay_tx                       (daemon_t *daemon, char const *tx_id);
bool                           daemon_ban                            (daemon_t *daemon, loki_fixed_string<32> const *ip);
bool                           daemon_unban                          (daemon_t *daemon, loki_fixed_string<32> const *ip);
bool                           daemon_set_log                        (daemon_t *daemon, int level);
daemon_status_t                daemon_status                         (daemon_t *daemon);
bool                           daemon_print_block                    (daemon_t *daemon, uint64_t height, loki_hash64 *block_hash);

// NOTE: This command is only available in integration mode, compiled out otherwise in the daemon
void                           daemon_relay_votes_and_uptime         (daemon_t *daemon);

// NOTE: Debug integration_test <sub cmd>, style of commands enabled in integration mode
bool                           daemon_mine_n_blocks                  (daemon_t *daemon, wallet_t *wallet, int num_blocks);
void                           daemon_mine_n_blocks                  (daemon_t *daemon, loki_addr const *addr, int num_blocks);
void                           daemon_mine_until_height              (daemon_t *daemon, loki_addr const *addr, uint64_t desired_height);
bool                           daemon_mine_until_height              (daemon_t *daemon, wallet_t *wallet, uint64_t desired_height);
void                           daemon_toggle_checkpoint_quorum       (daemon_t *daemon);
void                           daemon_toggle_obligation_quorum       (daemon_t *daemon);
void                           daemon_toggle_obligation_uptime_proof (daemon_t *daemon);
void                           daemon_toggle_obligation_checkpointing(daemon_t *daemon);
#endif
