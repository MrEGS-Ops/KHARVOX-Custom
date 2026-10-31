#pragma once
#include <stdint.h>

// Standalone versioned DLL mod ABI. Host supplies *HMD local* controller
// positions, never pointers into KHARVOX or DOOM engine objects.
#ifdef _WIN32
#define KX_EXPORT extern "C" __declspec(dllexport)
#else
#define KX_EXPORT extern "C" __attribute__((visibility("default")))
#endif
#define KX_ABI 1u
enum KxSide { KX_RIGHT=0, KX_LEFT=1 };
enum KxWeapon { KX_UNKNOWN=0, KX_PISTOL=1, KX_COMBAT_SHOTGUN=2,
 KX_HEAVY_ASSAULT_RIFLE=3, KX_PLASMA_RIFLE=4, KX_ROCKET_LAUNCHER=5,
 KX_SUPER_SHOTGUN=6, KX_GAUSS=7, KX_CHAINGUN=8, KX_BFG=9,
 KX_CHAINSAW=10, KX_FISTS=11 };
typedef struct KxHand { float x,y,z; uint32_t tracked,grip; } KxHand;
typedef struct KxFrame {
 uint32_t size,version,gameplay,tracking_valid;
 KxHand hand[2]; // physical right, left; head-local +Z BACKWARDS
 uint32_t weapon_id,weapon_changed,wheel_supported,wheel_blocked;
} KxFrame;
enum KxFlags { KX_NONE=0, KX_WHEEL_OPEN=1<<0, KX_WHEEL_HOLD=1<<1,
 KX_WHEEL_CONFIRM=1<<2, KX_WHEEL_CANCEL=1<<3,
 KX_HAPTIC_RIGHT=1<<4, KX_HAPTIC_LEFT=1<<5,
 KX_WEAPON_STOW=1<<6, KX_WEAPON_DRAW=1<<7,
 KX_WEAPON_SHOW=1<<8, KX_WEAPON_HIDE=1<<9,
 KX_WHEEL_SELECT=1<<10, KX_WHEEL_NEUTRAL=1<<11 };
typedef struct KxOutput {
 uint32_t size, flags;
 int32_t hand_side, weapon_id;
 float wheel_x,wheel_y;
 int32_t holster_right,holster_left,wheel_sector;
} KxOutput;
// Each independent mod DLL exports these identical C symbols.
KX_EXPORT uint32_t kharvox_mod_abi_version(void);
KX_EXPORT const char* kharvox_mod_id(void);
KX_EXPORT void* kharvox_mod_create(void);
KX_EXPORT void kharvox_mod_destroy(void*);
KX_EXPORT int kharvox_mod_frame(void*,const KxFrame*,KxOutput*);
