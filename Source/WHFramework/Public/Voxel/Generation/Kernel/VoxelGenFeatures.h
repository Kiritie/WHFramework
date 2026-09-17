#pragma once
#include "Voxel/Generation/Kernel/VoxelGenStructures.h"
namespace VoxelGen
{
struct Capsule { I3 a,b;int32_t radius=1; };
struct Ellipsoid { I3 center,radius; };
struct Tree
{
    Id id;
    I3 root;
    uint16_t trunk=0,leaves=0;
    Box bounds;
    std::vector<Capsule> branches;
    std::vector<Ellipsoid> canopy;
};
struct Vein
{
    Id id;
    uint16_t block=0;
    Box bounds;
    Capsule shape;
};
struct WHFRAMEWORK_API FeaturePlan
{
    std::vector<Tree> trees;
    std::vector<Vein> veins;
    Cell TreesAt(I3 p,Cell ground) const;
    Cell OreAt(I3 p,Cell ground,const Palette& palette) const;
    Cell PlantAt(I3 p,const Column& column,Cell current,Cell support,const Settings& settings,const Catalog& catalog) const;
};
WHFRAMEWORK_API bool PlanFeatures(const Settings& settings,const Catalog& catalog,const Terrain& terrain,const StructurePlan& structures,Box query,bool includeOres,FeaturePlan& out,std::string& error,const std::atomic_bool* cancel=nullptr);
}
