#ifndef PBLOG_TEST_COMMON_HH
#define PBLOG_TEST_COMMON_HH

#include <cstdarg>
#include <cstdio>
#include <string>

#include <pblog/common.h>

#define DEFAULT_MIN_SEVERITY 2

extern "C" {

int pblog_printf(int severity, const char *format, ...) {
  if (DEFAULT_MIN_SEVERITY > severity) return 0;

  va_list args;
  va_start(args, format);
  int ret = vfprintf(stderr, format, args);
  va_end(args);
  return ret;
}

}  // extern "C"

namespace pblog_test {

std::string StringPrintf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  int size = vsnprintf(nullptr, 0, format, args);
  va_end(args);

  std::string ret(size + 1, '\0');
  va_start(args, format);
  vsnprintf(&ret[0], size + 1, format, args);
  va_end(args);

  ret.resize(size);
  return ret;
}

}  // namespace pblog_test

#endif
