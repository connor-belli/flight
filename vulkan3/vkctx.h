#pragma once
#include "pch.h"
#include <string>
class VkCtx {
private:
	VkInstance _instance;
	VkDevice _device;
	VkPhysicalDevice _physicalDevice;
	VkDebugReportCallbackEXT _debugCallback;
	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;
	VmaAllocator _allocator;
public:
	VkCtx(const char** extensions, uint32_t extensions_count);
	VkInstance instance() const;
	VkDevice device() const;
	VkPhysicalDevice physicalDevice() const;
	VkQueue graphicsQueue() const;
	uint32_t graphicsQueueFamily() const;
	VmaAllocator allocator() const;
	~VkCtx();
};

void check_vk_result(VkResult err);
static VkAllocationCallbacks* allocCallback = nullptr;
VkShaderModule createShaderModule(const VkCtx& ctx, const std::vector<char>& code);
std::vector<char> readFile(const std::string& filename);