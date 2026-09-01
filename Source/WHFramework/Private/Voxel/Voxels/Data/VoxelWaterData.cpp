#include "Voxel/Voxels/Data/VoxelWaterData.h"

#include "Math/MathHelper.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Voxels/VoxelWater.h"

UVoxelWaterData::UVoxelWaterData()
{
	VoxelType = EVoxelType::Water;
	VoxelClass = UVoxelWater::StaticClass();
	Nature = EVoxelNature::Liquid;
	MeshDatas[0].bCustomMesh = true;
}

float UVoxelWaterData::GetWaterHeight(const FVoxelItem& InVoxelItem) const
{
	if(InVoxelItem.GetVoxelType() != EVoxelType::Water) return -1.f;
	const FVoxelLiquidState State(InVoxelItem.Data);
	if(InVoxelItem.Chunk)
	{
		const FVoxelItem& AboveItem = InVoxelItem.Chunk->GetVoxelComplex(InVoxelItem.Index + FIndex(0, 0, 1));
		if(AboveItem.GetVoxelType() == EVoxelType::Water) return 1.f;
	}
	return State.GetHeight();
}

float UVoxelWaterData::GetWaterCornerHeight(const FVoxelItem& InVoxelItem, int32 InX, int32 InY) const
{
	if(!InVoxelItem.Chunk) return GetWaterHeight(InVoxelItem);
	if(FVoxelLiquidState(InVoxelItem.Data).IsFalling()) return 1.f;
	float Height = 0.f;
	int32 Count = 0;
	bool bUpperFlow = false;
	bool bLowerFlow = false;
	bool bSourceFlow = false;
	const int32 VertexX = InX > 0 ? 1 : 0;
	const int32 VertexY = InY > 0 ? 1 : 0;
	for(int32 X = VertexX - 1; X <= VertexX; ++X)
	{
		for(int32 Y = VertexY - 1; Y <= VertexY; ++Y)
		{
			const FIndex ItemIndex = InVoxelItem.Index + FIndex(X, Y, 0);
			const FVoxelItem& Item = InVoxelItem.Chunk->GetVoxelComplex(ItemIndex);
			if(Item.GetVoxelType() == EVoxelType::Water)
			{
				const FVoxelLiquidState State(Item.Data);
				if(State.IsFalling()) continue;
				bSourceFlow |= State.IsSource();
				Height += GetWaterHeight(Item);
				++Count;
			}
			else if(!Item.IsUnknown())
			{
				const FVoxelItem& UpperItem = InVoxelItem.Chunk->GetVoxelComplex(ItemIndex + FIndex(0, 0, 1));
				const FVoxelItem& LowerItem = InVoxelItem.Chunk->GetVoxelComplex(ItemIndex + FIndex(0, 0, -1));
				if(UpperItem.GetVoxelType() == EVoxelType::Water && !FVoxelLiquidState(UpperItem.Data).IsFalling())
				{
					bUpperFlow = true;
					bSourceFlow |= FVoxelLiquidState(UpperItem.Data).IsSource();
				}
				if(LowerItem.GetVoxelType() == EVoxelType::Water && !FVoxelLiquidState(LowerItem.Data).IsFalling())
				{
					bLowerFlow = true;
					bSourceFlow |= FVoxelLiquidState(LowerItem.Data).IsSource();
				}
			}
		}
	}
	if(bUpperFlow) return bSourceFlow ? 1.f : 1.1f;
	if(bLowerFlow) return bSourceFlow ? 0.f : 0.1f;
	return Count > 0 ? Height / Count : GetWaterHeight(InVoxelItem);
}

bool UVoxelWaterData::ShouldBuildWaterFace(const FVoxelItem& InVoxelItem, EDirectionN InFacing) const
{
	if(InVoxelItem.Chunk && FVoxelLiquidState(InVoxelItem.Data).IsFalling() &&
		(InFacing == EDirectionN::Forward || InFacing == EDirectionN::Right || InFacing == EDirectionN::Backward || InFacing == EDirectionN::Left))
	{
		const FVoxelItem& AdjacentItem = InVoxelItem.Chunk->GetVoxelComplex(InVoxelItem.Index + FMathHelper::DirectionToIndex(InFacing));
		if(AdjacentItem.GetVoxelType() == EVoxelType::Water && !FVoxelLiquidState(AdjacentItem.Data).IsFalling()) return true;
	}
	return !InVoxelItem.Chunk || !InVoxelItem.Chunk->CheckVoxelAdjacent(InVoxelItem, InFacing);
}
