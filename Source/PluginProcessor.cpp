#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace
{
struct PianoSound : juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

/** A small, soft additive piano-ish voice so the standalone app is never silent. */
struct PianoVoice : juce::SynthesiserVoice
{
    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<PianoSound*> (s) != nullptr; }

    void startNote (int note, float vel, juce::SynthesiserSound*, int) override
    {
        const double f = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        inc = f / getSampleRate();
        ph = 0.0;
        level = 0.03f + 0.25f * vel * vel;
        bright = 0.25f + 0.75f * vel;
        env = 1.0f; att = 0.0f; releasing = false;
        const double half = juce::jmap (juce::jlimit (21.0, 108.0, (double) note), 21.0, 108.0, 3.2, 0.3);
        decay = (float) std::exp (-0.6931 / (half * getSampleRate()));
        rel = (float) std::exp (-1.0 / (0.09 * getSampleRate()));
        attInc = (float) (1.0 / (0.003 * getSampleRate()));
        hi = f < 3000.0;
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff) releasing = true;
        else { env = 0.0f; clearCurrentNote(); }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int n) override
    {
        if (! isVoiceActive()) return;
        const double tw = juce::MathConstants<double>::twoPi;
        for (int i = 0; i < n; ++i)
        {
            const float b = bright * env;
            float s = (float) std::sin (tw * ph);
            if (hi) s += b * (0.5f * (float) std::sin (tw * 2.0 * ph) + 0.22f * b * (float) std::sin (tw * 3.0 * ph) + 0.1f * b * b * (float) std::sin (tw * 4.0 * ph));
            att = juce::jmin (1.0f, att + attInc);
            const float v = s * level * env * att;
            for (int c = 0; c < out.getNumChannels(); ++c) out.addSample (c, start + i, v);
            ph += inc; if (ph >= 1.0) ph -= 1.0;
            env *= decay;
            if (releasing) env *= rel;
            if (env < 0.0004f) { clearCurrentNote(); break; }
        }
    }

    double ph = 0.0, inc = 0.0;
    float level = 0, bright = 0, env = 0, decay = 1, rel = 1, att = 0, attInc = 1;
    bool releasing = false, hi = true;
};

void setupSynth (juce::Synthesiser& s, double sampleRate)
{
    s.clearVoices(); s.clearSounds();
    for (int i = 0; i < 48; ++i) s.addVoice (new PianoVoice());
    s.addSound (new PianoSound());
    s.setCurrentPlaybackSampleRate (sampleRate);
}
}

//==============================================================================
FunkenflugProcessor::FunkenflugProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (ff::makeDefaultState())
{
    fifoData.resize ((size_t) fifo.getTotalSize());
    formats.addFormat (new juce::VST3PluginFormat());
    if (wrapperType != wrapperType_Standalone)
        state.setProperty ("builtInSynth", 0, nullptr);     // inside a DAW the piano VST makes the sound
    updateSnapshot();
    state.addListener (this);
    setupSynth (synth, 44100.0);
}

FunkenflugProcessor::~FunkenflugProcessor()
{
    *alive = false;
    loader.removeAllJobs (true, 60000);
    state.removeListener (this);
    unloadHostedPlugin();
}

void FunkenflugProcessor::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) { updateSnapshot(); }

void FunkenflugProcessor::updateSnapshot()
{
    auto s = ff::makeSnapshot (state);
    synthOn = s.c[ff::builtInSynth] != 0;
    volGain = juce::Decibels::decibelsToGain (s.f[ff::volume]);
    const juce::SpinLock::ScopedLockType l (snapLock);
    snap = s;
}

bool FunkenflugProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void FunkenflugProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate; blockSize = samplesPerBlock;
    collector.reset (sampleRate);
    synth.setCurrentPlaybackSampleRate (sampleRate);
    soundFont.setSampleRate (sampleRate);
    sfz.setSampleRate (sampleRate);
    const juce::ScopedLock l (hostedLock);
    if (hosted != nullptr)
    {
        hosted->releaseResources();
        hosted->setRateAndBufferSizeDetails (sr, blockSize);
        hosted->prepareToPlay (sr, blockSize);
        hostBuf.setSize (juce::jmax (2, hosted->getTotalNumInputChannels(), hosted->getTotalNumOutputChannels()), blockSize * 2);
    }
}

