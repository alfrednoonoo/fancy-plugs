#include "SatureEditor.h"

#include <fancy/plugin/ParamFormat.h>

#include <cmath>

using namespace fancy::ui;
namespace sat = fancy::dsp::saturation;

namespace
{
// design/phase3-mockup-src/sat.html at 100 %.
constexpr int baseWidth = 760;
constexpr int bodyHeight = 392;
constexpr int baseHeight = ShellEditor::headerHeight + bodyHeight;
constexpr int rightColumnX = 656;

constexpr int leftX = 13, contentRight = 646;
constexpr int rowY = 10, rowHeight = 26;
constexpr int displayY = 44, displayHeight = 160;
constexpr int curveX = leftX, curveWidth = 170;
constexpr int harmonicsX = curveX + curveWidth + 8, harmonicsWidth = 432;
constexpr int cardY = 212, cardHeight = 127;
constexpr int driveCardX = leftX, driveCardWidth = 367;
constexpr int bandCardX = driveCardX + driveCardWidth + 8, bandCardWidth = contentRight - bandCardX;
constexpr int footerY = 362;
constexpr int faderTop = 26, faderHeight = 331;

constexpr float labelHeight = 12.0f;
constexpr float harmonicsFloorDb = -72.0f;

const juce::Colour orange = colours::accentOrange;

constexpr OutMeter::Geometry outGeometry { 14.5f, 16.0f, 316.0f, 26.0f, 4.9f, 324.0f, false, true };

// The analysis tone: bin 171 of an 8192-point FFT at 48 kHz, 1001.95 Hz, so every harmonic up
// to H10 falls exactly on a bin.
constexpr double analysisRate = 48000.0;
constexpr int fftOrder = 13, fftSize = 1 << fftOrder, toneBin = 171;
constexpr double toneAmplitude = 0.251188643150958; // -12 dBFS

void setValue (juce::RangedAudioParameter& parameter, float value)
{
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}

void setUpLabel (juce::Label& label, const juce::String& text, juce::Justification justification, float height = labelHeight,
                 juce::Colour colour = colours::textSecondary)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (font (Weight::regular, height));
    label.setColour (juce::Label::textColourId, colour);
    label.setJustificationType (justification);
    label.setBorderSize ({});
    label.setInterceptsMouseClicks (false, false);
}

void cardKnobs (std::initializer_list<Knob*> knobs, int x, int width, int y)
{
    constexpr int knobWidth = 76, knobHeight = 92;
    const auto spacing = (float) width / (float) knobs.size();
    auto index = 0;

    for (auto* knob : knobs)
    {
        const auto centre = (float) x + spacing * ((float) index++ + 0.5f);
        knob->setBounds (juce::roundToInt (centre) - knobWidth / 2, y, knobWidth, knobHeight);
    }
}

void paintCardTitle (juce::Graphics& g, const char* text, int x, int y)
{
    g.setColour (colours::textSecondary);
    g.setFont (font (Weight::semiBold, 12.0f).withExtraKerningFactor (0.06f));
    g.drawText (text, x + 12, y + 6, 140, 24, juce::Justification::centredLeft, false);
}

juce::String thdText (float percent)
{
    if (percent < 0.01f)
        return "< 0.01 %";

    return (percent < 1.0f ? fancy::formatFixed (percent, 2) : (percent < 10.0f ? fancy::formatFixed (percent, 1) : juce::String (juce::roundToInt (percent)))) + " %";
}
} // namespace

// Panels ----------------------------------------------------------------------

void SatureEditor::Panels::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgBody);

    g.setColour (colours::bgSection);
    g.fillRect (rightColumnX, 0, baseWidth - rightColumnX, bodyHeight);
    g.fillRoundedRectangle ((float) driveCardX, (float) cardY, (float) driveCardWidth, (float) cardHeight, 6.0f);
    g.fillRoundedRectangle ((float) bandCardX, (float) cardY, (float) bandCardWidth, (float) cardHeight, 6.0f);

    paintCardTitle (g, "DRIVE", driveCardX, cardY);
    paintCardTitle (g, "BAND", bandCardX, cardY);

    // Phase 4 Wave 2: a light tint and a caption while an outer band is driven.
    if (outerText.isNotEmpty())
    {
        g.setColour (orange.withAlpha (0.07f));
        g.fillRoundedRectangle ((float) bandCardX, (float) cardY, (float) bandCardWidth, (float) cardHeight, 6.0f);
        g.setColour (orange);
        g.setFont (font (Weight::regular, 11.0f));
        g.drawText (outerText, bandCardX + 70, cardY + 6, bandCardWidth - 80, 24, juce::Justification::centredLeft, true);
    }
}

