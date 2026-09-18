#pragma once
#include "Voxel/Rendering/Kernel/VoxelViewDependencies.h"
namespace VoxelRepair  {
    struct SurfaceQuad  {
        uint8_t face=0;
        int32_t plane=0,u=0,v=0,width=0,height=0;
        Cell material=0;
        bool fluid=false;
    };
    struct SurfaceResult  {
        std::vector<SurfaceQuad> quads;
        uint64_t exposedUnitFaces=0;
    };
    enum class SurfaceStatus:uint8_t  {
        Success,Pending,Error,Canceled,Split
    };
    using FinalSampler=std::function<ReadBlock(Point)>;
    // Cube/Fluid semantic baseline. Special shapes are delegated to the existing shape mesher.
    // Bounds are [origin, origin+side). Sampler must supply an independent, known six-face halo.
    WHFRAMEWORK_API SurfaceStatus ExtractSurface(Point origin,int32_t side,const FinalSampler&, SurfaceResult&,std::string& error,uint32_t maxQuads=262144,const std::atomic_bool* cancel=nullptr);
}
