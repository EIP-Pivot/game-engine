#pragma once

#include <optional>

#include <cpplogger/Logger.hpp>
#include <cpplogger/utils/source_location.hpp>

#include "pivot/Platform.hxx"
#include "pivot/debug.hxx"

#ifndef NDEBUG
    #define DEBUG_FUNCTION logger.trace(::function_name()) << "Entered";

    #define pivot_assert(expr, msg)                                              \
        {                                                                        \
            if (UNLIKELY(!(expr))) {                                             \
                __pivot_assert_failed(#expr, msg);                               \
                if (pivot::plateform::isDebuggerPresent()) { PLATFORM_BREAK(); } \
                std::abort();                                                    \
            }                                                                    \
        }

    #define __pivot_assert_failed(expr, msg)                                       \
        logger.err(::file_position()) << "Assertion failed: " #expr " :: " << msg; \
        logger.stop();

    #define pivot_check(expr, msg) \
        ((LIKELY(!!(expr))) || __pivot_check_failed(#expr, msg)) || ([]() { if (pivot::plateform::isDebuggerPresent()) { PLATFORM_BREAK(); }}(), false)
    #define __pivot_check_failed(expr, msg) (logger.warn(::file_position()) << #expr ": " << msg, false)

#else
    #define pivot_assert(expr, msg) void(0);
    #define pivot_check(expr, msg) (LIKELY(!!(expr)));
    #define DEBUG_FUNCTION void(0);
#endif

namespace pivot
{

template <typename T>
using Ref = std::reference_wrapper<T>;

template <typename T>
using OptionalRef = std::optional<Ref<T>>;

}    // namespace pivot
