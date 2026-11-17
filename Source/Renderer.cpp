#include "Renderer.h"
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace juce::gl;

namespace ff
{
static const char* kFsVert = R"(#version 150 core
out vec2 uv;
void main(){ vec2 p=vec2(float((gl_VertexID<<1)&2), float(gl_VertexID&2)); uv=p; gl_Position=vec4(p*2.0-1.0,0.0,1.0); }
)";

static const char* kSpriteVert = R"(#version 150 core
in vec2 aPos; in vec2 aUV; in vec2 aHalf; in vec4 aCol; in vec4 aCol2; in vec4 aPar; in vec2 aExt;
uniform vec2 uSize;
out vec2 vUV; out vec2 vHalf; out vec4 vCol; out vec4 vCol2; out vec4 vPar; out vec2 vExt;
void main(){ vUV=aUV; vHalf=aHalf; vCol=aCol; vCol2=aCol2; vPar=aPar; vExt=aExt;
  gl_Position=vec4(aPos.x/uSize.x*2.0-1.0, 1.0-aPos.y/uSize.y*2.0, 0.0, 1.0); }
)";

static const char* kSpriteFrag = R"(#version 150 core
in vec2 vUV; in vec2 vHalf; in vec4 vCol; in vec4 vCol2; in vec4 vPar; in vec2 vExt;
uniform float uTime; uniform float uInvS;
out vec4 o;
float sdBox(vec2 p, vec2 h, float r){ vec2 q=abs(p)-h+r; return length(max(q,0.0))+min(max(q.x,q.y),0.0)-r; }
void main(){
  int mode=int(vPar.y+0.5);
  vec2 p=vUV/max(vHalf,vec2(0.001));
  if(mode==0){
    float d=sdBox(vUV,vHalf,vPar.x);
    float fill=clamp(0.5-d,0.0,1.0);
    vec3 c=mix(vCol.rgb,vCol2.rgb,clamp(vExt.x,0.0,1.0)*vCol2.a);
    float rim=smoothstep(-4.0,0.0,d)*0.35;
    float cyl=exp(-pow((p.x+0.35)*2.6,2.0));
    float glint=pow(0.5+0.5*sin((gl_FragCoord.y*0.011+gl_FragCoord.x*0.004)*uInvS-uTime*1.7),28.0);
    vec3 body=c*(0.82+rim)+(c*0.55+vec3(0.45))*vExt.y*(cyl*0.5+glint*0.8);
    vec3 glow=c*vPar.z*exp(-max(d,0.0)/max(vPar.w,1.0))*0.55;
    o=vec4((body*fill+glow*(1.0-fill))*vCol.a, fill*vCol.a);
  } else if(mode==1){
    float a=max(exp(-dot(p,p)*4.5)-0.011,0.0);
    o=vec4(vCol.rgb*a*vCol.a,0.0);
  } else if(mode==2){
    float ax=max(1.0-abs(p.x),0.0); float ay=clamp(p.y*0.5+0.5,0.0,1.0);
    o=vec4(vCol.rgb*ax*ax*ay*ay*ay*vCol.a,0.0);
  } else if(mode==3){
    float d=sdBox(vUV,vHalf,vPar.x);
    float fill=clamp(0.5-d,0.0,1.0);
    float shade=mix(0.72,1.0,smoothstep(-1.0,-0.55,p.y))*mix(1.0,0.88,smoothstep(0.8,1.0,p.y));
    float rim=smoothstep(-max(vPar.w,0.5),0.0,d)*vCol2.a;
    vec3 c=mix(vCol.rgb*mix(1.0,shade,vPar.z),vCol2.rgb,rim);
    float a=fill*mix(vCol.a,1.0,rim);
    o=vec4(c*a,a);
  } else if(mode==4){
    float r2=dot(p,p);
    float core=exp(-r2*14.0);
    float fl=exp(-abs(p.x)*22.0)*exp(-abs(p.y)*3.5)+exp(-abs(p.y)*22.0)*exp(-abs(p.x)*3.5);
    o=vec4(vCol.rgb*(core+fl*0.8)*max(1.0-r2,0.0)*vCol.a,0.0);
  } else if(mode==5){
    int shp=int(vPar.z+0.5); vec2 q=abs(p); float r=length(p);
    if(shp==2) r=q.x+q.y; else if(shp==3) r=max(q.x,q.y); else if(shp==4) r=max(q.x*0.866+q.y*0.5,q.y);
    float a=exp(-pow((r-0.82)/max(vPar.x,0.01),2.0));
    o=vec4(vCol.rgb*a*vCol.a,0.0);
  } else if(mode==6){
    float r=length(p);
    float a=smoothstep(1.0,0.86,r)*(0.3+0.7*smoothstep(0.5,0.95,r));
    o=vec4(vCol.rgb*a*vCol.a,0.0);
  } else {
    float a=max(exp(-dot(p,p)*3.0)-0.05,0.0);
    o=vec4(vCol.rgb*a*vCol.a, a*vCol.a*0.3);
  }
}
)";

static const char* kBgFrag = R"(#version 150 core
in vec2 uv; out vec4 o;
uniform vec3 uTop; uniform vec3 uBottom; uniform vec3 uAtmo;
uniform float uDim; uniform float uHasTex; uniform float uTexAspect; uniform float uAspect;
uniform float uFog; uniform float uRays; uniform float uTime; uniform float uHit;
uniform sampler2D uTex;
float hash(vec2 p){ p=fract(p*vec2(123.34,456.21)); p+=dot(p,p+45.32); return fract(p.x*p.y); }
float noise(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.0-2.0*f);
  return mix(mix(hash(i),hash(i+vec2(1,0)),f.x), mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x), f.y); }
