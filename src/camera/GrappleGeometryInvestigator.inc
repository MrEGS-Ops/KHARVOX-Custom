// Read-only evidence capture, invoked on the DOOM world thread.
// No unverified native functions are called. All candidate interpretations
// are hypotheses; none affect selection, activation or physics.
#if KHARVOX_EXPERIMENTAL_WORLD_GRAB
struct GrappleGeometryInvestigation {
    bool down{},pending{};
    unsigned long long generation{},serial{},started{};
    unsigned handle{};
    unsigned dumps{};
};
GrappleGeometryInvestigation grappleGeometryInvestigation{};
std::string geometryHex(std::uintptr_t value){
    std::ostringstream o;o<<"0x"<<std::hex<<value;return o.str();
}
std::string geometryFloats(const float* data,unsigned n){
    std::ostringstream o;o<<std::setprecision(7);
    for(unsigned i=0;i<n;++i){if(i)o<<',';o<<data[i];}
    return o.str();
}
bool geometryRead(std::uintptr_t address,void* out,std::size_t bytes){
    if(!address||!out||!bytes||address>UINTPTR_MAX-bytes)return false;
    SIZE_T got{};
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),
        out,bytes,&got)&&got==bytes;
}
bool geometryInImage(const ObjectDiscoveryNativeRecord& item,std::uintptr_t p){
    return p>=item.imageBase&&p-item.imageBase<item.imageSize;
}
std::string geometryType(const ObjectDiscoveryNativeRecord& item,std::uintptr_t address){
    std::uintptr_t vt{};std::array<char,80> label{};
    if(!geometryRead(address,&vt,sizeof(vt))||!geometryInImage(item,vt)
       ||!readNativeWorldGrabClassLabel(vt,item.imageBase,item.imageSize,label))return {};
    return label.data();
}
std::string geometryAsset(std::uintptr_t address){
    // Read only bounded printable resource-like paths, never arbitrary text.
    char s[192]{};
    if(!geometryRead(address,s,16))return {};
    if(std::memcmp(s,"models/",7)&&std::memcmp(s,"model/",6)
        &&std::memcmp(s,"materials/",10)&&std::memcmp(s,"textures/",9)
        &&std::memcmp(s,"collision/",10)&&std::memcmp(s,"generated/",10))return {};
    for(unsigned i=0;i<sizeof(s)-1;++i){
        if(i>=16&&!geometryRead(address+i,s+i,1))return {};
        const char ch=s[i];if(!ch)return i>=8?std::string(s,i):std::string{};
        if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')
            ||ch=='/'||ch=='_'||ch=='-'||ch=='.'))return {};
    }
    return {};
}
void geometryLog(const std::string& line){
    log("[GEOMETRY-INV] id="+std::to_string(grappleGeometryInvestigation.serial)+" "+line);
}
// Reflected DOOM 2016 Vulkan layout. Diagnostic only: never supplies a gameplay hit.
// Bound all work, reject alternate encodings, and compare decoded local bounds.
bool geometryReflectedSignature(const ObjectDiscoveryNativeRecord& item){
    constexpr unsigned char getter[]{0x48,0x8b,0x81,0x90,0x0e,0x00,0x00,0x48,0x85,0xc0,0x75,0x01,0xc3,0x48,0x05,0x88,0x00,0x00,0x00,0xc3};
    constexpr unsigned char loop[]{0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x6c,0x24,0x20,0x41,0x56,0x48,0x83,0xec,0x20,0x45,0x33,0xf6,0x48,0x8b,0xd9};
    unsigned char buf[32]{};
    return item.imageSize>0x283f9b8+8
        &&geometryRead(item.imageBase+0x15de100,buf,sizeof(getter))
        &&std::memcmp(buf,getter,sizeof(getter))==0
        &&geometryRead(item.imageBase+0x15d85e0,buf,sizeof(loop))
        &&std::memcmp(buf,loop,sizeof(loop))==0;
}
void geometryReflectedMesh(const ObjectDiscoveryNativeRecord& item,const ObjectDiscoveryGripFrame& in){
    const auto start=GetTickCount64();
    if(!geometryReflectedSignature(item)){geometryLog("MESH_SKIP reason=signature");return;}
    std::uintptr_t model{},vt{},surfaces{};
    std::uint32_t count{};
    float transform[15]{};
    const auto ep=reinterpret_cast<std::uintptr_t>(item.entity);
    if(!geometryRead(ep+0x408,&model,sizeof(model))||!geometryRead(model,&vt,sizeof(vt))
        ||vt!=item.imageBase+0x283f9b8||!geometryRead(model+0xae0,&surfaces,sizeof(surfaces))
        ||!geometryRead(model+0xae8,&count,sizeof(count))||!geometryRead(model+0xc8,transform,sizeof(transform))
        ||!surfaces||!count||count>128){
        geometryLog("MESH_SKIP reason=model_or_list model="+geometryHex(model)+" count="+std::to_string(count));return;
    }
    bool pose=objectDiscoveryAxisOk(transform+3);
    for(unsigned k=0;k<3;++k)
        pose=pose&&std::isfinite(transform[k])&&std::isfinite(transform[12+k])
            &&std::abs(transform[12+k])>.0001f&&std::abs(transform[12+k])<100.f;
    geometryLog("MESH_BEGIN model="+geometryHex(model)+" surfaces="+std::to_string(count)
        +" game_origin="+geometryFloats(transform,3)+" game_axis="+geometryFloats(transform+3,9)
        +" scale="+geometryFloats(transform+12,3)+" pose_plausible="+std::to_string(pose)
        +" ray="+geometryFloats(in.rayOrigin,3)+" aim="+geometryFloats(in.aim,3));
    unsigned decoded=0,logged=0,visited=0,totalTriangles=0;
    float best=in.reachUnits+1.f;std::array<float,3> hitPoint{};
    for(unsigned si=0;si<count&&si<16&&GetTickCount64()-start<20;++si){
        ++visited;
        std::array<unsigned char,0x60> surface{};
        if(!geometryRead(surfaces+si*0x60,surface.data(),surface.size()))break;
        std::uintptr_t material{},tri{},joints{},morph{};
        std::memcpy(&material,surface.data(),8);std::memcpy(&tri,surface.data()+0x18,8);
        std::memcpy(&joints,surface.data()+0x30,8);std::memcpy(&morph,surface.data()+0x40,8);
        if(!material||!tri||joints||morph){geometryLog("MESH_SURFACE_SKIP i="+std::to_string(si)+" reason=disabled_or_deformed");continue;}
        std::array<unsigned char,0x84> h{};
        if(!geometryRead(tri,h.data(),h.size()))continue;
        float bounds[6]{};std::uint32_t primitive{},ni{},nv{},mask{},gpuMask{};
        std::uintptr_t verts{},indices{};
        std::memcpy(bounds,h.data(),24);std::memcpy(&primitive,h.data()+0x1c,4);
        std::memcpy(&ni,h.data()+0x20,4);std::memcpy(&nv,h.data()+0x24,4);
        std::memcpy(&mask,h.data()+0x68,4);std::memcpy(&verts,h.data()+0x70,8);
        std::memcpy(&indices,h.data()+0x78,8);std::memcpy(&gpuMask,h.data()+0x80,4);
        geometryLog("MESH_SURFACE i="+std::to_string(si)+" primitive="+std::to_string(primitive)
            +" vertices="+std::to_string(nv)+" indices="+std::to_string(ni)
            +" cpu_mask="+geometryHex(mask)+" gpu_mask="+geometryHex(gpuMask)
            +" cpu_verts="+geometryHex(verts)+" cpu_indices="+geometryHex(indices)
            +" bounds="+geometryFloats(bounds,6));
        if(primitive!=0||mask!=0x1801f||!verts||!indices||nv<3||nv>4096||ni<3||ni%3||ni>12288
            ||totalTriangles+ni/3>8192)continue;
        bool boundsOk=true;
        for(unsigned k=0;k<3;++k)boundsOk=boundsOk&&std::isfinite(bounds[k])&&std::isfinite(bounds[k+3])
            &&bounds[k]<=bounds[k+3]&&std::abs(bounds[k])<100000.f&&std::abs(bounds[k+3])<100000.f;
        if(!boundsOk)continue;
        std::vector<std::uint16_t> ix(ni);
        if(!geometryRead(indices,ix.data(),ix.size()*sizeof(ix[0])))continue;
        bool indexOk=true;for(auto i:ix)if(i>=nv){indexOk=false;break;}
        if(!indexOk){geometryLog("MESH_REJECT reason=index_range");continue;}
        // Reflection says 48 bytes; an embedded comment says 32. Evaluate both,
        // accept only a unique bounds-consistent interpretation for telemetry.
        std::vector<std::array<float,3>> points;
        unsigned chosen=0,passing=0;
        for(unsigned stride:{48u,32u}){
            std::vector<unsigned char> raw(static_cast<std::size_t>(nv)*stride);
            if(!geometryRead(verts,raw.data(),raw.size()))continue;
            std::vector<std::array<float,3>> trial(nv);
            float lo[3]{1e30f,1e30f,1e30f},hi[3]{-1e30f,-1e30f,-1e30f};
            bool ok=true;
            for(unsigned i=0;i<nv&&ok;++i){
                std::memcpy(trial[i].data(),raw.data()+static_cast<std::size_t>(i)*stride,12);
                for(unsigned k=0;k<3;++k){
                    const float f=trial[i][k];
                    if(!std::isfinite(f)||std::abs(f)>100000.f){ok=false;break;}
                    lo[k]=std::min(lo[k],f);hi[k]=std::max(hi[k],f);
                }
            }
            for(unsigned k=0;k<3&&ok;++k){
                const float tolerance=std::max(.25f,(bounds[k+3]-bounds[k])*.025f);
                if(std::abs(lo[k]-bounds[k])>tolerance||std::abs(hi[k]-bounds[k+3])>tolerance)ok=false;
            }
            geometryLog("MESH_LAYOUT i="+std::to_string(si)+" stride="+std::to_string(stride)
                +" bounds_match="+std::to_string(ok)+" lo="+geometryFloats(lo,3)+" hi="+geometryFloats(hi,3));
            if(ok){++passing;chosen=stride;points=std::move(trial);}
        }
        std::array<unsigned char,0x84> check{};
        if(passing!=1||!geometryRead(tri,check.data(),check.size())||check!=h){
            geometryLog("MESH_REJECT i="+std::to_string(si)+" reason=ambiguous_or_changed");continue;
        }
        ++decoded;totalTriangles+=ni/3;
        std::vector<float> world;world.reserve(static_cast<std::size_t>(ni)*3);
        // Local triangle samples allow an offline wireframe comparison. No XR
        // render-thread publication or native renderer invocation in this pass.
        std::ostringstream sample;unsigned batch=0;
        for(unsigned j=0;j<ni;j+=3){
            for(unsigned q=0;q<3;++q){
                const auto& p=points[ix[j+q]];
                if(logged<256){
                    if(q)sample<<';';
                    sample<<geometryFloats(p.data(),3);
                }
                if(pose)for(unsigned k=0;k<3;++k){
                    float v=transform[k];
                    for(unsigned l=0;l<3;++l)v+=p[l]*transform[12+l]*transform[3+3*l+k];
                    world.push_back(v);
                }
            }
            if(logged<256){sample<<'|';++logged;++batch;}
            if(batch==8||(j+3==ni&&batch)){geometryLog("MESH_TRIANGLES i="+std::to_string(si)+" local="+sample.str());sample.str("");sample.clear();batch=0;}
        }
        geometryLog("MESH_DECODE i="+std::to_string(si)+" stride="+std::to_string(chosen)
            +" triangles="+std::to_string(ni/3)+" status=CANDIDATE_NOT_GAMEPLAY");
        if(pose&&!world.empty()){
            kharvox::GrappleRayAnchor hit{};
            if(kharvox::grappleRenderTriangleHit(in.rayOrigin,in.aim,world.data(),ni/3,in.reachUnits,hit)
                &&hit.rayDistance<best){best=hit.rayDistance;hitPoint=hit.worldPoint;}
        }
    }
    std::uintptr_t after{};std::uint32_t afterCount{};
    const bool stable=geometryRead(model+0xae0,&after,sizeof(after))&&after==surfaces
        &&geometryRead(model+0xae8,&afterCount,sizeof(afterCount))&&afterCount==count;
    if(stable&&decoded&&pose)nativeResearchEvidence(1u,"mesh decoded="+std::to_string(decoded)+" candidate_hit="+std::to_string(best<=in.reachUnits)+" depth="+std::to_string(best));
    geometryLog("MESH_END visited="+std::to_string(visited)+" decoded="+std::to_string(decoded)
        +" triangles="+std::to_string(totalTriangles)+" logged="+std::to_string(logged)
        +" list_stable="+std::to_string(stable)+" candidate_hit="+std::to_string(stable&&best<=in.reachUnits)
        +" candidate_world="+geometryFloats(hitPoint.data(),3)+" candidate_depth="+std::to_string(best)
        +" elapsed_ms="+std::to_string(GetTickCount64()-start)+" verified_geometry=NONE");
}
void geometryDumpObject(const ObjectDiscoveryNativeRecord& item,const ObjectDiscoveryGripFrame& in,const char* reason){
    auto& state=grappleGeometryInvestigation;
    if(state.dumps>=2||!item.entity||!item.body)return;
    if(item.generation!=levelTransitionGeneration.load(std::memory_order_acquire))return;
    std::uintptr_t ev{},bv{};
    const auto ep=reinterpret_cast<std::uintptr_t>(item.entity);
    const auto bp=reinterpret_cast<std::uintptr_t>(item.body);
    if(!geometryRead(ep,&ev,sizeof(ev))||!geometryRead(bp,&bv,sizeof(bv))
        ||ev!=item.entityVtable||bv!=item.bodyVtable)return;
    ++state.dumps;state.handle=item.handle;
    const auto start=GetTickCount64();
    geometryLog(std::string("OBJECT_BEGIN reason=")+reason+" handle="+std::to_string(item.handle)
        +" model="+item.modelKey.data()+" entity="+geometryHex(ep)+" body="+geometryHex(bp)
        +" entity_type="+geometryType(item,ep)+" body_type="+geometryType(item,bp)
        +" image="+geometryHex(item.imageBase)+" image_size="+geometryHex(item.imageSize)
        +" entity_vtable_rva="+geometryHex(ev-item.imageBase)
        +" body_vtable_rva="+geometryHex(bv-item.imageBase)
        +" origin="+geometryFloats(item.origin,3)+" axis="+geometryFloats(item.axis,9)
        +" motion="+std::to_string(item.nativeMotionObserved)
        +" observed_ms="+std::to_string(item.observedAtMs)
        +" moved_ms="+std::to_string(item.nativeMovedAtMs));
    geometryReflectedMesh(item,in);
    struct Node{std::uintptr_t address{},parent{};unsigned offset{},depth{},limit{};std::string type;};
    std::vector<Node> nodes;
    nodes.push_back({ep,0,0,0,4096,geometryType(item,ep)});
    if(bp!=ep)nodes.push_back({bp,0,0,0,2048,geometryType(item,bp)});
    unsigned edges{},bounds{},samples{},rawBytes{},visited{};
    constexpr unsigned maxNodes=24,maxEdges=120,maxBounds=48,maxSamples=24,maxRaw=32768;
    auto enqueue=[&](std::uintptr_t ptr,std::uintptr_t owner,unsigned off,unsigned depth,const std::string& type){
        if(type.empty()||nodes.size()>=maxNodes||depth>3)return;
        for(const auto& n:nodes)if(n.address==ptr)return;
        nodes.push_back({ptr,owner,off,depth,1024,type});
    };
    for(unsigned ni=0;ni<nodes.size()&&GetTickCount64()-start<35;++ni){
        // Copy node before enqueue grows the vector.
        const auto node=nodes[ni];
        std::array<unsigned char,4096> bytes{};unsigned size{};
        for(unsigned off=0;off<node.limit&&off+64<=bytes.size();off+=64){
            if(!geometryRead(node.address+off,bytes.data()+off,64))break;
            size=off+64;
        }
        if(!size)continue;++visited;
        geometryLog("NODE index="+std::to_string(ni)+" addr="+geometryHex(node.address)
            +" parent="+geometryHex(node.parent)+" parent_off="+geometryHex(node.offset)
            +" depth="+std::to_string(node.depth)+" type="+node.type+" bytes="+std::to_string(size));
        // Raw windows permit later offline decoding without guessing ABI.
        for(unsigned off=0;off<size&&rawBytes<maxRaw;off+=128){
            const unsigned count=std::min(128u,size-off);
            std::ostringstream hex;hex<<std::hex<<std::setfill('0');
            for(unsigned k=0;k<count;++k)hex<<std::setw(2)<<static_cast<unsigned>(bytes[off+k]);
            geometryLog("RAW node="+std::to_string(ni)+" off="+geometryHex(off)+" data="+hex.str());
            rawBytes+=count;
        }
        std::uintptr_t vt{};
        std::memcpy(&vt,bytes.data(),sizeof(vt));
        if(geometryInImage(item,vt)){
            std::ostringstream slots;slots<<"VTABLE node="<<ni<<" rva="<<geometryHex(vt-item.imageBase);
            for(unsigned k=0;k<48;++k){
                std::uintptr_t fn{};
                if(!geometryRead(vt+k*sizeof(fn),&fn,sizeof(fn))||!geometryInImage(item,fn)
                   ||!executableMemory(reinterpret_cast<const void*>(fn)))break;
                slots<<" "<<k<<":"<<geometryHex(fn-item.imageBase);
            }
            geometryLog(slots.str());
        }
        for(unsigned off=0;off+sizeof(std::uintptr_t)<=size&&GetTickCount64()-start<35;off+=sizeof(std::uintptr_t)){
            std::uintptr_t ptr{};std::memcpy(&ptr,bytes.data()+off,sizeof(ptr));
            if(ptr<0x10000||ptr>0x00007fffffffffffULL)continue;
            const auto embedded=node.address+off;
            // Detect embedded polymorphic objects as well as pointer fields.
            if(geometryInImage(item,ptr)){
                const auto type=geometryType(item,embedded);
                if(!type.empty()&&embedded!=node.address){
                    if(edges++<maxEdges)geometryLog("EMBEDDED node="+std::to_string(ni)
                        +" off="+geometryHex(off)+" type="+type);
                    enqueue(embedded,node.address,off,node.depth+1,type);
                }
                continue;
            }
            const auto type=geometryType(item,ptr);
            const auto asset=geometryAsset(ptr);
            if(!type.empty()||!asset.empty()){
                if(edges++<maxEdges)geometryLog("EDGE node="+std::to_string(ni)+" off="+geometryHex(off)
                    +" ptr="+geometryHex(ptr)+" type="+type+" asset="+asset);
                enqueue(ptr,node.address,off,node.depth+1,type);
                continue;
            }
            // Untyped arrays are only sampled when adjacent fields contain
            // plausible element counts. Preserve raw samples, not decoded claims.
            if(samples>=maxSamples||ptr%4||off+16>size)continue;
            std::uint32_t count{},capacity{};
            std::memcpy(&count,bytes.data()+off+8,4);
            std::memcpy(&capacity,bytes.data()+off+12,4);
            if(count<3||count>1000000||capacity<count||capacity>2000000)continue;
            std::array<unsigned char,96> sample{};
            if(!geometryRead(ptr,sample.data(),sample.size()))continue;
            std::ostringstream hex;hex<<std::hex<<std::setfill('0');
            for(auto ch:sample)hex<<std::setw(2)<<static_cast<unsigned>(ch);
            geometryLog("ARRAY_CANDIDATE node="+std::to_string(ni)+" off="+geometryHex(off)
                +" ptr="+geometryHex(ptr)+" count="+std::to_string(count)
                +" capacity="+std::to_string(capacity)+" sample96="+hex.str()+" layout=UNVERIFIED");
            ++samples;
        }
        // Potential AABBs: six contiguous finite floats with ordered extrema,
        // plausible extents, and a local or near-world centre. Many are false
        // positives; retain offsets and exact values for cross-object comparison.
        for(unsigned off=0;off+24<=size&&bounds<maxBounds&&GetTickCount64()-start<35;off+=4){
            float f[6]{};std::memcpy(f,bytes.data()+off,sizeof(f));
            bool plausible=true,local=true,world=true;float ext[3]{},centre[3]{};
            for(unsigned k=0;k<3;++k){
                ext[k]=f[k+3]-f[k];centre[k]=(f[k]+f[k+3])*.5f;
                if(!std::isfinite(f[k])||!std::isfinite(f[k+3])||ext[k]<.5f||ext[k]>500.f)
                    plausible=false;
                local=local&&std::abs(centre[k])<150.f;
                world=world&&std::abs(centre[k]-item.origin[k])<150.f;
            }
            if(!plausible||(!local&&!world))continue;
            geometryLog("BOUNDS_CANDIDATE node="+std::to_string(ni)+" off="+geometryHex(off)
                +" minmax="+geometryFloats(f,6)+" size="+geometryFloats(ext,3)
                +" centre="+geometryFloats(centre,3)+" local="+std::to_string(local)
                +" near_world="+std::to_string(world)+" layout=UNVERIFIED");
            ++bounds;
        }
    }
    geometryLog("OBJECT_END handle="+std::to_string(item.handle)+" nodes="+std::to_string(visited)
        +" queued="+std::to_string(nodes.size())+" edges="+std::to_string(edges)
        +" bounds="+std::to_string(bounds)+" array_samples="+std::to_string(samples)
        +" raw_bytes="+std::to_string(rawBytes)+" elapsed_ms="+std::to_string(GetTickCount64()-start)
        +" limit_hit="+std::to_string(GetTickCount64()-start>=35||nodes.size()>=maxNodes)
        +" verified_geometry=NONE");
}
void grappleGeometryInvestigatorTick(const ObjectDiscoveryGripFrame& in,bool valid,
    unsigned long long now,unsigned long long generation){
    auto& s=grappleGeometryInvestigation;
    if(!valid||!in.pressed||!((in.researchMask|in.researchTestMask)&1u)||in.calibrating){
        if(s.down)geometryLog("END elapsed_ms="+std::to_string(now-s.started));
        s.down=false;s.pending=false;return;
    }
    if(s.down&&s.generation==generation)return;
    s.down=true;s.pending=true;s.generation=generation;++s.serial;s.started=now;s.dumps=0;s.handle=0;
    geometryLog("BEGIN hand="+std::to_string(in.physicalHand)+" gen="+std::to_string(generation)
        +" grip="+geometryFloats(in.origin,3)+" grip_axis="+geometryFloats(in.axis,9)
        +" ray="+geometryFloats(in.rayOrigin,3)+" aim="+geometryFloats(in.aim,3)
        +" reach="+std::to_string(in.reachUnits)+" scan="+std::to_string(in.scanUnits)
        +" melee="+std::to_string(in.meleeActivationEnabled));
    struct Candidate{const ObjectDiscoveryNativeRecord* item{};float depth{},miss{},dist{};bool accepted{};};
    std::vector<Candidate> candidates;
    float norm2{};for(float x:in.aim)norm2+=x*x;
    if(!std::isfinite(norm2)||norm2<.25f||norm2>4.f)return;
    const float norm=std::sqrt(norm2);
    for(const auto& item:objectDiscoveryNative){
        if(!item.entity||!item.body||!item.poseValid||item.generation!=generation)continue;
        float d2{},depth{};for(unsigned k=0;k<3;++k){
            const float d=item.origin[k]-in.rayOrigin[k];d2+=d*d;depth+=d*in.aim[k]/norm;
        }
        if(!std::isfinite(d2)||depth<=0||depth>in.reachUnits+60.f)continue;
        const float miss=std::sqrt(std::max(0.f,d2-depth*depth));
        if(miss>100.f)continue;
        kharvox::GrappleRayAnchor ray{};
        const bool accepted=kharvox::grappleRayEstimate(in.rayOrigin,in.aim,item.origin,
            in.reachUnits,kharvox::kGrappleProxyMissUnits,ray);
        candidates.push_back({&item,depth,miss,std::sqrt(d2),accepted});
    }
    std::sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b){
        if(a.accepted!=b.accepted)return a.accepted>b.accepted;
        if(a.miss!=b.miss)return a.miss<b.miss;return a.depth<b.depth;
    });
    geometryLog("CANDIDATES count="+std::to_string(candidates.size()));
    for(unsigned i=0;i<candidates.size()&&i<16;++i){
        const auto& a=candidates[i];const auto& item=*a.item;
        geometryLog("CANDIDATE rank="+std::to_string(i)+" handle="+std::to_string(item.handle)
            +" model="+item.modelKey.data()+" origin="+geometryFloats(item.origin,3)
            +" depth="+std::to_string(a.depth)+" miss="+std::to_string(a.miss)
            +" distance="+std::to_string(a.dist)+" proxy_pass="+std::to_string(a.accepted)
            +" pose_age_ms="+std::to_string(now>=item.lastPositionAtMs?now-item.lastPositionAtMs:0)
            +" moving="+std::to_string(item.nativeMotionObserved));
    }
    if(!candidates.empty())geometryDumpObject(*candidates.front().item,in,"initial_ray_candidate");
    else geometryLog("NO_CANDIDATE near ray");
}
void grappleGeometryInvestigatorAcquired(const ObjectDiscoveryGripFrame& in,
    const ObjectDiscoveryNativeRecord& item,unsigned long long now){
    auto& s=grappleGeometryInvestigation;
    if(!((in.researchMask|in.researchTestMask)&1u)||!s.down||!s.pending)return;
    s.pending=false;
    geometryLog("ACQUIRED handle="+std::to_string(item.handle)+" initial_dump_handle="+std::to_string(s.handle)
        +" latency_ms="+std::to_string(now-s.started)+" origin="+geometryFloats(item.origin,3)
        +" axis="+geometryFloats(item.axis,9));
    if(s.handle!=item.handle)geometryDumpObject(item,in,"actual_acquisition");
}
void grappleGeometryInvestigatorContact(const ObjectDiscoveryGripFrame& in,
    const ObjectDiscoveryNativeRecord& item,const kharvox::GrappleRayAnchor& ray,
    const std::array<float,3>& offset,const std::array<float,9>& axis){
    if(!grappleGeometryInvestigation.down||!((in.researchMask|in.researchTestMask)&1u))return;
    geometryLog("CONTACT handle="+std::to_string(item.handle)
        +" world="+geometryFloats(ray.worldPoint.data(),3)+" depth="+std::to_string(ray.rayDistance)
        +" miss="+std::to_string(ray.missDistance)+" offset="+geometryFloats(offset.data(),3)
        +" local_axis="+geometryFloats(axis.data(),9)+" visual_mesh_verified="+std::to_string(ray.exactVisualMeshHit));
}
#else
void grappleGeometryInvestigatorTick(const ObjectDiscoveryGripFrame&,bool,unsigned long long,unsigned long long){}
void grappleGeometryInvestigatorAcquired(const ObjectDiscoveryGripFrame&,const ObjectDiscoveryNativeRecord&,unsigned long long){}
void grappleGeometryInvestigatorContact(const ObjectDiscoveryGripFrame&,const ObjectDiscoveryNativeRecord&,const kharvox::GrappleRayAnchor&,const std::array<float,3>&,const std::array<float,9>&){}
#endif
