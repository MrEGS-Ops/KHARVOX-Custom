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
    float origin[3]{},axis[9]{},aim[3]{},rayOrigin[3]{};
    float reachUnits{150.f};
    float scanUnits{400.f};
    unsigned physicalHand{};
    unsigned long long frameAtMs{},releaseSerial{};
    bool enabled{},pressed{},calibrating{},freeze{},offsetValid{},axisValid{};
    bool resetToNeutral{}; // one edge, re-arm only when the chord releases
    bool grappleAnchorEnabled{};
    bool meleeActivationEnabled{};
    bool nativeMeleeInputDown{};
    unsigned poseLockAcknowledged{}; // XR must confirm the NEW lock pose
    float wantedOffset[3]{},wantedAxis[9]{};
};
struct ObjectDiscoveryHeld {
    unsigned handle{},physicalHand{};
    unsigned long long generation{};
    float relativeOffset[3]{},relativeAxis[9]{};
    float frozenWorldOrigin[3]{},frozenWorldAxis[9]{};
    unsigned long long lastGripDownAtMs{};
    unsigned poseReadMisses{},poseLockRevision{};
    // The initial pickup Grip may immediately freeze the neutral object
    // once calibration is active; no extra release/re-press required.
    bool rotationActive{},neutralResetLatched{};
    bool active{},calibrating{},nativeFollowConfirmed{},grappleAnchor{};
};
std::array<ObjectDiscoveryNativeRecord,128> objectDiscoveryNative{};
ObjectDiscoveryHeld objectDiscoveryHeld{};
std::mutex objectDiscoveryGripMutex;
ObjectDiscoveryGripFrame objectDiscoveryGripInput{};
ObjectDiscoveryGripFrame objectDiscoveryLastGoodInput{};
KharvoxObjectGripFeedback objectDiscoveryGripFeedback{};
unsigned objectGripTargetStatus{}; // 0 unknown, 1 clear, 2 eligible or reserved
unsigned long long objectGripTargetAtMs{},objectGripTargetGeneration{};
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
// Single, verified source of Object Discovery candidates: DOOM's
// game-owned entity registry. Legacy focus-based and flat pointer-pool
// enumerators are retired. Hand aim still selects WHICH nearby object
// to catch, but never creates scanner candidates.
// A second proven native lead: DOOM's game-owned 32-byte entity registry.
// Used elsewhere in RIPSHIFT for safely reading pre-spawned map entities.
// NEVER treat it as a proximity pool until its pointer/count, engine
// version signatures, and the FocusTracker-known entity all match.
struct ObjectDiscoveryManagedEntry {
    std::uint32_t id{},check{};
    std::uintptr_t entity{};
    std::uint64_t remaining[2]{};
};
static_assert(sizeof(ObjectDiscoveryManagedEntry)==32);
struct ObjectDiscoveryManagedPool {
    const unsigned char* begin{};
    std::uintptr_t game{},imageBase{},imageSize{};
    unsigned long long generation{},lastAttemptMs{},lastSweepMs{};
    unsigned cursor{},entries{};
};
ObjectDiscoveryManagedPool objectDiscoveryManagedPool{};
std::atomic<unsigned> objectDiscoverySourceMode{}; // 0=offline/unavailable, 2=validated world registry

void objectDiscoverySeenEntity(const void* entity,std::uintptr_t base,
    std::uintptr_t size,unsigned handle,unsigned long long generation);
