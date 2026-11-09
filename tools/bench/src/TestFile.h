#pragma once

// FancyBench test files and parameter settings (docs/phase3-Q1.md §3).

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace bench
{

struct PluginEntry
{
    juce::String label, path, name, preset, todo;
    juce::var params; // an object: parameter name or "#index" -> value text, or "=0.5" normalised
    juce::var clicks; // optional: this plugin's own "param", "from" and "to" for the clicks test
};

struct TestSpec
{
    juce::String name;
    double sampleRate = 48000.0;
    std::vector<int> blockSizes { 128 };
    double tempo = 120.0;
    double settle = 2.0; // seconds of silence after the settings, so glides and smoothing finish
    std::vector<PluginEntry> plugins;
    juce::var tests; // an object: test name -> options
    juce::var source; // the whole file, as parsed

    bool has (const juce::String& test) const { return tests.hasProperty (test); }
    juce::var options (const juce::String& test) const { return tests[juce::Identifier (test)]; }
};

juce::Result parseTestFile (const juce::String& text, TestSpec& spec);
juce::Result loadTestFile (const juce::File& file, TestSpec& spec);

// %NAME% environment variables expanded. FANCY_VST3 defaults to %USERPROFILE%\VST3-Dev.
juce::String expandPath (const juce::String& path);

// A number from display text: "4.0:1" -> 4, "2.5k" or "2.50 kHz" -> 2500, "-inf" -> -inf.
// Returns false if the text doesn't start with a number.
bool parseDisplayNumber (const juce::String& text, double& value);

struct ParamSetting
{
    juce::AudioProcessorParameter* parameter = nullptr;
    juce::String name, requested, shown, note;
    float normalised = 0.0f;
};

juce::AudioProcessorParameter* findParameter (juce::AudioProcessor& processor, const juce::String& nameOrIndex, juce::String& error);

// The normalised value for a requested value. Uses the plugin's own text parser, and if what it
// then displays doesn't match, searches the parameter's range for the closest display.
juce::Result resolveValue (juce::AudioProcessorParameter& parameter, const juce::String& requested, ParamSetting& setting);

// Resolves and sets every parameter in `params`, in order.
juce::Result applyParams (juce::AudioProcessor& processor, const juce::var& params, std::vector<ParamSetting>& settings);

} // namespace bench
