#include "Voxel/Generation/VoxelGenerationPlanCache.h"

#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	constexpr int32 GenerationPlanTileSide = 256;
	constexpr int32 LakeAnchorSide = 64;

	template<typename KeyType, typename ValueType>
	using TConstCachePtr =
		TSharedPtr<
			const ValueType,
			ESPMode::ThreadSafe>;

	template<typename KeyType, typename ValueType, typename BuildGateType>
	bool GetOrBuildValue(
		FRWLock& InLock,
		TMap<KeyType, TConstCachePtr<KeyType, ValueType>>& InValues,
		TMap<KeyType, TSharedPtr<BuildGateType, ESPMode::ThreadSafe>>& InBuilds,
		TArray<KeyType>& InKeys,
		const KeyType& InKey,
		TFunctionRef<bool(ValueType&, FString&)> InBuild,
		TConstCachePtr<KeyType, ValueType>& OutValue,
		FString& OutError,
		const TAtomic<bool>* InCancel,
		const bool bInAllowGameThreadBuilds,
		const bool bInAllowWorkerWait,
		TFunctionRef<void(uint64)> InRecordWait,
		TFunction<void(bool)> InRecordLookup = {})
	{
		{
			FReadScopeLock Scope(InLock);

			if (const TConstCachePtr<KeyType, ValueType>* Found =
				InValues.Find(InKey))
			{
				OutValue = *Found;
				OutError.Reset();
				if (InRecordLookup) InRecordLookup(OutValue.IsValid());
				return OutValue.IsValid();
			}
		}
		if (InRecordLookup) InRecordLookup(false);

		// 运行时主线程只读取已发布值，冷查询不得创建构建门闩或执行构建器。
		if (!bInAllowGameThreadBuilds && IsInGameThread())
		{
			OutValue.Reset();
			OutError = TEXT("DependencyNotReady: voxel generation cache key is not published");
			return false;
		}

		TSharedPtr<BuildGateType, ESPMode::ThreadSafe> Gate;
		bool bOwner = false;

		{
			FWriteScopeLock Scope(InLock);

			if (const TConstCachePtr<KeyType, ValueType>* Found =
				InValues.Find(InKey))
			{
				OutValue = *Found;
				OutError.Reset();
				return OutValue.IsValid();
			}

			if (TSharedPtr<BuildGateType, ESPMode::ThreadSafe>* Existing =
				InBuilds.Find(InKey))
			{
				Gate = *Existing;
			}
			else
			{
				Gate =
					MakeShared<
						BuildGateType,
						ESPMode::ThreadSafe>();

				InBuilds.Add(
					InKey,
					Gate);

				bOwner = true;
			}
		}

		if (!bOwner)
		{
			if (!bInAllowWorkerWait)
			{
				OutError = TEXT("DependencyNotReady: voxel generation cache key is building");
				return false;
			}
			const double WaitStart =
				FPlatformTime::Seconds();

			while (!Gate->Event->Wait(2))
			{
				if (InCancel &&
					InCancel->Load())
				{
					OutError =
						TEXT("Canceled");

					return false;
				}
			}

			const uint64 WaitMicroseconds =
				static_cast<uint64>(
					FMath::Max(
						0.0,
						(FPlatformTime::Seconds() -
							WaitStart) *
							1000000.0));

			InRecordWait(
				WaitMicroseconds);

			if (!Gate->bSuccess)
			{
				OutError =
					Gate->Error;

				return false;
			}

			FReadScopeLock Scope(
				InLock);

			if (const TConstCachePtr<KeyType, ValueType>* Found =
				InValues.Find(InKey))
			{
				OutValue =
					*Found;

				OutError.Reset();
				return OutValue.IsValid();
			}

			OutError =
				TEXT(
					"Voxel generation cache build completed without publishing a value");

			return false;
		}

		ValueType LocalValue;
		FString BuildError;

		const bool bSuccess =
			(!InCancel || !InCancel->Load()) &&
			InBuild(
				LocalValue,
				BuildError);

		TConstCachePtr<KeyType, ValueType> BuiltValue;

		if (bSuccess)
		{
			BuiltValue =
				MakeShared<
					const ValueType,
					ESPMode::ThreadSafe>(
						MoveTemp(
							LocalValue));
		}

		{
			FWriteScopeLock Scope(
				InLock);

			if (BuiltValue)
			{
				const bool bWasAbsent =
					!InValues.Contains(
						InKey);

				InValues.Add(
					InKey,
					BuiltValue);

				if (bWasAbsent)
				{
					InKeys.Add(
						InKey);
				}
			}

			Gate->bSuccess =
				BuiltValue.IsValid();

			Gate->Error =
				Gate->bSuccess
					? FString()
					: BuildError;

			InBuilds.Remove(
				InKey);
		}

		/**
		 * Trigger 在锁外。
		 */
		Gate->Event->Trigger();

		if (!Gate->bSuccess)
		{
			OutError =
				Gate->Error;

			return false;
		}

		OutValue =
			MoveTemp(
				BuiltValue);

		OutError.Reset();
		return true;
	}

	template<typename KeyType, typename ValueType, typename KeepPredicate>
	void TrimMapBudgeted(
		TMap<KeyType, TSharedPtr<const ValueType, ESPMode::ThreadSafe>>& InValues,
		TArray<KeyType>& InKeys,
		int32& InOutCursor,
		int32& InOutBudget,
		TArray<TSharedPtr<const ValueType, ESPMode::ThreadSafe>>& OutRetired,
		KeepPredicate&& InKeep)
	{
		while (InOutBudget > 0 &&
			!InKeys.IsEmpty())
		{
			InOutCursor =
				FMath::Clamp(
					InOutCursor,
					0,
					InKeys.Num() - 1);

			const KeyType Key =
				InKeys[
					InOutCursor];

			--InOutBudget;

			if (!InValues.Contains(Key))
			{
				InKeys.RemoveAtSwap(
					InOutCursor,
					1,
					EAllowShrinking::No);

				if (InKeys.IsEmpty())
				{
					InOutCursor = 0;
				}

				continue;
			}

			if (!InKeep(Key))
			{
				if (TSharedPtr<const ValueType, ESPMode::ThreadSafe>* Retired =
					InValues.Find(Key))
				{
					/**
					 * 把最后一个 shared ref Move 到锁外释放。
					 * 避免大型 plan/array 在 shard lock 内析构。
					 */
					OutRetired.Add(
						MoveTemp(*Retired));
				}

				InValues.Remove(
					Key);

				InKeys.RemoveAtSwap(
					InOutCursor,
					1,
					EAllowShrinking::No);

				if (InKeys.IsEmpty())
				{
					InOutCursor = 0;
				}

				continue;
			}

			InOutCursor =
				(InOutCursor + 1) %
				InKeys.Num();
		}
	}
}

