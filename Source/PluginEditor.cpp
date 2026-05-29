#include "PluginEditor.h"
#include <cmath>
using namespace juce;

//==============================================================================
// BandResponse
//==============================================================================
float BandResponse::magnitude(int ti, float freq, float gdB, float q,
                               int slopeIdx, float hz, double sr)
{
    if (std::abs(gdB) < 0.001f &&
        ti != (int)EQFilterType::LowPass && ti != (int)EQFilterType::HighPass &&
        ti != (int)EQFilterType::Notch   && ti != (int)EQFilterType::BandPass) return 1.f;

    // cascade count
    int casc=1;
    if(ti==(int)EQFilterType::LowPass||ti==(int)EQFilterType::HighPass){
        switch(slopeIdx){ case 0:casc=1;break; case 1:casc=2;break; case 2:casc=2;break; case 3:casc=4;break; }
    }

    static const float bwQ2[]={0.7071f};
    static const float bwQ4[]={0.5412f,1.3066f,0.5412f,1.3066f};

    auto biquadMag=[&](double b0,double b1,double b2,double a0,double a1,double a2)->float{
        double wt=2*MathConstants<double>::pi*hz/sr;
        double c1=std::cos(wt),s1=std::sin(wt),c2=std::cos(2*wt),s2=std::sin(2*wt);
        double nr=b0+b1*c1+b2*c2, ni=b1*s1+b2*s2;
        double dr=a0+a1*c1+a2*c2, di=a1*s1+a2*s2;
        return (float)std::sqrt((nr*nr+ni*ni)/(dr*dr+di*di));
    };

    float totalMag=1.f;
    for(int s=0;s<casc;++s){
        double w0=2*MathConstants<double>::pi*freq/sr;
        double cosw=std::cos(w0),sinw=std::sin(w0);
        double A=std::pow(10.0,gdB/40.0);
        float stageQ=q;
        if(casc==2) stageQ=bwQ2[0];
        else if(casc==4) stageQ=bwQ4[s];
        double al=sinw/(2.0*stageQ);

        double b0,b1,b2,a0,a1,a2;
        if(ti==(int)EQFilterType::LowPass){
            b0=(1-cosw)/2; b1=1-cosw; b2=(1-cosw)/2;
            a0=1+al; a1=-2*cosw; a2=1-al;
        } else if(ti==(int)EQFilterType::HighPass){
            b0=(1+cosw)/2; b1=-(1+cosw); b2=(1+cosw)/2;
            a0=1+al; a1=-2*cosw; a2=1-al;
        } else if(ti==(int)EQFilterType::Notch){
            b0=1; b1=-2*cosw; b2=1;
            a0=1+al; a1=-2*cosw; a2=1-al;
        } else if(ti==(int)EQFilterType::BandPass){
            b0=al; b1=0; b2=-al;
            a0=1+al; a1=-2*cosw; a2=1-al;
        } else if(ti==(int)EQFilterType::Bell){
            b0=1+al*A; b1=-2*cosw; b2=1-al*A;
            a0=1+al/A; a1=-2*cosw; a2=1-al/A;
        } else if(ti==(int)EQFilterType::LowShelf){
            double sq=2*std::sqrt(A)*al;
            b0=A*((A+1)-(A-1)*cosw+sq); b1=2*A*((A-1)-(A+1)*cosw); b2=A*((A+1)-(A-1)*cosw-sq);
            a0=(A+1)+(A-1)*cosw+sq; a1=-2*((A-1)+(A+1)*cosw); a2=(A+1)+(A-1)*cosw-sq;
        } else if(ti==(int)EQFilterType::HighShelf){
            double sq=2*std::sqrt(A)*al;
            b0=A*((A+1)+(A-1)*cosw+sq); b1=-2*A*((A-1)+(A+1)*cosw); b2=A*((A+1)+(A-1)*cosw-sq);
            a0=(A+1)-(A-1)*cosw+sq; a1=2*((A-1)-(A+1)*cosw); a2=(A+1)-(A-1)*cosw-sq;
        } else { // Tilt
            double At=std::pow(10.0,(gdB*0.5)/40.0);
            double sq=2*std::sqrt(At)*al;
            b0=At*((At+1)-(At-1)*cosw+sq); b1=2*At*((At-1)-(At+1)*cosw); b2=At*((At+1)-(At-1)*cosw-sq);
            a0=(At+1)+(At-1)*cosw+sq; a1=-2*((At-1)+(At+1)*cosw); a2=(At+1)+(At-1)*cosw-sq;
        }
        totalMag *= biquadMag(b0,b1,b2,a0,a1,a2);
    }
    return totalMag;
}

//==============================================================================
// EQDisplay
//==============================================================================
EQDisplay::EQDisplay(DynamicEQProcessor& p) : proc(p)
{
    setMouseCursor(MouseCursor::CrosshairCursor);
    startTimerHz(30);
}

float EQDisplay::freqToX(float hz) const
    { return std::log10(hz/MIN_HZ)/std::log10(MAX_HZ/MIN_HZ)*(float)getWidth(); }
float EQDisplay::gainToY(float dB) const
    { return (MAX_DB-dB)/(MAX_DB-MIN_DB)*(float)getHeight(); }
float EQDisplay::xToFreq(float x) const
    { return MIN_HZ*std::pow(MAX_HZ/MIN_HZ,jlimit(0.f,1.f,x/(float)getWidth())); }
float EQDisplay::yToGain(float y) const
    { return MAX_DB-jlimit(0.f,1.f,y/(float)getHeight())*(MAX_DB-MIN_DB); }

int EQDisplay::bandAtPoint(Point<float> p, float r) const
{
    int best=-1; float bestD=r;
    for(int b=0;b<proc.getBandCount();++b){
        bool en=*proc.apvts.getRawParameterValue(proc.pid(b,"enabled"))>0.5f;
        if(!en) continue;
        float freq=*proc.apvts.getRawParameterValue(proc.pid(b,"freq"));
        float gain=*proc.apvts.getRawParameterValue(proc.pid(b,"gain"));
        float d=p.getDistanceFrom({freqToX(freq),gainToY(gain)});
        if(d<bestD){bestD=d;best=b;}
    }
    return best;
}

