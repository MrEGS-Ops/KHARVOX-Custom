#include "../src/openxr/ChainsawExperimentalModesPolicy.h"
#include "../src/openxr/XInputHapticsPolicy.h"
#include <cmath>
#include <limits>
#include <string>
#define CHECK(x) do{ if(!(x))return __LINE__; }while(false)
int main(){
    using namespace kharvox;
    constexpr auto mode=ChainsawExperimentalMode::TriggerRevPulses;
    CHECK(chainsawModeValid(9));
    CHECK(!chainsawModeUsesCircle(mode));
    CHECK(!chainsawModeStrokeFinish(mode));
    CHECK(chainsawModeIdleHz(mode,300)==200);
    CHECK(chainsawRevMaximumPulls==3);
    const ChainsawVec3 head{0,1.7f,0},forward{0,0,-1};
    const auto center=singleZoneCenter(head,forward);
    auto sample=[&](ChainsawExperimentState& st,bool native,
                    float pressure,float px,std::uint64_t ms,bool tracking=true,
                    bool headTracking=true){
        const float ratio=.48f/1.1f;
        const ChainsawVec3 hand{
            (center.x+px-head.x)*ratio,
            head.y+(center.y-head.y)*ratio,
            (center.z-head.z)*ratio
        };
        return updateChainsawExperiment(st,mode,native,pressure>.55f,tracking,
            hand.x,hand.y,hand.z,true,0,0,-1,ms,300,32,head,0.f,
            headTracking,forward,{1,0,0},{0,1,0},pressure);
    };
    ChainsawExperimentState st{};
    // The FIRST real trigger squeeze starts DOOM's native execution. It also
    // counts as rev one; no separate circle or arm-to-cut gesture is required.
    auto start=sample(st,true,1.f,.20f,100);
    CHECK(start.armed&&start.started&&start.targetHz==200);
    CHECK(st.cutPathLocked&&!st.seenOutside&&!st.revReady);
    CHECK(st.revCount==1&&!st.revPullEdge);
    auto initialBoost=sample(st,true,1.f,.20f,170);
    CHECK(initialBoost.targetHz<200&&initialBoost.targetHz>=30);
    CHECK(sample(st,true,1.f,.80f,1500).targetHz==200);
    CHECK(st.revCount==1&&!st.revPullEdge);
    // Merely relaxing from 1.0 to 0.23 never unlocks a new rev.
    sample(st,true,.23f,.20f,1530);
    CHECK(!st.revReady);
    sample(st,true,.90f,.20f,1560);
    CHECK(st.revCount==1);
    sample(st,true,.05f,.20f,1600);
    CHECK(st.revReady&&st.revReleaseEdge);
    sample(st,true,.20f,.20f,1620);
    CHECK(st.revReady&&st.revCount==1);
    // SECOND pull: one bounded smooth pulse with no accidental auto-renewal.
    auto second=sample(st,true,.35f,.20f,1640);
    CHECK(second.armed&&st.revPullEdge&&!st.revReady&&st.revCount==2);
    CHECK(second.targetHz==200); // begins with the 35 ms smooth attack
    auto rising=sample(st,true,.35f,.20f,1660);
    CHECK(rising.targetHz<200&&rising.targetHz>30);
    auto peak=sample(st,true,.35f,.20f,1690);
    CHECK(peak.targetHz<=rising.targetHz);
    for(std::uint64_t ms: {1710ull,1790ull,1900ull,2200ull}){
        auto held=sample(st,true,.35f,.80f,ms);
        CHECK(st.revCount==2&&!st.revPullEdge);
        CHECK(!held.completed&&!st.fastFinishLatched);
    }
    CHECK(st.lastRequestedHz==200);
    // THIRD physical pull latches DOOM's fastest tested 30Hz clock until
    // the native execution actually exits; further pulls cannot change it.
    sample(st,true,.05f,.80f,2300);
    CHECK(st.revReleaseEdge&&st.revReady);
    auto third=sample(st,true,.40f,.80f,2320);
    CHECK(st.revCount==3&&st.revPullEdge);
    CHECK(st.fastFinishLatched&&third.completed&&third.targetHz==30);
    CHECK(std::string(third.detail)=="THIRD_REV_FAST_FINISH");
    CHECK(sample(st,true,1.f,.80f,2570).targetHz==30);
    sample(st,true,.01f,.80f,2800,false,false);
    CHECK(!st.revReady&&!st.revReleaseEdge);
    auto forbiddenFourth=sample(st,true,.85f,.80f,2820,false,false);
    CHECK(!st.revPullEdge&&st.revCount==3&&forbiddenFourth.armed);
    CHECK(forbiddenFourth.completed&&forbiddenFourth.targetHz==30);
    CHECK(!sample(st,false,.85f,.80f,2860).armed);
    CHECK(st.revCount==0&&!st.fastFinishLatched);
    // First frame with no tracking: X6 still arms without its old circle.
    ChainsawExperimentState noTracking{};
    CHECK(sample(noTracking,true,1.f,.20f,3000,false,false).armed);
    CHECK(noTracking.cutPathLocked&&noTracking.revCount==1);
    // Nonfinite analog values must not create a new pull.
    ChainsawExperimentState invalid{};
    sample(invalid,true,1.f,.85f,3100);
    sample(invalid,true,std::numeric_limits<float>::quiet_NaN(),.20f,3140);
    CHECK(invalid.revReady&&!invalid.revPullEdge);
    // Strong, nonstop right controller texture with a clearly felt full-scale
    // accent while revving, rather than the old weak/brief isolated buzz.
    for(std::uint64_t ms=0;ms<1000;ms+=25){
        const auto texture=chainsawRevHapticTexture(ms,false);
        CHECK(texture.amplitude>=.89f&&texture.amplitude<=.92f);
        CHECK(texture.frequencyHz>=150.f&&texture.frequencyHz<=190.f);
    }
    CHECK(chainsawRevHapticTexture(500,true).amplitude==1.f);
    CHECK(chainsawRevHapticTexture(500,true).frequencyHz==190.f);
    const XInputHapticSignal native{1.f,60.f,true};
    const XInputHapticSignal between{.90f,166.f,true};
    const XInputHapticSignal full{1.f,190.f,true};
    CHECK(prioritizeRightChainsawRevHaptic(native,between,true).amplitude==.90f);
    CHECK(prioritizeRightChainsawRevHaptic(native,full,true).frequencyHz==190.f);
    CHECK(prioritizeRightChainsawRevHaptic(native,between,false).frequencyHz==60.f);
    CHECK(prioritizeRightChainsawRevHaptic(native,{},true).frequencyHz==60.f);
    return 0;
}
