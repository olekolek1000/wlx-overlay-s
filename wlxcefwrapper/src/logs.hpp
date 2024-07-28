#pragma once

#include "fmt/format.h"
#include <string_view>

namespace logs {
void logsPrintHeader(FILE *target);

template <typename... Args> void print(std::string_view format, Args... args) {
  auto text = fmt::format(fmt::runtime(format), args...);
  logsPrintHeader(stderr);
  fwrite(text.data(), 1, text.size(), stderr);
  fputs("\033[0m\n", stderr);
  fflush(stderr);
}

} // namespace logs