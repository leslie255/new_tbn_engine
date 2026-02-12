#pragma once

#include <GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>

#include "surface.hxx"

class Swapchain {
    wgpu::Device device = nullptr;

    uint32_t width = 0;
    uint32_t height = 0;

    SurfaceFormat format = {
        .color_format = wgpu::TextureFormat::Undefined,
        .depth_stencil_format = wgpu::TextureFormat::Undefined,
    };

    wgpu::Surface surface = nullptr;
    wgpu::Texture depth_stencil_texture = nullptr;

  public:
    /// If `true`, all resizes get deferred until the next `get_current_surface`.
    bool defer_resize = false;

    struct CreateInfo {
        bool create_depth_stencil_texture;
        wgpu::TextureFormat depth_stencil_format;
    };

    Swapchain() = default;
    Swapchain(
        const wgpu::Instance& instance,
        const wgpu::Adapter& adapter,
        wgpu::Device device,
        GLFWwindow* window,
        const CreateInfo& info
    );

    uint32_t get_width() const;

    uint32_t get_height() const;

    SurfaceFormat get_format() const;

    Surface get_current_surface();

    void reconfigure_for_size(uint32_t width, uint32_t height);

    void present();
};

