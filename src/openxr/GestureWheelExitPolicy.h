#pragma once
#include "../weapon/BackWeaponPolicy.h"
#include <cstdint>
namespace kharvox {
enum class GestureWheelExit { None, Confirm, Cancel };
inline GestureWheelExit classifyGestureWheelExit(bool previouslyOpen,bool stillInZone,bool gripHeld) {
    if(!previouslyOpen||stillInZone)return GestureWheelExit::None;
    return gripHeld?GestureWheelExit::Confirm:GestureWheelExit::Cancel;
}
// Side zero RIGHT, side one LEFT. The OPENING hand owns physical motion.
inline int wheelMovementSide(int openedSide,int fallbackWeaponSide){
    return openedSide==0||openedSide==1?openedSide:fallbackWeaponSide;
}
// Radial directions in native DOOM 2016 weapon wheel. On cancellation,
// steer to the ALREADY EQUIPPED sector while the wheel button remains down;
// when the button releases, DOOM retains that gun without switching back.
// BFG/Chainsaw are not wheel sectors.
struct GestureWheelSector {float x{},y{};bool valid{};};
inline GestureWheelSector previousWeaponSector(BackWeaponKind weapon){
    constexpr float d=.70710678f;
    switch(weapon){
    case BackWeaponKind::CombatShotgun:return {0.f,1.f,true};
    case BackWeaponKind::SuperShotgun:return {d,d,true};
    case BackWeaponKind::HeavyAssaultRifle:return {1.f,0.f,true};
    case BackWeaponKind::Chaingun:return {d,-d,true};
    case BackWeaponKind::PlasmaRifle:return {0.f,-1.f,true};
    case BackWeaponKind::GaussCannon:return {-d,-d,true};
    case BackWeaponKind::RocketLauncher:return {-1.f,0.f,true};
    case BackWeaponKind::Pistol:return {-d,d,true};
    default:return {};
    }
}
struct GestureWheelCancelDrain {
    int framesRemaining{};
    GestureWheelSector originalSector{};
    void cancel(BackWeaponKind original){
        originalSector=previousWeaponSector(original);
        framesRemaining=5;
    }
    void reset(){*this={};}
    bool active()const{return framesRemaining>0;}
    void frameSubmitted(){if(framesRemaining>0)--framesRemaining;}
};
} // namespace kharvox
