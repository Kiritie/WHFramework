#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"

class WHFRAMEWORK_API FVoxelGenerationPipeline
{
public:
	FVoxelGenerationPipeline(
		TSharedRef<
			const FVoxelGenerationRuntimeConfig,
			ESPMode::ThreadSafe> InConfig,
		TSharedRef<
			FVoxelGenerationPlanCache,
			ESPMode::ThreadSafe> InCache);

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

	bool SampleColumns(
		const FIntPoint& InOrigin,
		int32 InWidth,
		int32 InHeight,
		int32 InStep,
		TArray<FVoxelColumnSample>& OutColumns,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool SampleBlock(
		const FIntVector& InPosition,
		FVoxelBlockState& OutState,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

private:
	TSharedRef<
		const FVoxelGenerationRuntimeConfig,
		ESPMode::ThreadSafe> Config;

	TSharedRef<
		FVoxelGenerationPlanCache,
		ESPMode::ThreadSafe> Cache;
};
