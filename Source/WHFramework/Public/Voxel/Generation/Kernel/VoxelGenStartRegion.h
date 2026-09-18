#pragma once
#include "Voxel/Generation/Kernel/VoxelGenRepairCommon.h"
namespace VoxelRepair  {
    enum class StartAnchorKind:uint8_t  {
        Spawn,Town,Lake,RiverSource,CaveEntrance,Forest,Castle,Dungeon,LavaChamber
    };
    struct StartAnchor  {
        StartAnchorKind kind;
        Point offsetCells;
    };
    struct StartRegionPlan  {
        uint64_t layoutSeed=0;
        Point origin;
        int32_t cellCm=25,coreCells=0,outerCells=0;
        int32_t valleyPlane=0;
        uint8_t yaw=0;
        std::vector<StartAnchor> anchors;
        bool ConstrainHeight(Point xy,int32_t rawGroundPlane,int32_t& out,std::string&) const;
    };
    WHFRAMEWORK_API bool BuildStartRegion(int32_t seed,Point origin,int32_t cellCm, int32_t valleyGroundPlane,StartRegionPlan&,std::string&);
}
