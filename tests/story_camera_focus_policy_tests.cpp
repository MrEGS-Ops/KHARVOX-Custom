#include "../src/camera/StoryCameraFocusPolicy.h"

// These checks remain active in the GitHub Release configuration.
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

int main(){
    using kharvox::storyFocusCanUseAuthoredYaw;
    CHECK(storyFocusCanUseAuthoredYaw(true,true,true,false,false,false,false));
    CHECK(!storyFocusCanUseAuthoredYaw(true,true,true,false,false,false,false,true));
    CHECK(!storyFocusCanUseAuthoredYaw(false,true,true,false,false,false,false));
    CHECK(!storyFocusCanUseAuthoredYaw(true,false,true,false,false,false,false));
    CHECK(!storyFocusCanUseAuthoredYaw(true,true,false,false,false,false,false));
    CHECK(!storyFocusCanUseAuthoredYaw(true,true,true,true,false,false,false));
    CHECK(!storyFocusCanUseAuthoredYaw(true,true,true,false,true,false,false));
    CHECK(!storyFocusCanUseAuthoredYaw(true,true,true,false,false,true,false));
    CHECK(!storyFocusCanUseAuthoredYaw(true,true,true,false,false,false,true));

    kharvox::StoryCameraFocusAutoSuspend focus{};
    CHECK(!focus.update(false,false,true));  // normal play, user option available
    CHECK(focus.update(true,false,true));    // ledge begins before cutscene flag
    CHECK(focus.update(true,true,false));    // action is still active
    CHECK(focus.update(false,true,false));   // execution flag clears first
    CHECK(focus.update(false,true,true));    // not yet safe: native cutscene
    CHECK(focus.update(false,false,false));  // not yet safe: camera not gameplay
    CHECK(!focus.update(false,false,true));  // genuine gameplay return
    CHECK(!focus.update(false,false,true));  // stable across later frames
    CHECK(focus.update(true,true,false));    // independent chainsaw / Glory Kill
    CHECK(focus.update(false,true,false));   // lingering animated camera
    CHECK(!focus.update(false,false,true));  // restored only once back to play
    return 0;
}
