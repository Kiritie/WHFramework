#include "Voxel/Generation/VoxelGenerationPlanCache.h"

#include "HAL/PlatformProcess.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Misc/ScopeExit.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/Climate/VoxelClimateGenerator.h"
#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyComponent.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
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
					if constexpr (std::is_same_v<ValueType, FVoxelHydrologyPlan>)
					{
						Value->VisitHydrologyLeases([&](const FVoxelTerrainResourceLease* InLease)
						{
							FVoxelGenerationRequiredSourceScope::VisitLease(InLease, InVisit);
						});
					}
					else FVoxelGenerationRequiredSourceScope::VisitLease(Value->ResourceLease.Get(), InVisit);
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

uint64 FVoxelHydrologyFinePlan::GetAllocatedBytes() const
{
	return VoxelGenerationAllocation::AllocatedBytes(Members) +
		VoxelGenerationAllocation::AllocatedBytes(FixedContacts) + VoxelGenerationAllocation::AllocatedBytes(CertifiedDry);
}

bool FVoxelHydrologyFinePlan::Contains(const FIntPoint& InCell) const
{
	if (InCell == Key || CertifiedDry.Contains(InCell)) return true;
	if (!Water.bNaturalWater || InCell.X < Minimum.X || InCell.Y < Minimum.Y ||
		InCell.X > Maximum.X || InCell.Y > Maximum.Y) return false;
	int32 Low = 0, High = Members.Num();
	while (Low < High)
	{
		const int32 Mid = Low + (High - Low) / 2;
		const auto& Cell = Members[Mid];
		if (Cell == InCell) return true;
		if (Cell.X < InCell.X || (Cell.X == InCell.X && Cell.Y < InCell.Y)) Low = Mid + 1;
		else High = Mid;
	}
	return false;
}

FVoxelHydrologyFineWaterResult FVoxelHydrologyFinePlan::SampleWater(const FIntPoint& InCell) const
{
    check(Contains(InCell));
    return CertifiedDry.Contains(InCell) ? FVoxelHydrologyFineWaterResult() : Water;
}

namespace
{
    int32 FineWaterIndexLevel(const FVoxelHydrologyFinePlan& Plan)
    {
        const int64 Span = FMath::Max(int64(Plan.IndexMaximum.X) - Plan.IndexMinimum.X + 1,
            int64(Plan.IndexMaximum.Y) - Plan.IndexMinimum.Y + 1);
        int32 Level = 6;
        while (Level < 32 && (int64(1) << Level) < Span) ++Level;
        return Level;
    }

    template<typename ShardType>
    void LinkFineWaterIndex(ShardType& Shard, const FVoxelHydrologyFinePlan& Plan)
    {
        Plan.IndexLinkCount = 0;
        if (Plan.IndexMinimum == Plan.Key && Plan.IndexMaximum == Plan.Key) return;
        const int32 Level = FineWaterIndexLevel(Plan);
        const int64 Side = int64(1) << Level;
        for (int64 Y = VoxelGeneration::FloorDivideWide(Plan.IndexMinimum.Y, Side);
            Y <= VoxelGeneration::FloorDivideWide(Plan.IndexMaximum.Y, Side); ++Y)
        for (int64 X = VoxelGeneration::FloorDivideWide(Plan.IndexMinimum.X, Side);
            X <= VoxelGeneration::FloorDivideWide(Plan.IndexMaximum.X, Side); ++X)
        {
            check(Plan.IndexLinkCount < 4);
            auto& Link = Plan.IndexLinks[Plan.IndexLinkCount++];
            Link.Bucket = FIntVector(int32(X), int32(Y), Level);
            Link.Plan = &Plan; Link.Previous = nullptr;
            auto* const* Head = Shard.FineWaterIndex.Find(Link.Bucket);
            Link.Next = Head ? *Head : nullptr;
            if (Link.Next) Link.Next->Previous = &Link;
            Shard.FineWaterIndex.Add(Link.Bucket, &Link);
        }
        Shard.FineWaterIndexLevels |= uint32(1) << (Level - 6);
    }

    template<typename ShardType>
    void UnlinkFineWaterIndex(ShardType& Shard, const FVoxelHydrologyFinePlan& Plan)
    {
        for (int32 I = 0; I < Plan.IndexLinkCount; ++I)
        {
            auto& Link = Plan.IndexLinks[I];
            if (Link.Previous) Link.Previous->Next = Link.Next;
            else if (Link.Next) Shard.FineWaterIndex.FindChecked(Link.Bucket) = Link.Next;
            else Shard.FineWaterIndex.Remove(Link.Bucket);
            if (Link.Next) Link.Next->Previous = Link.Previous;
            Link.Previous = Link.Next = nullptr;
        }
        Plan.IndexLinkCount = 0;
    }
}

bool FVoxelHydrologyFinePlan::HasEquivalentOutput(const FVoxelHydrologyFinePlan& Other) const
{
	if (Key != Other.Key || Water.WaterZ != Other.Water.WaterZ ||
		Water.bNaturalWater != Other.Water.bNaturalWater || Water.bFixedWater != Other.Water.bFixedWater ||
		!(Water.FixedId == Other.Water.FixedId) || !(Water.NaturalKey == Other.Water.NaturalKey) ||
		Minimum != Other.Minimum || Maximum != Other.Maximum || Members != Other.Members || FixedContacts.Num() != Other.FixedContacts.Num()) return false;
	for (int32 Index = 0; Index < FixedContacts.Num(); ++Index)
	{
		const auto& A = FixedContacts[Index]; const auto& B = Other.FixedContacts[Index];
		if (A.Cell != B.Cell || A.GroundZ != B.GroundZ || A.ReceiverWaterZ != B.ReceiverWaterZ ||
			!(A.ReceiverId == B.ReceiverId)) return false;
	}
	return true;
}

struct FVoxelGenerationPlanCache::FFineWaterOwner
{
	FFineWaterOwner(TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> InTerrain,
		const FIntPoint& InCell, const FVoxelGenerationConsumerKey& InConsumer, uint64 InGeneration)
		: Config(InConfig), Terrain(InTerrain), Cell(InCell), Consumer(InConsumer), Generation(InGeneration)
	{

	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BodyLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResidentReservation;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> Terrain;
	TUniquePtr<FVoxelHydrologyComponentBuild> Reader;
	TUniquePtr<FVoxelHydrologyFineWaterQuery> Query;
	TMap<FVoxelHydrologyRegionKey, FVoxelHydrologyPlanPtr> Tiles;
	TArray<FVoxelHydrologyRegionKey> MissingTiles;
	FIntPoint Cell;
	FVoxelGenerationConsumerKey Consumer;
	uint64 Generation;
	FVoxelHydrologyFinePlan Plan;
	int32 MissingIndex = 0;
	int32 TileCapacity = 0;
	int32 CopyIndex = 0;
	int32 DryCopyIndex = 0, DryCapacity = 0;
	bool bQueryComplete = false;
	TArray<FVoxelHydrologySharedCell> ContactSortScratch;
	int64 SortWidth = 1, SortRun = 0, SortLeft = 0, SortRight = 0, SortWrite = 0;
	bool bMergeStarted = false;
	bool bCopying = false;
	bool bReady = false;
	TAtomic<bool> bCanceled{false};
	uint64 GetAllocatedBytes() const
	{
		return Plan.GetAllocatedBytes() + VoxelGenerationAllocation::AllocatedBytes(ContactSortScratch) + VoxelGenerationAllocation::AllocatedBytes(Tiles) +
			VoxelGenerationAllocation::AllocatedBytes(MissingTiles) +
			(Reader ? FMemory::QuantizeSize(sizeof(FVoxelHydrologyComponentBuild), alignof(FVoxelHydrologyComponentBuild)) + Reader->GetAllocatedBytes() : 0) +
			(Query ? FMemory::QuantizeSize(sizeof(FVoxelHydrologyFineWaterQuery), alignof(FVoxelHydrologyFineWaterQuery)) + Query->GetAllocatedBytes() : 0);
	}
	void VisitSources(FVoxelGenerationRequiredSourceScope::FVisit Visit) const
	{
		if (Config)
		{
			FVoxelGenerationRequiredSourceScope::VisitLease(Config->InputLease.Get(), Visit);
			FVoxelGenerationRequiredSourceScope::VisitLease(Config->RecipeInputLease.Get(), Visit);
		}
		if (Terrain) Visit({Terrain.Get(), 0, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::WorkerSharedBytes<const FVoxelTerrainGenerator>()});
		for (const auto& Pair : Tiles)
			Pair.Value->VisitHydrologyLeases([&](const FVoxelTerrainResourceLease* Lease)
			{ FVoxelGenerationRequiredSourceScope::VisitLease(Lease, Visit); });
	}
};

struct FVoxelGenerationPlanCache::FHydrologyComponentOwner
{
	FHydrologyComponentOwner(TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe,
		TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> InTerrain, const FIntPoint& InSeed)
	{
		Build.Emplace(InRecipe, InTerrain, InSeed);
	}
	~FHydrologyComponentOwner()
	{
	}

	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BodyLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResidentReservation;
	TOptional<FVoxelHydrologyComponentBuild> Build;
	FVoxelHydrologyRiverComponent Result;
	bool bReady = false;

	uint64 GetAllocatedBytes() const
	{
		return Result.GetAllocatedBytes() + (Build.IsSet() ? Build->GetAllocatedBytes() : 0);
	}
};

struct FVoxelGenerationPlanCache::FHydrologyRegionOwner
{
	FHydrologyRegionOwner(TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		TSharedRef<const FVoxelClimateGenerator, ESPMode::ThreadSafe> InClimate,
		TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> InTerrain,
		const FVoxelHydrologyRegionKey& InKey, const FVoxelGenerationConsumerKey& InConsumer,
		uint64 InGeneration)
		: Config(InConfig), Climate(InClimate), Terrain(InTerrain), Consumer(InConsumer), Generation(InGeneration)
	{
		Plan.Key = InKey;
	}
	~FHydrologyRegionOwner()
	{
	}

	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BodyLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResidentReservation;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<const FVoxelClimateGenerator, ESPMode::ThreadSafe> Climate;
	TSharedPtr<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> Terrain;
	FVoxelGenerationConsumerKey Consumer;
	uint64 Generation = 0;
	TArray<FIntPoint> Seeds;
	TArray<FVoxelHydrologyRegionKey> MissingTiles;
	TMap<FVoxelHydrologyRegionKey, FVoxelHydrologyPlanPtr> Tiles;
	TSharedPtr<FHydrologyComponentOwner, ESPMode::ThreadSafe> CurrentComponent;
	FVoxelHydrologyPlan Plan;
	int32 MissingIndex = 0;
	int32 SeedIndex = 0;
	int32 TileCapacity = 0;
	TAtomic<bool> bCanceled{false};
	bool bTopologyReady = false;
	bool bSeedsReady = false;
	bool bReady = false;

