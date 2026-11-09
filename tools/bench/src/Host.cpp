#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "Host.h"

#include <juce_events/juce_events.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>

namespace bench
{

namespace
{

using Clock = std::chrono::steady_clock;

juce::var makeObject() { return juce::var (new juce::DynamicObject()); }

void set (juce::var& object, const juce::Identifier& name, const juce::var& value)
{
    object.getDynamicObject()->setProperty (name, value);
}

double option (const juce::var& options, const char* name, double fallback)
{
    return options.hasProperty (name) ? (double) options[name] : fallback;
}

std::vector<double> optionList (const juce::var& options, const char* name, std::vector<double> fallback)
{
    if (const auto* list = options[name].getArray())
    {
        std::vector<double> values;

        for (const auto& item : *list)
            values.push_back ((double) item);

        if (! values.empty())
            return values;
    }

    return fallback;
}

double rounded (double value, double step = 0.001)
{
    return std::round (value / step) * step;
}

juce::uint64 ticks (const FILETIME& time)
{
    return ((juce::uint64) time.dwHighDateTime << 32) | time.dwLowDateTime;
}

double threadCpuSeconds()
{
    FILETIME created, exited, kernel, user;

    if (! GetThreadTimes (GetCurrentThread(), &created, &exited, &kernel, &user))
        return 0.0;

    return (double) (ticks (kernel) + ticks (user)) * 1.0e-7;
}

double processCpuSeconds()
{
    FILETIME created, exited, kernel, user;

    if (! GetProcessTimes (GetCurrentProcess(), &created, &exited, &kernel, &user))
        return 0.0;

    return (double) (ticks (kernel) + ticks (user)) * 1.0e-7;
}

Signal zeros (int numSamples) { return Signal ((size_t) std::max (0, numSamples), 0.0f); }

Signal join (const Signal& a, const Signal& b)
{
    Signal result (a);
    result.insert (result.end(), b.begin(), b.end());
    return result;
}

Signal tail (const Signal& signal, int from)
{
    return Signal (signal.begin() + std::min ((size_t) std::max (0, from), signal.size()), signal.end());
}

int maxBlockFor (const TestSpec& spec)
{
    return std::max (512, *std::max_element (spec.blockSizes.begin(), spec.blockSizes.end()));
}

struct Opened
{
    std::unique_ptr<Rig> rig;
    std::vector<ParamSetting> settings;
    juce::String error;
};

Opened openRig (const TestSpec& spec, const PluginEntry& entry, bool realtime, bool prepare = true)
{
    Opened opened;
    opened.rig = std::make_unique<Rig> (spec.sampleRate, spec.tempo, realtime);

    auto result = opened.rig->open (juce::File (expandPath (entry.path)), entry.name, maxBlockFor (spec));

    if (result.wasOk())
        result = opened.rig->apply (entry, opened.settings);

    if (result.failed())
    {
        opened.error = result.getErrorMessage();
        opened.rig.reset();
        return opened;
    }

    if (prepare)
        opened.rig->prepare();

    return opened;
}

juce::var cpuTest (const TestSpec& spec, const PluginEntry& entry, const RunOptions& options, const Log& log)
{
    const auto settings = spec.options ("cpu");
    const auto seconds = options.quick ? 1.0 : option (settings, "seconds", 20.0);
    const auto repeats = options.quick ? 1 : std::max (1, (int) option (settings, "repeats", 5.0));
    const auto pace = options.quick ? false : (bool) settings.getProperty ("pace", true);
    const auto uniform = settings["signal"].toString() == "uniform";

    juce::Array<juce::var> rows;

    for (const auto block : spec.blockSizes)
    {
        auto opened = openRig (spec, entry, true);

        if (opened.rig == nullptr)
            throw std::runtime_error (opened.error.toStdString());

        const auto figures = measureCpu (*opened.rig, block, seconds, repeats, pace, uniform,
                                         [&log, block] (const juce::String& line) { log ("cpu, block " + juce::String (block) + ", " + line); });

        auto row = makeObject();
        set (row, "block", block);
        set (row, "mean", rounded (figures.mean));
        set (row, "p99", rounded (figures.p99));
        set (row, "worst", rounded (figures.worst));
        set (row, "allThreads", rounded (figures.allThreads));
        set (row, "seconds", seconds);
        set (row, "repeats", repeats);
        set (row, "paced", pace);

        juce::Array<juce::var> runs;

        for (const auto run : figures.runs)
            runs.add (rounded (run));

        set (row, "runs", runs);
        rows.add (row);
    }

    return rows;
}

juce::var latencyTest (const TestSpec& spec, const PluginEntry& entry, const Log& log)
{
    auto opened = openRig (spec, entry, false);

    if (opened.rig == nullptr)
        throw std::runtime_error (opened.error.toStdString());

    int reported = 0;
    const auto measured = measureLatency (*opened.rig, spec.blockSizes.front(), reported);

    auto result = makeObject();
    set (result, "reported", reported);

    if (measured < -64)
    {
        set (result, "silent", true);
        log ("latency: reported " + juce::String (reported) + ", no output to measure");
    }
    else
    {
        set (result, "measured", measured);
        set (result, "difference", measured - reported);
        log ("latency: reported " + juce::String (reported) + ", measured " + juce::String (measured));
    }

    return result;
}

juce::var responseTest (const TestSpec& spec, const PluginEntry& entry, const RunOptions& options, const Log& log)
{
    auto opened = openRig (spec, entry, false);

    if (opened.rig == nullptr)
        throw std::runtime_error (opened.error.toStdString());

    const auto settings = spec.options ("response");
    const auto level = option (settings, "level", -20.0);
    const auto points = measureResponse (*opened.rig, spec.blockSizes.front(), level, options.quick ? 2.0 : 10.0);

    juce::Array<juce::var> rows;

    for (const auto& point : points)
        rows.add (juce::Array<juce::var> { rounded (point.hz, 0.01), rounded (point.db), rounded (point.degrees, 0.01) });

    log ("response: " + juce::String ((int) points.size()) + " points at " + juce::String (level, 1) + " dBFS");

    auto result = makeObject();
    set (result, "level", level);
    set (result, "latency", opened.rig->latency());
    set (result, "points", rows);
    return result;
}

juce::var thdTest (const TestSpec& spec, const PluginEntry& entry, const Log& log)
{
    auto opened = openRig (spec, entry, false);

    if (opened.rig == nullptr)
        throw std::runtime_error (opened.error.toStdString());

    auto& rig = *opened.rig;
    const auto settings = spec.options ("thd");
    const auto rate = spec.sampleRate;
    const auto length = analysisLength (rate);
    const auto preroll = (int) rate;

    juce::Array<juce::var> rows;

    for (const auto hz : optionList (settings, "frequencies", { 100.0, 1000.0, 5000.0 }))
    {
        if (hz <= 0.0 || hz >= 0.45 * rate)
            continue;

        const auto actual = binCentred (hz, rate);

        for (const auto level : optionList (settings, "levels", { -20.0, -6.0, 0.0 }))
        {
            const auto output = rig.render (sine (preroll + length, actual, rate, level), spec.blockSizes.front());
            const auto result = distortion (tail (output, preroll), actual, rate);

            auto row = makeObject();
            set (row, "hz", hz);
            set (row, "actualHz", rounded (actual));
            set (row, "level", level);
            set (row, "outputDbfs", rounded (result.fundamentalDbfs));
            set (row, "thdDb", rounded (result.thdDb));
            set (row, "thdPercent", rounded (result.thdPercent, 1.0e-6));
            set (row, "thdnDb", rounded (result.thdnDb));
            set (row, "thdnPercent", rounded (result.thdnPercent, 1.0e-6));
            rows.add (row);

            log ("thd: " + juce::String (juce::roundToInt (hz)) + " Hz at " + juce::String (level, 1) + " dBFS: THD " + juce::String (result.thdDb, 1)
                 + " dB, THD+N " + juce::String (result.thdnDb, 1) + " dB");
        }
    }

    return rows;
}

juce::var aliasingTest (const TestSpec& spec, const PluginEntry& entry, const Log& log)
{
    auto opened = openRig (spec, entry, false);

    if (opened.rig == nullptr)
        throw std::runtime_error (opened.error.toStdString());

    auto& rig = *opened.rig;
    const auto settings = spec.options ("aliasing");
    const auto rate = spec.sampleRate;
    const auto length = analysisLength (rate);
    const auto preroll = (int) rate;
    const auto level = option (settings, "level", 0.0);

    juce::Array<juce::var> rows;

    for (const auto hz : optionList (settings, "frequencies", { 5000.0, 10000.0, 15000.0 }))
    {
        if (hz <= 0.0 || hz >= 0.45 * rate)
            continue;

        const auto actual = binCentred (hz, rate);
        const auto output = rig.render (sine (preroll + length, actual, rate, level), spec.blockSizes.front());
        const auto result = aliasing (tail (output, preroll), actual, rate);

        // The spectrum, reduced to at most 2048 points by keeping each group's loudest bin.
        const auto bins = (int) result.spectrumDbfs.size();
        const auto group = (bins + 2047) / 2048;
        juce::Array<juce::var> spectrum;

        for (int start = 0; start < bins; start += group)
        {
            auto loudest = -400.0f;

            for (int k = start; k < std::min (bins, start + group); ++k)
                loudest = std::max (loudest, result.spectrumDbfs[(size_t) k]);

            spectrum.add (rounded (loudest, 0.1));
        }

        auto row = makeObject();
        set (row, "hz", hz);
        set (row, "actualHz", rounded (actual));
        set (row, "level", level);
        set (row, "outputDbfs", rounded (result.fundamentalDbfs));
        set (row, "aliases", result.count);
        set (row, "worstHz", rounded (result.worstHz, 0.1));
        set (row, "worstDbfs", rounded (result.worstDbfs));
        set (row, "worstRelativeDb", rounded (result.worstRelativeDb));
        set (row, "totalRelativeDb", rounded (result.totalRelativeDb));
        set (row, "spectrumStepHz", group * rate / length);
        set (row, "spectrum", spectrum);
        rows.add (row);

        log ("aliasing: " + juce::String (juce::roundToInt (hz)) + " Hz: " + juce::String (result.count) + " aliases, worst "
             + juce::String (result.worstDbfs, 1) + " dBFS at " + juce::String (result.worstHz, 0) + " Hz");
    }

    return rows;
}

juce::var nullTestFor (const TestSpec& spec, const PluginEntry& entry, const Log& log)
{
    juce::Array<juce::var> rows;

    for (const auto pink : { true, false })
    {
        auto opened = openRig (spec, entry, false);

        if (opened.rig == nullptr)
            throw std::runtime_error (opened.error.toStdString());

        const auto result = measureNull (*opened.rig, spec.blockSizes.front(), pink);

        auto row = makeObject();
        set (row, "signal", pink ? "pink noise, -18 dBFS RMS" : "1 kHz sine, -6 dBFS");
        set (row, "rmsDbfs", rounded (result.rmsDbfs));
        set (row, "peakDbfs", rounded (result.peakDbfs));
        set (row, "relativeDb", rounded (result.relativeDb));
        rows.add (row);

        log ("null, " + juce::String (pink ? "pink noise" : "sine") + ": " + juce::String (result.rmsDbfs, 1) + " dBFS RMS");
    }

    return rows;
}

juce::var clicksTest (const TestSpec& spec, const PluginEntry& entry, const RunOptions& options, const Log& log)
{
    const auto settings = spec.options ("clicks");
    const auto rate = spec.sampleRate;
    const auto period = std::max (0.005, option (settings, "period", 0.05));
    const auto seconds = options.quick ? 1.0 : option (settings, "seconds", 2.0);
    const auto preroll = (int) (0.5 * rate);
    const auto periodSamples = std::max (32, (int) std::round (period * rate));
    const auto input = sine (preroll + (int) (seconds * rate), 1000.0, rate, -12.0);

    ParamSetting from, to;

    const auto render = [&] (bool automate)
    {
        auto opened = openRig (spec, entry, false, false);

        if (opened.rig == nullptr)
            throw std::runtime_error (opened.error.toStdString());

        juce::String error;
        auto* parameter = findParameter (opened.rig->plugin(), settings["param"].toString(), error);

        if (parameter == nullptr)
            throw std::runtime_error (error.toStdString());

        for (auto [setting, text] : { std::pair<ParamSetting*, juce::String> { &from, settings["from"].toString() },
                                      std::pair<ParamSetting*, juce::String> { &to, settings["to"].toString() } })
        {
            const auto resolved = resolveValue (*parameter, text, *setting);

            if (resolved.failed())
                throw std::runtime_error (resolved.getErrorMessage().toStdString());
        }

        parameter->setValue (from.normalised);
        opened.rig->prepare();

        auto current = from.normalised;
        const auto output = opened.rig->render (input, 32, [&] (int position)
        {
            if (! automate || position < preroll)
                return;

            const auto value = ((position - preroll) / periodSamples) % 2 == 0 ? from.normalised : to.normalised;

            if (value != current)
            {
                parameter->setValue (value);
                current = value;
            }
        });

        return tail (output, preroll);
    };

    const auto still = clicks (render (false), 1000.0, rate, -60.0);
    const auto threshold = std::max (-60.0, still.peakDbfs + 6.0);
    const auto moving = clicks (render (true), 1000.0, rate, threshold);

    auto result = makeObject();
    set (result, "param", from.name);
    set (result, "from", from.shown);
    set (result, "to", to.shown);
    set (result, "period", period);
    set (result, "jumps", (int) (seconds / period));
    set (result, "staticPeakDbfs", rounded (still.peakDbfs));
    set (result, "peakDbfs", rounded (moving.peakDbfs));
    set (result, "thresholdDbfs", rounded (threshold));
    set (result, "count", moving.count);

    log ("clicks: " + juce::String (moving.count) + " bursts over " + juce::String (threshold, 1) + " dBFS, loudest "
         + juce::String (moving.peakDbfs, 1) + " dBFS (static " + juce::String (still.peakDbfs, 1) + ")");
    return result;
}

} // namespace

void pump (int milliseconds)
{
    if (auto* manager = juce::MessageManager::getInstanceWithoutCreating())
        manager->runDispatchLoopUntil (milliseconds);
}

juce::Optional<juce::AudioPlayHead::PositionInfo> BenchPlayHead::getPosition() const
{
    PositionInfo info;
    const auto seconds = (double) position / rate;
    const auto ppq = seconds * bpm / 60.0;

    info.setTimeInSamples (position);
    info.setTimeInSeconds (seconds);
    info.setBpm (bpm);
    info.setTimeSignature (TimeSignature {});
    info.setPpqPosition (ppq);
    info.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
    info.setIsPlaying (true);
    return info;
}

Rig::Rig (double sampleRate, double tempo, bool isRealtime)
    : rate (sampleRate), realtime (isRealtime)
{
    playHead.rate = sampleRate;
    playHead.bpm = tempo;
}

Rig::~Rig()
{
    if (instance != nullptr && prepared)
        instance->releaseResources();

    instance.reset();
}

juce::Result Rig::open (const juce::File& bundle, const juce::String& typeName, int block)
{
    maxBlock = block;

    if (! bundle.exists())
        return juce::Result::fail ("not installed at " + bundle.getFullPathName());

    juce::OwnedArray<juce::PluginDescription> types;
    format.findAllTypesForFile (types, bundle.getFullPathName());

    if (types.isEmpty())
        return juce::Result::fail ("couldn't load " + bundle.getFullPathName() + " as a VST3 plugin");

    auto* chosen = types[0];

    if (typeName.isNotEmpty())
    {
        chosen = nullptr;

        for (auto* type : types)
            if (type->name.equalsIgnoreCase (typeName))
                chosen = type;

        if (chosen == nullptr)
        {
            juce::StringArray names;

            for (auto* type : types)
                names.add (type->name);

            return juce::Result::fail ("no plugin called \"" + typeName + "\" in the bundle; it has " + names.joinIntoString (", "));
        }
    }

    juce::String error;
    instance = format.createInstanceFromDescription (*chosen, rate, maxBlock, error);

    if (instance == nullptr)
        return juce::Result::fail ("couldn't create " + chosen->name + ": " + error);

    // Stereo in and out, every other bus off.
    auto layout = instance->getBusesLayout();

    for (int i = 0; i < layout.inputBuses.size(); ++i)
        layout.inputBuses.getReference (i) = i == 0 ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::disabled();

    for (int i = 0; i < layout.outputBuses.size(); ++i)
        layout.outputBuses.getReference (i) = i == 0 ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::disabled();

    if (! instance->setBusesLayout (layout))
    {
        auto fallback = instance->getBusesLayout();

        for (int i = 1; i < fallback.inputBuses.size(); ++i)
            fallback.inputBuses.getReference (i) = juce::AudioChannelSet::disabled();

        instance->setBusesLayout (fallback);
    }

    instance->setNonRealtime (! realtime);
    instance->setPlayHead (&playHead);
    return juce::Result::ok();
}

juce::Result Rig::apply (const PluginEntry& entry, std::vector<ParamSetting>& settings)
{
    if (entry.preset.isNotEmpty())
    {
        const juce::File preset (expandPath (entry.preset));
        juce::MemoryBlock data;

        if (! preset.loadFileAsData (data))
            return juce::Result::fail ("can't read the preset " + preset.getFullPathName());

        // JUCE 8.0.17 loads a .vstpreset through the instance's VST3 client.
        auto* client = instance->getVST3Client();

        if (client == nullptr || ! client->setPreset (data))
            return juce::Result::fail ("the plugin didn't accept the preset " + preset.getFullPathName());
    }

    const auto result = applyParams (*instance, entry.params, settings);
    pump (50);
    return result;
}

void Rig::prepare()
{
    const auto channels = std::max ({ 2, instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels() });
    buffer.setSize (channels, maxBlock);
    instance->prepareToPlay (rate, maxBlock);
    prepared = true;
    playHead.position = 0;
}

void Rig::processOne (const Signal& input, Signal* output, int position, int size)
{
    juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), 0, size);
    view.clear();

    const auto inputs = std::min (instance->getMainBusNumInputChannels(), buffer.getNumChannels());

    for (int channel = 0; channel < inputs; ++channel)
        view.copyFrom (channel, 0, input.data() + position, size);

    midi.clear();
    instance->processBlock (view, midi);
    playHead.position += size;

    if (output != nullptr)
        std::copy (view.getReadPointer (0), view.getReadPointer (0) + size, output->begin() + position);
}

