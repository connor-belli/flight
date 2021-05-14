#pragma once
#include "pch.h"
#include "vkctx.h"
#include <array>

class DefaultPipeline {
    const VkCtx& _ctx;
    VkPipeline _pipeline;
    VkPipelineLayout _layout;
    VkDescriptorSetLayout _descriptorLayout;
public:
    DefaultPipeline(const VkCtx& ctx, VkRenderPass renderPass);
    DefaultPipeline(const DefaultPipeline&) = default;
    DefaultPipeline(const VkCtx& ctx) : _ctx(ctx) {}
    void destroy();

    VkPipeline pipeline() const;
    VkPipelineLayout layout() const;
    VkDescriptorSetLayout descriptorSetLayout() const;
};
