#include "../src/openxr/StoryFreeWalkPolicy.h"
#include <cassert>
int main() {
    using namespace kharvox;
    StoryFreeWalkState st{};
    auto run=[&](unsigned long long now,bool cutscene,bool rs=false,
                 bool glory=false,bool chain=false,bool ledge=false,
                 bool boss=false,bool menu=false,bool world=true,
                 bool enabled=true) {
        return updateStoryFreeWalk(st,enabled,cutscene,glory,chain,ledge,
                                   boss,menu,world,rs,now);
    };
    assert(!run(100,true).eligible);
    assert(!run(300,true,true).changed);
    assert(!run(350,true,false).eligible);
    const auto start=run(510,true,true);
    assert(start.eligible&&start.changed&&start.requested);
    assert(!run(600,true,true).changed);
    assert(run(700,true,false).requested);
    const auto stop=run(800,true,true);
    assert(stop.changed&&!stop.requested);
    run(1000,true,false);
    assert(run(1100,true,true).requested);
    const auto expired=run(31101,true,false);
    assert(expired.expired&&expired.changed&&!expired.requested);
    run(32200,true,false);
    assert(run(32300,true,true).requested);
    const auto menu=run(32301,true,false,false,false,false,false,true);
    assert(menu.changed&&!menu.requested);
    run(33000,true,false);
    assert(!run(33300,true,true,false,false,false,true).requested);
    // No story hold on execution, scene tail, ledge and disabled feature.
    StoryFreeWalkState attack{};
    auto step=[&](unsigned long long now,bool cut,bool rs=false,
                  bool glory=false,bool chain=false,bool ledge=false,
                  bool enabled=true) {
        return updateStoryFreeWalk(attack,enabled,cut,glory,chain,ledge,
            false,false,true,rs,now);
    };
    assert(!step(100,true,false,true).eligible);
    assert(!step(900,true,true).eligible);
    assert(!step(1000,false).eligible);
    assert(!step(1200,true).eligible);
    assert(!step(1450,true,true).changed);
    assert(step(1650,true,false).eligible);
    assert(step(1700,true,true).requested);
    assert(step(1800,true,false,false,true).changed);
    assert(!step(1900,true,true).requested);
    assert(!step(2200,true,false,false,false,true).requested);
    assert(!step(3000,true,true,false,false,false,false).requested);
    // The only native-write candidates are stable horizontal movement bits
    // on the same player whose gameplay baseline had neither bit set.
    assert(storyHorizontalWriteAllowed(true,true,true,true,0,1));
    assert(storyHorizontalWriteAllowed(true,true,true,true,0,2));
    assert(storyHorizontalWriteAllowed(true,true,true,true,0,3));
    assert(!storyHorizontalWriteAllowed(false,true,true,true,0,3));
    assert(!storyHorizontalWriteAllowed(true,false,true,true,0,3));
    assert(!storyHorizontalWriteAllowed(true,true,false,true,0,3));
    assert(!storyHorizontalWriteAllowed(true,true,true,false,0,3));
    assert(!storyHorizontalWriteAllowed(true,true,true,true,1,3));
    assert(!storyHorizontalWriteAllowed(true,true,true,true,0,0));
}
