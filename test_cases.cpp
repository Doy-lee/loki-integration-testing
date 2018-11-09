#include "test_cases.h"

#include <chrono>
#include <thread>

inline bool char_is_alpha(char ch)
{
  bool result = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
  return result;
}

inline bool char_is_num(char ch)
{
  bool result = (ch >= '0' && ch <= '9');
  return result;
}

char const *str_find(char const *src, int src_len, char const *find, int find_len = -1)
{
  if (find_len == -1) find_len = strlen(find);

  char const *buf_ptr = src;
  char const *buf_end = buf_ptr + src_len;
  char const *result  = nullptr;

  for (;*buf_ptr; ++buf_ptr)
  {
    int len_remaining = static_cast<int>(buf_end - buf_ptr);
    if (len_remaining < find_len) break;

    if (strncmp(buf_ptr, find, find_len) == 0)
    {
      result = buf_ptr;
      break;
    }
  }

  return result;
}

bool str_match(char const *src, char const *expect, int expect_len = -1)
{
  if (expect_len == -1) expect_len = strlen(expect);
  bool result = (strncmp(src, expect, expect_len) == 0);
  return result;
}

char const *str_find(loki_scratch_buf const *buf, char const *find, int find_len = -1)
{
  char const *result = str_find(buf->data, buf->len, find, find_len);
  return result;
}

inline char const *str_skip_to_next_digit(char const *src)
{
  char const *result = src;
  while (result && !char_is_num(result[0])) ++result;
  return result;
}

inline bool char_is_alphanum(char ch)
{
  bool result = char_is_alpha(ch) || char_is_num(ch);
  return result;
}

inline bool char_is_whitespace(char ch)
{
  bool result = (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
  return result;
}

inline char const *str_skip_to_next_alphanum(char const *src)
{
  char const *result = src;
  while (result && !char_is_alphanum(result[0])) ++result;
  return result;
}

inline char const *str_skip_to_next_alpha_char(char const *src)
{
  char const *result = src;
  while (result && !char_is_alpha(result[0])) ++result;
  return result;
}

inline char const *str_skip_to_next_whitespace(char const *src)
{
  char const *result = src;
  while (result && !char_is_whitespace(result[0])) ++result;
  return result;
}

inline char const *str_skip_to_next_word(char const **src)
{
  while ((*src) && !char_is_whitespace((*src)[0])) ++(*src);
  while ((*src) &&  char_is_whitespace((*src)[0])) ++(*src);
  return *src;
}

inline char const *str_skip_whitespace(char const *src)
{
  char const *result = src;
  while (result && char_is_whitespace(result[0])) ++result;
  return result;
}

void test_result::print_result()
{
  if (failed)
  {
    printf("  %s: FAILED\n",       name.data);
    printf("      Expected: %s\n", expected_str.data);
    printf("      Received: %s\n", received_str.data);
    printf("      Context:  %s\n", captured_stdout.data);
  }
  else
  {
    printf("  %s: PASSED\n", name.data);
  }
}

#define STR_EXPECT(test_result_var, src, expect_str) \
if (!str_match(src, expect_str)) \
{ \
  test_result_var.failed = true; \
  test_result_var.received_str = loki_buffer<1024>(src, (int)strlen(expect_str)); \
  test_result_var.expected_str = expect_str; \
  return test_result_var; \
}

#define INITIALISE_TEST_CONTEXT(test_result_var) reset_shared_memory(); test_result_var.name = loki_buffer<512>(__func__)

test_result prepare_registration__solo_auto_stake()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  char const wallet1[] = "T6U4ukY68vohsfrGMryFmqX5yRE4d5EC8E6QbinSo8ssW3heqoNjgNggTeym9NSLW4cnEp3ckpD9RZLW5qDGg3821c9SAtHMD";

  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "prepare_registration");
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y"); // Contribute entire stake?
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, wallet1); // Operator address
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y"); // Auto restake?
  result.captured_stdout = write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y"); // Confirm information correct?

  // Expected Format: register_service_node [auto] <operator cut> <address> <fraction> [<address> <fraction> [...]]]
  char const *register_str      = str_find(&result.captured_stdout, "register_service_node");
  char const *prev              = register_str;

  char const *auto_stake        = str_skip_to_next_word(&prev);
  char const *operator_portions = str_skip_to_next_word(&prev);
  char const *wallet_addr       = str_skip_to_next_word(&prev);
  char const *addr1_portions    = str_skip_to_next_word(&prev);

  STR_EXPECT(result, register_str,      "register_service_node");
  STR_EXPECT(result, auto_stake,        "auto");
  STR_EXPECT(result, operator_portions, "18446744073709551612");
  STR_EXPECT(result, wallet_addr,       wallet1);
  STR_EXPECT(result, addr1_portions,    "18446744073709551612");

  return result;
}

