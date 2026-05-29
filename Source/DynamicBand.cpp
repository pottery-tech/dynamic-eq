#include "DynamicBand.h"
using namespace juce;

void DynamicBand::prepare(double sampleRate, int blockSize)
{
    sr = sampleRate;
    dsp::ProcessSpec spec { sampleRate, (uint32)blockSize, 1 };
    for (int i=0;i<MAX_CASCADE;++i){ filterL[i].prepare(spec); filterL[i].reset(); filterR[i].prepare(spec); filterR[i].reset(); }
    detFilterL.prepare(spec); detFilterL.reset();
    detFilterR.prepare(spec); detFilterR.reset();
    envelope=0.f; scEnvL=0.f; currentGRdB=0.f;
    setAttackMs(10.f); setReleaseMs(100.f);
    coeffsDirty=true;
}

void DynamicBand::reset()
{
    for (int i=0;i<MAX_CASCADE;++i){ filterL[i].reset(); filterR[i].reset(); }
    detFilterL.reset(); detFilterR.reset();
    envelope=0.f; currentGRdB=0.f;
}

void DynamicBand::setAttackMs(float ms)
    { attackCoeff  = ms>0.f ? std::exp(-1.f/(float(sr)*ms*0.001f)) : 0.f; }
void DynamicBand::setReleaseMs(float ms)
    { releaseCoeff = ms>0.f ? std::exp(-1.f/(float(sr)*ms*0.001f)) : 0.f; }

//==============================================================================
void DynamicBand::updateCoeffs()
{
    if (!coeffsDirty) return;
    coeffsDirty = false;

    // Choose cascade count based on slope for HP/LP
    if (filterType==EQFilterType::LowPass || filterType==EQFilterType::HighPass)
    {
        switch(slope){
            case EQSlope::dB6:  cascadeCount=1; break;
            case EQSlope::dB12: cascadeCount=2; break; // 2x 1st-order? use 1 biquad
            case EQSlope::dB24: cascadeCount=2; break;
            case EQSlope::dB48: cascadeCount=4; break;
            default: cascadeCount=1;
        }
    } else { cascadeCount=1; }

    double w0 = 2.0*MathConstants<double>::pi*freq/sr;
    double cosw=std::cos(w0), sinw=std::sin(w0);
    double A = std::pow(10.0, gainDB/40.0);

    // Q per cascaded stage (Butterworth)
    static const float butterworthQ2[] = {0.7071f};
    static const float butterworthQ4[] = {0.5412f, 1.3066f};

    auto makeLP=[&](float stageQ) -> dsp::IIR::Coefficients<float>::Ptr {
        double al = sinw/(2.0*stageQ);
        double b0=( 1-cosw)/2, b1=1-cosw, b2=(1-cosw)/2;
        double a0=1+al, a1=-2*cosw, a2=1-al;
        return new dsp::IIR::Coefficients<float>(b0/a0,b1/a0,b2/a0,1.0,a1/a0,a2/a0);
    };
    auto makeHP=[&](float stageQ) -> dsp::IIR::Coefficients<float>::Ptr {
        double al = sinw/(2.0*stageQ);
        double b0=(1+cosw)/2, b1=-(1+cosw), b2=(1+cosw)/2;
        double a0=1+al, a1=-2*cosw, a2=1-al;
        return new dsp::IIR::Coefficients<float>(b0/a0,b1/a0,b2/a0,1.0,a1/a0,a2/a0);
    };

    // Build per cascade stage
    for (int s=0;s<cascadeCount;++s)
    {
        dsp::IIR::Coefficients<float>::Ptr c;
        float stageQ = bandQ;

        if (filterType==EQFilterType::LowPass){
            if(cascadeCount==2) stageQ=butterworthQ2[0];
            else if(cascadeCount==4) stageQ=butterworthQ4[s<2?s:s-2];
            c=makeLP(stageQ);
        } else if (filterType==EQFilterType::HighPass){
            if(cascadeCount==2) stageQ=butterworthQ2[0];
            else if(cascadeCount==4) stageQ=butterworthQ4[s<2?s:s-2];
            c=makeHP(stageQ);
        } else if (s==0) {
            // Single-stage filters
            double al = sinw/(2.0*bandQ);
            double sq2A = 2.0*std::sqrt(A);
            switch(filterType){
                case EQFilterType::Bell:{
                    double b0=1+al*A, b1=-2*cosw, b2=1-al*A;
                    double a0=1+al/A, a1=-2*cosw, a2=1-al/A;
                    c=new dsp::IIR::Coefficients<float>(b0/a0,b1/a0,b2/a0,1.0,a1/a0,a2/a0);
                } break;
                case EQFilterType::LowShelf:{
                    double sq=sq2A*al;
                    double b0=A*((A+1)-(A-1)*cosw+sq), b1=2*A*((A-1)-(A+1)*cosw), b2=A*((A+1)-(A-1)*cosw-sq);
                    double a0=(A+1)+(A-1)*cosw+sq,     a1=-2*((A-1)+(A+1)*cosw),   a2=(A+1)+(A-1)*cosw-sq;
                    c=new dsp::IIR::Coefficients<float>(b0/a0,b1/a0,b2/a0,1.0,a1/a0,a2/a0);
                } break;
                case EQFilterType::HighShelf:{
                    double sq=sq2A*al;
                    double b0=A*((A+1)+(A-1)*cosw+sq), b1=-2*A*((A-1)+(A+1)*cosw), b2=A*((A+1)+(A-1)*cosw-sq);
                    double a0=(A+1)-(A-1)*cosw+sq,      a1=2*((A-1)-(A+1)*cosw),    a2=(A+1)-(A-1)*cosw-sq;
                    c=new dsp::IIR::Coefficients<float>(b0/a0,b1/a0,b2/a0,1.0,a1/a0,a2/a0);
                } break;
                case EQFilterType::Notch:{
                    double b0=1, b1=-2*cosw, b2=1;
                    double a0=1+al, a1=-2*cosw, a2=1-al;
                    c=new dsp::IIR::Coefficients<float>(b0/a0,b1/a0,b2/a0,1.0,a1/a0,a2/a0);
                } break;
                case EQFilterType::BandPass:{
                    double b0=al, b1=0, b2=-al;
                    double a0=1+al, a1=-2*cosw, a2=1-al;
                    c=new dsp::IIR::Coefficients<float>(b0/a0,b1/a0,b2/a0,1.0,a1/a0,a2/a0);
                } break;
                case EQFilterType::Tilt:{
                    // Low shelf + high shelf combined via tilt: boost lows, cut highs (or vice versa)
                    double tiltGain = gainDB*0.5;
                    double At=std::pow(10.0,tiltGain/40.0);
                    double sq=2.0*std::sqrt(At)*al;
                    double b0=At*((At+1)-(At-1)*cosw+sq), b1=2*At*((At-1)-(At+1)*cosw), b2=At*((At+1)-(At-1)*cosw-sq);
                    double a0=(At+1)+(At-1)*cosw+sq,       a1=-2*((At-1)+(At+1)*cosw),   a2=(At+1)+(At-1)*cosw-sq;
                    c=new dsp::IIR::Coefficients<float>(b0/a0,b1/a0,b2/a0,1.0,a1/a0,a2/a0);
                } break;
                default: c=dsp::IIR::Coefficients<float>::makeAllPass(sr,freq); break;
            }
        } else { continue; }

        *filterL[s].coefficients = *c;
        *filterR[s].coefficients = *c;
    }

    // Detection filter (bell centred on freq)
    auto dc = dsp::IIR::Coefficients<float>::makePeakFilter(sr,freq,jmax(0.5f,bandQ),1.f);
    *detFilterL.coefficients = *dc;
    *detFilterR.coefficients = *dc;
}

