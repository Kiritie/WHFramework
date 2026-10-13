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
	const auto BuildCoverage = [&Manager, &Microsteps]() -> FVoxelGameThreadContinuation
	{
		for (int32 Step = 0; Step < 8193; ++Step)
		{
			++Microsteps;
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
	Manager.AdvanceCoveragePlan(FPlatformTime::Seconds() + 10.0);
	TestTrue(TEXT("即使时间充足，单次服务仍受微步上限限制"), Microsteps > 0 && Microsteps <= 4096);
	TestTrue(TEXT("中途让出保持原覆盖事务与门禁"),
		Manager.CoverageContinuation.IsValid() && Manager.ShouldDeferCoverage(Near));
	for (int32 Service = 0; Service < 16 && Manager.CoverageContinuation.IsValid(); ++Service)
	{
		Manager.AdvanceCoveragePlan(FPlatformTime::Seconds() + 10.0);
	}
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

#endif
