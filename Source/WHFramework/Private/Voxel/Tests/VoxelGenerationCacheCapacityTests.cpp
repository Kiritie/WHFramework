#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Ecology/VoxelEcology.h"
#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Generation/Climate/VoxelClimateGenerator.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Generation/VoxelGenerationPlanningRead.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "VoxelTestUtilities.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"

namespace
{
	FVoxelGenerationConsumerKey MakeCacheControlConsumer()
	{
		return {EVoxelGenerationConsumerKind::WorldFact, 41, FGuid::NewGuid(), 1,
			EVoxelGenerationSpatialKind::Column, FIntVector::ZeroValue, FIntVector::ZeroValue, 1};
	}

	class FCacheCapacityCommand : public IAutomationLatentCommand
	{
		enum class EPhase : uint8
		{
			None = 0,
			Check,
			Retire
		};

	public:
		explicit FCacheCapacityCommand(FAutomationTestBase& InTest)
			: Test(InTest)
		{
		}

		virtual ~FCacheCapacityCommand()
		{
		}

		virtual bool Update() override
		{
			if (Phase == EPhase::Check)
			{
				CheckFixture();
				ReleaseFixture();
				Cache.Reset();
				RetirementDeadline = FPlatformTime::Seconds() + 8.0;
				Phase = EPhase::Retire;
			}
			ObserveRetirement();
			const auto State = Resources->Snapshot();
			if (State.LeaseCount == 0)
			{
				Test.TestEqual(TEXT("实际后台删除完成后 Working 全部归还"),
					State.Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
				Test.TestEqual(TEXT("实际后台删除完成后 Resident 全部归还"),
					State.Used[int32(EVoxelTerrainResourcePool::Resident)], uint64(0));
				Test.TestEqual(TEXT("控制 backing、正文及句柄全部退休"),
					State.Used[int32(EVoxelTerrainResourcePool::Input)], State.LedgerMetadataBytes);
				return true;
			}
			if (FPlatformTime::Seconds() >= RetirementDeadline)
			{
				Test.AddError(TEXT("Cache capacity fixture 的真实后台释放超过 8 秒"));
				return true;
			}
			// GT 只观察账本；控制器和正文由真实 worker deleter 退出，不 Wait 或 join。
			return false;
		}

	protected:
		virtual void CheckFixture() = 0;

		virtual void ReleaseFixture()
		{
		}

		virtual void ObserveRetirement()
		{
		}

		FAutomationTestBase& Test;
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources =
			MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
		TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
		FString Error;

	private:
		EPhase Phase = EPhase::Check;
		double RetirementDeadline = 0.0;
	};

	class FCacheFactoryRefusalCommand final : public FCacheCapacityCommand
	{
	public:
		explicit FCacheFactoryRefusalCommand(FAutomationTestBase& InTest)
			: FCacheCapacityCommand(InTest)
		{
		}