Signal Rig::render (const Signal& input, int block, const std::function<void (int)>& beforeBlock)
{
    block = juce::jlimit (1, maxBlock, block);
    Signal output (input.size(), 0.0f);

    for (int position = 0; position < (int) input.size();)
    {
        const auto size = std::min (block, (int) input.size() - position);

        if (beforeBlock != nullptr)
            beforeBlock (position);

        processOne (input, &output, position, size);
        position += size;
    }

    return output;
}

Rig::Timing Rig::time (const Signal& input, int block, bool pace)
{
    block = juce::jlimit (1, maxBlock, block);
    Timing timing;
    timing.blockLoads.reserve (input.size() / (size_t) block + 1);

    SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    const auto cpuBefore = processCpuSeconds();
    const auto threadBefore = threadCpuSeconds();
    const auto start = Clock::now();

    for (int position = 0; position < (int) input.size();)
    {
        const auto size = std::min (block, (int) input.size() - position);
        const auto before = Clock::now();
        processOne (input, nullptr, position, size);
        const std::chrono::duration<double> spent = Clock::now() - before;

        timing.totalSeconds += spent.count();
        timing.blockLoads.push_back (spent.count() * rate / size);
        position += size;

        if (pace)
        {
            const auto deadline = start + std::chrono::duration_cast<Clock::duration> (std::chrono::duration<double> (position / rate));

            for (auto now = Clock::now(); now < deadline; now = Clock::now())
            {
                if (deadline - now > std::chrono::milliseconds (2))
                    Sleep (1);
                else
                    SwitchToThread();
            }
        }
    }

    // Other threads only: this thread's own time includes the pacing wait, not just the plugin.
    timing.otherThreadsCpuSeconds = std::max (0.0, (processCpuSeconds() - cpuBefore) - (threadCpuSeconds() - threadBefore));
    SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    return timing;
}

