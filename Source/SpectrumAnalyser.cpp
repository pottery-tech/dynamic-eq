#include "SpectrumAnalyser.h"
using namespace juce;

SpectrumAnalyser::SpectrumAnalyser()
    : fft(FFT_ORDER), window(FFT_SIZE, dsp::WindowingFunction<float>::hann)
{}

void SpectrumAnalyser::prepare(double sampleRate)
{
    sr = sampleRate;
    fftData.fill(0.f);
    scopeData.fill(0.f);
    frozenData.fill(0.f);
    fifoBuffer.fill(0.f);
    fifoIndex = 0;
    nextFFTBlockReady = false;
    frozen = false;
    frozenDataValid = false;
}

void SpectrumAnalyser::setFrozen(bool shouldFreeze)
{
    if (shouldFreeze && !frozen.load())
        snapshot();   // capture before stopping updates
    frozen = shouldFreeze;
    // Unfreezing: reset fifo so we get fresh data immediately
    if (!shouldFreeze)
    {
        fifoIndex = 0;
        nextFFTBlockReady = false;
    }
}

void SpectrumAnalyser::clearSnapshot()
{
    frozenData.fill(0.f);
    frozenDataValid = false;
    // Also unfreeze
    frozen = false;
    fifoIndex = 0;
    nextFFTBlockReady = false;
}

void SpectrumAnalyser::snapshot()
{
    frozenData = scopeData;
    frozenDataValid = true;
}

void SpectrumAnalyser::pushSamples(const float* L, const float* R, int numSamples)
{
    if (frozen.load()) return;

    for (int i = 0; i < numSamples; ++i)
    {
        fifoBuffer[(size_t)fifoIndex] = (L[i] + R[i]) * 0.5f;
        fifoIndex++;

        if (fifoIndex >= FFT_SIZE)
        {
            // Always overwrite — don't skip if previous block wasn't consumed yet
            std::copy(fifoBuffer.begin(), fifoBuffer.end(), fftData.begin());
            nextFFTBlockReady = true;
            fifoIndex = 0;
        }
    }
}

void SpectrumAnalyser::drawNextFrameOfSpectrum()
{
    // Work on a local copy so fftData isn't clobbered while reading
    std::array<float, FFT_SIZE * 2> localFFT = fftData;

    window.multiplyWithWindowingTable(localFFT.data(), FFT_SIZE);
    fft.performFrequencyOnlyForwardTransform(localFFT.data());

    float smooth = 0.f;
    switch (speed)
    {
        case Speed::Slow:     smooth = 0.94f; break;
        case Speed::Medium:   smooth = 0.82f; break;
        case Speed::Fast:     smooth = 0.60f; break;
        case Speed::VeryFast: smooth = 0.20f; break;
    }

    for (int i = 0; i < FFT_SIZE / 2; ++i)
    {
        float level = jmap(
            Decibels::gainToDecibels(localFFT[(size_t)i])
                - Decibels::gainToDecibels((float)FFT_SIZE),
            -80.f, 6.f, 0.f, 1.f);
        level = jlimit(0.f, 1.f, level);
        // Faster attack, slower release
        float coeff = (level > scopeData[(size_t)i]) ? smooth * 0.4f : smooth;
        scopeData[(size_t)i] = scopeData[(size_t)i] * coeff + level * (1.f - coeff);
    }
}

//==============================================================================
Path SpectrumAnalyser::buildPath(const std::array<float, FFT_SIZE/2>& data,
                                  float x0, float x1, float y0, float y1) const
{
    Path p;
    bool started = false;

    for (int i = 1; i < FFT_SIZE / 2; ++i)
    {
        float binFreq = (float)i * (float)sr / (float)FFT_SIZE;
        if (binFreq < 20.f || binFreq > 20000.f) continue;

        float xNorm = std::log10(binFreq / 20.f) / std::log10(20000.f / 20.f);
        float x = x0 + xNorm * (x1 - x0);
        float y = jlimit(y0, y1, y1 - data[(size_t)i] * (y1 - y0));

        if (!started) { p.startNewSubPath(x, y1); p.lineTo(x, y); started = true; }
        else            p.lineTo(x, y);
    }

    if (started) { p.lineTo(x1, y1); p.closeSubPath(); }
    return p;
}

Path SpectrumAnalyser::getPath(float x0, float x1, float y0, float y1, float, float)
{
    // Process any pending FFT block (even when frozen=false and block just arrived)
    if (!frozen.load() && nextFFTBlockReady.exchange(false))
        drawNextFrameOfSpectrum();

    return buildPath(scopeData, x0, x1, y0, y1);
}

Path SpectrumAnalyser::getFrozenPath(float x0, float x1, float y0, float y1, float, float)
{
    if (!frozenDataValid.load()) return {};
    return buildPath(frozenData, x0, x1, y0, y1);
}
