#include "../src/openxr/ChainsawAnimationInvestigationPolicy.h"
#include <cmath>
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){
    using namespace kharvox;
    CHECK(chainsawInvestigationMaxSamplesPerKill>=1000);
    ChainsawAnimationInvestigationState st{};
    ChainsawAnimationEvidenceInput in{};
    in.nowMs=300;in.renderValid=true;in.renderTime=5;
    CHECK(updateChainsawAnimationInvestigation(st,in).baselineSample);
    in.nowMs=500;in.renderTime=9;
    CHECK(updateChainsawAnimationInvestigation(st,in).baselineSample);
    CHECK(st.baselineSamples==2&&st.baselineStats[0].changed==1);
    in.nativeExecution=true;in.trigger=true;in.tracked=true;
    in.nowMs=510;in.requestedHz=300;in.renderTime=10;
    auto d=updateChainsawAnimationInvestigation(st,in);
    CHECK(d.started&&d.sample&&st.killIndex==1);
    in.nowMs=535;in.renderTime=11;
    d=updateChainsawAnimationInvestigation(st,in);
    CHECK(d.sample&&st.stats[0].changed==1);
    CHECK(st.stats[0].changedSlowClock==1);
    CHECK(st.stats[0].changedHandStill==1);
    in.nowMs=550;
    CHECK(!updateChainsawAnimationInvestigation(st,in).sample);
    in.nowMs=560;in.trigger=false;in.renderTime=12;
    d=updateChainsawAnimationInvestigation(st,in);
    CHECK(d.sample&&d.triggerReleased);
    in.nowMs=580;in.trigger=true;in.doubleTrigger=true;
    in.requestedHz=30;in.renderTime=17;
    d=updateChainsawAnimationInvestigation(st,in);
    CHECK(d.sample&&d.doublePressed&&d.triggerPressed&&d.clockSwitched);
    CHECK(st.stats[0].changedFastClock==1);
    in.nowMs=605;in.chordLocked=true;in.chordProgress=.3f;
    in.renderTime=20;in.handSpeed=.6f;
    d=updateChainsawAnimationInvestigation(st,in);
    CHECK(d.chordStart&&d.sample&&st.stats[0].changedHandMoving>0);
    in.nowMs=630;in.chordFinished=true;in.chordProgress=1.f;
    d=updateChainsawAnimationInvestigation(st,in);
    CHECK(d.chordEnd&&d.sample);
    CHECK(chainsawEvidenceProxyScore(st.stats[0])>0);
    CHECK(chainsawEvidenceProxyScore({})==0);
    in.nowMs=700;in.nativeExecution=false;in.trigger=false;
    d=updateChainsawAnimationInvestigation(st,in);
    CHECK(d.ended&&!st.active);
    CHECK(st.lastSampleCount==6);
    CHECK(st.lastKillDurationMs==190&&st.lastStats[0].changed>=3);
    CHECK(chainsawEvidenceProxyScore(st.lastStats[0])>0);
    in.nowMs=900;in.nativeExecution=true;in.renderTime=0;
    CHECK(updateChainsawAnimationInvestigation(st,in).started&&st.killIndex==2);
    CHECK(st.stats[0].changed==0);
    // No nonfinite invalid candidate may pollute scores.
    CHECK(!chainsawEvidenceValid(in,99));
    CHECK(chainsawEvidenceName(99)!=nullptr);
    CHECK(chainsawEvidenceName(3)!=nullptr);
    // Individual candidates operate independently. A stationary grip joint
    // can be observed separately from time and world-space weapon transforms.
    in.nowMs=925;in.gripValid=true;in.gripOrigin={1.f,0.f,0.f};
    CHECK(updateChainsawAnimationInvestigation(st,in).sample);
    in.nowMs=950;in.gripOrigin={1.2f,0.f,0.f};
    CHECK(updateChainsawAnimationInvestigation(st,in).sample);
    CHECK(st.stats[3].changed==1);
    CHECK(st.stats[4].pairs==0);
    in.nowMs=1000;in.nativeExecution=false;
    CHECK(updateChainsawAnimationInvestigation(st,in).ended);
    // Bounded replay: after 12 independent native executions, reject more
    // but keep baseline counters and all candidate evidence intact.
    for(unsigned k=2;k<chainsawInvestigationMaxKills;k++){
        in.nowMs=2000+100*k;in.nativeExecution=true;
        CHECK(updateChainsawAnimationInvestigation(st,in).started);
        in.nowMs+=50;in.nativeExecution=false;
        CHECK(updateChainsawAnimationInvestigation(st,in).ended);
    }
    in.nativeExecution=true;in.nowMs+=40;
    CHECK(updateChainsawAnimationInvestigation(st,in).capped);
    return 0;
}
