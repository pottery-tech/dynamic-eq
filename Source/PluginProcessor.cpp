#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace juce;

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout DynamicEQProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    for(int b=0;b<MAX_BANDS;++b){
        p.push_back(std::make_unique<AudioParameterBool> (pid(b,"enabled"), "B"+String(b+1)+" Enable",  false));
        p.push_back(std::make_unique<AudioParameterFloat>(pid(b,"freq"),    "B"+String(b+1)+" Freq",
            NormalisableRange<float>(20.f,20000.f,0.1f,0.3f), 1000.f));
        p.push_back(std::make_unique<AudioParameterFloat>(pid(b,"gain"),    "B"+String(b+1)+" Gain",
            NormalisableRange<float>(-30.f,30.f,0.01f), 0.f));
        p.push_back(std::make_unique<AudioParameterFloat>(pid(b,"q"),       "B"+String(b+1)+" Q",
            NormalisableRange<float>(0.1f,10.f,0.01f,0.5f), 0.707f));
        p.push_back(std::make_unique<AudioParameterChoice>(pid(b,"type"),   "B"+String(b+1)+" Type",
            StringArray{"Bell","Low Shelf","High Shelf","Low Pass","High Pass","Notch","Band Pass","Tilt"}, 0));
        p.push_back(std::make_unique<AudioParameterChoice>(pid(b,"slope"),  "B"+String(b+1)+" Slope",
            StringArray{"6 dB/oct","12 dB/oct","24 dB/oct","48 dB/oct"}, 1));
        p.push_back(std::make_unique<AudioParameterBool> (pid(b,"dyn"),     "B"+String(b+1)+" Dynamic", false));
        p.push_back(std::make_unique<AudioParameterFloat>(pid(b,"thresh"),  "B"+String(b+1)+" Thresh",
            NormalisableRange<float>(-60.f,0.f,0.1f), -20.f));
        p.push_back(std::make_unique<AudioParameterFloat>(pid(b,"ratio"),   "B"+String(b+1)+" Ratio",
            NormalisableRange<float>(1.f,20.f,0.1f,0.4f), 4.f));
        p.push_back(std::make_unique<AudioParameterFloat>(pid(b,"attack"),  "B"+String(b+1)+" Attack",
            NormalisableRange<float>(0.1f,200.f,0.1f,0.5f), 10.f));
        p.push_back(std::make_unique<AudioParameterFloat>(pid(b,"release"), "B"+String(b+1)+" Release",
            NormalisableRange<float>(1.f,2000.f,1.f,0.4f), 100.f));
        p.push_back(std::make_unique<AudioParameterChoice>(pid(b,"detmode"),"B"+String(b+1)+" DetMode",
            StringArray{"Internal","Sidechain","Manual"}, 0));
        // M/S routing per band: 0=Stereo, 1=Mid only, 2=Side only
        p.push_back(std::make_unique<AudioParameterChoice>(pid(b,"msroute"),"B"+String(b+1)+" M/S",
            StringArray{"Stereo","Mid","Side"}, 0));
    }

    p.push_back(std::make_unique<AudioParameterFloat>("output_gain","Output Gain",
        NormalisableRange<float>(-18.f,18.f,0.1f),0.f));
    p.push_back(std::make_unique<AudioParameterChoice>("proc_mode","Processing Mode",
        StringArray{"Stereo","Mid/Side"},0));
    p.push_back(std::make_unique<AudioParameterChoice>("phase_mode","Phase Mode",
        StringArray{"Minimum Phase","Linear Phase"},0));

    return {p.begin(),p.end()};
}

//==============================================================================
DynamicEQProcessor::DynamicEQProcessor()
    : AudioProcessor(BusesProperties()
        .withInput ("Input",     AudioChannelSet::stereo(),true)
        .withInput ("Sidechain", AudioChannelSet::stereo(),true)
        .withOutput("Output",    AudioChannelSet::stereo(),true)),
      apvts(*this, &undoManager, "PARAMS", createParameterLayout())
{}

bool DynamicEQProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    return l.getMainOutputChannelSet()==AudioChannelSet::stereo()
        && l.getMainInputChannelSet() ==AudioChannelSet::stereo();
}

double DynamicEQProcessor::getTailLengthSeconds() const
{
    return phaseMode==PhaseMode::Linear ? (double)LP_FIR_SIZE/currentSR : 0.0;
}

void DynamicEQProcessor::releaseResources()
{
    lpConvL.reset(); lpConvR.reset(); lpConvReady=false;
}

