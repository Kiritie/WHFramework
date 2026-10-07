#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/DWVolumeTransitionPlanner.h"
#include "Voxel/Runtime/VoxelBlockState.h"

struct FVoxelSection;
struct FVoxelVoxelProxyData;
struct FVoxelFineShardGeometry;
class FVoxelTerrainResourceLease;

/** 持有实际构建版本的面样本；Worker 不读取可变 Runtime。 */
struct FVoxelBoundaryFaceSnapshot
{
	FVoxelViewKey Key;
	uint8 Face = 0;
	int32 SampleStepCells = 0;
	int32 GridSide = 0;
	uint64 Revision = 0;
	TArray<uint32> States;
	TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> FineGeometry;
	TSharedPtr<const FVoxelVoxelProxyData, ESPMode::ThreadSafe> ProxyGeometry;
	int32 GetStateCount() const;
	bool GetPackedState(int32 InIndex, uint32& OutPacked) const;

	bool Sample(const FIntVector& InWorldCell, FVoxelBlockState& OutState) const;
	bool SameSource(const FVoxelBoundaryFaceSnapshot& InOther) const;

	static bool CaptureFine(const FVoxelViewKey& InKey, uint8 InFace,
		const FVoxelSection& InSection, FVoxelBoundaryFaceSnapshot& OutSnapshot);
	static bool CaptureProxy(const FVoxelVoxelProxyData& InData, uint8 InFace,
		FVoxelBoundaryFaceSnapshot& OutSnapshot);
	static bool CaptureFineShardAtom(const FVoxelViewKey& InKey, uint8 InFace,
		TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> InGeometry,
		FVoxelBoundaryFaceSnapshot& OutSnapshot);
	static bool CaptureProxyFrozen(TSharedPtr<const FVoxelVoxelProxyData, ESPMode::ThreadSafe> InData,
		uint8 InFace, FVoxelBoundaryFaceSnapshot& OutSnapshot);
};

struct FVoxelBoundaryTransitionPatch
{
	FVoxelVolumeTransitionFace Face;
	FVoxelBoundaryFaceSnapshot Neighbor;
};

struct FVoxelBoundaryTransitionContext
{
	FVoxelViewKey Owner;
	int32 SampleStepCells = 0;
	int32 GetSampleStep() const { return SampleStepCells; }
	int32 GetGridSide() const { return SampleStepCells > 0 ? Owner.GetSide() / SampleStepCells : 0; }
	TArray<FVoxelBoundaryTransitionPatch> Patches;
	// The opposite consumer yields its original face to the single interface owner.
	TArray<FVoxelVolumeTransitionFace> SuppressedFaces;
	uint64 SuppressionRevision = 0;
	uint64 InputSerial = 0;
	double FirstQueuedSeconds = 0.0;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	uint64 WorldEpoch = 0;
	uint64 GeneratorIdentity = 0;
	uint64 RegistryRevision = 0;
	uint64 ShapeRevision = 0;
	TSharedPtr<const FVoxelVoxelProxyData, ESPMode::ThreadSafe> OwnerGeometry;
	TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> FineOwnerGeometry;
	double MaximumTextureStretchCells = 8.0;
	TArray<TSharedPtr<const FVoxelVoxelProxyData, ESPMode::ThreadSafe>> SuppressionSources;

	bool CoversCell(uint8 InFace, const FIntVector& InLocalCell) const;
	bool Validate() const;
	bool SameInputs(const FVoxelBoundaryTransitionContext& InOther) const;
	uint64 Signature(uint64 InOwnerRevision) const;
};
