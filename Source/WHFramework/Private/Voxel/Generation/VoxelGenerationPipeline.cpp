#include "Voxel/Generation/VoxelGenerationPipeline.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Generation/Ecology/VoxelEcology.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "Voxel/Generation/VoxelGenerationPlanningRead.h"

namespace
{
	int32 NextPipelineScratchCapacity(const int32 InRequired, const int32 InCapacity)
	{
		return int32(FMath::Min<int64>(MAX_int32,
			FMath::Max<int64>(InRequired, FMath::Max<int64>(4, int64(InCapacity) * 2))));
	}

	template<typename ElementType>
	bool AppendPipelineScratch(
		FVoxelGenerationWorkspace& InWorkspace,
		TArray<ElementType>& InArray,
		const ElementType& InValue,
		FString& OutError)
	{
		if (InArray.Num() == MAX_int32)
		{
			OutError = TEXT("Oversized: generation pipeline scratch element count");
			return false;
		}
		if (InArray.Num() == InArray.Max() &&
			!InWorkspace.ReserveArray(InArray, NextPipelineScratchCapacity(InArray.Num() + 1, InArray.Max()), OutError))
		{
			return false;
		}
		InArray.Add(InValue);
		return true;
	}

	template<typename ElementType>
	void CopyPipelineOutput(const TArray<ElementType>& InValues, const TArrayView<ElementType> OutValues)
	{
		check(InValues.Num() == OutValues.Num());
		for (int32 Index = 0; Index < InValues.Num(); ++Index)
		{
			OutValues[Index] = InValues[Index];
		}
	}

#if WITH_DEV_AUTOMATION_TESTS
	bool AllowLegacyPipelineArrayOutput(
		const TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>& InCache,
		FString& OutError)
	{
		if (InCache->GetResources())
		{
			OutError = TEXT("InvalidResource: bound generation pipeline requires an explicitly granted caller span or visitor");
			return false;
		}
		return true;
	}
#endif

	bool MakeColumnGridBounds(
		const FVoxelGenerationSettings& InSettings,
		const FIntPoint& InOrigin,
		const int32 InWidth,
		const int32 InHeight,
		const int32 InStep,
		FVoxelGenerationBounds& OutBounds,
		int32& OutCount,
		FString& OutError)
	{
		if (InWidth <= 0 || InHeight <= 0 ||
			InWidth > 4096 || InHeight > 4096 || InStep <= 0)
		{
			OutError = TEXT("Voxel column grid parameters are invalid");
			return false;
		}

		const int64 Count = static_cast<int64>(InWidth) * InHeight;
		const int64 LastX = static_cast<int64>(InOrigin.X) +
			static_cast<int64>(InWidth - 1) * InStep;
		const int64 LastY = static_cast<int64>(InOrigin.Y) +
			static_cast<int64>(InHeight - 1) * InStep;
		if (Count > MAX_int32 || LastX >= MAX_int32 || LastY >= MAX_int32)
		{
			OutError = TEXT("Voxel column grid exceeds coordinate range");
			return false;
		}

		OutBounds = {
			FIntVector(InOrigin.X, InOrigin.Y, InSettings.MinZ),
			FIntVector(static_cast<int32>(LastX + 1),
				static_cast<int32>(LastY + 1), InSettings.MaxZ)
		};
		OutCount = static_cast<int32>(Count);
		OutError.Reset();
		return true;
	}
}

bool FVoxelGenerationPipeline::ResolveAllocation(
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& OutResources,
	FVoxelGenerationConsumerKey& OutConsumer,
	FString& OutError) const
{
	OutResources = Cache->GetResources();
	if (OutResources)
	{
		if (!FVoxelGenerationConsumerScope::GetConsumerKey() &&
			!FVoxelGenerationPlanningReadScope::GetConsumerKey() &&
			!FVoxelGenerationPlanningReadScope::GetTaskStamp())
		{
			OutError = TEXT("InvalidConsumer: generation pipeline requires its original execution consumer");
			return false;
		}
		if (!Cache->ResolveConsumer(OutConsumer, OutError))
		{
			return false;
		}
		OutError.Reset();
		return true;
	}
#if WITH_DEV_AUTOMATION_TESTS
	OutError.Reset();
	return true;
#else
	OutError = TEXT("InvalidConsumer: generation pipeline has no original resource ledger");
	return false;
#endif
}

