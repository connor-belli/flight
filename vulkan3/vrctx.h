#pragma once

#include "pch.h"
#include "openvr.h"
#include "vkctx.h"
#include "defaultpipeline.h"

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

struct Desc
{
	Desc(const VkCtx& ctx) : pipeline(ctx) {

	}
	VkImage m_pImage;
	VkImageLayout m_nImageLayout;
	VkDeviceMemory m_pDeviceMemory;
	VkImageView m_pImageView;
	VkImage m_pDepthStencilImage;
	VkImageLayout m_nDepthStencilImageLayout;
	VkDeviceMemory m_pDepthStencilDeviceMemory;
	VkImageView m_pDepthStencilImageView;
	VkRenderPass m_pRenderPass;
	DefaultPipeline pipeline;
	VkFramebuffer m_pFramebuffer;
};

struct VrCtx {
	vr::IVRSystem *sys;
	uint32_t width, height;
	Desc m_leftEyeDesc;
	Desc m_rightEyeDesc;
	glm::mat4 eyeRight;
	glm::mat4 eyeLeft;
	int m_nQueueFamilyIndex;
	VrCtx(const VkCtx& ctx) : m_leftEyeDesc(ctx), m_rightEyeDesc(ctx) {
		m_nQueueFamilyIndex = ctx.graphicsQueueFamily();
	}

	void initVrCtx(const VkCtx& ctx) {
		vr::EVRInitError eError = vr::VRInitError_None;
		sys = vr::VR_Init(&eError, vr::VRApplication_Scene);
		sys->GetRecommendedRenderTargetSize(&width, &height);
		createFrameBuffer(ctx, m_leftEyeDesc);
		createFrameBuffer(ctx, m_rightEyeDesc);
	}