struct FVoxelGenerationPlanCache::FBuildGate
{
	FBuildGate()
	{
		Event =
			FPlatformProcess::
				GetSynchEventFromPool(
					true);
	}

	~FBuildGate()
	{
		if (Event)
		{
			FPlatformProcess::
				ReturnSynchEventToPool(
					Event);

			Event = nullptr;
		}
	}

	FEvent* Event = nullptr;
	bool bSuccess = false;
	FString Error;
};

FVoxelGenerationPlanCache::FVoxelGenerationPlanCache(
	const bool bInAllowGameThreadBuilds,
	const bool bInAllowWorkerWait)
	: bAllowGameThreadBuilds(
		bInAllowGameThreadBuilds)
	, bAllowWorkerWait(bInAllowWorkerWait)
{
	for (int32 Index = 0; Index < 5; ++Index)
	{
		PlanHits[Index].Store(0);
		PlanMisses[Index].Store(0);
		PlanDonations[Index].Store(0);
	}
	NaturalShards.SetNum(
		ShardCount);

	PlanShards.SetNum(
		ShardCount);

	for (int32 Index = 0;
		Index < ShardCount;
		++Index)
	{
		NaturalShards[Index] =
			MakeUnique<FNaturalShard>();

		PlanShards[Index] =
			MakeUnique<FPlanShard>();
	}
}

FVoxelGenerationPlanCache::~FVoxelGenerationPlanCache()
{
	Reset();
}

int32 FVoxelGenerationPlanCache::NaturalShardIndex(
	const FIntPoint& InKey) const
{
	return
		static_cast<int32>(
			GetTypeHash(InKey) %
			ShardCount);
}

int32 FVoxelGenerationPlanCache::PlanShardIndex(
	const FVoxelHydrologyRegionKey& InKey) const
{
	return
		static_cast<int32>(
			GetTypeHash(InKey) %
			ShardCount);
}

int32 FVoxelGenerationPlanCache::PlanShardIndex(
	const FVoxelGenerationTileKey& InKey) const
{
	return
		static_cast<int32>(
			GetTypeHash(InKey) %
			ShardCount);
}

int32 FVoxelGenerationPlanCache::PlanShardIndex(
	const FVoxelEcologyTileKey& InKey) const
{
	return static_cast<int32>(GetTypeHash(InKey) % ShardCount);
}

void FVoxelGenerationPlanCache::RecordGateWait(
	const uint64 InMicroseconds)
{
	++GateWaitCount;
	GateWaitMicroseconds.AddExchange(
		InMicroseconds);
}

FVoxelPlanDiagnostics FVoxelGenerationPlanCache::GetPlanDiagnostics(const EVoxelGenerationPlanKind InKind) const
{
	const int32 Index = static_cast<int32>(InKind);
	FScopeLock Lock(&PlanDiagnosticsMutex);
	FVoxelPlanDiagnostics Result = PlanBuildDiagnostics[Index];
	Result.HitCount = PlanHits[Index].Load();
	Result.MissCount = PlanMisses[Index].Load();
	Result.PriorityDonations = PlanDonations[Index].Load();
	return Result;
}

void FVoxelGenerationPlanCache::RecordPlanLookup(const EVoxelGenerationPlanKind InKind, const bool bInHit) const
{
	(bInHit ? PlanHits[static_cast<int32>(InKind)] : PlanMisses[static_cast<int32>(InKind)]).AddExchange(1);
}

void FVoxelGenerationPlanCache::RecordPlanBuild(const EVoxelGenerationPlanKind InKind,
	const double InMilliseconds, const bool bInFailed)
{
	FScopeLock Lock(&PlanDiagnosticsMutex);
	PlanBuildDiagnostics[static_cast<int32>(InKind)].RecordBuild(InMilliseconds, bInFailed);
}

