#pragma once
#include "Voxel/Generation/Kernel/VoxelGenHabitat.h"
namespace VoxelRepair  {
    enum class ReadyStage:uint8_t  {
        Uninitialized,AssetsValidated,RecipeFrozen,ContentPlanReady,SpawnDataReady,SpawnCollisionReady,PrimaryViewReady,Playable,Failed
    };
    struct ReadinessSnapshot  {
        uint64_t epoch=0,expectedEpoch=0;
        bool failed=false,assetsValid=false,recipeFrozen=false,planCertified=false;
        uint64_t requiredContent=(RequiredContent & ~uint64_t(Content::Ocean)),contentWitnessed=0;
        uint32_t dataRequired=0,dataReady=0,collisionRequired=0,collisionReady=0;
        uint32_t viewRequired=0,viewReadyIncludingEmpty=0;
        bool needsCollision=true,needsView=true;
        uint32_t pendingCriticalDependencies=0;
    };
    struct ReadinessResult  {
        ReadyStage stage=ReadyStage::Uninitialized;
        bool playable=false;
        std::string reason;
    };
    WHFRAMEWORK_API ReadinessResult EvaluateReadiness(const ReadinessSnapshot&);
}
