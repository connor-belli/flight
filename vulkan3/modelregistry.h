#pragma once
#include "pch.h"
#include "model.h"
#include <unordered_map>
#include <optional>
#include <string>
#include <fstream>
#include "defaultpipeline.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"

struct PipelineObjects {
	VkPipeline pipeline;
	VkPipelineLayout layout;
	std::vector<Mesh> meshs;
};

struct ModelRef {
	size_t pipelineInd;
	size_t modelInd;
};

struct ModelRegistry {
	DefaultLayout layout;
	std::vector<ModelRef> modelIndToRefMap;
	std::unordered_map<std::string, ModelRef> stringToRef;
	std::vector<DefaultPipeline> pipelines;
	std::vector<PipelineObjects> pipelineObjects;

	Mesh& getByRef(ModelRef ref) {
		return pipelineObjects[ref.pipelineInd].meshs[ref.modelInd];
	}

	ModelRef refFromInd(size_t i) {
		return modelIndToRefMap[i];
	}

	ModelRef refFromName(const std::string& name) {
		return stringToRef[name];
	}

	ModelRegistry(VkCtx& ctx) : layout(ctx) {}
};

enum struct Component {
	BYTE = 5120,
	UNSIGNED_BYTE = 5121,
	SHORT = 5122,
	UNSIGNED_SHORT = 5123,
	INT = 5124,
	UNSIGNED_INT = 5125,
	FLOAT = 5126,
};

struct ShaderRef {
	std::string vertPath;
	std::string fragPath;
	std::string name;
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
	std::vector<ShaderRef> shaders;
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
	uint32_t indices = 0;
	uint32_t shaderIndex = 0;
};

struct Accessor {
	uint32_t bufferView;
	Component componentType;
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
	Mesh createModel(const VkCtx& ctx, const GLTFMesh& mesh);
	void processNodes(Node& node, glm::mat4 prevState, ModelRegistry& registry);

	void processRegistriesInternal(VkCtx& ctx, ModelRegistry& registry, VkRenderPass renderPass);
};

