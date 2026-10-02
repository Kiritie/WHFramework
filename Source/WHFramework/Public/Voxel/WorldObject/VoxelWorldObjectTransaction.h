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
	static bool HasSupport(const FVoxelWorldObjectDefinitionRuntime& InDefinition, const FIntVector& InAnchor, uint8 InYaw, const FVoxelRegistrySnapshot& InBlocks, const FVoxelShapeRegistry& InShapes, TFunctionRef<bool(const FIntVector&, FVoxelBlockState&)> InRead, bool& OutReady);
	static bool BuildRemoval(const FVoxelWorldRuntime& InWorld, const FVoxelRegistrySnapshot& InBlocks, const FVoxelWorldObjectRegistry& InObjects, const FVoxelWorldObjectInstance& InObject, FVoxelInteractionPlan& OutPlan, FString& OutError);
};
