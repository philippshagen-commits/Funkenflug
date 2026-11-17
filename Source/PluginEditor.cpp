#include "PluginEditor.h"
#include <iostream>

using namespace juce::gl;

//==============================================================================
namespace
{
const juce::Colour kPanel (0xf2101014), kAccent (0xffff8a2a), kText (0xffe9e6e1), kDim (0xff8b8a90);

struct FFLook : juce::LookAndFeel_V4
{
    FFLook()
    {
        setColourScheme ({ 0xff101014, 0xff1a1a20, 0xff101014, 0xff2c2c34, 0xffe9e6e1, 0xffff8a2a, 0xffffffff, 0xff2a2a31, 0xffe9e6e1 });
        setColour (juce::TextButton::buttonColourId, juce::Colour (0x00000000));
        setColour (juce::TextButton::buttonOnColourId, kAccent.withAlpha (0.25f));
        setColour (juce::TextButton::textColourOffId, kText);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::Slider::trackColourId, kAccent);
        setColour (juce::Slider::thumbColourId, juce::Colours::white);
        setColour (juce::Slider::backgroundColourId, juce::Colour (0x33ffffff));
        setColour (juce::PropertyComponent::backgroundColourId, juce::Colour (0x00000000));
        setColour (juce::PropertyComponent::labelTextColourId, kText);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1c1c22));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0x22ffffff));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff17171c));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent.withAlpha (0.3f));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0x00000000));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x00000000));
        setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1c1c22));
        setColour (juce::TextEditor::outlineColourId, juce::Colour (0x22ffffff));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.5f);
        auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId)
                                    : juce::Colours::white.withAlpha (down ? 0.18f : over ? 0.11f : 0.05f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 6.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::Font (juce::FontOptions (juce::jmin (14.0f, (float) h * 0.55f))); }

    void drawPropertyPanelSectionHeader (juce::Graphics& g, const juce::String& name, bool isOpen, int width, int height) override
    {
        g.setColour (kAccent);
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText ((isOpen ? juce::String::fromUTF8 ("\xe2\x80\x93  ") : "+  ") + name.toUpperCase(), 8, 0, width - 16, height, juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (8, height - 1, width - 16, 1);
    }
};

//==============================================================================
struct ColourWheel : juce::Component
{
    float h = 0.0f, s = 0.0f, v = 1.0f;
    std::function<void()> onChange;
    juce::Image img;

    void resized() override
    {
        const int d = juce::jmin (getWidth(), getHeight());
        if (d < 8) return;
        img = juce::Image (juce::Image::ARGB, d, d, true);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        const float R = (float) d * 0.5f - 1.0f, c = (float) d * 0.5f;
        for (int y = 0; y < d; ++y)
            for (int x = 0; x < d; ++x)
            {
                const float dx = (float) x + 0.5f - c, dy = (float) y + 0.5f - c, r = std::sqrt (dx * dx + dy * dy) / R;
                if (r > 1.02f) continue;
                const float hue = std::atan2 (dy, dx) / juce::MathConstants<float>::twoPi + 0.5f;
                bd.setPixelColour (x, y, juce::Colour::fromHSV (hue, juce::jmin (1.0f, r), 1.0f, juce::jlimit (0.0f, 1.0f, (1.02f - r) * R * 0.5f)));
            }
    }

    void paint (juce::Graphics& g) override
    {
        const int d = juce::jmin (getWidth(), getHeight());
        const auto area = juce::Rectangle<int> (d, d).withCentre (getLocalBounds().getCentre()).toFloat();
        g.drawImage (img, area);
        g.setColour (juce::Colours::black.withAlpha (1.0f - v));
        g.fillEllipse (area.reduced (0.5f));
        const float R = area.getWidth() * 0.5f - 1.0f, a = (h - 0.5f) * juce::MathConstants<float>::twoPi;
        const auto p = area.getCentre() + juce::Point<float> (std::cos (a), std::sin (a)) * (s * R);
        g.setColour (juce::Colours::black); g.drawEllipse (p.x - 6, p.y - 6, 12, 12, 3.0f);
        g.setColour (juce::Colours::white); g.drawEllipse (p.x - 6, p.y - 6, 12, 12, 1.5f);
    }

    void mouseDown (const juce::MouseEvent& e) override { mouseDrag (e); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int d = juce::jmin (getWidth(), getHeight());
        const auto c = getLocalBounds().getCentre().toFloat();
        const float dx = (float) e.x - c.x, dy = (float) e.y - c.y;
        h = std::atan2 (dy, dx) / juce::MathConstants<float>::twoPi + 0.5f;
        s = juce::jmin (1.0f, std::sqrt (dx * dx + dy * dy) / ((float) d * 0.5f - 1.0f));
        repaint();
        if (onChange) onChange();
    }
};

struct ColourPopup : juce::Component
{
    explicit ColourPopup (const juce::Value& target) : value (target)
    {
        auto c = juce::Colour::fromString (value.toString());
        wheel.h = c.getHue(); wheel.s = c.getSaturation(); wheel.v = c.getBrightness();
        addAndMakeVisible (wheel);
        addAndMakeVisible (bright);
        addAndMakeVisible (hex);
        addAndMakeVisible (lab);
        lab.setText ("Brightness", juce::dontSendNotification);
        lab.setFont (juce::Font (juce::FontOptions (12.0f)));
        bright.setSliderStyle (juce::Slider::LinearHorizontal);
        bright.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        bright.setRange (0.0, 1.0);
        bright.setValue (wheel.v, juce::dontSendNotification);
        bright.onValueChange = [this] { wheel.v = (float) bright.getValue(); wheel.repaint(); push(); };
        wheel.onChange = [this] { push(); };
        hex.setText (c.toDisplayString (false), false);
        hex.setJustification (juce::Justification::centred);
        hex.onReturnKey = [this]
        {
            auto t = hex.getText().trim().removeCharacters ("#");
            if (t.length() == 6)
            {
                auto col = juce::Colour::fromString ("ff" + t);
                wheel.h = col.getHue(); wheel.s = col.getSaturation(); wheel.v = col.getBrightness();
                bright.setValue (wheel.v, juce::dontSendNotification);
                wheel.repaint();
                value = col.toString();
            }
        };
        setSize (230, 300);
    }

