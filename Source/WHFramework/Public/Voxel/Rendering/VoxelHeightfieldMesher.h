#pragma once

#include "CoreMinimal.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"

struct FVoxelGenerationRuntimeConfig;
struct FVoxelRegistrySnapshot;
struct FVoxelMacroTileData;
struct FVoxelDistantCell;
struct FVoxelWaterSurfaceTileData;

class WHFRAMEWORK_API FVoxelHeightfieldMesher
{
public:
	static bool BuildBlockyTerrain(
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
		double InMaximumTextureStretchCells = 4.0,
		TConstArrayView<FLinearColor> InTopTints = {});

	static bool BuildMacro(
		const FVoxelMacroTileData& InMacro,
		const FVoxelGenerationRuntimeConfig& InConfig,
		const FVoxelRegistrySnapshot& InRegistry,
		FVoxelSectionMeshResult& OutMesh,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr,
		double InMaximumTextureStretchCells = 4.0);

	static bool AppendDistantCells(
		TConstArrayView<FVoxelDistantCell> InCells,
		const FIntPoint& InTileOrigin,
		int32 InStep,
		const FVoxelRegistrySnapshot& InRegistry,
		FVoxelSectionMeshResult& InOutMesh,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr,
		double InMaximumTextureStretchCells = 4.0);

	static bool BuildWater(
		const FVoxelWaterSurfaceTileData& InWater,
		const FVoxelRegistrySnapshot& InRegistry,
		FVoxelSectionMeshResult& OutMesh,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr,
		double InMaximumTextureStretchCells = 4.0);

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

};