void EQDisplay::mouseDown(const MouseEvent& e)
{
    if (e.mods.isRightButtonDown()){ int b=bandAtPoint(e.position,16.f); if(b>=0) showBandContextMenu(b,e.getScreenPosition()); return; }
    dragBand=bandAtPoint(e.position,16.f);
    if(dragBand>=0){ selectedBand=dragBand; if(onBandSelected) onBandSelected(selectedBand); }
    else {
        // double-click handled separately; single click deselects
    }
    repaint();
}
void EQDisplay::mouseDrag(const MouseEvent& e)
{
    if(dragBand<0) return;
    float newFreq=jlimit(20.f,20000.f,xToFreq(e.position.x));
    float newGain=jlimit(MIN_DB,MAX_DB,yToGain(e.position.y));
    if(auto* p=proc.apvts.getParameter(proc.pid(dragBand,"freq")))
        p->setValueNotifyingHost(p->convertTo0to1(newFreq));
    if(auto* p=proc.apvts.getParameter(proc.pid(dragBand,"gain")))
        p->setValueNotifyingHost(p->convertTo0to1(newGain));
}
void EQDisplay::mouseUp(const MouseEvent&){ dragBand=-1; }
void EQDisplay::mouseMove(const MouseEvent& e)
{
    int prev=hoveredBand; hoveredBand=bandAtPoint(e.position,16.f);
    if(hoveredBand!=prev) repaint();
    setMouseCursor(hoveredBand>=0?MouseCursor::UpDownLeftRightResizeCursor:MouseCursor::CrosshairCursor);
}
void EQDisplay::mouseDoubleClick(const MouseEvent& e)
{
    int b=bandAtPoint(e.position,16.f);
    if(b<0)
    {
        float hz      = xToFreq(e.position.x);
        float gainVal = yToGain(e.position.y);

        bool inCutRegion   = (gainVal < -6.f);
        bool inBoostRegion = (gainVal >  6.f);
        bool nearZero      = !inCutRegion && !inBoostRegion;

        EQFilterType type = EQFilterType::Bell;
        float initialGain = gainVal;

        if (nearZero)
        {
            type = EQFilterType::Bell;
        }
        else if (hz < 120.f && inCutRegion)
        {
            type        = EQFilterType::HighPass;
            initialGain = 0.f;
        }
        else if (hz > 8000.f && inCutRegion)
        {
            type        = EQFilterType::LowPass;
            initialGain = 0.f;
        }
        else if (hz < 300.f)
        {
            type = EQFilterType::LowShelf;
        }
        else if (hz > 3000.f)
        {
            type = EQFilterType::HighShelf;
        }

        // addBand sets type + slope correctly; just fix gain afterwards
        int idx = proc.addBand(hz, type);
        if(idx >= 0)
        {
            if(initialGain != 0.f)
                if(auto* p=proc.apvts.getParameter(proc.pid(idx,"gain")))
                    p->setValueNotifyingHost(p->convertTo0to1(initialGain));

            // Fire selection AFTER all params are written
            if(onBandSelected) onBandSelected(idx);
        }
    }
    else
    {
        // Double-click existing band — reset gain
        if(auto* p=proc.apvts.getParameter(proc.pid(b,"gain")))
            p->setValueNotifyingHost(p->convertTo0to1(0.f));
    }
    repaint();
}
void EQDisplay::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& w)
{
    int b=bandAtPoint(e.position,16.f); if(b<0) b=selectedBand; if(b<0) return;

    int typeIdx=(int)*proc.apvts.getRawParameterValue(proc.pid(b,"type"));
    bool isHPLP = (typeIdx==(int)EQFilterType::HighPass ||
                   typeIdx==(int)EQFilterType::LowPass);

    if(isHPLP)
    {
        // Scroll steps slope up/down: 6 → 12 → 24 → 48 dB/oct
        auto* p=dynamic_cast<juce::AudioParameterChoice*>(
                    proc.apvts.getParameter(proc.pid(b,"slope")));
        if(!p) return;
        int cur   = p->getIndex();
        int next  = jlimit(0, p->choices.size()-1, cur + (w.deltaY > 0 ? 1 : -1));
        p->setValueNotifyingHost((float)next / (float)(p->choices.size()-1));
    }
    else
    {
        float q=*proc.apvts.getRawParameterValue(proc.pid(b,"q"));
        q=jlimit(0.1f,10.f,q*(w.deltaY>0?1.15f:1.f/1.15f));
        if(auto* p=proc.apvts.getParameter(proc.pid(b,"q")))
            p->setValueNotifyingHost(p->convertTo0to1(q));
    }
    repaint();
}

void EQDisplay::showBandContextMenu(int b, Point<int> screenPos)
{
    PopupMenu menu;
    PopupMenu typeMenu;
    int curType=(int)*proc.apvts.getRawParameterValue(proc.pid(b,"type"));
    StringArray typeNames={"Bell","Low Shelf","High Shelf","Low Pass","High Pass","Notch","Band Pass","Tilt"};
    for(int i=0;i<typeNames.size();++i)
        typeMenu.addItem(100+i, typeNames[i], true, i==curType);

    PopupMenu slopeMenu;
    int curSlope=(int)*proc.apvts.getRawParameterValue(proc.pid(b,"slope"));
    StringArray slopeNames={"6 dB/oct","12 dB/oct","24 dB/oct","48 dB/oct"};
    for(int i=0;i<slopeNames.size();++i)
        slopeMenu.addItem(200+i, slopeNames[i], true, i==curSlope);

    bool dynOn=*proc.apvts.getRawParameterValue(proc.pid(b,"dyn"))>0.5f;

    menu.addSubMenu("Filter Type", typeMenu);
    menu.addSubMenu("Slope (HP/LP)", slopeMenu);
    menu.addSeparator();
    menu.addItem(300, dynOn?"Disable Dynamic EQ":"Enable Dynamic EQ");
    menu.addItem(301, "Reset Gain to 0 dB");
    menu.addSeparator();
    menu.addItem(302, "Delete Band", true, false);

    menu.showMenuAsync(PopupMenu::Options().withTargetScreenArea({screenPos.x,screenPos.y,1,1}),
        [this,b](int r){
            if(r>=100&&r<108){
                if(auto* p=proc.apvts.getParameter(proc.pid(b,"type")))
                    p->setValueNotifyingHost(p->convertTo0to1((float)(r-100)));
            } else if(r>=200&&r<204){
                if(auto* p=proc.apvts.getParameter(proc.pid(b,"slope")))
                    p->setValueNotifyingHost(p->convertTo0to1((float)(r-200)));
            } else if(r==300){
                bool cur=*proc.apvts.getRawParameterValue(proc.pid(b,"dyn"))>0.5f;
                if(auto* p=proc.apvts.getParameter(proc.pid(b,"dyn")))
                    p->setValueNotifyingHost(cur?0.f:1.f);
            } else if(r==301){
                if(auto* p=proc.apvts.getParameter(proc.pid(b,"gain")))
                    p->setValueNotifyingHost(p->convertTo0to1(0.f));
            } else if(r==302){
                int removed=b;
                proc.removeBand(b);
                if(onBandRemoved) onBandRemoved(removed);
                selectedBand=jmin(selectedBand,proc.getBandCount()-1);
                repaint();
            }
        });
}