// Attempt DOOM's already-known game entity registry independently of the
// gun-hand FocusTracker ray. Its exact signature/size and game vtable were
// validated for this retail executable by the cinematic manager. Without a
// FocusTracker seed, require several valid idEntity RTTI records including
// at least one genuine moveable prop before trusting it for enumeration.
void objectDiscoveryTryManagedPool(
    std::uintptr_t imageBase,std::uintptr_t imageSize,
    unsigned long long generation) {
#if KHARVOX_EXPERIMENTAL_WORLD_GRAB
    auto& pool=objectDiscoveryManagedPool;
    if(pool.generation!=generation)pool={};
    const auto now=GetTickCount64();
    if(pool.begin||now<pool.lastAttemptMs
        ||now-pool.lastAttemptMs<5000)return;
    pool.lastAttemptMs=now;pool.generation=generation;
    constexpr unsigned char expectedCode[]{
        0x48,0x8b,0x05,0x2c,0x50,0x7b,0x05};
    unsigned char code[sizeof(expectedCode)]{};
    std::uint64_t signature{};
    // Exact code/data fingerprints from existing read-only game manager
    // inspection. Never enumerate unknown versions on a guessed offset.
    if(!objectDiscoveryReadable(
            reinterpret_cast<const void*>(imageBase+0x3035ad8),8)
       ||!objectDiscoveryReadable(
            reinterpret_cast<const void*>(imageBase+0x300f600),8)
       ||!objectDiscoveryReadable(
            reinterpret_cast<const void*>(imageBase+0x35a69d),sizeof(code)))
        return;
    std::memcpy(&signature,reinterpret_cast<const void*>(
        imageBase+0x3035ad8),8);
    if(signature!=0x20000a54d0ULL)return;
    std::memcpy(&signature,reinterpret_cast<const void*>(
        imageBase+0x300f600),8);
    if(signature!=0x100004e98ULL)return;
    std::memcpy(code,reinterpret_cast<const void*>(
        imageBase+0x35a69d),sizeof(code));
    if(std::memcmp(code,expectedCode,sizeof(code)))return;
    std::uintptr_t game{},gameVtable{},entries{},again{};
    std::int32_t count{},againCount{};
    if(!objectDiscoveryReadable(
            reinterpret_cast<const void*>(imageBase+0x5b0f6d0),8))
        return;
    std::memcpy(&game,reinterpret_cast<const void*>(
        imageBase+0x5b0f6d0),8);
    if(!objectDiscoveryReadable(reinterpret_cast<const void*>(game),8))
        return;
    std::memcpy(&gameVtable,reinterpret_cast<const void*>(game),8);
    if(gameVtable!=imageBase+0x201e3d8
        ||!objectDiscoveryReadable(
            reinterpret_cast<const void*>(game+0xa54d8),16))
        return;
    std::memcpy(&entries,reinterpret_cast<const void*>(game+0xa54d8),8);
    std::memcpy(&count,reinterpret_cast<const void*>(game+0xa54e0),4);
    if(!entries||count<1||count>32768
        ||!objectDiscoveryReadable(reinterpret_cast<const void*>(entries),
            static_cast<std::size_t>(count)*sizeof(ObjectDiscoveryManagedEntry)))
        return;
    // Validate the registry on its own. No FocusTracker sample, hand
    // ray or secondary entity pool is needed to bootstrap.
    unsigned classValid{},movableFound{};
    // The previous ~512 uniformly spaced samples could miss sparse
    // movable entities altogether. Sweep contiguous 2K-entry windows
    // on successive 5-second validations instead of repeatedly
    // sampling the same indices. Still require strict class/registry proof.
    const unsigned sampleCount=std::min<unsigned>(
        static_cast<unsigned>(count),2048u);
    const unsigned firstSample=pool.cursor;
    for(unsigned sample=0;sample<sampleCount;++sample){
        const int index=static_cast<int>(
            (firstSample+sample)%static_cast<unsigned>(count));
        ObjectDiscoveryManagedEntry entry{};
        std::memcpy(&entry,reinterpret_cast<const void*>(
            entries+static_cast<std::size_t>(index)*sizeof(entry)),
            sizeof(entry));
        if(!entry.entity||entry.id!=entry.check
            ||entry.id==0x1fffffe)continue;
        const auto* entity=reinterpret_cast<const void*>(entry.entity);
        if(!objectDiscoveryReadable(entity,sizeof(std::uintptr_t)))
            continue;
        std::uintptr_t entityVtable{};
        std::memcpy(&entityVtable,entity,sizeof(entityVtable));
        if(entityVtable<imageBase
            ||entityVtable-imageBase>=imageSize)continue;
        std::array<char,80> label{};
        if(!readNativeWorldGrabClassLabel(entityVtable,
            imageBase,imageSize,label)
            ||std::strncmp(label.data(),"id",2)!=0)continue;
        ++classValid;
        if(std::strcmp(label.data(),"idProp2")==0
            ||std::strcmp(label.data(),"idProp_Moveable")==0)
            ++movableFound;
        if(classValid>=3&&movableFound>=1)break;
    }
    pool.cursor=(firstSample+sampleCount)
        %static_cast<unsigned>(count);
    std::memcpy(&again,reinterpret_cast<const void*>(game+0xa54d8),8);
    std::memcpy(&againCount,reinterpret_cast<const void*>(game+0xa54e0),4);
    if(classValid<3||movableFound<1
        ||again!=entries||againCount!=count){
        log("[OBJECT-DISCOVERY] proximity validation pending:"
            " scanned="+std::to_string(sampleCount)
            +" first="+std::to_string(firstSample)
            +" registry_entries="+std::to_string(count)
            +" valid_classes="+std::to_string(classValid)
            +" movable_classes="+std::to_string(movableFound)
            +" registry_stable="+std::to_string(
                again==entries&&againCount==count));
        return;
    }
    pool.begin=reinterpret_cast<const unsigned char*>(entries);
    pool.game=game;pool.imageBase=imageBase;pool.imageSize=imageSize;
    pool.entries=static_cast<unsigned>(count);
    pool.cursor=0;pool.lastSweepMs=0;
    objectDiscoverySourceMode.store(2,std::memory_order_release);
    log("[OBJECT-DISCOVERY] validated independent nearby entity registry; "
        "source=PROXIMITY entries="+std::to_string(count));
#else
    (void)imageBase;(void)imageSize;(void)generation;
#endif
}
void objectDiscoverySweepNativePool(unsigned long long now,
    unsigned long long generation){
#if KHARVOX_EXPERIMENTAL_WORLD_GRAB
    // TRUE proximity enumerator: advance through DOOM's validated
    // game-owned entity registry independent of the gun-hand focus ray.
    // The focus ray is used only once to PROVE the table's identity.
    auto& registry=objectDiscoveryManagedPool;
    if(registry.begin&&registry.generation==generation
        &&now>=registry.lastSweepMs
        &&now-registry.lastSweepMs>=175){
        registry.lastSweepMs=now;
        std::uintptr_t entriesNow{};
        std::int32_t countNow{};
        if(!objectDiscoveryReadable(
                reinterpret_cast<const void*>(registry.game+0xa54d8),16)){
            registry={};
        }else{
            std::memcpy(&entriesNow,reinterpret_cast<const void*>(
                registry.game+0xa54d8),8);
            std::memcpy(&countNow,reinterpret_cast<const void*>(
                registry.game+0xa54e0),4);
            if(entriesNow!=reinterpret_cast<std::uintptr_t>(registry.begin)
                ||countNow<1||countNow>32768
                ||!objectDiscoveryReadable(registry.begin,
                    static_cast<std::size_t>(countNow)*
                        sizeof(ObjectDiscoveryManagedEntry))){
                registry={};
            }else{
                registry.entries=static_cast<unsigned>(countNow);
                // Low-cost scan: 48 entries every 175ms instead of
                // 64 every 75ms (~68% fewer registry entry checks).
                // The FULL player-radius discovery remains enabled;
                // only its refresh cadence is reduced.
                constexpr unsigned managedReadsPerSweep=48;
                for(unsigned scanned=0;scanned<managedReadsPerSweep;++scanned){
                    const auto index=registry.cursor++%registry.entries;
                    ObjectDiscoveryManagedEntry slot{};
                    std::memcpy(&slot,registry.begin
                        +static_cast<std::size_t>(index)*sizeof(slot),
                        sizeof(slot));
                    if(!slot.entity||slot.id!=slot.check
                        ||slot.id==0x1fffffe)continue;
                    const auto* entity=reinterpret_cast<const void*>(
                        slot.entity);
                    if(!objectDiscoveryReadable(entity,
                        sizeof(std::uintptr_t)))continue;
                    std::uintptr_t vtable{};
                    std::memcpy(&vtable,entity,sizeof(vtable));
                    if(vtable<registry.imageBase
                        ||vtable-registry.imageBase>=registry.imageSize)
                        continue;
                    std::array<char,80> label{};
                    if(!readNativeWorldGrabClassLabel(vtable,
                        registry.imageBase,registry.imageSize,label))
                        continue;
                    if(std::strcmp(label.data(),"idProp2")!=0
                        &&std::strcmp(label.data(),"idProp_Moveable")!=0)
                        continue;
                    unsigned handle{};
                    for(const auto& seen:objectDiscoveryNative)
                        if(seen.entity==entity
                            &&seen.generation==generation){
                            handle=seen.handle;break;
                        }
                    if(!handle){
                        const auto address=slot.entity;
                        handle=static_cast<unsigned>(
                            (address>>4)^(address>>36));
                        if(!handle)handle=1u;
                    }
                    objectDiscoverySeenEntity(entity,registry.imageBase,
                        registry.imageSize,handle,generation);
                }
            }
        }
        if(!registry.begin)
            objectDiscoverySourceMode.store(0,std::memory_order_release);
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
bool objectDiscoveryReleaseToGravity(ObjectDiscoveryNativeRecord& item,bool wake=true) {
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
        if(wake)reinterpret_cast<Fn>(base+0x179A8F0)(item.body);
#if defined(_MSC_VER)
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
#endif
    return true;
#else
    (void)item;(void)wake;return false;
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
#include "GrappleGeometryInvestigator.inc"

// Melee activation requests ordinary JOY8 once; keep diagnostic logging.
// The ray is an origin proxy, not a collision or LOS query. Native melee
// direction and actual contact remain controlled by DOOM.
struct ObjectMeleeActivation {
    bool gripWasDown{},inputWasDown{},motionLogged{},sawInput{},readyToGrab{};
    unsigned handle{};
    unsigned long long generation{},started{},lastSample{},lastMoved{};
    const void* entity{};
    const void* body{};
    float baseline[3]{};
    bool pickupPoseValid{};
    kharvox::GrappleRayAnchor pickupRay{};
    std::array<float,3> pickupOffset{};
    std::array<float,9> pickupAxis{};
};
ObjectMeleeActivation objectMeleeActivation{};
// Mailbox protected by objectDiscoveryGripMutex; no pointers are published.
unsigned long long objectMeleeRequestAtMs{},objectMeleeRequestGeneration{};
bool objectMeleeNeedsRelease{};
void objectMeleeActivationTick(const ObjectDiscoveryGripFrame& in,
    bool valid,unsigned long long now,unsigned long long generation) {
    auto& p=objectMeleeActivation;
    const bool down=valid&&in.meleeActivationEnabled&&in.physicalHand==0&&in.pressed
        &&!in.calibrating;
    if(valid&&!in.pressed)objectMeleeNeedsRelease=false;
    if(!down||p.generation!=generation){
        {std::lock_guard<std::mutex> lock(objectDiscoveryGripMutex);
         objectMeleeRequestAtMs=0;}
        if(p.gripWasDown)log("[GRAPPLE-MELEE] end handle="+std::to_string(p.handle)
            +" elapsed_ms="+std::to_string(now-p.started)
            +" reason=release_or_context_change");
        p={};
        if(!down)return;
    }
    const auto vec=[](const float* v){return std::to_string(v[0])+","+
        std::to_string(v[1])+","+std::to_string(v[2]);};
    if(!p.gripWasDown){
        p.gripWasDown=true;p.generation=generation;
        p.started=now;
        if(objectMeleeNeedsRelease){log("[GRAPPLE-MELEE] release Grip to rearm");return;}
        objectMeleeNeedsRelease=true;
        if(objectDiscoveryHeld.active){
            log("[GRAPPLE-MELEE] skipped: already holding object");return;
        }
        ObjectDiscoveryNativeRecord* best=nullptr;
        float bestMiss=kharvox::kGrappleProxyMissUnits,bestDepth=in.reachUnits;
        unsigned candidates=0;
        for(auto& item:objectDiscoveryNative){
            if(!item.entity||!item.body||!item.poseValid||item.generation!=generation
                ||now<item.lastPositionAtMs||now-item.lastPositionAtMs>250)continue;
            kharvox::GrappleRayAnchor ray{};
            if(!kharvox::grappleRayEstimate(in.rayOrigin,in.aim,item.origin,
                    in.reachUnits,kharvox::kGrappleProxyMissUnits,ray))continue;
            ++candidates;
            if(!best||ray.missDistance<bestMiss
                ||(ray.missDistance==bestMiss&&ray.rayDistance<bestDepth)){
                best=&item;bestMiss=ray.missDistance;bestDepth=ray.rayDistance;
                p.pickupRay=ray;
            }
        }
        log("[GRAPPLE-MELEE] begin gen="+std::to_string(generation)+" origin="+vec(in.rayOrigin)
            +" aim="+vec(in.aim)+" reach_units="+std::to_string(in.reachUnits)
            +" proxy_candidates="+std::to_string(candidates)
            +" contact=UNVERIFIED LOS=UNVERIFIED dispatch=NATIVE_JOY8");
        if(!best){log("[GRAPPLE-MELEE] no fresh rigid-body proxy in reach");return;}
        if(in.grappleAnchorEnabled){
            p.pickupPoseValid=kharvox::grapplePickupPoseFromHit(in.origin,in.axis,
                best->origin,best->axis,p.pickupRay,p.pickupOffset,p.pickupAxis)
                &&objectDiscoveryAxisOk(p.pickupAxis.data());
            if(!p.pickupPoseValid){log("[GRAPPLE-MELEE] invalid pre-impact anchor; no pulse");return;}
        }
        p.handle=best->handle;p.entity=best->entity;p.body=best->body;
        p.lastMoved=best->nativeMovedAtMs;
        std::memcpy(p.baseline,best->origin,sizeof(p.baseline));
        log("[GRAPPLE-MELEE] selected handle="+std::to_string(p.handle)
            +" model="+std::string(best->modelKey.data())
            +" pos="+vec(best->origin)+" ray_depth="+std::to_string(bestDepth)
            +" miss="+std::to_string(bestMiss)
            +" prior_motion="+std::to_string(best->nativeMotionObserved));
        {
            if(best->nativeMotionObserved&&best->nativeMovedAtMs
                &&now>=best->nativeMovedAtMs&&now-best->nativeMovedAtMs<=3000){
                p.readyToGrab=true;
                log("[GRAPPLE-MELEE] target already moving; no activation pulse needed");
            }else{
                std::lock_guard<std::mutex> lock(objectDiscoveryGripMutex);
                objectMeleeRequestAtMs=now;
                objectMeleeRequestGeneration=generation;
                log("[GRAPPLE-MELEE] queued ordinary native melee for proxy handle="
                    +std::to_string(p.handle)+" (left AIM pose requested; actual hit direction unverified)");
            }
        }
    }
    if(in.nativeMeleeInputDown)p.sawInput=true;
    if(in.nativeMeleeInputDown&&!p.inputWasDown)
        log("[GRAPPLE-MELEE] native melee INPUT requested elapsed_ms="
            +std::to_string(now-p.started)+" handle="+std::to_string(p.handle)
            +" (not contact proof)");
    p.inputWasDown=in.nativeMeleeInputDown;
    if(!p.handle||p.motionLogged)return;
    if(!p.readyToGrab&&now-p.started>2000){
        log("[GRAPPLE-MELEE] timeout: no confirmed activation of selected target");
        p.motionLogged=true;return;
    }
    for(const auto& item:objectDiscoveryNative){
        if(item.handle!=p.handle||item.entity!=p.entity||item.body!=p.body
            ||item.generation!=p.generation||!item.poseValid)continue;
        if(objectDiscoveryHeld.active&&objectDiscoveryHeld.handle==p.handle){
            log("[GRAPPLE-MELEE] tracking ends: existing grab acquired target");
            p.motionLogged=true;return;
        }
        if(item.nativeMovedAtMs>p.lastMoved&&item.nativeMovedAtMs>=p.started){
            if(!p.sawInput)return; // wait for actual input observation
            p.readyToGrab=true;
            log("[GRAPPLE-MELEE] SAME TARGET native movement observed elapsed_ms="
                +std::to_string(now-p.started)+" handle="+std::to_string(p.handle)
                +" before="+vec(p.baseline)+" after="+vec(item.origin)
                +" (causality unverified)");
            p.motionLogged=true;
        }else if(now>=p.lastSample+500){
            p.lastSample=now;
            log("[GRAPPLE-MELEE] waiting handle="+std::to_string(p.handle)
                +" pos="+vec(item.origin)+" pose_age_ms="
                +std::to_string(now-item.lastPositionAtMs));
        }
        return;
    }
    log("[GRAPPLE-MELEE] target identity lost; no retarget");
    p.motionLogged=true;
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
        if(objectDiscoveryHeld.active)
            objectDiscoveryReleaseNative();
        // Scan independently of controller pointing/hand availability.
        // Invalid free-hand aim must prevent a GRAB, never a 360-degree
        // radius scan; the latter only needs game-world/body coordinates.
        if(worldBlocked||explicitRelease){
            objectMeleeActivationTick(input,false,now,0);return;
        }
    }
    if(!input.pressed)objectDiscoveryNeedsGripRelease=false;
    const auto generation=levelTransitionGeneration.load(std::memory_order_acquire);
    if(!objectDiscoveryEnabled.load(std::memory_order_acquire)){
        objectMeleeActivationTick(input,false,now,generation);
        objectDiscoveryNative={};
        objectDiscoveryReleaseNative();
        objectDiscoverySourceMode.store(0,std::memory_order_release);
        return;
    }
    if(objectDiscoveryManagedPool.generation!=generation)
        objectDiscoveryManagedPool={};
    // Independently bootstrap the world-owned entity registry on the DOOM
    // thread. The gun-hand ray is NOT required to start scanning. Native
    // pointer reads remain fail-closed against this executable's PE headers
    // and the validated registry signatures.
    if(!objectDiscoveryManagedPool.begin){
        const auto* image=reinterpret_cast<const unsigned char*>(
            GetModuleHandleW(nullptr));
        if(image&&objectDiscoveryReadable(image,sizeof(IMAGE_DOS_HEADER))){
            const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
            if(dos->e_magic==IMAGE_DOS_SIGNATURE
                &&dos->e_lfanew>0&&dos->e_lfanew<0x4000
                &&objectDiscoveryReadable(image+dos->e_lfanew,
                    sizeof(IMAGE_NT_HEADERS64))){
                const auto* pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(
                    image+dos->e_lfanew);
                if(pe->Signature==IMAGE_NT_SIGNATURE
                    &&pe->OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR64_MAGIC){
                    objectDiscoveryTryManagedPool(
                        reinterpret_cast<std::uintptr_t>(image),
                        pe->OptionalHeader.SizeOfImage,generation);
                }
            }
        }
    }
    objectDiscoverySourceMode.store(
        objectDiscoveryManagedPool.begin?2u:0u,
        std::memory_order_release);
    if(!objectDiscoveryManagedPool.begin){
        static unsigned long long lastNoProximityLogAtMs{};
        if(now>=lastNoProximityLogAtMs
            &&now-lastNoProximityLogAtMs>=10000){
            lastNoProximityLogAtMs=now;
            log("[OBJECT-DISCOVERY] radius source UNAVAILABLE: "
                "world registry not validated; NO gun/focus scanner fallback");
        }
    }
    objectDiscoverySweepNativePool(now,generation);
    // Conservative *background* physics budget. Held objects bypass ALL
    // of these throttles: keep the proven 12ms tick / hand follow intact.
    // Round-robin cursor prevents distant candidates starving near ones.
    static unsigned readCursor{};
    constexpr unsigned readsPerTick=12;
    unsigned reads{};
    const unsigned cacheCount=static_cast<unsigned>(objectDiscoveryNative.size());
    for(unsigned checked=0;checked<cacheCount;++checked){
        auto& item=objectDiscoveryNative[(readCursor+checked)%cacheCount];
        if(!item.entity)continue;
        if(item.generation!=generation){item={};continue;}
        const bool heldHere=objectDiscoveryHeld.active
            &&objectDiscoveryHeld.handle==item.handle;
        // A selected melee target bypasses background read throttles until
        // acquisition or timeout, even if the impulse moves it off the ray.
        const auto& pending=objectMeleeActivation;
        const bool pendingHere=inputValid&&input.pressed&&input.meleeActivationEnabled
            &&pending.handle==item.handle&&pending.entity==item.entity
            &&pending.body==item.body&&pending.generation==generation
            &&now>=pending.started&&now-pending.started<=2000;
        const bool priority=heldHere||pendingHere;
        unsigned long long readIntervalMs=80;
        if(!heldHere&&item.poseValid&&inputValid){
            float d2{};
            for(int axis=0;axis<3;++axis){
                const float d=item.origin[axis]-input.origin[axis];
                d2+=d*d;
            }
            if(d2>input.reachUnits*input.reachUnits*4.f)
                readIntervalMs=250;
        }
        if(!priority&&now>=item.lastPositionAtMs
            &&now-item.lastPositionAtMs<readIntervalMs)continue;
        if(!priority&&reads>=readsPerTick)continue;
        if(!priority)++reads;
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
    // Publish a current target check before any melee request or acquisition.
    // Includes stationary props so a failed wake cannot fall through to BFG.
    unsigned gripTarget=0;
    if(inputValid&&!worldBlocked&&!objectDiscoveryNeedsGripRelease
        &&objectDiscoveryManagedPool.begin&&input.physicalHand==0&&input.pressed){
        gripTarget=1;
        if(objectDiscoveryHeld.active)gripTarget=2;
        for(const auto& item:objectDiscoveryNative){
            if(gripTarget==2)break;
            if(!item.entity||!item.body||!item.poseValid||item.generation!=generation
                ||now<item.lastPositionAtMs||now-item.lastPositionAtMs>250)continue;
            if(input.grappleAnchorEnabled||input.meleeActivationEnabled){
                kharvox::GrappleRayAnchor ray{};
                if(kharvox::grappleRayEstimate(input.rayOrigin,input.aim,item.origin,
                    input.reachUnits,kharvox::kGrappleProxyMissUnits,ray))gripTarget=2;
            }else{
                float d2{},forward{};
                for(int k=0;k<3;++k){
                    const float d=item.origin[k]-input.origin[k];
                    d2+=d*d;forward+=d*input.aim[k];
                }
                if(d2<input.reachUnits*input.reachUnits&&forward>0.f
                    &&forward*forward>=.64f*d2)gripTarget=2;
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(objectDiscoveryGripMutex);
        // Preserve any positive match for this uninterrupted press; the XR
        // input dispatcher may not observe every native tick.
        if(gripTarget==1&&objectGripTargetStatus==2&&objectGripTargetGeneration==generation
            &&objectGripTargetAtMs&&now>=objectGripTargetAtMs&&now-objectGripTargetAtMs<200)
            gripTarget=2;
        objectGripTargetStatus=gripTarget;
        objectGripTargetAtMs=now;objectGripTargetGeneration=generation;
    }
    grappleGeometryInvestigatorTick(input,inputValid&&!worldBlocked&&!objectDiscoveryNeedsGripRelease,now,generation);
    objectMeleeActivationTick(input,inputValid&&!worldBlocked&&!objectDiscoveryNeedsGripRelease,now,generation);
    if(!inputValid)return;
    if(!objectDiscoveryHeld.active){
        if(!input.pressed||objectDiscoveryNeedsGripRelease)return;
        ObjectDiscoveryNativeRecord* nearest{};
        float best2=input.grappleAnchorEnabled
            ?kharvox::kGrappleProxyMissUnits*kharvox::kGrappleProxyMissUnits:input.reachUnits*input.reachUnits;
        for(auto& item:objectDiscoveryNative){
            if(input.meleeActivationEnabled&&input.physicalHand==0
                &&(!objectMeleeActivation.readyToGrab||item.handle!=objectMeleeActivation.handle
                    ||item.entity!=objectMeleeActivation.entity||item.body!=objectMeleeActivation.body))continue;
            if(!item.entity||!item.poseValid||!item.nativeMotionObserved
                ||!item.nativeMovedAtMs||now-item.nativeMovedAtMs>3000
                ||now-item.lastPositionAtMs>250||item.generation!=generation)
                continue;
            // Target eligibility was established before the native melee.
            // Do not require a second ray hit after the impulse. Identity,
            // generation, fresh native movement and the grip are still gated.
            if(input.meleeActivationEnabled&&input.physicalHand==0){
                nearest=&item;break;
            }
            float squared{},forward{};
            for(int axis=0;axis<3;++axis){
                const float delta=item.origin[axis]-input.origin[axis];
                squared+=delta*delta;
                forward+=delta*input.aim[axis];
            }
            if(input.grappleAnchorEnabled){
                kharvox::GrappleRayAnchor ray{};
                // The current broad-phase uses DOOM body origins only.
                // No shape-specific collision mesh/radius is inferred.
                if(!kharvox::grappleRayEstimate(input.rayOrigin,input.aim,
                        item.origin,input.reachUnits,kharvox::kGrappleProxyMissUnits,ray))continue;
                const float lateral2=ray.missDistance*ray.missDistance;
                if(lateral2>=best2)continue;
                best2=lateral2;nearest=&item;
            }else{
                if(squared>=best2||forward<=0.f
                    ||forward*forward<.64f*squared)continue;
                best2=squared;nearest=&item;
            }
        }
        if(!nearest){
            if(input.grappleAnchorEnabled){
                static unsigned long long lastGrappleMiss{};
                if(now>=lastGrappleMiss+1200){
                    lastGrappleMiss=now;
                    log("[GRAPPLE-ANCHOR] Grip ray had NO valid moving "
                        "object within reach and tightened 25-unit ray tolerance");
                }
            }
            return;
        }
        grappleGeometryInvestigatorAcquired(input,*nearest,now);
        auto& held=objectDiscoveryHeld;
        held={};held.active=true;held.handle=nearest->handle;
        held.physicalHand=input.physicalHand;
        held.generation=generation;
        // Capture an invariant 6DOF controller-relative target on GRAB,
        // not the body's current position. Otherwise an object one metre
        // away inherits its pickup offset for several frames, making
        // neutral calibration appear to follow the old barrel location.
        for(int local=0;local<3;++local){
            held.relativeOffset[local]=
                kharvox::kObjectGripNeutralOffsetUnits[local];
        }
        // Orientation is ALSO expressed in the hand frame, not WORLD
        // identity. The model's local up maps to the gun-aim hand's up.
        const auto neutral=kharvox::objectGripNeutralLocalAxis();
        if(!objectDiscoveryAxisOk(neutral.data())){
            held={};return;
        }
        std::copy(neutral.begin(),neutral.end(),held.relativeAxis);
        // Grapple mode changes the pickup anchor, not native eligibility or release.
        if(input.grappleAnchorEnabled){
            std::array<float,3> offset{};
            std::array<float,9> axis{};
            kharvox::GrappleRayAnchor ray{};
            const bool useLatched=input.meleeActivationEnabled&&input.physicalHand==0
                &&objectMeleeActivation.handle==nearest->handle;
            bool poseValid=false;
            if(useLatched){
                const auto& p=objectMeleeActivation;
                offset=p.pickupOffset;axis=p.pickupAxis;ray=p.pickupRay;
                poseValid=p.pickupPoseValid;
            }else poseValid=kharvox::grapplePickupPose(input.origin,input.axis,
                    input.rayOrigin,input.aim,nearest->origin,nearest->axis,
                    input.reachUnits,offset,axis,ray);
            if(!poseValid||!objectDiscoveryAxisOk(axis.data())){
                held={};return;
            }
            std::copy(offset.begin(),offset.end(),held.relativeOffset);
            std::copy(axis.begin(),axis.end(),held.relativeAxis);
            held.grappleAnchor=true;
            grappleGeometryInvestigatorContact(input,*nearest,ray,offset,axis);
            log("[GRAPPLE-ANCHOR] grabbed handle="
                +std::to_string(held.handle)
                +" contact=ESTIMATED_RAY_PROXY (NOT_RENDER_MESH)"
                +" ray_depth="+std::to_string(ray.rayDistance)
                +" miss="+std::to_string(ray.missDistance)
                +" world_point="+std::to_string(ray.worldPoint[0])
                +","+std::to_string(ray.worldPoint[1])
                +","+std::to_string(ray.worldPoint[2])
                +" locked 6DOF offset="
                +std::to_string(offset[0])+","
                +std::to_string(offset[1])+","
                +std::to_string(offset[2]));
        }
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
    if(!held.grappleAnchor&&input.resetToNeutral&&!held.neutralResetLatched){
        const auto neutral=kharvox::objectGripNeutralLocalAxis();
        if(objectDiscoveryAxisOk(neutral.data())){
            std::copy(neutral.begin(),neutral.end(),held.relativeAxis);
            for(int i=0;i<3;++i){
                held.relativeOffset[i]=kharvox::kObjectGripNeutralOffsetUnits[i];
            }
            // A neutral reset must not continue editing a stale
            // world-space freeze target from before the reset.
            held.rotationActive=false;
        }
    }
    held.neutralResetLatched=input.resetToNeutral;
    // Even the FIRST acquisition Grip can leave the freshly neutralized
    // prop floating while the player walks freely around it.
    bool releasedFrozenPoseThisTick=false;
    if(!held.grappleAnchor&&held.calibrating&&input.freeze){
        if(!held.rotationActive){
            // Freeze the complete CURRENT TARGET pose in world space,
            // NOT the lagging DOOM physics pose. The object keeps its
            // position and orientation while the user's hand moves.
            kharvox::objectGripToWorldPose(input.origin,input.axis,
                held.relativeOffset,held.relativeAxis,
                held.frozenWorldOrigin,held.frozenWorldAxis);
            held.rotationActive=true;
        }
        // Every tick: worldFrozen = handNow * localNow. The local pose is
        // what changes while the player rotates their controller.
        float nextOffset[3]{},nextAxis[9]{};
        kharvox::objectGripToHandLocalPose(input.origin,input.axis,
            held.frozenWorldOrigin,held.frozenWorldAxis,
            nextOffset,nextAxis);
        if(objectDiscoveryAxisOk(nextAxis)){
            std::copy(std::begin(nextOffset),std::end(nextOffset),
                held.relativeOffset);
            std::copy(std::begin(nextAxis),std::end(nextAxis),
                held.relativeAxis);
        }
    }else if(held.rotationActive){
        // Grip just released. Re-project once using the RELEASE frame's
        // controller pose so the object does not jump as it switches
        // from world-space freeze back to normal hand-local attachment.
        float nextOffset[3]{},nextAxis[9]{};
        kharvox::objectGripToHandLocalPose(input.origin,input.axis,
            held.frozenWorldOrigin,held.frozenWorldAxis,
            nextOffset,nextAxis);
        if(objectDiscoveryAxisOk(nextAxis)){
            std::copy(std::begin(nextOffset),std::end(nextOffset),
                held.relativeOffset);
            std::copy(std::begin(nextAxis),std::end(nextAxis),
                held.relativeAxis);
        }
        held.rotationActive=false;
        ++held.poseLockRevision;
        releasedFrozenPoseThisTick=true;
    }
    {
        // XR feedback is one world tick behind. Never overwrite the
        // world-locked pose with the previous frame's offset while Grip
        // is pressed or on the exact frame it is released.
        if(!held.grappleAnchor&&input.calibrating&&input.offsetValid
            &&!held.rotationActive&&!releasedFrozenPoseThisTick
            &&input.poseLockAcknowledged==held.poseLockRevision){
            for(int i=0;i<3;++i)held.relativeOffset[i]=input.wantedOffset[i];
        }else if(!held.grappleAnchor&&!input.calibrating&&input.offsetValid){
            for(int i=0;i<3;++i)held.relativeOffset[i]=input.wantedOffset[i];
            if(input.axisValid&&objectDiscoveryAxisOk(input.wantedAxis))
                std::memcpy(held.relativeAxis,input.wantedAxis,
                    sizeof(held.relativeAxis));
        }else if(!input.calibrating){
            // Normal hold never eases back toward a pickup-time body pose.
            // Keep the same hand-local position and orientation regardless
            // of which direction the player moves or points.
            // (If there is no profile yet, this is the neutral offset.)
        }
        float desired[3]{},desiredAxis[9]{};
        kharvox::objectGripToWorldPose(input.origin,input.axis,
            held.relativeOffset,held.relativeAxis,
            desired,desiredAxis);
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
            // Read back native pose instead of publishing the commanded
            // position as if it were a verified visible mesh transform.
            float actual[3]{},actualAxis[9]{};
            if(objectDiscoveryReadBodyPose(*item,actual,actualAxis)){
                float error2{};for(int i=0;i<3;++i){const float e=actual[i]-desired[i];error2+=e*e;}
                if(!held.nativeFollowConfirmed&&input.meleeActivationEnabled
                    &&held.handle==objectMeleeActivation.handle){
                    const bool stopped=objectDiscoveryReleaseToGravity(*item,false);
                    log("[GRAPPLE-MELEE] catch write readback handle="+std::to_string(held.handle)
                        +" elapsed_ms="+std::to_string(now-objectMeleeActivation.started)
                        +" pose_error_units="+std::to_string(std::sqrt(error2))
                        +" momentum_clear="+std::to_string(stopped)+" render_mesh=UNVERIFIED");
                }
                if(std::isfinite(error2)&&error2<1.f)held.nativeFollowConfirmed=true;
                std::memcpy(item->origin,actual,sizeof(actual));
                std::memcpy(item->axis,actualAxis,sizeof(actualAxis));
                // Discovery markers use actual sampled native physics pose,
                // not the synthetic desired hand target.
                objectDiscoveryPublish(*item,now);
            }
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
        f.rotationEditActive=held.rotationActive;
        f.poseLockRevision=held.poseLockRevision;
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
    if(!item->body&&now>=item->lastProbeAtMs
        &&now-item->lastProbeAtMs>=800){
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
    // The normal DOOM world callback ticks discovery every 12ms. Avoid
    // re-entering that tick recursively from within the 64-entry proximity
    // sweep; it can otherwise lose the held object or overflow the stack.
#else
    (void)entity;(void)base;(void)imageSize;(void)handle;(void)generation;
#endif
}
