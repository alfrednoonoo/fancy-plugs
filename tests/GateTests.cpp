#include "GateDsp.h"
#include "GateEditor.h"
#include "GateProcessor.h"

#if FANCY_HAS_COMP
 #include "CompDsp.h"
#endif

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/Decibels.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

using Catch::Approx;

namespace
{
constexpr double pi = std::numbers::pi;
constexpr double rate = 48000.0;
constexpr int lookaheadSamples = 2400;

struct Event
{
    std::int64_t time;
    juce::MidiMessage message;
};

struct Audio
{
    std::vector<float> left, right;
    std::vector<Event> events;

    explicit Audio (int numSamples, float value = 0.0f)
        : left ((size_t) numSamples, value), right ((size_t) numSamples, value) {}

    int size() const { return (int) left.size(); }

    void fill (int from, int to, float value)
    {
        std::fill (left.begin() + from, left.begin() + to, value);
        std::fill (right.begin() + from, right.begin() + to, value);
    }

    // A hit: a cosine burst at full amplitude from its first sample.
    void hit (int at, int length, double amplitude, double hz = 440.0)
    {
        for (int i = 0; i < length && at + i < size(); ++i)
            left[(size_t) (at + i)] = right[(size_t) (at + i)] = (float) (amplitude * std::cos (2.0 * pi * hz * i / rate));
    }

    void process (GateDsp& dsp, const Audio* key = nullptr, int blockSize = 256, std::int64_t timeOffset = 0)
    {
        for (int start = 0; start < size(); start += blockSize)
        {
            const auto count = std::min (blockSize, size() - start);
            float* main[] { left.data() + start, right.data() + start };
            const float* keyChannels[2] {};

            if (key != nullptr)
            {
                keyChannels[0] = key->left.data() + start;
                keyChannels[1] = key->right.data() + start;
            }

            juce::MidiBuffer midi;
            dsp.process (main, 2, keyChannels, key != nullptr ? 2 : 0, count, &midi);

            for (const auto metadata : midi)
                events.push_back ({ timeOffset + start + metadata.samplePosition, metadata.getMessage() });
        }
    }

    std::vector<Event> noteOns() const
    {
        std::vector<Event> found;

        for (const auto& event : events)
            if (event.message.isNoteOn())
                found.push_back (event);

        return found;
    }
};

Audio sine (double hz, double amplitude, int numSamples, double sampleRate = rate)
{
    Audio signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / sampleRate));

    return signal;
}

float fromDb (double db)
{
    return (float) std::pow (10.0, db / 20.0);
}

GateDsp::Targets gateTargets()
{
    GateDsp::Targets targets;
    targets.thresholdDb = -30.0;
    targets.hysteresisDb = -6.0;
    targets.attackMs = 0.0;
    targets.holdMs = 20.0;
    targets.releaseMs = 50.0;
    targets.rangeDb = -200.0;
    return targets;
}

void prepare (GateDsp& dsp, const GateDsp::Targets& targets, double sampleRate = rate)
{
    dsp.prepare (sampleRate);
    dsp.snapToTargets (targets);
}

int expectedVelocity (double db)
{
    return 1 + (int) std::lround (std::clamp ((db + 60.0) / 60.0, 0.0, 1.0) * 126.0);
}

void setParameter (GateProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getApvts().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

std::vector<float> render (GateProcessor& processor, int numSamples)
{
    juce::AudioBuffer<float> buffer (2, numSamples);
    juce::MidiBuffer midi;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto sample = (float) (0.5 * std::sin (2.0 * pi * 300.0 * i / rate)) * (i % 9600 < 2400 ? 1.0f : 0.01f);
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample);
    }

    processor.processBlock (buffer, midi);

    return { buffer.getReadPointer (0), buffer.getReadPointer (0) + numSamples };
}

template <typename Type>
Type* findChild (juce::Component& parent, const std::function<bool (Type&)>& matches)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* typed = dynamic_cast<Type*> (child); typed != nullptr && matches (*typed))
            return typed;

        if (auto* found = findChild<Type> (*child, matches))
            return found;
    }

    return nullptr;
}

juce::MouseEvent mouseEvent (juce::Component& target, juce::Point<float> position, juce::Point<float> pressed, bool dragged)
{
    const auto now = juce::Time::getCurrentTime();

    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position,
                             juce::ModifierKeys {}.withFlags (juce::ModifierKeys::leftButtonModifier),
                             juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation,
                             juce::MouseInputSource::defaultRotation, juce::MouseInputSource::defaultTiltX,
                             juce::MouseInputSource::defaultTiltY, &target, &target, now, pressed, now, 1, dragged);
}
} // namespace

// State machine -----------------------------------------------------------------------------

