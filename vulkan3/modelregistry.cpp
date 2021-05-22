#include "modelregistry.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "glm/gtc/quaternion.hpp"
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include "vkctx.h"

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
	accessor.componentType = val["componentType"].GetInt();
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
	scene = doc["scene"].GetInt();

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
