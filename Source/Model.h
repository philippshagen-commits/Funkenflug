#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstring>

namespace ff
{
constexpr double kInf = 1.0e18;

struct VNote { int key; float vel; int ch; double on, off, sus; };   // off = key released, sus = sound ends (pedal)
struct PedalPt { double t; float v; };

/** Turns a stream of MIDI events into drawable notes, including sustain-pedal extension. */
class TrackBuilder
{
public:
    std::vector<VNote> notes;
    std::vector<PedalPt> pedal;
    float pedalNow = 0.0f;

    void clear() { notes.clear(); pedal.clear(); pedalNow = 0.0f; }

    void add (const juce::MidiMessage& m, double t, int channelOverride = -1)
    {
        if (m.isNoteOn())
        {
            const int k = m.getNoteNumber();
            for (auto it = notes.rbegin(); it != notes.rend(); ++it)
                if (it->key == k)
                {
                    if (it->off >= kInf) it->off = t;
                    if (it->sus >= kInf) it->sus = t;
                }
            notes.push_back ({ k, m.getFloatVelocity(), channelOverride >= 0 ? channelOverride : m.getChannel() - 1, t, kInf, kInf });
        }
        else if (m.isNoteOff())
        {
            const int k = m.getNoteNumber();
            for (auto it = notes.rbegin(); it != notes.rend(); ++it)
                if (it->key == k && it->off >= kInf)
                {
                    it->off = t;
                    if (pedalNow < 0.5f) it->sus = t;
                    break;
                }
        }
        else if (m.isController() && m.getControllerNumber() == 64)
        {
            const float v = (float) m.getControllerValue() / 127.0f;
            if (pedalNow >= 0.5f && v < 0.5f)
                for (auto& n : notes)
                    if (n.off < kInf && n.sus >= kInf) n.sus = t;
            pedalNow = v;
            pedal.push_back ({ t, v });
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            finish (t);
        }
    }

    void finish (double t)
    {
        for (auto& n : notes)
        {
            if (n.off >= kInf) n.off = t;
            if (n.sus >= kInf) n.sus = juce::jmax (t, n.off);
        }
    }

    void prune (double before)
    {
        notes.erase (std::remove_if (notes.begin(), notes.end(), [before] (const VNote& n) { return n.sus < before; }), notes.end());
        if (pedal.size() > 2048) pedal.erase (pedal.begin(), pedal.begin() + 1024);
    }
};

inline float pedalAt (const std::vector<PedalPt>& p, double t)
{
    auto it = std::upper_bound (p.begin(), p.end(), t, [] (double tt, const PedalPt& a) { return tt < a.t; });
    return it == p.begin() ? 0.0f : (it - 1)->v;
}

/** An immutable loaded MIDI file or recorded take. */
struct Track
{
    struct Ev { double t; juce::uint8 d[3]; int n; };
    std::vector<VNote> notes;
    std::vector<PedalPt> pedal;
    std::vector<Ev> evs;                 // for audio playback, sorted by time
    juce::MidiMessageSequence seq;       // seconds, for saving
    double length = 0.0;
    juce::String name;
};

inline std::shared_ptr<Track> trackFromSequences (const juce::OwnedArray<juce::MidiMessageSequence>& tracks, const juce::String& name)
{
    auto tr = std::make_shared<Track>();
    tr->name = name;
    int noteTracks = 0;
    for (auto* s : tracks)
        for (auto* e : *s) if (e->message.isNoteOn()) { ++noteTracks; break; }

    struct Tagged { juce::MidiMessage m; int tag; };
    std::vector<Tagged> all;
    int tag = 0;
    for (auto* s : tracks)
    {
        bool hasNotes = false;
        for (auto* e : *s)
        {
            if (e->message.isMetaEvent() || e->message.isSysEx()) continue;
            hasNotes = hasNotes || e->message.isNoteOn();
            all.push_back ({ e->message, noteTracks > 1 ? tag : -1 });
        }
        if (hasNotes) ++tag;
    }
    std::stable_sort (all.begin(), all.end(), [] (const Tagged& a, const Tagged& b) { return a.m.getTimeStamp() < b.m.getTimeStamp(); });

    TrackBuilder b;
    for (auto& a : all)
    {
        const double t = a.m.getTimeStamp();
        b.add (a.m, t, a.tag);
        tr->seq.addEvent (a.m);
        if (a.m.getRawDataSize() <= 3)
        {
            Track::Ev ev { t, { 0, 0, 0 }, a.m.getRawDataSize() };
            memcpy (ev.d, a.m.getRawData(), (size_t) ev.n);
            tr->evs.push_back (ev);
        }
        tr->length = juce::jmax (tr->length, t);
    }
    b.finish (tr->length);
    tr->seq.updateMatchedPairs();
    tr->notes = std::move (b.notes);
    tr->pedal = std::move (b.pedal);
    return tr;
}

inline std::shared_ptr<Track> loadMidiFile (const juce::File& f)
{
    juce::FileInputStream in (f);
    juce::MidiFile mf;
    if (! in.openedOk() || ! mf.readFrom (in)) return {};
    mf.convertTimestampTicksToSeconds();
    juce::OwnedArray<juce::MidiMessageSequence> tracks;
    for (int i = 0; i < mf.getNumTracks(); ++i) tracks.add (new juce::MidiMessageSequence (*mf.getTrack (i)));
    auto tr = trackFromSequences (tracks, f.getFileNameWithoutExtension());
    return tr->notes.empty() ? nullptr : tr;
}

inline bool saveMidiFile (const Track& tr, const juce::File& f)
{
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (960);
    juce::MidiMessageSequence s;
    s.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0);   // 120 bpm -> 1920 ticks per second
    for (auto* e : tr.seq) { auto m = e->message; m.setTimeStamp (m.getTimeStamp() * 1920.0); s.addEvent (m); }
    mf.addTrack (s);
    f.deleteFile();
    juce::FileOutputStream out (f);
    return out.openedOk() && mf.writeTo (out);
}
}