TEST_CASE ("hysteresis opens at Threshold and closes below the close level", "[gate]")
{
    // Threshold -30, Hysteresis -6.
    GateDsp::Machine machine;

    CHECK (machine.step (-30.1, -30.0, -36.0, 1.0, 1.0, 0) == GateDsp::Machine::none);
    CHECK (machine.state == GateDsp::Machine::closed);
    CHECK (machine.step (-29.9, -30.0, -36.0, 1.0, 1.0, 0) == GateDsp::Machine::opened);

    machine.step (-29.9, -30.0, -36.0, 1.0, 1.0, 0);
    CHECK (machine.state == GateDsp::Machine::open);

    for (int i = 0; i < 100; ++i)
        CHECK (machine.step (-35.9, -30.0, -36.0, 1.0, 1.0, 0) == GateDsp::Machine::none);

    CHECK (machine.state == GateDsp::Machine::open);

    machine.step (-36.1, -30.0, -36.0, 1.0, 1.0, 0);
    CHECK (machine.state == GateDsp::Machine::hold);
    machine.step (-36.1, -30.0, -36.0, 1.0, 1.0, 0);
    CHECK (machine.state == GateDsp::Machine::closing);
    CHECK (machine.step (-36.1, -30.0, -36.0, 1.0, 1.0, 0) == GateDsp::Machine::shut);
    CHECK (machine.state == GateDsp::Machine::closed);

    // Back above the close level isn't enough: it needs Threshold again.
    CHECK (machine.step (-33.0, -30.0, -36.0, 1.0, 1.0, 0) == GateDsp::Machine::none);
    CHECK (machine.step (-29.0, -30.0, -36.0, 1.0, 1.0, 0) == GateDsp::Machine::opened);
}

TEST_CASE ("a new hit while the gate closes re-triggers it", "[gate]")
{
    GateDsp::Machine machine;

    machine.step (-10.0, -30.0, -36.0, 1.0, 0.01, 0);
    machine.step (-10.0, -30.0, -36.0, 1.0, 0.01, 0);
    machine.step (-50.0, -30.0, -36.0, 1.0, 0.01, 0);
    machine.step (-50.0, -30.0, -36.0, 1.0, 0.01, 0);
    machine.step (-50.0, -30.0, -36.0, 1.0, 0.01, 0);

    REQUIRE (machine.state == GateDsp::Machine::closing);
    CHECK (machine.step (-10.0, -30.0, -36.0, 1.0, 0.01, 0) == GateDsp::Machine::retriggered);
    CHECK (machine.state == GateDsp::Machine::opening);
}

// Gain ----------------------------------------------------------------------------------------

TEST_CASE ("a closed gate with Range at -inf is exact silence", "[gate]")
{
    GateDsp dsp;
    prepare (dsp, gateTargets());

    Audio signal (4800, 0.01f);
    signal.process (dsp);

    for (const auto sample : signal.left)
        REQUIRE (sample == 0.0f);
}

TEST_CASE ("Range sets how far the closed gate turns down", "[gate]")
{
    auto targets = gateTargets();
    targets.rangeDb = -20.0;

    GateDsp dsp;
    prepare (dsp, targets);

    Audio signal (4800, 0.01f);
    signal.process (dsp);

    CHECK (signal.left.back() == Approx (0.001f).epsilon (1.0e-4));
}

TEST_CASE ("an open gate passes the input exactly", "[gate]")
{
    GateDsp dsp;
    prepare (dsp, gateTargets());

    Audio signal (4800, 0.5f);
    signal.process (dsp);

    for (int i = 40; i < signal.size(); ++i)
        REQUIRE (signal.left[(size_t) i] == 0.5f);
}

TEST_CASE ("Attack at 0 ms still fades over 0.5 ms", "[gate]")
{
    GateDsp dsp;
    prepare (dsp, gateTargets());

    Audio signal (4800);
    signal.fill (1000, 4800, 0.5f);
    signal.process (dsp);

    auto fading = 0;
    auto largestStep = 0.0f;

    for (int i = 1000; i < 1100; ++i)
    {
        if (signal.left[(size_t) i] > 0.0f && signal.left[(size_t) i] < 0.5f)
            ++fading;

        largestStep = std::max (largestStep, std::abs (signal.left[(size_t) i] - signal.left[(size_t) i - 1]));
    }

    // 24 samples at 48 kHz.
    CHECK (fading >= 20);
    CHECK (fading <= 25);
    CHECK (largestStep < 0.15f);
}

TEST_CASE ("Hold keeps the gate open after the level falls", "[gate]")
{
    auto targets = gateTargets();
    targets.holdMs = 50.0;
    targets.releaseMs = 1.0;

    GateDsp dsp;
    prepare (dsp, targets);

    Audio signal (12000, 0.5f);
    signal.fill (4800, 12000, 0.001f);
    signal.process (dsp);

    CHECK (signal.left[4800 + 2300] == 0.001f);
    CHECK (signal.left[4800 + 2400 + 100] == 0.0f);
}

TEST_CASE ("Duck turns the signal down while the detector is over Threshold", "[gate]")
{
    auto targets = gateTargets();
    targets.duck = true;
    targets.rangeDb = -20.0;

    GateDsp dsp;
    prepare (dsp, targets);

    Audio quiet (4800, 0.01f);
    quiet.process (dsp);
    CHECK (quiet.left.back() == 0.01f);

    Audio loud (4800, 0.5f);
    loud.process (dsp);
    CHECK (loud.left.back() == Approx (0.05f).epsilon (1.0e-4));
}