bool FVoxelGenerationPipeline::AddOutputRequirements(
	const FVoxelTerrainResourceLease* InOutputLease,
	uint64& InOutRequiredInputBytes,
	int32& InOutRequiredLeaseCount,
	uint64& OutRequiredWorkingBytes,
	FString& OutError) const
{
	OutRequiredWorkingBytes = 0;
	const auto& Resources = Cache->GetResources();
	if (!InOutputLease || !Resources || !InOutputLease->IsOwnedBy(*Resources))
	{
		return true;
	}
	const uint64 Bytes = InOutputLease->Bytes();
	const EVoxelTerrainResourcePool Pool = InOutputLease->GetPool();
	if (Pool >= EVoxelTerrainResourcePool::Count)
	{
		OutError = TEXT("InvalidResource: generation pipeline output lease is not active");
		return false;
	}
	if (InOutRequiredLeaseCount == MAX_int32)
	{
		OutError = TEXT("CapacityUnachievable: generation pipeline required output lease count");
		return false;
	}
	if (Pool == EVoxelTerrainResourcePool::Input)
	{
		if (Bytes > MAX_uint64 - InOutRequiredInputBytes)
		{
			OutError = TEXT("CapacityUnachievable: generation pipeline required output input peak");
			return false;
		}
		InOutRequiredInputBytes += Bytes;
	}
	else if (Pool == EVoxelTerrainResourcePool::Working)
	{
		OutRequiredWorkingBytes = Bytes;
	}
	// 零字节的原租约仍有活跃 handle，不能因 payload 已释放而漏计其固定输入。
	++InOutRequiredLeaseCount;
	return true;
}

bool FVoxelGenerationPipeline::EnumerateCaveEntrances(
	const FIntPoint& InMin,
	const FIntPoint& InMax,
	TFunctionRef<bool(const FVoxelCaveEntrance&, const FVoxelGenerationWorkspace&, FString&)> InVisit,
	FString& OutError,
	const FVoxelTerrainResourceLease* InOutputLease) const
{
	if (!Config->Recipe || InMin.X > InMax.X || InMin.Y > InMax.Y ||
		!VoxelGeneration::IsWorldCell(InMin.X) || !VoxelGeneration::IsWorldCell(InMin.Y) ||
		!VoxelGeneration::IsWorldCell(InMax.X) || !VoxelGeneration::IsWorldCell(InMax.Y))
	{
		OutError = TEXT("Invalid cave entrance bounds");
		return false;
	}
	FVoxelGenerationQuery Query;
	if (!FVoxelGenerationQuery::Create(Config, Cache, Query, OutError, true,
		IsInGameThread(), IsInGameThread(), nullptr, InOutputLease))
	{
		return false;
	}
	const auto& Settings = Config->Recipe->Settings;
	const FVoxelGenerationBounds Bounds{FIntVector(InMin.X, InMin.Y, Settings.MinZ),
		FIntVector(int32(FMath::Min<int64>(int64(InMax.X) + 1, MAX_int32)),
			int32(FMath::Min<int64>(int64(InMax.Y) + 1, MAX_int32)), Settings.MaxZ)};
	FVoxelGenerationPlanKeyRanges Ranges;
	if (!FVoxelGenerationQuery::GetPlanKeyRanges(Bounds, Settings, Ranges, OutError))
	{
		return false;
	}
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	if (!ResolveAllocation(Resources, Consumer, OutError))
	{
		return false;
	}
	// 工作区先声明，候选数组和去重表的 native backing 先于原租约销毁。
	const uint64 SourceInputBytes = Query.GetRequiredInputBytes();
	const int32 SourceLeaseCount = Query.GetRequiredInputLeaseCount();
	uint64 RequiredInputBytes = SourceInputBytes;
	int32 RequiredLeaseCount = SourceLeaseCount;
	uint64 RequiredWorkingBytes = 0;
	if (!AddOutputRequirements(InOutputLease, RequiredInputBytes, RequiredLeaseCount, RequiredWorkingBytes, OutError))
	{
		return false;
	}
	FVoxelGenerationWorkspace Workspace(Resources, Consumer,
		RequiredInputBytes, RequiredLeaseCount, RequiredWorkingBytes);
	TSet<FVoxelStableId> Seen;
	TArray<FVoxelCaveEntrance> Entrances;
	for (int64 Y = Ranges.TileMin.Y; Y <= Ranges.TileMax.Y; ++Y)
	{
		for (int64 X = Ranges.TileMin.X; X <= Ranges.TileMax.X; ++X)
		{
			const FVoxelGenerationTileKey Key{FIntVector(int32(X), int32(Y), 0)};
			FVoxelCavePlanPtr Plan;
			if (!Query.EnsureCavePlan(Key, Plan, OutError))
			{
				return false;
			}
			for (const FVoxelCaveEntrance& Entrance : Plan->Entrances)
			{
				const bool bMouth = Entrance.MouthCenter.X >= InMin.X && Entrance.MouthCenter.Y >= InMin.Y && Entrance.MouthCenter.X <= InMax.X && Entrance.MouthCenter.Y <= InMax.Y;
				const bool bInterior = Entrance.InteriorJoin.X >= InMin.X && Entrance.InteriorJoin.Y >= InMin.Y && Entrance.InteriorJoin.X <= InMax.X && Entrance.InteriorJoin.Y <= InMax.Y;
				if ((bMouth || bInterior) && !Seen.Contains(Entrance.Id))
				{
					if (Seen.Num() == MAX_int32)
					{
						OutError = TEXT("Oversized: generation pipeline cave entrance identities");
						return false;
					}
					if (!Workspace.ReserveSet(Seen, Seen.Num() + 1, OutError) ||
						!AppendPipelineScratch(Workspace, Entrances, Entrance, OutError))
					{
						return false;
					}
					Seen.Add(Entrance.Id);
				}
			}
		}
	}
	// 去重已结束，先释放表再退相应授额，为调用者的候选输出保留真实峰值空间。
	Workspace.FreeSet(Seen);
	// visitor 的工作区已经计入自己原输出，Source 只声明自己仍强持的依赖，避免回传重复计费。
	Workspace.SetRequirements(SourceInputBytes, SourceLeaseCount, 0);
	// 完整读集已成功，输出 callback 不再触发后续原生采样或去重增长。
	for (const FVoxelCaveEntrance& Entrance : Entrances)
	{
		if (!InVisit(Entrance, Workspace, OutError))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Voxel cave entrance output visitor rejected a candidate");
			}
			return false;
		}
	}
	OutError.Reset();
	return true;
}

