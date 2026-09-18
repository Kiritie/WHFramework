#pragma once
#include "Voxel/Generation/Kernel/VoxelGenRepairCommon.h"
namespace VoxelRepair  {
    struct WalkSpan  {
        int32_t foot=0,ceiling=0;
    };
    // foot is the first free cell; floor is foot-1
    struct CaveRoute  {
        std::vector<Point> centerline;
        std::map<std::pair<int32_t,int32_t>,WalkSpan> footprint;
        int32_t minClearHeight=0,maxStepCells=1;
        bool Carves(Point) const;
        bool ProtectsFloor(Point) const;
    };
    struct CaveRouteSettings  {
        int32_t radiusCells=4,clearHeight=10,requiredHeadroom=9,maxStepCells=1,minHorizontalPerDrop=2;
        uint32_t maxNodes=4096,maxFootprint=262144;
    };
    WHFRAMEWORK_API bool BuildCaveRoute(uint64_t seed,Point entranceFoot,Point destinationFoot, const CaveRouteSettings&,CaveRoute&,std::string&,const std::atomic_bool* cancel=nullptr);
    WHFRAMEWORK_API bool ValidateWalkRoute(const CaveRoute&,Point startXY,Point endXY,std::string&,const std::atomic_bool* cancel=nullptr);
    WHFRAMEWORK_API bool AddCaveBranch(const CaveRoute& branch,CaveRoute& system,std::string&,const std::atomic_bool* cancel=nullptr);
}
