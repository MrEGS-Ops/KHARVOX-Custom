#pragma once
#include <cmath>

namespace kharvox {
constexpr int radialCategoryCount=6;
constexpr int radialFlyoutCount=5;
constexpr int radialPanelWidth=260;
constexpr int radialRowTop=170;
constexpr int radialRowStep=48;
constexpr int radialRowHeight=44;
constexpr int radialCenter=99;
constexpr int radialRowTarget=200;
constexpr int radialControlWidth=48;
constexpr double radialStep=6.283185307179586/radialCategoryCount;

inline double radialAngle(int i){return -1.570796326794897+i*radialStep;}
inline int radialPanelX(int i){return std::cos(radialAngle(i))<-.01?8:692;}
inline int radialRowY(int i){return radialRowTop+i*radialRowStep;}
inline int radialHit(float x,float y,int category,int count){
    if(category>=0&&category<radialFlyoutCount){
        const int left=radialPanelX(category);
        if(x>=left&&x<left+radialPanelWidth&&y>=radialRowTop){
            const int row=int((y-radialRowTop)/radialRowStep);
            if(row<count&&y<radialRowY(row)+radialRowHeight){
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