FVoxelGenerationPipeline::FVoxelGenerationPipeline(
	TSharedRef<
		const FVoxelGenerationRuntimeConfig,
		ESPMode::ThreadSafe> InConfig,
	TSharedRef<
		FVoxelGenerationPlanCache,
		ESPMode::ThreadSafe> InCache,
	TSharedPtr<const IVoxelGenerationOverlay, ESPMode::ThreadSafe> InOverlay,
	const bool bInRequirePlanPreflight)
	: Config(InConfig)
	, Cache(InCache)
	, Overlay(MoveTemp(InOverlay))
	, bRequirePlanPreflight(bInRequirePlanPreflight)
{
}

bool FVoxelGenerationPipeline::QueryReadyWaterfalls(
	const FIntPoint& InMin,
	const FIntPoint& InMax,
	TFunctionRef<bool(const FVoxelWaterfallSemantic&, const FVoxelGenerationWorkspace&, FString&)> InVisit,
	FString& OutError,
	const FVoxelTerrainResourceLease* InOutputLease) const
{
	if (InMin.X > InMax.X || InMin.Y > InMax.Y || !Config->Recipe ||
		!VoxelGeneration::IsWorldCell(InMin.X) || !VoxelGeneration::IsWorldCell(InMin.Y) ||
		!VoxelGeneration::IsWorldCell(InMax.X) || !VoxelGeneration::IsWorldCell(InMax.Y))
	{
		OutError = TEXT("Invalid waterfall query bounds");
		return false;
	}
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	if (!ResolveAllocation(Resources, Consumer, OutError))
	{
		return false;
	}
	const uint64 SourceInputBytes = Cache->GetRootInputBytes();
	const int32 SourceLeaseCount = SourceInputBytes ? 1 : 0;
	uint64 RequiredInputBytes = SourceInputBytes;
	int32 RequiredLeaseCount = SourceLeaseCount;
	uint64 RequiredWorkingBytes = 0;
	if (!AddOutputRequirements(InOutputLease, RequiredInputBytes, RequiredLeaseCount, RequiredWorkingBytes, OutError))
	{
		return false;
	}
	FVoxelGenerationWorkspace Workspace(Resources, Consumer,
		RequiredInputBytes, RequiredLeaseCount, RequiredWorkingBytes);
	TArray<FVoxelWaterfallSemantic> Waterfalls;
	const auto First = FVoxelGenerationQuery::HydrologyKeyForVoxel(InMin.X, InMin.Y, Config->Recipe->Settings);
	const auto Last = FVoxelGenerationQuery::HydrologyKeyForVoxel(InMax.X, InMax.Y, Config->Recipe->Settings);
	for (int64 Y = First.Coordinate.Y; Y <= Last.Coordinate.Y; ++Y)
	{
		for (int64 X = First.Coordinate.X; X <= Last.Coordinate.X; ++X)
		{
			FVoxelHydrologyPlanPtr Plan;
			if (!Cache->FindHydrology({FIntPoint(int32(X), int32(Y))}, Plan) || !Plan)
			{
				continue;
			}
			for (const FVoxelWaterfallSemantic& Fall : Plan->Waterfalls)
			{
				if (Fall.Upstream.X >= InMin.X && Fall.Upstream.Y >= InMin.Y &&
					Fall.Upstream.X <= InMax.X && Fall.Upstream.Y <= InMax.Y)
				{
					if (!AppendPipelineScratch(Workspace, Waterfalls, Fall, OutError))
					{
						return false;
					}
				}
			}
		}
	}
	Workspace.SetRequirements(SourceInputBytes, SourceLeaseCount, 0);
	for (const FVoxelWaterfallSemantic& Fall : Waterfalls)
	{
		if (!InVisit(Fall, Workspace, OutError))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Voxel waterfall output visitor rejected a candidate");
			}
			return false;
		}
	}
	OutError.Reset();
	return true;
}

void FVoxelGenerationPipeline::ReleaseOverlaySectionConsumer(const FIntVector& InSection) const
{
	if (Overlay) Overlay->ReleaseSectionConsumer(InSection);
}

void FVoxelGenerationPipeline::BindRetentionResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer) const
{
	check(IsInGameThread() && InConsumer.IsValid());
	if (RetentionResources)
	{
		check(RetentionResources.Get() == &InResources.Get() && RetentionConsumer == InConsumer);
		return;
	}
	RetentionResources = InResources;
	RetentionConsumer = InConsumer;
}

