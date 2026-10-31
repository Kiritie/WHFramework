#include "Voxel/Rendering/VoxelHeightfieldPageMesh.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

bool FVoxelHeightfieldPageMesh::Initialize(const FVoxelTerrainGeometry& InGeometry, FString& OutError,
	const uint64 InRetainedBytes, const bool bInCountOnly, const uint64 InPieceLimit, const uint64 InGeometryLimit)
{
	if (!InGeometry.Validate(OutError))
	{
		return false;
	}
	if (!InGeometry.Key.IsColumn())
	{
		OutError = TEXT("Mesh representation does not match its actual geometry");
		return false;
	}
	if (InPieceLimit == 0 || InGeometryLimit == 0)
	{
		OutError = TEXT("Invalid mesh capacity limits");
		return false;
	}
	FVoxelHeightfieldPageMesh Candidate;
	Candidate.Geometry = InGeometry;
	Candidate.RetainedBytes = InRetainedBytes;
	Candidate.PieceLimit = InPieceLimit;
	Candidate.GeometryLimit = InGeometryLimit;
	Candidate.bCountOnly = bInCountOnly;
	if (!VoxelStreamingGeometry::BuildShards(InGeometry, 32, 4096, Candidate.Shards, OutError))
	{
		return false;
	}
	Candidate.Shards.Reserve(4096);
	*this = MoveTemp(Candidate);
	return true;
}

bool FVoxelHeightfieldPageMesh::SplitWindow(FString& OutError)
{
	const auto Shard = Shards[NextShard];
	if (Shard.CellCounts.X <= 1 || Shards.Num() > 4096 - 3)
	{ OutError = TEXT("Oversized: heightfield Piece cannot fit the count/build peak"); return false; }
	const int32 Half = Shard.CellCounts.X / 2;
	Shards[NextShard].CellCounts = FIntVector(Half, Half, 0);
	for (int32 Y = 0; Y < 2; ++Y)
		for (int32 X = 0; X < 2; ++X)
			if (X || Y) Shards.Add({Shard.LocalMinCells + FIntVector(X, Y, 0) * Half * Geometry.SampleStepCells,
				FIntVector(Half, Half, 0)});
	return true;
}

bool FVoxelHeightfieldPageMesh::PreparePlan(FVoxelSectionMeshResult& InPlan,
	const uint64 InScratchBytes, bool& bOutSplit, FString& OutError)
{
	bOutSplit = false;
	const uint64 Bytes = InPlan.PlannedBytes();
	const uint64 OldBatchBytes = VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(EstimatedBatchCount);
	const uint64 NewBatchBytes = VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(EstimatedBatchCount + InPlan.Batches.Num());
	if (Bytes + InScratchBytes + OldBatchBytes + NewBatchBytes > PieceLimit)
	{
		bOutSplit = SplitWindow(OutError);
		return bOutSplit;
	}
	const uint64 PayloadBytes = Bytes - VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(InPlan.Batches.Num());
	if (PayloadBytes > GeometryLimit || EstimatedPayloadBytes > GeometryLimit - PayloadBytes ||
		NewBatchBytes > GeometryLimit - EstimatedPayloadBytes - PayloadBytes)
	{ OutError = TEXT("Oversized: heightfield geometry exceeds the Shard limit"); return false; }
	const uint64 CombinedBytes = Mesh.AllocatedBytes() - Mesh.Batches.GetAllocatedSize() + PayloadBytes +
		VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(Mesh.Batches.Num() + InPlan.Batches.Num());
	if (!bCountOnly && CombinedBytes > RetainedBytes)
	{ OutError = TEXT("Oversized: heightfield geometry exceeds admitted capacity"); return false; }
	EstimatedPayloadBytes += PayloadBytes;
	EstimatedBatchCount += InPlan.Batches.Num();
	EstimatedMeshBytes = EstimatedPayloadBytes + NewBatchBytes;
	return true;
}

bool FVoxelHeightfieldPageMesh::Retain(FVoxelSectionMeshResult&& InPart, FString& OutError)
{
	if (bCountOnly) { ++NextShard; return true; }
	const uint64 NewBatchBytes = VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(Mesh.Batches.Num() + InPart.Batches.Num());
	if (NewBatchBytes + Mesh.Batches.GetAllocatedSize() + InPart.Batches.GetAllocatedSize() > PieceLimit ||
		Mesh.AllocatedBytes() - Mesh.Batches.GetAllocatedSize() +
		InPart.AllocatedBytes() - InPart.Batches.GetAllocatedSize() + NewBatchBytes > RetainedBytes)
	{ OutError = TEXT("Oversized: heightfield batch metadata exceeds admitted capacity"); return false; }
	Mesh.Batches.Reserve(Mesh.Batches.Num() + InPart.Batches.Num());
	for (auto& Batch : InPart.Batches)
		if (!Batch.Mesh.Triangles.IsEmpty()) Mesh.Batches.Add(MoveTemp(Batch));
	++NextShard;
	return true;
}

