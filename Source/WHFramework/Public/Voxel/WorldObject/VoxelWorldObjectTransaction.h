#pragma once

#include "CoreMinimal.h"
#include "Voxel/Interaction/VoxelEditTransaction.h"
#include "Voxel/WorldObject/VoxelWorldObjectRegistry.h"

class WHFRAMEWORK_API FVoxelWorldObjectTransaction
{
public:
	static bool Build(const FVoxelWorldRuntime& InWorld, const FVoxelRegistrySnapshot& InBlocks, const FVoxelShapeRegistry& InShapes, const FVoxelWorldObjectRegistry& InObjects, const FVoxelTraceResult& InHit, EVoxelEditAction InAction, uint16 InPlaceType, const FVector& InView, FVoxelInteractionPlan& OutPlan, FString& OutError);
	static bool Validate(const FVoxelWorldRuntime& InWorld, const FVoxelRegistrySnapshot& InBlocks, const FVoxelShapeRegistry& InShapes, const FVoxelWorldObjectRegistry& InObjects, TArray<FVoxelCellEdit>& InOutCells, FString& OutError);
	static const FVoxelBlockEntityState* FindEntity(const FVoxelWorldRuntime& InWorld, const FIntVector& InAnchor);
};
