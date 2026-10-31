#include "KharvoxModV1.h"
#ifdef NDEBUG
#undef NDEBUG // keep behavioural asserts active in CI Release tests
#endif
#include <cassert>
#include <string>
#ifdef _WIN32
#include <windows.h>
using Library=HMODULE;
static Library openLib(const char* s){return LoadLibraryA(s);}
static void* symbol(Library l,const char* s){return reinterpret_cast<void*>(GetProcAddress(l,s));}
static void closeLib(Library l){FreeLibrary(l);}
#else
#include <dlfcn.h>
using Library=void*;
static Library openLib(const char* s){return dlopen(s,RTLD_NOW|RTLD_LOCAL);}
static void* symbol(Library l,const char* s){return dlsym(l,s);}
static void closeLib(Library l){dlclose(l);}
#endif
struct Mod {
 Library dll{};
 void* instance{};
 uint32_t (*abi)(){};
 const char* (*id)(){};
 void* (*create)(){};
 void (*destroy)(void*){};
 int (*frame)(void*,const KxFrame*,KxOutput*){};
 explicit Mod(const char* path){
  dll=openLib(path);assert(dll);
  abi=reinterpret_cast<decltype(abi)>(symbol(dll,"kharvox_mod_abi_version"));
  id=reinterpret_cast<decltype(id)>(symbol(dll,"kharvox_mod_id"));
  create=reinterpret_cast<decltype(create)>(symbol(dll,"kharvox_mod_create"));
  destroy=reinterpret_cast<decltype(destroy)>(symbol(dll,"kharvox_mod_destroy"));
  frame=reinterpret_cast<decltype(frame)>(symbol(dll,"kharvox_mod_frame"));
  assert(abi&&id&&create&&destroy&&frame&&abi()==KX_ABI);
  instance=create();assert(instance);
 }
 ~Mod(){destroy(instance);closeLib(dll);}
 KxOutput tick(const KxFrame& f){
  KxOutput o{};o.size=sizeof(o);assert(frame(instance,&f,&o)==1);return o;
 }
};
static bool has(KxOutput o,uint32_t flag){return (o.flags&flag)!=0;}
static KxFrame fresh(){
 KxFrame f{};f.size=sizeof(f);f.version=KX_ABI;f.gameplay=1;
 f.tracking_valid=1;f.wheel_supported=1;f.weapon_id=KX_PISTOL;
 f.hand[0].tracked=1;f.hand[1].tracked=1;return f;
}
int main(int argc,char** argv){
 assert(argc==3);
 Mod wheel(argv[1]),holster(argv[2]);
 assert(std::string(wheel.id())=="behind-head-weapon-wheel");
 assert(std::string(holster.id())=="dynamic-quick-holsters");
 KxFrame f=fresh();auto w=wheel.tick(f);assert(!w.flags);
 f.hand[0].x=.25f;f.hand[0].z=.25f;
 w=wheel.tick(f);assert(has(w,KX_WHEEL_OPEN)&&has(w,KX_WHEEL_HOLD));
 f.hand[0].x=.34f;f.hand[0].y=.07f;
 w=wheel.tick(f);assert(has(w,KX_WHEEL_SELECT)&&w.wheel_sector>=0);
 float selectedX=w.wheel_x;
 f.hand[0].grip=1;
 w=wheel.tick(f);assert(has(w,KX_WHEEL_HOLD)&&w.wheel_x==selectedX);
 f.hand[0].x=1.f;
 w=wheel.tick(f);assert(has(w,KX_WHEEL_CONFIRM)&&!has(w,KX_WHEEL_CANCEL));
 f.hand[0].grip=0;wheel.tick(f);
 f.hand[0].x=.25f;w=wheel.tick(f);assert(has(w,KX_WHEEL_OPEN));
 f.hand[0].x=1.f;w=wheel.tick(f);
 assert(has(w,KX_WHEEL_CANCEL)&&has(w,KX_WHEEL_NEUTRAL));
 f.wheel_blocked=1;f.hand[0].x=.25f;
 w=wheel.tick(f);assert(!has(w,KX_WHEEL_OPEN));
 f=fresh();f.wheel_supported=0;
 f.hand[0].x=.26f;f.hand[0].y=-.75f;f.hand[0].z=.2f;
 auto h=holster.tick(f);assert(has(h,KX_HAPTIC_RIGHT));
 f.hand[0].grip=1;h=holster.tick(f);
 assert(has(h,KX_WEAPON_STOW)&&has(h,KX_WEAPON_HIDE));
 assert(h.holster_right==KX_PISTOL);
 h=holster.tick(f);assert(!has(h,KX_WEAPON_DRAW));
 f.hand[0].grip=0;holster.tick(f);
 f.weapon_id=KX_SUPER_SHOTGUN;f.hand[0].grip=1;
 h=holster.tick(f);assert(has(h,KX_WEAPON_DRAW)&&h.weapon_id==KX_PISTOL);
 assert(h.holster_right==KX_UNKNOWN);
 f.hand[0].grip=0;holster.tick(f);
 f.weapon_id=KX_BFG;f.hand[0].grip=1;
 h=holster.tick(f);assert(!has(h,KX_WEAPON_STOW));
 f.gameplay=0;h=holster.tick(f);
 assert(!has(h,KX_WEAPON_STOW)&&!has(h,KX_WEAPON_DRAW));
 f.size=0;KxOutput bad{};bad.size=sizeof(bad);
 assert(wheel.frame(wheel.instance,&f,&bad)==0);
 return 0;
}
