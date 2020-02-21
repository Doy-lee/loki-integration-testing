#ifndef LOKI_WALLET_H
#define LOKI_WALLET_H
// -------------------------------------------------------------------------------------------------
//
// header
//
// -------------------------------------------------------------------------------------------------
struct wallet_params
{
  bool     disable_asking_password   = true;
  uint64_t refresh_from_block_height = 0;
};

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


// TODO(doyle): This function should probably run by default since you almost always want it
void                 wallet_set_default_testing_settings (wallet_t *wallet, wallet_params const params = {});
itest_ipc_result     wallet_address                      (wallet_t *wallet, int index, loki_addr *addr = nullptr); // Switch to subaddress at index
bool                 wallet_address_new                  (wallet_t *wallet, loki_addr *addr);
uint64_t             wallet_balance                      (wallet_t *wallet, uint64_t *unlocked_balance);
void                 wallet_exit                         (wallet_t *wallet);
bool                 wallet_integrated_address           (wallet_t *wallet, loki_addr *addr);
bool                 wallet_payment_id                   (wallet_t *wallet, loki_payment_id64 *id);
wallet_locked_stakes wallet_print_locked_stakes          (wallet_t *wallet);
bool                 wallet_refresh                      (wallet_t *wallet);
bool                 wallet_set_daemon                   (wallet_t *wallet, struct daemon_t const *daemon);
bool                 wallet_stake                        (wallet_t *wallet, loki_snode_key const *service_node_key, uint64_t amount, loki_transaction_id *tx_id = nullptr);
bool                 wallet_status                       (wallet_t *wallet, uint64_t *height); // height: The current height the wallet is synced at

// TODO(doyle): Need to support integrated address
itest_ipc_result     wallet_sweep_all                    (wallet_t *wallet, char      const *dest, loki_transaction *tx);
bool                 wallet_transfer                     (wallet_t *wallet, char      const *dest, uint64_t amount, loki_transaction *tx); // TODO(doyle): We only support whole amounts. Not atomic units either.
bool                 wallet_transfer                     (wallet_t *wallet, loki_addr const *dest, uint64_t amount, loki_transaction *tx);

// TODO(doyle): We should be able to roughly calculate how much blocks we need to mine
uint64_t             wallet_mine_until_unlocked_balance  (wallet_t *wallet, daemon_t *daemon, uint64_t desired_unlocked_balance, int blocks_between_check = LOKI_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW);

// TODO(doyle): This should return the transaction
bool                 wallet_request_stake_unlock         (wallet_t *wallet, loki_snode_key const *snode_key, uint64_t *unlock_height = nullptr);
bool                 wallet_register_service_node        (wallet_t *wallet, char const *registration_cmd, loki_transaction *tx = nullptr);
itest_ipc_result     wallet_buy_lns_mapping              (wallet_t *wallet, loki_string *owner, loki_string name, loki_string value, loki_transaction *tx = nullptr);
#endif
