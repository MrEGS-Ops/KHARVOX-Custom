#include "../src/openxr/WorldGrabProbePolicy.h"
#include <cassert>
int main(){
  using namespace kharvox;
  WorldGrabDebugLaserInput ray{};
  assert(!worldGrabDebugLaserVisible(ray)); // OFF by default
  ray.enabled=ray.gameplay=ray.tracked=ray.controllersReady=true;
  assert(worldGrabDebugLaserVisible(ray)); // No Grip needed to inspect aim
  ray.wheelBusy=true;assert(!worldGrabDebugLaserVisible(ray));
  ray.wheelBusy=false;ray.supportBusy=true;
  assert(!worldGrabDebugLaserVisible(ray));
  ray.supportBusy=false;ray.menuOpen=true;
  assert(!worldGrabDebugLaserVisible(ray));
  ray.menuOpen=false;ray.tracked=false;
  assert(!worldGrabDebugLaserVisible(ray));
  ray.tracked=true;ray.controllersReady=false;
  assert(!worldGrabDebugLaserVisible(ray));
  ray.controllersReady=true;ray.enabled=false;
  assert(!worldGrabDebugLaserVisible(ray));
  WorldGrabProbeState state{};
  WorldGrabProbeInput p{};
  p.gameplay=p.controllersReady=true;
  p.tracked={true,true};p.freeHand={true,false};
  p.distanceFromHead={.5f,.5f};p.grip={true,true};
  assert(!state.update(p).started[0]); // factory OFF
  p.enabled=true;
  assert(!state.update(p).started[0]); // already held, no unexpected grab
  p.grip={false,false};state.update(p);
  p.grip={true,true};
  auto e=state.update(p);
  assert(e.started[0]&&!e.started[1]);
  assert(!state.update(p).started[0]);
  p.distanceFromHead[0]=1.1f;
  assert(state.update(p).pastReach[0]);
  assert(!state.update(p).pastReach[0]);
  p.supportBusy=true;
  assert(state.update(p).released[0]);
  p.supportBusy=false;
  assert(!state.update(p).started[0]); // fresh press required
  p.grip={false,false};state.update(p);
  p.freeHand={false,true};p.grip={true,true};
  assert(state.update(p).started[1]); // Gunslinger left weapon
  p.gameplay=false;
  assert(state.update(p).released[1]); // automatic reset on context exit
  return 0;
}
