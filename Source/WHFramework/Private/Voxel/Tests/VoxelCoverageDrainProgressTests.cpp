#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/VoxelModule.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCoverageDrainProgressTest,
	"WHFramework.Voxel.Rendering.CoverageDrainProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCoverageDrainProgressTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler(true);
	FVoxelViewManager Manager(*Module, Scheduler, 41);
	const FVoxelCoverageRect Near{FIntPoint(-4, -4), FIntPoint(4, 4)};
	const FVoxelCoverageRect Far{FIntPoint(4096, 4096), FIntPoint(4100, 4100)};
	int32 Microsteps = 0;
	bool bDelayFirstStep = true;
	const auto BuildCoverage = [&Manager, &Microsteps, &bDelayFirstStep]() -> FVoxelGameThreadContinuation
	{
		for (int32 Step = 0; Step < 8193; ++Step)
		{
			++Microsteps;
			if (bDelayFirstStep)
			{
				FPlatformProcess::SleepNoStats(0.02f);
				bDelayFirstStep = false;
			}
			co_await FVoxelGameThreadContinuation::Yield();
		}
		Manager.MarkCoverageDirty();
	};
	Manager.bActiveCoverageFull = true;
	Manager.CoverageContinuation = BuildCoverage();
	TestTrue(TEXT("完整覆盖计算仍冻结相交提交"), Manager.ShouldDeferCoverage(Near));
	TestTrue(TEXT("完整覆盖计算不能提前放行远处提交"), Manager.ShouldDeferCoverage(Far));
	Manager.AdvanceCoveragePlan(FPlatformTime::Seconds() - 1.0);
	TestEqual(TEXT("过期共享期限不执行覆盖微步"), Microsteps, 0);
	Manager.AdvanceCoveragePlan(FPlatformTime::Seconds() + 0.01);
	TestEqual(TEXT("首个微步用尽期限后不继续执行下一微步"), Microsteps, 1);
	TestTrue(TEXT("中途让出保持原覆盖事务与门禁"),
		Manager.CoverageContinuation.IsValid() && Manager.ShouldDeferCoverage(Near));
	Manager.AdvanceCoveragePlan(FPlatformTime::Seconds() + 10.0);
	if (!TestFalse(TEXT("有界续跑最终完成原覆盖事务"), Manager.CoverageContinuation.IsValid()))
	{
		return false;
	}
	TestEqual(TEXT("覆盖微步没有重建或重复执行"), Microsteps, 8193);
	TestFalse(TEXT("完成后近处提交门禁已开放"), Manager.ShouldDeferCoverage(Near));
	TestFalse(TEXT("完成后远处提交门禁已开放"), Manager.ShouldDeferCoverage(Far));
	TestTrue(TEXT("新脏事件保留且请求下一帧发布消费机会"),
		Manager.bCoverageDirty && Manager.bCoverageDrainPending);
	Manager.AdvanceCoveragePlan(FPlatformTime::Seconds() + 10.0);
	TestFalse(TEXT("发布尚未服务时新脏事件不能立即重新冻结全部提交"), Manager.CoverageContinuation.IsValid());
	TestTrue(TEXT("排空等待不丢弃后续覆盖需求"), Manager.bCoverageDirty);
	Manager.Reset();
	TestFalse(TEXT("重置撤销旧世界的排空等待"), Manager.bCoverageDrainPending);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelContinuationCompletionTest,
	"WHFramework.Voxel.Rendering.ContinuationCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelContinuationCompletionTest::RunTest(const FString& InParameters)
{
	FVoxelGameThreadContinuation Empty;
	Empty.RunToCompletion();
	TestTrue(TEXT("空续体可同步完成"), Empty.IsComplete());
	int32 Steps = 0;
	int32 ReleasedFrames = 0;
	struct FFrameLifetime
	{
		int32& Released;
		~FFrameLifetime()
		{
			++Released;
		}
	};
	const auto Child = [&Steps, &ReleasedFrames]() -> FVoxelGameThreadContinuation
	{
		FFrameLifetime Lifetime{ReleasedFrames};
		for (int32 Index = 0; Index < 5; ++Index)
		{
			++Steps;
			co_await FVoxelGameThreadContinuation::Yield();
		}
	};
	const auto Parent = [&Child, &Steps]() -> FVoxelGameThreadContinuation
	{
		co_await Child();
		++Steps;
	};
	auto Work = Parent();
	Work.Resume();
	Work.Resume();
	TestEqual(TEXT("同步完成前保留已执行微步"), Steps, 1);
	auto Moved = MoveTemp(Work);
	Work.RunToCompletion();
	Moved.RunToCompletion();
	TestTrue(TEXT("移动后嵌套续体完成父子执行"), Moved.IsComplete());
	TestEqual(TEXT("嵌套微步恰好执行一次"), Steps, 6);
	TestEqual(TEXT("子帧析构恰好一次"), ReleasedFrames, 1);
	Moved.RunToCompletion();
	Moved.Reset();
	TestEqual(TEXT("完成后的重复调用不会重复执行"), Steps, 6);
	TestEqual(TEXT("重置不重复销毁子帧"), ReleasedFrames, 1);
	return true;
}

#endif
