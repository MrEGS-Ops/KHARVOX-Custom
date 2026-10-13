#include "../src/openxr/OpenHandReleasePolicy.h"
#include <cassert>

int main(){
    using namespace kharvox;
    OpenHandReleaseState s{};
    OpenHandContacts c{};
    assert(!updateOpenHandRelease(s,c,true,1000000000ull)); // missing sensors: safe
    c.valid=true;
    c.indexTouch=true;
    assert(!updateOpenHandRelease(s,c,true,1050000000ull));
    c.indexTouch=false;
    c.thumbstickTouch=true;
    assert(!updateOpenHandRelease(s,c,true,1100000000ull));
    c.thumbstickTouch=false;
    c.grip=.5f;
    assert(!updateOpenHandRelease(s,c,true,1200000000ull));
    c.grip=0;
    assert(!updateOpenHandRelease(s,c,true,1400000000ull));
    assert(!updateOpenHandRelease(s,c,true,1800000000ull));
    assert(updateOpenHandRelease(s,c,true,1851000000ull));
    assert(!updateOpenHandRelease(s,c,true,2500000000ull)); // no repeat
    c.indexTouch=true;
    assert(!updateOpenHandRelease(s,c,true,2600000000ull));
    c.indexTouch=false;
    delayOpenHandRelease(s,2700000000ull);
    assert(!updateOpenHandRelease(s,c,true,2750000000ull));
    assert(!updateOpenHandRelease(s,c,true,3370000000ull));
    assert(updateOpenHandRelease(s,c,true,3830000000ull));
    s={};
    c.faceUpperTouch=true;
    assert(!updateOpenHandRelease(s,c,true,5000000000ull));
    c.faceUpperTouch=false;
    c.thumbrestTouch=true;
    assert(!updateOpenHandRelease(s,c,true,5100000000ull));
    c.thumbrestTouch=false;
    c.faceLowerTouch=true;
    assert(!updateOpenHandRelease(s,c,true,5200000000ull));
    c.faceLowerTouch=false;
    c.trigger=.2f;
    assert(!updateOpenHandRelease(s,c,true,5300000000ull));
    assert(!updateOpenHandRelease(s,c,false,5400000000ull));
    return 0;
}
