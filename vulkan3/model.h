#pragma once
#include "pch.h"
#include "vertexbuffer.h"

struct Gamestate {
	glm::vec3 pos = glm::vec3(3.0f, 10.0f, -3.0f);
	double tx = 0, ty = 0;
	glm::mat4 planeState;
	float zoom = 1;
};

struct MeshInstanceState {
	glm::mat4 pose;
};

struct Mesh {
	VertexBuffer buffer;
	IndexBuffer indices;
	VkPipeline pipeline;
	float ambience = 0.75;
	float normMul = 5;
	float mixRatio = 0;
	glm::vec4 color = { 1, 1, 1, 1 };
	std::vector<MeshInstanceState> instances;
};