float fbm(vec2 p){ float a=0.5, s=0.0; for(int i=0;i<5;i++){ s+=a*noise(p); p=p*2.03+vec2(17.0,9.0); a*=0.5; } return s; }
void main(){
  vec3 c=mix(uBottom,uTop,smoothstep(0.0,1.0,uv.y));
  if(uHasTex>0.5){
    vec2 q=uv-0.5;
    if(uAspect>uTexAspect) q.y*=uTexAspect/uAspect; else q.x*=uAspect/uTexAspect;
    q+=0.5;
    c=texture(uTex,vec2(q.x,1.0-q.y)).rgb*(1.0-uDim);
  }
  if(uFog>0.001){
    float n=fbm(vec2(uv.x*uAspect*1.6+uTime*0.035, uv.y*2.4-uTime*0.02));
    float n2=fbm(vec2(uv.x*uAspect*3.1-uTime*0.02, uv.y*4.0+uTime*0.012)+n);
    float h=exp(-max(uv.y-uHit,0.0)*2.4);
    c+=uAtmo*uFog*0.34*n*n2*2.0*(0.2+h);
  }
  if(uRays>0.001){
    vec2 d=vec2((uv.x-0.5)*uAspect, uv.y)-vec2(0.25*sin(uTime*0.05),1.35);
    float ang=atan(d.x,-d.y);
    float rn=fbm(vec2(ang*9.0+uTime*0.03, uTime*0.06));
    float ray=pow(rn,3.0)*smoothstep(1.7,0.25,length(d))*smoothstep(0.9,0.0,abs(ang));
    c+=uAtmo*uRays*1.6*ray;
  }
  o=vec4(c,1.0);
}
)";

static const char* kBrightFrag = R"(#version 150 core
in vec2 uv; out vec4 o; uniform sampler2D uTex; uniform float uTh; uniform vec2 uTexel;
void main(){
  vec3 c=(texture(uTex,uv+uTexel*vec2(-0.5,-0.5)).rgb+texture(uTex,uv+uTexel*vec2(0.5,-0.5)).rgb
         +texture(uTex,uv+uTexel*vec2(-0.5,0.5)).rgb+texture(uTex,uv+uTexel*vec2(0.5,0.5)).rgb)*0.25;
  float l=max(c.r,max(c.g,c.b));
  o=vec4(c*smoothstep(uTh,uTh+0.6,l),1.0);
}
)";

static const char* kBlurFrag = R"(#version 150 core
in vec2 uv; out vec4 o; uniform sampler2D uTex; uniform vec2 uDir;
void main(){
  vec3 c=texture(uTex,uv).rgb*0.227027;
  c+=(texture(uTex,uv+uDir*1.3846).rgb+texture(uTex,uv-uDir*1.3846).rgb)*0.316216;
  c+=(texture(uTex,uv+uDir*3.2308).rgb+texture(uTex,uv-uDir*3.2308).rgb)*0.070270;
  o=vec4(c,1.0);
}
)";

static const char* kCompFrag = R"(#version 150 core
in vec2 uv; out vec4 o;
uniform sampler2D uScene; uniform sampler2D uB0; uniform sampler2D uB1; uniform sampler2D uB2; uniform sampler2D uB3;
uniform float uBloom; uniform float uVig;
float hash(vec2 p){ p=fract(p*vec2(123.34,456.21)); p+=dot(p,p+45.32); return fract(p.x*p.y); }
void main(){
  vec3 c=texture(uScene,uv).rgb;
  vec3 b=texture(uB0,uv).rgb*0.35+texture(uB1,uv).rgb*0.35+texture(uB2,uv).rgb*0.4+texture(uB3,uv).rgb*0.45;
  c+=b*uBloom;
  float v=smoothstep(0.45,1.25,length((uv-0.5)*vec2(1.25,1.0))*1.6);
  c*=1.0-uVig*0.8*v;
  c=mix(c, 1.0-exp(-c), smoothstep(0.7,1.6,max(c.r,max(c.g,c.b)))*0.6);
  c+=(hash(gl_FragCoord.xy)-0.5)/255.0;
  o=vec4(clamp(c,0.0,1.0),1.0);
}
)";

static const char* kCopyFrag = R"(#version 150 core
in vec2 uv; out vec4 o; uniform sampler2D uTex;
void main(){ o=vec4(texture(uTex,uv).rgb,1.0); }
)";

