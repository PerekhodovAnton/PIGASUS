#pragma once

#include <juce_graphics/juce_graphics.h>

namespace redline::theme
{
    // Deep purple — royal velvet / amplifier plush
    inline const juce::Colour bgDeep      { 0xff2a1842 };  // dominant surface
    inline const juce::Colour bgPaper     { 0xff2a1842 };  // alias
    inline const juce::Colour bgPaperWarm { 0xff1b0f2e };  // gauge face
    inline const juce::Colour bgPaperDeep { 0xff0f0820 };  // recessed panel
    inline const juce::Colour bgPaperSoft { 0xff3b225a };  // raised label tag

    inline const juce::Colour bgRoad      { 0xff100820 };  // legacy alias
    inline const juce::Colour bgRoadEdge  { 0xff20143a };

    inline const juce::Colour lineFaint   { 0xff3a1f50 };
    inline const juce::Colour lineMed     { 0xff5a3878 };
    inline const juce::Colour lineStrong  { 0xff8060a6 };

    // Cream type — warm against the cool purple
    inline const juce::Colour inkHigh     { 0xfff3ecd8 };
    inline const juce::Colour inkMid      { 0xffa898c4 };
    inline const juce::Colour inkLow      { 0xff70608a };

    // Default accent — warm gold (works on red where red-on-red wouldn't)
    inline const juce::Colour accentRed   { 0xffe8c04a };
    inline const juce::Colour accentRedHi { 0xffffd860 };

    // Per-mode accents — spread across hue families so each mode reads as
    // visibly different against the racing-red background. Warm to cool:
    //   tube  → orange-amber (warm vintage glow)
    //   tape  → tan / bronze (oxide neutral)
    //   diode → lime yellow (semiconductor zing, complementary kick)
    //   hard  → steel cyan   (cold-hard clipping, max contrast on red)
    inline juce::Colour modeAccent (int modeIdx) noexcept
    {
        switch (modeIdx)
        {
            case 0: return juce::Colour (0xffffa840);
            case 1: return juce::Colour (0xffd4b070);
            case 2: return juce::Colour (0xffd8e848);
            case 3: return juce::Colour (0xff80d0e8);
            default: return accentRed;
        }
    }

    // ---- Font helpers ----------------------------------------------------
    inline juce::Font headline (float size)
    {
        return juce::Font (juce::FontOptions ((float) size, juce::Font::bold))
                    .withExtraKerningFactor (0.06f);
    }

    inline juce::Font caption (float size)
    {
        return juce::Font (juce::FontOptions ((float) size, juce::Font::bold))
                    .withExtraKerningFactor (0.22f);
    }

    inline juce::Font body (float size)
    {
        return juce::Font (juce::FontOptions ((float) size, juce::Font::plain));
    }

    inline juce::Font numeric (float size, bool bold = false)
    {
        const auto monoName = juce::Font::getDefaultMonospacedFontName();
        const auto style = bold ? juce::Font::bold : juce::Font::plain;
        return juce::Font (juce::FontOptions (monoName, (float) size, style));
    }

    // Italic monospace — modern digital-readout look for live values
    // (Porsche-tach style — the digits feel like they're being pushed by speed).
    inline juce::Font digital (float size, bool bold = true)
    {
        const auto monoName = juce::Font::getDefaultMonospacedFontName();
        const auto style = bold
            ? juce::Font::FontStyleFlags (juce::Font::bold | juce::Font::italic)
            : juce::Font::italic;
        return juce::Font (juce::FontOptions (monoName, (float) size, style));
    }

    // Wider-tracked label — for prominent dashboard markings (PRESSURE, etc.)
    inline juce::Font labelWide (float size)
    {
        return juce::Font (juce::FontOptions ((float) size, juce::Font::bold))
                    .withExtraKerningFactor (0.36f);
    }
}
