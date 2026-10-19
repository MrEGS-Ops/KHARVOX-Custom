using System;

namespace KharvoxLauncher;

/// <summary>
/// Experimental chainsaw throttle transfer function. Pure, deterministic math
/// shared by calibration and test code; the native VR runtime must supply
/// the analogue trigger and tracked weapon-hand speed before this is active.
/// Hz is the existing KHARVOX action tick target, NOT haptic pulse frequency.
/// </summary>
internal static class ChainsawThrottleModel
{
    internal readonly struct Output
    {
        public Output(int tickHz, float hapticStrength, bool motionEnabled)
        {
            TickHz = tickHz;
            HapticStrength = hapticStrength;
            MotionEnabled = motionEnabled;
        }
        public int TickHz { get; }
        public float HapticStrength { get; }
        public bool MotionEnabled { get; }
    }

    // In this action-speed system, LOWER Hz means faster animation.
    internal static Output Evaluate(float trigger, float handSpeedMetresPerSecond,
        float fullMotionSpeed = 1.8f, int idleTickHz = 240,
        int fastestTickHz = 30, float idleHaptics = 0.20f,
        float maxHaptics = 0.90f)
    {
        trigger = FiniteClamp(trigger, 0f, 1f);
        handSpeedMetresPerSecond = FiniteClamp(handSpeedMetresPerSecond, 0f, 20f);
        fullMotionSpeed = FiniteClamp(fullMotionSpeed, 0.1f, 20f);
        idleTickHz = Math.Max(30, Math.Min(240, idleTickHz));
        fastestTickHz = Math.Max(30, Math.Min(idleTickHz, fastestTickHz));
        idleHaptics = FiniteClamp(idleHaptics, 0f, 1f);
        maxHaptics = FiniteClamp(maxHaptics, idleHaptics, 1f);

        // A deadzone prevents accidental cutting from trigger sensor noise.
        const float threshold = 0.10f;
        if (trigger <= threshold)
            return new Output(idleTickHz, idleHaptics, false);

        float throttle = (trigger - threshold) / (1f - threshold);
        float motion = Math.Min(1f, handSpeedMetresPerSecond / fullMotionSpeed);
        // Stationary full-throttle can rev, but cannot reach full cutting speed.
        float cutting = throttle * (0.20f + 0.80f * motion);
        int hz = (int)Math.Round(idleTickHz - (idleTickHz - fastestTickHz) * cutting);
        float haptics = idleHaptics + (maxHaptics - idleHaptics) *
            throttle * (0.65f + 0.35f * motion);
        return new Output(hz, haptics, true);
    }

    private static float FiniteClamp(float value, float min, float max) =>
        float.IsNaN(value) || float.IsInfinity(value) ? min : Math.Max(min, Math.Min(max, value));
}