	uint64 GetAllocatedBytes() const
	{
		return VoxelGenerationAllocation::AllocatedBytes(Seeds) +
			VoxelGenerationAllocation::AllocatedBytes(MissingTiles) +
			VoxelGenerationAllocation::AllocatedBytes(Tiles) + Plan.GetAllocatedBytes();
	}

	void VisitSources(FVoxelGenerationRequiredSourceScope::FVisit InVisit) const
	{
		if (Config)
		{
			FVoxelGenerationRequiredSourceScope::VisitLease(Config->InputLease.Get(), InVisit);
			FVoxelGenerationRequiredSourceScope::VisitLease(Config->RecipeInputLease.Get(), InVisit);
		}
		if (Climate) InVisit({Climate.Get(), 0, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::WorkerSharedBytes<const FVoxelClimateGenerator>()});
		if (Terrain) InVisit({Terrain.Get(), 0, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::WorkerSharedBytes<const FVoxelTerrainGenerator>()});
		for (const auto& Pair : Tiles)
		{
			FVoxelGenerationRequiredSourceScope::VisitLease(Pair.Value->ResourceLease.Get(), InVisit);
		}
		Plan.VisitHydrologyLeases([&](const FVoxelTerrainResourceLease* InLease)
		{
			FVoxelGenerationRequiredSourceScope::VisitLease(InLease, InVisit);
		});
		if (CurrentComponent)
		{
			FVoxelGenerationRequiredSourceScope::VisitLease(CurrentComponent->BodyLease.Get(), InVisit);
			FVoxelGenerationRequiredSourceScope::VisitLease(CurrentComponent->ResidentReservation.Get(), InVisit);
		}
	}
};

struct FVoxelGenerationPlanCache::FBuildGate
{
	FBuildGate()
	{
	}

	~FBuildGate()
	{
	}

	TSharedPtr<FHydrologyRegionOwner, ESPMode::ThreadSafe> HydrologyOwner;
	TSharedPtr<FFineWaterOwner, ESPMode::ThreadSafe> FineWaterOwner;
	bool bAdvancingFineWater = false;
	bool bAdvancingHydrology = false;
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
	for (int32 Index = 0; Index < 6; ++Index)
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

bool FVoxelGenerationPlanCache::FindHydrologyTopology(const FVoxelHydrologyRegionKey& InKey, FVoxelHydrologyPlanPtr& OutPlan) const
{
	const FPlanShard& Shard = *PlanShards[PlanShardIndex(InKey)];
	FReadScopeLock Scope(Shard.Lock);
	if (const auto* Found = Shard.HydrologyTopology.Find(InKey))
	{
		OutPlan = *Found;
		return OutPlan.IsValid();
	}
	return false;
}

bool FVoxelGenerationPlanCache::GetOrBuildHydrologyTopology(
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
			Shard.HydrologyTopology,
			Shard.HydrologyTopologyBuilds,
			Shard.HydrologyTopologyKeys,
			Shard.HydrologyTopologyBacking,
			ControlBackingCapacity,
			*this,
			Resources,
			ControlConsumer,
			InKey,
			InBuild,
			OutPlan,
			OutError,
			InCancel,
			bAllowGameThreadBuilds,
			bAllowWorkerWait,
			[this](const uint64 InWait)
			{
				RecordGateWait(InWait);
			});
}

bool FVoxelGenerationPlanCache::BindHydrologyRecipe(uint64 InRecipeHash, uint32 InAlgorithmVersion, FString& OutError, uint64 InExpectedGeneration)
{
	FWriteScopeLock Scope(RetentionLock);
	if (InAlgorithmVersion == 0 || InAlgorithmVersion > FVoxelGenerationRecipe::CurrentAlgorithmVersion)
	{
		OutError = TEXT("InvalidRecipe: unsupported hydrology algorithm version");
		return false;
	}
	if (InExpectedGeneration && InExpectedGeneration != HydrologyGeneration.Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	if (bHydrologyResetting.Load())
	{
		OutError = TEXT("Yielded: hydrology cache invalidation in progress");
		return false;
	}
	if (HydrologyAlgorithmVersion == 0)
	{
		HydrologyRecipeHash = InRecipeHash;
		HydrologyAlgorithmVersion = InAlgorithmVersion;
	}
	if (HydrologyRecipeHash != InRecipeHash || HydrologyAlgorithmVersion != InAlgorithmVersion)
	{
		OutError = TEXT("InvalidRecipe: recipe change requires a new cache and world generation");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FVoxelGenerationPlanCache::AdvanceHydrologyPlan(const FVoxelHydrologyRegionKey& InKey,
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
	TSharedRef<const FVoxelClimateGenerator, ESPMode::ThreadSafe> InClimate,
	TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> InTerrain,
	FVoxelHydrologyPlanPtr& OutPlan, FString& OutError, const TAtomic<bool>* InCancel)
{
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	if (bHydrologyResetting.Load())
	{
		OutError = TEXT("Yielded: hydrology cache invalidation in progress");
		return false;
	}
	const uint64 Generation = HydrologyGeneration.Load();
	if (!BindHydrologyRecipe(InConfig->Recipe->RecipeHash, InConfig->Recipe->AlgorithmVersion, OutError, Generation)) return false;
	if (FindHydrology(InKey, OutPlan))
	{
		if (Generation != HydrologyGeneration.Load())
		{
			OutPlan.Reset();
			OutError = TEXT("Canceled");
			return false;
		}
		OutError.Reset();
		return true;
	}
	if (!bAllowGameThreadBuilds && IsInGameThread())
	{
		OutError = TEXT("DependencyNotReady: Hydrology");
		return false;
	}
	FVoxelGenerationConsumerKey Consumer;
	if (Resources && !ResolveBuildConsumer(ControlConsumer, Consumer, OutError)) return false;
	if (!Resources) Consumer = ControlConsumer;
	FVoxelGenerationRequiredSourceScope CacheSources(Resources.Get(), this, EnumerateCacheRequiredSources);
	struct FHydrologyOutputSources
	{
		const FVoxelHydrologyPlanPtr& Plan;
		const FVoxelTerrainResources* Resources;
	};
	const FHydrologyOutputSources OutputSources{OutPlan, Resources.Get()};
	FVoxelGenerationRequiredSourceScope PreviousOutput(Resources.Get(), &OutputSources,
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			const auto& Source = *static_cast<const FHydrologyOutputSources*>(InContext);
			if (Source.Plan)
			{
				Source.Plan->VisitHydrologyLeases([&](const FVoxelTerrainResourceLease* InLease)
				{
					if (Source.Resources && InLease && InLease->IsOwnedBy(*Source.Resources))
						FVoxelGenerationRequiredSourceScope::VisitLease(InLease, InVisit);
				});
			}
		});

	const auto AcquireBody = [&](uint64 InBytes,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutWorking,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutResident)
	{
		if (!Resources) return true;
		FVoxelGenerationRequiredCapacity Required;
		if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, OutError)) return false;
		const auto Working = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Working,
			InBytes, OutWorking, 0, Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
		if (Working != EVoxelTerrainResourceAdmission::Granted)
		{
			OutError = Working == EVoxelTerrainResourceAdmission::CapacityBlocked
				? TEXT("CapacityBlocked: hydrology retained owner")
				: TEXT("CapacityUnachievable: hydrology retained owner and required sources");
			return false;
		}
		const auto Resident = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Resident,
			InBytes, OutResident, 0, Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount + 1);
		if (Resident != EVoxelTerrainResourceAdmission::Granted)
		{
			OutError = Resident == EVoxelTerrainResourceAdmission::CapacityBlocked
				? TEXT("CapacityBlocked: hydrology owner resident reservation")
				: TEXT("CapacityUnachievable: hydrology owner resident reservation and required sources");
			return false;
		}
		return true;
	};

