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
    CHECK(std::abs(center.z+1.1f)<.001f);
    CHECK(std::abs(center.y-1.53f)<.001f);
    CHECK(std::abs(chainsawZoneQuadWidthMeters-chainsawZoneQuadHeightMeters)<.001f);
    CHECK(chainsawZoneQuadWidthMeters<1.5f);
    CHECK(std::abs(chainsawSingleSwipeDistanceMeters-.32f)<.001f);
    CHECK(chainsawPhysicalMotionFastHz==45);
    CHECK(chainsawZoneGuidePulseFrequencyHz==150.f);
    CHECK(!chainsawZoneOutsidePulseOn(.68f,0));
    CHECK(chainsawZoneOutsidePulseOn(.69f,0));
    CHECK(!chainsawZoneOutsidePulseOn(.90f,160));
    CHECK(chainsawZoneOutsidePulseOn(.90f,360));
    CHECK(gripZoneOutlineAlpha(.69f,0)>120);
    CHECK(gripZoneOutlineAlpha(0,.69f)>120);
    const auto handAt=[&](float x,float y,float depth){
        const auto r=depth/1.10f;
        return ChainsawVec3{head.x+(center.x+x-head.x)*r,
                            head.y+(center.y+y-head.y)*r,
                            head.z+(center.z-head.z)*r};
    };
    const auto near=projectChainsawHandToOval(
        head,handAt(.2f,0,.48f),center,forward,right,up);
    const auto far=projectChainsawHandToOval(
        head,handAt(.2f,0,.9f),center,forward,right,up);
    CHECK(near.valid&&far.valid);
    CHECK(std::abs(near.normalizedRadius-far.normalizedRadius)<.001f);
    CHECK(!projectChainsawHandToOval(head,handAt(0,0,-.4f),
        center,forward,right,up).valid);
    auto sample=[&](ChainsawExperimentState& state,bool native,
                    bool trigger,bool tracked,float x,float y,float depth,
                    std::uint64_t ms){
        const auto p=handAt(x,y,depth);
        return updateChainsawExperiment(
            state,ChainsawExperimentalMode::GripPull,native,trigger,tracked,
            p.x,p.y,p.z,true,0,0,-1,ms,300,32,head,0.f,true,
            forward,right,up);
    };
    // Requires observed OUTSIDE then INSIDE while Trigger held.
    ChainsawExperimentState free{};
    CHECK(!sample(free,true,true,true,.8f,0,.48f,100).armed);
    CHECK(free.seenOutside&&!free.cutPathLocked);
    auto armed=sample(free,true,true,true,.20f,0,.48f,140);
    CHECK(armed.started&&armed.armed&&free.cutPathLocked&&free.latchedGrip);
    CHECK(free.progress==0.f&&free.strokeTravelMeters==0.f);
    // Motion OUTSIDE the circle, and moving vertically/off the initial
    // line, MUST accelerate and count exactly as motion inside did.
    auto moving=sample(free,true,true,true,.34f,0,.48f,180);
    CHECK(moving.armed&&moving.progress>0.f&&moving.targetHz<300);
    const float a=free.progress;
    moving=sample(free,true,true,true,.54f,.10f,.48f,220);
    CHECK(moving.progress>a&&moving.targetHz<300);
    const float b=free.progress;
    // Hand is now far outside the original circle, yet continues cutting.
    moving=sample(free,true,true,true,.75f,.22f,.48f,260);
    CHECK(moving.armed&&moving.progress>b&&moving.targetHz<300);
    const float c=free.progress;
    moving=sample(free,true,true,true,.91f,.40f,.48f,300);
    CHECK(moving.progress>c&&moving.targetHz<300);
    moving=sample(free,true,true,true,1.07f,.58f,.48f,340);
    CHECK(moving.completed&&moving.targetHz==30);
    CHECK(free.oneSwipeCommitted&&free.progress==1.f);
    CHECK(sample(free,true,true,false,1.20f,.70f,.48f,380).targetHz==30);
    CHECK(sample(free,true,false,false,1.20f,.70f,.48f,420).targetHz==30);
    CHECK(!sample(free,false,false,true,1.20f,.70f,.48f,460).armed);
    CHECK(!free.cutPathLocked&&!free.oneSwipeCommitted);

    // Pausing anywhere after activation drops near idle; no ring check.
    ChainsawExperimentState pause{};
    CHECK(!sample(pause,true,true,true,.80f,0,.48f,500).armed);
    CHECK(sample(pause,true,true,true,.20f,0,.48f,540).armed);
    CHECK(sample(pause,true,true,true,.39f,0,.48f,580).targetHz<300);
    auto p=sample(pause,true,true,true,.39f,0,.48f,620);
    CHECK(p.targetHz>45);
    CHECK(pause.cutPathLocked);
    // Trigger release is idle; the next press fast-forwards ANYWHERE,
    // including with tracking absent and the hand outside the circle.
    CHECK(sample(pause,true,false,true,1.f,0,.48f,660).targetHz==300);
    CHECK(sample(pause,true,true,false,1.f,0,.48f,700).targetHz==30);
    CHECK(pause.fastFinishLatched&&pause.triggerRestarts==1);
    CHECK(sample(pause,true,false,false,1.f,0,.48f,740).targetHz==30);
    CHECK(!sample(pause,false,false,false,1.f,0,.48f,780).armed);

    // Same second press can happen even before activation.
    ChainsawExperimentState skip{};
    CHECK(!sample(skip,true,true,true,.80f,0,.48f,800).armed);
    CHECK(!sample(skip,true,false,true,.80f,0,.48f,840).armed);
    CHECK(sample(skip,true,true,false,.80f,0,.48f,880).targetHz==30);
    CHECK(!skip.cutPathLocked&&skip.fastFinishLatched);
    CHECK(!sample(skip,false,false,false,0,0,.48f,920).armed);

    // No release or re-arming merely by leaving/re-entering the circle.
    ChainsawExperimentState leave{};
    CHECK(!sample(leave,true,true,true,.8f,0,.48f,1000).armed);
    CHECK(sample(leave,true,true,true,.2f,0,.48f,1040).armed);
    CHECK(sample(leave,true,true,true,.8f,0,.48f,1080).armed);
    CHECK(leave.cutPathLocked&&!leave.fastFinishLatched);
    CHECK(sample(leave,true,true,true,.2f,0,.48f,1120).armed);
    CHECK(leave.cutPathLocked&&!leave.fastFinishLatched);
    CHECK(!sample(leave,false,true,true,.2f,0,.48f,1160).armed);
    // Starting inside without seeing an outside Trigger-held frame cannot arm.
    ChainsawExperimentState inside{};
    CHECK(!sample(inside,true,true,true,0,0,.48f,1200).armed);
    CHECK(!sample(inside,true,true,true,.1f,0,.48f,1240).armed);
    CHECK(!inside.cutPathLocked);
    // Old separate Pointer Focus kept unchanged.
    ChainsawExperimentState ray{};
    auto start=updateChainsawExperiment(ray,ChainsawExperimentalMode::PointerFocus,
        true,false,true,0,0,0,true,0,0,-1,100,300,32,head);
    CHECK(start.started);
    auto moved=updateChainsawExperiment(ray,ChainsawExperimentalMode::PointerFocus,
        true,false,true,0,0,0,true,.5f,0,-.8660254f,115,300,32,head);
    CHECK(moved.completed&&moved.targetHz==30);
    return 0;
}
