#pragma once

#include <GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>

#include "canvas.hxx"

class Swapchain {
    wgpu::Device device = nullptr;

    uint32_t width = 0;
    uint32_t height = 0;

    CanvasFormat format = {
        .color_format = wgpu::TextureFormat::Undefined,
        .depth_stencil_format = wgpu::TextureFormat::Undefined,
    };

    wgpu::Surface surface = nullptr;
    wgpu::Texture depth_stencil_texture = nullptr;

  public:
    /// If `true`, all resizes get deferred until the next `get_current_surface`.
    bool defer_resize = false;

    struct CreateInfo {
        bool create_depth_stencil_texture = false;

        wgpu::TextureFormat depth_stencil_format = wgpu::TextureFormat::Undefined;

        /// Whether to prefer SRGB output textures.
        /// If surface does not support SRGB output, linear output would be used instead.
        bool prefer_srgb = true;

        /// Whether to prefer float over unorm for output color textures.
        /// Only applicable if `prefer_srgb == `false`, or if surface does not support SRGB output.
        bool prefer_float = false;
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

    CanvasFormat get_format() const;

    Canvas get_current_canvas();

    void reconfigure_for_size(uint32_t width, uint32_t height);

    void present();
};
