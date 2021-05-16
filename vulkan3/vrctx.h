#pragma once
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <vulkan/vulkan_win32.h>
#include "pch.h"
#include "vkctx.h"
#include "defaultpipeline.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include "openxr/openxr.h"
#include "openxr/openxr_platform.h"
#include "model.h"
#include "framedata.h"


#include <glm/glm.hpp>

static PFN_xrGetD3D11GraphicsRequirementsKHR ext_xrGetD3D11GraphicsRequirementsKHR = nullptr;
static PFN_xrCreateDebugUtilsMessengerEXT    ext_xrCreateDebugUtilsMessengerEXT = nullptr;
static PFN_xrDestroyDebugUtilsMessengerEXT   ext_xrDestroyDebugUtilsMessengerEXT = nullptr;
static PFN_xrLocateHandJointsEXT pfnLocateHandJointsEXT;
static PFN_xrCreateHandTrackerEXT pfnCreateHandTrackerEXT;
static VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
static XrFormFactor            app_config_form = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
static XrViewConfigurationType app_config_view = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

static XrDebugUtilsMessengerEXT xr_debug = {};


struct VrSwapChainImage {
	VkImage colorImage;
	VkImageView colorImageView;
	VkImageLayout layout;
	VkDeviceMemory memory;
	VkFramebuffer m_pFramebuffer;
};

struct Desc
{
	Desc(const VkCtx& ctx) {}
	VkImage m_pDepthStencilImage;
	VkImageLayout m_nDepthStencilImageLayout;
	VkDeviceMemory m_pDepthStencilDeviceMemory;
	VkImageView m_pDepthStencilImageView;
	XrSwapchain swapchain;

	std::vector<VrSwapChainImage> swapchainImages;
	std::vector<XrSwapchainImageD3D11KHR> surface_images;
};

XrPosef getDefaultPose();
glm::mat4 poseToMat(XrPosef pose);
XrPosef matToPose(glm::mat4 pose);

struct ActionSet {
	XrActionSet actionSet;
	XrAction headPoseAction;
	XrPath headPosePath;
	XrPath headSubactionPath;
	XrSpace headSpace;
	ActionSet() {

	}
	void init(XrInstance instance, XrSession session);
	void sync(XrSession session);
};

struct VrCtx {
	const VkCtx& ctx;
	ActionSet actionSet;
	XrInstance instance;
	XrSession session;
	XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
	XrSpace space;
	XrSpace viewSpace;
	XrSystemId id;
	DefaultPipeline pipeline;
	VkRenderPass m_pRenderPass;
	XrEnvironmentBlendMode blendMode;
	glm::mat4 prevPose;
	ID3D11Device* d3d_device = nullptr;
	ID3D11DeviceContext* d3d_context = nullptr;
	int64_t              d3d_swapchain_fmt = DXGI_FORMAT_R8G8B8A8_UNORM;
	std::vector<XrView> views;
	glm::mat4 headMat;
	std::vector<XrViewConfigurationView> configViews;
	XrHandTrackerEXT leftHandTracker;
	XrHandJointLocationEXT jointLocations[XR_HAND_JOINT_COUNT_EXT];
	uint32_t width, height;
	std::vector<Desc> descs;
	int m_nQueueFamilyIndex;


	VrCtx(const VkCtx& ctx) : ctx(ctx), pipeline(ctx) {
		m_nQueueFamilyIndex = ctx.graphicsQueueFamily();
	}

	void updateHeadMat(XrSession session, XrTime predictedTime);
	void updateView(XrTime predictedTime);
	void setView();
	void createVulkanResources();
	void initXr();
	IDXGIAdapter1* d3d_get_adapter(LUID& adapter_luid);

	bool d3d_init(LUID& adapter_luid);
	void initVrCtx(const VkCtx& ctx);
	void createFrameBuffer(const VkCtx& ctx, Desc& desc);
};



void RenderScene(SceneData& data, std::vector<Mesh>& meshes, Gamestate& gameState, VkCommandBuffer commandBuffer, int& i, VrCtx& vrCtx, int fi, glm::mat4 trans);
void RenderStereoTargets(SceneData& data, std::vector<Mesh>& meshes, Gamestate& gameState, VrCtx& vrCtx, VkCommandBuffer m_pCommandBuffer, int& i, int fi, std::vector<XrCompositionLayerProjectionView>& views, XrTime predictedTime);