void FVoxelGenerationPipeline::SetOverlayPlanningServices(FVoxelGenerationPlanningServices InServices) const
{
	if (Overlay) Overlay->SetPlanningServices(MoveTemp(InServices));
}

void FVoxelGenerationPipeline::NotifyOverlayPlanningDependencyReady(const FVoxelGenerationConsumerKey& InConsumer) const
{
	if (Overlay) Overlay->NotifyPlanningDependencyReady(InConsumer);
}

FVoxelOverlayPreflightStatus FVoxelGenerationPipeline::EnsureOverlayConsumerReady(
	const FVoxelGenerationConsumerKey& InConsumer,
	const FVoxelGenerationBounds& InBounds,
	const EVoxelWorkClass InWorkClass,
	const FVoxelTerrainServicePriority& InPriority) const
{
	return Overlay ? Overlay->EnsureConsumerReady(InConsumer, InBounds, InWorkClass, InPriority) : FVoxelOverlayPreflightStatus();
}

void FVoxelGenerationPipeline::ReleaseOverlayConsumer(const FVoxelGenerationConsumerKey& InConsumer) const
{
	if (Overlay) Overlay->ReleaseConsumer(InConsumer);
}

void FVoxelGenerationPipeline::SetTypedOverlayConsumerWakeCallback(
	TFunction<void(const FVoxelGenerationConsumerKey&)> InCallback) const
{
	if (Overlay) Overlay->SetTypedConsumerWakeCallback(MoveTemp(InCallback));
}

void FVoxelGenerationPipeline::SetOverlayServicePriorityDonation(
	TFunction<void(const FVoxelTaskStamp&, EVoxelWorkClass, const FVoxelTerrainServicePriority&)> InCallback) const
{
	if (Overlay) Overlay->SetServicePriorityDonation(MoveTemp(InCallback));
}

void FVoxelGenerationPipeline::AdvanceOverlayConsumerWakes(const double InDeadlineSeconds, const int32 InLimit) const
{
	if (Overlay) Overlay->AdvanceConsumerWakes(InDeadlineSeconds, InLimit);
}

FVoxelOverlayPreflightStatus FVoxelGenerationPipeline::EnsureOverlayBoundsReady(
	const FVoxelGenerationBounds& InBounds, const EVoxelWorkClass InWorkClass, const int32 InSourcePriority) const
{
	return Overlay ? Overlay->EnsureBoundsReady(InBounds, InWorkClass, InSourcePriority) : FVoxelOverlayPreflightStatus();
}

FVoxelOverlayPreflightStatus FVoxelGenerationPipeline::EnsureCoarseOverlayBoundsReady(
	const FVoxelGenerationBounds& InBounds, const EVoxelWorkClass InWorkClass, const int32 InSourcePriority) const
{
	return Overlay ? Overlay->EnsureCoarseBoundsReady(InBounds, InWorkClass, InSourcePriority) : FVoxelOverlayPreflightStatus();
}

FVoxelOverlayPreflightStatus FVoxelGenerationPipeline::EnsureOverlaySectionReady(
	const FIntVector& InSectionCoordinate,
	const EVoxelWorkClass InWorkClass,
	const int32 InSourcePriority,
	const double InDistanceScore,
	const double InForwardScore) const
{
	return Overlay ? Overlay->EnsureSectionReady(InSectionCoordinate, InWorkClass,
		InSourcePriority, InDistanceScore, InForwardScore) :
		FVoxelOverlayPreflightStatus();
}

