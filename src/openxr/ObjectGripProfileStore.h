#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <utility>

namespace kharvox {
// Auto profile identity must be a verified model/declaration path. Never
// substitute a transient native handle or a broad RTTI class (idProp2).
inline bool objectGripKeyValid(const std::string& key) noexcept {
    if(key.empty()||key.size()>192||key.front()=='/'
        ||key.find("..")!=std::string::npos)return false;
    for(char c:key)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')
        ||(c>='0'&&c<='9')||c=='/'||c=='_'||c=='-'||c=='.'||c==':'))
        return false;
    return true;
}
inline std::string objectGripAutomaticName(const std::string& key){
    if(!objectGripKeyValid(key))return {};
    const auto slash=key.find_last_of('/');
    auto text=key.substr(slash==std::string::npos?0:slash+1);
    const auto dot=text.find_last_of('.');
    if(dot!=std::string::npos)text.resize(dot);
    for(char& c:text)if(c=='_'||c=='-')c=' ';
    for(std::size_t i=0;i<text.size();++i)
        if((i==0||text[i-1]==' ')&&text[i]>='a'&&text[i]<='z')
            text[i]-='a'-'A';
    return text;
}
struct ObjectGripPose {
    bool configured{};
    std::array<float,3> offsetMetres{};
    std::array<float,4> localRotation{0.f,0.f,0.f,1.f};
};
struct ObjectGripProfile {std::array<ObjectGripPose,2> hands{};};
using ObjectGripProfiles=std::map<std::string,ObjectGripProfile>;
inline bool objectGripPoseValid(const ObjectGripPose& pose) noexcept {
    if(!pose.configured)return true;
    for(float value:pose.offsetMetres)
        if(!std::isfinite(value)||std::abs(value)>50.f)return false;
    float sum{};
    for(float value:pose.localRotation){
        if(!std::isfinite(value))return false;
        sum+=value*value;
    }
    return sum>.98f&&sum<1.02f;
}
inline bool objectGripConfigured(const ObjectGripProfiles& profiles,
    const std::string& verifiedKey,unsigned hand){
    if(hand>1||!objectGripKeyValid(verifiedKey))return false;
    const auto found=profiles.find(verifiedKey);
    return found!=profiles.end()&&found->second.hands[hand].configured
        &&objectGripPoseValid(found->second.hands[hand]);
}
// Loading an already-calibrated object is a normal gameplay pickup,
// NOT a request to open the editing overlay. Resolve the physical hand's
// saved pose from the verified model first; only a transient handle can
// select a session-only profile for an unnamed object.
inline const ObjectGripPose* objectGripSavedPose(
    const ObjectGripProfiles& byModel,
    const std::map<unsigned,ObjectGripProfile>& byInstance,
    const std::string& verifiedKey,unsigned handle,
    unsigned physicalHand) noexcept {
    if(physicalHand>1)return nullptr;
    if(objectGripKeyValid(verifiedKey)){
        const auto model=byModel.find(verifiedKey);
        if(model!=byModel.end()){
            const auto& pose=model->second.hands[physicalHand];
            if(pose.configured&&objectGripPoseValid(pose))return &pose;
        }
    }
    const auto session=byInstance.find(handle);
    if(session==byInstance.end())return nullptr;
    const auto& pose=session->second.hands[physicalHand];
    return pose.configured&&objectGripPoseValid(pose)?&pose:nullptr;
}

inline bool objectGripSave(const std::string& file,
    const ObjectGripProfiles& profiles){
    std::ostringstream output;
    output<<"object_grip_profiles_v1\n"<<std::fixed<<std::setprecision(6);
    for(const auto& pair:profiles){
        if(!objectGripKeyValid(pair.first))return false;
        output<<pair.first<<"\n";
        for(const auto& p:pair.second.hands){
            if(!objectGripPoseValid(p))return false;
            output<<(p.configured?1:0);
            for(float v:p.offsetMetres)output<<' '<<v;
            for(float v:p.localRotation)output<<' '<<v;
            output<<"\n";
        }
    }
    std::ofstream target(file,std::ios::trunc);
    if(!target)return false;
    target<<output.str();target.flush();return target.good();
}
inline bool objectGripLoad(const std::string& file,ObjectGripProfiles& profiles){
    std::ifstream input(file);
    if(!input)return false;
    std::string line;
    if(!std::getline(input,line)||line!="object_grip_profiles_v1")
        return false;
    ObjectGripProfiles parsed{};
    while(std::getline(input,line)){
        const std::string key=line;
        if(!objectGripKeyValid(key)||parsed.count(key)||parsed.size()>=512)
            return false;
        ObjectGripProfile profile{};
        for(auto& pose:profile.hands){
            if(!std::getline(input,line))return false;
            std::istringstream values(line);
            int configured{-1};
            if(!(values>>configured)||configured<0||configured>1)return false;
            pose.configured=configured==1;
            for(float& f:pose.offsetMetres)if(!(values>>f))return false;
            for(float& f:pose.localRotation)if(!(values>>f))return false;
            std::string excess;
            if(values>>excess||!objectGripPoseValid(pose))return false;
        }
        parsed.emplace(key,profile);
    }
    profiles=std::move(parsed);return true;
}

