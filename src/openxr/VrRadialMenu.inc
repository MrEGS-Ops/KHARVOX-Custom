// Shared menu geometry in compositor texture pixels.
using RadialMod=kharvox::VrLiveMod;
struct RadialSetting { const char* label; RadialMod mod; int action; };
struct RadialGroup { const char* name; std::vector<RadialSetting> settings; };
const RadialGroup radialGroups[]={
    {"Weapons",{{"Weapon grip calibration",RadialMod::WeaponGrip,130},{"Gauss two-hand firing",RadialMod::WeaponGrip,33}}},
    {"Movement",{{"Slower turn",RadialMod::TurnSpeed,130},{"Faster turn",RadialMod::TurnSpeed,131},{"Reset turn speed",RadialMod::TurnSpeed,132},{"Standing height",RadialMod::PhysicalCrouch,130},{"Crouching height",RadialMod::PhysicalCrouch,131},{"Reset heights",RadialMod::PhysicalCrouch,132}}},
    {"HUD / Stats",{{"Edit HUD",RadialMod::Stats,1},{"Level records",RadialMod::Stats,2}}},
    {"Gunslinger",{{"Set holsters",RadialMod::Gunslinger,130},{"Set wheel zones",RadialMod::Gunslinger,131},{"Reset zones",RadialMod::Gunslinger,132}}},
    {"Glory Kills",{{"Lower punch threshold",RadialMod::PhysicalGloryKills,130},{"Raise punch threshold",RadialMod::PhysicalGloryKills,131},{"Change punching hands",RadialMod::PhysicalGloryKills,132},{"Reset punch settings",RadialMod::PhysicalGloryKills,133},{"Slower KILLTIME",RadialMod::Killtime,130},{"Faster KILLTIME",RadialMod::Killtime,131},{"Reset KILLTIME",RadialMod::Killtime,132}}},
    {"PAINSAW",{}},{"Physical Crouch",{}}
};
constexpr double radialStep=6.283185307179586/7.0;
double radialAngle(int i){return -1.570796326794897+i*radialStep;}
int radialPanelX(int i){return std::cos(radialAngle(i))<-.01?8:692;}
int radialTarget(float x,float y){
    const auto& m=s.vrHandMenu;
    if(m.radialCategory>=0&&m.radialCategory<5){
        const int left=radialPanelX(m.radialCategory);
        if(x>=left&&x<left+260&&y>=170){
            const int index=int((y-170)/49);
            if(index<int(radialGroups[m.radialCategory].settings.size()))return 200+index;
        }
    }
    const float dx=x-480,dy=y-360,rr=dx*dx+dy*dy;
    if(rr<60*60)return 99;
    if(rr>202*202)return -1;
    double angle=std::atan2(dy,dx)-radialAngle(0)+radialStep/2;
    if(angle<0)angle+=7*radialStep;
    return 100+int(angle/radialStep)%7;
}
void openCalibrationWheel(){
    auto& m=s.vrHandMenu;
    if(m.weaponCalibrationStage)finishVrWeaponSequence();
    m.mode=kharvox::VrHandMenuMode::Menu;
    m.page=kharvox::VrMenuPage::WeaponGrip;
    m.calibrationWheel=true;m.waitingForEquip=false;m.resetWeaponPending=false;
    m.supportPreviewHeld=false;
}
void radialInput(int hit,bool click,bool devChord){
    auto& m=s.vrHandMenu;
    if(hit<100||hit>=107)m.suppressedCategory=-1;
    if(hit>=100&&hit<107){
        const int i=hit-100;
        if(click&&i>=5){
            m.selectedMod=i==5?RadialMod::Painsaw:RadialMod::PhysicalCrouch;
            handleVrLiveMenuAction(110);
        }else if(click&&m.radialPinned&&m.radialCategory==i){
            m.radialPinned=false;m.radialCategory=-1;m.suppressedCategory=i;
        }else if(i!=m.suppressedCategory&&(click||!m.radialPinned)){
            m.radialCategory=i;m.radialPinned=click&&i<5;
        }
    }else if(click&&hit==99){
        if(devChord){
            m.page=kharvox::VrMenuPage::Dev;m.selectedRow=0;m.modScroll=0;m.navigation={};
        }else m.mode=kharvox::VrHandMenuMode::Closed;
        m.consumeMenuTriggerUntilRelease=true;
    }else if(click&&hit>=200&&m.radialCategory>=0){
        const auto& setting=radialGroups[m.radialCategory].settings.at(hit-200);
        m.selectedMod=setting.mod;
        if(setting.mod==RadialMod::WeaponGrip&&setting.action==33){
            const bool previous=s.vrLiveSettings.gaussRequireTwoHandFire;
            s.vrLiveSettings.gaussRequireTwoHandFire=!previous;
            if(!saveVrLiveSettings())s.vrLiveSettings.gaussRequireTwoHandFire=previous;
            else if(!m.radialPinned)m.mode=kharvox::VrHandMenuMode::Closed;
        }else if(setting.mod==RadialMod::WeaponGrip)openCalibrationWheel();
        else if(setting.mod==RadialMod::Stats){
            m.page=setting.action==1?kharvox::VrMenuPage::Stats:kharvox::VrMenuPage::Records;
            m.hudDraft=s.vrLiveSettings;m.hudDrag=m.hudSelection=-1;m.navigation={};
        }else{
            handleVrLiveMenuAction(setting.action);
            if(!m.radialPinned&&m.mode==kharvox::VrHandMenuMode::Menu&&m.message=="Applied LIVE and saved to UserData.")
                m.mode=kharvox::VrHandMenuMode::Closed;
        }
        m.consumeMenuTriggerUntilRelease=true;
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
