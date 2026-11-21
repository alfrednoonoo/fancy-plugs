#pragma once

#include <fancy/ui/Style.h>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

// The EDIT drawer's view switch and its three small graphs (Phase 3 Q2.5, docs/comp.md
// section 6, reference/phase3/23__comp_q25_mockup.png).

// Detector | Character | Upward | Options | History. Styled as ParamSegments, but a view switch,
// not a parameter. Other names make other view switches, such as the history's zoom.
class DrawerTabs final : public juce::Component
{
public:
    enum Tab { detector, character, upward, options, history, numTabs };

    DrawerTabs();
    explicit DrawerTabs (juce::StringArray names);

    void setSelected (int tab);
    int getSelected() const noexcept { return selected; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void (int)> onChange;

private:
    juce::Rectangle<float> tabBounds (int index) const;

    juce::StringArray names;
    int selected = detector;
};

// The history page's read-out (docs/comp.md, Phase 4 Wave 1, section 5): the largest reduction
// in view, in the GR meter's yellow, or "paused" while the history is. A click calls onClick.
class HistoryCaption final : public juce::Component
{
public:
    void setText (const juce::String& newText, bool isPaused);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void()> onClick;

private:
    juce::String text;
    bool paused = false;
};

// Reads parameter values in their own units.
struct CompParameters
{
    std::function<juce::RangedAudioParameter& (const juce::String&)> find;

    float get (const juce::String& id) const;
    void set (const juce::String& id, float value) const;
    void begin (const juce::String& id) const;
    void end (const juce::String& id) const;
};

// The detector's response: Highpass and Lowpass (dimmer) and the three bands. The handles drag a
// band's Freq and Gain; the wheel sets the bell's Q. It only draws the detector: it never changes
// the audio path.
class DetectorCurve final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit DetectorCurve (CompParameters parameters);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    float xFor (double hz) const;
    double hzFor (float x) const;
    float yFor (double db) const;
    double dbFor (float y) const;
    juce::Point<float> handleFor (int band) const;
    int bandAt (juce::Point<float> position) const;

    CompParameters params;
    int dragging = -1;
};

// Character's transfer curve f(d·x)/d for the current Type and Drive, input against output in
// dBFS, with the unity line dashed.
class CharacterCurve final : public juce::Component
{
public:
    explicit CharacterCurve (CompParameters parameters);

    void paint (juce::Graphics&) override;

private:
    CompParameters params;
};

// The whole static curve, downward and upward, with Threshold, Up threshold and Floor marked,
// and a dot at the current detector level and its gain.
class UpwardCurve final : public juce::Component
{
public:
    explicit UpwardCurve (CompParameters parameters);

    void setCurrent (float levelDb, float liftDb, float autoOffsetDb);
    void paint (juce::Graphics&) override;

private:
    CompParameters params;
    float level = -200.0f, lift = 0.0f, offset = 0.0f;
};
