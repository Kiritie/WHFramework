#pragma once
#include "Voxel/Generation/Kernel/VoxelGenFeatures.h"
namespace VoxelGen
{
class WHFRAMEWORK_API Query
{
public:
    Query(Settings settings,std::shared_ptr<const Catalog> catalog);
    Query(const Query&)=delete;
    Query& operator=(const Query&)=delete;
    bool Prepare(Box bounds,bool visualOnly,std::string& error,const std::atomic_bool* cancel=nullptr);
    Cell Sample(I3 p) const;
    Column SampleColumn(int32_t x,int32_t y) const;
    int32_t UpperBound(int32_t x,int32_t y) const;
    bool Section(I3 section,std::array<Cell,4096>& out,const std::atomic_bool* cancel=nullptr) const;
    const StructurePlan& Structures() const {return structures;}
    const FeaturePlan& Features() const {return features;}
    const Settings& GetSettings() const {return s;}
    const Catalog& GetCatalog() const {return *catalog;}
    bool IsPrepared() const {return prepared;}
private:
    Cell WithoutPlants(I3 p,const Column& column) const;
    Settings s;
    std::shared_ptr<const Catalog> catalog;
    Terrain terrain;
    Box bounds;
    bool prepared=false;
    StructurePlan structures;
    FeaturePlan features;
    mutable std::map<std::pair<int32_t,int32_t>,Column> columns;
};
}
