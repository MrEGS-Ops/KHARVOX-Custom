#pragma once
namespace kharvox {
// Measured 2026-10-18: shortest complete 30-Hz Glory Kill ACTION-HZ interval
// was 929.139 ms; a 21.495 ms 1-present GK->chainsaw classifier transition
// is NOT a complete GK. Native sync enters ~20 ms after the action interval.
// Keep 700+90 ms before that lower bound with >100 ms expected margin.
// This is an experimental timing specific to the observed 30-Hz mode.
constexpr int kGkExitDelayMs=700;
constexpr int kChainsawExitDelayMs=40;
constexpr int kExitBlendMs=90;
constexpr int actionExitHandoffDelayMs(bool chainsawExecution,bool fullSpeedKill) noexcept {
    // For potentially much shorter full-speed bypass kills, retain the
    // original earliest release rather than assuming the 30-Hz measurements.
    return chainsawExecution||fullSpeedKill?kChainsawExitDelayMs:kGkExitDelayMs;
}
static_assert(kGkExitDelayMs+kExitBlendMs+100<929,
    "observed fastest complete GK needs at least 100ms headroom");
}