	protected:
		virtual void CheckFixture() override
		{
			const auto ControlConsumer = MakeCacheControlConsumer();
			if (!Test.TestTrue(TEXT("原资源域创建真实生产 Cache"), Resources->Configure({}) &&
				FVoxelGenerationPlanCache::Create(Resources, ControlConsumer, Cache, Error, true, false)))
			{
				return;
			}
			FVoxelGenerationPlanCache* Original = Cache.Get();
			const auto DeniedResources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
			FVoxelTerrainResourceLimits DeniedLimits;
			DeniedLimits.InputBytes = 1;
			if (!Test.TestTrue(TEXT("建立 Input 不足的独立域"), DeniedResources->Configure(DeniedLimits)))
			{
				return;
			}
			const auto Before = DeniedResources->Snapshot();
			Test.TestFalse(TEXT("根及 shards 未授予时 Create 拒收"),
				FVoxelGenerationPlanCache::Create(DeniedResources, ControlConsumer, Cache, Error, true, false));
			const auto After = DeniedResources->Snapshot();
			Test.TestTrue(TEXT("拒收明确报告容量"), Error.StartsWith(TEXT("CapacityBlocked")));
			if (!Test.TestTrue(TEXT("拒收保留调用方已存在的 Cache"), Cache.Get() == Original))
			{
				return;
			}
			Test.TestTrue(TEXT("已有 Cache 仍绑定原账本"), Cache->GetResources().Get() == &Resources.Get());
			Test.TestTrue(TEXT("已有 Cache 保留完整控制身份"), Cache->GetControlConsumer() == ControlConsumer);
			Test.TestEqual(TEXT("拒收没有先分配账本 backing"), After.LedgerMetadataBytes, uint64(0));
			Test.TestEqual(TEXT("拒收没有根租约或句柄"), After.LeaseCount, 0);
			Test.TestEqual(TEXT("拒收没有 Input 授额"), After.Used[int32(EVoxelTerrainResourcePool::Input)], uint64(0));
			Test.TestEqual(TEXT("拒收不改变账本版本"), After.Version, Before.Version);
			Test.TestEqual(TEXT("拒收不伪造容量释放"), After.ReleaseVersion, Before.ReleaseVersion);

			auto TaskConsumer = ControlConsumer;
			TaskConsumer.OwnerId = FGuid::NewGuid();
			TaskConsumer.SpatialKind = EVoxelGenerationSpatialKind::Section;
			TaskConsumer.RequestSerial = 19;
			FVoxelGenerationCacheRetentionPoint Point;
			if (!Test.TestTrue(TEXT("原域及 World 创建真实任务 retention"),
				Cache->RetainForTask(Point, Resources, TaskConsumer, Retention, Error)))
			{
				return;
			}
			const auto* OriginalRetention = Retention.Get();
			const auto ForeignResources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
			if (!Test.TestTrue(TEXT("其它账本有足够容量，拒收必须来自原域校验"), ForeignResources->Configure({})))
			{
				return;
			}
			const auto OriginalBefore = Resources->Snapshot();
			const auto ForeignBefore = ForeignResources->Snapshot();
			Test.TestFalse(TEXT("生产 Cache 拒绝其它 ledger 的 retention"),
				Cache->RetainForTask(Point, ForeignResources, TaskConsumer, Retention, Error));
			Test.TestTrue(TEXT("跨域拒收保留已有 retention"), Retention.Get() == OriginalRetention);
			Test.TestEqual(TEXT("跨域拒收不向其它账本授额"), ForeignResources->Snapshot().Version, ForeignBefore.Version);
			auto OtherWorld = TaskConsumer;
			++OtherWorld.WorldEpoch;
			Test.TestFalse(TEXT("生产 Cache 拒绝其它 World 的 retention"),
				Cache->RetainForTask(Point, Resources, OtherWorld, Retention, Error));
			Test.TestTrue(TEXT("跨 World 拒收保留已有 retention"), Retention.Get() == OriginalRetention);
			Test.TestEqual(TEXT("两种拒收均不改变原账本"), Resources->Snapshot().Version, OriginalBefore.Version);
		}

		virtual void ReleaseFixture() override
		{
			Retention.Reset();
		}

	private:
		TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> Retention;
	};

	template<typename ValueType>
	struct TCacheColumnFamily;

	template<>
	struct TCacheColumnFamily<FVoxelBaseColumnEntry>
	{
		using Ptr = FVoxelBaseColumnEntryPtr;

		static const TCHAR* Name()
		{
			return TEXT("Base");
		}

		static bool Get(FVoxelGenerationPlanCache& InCache, const FIntPoint& InKey,
			TFunctionRef<bool(FVoxelBaseColumnEntry&, FString&)> InBuild,
			Ptr& OutValue, FString& OutError, const TAtomic<bool>* InCancel)
		{
			return InCache.GetOrBuildBaseColumn(InKey, InBuild, OutValue, OutError, InCancel);
		}
	};

	template<>
	struct TCacheColumnFamily<FVoxelNaturalColumnEntry>
	{
		using Ptr = FVoxelNaturalColumnEntryPtr;

		static const TCHAR* Name()
		{
			return TEXT("Natural");
		}

		static bool Get(FVoxelGenerationPlanCache& InCache, const FIntPoint& InKey,
			TFunctionRef<bool(FVoxelNaturalColumnEntry&, FString&)> InBuild,
			Ptr& OutValue, FString& OutError, const TAtomic<bool>* InCancel)
		{
			return InCache.GetOrBuildNaturalColumn(InKey, InBuild, OutValue, OutError, InCancel);
		}
	};

	template<typename ValueType>
	class TCacheColumnCapacityCommand final : public FCacheCapacityCommand
	{
		using Family = TCacheColumnFamily<ValueType>;
		using Ptr = typename Family::Ptr;

	public:
		explicit TCacheColumnCapacityCommand(FAutomationTestBase& InTest)
			: FCacheCapacityCommand(InTest)
		{
		}