//==============================================================================
void Fbo::ensure (int nw, int nh, bool hdr)
{
    nw = juce::jmax (1, nw); nh = juce::jmax (1, nh);
    if (fbo != 0 && nw == w && nh == h) return;
    release();
    w = nw; h = nh;
    glGenTextures (1, &tex);
    glBindTexture (GL_TEXTURE_2D, tex);
    glTexImage2D (GL_TEXTURE_2D, 0, hdr ? GL_RGBA16F : GL_RGBA8, w, h, 0, GL_RGBA, hdr ? GL_FLOAT : GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers (1, &fbo);
    glBindFramebuffer (GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
}

void Fbo::release()
{
    if (fbo != 0) glDeleteFramebuffers (1, &fbo);
    if (tex != 0) glDeleteTextures (1, &tex);
    fbo = tex = 0; w = h = 0;
}

static std::unique_ptr<juce::OpenGLShaderProgram> makeProgram (const char* vs, const char* fs, bool spriteAttribs)
{
    auto& ctx = *juce::OpenGLContext::getCurrentContext();
    auto p = std::make_unique<juce::OpenGLShaderProgram> (ctx);
    bool ok = p->addVertexShader (vs) && p->addFragmentShader (fs);
    if (ok && spriteAttribs)
    {
        const char* names[] = { "aPos", "aUV", "aHalf", "aCol", "aCol2", "aPar", "aExt" };
        for (unsigned i = 0; i < 7; ++i) glBindAttribLocation (p->getProgramID(), i, names[i]);
    }
    ok = ok && p->link();
    if (! ok) { juce::Logger::writeToLog ("Funkenflug shader error: " + p->getLastError()); DBG (p->getLastError()); }
    return p;
}

void Renderer::init()
{
    sprite = makeProgram (kSpriteVert, kSpriteFrag, true);
    bg     = makeProgram (kFsVert, kBgFrag, false);
    bright = makeProgram (kFsVert, kBrightFrag, false);
    blur   = makeProgram (kFsVert, kBlurFrag, false);
    comp   = makeProgram (kFsVert, kCompFrag, false);
    copy   = makeProgram (kFsVert, kCopyFrag, false);

    glGenVertexArrays (1, &vao);
    glGenVertexArrays (1, &fsVao);
    glGenBuffers (1, &vbo);
    glBindVertexArray (vao);
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    const int sizes[] = { 2, 2, 2, 4, 4, 4, 2 };
    const GLsizei stride = 20 * sizeof (float);
    size_t off = 0;
    for (unsigned i = 0; i < 7; ++i)
    {
        glEnableVertexAttribArray (i);
        glVertexAttribPointer (i, sizes[i], GL_FLOAT, GL_FALSE, stride, (const void*) (off * sizeof (float)));
        off += (size_t) sizes[i];
    }
    glBindVertexArray (0);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}

void Renderer::shutdown()
{
    sprite.reset(); bg.reset(); bright.reset(); blur.reset(); comp.reset(); copy.reset();
    scene.release();
    for (auto& f : bl) f.release();
    for (auto& f : tmp) f.release();
    if (vbo != 0) glDeleteBuffers (1, &vbo);
    if (vao != 0) glDeleteVertexArrays (1, &vao);
    if (fsVao != 0) glDeleteVertexArrays (1, &fsVao);
    if (bgTex != 0) glDeleteTextures (1, &bgTex);
    vbo = vao = fsVao = bgTex = 0;
}

void Renderer::setBackground (const juce::Image& imgIn)
{
    if (bgTex != 0) { glDeleteTextures (1, &bgTex); bgTex = 0; }
    if (! imgIn.isValid()) return;
    auto img = imgIn;
    const int maxDim = 4096;
    if (img.getWidth() > maxDim || img.getHeight() > maxDim)
    {
        const double sc = (double) maxDim / juce::jmax (img.getWidth(), img.getHeight());
        img = img.rescaled ((int) (img.getWidth() * sc), (int) (img.getHeight() * sc), juce::Graphics::highResamplingQuality);
    }
    img = img.convertedToFormat (juce::Image::ARGB);
    const int w = img.getWidth(), h = img.getHeight();
    std::vector<juce::uint8> px ((size_t) w * (size_t) h * 4);
    {
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                auto c = bd.getPixelColour (x, y);
                auto* d = &px[((size_t) y * (size_t) w + (size_t) x) * 4];
                d[0] = c.getRed(); d[1] = c.getGreen(); d[2] = c.getBlue(); d[3] = 255;
            }
    }
    glGenTextures (1, &bgTex);
    glBindTexture (GL_TEXTURE_2D, bgTex);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    bgAspect = (float) w / (float) h;
}

void Renderer::resetParticles()
{
    parts.clear();
    motes.clear();
    moteType = -1;
    rng.setSeed (1234);
    prevT = -1.0e9;
    pedalVis = 0.0f;
    for (auto& e : emitAcc) e = 0.0f;
}

static bool isBlackKey (int k) { const int p = k % 12; return p == 1 || p == 3 || p == 6 || p == 8 || p == 10; }

void Renderer::buildLayout (KeyLayout& kl, int range, float W) const
{
    static const int los[] = { 21, 28, 36, 36, 0 }, his[] = { 108, 103, 96, 84, 127 };
    range = juce::jlimit (0, 4, range);
    kl.lo = los[range]; kl.hi = his[range];
    int nWhite = 0;
    for (int k = kl.lo; k <= kl.hi; ++k) if (! isBlackKey (k)) ++nWhite;
    kl.whiteW = W / (float) nWhite;
    kl.blackW = kl.whiteW * 0.58f;
    int wi = 0;
    for (int k = 0; k < 128; ++k) { kl.x[k] = -1000.0f; kl.w[k] = 0.0f; kl.black[k] = isBlackKey (k); }
    for (int k = kl.lo; k <= kl.hi; ++k)
    {
        if (! kl.black[k]) { kl.x[k] = (float) wi * kl.whiteW; kl.w[k] = kl.whiteW; ++wi; }
        else
        {
            static const float offs[12] = { 0, -0.12f, 0, 0.12f, 0, 0, -0.16f, 0, 0, 0, 0.16f, 0 };
            kl.x[k] = (float) wi * kl.whiteW - kl.blackW * 0.5f + offs[k % 12] * kl.blackW;
            kl.w[k] = kl.blackW;
        }
    }
}

void Renderer::quad (float cx, float cy, float hx, float hy, float pad, const float* c, const float* c2,
                     float radius, float mode, float glow, float glowSize, float gTop, float gBot, float shine)
{
    static const float sx[6] = { -1, 1, 1, -1, 1, -1 }, sy[6] = { -1, -1, 1, -1, 1, 1 };
    const float ex = hx + pad, ey = hy + pad;
    for (int i = 0; i < 6; ++i)
    {
        const float g = sy[i] < 0 ? gTop : gBot;
        const float v[20] = { cx + sx[i] * ex, cy + sy[i] * ey, sx[i] * ex, sy[i] * ey, hx, hy,
                              c[0], c[1], c[2], c[3], c2[0], c2[1], c2[2], c2[3],
                              radius, mode, glow, glowSize, g, shine };
        verts.insert (verts.end(), v, v + 20);
    }
}

void Renderer::fullscreen()
{
    glBindVertexArray (fsVao);
    glDrawArrays (GL_TRIANGLES, 0, 3);
}

void Renderer::blit (unsigned tex, int, int, int W, int H, unsigned targetFbo)
{
    glBindFramebuffer (GL_FRAMEBUFFER, targetFbo);
    glViewport (0, 0, W, H);
    glDisable (GL_BLEND);
    glDisable (GL_DEPTH_TEST);
    copy->use();
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, tex);
    copy->setUniform ("uTex", 0);
    fullscreen();
    glBindVertexArray (0);
}

