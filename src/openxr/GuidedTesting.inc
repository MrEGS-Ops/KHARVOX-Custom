// Offline, read-only guided testing. XR-thread state only. Native evidence
// crosses threads as counters and snapshots, never as engine pointers.
struct GuidedStep {const char* text;unsigned event;};
struct GuidedPlan {const char* title;std::array<GuidedStep,5> steps;unsigned count;};
enum GuidedEvent:unsigned {GuideScan,GuideMotion,GuideGrip,GuideHold,GuideRelease,GuideCollect,GuideVisual,GuideMelee,GuideHorizontal,GuideVertical};
GuidedPlan guidedPlan(unsigned id){
    using S=GuidedStep;
    if(id<5){
        const char* titles[]{"OBJECT DISCOVERY","GRAPPLE GRAB","ORIGINAL MELEE","PISTOL GRAPPLE","DYNAMIC SURFACE OFFSET"};
        if(id==2||id==3)return {titles[id],{{S{"Aim at a stationary non-explosive prop; hold Grip",GuideGrip},S{"Keep Grip held until the object attaches",GuideHold},S{"Release Grip normally",GuideRelease},S{"Check weapon / contact; Select PASS or FAIL",GuideVisual}}},4};
        return {titles[id],{{S{"Look around until nearby objects are discovered",GuideScan},S{"Kick a nearby non-explosive prop",GuideMotion},S{"Aim and hold Grip until the object attaches",GuideHold},S{"Release Grip normally",GuideRelease},S{"Check contact / drop; Select PASS or FAIL",GuideVisual}}},5};
    }
    const unsigned group=(id-5)%6;const bool test=id>=11;
    const char* titles[]{"MESH / SURFACE RAY","COLLISION / STATIC WORLD","ACTIVATION / CALLBACK","HOLD / RELEASE","PLAYER MOVEMENT","NATIVE LEDGE CHECKS"};
    const char* actions[]{"Aim support hand at a prop; hold Grip","Aim support hand at a prop face; hold Grip","Aim weapon hand at stationary prop; melee normally","Kick a non-explosive prop; grab and release it","Walk, strafe and jump; try a solid wall","Perform a normal native jump and mantle"};
    if(test)return {titles[group],{{S{"Kick a non-explosive prop; grab and hold it",GuideHold},S{"Release Grip normally",GuideRelease},S{"Does it drop naturally? Select PASS or FAIL",GuideVisual}}},3};
    if(group==4)return {titles[group],{{S{"Walk or strafe on level ground",GuideHorizontal},S{"Jump and land; observe vertical movement",GuideVertical}}},2};
    return {titles[group],{{S{actions[group],GuideCollect}}},1};
}
struct GuidedRun {
    unsigned id{99},step{},finished{},activeMask{},queued{},failures{},inconclusive{},manual{};
    unsigned evidenceBase{},releaseBase{},horizontalBase{},verticalBase{};
    unsigned completedGroups{},failedGroups{},skippedGroups{};
    unsigned long long generation{},stepStart{},lastTick{},elapsed{},completeAt{},lastSample{};
    bool priorGrip{},priorMelee{},held{},timedOut{};
    std::string previous;
};
GuidedRun guidedRun{};
constexpr int guideX=94,guideY=12,guideW=772,guideH=164;
constexpr int guideButtonY=127,guideButtonH=32,guideButtonStep=188,guideButtonW=182;
int guidedHover=-1;
float guidedCursorX=-1.f,guidedCursorY=-1.f;
bool guidedTriggerPreviously{},guidedTriggerOwned{};
kharvox::hands::HandPose menuAimRay{};
float menuAimRayMetres{};
bool guidedCalibrationVisible(){
    const auto& m=s.vrHandMenu;
    return s.objectGripCalibrating||m.mode==kharvox::VrHandMenuMode::Calibrating;
}
bool guidedOverlayVisible(){
#if KHARVOX_EXPERIMENTAL_WORLD_GRAB
    return (s.vrHandMenu.guidedTestingEnabled||s.vrHandMenu.researchQueueRunning)&&guidedRun.id<17
        &&(!guidedRun.completeAt||GetTickCount64()-guidedRun.completeAt<4000);
#else
    return false;
#endif
}
#if KHARVOX_EXPERIMENTAL_WORLD_GRAB
unsigned guidedEnabledMask(){
    const auto& m=s.vrHandMenu;
    unsigned mask=(m.nativeResearchMask<<5)|((m.nativeResearchTestMask&8u)<<11);
    if(mask)return mask;
    if(!m.activationInvestigator){
        if(m.dynamicSurfaceOffset)mask|=1u<<4;
        else if(m.pistolGrappleEnabled)mask|=1u<<3;
        else if(m.objectMeleeActivation)mask|=1u<<2;
        else if(m.grappleAnchor.enabled)mask|=1u<<1;
        else if(m.objectDiscovery.enabled)mask|=1u;
    }
    return mask;
}
void guidedBeginStep(const KharvoxGuidedEvidence& e,unsigned long long now){
    auto& g=guidedRun;
    g.timedOut=false;g.stepStart=now;g.elapsed=0;g.lastTick=now;g.releaseBase=e.releaseCount;g.horizontalBase=e.horizontalMotion;g.verticalBase=e.verticalMotion;
    if(g.id>=5&&g.id<17){const unsigned group=(g.id-5)%6;g.evidenceBase=g.id>=11?e.test[group]:e.collect[group];}
}
void guidedAdvance(const char* result,const KharvoxGuidedEvidence& e,unsigned long long now){
    auto& g=guidedRun;if(g.id>=17)return;
    const auto plan=guidedPlan(g.id);if(g.step>=plan.count)return;
    const std::string status=result;
    if(status=="FAIL"||status=="USER_FAIL")++g.failures;
    else if(status=="TIMEOUT"||status=="SKIP")++g.inconclusive;
    else if(status=="USER_OK"||status=="USER_OBSERVATION")++g.manual;
    g.previous=status;
    log("[GUIDED-TEST] group="+std::to_string(g.id)+" step="+std::to_string(g.step+1)
        +" outcome="+status+" action="+plan.steps[g.step].text+" elapsed_ms="+std::to_string(g.elapsed));
    ++g.step;
    if(g.step>=plan.count){
        g.finished|=1u<<g.id;g.completeAt=now;
        if(g.failures)++g.failedGroups;else if(g.inconclusive)++g.skippedGroups;else ++g.completedGroups;
        if(g.id>=5&&!(g.queued&~g.finished)){
            g.previous="Queue complete: "+std::to_string(g.completedGroups)+" done / "+std::to_string(g.failedGroups)+" failed / "+std::to_string(g.skippedGroups)+" skipped";
            log("[GUIDED-TEST] "+g.previous);
        }
        log("[GUIDED-TEST] end group="+std::to_string(g.id)+" failures="+std::to_string(g.failures)
            +" inconclusive="+std::to_string(g.inconclusive)+" manual="+std::to_string(g.manual)+" diagnostic_only=1");
    }else guidedBeginStep(e,now);
}
void guidedApplyResearchSelection(bool gameplay){
    auto& m=s.vrHandMenu;const auto& g=guidedRun;
    m.activeResearchMask=0;m.activeResearchTestMask=0;
    m.researchQueueRunning=gameplay&&((g.queued&~g.finished&~31u)!=0||(g.id>=5&&g.id<17));
    if(gameplay&&m.mode==kharvox::VrHandMenuMode::Closed&&!g.completeAt&&g.id>=5&&g.id<17){
        const unsigned bit=1u<<((g.id-5)%6);
        if(g.id<11)m.activeResearchMask=m.nativeResearchMask&bit;
        else m.activeResearchTestMask=m.nativeResearchTestMask&bit;
    }
    m.activationInvestigator=((m.activeResearchMask|m.activeResearchTestMask)&4u)!=0;
    m.releaseInvestigator=((m.activeResearchMask|m.activeResearchTestMask)&8u)!=0;
}
bool guidedResponseVisible(){
    return guidedOverlayVisible()&&!guidedRun.completeAt&&s.vrHandMenu.mode==kharvox::VrHandMenuMode::Closed&&!s.objectGripCalibrating;
}
void guidedTick(bool gameplay,bool grip,bool melee,bool held,const KharvoxObjectGripFeedback& feedback,unsigned long long generation){
    auto& g=guidedRun;auto& m=s.vrHandMenu;
    const auto now=GetTickCount64();
    const auto evidence=KharvoxCameraGuidedEvidence();
    if(g.generation!=generation){g={};g.generation=generation;}
    if(!m.guidedTestingEnabled&&!(m.nativeResearchMask|m.nativeResearchTestMask)){g={};g.generation=generation;m.guidedCommand=0;return;}
    const unsigned mask=guidedEnabledMask();
    const unsigned added=mask&~g.activeMask;
    if((mask&~31u)&&!(g.activeMask&~31u)){g.completedGroups=g.failedGroups=g.skippedGroups=0;}
    g.finished&=mask;g.queued=(g.queued|added)&mask;g.activeMask=mask;
    if(g.id<17&&!(mask&(1u<<g.id))){log("[GUIDED-TEST] cancelled group="+std::to_string(g.id)+" completed_steps="+std::to_string(g.step)+" remaining=UNASSESSED data_retained=1");g.id=99;g.completeAt=0;}
    const bool playable=gameplay&&m.mode==kharvox::VrHandMenuMode::Closed&&!s.objectGripCalibrating;
    const auto dt=playable&&g.lastTick&&now>=g.lastTick?std::min(250ULL,now-g.lastTick):0ULL;
    g.lastTick=now;
    if(!playable)return;
    if(g.id>=17||(g.completeAt&&now-g.completeAt>=4000)){
        g.id=99;g.completeAt=0;
        unsigned pending=g.queued&~g.finished;
        if(pending){
            unsigned next=0;
            const unsigned research=pending&~31u;if(research)pending=research;
            while(next<17&&!(pending&(1u<<next)))++next;
            g.id=next;g.queued&=~(1u<<next);g.step=0;g.failures=0;g.inconclusive=0;g.manual=0;g.previous.clear();
            guidedBeginStep(evidence,now);
            log("[GUIDED-TEST] begin group="+std::to_string(next)+" mode="+(next>=11?"test":next>=5?"collect":"regression"));
        }
    }
    const unsigned command=m.guidedCommand;m.guidedCommand=0;
    if(g.id>=17)return;
    const auto plan=guidedPlan(g.id);
    if(command==1){
        if(g.completeAt){g.step=0;g.completeAt=0;g.failures=g.inconclusive=g.manual=0;g.finished&=~(1u<<g.id);}
        g.previous="Retry requested";guidedBeginStep(evidence,now);log("[GUIDED-TEST] retry group="+std::to_string(g.id));return;
    }
    if(g.completeAt)return;
    const auto event=plan.steps[g.step].event;
    if(command==2){guidedAdvance("SKIP",evidence,now);return;}
    if(command==3||command==4){
        if(command==4)guidedAdvance("USER_FAIL",evidence,now);
        else if(event==GuideVisual)guidedAdvance("USER_OK",evidence,now);
        else{g.previous="Waiting for native evidence; use Retry or Skip";log("[GUIDED-TEST] manual pass cannot substitute native evidence");}
        return;
    }
    if(g.id==5&&!g.timedOut){
        const char* meshStates[]{"Waiting for Grip", "Grip received; searching", "No nearby ray candidate", "Executable signature mismatch", "Model or surface list unavailable", "Model found; layout not validated", "Candidate mesh decoded"};
        g.previous=meshStates[std::min(evidence.meshStage,6u)];
        if(evidence.meshHandle)g.previous+=" | ID "+std::to_string(evidence.meshHandle);
        if(evidence.meshRawBytes)g.previous+=" | raw "+std::to_string(evidence.meshRawBytes)+" bytes";
    }
    if(g.timedOut)return;
    g.elapsed+=dt;
    bool observed=false;
    if(event==GuideGrip)observed=grip;
    else if(event==GuideHold)observed=held&&feedback.nativeMotionVerified;
    else if(event==GuideRelease)observed=evidence.releaseCount!=g.releaseBase;
    else if(event==GuideMelee)observed=melee&&!g.priorMelee;
    else if(event==GuideHorizontal)observed=evidence.horizontalMotion!=g.horizontalBase;
    else if(event==GuideVertical)observed=evidence.verticalMotion!=g.verticalBase;
    else if(event==GuideCollect){
        const unsigned group=(g.id-5)%6;
        observed=evidence.collect[group]!=g.evidenceBase;
    }else if((event==GuideScan||event==GuideMotion)&&(!g.lastSample||now-g.lastSample>=100)){
        g.lastSample=now;KharvoxObjectDiscoverySnapshot snapshot{};
        if(KharvoxCameraReadObjectDiscovery(snapshot)&&snapshot.generation==generation)
            for(unsigned j=0;j<snapshot.count&&j<snapshot.candidates.size();++j){
                const auto& item=snapshot.candidates[j];
                if(item.observedAtMs>g.stepStart&&now>=item.observedAtMs&&now-item.observedAtMs<500
                    &&(event==GuideScan||item.nativeMovedAtMs>g.stepStart)){observed=true;break;}
            }
    }
    g.priorGrip=grip;g.priorMelee=melee;g.held=held;
    if(observed){const bool raw=event==GuideCollect&&g.id==5&&evidence.meshStage!=6;guidedAdvance(raw?"RAW_DATA_RECORDED_NOT_VALIDATED":event==GuideCollect?"DATA_RECORDED":"ACTION_OBSERVED",evidence,now);return;}
    if(event==GuideVisual)return; // explicit answer, no timeout
    const unsigned long long limit=event==GuideCollect?35000:20000;
    if(g.elapsed>=limit){g.timedOut=true;g.previous="TIMEOUT: "+(g.id==5?g.previous:std::string("No required action observed"))+"; Retry or Skip";
        log("[GUIDED-TEST] timeout group="+std::to_string(g.id)+" step="+std::to_string(g.step+1)+" waiting_for="+plan.steps[g.step].text);}
}
#else
bool guidedResponseVisible(){return false;}
#endif
