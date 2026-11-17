#include "Sfz.h"
#include <thread>
#include <atomic>
#include <algorithm>
#include <cmath>

namespace ff
{
namespace
{
int noteNumber (const juce::String& sIn)
{
    auto s = sIn.trim();
    if (s.isEmpty()) return -1;
    if (s.containsOnly ("-0123456789")) return s.getIntValue();
    static const int base[] = { 9, 11, 0, 2, 4, 5, 7 };   // a b c d e f g
    const auto c = juce::CharacterFunctions::toLowerCase (s[0]);
    if (c < 'a' || c > 'g') return -1;
    int n = base[c - 'a'], i = 1;
    if (s[i] == '#') { ++n; ++i; } else if (s[i] == 'b') { --n; ++i; }
    return n + (s.substring (i).getIntValue() + 1) * 12;
}

struct Preprocessor
{
    juce::File root;
    std::map<juce::String, juce::String> defines;
    juce::String out;
    int depth = 0;

    juce::String subst (juce::String s) const
    {
        if (! s.containsChar ('$')) return s;
        std::vector<std::pair<juce::String, juce::String>> d (defines.begin(), defines.end());
        std::sort (d.begin(), d.end(), [] (auto& a, auto& b) { return a.first.length() > b.first.length(); });
        for (auto& kv : d) s = s.replace (kv.first, kv.second);
        return s;
    }