void FVoxelGenerationPlanCache::RecordPlanDonation(const EVoxelGenerationPlanKind InKind)
{
	PlanDonations[static_cast<int32>(InKind)].AddExchange(1);
}

bool FVoxelGenerationPlanCache::GetOrBuildBaseColumn(
	const FIntPoint& InPosition,
	TFunctionRef<bool(
		FVoxelBaseColumnEntry&,
		FString&)> InBuild,
	FVoxelBaseColumnEntryPtr& OutEntry,
	FString& OutError,
	const TAtomic<bool>* InCancel)
{
	FNaturalShard& Shard =
		*NaturalShards[
			NaturalShardIndex(
				InPosition)];

	return GetOrBuildValue<
		FIntPoint,
		FVoxelBaseColumnEntry,
		FBuildGate>(
			Shard.Lock,
			Shard.BaseColumns,
			Shard.BaseColumnBuilds,
			Shard.BaseColumnKeys,
			InPosition,
			InBuild,
			OutEntry,
			OutError,
			InCancel,
			bAllowGameThreadBuilds,
			bAllowWorkerWait,
			[this](const uint64 InWait)
			{
				RecordGateWait(InWait);
			});
}

bool FVoxelGenerationPlanCache::GetOrBuildNaturalColumn(
	const FIntPoint& InPosition,
	TFunctionRef<bool(
		FVoxelNaturalColumnEntry&,
		FString&)> InBuild,
	FVoxelNaturalColumnEntryPtr& OutEntry,
	FString& OutError,
	const TAtomic<bool>* InCancel)
{
	FNaturalShard& Shard =
		*NaturalShards[
			NaturalShardIndex(
				InPosition)];

	return GetOrBuildValue<
		FIntPoint,
		FVoxelNaturalColumnEntry,
		FBuildGate>(
			Shard.Lock,
			Shard.NaturalColumns,
			Shard.NaturalColumnBuilds,
			Shard.NaturalColumnKeys,
			InPosition,
			InBuild,
			OutEntry,
			OutError,
			InCancel,
			bAllowGameThreadBuilds,
			bAllowWorkerWait,
			[this](const uint64 InWait)
			{
				RecordGateWait(InWait);
			});
}

bool FVoxelGenerationPlanCache::FindNaturalColumn(const FIntPoint& InPosition, FVoxelNaturalColumnEntryPtr& OutEntry) const
{
	const FNaturalShard& Shard = *NaturalShards[NaturalShardIndex(InPosition)];
	FReadScopeLock Scope(Shard.Lock);
	const FVoxelNaturalColumnEntryPtr* Found = Shard.NaturalColumns.Find(InPosition);
	OutEntry = Found ? *Found : nullptr;
	return OutEntry.IsValid();
}

bool FVoxelGenerationPlanCache::FindHydrology(
	const FVoxelHydrologyRegionKey& InKey,
	FVoxelHydrologyPlanPtr& OutPlan) const
{
	const FPlanShard& Shard =
		*PlanShards[
			PlanShardIndex(InKey)];

	FReadScopeLock Scope(
		Shard.Lock);

	if (const FVoxelHydrologyPlanPtr* Found =
		Shard.Hydrology.Find(InKey))
	{
		OutPlan = *Found;
		RecordPlanLookup(EVoxelGenerationPlanKind::Hydrology, OutPlan.IsValid());
		return OutPlan.IsValid();
	}

	RecordPlanLookup(EVoxelGenerationPlanKind::Hydrology, false);
	return false;
}

void FVoxelGenerationPlanCache::StoreHydrology(
	const FVoxelHydrologyRegionKey& InKey,
	FVoxelHydrologyPlanPtr InPlan)
{
	FPlanShard& Shard =
		*PlanShards[
			PlanShardIndex(InKey)];

	FWriteScopeLock Scope(
		Shard.Lock);

	const bool bNew =
		!Shard.Hydrology.Contains(
			InKey);

	Shard.Hydrology.Add(
		InKey,
		MoveTemp(InPlan));

	if (bNew)
	{
		Shard.HydrologyKeys.Add(
			InKey);
	}
}

bool FVoxelGenerationPlanCache::GetOrBuildHydrology(
	const FVoxelHydrologyRegionKey& InKey,
	TFunctionRef<bool(
		FVoxelHydrologyPlan&,
		FString&)> InBuild,
	FVoxelHydrologyPlanPtr& OutPlan,
	FString& OutError,
	const TAtomic<bool>* InCancel)
{
	FPlanShard& Shard =
		*PlanShards[
			PlanShardIndex(InKey)];

	return GetOrBuildValue<
		FVoxelHydrologyRegionKey,
		FVoxelHydrologyPlan,
		FBuildGate>(
			Shard.Lock,
			Shard.Hydrology,
			Shard.HydrologyBuilds,
			Shard.HydrologyKeys,
			InKey,
			[this, &InBuild](FVoxelHydrologyPlan& Value, FString& Error)
			{
				const double Started = FPlatformTime::Seconds();
				const bool bSuccess = InBuild(Value, Error);
				if (bSuccess || (!Error.StartsWith(TEXT("DependencyNotReady")) && Error != TEXT("Canceled")))
					RecordPlanBuild(EVoxelGenerationPlanKind::Hydrology, (FPlatformTime::Seconds() - Started) * 1000.0, !bSuccess);
				return bSuccess;
			},
			OutPlan,
			OutError,
			InCancel,
			bAllowGameThreadBuilds,
			bAllowWorkerWait,
			[this](const uint64 InWait)
			{
				RecordGateWait(InWait);
			}, [this](const bool bHit) { RecordPlanLookup(EVoxelGenerationPlanKind::Hydrology, bHit); });
}

