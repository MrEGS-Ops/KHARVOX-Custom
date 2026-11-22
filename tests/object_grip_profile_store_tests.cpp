#include "../src/openxr/ObjectGripProfileStore.h"
#include <cassert>
#include <cstdio>
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
    std::remove("object_grip_profile_test.cfg");
}
