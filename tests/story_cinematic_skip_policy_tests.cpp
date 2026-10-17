#include "../src/openxr/StoryCinematicSkipPolicy.h"
#include <cassert>

// Input policy tests only. DOOM's actual timescale reader and headset
// interaction still require a live-game test.
int main() {
    using namespace kharvox;
    StoryCinematicSkipState s{};
    auto step=[&](unsigned long long ms,bool scene,bool press=false,
                  bool glory=false,bool chain=false,bool ledge=false,
                  bool boss=false,bool menu=false,bool world=true,
                  bool enabled=true) {
        return updateStoryCinematicSkip(s,enabled,scene,glory,chain,ledge,
                                       boss,menu,world,press,ms);
    };

    assert(!step(1000,true).eligible);
    assert(!step(1200,true,true).changed); // ignore click during warmup
    assert(!step(1350,true,false).eligible);
    auto on=step(1401,true,true);
    assert(on.eligible&&on.consumeRs&&on.changed&&on.accelerate);
    assert(!step(1500,true,true).changed); // one click, one toggle
    assert(step(1600,true,false).accelerate);
    auto off=step(1650,true,true);
    assert(off.changed&&!off.accelerate);
    step(1700,true,false);
    assert(step(1750,true,true).accelerate);
    auto timed=step(21751,true,false);
    assert(timed.changed&&timed.expired&&!timed.accelerate);

    // Menu, pause, level transition, unfocus (enabled=false) restore now.
    step(23000,true,false);
    assert(step(23001,true,true).accelerate);
    assert(step(23002,true,false,false,false,false,false,true).changed);
    assert(!s.accelerating);
    step(24000,true,false);
    assert(!step(24401,true,true,false,false,false,true).accelerate);
    assert(!step(24500,true,false).accelerate);
    assert(step(25000,true,true).accelerate);
    assert(step(25001,true,false,false,false,false,false,false,false).changed);
    assert(!s.accelerating);

    // Execution tail is never a new story scene, even if cutscene stays 1.
    StoryCinematicSkipState tail{};
    auto run=[&](unsigned long long t,bool cut,bool press=false,
                 bool glory=false,bool chainsaw=false,bool ledge=false) {
        return updateStoryCinematicSkip(tail,true,cut,glory,chainsaw,
            ledge,false,false,true,press,t);
    };
    assert(!run(100,true,false,true).eligible);
    assert(!run(1200,true,true).eligible);
    assert(!run(2200,true,false).eligible);
    assert(!run(2300,false).eligible);
    assert(!run(2450,true,false).eligible);
    assert(!run(2700,true,true).changed); // new story must warm up
    assert(run(3101,true,false).eligible);
    assert(run(3200,true,true).accelerate);
    assert(run(3201,true,false,false,true).changed); // chainsaw interrupts
    assert(!run(3300,true,true).accelerate);
    assert(!run(3350,false).accelerate);
    assert(!run(3700,true,false,false,false,true).accelerate);
    // Disabled flag never consumes gameplay input.
    StoryCinematicSkipState disabled{};
    const auto no=updateStoryCinematicSkip(disabled,false,true,
        false,false,false,false,false,true,true,1000);
    assert(!no.consumeRs&&!no.accelerate);
}
