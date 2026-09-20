#pragma once

#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelBlockState.h"

struct WHFRAMEWORK_API FVoxelBlockEntityState
{
	uint16 Kind = 0;
	uint16 Schema = 1;
	TArray<uint8> Payload;

	bool operator==(const FVoxelBlockEntityState& InOther) const
	{
		return Kind == InOther.Kind &&
			Schema == InOther.Schema &&
			Payload == InOther.Payload;
	}
};

struct WHFRAMEWORK_API FVoxelSectionCellEdit
{
	int32 CellIndex = INDEX_NONE;
	bool bNatural = false;
	FVoxelBlockState State;
};

struct WHFRAMEWORK_API FVoxelEntityWrite
{
	int32 CellIndex = INDEX_NONE;
	bool bRemove = false;
	FVoxelBlockEntityState Value;
};

struct WHFRAMEWORK_API FVoxelSectionPatch
{
	FIntVector Section = FIntVector::ZeroValue;
	uint64 FromRevision = 0;
	uint64 ToRevision = 0;
	TArray<FVoxelSectionCellEdit> Edits;
	TArray<FVoxelEntityWrite> Entities;
};

struct WHFRAMEWORK_API FVoxelEditBatch
{
	FGuid TransactionId;
	TArray<FVoxelSectionPatch> Sections;
};

struct WHFRAMEWORK_API FVoxelPersistentSection
{
	FIntVector Section = FIntVector::ZeroValue;
	uint64 Revision = 0;
	TMap<int32, FVoxelBlockState> Blocks;
	TMap<int32, FVoxelBlockEntityState> Entities;

	bool IsEmpty() const
	{
		return Blocks.IsEmpty() && Entities.IsEmpty();
	}
};

struct WHFRAMEWORK_API FVoxelCellEdit
{
	FIntVector Position = FIntVector::ZeroValue;
	FVoxelBlockState Expected;
	FVoxelBlockState Value;
};

struct WHFRAMEWORK_API FVoxelEntityEdit
{
	FIntVector Position = FIntVector::ZeroValue;
	bool bRemove = false;
	FVoxelBlockEntityState Value;
};