//==============================================================================
void DynamicEQProcessor::setProcessingMode(ProcessingMode m) { processingMode=m; }
void DynamicEQProcessor::setPhaseMode(PhaseMode m)
{
    phaseMode=m;
    if(m==PhaseMode::Linear && currentSR>0) rebuildLinearPhaseIR();
    else { lpConvL.reset(); lpConvR.reset(); lpConvReady=false; }
}

void DynamicEQProcessor::rebuildLinearPhaseIR()
{
    // Build combined FIR from all enabled bands using frequency sampling
    const int N = LP_FIR_SIZE;
    std::vector<float> H(N/2+1, 1.f);

    for(int b=0;b<bandCount;++b){
        bool en=*apvts.getRawParameterValue(pid(b,"enabled"))>0.5f;
        if(!en) continue;
        float freq=*apvts.getRawParameterValue(pid(b,"freq"));
        float gain=*apvts.getRawParameterValue(pid(b,"gain"));
        float q   =*apvts.getRawParameterValue(pid(b,"q"));
        int   ti  =(int)*apvts.getRawParameterValue(pid(b,"type"));
        int   si  =(int)*apvts.getRawParameterValue(pid(b,"slope"));

        for(int k=0;k<=N/2;++k){
            float hz=(float)k*(float)currentSR/(float)N;
            if(hz<1.f) hz=1.f;
            // Reuse BandResponse magnitude (we include PluginEditor.h for it, or recompute inline)
            // Inline simplified bell for now; full types handled via cascade
            double w0=2*MathConstants<double>::pi*freq/currentSR;
            double wt=2*MathConstants<double>::pi*hz/currentSR;
            double A=std::pow(10.0,gain/40.0);
            double cw0=std::cos(w0),sw0=std::sin(w0);
            double al=sw0/(2.0*q);
            double b0,b1,b2,a0,a1,a2;
            // Bell for LP IR (simplified; full types would need same math as BandResponse)
            if(ti==0){ b0=1+al*A;b1=-2*cw0;b2=1-al*A;a0=1+al/A;a1=-2*cw0;a2=1-al/A; }
            else      { b0=1;b1=0;b2=0;a0=1;a1=0;a2=0; } // passthrough for other types in LP mode
            double c1=std::cos(wt),s1=std::sin(wt),c2=std::cos(2*wt),s2=std::sin(2*wt);
            double nr=b0+b1*c1+b2*c2,ni=b1*s1+b2*s2,dr=a0+a1*c1+a2*c2,di=a1*s1+a2*s2;
            H[(size_t)k]*=(float)std::sqrt((nr*nr+ni*ni)/(dr*dr+di*di));
        }
    }

    // IFFT H → time domain FIR (symmetric, zero-phase)
    std::vector<float> fir(N, 0.f);
    // Simple DFT-based synthesis (N is small enough)
    for(int n=0;n<N;++n){
        double val=H[0];
        for(int k=1;k<N/2;++k)
            val+=2.0*H[(size_t)k]*std::cos(2*MathConstants<double>::pi*k*n/N);
        val+=H[N/2]*std::cos(MathConstants<double>::pi*n);
        fir[(size_t)n]=(float)(val/N);
    }
    // Circularly shift to make causal (linear phase with N/2 delay)
    std::vector<float> causal(N);
    for(int n=0;n<N;++n) causal[(size_t)n]=fir[(size_t)((n+N/2)%N)];
    // Apply Hann window
    for(int n=0;n<N;++n)
        causal[(size_t)n]*=0.5f*(1.f-std::cos(2.f*MathConstants<float>::pi*(float)n/(float)(N-1)));

    // Load into convolution engines
    AudioBuffer<float> irBuf(1,N);
    irBuf.copyFrom(0,0,causal.data(),N);
    lpConvL.loadImpulseResponse(std::move(irBuf),currentSR,
        dsp::Convolution::Stereo::no, dsp::Convolution::Trim::no, dsp::Convolution::Normalise::no);
    AudioBuffer<float> irBuf2(1,N);
    irBuf2.copyFrom(0,0,causal.data(),N);
    lpConvR.loadImpulseResponse(std::move(irBuf2),currentSR,
        dsp::Convolution::Stereo::no, dsp::Convolution::Trim::no, dsp::Convolution::Normalise::no);
    lpConvReady=true;
}

