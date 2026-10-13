#include "../src/openxr/PersistentSuperShotgunReloadPolicy.h"
#include <cassert>

int main() {
    using namespace kharvox;
    PersistentSuperShotgunReloadState state{};
    PersistentSuperShotgunReloadInput in{};
    in.enabled=true;
    in.gameplayActive=true;
    in.superShotgunEquipped=true;
    in.fireRequested=true;
    in.nowNanoseconds=1000000000ull;
    auto o=updatePersistentSuperShotgunReload(state,in);
    assert(o.shotAttemptAccepted && o.forwardFire && state.reloadRequired);
    assert(o.remainingEquippedNanoseconds==persistentSsgActiveReloadNanoseconds);

    // Release, spend 200ms with SSG held, then QUICK SWITCH to rifle.
    in.fireRequested=false;in.nowNanoseconds+=100000000ull;
    updatePersistentSuperShotgunReload(state,in);
    in.nowNanoseconds+=100000000ull;
    updatePersistentSuperShotgunReload(state,in);
    const auto heldProgress=state.equippedProgressNanoseconds;
    assert(heldProgress==200000000ull);
    in.superShotgunEquipped=false;in.fireRequested=true;
    in.nowNanoseconds+=100000000ull;
    o=updatePersistentSuperShotgunReload(state,in);
    assert(o.forwardFire); // rifle never blocked by SSG rule
    in.nowNanoseconds+=9000000000ull;
    updatePersistentSuperShotgunReload(state,in);
    assert(state.equippedProgressNanoseconds==heldProgress); // NO background reload

    // Switching back with trigger held cannot fire the SSG.
    in.superShotgunEquipped=true;in.nowNanoseconds+=100000000ull;
    o=updatePersistentSuperShotgunReload(state,in);
    assert(!o.forwardFire && o.blockedByPersistentReload);
    assert(state.reloadRequired);
    in.fireRequested=false;
    for(int frame=0;frame<15;frame++){
        in.nowNanoseconds+=100000000ull;
        o=updatePersistentSuperShotgunReload(state,in);
    }
    assert(o.nativeReloadWindowCompleted); // after 1.5 seconds held
    assert(!state.reloadRequired);
    // New trigger press, now another single shot is allowed.
    in.fireRequested=true;in.nowNanoseconds+=100000000ull;
    o=updatePersistentSuperShotgunReload(state,in);
    assert(o.shotAttemptAccepted && o.forwardFire && state.reloadRequired);

    // Rapid repeat equip/unequip must not bypass the persistent cycle.
    in.fireRequested=false;in.nowNanoseconds+=100000000ull;
    updatePersistentSuperShotgunReload(state,in);
    for(int i=0;i<10;i++){
        in.superShotgunEquipped=false;in.nowNanoseconds+=100000000ull;
        updatePersistentSuperShotgunReload(state,in);
        in.nowNanoseconds+=1000000000ull;
        updatePersistentSuperShotgunReload(state,in);
        in.superShotgunEquipped=true;in.fireRequested=true;
        in.nowNanoseconds+=100000000ull;
        o=updatePersistentSuperShotgunReload(state,in);
        assert(!o.forwardFire && !o.shotAttemptAccepted);
        in.fireRequested=false;in.nowNanoseconds+=100000000ull;
        updatePersistentSuperShotgunReload(state,in);
    }
    assert(state.reloadRequired);

    // Menus/pauses/holster do not advance reload.
    const auto before=state.equippedProgressNanoseconds;
    in.gameplayActive=false;in.nowNanoseconds+=100000000ull;
    updatePersistentSuperShotgunReload(state,in);
    in.nowNanoseconds+=5000000000ull;
    updatePersistentSuperShotgunReload(state,in);
    assert(state.equippedProgressNanoseconds==before);
    in.gameplayActive=true;in.weaponHolstered=true;
    in.nowNanoseconds+=100000000ull;updatePersistentSuperShotgunReload(state,in);
    assert(state.equippedProgressNanoseconds==before);
    in.weaponHolstered=false;in.weaponWheelActive=true;
    in.nowNanoseconds+=100000000ull;updatePersistentSuperShotgunReload(state,in);
    assert(state.equippedProgressNanoseconds==before);

    // Native ammo empty never grants a new fire pulse.
    state={};
    in={};in.enabled=true;in.gameplayActive=true;
    in.superShotgunEquipped=true;in.nativeAmmoKnownEmpty=true;
    in.fireRequested=true;in.nowNanoseconds=1000000000ull;
    o=updatePersistentSuperShotgunReload(state,in);
    assert(!o.forwardFire && !o.shotAttemptAccepted && !state.reloadRequired);

    // Independent toggle OFF: input passes through; state cleared.
    in.enabled=false;
    o=updatePersistentSuperShotgunReload(state,in);
    assert(o.forwardFire && !state.reloadRequired);
    return 0;
}
