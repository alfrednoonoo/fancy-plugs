#include "RoomEditor.h"

#include <fancy/plugin/ParamFormat.h>

#include <cmath>

using namespace fancy::ui;

namespace
{
// design/phase3-mockup-src/room.html at 100 %, with Sature's margins and right column.
constexpr int baseWidth = 760;
constexpr int bodyHeight = 392;
constexpr int baseHeight = ShellEditor::headerHeight + bodyHeight;
constexpr int rightColumnX = 656;

constexpr int leftX = 13, contentRight = 646;
constexpr int displayX = leftX, displayY = 10, displayWidth = 610, displayHeight = 198;
constexpr int cardY = 216, cardHeight = 127;
constexpr int spaceCardX = leftX, spaceCardWidth = 415;
constexpr int toneCardX = spaceCardX + spaceCardWidth + 8, toneCardWidth = contentRight - toneCardX;
constexpr int knobWidth = 68, knobHeight = 92; // docs/room.md, Phase 4: under the 69 px spacing
constexpr int footerY = 362;
constexpr int faderTop = 26, faderHeight = 331;

constexpr float labelHeight = 12.0f;

const juce::Colour purple = colours::accentPurple;
const juce::Colour lightInk { 0xffededed };

constexpr OutMeter::Geometry outGeometry { 14.5f, 16.0f, 316.0f, 26.0f, 4.9f, 324.0f, false, true };

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

float textWidth (const juce::Font& f, const juce::String& text)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (f, text, 0.0f, 0.0f);
    return glyphs.getBoundingBox (0, -1, true).getWidth();
}

juce::String secondsText (double seconds)
{
    return fancy::formatFixed ((float) seconds, seconds < 10.0 ? 2 : 1) + " s";
}

juce::String tickText (double seconds)
{
    const auto whole = std::abs (seconds - std::round (seconds)) < 1.0e-6;
    return (whole ? juce::String (juce::roundToInt (seconds)) : fancy::formatFixed ((float) seconds, 1)) + " s";
}
} // namespace

// Panels ----------------------------------------------------------------------

void RoomEditor::Panels::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgBody);

    g.setColour (colours::bgSection);
    g.fillRect (rightColumnX, 0, baseWidth - rightColumnX, bodyHeight);
    g.fillRoundedRectangle ((float) spaceCardX, (float) cardY, (float) spaceCardWidth, (float) cardHeight, 6.0f);
    g.fillRoundedRectangle ((float) toneCardX, (float) cardY, (float) toneCardWidth, (float) cardHeight, 6.0f);

    paintCardTitle (g, "SPACE", spaceCardX, cardY);
    paintCardTitle (g, "TONE", toneCardX, cardY);
}

// DecayDisplay ----------------------------------------------------------------

RoomEditor::DecayDisplay::DecayDisplay (RoomProcessor& processor)
    : roomProcessor (processor)
{
    setTooltip ("The tail's level over time, worked out from the settings: the early reflections after Pre-delay (white), "
                "the overall decay (filled) and the decay of the highs (dashed), which falls faster with more Damping.");
}

RoomEditor::DecayDisplay::Shown RoomEditor::DecayDisplay::read() const
{
    Shown s;
    s.predelayMs = roomProcessor.getPredelayMs();
    s.size = (double) roomProcessor.read ("size") / 100.0;
    s.decay = (double) roomProcessor.read ("decay");
    s.damping = (double) roomProcessor.read ("damping") / 100.0;
    s.erGain = fancy::levelToGain (roomProcessor.read ("erlevel"), RoomProcessor::mixFloorDb);
    s.freeze = roomProcessor.isOn ("freeze");
    return s;
}

void RoomEditor::DecayDisplay::update()
{
    const auto now = read();

    if (! (now == shown))
    {
        shown = now;
        repaint();
    }
}

