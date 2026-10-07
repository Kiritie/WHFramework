#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/Save/VoxelDeltaTypes.h"

class FVoxelTerrainResourceLease;

struct WHFRAMEWORK_API FVoxelOverlaySnapshot
{
	// 租约最后析构，覆盖容器和实体Payload的完整销毁过程。
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> IndexLease;
	FIntVector Section = FIntVector::ZeroValue;
	uint64 Revision = 0;
	uint64 BasePersistedRevision = 0;
	// 目录和在途任务共享不可变提交版本，显式Air不从编辑集中删除。
	TMap<int32, FVoxelBlockState> Blocks;
	TMap<int32, FVoxelBlockEntityState> Entities;
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
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	TMap<FIntVector, FVoxelOverlaySnapshot> Sections;
	TMap<FIntVector, TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>> PinnedSections;

	virtual bool EnumerateModifiedSections(const FVoxelGenerationBounds& InBounds,
		TArray<FIntVector>& OutSections, const TAtomic<bool>* InCancel = nullptr) const override;
	virtual bool ReadOverlay(const FIntVector& InSection, FVoxelOverlaySnapshot& OutOverlay) const override;
	void ApplyAt(const FIntVector& InCell, FVoxelBlockState& InOutState) const;
	const FVoxelOverlaySnapshot* Find(const FIntVector& InSection) const;
	int32 Num() const;
	void Visit(TFunctionRef<void(const FIntVector&, const FVoxelOverlaySnapshot&)> InVisit) const;
	uint64 GetAllocatedBytes() const;
};
