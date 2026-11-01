#pragma once
#include <cmath>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <utility>
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
struct RightHandGripRotation {
    float x{},y{},z{},w{1.f};
    bool valid{};
};
inline bool validRightHandRotation(RightHandGripRotation q) noexcept {
    const float square=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    return std::isfinite(square)&&square>.81f&&square<1.21f;
}

// Gauss / Chaingun are physically large. 30 cm per-axis rejected verified
// ~55 cm and ~74 cm owner captures despite valid tracking. Limit the full
// Euclidean distance to 85 cm, including on reload.
inline constexpr float kMaximumRightHandPlacementMeters=0.85f;
inline bool validRightHandPlacement(RightHandGripOffset p) noexcept {
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)
        &&std::hypot(p.x,std::hypot(p.y,p.z))
            <=kMaximumRightHandPlacementMeters;
}
// The observed point of the RIGHT glove is relative to the shifted
// WEAPON anchor, not the controller. It can be roughly twice as far away
// as the original physical gun-anchor calibration, particularly for the
// large Chaingun. Do not apply the 85 cm native-anchor limit to it.
inline constexpr float kMaximumVisualGripDistanceMeters=1.75f;
inline bool validRightHandVisualPlacement(RightHandGripOffset p) noexcept {
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)
        &&std::hypot(p.x,std::hypot(p.y,p.z))
            <=kMaximumVisualGripDistanceMeters;
}
struct RightHandGripSettings {
    RightHandGripOffset gauss{},chaingun{};
    RightHandGripRotation gaussRotation{},chaingunRotation{};
    // Separate glove-from-weapon grip poses. The older gauss/chaingun
    // coordinates remain the VERIFIED gun-anchor offsets and are never
    // overwritten by live hand-model calibration.
    RightHandGripOffset gaussHand{},chaingunHand{},
        gaussSiegeHand{},chaingunTurretHand{};
    bool gaussHandValid{},chaingunHandValid{},
        gaussSiegeHandValid{},chaingunTurretHandValid{};
    RightHandGripRotation gaussSiegeRotation{},chaingunTurretRotation{};
    RightHandGripOffset gaussSiegeSupport{},chaingunTurretSupport{};
    bool gaussSiegeSupportValid{},chaingunTurretSupportValid{};
    // Move the rendered PRIMARY gun hand DOWN along the gun's own
    // up axis only while two-hand Grip is latched. This compensates for
    // the controller-origin anchor used by the earlier implementation.
    // Missing property in an existing local config has the same default.
    float rightHandDropMeters{0.08f};
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
        if(key=="gauss_cannon_rotation"||key=="chaingun_rotation"||
            key=="gauss_cannon_siege_rotation"||
            key=="chaingun_turret_rotation"){
            RightHandGripRotation q{};
            if(!(v>>q.x>>q.y>>q.z>>q.w)||(v>>extra)
                ||!validRightHandRotation(q)){++out.rejected;continue;}
            const float inv=1.f/std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
            q.x*=inv;q.y*=inv;q.z*=inv;q.w*=inv;q.valid=true;
            if(key=="gauss_cannon_rotation")out.gaussRotation=q;
            else if(key=="chaingun_rotation")out.chaingunRotation=q;
            else if(key=="gauss_cannon_siege_rotation")out.gaussSiegeRotation=q;
            else out.chaingunTurretRotation=q;
            ++out.accepted;
            continue;
        }
        if(key=="right_hand_drop_m"){
            // Separate global height setting from three-axis weapon offsets.
            // The unused extra xyz fields are not valid for this key.
            float drop{};
            std::istringstream v1(line.substr(split+1));
            if(!(v1>>drop)||(v1>>extra)||!std::isfinite(drop)
                ||drop<0.f||drop>0.20f){++out.rejected;continue;}
            out.rightHandDropMeters=drop;
            ++out.accepted;
            continue;
        }
        const bool extendedGrip=
            key=="gauss_cannon_hand_position"
            ||key=="chaingun_hand_position"
            ||key=="gauss_cannon_siege_hand_position"
            ||key=="chaingun_turret_hand_position"
            ||key=="gauss_cannon_siege_support_position"
            ||key=="chaingun_turret_support_position";
        if(!(v>>p.x>>p.y>>p.z)||(v>>extra)||
            !(extendedGrip?validRightHandVisualPlacement(p)
                         :validRightHandPlacement(p))){
            ++out.rejected;continue;
        }
        if(key=="gauss_cannon"){out.gauss=p;++out.accepted;}
        else if(key=="chaingun"){out.chaingun=p;++out.accepted;}
        else if(key=="gauss_cannon_hand_position"){
            out.gaussHand=p;out.gaussHandValid=true;++out.accepted;
        }else if(key=="chaingun_hand_position"){
            out.chaingunHand=p;out.chaingunHandValid=true;++out.accepted;
        }else if(key=="gauss_cannon_siege_hand_position"){
            out.gaussSiegeHand=p;out.gaussSiegeHandValid=true;++out.accepted;
        }else if(key=="chaingun_turret_hand_position"){
            out.chaingunTurretHand=p;out.chaingunTurretHandValid=true;++out.accepted;
        }else if(key=="gauss_cannon_siege_support_position"){
            out.gaussSiegeSupport=p;out.gaussSiegeSupportValid=true;++out.accepted;
        }else if(key=="chaingun_turret_support_position"){
            out.chaingunTurretSupport=p;out.chaingunTurretSupportValid=true;++out.accepted;
        }else ++out.rejected;
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
    if(!validRightHandPlacement(offset))
        return reject("placement is invalid or exceeds 85 cm total distance; reposition left hand");
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
// A left-controller orientation probe is expressed in WEAPON local space.
// Save with the same atomic/backup discipline as the owner's position captures.
inline bool saveRightHandGripRotation(const std::string&path,
    const std::string&weapon,RightHandGripRotation q,std::string*why=nullptr){
    auto fail=[&](const char*msg){if(why)*why=msg;return false;};
    if(weapon!="gauss_cannon"&&weapon!="chaingun")
        return fail("rotation calibration supports only Gauss/Chaingun");
    if(!validRightHandRotation(q))return fail("invalid hand rotation");
    const float inv=1.f/std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    q.x*=inv;q.y*=inv;q.z*=inv;q.w*=inv;
    const std::string key=weapon+"_rotation";
    namespace fs=std::filesystem;
    std::error_code ec;
    const fs::path dest(path),tmp(path+".rotation.tmp");
    if(fs::exists(dest,ec)&&fs::file_size(dest,ec)>16384)
        return fail("right-hand calibration file too large");
    std::vector<std::string> lines;
    {std::ifstream in(path);std::string row;while(std::getline(in,row))lines.push_back(row);
     if(in.bad())return fail("could not read right-hand settings");}
    std::ostringstream entry;entry<<key<<" = "<<std::fixed<<std::setprecision(8)
        <<q.x<<' '<<q.y<<' '<<q.z<<' '<<q.w;
    bool updated=false;
    for(auto&row:lines){
        const auto eq=row.find('=');
        if(eq==std::string::npos)continue;
        const auto first=row.find_first_not_of(" \t");
        if(first==std::string::npos||row[first]=='#')continue;
        auto name=row.substr(first,eq-first);
        const auto last=name.find_last_not_of(" \t");
        if(last!=std::string::npos)name.erase(last+1);
        if(name==key){row=entry.str();updated=true;}
    }
    if(!updated)lines.push_back(entry.str());
    if(fs::exists(dest,ec)){
        fs::copy_file(dest,path+".before-capture.bak",
            fs::copy_options::overwrite_existing,ec);
        if(ec)return fail("could not back up previous right-hand rotation");
    }
    {std::ofstream out(tmp,std::ios::trunc);if(!out)return fail("could not open rotation temp file");
     for(const auto&row:lines)out<<row<<'\n';
     out.flush();if(!out)return fail("could not save hand rotation");}
#ifdef _WIN32
    if(!MoveFileExW(tmp.wstring().c_str(),dest.wstring().c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        fs::remove(tmp,ec);return fail("could not replace saved rotation");}
#else
    fs::rename(tmp,dest,ec);
    if(ec){fs::remove(tmp);return fail("could not replace saved rotation");}
#endif
    return true;
}

// Atomic single-transaction update of one visual grip pose: both the
// glove POSITION and ROTATION are saved together, or neither is saved.
// Preserves the native weapon anchor values and all other weapon modes.
inline bool saveRightHandVisualPose(const std::string&path,
    const std::string&weapon,bool alternate,
    RightHandGripOffset pos,RightHandGripRotation q,std::string*why=nullptr){
    auto fail=[&](const char*msg){if(why)*why=msg;return false;};
    if((weapon!="gauss_cannon"&&weapon!="chaingun")||
       !validRightHandVisualPlacement(pos)||!validRightHandRotation(q))
        return fail("invalid measured position or orientation");
    const std::string prefix=weapon+
        (alternate?(weapon=="gauss_cannon"?"_siege":"_turret"):"");
    const std::string positionKey=prefix+"_hand_position";
    const std::string rotationKey=prefix+"_rotation";
    const float inv=1.f/std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    q.x*=inv;q.y*=inv;q.z*=inv;q.w*=inv;
    std::ostringstream position,rotation;
    position<<positionKey<<" = "<<std::fixed<<std::setprecision(8)
        <<pos.x<<' '<<pos.y<<' '<<pos.z;
    rotation<<rotationKey<<" = "<<std::fixed<<std::setprecision(8)
        <<q.x<<' '<<q.y<<' '<<q.z<<' '<<q.w;
    namespace fs=std::filesystem;
    const fs::path destination(path),temp(path+".live.tmp");
    std::error_code ec;
    if(fs::exists(destination,ec)&&fs::file_size(destination,ec)>16384)
        return fail("right-hand config exceeds safe file size");
    std::vector<std::string> lines;
    {std::ifstream input(path);std::string line;
     while(std::getline(input,line))lines.push_back(line);
     if(input.bad())return fail("failed reading existing settings");}
    bool posFound=false,rotFound=false;
    for(auto it=lines.begin();it!=lines.end();){
        auto eq=it->find('=');
        if(eq==std::string::npos){++it;continue;}
        const auto first=it->find_first_not_of(" \t");
        if(first==std::string::npos||(*it)[first]=='#'){++it;continue;}
        auto key=it->substr(first,eq-first);
        const auto end=key.find_last_not_of(" \t");
        if(end!=std::string::npos)key.erase(end+1);
        if(key==positionKey){
            if(posFound){it=lines.erase(it);continue;}
            *it=position.str();posFound=true;
        }else if(key==rotationKey){
            if(rotFound){it=lines.erase(it);continue;}
            *it=rotation.str();rotFound=true;
        }
        ++it;
    }
    if(!posFound)lines.push_back(position.str());
    if(!rotFound)lines.push_back(rotation.str());
    if(fs::exists(destination,ec)){
        fs::copy_file(destination,path+".before-live-capture.bak",
            fs::copy_options::overwrite_existing,ec);
        if(ec)return fail("unable to back up live calibration");
    }
    {std::ofstream output(temp,std::ios::trunc);
     if(!output)return fail("unable to create temporary calibration");
     for(const auto&line:lines)output<<line<<'\n';
     output.flush();if(!output)return fail("unable to flush both grip fields");}
#ifdef _WIN32
    if(!MoveFileExW(temp.wstring().c_str(),destination.wstring().c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        fs::remove(temp,ec);return fail("unable to replace saved live pose");}
#else
    fs::rename(temp,destination,ec);
    if(ec){fs::remove(temp);return fail("unable to replace saved live pose");}
#endif
    return true;
}
inline bool saveHeavyAlternativeSupport(const std::string&path,
    const std::string&weapon,RightHandGripOffset pos,std::string*why=nullptr){
    auto fail=[&](const char*msg){if(why)*why=msg;return false;};
    if((weapon!="gauss_cannon"&&weapon!="chaingun")||
       !validRightHandVisualPlacement(pos))
        return fail("unsupported heavy weapon support point");
    const std::string key=weapon+
        (weapon=="gauss_cannon"?"_siege":"_turret")+"_support_position";
    namespace fs=std::filesystem;
    const fs::path destination(path),temp(path+".alt-support.tmp");
    std::error_code ec;
    if(fs::exists(destination,ec)&&fs::file_size(destination,ec)>16384)
        return fail("settings file exceeds safe size");
    std::vector<std::string> lines;
    {std::ifstream input(path);std::string row;
     while(std::getline(input,row))lines.push_back(row);
     if(input.bad())return fail("cannot read existing settings");}
    std::ostringstream entry;
    entry<<key<<" = "<<std::fixed<<std::setprecision(8)
        <<pos.x<<' '<<pos.y<<' '<<pos.z;
    bool found=false;
    for(auto it=lines.begin();it!=lines.end();){
        const auto eq=it->find('=');
        if(eq==std::string::npos){++it;continue;}
        auto start=it->find_first_not_of(" \t");
        if(start==std::string::npos||(*it)[start]=='#'){++it;continue;}
        auto name=it->substr(start,eq-start);
        const auto end=name.find_last_not_of(" \t");
        if(end!=std::string::npos)name.erase(end+1);
        if(name==key){
            if(found){it=lines.erase(it);continue;}
            *it=entry.str();found=true;
        }
        ++it;
    }
    if(!found)lines.push_back(entry.str());
    if(fs::exists(destination,ec)){
        fs::copy_file(destination,path+".before-live-capture.bak",
            fs::copy_options::overwrite_existing,ec);
        if(ec)return fail("cannot back up existing mode profile");
    }
    {std::ofstream output(temp,std::ios::trunc);
     if(!output)return fail("cannot create temporary support profile");
     for(const auto&line:lines)output<<line<<'\n';
     output.flush();if(!output)return fail("cannot save alternate support");}
#ifdef _WIN32
    if(!MoveFileExW(temp.wstring().c_str(),destination.wstring().c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        fs::remove(temp,ec);return fail("could not replace alternate support");}
#else
    fs::rename(temp,destination,ec);
    if(ec){fs::remove(temp);return fail("could not replace alternate support");}
#endif
    return true;
}
// The two heavy-weapon support points live relative to the FINAL weapon
// anchor, not the older rifle/controller aim origin.
inline bool saveHeavySupportGrip(const std::string&path,
    const std::string&weapon,RightHandGripOffset pos,
    bool sideGrip,float radius,float weight,std::string*why=nullptr){
    auto fail=[&](const char*msg){if(why)*why=msg;return false;};
    if((weapon!="gauss_cannon"&&weapon!="chaingun")
        ||!validRightHandPlacement(pos)||!std::isfinite(radius)
        ||!std::isfinite(weight)||radius<.08f||radius>.40f
        ||weight<0.f||weight>1.f)
        return fail("invalid heavy weapon support calibration");
    namespace fs=std::filesystem;
    std::error_code ec;fs::path dest(path),tmp(path+".support.tmp");
    if(fs::exists(dest,ec)&&fs::file_size(dest,ec)>65536)
        return fail("two-hand calibration file too large");
    std::vector<std::string> rows;
    {std::ifstream in(path);std::string line;
     while(std::getline(in,line))rows.push_back(line);
     if(in.bad())return fail("cannot read two-hand calibration");}
    std::ostringstream record;
    record<<"2 "<<weapon<<' '<<(sideGrip?"side-grip":"barrel")<<' '
        <<std::fixed<<std::setprecision(7)<<pos.x<<' '<<pos.y<<' '
        <<pos.z<<' '<<radius<<' '<<weight;
    bool updated=false;
    for(auto it=rows.begin();it!=rows.end();){
        std::istringstream in(*it);std::string ver,key;
        if((in>>ver>>key)&&ver=="2"&&key==weapon){
            if(!updated){*it=record.str();updated=true;++it;}
            else it=rows.erase(it);
        }else ++it;
    }
    if(!updated)rows.push_back(record.str());
    if(fs::exists(dest,ec)){
        fs::copy_file(dest,path+".before-heavy-support.bak",
            fs::copy_options::overwrite_existing,ec);
        if(ec)return fail("unable to back up previous support calibration");
    }
    {std::ofstream out(tmp,std::ios::trunc);
     if(!out)return fail("unable to create support calibration temp file");
     for(const auto&line:rows)out<<line<<'\n';
     out.flush();if(!out)return fail("unable to save support calibration");}
#ifdef _WIN32
    if(!MoveFileExW(tmp.wstring().c_str(),dest.wstring().c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        fs::remove(tmp,ec);return fail("unable to install support calibration");}
#else
    fs::rename(tmp,dest,ec);
    if(ec){fs::remove(tmp);return fail("unable to install support calibration");}
#endif
    return true;
}
inline bool saveChainsawSupportGrip(const std::string&path,
    RightHandGripOffset pos,float radius,float weight,std::string*why=nullptr){
    auto fail=[&](const char*msg){if(why)*why=msg;return false;};
    if(!std::isfinite(pos.x)||!std::isfinite(pos.y)||!std::isfinite(pos.z)||
        !std::isfinite(radius)||!std::isfinite(weight)||
        std::abs(pos.x)>0.75f||std::abs(pos.y)>0.75f||
        std::abs(pos.z)>0.75f||radius<0.08f||radius>0.40f||
        weight<0.f||weight>1.f)return fail("invalid chainsaw grip measurement");
    namespace fs=std::filesystem;
    std::error_code ec;
    fs::path dest(path),temp(path+".chainsaw.tmp");
    if(fs::exists(dest,ec)&&fs::file_size(dest,ec)>65536)
        return fail("saved profile file too large");
    std::vector<std::string> original;
    {std::ifstream in(path);std::string row;
     while(std::getline(in,row))original.push_back(row);
     if(in.bad())return fail("unable to read saved profile");}
    std::ostringstream value;
    value<<"2 chainsaw side-grip "<<std::fixed<<std::setprecision(7)
        <<pos.x<<' '<<pos.y<<' '<<pos.z<<' '<<radius<<' '<<weight;
    std::vector<std::string> outLines;
    bool wrote=false;
    for(const auto&row:original){
        std::istringstream parse(row);std::string version,key;
        if((parse>>version>>key)&&version=="2"&&key=="chainsaw"){
            if(!wrote){outLines.push_back(value.str());wrote=true;}
        }else outLines.push_back(row);
    }
    if(!wrote)outLines.push_back(value.str());
    if(fs::exists(dest,ec)){
        const fs::path backup(path+".before-chainsaw-capture.bak");
        if(!fs::exists(backup,ec)){
            fs::copy_file(dest,backup,fs::copy_options::none,ec);
            if(ec)return fail("failed to back up original saved profiles");
        }
    }
    {std::ofstream out(temp,std::ios::trunc);
     if(!out)return fail("unable to create temporary profile");
     for(const auto&row:outLines)out<<row<<'\n';
     out.flush();if(!out)return fail("could not save chainsaw support");}
#ifdef _WIN32
    if(!MoveFileExW(temp.wstring().c_str(),dest.wstring().c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        fs::remove(temp,ec);return fail("could not replace saved chainsaw support");}
#else
    fs::rename(temp,dest,ec);
    if(ec){fs::remove(temp);return fail("could not replace saved chainsaw support");}
#endif
    return true;
}

}
