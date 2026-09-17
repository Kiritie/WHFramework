#include "Voxel/Generation/Kernel/VoxelGenQuery.h"
#include "Voxel/Generation/Kernel/VoxelGenMath.h"
namespace VoxelGen
{
Query::Query(Settings settings,std::shared_ptr<const Catalog> c):s(settings),catalog(std::move(c)),terrain(s,catalog->palette){}
bool Query::Prepare(Box b,bool visualOnly,std::string& error,const std::atomic_bool* cancel)
{
    prepared=false;columns.clear();structures={};features={};
    if(!b.Valid()||!ValidPosition(b.min)||!ValidPosition({b.max.x-1,b.max.y-1,b.max.z-1})||b.max.x-b.min.x>512||b.max.y-b.min.y>512){error="Invalid/big query: split into <=512-cell tiles";return false;}
    int32_t reach=32;
    for(const auto& t:catalog->trees)reach=std::max(reach,2*t.crownRadius+2*t.spacing+32);
    // Include every site that could affect every candidate used by the tree thinning rule.
    Box sites={{b.min.x-reach,b.min.y-reach,s.minZ},{b.max.x+reach,b.max.y+reach,s.maxZ}};
    StructurePlan sp;
    if(!PlanStructures(s,*catalog,terrain,sites,sp,error,cancel))return false;
    FeaturePlan fp;
    if(!PlanFeatures(s,*catalog,terrain,sp,b,!visualOnly,fp,error,cancel))return false;
    if(Canceled(cancel)){error="Canceled";return false;}
    bounds=b;structures=std::move(sp);features=std::move(fp);prepared=true;error.clear();return true;
}
Column Query::SampleColumn(int32_t x,int32_t y) const
{
    const auto key=std::make_pair(x,y);auto it=columns.find(key);if(it!=columns.end())return it->second;
    Column c=terrain.SampleColumn(x,y);
    // Per-query memoization only. Hitting the cap changes performance, never the answer.
    if(columns.size()<8192)columns.emplace(key,c);
    return c;
}
Cell Query::WithoutPlants(I3 p,const Column& column) const
{
    if(p.z<s.minZ||p.z>=s.maxZ)return 0;
    Cell value=terrain.Geology(p,column);
    value=features.OreAt(p,value,catalog->palette);
    value=structures.Sample(p,value,s,*catalog);
    return features.TreesAt(p,value);
}
Cell Query::Sample(I3 p) const
{
    if(!prepared||!bounds.Contains(p))return 0; // Facade validates requests before publishing results.
    const Column c=SampleColumn(p.x,p.y);Cell value=WithoutPlants(p,c);
    if(p.z==c.height+1&&!structures.Reserved(p.x,p.y))value=features.PlantAt(p,c,value,WithoutPlants({p.x,p.y,p.z-1},c),s,*catalog);
    return value;
}
int32_t Query::UpperBound(int32_t x,int32_t y) const
{
    const Column c=SampleColumn(x,y);int32_t z=std::max(c.height+1,c.water);
    for(const auto& b:structures.buildings)if(b.bounds.XY(x,y))z=std::max(z,b.bounds.max.z-1);
    for(const auto& t:features.trees)if(t.bounds.XY(x,y))z=std::max(z,t.bounds.max.z-1);
    return std::clamp(z,s.minZ,s.maxZ-1);
}
bool Query::Section(I3 key,std::array<Cell,4096>& out,const std::atomic_bool* cancel) const
{
    const I3 origin{key.x*16,key.y*16,key.z*16};
    if(!prepared||!bounds.Contains(origin)||!bounds.Contains(origin+I3{15,15,15}))return false;
    std::array<Cell,4096> buffer{};
    for(int32_t y=0;y<16;++y)for(int32_t x=0;x<16;++x)
    {
        if(Canceled(cancel))return false;
        for(int32_t z=0;z<16;++z)buffer[x+16*y+256*z]=Sample(origin+I3{x,y,z});
    }
    out=buffer;return true;
}
}