    void push()
    {
        auto c = juce::Colour::fromHSV (wheel.h, wheel.s, wheel.v, 1.0f);
        value = c.toString();
        hex.setText (c.toDisplayString (false), false);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        hex.setBounds (r.removeFromBottom (24).reduced (40, 0));
        r.removeFromBottom (6);
        auto row = r.removeFromBottom (22);
        lab.setBounds (row.removeFromLeft (70));
        bright.setBounds (row);
        r.removeFromBottom (6);
        wheel.setBounds (r);
    }

    juce::Value value;
    ColourWheel wheel;
    juce::Slider bright;
    juce::TextEditor hex;
    juce::Label lab;
};

struct ColourProp : juce::PropertyComponent, private juce::Value::Listener
{
    struct Swatch : juce::Component
    {
        ColourProp& owner;
        explicit Swatch (ColourProp& o) : owner (o) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (2.0f, 3.0f);
            g.setColour (juce::Colour::fromString (owner.value.toString()));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.drawRoundedRectangle (r, 5.0f, 1.0f);
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            juce::CallOutBox::launchAsynchronously (std::make_unique<ColourPopup> (owner.value), getScreenBounds(), nullptr);
        }
    };

    ColourProp (const juce::Value& v, const juce::String& name) : PropertyComponent (name), value (v), swatch (*this)
    {
        addAndMakeVisible (swatch);
        value.addListener (this);
    }
    void refresh() override { swatch.repaint(); }
    void valueChanged (juce::Value&) override { swatch.repaint(); }

    juce::Value value;
    Swatch swatch;
};

struct ButtonsProp : juce::PropertyComponent
{
    struct Row : juce::Component
    {
        juce::OwnedArray<juce::TextButton> buttons;
        void resized() override
        {
            auto r = getLocalBounds();
            const int w = r.getWidth() / juce::jmax (1, buttons.size());
            for (auto* b : buttons) b->setBounds (r.removeFromLeft (w).reduced (1, 2));
        }
    };
    ButtonsProp (const juce::String& name, std::initializer_list<std::pair<juce::String, std::function<void()>>> items) : PropertyComponent (name)
    {
        for (auto& it : items)
        {
            auto* b = row.buttons.add (new juce::TextButton (it.first));
            b->onClick = it.second;
            row.addAndMakeVisible (b);
        }
        addAndMakeVisible (row);
    }
    void refresh() override {}
    Row row;
};
}

