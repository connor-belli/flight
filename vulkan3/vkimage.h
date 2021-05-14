#pragma once
#include "pch.h"
#include "vkctx.h"
class Image {
	const VkCtx& _ctx;
	int _width, _height;
	VkImage _image;
	VmaAllocation _alloc;
	VkFormat _format;
public:
	Image(const VkCtx& ctx, int width, int height, VkFormat format, VkImageUsageFlags usage);
	Image(const Image&) = default;
	VkImageView makeImageView(VkImageAspectFlags flags);
	VkImage image();
	void destroy();
};