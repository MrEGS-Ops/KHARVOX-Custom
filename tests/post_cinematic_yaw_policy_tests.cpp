#include "../src/openxr/PostCinematicYawPolicy.h"

#include <cmath>

namespace {

bool near(float actual, float expected, float tolerance = 0.0001f) {
    return std::fabs(actual - expected) <= tolerance;
}

} // namespace

int main() {
    kharvox::PostCinematicYawGuard guard{};
    kharvox::armPostCinematicYawGuard(guard, 1000000000);
    if (!guard.active || guard.stableFrames != 0 || !near(guard.absorbedDegrees, 0.0f))
        return 1;

    auto update = kharvox::updatePostCinematicYawGuard(
        guard, 1010000000, 12.0f, true);
    if (!update.absorbBodyDelta
        || update.exit != kharvox::PostCinematicYawGuardExit::None
        || guard.stableFrames != 0 || !near(guard.absorbedDegrees, 12.0f))
        return 2;

    for (int frame = 0; frame < kharvox::postCinematicYawStableFramesRequired; ++frame) {
        update = kharvox::updatePostCinematicYawGuard(
            guard, 1520000000 + frame * 10000000, 0.01f, true);
    }
    if (guard.active
        || update.exit != kharvox::PostCinematicYawGuardExit::Stable
        || !near(guard.absorbedDegrees, 12.12f, 0.001f))
        return 3;

    kharvox::armPostCinematicYawGuard(guard, 2000000000);
    for (int frame = 0; frame < kharvox::postCinematicYawStableFramesRequired + 2; ++frame) {
        update = kharvox::updatePostCinematicYawGuard(
            guard, 2010000000 + frame * 10000000, 0.0f, true);
    }
    if (!guard.active || update.exit != kharvox::PostCinematicYawGuardExit::None)
        return 4;

    update = kharvox::updatePostCinematicYawGuard(
        guard, 8000000000, -4.0f, true);
    if (guard.active || !update.absorbBodyDelta
        || update.exit != kharvox::PostCinematicYawGuardExit::Timeout
        || !near(guard.absorbedDegrees, -4.0f))
        return 5;

    kharvox::armPostCinematicYawGuard(guard, 9000000000);
    if (!kharvox::cancelPostCinematicYawGuard(guard) || guard.active)
        return 6;
    if (kharvox::cancelPostCinematicYawGuard(guard))
        return 7;

    kharvox::armPostCinematicYawGuard(guard, 10000000000);
    update = kharvox::updatePostCinematicYawGuard(
        guard, 10100000000, 0.0f, false);
    if (!guard.active || update.absorbBodyDelta || guard.stableFrames != 0)
        return 8;

    kharvox::resetPostCinematicYawGuard(guard);
    if (guard.active || guard.startedAt != 0 || guard.lastMotionAt != 0
        || guard.stableFrames != 0
        || !near(guard.absorbedDegrees, 0.0f))
        return 9;

    // Once a kill ends, never activate hidden 600-deg/s body alignment
    // while the player is RUNNING. A post-cinematic guard can end at 500ms,
    // but body follow may only resume after the locomotion stick is neutral.
    kharvox::PostCinematicBodyTurnRecovery recovery{};
    kharvox::armPostCinematicBodyTurnRecovery(recovery);
    if(!recovery.active
        ||!kharvox::suppressAutomaticBodyTurnAfterCinematic(
            recovery,1000000000,true,false,true))
        return 10;
    if(!kharvox::suppressAutomaticBodyTurnAfterCinematic(
            recovery,4000000000,true,false,false))
        return 11; // moving for 3 seconds must NOT produce a delayed snap
    if(!kharvox::suppressAutomaticBodyTurnAfterCinematic(
            recovery,4010000000,false,false,false))
        return 12; // stick just released: idle settling window
    if(!kharvox::suppressAutomaticBodyTurnAfterCinematic(
            recovery,4250000000,false,false,false))
        return 13;
    if(kharvox::suppressAutomaticBodyTurnAfterCinematic(
            recovery,4260000000,false,false,false))
        return 14; // gentle automatic alignment now allowed
    if(!recovery.active
        ||!near(kharvox::postCinematicBodyTurnRecoveryDegreesPerSecond,90.f))
        return 15;
    if(kharvox::suppressAutomaticBodyTurnAfterCinematic(
            recovery,4300000000,false,true,false)||recovery.active)
        return 16; // explicit right stick always owns the turn
    kharvox::armPostCinematicBodyTurnRecovery(recovery);
    kharvox::resetPostCinematicBodyTurnRecovery(recovery);
    if(recovery.active||recovery.idleSince!=0)
        return 17;
    return 0;
}
