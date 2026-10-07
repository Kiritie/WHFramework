#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"
#include "Voxel/Rendering/VoxelFinePage.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

struct WHFRAMEWORK_API FVoxelFineSpatialCapacity
{
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	EVoxelTerrainResourcePool Pool = EVoxelTerrainResourcePool::Input;
	uint64 MetadataBytes = 0;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;

	template<typename KeyType>
	bool GrantSet(int32 InCount, FString& OutError)
	{
		if (!Resources) return true;
		const uint64 Bytes = MetadataBytes + VoxelBoundedAllocation::SetBytes<KeyType>(InCount);
		if (Resources->Acquire(Consumer, Pool, Bytes, Lease)) return true;
		const auto Limits = Resources->Snapshot().Limits;
		const uint64 Limit = Pool == EVoxelTerrainResourcePool::Working ? Limits.WorkingBytes : Limits.InputBytes;
		OutError = Bytes > Limit ? TEXT("Oversized: Fine spatial capacity") : TEXT("CapacityBlocked: Fine spatial capacity");
		return false;
	}
};

// 全部坐标为 25cm cell；空间尺寸不携带采样精度语义。
struct WHFRAMEWORK_API FVoxelFineShardKey
{
	FIntVector Min = FIntVector::ZeroValue;
	int32 SideCells = 32;

	bool operator==(const FVoxelFineShardKey& InOther) const = default;
	FVoxelGenerationBounds Bounds() const;
	bool Validate(FString& OutError) const;
};

WHFRAMEWORK_API uint32 GetTypeHash(const FVoxelFineShardKey& InKey);

struct WHFRAMEWORK_API FVoxelFineWindow
{
	uint64 Serial = 0;
	FIntVector Anchor = FIntVector::ZeroValue;
	FVoxelGenerationBounds Bounds;
	TSet<FVoxelFineShardKey> Shards;
};

struct WHFRAMEWORK_API FVoxelFineWindowSettings
{
	int32 RadiusCells = 256;
	int32 VerticalHalfExtentCells = 256;
	int32 PreferredShardSideCells = 32;
	int32 ShiftStepCells = 128;
	int32 VerticalShiftStepCells = 32;
	int32 GuardMarginCells = 32;
	int32 MaximumShardCount = 32768;
	double MinimumPreparationSeconds = 0.5;
	double MaximumPreparationSeconds = 8.0;

	bool Validate(int32 InCoreRadiusCells, int32 InGuardRadiusCells, FString& OutError) const;
};

template<typename KeyType>
struct TVoxelFineSetDelta
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	TSet<KeyType> Added;
	TSet<KeyType> Retained;
	TSet<KeyType> Removed;

	static TVoxelFineSetDelta Calculate(const TSet<KeyType>& InPrevious, const TSet<KeyType>& InNext)
	{
		TVoxelFineSetDelta Result;
		FString Error;
		const bool bSuccess = TryCalculate(InPrevious, InNext, Result, Error);
		checkf(bSuccess, TEXT("%s"), *Error);
		return Result;
	}

	static bool TryCalculate(const TSet<KeyType>& InPrevious, const TSet<KeyType>& InNext,
		TVoxelFineSetDelta& OutResult, FString& OutError,
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources = nullptr,
		const FVoxelGenerationConsumerKey& InConsumer = {})
	{
		TVoxelFineSetDelta Result;
		const int32 MaximumRetained = FMath::Min(InPrevious.Num(), InNext.Num());
		const uint64 Bytes = VoxelBoundedAllocation::SetBytes<KeyType>(InNext.Num()) +
			VoxelBoundedAllocation::SetBytes<KeyType>(InPrevious.Num()) +
			VoxelBoundedAllocation::SetBytes<KeyType>(MaximumRetained);
		if (InResources && !InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input,
			FMath::Max<uint64>(1, Bytes), Result.ResourceLease))
		{
			OutError = Bytes > InResources->Snapshot().Limits.InputBytes ?
				TEXT("Oversized: Fine set delta") : TEXT("CapacityBlocked: Fine set delta");
			return false;
		}
		Result.Added.Reserve(InNext.Num());
		Result.Removed.Reserve(InPrevious.Num());
		Result.Retained.Reserve(MaximumRetained);
		for (const KeyType& Key : InNext)
		{
			if (InPrevious.Contains(Key))
			{
				Result.Retained.Add(Key);
			}
			else
			{
				Result.Added.Add(Key);
			}
		}
		for (const KeyType& Key : InPrevious)
		{
			if (!InNext.Contains(Key))
			{
				Result.Removed.Add(Key);
			}
		}
		OutResult = MoveTemp(Result);
		OutError.Reset();
		return true;
	}
};

class WHFRAMEWORK_API FVoxelFineSpatial
{
public:
	static int64 FloorDivide(int64 InValue, int64 InDivisor);
	static bool TrySnap(const FIntVector& InCell, int32 InSide, FIntVector& OutCell);
	static double DistanceToBounds(const FVector& InCell, const FVoxelGenerationBounds& InBounds);
	static bool Intersects(const FVoxelGenerationBounds& InFirst, const FVoxelGenerationBounds& InSecond);
	static bool ContainsBounds(const FVoxelGenerationBounds& InOuter, const FVoxelGenerationBounds& InInner);
	static bool BuildWindow(const FIntVector& InAnchor, const FVoxelFineWindowSettings& InSettings,
		int32 InWorldMinZ, int32 InWorldMaxZ, uint64 InSerial, FVoxelFineWindow& OutWindow, FString& OutError,
		FVoxelFineSpatialCapacity* InCapacity = nullptr);
	static bool GatherCore(const FIntVector& InCenter, int32 InRadiusCells, int32 InVerticalHalfExtentCells,
		int32 InWorldMinZ, int32 InWorldMaxZ, TSet<FIntVector>& OutSections, FString& OutError,
		FVoxelFineSpatialCapacity* InCapacity = nullptr);
	static bool GatherSweptGuard(const FVector& InStart, const FVector& InEnd, int32 InRadiusCells,
		int32 InWorldMinZ, int32 InWorldMaxZ, int32 InMaximumSections, TSet<FIntVector>& OutSections, FString& OutError,
		FVoxelFineSpatialCapacity* InCapacity = nullptr);
	static bool GatherAffectedShards(const FVoxelGenerationBounds& InChangedBounds, int32 InSideCells,
		int32 InHaloCells, TSet<FVoxelFineShardKey>& OutKeys, FString& OutError);
	static bool SplitShard(const FVoxelFineShardKey& InParent, TArray<FVoxelFineShardKey>& OutChildren, FString& OutError);
};
