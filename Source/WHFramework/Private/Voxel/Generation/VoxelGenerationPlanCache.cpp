#include "Voxel/Generation/VoxelGenerationPlanCache.h"

#include "HAL/PlatformProcess.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationPlanningRead.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"

namespace
{
	constexpr int32 GenerationPlanTileSide = 256;
	constexpr int32 LakeAnchorSide = 64;
	constexpr int32 MaximumControlEntries = 1 << 20;
	constexpr int32 MaximumRetiredEntriesPerGroup = 64;

	void EnumerateCacheRequiredSources(const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
	{
		const auto& Cache = *static_cast<const FVoxelGenerationPlanCache*>(InContext);
		uint64 Bytes;
		int32 Count;
		Cache.GetRequiredInputCapacity(Bytes, Count);
		if (Count) InVisit({&Cache, 0, EVoxelTerrainResourcePool::Input, Bytes, Count});
	}

	template<typename ValueType>
	using TRetiredCacheValues = TArray<TSharedPtr<const ValueType, ESPMode::ThreadSafe>,
		TFixedAllocator<MaximumRetiredEntriesPerGroup>>;

	// 七族原生正文共用原消费者的构建额度与发布后的稳定缓存租约。
	template<typename ValueType>
	struct TCacheBodyCapacity
	{
		static constexpr bool Enabled =
			std::is_same_v<ValueType, FVoxelBaseColumnEntry> ||
			std::is_same_v<ValueType, FVoxelNaturalColumnEntry> ||
			std::is_same_v<ValueType, FVoxelHydrologyPlan> ||
			std::is_same_v<ValueType, FVoxelCavePlan> ||
			std::is_same_v<ValueType, FVoxelFeaturePlan> ||
			std::is_same_v<ValueType, FVoxelStructurePlan> ||
			std::is_same_v<ValueType, FVoxelEcologyPlan>;

		static uint64 BackingBytes(const ValueType& InValue)
		{
			if constexpr (std::is_same_v<ValueType, FVoxelBaseColumnEntry> ||
				std::is_same_v<ValueType, FVoxelNaturalColumnEntry>)
			{
				// 列正文的所有字段内嵌，GetAllocatedBytes 含 root，不能重复加入。
				return 0;
			}
			else
			{
				return InValue.GetAllocatedBytes();
			}
		}
	};

	template<typename KeyType>
	FVoxelGenerationConsumerKey CacheBodyOwner(
		const FVoxelGenerationConsumerKey& InControlConsumer, const KeyType& InKey)
	{
		auto Consumer = InControlConsumer;
		if constexpr (std::is_same_v<KeyType, FIntPoint>)
		{
			Consumer.SpatialKind = EVoxelGenerationSpatialKind::Column;
			Consumer.Min = FIntVector(InKey.X, InKey.Y, 0);
		}
		else
		{
			// Region/Tile key 是元数据身份，不转换成 world cell。
			Consumer.SpatialKind = EVoxelGenerationSpatialKind::Section;
			if constexpr (std::is_same_v<KeyType, FVoxelGenerationTileKey>)
			{
				Consumer.Min = InKey.Coordinate;
			}
			else
			{
				Consumer.Min = FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0);
			}
		}
		Consumer.Max = Consumer.Min;
		return Consumer;
	}

	bool ResolveBuildConsumer(
		const FVoxelGenerationConsumerKey& InControlConsumer,
		FVoxelGenerationConsumerKey& OutConsumer,
		FString& OutError)
	{
		OutConsumer = InControlConsumer;
		if (const auto* Stamp = FVoxelGenerationPlanningReadScope::GetTaskStamp())
		{
			if (Stamp->WorldEpoch != InControlConsumer.WorldEpoch || !Stamp->OwnerId.IsValid() ||
				Stamp->OwnerIncarnation == 0 || Stamp->Token == 0)
			{
				OutError = TEXT("InvalidConsumer: generation cache task requires the original complete World identity");
				return false;
			}
			OutConsumer = {EVoxelGenerationConsumerKind::WorldFact, Stamp->WorldEpoch, Stamp->OwnerId,
				Stamp->OwnerIncarnation, EVoxelGenerationSpatialKind::Section, Stamp->Section, Stamp->Section, Stamp->Token};
		}
		const auto* ReadConsumer = FVoxelGenerationPlanningReadScope::GetConsumerKey();
		if (ReadConsumer)
		{
			if (!ReadConsumer->IsValid() || ReadConsumer->WorldEpoch != InControlConsumer.WorldEpoch)
			{
				OutError = TEXT("InvalidConsumer: generation cache logical consumer belongs to another World");
				return false;
			}
			OutConsumer = *ReadConsumer;
		}
		if (const auto* Consumer = FVoxelGenerationConsumerScope::GetConsumerKey())
		{
			if (!Consumer->IsValid() || Consumer->WorldEpoch != InControlConsumer.WorldEpoch ||
				(ReadConsumer && !(*Consumer == *ReadConsumer)))
			{
				OutError = TEXT("InvalidConsumer: generation cache scopes require the same original World consumer");
				return false;
			}
			OutConsumer = *Consumer;
		}
		return OutConsumer.IsValid();
	}

	int32 NextControlCapacity(const int32 InRequired, const int32 InCapacity)
	{
		return FMath::Min(MaximumControlEntries,
			FMath::Max(InRequired, FMath::Max(4, InCapacity * 2)));
	}

