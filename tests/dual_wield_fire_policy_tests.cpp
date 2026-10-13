#include "../src/weapon/DualWieldFirePolicy.h"
#include <cassert>

int main() {
    using namespace kharvox;
    assert(dualWieldPairAllowed(BackWeaponKind::CombatShotgun,
                                BackWeaponKind::SuperShotgun));
    assert(!dualWieldPairAllowed(BackWeaponKind::CombatShotgun,
                                BackWeaponKind::CombatShotgun));
    for (auto heavy : {BackWeaponKind::GaussCannon,
                       BackWeaponKind::Chaingun,
                       BackWeaponKind::Bfg,
                       BackWeaponKind::RocketLauncher,
                       BackWeaponKind::Chainsaw}) {
        assert(!dualWieldPairAllowed(BackWeaponKind::CombatShotgun, heavy));
        assert(!dualWieldPairAllowed(heavy, BackWeaponKind::CombatShotgun));
    }
    DualCombatShotgunFireState gun{};
    assert(canDualFireCombatShotgun(gun, DualFireChannel::Primary, true, false));
    assert(canDualFireCombatShotgun(gun, DualFireChannel::Alternate, true, true));

    // Holding/releasing a charge without native acceptance spends nothing.
    assert(!gun.primarySpent && !gun.alternateSpent);
    assert(!canDualFireCombatShotgun(gun, DualFireChannel::Alternate, false, true));
    assert(!canDualFireCombatShotgun(gun, DualFireChannel::Alternate, true, false));
    assert(!gun.alternateSpent);

    // Primary then alt: exactly one of each, independent order.
    nativeDualCombatShotgunShotAccepted(gun, DualFireChannel::Primary);
    assert(!canDualFireCombatShotgun(gun, DualFireChannel::Primary, true, true));
    assert(canDualFireCombatShotgun(gun, DualFireChannel::Alternate, true, true));
    assert(!dualCombatShotgunCanReload(gun, true));
    nativeDualCombatShotgunShotAccepted(gun, DualFireChannel::Alternate);
    assert(gun.reloadPending);
    assert(!canDualFireCombatShotgun(gun, DualFireChannel::Alternate, true, true));
    assert(!canDualFireCombatShotgun(gun, DualFireChannel::Primary, true, true));
    assert(dualCombatShotgunCanReload(gun, false));
    nativeDualCombatShotgunReloadCompleted(gun);
    assert(canDualFireCombatShotgun(gun, DualFireChannel::Primary, true, true));

    // Alt then primary: a Charged Burst is committed ONCE on native
    // activation, regardless of how many sub-rounds are fired inside it.
    nativeDualCombatShotgunShotAccepted(gun, DualFireChannel::Alternate);
    assert(gun.alternateSpent && !gun.primarySpent);
    assert(!canDualFireCombatShotgun(gun, DualFireChannel::Alternate, true, true));
    assert(canDualFireCombatShotgun(gun, DualFireChannel::Primary, true, true));
    nativeDualCombatShotgunShotAccepted(gun, DualFireChannel::Primary);
    assert(gun.reloadPending);
    assert(dualCombatShotgunCanReload(gun, false));
    return 0;
}