void SatureEditor::Panels::setOuterText (const juce::String& text)
{
    if (text != outerText)
    {
        outerText = text;
        repaint();
    }
}

// TransferCurve ---------------------------------------------------------------

SatureEditor::TransferCurve::TransferCurve (SatureProcessor& processor)
    : satureProcessor (processor)
{
    setTooltip ("The curve the signal goes through: input level across, output level up, at the current Type, Drive and Character. "
                "The dashed line leaves the signal unchanged.");
}

void SatureEditor::TransferCurve::update()
{
    const auto targets = satureProcessor.readTargets();

    if (targets.type != lastType || targets.driveDb != lastDrive || targets.character != lastCharacter)
    {
        lastType = targets.type;
        lastDrive = targets.driveDb;
        lastCharacter = targets.character;
        repaint();
    }
}

void SatureEditor::TransferCurve::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto targets = satureProcessor.readTargets();

    g.setColour (colours::bgSection);
    g.fillRoundedRectangle (bounds, 6.0f);

    constexpr float margin = 16.0f;
    const auto width = bounds.getWidth(), height = bounds.getHeight();
    const auto xFor = [&] (double v) { return margin + 8.0f + (width - 2.0f * margin - 8.0f) * (float) (v + 1.0) * 0.5f; };
    const auto yFor = [&] (double v) { return height - margin - (height - 2.0f * margin) * (float) (v + 1.0) * 0.5f; };

    g.setColour (colours::bgBody);
    g.drawLine (xFor (-1.0), yFor (0.0), xFor (1.0), yFor (0.0), 1.0f);
    g.drawLine (xFor (0.0), yFor (-1.0), xFor (0.0), yFor (1.0), 1.0f);

    {
        juce::Path diagonal;
        diagonal.startNewSubPath (xFor (-1.0), yFor (-1.0));
        diagonal.lineTo (xFor (1.0), yFor (1.0));

        juce::Path dashed;
        const float dashes[] { 3.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, diagonal, dashes, 2);
        g.setColour (colours::grey6);
        g.fillPath (dashed);
    }

    const auto curve = sat::makeCurve (targets.type, targets.character);
    const auto drive = std::pow (10.0, targets.driveDb / 20.0);
    const auto dc = curve.shape (curve.bias);
    const auto f = [&] (double v) { return curve.shape (drive * v + curve.bias) - dc; };
    const auto peak = std::max (std::abs (f (1.0)), std::abs (f (-1.0))) * 1.04;

    juce::Path path;

    for (int step = 0; step <= 200; ++step)
    {
        const auto v = -1.0 + 2.0 * step / 200.0;
        const auto point = juce::Point<float> (xFor (v), yFor (f (v) / std::max (peak, 1.0e-9)));

        if (step == 0)
            path.startNewSubPath (point);
        else
            path.lineTo (point);
    }

    g.setColour (orange);
    g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (colours::textSecondary);
    g.setFont (font (Weight::regular, 10.0f));
    g.drawText (juce::String::fromUTF8 ("in \xe2\x86\x92 out"), juce::Rectangle<float> (10.0f, 4.0f, 100.0f, 14.0f), juce::Justification::centredLeft, false);
}

// Analysis --------------------------------------------------------------------

