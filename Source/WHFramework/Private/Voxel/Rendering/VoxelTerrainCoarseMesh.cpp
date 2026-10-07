#include "Voxel/Rendering/VoxelTerrainCoarseMesh.h"

#include "Voxel/Task/VoxelBoundedAllocation.h"

namespace
{
	constexpr int32 MaximumLeafTriangles = 32;
}

uint64 FVoxelTerrainCoarseMesh::GetAllocatedBytes() const
{
	return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainCoarseMesh>() + Ranges.GetAllocatedSize() + Roots.GetAllocatedSize();
}

uint64 FVoxelTerrainCoarseMesh::GetResourceAccountedBytes() const
{
	return GetAllocatedBytes();
}

void FVoxelTerrainCoarseMesh::VisitIntersecting(const FBox& InWorldCellBounds, TFunctionRef<void(int32)> InVisit) const
{
	const auto Visit = [&](auto&& Self, const int32 Index) -> void
	{
		const auto& Range = Ranges[Index];
		if (!Range.WorldCellBounds.Intersect(InWorldCellBounds)) return;
		if (Range.IsLeaf()) InVisit(Index);
		else
		{
			Self(Self, Range.Children[0]);
			Self(Self, Range.Children[1]);
		}
	};
	for (const int32 Root : Roots) Visit(Visit, Root);
}

FVoxelTerrainCoarseMeshBuilder::FVoxelTerrainCoarseMeshBuilder(
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, const FVoxelGenerationConsumerKey& InConsumer)
	: Resources(MoveTemp(InResources)), Consumer(InConsumer)
{
}

bool FVoxelTerrainCoarseMeshBuilder::Initialize(TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe> InMesh,
	const FVector& InWorldCellOrigin, const double InCellScale, const FVoxelTaskStamp& InStamp, FString& OutError)
{
	if (Input || !InMesh || !InMesh->bValidated || InWorldCellOrigin.ContainsNaN() ||
		!FMath::IsFinite(InCellScale) || InCellScale <= 0.0 || !InStamp.WorldEpoch || !InStamp.bHasGeometryIdentity)
	{
		OutError = TEXT("粗层共享索引范围输入不合法");
		return false;
	}
	uint64 Nodes = 0;
	for (const auto& Batch : InMesh->Batches)
	{
		const int32 Triangles = Batch.Mesh.Triangles.Num() / 3;
		if (!Triangles) continue;
		const uint64 Leaves = (uint64(Triangles) + MaximumLeafTriangles - 1) / MaximumLeafTriangles;
		const uint64 LeafSlots = FMath::RoundUpToPowerOfTwo64(Leaves);
		Nodes += LeafSlots * 2 - 1;
		++MaximumRoots;
	}
	if (Nodes > MAX_int32)
	{
		OutError = TEXT("粗层共享索引范围节点数溢出");
		return false;
	}
	MaximumNodes = int32(Nodes);
	Input = MoveTemp(InMesh);
	Stamp = InStamp;
	Origin = InWorldCellOrigin;
	Scale = InCellScale;
	OutError.Reset();
	return true;
}

EVoxelTerrainCoarseMeshProgress FVoxelTerrainCoarseMeshBuilder::Advance(const int32 InMaximumSteps,
	const TAtomic<bool>& InCancel, FString& OutError)
{
	if (bComplete) return EVoxelTerrainCoarseMeshProgress::Complete;
	if (!Input || InMaximumSteps <= 0) return EVoxelTerrainCoarseMeshProgress::Failed;
	if (InCancel.Load()) return EVoxelTerrainCoarseMeshProgress::Canceled;
	if (!Result)
	{
		const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainCoarseMesh>() +
			VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainCoarseRange>(MaximumNodes) +
			VoxelBoundedAllocation::ArrayBytes<int32>(MaximumRoots);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged, Bytes, Lease))
			return EVoxelTerrainCoarseMeshProgress::CapacityBlocked;
		Result = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainCoarseMesh>(Lease);
		Result->MetadataLease = MoveTemp(Lease);
		Result->Stamp = Stamp;
		Result->Source = Input;
		Result->WorldCellOrigin = Origin;
		Result->CellScale = Scale;
		Result->Ranges.Reserve(MaximumNodes);
		Result->Roots.Reserve(MaximumRoots);
	}
	for (int32 Step = 0; Step < InMaximumSteps; ++Step)
	{
		if (InCancel.Load()) return EVoxelTerrainCoarseMeshProgress::Canceled;
		if (Walk.IsEmpty())
		{
			if (!Input->Batches.IsValidIndex(BatchCursor))
			{
				bComplete = true;
				Input.Reset();
				return EVoxelTerrainCoarseMeshProgress::Complete;
			}
			const int32 Batch = BatchCursor++;
			const int32 Triangles = Input->Batches[Batch].Mesh.Triangles.Num() / 3;
			if (!Triangles) continue;
			const int32 Root = Result->Ranges.AddDefaulted();
			Result->Roots.Add(Root);
			Result->Ranges[Root].Batch = Batch;
			Result->Ranges[Root].TriangleCount = Triangles;
			Walk.Add({Root, 0, false});
			continue;
		}
		FWalk& Frame = Walk.Last();
		FVoxelTerrainCoarseRange& Range = Result->Ranges[Frame.Node];
		if (Range.TriangleCount > MaximumLeafTriangles)
		{
			if (!Frame.bExpanded)
			{
				const int32 LeftCount = Range.TriangleCount / 2;
				const int32 FirstChild = Result->Ranges.AddDefaulted(2);
				check(Result->Ranges.Num() <= MaximumNodes);
				Range.Children[0] = FirstChild;
				Range.Children[1] = FirstChild + 1;
				for (int32 Child = 0; Child < 2; ++Child)
				{
					auto& NewRange = Result->Ranges[FirstChild + Child];
					NewRange.Batch = Range.Batch;
					NewRange.FirstTriangle = Range.FirstTriangle + (Child ? LeftCount : 0);
					NewRange.TriangleCount = Child ? Range.TriangleCount - LeftCount : LeftCount;
				}
				Frame.bExpanded = true;
				Walk.Add({FirstChild + 1, 0, false});
				Walk.Add({FirstChild, 0, false});
			}
			else
			{
				Range.WorldCellBounds = Result->Ranges[Range.Children[0]].WorldCellBounds +
					Result->Ranges[Range.Children[1]].WorldCellBounds;
				Walk.Pop(EAllowShrinking::No);
			}
			continue;
		}
		if (Frame.Triangle == Range.TriangleCount)
		{
			Walk.Pop(EAllowShrinking::No);
			continue;
		}
		const auto& Mesh = Input->Batches[Range.Batch].Mesh;
		const int32 First = (Range.FirstTriangle + Frame.Triangle++) * 3;
		for (int32 Corner = 0; Corner < 3; ++Corner)
			Range.WorldCellBounds += Origin + Mesh.Vertices[Mesh.Triangles[First + Corner]] * Scale;
	}
	return EVoxelTerrainCoarseMeshProgress::Working;
}

TSharedPtr<const FVoxelTerrainCoarseMesh, ESPMode::ThreadSafe> FVoxelTerrainCoarseMeshBuilder::TakeResult()
{
	return bComplete ? MoveTemp(Result) : nullptr;
}
