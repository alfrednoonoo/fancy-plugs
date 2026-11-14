#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif

#include "VerbLibrary.h"

#include "IrBuilder.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <vector>

namespace verb
{

namespace
{
constexpr const char* magic = "FVL1";
constexpr int maxEntries = 256;

ModuleSpec moduleFrom (const juce::var& item, int uid)
{
    ModuleSpec module;
    module.type = typeFromStoredName (item["type"].toString()).value_or (ModuleType::reverse);
    module.uid = uid;
    module.seed = item.hasProperty ("seed") ? (int) item["seed"] : uid * 7919 + 1;
    module.on = true;

    const auto& settings = settingsOf (module.type);

    for (size_t i = 0; i < settings.size(); ++i)
    {
        const auto& setting = settings[i];
        auto value = setting.defaultValue;

        if (item.hasProperty (setting.id))
            value = juce::jlimit (setting.low, setting.high, (float) (double) item[setting.id]);

        module.values[i] = value;
    }

    return module;
}

juce::MemoryBlock encode (const juce::AudioBuffer<float>& audio)
{
    juce::MemoryBlock block;

    {
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::MemoryOutputStream> (block, false);

        juce::FlacAudioFormat format;
        const auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                                .withSampleRate (libraryRate)
                                                                .withNumChannels (audio.getNumChannels())
                                                                .withBitsPerSample (24));

        if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples()))
            return {};
    }

    return block;
}

juce::MemoryBlock resourceData()
{
   #if defined (_WIN32)
    const auto module = static_cast<HMODULE> (juce::Process::getCurrentModuleInstanceHandle());

    if (const auto resource = FindResourceW (module, L"FANCYVERBLIBRARY", MAKEINTRESOURCEW (10))) // RT_RCDATA
        if (const auto handle = LoadResource (module, resource))
            if (const auto* data = LockResource (handle))
                return juce::MemoryBlock (data, (size_t) SizeofResource (module, resource));
   #endif

    return {};
}
} // namespace

std::vector<Recipe> parseRecipes (const juce::String& json)
{
    std::vector<Recipe> recipes;
    const auto root = juce::JSON::parse (json);

    if (const auto* list = root["irs"].getArray())
    {
        for (const auto& item : *list)
        {
            Recipe recipe;
            recipe.category = item["category"].toString();
            recipe.name = item["name"].toString();

            if (const auto* chain = item["chain"].getArray())
            {
                auto uid = 1;

                for (const auto& module : *chain)
                    if (typeFromStoredName (module["type"].toString()).has_value())
                        recipe.chain.modules.push_back (moduleFrom (module, uid++));
            }

            if (recipe.name.isNotEmpty() && ! recipe.chain.modules.empty())
                recipes.push_back (std::move (recipe));
        }
    }

    return recipes;
}

double outputEnergy (const juce::AudioBuffer<float>& ir, double sampleRate, double seconds)
{
    const auto numSamples = ir.getNumSamples();
    const auto window = std::max (1, std::min (numSamples, (int) (seconds * sampleRate)));
    const auto four = ir.getNumChannels() >= 4;

    // The loudest window, so a reversed IR or a slow swell is judged by its body, not its start.
    std::vector<double> running ((size_t) numSamples + 1, 0.0);

    for (int i = 0; i < numSamples; ++i)
    {
        // A centred input reaches the left output through LL (and RL), the right through RR (and LR).
        const auto left = four ? (double) ir.getSample (0, i) + ir.getSample (2, i) : (double) ir.getSample (0, i);
        const auto right = four ? (double) ir.getSample (1, i) + ir.getSample (3, i) : (double) ir.getSample (1, i);
        running[(size_t) i + 1] = running[(size_t) i] + left * left + right * right;
    }

    auto loudest = 0.0;

    for (int start = 0; start + window <= numSamples; start += std::max (1, window / 50))
        loudest = std::max (loudest, running[(size_t) (start + window)] - running[(size_t) start]);

    return numSamples > 0 ? std::max (loudest, running[(size_t) numSamples] - running[(size_t) (numSamples - window)]) : 0.0;
}

