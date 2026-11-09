// FancyBench: measures any VST3 the same way, so ours can be compared with the plugins the user
// owns or trials (Phase 3 item Q1, docs/phase3-Q1.md; how-to in docs/bench.md).
//
//   FancyBench params <plugin.vst3>
//   FancyBench run <test.json> [--out <folder>] [--quick]
//   FancyBench smoke <dist> <measurements.txt> <out> <examples>     (CI)
//
// Each plugin is measured in a child process (FancyBench --one ...), so one that crashes or hangs
// can't lose the rest of the session.

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shell32.lib")

#include "Analysis.h"
#include "Host.h"
#include "Report.h"
#include "TestFile.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>
#include <map>

namespace
{

using namespace bench;

constexpr int hangSeconds = 120;

void print (const juce::String& line)
{
    std::cout << line.toStdString() << std::endl;
}

juce::String slug (const juce::String& text)
{
    juce::String result;
    const auto lower = text.toLowerCase();

    for (int i = 0; i < lower.length(); ++i)
        result << (juce::CharacterFunctions::isLetterOrDigit (lower[i]) ? juce::String::charToString (lower[i]) : juce::String ("-"));

    while (result.contains ("--"))
        result = result.replace ("--", "-");

    return result.trimCharactersAtStart ("-").trimCharactersAtEnd ("-").substring (0, 60);
}

juce::File argumentFile (const juce::String& path)
{
    return juce::File::getCurrentWorkingDirectory().getChildFile (expandPath (path));
}

juce::File fancyCompBundle()
{
    return juce::File (expandPath ("%FANCY_VST3%\\Fancy Comp.vst3"));
}

juce::var resultObject (const juce::String& label, const juce::String& path, const juce::String& status, const juce::String& reason)
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("label", label);
    object->setProperty ("path", path);
    object->setProperty ("status", status);
    object->setProperty ("reason", reason);
    return juce::var (object);
}

int usage()
{
    print ("FancyBench: measures VST3 plugins the same way, for head-to-head comparisons.\n\n"
           "  FancyBench params <plugin.vst3>                List a plugin's parameters, for a test file\n"
           "  FancyBench run <test.json> [--out <folder>]    Run a test file and write a report\n\n"
           "Example test files are beside FancyBench.exe, in examples\\. The how-to is docs/bench.md.");
    return 2;
}

// FancyBench params ------------------------------------------------------------------------

int listParams (const juce::String& path)
{
    Rig rig (48000.0, 120.0, false);
    const auto opened = rig.open (argumentFile (path), {}, 512);

    if (opened.failed())
    {
        print ("error: " + opened.getErrorMessage());
        return 2;
    }

    auto& plugin = rig.plugin();
    const auto description = plugin.getPluginDescription();
    print (description.name + " " + description.version + " by " + description.manufacturerName + ": "
           + juce::String (plugin.getParameters().size()) + " parameters\n");
    print ("Use the name in quotes, or #index, in a test file's \"params\". Values are text in the plugin's own units.\n");

    for (auto* parameter : plugin.getParameters())
    {
        const auto label = parameter->getLabel().trim();
        print (juce::String ("#") + juce::String (parameter->getParameterIndex()).paddedLeft (' ', 4) + "  "
               + parameter->getName (512).trim().quoted().paddedRight (' ', 40) + "  " + parameter->getText (parameter->getValue(), 64).trim()
               + (label.isNotEmpty() ? "   [" + label + "]" : juce::String()));
    }

    return 0;
}

// FancyBench --one (the child process) ---------------------------------------------------------