    void run (const juce::File& f)
    {
        if (++depth > 24 || ! f.existsAsFile()) { --depth; return; }
        juce::StringArray lines;
        lines.addLines (f.loadFileAsString());
        for (auto line : lines)
        {
            const int c = line.indexOf ("//");
            if (c >= 0) line = line.substring (0, c);
            while (line.isNotEmpty())
            {
                const int inc = line.indexOf ("#include"), def = line.indexOf ("#define");
                int next = -1;
                if (inc >= 0 && (def < 0 || inc < def)) next = inc; else if (def >= 0) next = def;
                if (next < 0) { out << subst (line) << "\n"; break; }
                out << subst (line.substring (0, next)) << " ";
                line = line.substring (next);
                if (line.startsWith ("#include"))
                {
                    const int q1 = line.indexOfChar ('"'), q2 = q1 >= 0 ? line.indexOfChar (q1 + 1, '"') : -1;
                    if (q2 < 0) break;
                    auto path = subst (line.substring (q1 + 1, q2)).replaceCharacter ('\\', '/');
                    auto target = root.getChildFile (path);
                    if (! target.existsAsFile()) target = f.getParentDirectory().getChildFile (path);
                    run (target);
                    line = line.substring (q2 + 1);
                }
                else
                {
                    auto toks = juce::StringArray::fromTokens (line.substring (7).trim(), " \t", "");
                    toks.removeEmptyStrings();
                    if (toks.size() >= 2) defines[toks[0]] = toks[1];
                    const int used = toks.size() >= 2 ? line.indexOf (line.indexOf (toks[0]) + toks[0].length(), toks[1]) + toks[1].length() : line.length();
                    line = line.substring (juce::jlimit (0, line.length(), used));
                }
            }
        }
        --depth;
    }
};

using Ops = std::map<juce::String, juce::String>;

float curve (int index, float x)
{
    switch (index)
    {
        case 1:  return 2.0f * x - 1.0f;
        case 2:  return 1.0f - x;
        case 3:  return 1.0f - 2.0f * x;
        case 4:  return x * x;
        case 5:  return 1.0f - x * x;
        case 6:  return std::sqrt (x);
        default: return x;
    }
}

/** base opcode value plus any "<name>_onccN" modulation evaluated at the instrument's default CC values. */
float withCC (const Ops& o, const juce::String& name, float def, const std::map<int, float>& cc)
{
    float v = def;
    if (auto it = o.find (name); it != o.end()) v = it->second.getFloatValue();
    const auto prefix = name + "_oncc";
    for (auto& kv : o)
        if (kv.first.startsWith (prefix))
        {
            const int n = kv.first.substring (prefix.length()).getIntValue();
            int cv = 0;
            if (auto c = o.find (name + "_curvecc" + juce::String (n)); c != o.end()) cv = c->second.getIntValue();
            float x = 0.0f;
            if (auto c = cc.find (n); c != cc.end()) x = c->second;
            v += kv.second.getFloatValue() * curve (cv, x);
        }
    return v;
}
}

//==============================================================================
std::shared_ptr<SfzInstrument> SfzInstrument::load (const juce::File& file, int layerStep, std::atomic<float>& progress, juce::String& error)
{
    Preprocessor pp;
    pp.root = file.getParentDirectory();
    pp.run (file);
    if (pp.out.trim().isEmpty()) { error = "The .sfz file is empty or could not be read."; return {}; }

    // tokenise into headers and opcodes
    Ops control, global, master, group, region;
    std::vector<Ops> regionOps;
    juce::String scope;
    auto flush = [&]
    {
        if (scope != "region") return;
        Ops all = global;
        for (auto* src : { &master, &group, &region }) for (auto& kv : *src) all[kv.first] = kv.second;
        regionOps.push_back (std::move (all));
    };
    juce::StringArray lines;
    lines.addLines (pp.out);
    for (auto& line : lines)
    {
        struct Tok { int pos; bool header; int nameEnd; };
        std::vector<Tok> toks;
        const int n = line.length();
        for (int i = 0; i < n; ++i)
        {
            if (line[i] == '<')
            {
                const int e = line.indexOfChar (i, '>');
                if (e > i) { toks.push_back ({ i, true, e }); i = e; }
            }
            else if ((i == 0 || juce::CharacterFunctions::isWhitespace (line[i - 1]) || line[i - 1] == '>')
                     && (juce::CharacterFunctions::isLetter (line[i]) || line[i] == '_'))
            {
                int j = i;
                while (j < n && (juce::CharacterFunctions::isLetterOrDigit (line[j]) || line[j] == '_')) ++j;
                if (j < n && line[j] == '=') { toks.push_back ({ i, false, j }); i = j; }
            }
        }
        for (size_t t = 0; t < toks.size(); ++t)
        {
            auto& tk = toks[t];
            if (tk.header)
            {
                flush();
                scope = line.substring (tk.pos + 1, tk.nameEnd).trim().toLowerCase();
                if (scope == "global") { global.clear(); master.clear(); group.clear(); }
                else if (scope == "master") { master.clear(); group.clear(); }
                else if (scope == "group") group.clear();
                region.clear();
            }
            else
            {
                const int end = t + 1 < toks.size() ? toks[t + 1].pos : n;
                auto name = line.substring (tk.pos, tk.nameEnd).toLowerCase();
                auto value = line.substring (tk.nameEnd + 1, end).trim();
                Ops* dst = scope == "control" ? &control : scope == "global" ? &global : scope == "master" ? &master
                         : scope == "group" ? &group : scope == "region" ? &region : nullptr;
                if (dst != nullptr) (*dst)[name] = value;
            }
        }
    }
    flush();
    if (regionOps.empty()) { error = "No regions were found in the .sfz file."; return {}; }

    std::map<int, float> cc;
    for (auto& kv : control)
    {
        if (kv.first.startsWith ("set_hdcc")) cc[kv.first.substring (8).getIntValue()] = kv.second.getFloatValue();
        else if (kv.first.startsWith ("set_cc")) cc[kv.first.substring (6).getIntValue()] = kv.second.getFloatValue() / 127.0f;
    }
    auto defaultPath = control.count ("default_path") ? control["default_path"].replaceCharacter ('\\', '/') : juce::String();

    auto inst = std::make_shared<SfzInstrument>();
    inst->name = file.getFileNameWithoutExtension();
    std::map<juce::String, int> sampleIndex;
    juce::StringArray samplePaths;
    struct Pending { SfzRegion r; };
    std::vector<SfzRegion> regs;

    for (auto& o : regionOps)
    {
        auto get = [&o] (const char* k, const juce::String& def = {}) { auto it = o.find (k); return it != o.end() ? it->second : def; };
        if (! o.count ("sample") || get ("sample").startsWith ("*")) continue;
        if (o.count ("sw_last") && o.count ("sw_default") && noteNumber (get ("sw_last")) != noteNumber (get ("sw_default"))) continue;
        SfzRegion r;
        if (o.count ("key")) { r.lokey = r.hikey = r.keycenter = noteNumber (get ("key")); }
        if (o.count ("lokey")) r.lokey = noteNumber (get ("lokey"));
        if (o.count ("hikey")) r.hikey = noteNumber (get ("hikey"));
        if (o.count ("pitch_keycenter")) r.keycenter = noteNumber (get ("pitch_keycenter"));
        r.lovel = get ("lovel", "0").getIntValue();
        r.hivel = get ("hivel", "127").getIntValue();
        r.tune = get ("tune", "0").getFloatValue() + 100.0f * get ("transpose", "0").getFloatValue();
        r.keytrack = get ("pitch_keytrack", "100").getFloatValue();
        r.veltrack = withCC (o, "amp_veltrack", 100.0f, cc);
        r.attack = juce::jmax (0.0f, withCC (o, "ampeg_attack", 0.0f, cc));
        r.release = juce::jmax (0.005f, withCC (o, "ampeg_release", 0.001f, cc));
        r.rtDecay = get ("rt_decay", "0").getFloatValue();
        r.offset = juce::jmax (0, (int) withCC (o, "offset", 0.0f, cc));
        r.lorand = get ("lorand", "0").getFloatValue();
        r.hirand = get ("hirand", "1").getFloatValue();
        r.releaseTrigger = get ("trigger", "attack").startsWith ("release");
        float amp = get ("amplitude", "100").getFloatValue() / 100.0f;
        for (auto& kv : o)      // ARIA: every amplitude_onccN multiplies
            if (kv.first.startsWith ("amplitude_oncc"))
            {
                const int n = kv.first.substring (14).getIntValue();
                const float x = cc.count (n) ? cc[n] : 0.0f;
                amp *= kv.second.getFloatValue() / 100.0f * curve (get (("amplitude_curvecc" + juce::String (n)).toRawUTF8(), "0").getIntValue(), x);
            }
        r.gain = juce::Decibels::decibelsToGain (get ("volume", "0").getFloatValue()) * juce::jlimit (0.0f, 4.0f, amp);
        r.offTime = juce::jlimit (0.003f, 2.0f, get ("off_time", "0.006").getFloatValue());
        if (o.count ("on_locc64")) { r.onLoCC = get ("on_locc64").getIntValue(); r.onHiCC = get ("on_hicc64", "127").getIntValue(); r.keyTriggered = false; }
        if (r.lokey < 0 || r.hikey < 0) r.keyTriggered = false;
        if (! r.keyTriggered && r.onLoCC < 0) continue;

        auto path = (defaultPath + get ("sample")).replaceCharacter ('\\', '/');
        auto it = sampleIndex.find (path);
        if (it == sampleIndex.end()) { it = sampleIndex.emplace (path, samplePaths.size()).first; samplePaths.add (path); }
        r.sample = it->second;
        regs.push_back (r);
    }
    if (regs.empty()) { error = "No playable regions were found in the .sfz file."; return {}; }

    // optional thinning of velocity layers to save memory
    if (layerStep > 1)
    {
        std::map<std::pair<int, int>, std::vector<size_t>> byKeys;
        for (size_t i = 0; i < regs.size(); ++i)
            if (regs[i].keyTriggered && ! regs[i].releaseTrigger && regs[i].hikey >= 0) byKeys[{ regs[i].lokey, regs[i].hikey }].push_back (i);
        std::vector<bool> drop (regs.size(), false);
        for (auto& kv : byKeys)
        {
            auto idx = kv.second;
            if (idx.size() < 4) continue;
            std::sort (idx.begin(), idx.end(), [&regs] (size_t a, size_t b) { return regs[a].lovel < regs[b].lovel; });
            int lo = regs[idx.front()].lovel;
            for (size_t k = 0; k < idx.size(); ++k)
            {
                const bool keep = ((idx.size() - 1 - k) % (size_t) layerStep) == 0;
                if (keep) { regs[idx[k]].lovel = lo; lo = regs[idx[k]].hivel + 1; }
                else drop[idx[k]] = true;
            }
        }
        std::vector<SfzRegion> kept;
        for (size_t i = 0; i < regs.size(); ++i) if (! drop[i]) kept.push_back (regs[i]);
        regs.swap (kept);
    }

    // load only the samples still referenced
    std::vector<int> remap ((size_t) samplePaths.size(), -1);
    std::vector<juce::File> files;
    for (auto& r : regs)
    {
        if (remap[(size_t) r.sample] < 0) { remap[(size_t) r.sample] = (int) files.size(); files.push_back (pp.root.getChildFile (samplePaths[r.sample])); }
        r.sample = remap[(size_t) r.sample];
    }
    inst->samples.resize (files.size());
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::atomic<int> nextFile { 0 }, doneFiles { 0 }, failed { 0 };
    auto worker = [&]
    {
        juce::AudioBuffer<float> buf (2, 32768);
        for (;;)
        {
            const int i = nextFile++;
            if (i >= (int) files.size()) break;
            std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (files[(size_t) i]));
            auto& s = inst->samples[(size_t) i];
            if (reader == nullptr || reader->lengthInSamples <= 0) { ++failed; ++doneFiles; continue; }
            s.frames = (int) juce::jmin ((juce::int64) 0x3fffffff, reader->lengthInSamples);
            s.rate = reader->sampleRate;
            s.data.resize ((size_t) s.frames * 2 + 8, 0);
            for (int pos = 0; pos < s.frames; pos += buf.getNumSamples())
            {
                const int n = juce::jmin (buf.getNumSamples(), s.frames - pos);
                reader->read (&buf, 0, n, pos, true, true);
                const float* l = buf.getReadPointer (0);
                const float* r = buf.getReadPointer (reader->numChannels > 1 ? 1 : 0);
                auto* d = s.data.data() + (size_t) pos * 2;
                for (int k = 0; k < n; ++k)
                {
                    d[k * 2]     = (juce::int16) juce::jlimit (-32767.0f, 32767.0f, l[k] * 32767.0f);
                    d[k * 2 + 1] = (juce::int16) juce::jlimit (-32767.0f, 32767.0f, r[k] * 32767.0f);
                }
            }
            ++doneFiles;
            progress = (float) doneFiles.load() / (float) files.size();
        }
    };
    {
        const int nThreads = juce::jlimit (1, 8, juce::SystemStats::getNumCpus() - 1);
        std::vector<std::thread> threads;
        for (int i = 0; i < nThreads; ++i) threads.emplace_back (worker);
        for (auto& t : threads) t.join();
    }
    if (failed.load() == (int) files.size()) { error = "None of the samples could be read. Keep the .sfz next to its sample folder."; return {}; }
    for (auto& s : inst->samples) inst->bytes += (juce::int64) s.data.size() * 2;
    inst->regions = std::move (regs);
    progress = 1.0f;
    return inst;
}

