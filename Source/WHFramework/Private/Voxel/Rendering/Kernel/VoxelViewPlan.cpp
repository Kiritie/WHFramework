#include "Voxel/Rendering/Kernel/VoxelViewPlan.h"
#include "Voxel/Generation/Kernel/VoxelGenMath.h"
#include <set>
namespace VoxelView
{
Box Key::Bounds() const {const int32_t s=Side();const I3 o{coordinate.x*s,coordinate.y*s,coordinate.z*s};return {o,o+I3{s,s,s}};}
Key Key::Parent() const {return {{VoxelGen::FloorDiv(coordinate.x,2),VoxelGen::FloorDiv(coordinate.y,2),VoxelGen::FloorDiv(coordinate.z,2)},uint8_t(level+1)};}
std::array<Key,8> Key::Children() const {std::array<Key,8> out{};for(int i=0;i<8;++i)out[i]={{coordinate.x*2+(i&1),coordinate.y*2+((i>>1)&1),coordinate.z*2+((i>>2)&1)},uint8_t(level-1)};return out;}
bool Settings::Validate() const {return maxLevel<=4&&nearRadiusCm>=1600&&nearRadiusCm<=12800&&farRadiusCm>nearRadiusCm&&farRadiusCm<=102400&&verticalRadiusCm>=6400&&verticalRadiusCm<=51200&&maxLeaves>=512&&maxLeaves<=32768;}
Cell Grid::Sample(I3 p) const {const auto b=key.Bounds();if(!b.Contains(p))return 0;const I3 q=p-b.min;const int s=key.Step();return cells[q.x/s+16*(q.y/s)+256*(q.z/s)];}
static int64_t Distance2(Box b,I3 p){const int64_t x=p.x<b.min.x?int64_t(b.min.x)-p.x:p.x>=b.max.x?int64_t(p.x)-b.max.x+1:0;const int64_t y=p.y<b.min.y?int64_t(b.min.y)-p.y:p.y>=b.max.y?int64_t(p.y)-b.max.y+1:0;const int64_t z=p.z<b.min.z?int64_t(b.min.z)-p.z:p.z>=b.max.z?int64_t(p.z)-b.max.z+1:0;return x*x+y*y+z*z;}
static bool FaceNeighbor(Box a,Box b)
{
    const bool x=(a.max.x==b.min.x||b.max.x==a.min.x)&&a.min.y<b.max.y&&a.max.y>b.min.y&&a.min.z<b.max.z&&a.max.z>b.min.z;
    const bool y=(a.max.y==b.min.y||b.max.y==a.min.y)&&a.min.x<b.max.x&&a.max.x>b.min.x&&a.min.z<b.max.z&&a.max.z>b.min.z;
    const bool z=(a.max.z==b.min.z||b.max.z==a.min.z)&&a.min.x<b.max.x&&a.max.x>b.min.x&&a.min.y<b.max.y&&a.max.y>b.min.y;
    return x||y||z;
}
bool Select(const Settings& v,const VoxelGen::Settings& w,I3 p,std::vector<Key>& out,std::string& error)
{
    if(!v.Validate()||!VoxelGen::ValidPosition(p)){error="Invalid view configuration";return false;}
    const int r=VoxelGen::CeilDivPositive(v.farRadiusCm,w.cellCm),vr=VoxelGen::CeilDivPositive(v.verticalRadiusCm,w.cellCm),side=16<<v.maxLevel;
    const int x0=VoxelGen::FloorDiv(p.x-r,side),x1=VoxelGen::FloorDiv(p.x+r,side),y0=VoxelGen::FloorDiv(p.y-r,side),y1=VoxelGen::FloorDiv(p.y+r,side),z0=VoxelGen::FloorDiv(std::max(w.minZ,p.z-vr),side),z1=VoxelGen::FloorDiv(std::min(w.maxZ-1,p.z+vr),side);
    std::vector<Key> stack,leaves;
    if(int64_t(x1-x0+1)*(y1-y0+1)*(z1-z0+1)>v.maxLeaves){error="Root tile budget exceeded; lower explicit far/vertical setting";return false;}
    for(int z=z0;z<=z1;++z)for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x)stack.push_back({{x,y,z},v.maxLevel});
    const int near=VoxelGen::CeilDivPositive(v.nearRadiusCm,w.cellCm);
    while(!stack.empty())
    {
        const Key k=stack.back();stack.pop_back();const Box b=k.Bounds();
        if(b.max.z<=w.minZ||b.min.z>=w.maxZ)continue;
        // Level transitions use 2:1 refinement. Keep a full tile-width band for balancing.
        const int64_t threshold=int64_t(near)*(int64_t{1}<<std::max(0,int(k.level)-1));
        if(k.level&&Distance2(b,p)<=threshold*threshold)
        {
            auto children=k.Children();stack.insert(stack.end(),children.begin(),children.end());
        }
        else leaves.push_back(k);
        if(stack.size()+leaves.size()>v.maxLeaves){error="View leaf budget exceeded; no partial coverage is published";return false;}
    }
    // Explicit balancing by face-neighbor lookup, not an O(N^2) pair scan.
    for(int pass=0;pass<5;++pass)
    {
        std::set<Key> lookup(leaves.begin(),leaves.end()),split;
        for(const Key& k:leaves)
        {
            const Box b=k.Bounds();
            const I3 points[]={{b.min.x-1,(b.min.y+b.max.y)/2,(b.min.z+b.max.z)/2},{b.max.x,(b.min.y+b.max.y)/2,(b.min.z+b.max.z)/2},{(b.min.x+b.max.x)/2,b.min.y-1,(b.min.z+b.max.z)/2},{(b.min.x+b.max.x)/2,b.max.y,(b.min.z+b.max.z)/2},{(b.min.x+b.max.x)/2,(b.min.y+b.max.y)/2,b.min.z-1},{(b.min.x+b.max.x)/2,(b.min.y+b.max.y)/2,b.max.z}};
            for(I3 point:points)for(uint8_t level=uint8_t(k.level+2);level<=v.maxLevel;++level)
            {
                const int n=16<<level;Key other{{VoxelGen::FloorDiv(point.x,n),VoxelGen::FloorDiv(point.y,n),VoxelGen::FloorDiv(point.z,n)},level};
                if(lookup.count(other))split.insert(other);
            }
        }
        if(split.empty())break;
        std::vector<Key> next;
        for(const Key& k:leaves)if(split.count(k)){auto children=k.Children();for(const auto& child:children){const auto b=child.Bounds();if(b.max.z>w.minZ&&b.min.z<w.maxZ)next.push_back(child);}}else next.push_back(k);
        if(next.size()>v.maxLeaves){error="Balanced coverage exceeds leaf budget";return false;}
        leaves=std::move(next);
    }
    std::sort(leaves.begin(),leaves.end());out=std::move(leaves);error.clear();return true;
}
bool SelectMany(const Settings& v,const VoxelGen::Settings& w,const std::vector<I3>& observers,const std::set<Key>& forced,std::vector<Key>& out,std::string& e)
{
    if(observers.empty()||observers.size()>4){e="Expected one to four local observers";return false;}
    std::set<Key> all,nodes,roots;
    for(I3 p:observers)
    {
        std::vector<Key> one;if(!Select(v,w,p,one,e))return false;
        for(Key k:one){all.insert(k);while(true){nodes.insert(k);if(k.level==v.maxLevel){roots.insert(k);break;}k=k.Parent();}}
    }
    std::vector<Key> stack(roots.begin(),roots.end()),leaves;
    while(!stack.empty())
    {
        Key k=stack.back();stack.pop_back();const auto b=k.Bounds();if(b.max.z<=w.minZ||b.min.z>=w.maxZ)continue;
        bool split=false;
        if(k.level){for(Key child:k.Children())if(nodes.count(child))split=true;for(Key f:forced)if(k.level>=f.level&&b.Intersects(f.Bounds()))split=true;}
        if(split){auto children=k.Children();stack.insert(stack.end(),children.begin(),children.end());}else leaves.push_back(k);
        if(stack.size()+leaves.size()>v.maxLeaves){e="Union/forced coverage exceeds leaf budget";return false;}
    }
    for(int pass=0;pass<5;++pass)
    {
        std::set<Key> lookup(leaves.begin(),leaves.end()),split;
        for(Key k:leaves)
        {
            const auto b=k.Bounds();
            for(int f=0;f<6;++f)
            {
                const int a=f/2,u=(a+1)%3,z=(a+2)%3;I3 p;p[a]=(f&1)?b.min[a]-1:b.max[a];p[u]=(b.min[u]+b.max[u])/2;p[z]=(b.min[z]+b.max[z])/2;
                for(uint8_t l=uint8_t(k.level+2);l<=v.maxLevel;++l){int side=16<<l;Key n{{VoxelGen::FloorDiv(p.x,side),VoxelGen::FloorDiv(p.y,side),VoxelGen::FloorDiv(p.z,side)},l};if(lookup.count(n))split.insert(n);}
            }
        }
        if(split.empty())break;
        std::vector<Key> next;for(Key k:leaves)if(split.count(k)){for(Key child:k.Children()){auto b=child.Bounds();if(b.max.z>w.minZ&&b.min.z<w.maxZ)next.push_back(child);}}else next.push_back(k);
        if(next.size()>v.maxLeaves){e="Balanced multi-view budget exceeded";return false;}leaves=std::move(next);
    }
    std::sort(leaves.begin(),leaves.end());out=std::move(leaves);e.clear();return true;
}
bool IsBalanced(const std::vector<Key>& leaves)
{
    // Test-only diagnostic: expensive by design; never call in the frame loop.
    for(size_t i=0;i<leaves.size();++i)for(size_t j=i+1;j<leaves.size();++j)if(FaceNeighbor(leaves[i].Bounds(),leaves[j].Bounds())&&std::abs(int(leaves[i].level)-int(leaves[j].level))>1)return false;
    return true;
}
static bool Plant(const VoxelGen::Catalog& c,Cell value){for(const auto& p:c.plants)if(VoxelGen::Symbol(value)==p.block)return true;return false;}
static Cell Majority(const std::map<Cell,uint32_t>& counts,const VoxelGen::Catalog& c)
{
    Cell best=0;uint32_t n=0;int bestKind=0;
    for(const auto& v:counts)
    {
        const uint16_t symbol=VoxelGen::Symbol(v.first);if(!symbol||Plant(c,v.first))continue;
        const int kind=(symbol==c.palette.water||symbol==c.palette.lava)?1:2;
        if(kind>bestKind||(kind==bestKind&&v.second>n)){best=v.first;n=v.second;bestKind=kind;}
    }
    return best;
}
bool BuildProxy(const VoxelGen::Settings& s,std::shared_ptr<const VoxelGen::Catalog> catalog,Key key,const std::vector<Override>& edits,Grid& out,std::string& error,const std::atomic_bool* cancel)
{
    if(key.level==0||key.level>4||edits.size()>1048576){error="Invalid proxy request";return false;}
    const Box b=key.Bounds();const int step=key.Step();VoxelGen::Query query(s,catalog);
    Box valid=b;valid.min.z=std::max(valid.min.z,s.minZ);valid.max.z=std::min(valid.max.z,s.maxZ);
    if(!query.Prepare(valid,true,error,cancel))return false;
    std::map<I3,Cell> overlay;std::set<int> touched;
    for(const auto& e:edits)
    {
        if(!b.Contains(e.position)||VoxelGen::Symbol(e.value)>=catalog->blocks.size()||!overlay.emplace(e.position,e.value).second){error="Proxy overlay is invalid/duplicated/outside tile";return false;}
        I3 p=e.position-b.min;touched.insert(p.x/step+16*(p.y/step)+256*(p.z/step));
    }
    Grid grid;grid.key=key;grid.natural=edits.empty();uint64_t work=0;
    auto sample=[&](I3 p){auto it=overlay.find(p);return it!=overlay.end()?it->second:query.Sample(p);};
    for(int z=0;z<16;++z)for(int y=0;y<16;++y)for(int x=0;x<16;++x)
    {
        if(VoxelGen::Canceled(cancel)){error="Canceled";return false;}
        const int index=x+16*y+256*z;const I3 p=b.min+I3{x*step,y*step,z*step};
        if(p.z>=s.maxZ||p.z+step<=s.minZ)continue;
        std::map<Cell,uint32_t> counts;
        if(touched.count(index))
        {
            // Edited buckets are recomputed from final fine cells. Removing the last leaf/wall cannot resurrect a natural proxy.
            for(int dz=0;dz<step;++dz)for(int dy=0;dy<step;++dy)for(int dx=0;dx<step;++dx)
            {
                if((++work&1023)==0&&VoxelGen::Canceled(cancel)){error="Canceled";return false;}
                ++counts[sample(p+I3{dx,dy,dz})];
            }
        }
        else
        {
            // Defined visual approximation: 27 stratified samples, no authority/collision use.
            const int positions[]={0,step/2,step-1};
            for(int dz:positions)for(int dy:positions)for(int dx:positions)++counts[sample(p+I3{dx,dy,dz})];
        }
        grid.cells[index]=Majority(counts,*catalog);
    }
    // Thin structure protection: rasterize any template solid whose source volume intersects a proxy cell.
    // This applies ONLY to untouched buckets. Edited buckets use exact final reduction above.
    for(const auto& building:query.Structures().buildings)
    {
        const auto& t=catalog->structures[building.prototype];
        for(const auto& run:t.writes)
        {
            if((++work&1023)==0&&VoxelGen::Canceled(cancel)){error="Canceled";return false;}
            if(!VoxelGen::Symbol(run.value))continue;
            const Box rb=VoxelGen::TransformBox({run.start,run.start+I3{run.length,1,1}},building.origin,building.scale,building.yaw);
            if(!rb.Intersects(b))continue;
            const I3 lo{std::max(0,VoxelGen::FloorDiv(rb.min.x-b.min.x,step)),std::max(0,VoxelGen::FloorDiv(rb.min.y-b.min.y,step)),std::max(0,VoxelGen::FloorDiv(rb.min.z-b.min.z,step))};
            const I3 hi{std::min(15,VoxelGen::FloorDiv(rb.max.x-1-b.min.x,step)),std::min(15,VoxelGen::FloorDiv(rb.max.y-1-b.min.y,step)),std::min(15,VoxelGen::FloorDiv(rb.max.z-1-b.min.z,step))};
            for(int z=lo.z;z<=hi.z;++z)for(int y=lo.y;y<=hi.y;++y)for(int x=lo.x;x<=hi.x;++x){int i=x+16*y+256*z;if(!touched.count(i))grid.cells[i]=VoxelGen::Pack(VoxelGen::Symbol(run.value),uint16_t((VoxelGen::State(run.value)+building.yaw)&3));}
        }
    }
    std::vector<uint8_t> canonical;canonical.reserve(16384);
    for(Cell c:grid.cells)for(int i=0;i<4;++i)canonical.push_back(uint8_t(c>>(i*8)));
    grid.hash=VoxelGen::HashBytes(canonical);out=std::move(grid);error.clear();return true;
}
Ticket Versions::Request(Key key)
{
    auto it=nodes.find(key);if(it==nodes.end()){Node n;n.wanted={epoch,nextSerial++,key};it=nodes.emplace(key,n).first;}
    it->second.pending=true;return it->second.wanted;
}
void Versions::Invalidate(Box cells)
{
    for(auto& pair:nodes)if(pair.first.Bounds().Intersects(cells)){pair.second.wanted.serial=nextSerial++;pair.second.dirty=true;pair.second.pending=false;}
}
bool Versions::Accept(Ticket t)
{
    auto it=nodes.find(t.key);if(it==nodes.end()||t.epoch!=epoch||t.serial!=it->second.wanted.serial)return false;
    it->second.accepted=t.serial;it->second.ready=true;it->second.dirty=false;it->second.pending=false;return true;
}
void Versions::Fail(Ticket t){auto it=nodes.find(t.key);if(it!=nodes.end()&&t.epoch==epoch&&t.serial==it->second.wanted.serial){it->second.pending=false;it->second.dirty=true;}}
void Versions::Release(Key k){nodes.erase(k);}
const Node* Versions::Find(Key k) const {auto it=nodes.find(k);return it==nodes.end()?nullptr:&it->second;}
}