bool FVoxelGenerationPlanCache::FindCave(
	const FVoxelGenerationTileKey& InKey,
	FVoxelCavePlanPtr& OutPlan) const
{
	const FPlanShard& Shard =
		*PlanShards[
			PlanShardIndex(InKey)];

	FReadScopeLock Scope(
		Shard.Lock);

	if (const FVoxelCavePlanPtr* Found =
		Shard.Caves.Find(InKey))
	{
		OutPlan = *Found;
		RecordPlanLookup(EVoxelGenerationPlanKind::Cave, OutPlan.IsValid());
		return OutPlan.IsValid();
	}

	RecordPlanLookup(EVoxelGenerationPlanKind::Cave, false);
	return false;
}

void FVoxelGenerationPlanCache::StoreCave(
	const FVoxelGenerationTileKey& InKey,
	FVoxelCavePlanPtr InPlan)
{
	FPlanShard& Shard =
		*PlanShards[
			PlanShardIndex(InKey)];

	FWriteScopeLock Scope(
		Shard.Lock);

	const bool bNew =
		!Shard.Caves.Contains(InKey);

	Shard.Caves.Add(
		InKey,
		MoveTemp(InPlan));

	if (bNew)
	{
		Shard.CaveKeys.Add(
			InKey);
	}
}

bool FVoxelGenerationPlanCache::GetOrBuildCave(
	const FVoxelGenerationTileKey& InKey,
	TFunctionRef<bool(
		FVoxelCavePlan&,
		FString&)> InBuild,
	FVoxelCavePlanPtr& OutPlan,
	FString& OutError,
	const TAtomic<bool>* InCancel)
{
	FPlanShard& Shard =
		*PlanShards[
			PlanShardIndex(InKey)];

	return GetOrBuildValue<
		FVoxelGenerationTileKey,
		FVoxelCavePlan,
		FBuildGate>(
			Shard.Lock,
			Shard.Caves,
			Shard.CaveBuilds,
			Shard.CaveKeys,
			InKey,
			[this, &InBuild](FVoxelCavePlan& Value, FString& Error)
			{
				const double Started = FPlatformTime::Seconds();
				const bool bSuccess = InBuild(Value, Error);
				if (bSuccess || (!Error.StartsWith(TEXT("DependencyNotReady")) && Error != TEXT("Canceled")))
					RecordPlanBuild(EVoxelGenerationPlanKind::Cave, (FPlatformTime::Seconds() - Started) * 1000.0, !bSuccess);
				return bSuccess;
			},
			OutPlan,
			OutError,
			InCancel,
			bAllowGameThreadBuilds,
			bAllowWorkerWait,
			[this](const uint64 InWait)
			{
				RecordGateWait(InWait);
			}, [this](const bool bHit) { RecordPlanLookup(EVoxelGenerationPlanKind::Cave, bHit); });
}

bool FVoxelGenerationPlanCache::FindFeature(
	const FVoxelGenerationTileKey& InKey,
	FVoxelFeaturePlanPtr& OutPlan) const
{
	const FPlanShard& Shard =
		*PlanShards[
			PlanShardIndex(InKey)];

	FReadScopeLock Scope(
		Shard.Lock);

	if (const FVoxelFeaturePlanPtr* Found =
		Shard.Features.Find(InKey))
	{
		OutPlan = *Found;
		RecordPlanLookup(EVoxelGenerationPlanKind::Feature, OutPlan.IsValid());
		return OutPlan.IsValid();
	}

	RecordPlanLookup(EVoxelGenerationPlanKind::Feature, false);
	return false;
}

bool FVoxelGenerationPlanCache::FindEcology(
	const FVoxelEcologyTileKey& InKey,
	FVoxelEcologyPlanPtr& OutPlan) const
{
	const FPlanShard& Shard = *PlanShards[PlanShardIndex(InKey)];
	FReadScopeLock Scope(Shard.Lock);
	if (const FVoxelEcologyPlanPtr* Found = Shard.Ecology.Find(InKey))
	{
		OutPlan = *Found;
		RecordPlanLookup(EVoxelGenerationPlanKind::Ecology, OutPlan.IsValid());
		return OutPlan.IsValid();
	}
	RecordPlanLookup(EVoxelGenerationPlanKind::Ecology, false);
	return false;
}

