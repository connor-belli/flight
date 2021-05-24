// Read comments in imgui_imple.h.
#define GLM_CONFIG_CLIP_CONTROL GLM_CLIP_CONTROL_RH_ZO
#include <array>
#include <chrono>
#include <exception>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include <chrono>


#include <stdio.h>         
#include <stdlib.h>    

#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define IMGUI_UNLIMITED_FRAME_RATE
#ifdef _DEBUG
#define IMGUI_VULKAN_DEBUG_REPORT
#endif

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "defaultrenderpass.h"
#include "defaultpipeline.h"
#include "defaultvertex.h"
#include "vertexbuffer.h"
#include "imgui_imple.h"
#include "vkimage.h"
#include "framedata.h"
#include "model.h"
#include "physics.h"
#include "vrctx.h"
#include "modelregistry.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/writer.h>


#define GLM_FORCE_DEPTH_ZERO_TO_ONE

using namespace std::chrono_literals;

#undef main

static VkPipelineCache          g_PipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool         g_DescriptorPool = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static uint32_t                 g_MinImageCount = 2;
static bool                     g_SwapChainRebuild = false;
constexpr bool vre = true;

XrTime lastPredictedTime = 0;

static void SetupVulkan(VkCtx& ctx)
{
	// Create Descriptor Pool
	VkResult err;
	{
		VkDescriptorPoolSize pool_sizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 10 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 10 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 10 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 10 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 10 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 10 }
		};
		int len = 11;
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 100 * len;
		pool_info.poolSizeCount = (uint32_t)len;
		pool_info.pPoolSizes = pool_sizes;
		err = vkCreateDescriptorPool(ctx.device(), &pool_info, allocCallback, &g_DescriptorPool);
		check_vk_result(err);
	}

}

