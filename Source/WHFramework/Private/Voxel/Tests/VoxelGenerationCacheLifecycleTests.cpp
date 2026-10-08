#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Async/Async.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "Voxel/Task/VoxelTerrainResources.h"

// 只观察引用数，不复制门闩引用；map、owner、waiter 三个持有者证明等待者已进入真实路径。
class FVoxelCacheLifecycleTestAccess
{
public:
#define VOXEL_PLAN_GATE_REFERENCES(Kind, KeyType, MapName) \
	static int32 Kind##References(FVoxelGenerationPlanCache& InCache, const KeyType& InKey) \
	{ \
		auto& Shard = *InCache.PlanShards[InCache.PlanShardIndex(InKey)]; \
		FReadScopeLock Lock(Shard.Lock); \
		const auto* Gate = Shard.MapName.Find(InKey); \
		return Gate ? Gate->GetSharedReferenceCount() : 0; \
	}
	VOXEL_PLAN_GATE_REFERENCES(Hydrology, FVoxelHydrologyRegionKey, HydrologyBuilds)
	VOXEL_PLAN_GATE_REFERENCES(Cave, FVoxelGenerationTileKey, CaveBuilds)
	VOXEL_PLAN_GATE_REFERENCES(Feature, FVoxelGenerationTileKey, FeatureBuilds)
	VOXEL_PLAN_GATE_REFERENCES(Structure, FVoxelGenerationTileKey, StructureBuilds)
	VOXEL_PLAN_GATE_REFERENCES(Ecology, FVoxelEcologyTileKey, EcologyBuilds)
#undef VOXEL_PLAN_GATE_REFERENCES

	static int32 NaturalReferences(FVoxelGenerationPlanCache& InCache, const FIntPoint& InKey)
	{
		auto& Shard = *InCache.NaturalShards[InCache.NaturalShardIndex(InKey)];
		FReadScopeLock Lock(Shard.Lock);
		const auto* Gate = Shard.NaturalColumnBuilds.Find(InKey);
		return Gate ? Gate->GetSharedReferenceCount() : 0;
	}
};

