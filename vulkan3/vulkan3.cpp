// Read comments in imgui_imple.h.
#define GLM_CONFIG_CLIP_CONTROL GLM_CLIP_CONTROL_RH_ZO
#include <array>
#include <chrono>
#include <exception>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>

#include <stdio.h>         
#include <stdlib.h>    

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "vkctx.h"
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

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <SDL2/SDL_mixer.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/writer.h>
#define TINYGLTF_USE_RAPIDJSON
#define TINYGLTF_NO_INCLUDE_JSON
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>
#include <openvr.h>


#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#undef main

#define IMGUI_UNLIMITED_FRAME_RATE
#ifdef _DEBUG
#define IMGUI_VULKAN_DEBUG_REPORT
#endif

static VkPipelineCache          g_PipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool         g_DescriptorPool = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static uint32_t                 g_MinImageCount = 2;
static bool                     g_SwapChainRebuild = false;
constexpr bool vre = true;



static void SetupVulkan(VkCtx& ctx)
{
	// Create Descriptor Pool
	VkResult err;
	{
		VkDescriptorPoolSize pool_sizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 }
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
	//printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

	// Create SwapChain, RenderPass, Framebuffer, etc.
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
	glm::mat4 proj = glm::ortho(-20.f, 20.f, 20.f, -20.0f, 1.0f, 2000.0f);
	//proj[1][1] *= -1;
	glm::mat4 rotate = glm::rotate(glm::rotate(glm::mat4(1.0f), (float)3.141592 / 4, glm::vec3(1.0, 0.0, 0.0)), (float)3.141592 / 4, glm::vec3(0.0, 1.0, 0.0));
	glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0, 0.0, -100)) * rotate * glm::translate(glm::mat4(1.0f), -state.pos);
	return proj * view;
}

static void FrameRender(const VkCtx& ctx, ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data, SceneData& data, std::vector<Mesh>& meshes, Gamestate& gameState, VrCtx& vrCtx)
{
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
		info.renderArea.extent.width = 4096;
		info.renderArea.extent.height = 4096;
		info.clearValueCount = 1;
		info.pClearValues = clearValues;
		vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
		VkViewport viewport;
		viewport.height = 4096;
		viewport.width = 4096;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		viewport.x = 0;
		viewport.y = 0;
		VkRect2D scissor;
		scissor.offset = { 0, 0 };
		scissor.extent = { 4096, 4096 };

		vkCmdSetViewport(fd->CommandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(fd->CommandBuffer, 0, 1, &scissor);
	}
	{
		vkCmdSetDepthBias(fd->CommandBuffer, 0.0f, 0.0f, 0.0f);
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
	if (vre) {
		RenderStereoTargets(data, meshes, gameState, vrCtx, fd->CommandBuffer, i, wd->FrameIndex);
	}

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
				vkCmdBindDescriptorSets(fd->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, data.pipeline.layout(), 0, 1, &data.descriptorSets[wd->FrameIndex], 1, &offset);
				vkCmdDrawIndexed(fd->CommandBuffer, mesh.indices.size(), 1, 0, 0, 0);
			}
		}
	}
	vkCmdEndRenderPass(fd->CommandBuffer);
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
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
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
	}
}

static glm::mat4 matConvert(vr::HmdMatrix34_t matPose) {
	glm::mat4 matrixObj(
		matPose.m[0][0], matPose.m[1][0], matPose.m[2][0], 0.0,
		matPose.m[0][1], matPose.m[1][1], matPose.m[2][1], 0.0,
		matPose.m[0][2], matPose.m[1][2], matPose.m[2][2], 0.0,
		matPose.m[0][3], matPose.m[1][3], matPose.m[2][3], 1.0f
	);
	return matrixObj;
}

