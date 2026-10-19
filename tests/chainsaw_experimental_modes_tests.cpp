#include "../src/openxr/ChainsawExperimentalModesPolicy.h"
#include <cmath>
#include <limits>
#define CHECK(x) do { if(!(x)) return __LINE__; } while(false)

int main(){
    using namespace kharvox;
    // Mode selection cannot arm without an actual DOOM chainsaw execution.
    for(auto mode:{ChainsawExperimentalMode::GripPull,
                    ChainsawExperimentalMode::Circular,
                    ChainsawExperimentalMode::PointerFocus}){
        ChainsawExperimentState inactive{};
        const auto a=updateChainsawExperiment(inactive,mode,false,false,true,
            0.f,0.f,0.f,true,0.f,0.f,-1.f,10,300,32);
        CHECK(!a.armed&&a.targetHz==300);
    }

    // GRIP: a fresh physical grab, after the execution has started.
    ChainsawExperimentState grip{};
    auto noGrip=updateChainsawExperiment(grip,ChainsawExperimentalMode::GripPull,
        true,false,true,0.f,0.f,0.f,true,0,0,-1,100,300,32);
    CHECK(!noGrip.armed&&noGrip.targetHz==300);
    auto grabbed=updateChainsawExperiment(grip,ChainsawExperimentalMode::GripPull,
        true,true,true,0.f,0.f,0.f,true,0,0,-1,110,300,32);
    CHECK(grabbed.armed&&grabbed.started&&grabbed.progress==0);
    auto partial=updateChainsawExperiment(grip,ChainsawExperimentalMode::GripPull,
        true,true,true,0.f,-.15f,0.f,true,0,0,-1,120,300,32);
    CHECK(partial.progress>.25f&&partial.targetHz<300&&partial.targetHz>30);
    auto released=updateChainsawExperiment(grip,ChainsawExperimentalMode::GripPull,
        true,false,true,0.f,-.29f,0.f,true,0,0,-1,130,300,32);
    CHECK(released.progress==partial.progress);
    auto complete=updateChainsawExperiment(grip,ChainsawExperimentalMode::GripPull,
        true,true,true,.32f,0.f,0.f,true,0,0,-1,145,300,32);
    CHECK(complete.completed&&complete.targetHz==30);
    auto returned=updateChainsawExperiment(grip,ChainsawExperimentalMode::GripPull,
        true,true,true,0.f,0.f,0.f,true,0,0,-1,160,300,32);
    CHECK(returned.completed&&returned.targetHz==30);
    auto nativeExit=updateChainsawExperiment(grip,ChainsawExperimentalMode::GripPull,
        false,true,true,0.f,0.f,0.f,true,0,0,-1,170,300,32);
    CHECK(!nativeExit.armed&&nativeExit.targetHz==300);

    ChainsawExperimentState noFreshGrip{};
    auto alreadyHeld=updateChainsawExperiment(noFreshGrip,
        ChainsawExperimentalMode::GripPull,true,true,true,
        0.f,0.f,0.f,true,0,0,-1,100,300,32);
    CHECK(!alreadyHeld.armed);
    (void)updateChainsawExperiment(noFreshGrip,ChainsawExperimentalMode::GripPull,
        true,false,true,0,0,0,true,0,0,-1,110,300,32);
    CHECK(updateChainsawExperiment(noFreshGrip,ChainsawExperimentalMode::GripPull,
        true,true,true,0,0,0,true,0,0,-1,120,300,32).armed);

    // CIRCLE: hand leaves the virtual center, then traces ONE 155 degree arc
    // clockwise or counterclockwise. Jitter inside the ring is not progress.
    ChainsawExperimentState circle{};
    const auto begin=updateChainsawExperiment(circle,ChainsawExperimentalMode::Circular,
        true,false,true,0.f,0.f,0.f,true,0,0,-1,100,300,32);
    CHECK(begin.started&&begin.progress==0.f);
    (void)updateChainsawExperiment(circle,ChainsawExperimentalMode::Circular,
        true,false,true,.12f,0.f,0.f,true,0,0,-1,110,300,32);
    const auto point=[&](float degrees,std::uint64_t ms){
        constexpr float rad=.0174532925199433f;
        return updateChainsawExperiment(circle,ChainsawExperimentalMode::Circular,
            true,false,true,.12f*std::cos(degrees*rad),
            .12f*std::sin(degrees*rad),0.f,true,0,0,-1,ms,300,32);
    };
    auto c1=point(20.f,120);
    auto c2=point(42.f,135);
    auto c3=point(64.f,150);
    auto c4=point(86.f,165);
    auto c5=point(108.f,180);
    auto c6=point(130.f,195);
    auto c7=point(152.f,210);
    auto c8=point(174.f,225); // >155 degrees across one arc
    CHECK(c1.progress>0.f&&c2.progress>c1.progress);
    CHECK(c4.progress>c3.progress&&c7.progress<c8.progress
        &&c8.completed&&c8.targetHz==30);
    CHECK(point(100.f,230).completed); // backtracking cannot reverse progress
    ChainsawExperimentState circleIdle{};
    (void)updateChainsawExperiment(circleIdle,ChainsawExperimentalMode::Circular,
        true,false,true,0,0,0,true,0,0,-1,100,300,32);
    auto centerJitter=updateChainsawExperiment(circleIdle,
        ChainsawExperimentalMode::Circular,true,false,true,
        .007f,.008f,0.f,true,0,0,-1,130,300,32);
    CHECK(centerJitter.progress==0.f);

    // POINTER: a ray angle shift, not headset rotation or physical travel.
    ChainsawExperimentState ray{};
    auto p0=updateChainsawExperiment(ray,ChainsawExperimentalMode::PointerFocus,
        true,false,true,0,0,0,true,0,0,-1,100,300,32);
    CHECK(p0.armed&&p0.started);
    auto p1=updateChainsawExperiment(ray,ChainsawExperimentalMode::PointerFocus,
        true,false,true,0,0,0,true,.20f,0,-.9797959f,120,300,32);
    CHECK(p1.progress>.15f&&p1.targetHz<300);
    auto p2=updateChainsawExperiment(ray,ChainsawExperimentalMode::PointerFocus,
        true,false,true,0,0,0,true,.48f,0,-.877268f,140,300,32);
    CHECK(p2.completed&&p2.targetHz==30);
    auto p3=updateChainsawExperiment(ray,ChainsawExperimentalMode::PointerFocus,
        true,false,true,0,0,0,false,
        std::numeric_limits<float>::quiet_NaN(),0,0,160,300,32);
    CHECK(p3.completed);
    CHECK(!updateChainsawExperiment(ray,ChainsawExperimentalMode::PointerFocus,
        false,false,true,0,0,0,true,0,0,-1,180,300,32).armed);
    return 0;
}
