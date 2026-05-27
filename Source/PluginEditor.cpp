#include "PluginEditor.h"
#include "Theme.h"
#include "BinaryData.h"

namespace
{
    // Load + downscale a PNG from embedded binary data, sized so the gauge
    // can draw the pig face crisply without holding a ~13 MB original.
    juce::Image loadPigSprite (const char* data, int size)
    {
        auto img = juce::ImageFileFormat::loadFrom (data, (size_t) size);
        if (! img.isValid()) return {};

        // Target ~480×420 — plenty of headroom for any reasonable gauge size.
        const int targetW = 480;
        const float aspect = (float) img.getHeight() / (float) img.getWidth();
        const int targetH = (int) std::round (targetW * aspect);
        return img.rescaled (targetW, targetH, juce::Graphics::highResamplingQuality);
    }
}

namespace
{
    constexpr int kWidth  = 580;
    constexpr int kHeight = 480;
}

RedlineAudioProcessorEditor::RedlineAudioProcessorEditor (RedlineAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      proc (p),
      waveform (p.getPeakHistory()),
      modeSelector (p.apvts, "mode",
                    juce::StringArray { "tube", "tape", "diode", "hard" }),
      speedo (p.apvts, "drive")
{
    setSize (kWidth, kHeight);

    addAndMakeVisible (speedo);
    addAndMakeVisible (modeSelector);
    addAndMakeVisible (waveform);

    waveform.setCeilingDb (p.getCeilingDb());

    buildGrain();
    buildStreaks();
    buildFlameFrames();

    // Load the rock-pig mascot sprites and hand them to the gauge.
    speedo.setMoodImages (
        loadPigSprite (BinaryData::pig_calm_png,   BinaryData::pig_calm_pngSize),
        loadPigSprite (BinaryData::pig_grumpy_png, BinaryData::pig_grumpy_pngSize),
        loadPigSprite (BinaryData::pig_angry_png,  BinaryData::pig_angry_pngSize),
        loadPigSprite (BinaryData::pig_rage_png,   BinaryData::pig_rage_pngSize));

    startTimerHz (60);
}

void RedlineAudioProcessorEditor::buildFlameFrames()
{
    // We render at 2× the final size and downsample for clean edges.
    constexpr int finalW = 64;
    constexpr int finalH = 144;
    constexpr int superW = finalW * 2;
    constexpr int superH = finalH * 2;

    flameFrames.clear();
    flameFrames.reserve (kFlameTemplates * kFlameFrames);

    for (int tmpl = 0; tmpl < kFlameTemplates; ++tmpl)
    {
        // Each template has different proportions + spine wobble character
        const float widthMul   = (tmpl == 0) ? 1.00f : 0.82f;
        const float spineScale = (tmpl == 0) ? 0.07f : 0.12f;
        const float tmplSeed   = (tmpl == 0) ? 0.0f : 4.7f;

        for (int frame = 0; frame < kFlameFrames; ++frame)
        {
            const float framePhase =
                (float) frame / (float) kFlameFrames * juce::MathConstants<float>::twoPi;

            juce::Image big (juce::Image::ARGB, superW, superH, true);
            juce::Image::BitmapData bd (big, juce::Image::BitmapData::writeOnly);

            const float cxF = (float) superW * 0.5f;

            // Spine offset — slight S-curve that wobbles each frame
            auto spineX = [&] (float vT) -> float
            {
                const float a = std::sin (vT * 4.4f + framePhase + tmplSeed) * spineScale
                              + std::sin (vT * 9.1f + framePhase * 1.7f + tmplSeed) * spineScale * 0.4f;
                return cxF + (float) superW * a;
            };

            // Half-width along the flame — wide at base, narrows toward tip, with per-frame
            // wobble that gives a flickering edge.
            auto halfWidth = [&] (float vT) -> float
            {
                // Bell-shape biased toward the lower-middle of the flame
                const float bell = std::pow (vT, 0.55f) * std::pow (1.0f - vT, 0.30f);
                const float wob = std::sin (vT * 11.0f + framePhase * 1.4f + tmplSeed) * 0.20f
                                + std::sin (vT * 23.5f + framePhase * 2.5f + tmplSeed) * 0.10f
                                + std::sin (vT * 37.0f + framePhase * 3.3f + tmplSeed) * 0.05f;
                return (float) superW * 0.48f * widthMul * bell * (1.0f + wob);
            };

            for (int py = 0; py < superH; ++py)
            {
                // vT: 1 at bottom, 0 at top
                const float vT = 1.0f - (float) py / (float) (superH - 1);
                const float sx = spineX (vT);
                const float hw = halfWidth (vT);
                if (hw < 0.1f) continue;

                for (int px = 0; px < superW; ++px)
                {
                    const float horiz = std::abs ((float) px - sx) / hw;
                    if (horiz > 1.3f) continue;

                    // Radial profile — peak at spine, smooth falloff
                    float radial = std::cos (juce::jmin (1.0f, horiz)
                                              * juce::MathConstants<float>::pi * 0.5f);
                    radial = std::pow (juce::jmax (0.0f, radial), 1.5f);

                    // Vertical fade at tip + soft fade at base
                    float vertI = 1.0f;
                    if (vT > 0.78f)
                        vertI = std::cos ((vT - 0.78f) / 0.22f
                                            * juce::MathConstants<float>::pi * 0.5f);
                    if (vT < 0.12f)
                        vertI *= vT / 0.12f;

                    // Internal detail noise — multi-octave sin product
                    const float detail =
                          std::sin (px * 0.30f + framePhase * 3.1f + tmplSeed)
                        * std::cos (py * 0.42f + framePhase * 2.4f + tmplSeed) * 0.10f
                        + std::sin (px * 0.18f + py * 0.21f + framePhase * 1.7f + tmplSeed) * 0.07f;

                    float intensity = radial * vertI * (0.88f + detail);
                    intensity = juce::jlimit (0.0f, 1.0f, intensity);
                    if (intensity < 0.005f) continue;

                    // Colour: white-yellow core → yellow → orange → red → dark
                    juce::Colour col;
                    if (intensity > 0.78f)
                    {
                        const float t = (intensity - 0.78f) / 0.22f;
                        col = juce::Colour::fromRGB (255, 232, 130).interpolatedWith (
                                juce::Colour::fromRGB (255, 250, 220), t);
                    }
                    else if (intensity > 0.50f)
                    {
                        const float t = (intensity - 0.50f) / 0.28f;
                        col = juce::Colour::fromRGB (255, 142, 32).interpolatedWith (
                                juce::Colour::fromRGB (255, 232, 130), t);
                    }
                    else if (intensity > 0.22f)
                    {
                        const float t = (intensity - 0.22f) / 0.28f;
                        col = juce::Colour::fromRGB (190, 38, 14).interpolatedWith (
                                juce::Colour::fromRGB (255, 142, 32), t);
                    }
                    else
                    {
                        const float t = intensity / 0.22f;
                        col = juce::Colour::fromRGB (60, 8, 4).interpolatedWith (
                                juce::Colour::fromRGB (190, 38, 14), t);
                    }

                    // Alpha tapers at top + a touch at the base
                    float a = intensity;
                    if (vT > 0.65f) a *= 1.0f - (vT - 0.65f) / 0.35f * 0.30f;
                    if (vT < 0.10f) a *= vT / 0.10f;

                    bd.setPixelColour (px, py, col.withAlpha (juce::jlimit (0.0f, 1.0f, a)));
                }
            }

            // Downsample for AA edges
            flameFrames.push_back (big.rescaled (finalW, finalH,
                                                  juce::Graphics::highResamplingQuality));
        }
    }
}

void RedlineAudioProcessorEditor::buildStreaks()
{
    juce::Random rng (98765);
    streaks.clear();
    streaks.reserve (56);
    for (int i = 0; i < 56; ++i)
    {
        Streak s;
        s.x       = rng.nextFloat() * (float) kWidth;
        s.yT      = rng.nextFloat();                       // position within asphalt
        s.speed   = 0.35f + rng.nextFloat() * 1.65f;       // velocity multiplier
        s.length  = 6.0f  + rng.nextFloat() * 48.0f;
        s.alpha   = 0.08f + rng.nextFloat() * 0.22f;
        s.dark    = rng.nextFloat() < 0.30f;
        streaks.push_back (s);
    }
}

RedlineAudioProcessorEditor::~RedlineAudioProcessorEditor() = default;

void RedlineAudioProcessorEditor::buildGrain()
{
    // Cream speckle for the red surface — analog film grain feel.
    const int size = 128;
    grainImg = juce::Image (juce::Image::ARGB, size, size, true);
    juce::Random rng (4242);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const int n = rng.nextInt (255);
            const int alpha = n < 234 ? 0 : (n - 234) * 4;
            grainImg.setPixelAt (x, y,
                juce::Colour::fromRGBA (243, 236, 216,
                    (juce::uint8) juce::jlimit (0, 45, alpha)));
        }
    }
}