SatureEditor::Harmonics SatureEditor::analyse (SatureDsp& dsp, SatureDsp::Targets targets)
{
    Harmonics result;

    targets.wetGain = 1.0;
    targets.dryGain = 0.0;
    targets.outputGain = 1.0;
    targets.trimGain = 1.0;
    targets.listen = false;
    targets.bypass = false;
    targets.oversampling = SatureDsp::x4;

    result.outsideBand = (targets.lowOn && targets.lowHz > 1000.0) || (targets.highOn && SatureDsp::effectiveHighHz (targets) < 1000.0);

    dsp.snapToTargets (targets);

    const auto hz = (double) toneBin * analysisRate / (double) fftSize;
    const auto warmUp = (int) (0.5 * analysisRate) + dsp.getLatencySamples();
    const auto total = warmUp + fftSize;

    std::vector<float> left ((size_t) total), right ((size_t) total);

    for (int i = 0; i < total; ++i)
        left[(size_t) i] = right[(size_t) i] = (float) (toneAmplitude * std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / analysisRate));

    for (int start = 0; start < total; start += 512)
    {
        float* channels[] { left.data() + start, right.data() + start };
        dsp.setTargets (targets);
        dsp.process (channels, 2, std::min (512, total - start));
    }

    // 4-term Blackman-Harris, sidelobes below -92 dB.
    std::vector<float> data ((size_t) fftSize * 2, 0.0f);

    for (int i = 0; i < fftSize; ++i)
    {
        const auto phase = 2.0 * juce::MathConstants<double>::pi * i / (fftSize - 1);
        const auto window = 0.35875 - 0.48829 * std::cos (phase) + 0.14128 * std::cos (2.0 * phase) - 0.01168 * std::cos (3.0 * phase);
        data[(size_t) i] = (float) (left[(size_t) (warmUp + i)] * window);
    }

    juce::dsp::FFT fft (fftOrder);
    fft.performFrequencyOnlyForwardTransform (data.data());

    std::array<double, 10> power {};

    for (int h = 1; h <= 10; ++h)
        for (int bin = h * toneBin - 4; bin <= h * toneBin + 4; ++bin)
            power[(size_t) h - 1] += (double) data[(size_t) bin] * (double) data[(size_t) bin];

    const auto fundamental = std::max (power[0], 1.0e-30);
    auto distortion = 0.0;

    for (int h = 1; h <= 10; ++h)
    {
        result.levelsDb[(size_t) h - 1] = (float) (10.0 * std::log10 (std::max (power[(size_t) h - 1], 1.0e-30) / fundamental));

        if (h >= 2)
            distortion += power[(size_t) h - 1];
    }

    result.thdPercent = (float) (100.0 * std::sqrt (distortion / fundamental));
    return result;
}

// HarmonicsCard ---------------------------------------------------------------

SatureEditor::HarmonicsCard::HarmonicsCard (SatureProcessor& processor)
    : juce::Thread ("Fancy Sature harmonics"), satureProcessor (processor)
{
    setTooltip ("What the current settings add to a 1 kHz tone at -12 dBFS: even harmonics in full orange, odd ones lighter, "
                "the tone itself dimmed. Worked out from the settings, not measured from the audio, so it's always there to read.");
    startThread (juce::Thread::Priority::low);
}

SatureEditor::HarmonicsCard::~HarmonicsCard()
{
    signalThreadShouldExit();
    notify();
    stopThread (4000);
}

bool SatureEditor::HarmonicsCard::sameSound (const SatureDsp::Targets& a, const SatureDsp::Targets& b)
{
    return a.type == b.type && a.driveDb == b.driveDb && a.character == b.character && a.toneDb == b.toneDb
        && a.lowOn == b.lowOn && a.lowHz == b.lowHz && a.highOn == b.highOn && a.highHz == b.highHz
        && a.autoGain == b.autoGain && a.dcFilter == b.dcFilter && a.minimumPhase == b.minimumPhase;
}

void SatureEditor::HarmonicsCard::update()
{
    if (frozen)
        return;

    const auto targets = satureProcessor.readTargets();

    if (! hasRequest || ! sameSound (targets, requested))
    {
        requested = targets;
        hasRequest = true;

        {
            const juce::SpinLock::ScopedLockType scoped (lock);
            pending = targets;
        }

        notify();
    }

    const auto version = resultVersion.load();

    if (version != shownVersion)
    {
        shownVersion = version;
        const juce::SpinLock::ScopedLockType scoped (lock);
        shown = result;
        repaint();
    }
}

void SatureEditor::HarmonicsCard::show (const Harmonics& harmonics)
{
    frozen = true;
    shown = harmonics;
    repaint();
}

void SatureEditor::HarmonicsCard::run()
{
    SatureDsp dsp;
    dsp.prepare (analysisRate);

    SatureDsp::Targets done;
    auto hasDone = false;

    while (! threadShouldExit())
    {
        SatureDsp::Targets next;

        {
            const juce::SpinLock::ScopedLockType scoped (lock);
            next = pending;
        }

        if (hasDone && sameSound (next, done))
        {
            wait (-1);
            continue;
        }

        const auto harmonics = analyse (dsp, next);
        done = next;
        hasDone = true;

        {
            const juce::SpinLock::ScopedLockType scoped (lock);
            result = harmonics;
        }

        ++resultVersion;
    }
}

