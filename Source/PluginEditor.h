#pragma once
#include "PluginProcessor.h"
#include "Renderer.h"
#include "PipeWriter.h"

struct ExportJob;
class SettingsPanel;

class FunkenflugEditor : public juce::AudioProcessorEditor,
                         private juce::OpenGLRenderer,
                         private juce::Timer,
                         private juce::ValueTree::Listener
{
public:
    explicit FunkenflugEditor (FunkenflugProcessor&);
    ~FunkenflugEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    struct ExportOptions { int w = 1920, h = 1080, fps = 60, crf = 18, audioMode = 1, audioKbps = 256; juce::File out, audioFile; bool quitWhenDone = false; };
    void startExport (ExportOptions);

private:
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    void timerCallback() override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    void openMidi();
    void saveTake();
    void toggleRecord();
    void togglePlay();
    void showRenderDialog();
    void showInstrumentMenu();
    void loadBackground();
    void launchVideoJob (ExportOptions, juce::File audio, double audioOffset);
    void exportFinished (bool ok, juce::String message, bool quit);
    void handleCommandLine();
    int keyAt (juce::Point<int>, float& vel) const;
    juce::String findFfmpeg() const;

    FunkenflugProcessor& proc;
    juce::OpenGLContext gl;
    ff::Renderer renderer;
    std::unique_ptr<juce::LookAndFeel> lnf;

    juce::Component bar;
    juce::TextButton openB { "Open" }, saveB { "Save take" }, recB { "Record" }, playB { "Play" }, stopB { "Stop" },
                     liveB { "Live" }, renderB { "Render video" }, instB { "Instrument" }, setB { "Settings" };
    juce::Slider seek;
    juce::Label timeL, statusL;
    std::unique_ptr<SettingsPanel> panel;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::DocumentWindow> hostedWindow;
    std::unique_ptr<juce::PropertiesFile> global;

    std::atomic<bool> liveView { true }, bgDirty { true }, cancelExport { false }, exporting { false };
    std::atomic<float> exportProgress { 0.0f }, audioProgress { 0.0f };
    juce::CriticalSection jobLock;
    std::unique_ptr<ExportJob> pendingJob, activeJob;
    juce::Image pendingBg;
    double lastWall = 0.0, startWall = 0.0;
    juce::uint32 lastMouse = 0;
    int mouseKey = -1;
    bool seekDragging = false;
    juce::String exportStage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FunkenflugEditor)
};
