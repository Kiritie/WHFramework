#pragma once

#include "CoreMinimal.h"
#include "Math/MathTypes.h"
#include "Parameter/ParameterTypes.h"

class FSaveGameStorage;

struct FVoxelRegionCoord
{
	int32 X = 0;
	int32 Y = 0;
	int32 Z = 0;

	bool operator==(const FVoxelRegionCoord& Other) const
	{
		return X == Other.X && Y == Other.Y && Z == Other.Z;
	}

	friend uint32 GetTypeHash(const FVoxelRegionCoord& Value)
	{
		return HashCombine(HashCombine(GetTypeHash(Value.X), GetTypeHash(Value.Y)), GetTypeHash(Value.Z));
	}
};

class WHFRAMEWORK_API FVoxelRegionStore
{
public:
	void SetSource(const FGuid& SaveId, int32 Generation, FSaveGameStorage* Storage);
	void Reset();
	bool HasChunk(const FIndex& ChunkIndex) const;
	bool LoadChunk(const FIndex& ChunkIndex, FParameter& OutData) const;
	void StageChunk(const FIndex& ChunkIndex, const FParameter& Data);
	bool WritePendingRegions(const FString& TempGenerationDir);
	void CommitPending();
	void AbortPending();

private:
	static constexpr int32 RegionSize = 8;
	static constexpr uint32 MagicValue = 0x44575652;

	static int32 FloorDiv(int32 Value, int32 Divisor);
	static FVoxelRegionCoord GetRegionCoord(const FIndex& ChunkIndex);
	static FString GetRegionFileName(const FVoxelRegionCoord& Coord);
	FString GetSourceRegionPath(const FVoxelRegionCoord& Coord) const;
	bool ReadRegion(const FString& Path, TMap<FIndex, FParameter>& OutChunks) const;
	bool WriteRegion(const FString& Path, const TMap<FIndex, FParameter>& Chunks) const;

private:
	FGuid SourceSaveId;
	int32 SourceGeneration = 0;
	FSaveGameStorage* SourceStorage = nullptr;
	TMap<FVoxelRegionCoord, TMap<FIndex, FParameter>> PendingReplacements;
};
