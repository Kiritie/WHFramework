#include "Voxel/Generation/VoxelGenerationPlanCache.h"

#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Task/VoxelTerrainResources.h"

namespace
{
	constexpr int32 GenerationPlanTileSide = 256;
	constexpr int32 LakeAnchorSide = 64;

	bool TryTileRetentionPosition(
		const FIntVector& InCoordinate,
		const int32 InSide,
		const bool bInUseCenter,
		FInt64Vector2& OutPosition)
	{
		const FInt64Vector Origin(
			static_cast<int64>(InCoordinate.X) * InSide,
			static_cast<int64>(InCoordinate.Y) * InSide,
			static_cast<int64>(InCoordinate.Z) * InSide);
		FVoxelGenerationBounds Bounds;
		if (!VoxelGeneration::ClipWorldBounds(
			Origin,
			FInt64Vector(Origin.X + InSide, Origin.Y + InSide, Origin.Z + InSide),
			Bounds))
		{
			return false;
		}

		// 完整 lattice 的元数据代表点允许域外；交集只验 key，不改变其中心或身份。
		OutPosition = FInt64Vector2(
			Origin.X + (bInUseCenter ? InSide / 2 : 0),
			Origin.Y + (bInUseCenter ? InSide / 2 : 0));
		return true;
	}

	bool TryHydrologyRetentionPosition(
		const FVoxelHydrologyRegionKey& InKey,
		const int32 InRegionSide,
		const int32 InCellSize,
		FInt64Vector2& OutPosition)
	{
		const int64 RegionSide = FMath::Max(1, InRegionSide);
		const int32 CellSize = FMath::Max(1, InCellSize);
		const int64 CellMin = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMin, CellSize);
		const int64 CellMax = VoxelGeneration::FloorDivideWide(
			VoxelGeneration::WorldCellMaxExclusive - 1, CellSize) + 1;
		const auto ClipAxis = [RegionSide, CellSize, CellMin, CellMax](
			const int32 InCoordinate, int64& OutCenter)
		{
			// 两因子乘积在 int64 内；先验 logical 交集再乘 CellSize，非法 key 不计算第三因子。
			const int64 Origin = static_cast<int64>(InCoordinate) * RegionSide;
			const int64 MinCell = FMath::Max(Origin, CellMin);
			const int64 MaxCell = FMath::Min(Origin + RegionSide, CellMax);
			if (MinCell >= MaxCell)
			{
				return false;
			}
			// 保留原完整 region 中心语义；此元数据不用于采样或转换成 world cell。
			OutCenter = Origin * CellSize + (RegionSide * CellSize) / 2;
			return true;
		};

