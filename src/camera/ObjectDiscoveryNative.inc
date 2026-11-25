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
    float scanUnits{400.f};
    float spinStickX{}; // right-stick horizontal, only while calibrating with GRIP
    unsigned physicalHand{};
    unsigned long long frameAtMs{},releaseSerial{};
    bool enabled{},pressed{},calibrating{},freeze{},offsetValid{},axisValid{};
    bool resetToNeutral{}; // one edge, re-arm only when the chord releases
    float wantedOffset[3]{},wantedAxis[9]{};
};
struct ObjectDiscoveryHeld {
    unsigned handle{},physicalHand{};
    unsigned long long generation{},capturedAtMs{};
    float relativeOffset[3]{},initialOffset[3]{},relativeAxis[9]{};
    float rotationHandStart[9]{},rotationBodyStart[9]{};
    float spinDegrees{};
    unsigned long long lastRotationAtMs{},lastGripDownAtMs{};
    unsigned poseReadMisses{};
    bool rotationActive{},neutralResetLatched{};
    bool active{},calibrating{},palmSettled{},nativeFollowConfirmed{};
};
std::array<ObjectDiscoveryNativeRecord,128> objectDiscoveryNative{};
ObjectDiscoveryHeld objectDiscoveryHeld{};
std::mutex objectDiscoveryGripMutex;
ObjectDiscoveryGripFrame objectDiscoveryGripInput{};
ObjectDiscoveryGripFrame objectDiscoveryLastGoodInput{};
KharvoxObjectGripFeedback objectDiscoveryGripFeedback{};
unsigned long long objectDiscoveryReleaseSerial{},objectDiscoveryLastTickMs{};
bool objectDiscoveryNeedsGripRelease{};
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
bool objectDiscoveryReadAxis(const void* body,std::uintptr_t function,
    float result[9]){
    if(!body||!function)return false;
    using Fn=const float* (__fastcall*)(void*,int);
#if defined(_MSC_VER)
    __try{
#endif
        const auto* p=reinterpret_cast<Fn>(function)(
            const_cast<void*>(body),0);
        if(!p||!objectDiscoveryReadable(p,sizeof(float)*9))return false;
        std::memcpy(result,p,sizeof(float)*9);
        return objectDiscoveryAxisOk(result);
#if defined(_MSC_VER)
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
#endif
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
        ||!objectDiscoveryReadAxis(item.body,axisFn,axis)
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
// Read-only, fail-closed discovery of a native entity-pointer pool.
// The manager slot is proven by both exact FocusTracker instruction
// signatures at install time. No speculative calls into DOOM are made.
// If the manager does not expose a pointer pool, ordinary ray-candidates
// remain available and this scanner stays disabled.
struct ObjectDiscoveryNativePool {
    const unsigned char* begin{};
    std::uintptr_t imageBase{},imageSize{};
    unsigned long long generation{},lastAttemptMs{},lastSweepMs{};
    unsigned cursor{},entries{};
};
ObjectDiscoveryNativePool objectDiscoveryNativePool{};
void objectDiscoverySeenEntity(const void* entity,std::uintptr_t base,
    std::uintptr_t size,unsigned handle,unsigned long long generation);
void objectDiscoveryLocateNativePool(const void* knownEntity,
    std::uintptr_t imageBase,std::uintptr_t imageSize,
    unsigned long long generation){
#if KHARVOX_EXPERIMENTAL_WORLD_GRAB
    if(!knownEntity)return;
    auto& pool=objectDiscoveryNativePool;
    if(pool.generation!=generation)pool={};
    const auto now=GetTickCount64();
    if(pool.begin||now-pool.lastAttemptMs<5000)return;
    pool.lastAttemptMs=now;pool.generation=generation;
    // RIP-displacement was verified at FocusTracker's two native
    // entity-manager callsites. Do not invoke the manager or dereference
    // it if any page/vtable fails validation.
    const std::uintptr_t slot=imageBase+0xD48B60+7+0x04DC6B69;
    if(!objectDiscoveryReadable(reinterpret_cast<const void*>(slot),
        sizeof(std::uintptr_t)))return;
    std::uintptr_t manager{};
    std::memcpy(&manager,reinterpret_cast<const void*>(slot),
        sizeof(manager));
    if(!objectDiscoveryReadable(reinterpret_cast<const void*>(manager),
        0x400))return;
    std::uintptr_t managerVtable{};
    std::memcpy(&managerVtable,reinterpret_cast<const void*>(manager),
        sizeof(managerVtable));
    if(managerVtable<imageBase
        ||managerVtable-imageBase>=imageSize)return;
    constexpr std::size_t testBytes=0x4000;
    for(std::size_t off=0;off<0x400;off+=sizeof(void*)){
        const auto field=reinterpret_cast<const unsigned char*>(manager)+off;
        std::uintptr_t candidate{};
        std::memcpy(&candidate,field,sizeof(candidate));
        if(candidate<0x10000||candidate>=imageBase
            &&candidate-imageBase<imageSize
            ||!objectDiscoveryReadable(
                reinterpret_cast<const void*>(candidate),testBytes))
            continue;
        auto* table=reinterpret_cast<const unsigned char*>(candidate);
        bool knownFound{};
        unsigned entityPointers{};
        // A real pointer pool must contain the DOOM-resolved entity AND
        // at least three distinct MSVC idEntity-derived objects.
        for(unsigned entry=0;entry<testBytes/sizeof(void*);++entry){
            const void* p{};
            std::memcpy(&p,table+entry*sizeof(void*),sizeof(p));
            if(p==knownEntity)knownFound=true;
            if(!objectDiscoveryReadable(p,sizeof(std::uintptr_t)))continue;
            std::uintptr_t vt{};
            std::memcpy(&vt,p,sizeof(vt));
            if(vt<imageBase||vt-imageBase>=imageSize)continue;
            std::array<char,80> label{};
            if(readNativeWorldGrabClassLabel(vt,imageBase,imageSize,label)
                &&std::strncmp(label.data(),"id",2)==0){
                ++entityPointers;
                if(knownFound&&entityPointers>=3)break;
            }
        }
        if(!knownFound||entityPointers<3)continue;
        pool.begin=table;pool.imageBase=imageBase;
        pool.imageSize=imageSize;
        pool.entries=static_cast<unsigned>(testBytes/sizeof(void*));
        pool.cursor=0;
        break;
    }
#else
    (void)knownEntity;(void)imageBase;
    (void)imageSize;(void)generation;
#endif
}
void objectDiscoverySweepNativePool(unsigned long long now,
    unsigned long long generation){
#if KHARVOX_EXPERIMENTAL_WORLD_GRAB
    auto& pool=objectDiscoveryNativePool;
    if(!pool.begin||pool.generation!=generation)return;
    if(now<pool.lastSweepMs||now-pool.lastSweepMs<125)return;
    pool.lastSweepMs=now;
    if(!objectDiscoveryReadable(pool.begin,0x4000)){
        pool={};return;
    }
    for(unsigned count=0;count<32&&pool.entries;++count){
        const unsigned index=pool.cursor++%pool.entries;
        const void* entity{};
        std::memcpy(&entity,pool.begin+index*sizeof(void*),
            sizeof(entity));
        if(!objectDiscoveryReadable(entity,sizeof(std::uintptr_t)))
            continue;
        std::uintptr_t vt{};
        std::memcpy(&vt,entity,sizeof(vt));
        if(vt<pool.imageBase||vt-pool.imageBase>=pool.imageSize)
            continue;
        std::array<char,80> label{};
        if(!readNativeWorldGrabClassLabel(
                vt,pool.imageBase,pool.imageSize,label))continue;
        if(std::strcmp(label.data(),"idProp2")!=0
            &&std::strcmp(label.data(),"idProp_Moveable")!=0)continue;
        unsigned handle=0;
        for(const auto& seen:objectDiscoveryNative)
            if(seen.entity==entity&&seen.generation==generation){
                handle=seen.handle;break;
            }
        if(!handle){
            const auto ptr=reinterpret_cast<std::uintptr_t>(entity);
            handle=static_cast<unsigned>((ptr>>4)^(ptr>>36));
            if(!handle)handle=1u;
        }
        objectDiscoverySeenEntity(entity,pool.imageBase,pool.imageSize,
            handle,generation);
    }
#else
    (void)now;(void)generation;
#endif
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
// Gentle native drop. No artificial downward velocity, kick or force.
// After previously verified real movement and a successful hold, clear any
// stale solver momentum and resume DOOM's own gravity via its exact
// signature-checked native Activate implementation. NEVER run on discovery,
// ray hits, or stationary/unverified bodies.
bool objectDiscoveryReleaseToGravity(ObjectDiscoveryNativeRecord& item) {
#if KHARVOX_EXPERIMENTAL_WORLD_GRAB
    if(!item.nativeMotionObserved
        ||item.generation!=levelTransitionGeneration.load(
            std::memory_order_acquire)
        ||!KharvoxCameraWorldActive())return false;
    float xyz[3]{},basis[9]{};
    if(!objectDiscoveryReadBodyPose(item,xyz,basis))return false;
    const auto base=item.imageBase;
    const auto* linear=reinterpret_cast<const unsigned char*>(
        base+0x179A958);
    const auto* angular=reinterpret_cast<const unsigned char*>(
        base+0x179AA70);
    constexpr unsigned char linearSig[]{
        0xF3,0x0F,0x10,0x87,0xEC,0x01,0x00,0x00,
        0xF3,0x0F,0x58,0x06,0xF3,0x0F,0x11,0x87,
        0xEC,0x01,0x00,0x00
    };
    constexpr unsigned char angularSig[]{
        0xF3,0x0F,0x11,0xA7,0xF8,0x01,0x00,0x00,
        0xF3,0x0F,0x58,0x87,0xFC,0x01,0x00,0x00
    };
    constexpr unsigned char wakeSig[]{
        0xC7,0x81,0x70,0x01,0x00,0x00,
        0xFF,0xFF,0xFF,0xFF,0xC3
    };
    const auto* wake=reinterpret_cast<const unsigned char*>(
        base+0x179A8F0);
    if(!objectDiscoveryReadable(linear,sizeof(linearSig))
        ||!objectDiscoveryReadable(angular,sizeof(angularSig))
        ||!objectDiscoveryReadable(wake,sizeof(wakeSig))
        ||std::memcmp(linear,linearSig,sizeof(linearSig))!=0
        ||std::memcmp(angular,angularSig,sizeof(angularSig))!=0
        ||std::memcmp(wake,wakeSig,sizeof(wakeSig))!=0
        ||!executableMemory(wake))return false;
    auto* linearVelocity=reinterpret_cast<float*>(
        static_cast<unsigned char*>(item.body)+0x1EC);
    auto* angularVelocity=reinterpret_cast<float*>(
        static_cast<unsigned char*>(item.body)+0x1F8);
    if(!writableMemory(linearVelocity,3*sizeof(float))
        ||!writableMemory(angularVelocity,3*sizeof(float)))
        return false;
    for(int i=0;i<3;++i)
        if(!std::isfinite(linearVelocity[i])
            ||!std::isfinite(angularVelocity[i])
            ||std::abs(linearVelocity[i])>1e7f
            ||std::abs(angularVelocity[i])>1e7f)return false;
#if defined(_MSC_VER)
    __try{
#endif
        constexpr float stopped[3]{0,0,0};
        std::memcpy(linearVelocity,stopped,sizeof(stopped));
        std::memcpy(angularVelocity,stopped,sizeof(stopped));
        using Fn=void (__fastcall*)(void*);
        reinterpret_cast<Fn>(base+0x179A8F0)(item.body);
#if defined(_MSC_VER)
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
#endif
    return true;
#else
    (void)item;return false;
#endif
}
void objectDiscoveryReleaseNative() {
    if(objectDiscoveryHeld.active&&objectDiscoveryHeld.nativeFollowConfirmed){
        if(auto* item=objectDiscoveryNativeByHandle(
                objectDiscoveryHeld.handle))
            if(item->generation==objectDiscoveryHeld.generation)
                objectDiscoveryReleaseToGravity(*item);
    }
    objectDiscoveryHeld={};
    objectDiscoveryLastGoodInput={};
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
    bool inputValid=input.enabled&&input.frameAtMs
        &&now>=input.frameAtMs&&now-input.frameAtMs<180
        &&objectDiscoveryAxisOk(input.axis);
    const bool explicitRelease=input.releaseSerial!=objectDiscoveryReleaseSerial;
    const bool worldBlocked=!KharvoxCameraWorldActive()
        ||KharvoxCameraCutsceneActive()||KharvoxCameraBossSequenceActive();
    // A single lost tracking sample / late XR submit must not drop an
    // already-attached rigid body. Retain its last validated hand pose for
    // at most 300ms; NEVER bridge an explicit release or blocked world.
    if(inputValid){
        objectDiscoveryLastGoodInput=input;
    }else if(objectDiscoveryHeld.active&&!explicitRelease&&!worldBlocked
        &&objectDiscoveryLastGoodInput.frameAtMs
        &&now>=objectDiscoveryLastGoodInput.frameAtMs
        &&now-objectDiscoveryLastGoodInput.frameAtMs<=300){
        const auto releaseSerial=input.releaseSerial;
        input=objectDiscoveryLastGoodInput;
        input.releaseSerial=releaseSerial;
        inputValid=true;
    }
    if(!inputValid||explicitRelease||worldBlocked){
        if(explicitRelease)objectDiscoveryNeedsGripRelease=true;
        objectDiscoveryReleaseSerial=input.releaseSerial;
        objectDiscoveryReleaseNative();
        if(!inputValid||worldBlocked||explicitRelease)return;
    }
    if(!input.pressed)objectDiscoveryNeedsGripRelease=false;
    const auto generation=levelTransitionGeneration.load(std::memory_order_acquire);
    if(!nativeGrabFocusObservationEnabled.load(std::memory_order_acquire)){
        objectDiscoveryNative={};
        objectDiscoveryReleaseNative();
        return;
    }
    objectDiscoverySweepNativePool(now,generation);
    // Larger 50m discovery cache, but a fixed per-tick physics read budget.
    // Round-robin fairness avoids continuously rereading the first objects;
    // a held object is exempt so its controller attachment stays responsive.
    static unsigned readCursor{};
    constexpr unsigned readsPerTick=24;
    unsigned reads{};
    const unsigned cacheCount=static_cast<unsigned>(objectDiscoveryNative.size());
    for(unsigned checked=0;checked<cacheCount;++checked){
        auto& item=objectDiscoveryNative[(readCursor+checked)%cacheCount];
        if(!item.entity)continue;
        if(item.generation!=generation){item={};continue;}
        const bool heldHere=objectDiscoveryHeld.active
            &&objectDiscoveryHeld.handle==item.handle;
        unsigned long long readIntervalMs=40;
        if(!heldHere&&item.poseValid&&inputValid){
            float d2{};
            for(int axis=0;axis<3;++axis){
                const float d=item.origin[axis]-input.origin[axis];
                d2+=d*d;
            }
            if(d2>input.reachUnits*input.reachUnits*4.f)
                readIntervalMs=140;
        }
        if(!heldHere&&now>=item.lastPositionAtMs
            &&now-item.lastPositionAtMs<readIntervalMs)continue;
        if(!heldHere&&reads>=readsPerTick)continue;
        if(!heldHere)++reads;
        float xyz[3]{},axis[9]{};
        if(!objectDiscoveryReadBodyPose(item,xyz,axis)){
            if(heldHere&&++objectDiscoveryHeld.poseReadMisses<=3)
                continue; // never write with unverified pose; retry briefly
            if(heldHere)objectDiscoveryReleaseNative();
            item={};continue;
        }
        if(heldHere)objectDiscoveryHeld.poseReadMisses=0;
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
    readCursor=(readCursor+readsPerTick)%cacheCount;
    if(!inputValid)return;
    if(!objectDiscoveryHeld.active){
        if(!input.pressed||objectDiscoveryNeedsGripRelease)return;
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
                held.initialOffset[local]+=input.axis[local*3+world]
                    *(nearest->origin[world]-input.origin[world]);
            held.relativeOffset[local]=held.initialOffset[local];
        }
        // Regardless of how the rigid body was lying, begin every pickup
        // at the authored canonical upright (world identity) and the same
        // centered palm-forward target. Do not inherit a rolled barrel.
        const auto neutral=kharvox::objectGripNeutralLocalAxis(input.axis);
        if(!objectDiscoveryAxisOk(neutral.data())){
            held={};return;
        }
        std::copy(neutral.begin(),neutral.end(),held.relativeAxis);
        held.lastGripDownAtMs=now;
    }
    auto& held=objectDiscoveryHeld;
    auto* item=objectDiscoveryNativeByHandle(held.handle);
    if(!item||held.generation!=generation
        ||held.physicalHand!=input.physicalHand){
        objectDiscoveryReleaseNative();return;
    }
    if(input.pressed)held.lastGripDownAtMs=now;
    // Debounce a momentary false release from a noisy controller trigger.
    // A deliberate release outside calibration still drops after 150ms.
    if(!input.pressed&&!input.calibrating
        &&(now<held.lastGripDownAtMs
           ||now-held.lastGripDownAtMs>150)){
        objectDiscoveryReleaseNative();return;
    }
    held.calibrating=input.calibrating;
    // The GRIP+offhand-trigger chord is an explicit re-calibration RESET:
    // send both pose components to the same neutral starting point as a
    // fresh pickup, regardless of the object's current physical attitude.
    // Preserve the native held rigid body; do not drop/regrab or Activate.
    if(input.resetToNeutral&&!held.neutralResetLatched){
        const auto neutral=kharvox::objectGripNeutralLocalAxis(input.axis);
        if(objectDiscoveryAxisOk(neutral.data())){
            std::copy(neutral.begin(),neutral.end(),held.relativeAxis);
            for(int i=0;i<3;++i){
                held.relativeOffset[i]=kharvox::kObjectGripNeutralOffsetUnits[i];
                held.initialOffset[i]=held.relativeOffset[i];
            }
            held.capturedAtMs=now;
            held.rotationActive=false;
            held.spinDegrees=0.f;
        }
    }
    held.neutralResetLatched=input.resetToNeutral;
    if(held.calibrating&&input.freeze){
        // A stable once-per-GRIP reference prevents physics feedback from
        // fighting the controller and flipping an item every other frame.
        // Hand pitch/roll tilts; horizontal right stick spins the object's
        // own local upright axis. The object stays at the calibrated offset.
        if(!held.rotationActive){
            std::memcpy(held.rotationHandStart,input.axis,
                sizeof(held.rotationHandStart));
            // Use our own held target, NOT the live physics body's pose:
            // the latter can still be lying on its side while the first
            // bounded steps bring it upright.
            for(int col=0;col<3;++col)
                for(int world=0;world<3;++world){
                    float value{};
                    for(int local=0;local<3;++local)
                        value+=input.axis[local*3+world]
                            *held.relativeAxis[col*3+local];
                    held.rotationBodyStart[col*3+world]=value;
                }
            held.spinDegrees=0.f;
            held.lastRotationAtMs=now;
            held.rotationActive=true;
        }
        const float seconds=std::min<unsigned long long>(
            now>=held.lastRotationAtMs?now-held.lastRotationAtMs:0,40ULL)
            *.001f;
        held.lastRotationAtMs=now;
        held.spinDegrees=std::remainder(
            held.spinDegrees
            +kharvox::objectGripSpinAxis(input.spinStickX)*90.f*seconds,
            360.f);
        const auto desiredWorld=kharvox::objectGripTiltAndSpinAxis(
            input.axis,held.rotationHandStart,held.rotationBodyStart,
            held.spinDegrees);
        const auto desiredLocal=kharvox::objectGripToHandLocalAxis(
            input.axis,desiredWorld.data());
        if(objectDiscoveryAxisOk(desiredLocal.data()))
            std::copy(desiredLocal.begin(),desiredLocal.end(),
                held.relativeAxis);
    }else{
        // On GRIP release keep the final local rotation as-is; don't snap.
        // The next GRIP press takes fresh stable references.
        held.rotationActive=false;
        held.lastRotationAtMs=now;
    }
    {
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
            for(int i=0;i<3;++i)
                held.relativeOffset[i]=held.initialOffset[i]*(1.f-ease)
                    +kharvox::kObjectGripNeutralOffsetUnits[i]*ease;
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
        // A 5m reach can exceed the former 500-unit emergency cutoff
        // after we move its target front-centre, especially if the player
        // moves quickly while holding it. Keep a generous finite safety
        // bound, and increase the per-world-tick tracking cap from 28 to
        // 40 native units without changing gravity or injecting velocity.
        if(!std::isfinite(len)||len>1500.f){
            objectDiscoveryReleaseNative();return;
        }
        constexpr float followPerTick=40.f;
        const float factor=len>followPerTick?followPerTick/len:1.f;
        for(int i=0;i<3;++i)
            desired[i]=item->origin[i]
                +(desired[i]-item->origin[i])*factor;
        if(objectDiscoveryAxisOk(desiredAxis)
            &&objectDiscoveryWriteBody(*item,desired,desiredAxis)){
            held.nativeFollowConfirmed=true;
            std::memcpy(item->origin,desired,sizeof(desired));
            std::memcpy(item->axis,desiredAxis,sizeof(desiredAxis));
            objectDiscoveryPublish(*item,now);
        }
    }
    // Do not report a "grabbed" object or open calibration if native
    // physics storage rejected every follow write. A pointer/handle
    // lookup alone does not prove a functional physical attachment.
    if(!held.nativeFollowConfirmed)return;
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
        // Never evict the currently held item's native pointer when
        // scanning a crowded area; Grip ownership survives ray changes.
        item=nullptr;
        for(auto& existing:objectDiscoveryNative){
            if(objectDiscoveryHeld.active
                &&existing.handle==objectDiscoveryHeld.handle
                &&existing.generation==objectDiscoveryHeld.generation)
                continue;
            if(!item||!existing.entity
                ||(item->entity
                    &&existing.observedAtMs<item->observedAtMs))
                item=&existing;
            if(!existing.entity)break;
        }
        if(!item)return;
        *item={};item->entity=entity;item->entityVtable=ev;
        item->handle=handle;item->generation=generation;
        item->imageBase=base;item->imageSize=imageSize;
    }
    item->observedAtMs=now;
    // Locate the manager's native entity-pointer pool using this
    // DOOM-resolved pointer. Falls back to ray-only if not validated.
    objectDiscoveryLocateNativePool(entity,base,imageSize,generation);
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
    // Radius filtering applies before a newly resolved entity enters the
    // persistent discovery list. Grip ownership is independent of this
    // visual scan boundary and keeps the originally pinned body intact.
    if(item->body
        &&(!objectDiscoveryHeld.active
            ||objectDiscoveryHeld.handle!=handle)){
        float xyz[3]{},nativeAxis[9]{},player[3]{},playerAxis[9]{};
        ObjectDiscoveryGripFrame published{};
        {
            std::lock_guard<std::mutex> guard(objectDiscoveryGripMutex);
            published=objectDiscoveryGripInput;
        }
        if(objectDiscoveryReadBodyPose(*item,xyz,nativeAxis)
            &&KharvoxCameraGetBodyPose(player,playerAxis)){
            float d2{};
            for(int i=0;i<3;++i){
                const float delta=xyz[i]-player[i];
                d2+=delta*delta;
            }
            if(d2>published.scanUnits*published.scanUnits){
                *item={};return;
            }
        }
    }
    if(item->body)objectDiscoveryGameTick();
#else
    (void)entity;(void)base;(void)imageSize;(void)handle;(void)generation;
#endif
}
