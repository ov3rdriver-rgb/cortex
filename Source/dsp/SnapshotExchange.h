#pragma once

#include <array>
#include <atomic>

namespace cortex
{
// Publishes the latest copy of a small struct from the audio thread to the UI
// without locks or allocation. Double-buffered: the writer fills the buffer the
// reader was not just told about, then flips the index. A reader slow enough to
// still be copying two publishes later can see a mixed frame; for display data
// that only means one slightly wrong repaint, so no further guarding.
template <typename T>
class SnapshotExchange
{
public:
    void publish (const T& value) noexcept   // audio thread
    {
        buffers[(size_t) writeIndex] = value;
        latest.store (writeIndex, std::memory_order_release);
        writeIndex ^= 1;
    }

    bool read (T& out) const noexcept   // UI thread
    {
        const int i = latest.load (std::memory_order_acquire);
        if (i < 0)
            return false;
        out = buffers[(size_t) i];
        return true;
    }

private:
    std::array<T, 2> buffers {};
    std::atomic<int> latest { -1 };
    int writeIndex = 0;
};
} // namespace cortex
