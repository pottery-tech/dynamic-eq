#pragma once
#include <JuceHeader.h>

//==============================================================================
/** Extended filter types matching Pro-Q style workflow */
enum class EQFilterType
{
    Bell = 0,
    LowShelf,
    HighShelf,
    LowPass,
    HighPass,
    Notch,
    BandPass,
    Tilt,
    NUM_TYPES
};

enum class EQSlope { dB6=0, dB12, dB24, dB48 };
enum class DetectionMode { Internal=0, Sidechain, Manual };

static inline juce::String filterTypeName(EQFilterType t)
{
    switch(t){
        case EQFilterType::Bell:      return "Bell";
        case EQFilterType::LowShelf:  return "Low Shelf";
        case EQFilterType::HighShelf: return "High Shelf";
        case EQFilterType::LowPass:   return "Low Pass";
        case EQFilterType::HighPass:  return "High Pass";
        case EQFilterType::Notch:     return "Notch";
        case EQFilterType::BandPass:  return "Band Pass";
        case EQFilterType::Tilt:      return "Tilt";
        default: return "Unknown";
    }
}

//==============================================================================
/** One EQ band: any filter type + optional dynamics */
class DynamicBand
{
public:
    DynamicBand() = default;

    void prepare(double sampleRate, int blockSize);
    void reset();

    // Filter params
    void setEnabled     (bool e)         { enabled = e; }
    void setFilterType  (EQFilterType t) { filterType = t; coeffsDirty = true; }
    void setFrequency   (float hz)       { freq = hz;      coeffsDirty = true; }
    void setGain        (float dB)       { gainDB = dB;    coeffsDirty = true; }
    void setQ           (float q)        { bandQ = q;      coeffsDirty = true; }
    void setSlope       (EQSlope s)      { slope = s;      coeffsDirty = true; }

    // Dynamics params
    void setDynamicEnabled(bool e)       { dynamicEnabled = e; }
    void setThreshold   (float dB)       { thresholdDB = dB; }
    void setRatio       (float r)        { ratio = r; }
    void setAttack      (float ms)       { setAttackMs(ms); }
    void setRelease     (float ms)       { setReleaseMs(ms); }
    void setKneeWidth   (float dB)       { kneeDB = dB; }
    void setMakeupGain  (float dB)       { makeupDB = dB; }
    void setDetectionMode(DetectionMode m){ detMode = m; }
    void setManualGR    (float dB)       { manualGRdB = dB; }

    bool isEnabled()        const { return enabled; }
    bool isDynamicEnabled() const { return dynamicEnabled; }
    EQFilterType getFilterType() const { return filterType; }
    float getFreq()  const { return freq; }
    float getGain()  const { return gainDB; }
    float getQ()     const { return bandQ; }
    float getGainReductionDB() const { return currentGRdB; }

    void processSidechain(const float* scL, const float* scR, int numSamples);
    float processBlock(float* L, float* R, int numSamples);

private:
    using Filter = juce::dsp::IIR::Filter<float>;

    // Up to 4 cascaded biquads for high-order HP/LP
    static constexpr int MAX_CASCADE = 4;
    Filter filterL[MAX_CASCADE], filterR[MAX_CASCADE];
    Filter detFilterL, detFilterR;
    int    cascadeCount { 1 };

    void updateCoeffs();
    void buildCoeffsForStage(int stage, double sr,
                             juce::dsp::IIR::Coefficients<float>::Ptr& outCoeffs) const;

    EQFilterType filterType   { EQFilterType::Bell };
    EQSlope      slope        { EQSlope::dB12 };
    float freq                { 1000.f };
    float gainDB              { 0.f };
    float bandQ               { 0.707f };
    bool  coeffsDirty         { true };
    bool  enabled             { false };   // bands start disabled; enable on add

    // Dynamics
    bool  dynamicEnabled  { false };
    float thresholdDB     { -20.f };
    float ratio           { 4.f };
    float kneeDB          { 6.f };
    float makeupDB        { 0.f };
    float attackCoeff     { 0.f };
    float releaseCoeff    { 0.f };
    float envelope        { 0.f };
    float currentGRdB     { 0.f };
    float manualGRdB      { 0.f };
    DetectionMode detMode { DetectionMode::Internal };
    float scEnvL          { 0.f };

    double sr { 44100.0 };

    void setAttackMs(float ms);
    void setReleaseMs(float ms);
    float computeGain(float levelDB) const;
};
