#pragma once
#include <array>
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
        if(!std::isfinite(value)||std::abs(value)>2.f)return false;
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
} // namespace kharvox
