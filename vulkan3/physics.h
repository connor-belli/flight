#pragma once
#include <bullet/btBulletDynamicsCommon.h>
#include <bullet/BulletDynamics/Vehicle/btRaycastVehicle.h>
#include "model.h"
#include <iostream>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include "bullet/BulletDynamics/Featherstone/btMultiBodyDynamicsWorld.h"
#include "bullet/BulletDynamics/Featherstone/btMultiBodyConstraintSolver.h"
#include "bullet/BulletDynamics/Featherstone/btMultiBodyPoint2Point.h"
#include "bullet/BulletDynamics/Featherstone/btMultiBodyLinkCollider.h"
#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "modelregistry.h"
#include <SDL2/SDL_mixer.h>
struct PhysicsContainer {
	btDynamicsWorld* dynamicsWorld;
};
static btCollisionShape* sphere = new btBoxShape({ 9.0, 3.5, 11.0 });

struct GroundShape {
	btBvhTriangleMeshShape* groundShape;
	btRigidBody* rigidBody;
	btTriangleIndexVertexArray* meshInterface;
	btMotionState* state;

	GroundShape(PhysicsContainer& container, const GLTFRoot& model, const GLTFMesh& mesh) {
		auto indicesAcc = model.accessors[mesh.indices];
		auto positionAcc = model.accessors[mesh.attributes.position];
		auto indicesBufferView = model.bufferViews[indicesAcc.bufferView];
		auto positionBufferView = model.bufferViews[positionAcc.bufferView];
		btIndexedMesh mIndexedMesh;
		bool isShort = indicesAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
		mIndexedMesh.m_indexType = isShort?PHY_SHORT:PHY_INTEGER;
		mIndexedMesh.m_numTriangles = indicesAcc.count / 3;
		mIndexedMesh.m_triangleIndexBase = (const unsigned char*)model.buffers[indicesBufferView.buffer].data.data() + indicesBufferView.byteOffset;
		mIndexedMesh.m_triangleIndexStride = (isShort?sizeof(uint16_t):sizeof(uint32_t)) * 3;
		mIndexedMesh.m_numVertices = positionAcc.count;
		mIndexedMesh.m_vertexBase = (const unsigned char*)model.buffers[positionBufferView.buffer].data.data() + positionBufferView.byteOffset;
		mIndexedMesh.m_vertexStride = 3 * sizeof(float);
		meshInterface = new btTriangleIndexVertexArray();
		meshInterface->addIndexedMesh(mIndexedMesh, mIndexedMesh.m_indexType);
		meshInterface->setScaling(btVector3{ 9999.9912109375,
					100,
					9999.9912109375 });
		groundShape = new btBvhTriangleMeshShape(meshInterface, false);
		btTransform trans;
		trans.setIdentity();
		trans.setOrigin({ 0, -100.003, 0 });
		state = new btDefaultMotionState(trans);
		rigidBody = new btRigidBody(0.0f, state, groundShape);
		container.dynamicsWorld->addRigidBody(rigidBody);
	}

	~GroundShape() {
		delete meshInterface;
		delete rigidBody;
		delete state;
		delete groundShape;
	}
};

struct PhysicsObj {

	int state;
	int meshIndice;
	btRigidBody* rigid;
	btVector3 inertia;

	PhysicsObj() = default;

	PhysicsObj(Mesh& mesh, PhysicsContainer& container, glm::vec3 pos, int meshIndex, btCollisionShape* shape, double mass, int group = 3) : state(mesh.instances.size()), meshIndice(meshIndex) {
		MeshInstanceState state;
		mesh.instances.push_back(state);
		shape->calculateLocalInertia(mass, inertia);
		btTransform globalTransform;
		globalTransform.setIdentity();
		globalTransform.setOrigin(btVector3{ pos[0], pos[1], pos[2]});
		//globalTransform.setRotation(btQuaternion(3.141592, 0, 0));

		rigid = new btRigidBody(mass, new btDefaultMotionState(globalTransform), shape, inertia);

		container.dynamicsWorld->addRigidBody(rigid, group, 3);
	}
	PhysicsObj(const PhysicsObj&) = default;
	

	void updateState(Mesh& mesh) {
		btTransform trans = rigid->getWorldTransform();
		btQuaternion qt = trans.getRotation();
		btVector3 l = trans.getOrigin();
		glm::vec3 pos = { l.x(), l.y(), l.z() };
		glm::quat q(qt.w(), qt.x(), qt.y(), qt.z());
		mesh.instances[state].pose = glm::translate(glm::mat4(1.0f), pos) * glm::toMat4(q);
	}

};


static Mix_Chunk* chunk;
static Mix_Chunk* groundHit;
static Mix_Chunk* tooLowGear;
static Mix_Chunk* breakSound;
static Mix_Chunk* engine;

