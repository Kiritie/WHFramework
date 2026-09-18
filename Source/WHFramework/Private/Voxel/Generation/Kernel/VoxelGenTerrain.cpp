#include "Voxel/Generation/Kernel/VoxelGenTerrain.h"
#include "Voxel/Generation/Kernel/VoxelGenMath.h"
#include <cstdlib>
namespace VoxelGen
{
int32_t Terrain::RawHeight(int32_t x,int32_t y) const
{
    const int64_t c=Noise2(s.seed,x,y,s.continentPeriod,0x1001);
    const int64_t e=Noise2(s.seed,x,y,s.erosionPeriod,0x1002);
    const int64_t m=std::abs(Noise2(s.seed,x,y,s.mountainPeriod,0x1003));
    const int64_t ridge=std::clamp<int64_t>((m-9000)*65536/20000,0,65536);
    const int64_t erosion=std::clamp<int64_t>(32768-e,8192,65536);
    const int64_t h=int64_t(s.base)*65536+c*2*s.continentAmplitude+ridge*erosion/65536*s.mountainAmplitude;
    const int64_t floor=h/65536-(h%65536<0);
    return int32_t(std::clamp<int64_t>(floor,s.minZ+4,s.maxZ-4));
}
Biome Terrain::ResolveBiome(const Column& c) const
{
    if(c.height<s.sea-12) return Biome::Ocean;
    // Altitude alone must not turn every highland into barren mountain. Snow and steepness
    // are resolved before the moisture-driven forest classes.
    if(c.temperature<-12000||(c.height>s.sea+384&&c.temperature<7000)) return Biome::Snow;
    if(c.slopePermille>1450||(c.height>s.sea+480&&c.slopePermille>700)) return Biome::Mountain;
    if(c.temperature>14000&&c.moisture<-11000) return Biome::Badlands;
    if(c.temperature>9000&&c.moisture<-5000) return Biome::Desert;
    if(c.temperature>9000&&c.moisture<3500) return Biome::Savanna;
    if(c.temperature>4000&&c.moisture>14000) return Biome::Rainforest;
    if(c.moisture>11500&&c.height<s.sea+18) return Biome::Wetland;
    if(c.moisture>2500) return Biome::Forest;
    return Biome::Plains;
}
bool Terrain::SolidDensity(I3 p,int32_t h) const
{
    return (int64_t(h)-p.z)*65536+int64_t(Noise(s.seed,p,s.detailPeriod,0x1006))*2*s.detailAmplitude>0;
}
Column Terrain::SampleColumn(int32_t x,int32_t y) const
{
    Column c;
    const int32_t raw=RawHeight(x,y);
    int32_t h=raw;

    // Deterministic stylized river corridor. The water plane follows a much slower field,
    // so upland rivers do not have to cut all the way down to global sea level.
    const int32_t warpX=Noise2(s.seed,x,y,s.riverPeriod*2,0x2001)/128;
    const int32_t warpY=Noise2(s.seed,x,y,s.riverPeriod*2,0x2002)/128;
    const int32_t river=std::abs(Noise2(s.seed,x+warpX,y+warpY,s.riverPeriod,0x2003));
    if(s.riverWidthQ15>0&&river<s.riverWidthQ15)
    {
        const int32_t slow=Noise2(s.seed,x,y,std::min(1048576,s.riverPeriod*4),0x2004);
        const int32_t riverWater=std::clamp(s.sea+slow/1024,s.sea-24,s.sea+48);
        const int32_t maxCut=std::max(64,s.maxSiteCutFill*6);
        if(raw>riverWater-32&&raw<riverWater+maxCut)
        {
            int64_t w=int64_t(s.riverWidthQ15-river)*65536/s.riverWidthQ15;
            w=w*w/65536;
            const int32_t bed=riverWater-s.riverDepth;
            h=int32_t((int64_t(raw)*(65536-w)+int64_t(std::min(raw,bed))*w)/65536);
            if(h<riverWater)
            {
                c.river=true;
                c.water=riverWater;
            }
        }
    }

    // Bounded deterministic lake candidate. The wider validation tolerance is important for
    // large-scale terrain: the previous single maxSiteCutFill check rejected almost every lake.
    const I3 region{FloorDiv(x,s.lakeSpacing),FloorDiv(y,s.lakeSpacing),0};
    const uint64_t ls=Seed(s.seed,region,0x2101);
    if(ls%1000<300)
    {
        const int32_t cx=region.x*s.lakeSpacing+s.lakeSpacing/2+Range(Mix(ls),-s.lakeSpacing/8,s.lakeSpacing/8);
        const int32_t cy=region.y*s.lakeSpacing+s.lakeSpacing/2+Range(Mix(ls+1),-s.lakeSpacing/8,s.lakeSpacing/8);
        const int32_t r=s.lakeRadius,dx=x-cx,dy=y-cy;
        const int64_t rr=int64_t(r)*r,d2=int64_t(dx)*dx+int64_t(dy)*dy;
        if(d2<rr)
        {
            const int32_t center=RawHeight(cx,cy),water=center-3;
            bool site=center>s.sea+6&&center<s.maxZ-2*s.lakeDepth;
            const int32_t edgeTolerance=std::max(8,s.maxSiteCutFill*2);
            for(int a=0;a<8&&site;++a)
            {
                const int ox[]={r,-r,0,0,r*7/10,r*7/10,-r*7/10,-r*7/10};
                const int oy[]={0,0,r,-r,r*7/10,-r*7/10,r*7/10,-r*7/10};
                const int32_t edge=RawHeight(cx+ox[a],cy+oy[a]);
                site=std::abs(edge-center)<=edgeTolerance;
            }
            if(site)
            {
                const int32_t depth=int32_t((rr-d2)*(s.lakeDepth+4)/rr);
                const int32_t shaped=water+4-depth;
                h=d2>rr*81/100?std::max(raw,water+3):std::min(raw,shaped);
                c.explicitBed=true;
                c.lake=h<water;
                if(c.lake)c.water=std::max(c.water,water);
            }
        }
    }

    c.densityHeight=h;
    int32_t top=std::min(s.maxZ-1,h+s.detailAmplitude+2);
    const int32_t bottom=std::max(s.minZ+2,h-s.detailAmplitude-2);
    if(c.explicitBed) c.height=h;
    else {while(top>=bottom&&!SolidDensity({x,y,top},h))--top;c.height=top;}
    if(c.height<s.sea)c.water=std::max(c.water,s.sea);

    // Approximate local slope from the macro terrain, not the noisy one-cell surface. This is
    // stable enough for ecology and prevents all high terrain from collapsing into Mountain.
    constexpr int32_t Step=8;
    int32_t maxDelta=0;
    maxDelta=std::max(maxDelta,std::abs(RawHeight(x+Step,y)-raw));
    maxDelta=std::max(maxDelta,std::abs(RawHeight(x-Step,y)-raw));
    maxDelta=std::max(maxDelta,std::abs(RawHeight(x,y+Step)-raw));
    maxDelta=std::max(maxDelta,std::abs(RawHeight(x,y-Step)-raw));
    c.slopePermille=std::clamp(maxDelta*1000/Step,0,100000);
    c.temperature=Noise2(s.seed,x,y,s.climatePeriod,0x1004)-std::max(0,c.height-s.sea)*20;
    c.temperature=std::clamp(c.temperature,-32768,32767);
    c.moisture=Noise2(s.seed,x,y,s.climatePeriod,0x1005);
    if(c.river||c.lake)c.moisture=std::max(c.moisture,12000);
    c.coast=std::abs(c.height-s.sea)<=8;
    c.biome=ResolveBiome(c);
    return c;
}
bool Terrain::SurfaceRouteCarved(I3 p) const
{
    // One deterministic entrance candidate per coarse XY cell. Each accepted entrance owns a
    // sloping two-segment capsule that starts above the terrain surface and reaches the deep
    // cave field. Sampling is position-only, so the same tunnel crosses Section boundaries.
    const int32_t spacing=std::clamp(s.cavePeriod*4,192,768);
    const int32_t gx=FloorDiv(p.x,spacing),gy=FloorDiv(p.y,spacing);
    const int32_t radius=std::clamp(s.caveWidthQ15/512,3,10);
    for(int oy=-1;oy<=1;++oy)for(int ox=-1;ox<=1;++ox)
    {
        const I3 cell{gx+ox,gy+oy,0};
        const uint64_t seed=Seed(s.seed,cell,0x3201);
        if(seed%1000>=300)continue;
        const int32_t ex=cell.x*spacing+Range(Mix(seed),spacing/4,spacing*3/4);
        const int32_t ey=cell.y*spacing+Range(Mix(seed+1),spacing/4,spacing*3/4);
        const int32_t surface=RawHeight(ex,ey);
        if(surface<=s.minZ+64||surface>=s.maxZ-8)continue;
        const int32_t depth=std::clamp(s.cavePeriod*2,56,192);
        const I3 start{ex,ey,surface+2};
        const I3 bend{ex+Range(Mix(seed+2),-spacing/5,spacing/5),ey+Range(Mix(seed+3),-spacing/5,spacing/5),std::max(s.minZ+20,surface-depth/2)};
        const I3 end{bend.x+Range(Mix(seed+4),-spacing/4,spacing/4),bend.y+Range(Mix(seed+5),-spacing/4,spacing/4),std::max(s.minZ+16,surface-depth)};
        if(InCapsule(p,start,bend,radius)||InCapsule(p,bend,end,radius+1)||InEllipsoid(p,end,{radius*3,radius*2,radius*2}))return true;
    }
    return false;
}
bool Terrain::Carved(I3 p,const Column& c) const
{
    if(p.z<=s.minZ+3||p.z>=c.height+1)return false;
    if(SurfaceRouteCarved(p))return true;
    const int32_t depth=c.height-p.z;
    // Background cave noise is now a secondary network. It remains mostly below the surface;
    // the explicit route above is what guarantees an explorable opening to daylight.
    const int32_t opening=Noise2(s.seed,p.x,p.y,s.cavePeriod*3,0x3004);
    if(depth<10&&opening<26000)return false;
    const int32_t a=std::abs(Noise(s.seed,p,s.cavePeriod,0x3001));
    const int32_t b2=std::abs(Noise(s.seed,p,s.cavePeriod+17,0x3002));
    const int32_t chamber=Noise(s.seed,p,s.chamberPeriod,0x3003);
    return (a<s.caveWidthQ15&&b2<s.caveWidthQ15)||(depth>24&&chamber>s.chamberThresholdQ15);
}
Cell Terrain::Geology(I3 p,const Column& c) const
{
    if(p.z<s.minZ||p.z>=s.maxZ)return 0;
    if(p.z<s.minZ+4)return Pack(b.bedrock);
    if(p.z>c.height)return p.z<=c.water?Pack(b.water):0;
    const int32_t depth=c.height-p.z;
    const I3 aq{FloorDiv(p.x,s.aquiferSpacing),FloorDiv(p.y,s.aquiferSpacing),FloorDiv(p.z,s.aquiferSpacing)};
    const uint64_t as=Seed(s.seed,aq,0x3101);
    if(as%1000<300)
    {
        const I3 center{aq.x*s.aquiferSpacing+s.aquiferSpacing/2,aq.y*s.aquiferSpacing+s.aquiferSpacing/2,aq.z*s.aquiferSpacing+s.aquiferSpacing/2};
        const int32_t r=s.aquiferRadius;
        if(center.z-r>s.minZ+4&&center.z+r+4<c.height)
        {
            if(InEllipsoid(p,center,{r+3,r+3,r/2+3}))
            {
                if(!InEllipsoid(p,center,{r,r,r/2}))return Pack(b.stone);
                return p.z<=center.z?Pack(center.z<=s.lavaCeiling?b.lava:b.water):0;
            }
        }
    }
    const bool protectedBed=(c.water>c.height||c.explicitBed)&&depth<8;
    if(!protectedBed&&Carved(p,c))return 0;
    if(!protectedBed&&!c.explicitBed&&depth<=s.detailAmplitude*2+4&&!SolidDensity(p,c.densityHeight))return 0;
    if(c.biome==Biome::Desert||c.biome==Biome::Badlands||c.biome==Biome::Ocean||c.coast||c.lake||c.river)return Pack(depth<8?b.sand:b.stone);
    if(depth==0)return Pack(c.biome==Biome::Snow?b.snow:b.grass);
    return Pack(depth<8?b.dirt:b.stone);
}
}
