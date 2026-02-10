#include "log.hxx"
#include "utils.hxx"

#include <mutex>
#include <vector>

static std::mutex log_mutex;

void lock_logging_backend() {
    log_mutex.lock();
}

void unlock_logging_backend() {
    log_mutex.unlock();
}

static std::mutex log_level_stack_mutex;
static std::vector<LogLevel> log_level_stack = {LogLevel::Info};
static std::atomic<LogLevel> current_log_level = LogLevel::Info;

void push_log_level(LogLevel log_level) {
    lock_mutex(log_level_stack_mutex, [&] {
        log_level_stack.push_back(log_level);
        current_log_level.store(log_level, std::memory_order_relaxed);
    });
}

void pop_log_level() {
    lock_mutex(log_level_stack_mutex, [&] {
        if (log_level_stack.size() == 1) {
            return;
        }

        log_level_stack.pop_back();
        current_log_level.store(
            log_level_stack[log_level_stack.size() - 1],
            std::memory_order_relaxed);
    });
}

LogLevel get_current_log_level() {
    return current_log_level.load(std::memory_order_acquire);
}