//==============================================================================
class SettingsPanel : public juce::Component
{
public:
    SettingsPanel (juce::ValueTree& st, std::function<void()> chooseBg, std::function<void()> clearBg,
                   std::function<void()> savePreset, std::function<void()> loadPreset)
        : state (st)
    {
        addAndMakeVisible (title);
        title.setText ("Settings", juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
        addAndMakeVisible (theme);
        for (int i = 0; i < ff::numThemes; ++i) theme.addItem (ff::themes[i].name, i + 1);
        theme.setTextWhenNothingSelected ("Theme...");
        theme.setSelectedId ((int) state.getProperty ("theme", 0) + 1, juce::dontSendNotification);
        theme.onChange = [this] { ff::applyTheme (state, theme.getSelectedId() - 1); };
        addAndMakeVisible (panel);

        const char* groups[] = { "Colours", "Notes", "Key-press effects", "Atmosphere", "Glow", "Keyboard", "Background", "Layout", "Sound" };
        for (auto* g : groups)
        {
            juce::Array<juce::PropertyComponent*> comps;
            const juce::String gs (g);
            for (auto& d : ff::choiceDefs)
                if (gs == d.group)
                {
                    auto opts = juce::StringArray::fromTokens (d.options, "|", "");
                    juce::Array<juce::var> vals;
                    for (int i = 0; i < opts.size(); ++i) vals.add (i);
                    comps.add (new juce::ChoicePropertyComponent (state.getPropertyAsValue (d.id, nullptr), d.name, opts, vals));
                }
            for (auto& d : ff::colourDefs)
                if (gs == d.group) comps.add (new ColourProp (state.getPropertyAsValue (d.id, nullptr), d.name));
            for (auto& d : ff::floatDefs)
                if (gs == d.group) comps.add (new juce::SliderPropertyComponent (state.getPropertyAsValue (d.id, nullptr), d.name, d.lo, d.hi, d.step));
            if (gs == "Background")
                comps.add (new ButtonsProp ("Image", { { "Choose...", chooseBg }, { "Remove", clearBg } }));
            if (gs == "Colours")
                comps.add (new ButtonsProp ("Preset file", { { "Save...", savePreset }, { "Load...", loadPreset } }));
            for (auto* c : comps) c->setPreferredHeight (26);
            panel.addSection (g, comps, gs == "Colours" || gs == "Notes" || gs == "Key-press effects" || gs == "Atmosphere");
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 12.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 12.0f, 1.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        auto top = r.removeFromTop (30);
        title.setBounds (top.removeFromLeft (110));
        theme.setBounds (top.reduced (0, 2));
        r.removeFromTop (8);
        panel.setBounds (r);
    }

    void syncTheme() { theme.setSelectedId ((int) state.getProperty ("theme", 0) + 1, juce::dontSendNotification); }

private:
    juce::ValueTree& state;
    juce::Label title;
    juce::ComboBox theme;
    juce::PropertyPanel panel;
};

//==============================================================================
struct ExportJob
{
    std::shared_ptr<ff::Track> track;
    FunkenflugEditor::ExportOptions opt;
    double leadIn = 1.0, tail = 3.0;
    juce::int64 frame = 0, total = 0;
    ff::PipeWriter pipe;
    ff::Fbo out;
    std::vector<juce::uint8> px;
    std::string ffmpeg, log;
    std::vector<std::string> args;
    bool started = false, failed = false;
    juce::File tempAudio;
};

struct HostedWindow : juce::DocumentWindow
{
    HostedWindow (const juce::String& name, juce::AudioProcessorEditor* ed, std::function<void()> onCloseFn)
        : DocumentWindow (name, juce::Colour (0xff101014), DocumentWindow::closeButton), onClose (std::move (onCloseFn))
    {
        setUsingNativeTitleBar (true);
        setContentOwned (ed, true);
        setResizable (ed->isResizable(), false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }
    void closeButtonPressed() override { if (onClose) onClose(); }
    std::function<void()> onClose;
};

//==============================================================================
FunkenflugEditor::FunkenflugEditor (FunkenflugProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    lnf = std::make_unique<FFLook>();
    setLookAndFeel (lnf.get());

    juce::PropertiesFile::Options o;
    o.applicationName = "Funkenflug";
    o.filenameSuffix = ".settings";
    o.folderName = "Funkenflug";
    o.osxLibrarySubFolder = "Application Support";
    global = std::make_unique<juce::PropertiesFile> (o);

    addAndMakeVisible (bar);
    for (auto* b : { &openB, &saveB, &recB, &playB, &stopB, &liveB, &renderB, &instB, &setB })
    {
        bar.addAndMakeVisible (b);
        b->setWantsKeyboardFocus (false);
    }
    bar.addAndMakeVisible (seek);
    bar.addAndMakeVisible (timeL);
    addAndMakeVisible (statusL);
    statusL.setJustificationType (juce::Justification::centred);
    statusL.setColour (juce::Label::backgroundColourId, juce::Colour (0xcc101014));
    statusL.setVisible (false);
    statusL.setInterceptsMouseClicks (false, false);
    timeL.setFont (juce::Font (juce::FontOptions (13.0f)));
    timeL.setColour (juce::Label::textColourId, kText);
    timeL.setJustificationType (juce::Justification::centred);
    seek.setSliderStyle (juce::Slider::LinearHorizontal);
    seek.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    seek.setWantsKeyboardFocus (false);
    seek.onDragStart = [this] { seekDragging = true; };
    seek.onDragEnd = [this] { seekDragging = false; proc.seek (seek.getValue()); };
    seek.onValueChange = [this] { if (seekDragging) proc.seek (seek.getValue()); };

    openB.onClick = [this] { openMidi(); };
    saveB.onClick = [this] { saveTake(); };
    recB.onClick = [this] { toggleRecord(); };
    playB.onClick = [this] { togglePlay(); };
    stopB.onClick = [this] { proc.stop(); };
    liveB.onClick = [this] { proc.pause(); liveView = ! liveView.load(); };
    renderB.onClick = [this] { if (exporting.load()) cancelExport = true; else showRenderDialog(); };
    instB.onClick = [this] { showInstrumentMenu(); };
    setB.onClick = [this] { panel->setVisible (! panel->isVisible()); setB.setToggleState (panel->isVisible(), juce::dontSendNotification); };

    panel = std::make_unique<SettingsPanel> (proc.state,
        [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Choose a background image", juce::File (global->getValue ("lastImageDir")), "*.png;*.jpg;*.jpeg;*.gif");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f.existsAsFile()) { global->setValue ("lastImageDir", f.getParentDirectory().getFullPathName()); proc.state.setProperty ("bgImage", f.getFullPathName(), nullptr); }
            });
        },
        [this] { proc.state.setProperty ("bgImage", "", nullptr); },
        [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Save preset", juce::File (global->getValue ("lastPresetDir")), "*.ffpreset");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == juce::File()) return;
                f = f.withFileExtension ("ffpreset");
                global->setValue ("lastPresetDir", f.getParentDirectory().getFullPathName());
                auto copy = proc.state.createCopy(); copy.removeAllChildren (nullptr);
                if (auto xml = copy.createXml()) xml->writeTo (f);
            });
        },
        [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Load preset", juce::File (global->getValue ("lastPresetDir")), "*.ffpreset");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
            {
                if (auto xml = juce::parseXML (fc.getResult()))
                {
                    auto t = juce::ValueTree::fromXml (*xml);
                    if (t.hasType (proc.state.getType()))
                        for (int i = 0; i < t.getNumProperties(); ++i)
                            if (t.getPropertyName (i) != juce::Identifier ("builtInSynth"))
                                proc.state.setProperty (t.getPropertyName (i), t.getProperty (t.getPropertyName (i)), nullptr);
                    panel->syncTheme();
                }
            });
        });
    addChildComponent (*panel);

    proc.state.addListener (this);
    addMouseListener (this, true);
    setWantsKeyboardFocus (true);
    setResizable (true, true);
    setResizeLimits (640, 360, 7680, 4320);
    setSize (1280, 720);

    loadBackground();
    startWall = lastWall = juce::Time::getMillisecondCounterHiRes() * 0.001;
    lastMouse = juce::Time::getMillisecondCounter();

    gl.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
    gl.setRenderer (this);
    gl.setComponentPaintingEnabled (true);
    gl.setContinuousRepainting (true);
    gl.attachTo (*this);

    startTimerHz (20);
    handleCommandLine();
}

FunkenflugEditor::~FunkenflugEditor()
{
    cancelExport = true;
    stopTimer();
    proc.state.removeListener (this);
    hostedWindow.reset();
    gl.detach();
    setLookAndFeel (nullptr);
}

