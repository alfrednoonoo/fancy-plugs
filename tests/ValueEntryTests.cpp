#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/plugin/ParamHelpers.h>
#include <fancy/ui/Controls.h>

#include <cmath>
#include <string>

using Catch::Approx;
using fancy::ui::Fader;
using fancy::ui::Knob;

namespace
{
std::unique_ptr<juce::AudioParameterFloat> frequencyParameter()
{
    return fancy::floatParameter ("freq", "Freq", fancy::logRange (20.0f, 20000.0f, 0.1f), 1234.5f,
                                  juce::AudioParameterFloatAttributes {}
                                      .withStringFromValueFunction ([] (float value, int) { return fancy::formatHz (value); })
                                      .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseHz (text); }));
}

// Gestures go straight to the control under the mouse. Clicks away have to take the real
// route, because the box watches for them on the desktop, so those go through the window.
struct Rig
{
    Rig()
    {
        window.setBounds (120, 120, 420, 260);
        knob.setBounds (10, 10, 100, 130);
        fader.setBounds (130, 10, 44, 200);
        headerKnob.setBounds (200, 10, 28, 28);

        window.addAndMakeVisible (knob);
        window.addAndMakeVisible (fader);
        window.addAndMakeVisible (headerKnob);

        // An unpainted window that isn't opaque is layered, and Windows can't hit-test it.
        window.setOpaque (true);
        window.setAlwaysOnTop (true);
        window.addToDesktop (0);
        window.setVisible (true);
    }

    enum class Phase { down, drag, up };

    void send (juce::Component& target, Phase phase, juce::Point<int> pressed, juce::Point<int> position, juce::ModifierKeys held, bool dragged)
    {
        const auto mods = phase == Phase::up ? held : held.withFlags (juce::ModifierKeys::leftButtonModifier);
        const auto now = juce::Time::getCurrentTime();
        const auto local = target.getLocalPoint (&window, position).toFloat();
        const auto localPressed = target.getLocalPoint (&window, pressed).toFloat();

        const juce::MouseEvent event (juce::Desktop::getInstance().getMainMouseSource(), local, mods,
                                      juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation,
                                      juce::MouseInputSource::defaultRotation, juce::MouseInputSource::defaultTiltX,
                                      juce::MouseInputSource::defaultTiltY, &target, &target, now, localPressed, now, 1, dragged);

        if (phase == Phase::down)
            target.mouseDown (event);
        else if (phase == Phase::drag)
            target.mouseDrag (event);
        else
            target.mouseUp (event);
    }

    void click (juce::Point<int> position, juce::ModifierKeys held = {})
    {
        auto* target = window.getComponentAt (position);
        REQUIRE (target != nullptr);

        send (*target, Phase::down, position, position, held, false);
        send (*target, Phase::up, position, position, held, false);
    }

    void drag (juce::Point<int> from, juce::Point<int> to, bool farEnoughToBeADrag)
    {
        auto* target = window.getComponentAt (from);
        REQUIRE (target != nullptr);

        send (*target, Phase::down, from, from, {}, false);
        send (*target, Phase::drag, from, to, {}, farEnoughToBeADrag);
        send (*target, Phase::up, from, to, {}, farEnoughToBeADrag);
    }

    void clickThroughWindow (juce::Point<int> position)
    {
        auto* peer = window.getPeer();

        if (! peer->contains (position, true))
            SKIP ("This desktop doesn't deliver clicks to the test window");

        const auto send = [&] (juce::ModifierKeys mods)
        {
            peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse, position.toFloat(), mods,
                                    juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation,
                                    juce::Time::currentTimeMillis());
        };

        send ({});
        send (juce::ModifierKeys {}.withFlags (juce::ModifierKeys::leftButtonModifier));
        send ({});
    }

    juce::TextEditor* box()
    {
        for (auto* child : window.getChildren())
            if (auto* editor = dynamic_cast<juce::TextEditor*> (child); editor != nullptr && editor->isVisible())
                return editor;

        return nullptr;
    }

    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    std::unique_ptr<juce::AudioParameterFloat> freq = frequencyParameter();
    std::unique_ptr<juce::AudioParameterFloat> level = fancy::levelParameter ("level", "Level", -60.0f, 12.0f, 0.0f, -60.0f);
    juce::Component window;
    Knob knob { *freq, "Freq", juce::Colours::teal, Knob::mediumStyle };
    Fader fader { *level, juce::Colours::teal };
    Knob headerKnob { *level, "", juce::Colours::teal, Knob::headerStyle };

    const juce::Point<int> knobValue { 60, 115 };
    const juce::Point<int> knobFace { 60, 70 };
    const juce::Point<int> faderValue { 152, 200 };
    const juce::Point<int> faderTrack { 152, 80 };
    const juce::Point<int> emptySpace { 350, 200 };
};
} // namespace