	protected:
		virtual void CheckFixture() override
		{
			Test.AddInfo(FString::Printf(TEXT("真实生产 Cache 正文容量族：%s"), Family::Name()));
			const uint64 BodyBytes = VoxelBoundedAllocation::WorkerSharedBytes<const ValueType>();
			FVoxelTerrainResourceLimits Limits;
			Limits.WorkingBytes = BodyBytes;
			Limits.ResidentBytes = BodyBytes * 3;
			Limits.OverlapBytes = BodyBytes;
			const auto ControlConsumer = MakeCacheControlConsumer();
			if (!Test.TestTrue(TEXT("生产工厂绑定受限 Working/Resident 原账本"), Resources->Configure(Limits) &&
				FVoxelGenerationPlanCache::Create(Resources, ControlConsumer, Cache, Error, true, false)))
			{
				return;
			}

			const FIntPoint WarmKey(0, 0);
			FIntPoint ColdKey;
			bool bFoundSameShard = false;
			for (int32 X = 1; X <= 4096; ++X)
			{
				const FIntPoint Candidate(X, 0);
				if (GetTypeHash(Candidate) % FVoxelGenerationPlanCache::ShardCount ==
					GetTypeHash(WarmKey) % FVoxelGenerationPlanCache::ShardCount)
				{
					ColdKey = Candidate;
					bFoundSameShard = true;
					break;
				}
			}
			if (!Test.TestTrue(TEXT("暖冷 key 使用同一 shard 的已授予控制 backing"), bFoundSameShard))
			{
				return;
			}
			FVoxelTaskStamp Stamp;
			Stamp.OwnerId = FGuid::NewGuid();
			Stamp.OwnerIncarnation = 1;
			Stamp.WorldEpoch = ControlConsumer.WorldEpoch;
			Stamp.Section = FIntVector(ColdKey.X, ColdKey.Y, 0);
			Stamp.Token = 19;
			const FVoxelGenerationConsumerKey Consumer{EVoxelGenerationConsumerKind::WorldFact,
				Stamp.WorldEpoch, Stamp.OwnerId, Stamp.OwnerIncarnation, EVoxelGenerationSpatialKind::Section,
				Stamp.Section, Stamp.Section, Stamp.Token};
			TAtomic<bool> Cancel{false};
			FVoxelGenerationPlanningReadScope Scope({}, &Stamp, &Cancel, &Consumer);
			Test.TestTrue(TEXT("TLS 使用完整原任务 stamp"), FVoxelGenerationPlanningReadScope::GetTaskStamp() == &Stamp);
			Test.TestTrue(TEXT("TLS 使用稳定完整 consumer"), FVoxelGenerationPlanningReadScope::GetConsumerKey() == &Consumer);
			int32 WarmBuilds = 0;
			if (!Test.TestTrue(TEXT("暖正文真实构建成功"), Family::Get(*Cache, WarmKey,
				[&](ValueType& OutValue, FString& BuildError)
				{
					++WarmBuilds;
					const auto BeforeBuild = Resources->Snapshot();
					Test.TestEqual(TEXT("callback 前已授予完整 Working 正文"),
						BeforeBuild.Used[int32(EVoxelTerrainResourcePool::Working)], BodyBytes);
					Test.TestEqual(TEXT("callback 前已预留完整 Resident standby"),
						BeforeBuild.Used[int32(EVoxelTerrainResourcePool::Resident)], BodyBytes);
					OutValue.Position = WarmKey;
					OutValue.Column.SurfaceZ = 61;
					BuildError.Reset();
					return true;
				}, Warmed, Error, &Cancel)))
			{
				return;
			}
			Test.TestEqual(TEXT("暖正文仅构建一次"), WarmBuilds, 1);
			WeakWarmed = Warmed;
			bWeakRetirementPending = true;
			Test.TestEqual(TEXT("发布后 Working 已归还"),
				Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
			Test.TestEqual(TEXT("暖正文固定额度只留 Resident"),
				Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Resident)], BodyBytes);