//==============================================================================
void DynamicBand::processSidechain(const float* scL, const float* scR, int numSamples)
{
    float sumSq=0.f;
    for(int i=0;i<numSamples;++i){ float s=(scL[i]+scR[i])*0.5f; sumSq+=s*s; }
    float rms=std::sqrt(sumSq/float(numSamples));
    float coeff=(rms>scEnvL)?attackCoeff:releaseCoeff;
    scEnvL=coeff*scEnvL+(1.f-coeff)*rms;
}

float DynamicBand::processBlock(float* L, float* R, int numSamples)
{
    if (!enabled){ currentGRdB=0.f; return 0.f; }
    updateCoeffs();

    // For non-dynamic filters with no gain modulation, just filter
    if (!dynamicEnabled)
    {
        for(int i=0;i<numSamples;++i){
            float l=L[i], r=R[i];
            for(int s=0;s<cascadeCount;++s){ l=filterL[s].processSample(l); r=filterR[s].processSample(r); }
            L[i]=l; R[i]=r;
        }
        currentGRdB=0.f; return 0.f;
    }

    float totalGR=0.f;
    for(int i=0;i<numSamples;++i)
    {
        float inL=L[i], inR=R[i];
        float grDB=0.f;

        if(detMode==DetectionMode::Manual){
            grDB=manualGRdB;
        } else {
            float detL=detFilterL.processSample(inL);
            float detR=detFilterR.processSample(inR);
            float level=0.f;
            if(detMode==DetectionMode::Sidechain){ level=scEnvL; }
            else {
                float raw=std::abs(detL+detR)*0.5f;
                float coeff=(raw>envelope)?attackCoeff:releaseCoeff;
                envelope=coeff*envelope+(1.f-coeff)*raw;
                level=envelope;
            }
            float levelDB=Decibels::gainToDecibels(level,-120.f);
            grDB=computeGain(levelDB)+makeupDB;   // computeGain already returns dB
            grDB=jlimit(-30.f,30.f,grDB);
        }
        totalGR+=grDB;

        // Modulate gain
        float savedGain=gainDB; gainDB+=grDB; coeffsDirty=true; updateCoeffs(); gainDB=savedGain; coeffsDirty=true;
        float l=inL, r=inR;
        for(int s=0;s<cascadeCount;++s){ l=filterL[s].processSample(l); r=filterR[s].processSample(r); }
        L[i]=l; R[i]=r;
    }
    currentGRdB=totalGR/float(numSamples);
    return currentGRdB;
}

float DynamicBand::computeGain(float levelDB) const
{
    // Returns gain change in dB to apply to the EQ band's gain parameter.
    // Positive ratio > 1 = compression (reduces gain when signal exceeds threshold).
    // Returns a negative dB offset when compressing.
    if (ratio <= 1.001f) return 0.f;  // no effect at ratio 1:1

    float halfKnee = kneeDB * 0.5f;
    float overshoot = levelDB - thresholdDB;

    float grDB = 0.f;
    if (overshoot <= -halfKnee)
    {
        grDB = 0.f;  // below threshold — no reduction
    }
    else if (overshoot < halfKnee)
    {
        // Soft knee — blend gradually
        float t = (overshoot + halfKnee) / kneeDB;  // 0..1
        grDB = (1.f/ratio - 1.f) * (overshoot + halfKnee) * t * 0.5f;
    }
    else
    {
        // Above knee — full compression
        grDB = (1.f/ratio - 1.f) * overshoot;
    }
    // grDB is negative for ratio>1 (gain reduction), zero or positive for expansion
    return grDB;
}
