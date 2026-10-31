#include "../src/openxr/GestureWheelExitPolicy.h"
#include <cassert>

int main() {
    using namespace kharvox;
    // Opening/moving inside the zone is never a confirmation or cancellation.
    assert(classifyGestureWheelExit(false,true,false)==GestureWheelExit::None);
    assert(classifyGestureWheelExit(true,true,false)==GestureWheelExit::None);
    assert(classifyGestureWheelExit(true,true,true)==GestureWheelExit::None);
    // Removing Grip while still inside must leave the wheel open.
    assert(classifyGestureWheelExit(true,true,false)==GestureWheelExit::None);
    // Physical hand exit alone closes; Grip state determines commit.
    assert(classifyGestureWheelExit(true,false,false)==GestureWheelExit::Cancel);
    assert(classifyGestureWheelExit(true,false,true)==GestureWheelExit::Confirm);
    // No delayed restore: cancel holds the native button while neutral input
    // reaches the game for three frames, then releases on the following frame.
    GestureWheelCancelDrain drain{};
    assert(!drain.active());
    drain.cancel();
    for(int i=0;i<3;++i) {
        assert(drain.active());
        drain.frameSubmitted();
    }
    assert(!drain.active());
    drain.cancel();
    drain.reset();
    assert(!drain.active());
    GestureWheelCancelGuard guard{};
    guard.arm(1000000000ULL);
    assert(!guard.shouldRestore(1100000000ULL,false,true));
    assert(!guard.shouldRestore(1220000000ULL,false,true));
    assert(guard.active);
    assert(guard.shouldRestore(1350000000ULL,false,false));
    assert(!guard.shouldRestore(1400000000ULL,false,false));
    assert(guard.shouldRestore(1530000000ULL,false,false));
    assert(!guard.shouldRestore(2300000000ULL,false,false));
    assert(!guard.active);
    guard.arm(3000000000ULL);
    assert(!guard.shouldRestore(3300000000ULL,true,false));
    assert(!guard.active);
    return 0;
}