bool FVoxelGenerationPlanCache::GetOrBuildEcology(
	const FVoxelEcologyTileKey& InKey,
	TFunctionRef<bool(FVoxelEcologyPlan&, FString&)> InBuild,
	FVoxelEcologyPlanPtr& OutPlan,
	FString& OutError,
	const TAtomic<bool>* InCancel)
{
	FPlanShard& Shard = *PlanShards[PlanShardIndex(InKey)];
	return GetOrBuildValue<FVoxelEcologyTileKey, FVoxelEcologyPlan, FBuildGate>(
		Shard.Lock,
		Shard.Ecology,
		Shard.EcologyBuilds,
		Shard.EcologyKeys,
		InKey,
			[this, &InBuild](FVoxelEcologyPlan& Value, FString& Error)
			{
				const double Started = FPlatformTime::Seconds();
				const bool bSuccess = InBuild(Value, Error);
				if (bSuccess || (!Error.StartsWith(TEXT("DependencyNotReady")) && Error != TEXT("Canceled")))
					RecordPlanBuild(EVoxelGenerationPlanKind::Ecology, (FPlatformTime::Seconds() - Started) * 1000.0, !bSuccess);
				return bSuccess;
			},
		OutPlan,
		OutError,
		InCancel,
		bAllowGameThreadBuilds,
		bAllowWorkerWait,
		[this](const uint64 InWait)
		{
			RecordGateWait(InWait);
		}, [this](const bool bHit) { RecordPlanLookup(EVoxelGenerationPlanKind::Ecology, bHit); });
}

void FVoxelGenerationPlanCache::StoreFeature(
	const FVoxelGenerationTileKey& InKey,
	FVoxelFeaturePlanPtr InPlan)
{
	FPlanShard& Shard =
		*PlanShards[
			PlanShardIndex(InKey)];

	FWriteScopeLock Scope(
		Shard.Lock);

	const bool bNew =
		!Shard.Features.Contains(
			InKey);

	Shard.Features.Add(
		InKey,
		MoveTemp(InPlan));

	if (bNew)
	{
		Shard.FeatureKeys.Add(
			InKey);
	}
}

bool FVoxelGenerationPlanCache::GetOrBuildFeature(
	const FVoxelGenerationTileKey& InKey,
	TFunctionRef<bool(
		FVoxelFeaturePlan&,
		FString&)> InBuild,
	FVoxelFeaturePlanPtr& OutPlan,
	FString& OutError,
	const TAtomic<bool>* InCancel)
{
	FPlanShard& Shard =
		*PlanShards[
			PlanShardIndex(InKey)];

	return GetOrBuildValue<
		FVoxelGenerationTileKey,
		FVoxelFeaturePlan,
		FBuildGate>(
			Shard.Lock,
			Shard.Features,
			Shard.FeatureBuilds,
			Shard.FeatureKeys,
			InKey,
			[this, &InBuild](FVoxelFeaturePlan& Value, FString& Error)
			{
				const double Started = FPlatformTime::Seconds();
				const bool bSuccess = InBuild(Value, Error);
				if (bSuccess || (!Error.StartsWith(TEXT("DependencyNotReady")) && Error != TEXT("Canceled")))
					RecordPlanBuild(EVoxelGenerationPlanKind::Feature, (FPlatformTime::Seconds() - Started) * 1000.0, !bSuccess);
				return bSuccess;
			},
			OutPlan,
			OutError,
			InCancel,
			bAllowGameThreadBuilds,
			bAllowWorkerWait,
			[this](const uint64 InWait)
			{
				RecordGateWait(InWait);
			}, [this](const bool bHit) { RecordPlanLookup(EVoxelGenerationPlanKind::Feature, bHit); });
}

bool FVoxelGenerationPlanCache::FindStructure(
	const FVoxelGenerationTileKey& InKey,
	FVoxelStructurePlanPtr& OutPlan) const
{
	const FPlanShard& Shard =
		*PlanShards[
			PlanShardIndex(InKey)];

	FReadScopeLock Scope(
		Shard.Lock);

	if (const FVoxelStructurePlanPtr* Found =
		Shard.Structures.Find(InKey))
	{
		OutPlan = *Found;
		RecordPlanLookup(EVoxelGenerationPlanKind::Structure, OutPlan.IsValid());
		return OutPlan.IsValid();
	}

	RecordPlanLookup(EVoxelGenerationPlanKind::Structure, false);
	return false;
}

void FVoxelGenerationPlanCache::StoreStructure(
	const FVoxelGenerationTileKey& InKey,
	FVoxelStructurePlanPtr InPlan)
{
	FPlanShard& Shard =
		*PlanShards[
			PlanShardIndex(InKey)];

	FWriteScopeLock Scope(
		Shard.Lock);

	const bool bNew =
		!Shard.Structures.Contains(
			InKey);

	Shard.Structures.Add(
		InKey,
		MoveTemp(InPlan));

	if (bNew)
	{
		Shard.StructureKeys.Add(
			InKey);
	}
}

