#include "../src/openxr/WeaponControlReentryPolicy.h"
#include <cstdlib>

static void require(bool ok) { if (!ok) std::abort(); }
int main() {
    using namespace kharvox;
    WeaponControlReentryPolicy gate{};
    using T = WeaponControlTransition;
    // Unknown executable revisions do not change live weapon pose.
    require(gate.update(false, true, false) == T::None);
    require(gate.update(true, true, true) == T::None);
    // Ordinary gameplay camera during terminal minigame is not weapon control.
    require(gate.update(true, true, false) == T::Suspended);
    require(gate.update(true, true, false) == T::None);
    // Temporary menus/cinematics cannot prematurely rebaseline.
    require(gate.update(true, false, false) == T::None);
    require(gate.update(true, false, true) == T::None);
    require(gate.update(true, true, true) == T::Resumed);
    require(gate.update(true, true, true) == T::None);
    // A second interaction must be handled independently.
    require(gate.update(true, true, false) == T::Suspended);
    require(gate.update(false, true, false) == T::None);
    require(gate.update(true, true, true) == T::None);
}