bool FVoxelGenerationPipeline::GenerateSection(
	const FIntVector& InSectionCoordinate,
	const TArrayView<FVoxelBlockState> OutBaseBlocks,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	const FVoxelTerrainResourceLease* InOutputLease) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_GenerateSection);

	constexpr int32 SectionSide = 16;
	constexpr int32 SectionCellCount =
		SectionSide *
		SectionSide *
		SectionSide;
	if (OutBaseBlocks.Num() != SectionCellCount)
	{
		OutError = TEXT("InvalidResource: generation section requires a caller-owned 4096-cell output span");
		return false;
	}

	const FInt64Vector WideMin(int64(InSectionCoordinate.X) * SectionSide,
		int64(InSectionCoordinate.Y) * SectionSide, int64(InSectionCoordinate.Z) * SectionSide);
	const FInt64Vector WideMax = WideMin + FInt64Vector(SectionSide);
	if (WideMin.X < VoxelGeneration::WorldCellMin || WideMin.Y < VoxelGeneration::WorldCellMin ||
		WideMin.Z < VoxelGeneration::WorldCellMin || WideMax.X > VoxelGeneration::WorldCellMaxExclusive ||
		WideMax.Y > VoxelGeneration::WorldCellMaxExclusive || WideMax.Z > VoxelGeneration::WorldCellMaxExclusive)
	{
		OutError = TEXT("Invalid generation section outside world cell domain");
		return false;
	}
	const FIntVector WorldMin(int32(WideMin.X), int32(WideMin.Y), int32(WideMin.Z));

	const FVoxelGenerationBounds Bounds {
		WorldMin,
		WorldMin +
			FIntVector(
				SectionSide)
	};

	FVoxelGenerationQuery Query;

	if (!FVoxelGenerationQuery::Create(
			Config,
			Cache,
			Query,
			OutError,
			!bRequirePlanPreflight,
			false,
			bRequirePlanPreflight,
			nullptr,
			InOutputLease) ||
		!Query.Prepare(
			Bounds,
			OutError,
			InCancel))
	{
		return false;
	}

	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	if (!ResolveAllocation(Resources, Consumer, OutError))
	{
		return false;
	}
	uint64 RequiredInputBytes = Query.GetRequiredInputBytes();
	int32 RequiredLeaseCount = Query.GetRequiredInputLeaseCount();
	uint64 RequiredWorkingBytes = 0;
	if (!AddOutputRequirements(InOutputLease, RequiredInputBytes, RequiredLeaseCount, RequiredWorkingBytes, OutError))
	{
		return false;
	}
	FVoxelGenerationWorkspace Workspace(Resources, Consumer,
		RequiredInputBytes, RequiredLeaseCount, RequiredWorkingBytes);
	TArray<FVoxelBlockState> Blocks;
	if (!Workspace.ReserveArray(Blocks, SectionCellCount, OutError))
	{
		return false;
	}
	Blocks.SetNumUninitialized(
		SectionCellCount);

	// 一列共享同一份自然采样，每格仍执行完整生成阶段，避免反复争用列缓存锁。
	uint32 Symbols[SectionSide];
	for (int32 Y = 0; Y < SectionSide; ++Y)
	{
		for (int32 X = 0; X < SectionSide; ++X)
		{
			if (!Query.SampleColumnSymbols(FIntPoint(WorldMin.X + X, WorldMin.Y + Y), WorldMin.Z,
				MakeArrayView(Symbols), OutError))
			{
				return false;
			}
			for (int32 Z = 0; Z < SectionSide; ++Z)
			{
				if (!Config->ToRuntime(Symbols[Z], Blocks[X + SectionSide * Y + SectionSide * SectionSide * Z]))
				{
					OutError = TEXT("Voxel generation returned an invalid recipe symbol");
					return false;
				}
			}
		}
	}

	if (Overlay && !Overlay->ApplySection(InSectionCoordinate, MakeArrayView(Blocks),
		OutError, InCancel))
	{
		return false;
	}

	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	CopyPipelineOutput(Blocks, OutBaseBlocks);

	OutError.Reset();
	return true;
}

bool FVoxelGenerationPipeline::SampleColumn(
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& OutColumn,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (InX == MAX_int32 || InY == MAX_int32)
	{
		OutError = TEXT("Voxel column exceeds coordinate range");
		return false;
	}

	const FVoxelGenerationBounds Bounds {
		FIntVector(
			InX,
			InY,
			Config->Recipe->
				Settings.MinZ),
		FIntVector(
			InX + 1,
			InY + 1,
			Config->Recipe->
				Settings.MaxZ)
	};

	FVoxelGenerationQuery Query;

	if (!FVoxelGenerationQuery::Create(
			Config,
			Cache,
			Query,
			OutError) ||
		!Query.PrepareColumns(
			Bounds,
			OutError,
			InCancel))
	{
		return false;
	}

	return Query.SampleColumn(
		InX,
		InY,
		OutColumn,
		OutError);
}