bool FVoxelGenerationPlanCache::GetOrBuildStructure(
	const FVoxelGenerationTileKey& InKey,
	TFunctionRef<bool(
		FVoxelStructurePlan&,
		FString&)> InBuild,
	FVoxelStructurePlanPtr& OutPlan,
	FString& OutError,
	const TAtomic<bool>* InCancel)
{
	FPlanShard& Shard =
		*PlanShards[
			PlanShardIndex(InKey)];

	return GetOrBuildValue<
		FVoxelGenerationTileKey,
		FVoxelStructurePlan,
		FBuildGate>(
			Shard.Lock,
			Shard.Structures,
			Shard.StructureBuilds,
			Shard.StructureKeys,
			InKey,
			[this, &InBuild](FVoxelStructurePlan& Value, FString& Error)
			{
				const double Started = FPlatformTime::Seconds();
				const bool bSuccess = InBuild(Value, Error);
				if (bSuccess || (!Error.StartsWith(TEXT("DependencyNotReady")) && Error != TEXT("Canceled")))
					RecordPlanBuild(EVoxelGenerationPlanKind::Structure, (FPlatformTime::Seconds() - Started) * 1000.0, !bSuccess);
				return bSuccess;
			},
			OutPlan,
			OutError,
			InCancel,
			bAllowGameThreadBuilds,
			bAllowWorkerWait,
			[this](const uint64 InWait)
			{
				RecordGateWait(InWait);
			}, [this](const bool bHit) { RecordPlanLookup(EVoxelGenerationPlanKind::Structure, bHit); });
}

void FVoxelGenerationPlanCache::UpdateRetention(
	const FVoxelGenerationCacheRetention& InRetention)
{
	FWriteScopeLock Scope(
		RetentionLock);

	Retention =
		InRetention;
}

FVoxelGenerationCacheRetentionLease::FVoxelGenerationCacheRetentionLease(
	TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache,
	const uint64 InId)
	: Cache(InCache)
	, Id(InId)
{
}

FVoxelGenerationCacheRetentionLease::~FVoxelGenerationCacheRetentionLease()
{
	Cache->ReleaseTaskRetention(Id);
}

TSharedRef<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe>
FVoxelGenerationPlanCache::RetainForTask(
	const FVoxelGenerationCacheRetentionPoint& InPoint)
{
	uint64 Id = 0;
	{
		FWriteScopeLock Scope(RetentionLock);
		Id = NextTaskRetentionId++;
		TaskRetentionPoints.Add(Id, InPoint);
	}
	return MakeShared<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe>(
		AsShared(), Id);
}

void FVoxelGenerationPlanCache::ReleaseTaskRetention(const uint64 InId)
{
	FWriteScopeLock Scope(RetentionLock);
	TaskRetentionPoints.Remove(InId);
}

