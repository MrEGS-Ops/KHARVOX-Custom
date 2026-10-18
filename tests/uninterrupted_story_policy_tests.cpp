#include "../src/openxr/UninterruptedStoryPolicy.h"
#include <cassert>

int main() {
    using namespace kharvox;
    UninterruptedStoryState s{};
    auto step=[&](std::uint64_t t,bool cut=true,bool kill=false,
                  bool chain=false,bool ledge=false,bool boss=false,
                  bool menu=false,bool world=true,bool tracking=true,
                  bool enabled=true) {
        return updateUninterruptedStory(s,enabled,cut,kill,chain,ledge,
                                        boss,menu,world,tracking,t);
    };
    assert(!step(100).active);
    assert(!step(999).active);
    auto begin=step(1000);
    assert(begin.entered&&begin.active&&begin.eligible);
    assert(shouldRequestStoryMovement(begin.active,true,true));
    assert(!shouldRequestStoryMovement(begin.active,false,true));
    assert(!shouldRequestStoryMovement(begin.active,true,false));
    assert(step(1200).active);
    auto timeout=step(46001);
    assert(timeout.exited&&timeout.expired&&!timeout.active);
    assert(!step(50000).active); // stuck generic cutscene cannot rearm
    assert(!step(51000,false).active);
    assert(!step(52000).active);
    assert(step(52901).active);
    auto killed=step(52905,true,true);
    assert(killed.exited&&!killed.active);
    assert(!step(53000).active); // execution tail
    assert(!step(53100,false).active);
    assert(!step(53300).active);
    assert(!step(54201,true,false,false,false,true).active);
    assert(!step(54500,false).active);
    assert(!step(54600,true,false,false,false,false,true).active);
    assert(!step(54700,true,false,false,false,false,false,false).active);
    assert(!step(54800,true,false,false,false,false,false,true,false).active);
    assert(!step(54900,true,false,false,false,false,false,true,true,false).active);
    UninterruptedStoryState another{};
    assert(!updateUninterruptedStory(another,false,true,false,false,
        false,false,false,true,true,100).active);
    assert(!updateUninterruptedStory(another,true,true,false,false,
        false,false,false,true,true,200).active);
    assert(updateUninterruptedStory(another,true,true,false,false,
        false,false,false,true,true,1101).active);
    assert(updateUninterruptedStory(another,true,false,false,false,
        false,false,false,true,true,1102).exited);
}