static void initAudio() {
	Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096);
	groundHit = Mix_LoadWAV("GroundProximity.wav");
	chunk = Mix_LoadWAV("LOWALT.wav");
	tooLowGear = Mix_LoadWAV("TooLowGear.wav");
	breakSound = Mix_LoadWAV("break.wav");
	engine = Mix_LoadWAV("engine.wav");
}

struct Plane {
	PhysicsObj main;
	PhysicsObj leftGear;
	PhysicsObj rightGear;
	PhysicsObj centerGear;
	btGeneric6DofConstraint* spring1;
	btGeneric6DofConstraint* spring2;
	bool spring1Broken, spring2Broken;
	float throttle = 0;
	int space = 0;
	int left = 0;
	int roll = 0;
	int yaw = 0;

	float aoa;
	btVector3 vel;
	btVector3 localVel;
	btMatrix3x3 basis;
	

	Plane(std::vector<Mesh> &meshes, PhysicsContainer& container, glm::vec3 pos) :spring1Broken(false), spring2Broken(false) {
		btCompoundShape* mainShape = new btCompoundShape();
		btTransform trans;
		trans.setIdentity();
		trans.setOrigin({ 0, -0.5, 0 });
		mainShape->addChildShape(trans, new btBoxShape({ 7, 1, 1 }));
		trans.setOrigin({ 2, -0.5, 6 });
		mainShape->addChildShape(trans, new btBoxShape({ 1.15, 0.14, 5 }));
		trans.setOrigin({ 2, -0.5, -6 });
		mainShape->addChildShape(trans, new btBoxShape({ 1.15, 0.14, 5 }));
		main = std::move(PhysicsObj(meshes[1], container, pos, 1, mainShape, 1.0f, 1));
		btSphereShape* gearShape1 = new btSphereShape(0.5);
		btSphereShape* gearShape2 = new btSphereShape(0.5);
		leftGear = std::move(PhysicsObj(meshes[4], container, pos + glm::vec3{3, -2.5, 8}, 4, gearShape1, 0.01f, 1));
		rightGear = std::move(PhysicsObj(meshes[4], container, pos + glm::vec3{ 3, -2.5, -8 }, 4, gearShape2, 0.01f, 1));
		btTransform transA = btTransform::getIdentity();
		transA.setIdentity();
		transA.setOrigin(btVector3{ 3, -2.5, 8 });
		transA.getBasis().setEulerZYX(0, 0, -3.141592 / 2);
		btTransform transB = btTransform::getIdentity();;
		transB.setIdentity();
		transB.getBasis().setEulerZYX(0, 0, -3.141592 / 2);

		btGeneric6DofSpringConstraint* spring = new btGeneric6DofSpringConstraint(*main.rigid, *leftGear.rigid, transA, transB, true);
		spring->enableSpring(1, true);
		spring->setBreakingImpulseThreshold(1.75);
		spring->setStiffness(1, 35.0f);
		spring->setDamping(1, 0.5f);
		spring->setEquilibriumPoint();
		spring->setAngularLowerLimit({0.0f, 0.0f, 0.0f});
		spring->setAngularUpperLimit({ 0.0f, 0.0f, 0.0f });
		spring1 = spring;
		container.dynamicsWorld->addConstraint(spring);
		transA.setOrigin(btVector3{ 3, -2.5, -8 });
		spring = new btGeneric6DofSpringConstraint(*main.rigid, *rightGear.rigid, transA, transB, true);
		spring->setBreakingImpulseThreshold(1.75);
		btGeneric6DofSpringConstraint::btConstraintInfo1 info;
		spring->enableSpring(1, true);
		spring->setStiffness(1, 80.0f);
		spring->setDamping(1, 0.5f);
		spring->setEquilibriumPoint();
		spring->setAngularLowerLimit({ 0.0f, 0.0f, 0.0f });
		spring->setAngularUpperLimit({ 0.0f, 0.0f, 0.0f });
		spring2 = spring;
		container.dynamicsWorld->addConstraint(spring);
	}