//==============================================================================
void EQDisplay::drawGrid(Graphics& g, float W, float H)
{
    // dB lines
    for(float dB:{-24.f,-18.f,-12.f,-6.f,0.f,6.f,12.f,18.f,24.f}){
        float y=gainToY(dB);
        g.setColour(dB==0.f?EQColours::Grid.brighter(0.3f):EQColours::Grid);
        g.drawHorizontalLine((int)y,0.f,W);
        if(std::abs(dB)>0.f){
            g.setColour(EQColours::GridText);
            g.setFont(Font(9.f));
            String s=(dB>0?"+":"")+String((int)dB)+"dB";
            g.drawText(s,4,(int)(y-10.f),40,12,Justification::centredLeft);
        }
    }
    // Frequency lines + labels
    float freqs[]={20,50,100,200,500,1000,2000,5000,10000,20000};
    for(float hz:freqs){
        float x=freqToX(hz);
        g.setColour(EQColours::Grid);
        g.drawVerticalLine((int)x,0.f,H);
        g.setColour(EQColours::GridText);
        g.setFont(Font(9.f));
        String lbl=hz>=1000.f?String((int)(hz/1000.f))+"k":String((int)hz);
        g.drawText(lbl,(int)x-16,(int)H-14,32,12,Justification::centred);
    }
}

void EQDisplay::drawSpectraBG(Graphics& g, float W, float H)
{
    // Background spectra — drawn BEHIND the EQ silhouette
    bool isMidSide=(int)*proc.apvts.getRawParameterValue("proc_mode")==1;

    if(isMidSide){
        auto midPath=proc.getMidAnalyser().getPath(0.f,W,0.f,H);
        g.setColour(Colour(0xffcc88ff).withAlpha(0.10f)); g.fillPath(midPath);
        g.setColour(Colour(0xffcc88ff).withAlpha(0.40f)); g.strokePath(midPath,PathStrokeType(1.f));

        auto sidePath=proc.getSideAnalyser().getPath(0.f,W,0.f,H);
        g.setColour(Colour(0xff88ccff).withAlpha(0.10f)); g.fillPath(sidePath);
        g.setColour(Colour(0xff88ccff).withAlpha(0.40f)); g.strokePath(sidePath,PathStrokeType(1.f));
    } else {
        // Main spectrum fill only — line drawn in FG
        auto sp=proc.getAnalyser().getPath(0.f,W,0.f,H);
        g.setColour(EQColours::Spectrum.withAlpha(0.08f)); g.fillPath(sp);
    }
}

void EQDisplay::drawSpectraFG(Graphics& g, float W, float H)
{
    // Foreground spectra — drawn ON TOP of the EQ silhouette
    bool isMidSide=(int)*proc.apvts.getRawParameterValue("proc_mode")==1;

    if(!isMidSide){
        // Main spectrum line
        auto sp=proc.getAnalyser().getPath(0.f,W,0.f,H);
        g.setColour(EQColours::Spectrum.withAlpha(0.5f)); g.strokePath(sp,PathStrokeType(1.f));

        // SC line — always on top, high visibility
        auto scPath=proc.getScAnalyser().getPath(0.f,W,0.f,H);
        g.setColour(EQColours::SCSpect.withAlpha(0.75f));
        g.strokePath(scPath, PathStrokeType(1.5f));
    }

    // Frozen snapshot always topmost
    auto& an=proc.getAnalyser();
    if(an.hasFrozenData()){
        auto fp=an.getFrozenPath(0.f,W,0.f,H);
        g.setColour(EQColours::Frozen.withAlpha(0.12f)); g.fillPath(fp);
        g.setColour(EQColours::Frozen.withAlpha(0.6f));
        g.strokePath(fp,PathStrokeType(1.5f,PathStrokeType::curved));
    }
}

void EQDisplay::drawEQCurves(Graphics& g, float W, float H)
{
    // Per-band individual curves (dim)
    for(int b=0;b<proc.getBandCount();++b){
        bool en=*proc.apvts.getRawParameterValue(proc.pid(b,"enabled"))>0.5f;
        if(!en) continue;
        float freq=*proc.apvts.getRawParameterValue(proc.pid(b,"freq"));
        float gain=*proc.apvts.getRawParameterValue(proc.pid(b,"gain"));
        float q   =*proc.apvts.getRawParameterValue(proc.pid(b,"q"));
        int   ti  =(int)*proc.apvts.getRawParameterValue(proc.pid(b,"type"));
        int   si  =(int)*proc.apvts.getRawParameterValue(proc.pid(b,"slope"));

        Path curve; bool started=false;
        for(int px=0;px<(int)W;px+=2){
            float hz=xToFreq((float)px);
            float mag=BandResponse::magnitude(ti,freq,gain,q,si,hz,44100.0);
            float y=jlimit(0.f,H,gainToY(Decibels::gainToDecibels(mag,-60.f)));
            if(!started){curve.startNewSubPath((float)px,y);started=true;}
            else curve.lineTo((float)px,y);
        }
        bool sel=(b==selectedBand);
        g.setColour(EQColours::bandColour(b).withAlpha(sel?0.4f:0.15f));
        g.strokePath(curve,PathStrokeType(sel?1.5f:1.f));
    }

    // Combined curve + silhouette
    {
        // Build combined magnitude array
        std::vector<float> magDB((int)W/2+1);
        std::vector<float> yPos ((int)W/2+1);
        float zeroY = gainToY(0.f);

        for(int px=0; px<(int)W; px+=2){
            int i=px/2;
            float mag=1.f;
            for(int b=0;b<proc.getBandCount();++b){
                bool en=*proc.apvts.getRawParameterValue(proc.pid(b,"enabled"))>0.5f;
                if(!en) continue;
                float freq=*proc.apvts.getRawParameterValue(proc.pid(b,"freq"));
                float gain=*proc.apvts.getRawParameterValue(proc.pid(b,"gain"));
                float q   =*proc.apvts.getRawParameterValue(proc.pid(b,"q"));
                int   ti  =(int)*proc.apvts.getRawParameterValue(proc.pid(b,"type"));
                int   si  =(int)*proc.apvts.getRawParameterValue(proc.pid(b,"slope"));
                mag*=BandResponse::magnitude(ti,freq,gain,q,si,xToFreq((float)px),44100.0);
            }
            magDB[(size_t)i] = Decibels::gainToDecibels(mag, -60.f);
            yPos [(size_t)i] = jlimit(0.f, H, gainToY(magDB[(size_t)i]));
        }

        int N = (int)W/2;

        // ── Silhouette: cut regions (below 0dB line) filled red ────────────
        // ── Silhouette: boost regions (above 0dB line) filled green ─────────
        // We scan for contiguous cut/boost segments and fill each as a polygon
        auto fillSegment = [&](int startI, int endI, bool isCut)
        {
            if(startI >= endI) return;
            Path seg;
            seg.startNewSubPath((float)(startI*2), zeroY);
            for(int i=startI; i<=endI; ++i)
                seg.lineTo((float)(i*2), yPos[(size_t)i]);
            seg.lineTo((float)(endI*2), zeroY);
            seg.closeSubPath();

            Colour col = isCut
                ? Colour(0xffff3333).withAlpha(0.18f)   // red for cuts
                : Colour(0xff33ff88).withAlpha(0.15f);  // green for boosts
            g.setColour(col);
            g.fillPath(seg);
        };

        int segStart = 0;
        bool inCut = (magDB[0] < -0.1f);

        for(int i=1; i<N; ++i)
        {
            bool nowCut = (magDB[(size_t)i] < -0.1f);
            bool nowBoost = (magDB[(size_t)i] > 0.1f);
            bool wasCut   = (magDB[(size_t)(i-1)] < -0.1f);
            bool wasBoost = (magDB[(size_t)(i-1)] > 0.1f);

            // Crossed zero line — close previous segment, start new one
            if((wasCut && !nowCut) || (wasBoost && !nowBoost) ||
               (!wasCut && !wasBoost && (nowCut || nowBoost)))
            {
                if(wasCut)   fillSegment(segStart, i-1, true);
                if(wasBoost) fillSegment(segStart, i-1, false);
                segStart = i;
            }
        }
        // Close final segment
        if(magDB[(size_t)(N-1)] < -0.1f)  fillSegment(segStart, N-1, true);
        if(magDB[(size_t)(N-1)] >  0.1f)  fillSegment(segStart, N-1, false);

        // ── Combined EQ curve line (white, on top of silhouette) ─────────────
        Path combined; bool started=false;
        for(int i=0; i<N; ++i){
            float x=(float)(i*2);
            if(!started){combined.startNewSubPath(x, yPos[(size_t)i]); started=true;}
            else combined.lineTo(x, yPos[(size_t)i]);
        }
        g.setColour(EQColours::CurveFill); 
        Path filled=combined;
        filled.lineTo((float)((N-1)*2), zeroY); filled.lineTo(0.f, zeroY); filled.closeSubPath();
        g.fillPath(filled);
        g.setColour(EQColours::Curve);
        g.strokePath(combined, PathStrokeType(2.f));
    }
}

