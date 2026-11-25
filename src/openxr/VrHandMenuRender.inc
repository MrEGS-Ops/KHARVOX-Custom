// VR weapon-hand settings panel. Its own OpenXR quad never modifies DOOM HUD
// surfaces, and the Vulkan staging buffer is used only while frame submission
// waits for GPU completion. No external assets or ImGui dependency.
bool initializeVrHandMenuSurface(const std::vector<int64_t>& formats) {
    auto& menu=vrHandMenuSurface;
    if(menu.ready)return true;
    auto& vk=s.vk;
    constexpr int64_t format=VK_FORMAT_R8G8B8A8_SRGB;
    if(std::find(formats.begin(),formats.end(),format)==formats.end()
        ||!vk.createBuffer||!vk.destroyBuffer||!vk.allocateMemory||!vk.freeMemory
        ||!vk.getBufferMemoryRequirements||!vk.getPhysicalDeviceMemoryProperties
        ||!vk.bindBufferMemory||!vk.mapMemory||!vk.unmapMemory
        ||!vk.cmdCopyBufferToImage)return false;
    const VkDeviceSize bytes=vrMenuW*vrMenuH*4u;
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size=bytes;bi.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if(vk.createBuffer(s.device,&bi,nullptr,&menu.staging)!=VK_SUCCESS)return false;
    VkMemoryRequirements req{};
    vk.getBufferMemoryRequirements(s.device,menu.staging,&req);
    VkPhysicalDeviceMemoryProperties props{};
    vk.getPhysicalDeviceMemoryProperties(s.physical,&props);
    uint32_t memType=props.memoryTypeCount;
    for(uint32_t i=0;i<props.memoryTypeCount;++i){
        constexpr VkMemoryPropertyFlags flags=
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT|VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        if((req.memoryTypeBits&(1u<<i))
            &&(props.memoryTypes[i].propertyFlags&flags)==flags){
            memType=i;break;
        }
    }
    bool ok=false;
    if(memType<props.memoryTypeCount){
        VkMemoryAllocateInfo mi{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mi.allocationSize=req.size;mi.memoryTypeIndex=memType;
        if(vk.allocateMemory(s.device,&mi,nullptr,&menu.memory)==VK_SUCCESS
            &&vk.bindBufferMemory(s.device,menu.staging,menu.memory,0)==VK_SUCCESS
            &&vk.mapMemory(s.device,menu.memory,0,bytes,0,&menu.mapped)==VK_SUCCESS){
            XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            ci.usageFlags=XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT
                |XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            ci.format=format;ci.sampleCount=1;ci.width=vrMenuW;ci.height=vrMenuH;
            ci.faceCount=1;ci.arraySize=1;ci.mipCount=1;
            if(XR_SUCCEEDED(s.createSwapchain(s.session,&ci,&menu.target.handle))){
                menu.target.width=vrMenuW;menu.target.height=vrMenuH;
                ok=initializeSwapchainImages(menu.target);
            }
        }
    }
    if(!ok){
        if(menu.target.handle)s.destroySwapchain(menu.target.handle);
        if(menu.mapped)vk.unmapMemory(s.device,menu.memory);
        if(menu.staging)vk.destroyBuffer(s.device,menu.staging,nullptr);
        if(menu.memory)vk.freeMemory(s.device,menu.memory,nullptr);
        menu={};
    }
    menu.ready=ok;
    return ok;
}
void paintVrHandMenu() {
    if(!vrHandMenuSurface.ready||!vrHandMenuSurface.mapped)return;
    auto& m=s.vrHandMenu;
    BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=vrMenuW;
    info.bmiHeader.biHeight=-static_cast<LONG>(vrMenuH);
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    info.bmiHeader.biCompression=BI_RGB;
    HDC dc=CreateCompatibleDC(nullptr);if(!dc)return;
    void* bits{};
    HBITMAP dib=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!dib||!bits){if(dib)DeleteObject(dib);DeleteDC(dc);return;}
    const auto oldBitmap=SelectObject(dc,dib);
    auto fill=[&](int x,int y,int w,int h,COLORREF color){
        const RECT rect{x,y,x+w,y+h};
        HBRUSH brush=CreateSolidBrush(color);
        FillRect(dc,&rect,brush);
        DeleteObject(brush);
    };
    // The 960x720 texture stays shared with the mod menu, but the warning
    // itself occupies only this small top-centre banner. All other pixels
    // are transparent while the warning is shown.
    constexpr int bannerX=166,bannerY=174,bannerW=628,bannerH=108;
    constexpr int resultX=173,resultY=515,resultW=614,resultH=126;
    const bool disconnectView=controllerReconnectBannerVisible();
    const bool firstTimeSetupView=!disconnectView
        &&s.firstTimeSetupNotice.visible
        &&m.mode==kharvox::VrHandMenuMode::Closed;
    const bool gameplayWelcomeView=!disconnectView&&!firstTimeSetupView
        &&s.gameplayWelcomeNotice.visible
        &&m.mode==kharvox::VrHandMenuMode::Closed;
    const bool resultView=!disconnectView&&!firstTimeSetupView
        &&!gameplayWelcomeView&&m.mode==kharvox::VrHandMenuMode::Closed
        &&KharvoxHudEndOfLevelMenuActive()
        &&s.levelProgress.completed&&s.levelProgress.completedTimeMs>0
        &&kharvox::levelResultNoticeVisible(
            s.levelResultNoticeStartedAtMs.load(std::memory_order_acquire),
            GetTickCount64());
    const bool objectCalibrationView=!disconnectView
        &&!firstTimeSetupView&&!gameplayWelcomeView
        &&s.objectGripCalibrating;
    const bool statsGameplay=KharvoxCameraGameplayActive()
        &&KharvoxCameraWorldActive()&&!KharvoxCameraCutsceneActive()
        &&!KharvoxHudPauseMenuActive()&&!KharvoxHudFullscreenMenuActive()
        &&!KharvoxHudDeathMenuActive()&&!KharvoxHudPauseRootVisible()
        &&!KharvoxHudEndOfLevelMenuActive()
        &&!s.quadMode&&!s.centeredQuadTransitionPending
        &&!s.immersiveCinematicActive;
    const std::array<bool,kharvox::kVrStatsItemCount> statsVisible{
        s.vrLiveSettings.deathCounterEnabled,
        s.vrLiveSettings.deathStatisticsEnabled,
        s.vrLiveSettings.campaignTimerEnabled,
        s.vrLiveSettings.sessionTimerEnabled,
        s.vrLiveSettings.levelDisplayEnabled[0],
        s.vrLiveSettings.levelDisplayEnabled[1],
        s.vrLiveSettings.levelDisplayEnabled[2]};
    const std::array<int,kharvox::kVrStatsItemCount> statsCorners{
        s.vrLiveSettings.deathCounterCorner,
        s.vrLiveSettings.deathStatisticsCorner,
        s.vrLiveSettings.campaignTimerCorner,
        s.vrLiveSettings.sessionTimerCorner,
        s.vrLiveSettings.levelDisplayPosition[0],
        s.vrLiveSettings.levelDisplayPosition[1],
        s.vrLiveSettings.levelDisplayPosition[2]};
    const bool statsLiveView=kharvox::campaignStatsCanDraw(
        statsGameplay,m.mode==kharvox::VrHandMenuMode::Closed,
        firstTimeSetupView||gameplayWelcomeView,disconnectView)
        &&std::any_of(statsVisible.begin(),statsVisible.end(),
            [](bool on){return on;});
    constexpr int counterH=60;
    // These independent onboarding panels share one transparent menu Quad.
    constexpr int setupX=95,setupY=115,setupW=770,setupH=495;
    constexpr int welcomeX=95,welcomeY=145,welcomeW=770,welcomeH=435;
    const bool calibrationView=!disconnectView&&!gameplayWelcomeView
        &&!firstTimeSetupView
        &&m.mode==kharvox::VrHandMenuMode::Calibrating;
    const bool devPanel=kharvox::vrDevCalibrationPanelVisible(
        m.mode,m.liveCapture,m.devWeaponAlignment,
        m.devControllerInputs);
    if(disconnectView){
        fill(bannerX,bannerY,bannerW,bannerH,RGB(17,24,32));
        fill(bannerX,bannerY,6,bannerH,RGB(231,161,62));
        fill(bannerX+6,bannerY,bannerW-6,3,RGB(76,88,99));
    }else if(firstTimeSetupView){
        fill(setupX,setupY,setupW,setupH,RGB(16,29,39));
        fill(setupX,setupY,7,setupH,RGB(55,209,214));
        fill(setupX+7,setupY,setupW-7,4,RGB(66,91,105));
        fill(setupX+24,setupY+73,setupW-48,2,RGB(66,91,105));
    }else if(gameplayWelcomeView){
        fill(welcomeX,welcomeY,welcomeW,welcomeH,RGB(16,29,39));
        fill(welcomeX,welcomeY,7,welcomeH,RGB(55,209,214));
        fill(welcomeX+7,welcomeY,welcomeW-7,4,RGB(66,91,105));
        fill(welcomeX+24,welcomeY+75,welcomeW-48,2,RGB(66,91,105));
    }else if(resultView){
        const auto accent=s.levelProgress.bestUpdated
            ?RGB(255,207,66):RGB(55,209,214);
        fill(resultX,resultY,resultW,resultH,RGB(17,23,31));
        fill(resultX,resultY,7,resultH,accent);
        fill(resultX+7,resultY,resultW-7,3,accent);
    }else if(objectCalibrationView){
        fill(38,150,660,421,RGB(16,25,35));
        fill(38,150,6,421,RGB(55,209,214));
        fill(38,150,660,65,RGB(28,55,65));
    }else if(statsLiveView){
        if(statsLiveView)for(int i=0;i<kharvox::kVrStatsItemCount;++i)if(statsVisible[i]){
            const int width=kharvox::campaignStatsPanelWidth(i);
            const int x=kharvox::statsPanelX(statsCorners[i],vrMenuW,width);
            const int y=kharvox::statsPanelY(statsCorners[i],vrMenuH,counterH);
            fill(x,y,width,counterH,RGB(19,23,30));
            fill(x,y,4,counterH,i>=2&&i!=6?RGB(55,209,214):RGB(210,69,76));
        }
    }else if(calibrationView){
        // Compact left-anchored help box. Everything outside it is truly
        // transparent, so the live weapon can be inspected and aimed.
        fill(22,144,590,432,RGB(15,22,32));
        fill(22,144,590,74,RGB(27,48,61));
        fill(22,144,5,432,RGB(55,209,214));
        if(devPanel){
            fill(630,144,308,432,RGB(18,29,41));
            fill(630,144,5,432,RGB(55,209,214));
            fill(630,198,308,2,RGB(76,88,99));
        }
    }else{
        fill(0,0,vrMenuW,vrMenuH,RGB(15,18,24));
        fill(0,0,vrMenuW,84,RGB(24,29,40));
        fill(38,82,884,3,RGB(55,209,214));
    }
    SetBkMode(dc,TRANSPARENT);
    HFONT title=CreateFontA(-34,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,DEFAULT_PITCH,"Segoe UI");
    HFONT regular=CreateFontA(-24,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,DEFAULT_PITCH,"Segoe UI");
    HFONT smallFont=CreateFontA(-20,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,DEFAULT_PITCH,"Segoe UI");
    HGDIOBJ previous=SelectObject(dc,title);
    auto write=[&](int x,int y,const std::string& text,COLORREF color){
        SetTextColor(dc,color);
        TextOutA(dc,x,y,text.c_str(),static_cast<int>(text.size()));
    };
    const COLORREF white=RGB(239,246,251),muted=RGB(170,186,202);
    if(disconnectView){
        SelectObject(dc,regular);
        write(bannerX+24,bannerY+13,"CONTROLLERS DISCONNECTED",white);
        SelectObject(dc,smallFont);
        write(bannerX+24,bannerY+58,
            "Press L3 + R3 together to reconnect.",RGB(255,204,128));
    }else if(firstTimeSetupView){
        SelectObject(dc,title);
        write(setupX+28,setupY+19,"RIPSHIFT - FIRST-TIME SETUP",white);
        SelectObject(dc,regular);
        write(setupX+30,setupY+95,
            "For the best RIPSHIFT VR experience, we recommend",white);
        write(setupX+30,setupY+128,
            "disabling two settings in DOOM.",white);
        // Required first-time setup instruction: high-contrast warning red.
        write(setupX+44,setupY+201,"TUTORIALS - OFF (REQUIRED)",
            RGB(255,92,92));
        write(setupX+44,setupY+249,
            "GLORY KILL HIGHLIGHT - OFF (OPTIONAL)",white);
        SelectObject(dc,smallFont);
        write(setupX+30,setupY+321,
            "Go to DOOM > Settings > Game to change them manually.",muted);
        fill(setupX+25,setupY+376,setupW-50,55,RGB(28,57,66));
        SelectObject(dc,regular);
        write(setupX+43,setupY+390,
            "Press L3 + R3 together to acknowledge.",RGB(84,233,214));
        SelectObject(dc,smallFont);
        write(setupX+30,setupY+452,
            "This notice will only appear once.",muted);
    }else if(gameplayWelcomeView){
        SelectObject(dc,title);
        write(welcomeX+28,welcomeY+19,"RIPSHIFT - READY TO PLAY",white);
        SelectObject(dc,regular);
        write(welcomeX+30,welcomeY+108,
            "Your VR experience is already configured.",white);
        write(welcomeX+30,welcomeY+150,
            "Enjoy the default gameplay first, then",white);
        write(welcomeX+30,welcomeY+183,
            "fine-tune it to your liking if needed.",white);
        SelectObject(dc,smallFont);
        write(welcomeX+30,welcomeY+245,
            "Hold R3 during gameplay to adjust settings.",RGB(84,233,214));
        fill(welcomeX+25,welcomeY+308,welcomeW-50,55,RGB(28,57,66));
        SelectObject(dc,regular);
        write(welcomeX+43,welcomeY+323,
            "Press L3 + R3 together to acknowledge.",RGB(84,233,214));
        SelectObject(dc,smallFont);
        write(welcomeX+30,welcomeY+386,
            "This notice will only appear once.",muted);
    }else if(resultView){
        const bool newBest=s.levelProgress.bestUpdated;
        const auto accent=newBest?RGB(255,207,66):RGB(84,233,214);
        SelectObject(dc,regular);
        write(resultX+27,resultY+16,newBest?"NEW PERSONAL BEST!"
            :"LEVEL TIME",accent);
        SelectObject(dc,smallFont);
        write(resultX+27,resultY+58,
            kharvox::campaignRunTimeText(s.levelProgress.completedTimeMs),white);
        if(newBest){
            write(resultX+252,resultY+58,s.levelProgress.previousBestMs
                ?std::string("BEAT PREVIOUS BEST BY ")
                    +kharvox::campaignRunTimeText(s.levelProgress.bestImprovementMs)
                :std::string("FIRST RECORDED COMPLETION!"),accent);
        }else{
            write(resultX+252,resultY+58,"Personal best unchanged.",muted);
        }
        write(resultX+27,resultY+92,
            "Completed mission: "+s.levelProgress.verifiedMapKey,muted);
    }else if(objectCalibrationView){
        SelectObject(dc,regular);
        write(56,165,"OBJECT GRIP CALIBRATION",RGB(84,233,214));
        write(57,227,s.objectGripName.empty()
            ?"Movable Object":s.objectGripName,white);
        SelectObject(dc,smallFont);
        write(57,268,s.objectGripPhysicalHand==0
            ?"LEFT HAND PROFILE":"RIGHT HAND PROFILE",RGB(84,233,214));
        const bool rotating=s.supportGripPressed;
        fill(54,308,620,106,rotating?RGB(22,62,61):RGB(31,43,59));
        write(68,321,rotating?"WORLD FLOAT  -  GRIP HELD"
            :"FINE-TUNE MODE  -  GRIP RELEASED",RGB(84,233,214));
        write(68,357,rotating?"OBJECT FROZEN: WALK + TURN FREELY"
            :"LEFT STICK: horizontal / forward / back",white);
        write(68,384,rotating?"Move hand to desired position + angle."
            :"RIGHT STICK Y: vertical (1cm / 5cm quick)",muted);
        write(57,443,rotating
            ?"RELEASE GRIP: LOCK TO HAND POSITION + ANGLE"
            :"Hold OFFHAND TRIGGER: 5 cm quick step (default 1 cm)",muted);
        write(57,473,"RIGHT TRIGGER: SAVE PROFILE",RGB(108,235,161));
        write(57,509,"B: CANCEL / DROP OBJECT",white);
        write(57,541,"GRIP + OFFHAND TRIGGER: RESET UPRIGHT",RGB(84,233,214));
        write(57,585,s.objectGripKey.empty()
            ?"Model name unverified: item profile is session-only."
            :"Profile auto-named from verified model resource.",
            s.objectGripKey.empty()?RGB(255,196,112):muted);
    }else if(statsLiveView){
        if(statsLiveView)for(int i=0;i<kharvox::kVrStatsItemCount;++i)if(statsVisible[i]){
            const int width=kharvox::campaignStatsPanelWidth(i);
            const int x=kharvox::statsPanelX(statsCorners[i],vrMenuW,width);
            const int y=kharvox::statsPanelY(statsCorners[i],vrMenuH,counterH);
            SelectObject(dc,smallFont);
            if(i==0||i==1){
                write(x+16,y+5,i==0?"CAMPAIGN DEATHS":"SESSION DEATHS",muted);
                SelectObject(dc,regular);
                write(x+16,y+28,std::to_string(i==0
                    ?KharvoxHudCampaignDeathCount()
                    :KharvoxHudCampaignSessionDeaths()),RGB(235,92,99));
            }else if(i==6){
                write(x+16,y+5,"LEVEL DEATHS",muted);
                SelectObject(dc,regular);
                write(x+16,y+28,std::to_string(s.levelProgress.armed
                    ?s.levelProgress.deaths:s.levelLiveStats.deaths),RGB(235,92,99));
            }else{
                const bool chosenLevel=i==5&&!s.levelProgress.armed
                    &&s.vrHandMenu.hudRecordSelected>=0
                    &&s.vrHandMenu.hudRecordSelected<static_cast<int>(
                        kharvox::kCampaignMissions.size());
                const bool lastLevel=i==5&&!s.levelProgress.armed
                    &&!chosenLevel&&!s.lastCompletedMissionKey.empty();
                const std::string key=s.levelProgress.armed
                    ?s.levelProgress.verifiedMapKey
                    :chosenLevel?kharvox::kCampaignMissions[
                        s.vrHandMenu.hudRecordSelected].key
                    :lastLevel?s.lastCompletedMissionKey
                    :s.levelProgress.verifiedMapKey;
                const auto bestRecord=i==5
                    ?kharvox::levelFastestRunFor(s.levelRecords,key):nullptr;
                const auto best=bestRecord?bestRecord->timeMs:0;
                write(x+16,y+5,i==2?"CAMPAIGN TIME"
                    :i==3?"SESSION TIME":i==4?"LEVEL TIME"
                    :lastLevel?"LAST LEVEL BEST":"LEVEL BEST",muted);
                SelectObject(dc,regular);
                const auto displayedTime=i==2?s.campaignRunTime.elapsedMs:
                    i==3?s.sessionRunTime.elapsedMs:
                    i==4?(s.levelProgress.armed
                         ?s.levelProgress.time.elapsedMs:s.levelLiveStats.time.elapsedMs):best;
                const auto timeText=i==5&&!best?std::string("--:--:--"):
                    kharvox::campaignRunTimeText(displayedTime);
                write(x+16,y+28,timeText,RGB(84,233,214));
                // One red number with no label, immediately beside the PB
                // time. Never substitute the separate fewest-deaths run.
                if(i==5&&bestRecord
                    &&bestRecord->deaths!=kharvox::kLevelDeathsUnknown){
                    SIZE timeSize{},deathSize{};
                    const auto deathsText=std::to_string(bestRecord->deaths);
                    GetTextExtentPoint32A(dc,timeText.c_str(),
                        static_cast<int>(timeText.size()),&timeSize);
                    GetTextExtentPoint32A(dc,deathsText.c_str(),
                        static_cast<int>(deathsText.size()),&deathSize);
                    const int deathsX=x+16+timeSize.cx+10;
                    if(deathsX+deathSize.cx<=x+width-10)
                        write(deathsX,y+28,deathsText,RGB(235,92,99));
                }
            }
        }
    }else if(calibrationView){
        // Status is a SECOND ROW beneath the title. No badge/text may
        // overlap or truncate LIVE CALIBRATION on either controller mode.
        const bool weaponCapture=m.liveCapture==kharvox::VrLiveCapture::None;
        const bool primary=weaponCapture&&(m.rightHand!=s.weaponHandLeft);
        write(39,149,"LIVE CALIBRATION",white);
        SelectObject(dc,smallFont);
        write(39,187,!weaponCapture?"ZONE PLACEMENT"
            :!m.previewValid?"WAITING FOR TRACKING"
            :primary&&m.calibrationGripHeld?"GLOVE ROTATION ACTIVE"
            :primary?"GUN FINE-TUNE ACTIVE":"SUPPORT POSITION READY",
            m.previewValid?RGB(108,235,161):RGB(239,197,116));
        SelectObject(dc,regular);
        const auto kind=KharvoxWeaponKindFromKey(m.weapon());
        const bool heavy=kind==KharvoxWeaponKind::GaussCannon
            ||kind==KharvoxWeaponKind::Chaingun;
        if(weaponCapture){
            const auto name=KharvoxWeaponKindDisplayName(kind);
            const std::string target=m.rightHand?"RIGHT":"LEFT";
            const std::string other=m.rightHand?"LEFT":"RIGHT";
            write(39,219,std::string(name&&*name?name:m.weapon()),white);
            SelectObject(dc,smallFont);
            write(39,253,target+(primary?" PRIMARY HAND":" SUPPORT HAND"),
                RGB(84,233,214));
            if(primary){
                // Default = 1cm gun fine-tune. Grip temporarily freezes
                // the gun; the WEAPON hand rotates the visible glove.
                const bool rotating=m.calibrationGripHeld;
                fill(35,289,564,151,rotating?RGB(24,66,65):RGB(31,43,59));
                fill(35,289,4,151,rotating
                    ?RGB(108,235,161):RGB(84,233,214));
                write(49,298,rotating?"GLOVE ROTATION (HOLD GRIP)"
                    :"GUN FINE-TUNE (NO GRIP)",RGB(84,233,214));
                if(rotating){
                    write(49,337,"GUN FROZEN: rotate "+target+" controller.",white);
                    write(49,373,"Your weapon hand adjusts glove angle.",white);
                    write(49,411,"Release Grip to lock glove rotation.",muted);
                }else{
                    write(49,337,"LEFT STICK: gun left/right/forward/back",white);
                    write(49,373,"RIGHT STICK: gun up/down (1 cm)",white);
                    write(49,411,"Hold "+target+" Grip to rotate glove.",muted);
                }
                if(heavy)
                    write(39,450,m.alternateProfile
                        ?"ALT FIRE PROFILE":"NORMAL PROFILE",RGB(84,233,214));
            }else{
                fill(35,289,564,151,RGB(31,43,59));
                fill(35,289,4,151,RGB(84,233,214));
                write(49,302,"SUPPORT HAND POSITION",RGB(84,233,214));
                write(49,344,"Move "+target+" controller to place glove.",white);
                write(49,381,"Align it with the weapon's support grip.",white);
                write(49,415,"No primary-hand fine-tune controls.",muted);
            }
            // Active mode is displayed below LIVE CALIBRATION in header.
        }else{
            const bool crouch=m.liveCapture==kharvox::VrLiveCapture::CrouchStanding
                ||m.liveCapture==kharvox::VrLiveCapture::CrouchLowered;
            write(39,235,crouch?"PHYSICAL CROUCH":"GUNSLINGER ZONE",white);
            SelectObject(dc,smallFont);
            write(39,295,crouch?"Move HEAD to the target height."
                :"Hold matching GRIP to move the zone.",white);
            write(39,340,crouch?"Press RIGHT TRIGGER to capture."
                :m.gunslingerZoneMoving?"DRAGGING: release GRIP to freeze."
                :"RELEASED: RIGHT TRIGGER saves.",RGB(84,233,214));
        }
        SelectObject(dc,smallFont);
        write(39,523,"RIGHT TRIGGER: SAVE",RGB(108,235,161));
        write(39,551,"B: CANCEL",muted);
        if(devPanel){
            // The original diagnostics were only a few words on the
            // instruction strip. A transparent-alpha mask clipped them,
            // and a head-zero condition silently suppressed zone status.
            // Draw an actual independent diagnostic panel on the right.
            constexpr int dx=632,dy=144,dw=306,dh=432;
            SelectObject(dc,regular);
            write(dx+18,dy+14,"DEV DIAGNOSTICS",white);
            SelectObject(dc,smallFont);
            if(m.liveCapture==kharvox::VrLiveCapture::None){
                write(dx+18,dy+69,"WEAPON / HAND ANCHOR",RGB(84,233,214));
                const auto& hand=s.weaponHandLeft
                    ?s.leftGripController:s.rightGripController;
                const bool tracked=hand.valid&&hand.positionTracked
                    &&s.lastWeaponRenderedValid;
                write(dx+18,dy+101,tracked
                    ?"TRACKING: LIVE":"TRACKING: WAITING",tracked
                        ?RGB(108,235,161):RGB(255,204,128));
                if(tracked){
                    // Both coordinates are in OpenXR tracking space.
                    // The unshifted gun anchor and controller grip remain
                    // separate even when a 1cm gun fine-tune is applied.
                    const auto diff=subtractVector(hand.position,
                        s.lastWeaponRenderedTrackingAnchor);
                    auto format=[&](const char* label,float value){
                        std::ostringstream out;
                        out<<label<<std::fixed<<std::setprecision(3)<<value;
                        return out.str();
                    };
                    write(dx+18,dy+136,format("HAND -> GUN X: ",diff.x),white);
                    write(dx+18,dy+169,format("HAND -> GUN Y: ",diff.y),white);
                    write(dx+18,dy+202,format("HAND -> GUN Z: ",diff.z),white);
                    write(dx+18,dy+251,"TOP VIEW / X-Z",muted);
                    fill(dx+17,dy+294,270,1,RGB(90,111,130));
                    fill(dx+152,dy+264,1,108,RGB(90,111,130));
                    const int hx=dx+152+static_cast<int>(std::clamp(
                        diff.x*380.f,-119.f,119.f));
                    const int hz=dy+294+static_cast<int>(std::clamp(
                        diff.z*280.f,-43.f,43.f));
                    fill(dx+146,dy+288,13,13,RGB(55,209,214));
                    fill(hx-5,hz-5,11,11,RGB(255,204,128));
                    write(dx+18,dy+389,"CYAN gun  /  GOLD controller",muted);
                }else{
                    write(dx+18,dy+182,
                        "Equip the selected weapon and",white);
                    write(dx+18,dy+213,"keep its hand tracked.",white);
                }
            }else{
                if(m.devControllerInputs){
                    using C=kharvox::VrLiveCapture;
                    const bool hip=m.liveCapture==C::HipRight
                        ||m.liveCapture==C::HipLeft;
                    const bool right=m.liveCapture==C::HipRight
                        ||m.liveCapture==C::WheelRight;
                    write(dx+18,dy+64,hip?"HIP ZONE":"HEAD / WHEEL ZONE",
                        RGB(84,233,214));
                    write(dx+18,dy+94,right?"RIGHT CONTROLLER"
                        :"LEFT CONTROLLER",muted);
                    const auto& sample=m.devInput;
                    auto on=[](bool value){return value?"ON":"off";};
                    const int top=dy+155;
                    write(dx+18,top,"R TRIGGER: "+std::string(on(sample.rightTrigger))
                        +"   L: "+on(sample.leftTrigger),RGB(84,233,214));
                    write(dx+18,top+27,"GRIP: "+std::string(on(sample.weaponGrip))
                        +"   L3: "+on(sample.leftClick)
                        +"   R3: "+on(sample.rightClick),white);
                    write(dx+18,top+57,"A: "+std::string(on(sample.select))
                        +"   B: "+on(sample.back),white);
                    std::ostringstream sticks;
                    sticks<<std::fixed<<std::setprecision(2)
                        <<"LEFT STICK "<<sample.leftStickX<<", "
                        <<sample.leftStickY;
                    write(dx+18,top+90,sticks.str(),muted);
                    write(dx+18,top+138,"Input is sampled live.",muted);
                }
            }
        }
    }else if(m.page==kharvox::VrMenuPage::Records
        ||m.page==kharvox::VrMenuPage::DevRecords){
        const bool dev=m.page==kharvox::VrMenuPage::DevRecords;
        SelectObject(dc,title);
        write(38,24,dev?"RIPSHIFT VR / DEV / LEVEL RECORDS"
            :"RIPSHIFT VR / LEVEL RECORDS",white);
        SelectObject(dc,smallFont);
        write(40,100,dev?"SELECT MISSION RECORDS TO RESET"
            :"MISSION RESULTS / TOP FIVE",muted);
        const int count=static_cast<int>(kharvox::kCampaignMissions.size())
            +(dev?1:0);
        const int chosen=std::clamp(m.recordSelected,0,std::max(0,count-1));
        for(int shown=0;shown<10;++shown){
            const int i=shown+m.recordsScroll;
            if(i>=count)break;
            const int y=136+shown*45;
            const bool active=i==chosen;
            fill(38,y,455,40,active?RGB(32,89,104):
                m.hovered==1000+i?RGB(44,54,71):RGB(27,32,43));
            if(active)fill(38,y,5,40,RGB(55,209,214));
            SelectObject(dc,smallFont);
            if(dev&&i==static_cast<int>(kharvox::kCampaignMissions.size())){
                write(55,y+10,"ALL LEVEL RECORDS",RGB(255,175,175));
            }else{
                const auto& mission=kharvox::kCampaignMissions[i];
                const bool current=s.levelProgress.armed
                    &&s.levelProgress.verifiedMapKey==mission.key;
                write(55,y+10,mission.title,current?RGB(84,233,214):white);
                if(current)write(439,y+10,"*",RGB(255,207,66));
            }
        }
        fill(797,16,129,57,m.hovered==95
            ?RGB(47,95,110):RGB(39,49,63));
        write(815,30,"BACK",white);
        SelectObject(dc,regular);
        write(538,115,dev?"RECORD RESET":"MISSION RECORDS",white);
        SelectObject(dc,smallFont);
        if(dev){
            const bool all=chosen==static_cast<int>(
                kharvox::kCampaignMissions.size());
            const bool confirming=m.pendingStatsReset==100+chosen
                &&m.confirmDeathResetUntilMs
                &&GetTickCount64()<=m.confirmDeathResetUntilMs;
            write(538,145,all?"ALL MISSIONS"
                :kharvox::kCampaignMissions[chosen].title,white);
            fill(531,186,384,58,m.hovered==130
                ||(m.navigation.settingsColumn&&m.navigation.actionIndex==0)
                ?RGB(83,52,61):RGB(60,38,48));
            write(548,202,confirming?"CONFIRM DELETE (SECOND PRESS)"
                :all?"DELETE ALL LEVEL RECORDS":"DELETE SELECTED MISSION",
                confirming?RGB(255,175,175):white);
            write(538,294,all?"Deletes ALL saved mission results."
                :"Only the selected mission is affected.",muted);
            write(538,339,"Removes both time and deaths rankings.",muted);
            write(538,383,"Live stats & HUD settings stay unchanged.",muted);
            write(538,437,"Press twice within 6 seconds.",RGB(84,233,214));
        }else{
            const auto& mission=kharvox::kCampaignMissions[chosen];
            const bool recording=s.levelProgress.armed&&!s.levelProgress.completed;
            const bool current=recording
                &&s.levelProgress.verifiedMapKey==mission.key;
            const bool confirming=recording
                ?kharvox::vrLevelStopPending(m,GetTickCount64())
                :kharvox::vrLevelStartPending(m,chosen,GetTickCount64());
            const auto ranked=kharvox::levelRankedRuns(
                s.levelRecords,mission.key,m.recordSortDeaths);
            write(538,152,mission.title,white);
            fill(531,190,384,47,m.hovered==131
                ||(m.navigation.settingsColumn&&m.navigation.actionIndex==0)
                ?RGB(44,76,88):RGB(32,53,65));
            write(549,203,m.recordSortDeaths?"SORT: FEWEST DEATHS  < >"
                :"SORT: FASTEST TIME  < >",RGB(84,233,214));
            write(542,270,"#   TIME",muted);
            write(701,270,"DEATHS",muted);
            write(777,270,"DATE",muted);
            if(ranked.empty())
                write(540,339,"No completed runs recorded.",muted);
            for(std::size_t i=0;i<ranked.size();++i){
                const int y=303+int(i)*48;
                const auto& run=ranked[i];
                fill(532,y-5,383,39,i==0?RGB(48,45,33):RGB(25,33,43));
                write(541,y,std::to_string(i+1),i==0?RGB(255,207,66):muted);
                write(570,y,kharvox::campaignRunTimeText(run.timeMs),white);
                write(714,y,run.deaths==kharvox::kLevelDeathsUnknown
                    ?std::string("--"):std::to_string(run.deaths),white);
                write(790,y,kharvox::levelRecordDateText(run.dateYmd),muted);
            }
            fill(531,565,384,53,m.hovered==130
                ||(m.navigation.settingsColumn&&m.navigation.actionIndex==1)
                ?RGB(52,83,99):RGB(39,49,63));
            write(550,579,recording
                ?(confirming?"CONFIRM STOP (SECOND PRESS)":"STOP RECORD")
                :(confirming?"CONFIRM START (SECOND PRESS)":"TRACK THIS LEVEL"),
                confirming?RGB(255,207,66):white);
            write(538,632,recording
                ?(confirming?"Discard attempt; prior records stay."
                    :"Double-confirm to abandon active record.")
                :(confirming?"Menu closes; timer and deaths reset."
                    :"Double-confirm to start a timed attempt."),
                confirming?RGB(255,207,66):muted);
        }
        write(40,646,"LEFT STICK: browse   A: select   B: back",muted);
        write(40,676,m.message.substr(0,105),RGB(84,233,214));
    }else if(m.page==kharvox::VrMenuPage::Stats
        ||m.page==kharvox::VrMenuPage::DevStats){
        const bool dev=m.page==kharvox::VrMenuPage::DevStats;
        SelectObject(dc,title);
        write(38,24,dev?"RIPSHIFT VR / DEV / STATS RESET":"RIPSHIFT VR / STATS",white);
        SelectObject(dc,smallFont);
        write(40,100,dev?"SELECT A STAT TO RESET":"DISPLAY OPTIONS",muted);
        const int chosen=std::clamp(m.statsSelected,0,
            dev?3:kharvox::kVrStatsItemCount);
        // Identical ten-row, 45px list pattern to the main Mod Menu.
        // Extra Stats appear by scrolling rather than shrinking buttons.
        const int statCount=dev?4:kharvox::kVrStatsItemCount+1;
        for(int shown=0;shown<10;++shown){
            const int i=shown+m.statsScroll;
            if(i>=statCount)break;
            const int y=136+shown*45;
            const bool active=i==chosen;
            const bool hovered=m.hovered==1000+i;
            fill(38,y,455,40,active?RGB(32,89,104):
                hovered?RGB(44,54,71):RGB(27,32,43));
            if(active)fill(38,y,5,40,RGB(55,209,214));
            SelectObject(dc,smallFont);
            write(55,y+10,dev?(i==0?"Campaign Deaths":
                i==1?"Session Deaths":i==2?"Campaign Run Timer":"Session Timer")
                :i==kharvox::kVrStatsItemCount?"LEVEL RECORDS  >"
                    :kharvox::vrStatsLabel(i),white);
            if(!dev&&i<kharvox::kVrStatsItemCount)
                write(428,y+10,kharvox::vrStatsEnabled(s.vrLiveSettings,i)
                    ?"ON":"OFF",kharvox::vrStatsEnabled(s.vrLiveSettings,i)
                        ?RGB(108,235,161):muted);
        }
        if(statCount>10)
            write(48,602,"LEFT STICK: scroll ("+
                std::to_string(chosen+1)+"/"+std::to_string(statCount)+")",muted);
        fill(797,16,129,57,m.hovered==95?RGB(47,95,110):RGB(39,49,63));
        SelectObject(dc,smallFont);
        write(815,30,"BACK",white);
        SelectObject(dc,regular);
        write(538,115,dev?"RESET OPTIONS"
            :chosen==kharvox::kVrStatsItemCount?"LEVEL RECORDS"
                :kharvox::vrStatsLabel(chosen),white);
        if(dev){
            const bool confirming=m.pendingStatsReset==chosen
                &&m.confirmDeathResetUntilMs
                &&GetTickCount64()<=m.confirmDeathResetUntilMs;
            fill(531,186,384,58,m.hovered==130
                ||(m.navigation.settingsColumn&&m.navigation.usingStick
                    &&m.navigation.actionIndex==0)
                    ?RGB(66,83,99):RGB(39,49,63));
            SelectObject(dc,smallFont);
            write(549,202,confirming?"CONFIRM RESET (SECOND PRESS)"
                :chosen==0?"RESET CAMPAIGN DEATHS"
                :chosen==1?"RESET SESSION DEATHS"
                 :chosen==2?"RESET CAMPAIGN TIMER":"RESET SESSION TIMER",
                confirming?RGB(235,92,99):white);
            const std::string value=chosen==0
                ?std::to_string(KharvoxHudCampaignDeathCount())+" deaths"
                :chosen==1?std::to_string(KharvoxHudCampaignSessionDeaths())+" deaths"
                :kharvox::campaignRunTimeText(chosen==2
                    ?s.campaignRunTime.elapsedMs:s.sessionRunTime.elapsedMs);
            write(538,292,"CURRENT: "+value,RGB(84,233,214));
            write(538,342,"Each reset needs two presses.",muted);
            const bool confirmAll=m.pendingStatsReset==4
                &&m.confirmDeathResetUntilMs
                &&GetTickCount64()<=m.confirmDeathResetUntilMs;
            fill(531,397,384,58,m.hovered==131
                ||(m.navigation.settingsColumn&&m.navigation.usingStick
                    &&m.navigation.actionIndex==1)
                    ?RGB(83,52,61):RGB(60,38,48));
            write(549,412,confirmAll?"CONFIRM RESET ALL (SECOND PRESS)"
                :"RESET ALL STATS",confirmAll?RGB(255,175,175):white);
            write(538,475,"Resets deaths, session & level timers.",muted);
            write(538,510,"Keeps personal bests and HUD settings.",RGB(84,233,214));
            write(538,552,"Closes the Mod Menu after reset.",muted);
        }else if(chosen==kharvox::kVrStatsItemCount){
            SelectObject(dc,smallFont);
            write(538,208,"VIEW SAVED LEVEL PERSONAL BESTS",white);
            write(538,267,"Press A to open the Records page.",RGB(84,233,214));
            write(538,325,"Records survive game restarts.",muted);
            write(538,380,"DEV has separate record resets.",muted);
        }else{
            const bool enabled=kharvox::vrStatsEnabled(s.vrLiveSettings,chosen);
            fill(531,159,384,60,m.hovered==110
                ||(m.navigation.settingsColumn&&m.navigation.actionIndex==0)
                    ?RGB(52,83,99):RGB(39,49,63));
            fill(553,172,36,36,RGB(54,79,91));
            if(enabled)fill(562,181,18,18,RGB(76,231,167));
            SelectObject(dc,regular);
            write(603,169,enabled?"DISPLAY ENABLED":"DISPLAY DISABLED",white);
            SelectObject(dc,smallFont);
            write(538,226,"POSITION ON SCREEN  /  12 x 10",muted);
            const int currentCell=kharvox::statsGridCellFromPosition(
                kharvox::vrStatsCorner(s.vrLiveSettings,chosen));
            // One miniature screen replaces 120 word-labelled buttons.
            // An occupied cell is dark but still selectable: it evicts the
            // other display only when the new position is confirmed.
            fill(kharvox::kStatsMenuGridX-2,kharvox::kStatsMenuGridY-2,
                kharvox::kStatsGridColumns*kharvox::kStatsMenuGridCellWidth+4,
                kharvox::kStatsGridRows*kharvox::kStatsMenuGridCellHeight+4,
                RGB(71,96,108));
            for(int cell=0;cell<kharvox::kStatsGridCells;++cell){
                const int col=cell%kharvox::kStatsGridColumns;
                const int row=cell/kharvox::kStatsGridColumns;
                const int x=kharvox::kStatsMenuGridX
                    +col*kharvox::kStatsMenuGridCellWidth;
                const int y=kharvox::kStatsMenuGridY
                    +row*kharvox::kStatsMenuGridCellHeight;
                const bool available=kharvox::statsGridCellSelectable(cell);
                const bool active=cell==currentCell;
                const bool focus=m.hovered==130+cell
                    ||(m.navigation.settingsColumn
                        &&m.navigation.usingStick
                        &&m.navigation.actionIndex==cell+1);
                bool occupied=false;
                if(available)for(int other=0;other<kharvox::kVrStatsItemCount;++other){
                    if(other==chosen||!kharvox::vrStatsEnabled(
                        s.vrLiveSettings,other))continue;
                    if(kharvox::vrStatsPositionsOverlap(chosen,
                        kharvox::statsPositionFromGridCell(cell),other,
                        kharvox::vrStatsCorner(s.vrLiveSettings,other))){
                        occupied=true;break;
                    }
                }
                fill(x,y,kharvox::kStatsMenuGridCellWidth-1,
                    kharvox::kStatsMenuGridCellHeight-1,
                    !available?RGB(3,8,12)
                    :focus?RGB(42,116,132)
                    :active?RGB(30,99,103)
                    :occupied?RGB(20,24,33):RGB(37,53,67));
                if(active&&available)
                    fill(x+9,y+8,kharvox::kStatsMenuGridCellWidth-19,
                        kharvox::kStatsMenuGridCellHeight-17,
                        RGB(76,231,167));
            }
            write(538,572,"Dark = occupied (select to replace).",muted);
            write(538,607,"Centre blocked; stats track while hidden.",muted);
        }
        SelectObject(dc,smallFont);
        write(40,645,"LEFT STICK: select / options   A: confirm   B: back",muted);
        write(40,676,m.message.substr(0,105),RGB(84,233,214));
    }else if(m.page==kharvox::VrMenuPage::Mods
        ||m.page==kharvox::VrMenuPage::Dev){
        const bool devPage=m.page==kharvox::VrMenuPage::Dev;
        write(38,24,devPage?"RIPSHIFT VR  /  DEV":"RIPSHIFT VR  /  MOD SETTINGS",white);
        SelectObject(dc,smallFont);
        write(40,102,devPage?"DEVELOPER TOOLS":"CONFIGURABLE MODS",muted);
        write(535,102,m.navigation.settingsColumn?"CONFIG OPTIONS":"MOD OVERVIEW",muted);
        const auto entries=activeVrMenuEntries();
        const int count=static_cast<int>(entries.size());
        const int selected=std::clamp(m.selectedRow,0,std::max(0,count-1));
        // Pointer hover previews a mod's details without entering or
        // selecting its settings; A still applies to the selected row.
        const int overviewSelected=!m.navigation.settingsColumn
            &&m.hovered>=1000&&m.hovered<1000+count
                ?m.hovered-1000:selected;
        for(int shown=0;shown<10;++shown){
            const int index=shown+m.modScroll;
            if(index>=count)break;
            const int y=136+shown*45;
            const bool active=index==selected;
            const bool hovered=m.hovered==1000+index
                ||(m.navigation.usingStick&&!m.navigation.settingsColumn
                    &&m.selectedRow==index);
            fill(38,y,455,40,active?RGB(32,89,104):
                hovered?RGB(44,54,71):RGB(27,32,43));
            if(active)fill(38,y,5,40,RGB(55,209,214));
            SelectObject(dc,smallFont);
            std::string title=entries[index].title;
            // Core mods stay fixed; Crouch, PAINSAW and the optional
            // Campaign Deaths has a legacy live ON/OFF control.
            if(title.size()>29)title.resize(29);
            write(55,y+10,title,entries[index].enabled?white:muted);
            const bool switchable=!devPage
                &&kharvox::vrLiveModToggleable(entries[index].mod);
            // The four fixed-on gameplay mods have no "SET" labels.
            // Use a simple disclosure arrow; Crouch/PAINSAW show status.
            write(switchable?436:458,y+10,
                switchable?(entries[index].enabled?"ON":"OFF"):">",
                switchable?(entries[index].enabled?RGB(108,235,161)
                    :RGB(151,158,171)):RGB(84,233,214));
        }
        if(count>10){
            SelectObject(dc,smallFont);
            write(48,602,"LEFT STICK: scroll  ("+
                std::to_string(selected+1)+"/"+std::to_string(count)+")",muted);
        }
        // Experimental is the one DEV row whose right-side switches are
        // visible and pointer-clickable as soon as the row is highlighted.
        const bool expInline=devPage&&count>0
            &&(entries[selected].developer==kharvox::VrMenuDevOption::Experimental
                ||entries[selected].developer==kharvox::VrMenuDevOption::GrappleAnchor);
        if(!m.navigation.settingsColumn&&!expInline){
            const auto mod=count>0?entries[overviewSelected].mod:kharvox::VrLiveMod::Other;
            const auto overview=entries[overviewSelected].developer
                ==kharvox::VrMenuDevOption::None
                    ?vrLiveModOverview(mod)
                    :vrDevOptionOverview(entries[overviewSelected].developer);
            SelectObject(dc,regular);
            write(536,127,count>0?entries[overviewSelected].title:"No available mods",white);
            SelectObject(dc,smallFont);
            write(538,170,"WHAT IT DOES / HOW TO USE IT",RGB(84,233,214));
            for(int line=0;line<6;++line){
                const char* text=overview.lines[line];
                if(text&&*text)
                    write(540,224+line*37,text,line==5?RGB(84,233,214):white);
            }
            const bool devFolder=!devPage&&count>0
                &&entries[overviewSelected].developer==kharvox::VrMenuDevOption::Folder;
            if(!devFolder){
                fill(531,550,384,59,m.hovered==115
                    ?RGB(53,95,108):RGB(36,78,92));
                SelectObject(dc,regular);
                write(624,563,devPage?"OPEN / TOGGLE >":"CONFIGURE  >",white);
            }else{
                SelectObject(dc,smallFont);
                write(538,563,"Select DEV to enter tools directly.",RGB(84,233,214));
            }
            SelectObject(dc,smallFont);
            write(538,618,devFolder?"A / POINTER: OPEN DEV    B: CLOSE"
                :"A: OPEN OPTIONS    B: CLOSE",muted);
        }else{
        const auto mod=count>0?entries[selected].mod:kharvox::VrLiveMod::Other;
        const bool editable=kharvox::vrLiveModEditable(mod);
        const bool toggleable=!devPage&&kharvox::vrLiveModToggleable(mod);
        const bool enabled=count>0&&entries[selected].enabled;
        SelectObject(dc,regular);
        write(536,126,count>0?entries[selected].title:"No active mods",white);
        if(devPage){
            using D=kharvox::VrMenuDevOption;
            const auto opt=count>0?entries[selected].developer:D::Experimental;
            SelectObject(dc,smallFont);
            write(538,163,opt==D::Experimental
                ?"OBJECT DISCOVERY MODE"
                :opt==D::GrappleAnchor?"GRAPPLE GRAB - EXPERIMENTAL"
                :"SESSION-ONLY DEVELOPER TOOL",muted);
            if(opt==D::GrappleAnchor){
                const bool on=m.grappleAnchor.enabled;
                fill(531,186,384,77,m.hovered==130
                    ?RGB(52,83,99):RGB(39,49,63));
                write(548,204,"GRAPPLE GRAB",white);
                write(843,204,on?"ON":"OFF",
                    on?RGB(128,255,177):muted);
                write(548,235,"A / TRIGGER: TOGGLE",muted);
                write(538,309,"Aim laser at a movable prop, Grip.",white);
                write(538,358,"Keep tilt; follow your wrist in 6DOF.",white);
                write(538,442,"CURRENT LIMITATION",RGB(255,196,112));
                write(538,478,"Native render-mesh hit not verified.",muted);
                write(538,508,"Existing ray point used until available.",muted);
                write(538,563,"No barrel-specific shape presets.",muted);
                write(538,612,m.objectDiscovery.enabled
                    ?"Scanner active. Moving props only."
                    :"ENABLE OBJECT DISCOVERY FIRST",m.objectDiscovery.enabled
                        ?RGB(128,255,177):RGB(255,196,112));
            }else if(opt!=D::Experimental&&opt!=D::GrappleAnchor){
                fill(531,186,384,58,m.hovered==130
                    ?RGB(52,83,99):RGB(39,49,63));
                write(548,204,opt==D::WeaponStability
                    ?"OPEN WEAPON RECOIL SETTINGS"
                     :opt==D::StatsReset
                        ?"OPEN STATS RESET SETTINGS"
                    :opt==D::HandFocus
                        ?(s.customMods.handFocusedRs?"SWITCH TO HEAD FOCUS"
                            :"SWITCH TO HAND FOCUS")
                    :(entries[selected].enabled?"TURN VISUALISER OFF"
                        :"TURN VISUALISER ON"),white);
            }else{
                const auto& cfg=m.objectDiscovery;
                const bool hoverToggle=m.hovered==130
                    ||(m.navigation.usingStick&&m.navigation.actionIndex==0);
                fill(531,186,384,77,hoverToggle
                    ?RGB(52,83,99):RGB(39,49,63));
                write(548,204,std::string("OBJECT DISCOVERY: ")
                    +(cfg.enabled?"ON":"OFF"),
                    cfg.enabled?RGB(128,255,177):white);
                write(548,235,"A / TRIGGER: TOGGLE",muted);
                const auto slider=[&](int index,int top,
                    const char* label,float value,float minimum,float maximum){
                    const bool selected=m.hovered==130+index
                        ||(m.navigation.usingStick&&m.navigation.actionIndex==index);
                    fill(531,top,384,77,selected
                        ?RGB(52,83,99):RGB(39,49,63));
                    std::ostringstream line;
                    line<<label<<"  "<<std::fixed<<std::setprecision(
                        index==1?0:1)<<value<<" m";
                    write(548,top+7,line.str(),white);
                    fill(556,top+40,338,4,RGB(85,104,121));
                    const int markerX=556+static_cast<int>(std::lround(
                        (value-minimum)/(maximum-minimum)*338.f));
                    fill(markerX-5,top+31,10,22,RGB(55,209,214));
                    write(548,top+59,"A: NEXT STEP  POINTER: SELECT",muted);
                };
                slider(1,277,"SCAN RADIUS",cfg.scanRadiusMetres,1.f,50.f);
                slider(2,368,"GRAB REACH",cfg.grabReachMetres,1.f,5.f);
                const bool laserSelected=m.hovered==133
                    ||(m.navigation.usingStick&&m.navigation.actionIndex==3);
                fill(531,459,384,77,laserSelected
                    ?RGB(52,83,99):RGB(39,49,63));
                write(548,476,std::string("GRAB REACH LASER: ")
                    +(cfg.grabReachLaserEnabled?"ON":"OFF"),
                    cfg.grabReachLaserEnabled?RGB(128,255,177):white);
                write(548,507,"A / TRIGGER: TOGGLE",muted);
                const bool meleeSelected=m.hovered==134
                    ||(m.navigation.usingStick&&m.navigation.actionIndex==4);
                fill(531,550,384,77,meleeSelected
                    ?RGB(52,83,99):RGB(39,49,63));
                write(548,558,m.objectMeleeProbeMode==1?"MELEE A: TRACE PROBE":
                    m.objectMeleeProbeMode==2?"MELEE B: TARGET PROBE":
                    "MELEE PROBES: OFF",white);
                write(548,580,"GRIP + MANUAL MELEE; LOG ONLY",muted);
                const unsigned discoverySource=KharvoxCameraObjectDiscoverySourceMode();
                write(548,605,!cfg.enabled?"SCANNER: OFF":
                    discoverySource==2?"SCANNER: ACTIVE":"SCANNER: UNAVAILABLE",
                    discoverySource==2?RGB(128,255,177):RGB(255,196,112));
            }
            if(opt==D::StatsReset){
                write(538,305,"Separate resets for all three Stats.",muted);
                write(538,348,"Each requires a second confirmation.",muted);
            }else if(opt!=D::Experimental&&opt!=D::GrappleAnchor){
                write(538,332,"Enabled tools only show in the matching",muted);
                write(538,361,"calibration screen.",muted);
            }
        }else if(toggleable){
            const bool focusToggle=m.navigation.usingStick
                &&m.navigation.settingsColumn&&m.navigation.actionIndex==0;
            fill(531,159,384,60,focusToggle?RGB(52,83,99):RGB(39,49,63));
            fill(553,172,36,36,RGB(54,79,91));
            if(enabled)fill(562,181,18,18,RGB(76,231,167));
            SelectObject(dc,regular);
            write(603,169,enabled?"ENABLED   (TOGGLE)":"DISABLED   (TOGGLE)",white);
        }else{
            SelectObject(dc,smallFont);
            write(538,155,kharvox::vrLiveModAlwaysEnabled(mod)
                ?"CORE FEATURE - ALWAYS ACTIVE":"SETTINGS - NO LIVE ON/OFF SWITCH",muted);
        }
        // DEV settings own their right pane; do not draw the ordinary
        // mod controls over the diagnostic switches.
        if(!devPage){
        SelectObject(dc,smallFont);
        if(!editable)
            write(545,251,"Configure in desktop launcher only.",muted);
        else{
            // Menu controls are shown once in footer to avoid card overlap.
            std::array<std::string,5> options{};
            int optionCount{};
            switch(mod){
                case kharvox::VrLiveMod::WeaponGrip:
                    options[0]="OPEN WEAPON CALIBRATION";
                    optionCount=1;
                    break;
                case kharvox::VrLiveMod::WeaponRecoil:
                    options[0]="OPEN RECOIL CALIBRATION";
                    optionCount=1;
                    break;
                case kharvox::VrLiveMod::Gunslinger:
                    options={"SET RIGHT HIP","SET LEFT HIP",
                             "SET RIGHT WHEEL","SET LEFT WHEEL","RESET ZONES"};
                    optionCount=5;
                    break;
                case kharvox::VrLiveMod::PhysicalCrouch:
                    options[0]="SET STANDING HEAD HEIGHT";
                    options[1]="SET CROUCHING HEAD HEIGHT";
                    options[2]="RESET HEIGHTS";
                    optionCount=3;
                    break;
                case kharvox::VrLiveMod::Killtime:
                    options[0]="SLOWER (+10 HZ)";
                    options[1]="FASTER (-10 HZ)";
                    options[2]="RESET SPEED";
                    optionCount=3;
                    break;
                case kharvox::VrLiveMod::TurnSpeed:
                    options[0]="SLOWER (-25 DEG/S)";
                    options[1]="FASTER (+25 DEG/S)";
                    options[2]="USE LAUNCHER SPEED";
                    optionCount=3;
                    break;
                case kharvox::VrLiveMod::DeathCounter:
                    options={"TOP LEFT","TOP RIGHT","BOTTOM LEFT","BOTTOM RIGHT",""};
                    optionCount=4;
                    break;
                case kharvox::VrLiveMod::PhysicalGloryKills:
                    options[0]="LOWER PUNCH THRESHOLD";
                    options[1]="RAISE PUNCH THRESHOLD";
                    options[2]="CHANGE ACTIVE PUNCH HAND";
                    options[3]="RESET PUNCH DEFAULTS";
                    optionCount=4;
                    break;
                default:break;
            }
            for(int index=0;index<optionCount;++index){
                const int y=(toggleable?248:186)+68*index;
                const bool focused=m.hovered==130+index
                    ||(m.navigation.usingStick&&m.navigation.settingsColumn
                        &&m.navigation.actionIndex==index+(toggleable?1:0));
                fill(531,y,384,58,focused
                     ?RGB(52,83,99):RGB(39,49,63));
                SelectObject(dc,smallFont);
                write(548,y+17,options[index],white);
            }
            if(mod==kharvox::VrLiveMod::Killtime){
                write(548,543,"GLORY KILL: "+vrLiveGloryKillSpeedText(),
                    RGB(84,233,214));
            }else if(mod==kharvox::VrLiveMod::TurnSpeed){
                write(548,543,"SMOOTH TURN: "+std::to_string(static_cast<int>(
                    std::lround(s.smoothTurnDegreesPerSecond)))+" DEG/S",
                    RGB(84,233,214));
                write(548,576,"SNAP ANGLE UNCHANGED",muted);
            }else if(mod==kharvox::VrLiveMod::PhysicalGloryKills){
                std::ostringstream speed;
                speed<<std::fixed<<std::setprecision(1)
                    <<s.vrLiveSettings.punchSpeedMetersPerSec;
                write(548,543,"PUNCH: "+speed.str()+" m/s  HAND: "+
                    (s.vrLiveSettings.punchHands==0?"RIGHT"
                        :s.vrLiveSettings.punchHands==1?"LEFT":"BOTH"),
                    RGB(84,233,214));
            }else if(mod==kharvox::VrLiveMod::PhysicalCrouch){
                write(548,543,s.vrLiveSettings.crouchLoweredSaved
                    ?"CUSTOM HEIGHTS ACTIVE":"DEFAULT CROUCH HEIGHTS",
                    RGB(84,233,214));
                if(!enabled)
                    write(548,575,"RIGHT STICK DOWN: TOGGLE CROUCH",RGB(84,233,214));
            }else if(mod==kharvox::VrLiveMod::DeathCounter){
                write(548,543,"POSITION: "+std::string(kharvox::deathCounterCornerName(
                    s.vrLiveSettings.deathCounterCorner)),RGB(84,233,214));
                write(548,574,"DEATHS: "+
                    std::to_string(KharvoxHudCampaignDeathCount()),muted);
            }else if(mod==kharvox::VrLiveMod::Painsaw){
                write(548,270,enabled?"PAINSAW CUSTOM SPEED ACTIVE"
                    :"NATIVE CHAINSAW SPEED",RGB(84,233,214));
            }else if(mod==kharvox::VrLiveMod::Gunslinger){
                write(548,605,"HAND ZONES UPDATE INSTANTLY",RGB(84,233,214));
            }
        }
        } // !devPage: prevent normal mod settings drawing over DEV
        } // close the selected-mod options pane before the shared footer
        SelectObject(dc,smallFont);
        if(devPage){
            fill(797,16,129,57,m.hovered==95
                ?RGB(47,95,110):RGB(39,49,63));
            SelectObject(dc,smallFont);
            write(815,30,"BACK",white);
        }
        if(m.navigation.settingsColumn)
            write(538,638,devPage?"B: DEV LIST    A: SELECT"
                :"B: BACK TO MODS     A: SELECT",muted);
        write(40,662,m.message.empty()?"No restart required. Changes stored in UserData."
            :m.message.substr(0,105),RGB(84,233,214));
    }else if(m.page==kharvox::VrMenuPage::WeaponRecoil){
        write(38,24,"RIPSHIFT VR  /  RECOIL CALIBRATION",white);
        SelectObject(dc,smallFont);
        write(40,102,"WEAPON PROFILES",muted);
        write(536,102,"ONE-HANDED WEAPON STABILITY",muted);
        const int selected=std::clamp(m.selectedRecoilWeapon,0,
            kharvox::kVrRecoilMenuEntryCount-1);
        for(int i=0;i<static_cast<int>(kharvox::kWeaponRecoilKinds.size());++i){
            const int y=136+i*45;
            const bool active=i==selected;
            const bool hover=m.hovered==200+i
                ||(m.navigation.usingStick&&!m.navigation.settingsColumn
                    &&active);
            fill(38,y,455,40,active?RGB(32,89,104):
                hover?RGB(44,54,71):RGB(27,32,43));
            if(active)fill(38,y,5,40,RGB(55,209,214));
            SelectObject(dc,regular);
            const auto key=kharvox::kWeaponRecoilKinds[static_cast<std::size_t>(i)];
            const auto name=KharvoxWeaponKindDisplayName(KharvoxWeaponKindFromKey(key));
            const char* label=i==7?"Minigun (Normal)"
                :i==9?"Minigun (Alt Fire)":(name&&*name?name:key);
            write(56,y+6,label,white);
            SelectObject(dc,smallFont);
            const int strength=static_cast<int>(std::lround(
                s.recoilSettings.strength[static_cast<std::size_t>(i)]*100.f));
            const bool on=s.recoilSettings.enabled[static_cast<std::size_t>(i)];
            write(372,y+9,(on?"ON ":"OFF ")+std::to_string(strength)+"%",
                on?(active?RGB(115,246,178):RGB(91,185,134))
                  :(active?RGB(244,175,125):muted));
        }
        // An extra row below the ten weapon profiles configures the
        // SHARED build-up. Gauss is excluded and retains manual tuning.
        const int dynamicY=136+kharvox::kVrRecoilMenuProfileCount*45;
        const bool dynamicSelected=selected==kharvox::kVrRecoilMenuProfileCount;
        const bool dynamicHover=m.hovered==210
            ||(m.navigation.usingStick&&!m.navigation.settingsColumn
                &&dynamicSelected);
        fill(38,dynamicY,455,40,dynamicSelected?RGB(32,89,104):
            dynamicHover?RGB(44,54,71):RGB(27,32,43));
        if(dynamicSelected)fill(38,dynamicY,5,40,RGB(55,209,214));
        SelectObject(dc,regular);
        write(56,dynamicY+6,"SHARED DYNAMIC BUILD-UP",white);
        SelectObject(dc,smallFont);
        write(372,dynamicY+9,s.recoilSettings.dynamicEnabled?"ON":"OFF",
            s.recoilSettings.dynamicEnabled?RGB(115,246,178):muted);

        const std::size_t index=dynamicSelected?0u:
            static_cast<std::size_t>(selected);
        const int strength=static_cast<int>(std::lround(
            s.recoilSettings.strength[index]*100.f));
        const int recovery=static_cast<int>(std::lround(
            s.recoilSettings.recovery[index]*100.f));
        SelectObject(dc,regular);
        const bool weaponRecoilOn=s.recoilSettings.enabled[index];
        std::array<std::string,6> controls{};
        if(dynamicSelected){
            write(537,126,s.recoilSettings.dynamicEnabled
                ?"SHARED DYNAMIC: ON":"SHARED DYNAMIC: OFF",
                s.recoilSettings.dynamicEnabled?
                    RGB(108,235,161):RGB(239,197,116));
            controls={{
                s.recoilSettings.dynamicEnabled
                    ?"DISABLE SHARED BUILD-UP":"ENABLE SHARED BUILD-UP",
                "LESS BUILD-UP (-5% PER SHOT)",
                "MORE BUILD-UP (+5% PER SHOT)",
                "LOWER MAX MULTIPLIER (-25%)",
                "HIGHER MAX MULTIPLIER (+25%)",
                "RESET SHARED DYNAMICS"
            }};
        }else{
            write(537,126,weaponRecoilOn?"THIS WEAPON: RECOIL ON"
                :"THIS WEAPON: RECOIL OFF",
                weaponRecoilOn?RGB(108,235,161):RGB(239,197,116));
            controls={{
                weaponRecoilOn?"DISABLE FOR THIS WEAPON":"ENABLE FOR THIS WEAPON",
                "LESS KICK (-10%)",
                "MORE KICK (+10%, MAX 900%)",
                "SLOWER RECOVERY (-10%, MIN 25%)",
                "FASTER RECOVERY (+10%, MAX 900%)",
                "RESET THIS WEAPON"
            }};
        }
        for(int i=0;i<6;++i){
            const int y=157+63*i;
            const bool focus=m.hovered==140+i
                ||(m.navigation.usingStick&&m.navigation.settingsColumn
                    &&m.navigation.actionIndex==i);
            fill(531,y,384,54,focus?RGB(52,83,99):RGB(39,49,63));
            SelectObject(dc,smallFont);
            write(548,y+16,controls[static_cast<std::size_t>(i)],white);
        }
        SelectObject(dc,smallFont);
        if(dynamicSelected){
            const int gain=static_cast<int>(std::lround(
                s.recoilSettings.dynamicBuildUpPerShot*100.f));
            const int cap=static_cast<int>(std::lround(
                s.recoilSettings.dynamicMaxMultiplier*100.f));
            write(539,548,"+ "+std::to_string(gain)+"% PER SHOT  /  CAP "
                +std::to_string(cap)+"%",RGB(84,233,214));
            write(539,579,"Gauss: separate kick/recovery | 2 hands stable",muted);
        }else{
            write(539,548,"KICK "+std::to_string(strength)+"%  /  RECOVERY "+
                std::to_string(recovery)+"%",RGB(84,233,214));
            write(539,579,selected==6
                ?"Gauss: independently tuned short punch + full return"
                :"1 hand: heavy recoil  |  2 hands: stable",muted);
        }
        write(539,614,"Close menu to test. B: BACK",muted);
        write(40,662,m.message.empty()
            ?"Dynamic build-up uses ammo-confirmed shots; Gauss tuned separately."
            :m.message.substr(0,98),RGB(84,233,214));
        fill(797,16,129,57,m.hovered==95
            ?RGB(47,95,110):RGB(39,49,63));
        write(815,30,"BACK",white);
    }else{
    write(38,24,"RIPSHIFT VR  /  HAND CALIBRATION",white);
    SelectObject(dc,smallFont);
    write(40,102,"WEAPON PROFILES",muted);
    write(536,102,"SELECT HAND",muted);
    for(size_t i=0;i<kharvox::kVrCalibrationWeapons.size();++i){
        const int y=136+int(i)*45;
        const bool selected=int(i)==m.selectedWeapon;
        const bool hover=m.hovered==int(i)
            ||(m.navigation.usingStick&&!m.navigation.settingsColumn
                &&m.selectedWeapon==int(i));
        fill(38,y,455,40,selected?RGB(32,89,104):
            hover?RGB(44,54,71):RGB(27,32,43));
        if(selected)fill(38,y,5,40,RGB(55,209,214));
        SelectObject(dc,regular);
        const auto key=kharvox::kVrCalibrationWeapons[i];
        const auto name=KharvoxWeaponKindDisplayName(KharvoxWeaponKindFromKey(key));
        write(56,y+6,name&&*name?name:key,white);
        SelectObject(dc,smallFont);
        const char* state=m.rightSaved[i]&&m.leftSaved[i]?"BOTH"
            :m.rightSaved[i]?"RIGHT":m.leftSaved[i]?"LEFT":"DEFAULT";
        write(365,y+9,state,selected?white:muted);
    }
    const auto button=[&](int x,int y,int w,int h,bool active,bool hovered){
        fill(x,y,w,h,active?RGB(31,104,115):
            hovered?RGB(53,68,84):RGB(39,49,63));
    };
    const auto selectedKind=KharvoxWeaponKindFromKey(m.weapon());
    const bool heavy=selectedKind==KharvoxWeaponKind::GaussCannon
        ||selectedKind==KharvoxWeaponKind::Chaingun;
    const auto gamepadFocus=[&](int action){
        return m.navigation.usingStick&&m.navigation.settingsColumn
            &&kharvox::vrMenuGripGamepadTarget(m.navigation,
                m.selectedWeapon,heavy,
                selectedKind==KharvoxWeaponKind::GaussCannon)==action;
    };
    button(531,151,170,58,!m.rightHand,m.hovered==20||gamepadFocus(20));
    button(715,151,202,58,m.rightHand,m.hovered==21||gamepadFocus(21));
    SelectObject(dc,regular);
    write(555,163,"LEFT",white);write(742,163,"RIGHT",white);
    SelectObject(dc,smallFont);
    write(538,233,s.weaponHandLeft?"WEAPON HELD: LEFT":"WEAPON HELD: RIGHT",muted);
    const bool primarySelected=m.rightHand!=s.weaponHandLeft;
    write(538,267,primarySelected
        ?"PRIMARY: position + rotation"
        :"SUPPORT: position only",muted);
    if(heavy){
        button(531,306,384,49,m.alternateProfile,
            m.hovered==32||gamepadFocus(32));
        write(550,317,m.alternateProfile
            ?(selectedKind==KharvoxWeaponKind::GaussCannon
                ?"GAUSS CANNON: ALT FIRE":"MINIGUN: ALT FIRE")
            :"PROFILE: NORMAL",white);
    }else{
        write(538,306,primarySelected
            ?"Hand locked to gun"
            :"Support rotation on grab",muted);
    }
    button(531,363,384,69,false,m.hovered==30||gamepadFocus(30));
    SelectObject(dc,title);
    write(568,376,"CALIBRATE",white);
    button(531,454,384,69,false,m.hovered==31||gamepadFocus(31));
    SelectObject(dc,regular);
    write(572,474,"RESET THIS WEAPON",white);
    SelectObject(dc,smallFont);
    if(selectedKind==KharvoxWeaponKind::GaussCannon){
        button(531,540,384,50,s.vrLiveSettings.gaussRequireTwoHandFire,
            m.hovered==33||gamepadFocus(33));
        write(549,552,s.vrLiveSettings.gaussRequireTwoHandFire
            ?"TWO-HAND FIRE: ON":"TWO-HAND FIRE: OFF",white);
        write(538,614,"Grip charges; both hands required to fire.",muted);
    }else{
        write(538,556,"Calibrate LEFT and RIGHT independently.",muted);
        write(538,614,"DEFAULT: factory   L/R/B: custom",muted);
    }
    write(538,640,"STICK: NAV   A: SELECT   B: BACK",muted);
    write(40,661,m.message.empty()?"Select your weapon and hand to begin.":m.message,RGB(84,233,214));
        // Back to the list of active mods, leaving weapon profiles intact.
        fill(797,16,129,57,m.hovered==95||gamepadFocus(95)
            ?RGB(47,95,110):RGB(39,49,63));
        SelectObject(dc,smallFont);
        write(815,30,"BACK",white);
    }
    // Render the same right-controller pointer on BOTH the main mods page
    // and weapon calibration page. Hit testing already supports both.
    if(!disconnectView&&!firstTimeSetupView&&!gameplayWelcomeView
       &&!statsLiveView&&!resultView&&!calibrationView
       &&m.cursorX>=0&&m.cursorY>=0
       &&m.cursorX<vrMenuW&&m.cursorY<vrMenuH){
        HBRUSH marker=CreateSolidBrush(RGB(97,255,234));
        HGDIOBJ oldBrush=SelectObject(dc,marker);
        HPEN pen=CreatePen(PS_SOLID,1,RGB(3,35,40));
        HGDIOBJ oldPen=SelectObject(dc,pen);
        const int x=int(m.cursorX),y=int(m.cursorY);
        Ellipse(dc,x-8,y-8,x+8,y+8);
        SelectObject(dc,oldPen);SelectObject(dc,oldBrush);
        DeleteObject(pen);DeleteObject(marker);
    }
    SelectObject(dc,previous);
    DeleteObject(title);DeleteObject(regular);DeleteObject(smallFont);
    // GDI's 32-bit DIB uses BGRA. Convert to straight-alpha RGBA required by
    // VK_FORMAT_R8G8B8A8_SRGB. The panel is intentionally opaque.
    auto* src=static_cast<const unsigned char*>(bits);
    auto* dst=static_cast<unsigned char*>(vrHandMenuSurface.mapped);
    for(size_t i=0;i<size_t(vrMenuW)*vrMenuH;++i){
        const size_t b=i*4;
        dst[b]=src[b+2];dst[b+1]=src[b+1];dst[b+2]=src[b];
        const int x=int(i%vrMenuW),y=int(i/vrMenuW);
        dst[b+3]=disconnectView
            ?(x>=bannerX&&x<bannerX+bannerW
                &&y>=bannerY&&y<bannerY+bannerH?255:0)
            :firstTimeSetupView
                ?(x>=setupX&&x<setupX+setupW
                    &&y>=setupY&&y<setupY+setupH?255:0)
            :gameplayWelcomeView
                ?(x>=welcomeX&&x<welcomeX+welcomeW
                    &&y>=welcomeY&&y<welcomeY+welcomeH?255:0)
            :resultView
                ?(x>=resultX&&x<resultX+resultW
                    &&y>=resultY&&y<resultY+resultH?245:0)
            :objectCalibrationView
                ?(x>=38&&x<698&&y>=150&&y<571?245:0)
            :(statsLiveView)
                ?([&](){
                    for(int i=0;i<kharvox::kVrStatsItemCount;++i)if(statsVisible[i]){
                        const int width=kharvox::campaignStatsPanelWidth(i);
                        const int left=kharvox::statsPanelX(
                            statsCorners[i],vrMenuW,width);
                        const int top=kharvox::statsPanelY(
                            statsCorners[i],vrMenuH,counterH);
                        if(x>=left&&x<left+width&&y>=top&&y<top+counterH)
                            return 220;
                    }
                    return 0;
                }())
            :(!calibrationView
                ||(x>=22&&x<612&&y>=144&&y<576)
                ||(devPanel&&x>=630&&x<938&&y>=144&&y<576)?255:0);
    }
    SelectObject(dc,oldBitmap);
    DeleteObject(dib);
    DeleteDC(dc);
}
void recordVrHandMenuCopy(VkCommandBuffer command,uint32_t index){
    auto& menu=vrHandMenuSurface;
    paintVrHandMenu();
    VkImage destination=menu.target.images[index].image;
    VkImageMemoryBarrier transition{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    transition.oldLayout=menu.target.initialized[index]
        ?VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED;
    transition.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transition.srcAccessMask=menu.target.initialized[index]?VK_ACCESS_MEMORY_READ_BIT:0;
    transition.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    transition.srcQueueFamilyIndex=transition.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    transition.image=destination;
    transition.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    s.vk.cmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&transition);
    VkBufferImageCopy copy{};
    copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
    copy.imageExtent={vrMenuW,vrMenuH,1};
    s.vk.cmdCopyBufferToImage(command,menu.staging,destination,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    transition.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transition.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    transition.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    transition.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT;
    s.vk.cmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&transition);
    menu.target.initialized[index]=true;
}
