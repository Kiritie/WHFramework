#pragma once

#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/Generation/VoxelGenerationConsumer.h"
#include "Voxel/Generation/VoxelGenerationPlanningRead.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Task/VoxelTerrainWorkSelection.h"

enum class EVoxelWorkClass : uint8;
struct FVoxelGenerationBounds;
struct FVoxelTaskStamp;
class FVoxelGenerationWorkspace;

struct WHFRAMEWORK_API FVoxelOverlayPreflightStatus
{
	bool bReady = true;
	bool bFailed = false;
	FString Error;
};

/** Immutable, worker-safe project composition applied to an exact generated section. */
class WHFRAMEWORK_API IVoxelGenerationOverlay
{
public:
	virtual ~IVoxelGenerationOverlay() = default;
	virtual const FVoxelTerrainResources* GetResourceOwner() const { return nullptr; }
	virtual void VisitRequiredSources(FVoxelGenerationRequiredSourceScope::FVisit InVisit) const
	{
	}
	virtual void MarkPublished() const
	{
	}

	virtual void SetPlanningServices(FVoxelGenerationPlanningServices InServices) const
	{
	}

	virtual void NotifyPlanningDependencyReady(const FVoxelGenerationConsumerKey& InConsumer) const
	{
	}

	/** 同一构建的完整消费者贯穿预检、唤醒与快照租用；准备完成不撤销订阅。 */
	virtual FVoxelOverlayPreflightStatus EnsureConsumerReady(
		const FVoxelGenerationConsumerKey& InConsumer,
		const FVoxelGenerationBounds& InBounds,
		EVoxelWorkClass InWorkClass,
		const FVoxelTerrainServicePriority& InPriority) const
	{
		return {};
	}

	virtual void ReleaseConsumer(const FVoxelGenerationConsumerKey& InConsumer) const
	{
	}

	virtual void SetTypedConsumerWakeCallback(
		TFunction<void(const FVoxelGenerationConsumerKey&)> InCallback) const
	{
	}

	virtual void SetServicePriorityDonation(
		TFunction<void(const FVoxelTaskStamp&, EVoxelWorkClass, const FVoxelTerrainServicePriority&)> InCallback) const
	{
	}

	virtual void AdvanceConsumerWakes(double InDeadlineSeconds, int32 InLimit) const
	{
	}

	virtual void SetConsumerWakeCallback(TFunction<void(const FIntVector&)> InCallback) const
	{
	}

	virtual void ReleaseSectionConsumer(const FIntVector& InSectionCoordinate) const
	{
	}

	virtual FVoxelOverlayPreflightStatus EnsureBoundsReady(const FVoxelGenerationBounds& InBounds,
		EVoxelWorkClass InWorkClass, int32 InSourcePriority) const
	{
		return {};
	}

	virtual FVoxelOverlayPreflightStatus EnsureCoarseBoundsReady(const FVoxelGenerationBounds& InBounds,
		EVoxelWorkClass InWorkClass, int32 InSourcePriority) const
	{
		return {};
	}

	/** Called on the admission thread before a section occupies a generation worker. */
	virtual FVoxelOverlayPreflightStatus EnsureSectionReady(
		const FIntVector& InSectionCoordinate,
		EVoxelWorkClass InWorkClass,
		int32 InSourcePriority,
		double InDistanceScore,
		double InForwardScore) const
	{
		return {};
	}

	virtual bool ApplySection(
		const FIntVector& InSectionCoordinate,
		TArrayView<FVoxelBlockState> InOutBlocks,
		FString& OutError,
		const TAtomic<bool>* InCancel) const = 0;

	virtual bool BuildCoarse(
		const FIntVector&,
		int32,
		int32,
		TMap<FIntVector, FVoxelBlockState>& OutCells,
		FString& OutError,
		const TAtomic<bool>*,
		FVoxelGenerationWorkspace* InOutputWorkspace = nullptr) const
	{
		OutCells.Reset();
		OutError.Reset();
		return true;
	}
};