void SatureEditor::HarmonicsCard::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto width = bounds.getWidth(), height = bounds.getHeight();

    g.setColour (colours::bgSection);
    g.fillRoundedRectangle (bounds, 6.0f);

    constexpr float left = 36.0f, top = 22.0f;
    const auto bottom = height - 24.0f;
    const auto yFor = [&] (float db) { return top + (bottom - top) * (-juce::jlimit (harmonicsFloorDb, 0.0f, db) / -harmonicsFloorDb); };

    g.setFont (font (Weight::regular, 9.0f));

    for (const auto db : { 0.0f, -24.0f, -48.0f, -72.0f })
    {
        g.setColour (db == 0.0f ? colours::grey5 : colours::bgBody);
        g.drawLine (left, yFor (db), width - 12.0f, yFor (db), 1.0f);
        g.setColour (colours::textSecondary);
        g.drawText (juce::String (juce::roundToInt (db)), juce::Rectangle<float> (0.0f, yFor (db) - 6.0f, left - 6.0f, 12.0f),
                    juce::Justification::centredRight, false);
    }

    constexpr float barWidth = 26.0f;
    const auto gap = (width - left - 20.0f - barWidth * 10.0f) / 9.0f;

    for (int h = 0; h < 10; ++h)
    {
        const auto x = left + 6.0f + (float) h * (barWidth + gap);
        const auto db = shown.outsideBand && h > 0 ? harmonicsFloorDb : shown.levelsDb[(size_t) h];
        const auto y = yFor (db);
        const auto even = (h + 1) % 2 == 0;

        if (bottom - y > 0.5f)
        {
            g.setColour (orange.withAlpha (h == 0 ? 0.35f : (even ? 1.0f : 0.7f)));
            g.fillRoundedRectangle (juce::Rectangle<float> (x, y, barWidth, bottom - y), 2.0f);
        }

        g.setColour (colours::textSecondary);
        g.drawText (h == 0 ? juce::String ("1k") : "H" + juce::String (h + 1), juce::Rectangle<float> (x - 6.0f, height - 18.0f, barWidth + 12.0f, 12.0f),
                    juce::Justification::centred, false);
    }

    const auto caption = shown.outsideBand
                             ? juce::String::fromUTF8 ("1 kHz is outside the band \xc2\xb7 no harmonics added to it")
                             : juce::String::fromUTF8 ("Harmonics added to a 1 kHz tone at these settings \xc2\xb7 THD ") + thdText (shown.thdPercent);

    g.setColour (colours::textSecondary);
    g.setFont (font (Weight::regular, 10.0f));
    g.drawText (caption, juce::Rectangle<float> (12.0f, 4.0f, width - 24.0f, 14.0f), juce::Justification::centredLeft, true);
}

// SatureEditor ----------------------------------------------------------------

