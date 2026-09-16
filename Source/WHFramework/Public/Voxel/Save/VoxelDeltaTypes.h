#pragma once
#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Runtime/VoxelBlockState.h"
struct WHFRAMEWORK_API FVoxelBlockEntityState
{
	uint16 Kind = 0;
	uint16 Schema = 1;
	TArray<uint8> Payload;
	bool operator==(const FVoxelBlockEntityState& B) const
	{
		return Kind == B.Kind && Schema == B.Schema && Payload == B.Payload;
	}
};
struct WHFRAMEWORK_API FVoxelSectionOverlay
{
	FVoxelSectionKey Key;
	uint64 Revision = 0;
	TMap<uint16, FVoxelBlockState> Blocks;
	TMap<uint16, FVoxelBlockEntityState> Entities;
};
struct WHFRAMEWORK_API FVoxelCellWrite
{
	uint16 Index = 0;
	FVoxelBlockState State;
};
struct WHFRAMEWORK_API FVoxelEntityWrite
{
	uint16 Index = 0;
	bool bRemove = false;
	FVoxelBlockEntityState Value;
};
struct WHFRAMEWORK_API FVoxelSectionPatch
{
	FVoxelSectionKey Key;
	uint64 FromRevision = 0;
	uint64 ToRevision = 0;
	TArray<FVoxelCellWrite> Blocks;
	TArray<FVoxelEntityWrite> Entities;
};
struct WHFRAMEWORK_API FVoxelEditBatch
{
	FGuid TransactionId;
	TArray<FVoxelSectionPatch> Sections;
};
struct WHFRAMEWORK_API FVoxelCellEdit
{
	FIntVector Position;
	FVoxelBlockState Expected;
	FVoxelBlockState Value;
};
struct WHFRAMEWORK_API FVoxelEntityEdit
{
	FIntVector Position;
	bool bRemove = false;
	FVoxelBlockEntityState Value;
};
