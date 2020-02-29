#pragma once
#include <string>
#include <time.h>
#include <vector>

#include "f4se/GameRTTI.h"
#include "f4se/GameReferences.h"
#include "f4se/NiNodes.h"
#include "f4se/BsGeometry.h"
#include "f4se/NiRTTI.h"

#include "config.h"

static NiPoint3 emptyPoint = NiPoint3(0, 0, 0);

class Collision
{

public:

	Collision(NiAVObject* node, std::vector<Sphere> colliderSpheres);
	~Collision();

	Actor* colliderActor;

	NiPoint3 lastColliderPosition = emptyPoint;
		
	bool Collision::IsItColliding(NiPoint3 &collisiondif, std::vector<Sphere> thingCollisionSpheres, std::vector<Sphere> collisionSpheres, bool maybe);
	
	NiPoint3 CheckCollision(bool &isItColliding, std::vector<Sphere> thingCollisionSpheres, float timeTick, long deltaT, float maxOffset, bool maybe);
	NiPoint3 CheckPelvisCollision(std::vector<Sphere> thingCollisionSpheres);

	std::vector<Sphere> collisionSpheres;
	NiAVObject* CollisionObject;
	std::string colliderNodeName;

};

static inline NiPoint3 GetPointFromPercentage(NiPoint3 lowWeight, NiPoint3 highWeight, float weight)
{
	return ((highWeight - lowWeight) * (weight / 100)) + lowWeight;
}

static inline float distance(NiPoint3 po1, NiPoint3 po2)
{
	/*LARGE_INTEGER startingTime, endingTime, elapsedMicroseconds;
	LARGE_INTEGER frequency;

	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&startingTime);
	LOG("distance Start");*/
	float x = po1.x - po2.x;
	float y = po1.y - po2.y;
	float z = po1.z - po2.z;
	float result = std::sqrt(x*x + y*y + z*z);
	/*QueryPerformanceCounter(&endingTime);
	elapsedMicroseconds.QuadPart = endingTime.QuadPart - startingTime.QuadPart;
	elapsedMicroseconds.QuadPart *= 1000000000LL;
	elapsedMicroseconds.QuadPart /= frequency.QuadPart;
	LOG("distance Update Time = %lld ns\n", elapsedMicroseconds.QuadPart);*/
	return result;
}
