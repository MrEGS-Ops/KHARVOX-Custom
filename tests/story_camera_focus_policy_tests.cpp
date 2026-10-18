#include "../src/camera/StoryCameraFocusPolicy.h"
#include <cassert>
int main(){
 using kharvox::storyFocusCanUseAuthoredYaw;
 // requested, immersive, world, sync, chainsaw, nativeLedge,
 // ledgeCinematicTailHeld, boss
 assert(storyFocusCanUseAuthoredYaw(true,true,true,false,false,false,false,false));
 assert(!storyFocusCanUseAuthoredYaw(false,true,true,false,false,false,false,false));
 assert(!storyFocusCanUseAuthoredYaw(true,false,true,false,false,false,false,false));
 assert(!storyFocusCanUseAuthoredYaw(true,true,false,false,false,false,false,false));
 assert(!storyFocusCanUseAuthoredYaw(true,true,true,true,false,false,false,false));
 assert(!storyFocusCanUseAuthoredYaw(true,true,true,false,true,false,false,false));
 assert(!storyFocusCanUseAuthoredYaw(true,true,true,false,false,true,false,false));
 // Native ledge flag ended, but camera still rendering the same ledge tail.
 assert(!storyFocusCanUseAuthoredYaw(true,true,true,false,false,false,true,false));
 assert(!storyFocusCanUseAuthoredYaw(true,true,true,false,false,false,false,true));
 // Independent story scene after gameplay-camera return uses authored yaw.
 assert(storyFocusCanUseAuthoredYaw(true,true,true,false,false,false,false,false));
 using kharvox::storyFocusCanClearLedgeHold;
 // Pre-cinematic gameplay frames must not release the native ledge hold.
 assert(!storyFocusCanClearLedgeHold(true,true,false,false,100,400));
 assert(!storyFocusCanClearLedgeHold(true,false,false,false,100,500));
 assert(!storyFocusCanClearLedgeHold(false,false,true,false,100,700));
 assert(!storyFocusCanClearLedgeHold(true,false,false,true,100,5000));
 assert(storyFocusCanClearLedgeHold(true,false,true,false,100,600));
 // Aborted climb where no cutscene ever happened: bounded automatic reset.
 assert(!storyFocusCanClearLedgeHold(true,false,false,false,100,2099));
 assert(storyFocusCanClearLedgeHold(true,false,false,false,100,2100));
}
