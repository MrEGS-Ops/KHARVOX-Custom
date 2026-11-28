// Shared controls for the radial render and its pointer hit testing.
using RadialMod=kharvox::VrLiveMod;
enum class RadialControl { Button, Stepper, Toggle, Choice, Reset };
struct RadialSetting {
    const char* label;
    RadialMod mod;
    int action;
    RadialControl control=RadialControl::Button;
};
struct RadialGroup { const char* name; std::vector<RadialSetting> settings; };
const RadialGroup radialGroups[]{
    {"Weapons",{
        {"Grip calibration",RadialMod::WeaponGrip,130}}},
    {"Movement",{
        {"Turn speed",RadialMod::TurnSpeed,130,RadialControl::Stepper},
        {"Reset turn",RadialMod::TurnSpeed,132,RadialControl::Reset},
        {"Physical Crouch",RadialMod::PhysicalCrouch,110,RadialControl::Toggle},
        {"Set Physical Crouch",RadialMod::PhysicalCrouch,130},
        {"Reset heights",RadialMod::PhysicalCrouch,132,RadialControl::Reset}}},
    {"HUD / Stats",{{"Edit HUD",RadialMod::Stats,1},{"Level records",RadialMod::Stats,2}}},
    {"PAINSAW",{}},
    {"Glory Kills",{
        {"Punch threshold",RadialMod::PhysicalGloryKills,130,RadialControl::Stepper},
        {"Punch hands",RadialMod::PhysicalGloryKills,132,RadialControl::Choice},
        {"Reset punch",RadialMod::PhysicalGloryKills,133,RadialControl::Reset},
        {"KILLTIME",RadialMod::Killtime,130,RadialControl::Stepper},
        {"Reset KILLTIME",RadialMod::Killtime,132,RadialControl::Reset}}},
    {"Gunslinger",{
        {"Set holsters",RadialMod::Gunslinger,130},
        {"Set wheel zones",RadialMod::Gunslinger,131},
        {"Reset zones",RadialMod::Gunslinger,132,RadialControl::Reset},
        {"Tutorial",RadialMod::Gunslinger,140,RadialControl::Toggle}}}
};
static_assert(sizeof(radialGroups)/sizeof(*radialGroups)==kharvox::radialCategoryCount);
int radialTarget(float x,float y){
    const int category=s.vrHandMenu.radialCategory;
    const int count=kharvox::radialHasFlyout(category)
        ?int(radialGroups[category].settings.size()):0;
    return kharvox::radialHit(x,y,category,count);
}
void openCalibrationWheel(){
    auto& m=s.vrHandMenu;
    if(m.weaponCalibrationStage)finishVrWeaponSequence();
    m.mode=kharvox::VrHandMenuMode::Menu;
    m.page=kharvox::VrMenuPage::WeaponGrip;
    KharvoxHudResetWeaponWheelPresentation();
    m.calibrationWheel=true;
    m.calibrationWheelLeft=s.weaponHandLeft;
    m.wheelAxisX=m.wheelAxisY=0.f;
    logVrCalibrationWheelYaw("open");
    m.waitingForEquip=false;
    m.resetWeaponPending=false;
    m.alternateProfile=false;
    m.supportPreviewHeld=false;
}
void radialInput(int hit,bool click,bool rightGrip){
    using namespace kharvox;
    auto& m=s.vrHandMenu;
    const bool center=hit==radialCenter;
    const bool openDev=m.radialDevUnlock.update(center,rightGrip,click);
    if(hit<100||hit>=100+radialCategoryCount)m.suppressedCategory=-1;
    const int description=hit>=100&&hit<100+radialCategoryCount&&radialHasFlyout(hit-100)?hit-100:
        (hit>=radialRowTarget&&m.radialPinned?m.radialCategory:-1);
    const auto now=GetTickCount64();
    if(description>=0){
        if(description!=m.radialDescription||m.radialDescriptionFadeOut){
            m.radialDescription=description;m.radialDescriptionSince=now;
        }
        m.radialDescriptionFadeOut=0;
    }else if(m.radialDescription>=0&&!m.radialDescriptionFadeOut){
        m.radialDescriptionFadeOut=now;
    }
    if(hit>=100&&hit<100+radialCategoryCount){
        const int i=hit-100;
        if(click&&i==radialPainsawIndex){
            m.selectedMod=RadialMod::Painsaw;
            handleVrLiveMenuAction(110);
        }else if(click&&m.radialPinned&&m.radialCategory==i){
            m.radialPinned=false;
            m.radialCategory=-1;
            m.radialResetRow=-1;
            m.message.clear();
            m.suppressedCategory=i;
        }else if(i!=m.suppressedCategory&&radialHasFlyout(i)&&(click||!m.radialPinned)){
            if(m.radialCategory!=i){
                m.radialResetRow=-1;
                m.message.clear();
            }
            m.radialCategory=i;
            m.radialPinned=click;
        }
    }else if(click&&center){
        if(openDev){
            m.page=VrMenuPage::Dev;
            m.selectedRow=0;
            m.modScroll=0;
            m.navigation={};
            m.radialResetRow=-1;
        }else if(!rightGrip){
            m.mode=VrHandMenuMode::Closed;
        }
        m.consumeMenuTriggerUntilRelease=true;
    }else if(click&&hit>=radialRowTarget&&m.radialCategory>=0){
        const int row=(hit-radialRowTarget)/3;
        const int part=(hit-radialRowTarget)%3;
        if(!radialHasFlyout(m.radialCategory)||row<0||row>=int(radialGroups[m.radialCategory].settings.size()))return;
        const auto& setting=radialGroups[m.radialCategory].settings[row];
        m.consumeMenuTriggerUntilRelease=true;
        if(setting.control==RadialControl::Stepper&&part==1)return;
        if(setting.control==RadialControl::Reset){
            if(m.radialResetRow!=row){
                m.radialResetRow=row;
                m.radialPinned=true;
                m.message="Restore defaults?";
                return;
            }
            if(part!=0){
                if(part==2){m.radialResetRow=-1;m.message.clear();}
                return;
            }
        }
        m.radialResetRow=-1;
        m.message.clear();
        m.radialPinned=true;
        m.selectedMod=setting.mod;
        if(setting.mod==RadialMod::Gunslinger&&setting.action==140){
            const auto before=s.gunslingerTutorial;
            kharvox::gunslingerTutorialToggle(s.gunslingerTutorial);
            if(!saveGunslingerTutorial()){
                s.gunslingerTutorial=before;m.message="Could not save tutorial setting.";
            }else m.message=s.gunslingerTutorial.enabled?"Tutorial ready. Close menu to begin.":"Tutorial OFF.";
        }else if(setting.mod==RadialMod::PhysicalCrouch&&setting.action==130)openVrCombinedCrouch();
        else if(setting.mod==RadialMod::WeaponGrip)openCalibrationWheel();
        else if(setting.mod==RadialMod::Stats){
            m.page=setting.action==1?VrMenuPage::Stats:VrMenuPage::Records;
            m.hudDraft=s.vrLiveSettings;
            m.hudDrag=m.hudSelection=-1;
            m.navigation={};
        }else{
            const int action=setting.action+(setting.control==RadialControl::Stepper&&part==2?1:0);
            handleVrLiveMenuAction(action);
        }
    }
}

