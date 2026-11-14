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

    // Emulated hands must never rearm inputs just because tracking returns.
    ControllerOnlySafetyState safety{};
    if(updateControllerOnlySafety(safety,false,false,false,true,true))return 12;
    for(int i=0;i<15;++i)
        if(updateControllerOnlySafety(safety,true,true,false,true,true))return 13;
    if(updateControllerOnlySafety(safety,true,true,true,false,false))return 14; // held trigger
    if(updateControllerOnlySafety(safety,true,true,false,true,true))return 15; // no acknowledge
    if(updateControllerOnlySafety(safety,true,true,true,false,true))return 16; // press BOTH sticks
    if(!updateControllerOnlySafety(safety,true,true,false,true,true))return 17; // release to arm
    if(!updateControllerOnlySafety(safety,true,true,false,true,true))return 18;
    if(updateControllerOnlySafety(safety,true,false,true,false,true))return 19; // tracking lost
    for(int i=0;i<15;++i)
        if(updateControllerOnlySafety(safety,true,true,false,true,true))return 20; // NO auto-rearm
    if(updateControllerOnlySafety(safety,true,true,true,false,true))return 21;
    if(!updateControllerOnlySafety(safety,true,true,false,true,true))return 22;
    if(updateControllerOnlySafety(safety,false,true,false,true,true))return 23; // profile switch
    return 0;
}
