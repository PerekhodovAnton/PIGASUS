#pragma once

#include <juce_dsp/juce_dsp.h>

namespace redline
{
    enum class SatMode { Tube = 0, Tape = 1, Diode = 2, Hard = 3 };

    // Multi-mode waveshapers. All return values roughly in [-1, 1] for typical inputs.
    struct Shapers
    {
        static inline float tube (float x) noexcept
        {
            // Asymmetric soft clip — emphasises even harmonics.
            const float bias = 0.18f;
            const float y = std::tanh (x + bias) - std::tanh (bias);
            return y;
        }

        static inline float tape (float x) noexcept
        {
            // Symmetric soft saturation with gentle knee.
            return std::tanh (x);
        }

        static inline float diode (float x) noexcept
        {
            // Half-wave-leaning asymmetric softclip — buzzy odd+even mix.
            if (x >= 0.0f)
                return std::tanh (1.4f * x);
            return 0.85f * std::tanh (0.7f * x);
        }

        static inline float hard (float x) noexcept
        {
            // Smooth hard clip — fast onset, lots of upper harmonics.
            const float t = 0.9f;
            if (x >  t) return  t + (1.0f - t) * std::tanh ((x - t) / (1.0f - t));
            if (x < -t) return -t + (1.0f - t) * std::tanh ((x + t) / (1.0f - t));
            return x;
        }

        static inline float shape (SatMode m, float x) noexcept
        {
            switch (m)
            {
                case SatMode::Tube:  return tube  (x);
                case SatMode::Tape:  return tape  (x);
                case SatMode::Diode: return diode (x);
                case SatMode::Hard:  return hard  (x);
            }
            return x;
        }
    };

    // Oversampled multi-mode saturator with auto-gain compensation so that
    // increasing Drive raises perceived loudness only via harmonics, not raw level.
    class Saturator
    {
    public:
        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            numChannels = (int) spec.numChannels;

            oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
                numChannels,
                2, // factor 2^2 = 4x
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                true);
            oversampler->initProcessing ((size_t) spec.maximumBlockSize);
            oversampler->reset();

            // Smooth the drive value to prevent zipper noise.
            driveSmoothed.reset (spec.sampleRate, 0.02);
            driveSmoothed.setCurrentAndTargetValue (driveLinear);

            // Activity envelope used by the GUI's speedometer animation.
            envCoeffAtk = std::exp (-1.0f / (float) (spec.sampleRate * 0.005));
            envCoeffRel = std::exp (-1.0f / (float) (spec.sampleRate * 0.150));
            activity = 0.0f;
        }

        void reset()
        {
            if (oversampler) oversampler->reset();
            activity = 0.0f;
        }

        void setDriveDb (float db)
        {
            driveDb = db;
            driveLinear = juce::Decibels::decibelsToGain (db);
            driveSmoothed.setTargetValue (driveLinear);
        }

        void setMode (SatMode m) { mode = m; }

        int getLatencySamples() const
        {
            return oversampler ? (int) oversampler->getLatencyInSamples() : 0;
        }

        // Returns the recent saturation activity envelope (0..1+) for the GUI.
        float getActivity() const noexcept { return activity.load(); }

        void process (juce::dsp::AudioBlock<float>& block)
        {
            const auto numSamples  = (int) block.getNumSamples();
            const auto numCh       = (int) block.getNumChannels();

            // Up-sample
            auto osBlock = oversampler->processSamplesUp (block);
            const auto osSamples = (int) osBlock.getNumSamples();

            float maxLevel = 0.0f;

            for (int n = 0; n < osSamples; ++n)
            {
                // Smoothed drive — we advance once per base sample, so step by osFactor.
                const int baseIdx = juce::jmin (numSamples - 1, n / (osSamples / juce::jmax (1, numSamples)));
                juce::ignoreUnused (baseIdx);

                const float drive = driveSmoothed.getNextValue();
                // Auto-makeup: full saturating signal is rescaled so peak stays near unity.
                // makeup tracks 1/drive^p, where p depends on shaper aggressiveness.
                const float p = (mode == SatMode::Hard)  ? 1.00f
                              : (mode == SatMode::Diode) ? 0.85f
                              : (mode == SatMode::Tape)  ? 0.80f
                                                         : 0.78f; // Tube
                const float makeup = 1.0f / std::pow (juce::jmax (1.0e-6f, drive), p);

                for (int ch = 0; ch < numCh; ++ch)
                {
                    auto* data = osBlock.getChannelPointer ((size_t) ch);
                    const float in  = data[n];
                    const float pre = in * drive;
                    const float shaped = Shapers::shape (mode, pre);
                    const float out = shaped * makeup;
                    data[n] = out;

                    maxLevel = juce::jmax (maxLevel, std::abs (shaped));
                }
            }

            // Down-sample back to the original block
            oversampler->processSamplesDown (block);

            // Update activity envelope (smoothed measure of how hard we're cooking)
            const float target = juce::jmin (1.5f, maxLevel);
            float a = activity.load();
            for (int i = 0; i < numSamples; ++i)
            {
                const float c = (target > a) ? envCoeffAtk : envCoeffRel;
                a = target + (a - target) * c;
            }
            activity.store (a);
        }

    private:
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
        juce::LinearSmoothedValue<float> driveSmoothed { 1.0f };

        SatMode mode { SatMode::Tube };
        float driveDb { 0.0f };
        float driveLinear { 1.0f };

        double sampleRate { 44100.0 };
        int numChannels { 2 };

        float envCoeffAtk { 0.0f }, envCoeffRel { 0.0f };
        std::atomic<float> activity { 0.0f };
    };
}
