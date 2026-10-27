#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyComponent.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyNetworkSearch.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologySourcePlan.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationCapacityWait.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Generation/VoxelGenerationRecipeCodec.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

namespace VoxelHydrologyGraphCacheTests
{
	constexpr int32 Side = 8;

	bool Setup(const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& OutResources,
		TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>& OutConfig,
		TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>& OutCache, FString& OutError, const bool bWideSourceBucket = false)
	{
		FVoxelGenerationRuntimeConfig Author = *VoxelTest::MakeGenerationConfig(407, FVoxelGenerationRecipe::CurrentAlgorithmVersion);
		FVoxelGenerationRecipe Recipe = *Author.Recipe;
		Recipe.Settings.HydrologyCellSize = 32;
		Recipe.Settings.HydrologyRegionSide = Side;
		if (bWideSourceBucket)
		{
			Recipe.Settings.RiverSourceAccumulation = 3;
			Recipe.Settings.RiverSourceSpacing = Side - 2;
		}
		if (!FVoxelGenerationRecipeCodec::RefreshHash(Recipe, OutError)) return false;
		Author.Recipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe));
		FVoxelTerrainResourceLimits Limits;
		Limits.InputBytes = 8ull * 1024 * 1024;
		Limits.WorkingBytes = 8ull * 1024 * 1024;
		Limits.ResidentBytes = 16ull * 1024 * 1024;
		Limits.OverlapBytes = 1ull * 1024 * 1024;
		return FVoxelTerrainResources::Create(Limits, InConsumer, OutResources, OutError) &&
			FVoxelGenerationBinding::Freeze(Author, OutResources.ToSharedRef(), InConsumer, OutConfig, OutError) &&
			FVoxelGenerationPlanCache::Create(OutResources.ToSharedRef(), InConsumer, OutCache, OutError, true, false);
	}

	EVoxelHydrologyClosureRead Sample(const FIntPoint& InCell, FVoxelHydrologyRegionSample& OutSample, FString& OutError)
	{
		OutSample.GroundZ = 8;
		OutSample.RainWeight = 3;
		if (InCell.X == -Side || InCell.Y == -Side || InCell.X == Side - 1 || InCell.Y == Side - 1)
		{
			OutSample.GroundZ = -4;
			OutSample.WaterZ = 0;
			OutSample.WaterId = {17, 19};
		}
		OutError.Reset();
		return EVoxelHydrologyClosureRead::Ready;
	}

	bool Finish(FVoxelGenerationPlanCache& InCache, const FVoxelHydrologyRegionKey& InKey,
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		FVoxelHydrologyRegionGraphBuild::FReadCell InRead, FVoxelHydrologyRegionGraphPtr& OutGraph, FString& OutError)
	{
		for (int32 Attempt = 0; Attempt < 10000; ++Attempt)
		{
			if (InCache.AdvanceHydrologyGraph(InKey, InConfig, InRead, OutGraph, OutError)) return true;
			if (!OutError.StartsWith(TEXT("Yielded"))) return false;
		}
		OutError = TEXT("Graph cache fixture exceeded finite work bound");
		return false;
	}

	TMap<FVoxelHydrologyRegionKey, FVoxelHydrologyPlan> MakeComponentTopology()
	{
		TMap<FVoxelHydrologyRegionKey, FVoxelHydrologyPlan> Topology;
		for (int32 Y = -4; Y < 4; ++Y)
			for (int32 X = -4; X < 4; ++X)
			{
				auto& Plan = Topology.Add({FIntPoint(X, Y)});
				Plan.Grid.WorldMinCell = FIntPoint(X, Y) * Side;
				Plan.Grid.Width = Side;
				Plan.Grid.Height = Side;
				Plan.Grid.CellSize = 32;
				Plan.Grid.GroundPlane.SetNum(Side * Side);
				Plan.Grid.RainWeight.Init(3, Side * Side);
				for (int32 Index = 0; Index < Side * Side; ++Index)
				{
					const FIntPoint Cell = Plan.Grid.ToWorldHydrologyCell(Index);
					const bool bWet = Cell.X <= -Side || Cell.Y <= -Side || Cell.X >= Side - 1 || Cell.Y >= Side - 1;
					Plan.Grid.GroundPlane[Index] = bWet ? -4 : 8;
					if (bWet) Plan.Grid.OceanOutlets.Add(Index, 0);
				}
			}
		return Topology;
	}

	void Maintain(FVoxelGenerationPlanCache& InCache)
	{
		for (int32 Index = 0; Index < FVoxelGenerationPlanCache::ShardCount * 2; ++Index)
		{
			InCache.TickMaintenance(256);
		}
	}

	bool AwaitRetirement(FVoxelGenerationPlanCache& InCache, const FVoxelHydrologyRegionKey& InKey)
	{
		const double Deadline = FPlatformTime::Seconds() + 2.0;
		do
		{
			Maintain(InCache);
			FVoxelHydrologyRegionGraphPtr Probe;
			if (!InCache.FindHydrologyGraph(InKey, Probe)) return true;
			Probe.Reset();
			FPlatformProcess::Sleep(0.001f);
		} while (FPlatformTime::Seconds() < Deadline);
		return false;
	}
}

class FVoxelHydrologyExteriorTestAccess
{
public:
	static void Pause(FVoxelHydrologyComponentBuild& InBuild)
	{
		InBuild.bTestPauseAfterExteriorSnapshot = true;
	}

	static bool HasSnapshot(const FVoxelHydrologyComponentBuild& InBuild)
	{
		return InBuild.ExteriorRunoff.IsSet();
	}

	static FIntPoint Cell(const FVoxelHydrologyComponentBuild& InBuild)
	{
		return InBuild.ExteriorRunoff->Cell;
	}

	static FIntPoint Coordinate(const FVoxelHydrologyComponentBuild& InBuild)
	{
		return InBuild.ExteriorRunoff->Coordinate;
	}

	static uint64 Fingerprint(const FVoxelHydrologyComponentBuild& InBuild)
	{
		return InBuild.ExteriorRunoff->Fingerprint;
	}

	static void CorruptFingerprint(FVoxelHydrologyComponentBuild& InBuild)
	{
		InBuild.ExteriorRunoff->Fingerprint ^= 1;
	}

