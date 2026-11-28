#pragma once
#include <cmath>
namespace kharvox {
constexpr double radialStep=6.283185307179586/7.0;
inline double radialAngle(int i){return -1.570796326794897+i*radialStep;}
inline int radialPanelX(int i){return std::cos(radialAngle(i))<-.01?8:692;}
inline int radialHit(float x,float y,int category,int count){
    if(category>=0&&category<5){
        const int left=radialPanelX(category);
        if(x>=left&&x<left+260&&y>=170){
            const int index=int((y-170)/49);
            if(index<count)return 200+index;
        }
    }
    const float dx=x-480,dy=y-360,rr=dx*dx+dy*dy;
    if(rr<60*60)return 99;
    if(rr>202*202)return -1;
    double angle=std::atan2(dy,dx)-radialAngle(0)+radialStep/2;
    if(angle<0)angle+=7*radialStep;
    return 100+int(angle/radialStep)%7;
}
}