	FPlanShard& Shard = *PlanShards[PlanShardIndex(InKey)];
	TSharedPtr<FBuildGate, ESPMode::ThreadSafe> Gate;
	{
		FWriteScopeLock Scope(Shard.Lock);
		if (Generation != HydrologyGeneration.Load() || bHydrologyResetting.Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		if (const auto* Existing = Shard.Hydrology.Find(InKey))
		{
			OutPlan = *Existing;
			OutError.Reset();
			return true;
		}
		if (const auto* Existing = Shard.HydrologyBuilds.Find(InKey))
		{
			Gate = *Existing;
			if (!Gate->HydrologyOwner)
			{
				OutError = TEXT("InvalidRecipe: legacy hydrology build occupies the new-version key");
				return false;
			}
			if (Gate->bAdvancingHydrology)
			{
				OutError = TEXT("Yielded: hydrology component has an active worker");
				return false;
			}
		}
		else
		{
			if (!PrepareControlStorage(Shard.Hydrology, Shard.HydrologyBuilds, Shard.HydrologyKeys,
				Shard.HydrologyBacking, ControlBackingCapacity, 1, 1, Resources, ControlConsumer, OutError)) return false;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> GateLease;
			FVoxelGenerationRequiredCapacity Required;
			if (Resources)
			{
				if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, OutError)) return false;
				const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input,
					VoxelBoundedAllocation::WorkerSharedBytes<FBuildGate>(), GateLease, 0,
					Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
				if (Admission != EVoxelTerrainResourceAdmission::Granted)
				{
					OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked
						? TEXT("CapacityBlocked: hydrology owner gate") : TEXT("CapacityUnachievable: hydrology owner gate");
					return false;
				}
			}
			FVoxelGenerationRequiredSourceScope GateSource(Resources.Get(), GateLease.Get());
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Resident;
			if (!AcquireBody(VoxelBoundedAllocation::WorkerSharedBytes<FHydrologyRegionOwner>(), Working, Resident)) return false;
			Gate = GateLease ? VoxelBoundedAllocation::MakeWorkerShared<FBuildGate>(GateLease) :
				MakeShared<FBuildGate, ESPMode::ThreadSafe>();
			Gate->HydrologyOwner = Working ? VoxelBoundedAllocation::MakeWorkerShared<FHydrologyRegionOwner>(Working,
				InConfig, InClimate, InTerrain, InKey, Consumer, Generation) :
				MakeShared<FHydrologyRegionOwner, ESPMode::ThreadSafe>(InConfig, InClimate, InTerrain, InKey, Consumer, Generation);
			Gate->HydrologyOwner->BodyLease = MoveTemp(Working);
			Gate->HydrologyOwner->ResidentReservation = MoveTemp(Resident);
			Shard.HydrologyBuilds.Add(InKey, Gate);
		}
		Gate->bAdvancingHydrology = true;
	}

	const auto Owner = Gate->HydrologyOwner;
	const FVoxelGenerationRequiredSource GateRoot{Gate.Get(), 0, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FBuildGate>()};
	FVoxelGenerationRequiredSourceScope GateSource(Resources.Get(), &GateRoot,
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			InVisit(*static_cast<const FVoxelGenerationRequiredSource*>(InContext));
		});
	FVoxelGenerationRequiredSourceScope OwnerSources(Resources.Get(), Owner.Get(),
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			static_cast<const FHydrologyRegionOwner*>(InContext)->VisitSources(InVisit);
		});
	FVoxelGenerationAllocationScope RegionAllocation(Resources, Owner->Consumer, Owner->BodyLease,
		Owner->ResidentReservation, VoxelBoundedAllocation::WorkerSharedBytes<FHydrologyRegionOwner>(), Owner->GetAllocatedBytes());
	bool bTerminal = false;
	ON_SCOPE_EXIT
	{
		FWriteScopeLock Scope(Shard.Lock);
		Gate->bAdvancingHydrology = false;
		if (bTerminal)
		{
			const auto* Current = Shard.HydrologyBuilds.Find(InKey);
			if (Current && *Current == Gate)
			{
				CompleteBuildGate(*Gate, nullptr, OutError);
				Shard.HydrologyBuilds.Remove(InKey);
			}
		}
	};
	const auto Stop = [&](const FString& InError)
	{
		OutError = InError;
		bTerminal = !OutError.StartsWith(TEXT("CapacityBlocked")) &&
			!OutError.StartsWith(TEXT("Yielded"));
		return false;
	};
	if (!RegionAllocation.Check(OutError)) return Stop(OutError);
	if (Owner->bCanceled.Load() || Owner->Generation != HydrologyGeneration.Load()) return Stop(TEXT("Canceled"));

	// 缺失拓扑一次只读或构建一张；旧构建器从不经过正式 Region Query。
	if (Owner->MissingIndex < Owner->MissingTiles.Num())
	{
		const auto Missing = Owner->MissingTiles[Owner->MissingIndex];
		if (!Owner->Tiles.Contains(Missing))
		{
			if (Owner->Tiles.Num() == MAX_int32) return Stop(TEXT("Oversized: hydrology topology tile count"));
			if (Owner->Tiles.Num() + 1 > Owner->TileCapacity)
			{
				const int32 Capacity = int32(FMath::Min<int64>(MAX_int32,
					FMath::Max<int64>(Owner->Tiles.Num() + 1, FMath::Max<int64>(4, int64(Owner->TileCapacity) * 2))));
				if (!VoxelGenerationAllocation::RetainMap(Owner->Tiles, Capacity, OutError)) return Stop(OutError);
				Owner->TileCapacity = Capacity;
			}
			FVoxelHydrologyPlanPtr Tile;
			FVoxelHydrologyGenerator Legacy(Owner->Config->Recipe.ToSharedRef(), Owner->Terrain.ToSharedRef());
			if (!GetOrBuildHydrologyTopology(Missing,
				[&](FVoxelHydrologyPlan& InPlan, FString& InError)
				{
					return Legacy.BuildPlan(Missing, InPlan, InError, InCancel) && InPlan.IndexSelectedRiverCells(InError);
				}, Tile, OutError, InCancel))
			{
				if (OutError.StartsWith(TEXT("DependencyNotReady"))) OutError = TEXT("Yielded: hydrology topology tile is building");
				return Stop(OutError);
			}
			Owner->Tiles.Add(Missing, MoveTemp(Tile));
		}
		++Owner->MissingIndex;
		return Stop(TEXT("Yielded: hydrology topology input advanced"));
	}
	const auto Lookup = [&](const FVoxelHydrologyRegionKey& InTile) -> const FVoxelHydrologyPlan*
	{
		const auto* Found = Owner->Tiles.Find(InTile);
		return Found ? Found->Get() : nullptr;
	};
	if (!Owner->bTopologyReady)
	{


		const auto& Settings = Owner->Config->Recipe->Settings;
		const int64 Side = FVoxelGenerationQuery::HydrologyKeySide(Settings, Owner->Config->Recipe->AlgorithmVersion);
		const int64 X = int64(InKey.Coordinate.X) * Side, Y = int64(InKey.Coordinate.Y) * Side;
		const int64 MinX = FMath::Max<int64>(VoxelGeneration::WorldCellMin, X);
		const int64 MinY = FMath::Max<int64>(VoxelGeneration::WorldCellMin, Y);
		const int64 MaxX = FMath::Min<int64>(VoxelGeneration::WorldCellMaxExclusive, X + Side);
		const int64 MaxY = FMath::Min<int64>(VoxelGeneration::WorldCellMaxExclusive, Y + Side);
		if (MinX >= MaxX || MinY >= MaxY) return Stop(TEXT("Invalid hydrology coverage outside world cell domain"));
		const auto First = FVoxelGenerationQuery::HydrologyTopologyKeyForVoxel(int32(MinX), int32(MinY), Settings).Coordinate;
		const auto Last = FVoxelGenerationQuery::HydrologyTopologyKeyForVoxel(int32(MaxX - 1), int32(MaxY - 1), Settings).Coordinate;
		const int64 Count = (int64(Last.X) - First.X + 1) * (int64(Last.Y) - First.Y + 1);
		if (Count <= 0 || Count > MAX_int32) return Stop(TEXT("Oversized: hydrology coverage topology count"));
		for (int64 TY = First.Y; TY <= Last.Y; ++TY)
			for (int64 TX = First.X; TX <= Last.X; ++TX)
			{
				const FVoxelHydrologyRegionKey TopologyKey{FIntPoint(int32(TX), int32(TY))};
				if (Owner->Tiles.Contains(TopologyKey)) continue;
				if (!VoxelGenerationAllocation::RetainArray(Owner->MissingTiles, 1, OutError)) return Stop(OutError);
				Owner->MissingTiles.Reset();
				Owner->MissingTiles.Add(TopologyKey);
				Owner->MissingIndex = 0;
				return Stop(TEXT("Yielded: hydrology coverage requires legacy topology"));
			}
		if (!VoxelGenerationAllocation::RetainArray(Owner->Plan.TopologyPlans, int32(Count), OutError)) return Stop(OutError);
		Owner->Plan.TopologyPlans.Reset();
		for (int64 TY = First.Y; TY <= Last.Y; ++TY)
			for (int64 TX = First.X; TX <= Last.X; ++TX)
				Owner->Plan.TopologyPlans.Add(Owner->Tiles.FindChecked(FVoxelHydrologyRegionKey{FIntPoint(int32(TX), int32(TY))}));
		Owner->Plan.TopologyPlan = Owner->Plan.TopologyPlans[0];
		Owner->bTopologyReady = true;
	}
	if (!Owner->bSeedsReady)
	{
		Owner->MissingTiles.Reset();
		Owner->MissingIndex = 0;
		if (!FVoxelHydrologyComponentBuild::CollectCoverageSeeds(InKey, Owner->Config->Recipe->Settings,
			Lookup, Owner->Seeds, Owner->MissingTiles, OutError))
		{
			if (!Owner->MissingTiles.IsEmpty()) return Stop(TEXT("Yielded: hydrology coverage needs topology inputs"));
			return Stop(OutError);
		}
		Owner->bSeedsReady = true;
		return Stop(TEXT("Yielded: hydrology coverage seeds collected"));
	}
	if (!Owner->CurrentComponent)
	{
		int32 Skipped = 0;
		while (Owner->SeedIndex < Owner->Seeds.Num() && Skipped < 256)
		{
			const FIntPoint Seed = Owner->Seeds[Owner->SeedIndex];
			const bool bCovered = Owner->Plan.RiverComponents.ContainsByPredicate([&](const auto& Component)
			{
				return Component->ContainsCell(Seed);
			});
			if (!bCovered) break;
			++Owner->SeedIndex;
			++Skipped;
		}
		if (Skipped == 256) return Stop(TEXT("Yielded: hydrology region advances covered drainage cells"));
	}
	if (Owner->SeedIndex < Owner->Seeds.Num())
	{
		if (!Owner->CurrentComponent)
		{
			const FIntPoint Seed = Owner->Seeds[Owner->SeedIndex];
			const int64 CellSize = Owner->Config->Recipe->Settings.HydrologyCellSize;
			const int64 Side = FVoxelGenerationQuery::HydrologyKeySide(Owner->Config->Recipe->Settings, Owner->Config->Recipe->AlgorithmVersion);
			const FIntPoint SeedRegion(int32(VoxelGeneration::FloorDivideWide(int64(Seed.X) * CellSize, Side)),
				int32(VoxelGeneration::FloorDivideWide(int64(Seed.Y) * CellSize, Side)));
			for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
			{
				for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
				{
					const int64 RegionX = int64(SeedRegion.X) + OffsetX;
					const int64 RegionY = int64(SeedRegion.Y) + OffsetY;
					if (RegionX < MIN_int32 || RegionY < MIN_int32 || RegionX > MAX_int32 || RegionY > MAX_int32) continue;
					FVoxelHydrologyPlanPtr Neighbor;
					if (!FindHydrology(FVoxelHydrologyRegionKey{FIntPoint(int32(RegionX), int32(RegionY))}, Neighbor)) continue;
					FVoxelGenerationRequiredSourceScope NeighborSources(Resources.Get(), Neighbor.Get(),
						[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
						{
							static_cast<const FVoxelHydrologyPlan*>(InContext)->VisitHydrologyLeases([&](const FVoxelTerrainResourceLease* InLease)
							{
								FVoxelGenerationRequiredSourceScope::VisitLease(InLease, InVisit);
							});
						});
					for (const auto& Component : Neighbor->RiverComponents)
					{
						if (!Component->ContainsCell(Seed)) continue;
						if (!VoxelGenerationAllocation::RetainArray(Owner->Plan.RiverComponents,
							Owner->Plan.RiverComponents.Num() + 1, OutError)) return Stop(OutError);
						Owner->Plan.RiverComponents.Add(Component);
						++Owner->SeedIndex;
						return Stop(TEXT("Yielded: hydrology region reused an adjacent immutable component"));
					}
				}
			}
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Resident;
			if (!AcquireBody(VoxelBoundedAllocation::WorkerSharedBytes<FHydrologyComponentOwner>(), Working, Resident)) return Stop(OutError);
			Owner->CurrentComponent = Working ? VoxelBoundedAllocation::MakeWorkerShared<FHydrologyComponentOwner>(Working,
				Owner->Config->Recipe.ToSharedRef(), Owner->Terrain.ToSharedRef(), Owner->Seeds[Owner->SeedIndex]) :
				MakeShared<FHydrologyComponentOwner, ESPMode::ThreadSafe>(Owner->Config->Recipe.ToSharedRef(), Owner->Terrain.ToSharedRef(), Owner->Seeds[Owner->SeedIndex]);
			Owner->CurrentComponent->BodyLease = MoveTemp(Working);
			Owner->CurrentComponent->ResidentReservation = MoveTemp(Resident);
		}
		const auto ComponentOwner = Owner->CurrentComponent;
		EVoxelHydrologyComponentStep Step = EVoxelHydrologyComponentStep::Ready;
		{
			FVoxelGenerationAllocationScope ComponentAllocation(Resources, Owner->Consumer, ComponentOwner->BodyLease,
				ComponentOwner->ResidentReservation, VoxelBoundedAllocation::WorkerSharedBytes<FHydrologyComponentOwner>(),
				ComponentOwner->GetAllocatedBytes());
			if (!ComponentAllocation.Check(OutError)) return Stop(OutError);
			if (!ComponentOwner->bReady)
			{
				Step = ComponentOwner->Build->Advance(Lookup, 256, OutError, InCancel);
				if (Step == EVoxelHydrologyComponentStep::Ready)
				{
					const uint64 Before = ComponentOwner->GetAllocatedBytes();
					ComponentOwner->Result = ComponentOwner->Build->TakeResult();
					ComponentOwner->Build.Reset();
					ComponentAllocation.ReleaseRetainedBytes(Before - ComponentOwner->GetAllocatedBytes());
					ComponentOwner->bReady = true;
				}
			}
			if (!ComponentAllocation.ValidateRetainedBytes(ComponentOwner->GetAllocatedBytes(), OutError)) return Stop(OutError);
		}
		if (Step == EVoxelHydrologyComponentStep::NeedTiles)
		{
			const auto& Missing = ComponentOwner->Build->GetMissingTiles();
			if (Missing.IsEmpty()) return Stop(TEXT("HydrologyConflict: component requested no actionable topology tile"));
			if (!VoxelGenerationAllocation::RetainArray(Owner->MissingTiles, Missing.Num(), OutError)) return Stop(OutError);
			Owner->MissingTiles.Reset();
			Owner->MissingTiles.Append(Missing);
			Owner->MissingIndex = 0;
			return Stop(TEXT("Yielded: hydrology component needs topology inputs"));
		}
		if (Step == EVoxelHydrologyComponentStep::Yielded) return Stop(TEXT("Yielded: hydrology component closure"));
		if (Step == EVoxelHydrologyComponentStep::Canceled) return Stop(TEXT("Canceled"));
		if (Step != EVoxelHydrologyComponentStep::Ready)
		{
			return Stop(OutError.IsEmpty() ? TEXT("HydrologyConflict: hydrology component did not produce a complete result") : OutError);
		}
		if (!VoxelGenerationAllocation::RetainArray(Owner->Plan.RiverComponents,
			Owner->Plan.RiverComponents.Num() + 1, OutError)) return Stop(OutError);
		const FIntPoint ComponentKey = ComponentOwner->Result.Key;
		FPlanShard& ComponentShard = *PlanShards[GetTypeHash(ComponentKey) % ShardCount];
		TSharedPtr<const FVoxelHydrologyRiverComponent, ESPMode::ThreadSafe> SharedComponent;
		bool bReusedComponent = false;
		{
			FWriteScopeLock Scope(ComponentShard.Lock);
			if (Owner->bCanceled.Load() || Owner->Generation != HydrologyGeneration.Load() || (InCancel && InCancel->Load())) return Stop(TEXT("Canceled"));
			if (const auto* Published = ComponentShard.RiverComponents.Find(ComponentKey))
			{
				SharedComponent = *Published;
				bReusedComponent = true;
			}
			else
			{
				if (ComponentShard.RiverComponents.Num() >= MaximumControlEntries)
					return Stop(TEXT("Oversized: hydrology component cache entries"));
				if (!PrepareControlMap(ComponentShard.RiverComponents, ComponentShard.RiverComponents.Num() + 1,
					ComponentShard.RiverComponentBacking.ValuesCapacity, ComponentShard.RiverComponentBacking.ValuesLease,
					ControlBackingCapacity, Resources, ControlConsumer, OutError) ||
					!PrepareControlKeys(ComponentShard.RiverComponentKeys, ComponentShard.RiverComponentKeys.Num() + 1,
						ComponentShard.RiverComponentBacking.KeysCapacity, ComponentShard.RiverComponentBacking.KeysLease,
						ControlBackingCapacity, Resources, ControlConsumer, OutError)) return Stop(OutError);
				ComponentOwner->Result.ResourceLease = ComponentOwner->BodyLease;
				if (ComponentOwner->BodyLease && !ComponentOwner->BodyLease->CommitReservation(
					ComponentOwner->ResidentReservation.ToSharedRef(), CacheBodyOwner(ControlConsumer, ComponentKey)))
					return Stop(TEXT("CapacityBlocked: hydrology component publication"));
				SharedComponent = TSharedPtr<const FVoxelHydrologyRiverComponent, ESPMode::ThreadSafe>(ComponentOwner, &ComponentOwner->Result);
				ComponentShard.RiverComponents.Add(ComponentKey, SharedComponent);
				ComponentShard.RiverComponentKeys.Add(ComponentKey);
				if (ComponentOwner->BodyLease) ComponentOwner->BodyLease->MarkPublished();
			}
		}


		if (bReusedComponent && !SharedComponent->HasEquivalentOutput(ComponentOwner->Result))
			return Stop(TEXT("HydrologyConflict: canonical component differs across region requests"));
		if (!Owner->Plan.RiverComponents.Contains(SharedComponent)) Owner->Plan.RiverComponents.Add(SharedComponent);
		Owner->CurrentComponent.Reset();
		++Owner->SeedIndex;
		return Stop(TEXT("Yielded: hydrology component committed"));
	}

	if (!Owner->bReady)
	{
		Owner->Plan.RiverComponents.Sort([](const auto& Left, const auto& Right)
		{
			return Left->Key.X != Right->Key.X ? Left->Key.X < Right->Key.X : Left->Key.Y < Right->Key.Y;
		});

		const auto& Settings = Owner->Config->Recipe->Settings;
		auto OwnsFall = [&](const FVoxelWaterfallSemantic& Fall)
		{
			return FVoxelGenerationQuery::HydrologyKeyForVoxel(Fall.Upstream.X, Fall.Upstream.Y,
				Settings, Owner->Config->Recipe->AlgorithmVersion) == InKey;
		};
		int64 FallCount = 0;
		for (const auto& Component : Owner->Plan.RiverComponents)
		{
			if (InCancel && InCancel->Load()) return Stop(TEXT("Canceled"));
			for (const auto& Fall : Component->Plan.Waterfalls) FallCount += OwnsFall(Fall) ? 1 : 0;
		}
		if (FallCount > MAX_int32) return Stop(TEXT("Oversized: hydrology region waterfall semantics"));
		if (!VoxelGenerationAllocation::RetainArray(Owner->Plan.Waterfalls, int32(FallCount), OutError)) return Stop(OutError);
		Owner->Plan.Waterfalls.Reset();
		for (const auto& Component : Owner->Plan.RiverComponents)
		{
			if (InCancel && InCancel->Load()) return Stop(TEXT("Canceled"));
			for (const auto& Fall : Component->Plan.Waterfalls)
				if (OwnsFall(Fall)) Owner->Plan.Waterfalls.Add(Fall);
		}
		VoxelGenerationAllocation::FreeRetainedArray(Owner->Seeds);
		VoxelGenerationAllocation::FreeRetainedArray(Owner->MissingTiles);
		VoxelGenerationAllocation::FreeRetainedMap(Owner->Tiles);
		Owner->Config.Reset();
		Owner->Climate.Reset();
		Owner->Terrain.Reset();
		Owner->bReady = true;
	}
	if (!RegionAllocation.ValidateRetainedBytes(Owner->GetAllocatedBytes(), OutError)) return Stop(OutError);
	{
		FWriteScopeLock Scope(Shard.Lock);
		const auto* Current = Shard.HydrologyBuilds.Find(InKey);
		if (!Current || *Current != Gate || Gate->bCompleted || Owner->bCanceled.Load() || Owner->Generation != HydrologyGeneration.Load() ||
			(InCancel && InCancel->Load())) return Stop(TEXT("Canceled"));
		Owner->Plan.ResourceLease = Owner->BodyLease;
		if (Owner->BodyLease && !Owner->BodyLease->CommitReservation(Owner->ResidentReservation.ToSharedRef(),
			CacheBodyOwner(ControlConsumer, InKey))) return Stop(TEXT("CapacityBlocked: hydrology region publication"));
		const FVoxelHydrologyPlanPtr Published(Owner, &Owner->Plan);
		check(Shard.Hydrology.Num() < Shard.HydrologyBacking.ValuesCapacity);
		check(Shard.HydrologyKeys.Num() < Shard.HydrologyBacking.KeysCapacity);
		Shard.Hydrology.Add(InKey, Published);
		Shard.HydrologyKeys.Add(InKey);
		if (Owner->BodyLease) Owner->BodyLease->MarkPublished();
		CompleteBuildGate(*Gate, Published, FString());
		Shard.HydrologyBuilds.Remove(InKey);
		OutPlan = Published;
	}
	OutError.Reset();
	return true;
}