void RoomEditor::DecayDisplay::paint (juce::Graphics& g)
{
    const auto s = read();
    const auto width = (float) getWidth(), height = (float) getHeight();

    g.setColour (colours::bgSection);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    constexpr float left = 32.0f, top = 14.0f;
    const auto right = width - 14.0f, bottom = height - 24.0f;

    const auto pd = s.predelayMs * 0.001;
    const auto end = pd + s.decay;

    // 0 to 3 s, extended to show the decay with room for its label.
    constexpr double spans[][2] { { 3.0, 0.5 }, { 5.0, 1.0 }, { 10.0, 2.0 }, { 15.0, 3.0 }, { 20.0, 4.0 }, { 25.0, 5.0 }, { 30.0, 5.0 } };
    auto span = spans[0][0], step = spans[0][1];

    for (const auto& candidate : spans)
    {
        span = candidate[0];
        step = candidate[1];

        if (end * 1.15 <= span)
            break;
    }

    const auto xFor = [&] (double t) { return left + (right - left) * (float) (t / span); };
    const auto yFor = [&] (double db) { return top + (bottom - top) * (float) (-db / 60.0); };

    const auto scaleFont = font (Weight::regular, 9.0f);
    const auto legendFont = font (Weight::regular, 10.0f);

    // Grid and scales.
    g.setFont (scaleFont);

    for (int db = 0; db >= -60; db -= 12)
    {
        g.setColour (db == 0 ? colours::grey5 : colours::bgBody);
        g.drawHorizontalLine (juce::roundToInt (yFor (db)), left, right);
        g.setColour (colours::textSecondary);
        g.drawText (juce::String (db), juce::Rectangle<float> (0.0f, yFor (db) - 6.0f, left - 6.0f, 12.0f), juce::Justification::centredRight, false);
    }

    for (auto t = 0.0; t <= span + 1.0e-9; t += step)
    {
        const auto text = tickText (t);
        const auto w = textWidth (scaleFont, text) + 2.0f;
        const auto x = xFor (t);
        const auto textX = t == 0.0 ? x : (t + step > span + 1.0e-9 ? x - w : x - 0.5f * w);
        g.drawText (text, juce::Rectangle<float> (textX, height - 16.0f, w, 12.0f), juce::Justification::centredLeft, false);
    }

    // The overall decay, and the highs' (dashed): -2 dB at Pre-delay, then 60 dB per RT60.
    const auto slope = [&] (double rt60, juce::Path& path)
    {
        const auto reach = s.freeze ? span : pd + rt60 * 58.0 / 60.0;
        path.startNewSubPath (xFor (pd), yFor (-2.0));

        if (s.freeze)
        {
            path.lineTo (xFor (span), yFor (-2.0));
            return;
        }

        path.lineTo (xFor (std::min (reach, span)), yFor (reach <= span ? -60.0 : -2.0 - 60.0 * (span - pd) / rt60));

        if (reach < span)
            path.lineTo (xFor (span), yFor (-60.0));
    };

    juce::Path overall;
    slope (s.decay, overall);

    auto fill = overall;
    fill.lineTo (xFor (span), yFor (-60.0));
    fill.lineTo (xFor (pd), yFor (-60.0));
    fill.closeSubPath();
    g.setColour (purple.withAlpha (0.28f));
    g.fillPath (fill);

    g.setColour (purple);
    g.strokePath (overall, juce::PathStrokeType (2.0f));

    if (! s.freeze)
    {
        juce::Path highs, dashed;
        slope (s.decay * (1.0 - 0.7 * s.damping), highs);
        const float dashes[] { 4.0f, 3.0f };
        juce::PathStrokeType (1.4f).createDashedStroke (dashed, highs, dashes, 2);
        g.setColour (purple.withAlpha (0.9f));
        g.fillPath (dashed);
    }

    // Early reflections, as thin lines after Pre-delay.
    if (s.erGain > 0.0)
    {
        const auto erDb = 20.0 * std::log10 (s.erGain);
        g.setColour (lightInk.withAlpha (0.7f));

        for (int ch = 0; ch < 2; ++ch)
        {
            for (int k = 0; k < RoomDsp::erTaps; ++k)
            {
                const auto tap = RoomDsp::reflection (ch, k);
                const auto t = pd + tap.ms * RoomDsp::sizeScale (s.size) * 0.001;
                const auto db = juce::jlimit (-60.0, 0.0, 20.0 * std::log10 (std::abs (tap.gain) / 0.315) - 4.0 + erDb);
                const auto x = (float) juce::roundToInt (xFor (t)) + 0.5f;
                g.drawLine (x, yFor (-60.0), x, yFor (db), 1.0f);
            }
        }
    }

    // The decay marker and its label.
    g.setFont (legendFont);

    if (s.freeze)
    {
        g.setColour (lightInk);
        g.drawText ("frozen: the tail holds", juce::Rectangle<float> (xFor (pd) + 6.0f, yFor (-52.0) - 7.0f, 200.0f, 14.0f),
                    juce::Justification::centredLeft, false);
    }
    else
    {
        const auto x = xFor (end);
        const float markerDashes[] { 3.0f, 3.0f };
        g.setColour (colours::grey6);
        g.drawDashedLine (juce::Line<float> (x, top, x, bottom), markerDashes, 2, 1.0f);

        const auto label = "decay " + secondsText (s.decay) + juce::String::fromUTF8 (" (to \xe2\x88\x92" "60 dB)");
        const auto w = textWidth (legendFont, label) + 2.0f;
        const auto labelX = x + 6.0f + w <= right ? x + 6.0f : x - 6.0f - w;
        g.setColour (lightInk);
        g.drawText (label, juce::Rectangle<float> (labelX, yFor (-52.0) - 7.0f, w, 14.0f), juce::Justification::centredLeft, false);
    }

    // The legend, right-aligned and laid out from measured widths.
    struct Entry
    {
        const char* text;
        int kind;
    };

    const Entry entries[] { { "early reflections", 0 }, { "decay", 1 }, { "highs", 2 } };
    auto x = right;

    for (int i = 2; i >= 0; --i)
    {
        const auto w = textWidth (legendFont, entries[i].text) + 2.0f;
        const auto textX = x - w;
        const auto swatchRight = textX - 4.0f, swatchLeft = swatchRight - 14.0f;
        const auto y = top + 6.0f;

        g.setColour (colours::textSecondary);
        g.drawText (entries[i].text, juce::Rectangle<float> (textX, top, w, 12.0f), juce::Justification::centredLeft, false);

        if (entries[i].kind == 0)
        {
            g.setColour (lightInk);
            g.drawLine (swatchLeft, y, swatchRight, y, 1.0f);
        }
        else if (entries[i].kind == 1)
        {
            g.setColour (purple);
            g.drawLine (swatchLeft, y, swatchRight, y, 2.0f);
        }
        else
        {
            const float dashes[] { 4.0f, 3.0f };
            g.setColour (purple);
            g.drawDashedLine (juce::Line<float> (swatchLeft, y, swatchRight, y), dashes, 2, 1.0f);
        }

        x = swatchLeft - 12.0f;
    }
}

