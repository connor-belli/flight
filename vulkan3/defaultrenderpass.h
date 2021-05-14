#pragma once

#include "pch.h"
#include "vkctx.h"

class Renderpass {
	const VkCtx& _ctx;
	VkRenderPass _renderPass = VK_NULL_HANDLE;
	void createRenderPass();
public:
	Renderpass(const VkCtx& ctx);
	Renderpass(const Renderpass&) = default;
	VkRenderPass renderPass();
	void destroy();
	void recreate();
};