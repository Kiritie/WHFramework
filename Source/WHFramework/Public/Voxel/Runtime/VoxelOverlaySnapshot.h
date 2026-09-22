#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"
#include "Voxel/Runtime/VoxelBlockState.h"

struct WHFRAMEWORK_API FVoxelOverlaySnapshot
{
	FIntVector Section = FIntVector::ZeroValue;
	uint64 Revision = 0;
	TMap<int32, FVoxelBlockState> Blocks;
};

class WHFRAMEWORK_API IVoxelOverlaySource
{
public:
	virtual ~IVoxelOverlaySource() = default;
	virtual bool EnumerateModifiedSections(const FVoxelGenerationBounds& InBounds,
		TArray<FIntVector>& OutSections, const TAtomic<bool>* InCancel = nullptr) const = 0;
	virtual bool ReadOverlay(const FIntVector& InSection, FVoxelOverlaySnapshot& OutOverlay) const = 0;
};

struct WHFRAMEWORK_API FVoxelOverlaySnapshotSet final : public IVoxelOverlaySource
{
	TMap<FIntVector, FVoxelOverlaySnapshot> Sections;

	virtual bool EnumerateModifiedSections(const FVoxelGenerationBounds& InBounds,
		TArray<FIntVector>& OutSections, const TAtomic<bool>* InCancel = nullptr) const override;
	virtual bool ReadOverlay(const FIntVector& InSection, FVoxelOverlaySnapshot& OutOverlay) const override;
	void ApplyAt(const FIntVector& InCell, FVoxelBlockState& InOutState) const;
	uint64 GetAllocatedBytes() const;
};
