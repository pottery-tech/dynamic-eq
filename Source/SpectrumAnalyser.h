#pragma once
#include <JuceHeader.h>

class SpectrumAnalyser
{
public:
    static constexpr int FFT_ORDER = 11;
    static constexpr int FFT_SIZE  = 1 << FFT_ORDER;

    /** Smoothing speed — controls how quickly the spectrum responds */
    enum class Speed { Slow=0, Medium, Fast, VeryFast };

    SpectrumAnalyser();

    void prepare(double sampleRate);
    void pushSamples(const float* L, const float* R, int numSamples);

    /** Freeze: captures snapshot and stops live updates.
        Unfreeze: resumes live updates but keeps snapshot visible until clearSnapshot() */
    void setFrozen(bool shouldFreeze);
    bool isFrozen() const { return frozen.load(); }

    /** Clear the frozen snapshot so it disappears from the display */
    void clearSnapshot();

    /** Smoothing speed for live spectrum */
    void setSpeed(Speed s) { speed = s; }
    Speed getSpeed() const { return speed; }

    juce::Path getPath(float x0, float x1, float y0, float y1,
                       float minDB=-80.f, float maxDB=6.f);

    juce::Path getFrozenPath(float x0, float x1, float y0, float y1,
                             float minDB=-80.f, float maxDB=6.f);

    bool hasFrozenData() const { return frozenDataValid.load(); }

private:
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::array<float, FFT_SIZE * 2> fftData   {};
    std::array<float, FFT_SIZE / 2> scopeData {};
    std::array<float, FFT_SIZE / 2> frozenData{};
    std::array<float, FFT_SIZE>     fifoBuffer{};
    int fifoIndex { 0 };

    std::atomic<bool> nextFFTBlockReady { false };
    std::atomic<bool> frozen            { false };
    std::atomic<bool> frozenDataValid   { false };
    Speed speed { Speed::Medium };
    double sr { 44100.0 };

    void drawNextFrameOfSpectrum();
    void snapshot();

    juce::Path buildPath(const std::array<float, FFT_SIZE/2>& data,
                         float x0, float x1, float y0, float y1) const;
};
