#include "../src/openxr/ChainsawTriggerThrottlePolicy.h"
#include <cmath>
#include <limits>
#define CHECK(x) do {if(!(x))return __LINE__;}while(false)
int main(){
 using kharvox::evaluateChainsawTriggerThrottle;
 const auto idle=evaluateChainsawTriggerThrottle(0.f,4.f);
 CHECK(!idle.motionEnabled&&idle.tickHz==240&&idle.hapticAmplitude==.20f);
 const auto noise=evaluateChainsawTriggerThrottle(.09f,5.f);
 CHECK(!noise.motionEnabled&&noise.tickHz==240);
 const auto still=evaluateChainsawTriggerThrottle(1.f,0.f);
 CHECK(still.motionEnabled&&still.tickHz==198&&still.hapticAmplitude>.6f);
 const auto full=evaluateChainsawTriggerThrottle(1.f,2.5f);
 CHECK(full.tickHz==30&&full.hapticAmplitude>=.89f);
 const auto middle=evaluateChainsawTriggerThrottle(.55f,2.5f);
 CHECK(middle.tickHz>full.tickHz&&middle.tickHz<idle.tickHz);
 const auto withoutTrigger=evaluateChainsawTriggerThrottle(0.f,20.f);
 CHECK(withoutTrigger.tickHz==240&&!withoutTrigger.motionEnabled);
 const auto bad=evaluateChainsawTriggerThrottle(
   std::numeric_limits<float>::quiet_NaN(),
   std::numeric_limits<float>::infinity());
 CHECK(!bad.motionEnabled&&bad.tickHz==240);
 const auto p=kharvox::textureChainsawTriggerHaptics(full,0);
 const auto q=kharvox::textureChainsawTriggerHaptics(full,28);
 CHECK(p.hapticAmplitude<q.hapticAmplitude);
 CHECK(q.hapticFrequencyHz==full.hapticFrequencyHz);
 CHECK(kharvox::textureChainsawTriggerHaptics(idle,0).hapticAmplitude==.20f);
 return 0;
}
