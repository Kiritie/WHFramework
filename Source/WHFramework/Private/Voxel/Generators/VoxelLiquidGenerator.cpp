
#include "Voxel/Generators/VoxelLiquidGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Misc/ScopeLock.h"

bool UVoxelLiquidGenerator::IsIndexBefore(const FIndex& A, const FIndex& B)
{
	if(A.X != B.X) return A.X < B.X;
	if(A.Y != B.Y) return A.Y < B.Y;
	return A.Z < B.Z;
}

void UVoxelLiquidGenerator::PrepareBatch(const TArray<FIndex>& InChunkIndices)
{
	BatchSnapshots.Reset();
	BatchUpdates.Reset();
	TArray<FIndex> ChunkIndices = InChunkIndices;
	ChunkIndices.Sort(IsIndexBefore);
	for(const FIndex& ChunkIndex : ChunkIndices)
	{
		if(UVoxelChunk* Chunk = Module->GetChunkByIndex(ChunkIndex))
		{
			BatchSnapshots.Add(ChunkIndex, CreateChunkSnapshot(Chunk));
		}
	}
}

void UVoxelLiquidGenerator::Generate(UVoxelChunk* InChunk)
{
	CalculateLiquidUpdates(InChunk);
}

void UVoxelLiquidGenerator::CompleteBatch(bool bCancelled)
{
	if(!bCancelled) ApplyBatchUpdates();
	BatchUpdates.Reset();
	BatchSnapshots.Reset();
}

TSharedPtr<const FVoxelLiquidSnapshotGrid, ESPMode::ThreadSafe> UVoxelLiquidGenerator::CreateChunkSnapshot(UVoxelChunk* InChunk) const
{
	constexpr int32 LiquidRange = FVoxelLiquidState::MaxLevel;
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	int32 LiquidMaxHeight = Module->GetWorldData().SeaLevel + 1;
	for(int32 X = -LiquidRange - 1; X <= ChunkSize.X + LiquidRange; ++X)
	{
		for(int32 Y = -LiquidRange - 1; Y <= ChunkSize.Y + LiquidRange; ++Y)
		{
			const FIndex ColumnIndex(X, Y, 0);
			const FIndex WorldIndex = InChunk->LocalIndexToWorld(ColumnIndex);
			UVoxelChunk* Chunk = InChunk->IsOnTheChunk(ColumnIndex) ? InChunk : Module->GetChunkByVoxelIndex(WorldIndex);
			if(!Chunk || Chunk->GetBuildStage() < Stage - 1) continue;
			const FVoxelTopography& Topography = Chunk->GetTopography(Chunk->WorldIndexToLocal(WorldIndex));
			LiquidMaxHeight = FMath::Max(LiquidMaxHeight, Topography.Height + 2);
			if(Topography.WaterHeight != INDEX_NONE) LiquidMaxHeight = FMath::Max(LiquidMaxHeight, Topography.WaterHeight + 1);
		}
	}
	LiquidMaxHeight = FMath::Min(LiquidMaxHeight, Module->GetWorldData().SkyHeight);
	TSharedRef<FVoxelLiquidSnapshotGrid, ESPMode::ThreadSafe> LiquidSnapshots = MakeShared<FVoxelLiquidSnapshotGrid, ESPMode::ThreadSafe>(
		FIndex(-LiquidRange, -LiquidRange, 0), FIndex(ChunkSize.X + LiquidRange * 2, ChunkSize.Y + LiquidRange * 2, LiquidMaxHeight));
	for(int32 X = -LiquidRange; X < ChunkSize.X + LiquidRange; ++X)
	{
		for(int32 Y = -LiquidRange; Y < ChunkSize.Y + LiquidRange; ++Y)
		{
			const FIndex ColumnIndex(X, Y, 0);
			const FIndex ColumnWorldIndex = InChunk->LocalIndexToWorld(ColumnIndex);
			UVoxelChunk* Chunk = InChunk->IsOnTheChunk(ColumnIndex) ? InChunk : Module->GetChunkByVoxelIndex(ColumnWorldIndex);
			if(!Chunk || Chunk->GetBuildStage() < Stage - 1) continue;
			for(int32 Z = 0; Z < LiquidMaxHeight; ++Z)
			{
				const FIndex VoxelIndex(X, Y, Z);
				const FIndex WorldIndex = InChunk->LocalIndexToWorld(VoxelIndex);
				const FVoxelItem Item = Chunk->GetVoxelSnapshot(Chunk->WorldIndexToLocal(WorldIndex));
				FVoxelLiquidSnapshot& Snapshot = *LiquidSnapshots->Find(VoxelIndex);
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
	return LiquidSnapshots;
}

void UVoxelLiquidGenerator::CalculateLiquidUpdates(UVoxelChunk* InChunk)
{
	if(!InChunk) return;
	const TSharedPtr<const FVoxelLiquidSnapshotGrid, ESPMode::ThreadSafe>* Snapshots = BatchSnapshots.Find(InChunk->GetIndex());
	if(!Snapshots || !Snapshots->IsValid()) return;
	TMap<FIndex, FVoxelLiquidUpdate> LiquidUpdates = UVoxelModuleStatics::CalculateVoxelLiquidUpdates(**Snapshots, TSet<FIndex>());
	for(auto Iter = LiquidUpdates.CreateIterator(); Iter; ++Iter)
	{
		if(!InChunk->IsOnTheChunk(Iter.Key())) Iter.RemoveCurrent();
	}
	FScopeLock ScopeLock(&BatchUpdatesCriticalSection);
	BatchUpdates.Add(InChunk->GetIndex(), MoveTemp(LiquidUpdates));
}

void UVoxelLiquidGenerator::ApplyBatchUpdates()
{
	TArray<FIndex> ChunkIndices;
	BatchUpdates.GetKeys(ChunkIndices);
	ChunkIndices.Sort(IsIndexBefore);
	for(const FIndex& ChunkIndex : ChunkIndices)
	{
		UVoxelChunk* Chunk = Module->GetChunkByIndex(ChunkIndex);
		const TMap<FIndex, FVoxelLiquidUpdate>* LiquidUpdates = BatchUpdates.Find(ChunkIndex);
		if(!Chunk || !LiquidUpdates) continue;
		TArray<FIndex> LocalIndices;
		LiquidUpdates->GetKeys(LocalIndices);
		LocalIndices.Sort(IsIndexBefore);
		for(const FIndex& LocalIndex : LocalIndices)
		{
			const FVoxelLiquidUpdate& Update = LiquidUpdates->FindChecked(LocalIndex);
			if(Update.bRemove)
			{
				Chunk->SetVoxel(LocalIndex, FVoxelItem::Empty, true);
			}
			else
			{
				FVoxelItem Item = Chunk->GetVoxel(LocalIndex);
				if(Item.GetVoxelType() != EVoxelType::Water) Item = FVoxelItem(EVoxelType::Water);
				Item.Data = Update.Data;
				Chunk->SetVoxel(LocalIndex, Item);
			}
		}
	}
}
