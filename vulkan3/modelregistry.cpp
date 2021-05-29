#include "modelregistry.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "glm/gtc/quaternion.hpp"
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include "vkctx.h"
#include <unordered_map>
#undef GetObject

void parseNode(Node& node, rapidjson::Value& val) {
	if (val.IsObject()) {
		node.name = val["name"].GetString();
		node.mesh = val["mesh"].GetInt();
		if (val.HasMember("children")) {
			auto arr = val["children"].GetArray();
			node.children.resize(arr.Size());
			for (int i = 0; i < arr.Size(); i++) {
				node.children[i] = arr[i].GetInt();
			}
		}
		glm::vec3 translation;
		if (val.HasMember("translation")) {
			auto t = val["translation"].GetArray();
			for (int i = 0; i < t.Size(); i++) {
				translation[i] = t[i].GetFloat();
			}
		}
		glm::vec3 scale(1.0f);
		if (val.HasMember("scale")) {
			auto t = val["scale"].GetArray();
			for (int i = 0; i < t.Size(); i++) {
				scale[i] = t[i].GetFloat();
			}
		}
		glm::quat rotation(1.0, 0.0, 0.0, 0.0f);
		if (val.HasMember("rotation")) {
			auto t = val["rotation"].GetArray();
			rotation = glm::quat(t[3].GetFloat(), t[0].GetFloat(), t[1].GetFloat(), t[2].GetFloat());
		}

		glm::mat4 rotMat = glm::toMat4(rotation);
		node.trans = glm::translate(glm::mat4(1.0f), translation) * rotMat * glm::scale(glm::mat4(1.0f), scale);
	}
	else {
		std::cout << "Node is not object" << std::endl;
	}
}

void parseMesh(GLTFMesh& mesh, rapidjson::Value& val) {
	mesh.name = val["name"].GetString();	
	mesh.indices = val["primitives"].GetArray()[0]["indices"].GetInt();
	if (val.HasMember("extensions") && val["extensions"].HasMember("EXT_flight_extension")) {
		auto ref = val["extensions"]["EXT_flight_extension"].GetObject();
		mesh.shaderIndex = ref["shader"]["shader_index"].GetInt();
	}
	auto& attrs = val["primitives"].GetArray()[0]["attributes"];
	if (attrs.HasMember("POSITION")) {
		mesh.attributes.position = attrs["POSITION"].GetInt();
	}
	if (attrs.HasMember("NORMAL")) {
		mesh.attributes.normal = attrs["NORMAL"].GetInt();
	}
	if (attrs.HasMember("COLOR_0")) {
		mesh.attributes.color = attrs["COLOR_0"].GetInt();
	}
	if (attrs.HasMember("TEXCOORD_0")) {
		mesh.attributes.uv = attrs["TEXCOORD_0"].GetInt();
	}
}

void parseAccessor(Accessor& accessor, rapidjson::Value& val) {
	accessor.bufferView = val["bufferView"].GetInt();
	accessor.componentType = static_cast<Component>(val["componentType"].GetInt());
	accessor.count = val["count"].GetInt();
	std::string count = val["type"].GetString();
	if (count == "VEC3") {
		accessor.componentCount = 3;
	}
	else if (count == "VEC4") {
		accessor.componentCount = 4;
	}
	else if (count == "VEC2") {
		accessor.componentCount = 2;
	}
	else if (count == "SCALAR") {
		accessor.componentCount = 1;
	}
	else if (count == "MAT4") {
		accessor.componentCount = 16;
	}
	else if (count == "MAT3") {
		accessor.componentCount = 9;
	}
	else {
		accessor.componentCount = 4;
	}
}

