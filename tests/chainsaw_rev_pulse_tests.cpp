#include "../src/openxr/ChainsawExperimentalModesPolicy.h"
#include <cmath>
#include <limits>
#define CHECK(x) do{ if(!(x))return __LINE__; }while(false)
int main(){
    using namespace kharvox;
    constexpr auto mode=ChainsawExperimentalMode::TriggerRevPulses;
    CHECK(chainsawModeValid(9));
    CHECK(!chainsawModeUsesCircle(mode));
    CHECK(!chainsawModeStrokeFinish(mode));
    CHECK(chainsawModeIdleHz(mode,300)==200);
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
    // X6 becomes active immediately when DOOM confirms a native chainsaw
    // execution, whether the tracked hand is inside or outside the old ring.
    auto start=sample(st,true,1.f,.20f,100);
    CHECK(start.armed&&start.started&&start.targetHz==200);
    CHECK(st.cutPathLocked&&!st.seenOutside&&!st.revReady);
    CHECK(st.revCount==0);
    CHECK(sample(st,true,1.f,.20f,170).targetHz==200);
    CHECK(sample(st,true,1.f,.80f,1500).targetHz==200);
    CHECK(st.revCount==0&&!st.revPullEdge);
    // Partial trigger release is not enough. A real release is mandatory.
    sample(st,true,.23f,.20f,1530);
    CHECK(!st.revReady);
    sample(st,true,.90f,.20f,1560);
    CHECK(st.revCount==0);
    sample(st,true,.05f,.20f,1600);
    CHECK(st.revReady&&st.revReleaseEdge);
    sample(st,true,.20f,.20f,1620);
    CHECK(st.revReady&&st.revCount==0);
    // Fresh pull begins a single bounded rev, with a smoothed attack.
    auto light=sample(st,true,.35f,.20f,1640);
    CHECK(light.armed&&st.revPullEdge&&!st.revReady&&st.revCount==1);
    CHECK(light.targetHz==200); // starts at baseline, no abrupt jump
    auto rising=sample(st,true,.35f,.20f,1660);
    CHECK(rising.targetHz<200&&rising.targetHz>30);
    auto peak=sample(st,true,.35f,.20f,1690);
    CHECK(peak.targetHz<=rising.targetHz);
    // Holding or hand motion must not renew the burst.
    for(std::uint64_t ms: {1710ull,1790ull,1900ull,2200ull}){
        auto held=sample(st,true,.35f,.80f,ms);
        CHECK(st.revCount==1&&!st.revPullEdge);
        CHECK(!held.completed&&!st.fastFinishLatched);
    }
    CHECK(st.lastRequestedHz==200);
    // Full-depth pull after release approaches the fastest 30Hz request
    // smoothly, then fades back to the 200Hz baseline.
    sample(st,true,.05f,.80f,2300);
    CHECK(st.revReleaseEdge&&st.revReady);
    auto medium=sample(st,true,.40f,.80f,2320);
    CHECK(st.revCount==2&&st.revPullEdge&&medium.targetHz==200);
    sample(st,true,1.f,.80f,2340);
    CHECK(st.revPeakPressure==1.f&&st.revStrength==1.f);
    auto maximum=sample(st,true,1.f,.80f,2370);
    CHECK(maximum.targetHz==30);
    CHECK(sample(st,true,1.f,.80f,2570).targetHz>=30);
    auto slowing=sample(st,true,1.f,.80f,2630);
    CHECK(slowing.targetHz>30&&slowing.targetHz<200);
    CHECK(sample(st,true,1.f,.80f,2750).targetHz==200);
    CHECK(st.revCount==2&&!st.fastFinishLatched);
    // Rapid tapping is valid without positional tracking or ring arming.
    sample(st,true,.01f,.80f,2800,false,false);
    CHECK(st.revReady&&st.revReleaseEdge);
    auto again=sample(st,true,.85f,.80f,2820,false,false);
    CHECK(st.revPullEdge&&st.revCount==3&&again.armed);
    CHECK(!st.fastFinishLatched&&!st.oneSwipeCommitted);
    CHECK(!sample(st,false,.85f,.80f,2860).armed);
    CHECK(st.revCount==0);
    // Cold start with lost hand and HMD tracking must still arm X6.
    ChainsawExperimentState noTracking{};
    CHECK(sample(noTracking,true,1.f,.20f,3000,false,false).armed);
    CHECK(noTracking.cutPathLocked);
    // Nonfinite analog input must never create a spurious rev.
    ChainsawExperimentState invalid{};
    sample(invalid,true,1.f,.85f,3100);
    sample(invalid,true,std::numeric_limits<float>::quiet_NaN(),.20f,3140);
    CHECK(invalid.revReady&&!invalid.revPullEdge);
    // Continuous near-maximum jaw vibration with a distinctive full-strength
    // accent at each pull; other control modes remain unchanged.
    for(std::uint64_t ms=0;ms<1000;ms+=25){
        const auto texture=chainsawRevHapticTexture(ms,false);
        CHECK(texture.amplitude>=.96f&&texture.amplitude<=1.f);
        CHECK(texture.frequencyHz>=150.f&&texture.frequencyHz<=190.f);
    }
    CHECK(chainsawRevHapticTexture(500,true).amplitude==1.f);
    CHECK(chainsawRevHapticTexture(500,true).frequencyHz==190.f);
    return 0;
}
