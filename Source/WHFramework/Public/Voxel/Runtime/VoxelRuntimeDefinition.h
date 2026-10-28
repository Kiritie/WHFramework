#pragma once
#include "UObject/PrimaryAssetId.h"
#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/Geometry/VoxelShapeTypes.h"
#include "Voxel/Rendering/VoxelRenderTypes.h"
class FVoxelShapeRegistry;
struct WHFRAMEWORK_API FVoxelRuntimeDefinition
{
    FName BlockName;
    FPrimaryAssetId AssetID;
    FPrimaryAssetId DropAssetID;
    uint16 TypeId=0;
    EVoxelShapeKind Shape=EVoxelShapeKind::FullCube;
    EVoxelRenderGroup RenderGroup=EVoxelRenderGroup::Opaque;
    bool bSolid=false,bOccludes=false,bReplaceable=false,bBreakable=true;
    int32 BreakMilliseconds=1000,DropCount=1;
    uint16 EntityKind=0;uint8 EntityVariant=0;
    FVoxelRuntimeFaceRef Faces[6];
    FVoxelRuntimeFaceRef UpperFaces[6];
    TMap<uint16, FVoxelResolvedShape> StateGeometry;
    const FVoxelResolvedShape* FindShape(const FVoxelShapeRegistry& InShapes, uint16 InState) const;
    const FVoxelResolvedShape& GetShape(const FVoxelShapeRegistry& InShapes, uint16 InState) const;
    const FVoxelRuntimeFaceRef& Face(uint16 State,uint8 MaterialFace)const;
};