void EQDisplay::drawHandles(Graphics& g, float W, float H)
{
    for(int b=0;b<proc.getBandCount();++b){
        bool en=*proc.apvts.getRawParameterValue(proc.pid(b,"enabled"))>0.5f;
        if(!en) continue;
        float freq=*proc.apvts.getRawParameterValue(proc.pid(b,"freq"));
        float gain=*proc.apvts.getRawParameterValue(proc.pid(b,"gain"));
        bool  dyn =*proc.apvts.getRawParameterValue(proc.pid(b,"dyn"))>0.5f;

        float hx=freqToX(freq), hy=gainToY(gain);
        auto  col=EQColours::bandColour(b);
        bool  sel=(b==selectedBand), hov=(b==hoveredBand);
        float r=HANDLE_R+(hov?2.f:0.f);

        // Glow for selected
        if(sel){
            g.setColour(col.withAlpha(0.2f));
            g.fillEllipse(hx-r-4.f,hy-r-4.f,(r+4.f)*2.f,(r+4.f)*2.f);
        }

        // Dynamic ring
        float gr=proc.getBandGainReduction(b);
        if(dyn && std::abs(gr)>0.3f){
            g.setColour(col.withAlpha(0.5f));
            float ringR=r+3.f+std::abs(gr)*0.3f;
            g.drawEllipse(hx-ringR,hy-ringR,ringR*2.f,ringR*2.f,1.5f);
        }

        // Main handle
        g.setColour(sel?col:col.withAlpha(0.8f));
        g.fillEllipse(hx-r,hy-r,r*2.f,r*2.f);
        g.setColour(Colours::black.withAlpha(0.4f));
        g.drawEllipse(hx-r,hy-r,r*2.f,r*2.f,1.f);

        // Band number
        g.setColour(Colours::black.withAlpha(0.85f));
        g.setFont(Font(9.f,Font::bold));
        g.drawText(String(b+1),(int)(hx-r),(int)(hy-r),(int)(r*2.f),(int)(r*2.f),Justification::centred);

        // Dynamic indicator dot
        if(dyn){
            g.setColour(Colours::white.withAlpha(0.9f));
            g.fillEllipse(hx+r-4.f,hy-r,4.f,4.f);
        }

        // Tooltip
        if(hov||sel){
            float q=*proc.apvts.getRawParameterValue(proc.pid(b,"q"));
            int ti=(int)*proc.apvts.getRawParameterValue(proc.pid(b,"type"));
            String tip=filterTypeName((EQFilterType)ti)+" | "
                +String(freq<1000.f?freq:freq/1000.f,1)+(freq>=1000.f?"k":"")+"Hz"
                +"  "+String(gain,1)+"dB"
                +"  Q:"+String(q,2);
            int tw=jmin(240,(int)W-20);
            int tx=jlimit(4,(int)W-tw-4,(int)hx-tw/2);
            int ty=hy>30.f?(int)hy-28:(int)hy+14;
            g.setColour(Colours::black.withAlpha(0.8f));
            g.fillRoundedRectangle((float)tx-2.f,(float)ty-2.f,(float)tw+4.f,16.f,3.f);
            g.setColour(col);
            g.setFont(Font(10.f));
            g.drawText(tip,tx,ty,tw,12,Justification::centred);
        }
    }
}

void EQDisplay::paint(Graphics& g)
{
    float W=(float)getWidth(), H=(float)getHeight();
    g.setColour(EQColours::DisplayBG);
    g.fillRoundedRectangle(getLocalBounds().toFloat(),6.f);

    drawGrid       (g,W,H);
    drawSpectraBG  (g,W,H);   // main + M/S spectra (filled, behind silhouette)
    drawEQCurves   (g,W,H);   // silhouette + EQ curve on top of spectra
    drawSpectraFG  (g,W,H);   // SC line + frozen snapshot on top of everything
    drawHandles    (g,W,H);

    // Hint text when no bands
    if(proc.getBandCount()==0){
        g.setColour(Colours::white.withAlpha(0.2f));
        g.setFont(Font(13.f));
        g.drawText("Double-click to add a band  —  position determines filter type",
            getLocalBounds().withTrimmedBottom(20), Justification::centred);
        g.setFont(Font(10.f));
        g.setColour(Colours::white.withAlpha(0.12f));
        g.drawText("Low freq + deep cut = HP  |  High freq + deep cut = LP  |  Edges = Shelf  |  Middle = Bell",
            getLocalBounds().withTrimmedTop(20), Justification::centred);
    }

    g.setColour(EQColours::Grid.brighter());
    g.drawRoundedRectangle(getLocalBounds().toFloat(),6.f,1.f);
}