void FunkenflugEditor::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& id)
{
    if (id == juce::Identifier ("bgImage")) loadBackground();
    if (id == juce::Identifier ("theme")) panel->syncTheme();
}

void FunkenflugEditor::loadBackground()
{
    juce::File f (proc.state.getProperty ("bgImage").toString());
    auto img = f.existsAsFile() ? juce::ImageFileFormat::loadFrom (f) : juce::Image();
    const juce::ScopedLock l (jobLock);
    pendingBg = img;
    bgDirty = true;
}

void FunkenflugEditor::resized()
{
    auto r = getLocalBounds();
    bar.setBounds (r.removeFromTop (44).reduced (8, 6));
    auto b = bar.getLocalBounds();
    auto place = [&b] (juce::Component& c, int w) { if (c.isVisible()) { c.setBounds (b.removeFromLeft (w)); b.removeFromLeft (4); } };
    place (openB, 58); place (saveB, 82); place (recB, 70); place (playB, 60); place (stopB, 54); place (liveB, 50);
    auto placeR = [&b] (juce::Component& c, int w) { if (c.isVisible()) { c.setBounds (b.removeFromRight (w)); b.removeFromRight (4); } };
    placeR (setB, 76); placeR (instB, 92); placeR (renderB, 108);
    timeL.setBounds (b.removeFromRight (96));
    seek.setBounds (b.reduced (6, 0));
    panel->setBounds (getWidth() - 372, 50, 362, juce::jmin (getHeight() - 60, 900));
    statusL.setBounds (getLocalBounds().withSizeKeepingCentre (juce::jmin (620, getWidth() - 40), 40).withY (getHeight() / 2 - 20));
}

//==============================================================================
int FunkenflugEditor::keyAt (juce::Point<int> p, float& vel) const
{
    const auto S = proc.getSnapshot();
    const float kbH = S.f[ff::keyboardHeight] * (float) getHeight();
    if (kbH < 4.0f || (float) p.y < (float) getHeight() - kbH) return -1;
    static const int los[] = { 21, 28, 36, 36, 0 }, his[] = { 108, 103, 96, 84, 127 };
    const int range = juce::jlimit (0, 4, S.c[ff::keyRange]), lo = los[range], hi = his[range];
    auto black = [] (int k) { const int pc = k % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; };
    int nWhite = 0;
    for (int k = lo; k <= hi; ++k) if (! black (k)) ++nWhite;
    const float ww = (float) getWidth() / (float) nWhite, bw = ww * 0.58f;
    const float yIn = ((float) p.y - ((float) getHeight() - kbH)) / kbH;
    vel = juce::jlimit (0.15f, 1.0f, 0.2f + yIn);
    int wi = 0, found = -1;
    for (int k = lo; k <= hi; ++k)
    {
        if (black (k)) { if (yIn < 0.63f && std::abs ((float) p.x - (float) wi * ww) < bw * 0.5f) return k; }
        else { if ((float) p.x >= (float) wi * ww && (float) p.x < (float) (wi + 1) * ww) found = k; ++wi; }
    }
    return found;
}

void FunkenflugEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent != this) return;
    float vel = 0.7f;
    mouseKey = keyAt (e.getPosition(), vel);
    if (mouseKey >= 0) proc.injectNote (mouseKey, vel, true);
    grabKeyboardFocus();
}

void FunkenflugEditor::mouseUp (const juce::MouseEvent&)
{
    if (mouseKey >= 0) proc.injectNote (mouseKey, 0.0f, false);
    mouseKey = -1;
}

void FunkenflugEditor::mouseMove (const juce::MouseEvent&) { lastMouse = juce::Time::getMillisecondCounter(); }

bool FunkenflugEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::spaceKey) { togglePlay(); return true; }
    if (k.getTextCharacter() == 'r') { toggleRecord(); return true; }
    if (k.getTextCharacter() == 's') { setB.triggerClick(); return true; }
    if (k.getTextCharacter() == 'o') { openMidi(); return true; }
    if (k.getTextCharacter() == 'l') { liveB.triggerClick(); return true; }
    if (k == juce::KeyPress::F11Key || k.getTextCharacter() == 'f')
    {
        if (auto* peer = getPeer()) peer->setFullScreen (! peer->isFullScreen());
        return true;
    }
    if (k == juce::KeyPress::escapeKey)
    {
        if (exporting.load()) cancelExport = true;
        else if (auto* peer = getPeer()) if (peer->isFullScreen()) peer->setFullScreen (false);
        return true;
    }
    return false;
}