SatureEditor::SatureEditor (SatureProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::full),
      typeSegments (parameter ("type"), orange, colours::bgWell),
      transferCurve (processor),
      harmonicsCard (processor),
      driveKnob (parameter ("drive"), "Drive", orange, Knob::tightStyle),
      characterKnob (parameter ("character"), "Character", orange, Knob::tightStyle, true),
      toneKnob (parameter ("tone"), "Tone", orange, Knob::tightStyle, true),
      lowKnob (parameter ("low"), "Low", colours::uiBlue, Knob::tightStyle),
      highKnob (parameter ("high"), "High", colours::uiBlue, Knob::tightStyle),
      autoGainCheckbox (parameter ("autogain"), "Auto gain", orange),
      listenCheckbox (getFancyProcessor(), "Listen to added harmonics", orange),
      editLink (orange),
      outMeter (outGeometry),
      wetFader (parameter ("wet"), colours::uiTeal),
      dryFader (parameter ("dry"), colours::uiTeal),
      oversamplingDropdown (parameter ("oversampling")),
      filterDropdown (parameter ("osfilter")),
      trimKnob (parameter ("trim"), "Input trim", colours::uiTeal, Knob::smallStyle, true),
      dcCheckbox (parameter ("dcfilter"), "DC filter", orange),
      lowDriveKnob (parameter ("lowDrive"), "Drive", orange, Knob::smallStyle),
      lowMixKnob (parameter ("lowMix"), "Mix", orange, Knob::smallStyle),
      highDriveKnob (parameter ("highDrive"), "Drive", orange, Knob::smallStyle),
      highMixKnob (parameter ("highMix"), "Mix", orange, Knob::smallStyle),
      offsetKnob (parameter ("offset"), "Offset", orange, Knob::smallStyle, true),
      lowCutKnob (parameter ("lowCut"), "Freq", orange, Knob::smallStyle),
      highCutKnob (parameter ("highCut"), "Freq", orange, Knob::smallStyle),
      spreadKnob (parameter ("spread"), "Spread", orange, Knob::smallStyle),
      lowSlopeDropdown (parameter ("lowCutSlope")),
      highSlopeDropdown (parameter ("highCutSlope")),
      channelsDropdown (parameter ("channels")),
      lowPosSegments (parameter ("lowCutPos"), orange, colours::bgWell),
      highPosSegments (parameter ("highCutPos"), orange, colours::bgWell)
{
    auto& body = getBody();

    // Phase 2.1 W.1: room in the header for undo, redo, A/B and Compare.
    setShortHeaderLabels (true);
    setPresetDropdownWidth (120);

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (0, 0, baseWidth, bodyHeight);
    body.addAndMakeVisible (panels);

    // The Type row.
    setUpLabel (typeLabel, "Type", juce::Justification::centredLeft);
    typeLabel.setBounds (leftX + 2, rowY, 34, rowHeight);
    typeSegments.setBounds (50, rowY + 1, 336, 24); // Phase 4 Wave 2: six Types (mockup 61)
    typeSegments.setTextHeight (labelHeight);
    typeSegments.setFitToText (true);
    typeSegments.setTooltip ("Tape: soft and warm, a little darker when driven hard. Tube: soft and uneven, rich in even harmonics. "
                             "Transistor: a harder knee. Clip: hard clipping with a small rounded corner.");

    // Displays.
    transferCurve.setBounds (curveX, displayY, curveWidth, displayHeight);
    harmonicsCard.setBounds (harmonicsX, displayY, harmonicsWidth, displayHeight);

    // Cards.
    cardKnobs ({ &driveKnob, &characterKnob, &toneKnob }, driveCardX, driveCardWidth, cardY + 30);
    cardKnobs ({ &lowKnob, &highKnob }, bandCardX, bandCardWidth, cardY + 30);

    driveKnob.setTooltip ("How hard the signal is pushed into the curve. With Auto gain on, the level stays put.");
    characterKnob.setTooltip ("Left: more even harmonics, from an uneven curve. Right: more odd harmonics, from a harder, even one.");
    toneKnob.setTooltip ("Tilts the saturated band after the curve: up brightens, down darkens. The total tilt from bottom to top.");
    lowKnob.setTooltip ("Below this, the signal passes clean, so the low end stays tight. Off saturates everything.");
    highKnob.setTooltip ("Above this, the signal passes clean, so the top doesn't get harsh. Off saturates everything. "
                         "Kept at least an octave above Low.");

    for (auto* knob : { &characterKnob, &toneKnob })
        knob->setShowDefaultMark (true);

    // Footer.
    autoGainCheckbox.setBounds (leftX, footerY, 96, 20);
    listenCheckbox.setBounds (leftX + 108, footerY, 190, 20);
    editLink.setBounds (contentRight - 64, footerY - 2, 64, 24);

    autoGainCheckbox.setLabelHeight (labelHeight);
    listenCheckbox.setLabelHeight (labelHeight);

    autoGainCheckbox.setTooltip ("Keeps the loudness about the same at any Type, Drive, Character and Tone, from a fixed curve "
                                 "measured on pink noise, so louder never fools the ear and the level never pumps.");
    listenCheckbox.setTooltip ("Plays only what the saturation adds, so you can hear the harmonics on their own. "
                               "Turns off when you close this window, and is never saved.");

    // Right column, as Delay's.
    outMeter.setBounds (rightColumnX - 6, 10, 46, 350);
    setUpLabel (wetLabel, "Wet", juce::Justification::centred, 14.0f, colours::textPrimary);
    setUpLabel (dryLabel, "Dry", juce::Justification::centred, 14.0f, colours::textPrimary);
    wetLabel.setBounds (690, 10, 33, 17);
    dryLabel.setBounds (723, 10, 33, 17);
    wetFader.setBounds (690, faderTop, 33, faderHeight);
    dryFader.setBounds (723, faderTop, 33, faderHeight);
    wetFader.setTooltip ("The saturated signal's level.");
    dryFader.setTooltip ("The clean signal's level, mixed in for parallel saturation. It stays lined up with the saturated one.");

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &typeLabel, &typeSegments, &transferCurve, &harmonicsCard,
             &driveKnob, &characterKnob, &toneKnob, &lowKnob, &highKnob,
             &autoGainCheckbox, &listenCheckbox, &editLink,
             &outMeter, &wetLabel, &dryLabel, &wetFader, &dryFader })
        body.addAndMakeVisible (item);

    // EDIT drawer.
    drawer.setBounds (0, displayY - 4, rightColumnX, bodyHeight - displayY + 4);

    setUpLabel (oversamplingLabel, "Oversampling", juce::Justification::centredLeft, 14.0f, colours::textPrimary);
    setUpLabel (filterLabel, "Oversampling filter", juce::Justification::centredLeft, 14.0f, colours::textPrimary);
    setUpLabel (aliasNote, {}, juce::Justification::topLeft, labelHeight);

    oversamplingLabel.setBounds (24, 60, 170, 24);
    oversamplingDropdown.setBounds (200, 60, 170, 24);
    aliasNote.setBounds (380, 60, 260, 40);
    filterLabel.setBounds (24, 100, 170, 24);
    filterDropdown.setBounds (200, 100, 170, 24);
    trimKnob.setBounds (24, 140, 90, 99);
    dcCheckbox.setBounds (140, 180, 92, 20); // narrowed for the OUTER BANDS group (Phase 4 Wave 2)

    oversamplingDropdown.setTooltip ("Runs the curve at a higher rate, so its harmonics don't fold back as tones that don't belong. "
                                     "Off aliases on bright, heavily driven sounds; 8x is cleaner.");
    filterDropdown.setTooltip ("Minimum phase adds almost no latency. Linear phase keeps the phase exact, with more latency. "
                               "Either way the clean and saturated signals stay lined up.");
    trimKnob.setTooltip ("The level going in, before the curve. Raising it drives harder, without Auto gain's correction.");
    dcCheckbox.setTooltip ("Removes the offset an uneven curve can add, so the waveform stays centred.");
    addLatencyNote (oversamplingDropdown);
    addLatencyNote (filterDropdown);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &oversamplingLabel, &oversamplingDropdown, &aliasNote, &filterLabel, &filterDropdown, &trimKnob, &dcCheckbox })
        drawer.addAndMakeVisible (item);

    // Phase 4 Wave 2: the OUTER BANDS group beside Input trim and DC filter.
    setUpLabel (outerCaption, "OUTER BANDS", juce::Justification::topLeft, 10.0f);
    setUpLabel (belowLowLabel, "Below Low", juce::Justification::topLeft, 11.0f);
    setUpLabel (aboveHighLabel, "Above High", juce::Justification::topLeft, 11.0f);
    setUpLabel (outerNote, "Type, Character, Tone and Tape HF loss are the mid band's.", juce::Justification::topLeft, 11.0f);
    outerCaption.setBounds (240, 139, 200, 12);
    belowLowLabel.setBounds (240, 155, 140, 14);
    aboveHighLabel.setBounds (390, 155, 140, 14);
    lowDriveKnob.setBounds (236, 168, 70, 99);
    lowMixKnob.setBounds (306, 168, 70, 99);
    highDriveKnob.setBounds (386, 168, 70, 99);
    highMixKnob.setBounds (456, 168, 70, 99);
    outerNote.setBounds (530, 178, 104, 60);

    lowDriveKnob.setTooltip ("Saturates the band below Low too, as hard as this. 0 leaves it clean.");
    lowMixKnob.setTooltip ("How much of the low band's added harmonics you hear.");
    highDriveKnob.setTooltip ("Saturates the band above High too, as hard as this. 0 leaves it clean.");
    highMixKnob.setTooltip ("How much of the high band's added harmonics you hear.");

    for (juce::Component* item : std::initializer_list<juce::Component*> { &outerCaption, &belowLowLabel, &aboveHighLabel, &outerNote,
                                                                           &lowDriveKnob, &lowMixKnob, &highDriveKnob, &highMixKnob })
        drawer.addAndMakeVisible (item);

    // Phase 4 Wave 2, items 35 to 39 (mockup 61; drawer coordinates are the mockup's window
    // coordinates less 113). The page switch beside the drawer's title, as Comp's.
    drawerTabs.setBounds (120, 13, 210, 22);
    drawerTabs.onChange = [this] (int page) { showDrawerPage (page); };
    drawer.addAndMakeVisible (drawerTabs);

    // Shape: Offset under Input trim.
    offsetKnob.setBounds (26, 245, 70, 99);
    offsetKnob.setTooltip ("Shifts the signal before the curve and back after it, so even a symmetric curve adds even harmonics, the 2nd first.");
    setUpLabel (offsetNote, "Bias before the curve: even harmonics, the 2nd first.", juce::Justification::topLeft, 11.0f);
    offsetNote.setBounds (104, 272, 120, 48);

    shapeItems = { &oversamplingLabel, &oversamplingDropdown, &aliasNote, &filterLabel, &filterDropdown, &trimKnob, &dcCheckbox,
                   &outerCaption, &belowLowLabel, &aboveHighLabel, &outerNote, &lowDriveKnob, &lowMixKnob, &highDriveKnob, &highMixKnob,
                   &offsetKnob, &offsetNote };

    // Tone and stereo: the cuts, then Spread and Channels.
    setUpLabel (lowCutTitle, "LOW CUT", juce::Justification::topLeft, 10.0f);
    setUpLabel (highCutTitle, "HIGH CUT", juce::Justification::topLeft, 10.0f);
    setUpLabel (stereoTitle, "STEREO", juce::Justification::topLeft, 10.0f);
    setUpLabel (lowSlopeLabel, "Slope", juce::Justification::centredLeft, 12.0f);
    setUpLabel (highSlopeLabel, "Slope", juce::Justification::centredLeft, 12.0f);
    setUpLabel (channelsLabel, "Channels", juce::Justification::centredLeft, 13.0f);
    setUpLabel (cutNote, "Pre shapes what's driven: a Low cut before the curve stops a heavy bass from muddying everything above it. "
                         "Post shapes what's heard: only the added harmonics. Off (the default) isn't run.",
                juce::Justification::topLeft, 11.0f);
    setUpLabel (spreadNote, "Widens only the added harmonics; the fundamental stays centred. 0 to 50 %, so mono keeps them (-3 dB at most).",
                juce::Justification::topLeft, 11.0f);
    setUpLabel (channelsNote, "Stereo (today), Mid only or Side only.", juce::Justification::topLeft, 11.0f);

    lowCutTitle.setBounds (24, 55, 200, 12);
    lowCutKnob.setBounds (27, 67, 70, 99);
    lowSlopeLabel.setBounds (118, 76, 40, 24);
    lowSlopeDropdown.setBounds (160, 76, 110, 24);
    lowPosSegments.setBounds (160, 111, 110, 22);

    highCutTitle.setBounds (330, 55, 200, 12);
    highCutKnob.setBounds (333, 67, 70, 99);
    highSlopeLabel.setBounds (424, 76, 40, 24);
    highSlopeDropdown.setBounds (466, 76, 110, 24);
    highPosSegments.setBounds (466, 111, 110, 22);

    cutNote.setBounds (24, 163, 560, 32);

    stereoTitle.setBounds (24, 211, 200, 12);
    spreadKnob.setBounds (27, 223, 70, 99);
    spreadNote.setBounds (118, 239, 220, 48);
    channelsLabel.setBounds (370, 238, 66, 24);
    channelsDropdown.setBounds (440, 238, 136, 24);
    channelsNote.setBounds (370, 271, 210, 16);

    for (auto* segments : { &lowPosSegments, &highPosSegments })
        segments->setTextHeight (12.0f);

    lowCutKnob.setTooltip ("A high-pass on the saturation, Off at the bottom.");
    highCutKnob.setTooltip ("A low-pass on the saturation, Off at the top.");
    lowSlopeDropdown.setTooltip ("How steep the low cut is.");
    highSlopeDropdown.setTooltip ("How steep the high cut is.");
    lowPosSegments.setTooltip ("Pre cuts what's driven, so the cut lows pass clean. Post cuts the added harmonics.");
    highPosSegments.setTooltip ("Pre cuts what's driven, so the cut highs pass clean. Post cuts the added harmonics, for less fizz.");
    spreadKnob.setTooltip ("Spreads the added harmonics wide, the fundamental centred. Up to 50 %, so they survive in mono.");
    channelsDropdown.setTooltip ("Stereo drives left and right. Mid only drives the centre and leaves the sides clean; Side only the reverse.");

    toneStereoItems = { &lowCutTitle, &lowCutKnob, &lowSlopeLabel, &lowSlopeDropdown, &lowPosSegments,
                        &highCutTitle, &highCutKnob, &highSlopeLabel, &highSlopeDropdown, &highPosSegments,
                        &cutNote, &stereoTitle, &spreadKnob, &spreadNote, &channelsLabel, &channelsDropdown, &channelsNote };

    for (auto* item : shapeItems)
        drawer.addAndMakeVisible (item);

    for (auto* item : toneStereoItems)
        drawer.addChildComponent (item);

    showDrawerPage (shapePage);

    updateOuter();

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (! drawer.isVisible()); };

    transferCurve.update();
}

