#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace redline
{
    // Mode selector styled as a row of car pedals. Each pedal is a metallic
    // pad with a grip pattern. Active pedal "presses" down with a glow underneath.
    class SegmentedModeSelector : public juce::Component,
                                   private juce::Timer
    {
    public:
        SegmentedModeSelector (juce::AudioProcessorValueTreeState& s,
                                const juce::String& paramId,
                                juce::StringArray segmentLabels)
            : state (s), paramID (paramId), labels (std::move (segmentLabels))
        {
            param = state.getParameter (paramID);
            jassert (param != nullptr);

            const int idx = (int) std::round (param->convertFrom0to1 (param->getValue()));
            currentIdx = idx;
            targetIdx  = idx;
            for (int i = 0; i < kMaxPedals; ++i)
                presses[i] = (i == idx) ? 1.0f : 0.0f;

            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            startTimerHz (60);
        }

        void setAccent (juce::Colour c) noexcept
        {
            if (c != accent) { accent = c; repaint(); }
        }

        void setSelectedIndex (int idx)
        {
            const int n = labels.size();
            if (n == 0) return;
            idx = juce::jlimit (0, n - 1, idx);
            if (idx != targetIdx)
            {
                targetIdx = idx;
                currentIdx = idx;
            }
        }

        void paint (juce::Graphics& g) override
        {
            const int n = labels.size();
            if (n == 0) return;

            const auto area = getLocalBounds().toFloat();
            const float segW = area.getWidth() / (float) n;

            // Reserve label band beneath the pads
            const float labelH = 16.0f;
            const float padArea = area.getHeight() - labelH;

            for (int i = 0; i < n; ++i)
            {
                auto seg = area.withX (area.getX() + i * segW).withWidth (segW);
                auto padRect = seg.removeFromTop (padArea);
                drawPedal (g, padRect, seg, labels[i], i);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int n = labels.size();
            if (n == 0 || param == nullptr) return;
            const float segW = (float) getWidth() / (float) n;
            int idx = juce::jlimit (0, n - 1, (int) std::floor (e.position.x / segW));

            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) idx));
            param->endChangeGesture();
            targetIdx = idx;
            currentIdx = idx;
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            const int n = labels.size();
            if (n == 0) return;
            const float segW = (float) getWidth() / (float) n;
            int idx = juce::jlimit (0, n - 1, (int) std::floor (e.position.x / segW));
            if (idx != hover) { hover = idx; repaint(); }
        }

        void mouseExit (const juce::MouseEvent&) override
        {
            if (hover != -1) { hover = -1; repaint(); }
        }

    private:
        static constexpr int kMaxPedals = 8;

        void drawPedal (juce::Graphics& g,
                         juce::Rectangle<float> padArea,
                         juce::Rectangle<float> labelArea,
                         const juce::String& label,
                         int idx)
        {
            const float press = presses[idx];
            const bool isActive = (idx == targetIdx);
            const bool isHover  = (idx == hover);

            // Pedal pad — slightly inset from segment edges
            auto pad = padArea.reduced (10.0f, 4.0f);
            pad.translate (0.0f, press * 4.0f);  // press-down displacement

            const float corner = 7.0f;

            // 1. Glow under active pedal
            if (press > 0.02f)
            {
                const float a = press;
                g.setColour (accent.withAlpha (0.18f * a));
                g.fillRoundedRectangle (pad.expanded (8.0f, 6.0f), corner + 5.0f);
                g.setColour (accent.withAlpha (0.30f * a));
                g.fillRoundedRectangle (pad.expanded (3.0f, 2.0f), corner + 2.0f);
            }

            // 2. Pedal body — metallic gradient
            {
                const auto topCol    = isActive ? juce::Colour (0xff62656c)
                                                : juce::Colour (0xff4a4d54);
                const auto bottomCol = isActive ? juce::Colour (0xff202329)
                                                : juce::Colour (0xff15171c);
                juce::ColourGradient grad (topCol,    pad.getCentreX(), pad.getY(),
                                            bottomCol, pad.getCentreX(), pad.getBottom(),
                                            false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (pad, corner);
            }

            // 3. Pedal border
            g.setColour (juce::Colour (0xff080a10));
            g.drawRoundedRectangle (pad, corner, 1.2f);

            // 4. Top edge highlight (subtle reflection)
            {
                g.setColour (juce::Colour (0xffaeb4be).withAlpha (isHover ? 0.35f : 0.22f));
                const float hlY = pad.getY() + 2.0f;
                g.drawLine (pad.getX() + corner, hlY,
                            pad.getRight() - corner, hlY, 0.9f);
            }

            // 5. Grip pattern — horizontal bars
            {
                g.setColour (juce::Colour (0xff080a10).withAlpha (0.55f));
                const int bars = 4;
                const float startY = pad.getY() + pad.getHeight() * 0.28f;
                const float spacing = pad.getHeight() * 0.13f;
                for (int b = 0; b < bars; ++b)
                {
                    const float yy = startY + b * spacing;
                    g.drawLine (pad.getX() + 6.0f, yy,
                                pad.getRight() - 6.0f, yy, 1.0f);
                }

                // Subtle highlight above each groove
                g.setColour (juce::Colour (0xffaeb4be).withAlpha (0.10f));
                for (int b = 0; b < bars; ++b)
                {
                    const float yy = startY + b * spacing - 1.5f;
                    g.drawLine (pad.getX() + 6.0f, yy,
                                pad.getRight() - 6.0f, yy, 0.6f);
                }
            }

            // 6. Accent dot — small mark in the upper corner of an active pedal
            if (isActive)
            {
                const float dotR = 2.5f;
                g.setColour (accent);
                g.fillEllipse (pad.getX() + 6.0f, pad.getY() + 6.0f,
                                dotR * 2.0f, dotR * 2.0f);
            }

            // 7. Label below pedal
            {
                const auto col = isActive ? theme::inkHigh
                                          : theme::inkMid.withAlpha (isHover ? 0.95f : 0.7f);
                g.setColour (col);
                g.setFont (theme::caption (10.0f));
                g.drawText (label.toLowerCase(),
                            labelArea.toNearestInt(),
                            juce::Justification::centred, false);

                if (isActive)
                {
                    const float underlineW = 14.0f;
                    g.setColour (accent.withAlpha (0.9f));
                    g.fillRect (juce::Rectangle<float> (
                        labelArea.getCentreX() - underlineW * 0.5f,
                        labelArea.getBottom() - 3.0f,
                        underlineW, 1.4f));
                }
            }
        }

        void timerCallback() override
        {
            bool anyMoving = false;
            for (int i = 0; i < (int) labels.size(); ++i)
            {
                const float target = (i == targetIdx) ? 1.0f : 0.0f;
                const float next   = presses[i] + (target - presses[i]) * 0.30f;
                if (std::abs (next - presses[i]) > 0.001f) anyMoving = true;
                presses[i] = next;
            }
            if (anyMoving) repaint();
        }

        juce::AudioProcessorValueTreeState& state;
        juce::String paramID;
        juce::RangedAudioParameter* param { nullptr };
        juce::StringArray labels;

        int   currentIdx { 0 };
        int   targetIdx  { 0 };
        int   hover      { -1 };
        float presses[kMaxPedals] {};
        juce::Colour accent { theme::accentRed };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedModeSelector)
    };
}
