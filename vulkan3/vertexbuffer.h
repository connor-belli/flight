#pragma once
#include "pch.h"
#include "vkctx.h"
#include "defaultvertex.h"


template<class T>
class PackedBuffer {
private:
    const VkCtx& _ctx;
    VkBuffer _buffer;
    VmaAllocation _alloc;
    VmaAllocationInfo _allocInfo;
    size_t _size;
public:
    PackedBuffer(const VkCtx& ctx, std::vector<T> vertices, VkBufferUsageFlags flags) : _ctx(ctx), _size(vertices.size()) {
        VkBufferCreateInfo bufferInfo{};

        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = sizeof(T) * vertices.size();
        bufferInfo.usage = flags;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo info{};
        info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        VkResult err = vmaCreateBuffer(ctx.allocator(), &bufferInfo, &info, &_buffer, &_alloc, &_allocInfo);
        check_vk_result(err);

        void* data;
        vmaMapMemory(ctx.allocator(), _alloc, &data);
        memcpy(data, vertices.data(), (size_t)bufferInfo.size);
        vmaUnmapMemory(ctx.allocator(), _alloc);
    }

    PackedBuffer(PackedBuffer&& o) : _ctx(o._ctx) {
        _size = o._size;
        _allocInfo = o._allocInfo;
        _alloc = o._alloc;
        _buffer = o._buffer;
        o._alloc = nullptr;
        o._buffer = VK_NULL_HANDLE;
    }

    ~PackedBuffer() {
        destroy();
    }

    size_t size() const {
        return _size;
    }

    //PackedBuffer(const PackedBuffer&) = default;

    VkBuffer buffer() const {
        return _buffer;
    }

    void destroy() {
        vmaDestroyBuffer(_ctx.allocator(), _buffer, _alloc);
    }
};

typedef PackedBuffer<Vertex> VertexBuffer;
typedef PackedBuffer<uint32_t> IndexBuffer;

class DynamicUniformBuffer {
    const VkCtx& _ctx;
    VkBuffer _uniform;
    VmaAllocation _alloc;
    VmaAllocationInfo _allocInfo;
    size_t _size;
    void* _data;
    static constexpr size_t alignment = 256;
public:
    DynamicUniformBuffer(const VkCtx& ctx, size_t size) : _ctx(ctx), _size(size), _data(nullptr) {
        //assert(size <= 65536 / alignment);
        VkBufferCreateInfo bufferInfo{};

        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size*alignment;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo info{};
        info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        VkResult err = vmaCreateBuffer(ctx.allocator(), &bufferInfo, &info, &_uniform, &_alloc, &_allocInfo);
        check_vk_result(err);
    }

    DynamicUniformBuffer(DynamicUniformBuffer&& o) : _ctx(o._ctx) {
        _alloc = o._alloc;
        _size = o._size;
        _data = o._data;
        _allocInfo = o._allocInfo;
        _uniform = o._uniform;

        o._uniform = VK_NULL_HANDLE;
        o._alloc = nullptr;
    }

    ~DynamicUniformBuffer() {
        destroy();
    }

    void destroy() {
        if(_alloc) unmap();
        vmaDestroyBuffer(_ctx.allocator(), _uniform, _alloc);
    }
    
    void copyFrom(const std::vector<UniformBufferObject>& uniforms) {
        assert(uniforms.size() <= _size);
        vmaMapMemory(_ctx.allocator(), _alloc, &_data);
        memcpy(_data, uniforms.data(), (size_t)alignment*_size);
        vmaUnmapMemory(_ctx.allocator(), _alloc);
        _data = nullptr;
    }

    void map() {
        vmaMapMemory(_ctx.allocator(), _alloc, &_data);
    }

    void unmap() {
        vmaUnmapMemory(_ctx.allocator(), _alloc);
        _data = nullptr;
    }


    void copyPersistent(const std::vector<UniformBufferObject>& uniforms) {
        assert(uniforms.size() <= _size);
        memcpy(_data, uniforms.data(), (size_t)alignment * _size);
    }

    void copyInd(size_t i, UniformBufferObject u) {
        memcpy((uint8_t*)_data + i * alignment, &u, (size_t)alignment);
    }

    VkBuffer buffer() {
        return _uniform;
    }
};