bool FVoxelGenerationPlanCache::FindFineWater(const FIntPoint& InCell, FVoxelHydrologyFinePlanPtr& OutPlan) const
{
	OutPlan.Reset();
	{
		const auto& Shard = *PlanShards[NaturalShardIndex(InCell)];
		FReadScopeLock Lock(Shard.Lock);
		if (const auto* Found = Shard.FineWater.Find(InCell)) { OutPlan = *Found; return true; }
	}

    for (const auto& Shard : PlanShards)
    {
        FReadScopeLock Lock(Shard->Lock);
        for (int32 Level = 6; Level <= 32; ++Level)
        {
            if (!(Shard->FineWaterIndexLevels & (uint32(1) << (Level - 6)))) continue;
            const int64 Side = int64(1) << Level;
            const FIntVector Bucket(int32(VoxelGeneration::FloorDivideWide(InCell.X, Side)),
                int32(VoxelGeneration::FloorDivideWide(InCell.Y, Side)), Level);
            auto* const* Head = Shard->FineWaterIndex.Find(Bucket);
            for (const auto* Link = Head ? *Head : nullptr; Link; Link = Link->Next)
            {
                const auto& Plan = *Link->Plan;
                if (InCell.X < Plan.IndexMinimum.X || InCell.Y < Plan.IndexMinimum.Y ||
                    InCell.X > Plan.IndexMaximum.X || InCell.Y > Plan.IndexMaximum.Y) continue;
                if (Plan.Contains(InCell))
                { OutPlan = Shard->FineWater.FindChecked(Plan.Key); return true; }
            }
        }
    }
	return false;
}