TEST_CASE ("switching Gate and Duck, or moving Range, doesn't click", "[gate]")
{
    auto targets = gateTargets();
    targets.rangeDb = -20.0;

    GateDsp dsp;
    prepare (dsp, targets);

    Audio before (4800, 0.5f);
    before.process (dsp);

    targets.duck = true;
    dsp.setTargets (targets);

    Audio after (4800, 0.5f);
    after.process (dsp);

    auto largestStep = std::abs (after.left[0] - before.left.back());

    for (size_t i = 1; i < after.left.size(); ++i)
        largestStep = std::max (largestStep, std::abs (after.left[i] - after.left[i - 1]));

    CHECK (largestStep < 0.05f);
    CHECK (after.left.back() == Approx (0.05f).epsilon (1.0e-4));

    // Range from -inf to 0 dB on a closed gate.
    auto closed = gateTargets();
    GateDsp quiet;
    prepare (quiet, closed);

    Audio silent (4800, 0.01f);
    silent.process (quiet);

    closed.rangeDb = 0.0;
    quiet.setTargets (closed);

    Audio rising (4800, 0.01f);
    rising.process (quiet);

    largestStep = rising.left[0];

    for (size_t i = 1; i < rising.left.size(); ++i)
        largestStep = std::max (largestStep, std::abs (rising.left[i] - rising.left[i - 1]));

    CHECK (largestStep < 0.002f);

    // Within 0.1 dB after 100 ms (five time constants), and settled soon after.
    CHECK (rising.left.back() > 0.01f * 0.9886f);

    Audio settled (14400, 0.01f);
    settled.process (quiet);
    CHECK (settled.left.back() == Approx (0.01f).epsilon (1.0e-4));
}

TEST_CASE ("Wet and Dry are independent", "[gate]")
{
    auto targets = gateTargets();
    targets.wetGain = 0.0;
    targets.dryGain = 1.0;

    GateDsp dsp;
    prepare (dsp, targets);

    Audio signal (4800, 0.01f);
    signal.process (dsp);

    CHECK (signal.left.back() == 0.01f);
}

TEST_CASE ("noise is only added while the gate is open", "[gate]")
{
    auto targets = gateTargets();
    targets.noiseGain = 0.1;

    GateDsp dsp;
    prepare (dsp, targets);

    Audio closed (4800, 0.001f);
    closed.process (dsp);

    for (const auto sample : closed.left)
        REQUIRE (sample == 0.0f);

    Audio open (4800, 0.5f);
    open.process (dsp);

    auto differs = 0;

    for (int i = 100; i < open.size(); ++i)
        if (std::abs (open.left[(size_t) i] - 0.5f) > 1.0e-4f)
            ++differs;

    CHECK (differs > 4000);
    CHECK (open.left != open.right);
}

TEST_CASE ("Preview filter plays what the detector hears", "[gate]")
{
    auto targets = gateTargets();
    targets.preview = true;
    targets.highpassHz = 2000.0;

    GateDsp dsp;
    prepare (dsp, targets);

    auto low = sine (100.0, 0.5, 9600);
    low.process (dsp);

    auto peak = 0.0f;

    for (int i = 4800; i < low.size(); ++i)
        peak = std::max (peak, std::abs (low.left[(size_t) i]));

    // 100 Hz through a 2 kHz 12 dB/oct high-pass: about -52 dB.
    CHECK (peak < 0.005f);
}

// Lookahead ---------------------------------------------------------------------------------

TEST_CASE ("latency is zero without Lookahead and an impulse comes out at once", "[gate]")
{
    auto targets = gateTargets();
    targets.rangeDb = 0.0;

    GateDsp dsp;
    prepare (dsp, targets);

    CHECK (dsp.getLatencySamples() == 0);

    Audio impulse (64);
    impulse.left[0] = impulse.right[0] = 0.5f;
    impulse.process (dsp);

    CHECK (impulse.left[0] == 0.5f);
}

TEST_CASE ("Lookahead's latency is 50 ms whatever Pre-open is, and the output lines up", "[gate]")
{
    const auto preOpenMs = GENERATE (0.0, 10.0, 50.0);

    auto targets = gateTargets();
    targets.lookahead = true;
    targets.preOpenMs = preOpenMs;
    targets.thresholdDb = -60.0;

    GateDsp dsp;
    prepare (dsp, targets);

    CHECK (dsp.getLatencySamples() == lookaheadSamples);

    const auto input = sine (440.0, 0.5, 24000);
    auto output = input;
    output.process (dsp);

    for (int i = lookaheadSamples + 500; i < input.size(); ++i)
        REQUIRE (output.left[(size_t) i] == input.left[(size_t) (i - lookaheadSamples)]);
}

TEST_CASE ("Pre-open opens the gate before the transient arrives", "[gate]")
{
    auto targets = gateTargets();
    targets.lookahead = true;
    targets.preOpenMs = 5.0;

    GateDsp dsp;
    prepare (dsp, targets);

    Audio signal (24000, 0.001f);
    signal.fill (10000, 24000, 0.5f);
    signal.process (dsp);

    // The gate opens 240 samples before the step comes out at 12400.
    CHECK (signal.left[12100] == 0.0f);
    CHECK (signal.left[12300] == 0.001f);
    CHECK (signal.left[12400] == 0.5f);
}

TEST_CASE ("changing Lookahead while playing fades instead of clicking", "[gate]")
{
    auto targets = gateTargets();
    targets.thresholdDb = -60.0;

    GateDsp dsp;
    prepare (dsp, targets);

    auto signal = sine (440.0, 0.5, 48000);
    auto copy = signal;

    Audio first (24000), second (24000);
    std::copy (signal.left.begin(), signal.left.begin() + 24000, first.left.begin());
    std::copy (signal.right.begin(), signal.right.begin() + 24000, first.right.begin());
    std::copy (signal.left.begin() + 24000, signal.left.end(), second.left.begin());
    std::copy (signal.right.begin() + 24000, signal.right.end(), second.right.begin());

    first.process (dsp);

    targets.lookahead = true;
    dsp.setTargets (targets);
    second.process (dsp);

    CHECK (dsp.getLatencySamples() == lookaheadSamples);

    auto largestStep = std::abs (second.left[0] - first.left.back());

    for (size_t i = 1; i < second.left.size(); ++i)
        largestStep = std::max (largestStep, std::abs (second.left[i] - second.left[i - 1]));

    // A 440 Hz sine at 0.5 moves up to 0.029 a sample by itself.
    CHECK (largestStep < 0.06f);
}

