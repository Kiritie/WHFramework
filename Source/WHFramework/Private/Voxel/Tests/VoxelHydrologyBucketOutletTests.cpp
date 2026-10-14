#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyBucketOutlet.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyBucketOutletResumeTest,
	"WHFramework.Voxel.Hydrology.BucketOutlet.OffGridLakeResume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyBucketOutletResumeTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	for (const int32 Offset : { 0, -21 })
	{
		const FIntPoint Anchor(Offset, Offset);
		const FIntPoint Lake = Anchor + FIntPoint(11, 11);
		const FIntPoint Maximum = Anchor + FIntPoint(20, 20);
		FVoxelStableId Body;
		Body.High = 123;
		Body.Low = 456;
		bool bPending = true;
		bool bCapacityBlocked = false;
		const auto Read = [&](const FIntPoint& InCell, FVoxelHydrologySectionSample& OutSample, FString& OutError)
		{
			if (bCapacityBlocked)
			{
				OutError = TEXT("CapacityBlocked: fixture");
				return EVoxelHydrologyClosureRead::Pending;
			}
			if (bPending && InCell == Lake) return EVoxelHydrologyClosureRead::Pending;
			OutSample.OriginalGround = InCell.X == Offset + 7 ? 30 : 8;
			if (InCell == Lake)
			{
				OutSample.OriginalGround = 0;
				OutSample.ExistingWaterZ = 5;
				OutSample.ExistingBodyId = Body;
			}
			return EVoxelHydrologyClosureRead::Ready;
		};
		FString Error;
		FVoxelHydrologyBucketOutlet Resumed;
		if (!TestTrue(TEXT("有界雨桶初始化"), Resumed.Initialize(Anchor, Anchor, Maximum, Error))) return false;
		TestTrue(TEXT("缺失细水输入不发布"), Resumed.Advance(Read, 10000, Error) == EVoxelHydrologySharedSearchStep::NeedInput && !Resumed.IsReady());
		TAtomic<bool> Cancel(true);
		TestTrue(TEXT("取消不发布"), Resumed.Advance(Read, 1, Error, &Cancel) == EVoxelHydrologySharedSearchStep::Canceled && !Resumed.IsReady());
		bPending = false;
		bCapacityBlocked = true;
		TestTrue(TEXT("容量等待不冒充无水"), Resumed.Advance(Read, 1, Error) == EVoxelHydrologySharedSearchStep::CapacityBlocked && !Resumed.IsReady());
		bCapacityBlocked = false;
		EVoxelHydrologySharedSearchStep Status = EVoxelHydrologySharedSearchStep::Yielded;
		for (int32 Call = 0; Call < 10000 && Status == EVoxelHydrologySharedSearchStep::Yielded; ++Call)
		{
			Status = Resumed.Advance(Read, 1, Error);
		}
		if (!TestTrue(TEXT("单步预算恢复完成"), Status == EVoxelHydrologySharedSearchStep::Ready)) return false;
		const FVoxelHydrologyBucketOutletResult Result = Resumed.GetResult();
		TestTrue(TEXT("离粗格湖有真实接收端"), Result.bHasOutlet);
		TestEqual(TEXT("保留精确细格坐标"), Result.Terminal, Lake);
		TestEqual(TEXT("保留湖水位"), Result.WaterZ, 5);
		TestTrue(TEXT("保留水体身份"), Result.BodyId == Body);
		TestEqual(TEXT("山脊阻力不能被湖水位替代"), Result.Barrier, 30);
		TestEqual(TEXT("阈值内路径最短"), Result.PathLength, 22);
		FVoxelHydrologyBucketOutlet Direct;
		if (!TestTrue(TEXT("无中断初始化"), Direct.Initialize(Anchor, Anchor, Maximum, Error))) return false;
		if (!TestTrue(TEXT("无中断完成"), Direct.Advance(Read, 10000, Error) == EVoxelHydrologySharedSearchStep::Ready)) return false;
		TestEqual(TEXT("恢复不改变指纹"), Result.Witness, Direct.GetResult().Witness);
		TestTrue(TEXT("恢复不改变完整路径"), Resumed.GetWitnessPath() == Direct.GetWitnessPath());
		Resumed.FreeBacking();
		Direct.FreeBacking();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyBucketOutletDryTest,
	"WHFramework.Voxel.Hydrology.BucketOutlet.CertifiedDry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyBucketOutletDryTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelHydrologyBucketOutlet Outlet;
	FString Error;
	if (!TestTrue(TEXT("纯干雨桶初始化"), Outlet.Initialize(FIntPoint::ZeroValue, FIntPoint(-2, -2), FIntPoint(2, 2), Error))) return false;
	const auto Read = [](const FIntPoint&, FVoxelHydrologySectionSample& OutSample, FString&)
	{
		OutSample.OriginalGround = 10;
		return EVoxelHydrologyClosureRead::Ready;
	};
	if (!TestTrue(TEXT("完整读取才证明无接收体"), Outlet.Advance(Read, 1000, Error) == EVoxelHydrologySharedSearchStep::Ready)) return false;
	TestFalse(TEXT("物理雨桶边缘不冒充出口"), Outlet.GetResult().bHasOutlet);
	Outlet.FreeBacking();
	return true;
}

#endif