			FVoxelTaskStamp InvalidStamps[] = {Stamp, Stamp, Stamp, Stamp};
			InvalidStamps[0].OwnerId = FGuid();
			InvalidStamps[1].OwnerIncarnation = 0;
			InvalidStamps[2].Token = 0;
			++InvalidStamps[3].WorldEpoch;
			const FIntPoint RejectedKeys[] = {WarmKey, ColdKey};
			for (const auto& InvalidStamp : InvalidStamps)
			{
				// 完整 logical consumer 不能掩盖缺失或跨 World 的原任务 stamp。
				FVoxelGenerationPlanningReadScope InvalidScope({}, &InvalidStamp, &Cancel, &Consumer);
				for (const auto& RejectedKey : RejectedKeys)
				{
					Ptr InvalidOutput = Warmed;
					int32 InvalidBuilds = 0;
					Test.TestFalse(TEXT("无效原任务 stamp 在暖读取及冷构建前拒收"),
						Family::Get(*Cache, RejectedKey,
							[&](ValueType&, FString&)
							{
								++InvalidBuilds;
								return true;
							}, InvalidOutput, Error, &Cancel));
					Test.TestEqual(TEXT("无效 stamp 不执行 callback"), InvalidBuilds, 0);
					Test.TestTrue(TEXT("无效 stamp 明确报告身份拒收"), Error.StartsWith(TEXT("InvalidConsumer")));
					Test.TestTrue(TEXT("无效 stamp 拒收保留调用方旧 Out"), InvalidOutput == Warmed);
				}
			}
			Test.TestTrue(TEXT("嵌套无效 scope 退出后恢复原任务 stamp"),
				FVoxelGenerationPlanningReadScope::GetTaskStamp() == &Stamp);
			Test.TestTrue(TEXT("嵌套无效 scope 退出后恢复原 consumer"),
				FVoxelGenerationPlanningReadScope::GetConsumerKey() == &Consumer);

			auto BlockerConsumer = Consumer;
			BlockerConsumer.OwnerId = FGuid::NewGuid();
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
			if (!Test.TestTrue(TEXT("不同 consumer 占满剩余 Resident"),
				Resources->Acquire(BlockerConsumer, EVoxelTerrainResourcePool::Resident, BodyBytes, Blocker)))
			{
				return;
			}
			const auto BeforeFailure = Resources->Snapshot();
			const uint64 ExternalBefore = Resources->ReleaseVersionExcludingOwner(Consumer);
			const uint64 CacheBytesBefore = Cache->GetAllocatedBytes();
			Ptr Output = Warmed;
			int32 ColdBuilds = 0;
			Test.TestFalse(TEXT("Resident 满时冷建在 callback 前拒收"), Family::Get(*Cache, ColdKey,
				[&](ValueType&, FString&)
				{
					++ColdBuilds;
					return true;
				}, Output, Error, &Cancel));
			const auto AfterFailure = Resources->Snapshot();
			Test.TestEqual(TEXT("拒收没有执行冷建 callback"), ColdBuilds, 0);
			Test.TestTrue(TEXT("冷建拒收报告容量"), Error.StartsWith(TEXT("CapacityBlocked")));
			Test.TestTrue(TEXT("冷建拒收保留调用方旧 Out"), Output == Warmed);
			Test.TestEqual(TEXT("失败前授予的 Working 真实归还"),
				AfterFailure.Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
			Test.TestEqual(TEXT("失败不改变 Resident 暖正文和 blocker"),
				AfterFailure.Used[int32(EVoxelTerrainResourcePool::Resident)], BodyBytes * 2);
			Test.TestEqual(TEXT("同 shard 的拒收不扩容账本"), AfterFailure.LedgerMetadataBytes, BeforeFailure.LedgerMetadataBytes);
			Test.TestEqual(TEXT("拒收不增长 Cache 控制存储或正文"), Cache->GetAllocatedBytes(), CacheBytesBefore);
			Test.TestEqual(TEXT("完整 TLS consumer 的失败退款不自唤醒"),
				Resources->ReleaseVersionExcludingOwner(Consumer), ExternalBefore);
			int32 UnexpectedWarmBuilds = 0;
			Ptr WarmAgain;
			Test.TestTrue(TEXT("Resident 满时既有暖正文继续可读"), Family::Get(*Cache, WarmKey,
				[&](ValueType&, FString&)
				{
					++UnexpectedWarmBuilds;
					return false;
				}, WarmAgain, Error, &Cancel));
			Test.TestEqual(TEXT("暖读取未重建"), UnexpectedWarmBuilds, 0);
			Test.TestTrue(TEXT("暖 immutable 身份保持"), WarmAgain == Warmed);

			Blocker.Reset();
			Test.TestTrue(TEXT("不同 consumer 的真实释放唤醒原尝试"),
				Resources->ReleaseVersionExcludingOwner(Consumer) > ExternalBefore);
			const bool bRetried = Family::Get(*Cache, ColdKey,
				[&](ValueType& OutValue, FString& BuildError)
				{
					++ColdBuilds;
					const auto BeforeBuild = Resources->Snapshot();
					Test.TestEqual(TEXT("重试 callback 前重新授予 Working"),
						BeforeBuild.Used[int32(EVoxelTerrainResourcePool::Working)], BodyBytes);
					Test.TestEqual(TEXT("重试 callback 前保留暖正文及冷 Resident standby"),
						BeforeBuild.Used[int32(EVoxelTerrainResourcePool::Resident)], BodyBytes * 2);
					OutValue.Position = ColdKey;
					OutValue.Column.SurfaceZ = 93;
					BuildError.Reset();
					return true;
				}, Output, Error, &Cancel);
			Test.TestTrue(TEXT("相同原任务重试成功"), bRetried);
			Test.TestEqual(TEXT("只有成功重试执行一次冷建"), ColdBuilds, 1);
			if (bRetried && Output)
			{
				Test.TestTrue(TEXT("成功提交新 immutable Out"), Output != Warmed && Output->Position == ColdKey);
				Test.TestEqual(TEXT("新正文保留 callback 内容"), Output->Column.SurfaceZ, 93);
			}
			Test.TestEqual(TEXT("成功发布没有重复授予 Resident"),
				Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Resident)], BodyBytes * 2);
			Test.TestEqual(TEXT("成功发布归还全部 Working"),
				Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
		}

