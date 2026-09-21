#pragma once

// 自动判断：如果是 Release 模式 (定义了 NDEBUG)，则保留 INFO 及以上；否则保留 DEBUG 及以上
#ifdef NDEBUG
    // 修改 1：编译期保留 INFO 级别
    #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#else
    #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#endif

#include <spdlog/spdlog.h>
#define INITLOG initlog()
#include <spdlog/sinks/stdout_color_sinks.h>

#define LogI SPDLOG_INFO
#define LogD SPDLOG_DEBUG
#define LogE SPDLOG_ERROR

inline void initlog() {
    auto console = spdlog::stdout_color_mt("console");
    spdlog::set_default_logger(console);

#ifdef NDEBUG
    // 修改 2：Release 模式运行期设置，允许输出 info
    console->set_level(spdlog::level::info);

    // flush_on 建议保持 err，如果设为 info 会导致每次打印 info 都刷新缓冲区，影响 Release 性能
    console->flush_on(spdlog::level::err);
#else
    // Debug 模式运行期设置
    console->set_level(spdlog::level::debug);
    console->flush_on(spdlog::level::trace);
#endif

    console->set_pattern("[%H:%M:%S] [%^%l%$] [thread %t] [%@]✅ %v");

    // 现在在 Release 模式下，这句话也能正常打印出来了
    SPDLOG_INFO("Logger 初始化完成!");
}