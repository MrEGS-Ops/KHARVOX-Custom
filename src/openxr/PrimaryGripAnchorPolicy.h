#pragma once
// Native-independent glue for keeping KHARVOX's gun and rendered grip at one
// shared anchor. All positions use one coordinate space; caller supplies its
// existing (unit quaternion) rotation function, so no XR dependencies leak.
namespace kharvox::primarygrip {
template<class Vec,class Quat,class Rotate>
inline Vec weaponAnchor(const Vec&normalWeaponAnchor,const Vec&physicalHand,
    const Quat&gunRotation,const Vec&capturedGrip,bool heavyGrip,
    Rotate rotate) {
    if(!heavyGrip)return normalWeaponAnchor;
    const auto offset=rotate(gunRotation,capturedGrip);
    return {physicalHand.x-offset.x,physicalHand.y-offset.y,
        physicalHand.z-offset.z};
}
template<class Vec,class Quat,class Rotate>
inline Vec localGrip(const Vec&weaponAnchor,const Vec&physicalHand,
    const Quat&gunRotation,Rotate rotate) {
    const Quat inverse{-gunRotation.x,-gunRotation.y,-gunRotation.z,
        gunRotation.w};
    const Vec difference{physicalHand.x-weaponAnchor.x,
        physicalHand.y-weaponAnchor.y,physicalHand.z-weaponAnchor.z};
    return rotate(inverse,difference);
}
template<class Vec,class Quat,class Rotate>
inline Vec lockedHand(const Vec&weaponAnchor,const Quat&gunRotation,
    const Vec&local,Rotate rotate) {
    const auto delta=rotate(gunRotation,local);
    return {weaponAnchor.x+delta.x,weaponAnchor.y+delta.y,
        weaponAnchor.z+delta.z};
}
}
