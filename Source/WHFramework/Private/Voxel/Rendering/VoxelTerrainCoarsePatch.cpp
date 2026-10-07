#include "Voxel/Rendering/VoxelTerrainCoarseMesh.h"

#include "Voxel/Task/VoxelBoundedAllocation.h"

namespace
{
	constexpr int32 MaximumPieceVertices = 32768;
	constexpr uint64 MaximumPieceBytes = 8ull * 1024 * 1024;
	constexpr uint64 MaximumPatchBytes = 64ull * 1024 * 1024;
	struct FVertex
	{
		FVector Position;
		FVector Normal;
		FVector2D UV[3];
		FLinearColor Color;
		FProcMeshTangent Tangent;
	};
	using FPolygon = TArray<FVertex, TInlineAllocator<9>>;
	struct FFragment
	{
		FPolygon Polygon;
		int32 NextBox = 0;
	};
	using FNodePtr = TSharedPtr<const FVoxelTerrainCoarsePatchNode, ESPMode::ThreadSafe>;
	struct FWalk
	{
		int32 Range = INDEX_NONE;
		int32 Phase = 0;
		FNodePtr Base;
		FNodePtr Children[2];
		int32 ExclusionCursor = 0;
		bool bCoverageChecked = false;
	};

	FVertex Interpolate(const FVertex& A, const FVertex& B, const double Alpha)
	{
		FVertex Out;
		Out.Position = FMath::Lerp(A.Position, B.Position, Alpha);
		Out.Normal = FMath::Lerp(A.Normal, B.Normal, Alpha).GetSafeNormal();
		for (int32 UV = 0; UV < 3; ++UV) Out.UV[UV] = FMath::Lerp(A.UV[UV], B.UV[UV], Alpha);
		Out.Color = FMath::Lerp(A.Color, B.Color, Alpha);
		Out.Tangent = FProcMeshTangent(FMath::Lerp(A.Tangent.TangentX, B.Tangent.TangentX, Alpha).GetSafeNormal(),
			A.Tangent.bFlipTangentY);
		return Out;
	}

	void Split(const FPolygon& Polygon, const int32 Axis, const double Plane, const bool bMinimum,
		FPolygon& Inside, FPolygon& Outside)
	{
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			const auto& A = Polygon[Index];
			const auto& B = Polygon[(Index + 1) % Polygon.Num()];
			const double DA = (A.Position[Axis] - Plane) * (bMinimum ? 1.0 : -1.0);
			const double DB = (B.Position[Axis] - Plane) * (bMinimum ? 1.0 : -1.0);
			(DA >= 0.0 ? Inside : Outside).Add(A);
			if ((DA >= 0.0) != (DB >= 0.0))
			{
				const FVertex Crossing = Interpolate(A, B, DA / (DA - DB));
				Inside.Add(Crossing);
				Outside.Add(Crossing);
			}
		}
		check(Inside.Num() <= 9 && Outside.Num() <= 9);
	}

	uint64 MeshCapacity(const int32 Count)
	{
		return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionMeshResult>() +
			VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(1) +
			2 * VoxelBoundedAllocation::ArrayBytes<FVector>(Count) +
			3 * VoxelBoundedAllocation::ArrayBytes<FVector2D>(Count) +
			VoxelBoundedAllocation::ArrayBytes<FLinearColor>(Count) +
			VoxelBoundedAllocation::ArrayBytes<FProcMeshTangent>(Count) +
			VoxelBoundedAllocation::ArrayBytes<int32>(Count);
	}
}

uint64 FVoxelTerrainCoarsePatch::GetAllocatedBytes() const
{
	return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainCoarsePatch>() + Roots.GetAllocatedSize() + NewBytes;
}

uint64 FVoxelTerrainCoarsePatch::GetResourceAccountedBytes() const
{
	return GetAllocatedBytes();
}

struct FVoxelTerrainCoarsePatchBuilder::FState
{
	FState(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer)
		: Resources(MoveTemp(InResources)), Consumer(InConsumer)
	{
	}

	bool AllowGrowth(const uint64 InAdditionalBytes)
	{
		if (InAdditionalBytes > MaximumPatchBytes || (Result && Result->GetAllocatedBytes() > MaximumPatchBytes - InAdditionalBytes))
		{ bOutputLimitExceeded = true; return false; }
		return true;
	}

