#include "../src/openxr/ChainsawLoopProbePolicy.h"
#include <cassert>
#define CHECK(x) do { if(!(x)) return __LINE__; } while(false)
int main(){
    using namespace kharvox;
    ChainsawLoopProbeState s{};
    auto update=[&](bool exec,bool trig,bool tracked,bool armed,
                    bool committed,float speed,std::uint64_t ms,bool cam=false){
        return updateChainsawLoopProbe(s,{exec,trig,tracked,armed,committed,cam,speed,ms});
    };
    CHECK(!update(false,false,true,false,false,0,5).started);
    const auto start=update(true,true,true,false,false,0,100);
    CHECK(start.started&&start.sample&&s.killIndex==1);
    CHECK(!update(true,true,true,true,false,.5f,120).sample);
    CHECK(update(true,true,true,true,false,.5f,150).sample);
    CHECK(!update(true,true,true,true,false,.01f,200).paused);
    CHECK(update(true,true,true,true,false,.01f,370).paused);
    CHECK(s.pauseCount==1&&s.sawPause);
    CHECK(update(true,true,true,true,false,.6f,400).resumed);
    CHECK(s.resumeCount==1&&s.sawResume);
    CHECK(update(true,true,true,true,true,.6f,455,true).committed);
    CHECK(s.sawCameraSource&&s.sawCommitted);
    CHECK(!update(true,true,true,true,true,.6f,510).committed);
    CHECK(!update(true,true,false,true,true,0,550).started);
    const auto exit=update(false,true,true,false,false,0,600);
    CHECK(exit.ended&&!s.active);
    CHECK(s.lastDurationMs==500&&s.lastPauseCount==1);
    CHECK(s.lastResumeCount==1&&s.lastSawCommitted);
    CHECK(s.lastSawCameraSource);
    CHECK(s.lastTrackingLossCount>0);
    CHECK(!update(false,false,true,false,false,0,650).ended);
    CHECK(update(true,true,true,true,false,.3f,800).started);
    CHECK(s.killIndex==2&&!s.sawCommitted);
    return 0;
}