bool FVoxelHeightfieldPageMesh::Advance(const FVoxelSurfaceTileData& InData,
	const FVoxelWaterSurfaceTileData& InWater, const FVoxelGenerationRuntimeConfig& InConfig,
	const FVoxelRegistrySnapshot& InRegistry, const double InTextureStretch,
	FVoxelSectionMeshResult& OutMesh, bool& bOutComplete, FString& OutError, const TAtomic<bool>* InCancel)
{
	bOutComplete = false;
	if (InData.Geometry != Geometry || !Geometry.Validate(OutError) ||
		Geometry.Key.Representation != EVoxelTerrainRepresentation::Surface ||
		InData.Key != FVoxelSurfaceTileKey{FIntPoint(Geometry.Key.Coordinate.X, Geometry.Key.Coordinate.Y), Geometry.Key.Level} ||
		InData.LocalMinCells != FIntPoint::ZeroValue || InData.Side < 2 || InData.Side > 257 ||
		InData.Step != Geometry.SampleStepCells || int64(InData.Side - 1) * InData.Step != Geometry.Key.SideCells() ||
		InData.GroundZ.Num() != InData.Side * InData.Side || InData.SurfaceStates.Num() != InData.GroundZ.Num() ||
		InWater.Key != InData.Key || InWater.Step != InData.Step || InWater.Side != InData.Side ||
		InWater.GroundZ.Num() != InData.GroundZ.Num() || InWater.WaterZ.Num() != InData.GroundZ.Num() ||
		InWater.WaterKind.Num() != InData.GroundZ.Num() ||
		!FMath::IsFinite(InTextureStretch) || InTextureStretch <= 0.0)
	{
		OutError = TEXT("Surface mesh input does not match its actual geometry");
		return false;
	}
	if (!Shards.IsValidIndex(NextShard))
	{
		if (!Mesh.ValidateAndCache()) { OutError = TEXT("Invalid completed page mesh"); return false; }
		OutMesh = MoveTemp(Mesh); bOutComplete = true; return true;
	}
	const auto Shard = Shards[NextShard];
	const FIntPoint Min(Shard.LocalMinCells.X / InData.Step, Shard.LocalMinCells.Y / InData.Step);
	const FIntRect Window(Min, Min + FIntPoint(Shard.CellCounts.X, Shard.CellCounts.Y));
	const int32 BatchBound = int32(FMath::Min(int64(24) * InRegistry.Definitions.Num() + 10,
		int64(4) * Window.Width() * Window.Height() + int64(6) * InData.DistantCells.Num()));
	const uint64 ScratchBytes = 3 * VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(BatchBound) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainBuildShard>(4096);
	if (ScratchBytes > PieceLimit) return SplitWindow(OutError);
	const auto Emit = [&](FVoxelSectionMeshResult& Part) -> bool
	{
		FVoxelSectionMeshResult Water;
		if (!FVoxelHeightfieldMesher::BuildBlockyTerrain(InData.Side, InData.Step, InData.GroundZ,
			InData.SurfaceStates, InConfig, InRegistry, Part, OutError, InCancel, {}, -0.02,
			InTextureStretch, {}, Window) ||
			!FVoxelHeightfieldMesher::BuildWater(InWater, InRegistry, Water, OutError, InCancel, InTextureStretch, Window)) return false;
		for (auto& Batch : Water.Batches)
			if (!Batch.Mesh.Triangles.IsEmpty() || Batch.Mesh.PlannedVertices > 0) Part.Batches.Add(MoveTemp(Batch));
		return FVoxelHeightfieldMesher::AppendDistantCells(InData.DistantCells, InData.GetBuildOrigin(), InData.Step,
			InRegistry, Part, OutError, InCancel, InTextureStretch, Window);
	};
	FVoxelSectionMeshResult Plan, Counted;
	{
		FVoxelHeightfieldMesher::FCapacityScope Scope(true, Plan.Batches, BatchBound);
		if (!Emit(Counted) || !Scope.IsValid()) return false;
	}
	bool bSplit = false;
	if (!PreparePlan(Plan, ScratchBytes, bSplit, OutError)) return false;
	if (bSplit) return true;
	if (bCountOnly) { ++NextShard; return true; }
	Counted = {};
	FVoxelSectionMeshResult Part;
	{
		FVoxelHeightfieldMesher::FCapacityScope Scope(false, Plan.Batches, BatchBound);
		if (!Emit(Part) || !Scope.IsValid()) { OutError = TEXT("Heightfield Count/Build batch identity changed"); return false; }
	}
	return Retain(MoveTemp(Part), OutError);
}

