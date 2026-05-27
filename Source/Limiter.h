#pragma once

#include <juce_dsp/juce_dsp.h>

namespace redline
{
    // Simple lookahead brick-wall limiter.
    // - 5 ms lookahead, scans the buffer for the maximum upcoming peak
    // - Smooth attack on the gain reduction, slower release
    // - Hard ceiling at the configured threshold
    class Limiter
    {
    public:
        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            numChannels = (int) spec.numChannels;

            lookaheadSamples = juce::jmax (1, (int) std::round (lookaheadMs * 0.001 * sampleRate));
            const int bufLen = lookaheadSamples + 16;
            for (int ch = 0; ch < (int) numChannels; ++ch)
            {
                delayBuf[ch].assign ((size_t) bufLen, 0.0f);
            }
            writePos = 0;

            attackCoef = std::exp (-1.0f / (float) (sampleRate * (lookaheadMs * 0.001)));
            releaseCoef = std::exp (-1.0f / (float) (sampleRate * (releaseMs * 0.001)));

            envelope = 1.0f;
            grPeak = 0.0f;
        }

        void reset()
        {
            for (auto& b : delayBuf) std::fill (b.begin(), b.end(), 0.0f);
            envelope = 1.0f;
            grPeak = 0.0f;
            writePos = 0;
        }

        void setCeilingDb (float db) { ceilingLin = juce::Decibels::decibelsToGain (db); }

        int getLatencySamples() const noexcept { return lookaheadSamples; }

        // GR in dB for metering (positive = how much we attenuated, average value)
        float getGainReductionDb() const noexcept { return grPeak.load(); }

        void process (juce::dsp::AudioBlock<float>& block)
        {
            const int numSamples = (int) block.getNumSamples();
            const int chans = juce::jmin ((int) block.getNumChannels(), (int) delayBuf.size());
            const int bufLen = (int) delayBuf[0].size();

            float maxGr = 0.0f;

            for (int n = 0; n < numSamples; ++n)
            {
                // Write incoming to delay buffer
                for (int ch = 0; ch < chans; ++ch)
                    delayBuf[ch][(size_t) writePos] = block.getChannelPointer ((size_t) ch)[n];

                // Find max |x| in the lookahead window
                float lookaheadPeak = 0.0f;
                for (int ch = 0; ch < chans; ++ch)
                {
                    int idx = writePos;
                    for (int k = 0; k < lookaheadSamples; ++k)
                    {
                        idx -= 1;
                        if (idx < 0) idx += bufLen;
                        lookaheadPeak = juce::jmax (lookaheadPeak, std::abs (delayBuf[ch][(size_t) idx]));
                    }
                }

                // Required attenuation to keep below ceiling
                float targetGain = 1.0f;
                if (lookaheadPeak > ceilingLin)
                    targetGain = ceilingLin / lookaheadPeak;

                // Smooth: snap down (attack) fast, recover (release) slow.
                if (targetGain < envelope)
                    envelope = targetGain + (envelope - targetGain) * attackCoef;
                else
                    envelope = targetGain + (envelope - targetGain) * releaseCoef;

                // Read delayed sample (oldest one in the buffer) and apply gain
                int readPos = writePos - (lookaheadSamples - 1);
                if (readPos < 0) readPos += bufLen;

                for (int ch = 0; ch < chans; ++ch)
                {
                    const float delayed = delayBuf[ch][(size_t) readPos];
                    const float out = delayed * envelope;
                    block.getChannelPointer ((size_t) ch)[n] = juce::jlimit (-ceilingLin, ceilingLin, out);
                }

                // Track GR for metering
                const float grDb = juce::Decibels::gainToDecibels (envelope, -60.0f);
                if (-grDb > maxGr) maxGr = -grDb;

                writePos = (writePos + 1) % bufLen;
            }

            // Smooth GR meter readout
            const float prev = grPeak.load();
            const float smoothed = 0.85f * prev + 0.15f * maxGr;
            grPeak.store (smoothed);
        }

    private:
        double sampleRate { 44100.0 };
        size_t numChannels { 2 };
        std::array<std::vector<float>, 8> delayBuf;
        int writePos { 0 };
        int lookaheadSamples { 240 };

        float lookaheadMs { 5.0f };
        float releaseMs   { 80.0f };

        float attackCoef  { 0.0f };
        float releaseCoef { 0.0f };

        float envelope { 1.0f };
        float ceilingLin { juce::Decibels::decibelsToGain (-0.1f) };

        std::atomic<float> grPeak { 0.0f };
    };
}
