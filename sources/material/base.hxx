#pragma once

#include <webgpu/webgpu_cpp.h>

#include "../shader_info.hxx"

struct MaterialBase {
    virtual ShaderInfo create_fragment_shader(const wgpu::Device& device) const;

    virtual wgpu::BindGroupLayout create_bind_group_layout(const wgpu::Device& device) const;

    virtual wgpu::BindGroup create_bind_group(
        const wgpu::Device& device,
        wgpu::BindGroupLayout layout) const;
};

template <typename T>
concept is_material = requires(T *a) {
    {
        ((const T*)a)->create_fragment_shader(std::declval<const wgpu::Device&>())
    } -> std::same_as<ShaderInfo>;
    {
        ((const T*)a)->create_bind_group_layout(std::declval<const wgpu::Device&>())
    } -> std::same_as<wgpu::BindGroupLayout>;
    {
        ((const T*)a)
            ->create_bind_group(
                std::declval<const wgpu::Device&>(),
                std::declval<wgpu::BindGroupLayout>())
    } -> std::same_as<wgpu::BindGroup>;
};