CpuFigures measureCpu (Rig& rig, int block, double seconds, int repeats, bool pace, bool uniformSignal, const Log& log)
{
    const auto rate = rig.sampleRate();
    const auto length = (int) (seconds * rate);
    const auto signal = [&] (int run)
    {
        return uniformSignal ? uniformNoise (length, 0.25f, 8) : pinkNoise (length, -18.0, 100u + (uint32_t) run);
    };

    // Two seconds untimed, so buffers, smoothing and background work have settled.
    rig.render (uniformSignal ? uniformNoise ((int) (2.0 * rate), 0.25f, 9) : pinkNoise ((int) (2.0 * rate), -18.0, 99), block);

    std::vector<double> means, p99s, alls;
    CpuFigures figures;

    for (int run = 0; run < repeats; ++run)
    {
        const auto timing = rig.time (signal (run), block, pace);
        const auto audioSeconds = length / rate;
        const auto mean = 100.0 * timing.totalSeconds / audioSeconds;
        const auto p99 = 100.0 * percentile (timing.blockLoads, 0.99);
        const auto worst = 100.0 * *std::max_element (timing.blockLoads.begin(), timing.blockLoads.end());
        // The process call's own time plus every other thread's, such as a plugin's workers.
        // GetProcessTimes counts in 15.6 ms ticks, so a run under 5 s is too short to say.
        const auto all = audioSeconds >= 5.0 ? mean + 100.0 * timing.otherThreadsCpuSeconds / audioSeconds : -1.0;

        means.push_back (mean);
        p99s.push_back (p99);
        alls.push_back (all);
        figures.worst = std::max (figures.worst, worst);
        figures.runs.push_back (mean);

        if (log != nullptr)
            log ("run " + juce::String (run + 1) + " of " + juce::String (repeats) + ": mean " + juce::String (mean, 3) + " %, 99th "
                 + juce::String (p99, 3) + " %, worst " + juce::String (worst, 2) + " %, all threads "
                 + (all >= 0.0 ? juce::String (all, 3) + " %" : juce::String ("too short to measure")));
    }

    figures.mean = median (means);
    figures.p99 = median (p99s);
    figures.allThreads = median (alls);
    return figures;
}