// RoomEditor ------------------------------------------------------------------

RoomEditor::RoomEditor (RoomProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::full),
      display (processor),
      predelayKnob (parameter ("predelay"), "Pre-delay", purple, Knob::tightStyle),
      noteKnob (parameter ("prenote"), "Pre-delay", purple, Knob::tightStyle),
      sizeKnob (parameter ("size"), "Size", purple, Knob::tightStyle),
      decayKnob (parameter ("decay"), "Decay", purple, Knob::tightStyle),
      diffusionKnob (parameter ("diffusion"), "Diffusion", purple, Knob::tightStyle),
      modKnob (parameter ("mod"), "Mod", purple, Knob::tightStyle),
      widthKnob (parameter ("width"), "Width", purple, Knob::tightStyle, true),
      dampingKnob (parameter ("damping"), "Damping", purple, Knob::tightStyle),
      lowCutKnob (parameter ("lowcut"), "Low cut", colours::uiBlue, Knob::tightStyle),
      highCutKnob (parameter ("highcut"), "High cut", colours::uiBlue, Knob::tightStyle),
      syncCheckbox (parameter ("sync"), "Sync pre-delay", purple),
      freezeCheckbox (parameter ("freeze"), "Freeze", purple),
      wetOnlyCheckbox (parameter ("wetonly"), "Wet only (send)", purple),
      editLink (purple),
      outMeter (outGeometry),
      wetFader (parameter ("wet"), colours::uiTeal),
      dryFader (parameter ("dry"), colours::uiTeal),
      qualityDropdown (parameter ("quality")),
      stereoDropdown (parameter ("stereoin")),
      duckingKnob (parameter ("ducking"), "Ducking", purple, Knob::smallStyle),
      erKnob (parameter ("erlevel"), "Early reflections", purple, Knob::smallStyle)
{
    auto& body = getBody();

    // Phase 2.1 W.1: room in the header for undo, redo, A/B and Compare.
    setShortHeaderLabels (true);
    setPresetDropdownWidth (120);

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (0, 0, baseWidth, bodyHeight);
    body.addAndMakeVisible (panels);

    display.setBounds (displayX, displayY, displayWidth, displayHeight);

    // Cards.
    cardKnobs ({ &predelayKnob, &sizeKnob, &decayKnob, &diffusionKnob, &modKnob, &widthKnob }, spaceCardX, spaceCardWidth, cardY + 30);
    cardKnobs ({ &dampingKnob, &lowCutKnob, &highCutKnob }, toneCardX, toneCardWidth, cardY + 30);
    noteKnob.setBounds (predelayKnob.getBounds());

    predelayKnob.setTooltip ("A gap before the reverb starts, so the dry sound stays clear in front of it.");
    sizeKnob.setTooltip ("How big the space is: the spacing of the early reflections and the network's delays. "
                         "Decay sets how long the tail lasts, whatever the Size.");
    decayKnob.setTooltip ("How long the tail takes to fall by 60 dB (RT60), at any Size.");
    diffusionKnob.setTooltip ("How quickly the echoes blur into a smooth tail. Lower keeps more distinct repeats at the start.");
    modKnob.setTooltip ("Slow, gentle movement of the delays inside the tail, which keeps it from ringing metallically.");
    widthKnob.setTooltip ("The reverb's stereo width: 0 is mono, 1 as it is, 2 wider, and below 0 swaps the sides. "
                          "The mono sum doesn't change.");
    dampingKnob.setTooltip ("How much faster the highs die away than the rest, as in a room with soft surfaces.");
    lowCutKnob.setTooltip ("Takes the lows out of the reverb, so it doesn't muddy the low end.");
    highCutKnob.setTooltip ("Takes the highs out of the reverb, for a darker, more distant sound.");
    widthKnob.setShowDefaultMark (true);

    // Footer.
    syncCheckbox.setBounds (leftX, footerY, 110, 20);
    freezeCheckbox.setBounds (leftX + 121, footerY, 62, 20);
    wetOnlyCheckbox.setBounds (leftX + 194, footerY, 114, 20);
    editLink.setBounds (contentRight - 64, footerY - 2, 64, 24);

    for (auto* checkbox : { &syncCheckbox, &freezeCheckbox, &wetOnlyCheckbox })
        checkbox->setLabelHeight (labelHeight);

    syncCheckbox.setTooltip ("Sets Pre-delay as a note length at the host's tempo (120 BPM without one), up to 250 ms.");
    freezeCheckbox.setTooltip ("Holds the tail as it is, for as long as Freeze is on. New playing still sounds through the early reflections.");
    wetOnlyCheckbox.setTooltip ("For a send: only the reverb comes out, whatever Dry is set to. Dry comes back when this is off.");

    // Right column, as Sature's.
    outMeter.setBounds (rightColumnX - 6, 10, 46, 350);
    setUpLabel (wetLabel, "Wet", juce::Justification::centred, 14.0f, colours::textPrimary);
    setUpLabel (dryLabel, "Dry", juce::Justification::centred, 14.0f, colours::textPrimary);
    wetLabel.setBounds (690, 10, 33, 17);
    dryLabel.setBounds (723, 10, 33, 17);
    wetFader.setBounds (690, faderTop, 33, faderHeight);
    dryFader.setBounds (723, faderTop, 33, faderHeight);
    wetFader.setTooltip ("The reverb's level.");
    dryFader.setTooltip ("The untouched signal's level.");

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &display, &predelayKnob, &sizeKnob, &decayKnob, &diffusionKnob, &modKnob, &widthKnob,
             &dampingKnob, &lowCutKnob, &highCutKnob, &syncCheckbox, &freezeCheckbox, &wetOnlyCheckbox, &editLink,
             &outMeter, &wetLabel, &dryLabel, &wetFader, &dryFader })
        body.addAndMakeVisible (item);

    body.addChildComponent (noteKnob);

    // EDIT drawer.
    drawer.setBounds (0, displayY - 4, rightColumnX, bodyHeight - displayY + 4);

    setUpLabel (qualityLabel, "Quality", juce::Justification::centredLeft, 14.0f, colours::textPrimary);
    setUpLabel (stereoLabel, "Stereo input", juce::Justification::centredLeft, 14.0f, colours::textPrimary);

    qualityLabel.setBounds (24, 60, 170, 24);
    qualityDropdown.setBounds (200, 60, 170, 24);
    stereoLabel.setBounds (24, 100, 170, 24);
    stereoDropdown.setBounds (200, 100, 170, 24);
    duckingKnob.setBounds (24, 140, 90, 99);
    erKnob.setBounds (130, 140, 120, 99);

    qualityDropdown.setTooltip ("Eco runs 8 delay lines, for the least CPU. Normal runs 16, and High adds smoother modulation. "
                                "On a change, the old tail rings out while the new one starts.");
    stereoDropdown.setTooltip ("Mono-sum feeds the reverb the sum of both channels, for a centred source on a stereo track.");
    duckingKnob.setTooltip ("Turns the reverb down while the dry signal plays, by up to 24 dB, and lets it bloom in the gaps.");
    erKnob.setTooltip ("The level of the early reflections, the first echoes that give the room its shape.");

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &qualityLabel, &qualityDropdown, &stereoLabel, &stereoDropdown, &duckingKnob, &erKnob })
        drawer.addAndMakeVisible (item);

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (! drawer.isVisible()); };

    updateStates();
    display.update();
}

