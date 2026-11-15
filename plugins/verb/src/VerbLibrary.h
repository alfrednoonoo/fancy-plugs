#pragma once

#include "VerbChain.h"

#include <juce_core/juce_core.h>

#include <memory>
#include <vector>

// Fancy Verb's IR library (Phase 3 Q2.8, docs/verb.md): 32 IRs made at build time from
// plugins/verb/library/recipes.json by tools/verb-library, with the plugin's own generators, and
// stored as 24-bit FLAC in one file that's built into the plugin as a Windows resource.
namespace verb
{

inline constexpr double libraryRate = 48000.0;
inline constexpr double libraryLoudnessSeconds = 0.5;
inline constexpr double libraryLoudnessEnergy = 1.0; // output energy over the loudest 0.5 s

struct Recipe
{
    juce::String category, name;
    ChainSpec chain;
};

// Reads the recipes file. Unknown modules and settings are ignored; a broken file gives none.
std::vector<Recipe> parseRecipes (const juce::String& json);

// The IR for a recipe at 48 kHz, at the library's loudness. Two channels, or four for true
// stereo in a four-channel file's order: LL, LR, RL, RR.
juce::AudioBuffer<float> renderRecipe (const Recipe& recipe);

// The energy of what a centred input gives at both outputs over the loudest 0.5 s.
double outputEnergy (const juce::AudioBuffer<float>& ir, double sampleRate, double seconds);

// The library file: each IR's category, name and FLAC data.
juce::MemoryBlock packLibrary (const std::vector<Recipe>& recipes);

class Library
{
public:
    struct Entry
    {
        juce::String category, name;
        int numChannels = 2;
        juce::MemoryBlock flac;

        juce::String key() const { return category + "/" + name; }
        bool isTrueStereo() const noexcept { return numChannels >= 4; }
    };

    // Reads a packed library. A damaged one gives no entries.
    explicit Library (const juce::MemoryBlock& packed);

    // The library built into this program or plugin, read once.
    static const Library& builtIn();

    static juce::StringArray categories();

    const std::vector<Entry>& getEntries() const noexcept { return entries; }
    const Entry* find (const juce::String& key) const;

    // Decodes an entry's IR. Not on the audio thread.
    std::shared_ptr<const IrAudio> load (const juce::String& key) const;

private:
    std::vector<Entry> entries;
};

#if ! defined (_WIN32)
// The packed library, embedded by the assembler in a file the build writes (VerbLibraryEmbed.cpp,
// docs/mac.md). Windows reads the same file from a resource instead.
juce::MemoryBlock embeddedLibraryData();
#endif

} // namespace verb