int measureLatency (Rig& rig, int block, int& reported)
{
    const auto rate = rig.sampleRate();
    const auto input = whiteNoise ((int) (3.0 * rate), -20.0, 5);
    const auto output = rig.render (input, block);

    reported = rig.latency();
    const auto maxLag = std::min (std::max (2 * reported + 1024, (int) rate), (int) input.size() / 2);
    return measureDelay (input, output, -64, maxLag);
}

std::vector<ResponsePoint> measureResponse (Rig& rig, int block, double levelDb, double seconds)
{
    const auto rate = rig.sampleRate();
    rig.render (zeros ((int) (2.0 * rate)), block);

    const auto sweep = logSweep (10.0, 0.48 * rate, seconds, rate, levelDb);
    const auto output = rig.render (join (sweep.signal, zeros ((int) (2.0 * rate) + rig.latency())), block);
    const auto frequencies = logFrequencies (20.0, std::min (20000.0, 0.45 * rate), 24);
    return sweepResponse (sweep, output, rig.latency(), frequencies);
}

Null measureNull (Rig& rig, int block, bool pink)
{
    const auto rate = rig.sampleRate();
    const auto length = (int) (3.0 * rate);
    const auto input = pink ? pinkNoise (length, -18.0, 11) : sine (length, 1000.0, rate, -6.0);
    const auto output = rig.render (input, block);
    return nullTest (input, output, rig.latency(), (int) rate);
}