int measureOne (const juce::StringArray& args)
{
    // --one <test.json> <index|reference> <result.json> <log.txt> <quick|full>
    if (args.size() < 6)
        return 2;

    SetErrorMode (SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    timeBeginPeriod (1);

    const juce::File testFile (args[1]), resultFile (args[3]), logFile (args[4]);
    RunOptions options;
    options.quick = args[5] == "quick";

    const Log log = [logFile] (const juce::String& line)
    {
        juce::FileOutputStream out (logFile);

        if (out.openedOk())
        {
            out << line << "\n";
            out.flush();
        }
    };

    TestSpec spec;
    const auto loaded = loadTestFile (testFile, spec);
    juce::var result;

    if (loaded.failed())
        result = resultObject ("?", {}, "failed", loaded.getErrorMessage());
    else if (args[2] == "reference")
        result = measureReference (spec, fancyCompBundle(), options, log);
    else if (juce::isPositiveAndBelow (args[2].getIntValue(), (int) spec.plugins.size()))
        result = measurePlugin (spec, spec.plugins[(size_t) args[2].getIntValue()], options, log);
    else
        result = resultObject ("?", {}, "failed", "no plugin " + args[2]);

    resultFile.replaceWithText (juce::JSON::toString (result));
    log ("done");
    timeEndPeriod (1);
    return 0;
}

// FancyBench run --------------------------------------------------------------------------------

juce::var runChild (const juce::File& testFile, const juce::String& which, const juce::String& label, const juce::File& logs, bool quick)
{
    const auto tag = slug (label).isNotEmpty() ? slug (label) : which;
    const auto resultFile = logs.getChildFile (tag + ".json");
    const auto logFile = logs.getChildFile (tag + ".log");
    resultFile.deleteFile();
    logFile.replaceWithText ({});

    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    juce::ChildProcess child;

    if (! child.start (juce::StringArray { exe.getFullPathName(), "--one", testFile.getFullPathName(), which, resultFile.getFullPathName(),
                                           logFile.getFullPathName(), quick ? "quick" : "full" },
                       0))
        return resultObject (label, {}, "failed", "couldn't start a measuring process");

    juce::int64 offset = 0;
    juce::String pending;
    auto lastActivity = juce::Time::getMillisecondCounter();
    auto hung = false;

    const auto readLog = [&]
    {
        const auto size = logFile.getSize();

        if (size <= offset)
            return false;

        juce::FileInputStream in (logFile);

        if (! in.openedOk())
            return false;

        in.setPosition (offset);
        juce::MemoryBlock data;
        in.readIntoMemoryBlock (data);
        offset += (juce::int64) data.getSize();
        pending << data.toString();

        while (pending.containsChar ('\n'))
        {
            const auto end = pending.indexOfChar ('\n');
            print ("  " + label + ": " + pending.substring (0, end).trimEnd());
            pending = pending.substring (end + 1);
        }

        return true;
    };

    while (child.isRunning())
    {
        juce::Thread::sleep (200);

        if (readLog())
            lastActivity = juce::Time::getMillisecondCounter();
        else if (juce::Time::getMillisecondCounter() - lastActivity > (juce::uint32) hangSeconds * 1000u)
        {
            child.kill();
            hung = true;
            break;
        }
    }

    readLog();

    if (hung)
        return resultObject (label, {}, "failed", "stopped responding for " + juce::String (hangSeconds) + " s; see logs/" + logFile.getFileName());

    juce::var result;

    if (resultFile.existsAsFile() && juce::JSON::parse (resultFile.loadFileAsString(), result).wasOk() && result.isObject())
        return result;

    return resultObject (label, {}, "failed", "the measuring process crashed (exit code " + juce::String ((int) child.getExitCode())
                                                  + "); see logs/" + logFile.getFileName());
}

int runFile (const juce::File& testFile, juce::File folder, bool quick, std::vector<juce::var>* resultsOut = nullptr)
{
    TestSpec spec;
    const auto loaded = loadTestFile (testFile, spec);

    if (loaded.failed())
    {
        print ("error in " + testFile.getFileName() + ": " + loaded.getErrorMessage());
        return 2;
    }

    if (folder == juce::File())
        folder = juce::File::getCurrentWorkingDirectory().getChildFile ("bench-results")
                     .getChildFile (juce::Time::getCurrentTime().formatted ("%Y-%m-%d-%H%M%S") + "-" + slug (spec.name));

    const auto logs = folder.getChildFile ("logs");
    logs.createDirectory();

    print ("FancyBench: " + spec.name + ", " + juce::String ((int) spec.plugins.size()) + " plugins, into " + folder.getFullPathName());

    juce::var reference;

    if (spec.has ("cpu"))
    {
        if (fancyCompBundle().exists())
        {
            print ("Fancy Comp at its defaults, as the CPU reference");
            reference = runChild (testFile, "reference", "Fancy Comp reference", logs, quick);
        }
        else
        {
            print ("note: Fancy Comp isn't installed at " + fancyCompBundle().getFullPathName() + ", so CPU figures have no reference ratio");
        }
    }

    std::vector<juce::var> results;
    auto failures = 0;

    for (size_t i = 0; i < spec.plugins.size(); ++i)
    {
        const auto& entry = spec.plugins[i];
        const juce::File bundle (expandPath (entry.path));

        if (! bundle.exists())
        {
            print (entry.label + ": skipped, not installed at " + bundle.getFullPathName());
            results.push_back (resultObject (entry.label, bundle.getFullPathName(), "skipped",
                                             "not installed at " + bundle.getFullPathName() + (entry.todo.isNotEmpty() ? " (" + entry.todo + ")" : juce::String())));
            continue;
        }

        print (entry.label + ": measuring");
        auto result = runChild (testFile, juce::String ((int) i), entry.label, logs, quick);
        const auto status = result["status"].toString();

        if (status == "failed")
            ++failures;

        print (entry.label + ": " + (status == "ok" ? juce::String ("done") : status + ", " + result["reason"].toString()));
        results.push_back (result);
    }

    writeReport (folder, spec, results, reference, thisMachine());
    print ("Report: " + folder.getChildFile ("report.md").getFullPathName());

    if (resultsOut != nullptr)
        *resultsOut = results;

    return failures == 0 ? 0 : 1;
}

// FancyBench smoke (CI) ---------------------------------------------------------------------------

struct Smoke
{
    int failures = 0;
    juce::StringArray lines;

    void report (const juce::String& check, bool passed, const juce::String& detail)
    {
        const auto line = juce::String (passed ? "PASS  " : "FAIL  ") + check + (detail.isNotEmpty() ? " (" + detail + ")" : juce::String());
        print (line);
        lines.add (line);

        if (! passed)
            ++failures;
    }
};

PluginEntry entryFor (const juce::File& bundle, std::initializer_list<std::pair<const char*, const char*>> params = {})
{
    PluginEntry entry;
    entry.label = bundle.getFileNameWithoutExtension();
    entry.path = bundle.getFullPathName();

    auto* object = new juce::DynamicObject();

    for (const auto& [name, value] : params)
        object->setProperty (name, value);

    entry.params = juce::var (object);
    return entry;
}

std::unique_ptr<Rig> smokeRig (const PluginEntry& entry, bool realtime, juce::String& error)
{
    auto rig = std::make_unique<Rig> (48000.0, 120.0, realtime);
    std::vector<ParamSetting> settings;
    auto result = rig->open (juce::File (entry.path), {}, 512);

    if (result.wasOk())
        result = rig->apply (entry, settings);

    if (result.failed())
    {
        error = result.getErrorMessage();
        return {};
    }

    rig->prepare();
    return rig;
}

int smoke (const juce::StringArray& args)
{
    // smoke <dist> <measurements.txt> <out> <examples>
    if (args.size() < 5)
        return usage();

    timeBeginPeriod (1);

    const auto dist = argumentFile (args[1]);
    const auto measurements = argumentFile (args[2]);
    const auto out = argumentFile (args[3]);
    const auto examples = argumentFile (args[4]);
    out.createDirectory();

    // The examples' %FANCY_VST3% paths, and the CPU reference, point at the built bundles.
    SetEnvironmentVariableW (L"FANCY_VST3", dist.getFullPathName().toWideCharPointer());

    Smoke checks;
    const auto bundle = [&dist] (const char* name) { return dist.getChildFile (juce::String (name) + ".vst3"); };

    // 1. Every plugin's reported latency matches its measured delay.
    for (const auto& file : dist.findChildFiles (juce::File::findDirectories, false, "*.vst3"))
    {
        std::vector<PluginEntry> configurations { entryFor (file) };

        if (file.getFileNameWithoutExtension() == "Fancy FIR")
            configurations.push_back (entryFor (file, { { "FFT size", "1024" } }));

        for (const auto& entry : configurations)
        {
            juce::String error;
            auto rig = smokeRig (entry, false, error);
            const auto name = entry.label + (entry.params.getDynamicObject()->getProperties().isEmpty() ? juce::String (" at defaults") : juce::String (" at FFT 1024"));

            if (rig == nullptr)
            {
                checks.report (name + ": latency", false, error);
                continue;
            }

            int reported = 0;
            const auto measured = measureLatency (*rig, 128, reported);
            const auto difference = measured - reported;
            checks.report (name + ": reported latency matches the measured delay", measured >= -64 && difference >= 0 && difference <= 3,
                          "reported " + juce::String (reported) + ", measured " + (measured < -64 ? juce::String ("no output") : juce::String (measured)));
        }
    }

    const auto text = measurements.loadFileAsString();
    const auto lines = juce::StringArray::fromLines (text);

    // 2. Fancy EQ's bell, against its own design's response from the Catch2 measurements.
    {
        std::vector<std::pair<double, double>> reference;

        for (const auto& line : lines)
        {
            if (! line.startsWith ("bench eq-bell reference:"))
                continue;

            for (const auto& pair : juce::StringArray::fromTokens (line.fromFirstOccurrenceOf (":", false, false), " ", ""))
                if (pair.containsChar (':'))
                    reference.emplace_back (pair.upToFirstOccurrenceOf (":", false, false).getDoubleValue(),
                                            pair.fromFirstOccurrenceOf (":", false, false).getDoubleValue());
        }

        juce::String error;
        auto rig = smokeRig (entryFor (bundle ("Fancy EQ"), { { "Band 2 Freq", "1000" }, { "Band 2 Gain", "6" }, { "Band 2 Q", "1" } }), false, error);

        if (reference.empty() || rig == nullptr)
        {
            checks.report ("Fancy EQ bell: response against its design", false, reference.empty() ? "no reference in " + measurements.getFileName() : error);
        }
        else
        {
            const auto measured = measureResponse (*rig, 128, -20.0, 10.0);
            auto worst = 0.0, worstHz = 0.0;
            auto compared = 0;

            for (const auto& [hz, db] : reference)
            {
                for (const auto& point : measured)
                {
                    if (std::abs (point.hz / hz - 1.0) < 1.0e-3)
                    {
                        ++compared;

                        if (std::abs (point.db - db) > worst)
                        {
                            worst = std::abs (point.db - db);
                            worstHz = hz;
                        }
                    }
                }
            }

            checks.report ("Fancy EQ bell, +6 dB at 1 kHz, Q 1: measured response within 0.1 dB of its design", compared > 200 && worst <= 0.1,
                          juce::String (compared) + " points, worst " + juce::String (worst, 4) + " dB at " + juce::String (worstHz, 1) + " Hz");
        }
    }

    // 3. Neutral settings null.
    for (const auto& entry : { entryFor (bundle ("Fancy EQ")), entryFor (bundle ("Fancy Comp"), { { "Wet", "=0" }, { "Dry", "0" } }) })
    {
        juce::String error;
        auto rig = smokeRig (entry, false, error);

        if (rig == nullptr)
        {
            checks.report (entry.label + ": null", false, error);
            continue;
        }

        const auto result = measureNull (*rig, 128, true);
        checks.report (entry.label + (entry.label == "Fancy EQ" ? " at defaults" : " with Wet off and Dry at 0 dB") + ": nulls against its input",
                      result.rmsDbfs < -120.0, juce::String (result.rmsDbfs, 1) + " dBFS RMS");
    }

    // 4. CPU ratios to Fancy Comp agree with the Catch2 measurements' ratios in the same run.
    {
        std::map<juce::String, double> testRatios;

        for (const auto& line : lines)
        {
            if (! line.startsWith ("workflow cpu: "))
                continue;

            const auto name = line.fromFirstOccurrenceOf ("workflow cpu: ", false, false).upToFirstOccurrenceOf (" default", false, false).trim();
            const auto ratio = line.fromFirstOccurrenceOf ("% (", false, false).upToFirstOccurrenceOf (" x Comp", false, false).getDoubleValue();

            if (ratio > 0.0)
                testRatios[name] = ratio;
        }

        const auto timeCpu = [&] (const PluginEntry& entry, juce::String& error) -> double
        {
            auto rig = smokeRig (entry, true, error);
            return rig == nullptr ? 0.0 : measureCpu (*rig, 128, 5.0, 5, false, true, {}).mean;
        };

        juce::String error;
        const auto reference = timeCpu (entryFor (bundle ("Fancy Comp")), error);

        for (const auto* name : { "Fancy Comp", "Fancy EQ", "Fancy MultiComp", "Fancy Delay" })
        {
            const auto mean = timeCpu (entryFor (bundle (name)), error);
            const auto found = testRatios.find (name);

            if (reference <= 0.0 || mean <= 0.0 || found == testRatios.end())
            {
                checks.report (juce::String (name) + ": CPU ratio to Fancy Comp", false,
                              found == testRatios.end() ? "no Catch2 figure in " + measurements.getFileName() : error);
                continue;
            }

            const auto ratio = mean / reference;
            const auto agreement = ratio / found->second;
            checks.report (juce::String (name) + ": CPU ratio to Fancy Comp within 25 % of the Catch2 figure", std::abs (agreement - 1.0) <= 0.25,
                          "bench " + juce::String (ratio, 2) + " x (" + juce::String (mean, 3) + " %), Catch2 " + juce::String (found->second, 2) + " x");
        }
    }

    // 5. The example files run against our plugins, with competitors skipped.
    for (const auto& file : examples.findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        std::vector<juce::var> results;
        const auto code = runFile (file, out.getChildFile ("examples").getChildFile (file.getFileNameWithoutExtension()), true, &results);
        const auto oursOk = ! results.empty() && results.front()["status"].toString() == "ok";
        checks.report ("example " + file.getFileName() + " runs", code == 0 && oursOk,
                      results.empty() ? "no results" : results.front()["label"].toString() + ": " + results.front()["status"].toString() + " "
                                                        + results.front()["reason"].toString());
    }

    out.getChildFile ("smoke.txt").replaceWithText (checks.lines.joinIntoString ("\n") + "\n");
    print (checks.failures == 0 ? "All bench checks passed" : juce::String (checks.failures) + " bench checks failed");
    timeEndPeriod (1);
    return checks.failures == 0 ? 0 : 1;
}

} // namespace

int main()
{
    SetConsoleOutputCP (CP_UTF8);

    // The wide command line, so paths outside the ANSI code page survive.
    juce::StringArray args;
    int count = 0;

    if (auto** wide = CommandLineToArgvW (GetCommandLineW(), &count))
    {
        for (int i = 1; i < count; ++i)
            args.add (juce::String (wide[i]));

        LocalFree (wide);
    }

    if (args.isEmpty())
        return usage();

    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (args[0] == "--one")
        return measureOne (args);

    if (args[0] == "params" && args.size() >= 2)
        return listParams (args[1]);

    if (args[0] == "run" && args.size() >= 2)
    {
        juce::File folder;
        auto quick = false;

        for (int i = 2; i < args.size(); ++i)
        {
            if (args[i] == "--out" && i + 1 < args.size())
                folder = argumentFile (args[++i]);
            else if (args[i] == "--quick")
                quick = true;
        }

        return runFile (argumentFile (args[1]), folder, quick);
    }

    if (args[0] == "smoke")
        return smoke (args);

    return usage();
}
