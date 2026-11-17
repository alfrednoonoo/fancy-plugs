// The website's demo clips (docs/website-plan.md section 7.1): each plugin's real editor, with a
// synthesized music loop playing through its real processor while a pointer moves one or two
// controls. Frames are saved at 30 per second, at 2x, as <dir>/<slug>/f0000.png and on; CI
// encodes them to video. Each frame processes exactly 1/30 s of audio and runs exactly one of the
// editor's 30 Hz updates, so a clip is the same on every run.
// Usage: fancy-screens --demos <output-directory> [<slug>]

#include "Demos.h"

#include <fancy/ui/Controls.h>
#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>

#if FANCY_HAS_KICKER
 #include "KickerProcessor.h"
#endif
#if FANCY_HAS_EXCITER
 #include "ExciterProcessor.h"
#endif
#if FANCY_HAS_COMP
 #include "CompProcessor.h"
#endif
#if FANCY_HAS_EQ
 #include "EqGraph.h"
 #include "EqProcessor.h"
#endif
#if FANCY_HAS_VERB
 #include "VerbProcessor.h"
#endif
#if FANCY_HAS_DELAY
 #include "DelayProcessor.h"
#endif
#if FANCY_HAS_MULTICOMP
 #include "MultiCompProcessor.h"
#endif
#if FANCY_HAS_FIR
 #include "FirProcessor.h"
#endif
#if FANCY_HAS_GATE
 #include "GateProcessor.h"
#endif
#if FANCY_HAS_LIMIT
 #include "LimitProcessor.h"
#endif
#if FANCY_HAS_DS
 #include "DsProcessor.h"
#endif
#if FANCY_HAS_DSMINI
 #include "DsMiniProcessor.h"
#endif
#if FANCY_HAS_SATURE
 #include "SatureProcessor.h"
#endif
#if FANCY_HAS_ROOM
 #include "RoomProcessor.h"
#endif

