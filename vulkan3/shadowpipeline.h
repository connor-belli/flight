#pragma once
#include "pch.h"
#include "vkctx.h"
#include <array>

class ShadowPipeline {
    const VkCtx& _ctx;
    VkPipeline _pipeline;
    VkPipelineLayout _layout;
    VkDescriptorSetLayout _descriptorLayout;
public:
    ShadowPipeline(const VkCtx& ctx, VkRenderPass renderPass);
    ShadowPipeline(const ShadowPipeline&) = default;
    void destroy();

    VkPipeline pipeline() const;
    VkPipelineLayout layout() const;
    VkDescriptorSetLayout descriptorSetLayout() const;
};
