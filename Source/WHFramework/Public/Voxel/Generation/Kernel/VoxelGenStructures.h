#pragma once
#include "Voxel/Generation/Kernel/VoxelGenTerrain.h"
namespace VoxelGen
{
struct Building
{
    Id id;
    uint32_t prototype=0;
    I3 origin;
    uint8_t yaw=0;
    int32_t scale=1, ground=0;
    Box bounds,pad;
};
struct Road
{
    I3 a,b;
    int32_t halfWidth=6,headroom=16;
    bool tunnel=false;
};
struct Site
{
    Id id;
    uint8_t kind=0; // 0=castle, 1=dungeon, 2=town; no gameplay/spawn semantics.
    Box bounds;
    I3 entrance;
};
struct WHFRAMEWORK_API StructurePlan
{
    std::vector<Building> buildings;
    std::vector<Road> roads;
    std::vector<Site> sites;
    std::vector<DetailPlacement> details;
    Cell Sample(I3 p,Cell natural,const Settings& s,const Catalog& c) const;
    bool Reserved(int32_t x,int32_t y,int32_t margin=0) const;
};
WHFRAMEWORK_API bool PlanStructures(const Settings& settings,const Catalog& catalog,const Terrain& terrain,Box query,StructurePlan& out,std::string& error,const std::atomic_bool* cancel=nullptr);
}