// Only verified model keys can identify a persistent calibration. The
// temporary native entity handle remains session-scoped and hand-specific.
enum class ObjectGripSaveResult {Invalid, Persistent, SessionOnly, DiskFailure};
inline ObjectGripSaveResult objectGripCommitPose(
    const std::string& filename,ObjectGripProfiles& byModel,
    std::map<unsigned,ObjectGripProfile>& byInstance,
    const std::string& model,unsigned handle,unsigned hand,
    const ObjectGripPose& pose){
    if(!handle||hand>1||!pose.configured||!objectGripPoseValid(pose))
        return ObjectGripSaveResult::Invalid;
    if(objectGripKeyValid(model)){
        auto pending=byModel;
        pending[model].hands[hand]=pose;
        if(!objectGripSave(filename,pending))
            return ObjectGripSaveResult::DiskFailure;
        byModel=std::move(pending);
        // Also resolve the just-saved native instance on the next pickup.
        byInstance[handle].hands[hand]=pose;
        return ObjectGripSaveResult::Persistent;
    }
    byInstance[handle].hands[hand]=pose;
    return ObjectGripSaveResult::SessionOnly;
}

inline std::array<float,9> objectGripQuaternionToAxis(
    const std::array<float,4>& q) {
    const float x=q[0],y=q[1],z=q[2],w=q[3];
    return {
        1.f-2.f*(y*y+z*z),2.f*(x*y+z*w),2.f*(x*z-y*w),
        2.f*(x*y-z*w),1.f-2.f*(x*x+z*z),2.f*(y*z+x*w),
        2.f*(x*z+y*w),2.f*(y*z-x*w),1.f-2.f*(x*x+y*y)
    };
}
inline std::array<float,4> objectGripAxisToQuaternion(
    const float m[9]) {
    // Native physics basis is column-major, with columns indexing by
    // col*3 + world coordinate.
    const float trace=m[0]+m[4]+m[8];
    float x{},y{},z{},w{};
    if(trace>0.f){
        const float t=std::sqrt(std::max(0.f,trace+1.f))*2.f;
        if(t<1e-5f)return {0.f,0.f,0.f,1.f};
        w=t*.25f;x=(m[5]-m[7])/t;
        y=(m[6]-m[2])/t;z=(m[1]-m[3])/t;
    }else if(m[0]>m[4]&&m[0]>m[8]){
        const float t=std::sqrt(std::max(0.f,1.f+m[0]-m[4]-m[8]))*2.f;
        if(t<1e-5f)return {0.f,0.f,0.f,1.f};
        w=(m[5]-m[7])/t;x=t*.25f;
        y=(m[3]+m[1])/t;z=(m[6]+m[2])/t;
    }else if(m[4]>m[8]){
        const float t=std::sqrt(std::max(0.f,1.f+m[4]-m[0]-m[8]))*2.f;
        if(t<1e-5f)return {0.f,0.f,0.f,1.f};
        w=(m[6]-m[2])/t;x=(m[3]+m[1])/t;
        y=t*.25f;z=(m[7]+m[5])/t;
    }else{
        const float t=std::sqrt(std::max(0.f,1.f+m[8]-m[0]-m[4]))*2.f;
        if(t<1e-5f)return {0.f,0.f,0.f,1.f};
        w=(m[1]-m[3])/t;x=(m[6]+m[2])/t;
        y=(m[7]+m[5])/t;z=t*.25f;
    }
    const float len=std::sqrt(x*x+y*y+z*z+w*w);
    if(len<1e-5f)return {0.f,0.f,0.f,1.f};
    return {x/len,y/len,z/len,w/len};
}
} // namespace kharvox