bool FVoxelGenerationPlanCache::AdvanceFineWater(const FIntPoint& InKey,
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
	TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> InTerrain,
	FVoxelHydrologyFinePlanPtr& OutPlan, FString& OutError, const TAtomic<bool>* InCancel)
{
	if (InConfig->Recipe->AlgorithmVersion <= FVoxelGenerationRecipe::LegacyHydrologyAlgorithmVersion)
	{ OutError = TEXT("InvalidRecipe: fine water certificates require v14"); return false; }
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	if (bHydrologyResetting.Load())
	{
		OutError = TEXT("Yielded: hydrology cache invalidation in progress");
		return false;
	}
	if (!VoxelGeneration::IsWorldCell(InKey.X) || !VoxelGeneration::IsWorldCell(InKey.Y))
	{ OutError = TEXT("Invalid fine water cell outside physical world"); return false; }
	const uint64 Generation = HydrologyGeneration.Load();
	if (!BindHydrologyRecipe(InConfig->Recipe->RecipeHash, InConfig->Recipe->AlgorithmVersion, OutError, Generation)) return false;
	if (FindFineWater(InKey, OutPlan))
	{
		if (Generation != HydrologyGeneration.Load())
		{ OutPlan.Reset(); OutError = TEXT("Canceled"); return false; }


		TSharedPtr<FBuildGate, ESPMode::ThreadSafe> Retired;
		{
			auto& Input = *PlanShards[NaturalShardIndex(InKey)];
			FWriteScopeLock Lock(Input.Lock);
			if (Generation != HydrologyGeneration.Load() || bHydrologyResetting.Load() || (InCancel && InCancel->Load()))
			{ OutError = TEXT("Canceled"); return false; }
			if (const auto* Existing = Input.FineWaterBuilds.Find(InKey))
			{
				Retired = *Existing;
				if (Retired->FineWaterOwner) Retired->FineWaterOwner->bCanceled.Store(true);
				CompleteBuildGate(*Retired, OutPlan, FString()); Input.FineWaterBuilds.Remove(InKey);
			}
		}
		OutError.Reset(); return true;
	}
	if (!bAllowGameThreadBuilds && IsInGameThread())
	{
		OutError = TEXT("DependencyNotReady: Hydrology");
		return false;
	}
	FVoxelGenerationConsumerKey Consumer;
	if (Resources && !ResolveBuildConsumer(ControlConsumer, Consumer, OutError)) return false;
	if (!Resources) Consumer = ControlConsumer;
	FVoxelGenerationRequiredSourceScope CacheSources(Resources.Get(), this, EnumerateCacheRequiredSources);
	struct FHydrologyOutputSources
	{
		const FVoxelHydrologyFinePlanPtr& Plan;
		const FVoxelTerrainResources* Resources;
	};
	const FHydrologyOutputSources OutputSources{OutPlan, Resources.Get()};
	FVoxelGenerationRequiredSourceScope PreviousOutput(Resources.Get(), &OutputSources,
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			const auto& Source = *static_cast<const FHydrologyOutputSources*>(InContext);
			if (Source.Plan)
			{
				FVoxelGenerationRequiredSourceScope::VisitLease(Source.Plan->ResourceLease.Get(), InVisit);
			}
		});

	const auto AcquireBody = [&](uint64 InBytes,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutWorking,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutResident)
	{
		if (!Resources) return true;
		FVoxelGenerationRequiredCapacity Required;
		if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, OutError)) return false;
		const auto Working = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Working,
			InBytes, OutWorking, 0, Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
		if (Working != EVoxelTerrainResourceAdmission::Granted)
		{
			OutError = Working == EVoxelTerrainResourceAdmission::CapacityBlocked
				? TEXT("CapacityBlocked: hydrology retained owner")
				: TEXT("CapacityUnachievable: hydrology retained owner and required sources");
			return false;
		}
		const auto Resident = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Resident,
			InBytes, OutResident, 0, Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount + 1);
		if (Resident != EVoxelTerrainResourceAdmission::Granted)
		{
			OutError = Resident == EVoxelTerrainResourceAdmission::CapacityBlocked
				? TEXT("CapacityBlocked: hydrology owner resident reservation")
				: TEXT("CapacityUnachievable: hydrology owner resident reservation and required sources");
			return false;
		}
		return true;
	};

	FPlanShard& Shard = *PlanShards[NaturalShardIndex(InKey)];
	TSharedPtr<FBuildGate, ESPMode::ThreadSafe> Gate;
	{
		FWriteScopeLock Scope(Shard.Lock);
		if (Generation != HydrologyGeneration.Load() || bHydrologyResetting.Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		if (const auto* Existing = Shard.FineWater.Find(InKey))
		{
			OutPlan = *Existing;
			OutError.Reset();
			return true;
		}
		if (const auto* Existing = Shard.FineWaterBuilds.Find(InKey))
		{
			Gate = *Existing;
			if (!Gate->FineWaterOwner)
			{
				OutError = TEXT("InvalidRecipe: legacy hydrology build occupies the new-version key");
				return false;
			}
			if (Gate->bAdvancingFineWater)
			{
				OutError = TEXT("Yielded: hydrology component has an active worker");
				return false;
			}
		}
		else
		{
			if (!PrepareControlStorage(Shard.FineWater, Shard.FineWaterBuilds, Shard.FineWaterKeys,
				Shard.FineWaterBacking, ControlBackingCapacity, 1, 1, Resources, ControlConsumer, OutError)) return false;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> GateLease;
			FVoxelGenerationRequiredCapacity Required;
			if (Resources)
			{
				if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, OutError)) return false;
				const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input,
					VoxelBoundedAllocation::WorkerSharedBytes<FBuildGate>(), GateLease, 0,
					Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
				if (Admission != EVoxelTerrainResourceAdmission::Granted)
				{
					OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked
						? TEXT("CapacityBlocked: hydrology owner gate") : TEXT("CapacityUnachievable: hydrology owner gate");
					return false;
				}
			}
			FVoxelGenerationRequiredSourceScope GateSource(Resources.Get(), GateLease.Get());
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Resident;
			if (!AcquireBody(VoxelBoundedAllocation::WorkerSharedBytes<FFineWaterOwner>(), Working, Resident)) return false;
			Gate = GateLease ? VoxelBoundedAllocation::MakeWorkerShared<FBuildGate>(GateLease) :
				MakeShared<FBuildGate, ESPMode::ThreadSafe>();
			Gate->FineWaterOwner = Working ? VoxelBoundedAllocation::MakeWorkerShared<FFineWaterOwner>(Working,
				InConfig, InTerrain, InKey, Consumer, Generation) :
				MakeShared<FFineWaterOwner, ESPMode::ThreadSafe>(InConfig, InTerrain, InKey, Consumer, Generation);
			Gate->FineWaterOwner->BodyLease = MoveTemp(Working);
			Gate->FineWaterOwner->ResidentReservation = MoveTemp(Resident);
			Shard.FineWaterBuilds.Add(InKey, Gate);
		}
		Gate->bAdvancingFineWater = true;
	}

	const auto Owner = Gate->FineWaterOwner;
	const FVoxelGenerationRequiredSource GateRoot{Gate.Get(), 0, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FBuildGate>()};
	FVoxelGenerationRequiredSourceScope GateSource(Resources.Get(), &GateRoot,
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			InVisit(*static_cast<const FVoxelGenerationRequiredSource*>(InContext));
		});
	FVoxelGenerationRequiredSourceScope OwnerSources(Resources.Get(), Owner.Get(),
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			static_cast<const FFineWaterOwner*>(InContext)->VisitSources(InVisit);
		});
	FVoxelGenerationAllocationScope FineAllocation(Resources, Owner->Consumer, Owner->BodyLease,
		Owner->ResidentReservation, VoxelBoundedAllocation::WorkerSharedBytes<FFineWaterOwner>(), Owner->GetAllocatedBytes());
	bool bTerminal = false;
	ON_SCOPE_EXIT
	{
		FWriteScopeLock Scope(Shard.Lock);
		Gate->bAdvancingFineWater = false;
		if (bTerminal)
		{
			const auto* Current = Shard.FineWaterBuilds.Find(InKey);
			if (Current && *Current == Gate)
			{
				CompleteBuildGate(*Gate, nullptr, OutError);
				Shard.FineWaterBuilds.Remove(InKey);
			}
		}
	};
	const auto Stop = [&](const FString& InError)
	{
		OutError = InError;
		bTerminal = !OutError.StartsWith(TEXT("CapacityBlocked")) &&
			!OutError.StartsWith(TEXT("Yielded"));
		return false;
	};
	if (!FineAllocation.Check(OutError)) return Stop(OutError);
	if (Owner->bCanceled.Load() || Owner->Generation != HydrologyGeneration.Load()) return Stop(TEXT("Canceled"));

	if (!Owner->bReady && Owner->MissingIndex < Owner->MissingTiles.Num())
	{
		const auto Missing = Owner->MissingTiles[Owner->MissingIndex];
		if (!Owner->Tiles.Contains(Missing))
		{
			if (Owner->Tiles.Num() == MAX_int32) return Stop(TEXT("Oversized: fine water input tiles"));
			if (Owner->Tiles.Num() + 1 > Owner->TileCapacity)
			{
				const int32 Capacity = int32(FMath::Min<int64>(MAX_int32,
					FMath::Max<int64>(Owner->Tiles.Num() + 1, FMath::Max<int64>(4, int64(Owner->TileCapacity) * 2))));
				if (!VoxelGenerationAllocation::RetainMap(Owner->Tiles, Capacity, OutError)) return Stop(OutError);
				Owner->TileCapacity = Capacity;
			}
			FVoxelHydrologyPlanPtr Tile;
			FVoxelHydrologyGenerator Legacy(Owner->Config->Recipe.ToSharedRef(), Owner->Terrain.ToSharedRef());
			if (!GetOrBuildHydrologyTopology(Missing, [&](FVoxelHydrologyPlan& Plan, FString& Error)
				{ return Legacy.BuildPlan(Missing, Plan, Error, InCancel) && Plan.IndexSelectedRiverCells(Error); }, Tile, OutError, InCancel))
			{
				if (OutError.StartsWith(TEXT("DependencyNotReady"))) OutError = TEXT("Yielded: fine water topology input building");
				return Stop(OutError);
			}
			Owner->Tiles.Add(Missing, MoveTemp(Tile));
		}
		++Owner->MissingIndex;
		return Stop(TEXT("Yielded: fine water topology input advanced"));
	}
	if (!Owner->bReady && !Owner->bCopying)
	{
		if (!Owner->Reader)
		{
			const uint64 Bytes = FMemory::QuantizeSize(sizeof(FVoxelHydrologyComponentBuild), alignof(FVoxelHydrologyComponentBuild));
			if (!FineAllocation.PrepareBytes(0, Bytes, OutError)) return Stop(OutError);
			Owner->Reader = MakeUnique<FVoxelHydrologyComponentBuild>(Owner->Config->Recipe.ToSharedRef(), Owner->Terrain.ToSharedRef(), FIntPoint::ZeroValue);
			if (!FineAllocation.SettleBytes(0, Bytes, Bytes, OutError)) return Stop(OutError);
		}
		if (!Owner->Query)
		{
			const uint64 Bytes = FMemory::QuantizeSize(sizeof(FVoxelHydrologyFineWaterQuery), alignof(FVoxelHydrologyFineWaterQuery));
			if (!FineAllocation.PrepareBytes(0, Bytes, OutError)) return Stop(OutError);
			Owner->Query = MakeUnique<FVoxelHydrologyFineWaterQuery>();
			if (!FineAllocation.SettleBytes(0, Bytes, Bytes, OutError)) return Stop(OutError);
			if (!Owner->Query->Initialize(InKey,
				FIntPoint(int32(VoxelGeneration::WorldCellMin), int32(VoxelGeneration::WorldCellMin)),
				FIntPoint(int32(VoxelGeneration::WorldCellMaxExclusive - 1), int32(VoxelGeneration::WorldCellMaxExclusive - 1)), OutError)) return Stop(OutError);
		}
		Owner->Reader->BeginFineInputStep(InCancel);
		const auto Lookup = [&](const FVoxelHydrologyRegionKey& Key) -> const FVoxelHydrologyPlan*
		{ const auto* Tile = Owner->Tiles.Find(Key); return Tile ? Tile->Get() : nullptr; };
		const auto Step = Owner->bQueryComplete ? EVoxelHydrologyFineWaterStep::Ready : Owner->Query->Advance([&](const FIntPoint& Cell, FVoxelHydrologySharedCell& Raw, FString& Error)
			{ return Owner->Reader->ReadCertifiedFineCell(Cell, Lookup, Raw, Error); }, 256, OutError, InCancel);
		if (Step == EVoxelHydrologyFineWaterStep::NeedTiles)
		{
			const auto& Missing = Owner->Reader->GetMissingTiles();
			if (Missing.IsEmpty()) return Stop(TEXT("FineWaterConflict: pending fine certificate has no topology request"));
			if (!VoxelGenerationAllocation::RetainArray(Owner->MissingTiles, Missing.Num(), OutError)) return Stop(OutError);
			Owner->MissingTiles.Reset(); Owner->MissingTiles.Append(Missing); Owner->MissingIndex = 0;
			return Stop(TEXT("Yielded: fine water certificate needs topology"));
		}
		if (Step == EVoxelHydrologyFineWaterStep::Yielded) return Stop(TEXT("Yielded: fine water certificate advanced"));
		if (Step == EVoxelHydrologyFineWaterStep::CapacityBlocked) return Stop(OutError.IsEmpty() ? TEXT("CapacityBlocked: fine water certificate") : OutError);
		if (Step == EVoxelHydrologyFineWaterStep::Canceled) return Stop(TEXT("Canceled"));
		if (Step != EVoxelHydrologyFineWaterStep::Ready) return Stop(OutError.IsEmpty() ? TEXT("FineWaterConflict: no complete receiver certificate") : OutError);
		Owner->bQueryComplete = true;


        const auto& Drainage = Owner->Query->GetDrainageCertificate();
        int32 DryBudget = 256;
        while (Owner->DryCopyIndex < Drainage.Num() && DryBudget > 0)
        {
            --DryBudget;
            if (InCancel && InCancel->Load()) return Stop(TEXT("Canceled"));
            const auto& Cell = Drainage[Owner->DryCopyIndex];
            if (Cell.bIncluded && !Cell.IsReceiver() && Cell.GroundZ >= Cell.SpillZ)
            {
                if (Owner->Plan.CertifiedDry.Num() == Owner->DryCapacity)
                {
                    if (Owner->DryCapacity == MAX_int32) return Stop(TEXT("Oversized: certified dry cells"));
                    const int32 Capacity = int32(FMath::Min<int64>(MAX_int32,
                        FMath::Max<int64>(16, int64(Owner->DryCapacity) * 2)));
                    if (!VoxelGenerationAllocation::RetainSet(Owner->Plan.CertifiedDry, Capacity, OutError)) return Stop(OutError);
                    Owner->DryCapacity = Capacity;
                }
                Owner->Plan.CertifiedDry.Add(Cell.Cell);
                if (Owner->Plan.CertifiedDry.Num() == 1)
                    Owner->Plan.IndexMinimum = Owner->Plan.IndexMaximum = InKey;
                Owner->Plan.IndexMinimum.X = FMath::Min(Owner->Plan.IndexMinimum.X, Cell.Cell.X);
                Owner->Plan.IndexMinimum.Y = FMath::Min(Owner->Plan.IndexMinimum.Y, Cell.Cell.Y);
                Owner->Plan.IndexMaximum.X = FMath::Max(Owner->Plan.IndexMaximum.X, Cell.Cell.X);
                Owner->Plan.IndexMaximum.Y = FMath::Max(Owner->Plan.IndexMaximum.Y, Cell.Cell.Y);
            }
            ++Owner->DryCopyIndex;
        }
        if (Owner->DryCopyIndex < Drainage.Num()) return Stop(TEXT("Yielded: certified dry transfer"));
		Owner->Plan.Water = Owner->Query->GetResult();
		Owner->Plan.Key = Owner->Plan.Water.bNaturalWater ? Owner->Plan.Water.NaturalKey.MinimumMember : InKey;
		Owner->Plan.Minimum = Owner->Plan.Maximum = Owner->Plan.Key;
        if (Owner->Plan.CertifiedDry.IsEmpty())
            Owner->Plan.IndexMinimum = Owner->Plan.IndexMaximum = Owner->Plan.Key;


		const uint64 Before = Owner->GetAllocatedBytes();
		Owner->Plan.Members = Owner->Query->TakeCells();
		Owner->Plan.FixedContacts = Owner->Query->TakeFixedContacts();
		Owner->Reader.Reset(); Owner->Query.Reset();
		FineAllocation.ReleaseRetainedBytes(Before - Owner->GetAllocatedBytes());
		VoxelGenerationAllocation::FreeRetainedMap(Owner->Tiles);
		VoxelGenerationAllocation::FreeRetainedArray(Owner->MissingTiles);
		Owner->Config.Reset(); Owner->Terrain.Reset();
		if (Owner->Plan.Water.bNaturalWater && Owner->Plan.Members.IsEmpty())
			return Stop(TEXT("FineWaterConflict: natural certificate has no members"));
		Owner->bCopying = true;
	}
	if (!Owner->bReady)
	{
		int32 Budget = 256;
		while (Owner->CopyIndex < Owner->Plan.Members.Num() && Budget-- > 0)
		{
			if (InCancel && InCancel->Load()) return Stop(TEXT("Canceled"));
			const auto Cell = Owner->Plan.Members[Owner->CopyIndex++];
			Owner->Plan.Minimum.X = FMath::Min(Owner->Plan.Minimum.X, Cell.X);
			Owner->Plan.Minimum.Y = FMath::Min(Owner->Plan.Minimum.Y, Cell.Y);
			Owner->Plan.Maximum.X = FMath::Max(Owner->Plan.Maximum.X, Cell.X);
			Owner->Plan.Maximum.Y = FMath::Max(Owner->Plan.Maximum.Y, Cell.Y);
            Owner->Plan.IndexMinimum.X = FMath::Min(Owner->Plan.IndexMinimum.X, Cell.X);
            Owner->Plan.IndexMinimum.Y = FMath::Min(Owner->Plan.IndexMinimum.Y, Cell.Y);
            Owner->Plan.IndexMaximum.X = FMath::Max(Owner->Plan.IndexMaximum.X, Cell.X);
            Owner->Plan.IndexMaximum.Y = FMath::Max(Owner->Plan.IndexMaximum.Y, Cell.Y);
		}
		if (Owner->CopyIndex < Owner->Plan.Members.Num()) return Stop(TEXT("Yielded: fine water certificate bounds"));
		const int64 Count = Owner->Plan.FixedContacts.Num();
		if (Count > 1 && Owner->ContactSortScratch.IsEmpty())
		{
			if (!VoxelGenerationAllocation::RetainArray(Owner->ContactSortScratch, int32(Count), OutError)) return Stop(OutError);
			Owner->ContactSortScratch.SetNum(int32(Count));
		}


		while (Owner->SortWidth < Count && Budget-- > 0)
		{
			if (InCancel && InCancel->Load()) return Stop(TEXT("Canceled"));
			const int64 Middle = FMath::Min(Owner->SortRun + Owner->SortWidth, Count);
			const int64 End = FMath::Min(Owner->SortRun + 2 * Owner->SortWidth, Count);
			if (!Owner->bMergeStarted)
			{
				Owner->SortLeft = Owner->SortWrite = Owner->SortRun;
				Owner->SortRight = Middle; Owner->bMergeStarted = true;
			}
			bool bLeft = Owner->SortRight >= End;
			if (Owner->SortLeft < Middle && Owner->SortRight < End)
			{
				const auto& A = Owner->Plan.FixedContacts[int32(Owner->SortLeft)].Cell;
				const auto& B = Owner->Plan.FixedContacts[int32(Owner->SortRight)].Cell;
				bLeft = A.X < B.X || (A.X == B.X && A.Y <= B.Y);
			}
			if (Owner->SortLeft >= Middle) bLeft = false;
			const int64 Source = bLeft ? Owner->SortLeft++ : Owner->SortRight++;
			Owner->ContactSortScratch[int32(Owner->SortWrite++)] = Owner->Plan.FixedContacts[int32(Source)];
			if (Owner->SortWrite == End)
			{
				Owner->bMergeStarted = false; Owner->SortRun += 2 * Owner->SortWidth;
				if (Owner->SortRun >= Count)
				{
					Swap(Owner->Plan.FixedContacts, Owner->ContactSortScratch);
					Owner->SortRun = 0; Owner->SortWidth *= 2;
				}
			}
		}
		if (Owner->SortWidth < Count) return Stop(TEXT("Yielded: fine water canonical boundary sort"));
		VoxelGenerationAllocation::FreeRetainedArray(Owner->ContactSortScratch);
		Owner->bReady = true;
	}
	if (!FineAllocation.ValidateRetainedBytes(Owner->GetAllocatedBytes(), OutError)) return Stop(OutError);
	FVoxelHydrologyFinePlanPtr ExistingOutput;
	const int32 InputShardIndex = NaturalShardIndex(InKey);
	const int32 OutputShardIndex = NaturalShardIndex(Owner->Plan.Key);
	FPlanShard& OutputShard = *PlanShards[OutputShardIndex];
	{
		FReadScopeLock Lock(OutputShard.Lock);
		if (const auto* Existing = OutputShard.FineWater.Find(Owner->Plan.Key)) ExistingOutput = *Existing;
	}

	if (ExistingOutput && !ExistingOutput->HasEquivalentOutput(Owner->Plan))
		return Stop(TEXT("FineWaterConflict: independent fine certificates differ"));
	{


		FWriteScopeLock FirstLock(PlanShards[FMath::Min(InputShardIndex, OutputShardIndex)]->Lock);
		TOptional<FWriteScopeLock> SecondLock;
		if (InputShardIndex != OutputShardIndex)
			SecondLock.Emplace(PlanShards[FMath::Max(InputShardIndex, OutputShardIndex)]->Lock);
		const auto* Current = Shard.FineWaterBuilds.Find(InKey);
		if (!Current || *Current != Gate || Gate->bCompleted || Owner->bCanceled.Load() ||
			Owner->Generation != HydrologyGeneration.Load() || bHydrologyResetting.Load() ||
			(InCancel && InCancel->Load())) return Stop(TEXT("Canceled"));
		FVoxelHydrologyFinePlanPtr Published;
		if (const auto* Existing = OutputShard.FineWater.Find(Owner->Plan.Key))
		{
			if (*Existing != ExistingOutput) return Stop(TEXT("Yielded: fine water canonical competitor changed"));
			Published = *Existing;
		}
		else
		{
			if (!PrepareControlStorage(OutputShard.FineWater, OutputShard.FineWaterBuilds, OutputShard.FineWaterKeys,
				OutputShard.FineWaterBacking, ControlBackingCapacity, 1, 0, Resources, ControlConsumer, OutError)) return Stop(OutError);
            if (Owner->Plan.IndexMinimum != Owner->Plan.Key || Owner->Plan.IndexMaximum != Owner->Plan.Key)
            {
                if (OutputShard.FineWaterIndex.Num() > MaximumControlEntries - 4)
                    return Stop(TEXT("Oversized: fine water spatial buckets"));
                if (!PrepareControlMap(OutputShard.FineWaterIndex, OutputShard.FineWaterIndex.Num() + 4,
                    OutputShard.FineWaterIndexCapacity, OutputShard.FineWaterIndexLease, ControlBackingCapacity,
                    Resources, ControlConsumer, OutError)) return Stop(OutError);
            }
			Owner->Plan.ResourceLease = Owner->BodyLease;
			if (Owner->BodyLease && !Owner->BodyLease->CommitReservation(Owner->ResidentReservation.ToSharedRef(),
				CacheBodyOwner(ControlConsumer, Owner->Plan.Key))) return Stop(TEXT("CapacityBlocked: fine water publication"));
			Published = FVoxelHydrologyFinePlanPtr(Owner, &Owner->Plan);
			OutputShard.FineWater.Add(Owner->Plan.Key, Published); OutputShard.FineWaterKeys.Add(Owner->Plan.Key);
            LinkFineWaterIndex(OutputShard, Owner->Plan);
			if (Owner->BodyLease) Owner->BodyLease->MarkPublished();
		}
		CompleteBuildGate(*Gate, Published, FString()); Shard.FineWaterBuilds.Remove(InKey);
		OutPlan = Published;
	}
	OutError.Reset(); return true;
}