static glm::mat4 matConvert4(vr::HmdMatrix44_t matPose) {
	glm::mat4 matrixObj(
		matPose.m[0][0], matPose.m[1][0], matPose.m[2][0], matPose.m[3][0],
		matPose.m[0][1], matPose.m[1][1], matPose.m[2][1], matPose.m[3][1],
		matPose.m[0][2], matPose.m[1][2], matPose.m[2][2], matPose.m[3][2],
		matPose.m[0][3], matPose.m[1][3], matPose.m[2][3], matPose.m[3][3]
	);
	return matrixObj;
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


static Mesh createModel(const VkCtx& ctx, tinygltf::Model &model, tinygltf::Mesh& mesh) {
	auto prims = mesh.primitives[0];
	auto indicesAcc = model.accessors[prims.indices];
	auto normalAcc = model.accessors[prims.attributes["NORMAL"]];
	bool hasColor = prims.attributes.find("COLOR_0") != prims.attributes.end();
	auto positionAcc = model.accessors[prims.attributes["POSITION"]];
	auto colorAcc = model.accessors[prims.attributes["COLOR_0"]];

	//assert(indicesAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT);
	auto indicesBufferView = model.bufferViews[indicesAcc.bufferView];
	auto normalBufferView = model.bufferViews[normalAcc.bufferView];
	auto positionBufferView = model.bufferViews[positionAcc.bufferView];
	auto colorBufferView = model.bufferViews[colorAcc.bufferView];

	
	std::vector<Vertex> vertices(positionAcc.count);
	if(hasColor)
		for (int i = 0; i < positionAcc.count; i++) {
			Vertex& v = vertices[i];
			memcpy(&v.pos, model.buffers[positionBufferView.buffer].data.data() + positionBufferView.byteOffset + i * sizeof(float) * 3, sizeof(float) * 3);
			memcpy(&v.color, model.buffers[colorBufferView.buffer].data.data() + colorBufferView.byteOffset + i * sizeof(unsigned short) * 4, sizeof(unsigned short) * 3);
			memcpy(&v.normal, model.buffers[normalBufferView.buffer].data.data() + normalBufferView.byteOffset + i * sizeof(float) * 3, sizeof(float) * 3);
		}
	else {
		for (int i = 0; i < positionAcc.count; i++) {
			Vertex& v = vertices[i];
			memcpy(&v.pos, model.buffers[positionBufferView.buffer].data.data() + positionBufferView.byteOffset + i * sizeof(float) * 3, sizeof(float) * 3);
			v.color = { 65565, 65565, 65565 };
			memcpy(&v.normal, model.buffers[normalBufferView.buffer].data.data() + normalBufferView.byteOffset + i * sizeof(float) * 3, sizeof(float) * 3);
		}
	}

	std::vector<uint32_t> indices(indicesAcc.count);
	if(indicesAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
		memcpy(indices.data(), model.buffers[indicesBufferView.buffer].data.data() + indicesBufferView.byteOffset, indicesBufferView.byteLength);
	else {
		for (int i = 0; i < indices.size(); i++) {
			uint16_t* begin = (uint16_t*) (model.buffers[indicesBufferView.buffer].data.data() + indicesBufferView.byteOffset);
			indices[i] = begin[i];
		}
	}
	VertexBuffer buffer(ctx, vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	IndexBuffer indexBuffer(ctx, indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	return std::move(Mesh{
		std::move(buffer),
		std::move(indexBuffer)
	});
}

void processNodes(tinygltf::Model &model, tinygltf::Node &node, glm::mat4 prevState, std::vector<Mesh>& meshes) {
	glm::vec3 pos(0.0f);
	if (node.translation.size() != 0) {
		pos = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
	}
	glm::quat rot(1.0, 0.0, 0.0, 0.0);
	if (node.rotation.size() != 0) {
		rot = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
	}
	glm::vec3 scale(1.0);
	if (node.scale.size() != 0) {
		scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
	}
	glm::mat4 rotMat = glm::toMat4(rot);
	glm::mat4 curState = glm::translate(glm::mat4(1.0f), pos) * rotMat * glm::scale(glm::mat4(1.0f), scale);
	if (node.mesh != -1) {
		MeshInstanceState state;
		state.pose = curState;
		meshes[node.mesh].instances.push_back(state);
	}
	for (int i = 0; i < node.children.size(); i++) {
		processNodes(model, model.nodes[node.children[i]], curState, meshes);
	}
}

Mix_Chunk* chunk;
Mix_Chunk* groundHit;
Mix_Chunk* tooLowGear;
Mix_Chunk* breakSound;
Mix_Chunk* engine;

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
	Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096);
	groundHit = Mix_LoadWAV("GroundProximity.wav");
	chunk = Mix_LoadWAV("LOWALT.wav");
	tooLowGear = Mix_LoadWAV("TooLowGear.wav");
	breakSound = Mix_LoadWAV("break.wav");
	engine = Mix_LoadWAV("engine.wav");
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
	tinygltf::TinyGLTF loader;
	tinygltf::Model model;
	std::string e;
	loader.LoadASCIIFromFile(&model, &e, &e, "untitled.gltf");
	std::vector<Mesh> meshes;
	for (int i = 0; i < model.meshes.size(); i++) {
		meshes.push_back(std::move(createModel(ctx, model, model.meshes[i])));
	}
	for (int i = 0; i < model.scenes[0].nodes.size(); i++) {
		processNodes(model, model.nodes[model.scenes[0].nodes[i]], glm::mat4(1.0f), meshes);
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
	GroundShape ground(container, model, model.meshes[0]);
	Uint64 NOW = SDL_GetPerformanceCounter();
	Uint64 LAST = 0;
	double deltaTime = 0;
	if (vre) {
		vr::VRCompositor();
	}
	VrCtx vrCtx(ctx);
	glm::mat4 lex;
	glm::mat4 rex;
	glm::mat4 lep;
	glm::mat4 rep;
	if (vre) {
		vrCtx.initVrCtx(ctx);
		lex = matConvert(vrCtx.sys->GetEyeToHeadTransform(vr::Eye_Left));
		rex = matConvert(vrCtx.sys->GetEyeToHeadTransform(vr::Eye_Right));
		lep = matConvert4(vrCtx.sys->GetProjectionMatrix(vr::Eye_Left, 0.1, 20000));
		lep[1][1] *= -1;
		rep = matConvert4(vrCtx.sys->GetProjectionMatrix(vr::Eye_Right, 0.1, 20000));
		rep[1][1] *= -1;
	}
	while (!done)
	{
		LAST = NOW;
		NOW = SDL_GetPerformanceCounter();

		deltaTime = ((NOW - LAST)  / (double)SDL_GetPerformanceFrequency());
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

			vr::TrackedDevicePose_t m_rTrackedDevicePose[vr::k_unMaxTrackedDeviceCount];
			vr::VRCompositor()->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0);

			int m_iValidPoseCount = 0;
			std::string m_strPoseClasses = "";
			if (m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
			{
				auto m_mat4HMDPose = matConvert(m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking);
				 m_mat4HMDPose = glm::inverse(m_mat4HMDPose);
				 vrCtx.eyeLeft = lep * glm::inverse(lex) * m_mat4HMDPose;
				 vrCtx.eyeRight = rep * glm::inverse(rex)* m_mat4HMDPose;
			}
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

		{
			ImGui::Begin("Info");

			ImGui::Text("Player Pos: %s", glm::to_string(state.pos).c_str());

			ImGui::ColorEdit3("clear color", (float*)&clear_color);

			if (ImGui::Button("Create pat man")) {
				PhysicsObj obj(meshes[2], container, state.pos + glm::vec3{ 0, 10, 0 }, 2, sphere, 1.0f);
				objs.push_back(obj);
				patCount++;
			}
			ImGui::SameLine();
			ImGui::Text("Pat count: %d", patCount);
			ImGui::Text("Throttle %f", plane.throttle);
			auto basis = plane.main.rigid->getWorldTransform().getBasis().inverse();
			btVector3 vel = plane.main.rigid->getLinearVelocity();
			btVector3 localVel = vel * basis.inverse();
			float aoa = atan2(localVel.y(), localVel.x());
			ImGui::Text("Angle of attack %f", aoa * 180 / 3.141592);
			if (vel.length() > 40) {
				btVector3 pos = plane.main.rigid->getWorldTransform().getOrigin();
				btVector3 to = pos + vel * 5;
				btCollisionWorld::ClosestRayResultCallback closestResults(pos, to);
				closestResults.m_flags |= btTriangleRaycastCallback::kF_FilterBackfaces;
				closestResults.m_collisionFilterGroup = 1;
				container.dynamicsWorld->rayTest(pos, to, closestResults);
				if (closestResults.hasHit() && abs(closestResults.m_hitNormalWorld.dot(vel.normalized())) > 0.25) {
					if (!Mix_Playing(1))
						Mix_PlayChannel(1, groundHit, -1);
				}
				else {
					Mix_HaltChannel(1);
				}
			}
			else {
				Mix_HaltChannel(1);
			}
			if (plane.throttle > 0.1) {
				glm::mat4 rotate = glm::rotate(glm::rotate(glm::mat4(1.0f), (float)state.ty, glm::vec3(1.0, 0.0, 0.0)), (float)state.tx, glm::vec3(0.0, 1.0, 0.0));
				glm::vec4 posp = glm::vec4{ -state.zoom, 0.0, 0.0, 1.0f } *rotate;

				glm::vec4 del = (posp - glm::vec4{ 4.0f, -0.5f, 0.0f, 1.0f });
				ImGui::Text("Pos %s", glm::to_string(del).c_str());
				float length = glm::length(del);
				float angle = atan2(-del.z, del.x);
				angle = 180 * (angle / 3.141592);
				ImGui::Text("Angle %f, length %f", length, angle);
				Mix_SetPosition(6, angle , length);
				if (!Mix_Playing(6)) {
					Mix_Volume(6, 32);
					Mix_PlayChannel(6, engine, -1);
				}
			}
			else {
				Mix_HaltChannel(6);
			}
			static bool debounce = false;
			if (vel.length() > 80) {
				if (!Mix_Playing(2)) {
					btVector3 pos = plane.main.rigid->getWorldTransform().getOrigin();
					btVector3 to = pos + btVector3{0, -10, 0}*basis;
					btCollisionWorld::ClosestRayResultCallback closestResults(pos, to);
					closestResults.m_flags |= btTriangleRaycastCallback::kF_FilterBackfaces;
					closestResults.m_collisionFilterGroup = 1;
					container.dynamicsWorld->rayTest(pos, to, closestResults);
					if (closestResults.hasHit()) {
						if (!Mix_Playing(2) && debounce == false) {
							Mix_PlayChannel(2, tooLowGear, 0);
							debounce = true;
						}
					}
					else {
						debounce = false;
					}
				}
			}
			else {
				debounce = false;
			}
			if (!plane.spring1Broken && !plane.spring1->isEnabled()) {
				Mix_PlayChannel(4, breakSound, 0);
				plane.spring1Broken = true;
			}
			if (!plane.spring2Broken && !plane.spring2->isEnabled()) {
				Mix_PlayChannel(5, breakSound, 0);
				plane.spring2Broken = true;
			}


			btVector3 up = (btVector3{ 0, 1, 0 } *basis);
			btVector3 forward = (btVector3{ 1, 0, 0 } * basis);
			ImGui::Text("vel %f %f %f", vel.x(), vel.y(), vel.z());
			if (vel.length() < 60 && vel.y() < -10) {
				if(!Mix_Playing(0))
					Mix_PlayChannel(0, chunk, -1);
			}
			else {
				Mix_HaltChannel(0);
			}

			ImGui::Text("up %f %f %f", up.x(), up.y(), up.z());
			ImGui::Text("forward %f %f %f", forward.x(), forward.y(), forward.z());
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::End();
		}
		plane.doPlanePhysics();
		container.dynamicsWorld->stepSimulation(deltaTime);
		for (auto &obj : objs) {
			obj.updateState(meshes[obj.meshIndice]);
		}
		btVector3 t = plane.main.rigid->getCenterOfMassTransform().getOrigin();
		plane.main.rigid->getWorldTransform().inverse().getOpenGLMatrix((float*)&state.planeState);
		state.pos = { t.x(), t.y(), t.z() };
		// Rendering
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
			if(vre)
				renderVR(ctx, vrCtx);
			FramePresent(ctx, wd);
		}
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