		virtual void ReleaseFixture() override
		{
			Warmed.Reset();
		}

		virtual void ObserveRetirement() override
		{
			if (bWeakRetirementPending && !WeakWarmed.IsValid())
			{
				const auto State = Resources->Snapshot();
				if (State.LeaseCount > 1)
				{
					return;
				}
				Test.TestEqual(TEXT("其它后台正文和控制存储已退休，仅剩暖 weak 控制器原租约"), State.LeaseCount, 1);
				Test.TestEqual(TEXT("最后 weak 独立持有暖正文的完整 Resident 额度"),
					State.Used[int32(EVoxelTerrainResourcePool::Resident)],
					VoxelBoundedAllocation::WorkerSharedBytes<const ValueType>());
				WeakWarmed.Reset();
				bWeakRetirementPending = false;
			}
		}

	private:
		Ptr Warmed;
		TWeakPtr<const ValueType, ESPMode::ThreadSafe> WeakWarmed;
		bool bWeakRetirementPending = false;
	};

	class FQueryGeneratorCapacityCommand final : public FCacheCapacityCommand
	{
	public:
		explicit FQueryGeneratorCapacityCommand(FAutomationTestBase& InTest)
			: FCacheCapacityCommand(InTest)
		{
		}

	protected:
		virtual void CheckFixture() override
		{
			const auto Control = MakeCacheControlConsumer();
			if (!Test.TestTrue(TEXT("Query 使用真实生产工厂与原账本"), Resources->Configure({}) &&
				FVoxelGenerationPlanCache::Create(Resources, Control, Cache, Error, true, false))) return;
			auto Consumer = Control;
			Consumer.OwnerId = FGuid::NewGuid();
			Consumer.RequestSerial = 81;
			const auto Config = VoxelTest::MakeGenerationConfig();
			const auto BeforeQuery = Resources->Snapshot();
			if (!Test.TestTrue(TEXT("八个原生生成器全部授予后 Query 构造"),
				FVoxelGenerationQuery::Create(Config, Cache.ToSharedRef(), Query, Error, false, false, false, &Consumer))) return;
			Test.TestEqual(TEXT("每个真实生成器各有一个租约"), Resources->Snapshot().LeaseCount, BeforeQuery.LeaseCount + 8);
			if (!Test.TestTrue(TEXT("准备旧 Query 的列范围"), Query.PrepareColumns(
				{FIntVector(0, 0, -64), FIntVector(1, 1, 128)}, Error))) return;

			FVoxelTaskStamp Stamp;
			Stamp.WorldEpoch = Consumer.WorldEpoch;
			Stamp.OwnerId = Consumer.OwnerId;
			Stamp.OwnerIncarnation = Consumer.OwnerIncarnation;
			Stamp.Token = Consumer.RequestSerial;
			FVoxelGenerationPlanningReadScope Scope({}, &Stamp, nullptr, &Consumer);
			FVoxelColumnSample Original;
			if (!Test.TestTrue(TEXT("原 Query 沿真实水文链采样"), Query.SampleColumn(0, 0, Original, Error))) return;
			auto Foreign = Consumer;
			++Foreign.WorldEpoch;
			const auto BeforeForeign = Resources->Snapshot();
			Test.TestFalse(TEXT("Query 构造拒绝跨 World 的完整 consumer"),
				FVoxelGenerationQuery::Create(Config, Cache.ToSharedRef(), Query, Error, false, false, false, &Foreign));
			Test.TestEqual(TEXT("身份拒收发生在所有 native grant 之前"), Resources->Snapshot().Version, BeforeForeign.Version);

			auto BlockerConsumer = Consumer;
			BlockerConsumer.OwnerId = FGuid::NewGuid();
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
			if (!Test.TestTrue(TEXT("其它消费者接纳 Input 压力"),
				Resources->Acquire(BlockerConsumer, EVoxelTerrainResourcePool::Input, 1, Blocker))) return;
			const auto BeforeFill = Resources->Snapshot();
			const uint64 LeaveBytes = VoxelBoundedAllocation::WorkerSharedBytes<const FVoxelClimateGenerator>() +
				VoxelBoundedAllocation::WorkerSharedBytes<const FVoxelTerrainGenerator>() +
				2 * VoxelBoundedAllocation::SharedBytes<FVoxelTerrainResourceLease>();
			const uint64 Available = BeforeFill.Limits.InputBytes - BeforeFill.Used[int32(EVoxelTerrainResourcePool::Input)];
			if (!Test.TestTrue(TEXT("留给部分生成器授予，但不足八个完整对象"), Available > LeaveBytes &&
				Blocker->Resize(1 + Available - LeaveBytes))) return;
			const auto BeforeDenied = Resources->Snapshot();
			const uint64 BeforeRelease = Resources->ReleaseVersionExcludingOwner(Consumer);
			Test.TestFalse(TEXT("八项预授予未齐时拒收构造并保留旧 Query"),
				FVoxelGenerationQuery::Create(Config, Cache.ToSharedRef(), Query, Error, false, false, false, &Consumer));
			Test.TestTrue(TEXT("报告真实 Input CapacityBlocked"), Error.StartsWith(TEXT("CapacityBlocked")));
			const auto AfterDenied = Resources->Snapshot();
			Test.TestTrue(TEXT("未创建部分生成器，拒收没有新增残留租约"), AfterDenied.LeaseCount <= BeforeDenied.LeaseCount);
			Test.TestTrue(TEXT("拒收的原 Input payload 和 handles 全部退回"),
				AfterDenied.Used[int32(EVoxelTerrainResourcePool::Input)] <= BeforeDenied.Used[int32(EVoxelTerrainResourcePool::Input)]);
			Test.TestEqual(TEXT("本次预授予回退不自唤醒"), Resources->ReleaseVersionExcludingOwner(Consumer), BeforeRelease);
			Blocker.Reset();
			FVoxelColumnSample Preserved;
			Test.TestTrue(TEXT("失败后旧 Query 的准备状态和原采样仍有效"), Query.SampleColumn(0, 0, Preserved, Error));
			Test.TestEqual(TEXT("失败后旧列的 surface 不变"), Preserved.SurfaceZ, Original.SurfaceZ);
			Test.TestTrue(TEXT("外部容量释放后同一 consumer 可重新构造"),
				FVoxelGenerationQuery::Create(Config, Cache.ToSharedRef(), Query, Error, false, false, false, &Consumer));
		}