TEST_CASE ("Typed numbers keep their value through units, signs and separators", "[entry]")
{
    using fancy::parseNumber;

    CHECK (parseNumber ("2500") == 2500.0f);
    CHECK (parseNumber ("2.5") == 2.5f);
    CHECK (parseNumber ("2,5") == 2.5f);
    CHECK (parseNumber ("2,500") == 2500.0f);
    CHECK (parseNumber ("1,234.5") == 1234.5f);
    CHECK (parseNumber ("0,125") == 0.125f);
    CHECK (parseNumber (".5") == 0.5f);
    CHECK (parseNumber (",5") == 0.5f);
    CHECK (parseNumber ("-6 dB") == -6.0f);
    CHECK (parseNumber ("-.5") == -0.5f);
    CHECK (parseNumber (juce::String::charToString ((juce::juce_wchar) 0x2212) + "6") == -6.0f);
    CHECK (parseNumber ("  12 ms") == 12.0f);
    CHECK (parseNumber ("50 %") == 50.0f);
    CHECK (parseNumber ("1.2.3") == 1.2f);
    CHECK (parseNumber ("abc") == 0.0f);
    CHECK (parseNumber ("") == 0.0f);
}

TEST_CASE ("Frequencies can be typed in Hz or kHz", "[entry]")
{
    using fancy::parseHz;

    CHECK (parseHz ("2500") == 2500.0f);
    CHECK (parseHz ("2500 Hz") == 2500.0f);
    CHECK (parseHz ("2.5k") == 2500.0f);
    CHECK (parseHz ("2.5K") == 2500.0f);
    CHECK (parseHz ("2.5 kHz") == 2500.0f);
    CHECK (parseHz ("2.5KHZ") == 2500.0f);
    CHECK (parseHz ("2,5k") == 2500.0f);
    CHECK (parseHz ("2,500") == 2500.0f);
    CHECK (parseHz (".5k") == 500.0f);
    CHECK (parseHz ("50") == 50.0f);
}

TEST_CASE ("Ratios can be typed either way round", "[entry]")
{
    constexpr float infinite = 1000.0f;

    CHECK (fancy::parseRatio ("4", infinite) == 4.0f);
    CHECK (fancy::parseRatio ("4:1", infinite) == 4.0f);
    CHECK (fancy::parseRatio ("1:4", infinite) == 4.0f);
    CHECK (fancy::parseRatio ("2.5:1", infinite) == 2.5f);
    CHECK (fancy::parseRatio ("inf:1", infinite) == infinite);
    CHECK (fancy::parseRatio ("inf", infinite) == infinite);
}

TEST_CASE ("Text without a value is told apart from text with one", "[entry]")
{
    CHECK (fancy::isValueText ("0"));
    CHECK (fancy::isValueText ("-inf dB"));
    CHECK (fancy::isValueText ("2.5k"));
    CHECK_FALSE (fancy::isValueText (""));
    CHECK_FALSE (fancy::isValueText ("abc"));
    CHECK_FALSE (fancy::isValueText ("-"));
}

TEST_CASE ("Frequencies outside a log range stop at its limits", "[entry]")
{
    const auto owned = frequencyParameter();
    const juce::RangedAudioParameter& parameter = *owned;

    for (const auto* text : { "-100", "0", "1", "50k", "99999999999999999999" })
    {
        const auto normalised = parameter.getValueForText (text);

        REQUIRE (std::isfinite (normalised));
        CHECK ((normalised == 0.0f || normalised == 1.0f));
    }
}