//==============================================================================
void FunkenflugEditor::timerCallback()
{
    auto tr = proc.getTrack();
    const bool hasTrack = tr != nullptr, playing = proc.isPlaying(), rec = proc.isRecording(), exp = exporting.load();
    playB.setButtonText (playing ? "Pause" : "Play");
    playB.setEnabled (hasTrack && ! rec && ! exp);
    stopB.setEnabled (hasTrack && ! rec && ! exp);
    saveB.setEnabled (hasTrack && ! rec && ! exp);
    openB.setEnabled (! rec && ! exp);
    recB.setEnabled (! exp);
    recB.setButtonText (rec ? "Stop rec" : "Record");
    recB.setToggleState (rec, juce::dontSendNotification);
    recB.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd0303a).withAlpha (0.7f));
    liveB.setToggleState (liveView.load() || ! hasTrack, juce::dontSendNotification);
    liveB.setEnabled (hasTrack && ! rec && ! exp);
    renderB.setEnabled (hasTrack && ! rec);
    renderB.setButtonText (exp ? "Cancel" : "Render video");
    seek.setEnabled (hasTrack && ! rec && ! exp);
    if (hasTrack)
    {
        seek.setRange (0.0, juce::jmax (1.0, tr->length), 0.0);
        const double t = proc.playheadNow();
        if (! seekDragging) seek.setValue (juce::jmin (t, tr->length), juce::dontSendNotification);
        auto fmt = [] (double s) { const int i = (int) juce::jmax (0.0, s); return juce::String (i / 60) + ":" + juce::String (i % 60).paddedLeft ('0', 2); };
        timeL.setText (fmt (t) + " / " + fmt (tr->length), juce::dontSendNotification);
    }
    else timeL.setText (rec ? "recording" : "live", juce::dontSendNotification);

    if (exp)
    {
        const float pr = exportStage == "audio" ? audioProgress.load() : exportProgress.load();
        statusL.setText ((exportStage == "audio" ? "Rendering audio  " : "Rendering video  ") + juce::String ((int) (pr * 100.0f)) + " %   (Esc to cancel)", juce::dontSendNotification);
        statusL.setVisible (true);
    }

    else if (proc.loading.load())
    {
        statusL.setText ("Loading samples  " + juce::String ((int) (proc.loadProgress.load() * 100.0f)) + " %", juce::dontSendNotification);
        statusL.setVisible (true);
    }
    else if (statusL.isVisible()) statusL.setVisible (false);

    // auto-hide the toolbar while playing or performing
    const bool idle = juce::Time::getMillisecondCounter() - lastMouse > 3500;
    const bool wantBar = ! idle || panel->isVisible() || exp || bar.isMouseOver (true);
    if (wantBar != bar.isVisible())
    {
        if (wantBar) juce::Desktop::getInstance().getAnimator().fadeIn (&bar, 150);
        else         juce::Desktop::getInstance().getAnimator().fadeOut (&bar, 400);
        setMouseCursor (wantBar ? juce::MouseCursor::NormalCursor : juce::MouseCursor::NoCursor);
    }
}

void FunkenflugEditor::openMidi()
{
    chooser = std::make_unique<juce::FileChooser> ("Open a MIDI file", juce::File (global->getValue ("lastMidiDir")), "*.mid;*.midi");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        global->setValue ("lastMidiDir", f.getParentDirectory().getFullPathName());
        if (auto tr = ff::loadMidiFile (f)) { proc.setTrack (tr); liveView = false; }
        else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Funkenflug", "That file has no notes or is not a valid MIDI file.");
    });
}

void FunkenflugEditor::saveTake()
{
    auto tr = proc.getTrack();
    if (tr == nullptr) return;
    chooser = std::make_unique<juce::FileChooser> ("Save as MIDI file", juce::File (global->getValue ("lastMidiDir")).getChildFile (tr->name + ".mid"), "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this, tr] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        global->setValue ("lastMidiDir", f.getParentDirectory().getFullPathName());
        if (! ff::saveMidiFile (*tr, f.withFileExtension ("mid")))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Funkenflug", "The file could not be written.");
    });
}

void FunkenflugEditor::toggleRecord()
{
    if (exporting.load()) return;
    if (! proc.isRecording())
    {
        proc.pause();
        liveView = true;
        proc.startRecording();
    }
    else if (auto tr = proc.stopRecording())
    {
        proc.setTrack (tr);
        liveView = false;
    }
}

void FunkenflugEditor::togglePlay()
{
    if (proc.getTrack() == nullptr || proc.isRecording() || exporting.load()) return;
    if (proc.isPlaying()) proc.pause();
    else { liveView = false; proc.play(); }
}

void FunkenflugEditor::showInstrumentMenu()
{
    juce::PopupMenu m;
    auto* h = proc.getHosted();
    const bool standalone = proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
    auto& sfp = proc.soundFont;
    m.addSectionHeader (proc.hasSampleInstrument() ? "Loaded: " + proc.sampleInstrumentName() : juce::String ("No SoundFont / SFZ loaded"));
    m.addItem ("Load SoundFont or SFZ (.sf2 / .sfz)...", ! proc.loading.load(), false, [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a SoundFont (.sf2) or SFZ instrument (.sfz)", juce::File (global->getValue ("lastSfDir")), "*.sf2;*.sfz");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (! f.existsAsFile()) return;
            global->setValue ("lastSfDir", f.getParentDirectory().getFullPathName());
            juce::Component::SafePointer<FunkenflugEditor> safe (this);
            proc.loadSoundFontAsync (f, [safe] (juce::String err)
            {
                if (safe != nullptr && err.isNotEmpty())
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Funkenflug", err);
            });
        });
    });
    if (sfp.isLoaded())
    {
        juce::PopupMenu presets;
        auto names = sfp.presetNames();
        for (int i = 0; i < names.size(); ++i)
            presets.addItem (names[i], true, i == sfp.getPreset(), [this, i] { proc.soundFont.setPreset (i); proc.state.setProperty ("sfPreset", i, nullptr); });
        m.addSubMenu ("SoundFont preset", presets);
    }
    {
        juce::PopupMenu layers;
        const int cur = proc.defaultSfzLayerStep();
        const std::pair<const char*, int> opts[] = { { "All velocity layers (best, most memory)", 1 }, { "Every second layer (half the memory)", 2 }, { "Every fourth layer (quarter)", 4 } };
        for (auto& o : opts)
            layers.addItem (o.first, true, cur == o.second, [this, step = o.second]
            {
                proc.state.setProperty ("sfzLayers", step, nullptr);
                juce::File f (proc.state.getProperty ("sfPath").toString());
                if (proc.sfz.isLoaded() && f.existsAsFile()) proc.loadSoundFontAsync (f, {});
            });
        m.addSubMenu ("SFZ detail", layers);
    }
    if (proc.hasSampleInstrument()) m.addItem ("Remove SoundFont / SFZ", [this] { proc.unloadSoundFont(); });
    if (! standalone) { m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (instB)); return; }
    m.addSectionHeader (h != nullptr ? "VST3: " + h->getName() : juce::String ("No VST3 instrument loaded"));
    m.addItem ("Load VST3 instrument...", [this]
    {
       #if JUCE_WINDOWS
        juce::File start ("C:\\Program Files\\Common Files\\VST3");
       #else
        juce::File start (juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".vst3"));
       #endif
        start = juce::File (global->getValue ("lastVstDir", start.getFullPathName()));
        chooser = std::make_unique<juce::FileChooser> ("Choose a VST3 instrument (.vst3)", start, "*.vst3");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (! f.exists()) return;
            global->setValue ("lastVstDir", f.getParentDirectory().getFullPathName());
            hostedWindow.reset();
            auto err = proc.loadHostedPlugin (f.getFullPathName());
            if (err.isNotEmpty()) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Funkenflug", err);
        });
    });
    m.addItem ("Show instrument window", h != nullptr && h->hasEditor(), false, [this]
    {
        if (auto* inst = proc.getHosted())
            if (auto* ed = inst->createEditorIfNeeded())
                hostedWindow = std::make_unique<HostedWindow> (inst->getName(), ed, [this] { hostedWindow.reset(); });
    });
    m.addItem ("Unload instrument", h != nullptr, false, [this] { hostedWindow.reset(); proc.unloadHostedPlugin(); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (instB));
}

