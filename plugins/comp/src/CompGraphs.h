#pragma once

#include <fancy/ui/Controls.h>
#include <fancy/ui/Style.h>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

// The EDIT drawer's view switch and its three small graphs (Phase 3 Q2.5, docs/comp.md
// section 6, reference/phase3/23__comp_q25_mockup.png).

// Detector | Character | Upward | Options | History. Styled as ParamSegments, but a view switch,
// not a parameter. Other names make other view switches, such as the history's zoom.
// Its drawing and clicks are fancy::ui::ViewTabs (moved to fancy-core unchanged).
class DrawerTabs final : public fancy::ui::ViewTabs
{
public:
    // Phase 4 Wave 3 (docs/comp.md, "History on the main page", decision 1 A): History left the
    // drawer for the main page.
    enum Tab { detector, character, upward, options, numTabs };

    DrawerTabs();
    explicit DrawerTabs (juce::StringArray names);
};

// The history page's read-out (docs/comp.md, Phase 4 Wave 1, section 5): the largest reduction
// in view, in the GR meter's yellow, or "paused" while the history is. A click calls onClick.
class HistoryCaption final : public juce::Component, public juce::SettableTooltipClient
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

// The detector's response: Highpass and Lowpass (dimmer) and the six bands (Phase 4 Wave 2,
// reference/phase4/55__comp_w2_sceq_mockup.png). Band tabs 1 to 6 in the top-left corner pick the
// band edited under the curve; each band that isn't Off has a handle that drags its Freq and Gain,
// and the wheel sets the selected Bell's Q. It only draws the detector: it never changes the audio.
class DetectorCurve final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit DetectorCurve (CompParameters parameters);

    void setSelected (int band);
    int getSelected() const noexcept { return selected; }
    std::function<void (int)> onSelect;

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
    int bandType (int band) const;
    juce::Rectangle<float> tabBounds (int band) const;

    CompParameters params;
    int dragging = -1;
    int selected = 0;
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
