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
    ChainsawExperimentState st{};
    CHECK(!sample(st,false,true,true,.70f,0,.48f,100).armed);
    const auto start=sample(st,true,true,true,.70f,0,.48f,110);
    CHECK(!start.armed&&st.seenOutside);
    CHECK(!sample(st,true,true,true,.59f,0,.48f,125).armed);
    const auto entry=sample(st,true,true,true,.25f,0,.48f,140);
    CHECK(entry.started&&entry.armed&&entry.enteredZone);
    CHECK(entry.targetHz<480);
    const auto cut=sample(st,true,true,true,.1f,0,.48f,160);
    CHECK(cut.armed&&cut.progress>.5f);
    CHECK(!sample(st,true,true,false,.1f,0,.48f,175).armed);
    CHECK(sample(st,true,true,true,.08f,0,.48f,190).armed);
    // Leaving the visible oval AFTER the initial arming MUST keep motion
    // speed control active. No second entrance is required.
    const auto outside=sample(st,true,true,true,.70f,0,.48f,210);
    CHECK(outside.armed);
    CHECK(outside.hoveredZone==-1);
    const auto outsideStroke=sample(st,true,true,true,.85f,0,.48f,225);
    CHECK(outsideStroke.armed&&outsideStroke.targetHz<480);
    CHECK(sample(st,true,true,true,.15f,0,.48f,240).armed);
    // Trigger release disarms even outside the oval. Re-press INSIDE
    // cannot bypass the original outside-to-inside arming requirement.
    CHECK(!sample(st,true,false,true,.15f,0,.48f,255).armed);
    CHECK(!st.seenOutside&&!st.latchedGrip);
    CHECK(!sample(st,true,true,true,.15f,0,.48f,270).armed);
    CHECK(!sample(st,true,true,true,.80f,0,.48f,285).armed);
    CHECK(sample(st,true,true,true,.15f,0,.48f,300).armed);
    CHECK(!sample(st,false,true,true,.15f,0,.48f,315).armed);
    CHECK(!st.active);

    // Starting inside MUST require Trigger held OUTSIDE, then re-entry.
    ChainsawExperimentState inside{};
    CHECK(!sample(inside,true,true,true,.12f,0,.48f,100).armed);
    CHECK(!inside.seenOutside);
    CHECK(!sample(inside,true,false,true,.80f,0,.48f,110).armed);
    CHECK(!inside.seenOutside);  // No trigger: cannot pre-arm.
    CHECK(!sample(inside,true,true,true,.80f,0,.48f,120).armed);
    CHECK(inside.seenOutside);
    CHECK(sample(inside,true,true,true,.2f,0,.48f,135).armed);
    CHECK(!sample(inside,true,false,true,.2f,0,.48f,150).armed);
    CHECK(!inside.seenOutside&&!inside.latchedGrip);
    CHECK(!sample(inside,true,true,true,.18f,0,.48f,165).armed);
    CHECK(!sample(inside,true,true,true,.78f,0,.48f,180).armed);
    CHECK(sample(inside,true,true,true,.15f,0,.48f,195).armed);
    CHECK(!sample(inside,false,true,true,.15f,0,.48f,210).armed);

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