		virtual void ReleaseFixture() override
		{
			Query = FVoxelGenerationQuery();
		}

	private:
		FVoxelGenerationQuery Query;
	};

	class FEcologyPlanCapacityCommand final : public FCacheCapacityCommand
	{
	public:
		explicit FEcologyPlanCapacityCommand(FAutomationTestBase& InTest)
			: FCacheCapacityCommand(InTest)
		{
		}

	protected:
		virtual void CheckFixture() override
		{
			FVoxelGenerationRecipe Recipe;
			Recipe.Settings.Seed = 137;
			Recipe.Settings.Ecology.Tree.bEnabled = false;
			Recipe.Settings.Ecology.Flower.bEnabled = false;
			Recipe.Settings.Ecology.Grass.bEnabled = true;
			Recipe.Settings.Ecology.Grass.Spacing = 8;
			Recipe.Settings.Ecology.Grass.PatchRadius = 0;
			Recipe.Settings.Ecology.Grass.PatchFillPermille = 1000;
			Recipe.Settings.Ecology.Grass.ChancePermille = 1000;
			Recipe.Settings.Ecology.Grass.DensityPermille = 1000;
			Recipe.Palette.Air = 0;
			Recipe.Palette.Grass = 1;
			Recipe.Ecology.GrassPlant = 4;
			Recipe.Biomes.AddDefaulted_GetRef().DefaultSurface = 1;
			const auto SharedRecipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe));
			const FVoxelEcologyGenerator Generator(SharedRecipe);
			auto SampleColumn = [](const FIntVector&, FVoxelColumnSample& OutColumn)
			{
				OutColumn = FVoxelColumnSample();
				OutColumn.BiomeIndex = 0;
				OutColumn.SurfaceZ = 0;
				OutColumn.SurfaceMaterial = 1;
				OutColumn.Climate.MoistureQ15 = 12000;
				return true;
			};
			auto SampleBase = [](const FIntVector& InPosition, uint32& OutValue)
			{
				OutValue = InPosition.Z <= 0 ? 1 : 0;
				return true;
			};
			const FVoxelGenerationBounds Bounds{FIntVector(0, 0, -64), FIntVector(64, 64, 128)};
			FVoxelEcologyPlan Baseline;
			if (!Test.TestTrue(TEXT("真实 Count/Build 生成固定草地 fixture"),
				Generator.BuildPlan(Bounds, SampleColumn, SampleBase, Baseline, Error))) return;
			if (!Test.TestEqual(TEXT("每个无半径草地格输出一次"), Baseline.Writes.Num(), 64)) return;
			const uint64 RootBytes = VoxelBoundedAllocation::WorkerSharedBytes<const FVoxelEcologyPlan>();
			const uint64 BodyBytes = RootBytes + Baseline.GetAllocatedBytes();
			FVoxelTerrainResourceLimits Limits;
			Limits.WorkingBytes = 2 * BodyBytes + 65536;
			Limits.ResidentBytes = 3 * BodyBytes;
			Limits.OverlapBytes = BodyBytes;
			const auto Control = MakeCacheControlConsumer();
			if (!Test.TestTrue(TEXT("生态正文绑定实际动态容量的原账本"), Resources->Configure(Limits) &&
				FVoxelGenerationPlanCache::Create(Resources, Control, Cache, Error, true, false))) return;
			const FVoxelEcologyTileKey WarmKey{FIntPoint::ZeroValue};
			FVoxelEcologyTileKey ColdKey;
			bool bFoundKey = false;
			for (int32 X = 1; X <= 4096; ++X)
			{
				const FVoxelEcologyTileKey Candidate{FIntPoint(X, 0)};
				if (GetTypeHash(Candidate) % FVoxelGenerationPlanCache::ShardCount ==
					GetTypeHash(WarmKey) % FVoxelGenerationPlanCache::ShardCount)
				{
					ColdKey = Candidate;
					bFoundKey = true;
					break;
				}
			}
			if (!Test.TestTrue(TEXT("冷暖生态 key 复用已授予的控制 backing"), bFoundKey)) return;
			auto Consumer = Control;
			Consumer.OwnerId = FGuid::NewGuid();
			Consumer.RequestSerial = 91;
			FVoxelTaskStamp Stamp;
			Stamp.WorldEpoch = Consumer.WorldEpoch;
			Stamp.OwnerId = Consumer.OwnerId;
			Stamp.OwnerIncarnation = Consumer.OwnerIncarnation;
			Stamp.Token = Consumer.RequestSerial;
			FVoxelGenerationPlanningReadScope Scope({}, &Stamp, nullptr, &Consumer);
			auto BuildWarm = [&](FVoxelEcologyPlan& OutPlan, FString& OutError)
			{
				return Generator.BuildPlan(Bounds, SampleColumn, SampleBase, OutPlan, OutError);
			};
			if (!Test.TestTrue(TEXT("真实生态算法在授额后发布 immutable 正文"),
				Cache->GetOrBuildEcology(WarmKey, BuildWarm, Warmed, Error))) return;
			Test.TestEqual(TEXT("真实 array/map backing 与原正文租约一起转 Resident"),
				Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Resident)], BodyBytes);
			for (const FVoxelEcologyPlanWrite& Write : Baseline.Writes)
			{
				uint32 Value = MAX_uint32;
				if (!Test.TestTrue(TEXT("授额后的真实 resolved 索引保持值"), Warmed->Sample(Write.Position, Value) && Value == Write.Value)) return;
			}
			const auto BeforeStore = Resources->Snapshot();
			Test.TestFalse(TEXT("不能将现有正文的租约冒充另一个新 key 的原租约"),
				Cache->StoreEcology(ColdKey, Warmed, Error));
			Test.TestTrue(TEXT("无租约证明的 Store 明确拒收"), Error.StartsWith(TEXT("InvalidResource")));
			Test.TestEqual(TEXT("拒收不发布另一个 immutable key"), Cache->GetStats().Ecology, 1);
			Test.TestEqual(TEXT("拒收不重复占用 Resident"),
				Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Resident)],
				BeforeStore.Used[int32(EVoxelTerrainResourcePool::Resident)]);
			Test.TestTrue(TEXT("同 key 已发布 immutable 继续优先"), Cache->StoreEcology(WarmKey, Warmed, Error));
			auto OtherConsumer = Consumer;
			OtherConsumer.OwnerId = FGuid::NewGuid();
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
			if (!Test.TestTrue(TEXT("剩余额度只容纳冷 root，不容纳动态 Writes"), Resources->Acquire(
				OtherConsumer, EVoxelTerrainResourcePool::Resident, BodyBytes - RootBytes, Blocker))) return;
			int32 ColdBuilds = 0;
			auto BuildCold = [&](FVoxelEcologyPlan& OutPlan, FString& OutError)
			{
				++ColdBuilds;
				const int32 MinX = ColdKey.Coordinate.X * 64;
				return Generator.BuildPlan({FIntVector(MinX, 0, -64), FIntVector(MinX + 64, 64, 128)},
					SampleColumn, SampleBase, OutPlan, OutError);
			};
			FVoxelEcologyPlanPtr Cold = Warmed;
			const uint64 BeforeRelease = Resources->ReleaseVersionExcludingOwner(Consumer);
			Test.TestFalse(TEXT("动态正文分配前 Resident 拒收"), Cache->GetOrBuildEcology(ColdKey, BuildCold, Cold, Error));
			Test.TestEqual(TEXT("进入真实 Count 后由动态 grant 拒收"), ColdBuilds, 1);
			Test.TestTrue(TEXT("明确可恢复 CapacityBlocked 并保留旧 Out"), Error.StartsWith(TEXT("CapacityBlocked")) && Cold == Warmed);
			Test.TestEqual(TEXT("失败正文及重分配峰值全部退回 Working"),
				Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
			Test.TestEqual(TEXT("内部失败退租不自唤醒"), Resources->ReleaseVersionExcludingOwner(Consumer), BeforeRelease);
			Blocker.Reset();
			Test.TestTrue(TEXT("原 consumer 在外部释放后继续同一冷 key"), Cache->GetOrBuildEcology(ColdKey, BuildCold, Cold, Error));
			Test.TestEqual(TEXT("重试成功后只驻留两个真实动态正文"),
				Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Resident)], 2 * BodyBytes);
		}

		virtual void ReleaseFixture() override
		{
			Warmed.Reset();
		}

	private:
		FVoxelEcologyPlanPtr Warmed;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCacheFactoryCapacityRefusalTest,
	"WHFramework.Voxel.FineRefactor.Generation.CacheFactoryCapacityRefusal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCacheFactoryCapacityRefusalTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCacheFactoryRefusalCommand>(*this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCacheColumnReservedCapacityTest,
	"WHFramework.Voxel.FineRefactor.Generation.CacheColumnReservedCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCacheColumnReservedCapacityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<TCacheColumnCapacityCommand<FVoxelBaseColumnEntry>>(*this));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<TCacheColumnCapacityCommand<FVoxelNaturalColumnEntry>>(*this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelQueryGeneratorCapacityTest,
	"WHFramework.Voxel.FineRefactor.Generation.QueryGeneratorCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelQueryGeneratorCapacityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FQueryGeneratorCapacityCommand>(*this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelEcologyPlanCapacityTest,
	"WHFramework.Voxel.FineRefactor.Generation.EcologyPlanCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEcologyPlanCapacityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEcologyPlanCapacityCommand>(*this));
	return true;
}
#endif
