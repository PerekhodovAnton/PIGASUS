#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PeakHistory.h"
#include "Theme.h"

namespace redline
{
    // Recessed paper-strip scope. Dark ink output trace, faint input trace
    // behind, one accent red for moments when the limiter actually pulled.
    class WaveformDisplay : public juce::Component,
                            private juce::Timer
    {
    public:
        explicit WaveformDisplay (const PeakHistory& historyRef)
            : history (historyRef)
        {
            setInterceptsMouseClicks (false, false);
            startTimerHz (60);
        }

        void setCeilingDb (float db) noexcept { ceilingDb = db; }
        void setAccent (juce::Colour c) noexcept { if (c != accent) { accent = c; repaint(); } }

        void paint (juce::Graphics& g) override
        {
            const auto area = getLocalBounds().toFloat();
            const float w = area.getWidth();
            const float h = area.getHeight();
            const float midY = area.getCentreY();
            const float halfH = h * 0.42f;

            // Recessed strip of slightly darker paper
            g.setColour (theme::bgPaperDeep);
            g.fillRect (area);
            g.setColour (theme::lineMed);
            g.drawRect (area, 1.0f);

            // Centerline
            g.setColour (theme::lineMed.withAlpha (0.55f));
            g.drawHorizontalLine ((int) midY,
                                   area.getX() + 4.0f, area.getRight() - 4.0f);

            // Ceiling dashes
            const float ceilingLin = juce::Decibels::decibelsToGain (ceilingDb);
            const float ceilingY1 = midY - halfH * juce::jlimit (0.0f, 1.0f, ceilingLin);
            const float ceilingY2 = midY + halfH * juce::jlimit (0.0f, 1.0f, ceilingLin);
            {
                const float dash[2] = { 3.0f, 5.0f };
                auto dashedLine = [&] (float y) {
                    juce::Path p;  p.startNewSubPath (area.getX() + 4.0f, y);
                                   p.lineTo (area.getRight() - 4.0f, y);
                    juce::Path stroked;
                    juce::PathStrokeType (0.8f).createDashedStroke (stroked, p, dash, 2);
                    g.fillPath (stroked);
                };
                g.setColour (theme::lineStrong);
                dashedLine (ceilingY1);
                dashedLine (ceilingY2);
            }

            const int W = (int) std::round (w);
            if (W <= 4) return;

            const int writeIdx = history.getWriteIndex();
            const int N = juce::jmin (W, PeakHistory::kCapacity - 1);
            const float xStep = w / (float) N;

            juce::Path topPath, botPath, limMarks;
            topPath.startNewSubPath (area.getX(), midY);
            botPath.startNewSubPath (area.getX(), midY);

            juce::Path inTop, inBot;
            inTop.startNewSubPath (area.getX(), midY);
            inBot.startNewSubPath (area.getX(), midY);

            for (int i = 0; i < N; ++i)
            {
                const auto f = history.getFrame (writeIdx - N + i);
                const float x = area.getX() + (float) i * xStep;
                const float oH = juce::jlimit (0.0f, 1.25f, f.out) * halfH;
                const float iH = juce::jlimit (0.0f, 1.25f, f.in ) * halfH;

                topPath.lineTo (x, midY - oH);
                botPath.lineTo (x, midY + oH);
                inTop .lineTo (x, midY - iH);
                inBot .lineTo (x, midY + iH);

                if (f.gr > 0.5f)
                    limMarks.addRectangle (x, area.getY() + 1.0f,
                                            juce::jmax (1.0f, xStep), 2.5f);
            }

            topPath.lineTo (area.getRight(), midY); topPath.closeSubPath();
            botPath.lineTo (area.getRight(), midY); botPath.closeSubPath();
            inTop .lineTo (area.getRight(), midY); inTop.closeSubPath();
            inBot .lineTo (area.getRight(), midY); inBot.closeSubPath();

            // Input — very faint ink (just contour cues)
            g.setColour (theme::inkMid.withAlpha (0.18f));
            g.fillPath (inTop);
            g.fillPath (inBot);

            // Output — solid dark ink
            g.setColour (theme::inkHigh.withAlpha (0.90f));
            g.fillPath (topPath);
            g.fillPath (botPath);

            // Limit ticks
            g.setColour (accent);
            g.fillPath (limMarks);
        }

    private:
        void timerCallback() override { repaint(); }

        const PeakHistory& history;
        float ceilingDb { -0.1f };
        juce::Colour accent { theme::accentRed };
    };
}
