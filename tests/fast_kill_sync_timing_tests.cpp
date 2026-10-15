#include "../src/openxr/CinematicRefreshPolicy.h"
#include <cassert>

// Regression from Build #81: the October 5 native full-speed sync CVar
// was accidentally overwritten to 0 EVERY FRAME by immersive cinematic
// refresh. Native Glory and Chainsaw executions must preserve the opt-in 1.
int main() {
    using namespace kharvox;
    // Fast Glory and Fast Chainsaw use the SAME native sync flag.
    assert(nativeSyncAdaptiveTickDuringCinematic(true)==1);
    assert(nativeSyncAdaptiveTickAfterCinematic(true,0)==1);
    assert(nativeSyncAdaptiveTickAfterCinematic(true,1)==1);

    // With fast mods OFF, leave existing immersive refresh behavior and the
    // original/native saved value unchanged at release.
    assert(nativeSyncAdaptiveTickDuringCinematic(false)==0);
    assert(nativeSyncAdaptiveTickAfterCinematic(false,0)==0);
    assert(nativeSyncAdaptiveTickAfterCinematic(false,1)==1);

    // The present-path fast flag MUST use either toggle (not both).
    for(bool glory : {false,true}) {
        for(bool chainsaw : {false,true}) {
            const bool nativeFast=glory||chainsaw;
            assert(nativeSyncAdaptiveTickDuringCinematic(nativeFast)
                   ==(glory||chainsaw?1:0));
        }
    }
    return 0;
}
