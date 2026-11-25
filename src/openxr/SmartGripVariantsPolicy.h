#pragma once
// RIPSHIFT Smart Grip: independent, OFF-by-default experimental pickup poses.
// The mesh inspector is diagnostic until bounds/shape fields are validated.
#include "../camera/ObjectGripRotationPolicy.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>
namespace kharvox {
inline constexpr unsigned kSmartGripPalm=1u;
inline constexpr unsigned kSmartGripKeepTilt=2u;
inline constexpr unsigned kSmartGripWorldUpright=4u;
inline constexpr unsigned kSmartGripPickupOffset=8u;
struct SmartGripSettings {
    bool enabled{};
    bool palmAnchor{};
    bool keepTilt{};
    bool worldUpright{};
    bool pickupOffset{};
    bool inspectMesh{};
};
inline unsigned smartGripFlags(SmartGripSettings s) noexcept {
    return (s.palmAnchor?kSmartGripPalm:0u)
        |(s.keepTilt?kSmartGripKeepTilt:0u)
        |(s.worldUpright?kSmartGripWorldUpright:0u)
        |(s.pickupOffset?kSmartGripPickupOffset:0u);
}
inline bool smartGripSave(const std::string& path,SmartGripSettings s){
    std::ofstream f(path,std::ios::trunc);
    if(!f)return false;
    f<<"smart_grip_experiments_v1\n"
     <<"enabled="<<s.enabled<<"\n"
     <<"palm_anchor="<<s.palmAnchor<<"\n"
     <<"keep_tilt="<<s.keepTilt<<"\n"
     <<"world_upright="<<s.worldUpright<<"\n"
     <<"pickup_offset="<<s.pickupOffset<<"\n"
     <<"inspect_mesh="<<s.inspectMesh<<"\n";
    f.flush();return f.good();
}
inline bool smartGripLoad(const std::string& path,SmartGripSettings& s){
    std::ifstream f(path);if(!f)return false;
    std::string line;
    if(!std::getline(f,line)||line!="smart_grip_experiments_v1")return false;
    auto readBool=[&](const char* key,bool& out){
        if(!std::getline(f,line))return false;
        const std::string prefix=std::string(key)+"=";
        if(line==prefix+"0"){out=false;return true;}
        if(line==prefix+"1"){out=true;return true;}
        return false;
    };
    SmartGripSettings value{};
    if(!readBool("enabled",value.enabled)
        ||!readBool("palm_anchor",value.palmAnchor)
        ||!readBool("keep_tilt",value.keepTilt)
        ||!readBool("world_upright",value.worldUpright)
        ||!readBool("pickup_offset",value.pickupOffset)
        ||!readBool("inspect_mesh",value.inspectMesh)
        ||std::getline(f,line))return false;
    s=value;return true;
}
// Variations can be combined. Pickup Offset overrides Palm Anchor;
// World Upright overrides Keep Tilt. No guessed mesh bounds used.
inline bool smartGripPickupPose(unsigned flags,
    const float handOrigin[3],const float handAxis[9],
    const float bodyOrigin[3],const float bodyAxis[9],
    std::array<float,3>& offset,
    std::array<float,9>& axis) noexcept {
    offset=kObjectGripNeutralOffsetUnits;
    axis=objectGripNeutralLocalAxis();
    if(!handOrigin||!handAxis||!bodyOrigin||!bodyAxis)return false;
    if(flags&kSmartGripPalm)offset={0.f,-2.f,-12.f};
    if(flags&kSmartGripPickupOffset){
        float originalOffset[3]{},originalAxis[9]{};
        objectGripToHandLocalPose(handOrigin,handAxis,
            bodyOrigin,bodyAxis,originalOffset,originalAxis);
        for(int i=0;i<3;++i){
            if(!std::isfinite(originalOffset[i]))return false;
            offset[i]=std::clamp(originalOffset[i],-75.f,75.f);
        }
    }
    if(flags&kSmartGripWorldUpright){
        constexpr float up[9]{1,0,0,0,1,0,0,0,1};
        axis=objectGripToHandLocalAxis(handAxis,up);
    }else if(flags&kSmartGripKeepTilt){
        axis=objectGripToHandLocalAxis(handAxis,bodyAxis);
    }
    for(float v:axis)if(!std::isfinite(v))return false;
    return true;
}
} // namespace kharvox