	bool ReserveFragments(const int32 Count)
	{
		if (Fragments.Max() >= Count) return true;
		const int32 Capacity = FMath::Max(16, int32(FMath::RoundUpToPowerOfTwo(uint32(Count))));
		const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<FFragment>(Capacity);
		if (Bytes > MaximumPieceBytes) { bFragmentLimitExceeded = true; return false; }
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Overlap;
		if (Fragments.GetAllocatedSize() && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
			Fragments.GetAllocatedSize(), Overlap)) return false;
		if (!FragmentLease)
		{
			if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, Bytes, FragmentLease)) return false;
		}
		else if (!FragmentLease->Resize(Bytes)) return false;
		Fragments.Reserve(Capacity);
		return true;
	}

	bool Append(const FPolygon& Polygon)
	{
		int32 Count = 0;
		for (int32 Index = 1; Index + 1 < Polygon.Num(); ++Index)
			if (FVector::CrossProduct(Polygon[Index].Position - Polygon[0].Position,
				Polygon[Index + 1].Position - Polygon[0].Position).SizeSquared() > UE_DOUBLE_SMALL_NUMBER) Count += 3;
		if (!Count) return true;
		if (!Building || Building->Batches[0].Mesh.Vertices.Num() + Count > MaximumPieceVertices)
		{
			if (Building && !Building->ValidateAndCache()) return false;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MeshLease;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PieceLease;
			const uint64 PieceBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainCoarsePatchPiece>();
			if (!AllowGrowth(MeshCapacity(0) + PieceBytes)) return false;
			if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged, MeshCapacity(0), MeshLease) ||
				!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged,
					PieceBytes, PieceLease)) return false;
			Building = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSectionMeshResult>(MeshLease);
			Building->Stamp = Geometry->Stamp;
			Building->ResourceLease = MoveTemp(MeshLease);
			Building->Batches.Reserve(1);
			const auto& SourceBatch = Geometry->Source->Batches[Geometry->Ranges[Walk.Last().Range].Batch];
			auto& Batch = Building->Batches.AddDefaulted_GetRef();
			Batch.Group = SourceBatch.Group;
			Batch.Bank = SourceBatch.Bank;
			Batch.MaterialVariant = SourceBatch.MaterialVariant;
			auto Piece = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainCoarsePatchPiece>(PieceLease);
			Piece->MetadataLease = MoveTemp(PieceLease);
			Piece->Mesh = Building;
			if (LastPiece) LastPiece->Next = Piece;
			else FirstPiece = Piece;
			LastPiece = MoveTemp(Piece);
			Result->NewBytes += MeshCapacity(0) + PieceBytes;
		}
		auto& Mesh = Building->Batches[0].Mesh;
		const int32 Required = Mesh.Vertices.Num() + Count;
		if (Mesh.Vertices.Max() < Required)
		{
			const int32 Capacity = FMath::Min(MaximumPieceVertices,
				FMath::Max(32, int32(FMath::RoundUpToPowerOfTwo(uint32(Required)))));
			const uint64 Bytes = MeshCapacity(Capacity);
			if (Bytes > MaximumPieceBytes) return false;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Overlap;
			const uint64 Previous = Building->ResourceLease->Bytes();
			if (!AllowGrowth(Bytes - Previous) || Bytes + Previous + Fragments.GetAllocatedSize() > MaximumPieceBytes)
			{ bOutputLimitExceeded = true; return false; }
			if (Mesh.Vertices.Max() && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, Previous, Overlap)) return false;
			if (!Building->ResourceLease->Resize(Bytes)) return false;
			Mesh.Vertices.Reserve(Capacity);
			Mesh.Normals.Reserve(Capacity);
			Mesh.UV0.Reserve(Capacity);
			Mesh.UV1.Reserve(Capacity);
			Mesh.UV2.Reserve(Capacity);
			Mesh.Colors.Reserve(Capacity);
			Mesh.Tangents.Reserve(Capacity);
			Mesh.Triangles.Reserve(Capacity);
			Result->NewBytes += Bytes - Previous;
		}
		for (int32 Index = 1; Index + 1 < Polygon.Num(); ++Index)
		{
			if (FVector::CrossProduct(Polygon[Index].Position - Polygon[0].Position,
				Polygon[Index + 1].Position - Polygon[0].Position).SizeSquared() <= UE_DOUBLE_SMALL_NUMBER) continue;
			for (const int32 Corner : {0, Index, Index + 1})
			{
				const FVertex& Vertex = Polygon[Corner];
				Mesh.Triangles.Add(Mesh.Vertices.Num());
				Mesh.Vertices.Add((Vertex.Position - Geometry->WorldCellOrigin) / Geometry->CellScale);
				Mesh.Normals.Add(Vertex.Normal);
				Mesh.UV0.Add(Vertex.UV[0]);
				Mesh.UV1.Add(Vertex.UV[1]);
				Mesh.UV2.Add(Vertex.UV[2]);
				Mesh.Colors.Add(Vertex.Color);
				Mesh.Tangents.Add(Vertex.Tangent);
			}
		}
		return true;
	}

	bool CompleteNode(FNodePtr Node)
	{
		Walk.Pop(EAllowShrinking::No);
		if (Walk.IsEmpty()) Result->Roots[RootCursor++] = Node;
		else Walk.Last().Children[Walk.Last().Phase - 1] = Node;
		return true;
	}

	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<const FVoxelTerrainCoarseMesh, ESPMode::ThreadSafe> Geometry;
	TSharedPtr<const FVoxelTerrainCoarsePatch, ESPMode::ThreadSafe> Base;
	TSharedPtr<const FVoxelTerrainCoarseExclusions, ESPMode::ThreadSafe> Exclusions;
	TSharedPtr<FVoxelTerrainCoarsePatch, ESPMode::ThreadSafe> Result;
	TArray<FWalk, TInlineAllocator<64>> Walk;
	TArray<FBox, TInlineAllocator<8>> ChangedBounds;
	TArray<FFragment> Fragments;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> FragmentLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
	TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> Building;
	TSharedPtr<FVoxelTerrainCoarsePatchPiece, ESPMode::ThreadSafe> FirstPiece;
	TSharedPtr<FVoxelTerrainCoarsePatchPiece, ESPMode::ThreadSafe> LastPiece;
	int32 RootCursor = 0;
	int32 TriangleCursor = 0;
	bool bComplete = false;
	bool bFragmentLimitExceeded = false;
	bool bOutputLimitExceeded = false;
};

