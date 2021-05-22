#pragma once
#include "pch.h"
#include "model.h"
#include <optional>
#include <string>
#include <fstream>
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#define TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT 5123
#define TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT 5125
struct PipelineObjects {
	VkPipeline pipeline;
	VkPipelineLayout layout;
	std::vector<Mesh> meshs;
};

struct ModelRegistry {
	std::vector<PipelineObjects> pipelineObjects;
};

struct Node {
	uint32_t mesh;
	std::string name;
	std::vector<uint32_t> children;
	glm::mat4 trans;
};

struct Scene {
	std::string name;
	std::vector<uint32_t> nodes;
};

struct VertexAttributes {
	uint32_t position;
	std::optional<uint32_t> normal;
	std::optional<uint32_t> uv;
	std::optional<uint32_t> color;
};

struct Material {
	std::string name;
};

struct GLTFMesh {
	std::string name;
	VertexAttributes attributes;
	uint32_t indices;
};

struct Accessor {
	uint32_t bufferView;
	uint32_t componentType;
	uint32_t componentCount;
	uint32_t count;
};

struct BufferView {
	uint32_t buffer;
	uint32_t byteLength;
	uint32_t byteOffset;
};

struct Buffer {
	uint32_t byteLength;
	std::string uri;
	std::vector<char> data;
};

struct GLTFRoot {
	uint32_t scene;
	std::vector<Scene> scenes;
	std::vector<Node> nodes;
	std::vector<GLTFMesh> meshes;
	std::vector<Accessor> accessors;
	std::vector<BufferView> bufferViews;
	std::vector<Buffer> buffers;
	void parseDoc(rapidjson::Document& doc);
	void loadBuffs();
};