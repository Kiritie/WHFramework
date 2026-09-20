#pragma once

#include "CoreMinimal.h"
#include "Parameter/ParameterTypes.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"
#include "Voxel/Runtime/VoxelBlockState.h"

struct FVoxelGenerationRuntimeConfig;
struct FVoxelRegistrySnapshot;

struct WHFRAMEWORK_API FVoxelFeatureCellWrite
{
	FIntVector Position =
		FIntVector::ZeroValue;

	/**
	 * Recipe Symbol + State。
	 * 不是 Runtime TypeId。
	 */
	uint32 Value = 0;
};

struct WHFRAMEWORK_API FVoxelFeatureInstance
{
	FVoxelStableId Id;

	FName DefinitionId;

	FIntVector Anchor =
		FIntVector::ZeroValue;

	TArray<FVoxelFeatureCellWrite> Writes;

	uint64 GetAllocatedBytes() const;
};

struct WHFRAMEWORK_API FVoxelFeatureBakeContext
{
	const FVoxelRegistrySnapshot* Registry = nullptr;
	const TMap<FName, uint16>* BlockSymbols = nullptr;

	bool ResolveBlockSymbol(
		const FPrimaryAssetId& InAssetId,
		uint16& OutSymbol,
		FString& OutError) const;
};

struct WHFRAMEWORK_API FVoxelFeatureQueryContext
{
	int32 WorldSeed = 0;

	const FVoxelGenerationRecipe* Recipe = nullptr;

	const FVoxelFeatureRuntimeDefinition* Definition = nullptr;

	FVoxelGenerationBounds QueryBounds;

	FIntVector CandidateAnchor =
		FIntVector::ZeroValue;

	FVoxelColumnSample Column;

	/**
	 * 由 Pipeline 提供。
	 * Feature Algorithm 不直接依赖 Generator 对象。
	 */
	TFunctionRef<bool(
		const FIntVector& InPosition,
		FVoxelColumnSample& OutColumn)> SampleColumn;

	TFunctionRef<bool(
		const FIntVector& InPosition,
		uint32& OutBaseSymbol)> SampleBaseSymbol;
};

class WHFRAMEWORK_API IVoxelFeatureAlgorithm
{
public:
	virtual ~IVoxelFeatureAlgorithm() = default;

	virtual FName GetId() const = 0;

	virtual uint32 GetVersion() const = 0;

	virtual bool GatherReferencedBlocks(
		const FParameter& InConfiguration,
		TArray<FPrimaryAssetId>& OutBlockAssets,
		FString& OutError) const = 0;

	/**
	 * Editor/Cook 时将 FParameter 转成稳定 bytes。
	 *
	 * Runtime 不解析 UObject / UStruct 资产。
	 */
	virtual bool BakeConfiguration(
		const FVoxelFeatureBakeContext& InContext,
		const FParameter& InConfiguration,
		TArray<uint8>& OutBytes,
		FString& OutError) const = 0;

	/**
	 * Runtime 纯值生成。
	 */
	virtual bool Generate(
		const FVoxelFeatureQueryContext& InContext,
		TConstArrayView<uint8> InConfigBytes,
		FVoxelFeatureInstance& OutInstance,
		FString& OutError) const = 0;
};

class WHFRAMEWORK_API FVoxelFeatureAlgorithmRegistry
{
public:
	static FVoxelFeatureAlgorithmRegistry& Get();

public:
	bool Register(
		TSharedRef<const IVoxelFeatureAlgorithm, ESPMode::ThreadSafe> InAlgorithm,
		FString& OutError);

	void Unregister(
		FName InAlgorithmId);

	TSharedPtr<const IVoxelFeatureAlgorithm, ESPMode::ThreadSafe> Find(
		FName InAlgorithmId) const;

	void Reset();

private:
	mutable FRWLock Lock;

	TMap<
		FName,
		TSharedRef<
			const IVoxelFeatureAlgorithm,
			ESPMode::ThreadSafe>> Algorithms;
};

class WHFRAMEWORK_API FVoxelFeaturePlanner
{
public:
	explicit FVoxelFeaturePlanner(
		TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe);

public:
	bool Plan(
		const FVoxelGenerationBounds& InBounds,
		TFunctionRef<bool(
			const FIntVector& InPosition,
			FVoxelColumnSample& OutColumn)> InSampleColumn,
		TFunctionRef<bool(
			const FIntVector& InPosition,
			uint32& OutBaseSymbol)> InSampleBaseSymbol,
		TArray<FVoxelFeatureInstance>& OutInstances,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

private:
	void GatherCandidateAnchors(
		const FVoxelFeatureRuntimeDefinition& InDefinition,
		const FVoxelGenerationBounds& InBounds,
		TArray<FIntVector>& OutAnchors) const;

	bool IsPlacementAllowed(
		const FVoxelFeatureRuntimeDefinition& InDefinition,
		const FIntVector& InAnchor,
		const FVoxelColumnSample& InColumn) const;

private:
	TSharedRef<
		const FVoxelGenerationRecipe,
		ESPMode::ThreadSafe> Recipe;
};