void FunkenflugProcessor::releaseResources()
{
    const juce::ScopedLock l (hostedLock);
    if (hosted != nullptr) hosted->releaseResources();
}

void FunkenflugProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    collector.removeNextBlockOfMessages (midi, n);

    // live events -> visual fifo
    const double blockT = (double) clock / sr;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.getRawDataSize() > 3 || ! (m.isNoteOnOrOff() || m.isController() || m.isPitchWheel())) continue;
        const auto scope = fifo.write (1);
        if (scope.blockSize1 > 0)
        {
            auto& e = fifoData[(size_t) scope.startIndex1];
            e.t = blockT + meta.samplePosition / sr;
            e.d[0] = e.d[1] = e.d[2] = 0;
            memcpy (e.d, m.getRawData(), (size_t) m.getRawDataSize());
        }
    }

    // file / take playback
    {
        const juce::SpinLock::ScopedTryLockType l (trackLock);
        if (l.isLocked())
        {
            auto* tr = track.get();
            bool allOff = pendingAllOff.exchange (false);
            const double sk = seekReq.exchange (-1.0);
            if (sk >= 0.0)
            {
                playPos = sk * sr;
                evIdx = 0;
                if (tr != nullptr)
                    evIdx = (size_t) (std::lower_bound (tr->evs.begin(), tr->evs.end(), sk,
                                                        [] (const ff::Track::Ev& e, double t) { return e.t < t; }) - tr->evs.begin());
                allOff = true;
            }
            if (allOff)
                for (int ch = 1; ch <= 16; ++ch)
                {
                    midi.addEvent (juce::MidiMessage::controllerEvent (ch, 64, 0), 0);
                    midi.addEvent (juce::MidiMessage::allNotesOff (ch), 0);
                }
            if (playing.load() && tr != nullptr)
            {
                const double t0 = playPos / sr, t1 = (playPos + n) / sr;
                while (evIdx < tr->evs.size() && tr->evs[evIdx].t < t1)
                {
                    auto& e = tr->evs[evIdx++];
                    midi.addEvent (e.d, e.n, juce::jlimit (0, n - 1, (int) ((e.t - t0) * sr)));
                }
                playPos += n;
                if (t1 > tr->length + 4.0) { playing = false; pendingAllOff = true; }
            }
            playSec = playPos / sr;
            playStamp = juce::Time::getMillisecondCounterHiRes();
        }
    }

    // sound
    bool done = false;
    {
        const juce::ScopedTryLock l (hostedLock);
        if (l.isLocked() && hosted != nullptr)
        {
            if (hostBuf.getNumSamples() >= n)
            {
                juce::AudioBuffer<float> view (hostBuf.getArrayOfWritePointers(), hostBuf.getNumChannels(), n);
                view.clear();
                hosted->processBlock (view, midi);
                const int outs = juce::jmax (1, hosted->getTotalNumOutputChannels());
                for (int c = 0; c < buffer.getNumChannels(); ++c)
                    buffer.copyFrom (c, 0, view, juce::jmin (c, outs - 1, view.getNumChannels() - 1), 0, n);
            }
            done = true;
        }
    }
    if (! done) done = soundFont.render (buffer, midi, n);
    if (! done) done = sfz.render (buffer, midi, n);
    if (! done && synthOn.load())
        synth.renderNextBlock (buffer, midi, 0, n);
    else
        synth.allNotesOff (0, false);
    finishOutput (buffer, n, volGain.load());
    midi.clear();

    clock += n;
    clockSec = (double) clock / sr;
    clockStamp = juce::Time::getMillisecondCounterHiRes();
}

void FunkenflugProcessor::finishOutput (juce::AudioBuffer<float>& b, int n, float gain)
{
    for (int c = 0; c < b.getNumChannels(); ++c)       // volume, then a soft knee so loud chords never clip harshly
    {
        auto* d = b.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            float x = d[i] * gain;
            const float a = std::abs (x);
            if (a > 0.7f) x = (x < 0.0f ? -1.0f : 1.0f) * (0.7f + 0.29f * std::tanh ((a - 0.7f) / 0.29f));
            d[i] = x;
        }
    }
}

