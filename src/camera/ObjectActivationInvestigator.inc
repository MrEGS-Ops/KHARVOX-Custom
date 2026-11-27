// Passive world-thread observer. No new hooks, calls into candidate code or writes.
// The wake field is a signature-derived hypothesis, not a verified active flag.
#if KHARVOX_EXPERIMENTAL_WORLD_GRAB
struct ActivationInvestigation {
    unsigned handle{};
    const void* entity{};
    void* body{};
    unsigned long long generation{},armed{},started{},lastAim{},lastSample{},lastSummary{},serial{};
    bool meleeWasDown{},signatureOk{},tracing{},motionSeen{};
    std::array<unsigned char,0x300> baseline{},previous{};
    std::array<unsigned char,0x300> entityBaseline{},entityPrevious{};
    float priorOrigin[3]{},startOrigin[3]{};
    unsigned samples{};
};
ActivationInvestigation activationInvestigation{};
unsigned long long activationInvestigationSerial{};
void activationLog(const std::string& msg){
    log("[ACTIVATION-INV] id="+std::to_string(activationInvestigation.serial)+" "+msg);
}
void objectActivationInvestigatorReset(const char* reason){
    if(activationInvestigation.handle)
        activationLog(std::string("end reason=")+reason+" samples="+std::to_string(activationInvestigation.samples)
            +" motion_seen="+std::to_string(activationInvestigation.motionSeen));
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
    return matches(0x179A8F0,wake,sizeof(wake))&&matches(0x179A958,linear,sizeof(linear))
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
        +" callback_observed_call=0";
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
    if(p.handle&&!p.tracing&&now-p.lastAim>1200&&!in.nativeMeleeInputDown){
        objectActivationInvestigatorReset("aim_lost");
    }
    if(!p.handle){
        if(!aimed||in.nativeMeleeInputDown)return;
        std::array<unsigned char,0x300> body{},entity{};
        if(!activationRead(*aimed,body,entity))return;
        p.handle=aimed->handle;p.entity=aimed->entity;p.body=aimed->body;
        p.generation=generation;p.serial=++activationInvestigationSerial;p.armed=now;p.lastAim=now;
        p.signatureOk=activationSignature(*aimed);
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
        activationSummary(*aimed,body,0,"baseline");
        return;
    }
    auto* item=objectDiscoveryNativeByHandle(p.handle);
    if(!item||item->entity!=p.entity||item->body!=p.body||item->generation!=p.generation){
        objectActivationInvestigatorReset("identity_lost");return;
    }
    if(!item->poseValid||now<item->lastPositionAtMs||now-item->lastPositionAtMs>250)return;
    const bool edge=in.nativeMeleeInputDown&&!p.meleeWasDown;
    p.meleeWasDown=in.nativeMeleeInputDown;
    if(edge)activationLog("ordinary_melee_input_edge armed_ms="+std::to_string(now-p.armed)
        +" input_not_contact_proof=1");
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
        p.tracing=true;p.started=now;p.lastSummary=0;
        p.baseline=p.previous;p.entityBaseline=p.entityPrevious;
        activationLog("trace_start reason="+std::string(edge?"melee_input":wakeChanged?"wake_word_change":velocityChanged?"reflected_state_change":"native_motion")
            +" sampled_order_not_call_trace=1");
        activationSummary(*item,p.previous,0,"pre_event_bytes");
    }
    if(p.tracing){
        ++p.samples;p.motionSeen=p.motionSeen||moved;
        if(p.signatureOk&&item->bodyVtable==item->imageBase+0x2874190&&p.motionSeen)nativeResearchEvidence(4u,"outcome=NATIVE_MOTION_OBSERVED pre_rest="+geometryHex(activationWord(p.baseline,0x170))+" rest_word="+geometryHex(activationWord(body,0x170))+" callback_call_observed=0");
        const auto elapsed=now-p.started;
        activationDiff("body",p.previous,body,elapsed);
        activationDiff("entity",p.entityPrevious,entity,elapsed);
        if(edge||moved||wakeChanged||velocityChanged||!p.lastSummary||now-p.lastSummary>=1000){
            activationSummary(*item,body,elapsed,"sample");p.lastSummary=now;
        }
        if(elapsed>=30000){
            activationDiff("body_vs_pre_event",p.baseline,body,elapsed);
            activationDiff("entity_vs_pre_event",p.entityBaseline,entity,elapsed);
            activationSummary(*item,body,elapsed,"final");
            objectActivationInvestigatorReset("30s_complete");return;
        }
    }
    p.previous=body;p.entityPrevious=entity;
    std::memcpy(p.priorOrigin,item->origin,sizeof(p.priorOrigin));
}
#endif
