#include "VerbChain.h"

#include <cmath>

namespace verb
{

namespace
{
constexpr double maxTrimSeconds = 60.0;

// A skew under 1 gives time knobs more travel at the short end, and still reaches 0.
const std::vector<Setting> noSettings;

const std::vector<Setting> lpHpSettings {
    { "lowpass", "Lowpass", 20.0f, 24000.0f, 24000.0f, 1.0f, 1.0f, 0, "Hz", true },
    { "highpass", "Highpass", 20.0f, 24000.0f, 20.0f, 1.0f, 1.0f, 0, "Hz", true },
};

const std::vector<Setting> echoSettings {
    { "start", "Start pos.", 0.0f, 1000.0f, 0.0f, 0.1f, 0.35f, 1, "ms" },
    { "length", "Length", 0.0f, 5000.0f, 500.0f, 0.1f, 0.35f, 1, "ms" },
    { "spacing", "Spacing", 1.0f, 1000.0f, 100.0f, 0.1f, 1.0f, 1, "ms", true },
    { "error", "Error", 0.0f, 100.0f, 0.0f, 0.1f, 0.5f, 1, "ms" },
    { "errdst", "ErrDst", 0.0f, 1.0f, 0.5f, 0.01f, 1.0f, 2, "" },
    { "startVol", "Start Vol.", -60.0f, 0.0f, -6.0f, 0.1f, 1.0f, 1, "dB" },
    { "endVol", "End Vol.", -60.0f, 0.0f, -24.0f, 0.1f, 1.0f, 1, "dB" },
};

const std::vector<Setting> normalizeSettings {
    { "peak", "Peak Level", -60.0f, 0.0f, 0.0f, 0.1f, 1.0f, 1, "dB" },
};

const std::vector<Setting> trimSettings {
    { "start", "Start", 0.0f, 60000.0f, 0.0f, 1.0f, 0.25f, 0, "ms" },
    { "endCut", "End cut", 0.0f, 60000.0f, 0.0f, 1.0f, 0.25f, 0, "ms" },
    { "stretch", "Stretch", 25.0f, 400.0f, 100.0f, 1.0f, 1.0f, 0, "%", true },
    { "fadeIn", "Fade in", 0.0f, 5000.0f, 0.0f, 1.0f, 0.35f, 0, "ms" },
    { "fadeOut", "Fade out", 0.0f, 5000.0f, 0.0f, 1.0f, 0.35f, 0, "ms" },
    { "gain", "Gain", -60.0f, 24.0f, 0.0f, 0.1f, 1.0f, 1, "dB" },
};

const std::vector<Setting> reverbSettings {
    { "length", "Length", 0.1f, 20.0f, 2.5f, 0.01f, 1.0f, 2, "s", true },
    { "decay", "Decay", 0.05f, 20.0f, 1.8f, 0.01f, 1.0f, 2, "s", true },
    { "density", "Density", 0.0f, 100.0f, 80.0f, 1.0f, 1.0f, 0, "%" },
    { "damping", "Damping", 0.0f, 100.0f, 40.0f, 1.0f, 1.0f, 0, "%" },
    { "spread", "Stereo spread", 0.0f, 100.0f, 100.0f, 1.0f, 1.0f, 0, "%" },
};
} // namespace

const std::vector<Setting>& settingsOf (ModuleType type)
{
    switch (type)
    {
        case ModuleType::lpHp:      return lpHpSettings;
        case ModuleType::echo:      return echoSettings;
        case ModuleType::normalize: return normalizeSettings;
        case ModuleType::trim:      return trimSettings;
        case ModuleType::reverb:    return reverbSettings;
        case ModuleType::file:
        case ModuleType::reverse:
        default:                    return noSettings;
    }
}

juce::String storedName (ModuleType type)
{
    switch (type)
    {
        case ModuleType::file:      return "file";
        case ModuleType::lpHp:      return "lphp";
        case ModuleType::echo:      return "echo";
        case ModuleType::normalize: return "normalize";
        case ModuleType::trim:      return "trim";
        case ModuleType::reverse:   return "reverse";
        case ModuleType::reverb:    return "reverb";
        default:                    return {};
    }
}

juce::String titleOf (ModuleType type)
{
    switch (type)
    {
        case ModuleType::file:      return "FILE";
        case ModuleType::lpHp:      return "LP/HP";
        case ModuleType::echo:      return "ECHO GENERATOR";
        case ModuleType::normalize: return "Normalize";
        case ModuleType::trim:      return "TRIM, GAIN, STRETCH";
        case ModuleType::reverse:   return "Reverse";
        case ModuleType::reverb:    return "REVERB GENERATOR";
        default:                    return {};
    }
}

juce::String chipOf (ModuleType type)
{
    switch (type)
    {
        case ModuleType::file:      return "FILE";
        case ModuleType::lpHp:      return "LP/HP";
        case ModuleType::echo:      return "ECHO";
        case ModuleType::normalize: return "NORMALIZE";
        case ModuleType::trim:      return "TRIM";
        case ModuleType::reverse:   return "REVERSE";
        case ModuleType::reverb:    return "REVERB";
        default:                    return {};
    }
}

juce::String menuNameOf (ModuleType type)
{
    switch (type)
    {
        case ModuleType::file:      return "File";
        case ModuleType::lpHp:      return "LP/HP";
        case ModuleType::echo:      return "Echo generator";
        case ModuleType::normalize: return "Normalize";
        case ModuleType::trim:      return "Trim, Gain, Stretch";
        case ModuleType::reverse:   return "Reverse";
        case ModuleType::reverb:    return "Reverb Generator";
        default:                    return {};
    }
}

std::optional<ModuleType> typeFromStoredName (const juce::String& name)
{
    for (const auto type : allModuleTypes)
        if (storedName (type) == name)
            return type;

    return std::nullopt;
}

juce::String gainModeName (int mode)
{
    return juce::String (gainModeUsesSrc (mode) ? "SRC-gain, " : "no SRC-gain, ")
         + (mode % 4 == 0 ? juce::String ("0dB") : juce::String (gainModeTrimDb (mode), 0) + "dB");
}

double gainModeTrimDb (int mode) noexcept
{
    return -6.0 * (double) (mode % 4);
}

bool gainModeUsesSrc (int mode) noexcept
{
    return mode >= 4;
}

juce::ValueTree makeModule (ModuleType type, int uid)
{
    juce::ValueTree module (ids::module);
    module.setProperty (ids::type, storedName (type), nullptr);
    module.setProperty (ids::on, true, nullptr);
    module.setProperty (ids::uid, uid, nullptr);
    module.setProperty (ids::seed, uid * 7919 + 1, nullptr);

    for (const auto& setting : settingsOf (type))
        module.setProperty (setting.id, setting.defaultValue, nullptr);

    if (type == ModuleType::file)
    {
        module.setProperty (ids::mono, true, nullptr);
        module.setProperty (ids::gainMode, 3, nullptr);
    }

    return module;
}

juce::ValueTree makeChain()
{
    juce::ValueTree chain (ids::chain);
    chain.setProperty (ids::trimStart, 0.0, nullptr);
    chain.setProperty (ids::trimEnd, 0.0, nullptr);
    return chain;
}

void sanitiseChain (juce::ValueTree& chain)
{
    for (int i = chain.getNumChildren(); --i >= 0;)
    {
        const auto child = chain.getChild (i);

        if (! child.hasType (ids::module) || ! typeFromStoredName (child[ids::type].toString()).has_value())
            chain.removeChild (i, nullptr);
    }

    while (chain.getNumChildren() > maxModules)
        chain.removeChild (chain.getNumChildren() - 1, nullptr);

    for (const auto& id : { ids::trimStart, ids::trimEnd })
    {
        const auto value = (double) chain[id];
        chain.setProperty (id, std::isfinite (value) ? juce::jlimit (0.0, maxTrimSeconds, value) : 0.0, nullptr);
    }

    // Two modules sharing a uid would confuse the rack, so a duplicate gets a new one.
    juce::Array<int> used;
    int next = 1;

    for (auto module : chain)
    {
        auto uid = (int) module[ids::uid];

        if (uid <= 0 || used.contains (uid))
        {
            while (used.contains (next) || chain.getChildWithProperty (ids::uid, next).isValid())
                ++next;

            uid = next;
            module.setProperty (ids::uid, uid, nullptr);
        }

        used.add (uid);
    }
}

ChainSpec readChain (const juce::ValueTree& chain)
{
    ChainSpec spec;
    spec.trimStart = chain[ids::trimStart];
    spec.trimEnd = chain[ids::trimEnd];

    for (const auto module : chain)
    {
        const auto type = typeFromStoredName (module[ids::type].toString());

        if (! type.has_value())
            continue;

        ModuleSpec item;
        item.type = *type;
        item.on = module.getProperty (ids::on, true);
        item.uid = module[ids::uid];
        item.seed = module[ids::seed];

        const auto& settings = settingsOf (*type);

        for (size_t i = 0; i < settings.size(); ++i)
        {
            const auto& setting = settings[i];
            const auto value = (float) module.getProperty (setting.id, setting.defaultValue);
            item.values[i] = std::isfinite (value) ? juce::jlimit (setting.low, setting.high, value) : setting.defaultValue;
        }

        if (*type == ModuleType::file)
        {
            item.mono = module.getProperty (ids::mono, true);
            item.gainMode = juce::jlimit (0, numGainModes - 1, (int) module.getProperty (ids::gainMode, 3));
            item.path = module[ids::path].toString();
            item.hash = module[ids::hash].toString();
            item.flac = module[ids::flac].toString();
            item.flacRate = module[ids::rate];
        }

        spec.modules.push_back (std::move (item));
    }

    return spec;
}

} // namespace verb
