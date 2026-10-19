#include "../src/openxr/ChainsawExperimentalModesPolicy.h"
#include "../src/openxr/GripZoneMarkerPolicy.h"
#include <cmath>
#define CHECK(x) do { if(!(x)) return __LINE__; } while(false)
int main(){
    using namespace kharvox;
    const ChainsawVec3 head{0.f,1.7f,0.f};
    const ChainsawVec3 forward{0.f,0.f,-1.f};
    const auto center=singleZoneCenter(head,forward);
    CHECK(std::abs(center.z+1.05f)<.001f);
    CHECK(std::abs(center.y-1.53f)<.001f);
    CHECK(std::abs(length3(minus3(center,head)) -1.063f)<.02f);
    CHECK(length3(minus3(zonePosition(head,0,0),center))<.001f);
    CHECK(length3(minus3(singleZoneCenter(head,{0,0,0}),center))<.001f);
    CHECK(gripZoneOutlineAlpha(0,0)==0);
    CHECK(gripZoneOutlineAlpha(1,1)==0);
    CHECK(gripZoneOutlineAlpha(.69f,0)>120);
    CHECK(gripZoneOutlineAlpha(0,.69f)>120);

    // Trigger normally starts the native DOOM execution BEFORE our first
    // hand control sample. Crucially do NOT require a second Trigger press.
    ChainsawExperimentState st{};
    auto update=[&](bool native,bool trigger,bool tracked,float offset,std::uint64_t ms){
        return updateChainsawExperiment(st,ChainsawExperimentalMode::GripPull,
            native,trigger,tracked,
            center.x+offset,center.y,center.z,true,0,0,-1,
            ms,480,32,head,0.f,true,forward);
    };
    CHECK(!update(false,true,true,.48f,100).armed);
    const auto start=update(true,true,true,.48f,110);
    CHECK(start.targetHz==480&&!start.armed&&st.seenOutside);
    const auto approach=update(true,true,true,.36f,125);
    CHECK(!approach.armed);
    const auto entry=update(true,true,true,.27f,140);
    CHECK(entry.started&&entry.armed&&entry.enteredZone);
    CHECK(entry.targetHz<480); // first crossing gets immediate momentum
    // Inside: any direction of real tracked movement controls CURRENT speed.
    const auto cut=update(true,true,true,.12f,160);
    CHECK(cut.armed&&cut.targetHz<=entry.targetHz);
    CHECK(cut.progress>0.5f);
    // Tracking dropout or leaving the circle removes the override, not
    // a super-slow 480Hz kill while the hand is missing or outside.
    const auto lost=update(true,true,false,.12f,175);
    CHECK(!lost.armed);
    const auto resume=update(true,true,true,.05f,190);
    CHECK(resume.armed);
    const auto off=update(true,true,true,.41f,210);
    CHECK(!off.armed);
    const auto reenter=update(true,true,true,.25f,225);
    CHECK(reenter.armed&&!reenter.started);
    const auto release=update(true,false,true,.19f,240);
    CHECK(!release.armed);
    CHECK(!update(false,true,true,.25f,250).armed);
    CHECK(!st.active);

    // Native execution can begin INSIDE the circle. Exit then re-enter
    // with Trigger held. This is not the retired rotating-hand Saw mod.
    ChainsawExperimentState inside{};
    auto sample=[&](float d,int ms){
        return updateChainsawExperiment(inside,ChainsawExperimentalMode::GripPull,
            true,true,true,center.x+d,center.y,center.z,
            true,0,0,-1,ms,300,32,head,0,true,forward);
    };
    CHECK(!sample(.08f,100).armed);
    CHECK(!sample(.42f,115).armed);
    CHECK(sample(.22f,130).armed);
    CHECK(sample(.12f,145).targetHz<300);
    CHECK(!updateChainsawExperiment(inside,ChainsawExperimentalMode::GripPull,
        false,false,true,center.x,center.y,center.z,
        true,0,0,-1,160,300,32,head,0,true,forward).armed);
    // Pointer remains a separate independent test, no rotary mode restored.
    ChainsawExperimentState ray{};
    const auto begin=updateChainsawExperiment(ray,ChainsawExperimentalMode::PointerFocus,
        true,false,true,0,0,0,true,0,0,-1,100,480,32,head);
    CHECK(begin.started);
    const auto moved=updateChainsawExperiment(ray,ChainsawExperimentalMode::PointerFocus,
        true,false,true,0,0,0,true,.5f,0,-.8660254f,115,480,32,head);
    CHECK(moved.completed&&moved.targetHz==30);
    return 0;
}