	void doPlanePhysics() {

		float coeffDrag = 0.1;
		float coeffDrag2 = 0.01;
		float coeffL = 0.1;
		main.rigid->activate();
		main.rigid->clearForces();


		auto basis = main.rigid->getWorldTransform().getBasis().inverse();
		float s = main.rigid->getLinearVelocity().length();
		btVector3 vel = main.rigid->getLinearVelocity();

		if (s > 0.1) {
			applyWingPhysics(btVector3{ 0, 0, 1 }, { -2, 0, 0 }, 24, 2.5, 0.0005, 0);
			applyWingPhysics(btVector3{ 0, 0, 1 }, { -5, 0, 0 }, 8, 2.5, 0.0005, space * 0.17);
			applyFinPhysics(btVector3{ 0, 1, 0 }, { -5, 0, 0 }, 4, 2.5, 0.0005, yaw * 0.17);
		}

		main.rigid->setDamping(0.01, 0.01);
		main.rigid->applyForce(btVector3{ 0.0, (float)10 * roll, 0 } * basis, btVector3{ 0, 0, 5 }*basis);

		glm::vec3 dir = glm::vec3(10.0, 0.0, 0);
		main.rigid->applyForce(throttle * btVector3{ dir.x, dir.y, dir.z } * basis, btVector3{5, 0, 0}*basis);


	}
	void applyWingPhysics(btVector3 perp, btVector3 loc, float wingSpan, float wingWidth, float coeff, float wingAngle) {
		btVector3 vel = main.rigid->getLinearVelocity();
		auto basis = main.rigid->getWorldTransform().getBasis().inverse();
		btVector3 localVel = vel * basis.inverse();
		float s = main.rigid->getLinearVelocity().length();
		float aoa = atan2(-localVel.y(), localVel.x()) - wingAngle;
		float wingArea = wingSpan * wingWidth;
		float aspectRatio = wingSpan * wingSpan / wingArea;
		float lift = aoa*(aspectRatio / (aspectRatio + 2.0f)) * 2.0f * 3.141592f;
		float drag = (lift * lift) / (aspectRatio * 3.141592f);
		float pressure = vel.length2() * 1.2754f * 0.5f * wingArea;
		lift = lift * pressure;
		drag = (0.021f + drag) * pressure;
		btVector3 va = -vel.cross(perp*basis) / s;
		btVector3 vb = vel.normalize();
		main.rigid->applyForce(coeff * (va * lift - vb * drag), loc*basis);
	}

	void applyFinPhysics(btVector3 perp, btVector3 loc, float wingSpan, float wingWidth, float coeff, float wingAngle) {
		btVector3 vel = main.rigid->getLinearVelocity() - loc.cross(main.rigid->getAngularVelocity());
		auto basis = main.rigid->getWorldTransform().getBasis().inverse();
		btVector3 localVel = vel * basis.inverse();
		float s = main.rigid->getLinearVelocity().length();
		float aoa = atan2(-localVel.z(), localVel.x()) - wingAngle;
		float wingArea = wingSpan * wingWidth;
		float aspectRatio = wingSpan * wingSpan / wingArea;
		float lift = aoa * (aspectRatio / (aspectRatio + 2.0f)) * 2.0f * 3.141592;
		float drag = (lift * lift) / (aspectRatio * 3.141592f);
		float pressure = vel.length2() * 1.2754f * 0.5f * wingArea;
		lift = lift * pressure;
		drag = (0.021f + drag) * pressure;
		btVector3 va = vel.cross(perp * basis) / s;
		btVector3 vb = vel.normalize();
		main.rigid->applyForce(coeff * (va * lift - vb * drag), loc*basis);
	}

	void calculatePlaneStats() {
		basis = main.rigid->getWorldTransform().getBasis().inverse();
		vel = main.rigid->getLinearVelocity();
		localVel = vel * basis.inverse();
		aoa = atan2(localVel.y(), localVel.x());
	}

	void doPlaneSounds(PhysicsContainer& container) {
		if (vel.length() > 40) {
			btVector3 pos = main.rigid->getWorldTransform().getOrigin();
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
		if (throttle > 0.1) {
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
				btVector3 pos = main.rigid->getWorldTransform().getOrigin();
				btVector3 to = pos + btVector3{ 0, -10, 0 }*basis;
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
		if (!spring1Broken && !spring1->isEnabled()) {
			Mix_PlayChannel(4, breakSound, 0);
			spring1Broken = true;
		}
		if (!spring2Broken && !spring2->isEnabled()) {
			Mix_PlayChannel(5, breakSound, 0);
			spring2Broken = true;
		}
		if (vel.length() < 60 && vel.y() < -10) {
			if (!Mix_Playing(0))
				Mix_PlayChannel(0, chunk, -1);
		}
		else {
			Mix_HaltChannel(0);
		}
	}
};

PhysicsContainer createPhysicsContainer() {
	btCollisionConfiguration* config = new btDefaultCollisionConfiguration;
	btCollisionDispatcher* dispatcher = new btCollisionDispatcher(config);
	btDbvtBroadphase* broadPhase = new btDbvtBroadphase(new btHashedOverlappingPairCache);
	btConstraintSolver* solver = new btSequentialImpulseConstraintSolver;
	btDynamicsWorld* dynamicsWorld = new btMultiBodyDynamicsWorld(dispatcher, broadPhase, new btMultiBodyConstraintSolver, config);
	dynamicsWorld->setGravity(btVector3(0, -9.81, 0));
	return { dynamicsWorld };
}