// Sidechain -----------------------------------------------------------------------------------

TEST_CASE ("a sidechain detector opens the gate on the sidechain", "[gate]")
{
    auto targets = gateTargets();
    targets.detector = GateDsp::sideLR;

    GateDsp dsp;
    prepare (dsp, targets);

    Audio key (9600);
    key.fill (4800, 9600, 0.5f);

    Audio signal (9600, 0.5f);
    signal.process (dsp, &key);

    CHECK (signal.left[4000] == 0.0f);
    CHECK (signal.left[9000] == 0.5f);
}

TEST_CASE ("the sidechain warning comes after 2 s of silence and clears when signal arrives", "[gate]")
{
    auto targets = gateTargets();
    targets.detector = GateDsp::sideL;

    GateDsp dsp;
    prepare (dsp, targets);

    Audio silentKey (48000);
    Audio main (48000, 0.1f);
    main.process (dsp, &silentKey);
    CHECK_FALSE (dsp.isSidechainMissing());

    Audio more (60000, 0.1f);
    Audio silentMore (60000);
    more.process (dsp, &silentMore);
    CHECK (dsp.isSidechainMissing());

    Audio key (24000, 0.1f);
    Audio last (24000, 0.1f);
    last.process (dsp, &key);
    CHECK_FALSE (dsp.isSidechainMissing());

    // A main-input detector never warns.
    targets.detector = GateDsp::mainLR;
    GateDsp plain;
    prepare (plain, targets);

    Audio long_ (150000, 0.1f);
    long_.process (plain);
    CHECK_FALSE (plain.isSidechainMissing());
}

TEST_CASE ("a missing sidechain bus is silence, not a crash", "[gate]")
{
    auto targets = gateTargets();
    targets.detector = GateDsp::sideLR;

    GateDsp dsp;
    prepare (dsp, targets);

    Audio signal (4800, 0.5f);
    signal.process (dsp);

    for (const auto sample : signal.left)
        REQUIRE (sample == 0.0f);
}

TEST_CASE ("Link off gates each channel on its own", "[gate]")
{
    auto targets = gateTargets();
    targets.link = false;

    GateDsp dsp;
    prepare (dsp, targets);

    Audio signal (4800, 0.5f);
    std::fill (signal.right.begin(), signal.right.end(), 0.001f);
    signal.process (dsp);

    CHECK (signal.left.back() == 0.5f);
    CHECK (signal.right.back() == 0.0f);

    targets.link = true;
    GateDsp linked;
    prepare (linked, targets);

    Audio both (4800, 0.5f);
    std::fill (both.right.begin(), both.right.end(), 0.001f);
    both.process (linked);

    CHECK (both.right.back() == 0.001f);
}

// MIDI ------------------------------------------------------------------------------------------

namespace
{
GateDsp::Targets midiTargets (bool lookahead, bool velocity)
{
    auto targets = gateTargets();
    targets.thresholdDb = -40.0;
    targets.midi = true;
    targets.velocityFromLevel = velocity;
    targets.lookahead = lookahead;
    targets.preOpenMs = 5.0;
    targets.releaseMs = 100.0;
    return targets;
}

// Hits every 0.5 s, 50 ms long.
Audio hits (std::initializer_list<double> levelsDb)
{
    Audio signal ((int) levelsDb.size() * 24000 + 24000);
    auto at = 4800;

    for (const auto db : levelsDb)
    {
        signal.hit (at, 2400, fromDb (db));
        at += 24000;
    }

    return signal;
}
} // namespace

TEST_CASE ("velocity follows how hard each hit is, in both Lookahead modes", "[gate]")
{
    const auto lookahead = GENERATE (false, true);

    GateDsp dsp;
    prepare (dsp, midiTargets (lookahead, true));

    auto signal = hits ({ -24.0, -12.0, -3.0 });
    signal.process (dsp);

    const auto notes = signal.noteOns();
    REQUIRE (notes.size() == 3);

    const double levels[] { -24.0, -12.0, -3.0 };

    for (size_t i = 0; i < 3; ++i)
    {
        INFO ("hit " << i);
        CHECK (std::abs (notes[i].message.getVelocity() - expectedVelocity (levels[i])) <= 2);
        CHECK (notes[i].message.getNoteNumber() == 69);
        CHECK (notes[i].message.getChannel() == 1);
    }

    CHECK (notes[1].message.getVelocity() - notes[0].message.getVelocity() >= 20);

    // The mapping puts 9 dB at 18.9 velocity steps, so this pair can't be 20 apart.
    CHECK (notes[2].message.getVelocity() - notes[1].message.getVelocity() >= 17);

    // Every note-on has its note-off, after it.
    auto offs = 0;

    for (const auto& event : signal.events)
        if (event.message.isNoteOff())
            ++offs;

    CHECK (offs == 3);

    for (size_t i = 1; i < signal.events.size(); ++i)
        CHECK (signal.events[i].time >= signal.events[i - 1].time);
}

TEST_CASE ("with Lookahead and Pre-open, the note lands where the opening is heard", "[gate]")
{
    const auto preOpenMs = GENERATE (2.0, 5.0, 25.0, 48.0);

    auto targets = midiTargets (true, true);
    targets.preOpenMs = preOpenMs;

    GateDsp dsp;
    prepare (dsp, targets);

    auto signal = hits ({ -12.0 });
    signal.process (dsp);

    const auto notes = signal.noteOns();
    REQUIRE (notes.size() == 1);

    const auto heard = 4800 + lookaheadSamples - (std::int64_t) std::lround (preOpenMs * 48.0);
    CHECK (std::abs (notes[0].time - heard) <= 1);
}