void parseScene(Scene& scene, rapidjson::Value& val) {
	scene.name = val["name"].GetString();
	if (val.HasMember("nodes")) {
		auto arr = val["nodes"].GetArray();
		scene.nodes.resize(arr.Size());
		for (int i = 0; i < arr.Size(); i++) {
			scene.nodes[i] = arr[i].GetInt();
		}
	}
	if (val.HasMember("extensions") && val["extensions"].HasMember("EXT_flight_extension")) {
		auto& ref = val["extensions"]["EXT_flight_extension"];
		for (auto& v : ref["shader_list"].GetArray()) {
			ShaderRef ref;
			ref.vertPath = v["vertex_shader_path"].GetString();
			ref.fragPath = v["fragment_shader_path"].GetString();
			ref.name = v["name"].GetString();
			scene.shaders.push_back(ref);
		}
	}
}

void parseBufferView(BufferView& bufferView, rapidjson::Value& val) {
	bufferView.buffer = val["buffer"].GetInt();
	bufferView.byteLength = val["byteLength"].GetInt();
	bufferView.byteOffset = val["byteOffset"].GetInt();
}

void parseBuffer(Buffer& buffer, rapidjson::Value& val) {
	buffer.byteLength = val["byteLength"].GetInt();
	buffer.uri = val["uri"].GetString();
}

void GLTFRoot::parseDoc(rapidjson::Document &doc) {
	auto& d = doc["scene"];
	scene = doc["scene"].GetInt();
	for (auto z = doc.MemberBegin(); z < doc.MemberEnd(); z++) {
		std::cout << z->name.GetString() << std::endl;
	}

	rapidjson::Value& scenesVal = doc["scenes"];
	if (scenesVal.IsArray()) {
		auto arr = scenesVal.GetArray();
		scenes.resize(arr.Size());
		for (auto i = arr.begin(); i < arr.end(); i++) {
			parseScene(scenes[i - arr.Begin()], *i);
		}
	}

	rapidjson::Value& nodesVal = doc["nodes"];
	if (nodesVal.IsArray()) {
		auto arr = nodesVal.GetArray();
		nodes.resize(arr.Size());
		for (auto i = arr.begin(); i < arr.end(); i++) {
			parseNode(nodes[i - arr.Begin()], *i);
		}
	}

	rapidjson::Value& meshesVal = doc["meshes"];
	if (meshesVal.IsArray()) {
		auto arr = meshesVal.GetArray();
		meshes.resize(arr.Size());
		for (auto i = arr.begin(); i < arr.end(); i++) {
			parseMesh(meshes[i - arr.begin()], *i);
		}
	}

	rapidjson::Value& accessorVal = doc["accessors"];
	if (accessorVal.IsArray()) {
		auto arr = accessorVal.GetArray();
		accessors.resize(arr.Size());
		for (auto i = arr.begin(); i < arr.end(); i++) {
			parseAccessor(accessors[i - arr.begin()], *i);
		}
	}

	rapidjson::Value& bufferViewsVal = doc["bufferViews"];
	if (bufferViewsVal.IsArray()) {
		auto arr = bufferViewsVal.GetArray();
		bufferViews.resize(arr.Size());
		for (auto i = arr.begin(); i < arr.end(); i++) {
			parseBufferView(bufferViews[i - arr.begin()], *i);
		}
	}

	rapidjson::Value& buffersVal = doc["buffers"];
	if (buffersVal.IsArray()) {
		auto arr = buffersVal.GetArray();
		buffers.resize(arr.Size());
		for (auto i = arr.begin(); i < arr.end(); i++) {
			parseBuffer(buffers[i - arr.begin()], *i);
		}
	}
}

void GLTFRoot::loadBuffs()
{
	for (Buffer& buf : buffers) {
		buf.data = readFile(buf.uri);
	}
}