//==============================================================================
double FunkenflugProcessor::liveNow() const
{
    const double ahead = (juce::Time::getMillisecondCounterHiRes() - clockStamp.load()) * 0.001;
    return clockSec.load() + juce::jlimit (0.0, 0.1, ahead);
}

double FunkenflugProcessor::playheadNow() const
{
    if (! playing.load()) return playSec.load();
    const double ahead = (juce::Time::getMillisecondCounterHiRes() - playStamp.load()) * 0.001;
    return playSec.load() + juce::jlimit (0.0, 0.1, ahead);
}

void FunkenflugProcessor::drainLive()
{
    std::lock_guard<std::mutex> l (liveMx);
    const auto scope = fifo.read (fifo.getNumReady());
    auto handle = [this] (int start, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            auto& e = fifoData[(size_t) (start + i)];
            const juce::MidiMessage m (e.d[0], e.d[1], e.d[2]);
            live.add (m, e.t);
            if (recording.load() && e.t >= recStart)
                recSeq.addEvent (m, e.t - recStart);
        }
    };
    handle (scope.startIndex1, scope.blockSize1);
    handle (scope.startIndex2, scope.blockSize2);
    live.prune (clockSec.load() - 14.0);
}

void FunkenflugProcessor::injectNote (int key, float vel, bool on)
{
    auto m = on ? juce::MidiMessage::noteOn (1, key, vel) : juce::MidiMessage::noteOff (1, key);
    m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
    collector.addMessageToQueue (m);
}

void FunkenflugProcessor::setTrack (std::shared_ptr<ff::Track> t)
{
    playing = false;
    {
        const juce::SpinLock::ScopedLockType l (trackLock);
        std::swap (track, t);
        playPos = 0.0; evIdx = 0;
    }
    playSec = 0.0;
    seekReq = 0.0;
}

std::shared_ptr<ff::Track> FunkenflugProcessor::getTrack() const
{
    const juce::SpinLock::ScopedLockType l (trackLock);
    return track;
}

void FunkenflugProcessor::play()
{
    if (auto t = getTrack())
    {
        if (playSec.load() >= t->length + 3.9) seekReq = 0.0;
        playStamp = juce::Time::getMillisecondCounterHiRes();
        playing = true;
    }
}
void FunkenflugProcessor::pause() { playing = false; pendingAllOff = true; }
void FunkenflugProcessor::stop()  { playing = false; seekReq = 0.0; playSec = 0.0; }
void FunkenflugProcessor::seek (double s) { seekReq = juce::jmax (0.0, s); playSec = juce::jmax (0.0, s); }

void FunkenflugProcessor::startRecording()
{
    drainLive();
    std::lock_guard<std::mutex> l (liveMx);
    recSeq.clear();
    recStart = clockSec.load();
    recording = true;
}