SatureEditor::~SatureEditor() = default;

SatureProcessor& SatureEditor::satureProcessor() noexcept
{
    return dynamic_cast<SatureProcessor&> (getFancyProcessor());
}

// Phase 4 Wave 2: an outer band exists only while its edge is set, so its pair dims otherwise;
// the BAND card names the bands being driven.
void SatureEditor::updateOuter()
{
    const auto targets = satureProcessor().readTargets();

    for (auto* knob : { &lowDriveKnob, &lowMixKnob })
        knob->setAlpha (targets.lowOn ? 1.0f : 0.4f);

    for (auto* knob : { &highDriveKnob, &highMixKnob })
        knob->setAlpha (targets.highOn ? 1.0f : 0.4f);

    juce::StringArray driven;

    if (targets.lowOn && targets.lowDriveDb > 0.0)
        driven.add ("low band +" + fancy::formatFixed ((float) targets.lowDriveDb, 1) + " dB");

    if (targets.highOn && targets.highDriveDb > 0.0)
        driven.add ("high band +" + fancy::formatFixed ((float) targets.highDriveDb, 1) + " dB");

    panels.setOuterText (driven.joinIntoString (juce::String::fromUTF8 (" \xc2\xb7 ")));
}

void SatureEditor::showDrawerPage (int page)
{
    drawerPage = page;
    drawerTabs.setSelected (page);

    for (auto* item : shapeItems)
        item->setVisible (page == shapePage);

    for (auto* item : toneStereoItems)
        item->setVisible (page == toneStereoPage);
}

