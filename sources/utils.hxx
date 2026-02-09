#pragma once

#include <fmt/base.h>
#include <fstream>
#include <source_location>

static inline std::string read_file(std::string_view path) {
  constexpr auto read_size = std::size_t(4096);
  auto stream = std::ifstream(path.data());
  if (!stream.is_open()) {
    fmt::println(stderr, "file does not exist at {}", path);
    __builtin_trap();
  }
  auto out = std::string();
  auto buf = std::string(read_size, '\0');
  while (stream.read(&buf[0], read_size)) {
    out.append(buf, 0, (size_t)stream.gcount());
  }
  out.append(buf, 0, (size_t)stream.gcount());
  return out;
}

[[noreturn]]
static inline void todo(std::source_location source_location = std::source_location::current()) {
  fmt::println(
      "[ERROR] UNIMPLEMENTED @ {}:{}:{}, in function {}",
      source_location.file_name(),
      source_location.line(),
      source_location.column(),
      source_location.function_name());
  std::abort();
}
