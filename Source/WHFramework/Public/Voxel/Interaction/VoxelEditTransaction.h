#pragma once
#include "CoreMinimal.h"
#include "Voxel/Interaction/VoxelEditTypes.h"
#include "Voxel/Interaction/VoxelRaycast.h"
class WHFRAMEWORK_API FVoxelEditTransaction
{
public:
    static bool ValidateBatch(const FVoxelWorldRuntime& World,const FVoxelRegistrySnapshot& Registry,
        const FVoxelShapeRegistry& Shapes,TArray<FVoxelCellEdit>& Cells,FString& Error);
    static bool Build(const FVoxelWorldRuntime& World,const FVoxelRegistrySnapshot& Registry,const FVoxelShapeRegistry& Shapes,
        const FVoxelTraceResult& Hit,EVoxelEditAction Action,uint16 PlaceType,const FVector& ViewDirection,double BlockSize,
        FVoxelInteractionPlan& Out,FString& Error);
};
