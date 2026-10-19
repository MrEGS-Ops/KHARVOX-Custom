#include "../src/openxr/ChainsawTriggerThrottlePolicy.h"
#include "../src/openxr/ChainsawHapticPreviewPolicy.h"
#include <cmath>
#include <limits>
#define CHECK(x) do {if(!(x))return __LINE__;}while(false)
int main(){
 using kharvox::evaluateChainsawTriggerThrottle;
 // Haptic preview protocol is data-only; no native gameplay/time/CVar writes.
 const std::uint64_t when=134203393000000000ull;
 const std::string uid="abcdef0123456789abcdef0123456789";
 const std::string prefix="KHARVOX_HAPTIC_PREVIEW_V1 "+
     std::to_string(when)+" "+uid;
 kharvox::ChainsawHapticPreviewRequest preview{};
 const std::string cutTest=prefix+" 0 1 20 90 45 20 35 75";
 CHECK(kharvox::parseChainsawHapticPreviewRequest(cutTest,when,preview));
 CHECK(preview.hand==0&&preview.cutting&&preview.config.roughnessPercent==35);
 const auto simulatedCut=kharvox::chainsawHapticPreviewSignal(preview,28);
 CHECK(simulatedCut.hapticAmplitude>.1f
     &&simulatedCut.tickHz==30);
 const auto stopRequest=prefix+" 2 0 20 90 45 20 35 75";
 CHECK(kharvox::parseChainsawHapticPreviewRequest(stopRequest,when,preview)
     &&preview.hand==2);
 CHECK(!kharvox::parseChainsawHapticPreviewRequest(cutTest,
     when+110000000ull,preview)); // reject older than 10 seconds
 CHECK(!kharvox::parseChainsawHapticPreviewRequest(
     prefix+" 0 1 20 200 45 20 35 75",when,preview)); // malformed level
 CHECK(!kharvox::parseChainsawHapticPreviewRequest(
     prefix+" 3 0 20 90 45 20 35 75",when,preview)); // unknown side
 CHECK(!kharvox::parseChainsawHapticPreviewRequest(
     cutTest+" garbage",when,preview)); // no extra commands

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
 // Haptic Lab parameters modify physical rumble but MUST NOT touch engine Hz.
 const kharvox::ChainsawHapticConfig heavy{};
 kharvox::ChainsawHapticConfig diesel=heavy;
 diesel.idlePercent=35;
 diesel.cuttingPercent=65;
 diesel.pulseRate=25;
 diesel.pulseDurationMs=35;
 diesel.roughnessPercent=10;
 diesel.motionSensitivityPercent=55;
 const auto heavyIdle=evaluateChainsawTriggerThrottle(0.f,3.f,1.8f,heavy);
 const auto dieselIdle=evaluateChainsawTriggerThrottle(0.f,3.f,1.8f,diesel);
 CHECK(heavyIdle.tickHz==dieselIdle.tickHz
     &&dieselIdle.hapticAmplitude>heavyIdle.hapticAmplitude);
 const auto heavyCut=evaluateChainsawTriggerThrottle(1.f,2.f,1.8f,heavy);
 const auto dieselCut=evaluateChainsawTriggerThrottle(1.f,2.f,1.8f,diesel);
 CHECK(heavyCut.tickHz==dieselCut.tickHz
     &&heavyCut.hapticAmplitude>dieselCut.hapticAmplitude);
 CHECK(heavyCut.hapticFrequencyHz!=dieselCut.hapticFrequencyHz);
 const auto smooth=kharvox::textureChainsawTriggerHaptics(heavyCut,0,
     kharvox::ChainsawHapticConfig{20,90,45,20,0,75});
 const auto rough=kharvox::textureChainsawTriggerHaptics(heavyCut,0,
     kharvox::ChainsawHapticConfig{20,90,45,20,100,75});
 CHECK(smooth.hapticAmplitude>rough.hapticAmplitude);
 kharvox::ChainsawHapticConfig longPulse=heavy;
 kharvox::ChainsawHapticConfig shortPulse=heavy;
 longPulse.pulseDurationMs=35;
 shortPulse.pulseDurationMs=5;
 CHECK(kharvox::textureChainsawTriggerHaptics(heavyCut,12,longPulse).hapticAmplitude
     >kharvox::textureChainsawTriggerHaptics(heavyCut,12,shortPulse).hapticAmplitude);
 kharvox::ChainsawHapticConfig sensitive=heavy;
 sensitive.motionSensitivityPercent=100;
 CHECK(evaluateChainsawTriggerThrottle(1.f,.9f,1.8f,sensitive).tickHz
     ==evaluateChainsawTriggerThrottle(1.f,.9f,1.8f,heavy).tickHz);
 CHECK(evaluateChainsawTriggerThrottle(1.f,.9f,1.8f,sensitive).hapticAmplitude
     >evaluateChainsawTriggerThrottle(1.f,.9f,1.8f,heavy).hapticAmplitude);
 const auto p=kharvox::textureChainsawTriggerHaptics(full,0);
 const auto q=kharvox::textureChainsawTriggerHaptics(full,28);
 CHECK(p.hapticAmplitude<q.hapticAmplitude);
 CHECK(q.hapticFrequencyHz==full.hapticFrequencyHz);
 CHECK(kharvox::textureChainsawTriggerHaptics(idle,0).hapticAmplitude==.20f);
 return 0;
}
