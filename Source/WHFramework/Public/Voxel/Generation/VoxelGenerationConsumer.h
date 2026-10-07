#pragma once

#include "CoreMinimal.h"

enum class EVoxelGenerationConsumerKind : uint8
{
	None,
	F0Section,
	FineShard,
	VoxelProxy,
	Heightfield,
	Spawn,
	Environment,
	Interaction,
	WorldFact
};

enum class EVoxelGenerationSpatialKind : uint8
{
	None,
	Section,
	Bounds,
	Column
};

/** Complete consumer identity. Hashes are lookup accelerators, never identity. */
struct WHFRAMEWORK_API FVoxelGenerationConsumerKey
{
	EVoxelGenerationConsumerKind Kind = EVoxelGenerationConsumerKind::None;
	uint64 WorldEpoch = 0;
	FGuid OwnerId;
	uint64 OwnerIncarnation = 0;
	EVoxelGenerationSpatialKind SpatialKind = EVoxelGenerationSpatialKind::None;
	FIntVector Min = FIntVector::ZeroValue;
	FIntVector Max = FIntVector::ZeroValue;
	uint64 RequestSerial = 0;

	bool operator==(const FVoxelGenerationConsumerKey& InOther) const = default;

	bool IsValid() const
	{
		if (Kind == EVoxelGenerationConsumerKind::None || WorldEpoch == 0 || !OwnerId.IsValid() ||
			OwnerIncarnation == 0 || RequestSerial == 0)
		{
			return false;
		}
		if (SpatialKind == EVoxelGenerationSpatialKind::Section || SpatialKind == EVoxelGenerationSpatialKind::Column)
		{
			return Max == Min;
		}
		return SpatialKind == EVoxelGenerationSpatialKind::Bounds &&
			Min.X < Max.X && Min.Y < Max.Y && Min.Z < Max.Z;
	}

	static FVoxelGenerationConsumerKey Section(uint64 InEpoch, const FGuid& InOwner,
		uint64 InIncarnation, const FIntVector& InSection, uint64 InSerial)
	{
		return {EVoxelGenerationConsumerKind::F0Section, InEpoch, InOwner, InIncarnation,
			EVoxelGenerationSpatialKind::Section, InSection, InSection, InSerial};
	}
};

FORCEINLINE uint32 GetTypeHash(const FVoxelGenerationConsumerKey& InKey)
{
	uint32 Hash = HashCombineFast(GetTypeHash(static_cast<uint8>(InKey.Kind)), GetTypeHash(InKey.WorldEpoch));
	Hash = HashCombineFast(Hash, GetTypeHash(InKey.OwnerId));
	Hash = HashCombineFast(Hash, GetTypeHash(InKey.OwnerIncarnation));
	Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(InKey.SpatialKind)));
	Hash = HashCombineFast(Hash, GetTypeHash(InKey.Min));
	Hash = HashCombineFast(Hash, GetTypeHash(InKey.Max));
	return HashCombineFast(Hash, GetTypeHash(InKey.RequestSerial));
}