TEST_CASE ("Clicking a value opens the box and typing moves the control", "[entry]")
{
    Rig rig;

    SECTION ("Enter applies")
    {
        rig.click (rig.knobValue);
        REQUIRE (rig.box() != nullptr);

        rig.box()->setText ("2.5k", false);
        rig.box()->onReturnKey();

        CHECK (rig.freq->get() == Approx (2500.0f).margin (0.1f));
        CHECK (rig.box() == nullptr);
    }

    SECTION ("Clicking on empty space applies")
    {
        rig.click (rig.knobValue);
        REQUIRE (rig.box() != nullptr);

        rig.box()->setText ("2500", false);
        rig.clickThroughWindow (rig.emptySpace);

        CHECK (rig.freq->get() == Approx (2500.0f).margin (0.1f));
        CHECK (rig.box() == nullptr);
    }

    SECTION ("Clicking another control applies")
    {
        rig.click (rig.knobValue);
        REQUIRE (rig.box() != nullptr);

        rig.box()->setText ("500", false);
        rig.clickThroughWindow (rig.faderTrack);

        CHECK (rig.freq->get() == Approx (500.0f).margin (0.1f));
        CHECK (rig.box() == nullptr);
    }

    SECTION ("Losing focus applies")
    {
        rig.click (rig.knobValue);
        REQUIRE (rig.box() != nullptr);

        rig.box()->setText ("500", false);
        rig.box()->onFocusLost();

        CHECK (rig.freq->get() == Approx (500.0f).margin (0.1f));
    }

    SECTION ("Applying twice only applies once")
    {
        rig.click (rig.knobValue);
        REQUIRE (rig.box() != nullptr);

        auto* box = rig.box();
        box->setText ("500", false);
        box->onReturnKey();
        rig.freq->setValueNotifyingHost (rig.freq->convertTo0to1 (800.0f));
        box->onFocusLost();

        CHECK (rig.freq->get() == Approx (800.0f).margin (0.1f));
    }

    SECTION ("Escape cancels")
    {
        const auto before = rig.freq->get();

        rig.click (rig.knobValue);
        REQUIRE (rig.box() != nullptr);

        rig.box()->setText ("500", false);
        rig.box()->onEscapeKey();

        CHECK (rig.freq->get() == before);
        CHECK (rig.box() == nullptr);
    }

    SECTION ("Text with no value leaves the control alone")
    {
        const auto before = rig.freq->get();
        const std::string text = GENERATE (as<std::string> {}, "abc", "", "-");

        rig.click (rig.knobValue);
        REQUIRE (rig.box() != nullptr);

        rig.box()->setText (juce::String (text), false);
        rig.box()->onReturnKey();

        CHECK (rig.freq->get() == before);
    }

    SECTION ("Unchanged text doesn't round the value")
    {
        const auto before = rig.freq->get();

        rig.click (rig.knobValue);
        REQUIRE (rig.box() != nullptr);
        CHECK (rig.box()->getText() == "1.23 kHz");

        rig.clickThroughWindow (rig.emptySpace);

        CHECK (rig.freq->get() == before);
    }

    SECTION ("Numbers past the limits stop there")
    {
        rig.click (rig.knobValue);
        REQUIRE (rig.box() != nullptr);

        rig.box()->setText ("50k", false);
        rig.box()->onReturnKey();

        CHECK (rig.freq->get() == 20000.0f);
    }
}

TEST_CASE ("Faders take typed values the same way", "[entry]")
{
    Rig rig;

    SECTION ("Enter applies, with or without a unit")
    {
        rig.click (rig.faderValue);
        REQUIRE (rig.box() != nullptr);

        rig.box()->setText ("-6", false);
        rig.box()->onReturnKey();

        CHECK (rig.level->get() == Approx (-6.0f));
    }

    SECTION ("Clicking away applies, and the box fits more than the fader's width")
    {
        rig.click (rig.faderValue);
        REQUIRE (rig.box() != nullptr);
        CHECK (rig.box()->getWidth() > rig.fader.getWidth());

        rig.box()->setText ("-12.5 dB", false);
        rig.clickThroughWindow (rig.emptySpace);

        CHECK (rig.level->get() == Approx (-12.5f));
    }

    SECTION ("-inf reaches the floor")
    {
        rig.click (rig.faderValue);
        REQUIRE (rig.box() != nullptr);

        rig.box()->setText ("-inf", false);
        rig.box()->onReturnKey();

        CHECK (rig.level->get() == -60.0f);
    }
}

TEST_CASE ("Other gestures keep working beside the value box", "[entry]")
{
    Rig rig;

    SECTION ("Clicking the knob's face doesn't open the box")
    {
        rig.click (rig.knobFace);
        CHECK (rig.box() == nullptr);
    }

    SECTION ("Clicking the fader's track doesn't open the box")
    {
        rig.click (rig.faderTrack);
        CHECK (rig.box() == nullptr);
    }

    SECTION ("Dragging from the value turns the knob")
    {
        const auto before = rig.freq->get();

        rig.drag (rig.knobValue, rig.knobValue.translated (0, -40), true);

        CHECK (rig.box() == nullptr);
        CHECK (rig.freq->get() > before);
    }

    SECTION ("A click that wobbles doesn't move the knob")
    {
        const auto before = rig.freq->get();

        rig.drag (rig.knobValue, rig.knobValue.translated (0, -1), false);

        CHECK (rig.box() != nullptr);
        CHECK (rig.freq->get() == before);
    }

    SECTION ("Ctrl-click opens the box from the knob's face")
    {
        rig.click (rig.knobFace, juce::ModifierKeys { juce::ModifierKeys::ctrlModifier });
        CHECK (rig.box() != nullptr);
    }

    SECTION ("A knob with no value text needs ctrl-click, and gets a box it can show text in")
    {
        const juce::Point<int> position { 214, 24 };

        rig.click (position);
        CHECK (rig.box() == nullptr);

        rig.click (position, juce::ModifierKeys { juce::ModifierKeys::ctrlModifier });
        REQUIRE (rig.box() != nullptr);
        CHECK (rig.box()->getWidth() > rig.headerKnob.getWidth());
        CHECK (rig.box()->getHeight() >= 22);
    }
}
