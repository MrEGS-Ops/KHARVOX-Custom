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
    CHECK(std::abs(chainsawZoneQuadWidthMeters-1.50f)<.001f);
    CHECK(std::abs(chainsawZoneQuadHeightMeters-1.98f)<.001f);
    CHECK(std::abs(chainsawSingleSwipeDistanceMeters-.25f)<.0001f);
    CHECK(chainsawCutChordLateralTolerance>.20f);
    CHECK(chainsawPhysicalMotionFastHz==45);
    CHECK(chainsawZoneGuidePulseAmplitude>.85f);
    CHECK(chainsawZoneGuidePulseFrequencyHz==150.f);
    CHECK(chainsawZoneGuidePulseOnMs<chainsawZoneGuidePulsePeriodMs);
    CHECK(!chainsawZoneOutsidePulseOn(-1.f,0));
    CHECK(!chainsawZoneOutsidePulseOn(std::nanf(""),0));
    CHECK(!chainsawZoneOutsidePulseOn(0.f,0));
    CHECK(!chainsawZoneOutsidePulseOn(.68f,0));
    CHECK(chainsawZoneOutsidePulseOn(.69f,0));
    CHECK(chainsawZoneOutsidePulseOn(.90f,159));
    CHECK(!chainsawZoneOutsidePulseOn(.90f,160));
    CHECK(!chainsawZoneOutsidePulseOn(.90f,359));
    CHECK(chainsawZoneOutsidePulseOn(.90f,360));
    CHECK(!chainsawZoneOutsidePulseOn(.20f,360));
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
    CHECK(!chainsawZoneOutsidePulseOn(atCentre.normalizedRadius,0));
    CHECK(!chainsawZoneOutsidePulseOn(atCentreFar.normalizedRadius,0));
    CHECK(chainsawZoneOutsidePulseOn(
        projectChainsawHandToOval(head,handAt(.60f,0.f,.48f),
            center,forward,right,up).normalizedRadius,0));
    CHECK(!chainsawZoneOutsidePulseOn(nearProj.normalizedRadius,0));
    CHECK(!chainsawZoneOutsidePulseOn(farProj.normalizedRadius,0));
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
    // 2nd trigger press must work BEFORE entering oval, even with no tracking.
    ChainsawExperimentState quick{};
    CHECK(!sample(quick,true,true,true,.80f,0,.48f,100).armed);
    CHECK(!quick.fastFinishLatched&&!quick.cutPathLocked);
    CHECK(!sample(quick,true,false,true,.80f,0,.48f,120).armed);
    const auto quickFinish=sample(quick,true,true,false,.80f,0,.48f,140);
    CHECK(quickFinish.armed&&quickFinish.completed&&quickFinish.targetHz==30);
    CHECK(quick.fastFinishLatched&&quick.triggerRestarts==1&&!quick.cutPathLocked);
    CHECK(sample(quick,true,false,false,.20f,0,.48f,160).targetHz==30);
    CHECK(sample(quick,true,true,false,.20f,0,.48f,180).targetHz==30);
    CHECK(quick.triggerRestarts==1);
    CHECK(!sample(quick,false,true,true,.20f,0,.48f,200).armed);
    CHECK(!quick.fastFinishLatched&&!quick.active);

    // FIRST held trigger starts the game kill: crossing from right
    // visible edge through centre to opposite LEFT edge completes gesture.
    ChainsawExperimentState chord{};
    CHECK(!sample(chord,true,true,true,.80f,0,.48f,300).armed);
    CHECK(chord.seenOutside&&chord.zoneGuideWasOutside);
    const auto entry=sample(chord,true,true,true,.46f,0,.48f,320);
    CHECK(entry.armed&&entry.started&&entry.targetHz==480);
    CHECK(chord.cutPathLocked&&chord.cutDirX<-.95f);
    CHECK(chord.progress==0.f&&chord.strokeTravelMeters==0.f);
    const auto firstMotion=sample(chord,true,true,true,.20f,0,.48f,340);
    CHECK(firstMotion.armed&&firstMotion.targetHz==chainsawPhysicalMotionFastHz);
    CHECK(firstMotion.progress>.1f&&firstMotion.progress<.5f);
    CHECK(!chord.oneSwipeCommitted);
    const float halfway=chord.progress;
    for(int i=0;i<7;i++)
        CHECK(sample(chord,true,true,true,.20f,0,.48f,360+20*i).armed);
    CHECK(chord.progress==halfway);
    CHECK(sample(chord,true,true,true,.20f,0,.48f,500).targetHz>455);
    CHECK(!chord.oneSwipeCommitted);
    // Backtrack toward entry: it must not count as finishing.
    CHECK(sample(chord,true,true,true,.55f,0,.48f,520).armed);
    CHECK(!chord.oneSwipeCommitted);
    CHECK(sample(chord,true,true,true,.26f,0,.48f,540).armed);
    CHECK(sample(chord,true,true,true,0.f,0,.48f,560).armed);
    CHECK(sample(chord,true,true,true,-.25f,0,.48f,580).armed);
    CHECK(sample(chord,true,true,true,-.45f,0,.48f,600).armed);
    const auto edgeDone=sample(chord,true,true,true,-.54f,0,.48f,620);
    CHECK(edgeDone.armed&&edgeDone.completed&&edgeDone.targetHz==30);
    CHECK(chord.oneSwipeCommitted&&!chord.fastFinishLatched);
    CHECK(chord.progress==1.f);
    CHECK(sample(chord,true,true,true,-.54f,0,.48f,900).targetHz==30);
    CHECK(sample(chord,true,false,true,-.54f,0,.48f,920).targetHz==30);
    CHECK(!sample(chord,false,false,true,-.54f,0,.48f,940).armed);
    CHECK(!chord.oneSwipeCommitted);

    // Leaving on the SAME edge and sweeping sideways is NOT completion.
    ChainsawExperimentState sideways{};
    CHECK(!sample(sideways,true,true,true,.84f,0,.48f,1000).armed);
    CHECK(sample(sideways,true,true,true,.48f,0,.48f,1020).armed);
    CHECK(sample(sideways,true,true,true,.84f,0,.48f,1040).armed);
    CHECK(!sideways.oneSwipeCommitted);
    CHECK(sample(sideways,true,true,true,.40f,0,.48f,1060).armed);
    CHECK(sample(sideways,true,true,true,.12f,.55f,.48f,1080).armed);
    CHECK(!sideways.oneSwipeCommitted);
    CHECK(sample(sideways,true,true,true,-.20f,.58f,.48f,1100).armed);
    CHECK(!sideways.oneSwipeCommitted);
    CHECK(!sample(sideways,false,false,true,-.20f,.58f,.48f,1120).armed);

    // Second press AFTER moving partway: immediate fast-finish, NO re-entry.
    ChainsawExperimentState partial{};
    CHECK(!sample(partial,true,true,true,.80f,0,.48f,1200).armed);
    CHECK(sample(partial,true,true,true,.46f,0,.48f,1220).armed);
    CHECK(sample(partial,true,true,true,.24f,0,.48f,1240).armed);
    CHECK(partial.progress>0.f&&!partial.oneSwipeCommitted);
    const auto released=sample(partial,true,false,true,.24f,0,.48f,1260);
    CHECK(released.armed&&released.targetHz==300&&!partial.fastFinishLatched);
    const auto skip=sample(partial,true,true,true,.80f,0,.48f,1280);
    CHECK(skip.armed&&skip.completed&&skip.targetHz==30);
    CHECK(partial.fastFinishLatched&&partial.triggerRestarts==1);
    CHECK(sample(partial,true,false,false,.80f,0,.48f,1300).targetHz==30);
    CHECK(!sample(partial,false,false,false,.80f,0,.48f,1320).armed);

    // Held without release must NEVER be mistaken for a second press.
    ChainsawExperimentState held{};
    CHECK(!sample(held,true,true,true,.85f,0,.48f,1400).armed);
    CHECK(!sample(held,true,true,true,.85f,0,.48f,1420).armed);
    CHECK(sample(held,true,true,true,.46f,0,.48f,1440).armed);
    CHECK(!held.fastFinishLatched);
    CHECK(!sample(held,false,true,true,.46f,0,.48f,1460).armed);

    // New kill resets direction and requires a new outside entry.
    ChainsawExperimentState inside{};
    CHECK(!sample(inside,true,true,true,.10f,0,.48f,1500).armed);
    CHECK(!sample(inside,true,true,true,.00f,0,.48f,1520).armed);
    CHECK(!inside.cutPathLocked&&!inside.fastFinishLatched);
    CHECK(!sample(inside,false,true,true,0,0,.48f,1540).armed);

    // Legacy Pointer Focus intentionally unchanged.
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
