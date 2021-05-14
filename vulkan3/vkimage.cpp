#include "vkimage.h"

Image::Image(const VkCtx& ctx, int width, int height, VkFormat format, VkImageUsageFlags usage) :_ctx(ctx), _width(width), _height(height), _format(format)
{
    VkImageCreateInfo depthCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    depthCreateInfo.extent = { (uint32_t)width, (uint32_t)height, 1 };
    depthCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    depthCreateInfo.arrayLayers = 1;
    depthCreateInfo.queueFamilyIndexCount = 1;
    uint32_t queue = ctx.graphicsQueueFamily();
    depthCreateInfo.pQueueFamilyIndices = &queue;
    depthCreateInfo.format = format,
    depthCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthCreateInfo.mipLevels = 1;
    depthCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depthCreateInfo.usage = usage;
    depthCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    check_vk_result(vmaCreateImage(ctx.allocator(), &depthCreateInfo, &allocCreateInfo, &_image, &_alloc, nullptr));
}

VkImageView Image::makeImageView(VkImageAspectFlags flags)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = _image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = _format;
    viewInfo.subresourceRange.aspectMask = flags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(_ctx.device(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
}

VkImage Image::image()
{
    return _image;
}

void Image::destroy()
{
	vmaDestroyImage(_ctx.allocator(), _image, _alloc);
}
