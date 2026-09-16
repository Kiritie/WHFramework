#pragma once
#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "Voxel/Geometry/VoxelShapeTypes.h"
#include "Voxel/Rendering/VoxelRenderTypes.h"
#include "Voxel/Runtime/VoxelBlockState.h"
struct WHFRAMEWORK_API FVoxelRuntimeDefinition
{
	FName BlockName;
	FPrimaryAssetId AssetID;
	FPrimaryAssetId DropAssetID;
	uint16 TypeId = 0;
	EVoxelShapeKind Shape = EVoxelShapeKind::FullCube;
	EVoxelRenderGroup RenderGroup = EVoxelRenderGroup::Opaque;
	bool bSolid = false;
	bool bOccludes = false;
	bool bReplaceable = false;
	bool bBreakable = true;
	int32 BreakMilliseconds = 1000;
	int32 DropCount = 1;
	uint16 EntityKind = 0;
	uint8 EntityVariant = 0;
	FVoxelRuntimeFaceRef Faces[6];
	FVoxelRuntimeFaceRef UpperFaces[6];
	const FVoxelRuntimeFaceRef& Face(uint16 State, uint8 MaterialFace) const;
};
