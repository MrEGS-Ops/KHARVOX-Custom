#include "../src/openxr/GestureWheelExitPolicy.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
int main(){
    using namespace kharvox;
    assert(classifyGestureWheelExit(false,true,false)==GestureWheelExit::None);
    assert(classifyGestureWheelExit(true,true,false)==GestureWheelExit::None);
    assert(classifyGestureWheelExit(true,true,true)==GestureWheelExit::None);
    assert(classifyGestureWheelExit(true,false,false)==GestureWheelExit::Cancel);
    assert(classifyGestureWheelExit(true,false,true)==GestureWheelExit::Confirm);
    assert(wheelMovementSide(0,1)==0);
    assert(wheelMovementSide(1,0)==1);
    assert(wheelMovementSide(-1,1)==1);
    constexpr float d=.70710678f;
    const auto near=[](float a,float b){return std::abs(a-b)<.0001f;};
    const BackWeaponKind kinds[8]={
      BackWeaponKind::CombatShotgun,BackWeaponKind::SuperShotgun,
      BackWeaponKind::HeavyAssaultRifle,BackWeaponKind::Chaingun,
      BackWeaponKind::PlasmaRifle,BackWeaponKind::GaussCannon,
      BackWeaponKind::RocketLauncher,BackWeaponKind::Pistol};
    const float xs[8]={0,d,1,d,0,-d,-1,-d};
    const float ys[8]={1,d,0,-d,-1,-d,0,d};
    for(int i=0;i<8;++i){
        const auto target=previousWeaponSector(kinds[i]);
        assert(target.valid&&near(target.x,xs[i])&&near(target.y,ys[i]));
    }
    assert(!previousWeaponSector(BackWeaponKind::Unknown).valid);
    assert(!previousWeaponSector(BackWeaponKind::Bfg).valid);
    assert(!previousWeaponSector(BackWeaponKind::Chainsaw).valid);
    GestureWheelCancelDrain drain{};
    drain.cancel(BackWeaponKind::SuperShotgun);
    assert(drain.active()&&drain.originalSector.valid);
    assert(near(drain.originalSector.x,d)&&near(drain.originalSector.y,d));
    for(int i=0;i<5;++i){assert(drain.active());drain.frameSubmitted();}
    assert(!drain.active());
    drain.cancel(BackWeaponKind::Pistol);
    drain.reset();
    assert(!drain.active());
    return 0;
}