void RedlineAudioProcessorEditor::timerCallback()
{
    speedo.setActivity (proc.getSatActivity());

    const float gr = proc.getGainReductionDb();
    displayedGrDb = displayedGrDb * 0.78f + gr * 0.22f;

    const int m = (int) proc.apvts.getRawParameterValue ("mode")->load();
    modeSelector.setSelectedIndex (m);

    // Smoothly travel toward the active mode's accent colour
    const auto targetAccent = redline::theme::modeAccent (m);
    currentAccent = currentAccent.interpolatedWith (targetAccent, 0.18f);

    speedo.setAccent (currentAccent);
    modeSelector.setAccent (currentAccent);
    waveform.setAccent (currentAccent);

    // The header + footer are painted by the editor itself, so repaint them
    // each frame while the accent is moving toward target.
    const bool accentStillMoving =
        std::abs (currentAccent.getRed()   - targetAccent.getRed())   > 1 ||
        std::abs (currentAccent.getGreen() - targetAccent.getGreen()) > 1 ||
        std::abs (currentAccent.getBlue()  - targetAccent.getBlue())  > 1;

    waveform.setCeilingDb (proc.getCeilingDb());

    // ---- Mode-independent pressure / usage --------------------------------
    //
    // Old formula was activity-dominant — but the saturator's peak shaped
    // output differs per mode (tube tanh peaks ~0.83, hard near 1.0, etc.),
    // which made the visuals respond inconsistently to mode changes.
    //
    // New: usage = drive (mode-agnostic) × gate (is audio flowing?) + GR.
    //   gate is a fast 0→1 from raw activity, so silence still reads as 0
    //   drive contribution is mildly expanded (pow 0.65) so mid-drive feels
    //   meaningfully different from low drive.
    const float activity   = proc.getSatActivity();
    const float driveNorm  = juce::jlimit (0.0f, 1.0f, proc.getDriveDb() / 12.0f);
    const float drivePush  = std::pow (driveNorm, 0.65f);
    const float audioGate  = juce::jmin (1.0f, activity * 3.5f);
    const float usage = juce::jlimit (0.0f, 1.0f,
                                       audioGate * (drivePush * 0.92f)
                                       + displayedGrDb * 0.05f);

    // Pressure for the footer indicator — asymmetric smoothing (quick rise,
    // slow bleed-down), like a real pressure gauge needle.
    {
        const float a = (usage > displayedPressure) ? 0.28f : 0.08f;
        displayedPressure += (usage - displayedPressure) * a;
    }

    // Map pressure to the pig mood — 0=calm, 3=rage.
    //
    // Plateau-with-transition mapping: most of the time pressure sits firmly
    // inside one state's "pure" zone (mood is an exact integer), and only
    // glides between states across narrow smoothstep transition bands. This
    // kills the mushy "always slightly in-between" look you get from a plain
    // continuous mapping.
    //
    //   pressure  | mood (state)
    //   0.00–0.16 | 0.0   calm
    //   0.16–0.22 | crossfade 0→1
    //   0.22–0.40 | 1.0   grumpy
    //   0.40–0.46 | crossfade 1→2
    //   0.46–0.68 | 2.0   angry
    //   0.68–0.74 | crossfade 2→3
    //   0.74–1.00 | 3.0   rage
    {
        auto smoothstep = [] (float e0, float e1, float x)
        {
            const float t = juce::jlimit (0.0f, 1.0f, (x - e0) / (e1 - e0));
            return t * t * (3.0f - 2.0f * t);
        };

        const float p = displayedPressure;
        float mood;
        if      (p < 0.22f) mood = smoothstep (0.16f, 0.22f, p);
        else if (p < 0.46f) mood = 1.0f + smoothstep (0.40f, 0.46f, p);
        else if (p < 0.74f) mood = 2.0f + smoothstep (0.68f, 0.74f, p);
        else                mood = 3.0f;
        speedo.setMood (mood);
    }

    // ---- Shake — slight, only past a high-usage threshold --------------------
    const float shakeThreshold = 0.70f;
    const float overT = juce::jmax (0.0f, usage - shakeThreshold) / (1.0f - shakeThreshold);
    const float targetAmp = overT * 1.8f;        // max ~1.8 px tremor at full load
    shakeAmp += (targetAmp - shakeAmp) * 0.18f;

    shakePhase += 1.0f / 60.0f;
    const float p = shakePhase;

    // Subtle two-frequency tremor — translation only, no rotation
    const float dx = (std::sin (p * 47.3f) + 0.5f * std::sin (p * 113.7f)) * shakeAmp;
    const float dy = (std::cos (p * 53.1f) + 0.6f * std::cos (p * 97.5f)) * shakeAmp * 0.7f;

    speedo.setTransform   (juce::AffineTransform::translation (dx,         dy));
    waveform.setTransform (juce::AffineTransform::translation (dx * 0.15f, dy * 0.15f));

    titleShakeX = dx * 0.25f;
    titleShakeY = dy * 0.25f;

    // ---- Flame particle system around the gauge ------------------------------
    {
        const auto gb = speedo.getBounds();
        const float gcx = (float) gb.getCentreX();
        const float gcy = (float) gb.getCentreY() + (float) gb.getHeight() * 0.04f;
        const float gr  = juce::jmin ((float) gb.getWidth() * 0.5f,
                                       (float) gb.getHeight() * 0.55f) * 0.94f;

        // Quadratic ramp — barely flickering at low usage, full inferno at high
        const float spawnsPerSec = usage * usage * 150.0f;
        flameSpawnAcc += spawnsPerSec / 60.0f;
        while (flameSpawnAcc >= 1.0f && flames.size() < 160)
        {
            flameSpawnAcc -= 1.0f;
            FlameParticle f;

            // Spawn around the upper 270° of the gauge (matches the visible
            // dial sweep). Bottom 90° stays calm because heat rises through.
            const float ang = (flameRng.nextFloat() - 0.5f) * juce::MathConstants<float>::pi * 1.55f;
            const float radial = gr + (flameRng.nextFloat() - 0.5f) * 5.0f;
            f.x = gcx + std::sin (ang) * radial;
            f.y = gcy - std::cos (ang) * radial;

            // Velocity: heavy upward bias + tiny radial outward + horizontal jitter
            const float upSpeed = 38.0f + flameRng.nextFloat() * 55.0f;
            f.vx = std::sin (ang) * 12.0f + (flameRng.nextFloat() - 0.5f) * 22.0f;
            f.vy = -upSpeed;

            f.age = 0.0f;
            f.lifetime = 0.55f + flameRng.nextFloat() * 0.55f;
            f.baseSize = 24.0f + flameRng.nextFloat() * 32.0f;   // sprite width in px
            f.frameOffset    = flameRng.nextInt (kFlameFrames);
            f.spriteTemplate = flameRng.nextInt (kFlameTemplates);
            flames.push_back (f);
        }

        // Update + cull (swap-and-pop)
        const float dt = 1.0f / 60.0f;
        for (int i = (int) flames.size() - 1; i >= 0; --i)
        {
            auto& f = flames[(size_t) i];
            f.age += dt;
            if (f.age >= f.lifetime)
            {
                f = flames.back();
                flames.pop_back();
            }
            else
            {
                // Strong buoyancy + lateral sway for a flickering look
                f.vy -= 95.0f * dt;
                f.vx += std::sin (f.age * 13.0f + f.lifetime * 19.0f) * 60.0f * dt;
                f.vx *= 0.985f;  // slight damping so the sway doesn't run away
                f.x  += f.vx * dt;
                f.y  += f.vy * dt;
            }
        }
    }

    // Road is always scrolling, so the editor needs a full repaint each frame
    // (this is the dashboard surface — everything else paints on top of it).
    repaint();
    lastMode = m;
}

void RedlineAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace redline::theme;

    const float w = (float) getWidth();
    const float h = (float) getHeight();

    // 1. Purple gradient surface — strongly interpolated with the active mode
    //    accent so changing modes visibly shifts the whole dashboard temperature.
    {
        const auto topCol = bgDeep.brighter (0.10f).interpolatedWith (currentAccent, 0.22f);
        const auto botCol = bgDeep.darker   (0.30f).interpolatedWith (currentAccent, 0.12f);
        juce::ColourGradient bg (topCol, 0, 0, botCol, 0, h, false);
        g.setGradientFill (bg);
        g.fillAll();
    }

    // 2. Strong accent halo behind the gauge — dominant per-mode signal
    {
        const float gx = w * 0.5f;
        const float gy = h * 0.30f;
        const float haloR = 280.0f;
        juce::ColourGradient halo (currentAccent.withAlpha (0.40f), gx, gy,
                                    currentAccent.withAlpha (0.0f),  gx + haloR, gy,
                                    true);
        halo.addColour (0.55, currentAccent.withAlpha (0.12f));
        g.setGradientFill (halo);
        g.fillRect (getLocalBounds());
    }

    // 2b. Subtle corner vignette in the inverse direction — pulls the eye to
    //     the centre and gives extra colour saturation toward the rim.
    {
        const float cx2 = w * 0.5f;
        const float cy2 = h * 0.55f;
        juce::ColourGradient vignette (juce::Colour (0x00000000),     cx2, cy2,
                                        currentAccent.withAlpha (0.16f),
                                        0.0f, 0.0f, true);
        g.setGradientFill (vignette);
        g.fillRect (getLocalBounds());
    }

    // 3. Flames around the gauge — drawn behind the gauge so they only show
    //    where they lick past the outer rim.
    drawFlames (g);

    // 4. Cream-speckle grain
    if (grainImg.isValid())
    {
        juce::Graphics::ScopedSaveState s (g);
        g.setTiledImageFill (grainImg, 0, 0, 1.0f);
        g.fillRect (getLocalBounds());
    }

    // 5. Inner dashboard frame
    g.setColour (lineFaint);
    g.drawRect (getLocalBounds().reduced (10), 1);

    drawHeader (g, getLocalBounds().reduced (10).removeFromTop (60));
    drawFooter (g, getLocalBounds().reduced (10).removeFromBottom (40));
}

