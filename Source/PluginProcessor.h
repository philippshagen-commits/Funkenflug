#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <mutex>
#include "Settings.h"
#include "Model.h"
#include "SoundFont.h"
#include "Sfz.h"

class FunkenflugProcessor : public juce::AudioProcessor, private juce::ValueTree::Listener
{
public:
    FunkenflugProcessor();
    ~FunkenflugProcessor() override;

    void prepareToPlay (double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Funkenflug"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::ValueTree state;
    ff::Snapshot getSnapshot() const { const juce::SpinLock::ScopedLockType l (snapLock); return snap; }

    // live input
    double liveNow() const;
    void drainLive();
    template <typename Fn> void withLive (Fn&& f) { std::lock_guard<std::mutex> l (liveMx); f (live); }
    void injectNote (int key, float vel, bool on);

    // transport
    void setTrack (std::shared_ptr<ff::Track>);
    std::shared_ptr<ff::Track> getTrack() const;
    void play();
    void pause();
    void stop();
    void seek (double seconds);
    bool isPlaying() const { return playing.load(); }
    double playheadNow() const;

    // recording
    void startRecording();
    std::shared_ptr<ff::Track> stopRecording();
    bool isRecording() const { return recording.load(); }

    // hosted instrument (standalone)
    juce::String loadHostedPlugin (const juce::String& fileOrId, const juce::MemoryBlock* stateToRestore = nullptr);
    void unloadHostedPlugin();
    juce::AudioPluginInstance* getHosted() const { return hosted.get(); }

    // SoundFont (.sf2)
    juce::String loadSoundFontNow (const juce::File&, int sfzLayerStep);            // blocking; .sf2 or .sfz
    void loadSoundFontAsync (const juce::File&, std::function<void (juce::String)> onDone);
    void unloadSoundFont();
    bool hasSampleInstrument() const { return soundFont.isLoaded() || sfz.isLoaded(); }
    juce::String sampleInstrumentName() const;
    int defaultSfzLayerStep() const;
    ff::SoundFontPlayer soundFont;
    ff::SfzPlayer sfz;
    std::atomic<float> loadProgress { 0.0f };
    std::atomic<bool> loading { false };

    bool renderAudioOffline (const ff::Track&, const juce::File& wav, double leadIn, double tail,
                             std::atomic<float>& progress, std::atomic<bool>& cancel);

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void updateSnapshot();

    mutable juce::SpinLock snapLock;
    ff::Snapshot snap;
    std::atomic<bool> synthOn { true };
    std::atomic<float> volGain { 1.0f };
    static void finishOutput (juce::AudioBuffer<float>&, int numSamples, float gain);

    struct LiveEv { double t; juce::uint8 d[3]; };
    juce::AbstractFifo fifo { 8192 };
    std::vector<LiveEv> fifoData;
    std::mutex liveMx;
    ff::TrackBuilder live;
    juce::MidiMessageSequence recSeq;
    double recStart = 0.0;
    std::atomic<bool> recording { false };

    juce::MidiMessageCollector collector;
    juce::Synthesiser synth;

    double sr = 44100.0;
    int blockSize = 512;
    juce::int64 clock = 0;
    std::atomic<double> clockSec { 0.0 }, clockStamp { 0.0 }, playSec { 0.0 }, playStamp { 0.0 };

    mutable juce::SpinLock trackLock;
    std::shared_ptr<ff::Track> track;
    double playPos = 0.0;      // samples
    size_t evIdx = 0;
    std::atomic<bool> playing { false }, pendingAllOff { false };
    std::atomic<double> seekReq { -1.0 };

    juce::AudioPluginFormatManager formats;
    juce::CriticalSection hostedLock;
    std::unique_ptr<juce::AudioPluginInstance> hosted;
    juce::String hostedId;
    juce::AudioBuffer<float> hostBuf;
    std::shared_ptr<std::atomic<bool>> alive { std::make_shared<std::atomic<bool>> (true) };
    juce::ThreadPool loader { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FunkenflugProcessor)
};