void FVoxelGenerationPlanCache::CancelFineWaterBuild(const FIntPoint& InKey,
	uint64 InWorldEpoch, const FGuid& InOwnerId, uint64 InOwnerIncarnation, uint64 InAttemptId)
{
	TSharedPtr<FBuildGate, ESPMode::ThreadSafe> Retired;
	FPlanShard& Shard = *PlanShards[NaturalShardIndex(InKey)];
	{
		FWriteScopeLock Lock(Shard.Lock);
		const auto* Existing = Shard.FineWaterBuilds.Find(InKey);
		if (!Existing || !(*Existing)->FineWaterOwner) return;
		const auto& Consumer = (*Existing)->FineWaterOwner->Consumer;
		if (Consumer.WorldEpoch != InWorldEpoch || Consumer.OwnerId != InOwnerId ||
			Consumer.OwnerIncarnation != InOwnerIncarnation || Consumer.RequestSerial != InAttemptId) return;
		Retired = *Existing; Retired->FineWaterOwner->bCanceled.Store(true);
		CompleteBuildGate(*Retired, nullptr, TEXT("Canceled")); Shard.FineWaterBuilds.Remove(InKey);
	}
}

void FVoxelGenerationPlanCache::CancelHydrologyBuild(const FVoxelHydrologyRegionKey& InKey,
	uint64 InWorldEpoch, const FGuid& InOwnerId, uint64 InOwnerIncarnation, uint64 InAttemptId)
{
	FPlanShard& Shard = *PlanShards[PlanShardIndex(InKey)];
	TSharedPtr<FBuildGate, ESPMode::ThreadSafe> Retired;
	{
		FWriteScopeLock Scope(Shard.Lock);
		const auto* Existing = Shard.HydrologyBuilds.Find(InKey);
		if (!Existing || !(*Existing)->HydrologyOwner) return;
		const auto& Consumer = (*Existing)->HydrologyOwner->Consumer;
		if (Consumer.WorldEpoch != InWorldEpoch || Consumer.OwnerId != InOwnerId ||
			Consumer.OwnerIncarnation != InOwnerIncarnation || Consumer.RequestSerial != InAttemptId) return;
		Retired = *Existing;
		Retired->HydrologyOwner->bCanceled.Store(true);
		CompleteBuildGate(*Retired, nullptr, TEXT("Canceled"));
		Shard.HydrologyBuilds.Remove(InKey);
	}
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
		NaturalHeadroom = FMath::Min(NativeLimit / 4, FMath::Max(NativeLimit / 16,
			uint64(MaximumRetiredEntriesPerGroup * ShardCount) *
			(VoxelBoundedAllocation::WorkerSharedBytes<const FVoxelBaseColumnEntry>() +
				VoxelBoundedAllocation::WorkerSharedBytes<const FVoxelNaturalColumnEntry>())));
		bNaturalCapacityPressure = State.Used[int32(EVoxelTerrainResourcePool::Resident)] >= NativeLimit - NaturalHeadroom;
	}

	int32 RemainingGroups = 10;
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
	int32 FineWaterBudget = TakeGroupBudget();
	int32 TopologyBudget = TakeGroupBudget();
	int32 ComponentBudget = TakeGroupBudget();
	int32 BaseColumnBudget = TakeGroupBudget() + PressureColumnBudget;
	int32 NaturalColumnBudget = TakeGroupBudget() + PressureColumnBudget;
	int32 CaveBudget = TakeGroupBudget();
	int32 EcologyBudget = TakeGroupBudget();
	int32 FeatureBudget = TakeGroupBudget();
	int32 StructureBudget = TakeGroupBudget();
	TRetiredCacheValues<FVoxelBaseColumnEntry> RetiredBaseColumns;
	TRetiredCacheValues<FVoxelNaturalColumnEntry> RetiredNaturalColumns;
	TRetiredCacheValues<FVoxelHydrologyPlan> RetiredHydrology;
	TRetiredCacheValues<FVoxelHydrologyFinePlan> RetiredFineWater;
	TRetiredCacheValues<FVoxelHydrologyPlan> RetiredTopology;
	TRetiredCacheValues<FVoxelHydrologyRiverComponent> RetiredComponents;
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
				const bool bCoverage = HydrologyAlgorithmVersion > FVoxelGenerationRecipe::LegacyHydrologyAlgorithmVersion;
				return TryHydrologyRetentionPosition(InKey,
					bCoverage ? VoxelHydrology::CoverageSide : Retention.HydrologyRegionSide,
					bCoverage ? 1 : Retention.HydrologyCellSize, RegionCenter) && IsRetained(
					RegionCenter,
					&FVoxelGenerationCacheRetentionPoint::HydrologyRadiusCells);
			});

		bRetirementLimited |= !TrimMapBudgeted(Shard.FineWater, Shard.FineWaterKeys,
			Shard.FineWaterCursor, FineWaterBudget, RetiredFineWater,
			[this, &Shard](const FIntPoint& Key)
			{
				const auto* Found = Shard.FineWater.Find(Key);
				if (!Found) return false;
				const auto& Plan = **Found;


				const auto Intersects = [&Plan](const FVoxelGenerationCacheRetentionPoint& Point)
				{
					const int64 Radius = Point.HydrologyRadiusCells;
					return Radius >= 0 && int64(Plan.IndexMinimum.X) <= int64(Point.Center.X) + Radius &&
						int64(Plan.IndexMaximum.X) >= int64(Point.Center.X) - Radius &&
						int64(Plan.IndexMinimum.Y) <= int64(Point.Center.Y) + Radius &&
						int64(Plan.IndexMaximum.Y) >= int64(Point.Center.Y) - Radius;
				};
				for (const auto& Point : Retention.Points) if (Intersects(Point)) return true;
				for (auto* Lease = TaskRetentionHead; Lease; Lease = Lease->Next) if (Intersects(Lease->Point)) return true;
                UnlinkFineWaterIndex(Shard, Plan);
				return false;
			});

		bRetirementLimited |= !TrimMapBudgeted(
			Shard.HydrologyTopology, Shard.HydrologyTopologyKeys, Shard.HydrologyTopologyCursor,
			TopologyBudget, RetiredTopology,
			[](const FVoxelHydrologyRegionKey&) { return true; }, true);

		bRetirementLimited |= !TrimMapBudgeted(
			Shard.RiverComponents, Shard.RiverComponentKeys, Shard.RiverComponentCursor,
			ComponentBudget, RetiredComponents,
			[](const FIntPoint&) { return true; }, true);

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
	RetiredFineWater.Reset();
	RetiredTopology.Reset();
	RetiredComponents.Reset();
	RetiredCaves.Reset();
	RetiredEcology.Reset();
	RetiredFeatures.Reset();
	RetiredStructures.Reset();
}