//==============================================================================
void SfzPlayer::setInstrument (std::shared_ptr<SfzInstrument> i)
{
    const juce::SpinLock::ScopedLockType l (lock);
    for (auto& v : voices) v.active = false;
    for (auto& d : down) d = false;
    for (auto& p : pending) p = false;
    pedal = false;
    std::swap (inst, i);
}

void SfzPlayer::start (const SfzRegion& r, int key, float vel, float extraGain, bool isNote)
{
    const auto& s = inst->samples[(size_t) r.sample];
    if (s.frames < 8) return;
    Voice* v = nullptr;
    for (auto& c : voices) if (! c.active) { v = &c; break; }
    if (v == nullptr)
    {
        v = &voices[0];
        for (auto& c : voices) if ((c.releasing && ! v->releasing) || (c.releasing == v->releasing && c.started < v->started)) v = &c;
    }
    const float vt = juce::jlimit (0.0f, 1.0f, r.veltrack / 100.0f);
    v->r = &r; v->s = &s;
    v->pos = (double) juce::jmin (r.offset, s.frames - 4);
    const double semis = (key - r.keycenter) * (double) r.keytrack / 100.0 + (double) r.tune / 100.0;
    v->step = s.rate / rate * std::pow (2.0, semis / 12.0);
    v->gain = r.gain * extraGain * ((1.0f - vt) + vt * vel * vel) * 0.8f / 32768.0f;
    v->attacking = r.attack > 0.0005f;
    v->env = v->attacking ? 0.0f : 1.0f;
    v->attInc = (float) (1.0 / (juce::jmax (0.0005f, r.attack) * rate));
    v->relCoef = (float) std::exp (-6.9 / ((double) r.release * rate));
    v->releasing = false; v->active = true; v->isNote = isNote; v->key = key; v->started = now;
}