//==============================================================================
juce::String FunkenflugEditor::findFfmpeg() const
{
   #if JUCE_WINDOWS
    const juce::String exe ("ffmpeg.exe");
   #else
    const juce::String exe ("ffmpeg");
   #endif
    juce::Array<juce::File> candidates;
    candidates.add (juce::File (global->getValue ("ffmpegPath")));
    candidates.add (juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile (exe));
    candidates.add (juce::File::getSpecialLocation (juce::File::currentApplicationFile).getSiblingFile (exe));
    candidates.add (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Funkenflug").getChildFile (exe));
    for (auto& c : candidates) if (c.existsAsFile()) { global->setValue ("ffmpegPath", c.getFullPathName()); return c.getFullPathName(); }
    return exe;     // rely on PATH
}

void FunkenflugEditor::showRenderDialog()
{
    if (proc.getTrack() == nullptr) return;
    auto* aw = new juce::AlertWindow ("Render video", "The loaded MIDI file or recorded take is rendered to an mp4.", juce::MessageBoxIconType::NoIcon, this);
    aw->addComboBox ("res", { "1280 x 720 (720p)", "1920 x 1080 (1080p)", "2560 x 1440 (1440p)", "3840 x 2160 (4K)" }, "Resolution");
    aw->addComboBox ("fps", { "30 fps", "60 fps" }, "Frame rate");
    aw->addComboBox ("q", { "Draft (smallest file)", "Good", "High", "Near lossless (large)" }, "Video quality");
    aw->addComboBox ("audio", { "No audio", "Current instrument (VST3 / SoundFont / SFZ / built-in)", "Audio file (wav / mp3 / flac)..." }, "Audio");
    aw->addComboBox ("kbps", { "128 kbps", "192 kbps", "256 kbps", "320 kbps" }, "Audio quality (AAC)");
    aw->getComboBoxComponent ("res")->setSelectedItemIndex (global->getIntValue ("rRes", 1));
    aw->getComboBoxComponent ("fps")->setSelectedItemIndex (global->getIntValue ("rFps", 1));
    aw->getComboBoxComponent ("q")->setSelectedItemIndex (global->getIntValue ("rQ", 2));
    aw->getComboBoxComponent ("audio")->setSelectedItemIndex (global->getIntValue ("rAudio", 1));
    aw->getComboBoxComponent ("kbps")->setSelectedItemIndex (global->getIntValue ("rKbps", 2));
    aw->addButton ("Render...", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<FunkenflugEditor> safe (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe, aw] (int result)
    {
        if (result != 1 || safe == nullptr) return;
        auto& self = *safe;
        static const int ws[] = { 1280, 1920, 2560, 3840 }, hs[] = { 720, 1080, 1440, 2160 }, crfs[] = { 26, 21, 17, 12 }, kb[] = { 128, 192, 256, 320 };
        const int ri = aw->getComboBoxComponent ("res")->getSelectedItemIndex(), fi = aw->getComboBoxComponent ("fps")->getSelectedItemIndex(),
                  qi = aw->getComboBoxComponent ("q")->getSelectedItemIndex(), ai = aw->getComboBoxComponent ("audio")->getSelectedItemIndex(),
                  ki = aw->getComboBoxComponent ("kbps")->getSelectedItemIndex();
        self.global->setValue ("rRes", ri); self.global->setValue ("rFps", fi); self.global->setValue ("rQ", qi);
        self.global->setValue ("rAudio", ai); self.global->setValue ("rKbps", ki);
        ExportOptions o;
        o.w = ws[ri]; o.h = hs[ri]; o.fps = fi == 0 ? 30 : 60; o.crf = crfs[qi]; o.audioMode = ai; o.audioKbps = kb[ki];
        auto tr = self.proc.getTrack();
        self.chooser = std::make_unique<juce::FileChooser> ("Save video as", juce::File (self.global->getValue ("lastVideoDir")).getChildFile ((tr != nullptr ? tr->name : juce::String ("video")) + ".mp4"), "*.mp4");
        self.chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [safe, o] (const juce::FileChooser& fc) mutable
        {
            if (safe == nullptr || fc.getResult() == juce::File()) return;
            o.out = fc.getResult().withFileExtension ("mp4");
            safe->global->setValue ("lastVideoDir", o.out.getParentDirectory().getFullPathName());
            if (o.audioMode != 2) { safe->startExport (o); return; }
            safe->chooser = std::make_unique<juce::FileChooser> ("Choose the audio file", juce::File (safe->global->getValue ("lastVideoDir")), "*.wav;*.mp3;*.flac;*.ogg;*.m4a");
            safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe, o] (const juce::FileChooser& fc2) mutable
            {
                if (safe == nullptr || ! fc2.getResult().existsAsFile()) return;
                o.audioFile = fc2.getResult();
                safe->startExport (o);
            });
        });
    }), true);
}

