#pragma once
#include <cmath>
#include <algorithm>

namespace kharvox {
constexpr int radialCategoryCount=6;
constexpr int radialPainsawIndex=3;
constexpr int radialFlyoutCount=5; // count only; PAINSAW index is not last
inline constexpr bool radialHasFlyout(int i){return i>=0&&i<radialCategoryCount&&i!=radialPainsawIndex;}
constexpr int radialPanelWidth=260;
constexpr int radialRowTop=170;
constexpr int radialRowStep=48;
constexpr int radialRowHeight=44;
constexpr int radialCenter=99;
constexpr int radialRowTarget=200;
constexpr int radialControlWidth=48;
// Shared non-interactive feedback area, below the PAINSAW hover text.
constexpr int radialNoticeWidth=404,radialNoticeX=(960-radialNoticeWidth)/2;
constexpr int radialNoticeY=608,radialNoticeBottom=712;
constexpr double radialStep=6.283185307179586/radialCategoryCount;

inline double radialAngle(int i){return -1.570796326794897+i*radialStep;}
constexpr int radialWeaponGripIndex=0;
constexpr int radialWeaponGripTop=36;
inline int radialPanelX(int i){return i==radialWeaponGripIndex?(960-radialPanelWidth)/2:std::cos(radialAngle(i))<-.01?8:692;}
inline int radialPanelTop(int category){return category==radialWeaponGripIndex?radialWeaponGripTop:radialRowTop;}
inline int radialRowY(int category,int row){return radialPanelTop(category)+row*radialRowStep;}
inline int radialDescriptionX(int i){return radialPanelX(i)==8?692:8;}
// Pixel coverage for the analytic sector/rim, without blurring text or texture.
struct RadialCoverage {int category{-1};float outer{},inner{};};
inline RadialCoverage radialCoverage(float dx,float dy){
    const double r=std::hypot(dx,dy);
    auto coverage=[](double signedDistance){return float(std::clamp(signedDistance+.5,0.0,1.0));};
    if(r<60.5)return {-1,coverage(60-r),coverage(59-r)};
    if(r>202.5||r<60.5)return {};
    double a=std::atan2(dy,dx)-radialAngle(0)+radialStep/2;
    if(a<0)a+=radialCategoryCount*radialStep;
    const int cat=int(a/radialStep)%radialCategoryCount;
    const double local=a-std::floor(a/radialStep)*radialStep-radialStep/2;
    const double edge=r*std::sin((radialStep-.025)/2-std::abs(local));
    const double d=std::min({202-r,r-61,edge});
    return {cat,coverage(d),coverage(d-1)};
}
inline int radialHit(float x,float y,int category,int count){
    if(radialHasFlyout(category)){
        const int left=radialPanelX(category);
        if(x>=left&&x<left+radialPanelWidth&&y>=radialPanelTop(category)){
            const int row=int((y-radialPanelTop(category))/radialRowStep);
            if(row<count&&y<radialRowY(category,row)+radialRowHeight){
                const float offset=x-left;
                const int part=offset<radialControlWidth?0:
                    offset>=radialPanelWidth-radialControlWidth?2:1;
                return radialRowTarget+row*3+part;
            }
        }
    }
    const float dx=x-480,dy=y-360,rr=dx*dx+dy*dy;
    if(rr<60*60)return radialCenter;
    if(rr>202*202)return -1;
    double angle=std::atan2(dy,dx)-radialAngle(0)+radialStep/2;
    if(angle<0)angle+=radialCategoryCount*radialStep;
    return 100+int(angle/radialStep)%radialCategoryCount;
}

struct RadialDevUnlock {
    bool unlocked{};
    int clicks{};

    bool update(bool centered,bool rightGrip,bool triggerEdge){
        if(!centered||!rightGrip)clicks=0;
        if(!centered||!triggerEdge)return false;
        if(unlocked)return true;
        if(rightGrip&&++clicks==5){
            unlocked=true;
            clicks=0;
            return true;
        }
        return false;
    }
};

constexpr int calibrationGuideTop=112;
constexpr int weaponGuideHeight=126;
constexpr int weaponGuideButtonTop=calibrationGuideTop+78;
inline int weaponGuideAction(float x,float y,bool alternateAvailable){
    if(y<weaponGuideButtonTop||y>=weaponGuideButtonTop+40)return -1;
    if(x>=464&&x<616)return 0;
    if(x>=626&&x<778)return 1;
    if(alternateAvailable&&x>=788&&x<940)return 2;
    return -1;
}
}