Mesh GLTFRoot::createModel(const VkCtx& ctx, const GLTFMesh& mesh) {
	auto indicesAcc = accessors[mesh.indices];
	auto normalAcc = accessors[mesh.attributes.normal.value_or(0)];
	bool hasColor = mesh.attributes.color.has_value();
	auto positionAcc = accessors[mesh.attributes.position];
	auto colorAcc = accessors[mesh.attributes.color.value_or(0)];

	//assert(indicesAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT);
	auto indicesBufferView = bufferViews[indicesAcc.bufferView];
	auto normalBufferView = bufferViews[normalAcc.bufferView];
	auto positionBufferView = bufferViews[positionAcc.bufferView];
	auto colorBufferView = bufferViews[colorAcc.bufferView];


	std::vector<Vertex> vertices(positionAcc.count);
	if (hasColor)
		for (int i = 0; i < positionAcc.count; i++) {
			Vertex& v = vertices[i];
			memcpy(&v.pos, buffers[positionBufferView.buffer].data.data() + positionBufferView.byteOffset + i * sizeof(float) * 3, sizeof(float) * 3);
			memcpy(&v.color, buffers[colorBufferView.buffer].data.data() + colorBufferView.byteOffset + i * sizeof(unsigned short) * 4, sizeof(unsigned short) * 3);
			memcpy(&v.normal, buffers[normalBufferView.buffer].data.data() + normalBufferView.byteOffset + i * sizeof(float) * 3, sizeof(float) * 3);
		}
	else {
		for (int i = 0; i < positionAcc.count; i++) {
			Vertex& v = vertices[i];
			memcpy(&v.pos, buffers[positionBufferView.buffer].data.data() + positionBufferView.byteOffset + i * sizeof(float) * 3, sizeof(float) * 3);
			v.color = { 65565, 65565, 65565 };
			memcpy(&v.normal, buffers[normalBufferView.buffer].data.data() + normalBufferView.byteOffset + i * sizeof(float) * 3, sizeof(float) * 3);
		}
	}

	std::vector<uint32_t> indices(indicesAcc.count);
	if (indicesAcc.componentType == Component::UNSIGNED_INT)
		memcpy(indices.data(), buffers[indicesBufferView.buffer].data.data() + indicesBufferView.byteOffset, indicesBufferView.byteLength);
	else {
		for (int i = 0; i < indices.size(); i++) {
			uint16_t* begin = (uint16_t*)(buffers[indicesBufferView.buffer].data.data() + indicesBufferView.byteOffset);
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

void GLTFRoot::processNodes(Node& node, glm::mat4 prevState, ModelRegistry& registry) {
	glm::vec3 pos(0.0f);
	glm::mat4 curState = node.trans;
	if (node.mesh != -1) {
		MeshInstanceState state;
		state.pose = curState;
		registry.getByRef(registry.refFromInd(node.mesh)).instances.push_back(state);
	}
	for (int i = 0; i < node.children.size(); i++) {
		processNodes(nodes[node.children[i]], curState, registry);
	}
}

void GLTFRoot::processRegistriesInternal(VkCtx& ctx, ModelRegistry& registry, VkRenderPass renderPass)
{
	std::unordered_map<std::string, VkShaderModule> shaders;
	for (auto shaderDesc : scenes[scene].shaders) {
		auto src = readFile(shaderDesc.fragPath);
		VkShaderModule m = createShaderModule(ctx, src);
		shaders[shaderDesc.fragPath] = m;
		src = readFile(shaderDesc.vertPath);
		m = createShaderModule(ctx, src);
		shaders[shaderDesc.vertPath] = m;
	}
	for (ShaderRef& refs : scenes[scene].shaders) {
		registry.pipelines.push_back(DefaultPipeline(ctx,
			renderPass,
			registry.layout,
			shaders[refs.vertPath],
			shaders[refs.fragPath]));
	}
	for (auto& pipeline : registry.pipelines) {
		PipelineObjects obj;
		obj.layout = registry.layout.layout();
		obj.pipeline = pipeline.pipeline();
		registry.pipelineObjects.push_back(std::move(obj));
	}
	registry.modelIndToRefMap.resize(meshes.size());
	int i = 0;
	for (auto& mesh : meshes) {
		auto& pipeline = registry.pipelineObjects[mesh.shaderIndex];
		registry.modelIndToRefMap[i] = { mesh.shaderIndex, pipeline.meshs.size() };
		registry.stringToRef[mesh.name] = registry.modelIndToRefMap[i];
		pipeline.meshs.push_back(std::move(createModel(ctx, mesh)));
		i++;
	}
}
