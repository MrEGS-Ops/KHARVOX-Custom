#include "../src/openxr/HandFocusPolicy.h"
#include <cassert>
int main(){
    using kharvox::useWeaponHandFocus;
    // Default once the mod is enabled: interactions DON'T toggle aiming.
    assert(useWeaponHandFocus(true,true,false,false,false,false));
    for(int rsClicks=0;rsClicks<6;++rsClicks)
        assert(useWeaponHandFocus(true,true,false,false,false,false));
    // Both physical weapon hands use the same persistent mode; ownership
    // selects which controller pose to publish at the caller.
    for(int leftWeaponHand=0;leftWeaponHand<2;++leftWeaponHand){
        (void)leftWeaponHand;
        assert(useWeaponHandFocus(true,true,false,false,false,false));
    }
    // Native executions/ledge/scripted boss sequences temporarily use HEAD.
    assert(!useWeaponHandFocus(true,true,true,false,false,false));
    assert(!useWeaponHandFocus(true,true,false,true,false,false));
    assert(!useWeaponHandFocus(true,true,false,false,true,false));
    assert(!useWeaponHandFocus(true,true,false,false,false,true));
    assert(!useWeaponHandFocus(true,false,false,false,false,false));
    assert(!useWeaponHandFocus(false,true,false,false,false,false));
    // When an execution ends, HAND focus comes back automatically.
    assert(useWeaponHandFocus(true,true,false,false,false,false));
    return 0;
}
