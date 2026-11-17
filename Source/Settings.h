#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace ff
{
enum P { visibleSeconds, keyboardHeight, noteWidth, noteRadius, noteOpacity, noteGlow, blackDarken,
         dynBrightness, sustainGhost, noteGradient, shine, splitKey,
         sparkAmount, sparkSize, sparkSpeed, sparkLife, sparkRise, sparkTurb, dynSparks, sparkColourMix, ripple, rippleSize, rippleWidth,
         ambientAmount, fog, rays,
         bloom, bloomThreshold, bloomRadius, impactFlash, keyLight, pedalGlow, bgDim, vignette, volume, numFloats };

struct FloatDef { const char* id; const char* name; const char* group; float lo, hi, def, step; };

inline const FloatDef floatDefs[numFloats] = {
    { "visibleSeconds", "Seconds on screen",      "Layout", 0.5f, 10.0f, 3.0f, 0.1f },
    { "keyboardHeight", "Keyboard height",        "Keyboard", 0.0f, 0.3f, 0.15f, 0.01f },
    { "noteWidth",      "Note width",             "Notes",  0.3f, 1.0f, 0.84f, 0.01f },
    { "noteRadius",     "Corner roundness",       "Notes",  0.0f, 1.0f, 0.55f, 0.01f },
    { "noteOpacity",    "Opacity",                "Notes",  0.1f, 1.0f, 0.95f, 0.01f },
    { "noteGlow",       "Edge glow",              "Notes",  0.0f, 2.0f, 0.8f, 0.01f },
    { "blackDarken",    "Darken black-key notes", "Notes",  0.0f, 0.6f, 0.22f, 0.01f },
    { "dynBrightness",  "Dynamics -> brightness", "Notes",  0.0f, 1.0f, 0.65f, 0.01f },
    { "sustainGhost",   "Pedal sustain trail",    "Notes",  0.0f, 1.0f, 0.35f, 0.01f },
    { "noteGradient",   "Gradient along note",    "Notes",  0.0f, 1.0f, 0.0f, 0.01f },
    { "shine",          "Shine / gloss",          "Notes",  0.0f, 1.5f, 0.5f, 0.01f },
    { "splitKey",       "Split key (60 = C4)", "Colours", 21.0f, 108.0f, 60.0f, 1.0f },
    { "sparkAmount",    "Amount",                 "Key-press effects", 0.0f, 3.0f, 1.0f, 0.01f },
    { "sparkSize",      "Size",                   "Key-press effects", 0.3f, 3.0f, 1.0f, 0.01f },
    { "sparkSpeed",     "Speed",                  "Key-press effects", 0.0f, 3.0f, 1.0f, 0.01f },
    { "sparkLife",      "Lifetime (s)",           "Key-press effects", 0.2f, 4.0f, 1.4f, 0.01f },
    { "sparkRise",      "Rise (+) / fall (-)",    "Key-press effects", -1.0f, 2.0f, 1.0f, 0.01f },
    { "sparkTurb",      "Turbulence",             "Key-press effects", 0.0f, 2.0f, 1.0f, 0.01f },
    { "dynSparks",      "Dynamics -> sparks",     "Key-press effects", 0.0f, 1.0f, 0.75f, 0.01f },
    { "sparkColourMix", "Use effect colour", "Key-press effects", 0.0f, 1.0f, 0.0f, 0.01f },
    { "ripple",         "Ripple rings",           "Key-press effects", 0.0f, 2.0f, 0.5f, 0.01f },
    { "rippleSize",     "Ripple size",            "Key-press effects", 0.2f, 3.0f, 1.0f, 0.01f },
    { "rippleWidth",    "Ripple thickness",       "Key-press effects", 0.2f, 4.0f, 1.0f, 0.01f },
    { "ambientAmount",  "Particle amount",        "Atmosphere", 0.0f, 3.0f, 0.6f, 0.01f },
    { "fog",            "Fog",                    "Atmosphere", 0.0f, 1.5f, 0.25f, 0.01f },
    { "rays",           "Light rays",             "Atmosphere", 0.0f, 1.5f, 0.0f, 0.01f },
    { "bloom",          "Bloom",                  "Glow",   0.0f, 2.5f, 1.0f, 0.01f },
    { "bloomThreshold", "Bloom threshold",        "Glow",   0.0f, 1.5f, 0.55f, 0.01f },
    { "bloomRadius",    "Bloom spread",           "Glow",   0.3f, 2.2f, 1.0f, 0.01f },
    { "impactFlash",    "Impact flash",           "Key-press effects",   0.0f, 2.0f, 1.0f, 0.01f },
    { "keyLight",       "Key light",              "Glow",   0.0f, 2.0f, 1.0f, 0.01f },
    { "pedalGlow",      "Pedal glow",             "Glow",   0.0f, 2.0f, 1.0f, 0.01f },
    { "bgDim",          "Image dim",              "Background", 0.0f, 1.0f, 0.55f, 0.01f },
    { "vignette",       "Vignette",               "Background", 0.0f, 1.0f, 0.5f, 0.01f },
    { "volume",         "Volume (dB)",            "Sound", -24.0f, 12.0f, 0.0f, 0.5f },
};

enum C { colourMode, keyRange, direction, builtInSynth, particleStyle, ambientType, rippleShape, keyboardStyle, numChoices };
struct ChoiceDef { const char* id; const char* name; const char* group; const char* options; int def; };
inline const ChoiceDef choiceDefs[numChoices] = {
    { "colourMode",   "Colour mode", "Colours", "Single colour|Two hands (split key)|By track / channel|Rainbow (pitch)|By dynamics|Across keyboard", 1 },
    { "keyRange",     "Range",    "Keyboard",  "88 keys|76 keys|61 keys|49 keys|All 128", 0 },
    { "direction",    "Playback direction", "Layout", "Falling onto keys|Rising from keys", 0 },
    { "builtInSynth", "Built-in piano sound", "Sound", "Off|On", 1 },
    { "particleStyle", "Particle style", "Key-press effects", "Sparks|Stars|Soft orbs|Smoke|None", 0 },
    { "ambientType",  "Floating particles", "Atmosphere", "None|Dust|Snow|Fireflies|Rising embers|Starfield", 1 },
    { "rippleShape",  "Ripple shape", "Key-press effects", "Flat ring|Circle|Diamond|Square|Hexagon", 0 },
    { "keyboardStyle", "Look", "Keyboard", "Classic|Vintage ivory|Harpsichord (inverted)|Neon|Made of light|Glass|Hidden (line only)", 0 },
};

enum Col { colA, colB, colGrad, colSpark, colAtmo, bgTop, bgBottom, numColours };
struct ColourDef { const char* id; const char* name; const char* group; juce::uint32 def; };
inline const ColourDef colourDefs[numColours] = {
    { "colourA",  "Colour A (right / loud)", "Colours", 0xffff8a2a },
    { "colourB",  "Colour B (left / soft)",  "Colours", 0xffff3d5a },
    { "colourGrad",  "Gradient end colour", "Notes", 0xffffe9b0 },
    { "colourSpark", "Effect colour", "Key-press effects", 0xffffd27a },
    { "colourAtmo",  "Atmosphere colour", "Atmosphere", 0xffff9a55 },
    { "bgTop",    "Top colour",    "Background", 0xff07060a },
    { "bgBottom", "Bottom colour", "Background", 0xff1a0d08 },
};

struct Snapshot
{
    float f[numFloats];
    int c[numChoices];
    juce::Colour col[numColours];
};

inline juce::ValueTree makeDefaultState()
{
    juce::ValueTree t ("FUNKENFLUG");
    for (auto& d : floatDefs)  t.setProperty (d.id, d.def, nullptr);
    for (auto& d : choiceDefs) t.setProperty (d.id, d.def, nullptr);
    for (auto& d : colourDefs) t.setProperty (d.id, juce::Colour (d.def).toString(), nullptr);
    t.setProperty ("bgImage", "", nullptr);
    t.setProperty ("sfPath", "", nullptr);
    t.setProperty ("sfPreset", 0, nullptr);
    t.setProperty ("sfzLayers", 0, nullptr);
    t.setProperty ("theme", 0, nullptr);
    return t;
}

inline Snapshot makeSnapshot (const juce::ValueTree& t)
{
    Snapshot s;
    for (int i = 0; i < numFloats; ++i)
        s.f[i] = juce::jlimit (floatDefs[i].lo, floatDefs[i].hi, (float) t.getProperty (floatDefs[i].id, floatDefs[i].def));
    for (int i = 0; i < numChoices; ++i)  s.c[i] = (int) t.getProperty (choiceDefs[i].id, choiceDefs[i].def);
    for (int i = 0; i < numColours; ++i)
        s.col[i] = juce::Colour::fromString (t.getProperty (colourDefs[i].id, juce::Colour (colourDefs[i].def).toString()).toString());
    return s;
}

struct Theme { const char* name; juce::uint32 a, b, grad, spark, atmo, top, bottom;
               int mode, style, ambient; float amount, rise, turb, bloomAmt, life, size, gradAmt, shineAmt, fogAmt, raysAmt, rippleAmt, sparkMix; };
inline const Theme themes[] = {
    { "Embers",   0xffff8a2a, 0xffff3d5a, 0xffffe9b0, 0xffffd27a, 0xffff9a55, 0xff07060a, 0xff1a0d08, 1, 0, 4, 1.0f,  1.0f, 1.0f, 1.0f, 1.4f, 1.0f, 0.0f, 0.5f, 0.25f, 0.0f, 0.4f, 0.0f },
    { "Aurora",   0xff3dffc2, 0xff8a5cff, 0xff5cc8ff, 0xffb8fff0, 0xff46d6b0, 0xff03060d, 0xff06141c, 1, 2, 3, 0.8f,  0.6f, 1.6f, 1.1f, 2.0f, 1.2f, 0.7f, 0.6f, 0.6f, 0.5f, 0.6f, 0.0f },
    { "Frost",    0xffbfe9ff, 0xff4aa3ff, 0xffffffff, 0xffeaf6ff, 0xffcfe8ff, 0xff04070d, 0xff0a1524, 1, 1, 2, 0.7f, -0.4f, 0.6f, 0.9f, 2.2f, 0.9f, 0.5f, 1.0f, 0.35f, 0.3f, 0.8f, 0.5f },
    { "Sakura",   0xffffb7d5, 0xffff5fa2, 0xffffe3f0, 0xffffd9ea, 0xffffc4dc, 0xff0d0609, 0xff1c0d16, 5, 2, 2, 0.6f, -0.25f, 1.4f, 0.9f, 2.6f, 1.2f, 0.6f, 0.7f, 0.3f, 0.2f, 0.5f, 0.3f },
    { "Gold",     0xffffe08a, 0xffd99a2b, 0xfffff6d6, 0xfffff0c0, 0xffffd98a, 0xff050505, 0xff120e06, 4, 1, 1, 1.2f,  1.3f, 0.8f, 1.2f, 1.2f, 0.9f, 0.4f, 1.2f, 0.15f, 0.6f, 0.3f, 0.6f },
    { "Midnight", 0xff7aa2ff, 0xffc77dff, 0xffe0d4ff, 0xffffffff, 0xff8fa8ff, 0xff020308, 0xff0a0a1e, 1, 1, 5, 0.7f,  0.5f, 0.8f, 1.0f, 2.0f, 0.8f, 0.6f, 0.8f, 0.2f, 0.0f, 1.0f, 0.4f },
    { "Smoke",    0xffe8e2d8, 0xff9aa0a8, 0xff6f7680, 0xffcfd3da, 0xff8a9099, 0xff040405, 0xff101114, 4, 3, 1, 0.9f,  0.7f, 1.3f, 0.6f, 2.8f, 1.6f, 0.5f, 0.3f, 0.9f, 0.4f, 0.2f, 0.7f },
    { "Rainbow",  0xffff5a5a, 0xff5a8cff, 0xffffffff, 0xffffffff, 0xffb0b8ff, 0xff05050a, 0xff0c0c18, 3, 0, 1, 1.0f,  1.0f, 1.0f, 1.0f, 1.4f, 1.0f, 0.3f, 0.7f, 0.2f, 0.0f, 0.6f, 0.0f },
    { "Mono",     0xffffffff, 0xff9aa0a8, 0xffcfd3da, 0xffffffff, 0xffffffff, 0xff000000, 0xff0a0a0c, 4, 0, 0, 0.4f,  0.8f, 0.6f, 0.7f, 1.0f, 0.8f, 0.0f, 0.2f, 0.0f, 0.0f, 0.3f, 0.0f },
};
inline const int numThemes = (int) (sizeof (themes) / sizeof (themes[0]));

inline void applyTheme (juce::ValueTree& t, int i)
{
    if (i < 0 || i >= numThemes) return;
    auto& th = themes[i];
    auto col = [&t] (const char* id, juce::uint32 c) { t.setProperty (id, juce::Colour (c).toString(), nullptr); };
    col ("colourA", th.a); col ("colourB", th.b); col ("colourGrad", th.grad); col ("colourSpark", th.spark);
    col ("colourAtmo", th.atmo); col ("bgTop", th.top); col ("bgBottom", th.bottom);
    t.setProperty ("colourMode", th.mode, nullptr);
    t.setProperty ("particleStyle", th.style, nullptr);
    t.setProperty ("ambientType", th.ambient, nullptr);
    t.setProperty ("sparkAmount", th.amount, nullptr);
    t.setProperty ("sparkRise", th.rise, nullptr);
    t.setProperty ("sparkTurb", th.turb, nullptr);
    t.setProperty ("bloom", th.bloomAmt, nullptr);
    t.setProperty ("sparkLife", th.life, nullptr);
    t.setProperty ("sparkSize", th.size, nullptr);
    t.setProperty ("noteGradient", th.gradAmt, nullptr);
    t.setProperty ("shine", th.shineAmt, nullptr);
    t.setProperty ("fog", th.fogAmt, nullptr);
    t.setProperty ("rays", th.raysAmt, nullptr);
    t.setProperty ("ripple", th.rippleAmt, nullptr);
    t.setProperty ("sparkColourMix", th.sparkMix, nullptr);
    static const int kb[] = { 0, 5, 4, 0, 1, 3, 0, 0, 0 };
    t.setProperty ("keyboardStyle", kb[i], nullptr);
    t.setProperty ("theme", i, nullptr);
}
}
