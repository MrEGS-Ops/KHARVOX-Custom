#include "../src/weapon/DualWieldFirePolicy.h"
#include <cassert>
#include <initializer_list>

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

    // SUPER SHOTGUN: first shot spends readiness, without duplicating shells.
    DualSuperShotgunFireState super{};
    assert(dualSuperShotgunCanFire(super, true));
    assert(!dualSuperShotgunCanFire(super, false));
    nativeDualSuperShotgunShotAccepted(super);
    assert(super.reloadRequired && !super.reloadInProgress);
    assert(!dualSuperShotgunCanFire(super, true));

    // Two guns held: no reload. Dropping the EMPTY SSG itself while still
    // holding the rifle is NOT a reload and cannot be used as a quick reset.
    assert(!dualSuperShotgunCanReload(super, true, false, true));
    assert(!dualSuperShotgunBeginReload(super, true, false, true));
    for (int swap=0; swap<3; ++swap) {
        dualSuperShotgunUpdateReloadHands(super, false, false); // holster SSG
        assert(!dualSuperShotgunCanReload(super, false, false, true));
        assert(!dualSuperShotgunBeginReload(super, false, false, true));
        assert(!nativeDualSuperShotgunReloadCompleted(super, false, false, true));
        dualSuperShotgunUpdateReloadHands(super, true, false); // redraw with rifle
        assert(super.reloadRequired && !super.reloadInProgress);
        assert(!dualSuperShotgunCanFire(super, true));
    }

    // An empty reserve may not produce a phantom reload.
    assert(!dualSuperShotgunBeginReload(super, true, true, false));
    assert(super.reloadRequired && !super.reloadInProgress);

    // Put the RIFLE away, leaving SSG held: reload becomes eligible.
    assert(dualSuperShotgunCanReload(super, true, true, true));
    assert(dualSuperShotgunBeginReload(super, true, true, true));
    assert(super.reloadInProgress);

    // Re-grabbing the rifle or dropping the SSG interrupts, never completes.
    dualSuperShotgunUpdateReloadHands(super, true, false);
    assert(!super.reloadInProgress);
    assert(!nativeDualSuperShotgunReloadCompleted(super, true, true, true));
    assert(super.reloadRequired);
    assert(dualSuperShotgunBeginReload(super, true, true, true));
    dualSuperShotgunUpdateReloadHands(super, false, true);
    assert(!nativeDualSuperShotgunReloadCompleted(super, true, true, true));
    assert(!dualSuperShotgunCanFire(super, true));

    // Still held, free other hand, and actual native reload completed.
    assert(dualSuperShotgunBeginReload(super, true, true, true));
    assert(!nativeDualSuperShotgunReloadCompleted(super, true, false, true));
    assert(super.reloadRequired);
    assert(dualSuperShotgunBeginReload(super, true, true, true));
    assert(nativeDualSuperShotgunReloadCompleted(super, true, true, true));
    assert(!super.reloadRequired && !super.reloadInProgress);
    assert(dualSuperShotgunCanFire(super, true));
    // Already ready: a second fake completion can't duplicate ammunition.
    assert(!nativeDualSuperShotgunReloadCompleted(super, true, true, true));
    return 0;
}
