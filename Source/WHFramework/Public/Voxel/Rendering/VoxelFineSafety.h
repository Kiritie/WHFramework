#pragma once

#include "CoreMinimal.h"
#include "Voxel/Streaming/VoxelInterestRuntime.h"
#include "Voxel/Rendering/VoxelTerrainRenderer.h"
#include "Voxel/Collision/VoxelCollisionPresenter.h"
#include "Voxel/Runtime/VoxelChangeHierarchy.h"

class FVoxelChangeHierarchy;

// 既有 Scheduler 的有界 continuation；只读两个实际回执根，不读取 UObject。
class WHFRAMEWORK_API FVoxelFineFeedbackCapture
{
public:
	FVoxelFineFeedbackCapture(TSharedRef<const FVoxelInterestSet, ESPMode::ThreadSafe> InInterest,
		TSharedPtr<const IVoxelTerrainPresentedSnapshot, ESPMode::ThreadSafe> InVisual,
		TSharedPtr<const FVoxelCollisionCoverageSnapshot, ESPMode::ThreadSafe> InPhysics,
		const FVoxelChangeHierarchy& InHierarchy, const FVoxelWorldManifest& InManifest,
		uint64 InShapeRevision, uint64 InRootSerial,
		TSharedRef<FVoxelFineResidencyFeedback, ESPMode::ThreadSafe> InFeedback);
	bool Advance(int32 InMaximumShards, const TAtomic<bool>& InCancel);
	TSharedRef<const FVoxelFineResidencyFeedback, ESPMode::ThreadSafe> GetFeedback() const;
	uint64 GetRenderSerial() const;
	uint64 GetPhysicsSerial() const;
	const FVoxelChangeHierarchySnapshot& GetVersions() const { return Hierarchy; }

private:
	FVoxelFineSectionCoverage CaptureSection(const FIntVector& InSection);
	TSharedRef<const FVoxelInterestSet, ESPMode::ThreadSafe> Interest;
	TSharedPtr<const IVoxelTerrainPresentedSnapshot, ESPMode::ThreadSafe> Visual;
	TSharedPtr<const FVoxelCollisionCoverageSnapshot, ESPMode::ThreadSafe> Physics;
	FVoxelChangeHierarchySnapshot Hierarchy;
	FVoxelWorldManifest Manifest;
	uint64 ShapeRevision = 0;
	TSharedRef<FVoxelFineResidencyFeedback, ESPMode::ThreadSafe> Feedback;
	int32 ShardCursor = 0;
	int32 RuntimeCursor = 0;
};
