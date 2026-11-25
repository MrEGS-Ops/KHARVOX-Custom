#include "../src/openxr/ObjectGripProfileStore.h"
#include "../src/camera/ObjectGripRotationPolicy.h"
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
    // Stable rotation references: no physics-feedback tumble.
    constexpr float eps=.0005f;
    const float identity[9]{1,0,0,0,1,0,0,0,1};
    // A hand yaw by itself MUST NOT twist the held item.
    // Every new pickup starts from world identity (authored upright),
    // irrespective of how the physics body was originally lying.
    const auto defaultUpright=objectGripNeutralLocalAxis(identity);
    for(int i=0;i<9;++i)
        assert(std::abs(defaultUpright[i]-identity[i])<eps);
    assert(kObjectGripNeutralOffsetUnits[0]==0.f);
    assert(kObjectGripNeutralOffsetUnits[1]==-4.f);
    assert(kObjectGripNeutralOffsetUnits[2]==-20.f);
    const auto yawHand=objectGripQuaternionToAxis({
        0.f,0.f,.70710678f,.70710678f});
    const auto noTwist=objectGripTiltAndSpinAxis(
        yawHand.data(),identity,identity,0.f);
    for(int i=0;i<9;++i)
        assert(std::abs(noTwist[i]-identity[i])<eps);
    // Stick spin is about the model's local vertical Z, no translation.
    const auto spin=objectGripTiltAndSpinAxis(
        identity,identity,identity,90.f);
    assert(std::abs(spin[0])<eps && std::abs(spin[1]-1.f)<eps);
    assert(std::abs(spin[3]+1.f)<eps && std::abs(spin[4])<eps);
    assert(std::abs(spin[8]-1.f)<eps);
    // Physical pitch controls tilt; spin preserves the same tilt axis.
    constexpr float sn=.258819045f,cs=.965925826f; // 30 degrees X
    const auto handTilt=objectGripQuaternionToAxis({sn,0.f,0.f,cs});
    const auto pitched=objectGripTiltAndSpinAxis(
        handTilt.data(),identity,identity,0.f);
    const auto pitchedAndSpun=objectGripTiltAndSpinAxis(
        handTilt.data(),identity,identity,90.f);
    for(int i=6;i<9;++i)
        assert(std::abs(pitched[i]-pitchedAndSpun[i])<eps);
    const auto local=objectGripToHandLocalAxis(
        handTilt.data(),pitchedAndSpun.data());
    const auto localQuat=objectGripAxisToQuaternion(local.data());
    float length=0.f;
    for(float v:localQuat)length+=v*v;
    assert(std::abs(length-1.f)<eps);
    assert(objectGripSpinAxis(.15f)==0.f);
    assert(objectGripSpinAxis(.8f)==.8f);
    assert(objectGripSpinAxis(-1.5f)==-1.f);
    std::remove("object_grip_profile_test.cfg");
}
