#include "DsMiniEditor.h"

#include <fancy/plugin/ParamFormat.h>

#include <cmath>

using namespace fancy::ui;

namespace
{
// design/phase3-mockup-src/dsmini.html at 100 %: the compact window, as Kicker's.
constexpr int baseWidth = 392;
constexpr int bodyHeight = 419;
constexpr int baseHeight = ShellEditor::headerHeight + bodyHeight;
constexpr int leftWidth = 276;

constexpr int displayX = 19, displayY = 71, displayWidth = 238, displayHeight = 92;
constexpr int footerY = 388;

constexpr float historyFloorDb = -60.0f, historyReductionDb = 24.0f;

const juce::Colour miniBlue = colours::uiBlue;

constexpr GainReductionMeter::Geometry gainGeometry { 13.0f, 23.0f, 330.0f, true, "GR" };
constexpr OutMeter::Geometry outGeometry { 9.0f, 23.0f, 353.0f, 33.0f, 5.33f, 361.0f, false, true };

void setValue (juce::RangedAudioParameter& parameter, float value)
{
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}

void setUpLabel (juce::Label& label, const juce::String& text, juce::Justification justification, float height, juce::Colour colour)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (font (Weight::regular, height));
    label.setColour (juce::Label::textColourId, colour);
    label.setJustificationType (justification);
    label.setBorderSize ({});
    label.setInterceptsMouseClicks (false, false);
}

// "4.9", "5", "12": kHz with one decimal, without a trailing ".0".
juce::String kilohertz (double hz)
{
    auto text = fancy::formatFixed ((float) (hz / 1000.0), 1);
    return text.endsWith (".0") ? text.dropLastCharacters (2) : text;
}
} // namespace

juce::String DsMiniEditor::captionFor (int profileIndex)
{
    const auto& profile = DsMiniProcessor::profiles[(size_t) juce::jlimit (0, (int) DsMiniProcessor::profiles.size() - 1, profileIndex)];
    const auto dot = juce::String::fromUTF8 (" \xc2\xb7 ");

    if (profile.shelf)
        return juce::String (profile.caption) + dot + "above " + kilohertz (profile.freqHz) + " kHz";

    // Width marks where the cut is half its depth, an octave apart at 1.0 oct.
    const auto half = std::pow (2.0, 0.5 * (double) profile.widthOctaves);

    return juce::String (profile.caption) + dot + kilohertz (profile.freqHz / half) + juce::String::fromUTF8 ("\xe2\x80\x93")
         + kilohertz (profile.freqHz * half) + " kHz";
}

// Panels ----------------------------------------------------------------------

void DsMiniEditor::Panels::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgBody);
    g.setColour (colours::bgSection);
    g.fillRect (leftWidth, 0, baseWidth - leftWidth, bodyHeight);
}

// Display ---------------------------------------------------------------------

DsMiniEditor::Display::Display (DsMiniProcessor& processor)
    : miniProcessor (processor)
{
    setTooltip ("The \"s\" level (grey) and how much is turned down (blue) over the last 3 s. "
                "The dashed line is where turning down starts; Amount moves it.");
}

void DsMiniEditor::Display::resized()
{
    history.setWidth (getWidth(), miniProcessor.getEngine());
    miniProcessor.setHistoryColumns (getWidth());
}

void DsMiniEditor::Display::update()
{
    if (history.isFrozen())
        return;

    history.update (miniProcessor.getEngine());
    repaint();
}

void DsMiniEditor::Display::showColumns (std::vector<fancy::dsp::DeEsser::HistoryColumn> columns)
{
    history.show (std::move (columns));
    repaint();
}

void DsMiniEditor::Display::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto targets = miniProcessor.targetsFor (miniProcessor.getProfile());

    g.setColour (colours::bgSection);
    g.fillRoundedRectangle (bounds, 6.0f);

    g.saveState();
    g.reduceClipRegion (getLocalBounds());
    history.paint (g, bounds.getWidth(), bounds.getHeight(), (float) targets.thresholdDb,
                   { miniBlue, juce::String::fromUTF8 ("\xe2\x80\x9cs\xe2\x80\x9d level \xc2\xb7 reduction \xc2\xb7 where it starts"),
                     true, historyFloorDb, historyReductionDb });
    g.restoreState();
}

