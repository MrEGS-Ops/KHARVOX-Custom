#include "../src/openxr/DamageHapticsPolicy.h"
#include <cmath>

static bool near(float a,float b) { return std::fabs(a-b)<0.001f; }

int main() {
    using namespace kharvox;
    DamageHapticState state{};
    const DamageHapticConfig cfg{};
    auto sample = PlayerVitalsSample{100,100,true,true,1};
    if (updateDamageHaptics(state,sample,100,cfg).active) return 1; // baseline
    sample.armour=90; // ten AP at 50ms = 500ms, fixed amplitude
    auto pulse=updateDamageHaptics(state,sample,200,cfg);
    if (!pulse.active || pulse.remainingMilliseconds!=500
        || !near(pulse.amplitude,0.7f) || !near(pulse.frequencyHz,115.f)) return 2;
    if (updateDamageHaptics(state,sample,650,cfg).remainingMilliseconds!=50) return 3;
    if (updateDamageHaptics(state,sample,700,cfg).active) return 4;
    sample.armour=95; // pickup never triggers
    if (updateDamageHaptics(state,sample,800,cfg).active) return 5;
    sample.health=90; // 10HP, fixed intensity, 500ms
    pulse=updateDamageHaptics(state,sample,900,cfg);
    if (!pulse.active || pulse.remainingMilliseconds!=500) return 6;
    sample.health=89;
    pulse=updateDamageHaptics(state,sample,920,cfg);
    if (!pulse.active || pulse.remainingMilliseconds!=480) return 7;
    sample.armour=0; // huge AP loss capped against the initial pulse start
    pulse=updateDamageHaptics(state,sample,925,cfg);
    if (pulse.remainingMilliseconds!=575) return 8;
    if (updateDamageHaptics(state,sample,1500,cfg).active) return 9;
    sample.health=88; // cooldown, so cannot queue infinite damage
    if (updateDamageHaptics(state,sample,1510,cfg).active) return 10;
    sample.health=87;
    pulse=updateDamageHaptics(state,sample,1600,cfg);
    if (!pulse.active || pulse.remainingMilliseconds!=50) return 11;
    sample={100,100,true,false,1}; // menu cuts effects
    if (updateDamageHaptics(state,sample,1620,cfg).active) return 12;
    sample.gameplay=true; // new baseline rather than phantom health change
    if (updateDamageHaptics(state,sample,1700,cfg).active) return 13;
    sample.health=0;
    if (!updateDamageHaptics(state,sample,1800,cfg).active) return 14;
    sample={100,100,true,true,2}; // respawn reset, no pulse
    if (updateDamageHaptics(state,sample,1900,cfg).active) return 15;
    resetDamageHaptics(state);
    if (state.hasBaseline || state.activeUntilMilliseconds) return 16;
    if (damagePulseDuration(1,cfg)!=50 || damagePulseDuration(10,cfg)!=500
        || damagePulseDuration(100,cfg)!=600 || damagePulseDuration(0,cfg)!=0)
        return 17;
    return 0;
}
