#include "KharvoxModV1.h"
#include <cmath>
#include <algorithm>
#include <new>
namespace {
struct Wheel {
 int side=-1,sector=-1;
 bool open=false,inside[2]{},gripLatch=false;
 float ax=0,ay=0,sx=0,sy=0;
};
bool inZone(const KxHand& h,int side,bool prev) {
 if(!h.tracked||!std::isfinite(h.x)||!std::isfinite(h.y)||!std::isfinite(h.z))return false;
 const float out=side==KX_RIGHT?h.x:-h.x;
 return out>=(prev?-.16f:-.04f)&&out<=(prev?.66f:.44f)
 &&h.y>=(prev?-.35f:-.18f)&&h.y<=(prev?.54f:.36f)
 &&h.z>=(prev?-.13f:.015f)&&h.z<=(prev?.86f:.60f);
}
void clear(Wheel& s){s.side=-1;s.sector=-1;s.open=false;s.gripLatch=false;
 s.ax=s.ay=s.sx=s.sy=0;}
void cue(KxOutput& out,int side){out.flags|=side==KX_RIGHT?KX_HAPTIC_RIGHT:KX_HAPTIC_LEFT;}
}
KX_EXPORT uint32_t kharvox_mod_abi_version(){return KX_ABI;}
KX_EXPORT const char* kharvox_mod_id(){return "behind-head-weapon-wheel";}
KX_EXPORT void* kharvox_mod_create(){return new(std::nothrow) Wheel{};}
KX_EXPORT void kharvox_mod_destroy(void* p){delete static_cast<Wheel*>(p);}
KX_EXPORT int kharvox_mod_frame(void* p,const KxFrame* f,KxOutput* out){
 if(!p||!f||!out||f->size!=sizeof(KxFrame)||f->version!=KX_ABI
 ||out->size!=sizeof(KxOutput))return 0;
 auto& s=*static_cast<Wheel*>(p);
 *out={};out->size=sizeof(KxOutput);out->hand_side=-1;out->wheel_sector=-1;
 if(!f->gameplay||!f->tracking_valid||!f->wheel_supported||f->wheel_blocked){
   if(s.open)out->flags=KX_WHEEL_CANCEL|KX_WHEEL_NEUTRAL;
   clear(s);s.inside[0]=s.inside[1]=false;return 1;
 }
 bool entered[2]{};
 for(int i=0;i<2;++i){
   const bool now=inZone(f->hand[i],i,s.inside[i]);
   entered[i]=now&&!s.inside[i];if(entered[i])cue(*out,i);s.inside[i]=now;
 }
 if(!s.open){
   // A new entry is required: resting a hand behind the head won't auto-open.
   const int side=entered[0]?0:entered[1]?1:-1;
   if(side>=0){
     clear(s);s.open=true;s.side=side;
     s.ax=f->hand[side].x;s.ay=f->hand[side].y;
     out->hand_side=side;out->flags|=KX_WHEEL_OPEN|KX_WHEEL_HOLD;
   }
   return 1;
 }
 const int side=s.side;out->hand_side=side;const auto& h=f->hand[side];
 if(!s.inside[side]){
   out->flags|=(h.grip&&s.gripLatch)?KX_WHEEL_CONFIRM:(KX_WHEEL_CANCEL|KX_WHEEL_NEUTRAL);
   clear(s);return 1;
 }
 out->flags|=KX_WHEEL_HOLD;
 if(h.grip)s.gripLatch=true;else s.gripLatch=false;
 if(!s.gripLatch){
   const float dx=h.x-s.ax,dy=h.y-s.ay;
   const float length=std::sqrt(dx*dx+dy*dy);
   if(length>.02f){
     const float radius=std::clamp((length-.02f)/.045f,0.f,1.f);
     const float strength=.16f+.84f*radius;
     s.sx=dx/length*strength;s.sy=dy/length*strength;
     constexpr float pi=3.14159265358979323846f;
     float angle=std::atan2(dy,dx);if(angle<0)angle+=2*pi;
     const int next=int(std::fmod(angle+pi/8,2*pi)/(pi/4));
     if(next!=s.sector)cue(*out,side);s.sector=next;
   }else{s.sx=s.sy=0;s.sector=-1;}
 }
 out->wheel_x=s.sx;out->wheel_y=s.sy;out->wheel_sector=s.sector;
 if(s.sector>=0)out->flags|=KX_WHEEL_SELECT;
 return 1;
}
