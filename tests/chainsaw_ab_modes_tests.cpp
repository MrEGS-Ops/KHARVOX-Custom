#include "../src/openxr/ChainsawExperimentalModesPolicy.h"
#include <cmath>
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){
    using namespace kharvox;
    CHECK(chainsawModeValid(1)&&chainsawModeValid(3));
    for(int i=4;i<=8;++i)CHECK(chainsawModeValid(i));
    CHECK(!chainsawModeValid(2)&&!chainsawModeValid(9));
    CHECK(chainsawModeUsesCircle(4)&&chainsawModeUsesCircle(7));
    CHECK(!chainsawModeUsesCircle(8)&&!chainsawModeUsesCircle(3));
    CHECK(chainsawModeSwipeTarget(4)<chainsawModeSwipeTarget(6));
    CHECK(chainsawModeSwipeTarget(7)<chainsawModeSwipeTarget(4));
    CHECK(!chainsawModeStrokeFinish(5)&&!chainsawModeStrokeFinish(8));
    CHECK(chainsawModeIdleHz(4,300)==180);
    CHECK(chainsawModeIdleHz(5,300)==200);
    CHECK(chainsawModeIdleHz(7,300)==155);
    const ChainsawVec3 head{0,1.7f,0}, forward{0,0,-1};
    const auto center=singleZoneCenter(head,forward);
    auto tick=[&](ChainsawExperimentState& st,ChainsawExperimentalMode mode,
                  bool native,bool trigger,bool tracking,
                  float screenX,std::uint64_t ms){
        const float depth=.48f,ratio=depth/1.1f;
        const ChainsawVec3 hand{
            (center.x+screenX-head.x)*ratio,
            head.y+(center.y-head.y)*ratio,
            (center.z-head.z)*ratio
        };
        return updateChainsawExperiment(st,mode,native,trigger,tracking,
            hand.x,hand.y,hand.z,true,0,0,-1,ms,300,32,
            head,0.f,true,forward,{1,0,0},{0,1,0});
    };
    // X1: one 24cm free-space swipe (two samples after entry) -> 30Hz.
    ChainsawExperimentState shortCut{};
    CHECK(!tick(shortCut,ChainsawExperimentalMode::ShortSwipe,
        true,true,true,.8f,100).armed);
    const auto armShort=tick(shortCut,ChainsawExperimentalMode::ShortSwipe,
        true,true,true,.2f,140);
    CHECK(armShort.armed&&armShort.started&&armShort.targetHz==180);
    CHECK(shortCut.phaseId==2);
    CHECK(tick(shortCut,ChainsawExperimentalMode::ShortSwipe,
        true,true,true,.42f,180).targetHz<=180);
    CHECK(tick(shortCut,ChainsawExperimentalMode::ShortSwipe,
        true,true,true,.64f,220).armed);
    const auto shortDone=tick(shortCut,ChainsawExperimentalMode::ShortSwipe,
        true,true,true,.85f,260);
    CHECK(shortDone.completed&&shortDone.targetHz==30&&shortCut.phaseId==3);
    CHECK(shortCut.strokeTravelMeters>=.24f);
    CHECK(!tick(shortCut,ChainsawExperimentalMode::ShortSwipe,
        false,false,true,.9f,300).armed);
    CHECK(shortCut.strokeTravelMeters==0.f);

    // X2: full hand motion controls speed but even >32cm does NOT
    // auto-finish. Second press works anywhere and is the fallback.
    ChainsawExperimentState throttle{};
    CHECK(!tick(throttle,ChainsawExperimentalMode::ContinuousThrottle,
        true,true,true,.8f,400).armed);
    CHECK(tick(throttle,ChainsawExperimentalMode::ContinuousThrottle,
        true,true,true,.2f,440).targetHz==200);
    CHECK(tick(throttle,ChainsawExperimentalMode::ContinuousThrottle,
        true,true,true,.4f,480).targetHz<200);
    CHECK(tick(throttle,ChainsawExperimentalMode::ContinuousThrottle,
        true,true,true,.65f,520).armed);
    auto running=tick(throttle,ChainsawExperimentalMode::ContinuousThrottle,
        true,true,true,.95f,560);
    CHECK(running.armed&&!running.completed);
    CHECK(throttle.strokeTravelMeters>.32f&&!throttle.oneSwipeCommitted);
    auto stopped=tick(throttle,ChainsawExperimentalMode::ContinuousThrottle,
        true,true,true,.95f,600);
    CHECK(stopped.targetHz>running.targetHz);
    CHECK(tick(throttle,ChainsawExperimentalMode::ContinuousThrottle,
        true,false,true,.95f,640).targetHz==300);
    const auto skipped=tick(throttle,ChainsawExperimentalMode::ContinuousThrottle,
        true,true,false,.95f,680);
    CHECK(skipped.completed&&skipped.targetHz==30&&throttle.fastFinishLatched);
    CHECK(!tick(throttle,ChainsawExperimentalMode::ContinuousThrottle,
        false,true,true,.95f,720).armed);

    // X3: DOOM-native start, then 220ms of approximation only AFTER arm,
    // then free-space motion; 28cm threshold ends the physical segment.
    ChainsawExperimentState three{};
    CHECK(!tick(three,ChainsawExperimentalMode::ThreePhase,
        true,true,true,.8f,800).armed);
    CHECK(tick(three,ChainsawExperimentalMode::ThreePhase,
        true,true,true,.2f,840).targetHz==180);
    CHECK(tick(three,ChainsawExperimentalMode::ThreePhase,
        true,true,true,.36f,880).targetHz==180);
    CHECK(tick(three,ChainsawExperimentalMode::ThreePhase,
        true,true,true,.50f,1060).armed);
    CHECK(tick(three,ChainsawExperimentalMode::ThreePhase,
        true,true,true,.66f,1100).targetHz<300);
    CHECK(three.phaseId==2);
    CHECK(tick(three,ChainsawExperimentalMode::ThreePhase,
        true,true,true,.92f,1140).armed);
    const auto threeDone=tick(three,ChainsawExperimentalMode::ThreePhase,
        true,true,true,1.12f,1180);
    CHECK(threeDone.targetHz==30&&threeDone.completed);

    // X4: fixed 155Hz physical-cut segment; change in motion does not
    // change engine clock, unlike X1/X2. Only cut-completion requests 30.
    ChainsawExperimentState impact{};
    CHECK(!tick(impact,ChainsawExperimentalMode::ImpactCut,
        true,true,true,.8f,1300).armed);
    CHECK(tick(impact,ChainsawExperimentalMode::ImpactCut,
        true,true,true,.2f,1340).targetHz==155);
    CHECK(tick(impact,ChainsawExperimentalMode::ImpactCut,
        true,true,true,.43f,1380).targetHz==155);
    CHECK(tick(impact,ChainsawExperimentalMode::ImpactCut,
        true,true,true,.60f,1420).targetHz==155);
    CHECK(tick(impact,ChainsawExperimentalMode::ImpactCut,
        true,true,true,.80f,1460).targetHz==30);
    CHECK(impact.oneSwipeCommitted);

    // X5: no circle, 40Hz immediately; hand motion only haptics,
    // and any second press still fast-finishes at 30Hz.
    ChainsawExperimentState fast{};
    const auto nativeFast=tick(fast,ChainsawExperimentalMode::NativeFast,
        true,true,false,0,1600);
    CHECK(nativeFast.started&&nativeFast.armed&&nativeFast.targetHz==40);
    CHECK(fast.cutPathLocked&&fast.phaseId==2);
    CHECK(!fast.oneSwipeCommitted);
    CHECK(tick(fast,ChainsawExperimentalMode::NativeFast,
        true,true,true,.9f,1640).targetHz==40);
    CHECK(tick(fast,ChainsawExperimentalMode::NativeFast,
        true,true,true,.4f,1680).targetHz==40);
    CHECK(!fast.oneSwipeCommitted);
    CHECK(tick(fast,ChainsawExperimentalMode::NativeFast,
        true,false,true,.4f,1720).targetHz==40);
    CHECK(tick(fast,ChainsawExperimentalMode::NativeFast,
        true,true,true,.4f,1760).targetHz==30);
    CHECK(!tick(fast,ChainsawExperimentalMode::NativeFast,
        false,true,true,.4f,1800).armed);
    // Unknown modes have safe legacy baseline normalization.
    ChainsawExperimentState invalid{};
    CHECK(!tick(invalid,static_cast<ChainsawExperimentalMode>(98),
        true,true,true,.8f,1900).armed);
    CHECK(invalid.mode==ChainsawExperimentalMode::GripPull);
    return 0;
}