namespace
{

constexpr double sampleRate = 48000.0;
constexpr int blockSize = 128;
constexpr int fps = 30;
constexpr int samplesPerFrame = (int) sampleRate / fps;
constexpr double clipSeconds = 6.0;
constexpr double prerollSeconds = 4.0;
constexpr float frameScale = 2.0f;

// --- The music: one bar at 120 BPM (2 s), looped, so a 6 s clip is three bars ---------------------
struct Mix
{
    float kick = 1.0f, snare = 1.0f, hats = 1.0f, ess = 0.0f, bass = 1.0f, pad = 1.0f;
    float ring = 0.0f; // a snare that rings at 740 Hz and 2.9 kHz, for FIR's Resonance mode
};

class Noise
{
public:
    float next() noexcept
    {
        state = state * 1664525u + 1013904223u;
        return (float) (state >> 8) / 8388608.0f - 1.0f;
    }

private:
    juce::uint32 state = 22222u;
};

juce::AudioBuffer<float> makeBar (const Mix& mix)
{
    constexpr double beat = 0.5;
    const auto length = (int) (sampleRate * beat * 4.0);
    juce::AudioBuffer<float> bar (2, length);
    bar.clear();

    const auto twoPi = juce::MathConstants<double>::twoPi;
    Noise noise;

    const auto add = [&] (int n, float left, float right)
    {
        bar.addSample (0, n, left);
        bar.addSample (1, n, right);
    };

    // Kick on 1, 3 and the and of 3.
    for (const auto start : { 0.0, 2.0, 2.5 })
    {
        double phase = 0.0;

        for (int i = 0; i < (int) (sampleRate * 0.45); ++i)
        {
            const auto n = (int) (start * beat * sampleRate) + i;
            if (n >= length) break;
            const auto t = i / sampleRate;
            phase += twoPi * (45.0 + 80.0 * std::exp (-t / 0.03)) / sampleRate;
            const auto v = (float) (std::sin (phase) * std::exp (-t / 0.22)) * mix.kick;
            add (n, v, v);
        }
    }

    // Snare on 2 and 4.
    for (const auto start : { 1.0, 3.0 })
    {
        float low = 0.0f;

        for (int i = 0; i < (int) (sampleRate * (mix.ring > 0.0f ? 0.95 : 0.35)); ++i)
        {
            const auto n = (int) (start * beat * sampleRate) + i;
            if (n >= length) break;
            const auto t = i / sampleRate;
            const auto white = noise.next();
            low += 0.25f * (white - low);
            const auto rings = mix.ring * (0.5 * std::sin (twoPi * 740.0 * t) + 0.35 * std::sin (twoPi * 2900.0 * t)) * std::exp (-t / 0.3);
            const auto v = (float) (((white - low) * 0.55 * std::exp (-t / 0.11) + 0.4 * std::sin (twoPi * 185.0 * t) * std::exp (-t / 0.07)) * mix.snare + rings);
            add (n, v * 0.95f, v);
        }
    }

    // Hats on every eighth, the off-beats a little open.
    for (int k = 0; k < 8; ++k)
    {
        float low = 0.0f;
        const auto decay = (k % 2 == 1) ? 0.07 : 0.025;

        for (int i = 0; i < (int) (sampleRate * 0.2); ++i)
        {
            const auto n = (int) (k * 0.5 * beat * sampleRate) + i;
            if (n >= length) break;
            const auto white = noise.next();
            low += 0.6f * (white - low);
            const auto v = (white - low) * 0.32f * (float) std::exp (-(i / sampleRate) / decay) * mix.hats;
            add (n, v * 0.8f, v);
        }
    }

    // A sung "s": band-limited noise around 5 to 9 kHz, for the de-essers.
    for (const auto start : { 0.5, 1.5, 3.5 })
    {
        float high = 0.0f, low = 0.0f;

        for (int i = 0; i < (int) (sampleRate * 0.2); ++i)
        {
            const auto n = (int) (start * beat * sampleRate) + i;
            if (n >= length) break;
            const auto t = i / sampleRate;
            const auto white = noise.next();
            high += 0.55f * (white - high);
            const auto band = white - high;
            low += 0.7f * (band - low);
            const auto env = (float) (std::min (1.0, t / 0.015) * std::exp (-t / 0.09));
            const auto v = low * 0.7f * env * mix.ess;
            add (n, v, v);
        }
    }

    // Bass, a note per beat, and a held A minor chord.
    const double bassNotes[] { 55.0, 55.0, 65.41, 49.0 };
    const double chord[] { 220.0, 261.63, 329.63 };
    double bassPhase = 0.0, padPhase[6] {};
    float bassLow = 0.0f, padLowL = 0.0f, padLowR = 0.0f;

    for (int n = 0; n < length; ++n)
    {
        const auto t = n / sampleRate;
        const auto beatIndex = std::min (3, (int) (t / beat));
        const auto inBeat = t - beatIndex * beat;

        bassPhase += bassNotes[beatIndex] / sampleRate;
        bassPhase -= std::floor (bassPhase);
        const auto saw = (float) (2.0 * bassPhase - 1.0);
        bassLow += 0.05f * (saw - bassLow);
        const auto bassEnv = (float) (inBeat < 0.42 ? 1.0 - 0.3 * inBeat : 0.0);

        float padL = 0.0f, padR = 0.0f;

        for (int v = 0; v < 6; ++v)
        {
            const auto detune = v < 3 ? 0.997 : 1.003;
            padPhase[v] += chord[v % 3] * detune / sampleRate;
            padPhase[v] -= std::floor (padPhase[v]);
            (v < 3 ? padL : padR) += (float) (2.0 * padPhase[v] - 1.0);
        }

        padLowL += 0.12f * (padL - padLowL);
        padLowR += 0.12f * (padR - padLowR);

        const auto b = bassLow * 0.55f * bassEnv * mix.bass;
        add (n, b + padLowL * 0.07f * mix.pad, b + padLowR * 0.07f * mix.pad);
    }

    // Peaks at -6 dBFS.
    const auto peak = juce::jmax (bar.getMagnitude (0, 0, length), bar.getMagnitude (1, 0, length));
    if (peak > 0.0f)
        bar.applyGain (0.5f / peak);

    return bar;
}

// --- The script: tracks of keyframes, and where the pointer is ------------------------------------
struct Key
{
    double t;
    float value;
};

struct Track
{
    juce::String paramId;
    std::vector<Key> keys;
    bool normalised = true; // values 0 to 1, or else in the parameter's own units
};

struct Script
{
    juce::String slug;
    std::function<std::unique_ptr<juce::AudioProcessor>()> make;
    Mix mix;
    std::vector<Track> tracks;
    std::function<void (juce::AudioProcessor&, juce::AudioProcessorEditor&)> setup;
    std::function<void (juce::AudioProcessor&)> afterChange;
    // Where the pointer holds, in editor coordinates, when not on the first track's control.
    std::function<std::optional<juce::Point<float>> (juce::AudioProcessorEditor&)> grip;
};

// Eases between keyframes, holding the first and last values outside them.
float valueAt (const Track& track, double t)
{
    const auto& k = track.keys;
    if (t <= k.front().t) return k.front().value;
    if (t >= k.back().t) return k.back().value;

    for (size_t i = 1; i < k.size(); ++i)
    {
        if (t <= k[i].t)
        {
            const auto x = (float) ((t - k[i - 1].t) / (k[i].t - k[i - 1].t));
            const auto eased = x * x * (3.0f - 2.0f * x);
            return k[i - 1].value + (k[i].value - k[i - 1].value) * eased;
        }
    }

    return k.back().value;
}

// The usual gesture: hold, go to the target, hold, come back, hold. Starts and ends the same, so
// the clip loops without a jump.
std::vector<Key> outAndBack (float from, float to)
{
    return { { 0.7, from }, { 2.5, to }, { 3.3, to }, { 5.1, from } };
}

bool isShownIn (const juce::Component& c, const juce::Component& root)
{
    for (auto* p = &c; p != nullptr && p != &root; p = p->getParentComponent())
        if (! p->isVisible())
            return false;

    return true;
}

template <typename Fn>
void forEachChild (juce::Component& root, Fn&& fn)
{
    for (auto* child : root.getChildren())
    {
        fn (*child);
        forEachChild (*child, fn);
    }
}

fancy::ui::ParamControl* findControl (juce::Component& editor, const juce::String& paramId)
{
    fancy::ui::ParamControl* found = nullptr;

    forEachChild (editor, [&] (juce::Component& c)
    {
        if (found != nullptr) return;

        if (auto* control = dynamic_cast<fancy::ui::ParamControl*> (&c))
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (&control->getParameter()))
                if (withId->paramID == paramId && isShownIn (c, editor))
                    found = control;
    });