bool FVoxelGenerationPipeline::EnumerateTrees(
	const FVoxelGenerationBounds& InBounds,
	TFunctionRef<void(const FIntVector&, int32, FVoxelStableId)> InVisit,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	FVoxelGenerationQuery Query;
	if (!FVoxelGenerationQuery::Create(Config, Cache, Query, OutError, true, bRequirePlanPreflight)) return false;
	bool bSampleFailed = false;
	auto SampleColumn = [&](const FIntVector& Position, FVoxelColumnSample& Column)
	{
		if (!Query.SampleEnvironmentColumn(Position.X, Position.Y, Column, OutError, InCancel))
		{
			bSampleFailed = true;
			return false;
		}
		return true;
	};
	auto SampleBase = [&](const FIntVector& Position, uint32& Symbol)
	{
		FVoxelColumnSample Column;
		if (!SampleColumn(Position, Column)) return false;
		Symbol = Position.Z <= Column.SurfaceZ ? Config->Recipe->Palette.Stone : Config->Recipe->Palette.Air;
		return true;
	};
	const FVoxelEcologyGenerator Ecology(Config->Recipe.ToSharedRef());
	int32 Candidates = 0;
	int32 Accepted = 0;
	Ecology.EnumerateTrees(InBounds, SampleColumn, SampleBase,
		[&](const FIntVector& Anchor, const int32 Height, const FVoxelStableId Id)
		{
			InVisit(Anchor, Height, Id);
		}, Candidates, Accepted, InCancel);
	if (bSampleFailed) return false;
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FVoxelGenerationPipeline::SampleEnvironment(
	const int32 InX,
	const int32 InY,
	FVoxelEnvironmentSample& OutSample,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (InX == MAX_int32 || InY == MAX_int32)
	{
		OutError = TEXT("Voxel environment exceeds coordinate range");
		return false;
	}

	FVoxelGenerationQuery Query;
	if (!FVoxelGenerationQuery::Create(Config, Cache, Query, OutError))
	{
		return false;
	}
	return Query.SampleEnvironmentColumn(
		InX, InY, OutSample.Column, OutError, InCancel);
}

bool FVoxelGenerationPipeline::TrySampleEnvironment(const FIntPoint& InCell, FVoxelEnvironmentSample& OutSample) const
{
	FVoxelNaturalColumnEntryPtr Entry;
	if (!Cache->FindNaturalColumn(InCell, Entry)) return false;
	OutSample.Column = Entry->Column;
	return true;
}

TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe>
FVoxelGenerationPipeline::RetainHydrologyForPlanning(
	const FIntPoint& InCenter,
	const int32 InSampleRadiusCells) const
{
	if (!RetentionResources || !RetentionConsumer.IsValid()) return nullptr;
	const FVoxelGenerationSettings& Settings = Config->Recipe->Settings;
	const int64 RegionSide = static_cast<int64>(FMath::Max(8, Settings.HydrologyRegionSide)) *
		FMath::Max(1, Settings.HydrologyCellSize);
	FVoxelGenerationCacheRetentionPoint Point;
	Point.Center = InCenter;
	Point.HydrologyRadiusCells = static_cast<int32>(FMath::Min<int64>(MAX_int32,
		static_cast<int64>(FMath::Max(0, InSampleRadiusCells)) + RegionSide));
	auto Consumer = RetentionConsumer;
	if (const auto* Stamp = FVoxelGenerationPlanningReadScope::GetTaskStamp(); Stamp &&
		Stamp->OwnerId.IsValid() && Stamp->OwnerIncarnation != 0 && Stamp->Token != 0)
	{
		if (Stamp->WorldEpoch != RetentionConsumer.WorldEpoch) return nullptr;
		Consumer = {EVoxelGenerationConsumerKind::WorldFact, Stamp->WorldEpoch, Stamp->OwnerId, Stamp->OwnerIncarnation,
			EVoxelGenerationSpatialKind::Section, Stamp->Section, Stamp->Section, Stamp->Token};
	}
	if (const auto* CapturedConsumer = FVoxelGenerationPlanningReadScope::GetConsumerKey())
	{
		if (!CapturedConsumer->IsValid() || CapturedConsumer->WorldEpoch != RetentionConsumer.WorldEpoch) return nullptr;
		// 重试使用原逻辑消费者，不能把本次任务 Token 的释放伪装成外部容量事件。
		Consumer = *CapturedConsumer;
	}
	if (const auto* ScopeConsumer = FVoxelGenerationConsumerScope::GetConsumerKey())
	{
		if (!ScopeConsumer->IsValid() || ScopeConsumer->WorldEpoch != RetentionConsumer.WorldEpoch ||
			(FVoxelGenerationPlanningReadScope::GetConsumerKey() && !(Consumer == *ScopeConsumer))) return nullptr;
		Consumer = *ScopeConsumer;
	}
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Lease;
	FString Error;
	return Cache->RetainForTask(Point, RetentionResources.ToSharedRef(), Consumer, Lease, Error) ? Lease : nullptr;
}

bool FVoxelGenerationPipeline::SampleEnvironments(
	const FIntPoint& InOrigin,
	const int32 InWidth,
	const int32 InHeight,
	const int32 InStep,
	const TArrayView<FVoxelEnvironmentSample> OutSamples,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	const bool bInUseColumnCache,
	const FVoxelTerrainResourceLease* InOutputLease) const
{
	FVoxelGenerationBounds Bounds;
	int32 Count = 0;
	if (!MakeColumnGridBounds(Config->Recipe->Settings, InOrigin,
		InWidth, InHeight, InStep, Bounds, Count, OutError))
	{
		return false;
	}
	if (OutSamples.Num() != Count)
	{
		OutError = TEXT("InvalidResource: environment sampling requires the complete caller-owned output span");
		return false;
	}

	FVoxelGenerationQuery Query;
	if (!FVoxelGenerationQuery::Create(Config, Cache, Query, OutError,
			bInUseColumnCache, bRequirePlanPreflight, false, nullptr, InOutputLease) ||
		!Query.PrepareColumns(Bounds, OutError, InCancel))
	{
		return false;
	}

	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	if (!ResolveAllocation(Resources, Consumer, OutError))
	{
		return false;
	}
	uint64 RequiredInputBytes = Query.GetRequiredInputBytes();
	int32 RequiredLeaseCount = Query.GetRequiredInputLeaseCount();
	uint64 RequiredWorkingBytes = 0;
	if (!AddOutputRequirements(InOutputLease, RequiredInputBytes, RequiredLeaseCount, RequiredWorkingBytes, OutError))
	{
		return false;
	}
	FVoxelGenerationWorkspace Workspace(Resources, Consumer,
		RequiredInputBytes, RequiredLeaseCount, RequiredWorkingBytes);
	TArray<FVoxelEnvironmentSample> Samples;
	if (!Workspace.ReserveArray(Samples, Count, OutError))
	{
		return false;
	}
	Samples.SetNumUninitialized(Count);
	int32 Index = 0;
	for (int32 Y = 0; Y < InHeight; ++Y)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		for (int32 X = 0; X < InWidth; ++X)
		{
			const int32 WorldX = static_cast<int32>(
				static_cast<int64>(InOrigin.X) + static_cast<int64>(X) * InStep);
			const int32 WorldY = static_cast<int32>(
				static_cast<int64>(InOrigin.Y) + static_cast<int64>(Y) * InStep);
			if (!Query.SampleEnvironmentColumn(WorldX, WorldY,
				Samples[Index++].Column, OutError, InCancel))
			{
				return false;
			}
		}
	}

	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	CopyPipelineOutput(Samples, OutSamples);
	OutError.Reset();
	return true;
}