	static bool Matches(const FVoxelHydrologyComponentBuild& InBuild, const FVoxelHydrologyNetworkRunoff& InRunoff)
	{
		if (!InBuild.ExteriorRunoff.IsSet()) return false;
		const auto& Snapshot = InBuild.ExteriorRunoff.GetValue();
		const auto* Cell = InRunoff.Find(Snapshot.Cell);
		return Cell && Snapshot.Coordinate == InRunoff.Coordinate && Snapshot.Generation == InRunoff.Generation &&
			Snapshot.Fingerprint == InRunoff.Fingerprint && Snapshot.Control.Control == Cell->Control &&
			Snapshot.Control.Accumulation == Cell->Accumulation && Snapshot.Control.IncomingRainWeight == Cell->IncomingRainWeight &&
			Snapshot.Control.MaxChildRainWeight == Cell->MaxChildRainWeight;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyGraphCacheNetworkTest,
	"WHFramework.Voxel.Hydrology.GraphCache.PublicationAndNetwork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyGraphCacheNetworkTest::RunTest(const FString& Parameters)
{
	using namespace VoxelHydrologyGraphCacheTests;
	(void)Parameters;
	FString Error;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(407, FGuid::NewGuid(), 1, {}, 1);
	FVoxelGenerationConsumerScope Identity(Consumer);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	if (!TestTrue(TEXT("从原世界账本冻结配置和创建原缓存"), Setup(Consumer, Resources, Config, Cache, Error)))
	{
		AddError(Error);
		return false;
	}
	TArray<FVoxelHydrologyRegionGraphPtr, TFixedAllocator<4>> Held;
	struct FSources
	{
		const TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>& Cache;
		const TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>& Config;
		const TArray<FVoxelHydrologyRegionGraphPtr, TFixedAllocator<4>>& Graphs;
	};
	const FSources Source{Cache, Config, Held};
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Resident;
	const uint64 RootBytes = VoxelBoundedAllocation::SharedBytes<FVoxelHydrologyNetworkSearch>();
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, RootBytes, Working) ||
		!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident, RootBytes, Resident)) return false;
	TSharedPtr<FVoxelHydrologyNetworkSearch, ESPMode::ThreadSafe> Search = MakeShared<FVoxelHydrologyNetworkSearch, ESPMode::ThreadSafe>();
	int32 Reads = 0;
	const auto ReadCell = [&](const FIntPoint& Cell, FVoxelHydrologyRegionSample& OutSample, FString& OutError)
	{
		++Reads;
		return Sample(Cell, OutSample, OutError);
	};
	{
		FVoxelGenerationRequiredSourceScope SourceScope(Resources.Get(), &Source,
			[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
			{
				const auto& Input = *static_cast<const FSources*>(InContext);
				uint64 Bytes;
				int32 Count;
				Input.Cache->GetRequiredInputCapacity(Bytes, Count);
				InVisit({Input.Cache.Get(), 0, EVoxelTerrainResourcePool::Input, Bytes, Count});
				FVoxelGenerationRequiredSourceScope::VisitLease(Input.Config->InputLease.Get(), InVisit);
				FVoxelGenerationRequiredSourceScope::VisitLease(Input.Config->RecipeInputLease.Get(), InVisit);
				for (const auto& Graph : Input.Graphs)
				{
					FVoxelGenerationRequiredSourceScope::VisitLease(Graph->ResourceLease.Get(), InVisit);
				}
			});
		FVoxelGenerationAllocationScope Allocation(Resources, Consumer, Working, Resident, RootBytes);
		if (!Allocation.Check(Error) || !Search->Initialize(FIntPoint(-1, -1), 32, Side, Cache->GetHydrologyGeneration(), Error)) return false;
		const auto ReadGraph = [&](const FIntPoint& Coordinate, const FVoxelHydrologyRegionGraph*& OutGraph, FString& OutError)
		{
			FVoxelHydrologyRegionGraphPtr Graph;
			if (!Cache->AdvanceHydrologyGraph({Coordinate}, Config.ToSharedRef(), ReadCell, Graph, OutError))
			{
				return OutError.StartsWith(TEXT("Yielded")) ? EVoxelHydrologyClosureRead::Pending : EVoxelHydrologyClosureRead::Failed;
			}
			if (!Held.Contains(Graph)) Held.Add(Graph);
			OutGraph = Graph.Get();
			return EVoxelHydrologyClosureRead::Ready;
		};
		EVoxelHydrologyNetworkStep Step = EVoxelHydrologyNetworkStep::None;
		int32 Waits = 0;
		for (int32 Attempt = 0; Attempt < 20000; ++Attempt)
		{
			Step = Search->Advance(Cache->GetHydrologyGeneration(), ReadGraph, 51, Error);
			if (!TestTrue(TEXT("嵌套缓存构建后网络仍只计自己的真实正文"), Allocation.ValidateRetainedBytes(Search->GetAllocatedBytes(), Error))) return false;
			if (Step == EVoxelHydrologyNetworkStep::Ready) break;
			if (Step == EVoxelHydrologyNetworkStep::NeedInput) ++Waits;
			else if (Step != EVoxelHydrologyNetworkStep::Yielded)
			{
				AddError(Error);
				return false;
			}
		}
		if (!TestTrue(TEXT("网络实际消费原缓存发布的四区摘要"), Step == EVoxelHydrologyNetworkStep::Ready && Held.Num() == 4)) return false;
		TestTrue(TEXT("冷缓存实际经历分片构建等待"), Waits > 0);
		TestEqual(TEXT("每个冻结输入格只生成一次"), Reads, Side * Side * 4);
		TestEqual(TEXT("四个摘要唯一发布到原缓存"), Cache->GetStats().HydrologyGraphs, 4);
		TestTrue(TEXT("网络控制包含跨区实际结果"), Search->GetResult().Cells.Num() > Side * Side);
		for (const auto& Graph : Held)
		{
			TestTrue(TEXT("实际正文使用原Resident租约并已经发布"), Graph->ResourceLease && Graph->ResourceLease->IsOwnedBy(*Resources) &&
				Graph->ResourceLease->GetPool() == EVoxelTerrainResourcePool::Resident && Graph->ResourceLease->IsPublished());
		}
		FVoxelHydrologyRegionGraphPtr Warm;
		TestTrue(TEXT("热读返回相同不可变正文"), Cache->AdvanceHydrologyGraph({FIntPoint(-1, -1)}, Config.ToSharedRef(), ReadCell, Warm, Error) && Warm == Held[0]);
		TestEqual(TEXT("热读没有重新生成输入"), Reads, Side * Side * 4);
	}
	Search.Reset();
	Working.Reset();
	Resident.Reset();
	const uint64 Fingerprint = Held[0]->Fingerprint;
	TWeakPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> ConfigWeak(Config);
	Config.Reset();
	TestFalse(TEXT("发布正文不保留只在构建阶段使用的配置对象"), ConfigWeak.IsValid());
	Cache->Reset();
	FVoxelHydrologyRegionGraphPtr Missing;
	TestFalse(TEXT("换代后原缓存不返回旧图"), Cache->FindHydrologyGraph({FIntPoint(-1, -1)}, Missing));
	TestEqual(TEXT("换代不能损坏仍由真实读者持有的不可变正文"), Held[0]->Fingerprint, Fingerprint);
	TWeakPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> CacheWeak(Cache);
	Cache.Reset();
	TestFalse(TEXT("已发布图不会反向强持缓存形成环"), CacheWeak.IsValid());
	Held.Empty();
    TestTrue(TEXT("弱引用控制器仍存活时保留实际Input授额"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)] > 0);
    ConfigWeak.Reset();
    CacheWeak.Reset();
	const double Deadline = FPlatformTime::Seconds() + 2.0;
	while (Resources->Snapshot().LeaseCount && FPlatformTime::Seconds() < Deadline) FPlatformProcess::Sleep(0.001f);
	const auto End = Resources->Snapshot();
	TestEqual(TEXT("实际异步销毁后所有租约归还"), End.LeaseCount, 0);
	TestEqual(TEXT("构建工作池无残留"), End.Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	TestEqual(TEXT("图及控制结果驻留池无残留"), End.Used[int32(EVoxelTerrainResourcePool::Resident)], uint64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyGraphCacheCancelTest,
	"WHFramework.Voxel.Hydrology.GraphCache.CancelResetAndSingleFlight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyGraphCacheCancelTest::RunTest(const FString& Parameters)
{
	using namespace VoxelHydrologyGraphCacheTests;
	(void)Parameters;
	FString Error;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(409, FGuid::NewGuid(), 1, {}, 1);
	FVoxelGenerationConsumerScope Identity(Consumer);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	if (!Setup(Consumer, Resources, Config, Cache, Error)) return false;
	const FVoxelHydrologyRegionKey Key{FIntPoint(-1, -1)};
	FVoxelHydrologyRegionGraphPtr Graph;
	bool bReentrantAttempted = false;
	bool bReentrantBlocked = false;
	int32 UnexpectedReads = 0;
	int32 FirstCellReads = 0;
	const auto Pending = [&](const FIntPoint& Cell, FVoxelHydrologyRegionSample& OutSample, FString& OutError)
	{
		if (!bReentrantAttempted)
		{
			bReentrantAttempted = true;
			FVoxelHydrologyRegionGraphPtr Reentrant;
			FString ReentrantError;
			const auto Unexpected = [&](const FIntPoint&, FVoxelHydrologyRegionSample&, FString&)
			{
				++UnexpectedReads;
				return EVoxelHydrologyClosureRead::Failed;
			};
			bReentrantBlocked = !Cache->AdvanceHydrologyGraph(Key, Config.ToSharedRef(), Unexpected, Reentrant, ReentrantError) &&
				ReentrantError == TEXT("Yielded: hydrology graph has an active worker");
		}
		if (Cell == FIntPoint(-Side, -Side))
		{
			++FirstCellReads;
			return Sample(Cell, OutSample, OutError);
		}
		OutError.Reset();
		return EVoxelHydrologyClosureRead::Pending;
	};
	TestFalse(TEXT("缺输入不发布半成品"), Cache->AdvanceHydrologyGraph(Key, Config.ToSharedRef(), Pending, Graph, Error));
	TestTrue(TEXT("同key重入只复用原门闩，不能执行第二个构建器"), bReentrantAttempted && bReentrantBlocked && UnexpectedReads == 0);
	TestEqual(TEXT("等待阶段没有发布正文"), Cache->GetStats().HydrologyGraphs, 0);
	Cache->CancelHydrologyGraphBuild(Key, Consumer.WorldEpoch, Consumer.OwnerId, Consumer.OwnerIncarnation, Consumer.RequestSerial + 1);
	TestFalse(TEXT("其它尝试的取消不终结原构建"), Cache->AdvanceHydrologyGraph(Key, Config.ToSharedRef(), Pending, Graph, Error));
	TestEqual(TEXT("原已消费前缀不因其它尝试取消而重读"), FirstCellReads, 1);
	TAtomic<bool> Cancel(true);
	TestFalse(TEXT("原消费者取消后不继续构建"), Cache->AdvanceHydrologyGraph(Key, Config.ToSharedRef(), Pending, Graph, Error, &Cancel));
	TestEqual(TEXT("取消明确返回原生命周期状态"), Error, FString(TEXT("Canceled")));
	const double Deadline = FPlatformTime::Seconds() + 2.0;
	while (Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)] && FPlatformTime::Seconds() < Deadline) FPlatformProcess::Sleep(0.001f);
	TestEqual(TEXT("取消后真实工作状态和授额释放"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	bool bReset = false;
	const uint64 Generation = Cache->GetHydrologyGeneration();
	const auto ResetRead = [&](const FIntPoint& Cell, FVoxelHydrologyRegionSample& OutSample, FString& OutError)
	{
		if (!bReset)
		{
			bReset = true;
			Cache->Reset();
		}
		return Sample(Cell, OutSample, OutError);
	};
	TestFalse(TEXT("读取中换代不能发布旧代结果"), Cache->AdvanceHydrologyGraph(Key, Config.ToSharedRef(), ResetRead, Graph, Error));
	TestEqual(TEXT("换代让当前构建取消"), Error, FString(TEXT("Canceled")));
	TestTrue(TEXT("原缓存代号确实递增"), Cache->GetHydrologyGeneration() > Generation);
	if (!TestTrue(TEXT("新代从原冻结配置重建并发布"), Finish(*Cache, Key, Config.ToSharedRef(), Sample, Graph, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("实际发布身份属于新代"), Graph->Generation, Cache->GetHydrologyGeneration());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyGraphCachePressureTest,
	"WHFramework.Voxel.Hydrology.GraphCache.PressureProtectsActualReaders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyGraphCachePressureTest::RunTest(const FString& Parameters)
{
	using namespace VoxelHydrologyGraphCacheTests;
	(void)Parameters;
	FString Error;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(411, FGuid::NewGuid(), 1, {}, 1);
	FVoxelGenerationConsumerScope Identity(Consumer);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	if (!Setup(Consumer, Resources, Config, Cache, Error)) return false;
	FVoxelGenerationCacheRetention Retention;
	Retention.Revision = 1;
	Cache->UpdateRetention(Retention);
	const FVoxelHydrologyRegionKey Key{FIntPoint(4096, 4096)};
	FVoxelHydrologyRegionGraphPtr Graph;
	if (!Finish(*Cache, Key, Config.ToSharedRef(), Sample, Graph, Error)) return false;
	const FVoxelHydrologyRegionKey WaitingKey{FIntPoint(4097, 4096)};
	FVoxelHydrologyRegionGraphPtr Waiting;
	const auto Pending = [](const FIntPoint&, FVoxelHydrologyRegionSample&, FString& OutError)
	{
		OutError.Reset();
		return EVoxelHydrologyClosureRead::Pending;
	};
	TestFalse(TEXT("原缓存中实际保留一个活动图构建"), Cache->AdvanceHydrologyGraph(WaitingKey, Config.ToSharedRef(), Pending, Waiting, Error));
	Graph.Reset();
	Maintain(*Cache);
	if (!TestTrue(TEXT("低压力活动构建允许复用不可变摘要"), Cache->FindHydrologyGraph(Key, Graph))) return false;
	const auto State = Resources->Snapshot();
	const uint64 Limit = State.Limits.ResidentBytes - State.Limits.OverlapBytes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Pressure;
	if (!TestTrue(TEXT("原预算内制造实际驻留压力"), Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident,
		Limit - State.Used[int32(EVoxelTerrainResourcePool::Resident)] - 1, Pressure))) return false;
	Maintain(*Cache);
	FVoxelHydrologyRegionGraphPtr Probe;
	TestTrue(TEXT("压力不能回收仍有真实读者的摘要"), Cache->FindHydrologyGraph(Key, Probe) && Probe == Graph);
	Probe.Reset();
	Graph.Reset();
	TestTrue(TEXT("读者及原异步持有者退出后压力可实际回收"), AwaitRetirement(*Cache, Key));
	TestFalse(TEXT("回收后的图不再从缓存可见"), Cache->FindHydrologyGraph(Key, Probe));
	Pressure.Reset();
	Cache->CancelHydrologyGraphBuild(WaitingKey, Consumer.WorldEpoch, Consumer.OwnerId, Consumer.OwnerIncarnation, Consumer.RequestSerial);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyNativeRunoffReaderTest,
	"WHFramework.Voxel.Hydrology.GraphCache.NativeRunoffPinsAcrossSlices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyNativeRunoffReaderTest::RunTest(const FString& Parameters)
{
	using namespace VoxelHydrologyGraphCacheTests;
	(void)Parameters;
	FString Error;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(413, FGuid::NewGuid(), 1, {}, 1);
	FVoxelGenerationConsumerScope Identity(Consumer);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	if (!Setup(Consumer, Resources, Config, Cache, Error)) return false;
	FVoxelGenerationCacheRetention Retention;
	Retention.Revision = 1;
	Cache->UpdateRetention(Retention);
	const FVoxelHydrologyRegionKey Key{FIntPoint(-1, -1)};
	FVoxelHydrologyRegionGraphPtr Graph;
	if (!Finish(*Cache, Key, Config.ToSharedRef(), Sample, Graph, Error)) return false;
	const auto* Original = Graph.Get();
	Graph.Reset();
	const auto Lookup = [](const FVoxelHydrologyRegionKey&) -> const FVoxelHydrologyPlan* { return nullptr; };
	const auto Request = [](const FVoxelHydrologyRegionKey&, FString& OutError)
	{
		OutError = TEXT("Yielded: fixture awaits a real neighboring macro input");
		return false;
	};
	TSharedPtr<const FVoxelHydrologyNetworkRunoff, ESPMode::ThreadSafe> Runoff;
	if (!TestTrue(TEXT("正式汇水在读取冻结图后跨片等待邻区输入"),
		!Cache->AdvanceHydrologyRunoff(Key, Config.ToSharedRef(), Lookup, Request, Runoff, Error) && Error.StartsWith(TEXT("Yielded"))))
	{
		AddError(Error);
		return false;
	}
	const auto State = Resources->Snapshot();
	const uint64 Limit = State.Limits.ResidentBytes - State.Limits.OverlapBytes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Pressure;
	if (!TestTrue(TEXT("原限额内产生真实压力"), Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident,
		Limit - State.Used[int32(EVoxelTerrainResourcePool::Resident)] - 1, Pressure))) return false;
	// 不保留测试侧强/弱引用；唯一实际读者必须来自正式续跑 owner。
	const double Deadline = FPlatformTime::Seconds() + 0.1;
	do
	{
		Maintain(*Cache);
		FVoxelHydrologyRegionGraphPtr Probe;
		if (!TestTrue(TEXT("压力维护跨片保留正式汇水实际读取的同一图"), Cache->FindHydrologyGraph(Key, Probe) && Probe.Get() == Original)) return false;
		Probe.Reset();
		FPlatformProcess::Sleep(0.001f);
	} while (FPlatformTime::Seconds() < Deadline);
	Cache->CancelHydrologyBuild(Key, Consumer.WorldEpoch, Consumer.OwnerId, Consumer.OwnerIncarnation, Consumer.RequestSerial);
	TestTrue(TEXT("取消正式消费者后图的真实读者退出并可回收"), AwaitRetirement(*Cache, Key));
	Pressure.Reset();
	Cache.Reset();
	Config.Reset();
	const double ReleaseDeadline = FPlatformTime::Seconds() + 2.0;
	while (Resources->Snapshot().LeaseCount && FPlatformTime::Seconds() < ReleaseDeadline) FPlatformProcess::Sleep(0.001f);
	TestEqual(TEXT("取消和销毁后所有实际租约归还"), Resources->Snapshot().LeaseCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologySharedControlLifecycleTest,
	"WHFramework.Voxel.Hydrology.GraphCache.SharedControlLeaseLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologySharedControlLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace VoxelHydrologyGraphCacheTests;
	(void)Parameters;
	FString Error;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(415, FGuid::NewGuid(), 1, {}, 1);
	FVoxelGenerationConsumerScope Identity(Consumer);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	if (!Setup(Consumer, Resources, Config, Cache, Error)) { AddError(Error); return false; }
	FVoxelGenerationCacheRetention Retention;
	Retention.Revision = 1;
	Cache->UpdateRetention(Retention);
	TArray<FVoxelHydrologyRegionGraphPtr, TFixedAllocator<4>> Graphs;
	for (int32 Y = -1; Y <= 0; ++Y)
		for (int32 X = -1; X <= 0; ++X)
		{
			FVoxelHydrologyRegionGraphPtr Graph;
			if (!Finish(*Cache, {FIntPoint(X, Y)}, Config.ToSharedRef(), Sample, Graph, Error)) { AddError(Error); return false; }
			Graphs.Add(Graph);
		}
	const FVoxelHydrologyNetworkKey Key{Graphs[0]->Nodes[0].Cell, true};
	const auto Lookup = [](const FVoxelHydrologyRegionKey&) -> const FVoxelHydrologyPlan* { return nullptr; };
	int32 MissingRequests = 0;
	const auto Request = [&](const FVoxelHydrologyRegionKey&, FString& OutError)
	{
		++MissingRequests;
		OutError = TEXT("Yielded: missing physical fixture input");
		return false;
	};
	const auto Complete = [&](TSharedPtr<const FVoxelHydrologyNetworkResult, ESPMode::ThreadSafe>& OutProof)
	{
		for (int32 Attempt = 0; Attempt < 10000; ++Attempt)
		{
			if (Cache->AdvanceHydrologyControl(Key, Config.ToSharedRef(), Lookup, Request, OutProof, Error)) return true;
			if (!Error.StartsWith(TEXT("Yielded"))) return false;
		}
		return false;
	};
	TSharedPtr<const FVoxelHydrologyNetworkResult, ESPMode::ThreadSafe> Proof;
	if (!Complete(Proof)) { AddError(Error); return false; }
	if (!TestTrue(TEXT("共享证明在原账本真实驻留并包含请求种子"), Proof->ResourceLease &&
		Proof->ResourceLease->IsOwnedBy(*Resources) && Proof->Find(Key.Seed))) return false;
	const uint64 Fingerprint = Proof->Fingerprint;
	TSharedPtr<const FVoxelHydrologyNetworkResult, ESPMode::ThreadSafe> OtherReader;
	{
		auto OtherConsumer = Consumer;
		OtherConsumer.OwnerId = FGuid::NewGuid();
		++OtherConsumer.RequestSerial;
		FVoxelGenerationConsumerScope OtherIdentity(OtherConsumer);
		if (!TestTrue(TEXT("不同原消费者复用同一不可变证明正文与租约"),
			Cache->AdvanceHydrologyControl(Key, Config.ToSharedRef(), Lookup, Request, OtherReader, Error) && OtherReader == Proof)) return false;
	}
	TestEqual(TEXT("同key只有一份已发布证明"), Cache->GetStats().HydrologyControls, 1);
	const auto State = Resources->Snapshot();
	const uint64 Limit = State.Limits.ResidentBytes - State.Limits.OverlapBytes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Pressure;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident,
		Limit - State.Used[int32(EVoxelTerrainResourcePool::Resident)] - 1, Pressure)) return false;
	Maintain(*Cache);
	TestEqual(TEXT("真实读者在容量压力下仍保护共享证明"), Cache->GetStats().HydrologyControls, 1);
	TWeakPtr<const FVoxelHydrologyNetworkResult, ESPMode::ThreadSafe> WeakProof(Proof);
	Proof.Reset();
	OtherReader.Reset();
	const double Deadline = FPlatformTime::Seconds() + 2.0;
	do { Maintain(*Cache); FPlatformProcess::Sleep(0.001f); }
	while ((Cache->GetStats().HydrologyControls || WeakProof.IsValid()) && FPlatformTime::Seconds() < Deadline);
	TestEqual(TEXT("最后读者退出后压力实际淘汰缓存正文"), Cache->GetStats().HydrologyControls, 0);
	TestFalse(TEXT("异步退休后正文没有遗留强引用"), WeakProof.IsValid());
	WeakProof.Reset();
	Pressure.Reset();
	if (!Complete(Proof)) { AddError(Error); return false; }
	TestEqual(TEXT("淘汰后重建仍得到完整相同证明"), Proof->Fingerprint, Fingerprint);
	const uint64 PreviousGeneration = Proof->Generation;
	TestEqual(TEXT("完整物理夹具没有扩张到额外输入"), MissingRequests, 0);
	Cache->Reset();
	TestEqual(TEXT("换代清空共享证明索引和存储"), Cache->GetStats().HydrologyControls, 0);
	TestTrue(TEXT("换代不破坏仍由旧读者持有的冻结正文"), Proof->Generation == PreviousGeneration && Proof->Find(Key.Seed));
	TestTrue(TEXT("新代身份已变化，不能命中旧证明"), Cache->GetHydrologyGeneration() != PreviousGeneration);
	TSharedPtr<const FVoxelHydrologyNetworkResult, ESPMode::ThreadSafe> NewProof;
	TestFalse(TEXT("新代不能从旧读者正文偷取缓存命中"),
		Cache->AdvanceHydrologyControl(Key, Config.ToSharedRef(), Lookup, Request, NewProof, Error));
	TestTrue(TEXT("新代必须重新请求真实输入且不发布半成品"), MissingRequests > 0 && !NewProof && Error.StartsWith(TEXT("Yielded")));
	TAtomic<bool> Cancel{true};
	TestFalse(TEXT("取消未就绪的新代证明不允许发布"),
		Cache->AdvanceHydrologyControl(Key, Config.ToSharedRef(), Lookup, Request, NewProof, Error, &Cancel));
	TestEqual(TEXT("取消错误明确保留"), Error, FString(TEXT("Canceled")));
	Proof.Reset();
	Graphs.Reset();
	Cache.Reset();
	Config.Reset();
	const double ReleaseDeadline = FPlatformTime::Seconds() + 2.0;
	while (Resources->Snapshot().LeaseCount && FPlatformTime::Seconds() < ReleaseDeadline) FPlatformProcess::Sleep(0.001f);
	TestEqual(TEXT("正文及控制backing销毁后原账本租约全部归还"), Resources->Snapshot().LeaseCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologySharedControlCancellationTest,
	"WHFramework.Voxel.Hydrology.GraphCache.SharedControlProducerCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologySharedControlCancellationTest::RunTest(const FString& Parameters)
{
	using namespace VoxelHydrologyGraphCacheTests;
	(void)Parameters;
	FString Error;
	const auto Producer = FVoxelGenerationConsumerKey::Section(417, FGuid::NewGuid(), 1, {}, 1);
	FVoxelGenerationConsumerScope ProducerIdentity(Producer);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	if (!Setup(Producer, Resources, Config, Cache, Error)) return false;
	const FVoxelHydrologyNetworkKey Key{FIntPoint::ZeroValue, false};
	const auto Lookup = [](const FVoxelHydrologyRegionKey&) -> const FVoxelHydrologyPlan* { return nullptr; };
	int32 Requests = 0;
	bool bRetireOnRequest = false;
	bool bProducerRetired = false;
	const auto Request = [&](const FVoxelHydrologyRegionKey& InRegion, FString& OutError)
	{
		++Requests;
		if (bRetireOnRequest && !bProducerRetired)
		{
			bProducerRetired = true;
			Cache->CancelHydrologyBuild(InRegion, Producer.WorldEpoch, Producer.OwnerId, Producer.OwnerIncarnation, Producer.RequestSerial);
		}
		OutError = TEXT("Yielded: fixture awaits original macro input");
		return false;
	};
	TSharedPtr<const FVoxelHydrologyNetworkResult, ESPMode::ThreadSafe> Proof;
	for (int32 Attempt = 0; Attempt < 100 && !Requests; ++Attempt)
		Cache->AdvanceHydrologyControl(Key, Config.ToSharedRef(), Lookup, Request, Proof, Error);
	if (!TestTrue(TEXT("原生产者真实进入输入等待并保留续跑门"), Requests > 0 && !Proof && Error.StartsWith(TEXT("Yielded")))) return false;
	auto Reader = Producer;
	Reader.OwnerId = FGuid::NewGuid();
	++Reader.RequestSerial;
	{
		FVoxelGenerationConsumerScope ReaderIdentity(Reader);
		bRetireOnRequest = true;
		TAtomic<bool> ReaderCancel{false};
		for (int32 Attempt = 0; Attempt < 100 && !bProducerRetired; ++Attempt)
			Cache->AdvanceHydrologyControl(Key, Config.ToSharedRef(), Lookup, Request, Proof, Error, &ReaderCancel);
		if (!TestTrue(TEXT("另一读者推进期间真实取消原生产者"), bProducerRetired)) return false;
		TestTrue(TEXT("有效同代读者仍等待输入，没有被原生产者连带取消"), !Proof && Error.StartsWith(TEXT("Yielded")) && !ReaderCancel.Load());
		TestFalse(TEXT("有效读者可以重新接纳而非读取已退休半成品"),
			Cache->AdvanceHydrologyControl(Key, Config.ToSharedRef(), Lookup, Request, Proof, Error, &ReaderCancel));
		TestTrue(TEXT("重新接纳保留输入等待语义"), Error.StartsWith(TEXT("Yielded")));
		ReaderCancel.Store(true);
		Cache->AdvanceHydrologyControl(Key, Config.ToSharedRef(), Lookup, Request, Proof, Error, &ReaderCancel);
		TestEqual(TEXT("读者自身取消仍明确生效"), Error, FString(TEXT("Canceled")));
	}
	Cache.Reset();
	Config.Reset();
	const double Deadline = FPlatformTime::Seconds() + 2.0;
	while (Resources->Snapshot().LeaseCount && FPlatformTime::Seconds() < Deadline) FPlatformProcess::Sleep(0.001f);
	TestEqual(TEXT("两个消费者取消后没有活动证明或输入租约泄漏"), Resources->Snapshot().LeaseCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyCoveredControlReaderTest,
	"WHFramework.Voxel.Hydrology.GraphCache.CoveredControlReaders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyCoveredControlReaderTest::RunTest(const FString& Parameters)
{
	using namespace VoxelHydrologyGraphCacheTests;
	(void)Parameters;
	FString Error;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(418, FGuid::NewGuid(), 1, {}, 1);
	FVoxelGenerationConsumerScope Identity(Consumer);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	if (!Setup(Consumer, Resources, Config, Cache, Error)) { AddError(Error); return false; }
	TArray<FVoxelHydrologyRegionGraphPtr, TFixedAllocator<4>> Graphs;
	for (int32 Y = -1; Y <= 0; ++Y)
		for (int32 X = -1; X <= 0; ++X)
		{
			FVoxelHydrologyRegionGraphPtr Graph;
			if (!Finish(*Cache, {FIntPoint(X, Y)}, Config.ToSharedRef(), Sample, Graph, Error)) { AddError(Error); return false; }
			if (!TestTrue(TEXT("物理核心首个节点保留规范边界身份"), Graph->Nodes[0].Cell == Graph->Minimum)) return false;
			Graphs.Add(Graph);
		}
	const FVoxelHydrologyNetworkKey Key{Graphs[0]->Nodes[0].Cell, true};
	const auto Lookup = [](const FVoxelHydrologyRegionKey&) -> const FVoxelHydrologyPlan* { return nullptr; };
	int32 MissingRequests = 0;
	const auto Request = [&](const FVoxelHydrologyRegionKey&, FString& OutError)
	{
		++MissingRequests;
		OutError = TEXT("Yielded: fixture requires additional physical input");
		return false;
	};
	TSharedPtr<const FVoxelHydrologyNetworkResult, ESPMode::ThreadSafe> Proof;
	for (int32 Attempt = 0; Attempt < 10000 && !Proof; ++Attempt)
	{
		if (!Cache->AdvanceHydrologyControl(Key, Config.ToSharedRef(), Lookup, Request, Proof, Error) && !Error.StartsWith(TEXT("Yielded")))
		{
			AddError(Error);
			return false;
		}
	}
	if (!TestTrue(TEXT("在真实有限输入上完成原共享证明"), Proof.IsValid())) return false;
	const auto* Neighbor = Proof->Cells.FindByPredicate([](const FVoxelHydrologyNetworkCell& InCell)
	{
		return InCell.Cell.X >= 0 || InCell.Cell.Y >= 0;
	});
	if (!TestNotNull(TEXT("证明实际包含相邻区域的已认证节点"), Neighbor)) return false;
	const FVoxelHydrologyNetworkCell Expected = *Neighbor;
	TSharedPtr<const FVoxelHydrologyNetworkResult, ESPMode::ThreadSafe> Borrowed;
	{
		auto ReaderConsumer = Consumer;
		ReaderConsumer.OwnerId = FGuid::NewGuid();
		++ReaderConsumer.RequestSerial;
		FVoxelGenerationConsumerScope ReaderIdentity(ReaderConsumer);
		if (!TestTrue(TEXT("邻区读者借用同一已发布正文而不启动第二份证明"),
			Cache->FindHydrologyControl(Expected.Cell, *Config, Proof->Generation, Borrowed) && Borrowed == Proof)) return false;
	}
	TestTrue(TEXT("复用同一原资源租约且控制值完整相同"), Borrowed->ResourceLease == Proof->ResourceLease && *Borrowed->Find(Expected.Cell) == Expected);
	TestEqual(TEXT("覆盖查询不新增缓存正文"), Cache->GetStats().HydrologyControls, 1);
	TestEqual(TEXT("覆盖查询不请求更多拓扑输入"), MissingRequests, 0);
	const uint64 Generation = Proof->Generation;
	TestFalse(TEXT("错误代次不返回旧证明"), Cache->FindHydrologyControl(Expected.Cell, *Config, Generation + 1, Borrowed));
	TestFalse(TEXT("未命中清除此前输出引用"), Borrowed.IsValid());
	TestFalse(TEXT("无覆盖节点不捏造结果"), Cache->FindHydrologyControl({Side * 100, Side * 100}, *Config, Generation, Borrowed));
	FVoxelGenerationRuntimeConfig OtherConfig = *Config;
	FVoxelGenerationRecipe OtherRecipe = *Config->Recipe;
	++OtherRecipe.RecipeHash;
	OtherConfig.Recipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(OtherRecipe));
	TestFalse(TEXT("相同代次也拒绝其他配方身份"), Cache->FindHydrologyControl(Expected.Cell, OtherConfig, Generation, Borrowed));
	Cache->Reset();
	TestFalse(TEXT("换代后不能从旧缓存索引取得证明"), Cache->FindHydrologyControl(Expected.Cell, *Config, Generation, Borrowed));
	TestTrue(TEXT("换代不破坏已持有的不可变控制"), *Proof->Find(Expected.Cell) == Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologySourceTransientInputsTest,
	"WHFramework.Voxel.Hydrology.GraphCache.SourceTransientInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologySourceTransientInputsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Error;
	FVoxelHydrologyRegionGraph Graph;
	Graph.Generation = 1;
	Graph.Fingerprint = 123;
	Graph.CellSize = 32;
	Graph.Width = 3;
	Graph.Height = 1;
	Graph.CellLocations = {0, 1, 2};
	Graph.Nodes.SetNum(3);
	FVoxelHydrologyNetworkRunoff Runoff;
	Runoff.Generation = 1;
	Runoff.GraphFingerprint = Graph.Fingerprint;
	Runoff.Fingerprint = 456;
	Runoff.Cells.SetNum(3);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Graph.Nodes[Index].Cell = FIntPoint(Index, 0);
		Graph.Nodes[Index].GroundZ = 10 + Index * 2;
		Runoff.Cells[Index].Control.Cell = FIntPoint(Index, 0);
		Runoff.Cells[Index].Control.bHasParent = Index < 2;
		Runoff.Cells[Index].Control.ParentCell = FIntPoint(Index + 1, 0);
		Runoff.Cells[Index].Accumulation = 20 + Index * 10;
	}
	Graph.Nodes[2].WaterZ = 0;
	Graph.Nodes[2].GroundZ = -1;
	FVoxelGenerationSettings Settings;
	Settings.HydrologyCellSize = 32;
	Settings.RiverSourceAccumulation = 10;
	Settings.RiverSourceSpacing = 4;
	FVoxelHydrologySourceBucket Bucket;
	Bucket.Generation = 1;
	Bucket.Fingerprint = 1000;
	Bucket.bHasWinner = true;
	Bucket.Winner.Cell = FIntPoint(1, 0);
	Bucket.Winner.Accumulation = 30;
	Bucket.WinnerId = {7, 9};
	const auto PendingBucket = [](const FIntPoint&, const FVoxelHydrologySourceBucket*&, FString& OutError)
	{
		OutError.Reset();
		return EVoxelHydrologyClosureRead::Pending;
	};
	const auto ReadyBucket = [&](const FIntPoint&, const FVoxelHydrologySourceBucket*& OutBucket, FString& OutError)
	{
		OutBucket = &Bucket;
		OutError.Reset();
		return EVoxelHydrologyClosureRead::Ready;
	};
	FVoxelHydrologySourcePlanBuild Build;
	if (!TestTrue(TEXT("匹配原始冻结输入初始化"), Build.Initialize(Graph, Runoff, Settings, Error))) return false;
	if (!TestTrue(TEXT("扫描完整区域后等待真实桶结果"), Build.Advance(1, &Graph, &Runoff, PendingBucket, 256, Error) == EVoxelHydrologyRegionStep::NeedInput)) return false;
	TestTrue(TEXT("桶等待不再要求持有图或汇水正文"), !Build.NeedsGraph() && !Build.NeedsRunoff());
	FVoxelHydrologyRegionGraph AttachmentGraph = Graph;
	Graph = {};
	Runoff = {};
	TestTrue(TEXT("销毁扫描输入仍保留正确等待状态"), Build.Advance(1, nullptr, nullptr, PendingBucket, 256, Error) == EVoxelHydrologyRegionStep::NeedInput);
	TestTrue(TEXT("无输入等待时仍拒绝跨代"), Build.Advance(2, nullptr, nullptr, PendingBucket, 1, Error) == EVoxelHydrologyRegionStep::Failed);
	TAtomic<bool> Cancel{true};
	TestTrue(TEXT("无工作预算时取消仍先于输入读取"), Build.Advance(1, nullptr, nullptr, ReadyBucket, 0, Error, &Cancel) == EVoxelHydrologyRegionStep::Canceled);
	TestTrue(TEXT("桶就绪后附着阶段等待原图而非重读汇水"), Build.Advance(1, nullptr, nullptr, ReadyBucket, 256, Error) == EVoxelHydrologyRegionStep::Yielded && Build.NeedsGraph() && !Build.NeedsRunoff());
	++AttachmentGraph.Fingerprint;
	TestTrue(TEXT("附着拒绝被改写的原图"), Build.Advance(1, &AttachmentGraph, nullptr, ReadyBucket, 1, Error) == EVoxelHydrologyRegionStep::Failed);
	--AttachmentGraph.Fingerprint;
	EVoxelHydrologyRegionStep Step = EVoxelHydrologyRegionStep::Yielded;
	for (int32 Attempt = 0; Attempt < 100 && Step == EVoxelHydrologyRegionStep::Yielded; ++Attempt)
		Step = Build.Advance(1, &AttachmentGraph, nullptr, ReadyBucket, 1, Error);
	if (!TestTrue(TEXT("无需汇水重建即可完成原源头附着"), Step == EVoxelHydrologyRegionStep::Ready)) { AddError(Error); return false; }
	const auto& Result = Build.GetResult();
	if (!TestEqual(TEXT("全桶竞争只保留真正赢家"), Result.Sources.Num(), 1)) return false;
	const auto& Source = Result.Sources[0];
	TestTrue(TEXT("源头身份位置雨量水位约束不变"), Source.Cell == FIntPoint(1, 0) && Source.Id.High == 7 && Source.Id.Low == 9 && Source.Accumulation == 30 && Source.Location == 1 && Source.AttachmentWaterCap == 12);
	TestEqual(TEXT("完成后临时候选存储实际释放"), Build.GetAllocatedBytes(), Result.GetAllocatedBytes());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologySourceWaitingReleaseTest,
	"WHFramework.Voxel.Hydrology.GraphCache.SourceWaitingReleasesInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologySourceWaitingReleaseTest::RunTest(const FString& Parameters)
{
	using namespace VoxelHydrologyGraphCacheTests;
	(void)Parameters;
	FString Error;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(420, FGuid::NewGuid(), 1, {}, 1);
	FVoxelGenerationConsumerScope Identity(Consumer);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	if (!Setup(Consumer, Resources, Config, Cache, Error, true)) { AddError(Error); return false; }
	FVoxelGenerationCacheRetention Retention;
	Retention.Revision = 1;
	Cache->UpdateRetention(Retention);
	const auto BoundedSample = [](const FIntPoint& InCell, FVoxelHydrologyRegionSample& OutSample, FString& OutError)
	{
		Sample(InCell, OutSample, OutError);
		if (InCell.X < -Side || InCell.Y < -Side || InCell.X >= Side || InCell.Y >= Side)
		{
			OutSample.GroundZ = -4;
			OutSample.WaterZ = 0;
			OutSample.WaterId = {17, 19};
		}
		return EVoxelHydrologyClosureRead::Ready;
	};
	for (int32 Y = -2; Y <= 1; ++Y)
		for (int32 X = -2; X <= 1; ++X)
		{
			if ((X == -2 || X == 1) && (Y == -2 || Y == 1)) continue;
			FVoxelHydrologyRegionGraphPtr Graph;
			if (!Finish(*Cache, {FIntPoint(X, Y)}, Config.ToSharedRef(), BoundedSample, Graph, Error)) { AddError(Error); return false; }
		}
	const FVoxelHydrologyRegionKey Key{FIntPoint(-1, -1)};
	const auto Lookup = [](const FVoxelHydrologyRegionKey&) -> const FVoxelHydrologyPlan* { return nullptr; };
	int32 Requests = 0;
	const auto Request = [&](const FVoxelHydrologyRegionKey&, FString& OutError)
	{
		++Requests;
		OutError = TEXT("Yielded: fixture awaits original bucket region");
		return false;
	};
	TSharedPtr<const FVoxelHydrologyNetworkRunoff, ESPMode::ThreadSafe> Runoff;
	for (int32 Attempt = 0; Attempt < 10000 && !Runoff; ++Attempt)
		if (!Cache->AdvanceHydrologyRunoff(Key, Config.ToSharedRef(), Lookup, Request, Runoff, Error) && !Error.StartsWith(TEXT("Yielded"))) { AddError(Error); return false; }
	if (!TestTrue(TEXT("有限湿边界内先完成真实汇水"), Runoff.IsValid() && Requests == 0)) return false;
	TWeakPtr<const FVoxelHydrologyNetworkRunoff, ESPMode::ThreadSafe> WeakRunoff = Runoff;
	Runoff.Reset();
	TSharedPtr<const FVoxelHydrologySourcePlan, ESPMode::ThreadSafe> Sources;
	for (int32 Attempt = 0; Attempt < 100 && !Requests; ++Attempt)
		if (!Cache->AdvanceHydrologySources(Key, Config.ToSharedRef(), Lookup, Request, Sources, Error) && !Error.StartsWith(TEXT("Yielded"))) { AddError(Error); return false; }
	if (!TestTrue(TEXT("正式源计划扫描后进入跨区桶等待"), Requests > 0 && !Sources && Error.StartsWith(TEXT("Yielded")))) return false;
	const auto State = Resources->Snapshot();
	const uint64 Limit = State.Limits.ResidentBytes - State.Limits.OverlapBytes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Pressure;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident, Limit - State.Used[int32(EVoxelTerrainResourcePool::Resident)] - 1, Pressure)) return false;
	TestTrue(TEXT("桶等待时原图不再被源计划无谓钉住"), AwaitRetirement(*Cache, Key));
	const double Deadline = FPlatformTime::Seconds() + 2.0;
	do { Maintain(*Cache); FPlatformProcess::Sleep(0.001f); }
	while (WeakRunoff.IsValid() && FPlatformTime::Seconds() < Deadline);
	TestFalse(TEXT("桶等待时原汇水正文也实际回收"), WeakRunoff.IsValid());
	Pressure.Reset();
	const int32 PreviousRequests = Requests;
	TestFalse(TEXT("缺少真实桶输入仍不能发布源计划"), Cache->AdvanceHydrologySources(Key, Config.ToSharedRef(), Lookup, Request, Sources, Error));
	TestTrue(TEXT("继续直接等待桶而非重建扫描输入"), Requests > PreviousRequests && !WeakRunoff.IsValid() && Error.StartsWith(TEXT("Yielded")));
	FVoxelHydrologyRegionGraphPtr Probe;
	TestFalse(TEXT("等待桶没有重建已完成扫描的图"), Cache->FindHydrologyGraph(Key, Probe));
	WeakRunoff.Reset();
	Cache->CancelHydrologyBuild(Key, Consumer.WorldEpoch, Consumer.OwnerId, Consumer.OwnerIncarnation, Consumer.RequestSerial);
	Cache.Reset();
	Config.Reset();
	const double ReleaseDeadline = FPlatformTime::Seconds() + 2.0;
	while (Resources->Snapshot().LeaseCount && FPlatformTime::Seconds() < ReleaseDeadline) FPlatformProcess::Sleep(0.001f);
	TestEqual(TEXT("取消等待后原账本租约完全归还"), Resources->Snapshot().LeaseCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologySharedRegionalComponentReadersTest,
	"WHFramework.Voxel.Hydrology.GraphCache.SharedRegionalComponentReaders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologySharedRegionalComponentReadersTest::RunTest(const FString& Parameters)
{
	using namespace VoxelHydrologyGraphCacheTests;
	(void)Parameters;
	FString Error;
	const auto Producer = FVoxelGenerationConsumerKey::Section(421, FGuid::NewGuid(), 1, {}, 1);
	FVoxelGenerationConsumerScope ProducerIdentity(Producer);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	if (!Setup(Producer, Resources, Config, Cache, Error, true)) { AddError(Error); return false; }
	const auto Topology = MakeComponentTopology();
	const FVoxelHydrologyRegionKey Key{FIntPoint(-1, -1)};
	auto Reader = Producer;
	Reader.OwnerId = FGuid::NewGuid();
	++Reader.RequestSerial;
	bool bSupplyInput = false;
	int32 Requests = 0;
	const auto Lookup = [&](const FVoxelHydrologyRegionKey& InKey) -> const FVoxelHydrologyPlan*
	{
		return bSupplyInput ? Topology.Find(InKey) : nullptr;
	};
	const auto Request = [&](const FVoxelHydrologyRegionKey&, FString& OutError)
	{
		++Requests;
		OutError = TEXT("Yielded: component fixture awaits topology");
		return false;
	};
	TSharedPtr<const FVoxelHydrologyRiverComponent, ESPMode::ThreadSafe> First;
	for (int32 Attempt = 0; Attempt < 100 && !Requests; ++Attempt)
		Cache->AdvanceHydrologyComponent(Key, Config.ToSharedRef(), Lookup, Request, First, Error);
	if (!TestTrue(TEXT("缺真实输入不能发布区域组件"), Requests > 0 && !First && Error.StartsWith(TEXT("Yielded")))) return false;
	const auto Waiting = Resources->Snapshot();
	TSharedPtr<const FVoxelHydrologyRiverComponent, ESPMode::ThreadSafe> Second;
	{
		FVoxelGenerationConsumerScope ReaderIdentity(Reader);
		for (int32 Attempt = 0; Attempt < 8; ++Attempt)
		{
			Cache->AdvanceHydrologyComponent(Key, Config.ToSharedRef(), Lookup, Request, Second, Error);
			if (!TestTrue(TEXT("第二消费者继续等待原输入而非发布半成品"), !Second && Error.StartsWith(TEXT("Yielded")))) return false;
		}
	}
	TestEqual(TEXT("同区域等待读者不新增租约"), Resources->Snapshot().LeaseCount, Waiting.LeaseCount);
	TestEqual(TEXT("同区域等待读者不复制在建正文"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], Waiting.Used[int32(EVoxelTerrainResourcePool::Working)]);
	bSupplyInput = true;
	bool bCheckedActiveReader = false;
	const auto ReentrantLookup = [&](const FVoxelHydrologyRegionKey& InKey) -> const FVoxelHydrologyPlan*
	{
		if (!bCheckedActiveReader)
		{
			bCheckedActiveReader = true;
			FVoxelGenerationConsumerScope ReaderIdentity(Reader);
			FString ReaderError;
			TestFalse(TEXT("原组件正在推进时第二消费者不启动并行正文"), Cache->AdvanceHydrologyComponent(Key, Config.ToSharedRef(), Lookup, Request, Second, ReaderError));
			TestTrue(TEXT("活动读者返回可重试状态"), !Second && ReaderError.StartsWith(TEXT("Yielded")));
		}
		return Topology.Find(InKey);
	};
	FVoxelGenerationCapacityWait Continuation;
	for (int32 Attempt = 0; Attempt < 100000 && !First; ++Attempt)
	{
		if (!Cache->AdvanceHydrologyComponent(Key, Config.ToSharedRef(), ReentrantLookup, Request, First, Error) && !Error.StartsWith(TEXT("Yielded")))
		{ AddError(Error); return false; }
		if (!First)
		{
			FVoxelTaskResult Slice;
			Slice.Error = Error;
			if (!TestTrue(TEXT("拓扑完整且无其他生产者时计算切片保留原立即续跑语义"),
				Continuation.Continue(Slice, 0) && Slice.bContinue && Slice.bContinueImmediately)) return false;
		}
	}
	if (!TestTrue(TEXT("原区域组件完整完成且真实覆盖活动读者"), First.IsValid() && bCheckedActiveReader)) return false;
	{
		FVoxelGenerationConsumerScope ReaderIdentity(Reader);
		if (!TestTrue(TEXT("第二消费者命中同一冻结正文及租约"), Cache->AdvanceHydrologyComponent(Key, Config.ToSharedRef(), Lookup, Request, Second, Error) && First == Second && First->ResourceLease == Second->ResourceLease)) return false;
	}
	const uint64 Fingerprint = First->Fingerprint;
	const uint64 Generation = First->Generation;
	const auto PublishedState = Resources->Snapshot();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Pressure;
	const uint64 ResidentLimit = PublishedState.Limits.ResidentBytes - PublishedState.Limits.OverlapBytes;
	if (!TestTrue(TEXT("真实驻留压力成功接纳"), Resources->Acquire(Producer, EVoxelTerrainResourcePool::Resident,
		ResidentLimit - PublishedState.Used[int32(EVoxelTerrainResourcePool::Resident)] - 1, Pressure))) return false;
	Maintain(*Cache);
	TestTrue(TEXT("压力不能退休仍由读者持有的共享正文"), Cache->AdvanceHydrologyComponent(Key, Config.ToSharedRef(), Lookup, Request, Second, Error) && First == Second);
	Pressure.Reset();
	TSharedPtr<const FVoxelHydrologyRiverComponent, ESPMode::ThreadSafe> Neighbor;
	for (int32 Attempt = 0; Attempt < 100000 && !Neighbor; ++Attempt)
	{
		if (!Cache->AdvanceHydrologyComponent({FIntPoint(0, -1)}, Config.ToSharedRef(), Lookup, Request, Neighbor, Error) && !Error.StartsWith(TEXT("Yielded")))
		{ AddError(Error); return false; }
	}
	TestTrue(TEXT("不同宏观区域不会误合并为同一正文"), Neighbor.IsValid() && Neighbor != First && Neighbor->Key != First->Key);
	Cache->Reset();
	TestTrue(TEXT("世界重置不会破坏旧读者正文"), First->Fingerprint == Fingerprint && First->Generation == Generation && Cache->GetHydrologyGeneration() != Generation);
	bSupplyInput = false;
	TSharedPtr<const FVoxelHydrologyRiverComponent, ESPMode::ThreadSafe> Fresh;
	Cache->AdvanceHydrologyComponent(Key, Config.ToSharedRef(), Lookup, Request, Fresh, Error);
	TestTrue(TEXT("新代不能借用旧代组件"), !Fresh && Error.StartsWith(TEXT("Yielded")));
	First.Reset();
	Second.Reset();
	Neighbor.Reset();
	Cache.Reset();
	Config.Reset();
	const double Deadline = FPlatformTime::Seconds() + 2.0;
	while (Resources->Snapshot().LeaseCount && FPlatformTime::Seconds() < Deadline) FPlatformProcess::Sleep(0.001f);
	TestEqual(TEXT("组件和世界重置后原账本租约全部归还"), Resources->Snapshot().LeaseCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologySharedRegionalComponentCancellationTest,
	"WHFramework.Voxel.Hydrology.GraphCache.SharedRegionalComponentCancellationAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologySharedRegionalComponentCancellationTest::RunTest(const FString& Parameters)
{
	using namespace VoxelHydrologyGraphCacheTests;
	(void)Parameters;
	FString Error;
	const auto Producer = FVoxelGenerationConsumerKey::Section(422, FGuid::NewGuid(), 1, {}, 1);
	FVoxelGenerationConsumerScope ProducerIdentity(Producer);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	if (!Setup(Producer, Resources, Config, Cache, Error, true)) { AddError(Error); return false; }
	const FVoxelHydrologyRegionKey Key{FIntPoint(-1, -1)};
	const auto Lookup = [](const FVoxelHydrologyRegionKey&) -> const FVoxelHydrologyPlan* { return nullptr; };
	int32 Requests = 0;
	bool bRetireProducer = false;
	bool bProducerRetired = false;
	const auto Request = [&](const FVoxelHydrologyRegionKey&, FString& OutError)
	{
		++Requests;
		if (bRetireProducer && !bProducerRetired)
		{
			bProducerRetired = true;
			Cache->CancelHydrologyBuild(Key, Producer.WorldEpoch, Producer.OwnerId, Producer.OwnerIncarnation, Producer.RequestSerial);
		}
		OutError = TEXT("Yielded: canceled component fixture awaits input");
		return false;
	};
	TSharedPtr<const FVoxelHydrologyRiverComponent, ESPMode::ThreadSafe> Component;
	const auto InitialState = Resources->Snapshot();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Pressure;
	const uint64 ResidentLimit = InitialState.Limits.ResidentBytes - InitialState.Limits.OverlapBytes;
	if (!Resources->Acquire(Producer, EVoxelTerrainResourcePool::Resident,
		ResidentLimit - InitialState.Used[int32(EVoxelTerrainResourcePool::Resident)] - 1, Pressure)) return false;
	TestFalse(TEXT("发布驻留预授额不足时不能启动伪就绪组件"), Cache->AdvanceHydrologyComponent(Key, Config.ToSharedRef(), Lookup, Request, Component, Error));
	TestTrue(TEXT("驻留容量等待可重试而非终端错误"), !Component && Error.StartsWith(TEXT("CapacityBlocked")));
	Pressure.Reset();
	for (int32 Attempt = 0; Attempt < 100 && !Requests; ++Attempt)
		Cache->AdvanceHydrologyComponent(Key, Config.ToSharedRef(), Lookup, Request, Component, Error);
	if (!TestTrue(TEXT("原生产者进入真实缺输入等待"), Requests > 0 && !Component)) return false;
	auto Reader = Producer;
	Reader.OwnerId = FGuid::NewGuid();
	++Reader.RequestSerial;
	{
		FVoxelGenerationConsumerScope ReaderIdentity(Reader);
		TAtomic<bool> Cancel{false};
		bRetireProducer = true;
		TestFalse(TEXT("原生产者取消后有效读者重新接纳"), Cache->AdvanceHydrologyComponent(Key, Config.ToSharedRef(), Lookup, Request, Component, Error, &Cancel));
		TestTrue(TEXT("有效读者不被连带取消也不读取半成品"), bProducerRetired && !Component && Error.StartsWith(TEXT("Yielded")) && !Cancel.Load());
		TestFalse(TEXT("原生产者退休后有效读者可重新请求同区域"), Cache->AdvanceHydrologyComponent(Key, Config.ToSharedRef(), Lookup, Request, Component, Error, &Cancel));
		TestTrue(TEXT("重新接纳仍真实等待拓扑输入"), !Component && Error.StartsWith(TEXT("Yielded")));
		Cancel.Store(true);
		Cache->AdvanceHydrologyComponent(Key, Config.ToSharedRef(), Lookup, Request, Component, Error, &Cancel);
		TestEqual(TEXT("读者自身取消明确返回取消"), Error, FString(TEXT("Canceled")));
	}
	Cache->Reset();
	Cache.Reset();
	Config.Reset();
	const double Deadline = FPlatformTime::Seconds() + 2.0;
	while (Resources->Snapshot().LeaseCount && FPlatformTime::Seconds() < Deadline) FPlatformProcess::Sleep(0.001f);
	TestEqual(TEXT("取消及重置不会遗留组件或输入租约"), Resources->Snapshot().LeaseCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyExteriorSnapshotLifecycleTest,
	"WHFramework.Voxel.Hydrology.GraphCache.ExteriorSnapshotLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyExteriorSnapshotLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace VoxelHydrologyGraphCacheTests;
	(void)Parameters;
	FString Error;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(423, FGuid::NewGuid(), 1, {}, 1);
	FVoxelGenerationConsumerScope Identity(Consumer);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	if (!Setup(Consumer, Resources, Config, Cache, Error, true)) { AddError(Error); return false; }
	FVoxelGenerationCacheRetention Retention;
	Retention.Revision = 1;
	Cache->UpdateRetention(Retention);
	TMap<FVoxelHydrologyRegionKey, TSharedPtr<FVoxelHydrologyPlan, ESPMode::ThreadSafe>> Topology;
	const auto Lookup = [&](const FVoxelHydrologyRegionKey& Key) -> const FVoxelHydrologyPlan*
	{
		if (const auto* Existing = Topology.Find(Key)) return Existing->Get();
		const int64 MinimumX = int64(Key.Coordinate.X) * Side;
		const int64 MinimumY = int64(Key.Coordinate.Y) * Side;
		if (MinimumX < MIN_int32 || MinimumY < MIN_int32 ||
			MinimumX + Side - 1 > MAX_int32 || MinimumY + Side - 1 > MAX_int32) return nullptr;
		const auto Plan = MakeShared<FVoxelHydrologyPlan, ESPMode::ThreadSafe>();
		auto& Grid = Plan->Grid;
		Grid.WorldMinCell = FIntPoint(int32(MinimumX), int32(MinimumY));
		Grid.Width = Side;
		Grid.Height = Side;
		Grid.CellSize = 32;
		Grid.GroundPlane.SetNum(Side * Side);
		Grid.RainWeight.Init(3, Side * Side);
		for (int32 Index = 0; Index < Side * Side; ++Index)
		{
			const FIntPoint Cell = Grid.ToWorldHydrologyCell(Index);
			const bool bWet = Cell.X <= -2 * Side || Cell.Y <= -2 * Side ||
				Cell.X >= 2 * Side - 1 || Cell.Y >= 2 * Side - 1;
			Grid.GroundPlane[Index] = bWet ? -4 : 8 + 4 * (2 * Side - 1 - Cell.X) + 32 * FMath::Abs(Cell.Y);
			if (bWet) Grid.OceanOutlets.Add(Index, 0);
		}
		Topology.Add(Key, Plan);
		return &Plan.Get();
	};
	const auto Request = [](const FVoxelHydrologyRegionKey& Key, FString& OutError)
	{
		OutError = FString::Printf(TEXT("Yielded: fixture requires unavailable topology (%d,%d)"), Key.Coordinate.X, Key.Coordinate.Y);
		return false;
	};
	struct FBuilder
	{
		TSharedPtr<FVoxelHydrologyComponentBuild, ESPMode::ThreadSafe> Body;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Resident;
	};
	const uint64 RootBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelHydrologyComponentBuild>();
	FIntPoint Seed(-Side + 1, -Side + 1);
	const auto Create = [&](FBuilder& Out)
	{
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, RootBytes, Out.Working) ||
			!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident, RootBytes, Out.Resident)) return false;
		Out.Body = MakeShared<FVoxelHydrologyComponentBuild, ESPMode::ThreadSafe>(Config.ToSharedRef(), *Cache, Seed);
		return true;
	};
	const auto Advance = [&](FBuilder& InBuilder, const TAtomic<bool>* Cancel = nullptr)
	{
		FVoxelGenerationRequiredSourceScope Sources(Resources.Get(), InBuilder.Body.Get(),
			[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
			{
				static_cast<const FVoxelHydrologyComponentBuild*>(Context)->VisitSources(Visit);
			});
		FVoxelGenerationAllocationScope Allocation(Resources, Consumer, InBuilder.Working, InBuilder.Resident,
			RootBytes, InBuilder.Body->GetAllocatedBytes());
		if (!Allocation.Check(Error)) return EVoxelHydrologyComponentStep::Failed;
		const auto Step = InBuilder.Body->Advance(Lookup, 256, Error, Cancel);
		if (Step != EVoxelHydrologyComponentStep::CapacityBlocked &&
			!Allocation.ValidateRetainedBytes(InBuilder.Body->GetAllocatedBytes(), Error)) return EVoxelHydrologyComponentStep::Failed;
		return Step;
	};
	const auto AwaitSnapshot = [&](FBuilder& InBuilder)
	{
		FVoxelHydrologyExteriorTestAccess::Pause(*InBuilder.Body);
		for (int32 Attempt = 0; Attempt < 300000; ++Attempt)
		{
			const auto Step = Advance(InBuilder);
			if (FVoxelHydrologyExteriorTestAccess::HasSnapshot(*InBuilder.Body))
				return Step == EVoxelHydrologyComponentStep::Yielded;
			if (Step != EVoxelHydrologyComponentStep::Yielded) return false;
		}
		return false;
	};
	const auto Finish = [&](FBuilder& InBuilder)
	{
		for (int32 Attempt = 0; Attempt < 300000; ++Attempt)
		{
			const auto Step = Advance(InBuilder);
			if (Step == EVoxelHydrologyComponentStep::Ready) return true;
			if (Step != EVoxelHydrologyComponentStep::Yielded) return false;
		}
		return false;
	};
	FBuilder Tested;
	bool bFoundExterior = false;
	for (const FIntPoint Candidate : {FIntPoint(-Side + 1, -Side + 1), FIntPoint(-Side + 1, 1),
		FIntPoint(1, -Side + 1), FIntPoint(1, 1)})
	{
		Seed = Candidate;
		if (!Create(Tested)) return false;
		if (AwaitSnapshot(Tested))
		{
			bFoundExterior = true;
			break;
		}
		if (!Error.IsEmpty()) { AddError(Error); return false; }
		Tested = {};
	}
	if (!TestTrue(TEXT("真实跨区出口已读取汇水并在渠道前暂停"), bFoundExterior)) return false;
	const FIntPoint Cell = FVoxelHydrologyExteriorTestAccess::Cell(*Tested.Body);
	const FIntPoint Coordinate = FVoxelHydrologyExteriorTestAccess::Coordinate(*Tested.Body);
	const uint64 Fingerprint = FVoxelHydrologyExteriorTestAccess::Fingerprint(*Tested.Body);
	TSharedPtr<const FVoxelHydrologyNetworkRunoff, ESPMode::ThreadSafe> Runoff;
	if (!TestTrue(TEXT("暂停快照对应正式缓存发布的外区汇水"),
		Cache->AdvanceHydrologyRunoff({Coordinate}, Config.ToSharedRef(), Lookup, Request, Runoff, Error) &&
		Runoff.IsValid() && FVoxelHydrologyExteriorTestAccess::Matches(*Tested.Body, *Runoff))) return false;
	TWeakPtr<const FVoxelHydrologyNetworkRunoff, ESPMode::ThreadSafe> Retired = Runoff;
	Runoff.Reset();
	const auto State = Resources->Snapshot();
	const uint64 Limit = State.Limits.ResidentBytes - State.Limits.OverlapBytes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Pressure;
	if (!TestTrue(TEXT("在原驻留预算中施加真实压力"), State.Used[int32(EVoxelTerrainResourcePool::Resident)] + 1 < Limit &&
		Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident,
			Limit - State.Used[int32(EVoxelTerrainResourcePool::Resident)] - 1, Pressure))) return false;
	Maintain(*Cache);
	if (!TestTrue(TEXT("无外部读者的原汇水正文在压力下真实退休"), !Retired.IsValid())) return false;
	if (!TestTrue(TEXT("退休不改变当前出口和冻结指纹"), FVoxelHydrologyExteriorTestAccess::HasSnapshot(*Tested.Body) &&
		FVoxelHydrologyExteriorTestAccess::Cell(*Tested.Body) == Cell &&
		FVoxelHydrologyExteriorTestAccess::Fingerprint(*Tested.Body) == Fingerprint)) return false;
	Pressure.Reset();
	if (!TestTrue(TEXT("实际缓存退休后继续完整生成"), Finish(Tested))) { AddError(Error); return false; }
	FBuilder Reference;
	if (!Create(Reference) || !TestTrue(TEXT("参考组件完整生成"), Finish(Reference))) { AddError(Error); return false; }
	TestTrue(TEXT("跨片快照与参考组件的水位、河床、曲线和完整指纹一致"),
		Tested.Body->GetResult().HasEquivalentOutput(Reference.Body->GetResult()));

	FBuilder Conflict;
	if (!Create(Conflict) || !TestTrue(TEXT("身份冲突注入前进入真实出口读取"), AwaitSnapshot(Conflict))) { AddError(Error); return false; }
	FVoxelHydrologyExteriorTestAccess::CorruptFingerprint(*Conflict.Body);
	bool bConflict = false;
	for (int32 Attempt = 0; Attempt < 300000; ++Attempt)
	{
		const auto Step = Advance(Conflict);
		if (Step == EVoxelHydrologyComponentStep::Failed)
		{
			bConflict = Error == TEXT("HydrologyComponentConflict: exterior channel and runoff identities differ");
			break;
		}
		if (Step != EVoxelHydrologyComponentStep::Yielded) break;
	}
	TestTrue(TEXT("注入的渠道与汇水身份冲突拒绝发布"), bConflict);
	FBuilder Canceled;
	if (!Create(Canceled) || !TestTrue(TEXT("取消前真实进入出口等待"), AwaitSnapshot(Canceled))) { AddError(Error); return false; }
	TAtomic<bool> Cancel{true};
	TestTrue(TEXT("等待中的原消费者取消不发布正文"), Advance(Canceled, &Cancel) == EVoxelHydrologyComponentStep::Canceled);
	FBuilder Reset;
	if (!Create(Reset) || !TestTrue(TEXT("换代前真实进入出口等待"), AwaitSnapshot(Reset))) { AddError(Error); return false; }
	Cache->Reset();
	TestTrue(TEXT("换代后的出口快照不能继续发布"), Advance(Reset) == EVoxelHydrologyComponentStep::Canceled);
	Tested = {};
	Reference = {};
	Conflict = {};
	Canceled = {};
	Reset = {};
	Retired.Reset();
	Cache.Reset();
	Config.Reset();
	const double Deadline = FPlatformTime::Seconds() + 2.0;
	while (Resources->Snapshot().LeaseCount && FPlatformTime::Seconds() < Deadline) FPlatformProcess::Sleep(0.001f);
	TestEqual(TEXT("所有原始工作、驻留和依赖租约均归还"), Resources->Snapshot().LeaseCount, 0);
	return true;
}

#endif
