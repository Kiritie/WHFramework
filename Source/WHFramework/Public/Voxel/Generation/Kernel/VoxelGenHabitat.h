#pragma once
#include "Voxel/Generation/Kernel/VoxelGenRepairCommon.h"
namespace VoxelRepair  {
    enum class Habitat:uint8_t  {
        Ocean,Wetland,Grassland,TemperateForest,ConiferForest,Desert,Alpine,Snow,BareRock
    };
    struct Environment  {
        int32_t temperatureQ15=0,moistureQ15=0,altitudeCm=0,snowLineCm=30000;
        int32_t slopePermille=0;
        bool underWater=false,nearWater=false;
    };
    struct Ecology  {
        Habitat habitat=Habitat::Grassland;
        uint16_t treePermille=0,plantPermille=0;
    };
    WHFRAMEWORK_API bool ResolveHabitat(const Environment&,Ecology&,std::string&);
    enum Content:uint64_t  {
        Terrain=1ull<<0,Climate=1ull<<1,Ocean=1ull<<2,River=1ull<<3,Lake=1ull<<4, Cave=1ull<<5,Ore=1ull<<6,Trees=1ull<<7,Plants=1ull<<8,Castle=1ull<<9, Dungeon=1ull<<10,Town=1ull<<11,Detail=1ull<<12,Lava=1ull<<13
    };
    constexpr uint64_t RequiredContent=(1ull<<14)-1;
    struct ContentInventory  {
        uint64_t enabled=0,witnessed=0;
        uint32_t treeRules=0,plantRules=0,oreRules=0,castles=0,dungeons=0,townHouses=0,details=0;
    };
    WHFRAMEWORK_API bool ValidateDefaultContent(const ContentInventory&,std::string&);
}
