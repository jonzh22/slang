// Local FTL integration: synchronous invocation resources, never retained by ASTs.
#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <mutex>
#include "slang/slang_export.h"

namespace slang::resource {

enum class Kind { Work, AllocationBytes };

/// The callback is serialized across parser workers. It must not reenter this
/// controller. Its first exception is retained until the invocation ends.
class SLANG_EXPORT Control {
public:
    using Callback = void (*)(void*, Kind, uint64_t);
    Control(Callback callback, void* context) noexcept : callback(callback), context(context) {}
    Control(const Control&) = delete;
    Control& operator=(const Control&) = delete;
    void charge(Kind kind, uint64_t amount);
    void check();
private:
    Callback callback;
    void* context;
    std::mutex mutex;
    std::exception_ptr failure;
};

SLANG_EXPORT extern thread_local Control* activeControl;
SLANG_EXPORT bool allocationHookAvailable() noexcept;

/// Borrowed synchronous scope. A parent must join its workers before destruction.
/// Callbacks and exception cleanup run outside admission to avoid recursive
/// accounting and throwing from cleanup destructors.
class Scope {
public:
    explicit Scope(Control* control) noexcept : previous(activeControl) { activeControl = control; }
    ~Scope() { activeControl = previous; }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
private:
    Control* previous;
};

inline void charge(Kind kind, uint64_t amount) {
    if (activeControl && std::uncaught_exceptions() == 0)
        activeControl->charge(kind, amount);
}
inline void work(uint64_t amount = 1) { charge(Kind::Work, amount); }
inline void allocation(size_t bytes) { charge(Kind::AllocationBytes, bytes); }

} // namespace slang::resource
