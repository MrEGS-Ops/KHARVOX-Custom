#pragma once
#include <array>
#include <algorithm>
#include <cmath>

namespace kharvox {
// The native idHands attachment joint sometimes jumps to a scripted
// interaction pose while the gameplay camera stays active. Suppress a large
// displacement relative to the settled neutral weapon joint, without ever
// rewriting the user-calibrated OpenXR hand offsets.
inline bool weaponPivotOutlier(const float sample[3],const float neutral[3]) noexcept {
    float dist2=0.f,norm2=0.f;
    for(int i=0;i<3;++i){
        if(!std::isfinite(sample[i])||!std::isfinite(neutral[i]))return true;
        const float diff=sample[i]-neutral[i];
        dist2+=diff*diff;norm2+=neutral[i]*neutral[i];
    }
    const float threshold=std::max(10.f,.5f*std::sqrt(norm2));
    return !std::isfinite(dist2)||dist2>threshold*threshold;
}
struct ScriptedWeaponPivotRecovery {
    enum class Event {None,Quarantine,Recovered};
    bool quarantined{};
    unsigned badFrames{},goodFrames{};
    Event observe(bool outlier) noexcept {
        if(outlier){
            goodFrames=0;
            if(badFrames<4)++badFrames;
            if(badFrames==4&&!quarantined){
                quarantined=true;
                return Event::Quarantine;
            }
            return Event::None;
        }
        badFrames=0;
        if(!quarantined)return Event::None;
        if(++goodFrames>=12){
            quarantined=false;goodFrames=0;
            return Event::Recovered;
        }
        return Event::None;
    }
};
}