//==============================================================================
// BandControlPanel
//==============================================================================
BandControlPanel::BandControlPanel(DynamicEQProcessor& p):proc(p)
{
    auto ss=[&](Slider& s){
        s.setSliderStyle(Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(Slider::TextBoxBelow,false,64,15);
        s.setColour(Slider::textBoxTextColourId,    Colours::lightgrey);
        s.setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
        addAndMakeVisible(s);
    };
    ss(freqSlider); ss(gainSlider); ss(qSlider);
    ss(threshSlider); ss(ratioSlider); ss(attackSlider); ss(releaseSlider);

    auto sc=[&](ComboBox& c, StringArray items){
        c.addItemList(items,1);
        c.setColour(ComboBox::backgroundColourId, Colours::black.withAlpha(0.5f));
        c.setColour(ComboBox::textColourId,       Colours::lightgrey);
        c.setColour(ComboBox::outlineColourId,    EQColours::Grid);
        addAndMakeVisible(c);
    };
    sc(typeBox, {"Bell","Low Shelf","High Shelf","Low Pass","High Pass","Notch","Band Pass","Tilt"});
    sc(slopeBox,{"6 dB/oct","12 dB/oct","24 dB/oct","48 dB/oct"});
    sc(detBox,  {"Internal","Sidechain","Manual"});
    sc(msRouteBox, {"Stereo","Mid","Side"});

    // DYN button — toggles, lights up when on
    dynButton.setClickingTogglesState(true);
    dynButton.setColour(TextButton::buttonColourId,   Colours::black.withAlpha(0.5f));
    dynButton.setColour(TextButton::buttonOnColourId, Colour(0xff1a4a1a));
    dynButton.setColour(TextButton::textColourOffId,  Colours::grey);
    dynButton.setColour(TextButton::textColourOnId,   Colour(0xff66ff66));
    addAndMakeVisible(dynButton);

    // BYPASS button
    bypassButton.setClickingTogglesState(true);
    bypassButton.setColour(TextButton::buttonColourId,   Colours::black.withAlpha(0.5f));
    bypassButton.setColour(TextButton::buttonOnColourId, Colour(0xff4a1a1a));
    bypassButton.setColour(TextButton::textColourOffId,  Colours::grey);
    bypassButton.setColour(TextButton::textColourOnId,   Colour(0xffff6666));
    addAndMakeVisible(bypassButton);

    // Section labels
    auto sectionLbl=[&](Label& l, const String& t){
        l.setText(t, dontSendNotification);
        l.setFont(Font(9.f, Font::bold));
        l.setColour(Label::textColourId, EQColours::GridText);
        l.setJustificationType(Justification::centredLeft);
        addAndMakeVisible(l);
    };
    sectionLbl(eqSectionLabel,  "EQ");
    sectionLbl(dynSectionLabel, "DYNAMICS");

    // Knob labels
    StringArray lblNames={"Freq","Gain","Q","Threshold","Ratio","Attack","Release"};
    for(int i=0;i<7;++i){
        labels[i].setText(lblNames[i], dontSendNotification);
        labels[i].setFont(Font(9.f));
        labels[i].setJustificationType(Justification::centred);
        labels[i].setColour(Label::textColourId, Colours::grey);
        addAndMakeVisible(labels[i]);
    }

    // band starts at -1 (nothing selected); attachments built when setBand() called
    band = -1;
}

void BandControlPanel::setSelectedBand(int b)
{
    if(band==b) return;
    band=b;
    rebuild();
}

void BandControlPanel::updateDynState()
{
    if(band<0||band>=proc.getBandCount()) return;
    bool dynOn=*proc.apvts.getRawParameterValue(pid(band,"dyn"))>0.5f;
    float alpha=dynOn?1.f:0.35f;
    Colour dimText=Colours::grey.withAlpha(dynOn?1.f:0.4f);

    for(auto* s:{&threshSlider,&ratioSlider,&attackSlider,&releaseSlider}){
        s->setAlpha(alpha);
        s->setInterceptsMouseClicks(dynOn,dynOn);
    }
    detBox.setAlpha(alpha);
    detBox.setInterceptsMouseClicks(dynOn,dynOn);
    for(int i=3;i<7;++i) labels[i].setColour(Label::textColourId, dimText);
    dynSectionLabel.setColour(Label::textColourId,
        dynOn ? Colour(0xff66ff66) : EQColours::GridText);
}

void BandControlPanel::rebuild()
{
    freqAtt.reset();gainAtt.reset();qAtt.reset();
    threshAtt.reset();ratioAtt.reset();attackAtt.reset();releaseAtt.reset();
    typeAtt.reset();slopeAtt.reset();detAtt.reset();msRouteAtt.reset();

    if(band<0||band>=proc.getBandCount()){ repaint(); return; }

    auto col=EQColours::bandColour(band);
    for(auto* s:{&freqSlider,&gainSlider,&qSlider,&threshSlider,&ratioSlider,&attackSlider,&releaseSlider})
        s->setColour(Slider::rotarySliderFillColourId, col);

    auto& ap=proc.apvts;
    freqAtt    =std::make_unique<SA>(ap,pid(band,"freq"),    freqSlider);
    gainAtt    =std::make_unique<SA>(ap,pid(band,"gain"),    gainSlider);
    qAtt       =std::make_unique<SA>(ap,pid(band,"q"),       qSlider);
    threshAtt  =std::make_unique<SA>(ap,pid(band,"thresh"),  threshSlider);
    ratioAtt   =std::make_unique<SA>(ap,pid(band,"ratio"),   ratioSlider);
    attackAtt  =std::make_unique<SA>(ap,pid(band,"attack"),  attackSlider);
    releaseAtt =std::make_unique<SA>(ap,pid(band,"release"), releaseSlider);
    typeAtt    =std::make_unique<CA>(ap,pid(band,"type"),    typeBox);
    slopeAtt   =std::make_unique<CA>(ap,pid(band,"slope"),   slopeBox);
    detAtt     =std::make_unique<CA>(ap,pid(band,"detmode"), detBox);
    msRouteAtt =std::make_unique<CA>(ap,pid(band,"msroute"), msRouteBox);

    bool dynOn=*ap.getRawParameterValue(pid(band,"dyn"))>0.5f;
    bool bypassed=!(*ap.getRawParameterValue(pid(band,"enabled"))>0.5f);
    dynButton   .setToggleState(dynOn,   dontSendNotification);
    bypassButton.setToggleState(bypassed,dontSendNotification);

    dynButton.onClick=[this]{
        bool d=dynButton.getToggleState();
        if(auto* p=proc.apvts.getParameter(pid(band,"dyn"))) p->setValueNotifyingHost(d?1.f:0.f);
        updateDynState();
    };
    bypassButton.onClick=[this]{
        bool byp=bypassButton.getToggleState();
        if(auto* p=proc.apvts.getParameter(pid(band,"enabled"))) p->setValueNotifyingHost(byp?0.f:1.f);
    };

    updateDynState();
    resized();   // lay out knobs immediately — without this they have zero size until parent triggers layout
    repaint();
}

void BandControlPanel::paint(Graphics& g)
{
    g.fillAll(EQColours::BG);

    if(band<0||band>=proc.getBandCount()){
        g.setColour(Colours::white.withAlpha(0.12f));
        g.setFont(Font(12.f));
        g.drawText("Double-click the display to add a band",getLocalBounds(),Justification::centred);
        return;
    }

    auto col=EQColours::bandColour(band);

    // Top accent line
    g.setColour(col.withAlpha(0.6f));
    g.fillRect(0,0,getWidth(),2);

    // Divider between EQ and Dynamics sections
    int divX=getWidth()/2;
    g.setColour(EQColours::Grid);
    g.drawVerticalLine(divX, 4.f, (float)getHeight()-8.f);

    // Band label top-left
    g.setColour(col);
    g.setFont(Font(10.f,Font::bold));
    String typeName=filterTypeName((EQFilterType)(int)*proc.apvts.getRawParameterValue(pid(band,"type")));
    g.drawText("BAND "+String(band+1)+"  "+typeName, 8, 4, 220, 14, Justification::centredLeft);

    // GR meter strip at very bottom
    float gr=jlimit(0.f,24.f,-proc.getBandGainReduction(band));
    float mw=(float)getWidth()-20.f;
    g.setColour(Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(10.f,(float)getHeight()-6.f,mw,3.f,1.f);
    g.setColour(col.withAlpha(0.8f));
    g.fillRoundedRectangle(10.f,(float)getHeight()-6.f,mw*(gr/24.f),3.f,1.f);
}

void BandControlPanel::resized()
{
    if(band<0) return;

    // Knob size scales with uiScale — base 52px, min 40
    int kSize = jmax(40, (int)(52.f * uiScale));
    int lblH  = 14;
    int knobH = kSize + lblH;
    int comboH= 22;
    int btnH  = 22;

    auto area = getLocalBounds().reduced(6);
    area.removeFromTop(20);    // title row
    area.removeFromBottom(8);  // GR meter

    int half = area.getWidth() / 2;
    auto leftArea  = area.removeFromLeft(half).reduced(4, 0);
    auto rightArea = area.reduced(4, 0);

    // ── LEFT: EQ section ────────────────────────────────────────────────────
    // Section label + bypass button
    auto eqHeader = leftArea.removeFromTop(20);
    eqSectionLabel.setBounds(eqHeader.removeFromLeft(30));
    bypassButton.setBounds(eqHeader.removeFromRight(60).reduced(2,1));
    leftArea.removeFromTop(2);

    // Type + slope + M/S route combos
    auto comboRow = leftArea.removeFromTop(comboH);
    int cw = comboRow.getWidth();
    typeBox    .setBounds(comboRow.removeFromLeft(cw*45/100).reduced(2,1));
    slopeBox   .setBounds(comboRow.removeFromLeft(cw*30/100).reduced(2,1));
    msRouteBox .setBounds(comboRow.reduced(2,1));
    leftArea.removeFromTop(4);

    // 3 knobs: Freq / Gain / Q
    auto knobRow = leftArea.removeFromTop(knobH);
    int kw3 = knobRow.getWidth() / 3;
    auto kn=[&](Slider& s, Label& l, Rectangle<int> b){
        l.setBounds(b.removeFromBottom(lblH));
        s.setBounds(b.reduced(2,0));
    };
    kn(freqSlider, labels[0], knobRow.removeFromLeft(kw3));
    kn(gainSlider, labels[1], knobRow.removeFromLeft(kw3));
    kn(qSlider,    labels[2], knobRow);

    // ── RIGHT: Dynamics section ──────────────────────────────────────────────
    // Section label + DYN button
    auto dynHeader = rightArea.removeFromTop(20);
    dynSectionLabel.setBounds(dynHeader.removeFromLeft(70));
    dynButton.setBounds(dynHeader.removeFromRight(60).reduced(2,1));
    rightArea.removeFromTop(2);

    // Detection mode combo
    auto detRow = rightArea.removeFromTop(comboH);
    detBox.setBounds(detRow.reduced(2,1));
    rightArea.removeFromTop(4);

    // 4 knobs: Thresh / Ratio / Attack / Release
    auto dynKnobRow = rightArea.removeFromTop(knobH);
    int kw4 = dynKnobRow.getWidth() / 4;
    kn(threshSlider,  labels[3], dynKnobRow.removeFromLeft(kw4));
    kn(ratioSlider,   labels[4], dynKnobRow.removeFromLeft(kw4));
    kn(attackSlider,  labels[5], dynKnobRow.removeFromLeft(kw4));
    kn(releaseSlider, labels[6], dynKnobRow);
}

//==============================================================================
// DividerBar
//==============================================================================
void DividerBar::paint(Graphics& g)
{
    g.setColour(EQColours::Grid);
    g.fillRoundedRectangle(getLocalBounds().toFloat(),3.f);
    g.setColour(Colours::white.withAlpha(0.3f));
    for(int i=-3;i<=3;++i)
        g.fillEllipse((float)(getWidth()/2+i*7-2),(float)(getHeight()/2-2),4.f,4.f);
}

//==============================================================================
// DynamicEQEditor
//==============================================================================
DynamicEQEditor::DynamicEQEditor(DynamicEQProcessor& p)
    : AudioProcessorEditor(p), proc(p),
      eqDisplay(p), controlPanel(p),
      outGainAtt(p.apvts,"output_gain",outGainSlider)
{
    addAndMakeVisible(eqDisplay);
    addAndMakeVisible(divider);
    addAndMakeVisible(controlPanel);

    divider.onDrag=[this](int d){
        int avail=getHeight()-TOOLBAR_H-DIVIDER_H-OUTGAIN_H-8;
        eqHeightPx=jlimit(MIN_EQ_H,avail-MIN_CTL_H,eqHeightPx+d);
        doLayout();
    };

    eqDisplay.onBandSelected=[this](int b){ selectBand(b); };
    eqDisplay.onBandRemoved=[this](int){
        int b=jmin(eqDisplay.getSelectedBand(),proc.getBandCount()-1);
        selectBand(b);
    };

    // Toolbar buttons
    auto styleBtn=[](TextButton& b, bool toggle=false){
        b.setColour(TextButton::buttonColourId,  Colours::black.withAlpha(0.5f));
        b.setColour(TextButton::buttonOnColourId,Colour(0xff003366));
        b.setColour(TextButton::textColourOffId, Colours::lightgrey);
        b.setColour(TextButton::textColourOnId,  EQColours::Frozen);
        if(toggle) b.setClickingTogglesState(true);
    };
    styleBtn(freezeBtn, true);
    styleBtn(clearFreezeBtn);
    styleBtn(speedBtn);
    styleBtn(scaleBtn);
    styleBtn(addBandBtn);

    // Freeze: toggle on = freeze (captures snapshot), toggle off = unfreeze (keeps snapshot)
    freezeBtn.onClick=[this]{
        bool f=freezeBtn.getToggleState();
        proc.getAnalyser().setFrozen(f);
        freezeBtn.setButtonText(f ? "[*] Frozen" : "[*] Freeze");
        // Show/hide clear button
        clearFreezeBtn.setVisible(proc.getAnalyser().hasFrozenData());
    };

    // Clear: removes the frozen snapshot entirely and unfreezes
    clearFreezeBtn.setVisible(false);
    clearFreezeBtn.onClick=[this]{
        proc.getAnalyser().setFrozen(false);
        proc.getAnalyser().clearSnapshot();
        freezeBtn.setToggleState(false, dontSendNotification);
        freezeBtn.setButtonText("[*] Freeze");
        clearFreezeBtn.setVisible(false);
    };

    // Speed menu
    speedBtn.onClick=[this]{
        PopupMenu m;
        auto cur=proc.getAnalyser().getSpeed();
        m.addItem(1, "Slow",      true, cur==SpectrumAnalyser::Speed::Slow);
        m.addItem(2, "Medium",    true, cur==SpectrumAnalyser::Speed::Medium);
        m.addItem(3, "Fast",      true, cur==SpectrumAnalyser::Speed::Fast);
        m.addItem(4, "Very Fast", true, cur==SpectrumAnalyser::Speed::VeryFast);
        m.showMenuAsync(PopupMenu::Options().withTargetComponent(speedBtn),[this](int r){
            const char* labels[]={"Slow","Med","Fast","V.Fast"};
            SpectrumAnalyser::Speed speeds[]={
                SpectrumAnalyser::Speed::Slow,
                SpectrumAnalyser::Speed::Medium,
                SpectrumAnalyser::Speed::Fast,
                SpectrumAnalyser::Speed::VeryFast
            };
            if(r>=1&&r<=4){
                proc.getAnalyser().setSpeed(speeds[r-1]);
                proc.getScAnalyser().setSpeed(speeds[r-1]);
                speedBtn.setButtonText(String("Spd: ")+labels[r-1]);
            }
        });
    };

    addBandBtn.onClick=[this]{
        float freq=440.f*(1.f+0.3f*(float)proc.getBandCount());
        freq=jlimit(30.f,18000.f,freq);
        int idx=proc.addBand(freq);
        if(idx>=0) selectBand(idx);
    };
    scaleBtn.onClick=[this]{
        PopupMenu m;
        m.addItem(1,"50%");m.addItem(2,"75%");m.addItem(3,"100%");
        m.addItem(4,"125%");m.addItem(5,"150%");m.addItem(6,"175%");
        m.showMenuAsync(PopupMenu::Options().withTargetComponent(scaleBtn),[this](int r){
            float sc[]={0.5f,0.75f,1.f,1.25f,1.5f,1.75f};
            const char* lb[]={"50%","75%","100%","125%","150%","175%"};
            if(r>=1&&r<=6){currentScale=sc[r-1];scaleBtn.setButtonText(lb[r-1]);applyScale(currentScale);}
        });
    };

    addAndMakeVisible(freezeBtn);
    addAndMakeVisible(clearFreezeBtn);
    addAndMakeVisible(speedBtn);
    addAndMakeVisible(addBandBtn);
    addAndMakeVisible(scaleBtn);

    // ── Undo / Redo ────────────────────────────────────────────────────────
    styleBtn(undoBtn); styleBtn(redoBtn);
    undoBtn.onClick=[this]{ proc.undoManager.undo(); };
    redoBtn.onClick=[this]{ proc.undoManager.redo(); };
    addAndMakeVisible(undoBtn);
    addAndMakeVisible(redoBtn);

    // ── M/S toggle ─────────────────────────────────────────────────────────
    msBtn.setClickingTogglesState(true);
    msBtn.setColour(TextButton::buttonColourId,   Colours::black.withAlpha(0.5f));
    msBtn.setColour(TextButton::buttonOnColourId, Colour(0xff2a1a4a));
    msBtn.setColour(TextButton::textColourOffId,  Colours::lightgrey);
    msBtn.setColour(TextButton::textColourOnId,   Colour(0xffcc88ff));
    msBtn.onClick=[this]{
        bool ms=msBtn.getToggleState();
        msBtn.setButtonText(ms?"Mid/Side":"Stereo");
        if(auto* p=proc.apvts.getParameter("proc_mode"))
            p->setValueNotifyingHost(ms?1.f:0.f);
    };
    addAndMakeVisible(msBtn);

    // ── Linear Phase toggle ─────────────────────────────────────────────────
    lpBtn.setClickingTogglesState(true);
    lpBtn.setColour(TextButton::buttonColourId,   Colours::black.withAlpha(0.5f));
    lpBtn.setColour(TextButton::buttonOnColourId, Colour(0xff1a3a1a));
    lpBtn.setColour(TextButton::textColourOffId,  Colours::lightgrey);
    lpBtn.setColour(TextButton::textColourOnId,   Colour(0xff88ffaa));
    lpBtn.onClick=[this]{
        bool lp=lpBtn.getToggleState();
        lpBtn.setButtonText(lp?"Lin Phase":"Min Phase");
        if(auto* p=proc.apvts.getParameter("phase_mode"))
            p->setValueNotifyingHost(lp?1.f:0.f);
    };
    addAndMakeVisible(lpBtn);

    // ── Presets ─────────────────────────────────────────────────────────────
    presetManager = std::make_unique<PresetManager>(proc);

    styleBtn(presetSaveBtn); styleBtn(presetLoadBtn);
    presetSaveBtn.onClick=[this]{ presetManager->showSaveDialog(this); };
    presetLoadBtn.onClick=[this]{ presetManager->showLoadMenu(&presetLoadBtn); };
    addAndMakeVisible(presetSaveBtn);
    addAndMakeVisible(presetLoadBtn);

    presetNameLabel.setText("(unsaved)", dontSendNotification);
    presetNameLabel.setFont(Font(10.f, Font::italic));
    presetNameLabel.setColour(Label::textColourId, Colours::grey);
    presetNameLabel.setJustificationType(Justification::centredLeft);
    addAndMakeVisible(presetNameLabel);

    titleLabel.setText("DYNAMIC EQ", dontSendNotification);
    titleLabel.setFont(Font(14.f,Font::bold));
    titleLabel.setColour(Label::textColourId,Colours::white.withAlpha(0.7f));
    addAndMakeVisible(titleLabel);

    outGainSlider.setSliderStyle(Slider::LinearHorizontal);
    outGainSlider.setTextBoxStyle(Slider::TextBoxRight,false,50,18);
    outGainSlider.setColour(Slider::trackColourId,         Colours::white.withAlpha(0.15f));
    outGainSlider.setColour(Slider::thumbColourId,         Colours::white.withAlpha(0.8f));
    outGainSlider.setColour(Slider::textBoxTextColourId,   Colours::lightgrey);
    outGainSlider.setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);
    addAndMakeVisible(outGainSlider);
    outGainLabel.setText("Output",dontSendNotification);
    outGainLabel.setFont(Font(10.f));
    outGainLabel.setColour(Label::textColourId,Colours::grey);
    outGainLabel.setJustificationType(Justification::centredRight);
    addAndMakeVisible(outGainLabel);

    setResizable(false,false);
    setSize(BASE_W,BASE_H);
    startTimerHz(20);
}

DynamicEQEditor::~DynamicEQEditor(){ stopTimer(); }

void DynamicEQEditor::timerCallback()
{
    controlPanel.updateDynState();

    // Update undo/redo button states
    undoBtn.setEnabled(proc.undoManager.canUndo());
    redoBtn.setEnabled(proc.undoManager.canRedo());
    undoBtn.setAlpha(proc.undoManager.canUndo() ? 1.f : 0.4f);
    redoBtn.setAlpha(proc.undoManager.canRedo() ? 1.f : 0.4f);

    // Sync preset name label
    String pname = presetManager->getCurrentPresetName();
    presetNameLabel.setText(pname.isNotEmpty() ? pname : "(unsaved)", dontSendNotification);

    repaint();
}

void DynamicEQEditor::selectBand(int b)
{
    eqDisplay.setSelectedBand(b);
    controlPanel.setSelectedBand(b);
    // Force a full layout pass so knobs get correct bounds immediately
    doLayout();
    controlPanel.resized();
}

void DynamicEQEditor::applyScale(float s)
{
    currentScale = s;
    controlPanel.setUIScale(s);
    setSize((int)(BASE_W*s),(int)(BASE_H*s));
}

void DynamicEQEditor::paint(Graphics& g)
{
    g.fillAll(EQColours::BG);

    // Toolbar background — two rows
    g.setColour(Colours::black.withAlpha(0.35f));
    g.fillRect(0, 0, getWidth(), TOOLBAR_H);
    g.setColour(EQColours::Grid);
    g.drawHorizontalLine(TOOLBAR_H, 0.f, (float)getWidth());
    // Row divider
    g.setColour(EQColours::Grid.withAlpha(0.5f));
    g.drawHorizontalLine(TOOLBAR_H/2, 0.f, (float)getWidth());

    // Linear phase latency warning
    bool lp=lpBtn.getToggleState();
    if(lp){
        g.setColour(Colour(0xff88ffaa).withAlpha(0.7f));
        g.setFont(Font(9.f));
        g.drawText("Linear Phase: "+String(proc.getTailLengthSeconds()*1000.f,1)+"ms latency",
                   getWidth()-260, TOOLBAR_H/2+4, 250, 14, Justification::centredRight);
    }

    // Spectrum legend bottom-right
    int lx = getWidth()-195;
    int ly = TOOLBAR_H+6;
    auto swatch=[&](Colour c, const String& lbl, int x){
        g.setColour(c); g.fillRect((float)x,(float)ly+1,8.f,6.f);
        g.setColour(EQColours::GridText); g.setFont(Font(9.f));
        g.drawText(lbl,x+10,ly,36,10,Justification::centredLeft);
    };
    bool ms=msBtn.getToggleState();
    if(ms){
        swatch(Colour(0xffcc88ff), "Mid",  lx);
        swatch(Colour(0xff88ccff), "Side", lx+52);
    } else {
        swatch(EQColours::Spectrum,"Main", lx);
        swatch(EQColours::SCSpect, "SC",   lx+52);
    }
    swatch(EQColours::Frozen,  "Frzn", lx+104);
}

void DynamicEQEditor::doLayout()
{
    int W=getWidth();
    int row1Y=2, row2Y=TOOLBAR_H/2+2;
    int btnH=TOOLBAR_H/2-4;

    // ── Row 1: Title | +Band | Undo | Redo | MS | LP | [spacer] | Scale ────
    titleLabel    .setBounds(6,      row1Y,   120, btnH);
    addBandBtn    .setBounds(130,    row1Y,    62, btnH);
    undoBtn       .setBounds(196,    row1Y,    46, btnH);
    redoBtn       .setBounds(246,    row1Y,    46, btnH);
    msBtn         .setBounds(300,    row1Y,    68, btnH);
    lpBtn         .setBounds(372,    row1Y,    72, btnH);
    scaleBtn      .setBounds(W-66,   row1Y,    60, btnH);

    // ── Row 2: Presets | [name] | Freeze | Clear | Speed ──────────────────
    presetSaveBtn .setBounds(6,      row2Y,    42, btnH);
    presetLoadBtn .setBounds(52,     row2Y,    62, btnH);
    presetNameLabel.setBounds(118,   row2Y,   200, btnH);
    freezeBtn     .setBounds(322,    row2Y,    76, btnH);
    clearFreezeBtn.setBounds(402,    row2Y,    42, btnH);
    speedBtn      .setBounds(448,    row2Y,    72, btnH);

    int y = TOOLBAR_H;

    // EQ display
    eqDisplay.setBounds(8, y+4, W-16, eqHeightPx);
    y += eqHeightPx+4;

    // Divider
    divider.setBounds(8, y+2, W-16, DIVIDER_H);
    y += DIVIDER_H+4;

    // Control panel
    int ctlH=getHeight()-y-OUTGAIN_H-6;
    controlPanel.setBounds(0, y, W, ctlH);
    y += ctlH+4;

    // Output gain
    auto ogr=Rectangle<int>(8,y,W-16,OUTGAIN_H-2);
    outGainLabel.setBounds(ogr.removeFromLeft(50));
    outGainSlider.setBounds(ogr);
}

void DynamicEQEditor::resized()
{
    if(eqHeightPx==0){
        int avail=getHeight()-TOOLBAR_H-DIVIDER_H-OUTGAIN_H-8;
        eqHeightPx=jlimit(MIN_EQ_H,avail-MIN_CTL_H,(int)(avail*0.6f));
    }
    doLayout();
}

//==============================================================================
juce::AudioProcessorEditor* DynamicEQProcessor::createEditor()
    { return new DynamicEQEditor(*this); }