TEST_CASE ("without Lookahead, the note is one velocity window late", "[gate]")
{
    const auto windowMs = GENERATE (1.0, 2.0, 5.0);

    auto targets = midiTargets (false, true);
    targets.velocityWindowMs = windowMs;

    GateDsp dsp;
    prepare (dsp, targets);

    auto signal = hits ({ -12.0 });
    signal.process (dsp);

    const auto notes = signal.noteOns();
    REQUIRE (notes.size() == 1);
    CHECK (std::abs (notes[0].time - (4800 + (std::int64_t) std::lround (windowMs * 48.0))) <= 1);

    targets.lookahead = false;
    CHECK (GateDsp::noteLatenessMs (targets) == windowMs);
}

TEST_CASE ("without Velocity from level, notes are on time at velocity 100", "[gate]")
{
    const auto lookahead = GENERATE (false, true);

    GateDsp dsp;
    prepare (dsp, midiTargets (lookahead, false));

    auto signal = hits ({ -12.0, -3.0 });
    signal.process (dsp);

    const auto notes = signal.noteOns();
    REQUIRE (notes.size() == 2);

    const auto offset = lookahead ? lookaheadSamples - 240 : 0;

    CHECK (std::abs (notes[0].time - (4800 + offset)) <= 1);
    CHECK (std::abs (notes[1].time - (28800 + offset)) <= 1);
    CHECK (notes[0].message.getVelocity() == 100);
    CHECK (notes[1].message.getVelocity() == 100);
}

TEST_CASE ("the velocity tooltip gives the actual delay", "[gate]")
{
    auto targets = midiTargets (false, true);

    CHECK (GateEditor::velocityTooltip (targets).contains ("2.0 ms late"));
    CHECK (GateEditor::velocityTooltip (targets).contains ("Turn on Lookahead"));

    targets.velocityWindowMs = 3.5;
    CHECK (GateEditor::velocityTooltip (targets).contains ("3.5 ms late"));

    targets.lookahead = true;
    targets.preOpenMs = 5.0;
    CHECK (GateDsp::noteLatenessMs (targets) == 0.0);
    CHECK_FALSE (GateEditor::velocityTooltip (targets).contains ("late"));

    targets.preOpenMs = 49.0;
    CHECK (GateDsp::noteLatenessMs (targets) == Approx (2.5));
    CHECK (GateEditor::velocityTooltip (targets).contains ("2.5 ms late"));

    targets.velocityFromLevel = false;
    CHECK (GateDsp::noteLatenessMs (targets) == 0.0);
}

TEST_CASE ("turning MIDI off, or changing Lookahead, ends a sounding note", "[gate]")
{
    auto targets = midiTargets (false, false);

    GateDsp dsp;
    prepare (dsp, targets);

    Audio first (4800, 0.5f);
    first.process (dsp);
    REQUIRE (first.noteOns().size() == 1);

    targets.midi = false;
    dsp.setTargets (targets);

    Audio second (4800, 0.5f);
    second.process (dsp, nullptr, 256, 4800);
    REQUIRE (second.events.size() == 1);
    CHECK (second.events[0].message.isNoteOff());

    // On again, then a Lookahead change.
    targets.midi = true;
    GateDsp other;
    prepare (other, targets);

    Audio third (4800, 0.5f);
    third.process (other);
    REQUIRE (third.noteOns().size() == 1);

    targets.lookahead = true;
    other.setTargets (targets);

    Audio fourth (9600, 0.5f);
    fourth.process (other, nullptr, 256, 4800);

    auto offs = 0;

    for (const auto& event : fourth.events)
        if (event.message.isNoteOff())
            ++offs;

    CHECK (offs >= 1);
    CHECK (fourth.events.front().message.isNoteOff());
}

// History ---------------------------------------------------------------------------------------

TEST_CASE ("the history gets one column per pixel of 3 s", "[gate]")
{
    GateDsp dsp;
    prepare (dsp, gateTargets());
    dsp.setHistoryColumns (456);

    Audio signal (48000, 0.5f);
    signal.process (dsp);

    std::vector<GateDsp::HistoryColumn> columns (1000);
    const auto count = dsp.popHistory (columns.data(), (int) columns.size());

    CHECK (std::abs (count - 152) <= 1);
    CHECK (columns[(size_t) count - 1].levelDb == Approx (-6.02f).margin (0.05f));
    CHECK (columns[(size_t) count - 1].gainDb == Approx (0.0f).margin (0.01f));
    CHECK (dsp.popHistory (columns.data(), (int) columns.size()) == 0);
}

// The usual checks ----------------------------------------------------------------------------

