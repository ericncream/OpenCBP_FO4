#include "Thing.h"
#include "log.h"
#include "f4se\NiNodes.h"
#include <time.h>

constexpr auto DEG_TO_RAD = 3.14159265 / 180;

#define DEBUG 0
#define TRANSFORM_DEBUG 0

std::unordered_map<const char*, NiPoint3> origLocalPos;
std::unordered_map<const char*, NiMatrix43> origLocalRot;
const char * skeletonNif_boneName = "skeleton.nif";
const char* COM_boneName = "COM";

Thing::Thing(NiAVObject* obj, BSFixedString& name)
    : boneName(name)
    , velocity(NiPoint3(0, 0, 0))
{
    oldWorldPos = obj->m_worldTransform.pos;

    time = clock();
}

Thing::~Thing() {
}

void showPos(NiPoint3 &p) {
    logger.info("%8.4f %8.4f %8.4f\n", p.x, p.y, p.z);
}

void showRot(NiMatrix43 &r) {
    logger.info("%8.4f %8.4f %8.4f %8.4f\n", r.data[0][0], r.data[0][1], r.data[0][2], r.data[0][3]);
    logger.info("%8.4f %8.4f %8.4f %8.4f\n", r.data[1][0], r.data[1][1], r.data[1][2], r.data[1][3]);
    logger.info("%8.4f %8.4f %8.4f %8.4f\n", r.data[2][0], r.data[2][1], r.data[2][2], r.data[2][3]);
}


float solveQuad(float a, float b, float c) {
    float k1 = (-b + sqrtf(b*b - 4*a*c)) / (2 * a);
    //float k2 = (-b - sqrtf(b*b - 4*a*c)) / (2 * a);
    //logger.error("k2 = %f\n", k2);
    return k1;
}




void Thing::updateConfig(configEntry_t & centry) {
    stiffness = centry["stiffness"];
    stiffness2 = centry["stiffness2"];
    damping = centry["damping"];
    maxOffsetX = centry["maxoffsetX"];
    maxOffsetY = centry["maxoffsetY"];
    maxOffsetZ = centry["maxoffsetZ"];
    timeTick = centry["timetick"];
    linearX = centry["linearX"];
    linearY = centry["linearY"];
    linearZ = centry["linearZ"];
    rotationalX = centry["rotationalX"];
    rotationalY = centry["rotationalY"];
    rotationalZ = centry["rotationalZ"];
    rotationX = centry["rotationX"];
    rotationY = centry["rotationY"];
    rotationZ = centry["rotationZ"];
    // Optional entries for backwards compatability 
    if (centry.find("timeStep") != centry.end())
        timeStep = centry["timeStep"];
    else 
        timeStep = 1.0f;
    gravityBias = centry["gravityBias"];
    gravityCorrection = centry["gravityCorrection"];
    cogOffsetY = centry["cogOffsetY"];
    cogOffsetX = centry["cogOffsetX"];
    cogOffsetZ = centry["cogOffsetZ"];
    fusionGirlEnabled = centry["FG"] == 1.0;
    if (timeTick <= 1)
        timeTick = 1;

    //zOffset = solveQuad(stiffness2, stiffness, -gravityBias);

    //logger.error("z offset = %f\n", solveQuad(stiffness2, stiffness, -gravityBias));
}

void Thing::dump() {
    //showPos(obj->m_worldTransform.pos);
    //showPos(obj->m_localTransform.pos);
}


static float clamp(float val, float min, float max) {
    if (val < min) return min;
    else if (val > max) return max;
    return val;
}

void Thing::reset() {

}

// Returns 
template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