	template<typename KeyType, typename ValueType, typename CapacityType>
	bool PrepareControlMap(
		TMap<KeyType, ValueType>& InMap,
		const int32 InRequired,
		int32& InCapacity,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InLease,
		CapacityType& InControlCapacity,
		const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
		const FVoxelGenerationConsumerKey& InControlConsumer,
		FString& OutError)
	{
		if (InRequired <= InCapacity)
		{
			return true;
		}
		const int32 Capacity = NextControlCapacity(InRequired, InCapacity);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (InResources)
		{
			FVoxelGenerationRequiredCapacity Sources;
			if (!FVoxelGenerationRequiredSourceScope::Collect(*InResources, nullptr, Sources, OutError)) return false;
			const auto Admission = InResources->TryAcquire(InControlConsumer, EVoxelTerrainResourcePool::Input,
				VoxelGenerationAllocation::Private::SetReserveBytes<TPair<KeyType, ValueType>>(InMap, Capacity), Lease,
				0, Sources.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Sources.LeaseCount);
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{
				OutError = Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable
					? TEXT("CapacityUnachievable: generation cache control map and original sources")
					: TEXT("CapacityBlocked: generation cache control map");
				return false;
			}
		}
		// 原 backing 租约仍在，完整新 backing 在 Reserve 前覆盖重分配峰值。
		InMap.Reserve(Capacity);
		if (Lease)
		{
			check(VoxelGenerationAllocation::AllocatedBytes(InMap) <= Lease->Bytes());
			const bool bResized = Lease->Resize(VoxelGenerationAllocation::AllocatedBytes(InMap));
			check(bResized);
		}
		InControlCapacity.Replace(InLease.Get(), Lease.Get());
		InLease = MoveTemp(Lease);
		InCapacity = Capacity;
		return true;
	}

	template<typename KeyType, typename CapacityType>
	bool PrepareControlKeys(
		TArray<KeyType>& InKeys,
		const int32 InRequired,
		int32& InCapacity,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InLease,
		CapacityType& InControlCapacity,
		const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
		const FVoxelGenerationConsumerKey& InControlConsumer,
		FString& OutError)
	{
		if (InRequired <= InCapacity)
		{
			return true;
		}
		const int32 Capacity = NextControlCapacity(InRequired, InCapacity);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (InResources)
		{
			FVoxelGenerationRequiredCapacity Sources;
			if (!FVoxelGenerationRequiredSourceScope::Collect(*InResources, nullptr, Sources, OutError)) return false;
			const auto Admission = InResources->TryAcquire(InControlConsumer, EVoxelTerrainResourcePool::Input,
				VoxelGenerationAllocation::Private::ArrayReserveBytes<KeyType, FDefaultAllocator>(Capacity), Lease,
				0, Sources.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Sources.LeaseCount);
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{
				OutError = Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable
					? TEXT("CapacityUnachievable: generation cache control keys and original sources")
					: TEXT("CapacityBlocked: generation cache control keys");
				return false;
			}
		}
		InKeys.Reserve(Capacity);
		if (Lease)
		{
			check(VoxelGenerationAllocation::AllocatedBytes(InKeys) <= Lease->Bytes());
			const bool bResized = Lease->Resize(VoxelGenerationAllocation::AllocatedBytes(InKeys));
			check(bResized);
		}
		InControlCapacity.Replace(InLease.Get(), Lease.Get());
		InLease = MoveTemp(Lease);
		InCapacity = Capacity;
		return true;
	}

