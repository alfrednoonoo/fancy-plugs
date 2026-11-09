#pragma once

// Hosting a VST3 for measurement, and the measurements themselves (docs/phase3-Q1.md §4).

#include "Analysis.h"
#include "TestFile.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <memory>

namespace bench
{

using Log = std::function<void (const juce::String&)>;

struct RunOptions
{
    bool quick = false; // CI: short CPU runs without pacing, shorter sweeps
};

class BenchPlayHead final : public juce::AudioPlayHead
{
public:
    double rate = 48000.0, bpm = 120.0;
    juce::int64 position = 0;

    juce::Optional<PositionInfo> getPosition() const override;
};

// One plugin instance, stereo in and out, with no sidechain.
class Rig
{
public:
    Rig (double rate, double tempo, bool realtime);
    ~Rig();

    juce::Result open (const juce::File& bundle, const juce::String& typeName, int maxBlock);
    juce::Result apply (const PluginEntry& entry, std::vector<ParamSetting>& settings);
    void prepare();

    // Feeds `input` to every main input channel and returns the first output channel.
    Signal render (const Signal& input, int block, const std::function<void (int position)>& beforeBlock = {});

    struct Timing
    {
        std::vector<double> blockLoads; // time / block duration
        double totalSeconds = 0.0, otherThreadsCpuSeconds = 0.0;
    };

    // Renders `input` in real-time mode and times each block; with `pace`, one block per block duration.
    Timing time (const Signal& input, int block, bool pace);

    juce::AudioPluginInstance& plugin() { return *instance; }
    int latency() const { return instance->getLatencySamples(); }
    double sampleRate() const { return rate; }

private:
    double rate;
    bool realtime;
    int maxBlock = 512;
    bool prepared = false;

    juce::VST3PluginFormat format;
    std::unique_ptr<juce::AudioPluginInstance> instance;
    BenchPlayHead playHead;
    juce::AudioBuffer<float> buffer;
    juce::MidiBuffer midi;

    void processOne (const Signal& input, Signal* output, int position, int size);
};

// Every test in the file on one plugin. Returns the result object written to the child's JSON.
juce::var measurePlugin (const TestSpec& spec, const PluginEntry& entry, const RunOptions& options, const Log& log);

// CPU only, at the plugin's defaults: the Fancy Comp reference that every CPU figure is a ratio to.
juce::var measureReference (const TestSpec& spec, const juce::File& bundle, const RunOptions& options, const Log& log);

// Individual measurements, for the CI smoke checks.
struct CpuFigures
{
    double mean = 0.0, p99 = 0.0, worst = 0.0, allThreads = 0.0;
    std::vector<double> runs;
};

CpuFigures measureCpu (Rig& rig, int block, double seconds, int repeats, bool pace, bool uniformSignal, const Log& log);
int measureLatency (Rig& rig, int block, int& reported); // returns the measured delay; below -64 if silent
std::vector<ResponsePoint> measureResponse (Rig& rig, int block, double levelDb, double seconds);
Null measureNull (Rig& rig, int block, bool pink);

void pump (int milliseconds);

} // namespace bench