TEST_CASE ("gate block size doesn't change the output or the MIDI", "[gate]")
{
    auto targets = midiTargets (true, true);

    GateDsp whole, single, mixed;
    prepare (whole, targets);
    prepare (single, targets);
    prepare (mixed, targets);

    auto a = hits ({ -24.0, -6.0 });
    auto b = a;
    auto c = a;

    a.process (whole, nullptr, 8192);
    b.process (single, nullptr, 1);

    // Variable sizes.
    std::mt19937 random (3);
    std::uniform_int_distribution<int> sizes (1, 700);

    for (int start = 0; start < c.size();)
    {
        const auto count = std::min (sizes (random), c.size() - start);
        float* main[] { c.left.data() + start, c.right.data() + start };
        juce::MidiBuffer midi;
        mixed.process (main, 2, nullptr, 0, count, &midi);

        for (const auto metadata : midi)
            c.events.push_back ({ start + metadata.samplePosition, metadata.getMessage() });

        start += count;
    }

    for (size_t i = 0; i < a.left.size(); ++i)
    {
        REQUIRE (a.left[i] == Approx (b.left[i]).margin (1.0e-6));
        REQUIRE (a.left[i] == Approx (c.left[i]).margin (1.0e-6));
    }

    REQUIRE (a.events.size() == b.events.size());
    REQUIRE (a.events.size() == c.events.size());

    for (size_t i = 0; i < a.events.size(); ++i)
    {
        CHECK (a.events[i].time == b.events[i].time);
        CHECK (a.events[i].time == c.events[i].time);
    }
}

TEST_CASE ("gate works at every supported sample rate and in mono", "[gate]")
{
    const auto sampleRate = GENERATE (44100.0, 96000.0, 192000.0, 384000.0);

    auto targets = gateTargets();
    targets.lookahead = true;

    GateDsp dsp;
    prepare (dsp, targets, sampleRate);

    CHECK (dsp.getLatencySamples() == (int) std::lround (0.05 * sampleRate));

    auto signal = sine (440.0, 0.5, 16384, sampleRate);
    float* channels[] { signal.left.data() };
    dsp.process (channels, 1, nullptr, 0, signal.size(), nullptr);

    for (const auto sample : signal.left)
        REQUIRE (std::isfinite (sample));
}

TEST_CASE ("bad input never poisons the gate", "[gate]")
{
    GateDsp dsp;
    prepare (dsp, gateTargets());

    Audio bad (64, std::numeric_limits<float>::quiet_NaN());
    bad.process (dsp);

    Audio infinite (64, std::numeric_limits<float>::infinity());
    infinite.process (dsp);

    auto signal = sine (440.0, 0.5, 4800);
    signal.process (dsp);

    for (const auto sample : signal.left)
        REQUIRE (std::isfinite (sample));
}

TEST_CASE ("bypass passes the delayed input exactly", "[gate]")
{
    auto targets = gateTargets();
    targets.lookahead = true;
    targets.bypass = true;

    GateDsp dsp;
    prepare (dsp, targets);

    const auto input = sine (440.0, 0.5, 9600);
    auto output = input;
    output.process (dsp);

    for (int i = lookaheadSamples; i < input.size(); ++i)
        REQUIRE (output.left[(size_t) i] == input.left[(size_t) (i - lookaheadSamples)]);
}

TEST_CASE ("random settings stay finite", "[gate]")
{
    std::mt19937 random (11);
    std::uniform_real_distribution<double> unit (0.0, 1.0);

    GateDsp dsp;
    prepare (dsp, gateTargets());

    for (int round = 0; round < 40; ++round)
    {
        GateDsp::Targets targets;
        targets.thresholdDb = -90.0 * unit (random);
        targets.hysteresisDb = -24.0 + 36.0 * unit (random);
        targets.attackMs = 500.0 * unit (random);
        targets.holdMs = 2000.0 * unit (random);
        targets.releaseMs = 1.0 + 4999.0 * unit (random);
        targets.rangeDb = -90.0 * unit (random);
        targets.rmsMs = 250.0 * unit (random);
        targets.lowpassHz = 20.0 + 21980.0 * unit (random);
        targets.highpassHz = 20.0 + 4980.0 * unit (random);
        targets.detector = (int) (6.0 * unit (random)) % 6;
        targets.lookahead = unit (random) > 0.5;
        targets.preOpenMs = 50.0 * unit (random);
        targets.duck = unit (random) > 0.5;
        targets.link = unit (random) > 0.5;
        targets.preview = unit (random) > 0.8;
        targets.noiseGain = unit (random);
        targets.midi = unit (random) > 0.5;
        targets.velocityFromLevel = unit (random) > 0.5;
        targets.velocityWindowMs = 1.0 + 4.0 * unit (random);
        dsp.setTargets (targets);

        auto signal = hits ({ -6.0 * unit (random) - 1.0 });
        Audio key = signal;
        signal.process (dsp, &key, 1 + (int) (1000.0 * unit (random)));

        for (const auto sample : signal.left)
            REQUIRE (std::isfinite (sample));
    }
}

// Processor -------------------------------------------------------------------------------------

TEST_CASE ("gate automation with an unchanged value is bit-identical", "[gate]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    GateProcessor plain, automated;

    for (auto* processor : { &plain, &automated })
    {
        setParameter (*processor, "threshold", -20.0f);
        setParameter (*processor, "range", -30.0f);
        processor->prepareToPlay (rate, 48000);
    }

    for (auto* parameter : automated.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
        const auto before = automated.getApvts().getRawParameterValue (ranged->getParameterID())->load();

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->getValue());
        parameter->endChangeGesture();

        INFO (ranged->getParameterID());
        CHECK (automated.getApvts().getRawParameterValue (ranged->getParameterID())->load() == before);
    }

    REQUIRE (render (plain, 48000) == render (automated, 48000));
}

