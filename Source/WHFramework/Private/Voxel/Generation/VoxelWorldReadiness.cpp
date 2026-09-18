#include "Voxel/Generation/VoxelWorldReadiness.h"
namespace VoxelRepair  {
    ReadinessResult EvaluateReadiness(const ReadinessSnapshot& s) {
        if(!s.epoch||s.epoch!=s.expectedEpoch)return  {
            ReadyStage::Failed,false,"Stale world readiness epoch"
        };
        if(s.failed)return  {
            ReadyStage::Failed,false,"World task or asset pipeline failed"
        };
        if(s.dataReady>s.dataRequired||s.collisionReady>s.collisionRequired||s.viewReadyIncludingEmpty>s.viewRequired) return  {
            ReadyStage::Failed,false,"Readiness counters belong to different epochs or requests"
        };
        if(!s.assetsValid)return  {
            ReadyStage::Uninitialized,false,"Validating complete assets"
        };
        if(!s.recipeFrozen)return  {
            ReadyStage::AssetsValidated,false,"Freezing recipe and start-plan identity"
        };
        if(!s.planCertified||(s.contentWitnessed&s.requiredContent)!=s.requiredContent) return  {
            ReadyStage::RecipeFrozen,false,"Required final content is not certified"
        };
        if(!s.dataRequired||s.dataReady<s.dataRequired)return  {
            ReadyStage::ContentPlanReady,false,"Loading spawn data"
        };
        if(s.needsCollision&&(!s.collisionRequired||s.collisionReady<s.collisionRequired)) return  {
            ReadyStage::SpawnDataReady,false,"Waiting for current spawn collision"
        };
        if(s.needsView&&(!s.viewRequired||s.viewReadyIncludingEmpty<s.viewRequired)) return  {
            ReadyStage::SpawnCollisionReady,false,"Waiting for primary visible surface coverage"
        };
        if(s.pendingCriticalDependencies)return  {
            ReadyStage::PrimaryViewReady,false,"Waiting for authoritative boundary dependencies"
        };
        return  {
            ReadyStage::Playable,true,"Ready"
        };
    }
}