	template<typename KeyType, typename ValueType, typename BuildGateType, typename BackingType, typename CapacityType>
	bool PrepareControlStorage(
		TMap<KeyType, TSharedPtr<const ValueType, ESPMode::ThreadSafe>>& InValues,
		TMap<KeyType, TSharedPtr<BuildGateType, ESPMode::ThreadSafe>>& InBuilds,
		TArray<KeyType>& InKeys,
		BackingType& InBacking,
		CapacityType& InControlCapacity,
		const int32 InAdditionalValues,
		const int32 InAdditionalBuilds,
		const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
		const FVoxelGenerationConsumerKey& InControlConsumer,
		FString& OutError)
	{
		const int64 ValuesRequired = static_cast<int64>(InValues.Num()) + InBuilds.Num() + InAdditionalValues;
		const int64 KeysRequired = static_cast<int64>(InKeys.Num()) + InBuilds.Num() + InAdditionalValues;
		const int64 BuildsRequired = static_cast<int64>(InBuilds.Num()) + InAdditionalBuilds;
		if (ValuesRequired > MaximumControlEntries || KeysRequired > MaximumControlEntries || BuildsRequired > MaximumControlEntries)
		{
			OutError = TEXT("Oversized: generation cache control entries");
			return false;
		}
		// 预留所有在途 gate 的发布槽，成功完成不会再增长 values 或 keys。
		return PrepareControlMap(InValues, int32(ValuesRequired), InBacking.ValuesCapacity, InBacking.ValuesLease, InControlCapacity,
			InResources, InControlConsumer, OutError) &&
			PrepareControlKeys(InKeys, int32(KeysRequired), InBacking.KeysCapacity, InBacking.KeysLease, InControlCapacity,
				InResources, InControlConsumer, OutError) &&
			PrepareControlMap(InBuilds, int32(BuildsRequired), InBacking.BuildsCapacity, InBacking.BuildsLease, InControlCapacity,
				InResources, InControlConsumer, OutError);
	}

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
		FCString::Strncpy(InGate.Error, InGate.bSuccess ? TEXT("") : *InError, UE_ARRAY_COUNT(InGate.Error));
		InGate.bCompleted = true;
		return true;
	}

	template<typename KeyType, typename ValueType, typename BuildGateType, typename BackingType, typename CapacityType>
	bool GetOrBuildValue(
		FRWLock& InLock,
		TMap<KeyType, TConstCachePtr<KeyType, ValueType>>& InValues,
		TMap<KeyType, TSharedPtr<BuildGateType, ESPMode::ThreadSafe>>& InBuilds,
		TArray<KeyType>& InKeys,
		BackingType& InBacking,
		CapacityType& InControlCapacity,
		const FVoxelGenerationPlanCache& InCache,
		const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
		const FVoxelGenerationConsumerKey& InControlConsumer,
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
		struct FOutputSource
		{
			const TConstCachePtr<KeyType, ValueType>& Value;
			const FVoxelTerrainResources* Resources;
		};
		const FOutputSource OutputSource{OutValue, InResources.Get()};
		FVoxelGenerationRequiredSourceScope OutputSources(InResources.Get(), &OutputSource,
			[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
			{
				const auto& Source = *static_cast<const FOutputSource*>(InContext);
				const auto& Value = Source.Value;
				if (Value && Value->ResourceLease && Source.Resources && Value->ResourceLease->IsOwnedBy(*Source.Resources))
				{
					FVoxelGenerationRequiredSourceScope::VisitLease(Value->ResourceLease.Get(), InVisit);
				}
			});
		FVoxelGenerationRequiredSourceScope CacheSources(InResources.Get(), &InCache,
			[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
			{
				const auto& Cache = *static_cast<const FVoxelGenerationPlanCache*>(InContext);
				uint64 Bytes;
				int32 Count;
				Cache.GetRequiredInputCapacity(Bytes, Count);
				if (Count) InVisit({&Cache, 0, EVoxelTerrainResourcePool::Input, Bytes, Count});
			});
		FVoxelGenerationConsumerKey BuildConsumer;
		if (InResources && !ResolveBuildConsumer(InControlConsumer, BuildConsumer, OutError))
		{
			return false;
		}
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BodyLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResidentReservation;
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
				if (!PrepareControlStorage(InValues, InBuilds, InKeys, InBacking, InControlCapacity, 1, 1,
					InResources, InControlConsumer, OutError))
				{
					return false;
				}
				TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> GateLease;
				FVoxelGenerationRequiredCapacity Sources;
				if (InResources && !FVoxelGenerationRequiredSourceScope::Collect(*InResources, nullptr, Sources, OutError)) return false;
				const auto GateAdmission = InResources ? InResources->TryAcquire(BuildConsumer, EVoxelTerrainResourcePool::Input,
					VoxelBoundedAllocation::WorkerSharedBytes<BuildGateType>(), GateLease, 0,
					Sources.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Sources.LeaseCount) : EVoxelTerrainResourceAdmission::Granted;
				if (GateAdmission != EVoxelTerrainResourceAdmission::Granted)
				{
					OutError = GateAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable
						? TEXT("CapacityUnachievable: generation cache build gate and original sources")
						: TEXT("CapacityBlocked: generation cache build gate");
					return false;
				}
				FVoxelGenerationRequiredSourceScope GateSources(InResources.Get(), GateLease.Get());
				if constexpr (TCacheBodyCapacity<ValueType>::Enabled)
				{
					const uint64 BodyBytes = VoxelBoundedAllocation::WorkerSharedBytes<const ValueType>();
					if (InResources)
					{
						if (!FVoxelGenerationRequiredSourceScope::Collect(*InResources, nullptr, Sources, OutError)) return false;
						const auto Limits = InResources->Snapshot().Limits;
						const uint64 Working = Sources.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Working)];
						const uint64 Resident = Sources.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Resident)];
						if (Working > Limits.WorkingBytes || BodyBytes > Limits.WorkingBytes - Working ||
							Resident > Limits.ResidentBytes - Limits.OverlapBytes ||
							BodyBytes > Limits.ResidentBytes - Limits.OverlapBytes - Resident)
						{
							OutError = TEXT("Oversized: generation cache native body and original sources exceed pool limit");
							return false;
						}
						const auto WorkingAdmission = InResources->TryAcquire(BuildConsumer, EVoxelTerrainResourcePool::Working,
							BodyBytes, BodyLease, 0, Sources.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Sources.LeaseCount);
						const auto ResidentAdmission = WorkingAdmission == EVoxelTerrainResourceAdmission::Granted
							? InResources->TryAcquire(BuildConsumer, EVoxelTerrainResourcePool::Resident, BodyBytes, ResidentReservation,
								0, Sources.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Sources.LeaseCount + 1)
							: WorkingAdmission;
						if (WorkingAdmission != EVoxelTerrainResourceAdmission::Granted || ResidentAdmission != EVoxelTerrainResourceAdmission::Granted)
						{
							OutError = WorkingAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ||
								ResidentAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable
								? TEXT("CapacityUnachievable: generation cache native body and original sources")
								: TEXT("CapacityBlocked: generation cache native body");
							return false;
						}
					}
				}
				Gate = GateLease ? VoxelBoundedAllocation::MakeWorkerShared<BuildGateType>(GateLease) :
					MakeShared<BuildGateType, ESPMode::ThreadSafe>();

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
		const FVoxelGenerationRequiredSource GateSource{Gate.Get(), 0, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::WorkerSharedBytes<BuildGateType>()};
		FVoxelGenerationRequiredSourceScope ActiveGateSource(InResources.Get(), &GateSource,
			[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
			{
				InVisit(*static_cast<const FVoxelGenerationRequiredSource*>(InContext));
			});

		if (!bOwner)
		{
			if (!bInAllowWorkerWait || IsInGameThread())
			{
				OutError = TEXT("DependencyNotReady: voxel generation cache key is building");
				return false;
			}
			const double WaitStart =
				FPlatformTime::Seconds();

			for (;;)
			{
				bool bCompleted = false;
				{
					FReadScopeLock Scope(InLock);
					bCompleted = Gate->bCompleted;
				}
				if (bCompleted)
				{
					break;
				}
				if (InCancel && InCancel->Load())
				{
					OutError =
						TEXT("Canceled");

					return false;
				}
				// 仅 tooling 允许 worker 轮询；gate 不拥有未知平台事件分配。
				FPlatformProcess::Sleep(0.002f);
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

		// scope 与原租约先声明；失败正文先释放 native backing，随后才退出上下文与授额。
		TOptional<FVoxelGenerationAllocationScope> AllocationScope;
		if constexpr (TCacheBodyCapacity<ValueType>::Enabled)
		{
			AllocationScope.Emplace(InResources, BuildConsumer, BodyLease, ResidentReservation,
				VoxelBoundedAllocation::WorkerSharedBytes<const ValueType>());
		}
		ValueType LocalValue;
		FString BuildError;

		bool bSuccess =
			(!InCancel || !InCancel->Load()) &&
			InBuild(
				LocalValue,
				BuildError);
		if constexpr (TCacheBodyCapacity<ValueType>::Enabled)
		{
			if (!AllocationScope->Check(BuildError))
			{
				bSuccess = false;
			}
			else if (bSuccess)
			{
				bSuccess = AllocationScope->ValidateRetainedBytes(TCacheBodyCapacity<ValueType>::BackingBytes(LocalValue), BuildError);
			}
		}

		TConstCachePtr<KeyType, ValueType> BuiltValue;

		if (bSuccess && (!InCancel || !InCancel->Load()))
		{
			LocalValue.ResourceLease = BodyLease;
			BuiltValue = BodyLease ? VoxelBoundedAllocation::MakeWorkerShared<const ValueType>(BodyLease, MoveTemp(LocalValue)) :
				MakeShared<const ValueType, ESPMode::ThreadSafe>(MoveTemp(LocalValue));
		}

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
					bool bCanPublish = true;
					if constexpr (TCacheBodyCapacity<ValueType>::Enabled)
					{
						if (BodyLease)
						{
							const auto CacheOwner = CacheBodyOwner(InControlConsumer, InKey);
							bCanPublish = BodyLease->CommitReservation(ResidentReservation.ToSharedRef(), CacheOwner);
							if (!bCanPublish) BuildError = TEXT("CapacityBlocked: generation cache native body publication");
						}
					}
					if (bCanPublish)
					{
						check(InValues.Num() < InBacking.ValuesCapacity);
						check(InKeys.Num() < InBacking.KeysCapacity);
						InValues.Add(InKey, BuiltValue);
						InKeys.Add(InKey);
						if (BodyLease) BodyLease->MarkPublished();
						ReadyValue = BuiltValue;
					}
				}

				if (!ReadyValue && BuildError.IsEmpty())
				{
					BuildError = TEXT("Voxel generation cache build failed");
				}
				CompleteBuildGate(*Gate, ReadyValue, BuildError);
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

	template<typename KeyType, typename ValueType, typename BuildGateType, typename BackingType, typename CapacityType>
	bool StoreValue(
		FRWLock& InLock,
		TMap<KeyType, TConstCachePtr<KeyType, ValueType>>& InValues,
		TMap<KeyType, TSharedPtr<BuildGateType, ESPMode::ThreadSafe>>& InBuilds,
		TArray<KeyType>& InKeys,
		BackingType& InBacking,
		CapacityType& InControlCapacity,
		const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
		const FVoxelGenerationConsumerKey& InControlConsumer,
		const KeyType& InKey,
		TConstCachePtr<KeyType, ValueType> InValue,
		FString& OutError)
	{
		if (!InValue)
		{
			OutError = TEXT("InvalidValue: generation cache Store requires an immutable body");
			return false;
		}
		FVoxelGenerationConsumerKey Consumer;
		if (InResources && !ResolveBuildConsumer(InControlConsumer, Consumer, OutError)) return false;

		TSharedPtr<BuildGateType, ESPMode::ThreadSafe> Gate;
		{
			FWriteScopeLock Scope(InLock);
			if (const TConstCachePtr<KeyType, ValueType>* Published = InValues.Find(InKey))
			{
				// 已发布的同 key 正文不更换，传入引用也在锁外退出。
				if (const TSharedPtr<BuildGateType, ESPMode::ThreadSafe>* Existing = InBuilds.Find(InKey))
				{
					Gate = *Existing;
					CompleteBuildGate(*Gate, *Published, FString());
					InBuilds.Remove(InKey);
				}
			}
			else
			{
				if (InResources)
				{
					OutError = TEXT("InvalidResource: generation cache Store cannot prove the original native body lease");
					return false;
				}
				const int32 Additional = InBuilds.Contains(InKey) ? 0 : 1;
				if (!PrepareControlStorage(InValues, InBuilds, InKeys, InBacking, InControlCapacity, Additional, 0,
					InResources, InControlConsumer, OutError))
				{
					return false;
				}
				check(InValues.Num() < InBacking.ValuesCapacity);
				check(InKeys.Num() < InBacking.KeysCapacity);
				InValues.Add(InKey, InValue);
				InKeys.Add(InKey);
				if (const TSharedPtr<BuildGateType, ESPMode::ThreadSafe>* Existing = InBuilds.Find(InKey))
				{
					Gate = *Existing;
					CompleteBuildGate(*Gate, InValue, FString());
					InBuilds.Remove(InKey);
				}
			}
		}
		OutError.Reset();
		return true;
	}

	template<typename KeyType, typename BuildGateType>
	void CancelBuildGates(TMap<KeyType, TSharedPtr<BuildGateType, ESPMode::ThreadSafe>>& InBuilds)
	{
		for (auto& Pair : InBuilds)
		{
			CompleteBuildGate(*Pair.Value, nullptr, TEXT("DependencyNotReady: voxel generation cache reset during build"));
		}
	}

	template<typename KeyType, typename ValueType, typename KeepPredicate>
	bool TrimMapBudgeted(
		TMap<KeyType, TSharedPtr<const ValueType, ESPMode::ThreadSafe>>& InValues,
		TArray<KeyType>& InKeys,
		int32& InOutCursor,
		int32& InOutBudget,
		TRetiredCacheValues<ValueType>& OutRetired,
		KeepPredicate&& InKeep,
		const bool bRetireUnreferenced = false)
	{
		while (InOutBudget > 0 &&
			!InKeys.IsEmpty())
		{
			if (OutRetired.Num() == MaximumRetiredEntriesPerGroup)
			{
				return false;
			}
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

			if (!InKeep(Key) || (bRetireUnreferenced && InValues.FindChecked(Key).IsUnique()))
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
		return true;
	}
}

struct FVoxelGenerationPlanCache::FBuildGate
{
	FBuildGate()
	{
	}

	~FBuildGate()
	{
	}

	// Result/Error/bSuccess/bCompleted 仅在所属 shard 锁下读写。
	TSharedPtr<const void, ESPMode::ThreadSafe> Result;
	bool bCompleted = false;
	bool bSuccess = false;
	TCHAR Error[1024] = {};
};

#if WITH_DEV_AUTOMATION_TESTS
FVoxelGenerationPlanCache::FVoxelGenerationPlanCache(
	const bool bInAllowGameThreadBuilds,
	const bool bInAllowWorkerWait)
	: bAllowGameThreadBuilds(
		bInAllowGameThreadBuilds)
	, bAllowWorkerWait(bInAllowWorkerWait)
{
	InitializeShards();
}
#endif

FVoxelGenerationPlanCache::FVoxelGenerationPlanCache(
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InControlConsumer,
	const bool bInAllowGameThreadBuilds,
	const bool bInAllowWorkerWait)
	: Resources(InResources)
	, ControlConsumer(InControlConsumer)
	, bAllowGameThreadBuilds(bInAllowGameThreadBuilds)
	, bAllowWorkerWait(bInAllowWorkerWait)
{
	InitializeShards();
}

bool FVoxelGenerationPlanCache::Create(
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InControlConsumer,
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>& OutCache,
	FString& OutError,
	const bool bInAllowGameThreadBuilds,
	const bool bInAllowWorkerWait)
{
	if (!InControlConsumer.IsValid() || (InResources->GetControlConsumer().IsValid() &&
		InResources->GetControlConsumer().WorldEpoch != InControlConsumer.WorldEpoch))
	{
		OutError = TEXT("InvalidConsumer: generation cache requires a complete World control identity");
		return false;
	}
	const uint64 RootBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelGenerationPlanCache>() +
		uint64(ShardCount) * (FMemory::QuantizeSize(sizeof(FNaturalShard), alignof(FNaturalShard)) +
			FMemory::QuantizeSize(sizeof(FPlanShard), alignof(FPlanShard))) +
		VoxelBoundedAllocation::ArrayBytes<TUniquePtr<FNaturalShard>>(ShardCount) +
		VoxelBoundedAllocation::ArrayBytes<TUniquePtr<FPlanShard>>(ShardCount);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	FVoxelGenerationRequiredSourceScope OldCacheSources(
		OutCache && OutCache->GetResources().Get() == &InResources.Get() ? &InResources.Get() : nullptr,
		OutCache.Get(), EnumerateCacheRequiredSources);
	FVoxelGenerationRequiredCapacity Required;
	if (!FVoxelGenerationRequiredSourceScope::Collect(InResources.Get(), nullptr, Required, OutError)) return false;
	const auto Admission = InResources->TryAcquire(InControlConsumer, EVoxelTerrainResourcePool::Input, RootBytes, Lease,
		0, Required.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		OutError = Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable
			? TEXT("CapacityUnachievable: generation cache root, shards and native ledger")
			: TEXT("CapacityBlocked: generation cache root and shards");
		return false;
	}
	// 在类工厂中调用私有构造器；控制器/deleter 与 WorkerSharedBytes 使用同一实际类型。
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache(
		new FVoxelGenerationPlanCache(InResources, InControlConsumer, bInAllowGameThreadBuilds, bInAllowWorkerWait),
		VoxelBoundedAllocation::TWorkerDeleter<FVoxelGenerationPlanCache>{Lease});
	Cache->RootInputBytes = RootBytes;
	Cache->RootLease = Lease;
	OutCache = MoveTemp(Cache);
	OutError.Reset();
	return true;
}

void FVoxelGenerationPlanCache::InitializeShards()
{
	for (int32 Index = 0; Index < 5; ++Index)
	{
		PlanHits[Index].Store(0);
		PlanMisses[Index].Store(0);
		PlanDonations[Index].Store(0);
	}
	NaturalShards.Reserve(ShardCount);
	PlanShards.Reserve(ShardCount);
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

const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& FVoxelGenerationPlanCache::GetResources() const
{
	return Resources;
}

void FVoxelGenerationPlanCache::FControlBackingCapacity::Replace(
	const FVoxelTerrainResourceLease* InOldLease,
	const FVoxelTerrainResourceLease* InNewLease)
{
	const uint64 OldBytes = InOldLease ? InOldLease->Bytes() : 0;
	const uint64 NewBytes = InNewLease ? InNewLease->Bytes() : 0;
	FWriteScopeLock Scope(Lock);
	check(OldBytes <= Bytes && NewBytes <= MAX_uint64 - (Bytes - OldBytes));
	Bytes = Bytes - OldBytes + NewBytes;
	LeaseCount += int32(InNewLease != nullptr) - int32(InOldLease != nullptr);
	check(LeaseCount >= 0);
}

void FVoxelGenerationPlanCache::MarkPublished() const
{
	if (RootLease) RootLease->MarkPublished();
}

void FVoxelGenerationPlanCache::GetRequiredInputCapacity(uint64& OutBytes, int32& OutLeaseCount) const
{
	FReadScopeLock Scope(ControlBackingCapacity.Lock);
	OutBytes = RootInputBytes + ControlBackingCapacity.Bytes;
	OutLeaseCount = int32(RootInputBytes != 0) + ControlBackingCapacity.LeaseCount;
}

const FVoxelGenerationConsumerKey& FVoxelGenerationPlanCache::GetControlConsumer() const
{
	return ControlConsumer;
}

bool FVoxelGenerationPlanCache::ResolveConsumer(
	FVoxelGenerationConsumerKey& OutConsumer, FString& OutError) const
{
	FVoxelGenerationConsumerKey Consumer;
	if (!Resources)
	{
#if WITH_DEV_AUTOMATION_TESTS
		OutConsumer = ControlConsumer;
		OutError.Reset();
		return true;
#else
		OutError = TEXT("InvalidConsumer: generation cache has no original World ledger");
		return false;
#endif
	}
	if (!ResolveBuildConsumer(ControlConsumer, Consumer, OutError))
	{
		return false;
	}
	OutConsumer = Consumer;
	OutError.Reset();
	return true;
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
	FReadScopeLock Lock(PlanDiagnosticsLock);
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
	FWriteScopeLock Lock(PlanDiagnosticsLock);
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
			Shard.BaseColumnBacking,
			ControlBackingCapacity,
			*this,
			Resources,
			ControlConsumer,
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
			Shard.NaturalColumnBacking,
			ControlBackingCapacity,
			*this,
			Resources,
			ControlConsumer,
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

bool FVoxelGenerationPlanCache::StoreHydrology(
	const FVoxelHydrologyRegionKey& InKey,
	FVoxelHydrologyPlanPtr InPlan,
	FString* OutError)
{
	FPlanShard& Shard =
		*PlanShards[
			PlanShardIndex(InKey)];

	FString Error;
	const bool bStored = StoreValue(Shard.Lock, Shard.Hydrology, Shard.HydrologyBuilds,
		Shard.HydrologyKeys, Shard.HydrologyBacking, ControlBackingCapacity, Resources, ControlConsumer, InKey, MoveTemp(InPlan), Error);
	if (OutError) *OutError = MoveTemp(Error);
	return bStored;
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
			Shard.HydrologyBacking,
			ControlBackingCapacity,
			*this,
			Resources,
			ControlConsumer,
			InKey,
			[this, &InBuild](FVoxelHydrologyPlan& Value, FString& Error)
			{
				const double Started = FPlatformTime::Seconds();
				const bool bSuccess = InBuild(Value, Error);
				if (bSuccess || (!Error.StartsWith(TEXT("DependencyNotReady")) &&
					!Error.StartsWith(TEXT("CapacityBlocked")) && Error != TEXT("Canceled")))
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

bool FVoxelGenerationPlanCache::StoreCave(
	const FVoxelGenerationTileKey& InKey,
	FVoxelCavePlanPtr InPlan,
	FString* OutError)
{
	FPlanShard& Shard =
		*PlanShards[
			PlanShardIndex(InKey)];

	FString Error;
	const bool bStored = StoreValue(Shard.Lock, Shard.Caves, Shard.CaveBuilds,
		Shard.CaveKeys, Shard.CaveBacking, ControlBackingCapacity, Resources, ControlConsumer, InKey, MoveTemp(InPlan), Error);
	if (OutError) *OutError = MoveTemp(Error);
	return bStored;
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
			Shard.CaveBacking,
			ControlBackingCapacity,
			*this,
			Resources,
			ControlConsumer,
			InKey,
			[this, &InBuild](FVoxelCavePlan& Value, FString& Error)
			{
				const double Started = FPlatformTime::Seconds();
				const bool bSuccess = InBuild(Value, Error);
				if (bSuccess || (!Error.StartsWith(TEXT("DependencyNotReady")) &&
					!Error.StartsWith(TEXT("CapacityBlocked")) && Error != TEXT("Canceled")))
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
		Shard.EcologyBacking,
		ControlBackingCapacity,
		*this,
		Resources,
		ControlConsumer,
		InKey,
			[this, &InBuild](FVoxelEcologyPlan& Value, FString& Error)
			{
				const double Started = FPlatformTime::Seconds();
				const bool bSuccess = InBuild(Value, Error);
				if (bSuccess || (!Error.StartsWith(TEXT("DependencyNotReady")) &&
					!Error.StartsWith(TEXT("CapacityBlocked")) && Error != TEXT("Canceled")))
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

bool FVoxelGenerationPlanCache::StoreFeature(
	const FVoxelGenerationTileKey& InKey,
	FVoxelFeaturePlanPtr InPlan,
	FString* OutError)
{
	FPlanShard& Shard =
		*PlanShards[
			PlanShardIndex(InKey)];

	FString Error;
	const bool bStored = StoreValue(Shard.Lock, Shard.Features, Shard.FeatureBuilds,
		Shard.FeatureKeys, Shard.FeatureBacking, ControlBackingCapacity, Resources, ControlConsumer, InKey, MoveTemp(InPlan), Error);
	if (OutError) *OutError = MoveTemp(Error);
	return bStored;
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
			Shard.FeatureBacking,
			ControlBackingCapacity,
			*this,
			Resources,
			ControlConsumer,
			InKey,
			[this, &InBuild](FVoxelFeaturePlan& Value, FString& Error)
			{
				const double Started = FPlatformTime::Seconds();
				const bool bSuccess = InBuild(Value, Error);
				if (bSuccess || (!Error.StartsWith(TEXT("DependencyNotReady")) &&
					!Error.StartsWith(TEXT("CapacityBlocked")) && Error != TEXT("Canceled")))
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

bool FVoxelGenerationPlanCache::StoreStructure(
	const FVoxelGenerationTileKey& InKey,
	FVoxelStructurePlanPtr InPlan,
	FString* OutError)
{
	FPlanShard& Shard =
		*PlanShards[
			PlanShardIndex(InKey)];

	FString Error;
	const bool bStored = StoreValue(Shard.Lock, Shard.Structures, Shard.StructureBuilds,
		Shard.StructureKeys, Shard.StructureBacking, ControlBackingCapacity, Resources, ControlConsumer, InKey, MoveTemp(InPlan), Error);
	if (OutError) *OutError = MoveTemp(Error);
	return bStored;
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
			Shard.StructureBacking,
			ControlBackingCapacity,
			*this,
			Resources,
			ControlConsumer,
			InKey,
			[this, &InBuild](FVoxelStructurePlan& Value, FString& Error)
			{
				const double Started = FPlatformTime::Seconds();
				const bool bSuccess = InBuild(Value, Error);
				if (bSuccess || (!Error.StartsWith(TEXT("DependencyNotReady")) &&
					!Error.StartsWith(TEXT("CapacityBlocked")) && Error != TEXT("Canceled")))
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

bool FVoxelGenerationPlanCache::UpdateRetention(
	const FVoxelGenerationCacheRetention& InRetention,
	const int32 InPointCount,
	TFunctionRef<bool(TArrayView<FVoxelGenerationCacheRetentionPoint>, FString&)> InFill,
	FString& OutError,
	const TAtomic<bool>* InCancel)
{
	if (InPointCount < 0)
	{
		OutError = TEXT("Oversized: generation cache retention point count");
		return false;
	}
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	if (Resources && !ControlConsumer.IsValid())
	{
		OutError = TEXT("InvalidConsumer: generation cache retention requires the original complete control identity");
		return false;
	}
#if !WITH_DEV_AUTOMATION_TESTS
	if (!Resources)
	{
		OutError = TEXT("InvalidConsumer: generation cache retention has no original World ledger");
		return false;
	}
#endif

	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CandidateLease;
	FVoxelGenerationRequiredSourceScope CacheSources(Resources.Get(), this, EnumerateCacheRequiredSources);
	FVoxelGenerationCacheRetention Candidate;
	Candidate.HydrologyRegionSide = InRetention.HydrologyRegionSide;
	Candidate.HydrologyCellSize = InRetention.HydrologyCellSize;
	Candidate.Revision = InRetention.Revision;
	const uint64 Bytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<
		FVoxelGenerationCacheRetentionPoint, FDefaultAllocator>(InPointCount);
	if (Resources)
	{
		FReadScopeLock Scope(RetentionLock);
		FVoxelGenerationRequiredCapacity Required;
		if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, OutError)) return false;
		const uint64 Limit = Resources->Snapshot().Limits.InputBytes;
		const uint64 PreviousBytes = Required.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)];
		if (Bytes > Limit || PreviousBytes > Limit - Bytes)
		{
			OutError = TEXT("Oversized: generation cache retention snapshot allocation peak exceeds original input pool limit");
			return false;
		}
		const auto Admission = Bytes ? Resources->TryAcquire(ControlConsumer, EVoxelTerrainResourcePool::Input,
			Bytes, CandidateLease, 0, PreviousBytes, Required.LeaseCount) : EVoxelTerrainResourceAdmission::Granted;
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			OutError = Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable
				? TEXT("CapacityUnachievable: retention snapshot and fixed native input")
				: TEXT("CapacityBlocked: generation cache retention snapshot input capacity");
			return false;
		}
	}
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}

	Candidate.Points.Reserve(InPointCount);
	Candidate.Points.SetNum(InPointCount);
	if (CandidateLease)
	{
		const uint64 ActualBytes = VoxelGenerationAllocation::AllocatedBytes(Candidate.Points);
		check(ActualBytes <= CandidateLease->Bytes());
		const bool bResized = CandidateLease->Resize(ActualBytes);
		check(bResized);
	}
	if (!InFill(MakeArrayView(Candidate.Points), OutError))
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
		}
		else if (OutError.IsEmpty())
		{
			OutError = TEXT("Generation cache retention source fill failed");
		}
		return false;
	}

	{
		FWriteScopeLock Scope(RetentionLock);
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		Swap(Retention.Points, Candidate.Points);
		ControlBackingCapacity.Replace(RetentionLease.Get(), CandidateLease.Get());
		Swap(RetentionLease, CandidateLease);
		Retention.HydrologyRegionSide = Candidate.HydrologyRegionSide;
		Retention.HydrologyCellSize = Candidate.HydrologyCellSize;
		Retention.Revision = Candidate.Revision;
	}
	Candidate.Points.Empty();
	CandidateLease.Reset();
	OutError.Reset();
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
void FVoxelGenerationPlanCache::UpdateRetention(const FVoxelGenerationCacheRetention& InRetention)
{
	FString Error;
	const bool bUpdated = UpdateRetention(InRetention, InRetention.Points.Num(),
		[&InRetention](TArrayView<FVoxelGenerationCacheRetentionPoint> OutPoints, FString&)
		{
			for (int32 Index = 0; Index < OutPoints.Num(); ++Index)
			{
				OutPoints[Index] = InRetention.Points[Index];
			}
			return true;
		}, Error);
	checkf(bUpdated, TEXT("%s"), *Error);
}
#endif

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
	if (Resources && (Resources.Get() != &InResources.Get() || InConsumer.WorldEpoch != ControlConsumer.WorldEpoch))
	{
		OutError = TEXT("InvalidConsumer: voxel cache retention must use the original World ledger");
		return false;
	}

	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> NewLease;
	FVoxelGenerationRequiredSourceScope CacheSources(Resources.Get(), this, EnumerateCacheRequiredSources);
	FVoxelGenerationRequiredSourceScope OldOutputSources(&InResources.Get(),
		OutLease && OutLease->ResourceLease->IsOwnedBy(InResources.Get()) ? &OutLease->ResourceLease.Get() : nullptr);
	{
		FWriteScopeLock Scope(RetentionLock);
		if (NextTaskRetentionId == MAX_uint64)
		{
			OutError = TEXT("CapacityUnachievable: voxel cache task retention identity exhausted");
			return false;
		}

		FVoxelGenerationRequiredCapacity Required;
		if (!FVoxelGenerationRequiredSourceScope::Collect(InResources.Get(), nullptr, Required, OutError)) return false;
		const auto Admission = InResources->TryAcquire(InConsumer, EVoxelTerrainResourcePool::Input,
			FVoxelGenerationCacheRetentionLease::GetRequiredAllocationBytes(), ResourceLease,
			0, Required.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			OutError = Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable
				? TEXT("CapacityUnachievable: voxel cache task retention and original native sources")
				: TEXT("CapacityBlocked: voxel cache task retention input capacity");
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
	bool bNaturalCapacityPressure = false;
	uint64 NaturalHeadroom = 0;
	// 自然列由冻结配置确定；容量紧张时退休仅由 Cache 持有的正文，为新采样腾出空间。
	// 正在读取的 shared ref 及依赖就绪所需的五族计划继续保持原有生命周期。
	if (Resources)
	{
		const auto State = Resources->Snapshot();
		const uint64 NativeLimit = State.Limits.ResidentBytes - State.Limits.OverlapBytes;
		// 至少留出一轮自然列和常规驻留区的 1/16，不能只在剩余不足一个计划正文时才开始退休。
		NaturalHeadroom = FMath::Min(NativeLimit, FMath::Max(NativeLimit / 16,
			uint64(MaximumRetiredEntriesPerGroup * ShardCount) *
			(VoxelBoundedAllocation::WorkerSharedBytes<const FVoxelBaseColumnEntry>() +
				VoxelBoundedAllocation::WorkerSharedBytes<const FVoxelNaturalColumnEntry>())));
		bNaturalCapacityPressure = State.Used[int32(EVoxelTerrainResourcePool::Resident)] >= NativeLimit - NaturalHeadroom;
	}

	int32 RemainingGroups = 7;
	auto TakeGroupBudget =
		[&RemainingBudget, &RemainingGroups]()
		{
			const int32 GroupBudget =
				RemainingGroups > 0
					? int32((int64(RemainingBudget) + RemainingGroups - 1) / RemainingGroups)
					: 0;

			RemainingBudget -= GroupBudget;
			--RemainingGroups;
			return GroupBudget;
		};

	const int32 PressureColumnBudget = bNaturalCapacityPressure ?
		FMath::Min(MaximumRetiredEntriesPerGroup, RemainingBudget / 4) : 0;
	RemainingBudget -= PressureColumnBudget * 2;
	int32 HydrologyBudget = TakeGroupBudget();
	int32 BaseColumnBudget = TakeGroupBudget() + PressureColumnBudget;
	int32 NaturalColumnBudget = TakeGroupBudget() + PressureColumnBudget;
	int32 CaveBudget = TakeGroupBudget();
	int32 EcologyBudget = TakeGroupBudget();
	int32 FeatureBudget = TakeGroupBudget();
	int32 StructureBudget = TakeGroupBudget();
	TRetiredCacheValues<FVoxelBaseColumnEntry> RetiredBaseColumns;
	TRetiredCacheValues<FVoxelNaturalColumnEntry> RetiredNaturalColumns;
	TRetiredCacheValues<FVoxelHydrologyPlan> RetiredHydrology;
	TRetiredCacheValues<FVoxelCavePlan> RetiredCaves;
	TRetiredCacheValues<FVoxelEcologyPlan> RetiredEcology;
	TRetiredCacheValues<FVoxelFeaturePlan> RetiredFeatures;
	TRetiredCacheValues<FVoxelStructurePlan> RetiredStructures;

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

	bool bRetirementLimited = false;

	{
		FNaturalShard& Shard =
			*NaturalShards[
				ShardIndex];

		FWriteScopeLock Scope(
			Shard.Lock);

		bRetirementLimited |= !TrimMapBudgeted(
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
			}, bNaturalCapacityPressure);

		bRetirementLimited |= !TrimMapBudgeted(
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
			}, bNaturalCapacityPressure);

	}

	{
		FPlanShard& Shard =
			*PlanShards[
				ShardIndex];

		FWriteScopeLock Scope(
			Shard.Lock);

		bRetirementLimited |= !TrimMapBudgeted(
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

		bRetirementLimited |= !TrimMapBudgeted(
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

		bRetirementLimited |= !TrimMapBudgeted(
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

		bRetirementLimited |= !TrimMapBudgeted(
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

		bRetirementLimited |= !TrimMapBudgeted(
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
	if (!bRetirementLimited)
	{
		MaintenanceShardCursor = (ShardIndex + 1) % ShardCount;
	}
	RetentionScope.Reset();
#if !UE_BUILD_SHIPPING
	static double NextPressureDiagnostic = 0.0;
	if (bNaturalCapacityPressure && FPlatformTime::Seconds() >= NextPressureDiagnostic)
	{
		const auto State = Resources->Snapshot();
		UE_LOG(LogTemp, Display, TEXT("Voxel natural cache pressure: epoch=%llu resident=%llu nativeLimit=%llu headroom=%llu checkedLimit=%d retiredBase=%d retiredNatural=%d shard=%d"),
			ControlConsumer.WorldEpoch, State.Used[int32(EVoxelTerrainResourcePool::Resident)],
			State.Limits.ResidentBytes - State.Limits.OverlapBytes, NaturalHeadroom, InMaxEntries,
			RetiredBaseColumns.Num(), RetiredNaturalColumns.Num(), ShardIndex);
		NextPressureDiagnostic = FPlatformTime::Seconds() + 5.0;
	}
#endif
	RetiredBaseColumns.Reset();
	RetiredNaturalColumns.Reset();
	RetiredHydrology.Reset();
	RetiredCaves.Reset();
	RetiredEcology.Reset();
	RetiredFeatures.Reset();
	RetiredStructures.Reset();
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

			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetiredBaseLease;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetiredNaturalLease;
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
				ControlBackingCapacity.Replace(Shard.BaseColumnBacking.ValuesLease.Get(), nullptr);
				ControlBackingCapacity.Replace(Shard.NaturalColumnBacking.ValuesLease.Get(), nullptr);
				RetiredBaseLease = MoveTemp(Shard.BaseColumnBacking.ValuesLease);
				RetiredNaturalLease = MoveTemp(Shard.NaturalColumnBacking.ValuesLease);
				Shard.BaseColumnBacking.ValuesCapacity = 0;
				Shard.NaturalColumnBacking.ValuesCapacity = 0;
				Shard.BaseColumnKeys.Reset();
				Shard.NaturalColumnKeys.Reset();
				Shard.BaseColumnCursor = 0;
				Shard.NaturalColumnCursor = 0;
			}
		}

		{
			FPlanShard& Shard =
				*PlanShards[Index];

			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetiredHydrologyLease;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetiredCaveLease;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetiredEcologyLease;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetiredFeatureLease;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetiredStructureLease;
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
				ControlBackingCapacity.Replace(Shard.HydrologyBacking.ValuesLease.Get(), nullptr);
				ControlBackingCapacity.Replace(Shard.CaveBacking.ValuesLease.Get(), nullptr);
				ControlBackingCapacity.Replace(Shard.EcologyBacking.ValuesLease.Get(), nullptr);
				ControlBackingCapacity.Replace(Shard.FeatureBacking.ValuesLease.Get(), nullptr);
				ControlBackingCapacity.Replace(Shard.StructureBacking.ValuesLease.Get(), nullptr);
				RetiredHydrologyLease = MoveTemp(Shard.HydrologyBacking.ValuesLease);
				RetiredCaveLease = MoveTemp(Shard.CaveBacking.ValuesLease);
				RetiredEcologyLease = MoveTemp(Shard.EcologyBacking.ValuesLease);
				RetiredFeatureLease = MoveTemp(Shard.FeatureBacking.ValuesLease);
				RetiredStructureLease = MoveTemp(Shard.StructureBacking.ValuesLease);
				Shard.HydrologyBacking.ValuesCapacity = 0;
				Shard.CaveBacking.ValuesCapacity = 0;
				Shard.EcologyBacking.ValuesCapacity = 0;
				Shard.FeatureBacking.ValuesCapacity = 0;
				Shard.StructureBacking.ValuesCapacity = 0;
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
	{
		FReadScopeLock Scope(RetentionLock);
		Bytes = VoxelGenerationAllocation::AllocatedBytes(Retention.Points);
	}

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