    return found;
}

juce::RangedAudioParameter* findParameter (juce::AudioProcessor& processor, const juce::String& paramId)
{
    for (auto* p : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (ranged->paramID == paramId)
                return ranged;

    return nullptr;
}

void listParameters (juce::AudioProcessor& processor)
{
    for (auto* p : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            std::cout << "    " << ranged->paramID << " (" << ranged->getName (40) << ")\n";
}

// An arrow pointer, white with a dark edge, and a faint ring under its tip while it drags.
void drawPointer (juce::Graphics& g, juce::Point<float> tip, bool held)
{
    if (held)
    {
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.fillEllipse (juce::Rectangle<float> (26.0f, 26.0f).withCentre (tip));
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawEllipse (juce::Rectangle<float> (26.0f, 26.0f).withCentre (tip), 1.2f);
    }

    juce::Path arrow;
    arrow.startNewSubPath (0.0f, 0.0f);
    arrow.lineTo (0.0f, 17.0f);
    arrow.lineTo (4.2f, 13.2f);
    arrow.lineTo (7.2f, 19.8f);
    arrow.lineTo (9.9f, 18.6f);
    arrow.lineTo (7.0f, 12.2f);
    arrow.lineTo (12.4f, 12.2f);
    arrow.closeSubPath();
    arrow.applyTransform (juce::AffineTransform::translation (tip));

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (arrow, juce::AffineTransform::translation (1.0f, 1.5f));
    g.setColour (juce::Colours::white);
    g.fillPath (arrow);
    g.setColour (juce::Colour (0xff141414));
    g.strokePath (arrow, juce::PathStrokeType (1.1f, juce::PathStrokeType::mitered));
}

bool writePng (const juce::Image& image, const juce::File& file)
{
    file.deleteFile();
    juce::FileOutputStream stream (file);
    juce::PNGImageFormat png;
    return stream.openedOk() && png.writeImageToStream (image, stream);
}

bool renderScript (const Script& script, const juce::File& outputDir)
{
    std::cout << "demo " << script.slug << std::endl;

    auto processor = script.make();
    processor->setRateAndBufferSizeDetails (sampleRate, blockSize);
    processor->prepareToPlay (sampleRate, blockSize);

    const std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorAndMakeActive());
    auto* shell = dynamic_cast<fancy::ui::ShellEditor*> (editor.get());

    if (shell == nullptr)
    {
        std::cout << "  no editor" << std::endl;
        return false;
    }

    shell->stopTimerForCapture();

    if (script.setup)
        script.setup (*processor, *editor);

    shell->tickForCapture();

    // Each track drives the control the editor shows for it, which for a Verb module setting is
    // the card's own parameter, or else the processor's parameter.
    struct Bound
    {
        const Track* track;
        juce::RangedAudioParameter* param;
        fancy::ui::ParamControl* control;
    };

    std::vector<Bound> bound;

    for (const auto& track : script.tracks)
    {
        auto* control = findControl (*editor, track.paramId);
        auto* param = control != nullptr ? &control->getParameter() : findParameter (*processor, track.paramId);

        if (param == nullptr)
        {
            std::cout << "  no parameter " << track.paramId << "; the processor has:\n";
            listParameters (*processor);
            return false;
        }

        std::cout << "  " << track.paramId << (control != nullptr ? " on its control" : " (no visible control)") << std::endl;
        bound.push_back ({ &track, param, control });
    }

    const auto set = [&] (double t)
    {
        bool changed = false;

        for (const auto& b : bound)
        {
            const auto v = valueAt (*b.track, t);
            const auto normalised = b.track->normalised ? v : b.param->convertTo0to1 (v);

            if (std::abs (b.param->getValue() - normalised) > 1.0e-6f)
            {
                b.param->setValueNotifyingHost (normalised);
                changed = true;
            }
        }

        if (changed && script.afterChange)
            script.afterChange (*processor);
    };

    const auto bar = makeBar (script.mix);
    const auto channels = juce::jmax (2, processor->getTotalNumInputChannels(), processor->getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer (channels, blockSize);
    juce::MidiBuffer midi;
    int position = 0;

    const auto play = [&] (int samples)
    {
        while (samples > 0)
        {
            const auto n = juce::jmin (samples, blockSize);
            buffer.setSize (channels, n, false, false, true);
            buffer.clear();

            for (int i = 0; i < n; ++i)
            {
                buffer.setSample (0, i, bar.getSample (0, position));
                buffer.setSample (1, i, bar.getSample (1, position));
                position = (position + 1) % bar.getNumSamples();
            }

            midi.clear();
            processor->processBlock (buffer, midi);
            samples -= n;
        }
    };

    // Pre-roll, so meters, analyzers and tails are already moving on the first frame.
    set (0.0);
    for (int f = 0; f < (int) (prerollSeconds * fps); ++f)
    {
        play (samplesPerFrame);
        shell->tickForCapture();
    }

    const auto dir = outputDir.getChildFile (script.slug);
    dir.deleteRecursively();
    dir.createDirectory();

    // The pointer drifts in from below right, holds the control, and drifts back out, ending
    // where it started.
    const juce::Point<float> away (46.0f, 38.0f);
    const auto firstControl = bound.empty() ? nullptr : bound.front().control;
    const auto startNormalised = bound.empty() ? 0.0f : bound.front().param->getValue();
    const auto firstMove = script.tracks.empty() ? 0.0 : script.tracks.front().keys.front().t;
    const auto lastMove = script.tracks.empty() ? 0.0 : script.tracks.front().keys.back().t;

    bool ok = true;

    for (int f = 0; f < (int) (clipSeconds * fps); ++f)
    {
        const auto t = f / (double) fps;
        set (t);
        play (samplesPerFrame);
        shell->tickForCapture();

        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, frameScale);

        std::optional<juce::Point<float>> grip;

        if (script.grip)
            grip = script.grip (*editor);
        else if (firstControl != nullptr)
        {
            auto p = editor->getLocalPoint (firstControl, firstControl->getGripPoint());

            // A knob turns under a still hand, so the hand moves as a drag would: up to turn up.
            if (dynamic_cast<fancy::ui::Knob*> (firstControl) != nullptr)
                p.y -= (bound.front().param->getValue() - startNormalised) * 70.0f;

            grip = p;
        }

        if (grip)
        {
            const auto approach = juce::jlimit (0.0, 1.0, (t - (firstMove - 0.6)) / 0.5);
            const auto leave = juce::jlimit (0.0, 1.0, (t - (lastMove + 0.2)) / 0.5);
            const auto x = (float) (approach - leave);
            const auto eased = x * x * (3.0f - 2.0f * x);
            const auto tip = *grip + away * (1.0f - eased);
            const auto held = t >= firstMove - 0.05 && t <= lastMove + 0.1;

            juce::Graphics g (image);
            g.addTransform (juce::AffineTransform::scale (frameScale));
            drawPointer (g, tip, held);
        }

        ok = writePng (image, dir.getChildFile (juce::String::formatted ("f%04d.png", f))) && ok;
    }

    processor->releaseResources();
    return ok;
}

template <typename P>
std::function<std::unique_ptr<juce::AudioProcessor>()> maker()
{
    return [] { return std::make_unique<P>(); };
}

std::vector<Script> scripts()
{
    std::vector<Script> s;
    const Mix full;

   #if FANCY_HAS_COMP
    s.push_back ({ "comp", maker<CompProcessor>(), full, { { "threshold", outAndBack (-6.0f, -26.0f), false } } });
   #endif
   #if FANCY_HAS_MULTICOMP
    s.push_back ({ "multicomp", maker<MultiCompProcessor>(), full, { { "b1thr", outAndBack (0.0f, -26.0f), false } } });
   #endif
   #if FANCY_HAS_GATE
    s.push_back ({ "gate", maker<GateProcessor>(), Mix { 1.0f, 1.0f, 1.2f, 0.0f, 0.4f, 0.0f }, { { "threshold", outAndBack (-48.0f, -16.0f), false } } });
   #endif
   #if FANCY_HAS_LIMIT
    s.push_back ({ "limit", maker<LimitProcessor>(), full, { { "threshold", outAndBack (0.0f, -10.0f), false } } });
   #endif
   #if FANCY_HAS_DS
    s.push_back ({ "ds", maker<DsProcessor>(), Mix { 0.5f, 0.5f, 1.4f, 1.6f, 0.4f, 0.6f }, { { "threshold", outAndBack (-14.0f, -42.0f), false } } });
   #endif
   #if FANCY_HAS_DSMINI
    s.push_back ({ "dsmini", maker<DsMiniProcessor>(), Mix { 0.5f, 0.5f, 1.4f, 1.6f, 0.4f, 0.6f }, { { "amount", outAndBack (0.15f, 0.9f) } } });
   #endif
   #if FANCY_HAS_EQ
    {
        Script eq { "eq", maker<EqProcessor>(), full,
                    { { "b2freq", { { 0.7, 400.0f }, { 1.9, 900.0f }, { 3.0, 3500.0f }, { 4.0, 1800.0f }, { 5.1, 400.0f } }, false },
                      { "b2gain", { { 0.7, 0.0f }, { 1.9, 9.0f }, { 3.0, -7.0f }, { 4.0, 5.0f }, { 5.1, 0.0f } }, false } } };

        // Band 2 selected, as clicking its handle does, so its knobs below turn with the drag.
        eq.setup = [] (juce::AudioProcessor&, juce::AudioProcessorEditor& editor)
        {
            EqGraph* graph = nullptr;
            forEachChild (editor, [&] (juce::Component& c) { if (graph == nullptr) graph = dynamic_cast<EqGraph*> (&c); });

            if (graph != nullptr && graph->onBandSelected)
                graph->onBandSelected (1);
        };

        // On the graph's band 2 handle, as a hand dragging it.
        eq.grip = [] (juce::AudioProcessorEditor& editor) -> std::optional<juce::Point<float>>
        {
            EqGraph* graph = nullptr;
            forEachChild (editor, [&] (juce::Component& c) { if (graph == nullptr) graph = dynamic_cast<EqGraph*> (&c); });

            if (graph == nullptr)
                return std::nullopt;

            return editor.getLocalPoint (graph, graph->getHandlePosition (1));
        };

        s.push_back (eq);
    }
   #endif
   #if FANCY_HAS_FIR
    {
        Mix ringing;
        ringing.ring = 1.0f;
        Script fir { "fir", maker<FirProcessor>(), ringing, { { "resDepth", outAndBack (1.0f, 18.0f), false } } };
        fir.setup = [] (juce::AudioProcessor& p, juce::AudioProcessorEditor&)
        {
            if (auto* mode = findParameter (p, "mode"))
                mode->setValueNotifyingHost (mode->convertTo0to1 (5.0f)); // Resonance
        };
        s.push_back (fir);
    }
   #endif
   #if FANCY_HAS_EXCITER
    s.push_back ({ "exciter", maker<ExciterProcessor>(), full, { { "harmonics", outAndBack (0.1f, 0.85f) } } });
   #endif
   #if FANCY_HAS_SATURE
    s.push_back ({ "sature", maker<SatureProcessor>(), full, { { "drive", outAndBack (0.1f, 0.85f) } } });
   #endif
   #if FANCY_HAS_KICKER
    s.push_back ({ "kicker", maker<KickerProcessor>(), Mix { 1.2f, 0.6f, 0.5f, 0.0f, 1.0f, 0.3f }, { { "freq", outAndBack (0.25f, 0.75f) } } });
   #endif
   #if FANCY_HAS_DELAY
    s.push_back ({ "delay", maker<DelayProcessor>(), Mix { 0.6f, 1.2f, 0.4f, 0.0f, 0.4f, 0.5f }, { { "t1fb", outAndBack (0.25f, 0.8f) } } });
   #endif
   #if FANCY_HAS_VERB
    {
        Script verb { "verb", maker<VerbProcessor>(), Mix { 0.6f, 1.2f, 0.4f, 0.0f, 0.4f, 0.5f }, { { "length", outAndBack (1.2f, 5.0f), false } } };
        verb.setup = [] (juce::AudioProcessor& p, juce::AudioProcessorEditor& e)
        {
            auto& verbProcessor = static_cast<VerbProcessor&> (p);
            const auto reverb = verbProcessor.addModule (verb::ModuleType::reverb, 0);
            verbProcessor.setModuleValue (reverb, "length", 1.2f);
            verbProcessor.waitForBuild (20000);
            static_cast<fancy::ui::ShellEditor&> (e).tickForCapture(); // the rack shows the new card
        };
        // Each change rebuilds the IR on a background thread; wait, so the waveform keeps up.
        verb.afterChange = [] (juce::AudioProcessor& p) { static_cast<VerbProcessor&> (p).waitForBuild (20000); };
        s.push_back (verb);
    }
   #endif
   #if FANCY_HAS_ROOM
    s.push_back ({ "room", maker<RoomProcessor>(), Mix { 0.6f, 1.2f, 0.4f, 0.0f, 0.4f, 0.5f },
                   { { "size", outAndBack (0.3f, 0.85f) }, { "decay", outAndBack (0.3f, 0.85f) } } });
   #endif

    return s;
}

} // namespace

int renderDemos (const juce::File& outputDir, const juce::String& only)
{
    if (! outputDir.createDirectory())
    {
        std::cerr << "cannot create " << outputDir.getFullPathName() << "\n";
        return 1;
    }

    bool ok = true;

    for (const auto& script : scripts())
        if (only.isEmpty() || only == script.slug)
            ok = renderScript (script, outputDir) && ok;

    return ok ? 0 : 1;
}
