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
	// 顺序记录同一窗口的真实 batch 分配，再按相同 emission 顺序一次性 Reserve。
	class WHFRAMEWORK_API FCapacityScope
	{
	public:
		FCapacityScope(bool bInCount, TArray<FVoxelRenderBatch>& InPlan, int32 InBatchLimit);
		~FCapacityScope();
		FCapacityScope(const FCapacityScope&) = delete;
		FCapacityScope& operator=(const FCapacityScope&) = delete;
		bool IsValid() const;
		void Bind(FVoxelRenderBatch& InBatch);
		bool PrepareBatches(FVoxelSectionMeshResult& InMesh);
		FVoxelRenderBatch& RejectedBatch();
		bool Count(FVoxelMeshBuffers& InMesh, int32 InVertices, int32 InIndices);
	private:
		FCapacityScope* Previous = nullptr;
		TArray<FVoxelRenderBatch>& Plan;
		int32 NextBatch = 0;
		int32 BatchLimit = 0;
		FVoxelRenderBatch Rejected;
		bool bCounting = false;
		bool bValid = true;
	};
	// Runs the same emission rules without allocating vertex buffers. Scoped to a worker shard.
	class WHFRAMEWORK_API FCountScope
	{
	public:
		explicit FCountScope(uint64* InBytes);
		~FCountScope();
		FCountScope(const FCountScope&) = delete;
		FCountScope& operator=(const FCountScope&) = delete;
	private:
		uint64* Previous = nullptr;
	};
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
		TConstArrayView<FLinearColor> InTopTints = {},
		const FIntRect& InWindow = FIntRect());

	static bool BuildMacro(
		const FVoxelMacroTileData& InMacro,
		const FVoxelGenerationRuntimeConfig& InConfig,
		const FVoxelRegistrySnapshot& InRegistry,
		FVoxelSectionMeshResult& OutMesh,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr,
		double InMaximumTextureStretchCells = 4.0,
		const FIntRect& InWindow = FIntRect());

	static bool AppendDistantCells(
		TConstArrayView<FVoxelDistantCell> InCells,
		const FIntPoint& InTileOrigin,
		int32 InStep,
		const FVoxelRegistrySnapshot& InRegistry,
		FVoxelSectionMeshResult& InOutMesh,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr,
		double InMaximumTextureStretchCells = 4.0,
		const FIntRect& InWindow = FIntRect());

	static bool BuildWater(
		const FVoxelWaterSurfaceTileData& InWater,
		const FVoxelRegistrySnapshot& InRegistry,
		FVoxelSectionMeshResult& OutMesh,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr,
		double InMaximumTextureStretchCells = 4.0,
		const FIntRect& InWindow = FIntRect());

	static bool BuildWaterGrid(int32 InSide, int32 InStep,
		TConstArrayView<int32> InGroundZ, TConstArrayView<int32> InWaterZ,
		TConstArrayView<uint8> InWaterKind, const FVoxelRegistrySnapshot& InRegistry,
		FVoxelSectionMeshResult& OutMesh, FString& OutError,
		const TAtomic<bool>* InCancel, double InMaximumTextureStretchCells,
		const FIntRect& InWindow);

private:
	static FVoxelRenderBatch& FindOrAddBatch(
		FVoxelSectionMeshResult& InOutMesh,
		EVoxelRenderGroup InGroup,
		uint16 InBank,
		uint8 InVariant = 0);

	static void AppendQuad(
		FVoxelMeshBuffers& InOutMesh,
		const FVector& InA,
		const FVector& InB,
		const FVector& InC,
		const FVector& InD,
		const FVoxelRuntimeFaceRef& InFace,
		const FLinearColor& InColor = FLinearColor::White);

};
