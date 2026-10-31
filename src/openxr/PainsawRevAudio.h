#pragma once
#include <cstdint>

namespace kharvox {
// One-shot, real-time-independent PAINSAW rev cues. XAudio2 routes to the
// default Windows audio endpoint (normally the VR headset when configured).
// No DOOM sound-engine pointers, game assets, tick-Hz hooks or audio settings.
class PainsawRevAudio final {
public:
    // 1..3: initial native kill + the two confirmed release/re-pull edges.
    static bool Play(unsigned revCount) noexcept;
    static void Stop() noexcept;
};
} // namespace kharvox