TEST_CASE ("gate state round-trips", "[gate]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    GateProcessor source;
    setParameter (source, "threshold", -24.0f);
    setParameter (source, "lookahead", 1.0f);
    setParameter (source, "preopen", 12.0f);
    setParameter (source, "mode", 1.0f);
    setParameter (source, "midiNote", 36.0f);
    setParameter (source, "velWindow", 3.5f);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    GateProcessor destination;
    destination.setStateInformation (state.getData(), (int) state.getSize());

    const auto targets = destination.readTargets();
    CHECK (targets.thresholdDb == Approx (-24.0));
    CHECK (targets.lookahead);
    CHECK (targets.preOpenMs == Approx (12.0));
    CHECK (targets.duck);
    CHECK (targets.note == 36);
    CHECK (targets.velocityWindowMs == Approx (3.5));
}

TEST_CASE ("corrupted gate state is ignored", "[gate]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    GateProcessor processor;

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    processor.setStateInformation (state.getData(), (int) state.getSize() / 2);

    juce::Random random (4321);

    for (size_t i = 0; i < state.getSize(); ++i)
        state[i] = (char) random.nextInt (256);

    processor.setStateInformation (state.getData(), (int) state.getSize());

    processor.prepareToPlay (rate, 512);
    CHECK (render (processor, 512).size() == 512);
}

TEST_CASE ("gate reports its latency and MIDI output", "[gate]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    GateProcessor processor;
    CHECK (processor.producesMidi());
    CHECK_FALSE (processor.getApvts().getParameter ("lookahead")->isAutomatable());

    processor.prepareToPlay (rate, 512);
    CHECK (processor.getLatencySamples() == 0);

    // The change fades out over one block and takes effect at the start of the next.
    setParameter (processor, "lookahead", 1.0f);
    render (processor, 4800);
    render (processor, 4800);
    CHECK (processor.getLatencySamples() == lookaheadSamples);

    setParameter (processor, "preopen", 40.0f);
    render (processor, 4800);
    CHECK (processor.getLatencySamples() == lookaheadSamples);
}

TEST_CASE ("gate supports mono, stereo and sidechain layouts", "[gate]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    GateProcessor processor;

    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses = { juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo() };
    layout.outputBuses = { juce::AudioChannelSet::stereo() };
    CHECK (processor.checkBusesLayoutSupported (layout));

    layout.inputBuses = { juce::AudioChannelSet::mono(), juce::AudioChannelSet::disabled() };
    layout.outputBuses = { juce::AudioChannelSet::mono() };
    CHECK (processor.checkBusesLayoutSupported (layout));

    layout.inputBuses = { juce::AudioChannelSet::stereo(), juce::AudioChannelSet::disabled() };
    layout.outputBuses = { juce::AudioChannelSet::mono() };
    CHECK_FALSE (processor.checkBusesLayoutSupported (layout));
}

TEST_CASE ("every gate preset loads and plays", "[gate]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    GateProcessor processor;
    processor.setNonRealtime (true);

    auto& presets = processor.getPresets();
    const auto names = presets.getNames();

    REQUIRE (names.size() >= 8);

    for (int i = 0; i < names.size(); ++i)
    {
        presets.load (i);
        processor.prepareToPlay (rate, 48000);

        for (const auto sample : render (processor, 48000))
            REQUIRE (std::isfinite (sample));
    }
}

TEST_CASE ("gate values can be typed", "[gate]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    GateProcessor processor;

    const auto typed = [&] (const char* id, const char* text)
    {
        const juce::RangedAudioParameter& parameter = *processor.getApvts().getParameter (id);
        return parameter.convertFrom0to1 (parameter.getValueForText (text));
    };

    CHECK (typed ("threshold", "-38") == Approx (-38.0f));
    CHECK (typed ("hysteresis", "-6 dB") == Approx (-6.0f));
    CHECK (typed ("attack", "0,5") == Approx (0.5f));
    CHECK (typed ("hold", "150 ms") == Approx (150.0f));
    CHECK (typed ("release", "1,000") == Approx (1000.0f).margin (0.1f));
    CHECK (typed ("lowpass", "2.5k") == Approx (2500.0f).margin (1.0f));
    CHECK (typed ("highpass", "120") == Approx (120.0f).margin (1.0f));
    CHECK (typed ("range", "-inf") == Approx ((float) GateDsp::rangeFloorDb));
    CHECK (typed ("midiNote", "A4") == 69.0f);
    CHECK (typed ("midiNote", "C#3") == 49.0f);
    CHECK (typed ("midiNote", "36") == 36.0f);
    CHECK (typed ("midiNote", "C-1") == 0.0f);
}

// Editor ------------------------------------------------------------------------------------------

TEST_CASE ("the gate editor opens at its size", "[gate]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    GateProcessor processor;
    processor.prepareToPlay (rate, 512);

    const std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());

    REQUIRE (editor != nullptr);
    CHECK (editor->getWidth() == 740);
    CHECK (editor->getHeight() == 421);

    processor.editorBeingDeleted (editor.get());
}

