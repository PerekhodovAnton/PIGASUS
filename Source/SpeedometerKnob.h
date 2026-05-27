#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace redline
{
    // Paper-printed instrument. The face is slightly warmer paper than the
    // page so the dial outline is just barely visible. All markings are ink.
    // One red — for the redline-zone band and the needle tip.
    class SpeedometerKnob : public juce::Component,
                            private juce::Timer,
                            private juce::ValueTree::Listener
    {
    public:
        SpeedometerKnob (juce::AudioProcessorValueTreeState& s, const juce::String& paramId)
            : state (s), paramID (paramId)
        {
            param = state.getParameter (paramID);
            jassert (param != nullptr);

            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            state.state.addListener (this);
            startTimerHz (45);
        }

        ~SpeedometerKnob() override
        {
            state.state.removeListener (this);
        }

        void setRange (float minV, float maxV) { rangeMin = minV; rangeMax = maxV; }
        void setActivity (float a) noexcept { targetActivity = juce::jlimit (0.0f, 2.0f, a); }
        void setAccent (juce::Colour c) noexcept { if (c != accent) { accent = c; repaint(); } }

        // Rock-pig mascot images (4 states: calm → grumpy → angry → rage)
        void setMoodImages (juce::Image calm, juce::Image grumpy,
                             juce::Image angry, juce::Image rage)
        {
            moodImages[0] = std::move (calm);
            moodImages[1] = std::move (grumpy);
            moodImages[2] = std::move (angry);
            moodImages[3] = std::move (rage);
        }

        // mood = 0..3 continuous. The gauge snaps between integer states
        // with hysteresis so the pig is always one clear expression — no
        // mushy in-between. Each state change triggers a punch animation.
        void setMood (float m) noexcept
        {
            targetMood = juce::jlimit (0.0f, 3.0f, m);

            // 0.55 hysteresis — must move more than halfway past the next
            // integer before the state flips. Prevents chatter near a threshold.
            const float cur = (float) targetStateIdx;
            if (targetMood - cur > 0.55f && targetStateIdx < 3)
            {
                ++targetStateIdx;
                punchT = 0.0f;
                punchSign = (lastPunchSign > 0.0f) ? -1.0f : 1.0f;
                lastPunchSign = punchSign;
            }
            else if (cur - targetMood > 0.55f && targetStateIdx > 0)
            {
                --targetStateIdx;
                punchT = 0.0f;
                punchSign = (lastPunchSign > 0.0f) ? -1.0f : 1.0f;
                lastPunchSign = punchSign;
            }
        }

        void paint (juce::Graphics& g) override
        {
            const auto bounds = getLocalBounds().toFloat();
            const float cx = bounds.getCentreX();
            const float cy = bounds.getCentreY() + bounds.getHeight() * 0.04f;
            const float r  = juce::jmin (bounds.getWidth() * 0.5f,
                                          bounds.getHeight() * 0.55f) * 0.94f;

            const float startA = juce::degreesToRadians (-135.0f);
            const float endA   = juce::degreesToRadians ( 135.0f);

            const float v = param ? param->getValue() : 0.0f;
            const float displayVal = juce::jlimit (0.0f, 1.0f, v);

            const float baseAngle  = startA + displayVal * (endA - startA);
            const float wobble     = juce::degreesToRadians (3.0f)
                                     * (currentActivity * (0.20f + displayVal * 0.55f));
            const float needleAngle = baseAngle + wobble;

            // -------- Face: a very subtle warmer-paper disc --------
            {
                g.setColour (theme::bgPaperWarm);
                g.fillEllipse (cx - r * 1.0f, cy - r * 1.0f, r * 2.0f, r * 2.0f);

                // A single thin printed boundary
                g.setColour (theme::lineMed);
                g.drawEllipse (cx - r * 1.0f, cy - r * 1.0f, r * 2.0f, r * 2.0f, 1.0f);
            }

            // -------- Redline zone (outer band, the only red on the face) --------
            const float redlineStart = 0.78f;
            {
                const float a0 = startA + redlineStart * (endA - startA);
                const float a1 = endA;
                const float bandR = r * 0.92f;
                const float bandThick = r * 0.050f;

                juce::Path band;
                band.addCentredArc (cx, cy, bandR, bandR, 0.0f, a0, a1, true);
                g.setColour (accent);
                g.strokePath (band, juce::PathStrokeType (bandThick,
                                                           juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::butt));
            }

            // -------- Tick marks --------
            {
                const int totalMinors = 24;                  // half-dB resolution across 0–12
                for (int i = 0; i <= totalMinors; ++i)
                {
                    const float t = (float) i / (float) totalMinors;
                    const bool isMajor = (i % 4) == 0;        // major every 2 dB
                    const bool inRedline = t >= redlineStart;
                    const float a = startA + t * (endA - startA);

                    const float rOuter = r * 0.85f;
                    const float rInner = isMajor ? r * 0.74f : r * 0.80f;
                    const float sx = cx + std::sin (a) * rInner;
                    const float sy = cy - std::cos (a) * rInner;
                    const float ex = cx + std::sin (a) * rOuter;
                    const float ey = cy - std::cos (a) * rOuter;

                    const auto col = inRedline ? accent : theme::inkHigh;
                    g.setColour (col.withAlpha (isMajor ? 1.0f : 0.45f));
                    g.drawLine (sx, sy, ex, ey, isMajor ? 1.6f : 0.85f);
                }
            }

            // -------- Number labels --------
            {
                g.setFont (theme::numeric (r * 0.11f, true));
                const int majors = 7;
                for (int i = 0; i < majors; ++i)
                {
                    const float t = (float) i / (float) (majors - 1);
                    const float a = startA + t * (endA - startA);
                    const float lblR = r * 0.62f;
                    const float lx = cx + std::sin (a) * lblR;
                    const float ly = cy - std::cos (a) * lblR;
                    const int valDb = (int) std::round (rangeMin + t * (rangeMax - rangeMin));

                    g.setColour (t >= redlineStart ? accent : theme::inkHigh);
                    g.drawText (juce::String (valDb),
                                (int) lx - 14, (int) ly - 8, 28, 16,
                                juce::Justification::centred, false);
                }
            }

            // -------- Inset digital readout — small printed strip below the pig --------
            {
                const float boxW = r * 0.50f;
                const float boxH = r * 0.18f;
                const float boxX = cx - boxW * 0.5f;
                const float boxY = cy + r * 0.62f;

                g.setColour (theme::bgPaperSoft);
                g.fillRoundedRectangle (boxX, boxY, boxW, boxH, 2.0f);
                g.setColour (theme::lineMed);
                g.drawRoundedRectangle (boxX, boxY, boxW, boxH, 2.0f, 1.0f);

                const auto dbStr = juce::String (rangeMin + displayVal * (rangeMax - rangeMin), 1);
                g.setColour (displayVal >= redlineStart ? accent : theme::inkHigh);
                g.setFont (theme::digital (boxH * 0.65f, true));
                g.drawText (dbStr + " dB",
                            juce::Rectangle<float> (boxX, boxY + 1.0f, boxW, boxH * 0.95f).toNearestInt(),
                            juce::Justification::centred, false);
            }

            // -------- Needle: ink shaft, red tip --------
            {
                const float nLen     = r * 0.78f;
                const float backLen  = r * 0.10f;
                const float baseW    = r * 0.022f;
                const float tipW     = r * 0.004f;

                const float tx = cx + std::sin (needleAngle) * nLen;
                const float ty = cy - std::cos (needleAngle) * nLen;
                const float bx = cx - std::sin (needleAngle) * backLen;
                const float by = cy + std::cos (needleAngle) * backLen;
                const float px = std::cos (needleAngle);
                const float py = std::sin (needleAngle);

                // Soft printed shadow beneath
                juce::Path shadow;
                shadow.startNewSubPath (bx - px * baseW + 0.8f, by - py * baseW + 0.8f);
                shadow.lineTo          (bx + px * baseW + 0.8f, by + py * baseW + 0.8f);
                shadow.lineTo          (tx + px * tipW  + 0.8f, ty + py * tipW  + 0.8f);
                shadow.lineTo          (tx - px * tipW  + 0.8f, ty - py * tipW  + 0.8f);
                shadow.closeSubPath();
                g.setColour (juce::Colour::fromRGBA (0, 0, 0, 50));
                g.fillPath (shadow);

                juce::Path needle;
                needle.startNewSubPath (bx - px * baseW, by - py * baseW);
                needle.lineTo          (bx + px * baseW, by + py * baseW);
                needle.lineTo          (tx + px * tipW,  ty + py * tipW);
                needle.lineTo          (tx - px * tipW,  ty - py * tipW);
                needle.closeSubPath();
                g.setColour (theme::inkHigh);
                g.fillPath (needle);

                // Red tip
                const float tipStartT = 0.82f;
                const float sxTip = bx + (tx - bx) * tipStartT;
                const float syTip = by + (ty - by) * tipStartT;
                const float baseAtTip = baseW + (tipW - baseW) * tipStartT;
                juce::Path tipPath;
                tipPath.startNewSubPath (sxTip - px * baseAtTip, syTip - py * baseAtTip);
                tipPath.lineTo          (sxTip + px * baseAtTip, syTip + py * baseAtTip);
                tipPath.lineTo          (tx    + px * tipW,      ty    + py * tipW);
                tipPath.lineTo          (tx    - px * tipW,      ty    - py * tipW);
                tipPath.closeSubPath();
                g.setColour (accent);
                g.fillPath (tipPath);

                // Central hub — small printed ink dot
                const float hubR = r * 0.045f;
                g.setColour (theme::inkHigh);
                g.fillEllipse (cx - hubR, cy - hubR, hubR * 2.0f, hubR * 2.0f);
            }

            // -------- Rock-pig mascot — snap state change with scale punch --------
            if (moodImages[(size_t) targetStateIdx].isValid())
            {
                // Rage gets a baseline scale boost — the source image has
                // steam clouds extending past the ears, which would shrink the
                // head if all states used the same display box.
                const float stateScale[4] = { 1.00f, 1.00f, 1.00f, 1.26f };

                // Punch envelope — sine-based ease-out so the decay is smooth
                // through both the high-velocity start and the soft tail.
                const float invT       = 1.0f - punchT;
                const float punchEase  = std::sin (invT * juce::MathConstants<float>::halfPi);
                const float punchScale = 1.0f + punchEase * 0.22f;

                // Brief tilt (alternates direction each transition) — rock-pig head-jerk
                const float tilt = juce::degreesToRadians (5.0f)
                                    * punchEase * punchSign;

                const float baseW = r * 1.18f;
                const float baseH = baseW * 0.86f;
                const float pigCx = cx;
                const float pigCy = cy - r * 0.10f;

                const float finalScale = punchScale * stateScale[targetStateIdx];
                const float pigW = baseW * finalScale;
                const float pigH = baseH * finalScale;

                // Accent flash glow behind the pig — radial wash that decays
                // along with the punch.
                if (punchEase > 0.02f)
                {
                    const float flashAlpha = punchEase * 0.34f;
                    juce::ColourGradient flash (accent.withAlpha (flashAlpha), pigCx, pigCy,
                                                 accent.withAlpha (0.0f),
                                                 pigCx + pigW * 0.55f, pigCy, true);
                    flash.addColour (0.45, accent.withAlpha (flashAlpha * 0.55f));
                    g.setGradientFill (flash);
                    g.fillEllipse (pigCx - pigW * 0.60f, pigCy - pigH * 0.60f,
                                    pigW * 1.20f, pigH * 1.20f);
                }

                {
                    juce::Graphics::ScopedSaveState ss (g);
                    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
                    g.addTransform (juce::AffineTransform::rotation (tilt, pigCx, pigCy));
                    g.drawImage (moodImages[(size_t) targetStateIdx],
                                  juce::Rectangle<float> (pigCx - pigW * 0.5f,
                                                            pigCy - pigH * 0.5f,
                                                            pigW, pigH),
                                  juce::RectanglePlacement::centred, false);
                }
            }

        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            dragStartY = e.position.y;
            dragStartValue = param ? param->getValue() : 0.0f;
            if (param) param->beginChangeGesture();
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (!param) return;
            const float dy = dragStartY - e.position.y;
            const float fine = e.mods.isShiftDown() ? 4.0f : 1.0f;
            float v = juce::jlimit (0.0f, 1.0f, dragStartValue + dy / (240.0f * fine));
            param->setValueNotifyingHost (v);
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (param) param->endChangeGesture();
        }

        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            if (!param) return;
            param->beginChangeGesture();
            param->setValueNotifyingHost (0.0f);
            param->endChangeGesture();
        }

        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
        {
            if (!param) return;
            float v = juce::jlimit (0.0f, 1.0f, param->getValue() + w.deltaY * 0.05f);
            param->beginChangeGesture();
            param->setValueNotifyingHost (v);
            param->endChangeGesture();
        }

    private:
        void timerCallback() override
        {
            currentActivity += (targetActivity - currentActivity) * 0.18f;

            // Real-time advance so the punch always settles in the same wall-clock
            // time regardless of timer drift / dropped frames — kills choppy
            // steps you'd otherwise see at uneven frame intervals.
            const double now = juce::Time::getMillisecondCounterHiRes();
            const double dt  = (lastFrameTime > 0.0)
                                 ? juce::jlimit (0.0, 0.10, (now - lastFrameTime) * 0.001)
                                 : 1.0 / 60.0;
            lastFrameTime = now;

            constexpr float punchDurationSec = 0.42f;
            if (punchT < 1.0f)
                punchT = juce::jmin (1.0f, punchT + (float) dt / punchDurationSec);

            repaint();
        }

        void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override
        {
            repaint();
        }

        juce::AudioProcessorValueTreeState& state;
        juce::String paramID;
        juce::RangedAudioParameter* param { nullptr };

        float rangeMin { 0.0f }, rangeMax { 12.0f };
        float dragStartY { 0.0f }, dragStartValue { 0.0f };

        float currentActivity { 0.0f };
        float targetActivity  { 0.0f };

        // Rock-pig mood — 0=calm, 1=grumpy, 2=angry, 3=rage
        std::array<juce::Image, 4> moodImages;
        float targetMood     { 0.0f };
        int   targetStateIdx { 0 };
        float punchT         { 1.0f };  // 0 = just triggered, 1 = settled
        float punchSign      { 1.0f };  // alternates each transition for variety
        float lastPunchSign  { 1.0f };
        double lastFrameTime { 0.0 };   // for real-time animation advance

        juce::Colour accent { theme::accentRed };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpeedometerKnob)
    };
}
