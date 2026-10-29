#include "Voxel/Rendering/VoxelProxyPageMesh.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

bool FVoxelProxyPageMesh::Initialize(const FVoxelTerrainGeometry& InGeometry, FString& OutError,
	const uint64 InRetainedBytes, const bool bInCountOnly, const uint64 InPieceLimit, const uint64 InGeometryLimit)
{
	if (!InGeometry.Validate(OutError))
	{
		return false;
	}
	if (InGeometry.Key.Representation != EVoxelTerrainRepresentation::VoxelProxy || InGeometry.Key.Level == 0)
	{
		OutError = TEXT("Mesh representation does not match its actual geometry");
		return false;
	}
	if (InPieceLimit == 0 || InGeometryLimit == 0)
	{
		OutError = TEXT("Invalid mesh capacity limits");
		return false;
	}
	FVoxelProxyPageMesh Candidate;
	Candidate.Geometry = InGeometry;
	Candidate.RetainedBytes = InRetainedBytes;
	Candidate.PieceLimit = InPieceLimit;
	Candidate.GeometryLimit = InGeometryLimit;
	Candidate.bCountOnly = bInCountOnly;
	if (!VoxelStreamingGeometry::BuildShards(InGeometry, 16, 4096, Candidate.Shards, OutError))
	{
		return false;
	}
	Candidate.Shards.Reserve(4096);
	*this = MoveTemp(Candidate);
	return true;
}

bool FVoxelProxyPageMesh::SplitWindow(FString& OutError)
{
	const auto Shard = Shards[NextShard];
	const int32 Side = Shard.CellCounts.X;
	if (Side <= 1 || Shards.Num() > 4096 - 7)
	{ OutError = TEXT("Oversized: proxy Piece cannot fit the count/build peak"); return false; }
	const int32 Half = Side / 2;
	Shards[NextShard].CellCounts = FIntVector(Half);
	for (int32 Z = 0; Z < 2; ++Z)
		for (int32 Y = 0; Y < 2; ++Y)
			for (int32 X = 0; X < 2; ++X)
				if (X || Y || Z) Shards.Add({Shard.LocalMinCells + FIntVector(X, Y, Z) * Half * Geometry.SampleStepCells, FIntVector(Half)});
	return true;
}

