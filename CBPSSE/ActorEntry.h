#pragma once
#include "f4se/GameReferences.h"

class ActorEntry {
    public:
    UInt32 id;
    Actor* actor;

    bool isInPowerArmor();
    bool IsTorsoArmorEquipped();
};