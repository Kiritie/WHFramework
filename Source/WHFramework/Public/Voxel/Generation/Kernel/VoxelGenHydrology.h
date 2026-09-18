#pragma once
#include "Voxel/Generation/Kernel/VoxelGenRepairCommon.h"
namespace VoxelRepair  {
    struct DrainageInput  {
        uint32_t width=0,height=0;
        std::vector<int32_t> groundPlane;
        std::vector<uint32_t> rainWeight;
        std::vector<uint8_t> allowed;
        std::map<uint32_t,int32_t> outlets;
        // index -> canonical receiving water plane
    };
    struct DrainageResult  {
        std::vector<int32_t> spillPlane;
        std::vector<int32_t> parent;
        std::vector<uint32_t> settledOrder;
        std::vector<uint64_t> accumulation;
    };
    WHFRAMEWORK_API bool BuildDrainage(const DrainageInput&,DrainageResult&,std::string&,const std::atomic_bool* cancel=nullptr);
    WHFRAMEWORK_API bool TraceDrainage(const DrainageInput&,const DrainageResult&,uint32_t source, std::vector<uint32_t>&,std::string&);
    struct BasinInput  {
        uint32_t width=0,height=0;
        int32_t waterPlane=0;
        std::vector<int32_t> groundPlane;
        std::vector<uint8_t> known,waterMask;
        // face: +X,-X,+Y,-Y. Entries are explicitly validated connectors, not guessed exits.
        std::map<std::pair<uint32_t,uint8_t>,int32_t> connectorPlanes;
    };
    struct BasinCertificate  {
        uint32_t waterCells=0,closedEdges=0,connectorEdges=0;
    };
    WHFRAMEWORK_API bool ValidateBasin(const BasinInput&,BasinCertificate&,std::string&,const std::atomic_bool* cancel=nullptr);
    struct RiverSection  {
        int32_t bedPlane=0,waterPlane=0,groundPlane=0;
        bool wet=false;
    };
    struct RiverShape  {
        int32_t halfWidth=12,bankWidth=20,shoreWidth=48,depth=8,maxCutFill=64;
    };
    WHFRAMEWORK_API bool EvaluateRiverSection(int32_t originalGround,int32_t waterPlane, int32_t distanceCells,const RiverShape&,RiverSection&,std::string&);
}
