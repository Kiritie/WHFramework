#include "Voxel/Rendering/VoxelHeightfieldPageMesh.h"

bool FVoxelHeightfieldPageMesh::Initialize(const FVoxelTerrainGeometry& InGeometry, FString& OutError,
	const uint64 InRetainedBytes, const bool bInCountOnly)
{
	Geometry = InGeometry;
	RetainedBytes = InRetainedBytes;
	bCountOnly = bInCountOnly;
	EstimatedMeshBytes = 0;
	NextShard = 0;
	Mesh = {};
	return VoxelStreamingGeometry::BuildShards(Geometry, 32, 4096, Shards, OutError);
}

bool FVoxelHeightfieldPageMesh::Retain(FVoxelSectionMeshResult&& InPart, FString& OutError)
{
	if (bCountOnly) { ++NextShard; return true; }
	for (auto& Batch : InPart.Batches)
		if (!Batch.Mesh.Triangles.IsEmpty()) Mesh.Batches.Add(MoveTemp(Batch));
	if (Mesh.Bytes() > RetainedBytes)
	{ OutError = TEXT("Oversized: heightfield mesh exceeds retained-input capacity"); return false; }
	++NextShard;
	return true;
}

bool FVoxelHeightfieldPageMesh::Advance(const FVoxelSurfaceTileData& InData,
	const FVoxelWaterSurfaceTileData& InWater, const FVoxelGenerationRuntimeConfig& InConfig,
	const FVoxelRegistrySnapshot& InRegistry, const double InTextureStretch,
	FVoxelSectionMeshResult& OutMesh, bool& bOutComplete, FString& OutError, const TAtomic<bool>* InCancel)
{
	bOutComplete = false;
	if (InData.Geometry != Geometry) { OutError = TEXT("Surface mesh geometry changed"); return false; }
	if (!Shards.IsValidIndex(NextShard)) { if (!Mesh.ValidateAndCache()) { OutError = TEXT("Invalid completed page mesh"); return false; } OutMesh = MoveTemp(Mesh); bOutComplete = true; return true; }
	FVoxelHeightfieldMesher::FCountScope CountScope(bCountOnly ? &EstimatedMeshBytes : nullptr);
	const auto& Shard = Shards[NextShard];
	const FIntPoint Min(Shard.LocalMinCells.X / InData.Step, Shard.LocalMinCells.Y / InData.Step);
	const FIntRect Window(Min, Min + FIntPoint(Shard.CellCounts.X, Shard.CellCounts.Y));
	FVoxelSectionMeshResult Part, Water;
	if (!FVoxelHeightfieldMesher::BuildBlockyTerrain(InData.Side, InData.Step, InData.GroundZ,
		InData.SurfaceMaterial, InConfig, InRegistry, Part, OutError, InCancel, {}, -0.02,
		InTextureStretch, {}, Window) ||
		!FVoxelHeightfieldMesher::BuildWater(InWater, InRegistry, Water, OutError, InCancel, InTextureStretch, Window) ||
		!FVoxelHeightfieldMesher::AppendDistantCells(InData.DistantCells, InData.GetBuildOrigin(), InData.Step,
		InRegistry, Part, OutError, InCancel, InTextureStretch, Window)) return false;
	for (auto& Batch : Water.Batches)
		if (!Batch.Mesh.Triangles.IsEmpty()) Part.Batches.Add(MoveTemp(Batch));
	return Retain(MoveTemp(Part), OutError);
}

bool FVoxelHeightfieldPageMesh::Advance(const FVoxelMacroTileData& InData,
	const FVoxelGenerationRuntimeConfig& InConfig, const FVoxelRegistrySnapshot& InRegistry,
	const double InTextureStretch, FVoxelSectionMeshResult& OutMesh, bool& bOutComplete,
	FString& OutError, const TAtomic<bool>* InCancel)
{
	bOutComplete = false;
	if (InData.Geometry != Geometry) { OutError = TEXT("Macro mesh geometry changed"); return false; }
	if (!Shards.IsValidIndex(NextShard)) { if (!Mesh.ValidateAndCache()) { OutError = TEXT("Invalid completed page mesh"); return false; } OutMesh = MoveTemp(Mesh); bOutComplete = true; return true; }
	FVoxelHeightfieldMesher::FCountScope CountScope(bCountOnly ? &EstimatedMeshBytes : nullptr);
	const auto& Shard = Shards[NextShard];
	const FIntPoint Min(Shard.LocalMinCells.X / InData.Step, Shard.LocalMinCells.Y / InData.Step);
	const FIntRect Window(Min, Min + FIntPoint(Shard.CellCounts.X, Shard.CellCounts.Y));
	FVoxelSectionMeshResult Part;
	if (!FVoxelHeightfieldMesher::BuildMacro(InData, InConfig, InRegistry, Part, OutError,
		InCancel, InTextureStretch, Window)) return false;
	return Retain(MoveTemp(Part), OutError);
}