juce::AudioBuffer<float> renderRecipe (const Recipe& recipe)
{
    const auto built = buildIr (recipe.chain, libraryRate);
    const auto numSamples = built.audio.getNumSamples();
    const auto trueStereo = built.audio.getNumChannels() >= 4;

    juce::AudioBuffer<float> audio (trueStereo ? 4 : 2, numSamples);

    if (trueStereo)
    {
        // The build holds LL, RR, RL, LR; a four-channel file holds LL, LR, RL, RR.
        const int order[] { 0, 3, 2, 1 };

        for (int ch = 0; ch < 4; ++ch)
            audio.copyFrom (ch, 0, built.audio, order[ch], 0, numSamples);
    }
    else
    {
        for (int ch = 0; ch < 2; ++ch)
            audio.copyFrom (ch, 0, built.audio, ch, 0, numSamples);
    }

    const auto energy = outputEnergy (audio, libraryRate, libraryLoudnessSeconds);

    if (energy > 0.0)
        audio.applyGain ((float) std::sqrt (libraryLoudnessEnergy / energy));

    return audio;
}

juce::MemoryBlock packLibrary (const std::vector<Recipe>& recipes)
{
    juce::MemoryBlock block;
    juce::MemoryOutputStream out (block, false);

    out.writeString (magic);
    out.writeInt ((int) recipes.size());

    for (const auto& recipe : recipes)
    {
        const auto audio = renderRecipe (recipe);
        const auto flac = encode (audio);

        out.writeString (recipe.category);
        out.writeString (recipe.name);
        out.writeInt (audio.getNumChannels());
        out.writeInt64 ((juce::int64) flac.getSize());
        out.write (flac.getData(), flac.getSize());
    }

    out.flush();
    return block;
}

Library::Library (const juce::MemoryBlock& packed)
{
    juce::MemoryInputStream in (packed, false);

    if (packed.getSize() < 8 || in.readString() != magic)
        return;

    const auto count = in.readInt();

    if (count < 0 || count > maxEntries)
        return;

    for (int i = 0; i < count; ++i)
    {
        Entry entry;
        entry.category = in.readString();
        entry.name = in.readString();
        entry.numChannels = in.readInt();

        const auto size = in.readInt64();

        if (size <= 0 || size > in.getNumBytesRemaining() || entry.numChannels < 1 || entry.numChannels > 4)
        {
            entries.clear();
            return;
        }

        entry.flac.setSize ((size_t) size);
        in.read (entry.flac.getData(), (int) size);
        entries.push_back (std::move (entry));
    }
}

const Library& Library::builtIn()
{
    static const Library library (resourceData());
    return library;
}

juce::StringArray Library::categories()
{
    return { "Rooms", "Halls", "Plates", "Chambers", "Special" };
}

const Library::Entry* Library::find (const juce::String& key) const
{
    for (const auto& entry : entries)
        if (entry.key() == key)
            return &entry;

    return nullptr;
}

std::shared_ptr<const IrAudio> Library::load (const juce::String& key) const
{
    const auto* entry = find (key);

    if (entry == nullptr)
        return nullptr;

    auto input = std::make_unique<juce::MemoryInputStream> (entry->flac, false);

    juce::FlacAudioFormat format;
    const std::unique_ptr<juce::AudioFormatReader> reader (format.createReaderFor (input.release(), true));

    if (reader == nullptr || reader->lengthInSamples <= 0)
        return nullptr;

    auto ir = std::make_shared<IrAudio>();
    ir->sampleRate = libraryRate;
    ir->audio.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);

    if (! reader->read (ir->audio.getArrayOfWritePointers(), ir->audio.getNumChannels(), 0, ir->audio.getNumSamples()))
        return nullptr;

    return ir;
}

} // namespace verb