void SfzPlayer::releaseKey (int key)
{
    for (auto& v : voices)
        if (v.active && v.isNote && v.key == key && ! v.releasing) v.releasing = true;
    const float held = (float) ((double) (now - keyOn[key]) / rate);
    const int vel = juce::jlimit (1, 127, (int) (keyVel[key] * 127.0f));
    for (auto& r : inst->regions)
        if (r.releaseTrigger && r.keyTriggered && key >= r.lokey && key <= r.hikey && vel >= r.lovel && vel <= r.hivel)
            start (r, key, keyVel[key], juce::Decibels::decibelsToGain (-r.rtDecay * held), false);
}

void SfzPlayer::handle (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        const int key = m.getNoteNumber(), vel = m.getVelocity();
        for (auto& v : voices)     // same key struck again: fade the old string quickly
            if (v.active && v.isNote && v.key == key) { v.releasing = true; v.relCoef = (float) std::exp (-6.9 / ((double) v.r->offTime * rate)); }
        down[key] = true; pending[key] = false; keyVel[key] = m.getFloatVelocity(); keyOn[key] = now;
        const float rnd = rng.nextFloat();
        for (auto& r : inst->regions)
            if (! r.releaseTrigger && r.keyTriggered && key >= r.lokey && key <= r.hikey && vel >= r.lovel && vel <= r.hivel
                && rnd >= r.lorand && (rnd < r.hirand || r.hirand >= 1.0f))
                start (r, key, m.getFloatVelocity(), 1.0f, true);
    }
    else if (m.isNoteOff())
    {
        const int key = m.getNoteNumber();
        if (! down[key]) return;
        down[key] = false;
        if (pedal) pending[key] = true; else releaseKey (key);
    }
    else if (m.isController() && m.getControllerNumber() == 64)
    {
        const int val = m.getControllerValue();
        const bool nowDown = val >= 64;
        if (nowDown != pedal)
        {
            const float rnd = rng.nextFloat();
            for (auto& r : inst->regions)
                if (! r.keyTriggered && r.onLoCC >= 0 && (nowDown ? r.onHiCC >= 64 : r.onHiCC < 64)
                    && rnd >= r.lorand && (rnd < r.hirand || r.hirand >= 1.0f))
                    start (r, r.keycenter, 1.0f, 1.0f, false);
        }
        if (pedal && ! nowDown)
            for (int k = 0; k < 128; ++k) if (pending[k]) { pending[k] = false; releaseKey (k); }
        pedal = nowDown;
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        for (int k = 0; k < 128; ++k) { down[k] = false; pending[k] = false; }
        for (auto& v : voices) if (v.active) v.releasing = true;
    }
}

