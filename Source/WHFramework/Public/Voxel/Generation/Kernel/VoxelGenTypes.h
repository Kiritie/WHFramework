#pragma once
#if !defined(VOXEL_GENERATION_STANDALONE)
#include "CoreMinimal.h"
#else
#define WHFRAMEWORK_API
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

// This namespace is a deterministic, value-only evaluator owned by UVoxelModule.
// It is NOT a subsystem, a live world, a scheduler, a save store, or an asset manager.
namespace VoxelGen
{
using Cell = uint32_t;
constexpr int32_t SectionSide = 16;
constexpr int32_t SectionVolume = 4096;
constexpr int32_t CoordinateLimit = 1 << 24;
constexpr uint32_t AlgorithmVersion = 3;
constexpr uint32_t CatalogFormat = 1;
struct I3
{
    int32_t x = 0, y = 0, z = 0;
    bool operator==(const I3& b) const { return x==b.x && y==b.y && z==b.z; }
    bool operator!=(const I3& b) const { return !(*this==b); }
    bool operator<(const I3& b) const { return std::tie(z,y,x)<std::tie(b.z,b.y,b.x); }
    int32_t& operator[](int a) { return a==0?x:a==1?y:z; }
    int32_t operator[](int a) const { return a==0?x:a==1?y:z; }
    I3 operator*(int32_t k) const { return {x*k,y*k,z*k}; }
    I3 operator+(I3 b) const { return {x+b.x,y+b.y,z+b.z}; }
    I3 operator-(I3 b) const { return {x-b.x,y-b.y,z-b.z}; }
};
struct Box
{
    I3 min, max; // Half-open, CELL CORNERS, never center coordinates.
    bool Valid() const { return min.x<max.x && min.y<max.y && min.z<max.z; }
    bool Contains(I3 p) const { return p.x>=min.x&&p.y>=min.y&&p.z>=min.z&&p.x<max.x&&p.y<max.y&&p.z<max.z; }
    bool XY(int32_t x,int32_t y) const { return x>=min.x&&y>=min.y&&x<max.x&&y<max.y; }
    bool Intersects(const Box& b) const { return min.x<b.max.x&&min.y<b.max.y&&min.z<b.max.z&&max.x>b.min.x&&max.y>b.min.y&&max.z>b.min.z; }
};
struct Id
{
    uint64_t high = 0, low = 0;
    bool operator==(const Id& b) const { return high==b.high&&low==b.low; }
    bool operator<(const Id& b) const { return std::tie(high,low)<std::tie(b.high,b.low); }
};
inline uint16_t Symbol(Cell c) { return uint16_t(c); }
inline uint16_t State(Cell c) { return uint16_t(c>>16); }
inline Cell Pack(uint16_t type,uint16_t state=0) { return uint32_t(type)|(uint32_t(state)<<16); }
inline bool Canceled(const std::atomic_bool* c) { return c&&c->load(std::memory_order_relaxed); }
inline bool ValidPosition(I3 p) { return int64_t(p.x)>=-CoordinateLimit&&int64_t(p.x)<CoordinateLimit&&int64_t(p.y)>=-CoordinateLimit&&int64_t(p.y)<CoordinateLimit&&int64_t(p.z)>=-32768&&int64_t(p.z)<32768; }
enum class Biome : uint8_t { None, Plains, Forest, Desert, Snow, Mountain, Ocean, Savanna, Rainforest, Wetland, Badlands };
enum class StructureKind : uint8_t { Castle, Dungeon, TownHouse };
enum class TreeKind : uint8_t { Broadleaf, Conifer };
struct WHFRAMEWORK_API Settings
{
    int32_t seed=1, cellCm=25, minZ=-512, maxZ=2048, sea=64, base=80;
    int32_t continentPeriod=16000, erosionPeriod=8000, mountainPeriod=4000, climatePeriod=24000, detailPeriod=128;
    int32_t continentAmplitude=384, mountainAmplitude=512, detailAmplitude=12;
    int32_t riverPeriod=3000, riverWidthQ15=1000, riverDepth=16;
    int32_t lakeSpacing=768, lakeRadius=96, lakeDepth=24;
    int32_t cavePeriod=96, chamberPeriod=192, caveWidthQ15=2400, chamberThresholdQ15=21000;
    int32_t aquiferSpacing=128, aquiferRadius=28, lavaCeiling=-320;
    int32_t structureSpacing=3072, structureChancePermille=600, maxSiteCutFill=16;
    bool Validate(std::string& error) const;
};
struct Column
{
    int32_t height=0, densityHeight=0, temperature=0, moisture=0, water=std::numeric_limits<int32_t>::min();
    Biome biome=Biome::Plains;
    bool river=false, lake=false, coast=false, explicitBed=false;
};
struct CellWrite { I3 p; Cell value=0; };
struct DetailPlacement
{
    Id id;
    uint32_t asset=0; // Catalog detail ordinal, NOT runtime ISM instance index.
    I3 cell;
    uint8_t yaw=0;
};
struct QueryBudget
{
    uint32_t maxStructures=64, maxTrees=2048, maxOreAnchors=65536;
    uint64_t maxCatalogBytes=64ull*1024*1024;
};
}
