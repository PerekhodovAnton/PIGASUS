#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace redline
{
    // Vertical IN/OUT/GR meter cluster, calibrated in dB.
    // Caller pushes linear levels for IN and OUT and dB for GR each frame.
    class LevelMeters : public juce::Component,
                        private juce::Timer
    {
    public:
        LevelMeters() { startTimerHz (45); }

        void setLevels (float inLin, float outLin, float grDb) noexcept
        {
            targetIn  = juce::Decibels::gainToDecibels (juce::jmax (1.0e-5f, inLin),  -60.0f);
            targetOut = juce::Decibels::gainToDecibels (juce::jmax (1.0e-5f, outLin), -60.0f);
            targetGr  = juce::jlimit (0.0f, 12.0f, grDb);
        }

        void paint (juce::Graphics& g) override
        {
            const auto area = getLocalBounds().toFloat();

            // Background
            g.setColour (theme::bgInset);
            g.fillRoundedRectangle (area, 8.0f);
            g.setColour (theme::lineSubtle);
            g.drawRoundedRectangle (area.reduced (0.5f), 8.0f, 1.0f);

            // 3 meter columns + 1 dB scale column
            const float colsArea = area.getWidth() - 18.0f;
            const float colW = colsArea / 4.0f;
            const float top  = area.getY() + 18.0f;
            const float bot  = area.getBottom() - 18.0f;
            const float meterH = bot - top;

            // dB scale on the left
            g.setColour (theme::textDim);
            g.setFont (juce::Font (juce::FontOptions (8.5f, juce::Font::bold)));
            for (int dB = 0; dB >= -60; dB -= 6)
            {
                const float t = (float) (-dB) / 60.0f;
                const float yy = top + meterH * t;
                g.setColour (theme::textDim);
                g.drawText (juce::String (dB),
                            (int) area.getX() + 2, (int) yy - 6, 22, 12,
                            juce::Justification::centredRight, false);
                g.setColour (theme::lineSubtle.withAlpha (0.5f));
                g.drawHorizontalLine ((int) yy,
                                       area.getX() + 26.0f, area.getRight() - 8.0f);
            }

            auto drawBar = [&] (float x, float displayDb,
                                 bool isGr,
                                 const juce::String& label)
            {
                juce::Rectangle<float> bar (x, top, colW - 4.0f, meterH);
                g.setColour (theme::bgDeep);
                g.fillRoundedRectangle (bar, 3.0f);

                const float dbVal = juce::jlimit (-60.0f, 0.0f, displayDb);
                const float fillT = isGr ? juce::jlimit (0.0f, 1.0f, displayDb / 12.0f)
                                         : (1.0f - (-dbVal / 60.0f));

                if (fillT > 0.0f)
                {
                    const float fillH = meterH * fillT;
                    juce::Rectangle<float> filled (bar.getX(),
                                                    bar.getBottom() - fillH,
                                                    bar.getWidth(),
                                                    fillH);
                    if (isGr)
                    {
                        juce::ColourGradient grad (theme::accent2, 0, filled.getY(),
                                                    theme::accent4, 0, filled.getBottom(), false);
                        g.setGradientFill (grad);
                    }
                    else
                    {
                        juce::ColourGradient grad (theme::accent1, 0, bar.getY(),
                                                    theme::accent4, 0, bar.getBottom(), false);
                        g.setGradientFill (grad);
                    }
                    g.fillRoundedRectangle (filled, 3.0f);
                }

                g.setColour (theme::lineSubtle);
                g.drawRoundedRectangle (bar.reduced (0.5f), 3.0f, 1.0f);

                g.setColour (theme::textSec);
                g.setFont (theme::label (8.5f));
                g.drawText (label,
                            (int) bar.getX(), (int) area.getY() + 2,
                            (int) bar.getWidth(), 14,
                            juce::Justification::centred, false);
            };

            const float xIn  = area.getX() + 26.0f + (colW - 4.0f) * 0.0f + 6.0f * 0.0f;
            const float xOut = xIn + colW;
            const float xGr  = xOut + colW;

            drawBar (xIn,  currentIn,  false, "IN");
            drawBar (xOut, currentOut, false, "OUT");
            drawBar (xGr,  currentGr,  true,  "GR");

            // Numeric readouts at bottom
            auto drawNum = [&] (float x, float val, bool isGr)
            {
                juce::String s = isGr ? juce::String (val, 1)
                                       : juce::String (juce::jmax (-60.0f, val), 1);
                g.setColour (theme::textPrimary);
                g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
                g.drawText (s,
                            (int) x, (int) bot + 2,
                            (int) colW - 4, 12,
                            juce::Justification::centred, false);
            };

            drawNum (xIn,  currentIn,  false);
            drawNum (xOut, currentOut, false);
            drawNum (xGr,  currentGr,  true);
        }

    private:
        void timerCallback() override
        {
            const float aFast = 0.50f, aSlow = 0.10f;
            auto smooth = [] (float cur, float tgt, float fastA, float slowA)
            {
                const float a = (tgt > cur) ? fastA : slowA;
                return cur + (tgt - cur) * a;
            };
            currentIn  = smooth (currentIn,  targetIn,  aFast, aSlow);
            currentOut = smooth (currentOut, targetOut, aFast, aSlow);
            currentGr  = smooth (currentGr,  targetGr,  aFast, aSlow);
            repaint();
        }

        float targetIn  { -60.0f }, currentIn  { -60.0f };
        float targetOut { -60.0f }, currentOut { -60.0f };
        float targetGr  {   0.0f }, currentGr  {   0.0f };
    };
}
