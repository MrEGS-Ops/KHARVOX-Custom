// Passive world-thread observer. No new hooks, calls into candidate code or writes.
// The wake field is a signature-derived hypothesis, not a verified active flag.
#if KHARVOX_EXPERIMENTAL_WORLD_GRAB
struct ActivationInvestigation {
    unsigned handle{};
    const void* entity{};
    void* body{};
    unsigned long long generation{},armed{},started{},lastAim{},lastSample{},lastSummary{},serial{};
    bool meleeWasDown{},signatureOk{},tracing{},motionSeen{},wakeSeen{};
    std::array<unsigned char,0x300> baseline{},previous{};
    std::array<unsigned char,0x300> entityBaseline{},entityPrevious{};
    float priorOrigin[3]{},startOrigin[3]{};
    unsigned samples{},wakeCallsReported{},callbackCallsReported{},callbackCallsBaseline{};
};
ActivationInvestigation activationInvestigation{};
// Observe the exact signature-validated rigid-body wake entry. The
// trampoline preserves native behavior. Only the armed body's calls count.
using ActivationWakeFn=void (__fastcall*)(void*);
ActivationWakeFn activationWakeOriginal{};
std::atomic<std::uintptr_t> activationWakeTarget{},activationWakeCaller{};
std::atomic<unsigned> activationWakeCalls{},activationWakeThread{};
std::atomic<unsigned long long> activationWakeAt{},activationWakeSequence{};
bool activationWakeHookInstalled{},activationWakeHookAttempted{};
void __fastcall activationWakeObserver(void* body){
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    activationWakeOriginal(body);
    if(reinterpret_cast<std::uintptr_t>(body)!=activationWakeTarget.load(std::memory_order_acquire))return;
    activationWakeCaller.store(caller,std::memory_order_relaxed);
    activationWakeAt.store(GetTickCount64(),std::memory_order_relaxed);
    activationWakeThread.store(GetCurrentThreadId(),std::memory_order_relaxed);
    activationWakeSequence.store(nativeInvestigationSequence.fetch_add(1,std::memory_order_relaxed)+1,std::memory_order_relaxed);
    activationWakeCalls.fetch_add(1,std::memory_order_release);
}
bool installActivationWakeObserver(const ObjectDiscoveryNativeRecord& item){
    if(activationWakeHookAttempted)return activationWakeHookInstalled;
    activationWakeHookAttempted=true;
    auto target=reinterpret_cast<void*>(item.imageBase+0x179A8F0);
    const auto init=MH_Initialize();
    if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)return false;
    void* original{};
    if(MH_CreateHook(target,reinterpret_cast<void*>(activationWakeObserver),&original)!=MH_OK)return false;
    activationWakeOriginal=reinterpret_cast<ActivationWakeFn>(original);
    // The first native instruction is a 10-byte store. Verify the trampoline
    // contains that instruction before enabling; other signature checks stay intact.
    unsigned char bytes[10]{};
    constexpr unsigned char expected[]{0xC7,0x81,0x70,0x01,0x00,0x00,0xFF,0xFF,0xFF,0xFF};
    if(!geometryRead(reinterpret_cast<std::uintptr_t>(original),bytes,10)||std::memcmp(bytes,expected,10)){
        MH_RemoveHook(target);activationWakeOriginal=nullptr;return false;
    }
    if(MH_EnableHook(target)!=MH_OK){MH_RemoveHook(target);activationWakeOriginal=nullptr;return false;}
    activationWakeHookInstalled=true;
    return true;
}
// Observe the identified callback entry without assuming its argument or return
// signature. The thunk preserves flags, volatile GPRs, XMM0-5 and stack arguments,
// then tail-jumps to MinHook's relocated original. No candidate is called by us.
std::atomic<std::uintptr_t> activationCallbackTarget{},activationCallbackCaller{};
std::atomic<unsigned> activationCallbackCalls{},activationCallbackThread{};
std::atomic<unsigned long long> activationCallbackAt{},activationCallbackSequence{};
bool activationCallbackInstalled{},activationCallbackAttempted{};
void __fastcall activationCallbackObserve(void* self,void* caller){
    if(reinterpret_cast<std::uintptr_t>(self)!=activationCallbackTarget.load(std::memory_order_acquire))return;
    activationCallbackCaller.store(reinterpret_cast<std::uintptr_t>(caller),std::memory_order_relaxed);
    activationCallbackAt.store(GetTickCount64(),std::memory_order_relaxed);
    activationCallbackThread.store(GetCurrentThreadId(),std::memory_order_relaxed);
    activationCallbackSequence.store(nativeInvestigationSequence.fetch_add(1,std::memory_order_relaxed)+1,std::memory_order_relaxed);
    activationCallbackCalls.fetch_add(1,std::memory_order_release);
}
bool installActivationCallbackObserver(const ObjectDiscoveryNativeRecord& item,std::uintptr_t fn){
    // The latest log identified this precise RTTI-checked virtual slot. Do
    // not hook a different executable layout or a mid-function address.
    if(fn!=item.imageBase+0x817c20||!executableMemory(reinterpret_cast<void*>(fn)))return false;
    if(activationCallbackAttempted)return activationCallbackInstalled;
    activationCallbackAttempted=true;
    DWORD64 base{};
    const auto entry=RtlLookupFunctionEntry(static_cast<DWORD64>(fn),&base,nullptr);
    if(!entry||base!=item.imageBase||base+entry->BeginAddress!=fn||entry->EndAddress<=entry->BeginAddress)return false;
    const auto init=MH_Initialize();
    if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)return false;
    auto* stub=static_cast<unsigned char*>(VirtualAlloc(nullptr,256,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!stub)return false;
    void* original{};
    if(MH_CreateHook(reinterpret_cast<void*>(fn),stub,&original)!=MH_OK){VirtualFree(stub,0,MEM_RELEASE);return false;}
    auto p=stub;
    // At entry RSP is 8 mod 16. Eight pushes and 0x88 reservation
    // align RSP for the helper and reserve shadow space plus six XMM slots.
    const unsigned char saveGpr[]{0x9c,0x50,0x51,0x52,0x41,0x50,0x41,0x51,0x41,0x52,0x41,0x53,
        0x48,0x81,0xec,0x88,0x00,0x00,0x00};
    std::memcpy(p,saveGpr,sizeof(saveGpr));p+=sizeof(saveGpr);
    for(unsigned i=0;i<6;++i){emit8(p,0xf3);emit8(p,0x0f);emit8(p,0x7f);emit8(p,static_cast<unsigned char>(0x44+i*8));emit8(p,0x24);emit8(p,static_cast<unsigned char>(0x20+i*16));}
    // Preserve RCX=self and pass the actual native return address in RDX.
    const unsigned char loadCaller[]{0x48,0x8b,0x94,0x24,0xc8,0x00,0x00,0x00};
    std::memcpy(p,loadCaller,sizeof(loadCaller));p+=sizeof(loadCaller);
    emit8(p,0x48);emit8(p,0xb8);emit64(p,reinterpret_cast<unsigned long long>(activationCallbackObserve));
    emit8(p,0xff);emit8(p,0xd0);
    for(unsigned i=0;i<6;++i){emit8(p,0xf3);emit8(p,0x0f);emit8(p,0x6f);emit8(p,static_cast<unsigned char>(0x44+i*8));emit8(p,0x24);emit8(p,static_cast<unsigned char>(0x20+i*16));}
    const unsigned char restore[]{0x48,0x81,0xc4,0x88,0x00,0x00,0x00,0x41,0x5b,0x41,0x5a,0x41,0x59,0x41,0x58,0x5a,0x59,0x58,0x9d,
        0xff,0x25,0x00,0x00,0x00,0x00};
    std::memcpy(p,restore,sizeof(restore));p+=sizeof(restore);
    emit64(p,reinterpret_cast<unsigned long long>(original));
    DWORD prior{};
    if(!VirtualProtect(stub,256,PAGE_EXECUTE_READ,&prior)||!FlushInstructionCache(GetCurrentProcess(),stub,p-stub)){
        MH_RemoveHook(reinterpret_cast<void*>(fn));VirtualFree(stub,0,MEM_RELEASE);return false;
    }
    if(MH_EnableHook(reinterpret_cast<void*>(fn))!=MH_OK){
        MH_RemoveHook(reinterpret_cast<void*>(fn));VirtualFree(stub,0,MEM_RELEASE);return false;
    }
    activationCallbackInstalled=true;return true;
}
unsigned long long activationInvestigationSerial{};
void activationLog(const std::string& msg){
    log("[ACTIVATION-INV] id="+std::to_string(activationInvestigation.serial)+" "+msg);
}
void objectActivationInvestigatorReset(const char* reason){
    if(activationInvestigation.handle)
        activationLog(std::string("end reason=")+reason+" samples="+std::to_string(activationInvestigation.samples)
            +" motion_seen="+std::to_string(activationInvestigation.motionSeen));
    activationWakeTarget.store(0,std::memory_order_release);
    activationCallbackTarget.store(0,std::memory_order_release);
    activationInvestigation={};
}
bool objectActivationInvestigatorPriority(const ObjectDiscoveryNativeRecord& item,
    unsigned long long generation,bool enabled){
    const auto& p=activationInvestigation;
    return enabled&&p.handle&&p.handle==item.handle&&p.entity==item.entity
        &&p.body==item.body&&p.generation==generation;
}
bool activationSignature(const ObjectDiscoveryNativeRecord& item){
    constexpr unsigned char wake[]{0xC7,0x81,0x70,0x01,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xC3};
    constexpr unsigned char linear[]{0xF3,0x0F,0x10,0x87,0xEC,0x01,0x00,0x00,
        0xF3,0x0F,0x58,0x06,0xF3,0x0F,0x11,0x87,0xEC,0x01,0x00,0x00};
    constexpr unsigned char angular[]{0xF3,0x0F,0x11,0xA7,0xF8,0x01,0x00,0x00,
        0xF3,0x0F,0x58,0x87,0xFC,0x01,0x00,0x00};
    const auto matches=[&](std::uintptr_t rva,const unsigned char* sig,std::size_t n){
        unsigned char bytes[32]{};
        return n<=sizeof(bytes)&&rva<item.imageSize&&n<=item.imageSize-rva
            &&geometryRead(item.imageBase+rva,bytes,n)&&std::memcmp(bytes,sig,n)==0;
    };
    unsigned char originalWake[10]{};
    const bool wakeMatch=activationWakeHookInstalled
        ?geometryRead(reinterpret_cast<std::uintptr_t>(activationWakeOriginal),originalWake,10)&&std::memcmp(originalWake,wake,10)==0
        :matches(0x179A8F0,wake,sizeof(wake));
    return wakeMatch&&matches(0x179A958,linear,sizeof(linear))
        &&matches(0x179AA70,angular,sizeof(angular));
}
bool activationRead(const ObjectDiscoveryNativeRecord& item,
    std::array<unsigned char,0x300>& body,std::array<unsigned char,0x300>& entity){
    const auto ep=reinterpret_cast<std::uintptr_t>(item.entity);
    const auto bp=reinterpret_cast<std::uintptr_t>(item.body);
    std::uintptr_t ev{},bv{};
    if(!geometryRead(ep,&ev,sizeof(ev))||!geometryRead(bp,&bv,sizeof(bv))
        ||ev!=item.entityVtable||bv!=item.bodyVtable)return false;
    return geometryRead(bp,body.data(),body.size())&&geometryRead(ep,entity.data(),entity.size());
}
unsigned activationWord(const std::array<unsigned char,0x300>& bytes,unsigned off){
    unsigned word{};std::memcpy(&word,bytes.data()+off,sizeof(word));return word;
}
void activationDiff(const char* type,const std::array<unsigned char,0x300>& before,
    const std::array<unsigned char,0x300>& after,unsigned long long elapsed){
    std::string line;unsigned changes{},logged{};
    for(unsigned off=0;off+4<=after.size();off+=4){
        const unsigned a=activationWord(before,off),b=activationWord(after,off);
        if(a==b)continue;
        ++changes;
        if(logged++>=64)continue;
        line+=" "+geometryHex(off)+":"+geometryHex(a)+">"+geometryHex(b)+
            "/xor="+geometryHex(a^b);
    }
    if(changes)activationLog(std::string("delta ")+type+" t_ms="+std::to_string(elapsed)
        +" changed="+std::to_string(changes)+line);
}
// Field names are reflected metadata, not a claim about solver units or vector halves.
// Exact rigid-body RTTI vtable is required; no callbacks are invoked.
std::string activationReflectedState(const ObjectDiscoveryNativeRecord& item,
    const std::array<unsigned char,0x300>& bytes){
    if(item.bodyVtable!=item.imageBase+0x2874190||!activationSignature(item))
        return " reflected_layout_unavailable=1";
    float spatial[6]{},force[6]{},push[6]{},solverOrigin[3]{},solverAxis[9]{},gravity[3]{};
    std::int32_t rest{};std::uint32_t physicsId{};std::uintptr_t callback{},vt{},fn{};
    std::memcpy(&rest,bytes.data()+0x170,4);
    std::memcpy(spatial,bytes.data()+0x1d4,24);std::memcpy(force,bytes.data()+0x1ec,24);
    std::memcpy(push,bytes.data()+0x204,24);
    std::memcpy(solverOrigin,bytes.data()+0x1a4,12);std::memcpy(solverAxis,bytes.data()+0x1b0,36);
    std::memcpy(gravity,bytes.data()+0x44,12);std::memcpy(&physicsId,bytes.data()+0xc,4);
    std::memcpy(&callback,bytes.data()+0x28,8);
    const bool callbackOk=geometryRead(callback,&vt,8)&&geometryInImage(item,vt)
        &&geometryRead(vt+0x48,&fn,8)&&geometryInImage(item,fn)
        &&executableMemory(reinterpret_cast<const void*>(fn));
    return " at_rest="+std::to_string(rest)+" spatial_velocity_1d4="+geometryFloats(spatial,6)
        +" external_force_1ec="+geometryFloats(force,6)+" push_velocity_204="+geometryFloats(push,6)
        +" solver_origin="+geometryFloats(solverOrigin,3)+" solver_axis="+geometryFloats(solverAxis,9)
        +" gravity="+geometryFloats(gravity,3)+" physics_id="+std::to_string(physicsId)
        +" callback="+geometryHex(callback)+" callback_type="+(callbackOk?geometryType(item,callback):std::string{})
        +" callback_slot48_rva="+geometryHex(callbackOk?fn-item.imageBase:0)
        +" callback_calls_observed="+std::to_string(activationCallbackCalls.load(std::memory_order_acquire)-activationInvestigation.callbackCallsBaseline);
}
void activationSummary(const ObjectDiscoveryNativeRecord& item,
    const std::array<unsigned char,0x300>& bytes,unsigned long long elapsed,const char* phase){
    const auto& p=activationInvestigation;
    std::string line=std::string(phase)+" t_ms="+std::to_string(elapsed)
        +" native_motion="+std::to_string(item.nativeMotionObserved)
        +" last_motion_ms="+std::to_string(item.nativeMovedAtMs)
        +" origin="+geometryFloats(item.origin,3)
        +" axis="+geometryFloats(item.axis,9);
    line+=activationReflectedState(item,bytes);
    activationLog(line);
}
void objectActivationInvestigatorTick(const ObjectDiscoveryGripFrame& in,bool valid,
    unsigned long long now,unsigned long long generation){
    auto& p=activationInvestigation;
    if(p.handle&&(p.generation!=generation||now<p.armed)){
        objectActivationInvestigatorReset("generation_or_clock");return;
    }
    // Point the weapon-hand ray without holding grip. A selected body remains latched during a
    // punch even if the hand ray moves away. No input is generated here.
    ObjectDiscoveryNativeRecord* aimed{};
    float bestMiss=kharvox::kGrappleProxyMissUnits,bestDepth=in.reachUnits;
    if(valid&&!p.tracing&&!in.pressed&&!in.calibrating){
        for(auto& item:objectDiscoveryNative){
            if(!item.entity||!item.body||!item.poseValid||item.generation!=generation
                ||now<item.lastPositionAtMs||now-item.lastPositionAtMs>250)continue;
            kharvox::GrappleRayAnchor ray{};
            if(!kharvox::grappleRayEstimate(in.rayOrigin,in.aim,item.origin,in.reachUnits,
                kharvox::kGrappleProxyMissUnits,ray))continue;
            if(!aimed||ray.missDistance<bestMiss||
                (ray.missDistance==bestMiss&&ray.rayDistance<bestDepth)){
                aimed=&item;bestMiss=ray.missDistance;bestDepth=ray.rayDistance;
            }
        }
    }
    if(p.handle&&!p.tracing&&aimed&&aimed->handle==p.handle&&aimed->entity==p.entity&&aimed->body==p.body)
        p.lastAim=now;
    // Retain a stationary selection during hand movement before the melee edge.
    if(p.handle&&!p.tracing&&now-p.lastAim>10000&&!in.nativeMeleeInputDown){
        objectActivationInvestigatorReset("aim_lost");
    }
    if(!p.handle){
        if(!aimed||in.nativeMeleeInputDown)return;
        std::array<unsigned char,0x300> body{},entity{};
        if(!activationRead(*aimed,body,entity))return;
        if(aimed->bodyVtable!=aimed->imageBase+0x2874190||!activationSignature(*aimed))return;
        if(static_cast<std::int32_t>(activationWord(body,0x170))<0)return; // require resting baseline
        p.handle=aimed->handle;p.entity=aimed->entity;p.body=aimed->body;
        p.generation=generation;p.serial=++activationInvestigationSerial;p.armed=now;p.lastAim=now;
        p.signatureOk=activationSignature(*aimed);
        const bool hook=installActivationWakeObserver(*aimed);
        p.wakeCallsReported=activationWakeCalls.load(std::memory_order_acquire);
        if(hook)activationWakeTarget.store(reinterpret_cast<std::uintptr_t>(p.body),std::memory_order_release);
        activationLog("WAKE_ENTRY_OBSERVER installed="+std::to_string(hook));
        p.baseline=p.previous=body;p.entityBaseline=p.entityPrevious=entity;
        std::memcpy(p.priorOrigin,aimed->origin,sizeof(p.priorOrigin));
        std::memcpy(p.startOrigin,aimed->origin,sizeof(p.startOrigin));
        activationLog("armed handle="+std::to_string(p.handle)+" model="+std::string(aimed->modelKey.data())
            +" body_vtable_rva="+geometryHex(aimed->bodyVtable-aimed->imageBase)
            +" entity_vtable_rva="+geometryHex(aimed->entityVtable-aimed->imageBase)
            +" wake_rva=0x179a8f0 signature_ok="+std::to_string(p.signatureOk)
            +" weapon_hand="+std::string(in.physicalHand==1?"right":"left")
            +" aim_origin="+geometryFloats(in.rayOrigin,3)+" aim_dir="+geometryFloats(in.aim,3)
            +" proxy_miss="+std::to_string(bestMiss)+" contact_unverified=1");
        // Enumerate matching virtual slots, without invoking any function.
        if(p.signatureOk)for(unsigned i=0;i<128;++i){
            std::uintptr_t fn{};
            if(!geometryRead(aimed->bodyVtable+i*sizeof(fn),&fn,sizeof(fn)))break;
            if(fn==aimed->imageBase+0x179A8F0||fn==aimed->imageBase+0x179A900||fn==aimed->imageBase+0x179AAD0)
                activationLog("candidate native slot="+geometryHex(i*sizeof(fn))+" rva="+geometryHex(fn-aimed->imageBase));
        }
        // Trace the identified callback candidate, not an assumed melee-specific function.
        std::uintptr_t cb{},vt{},fn{};
        std::memcpy(&cb,body.data()+0x28,8);
        if(geometryType(*aimed,cb)=="idEntityPhysicsCallbacks"&&geometryRead(cb,&vt,8)
            &&geometryRead(vt+0x48,&fn,8)&&geometryInImage(*aimed,fn)){
            unsigned char bytes[32]{};
            if(geometryRead(fn,bytes,sizeof(bytes))){
                std::ostringstream raw;raw<<std::hex<<std::setfill('0');
                for(auto b:bytes)raw<<std::setw(2)<<static_cast<unsigned>(b);
                activationLog("CALLBACK_ENTRY_CANDIDATE rva="+geometryHex(fn-aimed->imageBase)+" entry32="+raw.str()+" semantics=unverified");
                const bool installed=installActivationCallbackObserver(*aimed,fn);
                p.callbackCallsReported=p.callbackCallsBaseline=activationCallbackCalls.load(std::memory_order_acquire);
                if(installed)activationCallbackTarget.store(cb,std::memory_order_release);
                activationLog("CALLBACK_ENTRY_OBSERVER installed="+std::to_string(installed));
            }
        }
        activationSummary(*aimed,body,0,"baseline");
        return;
    }
    auto* item=objectDiscoveryNativeByHandle(p.handle);
    if(!item||item->entity!=p.entity||item->body!=p.body||item->generation!=p.generation){
        objectActivationInvestigatorReset("identity_lost");return;
    }
    if(!item->poseValid||now<item->lastPositionAtMs||now-item->lastPositionAtMs>250)return;
    const auto calls=activationWakeCalls.load(std::memory_order_acquire);
    if(calls!=p.wakeCallsReported){
        const auto caller=activationWakeCaller.load(std::memory_order_relaxed);
        activationLog("NATIVE_WAKE_ENTRY count_delta="+std::to_string(calls-p.wakeCallsReported)
            +" sequence="+std::to_string(activationWakeSequence.load())+" at_ms="+std::to_string(activationWakeAt.load())
            +" thread="+std::to_string(activationWakeThread.load())+" caller="+geometryHex(caller)
            +" caller_rva="+geometryHex(geometryInImage(*item,caller)?caller-item->imageBase:0));
        p.wakeCallsReported=calls;nativeResearchEvidence(4u,"NATIVE_WAKE_EXECUTED selected_body=1",true);
    }
    const auto callbackCalls=activationCallbackCalls.load(std::memory_order_acquire);
    if(callbackCalls!=p.callbackCallsReported){
        const auto caller=activationCallbackCaller.load(std::memory_order_relaxed);
        activationLog("NATIVE_CALLBACK_ENTRY count_delta="+std::to_string(callbackCalls-p.callbackCallsReported)
            +" sequence="+std::to_string(activationCallbackSequence.load())+" at_ms="+std::to_string(activationCallbackAt.load())
            +" thread="+std::to_string(activationCallbackThread.load())+" caller_rva="+geometryHex(geometryInImage(*item,caller)?caller-item->imageBase:0)
            +" slot48=1 melee_specific=unverified");
        p.callbackCallsReported=callbackCalls;nativeResearchEvidence(4u,"NATIVE_CALLBACK_CANDIDATE_EXECUTED selected_owner=1",false);
    }
    const bool edge=in.nativeMeleeInputDown&&!p.meleeWasDown;
    p.meleeWasDown=in.nativeMeleeInputDown;
    if(edge){activationLog("ordinary_melee_input_edge armed_ms="+std::to_string(now-p.armed)
        +" input_not_contact_proof=1");nativeResearchEvidence(4u,"MELEE_INPUT_CAPTURED contact_unverified=1",false);}
    const unsigned interval=p.tracing&&now-p.started>2000?100:12;
    if(!edge&&p.lastSample&&now-p.lastSample<interval)return;
    p.lastSample=now;
    std::array<unsigned char,0x300> body{},entity{};
    if(!activationRead(*item,body,entity)){objectActivationInvestigatorReset("read_failed");return;}
    float d2{};for(unsigned i=0;i<3;++i){const float d=item->origin[i]-p.priorOrigin[i];d2+=d*d;}
    const bool moved=std::isfinite(d2)&&d2>.25f;
    const bool wakeChanged=p.signatureOk&&activationWord(body,0x170)!=activationWord(p.previous,0x170);
    const bool velocityChanged=p.signatureOk&&std::memcmp(body.data()+0x1D4,p.previous.data()+0x1D4,72)!=0;
    if(!p.tracing&&(edge||moved||wakeChanged||velocityChanged)){
        p.tracing=true;p.started=now;p.lastSummary=0;p.motionSeen=false;p.wakeSeen=false;
        p.baseline=p.previous;p.entityBaseline=p.entityPrevious;
        activationLog("trace_start reason="+std::string(edge?"melee_input":wakeChanged?"wake_word_change":velocityChanged?"reflected_state_change":"native_motion")
            +" sampled_order_not_call_trace=1");
        activationSummary(*item,p.previous,0,"pre_event_bytes");
    }
    if(p.tracing){
        ++p.samples;
        if(wakeChanged&&!p.wakeSeen){p.wakeSeen=true;nativeResearchEvidence(4u,"WAKE_WORD_CHANGED pre="+geometryHex(activationWord(p.previous,0x170))+" post="+geometryHex(activationWord(body,0x170))+" callback_semantics_unverified=1",false);}
        if(moved&&!p.motionSeen){p.motionSeen=true;nativeResearchEvidence(4u,"outcome=FRESH_NATIVE_MOTION_OBSERVED pre_rest="+geometryHex(activationWord(p.baseline,0x170))+" rest_word="+geometryHex(activationWord(body,0x170))+" callback_semantics_unverified=1");}
        const auto elapsed=now-p.started;
        activationDiff("body",p.previous,body,elapsed);
        activationDiff("entity",p.entityPrevious,entity,elapsed);
        if(edge||moved||wakeChanged||velocityChanged||!p.lastSummary||now-p.lastSummary>=1000){
            activationSummary(*item,body,elapsed,"sample");p.lastSummary=now;
        }
        if(elapsed>=3000&&!in.nativeMeleeInputDown){
            activationDiff("body_vs_pre_event",p.baseline,body,elapsed);
            activationDiff("entity_vs_pre_event",p.entityBaseline,entity,elapsed);
            activationSummary(*item,body,elapsed,"final");
            nativeResearchEvidence(4u,"ATTEMPT_RECORDED motion="+std::to_string(p.motionSeen)+" wake_change="+std::to_string(p.wakeSeen)+" callback_semantics_unverified=1",false);
            objectActivationInvestigatorReset("attempt_window_complete_rearm");return;
        }
    }
    p.previous=body;p.entityPrevious=entity;
    std::memcpy(p.priorOrigin,item->origin,sizeof(p.priorOrigin));
}
#endif
