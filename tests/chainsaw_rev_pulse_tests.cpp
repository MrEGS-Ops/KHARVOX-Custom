#include "../src/openxr/ChainsawExperimentalModesPolicy.h"
#include <cmath>
#include <limits>
#define CHECK(x) do{ if(!(x))return __LINE__; }while(false)
int main(){
    using namespace kharvox;
    constexpr auto mode=ChainsawExperimentalMode::TriggerRevPulses;
    CHECK(chainsawModeValid(9));
    CHECK(chainsawModeUsesCircle(mode));
    CHECK(!chainsawModeStrokeFinish(mode));
    CHECK(chainsawModeIdleHz(mode,300)==200);
    CHECK(std::abs(chainsawZoneQuadWidthMeters-chainsawZoneQuadHeightMeters)<.001f);
    CHECK(chainsawZoneQuadWidthMeters>=1.4f);
    const ChainsawVec3 head{0,1.7f,0},forward{0,0,-1};
    const auto center=singleZoneCenter(head,forward);
    auto sample=[&](ChainsawExperimentState& st,bool native,
                    float pressure,float px,std::uint64_t ms,bool tracking=true){
        const float ratio=.48f/1.1f;
        const ChainsawVec3 hand{
            (center.x+px-head.x)*ratio,
            head.y+(center.y-head.y)*ratio,
            (center.z-head.z)*ratio
        };
        return updateChainsawExperiment(st,mode,native,pressure>.55f,tracking,
            hand.x,hand.y,hand.z,true,0,0,-1,
            ms,300,32,head,0.f,true,forward,{1,0,0},{0,1,0},pressure);
    };
    ChainsawExperimentState st{};
    // Initial native execution is already triggered. OUTSIDE -> INSIDE
    // activates once. The initial held Trigger does not count as a rev.
    auto outside=sample(st,true,1.f,.83f,100);
    CHECK(!outside.armed&&st.seenOutside);
    auto enter=sample(st,true,1.f,.20f,140);
    CHECK(enter.armed&&enter.started&&enter.targetHz==200);
    CHECK(st.cutPathLocked&&!st.revReady&&st.revCount==0);
    CHECK(sample(st,true,1.f,.20f,170).targetHz==200);
    // Holding across several seconds remains 200 Hz. No implicit repeat.
    CHECK(sample(st,true,1.f,.20f,1500).targetHz==200);
    CHECK(st.revCount==0&&!st.revPullEdge);
    // A half release isn't sufficient; must go BELOW .18.
    CHECK(sample(st,true,.23f,.20f,1530).targetHz==200);
    CHECK(!st.revReady);
    CHECK(sample(st,true,.90f,.20f,1560).targetHz==200);
    CHECK(st.revCount==0);
    CHECK(sample(st,true,.05f,.20f,1600).targetHz==200);
    CHECK(st.revReady&&st.revReleaseEdge);
    CHECK(!sample(st,true,.20f,.20f,1620).completed);
    CHECK(st.revCount==0&&st.revReady);
    // Light pull gives one modest speed burst.
    auto light=sample(st,true,.35f,.20f,1640);
    CHECK(light.armed&&st.revPullEdge&&!st.revReady&&st.revCount==1);
    CHECK(light.targetHz<200&&light.targetHz>30);
    // Hold cannot generate another pulse or extend the burst.
    for(std::uint64_t ms: {1660ull,1700ull,1780ull,1900ull,2200ull}){
        auto held=sample(st,true,.35f,.80f,ms);
        CHECK(st.revCount==1&&!st.revPullEdge);
        CHECK(!held.completed&&!st.fastFinishLatched);
    }
    CHECK(st.lastRequestedHz==200);
    // Full-depth pull after a REAL release goes to max speed, then
    // even holding 100% eventually returns to 200Hz.
    sample(st,true,.05f,.80f,2300);
    CHECK(st.revReleaseEdge&&st.revReady);
    auto medium=sample(st,true,.40f,.80f,2320);
    CHECK(st.revCount==2&&st.revPullEdge&&medium.targetHz<200);
    auto deep=sample(st,true,1.f,.80f,2340);
    CHECK(st.revCount==2&&!st.revPullEdge&&st.revPeakPressure==1.f);
    CHECK(deep.targetHz==30&&st.revStrength==1.f);
    CHECK(sample(st,true,1.f,.80f,2580).targetHz>30);
    CHECK(sample(st,true,1.f,.80f,2670).targetHz==200);
    CHECK(st.revCount==2&&!st.fastFinishLatched);
    // Fast physical tapping: each genuine release then new squeeze
    // restarts a burst and increases the rev counter. No motion needed.
    sample(st,true,.01f,.80f,2700,false);
    CHECK(st.revReady&&st.revReleaseEdge);
    auto again=sample(st,true,.85f,.80f,2720,false);
    CHECK(st.revPullEdge&&st.revCount==3&&again.targetHz<200);
    CHECK(!st.fastFinishLatched&&!st.oneSwipeCommitted);
    CHECK(sample(st,false,.85f,.80f,2760).armed==false);
    CHECK(st.revCount==0);
    // A non-finite pressure must be sanitized, never injecting a rev.
    ChainsawExperimentState invalid{};
    sample(invalid,true,1.f,.85f,3000);
    sample(invalid,true,1.f,.20f,3040);
    sample(invalid,true,std::numeric_limits<float>::quiet_NaN(),.20f,3080);
    CHECK(invalid.revReady&&!invalid.revPullEdge);
    return 0;
}