void FunkenflugEditor::startExport (ExportOptions o)
{
    auto tr = proc.getTrack();
    if (tr == nullptr || exporting.load()) return;
    proc.pause();
    hostedWindow.reset();
    exporting = true;
    cancelExport = false;
    exportProgress = 0.0f; audioProgress = 0.0f;

    if (o.audioMode == 1)
    {
        exportStage = "audio";
        auto wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("funkenflug_audio_" + juce::String (juce::Time::currentTimeMillis()) + ".wav");
        juce::Component::SafePointer<FunkenflugEditor> safe (this);
        auto* p = &proc;
        auto* prog = &audioProgress; auto* cancel = &cancelExport;
        juce::Thread::launch ([safe, p, tr, wav, o, prog, cancel]
        {
            const bool ok = p->renderAudioOffline (*tr, wav, 1.0, 3.0, *prog, *cancel);
            juce::MessageManager::callAsync ([safe, ok, wav, o]
            {
                if (safe == nullptr) { wav.deleteFile(); return; }
                if (! ok) { wav.deleteFile(); safe->exportFinished (false, safe->cancelExport.load() ? "Render cancelled." : "The audio could not be rendered.", o.quitWhenDone); return; }
                safe->launchVideoJob (o, wav, 0.0);
            });
        });
    }
    else launchVideoJob (o, o.audioMode == 2 ? o.audioFile : juce::File(), 1.0);
}

void FunkenflugEditor::launchVideoJob (ExportOptions o, juce::File audio, double audioOffset)
{
    exportStage = "video";
    auto job = std::make_unique<ExportJob>();
    job->track = proc.getTrack();
    job->opt = o;
    job->total = (juce::int64) std::ceil ((job->leadIn + job->track->length + job->tail) * o.fps);
    job->ffmpeg = findFfmpeg().toStdString();
    job->log = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("funkenflug_ffmpeg.log").getFullPathName().toStdString();
    if (o.audioMode == 1) job->tempAudio = audio;
    auto add = [&job] (std::initializer_list<juce::String> a) { for (auto& s : a) job->args.push_back (s.toStdString()); };
    add ({ "-y", "-f", "rawvideo", "-pix_fmt", "rgba", "-s", juce::String (o.w) + "x" + juce::String (o.h), "-r", juce::String (o.fps), "-i", "-" });
    if (audio.existsAsFile())
    {
        if (audioOffset > 0.0) add ({ "-itsoffset", juce::String (audioOffset, 3) });
        add ({ "-i", audio.getFullPathName() });
    }
    add ({ "-vf", "vflip", "-c:v", "libx264", "-preset", "medium", "-crf", juce::String (o.crf), "-pix_fmt", "yuv420p", "-movflags", "+faststart" });
    if (audio.existsAsFile())
        add ({ "-map", "0:v:0", "-map", "1:a:0", "-c:a", "aac", "-b:a", juce::String (o.audioKbps) + "k", "-t", juce::String ((double) job->total / o.fps, 3) });
    add ({ o.out.getFullPathName() });
    const juce::ScopedLock l (jobLock);
    pendingJob = std::move (job);
}

void FunkenflugEditor::exportFinished (bool ok, juce::String message, bool quit)
{
    exporting = false;
    if (quit)
    {
        std::cout << (ok ? "OK " : "FAILED ") << message << std::endl;
        juce::JUCEApplicationBase::quit();
        return;
    }
    juce::AlertWindow::showMessageBoxAsync (ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon, "Render video", message);
}

void FunkenflugEditor::handleCommandLine()
{
    if (! juce::JUCEApplicationBase::isStandaloneApp()) return;
    auto args = juce::JUCEApplicationBase::getCommandLineParameterArray();
    const int i = args.indexOf ("--render");
    if (i < 0 || i + 2 >= args.size()) return;
    auto val = [&args] (const char* name, juce::String def) { const int k = args.indexOf (name); return k >= 0 && k + 1 < args.size() ? args[k + 1] : def; };
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    auto tr = ff::loadMidiFile (cwd.getChildFile (args[i + 1]));
    if (tr == nullptr) { std::cout << "FAILED cannot read MIDI file" << std::endl; juce::MessageManager::callAsync ([] { juce::JUCEApplicationBase::quit(); }); return; }
    if (args.contains ("--theme")) ff::applyTheme (proc.state, val ("--theme", "0").getIntValue());
    for (int k = 0; k + 1 < args.size(); ++k)
        if (args[k] == "--set") proc.state.setProperty (args[k + 1].upToFirstOccurrenceOf ("=", false, false), args[k + 1].fromFirstOccurrenceOf ("=", false, false).getDoubleValue(), nullptr);
    if (args.contains ("--sf2"))
    {
        auto err = proc.loadSoundFontNow (cwd.getChildFile (val ("--sf2", "")), val ("--sfz-layers", "1").getIntValue());
        if (err.isEmpty()) { proc.soundFont.setPreset (val ("--sf2-preset", "0").getIntValue()); std::cout << "instrument: " << proc.sampleInstrumentName() << std::endl; }
        else std::cout << "instrument failed: " << err << std::endl;
    }
    if (args.contains ("--vst3")) { auto err = proc.loadHostedPlugin (cwd.getChildFile (val ("--vst3", "")).getFullPathName()); std::cout << "vst3: " << (err.isEmpty() ? juce::String ("loaded ") + proc.getHosted()->getName() : err) << std::endl; }
    if (args.contains ("--bg")) proc.state.setProperty ("bgImage", cwd.getChildFile (val ("--bg", "")).getFullPathName(), nullptr);
    proc.setTrack (tr);
    liveView = false;
    if (args.contains ("--ui")) { proc.seek (val ("--ui", "5").getDoubleValue()); if (args.contains ("--settings")) setB.triggerClick(); return; }
    ExportOptions o;
    auto size = val ("--size", "1920x1080");
    o.w = size.upToFirstOccurrenceOf ("x", false, true).getIntValue();
    o.h = size.fromFirstOccurrenceOf ("x", false, true).getIntValue();
    o.fps = val ("--fps", "60").getIntValue();
    o.crf = val ("--crf", "18").getIntValue();
    o.audioMode = args.contains ("--no-audio") ? 0 : 1;
    o.out = cwd.getChildFile (args[i + 2]);
    o.quitWhenDone = true;
    juce::Component::SafePointer<FunkenflugEditor> safe (this);
    juce::Timer::callAfterDelay (300, [safe, o] { if (safe != nullptr) safe->startExport (o); });
}