void SatureEditor::onTick (float seconds)
{
    auto& processor = satureProcessor();

    updateOuter(); // Phase 4 Wave 2

    // Item 37: a cut's slope and position dim while it's Off.
    {
        const auto targets = processor.readTargets();

        for (auto* item : std::initializer_list<juce::Component*> { &lowSlopeDropdown, &lowPosSegments })
            item->setAlpha (targets.lowCutOn ? 1.0f : 0.5f);

        for (auto* item : std::initializer_list<juce::Component*> { &highSlopeDropdown, &highPosSegments })
            item->setAlpha (targets.highCutOn ? 1.0f : 0.5f);
    }

    transferCurve.update();
    harmonicsCard.update();
    outMeter.update (processor.outputMeter, seconds);

    const auto targets = processor.readTargets();
    const auto latency = processor.getDsp().latencyFor (targets.oversampling, targets.minimumPhase);
    aliasNote.setText ("(+" + juce::String (latency) + (latency == 1 ? " sample" : " samples") + " latency)"
                           + (targets.oversampling == SatureDsp::off ? juce::String ("\nOff aliases on bright, heavily driven sounds.") : juce::String()),
                       juce::dontSendNotification);
}

void SatureEditor::showPreview (bool drawerOpen)
{
    stopUpdates();

    setValue (parameter ("type"), 0.0f);
    setValue (parameter ("drive"), 14.0f);
    setValue (parameter ("character"), -30.0f);
    setValue (parameter ("tone"), 2.0f);
    setValue (parameter ("low"), 120.0f);
    setValue (parameter ("high"), SatureProcessor::highOffHz);
    setValue (parameter ("wet"), 0.0f);

    transferCurve.update();

    SatureDsp dsp;
    dsp.prepare (analysisRate);
    harmonicsCard.show (analyse (dsp, satureProcessor().readTargets()));

    outMeter.show (-7.9f, -7.6f);

    const auto targets = satureProcessor().readTargets();
    const auto latency = satureProcessor().getDsp().latencyFor (targets.oversampling, targets.minimumPhase);
    aliasNote.setText ("(+" + juce::String (latency) + " samples latency)", juce::dontSendNotification);

    drawer.setVisible (drawerOpen);
    showDrawerPage (shapePage);
    updateOuter();
}

