#include "../src/camera/StoryCameraFocusPolicy.h"
#include <cassert>
int main(){
 using kharvox::storyFocusCanUseAuthoredYaw;
 assert(storyFocusCanUseAuthoredYaw(true,true,true,false,false,false,false));
 assert(!storyFocusCanUseAuthoredYaw(true,true,true,false,false,false,false,true));
 assert(!storyFocusCanUseAuthoredYaw(false,true,true,false,false,false,false));
 assert(!storyFocusCanUseAuthoredYaw(true,false,true,false,false,false,false));
 assert(!storyFocusCanUseAuthoredYaw(true,true,false,false,false,false,false));
 assert(!storyFocusCanUseAuthoredYaw(true,true,true,true,false,false,false));
 assert(!storyFocusCanUseAuthoredYaw(true,true,true,false,true,false,false));
 assert(!storyFocusCanUseAuthoredYaw(true,true,true,false,false,true,false));
 assert(!storyFocusCanUseAuthoredYaw(true,true,true,false,false,false,true));
}
