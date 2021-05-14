#pragma once

#include "pch.h"
#include "vkctx.h"

class ShadowRenderpass {
	const VkCtx& _ctx;
	VkRenderPass _renderPass = VK_NULL_HANDLE;
	void createRenderPass();
public:
	ShadowRenderpass(const VkCtx& ctx);
	ShadowRenderpass(const ShadowRenderpass&) = default;
	VkRenderPass renderPass();
	void destroy();
	void recreate();
};