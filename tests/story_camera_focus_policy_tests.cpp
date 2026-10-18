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
}
