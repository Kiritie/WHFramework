#pragma once
#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Generation/VoxelGenerationContext.h"
struct WHFRAMEWORK_API FVoxelStreamingSource
{
	FGuid Id;
	FIntVector Center;
	FVector Direction = FVector::ForwardVector;
	int32 RenderRadius = 12;
	int32 CollisionRadius = 4;
	int32 SimulationRadius = 3;
	int32 PreloadRadius = 14;
	int32 VerticalRadius = 4;
	bool bRender = true;
	bool bCollision = true;
	bool bSimulation = true;
};
struct WHFRAMEWORK_API FVoxelSectionDemand
{
	bool bMesh = false;
	bool bCollision = false;
	bool bSimulation = false;
	double Priority = 0;
};
class WHFRAMEWORK_API FVoxelStreaming
{
public:
	static bool Validate(const FVoxelStreamingSource& Source);
	static TMap<FVoxelSectionKey, FVoxelSectionDemand> Compute(const TArray<FVoxelStreamingSource>& Sources, const FVoxelGenerationSettings& Settings);
	static TArray<FVoxelSectionKey> ByPriority(const TMap<FVoxelSectionKey, FVoxelSectionDemand>& Desired);
};