namespace
{
	template<typename ValueType>
	struct TCacheLifecycleFamily;

#define VOXEL_CACHE_LIFECYCLE_FAMILY(Value, Key, Kind, StorePointer) \
	template<> struct TCacheLifecycleFamily<Value> \
	{ \
		using ValueType = Value; \
		using KeyType = Key; \
		using Ptr = TSharedPtr<const ValueType, ESPMode::ThreadSafe>; \
		static const TCHAR* Name() \
		{ \
			return TEXT(#Kind); \
		} \
		static bool Build(FVoxelGenerationPlanCache& InCache, const KeyType& InKey, \
			TFunctionRef<bool(ValueType&, FString&)> InBuild, Ptr& OutValue, FString& OutError, const TAtomic<bool>* InCancel) \
		{ \
			return InCache.GetOrBuild##Kind(InKey, InBuild, OutValue, OutError, InCancel); \
		} \
		static bool Find(FVoxelGenerationPlanCache& InCache, const KeyType& InKey, Ptr& OutValue) \
		{ \
			return InCache.Find##Kind(InKey, OutValue); \
		} \
		static int32 References(FVoxelGenerationPlanCache& InCache, const KeyType& InKey) \
		{ \
			return FVoxelCacheLifecycleTestAccess::Kind##References(InCache, InKey); \
		} \
		static constexpr void (FVoxelGenerationPlanCache::*Store)(const KeyType&, Ptr) = StorePointer; \
	};
	VOXEL_CACHE_LIFECYCLE_FAMILY(FVoxelHydrologyPlan, FVoxelHydrologyRegionKey, Hydrology, &FVoxelGenerationPlanCache::StoreHydrology)
	VOXEL_CACHE_LIFECYCLE_FAMILY(FVoxelCavePlan, FVoxelGenerationTileKey, Cave, &FVoxelGenerationPlanCache::StoreCave)
	VOXEL_CACHE_LIFECYCLE_FAMILY(FVoxelFeaturePlan, FVoxelGenerationTileKey, Feature, &FVoxelGenerationPlanCache::StoreFeature)
	VOXEL_CACHE_LIFECYCLE_FAMILY(FVoxelStructurePlan, FVoxelGenerationTileKey, Structure, &FVoxelGenerationPlanCache::StoreStructure)
	VOXEL_CACHE_LIFECYCLE_FAMILY(FVoxelEcologyPlan, FVoxelEcologyTileKey, Ecology, nullptr)
#undef VOXEL_CACHE_LIFECYCLE_FAMILY

	template<>
	struct TCacheLifecycleFamily<FVoxelNaturalColumnEntry>
	{
		using ValueType = FVoxelNaturalColumnEntry;
		using KeyType = FIntPoint;
		using Ptr = FVoxelNaturalColumnEntryPtr;
		static const TCHAR* Name()
		{
			return TEXT("Natural");
		}
		static bool Build(FVoxelGenerationPlanCache& InCache, const KeyType& InKey,
			TFunctionRef<bool(ValueType&, FString&)> InBuild, Ptr& OutValue, FString& OutError, const TAtomic<bool>* InCancel)
		{
			return InCache.GetOrBuildNaturalColumn(InKey, InBuild, OutValue, OutError, InCancel);
		}
		static bool Find(FVoxelGenerationPlanCache& InCache, const KeyType& InKey, Ptr& OutValue)
		{
			return InCache.FindNaturalColumn(InKey, OutValue);
		}
		static int32 References(FVoxelGenerationPlanCache& InCache, const KeyType& InKey)
		{
			return FVoxelCacheLifecycleTestAccess::NaturalReferences(InCache, InKey);
		}
		static constexpr void (FVoxelGenerationPlanCache::*Store)(const KeyType&, Ptr) = nullptr;
	};

	enum class ECacheLifecycleScenario : uint8
	{
		None = 0,
		Reset,
		Cancel,
		Store
	};

	template<typename ValueType>
	struct TCacheLifecycleState
	{
		using Family = TCacheLifecycleFamily<ValueType>;
		using Ptr = typename Family::Ptr;
		TCacheLifecycleState()
		{
			for (int32 Index = 0; Index < 2; ++Index)
			{
				Entered[Index] = FPlatformProcess::GetSynchEventFromPool(true);
				Release[Index] = FPlatformProcess::GetSynchEventFromPool(true);
			}
			for (Ptr& Value : Values) Value = Sentinel;
		}
		~TCacheLifecycleState()
		{
			for (int32 Index = 0; Index < 2; ++Index)
			{
				FPlatformProcess::ReturnSynchEventToPool(Entered[Index]);
				FPlatformProcess::ReturnSynchEventToPool(Release[Index]);
			}
		}
		void Unblock()
		{
			Cancel.Store(true);
			for (FEvent* Event : Release) Event->Trigger();
		}
		bool AllReady() const
		{
			for (int32 Index = 0; Index < 4; ++Index)
				if (Started[Index] && !Tasks[Index].IsReady()) return false;
			return true;
		}
		TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache =
			MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>(false, true);
		typename Family::KeyType Key {};
		Ptr Sentinel = MakeShared<ValueType, ESPMode::ThreadSafe>();
		Ptr Stored = MakeShared<ValueType, ESPMode::ThreadSafe>();
		Ptr Values[4];
		FString Errors[4];
		TFuture<void> Tasks[4];
		bool Started[4] = {};
		bool Success[4] = {};
		bool UnexpectedBuilder[4] = {};
		FEvent* Entered[2] = {};
		FEvent* Release[2] = {};
		TAtomic<bool> Cancel {false};
	};

	template<typename ValueType>
	class TCacheLifecycleCommand final : public IAutomationLatentCommand
	{
		using Family = TCacheLifecycleFamily<ValueType>;
		using StateType = TCacheLifecycleState<ValueType>;
		enum class EPhase : uint8
		{
			None = 0,
			Begin,
			OwnerEntered,
			OldWaiterAttached,
			OldWaiterRetired,
			NewOwnerEntered,
			OldOwnerRetired,
			NewWaiterAttached,
			AllFinished,
			Cleanup
		};
	public:
		TCacheLifecycleCommand(FAutomationTestBase& InTest, const ECacheLifecycleScenario InScenario)
			: Test(InTest), Scenario(InScenario), State(MakeShared<StateType, ESPMode::ThreadSafe>())
		{
			SetPhase(EPhase::Begin);
		}
		~TCacheLifecycleCommand()
		{
			State->Unblock();
		}

		virtual bool Update() override
		{
			// GameThread 只做零超时事件读取和 IsReady；线程完成前绝不 Get/Wait/join。
			if (Phase == EPhase::Cleanup)
			{
				if (State->AllReady()) return true;
				if (FPlatformTime::Seconds() < Deadline) return false;
				Test.AddError(Label(TEXT("清理后后台任务仍未退出")));
				return true;
			}
			if (FPlatformTime::Seconds() >= Deadline)
			{
				Test.AddError(Label(TEXT("显式交错阶段超时")));
				State->Unblock();
				State->Cache->Reset();
				SetPhase(EPhase::Cleanup);
				return false;
			}
			switch (Phase)
			{
			case EPhase::Begin:
				LaunchOwner(0);
				SetPhase(EPhase::OwnerEntered);
				break;
			case EPhase::OwnerEntered:
				if (!State->Entered[0]->Wait(0)) break;
				if (Scenario == ECacheLifecycleScenario::Cancel)
				{
					State->Cancel.Store(true);
					State->Release[0]->Trigger();
					SetPhase(EPhase::AllFinished);
				}
				else if (Scenario == ECacheLifecycleScenario::Store)
				{
					check(Family::Store);
					(State->Cache.Get().*Family::Store)(State->Key, State->Stored);
					State->Release[0]->Trigger();
					SetPhase(EPhase::AllFinished);
				}
				else
				{
					LaunchWaiter(2);
					SetPhase(EPhase::OldWaiterAttached);
				}
				break;
			case EPhase::OldWaiterAttached:
				if (Family::References(*State->Cache, State->Key) < 3) break;
				State->Cache->Reset();
				SetPhase(EPhase::OldWaiterRetired);
				break;
			case EPhase::OldWaiterRetired:
				if (!State->Tasks[2].IsReady()) break;
				Test.TestFalse(Label(TEXT("Reset终止已附着旧waiter")), State->Success[2]);
				Test.TestFalse(Label(TEXT("旧waiter不执行替代builder")), State->UnexpectedBuilder[2]);
				Test.TestTrue(Label(TEXT("失败旧waiter保留Out")), State->Values[2] == State->Sentinel);
				Test.TestFalse(Label(TEXT("waiter退出时旧owner仍受event阻塞")), State->Tasks[0].IsReady());
				LaunchOwner(1);
				SetPhase(EPhase::NewOwnerEntered);
				break;
			case EPhase::NewOwnerEntered:
				if (!State->Entered[1]->Wait(0)) break;
				State->Release[0]->Trigger();
				SetPhase(EPhase::OldOwnerRetired);
				break;
			case EPhase::OldOwnerRetired:
				if (!State->Tasks[0].IsReady()) break;
				Test.TestFalse(Label(TEXT("Reset后的旧owner被拒收")), State->Success[0]);
				Test.TestTrue(Label(TEXT("失败旧owner保留Out")), State->Values[0] == State->Sentinel);
				{
					typename Family::Ptr Found;
					Test.TestFalse(Label(TEXT("旧owner不能发布到新代cache")), Family::Find(*State->Cache, State->Key, Found));
				}
				Test.TestTrue(Label(TEXT("旧owner不能删除同key新gate")), Family::References(*State->Cache, State->Key) >= 2);
				LaunchWaiter(3);
				SetPhase(EPhase::NewWaiterAttached);
				break;
			case EPhase::NewWaiterAttached:
				if (State->Tasks[3].IsReady())
				{
					Test.AddError(Label(TEXT("新waiter未等待新gate而提前退出")));
					State->Release[1]->Trigger();
					SetPhase(EPhase::AllFinished);
				}
				else if (Family::References(*State->Cache, State->Key) >= 3)
				{
					State->Release[1]->Trigger();
					SetPhase(EPhase::AllFinished);
				}
				break;
			case EPhase::AllFinished:
				if (!State->AllReady()) break;
				VerifyFinished();
				return true;
			default:
				break;
			}
			return false;
		}
	private:
		FString Label(const TCHAR* InText) const
		{
			return FString::Printf(TEXT("%s: %s"), Family::Name(), InText);
		}
		void SetPhase(const EPhase InPhase)
		{
			Phase = InPhase;
			Deadline = FPlatformTime::Seconds() + 8.0;
		}
		void LaunchOwner(const int32 InIndex)
		{
			State->Started[InIndex] = true;
			State->Tasks[InIndex] = Async(EAsyncExecution::Thread, [Captured = State, InIndex]()
			{
				Captured->Success[InIndex] = Family::Build(*Captured->Cache, Captured->Key,
					[Captured, InIndex](ValueType&, FString& OutError)
					{
						Captured->Entered[InIndex]->Trigger();
						if (!Captured->Release[InIndex]->Wait(10000))
						{
							OutError = TEXT("Fixture builder release timed out");
							return false;
						}
						// Cancel用例故意返回true，必须由真实Cache在发布前拒收。
						return true;
					}, Captured->Values[InIndex], Captured->Errors[InIndex], &Captured->Cancel);
			});
		}
		void LaunchWaiter(const int32 InIndex)
		{
			State->Started[InIndex] = true;
			State->Tasks[InIndex] = Async(EAsyncExecution::Thread, [Captured = State, InIndex]()
			{
				Captured->Success[InIndex] = Family::Build(*Captured->Cache, Captured->Key,
					[Captured, InIndex](ValueType&, FString&)
					{
						Captured->UnexpectedBuilder[InIndex] = true;
						return false;
					}, Captured->Values[InIndex], Captured->Errors[InIndex], &Captured->Cancel);
			});
		}
		void VerifyFinished()
		{
			typename Family::Ptr Found;
			if (Scenario == ECacheLifecycleScenario::Cancel)
			{
				Test.TestFalse(Label(TEXT("执行期间Cancel拒收返回true的builder")), State->Success[0]);
				Test.TestTrue(Label(TEXT("Cancel失败保留Out")), State->Values[0] == State->Sentinel);
				Test.TestTrue(Label(TEXT("Cancel返回明确原因")), State->Errors[0].StartsWith(TEXT("Canceled")));
				Test.TestFalse(Label(TEXT("Cancel不发布正文")), Family::Find(*State->Cache, State->Key, Found));
			}
			else if (Scenario == ECacheLifecycleScenario::Store)
			{
				Test.TestTrue(Label(TEXT("并发Store终结owner为已发布成功")), State->Success[0]);
				Test.TestTrue(Label(TEXT("owner取得Store的原immutable引用")), State->Values[0] == State->Stored);
				Test.TestTrue(Label(TEXT("Store正文仍可读取")), Family::Find(*State->Cache, State->Key, Found));
				Test.TestTrue(Label(TEXT("owner不能覆盖Store正文")), Found == State->Stored);
			}
			else
			{
				Test.TestTrue(Label(TEXT("新owner正常发布")), State->Success[1]);
				Test.TestTrue(Label(TEXT("新waiter从自己的新gate返回成功")), State->Success[3]);
				Test.TestFalse(Label(TEXT("新waiter未创建替代builder")), State->UnexpectedBuilder[3]);
				Test.TestTrue(Label(TEXT("新owner及新waiter消费同一immutable")),
					State->Values[1].IsValid() && State->Values[1] == State->Values[3] && State->Values[1] != State->Sentinel);
				Test.TestTrue(Label(TEXT("cache仍发布新owner正文")), Family::Find(*State->Cache, State->Key, Found));
				Test.TestTrue(Label(TEXT("cache引用归属新owner")), Found == State->Values[1]);
			}
		}
		FAutomationTestBase& Test;
		ECacheLifecycleScenario Scenario;
		TSharedRef<StateType, ESPMode::ThreadSafe> State;
		EPhase Phase = EPhase::Begin;
		double Deadline = 0.0;
	};

	template<typename ValueType>
	void EnqueueCacheLifecycle(FAutomationTestBase& InTest, const ECacheLifecycleScenario InScenario)
	{
		FAutomationTestFramework::Get().EnqueueLatentCommand(
			MakeShared<TCacheLifecycleCommand<ValueType>>(InTest, InScenario));
	}

	void MaintainCache(FVoxelGenerationPlanCache& InCache)
	{
		for (int32 Index = 0; Index < FVoxelGenerationPlanCache::ShardCount * 2; ++Index) InCache.TickMaintenance(256);
	}

	bool SeedBoundaryFacts(FVoxelGenerationPlanCache& InCache, const int32 InCell, FString& OutError)
	{
		FVoxelGenerationSettings Settings;
		Settings.HydrologyRegionSide = 8;
		Settings.HydrologyCellSize = 3;
		InCache.StoreHydrology(FVoxelGenerationQuery::HydrologyKeyForVoxel(InCell, InCell, Settings),
			MakeShared<FVoxelHydrologyPlan, ESPMode::ThreadSafe>());
		const FVoxelGenerationTileKey Tile{FIntVector(VoxelGeneration::FloorDivide(InCell, 256),
			VoxelGeneration::FloorDivide(InCell, 256), 0)};
		InCache.StoreCave(Tile, MakeShared<FVoxelCavePlan, ESPMode::ThreadSafe>());
		InCache.StoreFeature(Tile, MakeShared<FVoxelFeaturePlan, ESPMode::ThreadSafe>());
		InCache.StoreStructure(Tile, MakeShared<FVoxelStructurePlan, ESPMode::ThreadSafe>());
		FVoxelEcologyPlanPtr Ecology;
		const FVoxelEcologyTileKey EcologyKey{FIntPoint(VoxelGeneration::FloorDivide(InCell, 64),
			VoxelGeneration::FloorDivide(InCell, 64))};
		return InCache.GetOrBuildEcology(EcologyKey,
			[](FVoxelEcologyPlan&, FString&)
			{
				return true;
			}, Ecology, OutError);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCacheLifecycleResetTest,
	"WHFramework.Voxel.Generation.CacheLifecycle.ResetGateABA",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCacheLifecycleResetTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	EnqueueCacheLifecycle<FVoxelHydrologyPlan>(*this, ECacheLifecycleScenario::Reset);
	EnqueueCacheLifecycle<FVoxelNaturalColumnEntry>(*this, ECacheLifecycleScenario::Reset);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCacheLifecycleCancelTest,
	"WHFramework.Voxel.Generation.CacheLifecycle.CancelDuringBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCacheLifecycleCancelTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	EnqueueCacheLifecycle<FVoxelHydrologyPlan>(*this, ECacheLifecycleScenario::Cancel);
	EnqueueCacheLifecycle<FVoxelCavePlan>(*this, ECacheLifecycleScenario::Cancel);
	EnqueueCacheLifecycle<FVoxelFeaturePlan>(*this, ECacheLifecycleScenario::Cancel);
	EnqueueCacheLifecycle<FVoxelStructurePlan>(*this, ECacheLifecycleScenario::Cancel);
	EnqueueCacheLifecycle<FVoxelEcologyPlan>(*this, ECacheLifecycleScenario::Cancel);
	EnqueueCacheLifecycle<FVoxelNaturalColumnEntry>(*this, ECacheLifecycleScenario::Cancel);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCacheLifecycleStoreTest,
	"WHFramework.Voxel.Generation.CacheLifecycle.ConcurrentStore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCacheLifecycleStoreTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	EnqueueCacheLifecycle<FVoxelHydrologyPlan>(*this, ECacheLifecycleScenario::Store);
	EnqueueCacheLifecycle<FVoxelCavePlan>(*this, ECacheLifecycleScenario::Store);
	EnqueueCacheLifecycle<FVoxelFeaturePlan>(*this, ECacheLifecycleScenario::Store);
	EnqueueCacheLifecycle<FVoxelStructurePlan>(*this, ECacheLifecycleScenario::Store);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCacheLifecycleWideRetentionTest,
	"WHFramework.Voxel.Generation.CacheLifecycle.WideRetention",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCacheLifecycleWideRetentionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(47, FGuid::NewGuid(), 1, {}, 1);
	FVoxelGenerationCacheRetention Retention;
	Retention.Revision = 1;
	Retention.HydrologyRegionSide = 8;
	Retention.HydrologyCellSize = 3;
	FVoxelGenerationCacheRetentionPoint Point;
	Point.Center = FIntPoint(MIN_int32, MIN_int32);
	Point.HydrologyRadiusCells = 4;
	Point.PlanRadiusCells = 256;
	Retention.Points.Add(Point);
	Cache->UpdateRetention(Retention);
	FString Error;
	if (!SeedBoundaryFacts(*Cache, MIN_int32, Error) || !SeedBoundaryFacts(*Cache, MAX_int32 - 1, Error))
	{
		AddError(Error);
		return false;
	}
	MaintainCache(*Cache);
	const auto Negative = Cache->GetStats();
	TestEqual(TEXT("负边界source保留完整region中心元数据MIN-4，允许在域外"), Negative.Hydrology, 1);
	TestEqual(TEXT("256tile相距约2^32不能因减法回绕而被source保留"), Negative.Caves + Negative.Features + Negative.Structures, 3);
	TestEqual(TEXT("64ecology相距约2^32不能因减法回绕而被source保留"), Negative.Ecology, 1);
	FVoxelGenerationSettings Settings;
	Settings.HydrologyRegionSide = 8;
	Settings.HydrologyCellSize = 3;
	FVoxelHydrologyPlanPtr Hydrology;
	TestTrue(TEXT("负边界水文center不Clamp为错误身份"), Cache->FindHydrology(
		FVoxelGenerationQuery::HydrologyKeyForVoxel(MIN_int32, MIN_int32, Settings), Hydrology));
	TestFalse(TEXT("远处正边界水文不能伪装成距离4"), Cache->FindHydrology(
		FVoxelGenerationQuery::HydrologyKeyForVoxel(MAX_int32 - 1, MAX_int32 - 1, Settings), Hydrology));
	Retention.Points[0].HydrologyRadiusCells = 3;
	++Retention.Revision;
	Cache->UpdateRetention(Retention);
	MaintainCache(*Cache);
	TestFalse(TEXT("半径3不能把MIN-4中心Clamp到MIN后误保留"), Cache->FindHydrology(
		FVoxelGenerationQuery::HydrologyKeyForVoxel(MIN_int32, MIN_int32, Settings), Hydrology));

	Retention.Points.Reset();
	++Retention.Revision;
	Cache->UpdateRetention(Retention);
	Point.Center = FIntPoint(MAX_int32 - 1, MAX_int32 - 1);
	Point.HydrologyRadiusCells = 6;
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> TaskRetention;
	if (!Cache->RetainForTask(Point, Resources, Consumer, TaskRetention, Error) ||
		!SeedBoundaryFacts(*Cache, MIN_int32, Error) || !SeedBoundaryFacts(*Cache, MAX_int32 - 1, Error))
	{
		AddError(Error);
		return false;
	}
	MaintainCache(*Cache);
	const auto Positive = Cache->GetStats();
	TestEqual(TEXT("正边界task保留完整region中心元数据MAX+5，允许在域外"), Positive.Hydrology, 1);
	TestEqual(TEXT("任务保留采用完整256tile原点距离"), Positive.Caves + Positive.Features + Positive.Structures, 3);
	TestEqual(TEXT("任务保留采用完整64ecology中心距离"), Positive.Ecology, 1);
	TestTrue(TEXT("合法partial region的完整域外中心元数据按宽整数距离保留"), Cache->FindHydrology(
		FVoxelGenerationQuery::HydrologyKeyForVoxel(MAX_int32 - 1, MAX_int32 - 1, Settings), Hydrology));
	TestFalse(TEXT("负边界region不被正边界任务误保留"), Cache->FindHydrology(
		FVoxelGenerationQuery::HydrologyKeyForVoxel(MIN_int32, MIN_int32, Settings), Hydrology));
	Point.HydrologyRadiusCells = 5;
	if (!Cache->RetainForTask(Point, Resources, Consumer, TaskRetention, Error))
	{
		AddError(Error);
		return false;
	}
	MaintainCache(*Cache);
	TestFalse(TEXT("半径5不能把MAX+5中心Clamp到MAX后误保留"), Cache->FindHydrology(
		FVoxelGenerationQuery::HydrologyKeyForVoxel(MAX_int32 - 1, MAX_int32 - 1, Settings), Hydrology));
	TaskRetention.Reset();
	MaintainCache(*Cache);
	const auto Released = Cache->GetStats();
	TestEqual(TEXT("任务真实退出后五类边界事实可淘汰"),
		Released.Hydrology + Released.Caves + Released.Features + Released.Structures + Released.Ecology, 0);

	// 合法设置可产生大于int32的region边长；完整中心是元数据，保留原lattice身份。
	Retention.HydrologyRegionSide = 2048;
	Retention.HydrologyCellSize = 1 << 21;
	++Retention.Revision;
	Cache->UpdateRetention(Retention);
	Point.HydrologyRadiusCells = 2;
	if (!Cache->RetainForTask(Point, Resources, Consumer, TaskRetention, Error))
	{
		AddError(Error);
		return false;
	}
	const FVoxelHydrologyRegionKey WideKey{FIntPoint::ZeroValue};
	const FVoxelHydrologyRegionKey WideFarKey{FIntPoint(-1, -1)};
	const FVoxelHydrologyRegionKey InvalidWideKey{FIntPoint(MAX_int32, MAX_int32)};
	Cache->StoreHydrology(WideKey, MakeShared<FVoxelHydrologyPlan, ESPMode::ThreadSafe>());
	Cache->StoreHydrology(WideFarKey, MakeShared<FVoxelHydrologyPlan, ESPMode::ThreadSafe>());
	Cache->StoreHydrology(InvalidWideKey, MakeShared<FVoxelHydrologyPlan, ESPMode::ThreadSafe>());
	MaintainCache(*Cache);
	TestTrue(TEXT("region边长2^32的完整中心元数据MAX+1距离2可保留"), Cache->FindHydrology(WideKey, Hydrology));
	TestFalse(TEXT("宽region另一侧中心不能回绕到附近"), Cache->FindHydrology(WideFarKey, Hydrology));
	TestFalse(TEXT("完全域外region在三因子乘法前拒收，不能溢出伪装成附近"), Cache->FindHydrology(InvalidWideKey, Hydrology));
	TaskRetention.Reset();
	MaintainCache(*Cache);
	TestFalse(TEXT("宽region的任务退出后可淘汰"), Cache->FindHydrology(WideKey, Hydrology));

	// (2*MAX+1)*(1282*6700417/2)=2^64-1；非法key若先做三因子signed乘法会伪装成中心(-1,-1)。
	Retention.HydrologyRegionSide = 1282;
	Retention.HydrologyCellSize = 6700417;
	++Retention.Revision;
	Cache->UpdateRetention(Retention);
	Point.Center = FIntPoint(-1, -1);
	Point.HydrologyRadiusCells = 0;
	Point.PlanRadiusCells = 0;
	if (!Cache->RetainForTask(Point, Resources, Consumer, TaskRetention, Error))
	{
		AddError(Error);
		return false;
	}
	const uint64 HalfSide = uint64(Retention.HydrologyRegionSide) * Retention.HydrologyCellSize / 2;
	TestEqual(TEXT("fixture精确覆盖三因子溢出后伪装成-1的数学分支"),
		(uint64(MAX_int32) * 2 + 1) * HalfSide, MAX_uint64);
	const FVoxelGenerationTileKey InvalidTile{FIntVector(MAX_int32, MAX_int32, 0)};
	const FVoxelEcologyTileKey InvalidEcologyKey{FIntPoint(MAX_int32, MAX_int32)};
	Cache->StoreHydrology(InvalidWideKey, MakeShared<FVoxelHydrologyPlan, ESPMode::ThreadSafe>());
	Cache->StoreCave(InvalidTile, MakeShared<FVoxelCavePlan, ESPMode::ThreadSafe>());
	Cache->StoreFeature(InvalidTile, MakeShared<FVoxelFeaturePlan, ESPMode::ThreadSafe>());
	Cache->StoreStructure(InvalidTile, MakeShared<FVoxelStructurePlan, ESPMode::ThreadSafe>());
	FVoxelEcologyPlanPtr Ecology;
	if (!Cache->GetOrBuildEcology(InvalidEcologyKey,
		[](FVoxelEcologyPlan&, FString&)
		{
			return true;
		}, Ecology, Error))
	{
		AddError(Error);
		return false;
	}
	MaintainCache(*Cache);
	TestFalse(TEXT("完全域外Hydro先拒收，不生成回绕中心-1"), Cache->FindHydrology(InvalidWideKey, Hydrology));
	FVoxelCavePlanPtr Cave;
	FVoxelFeaturePlanPtr Feature;
	FVoxelStructurePlanPtr Structure;
	TestFalse(TEXT("完全域外256tile Cave可维护淘汰"), Cache->FindCave(InvalidTile, Cave));
	TestFalse(TEXT("完全域外256tile Feature可维护淘汰"), Cache->FindFeature(InvalidTile, Feature));
	TestFalse(TEXT("完全域外256tile Structure可维护淘汰"), Cache->FindStructure(InvalidTile, Structure));
	TestFalse(TEXT("完全域外64ecology可维护淘汰"), Cache->FindEcology(InvalidEcologyKey, Ecology));
	TaskRetention.Reset();
	return true;
}

#endif
