#include "IrFiles.h"

#include "IrBuilder.h"

namespace verb
{

namespace
{
constexpr size_t maxCachedFiles = 8;
constexpr double flacNominalRate = 48000.0;

std::shared_ptr<const IrAudio> readFrom (juce::AudioFormatReader& reader, double sampleRate)
{
    const auto numChannels = juce::jlimit (1, 2, (int) reader.numChannels);
    const auto limit = (juce::int64) (maxIrSeconds * sampleRate);
    const auto numSamples = (int) juce::jmin (reader.lengthInSamples, limit);

    if (numSamples <= 0 || sampleRate <= 0.0)
        return nullptr;

    auto ir = std::make_shared<IrAudio>();
    ir->sampleRate = sampleRate;
    ir->audio.setSize (numChannels, numSamples);

    if (! reader.read (&ir->audio, 0, numSamples, 0, true, true))
        return nullptr;

    return ir;
}
} // namespace

juce::String hashOfFile (const juce::File& file)
{
    juce::FileInputStream stream (file);

    if (! stream.openedOk())
        return {};

    juce::uint64 hash = 0xcbf29ce484222325ull;
    juce::HeapBlock<juce::uint8> buffer (65536);

    for (;;)
    {
        const auto count = stream.read (buffer.get(), 65536);

        if (count <= 0)
            break;

        for (int i = 0; i < count; ++i)
        {
            hash ^= buffer[i];
            hash *= 0x100000001b3ull;
        }
    }

    return juce::String::toHexString ((juce::int64) hash);
}

std::shared_ptr<const IrAudio> readAudioFile (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    const std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
        return nullptr;

    return readFrom (*reader, reader->sampleRate);
}

juce::String encodeFlac (const IrAudio& ir)
{
    juce::MemoryBlock block;

    {
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::MemoryOutputStream> (block, false);

        juce::FlacAudioFormat format;
        const auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                                .withSampleRate (flacNominalRate)
                                                                .withNumChannels (ir.audio.getNumChannels())
                                                                .withBitsPerSample (24));

        if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (ir.audio, 0, ir.audio.getNumSamples()))
            return {};
    }

    return juce::Base64::toBase64 (block.getData(), block.getSize());
}

std::shared_ptr<const IrAudio> decodeFlac (const juce::String& base64, double sampleRate)
{
    if (base64.isEmpty() || sampleRate <= 0.0)
        return nullptr;

    juce::MemoryOutputStream decoded;

    if (! juce::Base64::convertFromBase64 (decoded, base64))
        return nullptr;

    auto input = std::make_unique<juce::MemoryInputStream> (decoded.getData(), decoded.getDataSize(), false);

    juce::FlacAudioFormat format;
    const std::unique_ptr<juce::AudioFormatReader> reader (format.createReaderFor (input.release(), true));

    if (reader == nullptr)
        return nullptr;

    return readFrom (*reader, sampleRate);
}

juce::Array<juce::File> audioFilesBeside (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    auto files = file.getParentDirectory().findChildFiles (juce::File::findFiles, false, formats.getWildcardForAllFormats());
    files.sort();
    return files;
}

std::shared_ptr<const IrAudio> IrResolver::find (const ModuleSpec& module, const juce::File& irFolder)
{
    if (auto embedded = decodeFlac (module.flac, module.flacRate))
        return embedded;

    if (juce::File::isAbsolutePath (module.path))
    {
        const juce::File file (module.path);

        if (file.existsAsFile())
            if (auto audio = readAudioFile (file))
                return audio;
    }

    if (irFolder.isDirectory() && module.path.isNotEmpty() && module.hash.isNotEmpty())
    {
        const auto name = module.path.fromLastOccurrenceOf ("\\", false, false).fromLastOccurrenceOf ("/", false, false);

        for (const auto& candidate : irFolder.findChildFiles (juce::File::findFiles, true, name))
            if (hashOfFile (candidate) == module.hash)
                if (auto audio = readAudioFile (candidate))
                    return audio;
    }

    return nullptr;
}

std::vector<int> IrResolver::resolve (ChainSpec& chain, const juce::File& irFolder)
{
    std::vector<int> missing;

    for (auto& module : chain.modules)
    {
        if (module.type != ModuleType::file || (module.path.isEmpty() && module.flac.isEmpty()))
            continue;

        const auto key = module.hash + "|" + module.path;

        if (const auto found = cache.find (key); found != cache.end())
        {
            module.file = found->second;
            continue;
        }

        module.file = find (module, irFolder);

        if (module.file == nullptr)
        {
            missing.push_back (module.uid);
            continue;
        }

        if (cache.size() >= maxCachedFiles)
            cache.clear();

        cache[key] = module.file;
    }

    return missing;
}

} // namespace verb
