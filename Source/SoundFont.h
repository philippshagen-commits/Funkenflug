#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

struct tsf;

namespace ff
{
/** Plays .sf2 SoundFonts (TinySoundFont, MIT licence). */
class SoundFontPlayer
{
public:
    SoundFontPlayer() = default;
    ~SoundFontPlayer();

    bool load (const juce::File&, double sampleRate);
    void unload();
    bool isLoaded() const { return sf != nullptr; }
    void setSampleRate (double);
    juce::StringArray presetNames() const;
    void setPreset (int index);
    int getPreset() const { return preset; }
    juce::File getFile() const { return file; }

    /** Audio thread. Returns false when no SoundFont is loaded (or it is being swapped). */
    bool render (juce::AudioBuffer<float>&, const juce::MidiBuffer&, int numSamples);
    void allOff();
    std::unique_ptr<SoundFontPlayer> cloneForOffline (double sampleRate) const;

private:
    void apply (const juce::MidiMessage&);
    tsf* sf = nullptr;
    juce::SpinLock lock;
    std::vector<float> inter;
    juce::File file;
    int preset = 0;
    double rate = 44100.0;
};
}
