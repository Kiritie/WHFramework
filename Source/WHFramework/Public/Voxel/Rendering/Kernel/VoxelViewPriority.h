#pragma once
#include "Voxel/Generation/Kernel/VoxelGenRepairCommon.h"
namespace VoxelRepair  {
    enum class WorkClass:uint8_t  {
        Safety,Startup,VisibleSurface,BoundaryDependency,Exploration,Background
    };
    struct WorkItem  {
        uint64_t stableId=0,queuedMs=0;
        WorkClass kind=WorkClass::Background;
        int64_t distanceSquaredCells=0;
        uint32_t forwardScore=0;
        bool certifiedEmpty=false;
    };
    struct WorkRank  {
        uint8_t group=0;
        int64_t score=0;
        uint64_t id=0;
        bool operator<(const WorkRank& b)const {
            return std::tie(group,score,id)<std::tie(b.group,b.score,b.id);
        }
    };
    WHFRAMEWORK_API WorkRank RankWork(const WorkItem&,uint64_t nowMs);
    WHFRAMEWORK_API std::vector<uint64_t> OrderMeshWork(const std::vector<WorkItem>&,uint64_t nowMs);
}