		FInt64Vector2 Center;
		if (!ClipAxis(InKey.Coordinate.X, Center.X) || !ClipAxis(InKey.Coordinate.Y, Center.Y))
		{
			return false;
		}
		OutPosition = Center;
		return true;
	}

	template<typename KeyType, typename ValueType>
	using TConstCachePtr =
		TSharedPtr<
			const ValueType,
			ESPMode::ThreadSafe>;

	// 调用方持有原 shard 写锁；每个门闩的正文与错误只能完成一次。
	template<typename BuildGateType>
	bool CompleteBuildGate(
		BuildGateType& InGate,
		const TSharedPtr<const void, ESPMode::ThreadSafe>& InResult,
		const FString& InError)
	{
		if (InGate.bCompleted)
		{
			return false;
		}

		InGate.Result = InResult;
		InGate.bSuccess = InResult.IsValid();
		InGate.Error = InGate.bSuccess ? FString() : InError;
		InGate.bCompleted = true;
		return true;
	}

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
		TConstCachePtr<KeyType, ValueType> ReadyValue;
		{
			FReadScopeLock Scope(InLock);

			if (const TConstCachePtr<KeyType, ValueType>* Found =
				InValues.Find(InKey))
			{
				ReadyValue = *Found;
			}
		}
		if (ReadyValue)
		{
			OutValue = MoveTemp(ReadyValue);
			OutError.Reset();
			if (InRecordLookup) InRecordLookup(true);
			return true;
		}
		if (InRecordLookup) InRecordLookup(false);

		// 运行时主线程只读取已发布值，冷查询不得创建构建门闩或执行构建器。
		if (!bInAllowGameThreadBuilds && IsInGameThread())
		{
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
				ReadyValue = *Found;
			}
			else if (TSharedPtr<BuildGateType, ESPMode::ThreadSafe>* Existing =
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
		if (ReadyValue)
		{
			OutValue = MoveTemp(ReadyValue);
			OutError.Reset();
			return true;
		}
		if (!Gate)
		{
			OutError = TEXT("Voxel generation cache contains an invalid published value");
			return false;
		}

		if (!bOwner)
		{
			if (!bInAllowWorkerWait || IsInGameThread())
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

			{
				FReadScopeLock Scope(InLock);
				if (InCancel && InCancel->Load())
				{
					OutError = TEXT("Canceled");
					return false;
				}
				if (!Gate->bCompleted || !Gate->bSuccess)
				{
					OutError = Gate->Error;
					return false;
				}
				// 读取原门闩持有的正文，Reset 后的新同 key 不属于本次等待。
				ReadyValue = StaticCastSharedPtr<const ValueType>(Gate->Result);
			}
			if (ReadyValue)
			{
				OutValue = MoveTemp(ReadyValue);
				OutError.Reset();
				return true;
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

		if (bSuccess && (!InCancel || !InCancel->Load()))
		{
			BuiltValue =
				MakeShared<
					const ValueType,
					ESPMode::ThreadSafe>(
						MoveTemp(
							LocalValue));
		}

		bool bTrigger = false;
		FString ResultError;
		{
			FWriteScopeLock Scope(
				InLock);

			// builder 返回 true 仍可能已经撤销；发布前在原锁内重新验收。
			const bool bCanceled = InCancel && InCancel->Load();
			const TSharedPtr<BuildGateType, ESPMode::ThreadSafe>* Current =
				InBuilds.Find(InKey);
			if (Current && *Current == Gate && !Gate->bCompleted)
			{
				if (bCanceled)
				{
					BuildError = TEXT("Canceled");
				}
				else if (const TConstCachePtr<KeyType, ValueType>* Published = InValues.Find(InKey))
				{
					// 同 key 的已发布 immutable 优先，旧 owner 不得覆盖。
					ReadyValue = *Published;
				}
				else if (BuiltValue)
				{
					InValues.Add(InKey, BuiltValue);
					InKeys.Add(InKey);
					ReadyValue = BuiltValue;
				}

				if (!ReadyValue && BuildError.IsEmpty())
				{
					BuildError = TEXT("Voxel generation cache build failed");
				}
				bTrigger = CompleteBuildGate(*Gate, ReadyValue, BuildError);
				InBuilds.Remove(InKey);
			}

			if (bCanceled)
			{
				ReadyValue.Reset();
				ResultError = TEXT("Canceled");
			}
			else if (Gate->bCompleted)
			{
				ReadyValue = StaticCastSharedPtr<const ValueType>(Gate->Result);
				ResultError = Gate->Error;
			}
			else
			{
				ResultError = TEXT("DependencyNotReady: voxel generation cache build gate was replaced");
			}
		}

		// 只有完成原门闩的一方唤醒；旧 owner 不再修改或触发 Reset/Store 的结果。
		if (bTrigger)
		{
			Gate->Event->Trigger();
		}

		if (!ReadyValue)
		{
			OutError = MoveTemp(ResultError);
			return false;
		}

		OutValue =
			MoveTemp(
				ReadyValue);

		OutError.Reset();
		return true;
	}

	template<typename KeyType, typename ValueType, typename BuildGateType>
	void StoreValue(
		FRWLock& InLock,
		TMap<KeyType, TConstCachePtr<KeyType, ValueType>>& InValues,
		TMap<KeyType, TSharedPtr<BuildGateType, ESPMode::ThreadSafe>>& InBuilds,
		TArray<KeyType>& InKeys,
		const KeyType& InKey,
		TConstCachePtr<KeyType, ValueType> InValue)
	{
		if (!InValue)
		{
			return;
		}

		TSharedPtr<BuildGateType, ESPMode::ThreadSafe> Gate;
		bool bTrigger = false;
		{
			FWriteScopeLock Scope(InLock);
			if (const TConstCachePtr<KeyType, ValueType>* Published = InValues.Find(InKey))
			{
				// 已发布的同 key 正文不更换，传入引用也在锁外退出。
				bTrigger = false;
				if (const TSharedPtr<BuildGateType, ESPMode::ThreadSafe>* Existing = InBuilds.Find(InKey))
				{
					Gate = *Existing;
					bTrigger = CompleteBuildGate(*Gate, *Published, FString());
					InBuilds.Remove(InKey);
				}
			}
			else
			{
				InValues.Add(InKey, InValue);
				InKeys.Add(InKey);
				if (const TSharedPtr<BuildGateType, ESPMode::ThreadSafe>* Existing = InBuilds.Find(InKey))
				{
					Gate = *Existing;
					bTrigger = CompleteBuildGate(*Gate, InValue, FString());
					InBuilds.Remove(InKey);
				}
			}
		}
		if (bTrigger)
		{
			Gate->Event->Trigger();
		}
	}

	template<typename KeyType, typename BuildGateType>
	void CancelBuildGates(TMap<KeyType, TSharedPtr<BuildGateType, ESPMode::ThreadSafe>>& InBuilds)
	{
		for (auto& Pair : InBuilds)
		{
			if (CompleteBuildGate(*Pair.Value, nullptr, TEXT("DependencyNotReady: voxel generation cache reset during build")))
			{
				// Trigger 只唤醒，不等待线程；仍持原锁保证随后 detach 不漏终结。
				Pair.Value->Event->Trigger();
			}
		}
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
	// Result/Error/bSuccess/bCompleted 仅在所属 shard 锁下读写。
	TSharedPtr<const void, ESPMode::ThreadSafe> Result;
	bool bCompleted = false;
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

	StoreValue(Shard.Lock, Shard.Hydrology, Shard.HydrologyBuilds,
		Shard.HydrologyKeys, InKey, MoveTemp(InPlan));
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

	StoreValue(Shard.Lock, Shard.Caves, Shard.CaveBuilds,
		Shard.CaveKeys, InKey, MoveTemp(InPlan));
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

	StoreValue(Shard.Lock, Shard.Features, Shard.FeatureBuilds,
		Shard.FeatureKeys, InKey, MoveTemp(InPlan));
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

	StoreValue(Shard.Lock, Shard.Structures, Shard.StructureBuilds,
		Shard.StructureKeys, InKey, MoveTemp(InPlan));
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
	TSharedRef<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InResourceLease,
	const FVoxelGenerationCacheRetentionPoint& InPoint,
	const uint64 InId)
	: ResourceLease(MoveTemp(InResourceLease))
	, Cache(MoveTemp(InCache))
	, Point(InPoint)
	, Id(InId)
{
}

FVoxelGenerationCacheRetentionLease::~FVoxelGenerationCacheRetentionLease()
{
	Cache->ReleaseTaskRetention(*this);
}

void FVoxelGenerationCacheRetentionLease::FDeleter::operator()(FVoxelGenerationCacheRetentionLease* InLease) const
{
	delete InLease;
}

uint64 FVoxelGenerationCacheRetentionLease::GetRequiredAllocationBytes()
{
	using FController = SharedPointerInternals::TReferenceControllerWithDeleter<
		FVoxelGenerationCacheRetentionLease, FDeleter, ESPMode::ThreadSafe>;
	return FMemory::QuantizeSize(sizeof(FVoxelGenerationCacheRetentionLease), alignof(FVoxelGenerationCacheRetentionLease)) +
		FMemory::QuantizeSize(sizeof(FController), alignof(FController));
}

bool FVoxelGenerationPlanCache::RetainForTask(
	const FVoxelGenerationCacheRetentionPoint& InPoint,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer,
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe>& OutLease,
	FString& OutError)
{
	if (!InConsumer.IsValid())
	{
		OutError = TEXT("InvalidConsumer: voxel cache retention requires a complete consumer key");
		return false;
	}

	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> NewLease;
	{
		FWriteScopeLock Scope(RetentionLock);
		if (NextTaskRetentionId == MAX_uint64)
		{
			OutError = TEXT("CapacityUnachievable: voxel cache task retention identity exhausted");
			return false;
		}

		if (!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input,
			FVoxelGenerationCacheRetentionLease::GetRequiredAllocationBytes(), ResourceLease))
		{
			OutError = TEXT("CapacityBlocked: voxel cache task retention input capacity");
			return false;
		}

		const uint64 Id = NextTaskRetentionId++;
		// deleter 随控制器存活；对象析构不能让尚有 weak 引用的控制器提前退容量。
		NewLease = TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe>(
			new FVoxelGenerationCacheRetentionLease(AsShared(), ResourceLease.ToSharedRef(), InPoint, Id),
			FVoxelGenerationCacheRetentionLease::FDeleter{ResourceLease.ToSharedRef()});
		NewLease->Previous = TaskRetentionTail;
		if (TaskRetentionTail)
		{
			TaskRetentionTail->Next = NewLease.Get();
		}
		else
		{
			TaskRetentionHead = NewLease.Get();
		}
		TaskRetentionTail = NewLease.Get();
	}

	OutLease = MoveTemp(NewLease);
	OutError.Reset();
	return true;
}

void FVoxelGenerationPlanCache::ReleaseTaskRetention(FVoxelGenerationCacheRetentionLease& InLease)
{
	FWriteScopeLock Scope(RetentionLock);
	if (InLease.Previous)
	{
		InLease.Previous->Next = InLease.Next;
	}
	else
	{
		TaskRetentionHead = InLease.Next;
	}
	if (InLease.Next)
	{
		InLease.Next->Previous = InLease.Previous;
	}
	else
	{
		TaskRetentionTail = InLease.Previous;
	}
	InLease.Previous = nullptr;
	InLease.Next = nullptr;
	InLease.Id = 0;
}

bool FVoxelGenerationPlanCache::IsRetained(
	const FInt64Vector2& InPosition,
	int32 FVoxelGenerationCacheRetentionPoint::* InRadius) const
{
	const auto ContainsPoint = [&InPosition, InRadius](const FVoxelGenerationCacheRetentionPoint& InPoint)
	{
		return FMath::Abs(InPosition.X - InPoint.Center.X) <= InPoint.*InRadius &&
			FMath::Abs(InPosition.Y - InPoint.Center.Y) <= InPoint.*InRadius;
	};
	for (const FVoxelGenerationCacheRetentionPoint& Point : Retention.Points)
	{
		if (ContainsPoint(Point))
		{
			return true;
		}
	}
	for (const FVoxelGenerationCacheRetentionLease* Lease = TaskRetentionHead; Lease; Lease = Lease->Next)
	{
		if (ContainsPoint(Lease->Point))
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

	// 保留锁覆盖逐项裁剪；任务点直接读取原节点，接纳与析构不能越过当前检查。
	TOptional<FReadScopeLock> RetentionScope;
	RetentionScope.Emplace(RetentionLock);

	if (Retention.Revision == 0)
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
			[this](
				const FIntPoint& InKey)
			{
				return VoxelGeneration::IsWorldCell(InKey.X) && VoxelGeneration::IsWorldCell(InKey.Y) && IsRetained(
					FInt64Vector2(InKey.X, InKey.Y),
					&FVoxelGenerationCacheRetentionPoint::NaturalRadiusCells);
			});

		TrimMapBudgeted(
			Shard.NaturalColumns,
			Shard.NaturalColumnKeys,
			Shard.NaturalColumnCursor,
			NaturalColumnBudget,
			RetiredNaturalColumns,
			[this](
				const FIntPoint& InKey)
			{
				return VoxelGeneration::IsWorldCell(InKey.X) && VoxelGeneration::IsWorldCell(InKey.Y) && IsRetained(
					FInt64Vector2(InKey.X, InKey.Y),
					&FVoxelGenerationCacheRetentionPoint::NaturalRadiusCells);
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
			[this](
				const FVoxelHydrologyRegionKey& InKey)
			{
				FInt64Vector2 RegionCenter;
				return TryHydrologyRetentionPosition(InKey, Retention.HydrologyRegionSide,
					Retention.HydrologyCellSize, RegionCenter) && IsRetained(
					RegionCenter,
					&FVoxelGenerationCacheRetentionPoint::HydrologyRadiusCells);
			});

		TrimMapBudgeted(
			Shard.Caves,
			Shard.CaveKeys,
			Shard.CaveCursor,
			CaveBudget,
			RetiredCaves,
			[this](
				const FVoxelGenerationTileKey& InKey)
			{
				FInt64Vector2 Origin;
				return TryTileRetentionPosition(InKey.Coordinate, GenerationPlanTileSide, false, Origin) && IsRetained(
					Origin,
					&FVoxelGenerationCacheRetentionPoint::PlanRadiusCells);
			});

		TrimMapBudgeted(
			Shard.Ecology,
			Shard.EcologyKeys,
			Shard.EcologyCursor,
			EcologyBudget,
			RetiredEcology,
			[this](const FVoxelEcologyTileKey& InKey)
			{
				FInt64Vector2 Center;
				return TryTileRetentionPosition(FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0),
					64, true, Center) && IsRetained(
					Center,
					&FVoxelGenerationCacheRetentionPoint::PlanRadiusCells);
			});

		TrimMapBudgeted(
			Shard.Features,
			Shard.FeatureKeys,
			Shard.FeatureCursor,
			FeatureBudget,
			RetiredFeatures,
			[this](
				const FVoxelGenerationTileKey& InKey)
			{
				FInt64Vector2 Origin;
				return TryTileRetentionPosition(InKey.Coordinate, GenerationPlanTileSide, false, Origin) && IsRetained(
					Origin,
					&FVoxelGenerationCacheRetentionPoint::PlanRadiusCells);
			});

		TrimMapBudgeted(
			Shard.Structures,
			Shard.StructureKeys,
			Shard.StructureCursor,
			StructureBudget,
			RetiredStructures,
			[this](
				const FVoxelGenerationTileKey& InKey)
			{
				FInt64Vector2 Origin;
				return TryTileRetentionPosition(InKey.Coordinate, GenerationPlanTileSide, false, Origin) && IsRetained(
					Origin,
					&FVoxelGenerationCacheRetentionPoint::PlanRadiusCells);
			});

	}
	RetentionScope.Reset();
}

