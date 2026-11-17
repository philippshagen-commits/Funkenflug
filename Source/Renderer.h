#pragma once
#include <juce_opengl/juce_opengl.h>
#include "Settings.h"
#include "Model.h"

namespace ff
{
struct FrameInput
{
    const std::vector<VNote>* notes = nullptr;
    const std::vector<std::pair<int, float>>* heldExtra = nullptr;   // extra pressed keys (live input during playback)
    double t = 0.0, dt = 1.0 / 60.0, wallTime = 0.0;
    bool falling = false;
    float pedal = 0.0f;
    int W = 0, H = 0;
    unsigned targetFbo = 0;
};

struct Fbo
{
    unsigned fbo = 0, tex = 0;
    int w = 0, h = 0;
    void ensure (int w, int h, bool hdr);
    void release();
};

class Renderer
{
public:
    void init();
    void shutdown();
    void setBackground (const juce::Image&);
    void resetParticles();
    void render (const Snapshot&, const FrameInput&);
    void blit (unsigned tex, int texW, int texH, int W, int H, unsigned targetFbo);

private:
    struct Particle { float x, y, vx, vy, age, life, size, r, g, b, phase; int style; };
    struct Mote { float x, y, vx, vy, size, phase, tw; };
    struct KeyLayout { int lo = 21, hi = 108; float whiteW = 0, blackW = 0; float x[128], w[128]; bool black[128]; };

    void buildLayout (KeyLayout&, int range, float W) const;
    void quad (float cx, float cy, float hx, float hy, float pad, const float* c, const float* c2,
               float radius, float mode, float glow, float glowSize, float gTop, float gBot, float shine);
    void fullscreen();

    std::unique_ptr<juce::OpenGLShaderProgram> sprite, bg, bright, blur, comp, copy;
    unsigned vao = 0, vbo = 0, fsVao = 0, bgTex = 0;
    float bgAspect = 1.0f;
    Fbo scene, bl[4], tmp[4];
    std::vector<float> verts;
    std::vector<Particle> parts;
    std::vector<Mote> motes;
    int moteType = -1;
    juce::Random rng { 1234 };
    double prevT = -1.0e9;
    float pedalVis = 0.0f;
    float emitAcc[128] = {};
};
}
