#pragma once
#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/Geometry/VoxelShapeTypes.h"
class WHFRAMEWORK_API FVoxelShapeRegistry
{
public:
    void BuildDefaults();
    static bool IsValidState(EVoxelShapeKind Shape,uint16 State);
    const FVoxelResolvedShape* Find(EVoxelShapeKind Shape,uint16 State)const;
    const FVoxelResolvedShape& Get(EVoxelShapeKind Shape,uint16 State)const;
    static uint8 RotateFace(uint8 Face,uint8 Yaw);
private:
    static uint32 Key(EVoxelShapeKind S,uint16 V){return uint32(S)|(uint32(V)<<8);}
    static FVoxelResolvedShape Build(EVoxelShapeKind S,uint16 State);
    TMap<uint32,FVoxelResolvedShape> Templates;
};