void FVoxelGenerationPlanCache::Reset()
{
	{
		FWriteScopeLock RetentionScope(RetentionLock);
		MaintenanceShardCursor = 0;
	}

	// 每个 shard 采用 Retention -> shard 顺序；原正文及 backing 在两把锁外退休。
	for (int32 Index = 0;
		Index < ShardCount;
		++Index)
	{
		{
			FNaturalShard& Shard =
				*NaturalShards[Index];

			TMap<FIntPoint, FVoxelBaseColumnEntryPtr> RetiredBaseColumns;
			TMap<FIntPoint, FVoxelNaturalColumnEntryPtr> RetiredNaturalColumns;
			{
				FWriteScopeLock RetentionScope(RetentionLock);
				FWriteScopeLock Scope(Shard.Lock);
				CancelBuildGates(Shard.BaseColumnBuilds);
				CancelBuildGates(Shard.NaturalColumnBuilds);
				Shard.BaseColumnBuilds.Reset();
				Shard.NaturalColumnBuilds.Reset();

				RetiredBaseColumns = MoveTemp(Shard.BaseColumns);
				RetiredNaturalColumns = MoveTemp(Shard.NaturalColumns);
				Shard.BaseColumnKeys.Reset();
				Shard.NaturalColumnKeys.Reset();
				Shard.BaseColumnCursor = 0;
				Shard.NaturalColumnCursor = 0;
			}
		}

		{
			FPlanShard& Shard =
				*PlanShards[Index];

			TMap<FVoxelHydrologyRegionKey, FVoxelHydrologyPlanPtr> RetiredHydrology;
			TMap<FVoxelGenerationTileKey, FVoxelCavePlanPtr> RetiredCaves;
			TMap<FVoxelEcologyTileKey, FVoxelEcologyPlanPtr> RetiredEcology;
			TMap<FVoxelGenerationTileKey, FVoxelFeaturePlanPtr> RetiredFeatures;
			TMap<FVoxelGenerationTileKey, FVoxelStructurePlanPtr> RetiredStructures;
			{
				FWriteScopeLock RetentionScope(RetentionLock);
				FWriteScopeLock Scope(Shard.Lock);
				CancelBuildGates(Shard.HydrologyBuilds);
				CancelBuildGates(Shard.CaveBuilds);
				CancelBuildGates(Shard.EcologyBuilds);
				CancelBuildGates(Shard.FeatureBuilds);
				CancelBuildGates(Shard.StructureBuilds);
				Shard.HydrologyBuilds.Reset();
				Shard.CaveBuilds.Reset();
				Shard.EcologyBuilds.Reset();
				Shard.FeatureBuilds.Reset();
				Shard.StructureBuilds.Reset();

				RetiredHydrology = MoveTemp(Shard.Hydrology);
				RetiredCaves = MoveTemp(Shard.Caves);
				RetiredEcology = MoveTemp(Shard.Ecology);
				RetiredFeatures = MoveTemp(Shard.Features);
				RetiredStructures = MoveTemp(Shard.Structures);
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
	}
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