void RedlineAudioProcessorEditor::drawHeader (juce::Graphics& g, juce::Rectangle<int> r)
{
    using namespace redline::theme;

    // Apply the title shake transform so the whole header content moves together
    juce::Graphics::ScopedSaveState saved (g);
    g.addTransform (juce::AffineTransform::translation (titleShakeX, titleShakeY));

    // Wordmark — bigger, slightly tracked. No slogan beneath.
    auto inner = r.reduced (20, 0);

    g.setColour (inkHigh);
    g.setFont (headline (30.0f));
    const juce::String mark = "PIGASUS";

    // Measure so we can put a brass rule right under the wordmark only.
    juce::GlyphArrangement ga;
    ga.addLineOfText (g.getCurrentFont(), mark,
                       (float) inner.getX(),
                       (float) inner.getCentreY() + 8.0f);
    const auto markBounds = ga.getBoundingBox (0, -1, true);
    ga.draw (g);

    // Short brass rule just under the wordmark — picks up the active mode's accent
    {
        const float ruleY = markBounds.getBottom() + 4.0f;
        const float ruleW = markBounds.getWidth() * 0.34f;
        g.setColour (currentAccent);
        g.fillRect (juce::Rectangle<float> (markBounds.getX(), ruleY,
                                              ruleW, 1.6f));
    }

}