void SfzPlayer::renderVoices (float* l, float* r, int n)
{
    for (auto& v : voices)
    {
        if (! v.active) continue;
        const juce::int16* d = v.s->data.data();
        const double last = (double) (v.s->frames - 3);
        double pos = v.pos;
        float env = v.env;
        int i = 0;
        for (; i < n; ++i)
        {
            if (pos >= last) { v.active = false; break; }
            const int ip = (int) pos;
            const float f = (float) (pos - ip);
            const juce::int16* p = d + (size_t) ip * 2;
            const float a = v.gain * env;
            l[i] += a * ((float) p[0] + f * (float) (p[2] - p[0]));
            r[i] += a * ((float) p[1] + f * (float) (p[3] - p[1]));
            pos += v.step;
            if (v.attacking) { env += v.attInc; if (env >= 1.0f) { env = 1.0f; v.attacking = false; } }
            if (v.releasing) { env *= v.relCoef; if (env < 0.0002f) { v.active = false; break; } }
        }
        v.pos = pos; v.env = env;
    }
    now += n;
}

bool SfzPlayer::render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, int n)
{
    const juce::SpinLock::ScopedTryLockType lk (lock);
    if (! lk.isLocked() || inst == nullptr) return false;
    if ((int) tl.size() < n) { tl.resize ((size_t) n); tr.resize ((size_t) n); }
    std::fill (tl.begin(), tl.begin() + n, 0.0f);
    std::fill (tr.begin(), tr.begin() + n, 0.0f);
    int done = 0;
    for (const auto meta : midi)
    {
        const int pos = juce::jlimit (done, n, meta.samplePosition);
        if (pos > done) { renderVoices (tl.data() + done, tr.data() + done, pos - done); done = pos; }
        handle (meta.getMessage());
    }
    if (n > done) renderVoices (tl.data() + done, tr.data() + done, n - done);
    for (int c = 0; c < out.getNumChannels(); ++c)
        out.addFrom (c, 0, c == 0 ? tl.data() : tr.data(), n);
    return true;
}
}
