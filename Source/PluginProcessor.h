#pragma once
#include <JuceHeader.h>
#include "DynamicBand.h"
#include "SpectrumAnalyser.h"

class DynamicEQProcessor : public juce::AudioProcessor
{
public:
    static constexpr int MAX_BANDS = 24;

    //── Processing modes ───────────────────────────────────────────────────────
    enum class ProcessingMode { Stereo=0, MidSide };
    enum class PhaseMode       { Minimum=0, Linear  };

    DynamicEQProcessor();
    ~DynamicEQProcessor() override = default;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Dynamic EQ"; }
    bool   acceptsMidi()  const override { return true; }
    bool   producesMidi() const override { return false; }
    double getTailLengthSeconds() const override;

    int  getNumPrograms() override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    //── APVTS ──────────────────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;
    juce::UndoManager undoManager;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //── Band management ────────────────────────────────────────────────────────
    DynamicBand& getBand(int i)              { return bands[i]; }
    float getBandGainReduction(int i) const  { return bands[i].getGainReductionDB(); }
    int   addBand(float freqHz, EQFilterType type = EQFilterType::Bell);
    void  removeBand(int index);
    int   getBandCount() const               { return bandCount; }
    void  syncBandParams(int b);

    //── Mode access ────────────────────────────────────────────────────────────
    ProcessingMode getProcessingMode() const { return processingMode; }
    PhaseMode      getPhaseMode()      const { return phaseMode; }
    void setProcessingMode(ProcessingMode m);
    void setPhaseMode(PhaseMode m);

    //── Analysers ──────────────────────────────────────────────────────────────
    SpectrumAnalyser& getAnalyser()      { return analyser; }
    SpectrumAnalyser& getInputAnalyser() { return inputAnalyser; }
    SpectrumAnalyser& getScAnalyser()    { return scAnalyser; }
    SpectrumAnalyser& getMidAnalyser()   { return midAnalyser; }
    SpectrumAnalyser& getSideAnalyser()  { return sideAnalyser; }

    //── Parameter ID helper ────────────────────────────────────────────────────
    static juce::String pid(int b, const char* n)
        { return "b"+juce::String(b)+"_"+n; }

private:
    DynamicBand bands[MAX_BANDS];
    int         bandCount { 0 };

    ProcessingMode processingMode { ProcessingMode::Stereo };
    PhaseMode      phaseMode      { PhaseMode::Minimum };

    //── Linear phase FIR convolution (one per band, stereo) ────────────────────
    // We use JUCE's Convolution engine with a generated FIR from the biquad IR
    static constexpr int LP_FIR_SIZE = 2048;   // latency in samples
    juce::dsp::Convolution lpConvL, lpConvR;
    bool lpConvReady { false };
    void rebuildLinearPhaseIR();

    //── M/S helpers ────────────────────────────────────────────────────────────
    static void encodeMS (float& mid, float& side, float L, float R);
    static void decodeMS (float& L,   float& R,    float mid, float side);

    SpectrumAnalyser analyser, inputAnalyser, scAnalyser, midAnalyser, sideAnalyser;
    static constexpr int SC_BUS = 1;

    double currentSR { 44100.0 };
    int    currentBS { 512 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicEQProcessor)
};