void RedlineAudioProcessorEditor::drawFooter (juce::Graphics& g, juce::Rectangle<int> r)
{
    using namespace redline::theme;

    g.setColour (lineFaint);
    g.drawHorizontalLine (r.getY(),
                          (float) r.getX() + 18.0f,
                          (float) r.getRight() - 18.0f);

    auto inner = r.reduced (18, 0);

    // "PRESSURE" label — heavier weight, wider tracking, slightly larger
    auto labelArea = inner.removeFromLeft (92);
    g.setColour (inkLow);
    g.setFont (labelWide (10.5f));
    g.drawText ("PRESSURE",
                labelArea.translated (0, 2),
                juce::Justification::centredLeft, false);

    // Numeric readout (right) — italic mono, digital-readout look.
    auto numArea = inner.removeFromRight (92);
    const float pressure = juce::jlimit (0.0f, 1.0f, displayedPressure);
    const bool inWarning = pressure > 0.78f;
    g.setColour (inWarning ? currentAccent : inkHigh);
    g.setFont (digital (14.0f, true));
    g.drawText (juce::String (pressure, 2) + " bar",
                numArea.translated (0, 2),
                juce::Justification::centredRight, false);

    // Pressure bar between label and readout
    auto barArea = inner.reduced (10, 0);
    const float barH = 6.0f;
    const float barY = barArea.getCentreY() - barH * 0.5f + 1.0f;
    const juce::Rectangle<float> barRect (barArea.getX(), barY, barArea.getWidth(), barH);

    // Recessed track
    g.setColour (juce::Colour (0xff0a0608));
    g.fillRoundedRectangle (barRect, 2.0f);

    // Tick marks at quarters
    g.setColour (lineMed.withAlpha (0.7f));
    for (int i = 1; i < 4; ++i)
    {
        const float xT = barRect.getX() + (float) i / 4.0f * barRect.getWidth();
        g.drawLine (xT, barRect.getY() - 1.0f,
                    xT, barRect.getBottom() + 1.0f, 0.7f);
    }

    // Faint red shadow in the upper-warning portion (last ~22%)
    {
        const float warnStartT = 0.78f;
        const float warnX = barRect.getX() + warnStartT * barRect.getWidth();
        g.setColour (juce::Colour (0xffa02018).withAlpha (0.22f));
        g.fillRect (warnX, barRect.getY(),
                     barRect.getRight() - warnX, barRect.getHeight());
    }

    // Live fill — accent up to warning zone, red beyond
    if (pressure > 0.0f)
    {
        const float warnStartT = 0.78f;
        const float fillW = pressure * barRect.getWidth();

        if (pressure <= warnStartT)
        {
            g.setColour (currentAccent);
            g.fillRect (juce::Rectangle<float> (barRect.getX(), barRect.getY(),
                                                  fillW, barRect.getHeight()));
        }
        else
        {
            // Safe segment in accent
            const float safeW = warnStartT * barRect.getWidth();
            g.setColour (currentAccent);
            g.fillRect (juce::Rectangle<float> (barRect.getX(), barRect.getY(),
                                                  safeW, barRect.getHeight()));

            // Warning segment fades into hot red
            const float warnW = fillW - safeW;
            juce::ColourGradient warn (currentAccent, barRect.getX() + safeW, 0,
                                        juce::Colour (0xffe34028),
                                        barRect.getX() + fillW, 0, false);
            g.setGradientFill (warn);
            g.fillRect (juce::Rectangle<float> (barRect.getX() + safeW, barRect.getY(),
                                                  warnW, barRect.getHeight()));
        }
    }

    // Border around the bar
    g.setColour (lineMed.withAlpha (0.9f));
    g.drawRoundedRectangle (barRect, 2.0f, 0.8f);
}

void RedlineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (60);
    area.removeFromBottom (40);

    auto gaugeArea = area.removeFromTop (240);
    const int gaugeW = juce::jmin (gaugeArea.getHeight() - 8, gaugeArea.getWidth() - 80);
    auto centredGauge = gaugeArea.withSizeKeepingCentre (gaugeW, gaugeArea.getHeight() - 4);
    speedo.setBounds (centredGauge);

    // Pedal strip — taller to fit pads + labels beneath
    auto modeStrip = area.removeFromTop (78).reduced (40, 4);
    modeSelector.setBounds (modeStrip);

    auto wf = area.reduced (14, 8);
    waveform.setBounds (wf);
}

void RedlineAudioProcessorEditor::drawFlames (juce::Graphics& g)
{
    if (flameFrames.empty()) return;

    // Frame-rate of sprite animation. The whole frame loop visually cycles
    // at ~24 fps; each particle has its own offset so they're never in sync.
    constexpr float spriteFps = 22.0f;

    for (const auto& f : flames)
    {
        const float ageFrac = juce::jlimit (0.0f, 1.0f, f.age / f.lifetime);

        // Pop up then ease down — drives both scale and alpha
        const float pulse = std::pow (std::sin (ageFrac * juce::MathConstants<float>::pi), 0.55f);

        // Fade in/out envelope
        const float fadeIn  = juce::jmin (1.0f, ageFrac / 0.08f);
        const float fadeOut = juce::jmin (1.0f, (1.0f - ageFrac) / 0.30f);
        const float alpha   = pulse * fadeIn * fadeOut;
        if (alpha < 0.01f) continue;

        // Pick the right sprite — template offset by spriteTemplate, frame by age
        const int frameIdx = ((int) std::floor (f.age * spriteFps) + f.frameOffset)
                              % kFlameFrames;
        const int spriteIdx = juce::jlimit (0, (int) flameFrames.size() - 1,
                                              f.spriteTemplate * kFlameFrames + frameIdx);
        const auto& sprite = flameFrames[(size_t) spriteIdx];

        // Size: width pulses with age; sprite aspect is 64:144 ≈ 1:2.25
        const float w = f.baseSize * pulse;
        const float h = w * ((float) sprite.getHeight() / (float) sprite.getWidth());
        if (w < 2.0f || h < 2.0f) continue;

        // Position: flame base sits near the particle anchor, body extends up
        const float dx = f.x - w * 0.5f;
        const float dy = f.y - h * 0.82f;

        const juce::Rectangle<float> target (dx, dy, w, h);

        // First pass — natural fire colours (yellow/orange/red).
        g.setOpacity (alpha);
        g.drawImage (sprite, target, juce::RectanglePlacement::stretchToFit, false);

        // Second pass — fill the sprite's alpha mask with the current mode
        // accent so flames pick up tube-amber / tape-bronze / diode-lime /
        // hard-cyan character. Kept at moderate alpha so they still read as fire.
        g.setOpacity (1.0f);
        g.setColour (currentAccent.withAlpha (alpha * 0.45f));
        g.drawImage (sprite, target, juce::RectanglePlacement::stretchToFit, true);
    }
}
