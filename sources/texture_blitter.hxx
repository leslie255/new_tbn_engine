#pragma once

#include "object.hxx"

#include <webgpu/webgpu_cpp.h>

class TextureBlitter : public ObjectBase {
    wgpu::Device device;
    wgpu::Queue queue;

    wgpu::BindGroupLayout bind_group_layout;
    wgpu::Buffer extend_uniform;
    wgpu::RenderPipeline pipeline;

  public:
    TextureBlitter() = default;
    constexpr TextureBlitter(nullptr_t) {}

    bool operator==(nullptr_t) {
        return this->device == nullptr;
    }

    struct CreateInfo {
        wgpu::TextureFormat src_format;
        wgpu::TextureFormat dst_format;
        uint32_t width;
        uint32_t height;
    };

    TextureBlitter(wgpu::Device device, wgpu::Queue queue, const CreateInfo& info);

    void resize(uint32_t width, uint32_t height);

    void blit(
        wgpu::CommandEncoder& encoder,
        wgpu::TextureView src_texture,
        wgpu::TextureView dst_texture
    ) const;
};
