#include "Voxel/Rendering/VoxelProxyPageMesh.h"

bool FVoxelProxyPageMesh::Initialize(const FVoxelTerrainGeometry& InGeometry, FString& OutError,
	const uint64 InRetainedBytes, const bool bInCountOnly)
{
	Geometry = InGeometry;
	RetainedBytes = InRetainedBytes;
	bCountOnly = bInCountOnly;
	EstimatedMeshBytes = 0;
	NextShard = 0;
	Mesh = {};
	return VoxelStreamingGeometry::BuildShards(Geometry, 16, 4096, Shards, OutError);
}

bool FVoxelProxyPageMesh::Advance(const FVoxelVoxelProxyData& InData, const FVoxelRegistrySnapshot& InRegistry,
	const FVoxelShapeRegistry& InShapes, const FVoxelBoundaryTransitionContext* InContext,
	const double InTextureRepeats, FVoxelSectionMeshResult& OutMesh, bool& bOutComplete,
	FString& OutError, const TAtomic<bool>* InCancel)
{
	bOutComplete = false;
	if (InData.Geometry != Geometry) { OutError = TEXT("Proxy mesh geometry changed"); return false; }
	if (!Shards.IsValidIndex(NextShard)) { if (!Mesh.ValidateAndCache()) { OutError = TEXT("Invalid completed page mesh"); return false; } OutMesh = MoveTemp(Mesh); bOutComplete = true; return true; }
	const auto& Shard = Shards[NextShard];
	const FIntVector Offset = Shard.LocalMinCells / Geometry.SampleStepCells;
	const int32 Side = Shard.CellCounts.X, PageSide = InData.GridSide;
	FVoxelSectionSnapshot Snapshot;
	Snapshot.GridSide = Side;
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
	FVoxelSectionMeshResult Part;
	if (!FVoxelSectionMesher::Build(Snapshot, InRegistry, InShapes, Part, InCancel, InTextureRepeats, InContext, false, false, Offset, PageSide))
	{ OutError = TEXT("Failed to mesh proxy execution window"); return false; }
	EstimatedMeshBytes += Part.Bytes();
	if (bCountOnly) { ++NextShard; return true; }
	for (auto& Batch : Part.Batches)
	{
		for (auto& Vertex : Batch.Mesh.Vertices) Vertex += FVector(Offset);
		if (!Batch.Mesh.Triangles.IsEmpty()) Mesh.Batches.Add(MoveTemp(Batch));
	}
	if (Mesh.Bytes() > RetainedBytes)
	{ OutError = TEXT("Oversized: accumulated proxy mesh exceeds retained-input capacity"); return false; }
	++NextShard;
	return true;
}
