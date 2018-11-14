#ifndef LOKI_STR_H
#define LOKI_STR_H

// Define LOKI_STR_IMPLEMENTATION in one CPP file

//
// Header
//
#include "loki_integration_tests.h"

bool        char_is_alpha              (char ch);
bool        char_is_num                (char ch);
bool        char_is_alphanum           (char ch);
bool        char_is_whitespace         (char ch);
char const *str_find                   (char const *src,              char const *find);
char const *str_find                   (char const *src, int src_len, char const *find, int find_len = -1);
char const *str_find                   (loki_scratch_buf const *buf,  char const *find, int find_len = -1);
bool        str_match                  (char const *src,              char const *find, int find_len = -1);
uint64_t    str_parse_loki_amount      (char const *amount);
char const *str_skip_to_next_digit     (char const *src);
char const *str_skip_to_next_alphanum  (char const *src);
char const *str_skip_to_next_alpha_char(char const *src);
char const *str_skip_to_next_whitespace(char const *src);
char const *str_skip_to_next_word      (char const **src);
char const *str_skip_whitespace        (char const *src);

#endif // LOKI_STR_H

//
// CPP Implementation
//
#ifdef LOKI_STR_IMPLEMENTATION
bool char_is_alpha(char ch)
{
  bool result = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
  return result;
}

bool char_is_num(char ch)
{
  bool result = (ch >= '0' && ch <= '9');
  return result;
}

char const *str_find(char const *src, int src_len, char const *find, int find_len)
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

char const *str_find(char const *src, char const *find)
{
  char const *result = str_find(src, strlen(src), find, strlen(find));
  return result;
}

char const *str_find(loki_scratch_buf const *buf, char const *find, int find_len)
{
  char const *result = str_find(buf->data, buf->len, find, find_len);
  return result;
}

bool str_match(char const *src, char const *find, int find_len)
{
  if (find_len == -1) find_len = strlen(find);
  bool result = (strncmp(src, find, find_len) == 0);
  return result;
}

uint64_t str_parse_loki_amount(char const *amount)
{
  // Example
  // 0.000000000
  char const *atomic_ptr = amount;
  while(atomic_ptr[0] && atomic_ptr[0] != '.') atomic_ptr++;
  atomic_ptr++;

  uint64_t whole_part  = atoi(amount) * LOKI_ATOMIC_UNITS;
  uint64_t atomic_part = atoi(++atomic_ptr);

  uint64_t result = whole_part + atomic_part;
  return result;
}

char const *str_skip_to_next_digit(char const *src)
{
  char const *result = src;
  while (result && !char_is_num(result[0])) ++result;
  return result;
}

bool char_is_alphanum(char ch)
{
  bool result = char_is_alpha(ch) || char_is_num(ch);
  return result;
}

bool char_is_whitespace(char ch)
{
  bool result = (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
  return result;
}

char const *str_skip_to_next_alphanum(char const *src)
{
  char const *result = src;
  while (result && !char_is_alphanum(result[0])) ++result;
  return result;
}

char const *str_skip_to_next_alpha_char(char const *src)
{
  char const *result = src;
  while (result && !char_is_alpha(result[0])) ++result;
  return result;
}

char const *str_skip_to_next_whitespace(char const *src)
{
  char const *result = src;
  while (result && !char_is_whitespace(result[0])) ++result;
  return result;
}

char const *str_skip_to_next_word(char const **src)
{
  while ((*src) && !char_is_whitespace((*src)[0])) ++(*src);
  while ((*src) &&  char_is_whitespace((*src)[0])) ++(*src);
  return *src;
}

char const *str_skip_whitespace(char const *src)
{
  char const *result = src;
  while (result && char_is_whitespace(result[0])) ++result;
  return result;
}
#endif // LOKI_STR_IMPLEMENTATION