RoomEditor::~RoomEditor() = default;

RoomProcessor& RoomEditor::roomProcessor() noexcept
{
    return dynamic_cast<RoomProcessor&> (getFancyProcessor());
}

void RoomEditor::updateStates()
{
    auto& processor = roomProcessor();

    // Sync pre-delay swaps the ms knob for the note knob in the same place (decision 5).
    const auto sync = processor.isOn ("sync");
    predelayKnob.setVisible (! sync);
    noteKnob.setVisible (sync);

    // A capped note keeps its text, in red, and the tooltip explains (docs/room.md, Phase 4).
    const auto capped = processor.isPredelayCapped();
    const auto note = RoomProcessor::noteName (juce::roundToInt (processor.read ("prenote")));
    const auto tip = capped ? note + " is " + juce::String (juce::roundToInt (RoomProcessor::noteMs (juce::roundToInt (processor.read ("prenote")), processor.getBpm())))
                                  + " ms at this tempo; Pre-delay stops at 250 ms."
                            : juce::String ("Pre-delay as a note length at the host's tempo.");

    if (capped != lastCapped || tip != lastNoteTip)
    {
        lastCapped = capped;
        lastNoteTip = tip;
        noteKnob.setValueColour (capped ? colours::uiRed : purple);
        noteKnob.setTooltip (tip);
        noteKnob.repaint();
    }

    // Decision 1: Dry is off while Wet only is on, and keeps its value.
    const auto wetOnly = processor.isOn ("wetonly");

    if (dryFader.isLocked() != wetOnly)
    {
        dryFader.setLocked (wetOnly, "Off while Wet only (send) is on");
        dryFader.setAlpha (wetOnly ? 0.5f : 1.0f);
    }
}

