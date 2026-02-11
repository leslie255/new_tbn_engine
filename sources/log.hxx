#pragma once

#include <concepts>
#include <fmt/base.h>

enum class LogLevel : uint16_t {
    Verbose = 0,
    Info,
    Warn,
    Error,
};

/// Code after this push would have minimum log level `log_level`, until it is popped.
/// Log levels are global per-process.
///
/// Consider using `log_level_scope` if possible.
void push_log_level(LogLevel log_level);

/// Consider using `log_level_scope` if possible.
void pop_log_level();

/// Get the current log level.
LogLevel get_current_log_level();

/// Code inside the scope would have minimum log level `log_level`.
/// Log levels are global per-process.
template <class F>
    requires std::invocable<F>
void log_level_scope(LogLevel log_level, F&& f) {
    push_log_level(log_level);
    f();
    pop_log_level();
}

/// Probably use `with_logging_backend_locked` instead of manually locking/unlocking.
void lock_logging_backend();

/// Probably use `with_logging_backend_locked` instead of manually locking/unlocking.
void unlock_logging_backend();

LogLevel gloabl_log_level();

template <class F>
    requires std::invocable<F>
void with_logging_backend_locked(F&& f) {
    lock_logging_backend();
    f();
    unlock_logging_backend();
}

template <class... T>
    requires(fmt::formattable<T> && ...)
void log_with_level(LogLevel log_level, fmt::format_string<T...> fmt, T&&... args) {
    if (log_level < get_current_log_level())
        return;

    with_logging_backend_locked([&]() {
        FILE* out;
#if defined(__EMSCRIPTEN__)
        out = stdout;
        switch (log_level) {
        case LogLevel::Verbose: fmt::print(out, "[VERBOSE] "); break;
        case LogLevel::Info: fmt::print(out, "[INFO] "); break;
        case LogLevel::Warn: fmt::print(out, "[WARN] "); break;
        case LogLevel::Error: fmt::print(out, "[ERROR] "); break;
        }
#else
        out = stderr;
        switch (log_level) {
        case LogLevel::Verbose: fmt::print(out, "[\e[0;34mVERBOSE\e[0m] "); break;
        case LogLevel::Info: fmt::print(out, "[\e[0;32mINFO\e[0m] "); break;
        case LogLevel::Warn: fmt::print(out, "[\e[0;33mWARN\e[0m] "); break;
        case LogLevel::Error: fmt::print(out, "[\e[0;31mERROR\e[0m] "); break;
        }
#endif
        fmt::println(out, fmt, std::forward<T>(args)...);
    });
}

template <class... T>
    requires(fmt::formattable<T> && ...)
void log_verbose(fmt::format_string<T...> fmt, T&&... args) {
    log_with_level(LogLevel::Verbose, fmt, std::forward<T>(args)...);
}

template <class... T>
    requires(fmt::formattable<T> && ...)
void log_info(fmt::format_string<T...> fmt, T&&... args) {
    log_with_level(LogLevel::Info, fmt, std::forward<T>(args)...);
}

template <class... T>
    requires(fmt::formattable<T> && ...)
void log_warn(fmt::format_string<T...> fmt, T&&... args) {
    log_with_level(LogLevel::Warn, fmt, std::forward<T>(args)...);
}

template <class... T>
    requires(fmt::formattable<T> && ...)
void log_error(fmt::format_string<T...> fmt, T&&... args) {
    log_with_level(LogLevel::Error, fmt, std::forward<T>(args)...);
}