//==============================================================================
void DynamicEQProcessor::prepareToPlay(double sr, int bs)
{
    currentSR=sr; currentBS=bs;
    for(auto& b:bands) b.prepare(sr,bs);
    analyser.prepare(sr); inputAnalyser.prepare(sr);
    scAnalyser.prepare(sr); midAnalyser.prepare(sr); sideAnalyser.prepare(sr);

    dsp::ProcessSpec spec{sr,(uint32)bs,1};
    lpConvL.prepare(spec); lpConvR.prepare(spec);
    lpConvReady=false;

    if((int)*apvts.getRawParameterValue("phase_mode")==1)
        rebuildLinearPhaseIR();
}

//==============================================================================
int DynamicEQProcessor::addBand(float freqHz, EQFilterType type)
{
    if(bandCount>=MAX_BANDS) return -1;
    int idx=bandCount++;
    bands[idx].reset();
    bands[idx].prepare(currentSR,currentBS);
    bands[idx].setEnabled(true);

    // enabled
    if(auto* p=apvts.getParameter(pid(idx,"enabled")))
        p->setValueNotifyingHost(1.f);
    // freq
    if(auto* p=apvts.getParameter(pid(idx,"freq")))
        p->setValueNotifyingHost(p->convertTo0to1(freqHz));
    // type — ChoiceParameter: normalized = index / (numChoices-1)
    if(auto* p=dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(pid(idx,"type"))))
        p->setValueNotifyingHost((float)(int)type / (float)(p->choices.size()-1));
    // gain
    if(auto* p=apvts.getParameter(pid(idx,"gain")))
        p->setValueNotifyingHost(p->convertTo0to1(0.f));
    // q
    if(auto* p=apvts.getParameter(pid(idx,"q")))
        p->setValueNotifyingHost(p->convertTo0to1(0.707f));
    // slope default 12dB for HP/LP
    if(type==EQFilterType::HighPass || type==EQFilterType::LowPass)
        if(auto* p=dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(pid(idx,"slope"))))
            p->setValueNotifyingHost(1.f / (float)(p->choices.size()-1)); // index 1 = 12dB/oct

    return idx;
}

void DynamicEQProcessor::removeBand(int index)
{
    if(index<0||index>=bandCount) return;
    const char* params[]={"enabled","freq","gain","q","type","slope","dyn",
                          "thresh","ratio","attack","release","detmode","msroute"};
    for(int i=index;i<bandCount-1;++i)
        for(auto* pn:params){
            float v=*apvts.getRawParameterValue(pid(i+1,pn));
            if(auto* p=apvts.getParameter(pid(i,pn))) p->setValueNotifyingHost(p->convertTo0to1(v));
        }
    bandCount--;
    if(auto* p=apvts.getParameter(pid(bandCount,"enabled"))) p->setValueNotifyingHost(0.f);
}

//==============================================================================
void DynamicEQProcessor::syncBandParams(int b)
{
    bool en=*apvts.getRawParameterValue(pid(b,"enabled"))>0.5f;
    bands[b].setEnabled(en);
    if(!en) return;

    bands[b].setFrequency(*apvts.getRawParameterValue(pid(b,"freq")));
    bands[b].setGain     (*apvts.getRawParameterValue(pid(b,"gain")));
    bands[b].setQ        (*apvts.getRawParameterValue(pid(b,"q")));
    bands[b].setSlope    ((EQSlope)(int)*apvts.getRawParameterValue(pid(b,"slope")));
    bands[b].setFilterType((EQFilterType)(int)*apvts.getRawParameterValue(pid(b,"type")));

    bool dyn=*apvts.getRawParameterValue(pid(b,"dyn"))>0.5f;
    bands[b].setDynamicEnabled(dyn);
    bands[b].setThreshold(*apvts.getRawParameterValue(pid(b,"thresh")));
    bands[b].setRatio    (*apvts.getRawParameterValue(pid(b,"ratio")));
    bands[b].setAttack   (*apvts.getRawParameterValue(pid(b,"attack")));
    bands[b].setRelease  (*apvts.getRawParameterValue(pid(b,"release")));
    bands[b].setDetectionMode((DetectionMode)(int)*apvts.getRawParameterValue(pid(b,"detmode")));
}

//==============================================================================
inline void DynamicEQProcessor::encodeMS(float& mid, float& side, float L, float R)
    { mid=(L+R)*0.5f; side=(L-R)*0.5f; }
inline void DynamicEQProcessor::decodeMS(float& L, float& R, float mid, float side)
    { L=mid+side; R=mid-side; }

