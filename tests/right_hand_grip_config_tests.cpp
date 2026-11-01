#include "../src/openxr/RightHandGripConfig.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <cstdio>
int main(){
 const char*path="kharvox_right_hand_config_test.tmp";
 {std::ofstream f(path);f<<"gauss_cannon = -0.04 0.07 -0.02\nchaingun = 0.03 -0.06 0.01\n"; }
 auto a=kharvox::readRightHandGripSettings(path);
 assert(a.accepted==2&&a.rejected==0);
 assert(std::abs(a.gauss.x+0.04f)<0.0001f&&std::abs(a.chaingun.z-0.01f)<0.0001f);
 {std::ofstream f(path);f<<"gauss_cannon = NaN 0 0\nchaingun = 0.5 0 0\n";}
 a=kharvox::readRightHandGripSettings(path);
 assert(a.accepted==0&&a.rejected==2&&a.gauss.x==0.f&&a.chaingun.x==0.f);
 const auto sample=kharvox::RightHandGripOffset{-0.03f,0.04f,-0.02f};
 std::string why;
 {std::ofstream f(path);f<<"# keep me\n"
     <<"gauss_cannon = 0 0 0\nchaingun = 0.07 0 0\n";}
 assert(kharvox::saveRightHandGripMeasurement(path,"gauss_cannon",sample,&why));
 a=kharvox::readRightHandGripSettings(path);
 assert(std::abs(a.gauss.x+0.03f)<0.0001f);
 assert(std::abs(a.chaingun.x-0.07f)<0.0001f);
 {std::ifstream f(path);std::string comment;std::getline(f,comment);
  assert(comment=="# keep me");}
 assert(!kharvox::saveRightHandGripMeasurement(path,"shotgun",sample,&why));
 assert(!kharvox::saveRightHandGripMeasurement(path,"chaingun",{0.4f,0.f,0.f},&why));
 a=kharvox::readRightHandGripSettings(path);
 assert(std::abs(a.gauss.x+0.03f)<0.0001f);
 std::remove((std::string(path)+".before-capture.bak").c_str());
 std::remove(path);return 0;
}
