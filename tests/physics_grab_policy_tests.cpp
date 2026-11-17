#include "../src/openxr/PhysicsGrabPolicy.h"
#include <cstdlib>

static void require(bool ok) { if (!ok) std::abort(); }

int main() {
    using namespace kharvox;
    PhysicsGrabPolicy policy{};
    PhysicsGrabInput in{};
    in.enabled = in.gameplay = in.controllerTracked = in.gripDown = true;
    in.handDistanceFromHeadMeters = 0.6f;
    in.candidate = {42u, PhysicsGrabKind::MovableProp, true, true, false, 0.65f};

    // Only an authenticated, unoccluded native dynamic object in reach.
    in.candidate.kind = PhysicsGrabKind::StaticWorld;
    require(policy.update(0, in).action == PhysicsGrabAction::None);
    in.candidate.kind = PhysicsGrabKind::Unknown;
    require(policy.update(0, in).action == PhysicsGrabAction::None);
    in.candidate.kind = PhysicsGrabKind::Enemy;
    require(policy.update(0, in).action == PhysicsGrabAction::None);
    in.candidate.kind = PhysicsGrabKind::MovableProp;
    in.candidate.nativeDynamicPhysicsConfirmed = false;
    require(policy.update(0, in).action == PhysicsGrabAction::None);
    in.candidate.nativeDynamicPhysicsConfirmed = true;
    in.candidate.unobstructed = false;
    require(policy.update(0, in).action == PhysicsGrabAction::None);
    in.candidate.unobstructed = true;
    in.candidate.handDistanceMeters = 0.91f;
    require(policy.update(0, in).action == PhysicsGrabAction::None);
    in.candidate.handDistanceMeters = 0.65f;
    in.candidate.scriptedOrProtected = true;
    require(policy.update(0, in).action == PhysicsGrabAction::None);
    in.candidate.scriptedOrProtected = false;

    auto pulled = policy.update(0, in);
    require(pulled.action == PhysicsGrabAction::Pull && pulled.ownsGrip
            && pulled.ownsTrigger && pulled.id == 42u);
    // Other hand cannot steal an already captured barrel.
    require(policy.update(1, in).action == PhysicsGrabAction::None);
    // First hand still cannot throw before native hold confirmation.
    in.triggerDown = true;
    require(policy.update(0, in).action == PhysicsGrabAction::Pull);
    // A trigger already pressed at pickup must not throw automatically.
    in.nativeHoldConfirmed = true;
    in.nativeHeldObjectStillDynamic = true;
    require(policy.update(0, in).action == PhysicsGrabAction::Hold);
    in.triggerDown = false;
    require(policy.update(0, in).action == PhysicsGrabAction::Hold);
    in.triggerDown = true;
    auto thrown = policy.update(0, in);
    require(thrown.action == PhysicsGrabAction::Throw && thrown.id == 42u);
    // Holding grip after a throw cannot recapture until physical release.
    require(policy.update(0, in).action == PhysicsGrabAction::None);
    in.gripDown = false;
    policy.update(0, in);
    in.gripDown = true;
    in.triggerDown = false;
    in.nativeHoldConfirmed = false;
    require(policy.update(0, in).action == PhysicsGrabAction::Pull);
    in.nativeHoldConfirmed = in.nativeHeldObjectStillDynamic = true;
    require(policy.update(0, in).action == PhysicsGrabAction::Hold);
    in.gripDown = false;
    require(policy.update(0, in).action == PhysicsGrabAction::Drop);

    // Only the hand that owns the object captures trigger/grip.
    in.gripDown = true;
    in.nativeHoldConfirmed = false;
    in.candidate.id = 77;
    require(policy.update(1, in).action == PhysicsGrabAction::Pull);
    in.candidate.id = 78;
    require(policy.update(0, in).action == PhysicsGrabAction::Pull);
    // Pause, controller disconnect, overreach, and two-hand support are safe releases.
    in.controllerTracked = false;
    require(policy.update(1, in).action == PhysicsGrabAction::Cancel);
    in.controllerTracked = true;
    require(policy.update(1, in).action == PhysicsGrabAction::None);
    in.gripDown = false;
    policy.update(1, in);
    in.gripDown = true;
    in.nativeHoldConfirmed = false;
    in.candidate.id = 90;
    require(policy.update(1, in).action == PhysicsGrabAction::Pull);
    in.nativeHoldConfirmed = in.nativeHeldObjectStillDynamic = true;
    require(policy.update(1, in).action == PhysicsGrabAction::Hold);
    in.handDistanceFromHeadMeters = 1.5f;
    require(policy.update(1, in).action == PhysicsGrabAction::Drop);
    in.handDistanceFromHeadMeters = 0.6f;
    in.weaponSupportGripOwnsPress = true;
    require(policy.update(0, in).action == PhysicsGrabAction::Cancel);

    // Explicit future enemy opt-in never bypasses native dynamic-body checks.
    PhysicsGrabPolicy enemies{};
    enemies.enemyGrabbingEnabled = true;
    in.weaponSupportGripOwnsPress = false;
    in.candidate = {123u, PhysicsGrabKind::Enemy, true, true, false, 0.6f};
    require(enemies.update(1, in).action == PhysicsGrabAction::Pull);
    enemies = {};
    in.candidate.nativeDynamicPhysicsConfirmed = false;
    require(enemies.update(1, in).action == PhysicsGrabAction::None);
    // Default stays OFF despite an otherwise valid target.
    in.candidate = {456u, PhysicsGrabKind::MovableProp, true, true, false, 0.5f};
    in.enabled = false;
    require(enemies.update(1, in).action == PhysicsGrabAction::None);
}
