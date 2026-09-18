#pragma once
#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Generation/VoxelGenerationContext.h"
struct WHFRAMEWORK_API FVoxelStreamingSource
{
    FGuid Id;
    FIntVector Center;
    FVector Direction=FVector::ForwardVector;
    int32 RenderRadius=12,CollisionRadius=4,SimulationRadius=3,PreloadRadius=14,VerticalRadius=4;
    bool bRender=true,bCollision=true,bSimulation=true;
};
struct WHFRAMEWORK_API FVoxelSectionDemand
{
    bool bMesh=false,bCollision=false,bSimulation=false;
    double Priority=0;
};
class WHFRAMEWORK_API FVoxelStreaming
{
public:
    static bool Validate(const FVoxelStreamingSource& Source);
    static TMap<FVoxelSectionKey,FVoxelSectionDemand> Compute(const TArray<FVoxelStreamingSource>& Sources,const FVoxelGenerationSettings& Settings);
    static TArray<FVoxelSectionKey> ByPriority(const TMap<FVoxelSectionKey,FVoxelSectionDemand>& Desired);
};
