#pragma once

#include "PluginProcessor.h"
#include "SpeedometerKnob.h"
#include "WaveformDisplay.h"
#include "SegmentedModeSelector.h"

class RedlineAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit RedlineAudioProcessorEditor (RedlineAudioProcessor&);
    ~RedlineAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawHeader (juce::Graphics&, juce::Rectangle<int>);
    void drawFooter (juce::Graphics&, juce::Rectangle<int>);
    void buildGrain();

    RedlineAudioProcessor& proc;

    redline::WaveformDisplay       waveform;
    redline::SegmentedModeSelector modeSelector;
    redline::SpeedometerKnob       speedo;

    juce::Image grainImg;

    float displayedGrDb { 0.0f };
    float displayedPressure { 0.0f };   // smoothed 0..1, for the footer indicator

    juce::Colour currentAccent { 0xffd9a348 };  // matches default tube
    int          lastMode      { -1 };

    // Shake state — driven by saturation activity + GR
    float shakePhase { 0.0f };
    float shakeAmp   { 0.0f };
    float titleShakeX { 0.0f }, titleShakeY { 0.0f };

    // Race-track scroll
    float roadPhase { 0.0f };

    struct Streak {
        float x, yT;        // x in pixels, yT is normalised 0..1 within asphalt area
        float speed;        // multiplier
        float length;
        float alpha;
        bool  dark;
    };
    std::vector<Streak> streaks;
    void buildStreaks();

    // Flame particle system — erupts around the gauge perimeter under load.
    // Each particle picks a pre-rendered sprite template + frame offset, and
    // cycles through animation frames based on its age. Sprites are generated
    // at startup with noise-driven detail to look like real fire.
    struct FlameParticle {
        float x, y;
        float vx, vy;
        float age;
        float lifetime;
        float baseSize;          // peak width when drawn, in pixels
        int   frameOffset;       // de-syncs particle animation
        int   spriteTemplate;    // which flame template to use
    };
    std::vector<FlameParticle> flames;
    std::vector<juce::Image>   flameFrames;   // [template][frame], flat-indexed
    juce::Random flameRng { 13579 };
    float flameSpawnAcc { 0.0f };
    void drawFlames (juce::Graphics&);
    void buildFlameFrames();

    static constexpr int kFlameFrames    = 12;
    static constexpr int kFlameTemplates = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RedlineAudioProcessorEditor)
};
