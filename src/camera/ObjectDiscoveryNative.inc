// Experimental DOOM-thread object cache and hand-relative pin.
// FocusTracker only seeds candidates; cached candidates are refreshed in the
// normal native world tick. Scanner and grip never wake stationary bodies.
struct ObjectDiscoveryNativeRecord {
    const void* entity{};
    void* body{};
    std::uintptr_t entityVtable{},bodyVtable{},imageBase{},imageSize{};
    unsigned handle{};
    unsigned long long generation{},observedAtMs{},lastProbeAtMs{},
        lastPositionAtMs{},nativeMovedAtMs{};
    float origin[3]{},axis[9]{};
    bool poseValid{},nativeMotionObserved{},modelChecked{};
    std::array<char,160> modelKey{};
};
struct ObjectDiscoveryGripFrame {
    float origin[3]{},axis[9]{},aim[3]{};
    float reachUnits{150.f};
    unsigned physicalHand{};
    unsigned long long frameAtMs{},releaseSerial{};
    bool enabled{},pressed{},calibrating{},freeze{},offsetValid{},axisValid{};
    float wantedOffset[3]{},wantedAxis[9]{};
};
struct ObjectDiscoveryHeld {
    unsigned handle{},physicalHand{};
    unsigned long long generation{},capturedAtMs{};
    float relativeOffset[3]{},initialOffset[3]{},relativeAxis[9]{};
    bool active{},calibrating{},palmSettled{};
};
std::array<ObjectDiscoveryNativeRecord,32> objectDiscoveryNative{};
ObjectDiscoveryHeld objectDiscoveryHeld{};
std::mutex objectDiscoveryGripMutex;
ObjectDiscoveryGripFrame objectDiscoveryGripInput{};
KharvoxObjectGripFeedback objectDiscoveryGripFeedback{};
unsigned long long objectDiscoveryReleaseSerial{},objectDiscoveryLastTickMs{};
bool objectDiscoveryReadable(const void* address,std::size_t len) {
    return address&&len&&readableMemory(address,len);
}
bool objectDiscoveryAxisOk(const float* axis) {
    if(!axis)return false;
    for(int row=0;row<3;++row){
        float squared{};
        for(int col=0;col<3;++col){
            const float x=axis[row*3+col];
            if(!std::isfinite(x)||std::abs(x)>1.1f)return false;
            squared+=x*x;
        }
        if(squared<.82f||squared>1.18f)return false;
    }
    return true;
}
bool objectDiscoveryReadBodyPose(ObjectDiscoveryNativeRecord& item,
    float origin[3],float axis[9]) {
    if(!item.entity||!item.body
        ||item.generation!=levelTransitionGeneration.load(std::memory_order_acquire)
        ||!objectDiscoveryReadable(item.entity,sizeof(std::uintptr_t))
        ||!objectDiscoveryReadable(item.body,sizeof(std::uintptr_t)))
        return false;
    std::uintptr_t ev{},bv{};
    std::memcpy(&ev,item.entity,sizeof(ev));
    std::memcpy(&bv,item.body,sizeof(bv));
    if(ev!=item.entityVtable||bv!=item.bodyVtable
        ||bv<item.imageBase||bv-item.imageBase>=item.imageSize)return false;
    std::uintptr_t originFn{},axisFn{};
    if(!objectDiscoveryReadable(reinterpret_cast<const void*>(bv+0x80),
            sizeof(originFn))
        ||!objectDiscoveryReadable(reinterpret_cast<const void*>(bv+0x88),
            sizeof(axisFn)))return false;
    std::memcpy(&originFn,reinterpret_cast<const void*>(bv+0x80),
        sizeof(originFn));
    std::memcpy(&axisFn,reinterpret_cast<const void*>(bv+0x88),
        sizeof(axisFn));
    if(originFn<item.imageBase||originFn-item.imageBase>=item.imageSize
        ||axisFn<item.imageBase||axisFn-item.imageBase>=item.imageSize
        ||!executableMemory(reinterpret_cast<const void*>(originFn))
        ||!executableMemory(reinterpret_cast<const void*>(axisFn)))return false;
    if(!objectDiscoveryReadOrigin(item.body,originFn,origin)
        ||!objectDiscoveryReadOrigin(item.body,axisFn,axis)
        ||!objectDiscoveryAxisOk(axis))return false;
    return true;
}
bool objectDiscoveryWriteBody(ObjectDiscoveryNativeRecord& item,
    const float origin[3],const float axis[9]) {
#if KHARVOX_EXPERIMENTAL_WORLD_GRAB
    // All writes are confined to already independently moving, locked,
    // exact-vtable-validated rigid bodies. Neither scan nor Grip initiation
    // can call Activate or move an object before this gate.
    if(!item.nativeMotionObserved||!objectDiscoveryAxisOk(axis))return false;
    float priorOrigin[3]{},priorAxis[9]{};
    if(!objectDiscoveryReadBodyPose(item,priorOrigin,priorAxis))return false;
    std::uintptr_t originFn{},axisFn{};
    std::memcpy(&originFn,reinterpret_cast<const void*>(item.bodyVtable+0x80),
        sizeof(originFn));
    std::memcpy(&axisFn,reinterpret_cast<const void*>(item.bodyVtable+0x88),
        sizeof(axisFn));
    using Fn=float* (__fastcall*)(void*,int);
    auto* nativeOrigin=reinterpret_cast<Fn>(originFn)(item.body,0);
    auto* nativeAxis=reinterpret_cast<Fn>(axisFn)(item.body,0);
    if(!nativeOrigin||!nativeAxis
        ||!writableMemory(nativeOrigin,sizeof(float)*3)
        ||!writableMemory(nativeAxis,sizeof(float)*9))return false;
#if defined(_MSC_VER)
    __try{
#endif
        std::memcpy(nativeOrigin,origin,sizeof(float)*3);
        std::memcpy(nativeAxis,axis,sizeof(float)*9);
#if defined(_MSC_VER)
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
#endif
    return true;
#else
    (void)item;(void)origin;(void)axis;return false;
#endif
}
void objectDiscoveryNativeModel(ObjectDiscoveryNativeRecord& item) {
    if(item.modelChecked)return;
    item.modelChecked=true;
    if(!item.entity)return;
    // Strictly consider native C string pointers to actual model asset
    // paths. Class names, handles, material paths and guessed labels are
    // forbidden as persistent profile identities.
    std::array<char,160> found{};
    bool multiple{};
    for(std::size_t off=0;off<0x1000;off+=sizeof(void*)){
        const auto* slot=static_cast<const unsigned char*>(item.entity)+off;
        if(!objectDiscoveryReadable(slot,sizeof(void*)))break;
        const char* ptr{};
        std::memcpy(&ptr,slot,sizeof(ptr));
        if(!objectDiscoveryReadable(ptr,96))continue;
        if(std::memcmp(ptr,"models/",7)!=0
            &&std::memcmp(ptr,"model/",6)!=0)continue;
        std::array<char,160> candidate{};
        unsigned n{};
        for(;n<candidate.size()-1&&objectDiscoveryReadable(ptr+n,1);++n){
            char c=ptr[n];if(!c)break;
            const bool allowed=(c>='A'&&c<='Z')||(c>='a'&&c<='z')
                ||(c>='0'&&c<='9')||c=='/'||c=='_'||c=='-'
                ||c=='.';
            if(!allowed)break;
            candidate[n]=c;
        }
        if(n<12||n>=candidate.size()-1||ptr[n])continue;
        const std::string path(candidate.data());
        const auto ends=[&](const char* suffix){
            const auto suffixLen=std::strlen(suffix);
            return path.size()>suffixLen&&path.compare(
                path.size()-suffixLen,suffixLen,suffix)==0;
        };
        if(!ends(".md6mesh")&&!ends(".md5mesh")&&!ends(".lwo")
            &&!ends(".mesh")&&!ends(".model")&&!ends(".static")
            &&!ends(".obj"))continue;
        if(found[0]&&std::strcmp(found.data(),candidate.data())!=0){
            multiple=true;break;
        }
        found=candidate;
    }
    if(!multiple)item.modelKey=found;
}
ObjectDiscoveryNativeRecord* objectDiscoveryNativeByHandle(unsigned handle) {
    for(auto& item:objectDiscoveryNative)
        if(item.handle==handle&&item.entity)return &item;
    return nullptr;
}
void objectDiscoveryPublish(const ObjectDiscoveryNativeRecord& item,
    unsigned long long now) {
    std::lock_guard<std::mutex> lock(objectDiscoveryMutex);
    auto& snapshot=objectDiscoverySnapshot;
    if(snapshot.generation!=item.generation){
        snapshot={};snapshot.generation=item.generation;
    }
    unsigned slot=snapshot.count;
    for(unsigned i=0;i<snapshot.count;++i)
        if(snapshot.candidates[i].handle==item.handle){slot=i;break;}
    if(slot==snapshot.candidates.size()){
        slot=0;
        for(unsigned i=1;i<snapshot.count;++i)
            if(snapshot.candidates[i].observedAtMs
                <snapshot.candidates[slot].observedAtMs)slot=i;
    }else if(slot==snapshot.count)++snapshot.count;
    auto& out=snapshot.candidates[slot];
    out={};out.handle=item.handle;
    out.generation=item.generation;out.observedAtMs=now;
    out.nativeMotionObserved=item.nativeMotionObserved;
    out.modelKey=item.modelKey;
    for(int i=0;i<3;++i)out.origin[i]=item.origin[i];
}
void objectDiscoveryReleaseNative() {
    objectDiscoveryHeld={};
    std::lock_guard<std::mutex> lock(objectDiscoveryGripMutex);
    objectDiscoveryGripFeedback={};
}
void objectDiscoveryGameTick() {
#if KHARVOX_EXPERIMENTAL_WORLD_GRAB
    const auto now=GetTickCount64();
    if(now<objectDiscoveryLastTickMs||now-objectDiscoveryLastTickMs<12)
        return;
    objectDiscoveryLastTickMs=now;
    ObjectDiscoveryGripFrame input{};
    {
        std::lock_guard<std::mutex> lock(objectDiscoveryGripMutex);
        input=objectDiscoveryGripInput;
    }
    const bool inputValid=input.enabled&&input.frameAtMs
        &&now>=input.frameAtMs&&now-input.frameAtMs<180
        &&objectDiscoveryAxisOk(input.axis);
    if(!inputValid||input.releaseSerial!=objectDiscoveryReleaseSerial
        ||!KharvoxCameraWorldActive()||KharvoxCameraCutsceneActive()
        ||KharvoxCameraBossSequenceActive()){
        objectDiscoveryReleaseSerial=input.releaseSerial;
        objectDiscoveryReleaseNative();
    }
    const auto generation=levelTransitionGeneration.load(std::memory_order_acquire);
    if(!nativeGrabFocusObservationEnabled.load(std::memory_order_acquire)){
        objectDiscoveryNative={};
        objectDiscoveryReleaseNative();
        return;
    }
    for(auto& item:objectDiscoveryNative){
        if(!item.entity)continue;
        if(item.generation!=generation){item={};continue;}
        // Throttle read-only physics sampling and preserve the
        // exact object even when FocusTracker points somewhere else.
        if(now>=item.lastPositionAtMs
            &&now-item.lastPositionAtMs<40
            &&(!objectDiscoveryHeld.active
                ||objectDiscoveryHeld.handle!=item.handle))continue;
        float xyz[3]{},axis[9]{};
        if(!objectDiscoveryReadBodyPose(item,xyz,axis)){
            if(objectDiscoveryHeld.active
                &&objectDiscoveryHeld.handle==item.handle)
                objectDiscoveryReleaseNative();
            item={};continue;
        }
        if(item.poseValid
            &&(!objectDiscoveryHeld.active
                ||objectDiscoveryHeld.handle!=item.handle)){
            float squared{};
            for(int i=0;i<3;++i){
                const float delta=xyz[i]-item.origin[i];
                squared+=delta*delta;
            }
            if(squared>.25f&&squared<250000.f){
                item.nativeMotionObserved=true;
                item.nativeMovedAtMs=now;
            }
        }
        std::memcpy(item.origin,xyz,sizeof(xyz));
        std::memcpy(item.axis,axis,sizeof(axis));
        item.poseValid=true;item.lastPositionAtMs=now;
        objectDiscoveryPublish(item,now);
    }
    if(!inputValid)return;
    if(!objectDiscoveryHeld.active){
        if(!input.pressed)return;
        ObjectDiscoveryNativeRecord* nearest{};
        float best2=input.reachUnits*input.reachUnits;
        for(auto& item:objectDiscoveryNative){
            if(!item.entity||!item.poseValid||!item.nativeMotionObserved
                ||!item.nativeMovedAtMs||now-item.nativeMovedAtMs>3000
                ||now-item.lastPositionAtMs>250||item.generation!=generation)
                continue;
            float squared{},forward{};
            for(int axis=0;axis<3;++axis){
                const float delta=item.origin[axis]-input.origin[axis];
                squared+=delta*delta;
                forward+=delta*input.aim[axis];
            }
            if(squared>=best2||forward<=0.f
                ||forward*forward<.64f*squared)continue;
            best2=squared;nearest=&item;
        }
        if(!nearest)return;
        auto& held=objectDiscoveryHeld;
        held={};held.active=true;held.handle=nearest->handle;
        held.physicalHand=input.physicalHand;
        held.generation=generation;held.capturedAtMs=now;
        for(int local=0;local<3;++local){
            for(int world=0;world<3;++world)
                held.relativeOffset[local]+=input.axis[local*3+world]
                    *(nearest->origin[world]-input.origin[world]);
            held.initialOffset[local]=held.relativeOffset[local];
            for(int col=0;col<3;++col){
                float component{};
                for(int world=0;world<3;++world)
                    component+=input.axis[local*3+world]
                        *nearest->axis[col*3+world];
                held.relativeAxis[col*3+local]=component;
            }
        }
        if(!objectDiscoveryAxisOk(held.relativeAxis)){
            held={};return;
        }
    }
    auto& held=objectDiscoveryHeld;
    auto* item=objectDiscoveryNativeByHandle(held.handle);
    if(!item||held.generation!=generation
        ||held.physicalHand!=input.physicalHand
        ||(!input.pressed&&!input.calibrating)){
        objectDiscoveryReleaseNative();return;
    }
    held.calibrating=input.calibrating;
    if(held.calibrating&&input.freeze){
        // Rotation calibration: keep the object WORLD-FIXED while the
        // controller turns, updating the local transform as needed.
        for(int local=0;local<3;++local){
            float off{};
            for(int world=0;world<3;++world)
                off+=input.axis[local*3+world]
                    *(item->origin[world]-input.origin[world]);
            held.relativeOffset[local]=off;
            for(int col=0;col<3;++col){
                float basis{};
                for(int world=0;world<3;++world)
                    basis+=input.axis[local*3+world]
                        *item->axis[col*3+world];
                held.relativeAxis[col*3+local]=basis;
            }
        }
    }else{
        if(input.calibrating&&input.offsetValid){
            for(int i=0;i<3;++i)held.relativeOffset[i]=input.wantedOffset[i];
        }else if(!input.calibrating&&input.offsetValid){
            for(int i=0;i<3;++i)held.relativeOffset[i]=input.wantedOffset[i];
            if(input.axisValid&&objectDiscoveryAxisOk(input.wantedAxis))
                std::memcpy(held.relativeAxis,input.wantedAxis,
                    sizeof(held.relativeAxis));
        }else if(!input.calibrating){
            // Move the object from the catch offset to a palm-adjacent
            // hold point without an instantaneous physics teleport.
            const float t=std::clamp(
                float(now-held.capturedAtMs)/300.f,0.f,1.f);
            const float ease=t*t*(3.f-2.f*t);
            constexpr float palm[3]{0.f,-4.f,-20.f};
            for(int i=0;i<3;++i)
                held.relativeOffset[i]=held.initialOffset[i]*(1.f-ease)
                    +palm[i]*ease;
        }
        float desired[3]{},desiredAxis[9]{};
        for(int world=0;world<3;++world){
            desired[world]=input.origin[world];
            for(int local=0;local<3;++local)
                desired[world]+=input.axis[local*3+world]
                    *held.relativeOffset[local];
            for(int col=0;col<3;++col){
                float basis{};
                for(int local=0;local<3;++local)
                    basis+=input.axis[local*3+world]
                        *held.relativeAxis[col*3+local];
                desiredAxis[col*3+world]=basis;
            }
        }
        float len2{};
        for(int i=0;i<3;++i){
            float d=desired[i]-item->origin[i];len2+=d*d;
        }
        const float len=std::sqrt(len2);
        if(!std::isfinite(len)||len>500.f){
            objectDiscoveryReleaseNative();return;
        }
        const float factor=len>28.f?28.f/len:1.f;
        for(int i=0;i<3;++i)
            desired[i]=item->origin[i]
                +(desired[i]-item->origin[i])*factor;
        if(objectDiscoveryAxisOk(desiredAxis)
            &&objectDiscoveryWriteBody(*item,desired,desiredAxis)){
            std::memcpy(item->origin,desired,sizeof(desired));
            std::memcpy(item->axis,desiredAxis,sizeof(desiredAxis));
            objectDiscoveryPublish(*item,now);
        }
    }
    {
        std::lock_guard<std::mutex> lock(objectDiscoveryGripMutex);
        auto& f=objectDiscoveryGripFeedback;
        f={};f.held=true;f.handle=item->handle;
        f.physicalHand=held.physicalHand;f.nativeMotionVerified=true;
        f.modelKey=item->modelKey;
        for(int i=0;i<3;++i)f.localOffsetUnits[i]=held.relativeOffset[i];
        std::memcpy(f.localAxis,held.relativeAxis,sizeof(f.localAxis));
    }
#else
    objectDiscoveryReleaseNative();
#endif
}
void objectDiscoverySeenEntity(const void* entity,std::uintptr_t base,
    std::uintptr_t imageSize,unsigned handle,unsigned long long generation) {
#if KHARVOX_EXPERIMENTAL_WORLD_GRAB
    if(!entity||!handle)return;
    const auto now=GetTickCount64();
    std::uintptr_t ev{};
    if(!objectDiscoveryReadable(entity,sizeof(ev)))return;
    std::memcpy(&ev,entity,sizeof(ev));
    if(ev<base||ev-base>=imageSize)return;
    ObjectDiscoveryNativeRecord* item=objectDiscoveryNativeByHandle(handle);
    if(item&&(item->entity!=entity||item->entityVtable!=ev
        ||item->generation!=generation)){*item={};item=nullptr;}
    if(!item){
        item=&objectDiscoveryNative[0];
        for(auto& existing:objectDiscoveryNative)
            if(!existing.entity||existing.observedAtMs<item->observedAtMs)
                item=&existing;
        *item={};item->entity=entity;item->entityVtable=ev;
        item->handle=handle;item->generation=generation;
        item->imageBase=base;item->imageSize=imageSize;
    }
    item->observedAtMs=now;
    if(!item->body&&now>=item->lastProbeAtMs
        &&now-item->lastProbeAtMs>=400){
        item->lastProbeAtMs=now;
        for(std::size_t off=0;off<0x4000;off+=sizeof(void*)){
            const auto* slot=static_cast<const unsigned char*>(entity)+off;
            if(!objectDiscoveryReadable(slot,sizeof(void*)))break;
            const void* ptr{};
            std::memcpy(&ptr,slot,sizeof(ptr));
            for(int layout=0;layout<2;++layout){
                const void* candidate=layout?ptr:static_cast<const void*>(slot);
                if(!objectDiscoveryReadable(candidate,sizeof(void*)))continue;
                std::uintptr_t vt{};
                std::memcpy(&vt,candidate,sizeof(vt));
                if(vt<base||vt-base>=imageSize)continue;
                std::array<char,80> klass{};
                if(!readNativeWorldGrabClassLabel(vt,base,imageSize,klass)
                    ||std::strcmp(klass.data(),"idPhysics_RigidBody")!=0)
                    continue;
                item->body=const_cast<void*>(candidate);item->bodyVtable=vt;
                break;
            }
            if(item->body)break;
        }
        if(item->body)objectDiscoveryNativeModel(*item);
    }
    if(item->body)objectDiscoveryGameTick();
#else
    (void)entity;(void)base;(void)imageSize;(void)handle;(void)generation;
#endif
}
