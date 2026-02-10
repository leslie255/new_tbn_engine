#pragma once

#include "log.hxx"
#include <fstream>
#include <source_location>

template <class F>
auto lock_mutex(std::mutex& mutex, F&& f) -> std::invoke_result_t<F> {
    std::lock_guard<std::mutex> lock(mutex);
    return std::forward<F>(f)();
}

static inline std::string read_file(std::string_view path) {
    constexpr auto read_size = std::size_t(4096);
    auto stream = std::ifstream(path.data());
    if (!stream.is_open()) {
        log_error("file does not exist at {}", path);
        abort();
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
    log_error(
        "UNIMPLEMENTED @ {}:{}:{}, in function {}",
        source_location.file_name(),
        source_location.line(),
        source_location.column(),
        source_location.function_name());
    abort();
}

template <class T>
    requires std::is_floating_point_v<T>
constexpr T degrees_to_radians(T x) {
    return x * 0.01745329252;
}
