#include "../src/openxr/HandFocusPolicy.h"
#include <cassert>
int main(){
    using kharvox::useWeaponHandFocus;
    using kharvox::worldHandFocusContext;
    // A world interaction may be checked before the gameplay physics hook
    // publishes a fresh frame. Do NOT require a separate gameplay flag.
    assert(worldHandFocusContext(true,false,false,false,false,false));
    assert(!worldHandFocusContext(false,false,false,false,false,false));
    assert(!worldHandFocusContext(true,true,false,false,false,false));
    assert(!worldHandFocusContext(true,false,true,false,false,false));
    assert(!worldHandFocusContext(true,false,false,true,false,false));
    assert(!worldHandFocusContext(true,false,false,false,true,false));
    assert(!worldHandFocusContext(true,false,false,false,false,true));
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
    // World interaction focus prioritizes the tracked hand even with no
    // HMD render-pose sample, such as during a first-person pickup/station.
    using kharvox::WorldFocusRaySource;
    using kharvox::chooseWorldFocusRaySource;
    assert(chooseWorldFocusRaySource(true,false,false,true,false)
        ==WorldFocusRaySource::Hand);
    assert(chooseWorldFocusRaySource(true,false,false,true,true)
        ==WorldFocusRaySource::Hand);
    assert(chooseWorldFocusRaySource(true,false,false,false,true)
        ==WorldFocusRaySource::Head);
    assert(chooseWorldFocusRaySource(true,false,false,false,false)
        ==WorldFocusRaySource::Native);
    assert(chooseWorldFocusRaySource(false,false,false,true,true)
        ==WorldFocusRaySource::Native);
    assert(chooseWorldFocusRaySource(true,true,false,true,true)
        ==WorldFocusRaySource::Native);
    assert(chooseWorldFocusRaySource(true,false,true,true,true)
        ==WorldFocusRaySource::Native);
    // When an execution ends, HAND focus comes back automatically.
    assert(useWeaponHandFocus(true,true,false,false,false,false));
    return 0;
}
