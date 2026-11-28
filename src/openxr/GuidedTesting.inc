// Offline, read-only guided testing. XR-thread state only. Native evidence
// crosses threads as counters and snapshots, never as engine pointers.
struct GuidedStep {const char* text;unsigned event;};
struct GuidedPlan {const char* title;std::array<GuidedStep,5> steps;unsigned count;};
enum GuidedEvent:unsigned {GuideScan,GuideMotion,GuideGrip,GuideHold,GuideRelease,GuideCollect,GuideMelee,GuideHorizontal,GuideVertical};
GuidedPlan guidedPlan(unsigned id){
    using S=GuidedStep;
    if(id<5){
        const char* titles[]{"OBJECT DISCOVERY","GRAPPLE GRAB","ORIGINAL MELEE","PISTOL GRAPPLE","DYNAMIC SURFACE OFFSET"};
        if(id==2||id==3)return {titles[id],{{S{"Aim at a stationary non-explosive prop; hold Grip",GuideGrip},S{"Keep Grip held until the object attaches",GuideHold},S{"Release Grip normally",GuideRelease}}},3};
        return {titles[id],{{S{"Look around until nearby objects are discovered",GuideScan},S{"Kick a nearby non-explosive prop",GuideMotion},S{"Aim and hold Grip until the object attaches",GuideHold},S{"Release Grip normally",GuideRelease}}},4};
    }
    const unsigned group=(id-5)%6;const bool test=id>=11;
    const char* titles[]{"MESH / SURFACE RAY","COLLISION / STATIC WORLD","ACTIVATION / CALLBACK","HOLD / RELEASE","PLAYER MOVEMENT","NATIVE LEDGE CHECKS"};
    const char* actions[]{"Aim support hand at a prop; hold Grip","Aim support hand at a prop face; hold Grip","Aim weapon hand at stationary prop; melee normally","Kick a non-explosive prop; grab and release it","Walk, strafe and jump; try a solid wall","Perform a normal native jump and mantle"};
    if(test&&group==3)return {titles[group],{{S{"Kick a non-explosive prop; grab and hold it",GuideHold},S{"Release Grip normally; repeat as needed",GuideRelease}}},2};
    if(group==4)return {titles[group],{{S{"Walk or strafe on level ground",GuideHorizontal},S{"Jump and land; observe vertical movement",GuideVertical}}},2};
    return {titles[group],{{S{actions[group],GuideCollect}}},1};
}
struct GuidedRun {
    unsigned id{99},step{},finished{},activeMask{},queued{},capturedSteps{},skippedSteps{};
    unsigned evidenceBase{},releaseBase{},horizontalBase{},verticalBase{};
    unsigned completedGroups{},partialGroups{},skippedGroups{};
    unsigned long long generation{},stepStart{},lastTick{},elapsed{},completeAt{},lastSample{};
    bool priorGrip{},priorMelee{},held{},observed{};
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
    g.observed=false;g.lastSample=0;g.stepStart=now;g.elapsed=0;g.lastTick=now;g.releaseBase=e.releaseCount;g.horizontalBase=e.horizontalMotion;g.verticalBase=e.verticalMotion;
    if(g.id>=5&&g.id<17){const unsigned group=(g.id-5)%6;g.evidenceBase=e.collect[group];}
}
void guidedAdvance(bool captured,const KharvoxGuidedEvidence& e,unsigned long long now){
    auto& g=guidedRun;if(g.id>=17)return;
    const auto plan=guidedPlan(g.id);if(g.step>=plan.count)return;
    const char* status=captured?"DATA_RECORDED":"SKIPPED_NO_DATA";
    if(captured)++g.capturedSteps;else ++g.skippedSteps;
    g.previous=status;
    log("[GUIDED-TEST] group="+std::to_string(g.id)+" step="+std::to_string(g.step+1)
        +" outcome="+status+" action="+plan.steps[g.step].text+" elapsed_ms="+std::to_string(g.elapsed)+" data_retained=1");
    ++g.step;
    if(g.step>=plan.count){
        g.finished|=1u<<g.id;g.completeAt=now;
        if(!g.capturedSteps)++g.skippedGroups;else if(g.skippedSteps)++g.partialGroups;else ++g.completedGroups;
        log("[GUIDED-TEST] end group="+std::to_string(g.id)+" captured_steps="+std::to_string(g.capturedSteps)
            +" skipped_steps="+std::to_string(g.skippedSteps)+" diagnostic_only=1");
        if(g.id>=5&&!(g.queued&~g.finished)){
            g.previous="Queue complete: "+std::to_string(g.completedGroups)+" captured / "+std::to_string(g.partialGroups)+" partial / "+std::to_string(g.skippedGroups)+" no data";
            log("[GUIDED-TEST] "+g.previous);
        }
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
    if((mask&~31u)&&!(g.activeMask&~31u)){g.completedGroups=g.partialGroups=g.skippedGroups=0;}
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
            g.id=next;g.queued&=~(1u<<next);g.step=0;g.capturedSteps=0;g.skippedSteps=0;g.previous.clear();
            guidedBeginStep(evidence,now);
            log("[GUIDED-TEST] begin group="+std::to_string(next)+" mode="+(next>=11?"test":next>=5?"collect":"regression"));
        }
    }
    const unsigned command=m.guidedCommand;m.guidedCommand=0;
    if(g.id>=17)return;
    const auto plan=guidedPlan(g.id);
    if(g.completeAt)return;
    const auto event=plan.steps[g.step].event;
    if(g.id==5){
        const char* meshStates[]{"Waiting for Grip", "Grip received; searching", "No nearby ray candidate", "Executable signature mismatch", "Model or surface list unavailable", "Model found; layout not validated", "Candidate mesh decoded"};
        g.previous=meshStates[std::min(evidence.meshStage,6u)];
        if(evidence.meshHandle)g.previous+=" | ID "+std::to_string(evidence.meshHandle);
        if(evidence.meshRawBytes)g.previous+=" | raw "+std::to_string(evidence.meshRawBytes)+" bytes";
    }
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
    // A capture is sticky until the user advances. Native observers keep
    // running, including after a miss or an unsuccessful action.
    if(g.id>=5&&g.id<17&&evidence.collect[(g.id-5)%6]!=g.evidenceBase)observed=true;
    if(observed&&!g.observed){
        g.observed=true;
        log("[GUIDED-TEST] evidence captured group="+std::to_string(g.id)+" step="+std::to_string(g.step+1));
    }
    if(g.observed){
        if(g.id==5)g.previous="Data captured | "+g.previous;
        else g.previous="Data captured; repeat or select NEXT";
    }else if(g.id!=5)g.previous="Recording; NEXT or SKIP whenever ready";
    if(command==5){
        log("[GUIDED-TEST] user_end group="+std::to_string(g.id)+" step="+std::to_string(g.step+1)
            +" current_data="+std::to_string(g.observed)+" data_retained=1");
        m.guidedTestingEnabled=false;m.nativeResearchMask=0;m.nativeResearchTestMask=0;
        m.activeResearchMask=0;m.activeResearchTestMask=0;m.researchQueueRunning=false;
        m.activationInvestigator=false;m.releaseInvestigator=false;
        g={};g.generation=generation;return;
    }
    if(command==2||command==3)guidedAdvance(g.observed,evidence,now);

}
#else
bool guidedResponseVisible(){return false;}
#endif
