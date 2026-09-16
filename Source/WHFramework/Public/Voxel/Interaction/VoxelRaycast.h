#pragma once
#include "CoreMinimal.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
enum class EVoxelTraceStatus : uint8
{
	Hit,
	Miss,
	NeedsData,
	Invalid
};
struct WHFRAMEWORK_API FVoxelTraceResult
{
	EVoxelTraceStatus Status = EVoxelTraceStatus::Miss;
	FIntVector Index = FIntVector::ZeroValue;
	FIntVector PlacementIndex = FIntVector::ZeroValue;
	FVector Point = FVector::ZeroVector;
	FVector Normal = FVector::ZeroVector;
	double Distance = 0;
	FVoxelBlockState State;
	bool bStartedInside = false;
};
class WHFRAMEWORK_API FVoxelRaycast
{
public:
	static FVoxelTraceResult Trace(const FVoxelWorldRuntime& World,
	                               const FVoxelRegistrySnapshot& Registry,
	                               const FVoxelShapeRegistry& Shapes,
	                               const FVector& Start,
	                               const FVector& Direction,
	                               double MaxDistance,
	                               double BlockSize);
};
