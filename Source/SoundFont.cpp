#include "SoundFont.h"

JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wall", "-Wextra", "-Wconversion", "-Wsign-conversion", "-Wshadow", "-Wcast-align", "-Wunused-function", "-Wimplicit-fallthrough", "-Wfloat-equal", "-Wdouble-promotion", "-Wswitch-enum", "-Wzero-as-null-pointer-constant", "-Wold-style-cast", "-Wcast-qual")
JUCE_BEGIN_IGNORE_WARNINGS_MSVC (4244 4267 4305 4456 4457 4996 4100 4701 4702 4706)
#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
#include "tsf.h"
JUCE_END_IGNORE_WARNINGS_MSVC
JUCE_END_IGNORE_WARNINGS_GCC_LIKE

namespace ff
{
static void configure (tsf* f, double sr, int preset)
{
    tsf_set_output (f, TSF_STEREO_INTERLEAVED, (int) sr, -5.0f);
    tsf_set_max_voices (f, 256);
    for (int ch = 0; ch < 16; ++ch)
        tsf_channel_set_presetindex (f, ch, juce::jlimit (0, juce::jmax (0, tsf_get_presetcount (f) - 1), preset));
}

SoundFontPlayer::~SoundFontPlayer() { unload(); }

bool SoundFontPlayer::load (const juce::File& f, double sampleRate)
{
    if (! f.existsAsFile() || f.getSize() > 0x7fff0000) return false;
    tsf* fresh = nullptr;
    {
        juce::MemoryMappedFile map (f, juce::MemoryMappedFile::readOnly);
        if (map.getData() == nullptr) return false;
        fresh = tsf_load_memory (map.getData(), (int) map.getSize());
    }
    if (fresh == nullptr || tsf_get_presetcount (fresh) <= 0) { if (fresh != nullptr) tsf_close (fresh); return false; }
    rate = sampleRate > 8000.0 ? sampleRate : 44100.0;
    preset = 0;
    configure (fresh, rate, preset);
    tsf* old = nullptr;
    {
        const juce::SpinLock::ScopedLockType l (lock);
        old = sf; sf = fresh;
    }
    if (old != nullptr) tsf_close (old);
    file = f;
    return true;
}

void SoundFontPlayer::unload()
{
    tsf* old = nullptr;
    {
        const juce::SpinLock::ScopedLockType l (lock);
        old = sf; sf = nullptr;
    }
    if (old != nullptr) tsf_close (old);
    file = juce::File();
}

void SoundFontPlayer::setSampleRate (double sr)
{
    const juce::SpinLock::ScopedLockType l (lock);
    rate = sr > 8000.0 ? sr : 44100.0;
    if (sf != nullptr) tsf_set_output (sf, TSF_STEREO_INTERLEAVED, (int) rate, -5.0f);
}

juce::StringArray SoundFontPlayer::presetNames() const
{
    juce::StringArray names;
    if (sf != nullptr)
        for (int i = 0; i < tsf_get_presetcount (sf); ++i)
            names.add (juce::String (i + 1) + "  " + juce::String (juce::CharPointer_UTF8 (tsf_get_presetname (sf, i))).trim());
    return names;
}

void SoundFontPlayer::setPreset (int index)
{
    const juce::SpinLock::ScopedLockType l (lock);
    if (sf == nullptr) return;
    preset = juce::jlimit (0, tsf_get_presetcount (sf) - 1, index);
    tsf_note_off_all (sf);
    for (int ch = 0; ch < 16; ++ch) tsf_channel_set_presetindex (sf, ch, preset);
}

void SoundFontPlayer::apply (const juce::MidiMessage& m)
{
    const int ch = juce::jlimit (0, 15, m.getChannel() - 1);
    if (m.isNoteOn())            tsf_channel_note_on (sf, ch, m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())      tsf_channel_note_off (sf, ch, m.getNoteNumber());
    else if (m.isController())   tsf_channel_midi_control (sf, ch, m.getControllerNumber(), m.getControllerValue());
    else if (m.isPitchWheel())   tsf_channel_set_pitchwheel (sf, ch, m.getPitchWheelValue());
}

void SoundFontPlayer::allOff()
{
    const juce::SpinLock::ScopedLockType l (lock);
    if (sf != nullptr) { for (int ch = 0; ch < 16; ++ch) tsf_channel_set_sustain (sf, ch, 0); tsf_note_off_all (sf); }
}

bool SoundFontPlayer::render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, int n)
{
    const juce::SpinLock::ScopedTryLockType l (lock);
    if (! l.isLocked() || sf == nullptr) return false;
    if ((int) inter.size() < n * 2) inter.resize ((size_t) n * 2);
    int done = 0;
    for (const auto meta : midi)
    {
        const int pos = juce::jlimit (done, n, meta.samplePosition);
        if (pos > done) { tsf_render_float (sf, inter.data() + done * 2, pos - done, 0); done = pos; }
        apply (meta.getMessage());
    }
    if (n > done) tsf_render_float (sf, inter.data() + done * 2, n - done, 0);
    for (int c = 0; c < out.getNumChannels(); ++c)
    {
        auto* d = out.getWritePointer (c);
        const int src = juce::jmin (c, 1);
        for (int i = 0; i < n; ++i) d[i] += inter[(size_t) (i * 2 + src)];
    }
    return true;
}

std::unique_ptr<SoundFontPlayer> SoundFontPlayer::cloneForOffline (double sampleRate) const
{
    if (sf == nullptr) return {};
    auto p = std::make_unique<SoundFontPlayer>();
    p->sf = tsf_copy (sf);
    if (p->sf == nullptr) return {};
    p->rate = sampleRate; p->preset = preset; p->file = file;
    p->inter.resize (16384);
    configure (p->sf, sampleRate, preset);
    return p;
}
}
