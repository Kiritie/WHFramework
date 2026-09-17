#pragma once
#include "Voxel/Generation/Kernel/VoxelGenTypes.h"
namespace VoxelGen
{
struct Palette
{
    uint16_t stone=1,dirt=2,grass=3,sand=4,snow=5,water=6,lava=7,bedrock=8,road=9;
};
struct TreeRule
{
    std::string key;
    TreeKind kind=TreeKind::Broadleaf;
    uint32_t biomeMask=0xffffffffu;
    uint16_t trunk=0,leaves=0;
    int32_t spacing=80,minHeight=96,maxHeight=160,trunkRadius=4,crownRadius=32;
    int32_t chancePermille=650,maxSlope=12;
};
struct PlantRule
{
    std::string key;
    uint32_t biomeMask=0xffffffffu;
    uint16_t block=0;
    int32_t chancePermille=100,patchPeriod=64;
};
struct OreRule
{
    std::string key;
    uint16_t block=0;
    int32_t minZ=-480,maxZ=160,spacing=48,radius=3,length=16,chancePermille=600;
    uint32_t biomeMask=0xffffffffu;
};
struct Run
{
    I3 start;
    int32_t length=0;
    Cell value=0;
};
struct DetailDefinition
{
    std::string key,assetPath;
    uint64_t geometryHash=0;
    Box footprint; // In target-grid cells. Full record is part of catalog hash.
};
struct TemplateDetail
{
    uint32_t asset=0;
    I3 position; // Source-grid coordinates, not centimeters.
    uint8_t yaw=0;
};
struct WHFRAMEWORK_API StructureTemplate
{
    std::string key;
    StructureKind kind=StructureKind::TownHouse;
    int32_t sourceCellCm=100,groundZ=0;
    Box bounds;
    I3 entrance; // Explicit source-cell CORNER at doorway foot level; TownHouse faces -Y.
    std::vector<Run> writes;
    std::vector<Box> clear;
    std::vector<TemplateDetail> details;
    Cell Sample(I3 source,bool& writes) const;
};
struct WHFRAMEWORK_API Catalog
{
    std::vector<std::string> blocks; // [0] MUST be core:air; all others unique stable block names.
    Palette palette;
    std::vector<TreeRule> trees;
    std::vector<PlantRule> plants;
    std::vector<OreRule> ores;
    std::vector<StructureTemplate> structures;
    std::vector<DetailDefinition> details;
    bool Validate(const Settings& settings,std::string& error) const;
};
WHFRAMEWORK_API bool EncodeCatalog(const Catalog& catalog,std::vector<uint8_t>& out,std::string& error);
WHFRAMEWORK_API bool DecodeCatalog(const std::vector<uint8_t>& bytes,Catalog& out,std::string& error);
WHFRAMEWORK_API uint64_t SettingsHash(const Settings& settings,uint64_t catalogHash);
}
