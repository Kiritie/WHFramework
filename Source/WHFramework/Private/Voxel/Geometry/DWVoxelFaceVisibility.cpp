#include "Voxel/Geometry/DWVoxelFaceVisibility.h"

#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Runtime/VoxelRegistry.h"

namespace
{
	bool CoversQuad(const FVoxelResolvedShape& InShape,
		const uint8 InFace, const FVoxelShapeQuad* InQuad)
	{
		if (!InQuad) return (InShape.OcclusionMask & (1u << InFace)) != 0;
		const int32 Axis = InFace / 2;
		const int32 UAxis = (Axis + 1) % 3;
		const int32 VAxis = (Axis + 2) % 3;
		double UMin = 1.0;
		double UMax = 0.0;
		double VMin = 1.0;
		double VMax = 0.0;
		for (const FVector& Vertex : InQuad->Vertices)
		{
			UMin = FMath::Min(UMin, Vertex[UAxis]);
			UMax = FMath::Max(UMax, Vertex[UAxis]);
			VMin = FMath::Min(VMin, Vertex[VAxis]);
			VMax = FMath::Max(VMax, Vertex[VAxis]);
		}
		const int32 FirstU = FMath::Clamp(FMath::FloorToInt(UMin * 16 + 1.e-6), 0, 15);
		const int32 LastU = FMath::Clamp(FMath::CeilToInt(UMax * 16 - 1.e-6) - 1, 0, 15);
		const int32 FirstV = FMath::Clamp(FMath::FloorToInt(VMin * 16 + 1.e-6), 0, 15);
		const int32 LastV = FMath::Clamp(FMath::CeilToInt(VMax * 16 - 1.e-6) - 1, 0, 15);
		for (int32 V = FirstV; V <= LastV; ++V)
		{
			for (int32 U = FirstU; U <= LastU; ++U)
			{
				if (!InShape.Covers(InFace, U, V)) return false;
			}
		}
		return true;
	}
}

bool VoxelFaceVisibility::ShouldRender(
	const FVoxelRegistrySnapshot& InRegistry,
	const FVoxelShapeRegistry& InShapes,
	const FVoxelBlockState InCurrent,
	const FVoxelBlockState InNeighbor,
	const uint8 InFace,
	const FVoxelShapeQuad* InQuad)
{
	if (InNeighbor.IsAir()) return true;
	const FVoxelRuntimeDefinition* Neighbor = InRegistry.Find(InNeighbor.TypeId);
	const FVoxelRuntimeDefinition* Current = InRegistry.Find(InCurrent.TypeId);
	if (!Neighbor || !Current) return true;
	const bool bSameTransparent = InCurrent.TypeId == InNeighbor.TypeId &&
		(Current->RenderGroup == EVoxelRenderGroup::Water ||
			Current->RenderGroup == EVoxelRenderGroup::Translucent);
	if (!Neighbor->bOccludes && !bSameTransparent) return true;
	const FVoxelResolvedShape* Shape = InShapes.Find(Neighbor->Shape, InNeighbor.State);
	return !Shape || !CoversQuad(*Shape, InFace ^ 1, InQuad);
}
