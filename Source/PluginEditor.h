#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PresetManager.h"

//==============================================================================
/** Colours */
namespace EQColours {
    const juce::Colour BG        { 0xff1a1a24 };
    const juce::Colour DisplayBG { 0xff0d0d14 };
    const juce::Colour Grid      { 0xff2a2a3a };
    const juce::Colour GridText  { 0xff555566 };
    const juce::Colour Curve     { 0xffffffff };
    const juce::Colour CurveFill { 0x18ffffff };
    const juce::Colour Spectrum  { 0xff00aaff };
    const juce::Colour SCSpect   { 0xffaa44ff };
    const juce::Colour Frozen    { 0xff44ccff };
    const juce::Colour Handle    { 0xffffffff };

    // Per-band colours cycling through palette
    inline juce::Colour bandColour(int b) {
        static const juce::Colour cols[] = {
            juce::Colour(0xff4fc3f7), juce::Colour(0xffffb74d), juce::Colour(0xffef5350),
            juce::Colour(0xff66bb6a), juce::Colour(0xffab47bc), juce::Colour(0xffff7043),
            juce::Colour(0xff26c6da), juce::Colour(0xffffd54f)
        };
        return cols[b % 8];
    }
}

//==============================================================================
/** Magnitude response math (standalone, no DSP objects needed) */
struct BandResponse
{
    static float magnitude(int typeIdx, float freq, float gainDB, float q,
                           int slopeIdx, float hz, double sr);
};

//==============================================================================
/** The interactive EQ display — spectrum + curve + draggable handles */
class EQDisplay : public juce::Component,
                  private juce::Timer
{
public:
    explicit EQDisplay(DynamicEQProcessor& proc);
    ~EQDisplay() override { stopTimer(); }

    void paint            (juce::Graphics&) override;
    void resized          () override {}
    void mouseDown        (const juce::MouseEvent&) override;
    void mouseDrag        (const juce::MouseEvent&) override;
    void mouseUp          (const juce::MouseEvent&) override;
    void mouseMove        (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove   (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    int  getSelectedBand() const { return selectedBand; }
    void setSelectedBand(int b)  { selectedBand=b; repaint(); }
    std::function<void(int)> onBandSelected;
    std::function<void(int)> onBandRemoved;

private:
    void timerCallback() override { repaint(); }

    DynamicEQProcessor& proc;
    int selectedBand { -1 };
    int hoveredBand  { -1 };
    int dragBand     { -1 };

    // Coordinate helpers
    float freqToX(float hz) const;
    float gainToY(float dB) const;
    float xToFreq(float x)  const;
    float yToGain(float y)  const;
    int   bandAtPoint(juce::Point<float> p, float radiusPx=12.f) const;

    void showBandContextMenu(int band, juce::Point<int> screenPos);

    void drawGrid      (juce::Graphics&, float W, float H);
    void drawSpectraBG (juce::Graphics&, float W, float H);
    void drawSpectraFG (juce::Graphics&, float W, float H);
    void drawEQCurves  (juce::Graphics&, float W, float H);
    void drawHandles   (juce::Graphics&, float W, float H);

    static constexpr float MIN_DB=-30.f, MAX_DB=30.f, MIN_HZ=20.f, MAX_HZ=20000.f;
    static constexpr float HANDLE_R=9.f;
};

//==============================================================================
/** Compact controls panel for the selected band */
class BandControlPanel : public juce::Component
{
public:
    explicit BandControlPanel(DynamicEQProcessor& proc);

    void setSelectedBand(int b);
    int  getSelectedBand() const { return band; }
    void setUIScale(float s) { uiScale = s; resized(); }
    void updateDynState();
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void rebuild();

    DynamicEQProcessor& proc;
    int   band    { -1 };
    float uiScale { 1.f };

    // EQ section
    juce::Slider freqSlider, gainSlider, qSlider;
    juce::ComboBox typeBox, slopeBox, msRouteBox;
    juce::TextButton bypassButton{"BYPASS"};

    // Dynamics section
    juce::Slider threshSlider, ratioSlider, attackSlider, releaseSlider;
    juce::ComboBox detBox;
    juce::TextButton dynButton{"  DYN  "};

    // Labels — 0-2 EQ, 3-6 Dyn
    juce::Label labels[8];
    juce::Label eqSectionLabel, dynSectionLabel;

    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SA> freqAtt,gainAtt,qAtt,threshAtt,ratioAtt,attackAtt,releaseAtt;
    std::unique_ptr<CA> typeAtt,slopeAtt,detAtt,msRouteAtt;

    static juce::String pid(int b, const char* n)
        { return DynamicEQProcessor::pid(b,n); }
};

//==============================================================================
/** Draggable divider */
class DividerBar : public juce::Component
{
public:
    std::function<void(int)> onDrag;
    DividerBar(){ setMouseCursor(juce::MouseCursor::UpDownResizeCursor); }
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent&) override { lastY=0; }
    void mouseDrag(const juce::MouseEvent& e) override {
        int d=e.getDistanceFromDragStartY()-lastY;
        lastY=e.getDistanceFromDragStartY();
        if(onDrag) onDrag(d);
    }
private: int lastY{0};
};

//==============================================================================
class DynamicEQEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit DynamicEQEditor(DynamicEQProcessor&);
    ~DynamicEQEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void doLayout();
    void selectBand(int b);
    void applyScale(float s);

    DynamicEQProcessor& proc;

    EQDisplay        eqDisplay;
    DividerBar       divider;
    BandControlPanel controlPanel;

    // Top toolbar
    juce::TextButton freezeBtn    {"[*] Freeze"};
    juce::TextButton clearFreezeBtn{"Clear"};
    juce::TextButton speedBtn     {"Spd: Med"};
    juce::TextButton scaleBtn     {"100%"};
    juce::TextButton addBandBtn   {"+ Band"};
    juce::TextButton undoBtn      {"Undo"};
    juce::TextButton redoBtn      {"Redo"};
    juce::TextButton msBtn        {"Stereo"};   // toggles Stereo / Mid-Side
    juce::TextButton lpBtn        {"Min Phase"}; // toggles Min / Linear phase
    juce::TextButton presetSaveBtn{"Save"};
    juce::TextButton presetLoadBtn{"Presets"};
    juce::Label      presetNameLabel;
    juce::Label      titleLabel;

    // Output gain
    juce::Slider outGainSlider;
    juce::Label  outGainLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment outGainAtt;

    std::unique_ptr<class PresetManager> presetManager;

    int   eqHeightPx { 0 };
    float currentScale { 1.f };

    static constexpr int BASE_W    = 960;
    static constexpr int BASE_H    = 580;
    static constexpr int TOOLBAR_H = 62;   // two rows
    static constexpr int DIVIDER_H = 8;
    static constexpr int OUTGAIN_H = 28;
    static constexpr int MIN_EQ_H  = 120;
    static constexpr int MIN_CTL_H = 90;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicEQEditor)
};