std::shared_ptr<ff::Track> FunkenflugProcessor::stopRecording()
{
    drainLive();
    recording = false;
    juce::OwnedArray<juce::MidiMessageSequence> tracks;
    {
        std::lock_guard<std::mutex> l (liveMx);
        double first = -1.0;
        for (auto* e : recSeq) if (e->message.isNoteOn()) { first = e->message.getTimeStamp(); break; }
        if (first < 0.0) return {};
        auto* s = new juce::MidiMessageSequence (recSeq);
        if (first > 0.5) s->addTimeToMessages (0.5 - first);
        tracks.add (s);
        recSeq.clear();
    }
    return ff::trackFromSequences (tracks, "Take " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S"));
}

//==============================================================================
juce::String FunkenflugProcessor::loadHostedPlugin (const juce::String& fileOrId, const juce::MemoryBlock* stateToRestore)
{
    juce::VST3PluginFormat fmt;
    juce::OwnedArray<juce::PluginDescription> descs;
    fmt.findAllTypesForFile (descs, fileOrId);
    if (descs.isEmpty()) return "No VST3 plugin was found in:\n" + fileOrId;
    auto* d = descs[0];
    for (auto* x : descs) if (x->isInstrument) { d = x; break; }

    juce::String err;
    auto inst = formats.createPluginInstance (*d, sr, blockSize, err);
    if (inst == nullptr) return err.isNotEmpty() ? err : juce::String ("The plugin could not be loaded.");
    inst->setRateAndBufferSizeDetails (sr, blockSize);
    inst->prepareToPlay (sr, blockSize);
    if (stateToRestore != nullptr && stateToRestore->getSize() > 0)
        inst->setStateInformation (stateToRestore->getData(), (int) stateToRestore->getSize());
    {
        const juce::ScopedLock l (hostedLock);
        std::swap (hosted, inst);
        hostedId = fileOrId;
        hostBuf.setSize (juce::jmax (2, hosted->getTotalNumInputChannels(), hosted->getTotalNumOutputChannels()), juce::jmax (blockSize * 2, 8192));
    }
    if (inst != nullptr) inst->releaseResources();
    return {};
}

void FunkenflugProcessor::unloadHostedPlugin()
{
    std::unique_ptr<juce::AudioPluginInstance> old;
    {
        const juce::ScopedLock l (hostedLock);
        std::swap (hosted, old);
        hostedId.clear();
    }
    if (old != nullptr) old->releaseResources();
}

bool FunkenflugProcessor::renderAudioOffline (const ff::Track& tr, const juce::File& wav, double leadIn, double tail,
                                              std::atomic<float>& progress, std::atomic<bool>& cancel)
{
    const double rate = sr > 8000.0 ? sr : 48000.0;
    const int bs = 512;
    wav.deleteFile();
    auto stream = std::make_unique<juce::FileOutputStream> (wav);
    if (! stream->openedOk()) return false;
    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::AudioFormatWriter> writer (fmt.createWriterFor (stream.get(), rate, 2, 24, {}, 0));
    if (writer == nullptr) return false;
    stream.release();

    const juce::ScopedLock l (hostedLock);     // audio thread skips the hosted plugin while we hold this
    juce::Synthesiser offline;
    auto* inst = hosted.get();
    if (inst != nullptr)
    {
        inst->releaseResources();
        inst->setNonRealtime (true);
        inst->setRateAndBufferSizeDetails (rate, bs);
        inst->prepareToPlay (rate, bs);
    }
    auto sfOffline = inst == nullptr ? soundFont.cloneForOffline (rate) : nullptr;
    ff::SfzPlayer sfzOffline;
    if (inst == nullptr && sfOffline == nullptr && sfz.isLoaded()) { sfzOffline.setInstrument (sfz.getInstrument()); sfzOffline.setSampleRate (rate); }
    if (inst == nullptr && sfOffline == nullptr && ! sfzOffline.isLoaded()) setupSynth (offline, rate);

    const int chans = inst != nullptr ? juce::jmax (2, inst->getTotalNumInputChannels(), inst->getTotalNumOutputChannels()) : 2;
    juce::AudioBuffer<float> buf (chans, bs), out (2, bs);
    juce::MidiBuffer midi;
    const juce::int64 total = (juce::int64) ((leadIn + tr.length + tail) * rate);
    size_t idx = 0;
    bool ok = true;
    for (juce::int64 pos = 0; pos < total && ok; pos += bs)
    {
        if (cancel.load()) { ok = false; break; }
        const double t0 = (double) pos / rate - leadIn, t1 = (double) (pos + bs) / rate - leadIn;
        midi.clear();
        while (idx < tr.evs.size() && tr.evs[idx].t < t1)
        {
            auto& e = tr.evs[idx++];
            midi.addEvent (e.d, e.n, juce::jlimit (0, bs - 1, (int) ((e.t - t0) * rate)));
        }
        buf.clear();
        if (inst != nullptr) inst->processBlock (buf, midi);
        else if (sfOffline != nullptr) sfOffline->render (buf, midi, bs);
        else if (sfzOffline.isLoaded()) sfzOffline.render (buf, midi, bs);
        else offline.renderNextBlock (buf, midi, 0, bs);
        const int outs = inst != nullptr ? juce::jmax (1, inst->getTotalNumOutputChannels()) : 2;
        for (int c = 0; c < 2; ++c) out.copyFrom (c, 0, buf, juce::jmin (c, outs - 1), 0, bs);
        finishOutput (out, bs, volGain.load());
        ok = writer->writeFromAudioSampleBuffer (out, 0, bs);
        progress = (float) pos / (float) total;
    }
    writer.reset();
    if (inst != nullptr)
    {
        inst->releaseResources();
        inst->setNonRealtime (false);
        inst->setRateAndBufferSizeDetails (sr, blockSize);
        inst->prepareToPlay (sr, blockSize);
    }
    return ok;
}

int FunkenflugProcessor::defaultSfzLayerStep() const
{
    const int saved = (int) state.getProperty ("sfzLayers", 0);
    if (saved == 1 || saved == 2 || saved == 4) return saved;
    return juce::SystemStats::getMemorySizeInMegabytes() >= 12000 ? 1 : 2;
}

juce::String FunkenflugProcessor::sampleInstrumentName() const
{
    if (soundFont.isLoaded()) return soundFont.getFile().getFileName();
    if (auto i = sfz.getInstrument()) return i->name + ".sfz  (" + juce::String ((int) (i->bytes / (1024 * 1024))) + " MB in memory)";
    return {};
}

juce::String FunkenflugProcessor::loadSoundFontNow (const juce::File& f, int sfzLayerStep)
{
    loadProgress = 0.0f;
    if (f.hasFileExtension ("sfz"))
    {
        juce::String err;
        auto inst = ff::SfzInstrument::load (f, sfzLayerStep, loadProgress, err);
        if (inst == nullptr) return err.isNotEmpty() ? err : juce::String ("The .sfz instrument could not be loaded.");
        sfz.setInstrument (inst);
        soundFont.unload();
        return {};
    }
    if (! soundFont.load (f, sr)) return "That SoundFont could not be loaded (.sf2 files up to 2 GB and .sfz instruments are supported).";
    sfz.setInstrument (nullptr);
    return {};
}

void FunkenflugProcessor::loadSoundFontAsync (const juce::File& f, std::function<void (juce::String)> onDone)
{
    if (loading.exchange (true)) return;
    const int step = defaultSfzLayerStep();
    auto flag = alive;
    loader.addJob ([this, flag, f, step, onDone]
    {
        auto err = loadSoundFontNow (f, step);
        loading = false;
        juce::MessageManager::callAsync ([this, flag, f, err, onDone]
        {
            if (! flag->load()) return;
            if (err.isEmpty()) state.setProperty ("sfPath", f.getFullPathName(), nullptr);
            if (onDone) onDone (err);
        });
    });
}

void FunkenflugProcessor::unloadSoundFont()
{
    soundFont.unload();
    sfz.setInstrument (nullptr);
    state.setProperty ("sfPath", "", nullptr);
}

//==============================================================================
void FunkenflugProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto copy = state.createCopy();
    copy.removeAllChildren (nullptr);
    if (hosted != nullptr && hostedId.isNotEmpty())
    {
        juce::ValueTree h ("HOSTED");
        juce::MemoryBlock mb;
        hosted->getStateInformation (mb);
        h.setProperty ("id", hostedId, nullptr);
        h.setProperty ("state", mb.toBase64Encoding(), nullptr);
        copy.addChild (h, -1, nullptr);
    }
    if (auto xml = copy.createXml()) copyXmlToBinary (*xml, dest);
}

void FunkenflugProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto t = juce::ValueTree::fromXml (*xml);
        if (! t.hasType (state.getType())) return;
        for (int i = 0; i < t.getNumProperties(); ++i)
            state.setProperty (t.getPropertyName (i), t.getProperty (t.getPropertyName (i)), nullptr);
        juce::File sfFile (state.getProperty ("sfPath").toString());
        if (sfFile.existsAsFile() && ! hasSampleInstrument() && ! loading.load())
        {
            const int pr = (int) state.getProperty ("sfPreset", 0);
            loadSoundFontAsync (sfFile, [this, pr] (juce::String err) { if (err.isEmpty() && soundFont.isLoaded()) { soundFont.setPreset (pr); state.setProperty ("sfPreset", pr, nullptr); } });
        }
        auto h = t.getChildWithName ("HOSTED");
        if (h.isValid() && wrapperType == wrapperType_Standalone)
        {
            juce::MemoryBlock mb;
            mb.fromBase64Encoding (h["state"].toString());
            loadHostedPlugin (h["id"].toString(), &mb);
        }
    }
}

juce::AudioProcessorEditor* FunkenflugProcessor::createEditor() { return new FunkenflugEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FunkenflugProcessor(); }