static void rgb (juce::Colour c, float* o, float mul = 1.0f, float a = 1.0f)
{
    o[0] = c.getFloatRed() * mul; o[1] = c.getFloatGreen() * mul; o[2] = c.getFloatBlue() * mul; o[3] = a;
}
static void mix3 (const float* a, const float* b, float t, float* o)
{
    for (int i = 0; i < 3; ++i) o[i] = a[i] + (b[i] - a[i]) * t;
}

//==============================================================================
void Renderer::render (const Snapshot& S, const FrameInput& F)
{
    if (sprite == nullptr || F.W <= 0 || F.H <= 0) return;
    const float W = (float) F.W, H = (float) F.H, s = H / 1080.0f;
    const float dt = (float) juce::jlimit (0.0, 0.1, F.dt);
    const double t = F.t;
    static const std::vector<VNote> noNotes;
    const auto& notes = F.notes != nullptr ? *F.notes : noNotes;

    KeyLayout kl;
    buildLayout (kl, S.c[keyRange], W);
    const float kbH = S.f[keyboardHeight] < 0.02f ? 0.0f : (S.c[keyboardStyle] == 6 ? 6.0f * s : S.f[keyboardHeight] * H);
    const float hitY = H - kbH;
    const float pps = hitY / S.f[visibleSeconds];
    const float vis = S.f[visibleSeconds];

    float cA[4], cB[4], cGrad[4], cSpark[4], cAtmo[4];
    const float white[4] = { 1, 1, 1, 1 };
    rgb (S.col[colA], cA); rgb (S.col[colB], cB); rgb (S.col[colGrad], cGrad); rgb (S.col[colSpark], cSpark); rgb (S.col[colAtmo], cAtmo);

    auto noteColour = [&] (const VNote& n, float* o)
    {
        switch (S.c[colourMode])
        {
            case 1:  memcpy (o, n.key >= (int) S.f[splitKey] ? cA : cB, 12); break;
            case 2:
            {
                if (n.ch <= 0) memcpy (o, cA, 12);
                else if (n.ch == 1) memcpy (o, cB, 12);
                else rgb (S.col[colA].withRotatedHue (0.17f * (float) n.ch), o);
                break;
            }
            case 3:  rgb (juce::Colour::fromHSV ((float) (n.key % 12) / 12.0f, juce::jmax (0.55f, S.col[colA].getSaturation()), 1.0f, 1.0f), o); break;
            case 4:  mix3 (cB, cA, n.vel, o); break;
            case 5:  mix3 (cB, cA, juce::jlimit (0.0f, 1.0f, (float) (n.key - kl.lo) / (float) juce::jmax (1, kl.hi - kl.lo)), o); break;
            default: memcpy (o, cA, 12); break;
        }
        o[3] = 1.0f;
    };
    auto brightness = [&] (float vel) { return (1.0f - S.f[dynBrightness]) + S.f[dynBrightness] * (0.35f + 0.95f * vel); };

    verts.clear();
    const bool timeJump = ! (t >= prevT && t - prevT < 0.3);

    //--- pedal glow
    pedalVis += (F.pedal - pedalVis) * juce::jmin (1.0f, dt * 9.0f);
    if (pedalVis > 0.01f && S.f[pedalGlow] > 0.0f)
    {
        float c[4]; mix3 (cA, cAtmo, 0.5f, c); c[3] = 0.20f * S.f[pedalGlow] * pedalVis;
        const float h = hitY * 0.30f;
        quad (W * 0.5f, hitY - h * 0.5f, W * 1.3f, h * 0.5f, 0, c, c, 0, 2, 0, 0, 0, 0, 0);
    }

    //--- ambient motes
    {
        static const int base[] = { 0, 110, 150, 34, 90, 170 };
        const int type = juce::jlimit (0, 5, S.c[ambientType]);
        const int want = (int) ((float) base[type] * S.f[ambientAmount]);
        if (type != moteType) { motes.clear(); moteType = type; }
        while ((int) motes.size() > want) motes.pop_back();
        while ((int) motes.size() < want)
        {
            Mote m;
            m.x = rng.nextFloat(); m.y = rng.nextFloat();
            m.phase = rng.nextFloat() * 6.283f; m.tw = 0.4f + rng.nextFloat() * 1.6f;
            m.size = 0.4f + rng.nextFloat() * 0.6f;
            m.vx = (rng.nextFloat() - 0.5f); m.vy = (rng.nextFloat() - 0.5f);
            motes.push_back (m);
        }
        const float wt = (float) F.wallTime;
        for (auto& m : motes)
        {
            float size = 3.0f, inten = 0.5f, mode = 1.0f;
            const float tw = 0.5f + 0.5f * std::sin (wt * m.tw + m.phase);
            switch (type)
            {
                case 1: m.x += m.vx * 0.012f * dt; m.y += (m.vy * 0.010f - 0.004f) * dt; size = 1.4f + 2.6f * m.size; inten = 0.25f + 0.45f * tw; break;
                case 2: m.x += (0.02f * std::sin (wt * 0.6f + m.phase) + 0.006f) * dt; m.y += (0.035f + 0.06f * m.size) * dt; size = 1.8f + 3.2f * m.size; inten = 0.55f; break;
                case 3: m.x += 0.03f * std::sin (wt * 0.31f * m.tw + m.phase) * dt; m.y += 0.025f * std::cos (wt * 0.23f * m.tw + m.phase * 1.7f) * dt;
                        size = 3.0f + 3.0f * m.size; inten = 2.2f * std::pow (tw, 3.0f); break;
                case 4: m.x += 0.02f * std::sin (wt * 0.9f * m.tw + m.phase) * dt; m.y -= (0.03f + 0.08f * m.size) * dt;
                        size = 1.3f + 2.2f * m.size; inten = 1.2f * (0.5f + 0.5f * std::sin (wt * 7.0f * m.tw + m.phase)) * (0.3f + 0.7f * m.y); break;
                case 5: size = 3.0f + 7.0f * m.size * m.size; inten = 0.25f + 0.9f * tw * tw; mode = 4.0f; break;
                default: break;
            }
            if (m.x < -0.02f) m.x += 1.04f; if (m.x > 1.02f) m.x -= 1.04f;
            if (m.y < -0.02f) m.y += 1.04f; if (m.y > 1.02f) m.y -= 1.04f;
            float c[4] = { cAtmo[0], cAtmo[1], cAtmo[2], inten };
            const float r = size * s * (mode == 1.0f ? 2.2f : 1.0f);
            quad (m.x * W, m.y * hitY, r, r, 0, c, c, 0, mode, 0, 0, 0, 0, 0);
        }
    }

    //--- notes
    struct Lit { float c[3]; float amt; float vel; };
    Lit lit[128];
    for (auto& l : lit) l.amt = 0.0f;
    const size_t fxStart = verts.size();
    std::vector<float> fx;   // additive effects collected separately so they draw after all notes
    auto pushFx = [&] (float cx, float cy, float hx, float hy, const float* c, float mode, float radius, float extra = 0.0f)
    {
        std::swap (verts, fx); quad (cx, cy, hx, hy, 0, c, c, radius, mode, extra, 0, 0, 0, 0); std::swap (verts, fx);
    };
    juce::ignoreUnused (fxStart);

    for (int pass = 0; pass < 2; ++pass)
    {
        for (auto& n : notes)
        {
            if (n.key < kl.lo || n.key > kl.hi || kl.black[n.key] != (pass == 1)) continue;
            if (F.falling ? (n.on > t + vis + 0.1 || n.sus < t - 1.0) : (n.on > t || juce::jmin (n.sus, t) < t - vis - 0.5)) continue;

            float col[4]; noteColour (n, col);
            if (kl.black[n.key]) for (int i = 0; i < 3; ++i) col[i] *= 1.0f - S.f[blackDarken];
            const float br = brightness (n.vel);
            const float xc = kl.x[n.key] + kl.w[n.key] * 0.5f;
            const float hx = kl.w[n.key] * 0.5f * S.f[noteWidth];
            auto yOf = [&] (double tau) { return F.falling ? hitY - (float) ((tau - t) * pps) : hitY - (float) ((t - tau) * pps); };
            const double offT = juce::jmin (n.off, F.falling ? n.off : t);
            const double susT = juce::jmin (n.sus, F.falling ? n.sus : t);
            const double bodyLen = juce::jmax (0.001, offT - n.on);

            auto drawSeg = [&] (double a, double b, float alpha, float widthMul, bool body)
            {
                float y0 = yOf (a), y1 = yOf (b);
                float top = juce::jmin (y0, y1), bot = juce::jmax (y0, y1);
                if (body && bot - top < 4.0f * s) top = bot - 4.0f * s;
                const float uTop = top, uBot = bot;
                top = juce::jmax (top, -40.0f * s); bot = juce::jmin (bot, hitY);
                if (bot - top < 0.5f) return;
                auto gAt = [&] (float y)
                {
                    const double tau = F.falling ? t + (hitY - y) / pps : t - (hitY - y) / pps;
                    return (float) juce::jlimit (0.0, 1.0, (tau - n.on) / bodyLen);
                };
                juce::ignoreUnused (uTop, uBot);
                float c[4] = { col[0] * br, col[1] * br, col[2] * br, alpha };
                float c2[4] = { cGrad[0] * br, cGrad[1] * br, cGrad[2] * br, body ? S.f[noteGradient] : 0.0f };
                const float h2 = (bot - top) * 0.5f, w2 = hx * widthMul;
                const float rad = S.f[noteRadius] * juce::jmin (w2, h2, 12.0f * s);
                const float pad = body ? 26.0f * s : 0.0f;
                quad (xc, top + h2, w2, h2, pad, c, c2, rad, 0, body ? S.f[noteGlow] : 0.0f, 7.0f * s,
                      gAt (top), gAt (bot), body ? S.f[shine] : 0.0f);
            };
            if (S.f[sustainGhost] > 0.01f && susT > offT + 0.02)
                drawSeg (offT, susT, 0.55f * S.f[sustainGhost] * S.f[noteOpacity], 0.45f, false);
            drawSeg (n.on, offT, S.f[noteOpacity], 1.0f, true);

            // effects at the hit line
            const double age = t - n.on;
            if (age < 0.0) continue;
            float ec[4]; mix3 (col, cSpark, S.f[sparkColourMix], ec);
            const bool down = t < n.off, sounding = t < n.sus;
            const float decay = juce::jmax (0.3f, std::exp (-(float) age / 1.8f));
            if (sounding)
            {
                auto& l = lit[n.key];
                const float amt = down ? 1.0f : 0.3f;
                if (amt >= l.amt) { l.amt = amt; l.vel = n.vel; memcpy (l.c, col, 12); }
            }
            if (down && S.f[keyLight] > 0.0f)
            {
                float c[4] = { col[0], col[1], col[2], 0.55f * S.f[keyLight] * br * decay };
                pushFx (xc, hitY, hx * 3.4f + 8.0f * s, hx * 3.4f + 8.0f * s, c, 1, 0);
                c[3] = 0.32f * S.f[keyLight] * br * decay;
                const float bh = hitY * 0.2f * (0.4f + n.vel);
                pushFx (xc, hitY - bh * 0.5f, hx * 1.7f, bh * 0.5f, c, 2, 0);
            }
            if (age < 0.7 && S.f[impactFlash] > 0.0f)
            {
                float c[4]; mix3 (ec, white, 0.45f, c);
                c[3] = 1.3f * S.f[impactFlash] * n.vel * std::exp (-(float) age / 0.13f);
                const float r = (50.0f + 120.0f * n.vel) * s;
                pushFx (xc, hitY, r, r * 0.8f, c, 1, 0);
            }
            if (age < 0.9 && S.f[ripple] > 0.0f)
            {
                const float k = (float) age / 0.9f;
                float c[4] = { ec[0], ec[1], ec[2], 0.9f * S.f[ripple] * (0.25f + n.vel) * (1.0f - k) * (1.0f - k) };
                const float r = (16.0f + 170.0f * std::sqrt (k) * (0.5f + n.vel)) * s * S.f[rippleSize];
                const int shape = S.c[rippleShape];
                pushFx (xc, hitY, r, shape == 0 ? r * 0.42f : r, c, 5, 0.07f * S.f[rippleWidth] / std::sqrt (S.f[rippleSize]), (float) shape);
            }

            // particles
            const int style = S.c[particleStyle];
            if (style < 4 && S.f[sparkAmount] > 0.0f && ! timeJump && parts.size() < 9000)
            {
                static const float rateMul[] = { 1.0f, 0.4f, 0.32f, 0.4f };
                const float dyn = (1.0f - S.f[dynSparks]) + S.f[dynSparks] * n.vel * 1.3f;
                float count = 0.0f;
                if (down) count += S.f[sparkAmount] * rateMul[style] * 55.0f * dyn * decay * dt;
                const bool onset = n.on > prevT && n.on <= t;
                if (onset) count += S.f[sparkAmount] * rateMul[style] * (6.0f + 42.0f * n.vel) * dyn;
                emitAcc[n.key] += count;
                int num = (int) emitAcc[n.key];
                emitAcc[n.key] -= (float) num;
                for (int i = 0; i < num; ++i)
                {
                    Particle p;
                    p.style = style;
                    p.x = xc + (rng.nextFloat() * 2.0f - 1.0f) * hx;
                    p.y = hitY - rng.nextFloat() * 3.0f * s;
                    const float sp = S.f[sparkSpeed] * s * (onset ? 1.6f : 1.0f);
                    const float u = rng.nextFloat();
                    switch (style)
                    {
                        case 0:  p.size = (1.5f + 2.8f * u * u) * s; p.vy = -(90.0f + 330.0f * rng.nextFloat()) * sp; p.vx = (rng.nextFloat() - 0.5f) * 150.0f * sp; break;
                        case 1:  p.size = (5.0f + 9.0f * u) * s;     p.vy = -(30.0f + 130.0f * rng.nextFloat()) * sp; p.vx = (rng.nextFloat() - 0.5f) * 110.0f * sp; break;
                        case 2:  p.size = (6.0f + 16.0f * u * u) * s; p.vy = -(20.0f + 90.0f * rng.nextFloat()) * sp;  p.vx = (rng.nextFloat() - 0.5f) * 70.0f * sp; break;
                        default: p.size = (16.0f + 22.0f * u) * s;   p.vy = -(25.0f + 70.0f * rng.nextFloat()) * sp;  p.vx = (rng.nextFloat() - 0.5f) * 50.0f * sp; break;
                    }
                    p.size *= S.f[sparkSize];
                    p.vy *= S.f[sparkRise];
                    p.age = 0.0f;
                    p.life = S.f[sparkLife] * (0.35f + 0.9f * rng.nextFloat());
                    p.phase = rng.nextFloat() * 6.283f;
                    const float wmix = style == 0 ? 0.25f + 0.35f * rng.nextFloat() : 0.15f;
                    float c[3]; mix3 (ec, white, wmix, c);
                    p.r = c[0]; p.g = c[1]; p.b = c[2];
                    parts.push_back (p);
                }
            }
        }
    }

    // live keys held during playback
    if (F.heldExtra != nullptr)
        for (auto& h : *F.heldExtra)
            if (h.first >= kl.lo && h.first <= kl.hi)
            {
                VNote n { h.first, h.second, 0, t, kInf, kInf };
                float col[4]; noteColour (n, col);
                lit[h.first].amt = 1.0f; lit[h.first].vel = h.second; memcpy (lit[h.first].c, col, 12);
                const float xc = kl.x[h.first] + kl.w[h.first] * 0.5f, hx = kl.w[h.first] * 0.5f;
                float c[4] = { col[0], col[1], col[2], 0.55f * S.f[keyLight] * brightness (h.second) };
                pushFx (xc, hitY, hx * 3.4f + 8.0f * s, hx * 3.4f + 8.0f * s, c, 1, 0);
            }

    verts.insert (verts.end(), fx.begin(), fx.end());

    //--- keyboard
    if (kbH > 0.0f)
    {
        struct KbStyle { float white[4], black[4], back[4]; float outline, outlineW, shade; int outlineSrc, feltSrc; };   // src: 0 = theme colour, 1 = fixed
        static const KbStyle styles[] = {
            { { 0.80f, 0.79f, 0.77f, 1 }, { 0.075f, 0.075f, 0.085f, 1 }, { 0.02f, 0.02f, 0.025f, 1 }, 0.0f, 1.0f, 1.0f, 0, 0 },   // classic
            { { 0.74f, 0.66f, 0.50f, 1 }, { 0.10f, 0.065f, 0.045f, 1 }, { 0.09f, 0.045f, 0.02f, 1 }, 0.0f, 1.0f, 1.0f, 0, 1 },    // vintage ivory
            { { 0.10f, 0.08f, 0.07f, 1 }, { 0.72f, 0.66f, 0.54f, 1 }, { 0.12f, 0.06f, 0.03f, 1 }, 0.0f, 1.0f, 1.0f, 0, 1 },       // harpsichord
            { { 0.03f, 0.03f, 0.045f, 1 }, { 0.0f, 0.0f, 0.0f, 1 }, { 0.0f, 0.0f, 0.0f, 1 }, 1.0f, 2.4f, 0.3f, 0, 0 },            // neon
            { { 1.0f, 1.0f, 1.0f, 0.07f }, { 1.0f, 1.0f, 1.0f, 0.14f }, { 0.0f, 0.0f, 0.0f, 0 }, 1.0f, 2.0f, 0.0f, 0, 0 },      // made of light
            { { 0.75f, 0.85f, 0.95f, 0.30f }, { 0.02f, 0.03f, 0.05f, 0.7f }, { 0.0f, 0.0f, 0.0f, 0.3f }, 0.75f, 1.8f, 0.6f, 1, 0 }, // glass
        };
        const int ksi = juce::jlimit (0, 6, S.c[keyboardStyle]);
        if (ksi < 6)
        {
            const auto& ks = styles[ksi];
            if (ks.back[3] > 0.0f)
                quad (W * 0.5f, hitY + kbH * 0.5f, W * 0.5f, kbH * 0.5f, 0, ks.back, ks.back, 0, 3, 0, 0, 0, 0, 0);
            const float gap = juce::jmax (1.0f, 1.2f * s), top = hitY + 3.0f * s;
            for (int pass = 0; pass < 2; ++pass)
                for (int k = kl.lo; k <= kl.hi; ++k)
                {
                    if (kl.black[k] != (pass == 1)) continue;
                    const float* base = pass == 0 ? ks.white : ks.black;
                    float c[4], oc[4];
                    float litc[3];
                    const float gain = 0.75f + 0.55f * S.f[keyLight] * brightness (lit[k].vel);
                    for (int i = 0; i < 3; ++i) litc[i] = lit[k].amt > 0.0f ? lit[k].c[i] * gain + 0.1f : 0.0f;
                    float b3[3] = { base[0], base[1], base[2] };
                    if (ksi == 1 || ksi == 2)   // slight unevenness of old keys
                    {
                        const float v = 0.94f + 0.08f * (float) ((k * 7919) % 13) / 12.0f;
                        for (auto& x : b3) x *= v;
                    }
                    mix3 (b3, litc, 0.92f * lit[k].amt, c);
                    c[3] = base[3] + (1.0f - base[3]) * juce::jmin (1.0f, lit[k].amt * 1.2f);
                    if (ks.outlineSrc == 0) { const float* src = k >= (int) S.f[splitKey] && S.c[colourMode] == 1 ? cA : (S.c[colourMode] == 1 ? cB : cA); for (int i = 0; i < 3; ++i) oc[i] = src[i] * 1.5f; }
                    else { oc[0] = oc[1] = oc[2] = 1.3f; }
                    oc[3] = ks.outline;
                    const float h = (pass == 0 ? kbH : kbH * 0.63f) - 3.0f * s;
                    const float hw = kl.w[k] * 0.5f - (pass == 0 ? gap * 0.5f : 0.0f);
                    quad (kl.x[k] + kl.w[k] * 0.5f, top + h * 0.5f - 4.0f * s, hw, h * 0.5f + 4.0f * s, 0, c, oc,
                          juce::jmin (hw, 3.5f * s), 3, ks.shade, juce::jmax (1.2f, ks.outlineW * s), 0, 0, 0);
                }
        }
        float fc[4]; mix3 (cA, cB, 0.5f, fc); fc[3] = 1.0f;
        if (ksi < 6 && styles[ksi].feltSrc == 1) { fc[0] = 0.75f; fc[1] = 0.08f; fc[2] = 0.10f; }
        for (int i = 0; i < 3; ++i) fc[i] *= 0.55f + 0.5f * pedalVis;
        quad (W * 0.5f, hitY + 1.0f * s, W * 0.5f, 1.8f * s, 14.0f * s, fc, fc, 0, 0, 0.6f + pedalVis, 5.0f * s, 0, 0, 0);
    }

    //--- particles: simulate + draw
    {
        const float rise = S.f[sparkRise], turb = S.f[sparkTurb];
        for (auto& p : parts)
        {
            p.age += dt;
            p.vx += turb * 240.0f * s * std::sin (p.phase + p.age * 5.3f + p.y * 0.012f) * dt;
            p.vy += -38.0f * s * rise * dt;
            const float drag = std::exp (-dt * (p.style == 0 ? 1.1f : 0.7f));
            p.vx *= drag; p.vy *= drag;
            p.x += p.vx * dt; p.y += p.vy * dt;
        }
        parts.erase (std::remove_if (parts.begin(), parts.end(), [&] (const Particle& p) { return p.age >= p.life || p.y < -60.0f || p.y > H + 60.0f; }), parts.end());
        for (auto& p : parts)
        {
            const float k = p.age / p.life;
            const float fade = std::pow (1.0f - k, 1.4f) * juce::jmin (1.0f, p.age * 30.0f + 0.3f);
            float c[4] = { p.r, p.g, p.b, 1.0f };
            float size = p.size, mode = 1.0f;
            switch (p.style)
            {
                case 0:  c[3] = 2.4f * fade * (0.75f + 0.25f * std::sin (p.phase + p.age * 31.0f)); size *= 2.4f * (1.0f - 0.4f * k); mode = 1; break;
                case 1:  c[3] = 1.8f * fade * (0.55f + 0.45f * std::sin (p.phase + p.age * 13.0f)); mode = 4; break;
                case 2:  c[3] = 0.5f * fade; size *= 1.0f + 0.5f * k; mode = 6; break;
                default: c[3] = 0.085f * fade; size *= 1.0f + 2.2f * k; mode = 7; break;
            }
            quad (p.x, p.y, size, size, 0, c, c, 0, mode, 0, 0, 0, 0, 0);
        }
    }
    prevT = t;

    //--- GL
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_SCISSOR_TEST);
    glDisable (GL_CULL_FACE);
    scene.ensure (F.W, F.H, true);
    glBindFramebuffer (GL_FRAMEBUFFER, scene.fbo);
    glViewport (0, 0, F.W, F.H);
    glDisable (GL_BLEND);

    bg->use();
    bg->setUniform ("uTop", S.col[bgTop].getFloatRed(), S.col[bgTop].getFloatGreen(), S.col[bgTop].getFloatBlue());
    bg->setUniform ("uBottom", S.col[bgBottom].getFloatRed(), S.col[bgBottom].getFloatGreen(), S.col[bgBottom].getFloatBlue());
    bg->setUniform ("uAtmo", cAtmo[0], cAtmo[1], cAtmo[2]);
    bg->setUniform ("uDim", S.f[bgDim]);
    bg->setUniform ("uHasTex", bgTex != 0 ? 1.0f : 0.0f);
    bg->setUniform ("uTexAspect", bgAspect);
    bg->setUniform ("uAspect", W / H);
    bg->setUniform ("uFog", S.f[fog]);
    bg->setUniform ("uRays", S.f[rays]);
    bg->setUniform ("uTime", (float) F.wallTime);
    bg->setUniform ("uHit", kbH / H);
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, bgTex);
    bg->setUniform ("uTex", 0);
    fullscreen();

    if (! verts.empty())
    {
        glEnable (GL_BLEND);
        glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        sprite->use();
        sprite->setUniform ("uSize", W, H);
        sprite->setUniform ("uTime", (float) F.wallTime);
        sprite->setUniform ("uInvS", 1.0f / s);
        glBindVertexArray (vao);
        glBindBuffer (GL_ARRAY_BUFFER, vbo);
        glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (verts.size() * sizeof (float)), verts.data(), GL_STREAM_DRAW);
        glDrawArrays (GL_TRIANGLES, 0, (GLsizei) (verts.size() / 20));
        glBindBuffer (GL_ARRAY_BUFFER, 0);
        glDisable (GL_BLEND);
    }

    // bloom chain
    int w = F.W, h = F.H;
    unsigned src = scene.tex;
    for (int i = 0; i < 4; ++i)
    {
        const int pw = w, ph = h;
        w = juce::jmax (1, w / 2); h = juce::jmax (1, h / 2);
        bl[i].ensure (w, h, true); tmp[i].ensure (w, h, true);
        glBindFramebuffer (GL_FRAMEBUFFER, bl[i].fbo);
        glViewport (0, 0, w, h);
        bright->use();
        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, src);
        bright->setUniform ("uTex", 0);
        bright->setUniform ("uTh", i == 0 ? S.f[bloomThreshold] : -10.0f);
        bright->setUniform ("uTexel", 1.0f / (float) pw, 1.0f / (float) ph);
        fullscreen();
        blur->use();
        blur->setUniform ("uTex", 0);
        for (int it = 0; it < 2; ++it)
        {
            glBindFramebuffer (GL_FRAMEBUFFER, tmp[i].fbo);
            glBindTexture (GL_TEXTURE_2D, bl[i].tex);
            blur->setUniform ("uDir", S.f[bloomRadius] / (float) w, 0.0f);
            fullscreen();
            glBindFramebuffer (GL_FRAMEBUFFER, bl[i].fbo);
            glBindTexture (GL_TEXTURE_2D, tmp[i].tex);
            blur->setUniform ("uDir", 0.0f, S.f[bloomRadius] / (float) h);
            fullscreen();
        }
        src = bl[i].tex;
    }

    glBindFramebuffer (GL_FRAMEBUFFER, F.targetFbo);
    glViewport (0, 0, F.W, F.H);
    comp->use();
    const char* names[] = { "uScene", "uB0", "uB1", "uB2", "uB3" };
    const unsigned texs[] = { scene.tex, bl[0].tex, bl[1].tex, bl[2].tex, bl[3].tex };
    for (int i = 0; i < 5; ++i)
    {
        glActiveTexture ((GLenum) (GL_TEXTURE0 + i));
        glBindTexture (GL_TEXTURE_2D, texs[i]);
        comp->setUniform (names[i], i);
    }
    comp->setUniform ("uBloom", S.f[bloom]);
    comp->setUniform ("uVig", S.f[vignette]);
    fullscreen();
    glActiveTexture (GL_TEXTURE0);
    glBindVertexArray (0);
}
}
