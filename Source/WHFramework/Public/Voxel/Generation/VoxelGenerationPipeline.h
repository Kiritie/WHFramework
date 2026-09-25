#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Generation/VoxelGenerationOverlay.h"

class WHFRAMEWORK_API FVoxelGenerationPipeline
{
public:
	FVoxelGenerationPipeline(
		TSharedRef<
			const FVoxelGenerationRuntimeConfig,
			ESPMode::ThreadSafe> InConfig,
		TSharedRef<
			FVoxelGenerationPlanCache,
			ESPMode::ThreadSafe> InCache,
		TSharedPtr<const IVoxelGenerationOverlay, ESPMode::ThreadSafe> InOverlay = nullptr);

	bool GenerateSection(
		const FIntVector& InSectionCoordinate,
		TArray<FVoxelBlockState>& OutBaseBlocks,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool SampleColumn(
		int32 InX,
		int32 InY,
		FVoxelColumnSample& OutColumn,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool SampleEnvironment(
		int32 InX,
		int32 InY,
		FVoxelEnvironmentSample& OutSample,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool SampleEnvironments(
		const FIntPoint& InOrigin,
		int32 InWidth,
		int32 InHeight,
		int32 InStep,
		TArray<FVoxelEnvironmentSample>& OutSamples,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr,
		bool bInUseColumnCache = true) const;

	bool SampleColumns(
		const FIntPoint& InOrigin,
		int32 InWidth,
		int32 InHeight,
		int32 InStep,
		TArray<FVoxelColumnSample>& OutColumns,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr,
		bool bInUseColumnCache = true) const;

	bool SampleBlock(
		const FIntVector& InPosition,
		FVoxelBlockState& OutState,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool BuildCoarseOverlay(
		const FIntVector& InOrigin,
		int32 InStep,
		int32 InGridSide,
		TMap<FIntVector, FVoxelBlockState>& OutCells,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

private:
	TSharedRef<
		const FVoxelGenerationRuntimeConfig,
		ESPMode::ThreadSafe> Config;

	TSharedRef<
		FVoxelGenerationPlanCache,
		ESPMode::ThreadSafe> Cache;

	TSharedPtr<const IVoxelGenerationOverlay, ESPMode::ThreadSafe> Overlay;
};
