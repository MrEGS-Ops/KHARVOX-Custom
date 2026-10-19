#include "../src/openxr/ChainsawExperimentalModesPolicy.h"
#include <cmath>
#include <limits>
#define CHECK(x) do {if(!(x)) return __LINE__;}while(false)
int main(){
    using namespace kharvox;
    const ChainsawVec3 h{0.f,1.7f,0.f};
    for(int i=0;i<4;++i){
        const auto z=zonePosition(h,0.f,i);
        CHECK(length3(minus3(z,h))<.85f);
        for(int j=0;j<i;++j)
            CHECK(length3(minus3(z,zonePosition(h,0.f,j)))>.30f);
    }
    // Each of the FOUR start spheres must allow a single deliberate
    // pull toward any of the OTHER THREE. Native kill and fresh Grip are
    // required. Legacy Circular mode 2 safely collapses to Grip Zones.
    for(int start=0;start<4;++start)for(int target=0;target<4;++target){
        if(start==target)continue;
        ChainsawExperimentState st{};
        const auto from=zonePosition(h,0.f,start);
        const auto to=zonePosition(h,0.f,target);
        auto invalid=updateChainsawExperiment(st,ChainsawExperimentalMode::GripPull,
            false,false,true,from.x,from.y,from.z,true,0,0,-1,0,480,32,h);
        CHECK(!invalid.armed&&invalid.targetHz==480);
        auto entered=updateChainsawExperiment(st,ChainsawExperimentalMode::GripPull,
            true,false,true,from.x,from.y,from.z,true,0,0,-1,100,480,32,h);
        CHECK(entered.hoveredZone==start&&entered.enteredZone&&!entered.armed);
        auto latch=updateChainsawExperiment(st,ChainsawExperimentalMode::GripPull,
            true,true,true,from.x,from.y,from.z,true,0,0,-1,110,480,32,h);
        CHECK(latch.armed&&latch.started&&latch.startZone==start);
        const ChainsawVec3 a={from.x+(to.x-from.x)*.30f,
            from.y+(to.y-from.y)*.30f,from.z+(to.z-from.z)*.30f};
        auto pulled=updateChainsawExperiment(st,ChainsawExperimentalMode::GripPull,
            true,true,true,a.x,a.y,a.z,true,0,0,-1,120,480,32,h);
        CHECK(pulled.targetZone==target&&pulled.progress>0.f
            &&pulled.targetHz<480);
        auto frozen=updateChainsawExperiment(st,ChainsawExperimentalMode::GripPull,
            true,false,false,0,0,0,true,0,0,-1,130,480,32,h);
        CHECK(frozen.progress==pulled.progress);
        const ChainsawVec3 b={from.x+(to.x-from.x)*.65f,
            from.y+(to.y-from.y)*.65f,from.z+(to.z-from.z)*.65f};
        (void)updateChainsawExperiment(st,ChainsawExperimentalMode::GripPull,
            true,true,true,b.x,b.y,b.z,true,0,0,-1,140,480,32,h);
        auto done=updateChainsawExperiment(st,ChainsawExperimentalMode::GripPull,
            true,true,true,to.x,to.y,to.z,true,0,0,-1,150,480,32,h);
        CHECK(done.completed&&done.targetHz==30&&done.targetZone==target);
        const auto exit=updateChainsawExperiment(st,ChainsawExperimentalMode::GripPull,
            false,true,true,to.x,to.y,to.z,true,0,0,-1,160,480,32,h);
        CHECK(!exit.armed&&exit.targetHz==480);
    }
    // A held grip on execution entry does NOT capture a zone until released.
    ChainsawExperimentState held{};
    const auto left=zonePosition(h,0.f,0);
    CHECK(!updateChainsawExperiment(held,ChainsawExperimentalMode::GripPull,
        true,true,true,left.x,left.y,left.z,true,0,0,-1,100,480,32,h).armed);
    (void)updateChainsawExperiment(held,ChainsawExperimentalMode::GripPull,
        true,false,true,left.x,left.y,left.z,true,0,0,-1,110,480,32,h);
    CHECK(updateChainsawExperiment(held,ChainsawExperimentalMode::GripPull,
        true,true,true,left.x,left.y,left.z,true,0,0,-1,120,480,32,h).armed);
    // A hand outside all zones cannot grab anything.
    ChainsawExperimentState far{};
    CHECK(!updateChainsawExperiment(far,ChainsawExperimentalMode::GripPull,
        true,true,true,9.f,1.f,4.f,true,0,0,-1,100,480,32,h).armed);
    // Yaw rotates all zones in tracking coordinates without moving anchor.
    CHECK(std::abs(zonePosition(h,1.5707963f,0).z
        -zonePosition(h,0.f,0).z)>.1f);
    // Pointer Focus remains independent, no actual laser is rendered.
    ChainsawExperimentState ray{};
    CHECK(updateChainsawExperiment(ray,ChainsawExperimentalMode::PointerFocus,
        true,false,true,0,0,0,true,0,0,-1,100,480,32,h).started);
    const auto rot=updateChainsawExperiment(ray,ChainsawExperimentalMode::PointerFocus,
        true,false,true,0,0,0,true,.5f,0,-.8660254f,115,480,32,h);
    CHECK(rot.completed&&rot.targetHz==30);
    return 0;
}
