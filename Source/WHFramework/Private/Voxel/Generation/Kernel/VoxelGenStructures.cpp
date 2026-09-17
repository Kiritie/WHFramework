#include "Voxel/Generation/Kernel/VoxelGenStructures.h"
#include "Voxel/Generation/Kernel/VoxelGenMath.h"
#include <cstdlib>
namespace VoxelGen
{
static int32_t RoadHeight(const Road& r,int32_t x,int32_t y,bool& inside)
{
    const I3 d=r.b-r.a;const int64_t len=int64_t(d.x)*d.x+int64_t(d.y)*d.y;
    const int64_t px=int64_t(x)-r.a.x,py=int64_t(y)-r.a.y;
    if(!len){inside=std::abs(px)<=r.halfWidth&&std::abs(py)<=r.halfWidth;return r.a.z;}
    const int64_t t=std::clamp<int64_t>(px*d.x+py*d.y,0,len);
    const int64_t qx=px*len-int64_t(d.x)*t,qy=py*len-int64_t(d.y)*t;
    // Avoid fourth-order overflow: use a rounded nearest point, with an explicit one-cell tolerance.
    const int64_t nx=qx/len,ny=qy/len;
    inside=nx*nx+ny*ny<=int64_t(r.halfWidth)*r.halfWidth;
    return r.a.z+int32_t(int64_t(d.z)*t/len);
}
bool StructurePlan::Reserved(int32_t x,int32_t y,int32_t margin) const
{
    for(const Site& s:sites)
    {
        if(s.kind==1) {const int64_t dx=int64_t(x)-s.entrance.x,dy=int64_t(y)-s.entrance.y;const int64_t r=16+margin;if(dx*dx+dy*dy<=r*r)return true;continue;}
        if(int64_t(x)>=int64_t(s.bounds.min.x)-margin&&int64_t(x)<int64_t(s.bounds.max.x)+margin&&int64_t(y)>=int64_t(s.bounds.min.y)-margin&&int64_t(y)<int64_t(s.bounds.max.y)+margin)return true;
    }
    return false;
}
Cell StructurePlan::Sample(I3 p,Cell natural,const Settings& s,const Catalog& c) const
{
    Cell value=natural;
    for(const Road& r:roads)
    {
        bool inside=false;const int32_t z=RoadHeight(r,p.x,p.y,inside);
        if(!inside)continue;
        if(p.z<=z&&p.z>=z-s.maxSiteCutFill)value=Pack(c.palette.road);
        else if(p.z>z&&p.z<=z+r.headroom)value=0;
    }
    for(const Building& b:buildings)
    {
        if(c.structures[b.prototype].kind!=StructureKind::Dungeon&&b.pad.XY(p.x,p.y))
        {
            if(p.z<=b.ground&&p.z>=b.ground-s.maxSiteCutFill)value=Pack(c.palette.stone);
            if(p.z>b.ground&&p.z<=b.ground+s.maxSiteCutFill)value=0;
        }
        if(!b.bounds.Contains(p))continue;
        I3 local=RotateCell(p-b.origin,uint8_t((4-b.yaw)&3));
        local={FloorDiv(local.x,b.scale),FloorDiv(local.y,b.scale),FloorDiv(local.z,b.scale)};
        bool writes=false;const Cell v=c.structures[b.prototype].Sample(local,writes);
        if(writes)value=VoxelGen::Symbol(v)?VoxelGen::Pack(VoxelGen::Symbol(v),uint16_t((VoxelGen::State(v)+b.yaw)&3)):0; // Explicit writes override clear volumes; no-record preserves terrain.
    }
    return value;
}
namespace
{
bool Suitable(const Terrain& terrain,I3 center,int32_t radius,int32_t limit)
{
    const Column c=terrain.SampleColumn(center.x,center.y);
    if(c.water>=c.height)return false;
    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)
    {
        const auto p=terrain.SampleColumn(center.x+x*radius,center.y+y*radius);
        if(p.water>=p.height||std::abs(p.height-c.height)>limit)return false;
    }
    return true;
}
Building MakeBuilding(const Settings& s,const Catalog& c,uint32_t proto,I3 center,int32_t ground,uint8_t yaw,Id id)
{
    const auto& t=c.structures[proto];Building b;b.id=id;b.prototype=proto;b.scale=t.sourceCellCm/s.cellCm;b.yaw=yaw;b.ground=ground;
    const I3 localCenter{(t.bounds.min.x+t.bounds.max.x)*b.scale/2,(t.bounds.min.y+t.bounds.max.y)*b.scale/2,t.groundZ*b.scale};
    b.origin=center-RotateCorner(localCenter,yaw);b.origin.z=ground-t.groundZ*b.scale;
    b.bounds=TransformBox(t.bounds,b.origin,b.scale,yaw);
    b.pad={{b.bounds.min.x,b.bounds.min.y,ground-s.maxSiteCutFill},{b.bounds.max.x,b.bounds.max.y,ground+1}};
    return b;
}
I3 Entry(const Building& b,const StructureTemplate& t)
{
    // Explicit source entry is a corner at the walkable doorway floor, including source-grid scale.
    return RotateCorner({t.entrance.x*b.scale,t.entrance.y*b.scale,t.entrance.z*b.scale},b.yaw)+b.origin;
}
void AddDetails(const Settings&, const Catalog& c,const Building& b,StructurePlan& p)
{
    const auto& t=c.structures[b.prototype];
    for(uint32_t i=0;i<t.details.size();++i)
    {
        const auto& d=t.details[i];DetailPlacement v;
        v.id={b.id.high,Mix(b.id.low^i^0x44455441)};v.asset=d.asset;
        v.cell=RotateCorner({d.position.x*b.scale,d.position.y*b.scale,d.position.z*b.scale},b.yaw)+b.origin;
        v.yaw=uint8_t((b.yaw+d.yaw)&3);p.details.push_back(v);
    }
}
}
bool PlanStructures(const Settings& s,const Catalog& c,const Terrain& terrain,Box q,StructurePlan& out,std::string& error,const std::atomic_bool* cancel)
{
    StructurePlan p;
    std::vector<uint32_t> castle,dungeon,houses;
    for(uint32_t i=0;i<c.structures.size();++i){auto k=c.structures[i].kind;(k==StructureKind::Castle?castle:k==StructureKind::Dungeon?dungeon:houses).push_back(i);}
    // All site extents are bounded by 1024 cells around the anchor.
    const int32_t margin=1024;
    const int32_t x0=FloorDiv(q.min.x-margin,s.structureSpacing),x1=FloorDiv(q.max.x+margin-1,s.structureSpacing);
    const int32_t y0=FloorDiv(q.min.y-margin,s.structureSpacing),y1=FloorDiv(q.max.y+margin-1,s.structureSpacing);
    if(int64_t(x1-x0+1)*(y1-y0+1)>64){error="Structure query too large; split into bounded generation tiles";return false;}
    for(int32_t y=y0;y<=y1;++y)for(int32_t x=x0;x<=x1;++x)
    {
        if(Canceled(cancel)){error="Canceled";return false;}
        const I3 region{x,y,0};const uint64_t seed=Seed(s.seed,region,0x4001);
        if(seed%1000>=uint64_t(s.structureChancePermille))continue;
        I3 anchor{x*s.structureSpacing+s.structureSpacing/2+Range(Mix(seed),-s.structureSpacing/8,s.structureSpacing/8),y*s.structureSpacing+s.structureSpacing/2+Range(Mix(seed+1),-s.structureSpacing/8,s.structureSpacing/8),0};
        if(!ValidPosition(anchor))continue;
        const Column column=terrain.SampleColumn(anchor.x,anchor.y);anchor.z=column.height;
        const uint8_t kind=uint8_t(Mix(seed+2)%3);
        if(kind==2&&!houses.empty())
        {
            if(column.biome==Biome::Ocean||column.biome==Biome::Mountain||!Suitable(terrain,anchor,240,s.maxSiteCutFill))continue;
            StructurePlan town;Site site;site.id=MakeId(s.seed,region,0x544f574e);site.kind=2;
            site.bounds={{anchor.x-256,anchor.y-192,anchor.z-s.maxSiteCutFill},{anchor.x+256,anchor.y+192,anchor.z+192}};site.entrance=anchor;
            if(!site.bounds.Intersects(q))continue;
            town.sites.push_back(site);
            town.roads.push_back({{anchor.x-208,anchor.y,anchor.z},{anchor.x+208,anchor.y,anchor.z},8,24,false});
            town.roads.push_back({anchor,anchor,24,32,false});
            bool valid=true;
            for(uint32_t i=0;i<8;++i)
            {
                const bool north=i<4;const int32_t lx=-144+int32_t(i%4)*96,ly=north?96:-96;
                const uint32_t prototype=houses[Mix(seed+100+i)%houses.size()];
                I3 center{anchor.x+lx,anchor.y+ly,anchor.z};
                const int32_t ground=terrain.SampleColumn(center.x,center.y).height;
                if(std::abs(ground-anchor.z)>s.maxSiteCutFill||!Suitable(terrain,center,40,s.maxSiteCutFill)){valid=false;break;}
                const uint8_t yaw=north?0:2;
                Building b=MakeBuilding(s,c,prototype,center,ground,yaw,MakeId(s.seed,region,0x544f574e,i+1));
                const I3 entry=Entry(b,c.structures[prototype]);
                // Authoring rule: every TownHouse entrance is on local -Y and faces its road.
                town.roads.push_back({{entry.x,anchor.y,anchor.z},{entry.x,entry.y,entry.z-1},6,16,false});
                town.buildings.push_back(b);AddDetails(s,c,b,town);
            }
            if(!valid)continue;
            p.sites.insert(p.sites.end(),town.sites.begin(),town.sites.end());p.buildings.insert(p.buildings.end(),town.buildings.begin(),town.buildings.end());p.roads.insert(p.roads.end(),town.roads.begin(),town.roads.end());p.details.insert(p.details.end(),town.details.begin(),town.details.end());
        }
        else
        {
            const auto& choices=kind==1?dungeon:castle;if(choices.empty())continue;
            const uint32_t prototype=choices[Mix(seed+3)%choices.size()];const auto& t=c.structures[prototype];const int32_t scale=t.sourceCellCm/s.cellCm;
            const int32_t radius=std::max(t.bounds.max.x-t.bounds.min.x,t.bounds.max.y-t.bounds.min.y)*scale/2+8;
            if(column.water>=column.height||!Suitable(terrain,anchor,std::min(radius,384),s.maxSiteCutFill))continue;
            int32_t ground=anchor.z;
            if(kind==1)ground-=std::max(32,(t.bounds.max.z-t.groundZ)*scale+16);
            Building b=MakeBuilding(s,c,prototype,anchor,ground,uint8_t(Mix(seed+4)&3),MakeId(s.seed,region,kind==1?0x44554e47:0x43415354));
            if(b.bounds.min.z<=s.minZ+4||b.bounds.max.z>=s.maxZ)continue;
            Site site;site.id=b.id;site.kind=kind;site.bounds=b.bounds;site.entrance=Entry(b,t);
            if(kind==1)
            {
                // A deterministic ramp corridor opens the dungeon to the surface. No spawner/trap.
                const int32_t depth=anchor.z-site.entrance.z+1;
                const I3 dir=RotateCorner({0,-1,0},b.yaw);
                const I3 outer{site.entrance.x+dir.x*(2*depth+16),site.entrance.y+dir.y*(2*depth+16),anchor.z};
                const int32_t actual=terrain.SampleColumn(outer.x,outer.y).height;
                if(std::abs(actual-anchor.z)>s.maxSiteCutFill)continue;
                Road tunnel{outer,{site.entrance.x,site.entrance.y,site.entrance.z-1},6,16,true};
                site.bounds.min.x=std::min(site.bounds.min.x,outer.x-8);site.bounds.min.y=std::min(site.bounds.min.y,outer.y-8);site.bounds.max.x=std::max(site.bounds.max.x,outer.x+9);site.bounds.max.y=std::max(site.bounds.max.y,outer.y+9);site.bounds.max.z=std::max(site.bounds.max.z,outer.z+17);site.entrance=outer;
                if(!site.bounds.Intersects(q))continue;
                p.roads.push_back(tunnel);
            }
            else if(!site.bounds.Intersects(q))continue;
            p.sites.push_back(site);p.buildings.push_back(b);AddDetails(s,c,b,p);
        }
        if(p.buildings.size()>64||p.details.size()>8192){error="Structure plan exceeds budget";return false;}
    }
    std::sort(p.buildings.begin(),p.buildings.end(),[](const Building& a,const Building& b){return a.id<b.id;});
    std::sort(p.details.begin(),p.details.end(),[](const DetailPlacement& a,const DetailPlacement& b){return a.id<b.id;});
    out=std::move(p);error.clear();return true;
}
}