// DsMiniEditor ----------------------------------------------------------------

DsMiniEditor::DsMiniEditor (DsMiniProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::compact),
      profileDropdown (parameter ("profile")),
      display (processor),
      amountKnob (parameter ("amount"), "Amount", miniBlue, Knob::largeStyle),
      listenCheckbox (processor, "Listen", miniBlue),
      editLink (miniBlue),
      gainMeter (gainGeometry),
      outMeter (outGeometry),
      modeSegments (parameter ("mode"), miniBlue, colours::bgWell),
      lookaheadDropdown (parameter ("lookahead")),
      linkCheckbox (parameter ("link"), "Link channels", miniBlue),
      saveAsDsButton (juce::String::fromUTF8 ("Save as Fancy DS preset\xe2\x80\xa6"))
{
    auto& body = getBody();

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (body.getLocalBounds());
    body.addAndMakeVisible (panels);

    // Left: the profile, its band, the history and Amount.
    profileDropdown.setStepArrows (true);
    profileDropdown.setBounds (18, 14, 240, 26);
    profileDropdown.setTooltip ("What you're de-essing. The arrows step through the profiles.");

    setUpLabel (captionLabel, {}, juce::Justification::centred, 12.0f, colours::textSecondary);
    captionLabel.setBounds (0, 46, leftWidth, 16);

    display.setBounds (displayX, displayY, displayWidth, displayHeight);

    amountKnob.setBounds ((leftWidth - 132) / 2, 182, 132, 172);
    amountKnob.setShowDefaultMark (true);
    amountKnob.setTooltip ("How much it de-esses. 0 does nothing, 5 is moderate, 10 is strong.");

    // Footer.
    listenCheckbox.setBounds (14, footerY, 90, 20);
    listenCheckbox.setLabelHeight (13.0f);
    listenCheckbox.setTooltip ("Plays only what's being taken away. Turns off when you close this window, and is never saved.");
    editLink.setBounds (leftWidth - 14 - 64, footerY - 2, 64, 24);

    // Right column.
    gainMeter.setBounds (leftWidth + 18, 14, 34, 380);
    gainMeter.setBarColour (miniBlue);
    outMeter.setBounds (leftWidth + 54, 14, 56, 390);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &profileDropdown, &captionLabel, &display, &amountKnob, &listenCheckbox, &editLink, &gainMeter, &outMeter })
        body.addAndMakeVisible (item);

    // EDIT drawer.
    drawer.setBounds (0, 0, leftWidth, bodyHeight);

    setUpLabel (modeLabel, "Mode", juce::Justification::centredLeft, 14.0f, colours::textSecondary);
    setUpLabel (lookaheadLabel, "Lookahead", juce::Justification::centredLeft, 14.0f, colours::textSecondary);
    modeLabel.setBounds (20, 60, 100, 24);
    modeSegments.setBounds (124, 60, 132, 24);
    modeSegments.setTooltip ("Split turns down only the band. Wide turns down the whole signal.");
    lookaheadLabel.setBounds (20, 100, 100, 24);
    lookaheadDropdown.setBounds (124, 100, 132, 24);
    lookaheadDropdown.setTooltip ("Starts turning down 2 ms before an \"s\" arrives, so the fastest onsets are caught.");
    addLatencyNote (lookaheadDropdown);
    linkCheckbox.setBounds (20, 140, 200, 20);
    linkCheckbox.setTooltip ("Both channels are turned down together, on the louder one.");

    saveAsDsButton.setBounds (20, 190, 236, 24);
    saveAsDsButton.onClick = [this] { showSaveAsDsPreset(); };
    setUpLabel (saveAsDsNote, "Opens in Fancy DS with every setting this profile and Amount use.",
                juce::Justification::topLeft, 12.0f, colours::textSecondary);
    saveAsDsNote.setBounds (20, 222, 236, 34);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &modeLabel, &modeSegments, &lookaheadLabel, &lookaheadDropdown, &linkCheckbox, &saveAsDsButton, &saveAsDsNote })
        drawer.addAndMakeVisible (item);

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (! drawer.isVisible()); };

    onTick (0.0f);
}

