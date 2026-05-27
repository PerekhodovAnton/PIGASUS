#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout RedlineAudioProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<P>(
        juce::ParameterID { "drive", 1 },
        "Drive",
        juce::NormalisableRange<float> (0.0f, 12.0f, 0.01f, 0.75f),
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("dB")));

    params.push_back (std::make_unique<C>(
        juce::ParameterID { "mode", 1 },
        "Saturation",
        juce::StringArray { "Tube", "Tape", "Diode", "Hard" },
        0));

    params.push_back (std::make_unique<P>(
        juce::ParameterID { "ceiling", 1 },
        "Ceiling",
        juce::NormalisableRange<float> (-6.0f, 0.0f, 0.01f, 1.0f),
        -0.1f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("dB")));

    return { params.begin(), params.end() };
}

RedlineAudioProcessor::RedlineAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

bool RedlineAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled()) return false;
    if (in != out) return false;
    if (in != juce::AudioChannelSet::mono()
     && in != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void RedlineAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) juce::jmax (getTotalNumInputChannels(),
                                                  getTotalNumOutputChannels());

    saturator.prepare (spec);
    limiter.prepare (spec);

    // ~5 ms per visualization frame → ~6 seconds of scrollback in the ring
    peakHistory.setFrameSamples (juce::jmax (32, (int) std::round (sampleRate * 0.005)));
    peakHistory.reset();

    inputPeakScratch.assign ((size_t) samplesPerBlock, 0.0f);
    inRmsDisplay.store (0.0f);
    outRmsDisplay.store (0.0f);

    setLatencySamples (saturator.getLatencySamples() + limiter.getLatencySamples());
}

void RedlineAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Pull params
    const float driveDb = apvts.getRawParameterValue ("drive")->load();
    const int   modeIdx = (int) apvts.getRawParameterValue ("mode")->load();
    const float ceiling = apvts.getRawParameterValue ("ceiling")->load();

    saturator.setDriveDb (driveDb);
    saturator.setMode (static_cast<redline::SatMode> (juce::jlimit (0, 3, modeIdx)));
    limiter.setCeilingDb (ceiling);

    const int numSamples = buffer.getNumSamples();
    const int chs = juce::jmax (totalNumInputChannels, totalNumOutputChannels);

    // Capture input peak per sample BEFORE processing
    if ((int) inputPeakScratch.size() < numSamples)
        inputPeakScratch.assign ((size_t) numSamples, 0.0f);

    double inSumSq = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        float pk = 0.0f;
        for (int ch = 0; ch < chs; ++ch)
            pk = juce::jmax (pk, std::abs (buffer.getSample (ch, i)));
        inputPeakScratch[(size_t) i] = pk;
        inSumSq += (double) pk * (double) pk;
    }

    juce::dsp::AudioBlock<float> block (buffer);
    saturator.process (block);
    limiter.process (block);

    // Now push input/output peaks plus GR into the visualization ring
    const float grNow = limiter.getGainReductionDb();
    double outSumSq = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        float outPk = 0.0f;
        for (int ch = 0; ch < chs; ++ch)
            outPk = juce::jmax (outPk, std::abs (buffer.getSample (ch, i)));
        outSumSq += (double) outPk * (double) outPk;
        peakHistory.push (inputPeakScratch[(size_t) i], outPk, grNow);
    }

    // RMS-ish levels for the side meters (linear)
    if (numSamples > 0)
    {
        const float inR  = (float) std::sqrt (inSumSq  / (double) numSamples);
        const float outR = (float) std::sqrt (outSumSq / (double) numSamples);
        // Smooth toward new value so the meter doesn't twitch
        const float prevIn  = inRmsDisplay.load();
        const float prevOut = outRmsDisplay.load();
        const float a = 0.35f;
        inRmsDisplay.store  (prevIn  + (inR  - prevIn ) * a);
        outRmsDisplay.store (prevOut + (outR - prevOut) * a);
    }
}

juce::AudioProcessorEditor* RedlineAudioProcessor::createEditor()
{
    return new RedlineAudioProcessorEditor (*this);
}

void RedlineAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RedlineAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// JUCE plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RedlineAudioProcessor();
}