//==============================================================================
void DynamicEQProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals ndn;
    int ns=buffer.getNumSamples();
    float* L=buffer.getWritePointer(0);
    float* R=buffer.getWritePointer(1);

    // Sidechain
    const float* scL=L, *scR=R;
    if(auto* scBus=getBus(true,SC_BUS); scBus&&scBus->isEnabled()){
        int ch=getChannelIndexInProcessBlockBuffer(true,SC_BUS,0);
        scL=buffer.getReadPointer(ch);
        scR=buffer.getReadPointer(jmin(ch+1,buffer.getNumChannels()-1));
    }
    scAnalyser.pushSamples(scL,scR,ns);

    // Capture input BEFORE any EQ processing
    inputAnalyser.pushSamples(L,R,ns);

    // Read mode params
    bool isMidSide = (int)*apvts.getRawParameterValue("proc_mode")==1;
    bool isLinear  = (int)*apvts.getRawParameterValue("phase_mode")==1;

    if(isMidSide!=( processingMode==ProcessingMode::MidSide)) setProcessingMode(isMidSide?ProcessingMode::MidSide:ProcessingMode::Stereo);
    if(isLinear !=(phaseMode==PhaseMode::Linear))             setPhaseMode(isLinear?PhaseMode::Linear:PhaseMode::Minimum);

    // Build per-sample mid/side buffers if needed
    std::vector<float> midBuf(ns), sideBuf(ns);
    if(isMidSide){
        for(int i=0;i<ns;++i) encodeMS(midBuf[(size_t)i],sideBuf[(size_t)i],L[i],R[i]);
    }

    // Process each band
    for(int b=0;b<MAX_BANDS;++b){
        syncBandParams(b);
        bands[b].processSidechain(scL,scR,ns);

        if(!bands[b].isEnabled()) continue;

        int msRoute = isMidSide ? (int)*apvts.getRawParameterValue(pid(b,"msroute")) : 0;

        if(!isMidSide || msRoute==0)
        {
            // Stereo mode or stereo routing in M/S mode
            if(isMidSide){
                // apply to both mid and side
                std::vector<float> tmpMid=midBuf, tmpSide=sideBuf;
                bands[b].processBlock(tmpMid.data(),tmpSide.data(),ns);
                midBuf=tmpMid; sideBuf=tmpSide;
            } else {
                bands[b].processBlock(L,R,ns);
            }
        }
        else if(msRoute==1) // Mid only
        {
            std::vector<float> dummy(ns,0.f);
            std::vector<float> tmp=midBuf;
            bands[b].processBlock(tmp.data(),dummy.data(),ns);
            midBuf=tmp;
        }
        else // Side only
        {
            std::vector<float> dummy(ns,0.f);
            std::vector<float> tmp=sideBuf;
            bands[b].processBlock(tmp.data(),dummy.data(),ns);
            sideBuf=tmp;
        }
    }

    // Decode M/S back to L/R
    if(isMidSide){
        for(int i=0;i<ns;++i) decodeMS(L[i],R[i],midBuf[(size_t)i],sideBuf[(size_t)i]);
    }

    // Linear phase convolution (replaces IIR result)
    if(isLinear && lpConvReady){
        dsp::AudioBlock<float> blockL(buffer.getArrayOfWritePointers(), 1, (size_t)ns);
        dsp::AudioBlock<float> blockR(buffer.getArrayOfWritePointers()+1, 1, (size_t)ns);
        dsp::ProcessContextReplacing<float> ctxL(blockL), ctxR(blockR);
        lpConvL.process(ctxL); lpConvR.process(ctxR);
    }

    // Output gain
    buffer.applyGain(Decibels::decibelsToGain((float)*apvts.getRawParameterValue("output_gain")));

    // Feed analysers
    if(isMidSide){
        midAnalyser.pushSamples(midBuf.data(),midBuf.data(),ns);
        sideAnalyser.pushSamples(sideBuf.data(),sideBuf.data(),ns);
    }
    analyser.pushSamples(L,R,ns);
}

//==============================================================================
void DynamicEQProcessor::getStateInformation(MemoryBlock& dest)
{
    auto state=apvts.copyState();
    state.setProperty("bandCount",bandCount,nullptr);
    state.setProperty("procMode",(int)processingMode,nullptr);
    state.setProperty("phaseMode",(int)phaseMode,nullptr);
    copyXmlToBinary(*state.createXml(),dest);
}

void DynamicEQProcessor::setStateInformation(const void* data, int size)
{
    auto xml=getXmlFromBinary(data,size);
    if(xml&&xml->hasTagName(apvts.state.getType())){
        auto vt=ValueTree::fromXml(*xml);
        bandCount=jlimit(0,MAX_BANDS,(int)vt.getProperty("bandCount",0));
        apvts.replaceState(vt);
    }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DynamicEQProcessor(); }