bool FVoxelProxyPageMesh::Advance(const FVoxelVoxelProxyData& InData, const FVoxelRegistrySnapshot& InRegistry,
	const FVoxelShapeRegistry& InShapes, const FVoxelBoundaryTransitionContext* InContext,
	const double InTextureRepeats, FVoxelSectionMeshResult& OutMesh, bool& bOutComplete,
	FString& OutError, const TAtomic<bool>* InCancel)
{
	bOutComplete = false;
	if (InData.Geometry != Geometry || !Geometry.Validate(OutError) ||
		InData.Key != FVoxelViewKey{Geometry.Key.Coordinate, Geometry.Key.Level} ||
		InData.LocalMinCells != FIntVector::ZeroValue || InData.GridSide <= 0 || InData.GridSide > 128 ||
		int64(InData.GridSide) * Geometry.SampleStepCells != Geometry.Key.SideCells() ||
		!InData.Cells.Validate(InData.GridSide) ||
		!FMath::IsFinite(InTextureRepeats) || InTextureRepeats <= 0.0)
	{
		OutError = TEXT("Proxy mesh input does not match its actual geometry");
		return false;
	}
	for (int32 Face = 0; Face < 6; ++Face)
	{
		if (InData.Known[Face] && InData.Halo[Face].Num() != InData.GridSide * InData.GridSide)
		{
			OutError = TEXT("Proxy mesh input has an invalid halo");
			return false;
		}
	}
	if (!Shards.IsValidIndex(NextShard)) { if (!Mesh.ValidateAndCache()) { OutError = TEXT("Invalid completed page mesh"); return false; } OutMesh = MoveTemp(Mesh); bOutComplete = true; return true; }
	const auto Shard = Shards[NextShard];
	const FIntVector Offset = Shard.LocalMinCells / Geometry.SampleStepCells;
	const int32 Side = Shard.CellCounts.X, PageSide = InData.GridSide;
	const auto* Interior = InRegistry.Find(InData.Cells.GetInterior().TypeId);
	if ((InData.Cells.GetInterior().IsAir() || (Interior && Interior->Shape == EVoxelShapeKind::FullCube && Interior->bOccludes)) &&
		!InData.Cells.MayHaveSurface(Offset, Side))
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ProxySkipInteriorWindow);
		++NextShard;
		return true;
	}
	const uint64 ClipScratch = FVoxelSectionMesher::TransitionClipScratchBytes(InContext, Offset, Side);
	if (ClipScratch > PieceLimit) return SplitWindow(OutError);
	const int32 BatchBound = int32(FMath::Min(int64(6) * Side * Side * Side, int64(12) * InRegistry.Definitions.Num()));
	const uint64 CountMetadata = ClipScratch + VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(BatchBound) +
		VoxelBoundedAllocation::MapBytes<uint32, int32>(BatchBound) +
		FVoxelSectionMesher::FaceMaskBytes(Side) +
		VoxelBoundedAllocation::ArrayBytes<uint32>(Side * Side * Side) +
		6 * VoxelBoundedAllocation::ArrayBytes<uint32>(Side * Side) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainBuildShard>(4096);
	if (CountMetadata > PieceLimit) return SplitWindow(OutError);
	FVoxelSectionSnapshot Snapshot;
	Snapshot.GridSide = Side;
	Snapshot.Blocks.Reserve(Side * Side * Side);
	Snapshot.Blocks.SetNumUninitialized(Side * Side * Side);
	const auto Sample = [&InData, PageSide](const FIntVector& Local, uint32& Out)
	{
		int32 Face = INDEX_NONE;
		for (int32 Axis = 0; Axis < 3; ++Axis)
			if (Local[Axis] < 0 || Local[Axis] >= PageSide)
			{ if (Face != INDEX_NONE) return false; Face = Axis * 2 + (Local[Axis] < 0 ? 1 : 0); }
		if (Face == INDEX_NONE) { Out = InData.Cells[Local.X + PageSide * (Local.Y + PageSide * Local.Z)].Pack(); return true; }
		if (!InData.Known[Face]) return false;
		const int32 Axis = Face / 2, U = (Axis + 1) % 3, V = (Axis + 2) % 3;
		Out = InData.Halo[Face][Local[U] + PageSide * Local[V]].Pack();
		return true;
	};
	for (int32 Z = 0; Z < Side; ++Z)
		for (int32 Y = 0; Y < Side; ++Y)
			for (int32 X = 0; X < Side; ++X)
				if (!Sample(Offset + FIntVector(X, Y, Z), Snapshot.Blocks[X + Side * (Y + Side * Z)])) return false;
	for (int32 Face = 0; Face < 6; ++Face)
	{
		const int32 Axis = Face / 2, U = (Axis + 1) % 3, V = (Axis + 2) % 3;
		Snapshot.Halo[Face].Reserve(Side * Side);
		Snapshot.Halo[Face].Init(0, Side * Side);
		Snapshot.Known[Face] = true;
		for (int32 Y = 0; Y < Side; ++Y)
			for (int32 X = 0; X < Side; ++X)
			{
				FIntVector Local = Offset;
				Local[Axis] += (Face & 1) ? -1 : Side;
				Local[U] += X; Local[V] += Y;
				Snapshot.Known[Face] &= Sample(Local, Snapshot.Halo[Face][X + Side * Y]);
			}
	}
	FVoxelSectionMeshResult Plan;
	if (!FVoxelSectionMesher::Build(Snapshot, InRegistry, InShapes, Plan, InCancel, InTextureRepeats,
		InContext, false, false, Offset, PageSide, true))
	{ OutError = TEXT("Failed to mesh proxy execution window"); return false; }
	const uint64 PlanBytes = Plan.PlannedBytes();
	const uint64 PlannedBatchBytes = VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(Plan.Batches.Num());
	const uint64 AddedPayload = PlanBytes - PlannedBatchBytes;
	const uint64 OldBatchBytes = VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(EstimatedBatchCount);
	const uint64 NewBatchBytes = VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(EstimatedBatchCount + Plan.Batches.Num());
	const uint64 ScratchBytes = ClipScratch + Snapshot.Blocks.GetAllocatedSize() + Plan.Batches.GetAllocatedSize() +
		VoxelBoundedAllocation::MapBytes<uint32, int32>(BatchBound) +
		FVoxelSectionMesher::FaceMaskBytes(Side) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainBuildShard>(4096);
	uint64 HaloBytes = 0;
	for (const auto& Halo : Snapshot.Halo) HaloBytes += Halo.GetAllocatedSize();
	if (PlanBytes + ScratchBytes + HaloBytes + OldBatchBytes + NewBatchBytes > PieceLimit)
	{
		return SplitWindow(OutError);
	}
	if (AddedPayload > GeometryLimit || EstimatedPayloadBytes > GeometryLimit - AddedPayload ||
		NewBatchBytes > GeometryLimit - EstimatedPayloadBytes - AddedPayload)
	{ OutError = TEXT("Oversized: proxy geometry exceeds the Shard limit"); return false; }
	EstimatedPayloadBytes += AddedPayload;
	EstimatedBatchCount += Plan.Batches.Num();
	EstimatedMeshBytes = EstimatedPayloadBytes + NewBatchBytes;
	if (bCountOnly) { ++NextShard; return true; }
	const uint64 CombinedBytes = Mesh.AllocatedBytes() - Mesh.Batches.GetAllocatedSize() + PlanBytes -
		VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(Plan.Batches.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(Mesh.Batches.Num() + Plan.Batches.Num());
	if (CombinedBytes > RetainedBytes)
	{ OutError = TEXT("Oversized: proxy geometry exceeds its admitted result capacity"); return false; }
	FVoxelSectionMeshResult Part;
	if (!FVoxelSectionMesher::Build(Snapshot, InRegistry, InShapes, Part, InCancel, InTextureRepeats,
		InContext, false, false, Offset, PageSide, false, &Plan))
	{ OutError = TEXT("Failed to build counted proxy execution window"); return false; }
	if (NewBatchBytes + Mesh.Batches.GetAllocatedSize() + Part.Batches.GetAllocatedSize() + ScratchBytes + HaloBytes > PieceLimit ||
		Mesh.AllocatedBytes() - Mesh.Batches.GetAllocatedSize() +
		Part.AllocatedBytes() - Part.Batches.GetAllocatedSize() + NewBatchBytes > RetainedBytes)
	{ OutError = TEXT("Oversized: proxy batch metadata exceeds admitted capacity"); return false; }
	Mesh.Batches.Reserve(Mesh.Batches.Num() + Part.Batches.Num());
	for (auto& Batch : Part.Batches)
	{
		for (auto& Vertex : Batch.Mesh.Vertices) Vertex += FVector(Offset);
		if (!Batch.Mesh.Triangles.IsEmpty()) Mesh.Batches.Add(MoveTemp(Batch));
	}
	if (Mesh.AllocatedBytes() > RetainedBytes)
	{ OutError = TEXT("Oversized: accumulated proxy mesh exceeds retained-input capacity"); return false; }
	++NextShard;
	return true;
}