//==============================================================================
void FunkenflugEditor::newOpenGLContextCreated()
{
    renderer.init();
    bgDirty = true;
    gl.setSwapInterval (1);
}

void FunkenflugEditor::openGLContextClosing()
{
    if (activeJob != nullptr) { activeJob->pipe.close(); activeJob->out.release(); activeJob.reset(); }
    renderer.shutdown();
}

void FunkenflugEditor::renderOpenGL()
{
    const double scale = gl.getRenderingScale();
    const int W = juce::roundToInt (scale * getWidth()), H = juce::roundToInt (scale * getHeight());
    GLint screen = 0;
    glGetIntegerv (GL_FRAMEBUFFER_BINDING, &screen);
    const auto S = proc.getSnapshot();

    if (bgDirty.exchange (false))
    {
        juce::Image img;
        { const juce::ScopedLock l (jobLock); img = pendingBg; }
        renderer.setBackground (img);
    }
    if (activeJob == nullptr)
    {
        const juce::ScopedLock l (jobLock);
        activeJob = std::move (pendingJob);
    }

    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;

    if (auto* job = activeJob.get())
    {
        auto& o = job->opt;
        if (! job->started)
        {
            job->started = true;
            renderer.resetParticles();
            job->px.resize ((size_t) o.w * (size_t) o.h * 4);
            job->failed = ! job->pipe.open (job->ffmpeg, job->args, job->log);
        }
        job->out.ensure (o.w, o.h, false);
        bool writeFailed = false;
        while (! job->failed && job->frame < job->total && ! cancelExport.load()
               && juce::Time::getMillisecondCounterHiRes() * 0.001 - now < 0.03)
        {
            ff::FrameInput F;
            F.notes = &job->track->notes;
            F.t = (double) job->frame / o.fps - job->leadIn;
            F.dt = 1.0 / o.fps;
            F.wallTime = (double) job->frame / o.fps;
            F.falling = S.c[ff::direction] == 0;
            F.pedal = ff::pedalAt (job->track->pedal, F.t);
            F.W = o.w; F.H = o.h; F.targetFbo = job->out.fbo;
            renderer.render (S, F);
            glBindFramebuffer (GL_FRAMEBUFFER, job->out.fbo);
            glPixelStorei (GL_PACK_ALIGNMENT, 1);
            glReadPixels (0, 0, o.w, o.h, GL_RGBA, GL_UNSIGNED_BYTE, job->px.data());
            if (! job->pipe.write (job->px.data(), job->px.size())) { writeFailed = true; break; }
            ++job->frame;
        }
        exportProgress = (float) job->frame / (float) juce::jmax ((juce::int64) 1, job->total);
        renderer.blit (job->out.tex, o.w, o.h, W, H, (unsigned) screen);

        if (job->failed || writeFailed || cancelExport.load() || job->frame >= job->total)
        {
            const bool cancelled = cancelExport.load();
            const int code = job->pipe.close();
            const bool ok = ! job->failed && ! writeFailed && ! cancelled && code == 0;
            juce::String msg;
            if (ok) msg = "Saved " + o.out.getFullPathName();
            else if (cancelled) { msg = "Render cancelled."; o.out.deleteFile(); }
            else msg = "ffmpeg could not be started or stopped with an error.\n\nPut ffmpeg.exe next to Funkenflug.exe (or install ffmpeg so it is on the PATH).\nDetails: " + juce::String (job->log);
            job->tempAudio.deleteFile();
            job->out.release();
            const bool quit = o.quitWhenDone;
            activeJob.reset();
            renderer.resetParticles();
            juce::Component::SafePointer<FunkenflugEditor> safe (this);
            juce::MessageManager::callAsync ([safe, ok, msg, quit] { if (safe != nullptr) safe->exportFinished (ok, msg, quit); });
        }
        lastWall = now;
        return;
    }

    proc.drainLive();
    ff::FrameInput F;
    F.W = W; F.H = H; F.targetFbo = (unsigned) screen;
    F.dt = now - lastWall; lastWall = now;
    F.wallTime = now - startWall;

    std::vector<ff::VNote> liveNotes;
    float livePedal = 0.0f;
    proc.withLive ([&] (ff::TrackBuilder& b) { liveNotes = b.notes; livePedal = b.pedalNow; });
    std::vector<std::pair<int, float>> held;
    auto tr = proc.getTrack();
    if (tr != nullptr && ! liveView.load())
    {
        F.notes = &tr->notes;
        F.t = proc.playheadNow();
        F.falling = S.c[ff::direction] == 0;
        F.pedal = juce::jmax (livePedal, ff::pedalAt (tr->pedal, F.t));
        for (auto& n : liveNotes) if (n.off >= ff::kInf) held.push_back ({ n.key, n.vel });
        F.heldExtra = &held;
    }
    else
    {
        F.notes = &liveNotes;
        F.t = proc.liveNow();
        F.falling = false;
        F.pedal = livePedal;
    }
    renderer.render (S, F);
}