test_result prepare_registration__100_percent_operator_cut_auto_stake()
{

  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  char const wallet1[] = "T6U4ukY68vohsfrGMryFmqX5yRE4d5EC8E6QbinSo8ssW3heqoNjgNggTeym9NSLW4cnEp3ckpD9RZLW5qDGg3821c9SAtHMD";
  char const wallet2[] = "T6TZgnpJ2uaC1cqS4E6M6u7QmGA79q2G19ToBHnqWHxMMDocNTiw2phg52XjkAmEZH9V5xQUsaR3cbcTnELE1vXP2YkhEqXad";

  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "prepare_registration");
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "n"); // Contribute entire stake?
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "100%"); // Percentage stake
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "50"); // How much loki to reserve?
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y"); // Do you want to reserve portions of the stake for other contribs?
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "1"); // Number of additional contributors
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, wallet1); // Operator address
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "25"); // How much loki to reserve for contributor 1
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, wallet2); // Contrib 1 address
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y"); // How much loki to reserve for contributor 1
  write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y"); // Autostake
  result.captured_stdout = write_to_stdin_mem_and_get_result(shared_mem_type::daemon, "y"); // Confirm

  // Expected Format: register_service_node [auto] <operator cut> <address> <fraction> [<address> <fraction> [...]]]
  char const *register_str     = str_find(&result.captured_stdout, "register_service_node");
  char const *prev             = register_str;

  char const *auto_stake       = str_skip_to_next_word(&prev);
  char const *operator_cut     = str_skip_to_next_word(&prev);
  char const *wallet1_addr     = str_skip_to_next_word(&prev);
  char const *wallet1_portions = str_skip_to_next_word(&prev);
  char const *wallet2_addr     = str_skip_to_next_word(&prev);
  char const *wallet2_portions = str_skip_to_next_word(&prev);

  STR_EXPECT(result, register_str,     "register_service_node");
  STR_EXPECT(result, auto_stake,       "auto");
  STR_EXPECT(result, operator_cut,     "18446744073709551612");
  STR_EXPECT(result, wallet1_addr,     wallet1);
  STR_EXPECT(result, wallet1_portions, "9223372036854775806"); // exactly 50% of staking portions
  STR_EXPECT(result, wallet2_addr,     wallet2);
  STR_EXPECT(result, wallet2_portions, "4611686018427387903"); // exactly 25% of staking portions

  return result;
}

test_result stake__from_subaddress()
{
  test_result result = {};
  INITIALISE_TEST_CONTEXT(result);

  daemon_t daemon          = start_daemon();
  wallet_t operator_wallet = create_wallet();
  start_wallet(&operator_wallet);

  // wallet_t stakers_wallet  = create_wallet();

  result.captured_stdout = write_to_stdin_mem_and_get_result(shared_mem_type::wallet, loki_scratch_buf("set_daemon 127.0.0.1:%d", daemon.rpc_port).data);
  result.captured_stdout = write_to_stdin_mem_and_get_result(shared_mem_type::wallet, "start_mining");
  STR_EXPECT(result, result.captured_stdout.data, "Mining started in daemon");
  write_to_stdin_mem_and_get_result(shared_mem_type::wallet, "set refresh-from-block-height 0");
  printf("%s\n", write_to_stdin_mem_and_get_result(shared_mem_type::wallet, "refresh").data);
  write_to_stdin_mem_and_get_result(shared_mem_type::wallet, "exit");

  return result;
}
