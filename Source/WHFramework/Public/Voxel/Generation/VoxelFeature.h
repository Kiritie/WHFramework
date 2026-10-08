#pragma once

#include "CoreMinimal.h"
#include "Parameter/ParameterTypes.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"
#include "Voxel/Runtime/VoxelBlockState.h"

struct FVoxelGenerationRuntimeConfig;
struct FVoxelRegistrySnapshot;
struct FVoxelFeaturePlan;
class FVoxelGenerationWorkspace;

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

	/** 输出归 caller Workspace；为空时归当前正文 retained scope，不得借局部工作区返回。 */
	FVoxelGenerationWorkspace* Workspace = nullptr;
	const TAtomic<bool>* Cancel = nullptr;

	bool ReserveWrites(
		TArray<FVoxelFeatureCellWrite>& InWrites,
		int32 InCapacity,
		FString& OutError) const;
	void FreeWrites(
		TArray<FVoxelFeatureCellWrite>& InWrites) const;
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
	 * Anchor、offset 与写入坐标先使用 int64，再经 VoxelGeneration::TryWorldCell
	 * 检查统一 cell 域后收窄；域外候选/写入跳过，不 Clamp Anchor 改变稳定身份。
	 * 最终输出过滤不能识别算法内部已经溢出回域内的坐标，扩展算法也必须遵守此契约。
	 * 所有 Writes 增长前调用 Context.ReserveWrites；失败保持旧 OutInstance，完整诊断向上传递。
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
	/** active 输出只能是新正文或当前 scope 的 retained 正文；失败保持旧 OutPlan。 */
	bool BuildPlan(
		const FVoxelGenerationBounds& InBounds,
		TFunctionRef<bool(const FIntVector&, FVoxelColumnSample&)> InSampleColumn,
		TFunctionRef<bool(const FIntVector&, uint32&)> InSampleBaseSymbol,
		FVoxelFeaturePlan& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	/**
	 * 成功输出及嵌套 Writes 使用同一 caller Workspace；无 Workspace 时归当前 retained scope。
	 * 旧 OutInstances 必须使用同一预算，caller Workspace 应先于输出容器声明。
	 */
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
		const TAtomic<bool>* InCancel = nullptr,
		FVoxelGenerationWorkspace* InOutputWorkspace = nullptr) const;

private:
	bool GatherCandidateAnchors(
		const FVoxelFeatureRuntimeDefinition& InDefinition,
		const FVoxelGenerationBounds& InBounds,
		TArray<FIntVector>& OutAnchors,
		FVoxelGenerationWorkspace& InWorkspace,
		FString& OutError,
		const TAtomic<bool>* InCancel) const;

	bool IsPlacementAllowed(
		const FVoxelFeatureRuntimeDefinition& InDefinition,
		const FIntVector& InAnchor,
		const FVoxelColumnSample& InColumn) const;

private:
	TSharedRef<
		const FVoxelGenerationRecipe,
		ESPMode::ThreadSafe> Recipe;
};
