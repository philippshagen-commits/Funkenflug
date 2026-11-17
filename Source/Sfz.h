#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <memory>

namespace ff
{
struct SfzSample { std::vector<juce::int16> data; int frames = 0; double rate = 44100.0; };   // stereo interleaved

struct SfzRegion
{
    int lokey = 0, hikey = 127, lovel = 0, hivel = 127, keycenter = 60, sample = -1, offset = 0;
    int onLoCC = -1, onHiCC = -1;          // CC64-triggered regions (pedal noises)
    float tune = 0.0f, keytrack = 100.0f, gain = 1.0f, veltrack = 100.0f, attack = 0.0f, release = 0.3f;
    float rtDecay = 0.0f, lorand = 0.0f, hirand = 1.0f, offTime = 0.006f;
    bool releaseTrigger = false, keyTriggered = true;
};

/** A loaded .sfz instrument (immutable once built, shared between realtime and offline players). */
struct SfzInstrument
{
    std::vector<SfzSample> samples;
    std::vector<SfzRegion> regions;
    juce::String name;
    juce::int64 bytes = 0;

    /** layerStep 1 = every velocity layer, 2 = every second, 4 = every fourth. */
    static std::shared_ptr<SfzInstrument> load (const juce::File&, int layerStep, std::atomic<float>& progress, juce::String& error);
};

class SfzPlayer
{
public:
    void setInstrument (std::shared_ptr<SfzInstrument>);
    std::shared_ptr<SfzInstrument> getInstrument() const { return inst; }
    bool isLoaded() const { return inst != nullptr; }
    void setSampleRate (double sr) { const juce::SpinLock::ScopedLockType l (lock); rate = sr > 8000.0 ? sr : 44100.0; }
    bool render (juce::AudioBuffer<float>&, const juce::MidiBuffer&, int numSamples);

private:
    struct Voice
    {
        const SfzRegion* r = nullptr; const SfzSample* s = nullptr;
        double pos = 0.0, step = 1.0;
        float gain = 0.0f, env = 0.0f, attInc = 1.0f, relCoef = 1.0f;
        bool active = false, releasing = false, attacking = true, isNote = false;
        int key = -1;
        juce::int64 started = 0;
    };
    void handle (const juce::MidiMessage&);
    void start (const SfzRegion&, int key, float vel, float extraGain, bool isNote);
    void releaseKey (int key);
    void renderVoices (float* l, float* r, int n);

    std::shared_ptr<SfzInstrument> inst;
    juce::SpinLock lock;
    double rate = 44100.0;
    Voice voices[160];
    bool down[128] = {}, pending[128] = {};
    float keyVel[128] = {};
    juce::int64 keyOn[128] = {};
    juce::int64 now = 0;
    bool pedal = false;
    juce::Random rng;
    std::vector<float> tl, tr;
};
}
