#pragma once

#include "object_base.hxx"

#include <webgpu/webgpu_cpp.h>

class TextureBlitter : public ObjectBase {
    wgpu::Device device;
    wgpu::Queue queue;

    wgpu::BindGroupLayout bind_group_layout;
    wgpu::Buffer extend_uniform;
    wgpu::RenderPipeline pipeline;

  public:
    TextureBlitter() = default;

    TextureBlitter(
        wgpu::Device device,
        wgpu::Queue queue,
        wgpu::TextureFormat src_format,
        wgpu::TextureFormat dst_format
    );

    void blit(
        wgpu::CommandEncoder& encoder,
        wgpu::TextureView src_texture,
        wgpu::TextureView dst_texture,
        uint32_t width,
        uint32_t height
    ) const;
};
