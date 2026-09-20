#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

using FVoxelCaveColumnSampler =
	TFunctionRef<
		bool(
			const FIntVector&,
			FVoxelColumnSample&)>;

struct WHFRAMEWORK_API FVoxelCaveSegment
{
	FIntVector Start = FIntVector::ZeroValue;
	FIntVector End = FIntVector::ZeroValue;
	int32 Radius = 2;

	bool IsValid() const
	{
		return Radius > 0;
	}
};

struct WHFRAMEWORK_API FVoxelCavePlan
{
	TArray<FVoxelCaveSegment> Segments;

	void Finalize();
	bool Carves(const FIntVector& InCell) const;
	bool ProtectsFloor(const FIntVector& InCell) const;
	uint64 GetAllocatedBytes() const;

private:
	TMap<FIntVector, TArray<int32>>
		SegmentIndicesBySection;
};

class WHFRAMEWORK_API FVoxelCaveGenerator
{
public:
	explicit FVoxelCaveGenerator(
		TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe);

	bool BuildPlan(
		const FVoxelGenerationBounds& InBounds,
		FVoxelCaveColumnSampler InColumnSampler,
		FVoxelCavePlan& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

private:
	bool TryBuildSystem(
		const FIntPoint& InAnchorGrid,
		FVoxelCaveColumnSampler InColumnSampler,
		TArray<FVoxelCaveSegment>& OutSegments,
		FString& OutError) const;

	void AddBranch(
		FRandomStream& InStream,
		const FIntVector& InStart,
		double InYaw,
		double InPitch,
		FVoxelCaveColumnSampler InColumnSampler,
		TArray<FVoxelCaveSegment>& OutSegments) const;

	FIntVector ClampBelowSurface(
		const FIntVector& InPosition,
		FVoxelCaveColumnSampler InColumnSampler) const;

private:
	static constexpr int32 SpawnPermille = 650;
	static constexpr int32 EntrancePermille = 820;
	static constexpr int32 BranchPermille = 120;
	static constexpr int32 RoomPermille = 60;
	static constexpr int32 MinimumSegments = 14;
	static constexpr int32 MaximumSegments = 28;
	static constexpr int32 MinimumSegmentLength = 3;
	static constexpr int32 MaximumSegmentLength = 6;
	static constexpr int32 MaximumRoomRadius = 4;

	TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
};
