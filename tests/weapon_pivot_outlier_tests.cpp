#include "../src/weapon/WeaponPivotOutlierPolicy.h"
#include <cassert>
int main(){
    using namespace kharvox;
    const float neutral[]{10.f,4.f,-7.f};
    const float ordinary[]{12.f,5.f,-7.f};
    const float displaced[]{50.f,-35.f,22.f};
    assert(!weaponPivotOutlier(ordinary,neutral));
    assert(weaponPivotOutlier(displaced,neutral));
    ScriptedWeaponPivotRecovery guard{};
    for(int i=0;i<3;++i)assert(guard.observe(true)==ScriptedWeaponPivotRecovery::Event::None);
    assert(guard.observe(true)==ScriptedWeaponPivotRecovery::Event::Quarantine);
    assert(guard.quarantined);
    for(int i=0;i<11;++i)assert(guard.observe(false)==ScriptedWeaponPivotRecovery::Event::None);
    assert(guard.observe(false)==ScriptedWeaponPivotRecovery::Event::Recovered);
    assert(!guard.quarantined);
    // Isolated movement does not trigger a scripted scene reset.
    assert(guard.observe(true)==ScriptedWeaponPivotRecovery::Event::None);
    assert(guard.observe(false)==ScriptedWeaponPivotRecovery::Event::None);
    return 0;
}