int hudTrayY(int item){return 710-(7-item)*37;}
int hudTarget(int x,int y){
    const auto& m=s.vrHandMenu;
    if(y>=660&&y<708&&x>=220&&x<640)return 100+(x-220)/140;
    for(int i=0;i<7;++i){
        const bool on=kharvox::vrStatsEnabled(m.hudDraft,i);
        const int p=kharvox::vrStatsCorner(m.hudDraft,i),w=kharvox::campaignStatsPanelWidth(i);
        const int left=on?kharvox::statsPanelX(p,960,w):700;
        const int top=on?kharvox::statsPanelY(p,720,60):hudTrayY(i);
        if(x>=left&&x<left+(on?w:254)&&y>=top&&y<top+(on?60:35))return i;
    }
    return -1;
}
void hudInput(int x,int y,bool press,bool down){
    auto& m=s.vrHandMenu;
    const int hit=hudTarget(x,y);
    if(press&&hit>=100){
        if(hit==102){if(m.hudSelection>=0)kharvox::vrStatsSetEnabled(m.hudDraft,m.hudSelection,false);return;}
        if(hit==100){
            if(!kharvox::vrLiveSave(kharvox::runtimePathA("vr_live_mods_saved.cfg"),m.hudDraft)){m.message="HUD save failed";return;}
            s.vrLiveSettings=m.hudDraft;
        }
        m.mode=kharvox::VrHandMenuMode::Closed;m.consumeMenuTriggerUntilRelease=true;return;
    }
    if(press&&hit>=0&&hit<7){
        const bool on=kharvox::vrStatsEnabled(m.hudDraft,hit);
        const int p=kharvox::vrStatsCorner(m.hudDraft,hit),w=kharvox::campaignStatsPanelWidth(hit);
        m.hudSelection=m.hudDrag=hit;
        m.hudOffsetX=x-(on?kharvox::statsPanelX(p,960,w):700);
        m.hudOffsetY=y-(on?kharvox::statsPanelY(p,720,60):hudTrayY(hit));
    }
    if(m.hudDrag<0)return;
    const int item=m.hudDrag,w=kharvox::campaignStatsPanelWidth(item);
    if(x>=0&&y>=0){
        m.hudX=std::clamp(x-m.hudOffsetX,0,960-w);
        m.hudY=std::clamp(y-m.hudOffsetY,0,660);
    }
    if(down)return;
    m.hudDrag=-1;
    if(x<0||y<0||x>=960||y>=720)return;
    if(x>=700&&y>=hudTrayY(0)){kharvox::vrStatsSetEnabled(m.hudDraft,item,false);return;}
    if((m.hudX<640&&m.hudX+w>220&&m.hudY+60>660)
        ||(m.hudX+w>700&&m.hudY+60>hudTrayY(0)))return;
    kharvox::vrStatsSetCorner(m.hudDraft,item,kharvox::statsPixelPosition(m.hudX,m.hudY));
    kharvox::vrStatsSetEnabled(m.hudDraft,item,true);
}
