//------------------------------------------------------------------------------
// Util.cpp
// Various utility functions and basic types used throughout the library
//
// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#include "slang/util/Util.h"
#include "slang/util/ResourceControl.h"

#include <fmt/core.h>

#if defined(SLANG_USE_MIMALLOC)
#    include <mimalloc.h>
namespace {
void* admittedNew(size_t size) {
    slang::resource::allocation(size);
    return mi_new(size);
}
void* admittedNewNothrow(size_t size) noexcept {
    try { slang::resource::allocation(size); }
    catch (...) { return nullptr; }
    return mi_new_nothrow(size);
}
void* admittedAlignedNew(size_t size, size_t alignment) {
    slang::resource::allocation(size);
    return mi_new_aligned(size, alignment);
}
void* admittedAlignedNewNothrow(size_t size, size_t alignment) noexcept {
    try { slang::resource::allocation(size); }
    catch (...) { return nullptr; }
    return mi_new_aligned_nothrow(size, alignment);
}
}
// Keep the existing backend, zero-size, alignment, new-handler and delete
// contracts. Only the four allocation entry points gain an admission callback.
#    define mi_new admittedNew
#    define mi_new_nothrow admittedNewNothrow
#    define mi_new_aligned admittedAlignedNew
#    define mi_new_aligned_nothrow admittedAlignedNewNothrow
#    include <mimalloc-new-delete.h>
#    undef mi_new
#    undef mi_new_nothrow
#    undef mi_new_aligned
#    undef mi_new_aligned_nothrow
#endif

namespace slang::resource {
thread_local Control* activeControl = nullptr;

void Control::charge(Kind kind, uint64_t amount) {
    Scope callbackScope(nullptr);
    std::lock_guard lock(mutex);
    if (failure)
        std::rethrow_exception(failure);
    try {
        if (callback)
            callback(context, kind, amount);
    }
    catch (...) {
        failure = std::current_exception();
        throw;
    }
}

void Control::check() {
    Scope callbackScope(nullptr);
    std::lock_guard lock(mutex);
    if (failure)
        std::rethrow_exception(failure);
}

bool allocationHookAvailable() noexcept {
#if defined(SLANG_USE_MIMALLOC)
    return true;
#else
    return false;
#endif
}
} // namespace slang::resource

namespace slang::assert {

[[noreturn]] void assertFailed(const char* expr, const std::source_location& location) {
    auto msg = fmt::format("Assertion '{}' failed\n  in file {}, line {}\n"
                           "  function: {}\n",
                           expr, location.file_name(), location.line(), location.function_name());

#if __cpp_exceptions
    throw AssertionException(msg);
#else
    fprintf(stderr, "%s", msg.c_str());
    std::abort();
#endif
}

#if !__cpp_exceptions
[[noreturn]] void handleThrow(const char* msg, const std::source_location& location) {
    fprintf(stderr,
            "internal compiler error: '%s'\n  in file %s, line %d\n"
            "  function: %s\n",
            msg, location.file_name(), location.line(), location.function_name());
    std::abort();
}
#endif

[[noreturn]] void handleUnreachable(const std::source_location& location) {
    auto msg = fmt::format("Supposedly unreachable code was executed\n  in file {}, line {}\n"
                           "  function: {}\n",
                           location.file_name(), location.line(), location.function_name());

#if __cpp_exceptions
    throw std::logic_error(msg);
#else
    fprintf(stderr, "%s", msg.c_str());
    std::abort();
#endif
}

} // namespace slang::assert
