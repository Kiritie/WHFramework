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
    if(c.height<s.sea-16) return Biome::Ocean;
    if(c.height>s.base+s.mountainAmplitude/2 && c.height>s.sea+96) return Biome::Mountain;
    if(c.temperature< -11000) return Biome::Snow;
    if(c.temperature>14000&&c.moisture< -11000) return Biome::Badlands;
    if(c.temperature>9000&&c.moisture< -5000) return Biome::Desert;
    if(c.temperature>9000&&c.moisture<3500) return Biome::Savanna;
    if(c.temperature>4000&&c.moisture>14000) return Biome::Rainforest;
    if(c.moisture>12000&&c.height<s.sea+12) return Biome::Wetland;
    if(c.moisture>3500) return Biome::Forest;
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
    // Stylized lowland river corridors. No promise of erosion simulation or global drainage.
    const int32_t warpX=Noise2(s.seed,x,y,s.riverPeriod*2,0x2001)/128;
    const int32_t warpY=Noise2(s.seed,x,y,s.riverPeriod*2,0x2002)/128;
    const int32_t river=std::abs(Noise2(s.seed,x+warpX,y+warpY,s.riverPeriod,0x2003));
    if(s.riverWidthQ15>0&&river<s.riverWidthQ15&&raw>s.sea-32&&raw<s.sea+96)
    {
        int64_t w=int64_t(s.riverWidthQ15-river)*65536/s.riverWidthQ15;
        w=w*w/65536;
        const int32_t bed=s.sea-s.riverDepth;
        h=int32_t((int64_t(raw)*(65536-w)+int64_t(std::min(raw,bed))*w)/65536);
        c.river=h<s.sea;
    }
    // One bounded lake candidate per world lattice cell; no neighboring live chunks are read.
    const I3 region{FloorDiv(x,s.lakeSpacing),FloorDiv(y,s.lakeSpacing),0};
    const uint64_t ls=Seed(s.seed,region,0x2101);
    if(ls%1000<220)
    {
        const int32_t cx=region.x*s.lakeSpacing+s.lakeSpacing/2+Range(Mix(ls),-s.lakeSpacing/8,s.lakeSpacing/8);
        const int32_t cy=region.y*s.lakeSpacing+s.lakeSpacing/2+Range(Mix(ls+1),-s.lakeSpacing/8,s.lakeSpacing/8);
        const int32_t r=s.lakeRadius,dx=x-cx,dy=y-cy;
        const int64_t rr=int64_t(r)*r,d2=int64_t(dx)*dx+int64_t(dy)*dy;
        if(d2<rr)
        {
            const int32_t center=RawHeight(cx,cy),water=center-4;
            bool site=center>s.sea+12&&center<s.maxZ-2*s.lakeDepth;
            for(int a=0;a<8&&site;++a)
            {
                const int ox[]={r,-r,0,0,r*7/10,r*7/10,-r*7/10,-r*7/10};
                const int oy[]={0,0,r,-r,r*7/10,-r*7/10,r*7/10,-r*7/10};
                const int32_t edge=RawHeight(cx+ox[a],cy+oy[a]);
                site=std::abs(edge-center)<=s.maxSiteCutFill;
            }
            if(site)
            {
                const int32_t depth=int32_t((rr-d2)*(s.lakeDepth+4)/rr);
                // The outer annulus is an explicit solid rim; the inner basin is carved.
                const int32_t shaped=water+4-depth;
                h=d2>rr*81/100?std::max(raw,water+3):std::min(raw,shaped);
                c.explicitBed=true;
                c.lake=h<water;
                if(c.lake)c.water=water;
            }
        }
    }
    c.densityHeight=h;
    int32_t top=std::min(s.maxZ-1,h+s.detailAmplitude+2);
    const int32_t bottom=std::max(s.minZ+2,h-s.detailAmplitude-2);
    // Lakes use explicit bed/rim heights to avoid density detail puncturing the rim.
    if(c.explicitBed) c.height=h;
    else {while(top>=bottom&&!SolidDensity({x,y,top},h))--top;c.height=top;}
    if(c.height<s.sea) c.water=std::max(c.water,s.sea);
    c.temperature=Noise2(s.seed,x,y,s.climatePeriod,0x1004)-std::max(0,c.height-s.sea)*20;
    c.temperature=std::clamp(c.temperature,-32768,32767);
    c.moisture=Noise2(s.seed,x,y,s.climatePeriod,0x1005);
    c.coast=std::abs(c.height-s.sea)<=8;
    c.biome=ResolveBiome(c);
    return c;
}
bool Terrain::Carved(I3 p,const Column& c) const
{
    if(p.z<=s.minZ+3||p.z>=c.height+1) return false;
    const int32_t depth=c.height-p.z;
    const int32_t opening=Noise2(s.seed,p.x,p.y,s.cavePeriod*3,0x3004);
    if(depth<12&&opening<18000) return false;
    const int32_t a=std::abs(Noise(s.seed,p,s.cavePeriod,0x3001));
    const int32_t b2=std::abs(Noise(s.seed,p,s.cavePeriod+17,0x3002));
    const int32_t chamber=Noise(s.seed,p,s.chamberPeriod,0x3003);
    return (a<s.caveWidthQ15&&b2<s.caveWidthQ15)||(depth>24&&chamber>s.chamberThresholdQ15);
}
Cell Terrain::Geology(I3 p,const Column& c) const
{
    if(p.z<s.minZ||p.z>=s.maxZ) return 0;
    if(p.z<s.minZ+4) return Pack(b.bedrock);
    if(p.z>c.height)
        return p.z<=c.water?Pack(b.water):0;
    const int32_t depth=c.height-p.z;
    // Bounded sealed aquifer / lava pockets: inner cavity + a 3-cell solid shell.
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
                if(!InEllipsoid(p,center,{r,r,r/2})) return Pack(b.stone);
                return p.z<=center.z?Pack(center.z<=s.lavaCeiling?b.lava:b.water):0;
            }
        }
    }
    const bool protectedBed=(c.water>c.height||c.explicitBed)&&depth<8;
    if(!protectedBed&&Carved(p,c))return 0;
    // Detail gives actual overhangs near a surface; do not re-evaluate the lake bed as height noise.
    if(!protectedBed&&!c.explicitBed && depth<=s.detailAmplitude*2+4&&!SolidDensity(p,c.densityHeight))return 0;
    if(c.biome==Biome::Desert||c.biome==Biome::Badlands||c.biome==Biome::Ocean||c.coast||c.lake||c.river) return Pack(depth<8?b.sand:b.stone);
    if(depth==0) return Pack(c.biome==Biome::Snow?b.snow:b.grass);
    return Pack(depth<8?b.dirt:b.stone);
}
}