bool FVoxelHeightfieldPageMesh::Advance(const FVoxelMacroTileData& InData,
	const FVoxelGenerationRuntimeConfig& InConfig, const FVoxelRegistrySnapshot& InRegistry,
	const double InTextureStretch, FVoxelSectionMeshResult& OutMesh, bool& bOutComplete,
	FString& OutError, const TAtomic<bool>* InCancel)
{
	bOutComplete = false;
	if (InData.Geometry != Geometry || !Geometry.Validate(OutError) ||
		Geometry.Key.Representation != EVoxelTerrainRepresentation::Macro ||
		InData.Key != FVoxelMacroTileKey{FIntPoint(Geometry.Key.Coordinate.X, Geometry.Key.Coordinate.Y), Geometry.Key.Level} ||
		InData.LocalMinCells != FIntPoint::ZeroValue || InData.Side < 2 || InData.Side > 257 ||
		InData.Step != Geometry.SampleStepCells || int64(InData.Side - 1) * InData.Step != Geometry.Key.SideCells() ||
		InData.Height.Num() != InData.Side * InData.Side || InData.WaterHeight.Num() != InData.Height.Num() ||
		InData.WaterKind.Num() != InData.Height.Num() || InData.SurfaceStates.Num() != InData.Height.Num() ||
		InData.ForestCoverage.Num() != InData.Height.Num() ||
		!FMath::IsFinite(InTextureStretch) || InTextureStretch <= 0.0)
	{
		OutError = TEXT("Macro mesh input does not match its actual geometry");
		return false;
	}
	if (!Shards.IsValidIndex(NextShard))
	{
		if (!Mesh.ValidateAndCache()) { OutError = TEXT("Invalid completed page mesh"); return false; }
		OutMesh = MoveTemp(Mesh); bOutComplete = true; return true;
	}
	const auto Shard = Shards[NextShard];
	const FIntPoint Min(Shard.LocalMinCells.X / InData.Step, Shard.LocalMinCells.Y / InData.Step);
	const FIntRect Window(Min, Min + FIntPoint(Shard.CellCounts.X, Shard.CellCounts.Y));
	const int32 BatchBound = int32(FMath::Min(int64(24) * InRegistry.Definitions.Num() + 10,
		int64(4) * Window.Width() * Window.Height() + int64(6) * InData.DistantCells.Num()));
	const uint64 ScratchBytes = 3 * VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(BatchBound) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainBuildShard>(4096) +
		VoxelBoundedAllocation::ArrayBytes<FLinearColor>(Window.Width() * Window.Height());
	if (ScratchBytes > PieceLimit) return SplitWindow(OutError);
	FVoxelSectionMeshResult Plan, Counted;
	{
		FVoxelHeightfieldMesher::FCapacityScope Scope(true, Plan.Batches, BatchBound);
		if (!FVoxelHeightfieldMesher::BuildMacro(InData, InConfig, InRegistry, Counted, OutError,
			InCancel, InTextureStretch, Window) || !Scope.IsValid()) return false;
	}
	bool bSplit = false;
	if (!PreparePlan(Plan, ScratchBytes, bSplit, OutError)) return false;
	if (bSplit) return true;
	if (bCountOnly) { ++NextShard; return true; }
	Counted = {};
	FVoxelSectionMeshResult Part;
	{
		FVoxelHeightfieldMesher::FCapacityScope Scope(false, Plan.Batches, BatchBound);
		if (!FVoxelHeightfieldMesher::BuildMacro(InData, InConfig, InRegistry, Part, OutError,
			InCancel, InTextureStretch, Window) || !Scope.IsValid())
		{ OutError = TEXT("Macro Count/Build batch identity changed"); return false; }
	}
	return Retain(MoveTemp(Part), OutError);
}