bool FVoxelGenerationPlanCache::IsRetained(
	const FIntPoint& InPosition,
	int32 FVoxelGenerationCacheRetentionPoint::* InRadius,
	const FVoxelGenerationCacheRetention& InRetention) const
{
	for (const FVoxelGenerationCacheRetentionPoint& Point : InRetention.Points)
	{
		if (FMath::Abs(static_cast<int64>(InPosition.X) - Point.Center.X) <= Point.*InRadius &&
			FMath::Abs(static_cast<int64>(InPosition.Y) - Point.Center.Y) <= Point.*InRadius)
		{
			return true;
		}
	}
	return false;
}
void FVoxelGenerationPlanCache::TickMaintenance(
	const int32 InMaxEntries)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(
		Voxel_GenerationCacheMaintenance);

	int32 RemainingBudget =
		FMath::Max(
			0,
			InMaxEntries);

	if (RemainingBudget <= 0)
	{
		return;
	}

	int32 RemainingGroups = 7;
	auto TakeGroupBudget =
		[&RemainingBudget, &RemainingGroups]()
		{
			const int32 GroupBudget =
				RemainingGroups > 0
					? FMath::DivideAndRoundUp(
						RemainingBudget,
						RemainingGroups)
					: 0;

			RemainingBudget -= GroupBudget;
			--RemainingGroups;
			return GroupBudget;
		};

	int32 HydrologyBudget = TakeGroupBudget();
	int32 BaseColumnBudget = TakeGroupBudget();
	int32 NaturalColumnBudget = TakeGroupBudget();
	int32 CaveBudget = TakeGroupBudget();
	int32 EcologyBudget = TakeGroupBudget();
	int32 FeatureBudget = TakeGroupBudget();
	int32 StructureBudget = TakeGroupBudget();
	TArray<FVoxelBaseColumnEntryPtr> RetiredBaseColumns;
	TArray<FVoxelNaturalColumnEntryPtr> RetiredNaturalColumns;
	TArray<FVoxelHydrologyPlanPtr> RetiredHydrology;
	TArray<FVoxelCavePlanPtr> RetiredCaves;
	TArray<FVoxelEcologyPlanPtr> RetiredEcology;
	TArray<FVoxelFeaturePlanPtr> RetiredFeatures;
	TArray<FVoxelStructurePlanPtr> RetiredStructures;

	// Hold the read lock through trimming so a newly registered task cannot
	// race with a maintenance snapshot and lose its plans after registration.
	TOptional<FReadScopeLock> RetentionScope;
	RetentionScope.Emplace(RetentionLock);
	FVoxelGenerationCacheRetention Snapshot = Retention;
	for (const TPair<uint64, FVoxelGenerationCacheRetentionPoint>& Entry : TaskRetentionPoints)
	{
		Snapshot.Points.Add(Entry.Value);
	}

	if (Snapshot.Revision == 0)
	{
		return;
	}

	const int32 ShardIndex =
		MaintenanceShardCursor %
		ShardCount;

	MaintenanceShardCursor =
		(MaintenanceShardCursor + 1) %
		ShardCount;

	{
		FNaturalShard& Shard =
			*NaturalShards[
				ShardIndex];

		FWriteScopeLock Scope(
			Shard.Lock);

		TrimMapBudgeted(
			Shard.BaseColumns,
			Shard.BaseColumnKeys,
			Shard.BaseColumnCursor,
			BaseColumnBudget,
			RetiredBaseColumns,
			[this, &Snapshot](
				const FIntPoint& InKey)
			{
				return IsRetained(
					InKey,
					&FVoxelGenerationCacheRetentionPoint::NaturalRadiusCells,
					Snapshot);
			});

		TrimMapBudgeted(
			Shard.NaturalColumns,
			Shard.NaturalColumnKeys,
			Shard.NaturalColumnCursor,
			NaturalColumnBudget,
			RetiredNaturalColumns,
			[this, &Snapshot](
				const FIntPoint& InKey)
			{
				return IsRetained(
					InKey,
					&FVoxelGenerationCacheRetentionPoint::NaturalRadiusCells,
					Snapshot);
			});

	}

	{
		FPlanShard& Shard =
			*PlanShards[
				ShardIndex];

		FWriteScopeLock Scope(
			Shard.Lock);

		TrimMapBudgeted(
			Shard.Hydrology,
			Shard.HydrologyKeys,
			Shard.HydrologyCursor,
			HydrologyBudget,
			RetiredHydrology,
			[this, &Snapshot](
				const FVoxelHydrologyRegionKey& InKey)
			{
				const int32 RegionCellSide =
					static_cast<int32>(FMath::Clamp<int64>(
						static_cast<int64>(FMath::Max(1, Snapshot.HydrologyRegionSide)) *
							FMath::Max(1, Snapshot.HydrologyCellSize),
						1,
						MAX_int32));

				const FIntPoint RegionCenter =
					InKey.Coordinate * RegionCellSide +
					FIntPoint(RegionCellSide / 2);

				return IsRetained(
					RegionCenter,
					&FVoxelGenerationCacheRetentionPoint::HydrologyRadiusCells,
					Snapshot);
			});

		TrimMapBudgeted(
			Shard.Caves,
			Shard.CaveKeys,
			Shard.CaveCursor,
			CaveBudget,
			RetiredCaves,
			[this, &Snapshot](
				const FVoxelGenerationTileKey& InKey)
			{
				const FIntVector Origin =
					InKey.Coordinate *
					GenerationPlanTileSide;

				return IsRetained(
					FIntPoint(
						Origin.X,
						Origin.Y),
					&FVoxelGenerationCacheRetentionPoint::PlanRadiusCells,
					Snapshot);
			});

		TrimMapBudgeted(
			Shard.Ecology,
			Shard.EcologyKeys,
			Shard.EcologyCursor,
			EcologyBudget,
			RetiredEcology,
			[this, &Snapshot](const FVoxelEcologyTileKey& InKey)
			{
				const FIntPoint Center = InKey.Coordinate * 64 + FIntPoint(32, 32);
				return IsRetained(
					Center,
					&FVoxelGenerationCacheRetentionPoint::PlanRadiusCells,
					Snapshot);
			});

		TrimMapBudgeted(
			Shard.Features,
			Shard.FeatureKeys,
			Shard.FeatureCursor,
			FeatureBudget,
			RetiredFeatures,
			[this, &Snapshot](
				const FVoxelGenerationTileKey& InKey)
			{
				const FIntVector Origin =
					InKey.Coordinate *
					GenerationPlanTileSide;

				return IsRetained(
					FIntPoint(
						Origin.X,
						Origin.Y),
					&FVoxelGenerationCacheRetentionPoint::PlanRadiusCells,
					Snapshot);
			});

		TrimMapBudgeted(
			Shard.Structures,
			Shard.StructureKeys,
			Shard.StructureCursor,
			StructureBudget,
			RetiredStructures,
			[this, &Snapshot](
				const FVoxelGenerationTileKey& InKey)
			{
				const FIntVector Origin =
					InKey.Coordinate *
					GenerationPlanTileSide;

				return IsRetained(
					FIntPoint(
						Origin.X,
						Origin.Y),
					&FVoxelGenerationCacheRetentionPoint::PlanRadiusCells,
					Snapshot);
			});

	}
	RetentionScope.Reset();
}

void FVoxelGenerationPlanCache::Reset()
{
	for (int32 Index = 0;
		Index < ShardCount;
		++Index)
	{
		{
			FNaturalShard& Shard =
				*NaturalShards[Index];

			FWriteScopeLock Scope(
				Shard.Lock);

			Shard.BaseColumns.Reset();
			Shard.NaturalColumns.Reset();

			Shard.BaseColumnBuilds.Reset();
			Shard.NaturalColumnBuilds.Reset();

			Shard.BaseColumnKeys.Reset();
			Shard.NaturalColumnKeys.Reset();

			Shard.BaseColumnCursor = 0;
			Shard.NaturalColumnCursor = 0;
		}

		{
			FPlanShard& Shard =
				*PlanShards[Index];

			FWriteScopeLock Scope(
				Shard.Lock);

			Shard.Hydrology.Reset();
			Shard.Caves.Reset();
			Shard.Ecology.Reset();
			Shard.Features.Reset();
			Shard.Structures.Reset();

			Shard.HydrologyBuilds.Reset();
			Shard.CaveBuilds.Reset();
			Shard.EcologyBuilds.Reset();
			Shard.FeatureBuilds.Reset();
			Shard.StructureBuilds.Reset();

			Shard.HydrologyKeys.Reset();
			Shard.CaveKeys.Reset();
			Shard.EcologyKeys.Reset();
			Shard.FeatureKeys.Reset();
			Shard.StructureKeys.Reset();

			Shard.HydrologyCursor = 0;
			Shard.CaveCursor = 0;
			Shard.EcologyCursor = 0;
			Shard.FeatureCursor = 0;
			Shard.StructureCursor = 0;
		}
	}

	MaintenanceShardCursor = 0;
}

