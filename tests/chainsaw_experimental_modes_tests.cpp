#include "../src/openxr/ChainsawExperimentalModesPolicy.h"
#include "../src/openxr/GripZoneMarkerPolicy.h"
#include <cmath>
#define CHECK(x) do { if(!(x)) return __LINE__; } while(false)
int main(){
    using namespace kharvox;
    const ChainsawVec3 head{0.f,1.7f,0.f};
    const ChainsawVec3 forward{0.f,0.f,-1.f};
    const ChainsawVec3 right{1.f,0.f,0.f},up{0.f,1.f,0.f};
    const auto center=singleZoneCenter(head,forward);
    CHECK(std::abs(center.z+1.10f)<.001f);
    CHECK(std::abs(center.y-1.53f)<.001f);
    CHECK(std::abs(length3(minus3(center,head))-1.113f)<.02f);
    CHECK(length3(minus3(zonePosition(head,0,0),center))<.001f);
    CHECK(length3(minus3(singleZoneCenter(head,{0,0,0}),center))<.001f);
    CHECK(chainsawZoneQuadHeightMeters>chainsawZoneQuadWidthMeters);
    CHECK(chainsawZoneQuadWidthMeters>1.2f);
    CHECK(std::abs(chainsawSingleSwipeDistanceMeters-.25f)<.0001f);
    CHECK(chainsawPhysicalMotionFastHz==45);
    CHECK(chainsawZoneGuidanceAmplitude(-1.f)==0.f);
    CHECK(chainsawZoneGuidanceAmplitude(0.f)==0.f);
    CHECK(chainsawZoneGuidanceAmplitude(.01f)>.0f);
    CHECK(chainsawZoneGuidanceAmplitude(.15f)>chainsawZoneGuidanceAmplitude(.01f));
    CHECK(chainsawZoneGuidanceAmplitude(.40f)>chainsawZoneGuidanceAmplitude(.15f));
    CHECK(chainsawZoneGuidanceAmplitude(.69f)>chainsawZoneGuidanceAmplitude(.40f));
    CHECK(chainsawZoneGuidanceAmplitude(.81f)==chainsawZoneGuideMaxAmplitude);
    CHECK(chainsawZoneGuidanceAmplitude(1.40f)==chainsawZoneGuideMaxAmplitude);
    CHECK(chainsawZoneGuidanceAmplitude(std::nanf(""))==0.f);
    CHECK(gripZoneOutlineAlpha(0,0)==0);
    CHECK(gripZoneOutlineAlpha(1,1)==0);
    CHECK(gripZoneOutlineAlpha(.69f,0)>120);
    CHECK(gripZoneOutlineAlpha(0,.69f)>120);

    // Physical controller halfway to the drawn quad must map to the
    // SAME visible position (not fail a 3D sphere collision).
    const auto handAt=[&](float screenX,float screenY,float depth){
        const float ratio=depth/1.10f;
        return ChainsawVec3{head.x+(center.x+screenX-head.x)*ratio,
                            head.y+(center.y+screenY-head.y)*ratio,
                            head.z+(center.z-head.z)*ratio};
    };
    const auto nearHand=handAt(.16f,0.f,.48f);
    const auto farHand=handAt(.16f,0.f,.9f);
    const auto nearProj=projectChainsawHandToOval(
        head,nearHand,center,forward,right,up);
    const auto farProj=projectChainsawHandToOval(
        head,farHand,center,forward,right,up);
    CHECK(nearProj.valid&&farProj.valid);
    CHECK(nearProj.normalizedRadius<chainsawZoneEnterRadius);
    const auto atCentre=projectChainsawHandToOval(
        head,handAt(0.f,0.f,.48f),center,forward,right,up);
    const auto atCentreFar=projectChainsawHandToOval(
        head,handAt(0.f,0.f,.90f),center,forward,right,up);
    CHECK(atCentre.valid&&atCentreFar.valid);
    CHECK(chainsawZoneGuidanceAmplitude(atCentre.normalizedRadius)<.005f);
    CHECK(chainsawZoneGuidanceAmplitude(atCentreFar.normalizedRadius)<.005f);
    CHECK(std::abs(chainsawZoneGuidanceAmplitude(farProj.normalizedRadius)-
        chainsawZoneGuidanceAmplitude(nearProj.normalizedRadius))<.0001f);
    CHECK(std::abs(nearProj.normalizedRadius-farProj.normalizedRadius)<.001f);
    CHECK(!projectChainsawHandToOval(head,handAt(0,0,-.4f),
        center,forward,right,up).valid);
    CHECK(projectChainsawHandToOval(head,handAt(.80f,0,.48f),
        center,forward,right,up).normalizedRadius>chainsawZoneExitRadius);

    auto sample=[&](ChainsawExperimentState& state,bool native,
                    bool trigger,bool tracked,float screenX,float screenY,
                    float depth,std::uint64_t ms){
        const auto hand=handAt(screenX,screenY,depth);
        return updateChainsawExperiment(
            state,ChainsawExperimentalMode::GripPull,native,trigger,tracked,
            hand.x,hand.y,hand.z,true,0,0,-1,ms,
            480,32,head,0.f,true,forward,right,up);
    };
    // Initial oval entry arms, but does NOT count as physical stroke or
    // inject a 30 Hz entry boost. First-press live motion caps at 45 Hz.
    ChainsawExperimentState st{};
    CHECK(!sample(st,false,true,true,.70f,0,.48f,100).armed);
    CHECK(!sample(st,true,true,true,.70f,0,.48f,110).armed);
    CHECK(st.seenOutside);
    CHECK(!sample(st,true,true,true,.59f,0,.48f,125).armed);
    const auto entry=sample(st,true,true,true,.25f,0,.48f,140);
    CHECK(entry.started&&entry.armed&&entry.enteredZone);
    CHECK(entry.targetHz==480); // test idle is deliberately 480
    CHECK(st.strokeTravelMeters==0.f&&!st.oneSwipeCommitted);
    CHECK(!st.fastFinishLatched&&st.triggerRestarts==0);
    const auto cut=sample(st,true,true,true,.1f,0,.48f,160);
    CHECK(cut.armed&&cut.targetHz==chainsawPhysicalMotionFastHz);
    CHECK(cut.progress>.5f);
    CHECK(!sample(st,true,true,false,.1f,0,.48f,175).armed);
    CHECK(sample(st,true,true,true,.08f,0,.48f,190).armed);
    CHECK(sample(st,true,true,true,.70f,0,.48f,210).armed);
    CHECK(sample(st,true,true,true,.85f,0,.48f,225).armed);
    CHECK(st.oneSwipeCommitted); // distance still tracked, no 30 Hz latch
    CHECK(sample(st,true,true,true,.85f,0,.48f,500).targetHz>45);
    // Trigger release must keep 300 Hz even when configured first-press
    // idle is 480 Hz; no fast-finish before the second press.
    const auto released=sample(st,true,false,true,.85f,0,.48f,515);
    CHECK(released.armed&&released.targetHz==300);
    CHECK(st.resumeEligible&&!st.latchedGrip&&!st.fastFinishLatched);
    CHECK(st.strokeTravelMeters==0.f&&!st.oneSwipeCommitted);
    // Second press skips the rest of the native execution at 30 Hz.
    const auto second=sample(st,true,true,true,.85f,0,.48f,530);
    CHECK(second.armed&&second.started&&second.targetHz==30);
    CHECK(st.fastFinishLatched&&st.triggerRestarts==1);
    CHECK(sample(st,true,true,true,.12f,0,.48f,545).targetHz==30);
    CHECK(sample(st,true,false,true,.12f,0,.48f,560).targetHz==30);
    CHECK(sample(st,true,false,false,.12f,0,.48f,575).armed);
    CHECK(sample(st,true,true,true,.12f,0,.48f,590).targetHz==30);
    CHECK(!sample(st,false,true,true,.12f,0,.48f,605).armed);
    CHECK(!st.active&&!st.fastFinishLatched&&!st.resumeEligible);

    // Starting INSIDE requires original outside-to-inside entry on every
    // NEW native kill, no accidental double-trigger activation.
    ChainsawExperimentState inside{};
    CHECK(!sample(inside,true,true,true,.12f,0,.48f,100).armed);
    CHECK(!sample(inside,true,false,true,.80f,0,.48f,110).armed);
    CHECK(!sample(inside,true,true,true,.80f,0,.48f,120).armed);
    CHECK(inside.seenOutside);
    const auto insideEntry=sample(inside,true,true,true,.20f,0,.48f,135);
    CHECK(insideEntry.armed&&insideEntry.targetHz==480);
    CHECK(!inside.oneSwipeCommitted&&inside.strokeTravelMeters==0.f);
    CHECK(sample(inside,true,false,true,.20f,0,.48f,150).targetHz==300);
    CHECK(inside.resumeEligible);
    CHECK(sample(inside,true,true,true,.18f,0,.48f,165).targetHz==30);
    CHECK(inside.triggerRestarts==1&&inside.fastFinishLatched);
    CHECK(!sample(inside,false,true,true,.18f,0,.48f,180).armed);

    // 25 cm stroke STARTS AFTER entry and never latches maximum speed.
    ChainsawExperimentState single{};
    CHECK(!sample(single,true,true,true,.84f,0,.48f,400).armed);
    const auto singleEntry=sample(single,true,true,true,.23f,0,.48f,415);
    CHECK(singleEntry.armed&&singleEntry.targetHz==480);
    CHECK(!single.oneSwipeCommitted&&single.strokeTravelMeters==0.f);
    const auto longCut=sample(single,true,true,true,.82f,0,.48f,430);
    CHECK(longCut.armed&&single.oneSwipeCommitted);
    CHECK(single.strokeTravelMeters>=chainsawSingleSwipeDistanceMeters);
    CHECK(longCut.targetHz==45&&!single.fastFinishLatched);
    ChainsawExperimentResult stationary{};
    for(int i=0;i<12;i++)
        stationary=sample(single,true,true,true,.82f,0,.48f,500+60*i);
    CHECK(stationary.armed&&stationary.targetHz>=470);
    CHECK(stationary.targetHz>longCut.targetHz);
    CHECK(!sample(single,true,true,false,.82f,0,.48f,1230).armed);
    CHECK(sample(single,true,true,true,.82f,0,.48f,1245).armed);
    CHECK(sample(single,true,false,true,.82f,0,.48f,1260).targetHz==300);
    CHECK(sample(single,true,true,false,.82f,0,.48f,1275).targetHz==30);
    CHECK(single.fastFinishLatched);
    CHECK(!sample(single,false,false,false,.82f,0,.48f,1290).armed);

    // Pausing halfway doesn't cross the stroke threshold, and later
    // resuming is a fresh physical measurement, not a forced 30 Hz finish.
    ChainsawExperimentState pause{};
    CHECK(!sample(pause,true,true,true,.84f,0,.48f,1500).armed);
    CHECK(sample(pause,true,true,true,.44f,0,.48f,1515).armed);
    CHECK(pause.strokeTravelMeters==0.f);
    CHECK(sample(pause,true,true,true,.20f,0,.48f,1530).armed);
    CHECK(pause.strokeTravelMeters>0.f&&!pause.oneSwipeCommitted);
    CHECK(sample(pause,true,true,true,.20f,0,.48f,1800).targetHz>45);
    CHECK(!pause.oneSwipeCommitted);
    const auto resumeCut=sample(pause,true,true,true,-.14f,0,.48f,1815);
    CHECK(resumeCut.armed&&pause.oneSwipeCommitted&&resumeCut.targetHz>=45);
    CHECK(!sample(pause,false,true,true,-.14f,0,.48f,1830).armed);
    // Releasing Trigger BEFORE the oval ever arms cannot claim 300 Hz
    // or enable second-press fast finish.
    ChainsawExperimentState fresh{};
    CHECK(!sample(fresh,true,true,true,.14f,0,.48f,2000).armed);
    CHECK(!sample(fresh,true,false,true,.14f,0,.48f,2015).armed);
    CHECK(!sample(fresh,true,true,true,.14f,0,.48f,2030).armed);
    CHECK(!fresh.resumeEligible&&!fresh.fastFinishLatched);

    ChainsawExperimentState ray{};
    const auto begin=updateChainsawExperiment(
        ray,ChainsawExperimentalMode::PointerFocus,
        true,false,true,0,0,0,true,0,0,-1,100,480,32,head);
    CHECK(begin.started);
    const auto moved=updateChainsawExperiment(
        ray,ChainsawExperimentalMode::PointerFocus,
        true,false,true,0,0,0,true,.5f,0,-.8660254f,115,480,32,head);
    CHECK(moved.completed&&moved.targetHz==30);
    return 0;
}
