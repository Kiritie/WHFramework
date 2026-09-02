
#include "Voxel/Generators/VoxelLiquidGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Voxels/Data/VoxelData.h"

void UVoxelLiquidGenerator::Generate(UVoxelChunk* InChunk)
{
	GenerateLiquid(InChunk);
}

void UVoxelLiquidGenerator::GenerateLiquid(UVoxelChunk* InChunk) const
{
	constexpr int32 LiquidRange = FVoxelLiquidState::MaxLevel;
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	int32 LiquidMaxHeight = Module->GetWorldData().SeaLevel + 1;
	for(int32 X = -LiquidRange - 1; X <= ChunkSize.X + LiquidRange; ++X)
	{
		for(int32 Y = -LiquidRange - 1; Y <= ChunkSize.Y + LiquidRange; ++Y)
		{
			const FIndex ColumnIndex(X, Y, 0);
			const bool bOnTheChunk = InChunk->IsOnTheChunk(ColumnIndex);
			UVoxelChunk* Chunk = bOnTheChunk ? InChunk : Module->GetChunkByVoxelIndex(InChunk->LocalIndexToWorld(ColumnIndex));
			if(!Chunk || (!bOnTheChunk && Chunk->GetBuildStage() != Stage)) continue;
			const FVoxelTopography& Topography = Chunk->GetTopography(Chunk->WorldIndexToLocal(InChunk->LocalIndexToWorld(ColumnIndex)));
			LiquidMaxHeight = FMath::Max(LiquidMaxHeight, Topography.Height + 2);
			if(Topography.WaterHeight != INDEX_NONE) LiquidMaxHeight = FMath::Max(LiquidMaxHeight, Topography.WaterHeight + 1);
		}
	}
	LiquidMaxHeight = FMath::Min(LiquidMaxHeight, Module->GetWorldData().SkyHeight);
	FVoxelLiquidSnapshotGrid LiquidSnapshots(FIndex(-LiquidRange, -LiquidRange, 0), FIndex(ChunkSize.X + LiquidRange * 2, ChunkSize.Y + LiquidRange * 2, LiquidMaxHeight));
	for(int32 X = -LiquidRange; X < ChunkSize.X + LiquidRange; ++X)
	{
		for(int32 Y = -LiquidRange; Y < ChunkSize.Y + LiquidRange; ++Y)
		{
			const FIndex ColumnIndex(X, Y, 0);
			const bool bOnTheChunk = InChunk->IsOnTheChunk(ColumnIndex);
			UVoxelChunk* Chunk = bOnTheChunk ? InChunk : Module->GetChunkByVoxelIndex(InChunk->LocalIndexToWorld(ColumnIndex));
			if(!Chunk || (!bOnTheChunk && Chunk->GetBuildStage() != Stage)) continue;
			for(int32 Z = 0; Z < LiquidMaxHeight; ++Z)
			{
				const FIndex VoxelIndex(X, Y, Z);
				const FIndex WorldIndex = InChunk->LocalIndexToWorld(VoxelIndex);
				const FVoxelItem Item = bOnTheChunk ? InChunk->GetVoxel(VoxelIndex) : Chunk->GetVoxelSnapshot(Chunk->WorldIndexToLocal(WorldIndex));
				FVoxelLiquidSnapshot& Snapshot = *LiquidSnapshots.Find(VoxelIndex);
				Snapshot.bGenerated = true;
				if(Item.IsValid() && !Item.IsUnknown())
				{
					Snapshot.VoxelType = Item.GetVoxelType();
					Snapshot.Data = Item.Data;
					Snapshot.bCanFlowThrough = Item.GetData().Nature == EVoxelNature::Foliage || Item.GetData().Nature == EVoxelNature::SemiFoliage;
				}
				else
				{
					Snapshot.VoxelType = EVoxelType::Empty;
				}
			}
		}
	}

	const TMap<FIndex, FVoxelLiquidUpdate> LiquidUpdates = UVoxelModuleStatics::CalculateVoxelLiquidUpdates(LiquidSnapshots, TSet<FIndex>());
	for(const auto& Iter : LiquidUpdates)
	{
		const bool bOnTheChunk = InChunk->IsOnTheChunk(Iter.Key);
		const FIndex WorldIndex = InChunk->LocalIndexToWorld(Iter.Key);
		UVoxelChunk* Chunk = bOnTheChunk ? InChunk : Module->GetChunkByVoxelIndex(WorldIndex);
		if(!Chunk || (!bOnTheChunk && (Chunk->GetBuildStage() != Stage || Iter.Value.bRemove))) continue;
		const FIndex LocalIndex = Chunk->WorldIndexToLocal(WorldIndex);
		if(Iter.Value.bRemove)
		{
			Chunk->SetVoxel(LocalIndex, FVoxelItem::Empty, true);
		}
		else
		{
			FVoxelItem Item = bOnTheChunk ? InChunk->GetVoxel(LocalIndex) : Chunk->GetVoxelSnapshot(LocalIndex);
			if(Item.GetVoxelType() != EVoxelType::Water) Item = FVoxelItem(EVoxelType::Water);
			Item.Data = Iter.Value.Data;
			Chunk->SetVoxel(LocalIndex, Item);
		}
	}
}
