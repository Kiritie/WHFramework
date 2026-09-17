#include "Voxel/Rendering/Kernel/VoxelViewCoverage.h"
#include "Voxel/Generation/Kernel/VoxelGenMath.h"
namespace VoxelView
{
namespace
{
bool Owner(const std::set<Key>& keys,I3 p,uint8_t max,Key& out)
{
    for(uint8_t l=0;l<=max;++l){int s=16<<l;Key k{{VoxelGen::FloorDiv(p.x,s),VoxelGen::FloorDiv(p.y,s),VoxelGen::FloorDiv(p.z,s)},l};if(keys.count(k)){out=k;return true;}}
    return false;
}
bool Neighbor(Box a,Box b)
{
    for(int ax=0;ax<3;++ax)
    {
        const int u=(ax+1)%3,v=(ax+2)%3;
        if((a.max[ax]==b.min[ax]||b.max[ax]==a.min[ax])&&a.min[u]<b.max[u]&&a.max[u]>b.min[u]&&a.min[v]<b.max[v]&&a.max[v]>b.min[v])return true;
    }
    return false;
}
}
bool Coverage::SetDesired(const std::vector<Key>& leaves)
{
    std::set<Key> t(leaves.begin(),leaves.end()),n,r;
    for(Key k:leaves)
    {
        if(k.level>maxLevel)return false;
        while(true){n.insert(k);if(k.level==maxLevel){r.insert(k);break;}k=k.Parent();}
    }
    for(const Key& k:t){Key p=k;while(p.level<maxLevel){p=p.Parent();if(t.count(p))return false;}}
    if(target==t)return true;
    target=std::move(t);nodes=std::move(n);roots=std::move(r);++generation;return true;
}
bool Coverage::Wants(Key k) const{return nodes.count(k)!=0;}
bool Coverage::FindOwner(I3 p,Key& out) const{return Owner(active,p,maxLevel,out);}
void Coverage::Clear(){target.clear();nodes.clear();roots.clear();active.clear();blocked.clear();++generation;}
std::set<Key> Coverage::Apply(const Change& c) const
{
    auto a=active;for(const Key& k:c.remove)a.erase(k);for(const Key& k:c.add)a.insert(k);return a;
}
bool Coverage::Balanced(const std::set<Key>& a) const
{
    for(const Key& k:a)
    {
        Box b=k.Bounds();
        for(int face=0;face<6;++face)
        {
            int ax=face/2,u=(ax+1)%3,v=(ax+2)%3;
            I3 p;p[ax]=(face%2==0)?b.max[ax]:b.min[ax]-1;p[u]=(b.min[u]+b.max[u])/2;p[v]=(b.min[v]+b.max[v])/2;
            Key other;
            if(Owner(a,p,maxLevel,other)){if(std::abs(int(k.level)-int(other.level))>1)return false;}
            else
            {
                const int s=16<<maxLevel;Key root{{VoxelGen::FloorDiv(p.x,s),VoxelGen::FloorDiv(p.y,s),VoxelGen::FloorDiv(p.z,s)},maxLevel};
                // A desired but still missing neighbor is temporarily at root resolution. Do not refine its boundary first.
                if(roots.count(root)&&int(maxLevel)-int(k.level)>1)return false;
            }
        }
    }
    return true;
}
bool Coverage::Build(std::vector<Key> remove,std::vector<Key> add,Change& out)
{
    Change c;c.remove=std::move(remove);c.add=std::move(add);c.generation=generation;
    for(const Key& k:c.remove)if(!active.count(k))return false;
    auto after=Apply(c);
    for(const Key& k:after){Key p=k;while(p.level<maxLevel){p=p.Parent();if(after.count(p))return false;}}
    if(!Balanced(after))return false;
    std::set<Key> affected(c.add.begin(),c.add.end());
    for(const Key& k:after)
    {
        for(const Key& q:c.remove)if(Neighbor(k.Bounds(),q.Bounds()))affected.insert(k);
        for(const Key& q:c.add)if(!(k==q)&&Neighbor(k.Bounds(),q.Bounds()))affected.insert(k);
    }
    c.remesh.assign(affected.begin(),affected.end());c.serial=nextSerial++;out=std::move(c);return true;
}
bool Coverage::Propose(const std::function<bool(Key)>& ready,std::vector<Key>& request,Change& out)
{
    request.clear();
    // Obsolete root domains are dropped before adding new roots after teleport.
    std::vector<Key> obsolete;
    for(Key k:active){Key root=k;while(root.level<maxLevel)root=root.Parent();if(!roots.count(root))obsolete.push_back(k);}
    if(!obsolete.empty())return Build(obsolete,{},out);
    for(const Key& root:roots)
    {
        bool covered=false;for(const Key& k:active)if(root.Bounds().Intersects(k.Bounds())){covered=true;break;}
        if(covered)continue;
        std::vector<Key> partition;bool complete=true;
        std::function<void(Key)> collect=[&](Key k)
        {
            if(blocked.count(k)&&k.level){for(Key child:k.Children())collect(child);return;}
            if(!ready(k)){request.push_back(k);complete=false;}else partition.push_back(k);
        };
        collect(root);if(complete&&Build({},partition,out))return true;
    }
    // Merge siblings only one level at a time; a large direct merge could break 2:1 continuity.
    std::set<Key> parents;
    for(const Key& k:active)if(k.level<maxLevel)parents.insert(k.Parent());
    for(const Key& p:parents)
    {
        bool coarseTarget=target.count(p)!=0;Key a=p;
        while(!coarseTarget&&a.level<maxLevel){a=a.Parent();coarseTarget=target.count(a)!=0;}
        if(!coarseTarget)continue;
        auto kids=p.Children();std::vector<Key> remove;bool all=true;
        for(const Key& k:kids){if(active.count(k))remove.push_back(k);else all=false;}
        if(!all)continue;
        if(!ready(p)){request.push_back(p);continue;}
        if(Build(remove,{p},out))return true;
    }
    // Split only when all eight children are ready. Empty/out-of-height children have explicit zero grids.
    for(const Key& k:active)
    {
        if(!k.level||target.count(k)||!nodes.count(k))continue;
        auto kids=k.Children();bool all=true;std::vector<Key> add;
        for(const Key& child:kids){add.push_back(child);if(!ready(child)){request.push_back(child);all=false;}}
        if(all&&Build({k},add,out))return true;
    }
    std::sort(request.begin(),request.end());request.erase(std::unique(request.begin(),request.end(),[](const Key& a,const Key& b){return a==b;}),request.end());return false;
}
bool Coverage::Replacement(Key k,Change& out){if(!active.count(k))return false;return Build({k},{k},out);}
bool Coverage::Commit(const Change& c)
{
    if(!c.serial||c.generation!=generation)return false;
    for(const Key& k:c.remove)if(!active.count(k))return false;
    auto after=Apply(c);if(!Balanced(after))return false;
    active=std::move(after);++generation;return true;
}
}