bool FVoxelGenerationPipeline::SampleColumns(
	const FIntPoint& InOrigin,
	const int32 InWidth,
	const int32 InHeight,
	const int32 InStep,
	const TArrayView<FVoxelColumnSample> OutColumns,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	const bool bInUseColumnCache,
	const FVoxelTerrainResourceLease* InOutputLease) const
{
	FVoxelGenerationBounds Bounds;
	int32 Count = 0;
	if (!MakeColumnGridBounds(Config->Recipe->Settings, InOrigin,
		InWidth, InHeight, InStep, Bounds, Count, OutError))
	{
		return false;
	}
	if (OutColumns.Num() != Count)
	{
		OutError = TEXT("InvalidResource: column sampling requires the complete caller-owned output span");
		return false;
	}

	FVoxelGenerationQuery Query;

	if (!FVoxelGenerationQuery::Create(
			Config,
			Cache,
			Query,
			OutError,
			bInUseColumnCache,
			bRequirePlanPreflight,
			false,
			nullptr,
			InOutputLease) ||
		!Query.PrepareColumns(
			Bounds,
			OutError,
			InCancel))
	{
		return false;
	}

	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	if (!ResolveAllocation(Resources, Consumer, OutError))
	{
		return false;
	}
	uint64 RequiredInputBytes = Query.GetRequiredInputBytes();
	int32 RequiredLeaseCount = Query.GetRequiredInputLeaseCount();
	uint64 RequiredWorkingBytes = 0;
	if (!AddOutputRequirements(InOutputLease, RequiredInputBytes, RequiredLeaseCount, RequiredWorkingBytes, OutError))
	{
		return false;
	}
	FVoxelGenerationWorkspace Workspace(Resources, Consumer,
		RequiredInputBytes, RequiredLeaseCount, RequiredWorkingBytes);
	TArray<FVoxelColumnSample> Columns;
	if (!Workspace.ReserveArray(Columns, Count, OutError))
	{
		return false;
	}
	Columns.SetNumUninitialized(Count);

	int32 Index = 0;

	for (int32 Y = 0;
		Y < InHeight;
		++Y)
	{
		if (InCancel &&
			InCancel->Load())
		{
			OutError =
				TEXT("Canceled");

			return false;
		}

		for (int32 X = 0;
			X < InWidth;
			++X)
		{
			if (!Query.SampleColumn(
				static_cast<int32>(static_cast<int64>(InOrigin.X) + static_cast<int64>(X) * InStep),
				static_cast<int32>(static_cast<int64>(InOrigin.Y) + static_cast<int64>(Y) * InStep),
				Columns[Index++],
				OutError))
			{
				return false;
			}
		}
	}

	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	CopyPipelineOutput(Columns, OutColumns);

	OutError.Reset();
	return true;
}

bool FVoxelGenerationPipeline::SampleBlock(
	const FIntVector& InPosition,
	FVoxelBlockState& OutState,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (!VoxelGeneration::IsWorldCell(InPosition.X) || !VoxelGeneration::IsWorldCell(InPosition.Y) ||
		!VoxelGeneration::IsWorldCell(InPosition.Z))
	{
		OutError = TEXT("Invalid block position outside world cell domain");
		return false;
	}
	FVoxelGenerationQuery Query;

	const FVoxelGenerationBounds Bounds {
		InPosition,
		InPosition +
			FIntVector(1)
	};

	if (!FVoxelGenerationQuery::Create(
			Config,
			Cache,
			Query,
			OutError) ||
		!Query.Prepare(
			Bounds,
			OutError,
			InCancel))
	{
		return false;
	}

	uint32 RecipeValue = 0;

	if (!Query.SampleSymbol(
			InPosition,
			RecipeValue,
			OutError) ||
		!Config->ToRuntime(
			RecipeValue,
			OutState))
	{
		if (OutError.IsEmpty())
		{
			OutError =
				TEXT("Voxel generation returned an invalid recipe symbol");
		}

		return false;
	}

	OutError.Reset();
	return true;
}

