#include "../src/openxr/GripThresholdPolicy.h"

#include <cmath>
#include <cstring>

int main() {
    using namespace kharvox;

    if (std::strcmp(gripInputComponent(GripControllerProfile::Default),
            "squeeze/value") != 0) return 1;
    if (std::strcmp(gripInputComponent(GripControllerProfile::ValveIndex),
            "squeeze/force") != 0) return 2;

    // Existing controller profiles retain the previous > 0.55 behavior.
    if (updateGripPressed(0.55f, false, GripControllerProfile::Default)) return 3;
    if (!updateGripPressed(0.56f, false, GripControllerProfile::Default)) return 4;
    if (updateGripPressed(0.55f, true, GripControllerProfile::Default)) return 5;

    // Valve Index needs a deliberate squeeze to engage.
    if (updateGripPressed(0.70f, false, GripControllerProfile::ValveIndex)) return 6;
    if (!updateGripPressed(0.71f, false, GripControllerProfile::ValveIndex)) return 7;

    // Once engaged, hysteresis prevents chatter while the grip is held.
    if (!updateGripPressed(0.60f, true, GripControllerProfile::ValveIndex)) return 8;
    if (updateGripPressed(0.55f, true, GripControllerProfile::ValveIndex)) return 9;

    if (updateGripPressed(NAN, false, GripControllerProfile::ValveIndex)) return 10;
    if (updateGripPressed(NAN, true, GripControllerProfile::ValveIndex)) return 11;

    // Cold startup: no hidden two-thumbstick gate if both physical profiles
    // and controller poses are tracked and all trigger/grip inputs are neutral.
    ControllerOnlySafetyState safety{};
    if(updateControllerOnlySafety(safety,false,false,false,true,true))return 12;
    for(int i=0;i<11;++i)
        if(updateControllerOnlySafety(safety,true,true,false,true,true))return 13;
    if(!updateControllerOnlySafety(safety,true,true,false,true,true))return 14;
    if(!safety.armed||safety.reconnectAckRequired)return 15;
    if(!updateControllerOnlySafety(safety,true,true,false,true,true))return 16;

    // A genuine tracking loss after arming MUST still require two-stick
    // confirmation; tracking recovery alone must not re-arm gameplay.
    if(updateControllerOnlySafety(safety,true,false,false,true,true))return 17;
    if(!safety.reconnectAckRequired||safety.armed)return 18;
    for(int i=0;i<15;++i)
        if(updateControllerOnlySafety(safety,true,true,false,true,true))return 19;
    if(updateControllerOnlySafety(safety,true,true,true,false,false))return 20; // trigger held
    if(updateControllerOnlySafety(safety,true,true,false,true,true))return 21;
    if(updateControllerOnlySafety(safety,true,true,true,false,true))return 22; // both clicks
    if(!updateControllerOnlySafety(safety,true,true,false,true,true))return 23; // released
    if(!safety.armed||safety.reconnectAckRequired)return 24;
    if(updateControllerOnlySafety(safety,false,true,false,true,true))return 25; // profile lost
    if(!safety.reconnectAckRequired)return 26;
    for(int i=0;i<15;++i)
        if(updateControllerOnlySafety(safety,true,true,false,true,true))return 27;
    if(updateControllerOnlySafety(safety,true,true,true,false,true))return 28;
    if(!updateControllerOnlySafety(safety,true,true,false,true,true))return 29;
    if(safety.reconnectAckRequired)return 30;

    // Controller-emulated hands cannot cold-start through the gate.
    ControllerOnlySafetyState emulated{};
    for(int i=0;i<20;++i)
        if(updateControllerOnlySafety(emulated,false,true,false,true,true))return 31;
    for(int i=0;i<20;++i)
        if(updateControllerOnlySafety(emulated,true,false,false,true,true))return 32;
    // Neutral triggers/grips are still required even on a clean startup.
    ControllerOnlySafetyState heldTrigger{};
    for(int i=0;i<16;++i)
        if(updateControllerOnlySafety(heldTrigger,true,true,false,true,false))return 33;
    if(!updateControllerOnlySafety(heldTrigger,true,true,false,true,true))return 34;
    return 0;
}
