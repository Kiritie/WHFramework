#include "Voxel/Navigation/VoxelNavigationTypes.h"

#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Runtime/VoxelRegistry.h"

bool FVoxelNavigationSurfaceBuilder::Build(const FVoxelSectionSnapshot& InSection, const FVoxelSectionSnapshot* InAbove,
	const FVoxelRegistrySnapshot& InRegistry, const FVoxelShapeRegistry& InShapes,
	double InCellSize, double InClearance, FVoxelNavigationSurface& OutSurface, const TAtomic<bool>* InCancel)
{
	if (InCellSize <= 0.0 || InClearance <= 0.0 || InSection.GridSide != 16)
	{
		return false;
	}
	FVoxelNavigationSurface Result;
	const int32 ClearanceCells = FMath::CeilToInt(InClearance / InCellSize);
	auto Read = [&](const FIntVector& InLocal, FVoxelBlockState& OutState)
	{
		if (InLocal.Z >= 16)
		{
			return InAbove && InLocal.Z < 32 && InAbove->TrySample(InLocal - FIntVector(0, 0, 16), OutState);
		}
		return InLocal.Z >= 0 && InSection.TrySample(InLocal, OutState);
	};
	for (int32 Height = 0; Height < 16; ++Height)
	{
		if (InCancel && InCancel->Load())
		{
			return false;
		}
		for (int32 Row = 0; Row < 16; ++Row)
		{
			for (int32 Column = 0; Column < 16; ++Column)
			{
				const FIntVector Cell(Column, Row, Height);
				FVoxelBlockState State;
				if (!Read(Cell, State))
				{
					continue;
				}
				const FVoxelRuntimeDefinition* Block = InRegistry.Find(State.TypeId);
				if (!Block || !Block->bSolid || Block->Shape == EVoxelShapeKind::Fluid || Block->Shape == EVoxelShapeKind::Door)
				{
					continue;
				}
				bool bClear = true;
				for (int32 Above = 1; Above <= ClearanceCells; ++Above)
				{
					FVoxelBlockState Overhead;
					if (!Read(Cell + FIntVector(0, 0, Above), Overhead))
					{
						bClear = false;
						break;
					}
					const FVoxelRuntimeDefinition* OverheadBlock = InRegistry.Find(Overhead.TypeId);
					if (!OverheadBlock || OverheadBlock->Shape == EVoxelShapeKind::Fluid ||
						(OverheadBlock->bSolid && !InShapes.Get(OverheadBlock->Shape, Overhead.State).CollisionBoxes.IsEmpty()))
					{
						bClear = false;
						break;
					}
				}
				if (!bClear)
				{
					continue;
				}
				for (const FVoxelShapeQuad& Quad : InShapes.Get(Block->Shape, State.State).Quads)
				{
					if (Quad.Face != 4)
					{
						continue;
					}
					const int32 FirstVertex = Result.Vertices.Num();
					for (const FVector& Vertex : Quad.Vertices)
					{
						const FVector Position = (FVector(Cell) + Vertex) * InCellSize;
						Result.Vertices.Add(Position);
						Result.Bounds += Position;
					}
					Result.Indices.Append({FirstVertex, FirstVertex + 1, FirstVertex + 2, FirstVertex, FirstVertex + 2, FirstVertex + 3});
				}
			}
		}
	}
	OutSurface = MoveTemp(Result);
	return true;
}
