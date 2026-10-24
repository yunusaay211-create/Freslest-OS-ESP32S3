#pragma once
#include <cstdlib>
#include <cstring>
#include <stddef.h>
#include <stdint.h>
// Bounded helpers shared by the TCP worker and host tests.
namespace browser_protocol {
inline bool append(char *out, size_t cap, size_t &n, const char *s) {
  size_t k = strlen(s);
  if (n + k >= cap)
    return false;
  memcpy(out + n, s, k + 1);
  n += k;
  return true;
}
inline bool quoted(char *out, size_t cap, size_t &n, const char *s) {
  if (!append(out, cap, n, "\""))
    return false;
  for (; *s; s++) {
    char b[7] = {*s, 0};
    unsigned char c = *s;
    if (c == '"' || c == '\\') {
      b[0] = '\\';
      b[1] = c;
      b[2] = 0;
    } else if (c < 32) {
      const char *hex = "0123456789abcdef";
      b[0] = '\\';
      b[1] = 'u';
      b[2] = '0';
      b[3] = '0';
      b[4] = hex[c >> 4];
      b[5] = hex[c & 15];
      b[6] = 0;
    }
    if (!append(out, cap, n, b))
      return false;
  }
  return append(out, cap, n, "\"");
}
inline bool unsignedDecimal(const char *s, size_t &value) {
  if (!*s)
    return false;
  value = 0;
  for (; *s; s++) {
    if (*s < '0' || *s > '9' || value > (SIZE_MAX - 9) / 10)
      return false;
    value = value * 10 + *s - '0';
  }
  return true;
}
inline int unhex(char c) {
  return c >= '0' && c <= '9'   ? c - '0'
         : c >= 'A' && c <= 'F' ? c - 'A' + 10
         : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                : -1;
}
inline void percentDecode(char *out, size_t cap, const char *s) {
  size_t n = 0;
  while (*s && n + 1 < cap) {
    unsigned char c = *s++;
    if (c == '%' && s[0] && s[1]) {
      int a = unhex(s[0]), b = unhex(s[1]);
      if (a >= 0 && b >= 0) {
        c = a * 16 + b;
        s += 2;
      }
    }
    if (c >= 32)
      out[n++] = c;
  }
  out[n] = 0;
}
inline bool pageUrl(const char *s) {
  size_t n = strnlen(s, 384);
  size_t prefix = !strncmp(s, "https://", 8)  ? 8
                  : !strncmp(s, "http://", 7) ? 7
                                              : 0;
  if (!prefix || n <= prefix || n >= 384 || s[prefix] == '/' ||
      s[prefix] == ':' || s[prefix] == '?')
    return false;
  for (size_t i = 0; i < n; i++)
    if ((unsigned char)s[i] <= 32 || (unsigned char)s[i] == 127)
      return false;
  return true;
}
} // namespace browser_protocol
