#include "vkctx.h"
#include <fstream>
#include <iostream>
constexpr bool vre = true;

#ifndef NDEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
    (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
    fprintf(stderr, "[vulkan] Debug report from ObjectType: Message: %s\n\n",  pMessage);
    return VK_FALSE;
}
#endif // IMGUI_VULKAN_DEBUG_REPORT


VkCtx::VkCtx(const char** extensions, uint32_t extensions_count)
{
    VkResult err;

    {
        VkApplicationInfo app_info = {};
        app_info.apiVersion = VK_API_VERSION_1_0;
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.enabledExtensionCount = extensions_count;
        create_info.ppEnabledExtensionNames = extensions;
        create_info.pApplicationInfo = &app_info;
#ifndef NDEBUG 
        // Enabling validation layers
        const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = layers;

        const char** extensions_ext = (const char**)malloc(sizeof(const char*) * ((size_t)extensions_count + 1));
        if (extensions_ext != nullptr) {
            memcpy(extensions_ext, extensions, extensions_count * sizeof(const char*));
            extensions_ext[extensions_count] = "VK_EXT_debug_report";
        }
        create_info.enabledExtensionCount = extensions_count + 1;
        create_info.ppEnabledExtensionNames = extensions_ext;

        // Create Vulkan Instance
        err = vkCreateInstance(&create_info, allocCallback, &_instance);
        check_vk_result(err);
        free(extensions_ext);

        // Get the function pointer (required for any extensions)
        auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(_instance, "vkCreateDebugReportCallbackEXT");
        assert(vkCreateDebugReportCallbackEXT != nullptr);

        // Setup the debug report callback
        VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
        debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
        debug_report_ci.pfnCallback = debug_report;
        debug_report_ci.pUserData = NULL;
        err = vkCreateDebugReportCallbackEXT(_instance, &debug_report_ci, allocCallback, &_debugCallback);
        check_vk_result(err);
#else
        // Create Vulkan Instance without any debug feature
        err = vkCreateInstance(&create_info, allocCallback, &_instance);
        check_vk_result(err);
        //IM_UNUSED(g_DebugReport);
#endif
    }
    // Select GPU
    {
        uint32_t gpu_count;
        err = vkEnumeratePhysicalDevices(_instance, &gpu_count, NULL);
        check_vk_result(err);
        assert(gpu_count > 0);

        std::vector<VkPhysicalDevice> physicalDevices(gpu_count);
        err = vkEnumeratePhysicalDevices(_instance, &gpu_count, physicalDevices.data());
        check_vk_result(err);

        for (auto phy : physicalDevices) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(phy, &props);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                _physicalDevice = phy;
            }
        }
        if (_physicalDevice == VK_NULL_HANDLE) {
            _physicalDevice = physicalDevices[0];
        }
    }

    // Select graphics queue family
    {
        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &count, NULL);
        std::vector<VkQueueFamilyProperties> queues(count);
        vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &count, queues.data());
        for (uint32_t i = 0; i < count; i++)
            if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                _graphicsQueueFamily = i;
                break;
            }
        assert(_graphicsQueueFamily != (uint32_t)-1);
    }
    {
        int device_extension_count = 1;
        const char* device_extensions[] = { "VK_KHR_swapchain" };
        const float queue_priority[] = { 1.0f };
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = _graphicsQueueFamily;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;
        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
        create_info.pQueueCreateInfos = queue_info;
        create_info.enabledExtensionCount = device_extension_count;
        create_info.ppEnabledExtensionNames = device_extensions;
        err = vkCreateDevice(_physicalDevice, &create_info, allocCallback, &_device);
        check_vk_result(err);
        vkGetDeviceQueue(_device, _graphicsQueueFamily, 0, &_graphicsQueue);
    }
    {
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;
        allocatorInfo.physicalDevice = _physicalDevice;
        allocatorInfo.device = _device;
        allocatorInfo.instance = _instance;

        err = vmaCreateAllocator(&allocatorInfo, &_allocator);
        check_vk_result(err);
    }
}

VkInstance VkCtx::instance() const
{
    return _instance;
}

VkDevice VkCtx::device() const
{
    return _device;
}

VkPhysicalDevice VkCtx::physicalDevice() const
{
    return _physicalDevice;
}

VkQueue VkCtx::graphicsQueue() const
{
    return _graphicsQueue;
}

uint32_t VkCtx::graphicsQueueFamily() const
{
    return _graphicsQueueFamily;
}

VmaAllocator VkCtx::allocator() const
{
    return _allocator;
}

VkCtx::~VkCtx()
{
#ifndef NDEBUG
    auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(_instance, "vkDestroyDebugReportCallbackEXT");
    vkDestroyDebugReportCallbackEXT(_instance, _debugCallback, allocCallback);
#endif // !NDEBUG
    vmaDestroyAllocator(_allocator);
    vkDestroyDevice(_device, allocCallback);
    vkDestroyInstance(_instance, allocCallback);
}

void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}
std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}
VkShaderModule createShaderModule(const VkCtx& ctx, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(ctx.device(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        exit(1);
    }

    return shaderModule;
}
