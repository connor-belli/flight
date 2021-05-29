#pragma once
#include "pch.h"
#include "vkctx.h"
#include <array>

class DefaultLayout {
    const VkCtx& _ctx;
    VkPipelineLayout _layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout _descriptorLayout = VK_NULL_HANDLE;
public:
    DefaultLayout(const DefaultLayout&) = default;
    DefaultLayout(const VkCtx& ctx);
    void destroy();

    VkPipelineLayout layout() const;
    VkDescriptorSetLayout descriptorSetLayout() const;
};

class DefaultPipeline {
    const VkCtx& _ctx;
    VkPipeline _pipeline = VK_NULL_HANDLE;

public:
    DefaultPipeline(const VkCtx& ctx, VkRenderPass renderPass, DefaultLayout& layout, VkShaderModule vertexShader, VkShaderModule fragmentShader);
    DefaultPipeline(const DefaultPipeline&) = default;
    DefaultPipeline(const VkCtx& ctx) : _ctx(ctx) {}
    void destroy();

    VkPipeline pipeline() const;

};