FVoxelGenerationCacheStats
FVoxelGenerationPlanCache::GetStats() const
{
	FVoxelGenerationCacheStats Stats;

	for (int32 Index = 0;
		Index < ShardCount;
		++Index)
	{
		{
			const FNaturalShard& Shard =
				*NaturalShards[Index];

			FReadScopeLock Scope(
				Shard.Lock);

			Stats.BaseColumns +=
				Shard.BaseColumns.Num();

			Stats.NaturalColumns +=
				Shard.NaturalColumns.Num();

		}

		{
			const FPlanShard& Shard =
				*PlanShards[Index];

			FReadScopeLock Scope(
				Shard.Lock);

			Stats.Hydrology +=
				Shard.Hydrology.Num();

			Stats.Caves +=
				Shard.Caves.Num();

			Stats.Ecology +=
				Shard.Ecology.Num();

			Stats.Features +=
				Shard.Features.Num();

			Stats.Structures +=
				Shard.Structures.Num();
		}
	}

	Stats.GateWaitCount =
		GateWaitCount.Load();

	Stats.GateWaitMicroseconds =
		GateWaitMicroseconds.Load();

	Stats.AllocatedBytes =
		GetAllocatedBytes();

	return Stats;
}

uint64 FVoxelGenerationPlanCache::GetAllocatedBytes() const
{
	uint64 Bytes = 0;

	for (int32 Index = 0;
		Index < ShardCount;
		++Index)
	{
		{
			const FNaturalShard& Shard =
				*NaturalShards[Index];

			FReadScopeLock Scope(
				Shard.Lock);

			Bytes +=
				Shard.BaseColumns.GetAllocatedSize() +
				Shard.NaturalColumns.GetAllocatedSize() +
				Shard.BaseColumnKeys.GetAllocatedSize() +
				Shard.NaturalColumnKeys.GetAllocatedSize();

			for (const TPair<FIntPoint, FVoxelBaseColumnEntryPtr>& Pair :
				Shard.BaseColumns)
			{
				if (Pair.Value)
				{
					Bytes +=
						Pair.Value->
							GetAllocatedBytes();
				}
			}

			for (const TPair<FIntPoint, FVoxelNaturalColumnEntryPtr>& Pair :
				Shard.NaturalColumns)
			{
				if (Pair.Value)
				{
					Bytes +=
						Pair.Value->
							GetAllocatedBytes();
				}
			}
		}

		{
			const FPlanShard& Shard =
				*PlanShards[Index];

			FReadScopeLock Scope(
				Shard.Lock);

			Bytes +=
				Shard.Hydrology.GetAllocatedSize() +
				Shard.Caves.GetAllocatedSize() +
				Shard.Ecology.GetAllocatedSize() +
				Shard.Features.GetAllocatedSize() +
				Shard.Structures.GetAllocatedSize() +
				Shard.HydrologyKeys.GetAllocatedSize() +
				Shard.CaveKeys.GetAllocatedSize() +
				Shard.EcologyKeys.GetAllocatedSize() +
				Shard.FeatureKeys.GetAllocatedSize() +
				Shard.StructureKeys.GetAllocatedSize();

			for (const TPair<FVoxelHydrologyRegionKey, FVoxelHydrologyPlanPtr>& Pair :
				Shard.Hydrology)
			{
				if (Pair.Value)
				{
					Bytes +=
						Pair.Value->
							GetAllocatedBytes();
				}
			}

			for (const TPair<FVoxelGenerationTileKey, FVoxelCavePlanPtr>& Pair :
				Shard.Caves)
			{
				if (Pair.Value)
				{
					Bytes +=
						Pair.Value->
							GetAllocatedBytes();
				}
			}

			for (const TPair<FVoxelGenerationTileKey, FVoxelFeaturePlanPtr>& Pair :
				Shard.Features)
			{
				if (Pair.Value)
				{
					Bytes +=
						Pair.Value->
							GetAllocatedBytes();
				}
			}

			for (const TPair<FVoxelEcologyTileKey, FVoxelEcologyPlanPtr>& Pair :
				Shard.Ecology)
			{
				if (Pair.Value)
				{
					Bytes += Pair.Value->GetAllocatedBytes();
				}
			}

			for (const TPair<FVoxelGenerationTileKey, FVoxelStructurePlanPtr>& Pair :
				Shard.Structures)
			{
				if (Pair.Value)
				{
					Bytes +=
						Pair.Value->
							GetAllocatedBytes();
				}
			}
		}
	}

	return Bytes;
}
