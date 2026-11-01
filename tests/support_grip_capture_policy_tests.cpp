#include "../src/openxr/SupportGripCapturePolicy.h"
#include <cassert>
int main(){
    kharvox::SupportGripCaptureChord chord{};
    assert(!chord.pressed(false,false));
    assert(!chord.pressed(true,false));
    assert(chord.pressed(true,true));
    assert(!chord.pressed(true,true));
    assert(!chord.pressed(false,true));
    assert(chord.pressed(true,true));
    assert(!chord.pressed(false,false));
    assert(chord.pressed(true,true));
}