bool FVoxelGenerationPipeline::BuildCoarseOverlay(
	const FIntVector& InOrigin,
	const int32 InStep,
	const int32 InGridSide,
	TMap<FIntVector, FVoxelBlockState>& OutCells,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (Overlay)
	{
		return Overlay->BuildCoarse(InOrigin, InStep, InGridSide, OutCells, OutError, InCancel);
	}
	OutCells.Reset();
	OutError.Reset();
	return true;
}

void FVoxelGenerationPipeline::SetOverlayConsumerWakeCallback(TFunction<void(const FIntVector&)> InCallback) const
{
	if (Overlay)
	{
		Overlay->SetConsumerWakeCallback(MoveTemp(InCallback));
	}
}

#if WITH_DEV_AUTOMATION_TESTS
bool FVoxelGenerationPipeline::GenerateSection(
	const FIntVector& InSectionCoordinate,
	TArray<FVoxelBlockState>& OutBaseBlocks,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (!AllowLegacyPipelineArrayOutput(Cache, OutError))
	{
		return false;
	}
	TArray<FVoxelBlockState> Candidate;
	Candidate.SetNumUninitialized(16 * 16 * 16);
	if (!GenerateSection(InSectionCoordinate, MakeArrayView(Candidate), OutError, InCancel))
	{
		return false;
	}
	OutBaseBlocks = MoveTemp(Candidate);
	return true;
}

bool FVoxelGenerationPipeline::SampleEnvironments(
	const FIntPoint& InOrigin,
	const int32 InWidth,
	const int32 InHeight,
	const int32 InStep,
	TArray<FVoxelEnvironmentSample>& OutSamples,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	const bool bInUseColumnCache) const
{
	if (!AllowLegacyPipelineArrayOutput(Cache, OutError))
	{
		return false;
	}
	FVoxelGenerationBounds Bounds;
	int32 Count = 0;
	if (!MakeColumnGridBounds(Config->Recipe->Settings, InOrigin,
		InWidth, InHeight, InStep, Bounds, Count, OutError))
	{
		return false;
	}
	TArray<FVoxelEnvironmentSample> Candidate;
	Candidate.SetNumUninitialized(Count);
	if (!SampleEnvironments(InOrigin, InWidth, InHeight, InStep,
		MakeArrayView(Candidate), OutError, InCancel, bInUseColumnCache))
	{
		return false;
	}
	OutSamples = MoveTemp(Candidate);
	return true;
}

bool FVoxelGenerationPipeline::SampleColumns(
	const FIntPoint& InOrigin,
	const int32 InWidth,
	const int32 InHeight,
	const int32 InStep,
	TArray<FVoxelColumnSample>& OutColumns,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	const bool bInUseColumnCache) const
{
	if (!AllowLegacyPipelineArrayOutput(Cache, OutError))
	{
		return false;
	}
	FVoxelGenerationBounds Bounds;
	int32 Count = 0;
	if (!MakeColumnGridBounds(Config->Recipe->Settings, InOrigin,
		InWidth, InHeight, InStep, Bounds, Count, OutError))
	{
		return false;
	}
	TArray<FVoxelColumnSample> Candidate;
	Candidate.SetNumUninitialized(Count);
	if (!SampleColumns(InOrigin, InWidth, InHeight, InStep,
		MakeArrayView(Candidate), OutError, InCancel, bInUseColumnCache))
	{
		return false;
	}
	OutColumns = MoveTemp(Candidate);
	return true;
}

bool FVoxelGenerationPipeline::QueryReadyWaterfalls(
	const FIntPoint& InMin,
	const FIntPoint& InMax,
	TArray<FVoxelWaterfallSemantic>& OutWaterfalls,
	FString& OutError) const
{
	if (!AllowLegacyPipelineArrayOutput(Cache, OutError))
	{
		return false;
	}
	TArray<FVoxelWaterfallSemantic> Candidate;
	if (!QueryReadyWaterfalls(InMin, InMax,
		[&Candidate](const FVoxelWaterfallSemantic& InFall, const FVoxelGenerationWorkspace&, FString&)
		{
			Candidate.Add(InFall);
			return true;
		}, OutError))
	{
		return false;
	}
	OutWaterfalls = MoveTemp(Candidate);
	return true;
}

bool FVoxelGenerationPipeline::EnumerateCaveEntrances(
	const FIntPoint& InMin,
	const FIntPoint& InMax,
	TArray<FVoxelCaveEntrance>& OutEntrances,
	FString& OutError) const
{
	if (!AllowLegacyPipelineArrayOutput(Cache, OutError))
	{
		return false;
	}
	TArray<FVoxelCaveEntrance> Candidate;
	if (!EnumerateCaveEntrances(InMin, InMax,
		[&Candidate](const FVoxelCaveEntrance& InEntrance, const FVoxelGenerationWorkspace&, FString&)
		{
			Candidate.Add(InEntrance);
			return true;
		}, OutError))
	{
		return false;
	}
	OutEntrances = MoveTemp(Candidate);
	return true;
}
#endif
