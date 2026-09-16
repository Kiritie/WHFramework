#pragma once
#include "CoreMinimal.h"
#include "VoxelShapeTypes.generated.h"
UENUM(BlueprintType)
enum class EVoxelShapeKind : uint8
{
	None,
	FullCube,
	Slab,
	Stair,
	Ladder,
	CrossPlant,
	Pane,
	Door,
	Trapdoor,
	Torch,
	Fluid
};
struct WHFRAMEWORK_API FVoxelShapeQuad
{
	FVector Vertices[4];
	FVector2D UV[4];
	uint8 Face = 0;
	uint8 MaterialFace = 0;
	bool bBoundary = false;
	bool bTwoSided = false;
};
struct WHFRAMEWORK_API FVoxelResolvedShape
{
	TArray<FVoxelShapeQuad> Quads;
	TArray<FBox> CollisionBoxes;
	TArray<FBox> SelectionBoxes;
	TArray<FVoxelShapeQuad> SelectionQuads;
	uint64 Coverage[6][4] = {};
	uint8 OcclusionMask = 0;
	bool Covers(uint8 Face, int32 U, int32 V) const;
};
