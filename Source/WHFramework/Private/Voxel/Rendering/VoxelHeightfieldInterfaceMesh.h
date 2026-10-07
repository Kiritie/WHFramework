#pragma once

#include "Voxel/Rendering/DWHeightfieldTransitionBuilder.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

// 每次仅准备一个冻结边缘的小区间；Count 和 Build 使用相同断点、材质及水面规则。
class FVoxelHeightfieldInterfaceMesh
{
public:
	void Initialize(const bool bInCount, const uint64 InPieceLimit, const uint64 InGeometryLimit,
		const uint64 InResultCapacity = 0)
	{
		bCount = bInCount; PieceLimit = InPieceLimit; GeometryLimit = InGeometryLimit; ResultCapacity = InResultCapacity;
		EdgeCursor = BatchCount = 0; Position = 0; WindowCells = 4; bEdgeStarted = false; PayloadBytes = 0; Mesh = {};
	}

	uint64 EstimatedBytes() const
	{
		return PayloadBytes + VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(BatchCount);
	}

	bool Advance(const FVoxelFrozenHeightfieldInputs& Inputs,
		const TMap<FVoxelHeightfieldNodeKey, FVoxelHeightfieldTileView>& Views,
		const FVoxelGenerationRuntimeConfig& Config, const FVoxelRegistrySnapshot& Registry,
		const int32 MinimumGroundZ, const double TextureStretch, FVoxelSectionMeshResult& OutMesh,
		bool& bComplete, FString& OutError, const TAtomic<bool>& Cancel)
	{
		bComplete = false;
		if (Cancel.Load()) { OutError = TEXT("Canceled"); return false; }
		if (!Inputs.Edges.IsValidIndex(EdgeCursor))
		{
			if (!bCount)
			{
				if (!Mesh.ValidateAndCache()) { OutError = TEXT("Invalid heightfield interface mesh"); return false; }
				OutMesh = MoveTemp(Mesh);
			}
			bComplete = true; return true;
		}
		const auto* Owner = Views.Find(Inputs.Owner);
		const auto& Original = Inputs.Edges[EdgeCursor];
		const auto* Neighbor = Views.Find(Original.Neighbor);
		if (!Owner || !Neighbor || Neighbor->Step <= 0 || Original.bPending)
		{ OutError = TEXT("DependencyNotReady: heightfield interface frozen side"); return false; }
		if (!bEdgeStarted) { Position = Original.RangeMin; bEdgeStarted = true; }
		auto Edge = Original;
		Edge.RangeMin = Position;
		Edge.RangeMax = int32(FMath::Min<int64>(Original.RangeMax, int64(Position) + int64(WindowCells) * Neighbor->Step));
		const int32 BatchBound = FMath::Min(10 * WindowCells + 10, 6 * Registry.Definitions.Num() + 10);
		const uint64 Scratch = 3 * VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(BatchBound);
		if (Scratch > PieceLimit) { OutError = TEXT("Oversized: heightfield interface Count metadata"); return false; }
		FVoxelSectionMeshResult Plan, Counted;
		{
			FVoxelHeightfieldMesher::FCapacityScope Scope(true, Plan.Batches, BatchBound);
			if (!FVoxelHeightfieldTransitionBuilder::BuildMesh(*Owner, MakeArrayView(&Edge, 1), Views, Config, Registry,
				MinimumGroundZ, TextureStretch, Counted, OutError, &Cancel, &Scope) || !Scope.IsValid()) return false;
		}
		Counted = {};
		const int32 AddedBatches = Plan.Batches.Num();
		const uint64 AddedPayload = Plan.PlannedBytes() - VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(AddedBatches);
		const uint64 NewBatchBytes = VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(BatchCount + AddedBatches);
		const uint64 Peak = Scratch + Plan.Batches.GetAllocatedSize() + AddedPayload +
			VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(BatchCount) + NewBatchBytes;
		if (Peak > PieceLimit)
		{
			if (WindowCells > 1) { WindowCells /= 2; return true; }
			OutError = TEXT("Oversized: minimum heightfield interface Piece peak"); return false;
		}
		if (PayloadBytes + AddedPayload + NewBatchBytes > GeometryLimit)
		{ OutError = TEXT("Oversized: heightfield interface cumulative geometry"); return false; }
		if (!bCount)
		{
			if (PayloadBytes + AddedPayload + NewBatchBytes > ResultCapacity)
			{ OutError = TEXT("Heightfield interface Count/Build capacity mismatch"); return false; }
			FVoxelSectionMeshResult Part;
			{
				FVoxelHeightfieldMesher::FCapacityScope Scope(false, Plan.Batches, BatchBound);
				if (!FVoxelHeightfieldTransitionBuilder::BuildMesh(*Owner, MakeArrayView(&Edge, 1), Views, Config, Registry,
					MinimumGroundZ, TextureStretch, Part, OutError, &Cancel, &Scope) || !Scope.IsValid()) return false;
			}
			Mesh.Batches.Reserve(BatchCount + AddedBatches);
			for (auto& Batch : Part.Batches) Mesh.Batches.Add(MoveTemp(Batch));
		}
		PayloadBytes += AddedPayload; BatchCount += AddedBatches;
		Position = Edge.RangeMax;
		if (Position == Original.RangeMax) { ++EdgeCursor; bEdgeStarted = false; }
		return true;
	}

private:
	FVoxelSectionMeshResult Mesh;
	int32 EdgeCursor = 0, Position = 0, BatchCount = 0, WindowCells = 4;
	uint64 PayloadBytes = 0, PieceLimit = 0, GeometryLimit = 0, ResultCapacity = 0;
	bool bCount = false, bEdgeStarted = false;
};
