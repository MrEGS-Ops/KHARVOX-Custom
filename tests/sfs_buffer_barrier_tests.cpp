#include <cassert>
#ifdef _WIN32
#include "../src/sfs/NativeSfs.cpp"
namespace kharvox::sfs {
CompiledShader compileStereoShader(const std::vector<uint32_t>&,const ShaderCompileOptions&){throw std::runtime_error("Unexpected compilation");}
bool hasStereoStorageOutput(const std::vector<uint32_t>&){throw std::runtime_error("Unexpected inspection");}
bool needsHeadsetProjection(const std::vector<uint32_t>&,bool,bool){throw std::runtime_error("Unexpected projection");}
}

static unsigned barrierCalls{};
void VKAPI_CALL recordBufferBarrier(VkCommandBuffer,VkPipelineStageFlags src,VkPipelineStageFlags dst,VkDependencyFlags deps,uint32_t nm,const VkMemoryBarrier* m,uint32_t nb,const VkBufferMemoryBarrier* b,uint32_t ni,const VkImageMemoryBarrier*) {
    assert(src==VK_PIPELINE_STAGE_TRANSFER_BIT&&dst==VK_PIPELINE_STAGE_VERTEX_INPUT_BIT&&deps==0);
    assert(nm==1&&m&&m->srcAccessMask==VK_ACCESS_TRANSFER_WRITE_BIT);
    assert(nb==1&&b&&b->offset==32&&b->size==64&&ni==0);++barrierCalls;
}
int main(){
    using namespace kharvox::sfs;
    void* dispatch = reinterpret_cast<void*>(uintptr_t(100));
    const auto cb = reinterpret_cast<VkCommandBuffer>(&dispatch);
    auto state = std::make_shared<State>();
    devices[dispatch] = state;
    state->dispatch.vkCmdPipelineBarrier=recordBufferBarrier;
    VkMemoryBarrier memory{VK_STRUCTURE_TYPE_MEMORY_BARRIER};memory.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    VkBufferMemoryBarrier buffer{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};buffer.offset=32;buffer.size=64;
    // No command registry entry exists, and the metadata lock is unavailable.
    // A buffer-only barrier must still reach the device unchanged.
    {
        std::unique_lock held(state->mutex);
        barriers(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,0,1,&memory,1,&buffer,0,nullptr);
    }
    assert(barrierCalls==1);
}
#else
int main(){}
#endif
