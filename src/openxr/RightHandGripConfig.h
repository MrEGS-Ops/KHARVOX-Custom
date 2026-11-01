#pragma once
#include <cmath>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <iomanip>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
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
// Replace only one permitted right-hand visual profile, preserving the
// other weapon and all comments. Save an adjacent backup before replacing.
inline bool saveRightHandGripMeasurement(
    const std::string& path,const std::string& key,
    const RightHandGripOffset& offset,std::string* why=nullptr){
    auto reject=[&](const char*reason){
        if(why)*why=reason;
        return false;
    };
    if(key!="gauss_cannon"&&key!="chaingun")
        return reject("unsupported weapon");
    if(!std::isfinite(offset.x)||!std::isfinite(offset.y)||
        !std::isfinite(offset.z)||std::abs(offset.x)>0.30f||
        std::abs(offset.y)>0.30f||std::abs(offset.z)>0.30f)
        return reject("placement exceeds +/-30 cm; reposition left hand");
    namespace fs=std::filesystem;
    std::error_code ec;
    if(fs::exists(path,ec)&&fs::file_size(path,ec)>16384)
        return reject("existing grip config exceeds size limit");
    std::vector<std::string> lines;
    {
        std::ifstream input(path);
        std::string line;
        while(std::getline(input,line))lines.push_back(line);
        if(input.bad())return reject("could not read existing grip config");
    }
    std::ostringstream record;
    record<<key<<" = "<<std::fixed<<std::setprecision(5)
        <<offset.x<<' '<<offset.y<<' '<<offset.z;
    bool found=false;
    for(auto&line:lines){
        const auto start=line.find_first_not_of(" \t");
        if(start==std::string::npos||line[start]=='#')continue;
        const auto eq=line.find('=',start);
        if(eq==std::string::npos)continue;
        auto k=line.substr(start,eq-start);
        const auto end=k.find_last_not_of(" \t");
        if(end!=std::string::npos)k.erase(end+1);
        if(k==key){line=record.str();found=true;}
    }
    if(!found)lines.push_back(record.str());
    const fs::path original(path),temp(path+".capture.tmp");
    if(fs::exists(original,ec)){
        fs::copy_file(original,path+".before-capture.bak",
            fs::copy_options::overwrite_existing,ec);
        if(ec)return reject("cannot back up current grip positions");
    }
    {
        std::ofstream output(temp,std::ios::trunc);
        if(!output)return reject("cannot create temporary grip config");
        for(const auto&line:lines)output<<line<<'\n';
        output.flush();
        if(!output)return reject("cannot write temporary grip config");
    }
#ifdef _WIN32
    if(!MoveFileExW(temp.wstring().c_str(),original.wstring().c_str(),
            MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        fs::remove(temp,ec);
        return reject("cannot replace grip config");
    }
#else
    fs::rename(temp,original,ec);
    if(ec){fs::remove(temp);return reject("cannot replace grip config");}
#endif
    return true;
}
}