juce::var measurePlugin (const TestSpec& spec, const PluginEntry& entry, const RunOptions& options, const Log& log)
{
    auto result = makeObject();
    set (result, "label", entry.label);
    set (result, "path", expandPath (entry.path));

    auto probe = openRig (spec, entry, false);

    if (probe.rig == nullptr)
    {
        set (result, "status", entry.todo.isNotEmpty() ? "skipped" : "failed");
        set (result, "reason", entry.todo.isNotEmpty() ? probe.error + " (" + entry.todo + ")" : probe.error);
        log ("can't run: " + probe.error);
        return result;
    }

    const auto& description = probe.rig->plugin().getPluginDescription();
    set (result, "plugin", description.name);
    set (result, "vendor", description.manufacturerName);
    set (result, "version", description.version);
    set (result, "parameters", probe.rig->plugin().getParameters().size());

    juce::Array<juce::var> settings;

    for (const auto& setting : probe.settings)
    {
        auto row = makeObject();
        set (row, "param", setting.name);
        set (row, "requested", setting.requested);
        set (row, "shown", setting.shown);

        if (setting.note.isNotEmpty())
            set (row, "note", setting.note);

        settings.add (row);
    }

    set (result, "settings", settings);
    probe.rig.reset();

    juce::StringArray errors;

    const auto attempt = [&] (const char* name, const std::function<juce::var()>& test)
    {
        if (! spec.has (name))
            return;

        try
        {
            set (result, name, test());
        }
        catch (const std::exception& error)
        {
            errors.add (juce::String (name) + ": " + error.what());
            log (juce::String (name) + " failed: " + error.what());
        }
    };

    attempt ("cpu", [&] { return cpuTest (spec, entry, options, log); });
    attempt ("latency", [&] { return latencyTest (spec, entry, log); });
    attempt ("response", [&] { return responseTest (spec, entry, options, log); });
    attempt ("thd", [&] { return thdTest (spec, entry, log); });
    attempt ("aliasing", [&] { return aliasingTest (spec, entry, log); });
    attempt ("null", [&] { return nullTestFor (spec, entry, log); });
    attempt ("clicks", [&] { return clicksTest (spec, entry, options, log); });

    set (result, "status", errors.isEmpty() ? "ok" : "failed");

    if (! errors.isEmpty())
        set (result, "reason", errors.joinIntoString ("; "));

    return result;
}

juce::var measureReference (const TestSpec& spec, const juce::File& bundle, const RunOptions& options, const Log& log)
{
    PluginEntry entry;
    entry.label = "Fancy Comp at its defaults (reference)";
    entry.path = bundle.getFullPathName();

    auto result = makeObject();
    set (result, "label", entry.label);
    set (result, "path", entry.path);

    try
    {
        set (result, "cpu", cpuTest (spec, entry, options, log));
        set (result, "status", "ok");
    }
    catch (const std::exception& error)
    {
        set (result, "status", "failed");
        set (result, "reason", juce::String (error.what()));
    }

    return result;
}

} // namespace bench
