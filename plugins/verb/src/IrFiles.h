#pragma once

#include "VerbChain.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <map>
#include <vector>

// Reading impulse files, and the embedded copy that keeps a project working when they move.
// Everything here does file or codec work, so none of it may run on the audio thread.
namespace verb
{

juce::String hashOfFile (const juce::File& file); // 64-bit FNV-1a, as hex

std::shared_ptr<const IrAudio> readAudioFile (const juce::File& file);

// 24-bit FLAC as Base64. The file records a nominal rate, so the real rate is saved with it.
juce::String encodeFlac (const IrAudio& ir);
std::shared_ptr<const IrAudio> decodeFlac (const juce::String& base64, double sampleRate);

juce::Array<juce::File> audioFilesBeside (const juce::File& file);

// Finds the audio for each File module: embedded, absolute path, then the user's IR folder.
// Keeps recent results, so moving a knob doesn't decode the file again. One thread only.
class IrResolver
{
public:
    // Fills in each File module's audio, and returns the uids that couldn't be found.
    std::vector<int> resolve (ChainSpec& chain, const juce::File& irFolder);

private:
    std::shared_ptr<const IrAudio> find (const ModuleSpec& module, const juce::File& irFolder);

    std::map<juce::String, std::shared_ptr<const IrAudio>> cache;
};

} // namespace verb
