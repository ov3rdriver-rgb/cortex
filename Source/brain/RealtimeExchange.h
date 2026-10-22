#pragma once

#include <atomic>
#include <memory>

namespace cortex
{
// Hands objects built on other threads to the audio thread without locks or
// frees on the audio thread (BRIEF.md §2):
//   any thread:    publish() / publishClear()
//   audio thread:  acquire() - swaps in whatever was published, retiring the old one
//   message thread: collectGarbage() - deletes what the audio thread retired
// A new object is only swapped in once the previous retiree has been collected.
template <typename T>
class RealtimeExchange
{
public:
    ~RealtimeExchange()
    {
        delete pending.exchange (nullptr);
        delete retired.exchange (nullptr);
        delete active;
    }

    void publish (std::unique_ptr<T> next)
    {
        clearRequested = false;
        delete pending.exchange (next.release());
    }

    void publishClear()
    {
        delete pending.exchange (nullptr);
        clearRequested = true;
    }

    T* acquire() noexcept
    {
        if (retired.load (std::memory_order_acquire) == nullptr)
        {
            if (auto* next = pending.exchange (nullptr, std::memory_order_acq_rel))
            {
                retired.store (active, std::memory_order_release);
                active = next;
            }
            else if (active != nullptr && clearRequested.exchange (false))
            {
                retired.store (active, std::memory_order_release);
                active = nullptr;
            }
        }
        return active;
    }

    void collectGarbage()
    {
        delete retired.exchange (nullptr, std::memory_order_acq_rel);
    }

private:
    std::atomic<T*> pending { nullptr };
    std::atomic<T*> retired { nullptr };
    std::atomic<bool> clearRequested { false };
    T* active = nullptr;   // audio thread only
};
} // namespace cortex
