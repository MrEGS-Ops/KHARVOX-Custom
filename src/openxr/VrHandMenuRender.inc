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
    fill(0,0,vrMenuW,vrMenuH,RGB(15,18,24));
    fill(0,0,vrMenuW,84,RGB(24,29,40));
    fill(38,82,884,3,RGB(55,209,214));
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
    if(m.page==kharvox::VrMenuPage::Mods){
        write(38,24,"KHARVOX VR  /  MOD SETTINGS",white);
        SelectObject(dc,smallFont);
        write(40,102,"CONFIGURABLE MODS",muted);
        write(535,102,"LIVE SETTINGS",muted);
        const auto entries=activeVrMenuEntries();
        const int count=static_cast<int>(entries.size());
        const int selected=std::clamp(m.selectedRow,0,std::max(0,count-1));
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
            // The left edge lists ALL enabled mods, but the five supported
            // adjustable ones remain visible even when switched off.
            if(title.size()>29)title.resize(29);
            write(55,y+10,title,entries[index].devSection
                ?RGB(84,233,214):entries[index].enabled?white:muted);
            write(entries[index].devSection?421:436,y+10,
                entries[index].devSection?"VIEW":entries[index].enabled?"ON":"OFF",
                entries[index].devSection?RGB(84,233,214):
                    entries[index].enabled?RGB(108,235,161):RGB(151,158,171));
        }
        if(count>10){
            SelectObject(dc,smallFont);
            write(48,602,"LEFT STICK: scroll  ("+
                std::to_string(selected+1)+"/"+std::to_string(count)+")",muted);
        }
        const bool devSelected=count>0&&entries[selected].devSection;
        if(devSelected){
            const auto dev=devVrMenuEntries();
            const int n=static_cast<int>(dev.size());
            const int focus=std::clamp(m.navigation.actionIndex,0,std::max(0,n-1));
            SelectObject(dc,regular);
            write(536,126,"DEV  /  READ-ONLY",white);
            SelectObject(dc,smallFont);
            write(538,156,"STATUS ONLY - CHANGE IN DESKTOP LAUNCHER",muted);
            for(int shown=0;shown<10;++shown){
                const int index=shown+m.devScroll;
                if(index>=n)break;
                const int y=186+shown*39;
                const bool selected=m.navigation.settingsColumn&&index==focus;
                const bool hovered=m.hovered==1200+index
                    ||(selected&&m.navigation.usingStick);
                fill(531,y,384,36,hovered?RGB(48,80,92)
                    :selected?RGB(35,66,79):RGB(27,32,43));
                std::string label=dev[index].title;
                if(label.size()>27)label.resize(27);
                write(545,y+8,label,white);
                const std::string status=dev[index].status;
                write(status=="LOCKED"?837:status=="DESKTOP"?829:856,y+8,
                    status,status=="LOCKED"?RGB(84,233,214):muted);
            }
            write(544,598,"DEV ITEMS: "+std::to_string(n)
                +(n>10?"  ("+std::to_string(focus+1)+"/"+std::to_string(n)+")":""),muted);
        }else{
        const auto mod=count>0?entries[selected].mod:kharvox::VrLiveMod::Other;
        const bool editable=kharvox::vrLiveModEditable(mod);
        const bool enabled=count>0&&entries[selected].enabled;
        SelectObject(dc,regular);
        write(536,126,count>0?entries[selected].title:"No active mods",white);
        const bool focusToggle=m.navigation.usingStick
            &&m.navigation.settingsColumn&&m.navigation.actionIndex==0;
        fill(531,159,384,60,focusToggle?RGB(52,83,99):
            editable?RGB(39,49,63):RGB(30,35,44));
        fill(553,172,36,36,editable?RGB(54,79,91):RGB(45,49,56));
        if(enabled)fill(562,181,18,18,RGB(76,231,167));
        SelectObject(dc,regular);
        write(603,169,editable?(enabled?"ENABLED   (TOGGLE)":"DISABLED   (TOGGLE)")
            :(enabled?"ENABLED   (READ-ONLY)":"READ-ONLY"),editable?white:muted);
        SelectObject(dc,smallFont);
        if(!editable)
            write(545,251,"Configure in desktop launcher only.",muted);
        else{
            write(540,226,"Point with RIGHT hand; trigger to select.",muted);
            std::array<std::string,5> options{};
            int optionCount{};
            switch(mod){
                case kharvox::VrLiveMod::WeaponGrip:
                    options[0]="OPEN WEAPON CALIBRATION";
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
                    options[0]="SLOWER (-10 HZ)";
                    options[1]="FASTER (+10 HZ)";
                    options[2]="RESET TO DESKTOP PRESET";
                    optionCount=3;
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
                const int y=248+68*index;
                const bool focused=m.hovered==130+index
                    ||(m.navigation.usingStick&&m.navigation.settingsColumn
                        &&m.navigation.actionIndex==index+1);
                fill(531,y,384,58,focused
                     ?RGB(52,83,99):RGB(39,49,63));
                SelectObject(dc,smallFont);
                write(548,y+17,options[index],white);
            }
            if(mod==kharvox::VrLiveMod::Killtime){
                write(548,543,"GLORY KILL: "+(s.vrLiveSettings.gloryKillHz>0
                    ?std::to_string(s.vrLiveSettings.gloryKillHz)+" Hz"
                    :std::string("desktop preset")),RGB(84,233,214));
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
            }else if(mod==kharvox::VrLiveMod::Gunslinger){
                write(548,609,"HAND ZONES UPDATE INSTANTLY",RGB(84,233,214));
            }
        }
        }
        SelectObject(dc,smallFont);
        write(538,623,"LEFT STICK NAV / A SELECT / B BACK",muted);
        write(40,662,m.message.empty()?"No restart required. Changes stored in UserData."
            :m.message.substr(0,105),RGB(84,233,214));
    }else{
    write(38,24,"KHARVOX VR  /  HAND CALIBRATION",white);
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
            :m.rightSaved[i]?"RIGHT":m.leftSaved[i]?"LEFT":"-";
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
                m.selectedWeapon,heavy)==action;
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
        button(531,306,384,49,m.alternateProfile,m.hovered==32||gamepadFocus(32));
        write(550,317,m.alternateProfile
            ?(selectedKind==KharvoxWeaponKind::GaussCannon
                ?"PROFILE: SIEGE":"PROFILE: TURRET")
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
    write(538,551,"LEFT / RIGHT / BOTH = gun grip saved",muted);
    write(538,582,"LEFT STICK NAV / A SELECT / B BACK",muted);
    write(538,611,"Hold RIGHT stick 1s: close",muted);
    write(40,661,m.message.empty()?"Select your weapon and hand to begin.":m.message,RGB(84,233,214));
        // Back to the list of active mods, leaving weapon profiles intact.
        fill(797,16,129,57,m.hovered==95||gamepadFocus(95)
            ?RGB(47,95,110):RGB(39,49,63));
        SelectObject(dc,smallFont);
        write(815,30,"BACK",white);
    }
    // Render the same right-controller pointer on BOTH the main mods page
    // and weapon calibration page. Hit testing already supports both.
    if(m.cursorX>=0&&m.cursorY>=0&&m.cursorX<vrMenuW&&m.cursorY<vrMenuH){
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
        dst[b]=src[b+2];dst[b+1]=src[b+1];dst[b+2]=src[b];dst[b+3]=255;
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