FVoxelTerrainCoarsePatchBuilder::FVoxelTerrainCoarsePatchBuilder(
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, const FVoxelGenerationConsumerKey& InConsumer)
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FState>(), Lease))
	{
		State = VoxelBoundedAllocation::MakeWorkerShared<FState>(Lease, MoveTemp(InResources), InConsumer);
		State->InputLease = MoveTemp(Lease);
	}
}

FVoxelTerrainCoarsePatchBuilder::~FVoxelTerrainCoarsePatchBuilder()
{
}

bool FVoxelTerrainCoarsePatchBuilder::Initialize(
	TSharedPtr<const FVoxelTerrainCoarseMesh, ESPMode::ThreadSafe> InGeometry,
	TSharedPtr<const FVoxelTerrainCoarsePatch, ESPMode::ThreadSafe> InBase,
	TSharedPtr<const FVoxelTerrainCoarseExclusions, ESPMode::ThreadSafe> InExclusions,
	const TConstArrayView<FBox> InChangedBounds, FString& OutError)
{
	if (!State)
	{
		OutError = TEXT("粗层裁切 Worker 容量尚未授予");
		return false;
	}
	if (State->Geometry || !InGeometry || !InExclusions || !InExclusions->Lease ||
		(InBase && (InBase->Geometry != InGeometry || InBase->Roots.Num() != InGeometry->Roots.Num())) || InChangedBounds.Num() > 8)
	{
		OutError = TEXT("粗层局部裁切输入或冻结根不合法");
		return false;
	}
	State->Geometry = MoveTemp(InGeometry);
	State->Base = MoveTemp(InBase);
	State->Exclusions = MoveTemp(InExclusions);
	State->ChangedBounds.Append(InChangedBounds.GetData(), InChangedBounds.Num());
	OutError.Reset();
	return true;
}

