#pragma once
namespace kharvox {
// DOOM's first-person scripted interactions can keep the normal gameplay
// camera while temporarily inhibiting native weapon control. Detect a
// stable interruption, then rebuild only temporary weapon-pose state.
struct ScriptedWeaponPoseBoundary {
    enum class Change {None,Suspend,Restore};
    bool observedControl{},suspended{};
    unsigned missing{},restored{};
    Change update(bool gameplay,bool weaponControl) noexcept {
        if(!gameplay){restored=0;return Change::None;}
        if(weaponControl){
            observedControl=true;missing=0;
            if(suspended){
                if(++restored>=5){suspended=false;restored=0;return Change::Restore;}
            }
            return Change::None;
        }
        restored=0;
        if(!observedControl||suspended)return Change::None;
        if(++missing>=3){suspended=true;return Change::Suspend;}
        return Change::None;
    }
};
}