void FVoxelGenerationPlanCache::Reset()
{
	bHydrologyResetting.Store(true);
	check(HydrologyGeneration.Load() != MAX_uint64);
	++HydrologyGeneration;
	ON_SCOPE_EXIT
	{
		bHydrologyResetting.Store(false);
	};
	{
		FWriteScopeLock RetentionScope(RetentionLock);
		MaintenanceShardCursor = 0;
		// 配方身份属于此 Cache 生命周期；Reset 只清正文，同配方可重建。
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
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetiredFineWaterLease;
            TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetiredFineIndexLease;
            TMap<FIntVector, FVoxelHydrologyFineIndexLink*> RetiredFineIndex;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetiredFineWaterBuildsLease;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetiredHydrologyBuildsLease;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetiredTopologyLease;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetiredComponentsLease;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetiredCaveLease;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetiredEcologyLease;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetiredFeatureLease;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RetiredStructureLease;
			TMap<FVoxelHydrologyRegionKey, FVoxelHydrologyPlanPtr> RetiredHydrology;
			TMap<FIntPoint, FVoxelHydrologyFinePlanPtr> RetiredFineWater;
			TMap<FIntPoint, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> RetiredFineWaterBuilds;
			TMap<FVoxelHydrologyRegionKey, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> RetiredHydrologyBuilds;
			TMap<FVoxelHydrologyRegionKey, FVoxelHydrologyPlanPtr> RetiredTopology;
			TMap<FIntPoint, TSharedPtr<const FVoxelHydrologyRiverComponent, ESPMode::ThreadSafe>> RetiredComponents;
			TMap<FVoxelGenerationTileKey, FVoxelCavePlanPtr> RetiredCaves;
			TMap<FVoxelEcologyTileKey, FVoxelEcologyPlanPtr> RetiredEcology;
			TMap<FVoxelGenerationTileKey, FVoxelFeaturePlanPtr> RetiredFeatures;
			TMap<FVoxelGenerationTileKey, FVoxelStructurePlanPtr> RetiredStructures;
			{
				FWriteScopeLock RetentionScope(RetentionLock);
				FWriteScopeLock Scope(Shard.Lock);
				for (const auto& Pair : Shard.HydrologyBuilds)
				{
					if (Pair.Value->HydrologyOwner) Pair.Value->HydrologyOwner->bCanceled.Store(true);
				}
				for (const auto& Pair : Shard.FineWaterBuilds)
					if (Pair.Value->FineWaterOwner) Pair.Value->FineWaterOwner->bCanceled.Store(true);
				CancelBuildGates(Shard.FineWaterBuilds);
				CancelBuildGates(Shard.HydrologyBuilds);
				CancelBuildGates(Shard.HydrologyTopologyBuilds);
				CancelBuildGates(Shard.CaveBuilds);
				CancelBuildGates(Shard.EcologyBuilds);
				CancelBuildGates(Shard.FeatureBuilds);
				CancelBuildGates(Shard.StructureBuilds);
				RetiredFineWaterBuilds = MoveTemp(Shard.FineWaterBuilds);
				ControlBackingCapacity.Replace(Shard.FineWaterBacking.BuildsLease.Get(), nullptr);
				RetiredFineWaterBuildsLease = MoveTemp(Shard.FineWaterBacking.BuildsLease);
				Shard.FineWaterBacking.BuildsCapacity = 0;
				RetiredHydrologyBuilds = MoveTemp(Shard.HydrologyBuilds);
				ControlBackingCapacity.Replace(Shard.HydrologyBacking.BuildsLease.Get(), nullptr);
				RetiredHydrologyBuildsLease = MoveTemp(Shard.HydrologyBacking.BuildsLease);
				Shard.HydrologyBacking.BuildsCapacity = 0;
				Shard.HydrologyTopologyBuilds.Reset();
				Shard.CaveBuilds.Reset();
				Shard.EcologyBuilds.Reset();
				Shard.FeatureBuilds.Reset();
				Shard.StructureBuilds.Reset();

				RetiredHydrology = MoveTemp(Shard.Hydrology);
				RetiredFineWater = MoveTemp(Shard.FineWater);
                RetiredFineIndex = MoveTemp(Shard.FineWaterIndex);
                ControlBackingCapacity.Replace(Shard.FineWaterIndexLease.Get(), nullptr);
                RetiredFineIndexLease = MoveTemp(Shard.FineWaterIndexLease);
                Shard.FineWaterIndexCapacity = 0; Shard.FineWaterIndexLevels = 0;
				RetiredTopology = MoveTemp(Shard.HydrologyTopology);
				RetiredComponents = MoveTemp(Shard.RiverComponents);
				RetiredCaves = MoveTemp(Shard.Caves);
				RetiredEcology = MoveTemp(Shard.Ecology);
				RetiredFeatures = MoveTemp(Shard.Features);
				RetiredStructures = MoveTemp(Shard.Structures);
				ControlBackingCapacity.Replace(Shard.HydrologyBacking.ValuesLease.Get(), nullptr);
				ControlBackingCapacity.Replace(Shard.FineWaterBacking.ValuesLease.Get(), nullptr);
				ControlBackingCapacity.Replace(Shard.HydrologyTopologyBacking.ValuesLease.Get(), nullptr);
				ControlBackingCapacity.Replace(Shard.RiverComponentBacking.ValuesLease.Get(), nullptr);
				ControlBackingCapacity.Replace(Shard.CaveBacking.ValuesLease.Get(), nullptr);
				ControlBackingCapacity.Replace(Shard.EcologyBacking.ValuesLease.Get(), nullptr);
				ControlBackingCapacity.Replace(Shard.FeatureBacking.ValuesLease.Get(), nullptr);
				ControlBackingCapacity.Replace(Shard.StructureBacking.ValuesLease.Get(), nullptr);
				RetiredHydrologyLease = MoveTemp(Shard.HydrologyBacking.ValuesLease);
				RetiredFineWaterLease = MoveTemp(Shard.FineWaterBacking.ValuesLease);
				RetiredTopologyLease = MoveTemp(Shard.HydrologyTopologyBacking.ValuesLease);
				RetiredComponentsLease = MoveTemp(Shard.RiverComponentBacking.ValuesLease);
				RetiredCaveLease = MoveTemp(Shard.CaveBacking.ValuesLease);
				RetiredEcologyLease = MoveTemp(Shard.EcologyBacking.ValuesLease);
				RetiredFeatureLease = MoveTemp(Shard.FeatureBacking.ValuesLease);
				RetiredStructureLease = MoveTemp(Shard.StructureBacking.ValuesLease);
				Shard.HydrologyBacking.ValuesCapacity = 0;
				Shard.FineWaterBacking.ValuesCapacity = 0;
				Shard.HydrologyTopologyBacking.ValuesCapacity = 0;
				Shard.RiverComponentBacking.ValuesCapacity = 0;
				Shard.CaveBacking.ValuesCapacity = 0;
				Shard.EcologyBacking.ValuesCapacity = 0;
				Shard.FeatureBacking.ValuesCapacity = 0;
				Shard.StructureBacking.ValuesCapacity = 0;
				Shard.HydrologyKeys.Reset();
				Shard.FineWaterKeys.Reset();
				Shard.HydrologyTopologyKeys.Reset();
				Shard.RiverComponentKeys.Reset();
				Shard.CaveKeys.Reset();
				Shard.EcologyKeys.Reset();
				Shard.FeatureKeys.Reset();
				Shard.StructureKeys.Reset();
				Shard.HydrologyCursor = 0;
				Shard.FineWaterCursor = 0;
				Shard.HydrologyTopologyCursor = 0;
				Shard.RiverComponentCursor = 0;
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

			Stats.FineWater += Shard.FineWater.Num();
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
				Shard.FineWater.GetAllocatedSize() + Shard.FineWaterKeys.GetAllocatedSize() + Shard.FineWaterIndex.GetAllocatedSize() +
				Shard.HydrologyTopology.GetAllocatedSize() +
				Shard.HydrologyTopologyKeys.GetAllocatedSize() +
				Shard.RiverComponents.GetAllocatedSize() +
				Shard.RiverComponentKeys.GetAllocatedSize() +
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

			for (const auto& Pair : Shard.RiverComponents)
			{
				if (Pair.Value) Bytes += Pair.Value->GetAllocatedBytes();
			}

			for (const auto& Pair : Shard.FineWater)
				if (Pair.Value) Bytes += VoxelBoundedAllocation::WorkerSharedBytes<FFineWaterOwner>() + Pair.Value->GetAllocatedBytes();
			for (const auto& Pair : Shard.HydrologyTopology)
			{
				if (Pair.Value) Bytes += Pair.Value->GetAllocatedBytes();
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
