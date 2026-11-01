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
 {std::ofstream f(path);f<<"gauss_cannon = NaN 0 0\nchaingun = 1.1 0 0\n";}
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
 // Owner's latest captures were about 55 cm (Gauss) and 74 cm
 // (Chaingun) apart. Both must be accepted and persist through reload.
 assert(kharvox::saveRightHandGripMeasurement(path,"gauss_cannon",
     {-0.09f,0.21f,0.50f},&why));
 assert(kharvox::saveRightHandGripMeasurement(path,"chaingun",
     {-0.13f,0.30f,0.66f},&why));
 a=kharvox::readRightHandGripSettings(path);
 assert(a.accepted==2&&a.rejected==0);
 assert(std::abs(a.gauss.z-0.50f)<0.0001f);
 assert(std::abs(a.chaingun.z-0.66f)<0.0001f);
 assert(!kharvox::saveRightHandGripMeasurement(path,"chaingun",
     {0.80f,0.50f,0.10f},&why)); // magnitude >85 cm
 assert(!kharvox::saveRightHandGripMeasurement(path,"chaingun",
     {1.10f,0.f,0.f},&why));
 a=kharvox::readRightHandGripSettings(path);
 assert(std::abs(a.gauss.z-0.50f)<0.0001f);
 assert(std::abs(a.chaingun.z-0.66f)<0.0001f);
 std::remove((std::string(path)+".before-capture.bak").c_str());
 const std::string twoHand=std::string(path)+".twohand";
 const std::string original="2 shotgun barrel 0 0 -0.25 0.2 1\n"
  "2 chainsaw side-grip -0.13 0.23 0 0.2 1\n"
  "2 chaingun side-grip -0.31 0 0 0.2 1\n";
 {std::ofstream out(twoHand);out<<original;}
 assert(kharvox::saveChainsawSupportGrip(twoHand,{-0.15f,0.10f,-0.04f},
    0.2f,1.f,&why));
 {std::ifstream in(twoHand);
  std::string text((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
  assert(text.find("2 shotgun barrel 0 0 -0.25 0.2 1")!=std::string::npos);
  assert(text.find("2 chaingun side-grip -0.31 0 0 0.2 1")!=std::string::npos);
  assert(text.find("2 chainsaw side-grip -0.1500000 0.1000000 -0.0400000")!=std::string::npos);}
 {std::ifstream in(twoHand+".before-chainsaw-capture.bak");
  std::string text((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
  assert(text==original);}
 assert(!kharvox::saveChainsawSupportGrip(twoHand,{0.95f,0,0},0.2f,1.f,&why));
 std::remove((twoHand+".before-chainsaw-capture.bak").c_str());
 std::remove(twoHand.c_str());
 std::remove(path);return 0;
}