void Thing::update(Actor *actor) {
    auto newTime = clock();
    auto deltaT = newTime - time;

    time = newTime;
    if (deltaT > 64) deltaT = 64;
    if (deltaT < 8) deltaT = 8;
    
    auto loadedState = actor->unkF0;
    if (!loadedState || !loadedState->rootNode) {
        logger.error("No loaded state for actor %08x\n", actor->formID);
        return;
    }
    auto obj = loadedState->rootNode->GetObjectByName(&boneName);

    if (!obj) {
        logger.error("Couldn't get name for loaded state for actor %08x\n", actor->formID);
        return;
    }

    if (!obj->m_parent) {
        logger.error("Couldn't get bone %s parent for actor %08x\n", boneName.c_str() , actor->formID);
        return;
    }

#if TRANSFORM_DEBUG
    auto sceneObj = obj;
    while (sceneObj->m_parent && sceneObj->m_name != "skeleton.nif")
    {
        logger.info(sceneObj->m_name);
        logger.info("\n---\n");
        logger.error("Actual m_localTransform.pos: ");
        showPos(sceneObj->m_localTransform.pos);
        logger.error("Actual m_worldTransform.pos: ");
        showPos(sceneObj->m_worldTransform.pos);
        logger.info("---\n");
        //logger.error("Actual m_localTransform.rot Matrix:\n");
        showRot(sceneObj->m_localTransform.rot);
        //logger.error("Actual m_worldTransform.rot Matrix:\n");
        showRot(sceneObj->m_worldTransform.rot);
        logger.info("---\n");
        //if (sceneObj->m_parent) {
        //	logger.error("Calculated m_worldTransform.pos: ");
        //	showPos((sceneObj->m_parent->m_worldTransform.rot.Transpose() * sceneObj->m_localTransform.pos) + sceneObj->m_parent->m_worldTransform.pos);
        //	logger.error("Calculated m_worldTransform.rot Matrix:\n");
        //	showRot(sceneObj->m_localTransform.rot * sceneObj->m_parent->m_worldTransform.rot);
        //}
        sceneObj = sceneObj->m_parent;	
    }
#endif

    // TODO rewrite these for performance
    // Save the bone's original local position if it already hasn't
    if (origLocalPos.find(boneName.c_str()) == origLocalPos.end()) {
        logger.error("for actor %08x, bone %s: ", actor->formID, boneName.c_str());
        logger.error("firstRun pos Set: ");
        origLocalPos.emplace(boneName.c_str(), obj->m_localTransform.pos);
        showPos(obj->m_localTransform.pos);
    }
    // Save the bone's original local rotation if it already hasn't
    if (origLocalRot.find(boneName.c_str()) == origLocalRot.end()) {
        logger.error("for actor %08x, bone %s: ", actor->formID, boneName.c_str());
        logger.error("firstRun rot Set:\n");
        origLocalRot.emplace(boneName.c_str(), obj->m_localTransform.rot);
        showRot(obj->m_localTransform.rot);
    }

    auto skeletonObj = obj;
    NiAVObject * comObj;
    bool skeletonFound = false;
    while (skeletonObj->m_parent)
    {
        if (skeletonObj->m_parent->m_name == BSFixedString(COM_boneName)) {
            comObj = skeletonObj->m_parent;
        }
        else if (skeletonObj->m_parent->m_name == BSFixedString(skeletonNif_boneName)) {
            skeletonObj = skeletonObj->m_parent;
            skeletonFound = true;
            break;
        }
        skeletonObj = skeletonObj->m_parent;
    }
    if (skeletonFound == false) {
        logger.error("Couldn't find skeleton for actor %08x\n", actor->formID);
        return;
    }
#if DEBUG
    logger.error("bone %s for actor %08x with parent %s\n", boneName.c_str(), actor->formID, skeletonObj->m_name.c_str());
    showRot(skeletonObj->m_worldTransform.rot);
    //showPos(obj->m_parent->m_worldTransform.rot.Transpose() * obj->m_localTransform.pos);
#endif
    NiMatrix43 targetRot;
    if (obj->m_name == BSFixedString("Breast_CBP_R_02") || obj->m_name == BSFixedString("Breast_CBP_L_02")) {
        targetRot = skeletonObj->m_localTransform.rot.Transpose();
    }
    else {
        //targetRot = skeletonObj->m_localTransform.rot.Transpose() *
        //			comObj->m_localTransform.rot *
        //			skeletonObj->m_localTransform.rot *
        //			obj->m_parent->m_worldTransform.rot.Transpose();
        targetRot = skeletonObj->m_localTransform.rot.Transpose();
    }
    NiPoint3 origWorldPos = (obj->m_parent->m_worldTransform.rot.Transpose() * origLocalPos.at(boneName.c_str())) +  obj->m_parent->m_worldTransform.pos;

    // Offset to move Center of Mass make rotational motion more significant
    NiPoint3 target = (targetRot * NiPoint3(cogOffsetX, cogOffsetY, cogOffsetZ)) + origWorldPos;

#if DEBUG
    logger.error("World Position: ");
    showPos(obj->m_worldTransform.pos);
    //logger.error("Parent World Position difference: ");
    //showPos(obj->m_worldTransform.pos - obj->m_parent->m_worldTransform.pos);
    logger.error("Target Rotation * cogOffsetY %8.4f: ", cogOffsetY);
    showPos(targetRot * NiPoint3(cogOffsetX, cogOffsetY, cogOffsetZ));
    //logger.error("Target Rotation:\n");
    //showRot(targetRot);
    logger.error("cogOffset x Transformation:");
    showPos(targetRot * NiPoint3(cogOffsetX, 0, 0));
    logger.error("cogOffset y Transformation:");
    showPos(targetRot * NiPoint3(0, cogOffsetY, 0));
    logger.error("cogOffset z Transformation:");
    showPos(targetRot * NiPoint3(0, 0, cogOffsetZ));
#endif

    // diff is Difference in position between old and new world position
    NiPoint3 diff = target - oldWorldPos;

    // Move up in for gravity correction
    diff += targetRot * NiPoint3(0, 0, gravityCorrection);

#if DEBUG
    logger.error("Diff after gravity correction %f: ", gravityCorrection);
    showPos(diff);
#endif

    if (fabs(diff.x) > 100 || fabs(diff.y) > 100 || fabs(diff.z) > 100) {
        logger.error("transform reset\n");
        obj->m_localTransform.pos = origLocalPos.at(boneName.c_str());
        oldWorldPos = target;
        velocity = NiPoint3(0, 0, 0);
        time = clock();
    } else {

        diff *= timeTick / (float)deltaT;
        NiPoint3 posDelta = NiPoint3(0, 0, 0);

        // Compute the "Spring" Force
        NiPoint3 diff2(diff.x * diff.x * sgn(diff.x), diff.y * diff.y * sgn(diff.y), diff.z * diff.z * sgn(diff.z));
        NiPoint3 force = (diff * stiffness) + (diff2 * stiffness2) - (targetRot * NiPoint3(0, 0, gravityBias));

#if DEBUG
        logger.error("Diff2: ");
        showPos(diff2);
        logger.error("Force with stiffness %f, stiffness2 %f, gravity bias %f: ", stiffness, stiffness2, gravityBias);
        showPos(force);
#endif

        do {
            // Assume mass is 1, so Accelleration is Force, can vary mass by changinf force
            //velocity = (velocity + (force * timeStep)) * (1 - (damping * timeStep));
            velocity = (velocity + (force * timeStep)) - (velocity * (damping * timeStep));

            // New position accounting for time
            posDelta += (velocity * timeStep);
            deltaT -= timeTick;
        } while (deltaT >= timeTick);

    NiPoint3 newPos = oldWorldPos +posDelta;

        oldWorldPos = diff + target;

#if DEBUG
        //logger.error("posDelta: ");
        //showPos(posDelta);
        logger.error("newPos: ");
        showPos(newPos);
#endif
        // clamp the difference to stop the breast severely lagging at low framerates
        diff = newPos - target;

        diff.x = clamp(diff.x, -maxOffsetX, maxOffsetX);
        diff.y = clamp(diff.y, -maxOffsetY, maxOffsetY);
        diff.z = clamp(diff.z - gravityCorrection, -maxOffsetZ, maxOffsetZ) + gravityCorrection;

        //oldWorldPos = target + diff;

#if DEBUG
        logger.error("diff from newPos: ");
        showPos(diff);
        //logger.error("oldWorldPos: ");
        //showPos(oldWorldPos);
#endif

        // move the bones based on the supplied weightings
        // Convert the world translations into local coordinates

        NiMatrix43 invRot;

        if (obj->m_name == BSFixedString("Breast_CBP_R_02") || obj->m_name == BSFixedString("Breast_CBP_L_02")) {
            NiMatrix43 standardRot;
            standardRot.SetEulerAngles(rotationX * DEG_TO_RAD,
                                       rotationY * DEG_TO_RAD,
                                       rotationZ * DEG_TO_RAD);
            invRot = standardRot * obj->m_parent->m_worldTransform.rot;
        }
        else {
            //invRot = obj->m_parent->m_worldTransform.rot * skeletonObj->m_localTransform.rot.Transpose() * comObj->m_localTransform.rot.Transpose();
            NiMatrix43 standardRot;
            standardRot.SetEulerAngles(rotationX * DEG_TO_RAD,
                                       rotationY * DEG_TO_RAD,
                                       rotationZ * DEG_TO_RAD);
            invRot = standardRot * obj->m_parent->m_worldTransform.rot;
        }

        auto localDiff = NiPoint3(diff.x * linearX,
                                  diff.y * linearY,
                                  diff.z * linearZ);
        auto rotDiff = localDiff;
        localDiff = invRot * localDiff;

        oldWorldPos = diff + target;
#if DEBUG
        logger.error("invRot x=10 Transformation:");
        showPos(invRot * NiPoint3(10, 0, 0));
        logger.error("invRot y=10 Transformation:");
        showPos(invRot * NiPoint3(0, 10, 0));
        logger.error("invRot z=10 Transformation:");
        showPos(invRot * NiPoint3(0, 0, 10));
        logger.error("oldWorldPos: ");
        showPos(oldWorldPos);
        logger.error("localTransform.pos: ");
        showPos(obj->m_localTransform.pos);
        logger.error("localDiff: ");
        showPos(localDiff);
        logger.error("rotDiff: ");
        showPos(rotDiff);

#endif
        // scale positions from config
        NiPoint3 newLocalPos = NiPoint3((localDiff.x) + origLocalPos.at(boneName.c_str()).x,
                                        (localDiff.y) + origLocalPos.at(boneName.c_str()).y,
                                        (localDiff.z) + origLocalPos.at(boneName.c_str()).z
        );
        obj->m_localTransform.pos = newLocalPos;
        
        rotDiff.x *= rotationalX;
        rotDiff.y *= rotationalY;
        rotDiff.z *= rotationalZ;

#if DEBUG
        logger.error("localTransform.pos after: ");
        showPos(obj->m_localTransform.pos);
        logger.error("origLocalPos:");
        showPos(origLocalPos.at(boneName.c_str()));
        logger.error("origLocalRot:");
        showRot(origLocalRot.at(boneName.c_str()));

#endif
        // Do rotation.
        NiMatrix43 standardRot;
        standardRot.SetEulerAngles(rotDiff.x, rotDiff.y, rotDiff.z);
        obj->m_localTransform.rot = standardRot * origLocalRot.at(boneName.c_str());
    }
#if DEBUG
    logger.error("end update()\n");
#endif



}