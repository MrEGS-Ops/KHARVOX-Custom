#pragma once
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
namespace kharvox {
// Weapon-relative offset in metres (+X right, +Y up, -Z forward).
struct RightHandGripOffset {float x{},y{},z{};};
struct RightHandGripSettings {
    RightHandGripOffset gauss{},chaingun{};
    int accepted{},rejected{};
};
inline RightHandGripSettings readRightHandGripSettings(const std::string&path){
    RightHandGripSettings out{};std::ifstream f(path);std::string line;
    while(std::getline(f,line)){
        const auto start=line.find_first_not_of(" \t\r");
        if(start==std::string::npos||line[start]=='#')continue;
        const auto split=line.find('=',start);
        if(split==std::string::npos){++out.rejected;continue;}
        auto key=line.substr(start,split-start);
        const auto end=key.find_last_not_of(" \t");
        if(end==std::string::npos){++out.rejected;continue;}
        key.erase(end+1);
        std::istringstream v(line.substr(split+1));
        RightHandGripOffset p{};std::string extra;
        if(!(v>>p.x>>p.y>>p.z)||(v>>extra)||
            !std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||
            std::abs(p.x)>0.30f||std::abs(p.y)>0.30f||std::abs(p.z)>0.30f){
            ++out.rejected;continue;
        }
        if(key=="gauss_cannon"){out.gauss=p;++out.accepted;}
        else if(key=="chaingun"){out.chaingun=p;++out.accepted;}
        else ++out.rejected;
    }
    return out;
}
}
