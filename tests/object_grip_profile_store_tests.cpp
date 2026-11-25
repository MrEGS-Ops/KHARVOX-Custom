#include "../src/openxr/ObjectGripProfileStore.h"
#include "../src/camera/ObjectGripRotationPolicy.h"
#include "../src/openxr/ObjectGripInputOwnershipPolicy.h"
#include <cassert>
#include <cstdio>
#include <cmath>
int main(){
    using namespace kharvox;
    assert(!objectGripKeyValid("../../escape"));
    assert(!objectGripKeyValid("id prop 2"));
    assert(objectGripAutomaticName("envhazard/barrels/red_explosive")
           =="Red Explosive");
    ObjectGripProfiles profiles;
    auto& pose=profiles["envhazard/barrels/red"].hands[0];
    pose.configured=true;
    pose.offsetMetres={.1f,0.f,-.35f};
    assert(objectGripConfigured(profiles,"envhazard/barrels/red",0));
    assert(!objectGripConfigured(profiles,"envhazard/barrels/red",1));
    assert(!objectGripConfigured(profiles,"envhazard/barrels/blue",0));
    assert(objectGripSave("object_grip_profile_test.cfg",profiles));
    ObjectGripProfiles loaded;
    assert(objectGripLoad("object_grip_profile_test.cfg",loaded));
    assert(objectGripConfigured(loaded,"envhazard/barrels/red",0));
    // Once saved, a pickup of THAT model and physical hand must bypass
    // the automatic calibration overlay and use the prior six-DOF pose.
    // Opposite hand or another model stays unsaved. Unnamed props may
    // reuse their session instance only, never across map generations.
    const std::map<unsigned,ObjectGripProfile> emptySessions{};
    const auto* savedLeft=objectGripSavedPose(loaded,emptySessions,
        "envhazard/barrels/red",17,0);
    assert(savedLeft&&savedLeft->configured);
    assert(std::abs(savedLeft->offsetMetres[0]-.1f)<.00001f);
    assert(!objectGripSavedPose(loaded,emptySessions,
        "envhazard/barrels/red",17,1));
    assert(!objectGripSavedPose(loaded,emptySessions,
        "envhazard/barrels/blue",17,0));
    assert(!objectGripSavedPose(loaded,emptySessions,
        "envhazard/barrels/red",17,2));
    std::map<unsigned,ObjectGripProfile> sessions{};
    sessions[94].hands[1].configured=true;
    sessions[94].hands[1].offsetMetres={0.f,.2f,-.1f};
    assert(objectGripSavedPose(loaded,sessions,"",94,1));
    assert(!objectGripSavedPose(loaded,sessions,"",95,1));
    assert(!objectGripSavedPose(loaded,sessions,"",94,0));
    // Stable rotation references: no physics-feedback tumble.
    constexpr float eps=.0005f;
    const float identity[9]{1,0,0,0,1,0,0,0,1};
    // The initial six-DOF pose is defined ONLY in the free-hand frame.
    // An object lying at 45 degrees, sideways, or upside-down MUST get
    // the same relative position and orientation on every new pickup.
    const auto neutral=objectGripNeutralLocalAxis();
    const float expectedNeutral[9]{
        1.f,0.f,0.f,0.f,0.f,-1.f,0.f,1.f,0.f};
    for(int i=0;i<9;++i)
        assert(std::abs(neutral[i]-expectedNeutral[i])<eps);
    assert(kObjectGripNeutralOffsetUnits[0]==0.f);
    assert(kObjectGripNeutralOffsetUnits[1]==-4.f);
    assert(kObjectGripNeutralOffsetUnits[2]==-20.f);
    // With the free hand held out like a gun, its local Y points UP
    // in DOOM world. The barrel's neutral model +Z must point UP too.
    const float gunAimedHand[9]{
        0.f,-1.f,0.f, 0.f,0.f,1.f, -1.f,0.f,0.f};
    const auto handWorldAxis=[&](const float hand[9]){
        std::array<float,9> world{};
        for(int col=0;col<3;++col)
            for(int axis=0;axis<3;++axis)
                for(int local=0;local<3;++local)
                    world[col*3+axis]+=
                        hand[local*3+axis]*neutral[col*3+local];
        return world;
    };
    const auto gunPoseWorld=handWorldAxis(gunAimedHand);
    assert(std::abs(gunPoseWorld[6])<eps);
    assert(std::abs(gunPoseWorld[7])<eps);
    assert(std::abs(gunPoseWorld[8]-1.f)<eps);
    // Turning/tilting the controller moves the world pose, but NOT the
    // rotation saved relative to the hand. No world-up recalc per tick.
    const auto relativeGun=objectGripToHandLocalAxis(
        gunAimedHand,gunPoseWorld.data());
    const auto handTurned=objectGripQuaternionToAxis({
        .2500f,.4330127f,.2500f,.8291562f});
    const auto turnedWorld=handWorldAxis(handTurned.data());
    const auto relativeTurned=objectGripToHandLocalAxis(
        handTurned.data(),turnedWorld.data());
    for(int i=0;i<9;++i){
        assert(std::abs(relativeGun[i]-neutral[i])<eps);
        assert(std::abs(relativeTurned[i]-neutral[i])<eps);
    }
    // The offset is also applied in the same rotated hand frame.
    const auto handWorldPosition=[&](const float hand[9]){
        std::array<float,3> world{};
        for(int axis=0;axis<3;++axis)
            for(int local=0;local<3;++local)
                world[axis]+=hand[local*3+axis]
                    *kObjectGripNeutralOffsetUnits[local];
        return world;
    };
    const auto firstPos=handWorldPosition(gunAimedHand);
    const auto turnedPos=handWorldPosition(handTurned.data());
    float separationSquared{};
    for(int axis=0;axis<3;++axis)
        separationSquared+=(firstPos[axis]-turnedPos[axis])
            *(firstPos[axis]-turnedPos[axis]);
    assert(separationSquared>1.f);
    // The old joystick-spin path has been retired. During WORLD FLOAT
    // the full object transform stays fixed while the hand moves, then
    // release re-expresses that identical world pose in hand coordinates.
    const auto local=objectGripToHandLocalAxis(
        gunAimedHand,gunPoseWorld.data());
    const auto localQuat=objectGripAxisToQuaternion(local.data());
    float relativeLength=0.f;
    for(float v:localQuat)relativeLength+=v*v;
    assert(std::abs(relativeLength-1.f)<eps);
    // Floating prop stays fixed in world space even when player walks
    // several metres and rotates the hand. Grip release computes a new
    // hand-relative 6DOF pose, reconstructing the same world transform.
    const float handA[3]{0.f,0.f,0.f};
    const float handB[3]{250.f,-100.f,30.f};
    const auto handBOrientation=objectGripQuaternionToAxis(
        {0.f,0.f,.70710678f,.70710678f});
    float frozenWorldPosition[3]{},frozenWorldBasis[9]{};
    objectGripToWorldPose(handA,identity,
        kObjectGripNeutralOffsetUnits.data(),neutral.data(),
        frozenWorldPosition,frozenWorldBasis);
    float localAfterWalking[3]{},axisAfterWalking[9]{};
    objectGripToHandLocalPose(handB,handBOrientation.data(),
        frozenWorldPosition,frozenWorldBasis,
        localAfterWalking,axisAfterWalking);
    float lockedPosition[3]{},lockedBasis[9]{};
    objectGripToWorldPose(handB,handBOrientation.data(),
        localAfterWalking,axisAfterWalking,lockedPosition,lockedBasis);
    for(int i=0;i<3;++i)
        assert(std::abs(lockedPosition[i]-frozenWorldPosition[i])<eps);
    for(int i=0;i<9;++i)
        assert(std::abs(lockedBasis[i]-frozenWorldBasis[i])<eps);
    assert(std::abs(localAfterWalking[0])
        +std::abs(localAfterWalking[1])>200.f);
    ObjectGripPose walkAroundSaved{};
    walkAroundSaved.configured=true;
    for(int i=0;i<3;++i)
        walkAroundSaved.offsetMetres[i]=localAfterWalking[i]/100.f;
    walkAroundSaved.localRotation=objectGripAxisToQuaternion(
        axisAfterWalking);
    assert(objectGripPoseValid(walkAroundSaved));
    walkAroundSaved.offsetMetres[0]=50.1f;
    assert(!objectGripPoseValid(walkAroundSaved));
    // The object, not a global gameplay grenade option, owns the support
    // trigger. Test BOTH native and XR mailbox ownership to close the
    // one-frame gap between the DOOM and OpenXR updates.
    assert(objectGripBlocksOffhandEquipment(true,true,false,true));
    assert(objectGripBlocksOffhandEquipment(true,true,true,false));
    assert(!objectGripBlocksOffhandEquipment(true,true,false,false));
    assert(!objectGripBlocksOffhandEquipment(true,false,true,true));
    assert(!objectGripBlocksOffhandEquipment(false,true,true,true));
    // Hold GRIP + support trigger: only the rising edge re-centers;
    // trigger without Grip remains the 5cm fine-tune modifier.
    assert(objectGripNeutralResetChordEdge(true,true,true,true,false));
    assert(!objectGripNeutralResetChordEdge(true,true,true,true,true));
    assert(!objectGripNeutralResetChordEdge(true,true,false,true,false));
    assert(!objectGripNeutralResetChordEdge(true,true,true,false,false));
    assert(!objectGripNeutralResetChordEdge(false,true,true,true,false));
    std::remove("object_grip_profile_test.cfg");
}
