#include "Voxel/Generation/Kernel/VoxelGenFeatures.h"
#include "Voxel/Generation/Kernel/VoxelGenMath.h"
#include <cstdlib>
namespace VoxelGen
{
static bool Allowed(uint32_t mask,Biome b){return (mask&(1u<<uint8_t(b)))!=0;}
static uint64_t KeyHash(const std::string& s){return HashBytes(std::vector<uint8_t>(s.begin(),s.end()));}
namespace
{
struct Candidate { bool valid=false;I3 root;uint32_t rule=0;uint64_t rank=0;I3 grid; };
Candidate CandidateAt(const Settings& s,const Catalog& c,const Terrain& t,const StructurePlan& structures,I3 grid)
{
    Candidate out;out.grid=grid;if(c.trees.empty())return out;
    const int32_t spacing=c.trees[0].spacing;
    const uint64_t seed=Seed(s.seed,grid,0x5001);
    const int32_t x=grid.x*spacing+Range(Mix(seed),spacing/8,spacing*7/8),y=grid.y*spacing+Range(Mix(seed+1),spacing/8,spacing*7/8);
    const Column column=t.SampleColumn(x,y);
    if(column.water>=column.height||column.height<=s.minZ+4||structures.Reserved(x,y,16))return out;
    std::vector<uint32_t> rules;
    for(uint32_t i=0;i<c.trees.size();++i)if(Allowed(c.trees[i].biomeMask,column.biome))rules.push_back(i);
    if(rules.empty())return out;
    out.rule=rules[Mix(seed+2)%rules.size()];const auto& rule=c.trees[out.rule];
    if(structures.Reserved(x,y,2*rule.crownRadius+4))return out;
    const int factor=column.biome==Biome::Forest?1000:column.biome==Biome::Rainforest?1000:column.biome==Biome::Plains?180:column.biome==Biome::Savanna?300:column.biome==Biome::Snow?250:500;
    if(Mix(seed+3)%1000000>=uint64_t(rule.chancePermille)*factor)return out;
    const Cell support=t.Geology({x,y,column.height},column);
    if(Symbol(support)!=c.palette.grass&&Symbol(support)!=c.palette.dirt&&Symbol(support)!=c.palette.snow)return out;
    const int32_t r=rule.trunkRadius+2;
    const I3 d[]={{r,0,0},{-r,0,0},{0,r,0},{0,-r,0}};
    for(auto off:d){const auto neighbor=t.SampleColumn(x+off.x,y+off.y);if(neighbor.water>=neighbor.height||std::abs(neighbor.height-column.height)>rule.maxSlope)return out;}
    if(column.height+rule.maxHeight+rule.crownRadius>=s.maxZ)return out;
    out.root={x,y,column.height+1};out.rank=Mix(seed^KeyHash(rule.key));out.valid=true;return out;
}
Tree BuildTree(const Settings& s,const TreeRule& rule,const Candidate& p)
{
    Tree tree;tree.id=MakeId(s.seed,p.grid,KeyHash(rule.key));tree.root=p.root;tree.trunk=rule.trunk;tree.leaves=rule.leaves;
    const int32_t h=Range(Mix(p.rank),rule.minHeight,rule.maxHeight),r=rule.crownRadius;
    tree.branches.push_back({p.root,p.root+I3{0,0,h},rule.trunkRadius});
    if(rule.kind==TreeKind::Conifer)
    {
        for(int i=0;i<5;++i){int rr=std::max(4,r-i*r/6);tree.canopy.push_back({p.root+I3{0,0,h/3+i*h/7},{rr,rr,std::max(4,h/7)}});}
    }
    else
    {
        tree.canopy.push_back({p.root+I3{0,0,h-r/3},{r,r,std::max(8,r*3/4)}});
        const I3 dirs[]={{1,0,0},{0,1,0},{-1,0,0},{0,-1,0}};
        const uint8_t rot=uint8_t(p.rank&3);
        for(int i=0;i<4;++i)
        {
            I3 d=dirs[(i+rot)&3];const int32_t extent=r*2/3;
            I3 base=p.root+I3{0,0,h*2/3+i*2};I3 end=p.root+I3{d.x*extent,d.y*extent,h*4/5+i*2};
            tree.branches.push_back({base,end,std::max(1,rule.trunkRadius/2)});
            tree.canopy.push_back({end,{std::max(6,r/2),std::max(6,r/2),std::max(6,r/2)}});
        }
    }
    tree.bounds={{p.root.x-2*r-2,p.root.y-2*r-2,p.root.z-rule.trunkRadius},{p.root.x+2*r+3,p.root.y+2*r+3,p.root.z+h+r+3}};
    return tree;
}
}
bool PlanFeatures(const Settings& s,const Catalog& c,const Terrain& terrain,const StructurePlan& structures,Box q,bool includeOres,FeaturePlan& out,std::string& error,const std::atomic_bool* cancel)
{
    FeaturePlan p;
    if(!c.trees.empty())
    {
        const int32_t spacing=c.trees[0].spacing;
        int32_t reach=0;for(const auto& t:c.trees)reach=std::max(reach,2*t.crownRadius+3);
        const int32_t x0=FloorDiv(q.min.x-reach,spacing),x1=FloorDiv(q.max.x+reach-1,spacing),y0=FloorDiv(q.min.y-reach,spacing),y1=FloorDiv(q.max.y+reach-1,spacing);
        if(int64_t(x1-x0+1)*(y1-y0+1)>4096){error="Tree query too large";return false;}
        std::map<I3,Candidate> cache;
        auto candidate=[&](I3 key)->const Candidate&{auto it=cache.find(key);if(it!=cache.end())return it->second;return cache.emplace(key,CandidateAt(s,c,terrain,structures,key)).first->second;};
        for(int32_t y=y0;y<=y1;++y)for(int32_t x=x0;x<=x1;++x)
        {
            if(Canceled(cancel)){error="Canceled";return false;}
            const Candidate center=candidate({x,y,0});if(!center.valid)continue;
            bool keep=true;
            for(int oy=-1;oy<=1&&keep;++oy)for(int ox=-1;ox<=1&&keep;++ox)
            {
                if(!ox&&!oy)continue;
                const Candidate neighbor=candidate({x+ox,y+oy,0});if(!neighbor.valid)continue;
                const int64_t dx=int64_t(center.root.x)-neighbor.root.x,dy=int64_t(center.root.y)-neighbor.root.y;
                if(dx*dx+dy*dy<int64_t(spacing/2)*(spacing/2)&&std::tie(neighbor.rank,neighbor.grid)<std::tie(center.rank,center.grid))keep=false;
            }
            if(keep){Tree t=BuildTree(s,c.trees[center.rule],center);if(t.bounds.Intersects(q))p.trees.push_back(std::move(t));}
        }
    }
    if(includeOres)
    {
        uint64_t count=0;
        for(const auto& rule:c.ores)
        {
            if(q.max.z<=rule.minZ||q.min.z>=rule.maxZ)continue;
            const int32_t reach=rule.length+rule.radius+1;
            const I3 lo{FloorDiv(q.min.x-reach,rule.spacing),FloorDiv(q.min.y-reach,rule.spacing),FloorDiv(std::max(q.min.z-reach,rule.minZ),rule.spacing)};
            const I3 hi{FloorDiv(q.max.x+reach-1,rule.spacing),FloorDiv(q.max.y+reach-1,rule.spacing),FloorDiv(std::min(q.max.z+reach-1,rule.maxZ-1),rule.spacing)};
            const uint64_t salt=KeyHash(rule.key)^0x6001;
            for(int32_t z=lo.z;z<=hi.z;++z)for(int32_t y=lo.y;y<=hi.y;++y)for(int32_t x=lo.x;x<=hi.x;++x)
            {
                if(++count>65536){error="Ore candidate budget exceeded";return false;}
                if((count&255)==0&&Canceled(cancel)){error="Canceled";return false;}
                const I3 anchor{x,y,z};const uint64_t seed=Seed(s.seed,anchor,salt);
                if(seed%1000>=uint64_t(rule.chancePermille))continue;
                I3 a{x*rule.spacing+Range(Mix(seed),0,rule.spacing-1),y*rule.spacing+Range(Mix(seed+1),0,rule.spacing-1),z*rule.spacing+Range(Mix(seed+2),0,rule.spacing-1)};
                if(a.z<rule.minZ||a.z>=rule.maxZ)continue;
                const auto column=terrain.SampleColumn(a.x,a.y);if(!Allowed(rule.biomeMask,column.biome))continue;
                I3 d{Range(Mix(seed+3),-rule.length,rule.length),Range(Mix(seed+4),-rule.length,rule.length),Range(Mix(seed+5),-rule.length/2,rule.length/2)};
                I3 end=a+d;const int r=rule.radius;
                Vein vein;vein.id=MakeId(s.seed,anchor,salt);vein.block=rule.block;vein.shape={a,end,r};
                vein.bounds={{std::min(a.x,end.x)-r,std::min(a.y,end.y)-r,std::max(rule.minZ,std::min(a.z,end.z)-r)},{std::max(a.x,end.x)+r+1,std::max(a.y,end.y)+r+1,std::min(rule.maxZ,std::max(a.z,end.z)+r+1)}};
                if(vein.bounds.Intersects(q))p.veins.push_back(vein);
            }
        }
    }
    if(p.trees.size()>2048){error="Tree output budget exceeded";return false;}
    std::sort(p.trees.begin(),p.trees.end(),[](const Tree& a,const Tree& b){return a.id<b.id;});
    std::sort(p.veins.begin(),p.veins.end(),[](const Vein& a,const Vein& b){return a.id<b.id;});
    out=std::move(p);error.clear();return true;
}
Cell FeaturePlan::OreAt(I3 p,Cell ground,const Palette& palette) const
{
    if(Symbol(ground)!=palette.stone)return ground;
    for(const auto& v:veins)if(v.bounds.Contains(p)&&InCapsule(p,v.shape.a,v.shape.b,v.shape.radius))return Pack(v.block);
    return ground;
}
Cell FeaturePlan::TreesAt(I3 p,Cell ground) const
{
    if(ground!=0)return ground; // Never overwrite terrain, structures, water or lava.
    Cell leaf=0;
    for(const auto& t:trees)
    {
        if(!t.bounds.Contains(p))continue;
        for(const auto& branch:t.branches)if(InCapsule(p,branch.a,branch.b,branch.radius))return Pack(t.trunk);
        if(!leaf)for(const auto& l:t.canopy)if(InEllipsoid(p,l.center,l.radius)){leaf=Pack(t.leaves);break;}
    }
    return leaf;
}
Cell FeaturePlan::PlantAt(I3 p,const Column& column,Cell current,Cell support,const Settings& s,const Catalog& c) const
{
    if(current||p.z!=column.height+1||column.water>=column.height||Symbol(support)!=c.palette.grass)return current;
    Cell result=0;uint64_t best=UINT64_MAX;
    for(const auto& rule:c.plants)
    {
        if(!Allowed(rule.biomeMask,column.biome))continue;
        const uint64_t salt=KeyHash(rule.key)^0x7001;
        const int32_t patch=Noise2(s.seed,p.x,p.y,rule.patchPeriod,salt);
        const uint64_t h=Seed(s.seed,p,salt);
        const int32_t density=rule.chancePermille*std::clamp(patch+16384,0,32768)/32768;
        if(h%1000<uint64_t(density)&&h<best){best=h;result=Pack(rule.block);}
    }
    return result;
}
}
