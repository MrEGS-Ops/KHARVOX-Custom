#include "KharvoxModV1.h"
#include <cmath>
#include <new>
namespace {
struct Holsters { uint32_t slot[2]{};bool inside[2]{},grip[2]{},hidden=false;int stowed=-1; };
bool eligible(uint32_t id){return id>=KX_PISTOL&&id<=KX_SUPER_SHOTGUN;}
bool inHip(const KxHand& h,int side,bool prev){
 if(!h.tracked||!std::isfinite(h.x)||!std::isfinite(h.y)||!std::isfinite(h.z))return false;
 const float outward=side==KX_RIGHT?h.x:-h.x;
 return outward>=(prev?.07f:.12f)&&outward<=(prev?.60f:.51f)
 &&h.y>=(prev?-1.14f:-1.04f)&&h.y<=(prev?-.38f:-.48f)
 &&h.z>=(prev?-.05f:.02f)&&h.z<=(prev?.53f:.43f);
}
void fill(KxOutput& out,const Holsters& h){
 out.holster_right=int32_t(h.slot[0]);out.holster_left=int32_t(h.slot[1]);
}
void cue(KxOutput& out,int side){out.flags|=side==KX_RIGHT?KX_HAPTIC_RIGHT:KX_HAPTIC_LEFT;}
}
KX_EXPORT uint32_t kharvox_mod_abi_version(){return KX_ABI;}
KX_EXPORT const char* kharvox_mod_id(){return "dynamic-quick-holsters";}
KX_EXPORT void* kharvox_mod_create(){return new(std::nothrow) Holsters{};}
KX_EXPORT void kharvox_mod_destroy(void* p){delete static_cast<Holsters*>(p);}
KX_EXPORT int kharvox_mod_frame(void* p,const KxFrame* f,KxOutput* out){
 if(!p||!f||!out||f->size!=sizeof(KxFrame)||f->version!=KX_ABI
 ||out->size!=sizeof(KxOutput))return 0;
 auto& s=*static_cast<Holsters*>(p);
 *out={};out->size=sizeof(KxOutput);out->hand_side=-1;
 if(!f->gameplay||!f->tracking_valid){
   if(s.hidden)out->flags|=KX_WEAPON_SHOW;
   s.hidden=false;s.stowed=-1;s.inside[0]=s.inside[1]=false;
   s.grip[0]=s.grip[1]=false;fill(*out,s);return 1;
 }
 if(f->weapon_changed&&s.hidden){
   out->flags|=KX_WEAPON_SHOW;s.hidden=false;s.stowed=-1;
 }
 for(int side=0;side<2;++side){
   const bool now=inHip(f->hand[side],side,s.inside[side]);
   if(now&&!s.inside[side])cue(*out,side);
   s.inside[side]=now;
   const bool down=!!f->hand[side].grip;
   const bool edge=down&&!s.grip[side];s.grip[side]=down;
   if(!now||!edge)continue;
   if(s.slot[side]!=KX_UNKNOWN){
     // Occupied hip ALWAYS draws; never overwrite with currently held gun.
     out->flags|=KX_WEAPON_DRAW|KX_WEAPON_SHOW;
     out->hand_side=side;out->weapon_id=int32_t(s.slot[side]);
     s.slot[side]=KX_UNKNOWN;s.hidden=false;s.stowed=-1;break;
   }
   if(!s.hidden&&eligible(f->weapon_id)&&s.slot[1-side]!=f->weapon_id){
     s.slot[side]=f->weapon_id;s.stowed=side;s.hidden=true;
     out->flags|=KX_WEAPON_STOW|KX_WEAPON_HIDE;
     out->hand_side=side;out->weapon_id=int32_t(f->weapon_id);break;
   }
 }
 fill(*out,s);return 1;
}