EVoxelTerrainCoarseMeshProgress FVoxelTerrainCoarsePatchBuilder::Advance(
	const int32 InMaximumSteps, const TAtomic<bool>& InCancel, FString& OutError)
{
	if (!State) return EVoxelTerrainCoarseMeshProgress::CapacityBlocked;
	auto& S = *State;
	const auto FragmentBlocked = [&S, &OutError]
	{
		if (!S.bFragmentLimitExceeded) return EVoxelTerrainCoarseMeshProgress::CapacityBlocked;
		OutError = TEXT("粗层局部裁切片段栈超过8MiB上限");
		return EVoxelTerrainCoarseMeshProgress::Failed;
	};
	if (S.bComplete) return EVoxelTerrainCoarseMeshProgress::Complete;
	if (!S.Geometry || InMaximumSteps <= 0) return EVoxelTerrainCoarseMeshProgress::Failed;
	if (!S.Result)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainCoarsePatch>() +
			VoxelBoundedAllocation::ArrayBytes<FNodePtr>(S.Geometry->Roots.Num());
		if (Bytes > MaximumPatchBytes) { OutError = TEXT("CapacityUnachievable: 粗层裁切根超过64MiB"); return EVoxelTerrainCoarseMeshProgress::Failed; }
		if (!S.Resources->Acquire(S.Consumer, EVoxelTerrainResourcePool::Staged, Bytes, Lease))
			return EVoxelTerrainCoarseMeshProgress::CapacityBlocked;
		S.Result = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainCoarsePatch>(Lease);
		S.Result->MetadataLease = MoveTemp(Lease);
		S.Result->Geometry = S.Geometry;
		S.Result->Roots.Reserve(S.Geometry->Roots.Num());
		S.Result->Roots.SetNum(S.Geometry->Roots.Num());
	}
	for (int32 Step = 0; Step < InMaximumSteps; ++Step)
	{
		if (S.bOutputLimitExceeded)
		{
			OutError = TEXT("CapacityUnachievable: 粗层裁切输出或单Piece峰值超过硬上限");
			return EVoxelTerrainCoarseMeshProgress::Failed;
		}
		if (S.bFragmentLimitExceeded)
		{
			OutError = TEXT("粗层局部裁切片段栈超过8MiB上限");
			return EVoxelTerrainCoarseMeshProgress::Failed;
		}
		if (InCancel.Load()) return EVoxelTerrainCoarseMeshProgress::Canceled;
		if (S.Walk.IsEmpty())
		{
			if (S.RootCursor == S.Geometry->Roots.Num())
			{
				S.bComplete = true;
				S.Fragments.Empty();
				S.FragmentLease.Reset();
				return EVoxelTerrainCoarseMeshProgress::Complete;
			}
			S.Walk.Add({S.Geometry->Roots[S.RootCursor], 0, S.Base ? S.Base->Roots[S.RootCursor] : nullptr, {}});
			continue;
		}
		auto& Frame = S.Walk.Last();
		const auto& Range = S.Geometry->Ranges[Frame.Range];
		if (Frame.Phase == 0)
		{
			const bool bAffected = !S.Base || S.ChangedBounds.ContainsByPredicate([&Range](const FBox& Box)
			{
				return Range.WorldCellBounds.Intersect(Box);
			});
			if (!bAffected) { S.CompleteNode(Frame.Base); continue; }
			if (!Frame.bCoverageChecked)
			{
				if (Frame.ExclusionCursor == S.Exclusions->Boxes.Num()) { S.CompleteNode(nullptr); continue; }
				if (!Range.WorldCellBounds.Intersect(S.Exclusions->Boxes[Frame.ExclusionCursor++])) continue;
				Frame.bCoverageChecked = true;
			}
		}
		if (!Range.IsLeaf())
		{
			if (Frame.Phase < 2)
			{
				const int32 Child = Frame.Phase++;
				S.Walk.Add({Range.Children[Child], 0, Frame.Base ? Frame.Base->Children[Child] : nullptr, {}});
				continue;
			}
			if (!Frame.Children[0] && !Frame.Children[1]) { S.CompleteNode(nullptr); continue; }
		}
		else if (S.TriangleCursor < Range.TriangleCount || !S.Fragments.IsEmpty())
		{
			Frame.Phase = 1;
			if (S.Fragments.IsEmpty())
			{
				if (!S.ReserveFragments(1)) return FragmentBlocked();
				FFragment Fragment;
				const auto& Mesh = S.Geometry->Source->Batches[Range.Batch].Mesh;
				const int32 First = (Range.FirstTriangle + S.TriangleCursor++) * 3;
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					const int32 Index = Mesh.Triangles[First + Corner];
					FVertex Vertex;
					Vertex.Position = S.Geometry->WorldCellOrigin + Mesh.Vertices[Index] * S.Geometry->CellScale;
					Vertex.Normal = Mesh.Normals[Index];
					Vertex.UV[0] = Mesh.UV0[Index];
					Vertex.UV[1] = Mesh.UV1[Index];
					Vertex.UV[2] = Mesh.UV2[Index];
					Vertex.Color = Mesh.Colors[Index];
					Vertex.Tangent = Mesh.Tangents[Index];
					Fragment.Polygon.Add(Vertex);
				}
				S.Fragments.Add(MoveTemp(Fragment));
				continue;
			}
			const int32 NextBox = S.Fragments.Last().NextBox;
			if (NextBox == S.Exclusions->Boxes.Num())
			{
				if (!S.Append(S.Fragments.Last().Polygon))
				{
					if (S.bOutputLimitExceeded)
					{ OutError = TEXT("CapacityUnachievable: 粗层裁切输出或单Piece峰值超过硬上限"); return EVoxelTerrainCoarseMeshProgress::Failed; }
					return EVoxelTerrainCoarseMeshProgress::CapacityBlocked;
				}
				S.Fragments.Pop(EAllowShrinking::No);
				continue;
			}
			const FBox& Box = S.Exclusions->Boxes[NextBox];
			FBox FragmentBounds(ForceInit);
			for (const auto& Vertex : S.Fragments.Last().Polygon) FragmentBounds += Vertex.Position;
			if (!FragmentBounds.Intersect(Box)) { ++S.Fragments.Last().NextBox; continue; }
			if (!S.ReserveFragments(S.Fragments.Num() + 5)) return FragmentBlocked();
			FPolygon Inside = MoveTemp(S.Fragments.Last().Polygon);
			S.Fragments.Pop(EAllowShrinking::No);
			for (int32 Plane = 0; Plane < 6 && Inside.Num() >= 3; ++Plane)
			{
				FPolygon NextInside;
				FPolygon Outside;
				Split(Inside, Plane / 2, Plane % 2 ? Box.Max[Plane / 2] : Box.Min[Plane / 2],
					Plane % 2 == 0, NextInside, Outside);
				if (Outside.Num() >= 3) S.Fragments.Add({MoveTemp(Outside), NextBox + 1});
				Inside = MoveTemp(NextInside);
			}
			continue;
		}
		if (Range.IsLeaf() && S.Building && !S.Building->ValidateAndCache())
		{
			OutError = TEXT("粗层局部裁切属性验证失败");
			return EVoxelTerrainCoarseMeshProgress::Failed;
		}
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainCoarsePatchNode>();
		if (!S.AllowGrowth(Bytes)) { OutError = TEXT("CapacityUnachievable: 粗层裁切节点超过64MiB"); return EVoxelTerrainCoarseMeshProgress::Failed; }
		if (!S.Resources->Acquire(S.Consumer, EVoxelTerrainResourcePool::Staged, Bytes, Lease))
			return EVoxelTerrainCoarseMeshProgress::CapacityBlocked;
		auto Node = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainCoarsePatchNode>(Lease);
		Node->MetadataLease = MoveTemp(Lease);
		Node->Range = Frame.Range;
		Node->Children[0] = Frame.Children[0];
		Node->Children[1] = Frame.Children[1];
		Node->FirstPiece = MoveTemp(S.FirstPiece);
		S.Result->NewBytes += Bytes;
		S.Building.Reset();
		S.LastPiece.Reset();
		S.TriangleCursor = 0;
		S.CompleteNode(Node);
	}
	return EVoxelTerrainCoarseMeshProgress::Working;
}

TSharedPtr<const FVoxelTerrainCoarsePatch, ESPMode::ThreadSafe> FVoxelTerrainCoarsePatchBuilder::TakeResult()
{
	return State && State->bComplete ? MoveTemp(State->Result) : nullptr;
}
