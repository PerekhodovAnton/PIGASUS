#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <juce_core/juce_core.h>

namespace redline
{
    // SPSC ring buffer of per-frame peaks (input, output, GR in dB).
    // Audio thread calls push() per sample; UI thread reads frames for drawing.
    class PeakHistory
    {
    public:
        static constexpr int kCapacity = 1024;
        struct Frame { float in; float out; float gr; };

        PeakHistory() { reset(); }

        void reset() noexcept
        {
            for (auto& f : data) f = { 0.0f, 0.0f, 0.0f };
            writeIdx.store (0, std::memory_order_relaxed);
            accIn = accOut = accGr = 0.0f;
            accCount = 0;
        }

        void setFrameSamples (int n) noexcept { frameSamples = juce::jmax (1, n); }
        int  getFrameSamples() const noexcept { return frameSamples; }

        // Audio thread
        void push (float inSample, float outSample, float gr) noexcept
        {
            accIn  = juce::jmax (accIn,  std::abs (inSample));
            accOut = juce::jmax (accOut, std::abs (outSample));
            accGr  = juce::jmax (accGr,  gr);

            if (++accCount >= frameSamples)
            {
                const int w = writeIdx.load (std::memory_order_relaxed);
                data[(size_t) w] = { accIn, accOut, accGr };
                writeIdx.store ((w + 1) % kCapacity, std::memory_order_release);
                accIn = accOut = accGr = 0.0f;
                accCount = 0;
            }
        }

        int   getWriteIndex() const noexcept { return writeIdx.load (std::memory_order_acquire); }
        Frame getFrame (int absoluteIdx) const noexcept
        {
            int i = absoluteIdx % kCapacity;
            if (i < 0) i += kCapacity;
            return data[(size_t) i];
        }

    private:
        std::array<Frame, kCapacity> data {};
        std::atomic<int> writeIdx { 0 };
        float accIn { 0.0f }, accOut { 0.0f }, accGr { 0.0f };
        int   accCount { 0 };
        int   frameSamples { 256 };
    };
}
