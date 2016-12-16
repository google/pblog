#ifndef PBLOG_TEST_COMMON_HH
#define PBLOG_TEST_COMMON_HH

#include "common.hh"

#include <cstdarg>
#include <cstdio>
#include <string>

#include <pblog/common.h>

#define DEFAULT_MIN_SEVERITY 2

extern "C" {

int pblog_printf(int severity, const char *format, ...) {
  if (DEFAULT_MIN_SEVERITY > severity) {
    return 0;
  }

  va_list args;
  va_start(args, format);                    // NOLINT
  int ret = vfprintf(stderr, format, args);  // NOLINT
  va_end(args);                              // NOLINT

  return ret;
}

}  // extern "C"

namespace pblog_test {

std::string StringPrintf(const char *format, ...) {  // NOLINT
  va_list args;
  va_start(args, format);                          // NOLINT
  int size = vsnprintf(nullptr, 0, format, args);  // NOLINT
  va_end(args);                                    // NOLINT

  std::string ret(size + 1, '\0');
  va_start(args, format);                      // NOLINT
  vsnprintf(&ret[0], size + 1, format, args);  // NOLINT
  va_end(args);                                // NOLINT

  ret.resize(size);
  return ret;
}

}  // namespace pblog_test

#endif
