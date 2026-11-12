#include "../src/openxr/RightHandGripConfig.h"
#include "../src/openxr/VrHandMenuPolicy.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <cstdio>
int main(){
 const char*path="kharvox_right_hand_config_test.tmp";
 {std::ofstream f(path);f<<"gauss_cannon = -0.04 0.07 -0.02\nchaingun = 0.03 -0.06 0.01\n"; }
 auto a=kharvox::readRightHandGripSettings(path);
 assert(a.accepted==2&&a.rejected==0);
 assert(std::abs(a.rightHandDropMeters-0.08f)<0.0001f);
 assert(std::abs(a.gauss.x+0.04f)<0.0001f && std::abs(a.chaingun.z-0.01f)<0.0001f);
 {std::ofstream f(path);f<<"gauss_cannon = NaN 0 0\nchaingun = 1.1 0 0\n";}
 a=kharvox::readRightHandGripSettings(path);
 assert(a.accepted==0&&a.rejected==2&&a.gauss.x==0.f&&a.chaingun.x==0.f);
 {std::ofstream f(path);f<<"# user settings\nright_hand_drop_m = 0.11\ngauss_cannon = 0 0 0\nchaingun = 0 0 0\n";}
 a=kharvox::readRightHandGripSettings(path);
 assert(a.accepted==3&&a.rejected==0);
 assert(std::abs(a.rightHandDropMeters-0.11f)<0.0001f);
 {std::ofstream f(path);f<<"right_hand_drop_m = 0.50\n";}
 a=kharvox::readRightHandGripSettings(path);
 assert(a.rejected==1);
 assert(std::abs(a.rightHandDropMeters-0.08f)<0.0001f);
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
 // Per-weapon RIGHT-hand rotation is stored independently of position.
 const kharvox::RightHandGripRotation orientation{0.f,0.70710678f,0.f,
     0.70710678f,true};
 assert(kharvox::saveRightHandGripRotation(
     path,"chaingun",orientation,&why));
 a=kharvox::readRightHandGripSettings(path);
 assert(a.accepted==3&&a.rejected==0);
 assert(a.chaingunRotation.valid);
 assert(std::abs(a.chaingunRotation.y-0.70710678f)<.0001f);
 assert(!a.gaussRotation.valid);
 assert(std::abs(a.gauss.z-.50f)<.0001f);
 assert(std::abs(a.chaingun.z-.66f)<.0001f);
 assert(!kharvox::saveRightHandGripRotation(
     path,"shotgun",orientation,&why));
 // A melee reset or runtime reload reads the same persistent rotation.
 const auto restored=kharvox::readRightHandGripSettings(path);
 assert(restored.chaingunRotation.valid
     &&std::abs(restored.chaingunRotation.w-.70710678f)<.0001f);
 {std::ofstream out(path);out<<"gauss_cannon_rotation = 0 0 0 0\\n";}
 a=kharvox::readRightHandGripSettings(path);
 assert(a.rejected==1&&!a.gaussRotation.valid);
 // A virtual probe 15 cm forward (-Z) of the LEFT controller allows the
 // right glove to be positioned without physically overlapping controllers.
 const auto probe=kharvox::liveHeavyGripProbePoint(
     {1.f,2.f,3.f},{0.f,0.f,0.f,1.f,true});
 assert(std::abs(probe.x-1.f)<.00001f);
 assert(std::abs(probe.y-2.f)<.00001f);
 assert(std::abs(probe.z-2.85f)<.00001f);
 const auto yawProbe=kharvox::liveHeavyGripProbePoint(
     {1.f,2.f,3.f},{0.f,.70710678f,0.f,.70710678f,true});
 assert(std::abs(yawProbe.x-.85f)<.00001f);
 assert(std::abs(yawProbe.y-2.f)<.00001f);
 assert(std::abs(yawProbe.z-3.f)<.00001f);
 const auto pitchProbe=kharvox::liveHeavyGripProbePoint(
     {1.f,2.f,3.f},{.70710678f,0.f,0.f,.70710678f,true});
 assert(std::abs(pitchProbe.x-1.f)<.00001f);
 assert(std::abs(pitchProbe.y-2.15f)<.00001f);
 assert(std::abs(pitchProbe.z-3.f)<.00001f);
 const auto unnormalized=kharvox::liveHeavyGripProbePoint(
     {1.f,2.f,3.f},{0.f,0.f,0.f,2.f,true});
 assert(std::abs(unnormalized.z-2.85f)<.00001f);
 const auto invalidProbe=kharvox::liveHeavyGripProbePoint(
     {0,0,0},{0,0,0,0,false});
 assert(!kharvox::validRightHandVisualPlacement(invalidProbe));
 // LIVE capture must atomically save a position and orientation without
 // moving the pre-existing verified weapon anchor or the other gun.
 const kharvox::RightHandGripRotation liveRotation{
     0.2f,0.3f,0.4f,0.84261498f,true};
 {std::ofstream f(path);
  f<<"# keep owner settings\n"
   <<"gauss_cannon = -0.09 0.21 0.50\n"
   <<"chaingun = -0.13 0.30 0.66\n"; }
 assert(kharvox::saveRightHandVisualPose(path,"gauss_cannon",false,
     {-.08f,.10f,.23f},liveRotation,&why));
 assert(kharvox::saveRightHandVisualPose(path,"gauss_cannon",true,
     {-.12f,.06f,.31f},liveRotation,&why));
 assert(kharvox::saveRightHandVisualPose(path,"chaingun",false,
     {-.17f,.09f,.24f},liveRotation,&why));
 assert(kharvox::saveRightHandVisualPose(path,"chaingun",true,
     {-.21f,.08f,.30f},liveRotation,&why));
 assert(kharvox::saveHeavyAlternativeSupport(path,"gauss_cannon",
     {-.25f,.03f,-.15f},&why));
 assert(kharvox::saveHeavyAlternativeSupport(path,"chaingun",
     {-.21f,.06f,-.11f},&why));
 a=kharvox::readRightHandGripSettings(path);
 assert(a.rejected==0&&a.accepted==12);
 assert(a.gaussHandValid&&a.gaussSiegeHandValid);
 assert(a.chaingunHandValid&&a.chaingunTurretHandValid);
 assert(a.gaussSiegeRotation.valid&&a.chaingunTurretRotation.valid);
 assert(a.gaussSiegeSupportValid&&a.chaingunTurretSupportValid);
 assert(std::abs(a.gauss.z-.50f)<.0001f);
 assert(std::abs(a.chaingun.z-.66f)<.0001f);
 assert(std::abs(a.gaussSiegeHand.z-.31f)<.0001f);
 assert(std::abs(a.chaingunTurretHand.z-.30f)<.0001f);
 assert(!kharvox::saveRightHandVisualPose(
     path,"invalid_weapon",false,{0,0,0},liveRotation,&why));
 // Valid model-local glove pose may be more than 85 cm from the gun
 // anchor, while the physical GUN anchor retains its strict 85 cm limit.
 assert(kharvox::saveRightHandVisualPose(
     path,"chaingun",false,{1.1f,0,0},liveRotation,&why));
 a=kharvox::readRightHandGripSettings(path);
 assert(a.chaingunHandValid&&std::abs(a.chaingunHand.x-1.1f)<.0001f);
 assert(!kharvox::saveRightHandVisualPose(
     path,"chaingun",false,{1.81f,0,0},liveRotation,&why));
 assert(!kharvox::saveRightHandVisualPose(
     path,"chaingun",false,{0,0,0},{0,0,0,0,false},&why));
 a=kharvox::readRightHandGripSettings(path);
 assert(a.rejected==0&&a.gaussSiegeRotation.valid);
 {std::ifstream f(path);std::string comment;std::getline(f,comment);
  assert(comment=="# keep owner settings");}
 std::remove((std::string(path)+".before-live-capture.bak").c_str());
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
 // Capturing a new heavy support must update ONLY that gun.
 assert(kharvox::saveHeavySupportGrip(twoHand,"gauss_cannon",
     {-.12f,.07f,-.20f},true,.20f,1.f,&why));
 assert(kharvox::saveHeavySupportGrip(twoHand,"chaingun",
     {-.10f,.11f,-.35f},true,.24f,.8f,&why));
 {std::ifstream in(twoHand);
  std::string text((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
  assert(text.find("2 shotgun barrel 0 0 -0.25 0.2 1")!=std::string::npos);
  assert(text.find("2 chainsaw side-grip -0.1500000 0.1000000 -0.0400000")!=std::string::npos);
  assert(text.find("2 gauss_cannon side-grip -0.1200000 0.0700000 -0.2000000")!=std::string::npos);
  assert(text.find("2 chaingun side-grip -0.1000000 0.1100000 -0.3500000")!=std::string::npos);
 }
 assert(!kharvox::saveHeavySupportGrip(twoHand,"invalid_weapon",
     {0,0,0},true,.2f,1.f,&why));
 std::remove((twoHand+".before-heavy-support.bak").c_str());
 std::remove((twoHand+".before-chainsaw-capture.bak").c_str());

 // Per-weapon VR hand menu: right hand saves position + quaternion. Left hand
 // support stores only a point; its quaternion remains a grab-time snapshot.
 auto menu=kharvox::VrHandMenuState{};
 assert(!menu.alternateProfile);
 menu.alternateProfile=true;
 assert(menu.alternateProfile); // Alternate heavy profile is a menu option, not a chord.
 assert(!kharvox::vrHandMenuBothStickEdge(menu,false,false));
 assert(kharvox::vrHandMenuBothStickEdge(menu,true,true));
 assert(!kharvox::vrHandMenuBothStickEdge(menu,true,true));
 assert(!kharvox::vrHandMenuBothStickEdge(menu,false,false));
 assert(kharvox::vrHandMenuBothStickEdge(menu,true,true));
 assert(kharvox::saveRightHandVisualPose(path,"pistol",false,
     {.05f,.02f,-.12f},{0.f,0.f,0.f,1.f,true},&why));
 assert(kharvox::saveRightHandVisualPose(path,"shotgun",false,
     {.07f,.03f,-.15f},{0.f,0.f,0.f,1.f,true},&why));
 auto visual=kharvox::readRightHandGripSettings(path);
 assert(visual.visualPositions.count("pistol")==1);
 assert(visual.visualRotations.count("pistol")==1);
 assert(visual.visualPositions.count("shotgun")==1);
 // Primary-hand calibration statuses must be independent of support grips.
 assert(!kharvox::hasSavedVrPrimaryPose(visual,"pistol",true));
 assert(kharvox::hasSavedVrPrimaryPose(visual,"pistol",false));
 assert(kharvox::saveRightHandVisualPose(path,"pistol",false,
     {-.08f,.05f,-.11f},{0.f,0.f,0.f,1.f,true},&why,true));
 assert(kharvox::saveRightHandVisualPose(path,"gauss_cannon",true,
     {-.15f,.06f,-.20f},{0.f,0.f,0.f,1.f,true},&why,true));
 assert(kharvox::saveHeavyAlternativeSupport(path,"gauss_cannon",
     {-.04f,.10f,-.12f},&why,true));
 visual=kharvox::readRightHandGripSettings(path);
 assert(kharvox::hasSavedVrPrimaryPose(visual,"pistol",false));
 assert(kharvox::hasSavedVrPrimaryPose(visual,"pistol",true)); // BOTH
 assert(visual.leftVisualPositions.count("gauss_cannon_siege")==1);
 assert(visual.leftVisualRotations.count("gauss_cannon_siege")==1);
 assert(visual.leftPrimaryAlternateSupport.count("gauss_cannon_siege")==1);
 assert(kharvox::saveHeavySupportGrip(twoHand,"pistol",
     {.03f,.04f,-.20f},false,.2f,1.f,&why));
 assert(kharvox::hasSavedVrSupport(twoHand,"pistol"));
 assert(kharvox::resetVrWeaponCalibrationFile(path,"pistol",true,&why));
 assert(kharvox::resetVrWeaponCalibrationFile(twoHand,"pistol",false,&why));
 visual=kharvox::readRightHandGripSettings(path);
 assert(visual.visualPositions.count("pistol")==0);
 assert(visual.visualRotations.count("pistol")==0);
 assert(visual.leftVisualPositions.count("pistol")==0);
 assert(visual.leftVisualRotations.count("pistol")==0);
 assert(!kharvox::hasSavedVrPrimaryPose(visual,"pistol",true));
 assert(visual.visualPositions.count("shotgun")==1);
 assert(visual.leftVisualPositions.count("gauss_cannon_siege")==1);
 assert(kharvox::resetVrWeaponCalibrationFile(path,"gauss_cannon",true,&why));
 visual=kharvox::readRightHandGripSettings(path);
 assert(visual.leftVisualPositions.count("gauss_cannon_siege")==0);
 assert(visual.leftPrimaryAlternateSupport.count("gauss_cannon_siege")==0);
 assert(!kharvox::hasSavedVrSupport(twoHand,"pistol"));
 assert(kharvox::hasSavedVrSupport(twoHand,"shotgun"));
 std::remove((std::string(path)+".before-vr-reset.bak").c_str());
 std::remove((twoHand+".before-vr-reset.bak").c_str());
 std::remove(twoHand.c_str());
 std::remove(path);return 0;
}
