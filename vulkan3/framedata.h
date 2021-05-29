#pragma once

#include "pch.h"
#include "defaultvertex.h"
#include "vertexbuffer.h"
#include "imgui_imple.h"
#include "defaultrenderpass.h"
#include "defaultpipeline.h"
#include "shadowrenderpass.h"
#include "shadowpipeline.h"
#include "vkimage.h"
struct SceneData {
    const VkCtx& _ctx;
    Renderpass pass;
    ShadowRenderpass shadowPass;
    ShadowPipeline shadowPipeline;
    Image depthImage;
    VkImageView depthView;
    VkSampler shadowSampler;
    Image shadowImage;
    VkImageView shadowView;
    VkFramebuffer shadowFramebuffer;


    std::vector<VkDescriptorSet> descriptorSets;
    std::vector<VkDescriptorSet> shadowSets;
    std::vector<DynamicUniformBuffer> uniformBuffers;
    std::vector<VkFramebuffer> frameBuffers;

    SceneData(const VkCtx& ctx, int width, int height, int nFrames, ImGui_ImplVulkanH_Window *wd) : _ctx(ctx),
        pass(_ctx),
        depthImage(ctx, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT),
        shadowImage(ctx, 2048, 2048, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT| VK_IMAGE_USAGE_SAMPLED_BIT),
        shadowPass(_ctx),
        shadowPipeline(_ctx, shadowPass.renderPass()) {


        depthView = depthImage.makeImageView(VK_IMAGE_ASPECT_DEPTH_BIT);
        shadowView = shadowImage.makeImageView(VK_IMAGE_ASPECT_DEPTH_BIT);
        VkSamplerCreateInfo sampler = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        sampler.magFilter = VK_FILTER_LINEAR;
        sampler.minFilter = VK_FILTER_LINEAR;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler.addressModeV = sampler.addressModeU;
        sampler.addressModeW = sampler.addressModeU;
        sampler.mipLodBias = 0.0f;
        sampler.maxAnisotropy = 1.0f;
        sampler.minLod = 0.0f;
        sampler.maxLod = 1.0f;
        sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        vkCreateSampler(ctx.device(), &sampler, nullptr, &shadowSampler);


        std::array<VkImageView, 1> attachments = {
            shadowView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = shadowPass.renderPass();
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = 2048;
        framebufferInfo.height = 2048;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(ctx.device(), &framebufferInfo, nullptr, &shadowFramebuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }



        uniformBuffers.reserve(nFrames);
        for (int i = 0; i < nFrames; i++) {
            uniformBuffers.push_back(DynamicUniformBuffer(ctx, 128));
            uniformBuffers[i].map();
        }

        frameBuffers.resize(nFrames);
        for (int i = 0; i < nFrames; i++) {
            std::array<VkImageView, 2> attachments = {
                wd->Frames[i].BackbufferView,
                depthView
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = pass.renderPass();
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = width;
            framebufferInfo.height = height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(ctx.device(), &framebufferInfo, nullptr, &frameBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void createDescriptors(DefaultLayout& layout, int nFrames, VkDescriptorPool pool) {
        std::vector<VkDescriptorSetLayout> layouts(nFrames, layout.descriptorSetLayout());
        std::vector<VkDescriptorSetLayout> shadowLayouts(nFrames, shadowPipeline.descriptorSetLayout());

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(nFrames);
        allocInfo.pSetLayouts = layouts.data();

        descriptorSets.resize(nFrames);
        if (vkAllocateDescriptorSets(_ctx.device(), &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        allocInfo.descriptorPool = pool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(nFrames);
        allocInfo.pSetLayouts = shadowLayouts.data();
        shadowSets.resize(nFrames);
        if (vkAllocateDescriptorSets(_ctx.device(), &allocInfo, shadowSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }
        for (int i = 0; i < nFrames; i++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i].buffer();
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            imageInfo.imageView = shadowView;
            imageInfo.sampler = shadowSampler;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr; // Optional
            descriptorWrite.pTexelBufferView = nullptr; // Optional

            VkWriteDescriptorSet samplerWrite{};
            samplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            samplerWrite.dstSet = descriptorSets[i];
            samplerWrite.dstBinding = 1;
            samplerWrite.dstArrayElement = 0;
            samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            samplerWrite.descriptorCount = 1;
            samplerWrite.pImageInfo = &imageInfo;
            VkWriteDescriptorSet writes[] = { samplerWrite, descriptorWrite };
            vkUpdateDescriptorSets(_ctx.device(), 2, writes, 0, nullptr);
            descriptorWrite.dstSet = shadowSets[i];
            vkUpdateDescriptorSets(_ctx.device(), 1, &descriptorWrite, 0, nullptr);
        }
    }

    void recreate(ImGui_ImplVulkanH_Window* wd) {
        pass.recreate();
        depthImage.destroy();
        vkDestroyImageView(_ctx.device(), depthView, allocCallback);
        for (int i = 0; i < frameBuffers.size(); i++) {
            vkDestroyFramebuffer(_ctx.device(), frameBuffers[i], allocCallback);
        }

        new (&depthImage) Image(_ctx, wd->Width, wd->Height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        depthView = depthImage.makeImageView(VK_IMAGE_ASPECT_DEPTH_BIT);
        for (int i = 0; i < wd->ImageCount; i++) {
            std::array<VkImageView, 2> attachments = {
                wd->Frames[i].BackbufferView,
                depthView
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = pass.renderPass();
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = wd->Width;
            framebufferInfo.height = wd->Height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(_ctx.device(), &framebufferInfo, nullptr, &frameBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }

    }

    void destroy() {
        for (int i = 0; i < uniformBuffers.size(); i++) {
            vkDestroyFramebuffer(_ctx.device(), frameBuffers[i], allocCallback);
        }
        vkDestroySampler(_ctx.device(), shadowSampler, allocCallback);
        vkDestroyImageView(_ctx.device(), depthView, allocCallback);
        vkDestroyImageView(_ctx.device(), shadowView, allocCallback);
        depthImage.destroy();
        shadowImage.destroy();
        shadowPipeline.destroy();
        pass.destroy();
        shadowPass.destroy();
    }
};
