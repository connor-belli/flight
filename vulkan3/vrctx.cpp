#include "vrctx.h"
#include "glm/gtc/quaternion.hpp"
#include <glm/gtx/quaternion.hpp>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "d3d10.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

static bool MemoryTypeFromProperties(const VkPhysicalDeviceMemoryProperties& memoryProperties, uint32_t nMemoryTypeBits, VkMemoryPropertyFlags nMemoryProperties, uint32_t* pTypeIndexOut)
{
	for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
	{
		if ((nMemoryTypeBits & 1) == 1)
		{
			// Type is available, does it match user properties?
			if ((memoryProperties.memoryTypes[i].propertyFlags & nMemoryProperties) == nMemoryProperties)
			{
				*pTypeIndexOut = i;
				return true;
			}
		}
		nMemoryTypeBits >>= 1;
	}

	// No memory types matched, return failure
	return false;
}

glm::mat4 getCamera(Gamestate &gameState);

glm::mat4 d3d_xr_projection(XrFovf fov, float nearZ, float farZ) {

	const float tanAngleLeft = tanf(fov.angleLeft);
	const float tanAngleRight = tanf(fov.angleRight);
	const float tanAngleDown = tanf(fov.angleDown);
	const float tanAngleUp = tanf(fov.angleUp);
	const float tanAngleWidth = tanAngleRight - tanAngleLeft;
	const float tanAngleHeight = (tanAngleDown - tanAngleUp);
	glm::mat4 resultm{};
	float* result = &resultm[0][0];
	// normal projection
	result[0] = 2 / tanAngleWidth;
	result[4] = 0;
	result[8] = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
	result[12] = 0;

	result[1] = 0;
	result[5] = 2 / tanAngleHeight;
	result[9] = (tanAngleUp + tanAngleDown) / tanAngleHeight;
	result[13] = 0;

	result[2] = 0;
	result[6] = 0;
	result[10] = -(farZ) / (farZ - nearZ);
	result[14] = -(farZ * (nearZ)) / (farZ - nearZ);

	result[3] = 0;
	result[7] = 0;
	result[11] = -1;
	result[15] = 0;
	return resultm;
}

XrPosef getDefaultPose()
{
	glm::quat rot(glm::vec3{ 0, 3.141592 / 2, 0.0 });
	const XrPosef  xr_pose_identity = { {rot.x, rot.y, rot.z, rot.w}, { 0, -1.0, 5.2} };
	return xr_pose_identity;
}

glm::mat4 poseToMat(XrPosef pose)
{
	auto q = pose.orientation;
	glm::quat quat(q.w, q.x, q.y, q.z);
	auto p = pose.position;

	glm::vec3 pos(p.x, p.y, p.z);

	return glm::translate(glm::mat4(1.0f), pos) * glm::toMat4(quat);
}

XrPosef matToPose(glm::mat4 pose)
{
	auto q = glm::quat_cast(pose);

	auto p = glm::vec3(pose[3]);

	return { { q.x, q.y, q.z, q.w }, {p.x, p.y, p.z} };
}