void RoomEditor::onTick (float seconds)
{
    updateStates();
    display.update();
    outMeter.update (roomProcessor().outputMeter, seconds);
}

void RoomEditor::showPreview (bool drawerOpen)
{
    stopUpdates();

    setValue (parameter ("predelay"), 12.0f);
    setValue (parameter ("size"), 55.0f);
    setValue (parameter ("decay"), 1.8f);
    setValue (parameter ("diffusion"), 70.0f);
    setValue (parameter ("mod"), 25.0f);
    setValue (parameter ("width"), 1.0f);
    setValue (parameter ("damping"), 45.0f);
    setValue (parameter ("lowcut"), 150.0f);
    setValue (parameter ("highcut"), 9000.0f);
    setValue (parameter ("wet"), -8.0f);
    setValue (parameter ("dry"), 0.0f);

    updateStates();
    display.update();
    outMeter.show (-11.2f, -10.8f);

    drawer.setVisible (drawerOpen);
}

juce::Colour RoomEditor::getAccentColour()
{
    return purple;
}

std::vector<ShellEditor::PopupOpener> RoomEditor::getPopupOpeners()
{
    auto openers = ShellEditor::getPopupOpeners();
    openers.push_back ({ "dropdown-quality", [this] { drawer.setVisible (true); qualityDropdown.click(); } });
    openers.push_back ({ "tooltip-freeze", [this] { showTip (freezeCheckbox, freezeCheckbox.getTooltip()); } });
    openers.push_back ({ "value-decay", [this] { decayKnob.openValueEditor(); } });
    return openers;
}