// All the ImGui_ImplVulkanH_XXX structures/functions are optional helpers used by the demo.
// Your real engine/app may not use them.
static void SetupVulkanWindow(const VkCtx& ctx, ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height)
{
	wd->Surface = surface;

	// Check for WSI support
	VkBool32 res;
	vkGetPhysicalDeviceSurfaceSupportKHR(ctx.physicalDevice(), ctx.graphicsQueueFamily(), wd->Surface, &res);
	if (res != VK_TRUE)
	{
		fprintf(stderr, "Error no WSI support on physical device 0\n");
		exit(-1);
	}
	// Select Surface Format
	const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
	const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(ctx.physicalDevice(), wd->Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);
	// Select Present Mode
#ifdef IMGUI_UNLIMITED_FRAME_RATE
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif
	wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(ctx.physicalDevice(), wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
	IM_ASSERT(g_MinImageCount >= 2);
	ImGui_ImplVulkanH_CreateOrResizeWindow(ctx.instance(), ctx.physicalDevice(), ctx.device(), wd, ctx.graphicsQueueFamily(), allocCallback, width, height, g_MinImageCount);
}


static void CleanupVulkan(VkCtx& ctx)
{
	vkDestroyDescriptorPool(ctx.device(), g_DescriptorPool, allocCallback);
}

static void CleanupVulkanWindow(VkCtx& ctx)
{
	ImGui_ImplVulkanH_DestroyWindow(ctx.instance(), ctx.device(), &g_MainWindowData, allocCallback);
}



glm::mat4 getCamera(Gamestate& state) {
	glm::mat4 proj = glm::ortho(-20.f, 20.f, 20.f, -20.0f, 10.0f, 2000.0f);
	glm::mat4 lookAt = glm::lookAt(glm::vec3(50, 50, 50), glm::vec3(0.0f), glm::vec3(0, 1, 0));
	glm::mat4 rotate = glm::rotate(glm::rotate(glm::mat4(1.0f), (float)3.141592 / 4, glm::vec3(1.0, 0.0, 0.0)), (float)3.141592 / 4, glm::vec3(0.0, 1.0, 0.0));
	glm::mat4 view = lookAt * glm::translate(glm::mat4(1.0f), -state.pos);
	return proj * view;
}

static void FrameRender(const VkCtx& ctx, ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data, SceneData& data, std::vector<Mesh>& meshes, Gamestate& gameState, VrCtx& vrCtx)
{
	XrFrameState frame_state = { XR_TYPE_FRAME_STATE };
	XrSwapchainImageWaitInfo wait_info = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
	wait_info.timeout = XR_INFINITE_DURATION;
	if (vre) {
		auto e = xrWaitFrame(vrCtx.session, nullptr, &frame_state);
		xrBeginFrame(vrCtx.session, nullptr);
		vrCtx.updateHeadMat(frame_state.predictedDisplayTime);
	}
	lastPredictedTime = frame_state.predictedDisplayTime;
	VkResult err;

	VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
	VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
	err = vkAcquireNextImageKHR(ctx.device(), wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
	{
		g_SwapChainRebuild = true;
		return;
	}
	check_vk_result(err);

	ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
	{
		err = vkWaitForFences(ctx.device(), 1, &fd->Fence, VK_TRUE, UINT64_MAX);    // wait indefinitely instead of periodically checking
		check_vk_result(err);

		err = vkResetFences(ctx.device(), 1, &fd->Fence);
		check_vk_result(err);
	}
	{
		err = vkResetCommandPool(ctx.device(), fd->CommandPool, 0);
		check_vk_result(err);
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
		check_vk_result(err);
	}
	int i = 0;
	{
		VkClearValue clearValues[] = { {1.0f, 0} };
		VkRenderPassBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass = data.shadowPass.renderPass();
		info.framebuffer = data.shadowFramebuffer;
		info.renderArea.extent.width = 2048;
		info.renderArea.extent.height = 2048;
		info.clearValueCount = 1;
		info.pClearValues = clearValues;
		vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
		VkViewport viewport;
		viewport.height = 2048;
		viewport.width = 2048;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		viewport.x = 0;
		viewport.y = 0;
		VkRect2D scissor;
		scissor.offset = { 0, 0 };
		scissor.extent = { 2048, 2048 };

		vkCmdSetViewport(fd->CommandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(fd->CommandBuffer, 0, 1, &scissor);
	}
	{
		vkCmdSetDepthBias(fd->CommandBuffer, 10000.0f, 0.0f, 1.0f);
		vkCmdBindPipeline(fd->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, data.shadowPipeline.pipeline());
		for (const Mesh& mesh : meshes) {
			VkDeviceSize offset = 0;
			VkBuffer vertexBuffer = mesh.buffer.buffer();
			vkCmdBindVertexBuffers(fd->CommandBuffer, 0, 1, &vertexBuffer, &offset);
			VkBuffer indexBuffer = mesh.indices.buffer();
			vkCmdBindIndexBuffer(fd->CommandBuffer, indexBuffer, offset, VK_INDEX_TYPE_UINT32);

			for (const MeshInstanceState& state : mesh.instances) {
				glm::mat4 model = state.pose;
				glm::mat4 mvp = getCamera(gameState) * model;
				data.uniformBuffers[wd->FrameIndex].copyInd(i, { mvp });
				uint32_t offset = 256 * i;
				i++;
				vkCmdBindDescriptorSets(fd->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, data.shadowPipeline.layout(), 0, 1, &data.shadowSets[wd->FrameIndex], 1, &offset);
				vkCmdDrawIndexed(fd->CommandBuffer, mesh.indices.size(), 1, 0, 0, 0);
			}
		}
		vkCmdEndRenderPass(fd->CommandBuffer);
	}
	std::vector<XrCompositionLayerProjectionView> views;
	if (vre) {
		vrCtx.vrGfxCtx.RenderStereoTargets(data, meshes, gameState,  fd->CommandBuffer, vrCtx, i, wd->FrameIndex, views, frame_state.predictedDisplayTime);
		
		VkImageMemoryBarrier imageMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageMemoryBarrier.image = fd->Backbuffer;
		imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
		imageMemoryBarrier.subresourceRange.levelCount = 1;
		imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
		imageMemoryBarrier.subresourceRange.layerCount = 1;
		imageMemoryBarrier.srcQueueFamilyIndex = ctx.graphicsQueueFamily();
		imageMemoryBarrier.dstQueueFamilyIndex = ctx.graphicsQueueFamily();
		vkCmdPipelineBarrier(fd->CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
	}

	if (!vre) {
		{
			VkClearValue clearValues[] = { wd->ClearValue, {1.0f, 0} };
			VkRenderPassBeginInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			info.renderPass = data.pass.renderPass();
			info.framebuffer = data.frameBuffers[wd->FrameIndex];
			info.renderArea.extent.width = wd->Width;
			info.renderArea.extent.height = wd->Height;
			info.clearValueCount = 2;
			info.pClearValues = clearValues;
			vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
		}

		vkCmdBindPipeline(fd->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, data.pipeline.pipeline());
		{
			VkViewport viewport;
			viewport.height = wd->Height;
			viewport.width = wd->Width;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			viewport.x = 0;
			viewport.y = 0;
			VkRect2D scissor;
			scissor.offset = { 0, 0 };
			scissor.extent = { (uint32_t)wd->Width, (uint32_t)wd->Height };

			vkCmdSetViewport(fd->CommandBuffer, 0, 1, &viewport);
			vkCmdSetScissor(fd->CommandBuffer, 0, 1, &scissor);
		}
		{
			for (const Mesh& mesh : meshes) {
				VkDeviceSize offset = 0;
				VkBuffer vertexBuffer = mesh.buffer.buffer();
				vkCmdBindVertexBuffers(fd->CommandBuffer, 0, 1, &vertexBuffer, &offset);
				VkBuffer indexBuffer = mesh.indices.buffer();
				vkCmdBindIndexBuffer(fd->CommandBuffer, indexBuffer, offset, VK_INDEX_TYPE_UINT32);
				glm::mat4 proj = glm::infinitePerspective(glm::radians(45.0f), wd->Width / (float)wd->Height, 1.0f);

				glm::mat4 rotate = glm::rotate(glm::rotate(glm::mat4(1.0f), (float)gameState.ty, glm::vec3(1.0, 0.0, 0.0)), (float)gameState.tx, glm::vec3(0.0, 1.0, 0.0));
				glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0, 0.0, -gameState.zoom)) * rotate * gameState.planeState;
				proj[1][1] *= -1;

				for (const MeshInstanceState& state : mesh.instances) {
					glm::mat4 model = state.pose;
					glm::mat4 mvp = proj * view * model;

					data.uniformBuffers[wd->FrameIndex].copyInd(i, { mvp, getCamera(gameState), model });
					uint32_t offset = 256 * i;
					i++;
					MaterialPushConstants c{};
					c.ambience = mesh.ambience;
					c.color = mesh.color;
					c.normalMulFactor = mesh.normMul;
					c.mixRatio = mesh.mixRatio;
					vkCmdPushConstants(fd->CommandBuffer, data.pipeline.layout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MaterialPushConstants), &c);
					vkCmdBindDescriptorSets(fd->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, data.pipeline.layout(), 0, 1, &data.descriptorSets[wd->FrameIndex], 1, &offset);
					vkCmdDrawIndexed(fd->CommandBuffer, mesh.indices.size(), 1, 0, 0, 0);
				}
			}
		}
		vkCmdEndRenderPass(fd->CommandBuffer);
	}
	else {
		VkOffset3D blitSizeSrc;
		blitSizeSrc.x = vrCtx.vrGfxCtx.width;
		blitSizeSrc.y = vrCtx.vrGfxCtx.height;
		blitSizeSrc.z = 1;
		VkOffset3D blitSizeDst;
		blitSizeDst.x = wd->Width;
		blitSizeDst.y = wd->Height;
		blitSizeDst.z = 1;

		VkImageBlit imageBlitRegion{};
		imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlitRegion.srcSubresource.layerCount = 1;
		imageBlitRegion.srcOffsets[1] = blitSizeSrc;
		imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlitRegion.dstSubresource.layerCount = 1;
		imageBlitRegion.dstOffsets[1] = blitSizeDst;

		vkCmdBlitImage(fd->CommandBuffer, vrCtx.vrGfxCtx.descs[0].swapchainImages[0].colorImage, vrCtx.vrGfxCtx.descs[0].swapchainImages[0].layout, fd->Backbuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlitRegion, VK_FILTER_NEAREST);
		VkImageMemoryBarrier imageMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageMemoryBarrier.image = fd->Backbuffer;
		imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
		imageMemoryBarrier.subresourceRange.levelCount = 1;
		imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
		imageMemoryBarrier.subresourceRange.layerCount = 1;
		imageMemoryBarrier.srcQueueFamilyIndex = ctx.graphicsQueueFamily();
		imageMemoryBarrier.dstQueueFamilyIndex = ctx.graphicsQueueFamily();
		vkCmdPipelineBarrier(fd->CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
	}
	{
		VkRenderPassBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass = wd->RenderPass;
		info.framebuffer = fd->Framebuffer;
		info.renderArea.extent.width = wd->Width;
		info.renderArea.extent.height = wd->Height;
		info.clearValueCount = 1;
		info.pClearValues = &wd->ClearValue;
		vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}
	ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);
	vkCmdEndRenderPass(fd->CommandBuffer);

	{
		if (vre) {
			for (int j = 0; j < vrCtx.vrGfxCtx.views.size(); j++) {
				Desc& desc = vrCtx.vrGfxCtx.descs[j];
				xrWaitSwapchainImage(desc.swapchain, &wait_info);
			}
		}
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &image_acquired_semaphore;
		info.pWaitDstStageMask = &wait_stage;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &fd->CommandBuffer;
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &render_complete_semaphore;

		err = vkEndCommandBuffer(fd->CommandBuffer);
		check_vk_result(err);
		err = vkQueueSubmit(ctx.graphicsQueue(), 1, &info, fd->Fence);
		check_vk_result(err);
		err = vkWaitForFences(ctx.device(), 1, &fd->Fence, true, 99999999);
		if (vre) {
			for (int j = 0; j < vrCtx.vrGfxCtx.views.size(); j++) {
				Desc& desc = vrCtx.vrGfxCtx.descs[j];
				XrSwapchainImageReleaseInfo release_info = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
				xrReleaseSwapchainImage(desc.swapchain, &release_info);
			}

			XrCompositionLayerProjection layer = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
			layer.viewCount = views.size();
			layer.views = views.data();
			layer.space = vrCtx.space;
			XrFrameEndInfo end_info{ XR_TYPE_FRAME_END_INFO };
			end_info.displayTime = frame_state.predictedDisplayTime;
			end_info.environmentBlendMode = vrCtx.vrGfxCtx.blendMode;
			end_info.layerCount = 1;
			auto x = (&layer);
			end_info.layers = (XrCompositionLayerBaseHeader**)&x;

			xrEndFrame(vrCtx.session, &end_info);
		}
	}
}


static void FramePresent(const VkCtx& ctx, ImGui_ImplVulkanH_Window* wd)
{
	if (g_SwapChainRebuild)
		return;
	VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
	VkPresentInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = &render_complete_semaphore;
	info.swapchainCount = 1;
	info.pSwapchains = &wd->Swapchain;
	info.pImageIndices = &wd->FrameIndex;
	VkResult err = vkQueuePresentKHR(ctx.graphicsQueue(), &info);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
	{
		g_SwapChainRebuild = true;
		return;
	}
	check_vk_result(err);
	wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->ImageCount; // Now we can use the next set of semaphores
}




int lockedX, lockedY;
bool locked = true;

int SDL_main(int, char**)
{
	// Setup SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0)
	{
		printf("Error: %s\n", SDL_GetError());
		return -1;
	}

	// Setup window
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_Window* window = SDL_CreateWindow("the gamecube", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
	SDL_MaximizeWindow(window);
	std::this_thread::sleep_for(100ms);
	// Setup Vulkan
	uint32_t extensions_count = 0;
	SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, NULL);
	const char** extensions = new const char* [extensions_count];
	SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, extensions);
	VkCtx ctx(extensions, extensions_count);
	SetupVulkan(ctx);

	delete[] extensions;
	// Create Window Surface
	VkSurfaceKHR surface;
	VkResult err;
	if (SDL_Vulkan_CreateSurface(window, ctx.instance(), &surface) == 0)
	{
		printf("Failed to create Vulkan surface.\n");
		return 1;
	}
	// Create Framebuffers
	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
	wd->ClearEnable = false;
	SetupVulkanWindow(ctx, wd, surface, w, h);
	wd->ClearEnable = false;
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForVulkan(window);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = ctx.instance();
	init_info.PhysicalDevice = ctx.physicalDevice();
	init_info.Device = ctx.device();
	init_info.QueueFamily = ctx.graphicsQueueFamily();
	init_info.Queue = ctx.graphicsQueue();
	init_info.PipelineCache = g_PipelineCache;
	init_info.DescriptorPool = g_DescriptorPool;
	init_info.Allocator = allocCallback;
	init_info.MinImageCount = g_MinImageCount;
	init_info.ImageCount = wd->ImageCount;
	init_info.CheckVkResultFn = check_vk_result;
	ImGui_ImplVulkan_Init(&init_info, wd->RenderPass);
	std::cout << "Init imgui" << std::endl;

	{
		// Use any command queue
		VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
		VkCommandBuffer command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;

		err = vkResetCommandPool(ctx.device(), command_pool, 0);
		check_vk_result(err);
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		err = vkBeginCommandBuffer(command_buffer, &begin_info);
		check_vk_result(err);

		ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

		VkSubmitInfo end_info = {};
		end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		end_info.commandBufferCount = 1;
		end_info.pCommandBuffers = &command_buffer;
		err = vkEndCommandBuffer(command_buffer);
		check_vk_result(err);
		err = vkQueueSubmit(ctx.graphicsQueue(), 1, &end_info, VK_NULL_HANDLE);
		check_vk_result(err);

		err = vkDeviceWaitIdle(ctx.device());
		check_vk_result(err);
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}

	rapidjson::Document doc;
	std::vector<char> s = readFile("untitled.gltf");
	doc.Parse(s.data(), s.size());
	GLTFRoot root;
	root.parseDoc(doc);
	root.loadBuffs();
	std::vector<Mesh> meshes;
	for (int i = 0; i < root.meshes.size(); i++) {
		meshes.push_back(std::move(root.createModel(ctx, root.meshes[i])));
	}
	for (int i = 0; i < root.scenes[0].nodes.size(); i++) {
		root.processNodes(root.nodes[root.scenes[0].nodes[i]], glm::mat4(1.0f), meshes);
	}

	int nFrames = wd->ImageCount;
	SceneData data(ctx, wd->Width, wd->Height, nFrames, wd, g_DescriptorPool);
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	Gamestate state;
	bool done = false;
	int patCount = 1;


	PhysicsContainer container = createPhysicsContainer();
	std::vector<PhysicsObj> objs;
	Plane plane(meshes, container, state.pos);
	objs.push_back(plane.main);
	objs.push_back(plane.leftGear);
	objs.push_back(plane.rightGear);
	GroundShape ground(container, root, root.nodes[0]);
	meshes[0].ambience = 0.75;
	meshes[0].normMul = 0.8;
	Uint64 NOW = SDL_GetPerformanceCounter();
	Uint64 LAST = 0;
	double deltaTime = 0;
	VrCtx vrCtx(ctx);
	initAudio();
	if (vre) {
		vrCtx.initVrCtx(ctx);
	}
	try {
		while (!done)
		{
			LAST = NOW;
			NOW = SDL_GetPerformanceCounter();

			deltaTime = ((NOW - LAST) / (double)SDL_GetPerformanceFrequency());
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				ImGui_ImplSDL2_ProcessEvent(&event);
				if (event.type == SDL_QUIT)
					done = true;
				if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
					done = true;
				if (event.type == SDL_MOUSEWHEEL) {
					state.zoom += -0.5 * event.wheel.y;
					if (state.zoom < 0) state.zoom = 0;
				}
				if (event.type == SDL_MOUSEMOTION) {
					if (locked) {
						int x = event.motion.xrel;
						int y = event.motion.yrel;
						state.tx += ((double)x / 1000);
						state.ty += ((double)y / 1000);
					}
				}
			}
			if (!io.WantCaptureKeyboard) {
				const uint8_t* keystate = SDL_GetKeyboardState(NULL);
				plane.roll = 0;
				plane.roll += keystate[SDL_SCANCODE_Q];
				plane.roll -= keystate[SDL_SCANCODE_E];

				plane.yaw = 0;
				plane.yaw += keystate[SDL_SCANCODE_D];
				plane.yaw -= keystate[SDL_SCANCODE_A];

				plane.space = 0;
				plane.space += keystate[SDL_SCANCODE_S];
				plane.space -= keystate[SDL_SCANCODE_W];

				if (keystate[SDL_SCANCODE_LCTRL]) {
					plane.throttle -= deltaTime;
					if (plane.throttle < 0) plane.throttle = 0;
				}
				if (keystate[SDL_SCANCODE_LSHIFT]) {
					plane.throttle += deltaTime;
					if (plane.throttle > 1) plane.throttle = 1;
				}
			}


			if (!io.WantCaptureMouse && (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_RIGHT)) && !locked) {
				SDL_SetRelativeMouseMode(SDL_TRUE);
				locked = true;
			}
			if (!(SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_RIGHT))) {
				SDL_SetRelativeMouseMode(SDL_FALSE);
				locked = false;
			}

			if (vre) {
				XrEventDataBuffer event_buffer = { XR_TYPE_EVENT_DATA_BUFFER };
				while (xrPollEvent(vrCtx.instance, &event_buffer) == XR_SUCCESS) {
					switch (event_buffer.type) {
					case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
						XrEventDataSessionStateChanged* changed = (XrEventDataSessionStateChanged*)&event_buffer;
						vrCtx.sessionState = changed->state;

						// Session state change is where we can begin and end sessions, as well as find quit messages!
						switch (vrCtx.sessionState) {
						case XR_SESSION_STATE_READY: {
							XrSessionBeginInfo begin_info = { XR_TYPE_SESSION_BEGIN_INFO };
							begin_info.primaryViewConfigurationType = app_config_view;
							xrBeginSession(vrCtx.session, &begin_info);
						} break;
						case XR_SESSION_STATE_STOPPING: {
							xrEndSession(vrCtx.session);
						} break;
						case XR_SESSION_STATE_EXITING:      done = true; break;
						case XR_SESSION_STATE_LOSS_PENDING: done = true; break;
						}
					} break;
					case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: done = true; return 1;
					}
					event_buffer = { XR_TYPE_EVENT_DATA_BUFFER };
				}
				vrCtx.actionSet.sync(vrCtx.session);
			}

			// Resize swap chain?
			if (g_SwapChainRebuild)
			{
				int width, height;
				SDL_GetWindowSize(window, &width, &height);
				if (width > 0 && height > 0)
				{
					ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
					ImGui_ImplVulkanH_CreateOrResizeWindow(ctx.instance(), ctx.physicalDevice(), ctx.device(), &g_MainWindowData, ctx.graphicsQueueFamily(), allocCallback, width, height, g_MinImageCount);
					data.recreate(wd);

					g_MainWindowData.FrameIndex = 0;
					g_SwapChainRebuild = false;
				}
			}

			// Start the Dear ImGui frame
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplSDL2_NewFrame(window);
			ImGui::NewFrame();
			plane.calculatePlaneStats();
			plane.doPlaneSounds(container);
			{
				ImGui::Begin("Info");

				ImGui::Text("Player Pos: %s", glm::to_string(state.pos).c_str());

				ImGui::ColorEdit3("Clear color", (float*)&clear_color);

				if (ImGui::Button("Create pat man")) {
					PhysicsObj obj(meshes[2], container, state.pos + glm::vec3{ 0, 10, 0 }, 2, sphere, 1.0f);
					objs.push_back(obj);
					patCount++;
				}
				ImGui::SameLine();
				ImGui::Text("Pat count: %d", patCount);
				ImGui::Text("Throttle %f", plane.throttle);

				ImGui::Text("Angle of attack %f", plane.aoa * 180 / 3.141592);

				btVector3 up = (btVector3{ 0, 1, 0 } *plane.basis);
				btVector3 forward = (btVector3{ 1, 0, 0 } *plane.basis);
				ImGui::Text("vel %f %f %f", plane.vel.x(), plane.vel.y(), plane.vel.z());


				if (ImGui::Button("Reset view") && vre) {
					vrCtx.resetReferenceSpace(lastPredictedTime);
				}

				ImGui::Text("up %f %f %f", up.x(), up.y(), up.z());
				ImGui::Text("forward %f %f %f", forward.x(), forward.y(), forward.z());
				ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
				ImGui::End();
			}
			plane.doPlanePhysics();
			container.dynamicsWorld->stepSimulation(deltaTime);
			for (auto& obj : objs) {
				obj.updateState(meshes[obj.meshIndice]);
			}


			btVector3 t = plane.main.rigid->getCenterOfMassTransform().getOrigin();
			plane.main.rigid->getWorldTransform().inverse().getOpenGLMatrix((float*)&state.planeState);
			state.pos = { t.x(), t.y(), t.z() };

			ImGui::Render();
			ImDrawData* draw_data = ImGui::GetDrawData();
			const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
			if (!is_minimized)
			{
				wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
				wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
				wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
				wd->ClearValue.color.float32[3] = clear_color.w;
				FrameRender(ctx, wd, draw_data, data, meshes, state, vrCtx);
				FramePresent(ctx, wd);
			}
		}
	}
	catch (std::exception &e) {
		std::cout << e.what() << std::endl;
	}

	// Cleanup
	err = vkDeviceWaitIdle(ctx.device());
	check_vk_result(err);
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	data.destroy();
	CleanupVulkanWindow(ctx);
	CleanupVulkan(ctx);

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}