TEST_CASE ("Pre-open is locked without Lookahead, and works with it", "[gate]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto lookahead = GENERATE (false, true);

    GateProcessor processor;
    setParameter (processor, "lookahead", lookahead ? 1.0f : 0.0f);
    processor.prepareToPlay (rate, 512);

    const std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
    REQUIRE (editor != nullptr);

    auto& preOpen = *processor.getApvts().getParameter ("preopen");

    auto* knob = findChild<fancy::ui::Knob> (*editor, [] (fancy::ui::Knob& candidate)
    {
        return candidate.getTooltip() == GateEditor::preOpenHint || candidate.getTooltip().startsWith ("How early");
    });

    REQUIRE (knob != nullptr);
    CHECK (knob->isLocked() == ! lookahead);
    CHECK ((knob->getTooltip() == GateEditor::preOpenHint) == ! lookahead);

    auto lockedClicks = 0;
    knob->onLockedClick = [&] { ++lockedClicks; };

    const auto before = preOpen.getValue();
    const juce::Point<float> face ((float) knob->getWidth() * 0.5f, 40.0f);

    // Drag up.
    knob->mouseDown (mouseEvent (*knob, face, face, false));
    knob->mouseDrag (mouseEvent (*knob, face.translated (0.0f, -60.0f), face, true));
    knob->mouseUp (mouseEvent (*knob, face.translated (0.0f, -60.0f), face, true));

    // The wheel.
    juce::MouseWheelDetails wheel {};
    wheel.deltaY = 0.5f;
    knob->mouseWheelMove (mouseEvent (*knob, face, face, false), wheel);

    // Typed text, through the parameter's own box: a click on the value.
    const juce::Point<float> value ((float) knob->getWidth() * 0.5f, 85.0f);
    knob->mouseDown (mouseEvent (*knob, value, value, false));
    knob->mouseUp (mouseEvent (*knob, value, value, false));

    const auto typingBox = findChild<juce::TextEditor> (*editor, [] (juce::TextEditor& box) { return box.isVisible(); }) != nullptr;

    if (lookahead)
    {
        CHECK (preOpen.getValue() != before);
        CHECK (lockedClicks == 0);
    }
    else
    {
        CHECK (preOpen.getValue() == before);
        CHECK (lockedClicks >= 1);
        CHECK_FALSE (typingBox);
    }

    processor.editorBeingDeleted (editor.get());
}

TEST_CASE ("a checkbox pulse starts", "[gate]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    auto parameter = std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "flag", 1 }, "Flag", false);
    fancy::ui::ParamCheckbox checkbox (*parameter, "Flag");

    CHECK_FALSE (checkbox.isPulsing());
    checkbox.pulse (juce::Colours::yellow, 600);
    CHECK (checkbox.isPulsing());
}

// Measurements ----------------------------------------------------------------------------------

namespace
{
template <typename Function>
double medianOfFive (Function&& measure)
{
    std::vector<double> runs;

    for (int run = 0; run < 5; ++run)
        runs.push_back (measure());

    std::sort (runs.begin(), runs.end());
    return runs[2];
}

constexpr int measureBlock = 128, measureBlocks = 6000;

double gateCpu (const GateDsp::Targets& targets)
{
    GateDsp dsp;
    prepare (dsp, targets);

    float left[measureBlock], right[measureBlock];
    float* channels[] { left, right };

    std::mt19937 random (8);
    std::uniform_real_distribution<float> noise (-0.25f, 0.25f);

    std::chrono::duration<double> spent {};
    juce::MidiBuffer midi;
    midi.ensureSize (2048);

    for (int block = 0; block < measureBlocks; ++block)
    {
        // Hits every 250 ms, so the gate opens and closes.
        const auto loud = (block * measureBlock) % 12000 < 2400;

        for (int i = 0; i < measureBlock; ++i)
        {
            left[i] = noise (random) * (loud ? 1.0f : 0.01f);
            right[i] = noise (random) * (loud ? 1.0f : 0.01f);
        }

        midi.clear();

        const auto start = std::chrono::steady_clock::now();

        dsp.setTargets (targets);
        dsp.process (channels, 2, nullptr, 0, measureBlock, &midi);

        spent += std::chrono::steady_clock::now() - start;
    }

    return 100.0 * spent.count() / ((double) (measureBlocks * measureBlock) / rate);
}
} // namespace

TEST_CASE ("gate CPU measurement", "[.measure]")
{
    auto defaults = gateTargets();
    defaults.thresholdDb = -30.0;

    auto busy = defaults;
    busy.lookahead = true;
    busy.rmsMs = 10.0;
    busy.lowpassHz = 6000.0;
    busy.highpassHz = 150.0;
    busy.midi = true;
    busy.velocityFromLevel = true;
    busy.noiseGain = 0.01;
    busy.link = false;

    const auto gate = medianOfFive ([&] { return gateCpu (defaults); });
    const auto gateBusy = medianOfFive ([&] { return gateCpu (busy); });

    std::printf ("gate cpu (median of 5): default %.3f %%, lookahead + filters + MIDI + noise, unlinked %.3f %%\n", gate, gateBusy);

   #if FANCY_HAS_COMP
    const auto comp = medianOfFive ([]
    {
        CompDsp dsp;
        dsp.prepare (rate);
        dsp.snapToTargets (CompDsp::Targets {});

        float left[measureBlock], right[measureBlock];
        float* channels[] { left, right };

        std::mt19937 random (8);
        std::uniform_real_distribution<float> noise (-0.25f, 0.25f);
        std::chrono::duration<double> spent {};

        for (int block = 0; block < measureBlocks; ++block)
        {
            for (int i = 0; i < measureBlock; ++i)
            {
                left[i] = noise (random);
                right[i] = noise (random);
            }

            const auto start = std::chrono::steady_clock::now();
            dsp.process (channels, 2, nullptr, 0, measureBlock);
            spent += std::chrono::steady_clock::now() - start;
        }

        return 100.0 * spent.count() / ((double) (measureBlocks * measureBlock) / rate);
    });

    std::printf ("gate cpu reference: Fancy Comp default %.3f %% in the same run; gate default / comp = %.2f, busy / comp = %.2f\n",
                 comp, gate / comp, gateBusy / comp);
   #endif

    SUCCEED();
}
