// Transparent ring texture used by the normal Gunslinger positioning overlay.
// Three perpendicular quads represent an in-world ellipsoid sized to native
// interaction bounds. This is not part of chainsaw gameplay or detection.
bool initializeGunslingerZonePreview(const std::vector<int64_t>& formats) {
    if (gunslingerZonePreviewReady) return true;
    auto& vk = s.vk;
    if (!vk.createBuffer || !vk.destroyBuffer || !vk.getBufferMemoryRequirements
        || !vk.allocateMemory || !vk.freeMemory || !vk.bindBufferMemory
        || !vk.mapMemory || !vk.unmapMemory || !vk.getPhysicalDeviceMemoryProperties
        || !vk.cmdCopyBufferToImage || !vk.queueWaitIdle) return false;
    constexpr int64_t format = VK_FORMAT_R8G8B8A8_SRGB;
    if (std::find(formats.begin(), formats.end(), format) == formats.end()) return false;
    std::vector<unsigned char> pixels;
    UINT width{}, height{};
    // Fully transparent centre and corners, thin low-opacity circumference.
    width=height=128;
    pixels.resize(size_t(width)*height*4);
    for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x){
        const float nx=(float(x)+.5f)/float(width)*2.f-1.f;
        const float ny=(float(y)+.5f)/float(height)*2.f-1.f;
        const auto p=(size_t(y)*width+x)*4;
        const float r=std::sqrt(nx*nx+ny*ny);
        const float distance=(r-.91f)/.025f;
        const float a=r<1.f?std::clamp(225.f*std::exp(-.5f*distance*distance),0.f,225.f):0.f;
        const auto opacity=static_cast<unsigned char>(a);
        // Premultiplied colour for the default OpenXR compositing mode.
        pixels[p+0]=static_cast<unsigned char>((75u*opacity)/255u);
        pixels[p+1]=static_cast<unsigned char>((230u*opacity)/255u);
        pixels[p+2]=opacity;
        pixels[p+3]=opacity;
    }

    VkBuffer buffer{};
    VkDeviceMemory memory{};
    VkCommandPool pool{};
    bool ok = false;
    // Every failure unwinds resources. Submitted work is completed before
    // freeing its staging buffer/pool or releasing the OpenXR image.
    do {
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = pixels.size(); bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if (vk.createBuffer(s.device, &bi, nullptr, &buffer) != VK_SUCCESS) break;
        VkMemoryRequirements requirements{};
        vk.getBufferMemoryRequirements(s.device, buffer, &requirements);
        VkPhysicalDeviceMemoryProperties properties{};
        vk.getPhysicalDeviceMemoryProperties(s.physical, &properties);
        uint32_t type = properties.memoryTypeCount;
        for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
            constexpr auto flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            if ((requirements.memoryTypeBits & (1u << i))
                && (properties.memoryTypes[i].propertyFlags & flags) == flags) { type = i; break; }
        }
        if (type == properties.memoryTypeCount) break;
        VkMemoryAllocateInfo mi{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mi.allocationSize = requirements.size; mi.memoryTypeIndex = type;
        if (vk.allocateMemory(s.device, &mi, nullptr, &memory) != VK_SUCCESS
            || vk.bindBufferMemory(s.device, buffer, memory, 0) != VK_SUCCESS) break;
        void* mapped{};
        if (vk.mapMemory(s.device, memory, 0, pixels.size(), 0, &mapped) != VK_SUCCESS) break;
        std::memcpy(mapped, pixels.data(), pixels.size()); vk.unmapMemory(s.device, memory);
        XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        ci.createFlags = XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT;
        ci.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        ci.format = format; ci.sampleCount = 1; ci.width = width; ci.height = height;
        ci.faceCount = 1; ci.arraySize = 1; ci.mipCount = 1;
        if (XR_FAILED(s.createSwapchain(s.session, &ci, &gunslingerZonePreview.handle))) break;
        gunslingerZonePreview.width = width; gunslingerZonePreview.height = height;
        if (!initializeSwapchainImages(gunslingerZonePreview)) break;
        XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t index{};
        if (XR_FAILED(s.acquireImage(gunslingerZonePreview.handle, &acquire, &index))
            || index >= gunslingerZonePreview.images.size()) break;
        XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wait.timeout = XR_INFINITE_DURATION;
        if (s.waitImage(gunslingerZonePreview.handle, &wait) != XR_SUCCESS) break;
        VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; pi.queueFamilyIndex = s.queueFamily;
        if (vk.createCommandPool(s.device, &pi, nullptr, &pool) != VK_SUCCESS) break;
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = pool; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
        VkCommandBuffer command{};
        if (vk.allocateCommandBuffers(s.device, &ai, &command) != VK_SUCCESS) break;
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vk.beginCommandBuffer(command, &begin) != VK_SUCCESS) break;
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = gunslingerZonePreview.images[index].image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vk.cmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        VkBufferImageCopy copy{}; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {width, height, 1};
        vk.cmdCopyBufferToImage(command, buffer, barrier.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        vk.cmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        if (vk.endCommandBuffer(command) != VK_SUCCESS) break;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
        {
            QueueAccessScope access;
            if (vk.queueSubmit(s.queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS) break;
            if (vk.queueWaitIdle(s.queue) != VK_SUCCESS) break;
        }
        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        ok = XR_SUCCEEDED(s.releaseImage(gunslingerZonePreview.handle, &release));
    } while (false);
    if (pool) vk.destroyCommandPool(s.device, pool, nullptr);
    if (buffer) vk.destroyBuffer(s.device, buffer, nullptr);
    if (memory) vk.freeMemory(s.device, memory, nullptr);
    if (!ok && gunslingerZonePreview.handle) { s.destroySwapchain(gunslingerZonePreview.handle); gunslingerZonePreview = {}; }
    gunslingerZonePreviewReady = ok;
    return ok;
}
