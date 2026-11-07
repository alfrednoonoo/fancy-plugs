#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>

#include <array>
#include <memory>
#include <optional>
#include <vector>

// The module chain: what each module type is, and how a chain is stored in plugin state.
// The ValueTree is the saved form, and ChainSpec is the checked copy the IR build works from.
namespace verb
{

enum class ModuleType { file, lpHp, echo, normalize, trim, reverse, reverb };

inline constexpr std::array<ModuleType, 7> allModuleTypes {
    ModuleType::file, ModuleType::lpHp, ModuleType::echo, ModuleType::normalize,
    ModuleType::trim, ModuleType::reverse, ModuleType::reverb
};

inline constexpr int maxModules = 16;
inline constexpr int maxSettings = 7;
inline constexpr int numGainModes = 8;

struct Setting
{
    const char* id;
    const char* label;
    float low, high, defaultValue, interval, skew;
    int decimals;
    const char* unit;
    bool logarithmic = false;
};

const std::vector<Setting>& settingsOf (ModuleType type);

juce::String storedName (ModuleType type);    // in saved state; never change these
juce::String titleOf (ModuleType type);       // card header
juce::String chipOf (ModuleType type);        // chain strip
juce::String menuNameOf (ModuleType type);    // add menu
std::optional<ModuleType> typeFromStoredName (const juce::String& name);

juce::String gainModeName (int mode);
double gainModeTrimDb (int mode) noexcept;
bool gainModeUsesSrc (int mode) noexcept;

namespace ids
{
inline const juce::Identifier chain ("CHAIN"), module ("MODULE"), type ("type"), on ("on"), uid ("uid"),
    seed ("seed"), path ("path"), hash ("hash"), flac ("flac"), rate ("rate"), mono ("mono"),
    gainMode ("gainMode"), trimStart ("trimStart"), trimEnd ("trimEnd");
}

struct IrAudio
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 48000.0;
};

struct ModuleSpec
{
    ModuleType type = ModuleType::lpHp;
    bool on = true;
    int uid = 0;
    int seed = 1;
    std::array<float, maxSettings> values {};

    // File modules only.
    bool mono = true;
    int gainMode = 3;
    juce::String path, hash, flac;
    double flacRate = 0.0;
    std::shared_ptr<const IrAudio> file;
};

struct ChainSpec
{
    std::vector<ModuleSpec> modules;
    double trimStart = 0.0; // seconds from the start of the built IR
    double trimEnd = 0.0;   // seconds from the start; 0 keeps everything
};

juce::ValueTree makeModule (ModuleType type, int uid);

// A chain tree with no modules and no trim.
juce::ValueTree makeChain();

// Removes what can't be used, so a damaged state can't reach the build.
void sanitiseChain (juce::ValueTree& chain);

ChainSpec readChain (const juce::ValueTree& chain);

} // namespace verb
