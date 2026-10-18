#include "../src/camera/MajorStoryFocusPolicy.h"
#include <cmath>
#define CHECK(x) do { if(!(x)) return __LINE__; } while(false)
int main(){
 using kharvox::MajorStoryFocusState;
 using kharvox::updateMajorStoryFocus;
 MajorStoryFocusState shortScene{};
 CHECK(updateMajorStoryFocus(shortScene,100,true,true,0).weight==0);
 CHECK(updateMajorStoryFocus(shortScene,700,true,true,70).weight==0);
 CHECK(updateMajorStoryFocus(shortScene,1200,true,true,70).weight==0);
 shortScene.reset();
 CHECK(!shortScene.engaged);
 MajorStoryFocusState stationary{};
 CHECK(updateMajorStoryFocus(stationary,100,true,true,0).weight==0);
 CHECK(updateMajorStoryFocus(stationary,1600,true,true,6).weight==0);
 CHECK(!stationary.engaged);
 MajorStoryFocusState major{};
 CHECK(updateMajorStoryFocus(major,100,true,true,3).weight==0);
 CHECK(updateMajorStoryFocus(major,1500,true,true,24).weight==0);
 auto qualify=updateMajorStoryFocus(major,1600,true,true,30);
 CHECK(qualify.newlyEngaged&&qualify.weight==0);
 auto fade=updateMajorStoryFocus(major,1850,true,true,40);
 CHECK(fade.weight>0.45f&&fade.weight<0.55f);
 CHECK(updateMajorStoryFocus(major,2150,true,true,45).weight==1);
 CHECK(updateMajorStoryFocus(major,2200,true,false,45).weight==0);
 MajorStoryFocusState userOff{};
 CHECK(updateMajorStoryFocus(userOff,100,false,true,0).weight==0);
 CHECK(updateMajorStoryFocus(userOff,2200,false,true,80).weight==0);
 CHECK(!userOff.engaged);
 CHECK(std::fabs(kharvox::shortestYawDegrees(175,-175)+10)<0.01f);
 CHECK(std::fabs(kharvox::shortestYawDegrees(-175,175)-10)<0.01f);
 return 0;
}