juce::Colour SatureEditor::getAccentColour()
{
    return orange;
}

std::vector<ShellEditor::PopupOpener> SatureEditor::getPopupOpeners()
{
    auto openers = ShellEditor::getPopupOpeners();
    openers.push_back ({ "dropdown-oversampling", [this] { drawer.setVisible (true); oversamplingDropdown.click(); } });
    openers.push_back ({ "tooltip-listen", [this] { showTip (listenCheckbox, listenCheckbox.getTooltip()); } });
    openers.push_back ({ "value-drive", [this] { driveKnob.openValueEditor(); } });

    // Phase 4 Wave 2, mockup 61: the Shape page with Offset at 0.10 (panel 2), then the Tone and
    // stereo page at the mockup's values (panel 3), and Fold chosen on the main window. Last, as
    // they leave those settings in place.
    openers.push_back ({ "page-shape", [this]
    {
        if (auto* editor = driveKnob.getValueEditor()) editor->setVisible (false);
        setValue (parameter ("offset"), 0.1f);
        drawer.setVisible (true);
        showDrawerPage (shapePage);
    } });
    openers.push_back ({ "page-tone", [this]
    {
        setValue (parameter ("lowCut"), 200.0f);
        setValue (parameter ("lowCutSlope"), 1.0f);
        setValue (parameter ("spread"), 35.0f);
        setValue (parameter ("channels"), 1.0f);
        drawer.setVisible (true);
        showDrawerPage (toneStereoPage);
    } });
    openers.push_back ({ "type-fold", [this]
    {
        drawer.setVisible (false);
        setValue (parameter ("type"), (float) fancy::dsp::saturation::fold);
        transferCurve.update();
    } });
    return openers;
}