inline void ActionSet::init(XrInstance instance, XrSession session) {
	XrResult res;
	XrActionSetCreateInfo actionset_info = { XR_TYPE_ACTION_SET_CREATE_INFO };
	strcpy_s(actionset_info.actionSetName, "gameplay");
	strcpy_s(actionset_info.localizedActionSetName, "Gameplay");
	res = xrCreateActionSet(instance, &actionset_info, &actionSet);
	xrStringToPath(instance, "/user/head", &headSubactionPath);

	XrActionCreateInfo action_info = { XR_TYPE_ACTION_CREATE_INFO };
	action_info.countSubactionPaths = 1;
	action_info.subactionPaths = &headSubactionPath;
	action_info.actionType = XR_ACTION_TYPE_POSE_INPUT;
	strcpy_s(action_info.actionName, "head_pose");
	strcpy_s(action_info.localizedActionName, "Head Pose");
	res = xrCreateAction(actionSet, &action_info, &headPoseAction);


	XrActionSpaceCreateInfo action_space_info = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
	action_space_info.action = headPoseAction;
	action_space_info.poseInActionSpace = getDefaultPose();
	action_space_info.subactionPath = headSubactionPath;
	res = xrCreateActionSpace(session, &action_space_info, &headSpace);
	XrSessionActionSetsAttachInfo attach_info = { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
	attach_info.countActionSets = 1;
	attach_info.actionSets = &actionSet;

	xrAttachSessionActionSets(session, &attach_info);

	//XrPath profile_path;
	//xrStringToPath(instance, "/user/head/input/aim/pose", &headPosePath);
	//XrActionSuggestedBinding bindings[] = {
	//  { headPoseAction, headSubactionPath } };
	//XrInteractionProfileSuggestedBinding suggested_binds = { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
	//suggested_binds.interactionProfile = s.interactionProfile;
	//ggested_binds.suggestedBindings = &bindings[0];
	//suggested_binds.countSuggestedBindings = _countof(bindings);
	//res = xrSuggestInteractionProfileBindings(instance, &suggested_binds);


}

void ActionSet::sync(XrSession session) {
	XrActiveActionSet action_set = {};
	action_set.actionSet = actionSet;
	action_set.subactionPath = XR_NULL_PATH;

	XrActionsSyncInfo sync_info = { XR_TYPE_ACTIONS_SYNC_INFO };
	sync_info.countActiveActionSets = 1;
	sync_info.activeActionSets = &action_set;

	auto res = xrSyncActions(session, &sync_info);
}

void VrCtx::updateHeadMat(XrSession session, XrTime predictedTime)
{
	XrSpaceLocation spaceRelation = { XR_TYPE_SPACE_LOCATION };
	XrResult        res = xrLocateSpace(viewSpace, space, predictedTime, &spaceRelation);

	if (XR_UNQUALIFIED_SUCCESS(res) &&
		(spaceRelation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
		(spaceRelation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
		headMat = poseToMat(spaceRelation.pose);
	}

	XrHandJointsLocateInfoEXT locateInfo{ XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT };
	locateInfo.baseSpace = space;
	locateInfo.time = predictedTime;

	XrHandJointLocationsEXT locations{ XR_TYPE_HAND_JOINT_LOCATIONS_EXT };
	locations.jointCount = XR_HAND_JOINT_COUNT_EXT;
	locations.jointLocations = jointLocations;

	res = pfnLocateHandJointsEXT(leftHandTracker, &locateInfo, &locations);

	if (!locations.isActive) {
		jointLocations[XR_HAND_JOINT_INDEX_TIP_EXT].pose = getDefaultPose();
		jointLocations[XR_HAND_JOINT_THUMB_TIP_EXT].pose = getDefaultPose();
	}
}


void VrCtx::updateView(XrTime predictedTime)
{

	XrReferenceSpaceCreateInfo ref_space = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
	glm::mat4 rot = headMat;
	prevPose = prevPose * rot;
	xrDestroySpace(space);
	ref_space.poseInReferenceSpace = matToPose(prevPose);
	ref_space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	xrCreateReferenceSpace(session, &ref_space, &space);
}

void VrCtx::setView()
{
	XrReferenceSpaceCreateInfo ref_space = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
	ref_space.poseInReferenceSpace = getDefaultPose();
	ref_space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	xrCreateReferenceSpace(session, &ref_space, &space);

	XrReferenceSpaceCreateInfo view_space = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
	view_space.poseInReferenceSpace = getDefaultPose();
	view_space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	xrCreateReferenceSpace(session, &view_space, &viewSpace);
}

inline void VrCtx::createVulkanResources() {
	uint32_t nTotalAttachments = 2;
	VkAttachmentDescription attachmentDescs[2];
	VkAttachmentReference attachmentReferences[2];
	attachmentReferences[0].attachment = 0;
	attachmentReferences[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachmentReferences[1].attachment = 1;
	attachmentReferences[1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	attachmentDescs[0].format = VK_FORMAT_R8G8B8A8_UNORM;
	attachmentDescs[0].samples = sampleCount;
	attachmentDescs[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescs[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescs[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescs[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescs[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachmentDescs[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachmentDescs[0].flags = 0;

	attachmentDescs[1].format = VK_FORMAT_D32_SFLOAT;
	attachmentDescs[1].samples = sampleCount;
	attachmentDescs[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescs[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescs[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescs[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescs[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachmentDescs[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachmentDescs[1].flags = 0;

	VkSubpassDescription subPassCreateInfo = {};
	subPassCreateInfo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subPassCreateInfo.flags = 0;
	subPassCreateInfo.inputAttachmentCount = 0;
	subPassCreateInfo.pInputAttachments = NULL;
	subPassCreateInfo.colorAttachmentCount = 1;
	subPassCreateInfo.pColorAttachments = &attachmentReferences[0];
	subPassCreateInfo.pResolveAttachments = NULL;
	subPassCreateInfo.pDepthStencilAttachment = &attachmentReferences[1];
	subPassCreateInfo.preserveAttachmentCount = 0;
	subPassCreateInfo.pPreserveAttachments = NULL;

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.flags = 0;
	renderPassCreateInfo.attachmentCount = 2;
	renderPassCreateInfo.pAttachments = &attachmentDescs[0];
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subPassCreateInfo;
	renderPassCreateInfo.dependencyCount = 0;
	renderPassCreateInfo.pDependencies = NULL;

	VkResult nResult = vkCreateRenderPass(ctx.device(), &renderPassCreateInfo, NULL, &m_pRenderPass);
	new (&pipeline) DefaultPipeline(ctx, m_pRenderPass);
}

inline void VrCtx::initXr() {
	std::vector<const char*> use_extensions = {
		XR_KHR_D3D11_ENABLE_EXTENSION_NAME,
		XR_EXT_DEBUG_UTILS_EXTENSION_NAME,
		XR_EXT_HAND_TRACKING_EXTENSION_NAME,
	};
	prevPose = glm::mat4(1.0f);
	XrInstanceCreateInfo createInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
	createInfo.enabledExtensionCount = use_extensions.size();
	createInfo.enabledExtensionNames = use_extensions.data();
	createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
	strcpy_s(createInfo.applicationInfo.applicationName, "engine");
	xrCreateInstance(&createInfo, &instance);
	if (instance == NULL) {
		throw std::runtime_error("Failed to make xr instance");
	}

	auto r = xrGetInstanceProcAddr(instance, "xrCreateDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)(&ext_xrCreateDebugUtilsMessengerEXT));
	r = xrGetInstanceProcAddr(instance, "xrDestroyDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)(&ext_xrDestroyDebugUtilsMessengerEXT));
	r = xrGetInstanceProcAddr(instance, "xrGetD3D11GraphicsRequirementsKHR", (PFN_xrVoidFunction*)(&ext_xrGetD3D11GraphicsRequirementsKHR));

	XrDebugUtilsMessengerCreateInfoEXT debug_info = { XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	debug_info.messageTypes =
		XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;
	debug_info.messageSeverities =
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debug_info.userCallback = [](XrDebugUtilsMessageSeverityFlagsEXT severity, XrDebugUtilsMessageTypeFlagsEXT types, const XrDebugUtilsMessengerCallbackDataEXT* msg, void* user_data) {
		printf("%s: %s\n", msg->functionName, msg->message);

		// Output to debug window
		char text[512];
		sprintf_s(text, "%s: %s", msg->functionName, msg->message);
		OutputDebugStringA(text);

		// Returning XR_TRUE here will force the calling function to fail
		return (XrBool32)XR_FALSE;
	};

	if (ext_xrCreateDebugUtilsMessengerEXT)
		ext_xrCreateDebugUtilsMessengerEXT(instance, &debug_info, &xr_debug);

	XrSystemGetInfo systemInfo = { XR_TYPE_SYSTEM_GET_INFO };
	systemInfo.formFactor = app_config_form;
	xrGetSystem(instance, &systemInfo, &id);

	uint32_t blend_count = 0;
	xrEnumerateEnvironmentBlendModes(instance, id, app_config_view, 1, &blend_count, &blendMode);
	XrGraphicsRequirementsD3D11KHR requirement = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
	ext_xrGetD3D11GraphicsRequirementsKHR(instance, id, &requirement);
	if (!d3d_init(requirement.adapterLuid))
		throw std::runtime_error("Could not init d3d");
	createVulkanResources();
	XrGraphicsBindingD3D11KHR binding = { XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
	binding.device = d3d_device;
	XrSessionCreateInfo sessionInfo = { XR_TYPE_SESSION_CREATE_INFO };
	sessionInfo.next = &binding;
	sessionInfo.systemId = id;
	xrCreateSession(instance, &sessionInfo, &session);
	if (!session)
		throw std::runtime_error("Could not make xr session");
	actionSet.init(instance, session);
	XrSystemHandTrackingPropertiesEXT handProps = { XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT };

	XrSystemProperties props = { XR_TYPE_SYSTEM_PROPERTIES };
	props.next = &handProps;
	xrGetSystemProperties(instance, id, &props);
	XrHandTrackerCreateInfoEXT handInfo = { XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT };
	xrGetInstanceProcAddr(instance, "xrCreateHandTrackerEXT",
		reinterpret_cast<PFN_xrVoidFunction*>(
			&pfnCreateHandTrackerEXT));
	xrGetInstanceProcAddr(instance, "xrLocateHandJointsEXT",
		reinterpret_cast<PFN_xrVoidFunction*>(
			&pfnLocateHandJointsEXT));
	handInfo.hand = XR_HAND_LEFT_EXT;
	handInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
	r = pfnCreateHandTrackerEXT(session, &handInfo, &leftHandTracker);

	setView();
	uint32_t view_count = 0;
	xrEnumerateViewConfigurationViews(instance, id, app_config_view, 0, &view_count, nullptr);
	configViews.resize(view_count, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
	views.resize(view_count, { XR_TYPE_VIEW });
	xrEnumerateViewConfigurationViews(instance, id, app_config_view, view_count, &view_count, configViews.data());
	for (uint32_t i = 0; i < view_count; i++) {
		XrViewConfigurationView& view = configViews[i];
		XrSwapchainCreateInfo    swapchain_info = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
		XrSwapchain              handle;
		swapchain_info.arraySize = 1;
		swapchain_info.mipCount = 1;
		swapchain_info.faceCount = 1;
		swapchain_info.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		swapchain_info.width = view.recommendedImageRectWidth;
		swapchain_info.height = view.recommendedImageRectHeight;
		swapchain_info.sampleCount = view.recommendedSwapchainSampleCount;
		swapchain_info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		xrCreateSwapchain(session, &swapchain_info, &handle);

		width = view.recommendedImageRectWidth;
		height = view.recommendedImageRectHeight;

		// Find out how many textures were generated for the swapchain
		uint32_t surface_count = 0;
		xrEnumerateSwapchainImages(handle, 0, &surface_count, nullptr);

		Desc desc(ctx);
		desc.swapchain = handle;
		desc.surface_images.resize(surface_count, { XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR });
		xrEnumerateSwapchainImages(desc.swapchain, surface_count, &surface_count, (XrSwapchainImageBaseHeader*)desc.surface_images.data());
		createFrameBuffer(ctx, desc);
		descs.push_back(desc);
	}
}

inline IDXGIAdapter1* VrCtx::d3d_get_adapter(LUID& adapter_luid) {
	// Turn the LUID into a specific graphics device adapter
	IDXGIAdapter1* final_adapter = nullptr;
	IDXGIAdapter1* curr_adapter = nullptr;
	IDXGIFactory1* dxgi_factory;
	DXGI_ADAPTER_DESC1 adapter_desc;

	CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)(&dxgi_factory));

	int curr = 0;
	while (dxgi_factory->EnumAdapters1(curr++, &curr_adapter) == S_OK) {
		curr_adapter->GetDesc1(&adapter_desc);

		if (memcmp(&adapter_desc.AdapterLuid, &adapter_luid, sizeof(&adapter_luid)) == 0) {
			final_adapter = curr_adapter;
			break;
		}
		curr_adapter->Release();
		curr_adapter = nullptr;
	}
	dxgi_factory->Release();
	return final_adapter;
}

inline bool VrCtx::d3d_init(LUID& adapter_luid) {
	IDXGIAdapter1* adapter = d3d_get_adapter(adapter_luid);
	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };

	if (adapter == nullptr)
		return false;
	if (FAILED(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, 0, 0, featureLevels, _countof(featureLevels), D3D11_SDK_VERSION, &d3d_device, nullptr, &d3d_context)))
		return false;

	adapter->Release();
	return true;
}

void VrCtx::initVrCtx(const VkCtx& ctx) {
	initXr();
}

inline void VrCtx::createFrameBuffer(const VkCtx& ctx, Desc& desc) {
	VkResult nResult;
	VkDedicatedAllocationImageCreateInfoNV dedicatedImageCreateInfo = {
		VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV, // sType
		0, // pNext
		VK_FALSE // dedicatedAllocation
	};
	VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.extent.width = width;
	imageCreateInfo.extent.height = height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.samples = (VkSampleCountFlagBits)1;
	imageCreateInfo.usage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	imageCreateInfo.flags = 0;
	imageCreateInfo.pNext = &dedicatedImageCreateInfo;

	VkImageViewCreateInfo imageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	imageViewCreateInfo.flags = 0;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = imageCreateInfo.format;
	imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;
	VkMemoryAllocateInfo memoryAllocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	VkMemoryRequirements memoryRequirements = {};
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	VkPhysicalDeviceMemoryProperties m_physicalDeviceMemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(ctx.physicalDevice(), &m_physicalDeviceMemoryProperties);
	desc.swapchainImages.resize(desc.surface_images.size());
	for (int i = 0; i < desc.surface_images.size(); i++) {
		nResult = vkCreateImage(ctx.device(), &imageCreateInfo, nullptr, &desc.swapchainImages[i].colorImage);

		vkGetImageMemoryRequirements(ctx.device(), desc.swapchainImages[i].colorImage, &memoryRequirements);

		if (!MemoryTypeFromProperties(m_physicalDeviceMemoryProperties, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memoryAllocateInfo.memoryTypeIndex));

		HANDLE handle;

		IDXGIResource1* tempResource = NULL;
		desc.surface_images[i].texture->QueryInterface(__uuidof(IDXGIResource1), (void**)&tempResource);
		tempResource->CreateSharedHandle(NULL,
			DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
			NULL,
			&handle);
		tempResource->Release();

		VkImportMemoryWin32HandleInfoNV importMemInfoNV = {
			VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV, // sType
			0, // pNext
			VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_IMAGE_BIT_NV, // handleTypes
			handle
		};
		VkMemoryAllocateInfo memInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, // sType
			&importMemInfoNV, // pNext
			memoryRequirements.size, // allocationSize
			memoryAllocateInfo.memoryTypeIndex
		};
		VkDedicatedAllocationMemoryAllocateInfoNV dedicatedAllocationInfo = {
			VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV // sType
		};

		nResult = vkAllocateMemory(ctx.device(), &memInfo, nullptr, &desc.swapchainImages[i].memory);
		nResult = vkBindImageMemory(ctx.device(), desc.swapchainImages[i].colorImage, desc.swapchainImages[i].memory, 0);
		imageViewCreateInfo.image = desc.swapchainImages[i].colorImage;
		nResult = vkCreateImageView(ctx.device(), &imageViewCreateInfo, nullptr, &desc.swapchainImages[i].colorImageView);
		desc.swapchainImages[i].layout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = VK_FORMAT_D32_SFLOAT;
	imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	nResult = vkCreateImage(ctx.device(), &imageCreateInfo, nullptr, &desc.m_pDepthStencilImage);
	if (nResult != VK_SUCCESS)
		vkGetImageMemoryRequirements(ctx.device(), desc.m_pDepthStencilImage, &memoryRequirements);

	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	if (!MemoryTypeFromProperties(m_physicalDeviceMemoryProperties, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memoryAllocateInfo.memoryTypeIndex));

	nResult = vkAllocateMemory(ctx.device(), &memoryAllocateInfo, nullptr, &desc.m_pDepthStencilDeviceMemory);

	nResult = vkBindImageMemory(ctx.device(), desc.m_pDepthStencilImage, desc.m_pDepthStencilDeviceMemory, 0);

	imageViewCreateInfo.image = desc.m_pDepthStencilImage;
	imageViewCreateInfo.format = imageCreateInfo.format;
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	nResult = vkCreateImageView(ctx.device(), &imageViewCreateInfo, nullptr, &desc.m_pDepthStencilImageView);

	// Create a renderpass


	for (int i = 0; i < desc.swapchainImages.size(); i++) {
		VkImageView attachments[2] = { desc.swapchainImages[i].colorImageView, desc.m_pDepthStencilImageView };
		VkFramebufferCreateInfo framebufferCreateInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		framebufferCreateInfo.renderPass = m_pRenderPass;
		framebufferCreateInfo.attachmentCount = 2;
		framebufferCreateInfo.pAttachments = &attachments[0];
		framebufferCreateInfo.width = width;
		framebufferCreateInfo.height = height;
		framebufferCreateInfo.layers = 1;
		nResult = vkCreateFramebuffer(ctx.device(), &framebufferCreateInfo, NULL, &desc.swapchainImages[i].m_pFramebuffer);
	}
	desc.m_nDepthStencilImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void RenderScene(SceneData& data, std::vector<Mesh>& meshes, Gamestate& gameState, VkCommandBuffer commandBuffer, int& i, VrCtx& vrCtx, int fi, glm::mat4 trans)
 {
	 {
		 vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vrCtx.pipeline.pipeline());
		 for (const Mesh& mesh : meshes) {
			 VkDeviceSize offset = 0;
			 VkBuffer vertexBuffer = mesh.buffer.buffer();
			 vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offset);
			 VkBuffer indexBuffer = mesh.indices.buffer();
			 vkCmdBindIndexBuffer(commandBuffer, indexBuffer, offset, VK_INDEX_TYPE_UINT32);

			 for (const MeshInstanceState& state : mesh.instances) {
				 glm::mat4 model = state.pose;
				 glm::mat4 mvp = trans * gameState.planeState * state.pose;
				 data.uniformBuffers[fi].copyInd(i, { mvp, getCamera(gameState), model });
				 uint32_t offset = 256 * i;
				 i++;
				 vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vrCtx.pipeline.layout(), 0, 1, &data.descriptorSets[fi], 1, &offset);
				 vkCmdDrawIndexed(commandBuffer, mesh.indices.size(), 1, 0, 0, 0);
			 }
		 }
	 }
}


void RenderStereoTargets(SceneData& data, std::vector<Mesh>& meshes, Gamestate& gameState, VrCtx& vrCtx, VkCommandBuffer m_pCommandBuffer, int& i, int fi, std::vector<XrCompositionLayerProjectionView>& views, XrTime predictedTime)
{
	// Set viewport and scissor
	VkViewport viewport = { 0.0f, 0.0f, (float)vrCtx.width, (float)vrCtx.height, 0.0f, 1.0f };
	vkCmdSetViewport(m_pCommandBuffer, 0, 1, &viewport);
	VkRect2D scissor = { 0, 0, vrCtx.width, vrCtx.height };
	vkCmdSetScissor(m_pCommandBuffer, 0, 1, &scissor);
	uint32_t         view_count = 0;
	XrViewState      view_state = { XR_TYPE_VIEW_STATE };
	XrViewLocateInfo locate_info = { XR_TYPE_VIEW_LOCATE_INFO };
	locate_info.viewConfigurationType = app_config_view;
	locate_info.displayTime = predictedTime;
	locate_info.space = vrCtx.space;
	xrLocateViews(vrCtx.session, &locate_info, &view_state, (uint32_t)vrCtx.views.size(), &view_count, vrCtx.views.data());
	views.resize(view_count);

	for (int j = 0; j < vrCtx.views.size(); j++) {
		Desc& desc = vrCtx.descs[j];
		uint32_t vir;
		XrSwapchainImageAcquireInfo acquire_info = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
		auto e = xrAcquireSwapchainImage(desc.swapchain, &acquire_info, &vir);
		auto& sw = desc.swapchainImages[vir];
		VkImageMemoryBarrier imageMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		imageMemoryBarrier.oldLayout = sw.layout;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		imageMemoryBarrier.image = sw.colorImage;
		imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
		imageMemoryBarrier.subresourceRange.levelCount = 1;
		imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
		imageMemoryBarrier.subresourceRange.layerCount = 1;
		imageMemoryBarrier.srcQueueFamilyIndex = vrCtx.m_nQueueFamilyIndex;
		imageMemoryBarrier.dstQueueFamilyIndex = vrCtx.m_nQueueFamilyIndex;
		vkCmdPipelineBarrier(m_pCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
		sw.layout = imageMemoryBarrier.newLayout;

		if (desc.m_nDepthStencilImageLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		{
			imageMemoryBarrier.image = desc.m_pDepthStencilImage;
			imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			imageMemoryBarrier.srcAccessMask = 0;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			imageMemoryBarrier.oldLayout = desc.m_nDepthStencilImageLayout;
			imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			vkCmdPipelineBarrier(m_pCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
			desc.m_nDepthStencilImageLayout = imageMemoryBarrier.newLayout;
		}

		views[j] = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
		views[j].pose = vrCtx.views[j].pose;
		views[j].fov = vrCtx.views[j].fov;
		views[j].subImage.swapchain = desc.swapchain;
		views[j].subImage.imageRect.offset = { 0, 0 };
		views[j].subImage.imageRect.extent = { (int)vrCtx.width, (int)vrCtx.height };

		glm::mat4 trans = d3d_xr_projection(views[j].fov, 0.1, 20000) * glm::inverse(poseToMat(views[j].pose));

		// Start the renderpass
		VkRenderPassBeginInfo renderPassBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		renderPassBeginInfo.renderPass = vrCtx.m_pRenderPass;
		renderPassBeginInfo.framebuffer = sw.m_pFramebuffer;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = vrCtx.width;
		renderPassBeginInfo.renderArea.extent.height = vrCtx.height;
		renderPassBeginInfo.clearValueCount = 2;
		VkClearValue clearValues[2];
		clearValues[0].color.float32[0] = 0.05f;
		clearValues[0].color.float32[1] = 0.05f;
		clearValues[0].color.float32[2] = 0.1f;
		clearValues[0].color.float32[3] = 1.0f;
		clearValues[1].depthStencil.depth = 1.0f;
		clearValues[1].depthStencil.stencil = 0;
		renderPassBeginInfo.pClearValues = &clearValues[0];
		vkCmdBeginRenderPass(m_pCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		RenderScene(data, meshes, gameState, m_pCommandBuffer, i, vrCtx, fi, trans);

		vkCmdEndRenderPass(m_pCommandBuffer);

		imageMemoryBarrier.image = sw.colorImage;
		imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		imageMemoryBarrier.oldLayout = sw.layout;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		vkCmdPipelineBarrier(m_pCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
		sw.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}
}