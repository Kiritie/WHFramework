#pragma once
#include "Voxel/Generation/Kernel/VoxelGenRepairCommon.h"
namespace VoxelRepair  {
    enum class ReadState:uint8_t  {
        Known,Unknown,Error
    };
    struct ReadBlock  {
        ReadState state=ReadState::Unknown;
        Cell value=0;
        bool opaqueFull=false,fluid=false,specialShape=false;
    };
    enum class FaceDecision:uint8_t  {
        Hidden,Emit,Deferred,Error
    };
    WHFRAMEWORK_API FaceDecision DecideFace(const ReadBlock& current,const ReadBlock& neighbor);
    struct DependencyStamp  {
        uint64_t epoch=0,recipe=0,token=0,revision=0,geometry=0;
        int32_t keyX=0,keyY=0,keyZ=0;
        uint8_t level=0,domain=0;
        bool operator==(const DependencyStamp& b)const {
            return std::tie(epoch,recipe,token,revision,geometry,keyX,keyY,keyZ,level,domain)==std::tie(b.epoch,b.recipe,b.token,b.revision,b.geometry,b.keyX,b.keyY,b.keyZ,b.level,b.domain);
        }
    };
    WHFRAMEWORK_API bool CanPublish(const DependencyStamp& captured,const DependencyStamp& current, const std::vector<DependencyStamp>& capturedNeighbors,const std::vector<DependencyStamp>& currentNeighbors);
}
