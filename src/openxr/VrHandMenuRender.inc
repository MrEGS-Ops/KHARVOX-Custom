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
    const bool editingHud=m.mode==kharvox::VrHandMenuMode::Menu&&m.page==kharvox::VrMenuPage::Stats&&!disconnectView;
    const bool radialView=m.mode==kharvox::VrHandMenuMode::Menu&&m.page==kharvox::VrMenuPage::Mods;
    const bool pickingWeapon=m.mode==kharvox::VrHandMenuMode::Menu&&(m.calibrationWheel||m.waitingForEquip);
    static constexpr int radialSize=406,radialX=277,radialY=157;
    std::vector<unsigned char> radialAlpha;
    int descriptionX=-1,descriptionY=0,descriptionHeight=0;
    float descriptionOpacity=0.f;
    int noticeX=-1,noticeY=0,noticeW=0,noticeH=0;
    const auto noticeNow=GetTickCount64();
    const bool pendingConfirmation=m.radialResetRow>=0||m.resetWeaponPending
        ||(m.pendingStatsReset>=0&&m.confirmDeathResetUntilMs&&noticeNow<=m.confirmDeathResetUntilMs);
    const float noticeOpacity=m.messageNotice.update(m.message,noticeNow,pendingConfirmation);
    const COLORREF descriptionBackground=RGB(21,31,39);
    const auto& layout=editingHud?m.hudDraft:s.vrLiveSettings;
    const std::array<bool,kharvox::kVrStatsItemCount> statsVisible{
        layout.deathCounterEnabled,
        layout.deathStatisticsEnabled,
        layout.campaignTimerEnabled,
        layout.sessionTimerEnabled,
        layout.levelDisplayEnabled[0],
        layout.levelDisplayEnabled[1],
        layout.levelDisplayEnabled[2]};
    const std::array<int,kharvox::kVrStatsItemCount> statsCorners{
        layout.deathCounterCorner,
        layout.deathStatisticsCorner,
        layout.campaignTimerCorner,
        layout.sessionTimerCorner,
        layout.levelDisplayPosition[0],
        layout.levelDisplayPosition[1],
        layout.levelDisplayPosition[2]};
    const bool statsLiveView=kharvox::campaignStatsCanDraw(
        statsGameplay,m.mode==kharvox::VrHandMenuMode::Closed,
        firstTimeSetupView||gameplayWelcomeView,disconnectView)
        &&std::any_of(statsVisible.begin(),statsVisible.end(),
            [](bool on){return on;});
    const bool guideView=statsGameplay&&guidedOverlayVisible()&&m.mode==kharvox::VrHandMenuMode::Closed
        &&!disconnectView&&!firstTimeSetupView&&!gameplayWelcomeView&&!resultView&&!objectCalibrationView;
    const bool gunslingerView=statsGameplay&&gunslingerTutorialVisibleInGameplay()&&!guideView
        &&!disconnectView&&!firstTimeSetupView&&!gameplayWelcomeView&&!resultView&&!objectCalibrationView;
    constexpr int helperX=145,helperY=24,helperW=670,helperH=108;
    const bool weaponGuide=m.mode==kharvox::VrHandMenuMode::Calibrating&&m.liveCapture==kharvox::VrLiveCapture::None&&m.weaponCalibrationStage;
    const bool calibrationGuide=(guidedCalibrationVisible()||weaponGuide)&&!disconnectView&&!firstTimeSetupView&&!gameplayWelcomeView&&!resultView;
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
    if(editingHud)fill(0,0,960,720,RGB(50,54,58));
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
        // Object calibration uses only the shared top instructor.
    }else if(statsLiveView||guideView||gunslingerView||editingHud){
        if(statsLiveView||editingHud)for(int i=0;i<kharvox::kVrStatsItemCount;++i)if(statsVisible[i]||(editingHud&&m.hudDrag==i)){
            const int width=kharvox::campaignStatsPanelWidth(i);
            const int x=editingHud&&m.hudDrag==i?m.hudX:kharvox::statsPanelX(statsCorners[i],vrMenuW,width);
            const int y=editingHud&&m.hudDrag==i?m.hudY:kharvox::statsPanelY(statsCorners[i],vrMenuH,counterH);
            fill(x,y,width,counterH,RGB(19,23,30));
            fill(x,y,4,counterH,i>=2&&i!=6?RGB(55,209,214):RGB(210,69,76));
        }
    }else if(calibrationView){
        if(devPanel){
            fill(630,144,308,432,RGB(18,29,41));
            fill(630,144,5,432,RGB(55,209,214));
            fill(630,198,308,2,RGB(76,88,99));
        }
    }else if(radialView||pickingWeapon){
        fill(0,0,960,720,RGB(0,0,0));
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
    auto noticeColor=[&](COLORREF color,COLORREF background,float opacity){
        auto blend=[&](int from,int to){return int(from+(to-from)*opacity+.5f);};
        return RGB(blend(GetRValue(background),GetRValue(color)),
            blend(GetGValue(background),GetGValue(color)),blend(GetBValue(background),GetBValue(color)));
    };
    const auto footerNoticeColor=noticeColor(RGB(84,233,214),RGB(15,18,24),noticeOpacity);
    auto wrap=[&](int x,int y,int width,int height,const std::string& value,COLORREF color){
        RECT rc{x,y,x+width,y+height};SetTextColor(dc,color);
        DrawTextA(dc,value.c_str(),static_cast<int>(value.size()),&rc,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX);
    };
    // Calibration instructions use one palette: controls and hold/release
    // actions yellow, explanatory text (including save/cancel) normal white.
    // Measure with the currently selected font and wrap at word boundaries.
    auto calibrationText=[&](int x,int y,int width,int height,const std::string& value,float opacity=1.f){
        TEXTMETRICA tm{};
        GetTextMetricsA(dc,&tm);
        const int lineHeight=tm.tmHeight+tm.tmExternalLeading;
        const auto lower=[](std::string word){
            for(char& c:word)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
            return word;
        };
        const auto key=[&](std::string word){
            while(!word.empty()&&(word.back()==':'||word.back()==';'||word.back()==','||word.back()=='.'))
                word.pop_back();
            return lower(word);
        };
        int px=x,py=y;
        size_t i=0;
        while(i<value.size()){
            const size_t start=i;
            const bool space=value[i]==' ';
            while(i<value.size()&&(value[i]==' ')==space)++i;
            const std::string token=value.substr(start,i-start);
            SIZE extent{};
            GetTextExtentPoint32A(dc,token.c_str(),int(token.size()),&extent);
            if(space){
                if(px>x)px+=extent.cx;
                continue;
            }
            if(px>x&&px+extent.cx>x+width){px=x;py+=lineHeight;}
            if(py+lineHeight>y+height)break;
            const auto word=key(token);
            size_t next=i;
            while(next<value.size()&&value[next]==' ')++next;
            const auto nextWord=key(value.substr(next,value.find(' ',next)-next));
            const bool control=word=="hold"||word=="release"||word=="grip"
                ||word=="sticks"||word=="stick"||word=="trigger"||word=="rt"||word=="lt"
                ||word=="b"||word=="a"||word=="l3"||word=="r3"
                ||((word=="left"||word=="right")&&(nextWord=="stick"||nextWord=="grip"))
                ||(word=="offhand"&&nextWord=="trigger");
            write(px,py,token,noticeColor(control?RGB(255,225,54):white,RGB(20,29,38),opacity));
            px+=extent.cx;
        }
    };
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
        RECT notice{setupX+25,setupY+344,setupX+setupW-25,setupY+372};
        SetTextColor(dc,muted);
        DrawTextA(dc,"(This notice will only appear once)",-1,&notice,
            DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        fill(setupX+25,setupY+setupH-75,setupW-50,55,RGB(28,57,66));
        SelectObject(dc,regular);
        const std::array<std::string,3> parts{"Press ","L3 + R3"," together to confirm."};
        std::array<SIZE,3> sizes{};
        int total=0;
        for(size_t i=0;i<parts.size();++i){
            GetTextExtentPoint32A(dc,parts[i].c_str(),static_cast<int>(parts[i].size()),&sizes[i]);
            total+=sizes[i].cx;
        }
        int x=setupX+(setupW-total)/2;
        for(size_t i=0;i<parts.size();++i){
            write(x,setupY+setupH-61,parts[i],i==1?RGB(255,225,54):RGB(84,233,214));
            x+=sizes[i].cx;
        }
    }else if(gameplayWelcomeView){
        auto centerLine=[&](int y,const std::string& value,COLORREF color){
            RECT rc{welcomeX+24,y,welcomeX+welcomeW-24,y+44};
            SetTextColor(dc,color);
            DrawTextA(dc,value.c_str(),static_cast<int>(value.size()),&rc,
                DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        };
        SelectObject(dc,title);
        centerLine(welcomeY+14,"RIPSHIFT - READY TO PLAY",white);
        SelectObject(dc,regular);
        centerLine(welcomeY+113,"Your basic VR experience is already configured.",white);
        centerLine(welcomeY+161,"Fine tune it to your liking.",white);
        fill(welcomeX+25,welcomeY+251,welcomeW-50,70,RGB(28,57,66));
        SelectObject(dc,title);
        const std::array<std::string,3> parts{"Hold ","R3"," to adjust settings."};
        std::array<SIZE,3> sizes{};
        int total=0;
        for(size_t i=0;i<parts.size();++i){
            GetTextExtentPoint32A(dc,parts[i].c_str(),static_cast<int>(parts[i].size()),&sizes[i]);
            total+=sizes[i].cx;
        }
        int x=welcomeX+(welcomeW-total)/2;
        for(size_t i=0;i<parts.size();++i){
            write(x,welcomeY+268,parts[i],i==1?RGB(255,225,54):RGB(84,233,214));
            x+=sizes[i].cx;
        }
        SelectObject(dc,smallFont);
        centerLine(welcomeY+349,"(This notice will only appear once)",muted);
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
        // Instructions rendered by calibrationGuide below.
    }else if(statsLiveView||guideView||gunslingerView||editingHud){
        if(statsLiveView||editingHud)for(int i=0;i<kharvox::kVrStatsItemCount;++i)if(statsVisible[i]||(editingHud&&m.hudDrag==i)){
            const int width=kharvox::campaignStatsPanelWidth(i);
            const int x=editingHud&&m.hudDrag==i?m.hudX:kharvox::statsPanelX(statsCorners[i],vrMenuW,width);
            const int y=editingHud&&m.hudDrag==i?m.hudY:kharvox::statsPanelY(statsCorners[i],vrMenuH,counterH);
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
        if(editingHud){
            SelectObject(dc,smallFont);
            write(332,335,"CLICK AND DRAG INTO POSITION",muted);
            for(int i=0;i<7;++i){
                if(!statsVisible[i]&&m.hudDrag!=i){
                    fill(700,hudTrayY(i),254,35,RGB(45,49,54));
                    write(710,hudTrayY(i)+5,kharvox::vrStatsLabel(i),muted);
                }else{
                    const int w=kharvox::campaignStatsPanelWidth(i);
                    const int x=m.hudDrag==i?m.hudX:kharvox::statsPanelX(statsCorners[i],960,w);
                    const int y=m.hudDrag==i?m.hudY:kharvox::statsPanelY(statsCorners[i],720,60);
                    const auto edge=i==m.hudSelection?RGB(104,255,228):RGB(50,144,146);
                    fill(x,y,w,2,edge);fill(x,y+58,w,2,edge);fill(x,y,2,60,edge);fill(x+w-2,y,2,60,edge);
                }
            }
            const char* labels[]={"SAVE","CANCEL","REMOVE"};
            for(int i=0;i<3;++i){
                fill(220+i*140,660,137,48,RGB(27,36,42));
                write(245+i*140,672,labels[i],i==2&&(m.hudSelection<0||!kharvox::vrStatsEnabled(layout,m.hudSelection))?muted:white);
            }
        }
    }else if(calibrationView){
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
    }else if(pickingWeapon){
        const bool waiting=m.waitingForEquip;
        fill(140,kharvox::calibrationGuideTop,680,70,RGB(21,31,39));
        fill(140,kharvox::calibrationGuideTop,4,70,RGB(84,233,214));
        SelectObject(dc,regular);
        if(waiting)write(156,kharvox::calibrationGuideTop+5,"Equipping weapon...",RGB(84,233,214));
        else calibrationText(156,kharvox::calibrationGuideTop+5,648,32,"Point at a weapon. Pull that hand's trigger.");
        SelectObject(dc,smallFont);
        if(!waiting)calibrationText(156,kharvox::calibrationGuideTop+36,648,32,"Hold either Grip to switch the pointer.");
    }else if(radialView){
        using namespace kharvox;
        SelectObject(dc,smallFont);
        const auto accent=RGB(112,232,224);
        auto label=[&](int x,int y,int w,int h,const char* value,COLORREF colour){
            RECT rc{x,y,x+w,y+h};SetTextColor(dc,colour);
            DrawTextA(dc,value,-1,&rc,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
        };
        auto control=[&](int x,int y,int w,int h,const char* value,bool hover,COLORREF colour){
            fill(x,y,w,h,hover?accent:RGB(67,87,99));
            fill(x+1,y+1,w-2,h-2,hover?(m.rightTriggerPreviously?RGB(23,84,87):RGB(36,67,77)):RGB(22,32,42));
            label(x+3,y,w-6,h,value,colour);
        };
        // Cached analytic coverage preserves sharp, consistently weighted
        // circular edges and dividers at native compositor resolution.
        static const auto coverage=[] {
            std::vector<RadialCoverage> out(radialSize*radialSize);
            for(int y=0;y<radialSize;++y)for(int x=0;x<radialSize;++x)
                out[y*radialSize+x]=radialCoverage(float(radialX+x)+.5f-480.f,float(radialY+y)+.5f-360.f);
            return out;
        }();
        radialAlpha.resize(coverage.size());
        const bool dev=m.radialDevUnlock.unlocked;
        const COLORREF rim=RGB(67,87,99);
        GdiFlush();
        auto* pixels=static_cast<unsigned char*>(bits);
        for(int y=0;y<radialSize;++y)for(int x=0;x<radialSize;++x){
            const int idx=y*radialSize+x;
            const auto& q=coverage[idx];
            if(q.outer<=0.f)continue;
            const bool hot=q.category>=0&&m.hovered==100+q.category;
            const COLORREF face=q.category<0?(dev&&m.hovered==radialCenter?RGB(25,81,84):RGB(19,31,40)):
                hot&&m.rightTriggerPreviously?RGB(24,93,96):hot||m.radialCategory==q.category?RGB(34,65,76):RGB(24,33,44);
            const float t=q.inner/q.outer;
            const auto blend=[&](int a,int b){return static_cast<unsigned char>(a+(b-a)*t+.5f);};
            auto* px=pixels+4*((radialY+y)*vrMenuW+radialX+x);
            px[0]=blend(GetBValue(rim),GetBValue(face));
            px[1]=blend(GetGValue(rim),GetGValue(face));
            px[2]=blend(GetRValue(rim),GetRValue(face));
            radialAlpha[idx]=static_cast<unsigned char>(226*q.outer+.5f);
        }
        for(int i=0;i<radialCategoryCount;++i){
            const int x=int(480+137*std::cos(radialAngle(i))),y=int(360+137*std::sin(radialAngle(i)));
            label(x-72,y-15,144,29,radialGroups[i].name,white);
            if(i==radialPainsawIndex){
                const bool on=s.customMods.chainsawTriggerThrottle||s.customMods.motionChainsawKillSpeed;
                label(x-40,y+10,80,23,on?"ON":"OFF",on?RGB(111,224,156):muted);
            }
        }
        label(423,341,114,38,dev?"Dev Mode":"_RIPSHIFT",accent);
        if(m.hovered==100+radialPainsawIndex)
            label(320,574,320,28,"More fun with the chainsaw.",muted);
        if(radialHasFlyout(m.radialCategory)){
            const int x=radialPanelX(m.radialCategory);
            const auto& opts=radialGroups[m.radialCategory].settings;
            const int w=radialPanelWidth,h=radialRowHeight,side=radialControlWidth;
            for(int i=0;i<int(opts.size());++i){
                const auto& option=opts[i];
                const int y=radialRowY(m.radialCategory,i),target=radialRowTarget+i*3;
                const bool hot=m.hovered>=target&&m.hovered<target+3;
                if(option.control==RadialControl::Stepper||m.radialResetRow==i){
                    const bool reset=m.radialResetRow==i;
                    control(x,y,side,h,reset?"Yes":"-",m.hovered==target,reset?RGB(255,202,137):accent);
                    control(x+w-side,y,side,h,reset?"No":"+",m.hovered==target+2,accent);
                    fill(x+side+3,y,w-2*side-6,h,RGB(22,32,42));
                    std::string value;
                    if(reset)value=option.label;
                    else if(option.mod==RadialMod::TurnSpeed){
                        const float speed=s.vrLiveSettings.smoothTurnSpeedDegreesPerSec;
                        value=std::to_string(int(speed?speed:s.vrDesktopSmoothTurnSpeed))+" deg/s";
                    }else if(option.mod==RadialMod::Killtime)value=vrLiveGloryKillSpeedText();
                    else{
                        char speed[24]{};
                        std::snprintf(speed,sizeof(speed),"%.1f m/s",s.vrLiveSettings.punchSpeedMetersPerSec);
                        value=speed;
                    }
                    if(!reset)label(x+side,y+1,w-2*side,21,option.label,muted);
                    label(x+side,y+(reset?0:20),w-2*side,reset?h:23,value.c_str(),white);
                }else if(option.control==RadialControl::Toggle||option.control==RadialControl::Choice){
                    const bool toggle=option.control==RadialControl::Toggle;
                    const bool on=option.mod==RadialMod::PhysicalCrouch?s.customMods.physicalCrouch:
                        option.mod==RadialMod::Gunslinger?s.gunslingerTutorial.enabled&&s.gunslingerTutorial.step<kharvox::gunslingerLessonCount:
                        s.vrLiveSettings.gaussRequireTwoHandFire;
                    const char* hands[]{"RIGHT","LEFT","BOTH"};
                    control(x,y,w,h,"",hot,white);
                    label(x+4,y,174,h,option.label,white);
                    label(x+177,y,78,h,toggle?(on?"ON":"OFF"):hands[std::clamp(s.vrLiveSettings.punchHands,0,2)],
                        toggle&&!on?muted:accent);
                }else{
                    control(x,y,w,h,option.label,hot,option.control==RadialControl::Reset?RGB(255,202,137):white);
                }
            }
            // A pending reset is an actionable prompt, not a transient notice.
            if(m.radialResetRow>=0&&!m.message.empty())
                label(x,radialRowY(m.radialCategory,int(opts.size()))+1,w,28,m.message.c_str(),accent);
        }
        // One consistent, non-interactive notification area for all categories.
        // Keep the PAINSAW hover description above it and never move the wheel.
        if(m.radialResetRow<0&&!m.message.empty()&&noticeOpacity>0.f){
            const std::string message=m.message=="Applied LIVE and saved to UserData."?"Saved.":m.message;
            RECT measured{0,0,radialNoticeWidth-20,0};
            DrawTextA(dc,message.c_str(),-1,&measured,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);
            const int height=std::min(radialNoticeBottom-radialNoticeY,int(measured.bottom-measured.top)+16);
            if(height>0){
                noticeX=radialNoticeX;noticeY=radialNoticeY;noticeW=radialNoticeWidth;noticeH=height;
                fill(noticeX,noticeY,noticeW,noticeH,RGB(22,32,42));
                RECT rc{noticeX+10,noticeY+8,noticeX+noticeW-10,noticeY+noticeH-8};
                SetTextColor(dc,accent);
                DrawTextA(dc,message.c_str(),-1,&rc,DT_CENTER|DT_WORDBREAK|DT_NOPREFIX);
            }
        }
        const int desc=m.radialDescription;
        const auto now=GetTickCount64();
        if(radialHasFlyout(desc)&&now>=m.radialDescriptionSince+200){
            descriptionOpacity=std::clamp(float(now-m.radialDescriptionSince-200)/120.f,0.f,1.f);
            if(m.radialDescriptionFadeOut)
                descriptionOpacity*=1.f-std::clamp(float(now-m.radialDescriptionFadeOut)/120.f,0.f,1.f);
            const char* descriptions[]{
                "Get a grip.",
                "Duck low, turn fast. Make Hell keep up with you.",
                "Keep score. Beat your best. Die less.",
                "",
                "Put your fists to work. Rip and tear at your own pace.",
                "Shoot like a gunslinger: reach behind your head or draw from the hip."};
            const int side=radialHasFlyout(m.radialCategory)?m.radialCategory:desc;
            descriptionX=radialDescriptionX(side);descriptionY=radialRowTop;
            RECT rc{0,0,radialPanelWidth-24,0};
            DrawTextA(dc,descriptions[desc],-1,&rc,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);
            descriptionHeight=int(rc.bottom-rc.top)+24;
            const bool pointerOver=m.cursorX>=descriptionX&&m.cursorX<descriptionX+radialPanelWidth
                &&m.cursorY>=descriptionY&&m.cursorY<descriptionY+descriptionHeight;
            if(descriptionOpacity>0.f&&!pointerOver){
                fill(descriptionX,descriptionY,radialPanelWidth,descriptionHeight,descriptionBackground);
                wrap(descriptionX+12,descriptionY+12,radialPanelWidth-24,descriptionHeight-24,descriptions[desc],muted);
            }else descriptionX=-1;
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
        fill(38,584,222,40,RGB(32,45,53));fill(265,584,228,40,RGB(32,45,53));
        write(60,591,"Previous missions",muted);write(289,591,"Next missions",muted);
        fill(797,16,129,57,m.hovered==95?RGB(47,95,110):RGB(39,49,63));
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
        write(40,676,m.message.substr(0,105),footerNoticeColor);
    }else if(m.page==kharvox::VrMenuPage::DevStats){
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
        }
        SelectObject(dc,smallFont);
        write(40,645,"LEFT STICK: select / options   A: confirm   B: back",muted);
        write(40,676,m.message.substr(0,105),footerNoticeColor);
    }else if(m.page==kharvox::VrMenuPage::Dev){
        write(38,24,"RIPSHIFT VR  /  DEV",white);
        SelectObject(dc,smallFont);
        write(40,102,"DEVELOPER TOOLS",muted);
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
            if(title.size()>29)title.resize(29);
            write(55,y+10,title,entries[index].enabled?white:muted);
            write(458,y+10,">",RGB(84,233,214));
        }
        if(count>10){
            SelectObject(dc,smallFont);
            write(48,602,"LEFT STICK: scroll  ("+
                std::to_string(selected+1)+"/"+std::to_string(count)+")",muted);
        }
        // Experimental is the one DEV row whose right-side switches are
        // visible and pointer-clickable as soon as the row is highlighted.
        const bool expInline=count>0
            &&(entries[selected].developer==kharvox::VrMenuDevOption::Experimental
                ||entries[selected].developer==kharvox::VrMenuDevOption::GrappleAnchor
                ||entries[selected].developer==kharvox::VrMenuDevOption::NativeInteractionResearch
);
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
            fill(531,550,384,59,m.hovered==115?RGB(53,95,108):RGB(36,78,92));
            SelectObject(dc,regular);
            write(624,563,"OPEN / TOGGLE >",white);
            SelectObject(dc,smallFont);
            write(538,618,"A: OPEN OPTIONS    B: CLOSE",muted);
        }else{
            SelectObject(dc,regular);
            write(536,126,count>0?entries[selected].title:"No active mods",white);
            using D=kharvox::VrMenuDevOption;
            const auto opt=count>0?entries[selected].developer:D::Experimental;
            SelectObject(dc,smallFont);
            write(538,163,opt==D::Experimental
                ?"OBJECT DISCOVERY MODE"
                :opt==D::GrappleAnchor?"GRAPPLE GRAB - EXPERIMENTAL"
                :opt==D::GaussTwoHandFire?"PERSISTENT FIRE SETTING":"SESSION-ONLY DEVELOPER TOOL",muted);
            if(opt==D::GuidedTesting){
                const char* labels[]{m.guidedTestingEnabled?"GUIDE HUD: ON":"GUIDE HUD: OFF","RETRY CURRENT STEP","SKIP CURRENT STEP","VISUAL CHECK: OK","VISUAL CHECK: FAIL"};
                for(int i=0;i<5;++i){
                    const int top=186+i*68;
                    const bool selected=m.hovered==130+i||(m.navigation.usingStick&&m.navigation.actionIndex==i);
                    fill(531,top,384,58,selected?RGB(52,83,99):RGB(39,49,63));
                    write(548,top+12,labels[i],white);
                }
                write(538,550,"Commands apply after closing DEV.",muted);
                write(538,577,"NEXT / SKIP preserve recorded evidence.",muted);
            }else if(opt==D::NativeInteractionResearch){
                const unsigned ready=KharvoxCameraNativeResearchReady();
                const char* labels[]{"MESH / SURFACE RAY","COLLISION / STATIC WORLD","ACTIVATION / CALLBACK","HELD OBJECT / MOVEMENT","PLAYER MOVEMENT","NATIVE LEDGE CHECKS"};
                for(int i=0;i<7;++i){
                    const int top=186+i*52;const unsigned bit=i?1u<<(i-1):0u;
                    const bool on=i&&((m.nativeResearchTestPage?m.nativeResearchTestMask:m.nativeResearchMask)&bit);
                    const bool selected=m.hovered==130+i||(m.navigation.usingStick&&m.navigation.actionIndex==i);
                    fill(531,top,384,46,selected?RGB(52,83,99):RGB(39,49,63));
                    write(548,top+4,i?labels[i-1]:(m.nativeResearchTestPage?"MODE: TEST":"MODE: COLLECT"),white);
                    if(i){write(843,top+4,on?"ON":"OFF",on?RGB(128,255,177):muted);
                        write(548,top+23,m.nativeResearchTestPage?(bit==8u?"Optional held movement test":"No functional test yet"):(ready&bit)?"Candidate data validated":"Awaiting validated data",muted);}
                    else write(548,top+23,"Press A to switch mode",muted);
                }
                write(538,558,"Select groups; close DEV to run in order.",muted);
                write(538,583,"Only the current group runs.",muted);
                write(538,608,"Research switches reset on restart.",muted);
            }else if(opt==D::GrappleAnchor){
                for(int i=0;i<4;++i){
                    const int top=186+i*60;
                    const bool active=i==5?m.releaseInvestigator:i==4?m.activationInvestigator:!m.activationInvestigator&&(i==0?(!m.dynamicSurfaceOffset&&m.grappleAnchor.enabled):
                        i==1?(!m.dynamicSurfaceOffset&&!m.pistolGrappleEnabled&&m.objectMeleeActivation):
                        i==2?m.pistolGrappleEnabled:m.dynamicSurfaceOffset);
                    const bool selected=m.hovered==130+i||(m.navigation.usingStick&&m.navigation.actionIndex==i);
                    fill(531,top,384,52,selected?RGB(52,83,99):RGB(39,49,63));
                    write(548,top+6,i==0?"GRAPPLE GRAB":i==1?"ORIGINAL MELEE":i==2?"PISTOL GRAPPLE":i==3?"DYNAMIC SURFACE OFFSET":i==4?"ACTIVATION INVESTIGATOR":"RELEASE INVESTIGATOR",white);
                    write(843,top+8,active?"ON":"OFF",active?RGB(128,255,177):muted);
                    write(548,top+27,i==0?"Original grab path":i==1?"Wake props (original only)":i==2?"Hidden pistol / left-hand melee":i==3?"Kick first / bounds contact":i==4?"Weapon hand melee; read-only":"Grab / release; read-only",muted);
                }
                write(538,550,m.activationInvestigator?"Investigator pauses all grabbing.":"Only one grab variant. Reach 2 m.",white);
                write(538,588,"Surface/Pistol/Investigator: session only.",muted);
            }else if(opt!=D::Experimental&&opt!=D::GrappleAnchor&&opt!=D::NativeInteractionResearch&&opt!=D::GuidedTesting){
                fill(531,186,384,58,m.hovered==130
                    ?RGB(52,83,99):RGB(39,49,63));
                write(548,204,opt==D::WeaponStability
                    ?"OPEN WEAPON RECOIL SETTINGS"
                     :opt==D::StatsReset
                        ?"OPEN STATS RESET SETTINGS"
                    :opt==D::GaussTwoHandFire?(s.vrLiveSettings.gaussRequireTwoHandFire?"GAUSS 2-HAND FIRE: ON":"GAUSS 2-HAND FIRE: OFF")
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
                const bool laserSelected=m.hovered==132
                    ||(m.navigation.usingStick&&m.navigation.actionIndex==2);
                fill(531,368,384,77,laserSelected
                    ?RGB(52,83,99):RGB(39,49,63));
                write(548,385,std::string("GRAB REACH LASER: ")
                    +(cfg.grabReachLaserEnabled?"ON":"OFF"),
                    cfg.grabReachLaserEnabled?RGB(128,255,177):white);
                write(548,416,"A / TRIGGER: TOGGLE",muted);
                write(538,480,"Fixed grab reach: 2 metres.",white);
                write(538,562,"Melee activation is under Grapple Grab.",muted);
                const unsigned discoverySource=KharvoxCameraObjectDiscoverySourceMode();
                write(538,620,!m.objectDiscoveryActive()?"SCANNER: OFF":
                    discoverySource==2?"SCANNER: ACTIVE":"SCANNER: UNAVAILABLE",
                    discoverySource==2?RGB(128,255,177):RGB(255,196,112));
            }
            if(opt==D::GaussTwoHandFire){
                write(538,305,"Applies to Gauss alternate fire.",muted);
                write(538,348,"Normal fire remains one-handed.",muted);
            }else if(opt==D::StatsReset){
                write(538,305,"Separate resets for all three Stats.",muted);
                write(538,348,"Each requires a second confirmation.",muted);
            }else if(opt!=D::Experimental&&opt!=D::GrappleAnchor&&opt!=D::NativeInteractionResearch&&opt!=D::GuidedTesting){
                write(538,332,"Enabled tools only show in the matching",muted);
                write(538,361,"calibration screen.",muted);
            }
        } // close the selected-mod options pane before the shared footer
        SelectObject(dc,smallFont);
        fill(797,16,129,57,m.hovered==95
            ?RGB(47,95,110):RGB(39,49,63));
        write(815,30,"BACK",white);
        if(m.navigation.settingsColumn)
            write(538,638,"B: DEV LIST    A: SELECT",muted);
        write(40,662,m.message.empty()?"No restart required. Changes stored in UserData."
            :m.message.substr(0,105),m.message.empty()?RGB(84,233,214):footerNoticeColor);
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
            :m.message.substr(0,98),m.message.empty()?RGB(84,233,214):footerNoticeColor);
        fill(797,16,129,57,m.hovered==95
            ?RGB(47,95,110):RGB(39,49,63));
        write(815,30,"BACK",white);
    }
    if(gunslingerView){
        const auto& t=s.gunslingerTutorial;
        fill(helperX,helperY,helperW,helperH,RGB(16,25,32));
        fill(helperX,helperY,3,helperH,RGB(255,225,54));
        SelectObject(dc,smallFont);
        const bool done=t.step>=kharvox::gunslingerLessonCount;
        const bool assigned=kharvox::gunslingerHolsterConfirmedVisible(t,GetTickCount64());
        const unsigned shownStep=assigned?t.step:t.step+1;
        const std::string titleText=done?"GUNSLINGER / TUTORIAL COMPLETE":
            "GUNSLINGER / "+std::to_string(shownStep)+" OF "+std::to_string(kharvox::gunslingerLessonCount);
        write(helperX+12,helperY+5,titleText,white);
        const int side=kharvox::gunslingerLessonSide(t.step);
        const std::string hand=side==0?"RIGHT":"LEFT";
        std::string hint;
        if(done)hint="Now go rip and tear.";
        else if(assigned)hint="Two buzzes confirm your weapon is holstered.";
        else if(kharvox::gunslingerShowLowerHint(t))
            hint="Keep holding GRIP and LOWER your weapon to your "+hand+" hip.";
        else if(kharvox::gunslingerLessonEvent(t.step)==kharvox::GunslingerLessonEvent::WheelEquip){
            if(s.behindHeadWheelActive&&s.behindHeadWheelSide==side)
                hint="REACH your arm towards a weapon and HOLD GRIP to select it.";
            else hint="REACH behind your head with your "+hand+" hand.";
        }else if(kharvox::gunslingerLessonEvent(t.step)==kharvox::GunslingerLessonEvent::HipAssign){
            if(t.hipReleaseReady)
                hint="That vibration means you're in the holster zone. RELEASE GRIP.";
            else hint="HOLD GRIP and LOWER your weapon to your "+hand+" hip.";
        }else hint="REACH towards your "+hand+" hip and PRESS GRIP to draw your weapon.";
        // Measure actual font widths so individually coloured words wrap
        // without clipping or changing spacing. Only approved keywords yellow.
        const COLORREF yellow=RGB(255,225,54);
        const int left=helperX+12,right=helperX+helperW-12;
        int x=left,y=helperY+34;
        SIZE space{};GetTextExtentPoint32A(dc," ",1,&space);
        std::istringstream words(hint);
        std::string word;
        while(words>>word){
            const auto stop=word.find_last_not_of(".,!?;:");
            const std::string key=stop==std::string::npos?word:word.substr(0,stop+1);
            const bool emphasis=key=="REACH"||key=="HOLD"||key=="RELEASE"
                ||key=="PRESS"||key=="LOWER"||key=="LEFT"||key=="RIGHT"||key=="GRIP";
            SIZE width{};GetTextExtentPoint32A(dc,word.c_str(),static_cast<int>(word.size()),&width);
            if(x!=left&&x+width.cx>right){x=left;y+=26;}
            if(emphasis&&key.size()<word.size()){
                SIZE keyWidth{};GetTextExtentPoint32A(dc,key.c_str(),static_cast<int>(key.size()),&keyWidth);
                write(x,y,key,yellow);write(x+keyWidth.cx,y,word.substr(key.size()),white);
            }else write(x,y,word,emphasis?yellow:white);
            x+=width.cx+space.cx;
        }
    }
    if(guideView){
        const auto& g=guidedRun;const auto plan=guidedPlan(g.id);
        fill(guideX,guideY,guideW,guideH,RGB(12,17,22));
        fill(guideX,guideY,4,guideH,RGB(55,209,214));
        SelectObject(dc,smallFont);
        const std::string phase=g.id>=11?"TEST":g.id>=5?"COLLECT":"REGRESSION";
        unsigned total=0,done=0;for(unsigned bit=5;bit<17;++bit){if(g.activeMask&(1u<<bit))++total;if(g.finished&(1u<<bit))++done;}
        const std::string queue=g.id>=5?"  ["+std::to_string(std::min(total,done+(g.completeAt?0u:1u)))+"/"+std::to_string(total)+"]":"";
        wrap(guideX+14,guideY+5,guideW-28,38,std::string(plan.title)+" / "+phase+queue+"  "+std::to_string(std::min(g.step+1,plan.count))+"/"+std::to_string(plan.count),RGB(84,233,214));
        const std::string& previous=g.previous;
        if(!previous.empty())wrap(guideX+14,guideY+46,guideW-28,46,previous,
            previous.find("SKIP")==0?RGB(255,204,128):RGB(128,255,177));
        const std::string next=g.completeAt?"Module finished; see log for evidence":plan.steps[g.step].text;
        wrap(guideX+14,guideY+100,guideW-28,72,next,g.completeAt?RGB(128,255,177):white);
        fill(guideX+12,guideY+176,guideW-24,3,RGB(58,70,79));
        fill(guideX+12,guideY+176,static_cast<int>((guideW-24)*g.step/plan.count),3,RGB(55,209,214));
        if(guidedResponseVisible()){
            const char* names[]{"NEXT","SKIP","END INVESTIGATION"};
            for(int j=0;j<3;++j){
                const int bx=guideX+12+j*guideButtonStep;
                fill(bx,guideY+guideButtonY,guideButtonW,guideButtonH,guidedHover==j?RGB(45,101,112):RGB(35,43,52));
                write(bx+15,guideY+guideButtonY+3,names[j],white);
            }
            if(guidedHover>=0)
                fill(static_cast<int>(guidedCursorX)-3,static_cast<int>(guidedCursorY)-3,6,6,RGB(97,255,234));
        }
    }
    if(calibrationGuide&&weaponGuide){
        const int top=kharvox::calibrationGuideTop;
        const int buttonY=kharvox::weaponGuideButtonTop;
        const auto accent=RGB(84,233,214);
        fill(12,top,936,kharvox::weaponGuideHeight,RGB(20,29,38));
        fill(12,top,4,kharvox::weaponGuideHeight,accent);
        SelectObject(dc,smallFont);
        const bool complete=m.weaponCalibrationStage==3;
        const bool primary=m.rightHand!=s.weaponHandLeft;
        const std::string hand=m.rightHand?"Right":"Left";
        const std::string heading=std::string(m.weapon())+" / "+
            (complete?"Hand profiles saved":hand+(primary?" primary":" support"));
        write(24,top+6,heading,accent);
        calibrationText(24,top+34,912,40,!complete&&!m.message.empty()?m.message:complete?"Calibrate the other hand or finish.":!m.previewValid?
            "Waiting for controller tracking.":primary?
            "Sticks move the grip. Hold Grip and rotate your hand.":
            "Hold support Grip to test aim. Release it to reposition.",
            !complete&&!m.message.empty()?noticeOpacity:1.f);
        SelectObject(dc,regular);
        const char* controls=complete?(m.calibrationStartedLeft?"RT: right hand   LT / B: finish":"LT: left hand   RT / B: finish"):
            primary?(m.rightHand?"RT: save   B: cancel":"LT: save   B: cancel"):
            (m.rightHand?"Grip + RT: save   B: cancel":"Grip + LT: save   B: cancel");
        calibrationText(24,buttonY+4,complete?912:430,36,controls);
        SelectObject(dc,smallFont);
        if(!complete){
            const auto kind=KharvoxWeaponCurrentKind();
            const bool alternate=kind==KharvoxWeaponKind::GaussCannon||kind==KharvoxWeaponKind::Chaingun;
            const int hovered=kharvox::weaponGuideAction(m.cursorX,m.cursorY,alternate);
            const int left[]{464,626,788};
            const int widths[]{152,152,152};
            const char* labels[]{m.resetWeaponPending?"Confirm reset":"Reset weapon","Change weapon","ALT FIRE"};
            for(int i=0;i<(alternate?3:2);++i){
                const bool selected=i==2&&m.alternateProfile;
                fill(left[i],buttonY,widths[i],40,hovered==i||selected?accent:RGB(65,83,94));
                fill(left[i]+1,buttonY+1,widths[i]-2,38,selected||(i!=2&&hovered==i)?RGB(31,68,75):RGB(27,39,49));
                RECT rc{left[i],buttonY,left[i]+widths[i],buttonY+40};
                SetTextColor(dc,i==0&&m.resetWeaponPending?RGB(255,219,134):selected?accent:white);
                DrawTextA(dc,labels[i],-1,&rc,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            }
        }
    }else if(calibrationGuide){
        const int top=kharvox::calibrationGuideTop;
        constexpr int height=156;
        fill(guideX,top,guideW,height,RGB(12,17,22));
        fill(guideX,top,4,height,RGB(55,209,214));
        SelectObject(dc,smallFont);
        const bool object=s.objectGripCalibrating;
        const bool zone=!object&&kharvox::vrLiveIsGunslingerCapture(m.liveCapture);
        const bool crouch=!object&&!zone&&m.liveCapture!=kharvox::VrLiveCapture::None;
        const bool primary=!object&&!zone&&!crouch&&m.rightHand!=s.weaponHandLeft;
        const bool rightZone=m.liveCapture==kharvox::VrLiveCapture::HipRight||m.liveCapture==kharvox::VrLiveCapture::WheelRight;
        const bool hip=m.liveCapture==kharvox::VrLiveCapture::HipRight||m.liveCapture==kharvox::VrLiveCapture::HipLeft;
        const std::string heading=object?"OBJECT GRIP / "+s.objectGripName:zone?std::string("GUNSLINGER / ")+(rightZone?"RIGHT ":"LEFT ")+(hip?"HOLSTER":"WHEEL"):crouch?"PHYSICAL CROUCH":std::string("WEAPON GRIP / ")+m.weapon()+" / "+(m.rightHand?"RIGHT ":"LEFT ")+(primary?"PRIMARY":"SUPPORT");
        wrap(guideX+14,top+8,guideW-28,38,heading,RGB(84,233,214));
        const std::string instruction=object?(s.supportGripPressed?"Grip held: walk and align hand to frozen object; release to lock":"Sticks fine-tune; offhand trigger = 5 cm; hold Grip to freeze")
            :zone?(m.gunslingerZoneMoving?"Move hand; release Grip to freeze":std::string("Hold ")+(rightZone?"RIGHT":"LEFT")+" Grip to move; release, then move out/in to test")
            :crouch?(m.liveCapture==kharvox::VrLiveCapture::CrouchStanding?"Stand normally; RT captures standing height":"Crouch to target height; RT saves both heights")
            :m.weaponCalibrationStage==3?(m.calibrationStartedLeft
                ?"Left-handed profiles saved. RT: calibrate right-handed weapon; LT or B: finish"
                :"Right-handed profiles saved. LT: calibrate left-handed weapon; RT or B: finish")
            :!m.previewValid?"Waiting for valid controller tracking"
            :primary?(m.calibrationGripHeld?"Rotate weapon hand; release Grip to lock":"Left stick: horizontal / depth; right stick: height; hold Grip to rotate")
            :"Move support hand into desired grip position";
        calibrationText(guideX+14,top+48,guideW-28,50,instruction);
        SelectObject(dc,regular);
        calibrationText(guideX+14,top+104,guideW-28,50,object?"RT: SAVE   B: CANCEL / DROP   GRIP + OFFHAND TRIGGER: RESET":m.weaponCalibrationStage==3?(m.calibrationStartedLeft?"RT: CONTINUE   LT / B: FINISH":"LT: CONTINUE   RT / B: FINISH"):!zone&&!crouch&&!m.rightHand?"LT: SAVE / NEXT    B: CANCEL CURRENT STAGE":"RT: SAVE / NEXT    B: CANCEL CURRENT STAGE");
    }
    // Render the same right-controller pointer on BOTH the main mods page
    // and weapon calibration page. Hit testing already supports both.
    if(!disconnectView&&!firstTimeSetupView&&!gameplayWelcomeView
       &&!statsLiveView&&!guideView&&!gunslingerView&&!resultView&&(!calibrationView||weaponGuide)
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
    GdiFlush();
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
            :weaponGuide?(x>=12&&x<948&&y>=kharvox::calibrationGuideTop&&y<kharvox::calibrationGuideTop+kharvox::weaponGuideHeight?225:devPanel&&x>=630&&x<938&&y>=144&&y<576?255:0)
            :radialView||pickingWeapon?([&](){
                if(radialView&&noticeX>=0&&x>=noticeX&&x<noticeX+noticeW
                    &&y>=noticeY&&y<noticeY+noticeH)return int(226*noticeOpacity+.5f);
                if(radialView&&!radialAlpha.empty()&&x>=radialX&&x<radialX+radialSize&&y>=radialY&&y<radialY+radialSize){
                    const auto alpha=radialAlpha[(y-radialY)*radialSize+x-radialX];
                    if(alpha)return int(alpha);
                }
                if(radialView&&descriptionX>=0&&x>=descriptionX&&x<descriptionX+kharvox::radialPanelWidth
                    &&y>=descriptionY&&y<descriptionY+descriptionHeight){
                    const bool background=src[b]==GetBValue(descriptionBackground)&&src[b+1]==GetGValue(descriptionBackground)&&src[b+2]==GetRValue(descriptionBackground);
                    return int((background?80:205)*descriptionOpacity);
                }
                return src[b]||src[b+1]||src[b+2]?226:0;
            }())
            :editingHud?210
            :calibrationGuide&&x>=guideX&&x<guideX+guideW&&y>=kharvox::calibrationGuideTop&&y<kharvox::calibrationGuideTop+156?205
            :objectCalibrationView
                ?0
            :(statsLiveView||guideView||gunslingerView)
                ?([&](){
                    if(gunslingerView&&x>=helperX&&x<helperX+helperW&&y>=helperY&&y<helperY+helperH)return 205;
                    if(guideView&&x>=guideX&&x<guideX+guideW&&y>=guideY&&y<guideY+guideH)return 205;
                    if(!statsLiveView)return 0;
                    for(int i=0;i<kharvox::kVrStatsItemCount;++i)if(statsVisible[i]||(editingHud&&m.hudDrag==i)){
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