	void createFrameBuffer(const VkCtx& ctx, Desc& desc) {
		//---------------------------//
//    Create color target    //
//---------------------------//
		VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.extent.width = width;
		imageCreateInfo.extent.height = height;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.samples = (VkSampleCountFlagBits)1;
		imageCreateInfo.usage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		imageCreateInfo.flags = 0;
		VkResult nResult;
		nResult = vkCreateImage(ctx.device(), &imageCreateInfo, nullptr, &desc.m_pImage);
		VkMemoryRequirements memoryRequirements = {};
		vkGetImageMemoryRequirements(ctx.device(), desc.m_pImage, &memoryRequirements);

		VkMemoryAllocateInfo memoryAllocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		memoryAllocateInfo.allocationSize = memoryRequirements.size;
		VkPhysicalDeviceMemoryProperties m_physicalDeviceMemoryProperties;
		vkGetPhysicalDeviceMemoryProperties(ctx.physicalDevice(), &m_physicalDeviceMemoryProperties);
		if (!MemoryTypeFromProperties(m_physicalDeviceMemoryProperties, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memoryAllocateInfo.memoryTypeIndex));

		nResult = vkAllocateMemory(ctx.device(), &memoryAllocateInfo, nullptr, &desc.m_pDeviceMemory);

		nResult = vkBindImageMemory(ctx.device(), desc.m_pImage, desc.m_pDeviceMemory, 0);

		VkImageViewCreateInfo imageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.image = desc.m_pImage;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = imageCreateInfo.format;
		imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		nResult = vkCreateImageView(ctx.device(), &imageViewCreateInfo, nullptr, &desc.m_pImageView);

		//-----------------------------------//
		//    Create depth/stencil target    //
		//-----------------------------------//
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
		uint32_t nTotalAttachments = 2;
		VkAttachmentDescription attachmentDescs[2];
		VkAttachmentReference attachmentReferences[2];
		attachmentReferences[0].attachment = 0;
		attachmentReferences[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachmentReferences[1].attachment = 1;
		attachmentReferences[1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		attachmentDescs[0].format = VK_FORMAT_R8G8B8A8_SRGB;
		attachmentDescs[0].samples = imageCreateInfo.samples;
		attachmentDescs[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescs[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescs[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescs[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDescs[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachmentDescs[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachmentDescs[0].flags = 0;

		attachmentDescs[1].format = VK_FORMAT_D32_SFLOAT;
		attachmentDescs[1].samples = imageCreateInfo.samples;
		attachmentDescs[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescs[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescs[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescs[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDescs[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachmentDescs[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachmentDescs[1].flags = 0;

		VkSubpassDescription subPassCreateInfo = { };
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

		VkRenderPassCreateInfo renderPassCreateInfo = { };
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.flags = 0;
		renderPassCreateInfo.attachmentCount = 2;
		renderPassCreateInfo.pAttachments = &attachmentDescs[0];
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subPassCreateInfo;
		renderPassCreateInfo.dependencyCount = 0;
		renderPassCreateInfo.pDependencies = NULL;

		nResult = vkCreateRenderPass(ctx.device(), &renderPassCreateInfo, NULL, &desc.m_pRenderPass);

		// Create the framebuffer
		VkImageView attachments[2] = { desc.m_pImageView, desc.m_pDepthStencilImageView };
		VkFramebufferCreateInfo framebufferCreateInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		framebufferCreateInfo.renderPass = desc.m_pRenderPass;
		framebufferCreateInfo.attachmentCount = 2;
		framebufferCreateInfo.pAttachments = &attachments[0];
		framebufferCreateInfo.width = width;
		framebufferCreateInfo.height = height;
		framebufferCreateInfo.layers = 1;
		nResult = vkCreateFramebuffer(ctx.device(), &framebufferCreateInfo, NULL, &desc.m_pFramebuffer);

		desc.m_nImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		desc.m_nDepthStencilImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		new (&desc.pipeline) DefaultPipeline(ctx, desc.m_pRenderPass);
	}
};


glm::mat4 getCamera(Gamestate& state);

void RenderScene(SceneData& data, std::vector<Mesh>& meshes, Gamestate& gameState, VkCommandBuffer commandBuffer, int& i, VrCtx& vrCtx, int fi, vr::Hmd_Eye eye) {
	{
		DefaultPipeline& p = ((eye == vr::Eye_Left) ? vrCtx.m_leftEyeDesc.pipeline : vrCtx.m_rightEyeDesc.pipeline);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, p.pipeline());
		for (const Mesh& mesh : meshes) {
			VkDeviceSize offset = 0;
			VkBuffer vertexBuffer = mesh.buffer.buffer();
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offset);
			VkBuffer indexBuffer = mesh.indices.buffer();
			vkCmdBindIndexBuffer(commandBuffer, indexBuffer, offset, VK_INDEX_TYPE_UINT32);

			for (const MeshInstanceState& state : mesh.instances) {
				glm::mat4 model = state.pose;
				glm::mat4 mvp = ((eye == vr::Eye_Left) ? vrCtx.eyeLeft : vrCtx.eyeRight) * glm::translate(glm::mat4(1.0f), {-3.8, 0, 0}) *gameState.planeState * state.pose;
				data.uniformBuffers[fi].copyInd(i, { mvp, getCamera(gameState), model });
				uint32_t offset = 256 * i;
				i++;
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, p.layout(), 0, 1, &data.descriptorSets[fi], 1, &offset);
				vkCmdDrawIndexed(commandBuffer, mesh.indices.size(), 1, 0, 0, 0);
			}
		}
	}
}


void RenderStereoTargets(SceneData& data, std::vector<Mesh>& meshes, Gamestate& gameState, VrCtx& vrCtx, VkCommandBuffer m_pCommandBuffer, int& i, int fi)
{

	// Set viewport and scissor
	VkViewport viewport = { 0.0f, 0.0f, (float)vrCtx.width, (float)vrCtx.height, 0.0f, 1.0f };
	vkCmdSetViewport(m_pCommandBuffer, 0, 1, &viewport);
	VkRect2D scissor = { 0, 0, vrCtx.width, vrCtx.height };
	vkCmdSetScissor(m_pCommandBuffer, 0, 1, &scissor);

	//----------//
	// Left Eye //
	//----------//
	// Transition eye image to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	VkImageMemoryBarrier imageMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	imageMemoryBarrier.oldLayout = vrCtx.m_leftEyeDesc.m_nImageLayout;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imageMemoryBarrier.image = vrCtx.m_leftEyeDesc.m_pImage;
	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	imageMemoryBarrier.subresourceRange.levelCount = 1;
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	imageMemoryBarrier.subresourceRange.layerCount = 1;
	imageMemoryBarrier.srcQueueFamilyIndex = vrCtx.m_nQueueFamilyIndex;
	imageMemoryBarrier.dstQueueFamilyIndex = vrCtx.m_nQueueFamilyIndex;
	vkCmdPipelineBarrier(m_pCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
	vrCtx.m_leftEyeDesc.m_nImageLayout = imageMemoryBarrier.newLayout;

	// Transition the depth buffer to VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL on first use
	if (vrCtx.m_leftEyeDesc.m_nDepthStencilImageLayout == VK_IMAGE_LAYOUT_UNDEFINED)
	{
		imageMemoryBarrier.image = vrCtx.m_leftEyeDesc.m_pDepthStencilImage;
		imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		imageMemoryBarrier.srcAccessMask = 0;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		imageMemoryBarrier.oldLayout = vrCtx.m_leftEyeDesc.m_nDepthStencilImageLayout;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		vkCmdPipelineBarrier(m_pCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
		vrCtx.m_leftEyeDesc.m_nDepthStencilImageLayout = imageMemoryBarrier.newLayout;
	}

	// Start the renderpass
	VkRenderPassBeginInfo renderPassBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	renderPassBeginInfo.renderPass = vrCtx.m_leftEyeDesc.m_pRenderPass;
	renderPassBeginInfo.framebuffer = vrCtx.m_leftEyeDesc.m_pFramebuffer;
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

	RenderScene(data, meshes, gameState, m_pCommandBuffer, i, vrCtx, fi, vr::Eye_Left);

	vkCmdEndRenderPass(m_pCommandBuffer);

	// Transition eye image to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL for display on the companion window
	imageMemoryBarrier.image = vrCtx.m_leftEyeDesc.m_pImage;
	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemoryBarrier.oldLayout = vrCtx.m_leftEyeDesc.m_nImageLayout;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	vkCmdPipelineBarrier(m_pCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
	vrCtx.m_leftEyeDesc.m_nImageLayout = imageMemoryBarrier.newLayout;

	//-----------//
	// Right Eye //
	//-----------//
	// Transition to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	imageMemoryBarrier.image = vrCtx.m_rightEyeDesc.m_pImage;
	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	imageMemoryBarrier.oldLayout = vrCtx.m_rightEyeDesc.m_nImageLayout;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	vkCmdPipelineBarrier(m_pCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
	vrCtx.m_rightEyeDesc.m_nImageLayout = imageMemoryBarrier.newLayout;

	// Transition the depth buffer to VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL on first use
	if (vrCtx.m_rightEyeDesc.m_nDepthStencilImageLayout == VK_IMAGE_LAYOUT_UNDEFINED)
	{
		imageMemoryBarrier.image = vrCtx.m_rightEyeDesc.m_pDepthStencilImage;
		imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		imageMemoryBarrier.srcAccessMask = 0;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		imageMemoryBarrier.oldLayout = vrCtx.m_rightEyeDesc.m_nDepthStencilImageLayout;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		vkCmdPipelineBarrier(m_pCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
		vrCtx.m_rightEyeDesc.m_nDepthStencilImageLayout = imageMemoryBarrier.newLayout;
	}

	// Start the renderpass
	renderPassBeginInfo.renderPass = vrCtx.m_rightEyeDesc.m_pRenderPass;
	renderPassBeginInfo.framebuffer = vrCtx.m_rightEyeDesc.m_pFramebuffer;
	renderPassBeginInfo.pClearValues = &clearValues[0];
	vkCmdBeginRenderPass(m_pCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	RenderScene(data, meshes, gameState, m_pCommandBuffer, i, vrCtx, fi, vr::Eye_Right);

	vkCmdEndRenderPass(m_pCommandBuffer);

	// Transition eye image to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL for display on the companion window
	imageMemoryBarrier.image = vrCtx.m_rightEyeDesc.m_pImage;
	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemoryBarrier.oldLayout = vrCtx.m_rightEyeDesc.m_nImageLayout;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	vkCmdPipelineBarrier(m_pCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
	vrCtx.m_rightEyeDesc.m_nImageLayout = imageMemoryBarrier.newLayout;
}

void renderVR(VkCtx& ctx, VrCtx& vrCtx) {
	vr::VRTextureBounds_t bounds;
	bounds.uMin = 0.0f;
	bounds.uMax = 1.0f;
	bounds.vMin = 0.0f;
	bounds.vMax = 1.0f;

	vr::VRVulkanTextureData_t vulkanData;
	vulkanData.m_nImage = (uint64_t)vrCtx.m_leftEyeDesc.m_pImage;
	vulkanData.m_pDevice = (VkDevice_T*)ctx.device();
	vulkanData.m_pPhysicalDevice = (VkPhysicalDevice_T*)ctx.physicalDevice();
	vulkanData.m_pInstance = (VkInstance_T*)ctx.instance();
	vulkanData.m_pQueue = (VkQueue_T*)ctx.graphicsQueue();
	vulkanData.m_nQueueFamilyIndex = ctx.graphicsQueueFamily();

	vulkanData.m_nWidth = vrCtx.width;
	vulkanData.m_nHeight = vrCtx.height;
	vulkanData.m_nFormat = VK_FORMAT_R8G8B8A8_SRGB;
	vulkanData.m_nSampleCount = 1;

	vr::Texture_t texture = { &vulkanData, vr::TextureType_Vulkan, vr::ColorSpace_Auto };
	vr::VRCompositor()->Submit(vr::Eye_Left, &texture, &bounds);

	vulkanData.m_nImage = (uint64_t)vrCtx.m_rightEyeDesc.m_pImage;
	vr::VRCompositor()->Submit(vr::Eye_Right, &texture, &bounds);
}