#pragma once

#include "CoreMinimal.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"

struct FVoxelGenerationRuntimeConfig;
struct FVoxelRegistrySnapshot;
struct FVoxelWaterSurfaceTileData;

class WHFRAMEWORK_API FVoxelHeightfieldMesher
{
public:
	static bool BuildTerrain(
		int32 InVertexSide,
		int32 InStep,
		TConstArrayView<int32> InHeights,
		TConstArrayView<uint16> InMaterials,
		const FVoxelGenerationRuntimeConfig& InConfig,
		const FVoxelRegistrySnapshot& InRegistry,
		FVoxelSectionMeshResult& OutMesh,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr,
		TConstArrayView<uint8> InCoverage = {},
		double InZBiasCells = 0.0,
		int32 InSkirtDepthCells = 8);

	static bool BuildWater(
		const FVoxelWaterSurfaceTileData& InWater,
		const FVoxelRegistrySnapshot& InRegistry,
		FVoxelSectionMeshResult& OutMesh,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

private:
	static FVoxelRenderBatch& FindOrAddBatch(
		FVoxelSectionMeshResult& InOutMesh,
		EVoxelRenderGroup InGroup,
		uint16 InBank);

	static void AppendQuad(
		FVoxelMeshBuffers& InOutMesh,
		const FVector& InA,
		const FVector& InB,
		const FVector& InC,
		const FVector& InD,
		const FVoxelRuntimeFaceRef& InFace,
		const FLinearColor& InColor = FLinearColor::White);

	static void AppendSkirtQuad(
		FVoxelMeshBuffers& InOutMesh,
		const FVector& InTopA,
		const FVector& InTopB,
		double InBottomZ,
		const FVoxelRuntimeFaceRef& InFace);
};
