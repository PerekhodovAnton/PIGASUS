#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Saturator.h"
#include "Limiter.h"
#include "PeakHistory.h"

class RedlineAudioProcessor : public juce::AudioProcessor
{
public:
    RedlineAudioProcessor();
    ~RedlineAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Public APVTS so the editor can attach to params
    juce::AudioProcessorValueTreeState apvts;

    // Live readouts for the GUI animation
    float getSatActivity() const { return saturator.getActivity(); }
    float getGainReductionDb() const { return limiter.getGainReductionDb(); }
    float getDriveDb() const { return *apvts.getRawParameterValue ("drive"); }
    int   getMode()    const { return (int) *apvts.getRawParameterValue ("mode"); }
    float getCeilingDb() const { return *apvts.getRawParameterValue ("ceiling"); }

    const redline::PeakHistory& getPeakHistory() const { return peakHistory; }
    float getInRms()  const noexcept { return inRmsDisplay.load(); }
    float getOutRms() const noexcept { return outRmsDisplay.load(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    redline::Saturator   saturator;
    redline::Limiter     limiter;
    redline::PeakHistory peakHistory;

    std::vector<float>   inputPeakScratch;
    std::atomic<float>   inRmsDisplay { 0.0f };
    std::atomic<float>   outRmsDisplay { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RedlineAudioProcessor)
};