DsMiniProcessor& DsMiniEditor::miniProcessor() noexcept
{
    return static_cast<DsMiniProcessor&> (getFancyProcessor());
}

void DsMiniEditor::onTick (float seconds)
{
    auto& processor = miniProcessor();
    const auto profile = processor.getProfile();

    if (profile != shownProfile)
    {
        shownProfile = profile;
        captionLabel.setText (captionFor (profile), juce::dontSendNotification);
    }

    const auto reduction = juce::jmin (processor.takeReduction(), 60.0f);
    gainMeter.update (reduction, reduction, seconds);
    outMeter.update (processor.outputMeter, seconds);
    display.update();
}

void DsMiniEditor::showSaveAsDsPreset()
{
    DialogSpec spec;
    spec.title = "Save as Fancy DS preset";
    spec.action = "Save";
    spec.fields.push_back ({ "Name", miniProcessor().getPresets().getCurrentName() });
    spec.validate = [] (const juce::StringArray& values)
    {
        return juce::File::createLegalFileName (values[0].trim()).isEmpty() ? juce::String ("Type a name.") : juce::String();
    };
    spec.note = [] (const juce::StringArray&)
    {
        return juce::String ("In Fancy DS's preset browser, under \"") + DsMiniProcessor::dsPresetFolder + "\".";
    };

    showDialog (getPopupParent(), std::move (spec), [safeThis = juce::Component::SafePointer<DsMiniEditor> (this)] (bool accepted, const juce::StringArray& values)
    {
        if (safeThis == nullptr || ! accepted)
            return;

        if (! safeThis->miniProcessor().saveAsDsPreset (values[0]))
        {
            DialogSpec failed;
            failed.title = "Couldn't save the preset";
            failed.body = "Fancy DS's preset folder couldn't be written.";
            failed.showCancel = false;
            showDialog (safeThis->getPopupParent(), std::move (failed));
        }
    });
}

void DsMiniEditor::showPreview (bool drawerOpen)
{
    stopUpdates();

    setValue (parameter ("profile"), 0.0f);
    setValue (parameter ("amount"), 6.0f);
    shownProfile = -1;
    captionLabel.setText (captionFor (0), juce::dontSendNotification);

    // The mockup's history: "s" bursts every 64 px over a quiet voice, turned down above where
    // it starts (-24 dB at Amount 6).
    std::vector<fancy::dsp::DeEsser::HistoryColumn> columns;
    juce::Random random (4);

    for (int x = 0; x < displayWidth; ++x)
    {
        const auto t = (x + 30) % 64;
        const auto levelDb = t < 10 ? -10.0f - 4.0f * random.nextFloat() : -46.0f - 6.0f * random.nextFloat();
        columns.push_back ({ levelDb, juce::jmax (0.0f, levelDb + 24.0f) * 0.4f });
    }

    display.showColumns (std::move (columns));

    gainMeter.show (4.8f, 4.8f);
    outMeter.show (-9.2f, -8.9f);

    drawer.setVisible (drawerOpen);
}

juce::Colour DsMiniEditor::getAccentColour()
{
    return miniBlue;
}

std::vector<ShellEditor::PopupOpener> DsMiniEditor::getPopupOpeners()
{
    auto openers = ShellEditor::getPopupOpeners();
    openers.push_back ({ "dropdown-profile", [this] { profileDropdown.click(); } });
    openers.push_back ({ "dropdown-lookahead", [this] { drawer.setVisible (true); lookaheadDropdown.click(); } });
    openers.push_back ({ "dialog-save-as-ds", [this] { drawer.setVisible (true); showSaveAsDsPreset(); } });
    openers.push_back ({ "tooltip-amount", [this] { showTip (amountKnob, amountKnob.getTooltip()); } });
    openers.push_back ({ "value-amount", [this] { amountKnob.openValueEditor(); } });
    return openers;
}
