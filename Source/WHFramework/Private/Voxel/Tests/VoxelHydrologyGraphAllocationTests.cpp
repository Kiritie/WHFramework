#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyRegionGraph.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHydrologyGraphAllocationResumeTest,
	"WHFramework.Voxel.Hydrology.RegionGraph.OriginalCapacityResume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHydrologyGraphAllocationResumeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr uint64 Generation = 197;
	constexpr int32 Side = 8;
	FString Error;
	const auto Sample = [](const FIntPoint& Cell, FVoxelHydrologyRegionSample& OutCell, FString& OutError)
	{
		OutCell.GroundZ = 8;
		OutCell.RainWeight = 5;
		if (Cell.X == 0)
		{
			OutCell.GroundZ = -4;
			OutCell.WaterZ = 0;
			OutCell.WaterId = {9, 11};
		}
		OutError.Reset();
		return EVoxelHydrologyClosureRead::Ready;
	};
	FVoxelHydrologyRegionGraphBuild Unblocked;
	if (!Unblocked.Initialize({}, Side, Side, 32, Generation, Error)) return false;
	EVoxelHydrologyRegionStep ReferenceStep = EVoxelHydrologyRegionStep::None;
	for (int32 Attempt = 0; Attempt < 10000; ++Attempt)
	{
		ReferenceStep = Unblocked.Advance(Generation, Sample, 67, Error);
		if (ReferenceStep != EVoxelHydrologyRegionStep::Yielded) break;
	}
	if (!TestTrue(TEXT("无压力参考区域真实完成"), ReferenceStep == EVoxelHydrologyRegionStep::Ready)) return false;
	FVoxelTerrainResourceLimits Limits;
	Limits.InputBytes = 1024 * 1024;
	Limits.WorkingBytes = 1024 * 1024;
	Limits.ResidentBytes = 2 * 1024 * 1024;
	Limits.OverlapBytes = 64 * 1024;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(Generation, FGuid::NewGuid(), 1, {}, 1);
	auto PressureConsumer = Consumer;
	PressureConsumer.OwnerId = FGuid::NewGuid();
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Resident;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Pressure;
	const uint64 RootBytes = VoxelBoundedAllocation::SharedBytes<FVoxelHydrologyRegionGraphBuild>();
	if (!FVoxelTerrainResources::Create(Limits, Consumer, Resources, Error) ||
		!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, RootBytes, Working) ||
		!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident, RootBytes, Resident)) return false;
	TSharedPtr<FVoxelHydrologyRegionGraphBuild, ESPMode::ThreadSafe> Build = MakeShared<FVoxelHydrologyRegionGraphBuild, ESPMode::ThreadSafe>();
	int32 Reads = 0;
	const auto Read = [&](const FIntPoint& Cell, FVoxelHydrologyRegionSample& OutCell, FString& OutError)
	{
		++Reads;
		return Sample(Cell, OutCell, OutError);
	};
	{
		FVoxelGenerationAllocationScope Allocation(Resources, Consumer, Working, Resident, RootBytes);
		if (!Allocation.Check(Error) || !Build->Initialize({}, Side, Side, 32, Generation, Error)) return false;
	}
	const auto AdvanceOne = [&](EVoxelHydrologyRegionStep& OutStep)
	{
		// 与原 Cache 一致：每个工作片建立新 scope，拒绝授额的错误不能在片内清除。
		FVoxelGenerationAllocationScope Allocation(Resources, Consumer, Working, Resident, RootBytes, Build->GetAllocatedBytes());
		if (!Allocation.Check(Error)) { AddError(Error); return false; }
		OutStep = Build->Advance(Generation, Read, 1, Error);
		if (OutStep == EVoxelHydrologyRegionStep::CapacityBlocked)
		{
			// 失败 scope 的 Check 保留原错误，直接独立核对已回滚的真实两份授额。
			return TestEqual(TEXT("拒绝分配后原工作租约精确覆盖实际正文"), Working->Bytes(), RootBytes + Build->GetAllocatedBytes()) &&
				TestEqual(TEXT("拒绝分配后原驻留预留精确回滚"), Resident->Bytes(), RootBytes + Build->GetAllocatedBytes());
		}
		if (!TestTrue(TEXT("每片仍按原授额计量真实 backing"), Allocation.ValidateRetainedBytes(Build->GetAllocatedBytes(), Error)))
		{
			AddError(Error);
			return false;
		}
		return true;
	};
	bool bBlocked = false;
	for (int32 Attempt = 0; Attempt < 10000; ++Attempt)
	{
		EVoxelHydrologyRegionStep Step = EVoxelHydrologyRegionStep::None;
		if (!AdvanceOne(Step)) return false;
		if (Step == EVoxelHydrologyRegionStep::CapacityBlocked)
		{
			bBlocked = Error.StartsWith(TEXT("CapacityBlocked"));
			break;
		}
		if (Step != EVoxelHydrologyRegionStep::Yielded) { AddError(Error); return false; }
		if (Reads == Side * Side && !Pressure)
		{
			const uint64 Used = Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)];
			if (!TestTrue(TEXT("已冻结输入的原正文仍在原工作上限内"), Used + 1 < Limits.WorkingBytes)) return false;
			if (!TestTrue(TEXT("真实输入读完后以无关原消费者占满剩余工作容量"), Resources->Acquire(PressureConsumer,
				EVoxelTerrainResourcePool::Working, Limits.WorkingBytes - Used - 1, Pressure))) return false;
		}
	}
	if (!TestTrue(TEXT("无关真实读者占用使后续正文等待授额，而非终止或越界"), bBlocked && Pressure.IsValid())) return false;
	Pressure.Reset();
	EVoxelHydrologyRegionStep Step = EVoxelHydrologyRegionStep::None;
	for (int32 Attempt = 0; Attempt < 10000; ++Attempt)
	{
		if (!AdvanceOne(Step)) return false;
		if (Step != EVoxelHydrologyRegionStep::Yielded) break;
	}
	if (!TestTrue(TEXT("原容量释放后实际续跑完成"), Step == EVoxelHydrologyRegionStep::Ready)) return false;
	TestEqual(TEXT("授额等待不重读已冻结的输入"), Reads, Side * Side);
	TestTrue(TEXT("恢复后全部字段和指纹与无压力结果一致"), Build->GetResult().HasEquivalentOutput(Unblocked.GetResult()));
	Build.Reset();
	Working.Reset();
	Resident.Reset();
	TestEqual(TEXT("退出后全部实际租约释放"), Resources->Snapshot().LeaseCount, 0);
	TestEqual(TEXT("原工作池无残留"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	TestEqual(TEXT("原驻留池无残留"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Resident)], uint64(0));
	return true;
}

#endif
