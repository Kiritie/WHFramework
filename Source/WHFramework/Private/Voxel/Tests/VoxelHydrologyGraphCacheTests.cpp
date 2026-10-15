#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyNetworkSearch.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Generation/VoxelGenerationRecipeCodec.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

namespace VoxelHydrologyGraphCacheTests
{
	constexpr int32 Side = 8;

	bool Setup(const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& OutResources,
		TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>& OutConfig,
		TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>& OutCache, FString& OutError)
	{
		FVoxelGenerationRuntimeConfig Author = *VoxelTest::MakeGenerationConfig(407, FVoxelGenerationRecipe::CurrentAlgorithmVersion);
		FVoxelGenerationRecipe Recipe = *Author.Recipe;
		Recipe.Settings.HydrologyCellSize = 32;
		Recipe.Settings.HydrologyRegionSide = Side;
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
		if (!Allocation.Check(Error) || !Search->Initialize(FIntPoint(-7, -7), 32, Side, Cache->GetHydrologyGeneration(), Error)) return false;
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

#endif
